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

use core::ffi::{c_long, c_ulong, c_void};
use std::sync::Mutex;

use ee_base::util::graph::{EeUtilColor, EeUtilGraph, EeUtilPoint, EeUtilSeries};

use ee_ui::bgobj::EeUiBgCall;
use ee_ui::buttonobj::EeUiButtonCall;
use ee_ui::comboobj::EeUiComboCall;
use ee_ui::controlobj::EeUiControlCall;
use ee_ui::direct::{self, Cpp, Kind};
use ee_ui::editobj::EeUiEditCall;
use ee_ui::formobj::EeUiFormCall;
use ee_ui::graphobj::UiGraph;
use ee_ui::groupobj::EeUiEntityGroupCall;
use ee_ui::labelobj::EeUiLabelCall;
use ee_ui::listobj::EeUiListCall;
use ee_ui::managerobj::EeUiManagerCall;
use ee_ui::obj::EeUiOp;
use ee_ui::sliderobj::EeUiSliderCall;
use ee_ui::statusobj::EeUiStatusBarCall;

use crate::camera::{
    EE_SCREENS_CAM_GRAB_EDITOR, EE_SCREENS_CAM_GRAB_MAIN, EE_SCREENS_OP_CAM_GRAB_BEGIN, EE_SCREENS_OP_CAM_GRAB_END,
};
use crate::campaign_editor::{
    EE_SCREENS_OP_CE_CLEAR_SELECTION, EE_SCREENS_OP_CE_COMBO_COUNT, EE_SCREENS_OP_CE_COMBO_SELECTION,
    EE_SCREENS_OP_CE_LIST_SELECT, EE_SCREENS_TEXT_CE_ALL_TEXT,
};
use crate::chat::*;
use crate::civ_choice::EE_SCREENS_OP_PDCS_HAS_SELECTION;
use crate::custom_campaign::*;
use crate::debug_sound::EE_SCREENS_OP_DSU_SLIDER;
use crate::diplomacy::{EE_SCREENS_OP_DS_BG, EE_SCREENS_OP_DS_DISABLE};
use crate::lobby_form::*;
use crate::main_game_options::{
    EE_SCREENS_OP_GO_HOT_KEY, EE_SCREENS_OP_GO_REMOVE_HOT_KEYS, EE_SCREENS_OP_GO_STRING_ID,
};
use crate::menus::*;
use crate::post_game::*;
use crate::save_load::{
    EE_SCREENS_OP_CURRENT_FORM_IS, EE_SCREENS_OP_FORM_LIST_SELECTED, EE_SCREENS_OP_SWAP_FORM, EE_SCREENS_TEXT_FORM_EDIT,
};
use crate::scenario_screens::{
    EE_SCREENS_OP_ADD_TRIGGER, EE_SCREENS_OP_LABEL_CLEAR, EE_SCREENS_OP_LABEL_LINE_COUNT,
    EE_SCREENS_OP_LABEL_START_LINE, EE_SCREENS_OP_REMOVE_BACKGROUND, EE_SCREENS_OP_TAB_STOP,
};
use crate::settings_form::*;
use crate::splash::EE_SCREENS_OP_SS_BAR_VALUE;

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
    /// `kPostGameLineGraph` (ee/EEUIManager.h): the summary's line graph, control 0 of its form.
    const TK_PGS_LINE_GRAPH: u32 = 0;
    /// The summary's combined graph and its two parts (`GAME_PGS_COMBINE`).
    const TK_PGS_COMBINED: u32 = 0x17;
    const TK_PGS_COMBINE_FIRST: u32 = 0x0d;
    const TK_PGS_COMBINE_SECOND: u32 = 0x0e;
}

/// `kKeyNone` (lle/ISKeyCode.h): no key, no modifier.
const KEY_NONE: u64 = 0x7fff;
/// `kUIEventFormOpen`, `kUIEventFormClose`, `kUIEventMouseMove`, the keyboard-captured event: `UIForm::SetEvent`'s
/// indices.
const FORM_EVENT_OPEN: isize = 0;
const FORM_EVENT_CLOSE: isize = 1;
const FORM_EVENT_MOUSE_MOVE: isize = 11;
const FORM_EVENT_KEYBOARD: isize = 16;

