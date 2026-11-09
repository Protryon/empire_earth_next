//! The UI toolkit's buttons (`User Interface/UIButton.cpp`, `UIDualButton.cpp`, `UIScrollButton.cpp`,
//! `UICheckBox.cpp`, `UIRadioButton.cpp`): the three-state button (normal, focused, pressed) driven by the mouse, the
//! keyboard and tabbing, what its draw pass shows (`CopyData`'s bitmap choice and centred text), and the check box's
//! and radio button's check logic and label geometry.
//!
//! The C++ objects keep their members. [`ButtonFields`] is `UIButton`'s own record (from `mTextRectangle` on),
//! [`CheckFields`] and [`RadioFields`] the subclasses' records; the `UIControl` members the buttons read or write
//! (`mBorderType`, `mRectangle`, `mBorderThickness`, `mIndex`) travel in [`ButtonState`]. Everything that touches
//! another object stays C++ and is reached through the host traits: the form's tab stop, mouse and keyboard capture,
//! the trigger events, sounds, the key states, the bitmaps' reference counts, the texts and labels. The virtual
//! members a button calls on itself (`ButtonDown`, `ButtonUp`, `MouseOver`, `MouseMove`, `SetDirty`, the capture
//! calls) are host calls too, so a subclass's override still runs; every host call with side effects stores the
//! records before it and reads them back after it (a trigger event can reach the same button).

use core::ffi::{c_long, c_ulong, c_void};

use crate::rect::{UiPoint, UiRect};

/// `kButtonStateNormal`.
pub(crate) const STATE_NORMAL: c_ulong = 0;
/// `kButtonStateFocused`.
pub(crate) const STATE_FOCUSED: c_ulong = 1;
/// `kButtonStatePressed`.
pub(crate) const STATE_PRESSED: c_ulong = 2;

/// `kControlTriggerChecked`.
pub(crate) const TRIGGER_CHECKED: c_ulong = 0x3e9;
/// `kControlTriggerUnchecked`.
pub(crate) const TRIGGER_UNCHECKED: c_ulong = 0x3ea;
/// `kControlTriggerButtonDown`.
pub(crate) const TRIGGER_BUTTON_DOWN: c_ulong = 0x3ef;
/// `kControlTriggerButtonUp`.
pub(crate) const TRIGGER_BUTTON_UP: c_ulong = 0x3f0;
/// `kControlTriggerRightButtonDown`.
pub(crate) const TRIGGER_RIGHT_BUTTON_DOWN: c_ulong = 0x3f5;
/// `kControlTriggerRightButtonUp`.
pub(crate) const TRIGGER_RIGHT_BUTTON_UP: c_ulong = 0x3f6;

/// `kScrollBarWidth`: the check box's and radio button's bitmap column.
pub(crate) const SCROLL_BAR_WIDTH: f32 = 20.0;

/// The sound manager's "no sample" (-1).
const NO_SOUND: c_ulong = c_ulong::MAX;

/// `UIsNone32`: the low 32 bits all set (a -1 in memory has 64 on LP64).
#[allow(clippy::unnecessary_cast)] // c_ulong is u64 on LP64
fn is_none32(v: c_ulong) -> bool {
    v as u32 == u32::MAX
}

/// `GEColor`: red, green, blue.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct UiColor {
    pub red: f32,
    pub green: f32,
    pub blue: f32,
}

/// `UIButton`'s members in their C++ order (the record starts at `mTextRectangle`). The pointers are C++'s and
/// never dereferenced here.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct ButtonFields {
    pub text_rect: UiRect,
    pub bitmap_location: UiPoint,
    pub pressed_bitmap: *mut c_void,
    pub normal_bitmap: *mut c_void,
    pub focused_bitmap: *mut c_void,
    pub copied_bitmap: *mut c_void,
    pub state: c_ulong,
    pub copied_text_rect: UiRect,
    pub bitmap_draw_offset: UiPoint,
    pub focused_text_color: UiColor,
    pub button_down_sound: c_ulong,
    pub button_up_sound: c_ulong,
    pub text: *mut c_void,
    pub copied_text: *mut c_void,
    pub key_down: bool,
    pub mouse_down: bool,
    pub has_copied_text: bool,
}

