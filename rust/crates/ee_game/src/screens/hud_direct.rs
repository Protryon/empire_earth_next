//! The HUD hosts' calls on the toolkit, the strings and the low-level engine's objects answered in Rust (the UI
//! cluster lane, round 1) instead of their C++ cases: the main game form's (`EEUIFMainGame::HostOp`), the HUD
//! controls' base ops (`EEUIRustControl::HostOp`) and the cursor's reads (`ee_screenscpp_cursor`). Every one of
//! these C++ cases was one call on a C++ shell over a Rust object (a `UIForm`, a control, a `UIManager`, a
//! `UWideString`, the language strings): the answers here make the same inward call the shell made.
//!
//! As in `hud_views.rs`, everything is below the hosts' traits (in the FFI adapters): the replay goldens are
//! unchanged by construction. An op that is not here goes on to the C++ host (`None`). Where the C++ ran on a NULL
//! pointer (a missing control) the answers are 0.

// (the casts of `long`s are no-ops on Windows and conversions on LP64)
#![allow(clippy::unnecessary_cast)]
#![cfg_attr(test, allow(dead_code))]

use core::ffi::{c_char, c_long, c_ulong, c_void};

use ee_base::plat_core::string::{PlatMbsCmp, PlatMbsInc, PlatMbsNBCmp, PlatMbsNInc};
use ee_base::ustring::{ee_uwstring_assign_text, EeUWideString};
use ee_ui::bgobj::EeUiBgCall;
use ee_ui::direct::{self, Cpp};
use ee_ui::formobj::EeUiFormCall;
use ee_ui::groupobj::EeUiEntityGroupCall;
use ee_ui::labelobj::EeUiLabelCall;
use ee_ui::managerobj::EeUiManagerCall;
use ee_ui::obj::EeUiOp;

use crate::screens::main_form::*;

// ------------------------------------------------------------------------------------------------ the strings

/// A C++ `UWideString` in place: its vtable pointer, then the Rust record it holds by value (lle/UWideString.h;
/// GEBitmap.cpp asserts `sizeof(UWideString) == sizeof(void*) + sizeof(EeUWideString)`).
#[repr(C)]
pub(crate) struct CppWide {
    vtable: *const c_void,
    pub(crate) s: EeUWideString,
}

/// The bytes of the string at `w` (its characters' bytes, no NUL; none for NULL).
///
/// # Safety
/// `w` is NULL or a live `UWideString`.
pub(crate) unsafe fn wide_bytes(w: *const CppWide) -> Vec<u8> {
    if w.is_null() {
        return Vec::new();
    }
    // SAFETY: the contract: the buffer holds a NUL-terminated string
    unsafe { direct::c_bytes((*w).s.string as *const c_void) }
}

/// `*w = text` (`UWideString::operator=(const char*)`, the text cut at its first NUL).
///
/// # Safety
/// `w` is a live `UWideString`.
pub(crate) unsafe fn wide_assign(w: *mut CppWide, text: &[u8]) {
    let t = direct::c_text(text);
    // SAFETY: the contract; the text is NUL-terminated
    unsafe { ee_uwstring_assign_text(&mut (*w).s, t.as_ptr() as *const c_char) }
}

/// `UWCompare(a, b) != 0` (`PlatMbsCmp` of the two strings).
pub(crate) fn differs(a: &[u8], b: &[u8]) -> bool {
    let (a, b) = (direct::c_text(a), direct::c_text(b));
    // SAFETY: two NUL-terminated strings
    unsafe { PlatMbsCmp(a.as_ptr(), b.as_ptr()) != 0 }
}

/// `UResourceFile::GetString(id, out)`'s text: the language string `(uint16_t)id`, cut to 4,499 bytes; `None`
/// when the table has none (the C++'s error path, which records a native error and leaves `out` alone).
pub(crate) fn resource_string(id: i64) -> Option<&'static [u8]> {
    let s = ee_base::util::strings::resource_string(id as u16)?.as_bytes();
    Some(&s[..s.len().min(4499)])
}

/// C's `atoi` on the bytes: leading white space, a sign, the digits (wrapping as the 32-bit `int` the CRT's
/// accumulates in).
pub(crate) fn atoi(text: &[u8]) -> i32 {
    let mut i = 0;
    while i < text.len() && matches!(text[i], b' ' | b'\t' | b'\n' | 0x0b | 0x0c | b'\r') {
        i += 1;
    }
    let negative = i < text.len() && text[i] == b'-';
    if i < text.len() && (text[i] == b'-' || text[i] == b'+') {
        i += 1;
    }
    let mut value: i32 = 0;
    while i < text.len() && text[i].is_ascii_digit() {
        value = value.wrapping_mul(10).wrapping_add((text[i] - b'0') as i32);
        i += 1;
    }
    if negative {
        value.wrapping_neg()
    } else {
        value
    }
}

// ------------------------------------------------------------------------------------------------ the toolkit

/// The label `control`'s line 0 (`UITextLabel::GetText(0)`; empty for a missing label).
pub(crate) fn label_text(control: Cpp) -> Vec<u8> {
    let line = direct::label_call(control, EeUiLabelCall::Line, 0, 0).unwrap_or(0) as *const CppWide;
    // SAFETY: the label's line (a UWideString it holds), or NULL
    unsafe { wide_bytes(line) }
}

