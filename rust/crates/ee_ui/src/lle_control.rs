//! `UIControl.cpp`'s C++ logic in Rust over one-statement shims (UI lane round 10): the control's host's event
//! case ([`control_call`], plugged in as `ee_ui_control_host` before `UIControl::HostOp`), the shared UI keys the
//! first control looks up, the mouse dispatch's button and handler calls (`mouse_ffi.rs`) and the manager bitmap a
//! whole object asks for. The control's record and methods were already Rust (`controlobj.rs`, `mouse.rs`).

// the logic is the FFI hosts' (absent with no_cpp: a crate's tests that call the toolkit)
#![cfg_attr(feature = "no_cpp", allow(dead_code))]

use crate::lle_cpp::{lle_host, Cpp, HostArgs};
use crate::mouse::{ButtonQuery, ControlMouseCall};
use crate::obj::EeUiOp as Op;

/// The control's host's moved cases (`UIControl::HostOp`); `None` for the C++ host's.
pub(crate) fn control_call<C: Cpp + ?Sized>(c: &mut C, h: &HostArgs) -> Option<isize> {
    match h.op {
        Op::FormEventCall => {
            event_call4(c, h.pa(), h.b);
            Some(0)
        }
        _ => None,
    }
}

/// Event `event`'s `b`: 0 `Uninitialize()`, 1 `Dereference()`, 2 `Reference()`, 3 `Initialize()`.
pub(crate) fn event_call4<C: Cpp + ?Sized>(c: &mut C, event: usize, b: isize) {
    match b {
        0 => c.event_uninitialize(event),
        1 => c.event_dereference(event),
        2 => c.event_reference(event),
        _ => c.event_initialize(event),
    }
}

lle_host!(
    /// The control's host: [`control_call`], then `UIControl::HostOp`.
    ee_ui_control_host => control_call, ee_uicpp_control_host
);

/// The shared UI keys (`UIControl`'s statics in the constructor's order: Space, Enter, Control, Alt, End, Home, the
/// arrows, Delete, Backspace, the keypad's End, Home, arrows, Del and Enter, X, C, V, Insert) by their key codes.
const CONTROL_KEYS: [u32; 24] = [
    0x39, 0x1c, 0xe0, 0xe2, 0xcf, 0xc7, 0xcb, 0xcd, 0xc8, 0xd0, 0xd3, 0x0e, 0x4f, 0x47, 0x4b, 0x4d, 0x48, 0x50, 0x53,
    0x9c, 0x2d, 0x2e, 0x2f, 0xd2,
];

/// The first control's key lookup (`UIControl::UIControl`): every shared key, unless the space key is set.
pub(crate) fn fetch_keys<C: Cpp + ?Sized>(c: &mut C) {
    if c.control_keys_ready() {
        return;
    }
    for (slot, &code) in CONTROL_KEYS.iter().enumerate() {
        c.control_key_fetch(slot as u32, code);
    }
}

/// The mouse dispatch's question to a button (`ee_uicpp_control_mouse` 0..7 of old).
pub(crate) fn button<C: Cpp + ?Sized>(c: &mut C, right: bool, q: ButtonQuery) -> bool {
    let entity = if right { c.mouse_r_button() } else { c.mouse_l_button() };
    match q {
        ButtonQuery::JustDoublePressed => c.entity_just_double_pressed(entity),
        ButtonQuery::JustUnpressed => c.entity_just_unpressed(entity),
        ButtonQuery::Value => c.entity_value(entity) != 0,
        ButtonQuery::JustPressed => c.entity_just_pressed(entity),
    }
}

/// The mouse dispatch's handler call on the control (`location` the `GE2DPoint*` handed back to `MouseMove`).
pub(crate) fn handler<C: Cpp + ?Sized>(
    c: &mut C,
    control: usize,
    location: usize,
    call: ControlMouseCall,
) -> core::ffi::c_long {
    match call {
        ControlMouseCall::LeftDouble => c.control_mouse_left_double(control),
        ControlMouseCall::LeftUp => c.control_mouse_left_up(control),
        ControlMouseCall::LeftDown => c.control_mouse_left_down(control),
        ControlMouseCall::RightUp => c.control_mouse_right_up(control),
        ControlMouseCall::RightDown => c.control_mouse_right_down(control),
        ControlMouseCall::Move => c.control_mouse_move(control, location),
    }
}

