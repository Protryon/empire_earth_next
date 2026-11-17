//! The map's plain members as Rust records the C++ classes derive from (R5b.4, the twenty-eighth round): `RTSMap`'s
//! block `mF1955F4` (+0x1955f4 in the original) .. `mB195620` and `EEMap`'s `mMapCharacteristics` (+0x19564c) ..
//! `mChecksumCursor` are [`EeUnitsRtsMapFields`] and [`EeUnitsMapFields`] (ee/EEMap.h; names, C++ types and order
//! kept). Their Load and Save are ee_sim::units::map_save's.

#![allow(non_snake_case)]

use std::os::raw::{c_long, c_ulong};

/// `RTSMap`'s plain members (ee/EEMap.h), in the class's order.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsRtsMapFields {
    /// the constructor's tile size (copied to `gMapTileSize`)
    pub mF1955F4: f32,
    pub mF1955F8: f32,
    pub mF1955FC: f32,
    pub mL195600: c_long,
    pub mL195604: c_long,
    pub mL195608: c_long,
    pub mL19560C: c_long,
    /// saved: `Create`'s height
    pub mL195610: c_long,
    /// saved: `Create`'s width
    pub mL195614: c_long,
    /// tiles per side
    pub mSize: c_long,
    /// saved: the player count
    pub mU19561C: c_ulong,
    /// the tiles exist
    pub mB195620: bool,
}

/// `EEMap`'s plain members (ee/EEMap.h), in the class's order.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsMapFields {
    /// saved
    pub mMapCharacteristics: c_ulong,
    /// saved: the base terrain
    pub mU195650: c_ulong,
    /// the next free entry of the tile array
    pub mNextTile: c_ulong,
    pub mTileArrayCount: c_ulong,
    /// saved: the cliff terrain
    pub mU19565C: c_ulong,
    /// `ComputeChecksum` checks 64 x 64 tiles from here
    pub mChecksumCursor: c_ulong,
}
