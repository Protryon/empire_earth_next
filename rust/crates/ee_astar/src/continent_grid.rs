//! The continent manager's grid from the continent fill (`EEAIContinentManager::CreateContinentGrid`, 0x41fa5c,
//! with `AddTileValue` 0x424fea and `AddShoreTile` 0x41f3e0), line by line: the tile-to-continent id grid, the
//! continent adjacency matrix and the neighbour lists (in the C++'s push order), the shore tiles (the water and space
//! tiles next to land, then the land tiles next to those), the ice shore tiles, and every continent's shore tiles.
//! The C++ manager stores what comes out (its grids, the continents' vectors).
//!
//! Part of the deterministic game (RUST_PLAN 2.1): integer arithmetic only, the C++'s loop orders; the lists the C++
//! sorts come out sorted (`U2DPoint<long>::operator<`: the lower y first, then the larger x), made unique where the
//! C++ makes them unique.

use crate::goals::{
    CATEGORY_LAND, CATEGORY_MAP_EDGE, CATEGORY_SPACE, CATEGORY_WATER, TERRAIN_CLIFF, TERRAIN_DEEP_WATER, TERRAIN_SPACE,
};
use crate::modifiers::TERRAIN_SHALLOW_WATER;
use std::cmp::Ordering;

/// A tile (`U2DPoint<long>`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EePoint {
    pub x: i32,
    pub y: i32,
}

/// `U2DPoint<long>::operator<`: the lower y first, then the larger x.
pub fn point_less(a: EePoint, b: EePoint) -> bool {
    a.y < b.y || (a.y == b.y && a.x > b.x)
}

/// `operator<` as an ordering (for the sorts and the binary searches over the C++'s sorted vectors).
pub fn point_cmp(a: EePoint, b: EePoint) -> Ordering {
    if point_less(a, b) {
        Ordering::Less
    } else if point_less(b, a) {
        Ordering::Greater
    } else {
        Ordering::Equal
    }
}

/// `std::binary_search` of a point in a vector sorted by `operator<`.
pub fn contains_point(sorted: &[EePoint], p: EePoint) -> bool {
    sorted.binary_search_by(|&q| point_cmp(q, p)).is_ok()
}

/// `std::sort` then `std::unique` + `erase`.
pub fn sort_unique(points: &mut Vec<EePoint>) {
    points.sort_by(|&a, &b| point_cmp(a, b));
    points.dedup();
}

/// A continent's neighbour (`mNeighbors.push_back`): continent `id` gets `neighbor`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeContinentNeighbor {
    pub id: u16,
    pub neighbor: u16,
}

/// A continent's shore tile.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeContinentShore {
    pub id: u16,
    pub tile: EePoint,
}

/// The fill's tile (`EEAIAStarContinentTile`): the continent id and the fill's kind (the continent's category).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct FillTile {
    pub id: u16,
    pub kind: u8,
}

/// What `CreateContinentGrid` computes, for the C++ to store.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct ContinentGrid {
    /// `mContinentIDs` (`x * width + y`)
    pub ids: Vec<u16>,
    /// `mTileFlags`: `next_id * next_id` adjacency flags (`[a * next_id + b]`)
    pub flags: Vec<u8>,
    /// the continents' `mNeighbors.push_back`s in order
    pub neighbors: Vec<EeContinentNeighbor>,
    /// `mShoreTiles`, sorted and unique
    pub shore: Vec<EePoint>,
    /// `mIceShoreTiles`, sorted (the manager's list was empty: `Reset` cleared it)
    pub ice_shore: Vec<EePoint>,
    /// every continent's `mShoreTiles` after `SortTiles(true)`: sorted and unique, grouped by continent id
    pub continent_shore: Vec<EeContinentShore>,
}

/// A terrain grid's entry where the map has no tile (`RTSMap::GetTile` gives NULL).
pub const NO_TILE: i32 = -1;

/// The land test of `AddTileValue`: a neighbour that is not deep water, shallow water, cliff or space.
fn is_land(terrain: i32) -> bool {
    terrain != TERRAIN_DEEP_WATER
        && terrain != TERRAIN_SHALLOW_WATER
        && terrain != TERRAIN_CLIFF
        && terrain != TERRAIN_SPACE
}

struct Map<'a> {
    size: i32,
    terrain: &'a [i32],
}

