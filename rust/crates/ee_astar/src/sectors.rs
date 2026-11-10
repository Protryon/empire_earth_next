//! The continent manager's path sector grid (`EEAIContinentManager::CreatePathSectors`'s sizes and sector
//! rectangles, 0x420b48; `EEAIPathSectorRegionInformation::Init`, 0x42bb93; `GetSectorEnd`, 0x4263cb) and the choke
//! point pass's size (`Build`, 0x41f86b), line by line: the sector and region sizes by the map's size class, the
//! sectors per side (a float division rounded up as `UMCeil` does), and each sector's tile rectangle clipped to the
//! map.
//!
//! Part of the deterministic game (RUST_PLAN 2.1): the one float operation (`(float)size / sectorSize`) in `f32`
//! and rounded as `lrintf` does.

/// The sector and region sizes of a map (`CreatePathSectors`: `theSectorSize`, `gEEAIRegionSize`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeSectorSizes {
    pub sector_size: i32,
    pub region_size: i32,
}

/// A sector's tile rectangle and its grid position (`EEAIPathSector::mBounds`, `mCenter`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeSectorRect {
    pub top: i32,
    pub left: i32,
    pub bottom: i32,
    pub right: i32,
    pub center_x: i32,
    pub center_y: i32,
}

/// 0x420b48: the sector and region sizes by the map's size class (`EEMap::mL195610`): the sector size stays 7
/// past the largest class, as the C++'s initial value.
pub fn sector_sizes(size_class: i32) -> EeSectorSizes {
    let (sector_size, region_size) = if size_class <= 0x2a {
        (7, 8)
    } else if size_class <= 0x3a {
        (10, 10)
    } else if size_class <= 0x4b {
        (0xc, 0xc)
    } else if size_class <= 0x69 {
        (0xf, 0xe)
    } else if size_class <= 0xa0 {
        (0x13, 0x10)
    } else if size_class <= 0xdc {
        (0x15, 0x12)
    } else {
        (7, 0xe)
    };
    EeSectorSizes { sector_size, region_size }
}

/// 0x41f86b `Build`: the choke point pass's number of passes by the map's size class (4 on small maps up to 7 on
/// huge ones; the C++'s `<= 0x4b || <= 0x69` is one class).
pub fn choke_passes(size_class: i32) -> i32 {
    if size_class <= 0x2a {
        4
    } else if size_class <= 0x3a {
        5
    } else if size_class <= 0x69 {
        6
    } else {
        7
    }
}

/// `lrintf` on x86-64: round half to even; the conversion's "indefinite" value out of range.
fn lrintf(value: f32) -> i32 {
    let rounded = value.round_ties_even();
    if (-2_147_483_648.0..2_147_483_648.0).contains(&rounded) {
        rounded as i32
    } else {
        i32::MIN
    }
}

/// `UMCeil`: rounds to nearest, then steps up when that went down.
fn um_ceil(value: f32) -> i32 {
    let result = lrintf(value);
    if (result as f32) < value {
        result.wrapping_add(1)
    } else {
        result
    }
}

/// 0x42bb93 `EEAIPathSectorRegionInformation::Init`: the sectors per side, `UMCeil((float)size / sectorSize)`.
pub fn sectors_per_side(map_size: i32, sector_size: i32) -> i32 {
    um_ceil(map_size as f32 / sector_size as f32)
}

/// 0x420b48: the tile rectangle of sector `(x, y)` of `sector_size` tiles on a `map_size` map: from
/// `(x, y) * sector_size`, to the sector's last tile (`GetSectorEnd`) or the map's, whichever comes first (the
/// C++'s `bottom <= end.x` compares the map's last row with the sector's last column); the centre is the grid
/// position.
pub fn sector_rect(x: i32, y: i32, sector_size: i32, map_size: i32) -> EeSectorRect {
    let mut top = sector_size * y;
    let mut left = sector_size * x;
    if left < 0 {
        left = 0;
    }
    if top < 0 {
        top = 0;
    }
    let end_x = (x + 1) * sector_size - 1;
    let end_y = (y + 1) * sector_size - 1;
    let mut bottom = map_size - 1;
    let mut right = end_x;
    if bottom <= end_x {
        right = bottom;
    }
    if end_y < bottom {
        bottom = end_y;
    }
    EeSectorRect { top, left, bottom, right, center_x: x, center_y: y }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_sizes_follow_the_map_classes() {
        assert_eq!(sector_sizes(0x2a), EeSectorSizes { sector_size: 7, region_size: 8 });
        assert_eq!(sector_sizes(0x2b), EeSectorSizes { sector_size: 10, region_size: 10 });
        assert_eq!(sector_sizes(0x4b), EeSectorSizes { sector_size: 12, region_size: 12 });
        assert_eq!(sector_sizes(0x69), EeSectorSizes { sector_size: 15, region_size: 14 });
        assert_eq!(sector_sizes(0xa0), EeSectorSizes { sector_size: 19, region_size: 16 });
        assert_eq!(sector_sizes(0xdc), EeSectorSizes { sector_size: 21, region_size: 18 });
        assert_eq!(sector_sizes(0xdd), EeSectorSizes { sector_size: 7, region_size: 14 });
        assert_eq!(choke_passes(0x2a), 4);
        assert_eq!(choke_passes(0x3a), 5);
        assert_eq!(choke_passes(0x4b), 6);
        assert_eq!(choke_passes(0x69), 6);
        assert_eq!(choke_passes(0x6a), 7);
    }

    #[test]
    fn the_sectors_per_side_round_the_float_quotient_up() {
        assert_eq!(sectors_per_side(100, 10), 10);
        assert_eq!(sectors_per_side(101, 10), 11);
        assert_eq!(sectors_per_side(105, 10), 11);
        assert_eq!(sectors_per_side(104, 10), 11);
        assert_eq!(sectors_per_side(7, 7), 1);
        assert_eq!(sectors_per_side(0, 7), 0);
        // lrintf rounds half to even, then the ceiling steps up whenever the rounded value fell short
        assert_eq!(um_ceil(2.5), 3);
        assert_eq!(um_ceil(3.5), 4);
        assert_eq!(um_ceil(3.0), 3);
        assert_eq!(um_ceil(-2.5), -2);
    }

    #[test]
    fn a_sector_rectangle_is_clipped_to_the_map_the_cpp_way() {
        assert_eq!(
            sector_rect(0, 0, 7, 30),
            EeSectorRect { top: 0, left: 0, bottom: 6, right: 6, center_x: 0, center_y: 0 }
        );
        assert_eq!(
            sector_rect(1, 2, 7, 30),
            EeSectorRect { top: 14, left: 7, bottom: 20, right: 13, center_x: 1, center_y: 2 }
        );
        // the last sector of a 30-tile map with 7-tile sectors (5 per side): clipped at 29
        assert_eq!(
            sector_rect(4, 4, 7, 30),
            EeSectorRect { top: 28, left: 28, bottom: 29, right: 29, center_x: 4, center_y: 4 }
        );
        // the C++'s clip of the right edge compares the map's last row with the sector's last column, so a sector
        // whose last column is the map's last row gets that row as its right edge
        assert_eq!(
            sector_rect(4, 0, 7, 34),
            EeSectorRect { top: 0, left: 28, bottom: 6, right: 33, center_x: 4, center_y: 0 }
        );
        assert_eq!(
            sector_rect(4, 0, 7, 35),
            EeSectorRect { top: 0, left: 28, bottom: 6, right: 34, center_x: 4, center_y: 0 }
        );
    }
}