/// A button's record and the `UIControl` members it uses.
#[derive(Clone, Copy, Debug)]
pub struct ButtonState {
    pub f: ButtonFields,
    /// `mBorderType` (written).
    pub border_type: c_ulong,
    /// `mRectangle` (read).
    pub rect: UiRect,
    /// `mBorderThickness` (read).
    pub border: c_ulong,
    /// `mIndex` (read).
    pub index: c_ulong,
}

/// The keys `UpdateKeyboard` asks about.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Key {
    SpaceJustPressed,
    EnterJustPressed,
    SpaceJustUnpressed,
    EnterJustUnpressed,
}

/// A button's way back into its C++ object.
pub trait ButtonHost {
    /// Reads the record.
    fn load(&self) -> ButtonState;
    /// Writes the record (`mBorderType` and `UIButton`'s members).
    fn store(&mut self, s: &ButtonState);
    /// The form's `GetTabStop()`.
    fn tab_stop(&self) -> c_ulong;
    /// The form's `GetTabbingOn()`.
    fn tabbing_on(&self) -> bool;
    /// The form's `SetTabStop(index)`.
    fn set_tab_stop(&mut self, index: c_ulong);
    /// The virtual `GetTabable()`.
    fn tabable(&self) -> bool;
    /// The virtual `SetDirty(true)`.
    fn set_dirty(&mut self);
    /// The sound manager's `Play(sound, 1, 0, 1, false, NULL)`.
    fn play(&mut self, sound: c_ulong);
    /// The virtual `CaptureMouse()`.
    fn capture_mouse(&mut self);
    /// The virtual `ReleaseCapturedMouse()`.
    fn release_mouse(&mut self);
    /// `CaptureKeyboard()`.
    fn capture_keyboard(&mut self);
    /// The virtual `ReleaseCapturedKeyboard()`.
    fn release_keyboard(&mut self);
    /// `FireTriggerEvent(event)`.
    fn fire(&mut self, event: c_ulong);
    /// `GetMouseLocation()`.
    fn mouse_location(&self) -> UiPoint;
    /// `GetHasMouseCapture()`.
    fn has_mouse_capture(&self) -> bool;
    /// `GetRightMouseButtonDown()`.
    fn right_button_down(&self) -> bool;
    /// A key's `JustPressed()` / `JustUnpressed()` (virtual calls on the key entities).
    fn key(&mut self, key: Key) -> bool;
    /// The virtual `ButtonDown()`.
    fn button_down(&mut self);
    /// The virtual `ButtonUp()`.
    fn button_up(&mut self);
    /// The virtual `MouseOver()`.
    fn mouse_over(&mut self);
    /// The virtual `MouseMove(at)`.
    fn mouse_move(&mut self, at: UiPoint);
    /// `UIControl::SetLocation(at)` (moves `mRectangle`, dirties the control).
    fn base_set_location(&mut self, at: UiPoint);
    /// `UIControl::CopyData()`.
    fn base_copy_data(&mut self) -> c_long;
    /// `mCopiedBitmap = bitmap`, dereferencing the old one and referencing the new one.
    fn set_copied_bitmap(&mut self, bitmap: *mut c_void);
    /// `mText->GetText()`'s bytes (NUL-terminated); only called when `mText` is set.
    fn text(&self) -> Vec<u8>;
    /// `mText->GetTextExtent()`: width and height.
    fn text_extent(&self) -> (f32, f32);
    /// `*mCopiedText = *mText`, then, when `focused`, `mCopiedText->SetFgColor(mFocusedTextColor)`.
    fn copy_text(&mut self, focused: bool);
}

/// The record held across host calls: written out before each, read back after.
struct Btn<'a, H: ButtonHost> {
    h: &'a mut H,
    s: ButtonState,
}

impl<'a, H: ButtonHost> Btn<'a, H> {
    fn new(h: &'a mut H) -> Self {
        let s = h.load();
        Btn { h, s }
    }

