//! The units' loads over their records (R5b.4, the twenty-sixth round): `RTSUnit::Load` (0x63db4f), `EEUnit::Load`
//! (0x6298fb) and `EEComplexUnit::Load` (0x5f2452) as Rust sequences over the records the classes derive from
//! (unit_fields.rs) and the host callback of the saves ([`crate::unit_save::EeUnitsUnitSaveFn`]), as player_save.rs
//! does for the player.
//!
//! The primitive reads go through the callback with the `FSFile` overload the original called; `io` holds the old
//! value in and the read value out, so a failed read leaves what the original's variable held. The plain members are
//! read into and written through the records' raw pointers, never by a reference held across a host call. The
//! unit's C++ parts are operations: the sections (the containers with the original's loops, the port's live state),
//! the composite members' `Load`, and the pointers the original set from the ids it read (kept as ids for
//! `RepairPointers`, which the load never follows).
//!
//! Kept quirks: the original's locals carried from one read to the next (`theValue`, `theFraction`, `theFlag`,
//! `theState`), the fractions copied into the `long`s whatever their read returned (`FinishLoad` turns them back),
//! the results it ignored (the first collision tile word, the times updated, the holder's id) or overwrote (the build
//! queue's, the citizen queue's and the cargo's sections; a held unit read failing on the last one), its four error
//! labels (`LoadFlags`, `LoadLegacy`, `LoadLateVersions`, `LoadFinalVersions`) and its version tests. A bool crosses
//! the callback as 0 or 1 (a hand-edited byte other than 0 or 1 becomes true, as in player_save.rs).

#![allow(non_snake_case)]
#![allow(clippy::unnecessary_cast)]

use std::ffi::c_void;
use std::ptr::{addr_of, addr_of_mut};

use crate::units::unit_fields::{EeUnitsComplexUnitFields, EeUnitsRtsUnitFields, EeUnitsUnitFields};
use crate::units::unit_save::{EeUnitsUnitSaveFn, EeUnitsUnitSaveIn, EeUnitsUnitSaveIo, SaveHost};

