//! The last three `UIButton` subclasses (`User Interface/UITriStateCheckBox.cpp`, `UIBlinkingButton.cpp`,
//! `UIScalableButton.cpp`): the three-state check box (off, on, "not") with its label beside the box, the blinking
//! button's fade (its opacities pulsed by a sine of the poll's time left) and the scalable button's three-slice draw
//! (a left cap, a right cap and a stretched middle cut from one bitmap).
//!
//! The C++ classes keep their members ([`TriStateFields`] from `mTextLabel` on, [`BlinkFields`] from `mBlinkPhase`
//! on); the tri-state box reaches its label, bitmaps and base class through [`TriStateHost`]. The blinking button's
//! `CopyData` and the scalable button's `CopyData`/`DrawPolygons` are pure: they return what the C++ then sets and
//! draws (`SetIsBlinking`, the poll and the GX calls stay C++).

use core::ffi::{c_long, c_ulong, c_void};

use crate::button::{
    box_rect, label_bitmap_offset, SCROLL_BAR_WIDTH, STATE_FOCUSED, STATE_NORMAL, STATE_PRESSED, TRIGGER_CHECKED,
};
use crate::rect::{UiPoint, UiRect};

// --- UITriStateCheckBox -----------------------------------------------------------------------------------------------

/// `UITriStateCheckBox`'s members in their C++ order (from `mTextLabel` on).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct TriStateFields {
    /// `UITextLabel*`, C++'s.
    pub text_label: *mut c_void,
    /// 0 off, 1 on, 2 "not".
    pub check_state: c_ulong,
    pub has_text: bool,
}

/// `kCheckStateOff`, `kCheckStateOn`, `kCheckStateNot`.
pub const CHECK_OFF: c_ulong = 0;
pub const CHECK_ON: c_ulong = 1;
pub const CHECK_NOT: c_ulong = 2;

/// The tri-state box's way back into its C++ object.
pub trait TriStateHost {
    /// Reads the record.
    fn load(&self) -> TriStateFields;
    /// Writes the record.
    fn store(&mut self, f: &TriStateFields);
    /// `mRectangle`.
    fn rect(&self) -> UiRect;
    /// `mBorderThickness`.
    fn border(&self) -> c_ulong;
    /// `mTextLabel->GetRectangle()` (only with a label).
    fn label_rect(&self) -> UiRect;
    /// `mTextLabel->SetRectangle(rect)`.
    fn label_set_rect(&mut self, rect: UiRect);
    /// `mTextLabel->SetLocation(at)`.
    fn label_set_location(&mut self, at: UiPoint);
    /// `mTextLabel->SetText(text, true)`.
    fn label_set_text(&mut self);
    /// `mTextLabel = new UITextLabel(text, font, id, rect, form, 0, index, 0)`.
    fn label_create(&mut self, rect: UiRect);
    /// `delete mTextLabel`.
    fn label_delete(&mut self);
    /// `UIControl::SetRectangle(rect)`.
    fn base_set_rectangle(&mut self, rect: UiRect);
    /// `UIControl::SetLocation(at)`.
    fn base_set_location(&mut self, at: UiPoint);
    /// The virtual `SetDirty(true)`.
    fn set_dirty(&mut self);
    /// `SetNormalBitmap`/`SetPressedBitmap` with the manager's bitmaps for the state (0 off, 1 on, 2 not).
    fn set_check_bitmaps(&mut self, state: c_ulong);
    /// `FireTriggerEvent(event)`.
    fn fire(&mut self, event: c_ulong);
    /// `UIButton::CopyData()`.
    fn base_copy_data(&mut self) -> c_long;
    /// `mTextLabel->CopyData()`.
    fn label_copy_data(&mut self) -> c_long;
}

/// The constructor's `UIButton` rectangle: a box at most 19 pixels square at the top-left.
pub fn tri_state_rect(rect: &UiRect) -> UiRect {
    box_rect(rect, false)
}

/// The constructor's bitmap offset: centred vertically in a box taller than the bitmap column (`None` keeps it).
pub fn tri_state_bitmap_offset(height: f32, border: c_ulong) -> Option<UiPoint> {
    label_bitmap_offset(height, border, false, true)
}