    fn finish(self) {
        self.h.store(&self.s);
    }

    fn call<R>(&mut self, f: impl FnOnce(&mut H) -> R) -> R {
        self.h.store(&self.s);
        let r = f(self.h);
        self.s = self.h.load();
        r
    }

    fn set_dirty(&mut self) {
        self.call(|h| h.set_dirty());
    }

    /// The form shows this button focused: tabbing is on and the tab stop is its index.
    fn focused_by_tab(&self) -> bool {
        self.h.tabbing_on() && self.h.tab_stop() == self.s.index
    }

    /// `ButtonUp`'s and `MouseMove`'s shared part: focused or normal (dirty on a change), the raised border.
    fn show_released(&mut self) {
        if self.focused_by_tab() {
            if self.s.f.state != STATE_FOCUSED {
                self.s.f.state = STATE_FOCUSED;
                self.set_dirty();
            }
        } else if self.s.f.state != STATE_NORMAL {
            self.s.f.state = STATE_NORMAL;
            self.set_dirty();
        }
        self.s.border_type = 1;
    }
}

/// The text rectangle: the control's rectangle inset by the border thickness (the constructor, `SetLocation`).
pub fn text_rect(rect: &UiRect, border: c_ulong) -> UiRect {
    let b = border as f32;
    UiRect::new(rect.left + b, rect.top + b, rect.right - b, rect.bottom - b)
}

/// The constructor's record after `UIControl`'s: no bitmaps (the C++ sets and references them next), no text, no
/// sounds, normal, the yellow focused text, the text rectangle inside the border. The members' default constructors
/// gave the same zero points and rectangles.
pub fn init(rect: &UiRect, border: c_ulong) -> ButtonFields {
    let null = core::ptr::null_mut();
    ButtonFields {
        text_rect: text_rect(rect, border),
        bitmap_location: UiPoint::default(),
        pressed_bitmap: null,
        normal_bitmap: null,
        focused_bitmap: null,
        copied_bitmap: null,
        state: STATE_NORMAL,
        copied_text_rect: UiRect::default(),
        bitmap_draw_offset: UiPoint::default(),
        focused_text_color: UiColor { red: 1.0, green: 1.0, blue: 0.0 },
        button_down_sound: NO_SOUND,
        button_up_sound: NO_SOUND,
        text: null,
        copied_text: null,
        key_down: false,
        mouse_down: false,
        has_copied_text: false,
    }
}

/// `UIButton::ButtonDown`: takes the tab stop if tabbable, shows the button pressed, plays the button-down sound.
pub fn button_down(h: &mut impl ButtonHost) {
    let mut b = Btn::new(h);
    if b.h.tab_stop() != b.s.index && b.h.tabable() {
        let i = b.s.index;
        b.call(|h| h.set_tab_stop(i));
    }
    if b.s.f.state != STATE_PRESSED {
        b.s.border_type = 0;
        b.s.f.state = STATE_PRESSED;
        b.set_dirty();
    }
    let sound = b.s.f.button_down_sound;
    if !is_none32(sound) {
        b.call(|h| h.play(sound));
    }
    b.finish();
}

/// `UIButton::ButtonUp`: focused or normal, the raised border, the button-up sound.
pub fn button_up(h: &mut impl ButtonHost) {
    let mut b = Btn::new(h);
    b.show_released();
    let sound = b.s.f.button_up_sound;
    if !is_none32(sound) {
        b.call(|h| h.play(sound));
    }
    b.finish();
}

/// `UIButton::MouseOver`: shows the button pressed.
pub fn mouse_over(h: &mut impl ButtonHost) {
    let mut b = Btn::new(h);
    if b.s.f.state != STATE_PRESSED {
        b.s.border_type = 0;
        b.s.f.state = STATE_PRESSED;
        b.set_dirty();
    }
    b.finish();
}