/// `UITextLabel::SetText(text)` (the replacing `SetText(text, true)`).
pub(crate) fn label_set_text(control: Cpp, text: &[u8]) {
    let t = direct::c_text(text);
    direct::label_call(control, EeUiLabelCall::SetText, t.as_ptr() as isize, 1);
}

/// `UITextLabel::SetText(long)`: the value as `PlatIToA` writes it in base 10.
pub(crate) fn label_set_long(control: Cpp, value: c_long) {
    label_set_text(control, value.to_string().as_bytes());
}

/// The form's control `index`.
fn ctl(form: Cpp, index: i64) -> Cpp {
    direct::control(form, index as c_ulong)
}

/// The form's manager (`GetManager()`).
fn manager(form: Cpp) -> Cpp {
    direct::manager_of(form)
}

/// The rectangle record of the control (left, top, right, bottom), zero for a missing control.
fn rect_of(control: Cpp) -> [f32; 4] {
    direct::with_fields(control, |r| [r.mRectangle.left, r.mRectangle.top, r.mRectangle.right, r.mRectangle.bottom])
        .unwrap_or([0.0; 4])
}

// ---------------------------------------------------------------------------------------- the main game form

/// `kMainGameTitle`, `kMainGameErrorLog`, `kMainGameDialogMessage` (ee/EEUIManager.h; the portrait is
/// `EE_SCREENS_MF_CTL_DIALOG_PORTRAIT`).
const MF_CTL_TITLE: i64 = 25;
const MF_CTL_ERROR_LOG: i64 = 27;
const MF_CTL_DIALOG_MESSAGE: i64 = 211;

/// The main game form's call record (`EEUIFMGFormCall`, EEUIFMainGame.cpp, which asserts its layout): the form,
/// the picked unit, the held text, the chat line's colour, name and text, then the C++ objects of the call (the
/// command button and its help string's result last).
#[repr(C)]
pub(crate) struct FormCallHead {
    pub(crate) form: Cpp,
    picked: *mut c_void,
    pub(crate) text: CppWide,
    color: *const c_void,
    pub(crate) name: CppWide,
    pub(crate) chat: CppWide,
    flags: c_ulong,
    /// the dialog message, node, unit, caster, calamity, old and new media, first unit, tile, wall and gate
    objects: [*mut c_void; 11],
    /// the command button (an `EEUIFMGStatusButton*`) and its help string's result (a `UWideString*`)
    pub(crate) button: Cpp,
    pub(crate) out: *mut CppWide,
}

