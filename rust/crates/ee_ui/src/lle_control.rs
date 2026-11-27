//! `UIControl.cpp`'s C++ logic in Rust over one-statement shims (UI lane round 10): the control's host's event
//! case ([`control_call`], plugged in as `ee_ui_control_host` before `UIControl::HostOp`), the shared UI keys the
//! first control looks up, the mouse dispatch's button and handler calls (`mouse_ffi.rs`) and the manager bitmap a
//! whole object asks for. The control's record and methods were already Rust (`controlobj.rs`, `mouse.rs`).

// the logic is the FFI hosts' (absent with no_cpp: a crate's tests that call the toolkit)
#![cfg_attr(feature = "no_cpp", allow(dead_code))]

use crate::lle_cpp::{Cpp, HostArgs};
use crate::mouse::{ButtonQuery, ControlMouseCall};
use crate::obj::EeUiOp as Op;
use core::ffi::c_void;
use core::sync::atomic::{AtomicUsize, Ordering};

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

/// The control's host: [`control_call`], then `UIControl::HostOp`.
///
/// # Safety
/// The C++ class's host contract: `obj` its live object, `f` the live floats its op reads and writes.
#[cfg(not(any(test, feature = "no_cpp")))]
#[no_mangle]
pub unsafe extern "C" fn ee_ui_control_host(
    obj: *mut core::ffi::c_void,
    op: crate::obj::EeUiOp,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
    f: *mut f32,
) -> isize {
    // SAFETY: the host contract, the arguments as given
    unsafe { crate::lle_cpp::host(control_call, &[crate::lle_hosts::control], obj, op, a, b, c, d, f) }
}

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

/// The shared UI keys and mouse buttons (UI lane round 13: once `UIControl`'s 24 key statics, `UIForm`'s Tab and
/// Shift and `UIManager`'s mouse buttons and wheel): the slot of each `ISEntity*` in [`ee_ui_key_slot`]'s record. The
/// first 24 are [`CONTROL_KEYS`]' order (`control_key_fetch`'s slot); the mouse three are
/// `mouse_button_fetch`'s order after [`EeUiKey::LeftMouse`].
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeUiKey {
    Space = 0,
    Enter,
    Control,
    Alt,
    End,
    Home,
    Left,
    Right,
    Up,
    Down,
    Delete,
    BackSpace,
    NumEnd,
    NumHome,
    NumLeft,
    NumRight,
    NumUp,
    NumDown,
    NumDelete,
    NumEnter,
    Cut,
    Copy,
    Paste,
    Insert,
    Tab,
    Shift,
    LeftMouse,
    RightMouse,
    MouseWheel,
}

const KEY_COUNT: usize = EeUiKey::MouseWheel as usize + 1;
#[allow(clippy::declare_interior_mutable_const)]
const NO_KEY: AtomicUsize = AtomicUsize::new(0);
/// The keys' `ISEntity*`s (0 until looked up), written by the C++ through [`ee_ui_key_slot`].
static KEYS: [AtomicUsize; KEY_COUNT] = [NO_KEY; KEY_COUNT];

/// `key`'s slot: the C++ `ISEntity*&` (`UIControl::Key`; `GetEntity`'s out reference).
#[no_mangle]
pub extern "C" fn ee_ui_key_slot(key: EeUiKey) -> *mut *mut c_void {
    KEYS[key as usize].as_ptr().cast()
}

/// The slot of the key numbered `i` in [`EeUiKey`]'s order ([`ee_ui_key_slot`]'s; none past the last: the first).
pub(crate) fn key_slot_at(i: u32) -> *mut *mut c_void {
    KEYS.get(i as usize).unwrap_or(&KEYS[0]).as_ptr().cast()
}

/// `key`'s entity (0 until looked up).
pub(crate) fn key(key: EeUiKey) -> usize {
    KEYS[key as usize].load(Ordering::Relaxed)
}

/// The key predicates (`UIControl::ControlKeyPressed` .. `UpKeyPressed`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeUiKeyPress {
    Control = 0,
    Copy,
    Cut,
    Delete,
    Down,
    End,
    Enter,
    Home,
    Left,
    Paste,
    Right,
    Shift,
    Up,
}

