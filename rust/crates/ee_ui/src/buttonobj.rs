//! `UIButton` as a whole Rust object (R6 ui round 11): the text rectangle, the bitmap location, the four bitmaps (C++
//! objects held by address, their references counted through ops), the state, the copied text rectangle, the draw
//! offset, the focused text colour, the two sounds (sample ids), the two texts (C++ `GEText`s by address) and the
//! key and mouse presses, with every method of `UIButton.cpp`, the header's and `UIDualButton`'s two handlers.
//!
//! The C++ `UIButton` stays the type the forms, the controls table and the game's sixteen button subclasses hold (a
//! `UIControl` with its vtable): its methods are one-line forwarders to [`ee_ui_button_call`], its constructor and
//! destructor make and free this object, and every call out is an [`EeUiOp`] through its host. The virtuals a
//! subclass overrides (`ButtonDown`, `ButtonUp`, `MouseOver`, `MouseMove`, `GetTabable`, `SetDirty`, the capture
//! calls) go through the C++ object, so the state is never borrowed across an op: a check box's `ButtonUp` toggles
//! and calls back in, a trigger event may read the button.
//!
//! The state machine is round 3's (`button.rs`, pinned by its goldens); this object is its [`ButtonHost`]. A
//! subclass's record is the object's [`Sub`] (R6 ui round 12: the check box, radio button and tri-state box in
//! `checkobj.rs`, the scroll arrow, blinking and scalable buttons in `variantobj.rs`).

use core::cell::RefCell;
use core::ffi::{c_long, c_ulong, c_void};

use crate::button::{
    self, ButtonFields, ButtonHost, ButtonState, Key, UiColor, STATE_FOCUSED, STATE_NORMAL, STATE_PRESSED,
};
use crate::obj::{read_buf, write_buf, EeUiHostFn, EeUiOp as Op, Host};
use crate::rect::{UiPoint, UiRect};

