//! The live adapter of the trigger tab's composed host ops (`editor_trigx.rs`): the C++ statement hosts by range
//! (`ee_screenscpp_seetx` .. `_seinx`), the toolkit's calls (`EE_SCREENS_TRGX_R_*`) answered in Rust through
//! `ee_ui::direct`, the hosts' entries (an op composed, else `None` for the C++ host's) and the `extern "C"` the
//! C++ still calls (the place fills, the two sorts' comparisons, `EESETriggerJump`).

use core::ffi::{c_long, c_ulong, c_void};

use ee_ui::comboobj::EeUiComboCall;
use ee_ui::controlobj::EeUiControlCall;
use ee_ui::direct::{self, Cpp};
use ee_ui::editobj::EeUiEditCall;
use ee_ui::obj::EeUiOp;

use crate::screens::editor_direct::{combo_append, edit_line, edit_set_text, list_append};
use crate::screens::editor_trigx::{self as trgx, *};
use crate::screens::hud_direct::{label_set_text, resource_string};

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_seetx(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
        pub fn ee_screenscpp_seobx(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
        pub fn ee_screenscpp_setrx(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
        pub fn ee_screenscpp_setsx(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
        pub fn ee_screenscpp_searx(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
        pub fn ee_screenscpp_seinx(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
    }
}

/// The `n` bytes at `p`.
///
/// # Safety
/// `p` holds `n` bytes (or `n` is not positive).
unsafe fn bytes(p: usize, n: i32) -> Vec<u8> {
    if n <= 0 || p == 0 {
        return Vec::new();
    }
    // SAFETY: the contract
    unsafe { core::slice::from_raw_parts(p as *const u8, n as usize) }.to_vec()
}

/// A text into the statement's buffer (p2, capacity i3): its whole length.
///
/// # Safety
/// p2 holds i3 bytes.
unsafe fn out(p: [usize; 3], i: &[i32; 4], t: &[u8]) -> i32 {
    let n = t.len().min(i[3].max(0) as usize);
    if n > 0 {
        // SAFETY: the contract
        unsafe { core::ptr::copy_nonoverlapping(t.as_ptr(), p[2] as *mut u8, n) };
    }
    t.len() as i32
}

/// The found object into the word p2 points at.
///
/// # Safety
/// p2 is an out word.
unsafe fn found(p: [usize; 3], v: usize) -> i32 {
    // SAFETY: the contract
    unsafe { *(p[2] as *mut usize) = v };
    0
}

/// One statement of the family: a toolkit call answered here, else its file's C++ statement host.
fn statement(op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> i32 {
    let c = p[0] as Cpp;
    // SAFETY: the statements' contract (editor_trigx.rs): live controls and objects, p1 i0 bytes of a text, p2 the
    // out word or a buffer of i3 bytes
    unsafe {
        match op {
            EE_SCREENS_TRGX_R_LABEL_SET_TEXT => {
                label_set_text(c, &bytes(p[1], i[0]));
                0
            }
            EE_SCREENS_TRGX_R_EDIT_SET_TEXT => {
                edit_set_text(c, &bytes(p[1], i[0]));
                0
            }
            EE_SCREENS_TRGX_R_EDIT_LINE => out(p, i, &edit_line(c)),
            EE_SCREENS_TRGX_R_EDIT_ALL_TEXT => {
                let t = direct::edit_call(c, EeUiEditCall::AllText, 0, 0).unwrap_or(0);
                out(p, i, &direct::c_bytes(t as *const c_void))
            }
            EE_SCREENS_TRGX_R_COMBO_TEXT => {
                let label = direct::combo_call(c, EeUiComboCall::Part, 4, 0).unwrap_or(0) as Cpp;
                out(p, i, &crate::screens::hud_direct::label_text(label))
            }
            EE_SCREENS_TRGX_R_COMBO_APPEND => {
                combo_append(c, &bytes(p[1], i[0]));
                0
            }
            EE_SCREENS_TRGX_R_COMBO_CLEAR => {
                direct::combo_call(c, EeUiComboCall::DeleteAllEntries, 0, 0);
                0
            }
            EE_SCREENS_TRGX_R_COMBO_SELECT => {
                direct::combo_call(c, EeUiComboCall::SetCurrentSelection, i[0] as u32 as c_ulong as isize, 0);
                0
            }
            EE_SCREENS_TRGX_R_LIST_CLEAR => {
                direct::list_op(c, EeUiOp::ListDeleteAll, 0, 0, 0);
                0
            }
            EE_SCREENS_TRGX_R_LIST_APPEND => {
                list_append(c, &bytes(p[1], i[0]));
                0
            }
            EE_SCREENS_TRGX_R_TRIGGER_EVENT => {
                let e = direct::control_call(c, EeUiControlCall::TriggerEvent, i[0] as c_ulong as isize, 0);
                found(p, e as usize)
            }
            EE_SCREENS_TRGX_R_SET_TRIGGER => {
                direct::control_call(c, EeUiControlCall::SetTrigger, i[0] as c_ulong as isize, p[1] as isize);
                0
            }
            EE_SCREENS_TRGX_R_EVENT_CALL => {
                direct::control_virtual(c, EeUiOp::FormEventCall, p[1] as isize, i[0] as isize, &mut [0.0; 4]);
                0
            }
            EE_SCREENS_TRGX_R_STRING => match resource_string(i[0] as c_ulong as i64) {
                Some(t) => out(p, i, t),
                None => -1,
            },
            EE_SCREENS_TRGX_R_SLOT => {
                let s = crate::screens::editor_direct::slot(p[0], i[0] != 0, i[1] as u32, i[2] as u32);
                found(p, if s.is_null() { 0 } else { *s })
            }
            EE_SCREENS_TRGX_R_SETS_CONTROL => {
                let tab = p[0] as *const crate::screens::editor_tscreen_ffi::EeScreensSeTriggerTab;
                let control =
                    crate::screens::editor_tscreen_ffi::ee_screens_sets_control(tab, i[0] as u32, i[1] as u32);
                found(p, control as usize)
            }
            EE_SCREENS_TRGX_R_SETS_SUB_SELECT => {
                let tab = p[0] as *const crate::screens::editor_tscreen_ffi::EeScreensSeTriggerTab;
                crate::screens::editor_tscreen_ffi::ee_screens_sets_sub_select(tab, i[0] as u32, i[1]);
                0
            }
            EE_SCREENS_TRGX_R_SETS_CALL => {
                let tab = p[0] as *const crate::screens::editor_tscreen_ffi::EeScreensSeTriggerTab;
                crate::screens::editor_tscreen_ffi::ee_screens_sets_call(
                    tab,
                    i[0] as u32,
                    i[1] as u32,
                    0.0,
                    0.0,
                    core::ptr::null_mut(),
                );
                0
            }
            // the hide-UI event's two views (`EESETriggerScreen::Enter`'s inline `SetRectangles`): its Rust event
            // found through the box, the views set in Rust
            EE_SCREENS_SETSX_HIDE_UI_VIEWS => {
                let mut event = 0usize;
                let q = [p[0] as *mut c_void, p[1] as *mut c_void, &mut event as *mut usize as *mut c_void];
                sys::ee_screenscpp_setsx(EE_SCREENS_SETSX_CHECKED_EVENT, q.as_ptr(), i.as_mut_ptr(), f.as_mut_ptr());
                crate::screens::editor_whole_ffi::ee_screens_see_hide_ui_views(
                    event as *mut crate::screens::editor_whole_ffi::EeScreensSeeEvent,
                    f.as_ptr(),
                    p[1] as *const f32,
                );
                0
            }
            _ => {
                let q = p.map(|v| v as *mut c_void);
                let host = match op {
                    0..=199 => sys::ee_screenscpp_seetx,
                    200..=299 => sys::ee_screenscpp_seobx,
                    300..=399 => sys::ee_screenscpp_setrx,
                    400..=499 => sys::ee_screenscpp_setsx,
                    500..=599 => sys::ee_screenscpp_searx,
                    _ => sys::ee_screenscpp_seinx,
                };
                host(op, q.as_ptr(), i.as_mut_ptr(), f.as_mut_ptr())
            }
        }
    }
}

/// The live statements as the compositions take them.
fn live() -> impl FnMut(u32, [usize; 3], &mut [i32; 4], &mut [f32; 4]) -> i32 {
    statement
}

// ---- the hosts' entries (after editor_direct's toolkit answers; `None`: the C++ host's)

/// The effect (`condition` false) or condition screen's generic host.
pub(crate) fn seet(condition: bool, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> Option<i32> {
    trgx::seet(&mut live(), condition, op, p, i, f)
}

/// The object screen's host.
pub(crate) fn seob(op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> Option<i32> {
    trgx::seob(&mut live(), op, p, i, f)
}

/// The trigger screen's host.
pub(crate) fn setr(op: u32, p: [usize; 3], i: &mut [i32; 4]) -> Option<i32> {
    trgx::setr(&mut live(), op, p, i)
}

/// The trigger tab's host.
pub(crate) fn sets(op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> Option<i32> {
    trgx::sets(&mut live(), op, p, i, f)
}

/// The area screen's host.
pub(crate) fn sear(op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> Option<i32> {
    trgx::sear(&mut live(), op, p, i, f)
}

/// The instructions tab's host.
pub(crate) fn sein(op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> Option<i32> {
    trgx::sein(&mut live(), op, p, i, f)
}

// ---- the C ABI (the C++ that still calls these by name forwards here)

/// `FillPlaceComboBox(areas, label, combo)` ([`fill_place`]).
#[no_mangle]
pub extern "C" fn ee_screens_trgx_fill_place(areas: bool, label: *mut c_void, combo: *mut c_void) {
    trgx::fill_place(&mut live(), areas, label as usize, combo as usize);
}

/// `FillScenarioComboBox(combo, label, areas)` ([`fill_scenario`]).
#[no_mangle]
pub extern "C" fn ee_screens_trgx_fill_scenario(combo: *mut c_void, label: *mut c_void, areas: bool) {
    trgx::fill_scenario(&mut live(), combo as usize, label as usize, areas);
}

/// `FillAreaObjectComboBox(label, combo, areas)` ([`fill_area_object`]).
#[no_mangle]
pub extern "C" fn ee_screens_trgx_fill_area_object(label: *mut c_void, combo: *mut c_void, areas: bool) {
    trgx::fill_area_object(&mut live(), label as usize, combo as usize, areas);
}

/// `SeetTechNameLess`'s comparison ([`tech_less`]).
#[no_mangle]
pub extern "C" fn ee_screens_trgx_tech_less(a: c_long, b: c_long) -> bool {
    trgx::tech_less(&mut live(), a, b)
}

/// `EESETSOUnitTypeNameLess`'s comparison ([`unit_type_less`]).
#[no_mangle]
pub extern "C" fn ee_screens_trgx_unit_type_less(a: c_ulong, b: c_ulong) -> bool {
    trgx::unit_type_less(&mut live(), a, b)
}

/// `EESETriggerJump(kind, selection)` ([`trigger_jump`]).
#[no_mangle]
pub extern "C" fn ee_screens_trgx_trigger_jump(kind: c_ulong, selection: c_ulong) {
    trgx::trigger_jump(&mut live(), kind, selection);
}
