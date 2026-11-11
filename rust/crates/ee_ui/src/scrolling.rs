//! The scrolling text label (`User Interface/UITLScrolling.cpp`, the credits): `CopyData`'s scroll by the elapsed
//! ticks and its snapshot of the visible lines, the start, scroll and end timers, `ScrollText`'s speeds and delays,
//! `Pause`/`Resume`/`StopScrolling`, and `DrawTextA`'s line places.
//!
//! The label, its lines, its three `TMPoll`s and the trigger event stay C++: each read, write and call is a host
//! call, in the C++'s order. The arithmetic is the C++'s types: the pixel offset is a `long` compared as a float,
//! the line loop's `y` an `unsigned long` compared with a `long` bottom (converted to unsigned), the delays
//! `unsigned long`s.

use core::ffi::{c_long, c_ulong};

use crate::rect::{float_to_long, um_floor, UiRect};

/// `kControlTriggerScrollingFinished`.
pub const TRIGGER_SCROLLING_FINISHED: c_ulong = 0x3f4;

/// The three timers.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ScrollPoll {
    /// `mScrollPoll`: one pixel per tick.
    Scroll = 0,
    /// `mStartPoll`: the delay before the scroll.
    Start = 1,
    /// `mEndPoll`: the delay after the last line.
    End = 2,
}

/// What is done to a timer.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PollCall {
    Running = 0,
    Poll = 1,
    Start = 2,
    Stop = 3,
    Pause = 4,
    Resume = 5,
    SetTimeDelay = 6,
}

/// The label's members the scroll keeps.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ScrollField {
    StartingLine = 0,
    DrawStartingLine = 1,
    EndOffset = 2,
    DrawLineCount = 3,
    PixelStartingOffset = 4,
    DrawPixelOffset = 5,
    Scrolling = 6,
}

/// The scrolling label as its scroll sees it.
pub trait ScrollHost {
    /// The label's text's font's `GetHeight()` (the draw text's in `CopyData`, a copy of it), `mRectangle`, `GetBorderThickness()`, `GetTextRectangle()`.
    fn font_height(&mut self) -> c_ulong;
    fn rect(&self) -> UiRect;
    fn border(&self) -> c_ulong;
    fn text_rect(&self) -> UiRect;
    /// A member (as the C++'s `unsigned long`, `long` or `bool` bits) and its assignment.
    fn get(&self, f: ScrollField) -> c_ulong;
    fn set(&mut self, f: ScrollField, v: c_ulong);
    /// The timer `p` asked or told `c` (`SetTimeDelay` takes `arg`) -> `GetIsRunning()` / `Poll()`'s result.
    fn poll(&mut self, p: ScrollPoll, c: PollCall, arg: c_ulong) -> c_ulong;
    /// `GetLineCount()`, `GetNumberLinesDisplayed()`.
    fn line_count(&self) -> usize;
    fn displayed(&self) -> c_ulong;
    /// `CopyData`'s C++ steps (the C++ copies the label's text into the draw text first): the draw lines resized
    /// to `n` when they differ; `mDrawRectangle = GetTextRectangle()`; `mDrawLines[i] = GetText(line)`;
    /// `UIControl::CopyData()`.
    fn resize_draw_lines(&mut self, n: usize);
    fn copy_draw_rect(&mut self);
    fn copy_line(&mut self, i: usize, line: usize);
    fn control_copy_data(&mut self) -> c_long;
    /// `FireTriggerEvent(kControlTriggerScrollingFinished)`, `SetDirty(true)`.
    fn fire_finished(&mut self);
    fn set_dirty(&mut self);
}

fn running(h: &mut impl ScrollHost, p: ScrollPoll) -> bool {
    h.poll(p, PollCall::Running, 0) != 0
}

fn call(h: &mut impl ScrollHost, p: ScrollPoll, c: PollCall) {
    h.poll(p, c, 0);
}

/// `CopyData`: the scroll moves up by the scroll timer's ticks (after the start delay), whole lines that left the
/// top are dropped, the lines from the first visible one down to the bottom are copied for drawing; when the last
/// line has scrolled `mEndOffset` pixels the end delay starts, after which the scroll stops and fires its trigger.
pub fn copy_data(h: &mut impl ScrollHost) -> c_long {
    use ScrollField as F;
    use ScrollPoll as P;
    let height = h.font_height();
    let line_height = height as f32;
    let r = h.rect();
    let border = h.border();
    let lines = um_floor((r.height() - (2 * border) as f32) / line_height).wrapping_add(2);
    h.resize_draw_lines(lines as c_ulong as usize);
    h.copy_draw_rect();
    let mut offset = h.get(F::PixelStartingOffset) as c_long;
    if running(h, P::Scroll) {
        offset = offset.wrapping_sub(h.poll(P::Scroll, PollCall::Poll, 0) as c_long);
        h.set(F::PixelStartingOffset, offset as c_ulong);
    }
    if running(h, P::Start) && h.poll(P::Start, PollCall::Poll, 0) > 0 {
        call(h, P::Start, PollCall::Stop);
        call(h, P::Scroll, PollCall::Start);
    }
    let top = h.rect().top + h.border() as f32 + 1.0 - line_height;
    while (h.get(F::PixelStartingOffset) as c_long as f32) < top {
        let o = (h.get(F::PixelStartingOffset) as c_long).wrapping_add(height as c_long);
        h.set(F::PixelStartingOffset, o as c_ulong);
        let s = h.get(F::StartingLine).wrapping_add(1);
        h.set(F::StartingLine, s);
    }
    let starting = h.get(F::StartingLine);
    if (starting as usize) < h.line_count() {
        let offset = h.get(F::PixelStartingOffset);
        h.set(F::DrawPixelOffset, offset);
        h.set(F::DrawStartingLine, starting);
        let mut i: c_ulong = 0;
        let r = h.rect();
        let bottom = um_floor(r.bottom - h.border() as f32);
        let mut y = offset;
        while y < bottom as c_ulong {
            if starting.wrapping_add(i) as usize >= h.line_count() {
                break;
            }
            h.copy_line(i as usize, starting.wrapping_add(i) as usize);
            i += 1;
            y = y.wrapping_add(height);
        }
        h.set(F::DrawLineCount, i);
        if running(h, P::Scroll) && starting.wrapping_add(i) as usize >= h.line_count() {
            let r = h.rect();
            let left = i.wrapping_mul(height) as f32
                + ((h.get(F::PixelStartingOffset) as c_long as f32 - r.top) + (h.border() as f32 + 1.0));
            if left < h.get(F::EndOffset) as f32 {
                call(h, P::Scroll, PollCall::Stop);
                call(h, P::End, PollCall::Start);
            }
        }
    } else if h.get(F::Scrolling) != 0 {
        call(h, P::Scroll, PollCall::Stop);
        call(h, P::End, PollCall::Start);
    }
    let result = h.control_copy_data();
    if running(h, P::End) && h.poll(P::End, PollCall::Poll, 0) > 0 {
        h.set(F::Scrolling, 0);
        call(h, P::End, PollCall::Stop);
        h.fire_finished();
    } else if h.get(F::Scrolling) != 0 {
        h.set_dirty();
    }
    result
}