// ---- the screens' events (`EEScreenEvent`s: C++ objects, made and released by the host's one helper)

/// cbindgen:ignore
#[cfg(not(test))]
mod sys {
    use core::ffi::c_void;
    extern "C" {
        /// `op` 0: a new `EEScreenEvent(code)` (`EECreateScreenEvent`: NULL for code 0); 1 the event's `Reference()`;
        /// 2 its `Dereference()` (UI Events/EEScreens.cpp).
        pub fn ee_screenscpp_event(op: u32, event: *mut c_void, code: u32) -> *mut c_void;
    }
}
#[cfg(not(test))]
fn event_op(op: u32, event: Cpp, code: u32) -> Cpp {
    // SAFETY: the host's helper; `event` is one it made (or NULL for a new one)
    unsafe { sys::ee_screenscpp_event(op, event, code) }
}
#[cfg(test)]
fn event_op(op: u32, event: Cpp, code: u32) -> Cpp {
    tests::event_op(op, event, code)
}

/// `EECreateScreenEvent(code)`: a new event, NULL for code 0.
fn new_event(code: u32) -> Cpp {
    event_op(0, core::ptr::null_mut(), code)
}
fn reference(event: Cpp) {
    event_op(1, event, 0);
}
fn dereference(event: Cpp) {
    event_op(2, event, 0);
}

/// `UIForm::AddHotKey(key, m1, m2, m3, just pressed, pressed, just unpressed, double pressed, 0, false)` (`UIForm.cpp`'s
/// three calls; the keys packed 16 bits each, a code past 0xffff as 0xffff). The form keeps the events' references.
fn add_hot_key(form: Cpp, keys: [u64; 4], events: [Cpp; 4]) {
    let mut packed = 0u64;
    for (i, k) in keys.iter().enumerate() {
        packed |= (*k).min(0xffff) << (16 * i);
    }
    direct::form_call(form, EeUiFormCall::AddHotKeyEvents, events[0] as isize, events[1] as isize);
    direct::form_call(form, EeUiFormCall::AddHotKeyEvents2, events[2] as isize, events[3] as isize);
    direct::form_call(form, EeUiFormCall::AddHotKeyBuild, packed as isize, 0);
}

/// A new event set as one of the form's events (`SetEvent(index, event)`), then released: the form holds it.
fn set_form_event(form: Cpp, index: isize, code: u32) {
    let event = new_event(code);
    direct::form_call(form, EeUiFormCall::SetEvent, index, event as isize);
    if !event.is_null() {
        dereference(event);
    }
}

/// The camera grabs' saved mouse move events (`sGrabSaved`: the main game form's, the editor's).
static GRAB_SAVED: Mutex<[usize; 2]> = Mutex::new([0; 2]);

/// `EEReleaseGrabScroll`'s first step: the mouse move event the form's grab saved, released.
#[cfg_attr(test, allow(dead_code))]
pub(crate) fn release_grab(form: u32) {
    let saved = GRAB_SAVED.lock().unwrap_or_else(|e| e.into_inner()).get_mut(form as usize).map(core::mem::take);
    if let Some(saved) = saved.filter(|&s| s != 0) {
        dereference(saved as Cpp);
    }
}

