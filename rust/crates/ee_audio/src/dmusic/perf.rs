//! The performance (dmusic's `Performance.c`, `Composer.c`, `Style.c`'s pattern choice, `Common.c`'s time
//! arithmetic): the segment's messages and the playing pattern's notes in two queues, handled at their music time
//! as the frames are rendered; the groove level picks the patterns; a transition is a composed one-measure segment.
//! Every EE patch of dmusic is kept (the clock that carries the fraction of a tick, the pattern choice measured
//! against the original, the groove level added once, the stop that lets what is due sound first, the reverb on the
//! mix), operation for operation: `golden_dmusic.txt` and the songs' goldens are the C's calls into its synthesizer.

use std::sync::atomic::Ordering;
use std::sync::Arc;

use super::files::*;
use super::queue::{Conflict, Queue};
use super::reverb::Reverb;
use super::synth::Sink;

const TICKS_PER_QUARTER_NOTE: u32 = 768;
const DEFAULT_TEMPO: f64 = 100.0;
const DEFAULT_SAMPLE_RATE: u32 = 44100;
const DEFAULT_SCALE_PATTERN: u32 = 0xab5ab5;
const CURVE_SPACING: u32 = 5;
/// dmusic's `M_PI` where the C runtime's headers define none (the game's build): 3.14159.
#[allow(clippy::approx_constant)]
const DM_PI: f64 = 3.14159;

/// `DmTiming`.
pub const TIMING_INSTANT: i32 = 1;
pub const TIMING_GRID: i32 = 2;
pub const TIMING_BEAT: i32 = 3;
pub const TIMING_MEASURE: i32 = 4;

/// `DmEmbellishmentType`.
pub const EMBELLISHMENT_NONE: i32 = 0;
pub const EMBELLISHMENT_END_AND_INTRO: i32 = 6;

/// dmusic's random numbers (`Dm_rand`, the C runtime's `rand()` there): an LCG of the classic `rand` form.
pub struct Rng(u32);

impl Rng {
    pub fn new(seed: u32) -> Rng {
        Rng(seed)
    }
    pub fn draw(&mut self) -> u32 {
        self.0 = self.0.wrapping_mul(1103515245).wrapping_add(12345);
        (self.0 >> 16) & 0x7fff
    }
    /// `Dm_randRange`: in (range / 2, range].
    fn range(&mut self, range: i32) -> i32 {
        let rnd = self.draw() % range as u32;
        range - (rnd / 2) as i32
    }
}

/// `Dm_getBeatLength`.
pub fn beat_length(sig: TimeSignature) -> u32 {
    if sig.beat == 0 {
        (TICKS_PER_QUARTER_NOTE * 4) / 256
    } else {
        (TICKS_PER_QUARTER_NOTE * 4) / u32::from(sig.beat)
    }
}

/// `Dm_getMeasureLength`.
pub fn measure_length(sig: TimeSignature) -> u32 {
    (u32::from(sig.beats_per_measure) * beat_length(sig)).max(1)
}

/// `Dm_getTicksPerSample`.
fn ticks_per_sample(sig: TimeSignature, bpm: f64, rate: u32) -> f64 {
    let pulses_per_beat = beat_length(sig);
    let beats_per_second = bpm / 60.0;
    let pulses_per_second = f64::from(pulses_per_beat) * beats_per_second;
    pulses_per_second / f64::from(rate)
}

/// `Dm_getTimeOffset`.
fn time_offset(grid_start: u32, offset: i32, sig: TimeSignature) -> u32 {
    let beat = beat_length(sig);
    let grids = u32::from(sig.grids_per_beat).max(1);
    let full = (grid_start / grids).wrapping_mul(beat);
    let partial = (grid_start % grids).wrapping_mul(beat / grids);
    (offset as u32).wrapping_add(full).wrapping_add(partial)
}

fn bit_count(v: u32) -> u32 {
    v.count_ones()
}

