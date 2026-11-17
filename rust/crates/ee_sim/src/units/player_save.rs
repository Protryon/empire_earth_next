//! The player's saved state (`EEPlayer::Load` 0x565cfa, `EEPlayer::Save` 0x569c46): the plain numbers as a record the
//! C++ class derives from (`class EEPlayer : public RTSPlayer, public EeUnitsPlayerSaved`, ee/EEPlayer.h; the fields
//! keep their C++ names and types, so every reader is unchanged) and both bodies as one sequence each over a host
//! callback ([`EeUnitsPlayerSaveFn`]).
//!
//! The callback does the file's primitive reads and writes with the `FSFile` overload the original called (the
//! 4-byte `long`/`unsigned long` adapters with their read-into-the-old-value semantics, `float`, `bool`) and the
//! player's C++ parts (the class upgrades, `RTSPlayer`'s record, the tech tree, the patrol, saved and hot-key
//! groups, the air traffic, the civilization, the herds, the events, the units off the map, the id lists) by
//! operation. The original's four load bodies (one per save era: minor < 0xd7, < 0xe1, < 0xe8, later) were the
//! same reads, each later era appending its own; [`load`] is that one sequence with the era's returns.
//!
//! The record is read and written through its raw pointer, never by a reference held across a host call: a
//! primitive read copies the record, reads into the copy's field and stores the copy back before the next call.
//! Kept quirks: the results the original ignored (the upgrades' count, a real upgrade's load, `RTSPlayer`'s
//! record, the saved groups, the population count, the save's air traffic and per-id writes), `theCount`,
//! `theColumns` and the scratch words carried from one read to the next, the queued improvements' loop that runs
//! on after a failed read, the tribute rows' unsigned bounds. One difference: a count from the file past an
//! array's size (resource caps 7, tributes 10, trade and refunds 5) reads its extra words into a scratch word
//! where the original wrote past the array (every save writes 7, 2 x 5 and 5).

#![allow(non_snake_case)]
// `c_ulong` and `c_long` are 32 bits on Windows and 64 elsewhere: the casts to and from the i64 slot are needed there
#![allow(clippy::unnecessary_cast)]

use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};

use crate::units::player::{load_list, save_list, EeUnitsQueued};

/// `EEPlayer`'s saved numbers (their C++ names and types).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsPlayerSaved {
    /// the scenario's player variables (zeroed together; saved)
    pub mScenarioVariables: [c_ulong; 10],
    /// non-zero: at war with that player (calamity targets; the store's diplomacy table)
    pub mEnemies: [c_ulong; 17],
    /// the tributes given and received by resource (two rows of 5)
    pub mTributes: [c_ulong; 10],
    /// 0..3 (the chat form's radio buttons 3, 2, 4, 5)
    pub mChatType: c_ulong,
    pub mCombatState: c_ulong,
    pub mGameResult: c_ulong,
    pub mFreeUnitCounter: c_ulong,
    pub mFreeUnitInterval: c_ulong,
    /// indexed by a resource (2..6 used)
    pub mResourceCaps: [c_long; 7],
    pub mPopLimit: c_long,
    pub mPopulationCount: c_long,
    pub mEndOfGameCount: c_long,
    /// written, never read back (a load reads and drops the word)
    pub mNumCapitols: c_long,
    /// indexed by a resource 2..6 as `[resource - 2]`
    pub mResourceTrade: [c_long; 5],
    pub mInsuranceRefunds: [c_long; 5],
    pub mNumVitalBuildings: c_long,
    pub mHighGroundBonus: f32,
    pub mHealTimer: f32,
    pub mSendChatMessage: [bool; 17],
    pub mSharedLOS: bool,
    pub mDisconnected: bool,
    pub mCivilizationLocked: bool,
    pub mCivilizationChosen: bool,
    pub mCivUpgradesPurchased: bool,
}

/// One operation's values: a number (`long`/`unsigned long`/an id/a count), a float, a bool (0 or 1).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsPlayerSaveIo {
    pub i: i64,
    pub f: f32,
    pub b: u8,
}

/// The host: `(ctx, op, index, io) -> the operation's result` (kErrorOK 0; a query's answer for the queries).
pub type EeUnitsPlayerSaveFn = unsafe extern "C" fn(*mut c_void, i32, u32, *mut EeUnitsPlayerSaveIo) -> i32;

// ---- the operations (the callback's `op`)
// the file's primitives: io holds the old value in, the read value out
pub const EE_UNITS_PLAYER_SAVE_READ_ULONG: i32 = 1;
pub const EE_UNITS_PLAYER_SAVE_READ_LONG: i32 = 2;
pub const EE_UNITS_PLAYER_SAVE_READ_FLOAT: i32 = 3;
pub const EE_UNITS_PLAYER_SAVE_READ_BOOL: i32 = 4;
pub const EE_UNITS_PLAYER_SAVE_WRITE_ULONG: i32 = 5;
pub const EE_UNITS_PLAYER_SAVE_WRITE_LONG: i32 = 6;
pub const EE_UNITS_PLAYER_SAVE_WRITE_FLOAT: i32 = 7;
pub const EE_UNITS_PLAYER_SAVE_WRITE_BOOL: i32 = 8;
/// the units' `unsigned short` (unit_save.rs, unit_load.rs)
pub const EE_UNITS_PLAYER_SAVE_READ_USHORT: i32 = 98;
pub const EE_UNITS_PLAYER_SAVE_WRITE_USHORT: i32 = 99;

/// Whether `op` is one of `EESavePrimitive`'s reads.
pub fn is_save_read(op: i32) -> bool {
    (EE_UNITS_PLAYER_SAVE_READ_ULONG..=EE_UNITS_PLAYER_SAVE_READ_BOOL).contains(&op)
        || op == EE_UNITS_PLAYER_SAVE_READ_USHORT
}

