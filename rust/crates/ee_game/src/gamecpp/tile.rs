//! A map tile's game side in Rust (game core lane, round 9; was EETile.cpp): the building images' part of the
//! checksum, the load, the out-of-sync dump and the save, an image's removal, and the terrain type (deep and shallow
//! water from the tile's height and its corners). Each C++ call or member is a case of the lane B chain
//! (EEMiniMap.cpp's `EECore9OpB3`); the methods forward here (`ee_gamecpp_tile_*`).
//!
//! Simulation code (RUST_PLAN 2.1): the terrain type's comparisons are the C++'s.

#![allow(clippy::not_unsafe_ptr_arg_deref, clippy::missing_safety_doc)]

use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};


use super::{h, hd, hp, pa, ptr, O, P};

/// `kTerrainDeepWater`, `kTerrainShallowWater`, `kTerrainSpace` (static_assert'd in EEEffectHost.cpp and
/// EEMiniMap.cpp).
const TL_DEEP_WATER: c_long = 1;
const TL_SHALLOW_WATER: c_long = 10;
const TL_SPACE: c_long = 12;

/// The tile's building images.
fn images(tile: P) -> Vec<P> {
    let count = h(O::MmTileDoppCount, tile, &[]) as c_ulong;
    (0..count).map(|i| hp(O::MmTileDopp, tile, &[i as i64])).collect()
}

/// A building image's unit's id.
fn image_unit_id(d: P) -> c_ulong {
    // SAFETY: the unit's record
    unsafe { (*crate::gamecpp::records::un_rts(hp(O::MmDoppUnit, d, &[]))).mUnitID }
}

/// EETile::GetChecksum: the tile's base checksum plus the unit ids of its building images.
#[no_mangle]
pub extern "C" fn ee_gamecpp_tile_checksum(tile: *const c_void, players: c_ulong) -> c_long {
    let t = tile as P;
    let mut sum: c_long = 0;
    for d in images(t) {
        sum = sum.wrapping_add(image_unit_id(d) as c_long);
    }
    (h(O::TlRtsChecksum, t, &[players as i64]) as c_long).wrapping_add(sum)
}

/// EETile::Load: the tile and its building images from the saved game; the image count's read result.
#[no_mangle]
pub extern "C" fn ee_gamecpp_tile_load(
    tile: *mut c_void,
    major: c_ulong,
    minor: c_ulong,
    file: *mut c_void,
    players: c_ulong,
) -> c_long {
    let t = tile as P;
    let file_addr = file as usize as i64;
    h(O::TlRtsLoad, t, &[major as i64, minor as i64, file_addr, players as i64]);
    let mut count: c_ulong = 0;
    let result = h(O::MpFileReadUlong, file as P, &[ptr(&mut count)]) as c_long;
    if result == 0 {
        for _ in 0..count {
            let d = hp(O::TlLoadImage, 0, &[major as i64, minor as i64, file_addr]);
            if d != 0 {
                h(O::TlImageSetTile, d, &[pa(t)]);
            }
        }
    }
    result
}

/// EETile::Dump: the building images' unit ids, then the base state, to the out-of-sync dump.
#[no_mangle]
pub extern "C" fn ee_gamecpp_tile_dump(tile: *const c_void, file: *mut c_void, players: c_ulong) {
    let t = tile as P;
    for (i, d) in images(t).into_iter().enumerate() {
        h(O::TlDumpImage, file as P, &[i as i64, image_unit_id(d) as c_long as i64]);
    }
    h(O::TlRtsDump, t, &[file as usize as i64, players as i64]);
}

/// EETile::RemoveDoppelganger: the image removed from the tile, if it is there.
#[no_mangle]
pub extern "C" fn ee_gamecpp_tile_remove_image(tile: *mut c_void, image: *mut c_void) {
    let t = tile as P;
    if let Some(i) = images(t).iter().position(|&d| d == image as P) {
        h(O::TlEraseImage, t, &[i as i64]);
    }
}

/// EETile::Save: the tile and its valid building images that belong to it (or to none); the first error.
#[no_mangle]
pub extern "C" fn ee_gamecpp_tile_save(tile: *mut c_void, file: *mut c_void, players: c_ulong) -> c_long {
    let t = tile as P;
    let file_addr = file as usize as i64;
    let mut result = h(O::TlRtsSave, t, &[file_addr, players as i64]) as c_long;
    if result == 0 {
        let kept: Vec<P> = images(t)
            .into_iter()
            .filter(|&d| {
                h(O::TlImageValid, d, &[]) != 0
                    && (h(O::TlImageTileNull, d, &[]) != 0 || h(O::MmDoppOnTile, d, &[pa(t)]) != 0)
            })
            .collect();
        result = h(O::TlWriteUlong, file as P, &[kept.len() as c_ulong as i64]) as c_long;
        if result == 0 {
            let mut j = 0;
            while j < kept.len() && result == 0 {
                result = h(O::TlImageSave, kept[j], &[file_addr]) as c_long;
                j += 1;
            }
        }
    }
    result
}

/// EETile::SetTerrainType: the terrain type and the mesh's space flag; a tile below sea level becomes deep water when
/// deep, else shallow water unless three of its corners are dry.
#[no_mangle]
pub extern "C" fn ee_gamecpp_tile_set_terrain_type(
    tile: *mut c_void,
    kind: c_long,
    x: c_long,
    y: c_long,
    map: *mut c_void,
) {
    let t = tile as P;
    let m = hp(O::TlDynMap, map as P, &[]);
    let mut kind = kind;
    h(O::TlSetIsSpace, m, &[x as i64, y as i64, (kind == TL_SPACE) as i64]);
    let height = hd(O::TlHeight, t, &[], &[]);
    if height < 0.0 {
        if height < -(hd(O::TlMapTileSize, 0, &[], &[]) * 0.75) {
            kind = TL_DEEP_WATER;
        } else {
            let mut count: c_long = 0;
            for (cx, cy) in [(x, y), (x + 1, y), (x, y + 1), (x + 1, y + 1)] {
                if h(O::TlValidPoint, m, &[cx as i64, cy as i64]) != 0
                    && hd(O::TlMeshZ, m, &[cx as i64, cy as i64], &[]) >= 0.0
                {
                    count += 1;
                }
            }
            if count < 3 {
                kind = TL_SHALLOW_WATER;
            }
        }
    }
    h(O::TlSetTerrainTypeField, t, &[kind as i64]);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_terrain_kinds_are_the_tables() {
        assert_eq!((TL_DEEP_WATER, TL_SHALLOW_WATER, TL_SPACE), (1, 10, 12));
    }
}
