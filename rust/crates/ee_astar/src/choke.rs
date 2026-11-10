//! The choke point pass's grids (`EEAIBuildChokePoints`, `EEAIChokePoint.cpp` 0x1c949, up to its flood fills: the
//! classification `EEAIChokeGetTileSerial` 0x1ccd0, the passes loop with `EEAIChokeGetKinds` 0x1ca7c, and the
//! smoothing `EEAIChokeSmooth` 0x1c78c), line by line. The
//! continent manager's choke point pass (`EEAIBuildChokePoints`) classifies every tile (`KIND_AREA` for a tile of a
//! water, space or edge continent or of a big forest, else 0), then smooths that grid `passes - 1` times: each pass
//! averages every non-area inner tile's kind over its 3x3 neighbourhood from the previous pass's grid, spreads a
//! neighbour's area serial into it, and flags the tiles that touch two different areas (or a flagged tile), so that
//! the flagged runs between areas become the choke points.
//!
//! Part of the deterministic game (RUST_PLAN 2.1): integer arithmetic only, the C++'s loop order, and the serial it
//! spreads is the last non-zero-kind neighbour's in that order, zero or not (the C++ keeps that variable outside the
//! tile loops, but only uses it when a non-zero kind was read, so nothing carries over).

/// A tile of an area of its own (`0x36`).
pub const KIND_AREA: i32 = 0x36;

/// 0x1c78c `EEAIChokeSmooth`: one pass over the inner tiles (`1..width-1` x `1..height-1`; the index is
/// `x * width + y`). `prev` is the previous pass's kinds (`inKinds`), `kinds` the fresh kinds, averaged in place
/// (`ioKinds`); `serials` and `flags` are the pass's per-tile area serials and border flags (the C++ globals). `first`
/// is the first pass, which averages area tiles in; the later passes skip them.
///
/// The flags a tile tests are the flags before this pass; the new ones replace them at the end.
pub fn smooth(
    prev: &[i32],
    kinds: &mut [i32],
    serials: &mut [u32],
    flags: &mut [u8],
    width: i32,
    height: i32,
    first: bool,
) {
    let size = (width * height) as usize;
    assert!(prev.len() >= size && kinds.len() >= size && serials.len() >= size && flags.len() >= size);
    // `long theSerial = 0;` outside the loops: the last non-zero-kind neighbour's serial
    let mut serial: u32 = 0;
    let mut new_flags = flags.to_vec();
    for i in 1..width - 1 {
        for j in 1..height - 1 {
            let mut border = false;
            let index = (i * width + j) as usize;
            if kinds[index] == KIND_AREA {
                continue;
            }
            let mut sum: i32 = 0;
            let mut count: i32 = 0;
            for x in i - 1..=i + 1 {
                for y in j - 1..=j + 1 {
                    let other = (x * width + y) as usize;
                    let kind = prev[other];
                    if kind == KIND_AREA && !first {
                        continue;
                    }
                    sum = sum.wrapping_add(kind);
                    count += 1;
                    // `(theSerial = serials[other]) != 0 && ...`: every non-zero kind assigns it, a zero serial too
                    if kind != 0 {
                        serial = serials[other];
                        if serial != 0 && serials[index] != 0 && (flags[other] != 0 || serial != serials[index]) {
                            border = true;
                        }
                    }
                }
            }
            if border {
                new_flags[index] = 1;
            }
            if sum != 0 && serials[index] == 0 {
                serials[index] = serial;
            }
            kinds[index] = if count != 0 { sum / count } else { 0 };
        }
    }
    flags[..size].copy_from_slice(&new_flags[..size]);
}

/// A tile as the choke point pass's classification reads it (`EEAIChokeGetTileSerial`): whether the map has the
/// tile (outside the playable rhombus it has none), its continent's category
/// and serial, whether it carries a wood resource (`EEAIHasResourceUnit`), and the forest the manager lists at it
/// (`GetForestAt`, then the manager's entry at that id: `found` when that entry exists) with its serial and tile count.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeChokeTileInfo {
    pub tile: bool,
    pub category: u8,
    pub continent_serial: u32,
    pub resource: bool,
    pub forest: bool,
    pub forest_serial: u32,
    pub forest_tiles: i32,
}

/// `EEAIContinentCategory`'s land and cliff.
const CATEGORY_LAND: u8 = 1;
const CATEGORY_CLIFF: u8 = 3;