fn held<C: Cpp + ?Sized>(c: &mut C, k: EeUiKey) -> bool {
    c.entity_long_value(key(k)) != 0
}

/// Shift held or just pressed (`ShiftKeyPressed`).
fn shift<C: Cpp + ?Sized>(c: &mut C) -> bool {
    c.entity_just_pressed(key(EeUiKey::Shift)) || held(c, EeUiKey::Shift)
}

/// A key or, with Num Lock off, its keypad twin held.
fn keypad<C: Cpp + ?Sized>(c: &mut C, main: EeUiKey, num: EeUiKey) -> bool {
    held(c, main) || c.num_lock_off() && held(c, num)
}

/// `UIControl::<which>KeyPressed()` (the original's bodies, in their order of calls).
pub(crate) fn key_pressed<C: Cpp + ?Sized>(c: &mut C, which: EeUiKeyPress) -> bool {
    use EeUiKey as K;
    use EeUiKeyPress as P;
    match which {
        P::Control => c.entity_long_value(key(K::Control)) > 0,
        P::Copy => held(c, K::Control) && held(c, K::Copy),
        P::Cut => held(c, K::Control) && held(c, K::Cut) || shift(c) && held(c, K::Delete),
        P::Paste => held(c, K::Control) && held(c, K::Paste) || shift(c) && held(c, K::Insert),
        P::Delete => keypad(c, K::Delete, K::NumDelete),
        P::Down => keypad(c, K::Down, K::NumDown),
        P::End => keypad(c, K::End, K::NumEnd),
        P::Home => keypad(c, K::Home, K::NumHome),
        P::Left => keypad(c, K::Left, K::NumLeft),
        P::Right => keypad(c, K::Right, K::NumRight),
        P::Up => keypad(c, K::Up, K::NumUp),
        P::Enter => c.entity_just_unpressed(key(K::Enter)) || c.entity_just_unpressed(key(K::NumEnter)),
        P::Shift => shift(c),
    }
}

/// `UIControl::<which>KeyPressed()`.
#[cfg(not(any(test, feature = "no_cpp")))]
#[no_mangle]
pub extern "C" fn ee_ui_key_pressed(which: EeUiKeyPress) -> bool {
    key_pressed(&mut crate::lle_cpp::Ffi, which)
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

/// The border colours every control's border draws with (`UIControl::SetBorderColors`; once the statics
/// `sDarkBorder`, black, and `sLightBorder`, white): dark then light, red, green, blue.
static BORDER_COLORS: std::sync::Mutex<[[f32; 3]; 2]> = std::sync::Mutex::new([[0.0; 3], [1.0; 3]]);

/// `UIControl::SetBorderColors(dark, light)`.
///
/// # Safety
/// `dark` and `light` three live floats each.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_set_border_colors(dark: *const f32, light: *const f32) {
    // SAFETY: the contract
    let (d, l) = unsafe { ([*dark, *dark.add(1), *dark.add(2)], [*light, *light.add(1), *light.add(2)]) };
    *BORDER_COLORS.lock().unwrap_or_else(|e| e.into_inner()) = [d, l];
}

/// `UIControl::DrawBorder`: [`crate::draw2d::ee_ui_draw_border`] in the border colours.
///
/// # Safety
/// As `ee_ui_draw_border`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_control_draw_border(
    rasterizer: *mut c_void,
    screen: core::ffi::c_ulong,
    rect: crate::rect::UiRect,
    thickness: core::ffi::c_ulong,
    opacity: f32,
    border_type: core::ffi::c_ulong,
) -> core::ffi::c_long {
    let [dark, light] = *BORDER_COLORS.lock().unwrap_or_else(|e| e.into_inner());
    // SAFETY: the contract; the colours are three floats each
    unsafe {
        crate::draw2d::ee_ui_draw_border(
            rasterizer,
            screen,
            rect,
            thickness,
            opacity,
            border_type,
            light.as_ptr(),
            dark.as_ptr(),
        )
    }
}

