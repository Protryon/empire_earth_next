//! The mini map's arithmetic (`src/Empire Earth/Map/EEMiniMap.cpp`): `SetMap`'s four transformations (the map to
//! the 128-pixel mini map, rotated a quarter turn and flipped, centred; the map to the battle grid of the smaller
//! side, at most 64; and their inverses) with the size of one mini-map pixel on the map, `ProjectToGround` (a
//! screen point's ray onto a height, or its far point when it misses) and `Refresh`'s view trapezoid (the camera
//! tilted 30 degrees, at least height 5, its sides shortened to the near edge's width, at least 32). The C++ keeps
//! the class, its surfaces, the dots and the drawing.
//!
//! The camera's `Update` refreshes the trapezoid every tick (EE-SIM included) and `SetMap` runs at a game's start:
//! simulation-run code under RUST_PLAN 2.1 (the C++'s f32 order), though nothing it computes feeds the simulation.

use ee_ge::transform::RotationOrder;
use ee_ge::types::{Ge2DPoint, Ge3DLine, GeRectangle, GeTransformation, GeVector};
use ee_ge::um::{lrintf, um_arctan2};
use ee_ge::viewport::GeViewport;

use crate::pick::screen_ray;

/// `kMiniMapSize`, `kMiniMapLast`: the mini map's texture is 128 pixels square.
pub(crate) const MINI_MAP_SIZE: f32 = 128.0;
pub(crate) const MINI_MAP_LAST: f32 = 127.0;
const SQRT_2: f32 = 1.41421354;
const MINUS_QUARTER_PI: f32 = -0.785398185;
const QUARTER_PI: f32 = 0.785398185;
const MINUS_SIXTH_PI: f32 = -0.523598790;
/// `kMinimumHeight`: `Refresh`'s lowest camera height.
pub(crate) const MINIMUM_HEIGHT: f32 = 5.0;
/// `kMinimumWidth`: the trapezoid's narrowest near edge.
pub(crate) const MINIMUM_WIDTH: f32 = 32.0;

/// `GETransformation::Apply(GE2DPoint, GE2DPoint)`.
fn apply_2d(t: &GeTransformation, p: Ge2DPoint) -> Ge2DPoint {
    let m = &t.m;
    Ge2DPoint { x: p.y * m[0][1] + p.x * m[0][0] + m[0][3], y: p.x * m[1][0] + p.y * m[1][1] + m[1][3] }
}

/// `UMCeilUnsigned`.
fn ceil_unsigned(value: f32) -> u32 {
    let mut r = lrintf(value) as u32;
    if (r as f32) < value {
        r = r.wrapping_add(1);
    }
    r
}

/// `SetMap`'s results.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxMiniMapTransforms {
    pub map_to_pixel: GeTransformation,
    pub pixel_to_map: GeTransformation,
    pub map_to_cell: GeTransformation,
    pub cell_to_map: GeTransformation,
    /// `mL12178`, `mL1217C`: one mini-map pixel's extent on the map, across and down, rounded up
    pub pixel_width: u32,
    pub pixel_height: u32,
}

/// The rotated, flipped, centred map-to-grid transformation for a grid of `side` pixels and its inverse.
fn grid(
    height: i32,
    width: i32,
    size: i32,
    side: f32,
    last: f32,
    diagonal: f32,
) -> (GeTransformation, GeTransformation) {
    let mut to = GeTransformation::IDENTITY;
    to.rotate(MINUS_QUARTER_PI, 0.0, 0.0);
    let scale = last / diagonal;
    let flip = -scale;
    to.scale(1.0, flip, 1.0);
    let offset = apply_2d(&to, Ge2DPoint { x: 0.0, y: height as f32 });
    to.m[0][3] = -offset.x + to.m[0][3];
    to.m[1][3] = to.m[1][3] - offset.y;
    to.scale(scale, 1.0, 1.0);
    let mut edge = apply_2d(&to, Ge2DPoint { x: size as f32 + 0.5, y: width as f32 + 0.5 });
    edge.x = side - edge.x - 1.0;
    edge.x = if edge.x < 4.0 { 0.0 } else { edge.x * 0.5 };
    edge.y = side - edge.y - 1.0;
    edge.y = if edge.y < 4.0 { 0.0 } else { edge.y * 0.5 };
    to.m[0][3] = edge.x + to.m[0][3];
    to.m[1][3] = edge.y + to.m[1][3];

    let mut back = GeTransformation::IDENTITY;
    back.m[0][3] = -edge.x + back.m[0][3];
    back.m[1][3] = back.m[1][3] - edge.y;
    back.scale(1.0 / scale, 1.0, 1.0);
    back.m[0][3] = offset.x + back.m[0][3];
    back.m[1][3] = offset.y + back.m[1][3];
    back.scale(1.0, 1.0 / flip, 1.0);
    back.rotate(QUARTER_PI, 0.0, 0.0);
    (to, back)
}