/// The inward calls (the C++ shim's methods).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeUiButtonCall {
    /// `CopyData()` -> its result.
    CopyData = 1,
    /// `DrawPolygons()` -> its result.
    DrawPolygons = 2,
    /// `DrawTextA()` -> its result.
    DrawText = 3,
    /// `UIButton::ButtonDown()`.
    ButtonDown = 4,
    /// `UIButton::ButtonUp()`.
    ButtonUp = 5,
    /// `UIButton::MouseLButtonDown()` -> OK.
    MouseLButtonDown = 6,
    /// `UIButton::MouseLButtonUp()` -> OK.
    MouseLButtonUp = 7,
    /// `UIButton::MouseMove(f[0], f[1])` -> OK.
    MouseMove = 8,
    /// `UIButton::MouseOver()`.
    MouseOver = 9,
    /// `UIButton::TabbedTo()`.
    TabbedTo = 10,
    /// `UIButton::TabbedFrom()`.
    TabbedFrom = 11,
    /// `UIButton::UpdateKeyboard()` -> OK.
    UpdateKeyboard = 12,
    /// `UIButton::SetLocation(f[0], f[1])`.
    SetLocation = 13,
    /// `SetText(*(const UWideString*)a, font b)`.
    SetText = 14,
    /// `ChangeFont(font a)`.
    ChangeFont = 15,
    /// `SetButtonDownSound(*(const UString*)a)`.
    SetButtonDownSound = 16,
    /// `SetButtonUpSound(*(const UString*)a)`.
    SetButtonUpSound = 17,
    /// `GetButtonDownSound()`.
    ButtonDownSound = 18,
    /// `GetButtonUpSound()`.
    ButtonUpSound = 19,
    /// `SetNormalBitmap(a)`.
    SetNormalBitmap = 20,
    /// `SetFocusedBitmap(a)`.
    SetFocusedBitmap = 21,
    /// `SetPressedBitmap(a)`.
    SetPressedBitmap = 22,
    /// `GetNormalBitmap()`.
    NormalBitmap = 23,
    /// `GetFocusedBitmap()`.
    FocusedBitmap = 24,
    /// `GetPressedBitmap()`.
    PressedBitmap = 25,
    /// `GetCurrentBitmap()`.
    CurrentBitmap = 26,
    /// `GetCopiedBitmap()`.
    CopiedBitmap = 27,
    /// `GetGEText()`.
    Text = 28,
    /// `GetIsButtonDown()`.
    IsButtonDown = 29,
    /// `GetState()`.
    State = 30,
    /// `GetBitmapLocation()` -> `f[0]`, `f[1]`.
    BitmapLocation = 31,
    /// `SetBitmapDrawOffset(f[0], f[1])`.
    SetBitmapDrawOffset = 32,
    /// `SetFocusedTextColor(f[0..3])`.
    SetFocusedTextColor = 33,
    /// `SetTextRectangle(f)`.
    SetTextRectangle = 34,
    /// `ReplaceNormalBitmap(a)` (no `SetDirty`).
    ReplaceNormalBitmap = 35,
    /// `ReplacePressedBitmap(a)` (no `SetDirty`).
    ReplacePressedBitmap = 36,
    /// `UIDualButton::MouseRButtonDown()` -> OK.
    DualMouseRButtonDown = 37,
    /// `UIDualButton::MouseRButtonUp()` -> OK.
    DualMouseRButtonUp = 38,

    // ---- UICheckBox (checkobj.rs)
    /// The constructor's part after `UIButton`'s: `SetText(*(const UWideString*)a, font b)` beside a box of height
    /// `f[0]` (the constructor's rectangle's), the bitmap scaled when `f[1]` is nonzero.
    CheckInit = 40,
    /// `ChangeWidth(a)`.
    CheckChangeWidth = 41,
    /// `CopyData()` -> its result.
    CheckCopyData = 42,
    /// `DrawPolygons()` -> its result.
    CheckDrawPolygons = 43,
    /// `DrawTextA()` -> its result.
    CheckDrawText = 44,
    /// `SetChecked(a != 0)`.
    CheckSetChecked = 45,
    /// `SetDisabled(a != 0)`.
    CheckSetDisabled = 46,
    /// `SetLocation(f[0], f[1])`.
    CheckSetLocation = 47,
    /// `SetText(*(const UWideString*)a, font b)`.
    CheckSetText = 48,
    /// `ToggleCheck()`.
    CheckToggle = 49,
    /// `ButtonUp()`: the toggle, then `UIButton::ButtonUp()`.
    CheckButtonUp = 50,
    /// `ButtonDown()`.
    CheckButtonDown = 51,
    /// `MouseLButtonDown()` -> OK.
    CheckMouseLButtonDown = 52,
    /// `MouseLButtonUp()` -> OK.
    CheckMouseLButtonUp = 53,
    /// `MouseMove(f[0], f[1])` -> OK.
    CheckMouseMove = 54,
    /// `GetIsChecked()`.
    CheckIsChecked = 55,
    /// `GetTextLabel()` (the check box's, radio button's or tri-state box's).
    TextLabel = 56,

    // ---- UIRadioButton (checkobj.rs)
    /// The constructor's part after `UIButton`'s: the other state's bitmaps, a checked button's swap, the bitmap
    /// offset in a box of height `f[0]`, then `SetText(*(const UWideString*)a, font b)`; `f[1]` the push button,
    /// `f[2]` checked, `f[3]` the scaled bitmap (nonzero: true).
    RadioInit = 60,
    /// `UIRadioButton::Check()`.
    RadioCheck = 61,
    /// `UIRadioButton::UnCheck()`.
    RadioUnCheck = 62,
    /// `CopyData()` -> its result.
    RadioCopyData = 63,
    /// `DrawPolygons()` -> its result.
    RadioDrawPolygons = 64,
    /// `DrawTextA()` -> its result.
    RadioDrawText = 65,
    /// `GetIsChecked()`.
    RadioIsChecked = 66,
    /// `GetTabable()`: not a push button.
    RadioTabable = 67,
    /// `SetDownCheckedBitmap(a)`.
    RadioSetDownChecked = 68,
    /// `SetDownUncheckedBitmap(a)`.
    RadioSetDownUnchecked = 69,
    /// `SetUpCheckedBitmap(a)`.
    RadioSetUpChecked = 70,
    /// `SetUpUncheckedBitmap(a)`.
    RadioSetUpUnchecked = 71,
    /// `SetLocation(f[0], f[1])`.
    RadioSetLocation = 72,
    /// `SetText(*(const UWideString*)a, font b)`.
    RadioSetText = 73,
    /// `ButtonUp()`: the virtual `Check()`, then `UIButton::ButtonUp()`.
    RadioButtonUp = 74,
    /// `MouseLButtonDown()` -> OK.
    RadioMouseLButtonDown = 75,

    // ---- UITriStateCheckBox (checkobj.rs)
    /// The constructor's part after `UIButton`'s: the bitmap offset in a box of height `f[0]`, then
    /// `SetText(*(const UWideString*)a, font b)`.
    TriInit = 80,
    /// `ChangeWidth(a)`.
    TriChangeWidth = 81,
    /// `CopyData()` -> its result.
    TriCopyData = 82,
    /// `DrawPolygons()` -> its result.
    TriDrawPolygons = 83,
    /// `DrawTextA()` -> its result.
    TriDrawText = 84,
    /// `GetCheckState()`.
    TriCheckState = 85,
    /// `SetCheckState(a)`.
    TriSetCheckState = 86,
    /// `SetLocation(f[0], f[1])`.
    TriSetLocation = 87,
    /// `SetText(*(const UWideString*)a, font b)`.
    TriSetText = 88,
    /// `CycleCheck()`.
    TriCycle = 89,
    /// `ButtonUp()`: the cycle, then `UIButton::ButtonUp()`.
    TriButtonUp = 90,

    // ---- UIScrollButton (variantobj.rs)
    /// The constructor's part after `UIButton`'s: the slider `a`, the direction `b` (nonzero: down), the polls made.
    ScrollInit = 95,
    /// `ButtonDown()`: the slider stepped on the press, after the delay and at every repeat, then `UIButton`'s.
    ScrollButtonDown = 96,
    /// `ButtonUp()`: the polls stopped, then `UIButton::ButtonUp()`.
    ScrollButtonUp = 97,

    // ---- UIBlinkingButton (variantobj.rs)
    /// The constructor's part after `UIButton`'s: the poll, the record for the blink rate `a`.
    BlinkInit = 100,
    /// `SetIsBlinking(a != 0)`.
    BlinkSetIsBlinking = 101,
    /// `GetIsBlinking()`.
    BlinkIsBlinking = 102,
    /// `CopyData()` -> its result: the fade, then `UIButton::CopyData()`.
    BlinkCopyData = 103,

    // ---- UIScalableButton (variantobj.rs)
    /// The constructor's source rectangle `a` (0 normal, 1 focused, 2 pressed) = `f`.
    ScalableInit = 110,
    /// `CopyData()` -> its result.
    ScalableCopyData = 111,
    /// `DrawPolygons()` -> its result.
    ScalableDrawPolygons = 112,
}