/// `UIButton::MouseLButtonDown`.
pub fn mouse_l_button_down(h: &mut impl ButtonHost) -> c_long {
    let mut b = Btn::new(h);
    b.s.f.mouse_down = true;
    b.call(|h| {
        h.capture_mouse();
        h.button_down();
        h.fire(TRIGGER_BUTTON_DOWN);
    });
    b.finish();
    0
}

/// `UIButton::MouseLButtonUp`: a release over the button clicks it.
pub fn mouse_l_button_up(h: &mut impl ButtonHost) -> c_long {
    let mut b = Btn::new(h);
    let at = b.h.mouse_location();
    b.call(|h| h.mouse_move(at));
    b.s.f.mouse_down = false;
    b.s.f.key_down = false;
    b.call(|h| {
        h.release_mouse();
        h.release_keyboard();
    });
    if b.s.rect.contains(b.h.mouse_location()) {
        b.call(|h| {
            h.button_up();
            h.fire(TRIGGER_BUTTON_UP);
        });
    }
    b.finish();
    0
}

/// `UIButton::MouseMove`: while held, pressed over the button (or while a key holds it), else focused or normal.
pub fn mouse_move(h: &mut impl ButtonHost, at: UiPoint) -> c_long {
    let mut b = Btn::new(h);
    if !(b.s.f.mouse_down || b.h.has_mouse_capture() && b.h.right_button_down()) {
        return 0;
    }
    if b.s.rect.contains(at) || b.s.f.key_down {
        b.call(|h| h.mouse_over());
    } else {
        b.show_released();
    }
    b.finish();
    0
}

/// `UIButton::TabbedTo`.
pub fn tabbed_to(h: &mut impl ButtonHost) {
    let mut b = Btn::new(h);
    b.s.f.state = STATE_FOCUSED;
    b.set_dirty();
    b.finish();
}

/// `UIButton::TabbedFrom`: normal, releasing the mouse, the keyboard and any press.
pub fn tabbed_from(h: &mut impl ButtonHost) {
    let mut b = Btn::new(h);
    b.s.f.state = STATE_NORMAL;
    b.call(|h| {
        h.release_mouse();
        h.release_keyboard();
    });
    b.s.f.mouse_down = false;
    b.s.f.key_down = false;
    b.set_dirty();
    b.finish();
}

/// `UIButton::UpdateKeyboard`: Space or Enter presses the button; their release after a key press on it clicks it.
pub fn update_keyboard(h: &mut impl ButtonHost) -> c_long {
    let mut b = Btn::new(h);
    if b.call(|h| h.key(Key::SpaceJustPressed) || h.key(Key::EnterJustPressed)) {
        b.s.f.key_down = true;
        b.call(|h| {
            h.capture_keyboard();
            h.button_down();
        });
        if !b.s.f.mouse_down {
            b.call(|h| h.fire(TRIGGER_BUTTON_DOWN));
        }
    } else if b.s.f.key_down && b.call(|h| h.key(Key::SpaceJustUnpressed) || h.key(Key::EnterJustUnpressed)) {
        b.s.f.mouse_down = false;
        b.call(|h| {
            h.release_mouse();
            h.release_keyboard();
        });
        b.s.f.key_down = false;
        b.call(|h| {
            h.button_up();
            h.fire(TRIGGER_BUTTON_UP);
        });
    }
    b.finish();
    0
}

/// `UIButton::SetLocation`: moves the control, then its text rectangle.
pub fn set_location(h: &mut impl ButtonHost, at: UiPoint) {
    let mut b = Btn::new(h);
    b.call(|h| h.base_set_location(at));
    b.s.f.text_rect = text_rect(&b.s.rect, b.s.border);
    b.finish();
}

