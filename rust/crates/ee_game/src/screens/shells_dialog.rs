//! The game's message box as kinds over the form and button shells (UI cluster round 4): once `EEUIFError` (a
//! `UIForm`: its constructor, `Display`, `Open` and `Close`) and its two button classes `EEUIFEBOK` and
//! `EEUIFEBCancel` (50x20, tinted; a press marks the button pressed and closes the box or its close form). The box's
//! plans and layouts are `dialog.rs`'; its controls are the kind's slots, which the box's host
//! (`ee_screenscpp_dialog`, `User Interface Thread/EEUIFError.cpp`) fills and drives; the mode is the kind's.

use core::cell::Cell;
use core::ffi::CStr;

use crate::screens::dialog::{self, DialogHost, EE_SCREENS_DLG_MODE};
use crate::screens::shells::*;

/// `EEUIFError` (a form; no args).
pub const EE_SCREENS_SK_DIALOG: u32 = 0x68;
/// `EEUIFEBOK`, `EEUIFEBCancel` (buttons; no args).
pub const EE_SCREENS_SK_DIALOG_OK: u32 = 0x69;
pub const EE_SCREENS_SK_DIALOG_CANCEL: u32 = 0x6a;

/// The box's `Display(kind a, alignment b & 0xffffffff, flag b >> 32)` (its inputs the host's).
pub const EE_SCREENS_SKOP_DIALOG_SHOW: u32 = 0x68;
/// A button's `SetCallback(a, b)` / `mCloseForm = a` / `mPressed` / `mCallback` / `mData`.
pub const EE_SCREENS_SKOP_DIALOG_SET_CALLBACK: u32 = 0x69;
pub const EE_SCREENS_SKOP_DIALOG_SET_CLOSE: u32 = 0x6a;
pub const EE_SCREENS_SKOP_DIALOG_PRESSED: u32 = 0x6b;
pub const EE_SCREENS_SKOP_DIALOG_CALLBACK: u32 = 0x6c;
pub const EE_SCREENS_SKOP_DIALOG_DATA: u32 = 0x6d;

/// A button's text focused colour `f[0..3]` (`SetFocusedTextColor`).
pub const EE_SCREENS_SH_FOCUSED_TEXT_COLOR: u32 = 0x94;
/// The copied draw rectangle filled with `f[0..3]` at the opacity `f[3]` (the 2D draw list's `ee_ge_draw2d_fill`).
pub const EE_SCREENS_SH_FILL: u32 = 0x95;
/// `gUserInterface->CloseForm(a)`.
pub const EE_SCREENS_SH_CLOSE_FORM: u32 = 0x96;

/// The slots: the message label, OK, No, Cancel, the edit box, the background.
const SLOTS: usize = 6;

/// The box's host, `ee_screenscpp_dialog(box, slots, op, a, b)`.
pub(crate) trait DialogCpp {
    fn dialog(&self, form: Cpp, slots: *mut usize, op: u32, a: u32, b: u32) -> u32;
}

/// The box's host: the plain toolkit calls on its controls answered on their Rust objects (the toolkit's direct
/// path, `ee_ui::direct`), the rest `ee_screenscpp_dialog`'s. The float register is kept here as well (the C++'s
/// copy feeds its own ops: the controls' sizes, the background).
#[cfg(not(test))]
pub(crate) struct CppDialog {
    floats: Cell<[f32; 9]>,
}

#[cfg(not(test))]
impl CppDialog {
    pub(crate) fn new() -> Self {
        CppDialog { floats: Cell::new([0.0; 9]) }
    }