/// The main game form's host ops on the toolkit, its call's strings and the language strings (`call` the host's
/// call record), each its C++ case's call.
///
/// # Safety
/// `call` is the host's live call record.
pub(crate) unsafe fn main_form(
    call: *mut FormCallHead,
    op: u32,
    a: i64,
    b: i64,
    io: &mut EeScreensMainFormIo,
) -> Option<i64> {
    // SAFETY: the contract (the record's members are reached through the pointer, never held across a call)
    let form = unsafe { (*call).form };
    let text = || unsafe { wide_bytes(&(*call).text) };
    let set_text = |t: &[u8]| unsafe { wide_assign(&mut (*call).text, t) };
    Some(match op {
        // ---- the held text and the chat line's
        EE_SCREENS_MF_OP_TEXT_SET => {
            // SAFETY: the Rust caller's `a` bytes at `b`
            let bytes = unsafe { core::slice::from_raw_parts(b as *const u8, a.max(0) as usize) };
            set_text(bytes);
            0
        }
        EE_SCREENS_MF_OP_TEXT_GET => {
            let t = text();
            if b != 0 {
                let n = t.len().min(a.max(0) as usize);
                // SAFETY: the Rust caller's buffer of `a` bytes at `b`
                unsafe { core::ptr::copy_nonoverlapping(t.as_ptr(), b as *mut u8, n) };
            }
            t.len() as i64
        }
        EE_SCREENS_MF_OP_TEXT_STRING => {
            set_text(resource_string(a)?);
            0
        }
        EE_SCREENS_MF_OP_CHAT_LENGTH => unsafe { (*call).chat.s.length as i64 },
        EE_SCREENS_MF_OP_CHAT_REST => {
            // the rest of the chat text after `a` characters (and a line break there); the name emptied
            // SAFETY: the chat's NUL-terminated buffer; the rest is copied before the chat is assigned
            let rest = unsafe {
                let mut next = PlatMbsNInc((*call).chat.s.string as *const u8, a as usize) as *const u8;
                if PlatMbsNBCmp(next, c"\n".as_ptr().cast(), 1) == 0 {
                    next = PlatMbsInc(next);
                }
                direct::c_bytes(next as *const c_void)
            };
            unsafe {
                wide_assign(&mut (*call).chat, &rest);
                wide_assign(&mut (*call).name, b"");
            }
            0
        }
        // ---- the chat lines (hud_ctl_ffi.rs: their records, timers and texts; absent from a test build)
        #[cfg(not(test))]
        EE_SCREENS_MF_OP_CHAT_DISPLAY => unsafe {
            let (color, flags) = ((*call).color, (*call).flags);
            let (name, chat) =
                (&(*call).name as *const CppWide as *const c_void, &(*call).chat as *const CppWide as *const c_void);
            crate::screens::hud_ctl_ffi::ee_screens_chat_line_display(ctl(form, a), color, name, chat, flags as u32)
                as i64
        },
        #[cfg(not(test))]
        EE_SCREENS_MF_OP_CHAT_COPY => {
            crate::screens::hud_ctl_ffi::chat_line_copy(ctl(form, a), ctl(form, a + 1)) as i64
        }
        #[cfg(not(test))]
        EE_SCREENS_MF_OP_CHAT_SHADOW => {
            let Some(r) = crate::screens::hud_ctl_ffi::chat_record(ctl(form, a)) else { return Some(0) };
            // SAFETY: the live line's record
            unsafe { (*r).mShadowOffset = b as c_ulong as u32 };
            1
        }
        // ---- the labels
        EE_SCREENS_MF_OP_LABEL_INT => atoi(&label_text(ctl(form, a))) as i64,
        EE_SCREENS_MF_OP_LABEL_SET_LONG => {
            label_set_long(ctl(form, a), b as c_long);
            0
        }
        EE_SCREENS_MF_OP_LABEL_DIFFERS => differs(&label_text(ctl(form, a)), &text()) as i64,
        EE_SCREENS_MF_OP_LABEL_SET_TEXT => {
            label_set_text(ctl(form, a), &text());
            0
        }
        EE_SCREENS_MF_OP_STATUS_TEXT => {
            label_set_text(ctl(form, MF_CTL_TITLE), &text());
            0
        }
        EE_SCREENS_MF_OP_LOG_APPEND => {
            direct::list_op(ctl(form, MF_CTL_ERROR_LOG), EeUiOp::ListAppendText, c"".as_ptr() as isize, 0, 0);
            0
        }
        // ---- the dialog message's scrolling text and portrait
        EE_SCREENS_MF_OP_INFO_HAS_TEXT => (!ctl(form, MF_CTL_DIALOG_MESSAGE).is_null()) as i64,
        EE_SCREENS_MF_OP_INFO_HAS_PORTRAIT => (!ctl(form, EE_SCREENS_MF_CTL_DIALOG_PORTRAIT).is_null()) as i64,
        EE_SCREENS_MF_OP_INFO_SCROLL => {
            let t = ctl(form, MF_CTL_DIALOG_MESSAGE);
            direct::label_call(t, EeUiLabelCall::ScrollTextFor, a as c_ulong as isize, b as c_ulong as isize);
            0
        }
        EE_SCREENS_MF_OP_INFO_RECT => {
            let r = rect_of(ctl(form, MF_CTL_DIALOG_MESSAGE));
            io.f[0] = r[3];
            io.f[1] = r[1];
            0
        }
        EE_SCREENS_MF_OP_INFO_STOP => {
            direct::label_call(ctl(form, MF_CTL_DIALOG_MESSAGE), EeUiLabelCall::StopScrolling, 0, 0);
            0
        }
        EE_SCREENS_MF_OP_INFO_PAUSE => {
            let call = if a != 0 { EeUiLabelCall::PauseScrolling } else { EeUiLabelCall::ResumeScrolling };
            direct::label_call(ctl(form, MF_CTL_DIALOG_MESSAGE), call, 0, 0);
            0
        }
        // ---- the form
        EE_SCREENS_MF_OP_MOUSE_CAPTURED => (direct::form_call(form, EeUiFormCall::MouseCaptured, 0, 0) != 0) as i64,
        EE_SCREENS_MF_OP_ADD_DIRTY => {
            // io.f is (left, top, right, bottom); the manager's call takes a GERectangle's order (bottom, left,
            // right, top: UIManager.cpp's UIManagerPutRect)
            let mut f = [io.f[3], io.f[0], io.f[2], io.f[1]];
            let m = direct::find(direct::Kind::Manager, manager(form)) as *const ee_ui::managerobj::EeUiManager;
            if !m.is_null() {
                // SAFETY: the live manager; f the rectangle
                unsafe {
                    ee_ui::managerobj::ee_ui_manager_call(
                        m,
                        EeUiManagerCall::AddDirtyRectangle,
                        form as isize,
                        0,
                        0,
                        f.as_mut_ptr(),
                    )
                };
            }
            0
        }
        EE_SCREENS_MF_OP_ENTITY_GROUP => {
            let g = direct::form_call(form, EeUiFormCall::EntityGroup, a as isize, 0) as Cpp;
            let call = if b != 0 { EeUiEntityGroupCall::Enable } else { EeUiEntityGroupCall::Disable };
            direct::group_call(g, call, 0);
            0
        }
        EE_SCREENS_MF_OP_HOT_KEY_GROUP => {
            direct::form_call(form, EeUiFormCall::SetHotKeyGroupEnabled, a as c_ulong as isize, (b != 0) as isize);
            0
        }
        EE_SCREENS_MF_OP_FORCE_UI_UPDATE => {
            direct::manager_call(manager(form), EeUiManagerCall::ForceUiUpdate, 0, 0, 0);
            0
        }
        EE_SCREENS_MF_OP_ALL_DIRTY => {
            direct::form_call(form, EeUiFormCall::SetAllControlsDirty, 0, 0);
            0
        }
        EE_SCREENS_MF_OP_BACKGROUND => {
            let bg = direct::form_call(form, EeUiFormCall::BackgroundById, a as c_ulong as isize, 0) as Cpp;
            direct::background_call(bg, EeUiBgCall::SetVisibility, (b != 0) as isize, 0);
            0
        }
        EE_SCREENS_MF_OP_REDRAW_ALL => {
            direct::manager_call(manager(form), EeUiManagerCall::RedrawAll, 0, 0, 0);
            0
        }
        EE_SCREENS_MF_OP_FORM_VISIBLE => {
            (direct::manager_call(manager(form), EeUiManagerCall::IsFormVisible, form as isize, 0, 0) != 0) as i64
        }
        EE_SCREENS_MF_OP_BASE_COPY_DATA => direct::form_call(form, EeUiFormCall::CopyData, 0, 0) as c_long as i64,
        op if op >= crate::screens::status_button::EE_SCREENS_MF_OP_SB => {
            // SAFETY: the contract
            let (button, out) = unsafe { ((*call).button, (*call).out) };
            return status_button(form, button, out, op - crate::screens::status_button::EE_SCREENS_MF_OP_SB, a, b);
        }
        EE_SCREENS_MF_OP_BASE => {
            let call = match a {
                EE_SCREENS_MF_BASE_ACTIVATE => EeUiFormCall::Activate,
                EE_SCREENS_MF_BASE_DEACTIVATE => EeUiFormCall::Deactivate,
                EE_SCREENS_MF_BASE_OPEN => EeUiFormCall::Open,
                EE_SCREENS_MF_BASE_CLOSE => EeUiFormCall::Close,
                EE_SCREENS_MF_BASE_UPDATE_MOUSE => EeUiFormCall::UpdateMouse,
                EE_SCREENS_MF_BASE_UPDATE_KEYBOARD => EeUiFormCall::UpdateKeyboard,
                _ => EeUiFormCall::DrawPolygons,
            };
            let r = direct::form_call(form, call, 0, 0) as c_long as i64;
            // the four void members answer 0
            if (EE_SCREENS_MF_BASE_ACTIVATE..=EE_SCREENS_MF_BASE_CLOSE).contains(&a) {
                0
            } else {
                r
            }
        }
        _ => return None,
    })
}

