//! `GERectangle` (`include/lle/GERectangle.h`) as the UI code uses it: a pinned record (bottom, left, right, top;
//! sizeof 0x10) and the float helpers of `LowLevelEngineCore.h` the UI arithmetic goes through.

use core::ffi::{c_long, c_ulong};

/// `GERectangle`: the C++ member order, `mBottom` first.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct UiRect {
    pub bottom: f32,
    pub left: f32,
    pub right: f32,
    pub top: f32,
}

const _: () = assert!(core::mem::size_of::<UiRect>() == 0x10);

/// `GE2DPoint`: x, y.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct UiPoint {
    pub x: f32,
    pub y: f32,
}

impl UiRect {
    /// `GERectangle(float left, float top, float right, float bottom)`.
    pub const fn new(left: f32, top: f32, right: f32, bottom: f32) -> Self {
        UiRect { bottom, left, right, top }
    }

    /// `GetWidth`: right - left.
    pub fn width(&self) -> f32 {
        self.right - self.left
    }

    /// `GetHeight`: bottom - top.
    pub fn height(&self) -> f32 {
        self.bottom - self.top
    }

    /// `GetTopLeft`.
    pub fn top_left(&self) -> UiPoint {
        UiPoint { x: self.left, y: self.top }
    }

    /// `Contains(const GE2DPoint&)`: inclusive on all four edges (NaN is outside).
    pub fn contains(&self, p: UiPoint) -> bool {
        p.x >= self.left && p.x <= self.right && p.y >= self.top && p.y <= self.bottom
    }

    /// `RoundToNearest`: each edge `UMFloor(edge + 0.5f)`, in the C++ order (top, left, right, bottom).
    pub fn round_to_nearest(&mut self) {
        self.top = um_floor(self.top + 0.5) as f32;
        self.left = um_floor(self.left + 0.5) as f32;
        self.right = um_floor(self.right + 0.5) as f32;
        self.bottom = um_floor(self.bottom + 0.5) as f32;
    }
}

/// `UMFloatToLong`: `lrintf` (round to nearest, ties to even) into a C `long`, with the C runtime's answer for NaN and
/// values out of range, which differs per system (Rust's `as` saturates, which only arm64 does):
/// - Windows (the UCRT, measured from the clang-cl build): inside [-2^31, 2^31] the `cvtss2si` result (2^31 itself
///   gives `LONG_MIN`), anything else and NaN give 0;
/// - x86-64 glibc: `cvtss2si` on 64 bits, the "integer indefinite" `LONG_MIN` out of range and for NaN;
/// - arm64: `fcvtns`, saturating, NaN 0.
#[inline]
pub fn float_to_long(value: f32) -> c_long {
    #[cfg(windows)]
    {
        if (-2_147_483_648.0..=2_147_483_648.0).contains(&value) {
            let rounded = value.round_ties_even();
            if rounded < 2_147_483_648.0 {
                rounded as c_long
            } else {
                c_long::MIN
            }
        } else {
            0
        }
    }
    #[cfg(all(not(windows), target_arch = "x86_64"))]
    {
        let rounded = value.round_ties_even();
        let min = c_long::MIN as f32;
        if rounded >= min && rounded < -min {
            rounded as c_long
        } else {
            c_long::MIN
        }
    }
    #[cfg(all(not(windows), not(target_arch = "x86_64")))]
    {
        value.round_ties_even() as c_long
    }
}

/// `UMFloor`: round to nearest, then one less when that is above the input (compared as a float).
#[inline]
pub fn um_floor(value: f32) -> c_long {
    let r = float_to_long(value);
    if (r as f32) > value {
        r.wrapping_sub(1) // -fwrapv
    } else {
        r
    }
}

/// `UMFloorUnsigned`: as [`um_floor`], on the `unsigned long` the rounding gives.
#[inline]
pub fn um_floor_unsigned(value: f32) -> c_ulong {
    let r = float_to_long(value) as c_ulong;
    if (r as f32) > value {
        r.wrapping_sub(1)
    } else {
        r
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn um_floor_is_round_then_step_down() {
        assert_eq!(um_floor(2.5), 2);
        assert_eq!(um_floor(3.5), 3);
        assert_eq!(um_floor(1.5), 1);
        assert_eq!(um_floor(-0.5), -1);
        assert_eq!(um_floor(-2.5), -3);
        assert_eq!(um_floor(-1.0), -1);
        assert_eq!(um_floor(0.9999), 0);
        assert_eq!(um_floor(-1e-40), -1);
        assert_eq!(um_floor(8388609.0), 8388609);
    }

    /// The UCRT's `lrintf` at the edges, printed by a scratch clang-cl program over `LowLevelEngineCore.h`
    /// (`lrintf`, `UMFloor`, `UMFloorUnsigned` of NaN, infinities, +-1e30, +-3e9, +-2^31 and the floats beside them).
    #[cfg(windows)]
    #[test]
    fn out_of_range_follows_the_ucrt() {
        for v in [f32::NAN, -f32::NAN, f32::INFINITY, 1e30, 3e9, 2147483904.0, 4294967296.0] {
            assert_eq!((float_to_long(v), um_floor(v), um_floor_unsigned(v)), (0, 0, 0), "{v}");
        }
        for v in [f32::NEG_INFINITY, -1e30, -3e9, -2147483904.0] {
            assert_eq!((float_to_long(v), um_floor(v), um_floor_unsigned(v)), (0, -1, 4294967295), "{v}");
        }
        assert_eq!(
            (float_to_long(2147483648.0), um_floor(2147483648.0), um_floor_unsigned(2147483648.0)),
            (-2147483648, -2147483648, 2147483648)
        );
        assert_eq!(
            (float_to_long(-2147483648.0), um_floor(-2147483648.0), um_floor_unsigned(-2147483648.0)),
            (-2147483648, -2147483648, 2147483647)
        );
        assert_eq!((float_to_long(-2147483520.0), um_floor_unsigned(-2147483520.0)), (-2147483520, 2147483775));
        assert_eq!(um_floor_unsigned(-2.5), 4294967293);
    }

    #[test]
    fn um_floor_unsigned_wraps_like_the_unsigned_cast() {
        assert_eq!(um_floor_unsigned(2.5), 2);
        assert_eq!(um_floor_unsigned(0.4), 0);
        assert_eq!(um_floor_unsigned(-0.4), c_ulong::MAX);
    }

    #[test]
    fn round_to_nearest_rounds_each_edge_half_up() {
        let mut r = UiRect::new(0.5, 1.5, -0.5, 2.49);
        r.round_to_nearest();
        assert_eq!(r, UiRect::new(1.0, 2.0, 0.0, 2.0));
    }

    #[test]
    fn contains_is_inclusive_and_refuses_nan() {
        let r = UiRect::new(0.0, 0.0, 10.0, 5.0);
        assert!(r.contains(UiPoint { x: 10.0, y: 5.0 }));
        assert!(r.contains(UiPoint { x: 0.0, y: 0.0 }));
        assert!(!r.contains(UiPoint { x: 10.01, y: 1.0 }));
        assert!(!r.contains(UiPoint { x: f32::NAN, y: 1.0 }));
        assert_eq!(r.width(), 10.0);
        assert_eq!(r.height(), 5.0);
        assert_eq!(r.top_left(), UiPoint { x: 0.0, y: 0.0 });
    }
}