// ---- the file's primitives (io holds the old value in, the read value out)
pub const EE_UNITS_UNIT_LOAD_READ_LONG: i32 = crate::units::player_save::EE_UNITS_PLAYER_SAVE_READ_LONG;
pub const EE_UNITS_UNIT_LOAD_READ_ULONG: i32 = crate::units::player_save::EE_UNITS_PLAYER_SAVE_READ_ULONG;
pub const EE_UNITS_UNIT_LOAD_READ_FLOAT: i32 = crate::units::player_save::EE_UNITS_PLAYER_SAVE_READ_FLOAT;
pub const EE_UNITS_UNIT_LOAD_READ_BOOL: i32 = crate::units::player_save::EE_UNITS_PLAYER_SAVE_READ_BOOL;
pub const EE_UNITS_UNIT_LOAD_READ_USHORT: i32 = crate::units::player_save::EE_UNITS_PLAYER_SAVE_READ_USHORT;
/// a section (`index`: `EE_UNITS_UNIT_LOAD_SECTION_*`), the result; io.b = 1 when the held units' loop left for
/// `LoadLateVersions`; io.i carries the original's `theValue` (the citizen queue, the cargo, the held units) or
/// `theState` (the unit variables) in and out
pub const EE_UNITS_UNIT_LOAD_SECTION: i32 = 110;
pub const EE_UNITS_UNIT_LOAD_SECTION_BUILD_QUEUE: u32 = 0;
pub const EE_UNITS_UNIT_LOAD_SECTION_CITIZEN_QUEUE: u32 = 1;
pub const EE_UNITS_UNIT_LOAD_SECTION_CARGO: u32 = 2;
pub const EE_UNITS_UNIT_LOAD_SECTION_WAYPOINTS: u32 = 3;
/// the goal queues, then the conversion and heal effects' handles (the original's chain)
pub const EE_UNITS_UNIT_LOAD_SECTION_GOAL_QUEUES: u32 = 4;
pub const EE_UNITS_UNIT_LOAD_SECTION_HELD_UNITS: u32 = 5;
pub const EE_UNITS_UNIT_LOAD_SECTION_UNIT_VARIABLES: u32 = 6;
/// the port's live state (0.254) and running state (0.256), put back at the first tick
pub const EE_UNITS_UNIT_LOAD_SECTION_LIVE_STATE: u32 = 7;
/// a composite member's read (`index`: `EE_UNITS_UNIT_LOAD_PART_*`), the result
pub const EE_UNITS_UNIT_LOAD_PART: i32 = 111;
pub const EE_UNITS_UNIT_LOAD_PART_PROJECTILE_START: u32 = 0;
/// the location's `Load`, then (read) `mPhysicalModel.SetLocation`
pub const EE_UNITS_UNIT_LOAD_PART_LOCATION: u32 = 1;
pub const EE_UNITS_UNIT_LOAD_PART_ANGULAR_VELOCITY: u32 = 2;
pub const EE_UNITS_UNIT_LOAD_PART_ORIENTATION: u32 = 3;
pub const EE_UNITS_UNIT_LOAD_PART_U0DC: u32 = 4;
pub const EE_UNITS_UNIT_LOAD_PART_COLOR: u32 = 5;
pub const EE_UNITS_UNIT_LOAD_PART_FIRE_EFFECT: u32 = 6;
/// the path's flag and goal tile; a unit with no path queues a pending path request
pub const EE_UNITS_UNIT_LOAD_PART_PATH: u32 = 7;
pub const EE_UNITS_UNIT_LOAD_PART_LAST_MAP_POSITION: u32 = 8;
/// `mInitialContactLocation`'s x, y, z (read as floats into the C++ point)
pub const EE_UNITS_UNIT_LOAD_PART_CONTACT_X: u32 = 9;
pub const EE_UNITS_UNIT_LOAD_PART_CONTACT_Y: u32 = 10;
pub const EE_UNITS_UNIT_LOAD_PART_CONTACT_Z: u32 = 11;
/// `mTurretAngles[index]`'s read (`EE_UNITS_UNIT_LOAD_TURRET_ANGLE`)
pub const EE_UNITS_UNIT_LOAD_TURRET_ANGLE: i32 = 112;
/// a C++ member set from a value read (`index`: `EE_UNITS_UNIT_LOAD_SET_*`; io.i or io.f the value)
pub const EE_UNITS_UNIT_LOAD_SET: i32 = 113;
/// `mBehavior = EEUnitBehavior::sTable[5]`
pub const EE_UNITS_UNIT_LOAD_SET_DEFAULT_BEHAVIOR: u32 = 0;
/// the scale (io.f) for a scenario or a zero scale from the class, then `SetScale`
pub const EE_UNITS_UNIT_LOAD_SET_SCALE: u32 = 1;
pub const EE_UNITS_UNIT_LOAD_SET_FRAME: u32 = 2;
pub const EE_UNITS_UNIT_LOAD_SET_COLLISION_TILE_X: u32 = 3;
pub const EE_UNITS_UNIT_LOAD_SET_COLLISION_TILE_Y: u32 = 4;
pub const EE_UNITS_UNIT_LOAD_SET_LAUNCHER_CLASS: u32 = 5;
pub const EE_UNITS_UNIT_LOAD_SET_PATROL_GROUP: u32 = 6;
pub const EE_UNITS_UNIT_LOAD_SET_PREVIOUS_PART: u32 = 7;
pub const EE_UNITS_UNIT_LOAD_SET_NEXT_PART: u32 = 8;
pub const EE_UNITS_UNIT_LOAD_SET_HOME_BASE: u32 = 9;
pub const EE_UNITS_UNIT_LOAD_SET_CONTAINER: u32 = 10;
/// the action read (io.i): started when the class has it (0 as 5000)
pub const EE_UNITS_UNIT_LOAD_SET_ACTION: u32 = 11;
/// `ResizeTurretAngles(io.i)`
pub const EE_UNITS_UNIT_LOAD_SET_TURRETS: u32 = 12;
pub const EE_UNITS_UNIT_LOAD_SET_GATHERED_RESOURCE: u32 = 13;
/// the last attacker's and the owner's references dropped (`index` 14, 16), then set from an id (15, 17)
pub const EE_UNITS_UNIT_LOAD_SET_LAST_ATTACKER_CLEAR: u32 = 14;
pub const EE_UNITS_UNIT_LOAD_SET_LAST_ATTACKER: u32 = 15;
pub const EE_UNITS_UNIT_LOAD_SET_OWNER_CLEAR: u32 = 16;
pub const EE_UNITS_UNIT_LOAD_SET_OWNER: u32 = 17;
pub const EE_UNITS_UNIT_LOAD_SET_MINE: u32 = 18;
pub const EE_UNITS_UNIT_LOAD_SET_HERD: u32 = 19;
pub const EE_UNITS_UNIT_LOAD_SET_HOLDER: u32 = 20;
pub const EE_UNITS_UNIT_LOAD_SET_BEHAVIOR: u32 = 21;
pub const EE_UNITS_UNIT_LOAD_SET_LAST_RESOURCE: u32 = 22;
/// `mMine = (EEUnit*)-1` (before its id's read)
pub const EE_UNITS_UNIT_LOAD_SET_MINE_UNSET: u32 = 23;

/// `kUnsetHeading` (0x446db0): `mInitialContactHeading` before its read.
pub const EE_UNITS_UNSET_HEADING: f32 = -10000.0;

