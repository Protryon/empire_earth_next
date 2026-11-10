//! `GEPhysicalModel`'s per-triangle tests (`GEPhysicalModel.cpp`): a line against one model triangle (the inner step
//! of `Intersects(location, model, frame, line, ...)`) and a triangle's bounding box (`ComputeTriangleBox`, the
//! model-against-model test's step). The C++ keeps the walks over the model, its materials, sub-models and vertex
//! arrays, and calls these per triangle.
//!
//! Units ask whether a projectile's line meets a target's model (`EEUnitClass.cpp`), so this is simulation code
//! (RUST_PLAN 2.1): the C++'s operations in its order, through the same `ee_ge` functions the C++ math forwards to.

use crate::types::{Ge3DLine, Ge3DPlane, Ge3DPoint, GeBoundingBox, GeTransformation, GeVector};
use crate::viewport::GeViewport;

/// One triangle of `Intersects(line)`: the plane through the triangle, the line's crossing with it, then whether the
/// line from the first corner through the crossing meets the opposite edge beyond the crossing. With `exact` the
/// crossing must lie on the segment (0..1). Returns the answer and the distance whenever the C++ wrote it (the plane
/// was not parallel), hit or not.
pub fn line_hits_triangle(
    line: &Ge3DLine,
    p1: Ge3DPoint,
    p2: Ge3DPoint,
    p3: Ge3DPoint,
    exact: bool,
) -> (bool, Option<f32>) {
    let side1 = GeVector::new(p2.x - p1.x, p2.y - p1.y, p2.z - p1.z);
    let side2 = GeVector::new(p3.x - p1.x, p3.y - p1.y, p3.z - p1.z);
    let plane = Ge3DPlane { point: p1, normal: side1.cross(side2) };
    let Some(distance) = line.intersects_plane(&plane) else {
        return (false, None);
    };
    let mut result = true;
    let hit = line.point_at(distance);
    if hit.x != p1.x || hit.y != p1.y || hit.z != p1.z {
        // Define(start, end): the direction is end - start
        let edge1 = Ge3DLine { start: p1, direction: GeVector::new(hit.x - p1.x, hit.y - p1.y, hit.z - p1.z) };
        let edge2 = Ge3DLine { start: p2, direction: GeVector::new(p3.x - p2.x, p3.y - p2.y, p3.z - p2.z) };
        let crossing = edge1.intersects_line(&edge2);
        result = crossing.hit;
        if result {
            let (t1, t2) = crossing.params.expect("a hit has its parameters");
            result = t1 >= 1.0 && t2 >= 0.0 && t2 <= 1.0;
        }
    }
    if exact {
        result = result && distance >= 0.0 && distance <= 1.0;
    }
    (result, Some(distance))
}

/// `ComputeTriangleBox`: the axis-aligned box of a triangle (each corner after the first lowers the minimum, or else
/// raises the maximum), then `Define`.
pub fn triangle_box(p1: Ge3DPoint, p2: Ge3DPoint, p3: Ge3DPoint) -> GeBoundingBox {
    let mut minimum = p1;
    let mut maximum = p1;
    for p in [p2, p3] {
        if p.x < minimum.x {
            minimum.x = p.x;
        } else if p.x > maximum.x {
            maximum.x = p.x;
        }
        if p.y < minimum.y {
            minimum.y = p.y;
        } else if p.y > maximum.y {
            maximum.y = p.y;
        }
        if p.z < minimum.z {
            minimum.z = p.z;
        } else if p.z > maximum.z {
            maximum.z = p.z;
        }
    }
    GeBoundingBox::new(minimum, maximum)
}