    /// The op on the toolkit's Rust objects, `None` for the C++'s.
    fn direct(&self, form: Cpp, slots: *mut usize, op: u32, a: u32, b: u32) -> Option<u32> {
        use crate::screens::dialog::*;
        use core::ffi::c_ulong;
        use ee_ui::direct;
        use ee_ui::editobj::EeUiEditCall;
        use ee_ui::formobj::EeUiFormCall;
        use ee_ui::labelobj::EeUiLabelCall;
        use ee_ui::obj::EeUiOp;
        // SAFETY: the box's live slots (six controls' addresses)
        let slot = |i: u32| unsafe { *slots.add((i as usize).min(SLOTS - 1)) } as Cpp;
        let (label, edit) = (slot(0), slot(4));
        let f = self.floats.get();
        let rgb = |i: usize| [f[i], f[i + 1], f[i + 2], 0.0];
        Some(match op {
            EE_SCREENS_DLG_FLOAT => {
                let mut r = self.floats.get();
                if let Some(v) = r.get_mut(a as usize) {
                    *v = f32::from_bits(b);
                }
                self.floats.set(r);
                return None;
            }
            EE_SCREENS_DLG_SHOW => {
                direct::control_virtual(slot(a), EeUiOp::SetDisplayMode, b as c_ulong as isize, 0, &mut [0.0; 4]);
                0
            }
            EE_SCREENS_DLG_TABBING => {
                let index = direct::with_fields(edit, |r| r.mIndex)?;
                direct::form_call(form, EeUiFormCall::SetTabbing, 1, 0);
                direct::form_call(form, EeUiFormCall::SetTabStop, index as isize, 0);
                0
            }
            EE_SCREENS_DLG_PASSWORD => {
                direct::edit_call(edit, EeUiEditCall::SetIsPasswordBox, (a != 0) as isize, 0)?;
                0
            }
            EE_SCREENS_DLG_LINES => direct::label_call(label, EeUiLabelCall::LineCount, 0, 0)? as u32,
            EE_SCREENS_DLG_SCROLL_BAR => {
                direct::label_call_f(label, EeUiLabelCall::SetScrollBarBgColor, 0, 0, &mut rgb(0))?;
                direct::label_call_f(label, EeUiLabelCall::SetScrollBarBgOpacity, 0, 0, &mut [f[3], 0.0, 0.0, 0.0])?;
                0
            }
            EE_SCREENS_DLG_ALIGN => {
                direct::label_call(label, EeUiLabelCall::SetAlignment, a as c_ulong as isize, 0)?;
                0
            }
            EE_SCREENS_DLG_TEXT_COLOR => {
                let text = if a == 0 {
                    direct::label_call(label, EeUiLabelCall::Text, 0, 0)?
                } else {
                    direct::edit_call(edit, EeUiEditCall::Text, 0, 0)?
                };
                let text = text as *const *mut ee_render::ge::smallobj::EeGeText;
                if text.is_null() {
                    return Some(0);
                }
                // SAFETY: the control's live GEText (its one member: the Rust text)
                unsafe {
                    let t = *text;
                    ee_render::ge::smallobj::ee_ge_text_set_transparent(t, true);
                    let c = ee_render::ge::smallobj::ee_ge_text_color(t, true);
                    core::slice::from_raw_parts_mut(c, 3).copy_from_slice(&f[..3]);
                }
                0
            }
            EE_SCREENS_DLG_ADD => {
                direct::form_call(form, EeUiFormCall::AddControl, slot(a) as isize, 0);
                0
            }
            EE_SCREENS_DLG_ENABLE => {
                let group = direct::form_call(form, EeUiFormCall::EntityGroup, 0, 0) as Cpp;
                direct::group_call(group, ee_ui::groupobj::EeUiEntityGroupCall::Enable, 0);
                0
            }
            _ => return None,
        })
    }
}

#[cfg(not(test))]
impl DialogCpp for CppDialog {
    fn dialog(&self, form: Cpp, slots: *mut usize, op: u32, a: u32, b: u32) -> u32 {
        extern "C" {
            fn ee_screenscpp_dialog(dialog: *mut core::ffi::c_void, slots: *mut usize, op: u32, a: u32, b: u32) -> u32;
        }
        if let Some(r) = self.direct(form, slots, op, a, b) {
            return r;
        }
        // SAFETY: the kind's six slots, live for the call
        if let Some(r) = crate::screens::dialog_calls::call(
            &mut crate::screens::game_cpp::Ffi,
            form as usize,
            unsafe { &mut *(slots as *mut [usize; 6]) },
            op,
            a,
            b,
        ) {
            return r;
        }
        // SAFETY: the live box and its slots
        unsafe { ee_screenscpp_dialog(form, slots, op, a, b) }
    }
}

/// `EEUIFError`: the slots and the mode (`mMode`, 0..4).
pub(crate) struct Dialog<D: DialogCpp> {
    slots: Cell<[usize; SLOTS]>,
    mode: Cell<u32>,
    cpp: D,
}

/// The plans' host on the box: the mode is the kind's, the rest the box's host's over the slots.
struct Plan<'a, D: DialogCpp> {
    form: Cpp,
    dialog: &'a Dialog<D>,
}

impl<D: DialogCpp> DialogHost for Plan<'_, D> {
    fn call(&mut self, op: u32, a: u32, b: u32) -> u32 {
        if op == EE_SCREENS_DLG_MODE {
            self.dialog.mode.set(a);
            return 0;
        }
        self.dialog.cpp.dialog(self.form, self.dialog.slots.as_ptr().cast(), op, a, b)
    }
}

/// The box's own ops of its host (past `dialog.rs`'): the controls deleted, the background scaled to the window, the
/// pressed button's callback posted (OK first, then No, then Cancel).
pub const EE_SCREENS_DLG_DELETE: u32 = 0x40;
pub const EE_SCREENS_DLG_SCALE_BACKGROUND: u32 = 0x41;
pub const EE_SCREENS_DLG_POST_CLOSED: u32 = 0x42;

impl<D: DialogCpp> Dialog<D> {
    pub(crate) fn new(cpp: D) -> Self {
        Dialog { slots: Cell::new([0; SLOTS]), mode: Cell::new(0), cpp }
    }
    fn plan(&self, c: &Ctx) -> Plan<'_, D> {
        Plan { form: c.ctl, dialog: self }
    }
    fn host(&self, c: &Ctx, op: u32) {
        self.cpp.dialog(c.ctl, self.slots.as_ptr().cast(), op, 0, 0);
    }
}