impl Map<'_> {
    /// `RTSMap::GetTile`: the terrain of a tile on the map; none off the map or where the map has no tile (the
    /// tiles outside the playable rhombus are NULL: `NO_TILE` in the grid)
    fn terrain(&self, x: i32, y: i32) -> Option<i32> {
        if (x as u32) < self.size as u32 && (y as u32) < self.size as u32 {
            let t = self.terrain[(x * self.size + y) as usize];
            (t != NO_TILE).then_some(t)
        } else {
            None
        }
    }
}

/// 0x424fea `AddTileValue`: records water or space tile `(x, y)` as a shore tile when a neighbour is land (not
/// water, space or ice), else as an ice-shore tile when a neighbour is ice. The neighbours in the C++'s order: the
/// first land one ends the walk.
fn add_tile_value(map: &Map, x: i32, y: i32, shore: &mut Vec<EePoint>, ice_shore: &mut Vec<EePoint>) {
    let Some(terrain) = map.terrain(x, y) else {
        return;
    };
    if terrain != TERRAIN_DEEP_WATER && terrain != TERRAIN_SHALLOW_WATER && terrain != TERRAIN_SPACE {
        return;
    }
    let mut ice = false;
    for (dx, dy) in [(-1, 0), (-1, 1), (-1, -1), (1, 0), (1, 1), (1, -1), (0, -1), (0, 1)] {
        if let Some(other) = map.terrain(x + dx, y + dy) {
            let land = is_land(other);
            ice = ice || (!land && other == TERRAIN_CLIFF);
            if land {
                shore.push(EePoint { x, y });
                return;
            }
        }
    }
    if ice {
        ice_shore.push(EePoint { x, y });
    }
}

/// 0x41f3e0 `AddShoreTile`: the shore step from water tile `from` to `tile`. True when `tile` is off the map, not on
/// a land continent or shallow water; otherwise `tile` joins the shore tiles and, with `from`, its continent's.
fn add_shore_tile(
    map: &Map,
    ids: &[u16],
    categories: &[u8],
    tile: EePoint,
    from: EePoint,
    shore: &mut Vec<EePoint>,
    continent_shore: &mut Vec<EeContinentShore>,
) -> bool {
    let mut result = false;
    if (tile.x as u32) < map.size as u32 && (tile.y as u32) < map.size as u32 {
        let id = ids[(tile.x * map.size + tile.y) as usize];
        result = true;
        if id != 0 && categories[id as usize] == CATEGORY_LAND {
            result = map.terrain(tile.x, tile.y) == Some(TERRAIN_SHALLOW_WATER);
            if !result {
                shore.push(tile);
                continent_shore.push(EeContinentShore { id, tile });
                continent_shore.push(EeContinentShore { id, tile: from });
            }
        }
    }
    result
}