/// `GEPhysicalModel::Intersects(viewport, orthographic)`: whether a model placed at `location` with `scale`, at a
/// frame with `internal` transform, `center`, bounding `radius` and `bbox`, can be visible in the viewport: its
/// bounding sphere against the four side planes and the clip distances, or (orthographic) its box against the
/// orthographic rectangle. The graphics thread and the unit picker call it (not the simulation).
#[allow(clippy::too_many_arguments)]
pub fn model_visible(
    location: &GeTransformation,
    scale: f32,
    internal: &GeTransformation,
    center: Ge3DPoint,
    radius: f32,
    bbox: &GeBoundingBox,
    viewport: &GeViewport,
    orthographic: bool,
) -> bool {
    let mut transform = location.compose(internal);
    if scale != 1.0 {
        transform.scale(scale, scale, scale);
        for i in 0..3 {
            transform.m[i][3] = location.m[i][3];
        }
    }
    if !orthographic {
        let point = transform.apply_point(center);
        let camera_point = viewport.camera_inverse.apply_point(point);
        let radius = radius * scale;
        let scaled = |n: GeVector| GeVector::new(n.x * radius, n.y * radius, n.z * radius);
        let left = scaled(viewport.left_plane.normal);
        let right = scaled(viewport.right_plane.normal);
        let top = scaled(viewport.top_plane.normal);
        let bottom = scaled(viewport.bottom_plane.normal);
        if camera_point.y + radius < viewport.front_clipping_distance
            || camera_point.y - radius > viewport.rear_clipping_distance
        {
            return false;
        }
        let offset = |v: GeVector| Ge3DPoint::new(camera_point.x + v.x, camera_point.y + v.y, camera_point.z + v.z);
        viewport.left_plane.is_point_on_or_above(offset(left))
            && viewport.right_plane.is_point_on_or_above(offset(right))
            && viewport.top_plane.is_point_on_or_above(offset(top))
            && viewport.bottom_plane.is_point_on_or_above(offset(bottom))
    } else {
        let camera_transform = viewport.camera_inverse.compose(&transform);
        let b = bbox.transform(&camera_transform);
        let scale = -viewport.y_clipping_plane;
        let r = &viewport.orthographic_rectangle;
        !(b.top_right_back.y < viewport.front_clipping_distance
            || b.bottom_left_front.y > viewport.rear_clipping_distance
            || b.top_right_back.x < r.left
            || r.right < b.bottom_left_front.x
            || b.top_right_back.z < r.bottom * scale
            || b.bottom_left_front.z > r.top * scale)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::types::GeRectangle;

    fn viewport() -> GeViewport {
        GeViewport::new(
            GeRectangle { bottom: 768.0, left: 0.0, right: 1024.0, top: 0.0 },
            GeRectangle { bottom: 300.0, left: -400.0, right: 400.0, top: -300.0 },
            GeTransformation::IDENTITY,
            1.0,
            100.0,
            1.0,
        )
    }

    #[test]
    fn a_model_is_visible_inside_the_frustum_and_not_behind_the_camera() {
        let v = viewport();
        let b = GeBoundingBox::new(Ge3DPoint::new(-1.0, -1.0, -1.0), Ge3DPoint::new(1.0, 1.0, 1.0));
        let mut at = GeTransformation::IDENTITY;
        at.m[1][3] = 20.0;
        let id = GeTransformation::IDENTITY;
        assert!(model_visible(&at, 1.0, &id, Ge3DPoint::default(), 1.0, &b, &v, false));
        assert!(model_visible(&at, 2.0, &id, Ge3DPoint::default(), 1.0, &b, &v, true));
        at.m[1][3] = -20.0;
        assert!(!model_visible(&at, 1.0, &id, Ge3DPoint::default(), 1.0, &b, &v, false));
        assert!(!model_visible(&at, 1.0, &id, Ge3DPoint::default(), 1.0, &b, &v, true));
        // far to the side, perspective: outside the left plane; a huge radius reaches back in
        at.m[1][3] = 20.0;
        at.m[0][3] = -200.0;
        assert!(!model_visible(&at, 1.0, &id, Ge3DPoint::default(), 1.0, &b, &v, false));
        assert!(model_visible(&at, 1.0, &id, Ge3DPoint::default(), 300.0, &b, &v, false));
    }

    fn p(x: f32, y: f32, z: f32) -> Ge3DPoint {
        Ge3DPoint::new(x, y, z)
    }

    #[test]
    fn a_line_down_through_the_triangle_hits_it() {
        let line = Ge3DLine { start: p(0.2, 0.2, 5.0), direction: GeVector::new(0.0, 0.0, -10.0) };
        let (hit, d) = line_hits_triangle(&line, p(0.0, 0.0, 0.0), p(1.0, 0.0, 0.0), p(0.0, 1.0, 0.0), true);
        assert!(hit);
        assert_eq!(d, Some(0.5));
    }

    #[test]
    fn a_line_beside_the_triangle_misses_but_writes_the_distance() {
        let line = Ge3DLine { start: p(2.0, 2.0, 5.0), direction: GeVector::new(0.0, 0.0, -10.0) };
        let (hit, d) = line_hits_triangle(&line, p(0.0, 0.0, 0.0), p(1.0, 0.0, 0.0), p(0.0, 1.0, 0.0), false);
        assert!(!hit);
        assert_eq!(d, Some(0.5));
    }

    #[test]
    fn a_parallel_line_writes_nothing() {
        let line = Ge3DLine { start: p(0.2, 0.2, 5.0), direction: GeVector::new(1.0, 0.0, 0.0) };
        assert_eq!(
            line_hits_triangle(&line, p(0.0, 0.0, 0.0), p(1.0, 0.0, 0.0), p(0.0, 1.0, 0.0), false),
            (false, None)
        );
    }

    #[test]
    fn exact_needs_the_crossing_on_the_segment() {
        let line = Ge3DLine { start: p(0.2, 0.2, 5.0), direction: GeVector::new(0.0, 0.0, -1.0) };
        let (hit, d) = line_hits_triangle(&line, p(0.0, 0.0, 0.0), p(1.0, 0.0, 0.0), p(0.0, 1.0, 0.0), true);
        assert!(!hit);
        assert_eq!(d, Some(5.0));
        assert!(line_hits_triangle(&line, p(0.0, 0.0, 0.0), p(1.0, 0.0, 0.0), p(0.0, 1.0, 0.0), false).0);
    }

    #[test]
    fn a_crossing_at_the_first_corner_counts() {
        let line = Ge3DLine { start: p(0.0, 0.0, 5.0), direction: GeVector::new(0.0, 0.0, -10.0) };
        assert!(line_hits_triangle(&line, p(0.0, 0.0, 0.0), p(1.0, 0.0, 0.0), p(0.0, 1.0, 0.0), true).0);
    }

    #[test]
    fn triangle_box_orders_the_corners() {
        let b = triangle_box(p(1.0, 5.0, -1.0), p(-2.0, 6.0, 0.0), p(0.0, 4.0, 3.0));
        assert_eq!(b.bottom_left_front, p(-2.0, 4.0, -1.0));
        assert_eq!(b.top_right_back, p(1.0, 6.0, 3.0));
        assert_eq!((b.width, b.depth, b.height), (3.0, 2.0, 4.0));
    }
}
