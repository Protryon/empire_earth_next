//! The scroll arrow (`UIScrollButton`), the blinking button (`UIBlinkingButton`) and the scalable button
//! (`UIScalableButton`) whole in the Rust button (R6 ui round 12): their records ([`ScrollFields`], [`BlinkObj`],
//! [`ScalableFields`]) are the button object's [`Sub`] and every method is here.
//!
//! The arrow's two polls (`TMPoll`s, the 500 ms delay and the 100 ms repeat) are C++ objects the Rust button holds
//! by address, made and deleted by [`Op::PollCreate`]/[`Op::PollDelete`] and driven by the poll ops; its slider is
//! the scroll bar's (by address, round 10's slider ops). Its stepping is round 3's ([`button::scroll_button_down`]).
//!
//! The blinking button's poll was an embedded `TMPoll` member; it is now made by [`Op::PollCreate`] at construction
//! and deleted with the button. Its fade is round 6's ([`button_variants::blink_copy_data`]); the opacities it reads
//! and sets are the control's (`UIControl` ops) and the bitmaps' ([`Op::BitmapSetOpacity`]).
//!
//! The scalable button's three-slice draw is round 6's ([`button_variants::scalable_slices`]); each slice is a
//! [`Op::BitmapStretch`] (the source rectangle's four floats packed two per integer), a location and a draw.

use core::ffi::{c_long, c_ulong, c_void};

use crate::button::{self, ScrollButtonHost};
use crate::button_variants::{self, BlinkFields};
use crate::buttonobj::{Btn, EeUiButton, EeUiButtonCall, Sub};
use crate::obj::EeUiOp as Op;
use crate::rect::UiRect;

/// The scroll arrow's delay before repeating and its repeat period (milliseconds).
const SCROLL_DELAY_MS: isize = 500;
const SCROLL_REPEAT_MS: isize = 100;
/// The blinking button's poll before `SetTimeDelay(rate)`.
const BLINK_POLL_MS: isize = 250;
/// `kDisplayModeShown`.
const DISPLAY_MODE_SHOWN: isize = 1;

/// `UIScrollButton`'s members: `mDecrement`, `mSlider`, `mRepeatPoll`, `mDelayPoll`.
#[derive(Clone, Copy, Debug)]
pub(crate) struct ScrollFields {
    pub decrement: c_ulong,
    pub slider: *mut c_void,
    pub repeat_poll: *mut c_void,
    pub delay_poll: *mut c_void,
}

/// `UIScalableButton`'s members: the normal, focused and pressed source rectangles and the copied one.
#[derive(Clone, Copy, Debug)]
pub(crate) struct ScalableFields {
    pub rects: [UiRect; 3],
    pub copied: UiRect,
}

/// Two floats' bits in one integer (`BitmapStretch`'s source rectangle).
fn pack(a: f32, b: f32) -> isize {
    (((a.to_bits() as u64) << 32) | b.to_bits() as u64) as isize
}

/// `UIBlinkingButton`'s members: its poll and round 6's record.
#[derive(Clone, Copy, Debug)]
pub(crate) struct BlinkObj {
    pub poll: *mut c_void,
    pub f: BlinkFields,
}

impl EeUiButton {
    fn blink(&self) -> Option<BlinkObj> {
        match *self.sub.borrow() {
            Sub::Blink(b) => Some(b),
            _ => None,
        }
    }

    fn store_blink(&self, b: &BlinkObj) {
        if let Sub::Blink(x) = &mut *self.sub.borrow_mut() {
            *x = *b;
        }
    }

    /// `UIBlinkingButton::SetIsBlinking`: blinking keeps the button redrawn every frame (and shown).
    fn set_is_blinking(&self, blinking: bool) {
        let Some(mut b) = self.blink() else { return };
        if b.f.is_blinking == blinking {
            return;
        }
        b.f.is_blinking = blinking;
        self.store_blink(&b);
        self.poll_op(Op::PollStop, b.poll);
        if blinking {
            self.poll_op(Op::PollStart, b.poll);
            self.op(Op::SetAlwaysDirty, 1, 0);
            self.op(Op::SetDirty, 1, 0);
            self.op(Op::SetDisplayMode, DISPLAY_MODE_SHOWN, 0);
        } else {
            self.op(Op::SetAlwaysDirty, b.f.saved_always_dirty as isize, 0);
            self.op(Op::SetDirty, 1, 0);
        }
    }