/// The event group: the ops that make an event and hand it to a form, a control, a hot key or the manager.
fn event_group(form: Cpp, op: u32, a: u32, b: u32, c: u32, d: u32) -> Option<u32> {
    match op {
        EE_SCREENS_OP_OPEN_EVENT | EE_SCREENS_OP_CLOSE_EVENT | EE_SCREENS_OP_TRIGGER => {
            let event = new_event(if op == EE_SCREENS_OP_TRIGGER { d } else { b });
            let target = form_by_id(form, a);
            match op {
                EE_SCREENS_OP_OPEN_EVENT => {
                    direct::form_call(target, EeUiFormCall::SetEvent, FORM_EVENT_OPEN, event as isize);
                }
                EE_SCREENS_OP_CLOSE_EVENT => {
                    direct::form_call(target, EeUiFormCall::SetEvent, FORM_EVENT_CLOSE, event as isize);
                }
                _ => {
                    direct::control_call(ctl(target, b), EeUiControlCall::SetTrigger, c as isize, event as isize);
                }
            }
            if !event.is_null() {
                dereference(event);
            }
        }
        EE_SCREENS_OP_TRIGGER_HERE => {
            let event = new_event(c);
            direct::control_call(ctl(form, a), EeUiControlCall::SetTrigger, b as isize, event as isize);
            if !event.is_null() {
                dereference(event);
            }
        }
        EE_SCREENS_OP_HOT_KEY => {
            // the settings' hot keys: the event fires on the key's release
            let event = new_event(d);
            let none = core::ptr::null_mut();
            add_hot_key(form, [a as u64, b as u64, c as u64, KEY_NONE], [none, none, event, none]);
        }
        EE_SCREENS_OP_CH_HOT_KEY => {
            let event = new_event(c);
            let none = core::ptr::null_mut();
            add_hot_key(form, [a as u64, b as u64, KEY_NONE, KEY_NONE], [event, none, none, none]);
        }
        EE_SCREENS_OP_GO_HOT_KEY => {
            let event = new_event(b);
            let none = core::ptr::null_mut();
            add_hot_key(form, [a as u64, KEY_NONE, KEY_NONE, KEY_NONE], [event, none, none, none]);
        }
        EE_SCREENS_OP_CH_KEYBOARD_EVENT | EE_SCREENS_OP_LE_KEYBOARD_EVENT => {
            set_form_event(form, FORM_EVENT_KEYBOARD, a)
        }
        EE_SCREENS_OP_CAM_GRAB_BEGIN => {
            // the form's mouse move event saved; the grab its mouse move event while the button is down
            let saved = direct::form_call(form, EeUiFormCall::Event, FORM_EVENT_MOUSE_MOVE, 0) as Cpp;
            if let Some(slot) = GRAB_SAVED.lock().unwrap_or_else(|e| e.into_inner()).get_mut(a as usize) {
                *slot = saved as usize;
            }
            if !saved.is_null() {
                reference(saved);
            }
            set_form_event(
                form,
                FORM_EVENT_MOUSE_MOVE,
                if a != 0 { EE_SCREENS_CAM_GRAB_EDITOR } else { EE_SCREENS_CAM_GRAB_MAIN },
            );
        }
        EE_SCREENS_OP_CAM_GRAB_END => {
            let saved =
                GRAB_SAVED.lock().unwrap_or_else(|e| e.into_inner()).get(a as usize).copied().unwrap_or(0) as Cpp;
            direct::form_call(form, EeUiFormCall::SetEvent, FORM_EVENT_MOUSE_MOVE, saved as isize);
            if !saved.is_null() {
                dereference(saved);
                if let Some(slot) = GRAB_SAVED.lock().unwrap_or_else(|e| e.into_inner()).get_mut(a as usize) {
                    *slot = 0;
                }
            }
        }
        EE_SCREENS_OP_ADD_TRIGGER => {
            let event = new_event(a);
            direct::manager_call(mgr(form), EeUiManagerCall::AddEventToTrigger, event as isize, form as isize, 0);
            if !event.is_null() {
                dereference(event);
            }
        }
        _ => return None,
    }
    Some(0)
}

// ---- the list boxes' and combo boxes' lines (the list's per-type host: `UIListBoxRust<T>::Op`)

/// `kErrorOK`.
const ERROR_OK: isize = 0;

fn list_op(list: Cpp, op: EeUiOp, a: isize, b: isize, c: isize) -> isize {
    direct::list_op(list, op, a, b, c).map_or(0, |r| r.0)
}

/// `GetCurrentSelection`: its result and the selection (`None` for a control that is not a list box).
fn list_selection(list: Cpp) -> Option<(isize, u32)> {
    direct::list_op(list, EeUiOp::ListSelection, 0, 0, 0).map(|(r, f)| (r, f[0].to_bits()))
}

/// The virtual `ChangeSelectionIndex(index, fire, scroll)`.
fn list_select(list: Cpp, index: u32, fire: bool, scroll: bool) {
    list_op(list, EeUiOp::ListChangeSelection, index as isize, fire as isize, scroll as isize);
}

