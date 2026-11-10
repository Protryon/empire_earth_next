//! The continent manager's tile values (`EEAIContinentManager::UpdateTileValues`, 0x4231d7), line by line: with
//! `water`, every water and space tile gets 10 per shore or ice-shore neighbour (negated, minus 10, on a shore tile);
//! else every land tile gets the wood weights of its 3x3 block (`GetResourceWeight`: 10 per wood unit on a tile,
//! which the C++ reads into a grid). The C++ stores a non-zero value into its grid and, clamped to +-120, into the
//! map tile's AI value.
//!
//! Part of the deterministic game (RUST_PLAN 2.1): integer arithmetic only, the C++'s sums in order.

use crate::continent_grid::{contains_point, EePoint, NO_TILE};
use crate::goals::{TERRAIN_DEEP_WATER, TERRAIN_SPACE};
use crate::modifiers::TERRAIN_SHALLOW_WATER;

/// The map tile's AI value of a tile value: clamped to +-120, as a byte.
pub fn ai_value(value: i32) -> i8 {
    value.clamp(-0x78, 0x78) as i8
}

/// What the pass reads: the map's size, every tile's terrain (`NO_TILE` where the map has none) and, for the land
/// pass, every tile's wood weight (both by `x * size + y`), which pass, and the manager's shore lists (sorted by
/// `U2DPoint<long>::operator<`).
pub struct TileWorld<'a> {
    pub size: i32,
    pub terrain: &'a [i32],
    pub water: bool,
    pub shore: &'a [EePoint],
    pub ice_shore: &'a [EePoint],
    pub weights: &'a [i32],
}

/// A tile's new value, 0 for none.
fn tile_value(w: &TileWorld, x: i32, y: i32) -> i32 {
    let TileWorld { size, terrain, water, shore, ice_shore, weights } = *w;
    let kind = terrain[(x * size + y) as usize];
    if kind == NO_TILE {
        return 0;
    }
    let is_water = kind == TERRAIN_DEEP_WATER || kind == TERRAIN_SHALLOW_WATER || kind == TERRAIN_SPACE;
    if water {
        if !is_water {
            return 0;
        }
        let shore_at = |dx: i32, dy: i32| if contains_point(shore, EePoint { x: x + dx, y: y + dy }) { 10 } else { 0 };
        let mut value = shore_at(0, -1)
            + shore_at(0, 1)
            + shore_at(1, -1)
            + shore_at(1, 0)
            + shore_at(1, 1)
            + shore_at(-1, -1)
            + shore_at(-1, 0)
            + shore_at(-1, 1);
        if !ice_shore.is_empty() {
            let ice_at =
                |dx: i32, dy: i32| if contains_point(ice_shore, EePoint { x: x + dx, y: y + dy }) { 10 } else { 0 };
            value += ice_at(0, -1)
                + ice_at(0, 1)
                + ice_at(1, -1)
                + ice_at(1, 0)
                + ice_at(1, 1)
                + ice_at(-1, -1)
                + ice_at(-1, 0)
                + ice_at(-1, 1);
        }
        if contains_point(shore, EePoint { x, y }) {
            value = -10 - value;
        }
        value
    } else {
        if is_water {
            return 0;
        }
        // `GetResourceWeight`: 0 off the map
        let weight = |dx: i32, dy: i32| {
            let (ox, oy) = (x + dx, y + dy);
            if (ox as u32) < size as u32 && (oy as u32) < size as u32 {
                weights[(ox * size + oy) as usize]
            } else {
                0
            }
        };
        weight(0, 0)
            + weight(0, 1)
            + weight(0, -1)
            + weight(1, -1)
            + weight(1, 0)
            + weight(1, 1)
            + weight(-1, -1)
            + weight(-1, 0)
            + weight(-1, 1)
    }
}