/// A subclass's own record (its constructor's init call sets it): what were the C++ subclass's members.
#[derive(Clone, Copy, Debug)]
pub(crate) enum Sub {
    /// `UIButton` itself, or a subclass without a Rust record.
    Plain,
    /// `UICheckBox` (`checkobj.rs`).
    Check(button::CheckFields),
    /// `UIRadioButton` (`checkobj.rs`).
    Radio(button::RadioFields),
    /// `UITriStateCheckBox` (`checkobj.rs`).
    TriState(crate::button_variants::TriStateFields),
    /// `UIScrollButton` (`variantobj.rs`).
    Scroll(crate::variantobj::ScrollFields),
    /// `UIBlinkingButton` (`variantobj.rs`).
    Blink(crate::variantobj::BlinkObj),
    /// `UIScalableButton` (`variantobj.rs`).
    Scalable(crate::variantobj::ScalableFields),
}

/// The Rust UIButton.
pub struct EeUiButton {
    pub(crate) host: Host,
    pub(crate) f: RefCell<ButtonFields>,
    pub(crate) sub: RefCell<Sub>,
}

/// The button as round 3's state machine sees it: the record copied in and out around every op, and a `SetText`'s
/// text and font (the label's, `checkobj.rs`).
pub(crate) struct Btn<'a> {
    pub(crate) o: &'a EeUiButton,
    /// A `SetText`'s `const UWideString*` and font (the label's create and text ops).
    pub(crate) text: isize,
    pub(crate) font: isize,
}