/// A combo box's `AppendLine`: the box dirtied, the line appended to its list, an open list reopened.
fn combo_append(combo: Cpp, text: &[u8]) {
    virt(combo, EeUiOp::SetDirty, 1);
    list_op(direct::combo_list(combo), EeUiOp::ListAppendText, text.as_ptr() as isize, 0, 0);
    direct::combo_call(combo, EeUiComboCall::Reopen, 0, 0);
}

fn floats(a: u32, b: u32, c: u32) -> [f32; 4] {
    [f32::from_bits(a), f32::from_bits(b), f32::from_bits(c), 0.0]
}

fn bar(control: Cpp, call: EeUiStatusBarCall, a: isize, b: isize, f: [f32; 4]) {
    direct::status_call(control, call, a, b, &mut { f });
}

/// The controls' plain ops of the post-game summary, the splash screen and the diplomacy form: rectangles,
/// colours, opacities, the status bars.
fn control_ops(form: Cpp, op: u32, a: u32, b: u32, c: u32, d: u32) -> Option<u32> {
    let r = match op {
        EE_SCREENS_OP_PGS_RECT => {
            let r = direct::with_fields(ctl(form, a), |r| r.mRectangle).unwrap_or_default();
            [r.left, r.top, r.right, r.bottom][(b & 3) as usize].to_bits()
        }
        EE_SCREENS_OP_PGS_BG_OPACITY => {
            direct::control_virtual(ctl(form, a), EeUiOp::SetBgOpacity, 0, 0, &mut floats(b, 0, 0));
            0
        }
        EE_SCREENS_OP_PGS_GET_BG_OPACITY => {
            let mut f = [0.0f32; 4];
            direct::control_virtual(ctl(form, a), EeUiOp::BgOpacity, 0, 0, &mut f);
            f[0].to_bits()
        }
        EE_SCREENS_OP_PGS_DIRTY => {
            virt(ctl(form, a), EeUiOp::SetDirty, 1);
            0
        }
        EE_SCREENS_OP_PGS_BG_COLOUR | EE_SCREENS_OP_DS_BG => {
            direct::control_virtual(ctl(form, a), EeUiOp::PartSetBgColor, 0, 0, &mut floats(b, c, d));
            0
        }
        EE_SCREENS_OP_PGS_DESIGNED => {
            let call = if a != 0 { EeUiFormCall::DesignedWidth } else { EeUiFormCall::DesignedHeight };
            direct::form_call(form, call, 0, 0) as u32
        }
        EE_SCREENS_OP_PGS_IS_BAR => !direct::find(Kind::StatusBar, ctl(form, a)).is_null() as u32,
        EE_SCREENS_OP_PGS_EXISTS => !ctl(form, a).is_null() as u32,
        EE_SCREENS_OP_PGS_INDEX => direct::with_fields(ctl(form, a), |r| r.mIndex as u32).unwrap_or(0),
        EE_SCREENS_OP_PGS_BAR_OPACITY => {
            bar(ctl(form, a), EeUiStatusBarCall::SetStatusBarOpacity, 0, 0, floats(b, 0, 0));
            0
        }
        EE_SCREENS_OP_PGS_BAR_RANGE => {
            let x = ctl(form, a);
            bar(x, EeUiStatusBarCall::SetRange, 0, b as isize, [0.0; 4]);
            bar(x, EeUiStatusBarCall::SetCurrentRangeValue, c as i32 as isize, 0, [0.0; 4]);
            0
        }
        EE_SCREENS_OP_PGS_BAR_COLOUR => {
            bar(ctl(form, a), EeUiStatusBarCall::SetStatusBarColor, 0, 0, floats(b, c, d));
            0
        }
        EE_SCREENS_OP_SS_BAR_VALUE => {
            bar(ctl(form, a), EeUiStatusBarCall::SetCurrentRangeValue, b as i32 as isize, 0, [0.0; 4]);
            0
        }
        _ => return None,
    };
    Some(r)
}

