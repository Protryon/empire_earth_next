//! The C interface of `effect_manager.rs`: the effect manager's creation plan and registration, its add and remove,
//! destruction queue, clear, repair, Load, Save and Update over the store (`ffi_mesh.rs`'s `EeGfxEffectStore`) and
//! the C++'s effects and goal markers (`EeGfxEffectManagerHost`). The store is borrowed for one operation at a time,
//! never across a callback.

use std::ffi::c_void;

use crate::effect_manager::{self, EeGfxEffectEntry, EeGfxEffectPlan, ManagerHost, StoreAccess};
use crate::effect_state::{EeGfxEffectRecord, EffectFile};
use crate::effect_store::{EffectPtr, EffectStore};
use crate::ffi_effect::{EeGfxEffectFile, File};
use crate::ffi_mesh::EeGfxEffectStore;

/// The effects and goal markers the manager calls (`context`: the C++'s; `effect`: an `EEEffect*`).
#[repr(C)]
pub struct EeGfxEffectManagerHost {
    pub context: *mut c_void,
    /// the effect's record (its kind, handle and saved flag)
    pub record: extern "C" fn(context: *mut c_void, effect: *mut c_void) -> *mut EeGfxEffectRecord,
    pub reference: extern "C" fn(context: *mut c_void, effect: *mut c_void),
    /// `EEEffect::Release` (may delete it)
    pub release: extern "C" fn(context: *mut c_void, effect: *mut c_void),
    pub set_up: extern "C" fn(context: *mut c_void, effect: *mut c_void) -> i32,
    pub step: extern "C" fn(context: *mut c_void, effect: *mut c_void) -> i32,
    pub repair: extern "C" fn(context: *mut c_void, effect: *mut c_void) -> i32,
    /// its graphic effect is above the detail setting
    pub above_detail: extern "C" fn(context: *mut c_void, effect: *mut c_void) -> bool,
    /// a load-time effect of a saved kind (null: none)
    pub new_loaded: extern "C" fn(context: *mut c_void, kind: u32) -> *mut c_void,
    /// the effect's Load and Save over the manager's file
    pub load: extern "C" fn(context: *mut c_void, effect: *mut c_void, file: *const EeGfxEffectFile) -> i32,
    pub save: extern "C" fn(context: *mut c_void, effect: *mut c_void, file: *const EeGfxEffectFile) -> i32,
    pub goals_update: extern "C" fn(context: *mut c_void),
    pub goals_load: extern "C" fn(context: *mut c_void, file: *const EeGfxEffectFile) -> i32,
    pub goals_repair: extern "C" fn(context: *mut c_void) -> i32,
}

struct Raw(*mut EeGfxEffectStore);

impl StoreAccess for Raw {
    fn with<R>(&mut self, f: impl FnOnce(&mut EffectStore) -> R) -> R {
        // SAFETY: the C++ passes the store `ee_gfx_effects_new` made; this borrow ends before any callback
        f(unsafe { &mut (*self.0).0 })
    }
}

struct Host<'a> {
    host: &'a EeGfxEffectManagerHost,
    file: *const EeGfxEffectFile,
}

impl Host<'_> {
    fn record(&self, e: EffectPtr) -> *mut EeGfxEffectRecord {
        (self.host.record)(self.host.context, e as *mut c_void)
    }
}

impl ManagerHost for Host<'_> {
    fn kind(&mut self, e: EffectPtr) -> i32 {
        // SAFETY: an effect's record lives as long as the effect; read by value
        unsafe { (*self.record(e)).kind }
    }
    fn handle(&mut self, e: EffectPtr) -> u32 {
        // SAFETY: as `kind`
        unsafe { (*self.record(e)).handle }
    }
    fn set_handle(&mut self, e: EffectPtr, handle: u32) {
        // SAFETY: as `kind`
        unsafe { (*self.record(e)).handle = handle }
    }
    fn saved(&mut self, e: EffectPtr) -> bool {
        // SAFETY: as `kind`
        unsafe { (*self.record(e)).saved }
    }
    fn set_saved(&mut self, e: EffectPtr, saved: bool) {
        // SAFETY: as `kind`
        unsafe { (*self.record(e)).saved = saved }
    }
    fn reference(&mut self, e: EffectPtr) {
        (self.host.reference)(self.host.context, e as *mut c_void)
    }
    fn release(&mut self, e: EffectPtr) {
        (self.host.release)(self.host.context, e as *mut c_void)
    }
    fn set_up(&mut self, e: EffectPtr) -> i32 {
        (self.host.set_up)(self.host.context, e as *mut c_void)
    }
    fn step(&mut self, e: EffectPtr) -> i32 {
        (self.host.step)(self.host.context, e as *mut c_void)
    }
    fn repair(&mut self, e: EffectPtr) -> i32 {
        (self.host.repair)(self.host.context, e as *mut c_void)
    }
    fn above_detail(&mut self, e: EffectPtr) -> bool {
        (self.host.above_detail)(self.host.context, e as *mut c_void)
    }
    fn new_loaded(&mut self, kind: u32) -> EffectPtr {
        (self.host.new_loaded)(self.host.context, kind) as EffectPtr
    }
    // the effects read and write the manager's own file (the one `file` wraps)
    fn load<F: EffectFile>(&mut self, e: EffectPtr, _file: &mut F) -> i32 {
        (self.host.load)(self.host.context, e as *mut c_void, self.file)
    }
    fn save<F: EffectFile>(&mut self, e: EffectPtr, _file: &mut F) -> i32 {
        (self.host.save)(self.host.context, e as *mut c_void, self.file)
    }
    fn goals_update(&mut self) {
        (self.host.goals_update)(self.host.context)
    }
    fn goals_load<F: EffectFile>(&mut self, _file: &mut F) -> i32 {
        (self.host.goals_load)(self.host.context, self.file)
    }
    fn goals_repair(&mut self) -> i32 {
        (self.host.goals_repair)(self.host.context)
    }
}

