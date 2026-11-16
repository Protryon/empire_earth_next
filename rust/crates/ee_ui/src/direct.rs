//! The direct path from the game's Rust to the toolkit's objects (R6 game screens B, round 2).
//!
//! Every toolkit class the game's screens call is a C++ shell over a Rust object (`mRust`, or the record its base
//! is). The screens' Rust (`ee_screens`) holds only the C++ pointers the toolkit hands out (a form, a control, a
//! group, a background), and used to make one C++ hop per call (`EEScreens.cpp`'s host cases) for the C++ forwarder
//! to reach the Rust object again. This module closes the loop: each Rust object's constructor records its C++
//! object here ([`put`], the destructor [`forget`]s it), so a Rust caller turns a C++ pointer into the Rust object
//! and makes the same inward call the C++ forwarder makes (`ee_ui_*_call` with the same arguments).
//!
//! The virtuals a game class may override (`SetDisplayMode`, `SetDirty`, `ReleaseCapturedMouse`, `SetBgOpacity`,
//! `TabbedTo`) still go through the C++ object: [`control_virtual`] is an op on the control record's host
//! (`UIControl::HostOp`), which makes the virtual call, so an override runs.
//!
//! Lookups hold the registry's lock only for the lookup: no lock is held across a call into an object (the calls
//! out re-enter, and may make or destroy controls).

use core::ffi::{c_ulong, c_void};
use std::collections::BTreeMap;
use std::sync::Mutex;

use crate::bgobj::{ee_ui_bg_call, EeUiBackgroundFields, EeUiBgCall};
use crate::buttonobj::{ee_ui_button_call, EeUiButton, EeUiButtonCall};
use crate::comboobj::{ee_ui_combo_call, EeUiCombo, EeUiComboCall};
use crate::controlobj::{ee_ui_control_call, EeUiControlCall, EeUiControlFields};
use crate::editobj::{ee_ui_edit_call, EeUiEdit, EeUiEditCall};
use crate::formobj::{ee_ui_form_call, EeUiForm, EeUiFormCall};
use crate::graphobj::EeUiGraph;
use crate::groupobj::{ee_ui_entity_group_call, EeUiEntityGroup, EeUiEntityGroupCall};
use crate::labelobj::{ee_ui_label_call, EeUiLabel, EeUiLabelCall};
use crate::listobj::{ee_ui_list_call, EeUiList, EeUiListCall};
use crate::managerobj::{ee_ui_manager_call, EeUiManager, EeUiManagerCall};
use crate::obj::EeUiOp;
use crate::sliderobj::{ee_ui_slider_call, EeUiSlider, EeUiSliderCall};
use crate::statusobj::{ee_ui_status_bar_call, EeUiStatusBar, EeUiStatusBarCall};

/// What a registered Rust object is.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub enum Kind {
    /// `UIForm`'s [`EeUiForm`].
    Form,
    /// `UIManager`'s [`EeUiManager`].
    Manager,
    /// `UIEntityGroup`'s [`EeUiEntityGroup`].
    Group,
    /// `UIBackground`'s base record ([`EeUiBackgroundFields`]).
    Background,
    /// `UIControl`'s base record ([`EeUiControlFields`]).
    Control,
    /// `UIButton`'s [`EeUiButton`] (and its subclasses': check box, radio button, blinking button...).
    Button,
    /// `UIComboBox`'s [`EeUiCombo`].
    Combo,
    /// `UITextLabel`'s [`EeUiLabel`].
    Label,
    /// `UIEditBox`'s [`EeUiEdit`].
    Edit,
    /// `UISlider`'s [`EeUiSlider`].
    Slider,
    /// `UIGraph`'s [`EeUiGraph`] (the line graph's and the post-game summary's).
    Graph,
    /// `UIListBox<T>`'s [`EeUiList`] (every instantiation; the lines are the C++'s, reached through its host).
    List,
    /// `UIStatusBar`'s [`EeUiStatusBar`].
    StatusBar,
}

/// The C++ pointer of a toolkit object (`UIForm*`, `UIControl*`, ...).
pub type Cpp = *mut c_void;

struct Registry {
    /// (C++ object, kind) -> the Rust object.
    by_cpp: BTreeMap<(usize, Kind), usize>,
    /// (Rust object, kind) -> the C++ object.
    by_obj: BTreeMap<(usize, Kind), usize>,
    /// The last manager made (the game makes one: `gUIManager`), for a call with no form.
    manager: usize,
}

static REGISTRY: Mutex<Registry> =
    Mutex::new(Registry { by_cpp: BTreeMap::new(), by_obj: BTreeMap::new(), manager: 0 });

fn registry() -> std::sync::MutexGuard<'static, Registry> {
    REGISTRY.lock().unwrap_or_else(|e| e.into_inner())
}

