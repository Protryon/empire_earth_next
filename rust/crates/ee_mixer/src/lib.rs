//! The sound mixer plug-in's logic (` snd`, `src/Low-Level Engine/Sound Engine/Mixer`): the sample channels, the
//! mixing arithmetic, the sample/channel bookkeeping. The device and decoding are miniaudio, in the C++ shim
//! (`Mixer.cpp`); `SESample` stays a C++ object reached through the `SampleHost` trait.

/// cbindgen:ignore
pub mod mixer;

#[cfg(not(test))]
pub mod ffi;

pub use mixer::{file_frame_bytes, Mixer, SampleData, SampleHost, CHANNEL_NONE};
