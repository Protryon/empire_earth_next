//! One line of the in-game chat display (`EEUICChatLine`) whole: the sender's name in the player's colour and as
//! much of the message as fits, each drawn from a copy made by `CopyData`, over a background that fades in the line's
//! last second; the line hides itself when its timer runs out. The record is the class's members but its texts and
//! timer (C++ `GEText`s and a `TMPoll`, reached through the host).

use core::ffi::c_void;

use crate::screens::hud_ctl::{
    cvtt_u32, EeScreensRect, HudHost, HudOps, Rec, EE_SCREENS_HC_COPIED_RECT, EE_SCREENS_HC_COPY_BASE,
    EE_SCREENS_HC_DRAW_BASE, EE_SCREENS_HC_HIDDEN, EE_SCREENS_HC_RECT, EE_SCREENS_HC_SET_BG_OPACITY,
    EE_SCREENS_HC_SET_BG_RECTANGLE, EE_SCREENS_HC_SET_DIRTY, EE_SCREENS_HC_SET_DISPLAY_MODE, EE_SCREENS_HC_SHOWN,
    EE_SCREENS_HC_TEXT_BASE,
};

/// -> the timer's `Poll()`.
pub const EE_SCREENS_CHL_POLL: u32 = 40;
/// -> the timer's time left.
pub const EE_SCREENS_CHL_TIME_LEFT: u32 = 41;
/// The timer stopped.
pub const EE_SCREENS_CHL_STOP: u32 = 42;
/// (delay) the timer's delay set and the timer started.
pub const EE_SCREENS_CHL_START: u32 = 43;
/// The drawn name and message copied from the name and message.
pub const EE_SCREENS_CHL_COPY_TEXTS: u32 = 44;
/// (text) -> its text's length; f[0..4] its extent, f[4..6] its location.
pub const EE_SCREENS_CHL_TEXT: u32 = 45;
/// (text) its location set to f[0..2].
pub const EE_SCREENS_CHL_LOCATE: u32 = 46;
/// (colour, name: `GEColor*`, `UWideString*`) the name's text and colour set.
pub const EE_SCREENS_CHL_SET_NAME: u32 = 47;
/// (message: `UWideString*`, width) -> where the message is cut to fit the width in the message's font.
pub const EE_SCREENS_CHL_CUT: u32 = 48;
/// (message, cut) the message's text set: the message's first `cut` characters, or all of it for -1.
pub const EE_SCREENS_CHL_SET_MESSAGE: u32 = 49;
/// (message) -> its length.
pub const EE_SCREENS_CHL_LENGTH: u32 = 50;
/// (text) -> the form's clipped draw of the text in the rectangle f[0..4].
pub const EE_SCREENS_CHL_DRAW: u32 = 51;

/// The texts of [`EE_SCREENS_CHL_TEXT`], [`EE_SCREENS_CHL_LOCATE`] and [`EE_SCREENS_CHL_DRAW`].
pub const EE_SCREENS_CHL_NAME: i64 = 0;
pub const EE_SCREENS_CHL_MESSAGE: i64 = 1;
pub const EE_SCREENS_CHL_DRAWN_NAME: i64 = 2;
pub const EE_SCREENS_CHL_DRAWN_MESSAGE: i64 = 3;

/// The gap between the name and the message.
const MESSAGE_SPACING: f32 = 5.0;

/// `EEUICChatLine`'s members (its protected base): the C++ objects it owns by address (the timer a `TMPoll*`,
/// the texts `GEText*`s, made and deleted by the class's constructor and destructor; the UI cluster lane, round 1).
#[repr(C)]
#[allow(non_snake_case)]
#[derive(Debug)]
pub struct EeScreensChatLineFields {
    /// The copied draw rectangle moved down by the shadow offset.
    pub mTextRectangle: EeScreensRect,
    /// The table's background opacity.
    pub mOpacity: f32,
    pub mShadowOffset: u32,
    pub mShowing: bool,
    pub mTimer: *mut c_void,
    /// the bold name, the message and the copies `CopyData` makes of them
    pub mName: *mut c_void,
    pub mMessage: *mut c_void,
    pub mDrawnName: *mut c_void,
    pub mDrawnMessage: *mut c_void,
}

impl Default for EeScreensChatLineFields {
    fn default() -> Self {
        let null = core::ptr::null_mut();
        EeScreensChatLineFields {
            mTextRectangle: EeScreensRect::default(),
            mOpacity: 0.0,
            mShadowOffset: 0,
            mShowing: false,
            mTimer: null,
            mName: null,
            mMessage: null,
            mDrawnName: null,
            mDrawnMessage: null,
        }
    }
}

/// A text's length, extent and location ([`EE_SCREENS_CHL_TEXT`]).
struct Text {
    length: i64,
    extent: EeScreensRect,
    x: f32,
}

fn text(h: &mut impl HudHost, which: i64) -> Text {
    let mut f = [0.0; 6];
    let length = h.op(EE_SCREENS_CHL_TEXT, which, 0, &mut f);
    Text { length, extent: EeScreensRect::new(f[0], f[1], f[2], f[3]), x: f[4] }
}

/// The constructor's part: the opacity, no shadow, showing.
pub fn init(rec: &Rec<EeScreensChatLineFields>, opacity: f32) {
    rec.set(|r| {
        r.mOpacity = opacity;
        r.mShadowOffset = 0;
        r.mShowing = true;
    });
}