impl<'a> Btn<'a> {
    pub(crate) fn new(o: &'a EeUiButton) -> Self {
        Btn { o, text: 0, font: 0 }
    }
    fn op(&self, op: Op, a: isize) -> isize {
        self.o.host.op(op, a, 0, 0, 0)
    }
}

impl ButtonHost for Btn<'_> {
    fn load(&self) -> ButtonState {
        let f = *self.o.f.borrow();
        ButtonState {
            f,
            border_type: self.op(Op::BorderType, 0) as c_ulong,
            rect: self.o.host.rect(),
            border: self.op(Op::BorderThickness, 0) as c_ulong,
            index: self.op(Op::Index, 0) as c_ulong,
        }
    }
    fn store(&mut self, s: &ButtonState) {
        *self.o.f.borrow_mut() = s.f;
        // the C++ wrote mBorderType every time; only a change is visible
        if self.op(Op::BorderType, 0) as c_ulong != s.border_type {
            self.op(Op::SetBorderType, s.border_type as isize);
        }
    }
    fn tab_stop(&self) -> c_ulong {
        self.op(Op::TabStop, 0) as c_ulong
    }
    fn tabbing_on(&self) -> bool {
        self.op(Op::TabbingOn, 0) != 0
    }
    fn set_tab_stop(&mut self, index: c_ulong) {
        self.op(Op::SetTabStop, index as isize);
    }
    fn tabable(&self) -> bool {
        self.op(Op::Tabable, 0) != 0
    }
    fn set_dirty(&mut self) {
        self.op(Op::SetDirty, 1);
    }
    fn play(&mut self, sound: c_ulong) {
        self.op(Op::SoundPlay, sound as isize);
    }
    fn capture_mouse(&mut self) {
        self.op(Op::CaptureMouse, 0);
    }
    fn release_mouse(&mut self) {
        self.op(Op::ReleaseMouse, 0);
    }
    fn capture_keyboard(&mut self) {
        self.op(Op::CaptureKeyboard, 0);
    }
    fn release_keyboard(&mut self) {
        self.op(Op::ReleaseKeyboard, 0);
    }
    fn fire(&mut self, event: c_ulong) {
        self.op(Op::FireTrigger, event as isize);
    }
    fn mouse_location(&self) -> UiPoint {
        self.o.host.mouse()
    }
    fn has_mouse_capture(&self) -> bool {
        self.op(Op::HasMouseCapture, 0) != 0
    }
    fn right_button_down(&self) -> bool {
        self.op(Op::RightButtonDown, 0) != 0
    }
    fn key(&mut self, key: Key) -> bool {
        let k = match key {
            Key::SpaceJustPressed => 0,
            Key::EnterJustPressed => 1,
            Key::SpaceJustUnpressed => 2,
            Key::EnterJustUnpressed => 3,
        };
        self.op(Op::Key, k) != 0
    }
    fn button_down(&mut self) {
        self.op(Op::ButtonDown, 0);
    }
    fn button_up(&mut self) {
        self.op(Op::ButtonUp, 0);
    }
    fn mouse_over(&mut self) {
        self.op(Op::ButtonMouseOver, 0);
    }
    fn mouse_move(&mut self, at: UiPoint) {
        self.o.host.op_point(Op::ButtonMouseMove, 0, at);
    }
    fn base_set_location(&mut self, at: UiPoint) {
        self.o.host.op_point(Op::BaseSetLocation, 0, at);
    }
    fn base_copy_data(&mut self) -> c_long {
        self.op(Op::BaseCopyData, 0) as c_long
    }
    fn set_copied_bitmap(&mut self, bitmap: *mut c_void) {
        let old = self.o.f.borrow().copied_bitmap;
        if !old.is_null() {
            self.op(Op::BitmapRelease, old as isize);
        }
        self.o.f.borrow_mut().copied_bitmap = bitmap;
        if !bitmap.is_null() {
            self.op(Op::BitmapReference, bitmap as isize);
        }
    }
    fn text(&self) -> Vec<u8> {
        // only its emptiness is read (`copy_data`): the length stands for the bytes
        let text = self.o.f.borrow().text;
        if self.op(Op::TextLength, text as isize) == 0 {
            vec![0]
        } else {
            vec![1, 0]
        }
    }
    fn text_extent(&self) -> (f32, f32) {
        let text = self.o.f.borrow().text;
        let p = self.o.host.get_point(Op::TextExtent, text as isize);
        (p.x, p.y)
    }
    fn copy_text(&mut self, focused: bool) {
        let f = *self.o.f.borrow();
        self.o.host.op(Op::TextCopy, f.copied_text as isize, f.text as isize, 0, 0);
        if focused {
            let c = f.focused_text_color;
            let mut b = [c.red, c.green, c.blue, 0.0];
            self.o.host.op_f(Op::TextSetFgColor, f.copied_text as isize, 0, 0, 0, &mut b);
        }
    }
}

