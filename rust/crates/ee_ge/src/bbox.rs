//! `GEBoundingBox` (`GEBoundingBox.cpp`) and `GERectangle` (`GERectangle.cpp`): the box's dimensions, containment,
//! overlap, line hit and transformed bounds; the rectangle's clipping and its split around another rectangle.

use crate::line::abs;
use crate::types::{Ge3DLine, Ge3DPlane, Ge3DPoint, GeBoundingBox, GeRectangle, GeTransformation, GeVector};

/// `Contains`' tolerance on every face.
const TOLERANCE: f32 = 0.00001;

impl GeBoundingBox {
    /// `Define(blf, trb)` (inline in the header): the corners, then `ComputeDimensions`.
    pub fn new(bottom_left_front: Ge3DPoint, top_right_back: Ge3DPoint) -> GeBoundingBox {
        let mut b = GeBoundingBox { bottom_left_front, top_right_back, ..GeBoundingBox::default() };
        b.compute_dimensions();
        b
    }

    /// `ComputeDimensions`: each axis ordered so the first corner holds the minimum, then width (x), height (z) and
    /// depth (y).
    pub fn compute_dimensions(&mut self) {
        let (a, b) = (&mut self.bottom_left_front, &mut self.top_right_back);
        if a.x > b.x {
            std::mem::swap(&mut a.x, &mut b.x);
        }
        if a.y > b.y {
            std::mem::swap(&mut a.y, &mut b.y);
        }
        if a.z > b.z {
            std::mem::swap(&mut a.z, &mut b.z);
        }
        self.width = self.top_right_back.x - self.bottom_left_front.x;
        self.height = self.top_right_back.z - self.bottom_left_front.z;
        self.depth = self.top_right_back.y - self.bottom_left_front.y;
    }

    /// `Contains`: inside, with 0.00001 of slack on every face.
    pub fn contains(&self, p: Ge3DPoint) -> bool {
        let (a, b) = (self.bottom_left_front, self.top_right_back);
        !(p.x < a.x - TOLERANCE
            || p.y < a.y - TOLERANCE
            || p.z < a.z - TOLERANCE
            || p.x > b.x + TOLERANCE
            || p.y > b.y + TOLERANCE
            || p.z > b.z + TOLERANCE)
    }

    /// `Intersects(box)`: overlap, touching faces included.
    pub fn intersects(&self, other: &GeBoundingBox) -> bool {
        let (a, b) = (self.bottom_left_front, self.top_right_back);
        let (c, d) = (other.bottom_left_front, other.top_right_back);
        !(a.x > d.x || a.y > d.y || a.z > d.z || b.x < c.x || b.y < c.y || b.z < c.z)
    }

    /// `LineToPlaneIntersection`: whether the line crosses the plane inside the box, and the distance to keep (the
    /// hit, unless `first` is false and `best` is nearer).
    fn line_to_plane(&self, line: &Ge3DLine, plane: &Ge3DPlane, best: f32, first: bool) -> Option<f32> {
        let distance = line.intersects_plane(plane)?;
        if !self.contains(line.point_at(distance)) {
            return None;
        }
        Some(if first || abs(distance) < abs(best) { distance } else { best })
    }

    /// `Intersects(line, distance)`: the six faces in the C++'s order (bottom, top, back, front, right, left); the
    /// distance is the nearest hit, or 0 with no hit (the C++ always writes it).
    pub fn intersects_line(&self, line: &Ge3DLine) -> (bool, f32) {
        let (a, b) = (self.bottom_left_front, self.top_right_back);
        let faces = [
            (a, GeVector::new(0.0, 1.0, 0.0)),
            (b, GeVector::new(0.0, -1.0, 0.0)),
            (b, GeVector::new(0.0, 0.0, -1.0)),
            (a, GeVector::new(0.0, 0.0, 1.0)),
            (a, GeVector::new(1.0, 0.0, 0.0)),
            (b, GeVector::new(-1.0, 0.0, 0.0)),
        ];
        let mut distance = 0.0f32;
        let mut result = false;
        for (index, (point, normal)) in faces.into_iter().enumerate() {
            let plane = Ge3DPlane { point, normal };
            // the first face always counts as the first; the others while nothing was hit yet
            let first = index == 0 || !result;
            if let Some(d) = self.line_to_plane(line, &plane, distance, first) {
                distance = d;
                result = true;
            }
        }
        (result, distance)
    }

