//! `EEComplexDoppelganger`'s bodies (the units' lane, round 7; were EEComplexDoppelganger.cpp): `Load`, `Save`, the
//! two `Initialize`s, and the loaded contacts' restore (`RestoreLoadedContacts`, the first tick's fix-up). Every C++
//! statement of the originals is one operation of [`DopHost::dop`] (the game's: `ee_units_dop_host`, after
//! dop_repair.rs's and dop_load.rs's), asked in the original's order; the version branches and the early returns
//! are here. `dop` is the `EEComplexDoppelganger*` (the contacts' restore: none), `a` the file or an object, `b`,
//! `c` values.

use crate::units::dop_repair::DopHost;
use crate::units::obj_fields::EeUnitsComplexDopFields;
use std::os::raw::{c_long, c_ulong};

// ---- the statements (`op`; the answer)
/// `EEDoppelganger::Load(b, c, *(FSFile*)a)`
pub const EE_UNITS_DOP_BASE_LOAD: i32 = 300;
/// `mTransformation.Load(*(FSFile*)a)`
pub const EE_UNITS_DOP_LOAD_TRANSFORMATION: i32 = 301;
/// `((FSFile*)a)->Read(the scratch model id)`
pub const EE_UNITS_DOP_READ_MODEL_ID: i32 = 302;
/// `mModel = reinterpret_cast<EEModel*>(the scratch model id)`
pub const EE_UNITS_DOP_SET_MODEL_FROM_ID: i32 = 303;
/// `((FSFile*)a)->Read(the member b)` (`EE_UNITS_DOP_FIELD_*`)
pub const EE_UNITS_DOP_READ_FIELD: i32 = 304;
/// `EEDoppelganger::Save(b, c, *(FSFile*)a)`
pub const EE_UNITS_DOP_BASE_SAVE: i32 = 305;
/// `mTransformation.Save(*(FSFile*)a)`
pub const EE_UNITS_DOP_SAVE_TRANSFORMATION: i32 = 306;
/// `((FSFile*)a)->Write((long)mModel->mMediaIndex)`
pub const EE_UNITS_DOP_WRITE_MODEL_ID: i32 = 307;
/// `((FSFile*)a)->Write(the member b)` (`EE_UNITS_DOP_FIELD_*`)
pub const EE_UNITS_DOP_WRITE_FIELD: i32 = 308;
// Initialize's (`a` the unit seen, `b` the player, `c` the tile)
/// `EEDoppelganger::Initialize(unit a, (EEPlayer*)b, (RTSTile*)c)`
pub const EE_UNITS_DOP_BASE_INITIALIZE: i32 = 309;
/// unit `a`'s `mUnitClass->mCategory == kUnitCategoryResource`
pub const EE_UNITS_DOP_SEEN_IS_RESOURCE: i32 = 310;
/// `mPackedHitPoints = unit a's mResourceAmount | 0x80000000`
pub const EE_UNITS_DOP_PACK_RESOURCE: i32 = 311;
/// `mPackedHitPoints = unit a's mHitPoints`
pub const EE_UNITS_DOP_PACK_HIT_POINTS: i32 = 312;
/// unit `a`'s `mComplexUnit`
pub const EE_UNITS_DOP_SEEN_COMPLEX: i32 = 313;
/// complex unit `a`'s `IsFoundation() == true`
pub const EE_UNITS_DOP_SEEN_IS_FOUNDATION: i32 = 314;
/// `mModel = EEGetPlayerClassGraphic(complex unit a's GetPlayer(), unit b's mUnitClass)`
pub const EE_UNITS_DOP_MODEL_FOUNDATION: i32 = 315;
/// `mModel = unit a's GetModel()`
pub const EE_UNITS_DOP_MODEL_OF: i32 = 316;
/// `mFrame = complex unit a's mPhysicalModel.GetFrame()`
pub const EE_UNITS_DOP_FRAME_OF: i32 = 317;
/// `mAnimation = complex unit a's mAnimation`
pub const EE_UNITS_DOP_ANIMATION_OF: i32 = 318;
/// complex unit `a`'s `mIsBuilt`
pub const EE_UNITS_DOP_SEEN_IS_BUILT: i32 = 319;
/// `mPackedHitPoints |= 0x80000000`
pub const EE_UNITS_DOP_MARK_BUILT: i32 = 320;
/// `mTransformation = complex unit a's mPhysicalModel.GetLocation()`
pub const EE_UNITS_DOP_TRANSFORMATION_OF: i32 = 321;
/// `mScale = complex unit a's mPhysicalModel.GetScale()`
pub const EE_UNITS_DOP_SCALE_OF: i32 = 322;
/// `mIsFoundation = complex unit a's IsFoundation()`
pub const EE_UNITS_DOP_FOUNDATION_OF: i32 = 323;
/// `mFrame = 0`
pub const EE_UNITS_DOP_FRAME_ZERO: i32 = 324;
/// `mAnimation = 0`
pub const EE_UNITS_DOP_ANIMATION_ZERO: i32 = 325;
/// `mScale = unit a's mUnitClass->GetScale()`
pub const EE_UNITS_DOP_SCALE_OF_CLASS: i32 = 326;
/// `mIsFoundation = false`
pub const EE_UNITS_DOP_NOT_FOUNDATION: i32 = 327;
/// `mTransformation.MakeIdentity()`
pub const EE_UNITS_DOP_IDENTITY: i32 = 328;
/// unit `a`'s `mUnitClass->mTileSize * 0.5f` (its bits)
pub const EE_UNITS_DOP_HALF_SIZE: i32 = 329;
/// unit `a`'s `mTileX`, `mTileY` (b 0, 1)
pub const EE_UNITS_DOP_SEEN_TILE: i32 = 330;
/// `gWorld.mMap->mTerrainMesh.GetMeshZ(the floats of the bits b, c)` (its bits)
pub const EE_UNITS_DOP_MESH_Z: i32 = 331;
/// `mTransformation.SetPosition(GE3DPoint(the floats of the bits b, c, a))`
pub const EE_UNITS_DOP_SET_POSITION: i32 = 332;
/// `mTileX = b`, `mTileY = c`... as one: `mTileX = b`
pub const EE_UNITS_DOP_SET_TILE_X: i32 = 333;
/// `mTileY = b`
pub const EE_UNITS_DOP_SET_TILE_Y: i32 = 334;
/// `mModel->LoadMedia()`
pub const EE_UNITS_DOP_LOAD_MEDIA: i32 = 335;
/// `EEDoppelganger::Initialize(doppelganger a, (EEPlayer*)b, (RTSTile*)c)` (the copy's)
pub const EE_UNITS_DOP_BASE_INITIALIZE_COPY: i32 = 336;
/// the member b (`EE_UNITS_DOP_FIELD_*`) `=` `dynamic_cast<EEComplexDoppelganger*>(a)`'s
pub const EE_UNITS_DOP_COPY_FIELD: i32 = 337;
// RestoreLoadedContacts' (the loaded contacts' list, swapped into the scratch first)
/// the scratch contacts `.swap(sLoadedContacts)` (after clearing the scratch)
pub const EE_UNITS_DOP_CONTACTS_TAKE: i32 = 338;
/// the scratch contacts' `size()`
pub const EE_UNITS_DOP_CONTACTS_COUNT: i32 = 339;
/// the scratch contacts' entry `b` is still the world's unit of its id
pub const EE_UNITS_DOP_CONTACTS_VALID: i32 = 340;
/// the scratch contacts' entry `b`'s unit's `ClearContacts()`
pub const EE_UNITS_DOP_CONTACTS_CLEAR: i32 = 341;
/// the scratch contacts' entry `b`'s ids' `size()`
pub const EE_UNITS_DOP_CONTACTS_ID_COUNT: i32 = 342;
/// `LoadedComplexUnit(the scratch contacts' entry b's id c)`
pub const EE_UNITS_DOP_CONTACTS_UNIT: i32 = 343;
/// the scratch contacts' entry `b`'s unit's `AddContact((EEComplexUnit*)a)`
pub const EE_UNITS_DOP_CONTACTS_ADD: i32 = 344;