/// UIControl's constructor: its two background rhombi made (UIControl.cpp's shims), the record's state
/// ([`crate::controlobj::ee_ui_control_init`] over [`ee_ui_control_host`]) with the border type, the shared keys looked up.
///
/// # Safety
/// `rec` is the live base of the C++ control `cpp`; `rect` four live floats.
#[cfg(not(any(test, feature = "no_cpp")))]
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_ui_control_construct(
    rec: *mut crate::controlobj::EeUiControlFields,
    cpp: *mut c_void,
    control_type: core::ffi::c_ulong,
    id: core::ffi::c_ulong,
    rect: *const f32,
    form: *mut c_void,
    index: core::ffi::c_ulong,
    border_thickness: core::ffi::c_ulong,
    border_type: core::ffi::c_ulong,
) {
    extern "C" {
        fn ee_uicpp_rhombus_new() -> *mut c_void;
    }
    // SAFETY: the contract; the rhombi are the record's before its state (the C++ constructor's order)
    unsafe {
        (*rec).mBgRhombus = ee_uicpp_rhombus_new();
        (*rec).mCopiedBgRhombus = ee_uicpp_rhombus_new();
        crate::controlobj::ee_ui_control_init(
            rec,
            cpp,
            Some(ee_ui_control_host),
            control_type,
            id,
            rect,
            form,
            index,
            border_thickness,
        );
        (*rec).mBorderType = border_type;
    }
    ee_ui_control_fetch_keys();
}

/// UIControl's destructor: every trigger event released, then the two rhombi deleted.
///
/// # Safety
/// `rec` a live record [`ee_ui_control_construct`] set up.
#[cfg(not(any(test, feature = "no_cpp")))]
#[no_mangle]
pub unsafe extern "C" fn ee_ui_control_destruct(rec: *mut crate::controlobj::EeUiControlFields) {
    extern "C" {
        fn ee_uicpp_rhombus_delete(rhombus: *mut c_void);
    }
    // SAFETY: the contract
    unsafe {
        crate::controlobj::ee_ui_control_release(rec);
        ee_uicpp_rhombus_delete((*rec).mBgRhombus);
        ee_uicpp_rhombus_delete((*rec).mCopiedBgRhombus);
    }
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

    #[test]
    fn key_predicates_read_the_slots_in_the_originals_order() {
        // the slots of keys no other test looks up
        let slot = |k: EeUiKey, v: usize| unsafe { *ee_ui_key_slot(k) = v as *mut c_void };
        slot(EeUiKey::Control, 30);
        slot(EeUiKey::Cut, 31);
        slot(EeUiKey::Shift, 32);
        slot(EeUiKey::Delete, 33);
        slot(EeUiKey::NumDelete, 34);
        slot(EeUiKey::Enter, 35);
        slot(EeUiKey::NumEnter, 36);
        let mut fake = Fake::default();
        fake.answer("entity_long_value", &[1, 0, 1, 1]).answer("entity_just_pressed", &[0]);
        assert!(key_pressed(&mut fake, EeUiKeyPress::Cut));
        assert_eq!(
            fake.calls(),
            "entity_long_value(30)\nentity_long_value(31)\nentity_just_pressed(32)\nentity_long_value(32)\n\
             entity_long_value(33)"
        );
        let mut fake = Fake::default();
        fake.answer("entity_long_value", &[0, 1]).answer("num_lock_off", &[1]);
        assert!(key_pressed(&mut fake, EeUiKeyPress::Delete));
        assert_eq!(fake.calls(), "entity_long_value(33)\nnum_lock_off()\nentity_long_value(34)");
        let mut fake = Fake::default();
        fake.answer("entity_long_value", &[-1]);
        assert!(!key_pressed(&mut fake, EeUiKeyPress::Control));
        let mut fake = Fake::default();
        fake.answer("entity_just_unpressed", &[0, 1]);
        assert!(key_pressed(&mut fake, EeUiKeyPress::Enter));
        assert_eq!(fake.calls(), "entity_just_unpressed(35)\nentity_just_unpressed(36)");
    }
}