/// The reader: the host, the primitives, and the original's locals carried between reads.
struct R<'a, H: SaveHost> {
    host: &'a mut H,
    /// `theValue` (an `unsigned long`)
    value: i64,
    /// `theFraction`
    fraction: f32,
    /// `theFlag`
    flag: u8,
    /// `theState` (a `long`)
    state: i64,
}

impl<H: SaveHost> R<'_, H> {
    fn read_i(&mut self, op: i32, old: i64) -> (i32, i64) {
        let mut io = EeUnitsUnitSaveIo { i: old, ..Default::default() };
        let r = self.host.op(op, 0, &mut io);
        (r, io.i)
    }
    fn read_f(&mut self, old: f32) -> (i32, f32) {
        let mut io = EeUnitsUnitSaveIo { f: old, ..Default::default() };
        let r = self.host.op(EE_UNITS_UNIT_LOAD_READ_FLOAT, 0, &mut io);
        (r, io.f)
    }
    fn read_b(&mut self, old: u8) -> (i32, u8) {
        let mut io = EeUnitsUnitSaveIo { b: old, ..Default::default() };
        let r = self.host.op(EE_UNITS_UNIT_LOAD_READ_BOOL, 0, &mut io);
        (r, u8::from(io.b != 0))
    }
    /// `inFile.Read(theValue)`
    fn value(&mut self) -> i32 {
        let (r, v) = self.read_i(EE_UNITS_UNIT_LOAD_READ_ULONG, self.value);
        self.value = v;
        r
    }
    /// `inFile.Read(theFraction)`
    fn fraction(&mut self) -> i32 {
        let (r, v) = self.read_f(self.fraction);
        self.fraction = v;
        r
    }
    fn set(&mut self, index: u32, i: i64, f: f32) {
        self.host.op(EE_UNITS_UNIT_LOAD_SET, index, &mut EeUnitsUnitSaveIo { i, f, b: 0 });
    }
    fn section(&mut self, index: u32) -> (i32, bool) {
        let mut io = EeUnitsUnitSaveIo::default();
        let r = self.host.op(EE_UNITS_UNIT_LOAD_SECTION, index, &mut io);
        (r, io.b != 0)
    }
    /// A section that reads into the original's `theValue` (the citizen queue, the cargo, the held units).
    fn section_value(&mut self, index: u32) -> (i32, bool) {
        let mut io = EeUnitsUnitSaveIo { i: self.value, ..Default::default() };
        let r = self.host.op(EE_UNITS_UNIT_LOAD_SECTION, index, &mut io);
        self.value = io.i;
        (r, io.b != 0)
    }
    fn part(&mut self, index: u32) -> i32 {
        self.host.op(EE_UNITS_UNIT_LOAD_PART, index, &mut EeUnitsUnitSaveIo::default())
    }
}

/// Reads a `long` member (`c_long`) through the record's raw pointer: the old value in, the read value stored.
macro_rules! rd_long {
    ($r:expr, $p:expr, $f:ident) => {{
        // SAFETY: the caller's pointer names a live record; no reference is held across the host call
        let old = unsafe { std::ptr::read(addr_of!((*$p).$f)) } as i64;
        let (res, v) = $r.read_i(EE_UNITS_UNIT_LOAD_READ_LONG, old);
        // SAFETY: as above
        unsafe { std::ptr::write(addr_of_mut!((*$p).$f), v as _) };
        res
    }};
}
macro_rules! rd_ulong {
    ($r:expr, $p:expr, $f:ident) => {{
        // SAFETY: the caller's pointer names a live record
        let old = unsafe { std::ptr::read(addr_of!((*$p).$f)) } as i64;
        let (res, v) = $r.read_i(EE_UNITS_UNIT_LOAD_READ_ULONG, old);
        // SAFETY: as above
        unsafe { std::ptr::write(addr_of_mut!((*$p).$f), v as _) };
        res
    }};
}
macro_rules! rd_ushort {
    ($r:expr, $p:expr, $f:ident) => {{
        // SAFETY: the caller's pointer names a live record
        let old = i64::from(unsafe { std::ptr::read(addr_of!((*$p).$f)) });
        let (res, v) = $r.read_i(EE_UNITS_UNIT_LOAD_READ_USHORT, old);
        // SAFETY: as above
        unsafe { std::ptr::write(addr_of_mut!((*$p).$f), v as u16) };
        res
    }};
}
macro_rules! rd_float {
    ($r:expr, $p:expr, $f:ident) => {{
        // SAFETY: the caller's pointer names a live record
        let old = unsafe { std::ptr::read(addr_of!((*$p).$f)) };
        let (res, v) = $r.read_f(old);
        // SAFETY: as above
        unsafe { std::ptr::write(addr_of_mut!((*$p).$f), v) };
        res
    }};
}
macro_rules! rd_bool {
    ($r:expr, $p:expr, $f:ident) => {{
        // SAFETY: the caller's pointer names a live record (its byte read, never a Rust bool of it)
        let old = unsafe { crate::units::unit_fields::bool_byte(addr_of!((*$p).$f)) };
        let (res, v) = $r.read_b(old);
        // SAFETY: as above
        unsafe { std::ptr::write(addr_of_mut!((*$p).$f), v != 0) };
        res
    }};
}
/// Stores a value into a member (no read).
macro_rules! put {
    ($p:expr, $f:ident, $v:expr) => {
        // SAFETY: the caller's pointer names a live record
        unsafe { std::ptr::write(addr_of_mut!((*$p).$f), $v) }
    };
}
/// The original's `memcpy(&this->m, &theFraction, 4)`: the float's bits into the `long`'s first four bytes (its low
/// half where `long` has 64 bits; `FinishLoad` reads the same four bytes back).
macro_rules! put_bits {
    ($p:expr, $f:ident, $v:expr) => {
        // SAFETY: the caller's pointer names a live record; a long has at least four bytes (little-endian)
        unsafe { std::ptr::write_unaligned(addr_of_mut!((*$p).$f).cast::<u32>(), ($v as f32).to_bits()) }
    };
}

