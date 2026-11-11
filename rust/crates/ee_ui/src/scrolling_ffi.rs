//! `extern "C"` for the scrolling label (`User Interface/UITLScrolling.cpp`): the label, its lines, timers and
//! trigger are C++ objects, asked through `ee_uicpp_scrolling_*` on each step, never cached.
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_int, c_long, c_ulong, c_void};

use crate::rect::{UiPoint, UiRect};
use crate::scrolling::{self, PollCall, ScrollField, ScrollHost, ScrollPoll};

/// cbindgen:ignore
mod sys {
    use core::ffi::{c_int, c_long, c_ulong, c_void};

    use crate::rect::UiRect;

    extern "C" {
        // which: 0 GetGEText()'s font height, 1 GetBorderThickness(), 2 GetLineCount(), 3 GetNumberLinesDisplayed()
        pub fn ee_uicpp_scrolling_value(label: *mut c_void, which: c_int) -> c_ulong;
        // which: 0 mRectangle, 1 GetTextRectangle()
        pub fn ee_uicpp_scrolling_rect(label: *mut c_void, which: c_int) -> UiRect;
        // a ScrollField member (its bits as an unsigned long), and its assignment
        pub fn ee_uicpp_scrolling_get(label: *mut c_void, field: c_int) -> c_ulong;
        pub fn ee_uicpp_scrolling_set(label: *mut c_void, field: c_int, value: c_ulong);
        // the ScrollPoll timer told a PollCall (SetTimeDelay(arg)) -> GetIsRunning() or Poll()
        pub fn ee_uicpp_scrolling_poll(label: *mut c_void, poll: c_int, call: c_int, arg: c_ulong) -> c_ulong;
        // call: 0 mDrawLines.resize(a) when its size differs, 1 mDrawRectangle = GetTextRectangle(),
        // 2 mDrawLines[a] = GetText(b), 3 UIControl::CopyData(), 4 FireTriggerEvent(scrolling finished),
        // 5 SetDirty(true)
        pub fn ee_uicpp_scrolling_call(label: *mut c_void, call: c_int, a: c_ulong, b: c_ulong) -> c_long;
    }
}

struct Label(*mut c_void);

impl Label {
    fn call(&mut self, call: c_int, a: c_ulong, b: c_ulong) -> c_long {
        unsafe { sys::ee_uicpp_scrolling_call(self.0, call, a, b) }
    }
    fn value(&self, which: c_int) -> c_ulong {
        unsafe { sys::ee_uicpp_scrolling_value(self.0, which) }
    }
}

impl ScrollHost for Label {
    fn font_height(&mut self) -> c_ulong {
        self.value(0)
    }
    fn rect(&self) -> UiRect {
        unsafe { sys::ee_uicpp_scrolling_rect(self.0, 0) }
    }
    fn border(&self) -> c_ulong {
        self.value(1)
    }
    fn text_rect(&self) -> UiRect {
        unsafe { sys::ee_uicpp_scrolling_rect(self.0, 1) }
    }
    fn get(&self, f: ScrollField) -> c_ulong {
        unsafe { sys::ee_uicpp_scrolling_get(self.0, f as c_int) }
    }
    fn set(&mut self, f: ScrollField, v: c_ulong) {
        unsafe { sys::ee_uicpp_scrolling_set(self.0, f as c_int, v) }
    }
    fn poll(&mut self, p: ScrollPoll, c: PollCall, arg: c_ulong) -> c_ulong {
        unsafe { sys::ee_uicpp_scrolling_poll(self.0, p as c_int, c as c_int, arg) }
    }
    fn line_count(&self) -> usize {
        self.value(2) as usize
    }
    fn displayed(&self) -> c_ulong {
        self.value(3)
    }
    fn resize_draw_lines(&mut self, n: usize) {
        self.call(0, n as c_ulong, 0);
    }
    fn copy_draw_rect(&mut self) {
        self.call(1, 0, 0);
    }
    fn copy_line(&mut self, i: usize, line: usize) {
        self.call(2, i as c_ulong, line as c_ulong);
    }
    fn control_copy_data(&mut self) -> c_long {
        self.call(3, 0, 0)
    }
    fn fire_finished(&mut self) {
        self.call(4, 0, 0);
    }
    fn set_dirty(&mut self) {
        self.call(5, 0, 0);
    }
}

/// `UITLScrolling::CopyData` (after the C++ copied the label's text into the draw text) -> its result.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_scrolling_copy_data(label: *mut c_void) -> c_long {
    scrolling::copy_data(&mut Label(label))
}

/// `UITLScrolling::ScrollText(speed, start delay, end delay, start offset, end offset)`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_scrolling_scroll_text(
    label: *mut c_void,
    speed: f32,
    start_delay: c_long,
    end_delay: c_long,
    start_offset: c_long,
    end_offset: c_long,
) {
    scrolling::scroll_text(&mut Label(label), speed, start_delay, end_delay, start_offset, end_offset);
}

/// `UITLScrolling::ScrollText(time, end delay)`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_scrolling_scroll_text_for(label: *mut c_void, time: c_ulong, end_delay: c_ulong) {
    scrolling::scroll_text_for(&mut Label(label), time, end_delay);
}

/// `PauseScrolling` (`resume` false) or `ResumeScrolling`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_scrolling_pause(label: *mut c_void, resume: bool) {
    scrolling::pause_or_resume(&mut Label(label), resume);
}

/// `StopScrolling`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_scrolling_stop(label: *mut c_void) {
    scrolling::stop_scrolling(&mut Label(label));
}

/// `DrawTextA`'s place for snapshot line `i`.
#[no_mangle]
pub extern "C" fn ee_ui_scrolling_line_at(
    draw_left: f32,
    font_height: c_ulong,
    i: c_ulong,
    offset: c_ulong,
) -> UiPoint {
    let (x, y) = scrolling::draw_line_at(draw_left, font_height, i, offset);
    UiPoint { x, y }
}
