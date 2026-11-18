//! The DirectMusic files the game's music is made of, as dmusic parsed them (`io/Segment.c`, `io/Style.c`,
//! `io/Band.c`, `io/Common.c`): a segment (`.sgt`: its tracks become messages), a style (`.sty`: bands, parts with
//! their notes and curves, patterns referring to parts) and a band (the instruments of the performance channels).
//! The instruments' collections are SoundFonts here ([`super::loader::Collection`]), set when a band is downloaded.

use std::sync::atomic::AtomicU32;
use std::sync::{Arc, Mutex};

use super::loader::Collection;
use super::riff::{fourcc, Riff, LIST, RIFF};

pub type Guid = [u8; 16];

/// `DmReference`: a file reference (`DMRF`).
#[derive(Clone, Debug, Default)]
pub struct Reference {
    pub class_id: Guid,
    pub valid_data: u32,
    pub guid: Guid,
    pub name: Option<String>,
    pub file: Option<String>,
}

impl Reference {
    fn parse(r: &mut Riff) -> Reference {
        let mut s = Reference::default();
        while let Some(mut c) = r.chunk() {
            if c.is(fourcc(b"refh"), 0) {
                s.class_id = c.guid();
                s.valid_data = c.dword();
            } else if c.is(fourcc(b"guid"), 0) {
                s.guid = c.guid();
            } else if c.is(fourcc(b"name"), 0) {
                s.name = Some(c.utf16());
            } else if c.is(fourcc(b"file"), 0) {
                s.file = Some(c.utf16());
            }
        }
        s
    }
}

/// The UNFO list's name (`DmUnfo`).
fn unfo(r: &mut Riff) -> Option<String> {
    let mut name = None;
    while let Some(mut c) = r.chunk() {
        if c.is(fourcc(b"UNAM"), 0) {
            name = Some(c.utf16());
        }
    }
    name
}

/// `DmTimeSignature`.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct TimeSignature {
    pub beats_per_measure: u8,
    pub beat: u8,
    pub grids_per_beat: u16,
}

impl TimeSignature {
    fn parse(r: &mut Riff) -> TimeSignature {
        TimeSignature { beats_per_measure: r.byte(), beat: r.byte(), grids_per_beat: r.word() }
    }
}

// `DmInstrumentOptions`
pub const INSTRUMENT_VALID_PATCH: u32 = 1 << 0;
pub const INSTRUMENT_VALID_BANKSELECT: u32 = 1 << 1;
pub const INSTRUMENT_VALID_PAN: u32 = 1 << 5;
pub const INSTRUMENT_VALID_VOLUME: u32 = 1 << 6;
pub const INSTRUMENT_VALID_TRANSPOSE: u32 = 1 << 7;
pub const INSTRUMENT_GENERAL_MIDI: u32 = 1 << 8;
pub const INSTRUMENT_ROLAND_GS: u32 = 1 << 9;
pub const INSTRUMENT_YAMAHA_XG: u32 = 1 << 10;

/// `DmInstrument`: one performance channel's instrument in a band.
#[derive(Debug, Default)]
pub struct Instrument {
    /// MSB in bits 16-22, LSB in bits 8-14, the drum flag in bit 31, the patch in bits 0-6.
    pub patch: u32,
    pub channel: u32,
    pub options: u32,
    pub pan: u8,
    pub volume: u8,
    pub transpose: i16,
    pub reference: Reference,
    /// The collection, once downloaded (a band's instruments are downloaded on the game's thread, read on the
    /// audio thread's band changes).
    pub collection: Mutex<Option<Arc<Collection>>>,
}

impl Instrument {
    fn parse(r: &mut Riff) -> Instrument {
        let mut s = Instrument::default();
        while let Some(mut c) = r.chunk() {
            if c.is(fourcc(b"bins"), 0) {
                s.patch = c.dword();
                let _assign_patch = c.dword();
                for _ in 0..4 {
                    let _note_range = c.dword();
                }
                s.channel = c.dword();
                s.options = c.dword();
                s.pan = c.byte().min(127);
                s.volume = c.byte().min(127);
                s.transpose = c.short();
                let _channel_priority = c.dword();
            } else if c.is(LIST, fourcc(b"DMRF")) {
                s.reference = Reference::parse(&mut c);
            }
        }
        s
    }

    pub fn collection(&self) -> Option<Arc<Collection>> {
        self.collection.lock().expect("no panics while locked").clone()
    }