// ------------------------------------------------------------------------------------------ the command buttons

/// The command button's ops of the form's host (`EEStatusButtonOp`, `EE_SCREENS_SB_OP_*`) on its record
/// (`EeScreensStatusButtonFields`), its `UIButton` and its form's controls: `out` the help string's result.
pub(crate) fn status_button(form: Cpp, button: Cpp, out: *mut CppWide, op: u32, a: i64, b: i64) -> Option<i64> {
    use crate::screens::status_button::*;
    use ee_ui::buttonobj::EeUiButtonCall as B;
    let rec = crate::screens::main_form_fields::button_record(button)?;
    // SAFETY: the live button's record; no call out while the reference lives
    let field = |f: &mut dyn FnMut(&mut crate::screens::main_form_fields::EeScreensStatusButtonFields) -> i64| {
        f(unsafe { &mut *rec })
    };
    let bitmap = |c: B| direct::button_call(button, c, 0, 0).unwrap_or(0);
    Some(match op {
        EE_SCREENS_SB_OP_IS_COMMAND => field(&mut |r| r.mIsCommand as i64),
        EE_SCREENS_SB_OP_BUTTON => field(&mut |r| r.mButton as i64),
        EE_SCREENS_SB_OP_LAST_ERROR => field(&mut |r| r.mLastErrorTime as u32 as i64),
        EE_SCREENS_SB_OP_SET_LAST_ERROR => field(&mut |r| {
            r.mLastErrorTime = a as c_ulong;
            0
        }),
        EE_SCREENS_SB_OP_SET_SHOW_COUNT => field(&mut |r| {
            r.mShowCount = a != 0;
            0
        }),
        EE_SCREENS_SB_OP_COUNT => field(&mut |r| r.mCount as i64),
        EE_SCREENS_SB_OP_SET_AVAILABLE => field(&mut |r| {
            r.mAvailable = a != 0;
            0
        }),
        EE_SCREENS_SB_OP_TIME => ee_base::time::ee_time_get() as i64,
        EE_SCREENS_SB_OP_STATE => direct::button_call(button, B::State, 0, 0).unwrap_or(0) as c_ulong as i64,
        EE_SCREENS_SB_OP_BITMAPS => {
            let (normal, pressed, focused) =
                (bitmap(B::NormalBitmap), bitmap(B::PressedBitmap), bitmap(B::FocusedBitmap));
            ((normal == pressed) as i64) | (((focused == pressed) as i64) << 1) | (((pressed == 0) as i64) << 2)
        }
        EE_SCREENS_SB_OP_BASE_COPY_DATA => direct::button_call(button, B::CopyData, 0, 0).unwrap_or(0) as c_long as i64,
        EE_SCREENS_SB_OP_BASE_BUTTON_UP => {
            direct::button_call(button, B::ButtonUp, 0, 0);
            0
        }
        EE_SCREENS_SB_OP_RELEASE_MOUSE => {
            direct::control_virtual(button, EeUiOp::ReleaseMouse, 0, 0, &mut [0.0; 4]);
            0
        }
        EE_SCREENS_SB_OP_SHOW_CANCEL => {
            let cancel = ctl(form, EE_SCREENS_MF_CTL_CANCEL_UI_MODE);
            direct::control_virtual(cancel, EeUiOp::SetDisplayMode, 1, 0, &mut [0.0; 4]);
            0
        }
        EE_SCREENS_SB_OP_HELP_OUT => {
            if !out.is_null() {
                // SAFETY: the Rust caller's `b` bytes at `a`; out the help string's live result
                unsafe { wide_assign(out, core::slice::from_raw_parts(a as *const u8, b.max(0) as usize)) };
            }
            0
        }
        _ => return None,
    })
}