/// Runs the reads in order while each returns 0; the first non-zero result (0 when all read).
macro_rules! chain {
    ($($e:expr),+ $(,)?) => {{
        let mut r = 0;
        $( if r == 0 { r = $e; } )+
        r
    }};
}

/// `RTSUnit::Load`: the tile, the id, the flags (the editor selection cleared). The first read error.
///
/// # Safety
/// `rts` points to a live record.
pub unsafe fn rts_load<H: SaveHost>(host: &mut H, rts: *mut EeUnitsRtsUnitFields) -> i32 {
    let mut rd = R { host, value: 0, fraction: 0.0, flag: 0, state: 0 };
    let r = rd_long!(rd, rts, mTileX);
    if r != 0 {
        return r;
    }
    let r = rd_long!(rd, rts, mTileY);
    if r != 0 {
        return r;
    }
    let r = rd_ulong!(rd, rts, mUnitID);
    if r != 0 {
        return r;
    }
    let r = rd_bool!(rd, rts, mIsQueuedForDelete);
    if r != 0 {
        return r;
    }
    let mut r = rd_bool!(rd, rts, mIsEditorSelected);
    put!(rts, mIsEditorSelected, false);
    if r == 0 {
        r = rd_bool!(rd, rts, mIsValid);
    }
    r
}

/// `EEUnit::Load`: the base unit, the resource type, the hit points' saved fraction (its bits held in `mHitPoints`
/// until `FinishLoad`), the amount and an unused word. The first read error.
///
/// # Safety
/// The pointers name the live records of one unit.
pub unsafe fn unit_load<H: SaveHost>(
    host: &mut H,
    rts: *mut EeUnitsRtsUnitFields,
    unit: *mut EeUnitsUnitFields,
) -> i32 {
    // SAFETY: the caller's contract
    let mut r = unsafe { rts_load(host, rts) };
    if r == 0 {
        let mut rd = R { host, value: 0, fraction: 0.0, flag: 0, state: 0 };
        r = rd_ulong!(rd, unit, mResourceType);
        if r == 0 {
            r = rd.fraction();
            put_bits!(unit, mHitPoints, rd.fraction);
            if r == 0 {
                r = rd_long!(rd, unit, mResourceAmount);
                if r == 0 {
                    // theUnused
                    r = rd.read_i(EE_UNITS_UNIT_LOAD_READ_LONG, 0).0;
                }
            }
        }
    }
    r
}