/// Records that the C++ object `cpp` has the Rust object `obj` of `kind` (its constructor's step).
pub fn put(kind: Kind, cpp: *const c_void, obj: *const c_void) {
    if cpp.is_null() || obj.is_null() {
        return;
    }
    let mut r = registry();
    if let Some(old) = r.by_cpp.insert((cpp as usize, kind), obj as usize) {
        r.by_obj.remove(&(old, kind));
    }
    r.by_obj.insert((obj as usize, kind), cpp as usize);
    if kind == Kind::Manager {
        r.manager = cpp as usize;
    }
}

/// A constructor's new object `obj`, recorded for its C++ object `cpp` ([`put`]) and handed back.
pub fn made<T>(kind: Kind, cpp: *mut c_void, obj: *mut T) -> *mut T {
    put(kind, cpp, obj as *const c_void);
    obj
}

/// Drops the Rust object `obj` of `kind` from the registry (its destructor's step; nothing when unknown).
pub fn forget(kind: Kind, obj: *const c_void) {
    let mut r = registry();
    let Some(cpp) = r.by_obj.remove(&(obj as usize, kind)) else {
        return;
    };
    if r.by_cpp.get(&(cpp, kind)) == Some(&(obj as usize)) {
        r.by_cpp.remove(&(cpp, kind));
    }
    if kind == Kind::Manager && r.manager == cpp {
        r.manager = 0;
    }
}

/// The Rust object of `kind` the C++ object `cpp` has, or NULL.
pub fn find(kind: Kind, cpp: *const c_void) -> *mut c_void {
    if cpp.is_null() {
        return core::ptr::null_mut();
    }
    let o = registry().by_cpp.get(&(cpp as usize, kind)).map_or(core::ptr::null_mut(), |&o| o as *mut c_void);
    check(kind, cpp, !o.is_null());
    o
}

// ---- the lookup check (round 3): `EE_DIRECT_CHECK=<file>` logs every lookup of a C++ object, found or missing,
// under the screens' op that made it ([`check_op`]); a missing non-NULL object of the kind an op expects is the
// bug class of round 2's backgrounds (a registry keyed by another pointer)

static CHECK: std::sync::OnceLock<Option<Mutex<std::fs::File>>> = std::sync::OnceLock::new();

fn check_file() -> Option<&'static Mutex<std::fs::File>> {
    CHECK
        .get_or_init(|| std::env::var_os("EE_DIRECT_CHECK").and_then(|p| std::fs::File::create(p).ok()).map(Mutex::new))
        .as_ref()
}

fn check_line(line: &str) {
    if let Some(f) = check_file() {
        use std::io::Write;
        let _ = writeln!(f.lock().unwrap_or_else(|e| e.into_inner()), "{line}");
    }
}

fn check(kind: Kind, cpp: *const c_void, found: bool) {
    if check_file().is_some() {
        check_line(&format!("{} {kind:?} {:#x}", if found { "found" } else { "MISSING" }, cpp as usize));
    }
}

/// The lookup check's op line: the screens' op `op` (an `EE_SCREENS_*` number) on `form` starts.
pub fn check_op(what: &str, op: u32) {
    if check_file().is_some() {
        check_line(&format!("{what} {op:#x}"));
    }
}

/// `UIBackground`'s constructor's step: the background `cpp` (the `UIBackground*` the forms hold; its record's own
/// host object is the record) has the record `rec`.
///
/// # Safety
/// `rec` is the live record of `cpp`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_direct_background(rec: *mut EeUiBackgroundFields, cpp: *mut c_void) {
    put(Kind::Background, cpp, rec as *const c_void);
}

/// The manager a call on `form` uses: the form's (`GetManager()`), the game's for no form.
pub fn manager_of(form: Cpp) -> Cpp {
    if form.is_null() {
        return registry().manager as Cpp;
    }
    form_call(form, EeUiFormCall::Manager, 0, 0) as Cpp
}

fn buf() -> [f32; 4] {
    [0.0; 4]
}

// ---- the inward calls, by C++ pointer (0 / nothing for an unknown object, where the C++ would have crashed)

/// One of `UIManager`'s methods on the manager `manager`.
pub fn manager_call(manager: Cpp, call: EeUiManagerCall, a: isize, b: isize, c: isize) -> isize {
    let m = find(Kind::Manager, manager) as *const EeUiManager;
    if m.is_null() {
        return 0;
    }
    let mut f = buf();
    // SAFETY: a live manager (registered between its construction and destruction)
    unsafe { ee_ui_manager_call(m, call, a, b, c, f.as_mut_ptr()) }
}

/// The manager's `GetFormByID(id)`.
pub fn form_by_id(manager: Cpp, id: c_ulong) -> Cpp {
    manager_call(manager, EeUiManagerCall::FormById, id as isize, 0, 0) as Cpp
}

