//! Tile-grid and mask queries, square and line marking (`Unit11.cpp`). Unsigned arithmetic wraps as the C++'s does:
//! a coordinate minus a radius below zero becomes huge and is clamped or rejected by the bound checks.

use crate::map::{is_cell_set, Grid};
use crate::math::{floor_unsigned, Point, Rotation, Vector, HALF_PI};

fn clamped_square(size: u32, x: u32, y: u32, radius: u32) -> (u32, u32, u32, u32) {
    let mut left = x.wrapping_sub(radius);
    let mut right = x.wrapping_add(radius);
    let mut top = y.wrapping_sub(radius);
    let mut bottom = y.wrapping_add(radius);
    if left >= size {
        left = 0;
    }
    if top >= size {
        top = 0;
    }
    if right >= size {
        right = size.wrapping_sub(1);
    }
    if bottom >= size {
        bottom = size.wrapping_sub(1);
    }
    (left, right, top, bottom)
}

/// `IsAnyMaskedTileClear`.
pub fn is_any_masked_tile_clear(mask: &Grid<u8>, tiles: &Grid<u32>, x: u32, y: u32, radius: u32) -> bool {
    let (left, right, top, bottom) = clamped_square(mask.size, x, y, radius);
    let mut result = false;
    let mut i = left;
    while i <= right && !result {
        let mut j = top;
        while j <= bottom && !result {
            result = mask.get(i, j) != 0 && (tiles.get(i, j) & 0x7fff) == 0;
            j = j.wrapping_add(1);
        }
        i = i.wrapping_add(1);
    }
    result
}

/// `AreAllMasked`.
pub fn are_all_masked(mask: &Grid<u8>, other: &Grid<u8>, x: u32, y: u32, radius: u32) -> bool {
    let (left, right, top, bottom) = clamped_square(mask.size, x, y, radius);
    let mut result = true;
    let mut i = left;
    while i <= right && result {
        let mut j = top;
        while j <= bottom && result {
            result = mask.get(i, j) != 0 && other.get(i, j) == 0;
            j = j.wrapping_add(1);
        }
        i = i.wrapping_add(1);
    }
    result
}

/// `IsSquareOpen`: false when all four corners of the square are on the map and in the mask, true otherwise (despite
/// its name, true means the square is not fully usable).
pub fn is_square_open(mask: &Grid<u8>, x: u32, y: u32, radius: u32) -> bool {
    let size = mask.size;
    let left = x.wrapping_sub(radius);
    let right = x.wrapping_add(radius).wrapping_add(1);
    let top = y.wrapping_sub(radius);
    let bottom = y.wrapping_add(radius).wrapping_add(1);
    !(left < size
        && right < size
        && top < size
        && bottom < size
        && mask.get(left, top) != 0
        && mask.get(left, bottom) != 0
        && mask.get(right, top) != 0
        && mask.get(right, bottom) != 0)
}

/// `Unit11_1581c`: whether a land tile borders an empty one (8-neighbourhood, within the map).
pub fn is_shore_tile(tiles: &Grid<u32>, x: u32, y: u32) -> bool {
    let size = tiles.size;
    let mut result = false;
    if x < size && y < size && (tiles.get(x, y) & 0x7fff) != 0 {
        let last = (x as i32).wrapping_add(1);
        let mut i = (x as i32).wrapping_sub(1);
        while !result && i <= last {
            let mut j = (y as i32).wrapping_sub(1);
            while !result && j <= (y as i32).wrapping_add(1) {
                if (i as u32) < size && (j as u32) < size {
                    result = (tiles.get(i as u32, j as u32) & 0x7fff) == 0;
                }
                j = j.wrapping_add(1);
            }
            i = i.wrapping_add(1);
        }
    }
    result
}

/// `IsTileOfType`.
pub fn is_tile_of_type(tiles: &Grid<u32>, x: u32, y: u32, tile_type: u32, special: bool) -> bool {
    let wanted = if special { tile_type.wrapping_add(0x400) } else { tile_type.wrapping_add(1) };
    (tiles.get(x, y) & 0x7fff) == wanted
}