/// Whether `op` is one of `EESavePrimitive`'s writes.
pub fn is_save_write(op: i32) -> bool {
    (EE_UNITS_PLAYER_SAVE_WRITE_ULONG..=EE_UNITS_PLAYER_SAVE_WRITE_BOOL).contains(&op)
        || op == EE_UNITS_PLAYER_SAVE_WRITE_USHORT
}
// the load's parts
/// a stand-in upgrade of the first unit class (index: the era's assertion line)
pub const EE_UNITS_PLAYER_SAVE_UPGRADE_DUMMY_NEW: i32 = 10;
/// upgrade `io.i`'s record into the player's upgrade (0) or the stand-in (its result)
pub const EE_UNITS_PLAYER_SAVE_UPGRADE_LOAD: i32 = 11;
pub const EE_UNITS_PLAYER_SAVE_UPGRADE_DUMMY_DELETE: i32 = 12;
pub const EE_UNITS_PLAYER_SAVE_RTS_LOAD: i32 = 13;
/// `sGoalCounter = io.i`
pub const EE_UNITS_PLAYER_SAVE_SET_GOAL_COUNTER: i32 = 14;
pub const EE_UNITS_PLAYER_SAVE_TECH_DELETE: i32 = 15;
/// a new tech tree, the game's copied, bound to the player, loaded
pub const EE_UNITS_PLAYER_SAVE_TECH_LOAD: i32 = 16;
pub const EE_UNITS_PLAYER_SAVE_PATROL_CLEAR: i32 = 17;
/// a new patrol group loaded, added when it loaded, dereferenced
pub const EE_UNITS_PLAYER_SAVE_PATROL_LOAD: i32 = 18;
pub const EE_UNITS_PLAYER_SAVE_SAVED_GROUPS_LOAD: i32 = 19;
/// `EEUnitStorePlayerTable` (the store's diplomacy table)
pub const EE_UNITS_PLAYER_SAVE_STORE_PLAYER_TABLE: i32 = 20;
pub const EE_UNITS_PLAYER_SAVE_AIR_LOAD: i32 = 21;
pub const EE_UNITS_PLAYER_SAVE_ACTIVE_LOAD: i32 = 22;
pub const EE_UNITS_PLAYER_SAVE_CIV_LOAD: i32 = 23;
pub const EE_UNITS_PLAYER_SAVE_GROUP_LOAD: i32 = 24;
pub const EE_UNITS_PLAYER_SAVE_CIVFILE_LOAD: i32 = 25;
/// `mQueuedImprovements[io.i]` (made if absent): its float into `io.f`
pub const EE_UNITS_PLAYER_SAVE_QUEUED_FLOAT: i32 = 26;
/// `mQueuedImprovements[io.i].first = io.f`
pub const EE_UNITS_PLAYER_SAVE_QUEUED_SET_FLOAT: i32 = 27;
/// `FSLoadVector` into `mQueuedImprovements[io.i].second`
pub const EE_UNITS_PLAYER_SAVE_QUEUED_VECTOR_LOAD: i32 = 28;
pub const EE_UNITS_PLAYER_SAVE_CONSTRUCTION_PUSH: i32 = 29;
/// `mHerds.resize(io.i, NULL)`
pub const EE_UNITS_PLAYER_SAVE_HERDS_RESIZE: i32 = 30;
/// `mHerds[index] = new EEHerd`, loaded
pub const EE_UNITS_PLAYER_SAVE_HERD_LOAD: i32 = 31;
pub const EE_UNITS_PLAYER_SAVE_EVENTS_LOAD: i32 = 32;
pub const EE_UNITS_PLAYER_SAVE_OFFMAP_RESIZE: i32 = 33;
pub const EE_UNITS_PLAYER_SAVE_OFFMAP_LOAD: i32 = 34;
pub const EE_UNITS_PLAYER_SAVE_WONDER_PUSH: i32 = 35;
pub const EE_UNITS_PLAYER_SAVE_WONDERS_CLEAR: i32 = 36;
// the save's parts (queries answer by the result or `io.i`/`io.f`)
/// `mUnitClassUpgrades.size()` into `io.i`
pub const EE_UNITS_PLAYER_SAVE_UPGRADE_SLOTS: i32 = 40;
/// 1: upgrade `index` is not NULL
pub const EE_UNITS_PLAYER_SAVE_UPGRADE_PRESENT: i32 = 41;
pub const EE_UNITS_PLAYER_SAVE_UPGRADE_SAVE: i32 = 42;
pub const EE_UNITS_PLAYER_SAVE_RTS_SAVE: i32 = 43;
/// `sGoalCounter` into `io.i`
pub const EE_UNITS_PLAYER_SAVE_GOAL_COUNTER: i32 = 44;
/// 1: the player has a tech tree
pub const EE_UNITS_PLAYER_SAVE_TECH_PRESENT: i32 = 45;
pub const EE_UNITS_PLAYER_SAVE_TECH_SAVE: i32 = 46;
/// `mPatrolGroups.size()` into `io.i`
pub const EE_UNITS_PLAYER_SAVE_PATROL_SLOTS: i32 = 47;
/// patrol group `index`: 0 NULL, 1 a town's, 2 another
pub const EE_UNITS_PLAYER_SAVE_PATROL_KIND: i32 = 48;
pub const EE_UNITS_PLAYER_SAVE_PATROL_SAVE: i32 = 49;
pub const EE_UNITS_PLAYER_SAVE_SAVED_GROUPS_SAVE: i32 = 50;
pub const EE_UNITS_PLAYER_SAVE_AIR_SAVE: i32 = 51;
pub const EE_UNITS_PLAYER_SAVE_ACTIVE_SAVE: i32 = 52;
pub const EE_UNITS_PLAYER_SAVE_CIV_SAVE: i32 = 53;
pub const EE_UNITS_PLAYER_SAVE_GROUP_SAVE: i32 = 54;
pub const EE_UNITS_PLAYER_SAVE_CIVFILE_SAVE: i32 = 55;
/// `mQueuedImprovements.size()` into `io.i`
pub const EE_UNITS_PLAYER_SAVE_QUEUED_COUNT: i32 = 56;
/// the `index`th entry (key order): its key into `io.i`, its float into `io.f`
pub const EE_UNITS_PLAYER_SAVE_QUEUED_ENTRY: i32 = 57;
/// `FSSaveVector` of the `index`th entry's list
pub const EE_UNITS_PLAYER_SAVE_QUEUED_VECTOR_SAVE: i32 = 58;
/// `mConstructionSites->GetSize()` into `io.i`
pub const EE_UNITS_PLAYER_SAVE_CONSTRUCTION_COUNT: i32 = 59;
/// the `index`th construction site's unit id into `io.i`
pub const EE_UNITS_PLAYER_SAVE_CONSTRUCTION_ID: i32 = 60;
pub const EE_UNITS_PLAYER_SAVE_HERD_COUNT: i32 = 61;
pub const EE_UNITS_PLAYER_SAVE_HERD_SAVE: i32 = 62;
pub const EE_UNITS_PLAYER_SAVE_EVENTS_SAVE: i32 = 63;
pub const EE_UNITS_PLAYER_SAVE_OFFMAP_COUNT: i32 = 64;
pub const EE_UNITS_PLAYER_SAVE_OFFMAP_SAVE: i32 = 65;
/// `mWonders->GetSize()` into `io.i`
pub const EE_UNITS_PLAYER_SAVE_WONDER_COUNT: i32 = 66;
pub const EE_UNITS_PLAYER_SAVE_WONDER_ID: i32 = 67;
// `LoadUnit`'s and `SaveUnit`'s parts (load_unit, save_unit: their own context)
/// whether unit type `io.i`'s class fits the saved kind (`io.b`: complex)
pub const EE_UNITS_PLAYER_SAVE_UNIT_FITS: i32 = 68;
/// a unit of class `io.i` made on the player off the map, loaded, killed when it was queued for delete
pub const EE_UNITS_PLAYER_SAVE_UNIT_LOAD: i32 = 69;
/// a stand-in unit (complex: `io.b`) loaded, its id kept, repaired, cleared and dropped
pub const EE_UNITS_PLAYER_SAVE_UNIT_STANDIN: i32 = 70;
/// the unit saved: its type into `io.i`, whether it is a path point into `io.b`; the result whether it is complex
pub const EE_UNITS_PLAYER_SAVE_UNIT_KIND: i32 = 71;
/// the unit's own `Save`
pub const EE_UNITS_PLAYER_SAVE_UNIT_SAVE: i32 = 72;