/// `fixup_scale`.
fn fixup_scale(scale: u32, scale_root: u8) -> u32 {
    const FALLBACK: [u32; 12] = [
        0xab5ab5, 0x6ad6ad, 0x5ab5ab, 0xad5ad5, 0x6b56b5, 0x5ad5ad, 0x56b56b, 0xd5ad5a, 0xb56b56, 0xd6ad6a, 0xb5ab5a,
        0xad6ad6,
    ];
    let mut scale = (scale & 0x0FFF) | (scale << 12);
    scale >>= 12 - (u32::from(scale_root) & 12);
    scale = (scale & 0x0FFF) | (scale << 12);
    if bit_count(scale & 0xFFF) <= 4 {
        let mut best = FALLBACK[0];
        let mut best_score = 0;
        for f in FALLBACK {
            let score = bit_count((f & scale) & 0xFFF);
            if score > best_score {
                best = f;
                best_score = score;
            }
        }
        scale = best;
    }
    if scale & 0xFF00_0000 == 0 {
        scale |= (scale & 0xFFF000) << 12;
    }
    scale
}

/// `DmPerformance_musicValueToMidi`: a style note's music value in the chord and scale, or -1.
pub fn music_value_to_midi(chord: SubChord, mode: u8, mut value: u16) -> i32 {
    let mut offset: u32 = 0;
    while value >= 0xE000 {
        value = value.wrapping_add(0x1000);
        offset = offset.wrapping_sub(12);
    }
    let music_tmp = (value & 0x00F0) + 0x0070;
    if music_tmp & 0x0F00 != 0 {
        value = (value & 0xFF0F) | (music_tmp & 0x00F0);
        offset = offset.wrapping_sub(12);
    }
    let mut root: u16 = 0;
    if mode & PLAY_CHORD_ROOT != 0 {
        root = u16::from(chord.chord_root);
    } else if mode & PLAY_KEY_ROOT != 0 {
        return -1;
    }
    if mode & (PLAY_CHORD_INTERVALS | PLAY_SCALE_INTERVALS) == 0 {
        return -1;
    }
    let scale_pattern = fixup_scale(
        if chord.scale_pattern != 0 { chord.scale_pattern } else { DEFAULT_SCALE_PATTERN },
        chord.scale_root,
    );
    let chord_pattern = if chord.chord_pattern == 0 { 1 } else { chord.chord_pattern };
    let chord_position = (value & 0x0f00) >> 8;
    let scale_position = (value & 0x0070) >> 4;
    let mut accidentals = (value & 0x000f) as i16;
    if accidentals > 8 {
        accidentals -= 16;
    }
    let accidentals = i32::from(accidentals);
    let root_i = i32::from(root);
    let root_octave = root % 12;
    let chord_bits = bit_count(chord_pattern) as u16;
    let mut note_value: i32 = 0;
    let note_offset: i32;
    let mut note_pattern: u32;
    let mut note_position: u16;
    if mode & PLAY_CHORD_INTERVALS != 0 && scale_position == 0 && chord_position < chord_bits {
        note_offset = root_i + accidentals;
        note_pattern = chord_pattern;
        note_position = chord_position;
    } else if mode & PLAY_CHORD_INTERVALS != 0 && chord_position < chord_bits {
        note_pattern = chord_pattern;
        note_position = chord_position;
        if note_pattern != 0 {
            while note_pattern & 1 == 0 {
                note_pattern >>= 1;
                note_value += 1;
            }
        }
        if note_position > 0 {
            loop {
                note_pattern >>= 1;
                note_value += 1;
                if note_pattern & 1 != 0 {
                    note_position -= 1;
                }
                if note_pattern == 0 {
                    note_value += i32::from(note_position);
                    break;
                }
                if note_position == 0 {
                    break;
                }
            }
        }
        note_value += i32::from(root_octave);
        note_offset = accidentals + root_i - i32::from(root_octave);
        note_pattern = scale_pattern >> (note_value % 12) as u32;
        note_position = scale_position;
    } else if mode & PLAY_SCALE_INTERVALS != 0 {
        note_value = i32::from(root_octave);
        note_offset = accidentals + root_i - i32::from(root_octave);
        note_pattern = scale_pattern >> root_octave;
        note_position = chord_position.wrapping_mul(2).wrapping_add(scale_position);
    } else {
        return -1;
    }
    note_position = note_position.wrapping_add(1);
    while note_position > 0 {
        note_value += 1;
        if note_pattern & 1 != 0 {
            note_position -= 1;
        }
        if note_pattern == 0 {
            note_value += i32::from(note_position);
            break;
        }
        note_pattern >>= 1;
    }
    note_value -= 1;
    note_value += note_offset;
    note_value = note_value.wrapping_add(offset as i32);
    let octave = i32::from(((value >> 12) & 0xF) as i16) * 12;
    if mode & PLAY_CHORD_ROOT != 0 {
        octave + note_value - 12
    } else {
        octave + note_value
    }
}

