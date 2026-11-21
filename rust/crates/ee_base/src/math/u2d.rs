//! The `U2DPoint`/`U2DRectangle` template members that ran as algorithms (`include/lle/U2DPoint.h`,
//! `U2DRectangle.h`, the exe's header view): the tiles a line crosses (`U2DPoint<long>::GetLineTiles`) and a
//! rectangle's closest point (`U2DRectangle<float>::GetClosestPoint`, with `GetClosestCorner`,
//! `GetClosestPointOnSegment` and `ProjectOnSegment`), in the C++'s float operations and order. The C++ templates
//! forward their one instantiation each here.

use std::ffi::{c_long, c_void};

use crate::fs::cppfile::{ee_fs_read_long, CppFile};

use crate::math::types::{Ge3DLine, Ge3DPoint, GeVector};

/// `U2DPoint<long>::GetLineTiles(inTo, inStep, outTiles)`: from this tile towards `to` in steps of `step` tiles,
/// each tile the line enters once (truncated coordinates), then `to` itself.
pub fn line_tiles(from: [c_long; 2], to: [c_long; 2], step: f32) -> Vec<[c_long; 2]> {
    let mut tiles = Vec::new();
    if from != to {
        let start = Ge3DPoint { x: from[0] as f32, y: from[1] as f32, z: 0.0 };
        let end = Ge3DPoint { x: to[0] as f32, y: to[1] as f32, z: 0.0 };
        let mut direction = GeVector { x: to[0] as f32 - from[0] as f32, y: to[1] as f32 - from[1] as f32, z: 0.0 };
        direction.normalize();
        direction.x *= step;
        direction.y *= step;
        let line = Ge3DLine { start, direction };
        // (the C++ left the length unset when neither axis solves: a zero direction, which integer ends never give)
        let length = line.solve_for_x(end.x).or_else(|| line.solve_for_y(end.y)).unwrap_or(0.0);
        let mut distance = 0.0f32;
        let mut last: [c_long; 2] = [0, 0];
        while distance < length {
            let point = line.point_at(distance);
            let tile = [point.x as c_long, point.y as c_long];
            if last != tile || tiles.is_empty() {
                tiles.push(tile);
            }
            distance += step;
            last = tile;
        }
    }
    tiles.push(to);
    tiles
}

/// A `U2DPoint<float>`.
type P = [f32; 2];

/// `DistanceToSquaredF`/`DistanceToSquared` of `U2DPoint<float>`: the x term first.
fn distance_squared(a: P, b: P) -> f32 {
    (a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1])
}

/// `DistanceToF`.
fn distance(a: P, b: P) -> f32 {
    distance_squared(a, b).sqrt()
}

/// A 2D vector of the C++'s `GEVector` (z zero), normalized.
fn unit(x: f32, y: f32) -> GeVector {
    let mut v = GeVector { x, y, z: 0.0 };
    v.normalize();
    v
}

/// `ProjectOnSegment(inDot, inStart, inEnd)`: the point `dot` times this point's distance from `start` along
/// `start..end` (the offset vector is normalized, unused, as the C++).
fn project_on_segment(this: P, dot: f32, start: P, end: P) -> P {
    if start == end {
        return this;
    }
    let edge = unit(end[0] - start[0], end[1] - start[1]);
    let length = distance(start, this) * dot;
    [edge.x * length + start[0], edge.y * length + start[1]]
}

/// `GetClosestPointOnSegment(inStart, inEnd)`: the projection when the angle at `start` is not obtuse, else the
/// nearer end.
fn closest_point_on_segment(this: P, start: P, end: P) -> P {
    if this == start || this == end {
        return this;
    }
    if start == end {
        return start;
    }
    let offset = unit(this[0] - start[0], this[1] - start[1]);
    let edge = unit(end[0] - start[0], end[1] - start[1]);
    let dot = offset.dot(edge);
    if dot >= 0.0 {
        project_on_segment(this, dot, start, end)
    } else if distance_squared(this, start) < distance_squared(this, end) {
        start
    } else {
        end
    }
}

/// `U2DRectangle<float>` as `[left, top, right, bottom]`.
type R = [f32; 4];