// the game's statements under the fields' reads, writes and copies (units round 11: those are Rust over the record)
/// the doppelganger's `EeUnitsComplexDopFields` base
pub const EE_UNITS_DOP_FIELDS: i32 = 10000;
/// `dynamic_cast<EEComplexDoppelganger*>(a)`'s `EeUnitsComplexDopFields` base
pub const EE_UNITS_DOP_OTHER_FIELDS: i32 = 10001;
/// `mTransformation = dynamic_cast<EEComplexDoppelganger*>(a)->mTransformation`
pub const EE_UNITS_DOP_COPY_TRANSFORMATION: i32 = 10002;

/// the members the loads, saves and copies name (`EE_UNITS_DOP_READ_FIELD`'s and the others' `b`)
pub const EE_UNITS_DOP_FIELD_PACKED_HIT_POINTS: i64 = 0;
pub const EE_UNITS_DOP_FIELD_FRAME: i64 = 1;
pub const EE_UNITS_DOP_FIELD_ANIMATION: i64 = 2;
pub const EE_UNITS_DOP_FIELD_SCALE: i64 = 3;
pub const EE_UNITS_DOP_FIELD_TILE_X: i64 = 4;
pub const EE_UNITS_DOP_FIELD_TILE_Y: i64 = 5;
pub const EE_UNITS_DOP_FIELD_FOUNDATION: i64 = 6;
pub const EE_UNITS_DOP_FIELD_MODEL: i64 = 7;
pub const EE_UNITS_DOP_FIELD_TRANSFORMATION: i64 = 8;