/// `Dm_toEmbellishmentFlagset` (it tests the command's value bit by bit, as dmusic did).
fn embellishment_flags(cmd: u8) -> u32 {
    let mut f = 0;
    if cmd & COMMAND_FILL != 0 {
        f |= 1;
    }
    if cmd & COMMAND_INTRO != 0 {
        f |= 2;
    }
    if cmd & COMMAND_BREAK != 0 {
        f |= 4;
    }
    if cmd & COMMAND_END != 0 {
        f |= 8;
    }
    f
}

/// `DmStyle_getRandomPattern` (its EE patch): of the patterns whose groove range holds the level, the longest that
/// fits the measures left (random only among equals), else the longest; with none in range, the nearest range's.
fn choose_pattern<'a>(
    style: &'a Style,
    groove: u32,
    cmd: u8,
    measures_left: u32,
    rng: &mut Rng,
) -> Option<&'a Pattern> {
    let embellishment = embellishment_flags(cmd);
    let mut best_distance = u32::MAX;
    let mut best_length = 0u32;
    let mut best_fit = 0u32;
    let mut candidates = 0u32;
    for pass in 0..3 {
        let length = if best_fit != 0 { best_fit } else { best_length };
        let mut pick = if pass == 2 { rng.draw() % candidates } else { 0 };
        for p in &style.patterns {
            let e = u32::from(p.embellishment);
            if e != embellishment && e & embellishment == 0 {
                continue;
            }
            if p.embellishment == u16::from(COMMAND_GROOVE) && p.length_measures == 1 {
                continue;
            }
            let (bottom, top) = (u32::from(p.groove_bottom), u32::from(p.groove_top));
            let d = if groove < bottom { bottom - groove } else { groove.saturating_sub(top) };
            let len = u32::from(p.length_measures);
            if pass == 0 {
                if d < best_distance {
                    best_distance = d;
                    best_length = 0;
                    best_fit = 0;
                }
                if d == best_distance {
                    best_length = best_length.max(len);
                    if len <= measures_left && len > best_fit {
                        best_fit = len;
                    }
                }
            } else if d == best_distance && len == length {
                if pass == 1 {
                    candidates += 1;
                } else if pick == 0 {
                    return Some(p);
                } else {
                    pick -= 1;
                }
            }
        }
        if pass == 1 && candidates == 0 {
            return None;
        }
    }
    None
}

fn lerp(x: f32, start: f32, end: f32) -> f32 {
    (1.0 - x) * start + x * end
}

/// `DmCurve_lerp`.
fn curve_value(c: &Curve, phase: f32) -> f32 {
    let (s, e) = (f32::from(c.start_value), f32::from(c.end_value));
    match c.curve_shape {
        SHAPE_LINEAR => lerp(phase, s, e),
        SHAPE_INSTANT => e,
        SHAPE_EXP => lerp(phase.powf(4.0), s, e),
        SHAPE_LOG => lerp(phase.sqrt(), s, e),
        SHAPE_SINE => {
            let phase = (((phase - 0.5) * DM_PI as f32).sin() + 1.0) * 0.5;
            lerp(phase, s, e)
        }
        _ => s,
    }
}

/// The performance's state, under its lock.
pub struct Performance<S: Sink> {
    control_queue: Queue,
    music_queue: Queue,
    segment: Option<Arc<Segment>>,
    segment_start: u32,
    style: Option<Arc<Style>>,
    band: Option<Arc<Band>>,
    pub synth: S,
    sample_rate: u32,
    variation: u32,
    time: u32,
    groove: u8,
    groove_range: u8,
    groove_base: u8,
    groove_master: i32,
    time_carry: f64,
    tempo: f64,
    /// The last chord and its time (`slf->chord`; None: never one, dmusic's zeroed chord).
    chord: Option<(u32, Chord)>,
    time_signature: TimeSignature,
    reverb: Option<Reverb>,
    reverb_on: bool,
    pub rng: Rng,
}

impl<S: Sink> Performance<S> {
    /// `DmPerformance_create`.
    pub fn new(rate: u32, synth: S) -> Self {
        Performance {
            control_queue: Queue::default(),
            music_queue: Queue::default(),
            segment: None,
            segment_start: 0,
            style: None,
            band: None,
            synth,
            sample_rate: if rate == 0 { DEFAULT_SAMPLE_RATE } else { rate },
            variation: 0,
            time: 0,
            groove: 1,
            groove_range: 0,
            groove_base: 1,
            groove_master: 0,
            time_carry: 0.0,
            tempo: DEFAULT_TEMPO,
            chord: None,
            time_signature: TimeSignature { beats_per_measure: 4, beat: 4, grids_per_beat: 2 },
            reverb: None,
            reverb_on: false,
            rng: Rng::new(1),
        }
    }

