//! TEST ONLY: the scenario manager's capture records (ee/EEScenarioCapture.h; tools/sim_capture.py --profile
//! scenario), built here over the world's reads by operation (`EE_SCENARIO_CAP_*`, each one C++ statement in
//! `Scenario Manager/EEScenarioCapture.cpp`'s `ee_scenariocpp_capture`). Moved whole from EEScenarioCapture.cpp in
//! the scenario editor lane's round 3; the records' bytes are the C++'s (golden_cpp_scenario.txt fixes them).
//!
//! The records: SMCE (a condition's Evaluate), SMTE (a trigger's), SMOB (an object's), SMCP (a player choice), SMAR
//! (an area query), SMEX and SMEW (an effect's run and its world after), SMSV (the manager's saved bytes), SMUP (a
//! trigger round), SMPA (Set Player Attribute), SMTK (a message's tokens), SMFP (a placement), SMCU (a unit's or a
//! class's change). Reads only: nothing here changes the game (the out-of-synch sum's last value is restored as the
//! C++ did).

// the records keep the C++ member names; c_long/c_ulong are 32 bits on Windows
#![allow(non_snake_case, clippy::unnecessary_cast)]

use std::cell::Cell;
use std::ffi::{c_char, CStr};
use std::os::raw::{c_long, c_ulong};

use crate::app::capture_record::{fnv, Record, FNV_BASIS};
use crate::scenario::fields::{
    EeScenarioConditionFields, EeScenarioEffectFields, EeScenarioObjectFields, EeScenarioTriggerFields,
};
use crate::scenario::manager::{EeScenarioManagerFields, EeScenarioTimedQueue};
use crate::world::stats_fields::EeUnitsUnitClassUpgradeFields;
use ee_base::fs::EeFsMemFile;

// ---- the reads (`ee_scenariocpp_capture(op, p, a, b)`): `p` an object's address, `a` and `b` numbers; a text is
// its C string's address; the float reads are `ee_scenariocpp_capture_f(op, p)`'s