    /// One float into an op (an opacity).
    fn op_float(&self, op: Op, a: isize, x: f32) {
        self.host.op_f(op, a, 0, 0, 0, &mut [x, 0.0, 0.0, 0.0]);
    }

    /// `UIBlinkingButton::CopyData`: the fade's opacities set, then `UIButton::CopyData`.
    fn blink_copy_data(&self) -> isize {
        let Some(mut b) = self.blink() else { return 0 };
        let polled = self.poll_op(Op::PollPoll, b.poll) != 0;
        let time_left = self.poll_op(Op::PollTimeLeft, b.poll) as c_ulong;
        let state = self.fields().state;
        let bg = self.host.get_point(Op::BgOpacity, 0).x;
        let border = self.host.get_point(Op::BorderOpacity, 0).x;
        let show = button_variants::blink_copy_data(&mut b.f, polled, time_left, state, bg, border);
        self.store_blink(&b);
        self.op_float(Op::SetBgOpacity, 0, show.background);
        self.op_float(Op::SetBorderOpacity, 0, show.border);
        let pressed = self.fields().pressed_bitmap;
        if !pressed.is_null() {
            self.op_float(Op::BitmapSetOpacity, pressed as isize, show.pressed);
        }
        let normal = self.fields().normal_bitmap;
        if !normal.is_null() {
            self.op_float(Op::BitmapSetOpacity, normal as isize, show.normal);
        }
        button::copy_data(&mut Btn::new(self)) as isize
    }

    fn scalable(&self) -> Option<ScalableFields> {
        match *self.sub.borrow() {
            Sub::Scalable(x) => Some(x),
            _ => None,
        }
    }

    /// `UIScalableButton::CopyData`: the state's source rectangle copied, then `UIButton::CopyData`.
    fn scalable_copy_data(&self) -> isize {
        if let Some(mut x) = self.scalable() {
            let f = self.fields();
            let [normal, focused, pressed] = x.rects;
            let has_focused = !f.focused_bitmap.is_null();
            x.copied =
                button_variants::scalable_copied_rect(f.state, has_focused, &normal, &focused, &pressed, &x.copied);
            *self.sub.borrow_mut() = Sub::Scalable(x);
        }
        button::copy_data(&mut Btn::new(self)) as isize
    }

    /// `UIScalableButton::DrawPolygons`: the copied bitmap drawn as a left cap, a right cap and a stretched middle;
    /// the last draw's result.
    fn scalable_draw_polygons(&self) -> isize {
        let bitmap = self.fields().copied_bitmap;
        let Some(x) = self.scalable() else { return 0 };
        if bitmap.is_null() {
            return 0;
        }
        let draw = self.host.get_rect(Op::CopiedDrawRect, 0);
        let width = self.op(Op::BitmapTextureWidth, bitmap as isize, 0) as c_ulong;
        let mut r = 0;
        for s in button_variants::scalable_slices(&x.copied, &draw, width) {
            let src = s.source;
            let mut f = crate::obj::rect_buf(s.destination);
            let (c, d) = (pack(src.bottom, src.left), pack(src.right, src.top));
            self.host.op_f(Op::BitmapStretch, bitmap as isize, 0, c, d, &mut f);
            self.host.op_point(Op::BitmapSetLocation, bitmap as isize, s.location);
            r = self.op(Op::BitmapDraw, bitmap as isize, 0);
        }
        r
    }

    fn scroll(&self) -> ScrollFields {
        match *self.sub.borrow() {
            Sub::Scroll(s) => s,
            _ => ScrollFields {
                decrement: 0,
                slider: core::ptr::null_mut(),
                repeat_poll: core::ptr::null_mut(),
                delay_poll: core::ptr::null_mut(),
            },
        }
    }

    fn poll_op(&self, op: Op, poll: *mut c_void) -> isize {
        self.op(op, poll as isize, 0)
    }

    /// The variants' destructor part: the arrow's polls deleted (the repeat poll first), the blinking button's poll.
    pub(crate) fn release_variant(&self) {
        match *self.sub.borrow() {
            Sub::Scroll(s) => {
                self.poll_op(Op::PollDelete, s.repeat_poll);
                self.poll_op(Op::PollDelete, s.delay_poll);
            }
            Sub::Blink(b) => {
                self.poll_op(Op::PollDelete, b.poll);
            }
            _ => {}
        }
    }

