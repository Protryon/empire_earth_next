//! The condition and effect screens' controls and their toolkit ops answered in Rust (the scenario editor lane,
//! round 2) instead of the generic host's cases (`ee_screenscpp_seet` / `ee_screenscpp_sect`,
//! `Scenario Editor/EESETSEffect.cpp`).
//!
//! The two control records (`EESETSEffectControls`, `EESETSConditionControls`: plain arrays of control pointers) are
//! made and freed here; [`slot`] is the address of a member by array and index (`EE_SCREENS_SEET_A_*`), which the
//! C++'s remaining cases read through `ee_screens_seet_slot` (EESETSEffect.cpp asserts the layout). Every answer here
//! is its C++ case's statement made on the toolkit's Rust object through `ee_ui::direct` (the same inward call the
//! C++ shell made; the virtuals through the control's host, so an override runs). An op that is not here goes on to
//! the C++ host (`None`). As in `hud_direct.rs`, this sits below the hosts' trait (in the FFI adapters): the unit
//! tests' recorded op streams are unchanged by construction.

// (the casts of the C `unsigned long`s are no-ops on Windows and conversions on LP64)
#![allow(clippy::unnecessary_cast)]
#![cfg_attr(test, allow(dead_code))]

use core::ffi::{c_long, c_ulong, c_void};

use ee_ui::buttonobj::EeUiButtonCall;
use ee_ui::comboobj::EeUiComboCall;
use ee_ui::controlobj::EeUiControlCall;
use ee_ui::direct::{self, Cpp};
use ee_ui::editobj::EeUiEditCall;
use ee_ui::obj::EeUiOp;
use ee_ui::sliderobj::EeUiSliderCall;

use crate::screens::editor_controls::EE_SCREENS_SEET_A_EDIT48;
use crate::screens::editor_controls::EE_SCREENS_SEET_OP_SLOT;
use crate::screens::editor_effects::*;
use crate::screens::hud_direct::{atoi, label_set_long, label_set_text, label_text, resource_string};

/// One member array of a control record: its array number, first slot, length (0: a single member, its index
/// ignored, as the C++'s `mTextEditBox`, `mEditBox48` and the condition's `mSlider`).
type Member = (u32, usize, usize);

/// `EESETSEffectControls`' members in declaration order (46 pointers).
const EFFECT: [Member; 13] = [
    (EE_SCREENS_SEET_A_LABEL, 0, 13),
    (EE_SCREENS_SEET_A_EDIT, 13, 2),
    (EE_SCREENS_SEET_A_EDIT2, 15, 2),
    (EE_SCREENS_SEET_A_TEXT, 17, 0),
    (EE_SCREENS_SEET_A_EDIT48, 18, 0),
    (EE_SCREENS_SEET_A_COMBO, 19, 4),
    (EE_SCREENS_SEET_A_ECOMBO, 23, 2),
    (EE_SCREENS_SEET_A_SCEN, 25, 4),
    (EE_SCREENS_SEET_A_PLAYER, 29, 3),
    (EE_SCREENS_SEET_A_RADIO, 32, 6),
    (EE_SCREENS_SEET_A_SLIDER, 38, 4),
    (EE_SCREENS_SEET_A_CHECK, 42, 2),
    (EE_SCREENS_SEET_A_LIST, 44, 2),
];
/// The effect record's size in pointers.
pub const EE_SCREENS_SEET_EFFECT_SLOTS: usize = 46;

/// `EESETSConditionControls`' members in declaration order (23 pointers).
const CONDITION: [Member; 9] = [
    (EE_SCREENS_SEET_A_LABEL, 0, 5),
    (EE_SCREENS_SEET_A_EDIT, 5, 2),
    (EE_SCREENS_SEET_A_TEXT, 7, 0),
    (EE_SCREENS_SEET_A_COMBO, 8, 3),
    (EE_SCREENS_SEET_A_SCEN, 11, 4),
    (EE_SCREENS_SEET_A_CHECK, 15, 3),
    (EE_SCREENS_SEET_A_PLAYER, 18, 2),
    (EE_SCREENS_SEET_A_RADIO, 20, 2),
    (EE_SCREENS_SEET_A_SLIDER, 22, 0),
];
/// The condition record's size in pointers.
pub const EE_SCREENS_SEET_CONDITION_SLOTS: usize = 23;

/// The slot number of the member `a`, `k` (`None`: the record has no such member, or the index is past its array).
pub fn slot_index(condition: bool, a: u32, k: u32) -> Option<usize> {
    let table: &[Member] = if condition { &CONDITION } else { &EFFECT };
    let &(_, first, len) = table.iter().find(|m| m.0 == a)?;
    if len == 0 {
        return Some(first);
    }
    ((k as usize) < len).then_some(first + k as usize)
}

/// The address of the member `a`, `k` of the record `controls` (NULL: none).
///
/// # Safety
/// `controls` is NULL or a record from [`controls_new`] of the same kind.
pub unsafe fn slot(controls: usize, condition: bool, a: u32, k: u32) -> *mut usize {
    match slot_index(condition, a, k) {
        // SAFETY: the record holds the slot (the contract; `slot_index` bounds it)
        Some(n) if controls != 0 => unsafe { (controls as *mut usize).add(n) },
        _ => core::ptr::null_mut(),
    }
}

/// A new empty record (every member NULL): the C++'s `new EESETSEffectControls()` / `EESETSConditionControls()`.
pub fn controls_new(condition: bool) -> usize {
    let n = if condition { EE_SCREENS_SEET_CONDITION_SLOTS } else { EE_SCREENS_SEET_EFFECT_SLOTS };
    Box::into_raw(vec![0usize; n].into_boxed_slice()) as *mut usize as usize
}

/// Frees a record of [`controls_new`] (its controls were deleted before).
///
/// # Safety
/// `controls` is 0 or a record of [`controls_new`] of the same kind, not used again.
pub unsafe fn controls_free(controls: usize, condition: bool) {
    if controls == 0 {
        return;
    }
    let n = if condition { EE_SCREENS_SEET_CONDITION_SLOTS } else { EE_SCREENS_SEET_EFFECT_SLOTS };
    // SAFETY: the contract: the boxed slice `controls_new` leaked
    drop(unsafe { Box::from_raw(core::ptr::slice_from_raw_parts_mut(controls as *mut usize, n)) });
}