/// `EESMCaptureSeed()`: the physics random seed (0 without physics).
pub const EE_SCENARIO_CAP_SEED: u32 = 0;
/// `gWorld.mScenarioManager`.
pub const EE_SCENARIO_CAP_MANAGER: u32 = 1;
/// The manager's lists: `mTriggers`, `mConditions`, `mEffects`, `mObjects`, `mAreas`, `mPendingEffects`: the size
/// (`p` the manager) and element `a` (`_AT`).
pub const EE_SCENARIO_CAP_TRIGGERS: u32 = 10;
pub const EE_SCENARIO_CAP_TRIGGER_AT: u32 = 11;
pub const EE_SCENARIO_CAP_CONDITIONS: u32 = 12;
pub const EE_SCENARIO_CAP_CONDITION_AT: u32 = 13;
pub const EE_SCENARIO_CAP_EFFECTS: u32 = 14;
pub const EE_SCENARIO_CAP_EFFECT_AT: u32 = 15;
pub const EE_SCENARIO_CAP_OBJECTS: u32 = 16;
pub const EE_SCENARIO_CAP_OBJECT_AT: u32 = 17;
pub const EE_SCENARIO_CAP_AREAS: u32 = 18;
pub const EE_SCENARIO_CAP_AREA_AT: u32 = 19;
pub const EE_SCENARIO_CAP_PENDING: u32 = 20;
pub const EE_SCENARIO_CAP_PENDING_AT: u32 = 21;
/// `p`'s `Save(b >> 32, (uint32_t)b, *file a)`: a trigger, condition, effect, object, area, the manager, a
/// player's `mSavedGroups`.
pub const EE_SCENARIO_CAP_SAVE_TRIGGER: u32 = 30;
pub const EE_SCENARIO_CAP_SAVE_CONDITION: u32 = 31;
pub const EE_SCENARIO_CAP_SAVE_EFFECT: u32 = 32;
pub const EE_SCENARIO_CAP_SAVE_OBJECT: u32 = 33;
pub const EE_SCENARIO_CAP_SAVE_AREA: u32 = 34;
pub const EE_SCENARIO_CAP_SAVE_MANAGER: u32 = 35;
pub const EE_SCENARIO_CAP_SAVE_GROUPS: u32 = 36;
/// `new FSMemoryFile(p, false)` over a Rust memory file `p` (an `EeFsMemFile*` the record owns), its `delete`.
pub const EE_SCENARIO_CAP_FILE_NEW: u32 = 40;
pub const EE_SCENARIO_CAP_FILE_FREE: u32 = 42;
/// The manager's record (`EeScenarioManagerFields*`), `mLastChatText`, `mMediaStrings.GetSection(a)`,
/// `mMessages.size()`, `mTimedEffects`; the camera's `mKeys.size()` and `mPathIndex`.
pub const EE_SCENARIO_CAP_MANAGER_FIELDS: u32 = 50;
pub const EE_SCENARIO_CAP_LAST_CHAT: u32 = 51;
pub const EE_SCENARIO_CAP_MEDIA_SECTION: u32 = 52;
pub const EE_SCENARIO_CAP_MESSAGES: u32 = 53;
pub const EE_SCENARIO_CAP_TIMED: u32 = 54;
pub const EE_SCENARIO_CAP_CAMERA_KEYS: u32 = 55;
pub const EE_SCENARIO_CAP_CAMERA_PATH: u32 = 56;
/// A condition's record and `mChatText`; a trigger's record, `mConditions.size()`, `mEffects.size()`; an object's
/// record, `mUnits.size()`, `mUnits[a]`; an effect's record.
pub const EE_SCENARIO_CAP_CONDITION_FIELDS: u32 = 60;
pub const EE_SCENARIO_CAP_CONDITION_CHAT: u32 = 61;
pub const EE_SCENARIO_CAP_TRIGGER_FIELDS: u32 = 62;
pub const EE_SCENARIO_CAP_TRIGGER_CONDITIONS: u32 = 63;
pub const EE_SCENARIO_CAP_TRIGGER_EFFECTS: u32 = 64;
pub const EE_SCENARIO_CAP_OBJECT_FIELDS: u32 = 65;
pub const EE_SCENARIO_CAP_OBJECT_UNITS: u32 = 66;
pub const EE_SCENARIO_CAP_OBJECT_UNIT: u32 = 67;
pub const EE_SCENARIO_CAP_EFFECT_FIELDS: u32 = 68;
/// An area's `mID`, `mSelectMode`, `mAnchor.mX`, `.mY`, `mRect`'s left, top, right, bottom (floats).
pub const EE_SCENARIO_CAP_AREA_ID: u32 = 70;
pub const EE_SCENARIO_CAP_AREA_SELECT: u32 = 71;
pub const EE_SCENARIO_CAP_AREA_ANCHOR_X: u32 = 72;
pub const EE_SCENARIO_CAP_AREA_ANCHOR_Y: u32 = 73;
pub const EE_SCENARIO_CAP_AREA_LEFT: u32 = 74;
pub const EE_SCENARIO_CAP_AREA_TOP: u32 = 75;
pub const EE_SCENARIO_CAP_AREA_RIGHT: u32 = 76;
pub const EE_SCENARIO_CAP_AREA_BOTTOM: u32 = 77;
/// The world: `mInCinematic`, `mNumPlayers`, `mPlayers[a]`, `GetTimeOfDay()` and `mGameSpeed` (floats), the physics'
/// `mTimeOfDayRuns`, the weather's enabled flag, the physics' `mLastOutOfSynchChecksum`, `GetOutOfSynchChecksum(false,
/// a)`, the store of `mLastOutOfSynchChecksum = a`, `EECPOutOfSynchChecksum()`, `mUnits.mFirst`, a unit's
/// `mTileNode->mNext`.
pub const EE_SCENARIO_CAP_IN_CINEMATIC: u32 = 80;
pub const EE_SCENARIO_CAP_NUM_PLAYERS: u32 = 81;
pub const EE_SCENARIO_CAP_PLAYER: u32 = 82;
pub const EE_SCENARIO_CAP_TIME_OF_DAY: u32 = 83;
pub const EE_SCENARIO_CAP_GAME_SPEED: u32 = 84;
pub const EE_SCENARIO_CAP_TIME_OF_DAY_RUNS: u32 = 85;
pub const EE_SCENARIO_CAP_WEATHER: u32 = 86;
pub const EE_SCENARIO_CAP_LAST_SUM: u32 = 87;
pub const EE_SCENARIO_CAP_SUM: u32 = 88;
pub const EE_SCENARIO_CAP_SET_LAST_SUM: u32 = 89;
pub const EE_SCENARIO_CAP_CP_SUM: u32 = 90;
pub const EE_SCENARIO_CAP_FIRST_UNIT: u32 = 91;
pub const EE_SCENARIO_CAP_NEXT_UNIT: u32 = 92;
/// A player's `mGameResult`, `mSeesAll`, `mObserverMode`, `mEnemies[a]`, the sizes of `mPendingGroups`,
/// `mRunningGroups` and `mPatrolGroups`, `mCivilization.mAvailablePoints` and `mMaxPoints`, `mComputerPlayer`,
/// `mResourceCaps[a]`, `mShareBonus`, `mScenarioVariables[a]`, `mTributes[a]`, `mMiningLimit`, `mPlayerID`; a
/// computer player's `mFavoriteLandType`, `mFavoriteSeaType`, `mFavoriteAirType`, `mDifficulty`.
pub const EE_SCENARIO_CAP_PLAYER_RESULT: u32 = 100;
pub const EE_SCENARIO_CAP_PLAYER_SEES_ALL: u32 = 101;
pub const EE_SCENARIO_CAP_PLAYER_OBSERVER: u32 = 102;
pub const EE_SCENARIO_CAP_PLAYER_ENEMY: u32 = 103;
pub const EE_SCENARIO_CAP_PLAYER_PENDING_GROUPS: u32 = 104;
pub const EE_SCENARIO_CAP_PLAYER_RUNNING_GROUPS: u32 = 105;
pub const EE_SCENARIO_CAP_PLAYER_PATROL_GROUPS: u32 = 106;
pub const EE_SCENARIO_CAP_PLAYER_CIV_AVAILABLE: u32 = 107;
pub const EE_SCENARIO_CAP_PLAYER_CIV_MAX: u32 = 108;
pub const EE_SCENARIO_CAP_PLAYER_COMPUTER: u32 = 109;
pub const EE_SCENARIO_CAP_COMPUTER_LAND: u32 = 110;
pub const EE_SCENARIO_CAP_COMPUTER_SEA: u32 = 111;
pub const EE_SCENARIO_CAP_COMPUTER_AIR: u32 = 112;
pub const EE_SCENARIO_CAP_PLAYER_RESOURCE: u32 = 113;
pub const EE_SCENARIO_CAP_PLAYER_SHARE_BONUS: u32 = 114;
pub const EE_SCENARIO_CAP_COMPUTER_DIFFICULTY: u32 = 115;
pub const EE_SCENARIO_CAP_PLAYER_VARIABLE: u32 = 116;
pub const EE_SCENARIO_CAP_PLAYER_TRIBUTE: u32 = 117;
pub const EE_SCENARIO_CAP_PLAYER_MINING_LIMIT: u32 = 118;
pub const EE_SCENARIO_CAP_PLAYER_ID: u32 = 119;
/// Units: `gWorld.GetUnit(a)`; a unit's `mPlayer`, `GetHitPoints()`, `mTileX`, `mTileY`, `mTileNode`,
/// `GetStats()->mName`, `mComplexUnit`, `GetChecksum(3)`, `mUnitID`, `mResourceAmount`, `mResourceType`; a complex
/// unit's `GetCommandType()`, `GetGoal()`, a goal's `mKind`, `mGoalQueues.size()`, queue `a`'s goal count and goal
/// `b`, its flags (`mIsDisabled`, `mIsCommandable`, `mIsDeletable`, `mIsLocked`, `mIsDetachedFromAI`),
/// `mBehavior`, `EEUnitBehavior::sTable`'s size and entry `a`, `mContainer`, `mFlightTime`, `mAmmunition`,
/// `mCargo.size()`, its location's matrix (the address of 16 floats), `mGarrisonCount`, the model's scale (a float),
/// `mReloadTimer`, `mPower`, `mRottingAwayTime`, `mIsBuilt`, `mIsAnimationPaused`, `mUnitVariables`' size and
/// entry `a`; an upgrade's record (`EeUnitsUnitClassUpgradeFields*`).
pub const EE_SCENARIO_CAP_UNIT: u32 = 120;
pub const EE_SCENARIO_CAP_UNIT_PLAYER: u32 = 121;
pub const EE_SCENARIO_CAP_UNIT_HIT_POINTS: u32 = 122;
pub const EE_SCENARIO_CAP_UNIT_TILE_X: u32 = 123;
pub const EE_SCENARIO_CAP_UNIT_TILE_Y: u32 = 124;
pub const EE_SCENARIO_CAP_UNIT_TILE_NODE: u32 = 125;
pub const EE_SCENARIO_CAP_UNIT_NAME: u32 = 126;
pub const EE_SCENARIO_CAP_UNIT_COMPLEX: u32 = 127;
pub const EE_SCENARIO_CAP_UNIT_CHECKSUM: u32 = 128;
pub const EE_SCENARIO_CAP_UNIT_ID: u32 = 129;
pub const EE_SCENARIO_CAP_UNIT_RESOURCE_AMOUNT: u32 = 130;
pub const EE_SCENARIO_CAP_UNIT_RESOURCE_TYPE: u32 = 131;
pub const EE_SCENARIO_CAP_COMMAND: u32 = 140;
pub const EE_SCENARIO_CAP_GOAL: u32 = 141;
pub const EE_SCENARIO_CAP_GOAL_KIND: u32 = 142;
pub const EE_SCENARIO_CAP_QUEUES: u32 = 143;
pub const EE_SCENARIO_CAP_QUEUE_LEN: u32 = 144;
pub const EE_SCENARIO_CAP_QUEUE_GOAL: u32 = 145;
pub const EE_SCENARIO_CAP_DISABLED: u32 = 146;
pub const EE_SCENARIO_CAP_COMMANDABLE: u32 = 147;
pub const EE_SCENARIO_CAP_DELETABLE: u32 = 148;
pub const EE_SCENARIO_CAP_LOCKED: u32 = 149;
pub const EE_SCENARIO_CAP_DETACHED: u32 = 150;
pub const EE_SCENARIO_CAP_BEHAVIOR: u32 = 151;
pub const EE_SCENARIO_CAP_BEHAVIORS: u32 = 152;
pub const EE_SCENARIO_CAP_BEHAVIOR_AT: u32 = 153;
pub const EE_SCENARIO_CAP_CONTAINER: u32 = 154;
pub const EE_SCENARIO_CAP_FLIGHT_TIME: u32 = 155;
pub const EE_SCENARIO_CAP_AMMUNITION: u32 = 156;
pub const EE_SCENARIO_CAP_CARGO: u32 = 157;
pub const EE_SCENARIO_CAP_LOCATION: u32 = 158;
pub const EE_SCENARIO_CAP_GARRISON: u32 = 159;
pub const EE_SCENARIO_CAP_SCALE: u32 = 160;
pub const EE_SCENARIO_CAP_RELOAD_TIMER: u32 = 161;
pub const EE_SCENARIO_CAP_POWER: u32 = 162;
pub const EE_SCENARIO_CAP_ROTTING: u32 = 163;
pub const EE_SCENARIO_CAP_BUILT: u32 = 164;
pub const EE_SCENARIO_CAP_PAUSED: u32 = 165;
pub const EE_SCENARIO_CAP_VARIABLES: u32 = 166;
pub const EE_SCENARIO_CAP_VARIABLE_AT: u32 = 167;
pub const EE_SCENARIO_CAP_UPGRADE_FIELDS: u32 = 168;
/// A `std::vector<long>`'s size and element `a` (SMCP's players).
pub const EE_SCENARIO_CAP_LONGS: u32 = 170;
pub const EE_SCENARIO_CAP_LONGS_AT: u32 = 171;

