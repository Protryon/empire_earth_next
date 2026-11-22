//! `EEGhostUnit`'s remaining bodies (the units' lane, round 9; were EEGhostUnit.cpp): the constructor's and the
//! destructor's media, `Reset`, `Load`, `Save`, `SetFarmSlotTile` and `SetAnimation`. Every C++ statement of the
//! originals is one operation of the ghost host ([`GhostHost::ghost`], `ee_units_ghost_host`: its
//! `EE_UNITS_GHOST_*` and the `EE_UNITS_GHOST_M_*` below), asked in the original's order.

use crate::units::ghost_body::{
    GhostHost, EE_UNITS_GHOST_SET_CAN_PLACE, EE_UNITS_GHOST_SET_TILE_X, EE_UNITS_GHOST_SET_TILE_Y,
    EE_UNITS_GHOST_TILE_X, EE_UNITS_GHOST_TILE_Y, EE_UNITS_GHOST_UNIT_TYPE,
};
use crate::units::ghost_make::{EE_UNITS_GHOST_CLASS_AT, EE_UNITS_GHOST_SET_X, EE_UNITS_GHOST_SET_Y, EE_UNITS_GHOST_SET_Z};

// ---- the statements (`op`; the answer)
/// the ghost's `mPlayer = (EEPlayer*)b`
pub const EE_UNITS_GHOST_M_SET_PLAYER: i32 = 4000;
/// the ghost's `mUnitClass = (EEUnitClass*)b`
pub const EE_UNITS_GHOST_M_SET_CLASS: i32 = 4001;
/// `gFreeCamera`
pub const EE_UNITS_GHOST_M_FREE_CAMERA: i32 = 4002;
/// player `a`'s `LoadUnitClassMedia(b)`
pub const EE_UNITS_GHOST_M_LOAD_CLASS_MEDIA: i32 = 4003;
/// the ghost's `mPlayer->SetUnitClassMedia(b, 0x7f)`
pub const EE_UNITS_GHOST_M_SET_CLASS_MEDIA: i32 = 4004;
/// `gDataPreLoading->PostLoadUnitClass(player a's mPlayerID, b)`
pub const EE_UNITS_GHOST_M_POST_LOAD: i32 = 4005;
/// the ghost's `mNoMedia = b != 0`
pub const EE_UNITS_GHOST_M_SET_NO_MEDIA: i32 = 4006;
/// the ghost's `mNoMedia`
pub const EE_UNITS_GHOST_M_NO_MEDIA: i32 = 4007;
/// `UShutdownInProgress()`
pub const EE_UNITS_GHOST_M_SHUTTING_DOWN: i32 = 4008;
/// `gDataPreLoading->PostUnloadUnitClass(the ghost's mPlayer->mPlayerID, its mUnitClass->GetUnitType())`
pub const EE_UNITS_GHOST_M_POST_UNLOAD: i32 = 4009;
/// the ghost's `mPlayer->UnloadUnitClassMedia(mUnitClass->GetUnitType())`
pub const EE_UNITS_GHOST_M_UNLOAD_CLASS_MEDIA: i32 = 4010;
/// the ghost's `mFrame = b`
pub const EE_UNITS_GHOST_M_SET_FRAME: i32 = 4011;
/// the ghost's `mAnimation = b`
pub const EE_UNITS_GHOST_M_SET_ANIMATION_INDEX: i32 = 4012;
/// `memset(the ghost's mFootprint, 0, ...)`
pub const EE_UNITS_GHOST_M_CLEAR_FOOTPRINT: i32 = 4013;
/// `memset(the ghost's mHeights, 0, ...)`
pub const EE_UNITS_GHOST_M_CLEAR_HEIGHTS: i32 = 4014;
/// the saved game's file `= (FSFile*)a`
pub const EE_UNITS_GHOST_M_SET_FILE: i32 = 4015;
/// the file's `Read(an unsigned long)` (kept: `EE_UNITS_GHOST_M_VALUE`)
pub const EE_UNITS_GHOST_M_READ_ULONG: i32 = 4016;
/// the unsigned long read last
pub const EE_UNITS_GHOST_M_VALUE: i32 = 4017;
/// the file's `Write((unsigned long)b)`
pub const EE_UNITS_GHOST_M_WRITE_ULONG: i32 = 4018;
/// the file's `Write(the ghost's mPlayer->GetPlayerID())`
pub const EE_UNITS_GHOST_M_WRITE_PLAYER_ID: i32 = 4019;
/// the ghost's `GetModel()->mStartFrames[b]`
pub const EE_UNITS_GHOST_M_START_FRAME: i32 = 4020;