/// `EEComplexUnit::Load` after `EEUnit::Load` (its result `base`): the sections, the members, the version-dependent
/// tail and the port's live state, in the original's order with its labels. The read status.
///
/// # Safety
/// `cx` points to the live record of the unit the host loads.
pub unsafe fn complex_load_rest<H: SaveHost>(
    host: &mut H,
    cx: *mut EeUnitsComplexUnitFields,
    inp: &EeUnitsUnitSaveIn,
    base: i32,
) -> i32 {
    let mut rd = R { host, value: 0, fraction: 0.0, flag: 0, state: 0 };
    let (major, minor) = (inp.major, inp.minor);
    let mut r = base;
    if r == 0 {
        // (a failed read goes on at LoadFlags, whose first read starts a new result)
        load_to_flags(&mut rd, cx);
        // LoadFlags:
        r = chain!(
            rd_ulong!(rd, cx, mConversionStartTime),
            rd_ushort!(rd, cx, mCollisionState),
            rd_bool!(rd, cx, mIsDisabled),
            rd_bool!(rd, cx, mIsDeletable),
            rd_bool!(rd, cx, mIsCommandable),
            rd_bool!(rd, cx, mIsAnimationPaused),
            rd_bool!(rd, cx, mHasArrived),
            rd_bool!(rd, cx, mIsPathDone),
            rd_bool!(rd, cx, mIsClearanceGranted),
        );
        if r == 0 {
            r = rd_bool!(rd, cx, mIsBuilt);
        }
        // LoadLegacy:
        if minor <= 0x1b {
            for _ in 0..2 {
                if r == 0 {
                    let (res, v) = rd.read_b(rd.flag);
                    rd.flag = v;
                    r = res;
                }
            }
        } else {
            if r == 0 {
                r = rd_float!(rd, cx, mOpacity);
            }
            if r == 0 {
                r = rd_bool!(rd, cx, mIsCloaked);
            }
        }
        if r == 0 {
            r = rd.value();
            rd.set(EE_UNITS_UNIT_LOAD_SET_GATHERED_RESOURCE, rd.value, 0.0);
            if r == 0 {
                r = rd_ulong!(rd, cx, mNumAreaEffects);
                if r == 0 {
                    r = rd_ulong!(rd, cx, mU2FC);
                }
            }
        }
        rd.set(EE_UNITS_UNIT_LOAD_SET_LAST_ATTACKER_CLEAR, 0, 0.0);
        if r == 0 {
            r = rd.value();
            rd.set(EE_UNITS_UNIT_LOAD_SET_LAST_ATTACKER, rd.value, 0.0);
        }
        rd.set(EE_UNITS_UNIT_LOAD_SET_OWNER_CLEAR, 0, 0.0);
        if r == 0 {
            r = rd.value();
            rd.set(EE_UNITS_UNIT_LOAD_SET_OWNER, rd.value, 0.0);
            if r == 0 {
                r = rd.fraction();
                put_bits!(cx, mGarrisonCount, rd.fraction);
                if r == 0 {
                    r = chain!(
                        rd_long!(rd, cx, mGarrisonPopulation),
                        rd_bool!(rd, cx, mIsPaidFor),
                        rd_long!(rd, cx, mNumBuilders)
                    );
                }
            }
        }
        rd.value = 0xffff_ffff;
        rd.set(EE_UNITS_UNIT_LOAD_SET_MINE_UNSET, 0, 0.0);
        if r == 0 {
            r = rd.value();
            rd.set(EE_UNITS_UNIT_LOAD_SET_MINE, rd.value, 0.0);
            if r == 0 {
                r = chain!(
                    rd.part(EE_UNITS_UNIT_LOAD_PART_U0DC),
                    rd_bool!(rd, cx, mIsDetachedFromAI),
                    rd.part(EE_UNITS_UNIT_LOAD_PART_COLOR),
                    rd_bool!(rd, cx, mIsTinted),
                    rd_bool!(rd, cx, mIsLocked),
                    rd_ulong!(rd, cx, mGateDirection),
                    rd_float!(rd, cx, mOpenFraction),
                );
                if r == 0 {
                    r = rd.section(EE_UNITS_UNIT_LOAD_SECTION_GOAL_QUEUES).0;
                }
            }
        }
        rd.value = 0;
        if r == 0 {
            r = rd.value();
            rd.set(EE_UNITS_UNIT_LOAD_SET_HERD, rd.value, 0.0);
        }
        rd.value = 0xffff_ffff;
        if r == 0 {
            // (its result not kept)
            rd.value();
            rd.set(EE_UNITS_UNIT_LOAD_SET_HOLDER, rd.value, 0.0);
        }
        let (res, jumped) = rd.section_value(EE_UNITS_UNIT_LOAD_SECTION_HELD_UNITS);
        r = res;
        if !jumped {
            r = load_to_late(&mut rd, cx);
        }
        // LoadLateVersions:
        if minor >= 0xc9 && r == 0 {
            r = rd_bool!(rd, cx, mIsFoundation);
            if r == 0 && (major != 0 || minor >= 0xcd) {
                // (its loop reads into the original's `theState`)
                let mut io = EeUnitsUnitSaveIo { i: rd.state, ..Default::default() };
                r = rd.host.op(EE_UNITS_UNIT_LOAD_SECTION, EE_UNITS_UNIT_LOAD_SECTION_UNIT_VARIABLES, &mut io);
                rd.state = io.i;
            }
        }
        if minor >= 0xd3 && r == 0 {
            r = rd_long!(rd, cx, mFormationRank);
            if r == 0 {
                r = rd_long!(rd, cx, mWaterState);
            }
        }
        if minor >= 0xd5 && r == 0 {
            r = rd.part(EE_UNITS_UNIT_LOAD_PART_LAST_MAP_POSITION);
        }
        if minor >= 0xe2 && r == 0 {
            r = rd_ulong!(rd, cx, mCloaked);
        }
        if minor >= 0xe8 && r == 0 {
            r = rd_ulong!(rd, cx, mCamouflageTimer);
        }
    }
    // LoadFinalVersions:
    if major == 0 && minor <= 0xea {
        put!(cx, mIsSpecialAttack, false);
        put!(cx, mDisabledReason, 0);
    } else if r == 0 {
        r = rd_bool!(rd, cx, mIsSpecialAttack);
        if r == 0 {
            r = rd_ulong!(rd, cx, mDisabledReason);
        }
    }
    if minor < 0xf3 {
        put!(cx, mPlanetID, 0);
    } else if r == 0 {
        r = rd_ushort!(rd, cx, mPlanetID);
    }
    if major == 0 && minor < 0xf4 {
        put!(cx, mSight, 0.0);
    } else if r == 0 {
        r = rd_float!(rd, cx, mSight);
    }
    if (major != 0 || minor >= 0xf5) && r == 0 {
        r = rd_ulong!(rd, cx, mU32C);
    }
    if r == 0 && inp.live_state != 0 {
        r = rd.section(EE_UNITS_UNIT_LOAD_SECTION_LIVE_STATE).0;
    }
    r
}

