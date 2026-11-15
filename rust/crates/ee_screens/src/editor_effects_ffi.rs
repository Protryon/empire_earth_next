//! `extern "C"` for the scenario editor's effect types whole in Rust (`editor_effects.rs`): their own events
//! (`EESEERust`'s military group, dialogue and AI attribute kinds; the effect screen, `editor_subscreens_ffi.rs`, owns
//! the types). The one host is `ee_screenscpp_seet` (`Scenario Editor/EESETSEffect.cpp`).

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

/// The effect types (opaque to C++; the effect screen's `EffectTypes`, whose address their events carry).
#[repr(transparent)]
pub struct EeScreensSeEffects(EffectTypes);

fn fx(h: &mut Host, me: *const EeScreensSeEffects, controls: *mut c_void) -> Fx<'_, Host> {
    Fx {
        h,
        controls: controls as usize,
        effect: 0,
        fields: core::ptr::null_mut::<EeScenarioEffectFields>(),
        me: me as usize,
    }
}

/// The effect types' events (`EESEERust`, made by `EE_SCREENS_SEET_EV_OBJECTS` with these types and the controls):
/// 0 the military group's object moved in (`a` 1) or out (`a` 0) of the group (`b`: the list's line), 1 the
/// dialogue's camera scroll speed shown, 2 the AI attribute's ranges shown (`b`: the attribute combo box's line).
///
/// # Safety
/// `types` are an effect screen's types (`ee_screens_sefx_new`; or NULL); `controls` are its controls.
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
    let mut c = fx(&mut h, types, controls);
    match what {
        0 => t.ai.move_object(&mut c, a != 0, b),
        1 => crate::editor_effects_more::show_camera_scroll_speed(&mut c),
        2 => t.ai.show_ranges(&mut c, b),
        _ => {}
    }
}