    /// The collection's file as dmusic kept it: a general MIDI, GS or XG instrument with a valid patch is from
    /// `gm.dls` (its download rewrote the reference).
    pub fn file(&self) -> Option<&str> {
        let valid = self.options & (INSTRUMENT_VALID_PATCH | INSTRUMENT_VALID_BANKSELECT) != 0;
        if valid && self.options & (INSTRUMENT_GENERAL_MIDI | INSTRUMENT_ROLAND_GS | INSTRUMENT_YAMAHA_XG) != 0 {
            Some("gm.dls")
        } else {
            self.reference.file.as_deref()
        }
    }
}

/// `DmBand`.
#[derive(Debug, Default)]
pub struct Band {
    pub name: Option<String>,
    pub instruments: Vec<Instrument>,
}

impl Band {
    /// `DmBand_parse` (None: a corrupt instrument list).
    pub fn parse(r: &mut Riff) -> Option<Band> {
        let mut s = Band::default();
        while let Some(mut c) = r.chunk() {
            if c.is(LIST, fourcc(b"UNFO")) {
                s.name = unfo(&mut c);
            } else if c.is(LIST, fourcc(b"lbil")) {
                let n = c.count();
                for _ in 0..n {
                    let mut i = c.chunk()?;
                    if !i.is(LIST, fourcc(b"lbin")) {
                        return None;
                    }
                    s.instruments.push(Instrument::parse(&mut i));
                }
            }
        }
        Some(s)
    }
}

// `DmPlayModeFlags`
pub const PLAY_FIXED: u8 = 0;
pub const PLAY_KEY_ROOT: u8 = 1;
pub const PLAY_CHORD_ROOT: u8 = 2;
pub const PLAY_SCALE_INTERVALS: u8 = 4;
pub const PLAY_CHORD_INTERVALS: u8 = 8;
pub const PLAY_NONE: u8 = 16;

/// `DmNote`.
#[derive(Clone, Copy, Debug, Default)]
pub struct Note {
    pub grid_start: u32,
    pub variation: u32,
    pub duration: u32,
    pub time_offset: i16,
    pub music_value: u16,
    pub velocity: u8,
    pub time_range: u32,
    pub duration_range: u32,
    pub velocity_range: u8,
    pub play_mode_flags: u8,
}

// `DmCurveType`, `DmCurveShape`
pub const CURVE_PITCH_BEND: u8 = 3;
pub const CURVE_CONTROL_CHANGE: u8 = 4;
pub const SHAPE_LINEAR: u8 = 0;
pub const SHAPE_INSTANT: u8 = 1;
pub const SHAPE_EXP: u8 = 2;
pub const SHAPE_LOG: u8 = 3;
pub const SHAPE_SINE: u8 = 4;

/// `DmCurve`.
#[derive(Clone, Copy, Debug, Default)]
pub struct Curve {
    pub grid_start: u32,
    pub variation: u32,
    pub duration: u32,
    pub time_offset: i16,
    pub start_value: i16,
    pub end_value: i16,
    pub reset_value: i16,
    pub event_type: u8,
    pub curve_shape: u8,
    pub cc_data: u8,
    pub flags: u8,
}

/// `DmPart`.
#[derive(Clone, Debug, Default)]
pub struct Part {
    pub time_signature: TimeSignature,
    pub variation_choices: [u32; 32],
    pub part_id: Guid,
    pub play_mode_flags: u8,
    pub notes: Vec<Note>,
    pub curves: Vec<Curve>,
}

impl Part {
    /// `DmPart_getValidVariationCount`.
    pub fn valid_variation_count(&self) -> u32 {
        self.variation_choices.iter().take_while(|&&v| v & 0x0FFF_FFFF != 0).count() as u32
    }
}

/// `DmVariationType`
pub const VARIATION_SEQUENTIAL: u8 = 0;
pub const VARIATION_RANDOM: u8 = 1;
pub const VARIATION_RANDOM_START: u8 = 2;
pub const VARIATION_NO_REPEAT: u8 = 3;

/// `DmPartReference`.
#[derive(Clone, Copy, Debug, Default)]
pub struct PartReference {
    pub part_id: Guid,
    pub logical_part_id: u16,
    pub variation_lock_id: u8,
    pub subchord_level: u8,
    pub random_variation: u8,
}

/// `DmPattern`.
#[derive(Clone, Debug, Default)]
pub struct Pattern {
    pub name: Option<String>,
    pub time_signature: TimeSignature,
    pub groove_bottom: u8,
    pub groove_top: u8,
    pub embellishment: u16,
    pub length_measures: u16,
    pub parts: Vec<PartReference>,
}