/// The save version the records' Saves use (`kSaveGameMajorVersion`, `kSaveGameMinorVersion`), packed as the
/// `_SAVE_*` reads take it: the C++ forwards pass it in.
pub type EeScenarioCapVersion = i64;

/// The world as the records read it.
pub trait CaptureWorld {
    /// One read (`EE_SCENARIO_CAP_*`).
    fn read(&mut self, op: u32, p: u64, a: i64, b: i64) -> i64;
    /// One float read (the area's rectangle, the time of day, the game speed, the model's scale).
    fn readf(&mut self, op: u32, p: u64) -> f32;
}

/// SMOB's units before (`EESMCaptureObjectState`): their count and FNV-1a, the seed.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScenarioCapObjectBefore {
    pub mCount: u32,
    pub mHash: u64,
    pub mSeed: u32,
}

/// SMTE's trigger state before (`EESMCaptureTriggerState`): on, fired, the condition start time.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScenarioCapTriggerBefore {
    pub mIsOn: bool,
    pub mHasFired: bool,
    pub mStart: c_ulong,
}

thread_local! {
    /// The objects' Evaluate depth: SMCP and SMAR are not recorded inside one.
    static OBJECT_DEPTH: Cell<i32> = const { Cell::new(0) };
}

/// SMUP's rounds (every 120th also records SMSV).
static ROUNDS: std::sync::atomic::AtomicU32 = std::sync::atomic::AtomicU32::new(0);

const fn tag(name: &[u8; 4]) -> u32 {
    u32::from_le_bytes(*name)
}

/// The FNV-1a of a C string (0 bytes for null).
///
/// # Safety
/// `text` is null or a C string.
unsafe fn text_hash_of(text: *const c_char) -> u64 {
    if text.is_null() {
        return fnv(FNV_BASIS, &[]);
    }
    // SAFETY: the caller's contract
    fnv(FNV_BASIS, unsafe { CStr::from_ptr(text) }.to_bytes())
}

fn b(v: bool) -> u32 {
    v as u32
}

/// A record over a world: the reads' shorthands.
struct R<'w, W: CaptureWorld> {
    w: &'w mut W,
    r: Record,
}

impl<'w, W: CaptureWorld> R<'w, W> {
    fn new(w: &'w mut W, name: &[u8; 4]) -> Self {
        R { w, r: Record::new(tag(name)) }
    }
    fn rd(&mut self, op: u32, p: u64, a: i64) -> i64 {
        self.w.read(op, p, a, 0)
    }
    fn ptr(&mut self, op: u32, p: u64, a: i64) -> u64 {
        self.w.read(op, p, a, 0) as u64
    }
    fn text(&mut self, op: u32, p: u64, a: i64) -> u64 {
        let t = self.ptr(op, p, a) as usize as *const c_char;
        // SAFETY: a text read answers a C string's address (or null)
        unsafe { text_hash_of(t) }
    }
    fn seed(&mut self) -> u32 {
        self.rd(EE_SCENARIO_CAP_SEED, 0, 0) as u32
    }
    fn u32(&mut self, v: u32) {
        self.r.u32(v);
    }
    fn i32(&mut self, v: i32) {
        self.r.i32(v);
    }
    fn f32(&mut self, v: f32) {
        self.r.f32(v);
    }
    fn u64(&mut self, v: u64) {
        self.r.u64(v);
    }
    fn commit(self) {
        #[cfg(test)]
        tests::COMMITTED.with(|c| c.borrow_mut().push(self.r.clone()));
        self.r.commit();
    }
    /// The manager (0: none).
    fn manager(&mut self) -> u64 {
        self.ptr(EE_SCENARIO_CAP_MANAGER, 0, 0)
    }
    fn manager_fields(&mut self, m: u64) -> EeScenarioManagerFields {
        let p = self.ptr(EE_SCENARIO_CAP_MANAGER_FIELDS, m, 0) as usize as *const EeScenarioManagerFields;
        // SAFETY: the read answers the live manager's record
        unsafe { std::ptr::read(p) }
    }
    /// `TimedEffects`: the count, then each entry's index, time and flags.
    fn timed(&mut self, m: u64) {
        let q = self.ptr(EE_SCENARIO_CAP_TIMED, m, 0) as usize as *const EeScenarioTimedQueue;
        // SAFETY: the manager's live queue
        let entries = unsafe { (*q).entries.clone() };
        self.u32(entries.len() as u32);
        for e in entries {
            self.u32(e.mEffectIndex as u32);
            self.u32(e.mTime as u32);
            self.u32(b(e.mIsInCinematic) | (b(e.mHasRun) << 1));
        }
    }
    /// The index of `element` in the manager's list (`count_op`, `at_op`), -1 if none or no manager.
    fn index_of(&mut self, count_op: u32, at_op: u32, element: u64) -> i32 {
        let m = self.manager();
        if m == 0 {
            return -1;
        }
        let n = self.rd(count_op, m, 0);
        for i in 0..n {
            if self.ptr(at_op, m, i) == element {
                return i as i32;
            }
        }
        -1
    }
    /// The FNV-1a of a memory file's bytes after `save(file)` (its result returned too).
    fn saved(&mut self, save: impl FnOnce(&mut Self, u64) -> i64) -> (i64, u64) {
        // `FSMemoryFile()`'s defaults: 1000-byte blocks, growing by doubling, one block
        let state = Box::into_raw(Box::new(EeFsMemFile::new(1000, true, 1)));
        let file = self.ptr(EE_SCENARIO_CAP_FILE_NEW, state as usize as u64, 0);
        let result = save(self, file);
        self.rd(EE_SCENARIO_CAP_FILE_FREE, file, 0);
        // SAFETY: the box made above, which the C++ file only borrowed
        let state = unsafe { Box::from_raw(state) };
        (result, fnv(FNV_BASIS, state.contents()))
    }
    /// `SavedList`: a list's count, the first error and the hash of its elements' Saves.
    fn saved_list(&mut self, m: u64, count_op: u32, at_op: u32, save_op: u32, version: EeScenarioCapVersion) {
        let n = self.rd(count_op, m, 0);
        let (result, hash) = self.saved(|s, file| {
            let mut result = 0i64;
            for i in 0..n {
                let e = s.ptr(at_op, m, i);
                if e != 0 && result == 0 {
                    result = s.w.read(save_op, e, file as i64, version);
                }
            }
            result
        });
        self.u32(n as u32);
        self.i32(result as i32);
        self.u64(hash);
    }
    fn pending_list(&mut self, m: u64, version: EeScenarioCapVersion) {
        self.saved_list(m, EE_SCENARIO_CAP_PENDING, EE_SCENARIO_CAP_PENDING_AT, EE_SCENARIO_CAP_SAVE_EFFECT, version);
    }
}

