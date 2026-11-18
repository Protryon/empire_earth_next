//! The performance's synthesizer (dmusic's `Synth.c` on TinySoundFont; here on rustysynth, a SoundFont
//! synthesizer). The performance calls it through [`Sink`] (the golden replay logs those calls instead); [`Synth`]
//! keeps `Synth.c`'s performance channels (their font, volume, expression, pan and pitch with their reset values,
//! the transpose) and plays each on its collection's synthesizer.
//!
//! What differs from TinySoundFont is the synthesis itself (rustysynth's envelopes, filter, interpolation and its
//! 64-frame blocks: a message lands on the next block of its synthesizer, at most 1.3 ms late at 48 kHz). The rest is
//! `Synth.c`'s: the channel volume is (volume x expression)^2 (DLS's 40 log10 curves, rustysynth's own), the master
//! volume applies to the notes already sounding, a missing preset leaves the channel's last one.

use std::sync::Arc;

use rustysynth::{Synthesizer, SynthesizerSettings};

use super::files::{Band, INSTRUMENT_VALID_PAN, INSTRUMENT_VALID_TRANSPOSE, INSTRUMENT_VALID_VOLUME};
use super::loader::Collection;

/// The performance's calls into its synthesizer (dmusic's `DmSynth_*`).
pub trait Sink: Send {
    fn reset(&mut self);
    fn set_volume(&mut self, volume: f32);
    /// The fonts of a band's collections, made ahead of its band change (on the game's thread).
    fn prepare_fonts(&mut self, band: &Band);
    fn band_update(&mut self, band: &Band);
    fn control(&mut self, channel: u32, control: u8, value: f32);
    fn control_reset(&mut self, channel: u32, control: u8, reset: f32);
    fn pitch_bend(&mut self, channel: u32, bend: i32);
    fn pitch_bend_reset(&mut self, channel: u32, reset: i32);
    fn note_on(&mut self, channel: u32, note: u8, velocity: u8);
    fn note_off(&mut self, channel: u32, note: u8);
    fn note_off_everything(&mut self);
    /// Overwrites `out` (interleaved stereo) with the next frames.
    fn render(&mut self, out: &mut [f32]);
}

const CC_VOLUME: u8 = 7;
const CC_PAN: u8 = 10;
const CC_EXPRESSION: u8 = 11;
const PITCH_BEND_NEUTRAL: i32 = 8192;
const VOLUME_GM_DEFAULT: f32 = 100.0 / 127.0;
/// Voices per synthesizer (TinySoundFont's per collection were 128).
const MAX_VOICES: usize = 128;
/// The MIDI channels of one rustysynth synthesizer that carry performance channels (not 9: rustysynth adds 128 to
/// its bank selects, the percussion channel's).
const MIDI_CHANNELS: [i32; 15] = [0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14, 15];

/// One collection's synthesizers (`DmSynthFont`): a performance channel gets a MIDI channel on one of them the first
/// time it plays the collection, and keeps it (as it kept its TinySoundFont channel).
struct Font {
    collection: Arc<Collection>,
    synths: Vec<Synthesizer>,
    /// (performance channel, synthesizer, MIDI channel)
    channels: Vec<(u32, usize, i32)>,
}

impl Font {
    /// The performance channel's synthesizer and MIDI channel, given one on first use.
    fn midi(&mut self, channel: u32, rate: u32) -> Option<(usize, i32)> {
        if let Some(&(_, s, m)) = self.channels.iter().find(|c| c.0 == channel) {
            return Some((s, m));
        }
        let n = self.channels.len();
        let (s, m) = (n / MIDI_CHANNELS.len(), MIDI_CHANNELS[n % MIDI_CHANNELS.len()]);
        if s == self.synths.len() {
            let mut settings = SynthesizerSettings::new(rate as i32);
            settings.maximum_polyphony = MAX_VOICES;
            settings.enable_reverb_and_chorus = false; // the performance's own reverb runs on the mix
            let synth = Synthesizer::new(&self.collection.font, &settings).ok()?;
            self.synths.push(synth);
        }
        self.channels.push((channel, s, m));
        Some((s, m))
    }
}

/// `DmSynthChannel`.
#[derive(Clone, Copy, Default)]
struct Channel {
    font: Option<usize>,
    transpose: i32,
    reset_volume: f32,
    reset_expression: f32,
    volume: f32,
    expression: f32,
    reset_pan: f32,
    reset_pitch: i32,
}

