//! The Sound Engine's sound manager (`docs/RUST_PLAN.md` R3.6) behind the C++ `SESoundManager` facade: which channel
//! an instance gets, the category and instance limits and priorities, the paused queue of looping sounds, the
//! scavenger thread's decisions, one-shot sounds and the random start offsets; and `SEConductor`'s score table and
//! decisions (`conductor`), over dmusic's objects which stay C++; and the game's music manager's decisions
//! (`music`: `EEMusicManager`'s song changes, fades, intensity rules and timers); and the mixers' registry and the
//! Sound Engine's start and shutdown decisions (`mixers`).
//!
//! C++ keeps the objects and their lifetimes: the instance nodes (pooled, intrusively reference counted, held by
//! units, unit classes and effects), the samples (shared with the mixer plug-in), the sound path, the mixer, the
//! scavenger thread and the manager's lock. Rust sees them as opaque handles through [`manager::Host`] and is called
//! with the C++ lock held where the C++ held it.
//!
//! `golden_sound.txt` holds the C++ implementation's behaviour (scenario scripts over a fake mixer, and MSVC
//! `std::sort` permutations), dumped by a throwaway driver before the C++ was replaced.

pub mod conductor;
pub mod manager;
pub mod mixers;
pub mod msvc_sort;
pub mod music;

#[cfg(not(test))]
pub mod conductor_ffi;
#[cfg(not(test))]
pub mod ffi;
#[cfg(not(test))]
pub mod mixers_ffi;
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