    /// `Transform(transformation, out)`: the axis-aligned box around the eight transformed corners (in the C++'s
    /// corner order; `TransformPoint` widens the running bounds).
    pub fn transform(&self, t: &GeTransformation) -> GeBoundingBox {
        let (a, b) = (self.bottom_left_front, self.top_right_back);
        let corners = [
            Ge3DPoint::new(a.x, a.y, a.z),
            Ge3DPoint::new(b.x, a.y, a.z),
            Ge3DPoint::new(a.x, b.y, a.z),
            Ge3DPoint::new(a.x, a.y, b.z),
            Ge3DPoint::new(b.x, b.y, b.z),
            Ge3DPoint::new(a.x, b.y, b.z),
            Ge3DPoint::new(b.x, a.y, b.z),
            Ge3DPoint::new(b.x, b.y, a.z),
        ];
        let mut minimum = Ge3DPoint::default();
        let mut maximum = Ge3DPoint::default();
        for (index, corner) in corners.into_iter().enumerate() {
            let p = t.apply_point(corner);
            if index == 0 {
                minimum = p;
                maximum = p;
                continue;
            }
            if minimum.x > p.x {
                minimum.x = p.x;
            } else if maximum.x < p.x {
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
}

/// Whether the two rectangles overlap, as both `And`s test it.
fn overlaps(a: &GeRectangle, b: &GeRectangle) -> bool {
    !(a.left > b.right || a.right < b.left || a.top > b.bottom || a.bottom < b.top)
}

impl GeRectangle {
    pub const fn new(left: f32, top: f32, right: f32, bottom: f32) -> GeRectangle {
        GeRectangle { bottom, left, right, top }
    }

    /// `And(rectangle)`: clipped to the other rectangle; `false` (unchanged) when they do not overlap.
    pub fn and_in_place(&mut self, other: &GeRectangle) -> bool {
        let result = overlaps(self, other);
        if result {
            if self.left < other.left {
                self.left = other.left;
            }
            if self.right > other.right {
                self.right = other.right;
            }
            if self.top < other.top {
                self.top = other.top;
            }
            if self.bottom > other.bottom {
                self.bottom = other.bottom;
            }
        }
        result
    }

    /// `And(rectangle, out)`: the overlap, `None` when there is none (the C++ leaves the output alone then).
    pub fn and(&self, other: &GeRectangle) -> Option<GeRectangle> {
        if !overlaps(self, other) {
            return None;
        }
        Some(GeRectangle {
            left: if self.left < other.left { other.left } else { self.left },
            right: if self.right > other.right { other.right } else { self.right },
            top: if self.top < other.top { other.top } else { self.top },
            bottom: if self.bottom > other.bottom { other.bottom } else { self.bottom },
        })
    }

    /// `RemoveIntersectingRectangle(rectangle, out)`: the pieces left, right, above and below the other rectangle's
    /// edges that cut this one, pushed in that order; the whole rectangle when no edge cuts it, it is not inside
    /// the other, and the output vector was empty (`existing` is its size before the call: the C++ tests the
    /// vector's size, not the pieces it added).
    pub fn remove_intersecting(&self, other: &GeRectangle, existing: usize, mut push: impl FnMut(GeRectangle)) {
        let mut pieces = Vec::with_capacity(4);
        let mut left_split = false;
        let mut right_split = false;
        if other.left >= self.left && other.left <= self.right {
            pieces.push(GeRectangle::new(self.left, self.top, other.left, self.bottom));
            left_split = true;
        }
        if other.right >= self.left && other.right <= self.right {
            pieces.push(GeRectangle::new(other.right, self.top, self.right, self.bottom));
            right_split = true;
        }
        let left = if left_split { other.left } else { self.left };
        let right = if right_split { other.right } else { self.right };
        if other.top >= self.top && other.top <= self.bottom {
            pieces.push(GeRectangle::new(left, self.top, right, other.top));
        }
        if other.bottom >= self.top && other.bottom <= self.bottom {
            pieces.push(GeRectangle::new(left, other.bottom, right, self.bottom));
        }
        if existing + pieces.len() == 0
            && (self.left < other.left
                || self.right > other.right
                || self.top < other.top
                || self.bottom > other.bottom)
        {
            pieces.push(*self);
        }
        pieces.into_iter().for_each(&mut push);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn p(x: f32, y: f32, z: f32) -> Ge3DPoint {
        Ge3DPoint::new(x, y, z)
    }

    #[test]
    fn dimensions_order_the_corners() {
        let b = GeBoundingBox::new(p(2.0, 5.0, 1.0), p(0.0, 1.0, 4.0));
        assert_eq!(b.bottom_left_front, p(0.0, 1.0, 1.0));
        assert_eq!(b.top_right_back, p(2.0, 5.0, 4.0));
        assert_eq!((b.width, b.height, b.depth), (2.0, 3.0, 4.0));
    }

    #[test]
    fn containment_has_slack_and_overlap_counts_touching() {
        let b = GeBoundingBox::new(p(0.0, 0.0, 0.0), p(1.0, 1.0, 1.0));
        assert!(b.contains(p(1.000001, 0.5, 0.5)));
        assert!(!b.contains(p(1.001, 0.5, 0.5)));
        assert!(b.intersects(&GeBoundingBox::new(p(1.0, 1.0, 1.0), p(2.0, 2.0, 2.0))));
        assert!(!b.intersects(&GeBoundingBox::new(p(1.5, 0.0, 0.0), p(2.0, 1.0, 1.0))));
    }

    #[test]
    fn line_hits_the_nearest_face() {
        let b = GeBoundingBox::new(p(0.0, 0.0, 0.0), p(1.0, 1.0, 1.0));
        let l = Ge3DLine { start: p(-2.0, 0.5, 0.5), direction: GeVector::new(1.0, 0.0, 0.0) };
        assert_eq!(b.intersects_line(&l), (true, 2.0));
        let miss = Ge3DLine { start: p(-2.0, 3.0, 0.5), direction: GeVector::new(1.0, 0.0, 0.0) };
        assert_eq!(b.intersects_line(&miss), (false, 0.0));
    }

    #[test]
    fn transform_bounds_the_corners() {
        let b = GeBoundingBox::new(p(0.0, 0.0, 0.0), p(1.0, 2.0, 3.0));
        let mut t = GeTransformation::IDENTITY;
        t.m[0][3] = 10.0;
        t.m[0][0] = -1.0;
        let o = b.transform(&t);
        assert_eq!(o.bottom_left_front, p(9.0, 0.0, 0.0));
        assert_eq!(o.top_right_back, p(10.0, 2.0, 3.0));
    }

    #[test]
    fn rectangle_clipping() {
        let a = GeRectangle::new(0.0, 0.0, 10.0, 10.0);
        let b = GeRectangle::new(5.0, -5.0, 15.0, 5.0);
        assert_eq!(a.and(&b), Some(GeRectangle::new(5.0, 0.0, 10.0, 5.0)));
        let mut c = a;
        assert!(c.and_in_place(&b));
        assert_eq!(c, GeRectangle::new(5.0, 0.0, 10.0, 5.0));
        let far = GeRectangle::new(20.0, 20.0, 30.0, 30.0);
        assert_eq!(a.and(&far), None);
        let mut d = a;
        assert!(!d.and_in_place(&far));
        assert_eq!(d, a);
    }

    #[test]
    fn rectangle_split_around_another() {
        let a = GeRectangle::new(0.0, 0.0, 10.0, 10.0);
        let mut out = Vec::new();
        a.remove_intersecting(&GeRectangle::new(4.0, 4.0, 6.0, 6.0), 0, |r| out.push(r));
        assert_eq!(
            out,
            vec![
                GeRectangle::new(0.0, 0.0, 4.0, 10.0),
                GeRectangle::new(6.0, 0.0, 10.0, 10.0),
                GeRectangle::new(4.0, 0.0, 6.0, 4.0),
                GeRectangle::new(4.0, 6.0, 6.0, 10.0),
            ]
        );
        // no edge cuts and it is not inside the other: the whole rectangle, only into an empty vector
        out.clear();
        let outside = GeRectangle::new(20.0, 20.0, 30.0, 30.0);
        a.remove_intersecting(&outside, 0, |r| out.push(r));
        assert_eq!(out, vec![a]);
        out.clear();
        a.remove_intersecting(&outside, 1, |r| out.push(r));
        assert!(out.is_empty());
        // covered by the other: nothing
        a.remove_intersecting(&GeRectangle::new(-1.0, -1.0, 11.0, 11.0), 0, |r| out.push(r));
        assert!(out.is_empty());
    }
}