/// The list and combo ops.
fn list_ops(form: Cpp, op: u32, a: u32, b: u32, c: u32) -> Option<u32> {
    let r = match op {
        EE_SCREENS_OP_LIST_SELECT => {
            list_select(ctl(form, a), b, false, true);
            0
        }
        EE_SCREENS_OP_LIST_CLEAR => {
            list_op(ctl(form, a), EeUiOp::ListDeleteAll, 0, 0, 0);
            0
        }
        EE_SCREENS_OP_CE_LIST_SELECT => {
            list_select(ctl(form, a), b, false, c != 0);
            0
        }
        EE_SCREENS_OP_CE_CLEAR_SELECTION => {
            list_op(ctl(form, a), EeUiOp::ListClearSelection, 0, 0, 0);
            0
        }
        EE_SCREENS_OP_CE_COMBO_SELECTION => {
            // the combo box's own selection (`GetCurrentSelection`: kErrorOK unless it is -1)
            let s = direct::combo_call(ctl(form, a), EeUiComboCall::Selection, 0, 0).unwrap_or(-1) as u32;
            if s != u32::MAX {
                s
            } else {
                0xffff_ffff
            }
        }
        EE_SCREENS_OP_CE_COMBO_COUNT | EE_SCREENS_OP_LE_COMBO_LINES => {
            list_op(direct::combo_list(ctl(form, a)), EeUiOp::ListLineCount, 0, 0, 0) as u32
        }
        EE_SCREENS_OP_LE_COMBO_DELETE_LINE => {
            direct::combo_call(ctl(form, a), EeUiComboCall::DeleteLine, b as isize, 0);
            0
        }
        EE_SCREENS_OP_FORM_LIST_CLEAR => {
            list_op(ctl(form_by_id(form, a), b), EeUiOp::ListDeleteAll, 0, 0, 0);
            0
        }
        EE_SCREENS_OP_FORM_LIST_COUNT => list_op(ctl(form_by_id(form, a), b), EeUiOp::ListLineCount, 0, 0, 0) as u32,
        EE_SCREENS_OP_FORM_LIST_SELECT => {
            list_select(ctl(form_by_id(form, a), b), c, true, false);
            0
        }
        EE_SCREENS_OP_FORM_LIST_SELECTION => list_selection(ctl(form_by_id(form, a), b)).map_or(0, |s| s.1),
        EE_SCREENS_OP_FORM_LIST_SELECTED => {
            let target = if a != 0 { form_by_id(form, a - 1) } else { form };
            match list_selection(ctl(target, b)) {
                Some((ERROR_OK, s)) => s,
                _ => 0xffff_ffff,
            }
        }
        EE_SCREENS_OP_PDCS_HAS_SELECTION => {
            (direct::list_call(ctl(form, a), EeUiListCall::HasSelection, 0, 0).unwrap_or(0) != 0) as u32
        }
        _ => return None,
    };
    Some(r)
}

/// The list texts: a list box's or a combo box's line, by the host's numbering.
fn list_text(form: Cpp, which: u32, arg: u32) -> Option<Vec<u8>> {
    let (list, line) = match which {
        EE_SCREENS_TEXT_LIST_DATA => (ctl(form, arg >> 16), arg & 0xffff),
        EE_SCREENS_TEXT_COMBO_DATA => (direct::combo_list(ctl(form, arg >> 16)), arg & 0xffff),
        EE_SCREENS_TEXT_FORM_LIST_DATA => {
            (ctl(form_by_id(core::ptr::null_mut(), arg >> 24), (arg >> 16) & 0xff), arg & 0xffff)
        }
        _ => return None,
    };
    Some(direct::list_line(list, line as isize).unwrap_or_default())
}

/// The summary's series register (`BuildGraphs`' series: `PGS_SERIES_CLEAR`, `_POINT`, `_ADD`).
static PGS_SERIES: Mutex<Option<EeUtilSeries>> = Mutex::new(None);

fn pgs_series<R>(f: impl FnOnce(&mut EeUtilSeries) -> R) -> R {
    let mut s = PGS_SERIES.lock().unwrap_or_else(|e| e.into_inner());
    f(s.get_or_insert_with(EeUtilSeries::default))
}