/// `GetClosestCorner(inPoint, outFirst, outSecond)`: the corner nearest `point` and its two neighbours.
fn closest_corner(r: R, point: P) -> (P, P, P) {
    let (top_left, top_right, bottom_left, bottom_right) = ([r[0], r[1]], [r[2], r[1]], [r[0], r[3]], [r[2], r[3]]);
    let mut closest = top_left;
    let mut best = distance_squared(point, top_left);
    let (mut first, mut second) = (bottom_left, top_right);
    let d = distance_squared(point, bottom_left);
    if d < best {
        best = d;
        closest = bottom_left;
        (first, second) = (bottom_right, top_left);
    }
    let d = distance_squared(point, top_right);
    if d < best {
        closest = top_right;
        best = d;
        (first, second) = (top_left, bottom_right);
    }
    if distance_squared(point, bottom_right) < best {
        closest = bottom_right;
        (first, second) = (bottom_left, top_right);
    }
    (closest, first, second)
}

/// `U2DRectangle<float>::GetClosestPoint(inPoint)`: the nearer of the closest points on the two edges at the
/// corner nearest `point`.
pub fn rect_closest_point(r: R, point: P) -> P {
    let (corner, first, second) = closest_corner(r, point);
    let result = closest_point_on_segment(point, corner, first);
    let d = distance_squared(result, point);
    let other = closest_point_on_segment(point, corner, second);
    if d < distance_squared(other, point) {
        result
    } else {
        other
    }
}

/// `GetLineTiles`' C side: `assign(out, tiles, count)` replaces the C++ vector's contents.
pub type EeU2DAssignTiles = unsafe extern "C" fn(out: *mut c_void, tiles: *const [c_long; 2], count: usize);

/// `U2DPoint<long>::GetLineTiles(inTo, inStep, outTiles)`.
///
/// # Safety
/// `from` and `to` point at two `long`s; `assign` takes `out`.
#[no_mangle]
pub unsafe extern "C" fn ee_u2d_line_tiles(
    from: *const [c_long; 2],
    to: *const [c_long; 2],
    step: f32,
    out: *mut c_void,
    assign: EeU2DAssignTiles,
) {
    // SAFETY: the caller's contract
    unsafe {
        let tiles = line_tiles(*from, *to, step);
        assign(out, tiles.as_ptr(), tiles.len());
    }
}

/// `U2DRectangle<float>::GetClosestPoint(inPoint)` (the rectangle's members in their order: bottom, left, right,
/// top).
///
/// # Safety
/// `rect` points at four floats, `point` and `out` at two.
#[no_mangle]
pub unsafe extern "C" fn ee_u2d_rect_closest_point(rect: *const [f32; 4], point: *const [f32; 2], out: *mut [f32; 2]) {
    // SAFETY: the caller's contract
    unsafe {
        let [bottom, left, right, top] = *rect;
        *out = rect_closest_point([left, top, right, bottom], *point);
    }
}

/// The C++ `kErrorNotOpen`.
const ERROR_NOT_OPEN: c_long = 4;

/// The rectangle's members in their order (bottom, left, right, top), as `U2DRectangle<long>` or `<float>` holds
/// them.
#[derive(Clone, Copy)]
enum RectFields {
    Long(*mut [c_long; 4]),
    Float(*mut [f32; 4]),
}

/// `U2DRectangle<T>::Load` / `Save`: left, top, right, bottom through the file's typed calls (`Read(long&)`'s
/// 32-bit copy, or `Read(float&)`), the first error ending it; `kErrorNotOpen` when the file is not open.
fn rect_io(file: &mut CppFile, open: bool, fields: RectFields, write: bool) -> c_long {
    if !open {
        return ERROR_NOT_OPEN;
    }
    for i in [1, 3, 2, 0] {
        // SAFETY: the rectangle is valid for the call (the FFI's contract)
        let status = unsafe {
            match (fields, write) {
                (RectFields::Long(r), false) => ee_fs_read_long(file.0, &mut (*r)[i]),
                (RectFields::Long(r), true) => file.write_i32((*r)[i] as i32) as c_long,
                (RectFields::Float(r), false) => file.read_f32(&mut (*r)[i]) as c_long,
                (RectFields::Float(r), true) => file.write_f32((*r)[i]) as c_long,
            }
        };
        if status != 0 {
            return status;
        }
    }
    0
}