/// `DmStyle`.
#[derive(Debug, Default)]
pub struct Style {
    pub guid: Guid,
    pub name: Option<String>,
    pub time_signature: TimeSignature,
    pub tempo: f64,
    pub bands: Vec<Arc<Band>>,
    pub parts: Vec<Part>,
    pub patterns: Vec<Pattern>,
}

/// `DmStyle_toCorrectedRange`: a note's time or duration range byte.
fn corrected_range(range: u8) -> u32 {
    let r = u32::from(range);
    match range {
        0..=190 => r,
        191..=212 => ((r - 190) * 5) + 190,
        213..=232 => ((r - 212) * 10) + 300,
        _ => ((r - 232) * 50) + 500,
    }
}

/// The records of a `note` or `crve` chunk: the item size, then as many items as fit.
fn items(r: &mut Riff, mut item: impl FnMut(&mut Riff)) {
    let size = r.dword();
    if size == 0 {
        return;
    }
    let count = (r.len().wrapping_sub(r.pos)) / size;
    for _ in 0..count {
        let end = r.pos.wrapping_add(size);
        item(r);
        r.pos = end;
    }
}

impl Style {
    /// `DmStyle_parse` (None: not a RIFF file).
    pub fn parse(buf: &[u8]) -> Option<Style> {
        let mut rif = Riff::file(buf)?;
        let mut s = Style::default();
        while let Some(mut c) = rif.chunk() {
            if c.is(fourcc(b"styh"), 0) {
                s.time_signature = TimeSignature::parse(&mut c);
                s.tempo = c.double();
            } else if c.is(fourcc(b"guid"), 0) {
                s.guid = c.guid();
            } else if c.is(LIST, fourcc(b"UNFO")) {
                s.name = unfo(&mut c);
            } else if c.is(RIFF, fourcc(b"DMBD")) {
                s.bands.push(Arc::new(Band::parse(&mut c)?));
            } else if c.is(LIST, fourcc(b"part")) {
                s.parts.push(parse_part(&mut c));
            } else if c.is(LIST, fourcc(b"pttn")) {
                s.patterns.push(parse_pattern(&mut c));
            }
        }
        Some(s)
    }

    /// `DmStyle_findPart`.
    pub fn find_part(&self, r: &PartReference) -> Option<&Part> {
        self.parts.iter().find(|p| p.part_id == r.part_id)
    }
}

fn parse_part(r: &mut Riff) -> Part {
    let mut s = Part::default();
    while let Some(mut c) = r.chunk() {
        if c.is(fourcc(b"prth"), 0) {
            s.time_signature = TimeSignature::parse(&mut c);
            for v in s.variation_choices.iter_mut() {
                *v = c.dword();
            }
            s.part_id = c.guid();
            let _length_measures = c.word();
            s.play_mode_flags = c.byte();
        } else if c.is(fourcc(b"note"), 0) {
            items(&mut c, |c| {
                s.notes.push(Note {
                    grid_start: c.dword(),
                    variation: c.dword(),
                    duration: c.dword(),
                    time_offset: c.short(),
                    music_value: c.word(),
                    velocity: c.byte(),
                    time_range: corrected_range(c.byte()),
                    duration_range: corrected_range(c.byte()),
                    velocity_range: c.byte(),
                    play_mode_flags: {
                        let _inversion_id = c.byte();
                        c.byte()
                    },
                })
            });
        } else if c.is(fourcc(b"crve"), 0) {
            items(&mut c, |c| {
                let grid_start = c.dword();
                let variation = c.dword();
                let duration = c.dword();
                let _reset_duration = c.dword();
                s.curves.push(Curve {
                    grid_start,
                    variation,
                    duration,
                    time_offset: c.short(),
                    start_value: c.short(),
                    end_value: c.short(),
                    reset_value: c.short(),
                    event_type: c.byte(),
                    curve_shape: c.byte(),
                    cc_data: c.byte(),
                    flags: c.byte(),
                })
            });
        }
    }
    s
}

fn parse_pattern(r: &mut Riff) -> Pattern {
    let mut s = Pattern::default();
    while let Some(mut c) = r.chunk() {
        if c.is(fourcc(b"ptnh"), 0) {
            s.time_signature = TimeSignature::parse(&mut c);
            s.groove_bottom = c.byte();
            s.groove_top = c.byte();
            s.embellishment = c.word();
            s.length_measures = c.word();
        } else if c.is(LIST, fourcc(b"UNFO")) {
            s.name = unfo(&mut c);
        } else if c.is(LIST, fourcc(b"pref")) {
            let mut p = PartReference::default();
            while let Some(mut d) = c.chunk() {
                if d.is(fourcc(b"prfc"), 0) {
                    p.part_id = d.guid();
                    p.logical_part_id = d.word();
                    p.variation_lock_id = d.byte();
                    p.subchord_level = d.byte();
                    let _priority = d.byte();
                    p.random_variation = d.byte();
                }
            }
            s.parts.push(p);
        }
    }
    s
}