/// 0x41ccd0 `EEAIChokeGetTileSerial`: whether a tile belongs to a fixed area, and its serial: a water, space or edge
/// continent's; on land or cliff with a resource, the forest's over 6 tiles (0 for a smaller one), or the
/// continent's outside a listed forest. Where the map has no tile: an area with serial -1 (the C++'s `long` -1).
pub fn tile_serial(info: &EeChokeTileInfo) -> Option<u32> {
    if !info.tile {
        Some(u32::MAX)
    } else if info.category != CATEGORY_LAND && info.category != CATEGORY_CLIFF {
        Some(info.continent_serial)
    } else if info.resource {
        if info.forest {
            Some(if info.forest_tiles > 6 { info.forest_serial } else { 0 })
        } else {
            Some(info.continent_serial)
        }
    } else {
        None
    }
}

/// 0x41c949 `EEAIBuildChokePoints` up to the flood fills: classifies every tile of a `size` x `size` map
/// (`infos` by `x * size + y`) into `kinds` (`KIND_AREA` for a tile of an area of its own, else 0) and `serials`,
/// then smooths a copy of the kinds `passes - 1` times from the fresh classification each time (the C++ re-read the
/// base grid every pass, averaging into it from the previous pass's result), which spreads the serials and sets the
/// `flags` of the tiles between two areas. The smoothed kinds themselves are thrown away, as the C++ did.
pub fn build_grids(
    size: i32,
    passes: i32,
    infos: &[EeChokeTileInfo],
    kinds: &mut [i32],
    serials: &mut [u32],
    flags: &mut [u8],
) {
    let n = (size * size) as usize;
    assert!(infos.len() >= n && kinds.len() >= n && serials.len() >= n && flags.len() >= n);
    for i in 0..n {
        match tile_serial(&infos[i]) {
            Some(serial) => {
                serials[i] = serial;
                kinds[i] = KIND_AREA;
            }
            None => {
                serials[i] = 0;
                kinds[i] = 0;
            }
        }
    }
    flags[..n].fill(0);
    let mut smoothed = kinds[..n].to_vec();
    let last = passes - 1;
    let mut left = last;
    while left > 0 {
        let mut fresh = kinds[..n].to_vec();
        smooth(&smoothed, &mut fresh, serials, flags, size, size, left == last);
        smoothed = fresh;
        left -= 1;
    }
}

#[cfg(test)]
mod build_tests {
    use super::*;

    fn area(serial: u32) -> EeChokeTileInfo {
        EeChokeTileInfo { tile: true, category: 2, continent_serial: serial, ..Default::default() }
    }

    fn land() -> EeChokeTileInfo {
        EeChokeTileInfo { tile: true, category: 1, continent_serial: 3, ..Default::default() }
    }

    #[test]
    fn a_tiles_serial_is_its_continents_or_its_big_forests() {
        assert_eq!(tile_serial(&area(5)), Some(5));
        assert_eq!(tile_serial(&EeChokeTileInfo { category: 5, continent_serial: 9, ..area(0) }), Some(9));
        assert_eq!(tile_serial(&EeChokeTileInfo::default()), Some(u32::MAX));
        assert_eq!(tile_serial(&land()), None);
        assert_eq!(tile_serial(&EeChokeTileInfo { category: 3, ..land() }), None);
        let wood = EeChokeTileInfo { resource: true, ..land() };
        assert_eq!(tile_serial(&wood), Some(3));
        let small = EeChokeTileInfo { forest: true, forest_serial: 8, forest_tiles: 6, ..wood };
        assert_eq!(tile_serial(&small), Some(0));
        let big = EeChokeTileInfo { forest_tiles: 7, ..small };
        assert_eq!(tile_serial(&big), Some(8));
        // a forest listed at a tile of a cliff continent counts the same way
        assert_eq!(tile_serial(&EeChokeTileInfo { category: 3, ..big }), Some(8));
    }

