//! The sub sector links of the continent manager (`EEAIContinentManager::LinkSubSectorGrid`, 0x420ee1, and the
//! re-linking walk of `RelinkSubSectors`, 0x4215dc), line by line: over a rectangle of tiles, every pair of
//! neighbouring sub sectors is linked (`LinkSubSectors`: both become neighbours; a diagonal pair, when the two
//! corner sub sectors beside it exist and differ from both, gets a link through those corners). The walk's decisions
//! come out as the list of `LinkSubSectors` calls, in order, which the C++ applies to its objects: the objects' state
//! (the neighbour lists, the links) never feeds the walk, only which sub sectors exist.
//!
//! Part of the deterministic game (RUST_PLAN 2.1): the C++'s loop order and tests.

/// One `LinkSubSectors(sub, &other, add, (first, second))` call: `first` and `second` are the corner sub sectors
/// of a diagonal link (both set when `add`, 0 otherwise).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeSubSectorLink {
    pub sub: u16,
    pub other: u16,
    pub add: bool,
    pub first: u16,
    pub second: u16,
}

/// The sub sectors as the walk sees them.
pub trait Grid {
    /// the sub sector id at a tile (0 none); asked only inside the walk's rectangle
    fn id_at(&mut self, x: i32, y: i32) -> u16;
    /// whether the manager lists a sub sector under `id`
    fn present(&mut self, id: u16) -> bool;
}

/// The diagonal step: `other` at the diagonal, `first` the corner at the other's column and this row, `second` the
/// corner at this column and the other's row. The link goes through both corners only when both exist and differ
/// from the pair; otherwise the two are plain neighbours.
fn diagonal<G: Grid + ?Sized>(
    grid: &mut G,
    id: u16,
    other: u16,
    first_at: (i32, i32),
    second_at: (i32, i32),
    out: &mut Vec<EeSubSectorLink>,
) {
    let mut count = 0;
    let mut first = 0;
    let mut second = 0;
    let f = grid.id_at(first_at.0, first_at.1);
    if f != id && f != other && f != 0 && grid.present(f) {
        first = f;
        count = 1;
        let s = grid.id_at(second_at.0, second_at.1);
        if s != id && s != other && s != 0 && grid.present(s) {
            second = s;
            count = 2;
        }
    }
    if count != 2 {
        first = 0;
        second = 0;
    }
    out.push(EeSubSectorLink { sub: id, other, add: count == 2, first, second });
}