/// `UIButton::CopyData`: the bitmap for the state, its location, and the text centred in the text rectangle (one
/// pixel down and right when pressed), coloured when focused or pressed.
pub fn copy_data(h: &mut impl ButtonHost) -> c_long {
    let mut b = Btn::new(h);
    let result = b.call(|h| h.base_copy_data());
    let f = &b.s.f;
    let bitmap = match f.state {
        STATE_NORMAL => f.normal_bitmap,
        STATE_FOCUSED => {
            if f.focused_bitmap.is_null() {
                f.normal_bitmap
            } else {
                f.focused_bitmap
            }
        }
        STATE_PRESSED => f.pressed_bitmap,
        // an unknown state keeps the copied bitmap (the C++ switch has no default), dereferenced and re-referenced
        _ => f.copied_bitmap,
    };
    b.call(|h| h.set_copied_bitmap(bitmap));
    let border = b.s.border as f32;
    b.s.f.bitmap_location.x = b.s.f.bitmap_draw_offset.x + b.s.rect.left + border;
    b.s.f.bitmap_location.y = b.s.f.bitmap_draw_offset.y + b.s.rect.top + border;
    let has_text = !b.s.f.text.is_null() && !crate::text::is_empty(&b.h.text());
    if has_text {
        let mut r = b.s.f.text_rect;
        if b.s.f.state == STATE_PRESSED {
            let (left, top) = (r.left + 1.0, r.top + 1.0);
            r.top = top;
            r.left = left;
            let (right, bottom) = (r.right + 1.0, r.bottom + 1.0);
            r.bottom = bottom;
            r.right = right;
        }
        let (w, _) = b.h.text_extent();
        if w < r.width() {
            r.left = r.width() * 0.5 + r.left - w * 0.5;
        }
        let (_, ht) = b.h.text_extent();
        if ht < r.height() {
            r.top = r.height() * 0.5 + r.top - ht * 0.5;
        }
        b.s.f.copied_text_rect = r;
        let focused = b.s.f.state == STATE_FOCUSED || b.s.f.state == STATE_PRESSED;
        b.call(|h| h.copy_text(focused));
        b.s.f.has_copied_text = true;
    } else {
        b.s.f.has_copied_text = false;
    }
    b.finish();
    result
}

/// `UIDualButton::MouseRButtonDown`.
pub fn dual_mouse_r_button_down(h: &mut impl ButtonHost) -> c_long {
    h.capture_mouse();
    h.button_down();
    h.fire(TRIGGER_RIGHT_BUTTON_DOWN);
    0
}

/// `UIDualButton::MouseRButtonUp`: a right release over the button pops it up.
pub fn dual_mouse_r_button_up(h: &mut impl ButtonHost) -> c_long {
    let at = h.mouse_location();
    h.mouse_move(at);
    h.release_mouse();
    let rect = h.load().rect;
    if rect.contains(h.mouse_location()) {
        h.button_up();
        h.fire(TRIGGER_RIGHT_BUTTON_UP);
    }
    0
}

/// A scroll arrow's two polls and its slider.
pub trait ScrollButtonHost: ButtonHost {
    /// `mDecrement`.
    fn decrement(&self) -> c_ulong;
    /// `mDelayPoll->GetIsRunning()`.
    fn delay_running(&self) -> bool;
    /// `mDelayPoll->Poll()`.
    fn delay_poll(&mut self) -> bool;
    /// `mDelayPoll->Start()`.
    fn delay_start(&mut self);
    /// `mDelayPoll->Stop()`.
    fn delay_stop(&mut self);
    /// `mRepeatPoll->GetIsRunning()`.
    fn repeat_running(&self) -> bool;
    /// `mRepeatPoll->Poll()`.
    fn repeat_poll(&mut self) -> bool;
    /// `mRepeatPoll->Start()`.
    fn repeat_start(&mut self);
    /// `mSlider->GetCurrentRangeValue()`.
    fn slider_value(&self) -> c_long;
    /// `mSlider->SetCurrentRangeValue(value)`.
    fn set_slider_value(&mut self, value: c_long);
}

/// `UIScrollButton::ButtonDown`: steps the slider on the press, again after the delay, then at every repeat; then
/// `UIButton::ButtonDown`.
pub fn scroll_button_down(h: &mut impl ScrollButtonHost) {
    let step: c_long = if h.decrement() != 0 { -1 } else { 1 };
    if h.delay_running() {
        if h.delay_poll() {
            h.repeat_start();
            h.delay_stop();
            let v = h.slider_value().wrapping_add(step);
            h.set_slider_value(v);
        }
    } else if h.repeat_running() {
        if h.repeat_poll() {
            let v = h.slider_value().wrapping_add(step);
            h.set_slider_value(v);
        }
    } else {
        h.delay_start();
        let v = h.slider_value().wrapping_add(step);
        h.set_slider_value(v);
    }
    button_down(h);
}

