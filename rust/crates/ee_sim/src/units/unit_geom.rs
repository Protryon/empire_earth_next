//! The units' positions and distances over the world views (the units behaviour lane, round 1): the inline and
//! virtual C++ members every host answered for its own bodies (`EEUnit::GetPosition`, `EEComplexUnit::GetPosition`,
//! `DistanceSquaredToUnit`, `DistanceSquaredTo`, `DistanceSquared2D`), one Rust copy each. The float arithmetic is
//! the members' in their order (RUST_PLAN 2.1: no FMA, the long tile coordinates converted to float first).

use crate::world::UnitView;

/// `EEUnit::GetPosition` of a simple unit: its tile's centre at the tile's height.
pub fn tile_position(tile_x: i64, tile_y: i64, height: f32) -> [f32; 3] {
    [tile_x as i32 as f32 + 0.5, tile_y as i32 as f32 + 0.5, height]
}

/// The squared map distance from `from` (x, y) to `to` (x, y), the y difference first (`DistanceSquaredTo`,
/// `DistanceSquaredToUnit`).
pub fn distance_squared_y_first(from: [f32; 2], to: [f32; 2]) -> f32 {
    let dy = from[1] - to[1];
    let dx = from[0] - to[0];
    dx * dx + dy * dy
}

/// `DistanceSquared2D(point)`: x first, each difference computed twice as the inline member did.
pub fn distance_squared_2d(from: [f32; 2], to: [f32; 2]) -> f32 {
    (from[0] - to[0]) * (from[0] - to[0]) + (from[1] - to[1]) * (from[1] - to[1])
}

/// `inUnit->GetPosition(outPoint)` (the virtual): a complex unit's location, a simple unit's tile centre.
pub fn position(u: &UnitView) -> [f32; 3] {
    if u.is_complex() {
        return u.location();
    }
    let (x, y) = u.tile_xy();
    tile_position(x, y, u.tile().map_or(0.0, |t| t.height()))
}

/// `complex->DistanceSquaredToUnit(other)`: to the other's complex unit's location, else to its tile's corner.
pub fn distance_squared_to_unit(complex: &UnitView, other: &UnitView) -> f32 {
    let l = complex.location();
    let to = match other.complex_unit() {
        Some(o) => {
            let m = o.location();
            [m[0], m[1]]
        }
        None => {
            let (x, y) = other.tile_xy();
            [x as i32 as f32, y as i32 as f32]
        }
    };
    distance_squared_y_first([l[0], l[1]], to)
}

/// `complex->DistanceSquaredTo(other)` (two complex units' locations).
pub fn distance_squared_to(complex: &UnitView, other: &UnitView) -> f32 {
    let (a, b) = (complex.location(), other.location());
    distance_squared_y_first([a[0], a[1]], [b[0], b[1]])
}

/// The view of a unit handle the C++ handed in, for `f` (None for 0).
pub fn with_unit<R>(unit: u64, f: impl FnOnce(&UnitView) -> R) -> Option<R> {
    // SAFETY: the hosts' bodies run inside the C++ call that holds the world; its handles are live units
    let token = unsafe { crate::world::WorldRead::begin() };
    // SAFETY: as above
    unsafe { UnitView::of(&token, unit) }.map(|u| f(&u))
}

/// [`position`] of a unit handle.
pub fn position_of(unit: u64) -> [f32; 3] {
    with_unit(unit, position).expect("a live unit")
}

/// [`distance_squared_to_unit`] of two handles.
pub fn distance_squared_to_unit_of(complex: u64, other: u64) -> f32 {
    with_unit(complex, |c| with_unit(other, |o| distance_squared_to_unit(c, o))).flatten().expect("live units")
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_simple_units_position_is_its_tile_centre() {
        assert_eq!(tile_position(3, 7, 1.25), [3.5, 7.5, 1.25]);
        assert_eq!(tile_position(-1, 0, 0.0), [-0.5, 0.5, 0.0]);
    }

    #[test]
    fn the_distances_square_and_sum_the_differences() {
        assert_eq!(distance_squared_y_first([1.0, 2.0], [4.0, 6.0]), 25.0);
        assert_eq!(distance_squared_2d([1.0, 2.0], [4.0, 6.0]), 25.0);
        // the same floats as the C++'s order: no fused multiply-add
        let (a, b) = ([0.1f32, 0.7], [0.3f32, 0.2]);
        let dy = a[1] - b[1];
        let dx = a[0] - b[0];
        assert_eq!(distance_squared_y_first(a, b).to_bits(), (dx * dx + dy * dy).to_bits());
    }
}