    pub fn sample_rate(&self) -> u32 {
        self.sample_rate
    }

    /// `Dm_getBoundaryOffset`.
    fn boundary(&self, timing: i32) -> u32 {
        let unit = match timing {
            TIMING_GRID => beat_length(self.time_signature) / u32::from(self.time_signature.grids_per_beat).max(1),
            TIMING_BEAT => beat_length(self.time_signature),
            TIMING_MEASURE => measure_length(self.time_signature),
            _ => return self.time,
        };
        let offset = self.time.wrapping_sub(self.segment_start);
        let delay = if offset != 0 && unit != 0 { unit - (offset % unit) } else { 0 };
        self.time.wrapping_add(delay)
    }

    /// `DmPerformance_playSegment` (None: stop at the boundary). The fonts of the segment's bands and of its styles'
    /// bands are made here, on the caller's thread.
    pub fn play_segment(&mut self, segment: Option<Arc<Segment>>, timing: i32) -> bool {
        if let Some(s) = &segment {
            if !s.downloaded {
                return false;
            }
            for m in s.messages.lock().expect("no panics while locked").iter() {
                match &m.kind {
                    Kind::Band(Some(b)) => self.synth.prepare_fonts(b),
                    Kind::Style { style: Some(st), .. } => {
                        for b in &st.bands {
                            self.synth.prepare_fonts(b);
                        }
                    }
                    _ => {}
                }
            }
        }
        let msg = Message { time: 0, kind: Kind::Segment { segment, repeat: 0 } };
        let at = self.boundary(timing);
        self.control_queue.add(&msg, at, Conflict::Replace);
        true
    }

    /// `DmPerformance_playTransition`: a composed transition (the pattern of the embellishment, one measure), then
    /// the segment.
    pub fn play_transition(&mut self, segment: Arc<Segment>, embellishment: i32, timing: i32) -> bool {
        if !segment.downloaded {
            return false;
        }
        if self.segment.is_none() {
            return self.play_segment(Some(segment), timing);
        }
        let transition = self.compose_transition(segment, embellishment);
        self.play_segment(Some(Arc::new(transition)), timing)
    }

    /// `Dm_composeTransition`, its message times as dmusic wrote them: the chord (the last one handled) keeps its own
    /// time, and so the command after it (a C union's fields shared the time).
    fn compose_transition(&self, segment: Arc<Segment>, embellishment: i32) -> Segment {
        let mut length = 0;
        let mut messages = Vec::new();
        if embellishment != EMBELLISHMENT_NONE {
            if let Some(style) = &self.style {
                messages.push(Message { time: 0, kind: Kind::Tempo(style.tempo) });
                messages.push(Message { time: 0, kind: Kind::Band(self.band.clone()) });
                messages.push(Message {
                    time: 0,
                    kind: Kind::Style { reference: Reference::default(), style: Some(style.clone()) },
                });
                // a performance that never had a chord had dmusic's zeroed one: a note-off of note 0 at time 0
                let (time, chord) = match self.chord {
                    Some((t, c)) => (t, Kind::Chord(c)),
                    None => (0, Kind::Note { on: false, note: 0, velocity: 0, channel: 0 }),
                };
                messages.push(Message { time, kind: chord });
                let command = if embellishment == EMBELLISHMENT_END_AND_INTRO {
                    COMMAND_END
                } else {
                    embellishment_to_command(embellishment)
                };
                messages.push(Message {
                    time,
                    kind: Kind::Command(Command { command, groove_level: 1, groove_range: 0, repeat_mode: 4 }),
                });
                length = measure_length(style.time_signature);
            }
        }
        messages.push(Message { time: length, kind: Kind::Segment { segment: Some(segment), repeat: 0 } });
        Segment {
            name: Some("Composed Transition".into()),
            repeats: 1.into(),
            length,
            downloaded: true,
            messages: messages.into(),
            ..Segment::default()
        }
    }

    /// `DmPerformance_setVolume`.
    pub fn set_volume(&mut self, volume: f32) {
        self.synth.set_volume(volume);
    }