const OK: i32 = 0;

/// The player's C++ side: one call per operation.
pub trait PlayerSaveHost {
    fn call(&mut self, op: i32, index: u32, io: &mut EeUnitsPlayerSaveIo) -> i32;

    fn op(&mut self, op: i32) -> i32 {
        self.call(op, 0, &mut EeUnitsPlayerSaveIo::default())
    }
    fn op_at(&mut self, op: i32, index: u32) -> i32 {
        self.call(op, index, &mut EeUnitsPlayerSaveIo::default())
    }
    fn op_with(&mut self, op: i32, value: i64) -> i32 {
        self.call(op, 0, &mut EeUnitsPlayerSaveIo { i: value, ..Default::default() })
    }
    /// A query's `io` after the call.
    fn query(&mut self, op: i32, index: u32) -> EeUnitsPlayerSaveIo {
        let mut io = EeUnitsPlayerSaveIo::default();
        self.call(op, index, &mut io);
        io
    }
    fn read_ulong(&mut self, v: &mut c_ulong) -> i32 {
        let mut io = EeUnitsPlayerSaveIo { i: *v as i64, ..Default::default() };
        let r = self.call(EE_UNITS_PLAYER_SAVE_READ_ULONG, 0, &mut io);
        *v = io.i as c_ulong;
        r
    }
    fn read_long(&mut self, v: &mut c_long) -> i32 {
        let mut io = EeUnitsPlayerSaveIo { i: *v as i64, ..Default::default() };
        let r = self.call(EE_UNITS_PLAYER_SAVE_READ_LONG, 0, &mut io);
        *v = io.i as c_long;
        r
    }
    fn read_float(&mut self, v: &mut f32) -> i32 {
        let mut io = EeUnitsPlayerSaveIo { f: *v, ..Default::default() };
        let r = self.call(EE_UNITS_PLAYER_SAVE_READ_FLOAT, 0, &mut io);
        *v = io.f;
        r
    }
    fn read_bool(&mut self, v: &mut bool) -> i32 {
        let mut io = EeUnitsPlayerSaveIo { b: u8::from(*v), ..Default::default() };
        let r = self.call(EE_UNITS_PLAYER_SAVE_READ_BOOL, 0, &mut io);
        *v = io.b != 0;
        r
    }
    fn write_ulong(&mut self, v: c_ulong) -> i32 {
        self.call(EE_UNITS_PLAYER_SAVE_WRITE_ULONG, 0, &mut EeUnitsPlayerSaveIo { i: v as i64, ..Default::default() })
    }
    fn write_long(&mut self, v: c_long) -> i32 {
        self.call(EE_UNITS_PLAYER_SAVE_WRITE_LONG, 0, &mut EeUnitsPlayerSaveIo { i: v as i64, ..Default::default() })
    }
    fn write_float(&mut self, v: f32) -> i32 {
        self.call(EE_UNITS_PLAYER_SAVE_WRITE_FLOAT, 0, &mut EeUnitsPlayerSaveIo { f: v, ..Default::default() })
    }
    fn write_bool(&mut self, v: bool) -> i32 {
        self.call(EE_UNITS_PLAYER_SAVE_WRITE_BOOL, 0, &mut EeUnitsPlayerSaveIo { b: u8::from(v), ..Default::default() })
    }
}