/// `CopyData`: while the timer runs, the texts copied side by side under the shadow offset over a background that
/// fades in the last second; once it ran out, hidden. The text rectangle follows the copied draw rectangle.
pub fn copy_data(rec: &Rec<EeScreensChatLineFields>, h: &mut impl HudHost) -> i64 {
    if h.call(EE_SCREENS_CHL_POLL, 0, 0) == 0 {
        h.call(EE_SCREENS_CHL_COPY_TEXTS, 0, 0);
        let rect = h.rect(EE_SCREENS_HC_RECT);
        let shadow = rec.get(|r| r.mShadowOffset) as f32;
        let top = rect.top + shadow;
        h.op(EE_SCREENS_CHL_LOCATE, EE_SCREENS_CHL_DRAWN_NAME, 0, &mut [rect.left, top]);
        let name = text(h, EE_SCREENS_CHL_DRAWN_NAME);
        let x = if name.length > 0 { name.extent.right + MESSAGE_SPACING } else { rect.left };
        h.op(EE_SCREENS_CHL_LOCATE, EE_SCREENS_CHL_DRAWN_MESSAGE, 0, &mut [x, top]);
        let message = text(h, EE_SCREENS_CHL_DRAWN_MESSAGE);
        let mut bg = rect;
        bg.right = message.extent.right + message.x;
        bg.top = rect.top + shadow;
        bg.bottom = rect.bottom + shadow;
        h.with_rect(EE_SCREENS_HC_SET_BG_RECTANGLE, 0, bg);
        let opacity = rec.get(|r| r.mOpacity);
        let left = h.call(EE_SCREENS_CHL_TIME_LEFT, 0, 0) as u32;
        let opacity = if left <= 1 { h.call(EE_SCREENS_CHL_TIME_LEFT, 0, 0) as u32 as f32 * opacity } else { opacity };
        h.op(EE_SCREENS_HC_SET_BG_OPACITY, 0, 0, &mut [opacity]);
        rec.set(|r| r.mShowing = true);
    } else {
        rec.set(|r| r.mShowing = false);
        h.call(EE_SCREENS_HC_SET_DISPLAY_MODE, EE_SCREENS_HC_HIDDEN, 0);
        h.call(EE_SCREENS_CHL_STOP, 0, 0);
    }
    let result = h.call(EE_SCREENS_HC_COPY_BASE, 0, 0);
    let mut t = h.rect(EE_SCREENS_HC_COPIED_RECT);
    let shadow = rec.get(|r| r.mShadowOffset);
    t.top += shadow as f32;
    t.bottom += shadow as f32;
    rec.set(|r| r.mTextRectangle = t);
    result
}

/// `Display`: the name in the colour and as much of the message as fits shown for `time` (at least 1); where the
/// message was cut.
pub fn display(
    h: &mut impl HudHost,
    color: *const c_void,
    name: *const c_void,
    message: *const c_void,
    time: u32,
) -> u32 {
    let time = if time == 0 { 1 } else { time };
    h.call(EE_SCREENS_CHL_SET_NAME, color as i64, name as i64);
    let rect = h.rect(EE_SCREENS_HC_RECT);
    let extent = text(h, EE_SCREENS_CHL_NAME).extent;
    let width = cvtt_u32((rect.right - rect.left) - (extent.right - extent.left));
    let cut = h.call(EE_SCREENS_CHL_CUT, message as i64, width as i64) as u32;
    let whole = (cut as i64) >= h.call(EE_SCREENS_CHL_LENGTH, message as i64, 0) as u32 as i64;
    h.call(EE_SCREENS_CHL_SET_MESSAGE, message as i64, if whole { -1 } else { cut as i64 });
    h.call(EE_SCREENS_CHL_STOP, 0, 0);
    h.call(EE_SCREENS_CHL_START, time as i64, 0);
    h.call(EE_SCREENS_HC_SET_DISPLAY_MODE, EE_SCREENS_HC_SHOWN, 0);
    h.call(EE_SCREENS_HC_SET_DIRTY, 1, 0);
    cut
}

/// `DrawPolygons`: the base's while showing.
pub fn draw(rec: &Rec<EeScreensChatLineFields>, h: &mut impl HudHost) -> i64 {
    if rec.get(|r| r.mShowing) {
        h.call(EE_SCREENS_HC_DRAW_BASE, 0, 0)
    } else {
        0
    }
}

/// `DrawTextA`: the base's, then the name and the message clipped to the text rectangle while showing with more
/// than a second left.
pub fn draw_text(rec: &Rec<EeScreensChatLineFields>, h: &mut impl HudHost) -> i64 {
    let mut result = h.call(EE_SCREENS_HC_TEXT_BASE, 0, 0);
    if rec.get(|r| r.mShowing) && h.call(EE_SCREENS_CHL_TIME_LEFT, 0, 0) as u32 > 1 {
        let t = rec.get(|r| r.mTextRectangle);
        h.with_rect(EE_SCREENS_CHL_DRAW, EE_SCREENS_CHL_DRAWN_NAME, t);
        let t = rec.get(|r| r.mTextRectangle);
        result = h.with_rect(EE_SCREENS_CHL_DRAW, EE_SCREENS_CHL_DRAWN_MESSAGE, t);
    }
    result
}

#[cfg(test)]
mod tests;
