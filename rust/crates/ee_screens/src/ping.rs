//! The lobby's ping meter (`EEUICPing`) whole: five 10x10 bars 12 pixels apart, the first `mNumBars + 1` drawn.
//! The record is the class's members but its bars (C++ `GERhombus`es, set and drawn through the host).

use crate::hud_ctl::{cvtt_u32, EeScreensRect, HudHost, HudOps, Rec, EE_SCREENS_HC_DRAW_BASE, EE_SCREENS_HC_SET_DIRTY};

/// (bar, f[0..4]) the bar's rectangle set, its colour green.
pub const EE_SCREENS_PING_BAR: u32 = 32;
/// (bar) -> the rasterizer's draw of the bar.
pub const EE_SCREENS_PING_DRAW: u32 = 33;

const BARS: u32 = 5;

/// `EEUICPing`'s members (its protected base) but the bars.
#[repr(C)]
#[allow(non_snake_case)]
#[derive(Debug, Default)]
pub struct EeScreensPingFields {
    /// The lit bars less one (`SetPing`'s quality times 5).
    pub mNumBars: u32,
}

/// The constructor's part: the bars placed from the meter's location, none lit.
pub fn init(rec: &Rec<EeScreensPingFields>, h: &mut impl HudHost, x: f32, y: f32) {
    for i in 0..BARS {
        let left = x + (i * 12) as f32;
        h.with_rect(EE_SCREENS_PING_BAR, i as i64, EeScreensRect::new(left, y, left + 10.0, y + 10.0));
    }
    rec.set(|r| r.mNumBars = 0);
}

/// `DrawPolygons`: the base's, then the bars up to the lit count; the last draw's result.
pub fn draw(rec: &Rec<EeScreensPingFields>, h: &mut impl HudHost) -> i64 {
    let mut result = h.call(EE_SCREENS_HC_DRAW_BASE, 0, 0);
    for i in 0..BARS {
        if i <= rec.get(|r| r.mNumBars) {
            result = h.call(EE_SCREENS_PING_DRAW, i as i64, 0);
        }
    }
    result
}

/// `SetPing`: the lit count from a quality of 0..1, redrawn on a change.
pub fn set_ping(rec: &Rec<EeScreensPingFields>, h: &mut impl HudHost, ping: f32) {
    let bars = cvtt_u32(ping * 5.0);
    if bars != rec.get(|r| r.mNumBars) {
        h.call(EE_SCREENS_HC_SET_DIRTY, 1, 0);
        rec.set(|r| r.mNumBars = bars);
    }
}

#[cfg(test)]
mod tests;