/// The record behind its raw pointer: copied out for each access, stored back after a primitive read.
#[derive(Clone, Copy)]
pub struct Rec(*mut EeUnitsPlayerSaved);

impl Rec {
    /// # Safety
    /// `p` points to a live record for the body's whole run, written meanwhile only by the C++ it calls.
    pub unsafe fn new(p: *mut EeUnitsPlayerSaved) -> Self {
        Rec(p)
    }
    fn get(self) -> EeUnitsPlayerSaved {
        // SAFETY: Rec::new's contract; no reference to the record outlives this copy
        unsafe { *self.0 }
    }
    fn put(self, v: &EeUnitsPlayerSaved) {
        // SAFETY: as get
        unsafe { *self.0 = *v }
    }
    /// One primitive read into a field of the record (the host call reads the file only).
    fn read<H: PlayerSaveHost + ?Sized>(
        self,
        h: &mut H,
        f: impl FnOnce(&mut H, &mut EeUnitsPlayerSaved) -> i32,
    ) -> i32 {
        let mut copy = self.get();
        let r = f(h, &mut copy);
        self.put(&copy);
        r
    }
}

/// `EESaveHasLiveState` (ee/EESaveGameFiles.h): from 0.254 a save keeps every patrol group in order.
fn has_live_state(major: u32, minor: u32) -> bool {
    major > 0 || minor >= 254
}

