//! The game pointer's arithmetic (`src/RTS Engine/Graphics/RTSMouse.cpp`): `ClampPosition` (the position kept inside
//! the bounds, the right and bottom edges excluded) and `Update`'s move (the raw mouse axes scaled by the bounds over
//! the desktop, the sensitivity and the smallest raw move seen so far). The C++ class keeps its members, locks,
//! surfaces and the input entities; it hands these functions plain values. The smallest moves are the unit's two
//! file statics (`sMinXValue`/`sMinYValue`), passed in and out; the C++ prints the "MinXValue now" debug lines when
//! the result says a minimum changed.

use ee_ge::types::{Ge2DPoint, GeRectangle};

/// `ClampPosition`: `position` inside `bounds` (left/top inclusive; past right/bottom: one less than the edge).
pub fn clamp_position(position: Ge2DPoint, bounds: &GeRectangle) -> Ge2DPoint {
    let mut p = position;
    if p.x < bounds.left {
        p.x = bounds.left;
    } else if p.x >= bounds.right {
        p.x = bounds.right - 1.0;
    }
    if p.y < bounds.top {
        p.y = bounds.top;
    } else if p.y >= bounds.bottom {
        p.y = bounds.bottom - 1.0;
    }
    p
}

/// What `Update`'s move did: the new position (clamped) when the mouse moved, and whether each smallest move
/// changed (the C++'s debug lines).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxMouseMove {
    pub position: Ge2DPoint,
    /// the mouse moved: `position` is the new one (else it is the old one, unclamped as before)
    pub moved: bool,
    pub min_x_changed: bool,
    pub min_y_changed: bool,
}

/// The pointer's inputs for `Update`'s move.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxMouseInput {
    pub position: Ge2DPoint,
    pub bounds: GeRectangle,
    pub desktop_width: u32,
    pub desktop_height: u32,
    pub sensitivity: f32,
    /// the axes' `GetNormalizedValue`
    pub move_x: i32,
    pub move_y: i32,
}

/// The magnitude as the C++'s `x < 0 ? -x : x` (two's complement: `LONG_MIN` stays itself).
#[inline]
fn magnitude(v: i32) -> i32 {
    if v < 0 {
        v.wrapping_neg()
    } else {
        v
    }
}

/// `Update`'s move unless frozen (the caller tests `mIsFrozen`): `min_x`/`min_y` are `sMinXValue`/`sMinYValue`.
pub fn update_move(input: &EeGfxMouseInput, min_x: &mut i32, min_y: &mut i32) -> EeGfxMouseMove {
    let b = &input.bounds;
    // (right - left) / (float)desktop * sensitivity, in that order
    let scale_x = (b.right - b.left) / input.desktop_width as f32 * input.sensitivity;
    let scale_y = (b.bottom - b.top) / input.desktop_height as f32 * input.sensitivity;
    let mut out = EeGfxMouseMove { position: input.position, ..Default::default() };
    let (mx, my) = (input.move_x, input.move_y);
    if mx == 0 && my == 0 {
        return out;
    }
    let mut dx = 0.0f32;
    let mut dy = 0.0f32;
    if mx != 0 {
        if magnitude(mx) < *min_x {
            *min_x = magnitude(mx);
            out.min_x_changed = true;
        }
        dx = (scale_x + 0.0625) * mx as f32 * 4.0 / *min_x as f32;
    }
    if my != 0 {
        if magnitude(my) < *min_y {
            *min_y = magnitude(my);
            out.min_y_changed = true;
        }
        dy = (scale_y + 0.0625) * my as f32 * 4.0 / *min_y as f32;
    }
    let moved = Ge2DPoint { x: dx + input.position.x, y: dy + input.position.y };
    out.position = clamp_position(moved, b);
    out.moved = true;
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    fn rect(left: f32, top: f32, right: f32, bottom: f32) -> GeRectangle {
        GeRectangle { bottom, left, right, top }
    }

    #[test]
    fn clamp_keeps_the_right_and_bottom_edges_out() {
        let b = rect(0.0, 0.0, 800.0, 600.0);
        assert_eq!(clamp_position(Ge2DPoint { x: -3.0, y: 700.0 }, &b), Ge2DPoint { x: 0.0, y: 599.0 });
        assert_eq!(clamp_position(Ge2DPoint { x: 800.0, y: 0.0 }, &b), Ge2DPoint { x: 799.0, y: 0.0 });
        assert_eq!(clamp_position(Ge2DPoint { x: 799.5, y: 10.0 }, &b), Ge2DPoint { x: 799.5, y: 10.0 });
        // NaN compares false both ways: it stays
        assert!(clamp_position(Ge2DPoint { x: f32::NAN, y: 1.0 }, &b).x.is_nan());
    }

    #[test]
    fn move_scales_by_the_smallest_move_seen() {
        let input = EeGfxMouseInput {
            position: Ge2DPoint { x: 400.0, y: 300.0 },
            bounds: rect(0.0, 0.0, 800.0, 600.0),
            desktop_width: 1600,
            desktop_height: 1200,
            sensitivity: 1.0,
            move_x: 3,
            move_y: -6,
        };
        let (mut mx, mut my) = (100, 100);
        let out = update_move(&input, &mut mx, &mut my);
        assert_eq!((mx, my), (3, 6));
        assert!(out.moved && out.min_x_changed && out.min_y_changed);
        // (0.5 + 0.0625) * 3 * 4 / 3 = 2.25; (0.5 + 0.0625) * -6 * 4 / 6 = -2.25
        assert_eq!(out.position, Ge2DPoint { x: 402.25, y: 297.75 });
        let still = update_move(&EeGfxMouseInput { move_x: 0, move_y: 0, ..input }, &mut mx, &mut my);
        assert_eq!(still, EeGfxMouseMove { position: input.position, ..Default::default() });
        let far = update_move(&EeGfxMouseInput { move_x: 30000, move_y: i32::MIN, ..input }, &mut mx, &mut my);
        assert_eq!((mx, my), (3, i32::MIN));
        assert_eq!(far.position.x, 799.0);
        assert!(!far.min_x_changed && far.min_y_changed);
    }
}