/// SMCE: a condition's Evaluate.
pub fn condition<W: CaptureWorld>(w: &mut W, condition: u64, result: bool, seed: u32) {
    let mut r = R::new(w, b"SMCE");
    let p = r.ptr(EE_SCENARIO_CAP_CONDITION_FIELDS, condition, 0) as usize as *const EeScenarioConditionFields;
    // SAFETY: the live condition's record
    let c = unsafe { std::ptr::read(p) };
    for v in [c.mID, c.mType, c.mOperator, c.mDifficulty, c.mDiplomacy, c.mAttribute, c.mOtherAttribute] {
        r.u32(v as u32);
    }
    r.i32(c.mTechnology as i32);
    for v in [c.mAmount, c.mObjectIndex, c.mPlayer, c.mOtherPlayer, c.mMinTime, c.mMaxTime, c.mHour, c.mTrigger] {
        r.u32(v as u32);
    }
    r.u32(
        b(c.mIsTriggerOn)
            | (b(c.mHasTriggerFired) << 1)
            | (b(c.mUsesDifficulty) << 2)
            | (b(c.mUsesElapsedTime) << 3)
            | (b(c.mUsesHour) << 4)
            | (b(c.mUsesPlayerAttribute) << 5),
    );
    let chat = r.text(EE_SCENARIO_CAP_CONDITION_CHAT, condition, 0);
    r.u64(chat);
    let m = r.manager();
    let last = if m != 0 { r.text(EE_SCENARIO_CAP_LAST_CHAT, m, 0) } else { 0 };
    r.u64(last);
    let time = if m != 0 { r.manager_fields(m).mTime } else { 0.0 };
    r.f32(time);
    r.u32(seed);
    let now = r.seed();
    r.u32(now);
    r.u32(b(result));
    r.commit();
}

/// The trigger's state before its Evaluate.
pub fn trigger_before<W: CaptureWorld>(w: &mut W, trigger: u64) -> EeScenarioCapTriggerBefore {
    let t = trigger_fields(w, trigger);
    EeScenarioCapTriggerBefore { mIsOn: t.mIsOn, mHasFired: t.mHasFired, mStart: t.mConditionStartTime }
}

fn trigger_fields<W: CaptureWorld>(w: &mut W, trigger: u64) -> EeScenarioTriggerFields {
    let p = w.read(EE_SCENARIO_CAP_TRIGGER_FIELDS, trigger, 0, 0) as usize as *const EeScenarioTriggerFields;
    // SAFETY: the live trigger's record
    unsafe { std::ptr::read(p) }
}

/// SMTE: a trigger's Evaluate.
pub fn trigger<W: CaptureWorld>(w: &mut W, trigger: u64, before: &EeScenarioCapTriggerBefore, time: f32, seed: u32) {
    let t = trigger_fields(w, trigger);
    let mut r = R::new(w, b"SMTE");
    r.u32(t.mID as u32);
    r.u32(b(before.mIsOn) | (b(before.mHasFired) << 1));
    r.u32(before.mStart as u32);
    r.f32(time);
    r.u32(t.mConditionDuration as u32);
    let cinematic = r.rd(EE_SCENARIO_CAP_IN_CINEMATIC, 0, 0) != 0;
    r.u32(b(t.mIsLooping) | (b(t.mIsInCinematic) << 1) | (b(cinematic) << 2));
    let conditions = r.rd(EE_SCENARIO_CAP_TRIGGER_CONDITIONS, trigger, 0);
    r.u32(conditions as u32);
    let effects = r.rd(EE_SCENARIO_CAP_TRIGGER_EFFECTS, trigger, 0);
    r.u32(effects as u32);
    r.u32(b(t.mIsOn) | (b(t.mHasFired) << 1));
    r.u32(t.mConditionStartTime as u32);
    r.u32(seed);
    let now = r.seed();
    r.u32(now);
    let m = r.manager();
    if m != 0 {
        r.timed(m);
    }
    r.commit();
}

/// An object's units: their count and FNV-1a.
fn object_units<W: CaptureWorld>(w: &mut W, object: u64) -> (u32, u64) {
    let n = w.read(EE_SCENARIO_CAP_OBJECT_UNITS, object, 0, 0);
    let mut hash = FNV_BASIS;
    for i in 0..n {
        let id = w.read(EE_SCENARIO_CAP_OBJECT_UNIT, object, i, 0) as u32;
        hash = fnv(hash, &id.to_le_bytes());
    }
    (n as u32, hash)
}

/// SMOB's state before an object's Evaluate (and the depth that keeps SMCP and SMAR out).
pub fn object_begin<W: CaptureWorld>(w: &mut W, object: u64) -> EeScenarioCapObjectBefore {
    let (count, hash) = object_units(w, object);
    let seed = w.read(EE_SCENARIO_CAP_SEED, 0, 0, 0) as u32;
    OBJECT_DEPTH.with(|d| d.set(d.get() + 1));
    EeScenarioCapObjectBefore { mCount: count, mHash: hash, mSeed: seed }
}

