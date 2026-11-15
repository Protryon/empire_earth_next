//! `UIControl`'s members as a Rust record (R6 ui round 19): [`EeUiControlFields`] is the C++ class's (private) base,
//! so every control's state lives in this record, and the base's non-virtual logic runs here over it: the
//! constructor's state, `CopyData`, `SetDirty`, `SetDisplayMode`, `SetLocation`, `SetRectangle`, the trigger events
//! (`SetTriggerEvent`, `GetTriggerEvent`, `FireTriggerEvent`, the destructor's release), the captures and the help
//! string.
//!
//! The field names are the C++ members' (the pattern of `ee_scenario`'s `EeScenarioObjectFields`): the C++ class reads
//! them in place (its inline getters, `GetRectangle()` answering the record's rectangle as a `GERectangle`), and a
//! Rust caller holding a control's record (`UIControl::GetFields()`) reads them directly. The two background rhombi
//! are C++ `GERhombus`es the C++ constructor makes and the record holds by address; the trigger events are a Rust map
//! the record owns.
//!
//! Every call out is an [`EeUiOp`] on the record's host (`UIControl.cpp`'s `UIControlHost::Op`, the object
//! `mCpp`): the form's and manager's questions (pure), the virtuals a subclass may override (`SetDirty`,
//! `HideControl`, `SetLocation`), the rhombi (0 the background, 1 its copy) and the events. No reference into the
//! record is held across a call out (the virtuals re-enter): each step reads or writes the fields it needs afresh.

use core::ffi::{c_ulong, c_void};

use crate::obj::{read_buf, EeUiHostFn, EeUiOp as Op, Host};
use crate::rect::UiRect;

/// The trigger events (`std::map<unsigned long, UIEvent*>`): event id -> the event (referenced), in id order.
#[derive(Debug, Default)]
pub struct EeUiTriggers {
    v: Vec<(c_ulong, isize)>,
}

impl EeUiTriggers {
    fn find(&self, id: c_ulong) -> Result<usize, usize> {
        self.v.binary_search_by(|e| e.0.cmp(&id))
    }
}

/// `UIControl`'s members (`include/lle/UIControl.h`), the C++ class's base.
#[repr(C)]
#[allow(non_snake_case)]
#[derive(Debug)]
pub struct EeUiControlFields {
    /// The C++ control (the host's object; `this` in the comparisons with the form's controls and captures).
    pub mCpp: *mut c_void,
    /// The host of the calls out.
    pub mHost: EeUiHostFn,
    /// The trigger events (owned; made by [`ee_ui_control_init`], freed by [`ee_ui_control_release`]).
    pub mTriggerEvents: *mut EeUiTriggers,
    /// The background (`GERhombus*`, the C++ constructor's).
    pub mBgRhombus: *mut c_void,
    /// The background as of the last `CopyData` (`GERhombus*`).
    pub mCopiedBgRhombus: *mut c_void,
    /// The form (`UIForm*`).
    pub mForm: *mut c_void,
    /// The hot key (`UIHotKey*`).
    pub mHotKey: *mut c_void,
    pub mRectangle: UiRect,
    /// `mRectangle` as of the last `CopyData`.
    pub mCopiedDrawRectangle: UiRect,
    /// `kDisplayModeHidden` 0, `kDisplayModeShown` 1.
    pub mDisplayMode: c_ulong,
    pub mControlType: c_ulong,
    pub mBorderType: c_ulong,
    pub mCopiedBorderType: c_ulong,
    pub mIndex: c_ulong,
    pub mId: c_ulong,
    pub mDrawCount: c_ulong,
    pub mBorderThickness: c_ulong,
    pub mStringTableID: c_ulong,
    pub mBorderOpacity: f32,
    pub mCopiedBorderOpacity: f32,
    pub mIsDirty: bool,
    pub mLeftButtonDown: bool,
    pub mRightButtonDown: bool,
    pub mCopiedHasBgOpacity: bool,
    pub mUnknown9C: bool,
    pub mHasBgOpacity: bool,
    pub mAlwaysDirty: bool,
}

