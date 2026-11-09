//! `GE3DLine` (`GE3DLine.cpp`, the line block of `GE2DPoint.cpp`) and `GE3DPlane` (`GE3DPlane.cpp`): intersections,
//! the axis solvers, the plane through three points and the point-side tests.

use crate::types::{Ge3DLine, Ge3DPlane, Ge3DPoint, GeVector};
use crate::um::sqrt;

/// `IS_ZERO`: within 0.0001 of zero.
#[inline]
fn is_zero(x: f32) -> bool {
    (-0.0001..=0.0001).contains(&x)
}

/// `x < 0 ? -x : x`, as the C++ writes its absolute values (no `fabs`).
#[inline]
pub(crate) fn abs(x: f32) -> f32 {
    if x < 0.0 {
        -x
    } else {
        x
    }
}

/// `GE3DLine::Intersects(GE3DLine)`'s tolerance in the third axis.
const LINE_TOLERANCE: f32 = 0.05;

/// `GE3DLine::Intersects(GE3DLine)`: the answer and, when a branch computed them, the parameters on each line (the
/// C++ writes them even when the lines miss in the third axis, and leaves them alone when every pair is parallel).
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct LineHit {
    pub hit: bool,
    pub params: Option<(f32, f32)>,
}

impl Ge3DLine {
    /// `GetPointAt`: start + t * direction.
    #[inline]
    pub fn point_at(&self, t: f32) -> Ge3DPoint {
        Ge3DPoint {
            x: self.direction.x * t + self.start.x,
            y: self.direction.y * t + self.start.y,
            z: self.direction.z * t + self.start.z,
        }
    }

    /// `Intersects(point, tolerance, t)`: whether the line passes within `tolerance` of the point, and the nearer of
    /// the two crossing parameters.
    pub fn intersects_point(&self, point: Ge3DPoint, tolerance: f32) -> Option<f32> {
        let s = self.start;
        let d = self.direction;
        let delta = GeVector::new(s.x - point.x, s.y - point.y, s.z - point.z);
        let a = d.magnitude_squared();
        let b = 2.0 * (d.x * delta.x + d.y * delta.y + d.z * delta.z);
        let c = point.x * point.x + point.y * point.y + point.z * point.z + s.x * s.x + s.y * s.y + s.z * s.z
            - 2.0 * (point.x * s.x + point.y * s.y + point.z * s.z)
            - tolerance * tolerance;
        let discriminant = b * b - 4.0 * a * c;
        if discriminant < 0.0 {
            return None;
        }
        if discriminant > 0.0 {
            let root = sqrt(discriminant);
            let inverse = 1.0 / (2.0 * a);
            let t1 = (root - b) * inverse;
            let t2 = (-b - root) * inverse;
            Some(if abs(t1) < abs(t2) { t1 } else { t2 })
        } else {
            Some(-b / (2.0 * a))
        }
    }