/// One of `UIForm`'s methods on `form`, with the float buffer `f`.
pub fn form_call_f(form: Cpp, call: EeUiFormCall, a: isize, b: isize, f: &mut [f32; 4]) -> isize {
    let o = find(Kind::Form, form) as *const EeUiForm;
    if o.is_null() {
        return 0;
    }
    // SAFETY: a live form
    unsafe { ee_ui_form_call(o, call, a, b, f.as_mut_ptr()) }
}

/// One of `UIForm`'s methods on `form`.
pub fn form_call(form: Cpp, call: EeUiFormCall, a: isize, b: isize) -> isize {
    form_call_f(form, call, a, b, &mut buf())
}

/// The form's `GetControl(index)`.
pub fn control(form: Cpp, index: c_ulong) -> Cpp {
    form_call(form, EeUiFormCall::Control, index as isize, 0) as Cpp
}

/// The control's record, or NULL.
pub fn fields(control: Cpp) -> *mut EeUiControlFields {
    find(Kind::Control, control) as *mut EeUiControlFields
}

/// One of `UIControl`'s base methods on `control`.
pub fn control_call(control: Cpp, call: EeUiControlCall, a: isize, b: isize) -> isize {
    let r = fields(control);
    if r.is_null() {
        return 0;
    }
    let mut f = buf();
    // SAFETY: a live control record
    unsafe { ee_ui_control_call(r, call, a, b, f.as_mut_ptr()) }
}

/// An op on the control's C++ object through its record's host (`UIControl::HostOp`): the virtuals (`SetDirty`,
/// `SetDisplayMode`, `ReleaseMouse`, `SetBgOpacity`, `CtlTabbedTo`) and the event references (`FormEventCall`).
pub fn control_virtual(control: Cpp, op: EeUiOp, a: isize, b: isize, f: &mut [f32; 4]) -> isize {
    let r = fields(control);
    if r.is_null() {
        return 0;
    }
    // SAFETY: a live control record; the host is read before the call (it may re-enter)
    let (host, cpp) = unsafe { ((*r).mHost, (*r).mCpp) };
    match host {
        // SAFETY: the record's host and its object
        Some(h) => unsafe { h(cpp, op, a, b, 0, 0, f.as_mut_ptr()) },
        None => 0,
    }
}

/// A field of the control's record (`GetId()`, `GetIndex()`...), read afresh.
pub fn with_fields<R>(control: Cpp, read: impl FnOnce(&mut EeUiControlFields) -> R) -> Option<R> {
    let r = fields(control);
    if r.is_null() {
        return None;
    }
    // SAFETY: a live control record; no call out while the reference lives
    Some(read(unsafe { &mut *r }))
}

