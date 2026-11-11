//! The AI areas' geometry on their hulls (`EEAIArea.cpp`, `EEAIContinent.cpp`): the closest points of an area's
//! tiles (or a continent's shore) to another area's hull, the hull point nearest a tile, the hulls' distance, the
//! tiles under a hull, and a continent's shore tiles in a set of water bodies or space regions. The C++ classes keep
//! their objects; the shore walk asks the continent manager through `ShoreHost` in the C++'s order and number.

use std::ffi::c_long;

use crate::hull::{self, EeHullPoint};

/// A hull's bounds (`U2DRectangle<long>`, its members in their order).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeHullRect {
    pub bottom: c_long,
    pub left: c_long,
    pub right: c_long,
    pub top: c_long,
}

impl EeHullRect {
    /// `U2DRectangle<long>::Contains`: edges included.
    pub fn contains(&self, p: EeHullPoint) -> bool {
        p.x >= self.left && p.x <= self.right && p.y >= self.top && p.y <= self.bottom
    }
}

/// `U2DConvexHull<long>::BoundedContains`: the bounds first, then the hull.
pub fn bounded_contains(outline: &[EeHullPoint], bounds: &EeHullRect, p: EeHullPoint) -> bool {
    bounds.contains(p) && hull::contains(outline, p)
}

/// `EEAIArea::GetClosestHullPoint`: the tile itself inside the hull, else the hull's closest outline point.
pub fn closest_hull_point(outline: &[EeHullPoint], bounds: &EeHullRect, p: EeHullPoint) -> EeHullPoint {
    if !bounded_contains(outline, bounds, p) {
        hull::closest_outline_point(outline, p)
    } else {
        p
    }
}

/// `EEAIArea::GetHullDistanceSquared`: the `float` squared distance of the two hulls' closest points.
pub fn hull_distance_squared(outline: &[EeHullPoint], other: &[EeHullPoint]) -> f32 {
    let (a, b) = hull::closest_points(outline, other);
    hull::distance_squared_f(a, b)
}

/// The loop of `EEAIArea::GetClosestPoints` (over the area's tiles, `BuildPointList(1)`) and
/// `EEAIContinent::GetClosestPoints` (over the shore tiles): from the last point, each point's closest outline point
/// of `other`, the first strictly nearer pair kept (by `long` squared distance). `None` for no points.
pub fn closest_points_to_hull(points: &[EeHullPoint], other: &[EeHullPoint]) -> Option<(EeHullPoint, EeHullPoint)> {
    let last = *points.last()?;
    let mut this = last;
    let mut closest = hull::closest_outline_point(other, last);
    let mut best = hull::distance_squared(this, closest);
    for i in (0..points.len() - 1).rev() {
        let candidate = hull::closest_outline_point(other, points[i]);
        let distance = hull::distance_squared(candidate, points[i]);
        if distance >= best {
            continue;
        }
        best = distance;
        this = points[i];
        closest = candidate;
    }
    Some((this, closest))
}

/// `EEAIArea::GetTiles`: every tile of the hull's scan lines, row by row (lower y first), each row from its first x.
pub fn tiles(outline: &[EeHullPoint], out: &mut Vec<EeHullPoint>) {
    for (y, first, last) in hull::scan_lines(outline) {
        let mut x = first;
        while x <= last {
            out.push(EeHullPoint::new(x, y));
            x = x.wrapping_add(1);
        }
    }
}

/// What `EEAIContinent::GetShoreInRegions` asks the continent manager.
pub trait ShoreHost {
    /// `GetPathAreaAt(tile)`: `None` without an area, else its region id of the asked kind (the space region's when
    /// `space`, else the water body's).
    fn path_area_region(&mut self, tile: EeHullPoint, space: bool) -> Option<u16>;
    /// `GetContinentAt(x, y)` exists and is a land continent.
    fn is_land(&mut self, tile: EeHullPoint) -> bool;
    /// `GetContinent(tile, kind)`: the region id (space region or water body) of the path area at the tile, 0 without.
    fn region(&mut self, tile: EeHullPoint, space: bool) -> u16;
    /// `gMapWidth`, `gMapHeight`.
    fn map_size(&self) -> (u32, u32);
}

