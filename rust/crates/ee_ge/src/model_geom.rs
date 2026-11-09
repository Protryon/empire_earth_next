//! `GEModel`'s geometry (`GEModel.cpp`): the frames' bounding boxes, the centre and radii, the vertex normals, the
//! graphics transform, the scale removal's matrices, the tag point order and the degenerate polygon check. The model
//! and its arrays stay C++ (`GEVertexArray`'s virtuals); the C++ gathers a frame's points and LOD 0's indices into
//! flat arrays, calls these and stores the results. The graphics thread and the UI call them (effects, selection
//! models, the sky): no simulation caller, but the C++'s `f32` order is kept anyway.

use crate::types::{Ge3DPoint, GeBoundingBox, GeTransformation, GeVector};
use crate::um::sqrt;

/// `ComputeBoundingBoxes` for one frame: from the first point, each axis takes a smaller value as its minimum,
/// else a larger one as its maximum; then `Define`.
pub fn frame_box(points: &[Ge3DPoint]) -> GeBoundingBox {
    let first = points.first().copied().unwrap_or_default();
    let (mut min, mut max) = (first, first);
    for p in points {
        if p.x < min.x {
            min.x = p.x;
        } else if p.x > max.x {
            max.x = p.x;
        }
        if p.y < min.y {
            min.y = p.y;
        } else if p.y > max.y {
            max.y = p.y;
        }
        if p.z < min.z {
            min.z = p.z;
        } else if p.z > max.z {
            max.z = p.z;
        }
    }
    GeBoundingBox::new(min, max)
}

/// `ComputeBoundingVolumes`' centre: the mean of every frame's two corners (`1 / (frames * 2)` times the sums).
pub fn center(boxes: &[GeBoundingBox]) -> Ge3DPoint {
    let scale = 1.0f32 / (boxes.len() as u32 as f32 * 2.0);
    let (mut x, mut y, mut z) = (0.0f32, 0.0f32, 0.0f32);
    for b in boxes {
        let (min, max) = (b.bottom_left_front, b.top_right_back);
        x += min.x + max.x;
        y += min.y + max.y;
        z += min.z + max.z;
    }
    Ge3DPoint { x: scale * x, y: scale * y, z: scale * z }
}

/// `ComputeBoundingVolumes`' radius of one frame: the root of the largest squared distance from the centre.
pub fn radius(center: Ge3DPoint, points: &[Ge3DPoint]) -> f32 {
    let mut largest = 0.0f32;
    for &p in points {
        let d = center.distance_to_squared(p);
        if d > largest {
            largest = d;
        }
    }
    sqrt(largest)
}

/// `ComputeModelNormals` for one frame: each LOD 0 triangle's (p3 - p1) x (p2 - p1), unnormalized, summed into its
/// three corners, then every sum normalized. An index past the points reads a zero point and adds nowhere (the
/// C++ read and wrote past its arrays).
pub fn vertex_normals(points: &[Ge3DPoint], indices: &[u32]) -> Vec<GeVector> {
    let mut sums = vec![GeVector::default(); points.len()];
    let at = |i: u32| points.get(i as usize).copied().unwrap_or_default();
    for t in indices.as_chunks::<3>().0 {
        let (p1, p2, p3) = (at(t[0]), at(t[1]), at(t[2]));
        let e1 = GeVector { x: p2.x - p1.x, y: p2.y - p1.y, z: p2.z - p1.z };
        let e2 = GeVector { x: p3.x - p1.x, y: p3.y - p1.y, z: p3.z - p1.z };
        let n = e2.cross(e1);
        for &i in t {
            if let Some(s) = sums.get_mut(i as usize) {
                s.x += n.x;
                s.y += n.y;
                s.z += n.z;
            }
        }
    }
    for s in sums.iter_mut() {
        s.normalize();
    }
    sums
}

/// `PrepareTransforms`' graphics transform: post x (the 3x3 of internal x pre scaled, its translation kept).
pub fn graphics_transform(
    internal: &GeTransformation,
    pre: &GeTransformation,
    post: &GeTransformation,
    scale: f32,
) -> GeTransformation {
    let t = internal.compose(pre);
    let mut scaled = GeTransformation::default();
    for r in 0..3 {
        for c in 0..3 {
            scaled.m[r][c] = t.m[r][c] * scale;
        }
        scaled.m[r][3] = t.m[r][3];
    }
    scaled.m[3] = [0.0, 0.0, 0.0, 1.0];
    post.compose(&scaled)
}