/// `DmSynth` on rustysynth.
pub struct Synth {
    rate: u32,
    volume: f32,
    fonts: Vec<Font>,
    channels: Vec<Channel>,
    left: Vec<f32>,
    right: Vec<f32>,
}

/// A 0..1 value as a 14-bit controller (coarse, fine).
fn cc14(value: f32) -> (i32, i32) {
    let v = (value * 16383.0).round().clamp(0.0, 16383.0) as i32;
    (v >> 7, v & 0x7f)
}

impl Synth {
    pub fn new(rate: u32) -> Synth {
        Synth { rate, volume: 1.0, fonts: Vec::new(), channels: Vec::new(), left: Vec::new(), right: Vec::new() }
    }

    fn font_of(&self, collection: &Arc<Collection>) -> Option<usize> {
        self.fonts.iter().position(|f| Arc::ptr_eq(&f.collection, collection))
    }

    /// `DmSynth_updateFonts`: a font for each collection of the band that has none.
    fn update_fonts(&mut self, band: &Band) {
        for ins in &band.instruments {
            let Some(c) = ins.collection() else { continue };
            if self.font_of(&c).is_none() {
                self.fonts.push(Font { collection: c, synths: Vec::new(), channels: Vec::new() });
            }
        }
    }

    /// A MIDI message to the performance channel's synthesizer.
    fn send(&mut self, font: usize, channel: u32, command: i32, data1: i32, data2: i32) {
        let rate = self.rate;
        let f = &mut self.fonts[font];
        if let Some((s, m)) = f.midi(channel, rate) {
            let synth = &mut f.synths[s];
            synth.set_master_volume(self.volume);
            synth.process_midi_message(m, command, data1, data2);
        }
    }

    /// `DmSynth_applyVolume`: (volume x expression)^2, rustysynth's curve of CC 7 (with CC 11 at its full value).
    fn apply_volume(&mut self, channel: u32) {
        let c = self.channels[channel as usize];
        let Some(font) = c.font else { return };
        let (coarse, fine) = cc14(c.volume * c.expression);
        self.send(font, channel, 0xB0, 7, coarse);
        self.send(font, channel, 0xB0, 39, fine);
        self.send(font, channel, 0xB0, 11, 127);
        self.send(font, channel, 0xB0, 43, 127);
    }

    fn set_pan(&mut self, font: usize, channel: u32, pan: f32) {
        let (coarse, fine) = cc14(pan);
        self.send(font, channel, 0xB0, 10, coarse);
        self.send(font, channel, 0xB0, 42, fine);
    }

    fn set_pitch(&mut self, font: usize, channel: u32, wheel: i32) {
        let w = wheel.clamp(0, 0x3fff);
        self.send(font, channel, 0xE0, w & 0x7f, w >> 7);
    }

    /// The preset (bank, patch) of the collection, if it has it.
    fn has_preset(collection: &Collection, bank: i32, patch: i32) -> bool {
        collection.font.get_presets().iter().any(|p| p.get_bank_number() == bank && p.get_patch_number() == patch)
    }