macro_rules! sub_call {
    ($(#[$m:meta])* $name:ident, $kind:expr, $ty:ty, $call:ty, $f:ident) => {
        $(#[$m])*
        pub fn $name(control: Cpp, call: $call, a: isize, b: isize) -> Option<isize> {
            let o = find($kind, control) as *const $ty;
            if o.is_null() {
                return None;
            }
            let mut f = buf();
            // SAFETY: a live object of the kind
            Some(unsafe { $f(o, call, a, b, f.as_mut_ptr()) })
        }
    };
}

sub_call!(
    /// One of `UIButton`'s (or a subclass's) methods; `None` when `control` is not a button.
    button_call, Kind::Button, EeUiButton, EeUiButtonCall, ee_ui_button_call
);
sub_call!(
    /// One of `UIComboBox`'s methods; `None` when `control` is not a combo box.
    combo_call, Kind::Combo, EeUiCombo, EeUiComboCall, ee_ui_combo_call
);
sub_call!(
    /// One of `UITextLabel`'s methods; `None` when `control` is not a label.
    label_call, Kind::Label, EeUiLabel, EeUiLabelCall, ee_ui_label_call
);
sub_call!(
    /// One of `UISlider`'s methods; `None` when `control` is not a slider.
    slider_call, Kind::Slider, EeUiSlider, EeUiSliderCall, ee_ui_slider_call
);

/// One of `UIStatusBar`'s methods with the float buffer `f`; `None` when `control` is not a status bar.
pub fn status_call(control: Cpp, call: EeUiStatusBarCall, a: isize, b: isize, f: &mut [f32; 4]) -> Option<isize> {
    let o = find(Kind::StatusBar, control) as *const EeUiStatusBar;
    if o.is_null() {
        return None;
    }
    // SAFETY: a live status bar
    Some(unsafe { ee_ui_status_bar_call(o, call, a, b, f.as_mut_ptr()) })
}

/// One of `UIEditBox`'s methods; `None` when `control` is not an edit box.
pub fn edit_call(control: Cpp, call: EeUiEditCall, a: isize, b: isize) -> Option<isize> {
    let o = find(Kind::Edit, control) as *const EeUiEdit;
    if o.is_null() {
        return None;
    }
    let mut f = buf();
    // SAFETY: a live edit box
    Some(unsafe { ee_ui_edit_call(o, call, a, b, 0, 0, f.as_mut_ptr()) })
}

/// One of `UIEntityGroup`'s methods on the group `group`.
pub fn group_call(group: Cpp, call: EeUiEntityGroupCall, a: isize) -> isize {
    let o = find(Kind::Group, group) as *const EeUiEntityGroup;
    if o.is_null() {
        return 0;
    }
    // SAFETY: a live group
    unsafe { ee_ui_entity_group_call(o, call, a) }
}

/// One of `UIBackground`'s methods on the background `background`.
pub fn background_call(background: Cpp, call: EeUiBgCall, a: isize, b: isize) -> isize {
    let r = find(Kind::Background, background) as *mut EeUiBackgroundFields;
    if r.is_null() {
        return 0;
    }
    let mut f = buf();
    // SAFETY: a live background record
    unsafe { ee_ui_bg_call(r, call, a, b, f.as_mut_ptr()) }
}

/// One of `UIListBox`'s methods on the list `control`; `None` when `control` is not a list box.
pub fn list_call(control: Cpp, call: EeUiListCall, a: isize, b: isize) -> Option<isize> {
    let o = find(Kind::List, control) as *const EeUiList;
    if o.is_null() {
        return None;
    }
    let mut f = buf();
    // SAFETY: a live list box
    Some(unsafe { ee_ui_list_call(o, call, a, b, f.as_mut_ptr()) })
}

/// An op on the list `control` through its per-type host (`UIListBoxRust<T>::Op`: the lines and the virtuals); the
/// result and the float buffer; `None` when `control` is not a list box.
pub fn list_op(control: Cpp, op: EeUiOp, a: isize, b: isize, c: isize) -> Option<(isize, [f32; 4])> {
    let o = find(Kind::List, control) as *const EeUiList;
    if o.is_null() {
        return None;
    }
    // SAFETY: a live list box; its host is copied out before the call (which may re-enter)
    let host = unsafe { (*o).host };
    let mut f = buf();
    let r = host.op_f(op, a, b, c, 0, &mut f);
    Some((r, f))
}

/// The list `control`'s line `index` (`GetData(index)` of a `UIListBox<UWideString>`), `None` past the end or for
/// another control.
pub fn list_line(control: Cpp, index: isize) -> Option<Vec<u8>> {
    let (p, _) = list_op(control, EeUiOp::ListLineText, index, 0, 0)?;
    // SAFETY: the line's NUL-terminated bytes (or NULL)
    (p != 0).then(|| unsafe { c_bytes(p as *const c_void) })
}

/// The combo box `control`'s drop-down list (its `UILBString*`), NULL when `control` is not a combo box.
pub fn combo_list(control: Cpp) -> Cpp {
    let o = find(Kind::Combo, control) as *const EeUiCombo;
    if o.is_null() {
        return core::ptr::null_mut();
    }
    // SAFETY: a live combo box
    unsafe { (*o).list_cpp() as Cpp }
}

/// The graph `control` (a `UIGraph`) handed to `f`; `None` when `control` is not a graph.
pub fn with_graph<R>(control: Cpp, f: impl FnOnce(&EeUiGraph) -> R) -> Option<R> {
    let o = find(Kind::Graph, control) as *const EeUiGraph;
    if o.is_null() {
        return None;
    }
    // SAFETY: a live graph (registered between its construction and destruction); a shared reference, as the
    // inward calls take it
    Some(f(unsafe { &*o }))
}

/// `UITextLabel::SetText(long)`'s text: `PlatIToA(value, 10)` (the sign, then the decimal digits).
pub fn label_number_text(value: i32) -> Vec<u8> {
    let mut t = value.to_string().into_bytes();
    t.push(0);
    t
}

/// A text as the C++'s `UWideString(std::string(bytes).c_str())` passes it on: cut at its first NUL, NUL-ended.
pub fn c_text(text: &[u8]) -> Vec<u8> {
    let n = text.iter().position(|&b| b == 0).unwrap_or(text.len());
    let mut t = text[..n].to_vec();
    t.push(0);
    t
}

/// The NUL-terminated bytes at `p` (none for NULL).
///
/// # Safety
/// `p` is NULL or a live NUL-terminated string.
pub unsafe fn c_bytes(p: *const c_void) -> Vec<u8> {
    if p.is_null() {
        return Vec::new();
    }
    // SAFETY: the contract
    unsafe { core::ffi::CStr::from_ptr(p as *const core::ffi::c_char) }.to_bytes().to_vec()
}

#[cfg(test)]
#[path = "direct_tests.rs"]
mod tests;
