//! The mixers' registry and the Sound Engine's start and shutdown decisions (`SEMixer.cpp`, `Sound Engine.cpp`):
//! which plug-ins become mixers (every module of the sound type in the live plug-in table whose plug-in says it is
//! valid), which mixer is active and how a change of the active mixer stops, deactivates and activates, and the
//! order mixers are deleted at shutdown.
//!
//! The objects stay C++: an `SEMixer` (its plug-in's entry points, its conductor), the `PIMModule`, the active
//! mixer pointer the renderers read inline (`SEMixer::sActiveMixer`). They are reached through [`Host`]; a mixer is
//! an opaque handle. The registry (`SEMixer::sMixers` before) is Rust's.
//!
//! The mixing itself (volumes, pan, pitch, the channels) is the plug-in's: `ee_mixer` (R3.1); the choice of channel
//! and the voice limits are the sound manager's: [`crate::manager`].

use std::ffi::c_void;

/// `kErrorOK`, `kErrorNotFound`, `kErrorNoMoreFiles`, `kErrorFailed`, `kErrorBeyondEnd`.
pub(crate) const OK: i32 = 0;
pub(crate) const NOT_FOUND: i32 = 3;
pub(crate) const NO_MORE_FILES: i32 = 7;
pub(crate) const FAILED: i32 = 9;
pub(crate) const BEYOND_END: i32 = 10;

/// An `SEMixer*` (never null).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Mixer(pub *mut c_void);

// SAFETY: a mixer handle is only an address here; the registry never dereferences it.
unsafe impl Send for Mixer {}

/// The registered mixers in creation order (`SEMixer::sMixers`).
#[derive(Debug, Default)]
pub struct Registry {
    pub mixers: Vec<Mixer>,
}

impl Registry {
    pub const fn new() -> Self {
        Registry { mixers: Vec::new() }
    }
    /// The constructor's `sMixers.push_back(this)`.
    pub fn register(&mut self, m: Mixer) {
        self.mixers.push(m);
    }
    /// The destructor's `find` + `erase` (the first entry; none: nothing).
    pub fn unregister(&mut self, m: Mixer) {
        if let Some(i) = self.mixers.iter().position(|&x| x == m) {
            self.mixers.remove(i);
        }
    }
    /// `GetMixer`.
    pub fn get(&self, index: u32) -> Result<Mixer, i32> {
        self.mixers.get(index as usize).copied().ok_or(BEYOND_END)
    }
    pub fn count(&self) -> u32 {
        self.mixers.len() as u32
    }
}

/// What the mixer decisions ask of the C++ side.
pub trait Host {
    /// `SEMixer::sActiveMixer`.
    fn active(&self) -> Option<Mixer>;
    fn set_active(&mut self, m: Option<Mixer>);
    /// `m->mConductor.SetConductorState(0)`.
    fn stop_music(&mut self, m: Mixer) -> i32;
    /// The plug-in's `SPIMActivateMixer` / `SPIMDeactivateMixer`.
    fn activate(&mut self, m: Mixer) -> i32;
    fn deactivate(&mut self, m: Mixer) -> i32;
}

/// `UseMixer`: the active mixer's music stopped and the mixer deactivated (either failing: `kErrorFailed`, and it
/// stays active), then the new one (None: none) activated.
pub fn use_mixer(h: &mut impl Host, m: Option<Mixer>) -> i32 {
    let mut r = OK;
    if let Some(a) = h.active() {
        r = h.stop_music(a);
        if r == OK {
            r = h.deactivate(a);
        }
        if r == OK {
            h.set_active(None);
        } else {
            r = FAILED;
        }
    }
    let Some(m) = m else {
        return r;
    };
    if r != OK {
        return r;
    }
    if h.activate(m) == OK {
        h.set_active(Some(m));
        OK
    } else {
        FAILED
    }
}

/// The destructor's part before it leaves the registry: the active mixer stops its music and deactivates (their
/// results ignored) and nothing is active any more.
pub fn on_destroy(h: &mut impl Host, m: Mixer) {
    if h.active() == Some(m) {
        h.stop_music(m);
        h.deactivate(m);
        h.set_active(None);
    }
}

/// What the Sound Engine's start and shutdown ask of the C++ side: the plug-in table's walk and the mixers' life.
pub trait EngineHost {
    /// `PIMModule::GetFirstModule(directory, ' snd', module)`; the module is kept for the next calls.
    fn first_module(&mut self) -> i32;
    /// `PIMModule::GetNextModule(module)`.
    fn next_module(&mut self) -> i32;
    /// `new SEMixer(module, window)` (it registers itself).
    fn create_mixer(&mut self) -> Mixer;
    /// `mixer->IsValid()` (the plug-in's `PIMIsValid`).
    fn is_valid(&mut self, m: Mixer) -> bool;
    /// `delete mixer` (it leaves the registry and unloads its module).
    fn delete_mixer(&mut self, m: Mixer);
    /// The registry's count and first mixer.
    fn count(&mut self) -> u32;
    fn first(&mut self) -> Option<Mixer>;
}

/// `SEInitializeMixers`: a mixer for every module of the sound type, the invalid ones deleted at once;
/// `kErrorNotFound` when none is left, else the table walk's result (`kErrorOK` at its end).
pub fn initialize_mixers(h: &mut impl EngineHost) -> i32 {
    let mut r = h.first_module();
    if r == OK {
        loop {
            let m = h.create_mixer();
            if !h.is_valid(m) {
                h.delete_mixer(m);
            }
            r = h.next_module();
            if r != OK {
                break;
            }
        }
    }
    if r == NO_MORE_FILES {
        r = if h.count() > 0 { OK } else { NOT_FOUND };
    }
    r
}

/// `SEShutdown`'s mixers: the first one deleted until none is left.
pub fn delete_mixers(h: &mut impl EngineHost) {
    while h.count() > 0 {
        match h.first() {
            Some(m) => h.delete_mixer(m),
            None => break,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn m(i: usize) -> Mixer {
        Mixer(i as *mut c_void)
    }

    #[test]
    fn registry_order_and_removal() {
        let mut r = Registry::new();
        r.register(m(1));
        r.register(m(2));
        r.register(m(1));
        assert_eq!(r.get(2), Ok(m(1)));
        assert_eq!(r.get(3), Err(BEYOND_END));
        r.unregister(m(1));
        assert_eq!(r.mixers, [m(2), m(1)]);
        r.unregister(m(7));
        assert_eq!(r.count(), 2);
    }
}
