//! The music engine: a DirectMusic performance in Rust, ported from GothicKit/dmusic (MIT Modern Variant, commit
//! b73b270b, with the EE patches it carried in src/ThirdParty/dmusic, removed with this port) and playing on
//! rustysynth, a SoundFont synthesizer, instead of TinySoundFont.
//!
//! The game's songs are DirectMusic segments (`.sgt`) on styles (`.sty`): a segment's command track sets the groove
//! level, the performance picks the style's pattern for it (plus the music manager's intensity), and the pattern's
//! notes follow the segment's chords. Their instruments were DLS collections (`.dls`); `tools/dls2sf2.py` converted
//! each into a SoundFont (`.sf2`) once, when the data is built, and the general MIDI set the original took from the
//! system is the SoundFont built into the program (`EE.sf2`, [`GENERAL_MIDI`]).
//!
//! - [`files`]: the segment, style and band files; [`loader`]: loading and the instrument collections;
//! - [`perf`]: the performance (queues, clock, patterns, transitions, stop); [`queue`]: its message heap;
//! - [`synth`]: the performance channels on rustysynth; [`reverb`]: the reverb on the mix;
//! - [`seam`]: the conductor's calls (dmusic's C names and calling convention, so the conductor and its golden
//!   replay are unchanged).

pub mod files;
pub mod loader;
pub mod perf;
pub mod queue;
pub mod reverb;
pub mod riff;
pub mod seam;
pub mod synth;

#[cfg(test)]
mod golden;

/// The general MIDI instruments of the songs that use them (harp, vibraphone, choir, oboe, orchestra, the drum kit's
/// triangle), built into the program: the original took them from the system's `gm.dls`, which the game's files do
/// not include. A `gm.sf2` in the music folder takes their place.
pub static GENERAL_MIDI: &[u8] = include_bytes!("EE.sf2");