/// `SetMap` for a map of `height` (`mL195610`) by `width` (`mL195614`) tiles and `size` tiles per side.
pub fn transforms(height: i32, width: i32, size: i32) -> EeGfxMiniMapTransforms {
    let (larger, mut smaller) =
        if height > width { (height as f32, width as f32) } else { (width as f32, height as f32) };
    smaller = if smaller < 64.0 { smaller } else { 64.0 };
    let diagonal = larger * SQRT_2;
    let (map_to_pixel, pixel_to_map) = grid(height, width, size, MINI_MAP_SIZE, MINI_MAP_LAST, diagonal);
    let a = apply_2d(&pixel_to_map, Ge2DPoint { x: 0.0, y: 0.0 });
    let b = apply_2d(&pixel_to_map, Ge2DPoint { x: 1.0, y: 1.0 });
    let mut d = b.x - a.x;
    if d < 0.0 {
        d = -d;
    }
    let pixel_width = ceil_unsigned(d);
    let a = apply_2d(&pixel_to_map, Ge2DPoint { x: 0.0, y: 1.0 });
    let b = apply_2d(&pixel_to_map, Ge2DPoint { x: 1.0, y: 0.0 });
    let mut d = b.y - a.y;
    if d < 0.0 {
        d = -d;
    }
    let pixel_height = ceil_unsigned(d);
    let (map_to_cell, cell_to_map) = grid(height, width, size, smaller, smaller - 1.0, diagonal);
    EeGfxMiniMapTransforms { map_to_pixel, pixel_to_map, map_to_cell, cell_to_map, pixel_width, pixel_height }
}

/// `ProjectToGround`: where a screen point's ray meets height `z` ahead of the near plane, else the ray's far point.
pub fn project_to_ground(vp: &GeViewport, orthographic: bool, point: Ge2DPoint, z: f32) -> Ge2DPoint {
    let ray = screen_ray(vp, orthographic, point);
    let line = Ge3DLine {
        start: ray.front,
        direction: GeVector { x: ray.rear.x - ray.front.x, y: ray.rear.y - ray.front.y, z: ray.rear.z - ray.front.z },
    };
    match line.solve_for_z_point(z) {
        Some((t, ground)) if t >= 0.0 => Ge2DPoint { x: ground.x, y: ground.y },
        _ => Ge2DPoint { x: ray.rear.x, y: ray.rear.y },
    }
}

/// `Refresh`'s corners: 0 bottom left, 1 bottom right, 2 top left, 3 top right (`mCorner0..3`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxMiniMapView {
    pub corners: [Ge2DPoint; 4],
}

/// `Refresh`: the view `vp` with its camera turned to the same heading, pitched 30 degrees down and at least 5 high,
/// its screen corners projected onto the ground and through `map_to_pixel`; the far corners pulled in so neither
/// side is longer than the near edge (at least 32 pixels).
pub fn view_trapezoid(vp: &GeViewport, orthographic: bool, map_to_pixel: &GeTransformation) -> EeGfxMiniMapView {
    let mut v = *vp;
    let mut camera = v.camera;
    let direction = camera.apply_vector(GeVector { x: 0.0, y: 1.0, z: 0.0 });
    camera.set_orientation(RotationOrder::Ypr, um_arctan2(-direction.x, direction.y), MINUS_SIXTH_PI, 0.0);
    if camera.m[2][3] < MINIMUM_HEIGHT {
        camera.m[2][3] = MINIMUM_HEIGHT;
    }
    v.set_camera(&camera);
    let r = v.view_rectangle;
    let corner = |x: f32, y: f32| apply_2d(map_to_pixel, project_to_ground(&v, orthographic, Ge2DPoint { x, y }, 0.0));
    let mut c2 = corner(r.left, r.top);
    let c0 = corner(r.left, r.bottom);
    let mut c3 = corner(r.right, r.top);
    let c1 = corner(r.right, r.bottom);
    let mut width = c1.distance_to(c0);
    if width < MINIMUM_WIDTH {
        width = MINIMUM_WIDTH;
    }
    let distance = c0.distance_to(c2);
    if distance > width {
        let scale = width / distance;
        c2 = Ge2DPoint { x: (c2.x - c0.x) * scale + c0.x, y: c0.y + (c2.y - c0.y) * scale };
    }
    let distance = c1.distance_to(c3);
    if distance > width {
        let scale = width / distance;
        c3 = Ge2DPoint { x: (c3.x - c1.x) * scale + c1.x, y: c1.y + (c3.y - c1.y) * scale };
    }
    EeGfxMiniMapView { corners: [c0, c1, c2, c3] }
}

