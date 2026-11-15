//! `extern "C"` for the scenario editor's condition types whole in Rust (`editor_conditions.rs`): the object the
//! condition screen (`EESETSCondition`) holds in place of its seven `EESETSConditionType`s, made, called and freed by
//! it. The one host is `ee_screenscpp_sect` (`Scenario Editor/EESETSEffect.cpp`, the effect types' host's twin).

use core::ffi::c_void;

use ee_scenario::fields::EeScenarioConditionFields;

use crate::editor_conditions::{ConditionTypes, Cx};
use crate::editor_players::SeplHost;

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_sect(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
    }
}

struct Host;

impl SeplHost for Host {
    fn op(&mut self, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> i32 {
        let p = p.map(|x| x as *mut c_void);
        unsafe { sys::ee_screenscpp_sect(op, p.as_ptr(), i.as_mut_ptr(), f.as_mut_ptr()) }
    }
}

/// The condition types (opaque to C++).
pub struct EeScreensSeConditions(ConditionTypes);

fn cx(h: &mut Host, controls: *mut c_void, condition: *mut c_void, fields: *mut c_void) -> Cx<'_, Host> {
    Cx {
        h,
        controls: controls as usize,
        effect: condition as usize,
        fields: fields as *mut EeScenarioConditionFields,
        me: 0,
    }
}

/// The condition types' constructors (`EESETSCondition`'s `new`s): the player attribute type's event made. Freed
/// by `ee_screens_sect_free`.
///
/// # Safety
/// `controls` is the condition screen's `EESETSConditionControls`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sect_new(controls: *mut c_void) -> *mut EeScreensSeConditions {
    let mut h = Host;
    let mut c = cx(&mut h, controls, core::ptr::null_mut(), core::ptr::null_mut());
    Box::into_raw(Box::new(EeScreensSeConditions(ConditionTypes::new(&mut c))))
}

/// The destructors: the event released, the object freed.
///
/// # Safety
/// `types` comes from `ee_screens_sect_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sect_free(types: *mut EeScreensSeConditions, controls: *mut c_void) {
    if types.is_null() {
        return;
    }
    let b = unsafe { Box::from_raw(types) };
    let mut h = Host;
    let mut c = cx(&mut h, controls, core::ptr::null_mut(), core::ptr::null_mut());
    b.0.destroy(&mut c);
}

/// The condition type `ty`'s (`kSMCondition*`) slot `slot` (`EE_SCREENS_SEET_STORE`, `_HIDE`, `_LOAD`, `_SHOW`)
/// over the controls and, for the slots that take one, the condition (`EESMCondition*`) and its numbers (its
/// `EeScenarioConditionFields`).
///
/// # Safety
/// `types` comes from `ee_screens_sect_new` (or is NULL); `fields` is `condition`'s numbers when the slot reads them.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sect_call(
    types: *const EeScreensSeConditions,
    ty: u32,
    slot: u32,
    controls: *mut c_void,
    condition: *mut c_void,
    fields: *mut c_void,
) {
    if types.is_null() {
        return;
    }
    // shared: a host call can come back into the types (a radio button's event)
    let t = unsafe { &(*types).0 };
    let mut h = Host;
    let mut c = cx(&mut h, controls, condition, fields);
    t.call(&mut c, ty, slot);
}