/// The check box's and radio button's bitmap rectangle (their constructors' `UIButton` argument): a box at most 19
/// pixels square at the top-left, unless the bitmap scales.
pub fn box_rect(rect: &UiRect, scale_bitmap: bool) -> UiRect {
    let right = if !scale_bitmap && rect.width() <= SCROLL_BAR_WIDTH { rect.left + 19.0 } else { rect.right };
    let bottom = if !scale_bitmap && rect.height() <= SCROLL_BAR_WIDTH { rect.top + 19.0 } else { rect.bottom };
    UiRect::new(rect.left, rect.top, right, bottom)
}

/// `UICheckBox`'s members in their C++ order (from `mTextLabel` on).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct CheckFields {
    /// `UITextLabel*`, C++'s.
    pub text_label: *mut c_void,
    pub is_checked: bool,
    pub has_text: bool,
    pub scale_bitmap: bool,
    pub is_disabled: bool,
}

/// `UIRadioButton`'s members in their C++ order (from `mOtherNormalBitmap` on).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct RadioFields {
    pub other_normal_bitmap: *mut c_void,
    pub other_pressed_bitmap: *mut c_void,
    /// `UITextLabel*`, C++'s.
    pub text_label: *mut c_void,
    pub is_checked: bool,
    pub has_text: bool,
    pub is_push_button: bool,
    pub scale_bitmap: bool,
}

/// What the check box and the radio button share: a label beside the box.
pub trait LabelHost: ButtonHost {
    /// `mTextLabel->SetText(text, true)`.
    fn label_set_text(&mut self);
    /// `mTextLabel = new UITextLabel(text, font, id, rect, form, 0, index, 0)`.
    fn label_create(&mut self, rect: UiRect);
    /// `mTextLabel->SetLocation(at)`.
    fn label_set_location(&mut self, at: UiPoint);
}

/// A check box's way back into its C++ object.
pub trait CheckHost: LabelHost {
    /// Reads `UICheckBox`'s record.
    fn load_check(&self) -> CheckFields;
    /// Writes `UICheckBox`'s record.
    fn store_check(&mut self, c: &CheckFields);
    /// `SetNormalBitmap`/`SetPressedBitmap` with the manager's check bitmaps, on or off (each dirties the box).
    fn set_check_bitmaps(&mut self, on: bool);
}

/// A radio button's way back into its C++ object.
pub trait RadioHost: LabelHost {
    /// Reads `UIRadioButton`'s record.
    fn load_radio(&self) -> RadioFields;
    /// Writes `UIRadioButton`'s record.
    fn store_radio(&mut self, r: &RadioFields);
    /// `Check`'s and `UnCheck`'s bitmap swap: the normal and pressed bitmaps trade places with the other check
    /// state's (`SetNormalBitmap`/`SetPressedBitmap` dirty the button), then `SetDirty(true)`.
    fn swap_bitmaps(&mut self);
    /// The form's `GetControlCount()`.
    fn control_count(&self) -> c_long;
    /// The form's control `i` exists and is a radio button.
    fn is_radio(&self, i: c_long) -> bool;
    /// That radio button's `mIsChecked`.
    fn radio_checked(&self, i: c_long) -> bool;
    /// That radio button's `UnCheck()`.
    fn uncheck_radio(&mut self, i: c_long);
}

/// The label's left edge: after the scaled bitmap (as wide as the box is high) or the 20-pixel column.
fn label_left(rect: &UiRect, border: c_ulong, scale_bitmap: bool) -> f32 {
    if scale_bitmap {
        rect.height() + rect.left + border as f32
    } else {
        rect.left + border as f32 + SCROLL_BAR_WIDTH
    }
}