fn host<'a>(h: *const EeGfxEffectManagerHost, file: *const EeGfxEffectFile) -> Host<'a> {
    // SAFETY: the C++ passes a host alive for the call
    Host { host: unsafe { &*h }, file }
}

/// An entry point's choice for a graphic effect of `kind` (`detail_ok`: its detail level is within the setting;
/// `from_alive`/`to_alive`: the units are there and living).
#[no_mangle]
pub extern "C" fn ee_gfx_effects_plan(
    entry: EeGfxEffectEntry,
    kind: i32,
    detail_ok: bool,
    from_alive: bool,
    to_alive: bool,
) -> EeGfxEffectPlan {
    effect_manager::plan(entry, kind, detail_ok, from_alive, to_alive)
}

/// Registers an entry point's new effect (null: none): unsaved unless `saved`, set up, added under a new handle;
/// writes the handle to `out_handle` (null: not asked) when there is an effect. Returns the set-up's result (0: none).
///
/// # Safety
/// `s` is a store, `h` a host, `out_handle` null or writable.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effects_create(
    s: *mut EeGfxEffectStore,
    h: *const EeGfxEffectManagerHost,
    effect: *mut c_void,
    saved: bool,
    out_handle: *mut u32,
) -> i32 {
    let (result, handle) =
        effect_manager::create(&mut Raw(s), &mut host(h, std::ptr::null()), effect as EffectPtr, saved);
    if let Some(handle) = handle {
        if !out_handle.is_null() {
            // SAFETY: the function's contract
            unsafe { *out_handle = handle };
        }
    }
    result
}

/// `AddEffect`.
///
/// # Safety
/// `s` is a store, `h` a host, `effect` an effect.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effects_add_effect(
    s: *mut EeGfxEffectStore,
    h: *const EeGfxEffectManagerHost,
    effect: *mut c_void,
    new_handle: bool,
) {
    effect_manager::add_effect(&mut Raw(s), &mut host(h, std::ptr::null()), effect as EffectPtr, new_handle)
}

/// `RemoveEffect`.
///
/// # Safety
/// As `ee_gfx_effects_add_effect`.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effects_remove_effect(
    s: *mut EeGfxEffectStore,
    h: *const EeGfxEffectManagerHost,
    effect: *mut c_void,
) {
    effect_manager::remove_effect(&mut Raw(s), &mut host(h, std::ptr::null()), effect as EffectPtr)
}

/// `DestroyQueued`.
///
/// # Safety
/// `s` is a store, `h` a host.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effects_destroy_queued(
    s: *mut EeGfxEffectStore,
    h: *const EeGfxEffectManagerHost,
) -> i32 {
    effect_manager::destroy_queued(&mut Raw(s), &mut host(h, std::ptr::null()))
}

/// `ClearEffects`.
///
/// # Safety
/// `s` is a store, `h` a host.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effects_clear(s: *mut EeGfxEffectStore, h: *const EeGfxEffectManagerHost) {
    effect_manager::clear(&mut Raw(s), &mut host(h, std::ptr::null()))
}

/// `RepairPointers` (the goal markers' through the host).
///
/// # Safety
/// `s` is a store, `h` a host.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effects_repair(s: *mut EeGfxEffectStore, h: *const EeGfxEffectManagerHost) -> i32 {
    effect_manager::repair(&mut Raw(s), &mut host(h, std::ptr::null()))
}

/// `Load` from `file` (the save's version `major`.`minor`).
///
/// # Safety
/// `s` is a store, `h` a host, `file` a file.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effects_load(
    s: *mut EeGfxEffectStore,
    h: *const EeGfxEffectManagerHost,
    file: *const EeGfxEffectFile,
    major: u32,
    minor: u32,
) -> i32 {
    // SAFETY: the function's contract
    let mut f = File(unsafe { &*file });
    effect_manager::load(&mut Raw(s), &mut host(h, file), &mut f, major, minor)
}

/// `Save` to `file`.
///
/// # Safety
/// `s` is a store, `h` a host, `file` a file.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effects_save(
    s: *mut EeGfxEffectStore,
    h: *const EeGfxEffectManagerHost,
    file: *const EeGfxEffectFile,
) -> i32 {
    // SAFETY: the function's contract
    let mut f = File(unsafe { &*file });
    effect_manager::save(&mut Raw(s), &mut host(h, file), &mut f)
}

/// `Update`'s goal markers, destruction queue and step loop.
///
/// # Safety
/// `s` is a store, `h` a host.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effects_update(s: *mut EeGfxEffectStore, h: *const EeGfxEffectManagerHost) {
    effect_manager::update(&mut Raw(s), &mut host(h, std::ptr::null()))
}