/// Which of the three bitmaps a setter replaces.
#[derive(Clone, Copy)]
pub(crate) enum Which {
    Normal,
    Focused,
    Pressed,
}

impl EeUiButton {
    /// The constructor's part after `UIControl`'s (the C++ set `mBorderType` to 1): round 3's record, then the
    /// normal, pressed (the normal one when none) and focused bitmaps referenced in that order.
    pub fn new(host: Host, normal: *mut c_void, focused: *mut c_void, pressed: *mut c_void) -> Self {
        let rect = host.rect();
        let border = host.op(Op::BorderThickness, 0, 0, 0, 0) as c_ulong;
        let mut f = button::init(&rect, border);
        let reference = |b: *mut c_void| {
            if !b.is_null() {
                host.op(Op::BitmapReference, b as isize, 0, 0, 0);
            }
        };
        f.normal_bitmap = normal;
        reference(normal);
        f.pressed_bitmap = if pressed.is_null() { normal } else { pressed };
        reference(f.pressed_bitmap);
        f.focused_bitmap = focused;
        reference(focused);
        EeUiButton { host, f: RefCell::new(f), sub: RefCell::new(Sub::Plain) }
    }

    pub(crate) fn fields(&self) -> ButtonFields {
        *self.f.borrow()
    }

    pub(crate) fn op(&self, op: Op, a: isize, b: isize) -> isize {
        self.host.op(op, a, b, 0, 0)
    }

    /// The destructor: the subclass's part first (its destructor ran before `UIButton`'s), then the texts deleted,
    /// the four bitmaps' references and the two sounds released.
    pub fn release(&self) {
        self.release_sub();
        self.release_variant();
        let f = self.fields();
        if !f.text.is_null() {
            self.op(Op::TextDelete, f.text as isize, 0);
            self.op(Op::TextDelete, f.copied_text as isize, 0);
        }
        for b in [f.normal_bitmap, f.pressed_bitmap, f.focused_bitmap, f.copied_bitmap] {
            if !b.is_null() {
                self.op(Op::BitmapRelease, b as isize, 0);
            }
        }
        for s in [f.button_down_sound, f.button_up_sound] {
            if !button::is_none32(s) {
                self.op(Op::SoundUnload, s as isize, 0);
            }
        }
    }

    /// `DrawPolygons`: the control, then the copied bitmap at its location.
    pub fn draw_polygons(&self) -> isize {
        let mut r = self.op(Op::BaseDrawPolygons, 0, 0);
        let f = self.fields();
        if !f.copied_bitmap.is_null() {
            let bitmap = f.copied_bitmap as isize;
            self.host.op_point(Op::BitmapSetLocation, bitmap, f.bitmap_location);
            r = self.op(Op::BitmapDraw, bitmap, 0);
        }
        r
    }

    /// `DrawTextA`: the control's text, then the copied text clipped to its rectangle.
    pub fn draw_text(&self) -> isize {
        let mut r = self.op(Op::BaseDrawText, 0, 0);
        let f = self.fields();
        if f.has_copied_text {
            let text = f.copied_text as isize;
            self.host.op_point(Op::TextSetLocation, text, f.copied_text_rect.top_left());
            r = self.host.op_rect(Op::DrawClippedText, text, 0, f.copied_text_rect);
        }
        r
    }