/// The summary's graph `index` (0..30 its line graph's stored ones, 31 the shown one, 32..39 its form manager's)
/// handed to `f`; `None` without the line graph or the graph.
fn pgs_graph<R>(form: Cpp, index: u32, f: impl FnOnce(&mut UiGraph) -> R) -> Option<R> {
    direct::with_graph(ctl(form, K::TK_PGS_LINE_GRAPH), |g| g.graph_at(index, f)).flatten()
}

/// The colours of the statistics' records (`gWorld.mStatistics.mRecords[i].mColor`; the first is no player's).
#[cfg(not(test))]
fn pgs_record_colours() -> Vec<[f32; 3]> {
    crate::post_game_ffi::record_colours()
}
#[cfg(test)]
fn pgs_record_colours() -> Vec<[f32; 3]> {
    tests::RECORD_COLOURS.with(|c| c.borrow().clone())
}

/// `GAME_PGS_COMBINE`: graph 0x17's series become those of 0x0e combined with 0x0d (`CombineGraph`), the first
/// `records - 1` of them, each in its record's colour (records 1..); its title and labels stay.
fn pgs_combine(form: Cpp) {
    let Some(first) = pgs_graph(form, K::TK_PGS_COMBINE_FIRST, |g| g.data.clone()) else {
        return;
    };
    let Some(second) = pgs_graph(form, K::TK_PGS_COMBINE_SECOND, |g| g.data.clone()) else {
        return;
    };
    let mut both = EeUtilGraph::default();
    second.combine(&first, &mut both);
    let colours = pgs_record_colours();
    let mut combined = EeUtilGraph::default();
    for (i, c) in colours.iter().enumerate().skip(1) {
        if i > both.len() {
            break;
        }
        let mut series = both.series(i - 1).clone();
        series.set_color(EeUtilColor { red: c[0], green: c[1], blue: c[2] });
        combined.add_series(&series);
    }
    pgs_graph(form, K::TK_PGS_COMBINED, |g| g.data = combined);
}

/// The series `series` of the summary's graph `graph` handed to `f`; `None` without it.
fn pgs_series_of<R>(form: Cpp, graph: u32, series: u32, f: impl FnOnce(&EeUtilSeries) -> R) -> Option<R> {
    pgs_graph(form, graph, |g| {
        if (series as usize) < g.data.len() {
            Some(f(g.data.series(series as usize)))
        } else {
            None
        }
    })
    .flatten()
}