    /// `DmSynth_assignInstrumentChannels`.
    fn assign_channels(&mut self, band: &Band) {
        let count = band.instruments.iter().map(|i| i.channel as usize).max().map_or(0, |m| m + 1);
        if count > self.channels.len() {
            self.channels.resize(count, Channel::default());
        }
        for ins in &band.instruments {
            let Some(collection) = ins.collection() else { continue };
            let ch = ins.channel;
            let chan = &mut self.channels[ch as usize];
            if chan.font.is_none() {
                chan.reset_volume = VOLUME_GM_DEFAULT;
                chan.reset_expression = 1.0;
                chan.volume = chan.reset_volume;
                chan.expression = chan.reset_expression;
                chan.reset_pan = 0.5;
                chan.reset_pitch = PITCH_BEND_NEUTRAL;
                chan.transpose = 0;
            }
            let font = self.font_of(&collection);
            self.channels[ch as usize].font = font;
            let Some(font) = font else { continue };
            // the band patch: MSB in bits 16-22, LSB in 8-14, the drum flag in 31
            let drum = ins.patch & 0x8000_0000 != 0;
            let mut bank =
                if drum { 128 } else { ((((ins.patch >> 16) & 0x7f) << 8) | ((ins.patch >> 8) & 0x7f)) as i32 };
            let patch = (ins.patch & 0x7f) as i32;
            if !collection.dls_numbering && !drum {
                bank >>= 8; // a SoundFont numbers its variation banks by the MSB alone
            }
            let preset = if Self::has_preset(&collection, bank, patch) {
                Some((bank, patch))
            } else if !collection.dls_numbering {
                // a SoundFont without that variation or drum kit: the capital tone or the standard kit
                let (b, p) = if drum { (128, 0) } else { (0, patch) };
                Self::has_preset(&collection, b, p).then_some((b, p))
            } else {
                None
            };
            if let Some((b, p)) = preset {
                self.send(font, ch, 0xB0, 0, b);
                self.send(font, ch, 0xC0, p, 0);
            }
            if ins.options & INSTRUMENT_VALID_PAN != 0 {
                let pan = f32::from(ins.pan) / 127.0;
                self.set_pan(font, ch, pan);
                self.channels[ch as usize].reset_pan = pan;
            }
            if ins.options & INSTRUMENT_VALID_VOLUME != 0 {
                let v = f32::from(ins.volume) / 127.0;
                self.channels[ch as usize].volume = v;
                self.channels[ch as usize].reset_volume = v;
            }
            self.apply_volume(ch);
            if ins.options & INSTRUMENT_VALID_TRANSPOSE != 0 {
                self.channels[ch as usize].transpose = i32::from(ins.transpose);
            }
        }
    }

    /// The channel's font, when the channel exists and has one.
    fn font(&self, channel: u32) -> Option<usize> {
        self.channels.get(channel as usize).and_then(|c| c.font)
    }
}

impl Sink for Synth {
    fn reset(&mut self) {
        for ch in 0..self.channels.len() as u32 {
            let c = &mut self.channels[ch as usize];
            let Some(font) = c.font else { continue };
            c.volume = c.reset_volume;
            c.expression = c.reset_expression;
            let (pan, pitch) = (c.reset_pan, c.reset_pitch);
            self.apply_volume(ch);
            self.set_pan(font, ch, pan);
            self.set_pitch(font, ch, pitch);
        }
    }

    fn set_volume(&mut self, volume: f32) {
        self.volume = volume;
        for f in &mut self.fonts {
            for s in &mut f.synths {
                s.set_master_volume(volume);
            }
        }
    }

    fn prepare_fonts(&mut self, band: &Band) {
        self.update_fonts(band);
    }

    fn band_update(&mut self, band: &Band) {
        self.update_fonts(band);
        self.assign_channels(band);
    }

    fn control(&mut self, channel: u32, control: u8, value: f32) {
        let Some(font) = self.font(channel) else { return };
        match control {
            CC_VOLUME => {
                self.channels[channel as usize].volume = value;
                self.apply_volume(channel);
            }
            CC_EXPRESSION => {
                self.channels[channel as usize].expression = value;
                self.apply_volume(channel);
            }
            CC_PAN => self.set_pan(font, channel, value),
            _ => {}
        }
    }

    fn control_reset(&mut self, channel: u32, control: u8, reset: f32) {
        if self.font(channel).is_none() {
            return;
        }
        let c = &mut self.channels[channel as usize];
        match control {
            CC_VOLUME => c.reset_volume = reset,
            CC_EXPRESSION => c.reset_expression = reset,
            CC_PAN => c.reset_pan = reset,
            _ => {}
        }
    }

    fn pitch_bend(&mut self, channel: u32, bend: i32) {
        if let Some(font) = self.font(channel) {
            self.set_pitch(font, channel, bend);
        }
    }

    fn pitch_bend_reset(&mut self, channel: u32, reset: i32) {
        if self.font(channel).is_some() {
            self.channels[channel as usize].reset_pitch = reset;
        }
    }

    fn note_on(&mut self, channel: u32, note: u8, velocity: u8) {
        let Some(font) = self.font(channel) else { return };
        let key = i32::from(note) + self.channels[channel as usize].transpose;
        self.send(font, channel, 0x90, key, i32::from(velocity));
    }

    fn note_off(&mut self, channel: u32, note: u8) {
        let Some(font) = self.font(channel) else { return };
        let key = i32::from(note) + self.channels[channel as usize].transpose;
        self.send(font, channel, 0x80, key, 0);
    }