/// 0x41fa5c `CreateContinentGrid` over a `size` x `size` map: `fill` the continent fill's tiles (`x * size + y`),
/// `terrain` every tile's terrain type (`NO_TILE` where the map has none), `categories` the continents' categories by id (`next_id` of them, 0 unused).
/// The neighbour lists are built in the C++'s order (x, then y; the tile at x - 1, then at y - 1); a tile of a map
/// edge continent (kind 5) makes no neighbours and no shore.
pub fn create_continent_grid(
    size: i32,
    fill: &[FillTile],
    terrain: &[i32],
    categories: &[u8],
    next_id: i32,
) -> ContinentGrid {
    let n = (size * size) as usize;
    assert!(fill.len() >= n && terrain.len() >= n && categories.len() >= next_id as usize);
    let map = Map { size, terrain };
    let mut out = ContinentGrid { ids: vec![0; n], flags: vec![0; (next_id * next_id) as usize], ..Default::default() };
    let width = next_id as usize;
    // a continent's neighbour list so far (`HasNeighbor`)
    let mut has_neighbor = vec![Vec::<u16>::new(); width];
    let mut add_neighbor = |out: &mut ContinentGrid, id: u16, other: u16| {
        out.flags[id as usize * width + other as usize] = 1;
        out.flags[other as usize * width + id as usize] = 1;
        if !has_neighbor[id as usize].contains(&other) {
            has_neighbor[id as usize].push(other);
            out.neighbors.push(EeContinentNeighbor { id, neighbor: other });
        }
        if !has_neighbor[other as usize].contains(&id) {
            has_neighbor[other as usize].push(id);
            out.neighbors.push(EeContinentNeighbor { id: other, neighbor: id });
        }
    };
    for x in 0..size {
        for y in 0..size {
            let at = (x * size + y) as usize;
            let id = fill[at].id;
            out.ids[at] = id;
            if id == 0 {
                continue;
            }
            out.flags[id as usize * width + id as usize] = 1;
            if fill[at].kind == CATEGORY_MAP_EDGE {
                continue;
            }
            if x > 0 {
                let other = fill[at - size as usize];
                if other.id != id && other.id != 0 && other.kind != CATEGORY_MAP_EDGE {
                    add_neighbor(&mut out, id, other.id);
                }
            }
            if y > 0 {
                let other = fill[at - 1];
                if other.id != id && other.id != 0 && other.kind != CATEGORY_MAP_EDGE {
                    add_neighbor(&mut out, id, other.id);
                }
            }
            add_tile_value(&map, x, y, &mut out.shore, &mut out.ice_shore);
        }
    }
    // the shore tiles so far (the water and space ones), from the last: each joins its continent's shore, and the
    // land tiles around a water or space one join the shore too (appended past this walk's end)
    let count = out.shore.len();
    for i in (0..count).rev() {
        let tile = out.shore[i];
        let id = out.ids[(tile.x * size + tile.y) as usize];
        if id == 0 {
            continue;
        }
        out.continent_shore.push(EeContinentShore { id, tile });
        let category = categories[id as usize];
        if category != CATEGORY_WATER && category != CATEGORY_SPACE {
            continue;
        }
        for (dx, dy) in [(-1, 0), (-1, 1), (-1, -1), (1, 0), (1, 1), (1, -1), (0, -1), (0, 1)] {
            let point = EePoint { x: tile.x + dx, y: tile.y + dy };
            add_shore_tile(&map, &out.ids, categories, point, tile, &mut out.shore, &mut out.continent_shore);
        }
    }
    sort_unique(&mut out.shore);
    out.ice_shore.sort_by(|&a, &b| point_cmp(a, b));
    // `SortTiles(true)` of every continent: by continent, each list sorted and unique
    let mut by_continent = vec![Vec::<EePoint>::new(); width];
    for entry in &out.continent_shore {
        by_continent[entry.id as usize].push(entry.tile);
    }
    out.continent_shore.clear();
    for (id, list) in by_continent.iter_mut().enumerate() {
        sort_unique(list);
        for &tile in list.iter() {
            out.continent_shore.push(EeContinentShore { id: id as u16, tile });
        }
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    fn p(x: i32, y: i32) -> EePoint {
        EePoint { x, y }
    }

    /// A map from rows of characters: 'L' land, 'W' deep water, 'S' shallow water, 'C' cliff, 'X' space, 'E' an edge
    /// (land terrain in a map edge continent), '.' no tile. Continents by letter runs: each distinct character gets its own id
    /// in reading order, so a test map must keep each continent's tiles under one character per continent id.
    fn world(rows: &[&str], continents: &[(char, u16)]) -> (i32, Vec<FillTile>, Vec<i32>, Vec<u8>, i32) {
        let size = rows.len() as i32;
        let next_id = continents.iter().map(|c| c.1).max().unwrap_or(0) as i32 + 1;
        let mut fill = vec![FillTile::default(); (size * size) as usize];
        let mut terrain = vec![0; (size * size) as usize];
        let mut categories = vec![0u8; next_id as usize];
        // rows are y, columns x (`x * size + y`)
        for (y, row) in rows.iter().enumerate() {
            for (x, ch) in row.chars().enumerate() {
                let at = x * size as usize + y;
                let (t, cat) = match ch {
                    'L' => (0, CATEGORY_LAND),
                    'W' => (TERRAIN_DEEP_WATER, CATEGORY_WATER),
                    'S' => (TERRAIN_SHALLOW_WATER, CATEGORY_LAND),
                    'C' => (TERRAIN_CLIFF, 3),
                    'X' => (TERRAIN_SPACE, CATEGORY_SPACE),
                    'E' => (0, CATEGORY_MAP_EDGE),
                    '.' => (NO_TILE, 0),
                    _ => panic!(),
                };
                let id = continents.iter().find(|c| c.0 == ch).map(|c| c.1).unwrap_or(0);
                terrain[at] = t;
                fill[at] = FillTile { id, kind: cat };
                if id != 0 {
                    categories[id as usize] = cat;
                }
            }
        }
        (size, fill, terrain, categories, next_id)
    }

    #[test]
    fn ids_flags_and_neighbours_follow_the_fill_in_the_cpp_order() {
        // x = 0..1 land (1), x = 2 water (2), x = 3 land (3); rows are y
        let rows = ["LLWL", "LLWL", "LLWL", "LLWL"];
        let (size, fill, terrain, categories, next) = world(&rows, &[('L', 1), ('W', 2)]);
        // the right column is another land continent: patch its ids
        let mut fill = fill;
        for y in 0..4 {
            fill[3 * 4 + y].id = 3;
        }
        let mut categories = categories;
        categories.push(CATEGORY_LAND);
        let g = create_continent_grid(size, &fill, &terrain, &categories, next + 1);
        assert_eq!(g.ids[0], 1);
        assert_eq!(g.ids[2 * 4 + 1], 2);
        assert_eq!(g.ids[3 * 4], 3);
        let w = 4usize;
        assert_eq!(g.flags[w + 1], 1);
        assert_eq!(g.flags[2 * w + 2], 1);
        assert_eq!(g.flags[w + 2], 1);
        assert_eq!(g.flags[2 * w + 1], 1);
        assert_eq!(g.flags[2 * w + 3], 1);
        assert_eq!(g.flags[3 * w + 2], 1);
        // land 1 and land 3 never touch
        assert_eq!(g.flags[w + 3], 0);
        assert_eq!(g.flags[3 * w + 1], 0);
        assert_eq!(g.flags[0], 0);
        // the walk meets the water at (2, 0) whose x - 1 neighbour is land 1: 2 gets 1, then 1 gets 2; then at
        // (3, 0) land 3 meets water 2: 3 gets 2, 2 gets 3
        assert_eq!(
            g.neighbors,
            vec![
                EeContinentNeighbor { id: 2, neighbor: 1 },
                EeContinentNeighbor { id: 1, neighbor: 2 },
                EeContinentNeighbor { id: 3, neighbor: 2 },
                EeContinentNeighbor { id: 2, neighbor: 3 },
            ]
        );
        // the shore: the water column (next to land) and both land columns beside it, sorted: y first, larger x first
        let mut want: Vec<EePoint> = (0..4).flat_map(|y| [p(3, y), p(2, y), p(1, y)]).collect();
        want.sort_by(|&a, &b| point_cmp(a, b));
        assert_eq!(g.shore, want);
        assert!(g.ice_shore.is_empty());
        // the water continent's shore tiles are its own tiles; the land ones' the land tiles next to the water, and
        // the water tiles they were reached from
        let shore_of =
            |id: u16| -> Vec<EePoint> { g.continent_shore.iter().filter(|e| e.id == id).map(|e| e.tile).collect() };
        let mut w2: Vec<EePoint> = (0..4).map(|y| p(2, y)).collect();
        w2.sort_by(|&a, &b| point_cmp(a, b));
        assert_eq!(shore_of(2), w2);
        let mut l1: Vec<EePoint> = (0..4).flat_map(|y| [p(1, y), p(2, y)]).collect();
        l1.sort_by(|&a, &b| point_cmp(a, b));
        assert_eq!(shore_of(1), l1);
        let mut l3: Vec<EePoint> = (0..4).flat_map(|y| [p(3, y), p(2, y)]).collect();
        l3.sort_by(|&a, &b| point_cmp(a, b));
        assert_eq!(shore_of(3), l3);
        assert!(shore_of(0).is_empty());
    }

    #[test]
    fn a_map_edge_continent_makes_no_neighbours_and_shallows_are_no_shore_step() {
        // y = 0 is the edge continent (3); x = 0 water (2) with shallows at (0, 2); the rest land (1)
        let rows = ["EEE", "WLL", "SLL"];
        let (size, fill, terrain, categories, next) = world(&rows, &[('L', 1), ('W', 2), ('E', 3)]);
        let mut fill = fill;
        // the shallow tile belongs to the land continent
        fill[2].id = 1;
        let g = create_continent_grid(size, &fill, &terrain, &categories, next);
        // the edge continent is adjacent to nothing but itself, though it touches both (the matrix is 4 x 4)
        assert_eq!(next, 4);
        assert_eq!(g.flags[3 * 4 + 3], 1);
        assert_eq!(g.flags[3 * 4 + 1], 0);
        assert_eq!(g.flags[4 + 3], 0);
        assert_eq!(g.flags[2 * 4 + 1], 1);
        assert_eq!(g.flags[4 + 2], 1);
        assert_eq!(
            g.neighbors,
            vec![EeContinentNeighbor { id: 1, neighbor: 2 }, EeContinentNeighbor { id: 2, neighbor: 1 }]
        );
        // (0, 1) is water next to land (its x + 1 neighbour): a shore tile; (0, 2) shallow next to land: a shore
        // tile too (shallows are a water terrain for AddTileValue); the edge row's land tiles make no shore, and
        // the step from (0, 1) to (0, 0) (an edge continent) and to (0, 2) (shallows) adds nothing
        assert!(g.shore.contains(&p(0, 1)));
        assert!(g.shore.contains(&p(0, 2)));
        assert!(!g.shore.contains(&p(0, 0)));
        assert!(!g.shore.contains(&p(1, 0)));
        assert!(g.shore.contains(&p(1, 1)));
        assert!(g.shore.contains(&p(1, 2)));
        assert_eq!(g.shore.len(), 4);
        // (0, 2) is on the land continent: its own shore tile, so it is listed under 1; the water tile's continent 2
        // lists (0, 1)
        let shore_of =
            |id: u16| -> Vec<EePoint> { g.continent_shore.iter().filter(|e| e.id == id).map(|e| e.tile).collect() };
        assert_eq!(shore_of(2), vec![p(0, 1)]);
        assert_eq!(shore_of(1), vec![p(1, 1), p(0, 1), p(1, 2), p(0, 2)]);
    }

    #[test]
    fn ice_next_to_water_without_land_is_an_ice_shore_tile_sorted() {
        // water (2) with cliffs (3) around at y = 0: no land anywhere
        let rows = ["CCC", "WWW", "WWW"];
        let (size, fill, terrain, categories, next) = world(&rows, &[('W', 2), ('C', 3)]);
        let g = create_continent_grid(size, &fill, &terrain, &categories, next);
        assert!(g.shore.is_empty());
        assert_eq!(g.ice_shore, vec![p(2, 1), p(1, 1), p(0, 1)]);
        assert!(g.continent_shore.is_empty());
        // the two continents are neighbours once each
        assert_eq!(g.neighbors.len(), 2);
    }

    #[test]
    fn a_missing_tile_is_neither_land_nor_shore_nor_a_step() {
        // the map's corner has no tile; the water at (0, 1) sees only it and land at (1, 1)
        let rows = [".LL", "WLL", "WLL"];
        let (size, fill, terrain, categories, next) = world(&rows, &[('L', 1), ('W', 2)]);
        let g = create_continent_grid(size, &fill, &terrain, &categories, next);
        assert_eq!(g.ids[0], 0);
        assert!(g.shore.contains(&p(0, 1)));
        assert!(!g.shore.contains(&p(0, 0)));
        assert!(g.ice_shore.is_empty());
        // water with only missing tiles and water around it is no shore at all
        let rows = ["...", ".W.", "..."];
        let (size, fill, terrain, categories, next) = world(&rows, &[('W', 2)]);
        let g = create_continent_grid(size, &fill, &terrain, &categories, next);
        assert!(g.shore.is_empty() && g.ice_shore.is_empty() && g.continent_shore.is_empty());
    }

    #[test]
    fn the_point_order_and_the_searches_are_the_cpp_ones() {
        assert!(point_less(p(5, 0), p(0, 1)));
        assert!(point_less(p(3, 1), p(2, 1)));
        assert!(!point_less(p(2, 1), p(2, 1)));
        let mut v = vec![p(0, 0), p(1, 0), p(0, 0), p(2, 1), p(5, 1)];
        sort_unique(&mut v);
        assert_eq!(v, vec![p(1, 0), p(0, 0), p(5, 1), p(2, 1)]);
        assert!(contains_point(&v, p(5, 1)));
        assert!(!contains_point(&v, p(3, 1)));
        assert!(!contains_point(&[], p(0, 0)));
    }
}