/// `ChangeWidth`: the label shrunk to fit beside the box, then the box's own width.
pub fn tri_state_change_width(h: &mut impl TriStateHost, width: c_ulong) {
    if !h.load().text_label.is_null() {
        let mut r = h.label_rect();
        r.right = (r.left + width as f32) - SCROLL_BAR_WIDTH;
        h.label_set_rect(r);
    }
    let mut r = h.rect();
    r.right = (r.left + width as f32) - 1.0;
    h.base_set_rectangle(r);
}

/// `CopyData`: the box's draw state, then the label's; a label whose text was cleared is deleted.
pub fn tri_state_copy_data(h: &mut impl TriStateHost) -> c_long {
    let mut result = h.base_copy_data();
    let mut f = h.load();
    if f.has_text {
        result = h.label_copy_data();
    } else if !f.text_label.is_null() {
        h.label_delete();
        f = h.load();
        f.text_label = core::ptr::null_mut();
        h.store(&f);
    }
    result
}

/// `SetCheckState`: the state, its bitmaps and the checked trigger (an unknown state only stores the value).
pub fn tri_state_set_check_state(h: &mut impl TriStateHost, state: c_ulong) {
    let mut f = h.load();
    f.check_state = state;
    h.store(&f);
    if !matches!(state, CHECK_OFF | CHECK_ON | CHECK_NOT) {
        return;
    }
    h.set_check_bitmaps(state);
    h.fire(TRIGGER_CHECKED);
}

/// The label's top-left beside the box: past the border and the bitmap column.
fn tri_state_label_at(rect: &UiRect, border: c_ulong) -> UiPoint {
    UiPoint { x: (rect.left + border as f32) + SCROLL_BAR_WIDTH, y: rect.top + border as f32 }
}

/// `SetLocation`: the box moved, then its label.
pub fn tri_state_set_location(h: &mut impl TriStateHost, at: UiPoint) {
    h.base_set_location(at);
    if h.load().text_label.is_null() {
        return;
    }
    let at = tri_state_label_at(&h.rect(), h.border());
    h.label_set_location(at);
}

/// `SetText` (`text`: the new text's bytes, NUL-terminated): the label's text (and the box dirty), or a new label
/// beside the box; empty text hides the label.
pub fn tri_state_set_text(h: &mut impl TriStateHost, text: &[u8]) {
    let mut f = h.load();
    if crate::text::is_empty(text) {
        f.has_text = false;
        h.store(&f);
        return;
    }
    if !f.text_label.is_null() {
        h.label_set_text();
        h.set_dirty();
    } else {
        let r = h.rect();
        let b = h.border() as f32;
        let at = tri_state_label_at(&r, h.border());
        h.label_create(UiRect::new(at.x, at.y, r.right - b, r.bottom - b));
    }
    f = h.load();
    f.has_text = true;
    h.store(&f);
    h.set_dirty();
}

/// `CycleCheck`: off -> on -> not -> off.
pub fn tri_state_cycle(h: &mut impl TriStateHost) {
    let mut f = h.load();
    f.check_state = (f.check_state + 1) % 3;
    h.store(&f);
    tri_state_set_check_state(h, f.check_state);
}

// --- UIBlinkingButton --------------------------------------------------------------------------------------------------

/// `UIBlinkingButton`'s members in their C++ order (from `mBlinkPhase` on).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct BlinkFields {
    /// 1 fading out, 0 fading in.
    pub blink_phase: c_ulong,
    /// Milliseconds per phase, 50..5000.
    pub blink_rate: c_ulong,
    pub pressed_opacity: f32,
    pub normal_opacity: f32,
    pub bg_opacity: f32,
    pub full_border_opacity: f32,
    pub saved_always_dirty: bool,
    pub needs_opacities: bool,
    pub is_blinking: bool,
}

/// The constructor's blink period: 0 means 250, else clamped to 50..5000.
pub fn blink_rate(rate: c_ulong) -> c_ulong {
    if rate == 0 {
        250
    } else {
        rate.clamp(50, 5000)
    }
}