    fn note_off_everything(&mut self) {
        for ch in 0..self.channels.len() as u32 {
            if let Some(font) = self.font(ch) {
                // release (not cut) every note of the channel on its font: rustysynth's All Notes Off
                self.send(font, ch, 0xB0, 0x7B, 0);
            }
        }
    }

    fn render(&mut self, out: &mut [f32]) {
        out.fill(0.0);
        let frames = out.len() / 2;
        if self.left.len() < frames {
            self.left.resize(frames, 0.0);
            self.right.resize(frames, 0.0);
        }
        for f in &mut self.fonts {
            for s in &mut f.synths {
                s.render(&mut self.left[..frames], &mut self.right[..frames]);
                for i in 0..frames {
                    out[2 * i] += self.left[i];
                    out[2 * i + 1] += self.right[i];
                }
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::dmusic::files::{Instrument, INSTRUMENT_VALID_PATCH};
    use std::sync::Mutex;

    fn general_midi() -> Arc<Collection> {
        Arc::new(Collection::parse(crate::dmusic::GENERAL_MIDI, [0; 16]).unwrap())
    }

    fn band(c: &Arc<Collection>, channel: u32, patch: u32) -> Band {
        Band {
            name: None,
            instruments: vec![Instrument {
                patch,
                channel,
                options: INSTRUMENT_VALID_PATCH | INSTRUMENT_VALID_VOLUME,
                volume: 127,
                collection: Mutex::new(Some(c.clone())),
                ..Instrument::default()
            }],
        }
    }

    fn peak(s: &mut Synth, frames: usize) -> f32 {
        let mut buf = vec![0f32; 2 * frames];
        s.render(&mut buf);
        buf.iter().fold(0f32, |m, v| m.max(v.abs()))
    }

    #[test]
    fn a_note_sounds_on_its_channel_and_stops() {
        let c = general_midi();
        let preset = &c.font.get_presets()[0];
        let patch = preset.get_patch_number() as u32 | ((preset.get_bank_number() as u32) << 16);
        let mut s = Synth::new(48000);
        assert_eq!(peak(&mut s, 64), 0.0, "no fonts: silence");
        let b = band(&c, 17, patch);
        s.prepare_fonts(&b);
        s.band_update(&b);
        s.note_on(17, 72, 120);
        assert!(peak(&mut s, 4800) > 0.001, "the note sounds");
        s.note_on(3, 72, 120); // a channel without an instrument
        s.note_off(17, 72);
        s.note_off_everything();
        let mut last = 1.0;
        for _ in 0..40 {
            last = peak(&mut s, 4800);
        }
        assert!(last < 0.0005, "released: {last}");
    }

    #[test]
    fn the_master_volume_scales_sounding_notes() {
        let c = general_midi();
        let p = &c.font.get_presets()[0];
        let patch = p.get_patch_number() as u32 | ((p.get_bank_number() as u32) << 16);
        let mut s = Synth::new(48000);
        let b = band(&c, 0, patch);
        s.band_update(&b);
        s.note_on(0, 60, 127);
        let _ = peak(&mut s, 2400);
        let loud = peak(&mut s, 960);
        s.set_volume(0.0);
        let _ = peak(&mut s, 64); // the block rendered before
        assert_eq!(peak(&mut s, 960), 0.0);
        assert!(loud > 0.0);
    }

    #[test]
    fn channels_take_midi_channels_in_turn_past_one_synthesizer() {
        let c = general_midi();
        let mut f = Font { collection: c, synths: Vec::new(), channels: Vec::new() };
        let mut got = Vec::new();
        for ch in 0..17 {
            got.push(f.midi(100 + ch, 48000).unwrap());
        }
        assert_eq!(got[8], (0, 8));
        assert_eq!(got[9], (0, 10), "never MIDI channel 9");
        assert_eq!(got[15], (1, 0));
        assert_eq!(f.midi(100, 48000), Some((0, 0)), "a channel keeps its MIDI channel");
        assert_eq!(f.synths.len(), 2);
        assert_eq!(cc14(1.0), (127, 127));
        assert_eq!(cc14(0.0), (0, 0));
        assert_eq!(cc14(2.0), (127, 127));
    }
}
