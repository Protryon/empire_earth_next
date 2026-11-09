//! The height map: `U2DFractal::ComputeFractal` (diamond-square), `HMUFractal`'s value generator and
//! `CMUFilterHeightMap`.

use crate::grid::is_shore_tile;
use crate::map::{Grid, Map, URandomState};
use ee_random::URandom;

fn is_tile_clear(tiles: &Grid<u32>, x: u32, y: u32) -> bool {
    (tiles.get(x, y) & 0x7fff) == 0
}

/// `HMUFractal::GenerateFractalValue`: one tile's height, also stored in the map.
fn fractal_value(
    map: &mut Map,
    random: &mut URandomState,
    range: f32,
    scale: f32,
    average: f32,
    x: u32,
    y: u32,
) -> f32 {
    let s = map.s;
    let low = s.land_elevation_threshold - s.elevation_unit * 0.5;
    let mut high = s.land_elevation_threshold - s.elevation_unit;
    if s.is_space_map != 0 && is_tile_clear(&map.tiles, x, y) {
        map.heights.set(x, y, low);
        return low;
    }
    let land = !is_tile_clear(&map.tiles, x, y);
    let flag = map.flat_tiles.get(x, y);
    let shore = is_shore_tile(&map.tiles, x, y);
    let mut average = average;
    let value = if flag != 0 {
        s.flat_elevation
    } else if shore {
        low
    } else if land {
        if average < s.land_elevation_threshold {
            average = s.land_elevation_threshold - average + s.land_elevation_threshold;
        }
        let v = average - (range - random.generate() * (range + range)) * s.elevation_scale * scale;
        let v = if v > s.land_elevation_threshold { v } else { s.land_elevation_threshold };
        if v < 1.0 {
            v
        } else {
            1.0
        }
    } else {
        if s.coastal_to_deeper_water_percentage > 0.0 && map.random.generate() <= s.coastal_to_deeper_water_percentage {
            high -= s.elevation_unit;
        }
        if average > high {
            average = high;
        }
        let v = average + (range - random.generate() * (range + range)) * s.elevation_scale * scale;
        let v = if v > 0.0 { v } else { 0.0 };
        if v < high {
            v
        } else {
            high
        }
    };
    map.heights.set(x, y, value);
    value
}

/// `CMUCreateHeightMap`: `U2DFractal::ComputeFractal` from the script's initial elevation, chaos and seed. The
/// fractal's own stream is seeded raw (`theRandom.mSeed = inSeed`, no 0 -> 1 fix-up).
pub fn create_height_map(map: &mut Map) {
    let dimension = map.s.grid_size;
    let corner = map.s.initial_elevation;
    let range = map.s.height_map_chaos;
    let mut random = URandomState(URandom { seed: map.s.seed });
    let d = dimension as usize;
    let mut values = vec![0.0f32; d * d];
    let at = |x: u32, y: u32| x as usize * d + y as usize;
    values[0] = corner;
    let mut step = dimension;
    if step <= 1 {
        return;
    }
    let the_dimension = step as f32;
    loop {
        let scale = step as f32 / the_dimension;
        let half = step / 2;
        let mut y = 0u32;
        while y < dimension {
            let mut x = 0u32;
            while x < dimension {
                let next_x = (x + step) % dimension;
                let next_y = (y + step) % dimension;
                let average =
                    (values[at(x, y)] + values[at(next_x, y)] + values[at(x, next_y)] + values[at(next_x, next_y)])
                        * 0.25;
                values[at(x + half, y + half)] =
                    fractal_value(map, &mut random, range, scale, average, x + half, y + half);
                x += step;
            }
            y += step;
        }
        let mut y = 0u32;
        while y < dimension {
            let mut x = 0u32;
            while x < dimension {
                let next_x = (x + step) % dimension;
                let previous_x = x.wrapping_sub(half) % dimension;
                let next_y = (y + step) % dimension;
                let average = (values[at(previous_x, y + half)]
                    + values[at(x, next_y)]
                    + values[at(x + half, y + half)]
                    + values[at(x, y)])
                    * 0.25;
                values[at(x, y + half)] = fractal_value(map, &mut random, range, scale, average, x, y + half);
                let previous_y = y.wrapping_sub(half) % dimension;
                let average = (values[at(x + half, previous_y)]
                    + values[at(next_x, y)]
                    + values[at(x, y)]
                    + values[at(x + half, y + half)])
                    * 0.25;
                values[at(x + half, y)] = fractal_value(map, &mut random, range, scale, average, x + half, y);
                x += step;
            }
            y += step;
        }
        step /= 2;
        if step <= 1 {
            break;
        }
    }
}

/// One tile of `CMUFilterHeightMap`'s sweep. `neighbors` in the C++'s order: the two along one edge (the second only
/// where the first exists) and the third. A tile touching land and water flattens to sea level with those neighbours.
fn filter_cell(heights: &mut Grid<f32>, center: (u32, u32), neighbors: [Option<(u32, u32)>; 3], sea: f32, step: f32) {
    let mut value = heights.get(center.0, center.1);
    let mut higher = 0;
    let mut lower = 0;
    let mut above = [false; 3];
    for (i, neighbor) in neighbors.iter().enumerate() {
        let Some((x, y)) = *neighbor else { continue };
        let height = heights.get(x, y);
        if height > sea {
            higher += 1;
            above[i] = true;
        } else if height < sea {
            lower += 1;
        }
        if value - height > step {
            value = height + step;
        }
    }
    if higher > 0 {
        if lower > 0 {
            value = sea;
            for i in [2, 1, 0] {
                if above[i] {
                    let (x, y) = neighbors[i].expect("above means present");
                    heights.set(x, y, sea);
                }
            }
        } else if value < sea {
            value = sea;
        }
    } else if lower > 0 && value > sea {
        value = sea;
    }
    heights.set(center.0, center.1, value);
}