    /// `SetText`: the two texts made with the font on first use, the text set, the button dirtied.
    pub fn set_text(&self, text: isize, font: isize) {
        if self.fields().text.is_null() {
            let t = self.op(Op::TextCreate, font, 0) as *mut c_void;
            self.f.borrow_mut().text = t;
            let c = self.op(Op::TextCreate, font, 0) as *mut c_void;
            self.f.borrow_mut().copied_text = c;
        }
        let t = self.fields().text as isize;
        self.op(Op::TextSetText, t, text);
        self.op(Op::SetDirty, 1, 0);
    }

    /// `ChangeFont`: both texts' font, when there is a text.
    pub fn change_font(&self, font: isize) {
        let f = self.fields();
        if !f.text.is_null() {
            self.op(Op::TextSetFont, f.text as isize, font);
            self.op(Op::TextSetFont, f.copied_text as isize, font);
        }
    }

    /// `SetButtonDownSound` / `SetButtonUpSound`: the old sample unloaded, the new one loaded.
    pub fn set_sound(&self, up: bool, name: isize) {
        let old = if up { self.fields().button_up_sound } else { self.fields().button_down_sound };
        if !button::is_none32(old) {
            self.op(Op::SoundUnload, old as isize, 0);
        }
        let id = self.op(Op::SoundLoad, name, 0) as c_ulong;
        let mut f = self.f.borrow_mut();
        if up {
            f.button_up_sound = id;
        } else {
            f.button_down_sound = id;
        }
    }

    /// The bitmap setters: the old reference released, the new one taken, then (`Set*Bitmap`) the button dirtied.
    pub(crate) fn set_bitmap(&self, which: Which, bitmap: *mut c_void, dirty: bool) {
        let get = |f: &ButtonFields| match which {
            Which::Normal => f.normal_bitmap,
            Which::Focused => f.focused_bitmap,
            Which::Pressed => f.pressed_bitmap,
        };
        let old = get(&self.fields());
        if !old.is_null() {
            self.op(Op::BitmapRelease, old as isize, 0);
        }
        {
            let mut f = self.f.borrow_mut();
            match which {
                Which::Normal => f.normal_bitmap = bitmap,
                Which::Focused => f.focused_bitmap = bitmap,
                Which::Pressed => f.pressed_bitmap = bitmap,
            }
        }
        if !bitmap.is_null() {
            self.op(Op::BitmapReference, bitmap as isize, 0);
        }
        if dirty {
            self.op(Op::SetDirty, 1, 0);
        }
    }

    /// `GetCurrentBitmap`: the state's bitmap (none for an unknown state).
    pub fn current_bitmap(&self) -> *mut c_void {
        let f = self.fields();
        match f.state {
            STATE_NORMAL => f.normal_bitmap,
            STATE_FOCUSED => f.focused_bitmap,
            STATE_PRESSED => f.pressed_bitmap,
            _ => core::ptr::null_mut(),
        }
    }