/// From the build queue to the times updated: the part a failed read leaves for `LoadFlags` (whose first read
/// starts a new result).
fn load_to_flags<H: SaveHost>(rd: &mut R<'_, H>, cx: *mut EeUnitsComplexUnitFields) -> i32 {
    put!(cx, mInitialContactHeading, EE_UNITS_UNSET_HEADING);
    rd.set(EE_UNITS_UNIT_LOAD_SET_DEFAULT_BEHAVIOR, 0, 0.0);
    // each section's count read starts a new result: the build queue's, the citizen queue's and the cargo's errors
    // are not tested
    rd.section(EE_UNITS_UNIT_LOAD_SECTION_BUILD_QUEUE);
    rd.section_value(EE_UNITS_UNIT_LOAD_SECTION_CITIZEN_QUEUE);
    rd.section_value(EE_UNITS_UNIT_LOAD_SECTION_CARGO);
    let mut r = rd.section(EE_UNITS_UNIT_LOAD_SECTION_WAYPOINTS).0;
    if r != 0 {
        return r;
    }
    r = chain!(rd.part(EE_UNITS_UNIT_LOAD_PART_PROJECTILE_START), rd.part(EE_UNITS_UNIT_LOAD_PART_LOCATION));
    if r != 0 {
        return r;
    }
    let (res, flag) = rd.read_b(rd.flag);
    rd.flag = flag;
    put!(cx, mIsFiringHigh, flag != 0);
    if res != 0 {
        return res;
    }
    // theScale
    let (res, scale) = rd.read_f(0.0);
    if res != 0 {
        return res;
    }
    rd.set(EE_UNITS_UNIT_LOAD_SET_SCALE, 0, scale);
    r = rd.value();
    rd.set(EE_UNITS_UNIT_LOAD_SET_FRAME, rd.value, 0.0);
    r = chain!(r, rd.part(EE_UNITS_UNIT_LOAD_PART_ANGULAR_VELOCITY), rd.part(EE_UNITS_UNIT_LOAD_PART_ORIENTATION));
    if r != 0 {
        return r;
    }
    // the first word's result is not tested
    let (_, x) = rd.read_i(EE_UNITS_UNIT_LOAD_READ_LONG, rd.state);
    rd.state = x;
    rd.set(EE_UNITS_UNIT_LOAD_SET_COLLISION_TILE_X, rd.state, 0.0);
    let (res, y) = rd.read_i(EE_UNITS_UNIT_LOAD_READ_LONG, rd.state);
    rd.state = y;
    rd.set(EE_UNITS_UNIT_LOAD_SET_COLLISION_TILE_Y, rd.state, 0.0);
    r = chain!(res, rd.value());
    if r != 0 {
        return r;
    }
    rd.set(EE_UNITS_UNIT_LOAD_SET_LAUNCHER_CLASS, rd.value, 0.0);
    for index in [
        EE_UNITS_UNIT_LOAD_SET_PATROL_GROUP,
        EE_UNITS_UNIT_LOAD_SET_PREVIOUS_PART,
        EE_UNITS_UNIT_LOAD_SET_NEXT_PART,
        EE_UNITS_UNIT_LOAD_SET_HOME_BASE,
        EE_UNITS_UNIT_LOAD_SET_CONTAINER,
    ] {
        r = rd.value();
        rd.set(index, rd.value, 0.0);
        if r != 0 {
            return r;
        }
    }
    // theAction (its own local: no old value carried)
    let (res, action) = rd.read_i(EE_UNITS_UNIT_LOAD_READ_ULONG, 0);
    rd.set(EE_UNITS_UNIT_LOAD_SET_ACTION, action, 0.0);
    if res != 0 {
        return res;
    }
    // thePartsResult and theCount
    let (mut parts, count) = rd.read_i(EE_UNITS_UNIT_LOAD_READ_LONG, 0);
    if parts != 0 {
        return parts;
    }
    rd.set(EE_UNITS_UNIT_LOAD_SET_TURRETS, count, 0.0);
    put!(cx, mNumTurretAngles, count as _);
    let mut q = 0i64;
    while q < count {
        if parts != 0 {
            return parts;
        }
        parts = rd.host.op(EE_UNITS_UNIT_LOAD_TURRET_ANGLE, q as u32, &mut EeUnitsUnitSaveIo::default());
        q += 1;
    }
    if parts != 0 {
        return parts;
    }
    r = chain!(
        rd_float!(rd, cx, mHeight),
        rd_float!(rd, cx, mVelocity),
        rd_float!(rd, cx, mF220),
        rd_float!(rd, cx, mF21C),
        rd_long!(rd, cx, mCurrentResearch),
        rd_ulong!(rd, cx, mAnimation),
        rd_ulong!(rd, cx, mCurrentPhysicsAction),
        rd_ulong!(rd, cx, mCurrentPhysicsOrder),
        rd_ulong!(rd, cx, mFacing),
        rd_float!(rd, cx, mLaunchYaw),
    );
    if r != 0 {
        return r;
    }
    r = rd.fraction();
    put_bits!(cx, mLastHitPoints, rd.fraction);
    if r != 0 {
        return r;
    }
    r = chain!(rd_long!(rd, cx, mExitingUnitID), rd_long!(rd, cx, mCrewHitPoints), rd_long!(rd, cx, mProductionTime));
    if r != 0 {
        return r;
    }
    r = rd.fraction();
    put_bits!(cx, mFlightTime, rd.fraction);
    if r != 0 {
        return r;
    }
    r = chain!(
        rd_long!(rd, cx, mReloadTimer),
        rd_long!(rd, cx, mCargoRoom),
        rd_long!(rd, cx, mRottingAwayTime),
        rd_long!(rd, cx, mTargetSearchTimer),
        rd_long!(rd, cx, mWorkTimer),
        rd_long!(rd, cx, mRegenerationTimer),
        rd_float!(rd, cx, mHealSearchTimer),
        rd_float!(rd, cx, mCollisionResolutionCount),
        rd_long!(rd, cx, mWakeTimer),
        rd_long!(rd, cx, mTrailEffectTimer),
        rd_ulong!(rd, cx, mAmmunition),
        rd_ulong!(rd, cx, mAnimationTime),
        rd.value(),
        rd_ulong!(rd, cx, mWeaponTagPoint),
    );
    if r != 0 {
        return r;
    }
    // (its result not kept)
    rd_ulong!(rd, cx, mTimesUpdated);
    0
}