/// The check box's and radio button's bitmap offset (their constructors): centred vertically in a box taller than
/// the bitmap column. `None` keeps the offset. `float_border`: the check box doubles the thickness as a float, the
/// radio button as an integer.
#[allow(clippy::neg_cmp_op_on_partial_ord)] // NaN keeps the offset, as the C++'s `height > 20` test
pub fn label_bitmap_offset(height: f32, border: c_ulong, scale_bitmap: bool, float_border: bool) -> Option<UiPoint> {
    if scale_bitmap || !(height > SCROLL_BAR_WIDTH) {
        return None;
    }
    let twice = if float_border { 2.0 * border as f32 } else { border.wrapping_mul(2) as f32 };
    Some(UiPoint { x: 0.0, y: (height - twice) * 0.5 - 10.0 })
}

/// `SetLocation`'s label place (check box and radio button); `label` reads the label pointer and the scaling flag
/// after the move.
fn move_label<H: LabelHost>(h: &mut H, at: UiPoint, label: impl Fn(&H) -> (bool, bool)) {
    h.base_set_location(at);
    let (has_label, scale_bitmap) = label(h);
    if !has_label {
        return;
    }
    let s = h.load();
    let x = label_left(&s.rect, s.border, scale_bitmap);
    let y = s.rect.top + s.border as f32;
    h.label_set_location(UiPoint { x, y });
}

/// `UICheckBox::SetChecked`: unless disabled, the check bitmaps and the checked or unchecked trigger.
pub fn check_set_checked(h: &mut impl CheckHost, checked: bool) {
    let mut c = h.load_check();
    if c.is_disabled {
        return;
    }
    c.is_checked = checked;
    h.store_check(&c);
    h.set_check_bitmaps(checked);
    let c = h.load_check();
    h.fire(if c.is_checked { TRIGGER_CHECKED } else { TRIGGER_UNCHECKED });
}

/// `UICheckBox::ToggleCheck`.
pub fn check_toggle(h: &mut impl CheckHost) {
    let c = h.load_check();
    if !c.is_disabled {
        check_set_checked(h, !c.is_checked);
    }
}

/// `UICheckBox::SetDisabled`: a disabled box shows pressed, an enabled one normal.
pub fn check_set_disabled(h: &mut impl CheckHost, disabled: bool) {
    let mut c = h.load_check();
    c.is_disabled = disabled;
    h.store_check(&c);
    let mut s = h.load();
    if !disabled {
        if s.f.state == STATE_PRESSED {
            s.border_type = 1;
            s.f.state = STATE_NORMAL;
            h.store(&s);
            h.set_dirty();
        }
        return;
    }
    if s.f.state == STATE_PRESSED {
        return;
    }
    s.border_type = 0;
    s.f.state = STATE_PRESSED;
    h.store(&s);
    h.set_dirty();
}

/// `UICheckBox::SetLocation`.
pub fn check_set_location(h: &mut impl CheckHost, at: UiPoint) {
    move_label(h, at, |h| {
        let c = h.load_check();
        (!c.text_label.is_null(), c.scale_bitmap)
    });
}

/// `UICheckBox::SetText` (`text`: the new text's bytes, NUL-terminated): the label's text, or a new label beside the
/// box; empty text hides the label.
pub fn check_set_text(h: &mut impl CheckHost, text: &[u8]) {
    let mut c = h.load_check();
    if crate::text::is_empty(text) {
        c.has_text = false;
        h.store_check(&c);
        return;
    }
    if !c.text_label.is_null() {
        h.label_set_text();
        h.set_dirty();
    } else {
        let s = h.load();
        let (r, b) = (s.rect, s.border as f32);
        let left = label_left(&r, s.border, c.scale_bitmap);
        let bottom = r.height() - s.border.wrapping_mul(2) as f32 + r.top;
        h.label_create(UiRect::new(left, r.top + b, r.right - b, bottom));
    }
    let mut c = h.load_check();
    c.has_text = true;
    h.store_check(&c);
    h.set_dirty();
}

