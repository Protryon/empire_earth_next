//! The frame capture's view tests: the effects' bounding-sphere frustum test (`EEEffect2D::Queue`,
//! `EEEffect3D::Queue`: the sphere's six extreme points through the view's normalisation, each against one side of
//! the screen or depth range) and `GetUnitOpacity`'s near-plane fade of a land building (`EERenderObject.cpp`: its
//! bounding sphere's near side within half a world unit of the front clipping plane fades it out). The C++ keeps the
//! effects' tile and visibility tests, the units' see-through modes and their models' centres.
//!
//! This is the renderer's, not the simulation's.

use ee_ge::types::Ge3DPoint;
use ee_ge::viewport::GeViewport;

/// `kEERenderObject_F44136C`: the fade's depth, in world units in front of the near plane.
pub(crate) const NEAR_FADE_DEPTH: f32 = 0.5;

/// `NormalizePerspectiveCameraPoint` / `NormalizeOrthographicCameraPoint`: a camera-space point to the normalised
/// view (x across, y up, z the depth) and its w.
fn normalize(vp: &GeViewport, orthographic: bool, p: Ge3DPoint) -> (Ge3DPoint, f32) {
    if !orthographic {
        let s = Ge3DPoint {
            x: p.x * vp.perspective_scale,
            y: p.z * vp.perspective_scale,
            z: p.y * vp.perspective_depth_scale + vp.perspective_depth_offset,
        };
        (s, p.y)
    } else {
        let s = Ge3DPoint {
            x: p.x * vp.orthographic_scale,
            y: p.z * vp.orthographic_scale,
            z: p.y * vp.orthographic_depth_scale + vp.orthographic_depth_offset,
        };
        (s, 1.0)
    }
}

/// The effects' frustum test: whether a sphere at world `location` of `radius` may show (the C++ queues it; else
/// `Queue` returns 10). Each extreme point is tested against one plane only, in the original's order: right of the
/// left edge, left of the right edge, above the bottom, below the top, behind the near plane, in front of the far.
pub fn effect_in_view(vp: &GeViewport, orthographic: bool, location: Ge3DPoint, radius: f32) -> bool {
    let c = vp.camera_inverse.apply_point(location);
    let mut p = Ge3DPoint { x: c.x + radius, y: c.y, z: c.z };
    let (s, w) = normalize(vp, orthographic, p);
    if w == 0.0 || s.x / w < -1.0 {
        return false;
    }
    p.x = c.x - radius;
    let (s, w) = normalize(vp, orthographic, p);
    if w == 0.0 || s.x / w > 1.0 {
        return false;
    }
    p.x = c.x;
    p.z = c.z + radius;
    let (s, w) = normalize(vp, orthographic, p);
    if w == 0.0 || s.y / w < -vp.y_clipping_plane {
        return false;
    }
    p.z = c.z - radius;
    let (s, w) = normalize(vp, orthographic, p);
    // the C++ compares in double (an owner-approved stand-in for the x87 compare): the same as in f32
    if w == 0.0 || s.y / w > vp.y_clipping_plane {
        return false;
    }
    p.z = c.z;
    p.y = c.y + radius;
    let (s, w) = normalize(vp, orthographic, p);
    if w <= 0.0 || s.z / w < 0.0 {
        return false;
    }
    p.y = c.y - radius;
    let (s, w) = normalize(vp, orthographic, p);
    if w >= 0.0 && (w <= 0.0 || s.z / w > 1.0) {
        return false;
    }
    true
}

/// `GetUnitOpacity`'s near fade of a land building whose model's centre is at world `center`, its bounding radius
/// `radius` scaled by `scale`: the translucency and opacity after the fade (unchanged when the sphere's near side
/// is more than half a unit beyond the front clipping plane; else the opacity is at most that distance over half a
/// unit, at least 0, and the unit is translucent).
pub fn near_fade(
    vp: &GeViewport,
    center: Ge3DPoint,
    radius: f32,
    scale: f32,
    translucent: bool,
    opacity: f32,
) -> (bool, f32) {
    let range = 1.0 / (vp.rear_clipping_distance - vp.front_clipping_distance);
    let limit = NEAR_FADE_DEPTH * range;
    let view = vp.camera_inverse.apply_point(center);
    let mut depth = (view.y - radius * scale - vp.front_clipping_distance) * range;
    let (mut translucent, mut opacity) = (translucent, opacity);
    if depth < limit {
        depth /= limit;
        translucent = true;
        if depth < 0.0 {
            depth = 0.0;
        }
        if depth < opacity {
            opacity = depth;
        }
    }
    (translucent, opacity)
}

#[cfg(test)]
mod tests {
    use super::*;
    use ee_ge::types::{GeRectangle, GeTransformation};

    fn viewport() -> GeViewport {
        // looking down +y from the origin
        GeViewport::new(
            GeRectangle { bottom: 600.0, left: 0.0, right: 800.0, top: 0.0 },
            GeRectangle { bottom: 30.0, left: -40.0, right: 40.0, top: -30.0 },
            GeTransformation::IDENTITY,
            1.0,
            50.0,
            1.0,
        )
    }

    #[test]
    fn spheres_ahead_show_and_behind_do_not() {
        let vp = viewport();
        assert!(effect_in_view(&vp, false, Ge3DPoint::new(0.0, 10.0, 0.0), 1.0));
        assert!(!effect_in_view(&vp, false, Ge3DPoint::new(0.0, -10.0, 0.0), 1.0));
        assert!(!effect_in_view(&vp, false, Ge3DPoint::new(100.0, 10.0, 0.0), 1.0));
        assert!(!effect_in_view(&vp, false, Ge3DPoint::new(0.0, 80.0, 0.0), 1.0));
    }

    #[test]
    fn buildings_fade_at_the_near_plane() {
        let vp = viewport();
        assert_eq!(near_fade(&vp, Ge3DPoint::new(0.0, 10.0, 0.0), 1.0, 1.0, false, 1.0), (false, 1.0));
        let (t, o) = near_fade(&vp, Ge3DPoint::new(0.0, 2.25, 0.0), 1.0, 1.0, false, 1.0);
        assert!(t && (o - 0.5).abs() < 1e-4, "{o}");
        assert_eq!(near_fade(&vp, Ge3DPoint::new(0.0, 0.5, 0.0), 1.0, 1.0, false, 1.0), (true, 0.0));
    }
}