/// `EEUIMiniMap::CenterCamera`'s mini-map point: the mouse from the map's top left (`origin`, in window pixels) in
/// mini-map units (the window's size over 800 x 600), clamped into the map's bounds (`sBounds`) when outside them.
pub fn center_point(mouse: Ge2DPoint, origin: Ge2DPoint, width: u32, height: u32, bounds: &GeRectangle) -> Ge2DPoint {
    let mut p = Ge2DPoint {
        x: (mouse.x - origin.x) / (width as f32 * 0.00125),
        y: (mouse.y - origin.y) / (height as f32 * 0.0016666667),
    };
    let inside = p.x >= bounds.left && p.x <= bounds.right && p.y >= bounds.top && p.y <= bounds.bottom;
    if !inside {
        if bounds.left > p.x {
            p.x = bounds.left;
        } else if bounds.right < p.x {
            p.x = bounds.right;
        }
        if p.y < bounds.top {
            p.y = bounds.top;
        } else if p.y > bounds.bottom {
            p.y = bounds.bottom;
        }
    }
    p
}

/// `EEUIMiniMap::MouseRButtonDown`'s mini-map point: the same mouse arithmetic as [`center_point`], but its clamp
/// makes two separate tests per axis (left then right, top then bottom), not `else if`: with inverted bounds a
/// point past both ends takes the second.
pub fn press_point(mouse: Ge2DPoint, origin: Ge2DPoint, width: u32, height: u32, bounds: &GeRectangle) -> Ge2DPoint {
    let mut p = Ge2DPoint {
        x: (mouse.x - origin.x) / (width as f32 * 0.00125),
        y: (mouse.y - origin.y) / (height as f32 * 0.0016666667),
    };
    let inside = p.x >= bounds.left && p.x <= bounds.right && p.y >= bounds.top && p.y <= bounds.bottom;
    if !inside {
        if bounds.left > p.x {
            p.x = bounds.left;
        }
        if bounds.right < p.x {
            p.x = bounds.right;
        }
        if p.y < bounds.top {
            p.y = bounds.top;
        }
        if p.y > bounds.bottom {
            p.y = bounds.bottom;
        }
    }
    p
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_pixel_transformation_inverts() {
        let t = transforms(128, 128, 128);
        for (x, y) in [(0.0, 0.0), (40.0, 70.0), (128.0, 0.0), (127.0, 128.0)] {
            let p = apply_2d(&t.map_to_pixel, Ge2DPoint { x, y });
            let q = apply_2d(&t.pixel_to_map, p);
            assert!((q.x - x).abs() < 1e-3 && (q.y - y).abs() < 1e-3, "{q:?}");
            let p = apply_2d(&t.map_to_cell, Ge2DPoint { x, y });
            let q = apply_2d(&t.cell_to_map, p);
            assert!((q.x - x).abs() < 1e-3 && (q.y - y).abs() < 1e-3, "{q:?}");
        }
        assert!(t.pixel_width >= 1 && t.pixel_height >= 1);
    }

    #[test]
    fn the_press_clamps_twice_per_axis() {
        // inverted bounds (left 10, right 0): the press takes the right edge, the camera's centre the left
        let b = GeRectangle { left: 10.0, top: 0.0, right: 0.0, bottom: 5.0 };
        let m = Ge2DPoint { x: -4.0, y: 2.0 };
        let o = Ge2DPoint { x: 0.0, y: 0.0 };
        assert_eq!(press_point(m, o, 800, 600, &b).x, 0.0);
        assert_eq!(center_point(m, o, 800, 600, &b).x, 10.0);
        let inside = Ge2DPoint { x: 4.0, y: 2.0 };
        let b = GeRectangle { left: 0.0, top: 0.0, right: 10.0, bottom: 5.0 };
        assert_eq!(press_point(inside, o, 800, 600, &b), Ge2DPoint { x: 4.0, y: 2.0 });
    }

    #[test]
    fn ceilings() {
        assert_eq!(ceil_unsigned(1.0), 1);
        assert_eq!(ceil_unsigned(1.2), 2);
        assert_eq!(ceil_unsigned(1.5), 2);
        assert_eq!(ceil_unsigned(2.5), 3);
    }
}