/// SMOB: an object's Evaluate.
pub fn object_end<W: CaptureWorld>(
    w: &mut W,
    object: u64,
    include_dead: bool,
    result: bool,
    before: &EeScenarioCapObjectBefore,
) {
    OBJECT_DEPTH.with(|d| d.set(d.get() - 1));
    let mut r = R::new(w, b"SMOB");
    let index = r.index_of(EE_SCENARIO_CAP_OBJECTS, EE_SCENARIO_CAP_OBJECT_AT, object);
    r.i32(index);
    let p = r.ptr(EE_SCENARIO_CAP_OBJECT_FIELDS, object, 0) as usize as *const EeScenarioObjectFields;
    // SAFETY: the live object's record
    let o = unsafe { std::ptr::read(p) };
    for v in [
        o.mL64,
        o.mOwner,
        o.mFamily,
        o.mClass,
        o.mMinCount,
        o.mMaxCount,
        o.mArea,
        o.mNearMode,
        o.mNearObject,
        o.mParent,
        o.mAttribute,
        o.mAttributeMin,
        o.mAttributeMax,
        o.mVisibleByPlayer,
        o.mState,
        o.mSelectedByPlayer,
        o.mInAreaCheck,
        o.mNearCheck,
        o.mHasAttributeCheck,
        o.mVisibleByCheck,
        o.mHasStateCheck,
        o.mSelectedByCheck,
    ] {
        r.u32(v as u32);
    }
    r.u32(b(o.mIsAttributePercent) | (b(include_dead) << 1));
    r.u32(before.mCount);
    r.u64(before.mHash);
    r.u32(before.mSeed);
    r.u32(b(result));
    let (count, hash) = object_units(r.w, object);
    r.u32(count);
    r.u64(hash);
    r.u32(b(o.mIsSelectOnMap) | (b(o.mIsDynamic) << 1));
    let now = r.seed();
    r.u32(now);
    r.commit();
}

fn in_object() -> bool {
    OBJECT_DEPTH.with(|d| d.get() > 0)
}

/// SMCP: a player choice outside an object's Evaluate (`players` 0: no list, else a `std::vector<long>*`).
pub fn players<W: CaptureWorld>(w: &mut W, kind: i32, single: bool, result: i32, players: u64, seed: u32) {
    if in_object() {
        return;
    }
    let mut r = R::new(w, b"SMCP");
    r.i32(kind);
    r.u32(b(single));
    r.i32(result);
    if players != 0 {
        let n = r.rd(EE_SCENARIO_CAP_LONGS, players, 0);
        r.u32(n as u32);
        for i in 0..n {
            let p = r.rd(EE_SCENARIO_CAP_LONGS_AT, players, i);
            r.i32(p as i32);
        }
    } else {
        r.u32(0xffff_ffff);
    }
    let n = r.rd(EE_SCENARIO_CAP_NUM_PLAYERS, 0, 0);
    r.u32(n as u32);
    r.u32(seed);
    let now = r.seed();
    r.u32(now);
    r.commit();
}

/// SMAR: an area query outside an object's Evaluate.
#[allow(clippy::too_many_arguments)]
pub fn area<W: CaptureWorld>(w: &mut W, area: u64, kind: i32, a: i32, b_: i32, x: i32, y: i32, hash: u64) {
    if in_object() {
        return;
    }
    let mut r = R::new(w, b"SMAR");
    r.i32(kind);
    let index = r.index_of(EE_SCENARIO_CAP_AREAS, EE_SCENARIO_CAP_AREA_AT, area);
    r.i32(index);
    for op in [EE_SCENARIO_CAP_AREA_ID, EE_SCENARIO_CAP_AREA_SELECT] {
        let v = r.rd(op, area, 0);
        r.u32(v as u32);
    }
    for op in [EE_SCENARIO_CAP_AREA_ANCHOR_X, EE_SCENARIO_CAP_AREA_ANCHOR_Y] {
        let v = r.rd(op, area, 0);
        r.i32(v as i32);
    }
    for op in
        [EE_SCENARIO_CAP_AREA_LEFT, EE_SCENARIO_CAP_AREA_TOP, EE_SCENARIO_CAP_AREA_RIGHT, EE_SCENARIO_CAP_AREA_BOTTOM]
    {
        let v = r.w.readf(op, area);
        r.f32(v);
    }
    for v in [a, b_, x, y] {
        r.i32(v);
    }
    r.u64(hash);
    r.commit();
}

fn effect_fields<W: CaptureWorld>(w: &mut W, effect: u64) -> EeScenarioEffectFields {
    let p = w.read(EE_SCENARIO_CAP_EFFECT_FIELDS, effect, 0, 0) as usize as *const EeScenarioEffectFields;
    // SAFETY: the live effect's record
    unsafe { std::ptr::read(p) }
}

/// SMEX: an effect's run, then SMEW (its world after).
pub fn effect<W: CaptureWorld>(w: &mut W, effect: u64, cinematic: bool, seed: u32, version: EeScenarioCapVersion) {
    let e = effect_fields(w, effect);
    let mut r = R::new(w, b"SMEX");
    for v in [
        e.mID,
        e.mType,
        e.mAIAction,
        e.mMediaAction,
        e.mPlayerAction,
        e.mObjectAction,
        e.mGameAction,
        e.mObject,
        e.mPlayer,
    ] {
        r.u32(v as u32);
    }
    r.u32(b(cinematic));
    r.u32(seed);
    let now = r.seed();
    r.u32(now);
    let m = r.manager();
    let result = if m != 0 { r.manager_fields(m).mGameResult as i32 } else { 0 };
    r.i32(result);
    let in_cinematic = r.rd(EE_SCENARIO_CAP_IN_CINEMATIC, 0, 0);
    r.u32((in_cinematic != 0) as u32);
    if m != 0 {
        r.timed(m);
    }
    r.commit();
    effect_world(w, effect, version);
}