/// The main panel group's ops on its command buttons' records (`ee_screenscpp_main_group`, `EE_SCREENS_MGR_OP_*`):
/// `owner` the group's form (`mOwner`), whose control a is the button.
pub(crate) fn main_group(owner: impl FnOnce() -> Cpp, op: u32, a: i64, b: i64) -> Option<i64> {
    use crate::screens::main_group::*;
    use ee_ui::buttonobj::EeUiButtonCall as B;
    if !matches!(
        op,
        EE_SCREENS_MGR_OP_RESET_COUNT
            | EE_SCREENS_MGR_OP_ADD_COUNT
            | EE_SCREENS_MGR_OP_BUTTON_TECH
            | EE_SCREENS_MGR_OP_COPY_BITMAPS
    ) {
        return None;
    }
    let form = owner();
    let button = ctl(form, a);
    if op == EE_SCREENS_MGR_OP_COPY_BITMAPS {
        // the control b's normal and pressed bitmaps on the button a
        let source = ctl(form, b);
        let normal = direct::button_call(source, B::NormalBitmap, 0, 0).unwrap_or(0);
        let pressed = direct::button_call(source, B::PressedBitmap, 0, 0).unwrap_or(0);
        direct::button_call(button, B::SetNormalBitmap, normal, 0);
        direct::button_call(button, B::SetPressedBitmap, pressed, 0);
        return Some(0);
    }
    let rec = crate::screens::main_form_fields::button_record(button)?;
    // SAFETY: the live button's record; the borrow ends before the dirty call
    let r = unsafe { &mut *rec };
    Some(match op {
        EE_SCREENS_MGR_OP_RESET_COUNT => {
            r.mCount = 0;
            direct::control_virtual(button, EeUiOp::SetDirty, 1, 0, &mut [0.0; 4]);
            0
        }
        EE_SCREENS_MGR_OP_ADD_COUNT => {
            r.mCount = r.mCount.wrapping_add(b as c_long);
            0
        }
        _ => {
            if r.mIsCommand {
                EE_SCREENS_MGR_COMMAND
            } else {
                r.mTechnology as i64
            }
        }
    })
}

// --------------------------------------------------------------------------------------------- the status box

/// The head of the status box's call record (`EEUIFMGStatusBoxCall`, EEUIFMGStatusBox.cpp, which asserts it): the
/// box (its record), the unit, the complex unit, the held text.
#[repr(C)]
pub(crate) struct BoxCallHead {
    pub(crate) record: *mut crate::screens::main_form_fields::EeScreensStatusBoxFields,
    unit: *mut c_void,
    complex: *mut c_void,
    pub(crate) text: CppWide,
}