    /// `DmPerformance_setGrooveLevel`: added to every command's groove level from then on.
    pub fn set_groove_level(&mut self, level: u8) {
        self.groove_master = i32::from(level);
    }

    /// `DmPerformance_setReverb`.
    pub fn set_reverb(&mut self, enabled: bool, gain: f32, mix: f32, time: f32, hf_ratio: f32) {
        if enabled && self.reverb.is_none() {
            self.reverb = Some(Reverb::new(self.sample_rate as f32));
        }
        if let Some(r) = &mut self.reverb {
            r.set(gain, mix, time, hf_ratio);
        }
        self.reverb_on = enabled && self.reverb.is_some();
    }

    /// The earlier of the queues' first messages (the control queue's on a tie): (from the control queue, message).
    fn next_message(&self) -> Option<(bool, Message)> {
        match (self.control_queue.first(), self.music_queue.first()) {
            (None, None) => None,
            (Some(c), None) => Some((true, c.clone())),
            (None, Some(m)) => Some((false, m.clone())),
            (Some(c), Some(m)) => {
                if c.time <= m.time {
                    Some((true, c.clone()))
                } else {
                    Some((false, m.clone()))
                }
            }
        }
    }

    fn pop(&mut self, control: bool) {
        if control {
            self.control_queue.pop();
        } else {
            self.music_queue.pop();
        }
    }

    /// `DmPerformance_renderPcm` (float, interleaved stereo): `out.len()` samples.
    pub fn render(&mut self, out: &mut [f32]) {
        let len = out.len() & !1;
        let mut sample = 0usize;
        while sample < len {
            let Some((control, msg)) = self.next_message() else { break };
            let tps = ticks_per_sample(self.time_signature, self.tempo, self.sample_rate) / 2.0;
            let time_offset = (msg.time as i32).wrapping_sub(self.time as i32).max(0) as u32;
            let needed = (f64::from(time_offset) - self.time_carry) / tps;
            if needed >= (len - sample) as f64 + 1.0 {
                break;
            }
            let mut offset = if needed > 0.0 { needed as u32 } else { 0 };
            if offset as usize > len - sample {
                break;
            }
            offset += offset % 2;
            if offset > 0 {
                self.synth.render(&mut out[sample..sample + offset as usize]);
            }
            sample += offset as usize;
            self.time = self.time.wrapping_add(time_offset);
            self.time_carry += tps * f64::from(offset) - f64::from(time_offset);
            self.pop(control);
            self.handle(msg);
        }
        let remaining = (len - sample) as u32;
        self.synth.render(&mut out[sample..len]);
        let rendered = ticks_per_sample(self.time_signature, self.tempo, self.sample_rate) / 2.0 * f64::from(remaining)
            + self.time_carry;
        let whole = if rendered > 0.0 { rendered as u32 } else { 0 };
        self.time = self.time.wrapping_add(whole);
        self.time_carry = rendered - f64::from(whole);
        if self.reverb_on {
            if let Some(r) = &mut self.reverb {
                r.process_float(&mut out[..len]);
            }
        }
    }

    /// `DmPerformance_stop` (its EE patch): what is due within the look-ahead still sounds (a segment started just
    /// before, its band, its first notes), then every sounding note is released and the rest dropped.
    pub fn stop(&mut self, lookahead_ms: u32) {
        while let Some((control, msg)) = self.next_message() {
            let tempo = if self.tempo > 0.0 { self.tempo } else { 120.0 };
            let window = (f64::from(lookahead_ms) * tempo * 768.0 / 60000.0) as u32;
            if msg.time > self.time.wrapping_add(window) {
                break;
            }
            self.pop(control);
            self.handle(msg);
        }
        self.segment_change(None, 0);
    }

    fn handle(&mut self, msg: Message) {
        let time = msg.time;
        match msg.kind {
            Kind::Segment { segment, repeat } => self.segment_change(segment, repeat),
            Kind::Style { style, .. } => {
                if let Some(s) = style {
                    self.time_signature = s.time_signature;
                    self.style = Some(s);
                }
            }
            Kind::Band(band) => {
                if let Some(b) = band {
                    self.synth.band_update(&b);
                    self.band = Some(b);
                }
            }
            Kind::Tempo(t) => self.tempo = t,
            Kind::Command(c) => self.command(c),
            Kind::Chord(c) => self.chord = Some((time, c)),
            Kind::Note { on, note, velocity, channel } => {
                if on {
                    self.synth.note_on(channel, note, velocity);
                } else {
                    self.synth.note_off(channel, note);
                }
            }
            Kind::Control { control, value, channel, reset, reset_value } => {
                self.synth.control(channel, control, value);
                self.synth.control_reset(channel, control, if reset { reset_value } else { value });
            }
            Kind::PitchBend { channel, value, reset, reset_value } => {
                self.synth.pitch_bend(channel, value);
                if reset {
                    self.synth.pitch_bend_reset(channel, reset_value);
                }
            }
        }
    }