/// From the growth to the last resource: the part after the held units (a failed read goes on at
/// `LoadLateVersions`). The result.
fn load_to_late<H: SaveHost>(rd: &mut R<'_, H>, cx: *mut EeUnitsComplexUnitFields) -> i32 {
    let mut r = chain!(
        rd_float!(rd, cx, mGrowth),
        rd_bool!(rd, cx, mB34F),
        rd_bool!(rd, cx, mIsRegrowing),
        rd_ulong!(rd, cx, mNumAttackers),
        rd_ulong!(rd, cx, mGrowTime),
        rd_ulong!(rd, cx, mGrazeTime),
        rd.part(EE_UNITS_UNIT_LOAD_PART_FIRE_EFFECT),
    );
    if r != 0 {
        return r;
    }
    // the path's result is overwritten by the morale's first read
    rd.part(EE_UNITS_UNIT_LOAD_PART_PATH);
    r = chain!(rd_long!(rd, cx, mPositiveMorale), rd_long!(rd, cx, mNegativeMorale));
    if r != 0 {
        return r;
    }
    r = rd.fraction();
    put_bits!(cx, mPower, rd.fraction);
    if r != 0 {
        return r;
    }
    r = chain!(
        rd_long!(rd, cx, mPowerRegenTimer),
        rd_bool!(rd, cx, mIsCloakable),
        rd.part(EE_UNITS_UNIT_LOAD_PART_CONTACT_X),
        rd.part(EE_UNITS_UNIT_LOAD_PART_CONTACT_Y),
        rd.part(EE_UNITS_UNIT_LOAD_PART_CONTACT_Z),
        rd_float!(rd, cx, mInitialContactHeading),
    );
    if r != 0 {
        return r;
    }
    r = rd.value();
    rd.set(EE_UNITS_UNIT_LOAD_SET_BEHAVIOR, rd.value, 0.0);
    if r == 0 {
        r = rd_ushort!(rd, cx, mExperience);
        if r == 0 {
            r = rd.value();
            rd.set(EE_UNITS_UNIT_LOAD_SET_LAST_RESOURCE, rd.value, 0.0);
        }
    }
    r
}

