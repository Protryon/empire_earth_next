//! The box selection's rectangle (`src/Empire Earth/User Interface Thread/EEUICSelectUnits.cpp`): the press's point,
//! the drag's end (rounded, clamped to the game view), when a press becomes a drag, the box from the two points (as
//! the move and the release each build it) and the eight lines the box is drawn with. The C++ keeps the control: its
//! members, the mouse capture, the dirty rectangles, the clip to the view (`GERectangle::And`, `ee_ge`) and the
//! lasso's selection; the picker's frustum over the box is `pick.rs` and `ee_ge`'s `frustum_world_planes`.
//!
//! This is the user interface's, not the simulation's: the selection reaches the simulation only as the lasso's
//! message.

use ee_ge::types::{Ge2DPoint, GeRectangle};
use ee_ge::um::lrintf;

/// `UMFloatToLong` of each coordinate, back to floats.
fn rounded(x: f32, y: f32) -> Ge2DPoint {
    Ge2DPoint { x: lrintf(x) as f32, y: lrintf(y) as f32 }
}

/// `GERectangle::Contains(point)`: edges included.
fn contains(r: &GeRectangle, p: Ge2DPoint) -> bool {
    p.x >= r.left && p.x <= r.right && p.y >= r.top && p.y <= r.bottom
}

/// `StartSelection`'s drag start: the press rounded to whole pixels.
pub fn start_point(location: Ge2DPoint) -> Ge2DPoint {
    rounded(location.x, location.y)
}

/// `MouseMove`'s drag end: the mouse rounded, clamped first to the game view (a pixel short of its bottom) when it
/// is outside.
pub fn drag_end(location: Ge2DPoint, view: &GeRectangle) -> Ge2DPoint {
    if contains(view, location) {
        return rounded(location.x, location.y);
    }
    let y = if location.y < view.top {
        view.top
    } else if location.y > view.bottom - 1.0 {
        view.bottom - 1.0
    } else {
        location.y
    };
    let x = if location.x < view.left {
        view.left
    } else if location.x > view.right {
        view.right
    } else {
        location.x
    };
    rounded(x, y)
}

/// `kDragDistance`: how far the mouse moves from the press before a drag starts, in pixels.
pub(crate) const DRAG_DISTANCE: f32 = 10.0;

/// Whether the drag has gone far enough to start (`MouseMove`, the control shown and not yet dragging).
pub fn starts_drag(start: Ge2DPoint, end: Ge2DPoint) -> bool {
    start.distance_to(end) >= DRAG_DISTANCE
}

/// `MouseMove`'s box: each side from the smaller and the larger coordinate (the end's on a tie).
pub fn drag_box(start: Ge2DPoint, end: Ge2DPoint) -> GeRectangle {
    let mut r = GeRectangle::default();
    if start.x < end.x {
        r.left = start.x;
        r.right = end.x;
    } else {
        r.right = start.x;
        r.left = end.x;
    }
    if start.y < end.y {
        r.top = start.y;
        r.bottom = end.y;
    } else {
        r.bottom = start.y;
        r.top = end.y;
    }
    r
}

/// `MouseLButtonUp`'s box: the start as its top left, the end as its bottom right, each pair swapped when reversed.
pub fn release_box(start: Ge2DPoint, end: Ge2DPoint) -> GeRectangle {
    let mut r = GeRectangle { left: start.x, top: start.y, right: end.x, bottom: end.y };
    if r.left > r.right {
        std::mem::swap(&mut r.left, &mut r.right);
    }
    if r.top > r.bottom {
        std::mem::swap(&mut r.top, &mut r.bottom);
    }
    r
}

/// The box's lines (`DrawPolygons`): four shadow lines a pixel down and right, then four white ones, each a start
/// and an end.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxSelectLines {
    pub points: [[Ge2DPoint; 2]; 8],
}

/// `DrawPolygons`' lines round the box `r` (as of the last `CopyData`).
pub fn box_lines(r: &GeRectangle) -> EeGfxSelectLines {
    let p = |x: f32, y: f32| Ge2DPoint { x, y };
    let (l, t, rt, b) = (r.left, r.top, r.right, r.bottom);
    EeGfxSelectLines {
        points: [
            [p(l + 1.0, t + 1.0), p(rt + 1.0, t + 1.0)],
            [p(l + 1.0, t + 1.0), p(l + 1.0, b + 1.0)],
            [p(l + 1.0, b), p(rt + 1.0, b)],
            [p(rt + 1.0, t + 1.0), p(rt + 1.0, b + 1.0)],
            [p(l, t), p(rt + 1.0, t)],
            [p(l, t), p(l, b)],
            [p(l, b - 1.0), p(rt, b - 1.0)],
            [p(rt, t), p(rt, b)],
        ],
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn view() -> GeRectangle {
        GeRectangle { left: 0.0, top: 20.0, right: 800.0, bottom: 548.0 }
    }

    #[test]
    fn the_drag_end_rounds_and_clamps_to_the_view() {
        let v = view();
        assert_eq!(drag_end(Ge2DPoint { x: 10.5, y: 30.5 }, &v), Ge2DPoint { x: 10.0, y: 30.0 });
        assert_eq!(drag_end(Ge2DPoint { x: 11.5, y: 31.6 }, &v), Ge2DPoint { x: 12.0, y: 32.0 });
        assert_eq!(drag_end(Ge2DPoint { x: -5.0, y: 900.0 }, &v), Ge2DPoint { x: 0.0, y: 547.0 });
        assert_eq!(drag_end(Ge2DPoint { x: 900.0, y: 0.0 }, &v), Ge2DPoint { x: 800.0, y: 20.0 });
        // on the bottom edge: inside, not clamped
        assert_eq!(drag_end(Ge2DPoint { x: 5.0, y: 548.0 }, &v), Ge2DPoint { x: 5.0, y: 548.0 });
    }

    #[test]
    fn a_drag_starts_ten_pixels_out() {
        let s = Ge2DPoint { x: 100.0, y: 100.0 };
        assert!(!starts_drag(s, Ge2DPoint { x: 106.0, y: 107.0 }));
        assert!(starts_drag(s, Ge2DPoint { x: 106.0, y: 108.0 }));
    }

    #[test]
    fn both_boxes_order_their_corners() {
        let a = Ge2DPoint { x: 50.0, y: 10.0 };
        let b = Ge2DPoint { x: 20.0, y: 40.0 };
        let want = GeRectangle { left: 20.0, top: 10.0, right: 50.0, bottom: 40.0 };
        assert_eq!(drag_box(a, b), want);
        assert_eq!(release_box(a, b), want);
        let lines = box_lines(&want);
        assert_eq!(lines.points[4], [Ge2DPoint { x: 20.0, y: 10.0 }, Ge2DPoint { x: 51.0, y: 10.0 }]);
        assert_eq!(lines.points[6][0], Ge2DPoint { x: 20.0, y: 39.0 });
    }
}