/// The manager's bitmap index (`EeUiManagerCall::Bitmap`) of a whole object's `EeUiOp::ManagerBitmap` `a`: the scroll
/// bar's eight, the check box's six, the radio button's four.
const OBJ_MANAGER_BITMAPS: [u32; 18] = [2, 8, 3, 9, 6, 4, 7, 5, 11, 13, 12, 14, 15, 16, 17, 19, 18, 20];

/// `EeUiOp::ManagerBitmap`'s `a` as the manager's bitmap index.
#[no_mangle]
pub extern "C" fn ee_ui_obj_manager_bitmap(a: isize) -> u32 {
    OBJ_MANAGER_BITMAPS[a as usize]
}

/// The constructor's key lookup.
#[cfg(not(any(test, feature = "no_cpp")))]
#[no_mangle]
pub extern "C" fn ee_ui_control_fetch_keys() {
    fetch_keys(&mut crate::lle_cpp::Ffi);
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::lle_cpp::Fake;

    #[test]
    fn event_case_picks_by_b_and_other_ops_are_the_cpp_hosts() {
        let mut fake = Fake::default();
        let mut f = [0.0f32; 4];
        for b in 0..4 {
            assert_eq!(control_call(&mut fake, &HostArgs::new(1, Op::FormEventCall, [6, b, 0, 0], &mut f)), Some(0));
        }
        assert_eq!(control_call(&mut fake, &HostArgs::new(1, Op::CtlHide, [0; 4], &mut f)), None);
        assert_eq!(
            fake.calls(),
            "event_uninitialize(6)\nevent_dereference(6)\nevent_reference(6)\nevent_initialize(6)"
        );
    }

    #[test]
    fn keys_are_fetched_once_in_the_constructors_order() {
        let mut fake = Fake::default();
        fetch_keys(&mut fake);
        let calls = fake.calls();
        assert!(calls.starts_with("control_keys_ready()\ncontrol_key_fetch(0, 57)\ncontrol_key_fetch(1, 28)\n"));
        assert!(calls.ends_with("control_key_fetch(23, 210)"));
        assert_eq!(fake.log.len(), 25);
        let mut fake = Fake::default();
        fake.answer("control_keys_ready", &[1]);
        fetch_keys(&mut fake);
        assert_eq!(fake.log.len(), 1);
    }

    #[test]
    fn buttons_and_handlers() {
        let mut fake = Fake::default();
        fake.answer("mouse_l_button", &[3, 3]).answer("mouse_r_button", &[4, 4]).answer("entity_value", &[1]);
        assert!(!button(&mut fake, false, ButtonQuery::JustDoublePressed));
        assert!(button(&mut fake, true, ButtonQuery::Value));
        button(&mut fake, true, ButtonQuery::JustPressed);
        button(&mut fake, false, ButtonQuery::JustUnpressed);
        for call in [
            ControlMouseCall::LeftDouble,
            ControlMouseCall::LeftUp,
            ControlMouseCall::LeftDown,
            ControlMouseCall::RightUp,
            ControlMouseCall::RightDown,
            ControlMouseCall::Move,
        ] {
            handler(&mut fake, 9, 10, call);
        }
        assert_eq!(
            fake.calls(),
            "mouse_l_button()\nentity_just_double_pressed(3)\nmouse_r_button()\nentity_value(4)\nmouse_r_button()\n\
             entity_just_pressed(4)\nmouse_l_button()\nentity_just_unpressed(3)\ncontrol_mouse_left_double(9)\n\
             control_mouse_left_up(9)\ncontrol_mouse_left_down(9)\ncontrol_mouse_right_up(9)\n\
             control_mouse_right_down(9)\ncontrol_mouse_move(9, 10)"
        );
    }

    #[test]
    fn manager_bitmaps_map_to_the_managers_indices() {
        assert_eq!(ee_ui_obj_manager_bitmap(0), 2);
        assert_eq!(ee_ui_obj_manager_bitmap(7), 5);
        assert_eq!(ee_ui_obj_manager_bitmap(17), 20);
    }
}
