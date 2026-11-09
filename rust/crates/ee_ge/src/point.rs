//! `GE2DPoint` and `GE3DPoint` (`GE2DPoint.cpp`, `GE3DPoint.cpp`): distances, the side test, rotation about a
//! centre, and the interpolation along a segment.

use crate::types::{Ge2DPoint, Ge3DLine, Ge3DPoint, GeVector};
use crate::um::{sqrt, um_arctan2, um_cos, HALF_PI};

impl Ge2DPoint {
    pub const fn new(x: f32, y: f32) -> Self {
        Ge2DPoint { x, y }
    }

    /// `GetDirection(from, to)`: the cross product that tells which side of the line from-to the point lies on.
    pub fn direction(self, from: Ge2DPoint, to: Ge2DPoint) -> f32 {
        (to.y - self.y) * (from.x - to.x) - (from.y - to.y) * (to.x - self.x)
    }

    /// `DistanceTo` (`UMFastMath::Sqrt`).
    pub fn distance_to(self, other: Ge2DPoint) -> f32 {
        sqrt((self.x - other.x) * (self.x - other.x) + (self.y - other.y) * (self.y - other.y))
    }

    /// `DistanceToSquared`.
    pub fn distance_to_squared(self, other: Ge2DPoint) -> f32 {
        let dy = self.y - other.y;
        let dx = self.x - other.x;
        dx * dx + dy * dy
    }

    /// `RotatePoint(angle, centre[, out])`: the point turned by `angle` radians about `center`, through the table
    /// arc tangent and cosine.
    pub fn rotated(self, angle: f32, center: Ge2DPoint) -> Ge2DPoint {
        let delta = Ge2DPoint::new(self.x - center.x, self.y - center.y);
        let distance = delta.distance_to(Ge2DPoint::default());
        let angle = um_arctan2(delta.y, delta.x) + angle;
        Ge2DPoint { x: um_cos(angle) * distance + center.x, y: um_cos(HALF_PI - angle) * distance + center.y }
    }

    /// `InterpolateTo(to, step, out)`: the points every `step` along the segment, then `to` itself (alone when the
    /// points are equal). The C++ reads an uninitialized end parameter when the normalized direction is zero (the
    /// points within 0.0001 of each other but not equal); the end is 0 here, so only `to` is emitted.
    pub fn interpolate_to(self, to: Ge2DPoint, step: f32, mut push: impl FnMut(Ge2DPoint)) {
        if self != to {
            let start = Ge3DPoint::new(self.x, self.y, 0.0);
            let end = Ge3DPoint::new(to.x, to.y, 0.0);
            let mut direction = GeVector::new(to.x - self.x, to.y - self.y, 0.0);
            direction.normalize();
            direction.x *= step;
            direction.y *= step;
            let line = Ge3DLine { start, direction };
            let end_t = line.solve_for_x(end.x).or_else(|| line.solve_for_y(end.y)).unwrap_or(0.0);
            let mut t = 0.0f32;
            while t < end_t {
                let current = line.point_at(t);
                push(Ge2DPoint::new(current.x, current.y));
                t += step;
            }
        }
        push(to);
    }
}

impl Ge3DPoint {
    /// `DistanceTo` (`UMFastMath::Sqrt`).
    pub fn distance_to(self, other: Ge3DPoint) -> f32 {
        sqrt(
            (self.x - other.x) * (self.x - other.x)
                + (self.y - other.y) * (self.y - other.y)
                + (self.z - other.z) * (self.z - other.z),
        )
    }

    /// `DistanceToSquared`.
    pub fn distance_to_squared(self, other: Ge3DPoint) -> f32 {
        let dz = self.z - other.z;
        let dy = self.y - other.y;
        let dx = self.x - other.x;
        dx * dx + dy * dy + dz * dz
    }