/// The walk over `x0..x_end` x `y0..y_end`: for every tile with a sub sector, the links to the sub sectors at
/// (x - 1, y), (x, y - 1), the diagonal (x - 1, y - 1) through the corners (x - 1, y) and (x, y - 1), and the
/// diagonal (x + 1, y - 1) through (x + 1, y) and (x, y - 1), in that order. `LinkSubSectorGrid` walks the whole map
/// (`x_end`, `y_end` its size); `RelinkSubSectors` a sector grown by one tile on each side, clipped to the map.
pub fn link_walk<G: Grid + ?Sized>(
    grid: &mut G,
    x0: i32,
    x_end: i32,
    y0: i32,
    y_end: i32,
    out: &mut Vec<EeSubSectorLink>,
) {
    for x in x0..x_end {
        for y in y0..y_end {
            let id = grid.id_at(x, y);
            if id == 0 {
                continue;
            }
            if x > 0 {
                let other = grid.id_at(x - 1, y);
                if other != id && other != 0 {
                    out.push(EeSubSectorLink { sub: id, other, add: false, first: 0, second: 0 });
                }
            }
            if y > 0 {
                let other = grid.id_at(x, y - 1);
                if other != id && other != 0 {
                    out.push(EeSubSectorLink { sub: id, other, add: false, first: 0, second: 0 });
                }
            }
            if x > 0 && y > 0 {
                let other = grid.id_at(x - 1, y - 1);
                if other != id && other != 0 {
                    diagonal(grid, id, other, (x - 1, y), (x, y - 1), out);
                }
            }
            if x + 1 >= x_end || y < 1 {
                continue;
            }
            let other = grid.id_at(x + 1, y - 1);
            if other == id || other == 0 {
                continue;
            }
            // `y < y_end` always holds inside the walk
            diagonal(grid, id, other, (x + 1, y), (x, y - 1), out);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    struct Ids {
        size: i32,
        ids: Vec<u16>,
        missing: Vec<u16>,
        reads: Vec<(i32, i32)>,
    }

    impl Grid for Ids {
        fn id_at(&mut self, x: i32, y: i32) -> u16 {
            self.reads.push((x, y));
            assert!((x as u32) < self.size as u32 && (y as u32) < self.size as u32, "a read off the walk");
            self.ids[(x * self.size + y) as usize]
        }
        fn present(&mut self, id: u16) -> bool {
            !self.missing.contains(&id)
        }
    }

    fn grid(size: i32, rows: &[&str], missing: &[u16]) -> Ids {
        let mut ids = vec![0; (size * size) as usize];
        for (y, row) in rows.iter().enumerate() {
            for (x, ch) in row.chars().enumerate() {
                ids[x * size as usize + y] = ch.to_digit(10).unwrap() as u16;
            }
        }
        Ids { size, ids, missing: missing.to_vec(), reads: vec![] }
    }

    fn plain(sub: u16, other: u16) -> EeSubSectorLink {
        EeSubSectorLink { sub, other, add: false, first: 0, second: 0 }
    }

    #[test]
    fn orthogonal_neighbours_link_in_walk_order_and_a_diagonal_goes_through_its_corners() {
        // rows are y: a 2 x 2 checker of four sub sectors
        let mut g = grid(2, &["12", "34"], &[]);
        let mut out = vec![];
        link_walk(&mut g, 0, 2, 0, 2, &mut out);
        assert_eq!(
            out,
            vec![
                // (0, 1): 3 under 1, then its (x + 1, y - 1) diagonal 2 through the corners (1, 1) = 4 and (0, 0) = 1
                plain(3, 1),
                EeSubSectorLink { sub: 3, other: 2, add: true, first: 4, second: 1 },
                // (1, 0): 2 beside 1 (no row above, no column to the right)
                plain(2, 1),
                // (1, 1): 4 beside 3, under 2, diagonal to 1 through (0, 1) = 3 and (1, 0) = 2: both exist
                plain(4, 3),
                plain(4, 2),
                EeSubSectorLink { sub: 4, other: 1, add: true, first: 3, second: 2 },
            ]
        );
        // a missing corner sub sector makes the diagonal a plain neighbour again (a missing `other` is the
        // applier's business: the walk still lists the call)
        let mut g = grid(2, &["12", "34"], &[2]);
        let mut out = vec![];
        link_walk(&mut g, 0, 2, 0, 2, &mut out);
        assert_eq!(out[1], EeSubSectorLink { sub: 3, other: 2, add: true, first: 4, second: 1 });
        assert_eq!(out[5], plain(4, 1));
        assert_eq!(out.len(), 6);
    }

    #[test]
    fn a_clipped_walk_tests_its_own_end_not_the_maps() {
        // a 3 x 3 map; the walk covers x 0..2, y 0..3: the (x + 1, y - 1) diagonal of x = 1 needs x + 1 < 2, so
        // (2, 0) is never read for it
        let mut g = grid(3, &["123", "456", "789"], &[]);
        let mut out = vec![];
        link_walk(&mut g, 0, 2, 0, 3, &mut out);
        assert!(!g.reads.contains(&(2, 0)));
        assert!(!g.reads.contains(&(2, 1)));
        // (1, 1) = 5: beside 4, under 2, diagonal to 1 through 4 and 2
        assert!(out.contains(&EeSubSectorLink { sub: 5, other: 1, add: true, first: 4, second: 2 }));
        assert!(!out.iter().any(|l| l.sub == 5 && l.other == 3));
        // the full walk does read the (x + 1, y - 1) diagonal of x = 1: 5 to 3 through (2, 1) = 6 and (1, 0) = 2
        let mut g = grid(3, &["123", "456", "789"], &[]);
        let mut out = vec![];
        link_walk(&mut g, 0, 3, 0, 3, &mut out);
        assert!(out.contains(&EeSubSectorLink { sub: 5, other: 3, add: true, first: 6, second: 2 }));
        // a walk starting past 0 still looks left and up of its first column and row
        let mut g = grid(3, &["123", "456", "789"], &[]);
        let mut out = vec![];
        link_walk(&mut g, 1, 3, 1, 3, &mut out);
        assert_eq!(out[0], plain(5, 4));
        assert_eq!(out[1], plain(5, 2));
        assert_eq!(out[2], EeSubSectorLink { sub: 5, other: 1, add: true, first: 4, second: 2 });
    }

    #[test]
    fn same_ids_and_empty_tiles_make_no_links() {
        let mut g = grid(2, &["11", "10"], &[]);
        let mut out = vec![];
        link_walk(&mut g, 0, 2, 0, 2, &mut out);
        assert!(out.is_empty());
    }
}