/// The inward calls (the C++ class's methods that forward here).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeUiControlCall {
    /// `UIControl::CopyData()` -> 0.
    CopyData = 1,
    /// `FireTriggerEvent(a)`.
    FireTrigger = 2,
    /// `GetHasKeyboardCapture()`.
    HasKeyboardCapture = 3,
    /// `GetHasMouseCapture()`.
    HasMouseCapture = 4,
    /// `UIControl::GetHelpString(*(UWideString*)a)` -> whether the control has one.
    HelpString = 5,
    /// `GetLeftMouseButtonDown()`.
    LeftButtonDown = 6,
    /// `GetRightMouseButtonDown()`.
    RightButtonDown = 7,
    /// `GetTriggerEvent(a)` -> the event or 0.
    TriggerEvent = 8,
    /// `UIControl::ReleaseCapturedKeyboard()`.
    ReleaseKeyboard = 9,
    /// `UIControl::ReleaseCapturedMouse()`.
    ReleaseMouse = 10,
    /// `SetBgRectangle(f)`.
    SetBgRectangle = 11,
    /// `UIControl::SetDirty(a != 0)`.
    SetDirty = 12,
    /// `UIControl::SetDisplayMode(a)`.
    SetDisplayMode = 13,
    /// `UIControl::SetLocation(f[0], f[1])`.
    SetLocation = 14,
    /// `UIControl::SetRectangle(f)`.
    SetRectangle = 15,
    /// `SetTriggerEvent(a, event b)`.
    SetTrigger = 16,
    /// `CaptureKeyboard()`.
    CaptureKeyboard = 17,
    /// `CaptureMouse()`.
    CaptureMouse = 18,
}

/// The background rhombus (the rhombus ops' `a`; 1 is its copy, `RhombusCopy`'s target).
const BG: isize = 0;

/// The rectangle grown by half a pixel on each side (the background's).
fn grown(r: UiRect) -> UiRect {
    UiRect::new(r.left - 0.5, r.top - 0.5, r.right + 0.5, r.bottom + 0.5)
}

/// A control's record, reached by address: every access reads or writes the C++ object's base afresh.
#[derive(Clone, Copy)]
pub struct Ctl(*mut EeUiControlFields);

impl Ctl {
    /// # Safety
    /// `p` is a live record (a C++ control's base) for as long as the value is used.
    pub unsafe fn new(p: *mut EeUiControlFields) -> Self {
        Ctl(p)
    }

    /// A copy of the fields named by `f` (no reference outlives the read).
    fn get<T>(&self, f: impl FnOnce(&EeUiControlFields) -> T) -> T {
        // SAFETY: live (the constructor's contract); the borrow ends before any call out
        f(unsafe { &*self.0 })
    }

    fn set(&self, f: impl FnOnce(&mut EeUiControlFields)) {
        // SAFETY: as get
        f(unsafe { &mut *self.0 })
    }

    /// `GetRectangle()`.
    pub fn rect(&self) -> UiRect {
        self.get(|r| r.mRectangle)
    }

    /// `GetId()`.
    pub fn id(&self) -> c_ulong {
        self.get(|r| r.mId)
    }

    /// `GetIndex()`.
    pub fn index(&self) -> c_ulong {
        self.get(|r| r.mIndex)
    }

    /// `GetDisplayMode()`.
    pub fn display_mode(&self) -> c_ulong {
        self.get(|r| r.mDisplayMode)
    }

    /// `GetForm()`.
    pub fn form(&self) -> *mut c_void {
        self.get(|r| r.mForm)
    }

    /// The C++ control (the object a caller's host calls the virtuals on).
    pub fn cpp(&self) -> *mut c_void {
        self.get(|r| r.mCpp)
    }

    fn host(&self) -> Host {
        self.get(|r| Host { obj: r.mCpp, f: r.mHost })
    }

    fn this(&self) -> isize {
        self.get(|r| r.mCpp as isize)
    }

    fn op(&self, op: Op, a: isize, b: isize) -> isize {
        self.host().op(op, a, b, 0, 0)
    }

    /// The form's control in this control's slot (`mForm->GetControl(mIndex)`).
    fn slot(&self) -> isize {
        self.op(Op::CtlFormControl, self.get(|r| r.mIndex) as isize, 0)
    }

    fn group_enabled(&self) -> bool {
        self.op(Op::CtlGroupEnabled, self.get(|r| r.mId) as isize, 0) != 0
    }