/// `sprintf(theText, "The Ghost Unit Player Number was: %ld", mPlayer->mPlayerID)` (`theText` is the host's)
pub const EE_UNITS_GHOST_M_CHECKSUM_PLAYER_TEXT: i32 = 4021;
/// `sprintf(theText, "The Object Type was %s", mUnitClass->mName)`
pub const EE_UNITS_GHOST_M_CHECKSUM_CLASS_TEXT: i32 = 4023;
/// checksum `a`'s `AddChecksum(theText, b)`
pub const EE_UNITS_GHOST_M_CHECKSUM_ADD: i32 = 4022;

/// `Reset`'s animation
const RESET_ANIMATION: i64 = 7;

fn q<H: GhostHost + ?Sized>(h: &mut H, op: i32, g: u64) -> i64 {
    h.ghost(op, g, 0, 0, 0)
}

fn qb<H: GhostHost + ?Sized>(h: &mut H, op: i32, g: u64, b: i64) -> i64 {
    h.ghost(op, g, 0, b, 0)
}

fn qab<H: GhostHost + ?Sized>(h: &mut H, op: i32, g: u64, a: u64, b: i64) -> i64 {
    h.ghost(op, g, a, b, 0)
}

/// `Reset()`: an empty, unplaceable ghost at the origin with animation 7.
pub fn reset<H: GhostHost + ?Sized>(h: &mut H, g: u64) {
    let zero = i64::from(0.0f32.to_bits());
    qb(h, EE_UNITS_GHOST_SET_X, g, zero);
    qb(h, EE_UNITS_GHOST_SET_Y, g, zero);
    qb(h, EE_UNITS_GHOST_SET_Z, g, zero);
    qb(h, EE_UNITS_GHOST_SET_CAN_PLACE, g, 0);
    qb(h, EE_UNITS_GHOST_M_SET_PLAYER, g, 0);
    qb(h, EE_UNITS_GHOST_SET_TILE_X, g, 0);
    qb(h, EE_UNITS_GHOST_SET_TILE_Y, g, 0);
    qb(h, EE_UNITS_GHOST_M_SET_CLASS, g, 0);
    qb(h, EE_UNITS_GHOST_M_SET_FRAME, g, 0);
    qb(h, EE_UNITS_GHOST_M_SET_ANIMATION_INDEX, g, RESET_ANIMATION);
    qb(h, EE_UNITS_GHOST_M_SET_NO_MEDIA, g, 0);
    q(h, EE_UNITS_GHOST_M_CLEAR_FOOTPRINT, g);
    q(h, EE_UNITS_GHOST_M_CLEAR_HEIGHTS, g);
}