/// The summary's graph ops (`EE_SCREENS_OP_PGS_*` on its line graph's graphs, `GAME_PGS_COMBINE`).
fn pgs_op(form: Cpp, op: u32, a: u32, b: u32, c: u32, d: u32) -> Option<u32> {
    let r = match op {
        EE_SCREENS_OP_PGS_SERIES_ENABLED => {
            pgs_graph(form, a, |g| {
                if (b as usize) < g.data.len() {
                    g.data.set_enabled(b as usize, c != 0);
                }
            });
            0
        }
        EE_SCREENS_OP_PGS_SHOW_GRAPH => {
            direct::with_graph(ctl(form, K::TK_PGS_LINE_GRAPH), |g| g.show_graph(a));
            0
        }
        EE_SCREENS_OP_PGS_CLEAR_GRAPH => {
            pgs_graph(form, a, |g| g.data.clear());
            0
        }
        EE_SCREENS_OP_PGS_PRUNE => {
            pgs_graph(form, a, |g| g.data.prune());
            0
        }
        EE_SCREENS_OP_PGS_SERIES_CLEAR => {
            pgs_series(|s| s.clear());
            0
        }
        EE_SCREENS_OP_PGS_SERIES_POINT => {
            pgs_series(|s| s.add_point(EeUtilPoint { x: a as i32 as c_long, y: b as i32 as c_long }));
            0
        }
        EE_SCREENS_OP_PGS_SERIES_ADD => {
            let series = pgs_series(|s| s.clone());
            pgs_graph(form, a, |g| g.data.add_series(&series));
            0
        }
        EE_SCREENS_GAME_PGS_COMBINE => {
            pgs_combine(form);
            0
        }
        EE_SCREENS_OP_PGS_AXES => {
            direct::with_graph(ctl(form, K::TK_PGS_LINE_GRAPH), |g| {
                g.set_label_y(false);
                g.set_label_x(true);
            });
            0
        }
        EE_SCREENS_OP_PGS_MAX_Y => pgs_graph(form, a, |g| g.data.range().max_y as u32).unwrap_or(0),
        EE_SCREENS_OP_PGS_MIN_Y => pgs_graph(form, a, |g| g.data.range().min_y as u32).unwrap_or(0),
        EE_SCREENS_OP_PGS_SERIES_MAX_Y => pgs_series_of(form, a, b, |s| s.range().max_y as u32).unwrap_or(0),
        EE_SCREENS_OP_PGS_SERIES_COUNT => pgs_graph(form, a, |g| g.data.len() as u32).unwrap_or(0),
        EE_SCREENS_OP_PGS_POINT_COUNT => pgs_series_of(form, a, b, |s| s.points().len() as u32).unwrap_or(0),
        EE_SCREENS_OP_PGS_POINT => pgs_series_of(form, a, b, |s| {
            s.points().get(c as usize).map_or(0, |p| if d != 0 { p.y as u32 } else { p.x as u32 })
        })
        .unwrap_or(0),
        EE_SCREENS_GAME_PGS_NEW_MANAGER => {
            // the new form manager came with empty graphs, and the series register was reset: the graphs are the
            // line graph's now; the C++ still makes the manager (None: the host's case runs)
            pgs_series(|s| *s = EeUtilSeries::default());
            direct::with_graph(ctl(form, K::TK_PGS_LINE_GRAPH), |g| g.reset_manager_graphs());
            return None;
        }
        _ => return None,
    };
    Some(r)
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
pub(crate) fn op(form: *mut c_void, entity: *mut c_void, op: u32, a: u32, b: u32, c: u32, d: u32) -> Option<u32> {
    direct::check_op("op", op);
    if let Some(r) = pgs_op(form, op, a, b, c, d) {
        return Some(r);
    }
    if let Some(r) = event_group(form, op, a, b, c, d) {
        return Some(r);
    }
    if let Some(r) = list_ops(form, op, a, b, c) {
        return Some(r);
    }
    if let Some(r) = control_ops(form, op, a, b, c, d) {
        return Some(r);
    }
    if op == EE_SCREENS_GAME_PGS_NEW_MANAGER {
        return None;
    }
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
    direct::check_op("text", which);
    if let Some(t) = list_text(form, which, arg) {
        return Some(t);
    }
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
    direct::check_op("put", which);
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
        // the list boxes' and combo boxes' appends
        EE_SCREENS_TEXT_COMBO_APPEND => {
            let combo = ctl(form, arg);
            if !combo.is_null() {
                combo_append(combo, &t);
            }
        }
        EE_SCREENS_TEXT_COMBO_INSERT => {
            direct::combo_call(ctl(form, arg >> 16), EeUiComboCall::InsertLine, p, (arg & 0xffff) as isize);
        }
        EE_SCREENS_TEXT_LIST_APPEND => {
            list_op(ctl(form, arg), EeUiOp::ListAppendText, p, 0, 0);
        }
        EE_SCREENS_TEXT_FORM_LIST_APPEND => {
            let target = form_by_id(core::ptr::null_mut(), arg >> 16);
            list_op(ctl(target, arg & 0xffff), EeUiOp::ListAppendText, p, 0, 0);
        }
        // the post-game summary's graph `arg`'s title and labels
        EE_SCREENS_TEXT_PGS_TITLE | EE_SCREENS_TEXT_PGS_Y_LABEL | EE_SCREENS_TEXT_PGS_X_LABEL => {
            let text = t[..t.len() - 1].to_vec();
            pgs_graph(form, arg, |g| match which {
                EE_SCREENS_TEXT_PGS_TITLE => g.title = text,
                EE_SCREENS_TEXT_PGS_Y_LABEL => g.y_label = text,
                _ => g.x_label = text,
            });
        }
        _ => return None,
    }
    Some(0)
}

#[cfg(test)]
#[path = "toolkit_tests.rs"]
mod tests;
