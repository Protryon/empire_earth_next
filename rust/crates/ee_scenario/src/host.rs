//! The world as the scenario logic reads it: one read by operation code (`EE_SCENARIO_READ_*`) with four integer
//! arguments (unused ones 0) and an integer answer, each the original's expression at its place (the C++ host in
//! `Scenario Manager/EEScenarioHost.cpp`). A read may run game code (an object's or a condition's `Evaluate`, a random
//! draw), so the logic makes each read exactly where and as often as the original did.

use std::ffi::c_void;

/// `gWorld.mNumPlayers`.
pub const EE_SCENARIO_READ_NUM_PLAYERS: u32 = 0;
/// `gWorld.mPlayers[a] != NULL` (1/0).
pub const EE_SCENARIO_READ_PLAYER_EXISTS: u32 = 1;
/// `gWorld.GetPlayer(a)->mHumanPlayer != NULL` (1/0).
pub const EE_SCENARIO_READ_PLAYER_HUMAN: u32 = 2;
/// `gWorld.GetPlayer(a)->mComputerPlayer != NULL` (1/0).
pub const EE_SCENARIO_READ_PLAYER_COMPUTER: u32 = 3;
/// `gPhysics->GetRandom(a, b)` (the physics stream's draw: `long`s, both inclusive).
pub const EE_SCENARIO_READ_RANDOM: u32 = 4;
/// `gWorld.mPlayers[a]->mEnemies[b]`.
pub const EE_SCENARIO_READ_ENEMY: u32 = 5;
/// `gWorld.mPlayers[a]->mResourceCaps[b]` (a `long`).
pub const EE_SCENARIO_READ_RESOURCE: u32 = 6;
/// `GetTotalPopulation(false, false)`.
pub const EE_SCENARIO_READ_POPULATION: u32 = 7;
/// `GetPopulationLimit(0)`.
pub const EE_SCENARIO_READ_POPULATION_LIMIT: u32 = 8;
/// `mUnitCount`.
pub const EE_SCENARIO_READ_UNIT_COUNT: u32 = 9;
/// `mComputerPlayer->mDifficulty`, -1 without a computer player.
pub const EE_SCENARIO_READ_COMPUTER_DIFFICULTY: u32 = 10;
/// `mScenarioVariables[b]`.
pub const EE_SCENARIO_READ_VARIABLE: u32 = 11;
/// `mTributes[b]`.
pub const EE_SCENARIO_READ_TRIBUTE: u32 = 12;
/// `mSavedGroups.GetLastValue(b)` (the statistics).
pub const EE_SCENARIO_READ_STATISTIC: u32 = 13;
/// `mMiningLimit`.
pub const EE_SCENARIO_READ_MINING_LIMIT: u32 = 14;
/// `EEEpochCount()`.
pub const EE_SCENARIO_READ_EPOCH_COUNT: u32 = 15;
/// `GetTechTree()->mCurrentEpoch`.
pub const EE_SCENARIO_READ_EPOCH: u32 = 16;
/// `GetTechTree()->Get(b)`: -1 without a node, else bit 0 `mData->mMakesUnit`, bit 1 `mIsResearched`.
pub const EE_SCENARIO_READ_TECH_NODE: u32 = 17;
/// The node's `GetIsAvailable()` with `mIsResearched` cleared around the call (and restored).
pub const EE_SCENARIO_READ_TECH_AVAILABLE: u32 = 18;
/// `GetScenarioObject(a)`: -1 without an object, else its `Evaluate(0)` (1/0).
pub const EE_SCENARIO_READ_OBJECT: u32 = 19;
/// `GetTrigger(a)`: -1 without a trigger, else bit 0 `mHasFired`, bit 1 `mIsOn`.
pub const EE_SCENARIO_READ_TRIGGER: u32 = 20;
/// The scenario clock `mTime`'s bits (an `f32`).
pub const EE_SCENARIO_READ_TIME: u32 = 21;
/// `gWorld.GetTimeOfDay()`'s bits (an `f32`).
pub const EE_SCENARIO_READ_TIME_OF_DAY: u32 = 22;
/// `gDifficultyLevel`.
pub const EE_SCENARIO_READ_DIFFICULTY: u32 = 23;
/// Whether the last chat text contains the condition's chat text, both lowered (`PlatMbsLwr`; 1/0; `a` unused).
pub const EE_SCENARIO_READ_CHAT_CONTAINS: u32 = 24;
/// `GetCondition(a)`: -1 without a condition, else its `Evaluate()` (1/0).
pub const EE_SCENARIO_READ_CONDITION: u32 = 25;