/// `EEPlayer::Load`: the player's saved state; the read status (some reads' errors ignored, as in the original).
pub fn load<H: PlayerSaveHost + ?Sized>(h: &mut H, rec: Rec, major: u32, minor: u32) -> i32 {
    // the original's four bodies: era 1 < 0xd7, 2 < 0xe1, 3 < 0xe8, 4 the rest (any later major)
    let era = match (major, minor) {
        (0, m) if m < 0xd7 => 1,
        (0, m) if m < 0xe1 => 2,
        (0, m) if m < 0xe8 => 3,
        _ => 4,
    };
    let verify_line: u32 = [4282, 4610, 4960, 5331][era - 1];
    let mut rows: c_long = 0;
    let mut columns: c_long = 0;
    let mut count: c_ulong = 0;
    let mut goal_counter: c_ulong = 0;
    let mut format_version: c_ulong = 0;
    let mut id: c_ulong = 0;
    let mut scratch_long: c_long = 0;
    let mut scratch_ulong: c_ulong = 0;
    let mut result = OK;

    let mut i: c_ulong = 0;
    while i < 10 && result == OK {
        result = rec.read(h, |h, s| h.read_ulong(&mut s.mScenarioVariables[i as usize]));
        i += 1;
    }
    // the class upgrades: a real one's result is ignored, the stand-in's ends the loop
    h.read_ulong(&mut count);
    let mut upgrade_result = h.read_ulong(&mut format_version);
    h.op_at(EE_UNITS_PLAYER_SAVE_UPGRADE_DUMMY_NEW, verify_line);
    i = 0;
    while upgrade_result == OK && i < count {
        upgrade_result = h.read_ulong(&mut id);
        if upgrade_result == OK {
            upgrade_result = h.op_with(EE_UNITS_PLAYER_SAVE_UPGRADE_LOAD, id as i64);
        }
        i += 1;
    }
    h.op(EE_UNITS_PLAYER_SAVE_UPGRADE_DUMMY_DELETE);
    h.op(EE_UNITS_PLAYER_SAVE_RTS_LOAD);
    result = h.read_ulong(&mut goal_counter);
    if result == OK {
        h.op_with(EE_UNITS_PLAYER_SAVE_SET_GOAL_COUNTER, goal_counter as i64);
    }
    h.op(EE_UNITS_PLAYER_SAVE_TECH_DELETE);
    let mut have_tech_tree = false;
    result = h.read_bool(&mut have_tech_tree);
    if result == OK {
        if have_tech_tree {
            result = h.op(EE_UNITS_PLAYER_SAVE_TECH_LOAD);
        }
        if result == OK {
            result = rec.read(h, |h, s| h.read_long(&mut s.mPopLimit));
        }
        if result == OK {
            result = h.read_ulong(&mut count);
        }
        if result == OK {
            i = 0;
            while i < count && result == OK {
                result = if i < 7 {
                    rec.read(h, |h, s| h.read_long(&mut s.mResourceCaps[i as usize]))
                } else {
                    h.read_long(&mut scratch_long)
                };
                i += 1;
            }
            if result == OK {
                let mut patrol_result = h.read_ulong(&mut count);
                if patrol_result == OK && count > 0 {
                    h.op(EE_UNITS_PLAYER_SAVE_PATROL_CLEAR);
                    i = 0;
                    while i < count && patrol_result == OK {
                        patrol_result = h.op(EE_UNITS_PLAYER_SAVE_PATROL_LOAD);
                        i += 1;
                    }
                }
            }
        }
    }
    // the original discards these results, even after a failed record
    h.op(EE_UNITS_PLAYER_SAVE_SAVED_GROUPS_LOAD);
    rec.read(h, |h, s| h.read_long(&mut s.mPopulationCount));
    result = rec.read(h, |h, s| h.read_long(&mut s.mEndOfGameCount));
    i = 0;
    while i < 17 && result == OK {
        result = rec.read(h, |h, s| h.read_ulong(&mut s.mEnemies[i as usize]));
        if result == OK {
            result = rec.read(h, |h, s| h.read_bool(&mut s.mSendChatMessage[i as usize]));
        }
        i += 1;
    }
    h.op(EE_UNITS_PLAYER_SAVE_STORE_PLAYER_TABLE);
    if result == OK {
        result = h.read_ulong(&mut count);
    }
    if result == OK {
        i = 0;
        while i < count && result == OK {
            result = h.op_at(EE_UNITS_PLAYER_SAVE_AIR_LOAD, i as u32);
            i += 1;
        }
        if result == OK {
            result = h.op(EE_UNITS_PLAYER_SAVE_ACTIVE_LOAD);
        }
        if result == OK {
            result = rec.read(h, |h, s| h.read_ulong(&mut s.mCombatState));
        }
        if result == OK {
            // the saved mNumCapitols word, read and dropped
            result = h.read_ulong(&mut format_version);
        }
        if result == OK {
            result = rec.read(h, |h, s| h.read_bool(&mut s.mSharedLOS));
        }
        if result == OK {
            result = rec.read(h, |h, s| h.read_ulong(&mut s.mChatType));
        }
        if result == OK {
            result = h.op(EE_UNITS_PLAYER_SAVE_CIV_LOAD);
        }
    }
    for g in 0..20 {
        if result == OK {
            result = h.op_at(EE_UNITS_PLAYER_SAVE_GROUP_LOAD, g);
        }
    }
    if result == OK {
        result = h.op(EE_UNITS_PLAYER_SAVE_CIVFILE_LOAD);
    }
    if result == OK {
        result = h.read_ulong(&mut count);
    }
    if result == OK {
        // no result test in the loop's condition: a failed read goes on to the next entry
        i = 0;
        while i < count {
            result = h.read_ulong(&mut id);
            if result == OK {
                let mut io = EeUnitsPlayerSaveIo { i: id as i64, ..Default::default() };
                h.call(EE_UNITS_PLAYER_SAVE_QUEUED_FLOAT, 0, &mut io);
                let mut first = io.f;
                result = h.read_float(&mut first);
                h.call(
                    EE_UNITS_PLAYER_SAVE_QUEUED_SET_FLOAT,
                    0,
                    &mut EeUnitsPlayerSaveIo { i: id as i64, f: first, b: 0 },
                );
                if result == OK {
                    result = h.op_with(EE_UNITS_PLAYER_SAVE_QUEUED_VECTOR_LOAD, id as i64);
                }
            }
            i += 1;
        }
    }
    if result == OK {
        result = h.read_ulong(&mut count);
    }
    if result == OK {
        i = 0;
        while i < count && result == OK {
            let mut unit_id: c_long = 0;
            result = h.read_long(&mut unit_id);
            if result == OK {
                h.op_with(EE_UNITS_PLAYER_SAVE_CONSTRUCTION_PUSH, unit_id as i64);
            }
            i += 1;
        }
    }
    if result == OK {
        count = 0;
        result = h.read_ulong(&mut count);
        if result == OK {
            h.op_with(EE_UNITS_PLAYER_SAVE_HERDS_RESIZE, count as i64);
            i = 0;
            while i < count && result == OK {
                result = h.op_at(EE_UNITS_PLAYER_SAVE_HERD_LOAD, i as u32);
                i += 1;
            }
        }
    }
    if result == OK {
        result = h.read_long(&mut rows);
        if result == OK {
            result = h.read_long(&mut columns);
            // `unsigned long r < long theRows`: the bounds compare unsigned
            let mut r: c_ulong = 0;
            while r < rows as c_ulong {
                let mut c: c_ulong = 0;
                while c < columns as c_ulong && result == OK {
                    let at = r.wrapping_mul(5).wrapping_add(c);
                    result = if at < 10 {
                        rec.read(h, |h, s| h.read_ulong(&mut s.mTributes[at as usize]))
                    } else {
                        h.read_ulong(&mut scratch_ulong)
                    };
                    c += 1;
                }
                r = r.wrapping_add(1);
            }
        }
    }
    if result == OK {
        result = rec.read(h, |h, s| h.read_ulong(&mut s.mGameResult));
    }
    if result == OK {
        result = h.op(EE_UNITS_PLAYER_SAVE_EVENTS_LOAD);
    }
    if result == OK {
        result = rec.read(h, |h, s| h.read_float(&mut s.mHighGroundBonus));
    }
    if result == OK {
        result = rec.read(h, |h, s| h.read_float(&mut s.mHealTimer));
    }
    if result == OK {
        result = rec.read(h, |h, s| h.read_bool(&mut s.mDisconnected));
    }
    if result == OK {
        result = rec.read(h, |h, s| h.read_bool(&mut s.mCivilizationLocked));
    }
    if result == OK {
        result = rec.read(h, |h, s| h.read_bool(&mut s.mCivilizationChosen));
    }
    if result == OK {
        result = rec.read(h, |h, s| h.read_bool(&mut s.mCivUpgradesPurchased));
    }
    if era == 1 {
        return result;
    }
    if result == OK {
        count = 0;
        result = h.read_ulong(&mut count);
        if result == OK {
            h.op_with(EE_UNITS_PLAYER_SAVE_OFFMAP_RESIZE, count as i64);
            i = 0;
            while i < count && result == OK {
                result = h.op_at(EE_UNITS_PLAYER_SAVE_OFFMAP_LOAD, i as u32);
                i += 1;
            }
        }
    }
    if era == 2 {
        return result;
    }
    if result == OK {
        count = 0;
        result = h.read_ulong(&mut count);
        if result == OK {
            i = 0;
            while i < count && result == OK {
                let mut unit_id: c_long = 0;
                result = h.read_long(&mut unit_id);
                if result == OK {
                    h.op_with(EE_UNITS_PLAYER_SAVE_WONDER_PUSH, unit_id as i64);
                }
                i += 1;
            }
        }
    }
    if major == 0 && minor < 0xe2 {
        h.op(EE_UNITS_PLAYER_SAVE_WONDERS_CLEAR);
    }
    if era == 3 {
        return result;
    }
    if major == 0 && minor < 0xe9 {
        let mut s = rec.get();
        s.mFreeUnitCounter = 0;
        rec.put(&s);
    } else {
        result = rec.read(h, |h, s| h.read_ulong(&mut s.mFreeUnitCounter));
    }
    if result == OK {
        if major != 0 || minor > 0xf0 {
            i = 0;
            while i < columns as c_ulong && result == OK {
                result = if i < 5 {
                    rec.read(h, |h, s| h.read_long(&mut s.mResourceTrade[i as usize]))
                } else {
                    h.read_long(&mut scratch_long)
                };
                i += 1;
            }
        }
        if major != 0 || minor > 0xf1 {
            i = 0;
            while i < columns as c_ulong && result == OK {
                result = if i < 5 {
                    rec.read(h, |h, s| h.read_long(&mut s.mInsuranceRefunds[i as usize]))
                } else {
                    h.read_long(&mut scratch_long)
                };
                i += 1;
            }
        }
        if result == OK && (major != 0 || minor >= 0xf5) {
            result = rec.read(h, |h, s| h.read_long(&mut s.mNumVitalBuildings));
        }
        if result == OK && (major != 0 || minor > 0xf5) {
            result = rec.read(h, |h, s| h.read_ulong(&mut s.mFreeUnitInterval));
        }
    }
    result
}