/// The constructor's record after `UIButton`'s: fading out, the clamped rate, the full opacities still to be sampled
/// (the C++ left them unset; zero here), `mAlwaysDirty` saved.
pub fn blink_init(rate: c_ulong, always_dirty: bool) -> BlinkFields {
    BlinkFields {
        blink_phase: 1,
        blink_rate: blink_rate(rate),
        pressed_opacity: 0.0,
        normal_opacity: 0.0,
        bg_opacity: 0.0,
        full_border_opacity: 0.0,
        saved_always_dirty: always_dirty,
        needs_opacities: true,
        is_blinking: false,
    }
}

/// What `CopyData` shows: the background's, the border's and the two bitmaps' opacities.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct BlinkShow {
    pub background: f32,
    pub border: f32,
    pub pressed: f32,
    pub normal: f32,
}

/// `CopyData`'s fade: the full opacities sampled on the first call (`bg`, `border`: the control's), the phase flipped
/// when the poll expired, and, while blinking and not pressed, every opacity scaled by the sine of the phase's
/// progress (`time_left` of `blink_rate` milliseconds); full otherwise.
pub fn blink_copy_data(
    f: &mut BlinkFields,
    polled: bool,
    time_left: c_ulong,
    state: c_ulong,
    bg: f32,
    border: f32,
) -> BlinkShow {
    if f.needs_opacities {
        f.pressed_opacity = 1.0;
        f.normal_opacity = 1.0;
        f.bg_opacity = bg;
        f.full_border_opacity = border;
        f.needs_opacities = false;
    }
    if polled {
        f.blink_phase = (f.blink_phase != 1) as c_ulong;
    }
    let fraction = time_left as f32 / f.blink_rate as f32;
    if f.is_blinking && state != STATE_PRESSED {
        let scale = if f.blink_phase == 1 {
            ee_ge::um::um_sin((1.0 - fraction) * 1.570_796_4)
        } else {
            ee_ge::um::um_sin(fraction * 1.570_796_4)
        };
        BlinkShow {
            background: scale * f.bg_opacity,
            border: scale * f.full_border_opacity,
            pressed: scale * f.pressed_opacity,
            normal: scale * f.normal_opacity,
        }
    } else {
        BlinkShow {
            background: f.bg_opacity,
            border: f.full_border_opacity,
            pressed: f.pressed_opacity,
            normal: f.normal_opacity,
        }
    }
}

// --- UIScalableButton --------------------------------------------------------------------------------------------------

/// `CopyData`'s source rectangle for the state (focused falls back to normal without a focus bitmap; another state
/// keeps the current one).
pub fn scalable_copied_rect(
    state: c_ulong,
    has_focused_bitmap: bool,
    normal: &UiRect,
    focused: &UiRect,
    pressed: &UiRect,
    current: &UiRect,
) -> UiRect {
    match state {
        STATE_NORMAL => *normal,
        STATE_FOCUSED => {
            if has_focused_bitmap {
                *focused
            } else {
                *normal
            }
        }
        STATE_PRESSED => *pressed,
        _ => *current,
    }
}

/// One of the three draws: the bitmap's source and destination rectangles, and where it is drawn.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Slice {
    pub source: UiRect,
    pub destination: UiRect,
    pub location: UiPoint,
}

/// `DrawPolygons`' slices in draw order: the left cap (the bitmap's left edge to the source's left), the right cap
/// (the source's right to the texture's width) and the middle stretched between them; `copied` is the state's source
/// rectangle, `draw` the control's rectangle as of `CopyData`.
pub fn scalable_slices(copied: &UiRect, draw: &UiRect, texture_width: c_ulong) -> [Slice; 3] {
    let left_source = UiRect::new(0.0, copied.top, copied.left + 1.0, copied.bottom);
    let mut destination = UiRect::new(draw.left, draw.top, (draw.left + left_source.width()) - 1.0, draw.bottom);
    let left = Slice { source: left_source, destination, location: draw.top_left() };
    let right_source = UiRect::new(copied.right + 1.0, copied.top, texture_width as f32, copied.bottom);
    destination.left = (draw.right - right_source.width()) + 1.0;
    destination.right = draw.right;
    let right = Slice { source: right_source, destination, location: destination.top_left() };
    destination.left = (draw.left + copied.left) + 1.0;
    destination.right = (draw.right - (texture_width as f32 - copied.right)) + 1.0;
    let middle = Slice { source: *copied, destination, location: destination.top_left() };
    [left, right, middle]
}

#[cfg(test)]
mod tests;
