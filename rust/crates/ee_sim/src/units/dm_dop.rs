//! `EEDoppelganger`'s bodies (the units' lane, round 9; were EEDoppelganger.cpp): `Reset`, `Clear`, the two
//! `Initialize`s, `Load`, `RepairPointers` and `Save`. Every C++ statement of the originals is one operation of the
//! doppelganger manager's host ([`DmHost::dm`], `ee_units_dm_host` in EEDoppelgangerManager.cpp: its `EE_UNITS_DM_*`
//! and the doppelganger's `EE_UNITS_DM_DG_*` below), asked in the original's order. `d` is the doppelganger.

use crate::units::dm_body::{
    free, DmHost, EE_UNITS_DM_DOPPELGANGER_PLAYER, EE_UNITS_DM_DOPPELGANGER_TILE, EE_UNITS_DM_DOPPELGANGER_UNIT,
    EE_UNITS_DM_IS_COMPLEX, EE_UNITS_DM_PLAYER_AT, EE_UNITS_DM_PLAYER_ID, EE_UNITS_DM_READ_LONG,
    EE_UNITS_DM_READ_ULONG, EE_UNITS_DM_SET_FILE, EE_UNITS_DM_TILE_ADD,
};

// ---- the doppelganger's statements (`op`; the answer)
/// doppelganger `a`'s `mUnit = (EEUnit*)b`
pub const EE_UNITS_DM_DG_SET_UNIT: i32 = 4000;
/// doppelganger `a`'s `mPlayer = (EEPlayer*)b`
pub const EE_UNITS_DM_DG_SET_PLAYER: i32 = 4001;
/// doppelganger `a`'s `mTile = (RTSTile*)b`
pub const EE_UNITS_DM_DG_SET_TILE: i32 = 4002;
/// doppelganger `a`'s `mUnit->mStats->mModel->LoadMedia()`
pub const EE_UNITS_DM_DG_LOAD_MEDIA: i32 = 4003;
/// doppelganger `a`'s `mUnit->mStats->mModel->UnloadMedia()`
pub const EE_UNITS_DM_DG_UNLOAD_MEDIA: i32 = 4004;
/// unit `a`'s `Reference()`
pub const EE_UNITS_DM_DG_REFERENCE: i32 = 4005;
/// unit `a`'s `Dereference()`
pub const EE_UNITS_DM_DG_DEREFERENCE: i32 = 4006;
/// the unsigned long read last (`EE_UNITS_DM_READ_ULONG`)
pub const EE_UNITS_DM_DG_VALUE: i32 = 4007;
/// the long read last (`EE_UNITS_DM_READ_LONG`)
pub const EE_UNITS_DM_DG_COORDINATE: i32 = 4008;
/// doppelganger `a`'s `(int32_t)USavedID(mPlayer)`
pub const EE_UNITS_DM_DG_SAVED_PLAYER: i32 = 4009;
/// `static_cast<EEUnit*>(gWorld.GetUnit(USavedID(doppelganger a's mUnit)))`
pub const EE_UNITS_DM_DG_SAVED_UNIT: i32 = 4010;
/// doppelganger `a`'s `GetIsValid()`
pub const EE_UNITS_DM_DG_IS_VALID: i32 = 4011;
/// the file's `Write(b != 0)` (a bool)
pub const EE_UNITS_DM_DG_WRITE_BOOL: i32 = 4012;
/// the file's `Write((unsigned long)b)`
pub const EE_UNITS_DM_DG_WRITE_ULONG: i32 = 4013;
/// doppelganger `a`'s `mUnit->mUnitID`
pub const EE_UNITS_DM_DG_UNIT_ID: i32 = 4014;

/// `mPool.DumpStats(false)`
pub const EE_UNITS_DM_DG_POOL_DUMP: i32 = 4015;
/// `mPool.Release()`
pub const EE_UNITS_DM_DG_POOL_RELEASE: i32 = 4016;
/// `mComplexPool.DumpStats(false)`
pub const EE_UNITS_DM_DG_COMPLEX_POOL_DUMP: i32 = 4017;
/// `mComplexPool.Release()`
pub const EE_UNITS_DM_DG_COMPLEX_POOL_RELEASE: i32 = 4018;

/// `RepairPointers`' failure (a unit or a player gone)
const REPAIR_FAILED: i64 = 9;

fn q<H: DmHost + ?Sized>(h: &mut H, op: i32, a: u64) -> i64 {
    h.dm(op, a, 0, 0)
}

fn qb<H: DmHost + ?Sized>(h: &mut H, op: i32, a: u64, b: i64) -> i64 {
    h.dm(op, a, b, 0)
}

