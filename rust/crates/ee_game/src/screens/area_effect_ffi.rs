//! `extern "C"` for the area effects' objects whole in Rust (`area_effect_obj.rs`, UI lane round 12): the C++
//! members' forwards (`EEAreaEffect`, `EEAreaEffectTemplate`, `EEAEManager`, `EEAEPassFunctor::Apply`,
//! `EEUnitAreaEffectList::KeepStrongest`) and the capture's hook (`area_effect_capture.rs`). The objects cross as
//! addresses (`EEAreaEffect*`, `EEAreaEffectTemplate*`, `EEComplexUnit*`, `EEAEManager*`), as the shims take them;
//! the entries some shim lists name (`aem_covers`, `ael_strength`, ...) keep those shims' types.
#![allow(clippy::unnecessary_cast)] // c_long and c_ulong are 32 bits on Windows, 64 on LP64
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_long, c_ulong};
use std::sync::atomic::AtomicBool;

use ee_base::math::types::Ge2DPoint;
use ee_sim::tech::area_effect::{is_immunity, EeAreaEffectData, EeAreaEffectFields};

use crate::screens::area_effect_obj as obj;
use crate::screens::game_cpp::{Cpp, Ffi};

#[no_mangle]
pub extern "C" fn ee_screens_ae_construct(effect: usize, template: usize) {
    obj::construct(&mut Ffi, effect, template)
}

#[no_mangle]
pub extern "C" fn ee_screens_ae_destroy(effect: usize) {
    obj::destroy(&mut Ffi, effect)
}

/// `AppliedTo(unit)` (the shim list's `aem_applied_to`).
#[no_mangle]
pub extern "C" fn ee_screens_ae_applied_to(effect: usize, unit: usize) {
    obj::applied(&mut Ffi, effect, unit, false)
}

#[no_mangle]
pub extern "C" fn ee_screens_ae_removed_from(effect: usize, unit: usize) {
    obj::applied(&mut Ffi, effect, unit, true)
}

/// `Covers(player, unit, point)` (`aem_covers`).
#[no_mangle]
pub extern "C" fn ee_screens_ae_covers(effect: usize, player: u64, unit: usize, x: f32, y: f32) -> bool {
    obj::covers(&mut Ffi, effect, player, unit, x, y)
}

/// `GetStrength` (`ael_strength`).
#[no_mangle]
pub extern "C" fn ee_screens_ae_strength(effect: usize) -> i64 {
    obj::strength(&mut Ffi, effect) as i64
}

#[no_mangle]
pub extern "C" fn ee_screens_ae_level(effect: usize) -> i64 {
    obj::level(&mut Ffi, effect) as i64
}

#[no_mangle]
pub extern "C" fn ee_screens_ae_radius(effect: usize) -> f32 {
    obj::radius(&mut Ffi, effect)
}

#[no_mangle]
pub extern "C" fn ee_screens_ae_scale(effect: usize) -> f32 {
    obj::scale(&mut Ffi, effect)
}

/// `IsImmunity` (`aem_is_immunity`).
#[no_mangle]
pub extern "C" fn ee_screens_ae_is_immunity(effect: usize) -> bool {
    let p = Ffi.ae_fields(effect) as *const EeAreaEffectFields;
    // SAFETY: the live effect's record
    is_immunity(unsafe { &*p })
}

/// `Modify(kind, *value, *out)` (`ael_modify`: `value` and `out` the caller's longs, maybe the same).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ae_modify(effect: usize, kind: i64, value: usize, out: usize) -> bool {
    // SAFETY: the caller's long, read at the call (as the original's by-value copy)
    let v = unsafe { *(value as *const c_long) };
    match obj::modify(&mut Ffi, effect, kind as c_long, v) {
        Some(m) => {
            // SAFETY: the caller's out long
            unsafe { *(out as *mut c_long) = m };
            true
        }
        None => false,
    }
}

/// `MarkDirty` (`ael_mark_dirty`).
#[no_mangle]
pub extern "C" fn ee_screens_ae_mark_dirty(effect: usize) {
    obj::mark_dirty(&mut Ffi, effect)
}

/// `Recount(arg)` (`ael_recount`; the argument unread).
#[no_mangle]
pub extern "C" fn ee_screens_ae_recount(effect: usize, _value: f32) {
    obj::recount(&mut Ffi, effect)
}

/// `Tick(unit, elapsed)` (`ael_tick`).
#[no_mangle]
pub extern "C" fn ee_screens_ae_tick(effect: usize, unit: usize, elapsed: f32) {
    obj::tick(&mut Ffi, effect, unit, elapsed)
}

/// `ShowOn(unit)` (`ael_show_on`).
#[no_mangle]
pub extern "C" fn ee_screens_ae_show_on(effect: usize, unit: usize) {
    obj::show_on(&mut Ffi, effect, unit)
}