/// A unit of an effect's objects as SMEW reads it: its checksum and the state the effects write.
fn effect_unit<W: CaptureWorld>(r: &mut R<'_, W>, id: u32) {
    let u = r.ptr(EE_SCENARIO_CAP_UNIT, 0, id as i64);
    r.u32(id);
    r.u32((u != 0) as u32);
    if u == 0 {
        return;
    }
    let sum = r.rd(EE_SCENARIO_CAP_UNIT_CHECKSUM, u, 0);
    r.i32(sum as i32);
    let player = r.ptr(EE_SCENARIO_CAP_UNIT_PLAYER, u, 0);
    let pid = if player != 0 { r.rd(EE_SCENARIO_CAP_PLAYER_ID, player, 0) as i32 } else { -1 };
    r.i32(pid);
    for op in [EE_SCENARIO_CAP_UNIT_HIT_POINTS, EE_SCENARIO_CAP_UNIT_TILE_X, EE_SCENARIO_CAP_UNIT_TILE_Y] {
        let v = r.rd(op, u, 0);
        r.i32(v as i32);
    }
    let node = r.rd(EE_SCENARIO_CAP_UNIT_TILE_NODE, u, 0);
    r.u32((node != 0) as u32);
    let name = r.text(EE_SCENARIO_CAP_UNIT_NAME, u, 0);
    r.u64(name);
    let c = r.ptr(EE_SCENARIO_CAP_UNIT_COMPLEX, u, 0);
    r.u32((c != 0) as u32);
    if c == 0 {
        return;
    }
    let command = r.rd(EE_SCENARIO_CAP_COMMAND, c, 0);
    r.u32(command as u32);
    let goal = r.ptr(EE_SCENARIO_CAP_GOAL, c, 0);
    let kind = if goal != 0 { r.rd(EE_SCENARIO_CAP_GOAL_KIND, goal, 0) as i32 } else { -1 };
    r.i32(kind);
    let queues = r.rd(EE_SCENARIO_CAP_QUEUES, c, 0);
    r.u32(queues as u32);
    for q in 0..queues {
        let n = r.rd(EE_SCENARIO_CAP_QUEUE_LEN, c, q);
        r.u32(n as u32);
        for g in 0..n {
            let goal = r.w.read(EE_SCENARIO_CAP_QUEUE_GOAL, c, q, g) as u64;
            let kind = if goal != 0 { r.rd(EE_SCENARIO_CAP_GOAL_KIND, goal, 0) as i32 } else { -1 };
            r.i32(kind);
        }
    }
    let mut flags = 0u32;
    for (i, op) in [
        EE_SCENARIO_CAP_DISABLED,
        EE_SCENARIO_CAP_COMMANDABLE,
        EE_SCENARIO_CAP_DELETABLE,
        EE_SCENARIO_CAP_LOCKED,
        EE_SCENARIO_CAP_DETACHED,
    ]
    .into_iter()
    .enumerate()
    {
        flags |= ((r.rd(op, c, 0) != 0) as u32) << i;
    }
    r.u32(flags);
    let behavior = r.ptr(EE_SCENARIO_CAP_BEHAVIOR, c, 0);
    let mut index = -1i32;
    let n = r.rd(EE_SCENARIO_CAP_BEHAVIORS, 0, 0);
    for i in 0..n {
        if r.ptr(EE_SCENARIO_CAP_BEHAVIOR_AT, 0, i) == behavior {
            index = i as i32;
        }
    }
    r.i32(index);
    let container = r.ptr(EE_SCENARIO_CAP_CONTAINER, c, 0);
    let cid = if container != 0 { r.rd(EE_SCENARIO_CAP_UNIT_ID, container, 0) as i32 } else { -1 };
    r.i32(cid);
    for op in [EE_SCENARIO_CAP_FLIGHT_TIME, EE_SCENARIO_CAP_AMMUNITION] {
        let v = r.rd(op, c, 0);
        r.i32(v as i32);
    }
    let cargo = r.rd(EE_SCENARIO_CAP_CARGO, c, 0);
    r.u32(cargo as u32);
    let m = r.ptr(EE_SCENARIO_CAP_LOCATION, c, 0) as usize as *const [f32; 16];
    // SAFETY: the live unit's location matrix (16 floats)
    let matrix = unsafe { std::ptr::read_unaligned(m) };
    for v in matrix {
        r.f32(v);
    }
}

fn effect_object<W: CaptureWorld>(r: &mut R<'_, W>, m: u64, index: c_ulong) {
    let o = r.ptr(EE_SCENARIO_CAP_OBJECT_AT, m, index as u32 as i64);
    if o == 0 {
        r.u32(0xffff_ffff);
        return;
    }
    let n = r.rd(EE_SCENARIO_CAP_OBJECT_UNITS, o, 0);
    r.u32(n as u32);
    for i in 0..n {
        let id = r.rd(EE_SCENARIO_CAP_OBJECT_UNIT, o, i) as u32;
        effect_unit(r, id);
    }
}

/// SMEW: an effect's world after its run.
pub fn effect_world<W: CaptureWorld>(w: &mut W, effect: u64, version: EeScenarioCapVersion) {
    let m = w.read(EE_SCENARIO_CAP_MANAGER, 0, 0, 0) as u64;
    if m == 0 {
        return;
    }
    let e = effect_fields(w, effect);
    let mut r = R::new(w, b"SMEW");
    r.u32(e.mID as u32);
    let (result, hash) = r.saved(|s, file| s.w.read(EE_SCENARIO_CAP_SAVE_EFFECT, effect, file as i64, version));
    r.i32(result as i32);
    r.u64(hash);
    // the world's out-of-synch sums as EE-SIM's --checksum-trace reads them (reads only)
    let saved = r.rd(EE_SCENARIO_CAP_LAST_SUM, 0, 0);
    let sum = r.rd(EE_SCENARIO_CAP_SUM, 0, 0x1 | 0x2 | 0x10 | 0x40 | 0x100);
    r.u32(sum as u32);
    r.rd(EE_SCENARIO_CAP_SET_LAST_SUM, 0, saved);
    let (mut units, mut count) = (0u32, 0u32);
    let mut u = r.ptr(EE_SCENARIO_CAP_FIRST_UNIT, 0, 0);
    while u != 0 {
        units = units.wrapping_add(r.rd(EE_SCENARIO_CAP_UNIT_CHECKSUM, u, 0) as u32);
        count = count.wrapping_add(1);
        u = r.ptr(EE_SCENARIO_CAP_NEXT_UNIT, u, 0);
    }
    r.u32(count);
    r.u32(units);
    let cp = r.rd(EE_SCENARIO_CAP_CP_SUM, 0, 0);
    r.u32(cp as u32);
    // the units of the objects an effect names
    for index in [e.mObject, e.mTaskObject, e.mPlaceObject, e.mTargetObject, e.mLocationObject] {
        effect_object(&mut r, m, index);
    }
    // the players
    let n = r.rd(EE_SCENARIO_CAP_NUM_PLAYERS, 0, 0);
    r.u32(n as u32);
    for p in 0..n {
        let player = r.ptr(EE_SCENARIO_CAP_PLAYER, 0, p);
        r.u32((player != 0) as u32);
        if player == 0 {
            continue;
        }
        let result = r.rd(EE_SCENARIO_CAP_PLAYER_RESULT, player, 0);
        r.u32(result as u32);
        let sees = r.rd(EE_SCENARIO_CAP_PLAYER_SEES_ALL, player, 0) != 0;
        let observer = r.rd(EE_SCENARIO_CAP_PLAYER_OBSERVER, player, 0) != 0;
        r.u32(b(sees) | (b(observer) << 1));
        for q in 0..n {
            let v = r.rd(EE_SCENARIO_CAP_PLAYER_ENEMY, player, q);
            r.i32(v as i32);
        }
        for op in [
            EE_SCENARIO_CAP_PLAYER_PENDING_GROUPS,
            EE_SCENARIO_CAP_PLAYER_RUNNING_GROUPS,
            EE_SCENARIO_CAP_PLAYER_PATROL_GROUPS,
            EE_SCENARIO_CAP_PLAYER_CIV_AVAILABLE,
            EE_SCENARIO_CAP_PLAYER_CIV_MAX,
        ] {
            let v = r.rd(op, player, 0);
            r.u32(v as u32);
        }
        let computer = r.ptr(EE_SCENARIO_CAP_PLAYER_COMPUTER, player, 0);
        r.u32((computer != 0) as u32);
        if computer != 0 {
            for op in [EE_SCENARIO_CAP_COMPUTER_LAND, EE_SCENARIO_CAP_COMPUTER_SEA, EE_SCENARIO_CAP_COMPUTER_AIR] {
                let v = r.rd(op, computer, 0);
                r.u32(v as u32);
            }
        }
    }
    // the manager, the world and the camera
    for slot in [0, 3, 2] {
        let h = r.text(EE_SCENARIO_CAP_MEDIA_SECTION, m, slot);
        r.u64(h);
    }
    let messages = r.rd(EE_SCENARIO_CAP_MESSAGES, m, 0);
    r.u32(messages as u32);
    let pending = r.rd(EE_SCENARIO_CAP_PENDING, m, 0);
    r.u32(pending as u32);
    triggers_state(&mut r, m, true);
    let tod = r.w.readf(EE_SCENARIO_CAP_TIME_OF_DAY, 0);
    r.f32(tod);
    let speed = r.w.readf(EE_SCENARIO_CAP_GAME_SPEED, 0);
    r.f32(speed);
    let runs = r.rd(EE_SCENARIO_CAP_TIME_OF_DAY_RUNS, 0, 0) != 0;
    let cinematic = r.rd(EE_SCENARIO_CAP_IN_CINEMATIC, 0, 0) != 0;
    let weather = r.rd(EE_SCENARIO_CAP_WEATHER, 0, 0) != 0;
    r.u32(b(runs) | (b(cinematic) << 1) | (b(weather) << 2));
    for op in [EE_SCENARIO_CAP_CAMERA_KEYS, EE_SCENARIO_CAP_CAMERA_PATH] {
        let v = r.rd(op, 0, 0);
        r.u32(v as u32);
    }
    r.commit();
}