/// `EEPlayer::Save`: the player's state; the write status (some writes' errors ignored, as in the original).
pub fn save<H: PlayerSaveHost + ?Sized>(h: &mut H, rec: Rec, major: u32, minor: u32) -> i32 {
    let mut result = OK;
    let mut i: u32 = 0;
    while i < 10 && result == OK {
        result = h.write_ulong(rec.get().mScenarioVariables[i as usize]);
        i += 1;
    }
    let slots = h.query(EE_UNITS_PLAYER_SAVE_UPGRADE_SLOTS, 0).i as u64;
    let mut count: u64 = 0;
    for j in 0..slots {
        if h.op_at(EE_UNITS_PLAYER_SAVE_UPGRADE_PRESENT, j as u32) != 0 {
            count += 1;
        }
    }
    h.write_ulong(count as c_ulong);
    result = h.write_ulong(0x10);
    if result == OK {
        let mut u: u64 = 0;
        while u < slots && result == OK {
            if h.op_at(EE_UNITS_PLAYER_SAVE_UPGRADE_PRESENT, u as u32) != 0 {
                result = h.write_ulong(u as c_ulong);
                if result == OK {
                    result = h.op_at(EE_UNITS_PLAYER_SAVE_UPGRADE_SAVE, u as u32);
                }
            }
            u += 1;
        }
    }
    h.op(EE_UNITS_PLAYER_SAVE_RTS_SAVE);
    let goal_counter = h.query(EE_UNITS_PLAYER_SAVE_GOAL_COUNTER, 0).i;
    result = h.write_ulong(goal_counter as c_ulong);
    if result == OK {
        let have_tech_tree = h.op(EE_UNITS_PLAYER_SAVE_TECH_PRESENT) != 0;
        result = h.write_bool(have_tech_tree);
    }
    if result == OK {
        // the tree's record when it has one (the host answers OK without)
        result = h.op(EE_UNITS_PLAYER_SAVE_TECH_SAVE);
        if result == OK {
            result = h.write_long(rec.get().mPopLimit);
        }
        if result == OK {
            result = h.write_ulong(7);
            if result == OK {
                let mut r = 0;
                while r < 7 && result == OK {
                    result = h.write_long(rec.get().mResourceCaps[r]);
                    r += 1;
                }
            }
            let patrols = h.query(EE_UNITS_PLAYER_SAVE_PATROL_SLOTS, 0).i as u64;
            if result == OK && has_live_state(major, minor) {
                // port (0.254): every patrol group, the towns' too, in their order
                let n = (0..patrols).filter(|&p| h.op_at(EE_UNITS_PLAYER_SAVE_PATROL_KIND, p as u32) != 0).count();
                result = h.write_ulong(n as c_ulong);
                let mut p = 0;
                while p < patrols && result == OK {
                    if h.op_at(EE_UNITS_PLAYER_SAVE_PATROL_KIND, p as u32) != 0 {
                        result = h.op_at(EE_UNITS_PLAYER_SAVE_PATROL_SAVE, p as u32);
                    }
                    p += 1;
                }
            } else if result == OK {
                // the original: the towns' groups left out, the others written backwards
                let n =
                    (0..patrols).rev().filter(|&p| h.op_at(EE_UNITS_PLAYER_SAVE_PATROL_KIND, p as u32) == 2).count();
                result = h.write_ulong(n as c_ulong);
                if result == OK && n > 0 {
                    let mut p = patrols;
                    while p > 0 && result == OK {
                        p -= 1;
                        if h.op_at(EE_UNITS_PLAYER_SAVE_PATROL_KIND, p as u32) == 2 {
                            result = h.op_at(EE_UNITS_PLAYER_SAVE_PATROL_SAVE, p as u32);
                        }
                    }
                }
            }
        }
    }
    h.op(EE_UNITS_PLAYER_SAVE_SAVED_GROUPS_SAVE);
    h.write_long(rec.get().mPopulationCount);
    h.write_long(rec.get().mEndOfGameCount);
    for e in 0..17 {
        h.write_ulong(rec.get().mEnemies[e]);
        h.write_bool(rec.get().mSendChatMessage[e]);
    }
    result = h.write_ulong(4);
    if result != OK {
        return result;
    }
    i = 0;
    while i < 4 && result == OK {
        result = h.op_at(EE_UNITS_PLAYER_SAVE_AIR_SAVE, i);
        i += 1;
    }
    if result == OK {
        result = h.op(EE_UNITS_PLAYER_SAVE_ACTIVE_SAVE);
    }
    if result == OK {
        result = h.write_ulong(rec.get().mCombatState);
    }
    if result == OK {
        result = h.write_long(rec.get().mNumCapitols);
    }
    if result == OK {
        result = h.write_bool(rec.get().mSharedLOS);
    }
    if result == OK {
        result = h.write_ulong(rec.get().mChatType);
    }
    if result == OK {
        result = h.op(EE_UNITS_PLAYER_SAVE_CIV_SAVE);
    }
    if result == OK {
        for g in 0..20 {
            if result == OK {
                result = h.op_at(EE_UNITS_PLAYER_SAVE_GROUP_SAVE, g);
            }
        }
    }
    if result == OK {
        result = h.op(EE_UNITS_PLAYER_SAVE_CIVFILE_SAVE);
    }
    if result == OK {
        let size = h.query(EE_UNITS_PLAYER_SAVE_QUEUED_COUNT, 0).i;
        result = h.write_ulong(size as c_ulong);
        if result == OK {
            for k in 0..size as u32 {
                let entry = h.query(EE_UNITS_PLAYER_SAVE_QUEUED_ENTRY, k);
                result = h.write_ulong(entry.i as c_ulong);
                if result != OK {
                    continue;
                }
                result = h.write_float(entry.f);
                if result == OK {
                    result = h.op_at(EE_UNITS_PLAYER_SAVE_QUEUED_VECTOR_SAVE, k);
                }
            }
        }
    }
    if result == OK {
        let size = h.query(EE_UNITS_PLAYER_SAVE_CONSTRUCTION_COUNT, 0).i;
        result = h.write_ulong(size as c_ulong);
        if result == OK {
            // every id written, the last write's result kept
            for k in 0..size as u32 {
                let unit_id = h.query(EE_UNITS_PLAYER_SAVE_CONSTRUCTION_ID, k).i;
                result = h.write_ulong(unit_id as c_ulong);
            }
        }
    }
    if result == OK {
        let herds = h.query(EE_UNITS_PLAYER_SAVE_HERD_COUNT, 0).i as u64;
        result = h.write_ulong(herds as c_ulong);
        let mut k = 0;
        while k < herds && result == OK {
            result = h.op_at(EE_UNITS_PLAYER_SAVE_HERD_SAVE, k as u32);
            k += 1;
        }
    }
    if result == OK {
        result = h.write_ulong(2);
    }
    if result == OK {
        result = h.write_ulong(5);
        for direction in 0..2 {
            let mut c = 0;
            while c < 5 && result == OK {
                result = h.write_ulong(rec.get().mTributes[direction * 5 + c]);
                c += 1;
            }
        }
    }
    if result == OK {
        result = h.write_ulong(rec.get().mGameResult);
    }
    if result == OK {
        result = h.op(EE_UNITS_PLAYER_SAVE_EVENTS_SAVE);
    }
    if result == OK {
        result = h.write_float(rec.get().mHighGroundBonus);
    }
    if result == OK {
        result = h.write_float(rec.get().mHealTimer);
    }
    if result == OK {
        result = h.write_bool(rec.get().mDisconnected);
    }
    if result == OK {
        result = h.write_bool(rec.get().mCivilizationLocked);
    }
    if result == OK {
        result = h.write_bool(rec.get().mCivilizationChosen);
    }
    if result == OK {
        result = h.write_bool(rec.get().mCivUpgradesPurchased);
    }
    if result == OK {
        let offmap = h.query(EE_UNITS_PLAYER_SAVE_OFFMAP_COUNT, 0).i as u64;
        result = h.write_ulong(offmap as c_ulong);
        let mut k = 0;
        while k < offmap && result == OK {
            result = h.op_at(EE_UNITS_PLAYER_SAVE_OFFMAP_SAVE, k as u32);
            k += 1;
        }
    }
    if result == OK {
        let wonders = h.query(EE_UNITS_PLAYER_SAVE_WONDER_COUNT, 0).i as u64;
        result = h.write_ulong(wonders as c_ulong);
        let mut k = 0;
        while k < wonders && result == OK {
            let unit_id = h.query(EE_UNITS_PLAYER_SAVE_WONDER_ID, k as u32).i;
            result = h.write_ulong(unit_id as c_ulong);
            k += 1;
        }
    }
    if result == OK {
        result = h.write_ulong(rec.get().mFreeUnitCounter);
    }
    if result == OK {
        let mut k = 0;
        while k < 5 && result == OK {
            result = h.write_long(rec.get().mResourceTrade[k]);
            k += 1;
        }
    }
    if result == OK {
        let mut k = 0;
        while k < 5 && result == OK {
            result = h.write_long(rec.get().mInsuranceRefunds[k]);
            k += 1;
        }
    }
    if result == OK {
        result = h.write_long(rec.get().mNumVitalBuildings);
    }
    if result == OK {
        result = h.write_ulong(rec.get().mFreeUnitInterval);
    }
    result
}