// `DmCommandType`
pub const COMMAND_GROOVE: u8 = 0;
pub const COMMAND_FILL: u8 = 1;
pub const COMMAND_INTRO: u8 = 2;
pub const COMMAND_BREAK: u8 = 3;
pub const COMMAND_END: u8 = 4;
pub const COMMAND_END_AND_INTRO: u8 = 5;

/// `DmMessage_Command`.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Command {
    pub command: u8,
    pub groove_level: u8,
    pub groove_range: u8,
    pub repeat_mode: u8,
}

/// One sub-chord of a chord.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct SubChord {
    pub chord_pattern: u32,
    pub scale_pattern: u32,
    pub levels: u32,
    pub chord_root: u8,
    pub scale_root: u8,
}

/// `DmMessage_Chord` without its name (only traced by dmusic).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Chord {
    pub subchord_count: u32,
    pub subchords: [SubChord; 4],
}

/// `DmMessage`'s kinds, in dmusic's order (the queue sorts equal times by it).
#[derive(Clone, Debug)]
pub enum Kind {
    Note { on: bool, note: u8, velocity: u8, channel: u32 },
    Control { control: u8, value: f32, channel: u32, reset: bool, reset_value: f32 },
    PitchBend { channel: u32, value: i32, reset: bool, reset_value: i32 },
    Segment { segment: Option<Arc<Segment>>, repeat: u32 },
    Style { reference: Reference, style: Option<Arc<Style>> },
    Band(Option<Arc<Band>>),
    Tempo(f64),
    Chord(Chord),
    Command(Command),
}

impl Kind {
    /// `DmMessageType`.
    pub fn rank(&self) -> u8 {
        match self {
            Kind::Note { .. } => 0,
            Kind::Control { .. } => 1,
            Kind::PitchBend { .. } => 2,
            Kind::Segment { .. } => 3,
            Kind::Style { .. } => 4,
            Kind::Band(_) => 5,
            Kind::Tempo(_) => 6,
            Kind::Chord(_) => 7,
            Kind::Command(_) => 8,
        }
    }
}

/// A message and its time in music ticks (768 a quarter note).
#[derive(Clone, Debug)]
pub struct Message {
    pub time: u32,
    pub kind: Kind,
}

/// `DmSegment`.
#[derive(Debug, Default)]
pub struct Segment {
    pub name: Option<String>,
    /// Set by the game's thread (`SEScore::SetRepeatCount`), read by the audio thread's segment changes.
    pub repeats: AtomicU32,
    pub length: u32,
    pub play_start: u32,
    pub loop_start: u32,
    pub loop_end: u32,
    pub guid: Guid,
    /// The tracks' messages in file order; a segment change sets a message at time -1 to the start (as dmusic did,
    /// in the segment itself).
    pub messages: Mutex<Vec<Message>>,
    pub downloaded: bool,
}

impl Segment {
    /// `DmSegment_parse` (None: not a RIFF file).
    pub fn parse(buf: &[u8]) -> Option<Segment> {
        let mut rif = Riff::file(buf)?;
        let mut s = Segment::default();
        let mut messages = Vec::new();
        while let Some(mut c) = rif.chunk() {
            if c.is(fourcc(b"segh"), 0) {
                s.repeats = AtomicU32::new(c.dword());
                s.length = c.dword();
                s.play_start = c.dword();
                s.loop_start = c.dword();
                s.loop_end = c.dword();
                let _resolution = c.dword();
            } else if c.is(fourcc(b"guid"), 0) {
                s.guid = c.guid();
            } else if c.is(LIST, fourcc(b"UNFO")) {
                s.name = unfo(&mut c);
            } else if c.is(LIST, fourcc(b"trkl")) {
                // a track whose band does not parse ends the track list there (its messages so far stay), as
                // dmusic's DmSegment_parse ignored the list's result
                while let Some(mut t) = c.chunk() {
                    if t.is(RIFF, fourcc(b"DMTK")) && parse_track(&mut t, &mut messages).is_none() {
                        break;
                    }
                }
            }
        }
        s.messages = Mutex::new(messages);
        Some(s)
    }