#[no_mangle]
pub extern "C" fn ee_screens_ae_set_unit(effect: usize, unit: usize) {
    obj::set_unit(&mut Ffi, effect, unit)
}

/// `IsInRange(point)` (`aem_in_range`).
#[no_mangle]
pub extern "C" fn ee_screens_ae_in_range(effect: usize, x: f32, y: f32) -> bool {
    obj::in_range(&mut Ffi, effect, x, y)
}

/// The effect's template's `AffectsPlayer(player, (long)(unsigned long)other)` (`aem_affects_player`).
#[no_mangle]
pub extern "C" fn ee_screens_ae_affects_player(effect: usize, player: i64, other: u64) -> bool {
    let t = Ffi.ae_template(effect);
    obj::template_affects(&mut Ffi, t, player as i32, other as c_ulong as c_long as i32, false)
}

/// The same reversed (`aem_affects_player_reversed`).
#[no_mangle]
pub extern "C" fn ee_screens_ae_affects_player_reversed(effect: usize, player: i64, other: u64) -> bool {
    let t = Ffi.ae_template(effect);
    obj::template_affects(&mut Ffi, t, player as i32, other as c_ulong as c_long as i32, true)
}

#[no_mangle]
pub extern "C" fn ee_screens_aet_construct(template: usize) {
    obj::template_construct(&mut Ffi, template)
}

#[no_mangle]
pub extern "C" fn ee_screens_aet_destroy(template: usize) {
    obj::template_destroy(&mut Ffi, template)
}

/// `AffectsPlayer(player, other)` / `AffectsPlayerReversed` (`reversed`) of a template.
#[no_mangle]
pub extern "C" fn ee_screens_aet_affects(template: usize, player: i32, other: i32, reversed: bool) -> bool {
    obj::template_affects(&mut Ffi, template, player, other, reversed)
}

/// `IsWithin(center, point, scale, unit)` of a template.
#[no_mangle]
pub extern "C" fn ee_screens_aet_is_within(
    template: usize,
    center_x: f32,
    center_y: f32,
    point_x: f32,
    point_y: f32,
    scale: f32,
    unit: usize,
) -> bool {
    let center = Ge2DPoint { x: center_x, y: center_y };
    obj::template_is_within(&mut Ffi, template, center, Ge2DPoint { x: point_x, y: point_y }, scale, unit)
}

/// `Init(data)` of a template (`data` an `EEAreaEffectData*`, the record's layout).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_aet_init(template: usize, data: usize) -> bool {
    // SAFETY: the caller's record, the same layout
    let d = unsafe { *(data as *const EeAreaEffectData) };
    obj::template_init(&mut Ffi, template, &d)
}

/// `EEAEPassFunctor::Apply(unit)`: one unit of a manager pass.
#[no_mangle]
pub extern "C" fn ee_screens_aem_pass(which: u32, arg: usize, player: u64, elapsed: f32, x: f32, y: f32, unit: usize) {
    obj::pass(&mut Ffi, which, arg, player, elapsed, x, y, unit)
}

#[no_mangle]
pub extern "C" fn ee_screens_aem_tick(manager: usize, elapsed: f32) {
    obj::manager_tick(&mut Ffi, manager, elapsed)
}

#[no_mangle]
pub extern "C" fn ee_screens_aem_clear(manager: usize) {
    obj::manager_clear(&mut Ffi, manager)
}

#[no_mangle]
pub extern "C" fn ee_screens_aem_clear_effects(manager: usize) {
    obj::clear_effects(&mut Ffi, manager)
}

#[no_mangle]
pub extern "C" fn ee_screens_aem_create(manager: usize, index: u64, unit: usize) -> usize {
    obj::create(&mut Ffi, manager, index, unit)
}

/// `LoadTemplateMedia(index)` / `UnloadTemplateMedia(index)` (`load` false).
#[no_mangle]
pub extern "C" fn ee_screens_aem_template_media(manager: usize, index: u64, load: bool) {
    obj::template_media(&mut Ffi, manager, index, load)
}

/// `EEUnitAreaEffectList::KeepStrongest`.
#[no_mangle]
pub extern "C" fn ee_screens_ael_keep_strongest(list: usize) {
    crate::screens::area_effect_lists::keep_strongest(&mut Ffi, list)
}

/// TEST ONLY: the capture's record after a tick (`EEAECaptureTick`'s target).
#[no_mangle]
pub extern "C" fn ee_screens_ae_capture_tick(elapsed: f32) {
    static TEMPLATES: AtomicBool = AtomicBool::new(false);
    crate::screens::area_effect_capture::tick(&mut Ffi, elapsed, &TEMPLATES, &mut ee_sim::app::capture::commit)
}