    /// `DmPerformance_handleCommandMessage`.
    fn command(&mut self, msg: Command) {
        if msg.command == COMMAND_GROOVE {
            self.groove_base = msg.groove_level;
            self.groove = (i32::from(msg.groove_level) + self.groove_master).clamp(0, 100) as u8;
            self.groove_range = msg.groove_range;
            if msg.groove_range != 0 {
                let g = i32::from(self.groove) + self.rng.range(i32::from(msg.groove_range));
                self.groove = g.clamp(0, 100) as u8;
            }
        }
        let mut measures_left = u32::MAX;
        if let Some(s) = &self.segment {
            let end = self.segment_start.wrapping_add(s.length);
            measures_left = if end > self.time { (end - self.time) / measure_length(self.time_signature) } else { 0 };
        }
        let Some(style) = self.style.clone() else { return };
        let Some(pattern) = choose_pattern(&style, u32::from(self.groove), msg.command, measures_left, &mut self.rng)
        else {
            return;
        };
        self.play_pattern(&style, pattern);
    }

    /// `DmPerformance_playPattern`.
    fn play_pattern(&mut self, style: &Style, pattern: &Pattern) {
        self.music_queue.clear();
        self.synth.note_off_everything();
        self.synth.reset();
        self.generate(pattern, style);
        let next = Message {
            time: 0,
            kind: Kind::Command(Command {
                command: COMMAND_GROOVE,
                groove_level: self.groove_base,
                groove_range: self.groove_range,
                repeat_mode: 0,
            }),
        };
        let length = measure_length(self.time_signature).wrapping_mul(u32::from(pattern.length_measures));
        self.music_queue.add(&next, self.time.wrapping_add(length), Conflict::Keep);
        self.variation = self.variation.wrapping_add(1);
    }

    /// `DmPattern_generateMessages`: each part's variation, sub-chord, notes and curves.
    fn generate(&mut self, pattern: &Pattern, style: &Style) {
        let mut variation = [-1i64; 256];
        let chord = self.chord.map(|c| c.1).unwrap_or_default();
        for pref in &pattern.parts {
            let Some(part) = style.find_part(pref) else { continue };
            let lock = pref.variation_lock_id as usize;
            if lock == 0 || variation[lock] == -1 {
                match pref.random_variation {
                    VARIATION_SEQUENTIAL | VARIATION_RANDOM_START => variation[lock] = i64::from(self.variation),
                    VARIATION_RANDOM | VARIATION_NO_REPEAT => variation[lock] = i64::from(self.rng.draw()),
                    _ => {}
                }
            }
            let count = part.valid_variation_count();
            if count == 0 {
                continue; // (dmusic divided by zero here)
            }
            let variation_id = 1u32.wrapping_shl((variation[lock] as u32) % count);
            let mut level = chord.subchords[0];
            for s in &chord.subchords[..chord.subchord_count as usize] {
                if s.levels & 1u32.wrapping_shl(u32::from(pref.subchord_level)) != 0 {
                    level = *s;
                    break;
                }
            }
            let channel = u32::from(pref.logical_part_id);
            self.generate_notes(part, level, variation_id, channel);
            self.generate_curves(part, variation_id, channel);
        }
    }