    /// `Intersects(line, t, lineT)`: the lines meet when, in the first pair of axes where they are not parallel, the
    /// crossing agrees in the third axis within 0.05.
    pub fn intersects_line(&self, other: &Ge3DLine) -> LineHit {
        let a = self;
        let b = other;
        let delta = GeVector::new(a.start.x - b.start.x, a.start.y - b.start.y, a.start.z - b.start.z);
        let xy = b.direction.y * a.direction.x - b.direction.x * a.direction.y;
        let xz = a.direction.x * b.direction.z - b.direction.x * a.direction.z;
        let yz = a.direction.y * b.direction.z - b.direction.y * a.direction.z;
        let within = |difference: f32| difference > -LINE_TOLERANCE && difference < LINE_TOLERANCE;
        if is_zero(xy) || (is_zero(delta.x) && is_zero(delta.y)) {
            if is_zero(yz) || (is_zero(delta.y) && is_zero(delta.z)) {
                if !is_zero(xz) && !(is_zero(delta.x) && is_zero(delta.z)) {
                    let inverse = 1.0 / xz;
                    let t = (b.direction.x * delta.z - b.direction.z * delta.x) * inverse;
                    let u = (a.direction.x * delta.z - a.direction.z * delta.x) * inverse;
                    let difference = a.direction.y * t + a.start.y - (b.direction.y * u + b.start.y);
                    return LineHit { hit: within(difference), params: Some((t, u)) };
                }
                LineHit { hit: false, params: None }
            } else {
                let inverse = 1.0 / yz;
                let t = (b.direction.y * delta.z - delta.y * b.direction.z) * inverse;
                let u = (a.direction.y * delta.z - delta.y * a.direction.z) * inverse;
                let difference = a.direction.x * t + a.start.x - (b.direction.x * u + b.start.x);
                LineHit { hit: within(difference), params: Some((t, u)) }
            }
        } else {
            let inverse = 1.0 / xy;
            let t = (b.direction.x * delta.y - b.direction.y * delta.x) * inverse;
            let u = (a.direction.x * delta.y - a.direction.y * delta.x) * inverse;
            let difference = a.direction.z * t + a.start.z - (b.direction.z * u + b.start.z);
            LineHit { hit: within(difference), params: Some((t, u)) }
        }
    }

    /// `IntersectsIn2D`: the crossing in the xy plane, `None` when parallel.
    pub fn intersects_in_2d(&self, other: &Ge3DLine) -> Option<(f32, f32)> {
        let a = self;
        let b = other;
        let delta = GeVector::new(a.start.x - b.start.x, a.start.y - b.start.y, 0.0);
        let denominator = b.direction.y * a.direction.x - b.direction.x * a.direction.y;
        if denominator != 0.0 {
            let inverse = 1.0 / denominator;
            let t = (b.direction.x * delta.y - b.direction.y * delta.x) * inverse;
            let u = (a.direction.x * delta.y - a.direction.y * delta.x) * inverse;
            Some((t, u))
        } else {
            None
        }
    }

    /// `Intersects(plane, t)`: the parameter of the crossing point, `None` when parallel.
    pub fn intersects_plane(&self, plane: &Ge3DPlane) -> Option<f32> {
        let p = plane.point;
        let delta = GeVector::new(p.x - self.start.x, p.y - self.start.y, p.z - self.start.z);
        let denominator = plane.normal.dot(self.direction);
        if denominator != 0.0 {
            Some(delta.dot(plane.normal) / denominator)
        } else {
            None
        }
    }

    /// `SolveForX(x, t)`.
    pub fn solve_for_x(&self, x: f32) -> Option<f32> {
        let direction = self.direction.x;
        (direction != 0.0).then(|| (x - self.start.x) / direction)
    }

    /// `SolveForY(y, t)`.
    pub fn solve_for_y(&self, y: f32) -> Option<f32> {
        let direction = self.direction.y;
        (direction != 0.0).then(|| (y - self.start.y) / direction)
    }

    /// `SolveForZ(z, t)`.
    pub fn solve_for_z(&self, z: f32) -> Option<f32> {
        let direction = self.direction.z;
        (direction != 0.0).then(|| (z - self.start.z) / direction)
    }

    /// `SolveForX(x, t, point)`: the parameter and the point (its x is `x` itself).
    pub fn solve_for_x_point(&self, x: f32) -> Option<(f32, Ge3DPoint)> {
        let t = self.solve_for_x(x)?;
        let d = self.direction;
        Some((t, Ge3DPoint { x, y: d.y * t + self.start.y, z: d.z * t + self.start.z }))
    }

    /// `SolveForY(y, t, point)`.
    pub fn solve_for_y_point(&self, y: f32) -> Option<(f32, Ge3DPoint)> {
        let t = self.solve_for_y(y)?;
        let d = self.direction;
        Some((t, Ge3DPoint { x: d.x * t + self.start.x, y, z: d.z * t + self.start.z }))
    }