/// `Reset()`: no unit, viewing player or tile.
pub fn reset<H: DmHost + ?Sized>(h: &mut H, d: u64) {
    qb(h, EE_UNITS_DM_DG_SET_UNIT, d, 0);
    qb(h, EE_UNITS_DM_DG_SET_PLAYER, d, 0);
    qb(h, EE_UNITS_DM_DG_SET_TILE, d, 0);
}

/// `Clear()`: the unit's media unloaded (a simple doppelganger's) and the unit released, then `Reset`.
pub fn clear<H: DmHost + ?Sized>(h: &mut H, d: u64) {
    let unit = q(h, EE_UNITS_DM_DOPPELGANGER_UNIT, d) as u64;
    if unit != 0 {
        if q(h, EE_UNITS_DM_IS_COMPLEX, d) == 0 {
            q(h, EE_UNITS_DM_DG_UNLOAD_MEDIA, d);
        }
        q(h, EE_UNITS_DM_DG_DEREFERENCE, unit);
    }
    reset(h, d);
}

/// `Initialize(unit, player, tile)`: remembers the unit as `player` sees it on `tile`.
pub fn initialize<H: DmHost + ?Sized>(h: &mut H, d: u64, unit: u64, player: u64, tile: u64) {
    qb(h, EE_UNITS_DM_DG_SET_PLAYER, d, player as i64);
    qb(h, EE_UNITS_DM_DG_SET_UNIT, d, unit as i64);
    if q(h, EE_UNITS_DM_IS_COMPLEX, d) == 0 {
        q(h, EE_UNITS_DM_DG_LOAD_MEDIA, d);
    }
    q(h, EE_UNITS_DM_DG_REFERENCE, unit);
    qb(h, EE_UNITS_DM_DG_SET_TILE, d, tile as i64);
}

/// `Initialize(other, player, tile)`: a copy of another doppelganger's unit and tile for `player` (`tile` unused).
pub fn initialize_copy<H: DmHost + ?Sized>(h: &mut H, d: u64, other: u64, player: u64) {
    qb(h, EE_UNITS_DM_DG_SET_PLAYER, d, player as i64);
    let unit = q(h, EE_UNITS_DM_DOPPELGANGER_UNIT, other) as u64;
    q(h, EE_UNITS_DM_DG_REFERENCE, unit);
    let unit = q(h, EE_UNITS_DM_DOPPELGANGER_UNIT, other);
    qb(h, EE_UNITS_DM_DG_SET_UNIT, d, unit);
    let tile = q(h, EE_UNITS_DM_DOPPELGANGER_TILE, other);
    qb(h, EE_UNITS_DM_DG_SET_TILE, d, tile);
    if q(h, EE_UNITS_DM_IS_COMPLEX, d) == 0 {
        q(h, EE_UNITS_DM_DG_LOAD_MEDIA, d);
    }
}

/// `Load(major, minor, file)`: the unit's id and the player's index, kept for `RepairPointers`; the second read's
/// result.
pub fn load<H: DmHost + ?Sized>(h: &mut H, d: u64, file: u64) -> i64 {
    q(h, EE_UNITS_DM_SET_FILE, file);
    q(h, EE_UNITS_DM_READ_ULONG, 0);
    let id = q(h, EE_UNITS_DM_DG_VALUE, 0);
    qb(h, EE_UNITS_DM_DG_SET_UNIT, d, id);
    let result = q(h, EE_UNITS_DM_READ_LONG, 0);
    let player = q(h, EE_UNITS_DM_DG_COORDINATE, 0);
    qb(h, EE_UNITS_DM_DG_SET_PLAYER, d, player);
    result
}

/// `RepairPointers(first, second)`: the saved unit and player found again; a simple doppelganger whose unit or
/// player is gone is freed (0), one found goes back on its tile with its media; 9 when one is gone otherwise.
pub fn repair_pointers<H: DmHost + ?Sized>(h: &mut H, d: u64) -> i64 {
    let mut result = 0;
    let player = q(h, EE_UNITS_DM_DG_SAVED_PLAYER, d);
    let unit = q(h, EE_UNITS_DM_DG_SAVED_UNIT, d) as u64;
    if unit != 0 {
        qb(h, EE_UNITS_DM_DG_SET_UNIT, d, unit as i64);
        q(h, EE_UNITS_DM_DG_REFERENCE, unit);
    } else {
        qb(h, EE_UNITS_DM_DG_SET_UNIT, d, 0);
        result = REPAIR_FAILED;
    }
    let owner = qb(h, EE_UNITS_DM_PLAYER_AT, 0, player);
    if owner != 0 {
        qb(h, EE_UNITS_DM_DG_SET_PLAYER, d, owner);
    } else {
        qb(h, EE_UNITS_DM_DG_SET_PLAYER, d, 0);
        result = REPAIR_FAILED;
    }
    if result == REPAIR_FAILED {
        if q(h, EE_UNITS_DM_IS_COMPLEX, d) == 0 {
            result = 0;
            if q(h, EE_UNITS_DM_DOPPELGANGER_UNIT, d) != 0 {
                q(h, EE_UNITS_DM_DG_LOAD_MEDIA, d);
            }
            free(h, d);
        }
    } else if q(h, EE_UNITS_DM_IS_COMPLEX, d) == 0 {
        let tile = q(h, EE_UNITS_DM_DOPPELGANGER_TILE, d) as u64;
        qb(h, EE_UNITS_DM_TILE_ADD, tile, d as i64);
        q(h, EE_UNITS_DM_DG_LOAD_MEDIA, d);
    }
    result
}