/// The triggers' count and each one's state (on, fired; with its start time shifted in when `start`), all ones for
/// a null trigger.
fn triggers_state<W: CaptureWorld>(r: &mut R<'_, W>, m: u64, start: bool) {
    let n = r.rd(EE_SCENARIO_CAP_TRIGGERS, m, 0);
    r.u32(n as u32);
    for i in 0..n {
        let t = r.ptr(EE_SCENARIO_CAP_TRIGGER_AT, m, i);
        if t == 0 {
            r.u32(0xffff_ffff);
            continue;
        }
        let f = trigger_fields(r.w, t);
        let mut v = b(f.mIsOn) | (b(f.mHasFired) << 1);
        if start {
            v |= (f.mConditionStartTime as u32) << 2;
        }
        r.u32(v);
    }
}

/// SMSV: the manager's saved bytes.
pub fn saved<W: CaptureWorld>(w: &mut W, m: u64, kind: i32, result: i32, version: EeScenarioCapVersion) {
    let mut r = R::new(w, b"SMSV");
    r.i32(kind);
    r.i32(result);
    r.saved_list(m, EE_SCENARIO_CAP_TRIGGERS, EE_SCENARIO_CAP_TRIGGER_AT, EE_SCENARIO_CAP_SAVE_TRIGGER, version);
    r.saved_list(m, EE_SCENARIO_CAP_CONDITIONS, EE_SCENARIO_CAP_CONDITION_AT, EE_SCENARIO_CAP_SAVE_CONDITION, version);
    r.saved_list(m, EE_SCENARIO_CAP_EFFECTS, EE_SCENARIO_CAP_EFFECT_AT, EE_SCENARIO_CAP_SAVE_EFFECT, version);
    r.saved_list(m, EE_SCENARIO_CAP_OBJECTS, EE_SCENARIO_CAP_OBJECT_AT, EE_SCENARIO_CAP_SAVE_OBJECT, version);
    r.saved_list(m, EE_SCENARIO_CAP_AREAS, EE_SCENARIO_CAP_AREA_AT, EE_SCENARIO_CAP_SAVE_AREA, version);
    r.pending_list(m, version);
    let (result, hash) = r.saved(|s, file| s.w.read(EE_SCENARIO_CAP_SAVE_MANAGER, m, file as i64, version));
    r.i32(result as i32);
    r.u64(hash);
    r.commit();
}

/// SMUP: a trigger round (every 120th also SMSV, kind 1).
pub fn update<W: CaptureWorld>(w: &mut W, m: u64, human: bool, version: EeScenarioCapVersion) {
    let rounds = ROUNDS.fetch_add(1, std::sync::atomic::Ordering::Relaxed).wrapping_add(1);
    if rounds.is_multiple_of(120) {
        saved(w, m, 1, 0, version);
    }
    let mut r = R::new(w, b"SMUP");
    let f = r.manager_fields(m);
    r.f32(f.mTime);
    r.f32(f.mTriggerTimer);
    let cinematic = r.rd(EE_SCENARIO_CAP_IN_CINEMATIC, 0, 0) != 0;
    r.u32(
        b(human)
            | (b(cinematic) << 1)
            | (b(f.mIsMovingCamera) << 2)
            | (b(f.mIsShowingDialog) << 3)
            | (b(f.mIsEndingCinematic) << 4),
    );
    triggers_state(&mut r, m, false);
    r.timed(m);
    let messages = r.rd(EE_SCENARIO_CAP_MESSAGES, m, 0);
    r.u32(messages as u32);
    let f = r.manager_fields(m);
    r.i32(f.mGameResult as i32);
    let now = r.seed();
    r.u32(now);
    r.commit();
}

/// SMPA: a Set Player Attribute (`player` an index into `mPlayers`).
pub fn player_attribute<W: CaptureWorld>(
    w: &mut W,
    player: c_long,
    attribute: c_ulong,
    mode: c_long,
    amount: c_long,
    seed: u32,
    version: EeScenarioCapVersion,
) {
    let mut r = R::new(w, b"SMPA");
    r.i32(player as i32);
    r.u32(attribute as u32);
    r.i32(mode as i32);
    r.i32(amount as i32);
    r.u32(seed);
    let now = r.seed();
    r.u32(now);
    let p = if (0..crate::world::view::EE_WORLD_PLAYER_SLOTS as c_long).contains(&player) {
        r.ptr(EE_SCENARIO_CAP_PLAYER, 0, player as i64)
    } else {
        0
    };
    r.u32((p != 0) as u32);
    if p != 0 {
        for i in 0..7 {
            let v = r.rd(EE_SCENARIO_CAP_PLAYER_RESOURCE, p, i);
            r.i32(v as i32);
        }
        let v = r.rd(EE_SCENARIO_CAP_PLAYER_SHARE_BONUS, p, 0);
        r.i32(v as i32);
        let computer = r.ptr(EE_SCENARIO_CAP_PLAYER_COMPUTER, p, 0);
        let difficulty = if computer != 0 { r.rd(EE_SCENARIO_CAP_COMPUTER_DIFFICULTY, computer, 0) as i32 } else { -1 };
        r.i32(difficulty);
        for op in [EE_SCENARIO_CAP_PLAYER_VARIABLE, EE_SCENARIO_CAP_PLAYER_TRIBUTE] {
            for i in 0..10 {
                let v = r.rd(op, p, i);
                r.u32(v as u32);
            }
        }
        let v = r.rd(EE_SCENARIO_CAP_PLAYER_MINING_LIMIT, p, 0);
        r.i32(v as i32);
        let (result, hash) = r.saved(|s, file| s.w.read(EE_SCENARIO_CAP_SAVE_GROUPS, p, file as i64, version));
        r.i32(result as i32);
        r.u64(hash);
    }
    let m = r.manager();
    if m != 0 {
        r.pending_list(m, version);
    }
    r.commit();
}