/// `EEAIContinent::GetShoreInRegions`: the shore tiles (from the last) whose path area lies in one of `ids`' regions
/// (the space regions when `space`, else the water bodies), or a land continent's tiles with a neighbour in one;
/// every shore tile when `ids` is empty or holds a 0. `out` is cleared first.
///
/// The neighbours in the C++'s order (y - 1, y, y + 1 for x - 1, then x, then x + 1), each on the map; the ids from
/// the last, asking the region again for each (as the C++ did).
pub fn shore_in_regions(
    shore: &[EeHullPoint],
    ids: &[u16],
    space: bool,
    host: &mut impl ShoreHost,
    out: &mut Vec<EeHullPoint>,
) {
    out.clear();
    if ids.is_empty() || ids.contains(&0) {
        out.extend_from_slice(shore);
        return;
    }
    const NEIGHBOURS: [(c_long, c_long); 8] = [(-1, -1), (-1, 0), (-1, 1), (0, -1), (0, 1), (1, -1), (1, 0), (1, 1)];
    let (width, height) = host.map_size();
    for &tile in shore.iter().rev() {
        if let Some(id) = host.path_area_region(tile, space) {
            if id != 0 {
                if ids.iter().rev().any(|&i| i == id) {
                    out.push(tile);
                }
                continue;
            }
        }
        if !host.is_land(tile) {
            continue;
        }
        for (dx, dy) in NEIGHBOURS {
            let neighbour = EeHullPoint::new(tile.x.wrapping_add(dx), tile.y.wrapping_add(dy));
            // `(unsigned long)x < gMapWidth`
            if !((neighbour.x as u32) < width && (neighbour.y as u32) < height) {
                continue;
            }
            let mut found = false;
            for &id in ids.iter().rev() {
                found = host.region(neighbour, space) == id;
                if found {
                    break;
                }
            }
            if found {
                out.push(tile);
                break;
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn p(x: c_long, y: c_long) -> EeHullPoint {
        EeHullPoint::new(x, y)
    }

    fn square() -> (Vec<EeHullPoint>, EeHullRect) {
        let (outline, shape) = hull::set_points(&[p(0, 0), p(4, 0), p(4, 4), p(0, 4)], false);
        (outline, EeHullRect { bottom: shape.bottom, left: shape.left, right: shape.right, top: shape.top })
    }

    #[test]
    fn the_hull_point_is_the_tile_inside_and_the_outline_outside() {
        let (outline, bounds) = square();
        assert_eq!(closest_hull_point(&outline, &bounds, p(2, 3)), p(2, 3));
        assert_eq!(closest_hull_point(&outline, &bounds, p(4, 4)), p(4, 4));
        assert_eq!(closest_hull_point(&outline, &bounds, p(7, 4)), p(4, 4));
        assert!(bounded_contains(&outline, &bounds, p(0, 4)));
        assert!(!bounded_contains(&outline, &bounds, p(-1, 2)));
        // the bounds decide first: a hull without points is never asked
        assert!(!bounded_contains(&[], &EeHullRect::default(), p(1, 1)));
    }

    #[test]
    fn the_closest_tile_is_the_first_strictly_nearer_from_the_end() {
        let (outline, _) = square();
        // two tiles at the same distance from the hull: the last one stays
        let tiles = [p(6, 1), p(6, 3), p(9, 2)];
        assert_eq!(closest_points_to_hull(&tiles, &outline), Some((p(6, 3), p(4, 3))));
        assert_eq!(closest_points_to_hull(&[p(9, 2), p(6, 1)], &outline), Some((p(6, 1), p(4, 1))));
        assert_eq!(closest_points_to_hull(&[], &outline), None);
        assert_eq!(hull_distance_squared(&outline, &[p(8, 0), p(8, 4), p(12, 4), p(12, 0)]), 16.0);
    }

    #[test]
    fn the_tiles_of_a_hull_row_by_row() {
        let (outline, _) = hull::set_points(&[p(1, 1), p(2, 1), p(2, 2), p(1, 2)], false);
        let mut out = vec![p(9, 9)];
        tiles(&outline, &mut out);
        assert_eq!(out, vec![p(9, 9), p(1, 1), p(2, 1), p(1, 2), p(2, 2)]);
    }

    struct Map {
        /// (tile, region) of the path areas at the shore and around it; land at x < 5
        areas: Vec<(EeHullPoint, u16)>,
        asked: usize,
    }

    impl ShoreHost for Map {
        fn path_area_region(&mut self, tile: EeHullPoint, _space: bool) -> Option<u16> {
            self.areas.iter().find(|(t, _)| *t == tile).map(|&(_, r)| r)
        }
        fn is_land(&mut self, tile: EeHullPoint) -> bool {
            tile.x < 5
        }
        fn region(&mut self, tile: EeHullPoint, space: bool) -> u16 {
            self.asked += 1;
            self.path_area_region(tile, space).unwrap_or(0)
        }
        fn map_size(&self) -> (u32, u32) {
            (8, 8)
        }
    }

    #[test]
    fn the_shore_in_regions_by_area_or_by_a_neighbour() {
        let shore = [p(4, 0), p(4, 3), p(6, 3), p(0, 7)];
        // (6, 3) has a path area in region 2; (4, 3) has a region-0 area and a neighbour (5, 3) in region 2;
        // (4, 0) a neighbour (5, -1) off the map and none else; (0, 7) a neighbour in region 9
        let mut map = Map { areas: vec![(p(6, 3), 2), (p(4, 3), 0), (p(5, 3), 2), (p(1, 7), 9)], asked: 0 };
        let mut out = vec![p(1, 1)];
        shore_in_regions(&shore, &[2, 3], false, &mut map, &mut out);
        assert_eq!(out, vec![p(6, 3), p(4, 3)]);
        // every shore tile with no ids or a 0 among them
        shore_in_regions(&shore, &[], false, &mut map, &mut out);
        assert_eq!(out, shore.to_vec());
        shore_in_regions(&shore, &[5, 0], true, &mut map, &mut out);
        assert_eq!(out, shore.to_vec());
        // the region is asked once per id until one matches
        map.asked = 0;
        shore_in_regions(&[p(4, 3)], &[7, 2], false, &mut map, &mut out);
        assert_eq!(out, vec![p(4, 3)]);
        // neighbours (3, 2) (3, 3) (3, 4) (4, 2) (4, 4) (5, 2): two ids each; (5, 3): 2 matches on the first asked
        assert_eq!(map.asked, 13);
    }
}
