//! The mixer plug-in's table (LLE round 8, from `Mixer.cpp`): its result codes and the names of its entry points,
//! which the sound engine looks up one by one (`ee_soundcpp_module_entry`, ffi.rs).

use std::ffi::c_long;

/// `kSPIMOK`.
pub const SPIM_OK: c_long = 0;
/// `kSPIMNativeError`: the file could not be read.
pub const SPIM_NATIVE_ERROR: c_long = 1;
/// `kSPIMFailed`: the file could not be decoded.
pub const SPIM_FAILED: c_long = 9;
/// `kErrorNoMoreFiles` (`ee_soundcpp_next_module`).
pub const NO_MORE_FILES: i32 = 7;
/// The one plug-in's module (`SEMixerStart::mModule`: any non-null value; nothing reads it but the engine's walk).
pub const MODULE: usize = 1;

/// The entry points, in the table's order (ffi.rs `entries`).
pub const NAMES: [&[u8]; 32] = [
    b"SPIMActivateMixer",
    b"SPIMCreateChannels",
    b"SPIMDeactivateMixer",
    b"SPIMDeleteWaveData",
    b"SPIMDestroyChannels",
    b"SPIMDuplicateSample",
    b"SPIMGetCPUUsage",
    b"SPIMGetChannelCount",
    b"SPIMGetDirectSoundInterface",
    b"SPIMGetHighestTimerDelay",
    b"SPIMGetMaximumChannelCount",
    b"SPIMGetOutputRate",
    b"SPIMGetSampleOffset",
    b"SPIMGetSamplePlayBackTime",
    b"SPIMGetVolume",
    b"SPIMInitializeMixer",
    b"SPIMIsChannelBusyPlaying",
    b"SPIMLoadWaveData",
    b"SPIMMixAhead",
    b"SPIMPlayChannel",
    b"SPIMProcessMessage",
    b"SPIMSetChannelSample",
    b"SPIMSetLatency",
    b"SPIMSetMusicSource",
    b"SPIMSetSampleLooping",
    b"SPIMSetSampleOffset",
    b"SPIMSetSamplePan",
    b"SPIMSetSamplePitch",
    b"SPIMSetSampleVolume",
    b"SPIMSetVolume",
    b"SPIMStopAllChannels",
    b"SPIMStopChannel",
];

/// The table index of the entry point `name` (exact, case-sensitive: the C++ table's `strcmp`).
pub fn index(name: &[u8]) -> Option<usize> {
    NAMES.iter().position(|n| *n == name)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn every_name_the_sound_engine_asks_is_in_the_table() {
        // the sound engine's own list (sound mixerobj.rs NAMES, NUL-terminated there)
        for name in crate::sound::mixerobj::entry_names() {
            assert!(index(name).is_some(), "{}", String::from_utf8_lossy(name));
        }
        assert_eq!(index(b"SPIMLoadWaveData"), Some(17));
        assert_eq!(index(b"spimloadwavedata"), None);
        assert_eq!(index(b"SPIMNothing"), None);
        let mut sorted = NAMES.to_vec();
        sorted.sort();
        sorted.dedup();
        assert_eq!(sorted.len(), NAMES.len());
    }
}