/// The status box's host ops (`ee_screenscpp_status_box`, `EE_SCREENS_BOX_OP_*`) on its record, its controls'
/// toolkit objects, the held text and the world's views.
///
/// # Safety
/// `call` is the host's live call record.
pub(crate) unsafe fn status_box(call: *mut BoxCallHead, op: u32, a: i64, b: i64, c: i64) -> Option<i64> {
    use crate::screens::status_box::*;
    // SAFETY: the contract
    let rec = unsafe { (*call).record };
    let text = || unsafe { wide_bytes(&(*call).text) };
    let set_text = |t: &[u8]| unsafe { wide_assign(&mut (*call).text, t) };
    if op == EE_SCREENS_BOX_OP_FORMAT {
        let (b, c) = (b as c_long, c as c_long);
        set_text(
            match a {
                EE_SCREENS_BOX_FMT_PAIR => format!("{b}/{c}"),
                EE_SCREENS_BOX_FMT_BONUS => format!("+ {b}"),
                EE_SCREENS_BOX_FMT_SUM => format!("{b}+{c}"),
                EE_SCREENS_BOX_FMT_ONE => format!("{b}"),
                _ => String::new(),
            }
            .as_bytes(),
        );
        return Some(0);
    }
    if op == EE_SCREENS_BOX_OP_IN_CINEMATIC {
        // SAFETY: the UI thread holds the world's lock over the box's update
        let t = unsafe { ee_sim::world::WorldRead::begin() };
        return Some(ee_sim::world::WorldView::global(&t).is_some_and(|w| w.in_cinematic()) as i64);
    }
    if rec.is_null() {
        return None;
    }
    // SAFETY: the live box's record, read afresh for each op (no reference across a call out)
    let control = |id: i64| unsafe { (*rec).control(id) };
    Some(match op {
        EE_SCREENS_BOX_OP_GET => unsafe {
            match a {
                EE_SCREENS_BOX_FIELD_COUNT => (*rec).mAttributeCount as i64,
                EE_SCREENS_BOX_FIELD_ARMOR_COUNT => (*rec).mArmorAttributeCount as i64,
                EE_SCREENS_BOX_FIELD_RESOURCE_SHOWN => (*rec).mIsResourceShown as i64,
                _ => *(*rec).mAttributes.get((a - EE_SCREENS_BOX_FIELD_ATTRIBUTES) as usize)? as i64,
            }
        },
        EE_SCREENS_BOX_OP_SET => unsafe {
            match a {
                EE_SCREENS_BOX_FIELD_COUNT => (*rec).mAttributeCount = b as c_long,
                EE_SCREENS_BOX_FIELD_ARMOR_COUNT => (*rec).mArmorAttributeCount = b as c_long,
                EE_SCREENS_BOX_FIELD_RESOURCE_SHOWN => (*rec).mIsResourceShown = b != 0,
                _ => *(*rec).mAttributes.get_mut((a - EE_SCREENS_BOX_FIELD_ATTRIBUTES) as usize)? = b as c_long,
            }
            0
        },
        EE_SCREENS_BOX_OP_PRESENT => (!control(a).is_null()) as i64,
        EE_SCREENS_BOX_OP_MODE => direct::with_fields(control(a), |r| r.mDisplayMode as i64).unwrap_or(0),
        EE_SCREENS_BOX_OP_SET_MODE => {
            direct::control_virtual(control(a), EeUiOp::SetDisplayMode, b as c_ulong as isize, 0, &mut [0.0; 4]);
            0
        }
        EE_SCREENS_BOX_OP_DIRTY => {
            direct::control_virtual(control(a), EeUiOp::SetDirty, 1, 0, &mut [0.0; 4]);
            0
        }
        EE_SCREENS_BOX_OP_STRING_ID => {
            direct::with_fields(control(a), |r| r.mStringTableID = b as c_ulong);
            0
        }
        EE_SCREENS_BOX_OP_SET_TEXT => {
            label_set_text(control(a), &text());
            0
        }
        EE_SCREENS_BOX_OP_TEXT_DIFFERS => differs(&label_text(control(a)), &text()) as i64,
        _ => return None,
    })
}

// ---------------------------------------------------------------------------------- the main game form's events

/// The main game form's events' ops of the screens' host (`EEScreensMainGameCall`, main_game.rs
/// `EE_SCREENS_GAME_MG_*`) on the form's record (`EeScreensMainFormFields`) and its controls: `form` the event's
/// form (the main game form).
pub(crate) fn main_game(form: Cpp, op: u32, a: u32) -> Option<u32> {
    use crate::screens::main_game::*;
    let field = |f: &mut dyn FnMut(&mut crate::screens::main_form_fields::EeScreensMainFormFields) -> u32| {
        let rec = crate::screens::main_form_fields::record(form)?;
        // SAFETY: the live form's record; no call out while the reference lives
        Some(f(unsafe { &mut *rec }))
    };
    match op {
        EE_SCREENS_GAME_MG_UI_STATE => field(&mut |r| r.mUIState as u32),
        EE_SCREENS_GAME_MG_CURSOR_LOCK => field(&mut |r| {
            r.mIsCursorLocked = a != 0;
            0
        }),
        EE_SCREENS_GAME_MG_CURRENT_GROUP => field(&mut |r| {
            r.mCurrentGroup = a as c_ulong;
            0
        }),
        EE_SCREENS_GAME_MG_DIALOG_SHOWN => {
            let shown = direct::with_fields(ctl(form, MF_CTL_DIALOG_MESSAGE), |r| r.mDisplayMode == 1).unwrap_or(false);
            // SAFETY: the UI thread holds the world over its events
            let t = unsafe { ee_sim::world::WorldRead::begin() };
            Some((shown || ee_sim::world::WorldView::global(&t).is_some_and(|w| w.in_cinematic())) as u32)
        }
        _ => None,
    }
}

// ------------------------------------------------------------------------------------------ the HUD controls

/// The control's rectangle record as an op's floats (left, top, right, bottom).
fn put_ltrb(r: &ee_ui::rect::UiRect, f: &mut [f32]) {
    f[..4].copy_from_slice(&[r.left, r.top, r.right, r.bottom]);
}

