//! The screens' toolkit ops on the toolkit's Rust objects directly (round 2 of game screens B; `ee_ui direct.rs`).
//!
//! The forms' hosts (`menus_ffi.rs`'s `Form`) send every op of the screens to `EEScreens.cpp`'s host, one C++ `case`
//! each. The ops that only call the UI toolkit (a control found by the form and its index, then one of its methods)
//! are answered here instead, before the C++ hop: the control's C++ pointer comes from the form's Rust object, the
//! method is the same inward call its C++ forwarder makes (`ee_ui::direct`), and the virtuals a game class may
//! override go through the control's C++ object (`direct::control_virtual`). Every op here was a `case` of the C++
//! host, transcribed step for step; the C++ cases are gone.
//!
//! A control the form does not have answers as the C++ answered a NULL `GetControl` where it checked (0, nothing
//! done); where the C++ did not check (it would have crashed), nothing is done either.

// `c_ulong` is `u32` on Windows and `u64` on LP64: the casts between it and the ops' `u32` stay
#![allow(clippy::unnecessary_cast)]

use core::ffi::{c_ulong, c_void};

use ee_ui::bgobj::EeUiBgCall;
use ee_ui::buttonobj::EeUiButtonCall;
use ee_ui::comboobj::EeUiComboCall;
use ee_ui::controlobj::EeUiControlCall;
use ee_ui::direct::{self, Cpp, Kind};
use ee_ui::editobj::EeUiEditCall;
use ee_ui::formobj::EeUiFormCall;
use ee_ui::groupobj::EeUiEntityGroupCall;
use ee_ui::labelobj::EeUiLabelCall;
use ee_ui::managerobj::EeUiManagerCall;
use ee_ui::obj::EeUiOp;
use ee_ui::sliderobj::EeUiSliderCall;

use crate::campaign_editor::EE_SCREENS_TEXT_CE_ALL_TEXT;
use crate::chat::*;
use crate::debug_sound::EE_SCREENS_OP_DSU_SLIDER;
use crate::diplomacy::EE_SCREENS_OP_DS_DISABLE;
use crate::lobby_form::*;
use crate::main_game_options::{EE_SCREENS_OP_GO_REMOVE_HOT_KEYS, EE_SCREENS_OP_GO_STRING_ID};
use crate::menus::*;
use crate::save_load::{EE_SCREENS_OP_CURRENT_FORM_IS, EE_SCREENS_OP_SWAP_FORM, EE_SCREENS_TEXT_FORM_EDIT};
use crate::scenario_screens::{
    EE_SCREENS_OP_LABEL_CLEAR, EE_SCREENS_OP_LABEL_LINE_COUNT, EE_SCREENS_OP_LABEL_START_LINE,
    EE_SCREENS_OP_REMOVE_BACKGROUND, EE_SCREENS_OP_TAB_STOP,
};
use crate::settings_form::*;

/// The toolkit's constants the ops use (lle/UIControl.h, GETextAlignment.h, ee/EEUIManager.h; associated, so the
/// header generator leaves them out).
struct K;

impl K {
    /// `kControlTriggerSelectionChanged` (lle/UIControl.h).
    const TK_TRIGGER_SELECTION_CHANGED: isize = 0x3eb;
    /// `kTextAlignLeft`, `kTextAlignCenter` (lle/GETextAlignment.h).
    const TK_ALIGN_LEFT: isize = 0;
    const TK_ALIGN_CENTER: isize = 1;
    /// `kFormChatLog`, `kChatLogChatLogTextBox` (ee/EEUIManager.h): the chat log's label.
    const TK_FORM_CHAT_LOG: c_ulong = 29;
    const TK_CHAT_LOG_TEXT_BOX: c_ulong = 1;
    /// `UIControl::HostOp`'s `FormEventCall`: `b` 1 dereferences the event `a`, 2 references it.
    const TK_EVENT_DEREFERENCE: isize = 1;
    const TK_EVENT_REFERENCE: isize = 2;
}

fn mgr(form: Cpp) -> Cpp {
    direct::manager_of(form)
}

fn form_by_id(form: Cpp, id: u32) -> Cpp {
    direct::form_by_id(mgr(form), id as c_ulong)
}

fn ctl(form: Cpp, index: u32) -> Cpp {
    direct::control(form, index as c_ulong)
}

fn virt(control: Cpp, op: EeUiOp, a: isize) -> isize {
    direct::control_virtual(control, op, a, 0, &mut [0.0; 4])
}