    #[test]
    fn the_grids_classify_then_smooth_from_the_fresh_kinds_each_pass() {
        // a 6 x 6 map: column x = 1 is area A (serial 7), column x = 4 is area B (serial 9), the rest land
        let size = 6;
        let mut infos = vec![land(); 36];
        for y in 0..6 {
            infos[(size + y) as usize] = area(7);
            infos[(size * 4 + y) as usize] = area(9);
        }
        let mut kinds = vec![-1; 36];
        let mut serials = vec![5u32; 36];
        let mut flags = vec![2u8; 36];
        // one pass: the classification alone, nothing smoothed, the flags cleared
        build_grids(size, 1, &infos, &mut kinds, &mut serials, &mut flags);
        assert_eq!(kinds[(size + 2) as usize], KIND_AREA);
        assert_eq!(serials[(size + 2) as usize], 7);
        assert_eq!(kinds[(size * 2 + 2) as usize], 0);
        assert_eq!(serials[(size * 2 + 2) as usize], 0);
        assert!(flags.iter().all(|&f| f == 0));
        // three passes (two smoothings): the tiles between the areas took a serial and the second smoothing flagged
        // them; the base kinds are untouched
        build_grids(size, 3, &infos, &mut kinds, &mut serials, &mut flags);
        assert_eq!(kinds[(size * 2 + 2) as usize], 0);
        assert_eq!(serials[(size * 2 + 2) as usize], 7);
        assert_eq!(serials[(size * 3 + 2) as usize], 9);
        assert_eq!(flags[(size * 2 + 2) as usize], 1);
        assert_eq!(flags[(size * 3 + 2) as usize], 1);
        assert_eq!(flags[(size + 2) as usize], 0);
        // the same as driving `smooth` by hand over the C++'s copies
        let mut base = vec![0; 36];
        let mut s2 = vec![0u32; 36];
        let mut f2 = vec![0u8; 36];
        for (i, info) in infos.iter().enumerate() {
            if let Some(s) = tile_serial(info) {
                base[i] = KIND_AREA;
                s2[i] = s;
            }
        }
        let mut smoothed = base.clone();
        for first in [true, false] {
            let mut fresh = base.clone();
            smooth(&smoothed, &mut fresh, &mut s2, &mut f2, size, size, first);
            smoothed = fresh;
        }
        assert_eq!(serials, s2);
        assert_eq!(flags, f2);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn grid(width: i32, cells: &[(i32, i32, i32)]) -> Vec<i32> {
        let mut g = vec![0; (width * width) as usize];
        for &(x, y, v) in cells {
            g[(x * width + y) as usize] = v;
        }
        g
    }

    #[test]
    fn a_tile_between_two_areas_is_flagged_and_takes_a_serial() {
        // a 6 x 6 map: column x = 1 is area A (serial 7), column x = 4 is area B (serial 9); x = 2 and 3 lie between
        let w = 6;
        let mut kinds = vec![0; 36];
        let mut serials = vec![0u32; 36];
        for y in 0..6 {
            kinds[(w + y) as usize] = KIND_AREA;
            kinds[(w * 4 + y) as usize] = KIND_AREA;
            serials[(w + y) as usize] = 7;
            serials[(w * 4 + y) as usize] = 9;
        }
        let prev = kinds.clone();
        let mut flags = vec![0u8; 36];
        // the first pass: the area tiles count into the average; a tile next to A takes A's serial, one next to B
        // takes B's (the last non-zero serial read in the 3x3 walk, x then y); nothing is a border yet (no tile
        // between them had a serial)
        smooth(&prev, &mut kinds, &mut serials, &mut flags, w, w, true);
        let a = (w * 2 + 2) as usize;
        let b = (w * 3 + 2) as usize;
        assert_eq!(kinds[a], (3 * KIND_AREA) / 9); // three area neighbours of nine
        assert_eq!(serials[a], 7);
        assert_eq!(serials[b], 9);
        assert_eq!(flags[a], 0);
        assert_eq!(flags[b], 0);
        // the area columns are untouched; the outer ring is never visited
        assert_eq!(kinds[(w + 2) as usize], KIND_AREA);
        assert_eq!(kinds[0], 0);
        assert_eq!(serials[(w * 5 + 2) as usize], 0);
        // the second pass skips the area tiles: the two middle tiles see each other's different serials: borders
        let prev = kinds.clone();
        smooth(&prev, &mut kinds, &mut serials, &mut flags, w, w, false);
        assert_eq!(flags[a], 1);
        assert_eq!(flags[b], 1);
        assert_eq!(serials[a], 7);
        // the average now runs over the six non-area neighbours, all at 18 from the first pass
        assert_eq!(kinds[a], 18);
        // a gap one tile wide is never flagged: every tile of it reads the same, last, serial
        let w = 5;
        let mut kinds = vec![0; 25];
        let mut serials = vec![0u32; 25];
        for y in 0..5 {
            kinds[(w + y) as usize] = KIND_AREA;
            kinds[(w * 3 + y) as usize] = KIND_AREA;
            serials[(w + y) as usize] = 7;
            serials[(w * 3 + y) as usize] = 9;
        }
        let mut flags = vec![0u8; 25];
        for first in [true, false, false] {
            let prev = kinds.clone();
            smooth(&prev, &mut kinds, &mut serials, &mut flags, w, w, first);
        }
        assert_eq!(serials[(w * 2 + 2) as usize], 9);
        assert!(flags.iter().all(|&f| f == 0));
    }

    #[test]
    fn a_tile_next_to_one_area_only_is_no_border_and_keeps_its_serial() {
        let w = 4;
        let mut kinds = grid(w, &[(1, 1, KIND_AREA), (1, 2, KIND_AREA)]);
        let prev = kinds.clone();
        let mut serials = vec![0u32; 16];
        serials[(w + 1) as usize] = 5;
        serials[(w + 2) as usize] = 5;
        let mut flags = vec![0u8; 16];
        smooth(&prev, &mut kinds, &mut serials, &mut flags, w, w, true);
        let t = (w * 2 + 1) as usize;
        assert_eq!(serials[t], 5);
        assert_eq!(flags[t], 0);
        assert_eq!(kinds[t], (2 * KIND_AREA) / 9);
        // a second pass sees the same serial everywhere: still no border, the serial stays
        let prev = kinds.clone();
        smooth(&prev, &mut kinds, &mut serials, &mut flags, w, w, false);
        assert_eq!(serials[t], 5);
        assert_eq!(flags[t], 0);
    }

    #[test]
    fn the_stored_serial_is_the_last_non_zero_kind_neighbours_zero_included() {
        // (2, 2) sees the area tile (1, 1) with serial 42 and a kind-10 tile (3, 3) with no serial: the walk (x,
        // then y) reads (3, 3) last, so the serial stored is 0, as the C++'s `(theSerial = ...) != 0` leaves it
        let w = 6;
        let mut kinds = grid(w, &[(1, 1, KIND_AREA), (3, 3, 10)]);
        let prev = kinds.clone();
        let mut serials = vec![0u32; 36];
        serials[(w + 1) as usize] = 42;
        let mut flags = vec![0u8; 36];
        smooth(&prev, &mut kinds, &mut serials, &mut flags, w, w, true);
        assert_eq!(serials[(w * 2 + 2) as usize], 0);
        assert_eq!(kinds[(w * 2 + 2) as usize], (KIND_AREA + 10) / 9);
        // (2, 1) reads (1, 1) last among its non-zero kinds: serial 42
        assert_eq!(serials[(w * 2 + 1) as usize], 42);
        // (4, 4) has only the kind-10 tile around it, serial 0: a sum without a serial stores 0
        assert_eq!(serials[(w * 4 + 4) as usize], 0);
        assert_eq!(kinds[(w * 4 + 4) as usize], 10 / 9);
        // a tile with no kinds around it stays at 0
        assert_eq!(serials[(w * 4 + 1) as usize], 0);
        assert_eq!(kinds[(w * 4 + 1) as usize], 0);
    }

    #[test]
    fn a_flagged_neighbour_flags_a_tile_with_a_serial() {
        let w = 4;
        let mut kinds = grid(w, &[(1, 1, 10), (2, 2, 10)]);
        let prev = kinds.clone();
        let mut serials = vec![0u32; 16];
        serials[(w + 1) as usize] = 3;
        serials[(w * 2 + 2) as usize] = 3;
        let mut flags = vec![0u8; 16];
        flags[(w + 1) as usize] = 1;
        smooth(&prev, &mut kinds, &mut serials, &mut flags, w, w, true);
        // (2, 2) has a serial and its neighbour (1, 1) is flagged: a border, read from the flags before the pass
        assert_eq!(flags[(w * 2 + 2) as usize], 1);
        // (1, 1) itself: its neighbour (2, 2) was not flagged before the pass and has the same serial
        assert_eq!(flags[(w + 1) as usize], 1); // kept from before
        assert_eq!(flags[(w + 2) as usize], 0); // no serial of its own before the pass: never a border
        assert_eq!(serials[(w + 2) as usize], 3);
    }
}