/// The member `a`, `k` of a record (`EESETSEffect.cpp`'s remaining cases' accessor; NULL: no such member).
///
/// # Safety
/// `controls` is NULL or a live record of the kind `condition` names.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_seet_slot(controls: *mut c_void, condition: bool, a: u32, k: u32) -> *mut c_void {
    // SAFETY: the contract
    let s = unsafe { slot(controls as usize, condition, a, k) };
    if s.is_null() {
        core::ptr::null_mut()
    } else {
        // SAFETY: a slot of the live record
        unsafe { *s as *mut c_void }
    }
}

/// `SeetWrite`: the text into the buffer `out` (capacity `cap`, not terminated) -> its whole length.
///
/// # Safety
/// `out` holds `cap` bytes (or `cap` is not positive).
unsafe fn write(out: usize, cap: i32, text: &[u8]) -> i32 {
    let n = text.len().min(cap.max(0) as usize);
    if n > 0 {
        // SAFETY: the contract
        unsafe { core::ptr::copy_nonoverlapping(text.as_ptr(), out as *mut u8, n) };
    }
    text.len() as i32
}

/// `SeetBytes`: the `n` bytes at `p` as the C++'s `UWideString(std::string(p, n).c_str())` passed them on.
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

/// The language string `id` (`gResource->GetString` into an empty string: empty when there is none).
fn string(id: i32) -> Vec<u8> {
    resource_string(id as c_ulong as i64).unwrap_or_default().to_vec()
}

/// Whether the array `a` holds combo boxes (`SeetCombo`'s arrays: the effect's own, the scenario and player ones).
fn is_combo(a: u32) -> bool {
    matches!(a, EE_SCREENS_SEET_A_COMBO | EE_SCREENS_SEET_A_ECOMBO | EE_SCREENS_SEET_A_SCEN | EE_SCREENS_SEET_A_PLAYER)
}

/// Whether the array `a` holds edit boxes (`SeetEdit`'s arrays; the condition record has no second edit boxes).
fn is_edit(condition: bool, a: u32) -> bool {
    match a {
        EE_SCREENS_SEET_A_EDIT | EE_SCREENS_SEET_A_TEXT => true,
        EE_SCREENS_SEET_A_EDIT2 => !condition,
        _ => false,
    }
}

/// The edit box's line 0 (`GetText(0)`).
pub(crate) fn edit_line(c: Cpp) -> Vec<u8> {
    let p = direct::edit_call(c, EeUiEditCall::Line, 0, 0).unwrap_or(0);
    // SAFETY: the edit box's line bytes (or NULL)
    unsafe { direct::c_bytes(p as *const c_void) }
}

/// `UIEditBox::SetText(text)`.
pub(crate) fn edit_set_text(c: Cpp, text: &[u8]) {
    let t = direct::c_text(text);
    direct::edit_call(c, EeUiEditCall::SetText, t.as_ptr() as isize, 0);
}

/// The combo box's text label's line 0 (`GetTextLabel()->GetText(0)`).
fn combo_text(c: Cpp) -> Vec<u8> {
    let label = direct::combo_call(c, EeUiComboCall::Part, 4, 0).unwrap_or(0) as Cpp;
    label_text(label)
}

/// `UIComboBox::AppendLine(text)`: dirty, the line appended to the list, an open list reopened.
pub(crate) fn combo_append(c: Cpp, text: &[u8]) {
    let t = direct::c_text(text);
    direct::control_virtual(c, EeUiOp::SetDirty, 1, 0, &mut [0.0; 4]);
    direct::list_op(direct::combo_list(c), EeUiOp::ListAppendText, t.as_ptr() as isize, 0, 0);
    direct::combo_call(c, EeUiComboCall::Reopen, 0, 0);
}

/// The list box's virtual `AppendLine(text)`.
pub(crate) fn list_append(c: Cpp, text: &[u8]) {
    let t = direct::c_text(text);
    direct::list_op(c, EeUiOp::ListAppendText, t.as_ptr() as isize, 0, 0);
}

/// `UIControl::GetWidth()`: `UMFloor(width + 1)` (round to nearest, then down when above).
fn width_of(c: Cpp) -> i32 {
    let w = direct::with_fields(c, |r| r.mRectangle.right - r.mRectangle.left).unwrap_or(0.0) + 1.0;
    let mut n = w.round_ties_even() as i32;
    if n as f32 > w {
        n -= 1;
    }
    n
}

