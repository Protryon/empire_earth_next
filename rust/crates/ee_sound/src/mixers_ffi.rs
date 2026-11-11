//! The C ABI of `SEMixer.cpp` and `Sound Engine.cpp`, and the C++ trampolines (`ee_mixerscpp_*`, defined in
//! `SEMixer.cpp`) through which the decisions reach the mixers, the active mixer and the plug-in table. Compiled out
//! of `cargo test`. The registry is a process global behind a lock that is never held across a trampoline (the
//! destructor a trampoline runs comes back for it).

use std::ffi::c_void;
use std::sync::Mutex;

use crate::mixers::{self, EngineHost, Host, Mixer, Registry};

/// cbindgen:ignore
mod sys {
    use super::*;
    extern "C" {
        pub(super) fn ee_mixerscpp_active() -> *mut c_void;
        pub(super) fn ee_mixerscpp_set_active(mixer: *mut c_void);
        pub(super) fn ee_mixerscpp_stop_music(mixer: *mut c_void) -> i32;
        pub(super) fn ee_mixerscpp_activate(mixer: *mut c_void) -> i32;
        pub(super) fn ee_mixerscpp_deactivate(mixer: *mut c_void) -> i32;
        pub(super) fn ee_mixerscpp_first_module(ctx: *mut c_void) -> i32;
        pub(super) fn ee_mixerscpp_next_module(ctx: *mut c_void) -> i32;
        pub(super) fn ee_mixerscpp_create_mixer(ctx: *mut c_void) -> *mut c_void;
        pub(super) fn ee_mixerscpp_is_valid(mixer: *mut c_void) -> bool;
        pub(super) fn ee_mixerscpp_delete_mixer(mixer: *mut c_void);
    }
}
use sys::*;

static REGISTRY: Mutex<Registry> = Mutex::new(Registry::new());

fn registry() -> std::sync::MutexGuard<'static, Registry> {
    REGISTRY.lock().unwrap_or_else(|e| e.into_inner())
}

fn mixer(p: *mut c_void) -> Option<Mixer> {
    (!p.is_null()).then_some(Mixer(p))
}

struct CppHost;

impl Host for CppHost {
    fn active(&self) -> Option<Mixer> {
        mixer(unsafe { ee_mixerscpp_active() })
    }
    fn set_active(&mut self, m: Option<Mixer>) {
        unsafe { ee_mixerscpp_set_active(m.map_or(std::ptr::null_mut(), |m| m.0)) }
    }
    fn stop_music(&mut self, m: Mixer) -> i32 {
        unsafe { ee_mixerscpp_stop_music(m.0) }
    }
    fn activate(&mut self, m: Mixer) -> i32 {
        unsafe { ee_mixerscpp_activate(m.0) }
    }
    fn deactivate(&mut self, m: Mixer) -> i32 {
        unsafe { ee_mixerscpp_deactivate(m.0) }
    }
}

/// `ctx` is the start's state (the directory and window, the module being walked) in `SEInitializeMixers`.
struct CppEngine {
    ctx: *mut c_void,
}

impl EngineHost for CppEngine {
    fn first_module(&mut self) -> i32 {
        unsafe { ee_mixerscpp_first_module(self.ctx) }
    }
    fn next_module(&mut self) -> i32 {
        unsafe { ee_mixerscpp_next_module(self.ctx) }
    }
    fn create_mixer(&mut self) -> Mixer {
        Mixer(unsafe { ee_mixerscpp_create_mixer(self.ctx) })
    }
    fn is_valid(&mut self, m: Mixer) -> bool {
        unsafe { ee_mixerscpp_is_valid(m.0) }
    }
    fn delete_mixer(&mut self, m: Mixer) {
        unsafe { ee_mixerscpp_delete_mixer(m.0) }
    }
    fn count(&mut self) -> u32 {
        registry().count()
    }
    fn first(&mut self) -> Option<Mixer> {
        registry().get(0).ok()
    }
}

/// The constructor: a new mixer joins the registry.
///
/// # Safety
/// `mixer` is a live `SEMixer*`.
#[no_mangle]
pub unsafe extern "C" fn ee_mixers_register(mixer: *mut c_void) {
    registry().register(Mixer(mixer))
}

/// The destructor's decisions: the active mixer stops and deactivates, then the mixer leaves the registry.
///
/// # Safety
/// `mixer` is the `SEMixer*` being destroyed.
#[no_mangle]
pub unsafe extern "C" fn ee_mixers_destroy(mixer: *mut c_void) {
    mixers::on_destroy(&mut CppHost, Mixer(mixer));
    registry().unregister(Mixer(mixer))
}

/// `SEMixer::GetCount`.
#[no_mangle]
pub extern "C" fn ee_mixers_count() -> u32 {
    registry().count()
}

/// `SEMixer::GetMixer`: `kErrorBeyondEnd` past the last (`out` untouched).
///
/// # Safety
/// `out` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_mixers_get(index: u32, out: *mut *mut c_void) -> i32 {
    match registry().get(index) {
        Ok(m) => {
            unsafe { *out = m.0 };
            mixers::OK
        }
        Err(e) => e,
    }
}

/// `SEMixer::UseMixer` (null: none).
///
/// # Safety
/// `mixer` is null or a live `SEMixer*`.
#[no_mangle]
pub unsafe extern "C" fn ee_mixers_use(mixer_ptr: *mut c_void) -> i32 {
    mixers::use_mixer(&mut CppHost, mixer(mixer_ptr))
}

/// `SEInitializeMixers` over the C++ start state `ctx`.
///
/// # Safety
/// `ctx` is what the trampolines expect.
#[no_mangle]
pub unsafe extern "C" fn ee_mixers_initialize(ctx: *mut c_void) -> i32 {
    mixers::initialize_mixers(&mut CppEngine { ctx })
}

/// `SEShutdown`'s mixers.
#[no_mangle]
pub extern "C" fn ee_mixers_delete_all() {
    mixers::delete_mixers(&mut CppEngine { ctx: std::ptr::null_mut() })
}