    /// The constructor's state: shown, dirty, opaque borders, no events; the background transparent black, half a
    /// pixel outside the control.
    #[allow(clippy::too_many_arguments)]
    pub fn init(
        &self,
        cpp: *mut c_void,
        host: EeUiHostFn,
        control_type: c_ulong,
        id: c_ulong,
        rect: UiRect,
        form: *mut c_void,
        index: c_ulong,
        border_thickness: c_ulong,
    ) {
        self.set(|r| {
            r.mCpp = cpp;
            r.mHost = host;
            r.mTriggerEvents = Box::into_raw(Box::default());
            r.mBorderOpacity = 1.0;
            r.mControlType = control_type;
            r.mCopiedBorderOpacity = 1.0;
            r.mId = id;
            r.mForm = form;
            r.mIndex = index;
            r.mDisplayMode = 1;
            r.mBorderThickness = border_thickness;
            r.mLeftButtonDown = false;
            r.mRightButtonDown = false;
            r.mAlwaysDirty = false;
            r.mDrawCount = 0;
            r.mIsDirty = true;
            r.mBorderType = 0;
            r.mCopiedBorderType = 0;
            r.mCopiedHasBgOpacity = false;
            r.mUnknown9C = false;
            r.mStringTableID = 0;
            r.mHotKey = core::ptr::null_mut();
            r.mRectangle = rect;
            r.mCopiedDrawRectangle = UiRect::default();
            r.mHasBgOpacity = false;
        });
        let h = self.host();
        h.op_f(Op::RhombusSetColor, BG, 0, 0, 0, &mut [0.0; 4]);
        h.op_f(Op::RhombusSetOpacity, BG, 0, 0, 0, &mut [0.0; 4]);
        h.op_rect(Op::RhombusSetRect, BG, 0, grown(rect));
    }

    /// The destructor: every trigger event uninitialized and released, in id order; the map freed.
    pub fn release(&self) {
        let t = self.get(|r| r.mTriggerEvents);
        if t.is_null() {
            return;
        }
        let all = {
            // SAFETY: made by init, freed only here; the borrow ends before the calls out
            let m = unsafe { &mut *t };
            core::mem::take(&mut m.v)
        };
        for (_, e) in all {
            self.event_call(e, 0);
            self.event_call(e, 1);
        }
        self.set(|r| r.mTriggerEvents = core::ptr::null_mut());
        // SAFETY: as above; no other holder
        drop(unsafe { Box::from_raw(t) });
    }

    /// Event `e`'s 0 `Uninitialize`, 1 `Dereference`, 2 `Reference`, 3 `Initialize`.
    fn event_call(&self, e: isize, which: isize) {
        self.op(Op::FormEventCall, e, which);
    }

    fn triggers(&self) -> *mut EeUiTriggers {
        self.get(|r| r.mTriggerEvents)
    }

    /// `CopyData`: the background, border and rectangle snapshot for drawing and the surfaces still to draw.
    pub fn copy_data(&self) -> isize {
        if self.get(|r| r.mHasBgOpacity) {
            self.set(|r| r.mCopiedHasBgOpacity = true);
            self.op(Op::RhombusCopy, 0, 0);
        } else {
            self.set(|r| r.mCopiedHasBgOpacity = false);
        }
        if self.get(|r| r.mAlwaysDirty) {
            self.op(Op::SetDirty, 1, 0);
        } else {
            self.set_dirty(false);
        }
        self.set(|r| {
            r.mCopiedBorderOpacity = r.mBorderOpacity;
            r.mCopiedBorderType = r.mBorderType;
            r.mCopiedDrawRectangle = r.mRectangle;
        });
        let n = self.op(Op::FormSurfaces, 0, 0) as c_ulong;
        self.set(|r| r.mDrawCount = n);
        0
    }

    /// `FireTriggerEvent`: queues the trigger's event, if one is set.
    pub fn fire_trigger(&self, id: c_ulong) {
        let e = self.trigger_event(id);
        if e != 0 {
            self.op(Op::CtlAddEvent, e, 0);
        }
    }

    /// `GetTriggerEvent`: the event set for the trigger, or 0.
    pub fn trigger_event(&self, id: c_ulong) -> isize {
        let t = self.triggers();
        // SAFETY: made by init; read without a call out
        let t = unsafe { &*t };
        match t.find(id) {
            Ok(i) => t.v[i].1,
            Err(_) => 0,
        }
    }

    /// `SetTriggerEvent`: the old event for the trigger uninitialized, released and removed; the new one (if any)
    /// referenced, initialized and kept.
    pub fn set_trigger(&self, id: c_ulong, event: isize) {
        let t = self.triggers();
        let old = {
            // SAFETY: made by init; the borrow ends before the calls out
            let m = unsafe { &mut *t };
            m.find(id).ok().map(|i| m.v.remove(i).1)
        };
        if let Some(e) = old {
            self.event_call(e, 0);
            self.event_call(e, 1);
        }
        if event == 0 {
            return;
        }
        self.event_call(event, 2);
        self.event_call(event, 3);
        // SAFETY: as above; the slot is free (removed above, and the calls out do not reach this control's map)
        let t = unsafe { &mut *t };
        match t.find(id) {
            Ok(_) => {} // std::map::insert keeps the present entry
            Err(i) => t.v.insert(i, (id, event)),
        }
    }