/// Any HUD control's base ops (`EEUIRustControl::HostOp`, `EE_SCREENS_HC_*`) on the control's toolkit record:
/// `ctl` the control (an `EEUIRustControl*`), `f` the op's floats (rectangles left, top, right, bottom).
pub(crate) fn control(ctl: Cpp, op: u32, a: i64, f: &mut [f32]) -> Option<i64> {
    use crate::screens::hud_ctl::*;
    let virt = |op: EeUiOp, a: isize, f0: f32| {
        direct::control_virtual(ctl, op, a, 0, &mut [f0, 0.0, 0.0, 0.0]);
    };
    let form = || direct::with_fields(ctl, |r| r.mForm).unwrap_or(core::ptr::null_mut());
    // a rectangle in a GERectangle's order (bottom, left, right, top: the form's and manager's calls take it)
    let ge = |f: &[f32]| [f[3], f[0], f[2], f[1]];
    Some(match op {
        EE_SCREENS_HC_SET_DIRTY => {
            virt(EeUiOp::SetDirty, (a != 0) as isize, 0.0);
            0
        }
        EE_SCREENS_HC_SET_DISPLAY_MODE => {
            virt(EeUiOp::SetDisplayMode, a as c_ulong as isize, 0.0);
            0
        }
        EE_SCREENS_HC_SET_BG_OPACITY => {
            virt(EeUiOp::SetBgOpacity, 0, f[0]);
            0
        }
        EE_SCREENS_HC_COPY_BASE => {
            direct::control_call(ctl, ee_ui::controlobj::EeUiControlCall::CopyData, 0, 0) as c_long as i64
        }
        EE_SCREENS_HC_TEXT_BASE => {
            // UIControl::DrawTextA: the draw count down, kErrorOK
            direct::with_fields(ctl, |r| r.mDrawCount = r.mDrawCount.wrapping_sub(1));
            0
        }
        EE_SCREENS_HC_CAPTURE_MOUSE | EE_SCREENS_HC_CAPTURE_KEYBOARD | EE_SCREENS_HC_RELEASE_KEYBOARD => {
            use ee_ui::controlobj::EeUiControlCall as C;
            let call = match op {
                EE_SCREENS_HC_CAPTURE_MOUSE => C::CaptureMouse,
                EE_SCREENS_HC_CAPTURE_KEYBOARD => C::CaptureKeyboard,
                // (no HUD control overrides ReleaseCapturedKeyboard: the base's)
                _ => C::ReleaseKeyboard,
            };
            direct::control_call(ctl, call, 0, 0);
            0
        }
        EE_SCREENS_HC_RELEASE_MOUSE => {
            virt(EeUiOp::ReleaseMouse, 0, 0.0);
            0
        }
        EE_SCREENS_HC_RECT => {
            let r = direct::with_fields(ctl, |r| r.mRectangle)?;
            put_ltrb(&r, f);
            0
        }
        EE_SCREENS_HC_COPIED_RECT => {
            let r = direct::with_fields(ctl, |r| r.mCopiedDrawRectangle)?;
            put_ltrb(&r, f);
            0
        }
        EE_SCREENS_HC_DISPLAY_MODE => direct::with_fields(ctl, |r| r.mDisplayMode as i64).unwrap_or(0),
        EE_SCREENS_HC_MODIFIERS => ee_base::input::ffi::keyboard_modifiers() as i64,
        EE_SCREENS_HC_FORM_REDRAW => {
            direct::form_call_f(form(), EeUiFormCall::RedrawIntersectControls, 0, 0, &mut ge(f));
            0
        }
        EE_SCREENS_HC_ADD_DIRTY => {
            let form = form();
            let m = direct::find(direct::Kind::Manager, manager(form)) as *const ee_ui::managerobj::EeUiManager;
            if !m.is_null() {
                let mut r = ge(f);
                // SAFETY: the live manager; r the rectangle
                unsafe {
                    ee_ui::managerobj::ee_ui_manager_call(
                        m,
                        EeUiManagerCall::AddDirtyRectangle,
                        form as isize,
                        0,
                        0,
                        r.as_mut_ptr(),
                    )
                };
            }
            0
        }
        _ => return None,
    })
}

// ------------------------------------------------------------------------------------------------ the cursor

/// `EEUIFMGCursorRead` (EEUIFMainGame.cpp, which asserts its size): the form, the local player, its selected
/// group, the group's first unit and the picked target, for one `UpdateCursorKind`.
#[repr(C)]
pub(crate) struct CursorRead {
    form: Cpp,
    player: *mut c_void,
    group: *mut c_void,
    first: *mut c_void,
    target: *mut c_void,
}

