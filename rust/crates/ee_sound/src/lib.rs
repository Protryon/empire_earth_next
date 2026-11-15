//! The Sound Engine's sound manager (`docs/RUST_PLAN.md` R3.6) behind the C++ `SESoundManager` facade: which channel
//! an instance gets, the category and instance limits and priorities, the paused queue of looping sounds, the
//! scavenger thread's decisions, one-shot sounds and the random start offsets; and `SEConductor`'s score table and
//! decisions (`conductor`), over dmusic's objects which stay C++; and the game's music manager's decisions
//! (`music`: `EEMusicManager`'s song changes, fades, intensity rules and timers); and the mixers' registry and the
//! Sound Engine's start and shutdown decisions (`mixers`).
//!
//! The objects are whole in Rust too (LLE lane round 6): the instance nodes and their pool, the samples and the
//! scores (`objects`), the mixers with their plug-in entry points and registry (`mixerobj`), the conductor on dmusic
//! (`conductorobj`) and the sound manager with its scavenger thread (`managerobj`); the C++ classes are data-less
//! forwarders. What they reach outside the crate (the Thread System, dmusic, the platform, the plug-in module and the
//! sound path) is in `sys`. The decisions see the objects through [`manager::Host`] and [`conductor::Host`].
//!
//! `golden_sound.txt` holds the C++ implementation's behaviour (scenario scripts over a fake mixer, and MSVC
//! `std::sort` permutations), dumped by a throwaway driver before the C++ was replaced.

pub mod conductor;
pub mod conductorobj;
pub mod manager;
pub mod managerobj;
pub mod mixerobj;
pub mod mixers;
pub mod msvc_sort;
pub mod music;
pub mod objects;
/// cbindgen:ignore
pub mod sys;

#[cfg(not(test))]
pub mod music_ffi;

#[cfg(test)]
mod fake;
#[cfg(test)]
mod golden;
#[cfg(test)]
mod golden_conductor;
#[cfg(test)]
mod golden_mixers;
#[cfg(test)]
mod golden_music;
#[cfg(test)]
mod golden_objects;