/// `EEPlayer::LoadUnit`: the unit's type, from 0.223 whether it was saved (none: done), whether it is complex, then the
/// unit: made on the player when its class fits the kind, else loaded into a stand-in whose id is kept. (The original's
/// two bodies by era, the earlier one passing major 0, which is its major.)
pub fn load_unit<H: PlayerSaveHost + ?Sized>(h: &mut H, major: u32, minor: u32) -> i32 {
    let mut unit_type: c_ulong = 0;
    let mut result = h.read_ulong(&mut unit_type);
    if result != OK {
        return result;
    }
    if !(major == 0 && minor < 0xdf) {
        let mut saved = true;
        result = h.read_bool(&mut saved);
        if !saved || result != OK {
            return result;
        }
    }
    let mut complex = false;
    result = h.read_bool(&mut complex);
    if result != OK {
        return result;
    }
    let mut io = EeUnitsPlayerSaveIo { i: unit_type as i64, b: u8::from(complex), f: 0.0 };
    let fits = h.call(EE_UNITS_PLAYER_SAVE_UNIT_FITS, 0, &mut io) != 0;
    h.call(if fits { EE_UNITS_PLAYER_SAVE_UNIT_LOAD } else { EE_UNITS_PLAYER_SAVE_UNIT_STANDIN }, 0, &mut io)
}