/// `RemoveModelScale`'s matrices for one frame.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
#[repr(C)]
pub struct EeGeScaleRemoval {
    /// The scale baked into the frame's points, normals, tag points, box and (frame 0) centre.
    pub scale: GeTransformation,
    /// The largest of the three scales (the radius's factor).
    pub largest: f32,
    /// The frame's transform within its parent (`parent x internal`), for the sub-models.
    pub transform: GeTransformation,
    /// The new internal transform: the rotation without its scale, the translation re-derived.
    pub unscaled: GeTransformation,
    /// The accumulated scales the sub-models receive.
    pub x: f32,
    pub y: f32,
    pub z: f32,
}

/// `RemoveModelScale` without the arrays: each axis's scale is the length of the internal transform's column times
/// the parent's scale on it.
pub fn scale_removal(
    internal: &GeTransformation,
    parent: &GeTransformation,
    x: f32,
    y: f32,
    z: f32,
) -> EeGeScaleRemoval {
    let transform = parent.compose(internal);
    let column = |c: usize| GeVector { x: internal.m[0][c], y: internal.m[1][c], z: internal.m[2][c] }.magnitude();
    let sx = column(0);
    let x = sx * x;
    let sy = column(1);
    let y = sy * y;
    let sz = column(2);
    let z = sz * z;
    let mut scale = GeTransformation::IDENTITY;
    scale.m[0][0] = x;
    scale.m[1][1] = y;
    scale.m[2][2] = z;
    let mut largest = x;
    if y > largest {
        largest = y;
    }
    if z > largest {
        largest = z;
    }
    let mut unscaled = GeTransformation::IDENTITY;
    for r in 0..3 {
        for c in 0..3 {
            unscaled.m[r][c] = internal.m[r][c];
        }
        unscaled.m[r][3] = 0.0;
    }
    unscaled.m[3] = [0.0, 0.0, 0.0, 1.0];
    unscaled.scale(1.0 / sx, 1.0 / sy, 1.0 / sz);
    let origin = Ge3DPoint::default();
    let base = parent.apply_point(origin);
    let moved = transform.apply_point(origin);
    unscaled.m[0][3] = moved.x - base.x;
    unscaled.m[1][3] = moved.y - base.y;
    unscaled.m[2][3] = moved.z - base.z;
    EeGeScaleRemoval { scale, largest, transform, unscaled, x, y, z }
}

/// `SortTagPoints`' selection sort on the names' first characters (signed, as MSVC's `char`): for each position,
/// the index swapped into it (itself when none is smaller).
pub fn tag_point_order(first: &[i8]) -> Vec<u32> {
    let mut keys = first.to_vec();
    let mut out = Vec::with_capacity(keys.len());
    for i in 0..keys.len() {
        let mut minimum = i;
        for j in i + 1..keys.len() {
            if keys[j] < keys[minimum] {
                minimum = j;
            }
        }
        keys.swap(i, minimum);
        out.push(minimum as u32);
    }
    out
}

/// `Verify` for one frame: false when a material's LOD 0 triangle (its indices offset by the material's first
/// vertex) has two equal corners. `ranges` holds per material the polygon start, polygon count and vertex start.
/// An index past the points reads a zero point.
pub fn frame_is_sound(points: &[Ge3DPoint], indices: &[u32], ranges: &[[u32; 3]]) -> bool {
    let at = |i: u32| points.get(i as usize).copied().unwrap_or_default();
    let index = |k: usize| indices.get(k).copied().unwrap_or(0);
    let mut sound = true;
    for &[start, count, vertex] in ranges {
        let end = start.wrapping_add(count);
        let mut k = start;
        while k < end {
            let p1 = at(index(k as usize * 3).wrapping_add(vertex));
            let p2 = at(index(k as usize * 3 + 1).wrapping_add(vertex));
            let p3 = at(index(k as usize * 3 + 2).wrapping_add(vertex));
            if p1 == p2 || p1 == p3 || p2 == p3 {
                sound = false;
            }
            k += 1;
        }
    }
    sound
}

#[cfg(test)]
mod tests {
    use super::*;