    /// `SolveForZ(z, t, point)`.
    pub fn solve_for_z_point(&self, z: f32) -> Option<(f32, Ge3DPoint)> {
        let t = self.solve_for_z(z)?;
        let d = self.direction;
        Some((t, Ge3DPoint { x: d.x * t + self.start.x, y: d.y * t + self.start.y, z }))
    }
}

impl Ge3DPlane {
    /// `Define(p1, p2, p3)`: the first point and the unit normal (p2 - p3) x (p1 - p2).
    pub fn through(p1: Ge3DPoint, p2: Ge3DPoint, p3: Ge3DPoint) -> Ge3DPlane {
        let first = GeVector::new(p1.x - p2.x, p1.y - p2.y, p1.z - p2.z);
        let second = GeVector::new(p2.x - p3.x, p2.y - p3.y, p2.z - p3.z);
        let mut normal = second.cross(first);
        normal.normalize();
        Ge3DPlane { point: p1, normal }
    }

    /// `Intersects(plane, line)`: the line where the planes meet, `None` when they are parallel (the squared length
    /// of the normals' cross product at most 1e-10). The C++ reads an uninitialized parameter when its helper line
    /// misses the other plane (only for degenerate normals); that parameter is 0 here.
    pub fn intersects(&self, other: &Ge3DPlane) -> Option<Ge3DLine> {
        let normal = self.normal;
        let direction = normal.cross(other.normal);
        // the C++'s constant: the folded float square of 0.00001f (9.9999994e-11f)
        if direction.magnitude_squared() > 0.00001f32 * 0.00001f32 {
            let perpendicular = normal.cross(direction);
            let line = Ge3DLine { start: self.point, direction: perpendicular };
            let t = line.intersects_plane(other).unwrap_or(0.0);
            Some(Ge3DLine { start: line.point_at(t), direction })
        } else {
            None
        }
    }

    #[inline]
    fn side(&self, point: Ge3DPoint) -> f32 {
        let delta = GeVector::new(point.x - self.point.x, point.y - self.point.y, point.z - self.point.z);
        delta.dot(self.normal)
    }

    /// `IsPointAbovePlane`: strictly on the normal's side.
    pub fn is_point_above(&self, point: Ge3DPoint) -> bool {
        self.side(point) > 0.0
    }

    /// `IsPointBelowPlane`.
    pub fn is_point_below(&self, point: Ge3DPoint) -> bool {
        self.side(point) < 0.0
    }

    /// `IsPointOnOrAbovePlane`.
    pub fn is_point_on_or_above(&self, point: Ge3DPoint) -> bool {
        self.side(point) >= 0.0
    }

    /// `IsPointOnOrBelowPlane`.
    pub fn is_point_on_or_below(&self, point: Ge3DPoint) -> bool {
        self.side(point) <= 0.0
    }