/// 0x4231d7 `UpdateTileValues`: `values[x * size + y]` gets every tile's new value (0: unchanged; a tile the map
/// has none of gets nothing) and `bytes` the AI value byte to store with it.
pub fn update_tile_values(w: &TileWorld, values: &mut [i32], bytes: &mut [i8]) {
    let size = w.size;
    let n = (size * size) as usize;
    assert!(w.terrain.len() >= n && values.len() >= n && bytes.len() >= n && (w.water || w.weights.len() >= n));
    for x in (0..size).rev() {
        for y in (0..size).rev() {
            let value = tile_value(w, x, y);
            values[(x * size + y) as usize] = value;
            bytes[(x * size + y) as usize] = ai_value(value);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::continent_grid::sort_unique;

    fn p(x: i32, y: i32) -> EePoint {
        EePoint { x, y }
    }

    fn run(
        size: i32,
        terrain: &[i32],
        water: bool,
        shore: &[EePoint],
        ice_shore: &[EePoint],
        weights: &[i32],
    ) -> (Vec<i32>, Vec<i8>) {
        let mut values = vec![0; (size * size) as usize];
        let mut bytes = vec![0i8; (size * size) as usize];
        let w = TileWorld { size, terrain, water, shore, ice_shore, weights };
        update_tile_values(&w, &mut values, &mut bytes);
        (values, bytes)
    }

    #[test]
    fn water_tiles_count_their_shore_neighbours_and_a_shore_tile_is_negative() {
        // a 3 x 3 water map; the shore tiles are (1, 1) and (0, 0)
        let size = 3;
        let terrain = vec![TERRAIN_DEEP_WATER; 9];
        let mut shore = vec![p(1, 1), p(0, 0)];
        sort_unique(&mut shore);
        let (values, bytes) = run(size, &terrain, true, &shore, &[], &[]);
        // (2, 2): one shore neighbour (1, 1)
        assert_eq!(values[2 * 3 + 2], 10);
        // (1, 0): two shore neighbours
        assert_eq!(values[3], 20);
        // (1, 1) is a shore tile with (0, 0) beside it: -10 - 10
        assert_eq!(values[4], -20);
        assert_eq!(bytes[4], -20);
        // (0, 0): a shore tile with (1, 1) beside it
        assert_eq!(values[0], -20);
        // ice shore tiles add 10 as well, and the map edge is no shore
        let ice = vec![p(2, 0)];
        let (values, _) = run(size, &terrain, true, &shore, &ice, &[]);
        assert_eq!(values[3], 30);
        assert_eq!(values[2 * 3 + 1], 20);
        // a land tile gets nothing in the water pass
        let mut terrain = terrain;
        terrain[2 * 3 + 2] = 0;
        let (values, bytes) = run(size, &terrain, true, &shore, &ice, &[]);
        assert_eq!(values[2 * 3 + 2], 0);
        assert_eq!(bytes[2 * 3 + 2], 0);
    }

    #[test]
    fn land_tiles_sum_the_wood_weights_around_them_clamped_in_the_byte() {
        let size = 3;
        let mut terrain = vec![0; 9];
        terrain[4] = TERRAIN_SHALLOW_WATER;
        let mut weights = vec![0; 9];
        weights[0] = 10;
        weights[4] = 30;
        weights[2 * 3 + 2] = 200;
        let (values, bytes) = run(size, &terrain, false, &[], &[], &weights);
        // (0, 0): itself and the shallows (1, 1), which still count as a neighbour's weight
        assert_eq!(values[0], 40);
        assert_eq!(bytes[0], 40);
        // the shallows tile itself gets no value in the land pass
        assert_eq!(values[4], 0);
        // (2, 1): (1, 1) and (2, 2)
        assert_eq!(values[2 * 3 + 1], 230);
        assert_eq!(bytes[2 * 3 + 1], 120);
        assert_eq!(ai_value(-200), -120);
        assert_eq!(ai_value(0), 0);
        // off the map counts nothing: (0, 2) sees (1, 1) only
        assert_eq!(values[2], 30);
        // a missing tile gets no value, in either pass, but its weight (0, as `GetResourceWeight` gives) is read
        terrain[2] = NO_TILE;
        let (values, _) = run(size, &terrain, false, &[], &[], &weights);
        assert_eq!(values[2], 0);
        let (values, _) = run(size, &terrain, true, &[], &[], &[]);
        assert_eq!(values[2], 0);
    }
}