    /// `GetHasKeyboardCapture`: whether the form's control in this slot holds the keyboard.
    pub fn has_keyboard_capture(&self) -> bool {
        self.slot() == self.op(Op::CtlKeyboardCaptured, 0, 0)
    }

    /// `GetHasMouseCapture`.
    pub fn has_mouse_capture(&self) -> bool {
        self.slot() == self.op(Op::CtlMouseCaptured, 0, 0)
    }

    /// `GetHelpString`: the string table's string into `out` when the control has one.
    pub fn help_string(&self, out: isize) -> bool {
        let id = self.get(|r| r.mStringTableID);
        if id > 0 {
            self.op(Op::CtlHelpString, out, id as isize);
            return true;
        }
        false
    }

    /// `GetLeftMouseButtonDown`: the flag of the form's control in this slot.
    pub fn left_button_down(&self) -> bool {
        self.op(Op::CtlSlotLeftDown, self.get(|r| r.mIndex) as isize, 0) != 0
    }

    /// `GetRightMouseButtonDown`.
    pub fn right_button_down(&self) -> bool {
        self.op(Op::CtlSlotRightDown, self.get(|r| r.mIndex) as isize, 0) != 0
    }

    /// `CaptureKeyboard`.
    pub fn capture_keyboard(&self) {
        self.op(Op::CtlSetKeyboardCaptured, self.this(), 0);
    }

    /// `CaptureMouse`.
    pub fn capture_mouse(&self) {
        self.op(Op::CtlSetMouseCaptured, self.this(), 0);
    }

    /// `ReleaseCapturedKeyboard`: when this control holds it.
    pub fn release_keyboard(&self) {
        if self.op(Op::CtlKeyboardCaptured, 0, 0) == self.this() {
            self.op(Op::CtlSetKeyboardCaptured, 0, 0);
        }
    }

    /// `ReleaseCapturedMouse`.
    pub fn release_mouse(&self) {
        if self.op(Op::CtlMouseCaptured, 0, 0) == self.this() {
            self.op(Op::CtlSetMouseCaptured, 0, 0);
        }
    }

    /// `SetBgRectangle`: the background, half a pixel outside `r`.
    pub fn set_bg_rectangle(&self, r: UiRect) {
        self.host().op_rect(Op::RhombusSetRect, BG, 0, grown(r));
    }

    /// `SetDirty`: dirtying goes to the control owning the form slot when that is another one.
    pub fn set_dirty(&self, dirty: bool) {
        if dirty {
            let c = self.slot();
            if c != 0 && c != self.this() {
                self.op(Op::FormCtlSetDirty, c, 1);
            } else {
                self.set(|r| r.mIsDirty = true);
            }
        } else {
            self.set(|r| r.mIsDirty = false);
        }
    }

    /// `SetDisplayMode`: hiding a control of an enabled group on the current form dirties its area and tabs away
    /// from it; any other change dirties it (an enabled group's).
    pub fn set_display_mode(&self, mode: c_ulong) {
        if self.get(|r| r.mDisplayMode) != mode {
            self.set(|r| r.mDrawCount = 0);
            if mode == 0 && self.group_enabled() && self.get(|r| r.mForm as isize) == self.op(Op::CtlCurrentForm, 0, 0)
            {
                self.op(Op::CtlHide, 0, 0);
                let rect = self.get(|r| r.mRectangle);
                self.host().op_rect(Op::CtlAddDirty, 0, 0, rect);
                if self.op(Op::TabbingOn, 0, 0) != 0 && self.op(Op::TabStop, 0, 0) as c_ulong == self.get(|r| r.mIndex)
                {
                    self.op(Op::CtlTab, 0, 0);
                }
            } else if self.group_enabled() {
                self.op(Op::SetDirty, 1, 0);
            }
        }
        self.set(|r| r.mDisplayMode = mode);
    }

    /// `SetLocation`: moves the rectangle and background to a new top-left, dirtying the old area when this control
    /// owns its slot, is shown and its group enabled.
    pub fn set_location(&self, x: f32, y: f32) {
        let c = self.slot();
        let rect = self.get(|r| r.mRectangle);
        let dx = x - rect.left;
        let dy = y - rect.top;
        if c != 0 && c == self.this() && self.get(|r| r.mDisplayMode) == 1 && self.group_enabled() {
            self.host().op_rect(Op::CtlAddDirty, 0, 0, rect);
        }
        let rect = self.get(|r| r.mRectangle);
        let moved = UiRect::new(rect.left + dx, rect.top + dy, rect.right + dx, rect.bottom + dy);
        self.set(|r| r.mRectangle = moved);
        self.host().op_rect(Op::RhombusSetRect, BG, 0, grown(moved));
        self.op(Op::SetDirty, 1, 0);
    }