/// the fields after the model, in the files' order
const AFTER_MODEL: [i64; 6] = [
    EE_UNITS_DOP_FIELD_PACKED_HIT_POINTS,
    EE_UNITS_DOP_FIELD_FRAME,
    EE_UNITS_DOP_FIELD_ANIMATION,
    EE_UNITS_DOP_FIELD_SCALE,
    EE_UNITS_DOP_FIELD_TILE_X,
    EE_UNITS_DOP_FIELD_TILE_Y,
];

fn float(b: i64) -> f32 {
    f32::from_bits(b as u32)
}

fn bits(v: f32) -> i64 {
    i64::from(v.to_bits())
}

/// The typed calls of a C++ `FSFile` the fields make ([`ee_base::fs::cppfile::CppFile`]; a byte file in the tests):
/// `Read`/`Write` of a `long` (an `int32_t` in the file), an `unsigned long` (a `uint32_t`), a `float` and a `bool`
/// (one byte).
pub trait DopFile {
    fn read_i32(&mut self, v: &mut i32) -> i32;
    fn read_u32(&mut self, v: &mut u32) -> i32;
    fn read_f32(&mut self, v: &mut f32) -> i32;
    fn read_u8(&mut self, v: &mut u8) -> i32;
    fn write_i32(&mut self, v: i32) -> i32;
    fn write_u32(&mut self, v: u32) -> i32;
    fn write_f32(&mut self, v: f32) -> i32;
    fn write_u8(&mut self, v: u8) -> i32;
}

/// `file.Read(the member field)` (`EE_UNITS_DOP_FIELD_*`; was EEDopReadField): the read starts from the member's
/// value and stores what the file's call left, whatever the status.
#[allow(clippy::unnecessary_cast)] // (c_long/c_ulong: 32 bits on Windows, 64 on LP64)
pub fn read_field<F: DopFile + ?Sized>(f: &mut F, r: &mut EeUnitsComplexDopFields, field: i64) -> i32 {
    fn long<F: DopFile + ?Sized>(f: &mut F, m: &mut c_long) -> i32 {
        let mut v = *m as i32;
        let status = f.read_i32(&mut v);
        *m = v as c_long;
        status
    }
    fn ulong<F: DopFile + ?Sized>(f: &mut F, m: &mut c_ulong) -> i32 {
        let mut v = *m as u32;
        let status = f.read_u32(&mut v);
        *m = v as c_ulong;
        status
    }
    match field {
        EE_UNITS_DOP_FIELD_PACKED_HIT_POINTS => long(f, &mut r.mPackedHitPoints),
        EE_UNITS_DOP_FIELD_FRAME => ulong(f, &mut r.mFrame),
        EE_UNITS_DOP_FIELD_ANIMATION => ulong(f, &mut r.mAnimation),
        EE_UNITS_DOP_FIELD_SCALE => f.read_f32(&mut r.mScale),
        EE_UNITS_DOP_FIELD_TILE_X => long(f, &mut r.mTileX),
        EE_UNITS_DOP_FIELD_TILE_Y => long(f, &mut r.mTileY),
        _ => {
            let mut v = u8::from(r.mIsFoundation);
            let status = f.read_u8(&mut v);
            r.mIsFoundation = v != 0;
            status
        }
    }
}

