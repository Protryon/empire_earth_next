//! `extern "C"` for the scenario editor's effect types whole in Rust (`editor_effects.rs`): the object the effect
//! screen (`EESETSEffect`) holds in place of its seven `EESETSEffectType`s, made, called and freed by it, and the
//! effect types' own events (`EESEERust`'s military group, dialogue and AI attribute kinds). The one host is
//! `ee_screenscpp_seet` (`Scenario Editor/EESETSEffect.cpp`).

use core::ffi::c_void;

use ee_scenario::fields::EeScenarioEffectFields;

use crate::editor_effects::{EffectTypes, Fx};
use crate::editor_players::SeplHost;

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_seet(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
    }
}

struct Host;

impl SeplHost for Host {
    fn op(&mut self, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> i32 {
        let p = p.map(|x| x as *mut c_void);
        unsafe { sys::ee_screenscpp_seet(op, p.as_ptr(), i.as_mut_ptr(), f.as_mut_ptr()) }
    }
}

/// The effect types (opaque to C++).
pub struct EeScreensSeEffects(EffectTypes);

fn fx(
    h: &mut Host,
    me: *const EeScreensSeEffects,
    controls: *mut c_void,
    effect: *mut c_void,
    fields: *mut c_void,
) -> Fx<'_, Host> {
    Fx {
        h,
        controls: controls as usize,
        effect: effect as usize,
        fields: fields as *mut EeScenarioEffectFields,
        me: me as usize,
    }
}

/// The effect types' constructors (`EESETSEffect`'s `new`s): their events made. Freed by `ee_screens_seet_free`.
///
/// # Safety
/// `controls` is the effect screen's `EESETSEffectControls`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_seet_new(controls: *mut c_void) -> *mut EeScreensSeEffects {
    let mut h = Host;
    let mut c = fx(&mut h, core::ptr::null(), controls, core::ptr::null_mut(), core::ptr::null_mut());
    Box::into_raw(Box::new(EeScreensSeEffects(EffectTypes::new(&mut c))))
}

/// The destructors: the events released, the object freed.
///
/// # Safety
/// `types` comes from `ee_screens_seet_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_seet_free(types: *mut EeScreensSeEffects, controls: *mut c_void) {
    if types.is_null() {
        return;
    }
    let b = unsafe { Box::from_raw(types) };
    let mut h = Host;
    let mut c = fx(&mut h, types, controls, core::ptr::null_mut(), core::ptr::null_mut());
    b.0.destroy(&mut c);
}

/// The effect type `ty`'s (`kSMEffect*`) slot `slot` (`EE_SCREENS_SEET_CHANGE_ACTION` ...) over the controls and,
/// for the slots that take one, the effect (`EESMEffect*`) and its numbers (its `EeScenarioEffectFields`).
///
/// # Safety
/// `types` comes from `ee_screens_seet_new` (or is NULL); `fields` is `effect`'s numbers when the slot reads them.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_seet_call(
    types: *const EeScreensSeEffects,
    ty: u32,
    slot: u32,
    controls: *mut c_void,
    effect: *mut c_void,
    fields: *mut c_void,
) {
    if types.is_null() {
        return;
    }
    // shared: a host call can come back into the types (a combo box's event)
    let t = unsafe { &(*types).0 };
    let mut h = Host;
    let mut c = fx(&mut h, types, controls, effect, fields);
    t.call(&mut c, ty, slot);
}

/// The effect types' events (`EESEERust`, made by `EE_SCREENS_SEET_EV_OBJECTS` with these types and the controls):
/// 0 the military group's object moved in (`a` 1) or out (`a` 0) of the group (`b`: the list's line), 1 the
/// dialogue's camera scroll speed shown, 2 the AI attribute's ranges shown (`b`: the attribute combo box's line).
///
/// # Safety
/// `types` comes from `ee_screens_seet_new` (or is NULL); `controls` are the effect screen's.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_seet_event(
    types: *const EeScreensSeEffects,
    controls: *mut c_void,
    what: u32,
    a: u32,
    b: u32,
) {
    if types.is_null() {
        return;
    }
    let t = unsafe { &(*types).0 };
    let mut h = Host;
    let mut c = fx(&mut h, types, controls, core::ptr::null_mut(), core::ptr::null_mut());
    match what {
        0 => t.ai.move_object(&mut c, a != 0, b),
        1 => crate::editor_effects_more::show_camera_scroll_speed(&mut c),
        2 => t.ai.show_ranges(&mut c, b),
        _ => {}
    }
}