    /// `DmPattern_generateNoteMessages`.
    fn generate_notes(&mut self, part: &Part, chord: SubChord, variation: u32, channel: u32) {
        let time = self.time;
        for note in &part.notes {
            if note.variation & variation == 0 {
                continue;
            }
            let flags = if note.play_mode_flags == PLAY_NONE { part.play_mode_flags } else { note.play_mode_flags };
            let midi = if flags != PLAY_FIXED {
                music_value_to_midi(chord, flags, note.music_value)
            } else {
                i32::from(note.music_value)
            };
            if midi < 0 {
                continue;
            }
            let mut offset = time_offset(note.grid_start, i32::from(note.time_offset), part.time_signature);
            if note.time_range != 0 {
                offset = offset.wrapping_add(self.rng.range(note.time_range as i32) as u32);
            }
            let mut duration = note.duration as i32;
            if note.duration_range != 0 {
                duration = duration.wrapping_add(self.rng.range(note.duration_range as i32));
            }
            duration = duration.max(1);
            let mut velocity = i32::from(note.velocity);
            if note.velocity_range != 0 {
                velocity += self.rng.range(i32::from(note.velocity_range));
            }
            let velocity = velocity.clamp(1, 127) as u8;
            let start = (i64::from(time) + i64::from(offset as i32)).max(0) as u32;
            let on = Message { time: 0, kind: Kind::Note { on: true, note: midi as u8, velocity, channel } };
            self.music_queue.add(&on, start, Conflict::Append);
            let off = Message { time: 0, kind: Kind::Note { on: false, note: midi as u8, velocity, channel } };
            self.music_queue.add(&off, start.wrapping_add(duration as u32), Conflict::Append);
        }
    }

    /// `DmPattern_generateCurveMessages`: control changes and pitch bends every 5 ticks.
    fn generate_curves(&mut self, part: &Part, variation: u32, channel: u32) {
        let time = self.time;
        for curve in &part.curves {
            if curve.variation & variation == 0 {
                continue;
            }
            let start =
                time.wrapping_add(time_offset(curve.grid_start, i32::from(curve.time_offset), part.time_signature));
            let reset = curve.flags & 1 != 0;
            match curve.event_type {
                CURVE_PITCH_BEND => {
                    let mut prev = i32::from(curve.start_value);
                    for k in 0..curve.duration / CURVE_SPACING {
                        let offset = k * CURVE_SPACING;
                        let phase = offset as f32 / curve.duration as f32;
                        let value = curve_value(curve, phase) as i32;
                        if value == prev {
                            continue;
                        }
                        let kind = Kind::PitchBend { channel, value, reset, reset_value: value };
                        self.music_queue.add(&Message { time: 0, kind }, start.wrapping_add(offset), Conflict::Append);
                        prev = value;
                    }
                }
                CURVE_CONTROL_CHANGE => {
                    let in_range = |v: i16| (0..=127).contains(&v);
                    if !(in_range(curve.start_value) && in_range(curve.end_value)) {
                        continue;
                    }
                    let mut prev = f32::from(curve.start_value) - 1.0;
                    for k in 0..curve.duration / CURVE_SPACING {
                        let offset = k * CURVE_SPACING;
                        let phase = offset as f32 / curve.duration as f32;
                        let value = curve_value(curve, phase) / 127.0;
                        if value == prev {
                            continue;
                        }
                        let kind = Kind::Control {
                            control: curve.cc_data,
                            value,
                            channel,
                            reset,
                            reset_value: f32::from(curve.reset_value) / 127.0,
                        };
                        self.music_queue.add(&Message { time: 0, kind }, start.wrapping_add(offset), Conflict::Append);
                        prev = value;
                    }
                }
                _ => {}
            }
        }
    }

    /// `DmPerformance_handleSegmentMessage`: the playing segment stopped (queues cleared, notes released) and the
    /// next one's messages queued from its start, or nothing (None).
    fn segment_change(&mut self, segment: Option<Arc<Segment>>, repeat: u32) {
        self.segment = None;
        self.control_queue.clear();
        self.music_queue.clear();
        self.synth.note_off_everything();
        let Some(sgt) = segment else {
            self.style = None;
            self.band = None;
            self.time = 0;
            return;
        };
        self.time = 0;
        let start = if repeat != 0 { sgt.loop_start } else { sgt.play_start };
        let end = if repeat != 0 && sgt.loop_end != 0 { sgt.loop_end } else { sgt.length };
        {
            let mut messages = sgt.messages.lock().expect("no panics while locked");
            for m in messages.iter_mut() {
                if m.time == 0xffff_ffff {
                    m.time = start;
                }
                if m.time < start || m.time > end {
                    continue;
                }
                let at = self.time.wrapping_add(m.time).wrapping_sub(start);
                let conflict = if matches!(m.kind, Kind::Note { .. }) { Conflict::Append } else { Conflict::Replace };
                self.control_queue.add(m, at, conflict);
            }
        }
        let groove = Message { time: 0, kind: Kind::Command(Command { groove_level: 1, ..Command::default() }) };
        self.control_queue.add(&groove, 0, Conflict::Keep);
        self.segment_start = self.time;
        if repeat < sgt.repeats.load(Ordering::SeqCst) {
            let at = self.time.wrapping_add(sgt.length);
            let next = Message { time: 0, kind: Kind::Segment { segment: Some(sgt.clone()), repeat: repeat + 1 } };
            self.control_queue.add(&next, at, Conflict::Keep);
        }
        self.segment = Some(sgt);
    }
}