/// `file.Write(the member field)` (was EEDopWriteField).
#[allow(clippy::unnecessary_cast)]
pub fn write_field<F: DopFile + ?Sized>(f: &mut F, r: &EeUnitsComplexDopFields, field: i64) -> i32 {
    match field {
        EE_UNITS_DOP_FIELD_PACKED_HIT_POINTS => f.write_i32(r.mPackedHitPoints as i32),
        EE_UNITS_DOP_FIELD_FRAME => f.write_u32(r.mFrame as u32),
        EE_UNITS_DOP_FIELD_ANIMATION => f.write_u32(r.mAnimation as u32),
        EE_UNITS_DOP_FIELD_SCALE => f.write_f32(r.mScale),
        EE_UNITS_DOP_FIELD_TILE_X => f.write_i32(r.mTileX as i32),
        EE_UNITS_DOP_FIELD_TILE_Y => f.write_i32(r.mTileY as i32),
        _ => f.write_u8(u8::from(r.mIsFoundation)),
    }
}

/// The member `field` of the record copied from another's (was EEDopCopyField; the transformation is not a record
/// member: `EE_UNITS_DOP_COPY_TRANSFORMATION`).
pub fn copy_field(r: &mut EeUnitsComplexDopFields, other: &EeUnitsComplexDopFields, field: i64) {
    match field {
        EE_UNITS_DOP_FIELD_MODEL => r.mModel = other.mModel,
        EE_UNITS_DOP_FIELD_FRAME => r.mFrame = other.mFrame,
        EE_UNITS_DOP_FIELD_ANIMATION => r.mAnimation = other.mAnimation,
        EE_UNITS_DOP_FIELD_PACKED_HIT_POINTS => r.mPackedHitPoints = other.mPackedHitPoints,
        EE_UNITS_DOP_FIELD_SCALE => r.mScale = other.mScale,
        EE_UNITS_DOP_FIELD_TILE_X => r.mTileX = other.mTileX,
        EE_UNITS_DOP_FIELD_TILE_Y => r.mTileY = other.mTileY,
        _ => r.mIsFoundation = other.mIsFoundation,
    }
}

/// `Load(major, minor, file)`: the base data, the transformation, the model's index (kept as the pointer for
/// `RepairPointers`), hit points, frame, animation, scale, tile, and the foundation flag from minor 0xe6 on. The
/// first read error.
pub fn load<H: DopHost + ?Sized>(h: &mut H, dop: u64, file: u64, major: i64, minor: i64) -> i64 {
    let mut q = |op: i32, b: i64, c: i64| h.dop(op, dop, file, b, c);
    let r = q(EE_UNITS_DOP_BASE_LOAD, major, minor);
    if r != 0 {
        return r;
    }
    q(EE_UNITS_DOP_LOAD_TRANSFORMATION, 0, 0);
    let r = q(EE_UNITS_DOP_READ_MODEL_ID, 0, 0);
    q(EE_UNITS_DOP_SET_MODEL_FROM_ID, 0, 0);
    if r != 0 {
        return r;
    }
    let old = major == 0 && minor < 0xe6;
    for (k, field) in AFTER_MODEL.iter().enumerate() {
        let r = q(EE_UNITS_DOP_READ_FIELD, *field, 0);
        if r != 0 || (old && k == AFTER_MODEL.len() - 1) {
            return r;
        }
    }
    q(EE_UNITS_DOP_READ_FIELD, EE_UNITS_DOP_FIELD_FOUNDATION, 0)
}