    fn p(x: f32, y: f32, z: f32) -> Ge3DPoint {
        Ge3DPoint::new(x, y, z)
    }

    #[test]
    fn a_box_takes_the_minimum_before_the_maximum() {
        // the else: a point below the first point's x never raises the maximum
        let b = frame_box(&[p(1.0, 1.0, 1.0), p(0.0, 2.0, -1.0), p(3.0, 0.0, 5.0)]);
        assert_eq!(b.bottom_left_front, p(0.0, 0.0, -1.0));
        assert_eq!(b.top_right_back, p(3.0, 2.0, 5.0));
        assert_eq!((b.width, b.height, b.depth), (3.0, 6.0, 2.0));
    }

    #[test]
    fn centre_and_radius() {
        let boxes = [
            GeBoundingBox::new(p(0.0, 0.0, 0.0), p(2.0, 2.0, 2.0)),
            GeBoundingBox::new(p(2.0, 2.0, 2.0), p(4.0, 4.0, 4.0)),
        ];
        let c = center(&boxes);
        assert_eq!(c, p(2.0, 2.0, 2.0));
        assert_eq!(radius(c, &[p(2.0, 2.0, 2.0), p(2.0, 5.0, 6.0)]), 5.0);
        assert_eq!(radius(c, &[]), 0.0);
    }

    #[test]
    fn normals_point_from_the_winding() {
        let n = vertex_normals(&[p(0.0, 0.0, 0.0), p(1.0, 0.0, 0.0), p(0.0, 1.0, 0.0), p(9.0, 9.0, 9.0)], &[0, 1, 2]);
        // (p3 - p1) x (p2 - p1) = y x x = -z; the unused vertex stays zero
        assert_eq!(n[0], GeVector::new(0.0, 0.0, -1.0));
        assert_eq!(n[3], GeVector::default());
        // an index past the end adds nowhere
        let n = vertex_normals(&[p(0.0, 0.0, 0.0), p(1.0, 0.0, 0.0)], &[0, 1, 7]);
        assert_eq!(n.len(), 2);
    }

    #[test]
    fn the_graphics_transform_scales_the_rotation_only() {
        let mut internal = GeTransformation::IDENTITY;
        internal.m[0][3] = 5.0;
        let t = graphics_transform(&internal, &GeTransformation::IDENTITY, &GeTransformation::IDENTITY, 2.0);
        assert_eq!(t.m[0], [2.0, 0.0, 0.0, 5.0]);
        assert_eq!(t.m[3], [0.0, 0.0, 0.0, 1.0]);
    }

    #[test]
    fn scale_removal_splits_scale_from_rotation() {
        let mut internal = GeTransformation::IDENTITY;
        internal.m[0][0] = 2.0;
        internal.m[1][1] = 3.0;
        internal.m[0][3] = 7.0;
        let r = scale_removal(&internal, &GeTransformation::IDENTITY, 1.0, 1.0, 1.0);
        assert_eq!((r.x, r.y, r.z, r.largest), (2.0, 3.0, 1.0, 3.0));
        assert_eq!(r.unscaled.m[0], [1.0, 0.0, 0.0, 7.0]);
        assert_eq!(r.unscaled.m[1][1], 1.0);
        assert_eq!(r.scale.m[1][1], 3.0);
    }

    #[test]
    fn tag_points_sort_by_signed_first_character() {
        // 0xe9 is negative as a char: it sorts first
        assert_eq!(tag_point_order(&[b'b' as i8, b'a' as i8, 0xe9u8 as i8, b'a' as i8]), vec![2, 1, 3, 3]);
        assert_eq!(tag_point_order(&[]), Vec::<u32>::new());
    }

    #[test]
    fn verify_finds_degenerate_triangles() {
        let pts = [p(0.0, 0.0, 0.0), p(1.0, 0.0, 0.0), p(0.0, 1.0, 0.0), p(0.0, 1.0, 0.0)];
        assert!(frame_is_sound(&pts, &[0, 1, 2], &[[0, 1, 0]]));
        assert!(!frame_is_sound(&pts, &[0, 1, 2, 0, 2, 3], &[[0, 2, 0]]));
        // the vertex start offsets the indices
        assert!(!frame_is_sound(&pts, &[0, 1, 2], &[[0, 1, 1]]));
    }
}