    /// One inward call; `f` points at four floats (or is NULL for the calls without).
    ///
    /// # Safety
    /// `f` is NULL or four live floats.
    pub unsafe fn call(&self, call: EeUiButtonCall, a: isize, b: isize, f: *mut f32) -> isize {
        type C = EeUiButtonCall;
        // SAFETY: the contract
        let buf = unsafe { read_buf(f) };
        let at = UiPoint { x: buf[0], y: buf[1] };
        let mut h = Btn::new(self);
        let fields = self.fields();
        match call {
            C::CopyData => return button::copy_data(&mut h) as isize,
            C::DrawPolygons => return self.draw_polygons(),
            C::DrawText => return self.draw_text(),
            C::ButtonDown => button::button_down(&mut h),
            C::ButtonUp => button::button_up(&mut h),
            C::MouseLButtonDown => return button::mouse_l_button_down(&mut h) as isize,
            C::MouseLButtonUp => return button::mouse_l_button_up(&mut h) as isize,
            C::MouseMove => return button::mouse_move(&mut h, at) as isize,
            C::MouseOver => button::mouse_over(&mut h),
            C::TabbedTo => button::tabbed_to(&mut h),
            C::TabbedFrom => button::tabbed_from(&mut h),
            C::UpdateKeyboard => return button::update_keyboard(&mut h) as isize,
            C::SetLocation => button::set_location(&mut h, at),
            C::SetText => self.set_text(a, b),
            C::ChangeFont => self.change_font(a),
            C::SetButtonDownSound => self.set_sound(false, a),
            C::SetButtonUpSound => self.set_sound(true, a),
            C::ButtonDownSound => return fields.button_down_sound as isize,
            C::ButtonUpSound => return fields.button_up_sound as isize,
            C::SetNormalBitmap => self.set_bitmap(Which::Normal, a as *mut c_void, true),
            C::SetFocusedBitmap => self.set_bitmap(Which::Focused, a as *mut c_void, true),
            C::SetPressedBitmap => self.set_bitmap(Which::Pressed, a as *mut c_void, true),
            C::NormalBitmap => return fields.normal_bitmap as isize,
            C::FocusedBitmap => return fields.focused_bitmap as isize,
            C::PressedBitmap => return fields.pressed_bitmap as isize,
            C::CurrentBitmap => return self.current_bitmap() as isize,
            C::CopiedBitmap => return fields.copied_bitmap as isize,
            C::Text => return fields.text as isize,
            C::IsButtonDown => return (fields.mouse_down || fields.key_down) as isize,
            C::State => return fields.state as isize,
            C::BitmapLocation => {
                let p = fields.bitmap_location;
                // SAFETY: the contract
                unsafe { write_buf(f, [p.x, p.y, buf[2], buf[3]]) };
            }
            C::SetBitmapDrawOffset => self.f.borrow_mut().bitmap_draw_offset = at,
            C::SetFocusedTextColor => {
                self.f.borrow_mut().focused_text_color = UiColor { red: buf[0], green: buf[1], blue: buf[2] }
            }
            C::SetTextRectangle => {
                self.f.borrow_mut().text_rect = UiRect { bottom: buf[0], left: buf[1], right: buf[2], top: buf[3] }
            }
            C::ReplaceNormalBitmap => self.set_bitmap(Which::Normal, a as *mut c_void, false),
            C::ReplacePressedBitmap => self.set_bitmap(Which::Pressed, a as *mut c_void, false),
            C::DualMouseRButtonDown => return button::dual_mouse_r_button_down(&mut h) as isize,
            C::DualMouseRButtonUp => return button::dual_mouse_r_button_up(&mut h) as isize,
            _ => return self.call_sub(call, a, b, buf),
        }
        0
    }
}

// ---- the C ABI

/// A new Rust button for the C++ `button` (its `UIControl` part constructed), its calls out through `host`.
#[no_mangle]
pub extern "C" fn ee_ui_button_new(
    button: *mut c_void,
    host: EeUiHostFn,
    normal: *mut c_void,
    focused: *mut c_void,
    pressed: *mut c_void,
) -> *mut EeUiButton {
    crate::direct::made(
        crate::direct::Kind::Button,
        button,
        Box::into_raw(Box::new(EeUiButton::new(Host { obj: button, f: host }, normal, focused, pressed))),
    )
}

/// The destructor: releases the texts, bitmaps and sounds and frees the object.
///
/// # Safety
/// `button` came from `ee_ui_button_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_button_free(button: *mut EeUiButton) {
    crate::direct::forget(crate::direct::Kind::Button, button as *const core::ffi::c_void);
    if button.is_null() {
        return;
    }
    // SAFETY: the contract
    unsafe { (*button).release() };
    // SAFETY: the contract
    drop(unsafe { Box::from_raw(button) });
}

/// One of UIButton's methods ([`EeUiButtonCall`]).
///
/// # Safety
/// `button` is live; `f` is NULL or four live floats.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_button_call(
    button: *const EeUiButton,
    call: EeUiButtonCall,
    a: isize,
    b: isize,
    f: *mut f32,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out re-enter through this function
    unsafe { (*button).call(call, a, b, f) }
}

#[cfg(test)]
#[path = "buttonobj_tests.rs"]
mod tests;