/// The check box's mouse and button handlers do nothing while it is disabled.
pub fn check_enabled(h: &impl CheckHost) -> bool {
    !h.load_check().is_disabled
}

/// `UIRadioButton::RemoveOtherChecks`: unchecks the first checked radio button among the adjacent radio controls
/// before, then after, this one.
pub fn radio_remove_other_checks(h: &mut impl RadioHost) {
    let index = h.load().index as c_long;
    let mut i = index.wrapping_sub(1);
    while i >= 0 {
        if !h.is_radio(i) {
            break;
        }
        if h.radio_checked(i) {
            h.uncheck_radio(i);
            return;
        }
        i -= 1;
    }
    let mut i = index.wrapping_add(1);
    while i < h.control_count() {
        if !h.is_radio(i) {
            break;
        }
        if h.radio_checked(i) {
            h.uncheck_radio(i);
            return;
        }
        i += 1;
    }
}

/// `UIRadioButton::Check`: unchecks the checked neighbour, swaps to the checked bitmaps, a push button stays
/// pressed, fires the checked trigger.
pub fn radio_check(h: &mut impl RadioHost) {
    if h.load_radio().is_checked {
        return;
    }
    radio_remove_other_checks(h);
    let mut r = h.load_radio();
    r.is_checked = true;
    h.store_radio(&r);
    h.swap_bitmaps();
    let mut s = h.load();
    s.border_type = 0;
    if h.load_radio().is_push_button {
        s.f.state = STATE_PRESSED;
    }
    h.store(&s);
    h.fire(TRIGGER_CHECKED);
}

/// `UIRadioButton::UnCheck`: swaps to the unchecked bitmaps, a push button pops up, fires the unchecked trigger.
pub fn radio_uncheck(h: &mut impl RadioHost) {
    let mut r = h.load_radio();
    if !r.is_checked {
        return;
    }
    r.is_checked = false;
    h.store_radio(&r);
    h.swap_bitmaps();
    let mut s = h.load();
    if h.load_radio().is_push_button {
        s.f.state = STATE_NORMAL;
    }
    s.border_type = 1;
    h.store(&s);
    h.fire(TRIGGER_UNCHECKED);
}

/// The start of `UIRadioButton::CopyData`: a checked push button loses its border. Then `UIButton::CopyData`.
pub fn radio_copy_data(h: &mut impl RadioHost) -> c_long {
    let r = h.load_radio();
    if r.is_checked && r.is_push_button {
        let mut s = h.load();
        s.border_type = 0;
        h.store(&s);
    }
    copy_data(h)
}

/// `UIRadioButton::MouseLButtonDown`: a checked push button ignores the press.
pub fn radio_mouse_l_button_down(h: &mut impl RadioHost) -> c_long {
    let r = h.load_radio();
    if !r.is_push_button || !r.is_checked {
        mouse_l_button_down(h)
    } else {
        0
    }
}

/// `UIRadioButton::SetLocation`.
pub fn radio_set_location(h: &mut impl RadioHost, at: UiPoint) {
    move_label(h, at, |h| {
        let r = h.load_radio();
        (!r.text_label.is_null(), r.scale_bitmap)
    });
}

/// `UIRadioButton::SetText` (as the check box's, with the label reaching the box's right and bottom edges less the
/// border).
pub fn radio_set_text(h: &mut impl RadioHost, text: &[u8]) {
    let mut r = h.load_radio();
    if crate::text::is_empty(text) {
        r.has_text = false;
        h.store_radio(&r);
        return;
    }
    if !r.text_label.is_null() {
        h.label_set_text();
        h.set_dirty();
    } else {
        let s = h.load();
        let (q, b) = (s.rect, s.border as f32);
        let left = label_left(&q, s.border, r.scale_bitmap);
        let right = q.width() + q.left - b;
        let bottom = q.height() + q.top - b;
        h.label_create(UiRect::new(left, q.top + b, right, bottom));
    }
    let mut r = h.load_radio();
    r.has_text = true;
    h.store_radio(&r);
    h.set_dirty();
}

#[cfg(test)]
mod tests;