    /// The variants' inward calls ([`EeUiButtonCall`] from 95 on).
    pub(crate) fn call_variant(&self, call: EeUiButtonCall, a: isize, b: isize, f: [f32; 4]) -> isize {
        type C = EeUiButtonCall;
        let mut h = Btn::new(self);
        match call {
            C::ScrollInit => {
                let delay = self.op(Op::PollCreate, SCROLL_DELAY_MS, 0) as *mut c_void;
                let repeat = self.op(Op::PollCreate, SCROLL_REPEAT_MS, 0) as *mut c_void;
                *self.sub.borrow_mut() = Sub::Scroll(ScrollFields {
                    decrement: b as c_ulong,
                    slider: a as *mut c_void,
                    repeat_poll: repeat,
                    delay_poll: delay,
                });
            }
            C::ScrollButtonDown => button::scroll_button_down(&mut h),
            C::ScrollButtonUp => {
                let s = self.scroll();
                self.poll_op(Op::PollStop, s.repeat_poll);
                self.poll_op(Op::PollStop, s.delay_poll);
                button::button_up(&mut h);
            }
            C::BlinkInit => {
                let poll = self.op(Op::PollCreate, BLINK_POLL_MS, 0) as *mut c_void;
                let always_dirty = self.op(Op::AlwaysDirty, 0, 0) != 0;
                let f = button_variants::blink_init(a as c_ulong, always_dirty);
                *self.sub.borrow_mut() = Sub::Blink(BlinkObj { poll, f });
                self.op(Op::PollSetDelay, poll as isize, f.blink_rate as isize);
            }
            C::BlinkSetIsBlinking => self.set_is_blinking(a != 0),
            C::BlinkIsBlinking => return self.blink().is_some_and(|b| b.f.is_blinking) as isize,
            C::BlinkCopyData => return self.blink_copy_data(),
            C::ScalableInit => {
                let rect = UiRect { bottom: f[0], left: f[1], right: f[2], top: f[3] };
                let empty = ScalableFields { rects: [UiRect::default(); 3], copied: UiRect::default() };
                let mut x = self.scalable().unwrap_or(empty);
                if let Some(slot) = x.rects.get_mut(a as usize) {
                    *slot = rect;
                }
                *self.sub.borrow_mut() = Sub::Scalable(x);
            }
            C::ScalableCopyData => return self.scalable_copy_data(),
            C::ScalableDrawPolygons => return self.scalable_draw_polygons(),
            _ => {}
        }
        0
    }
}

impl ScrollButtonHost for Btn<'_> {
    fn decrement(&self) -> c_ulong {
        self.o.scroll().decrement
    }
    fn delay_running(&self) -> bool {
        self.o.poll_op(Op::PollRunning, self.o.scroll().delay_poll) != 0
    }
    fn delay_poll(&mut self) -> bool {
        self.o.poll_op(Op::PollPoll, self.o.scroll().delay_poll) != 0
    }
    fn delay_start(&mut self) {
        self.o.poll_op(Op::PollStart, self.o.scroll().delay_poll);
    }
    fn delay_stop(&mut self) {
        self.o.poll_op(Op::PollStop, self.o.scroll().delay_poll);
    }
    fn repeat_running(&self) -> bool {
        self.o.poll_op(Op::PollRunning, self.o.scroll().repeat_poll) != 0
    }
    fn repeat_poll(&mut self) -> bool {
        self.o.poll_op(Op::PollPoll, self.o.scroll().repeat_poll) != 0
    }
    fn repeat_start(&mut self) {
        self.o.poll_op(Op::PollStart, self.o.scroll().repeat_poll);
    }
    fn slider_value(&self) -> c_long {
        self.o.op(Op::SliderValue, self.o.scroll().slider as isize, 0) as c_long
    }
    fn set_slider_value(&mut self, value: c_long) {
        self.o.op(Op::SliderSetValue, self.o.scroll().slider as isize, value as isize);
    }
}

#[cfg(test)]
#[path = "variantobj_tests.rs"]
mod tests;