impl<D: DialogCpp> Kind for Dialog<D> {
    fn name(&self) -> &'static CStr {
        c"EEUIFError"
    }
    fn overrides(&self) -> u64 {
        (1 << EE_SCREENS_SV_OPEN) | (1 << EE_SCREENS_SV_CLOSE)
    }
    fn init(&self, c: &Ctx) {
        dialog::construct(&mut self.plan(c));
    }
    fn virt(&self, c: &Ctx, v: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        match v {
            EE_SCREENS_SV_OPEN => {
                c.form_base(EE_SCREENS_SV_OPEN, 0, 0, f);
                self.host(c, EE_SCREENS_DLG_SCALE_BACKGROUND);
                dialog::open(&mut self.plan(c), self.mode.get());
                0
            }
            EE_SCREENS_SV_CLOSE => {
                self.host(c, EE_SCREENS_DLG_POST_CLOSED);
                c.form_base(EE_SCREENS_SV_CLOSE, 0, 0, f);
                0
            }
            _ => c.form_base(v, a, b, f),
        }
    }
    fn op(&self, c: &Ctx, op: u32, a: i64, b: i64, _f: &mut [f32; 4]) -> i64 {
        if op == EE_SCREENS_SKOP_DIALOG_SHOW {
            dialog::show(&mut self.plan(c), a as u32, b as u32, (b >> 32) != 0);
        }
        0
    }
    fn free(&self, c: &Ctx) {
        self.host(c, EE_SCREENS_DLG_DELETE);
    }
}

/// `EEUIFEBOK` / `EEUIFEBCancel`.
pub(crate) struct DialogButton {
    name: &'static CStr,
    callback: Cell<i64>,
    data: Cell<i64>,
    close_form: Cell<i64>,
    pressed: Cell<bool>,
}

impl DialogButton {
    pub(crate) fn new(cancel: bool) -> Self {
        DialogButton {
            name: if cancel { c"EEUIFEBCancel" } else { c"EEUIFEBOK" },
            callback: Cell::new(0),
            data: Cell::new(0),
            close_form: Cell::new(0),
            pressed: Cell::new(false),
        }
    }
}

/// `kMessageFocusedColor`, `kMessageButtonColor` and `kMessageButtonOpacity`.
const FOCUSED: [f32; 4] = [0.61, 0.46, 0.0, 0.0];
const FILL: [f32; 4] = [1.0, 1.0, 1.0, 0.4];

impl Kind for DialogButton {
    fn name(&self) -> &'static CStr {
        self.name
    }
    fn overrides(&self) -> u64 {
        (1 << EE_SCREENS_SV_DRAW_POLYGONS) | (1 << EE_SCREENS_SV_BUTTON_UP)
    }
    fn init(&self, c: &Ctx) {
        c.call_f(EE_SCREENS_SH_FOCUSED_TEXT_COLOR, 0, FOCUSED);
    }
    fn virt(&self, c: &Ctx, v: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        match v {
            EE_SCREENS_SV_DRAW_POLYGONS => {
                c.call_f(EE_SCREENS_SH_FILL, 0, FILL);
                c.base0(EE_SCREENS_SV_DRAW_POLYGONS)
            }
            EE_SCREENS_SV_BUTTON_UP => {
                self.pressed.set(true);
                c.base0(EE_SCREENS_SV_BUTTON_UP);
                c.call(EE_SCREENS_SH_CLOSE_FORM, self.close_form.get(), 0);
                0
            }
            _ => c.base(v, a, b, f),
        }
    }
    fn op(&self, _c: &Ctx, op: u32, a: i64, b: i64, _f: &mut [f32; 4]) -> i64 {
        match op {
            EE_SCREENS_SKOP_DIALOG_SET_CALLBACK => {
                self.callback.set(a);
                self.data.set(b);
            }
            EE_SCREENS_SKOP_DIALOG_SET_CLOSE => self.close_form.set(a),
            EE_SCREENS_SKOP_DIALOG_PRESSED => return i64::from(self.pressed.get()),
            EE_SCREENS_SKOP_DIALOG_CALLBACK => return self.callback.get(),
            EE_SCREENS_SKOP_DIALOG_DATA => return self.data.get(),
            _ => {}
        }
        0
    }
}

pub(crate) fn make(kind: u32, args: &[i64]) -> Option<Box<dyn Kind>> {
    Some(match kind {
        #[cfg(not(test))]
        EE_SCREENS_SK_DIALOG => Box::new(Dialog::new(CppDialog::new())),
        EE_SCREENS_SK_DIALOG_OK => Box::new(DialogButton::new(false)),
        EE_SCREENS_SK_DIALOG_CANCEL => Box::new(DialogButton::new(true)),
        _ => return crate::screens::shells_saved::make(kind, args),
    })
}

#[cfg(test)]
mod tests;