/// `EEPlayer::SaveUnit`: the type, whether it is saved (not a path point before 0.254's live state), whether it is
/// complex, then the unit's own save.
pub fn save_unit<H: PlayerSaveHost + ?Sized>(h: &mut H, major: u32, minor: u32) -> i32 {
    let mut io = EeUnitsPlayerSaveIo::default();
    let complex = h.call(EE_UNITS_PLAYER_SAVE_UNIT_KIND, 0, &mut io) != 0;
    let mut result = h.write_ulong(io.i as c_ulong);
    if result != OK {
        return result;
    }
    let saved = io.b == 0 || has_live_state(major, minor);
    result = h.write_bool(saved);
    if !saved {
        return result;
    }
    result = h.write_bool(complex);
    if result == OK {
        result = h.op(EE_UNITS_PLAYER_SAVE_UNIT_SAVE);
    }
    result
}

// ---- FFI

struct FfiHost {
    ctx: *mut c_void,
    f: EeUnitsPlayerSaveFn,
    /// the player's queued improvements (player.rs): the QUEUED operations are answered here
    queued: *mut EeUnitsQueued,
}

impl FfiHost {
    fn prim(&mut self, op: i32, io: &mut EeUnitsPlayerSaveIo) -> i32 {
        // SAFETY: the C++ callback with its own context; io lives for the call
        unsafe { (self.f)(self.ctx, op, 0, io) }
    }
}

impl PlayerSaveHost for FfiHost {
    fn call(&mut self, op: i32, index: u32, io: &mut EeUnitsPlayerSaveIo) -> i32 {
        if !self.queued.is_null() {
            // SAFETY: the player's live queue; the C++ primitives never reach it
            let q = unsafe { &mut *self.queued };
            let nth = |q: &mut EeUnitsQueued| {
                q.map.iter_mut().nth(index as usize).map(|(k, e)| (*k, e as *mut (f32, Vec<c_ulong>)))
            };
            match op {
                EE_UNITS_PLAYER_SAVE_QUEUED_FLOAT => {
                    io.f = q.entry(io.i as c_ulong).0;
                    return OK;
                }
                EE_UNITS_PLAYER_SAVE_QUEUED_SET_FLOAT => {
                    q.entry(io.i as c_ulong).0 = io.f;
                    return OK;
                }
                EE_UNITS_PLAYER_SAVE_QUEUED_VECTOR_LOAD => {
                    let list = &mut q.entry(io.i as c_ulong).1 as *mut Vec<c_ulong>;
                    // SAFETY: the entry lives across the reads (nothing else touches the queue)
                    return load_list(unsafe { &mut *list }, |op, io| self.prim(op, io));
                }
                EE_UNITS_PLAYER_SAVE_QUEUED_COUNT => {
                    io.i = q.map.len() as i64;
                    return OK;
                }
                EE_UNITS_PLAYER_SAVE_QUEUED_ENTRY => {
                    if let Some((key, entry)) = nth(q) {
                        io.i = key as i64;
                        // SAFETY: the entry just found
                        io.f = unsafe { (*entry).0 };
                    }
                    return OK;
                }
                EE_UNITS_PLAYER_SAVE_QUEUED_VECTOR_SAVE => {
                    let Some((_, entry)) = nth(q) else { return OK };
                    // SAFETY: as the load
                    return save_list(unsafe { &(*entry).1 }, |op, io| self.prim(op, io));
                }
                _ => {}
            }
        }
        // SAFETY: the C++ callback with its own context; io lives for the call
        unsafe { (self.f)(self.ctx, op, index, io) }
    }
}

/// `EEPlayer::Load`: the record `rec` (the player's `EeUnitsPlayerSaved` base), the queue `queued` (answered here)
/// and its C++ parts through `f`.
///
/// # Safety
/// `rec` points to the player's live record, `queued` to its queue (or is null: `f` answers it); `f` is valid with
/// `ctx` for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_units_player_load(
    ctx: *mut c_void,
    f: EeUnitsPlayerSaveFn,
    rec: *mut EeUnitsPlayerSaved,
    queued: *mut EeUnitsQueued,
    major: u32,
    minor: u32,
) -> i32 {
    load(&mut FfiHost { ctx, f, queued }, Rec::new(rec), major, minor)
}

/// `EEPlayer::Save`: as [`ee_units_player_load`].
///
/// # Safety
/// As [`ee_units_player_load`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_player_save(
    ctx: *mut c_void,
    f: EeUnitsPlayerSaveFn,
    rec: *mut EeUnitsPlayerSaved,
    queued: *mut EeUnitsQueued,
    major: u32,
    minor: u32,
) -> i32 {
    save(&mut FfiHost { ctx, f, queued }, Rec::new(rec), major, minor)
}

/// `EEPlayer::LoadUnit` (`save` false) or `SaveUnit` (see [`load_unit`], [`save_unit`]) through `f` on `ctx`.
///
/// # Safety
/// `f` is valid with `ctx` for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_units_player_unit(
    ctx: *mut c_void,
    f: EeUnitsPlayerSaveFn,
    save_it: bool,
    major: u32,
    minor: u32,
) -> i32 {
    let mut h = FfiHost { ctx, f, queued: std::ptr::null_mut() };
    if save_it {
        save_unit(&mut h, major, minor)
    } else {
        load_unit(&mut h, major, minor)
    }
}

#[cfg(test)]
#[path = "player_save_tests.rs"]
mod tests;