/// The cursor's world reads (`ee_screenscpp_cursor`, `EE_SCREENS_CURSOR_OP_*`) on the world's views: the first
/// selected unit's, the target's (the picked `mTarget`), the selection's unit `a` and its cargo `b`; positions and
/// ranges into `f`. The tests that are a unit's or class's method (gathering, healing, converting, attacking a
/// class, the protection, the aim) stay the C++'s.
///
/// # Safety
/// `read` is the host's live read state; its units are live (the UI thread holds the world).
pub(crate) unsafe fn cursor(read: *const CursorRead, op: u32, a: u32, b: u32, f: &mut [f32; 3]) -> Option<i32> {
    use crate::screens::cursor::*;
    use ee_sim::world::{GroupView, PlayerView, UnitView, WorldRead};
    use ee_sim::world_field;
    // SAFETY: the UI thread holds the world's read or write lock over the cursor's update
    let t = unsafe { WorldRead::begin() };
    // SAFETY: the contract
    let (player, group, first, target) = unsafe { ((*read).player, (*read).group, (*read).first, (*read).target) };
    let unit = |h: *mut c_void| unsafe { UnitView::of(&t, h as u64) };
    let selected = || {
        let g = unsafe { GroupView::of(&t, group as u64) }?;
        unit(g.unit_handle(a as usize) as *mut c_void)
    };
    let cargo = || selected()?.complex_unit()?.cargo(b as usize);
    let target = || unit(target);
    let class_flag = |u: Option<UnitView<'_>>, k: fn(&ee_sim::world::ClassView<'_>) -> bool| {
        u.and_then(|u| u.class()).is_some_and(|c| k(&c)) as i32
    };
    let attack = |u: Option<UnitView<'_>>| {
        u.and_then(|u| u.stats()).is_some_and(|s| world_field!(s.fields(), mAttack) > 0.0) as i32
    };
    let kind_flags =
        |u: Option<UnitView<'_>>| u.and_then(|u| u.class()).map_or(0, |c| world_field!(c.fields(), mKindFlags) as i32);
    let position = |u: Option<UnitView<'_>>, f: &mut [f32; 3]| {
        *f = u.map_or([0.0; 3], |u| u.location());
        0
    };
    let min_range = |u: Option<UnitView<'_>>, f: &mut [f32; 3]| {
        f[0] = u.and_then(|u| u.class()).map_or(0.0, |c| world_field!(c.fields(), mMinRange));
        0
    };
    Some(match op {
        EE_SCREENS_CURSOR_OP_FIRST => match unit(first) {
            None => 0,
            Some(u) if u.hit_points() > 0 => 1,
            Some(_) => 2,
        },
        EE_SCREENS_CURSOR_OP_FIRST_MEDIC => class_flag(unit(first), |c| world_field!(c.fields(), mIsMedic)),
        EE_SCREENS_CURSOR_OP_T_KIND => kind_flags(target()),
        EE_SCREENS_CURSOR_OP_T_OWNER => {
            let owner = target().and_then(|u| u.player());
            if owner.as_ref().map_or(0, |p| p.handle()) == player as u64 {
                0
            } else {
                let me = unsafe { PlayerView::of(&t, player as u64) };
                let id = owner.map_or(0, |p| p.id()) as usize;
                if me.is_some_and(|p| p.enemy(id) != 0) {
                    1
                } else {
                    2
                }
            }
        }
        EE_SCREENS_CURSOR_OP_T_HP => target().map_or(0, |u| u.hit_points() as c_long as i32),
        EE_SCREENS_CURSOR_OP_T_MAX_HP => {
            target().and_then(|u| u.stats()).map_or(0, |s| s.max_hit_points() as c_long as i32)
        }
        EE_SCREENS_CURSOR_OP_T_ATTACK => attack(target()),
        EE_SCREENS_CURSOR_OP_T_TYPE => target().and_then(|u| u.class()).map_or(0, |c| c.unit_type() as i32),
        EE_SCREENS_CURSOR_OP_T_TOWER => class_flag(target(), |c| world_field!(c.fields(), mIsTower)),
        EE_SCREENS_CURSOR_OP_T_CATEGORY => target().and_then(|u| u.class()).map_or(0, |c| c.category() as i32),
        EE_SCREENS_CURSOR_OP_T_ENTERABLE => {
            target().is_some_and(|u| u.has_tile_node() && u.complex_unit().is_some_and(|c| c.is_built())) as i32
        }
        EE_SCREENS_CURSOR_OP_T_COMPLEX => target().is_some_and(|u| u.complex_unit_handle() != 0) as i32,
        EE_SCREENS_CURSOR_OP_SEL_COUNT => unsafe { GroupView::of(&t, group as u64) }.map_or(0, |g| g.len() as i32),
        EE_SCREENS_CURSOR_OP_SEL_KIND => kind_flags(selected()),
        EE_SCREENS_CURSOR_OP_SEL_ATTACK => attack(selected()),
        EE_SCREENS_CURSOR_OP_SEL_BUILT => {
            selected().and_then(|u| u.complex_unit()).is_some_and(|c| c.is_built()) as i32
        }
        EE_SCREENS_CURSOR_OP_SEL_ON_MAP => selected().is_some_and(|u| u.has_tile_node()) as i32,
        EE_SCREENS_CURSOR_OP_SEL_CARGO => selected().and_then(|u| u.complex_unit()).map_or(0, |c| c.cargo_len() as i32),
        EE_SCREENS_CURSOR_OP_SEL_TYPE => selected().and_then(|u| u.class()).map_or(0, |c| c.unit_type() as i32),
        EE_SCREENS_CURSOR_OP_SEL_COMPLEX => selected().is_some_and(|u| u.complex_unit_handle() != 0) as i32,
        EE_SCREENS_CURSOR_OP_SEL_POSITION => position(selected().and_then(|u| u.complex_unit()), f),
        EE_SCREENS_CURSOR_OP_SEL_MIN_RANGE => min_range(selected(), f),
        EE_SCREENS_CURSOR_OP_CARGO_ATTACK => attack(cargo()),
        EE_SCREENS_CURSOR_OP_CARGO_POSITION => position(cargo(), f),
        EE_SCREENS_CURSOR_OP_CARGO_MIN_RANGE => min_range(cargo(), f),
        _ => return None,
    })
}

#[cfg(test)]
#[path = "hud_direct_tests.rs"]
mod tests;