/// `ScrollText(speed, start delay, end delay, start offset, end offset)`: the scroll timer's period is a pixel's
/// time at `speed` lines a second (at least 1 ms), the start delay runs first, the first line starts `start offset`
/// pixels below the text's top.
pub fn scroll_text(
    h: &mut impl ScrollHost,
    speed: f32,
    start_delay: c_long,
    end_delay: c_long,
    start_offset: c_long,
    end_offset: c_long,
) {
    use ScrollPoll as P;
    let mut delay = 1000.0 / (h.font_height() as f32 * speed);
    if delay < 1.0 {
        delay = 1.0;
    }
    call(h, P::Scroll, PollCall::Stop);
    h.poll(P::Scroll, PollCall::SetTimeDelay, um_floor(delay) as c_ulong);
    call(h, P::End, PollCall::Stop);
    h.poll(P::End, PollCall::SetTimeDelay, end_delay as c_ulong);
    call(h, P::Start, PollCall::Stop);
    h.poll(P::Start, PollCall::SetTimeDelay, start_delay as c_ulong);
    call(h, P::Start, PollCall::Start);
    let offset = um_floor(h.border() as f32 + h.rect().top + start_offset as f32 + 1.0);
    h.set(ScrollField::PixelStartingOffset, offset as c_ulong);
    h.set(ScrollField::EndOffset, end_offset as c_ulong);
    h.set(ScrollField::StartingLine, 0);
    h.set(ScrollField::Scrolling, 1);
    h.set_dirty();
}

/// `ScrollText(time, end delay)`: a text longer than the label scrolls at half a line a second, its start delay
/// the time's share of half the visible lines and its end delay what the scroll leaves of `time`; a text that fits
/// waits `time` and ends.
pub fn scroll_text_for(h: &mut impl ScrollHost, time: c_ulong, end_delay: c_ulong) {
    let lines = h.line_count() as c_ulong;
    let displayed = h.displayed();
    let start;
    let mut end = end_delay;
    if lines > displayed {
        start = float_to_long((time / lines) as f32 * (displayed as f32 * 0.5)) as c_ulong;
        let t = h.text_rect();
        let scroll = float_to_long(((lines as f32 - t.height() / h.font_height() as f32) * 2.0) * 1000.0);
        end = time.wrapping_sub(scroll as c_ulong).wrapping_sub(start);
    } else {
        start = time;
    }
    let height = um_floor(h.rect().height());
    scroll_text(h, 0.5, start as c_long, end as c_long, 0, height);
}

/// `PauseScrolling` (`resume` false) and `ResumeScrolling`: the running timer, the end delay first, then the start
/// delay, then the scroll; a resume redraws.
pub fn pause_or_resume(h: &mut impl ScrollHost, resume: bool) {
    if h.get(ScrollField::Scrolling) == 0 {
        return;
    }
    let c = if resume { PollCall::Resume } else { PollCall::Pause };
    for p in [ScrollPoll::End, ScrollPoll::Start, ScrollPoll::Scroll] {
        if running(h, p) {
            call(h, p, c);
            break;
        }
    }
    if resume {
        h.set_dirty();
    }
}

/// `StopScrolling`: the timers stop and the trigger fires.
pub fn stop_scrolling(h: &mut impl ScrollHost) {
    if h.get(ScrollField::Scrolling) == 0 {
        return;
    }
    h.set(ScrollField::Scrolling, 0);
    call(h, ScrollPoll::End, PollCall::Stop);
    call(h, ScrollPoll::Scroll, PollCall::Stop);
    call(h, ScrollPoll::Start, PollCall::Stop);
    h.fire_finished();
}

/// `DrawTextA`'s place for snapshot line `i`: the draw rectangle's left, the font's height times `i` past the draw
/// offset (`unsigned long` arithmetic).
pub fn draw_line_at(draw_left: f32, font_height: c_ulong, i: c_ulong, offset: c_ulong) -> (f32, f32) {
    (draw_left, font_height.wrapping_mul(i).wrapping_add(offset) as f32)
}

#[cfg(test)]
mod tests;