/// The toolkit ops of the generic host answered in Rust: `p` and `i`, `f` as the host takes them (p0 the controls,
/// p1 the entry or a handle, p2 a buffer or an out word; i0 the array, i1 the index, i2 and i3 the arguments).
/// `condition`: the condition screen's record (`ee_screenscpp_sect`), else the effect screen's.
///
/// # Safety
/// `p` are the host's arguments for `op` (a live record of the kind, the buffers the op names).
pub unsafe fn seet(condition: bool, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> Option<i32> {
    let (a, k) = (i[0] as u32, i[1] as u32);
    // SAFETY: the contract (a live record of the kind)
    let at = || unsafe {
        let s = slot(p[0], condition, a, k);
        if s.is_null() {
            core::ptr::null_mut()
        } else {
            *s as Cpp
        }
    };
    Some(match op {
        EE_SCREENS_SEET_OP_CONTROLS_NEW => {
            // SAFETY: p2 is the out word
            unsafe { *(p[2] as *mut usize) = controls_new(condition) };
            0
        }
        EE_SCREENS_SEET_OP_CONTROLS_FREE => {
            // SAFETY: the record, freed once
            unsafe { controls_free(p[0], condition) };
            0
        }
        EE_SCREENS_SEET_OP_SLOT => {
            // SAFETY: the record's slot; p2 the control (set) or the out word (get)
            unsafe {
                let s = slot(p[0], condition, a, k);
                if i[2] != 0 {
                    if !s.is_null() {
                        *s = p[2];
                    }
                } else {
                    *(p[2] as *mut usize) = if s.is_null() { 0 } else { *s };
                }
            }
            0
        }
        EE_SCREENS_SEET_OP_DISPLAY => {
            direct::control_virtual(at(), EeUiOp::SetDisplayMode, i[2] as c_ulong as isize, 0, &mut [0.0; 4]);
            0
        }
        EE_SCREENS_SEET_OP_RECT => {
            let r = direct::with_fields(at(), |r| {
                [r.mRectangle.left, r.mRectangle.top, r.mRectangle.right, r.mRectangle.bottom]
            })
            .unwrap_or([0.0; 4]);
            *f = r;
            0
        }
        EE_SCREENS_SEET_OP_LOCATION => {
            let mut g = [f[0], f[1], 0.0, 0.0];
            direct::control_virtual(at(), EeUiOp::VirtSetLocation, 0, 0, &mut g);
            0
        }
        EE_SCREENS_SEET_OP_TEXT_STRING | EE_SCREENS_SEET_OP_TEXT => {
            let text = if op == EE_SCREENS_SEET_OP_TEXT_STRING {
                string(i[2])
            } else {
                // SAFETY: p2 holds i2 bytes
                unsafe { bytes(p[2], i[2]) }
            };
            if a == EE_SCREENS_SEET_A_LABEL {
                label_set_text(at(), &text);
            } else {
                edit_set_text(at(), &text);
            }
            0
        }
        EE_SCREENS_SEET_OP_NUMBER => {
            if a == EE_SCREENS_SEET_A_LABEL {
                label_set_long(at(), i[2] as c_long);
            } else {
                // (`SetText(unsigned long)`: `PlatIToA` of it as an `int`)
                edit_set_text(at(), i[2].to_string().as_bytes());
            }
            0
        }
        EE_SCREENS_SEET_OP_GET_TEXT => {
            let text = if a == EE_SCREENS_SEET_A_LABEL {
                label_text(at())
            } else if is_combo(a) {
                combo_text(at())
            } else {
                edit_line(at())
            };
            // SAFETY: p2 holds i3 bytes
            unsafe { write(p[2], i[3], &text) }
        }
        EE_SCREENS_SEET_OP_GET_ALL_TEXT => {
            let t = direct::edit_call(at(), EeUiEditCall::AllText, 0, 0).unwrap_or(0);
            // SAFETY: the edit box's text bytes (or NULL); p2 holds i3 bytes
            unsafe { write(p[2], i[3], &direct::c_bytes(t as *const c_void)) }
        }
        EE_SCREENS_SEET_OP_LIST_DATA if !condition => {
            let line = direct::list_line(at(), i[2] as c_ulong as isize).unwrap_or_default();
            // SAFETY: p2 holds i3 bytes
            unsafe { write(p[2], i[3], &line) }
        }
        EE_SCREENS_SEET_OP_TEXT_INT => {
            if a == EE_SCREENS_SEET_A_LABEL {
                atoi(&label_text(at()))
            } else {
                atoi(&edit_line(at()))
            }
        }
        EE_SCREENS_SEET_OP_WIDTH => {
            let w = i[2] as c_ulong as isize;
            if a == EE_SCREENS_SEET_A_CHECK {
                direct::button_call(at(), EeUiButtonCall::CheckChangeWidth, w, 0);
            } else if is_edit(condition, a) {
                direct::edit_call(at(), EeUiEditCall::ChangeWidth, w, 0);
            } else {
                direct::combo_call(at(), EeUiComboCall::ChangeWidth, w, 0);
            }
            0
        }
        EE_SCREENS_SEET_OP_GET_WIDTH => width_of(at()),
        EE_SCREENS_SEET_OP_CLEAR => {
            if a == EE_SCREENS_SEET_A_LIST {
                direct::list_op(at(), EeUiOp::ListDeleteAll, 0, 0, 0);
            } else {
                direct::combo_call(at(), EeUiComboCall::DeleteAllEntries, 0, 0);
            }
            0
        }
        EE_SCREENS_SEET_OP_APPEND_STRING | EE_SCREENS_SEET_OP_APPEND => {
            let text = if op == EE_SCREENS_SEET_OP_APPEND_STRING {
                string(i[2])
            } else {
                // SAFETY: p2 holds i2 bytes
                unsafe { bytes(p[2], i[2]) }
            };
            if a == EE_SCREENS_SEET_A_LIST {
                list_append(at(), &text);
            } else {
                combo_append(at(), &text);
            }
            0
        }
        EE_SCREENS_SEET_OP_SELECTION => direct::combo_call(at(), EeUiComboCall::Selection, 0, 0).unwrap_or(-1) as i32,
        EE_SCREENS_SEET_OP_HANDLE_SELECTION => {
            direct::combo_call(p[1] as Cpp, EeUiComboCall::Selection, 0, 0).unwrap_or(-1) as i32
        }
        EE_SCREENS_SEET_OP_SET_SELECTION => {
            direct::combo_call(at(), EeUiComboCall::SetCurrentSelection, i[2] as u32 as c_ulong as isize, 0);
            0
        }
        EE_SCREENS_SEET_OP_LINE_COUNT => {
            direct::list_op(direct::combo_list(at()), EeUiOp::ListLineCount, 0, 0, 0).map_or(0, |(n, _)| n as i32)
        }
        EE_SCREENS_SEET_OP_EVENT_HOLD => {
            let c = at();
            let trigger = i[2] as c_ulong as isize;
            let event = direct::control_call(c, EeUiControlCall::TriggerEvent, trigger, 0);
            if event != 0 {
                direct::control_virtual(c, EeUiOp::FormEventCall, event, 2, &mut [0.0; 4]);
                direct::control_call(c, EeUiControlCall::SetTrigger, trigger, 0);
            }
            // SAFETY: p2 is the out word
            unsafe { *(p[2] as *mut usize) = event as usize };
            0
        }
        EE_SCREENS_SEET_OP_EVENT_RESTORE => {
            let c = at();
            direct::control_call(c, EeUiControlCall::SetTrigger, i[2] as c_ulong as isize, p[1] as isize);
            direct::control_virtual(c, EeUiOp::FormEventCall, p[1] as isize, 1, &mut [0.0; 4]);
            0
        }
        EE_SCREENS_SEET_OP_EVENT_NULL => {
            direct::control_call(at(), EeUiControlCall::SetTrigger, i[2] as c_ulong as isize, 0);
            0
        }
        EE_SCREENS_SEET_OP_EVENT_SET => {
            direct::control_call(at(), EeUiControlCall::SetTrigger, i[2] as c_ulong as isize, p[1] as isize);
            0
        }
        EE_SCREENS_SEET_OP_FIRE_RANGE => {
            // (`kControlTriggerRangeChanged`; SAFETY: the record, its sliders' array)
            let s = unsafe { ee_screens_seet_slot(p[0] as *mut c_void, condition, EE_SCREENS_SEET_A_SLIDER, k) };
            direct::control_call(s, EeUiControlCall::FireTrigger, 0x3e8, 0);
            0
        }
        EE_SCREENS_SEET_OP_CHECKED => {
            // (a radio button, else the check box of the index whatever the array)
            let (call, array) = if a == EE_SCREENS_SEET_A_RADIO {
                (EeUiButtonCall::RadioIsChecked, EE_SCREENS_SEET_A_RADIO)
            } else {
                (EeUiButtonCall::CheckIsChecked, EE_SCREENS_SEET_A_CHECK)
            };
            // SAFETY: the record
            let c = unsafe { ee_screens_seet_slot(p[0] as *mut c_void, condition, array, k) };
            (direct::button_call(c, call, 0, 0).unwrap_or(0) != 0) as i32
        }
        EE_SCREENS_SEET_OP_SET_CHECKED => {
            // SAFETY: the record (the check boxes' array)
            let c = unsafe { ee_screens_seet_slot(p[0] as *mut c_void, condition, EE_SCREENS_SEET_A_CHECK, k) };
            direct::button_call(c, EeUiButtonCall::CheckSetChecked, (i[2] != 0) as isize, 0);
            0
        }
        EE_SCREENS_SEET_OP_SLIDER
        | EE_SCREENS_SEET_OP_SET_SLIDER
        | EE_SCREENS_SEET_OP_RANGE
        | EE_SCREENS_SEET_OP_PAGE_SIZE => {
            // SAFETY: the record (the sliders' array; the condition's one slider whatever the index)
            let s = unsafe { ee_screens_seet_slot(p[0] as *mut c_void, condition, EE_SCREENS_SEET_A_SLIDER, k) };
            match op {
                EE_SCREENS_SEET_OP_SLIDER => {
                    direct::slider_call(s, EeUiSliderCall::CurrentRangeValue, 0, 0).unwrap_or(0) as c_long as i32
                }
                EE_SCREENS_SEET_OP_SET_SLIDER => {
                    direct::slider_call(s, EeUiSliderCall::SetCurrentRangeValue, i[2] as c_long as isize, 0);
                    0
                }
                EE_SCREENS_SEET_OP_RANGE => {
                    let (lo, hi) = (i[2] as c_ulong as isize, i[3] as c_ulong as isize);
                    direct::slider_call(s, EeUiSliderCall::SetRange, lo, hi);
                    0
                }
                _ => {
                    direct::slider_call(s, EeUiSliderCall::SetPageSize, i[2] as c_ulong as isize, 0);
                    0
                }
            }
        }
        EE_SCREENS_SEET_OP_STRING_TABLE => {
            direct::with_fields(at(), |r| r.mStringTableID = i[2] as c_ulong);
            0
        }
        EE_SCREENS_SEET_OP_STRING => {
            // SAFETY: p2 holds i1 bytes
            unsafe { write(p[2], i[1], &string(i[2])) }
        }
        _ => return None,
    })
}

// ---------------------------------------------------------------------------------------- the editor's events

/// The editor events' host ops on the toolkit answered in Rust (`ee_screenscpp_se`, `Scenario Editor/EESEEvents.cpp`):
/// `form` is the editor's form, `entity` the event's control. `None`: the C++ host's.
pub fn se(form: Cpp, entity: Cpp, op: u32, a: u32, b: u32) -> Option<u32> {
    use crate::screens::scenario_editor::*;
    let by_id =
        |id: u32| direct::form_call(form, ee_ui::formobj::EeUiFormCall::ControlById, id as c_ulong as isize, 0) as Cpp;
    Some(match op {
        EE_SCREENS_SE_OP_ENTITY_ID => direct::with_fields(entity, |r| r.mId).unwrap_or(0) as u32,
        EE_SCREENS_SE_OP_LIST_SELECTION => match direct::list_op(entity, EeUiOp::ListSelection, 0, 0, 0) {
            Some((0, f)) => f[0].to_bits(),
            _ => 0xffff_ffff,
        },
        EE_SCREENS_SE_OP_COMBO_SELECTION => {
            direct::combo_call(entity, EeUiComboCall::Selection, 0, 0).unwrap_or(-1) as u32
        }
        EE_SCREENS_SE_OP_CHECKED => {
            (direct::button_call(entity, EeUiButtonCall::CheckIsChecked, 0, 0).unwrap_or(0) != 0) as u32
        }
        EE_SCREENS_SE_OP_SHOW => {
            let c = by_id(a);
            if !c.is_null() {
                direct::control_virtual(c, EeUiOp::SetDisplayMode, b as c_ulong as isize, 0, &mut [0.0; 4]);
            }
            0
        }
        EE_SCREENS_SE_OP_RANGE => {
            direct::slider_call(by_id(a), EeUiSliderCall::CurrentRangeValue, 0, 0).unwrap_or(0) as c_long as u32
        }
        EE_SCREENS_SE_OP_LABEL_NUMBER => {
            label_set_long(by_id(a), b as i32 as c_long);
            0
        }
        EE_SCREENS_SE_OP_EDIT_LINES => direct::edit_call(entity, EeUiEditCall::LineCount, 0, 0).unwrap_or(0) as u32,
        _ => return None,
    })
}

/// `ee_screenscpp_se_text`'s text: the edit box `entity`'s line `arg` (`EE_SCREENS_SE_TEXT_EDIT_LINE`), else the
/// language string `arg` (empty when there is none).
pub fn se_text(entity: Cpp, which: u32, arg: u32) -> Vec<u8> {
    if which == crate::screens::scenario_editor::EE_SCREENS_SE_TEXT_EDIT_LINE {
        let p = direct::edit_call(entity, EeUiEditCall::Line, arg as c_ulong as isize, 0).unwrap_or(0);
        // SAFETY: the edit box's line bytes (or NULL)
        return unsafe { direct::c_bytes(p as *const c_void) };
    }
    string(arg as i32)
}

/// `ee_screenscpp_se_set_text`: the edit box `entity`'s text set to the bytes (as `UWideString(std::string(...))`).
pub fn se_set_text(entity: Cpp, text: &[u8]) {
    edit_set_text(entity, text);
}

// ---------------------------------------------------------------------------------------- the shared maker's host

/// `SeplText`: a control's text by kind (`EE_SCREENS_SEPL_TEXT_*`): none, the string `string`, the number, "%s %ld",
/// "%ld %s", or `max(1, number >> 8)` copies of the byte `number & 0xff` (cut at a NUL).
pub fn sepl_text(kind: u32, string_id: u32, number: c_long) -> Vec<u8> {
    use crate::screens::editor_players::*;
    let s = || string(string_id as i32);
    match kind {
        EE_SCREENS_SEPL_TEXT_STRING => s(),
        EE_SCREENS_SEPL_TEXT_NUMBER => number.to_string().into_bytes(),
        EE_SCREENS_SEPL_TEXT_STRING_NUMBER => [s(), b" ".to_vec(), number.to_string().into_bytes()].concat(),
        EE_SCREENS_SEPL_TEXT_NUMBER_STRING => [number.to_string().into_bytes(), b" ".to_vec(), s()].concat(),
        EE_SCREENS_SEPL_TEXT_CHAR => {
            let byte = (number & 0xff) as u8;
            if byte == 0 {
                Vec::new()
            } else {
                vec![byte; (number >> 8).max(1) as usize]
            }
        }
        _ => Vec::new(),
    }
}

/// The shared maker's host ops on the toolkit answered in Rust (`ee_screenscpp_sepl`,
/// `Scenario Editor/EESERBUnitTypes.cpp`): p0 the control, p1 a second handle, i and f as the host's. `None`: the
/// C++ host's.
///
/// # Safety
/// `p` are the host's arguments for `op` (live controls, the buffers the op names).
pub unsafe fn sepl_direct(op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> Option<i32> {
    use crate::screens::editor_map::{EE_SCREENS_SEPL_OP_EDIT_BYTES, EE_SCREENS_SEPL_OP_HELP};
    use crate::screens::editor_players::*;
    let c = p[0] as Cpp;
    Some(match op {
        EE_SCREENS_SEPL_OP_RECT => {
            *f = direct::with_fields(c, |r| {
                [r.mRectangle.left, r.mRectangle.top, r.mRectangle.right, r.mRectangle.bottom]
            })
            .unwrap_or([0.0; 4]);
            0
        }
        EE_SCREENS_SEPL_OP_SET_DISPLAY => {
            direct::control_virtual(c, EeUiOp::SetDisplayMode, i[0] as c_ulong as isize, 0, &mut [0.0; 4]);
            0
        }
        EE_SCREENS_SEPL_OP_DISPLAY => direct::with_fields(c, |r| r.mDisplayMode).unwrap_or(0) as i32,
        EE_SCREENS_SEPL_OP_SET_CHECKED => {
            direct::button_call(c, EeUiButtonCall::CheckSetChecked, (i[0] != 0) as isize, 0);
            0
        }
        EE_SCREENS_SEPL_OP_CHECKED => {
            (direct::button_call(c, EeUiButtonCall::CheckIsChecked, 0, 0).unwrap_or(0) != 0) as i32
        }
        EE_SCREENS_SEPL_OP_RADIO_CHECKED | EE_SCREENS_SEPL_OP_RADIO_IS_CHECKED => {
            (direct::button_call(c, EeUiButtonCall::RadioIsChecked, 0, 0).unwrap_or(0) != 0) as i32
        }
        EE_SCREENS_SEPL_OP_COMBO_CLEAR => {
            direct::combo_call(c, EeUiComboCall::DeleteAllEntries, 0, 0);
            0
        }
        EE_SCREENS_SEPL_OP_COMBO_APPEND => {
            combo_append(c, &sepl_text(i[0] as u32, i[1] as u32, i[2] as c_long));
            0
        }
        EE_SCREENS_SEPL_OP_COMBO_SELECTION => {
            direct::combo_call(c, EeUiComboCall::Selection, 0, 0).unwrap_or(-1) as i32
        }
        EE_SCREENS_SEPL_OP_LIST_SELECTION => direct::list_op(direct::combo_list(c), EeUiOp::ListSelection, 0, 0, 0)
            .map_or(0, |(_, f)| f[0].to_bits() as i32),
        EE_SCREENS_SEPL_OP_SET_SELECTION => {
            direct::combo_call(c, EeUiComboCall::SetCurrentSelection, i[0] as c_ulong as isize, 0);
            0
        }
        EE_SCREENS_SEPL_OP_EDIT_NUMBER => {
            edit_set_text(c, (i[0] as c_ulong).to_string().as_bytes());
            0
        }
        EE_SCREENS_SEPL_OP_EDIT_COPY => {
            edit_set_text(c, &edit_line(p[1] as Cpp));
            0
        }
        EE_SCREENS_SEPL_OP_EDIT_INT | EE_SCREENS_SEPL_OP_EDIT_ATOI => atoi(&edit_line(c)),
        EE_SCREENS_SEPL_OP_SLIDER => {
            direct::slider_call(c, EeUiSliderCall::CurrentRangeValue, 0, 0).unwrap_or(0) as c_long as i32
        }
        EE_SCREENS_SEPL_OP_SET_SLIDER => {
            direct::slider_call(c, EeUiSliderCall::SetCurrentRangeValue, i[0] as c_long as isize, 0);
            0
        }
        EE_SCREENS_SEPL_OP_LABEL_NUMBER => {
            label_set_long(c, i[0] as c_long);
            0
        }
        EE_SCREENS_SEPL_OP_GROUP => {
            group_enable(c, i[0], i[1] != 0);
            0
        }
        EE_SCREENS_SEPL_OP_SET_LOCATION => {
            let mut g = [f[0], f[1], 0.0, 0.0];
            direct::control_virtual(c, EeUiOp::VirtSetLocation, 0, 0, &mut g);
            0
        }
        EE_SCREENS_SEPL_OP_TRI_STATE => direct::button_call(c, EeUiButtonCall::TriCheckState, 0, 0).unwrap_or(0) as i32,
        EE_SCREENS_SEPL_OP_SET_TRI_STATE => {
            direct::button_call(c, EeUiButtonCall::TriSetCheckState, i[0] as c_ulong as isize, 0);
            0
        }
        EE_SCREENS_SEPL_OP_LIST_CLEAR => {
            direct::list_op(c, EeUiOp::ListDeleteAll, 0, 0, 0);
            0
        }
        EE_SCREENS_SEPL_OP_LINE_COUNT => {
            direct::list_op(direct::combo_list(c), EeUiOp::ListLineCount, 0, 0, 0).map_or(0, |(n, _)| n as i32)
        }
        EE_SCREENS_SEPL_OP_LABEL_TEXT => {
            label_set_text(c, &sepl_text(i[0] as u32, i[1] as u32, i[2] as c_long));
            0
        }
        EE_SCREENS_SEPL_OP_HELP => {
            direct::with_fields(c, |r| r.mStringTableID = i[0] as c_ulong);
            0
        }
        EE_SCREENS_SEPL_OP_EDIT_BYTES => {
            // SAFETY: p1 holds i0 bytes
            edit_set_text(c, &unsafe { bytes(p[1], i[0]) });
            0
        }
        _ => return None,
    })
}

/// The shared maker's host: an op answered in Rust ([`sepl_direct`]), else the C++'s `ee_screenscpp_sepl`.
///
/// # Safety
/// As `ee_screenscpp_sepl`: `p` points at the three handles of the op.
#[cfg(not(test))]
pub(crate) unsafe fn sepl(op: u32, p: *const *mut c_void, i: &mut [i32; 4], f: &mut [f32; 4]) -> i32 {
    /// cbindgen:ignore
    mod sys {
        use core::ffi::c_void;

        extern "C" {
            pub fn ee_screenscpp_sepl(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
            pub fn ee_screenscpp_sepx(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
        }
    }
    // SAFETY: the contract (three handles)
    let q = unsafe { [*p as usize, *p.add(1) as usize, *p.add(2) as usize] };
    // SAFETY: the contract
    if let Some(r) = unsafe { sepl_direct(op, q, i, f) } {
        return r;
    }
    // the statement sequences composed in Rust (editor_sepl.rs) over the C++'s single statements
    let mut x = |op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]| -> i32 {
        use crate::screens::editor_players::EE_SCREENS_SEPL_OP_COMBO_SELECTION;
        if op == crate::screens::editor_sepl::EE_SCREENS_SEPL_OP_COMBO_SELECTION_RAW {
            // SAFETY: p0 the op's live combo box
            return unsafe { sepl_direct(EE_SCREENS_SEPL_OP_COMBO_SELECTION, p, i, f) }.unwrap_or(-1);
        }
        let p = p.map(|v| v as *mut c_void);
        // SAFETY: the statements take the composed op's live handles
        unsafe { sys::ee_screenscpp_sepx(op, p.as_ptr(), i.as_mut_ptr(), f.as_mut_ptr()) }
    };
    if let Some(r) = crate::screens::editor_sepl::compose(&mut x, op, q, i, f) {
        return r;
    }
    // SAFETY: the contract
    unsafe { sys::ee_screenscpp_sepl(op, p, i.as_mut_ptr(), f.as_mut_ptr()) }
}

// ---------------------------------------------------------------------------------------- the object screen's host

/// The combo box's drop-down line `k` (`GetListBox()->GetData(k)`), empty past its lines.
fn combo_line(c: Cpp, k: isize) -> Vec<u8> {
    direct::list_line(direct::combo_list(c), k).unwrap_or_default()
}

/// The combo box's drop-down line count.
fn combo_lines(c: Cpp) -> usize {
    direct::list_op(direct::combo_list(c), EeUiOp::ListLineCount, 0, 0, 0).map_or(0, |(n, _)| n as usize)
}

/// An action's name without its NUL (`EEUnitAction::GetActionName`; empty for an unknown id).
fn action_name(id: i32) -> &'static [u8] {
    let n = ee_sim::units::script::action_name(id as c_ulong as u32);
    &n[..n.len().saturating_sub(1)]
}

/// The object screen's host ops on the toolkit and the action names answered in Rust (`ee_screenscpp_seob`,
/// `Scenario Editor/EESETSObject.cpp`). `None`: the C++ host's.
///
/// # Safety
/// `p` are the host's arguments for `op` (live controls, the buffers the op names).
pub unsafe fn seob(op: u32, p: [usize; 3], i: &mut [i32; 4]) -> Option<i32> {
    use crate::screens::editor_object::*;
    let combo = p[0] as Cpp;
    Some(match op {
        EE_SCREENS_SEOB_OP_ACTION_APPEND => {
            let name = action_name(i[0]);
            if !name.is_empty() {
                combo_append(combo, name);
            }
            0
        }
        EE_SCREENS_SEOB_OP_ACTION_ID => {
            // (no selected line: the empty name, as the C++ host's)
            let line = direct::combo_call(combo, EeUiComboCall::Selection, 0, 0).unwrap_or(-1) as c_ulong as usize;
            let name = if line < combo_lines(combo) { combo_line(combo, line as isize) } else { Vec::new() };
            ee_sim::units::script::action_id(&name)
        }
        EE_SCREENS_SEOB_OP_ACTION_INDEX => {
            let name = action_name(i[0]);
            let n = combo_lines(combo);
            (0..n).find(|&k| !name.is_empty() && combo_line(combo, k as isize) == name).unwrap_or(n) as i32
        }
        EE_SCREENS_SEOB_OP_CLASS_NAMES => {
            // "any class", then the classes p1 holds (i0 of them, the first the "any" slot) by their names
            direct::combo_call(combo, EeUiComboCall::DeleteAllEntries, 0, 0);
            combo_append(combo, &string(EE_SCREENS_SEOB_IDS_ANY_CLASS as i32));
            let types = p[1] as *const u32;
            for k in 1..i[0].max(0) as usize {
                // SAFETY: p1 holds i0 class numbers
                let class = unsafe { *types.add(k) } as usize;
                // SAFETY: the editor's reads hold the world (editor_world.rs's note)
                let t = unsafe { ee_sim::world::WorldRead::begin() };
                let name = ee_sim::world::ClassView::by_index(&t, class)
                    .map_or(0, |c| ee_sim::world_field!(c.fields(), mNameStringID));
                drop(t);
                combo_append(combo, &string(name as i32));
            }
            0
        }
        EE_SCREENS_SEOB_OP_TEXT if i[0] == EE_SCREENS_SEOB_TEXT_STRING || i[0] == EE_SCREENS_SEOB_TEXT_LINE => {
            let text = if i[0] == EE_SCREENS_SEOB_TEXT_STRING {
                string(i[1])
            } else if (i[1] as c_ulong as usize) < combo_lines(p[1] as Cpp) {
                // (a line past the list: the original read past it; none here)
                combo_line(p[1] as Cpp, i[1] as c_ulong as isize)
            } else {
                Vec::new()
            };
            // SAFETY: p0 holds i2 bytes
            unsafe { write(p[0], i[2], &text) }
        }
        _ => return None,
    })
}

// ------------------------------------------------------------------------- the trigger tab's and trigger screen's

/// The trigger tab's host ops on its list and groups answered in Rust (`ee_screenscpp_sets`,
/// `Scenario Editor/EESETriggerScreen.cpp`; p0 the list or a control), and the trigger screen's event slot
/// (`ee_screenscpp_setr`, `EESETSTrigger.cpp`). `None`: the C++ host's.
pub fn sets(op: u32, p: [usize; 3], i: &[i32; 4]) -> Option<i32> {
    use crate::screens::editor_trigger::EE_SCREENS_SETR_OP_SET_EVENT;
    use crate::screens::editor_tscreen::*;
    let c = p[0] as Cpp;
    Some(match op {
        EE_SCREENS_SETS_OP_LIST_SELECTION => match direct::list_op(c, EeUiOp::ListSelection, 0, 0, 0) {
            Some((0, f)) => f[0].to_bits() as i32,
            _ => -1,
        },
        EE_SCREENS_SETS_OP_LIST_COUNT => {
            direct::list_op(c, EeUiOp::ListLineCount, 0, 0, 0).map_or(0, |(n, _)| n as i32)
        }
        EE_SCREENS_SETS_OP_LIST_SELECT => {
            let (line, b, d) = (i[0] as c_ulong as isize, (i[1] != 0) as isize, (i[2] != 0) as isize);
            direct::list_op(c, EeUiOp::ListChangeSelection, line, b, d);
            0
        }
        EE_SCREENS_SETS_OP_GROUP => {
            group_enable(c, i[0], i[1] != 0);
            0
        }
        EE_SCREENS_SETS_OP_CARET_END => {
            // the caret after the edit box's last character (`UWideString::GetLength`: `PlatMbsLen`), line 0
            let all = direct::edit_call(c, EeUiEditCall::AllText, 0, 0).unwrap_or(0);
            // SAFETY: the edit box's text bytes (or NULL)
            let text = direct::c_text(&unsafe { direct::c_bytes(all as *const c_void) });
            // SAFETY: a NUL-terminated copy
            let n = unsafe { ee_base::plat_core::string::PlatMbsLen(text.as_ptr()) };
            direct::edit_call(c, EeUiEditCall::SetCaretLocation, n as c_ulong as isize, 0);
            0
        }
        EE_SCREENS_SETR_OP_SET_EVENT => {
            // (`kControlTriggerSelectionChanged`)
            direct::control_call(c, EeUiControlCall::SetTrigger, 0x3eb, p[1] as isize);
            0
        }
        _ => return None,
    })
}

/// `inControl->GetForm()->GetEntityGroup(group)->Enable()` / `Disable()`.
fn group_enable(c: Cpp, group: i32, on: bool) {
    let form = direct::with_fields(c, |r| r.mForm).unwrap_or(core::ptr::null_mut()) as Cpp;
    let g = direct::form_call(form, ee_ui::formobj::EeUiFormCall::EntityGroup, group as c_ulong as isize, 0);
    let call =
        if on { ee_ui::groupobj::EeUiEntityGroupCall::Enable } else { ee_ui::groupobj::EeUiEntityGroupCall::Disable };
    direct::group_call(g as Cpp, call, 0);
}

// ---------------------------------------------------------------------------------------------- the map tab's host

/// The map tab's host ops on the toolkit answered in Rust (`ee_screenscpp_semp`, `Scenario Editor/EEUIFSEMap.cpp`;
/// the terrain tab routes its own there too). `None`: the C++ host's.
///
/// # Safety
/// `p` are the host's arguments for `op` (live controls, the out words the op names).
pub unsafe fn semp_direct(op: u32, p: [usize; 3], i: &[i32; 4], f: &mut [f32; 4]) -> Option<i32> {
    use crate::screens::editor_map::*;
    let c = p[0] as Cpp;
    Some(match op {
        EE_SCREENS_SEMP_OP_LABEL_RECT => {
            // (a radio button's text label (i0), else the combo box's)
            let label = if i[0] != 0 {
                direct::button_call(c, EeUiButtonCall::TextLabel, 0, 0).unwrap_or(0)
            } else {
                direct::combo_call(c, EeUiComboCall::Part, 4, 0).unwrap_or(0)
            } as Cpp;
            *f = direct::with_fields(label, |r| {
                [r.mRectangle.left, r.mRectangle.top, r.mRectangle.right, r.mRectangle.bottom]
            })
            .unwrap_or([0.0; 4]);
            0
        }
        EE_SCREENS_SEMP_OP_COMBO_ATOI => atoi(&combo_text(c)),
        EE_SCREENS_SEMP_OP_CONTROL => {
            let control = direct::form_call(c, ee_ui::formobj::EeUiFormCall::ControlById, i[0] as c_ulong as isize, 0);
            // SAFETY: p1 is the out word
            unsafe { *(p[1] as *mut usize) = control as usize };
            0
        }
        _ => return None,
    })
}

/// The map tab's host: an op answered in Rust ([`semp_direct`]), else the C++'s `ee_screenscpp_semp`.
///
/// # Safety
/// As `ee_screenscpp_semp`: `p` points at the three handles of the op, `i` and `f` at four values each.
#[cfg(not(test))]
pub(crate) unsafe fn semp(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32 {
    /// cbindgen:ignore
    mod sys {
        use core::ffi::c_void;

        extern "C" {
            pub fn ee_screenscpp_semp(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
        }
    }
    // SAFETY: the contract
    unsafe {
        let q = [*p as usize, *p.add(1) as usize, *p.add(2) as usize];
        if let Some(r) = semp_direct(op, q, &*(i as *const [i32; 4]), &mut *(f as *mut [f32; 4])) {
            return r;
        }
        sys::ee_screenscpp_semp(op, p, i, f)
    }
}

// ------------------------------------------------------------------------------------------- the unit tab's host

/// The unit tab's host ops on its lists and events answered in Rust (`ee_screenscpp_seun`,
/// `Scenario Editor/EESEUnit.cpp`; p0 the list or the control). `None`: the C++ host's.
pub fn seun(op: u32, p: [usize; 3], i: &[i32; 4]) -> Option<i32> {
    use crate::screens::editor_unit::*;
    let c = p[0] as Cpp;
    Some(match op {
        EE_SCREENS_SEUN_OP_SET_EVENT => {
            direct::control_call(c, EeUiControlCall::SetTrigger, i[0] as c_ulong as isize, p[1] as isize);
            0
        }
        EE_SCREENS_SEUN_OP_LIST_SELECTION => match direct::list_op(c, EeUiOp::ListSelection, 0, 0, 0) {
            Some((0, f)) => f[0].to_bits() as i32,
            _ => -1,
        },
        EE_SCREENS_SEUN_OP_LIST_COUNT => {
            direct::list_op(c, EeUiOp::ListLineCount, 0, 0, 0).map_or(0, |(n, _)| n as i32)
        }
        EE_SCREENS_SEUN_OP_LIST_SELECT => {
            direct::list_op(c, EeUiOp::ListChangeSelection, i[0] as c_ulong as isize, 1, 0);
            0
        }
        _ => return None,
    })
}

// --------------------------------------------------------------------------------------- the editor events' host

/// The editor events' host ops on the toolkit answered in Rust (`ee_screenscpp_see`,
/// `Scenario Editor/EEUIFSETRBPaintType.cpp`; p0 the control or the form). `None`: the C++ host's.
pub fn see(op: u32, p: [usize; 3], i: &[i32; 4]) -> Option<i32> {
    use crate::screens::editor_events::*;
    let c = p[0] as Cpp;
    Some(match op {
        EE_SCREENS_SEE_OP_CHECKED => {
            (direct::button_call(c, EeUiButtonCall::CheckIsChecked, 0, 0).unwrap_or(0) != 0) as i32
        }
        EE_SCREENS_SEE_OP_RANGE => {
            direct::slider_call(c, EeUiSliderCall::CurrentRangeValue, 0, 0).unwrap_or(0) as c_long as i32
        }
        EE_SCREENS_SEE_OP_SET_DISPLAY => {
            direct::control_virtual(c, EeUiOp::SetDisplayMode, i[0] as c_ulong as isize, 0, &mut [0.0; 4]);
            0
        }
        EE_SCREENS_SEE_OP_SET_SELECTION => {
            direct::combo_call(c, EeUiComboCall::SetCurrentSelection, i[0] as c_ulong as isize, 0);
            0
        }
        EE_SCREENS_SEE_OP_ENTITY_ID => direct::with_fields(c, |r| r.mId).unwrap_or(0) as i32,
        EE_SCREENS_SEE_OP_LIST_SELECTION => match direct::list_op(c, EeUiOp::ListSelection, 0, 0, 0) {
            Some((0, f)) => f[0].to_bits() as i32,
            _ => -1,
        },
        EE_SCREENS_SEE_OP_COMBO_SELECTION => direct::combo_call(c, EeUiComboCall::Selection, 0, 0).unwrap_or(-1) as i32,
        EE_SCREENS_SEE_OP_LINE_COUNT => {
            direct::list_op(direct::combo_list(c), EeUiOp::ListLineCount, 0, 0, 0).map_or(0, |(n, _)| n as i32)
        }
        EE_SCREENS_SEE_OP_LABEL_NUMBER => {
            // (`SetText(long)` of the `unsigned long`)
            let label = direct::form_call(c, ee_ui::formobj::EeUiFormCall::ControlById, i[0] as c_ulong as isize, 0);
            label_set_long(label as Cpp, i[1] as u32 as c_ulong as c_long);
            0
        }
        _ => return None,
    })
}

// ------------------------------------------------------------------------------------------ the editor form's host

/// The editor form's host ops on the toolkit answered in Rust (`ee_screenscpp_sefm`,
/// `Scenario Editor/EEUIFScenarioEditor.cpp`; p0 a control, p1 the form or a text). `None`: the C++ host's.
///
/// # Safety
/// `p` are the host's arguments for `op` (the live form and controls, a live `UWideString` the op names).
pub unsafe fn sefm(op: u32, p: [usize; 3], i: &[i32; 4]) -> Option<i32> {
    use crate::screens::editor_form::*;
    use ee_ui::formobj::EeUiFormCall;
    let form = p[1] as Cpp;
    Some(match op {
        EE_SCREENS_SEFM_OP_GROUP_ENABLE => {
            let g = direct::form_call(form, EeUiFormCall::EntityGroup, i[0] as c_ulong as isize, 0);
            let call = if i[1] != 0 {
                ee_ui::groupobj::EeUiEntityGroupCall::Enable
            } else {
                ee_ui::groupobj::EeUiEntityGroupCall::Disable
            };
            direct::group_call(g as Cpp, call, 0);
            0
        }
        EE_SCREENS_SEFM_OP_FIRE => {
            // (`kControlTriggerButtonUp` of the form's control i0, when there is one)
            let c = direct::form_call(form, EeUiFormCall::ControlById, i[0] as c_ulong as isize, 0) as Cpp;
            if !c.is_null() {
                direct::control_call(c, EeUiControlCall::FireTrigger, 0x3f0, 0);
            }
            0
        }
        EE_SCREENS_SEFM_OP_CAPTURED => {
            let call = if i[0] != 0 { EeUiFormCall::KeyboardCaptured } else { EeUiFormCall::MouseCaptured };
            (direct::form_call(form, call, 0, 0) != 0) as i32
        }
        EE_SCREENS_SEFM_OP_HELP_LABEL => {
            direct::form_call(form, EeUiFormCall::SetHelpTextLabel, p[0] as isize, 0);
            0
        }
        EE_SCREENS_SEFM_OP_COMBO_SELECT => {
            direct::combo_call(p[0] as Cpp, EeUiComboCall::SetCurrentSelection, i[0] as c_ulong as isize, 0);
            0
        }
        EE_SCREENS_SEFM_OP_LABEL_TEXT => {
            // SAFETY: p1 is a live UWideString (the contract)
            let text =
                unsafe { crate::screens::hud_direct::wide_bytes(p[1] as *const crate::screens::hud_direct::CppWide) };
            label_set_text(p[0] as Cpp, &text);
            0
        }
        _ => return None,
    })
}

#[cfg(test)]
#[path = "editor_direct_tests.rs"]
mod tests;