/// The constructor `EEGhostUnit(class, player, no media)`: `Reset`, the class and player, the class media loaded
/// (free camera) or queued (unless no media).
pub fn construct<H: GhostHost + ?Sized>(h: &mut H, g: u64, class: i64, player: u64, no_media: bool) {
    reset(h, g);
    let c = qb(h, EE_UNITS_GHOST_CLASS_AT, 0, class);
    qb(h, EE_UNITS_GHOST_M_SET_CLASS, g, c);
    qb(h, EE_UNITS_GHOST_M_SET_PLAYER, g, player as i64);
    if q(h, EE_UNITS_GHOST_M_FREE_CAMERA, 0) != 0 {
        qab(h, EE_UNITS_GHOST_M_LOAD_CLASS_MEDIA, g, player, class);
        qb(h, EE_UNITS_GHOST_M_SET_CLASS_MEDIA, g, class);
    } else if !no_media {
        qab(h, EE_UNITS_GHOST_M_POST_LOAD, g, player, class);
    }
    qb(h, EE_UNITS_GHOST_M_SET_NO_MEDIA, g, i64::from(no_media));
}

/// The destructor: the class media unloaded (queued unless free camera or shutting down).
pub fn destroy<H: GhostHost + ?Sized>(h: &mut H, g: u64) {
    if q(h, EE_UNITS_GHOST_M_NO_MEDIA, g) == 0 {
        if q(h, EE_UNITS_GHOST_M_FREE_CAMERA, 0) == 0 && q(h, EE_UNITS_GHOST_M_SHUTTING_DOWN, 0) == 0 {
            q(h, EE_UNITS_GHOST_M_POST_UNLOAD, g);
            return;
        }
        q(h, EE_UNITS_GHOST_M_UNLOAD_CLASS_MEDIA, g);
    }
}

/// `Load(major, minor, file)`: the class and player ids (for the repair); the first read error.
pub fn load<H: GhostHost + ?Sized>(h: &mut H, g: u64, file: u64) -> i64 {
    qab(h, EE_UNITS_GHOST_M_SET_FILE, g, file, 0);
    let mut result = q(h, EE_UNITS_GHOST_M_READ_ULONG, g);
    let value = q(h, EE_UNITS_GHOST_M_VALUE, g);
    qb(h, EE_UNITS_GHOST_M_SET_CLASS, g, value);
    if result == 0 {
        result = q(h, EE_UNITS_GHOST_M_READ_ULONG, g);
        let value = q(h, EE_UNITS_GHOST_M_VALUE, g);
        qb(h, EE_UNITS_GHOST_M_SET_PLAYER, g, value);
    }
    result
}

/// `Save(major, minor, file)`: the unit type and the player id; the first write error.
pub fn save<H: GhostHost + ?Sized>(h: &mut H, g: u64, file: u64) -> i64 {
    qab(h, EE_UNITS_GHOST_M_SET_FILE, g, file, 0);
    let unit_type = q(h, EE_UNITS_GHOST_UNIT_TYPE, g);
    let mut result = qb(h, EE_UNITS_GHOST_M_WRITE_ULONG, g, unit_type);
    if result == 0 {
        result = q(h, EE_UNITS_GHOST_M_WRITE_PLAYER_ID, g);
    }
    result
}

/// `SetFarmSlotTile(ghost, direction)`: the ghost moved to the slot tile in `direction` from its tile.
pub fn set_farm_slot_tile<H: GhostHost + ?Sized>(h: &mut H, g: u64, direction: i64) {
    let x = q(h, EE_UNITS_GHOST_TILE_X, g);
    let y = q(h, EE_UNITS_GHOST_TILE_Y, g);
    let next = slot_tile(x, y, direction);
    qb(h, EE_UNITS_GHOST_SET_TILE_X, g, i64::from(next[0]));
    qb(h, EE_UNITS_GHOST_SET_TILE_Y, g, i64::from(next[1]));
}

/// `EEGetSlotTile`'s (noted: its UUTL kind 45, as the original's call wrote)
#[cfg(not(test))]
fn slot_tile(x: i64, y: i64, direction: i64) -> [i32; 2] {
    let mut t = [0i32; 2];
    // SAFETY: a live out pair
    unsafe { crate::units::uu_ffi::ee_units_uu_slot_tile(x as i32, y as i32, direction as i32, &mut t) };
    t
}