fn button(control: Cpp, call: EeUiButtonCall, a: isize) -> u32 {
    direct::button_call(control, call, a, 0).unwrap_or(0) as u32
}

fn label(control: Cpp, call: EeUiLabelCall, a: isize, b: isize) -> u32 {
    direct::label_call(control, call, a, b).unwrap_or(0) as u32
}

fn id_of(control: Cpp) -> Option<u32> {
    direct::with_fields(control, |r| r.mId as u32)
}

/// One of the screens' ops on the form `form` (the event's entity `entity`), when it is a toolkit op: its result.
/// `None` for the ops the C++ host answers.
pub(crate) fn op(form: *mut c_void, entity: *mut c_void, op: u32, a: u32, b: u32, c: u32) -> Option<u32> {
    let r = match op {
        // ---- the manager's
        EE_SCREENS_OP_PUSH => {
            let m = mgr(form);
            direct::manager_call(m, EeUiManagerCall::Push, direct::form_by_id(m, a as c_ulong) as isize, 0, 0);
            0
        }
        EE_SCREENS_OP_POP => {
            direct::manager_call(mgr(form), EeUiManagerCall::Pop, 0, 0, 0);
            0
        }
        EE_SCREENS_OP_SWAP_FORM => {
            let m = mgr(form);
            direct::manager_call(m, EeUiManagerCall::SwapForm, direct::form_by_id(m, a as c_ulong) as isize, 0, 0);
            0
        }
        EE_SCREENS_OP_CURRENT_FORM_IS => {
            let m = mgr(form);
            let current = direct::manager_call(m, EeUiManagerCall::CurrentForm, 0, 0, 0);
            (current == direct::form_by_id(m, a as c_ulong) as isize) as u32
        }
        EE_SCREENS_OP_LE_FORM_VISIBLE => {
            let m = mgr(form);
            let target = direct::form_by_id(m, a as c_ulong) as isize;
            (direct::manager_call(m, EeUiManagerCall::IsFormVisible, target, 0, 0) != 0) as u32
        }
        // ---- the form's
        EE_SCREENS_OP_GROUP => {
            let target = if c != 0 { form_by_id(form, c - 1) } else { form };
            let group = direct::form_call(target, EeUiFormCall::EntityGroup, a as isize, 0) as Cpp;
            let call = if b != 0 { EeUiEntityGroupCall::Enable } else { EeUiEntityGroupCall::Disable };
            direct::group_call(group, call, 0);
            0
        }
        EE_SCREENS_OP_BACKGROUND => {
            let target = if c != 0 { form_by_id(form, c - 1) } else { form };
            let bg = direct::form_call(target, EeUiFormCall::BackgroundById, a as isize, 0) as Cpp;
            direct::background_call(bg, EeUiBgCall::SetVisibility, (b != 0) as isize, 0);
            0
        }
        EE_SCREENS_OP_REMOVE_BACKGROUND => {
            // (the id as a signed long, as the C++'s GetBackgroundByID((long)(int32_t)inA))
            let bg = direct::form_call(form, EeUiFormCall::BackgroundById, a as i32 as c_ulong as isize, 0);
            if bg != 0 {
                direct::form_call(form, EeUiFormCall::RemoveBackground, bg, 0);
            }
            0
        }
        EE_SCREENS_OP_TAB_STOP | EE_SCREENS_OP_CH_TAB_STOP => {
            direct::form_call(form, EeUiFormCall::SetTabStop, a as isize, 0);
            0
        }
        EE_SCREENS_OP_LE_KEYBOARD_CAPTURED | EE_SCREENS_OP_CH_KEYBOARD_CAPTURED => {
            (direct::form_call(form, EeUiFormCall::KeyboardCaptured, 0, 0) != 0) as u32
        }
        EE_SCREENS_OP_GO_REMOVE_HOT_KEYS => {
            direct::form_call(form, EeUiFormCall::RemoveAllHotKeys, 0, 0);
            0
        }
        // ---- a control's (UIControl's)
        EE_SCREENS_OP_ENTITY_INDEX => direct::with_fields(entity, |r| r.mIndex as u32).unwrap_or(0),
        EE_SCREENS_OP_ENTITY_ID => {
            if entity.is_null() {
                0xffff_ffff
            } else {
                id_of(entity).unwrap_or(0)
            }
        }
        EE_SCREENS_OP_CONTROL_ID => id_of(ctl(form, a)).unwrap_or(0),
        EE_SCREENS_OP_SHOW => {
            virt(ctl(form, a), EeUiOp::SetDisplayMode, b as isize);
            0
        }
        EE_SCREENS_OP_RELEASE_MOUSE => {
            virt(ctl(form, a), EeUiOp::ReleaseMouse, 0);
            0
        }
        EE_SCREENS_OP_LE_DIRTY => {
            virt(ctl(form, a), EeUiOp::SetDirty, 1);
            0
        }
        EE_SCREENS_OP_LE_OPACITY => {
            direct::control_virtual(ctl(form, a), EeUiOp::SetBgOpacity, 0, 0, &mut [1.0, 0.0, 0.0, 0.0]);
            0
        }
        EE_SCREENS_OP_CH_TABBED_TO => {
            virt(ctl(form, a), EeUiOp::CtlTabbedTo, 0);
            0
        }
        EE_SCREENS_OP_GO_STRING_ID => {
            direct::with_fields(ctl(form, a), |r| r.mStringTableID = b as c_ulong);
            0
        }
        EE_SCREENS_OP_LE_HAS_TRIGGER => {
            (direct::control_call(ctl(form, a), EeUiControlCall::TriggerEvent, b as isize, 0) != 0) as u32
        }
        EE_SCREENS_OP_DSU_SLIDER => !direct::find(Kind::Slider, entity).is_null() as u32,
        // ---- the check boxes', radio buttons' and blinking buttons'
        EE_SCREENS_OP_CHECKBOX => button(ctl(form, a), EeUiButtonCall::CheckIsChecked, 0).min(1),
        EE_SCREENS_OP_SET_CHECKBOX => {
            button(ctl(form, a), EeUiButtonCall::CheckSetChecked, (b != 0) as isize);
            0
        }
        EE_SCREENS_OP_RADIO => button(ctl(form, a), EeUiButtonCall::RadioIsChecked, 0).min(1),
        EE_SCREENS_OP_CHECK_RADIO => {
            button(ctl(form, a), EeUiButtonCall::RadioCheck, 0);
            0
        }
        EE_SCREENS_OP_DS_DISABLE => {
            button(ctl(form, a), EeUiButtonCall::CheckSetDisabled, 1);
            0
        }
        EE_SCREENS_OP_LE_BLINKING => {
            button(ctl(form, a), EeUiButtonCall::BlinkSetIsBlinking, 1);
            0
        }
        // ---- the sliders'
        EE_SCREENS_OP_RANGE => {
            direct::slider_call(ctl(form, a), EeUiSliderCall::CurrentRangeValue, 0, 0).unwrap_or(0) as u32
        }
        EE_SCREENS_OP_SET_RANGE => {
            direct::slider_call(ctl(form, a), EeUiSliderCall::SetCurrentRangeValue, b as i32 as isize, 0);
            0
        }
        EE_SCREENS_OP_ENTITY_RANGE => {
            direct::slider_call(entity, EeUiSliderCall::CurrentRangeValue, 0, 0).unwrap_or(0) as u32
        }
        // ---- the combo boxes'
        EE_SCREENS_OP_COMBO_SELECT => {
            direct::combo_call(ctl(form, a), EeUiComboCall::SetCurrentSelection, b as isize, 0);
            0
        }
        EE_SCREENS_OP_COMBO_CLEAR => {
            direct::combo_call(ctl(form, a), EeUiComboCall::DeleteAllEntries, 0, 0);
            0
        }
        EE_SCREENS_OP_LE_TYPE_SELECT => {
            // LobbySelectPlayerType: the line without firing the box's change event
            let bx = ctl(form, a);
            let ev = direct::control_call(bx, EeUiControlCall::TriggerEvent, K::TK_TRIGGER_SELECTION_CHANGED, 0);
            if ev != 0 {
                direct::control_virtual(bx, EeUiOp::FormEventCall, ev, K::TK_EVENT_REFERENCE, &mut [0.0; 4]);
                direct::control_call(bx, EeUiControlCall::SetTrigger, K::TK_TRIGGER_SELECTION_CHANGED, 0);
            }
            if b != 0 {
                direct::combo_call(bx, EeUiComboCall::SetCurrentSelection, c as isize, 0);
            }
            if ev != 0 {
                direct::control_call(bx, EeUiControlCall::SetTrigger, K::TK_TRIGGER_SELECTION_CHANGED, ev);
                direct::control_virtual(bx, EeUiOp::FormEventCall, ev, K::TK_EVENT_DEREFERENCE, &mut [0.0; 4]);
            }
            0
        }
        // ---- the labels'
        EE_SCREENS_OP_LABEL_NUMBER => {
            let t = direct::label_number_text(b as i32);
            label(ctl(form, a), EeUiLabelCall::SetText, t.as_ptr() as isize, 1);
            0
        }
        EE_SCREENS_OP_LABEL_CLEAR => {
            label(ctl(form, a), EeUiLabelCall::DeleteArray, 0, 0);
            0
        }
        EE_SCREENS_OP_LABEL_LINE_COUNT => label(ctl(form, a), EeUiLabelCall::LineCount, 0, 0),
        EE_SCREENS_OP_LABEL_START_LINE => {
            label(ctl(form, a), EeUiLabelCall::SetStartingDisplayLine, b as isize, 0);
            0
        }
        EE_SCREENS_OP_CL_BEGIN => {
            let l = chat_log_label(form);
            label(l, EeUiLabelCall::DeleteArray, 0, 0);
            label(l, EeUiLabelCall::SetAlignment, K::TK_ALIGN_LEFT, 0);
            0
        }
        EE_SCREENS_OP_CL_CENTER => {
            label(chat_log_label(form), EeUiLabelCall::SetAlignment, K::TK_ALIGN_CENTER, 0);
            0
        }
        EE_SCREENS_OP_CL_END => {
            let l = chat_log_label(form);
            let lines = label(l, EeUiLabelCall::LineCount, 0, 0);
            label(l, EeUiLabelCall::SetStartingDisplayLine, lines.wrapping_sub(1) as c_ulong as isize, 0);
            0
        }
        // ---- the edit boxes'
        EE_SCREENS_OP_LE_EDIT_DELETE => {
            direct::edit_call(ctl(form, a), EeUiEditCall::Clear, 0, 0);
            0
        }
        _ => return None,
    };
    Some(r)
}

