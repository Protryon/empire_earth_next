//! `extern "C"` for a text label's geometry (`User Interface/UITextLabel.cpp`): the label, its text object and its
//! scroll bar are C++ objects, asked through `ee_uicpp_label_fit_*` on each step, never cached.
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_int, c_long, c_ulong, c_void};

use crate::label_fit::{self, LabelHost};
use crate::rect::UiRect;

/// cbindgen:ignore
mod sys {
    use core::ffi::{c_int, c_ulong, c_void};

    use crate::rect::UiRect;

    extern "C" {
        // which: 0 mFitRectToText, 1 mLabelType, 2 mAlignment, 3 mLines.size(), 4 mNumberLinesDisplayed,
        // 5 mScrollBar != NULL, 6 mScrollBarRemoved, 7 the slider's GetCurrentRangeValue()
        pub fn ee_uicpp_label_fit_value(label: *mut c_void, which: c_int) -> c_ulong;
        // which: 0 mOriginalRectangle, 1 mRectangle, 2 mScrollBarRectangle, 3 mScrollBar->GetRectangle()
        pub fn ee_uicpp_label_fit_rect(label: *mut c_void, which: c_int) -> UiRect;
        // mText->SetText(*mLines[i]), then its extent's width and height
        pub fn ee_uicpp_label_fit_measure(label: *mut c_void, i: c_ulong, width: *mut f32, height: *mut f32);
        // call: 0 mScrollBarRemoved = value, 1 SetRectangle(rect), 2 SetDirty(true), 3 SetBgRectangle(rect),
        // 4 mScrollBar->SetRange(0, value), 5 mScrollBar = new UIScrollBar(..., length, ...), 6 SetPageSize(value),
        // 7 the scroll bar's SetBgColor(mScrollBarBgColor) and SetBgOpacity(mScrollBarBgOpacity)
        pub fn ee_uicpp_label_fit_call(label: *mut c_void, call: c_int, value: c_ulong, rect: UiRect, length: f32);
    }
}

struct Label(*mut c_void);

impl Label {
    fn value(&self, which: c_int) -> c_ulong {
        unsafe { sys::ee_uicpp_label_fit_value(self.0, which) }
    }
    fn rect_of(&self, which: c_int) -> UiRect {
        unsafe { sys::ee_uicpp_label_fit_rect(self.0, which) }
    }
    fn call(&mut self, call: c_int, value: c_ulong, rect: UiRect, length: f32) {
        unsafe { sys::ee_uicpp_label_fit_call(self.0, call, value, rect, length) }
    }
}

impl LabelHost for Label {
    fn fit(&self) -> bool {
        self.value(0) != 0
    }
    fn label_type(&self) -> c_ulong {
        self.value(1)
    }
    fn alignment(&self) -> c_ulong {
        self.value(2)
    }
    fn original(&self) -> UiRect {
        self.rect_of(0)
    }
    fn rect(&self) -> UiRect {
        self.rect_of(1)
    }
    fn lines(&self) -> usize {
        self.value(3) as usize
    }
    fn displayed(&self) -> c_ulong {
        self.value(4)
    }
    fn measure(&mut self, i: usize) -> (f32, f32) {
        let (mut w, mut h) = (0.0, 0.0);
        unsafe { sys::ee_uicpp_label_fit_measure(self.0, i as c_ulong, &mut w, &mut h) };
        (w, h)
    }
    fn has_scroll_bar(&self) -> bool {
        self.value(5) != 0
    }
    fn removed(&self) -> bool {
        self.value(6) != 0
    }
    fn set_removed(&mut self, removed: bool) {
        self.call(0, removed as c_ulong, UiRect::default(), 0.0);
    }
    fn scroll_value(&mut self) -> c_long {
        self.value(7) as c_long
    }
    fn set_rectangle(&mut self, r: UiRect) {
        self.call(1, 0, r, 0.0);
    }
    fn set_dirty(&mut self) {
        self.call(2, 0, UiRect::default(), 0.0);
    }
    fn set_bg_rectangle(&mut self, r: UiRect) {
        self.call(3, 0, r, 0.0);
    }
    fn set_range(&mut self, max: c_ulong) {
        self.call(4, max, UiRect::default(), 0.0);
    }
    fn create_scroll_bar(&mut self, height: f32) {
        self.call(5, 0, UiRect::default(), height);
    }
    fn set_page_size(&mut self, page: c_ulong) {
        self.call(6, page, UiRect::default(), 0.0);
    }
    fn set_scroll_bar_colors(&mut self) {
        self.call(7, 0, UiRect::default(), 0.0);
    }
    fn scroll_bar_rect(&self) -> UiRect {
        self.rect_of(2)
    }
    fn scroll_bar_control_rect(&self) -> UiRect {
        self.rect_of(3)
    }
}

/// `UITextLabel::ComputeTextLabelRect`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_label_compute_rect(label: *mut c_void) {
    label_fit::compute_rect(&mut Label(label));
}

/// `UITextLabel::CheckScrollBar`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_label_check_scroll_bar(label: *mut c_void) {
    label_fit::check_scroll_bar(&mut Label(label));
}

/// `UITextLabel::SetLocation`'s rectangles after the control's move: the label's rectangle and border and whether
/// it has a scroll bar -> the scroll bar strip in `scroll_bar`, the text rectangle in `text`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_label_location_rects(
    rect: UiRect,
    border: c_ulong,
    has_scroll_bar: bool,
    scroll_bar: *mut UiRect,
    text: *mut UiRect,
) {
    let (s, t) = label_fit::location_rects(rect, border, has_scroll_bar);
    *scroll_bar = s;
    *text = t;
}