#[cfg(test)]
fn slot_tile(x: i64, y: i64, direction: i64) -> [i32; 2] {
    crate::units::uu_tiles::slot_tile([x as i32, y as i32], direction as i32)
}

/// `SetAnimation(index)`: the animation and its first frame.
pub fn set_animation<H: GhostHost + ?Sized>(h: &mut H, g: u64, index: i64) {
    let frame = qb(h, EE_UNITS_GHOST_M_START_FRAME, g, index);
    qb(h, EE_UNITS_GHOST_M_SET_FRAME, g, frame);
    qb(h, EE_UNITS_GHOST_M_SET_ANIMATION_INDEX, g, index);
}

/// `AddChecksum(checksum)`: the ghost's player number and class name.
pub fn add_checksum<H: GhostHost + ?Sized>(h: &mut H, g: u64, checksum: u64) {
    q(h, EE_UNITS_GHOST_M_CHECKSUM_PLAYER_TEXT, g);
    qab(h, EE_UNITS_GHOST_M_CHECKSUM_ADD, g, checksum, 0);
    q(h, EE_UNITS_GHOST_M_CHECKSUM_CLASS_TEXT, g);
    qab(h, EE_UNITS_GHOST_M_CHECKSUM_ADD, g, checksum, 1);
}

// ---- the bodies (`ee_units_ghost_more`'s `what`)
pub const EE_UNITS_GHOST_M_DO_RESET: i32 = 1;
pub const EE_UNITS_GHOST_M_DO_CONSTRUCT: i32 = 2;
pub const EE_UNITS_GHOST_M_DO_DESTROY: i32 = 3;
pub const EE_UNITS_GHOST_M_DO_LOAD: i32 = 4;
pub const EE_UNITS_GHOST_M_DO_SAVE: i32 = 5;
pub const EE_UNITS_GHOST_M_DO_SET_FARM_SLOT_TILE: i32 = 6;
pub const EE_UNITS_GHOST_M_DO_SET_ANIMATION: i32 = 7;
pub const EE_UNITS_GHOST_M_DO_ADD_CHECKSUM: i32 = 8;

/// Runs body `what` on ghost `g` (`a` an object: the player or the file; `b`, `c` values).
pub fn run<H: GhostHost + ?Sized>(h: &mut H, what: i32, g: u64, a: u64, b: i64, c: i64) -> i64 {
    match what {
        EE_UNITS_GHOST_M_DO_RESET => reset(h, g),
        EE_UNITS_GHOST_M_DO_CONSTRUCT => construct(h, g, b, a, c != 0),
        EE_UNITS_GHOST_M_DO_DESTROY => destroy(h, g),
        EE_UNITS_GHOST_M_DO_LOAD => return load(h, g, a),
        EE_UNITS_GHOST_M_DO_SAVE => return save(h, g, a),
        EE_UNITS_GHOST_M_DO_SET_FARM_SLOT_TILE => set_farm_slot_tile(h, g, b),
        EE_UNITS_GHOST_M_DO_SET_ANIMATION => set_animation(h, g, b),
        EE_UNITS_GHOST_M_DO_ADD_CHECKSUM => add_checksum(h, g, a),
        _ => {}
    }
    0
}

#[cfg(not(test))]
mod game {
    use super::*;
    use crate::units::ghost_body::game::Game;
    use std::os::raw::c_void;

    /// `EEGhostUnit`'s body `what` (`EE_UNITS_GHOST_M_DO_*`) on `ghost`.
    ///
    /// # Safety
    /// `ghost` is a live `EEGhostUnit` (being made or destroyed for the constructor's and destructor's), `a` live as
    /// the body needs it.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_ghost_more(what: i32, ghost: *mut c_void, a: *mut c_void, b: i64, c: i64) -> i64 {
        run(&mut Game, what, ghost as usize as u64, a as usize as u64, b, c)
    }
}

#[cfg(test)]
#[path = "ghost_more_tests.rs"]
mod tests;