    /// `IsPointOnPlane`: the normalized direction from the plane's point within 1e-6 of perpendicular to the normal.
    pub fn is_point_on(&self, point: Ge3DPoint) -> bool {
        let mut delta = GeVector::new(point.x - self.point.x, point.y - self.point.y, point.z - self.point.z);
        delta.normalize();
        let dot = delta.dot(self.normal);
        dot > -0.000001 && dot < 0.000001
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn line(s: (f32, f32, f32), d: (f32, f32, f32)) -> Ge3DLine {
        Ge3DLine { start: Ge3DPoint::new(s.0, s.1, s.2), direction: GeVector::new(d.0, d.1, d.2) }
    }

    #[test]
    fn point_hit_takes_the_nearer_root() {
        let l = line((0.0, 0.0, 0.0), (1.0, 0.0, 0.0));
        // a sphere of radius 1 around (5, 0, 0): crossings at t = 4 and 6
        assert_eq!(l.intersects_point(Ge3DPoint::new(5.0, 0.0, 0.0), 1.0), Some(4.0));
        assert_eq!(l.intersects_point(Ge3DPoint::new(5.0, 3.0, 0.0), 1.0), None);
        // tangent: one root
        assert_eq!(l.intersects_point(Ge3DPoint::new(5.0, 1.0, 0.0), 1.0), Some(5.0));
    }

    #[test]
    fn crossing_lines_and_parallel_lines() {
        let a = line((0.0, 0.0, 0.0), (1.0, 0.0, 0.0));
        let b = line((2.0, -1.0, 0.0), (0.0, 1.0, 0.0));
        assert_eq!(a.intersects_line(&b), LineHit { hit: true, params: Some((2.0, 1.0)) });
        let c = line((2.0, -1.0, 1.0), (0.0, 1.0, 0.0));
        assert!(!a.intersects_line(&c).hit);
        assert!(a.intersects_line(&c).params.is_some());
        assert_eq!(a.intersects_line(&a), LineHit { hit: false, params: None });
        assert_eq!(a.intersects_in_2d(&b), Some((2.0, 1.0)));
        assert_eq!(a.intersects_in_2d(&a), None);
    }

    #[test]
    fn plane_crossing_and_solvers() {
        let l = line((0.0, 0.0, 10.0), (0.0, 0.0, -2.0));
        let ground = Ge3DPlane { point: Ge3DPoint::default(), normal: GeVector::new(0.0, 0.0, 1.0) };
        assert_eq!(l.intersects_plane(&ground), Some(5.0));
        assert_eq!(l.solve_for_z(4.0), Some(3.0));
        assert_eq!(l.solve_for_x(4.0), None);
        assert_eq!(l.solve_for_z_point(4.0), Some((3.0, Ge3DPoint::new(0.0, 0.0, 4.0))));
        assert_eq!(
            line((1.0, 2.0, 3.0), (1.0, 1.0, 1.0)).solve_for_y_point(4.0),
            Some((2.0, Ge3DPoint::new(3.0, 4.0, 5.0)))
        );
    }

    #[test]
    fn plane_through_three_points_and_sides() {
        let p = Ge3DPlane::through(
            Ge3DPoint::new(0.0, 0.0, 0.0),
            Ge3DPoint::new(1.0, 0.0, 0.0),
            Ge3DPoint::new(0.0, 1.0, 0.0),
        );
        // (p2 - p3) x (p1 - p2): counter-clockwise points give a normal facing -z
        assert_eq!(p.normal, GeVector::new(0.0, 0.0, -1.0));
        assert!(p.is_point_below(Ge3DPoint::new(3.0, 3.0, 1.0)));
        assert!(p.is_point_above(Ge3DPoint::new(3.0, 3.0, -1.0)));
        assert!(p.is_point_on_or_above(Ge3DPoint::new(3.0, 3.0, 0.0)));
        assert!(p.is_point_on_or_below(Ge3DPoint::new(3.0, 3.0, 0.0)));
        assert!(p.is_point_on(Ge3DPoint::new(3.0, 3.0, 0.0)));
        assert!(!p.is_point_on(Ge3DPoint::new(3.0, 3.0, 0.1)));
        // collinear points: a zero normal
        let flat =
            Ge3DPlane::through(Ge3DPoint::default(), Ge3DPoint::new(1.0, 0.0, 0.0), Ge3DPoint::new(2.0, 0.0, 0.0));
        assert_eq!(flat.normal, GeVector::default());
    }

    #[test]
    fn two_planes_meet_in_a_line() {
        let ground = Ge3DPlane { point: Ge3DPoint::default(), normal: GeVector::new(0.0, 0.0, 1.0) };
        let wall = Ge3DPlane { point: Ge3DPoint::new(2.0, 0.0, 0.0), normal: GeVector::new(1.0, 0.0, 0.0) };
        let l = ground.intersects(&wall).unwrap();
        assert_eq!(l.direction, GeVector::new(0.0, 1.0, 0.0));
        assert_eq!(l.start, Ge3DPoint::new(2.0, 0.0, 0.0));
        assert_eq!(ground.intersects(&ground), None);
    }
}
