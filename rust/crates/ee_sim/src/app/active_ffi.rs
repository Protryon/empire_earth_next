//! The active unit lists' C ABI (rust/sim/app.h), called by EEWorld.cpp, EEPWorldHelper.cpp, EEPhysics.cpp and
//! EESim.cpp: the process's lists ([`Lists::world`]).

use super::active::Lists;

/// A unit's reference count changed by `delta` (the C++'s `Reference` / `Dereference`, applied at once).
pub type EeAppActiveRefsFn = unsafe extern "C" fn(u32, i32);
/// The C++'s own step between the queues' and the lists' references (`Clear`'s unborn deactivations).
pub type EeAppActiveBetweenFn = unsafe extern "C" fn();

/// `ActivateUnit` after its checks -> true when queued (take a reference).
#[no_mangle]
pub extern "C" fn ee_app_active_activate(id: u32) -> bool {
    Lists::world().activate(id)
}

/// `DeactivateUnit` for a unit with an id -> true when queued (take a reference; a born unit's reference moves).
#[no_mangle]
pub extern "C" fn ee_app_active_deactivate(id: u32) -> bool {
    Lists::world().deactivate(id)
}

/// `UpdateActiveUnits`' moves, the references changed through `refs`.
///
/// # Safety
/// `refs` is callable with any queued unit's id.
#[no_mangle]
pub unsafe extern "C" fn ee_app_active_update(refs: EeAppActiveRefsFn) {
    // SAFETY: the caller's contract
    Lists::world().update(&mut |id, delta| unsafe { refs(id, delta) });
}

/// Every list emptied, the references dropped through `refs` (`between` after the queues').
///
/// # Safety
/// As [`ee_app_active_update`]; `between` callable.
#[no_mangle]
pub unsafe extern "C" fn ee_app_active_clear(refs: EeAppActiveRefsFn, between: EeAppActiveBetweenFn) {
    // SAFETY: the caller's contract
    Lists::world().clear(&mut |id, delta| unsafe { refs(id, delta) }, &mut || unsafe { between() });
}

/// A unit stepped from now (two references taken by the caller).
#[no_mangle]
pub extern "C" fn ee_app_active_start(id: u32) {
    Lists::world().start(id)
}

/// The list `which` (0 stepped, 1 activation, 2 deactivation) into `out` (at most `cap`) -> its length.
///
/// # Safety
/// `out` holds `cap` ids (NULL with 0).
#[no_mangle]
pub unsafe extern "C" fn ee_app_active_list(which: u32, out: *mut u32, cap: usize) -> usize {
    let ids = Lists::world().list(which);
    for (k, id) in ids.iter().take(cap).enumerate() {
        // SAFETY: k < cap
        unsafe { *out.add(k) = *id };
    }
    ids.len()
}

/// The list `which` replaced by `n` ids.
///
/// # Safety
/// `ids` holds `n` ids (NULL with 0).
#[no_mangle]
pub unsafe extern "C" fn ee_app_active_set_list(which: u32, ids: *const u32, n: usize) {
    // SAFETY: the caller's contract
    let ids = if n == 0 { &[][..] } else { unsafe { std::slice::from_raw_parts(ids, n) } };
    Lists::world().set_list(which, ids)
}

/// A unit put in the stepped set or taken out of it.
#[no_mangle]
pub extern "C" fn ee_app_active_set(id: u32, on: bool) {
    Lists::world().set_active(id, on)
}

/// The list `which`'s id at `index` (0xffffffff past its end: the tick's step by index).
#[no_mangle]
pub extern "C" fn ee_app_active_at(which: u32, index: usize) -> u32 {
    Lists::world().at(which, index).unwrap_or(u32::MAX)
}