/// `Dm_embellishmentToCommand`.
fn embellishment_to_command(e: i32) -> u8 {
    match e {
        2 => COMMAND_FILL,
        3 => COMMAND_INTRO,
        4 => COMMAND_BREAK,
        5 => COMMAND_END,
        6 => COMMAND_END_AND_INTRO,
        _ => COMMAND_GROOVE,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_rng_is_the_classic_rand() {
        let mut r = Rng::new(1);
        assert_eq!([r.draw(), r.draw(), r.draw()], [16838, 5758, 10113]);
    }

    #[test]
    fn time_arithmetic() {
        let sig = TimeSignature { beats_per_measure: 3, beat: 8, grids_per_beat: 4 };
        assert_eq!(beat_length(sig), 384);
        assert_eq!(measure_length(sig), 1152);
        assert_eq!(beat_length(TimeSignature { beat: 0, ..sig }), 12);
        assert_eq!(time_offset(5, -1, sig), 384 + 96 - 1);
        assert!((ticks_per_sample(TimeSignature { beat: 4, ..sig }, 120.0, 48000) - 0.032).abs() < 1e-12);
    }

    #[test]
    fn music_values() {
        // a C major chord (C E G) on the major scale, root C (0): chord positions 0, 1, 2 are C, E, G
        let c = SubChord { chord_pattern: 0x91, scale_pattern: 0xab5ab5, levels: 1, chord_root: 0, scale_root: 0 };
        let mode = PLAY_CHORD_ROOT | PLAY_CHORD_INTERVALS;
        let base = music_value_to_midi(c, mode, 0x5000);
        assert_eq!(music_value_to_midi(c, mode, 0x5100) - base, 4);
        assert_eq!(music_value_to_midi(c, mode, 0x5200) - base, 7);
        assert_eq!(music_value_to_midi(c, mode, 0x6000) - base, 12);
        assert_eq!(music_value_to_midi(c, PLAY_KEY_ROOT | PLAY_CHORD_INTERVALS, 0x5000), -1);
        assert_eq!(music_value_to_midi(c, PLAY_CHORD_ROOT, 0x5000), -1);
        assert_eq!(music_value_to_midi(c, mode, 0x5001) - base, 1, "an accidental");
    }

    #[test]
    fn embellishments_are_tested_bit_by_bit() {
        assert_eq!(embellishment_flags(COMMAND_GROOVE), 0);
        assert_eq!(embellishment_flags(COMMAND_FILL), 5);
        assert_eq!(embellishment_flags(COMMAND_INTRO), 6);
        assert_eq!(embellishment_flags(COMMAND_BREAK), 7);
        assert_eq!(embellishment_flags(COMMAND_END), 8);
    }

    fn pattern(bottom: u8, top: u8, measures: u16) -> Pattern {
        Pattern { groove_bottom: bottom, groove_top: top, length_measures: measures, ..Pattern::default() }
    }

    #[test]
    fn the_pattern_that_fits() {
        let style = Style {
            patterns: vec![
                pattern(1, 10, 12),
                pattern(1, 10, 4),
                pattern(1, 10, 4),
                pattern(20, 30, 8),
                pattern(1, 10, 1),
            ],
            ..Style::default()
        };
        let mut rng = Rng::new(1);
        let p = |r: &mut Rng, groove, left| {
            choose_pattern(&style, groove, COMMAND_GROOVE, left, r).map(|p| p as *const Pattern as usize)
        };
        let at = |i: usize| Some(&style.patterns[i] as *const Pattern as usize);
        assert_eq!(p(&mut rng, 2, 100), at(0), "the longest that fits");
        let four = p(&mut rng, 2, 7);
        assert!(four == at(1) || four == at(2), "a 4-measure one");
        assert_eq!(p(&mut rng, 2, 3), at(0), "none fits: the longest");
        assert_eq!(p(&mut rng, 40, 100), at(3), "out of every range: the nearest");
        assert!(choose_pattern(&Style::default(), 2, COMMAND_GROOVE, 9, &mut rng).is_none());
    }
}