/// `CMUFilterHeightMap`: sweeps the height map diagonally in both directions.
pub fn filter_height_map(map: &mut Map) {
    let range = map.s.max_int_elevation as f32 - map.s.min_int_elevation as f32;
    let step = 1.0f32 / range;
    let sea = -(map.s.min_int_elevation as f32 / range);
    let size = map.s.grid_size as i32;
    let h = &mut map.heights;
    let c = |a: i32, b: i32| (a as u32, b as u32);
    let mut i = size;
    while i > 0 {
        let mut j = 0;
        loop {
            let x = i - 1 + j;
            if x >= size {
                break;
            }
            filter_cell(
                h,
                c(j, x),
                [
                    (j > 0).then(|| c(j - 1, x)),
                    (j > 0 && x < size - 1).then(|| c(j - 1, x + 1)),
                    (x < size - 1).then(|| c(j, x + 1)),
                ],
                sea,
                step,
            );
            filter_cell(
                h,
                c(x, j),
                [
                    (x < size - 1).then(|| c(x + 1, j)),
                    (x < size - 1 && j > 0).then(|| c(x + 1, j - 1)),
                    (j > 0).then(|| c(x, j - 1)),
                ],
                sea,
                step,
            );
            j += 1;
        }
        i -= 1;
    }
    for i in 0..size {
        let mut j = 0;
        loop {
            let x = i + j;
            if x >= size {
                break;
            }
            filter_cell(
                h,
                c(x, j),
                [
                    (x > 0).then(|| c(x - 1, j)),
                    (x > 0 && j < size - 1).then(|| c(x - 1, j + 1)),
                    (j < size - 1).then(|| c(x, j + 1)),
                ],
                sea,
                step,
            );
            filter_cell(
                h,
                c(j, x),
                [
                    (j < size - 1).then(|| c(j + 1, x)),
                    (j < size - 1 && x > 0).then(|| c(j + 1, x - 1)),
                    (x > 0).then(|| c(j, x - 1)),
                ],
                sea,
                step,
            );
            j += 1;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn test_map(size: u32) -> Map {
        let mut map = Map::default();
        map.s.grid_size = size;
        map.s.land_elevation_threshold = 0.5;
        map.s.elevation_unit = 0.05;
        map.s.elevation_scale = 0.2;
        map.s.height_map_chaos = 0.3;
        map.s.initial_elevation = 0.5;
        map.s.flat_elevation = 0.6;
        map.s.min_int_elevation = -10;
        map.s.max_int_elevation = 20;
        map.tiles = Grid::new(size + 1, 0);
        map.heights = Grid::new(size + 1, 0.5);
        map.flat_tiles = Grid::new(size + 1, 0);
        map.random = URandomState(URandom::new(1));
        map
    }

    #[test]
    fn water_stays_below_the_shore_height() {
        let mut map = test_map(8);
        map.s.seed = 3;
        create_height_map(&mut map);
        let high = 0.5 - 0.05;
        for x in 0..8 {
            for y in 0..8 {
                if (x, y) != (0, 0) {
                    assert!(map.heights.get(x, y) <= high, "{x} {y}");
                }
            }
        }
    }

    #[test]
    fn flat_tiles_get_the_flat_elevation_and_the_same_seed_repeats() {
        let mut map = test_map(8);
        map.s.seed = 3;
        map.flat_tiles.set(4, 4, 1);
        create_height_map(&mut map);
        assert_eq!(map.heights.get(4, 4), 0.6);
        let first = map.heights.data.clone();
        let mut map = test_map(8);
        map.s.seed = 3;
        map.flat_tiles.set(4, 4, 1);
        create_height_map(&mut map);
        assert_eq!(first, map.heights.data);
    }

    #[test]
    fn empty_space_map_tiles_sit_just_under_the_threshold() {
        let mut map = test_map(4);
        map.s.is_space_map = 1;
        create_height_map(&mut map);
        assert_eq!(map.heights.get(2, 2), 0.5 - 0.05 * 0.5);
    }

    #[test]
    fn the_filter_limits_the_slope_to_one_step() {
        let mut map = test_map(4);
        // above sea level everywhere (sea = 1/3): a peak gets pulled down to a neighbour plus a step
        map.heights = Grid::new(5, 0.5);
        map.heights.set(1, 2, 0.9);
        filter_height_map(&mut map);
        let step = 1.0f32 / 30.0;
        assert!(map.heights.get(1, 2) <= 0.5 + step + 0.0001);
    }

    #[test]
    fn a_tile_touching_land_and_water_flattens_to_sea_level() {
        let mut heights = Grid::new(3, 0.0f32);
        heights.set(0, 0, 0.5); // above sea (0.3)
        heights.set(1, 1, 0.1); // below
        heights.set(0, 1, 0.5);
        filter_cell(&mut heights, (1, 0), [Some((0, 0)), Some((1, 1)), Some((0, 1))], 0.3, 1.0);
        assert_eq!(heights.get(1, 0), 0.3);
        assert_eq!(heights.get(0, 0), 0.3);
        assert_eq!(heights.get(0, 1), 0.3);
        assert_eq!(heights.get(1, 1), 0.1);
    }
}