/// The C ABI: `RTSUnit::Load` (`unit`, `complex` null), `EEUnit::Load` (`complex` null) or `EEComplexUnit::Load` (all
/// three) of one unit over the host callback.
///
/// # Safety
/// The record pointers name the live records of the unit the callback loads (null where noted); `inp` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_units_unit_load(
    ctx: *mut c_void,
    f: EeUnitsUnitSaveFn,
    rts: *mut EeUnitsRtsUnitFields,
    unit: *mut EeUnitsUnitFields,
    complex: *mut EeUnitsComplexUnitFields,
    inp: *const EeUnitsUnitSaveIn,
) -> i32 {
    let mut host = crate::units::unit_save::CallbackHost { ctx, f };
    // SAFETY: the caller's contract
    let inp = unsafe { *inp };
    if unit.is_null() {
        // SAFETY: the caller's contract
        return unsafe { rts_load(&mut host, rts) };
    }
    // SAFETY: the caller's contract
    let r = unsafe { unit_load(&mut host, rts, unit) };
    if complex.is_null() {
        return r;
    }
    // SAFETY: the caller's contract
    unsafe { complex_load_rest(&mut host, complex, &inp, r) }
}

/// The maxima `EEComplexUnit::FinishLoad` turns the saved fractions back into amounts with (the unit's stats' and
/// class's).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsFinishLoadIn {
    /// `mStats->mMaxHitPoints` (the last hit points' fraction)
    pub max_hit_points: std::os::raw::c_long,
    /// `mStats->mFlightTime`
    pub flight_time: std::os::raw::c_long,
    /// `GetUnitClass()->mGarrisonCapacity`
    pub garrison_capacity: std::os::raw::c_long,
    /// `mStats->mPower`
    pub power: std::os::raw::c_long,
}

/// The float a load left in a `long`'s first four bytes (`memcpy(&theFraction, &this->m, 4)`; little-endian).
fn bits_of(value: std::os::raw::c_long) -> f32 {
    f32::from_bits(value as u32)
}

/// One fraction back into an amount: `(long)(fraction * maximum + 0.00001f)`, at least 1 when the fraction is not 0.
#[allow(clippy::cast_precision_loss, clippy::cast_possible_truncation)]
fn amount(fraction: f32, product: f32) -> std::os::raw::c_long {
    let v = (product + 0.00001) as std::os::raw::c_long;
    if fraction != 0.0 && v == 0 {
        1
    } else {
        v
    }
}

/// `EEComplexUnit::FinishLoad`'s four fractions (0x5f3395), in its order and with its operand order.
pub fn finish_fractions(cx: &mut EeUnitsComplexUnitFields, input: &EeUnitsFinishLoadIn) {
    let f = bits_of(cx.mLastHitPoints);
    cx.mLastHitPoints = amount(f, input.max_hit_points as f32 * f);
    let f = bits_of(cx.mFlightTime);
    cx.mFlightTime = amount(f, f * input.flight_time as f32);
    let f = bits_of(cx.mGarrisonCount);
    cx.mGarrisonCount = amount(f, f * input.garrison_capacity as f32);
    let f = bits_of(cx.mPower);
    cx.mPower = amount(f, f * input.power as f32);
}

/// `EEUnit::FinishLoad`'s hit points (0x629973): rounded by `UMFloatToLong` (the UCRT's `lrintf`), unlike the four.
pub fn finish_hit_points(unit: &mut EeUnitsUnitFields, max_hit_points: std::os::raw::c_long) {
    let f = bits_of(unit.mHitPoints);
    let v = ee_render::ge::um::lrintf(max_hit_points as f32 * f + 0.00001) as std::os::raw::c_long;
    unit.mHitPoints = if f != 0.0 && v == 0 { 1 } else { v };
}

/// `EEComplexUnit::FinishLoad`'s four fractions over the unit's record.
///
/// # Safety
/// `cx` is the unit's live record; `input` readable.
#[no_mangle]
pub unsafe extern "C" fn ee_units_unit_finish_fractions(
    cx: *mut EeUnitsComplexUnitFields,
    input: *const EeUnitsFinishLoadIn,
) {
    // SAFETY: the caller's contract; no host call while the references live
    unsafe { finish_fractions(&mut *cx, &*input) };
}

/// `EEUnit::FinishLoad`'s hit points over the unit's record.
///
/// # Safety
/// `unit` is the unit's live record.
#[no_mangle]
pub unsafe extern "C" fn ee_units_unit_finish_hit_points(
    unit: *mut EeUnitsUnitFields,
    max_hit_points: std::os::raw::c_long,
) {
    // SAFETY: the caller's contract
    unsafe { finish_hit_points(&mut *unit, max_hit_points) };
}

#[cfg(test)]
#[path = "unit_load_tests.rs"]
mod tests;