/// `Save(major, minor, file)`: a valid doppelganger's complex flag, unit id and player id; the last write's result.
pub fn save<H: DmHost + ?Sized>(h: &mut H, d: u64, file: u64) -> i64 {
    q(h, EE_UNITS_DM_SET_FILE, file);
    let complex = q(h, EE_UNITS_DM_IS_COMPLEX, d);
    let mut result = 0;
    if q(h, EE_UNITS_DM_DG_IS_VALID, d) != 0 {
        result = qb(h, EE_UNITS_DM_DG_WRITE_BOOL, 0, complex);
        if result == 0 {
            let id = q(h, EE_UNITS_DM_DG_UNIT_ID, d);
            result = qb(h, EE_UNITS_DM_DG_WRITE_ULONG, 0, id);
            if result == 0 {
                let player = q(h, EE_UNITS_DM_DOPPELGANGER_PLAYER, d) as u64;
                let id = q(h, EE_UNITS_DM_PLAYER_ID, player);
                result = qb(h, EE_UNITS_DM_DG_WRITE_ULONG, 0, id);
            }
        }
    }
    result
}

/// `EEDoppelgangerManager::Release()`: both pools' statistics dumped and the pools released.
pub fn release<H: DmHost + ?Sized>(h: &mut H) {
    for op in [EE_UNITS_DM_DG_POOL_DUMP, EE_UNITS_DM_DG_POOL_RELEASE, EE_UNITS_DM_DG_COMPLEX_POOL_DUMP, EE_UNITS_DM_DG_COMPLEX_POOL_RELEASE] {
        q(h, op, 0);
    }
}

// ---- the bodies (`ee_units_dm_dop`'s `what`)
pub const EE_UNITS_DM_DG_DO_RESET: i32 = 1;
pub const EE_UNITS_DM_DG_DO_CLEAR: i32 = 2;
pub const EE_UNITS_DM_DG_DO_INITIALIZE: i32 = 3;
pub const EE_UNITS_DM_DG_DO_INITIALIZE_COPY: i32 = 4;
pub const EE_UNITS_DM_DG_DO_LOAD: i32 = 5;
pub const EE_UNITS_DM_DG_DO_REPAIR_POINTERS: i32 = 6;
pub const EE_UNITS_DM_DG_DO_SAVE: i32 = 7;
pub const EE_UNITS_DM_DG_DO_MANAGER_RELEASE: i32 = 8;

/// Runs body `what` of doppelganger `d` (`a` a unit, another doppelganger or the file; `b` the player; `c` the tile).
pub fn run<H: DmHost + ?Sized>(h: &mut H, what: i32, d: u64, a: u64, b: u64, c: u64) -> i64 {
    match what {
        EE_UNITS_DM_DG_DO_RESET => reset(h, d),
        EE_UNITS_DM_DG_DO_CLEAR => clear(h, d),
        EE_UNITS_DM_DG_DO_INITIALIZE => initialize(h, d, a, b, c),
        EE_UNITS_DM_DG_DO_INITIALIZE_COPY => initialize_copy(h, d, a, b),
        EE_UNITS_DM_DG_DO_LOAD => return load(h, d, a),
        EE_UNITS_DM_DG_DO_REPAIR_POINTERS => return repair_pointers(h, d),
        EE_UNITS_DM_DG_DO_SAVE => return save(h, d, a),
        EE_UNITS_DM_DG_DO_MANAGER_RELEASE => release(h),
        _ => {}
    }
    0
}

#[cfg(not(test))]
mod game {
    use super::*;
    use crate::units::dm_body::game::Game;
    use std::os::raw::c_void;

    /// `EEDoppelganger`'s body `what` (`EE_UNITS_DM_DG_DO_*`) of doppelganger `d`.
    ///
    /// # Safety
    /// The objects named are live, on the simulation thread.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_dm_dop(what: i32, d: *mut c_void, a: *mut c_void, b: *mut c_void, c: *mut c_void) -> i64 {
        run(&mut Game, what, d as usize as u64, a as usize as u64, b as usize as u64, c as usize as u64)
    }
}

#[cfg(test)]
#[path = "dm_dop_tests.rs"]
mod tests;