/// `Save(major, minor, file)`: the base data, the transformation, the model's index, hit points, frame, animation,
/// scale, tile and the foundation flag. The first write error.
pub fn save<H: DopHost + ?Sized>(h: &mut H, dop: u64, file: u64, major: i64, minor: i64) -> i64 {
    let mut q = |op: i32, b: i64, c: i64| h.dop(op, dop, file, b, c);
    let r = q(EE_UNITS_DOP_BASE_SAVE, major, minor);
    if r != 0 {
        return r;
    }
    q(EE_UNITS_DOP_SAVE_TRANSFORMATION, 0, 0);
    let r = q(EE_UNITS_DOP_WRITE_MODEL_ID, 0, 0);
    if r != 0 {
        return r;
    }
    for field in AFTER_MODEL {
        let r = q(EE_UNITS_DOP_WRITE_FIELD, field, 0);
        if r != 0 {
            return r;
        }
    }
    q(EE_UNITS_DOP_WRITE_FIELD, EE_UNITS_DOP_FIELD_FOUNDATION, 0)
}

/// `Initialize(unit, player, tile)`: what a viewing player sees of a unit (model, frame, animation, hit points or a
/// resource's amount, built flag, position and scale; a unit with no complex part stands built at its tile's
/// centre), then its media loaded.
pub fn initialize<H: DopHost + ?Sized>(h: &mut H, dop: u64, unit: u64, player: i64, tile: i64) {
    let mut q = |op: i32, a: u64, b: i64, c: i64| h.dop(op, dop, a, b, c);
    let complex = q(EE_UNITS_DOP_SEEN_COMPLEX, unit, 0, 0) as u64;
    q(EE_UNITS_DOP_BASE_INITIALIZE, unit, player, tile);
    if q(EE_UNITS_DOP_SEEN_IS_RESOURCE, unit, 0, 0) != 0 {
        q(EE_UNITS_DOP_PACK_RESOURCE, unit, 0, 0);
    } else {
        q(EE_UNITS_DOP_PACK_HIT_POINTS, unit, 0, 0);
    }
    if complex != 0 {
        if q(EE_UNITS_DOP_SEEN_IS_FOUNDATION, complex, 0, 0) != 0 {
            q(EE_UNITS_DOP_MODEL_FOUNDATION, complex, unit as i64, 0);
        } else {
            q(EE_UNITS_DOP_MODEL_OF, unit, 0, 0);
        }
        q(EE_UNITS_DOP_FRAME_OF, complex, 0, 0);
        q(EE_UNITS_DOP_ANIMATION_OF, complex, 0, 0);
        if q(EE_UNITS_DOP_SEEN_IS_BUILT, complex, 0, 0) != 0 {
            q(EE_UNITS_DOP_MARK_BUILT, 0, 0, 0);
        }
        q(EE_UNITS_DOP_TRANSFORMATION_OF, complex, 0, 0);
        q(EE_UNITS_DOP_SCALE_OF, complex, 0, 0);
        q(EE_UNITS_DOP_FOUNDATION_OF, complex, 0, 0);
    } else {
        q(EE_UNITS_DOP_MODEL_OF, unit, 0, 0);
        q(EE_UNITS_DOP_FRAME_ZERO, 0, 0, 0);
        q(EE_UNITS_DOP_ANIMATION_ZERO, 0, 0, 0);
        q(EE_UNITS_DOP_SCALE_OF_CLASS, unit, 0, 0);
        q(EE_UNITS_DOP_MARK_BUILT, 0, 0, 0);
        q(EE_UNITS_DOP_NOT_FOUNDATION, 0, 0, 0);
        q(EE_UNITS_DOP_IDENTITY, 0, 0, 0);
        q(EE_UNITS_DOP_IDENTITY, 0, 0, 0);
        let half = float(q(EE_UNITS_DOP_HALF_SIZE, unit, 0, 0));
        let x = q(EE_UNITS_DOP_SEEN_TILE, unit, 0, 0) as std::os::raw::c_long as f32 + half;
        let y = q(EE_UNITS_DOP_SEEN_TILE, unit, 1, 0) as std::os::raw::c_long as f32 + half;
        let z = q(EE_UNITS_DOP_MESH_Z, 0, bits(x), bits(y));
        q(EE_UNITS_DOP_SET_POSITION, z as u64, bits(x), bits(y));
    }
    let tx = q(EE_UNITS_DOP_SEEN_TILE, unit, 0, 0);
    q(EE_UNITS_DOP_SET_TILE_X, 0, tx, 0);
    let ty = q(EE_UNITS_DOP_SEEN_TILE, unit, 1, 0);
    q(EE_UNITS_DOP_SET_TILE_Y, 0, ty, 0);
    q(EE_UNITS_DOP_LOAD_MEDIA, 0, 0, 0);
}