/// `MarkSquare`.
pub fn mark_square(mask: &mut Grid<u8>, x: u32, y: u32, size_of_square: u32) {
    let size = mask.size;
    for i in 0..size_of_square {
        for j in 0..size_of_square {
            if x.wrapping_add(i) < size && y.wrapping_add(j) < size {
                mask.set(x.wrapping_add(i), y.wrapping_add(j), 1);
            }
        }
    }
}

/// `MarkLine`.
pub fn mark_line(mask: &mut Grid<u8>, from: Point, to: Point, width: f32) {
    let width = width * 0.5;
    let size = mask.size;
    let rotation = Rotation::yaw(HALF_PI);
    let direction = Vector { x: to.x - from.x, y: to.y - from.y, z: 0.0 };
    let mut side = rotation.apply(direction);
    side.normalize();
    side.x *= width;
    side.y *= width;
    let mut line = Vec::new();
    let mut cross = Vec::new();
    from.interpolate_to(to, 1.0, &mut line);
    for point in &line {
        let left = Point::new(point.x + side.x, point.y + side.y);
        let right = Point::new(point.x - side.x, point.y - side.y);
        left.interpolate_to(right, 1.0, &mut cross);
        for c in &cross {
            let cx = floor_unsigned(c.x);
            let cy = floor_unsigned(c.y);
            if cx < size && cy < size {
                mask.set(cx, cy, 1);
            }
        }
    }
}

/// Whether a tile lies on the valid map (`CMUIsCellSet` on the valid-tile grid).
pub fn is_valid_cell(valid: &Grid<u8>, x: u32, y: u32) -> bool {
    is_cell_set(valid, x, y)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn full(size: u32) -> Grid<u8> {
        Grid::new(size, 1)
    }

    #[test]
    fn square_open_is_false_only_when_every_corner_is_usable() {
        let mask = full(10);
        assert!(!is_square_open(&mask, 5, 5, 1));
        // a corner off the map: wrapping makes left huge
        assert!(is_square_open(&mask, 0, 5, 1));
        let mut hole = full(10);
        hole.set(4, 4, 0);
        assert!(is_square_open(&hole, 5, 5, 1));
    }

    #[test]
    fn any_masked_clear_looks_at_the_clamped_square() {
        let mask = full(4);
        let mut tiles = Grid::new(4, 1u32);
        assert!(!is_any_masked_tile_clear(&mask, &tiles, 1, 1, 1));
        tiles.set(0, 0, 0x8000); // only the low 15 bits count
        assert!(is_any_masked_tile_clear(&mask, &tiles, 1, 1, 1));
    }

    #[test]
    fn all_masked_needs_mask_and_not_other() {
        let mask = full(4);
        let mut other = Grid::new(4, 0u8);
        assert!(are_all_masked(&mask, &other, 1, 1, 1));
        other.set(2, 2, 1);
        assert!(!are_all_masked(&mask, &other, 1, 1, 1));
    }

    #[test]
    fn shore_tile_needs_land_with_an_empty_neighbour() {
        let mut tiles = Grid::new(3, 1u32);
        assert!(!is_shore_tile(&tiles, 1, 1));
        tiles.set(0, 0, 0);
        assert!(is_shore_tile(&tiles, 1, 1));
        tiles.set(1, 1, 0);
        assert!(!is_shore_tile(&tiles, 1, 1));
    }

    #[test]
    fn tile_type_is_owner_plus_one_or_neutral_offset() {
        let mut tiles = Grid::new(1, 3u32);
        assert!(is_tile_of_type(&tiles, 0, 0, 2, false));
        tiles.set(0, 0, 0x402);
        assert!(is_tile_of_type(&tiles, 0, 0, 2, true));
    }

    #[test]
    fn mark_square_clips_to_the_map() {
        let mut mask = Grid::new(3, 0u8);
        mark_square(&mut mask, 2, 2, 4);
        assert_eq!(mask.data.iter().filter(|&&v| v == 1).count(), 1);
        assert_eq!(mask.get(2, 2), 1);
    }

    #[test]
    fn mark_line_sets_tiles_along_the_line() {
        let mut mask = Grid::new(8, 0u8);
        mark_line(&mut mask, Point::new(1.5, 1.5), Point::new(5.5, 1.5), 0.75);
        assert_eq!(mask.get(3, 1), 1);
        assert_eq!(mask.get(3, 5), 0);
    }
}