    /// `InterpolateTo(to, step, out)`: as the 2D version, solving for x, else y, else z (the same uninitialized end
    /// is 0 here).
    pub fn interpolate_to(self, to: Ge3DPoint, step: f32, mut push: impl FnMut(Ge3DPoint)) {
        if self != to {
            let mut direction = GeVector::new(to.x - self.x, to.y - self.y, to.z - self.z);
            direction.normalize();
            direction.x *= step;
            direction.y *= step;
            direction.z *= step;
            let line = Ge3DLine { start: self, direction };
            let end_t = line
                .solve_for_x(to.x)
                .or_else(|| line.solve_for_y(to.y))
                .or_else(|| line.solve_for_z(to.z))
                .unwrap_or(0.0);
            let mut t = 0.0f32;
            while t < end_t {
                push(line.point_at(t));
                t += step;
            }
        }
        push(to);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn side_of_a_line() {
        let from = Ge2DPoint::new(0.0, 0.0);
        let to = Ge2DPoint::new(1.0, 0.0);
        assert_eq!(Ge2DPoint::new(0.5, 1.0).direction(from, to), 1.0);
        assert_eq!(Ge2DPoint::new(0.5, -1.0).direction(from, to), -1.0);
        assert_eq!(Ge2DPoint::new(3.0, 0.0).direction(from, to), 0.0);
    }

    #[test]
    fn distances() {
        assert_eq!(Ge2DPoint::new(0.0, 0.0).distance_to(Ge2DPoint::new(3.0, 4.0)), 5.0);
        assert_eq!(Ge2DPoint::new(0.0, 0.0).distance_to_squared(Ge2DPoint::new(3.0, 4.0)), 25.0);
        assert_eq!(Ge3DPoint::new(1.0, 2.0, 3.0).distance_to(Ge3DPoint::new(3.0, 5.0, 9.0)), 7.0);
        assert_eq!(Ge3DPoint::new(1.0, 2.0, 3.0).distance_to_squared(Ge3DPoint::new(3.0, 5.0, 9.0)), 49.0);
    }

    #[test]
    fn rotation_about_a_centre_is_table_accurate() {
        let p = Ge2DPoint::new(2.0, 1.0).rotated(HALF_PI, Ge2DPoint::new(1.0, 1.0));
        assert!((p.x - 1.0).abs() < 0.001 && (p.y - 2.0).abs() < 0.001, "{p:?}");
        // a point on its centre stays there
        assert_eq!(Ge2DPoint::new(1.0, 1.0).rotated(0.7, Ge2DPoint::new(1.0, 1.0)), Ge2DPoint::new(1.0, 1.0));
    }

    #[test]
    fn interpolation_ends_with_the_target() {
        let mut out = Vec::new();
        Ge2DPoint::new(0.0, 0.0).interpolate_to(Ge2DPoint::new(2.0, 0.0), 1.0, |p| out.push(p));
        assert_eq!(out, vec![Ge2DPoint::new(0.0, 0.0), Ge2DPoint::new(1.0, 0.0), Ge2DPoint::new(2.0, 0.0)]);
        out.clear();
        Ge2DPoint::new(5.0, 5.0).interpolate_to(Ge2DPoint::new(5.0, 5.0), 1.0, |p| out.push(p));
        assert_eq!(out, vec![Ge2DPoint::new(5.0, 5.0)]);
        let mut out3 = Vec::new();
        Ge3DPoint::new(0.0, 0.0, 0.0).interpolate_to(Ge3DPoint::new(0.0, 0.0, 1.5), 1.0, |p| out3.push(p));
        assert_eq!(
            out3,
            vec![Ge3DPoint::new(0.0, 0.0, 0.0), Ge3DPoint::new(0.0, 0.0, 1.0), Ge3DPoint::new(0.0, 0.0, 1.5)]
        );
    }

    #[test]
    fn interpolation_of_nearly_equal_points_gives_only_the_target() {
        let mut out = Vec::new();
        Ge2DPoint::new(0.0, 0.0).interpolate_to(Ge2DPoint::new(0.00001, 0.0), 1.0, |p| out.push(p));
        assert_eq!(out, vec![Ge2DPoint::new(0.00001, 0.0)]);
    }
}
