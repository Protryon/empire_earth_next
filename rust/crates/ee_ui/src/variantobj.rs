//! The scroll arrow (`UIScrollButton`) whole in the Rust button (R6 ui round 12): its record ([`ScrollFields`]) is
//! the button object's [`Sub`] and every method is here.
//!
//! The arrow's two polls (`TMPoll`s, the 500 ms delay and the 100 ms repeat) are C++ objects the Rust button holds
//! by address, made and deleted by [`Op::PollCreate`]/[`Op::PollDelete`] and driven by the poll ops; its slider is
//! the scroll bar's (by address, round 10's slider ops). Its stepping is round 3's ([`button::scroll_button_down`]).

use core::ffi::{c_long, c_ulong, c_void};

use crate::button::{self, ScrollButtonHost};
use crate::buttonobj::{Btn, EeUiButton, EeUiButtonCall, Sub};
use crate::obj::EeUiOp as Op;

/// The scroll arrow's delay before repeating and its repeat period (milliseconds).
const SCROLL_DELAY_MS: isize = 500;
const SCROLL_REPEAT_MS: isize = 100;

/// `UIScrollButton`'s members: `mDecrement`, `mSlider`, `mRepeatPoll`, `mDelayPoll`.
#[derive(Clone, Copy, Debug)]
pub(crate) struct ScrollFields {
    pub decrement: c_ulong,
    pub slider: *mut c_void,
    pub repeat_poll: *mut c_void,
    pub delay_poll: *mut c_void,
}

impl EeUiButton {
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

    /// The variants' destructor part: the arrow's polls deleted, the repeat poll first.
    pub(crate) fn release_variant(&self) {
        if let Sub::Scroll(s) = *self.sub.borrow() {
            self.poll_op(Op::PollDelete, s.repeat_poll);
            self.poll_op(Op::PollDelete, s.delay_poll);
        }
    }

    /// The variants' inward calls ([`EeUiButtonCall`] from 95 on).
    pub(crate) fn call_variant(&self, call: EeUiButtonCall, a: isize, b: isize, _f: [f32; 4]) -> isize {
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