/// `Initialize(other, player, tile)`: another complex doppelganger's view copied for a new viewing player.
pub fn initialize_copy<H: DopHost + ?Sized>(h: &mut H, dop: u64, other: u64, player: i64, tile: i64) {
    h.dop(EE_UNITS_DOP_BASE_INITIALIZE_COPY, dop, other, player, tile);
    for field in [
        EE_UNITS_DOP_FIELD_MODEL,
        EE_UNITS_DOP_FIELD_FRAME,
        EE_UNITS_DOP_FIELD_ANIMATION,
        EE_UNITS_DOP_FIELD_PACKED_HIT_POINTS,
        EE_UNITS_DOP_FIELD_SCALE,
        EE_UNITS_DOP_FIELD_TRANSFORMATION,
        EE_UNITS_DOP_FIELD_TILE_X,
        EE_UNITS_DOP_FIELD_TILE_Y,
        EE_UNITS_DOP_FIELD_FOUNDATION,
    ] {
        h.dop(EE_UNITS_DOP_COPY_FIELD, dop, other, field, 0);
    }
}

/// `RestoreLoadedContacts()`: each loaded unit's contacts as saved, put back at the first tick in two passes (clear
/// every unit's, then add each unit's ids last first), skipping a unit that is no longer the world's of its id.
pub fn restore_loaded_contacts<H: DopHost + ?Sized>(h: &mut H) {
    let mut q = |op: i32, a: u64, b: i64, c: i64| h.dop(op, 0, a, b, c);
    q(EE_UNITS_DOP_CONTACTS_TAKE, 0, 0, 0);
    let mut i = 0;
    while i < q(EE_UNITS_DOP_CONTACTS_COUNT, 0, 0, 0) {
        if q(EE_UNITS_DOP_CONTACTS_VALID, 0, i, 0) != 0 {
            q(EE_UNITS_DOP_CONTACTS_CLEAR, 0, i, 0);
        }
        i += 1;
    }
    let mut i = 0;
    while i < q(EE_UNITS_DOP_CONTACTS_COUNT, 0, 0, 0) {
        if q(EE_UNITS_DOP_CONTACTS_VALID, 0, i, 0) != 0 {
            // AddContact adds at the head: the last saved first
            let mut k = q(EE_UNITS_DOP_CONTACTS_ID_COUNT, 0, i, 0);
            while k > 0 {
                k -= 1;
                let contact = q(EE_UNITS_DOP_CONTACTS_UNIT, 0, i, k) as u64;
                if contact != 0 {
                    q(EE_UNITS_DOP_CONTACTS_ADD, contact, i, 0);
                }
            }
        }
        i += 1;
    }
}

#[cfg(not(test))]
mod game {
    use super::*;
    use crate::units::dop_repair::game::Game as World;
    use ee_base::fs::{CppFile, FsFile, FsWrite};
    use std::os::raw::c_void;