/// `U2DRectangle<T>::Load` (`write` false) and `Save` over a C++ `FSFile*` whose `GetIsOpen()` is `open`; `rect`
/// is the rectangle (four `long`s, or with `float` four floats).
///
/// # Safety
/// `file` is a live C++ `FSFile*`; `rect` points at the four members.
#[no_mangle]
pub unsafe extern "C" fn ee_u2d_rect_io(
    file: *mut c_void,
    open: bool,
    rect: *mut c_void,
    float: bool,
    write: bool,
) -> c_long {
    let fields = if float { RectFields::Float(rect.cast()) } else { RectFields::Long(rect.cast()) };
    rect_io(&mut CppFile(file), open, fields, write)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_line_of_tiles_ends_at_its_target() {
        assert_eq!(line_tiles([3, 4], [3, 4], 1.0), vec![[3, 4]]);
        assert_eq!(line_tiles([0, 0], [3, 0], 1.0), vec![[0, 0], [1, 0], [2, 0], [3, 0]]);
        assert_eq!(line_tiles([0, 0], [0, -2], 1.0), vec![[0, 0], [0, -1], [0, -2]]);
        // a diagonal: steps of one tile along the line, a tile entered once
        assert_eq!(line_tiles([0, 0], [2, 2], 1.0), vec![[0, 0], [1, 1], [2, 2]]);
        assert_eq!(line_tiles([0, 0], [4, 1], 0.5).first(), Some(&[0, 0]));
        assert_eq!(line_tiles([0, 0], [4, 1], 0.5).last(), Some(&[4, 1]));
        // a step past the length: the start, then the target
        assert_eq!(line_tiles([0, 0], [1, 0], 5.0), vec![[0, 0], [1, 0]]);
    }

    #[test]
    fn the_closest_point_of_a_rectangle() {
        let near = |a: P, b: P| (a[0] - b[0]).abs() < 1e-5 && (a[1] - b[1]).abs() < 1e-5;
        let r = [0.0, 0.0, 10.0, 4.0];
        // outside, beside an edge: the projection onto it (through the normalized vectors: within float rounding)
        assert!(near(rect_closest_point(r, [5.0, -3.0]), [5.0, 0.0]));
        assert!(near(rect_closest_point(r, [12.0, 2.0]), [10.0, 2.0]));
        // beyond a corner: the corner
        assert_eq!(rect_closest_point(r, [-2.0, -2.0]), [0.0, 0.0]);
        assert_eq!(rect_closest_point(r, [11.0, 5.0]), [10.0, 4.0]);
        // on a corner
        assert_eq!(rect_closest_point(r, [10.0, 0.0]), [10.0, 0.0]);
        // a point rectangle
        assert_eq!(rect_closest_point([1.0, 1.0, 1.0, 1.0], [3.0, 3.0]), [1.0, 1.0]);
    }

    #[test]
    fn the_corner_and_its_neighbours() {
        let r = [0.0, 0.0, 10.0, 4.0];
        assert_eq!(closest_corner(r, [-1.0, -1.0]), ([0.0, 0.0], [0.0, 4.0], [10.0, 0.0]));
        assert_eq!(closest_corner(r, [-1.0, 5.0]), ([0.0, 4.0], [10.0, 4.0], [0.0, 0.0]));
        assert_eq!(closest_corner(r, [11.0, -1.0]), ([10.0, 0.0], [0.0, 0.0], [10.0, 4.0]));
        assert_eq!(closest_corner(r, [11.0, 5.0]), ([10.0, 4.0], [0.0, 4.0], [10.0, 0.0]));
    }

    #[test]
    fn a_rectangle_loads_and_saves_left_top_right_bottom() {
        use crate::fs::cppfile::tests::with_file;
        // bottom, left, right, top in memory; left, top, right, bottom in the file
        let mut longs: [c_long; 4] = [4, 1, 3, -2];
        let ((), m) = with_file(&[], |f| {
            // SAFETY: a registered file, a valid rectangle
            assert_eq!(unsafe { ee_u2d_rect_io(f.0, true, longs.as_mut_ptr().cast(), false, true) }, 0);
        });
        assert_eq!(m.log, ["write_i32 1", "write_i32 -2", "write_i32 3", "write_i32 4"]);
        let mut back: [c_long; 4] = [0; 4];
        let ((), m) = with_file(&m.data, |f| {
            // SAFETY: as above
            assert_eq!(unsafe { ee_u2d_rect_io(f.0, true, back.as_mut_ptr().cast(), false, false) }, 0);
        });
        assert_eq!((back, m.log.len()), (longs, 4));
        // a short file: the first error, the fields after it untouched; not open: 4 and nothing read
        let mut floats: [f32; 4] = [9.0; 4];
        let ((), m) = with_file(&1.5f32.to_bits().to_le_bytes(), |f| {
            // SAFETY: as above
            assert_eq!(unsafe { ee_u2d_rect_io(f.0, true, floats.as_mut_ptr().cast(), true, false) }, 0x12);
            assert_eq!(unsafe { ee_u2d_rect_io(f.0, false, floats.as_mut_ptr().cast(), true, false) }, 4);
        });
        assert_eq!(floats, [9.0, 1.5, 9.0, 9.0]);
        assert_eq!(m.log, ["read_f32", "read_f32"]);
    }
}