/// SMTK: a message's tokens expanded (the texts as C strings).
///
/// # Safety
/// `text` and `result` are null or C strings.
pub unsafe fn tokens<W: CaptureWorld>(w: &mut W, text: *const c_char, result: *const c_char, seed: u32) {
    let mut r = R::new(w, b"SMTK");
    // SAFETY: the caller's contract
    unsafe {
        r.u64(text_hash_of(text));
        r.u64(text_hash_of(result));
    }
    r.u32(seed);
    let now = r.seed();
    r.u32(now);
    r.commit();
}

/// SMFP: a placement.
#[allow(clippy::too_many_arguments)]
pub fn placement<W: CaptureWorld>(
    w: &mut W,
    object: u64,
    unit_type: c_ulong,
    random: bool,
    result: bool,
    x: f32,
    y: f32,
    seed: u32,
) {
    let mut r = R::new(w, b"SMFP");
    let index = r.index_of(EE_SCENARIO_CAP_OBJECTS, EE_SCENARIO_CAP_OBJECT_AT, object);
    r.i32(index);
    r.u32(unit_type as u32);
    r.u32(b(random) | (b(result) << 1));
    r.f32(x);
    r.f32(y);
    r.u32(seed);
    let now = r.seed();
    r.u32(now);
    r.commit();
}

/// SMCU: kind 0 EEChangeUnit (`unit` its `EEUnit*` or 0), kind 1 EEModifyUnitAttribute (`player`, `upgrade` or 0).
#[allow(clippy::too_many_arguments)]
pub fn unit_change<W: CaptureWorld>(
    w: &mut W,
    kind: c_long,
    args: &[c_long; 7],
    unit: u64,
    player: u64,
    upgrade: u64,
    seed: u32,
    version: EeScenarioCapVersion,
) {
    let mut r = R::new(w, b"SMCU");
    r.i32(kind as i32);
    for a in args {
        r.i32(*a as i32);
    }
    r.u32(seed);
    let now = r.seed();
    r.u32(now);
    if kind == 0 {
        r.u32((unit != 0) as u32);
        if unit != 0 {
            let id = r.rd(EE_SCENARIO_CAP_UNIT_ID, unit, 0);
            r.u32(id as u32);
            for op in [
                EE_SCENARIO_CAP_UNIT_CHECKSUM,
                EE_SCENARIO_CAP_UNIT_HIT_POINTS,
                EE_SCENARIO_CAP_UNIT_RESOURCE_AMOUNT,
                EE_SCENARIO_CAP_UNIT_RESOURCE_TYPE,
            ] {
                let v = r.rd(op, unit, 0);
                r.i32(v as i32);
            }
            let node = r.rd(EE_SCENARIO_CAP_UNIT_TILE_NODE, unit, 0);
            r.u32((node != 0) as u32);
            let c = r.ptr(EE_SCENARIO_CAP_UNIT_COMPLEX, unit, 0);
            r.u32((c != 0) as u32);
            if c != 0 {
                for op in [EE_SCENARIO_CAP_FLIGHT_TIME, EE_SCENARIO_CAP_GARRISON, EE_SCENARIO_CAP_AMMUNITION] {
                    let v = r.rd(op, c, 0);
                    r.u32(v as u32);
                }
                let scale = r.w.readf(EE_SCENARIO_CAP_SCALE, c);
                r.f32(scale);
                for op in [EE_SCENARIO_CAP_RELOAD_TIMER, EE_SCENARIO_CAP_POWER, EE_SCENARIO_CAP_ROTTING] {
                    let v = r.rd(op, c, 0);
                    r.i32(v as i32);
                }
                let built = r.rd(EE_SCENARIO_CAP_BUILT, c, 0) != 0;
                let paused = r.rd(EE_SCENARIO_CAP_PAUSED, c, 0) != 0;
                r.u32(b(built) | (b(paused) << 1));
                let n = r.rd(EE_SCENARIO_CAP_VARIABLES, c, 0);
                for i in 0..n {
                    let v = r.rd(EE_SCENARIO_CAP_VARIABLE_AT, c, i);
                    r.i32(v as i32);
                }
                let command = r.rd(EE_SCENARIO_CAP_COMMAND, c, 0);
                r.u32(command as u32);
                let queues = r.rd(EE_SCENARIO_CAP_QUEUES, c, 0);
                r.u32(queues as u32);
                for q in 0..queues {
                    let n = r.rd(EE_SCENARIO_CAP_QUEUE_LEN, c, q);
                    r.u32(n as u32);
                }
            }
        }
    } else {
        let pid = if player != 0 { r.rd(EE_SCENARIO_CAP_PLAYER_ID, player, 0) as i32 } else { -1 };
        r.i32(pid);
        r.u32((upgrade != 0) as u32);
        if upgrade != 0 {
            let p = r.ptr(EE_SCENARIO_CAP_UPGRADE_FIELDS, upgrade, 0) as usize as *const EeUnitsUnitClassUpgradeFields;
            // SAFETY: the live upgrade's record
            let u = unsafe { std::ptr::read(p) };
            for a in u.mArmor {
                r.f32(a);
            }
            r.i32(u.mReloadTime as i32);
            r.f32(u.mAttack);
            r.i32(u.mWorkRate as i32);
            r.i32(u.mFlightTime as i32);
            r.f32(u.mLeadership);
            r.i32(u.mMaxObjectCarry as i32);
            r.i32(u.mRepairRate as i32);
            r.i32(u.mMaxHitPoints as i32);
            r.i32(u.mCarryAmount as i32);
            r.i32(u.mHealingPoints as i32);
            r.f32(u.mRange);
            r.f32(u.mLineOfSight);
            r.f32(u.mMaxVelocity);
            r.i32(u.mPower as i32);
            r.u32(b(u.mIsInitialized));
            r.f32(u.mCameraFocus);
        }
    }
    let m = r.manager();
    if m != 0 {
        r.pending_list(m, version);
    }
    r.commit();
}

#[cfg(test)]
#[path = "sm_capture_tests.rs"]
mod tests;