    pub fn repeats(&self) -> u32 {
        self.repeats.load(std::sync::atomic::Ordering::SeqCst)
    }
}

fn parse_track(r: &mut Riff, out: &mut Vec<Message>) -> Option<()> {
    while let Some(mut c) = r.chunk() {
        if c.is(fourcc(b"tetr"), 0) {
            items(&mut c, |c| {
                let time = c.dword();
                let _pad = c.dword();
                out.push(Message { time, kind: Kind::Tempo(c.double()) });
            });
        } else if c.is(fourcc(b"cmnd"), 0) {
            items(&mut c, |c| {
                let time = c.dword();
                let _measure = c.word();
                let _beat = c.byte();
                let command = c.byte();
                let groove_level = c.byte();
                let mut groove_range = c.byte();
                let mut repeat_mode = c.byte();
                if groove_range > 100 {
                    groove_range = 0;
                }
                if groove_range % 2 != 0 {
                    groove_range -= 1;
                }
                if repeat_mode > 5 {
                    repeat_mode = 0;
                }
                out.push(Message {
                    time,
                    kind: Kind::Command(Command { command, groove_level, groove_range, repeat_mode }),
                });
            });
        } else if c.is(LIST, fourcc(b"cord")) {
            while let Some(mut d) = c.chunk() {
                if d.is(fourcc(b"crdb"), 0) {
                    out.push(parse_chord(&mut d));
                }
            }
        } else if c.is(LIST, fourcc(b"sttr")) {
            while let Some(mut d) = c.chunk() {
                if d.is(LIST, fourcc(b"strf")) {
                    let mut time = 0;
                    let mut reference = Reference::default();
                    while let Some(mut e) = d.chunk() {
                        if e.is(fourcc(b"stmp"), 0) {
                            time = e.dword();
                        } else if e.is(LIST, fourcc(b"DMRF")) {
                            reference = Reference::parse(&mut e);
                        }
                    }
                    out.push(Message { time, kind: Kind::Style { reference, style: None } });
                }
            }
        } else if c.is(RIFF, fourcc(b"DMBT")) {
            while let Some(mut d) = c.chunk() {
                if !d.is(LIST, fourcc(b"lbdl")) {
                    continue;
                }
                while let Some(mut e) = d.chunk() {
                    if !e.is(LIST, fourcc(b"lbnd")) {
                        continue;
                    }
                    let mut time = 0;
                    let mut band = None;
                    while let Some(mut f) = e.chunk() {
                        if f.is(fourcc(b"bdih"), 0) {
                            time = f.dword();
                        } else if f.is(RIFF, fourcc(b"DMBD")) {
                            band = Some(Arc::new(Band::parse(&mut f)?));
                        }
                    }
                    out.push(Message { time, kind: Kind::Band(band) });
                }
            }
        }
    }
    Some(())
}

fn parse_chord(r: &mut Riff) -> Message {
    let size = r.dword();
    let end = r.pos.wrapping_add(size);
    let _name: [u8; 32] = r.read();
    let time = r.dword();
    r.pos = end;
    let mut chord = Chord { subchord_count: r.dword(), ..Chord::default() };
    if chord.subchord_count > 4 {
        chord.subchord_count = 4;
    }
    let size = r.dword();
    for i in 0..chord.subchord_count as usize {
        let end = r.pos.wrapping_add(size);
        let s = &mut chord.subchords[i];
        s.chord_pattern = r.dword();
        s.scale_pattern = r.dword();
        let _inversion_points = r.dword();
        s.levels = r.dword();
        s.chord_root = r.byte();
        s.scale_root = r.byte();
        r.pos = end;
    }
    Message { time, kind: Kind::Chord(chord) }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn corrected_ranges() {
        assert_eq!(corrected_range(190), 190);
        assert_eq!(corrected_range(191), 195);
        assert_eq!(corrected_range(212), 300);
        assert_eq!(corrected_range(213), 310);
        assert_eq!(corrected_range(232), 500);
        assert_eq!(corrected_range(255), 1650);
    }

    #[test]
    fn variation_count_stops_at_the_first_empty_choice() {
        let mut p = Part::default();
        p.variation_choices[0] = 1;
        p.variation_choices[1] = 0xF000_0000;
        p.variation_choices[2] = 4;
        assert_eq!(p.valid_variation_count(), 1);
    }

    #[test]
    fn a_non_riff_file_does_not_parse() {
        assert!(Segment::parse(b"RIFF").is_none());
        assert!(Style::parse(&[0; 4]).is_none());
    }
}