/// The chat log form's text label (`gUIManager`'s form 29, control 1: the chat log ops name the game's manager).
fn chat_log_label(_form: Cpp) -> Cpp {
    direct::control(
        direct::form_by_id(direct::manager_of(core::ptr::null_mut()), K::TK_FORM_CHAT_LOG),
        K::TK_CHAT_LOG_TEXT_BOX,
    )
}

/// A text of the screens (`EE_SCREENS_TEXT_*`) when it is a toolkit read: `Some(text)`; `None` for the host's.
pub(crate) fn text(form: *mut c_void, which: u32, arg: u32) -> Option<Vec<u8>> {
    match which {
        EE_SCREENS_TEXT_MUSIC_FILE => {
            let p = direct::form_call(form, EeUiFormCall::MusicFileName, 0, 0) as *const c_void;
            // SAFETY: the form's NUL-terminated name (or NULL for an unknown form)
            Some(unsafe { direct::c_bytes(p) })
        }
        EE_SCREENS_TEXT_EDIT => {
            let p = direct::edit_call(ctl(form, arg), EeUiEditCall::Line, 0, 0).unwrap_or(0) as *const c_void;
            // SAFETY: the edit box's NUL-terminated first line
            Some(unsafe { direct::c_bytes(p) })
        }
        EE_SCREENS_TEXT_CE_ALL_TEXT => {
            let p = direct::edit_call(ctl(form, arg), EeUiEditCall::AllText, 0, 0).unwrap_or(0) as *const c_void;
            // SAFETY: the edit box's NUL-terminated text
            Some(unsafe { direct::c_bytes(p) })
        }
        _ => None,
    }
}

/// A text into the screens (`EE_SCREENS_TEXT_*`) when it is a toolkit write: `Some(result)`; `None` for the host's.
pub(crate) fn put_text(form: *mut c_void, which: u32, arg: u32, text: &[u8]) -> Option<u32> {
    let t = direct::c_text(text);
    let p = t.as_ptr() as isize;
    match which {
        EE_SCREENS_TEXT_MUSIC_FILE => {
            direct::form_call(form, EeUiFormCall::SetMusicFileName, p, 0);
        }
        EE_SCREENS_TEXT_EDIT => {
            direct::edit_call(ctl(form, arg), EeUiEditCall::SetText, p, 0);
        }
        EE_SCREENS_TEXT_FORM_EDIT => {
            let target = form_by_id(core::ptr::null_mut(), arg >> 16);
            direct::edit_call(direct::control(target, (arg & 0xffff) as c_ulong), EeUiEditCall::SetText, p, 0);
        }
        EE_SCREENS_TEXT_LABEL => {
            direct::label_call(ctl(form, arg), EeUiLabelCall::SetText, p, 1);
        }
        _ => return None,
    }
    Some(0)
}

#[cfg(test)]
#[path = "toolkit_tests.rs"]
mod tests;
