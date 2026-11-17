//! The continent manager's queries over its analysis, line by line: which continent chain `AreConnected` (0x422872)
//! compares for a set of crossing flags, the adjacency row walks of `GetConnectedIDs` (0x421e22) and
//! `GetAdjacentIDs` (0x421e80), and `GetLargestContinent`'s pick (0x422952).
//!
//! Part of the deterministic game (RUST_PLAN 2.1): no arithmetic, the C++'s loop orders (from the last index down)
//! and its ties (the first largest from the top).

/// `kAIChainLandCliff` .. `kAIChainLandCliffSpace` (ee/EEAIKindIDs.h).
pub const CHAIN_LAND_CLIFF: i32 = 1;
pub const CHAIN_LAND_WATER: i32 = 2;
pub const CHAIN_LAND_WATER_CLIFF: i32 = 3;
pub const CHAIN_LAND_SPACE: i32 = 4;
pub const CHAIN_LAND_WATER_SPACE: i32 = 5;
pub const CHAIN_LAND_CLIFF_SPACE: i32 = 6;

/// 0x422872: the chain two different continents must share for crossing `flags` (1 cliff, 2 water, 8 space), -1 for
/// none. (The C++ comments the two swapped chain kinds of the cliff + space and water + space cases; kept.)
pub fn connected_chain(flags: i32) -> i32 {
    if flags == 0 {
        -1
    } else if flags & 1 != 0 {
        if flags & 2 != 0 {
            CHAIN_LAND_WATER_CLIFF
        } else if flags & 8 != 0 {
            CHAIN_LAND_CLIFF_SPACE
        } else {
            CHAIN_LAND_CLIFF
        }
    } else if flags & 2 != 0 {
        if flags & 8 != 0 {
            CHAIN_LAND_WATER_SPACE
        } else {
            CHAIN_LAND_WATER
        }
    } else if flags & 8 != 0 {
        CHAIN_LAND_SPACE
    } else {
        -1
    }
}

/// 0x421e22 / 0x421e80: the continents adjacent to continent `id` from its row of the adjacency matrix, from the
/// last down, never 0; `with_self` keeps `id` itself (`GetConnectedIDs`), else it is left out (`GetAdjacentIDs`).
/// An `id` past the matrix gives none.
pub fn adjacent_ids(flags: &[bool], width: usize, id: u16, with_self: bool) -> Vec<u16> {
    let id_index = id as usize;
    if id_index >= width {
        return Vec::new();
    }
    let row = &flags[id_index * width..id_index * width + width];
    (1..width).rev().filter(|&i| row[i] && (with_self || i != id_index)).map(|i| i as u16).collect()
}

/// A continent as `GetLargestContinent` reads it.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeContinentSize {
    pub present: bool,
    pub category: u8,
    /// `mTiles.Size()` (a `long`)
    pub size: i32,
}

/// 0x422952: the index of the largest continent of category `kind` (`(char)inKind`; 0 any), -1 if none. From the
/// last down, a later one replaces the best only when strictly larger.
pub fn largest_continent(continents: &[EeContinentSize], kind: i8) -> i32 {
    let mut best: i32 = -1;
    for (i, c) in continents.iter().enumerate().rev() {
        if !c.present {
            continue;
        }
        // `mCategory == (char)inKind`: the byte widened unsigned, the char signed
        let fits = kind == 0 || c.category as i32 == kind as i32;
        if best == -1 {
            if fits {
                best = i as i32;
            }
        } else if fits && continents[best as usize].size < c.size {
            best = i as i32;
        }
    }
    best
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn chains() {
        assert_eq!(connected_chain(0), -1);
        assert_eq!(connected_chain(1), CHAIN_LAND_CLIFF);
        assert_eq!(connected_chain(1 | 2), CHAIN_LAND_WATER_CLIFF);
        assert_eq!(connected_chain(1 | 2 | 8), CHAIN_LAND_WATER_CLIFF);
        assert_eq!(connected_chain(1 | 8), CHAIN_LAND_CLIFF_SPACE);
        assert_eq!(connected_chain(2), CHAIN_LAND_WATER);
        assert_eq!(connected_chain(2 | 8), CHAIN_LAND_WATER_SPACE);
        assert_eq!(connected_chain(8), CHAIN_LAND_SPACE);
        assert_eq!(connected_chain(4), -1);
        assert_eq!(connected_chain(16 | 4), -1);
    }

    #[test]
    fn rows() {
        // 4 continents: 1 touches 0, 1, 2 and 3; 2 touches 1
        let mut flags = vec![false; 16];
        for i in 0..4 {
            flags[4 + i] = true;
        }
        flags[2 * 4 + 1] = true;
        assert_eq!(adjacent_ids(&flags, 4, 1, true), vec![3, 2, 1]);
        assert_eq!(adjacent_ids(&flags, 4, 1, false), vec![3, 2]);
        assert_eq!(adjacent_ids(&flags, 4, 2, false), vec![1]);
        assert_eq!(adjacent_ids(&flags, 4, 0, true), Vec::<u16>::new());
        assert_eq!(adjacent_ids(&flags, 4, 4, true), Vec::<u16>::new());
    }

    #[test]
    fn largest() {
        let c = |category: u8, size: i32| EeContinentSize { present: true, category, size };
        let list = [EeContinentSize::default(), c(1, 10), c(2, 50), c(1, 10), c(1, 4)];
        // ties keep the higher index (found first from the top)
        assert_eq!(largest_continent(&list, 1), 3);
        assert_eq!(largest_continent(&list, 0), 2);
        assert_eq!(largest_continent(&list, 2), 2);
        assert_eq!(largest_continent(&list, 4), -1);
        assert_eq!(largest_continent(&[], 0), -1);
        assert_eq!(largest_continent(&[c(200, 1)], -56), -1);
    }
}