    /// `SetRectangle`: the old area dirtied as `SetLocation`'s, the rectangle set, then the virtual `SetLocation` to
    /// its top-left and the background.
    pub fn set_rectangle(&self, r: UiRect) {
        let c = self.slot();
        if c != 0 && c == self.this() && self.get(|r| r.mDisplayMode) == 1 && self.group_enabled() {
            let old = self.get(|r| r.mRectangle);
            self.host().op_rect(Op::CtlAddDirty, 0, 0, old);
        }
        self.set(|s| s.mRectangle = r);
        let h = self.host();
        h.op_f(Op::VirtSetLocation, 0, 0, 0, 0, &mut [r.left, r.top, 0.0, 0.0]);
        let now = self.get(|r| r.mRectangle);
        h.op_rect(Op::RhombusSetRect, BG, 0, grown(now));
    }

    /// One inward call; `f` points at four floats (or is NULL for the calls without).
    ///
    /// # Safety
    /// `f` is NULL or four live floats.
    pub unsafe fn call(&self, call: EeUiControlCall, a: isize, b: isize, f: *mut f32) -> isize {
        type C = EeUiControlCall;
        // SAFETY: the contract
        let buf = unsafe { read_buf(f) };
        let rect = crate::obj::buf_rect(&buf);
        match call {
            C::CopyData => return self.copy_data(),
            C::FireTrigger => self.fire_trigger(a as c_ulong),
            C::HasKeyboardCapture => return self.has_keyboard_capture() as isize,
            C::HasMouseCapture => return self.has_mouse_capture() as isize,
            C::HelpString => return self.help_string(a) as isize,
            C::LeftButtonDown => return self.left_button_down() as isize,
            C::RightButtonDown => return self.right_button_down() as isize,
            C::TriggerEvent => return self.trigger_event(a as c_ulong),
            C::ReleaseKeyboard => self.release_keyboard(),
            C::ReleaseMouse => self.release_mouse(),
            C::SetBgRectangle => self.set_bg_rectangle(rect),
            C::SetDirty => self.set_dirty(a != 0),
            C::SetDisplayMode => self.set_display_mode(a as c_ulong),
            C::SetLocation => self.set_location(buf[0], buf[1]),
            C::SetRectangle => self.set_rectangle(rect),
            C::SetTrigger => self.set_trigger(a as c_ulong, b),
            C::CaptureKeyboard => self.capture_keyboard(),
            C::CaptureMouse => self.capture_mouse(),
        }
        0
    }
}

// ---- the C ABI

/// The constructor's state of the record `rec` (the C++ control `cpp`'s base; its rhombi already made), the rectangle
/// at `rect` (`GERectangle`'s four floats).
///
/// # Safety
/// `rec` is the live base of `cpp`; `rect` four live floats.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_ui_control_init(
    rec: *mut EeUiControlFields,
    cpp: *mut c_void,
    host: EeUiHostFn,
    control_type: c_ulong,
    id: c_ulong,
    rect: *const f32,
    form: *mut c_void,
    index: c_ulong,
    border_thickness: c_ulong,
) {
    // SAFETY: the contract
    let r = crate::obj::buf_rect(&unsafe { read_buf(rect) });
    // SAFETY: the contract
    unsafe { Ctl::new(rec) }.init(cpp, host, control_type, id, r, form, index, border_thickness);
    crate::direct::put(crate::direct::Kind::Control, cpp, rec as *const c_void);
}

/// The destructor's part: the trigger events released and the map freed (the C++ deletes the rhombi).
///
/// # Safety
/// `rec` is a live record [`ee_ui_control_init`] set up.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_control_release(rec: *mut EeUiControlFields) {
    crate::direct::forget(crate::direct::Kind::Control, rec as *const c_void);
    // SAFETY: the contract
    unsafe { Ctl::new(rec) }.release();
}

/// One of UIControl's methods ([`EeUiControlCall`]).
///
/// # Safety
/// `rec` is a live record [`ee_ui_control_init`] set up; `f` is NULL or four live floats.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_control_call(
    rec: *mut EeUiControlFields,
    call: EeUiControlCall,
    a: isize,
    b: isize,
    f: *mut f32,
) -> isize {
    // SAFETY: the contract
    unsafe { Ctl::new(rec).call(call, a, b, f) }
}

#[cfg(test)]
#[path = "controlobj_tests.rs"]
mod tests;