/// `gContinentManager.GetContinentID(a, b)`.
pub const EE_SCENARIO_READ_CONTINENT: u32 = 26;
/// `gContinentManager.GetContinentCenter(a, ...)`: the tile packed (x in the low 32 bits, y in the high).
pub const EE_SCENARIO_READ_CONTINENT_CENTER: u32 = 27;
/// `gWorld.GetPlayer(a)->GetNumTowns()`.
pub const EE_SCENARIO_READ_TOWN_COUNT: u32 = 28;
/// `GetPlayer(a)->mTowns[b]->mHull.BoundedContains(U2DPoint(c, d))` (1/0).
pub const EE_SCENARIO_READ_TOWN_CONTAINS: u32 = 29;
/// `GetPlayer(a)->mTowns[b]->mHull.GetCenter()`, packed.
pub const EE_SCENARIO_READ_TOWN_CENTER: u32 = 30;
/// The host's unit's `GetTileX()`, `GetTileY()`, packed.
pub const EE_SCENARIO_READ_UNIT_TILE: u32 = 31;
/// The host's unit's `GetComplexUnit() != NULL` (1/0).
pub const EE_SCENARIO_READ_UNIT_HAS_COMPLEX: u32 = 32;
/// The host's unit's complex unit's `mU0DC` (its previous tile), packed.
pub const EE_SCENARIO_READ_UNIT_PREVIOUS_TILE: u32 = 33;

/// TEST ONLY: the physics random seed while the simulation capture records (`EESMCaptureSeed()`), -1 while it does
/// not (ee/EEScenarioCapture.h).
pub const EE_SCENARIO_READ_CAPTURE_SEED: u32 = 49;

/// The world's reads.
pub trait Host {
    fn read4(&mut self, op: u32, a: i32, b: i32, c: i32, d: i32) -> i64;
    /// TEST ONLY: a player choice's capture record (SMCP: the choice, the single flag, the result, the players or
    /// `None` for a caller without a list, the seed before).
    fn players(&mut self, _kind: i32, _single: bool, _result: i32, _players: Option<&[i32]>, _seed: u32) {}
    /// A read of two arguments.
    fn read(&mut self, op: u32, a: i32, b: i32) -> i64 {
        self.read4(op, a, b, 0, 0)
    }
}

/// The C ABI's host: a context and the read callback.
#[repr(C)]
pub struct EeScenarioHost {
    pub ctx: *mut c_void,
    pub read: unsafe extern "C" fn(ctx: *mut c_void, op: u32, a: i32, b: i32, c: i32, d: i32) -> i64,
    /// TEST ONLY: [`Host::players`] (`players` null when `listed` is false).
    pub players: unsafe extern "C" fn(
        ctx: *mut c_void,
        kind: i32,
        single: bool,
        result: i32,
        listed: bool,
        players: *const i32,
        count: usize,
        seed: u32,
    ),
}

impl Host for EeScenarioHost {
    fn read4(&mut self, op: u32, a: i32, b: i32, c: i32, d: i32) -> i64 {
        // SAFETY: the table's contract: `ctx` is live for the call that handed the table over and `read` is callable
        // with it
        unsafe { (self.read)(self.ctx, op, a, b, c, d) }
    }
    fn players(&mut self, kind: i32, single: bool, result: i32, players: Option<&[i32]>, seed: u32) {
        let (listed, at, count) = match players {
            Some(p) => (true, p.as_ptr(), p.len()),
            None => (false, std::ptr::null(), 0),
        };
        // SAFETY: as `read`; `at` points to `count` players for the call
        unsafe { (self.players)(self.ctx, kind, single, result, listed, at, count, seed) }
    }
}