    impl DopFile for CppFile {
        fn read_i32(&mut self, v: &mut i32) -> i32 {
            CppFile::read_i32(self, v)
        }
        fn read_u32(&mut self, v: &mut u32) -> i32 {
            FsFile::read_u32(self, v)
        }
        fn read_f32(&mut self, v: &mut f32) -> i32 {
            CppFile::read_f32(self, v)
        }
        // (a `bool` is one byte in the file, read and written as the byte: `Read(unsigned char&)`)
        fn read_u8(&mut self, v: &mut u8) -> i32 {
            CppFile::read_u8(self, v)
        }
        fn write_i32(&mut self, v: i32) -> i32 {
            CppFile::write_i32(self, v)
        }
        fn write_u32(&mut self, v: u32) -> i32 {
            FsWrite::write_u32(self, v)
        }
        fn write_f32(&mut self, v: f32) -> i32 {
            CppFile::write_f32(self, v)
        }
        fn write_u8(&mut self, v: u8) -> i32 {
            CppFile::write_u8(self, v)
        }
    }

    /// The game's host: the fields' reads, writes and copies on the doppelganger's record here, the rest the C++'s.
    struct Game;

    impl DopHost for Game {
        fn dop(&mut self, op: i32, dop: u64, a: u64, b: i64, c: i64) -> i64 {
            let record = || World.dop(EE_UNITS_DOP_FIELDS, dop, 0, 0, 0) as usize as *mut EeUnitsComplexDopFields;
            let file = || CppFile(a as usize as *mut c_void);
            // SAFETY (the three blocks): the body runs inside the live doppelganger's member; `a` is its open file or
            // the other live complex doppelganger, as the C++ members were handed them
            match op {
                EE_UNITS_DOP_READ_FIELD => i64::from(read_field(&mut file(), unsafe { &mut *record() }, b)),
                EE_UNITS_DOP_WRITE_FIELD => i64::from(write_field(&mut file(), unsafe { &*record() }, b)),
                EE_UNITS_DOP_COPY_FIELD if b == EE_UNITS_DOP_FIELD_TRANSFORMATION => {
                    World.dop(EE_UNITS_DOP_COPY_TRANSFORMATION, dop, a, 0, 0)
                }
                EE_UNITS_DOP_COPY_FIELD => {
                    let other =
                        World.dop(EE_UNITS_DOP_OTHER_FIELDS, dop, a, 0, 0) as usize as *const EeUnitsComplexDopFields;
                    unsafe { copy_field(&mut *record(), &*other, b) };
                    0
                }
                _ => World.dop(op, dop, a, b, c),
            }
        }
    }

    /// `EEComplexDoppelganger`'s body `what` (`EE_UNITS_DOP_DO_*`) on `dop` (`a` the file or the unit or other
    /// doppelganger, `b`, `c` the version or the player and tile).
    ///
    /// # Safety
    /// `dop` is a live `EEComplexDoppelganger` (null for the contacts' restore), `a` live as the body needs it.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_dop_body(what: i32, dop: *mut c_void, a: *mut c_void, b: i64, c: i64) -> i64 {
        let (d, a) = (dop as usize as u64, a as usize as u64);
        match what {
            EE_UNITS_DOP_DO_LOAD => return load(&mut Game, d, a, b, c),
            EE_UNITS_DOP_DO_SAVE => return save(&mut Game, d, a, b, c),
            EE_UNITS_DOP_DO_INITIALIZE => initialize(&mut Game, d, a, b, c),
            EE_UNITS_DOP_DO_INITIALIZE_COPY => initialize_copy(&mut Game, d, a, b, c),
            EE_UNITS_DOP_DO_RESTORE_CONTACTS => restore_loaded_contacts(&mut Game),
            _ => {}
        }
        0
    }
}

// ---- the bodies (`ee_units_dop_body`'s `what`)
pub const EE_UNITS_DOP_DO_LOAD: i32 = 1;
pub const EE_UNITS_DOP_DO_SAVE: i32 = 2;
pub const EE_UNITS_DOP_DO_INITIALIZE: i32 = 3;
pub const EE_UNITS_DOP_DO_INITIALIZE_COPY: i32 = 4;
pub const EE_UNITS_DOP_DO_RESTORE_CONTACTS: i32 = 5;

#[cfg(test)]
#[path = "dop_file_tests.rs"]
mod tests;
