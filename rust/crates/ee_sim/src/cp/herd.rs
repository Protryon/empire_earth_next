//! `EEHerd` whole (the AI Thread cluster, round 2): a herd of wild animals as an AI area (EEHerd.cpp 0xa1910..0xa543d).
//! The herd's members are the record [`EeHerdFields`] the C++ class derives from (`class EEHerd : public EEAIArea,
//! public EeHerdFields`, ee/EEHerd.h: the names, types and meanings of the original's members, so their readers keep
//! them); its lists (the units, the looks by unit id, a loaded game's tiles) are Rust's ([`EeHerdLists`]). Every body is
//! here, in the original's order of reads, draws and calls, over the world the herd asks ([`HerdHost`]: the units, the
//! goals, the formations, the map, the continent manager, the area the herd is; the game's host is
//! `ee_cp_herd_host`, EEHerd.cpp).
//!
//! The record is reached through a raw pointer and never borrowed across a host call: a call can come back into the
//! herd (a newborn placed by `GiveBirth` joins it through the player's `AssignHerds`).

#![allow(non_snake_case)]
// `c_long`/`c_ulong` are 32 bits on Windows and 64 elsewhere
#![allow(clippy::unnecessary_cast, clippy::useless_conversion)]
// the original's tests read as it wrote them (`!(a < b)` is not `a >= b` for a NaN)
#![allow(clippy::neg_cmp_op_on_partial_ord, clippy::field_reassign_with_default)]

use std::collections::BTreeMap;
use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};

use ee_base::math::types::Ge3DPoint;
use ee_base::math::um;
use ee_data::tables::EeDataAnimalFields;

use super::file::SaveFile;

/// `EEUnitHandle`'s none.
pub const EE_CP_HERD_NONE: u32 = 0xffff_ffff;

/// `U2DRectangle<long>` (its layout: bottom, left, right, top).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeHerdRect {
    pub mBottom: c_long,
    pub mLeft: c_long,
    pub mRight: c_long,
    pub mTop: c_long,
}

impl EeHerdRect {
    /// `Extend`.
    fn extend(&mut self, x: c_long, y: c_long) {
        self.mLeft = self.mLeft.min(x);
        self.mRight = self.mRight.max(x);
        self.mTop = self.mTop.min(y);
        self.mBottom = self.mBottom.max(y);
    }
}

/// `U2DPoint<long>`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeHerdPoint {
    pub mX: c_long,
    pub mY: c_long,
}

impl EeHerdPoint {
    fn new(x: c_long, y: c_long) -> EeHerdPoint {
        EeHerdPoint { mX: x, mY: y }
    }
}

/// The herd's lists (Rust's): the units (`mUnits`, unit handles in the original's order), the looks by unit id
/// (`mMedia`, `EEMedia*`), a loaded game's area tiles (from Load to RepairPointers; the original's side table).
#[derive(Default)]
pub struct EeHerdLists {
    pub units: Vec<u32>,
    media: BTreeMap<c_ulong, u64>,
    loaded_tiles: Option<Vec<EeHerdPoint>>,
}

/// `EEHerd`'s members (+0x68 .. +0xf9 in the original; the media map is in [`EeHerdLists`]).
#[repr(C)]
#[derive(Debug)]
pub struct EeHerdFields {
    /// +0x84 the territory
    pub mBounds2: EeHerdRect,
    /// +0x94
    pub mP94: EeHerdPoint,
    /// +0x9c the target's tile
    pub mCenter: EeHerdPoint,
    /// +0xa4 `EEAnimal*` (a load's saved index until RepairPointers)
    pub mAnimal: u64,
    /// +0xa8 `UReferenceCounter*`: a path request (cancelled through the path manager)
    pub mPathRequest: u64,
    /// +0xac `EEPlayer*` (a load's saved id until RepairPointers)
    pub mPlayer: u64,
    /// +0xb0 the leader (a unit handle: its id)
    pub mLeader: u32,
    /// +0xb4 the target (a unit handle)
    pub mTarget: u32,
    pub mFB8: f32,
    /// +0xbc the animal index (the units' class `mAnimalID`)
    pub mAnimalID: c_ulong,
    /// +0xc0 an area serial (`gContinentManager.GetArea`)
    pub mGrazeAreaID: c_ulong,
    /// +0xc4 (0, 2, 3, 4, 7, 9, 0xa, 0xb)
    pub mState: c_ulong,
    pub mLC8: c_ulong,
    /// +0xcc the next birth
    pub mBirthTimer: c_ulong,
    pub mFollowTime: c_ulong,
    /// +0xd4 the next update
    pub mNextUpdateTime: c_ulong,
    pub mLD8: c_ulong,
    pub mChaseTime: c_ulong,
    pub mHuntTime: c_ulong,
    pub mLE4: c_ulong,
    /// +0xe8 the animal's herd radius squared (the joining range)
    pub mRadiusSquared: c_long,
    /// +0xec the young ones
    pub mNumYoung: c_long,
    /// +0xf0 the most units
    pub mMaxUnits: c_long,
    pub mContinent: u16,
    pub mID: u16,
    pub mBreeds: bool,
    pub mIsActive: bool,
    /// the lists (owned; `ee_cp_herd_init` makes them, `ee_cp_herd_free` drops them)
    pub mLists: *mut EeHerdLists,
}

/// A unit as the herd reads it (`EEComplexUnit` and its class; one host call).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeHerdUnit {
    /// `mUnitID`
    pub unit_id: c_ulong,
    /// `EEUnitHandleID`: what a handle to it holds
    pub handle: u32,
    pub tile_x: c_long,
    pub tile_y: c_long,
    pub hit_points: c_long,
    pub growth: f32,
    pub graze_time: c_ulong,
    pub grow_time: c_ulong,
    /// `mCommand ? mCommand->mType : 0`
    pub command: c_long,
    /// `mWaypoints.size()`
    pub waypoints: i64,
    /// `mHeldUnits.size()`
    pub held: i64,
    /// `mHolder`
    pub holder: u64,
    /// `mHerd` (`EEHerd*`)
    pub herd: u64,
    /// `mPlayer`
    pub player: u64,
    pub last_attacker: u64,
    /// `mStats->mModel` (its `EEMedia`)
    pub model: u64,
    /// the class's `mAttack`, `mAmmoType`, `mFamily`, `mAnimalID`
    pub attack: c_long,
    pub ammo_type: c_long,
    pub family: c_ulong,
    pub animal_id: c_long,
    pub disabled: bool,
    /// `mStats->mIsInitialized`
    pub initialized: bool,
    /// `GetGoal() != NULL`
    pub has_goal: bool,
    pub regrowing: bool,
    pub b34f: bool,
    /// the class's `CanBeAttacked()`
    pub can_be_attacked: bool,
}

/// One host operation's values.
#[repr(C)]
pub struct EeHerdIo {
    pub v: [i64; 6],
    pub f: [f32; 4],
    pub unit: EeHerdUnit,
    /// the units an operation reads (`EEComplexUnit*`)
    pub input: *const u64,
    pub input_len: usize,
    /// the lists an operation pushes onto (`ee_cp_herd_push`: four values a row)
    pub list: *mut c_void,
    pub list2: *mut c_void,
}

impl Default for EeHerdIo {
    fn default() -> Self {
        EeHerdIo {
            v: [0; 6],
            f: [0.0; 4],
            unit: EeHerdUnit::default(),
            input: std::ptr::null(),
            input_len: 0,
            list: std::ptr::null_mut(),
            list2: std::ptr::null_mut(),
        }
    }
}

/// Pushes a row onto an operation's list (`EeHerdIo::list`, `list2`).
///
/// # Safety
/// `list` is a list of a running operation (a `Vec<[i64; 4]>`).
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_push(list: *mut c_void, a: i64, b: i64, c: i64, d: i64) {
    // SAFETY: the caller's contract
    unsafe { (*(list as *mut Vec<[i64; 4]>)).push([a, b, c, d]) }
}

// ---- the host operations (`a`, `b` objects; `io.v`, `io.f` values; the result)
/// unit `a` into `io.unit`
pub const EE_CP_HERD_UNIT: i32 = 1;
/// the unit of handle `v0` (`EEComplexUnit*`, 0 none)
pub const EE_CP_HERD_RESOLVE: i32 = 2;
/// `a->mReferenceCount++`
pub const EE_CP_HERD_REF: i32 = 3;
/// `a->Dereference()`
pub const EE_CP_HERD_DEREF: i32 = 4;
/// `a->SetHerd(b)`
pub const EE_CP_HERD_SET_HERD: i32 = 5;
/// `a->SetGrazeTime(v0)`
pub const EE_CP_HERD_SET_GRAZE: i32 = 6;
/// `a->SetGrowth(f0)`
pub const EE_CP_HERD_SET_GROWTH: i32 = 7;
/// `a->SetGrowTime(v0)`
pub const EE_CP_HERD_SET_GROW_TIME: i32 = 8;
/// `gPhysics->GetRandom(v0, v1)` (long)
pub const EE_CP_HERD_RANDOM_LONG: i32 = 9;
/// `gPhysics->GetRandom(f0, f1)` into `f0`
pub const EE_CP_HERD_RANDOM_FLOAT: i32 = 10;
/// `gPhysics->GetRandom()` into `f0`
pub const EE_CP_HERD_RANDOM: i32 = 11;
/// `gPhysics->mRandom.Generate()` into `f0`
pub const EE_CP_HERD_GENERATE: i32 = 12;
/// `gPhysics->mGameTime` (-1 without physics)
pub const EE_CP_HERD_GAME_TIME: i32 = 13;
/// `gContinentManager.GetContinentID(a)`
pub const EE_CP_HERD_CONTINENT_OF: i32 = 14;
/// `gContinentManager.GetContinentID(v0, v1)`
pub const EE_CP_HERD_CONTINENT_AT: i32 = 15;
/// a continent (v1: 0 `GetContinentFromID(v0)`, 1 `GetContinentByID(v0)`, 2 `GetContinents()[v0]`): whether there
/// is one, its category into `v0` and its hull's bounds into `v1..v4` (bottom, left, right, top)
pub const EE_CP_HERD_CONTINENT: i32 = 16;
/// area `a`'s `GetRandomPoint()` into `v0`, `v1`
pub const EE_CP_HERD_AREA_POINT: i32 = 17;
/// `gContinentManager.GetArea(v0)`
pub const EE_CP_HERD_AREA: i32 = 18;
/// `gContinentManager.GetRandomContinent(kContinentCategoryMapEdge, gPhysics->mRandom)`
pub const EE_CP_HERD_MAP_EDGE: i32 = 19;
/// `EEAIArea::Create` of the herd over the `input_len` tiles at `input` (x, y pairs)
pub const EE_CP_HERD_AREA_CREATE: i32 = 20;
/// `GetNumTiles()`
pub const EE_CP_HERD_NUM_TILES: i32 = 21;
/// `HullContains((v0, v1))`
pub const EE_CP_HERD_HULL_CONTAINS: i32 = 22;
/// `GetClosestHullPoint((v0, v1))` into `v0`, `v1`
pub const EE_CP_HERD_CLOSEST_HULL: i32 = 23;
/// `GetHullBounds()` into `v0..v3` (bottom, left, right, top)
pub const EE_CP_HERD_HULL_BOUNDS: i32 = 24;
/// `((EEHerd*)a)->GetHullDistanceSquared((EEHerd*)b)` into `f0`
pub const EE_CP_HERD_HULL_DISTANCE: i32 = 25;
/// `EEAIArea::GetRandomPoint()` into `v0`, `v1`
pub const EE_CP_HERD_BASE_POINT: i32 = 26;
/// `GetTileSet().BuildPointList(tiles, 1)` (pushed)
pub const EE_CP_HERD_TILES: i32 = 27;
/// `EEGetUnitsInRectangle` of the rectangle `v0..v3` (bottom, left, right, top) with the herd's filter (continent
/// `v4`, `v5` bit 0 huntable, bit 1 moving): the units pushed
pub const EE_CP_HERD_UNITS_IN_RECT: i32 = 28;
/// an inward circle (`EEFormation::Create(6)`) placed over the `input` units: `v0` 0 Chase (centre `f0..f2`, a random
/// radius, kept), 1 MoveTo (centre, radius `f3`; `v1` 0: not placed), 2 Surround (centre, a random radius); the units
/// after `Place` into `list2`, the slots (x, y, z, heading as float bits) into `list`
pub const EE_CP_HERD_FORMATION: i32 = 29;
/// `CanStandOn(a, (v0, v1))`; the herd's units at `input`
pub const EE_CP_HERD_STAND: i32 = 30;
pub const EE_CP_HERD_MEDIA_LOAD: i32 = 31;
pub const EE_CP_HERD_MEDIA_UNLOAD: i32 = 32;
/// `EEAnimal::Get(v0)` (`sTable[v0]`)
pub const EE_CP_HERD_ANIMAL: i32 = 33;
/// the animal `a`'s record (`static_cast<EeDataAnimalFields*>`)
pub const EE_CP_HERD_ANIMAL_FIELDS: i32 = 34;
/// the animal `a`'s `mMedia`
pub const EE_CP_HERD_ANIMAL_MEDIA: i32 = 35;
/// `a->mUnitClass->Flee(a)`
pub const EE_CP_HERD_FLEE: i32 = 36;
/// `EEDamageFactor(v0, v1)` into `f0`
pub const EE_CP_HERD_DAMAGE_FACTOR: i32 = 37;
/// `a`'s last attacker dereferenced and cleared
pub const EE_CP_HERD_CLEAR_ATTACKER: i32 = 38;
/// `a` released by its holder (`ReleaseHeld`, the holder dereferenced, `mHolder` cleared)
pub const EE_CP_HERD_RELEASE_HOLDER: i32 = 39;
/// `a->mHeldUnits[v0]`
pub const EE_CP_HERD_HELD: i32 = 40;
/// `a->mWaypoints[v0].mLocation` into `f0..f2`
pub const EE_CP_HERD_WAYPOINT: i32 = 41;
/// `a->GetPosition()` into `f0..f2`
pub const EE_CP_HERD_POSITION: i32 = 42;
/// `a->DistanceSquaredTo(b)` into `f0`
pub const EE_CP_HERD_DISTANCE: i32 = 43;
/// `gWorld.mMap->GetTile(v0, v1) != NULL`
pub const EE_CP_HERD_MAP_TILE: i32 = 44;
/// `gWorld.mMap->GetTileAtLocation((v0, v1)) != NULL`; -1: no map
pub const EE_CP_HERD_MAP_TILE_AT: i32 = 45;
/// `gWorld.mMap->mTerrainMesh.GetZ(v0, v1)` into `f0`
pub const EE_CP_HERD_MAP_Z: i32 = 46;
/// `gWorld.GetMap()->GetSize()`; `gMapWidth`, `gMapHeight` into `v0`, `v1`
pub const EE_CP_HERD_MAP_SIZE: i32 = 47;
/// a young one of animal `b` born by parent `a` (`CreateUnit`, its growth, grow time, search timer, `HoldUnit`)
pub const EE_CP_HERD_BIRTH: i32 = 48;
/// `gPathManager.CancelAutoPathRequest(a)`, then `a->Dereference()`
pub const EE_CP_HERD_CANCEL_PATH: i32 = 49;
/// the herds of player `a` (`EEPlayer::GetHerds`' order: from the last), pushed (`EEHerd*`)
pub const EE_CP_HERD_PLAYER_HERDS: i32 = 50;
/// herd `a`'s record (`static_cast<EeHerdFields*>`)
pub const EE_CP_HERD_FIELDS_OF: i32 = 51;
/// `a->mGoalQueues[0].mGoals.size()`
pub const EE_CP_HERD_GOALS: i32 = 52;
/// `a`'s queued goal `v0`: a location goal's location into `f0`, `f1`; whether it is one
pub const EE_CP_HERD_GOAL_LOCATION: i32 = 53;
/// `a->GetPhysicalModel().GetLocation()` into `f0`, `f1`
pub const EE_CP_HERD_PHYSICAL: i32 = 54;
/// `gLoadingSavedGame` (bit 0), `gWorldClearing` (bit 1)
pub const EE_CP_HERD_FLAGS: i32 = 55;
/// an attack goal on `b` (`NewAttackUnit(a, b, true, false)`) given to `a` (`GiveGoal(.., 0,
/// kActionInitialAttackState)`, else deleted); whether it was taken
pub const EE_CP_HERD_GOAL_ATTACK: i32 = 56;
/// a location goal (`NewLocation(&(f0, f1, f2), v0, v1, v2, f3, false, false)`; `v4` 1: the three-float overload)
/// given to `a` (`GiveGoal(.., v3, kActionPrepareToMove)`, else deleted); whether it was taken
pub const EE_CP_HERD_GOAL_MOVE: i32 = 57;
/// `gWorld.GetPlayer(v0)`
pub const EE_CP_HERD_PLAYER: i32 = 58;
/// player `a`'s `mPlayerID`
pub const EE_CP_HERD_PLAYER_ID: i32 = 59;
/// `dynamic_cast<EEComplexUnit*>(gWorld.GetUnit(v0))`
pub const EE_CP_HERD_UNIT_BY_ID: i32 = 60;
/// `EESaveHasLiveState(v0, v1)` (bit 0), `EESaveHasContinentRegions(v0, v1)` (bit 1)
pub const EE_CP_HERD_SAVE_FLAGS: i32 = 61;
/// `EEHerd::IsAnimalOnMap(a)`
pub const EE_CP_HERD_ANIMAL_ON_MAP: i32 = 62;
/// whether the world has a map (`gWorld.mMap`)
pub const EE_CP_HERD_HAS_MAP: i32 = 63;

/// `kActionIdle`, `kActionCircle` (ee/EEUnitAction.h).
const ACTION_IDLE: c_long = 5000;
const ACTION_CIRCLE: c_long = 1008;
/// `kContinentCategoryWater`.
const CONTINENT_WATER: i64 = 2;
/// `kHeightEpsilon`.
const HEIGHT_EPSILON: f32 = 0.01;
/// a unit off the map (`GetTileX()`).
const OFF_MAP: c_long = -10000;

/// The world the herd asks.
pub trait HerdHost {
    /// The `EEHerd*` the herd is.
    fn this(&self) -> u64;
    fn call(&mut self, op: i32, a: u64, b: u64, io: &mut EeHerdIo) -> i64;
}

/// A slot of a placed formation.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
struct Slot {
    position: Ge3DPoint,
    heading: f32,
}

/// The herd over its record and its host.
pub struct Herd<'h, H: HerdHost + ?Sized> {
    h: &'h mut H,
    f: *mut EeHerdFields,
}

impl<'h, H: HerdHost + ?Sized> Herd<'h, H> {
    /// # Safety
    /// `f` is a live record with its lists for as long as the value lives.
    pub unsafe fn new(h: &'h mut H, f: *mut EeHerdFields) -> Self {
        Herd { h, f }
    }

    // ---- the record (short borrows only: never across a host call)
    #[allow(clippy::mut_from_ref)]
    fn f(&self) -> &mut EeHerdFields {
        // SAFETY: the contract of `new`; no borrow outlives the expression that takes it
        unsafe { &mut *self.f }
    }
    #[allow(clippy::mut_from_ref)]
    fn lists(&self) -> &mut EeHerdLists {
        // SAFETY: as `f`; the lists live as long as the record
        unsafe { &mut *self.f().mLists }
    }
    fn unit_count(&self) -> usize {
        self.lists().units.len()
    }
    fn unit_handle(&self, i: usize) -> u32 {
        self.lists().units[i]
    }
    fn animal(&mut self) -> EeDataAnimalFields {
        let a = self.f().mAnimal;
        self.animal_of(a)
    }

    // ---- the host's operations
    fn op0(&mut self, op: i32, a: u64, b: u64) -> i64 {
        let mut io = EeHerdIo::default();
        self.h.call(op, a, b, &mut io)
    }
    fn opv(&mut self, op: i32, a: u64, v: &[i64]) -> (i64, EeHerdIo) {
        let mut io = EeHerdIo::default();
        io.v[..v.len()].copy_from_slice(v);
        let r = self.h.call(op, a, 0, &mut io);
        (r, io)
    }
    fn unit(&mut self, u: u64) -> EeHerdUnit {
        let mut io = EeHerdIo::default();
        self.h.call(EE_CP_HERD_UNIT, u, 0, &mut io);
        io.unit
    }
    fn resolve(&mut self, handle: u32) -> u64 {
        self.opv(EE_CP_HERD_RESOLVE, 0, &[i64::from(handle)]).0 as u64
    }
    /// `mUnits[i]` as the original's handle read it.
    fn unit_at(&mut self, i: usize) -> u64 {
        let handle = self.unit_handle(i);
        self.resolve(handle)
    }
    /// `mUnits` as a list of units (`assign(mUnits.begin(), mUnits.end())`).
    fn units(&mut self) -> Vec<u64> {
        (0..self.unit_count()).map(|i| self.unit_at(i)).collect()
    }
    fn reference(&mut self, u: u64) {
        self.op0(EE_CP_HERD_REF, u, 0);
    }
    fn dereference(&mut self, u: u64) {
        self.op0(EE_CP_HERD_DEREF, u, 0);
    }
    fn set_herd(&mut self, u: u64, herd: u64) {
        self.op0(EE_CP_HERD_SET_HERD, u, herd);
    }
    fn set_graze(&mut self, u: u64, t: c_ulong) {
        self.opv(EE_CP_HERD_SET_GRAZE, u, &[t as i64]);
    }
    fn random_long(&mut self, min: c_long, max: c_long) -> c_long {
        self.opv(EE_CP_HERD_RANDOM_LONG, 0, &[i64::from(min), i64::from(max)]).0 as c_long
    }
    fn random_float(&mut self, min: f32, max: f32) -> f32 {
        let mut io = EeHerdIo::default();
        io.f[0] = min;
        io.f[1] = max;
        self.h.call(EE_CP_HERD_RANDOM_FLOAT, 0, 0, &mut io);
        io.f[0]
    }
    fn float_op(&mut self, op: i32, a: u64, b: u64) -> f32 {
        let mut io = EeHerdIo::default();
        self.h.call(op, a, b, &mut io);
        io.f[0]
    }
    fn game_time(&mut self) -> c_ulong {
        self.op0(EE_CP_HERD_GAME_TIME, 0, 0) as c_ulong
    }
    fn continent_at(&mut self, x: c_long, y: c_long) -> u16 {
        self.opv(EE_CP_HERD_CONTINENT_AT, 0, &[i64::from(x), i64::from(y)]).0 as u16
    }
    /// A continent's category and hull bounds (`None`: no continent).
    fn continent(&mut self, id: u16, mode: i64) -> Option<(i64, EeHerdRect)> {
        let (r, io) = self.opv(EE_CP_HERD_CONTINENT, 0, &[i64::from(id), mode]);
        (r != 0).then(|| (io.v[0], rect_of(&io.v[1..5])))
    }
    fn hull_contains(&mut self, x: c_long, y: c_long) -> bool {
        self.opv(EE_CP_HERD_HULL_CONTAINS, 0, &[i64::from(x), i64::from(y)]).0 != 0
    }
    fn hull_bounds(&mut self) -> EeHerdRect {
        let (_, io) = self.opv(EE_CP_HERD_HULL_BOUNDS, 0, &[]);
        rect_of(&io.v[0..4])
    }
    fn num_tiles(&mut self) -> i64 {
        self.op0(EE_CP_HERD_NUM_TILES, 0, 0)
    }
    fn map_tile(&mut self, x: c_long, y: c_long) -> bool {
        self.opv(EE_CP_HERD_MAP_TILE, 0, &[i64::from(x), i64::from(y)]).0 != 0
    }
    fn flags(&mut self) -> (bool, bool) {
        let r = self.op0(EE_CP_HERD_FLAGS, 0, 0);
        (r & 1 != 0, r & 2 != 0)
    }
    fn animal_of(&mut self, animal: u64) -> EeDataAnimalFields {
        let p = self.op0(EE_CP_HERD_ANIMAL_FIELDS, animal, 0) as usize as *const EeDataAnimalFields;
        // SAFETY: the host hands a live animal's record (a copy is taken at once)
        unsafe { *p }
    }
    fn position(&mut self, u: u64) -> Ge3DPoint {
        let mut io = EeHerdIo::default();
        self.h.call(EE_CP_HERD_POSITION, u, 0, &mut io);
        Ge3DPoint { x: io.f[0], y: io.f[1], z: io.f[2] }
    }
    fn media(&mut self, op: i32, media: u64) {
        self.op0(op, media, 0);
    }
    /// The area's points list (`input`).
    fn area_create(&mut self, tiles: &[EeHerdPoint]) {
        let flat: Vec<u64> = tiles.iter().flat_map(|t| [t.mX as i64 as u64, t.mY as i64 as u64]).collect();
        let mut io = EeHerdIo { input: flat.as_ptr(), input_len: tiles.len(), ..Default::default() };
        self.h.call(EE_CP_HERD_AREA_CREATE, 0, 0, &mut io);
    }
    /// `GiveGoal` of an attack goal on `target`.
    fn goal_attack(&mut self, u: u64, target: u64) -> bool {
        self.op0(EE_CP_HERD_GOAL_ATTACK, u, target) != 0
    }
    /// `GiveGoal` of a location goal.
    #[allow(clippy::too_many_arguments)]
    fn goal_move(
        &mut self,
        u: u64,
        at: Ge3DPoint,
        b1: bool,
        b2: bool,
        face: bool,
        heading: f32,
        keep: bool,
        floats: bool,
    ) -> bool {
        let mut io = EeHerdIo::default();
        io.f = [at.x, at.y, at.z, heading];
        io.v[..5].copy_from_slice(&[i64::from(b1), i64::from(b2), i64::from(face), i64::from(keep), i64::from(floats)]);
        self.h.call(EE_CP_HERD_GOAL_MOVE, u, 0, &mut io) != 0
    }
    /// The inward circle placed over `units` (the units after `Place`, the slots).
    fn formation(
        &mut self,
        mode: i64,
        center: Ge3DPoint,
        radius: f32,
        place: bool,
        units: &[u64],
    ) -> (Vec<u64>, Vec<Slot>) {
        let mut slots: Vec<[i64; 4]> = Vec::new();
        let mut placed: Vec<[i64; 4]> = Vec::new();
        let mut io = EeHerdIo {
            input: units.as_ptr(),
            input_len: units.len(),
            list: &mut slots as *mut Vec<[i64; 4]> as *mut c_void,
            list2: &mut placed as *mut Vec<[i64; 4]> as *mut c_void,
            ..Default::default()
        };
        io.v[0] = mode;
        io.v[1] = i64::from(place);
        io.f = [center.x, center.y, center.z, radius];
        self.h.call(EE_CP_HERD_FORMATION, 0, 0, &mut io);
        let units = placed.iter().map(|r| r[0] as u64).collect();
        let slots = slots
            .iter()
            .map(|r| Slot {
                position: Ge3DPoint {
                    x: f32::from_bits(r[0] as u32),
                    y: f32::from_bits(r[1] as u32),
                    z: f32::from_bits(r[2] as u32),
                },
                heading: f32::from_bits(r[3] as u32),
            })
            .collect();
        (units, slots)
    }
    fn pushed(&mut self, op: i32, a: u64, v: &[i64]) -> Vec<[i64; 4]> {
        let mut rows: Vec<[i64; 4]> = Vec::new();
        let mut io = EeHerdIo { list: &mut rows as *mut Vec<[i64; 4]> as *mut c_void, ..Default::default() };
        io.v[..v.len()].copy_from_slice(v);
        self.h.call(op, a, 0, &mut io);
        rows
    }
    fn units_in_rect(&mut self, r: EeHerdRect, continent: u16, huntable: bool, moving: bool) -> Vec<u64> {
        let flags = i64::from(huntable) | (i64::from(moving) << 1);
        let v = [
            i64::from(r.mBottom),
            i64::from(r.mLeft),
            i64::from(r.mRight),
            i64::from(r.mTop),
            i64::from(continent),
            flags,
        ];
        self.pushed(EE_CP_HERD_UNITS_IN_RECT, 0, &v).iter().map(|r| r[0] as u64).collect()
    }
    fn stand(&mut self, u: u64, x: c_long, y: c_long) -> bool {
        let units = self.units();
        let mut io = EeHerdIo { input: units.as_ptr(), input_len: units.len(), ..Default::default() };
        io.v[0] = i64::from(x);
        io.v[1] = i64::from(y);
        self.h.call(EE_CP_HERD_STAND, u, 0, &mut io) != 0
    }

    // ---- the record's own steps
    /// `ReleaseLeader`.
    fn release_leader(&mut self) {
        let leader = self.f().mLeader;
        let p = self.resolve(leader);
        if p != 0 {
            self.dereference(p);
        }
        self.f().mLeader = EE_CP_HERD_NONE;
    }
    /// `ReleaseTarget`.
    fn release_target(&mut self) {
        let target = self.f().mTarget;
        let p = self.resolve(target);
        if p != 0 {
            self.dereference(p);
        }
        self.f().mTarget = EE_CP_HERD_NONE;
    }
    fn target(&mut self) -> u64 {
        let target = self.f().mTarget;
        self.resolve(target)
    }

    /// The constructor's body (`EEHerd(player, animal, id)` after `Clear`).
    pub fn construct(&mut self, player: u64, animal_index: c_long, id: u16) {
        self.f().mLeader = EE_CP_HERD_NONE;
        self.f().mPathRequest = 0;
        self.f().mTarget = EE_CP_HERD_NONE;
        self.clear();
        self.f().mPlayer = player;
        self.f().mID = id;
        self.f().mAnimalID = animal_index as c_ulong;
        let animal = self.opv(EE_CP_HERD_ANIMAL, 0, &[i64::from(animal_index)]).0 as u64;
        self.f().mAnimal = animal;
        self.f().mPathRequest = 0;
        let a = self.animal();
        self.f().mRadiusSquared = a.mHerdRadius.wrapping_mul(a.mHerdRadius);
        self.f().mBreeds = a.mBreeds;
        if self.f().mBreeds {
            let t = self.random_long(a.mMinBirthInterval, a.mMaxBirthInterval);
            self.f().mBirthTimer = t as c_ulong;
        }
        let n = self.random_long(a.mMinHerdSize, a.mMaxHerdSize);
        self.f().mMaxUnits = n;
    }

    /// `Clear`: no units, looks, path request, leader or target; the timers and state zeroed (the area kind is the
    /// C++'s: `SetAreaType(kAIAreaHerd)`).
    pub fn clear(&mut self) {
        self.clear_units();
        self.f().mBounds2 = EeHerdRect::default();
        self.f().mCenter = EeHerdPoint::default();
        self.f().mAnimal = 0;
        self.f().mPlayer = 0;
        let request = self.f().mPathRequest;
        if request != 0 {
            self.op0(EE_CP_HERD_CANCEL_PATH, request, 0);
            self.f().mPathRequest = 0;
        }
        self.release_leader();
        self.release_target();
        let f = self.f();
        f.mNumYoung = 0;
        f.mP94 = EeHerdPoint::default();
        f.mFB8 = 0.0;
        f.mFollowTime = 0;
        f.mGrazeAreaID = 0;
        f.mState = 0;
        f.mBirthTimer = 0;
        f.mLE4 = 0;
        f.mNextUpdateTime = 0;
        f.mLD8 = 0;
        f.mMaxUnits = 0;
        f.mContinent = 0;
        f.mID = 0;
        f.mBreeds = false;
        f.mLC8 = 0;
        f.mChaseTime = 0;
        f.mHuntTime = 0;
        f.mAnimalID = 0x3e8;
        f.mRadiusSquared = 0x169;
        f.mIsActive = true;
    }

    /// `ClearUnits`: every unit released (those still in this herd detached), the looks forgotten (not unloaded).
    pub fn clear_units(&mut self) {
        let this = self.h.this();
        for i in (0..self.unit_count()).rev() {
            let u = self.unit_at(i);
            if self.unit(u).herd == this {
                self.set_herd(u, 0);
            }
            self.dereference(u);
        }
        self.lists().units.clear();
        self.lists().media.clear();
    }

    /// `AddUnit`: an animal joins (unchecked once the herd has tiles, else if `CanTake` accepts it); the area is made
    /// again when it is adult or the territory small.
    pub fn add_unit(&mut self, unit: u64) -> bool {
        let mut tiles = Vec::new();
        let mut result = false;
        if self.num_tiles() != 0 || self.can_take(unit, true) {
            result = true;
            let u = self.unit(unit);
            if u.growth != 1.0 {
                self.f().mNumYoung = self.f().mNumYoung.wrapping_add(1);
            }
            let continent = self.op0(EE_CP_HERD_CONTINENT_OF, unit, 0) as u16;
            self.f().mContinent = continent;
            self.reference(unit);
            let this = self.h.this();
            self.set_herd(unit, this);
            self.lists().units.push(u.handle);
            let u = self.unit(unit);
            self.f().mIsActive = u.initialized;
            let a = self.animal();
            let now = self.op0(EE_CP_HERD_GAME_TIME, 0, 0);
            if now >= 0 {
                let r = self.random_long(a.mMinGrazeTime, a.mMaxGrazeTime);
                self.set_graze(unit, (now as c_long).wrapping_add(r) as c_ulong);
            } else {
                self.set_graze(unit, u.graze_time.wrapping_add(a.mMinGrazeTime as c_ulong));
            }
            for i in (0..self.unit_count()).rev() {
                let p = self.unit_at(i);
                let t = self.unit(p);
                tiles.push(EeHerdPoint::new(t.tile_x, t.tile_y));
            }
            let u = self.unit(unit);
            self.set_unit_media(u.model, u.unit_id);
            let b = self.f().mBounds2;
            if u.growth == 1.0 || (b.mRight.wrapping_sub(b.mLeft) <= 4 && b.mBottom.wrapping_sub(b.mTop) <= 4) {
                self.create(&tiles);
            }
            if self.unit(unit).regrowing {
                self.f().mState = 10;
            }
        }
        result
    }

    /// `HandleUnitHit`: a predator strikes back one time in four at an attacker it can hurt, else the animal flees;
    /// its last attacker is forgotten.
    pub fn handle_unit_hit(&mut self, unit: u64) {
        let u = self.unit(unit);
        let attacker = u.last_attacker;
        let herd = u.herd;
        let herd_animal = {
            let p = self.op0(EE_CP_HERD_FIELDS_OF, herd, 0) as usize as *const EeHerdFields;
            // SAFETY: the unit's herd is a live herd (the original read it the same way)
            unsafe { (*p).mAnimal }
        };
        let predator = self.animal_of(herd_animal).mIsPredator;
        if predator && !(self.float_op(EE_CP_HERD_GENERATE, 0, 0) < 0.75) {
            if attacker != 0 && u.attack > 0 {
                let target = u.ammo_type;
                let a = self.unit(attacker);
                let source = a.family;
                if a.can_be_attacked && self.damage_factor(source as c_long, target) > 0.0 {
                    self.attack(attacker, &[unit]);
                }
            }
        } else {
            self.op0(EE_CP_HERD_FLEE, unit, 0);
        }
        self.op0(EE_CP_HERD_CLEAR_ATTACKER, unit, 0);
    }

    fn damage_factor(&mut self, source: c_long, target: c_long) -> f32 {
        let mut io = EeHerdIo::default();
        io.v[0] = i64::from(source);
        io.v[1] = i64::from(target);
        self.h.call(EE_CP_HERD_DAMAGE_FACTOR, 0, 0, &mut io);
        io.f[0]
    }

    /// `Attack`: an attack goal per unit, from the last.
    fn attack(&mut self, target: u64, units: &[u64]) {
        for &u in units.iter().rev() {
            self.goal_attack(u, target);
        }
    }

    /// `Attack` over the herd's own list (read live, as the original's reference was).
    fn attack_all(&mut self, target: u64) {
        for i in (0..self.unit_count()).rev() {
            let u = self.unit_at(i);
            self.goal_attack(u, target);
        }
    }

    fn target_moving(&mut self) -> bool {
        let t = self.target();
        if t == 0 {
            return false;
        }
        let u = self.unit(t);
        u.tile_x != OFF_MAP && u.waypoints > 1 && u.hit_points > 0
    }

    /// `Chase`: on a water continent, a random moving ship when the target is lost; the herd circles it or heads for
    /// one of its next waypoints. Whether there is a live moving target.
    pub fn chase(&mut self) -> bool {
        let continent = self.f().mContinent;
        if let Some((category, hull)) = self.continent(continent, 0) {
            if category == CONTINENT_WATER && !self.target_moving() {
                self.release_target();
                let targets = self.units_in_rect(hull, continent, false, true);
                if !targets.is_empty() {
                    let i = self.random_long(0, (targets.len() as c_long).wrapping_sub(1));
                    let t = targets[i as usize];
                    let handle = self.unit(t).handle;
                    self.f().mTarget = handle;
                    self.reference(t);
                    let u = self.unit(t);
                    self.f().mCenter = EeHerdPoint::new(u.tile_x, u.tile_y);
                }
            }
        }
        if self.target_moving() {
            let units = self.units();
            let target = self.target();
            let position = self.position(target);
            let (mut units, slots) = self.formation(0, position, 0.0, true, &units);
            while let Some(&u) = units.last() {
                let info = self.unit(u);
                if info.tile_x != OFF_MAP && !info.disabled {
                    let mut heading = 0.0;
                    let mut face = false;
                    let location;
                    let target = self.target();
                    if self.float_op(EE_CP_HERD_DISTANCE, u, target) > 9.0 && !slots.is_empty() {
                        let i = self.random_long(0, (slots.len() as c_long).wrapping_sub(1)) as usize;
                        location = slots[i].position;
                        face = true;
                        heading = slots[i].heading;
                    } else {
                        let mut waypoint = self.random_long(1, 7) as i64;
                        let count = self.unit(target).waypoints;
                        if waypoint >= count {
                            waypoint = count;
                        }
                        // the clamp lands one past the last waypoint: that slot reads as the zero location
                        location = if waypoint < count {
                            let mut io = EeHerdIo::default();
                            io.v[0] = waypoint;
                            self.h.call(EE_CP_HERD_WAYPOINT, target, 0, &mut io);
                            Ge3DPoint { x: io.f[0], y: io.f[1], z: io.f[2] }
                        } else {
                            Ge3DPoint::default()
                        };
                    }
                    self.goal_move(u, location, true, true, face, heading, false, false);
                }
                units.pop();
            }
            true
        } else {
            self.release_target();
            false
        }
    }

    /// `MoveTo`: every animal walks through all the slots of a ring of `radius` round the tile (on the herd's
    /// continent). Whether any animal was ordered.
    pub fn move_to(&mut self, x: c_long, y: c_long, radius: c_ulong) -> bool {
        let mut result = false;
        let mut center = Ge3DPoint { x: x as f32 + 0.5, y: y as f32 + 0.5, z: 0.0 };
        if self.map_tile(x, y) {
            let mut io = EeHerdIo::default();
            io.v[0] = i64::from(x);
            io.v[1] = i64::from(y);
            self.h.call(EE_CP_HERD_MAP_Z, 0, 0, &mut io);
            let z = io.f[0];
            if z < 0.0 {
                center.z = z * 0.5;
            }
        }
        let (_, dims) = self.opv(EE_CP_HERD_MAP_SIZE, 0, &[]);
        let on_map = (x as c_ulong as u64) < dims.v[0] as u64 && (y as c_ulong as u64) < dims.v[1] as u64;
        let place = on_map && self.continent_at(x, y) == self.f().mContinent;
        let units = if place { self.units() } else { Vec::new() };
        let (mut units, slots) = self.formation(1, center, radius as f32, place, &units);
        if place {
            while let Some(&u) = units.last() {
                let info = self.unit(u);
                if info.tile_x != OFF_MAP && !info.disabled {
                    result = true;
                    for slot in slots.iter().rev() {
                        self.goal_move(u, slot.position, true, false, true, slot.heading, true, false);
                    }
                }
                units.pop();
            }
        }
        result
    }

    /// `Hunt`: on a water continent, a random fishing boat to circle. Whether the herd was ordered.
    pub fn hunt(&mut self) -> bool {
        let continent = self.f().mContinent;
        match self.continent(continent, 1) {
            Some((category, _)) if category == CONTINENT_WATER => {}
            _ => return false,
        }
        self.release_target();
        let units = self.prey(continent);
        if units.is_empty() {
            return false;
        }
        let i = self.random_long(0, (units.len() as c_long).wrapping_sub(1));
        let t = units[i as usize];
        let info = self.unit(t);
        self.f().mTarget = info.handle;
        self.reference(t);
        let info = self.unit(t);
        self.f().mCenter = EeHerdPoint::new(info.tile_x, info.tile_y);
        let radius = self.random_long(2, 5);
        self.move_to(info.tile_x, info.tile_y, radius as c_ulong)
    }

    /// `GetPrey`: the living fishing boats in the bounds of the herd's continent (on `continent` unless 0).
    fn prey(&mut self, continent: u16) -> Vec<u64> {
        let own = self.f().mContinent;
        match self.continent(own, 2) {
            Some((_, hull)) => self.units_in_rect(hull, continent, true, false),
            None => Vec::new(),
        }
    }

    /// `PickLeader`: the unit with the most hit points leads, with the leader's look (the old one's back).
    pub fn pick_leader(&mut self) {
        let mut best = 0u64;
        let mut best_hp = 0;
        for i in 0..self.unit_count() {
            let u = self.unit_at(i);
            let hp = self.unit(u).hit_points;
            if best != 0 {
                if hp > best_hp {
                    best = u;
                    best_hp = hp;
                }
            } else {
                best = u;
                best_hp = hp;
            }
        }
        let leader = self.f().mLeader;
        let leader = self.resolve(leader);
        if best == 0 || best == leader {
            return;
        }
        let animal = self.f().mAnimal;
        let media = self.op0(EE_CP_HERD_ANIMAL_MEDIA, animal, 0) as u64;
        if media != 0 {
            let id = self.unit(best).unit_id;
            self.set_unit_media(media, id);
        }
        if leader != 0 {
            let l = self.unit(leader);
            self.set_unit_media(l.model, l.unit_id);
        }
        self.release_leader();
        self.f().mLeader = self.unit(best).handle;
        self.reference(best);
    }

    /// `Create` (the area's override): the area made, then (unless a load) the territory widened by a random
    /// margin; then `PickLeader`. A 0.254 save's loaded area keeps its territory and leader.
    pub fn create(&mut self, tiles: &[EeHerdPoint]) {
        self.area_create(tiles);
        let (loading, _) = self.flags();
        if loading && self.lists().loaded_tiles.is_some() {
            return;
        }
        let b = self.f().mBounds2;
        let big = b.mRight.wrapping_sub(b.mLeft) > 4 || b.mBottom.wrapping_sub(b.mTop) > 4;
        if !(loading && big) {
            let bounds = self.hull_bounds();
            self.f().mBounds2 = bounds;
            let a = self.animal();
            let margin =
                self.random_long(a.mMaxTerritoryMargin.wrapping_sub(a.mMinTerritoryMargin) / 2, a.mMaxTerritoryMargin);
            let b = self.f().mBounds2;
            let y = b.mTop.wrapping_sub(margin).max(0);
            let x = b.mLeft.wrapping_sub(margin).max(0);
            self.f().mBounds2.extend(x, y);
            let b = self.f().mBounds2;
            self.f().mBounds2.extend(b.mRight.wrapping_add(margin), b.mBottom.wrapping_add(margin));
        }
        self.pick_leader();
    }

    /// `Graze`: a free animal walks to a random tile it can stand on (the graze area's, else the territory's); its
    /// young follow to tiles near it.
    pub fn graze(&mut self, unit: u64, time: c_ulong) {
        let mut tried: Vec<EeHerdPoint> = Vec::new();
        let area_id = self.f().mGrazeAreaID;
        let area = self.opv(EE_CP_HERD_AREA, 0, &[area_id as i64]).0 as u64;
        let mut found = false;
        let u = self.unit(unit);
        if u.holder != 0 {
            return;
        }
        let mut tile = EeHerdPoint::new(u.tile_x, u.tile_y);
        let continent = self.continent_at(tile.mX, tile.mY);
        if area != 0 {
            let mut i = 0;
            while i < 15 && !found {
                let (_, io) = self.opv(EE_CP_HERD_AREA_POINT, area, &[]);
                tile = EeHerdPoint::new(io.v[0] as c_long, io.v[1] as c_long);
                found = self.stand(unit, tile.mX, tile.mY);
                i += 1;
            }
        }
        let mut j = 0;
        while j < 15 && !found {
            tile = self.random_tile();
            found = self.stand(unit, tile.mX, tile.mY);
            j += 1;
        }
        let (_, dims) = self.opv(EE_CP_HERD_MAP_SIZE, 0, &[]);
        if !((tile.mX as c_ulong as u64) < dims.v[0] as u64)
            || !((tile.mY as c_ulong as u64) < dims.v[1] as u64)
            || self.continent_at(tile.mX, tile.mY) != continent
        {
            return;
        }
        let mut location = Ge3DPoint::default();
        location.x = tile.mX as f32 + self.random_float(HEIGHT_EPSILON, 0.99);
        location.y = tile.mY as f32 + self.random_float(HEIGHT_EPSILON, 0.99);
        let a = self.animal();
        let moved = self.goal_move(unit, location, true, !a.mIsPredator, false, 0.0, false, false);
        let r = self.random_long(a.mMinGrazeTime, a.mMaxGrazeTime);
        self.set_graze(unit, (r as c_ulong).wrapping_add(time));
        if !moved || self.unit(unit).held == 0 {
            return;
        }
        let start = tile;
        let top = start.mY.wrapping_sub(1).max(0);
        let left = start.mX.wrapping_sub(1).max(0);
        let limit = (dims.v[2] as c_long).wrapping_sub(1);
        let right = start.mX.wrapping_add(1).min(limit);
        let bottom = start.mY.wrapping_add(1).min(limit);
        let held = self.unit(unit).held;
        for k in (0..held).rev() {
            let young = self.opv(EE_CP_HERD_HELD, unit, &[k]).0 as u64;
            tile = start;
            if self.unit(young).growth > 0.79 {
                found = false;
                let mut other = EeHerdPoint::default();
                let mut m = 0;
                while !found && m < 5 {
                    other.mX = self.random_long(0, right.wrapping_sub(left)).wrapping_add(left);
                    other.mY = self.random_long(0, bottom.wrapping_sub(top)).wrapping_add(top);
                    if !tried.contains(&other) {
                        found = self.stand(young, other.mX, other.mY);
                    }
                    m += 1;
                }
                if found {
                    tried.push(other);
                    tile = other;
                }
            }
            location.x = tile.mX as f32 + self.random_float(HEIGHT_EPSILON, 0.99);
            location.y = tile.mY as f32 + self.random_float(HEIGHT_EPSILON, 0.99);
            self.goal_move(young, location, true, !a.mIsPredator, false, 0.0, false, false);
            let r = self.random_long(a.mMinGrazeTime, a.mMaxGrazeTime);
            self.set_graze(young, (r as c_ulong).wrapping_add(time));
        }
    }

    /// `FindMother`: the parent of a newborn (rarely one with one or two young, else an adult with none, or failing
    /// that one with young); none when the pick has more than two.
    fn find_mother(&mut self) -> u64 {
        let mut result = 0u64;
        let mut best = 0u64;
        let mut count: i64 = -1;
        if self.float_op(EE_CP_HERD_RANDOM, 0, 0) <= 0.065 {
            for i in (0..self.unit_count()).rev() {
                let p = self.unit_at(i);
                let u = self.unit(p);
                if u.tile_x == OFF_MAP || u.disabled || u.held <= 0 || u.held > 2 {
                    continue;
                }
                result = p;
                count = u.held;
                break;
            }
        }
        if result == 0 {
            for j in (0..self.unit_count()).rev() {
                let p = self.unit_at(j);
                let u = self.unit(p);
                if u.growth != 1.0 || u.tile_x == OFF_MAP || u.disabled {
                    continue;
                }
                if u.held != 0 {
                    best = p;
                    if count < 0 || count >= u.held {
                        count = u.held;
                    }
                } else {
                    result = p;
                    count = 0;
                    break;
                }
            }
            if result == 0 && best != 0 {
                result = best;
            }
        }
        if count > 2 {
            result = 0;
        }
        result
    }

    /// `GetRandomTile`: a random tile of the territory.
    fn random_tile(&mut self) -> EeHerdPoint {
        let b = self.f().mBounds2;
        let x = self.random_long(0, b.mRight.wrapping_sub(b.mLeft));
        let px = b.mLeft.wrapping_add(x);
        let b = self.f().mBounds2;
        let y = self.random_long(0, b.mBottom.wrapping_sub(b.mTop));
        EeHerdPoint::new(px, b.mTop.wrapping_add(y))
    }

    /// `AttackBack`: the hit animal (or the whole herd) attacks its last attacker when it can hurt it.
    pub fn attack_back(&mut self, unit: u64, all: bool) {
        let u = self.unit(unit);
        let attacker = u.last_attacker;
        if attacker == 0 || u.attack <= 0 {
            return;
        }
        let target = u.ammo_type;
        let a = self.unit(attacker);
        if !a.can_be_attacked || !(self.damage_factor(a.family as c_long, target) > 0.0) {
            return;
        }
        let units = if all { self.units() } else { vec![unit] };
        self.attack(attacker, &units);
    }

    /// `GrowUnit`: a young one grows a step; an adult is let go by its parent.
    fn grow_unit(&mut self, unit: u64) {
        let a = self.animal();
        let mut growth = a.mGrowthStep + self.unit(unit).growth;
        if 1.0 < growth {
            growth = 1.0;
        }
        let mut io = EeHerdIo::default();
        io.f[0] = growth;
        self.h.call(EE_CP_HERD_SET_GROWTH, unit, 0, &mut io);
        let t = self.game_time().wrapping_add(a.mGrowInterval as c_ulong);
        self.opv(EE_CP_HERD_SET_GROW_TIME, unit, &[t as i64]);
        let u = self.unit(unit);
        if !(u.growth >= 1.0) || u.holder == 0 {
            return;
        }
        self.f().mNumYoung = self.f().mNumYoung.wrapping_sub(1);
        self.op0(EE_CP_HERD_RELEASE_HOLDER, unit, 0);
    }

    /// `FollowNearestHerd`: the herd surrounds the animal nearest its leader in the nearest other herd of another,
    /// harmless animal on its continent. Whether the herd has a target.
    fn follow_nearest_herd(&mut self) -> bool {
        let leader = self.f().mLeader;
        let mut leader = self.resolve(leader);
        if leader == 0 {
            leader = if self.unit_count() > 0 { self.unit_at(0) } else { 0 };
        }
        let this = self.h.this();
        let player = self.f().mPlayer;
        let all = self.pushed(EE_CP_HERD_PLAYER_HERDS, player, &[]);
        let (own_animal, own_continent) = (self.f().mAnimalID, self.f().mContinent);
        let mut herds = Vec::new();
        for row in all {
            let herd = row[0] as u64;
            let p = self.op0(EE_CP_HERD_FIELDS_OF, herd, 0) as usize as *const EeHerdFields;
            // SAFETY: the player's herds are live; their records are read here and not kept
            let (animal_id, animal, continent) = unsafe { ((*p).mAnimalID, (*p).mAnimal, (*p).mContinent) };
            if herd != this
                && animal_id != own_animal
                && animal != 0
                && !self.animal_of(animal).mIsPredator
                && continent == own_continent
            {
                herds.push(herd);
            }
        }
        self.release_target();
        let mut best = 0.0f32;
        let mut best_herd = 0u64;
        for &herd in herds.iter().rev() {
            let d = self.float_op(EE_CP_HERD_HULL_DISTANCE, herd, this);
            if best == 0.0 || d <= best {
                best_herd = herd;
                best = d;
            }
        }
        if best_herd != 0 {
            let p = self.op0(EE_CP_HERD_FIELDS_OF, best_herd, 0) as usize as *const EeHerdFields;
            // SAFETY: a live herd's record; its list is copied out before any host call
            let others: Vec<u32> = unsafe { (*(*p).mLists).units.to_vec() };
            let mut target = 0u64;
            let mut nearest = 0.0f32;
            for &handle in others.iter().rev() {
                let u = self.resolve(handle);
                let d = self.float_op(EE_CP_HERD_DISTANCE, leader, u);
                if target == 0 || d <= nearest {
                    target = u;
                    nearest = d;
                }
            }
            if target != 0 {
                self.surround(target);
            }
        }
        self.target() != 0
    }

    /// `Update`: the young that are due grow, the state's handler runs, the next update is set.
    pub fn update(&mut self, time: c_ulong) {
        let mut delay: c_ulong = 1000;
        if self.f().mNumYoung != 0 {
            self.f().mNumYoung = 0;
            for i in (0..self.unit_count()).rev() {
                let u = self.unit_at(i);
                let info = self.unit(u);
                if !(info.growth < 1.0) {
                    continue;
                }
                self.f().mNumYoung = self.f().mNumYoung.wrapping_add(1);
                if info.grow_time <= time {
                    self.grow_unit(u);
                }
            }
        }
        match self.f().mState {
            0 => delay = self.update_resting(time),
            2 => delay = self.update_surrounding(time),
            3 => delay = self.update_attacking(time),
            4 => delay = 1000,
            7 => delay = self.update_chasing(time),
            9 => delay = self.update_hunting(time),
            0xa => delay = self.update_leaving(),
            _ => {}
        }
        self.f().mNextUpdateTime = delay.wrapping_add(time);
    }

    /// `RemoveUnit`: an animal leaves (leader, herd link, look); the area is made again once the herd is empty.
    pub fn remove_unit(&mut self, unit: u64) -> bool {
        let mut tiles = Vec::new();
        let mut at = None;
        for i in 0..self.unit_count() {
            if self.unit_at(i) == unit {
                at = Some(i);
                break;
            }
        }
        let Some(at) = at else {
            return false;
        };
        self.lists().units.remove(at);
        self.dereference(unit);
        for i in (0..self.unit_count()).rev() {
            let u = self.unit_at(i);
            let t = self.unit(u);
            tiles.push(EeHerdPoint::new(t.tile_x, t.tile_y));
        }
        let leader = self.f().mLeader;
        if unit == self.resolve(leader) {
            self.release_leader();
        }
        let this = self.h.this();
        let u = self.unit(unit);
        if u.herd == this && u.herd != 0 {
            self.set_herd(unit, 0);
        }
        self.remove_unit_media(u.unit_id);
        let (_, clearing) = self.flags();
        if !clearing && tiles.is_empty() {
            self.create(&tiles);
        }
        true
    }

    /// `HasUnit`.
    pub fn has_unit(&mut self, unit: u64) -> bool {
        (0..self.unit_count()).any(|i| self.unit_at(i) == unit)
    }

    /// `GiveBirth`: a newborn held by a parent (when one is found); the next birth set.
    fn give_birth(&mut self, time: c_ulong) -> bool {
        let parent = self.find_mother();
        let mut result = false;
        if parent != 0 {
            let animal = self.f().mAnimal;
            self.op0(EE_CP_HERD_BIRTH, parent, animal);
            result = true;
        }
        let a = self.animal();
        let r = self.random_long(a.mMinBirthInterval, a.mMaxBirthInterval);
        self.f().mBirthTimer = (r as c_ulong).wrapping_add(time);
        result
    }

    /// `ResetGrazeTimes`: the animals graze at once (all, or the first `index` after an optional shuffle).
    fn reset_graze_times(&mut self, index: c_long, shuffle: bool) {
        let mut units = self.units();
        if shuffle && !units.is_empty() {
            // Dinkum's random_shuffle (EEAIRandomShuffle): a draw of GetRandom(0, count - 1) per position from the second
            for (count, at) in (2 as c_long..).zip(1..units.len()) {
                let j = self.random_long(0, (count - 1).max(0));
                units.swap(at, j as usize);
            }
        }
        if index >= 0 && (index as usize) < units.len() {
            units.truncate(index as usize);
        }
        for &u in units.iter().rev() {
            self.set_graze(u, 0);
        }
    }

    /// `SetUnitMedia`: the look kept for a unit id, loaded (the one it replaces unloaded).
    fn set_unit_media(&mut self, media: u64, key: c_ulong) {
        match self.lists().media.get(&key).copied() {
            Some(old) => {
                if media != old {
                    self.media(EE_CP_HERD_MEDIA_UNLOAD, old);
                    self.lists().media.insert(key, media);
                    self.media(EE_CP_HERD_MEDIA_LOAD, media);
                }
            }
            None => {
                self.media(EE_CP_HERD_MEDIA_LOAD, media);
                self.lists().media.insert(key, media);
            }
        }
    }

    /// `RemoveUnitMedia`.
    fn remove_unit_media(&mut self, key: c_ulong) {
        if let Some(old) = self.lists().media.get(&key).copied() {
            self.media(EE_CP_HERD_MEDIA_UNLOAD, old);
            self.lists().media.remove(&key);
        }
    }

    /// `CanTake`: the same player and animal, room left, and no tiles yet or (`check_range`) on the herd's continent
    /// within the herd radius of its hull.
    pub fn can_take(&mut self, unit: u64, check_range: bool) -> bool {
        let u = self.unit(unit);
        let mut result = false;
        if u.player == self.f().mPlayer
            && self.op0(EE_CP_HERD_ANIMAL_ON_MAP, unit, 0) != 0
            && self.f().mAnimalID == u.animal_id as c_ulong
            && (self.unit_count() as u64) < (self.f().mMaxUnits as i64 as u64)
        {
            result = self.num_tiles() == 0;
            if !result && check_range {
                let id = self.continent_at(u.tile_x, u.tile_y);
                let continent = self.f().mContinent;
                if id == continent || continent == 0 {
                    result = self.hull_contains(u.tile_x, u.tile_y);
                    if !result {
                        let (_, io) = self.opv(EE_CP_HERD_CLOSEST_HULL, 0, &[i64::from(u.tile_x), i64::from(u.tile_y)]);
                        let (cx, cy) = (io.v[0] as c_long, io.v[1] as c_long);
                        let dx = cx.wrapping_sub(u.tile_x);
                        let dy = cy.wrapping_sub(u.tile_y);
                        result = dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy)) <= self.f().mRadiusSquared;
                    }
                }
            }
        }
        result
    }

    /// `Surround`: the herd on a ring round the target (the nearest animal to each slot, from the last); it becomes
    /// the target.
    fn surround(&mut self, target: u64) {
        let position = self.position(target);
        let units = self.units();
        let (mut units, mut slots) = self.formation(2, position, 0.0, true, &units);
        while !units.is_empty() && !slots.is_empty() {
            let mut best = 0u64;
            let mut best_index = 0usize;
            let mut best_distance = -1.0f32;
            let slot = *slots.last().unwrap();
            for i in (0..units.len()).rev() {
                let p = self.position(units[i]);
                let d = slot.position.distance_to_squared(p);
                if best_distance != -1.0 && !(d < best_distance) {
                    continue;
                }
                best = units[i];
                best_index = i;
                best_distance = d;
            }
            units.remove(best_index);
            self.goal_move(best, slot.position, true, false, true, slot.heading, false, false);
            slots.pop();
        }
        self.reference(target);
        self.release_target();
        let u = self.unit(target);
        self.f().mTarget = u.handle;
        self.f().mCenter = EeHerdPoint::new(u.tile_x, u.tile_y);
    }

    /// `UpdateSurrounding` (state 2): the herd follows the moving target and attacks once the follow timer runs out;
    /// it rests again when the target is gone.
    fn update_surrounding(&mut self, time: c_ulong) -> c_ulong {
        let t = self.target();
        if t != 0 && self.unit(t).hit_points > 0 {
            let u = self.unit(t);
            if EeHerdPoint::new(u.tile_x, u.tile_y) != self.f().mCenter {
                self.surround(t);
            }
            if self.f().mFollowTime <= time {
                let t = self.target();
                self.attack_all(t);
                self.f().mState = 3;
            }
        } else {
            self.release_target();
            let n = self.unit_count() as c_long;
            self.reset_graze_times(n, true);
            self.f().mState = 0;
        }
        2000
    }

    /// `UpdateChasing` (state 7).
    fn update_chasing(&mut self, time: c_ulong) -> c_ulong {
        let mut following = true;
        let t = self.target();
        if t != 0 && self.unit(t).hit_points > 0 && self.f().mChaseTime > time {
            let u = self.unit(t);
            let tile = EeHerdPoint::new(u.tile_x, u.tile_y);
            if distance_squared_f(tile, self.f().mCenter) > 9.0 {
                following = self.chase();
                self.f().mCenter = tile;
            }
        } else {
            following = false;
        }
        if following {
            return 1000;
        }
        self.release_target();
        let n = self.unit_count() as c_long;
        self.reset_graze_times(n, true);
        self.f().mState = 0;
        let a = self.animal();
        let r = self.random_long(a.mMinHuntInterval, a.mMaxHuntInterval);
        self.f().mChaseTime = (r as c_ulong).wrapping_add(time);
        1000
    }

    /// `UpdateHunting` (state 9).
    fn update_hunting(&mut self, time: c_ulong) -> c_ulong {
        let t = self.target();
        if t != 0 && self.unit(t).hit_points > 0 && self.f().mHuntTime > time {
            let u = self.unit(t);
            let tile = EeHerdPoint::new(u.tile_x, u.tile_y);
            if tile == self.f().mCenter {
                return 1000;
            }
            let radius = self.random_long(2, 5);
            let following = self.move_to(tile.mX, tile.mY, radius as c_ulong);
            self.f().mCenter = tile;
            if following {
                return 1000;
            }
        }
        self.release_target();
        let n = self.unit_count() as c_long;
        self.reset_graze_times(n, true);
        self.f().mState = 0;
        let a = self.animal();
        let r = self.random_long(a.mMinHuntInterval, a.mMaxHuntInterval);
        self.f().mHuntTime = (r as c_ulong).wrapping_add(time);
        1000
    }

    /// `UpdateAttacking` (state 3).
    fn update_attacking(&mut self, time: c_ulong) -> c_ulong {
        let t = self.target();
        if t != 0 && self.unit(t).hit_points > 0 {
            for i in (0..self.unit_count()).rev() {
                let u = self.unit_at(i);
                if self.unit(u).has_goal {
                    continue;
                }
                let target = self.target();
                self.goal_attack(u, target);
            }
        } else {
            let n = (self.unit_count() as c_long).wrapping_sub(1);
            let index = self.random_long(0, n);
            self.reset_graze_times(index, true);
            let a = self.animal();
            let r = self.random_long(a.mMinRestInterval, a.mMaxRestInterval);
            self.f().mFollowTime = (r as c_ulong).wrapping_add(time);
            self.f().mState = 0;
        }
        1000
    }

    /// `UpdateResting` (state 0): idle animals graze, a birth when due, a chase (animal 0xb), a hunt (0xa) or a
    /// predator's following the nearest herd.
    fn update_resting(&mut self, time: c_ulong) -> c_ulong {
        if !self.f().mIsActive {
            return 1000;
        }
        for i in (0..self.unit_count()).rev() {
            let u = self.unit_at(i);
            let info = self.unit(u);
            if !info.disabled
                && info.tile_x != OFF_MAP
                && (info.command == ACTION_IDLE || info.command == ACTION_CIRCLE)
                && self.f().mState == 0
                && info.initialized
                && info.graze_time < time
                && info.holder == 0
            {
                self.graze(u, time);
            }
        }
        if self.f().mBreeds
            && (self.unit_count() as u64) < (self.f().mMaxUnits as i64 as u64)
            && self.f().mBirthTimer < time
        {
            self.give_birth(time);
        }
        let a = self.animal();
        if self.f().mState == 0 && a.mAnimalID == 0xb && self.f().mChaseTime <= time {
            if self.float_op(EE_CP_HERD_GENERATE, 0, 0) <= 0.45 && self.chase() && self.target() != 0 {
                let t = self.target();
                let u = self.unit(t);
                self.f().mCenter = EeHerdPoint::new(u.tile_x, u.tile_y);
                self.f().mState = 7;
            }
            let a = self.animal();
            let r = self.random_long(a.mMinHuntInterval, a.mMaxHuntInterval);
            self.f().mChaseTime = (r as c_ulong).wrapping_add(time);
        }
        let a = self.animal();
        if self.f().mState == 0 && a.mAnimalID == 0xa && self.f().mHuntTime <= time {
            if self.float_op(EE_CP_HERD_GENERATE, 0, 0) <= 0.45 && self.hunt() && self.target() != 0 {
                let t = self.target();
                let u = self.unit(t);
                self.f().mCenter = EeHerdPoint::new(u.tile_x, u.tile_y);
                self.f().mState = 9;
            }
            let a = self.animal();
            let r = self.random_long(a.mMinHuntInterval, a.mMaxHuntInterval);
            self.f().mHuntTime = (r as c_ulong).wrapping_add(time);
        }
        let a = self.animal();
        if !a.mIsPredator || !(self.f().mFollowTime <= time) {
            return 1000;
        }
        if self.follow_nearest_herd() {
            self.f().mState = 2;
        }
        let a = self.animal();
        let r = self.random_long(a.mMinFollowInterval, a.mMaxFollowInterval);
        self.f().mFollowTime = (r as c_ulong).wrapping_add(time);
        1000
    }

    /// `UpdateLeaving` (state 0xa): each animal still on the map heads for an off-map tile by a random map-edge
    /// area; state 0xb once all are off.
    fn update_leaving(&mut self) -> c_ulong {
        let mut off_map: usize = 0;
        for i in (0..self.unit_count()).rev() {
            let u = self.unit_at(i);
            let mut leaving = false;
            let mut io = EeHerdIo::default();
            self.h.call(EE_CP_HERD_PHYSICAL, u, 0, &mut io);
            if self.map_tile(um::floor(io.f[0]) as c_long, um::floor(io.f[1]) as c_long) {
                let goals = self.op0(EE_CP_HERD_GOALS, u, 0);
                let mut j = goals - 1;
                while j >= 0 && !leaving {
                    let mut io = EeHerdIo::default();
                    io.v[0] = j;
                    if self.h.call(EE_CP_HERD_GOAL_LOCATION, u, 0, &mut io) != 0 {
                        leaving = !self.map_tile(um::floor(io.f[0]) as c_long, um::floor(io.f[1]) as c_long);
                    }
                    j -= 1;
                }
                if !leaving {
                    let mut exit = EeHerdPoint::default();
                    let area = self.op0(EE_CP_HERD_MAP_EDGE, 0, 0) as u64;
                    if area != 0 {
                        let (_, io) = self.opv(EE_CP_HERD_AREA_POINT, area, &[]);
                        let (px, py) = (io.v[0] as c_long, io.v[1] as c_long);
                        // EEAIAStarBounds of the point, grown by 10 (unclamped)
                        let b = EeHerdRect {
                            mLeft: px.wrapping_sub(10),
                            mTop: py.wrapping_sub(10),
                            mRight: px.wrapping_add(10),
                            mBottom: py.wrapping_add(10),
                        };
                        let corners =
                            [(b.mLeft, b.mTop), (b.mRight, b.mTop), (b.mRight, b.mBottom), (b.mLeft, b.mBottom)];
                        exit = EeHerdPoint::new(OFF_MAP, OFF_MAP);
                        for (x, y) in corners {
                            if !self.map_tile(x, y) {
                                exit = EeHerdPoint::new(x, y);
                                break;
                            }
                        }
                    } else {
                        exit.mX = OFF_MAP;
                        exit.mY = OFF_MAP;
                    }
                    let at = Ge3DPoint { x: exit.mX as f32 + 0.5, y: exit.mY as f32 + 0.5, z: 0.0 };
                    self.goal_move(u, at, true, true, false, 0.0, false, true);
                }
            } else {
                off_map += 1;
            }
        }
        if off_map == self.unit_count() {
            self.f().mState = 0xb;
        }
        1000
    }

    /// `GetRandomPoint` (the vtable's slot; EETown's body by ICF): a random map tile in the hull (16 tries in its
    /// bounds), else the area's random point.
    pub fn random_point(&mut self) -> EeHerdPoint {
        random_area_point(|op, v| self.opv(op, 0, v))
    }

    /// `Load`: the herd from a saved game (ids for its animal, player, leader, target and units until
    /// RepairPointers); the first read error.
    pub fn load(&mut self, major: u32, minor: u32, file: &mut SaveFile) -> i32 {
        let (mut r, v) = file.read_ulong(0);
        self.f().mAnimal = u64::from(v);
        if r == 0 {
            let (r2, v) = file.read_ulong(0);
            r = r2;
            self.f().mPlayer = u64::from(v);
        }
        if r == 0 {
            let (r2, v) = file.read_ulong(EE_CP_HERD_NONE);
            r = r2;
            self.f().mLeader = v;
        }
        if r == 0 {
            let (r2, v) = file.read_ulong(EE_CP_HERD_NONE);
            r = r2;
            self.f().mTarget = v;
            if r == 0 {
                let b = self.f().mBounds2;
                let (r2, v) = file.read_long(b.mLeft as i32);
                r = r2;
                self.f().mBounds2.mLeft = v as c_long;
                if r == 0 {
                    let (r2, v) = file.read_long(b.mTop as i32);
                    r = r2;
                    self.f().mBounds2.mTop = v as c_long;
                    if r == 0 {
                        let (r2, v) = file.read_long(b.mRight as i32);
                        r = r2;
                        self.f().mBounds2.mRight = v as c_long;
                        if r == 0 {
                            let (r2, v) = file.read_long(b.mBottom as i32);
                            r = r2;
                            self.f().mBounds2.mBottom = v as c_long;
                        }
                    }
                }
            }
            macro_rules! rd {
                ($f:ident, ulong) => {
                    if r == 0 {
                        let (r2, v) = file.read_ulong(self.f().$f as u32);
                        r = r2;
                        self.f().$f = v as c_ulong;
                    }
                };
                ($f:ident, long) => {
                    if r == 0 {
                        let (r2, v) = file.read_long(self.f().$f as i32);
                        r = r2;
                        self.f().$f = v as c_long;
                    }
                };
            }
            if r == 0 {
                let (r2, v) = file.read_ushort(self.f().mID);
                r = r2;
                self.f().mID = v;
            }
            if r == 0 {
                let (r2, v) = file.read_float(self.f().mFB8);
                r = r2;
                self.f().mFB8 = v;
            }
            if r == 0 {
                let (r2, v) = file.read_bool(self.f().mBreeds);
                r = r2;
                self.f().mBreeds = v;
            }
            rd!(mAnimalID, ulong);
            rd!(mGrazeAreaID, ulong);
            rd!(mState, ulong);
            rd!(mBirthTimer, ulong);
            rd!(mLE4, ulong);
            rd!(mNextUpdateTime, ulong);
            rd!(mLD8, ulong);
            rd!(mRadiusSquared, long);
            rd!(mMaxUnits, long);
            if r == 0 {
                let (r2, v) = file.read_ushort(self.f().mContinent);
                r = r2;
                self.f().mContinent = v;
            }
            rd!(mFollowTime, ulong);
            if r == 0 {
                r = read_point(file, &mut self.f().mCenter);
            }
            if r == 0 {
                r = read_point(file, &mut self.f().mP94);
            }
        }
        let mut count = 0u32;
        // an earlier read error is kept
        if r == 0 {
            let (r2, v) = file.read_ulong(0);
            r = r2;
            count = v;
        }
        if r != 0 {
            return r;
        }
        self.lists().units.resize(count as usize, EE_CP_HERD_NONE);
        for i in 0..count as usize {
            if r == 0 {
                let (r2, v) = file.read_ulong(0);
                r = r2;
                self.lists().units[i] = v;
            }
        }
        macro_rules! rdu {
            ($f:ident) => {
                if r == 0 {
                    let (r2, v) = file.read_ulong(self.f().$f as u32);
                    r = r2;
                    self.f().$f = v as c_ulong;
                }
            };
        }
        rdu!(mHuntTime);
        rdu!(mChaseTime);
        rdu!(mLC8);
        // port (0.254): the area's tiles (RepairPointers)
        if r == 0 && self.save_flags(major, minor) & 1 != 0 {
            let (r2, n) = file.read_ulong(0);
            r = r2;
            let mut tiles = Vec::new();
            let mut i = 0;
            while i < n && r == 0 {
                let mut tile = EeHerdPoint::default();
                let (r2, x) = file.read_long(tile.mX as i32);
                r = r2;
                tile.mX = x as c_long;
                if r == 0 {
                    let (r2, y) = file.read_long(tile.mY as i32);
                    r = r2;
                    tile.mY = y as c_long;
                }
                tiles.push(tile);
                i += 1;
            }
            if r == 0 {
                self.lists().loaded_tiles = Some(tiles);
            }
        }
        r
    }

    fn save_flags(&mut self, major: u32, minor: u32) -> i64 {
        self.opv(EE_CP_HERD_SAVE_FLAGS, 0, &[i64::from(major), i64::from(minor)]).0
    }

    /// `Save`: the herd to the saved game (ids for its animal, player, leader, target and units); the first write
    /// error.
    pub fn save(&mut self, major: u32, minor: u32, file: &mut SaveFile) -> i32 {
        let a = self.animal();
        let mut r = file.write_ulong(a.mAnimalID as u32);
        let player = self.f().mPlayer;
        let player_id = self.op0(EE_CP_HERD_PLAYER_ID, player, 0) as u32;
        if r == 0 {
            r = file.write_ulong(player_id);
        }
        let leader = self.f().mLeader;
        let leader = self.resolve(leader);
        let v = if leader == 0 { EE_CP_HERD_NONE } else { self.unit(leader).unit_id as u32 };
        if r == 0 {
            r = file.write_ulong(v);
        }
        let target = self.target();
        let v = if target == 0 { EE_CP_HERD_NONE } else { self.unit(target).unit_id as u32 };
        if r == 0 {
            r = file.write_ulong(v);
            let f = self.f();
            if r == 0 {
                let b = f.mBounds2;
                r = file.write_long(b.mLeft as i32);
                if r == 0 {
                    r = file.write_long(b.mTop as i32);
                    if r == 0 {
                        r = file.write_long(b.mRight as i32);
                        if r == 0 {
                            r = file.write_long(b.mBottom as i32);
                        }
                    }
                }
            }
            let f = self.f();
            let steps: [&dyn Fn(&mut SaveFile) -> i32; 16] = [
                &|file| file.write_ushort(f.mID),
                &|file| file.write_float(f.mFB8),
                &|file| file.write_bool(f.mBreeds),
                &|file| file.write_ulong(f.mAnimalID as u32),
                &|file| file.write_ulong(f.mGrazeAreaID as u32),
                &|file| file.write_ulong(f.mState as u32),
                &|file| file.write_ulong(f.mBirthTimer as u32),
                &|file| file.write_ulong(f.mLE4 as u32),
                &|file| file.write_ulong(f.mNextUpdateTime as u32),
                &|file| file.write_ulong(f.mLD8 as u32),
                &|file| file.write_long(f.mRadiusSquared as i32),
                &|file| file.write_long(f.mMaxUnits as i32),
                &|file| file.write_ushort(f.mContinent),
                &|file| file.write_ulong(f.mFollowTime as u32),
                &|file| write_point(file, f.mCenter),
                &|file| write_point(file, f.mP94),
            ];
            for step in steps {
                if r == 0 {
                    r = step(file);
                }
            }
        }
        let count = self.unit_count();
        // an earlier write error is kept
        if r == 0 {
            r = file.write_ulong(count as u32);
        }
        if r != 0 {
            return r;
        }
        for i in 0..count {
            let u = self.unit_at(i);
            let id = self.unit(u).unit_id as u32;
            if r == 0 {
                r = file.write_ulong(id);
            }
        }
        let f = self.f();
        for v in [f.mHuntTime, f.mChaseTime, f.mLC8] {
            if r == 0 {
                r = file.write_ulong(v as u32);
            }
        }
        // port (0.254): the area's tiles, so a loaded herd keeps its area, territory and leader
        if r == 0 && self.save_flags(major, minor) & 1 != 0 {
            let tiles = self.pushed(EE_CP_HERD_TILES, 0, &[]);
            r = file.write_ulong(tiles.len() as u32);
            for t in &tiles {
                if r != 0 {
                    break;
                }
                r = file.write_long(t[0] as i32);
                if r == 0 {
                    r = file.write_long(t[1] as i32);
                }
            }
        }
        r
    }

    /// `RepairPointers`: the saved ids back to the player, animal, units, leader and target; the looks, the area
    /// (as saved from 0.254), the young counted.
    pub fn repair_pointers(&mut self, major: u32, minor: u32) -> c_long {
        let player = self.f().mPlayer;
        let player = self.opv(EE_CP_HERD_PLAYER, 0, &[player as u32 as i64]).0 as u64;
        self.f().mPlayer = player;
        let animal = self.f().mAnimal;
        let animal = self.opv(EE_CP_HERD_ANIMAL, 0, &[animal as u32 as i64]).0 as u64;
        self.f().mAnimal = animal;
        if animal != 0 {
            let a = self.animal();
            self.f().mRadiusSquared = a.mHerdRadius.wrapping_mul(a.mHerdRadius);
        }
        let this = self.h.this();
        for i in (0..self.unit_count()).rev() {
            let saved = self.unit_handle(i);
            if saved != EE_CP_HERD_NONE {
                let p = self.opv(EE_CP_HERD_UNIT_BY_ID, 0, &[i64::from(saved)]).0 as u64;
                let handle = if p != 0 { self.unit(p).handle } else { EE_CP_HERD_NONE };
                self.lists().units[i] = handle;
                let p = self.resolve(handle);
                if p != 0 {
                    self.reference(p);
                    self.set_herd(p, this);
                }
            } else {
                self.lists().units[i] = EE_CP_HERD_NONE;
            }
            if self.unit_at(i) == 0 {
                self.lists().units.remove(i);
            }
        }
        if self.unit_count() > 0 {
            let first = self.unit_at(0);
            let class_animal = self.unit(first).animal_id;
            let animal = self.f().mAnimal;
            if animal == 0 || self.animal_of(animal).mAnimalID != class_animal {
                let animal = self.opv(EE_CP_HERD_ANIMAL, 0, &[i64::from(class_animal)]).0 as u64;
                self.f().mAnimal = animal;
            }
        }
        let leader = self.f().mLeader;
        if leader != EE_CP_HERD_NONE {
            let p = self.opv(EE_CP_HERD_UNIT_BY_ID, 0, &[i64::from(leader)]).0 as u64;
            self.f().mLeader = if p != 0 { self.unit(p).handle } else { EE_CP_HERD_NONE };
            let l = self.f().mLeader;
            let p = self.resolve(l);
            if p != 0 {
                self.reference(p);
            }
        } else {
            self.f().mLeader = EE_CP_HERD_NONE;
        }
        let animal = self.f().mAnimal;
        if animal != 0 {
            for j in (0..self.unit_count()).rev() {
                let p = self.unit_at(j);
                let u = self.unit(p);
                self.set_unit_media(u.model, u.unit_id);
                let media = self.op0(EE_CP_HERD_ANIMAL_MEDIA, animal, 0) as u64;
                if u.b34f && media != 0 {
                    self.set_unit_media(media, u.unit_id);
                }
            }
            let leader = self.f().mLeader;
            if self.resolve(leader) == 0 && self.animal_of(animal).mHasLeader {
                self.pick_leader();
            }
        }
        let target = self.f().mTarget;
        if target != EE_CP_HERD_NONE {
            let p = self.opv(EE_CP_HERD_UNIT_BY_ID, 0, &[i64::from(target)]).0 as u64;
            self.f().mTarget = if p != 0 { self.unit(p).handle } else { EE_CP_HERD_NONE };
            let p = self.target();
            if p != 0 {
                self.reference(p);
            }
        } else {
            self.f().mTarget = EE_CP_HERD_NONE;
        }
        // port (0.263): a save with its continent regions keeps the saved continent
        if self.unit_count() > 0 && self.save_flags(major, minor) & 2 == 0 {
            let first = self.unit_at(0);
            self.f().mContinent = self.op0(EE_CP_HERD_CONTINENT_OF, first, 0) as u16;
        }
        let mut tiles = Vec::new();
        for k in (0..self.unit_count()).rev() {
            let p = self.unit_at(k);
            let u = self.unit(p);
            tiles.push(EeHerdPoint::new(u.tile_x, u.tile_y));
        }
        if let Some(loaded) = self.lists().loaded_tiles.clone() {
            tiles = loaded;
        }
        self.create(&tiles);
        self.lists().loaded_tiles = None;
        let mut young = 0;
        for i in (0..self.unit_count()).rev() {
            let p = self.unit_at(i);
            if self.unit(p).growth < 1.0 {
                young += 1;
            }
        }
        self.f().mNumYoung = young;
        0
    }
}

/// `U2DPoint<long>::Load`.
fn read_point(file: &mut SaveFile, p: &mut EeHerdPoint) -> i32 {
    let (mut r, x) = file.read_long(p.mX as i32);
    p.mX = x as c_long;
    if r == 0 {
        let (r2, y) = file.read_long(p.mY as i32);
        r = r2;
        p.mY = y as c_long;
    }
    r
}

/// `U2DPoint<long>::Save`.
fn write_point(file: &mut SaveFile, p: EeHerdPoint) -> i32 {
    let mut r = file.write_long(p.mX as i32);
    if r == 0 {
        r = file.write_long(p.mY as i32);
    }
    r
}

fn rect_of(v: &[i64]) -> EeHerdRect {
    EeHerdRect { mBottom: v[0] as c_long, mLeft: v[1] as c_long, mRight: v[2] as c_long, mTop: v[3] as c_long }
}

/// `U2DPoint<long>::DistanceToSquaredF`: the x square as a float plus the y square.
fn distance_squared_f(p: EeHerdPoint, q: EeHerdPoint) -> f32 {
    let dx = p.mX.wrapping_sub(q.mX);
    let dy = p.mY.wrapping_sub(q.mY);
    dx.wrapping_mul(dx) as f32 + dy.wrapping_mul(dy) as f32
}

/// `EETown::GetRandomPoint` 0xac203 (the herd's slot by ICF): 16 tries of a random tile in the hull's bounds that is
/// on the map and in the hull, else `EEAIArea::GetRandomPoint`. `op` asks the area's host: HAS_MAP, HULL_BOUNDS,
/// RANDOM_LONG, MAP_TILE_AT, HULL_CONTAINS, BASE_POINT (the herd's codes).
pub fn random_area_point(mut op: impl FnMut(i32, &[i64]) -> (i64, EeHerdIo)) -> EeHerdPoint {
    let mut result = EeHerdPoint::default();
    let mut found = false;
    if op(EE_CP_HERD_HAS_MAP, &[]).0 != 0 {
        let mut i = 15;
        while i >= 0 && !found {
            let b = rect_of(&op(EE_CP_HERD_HULL_BOUNDS, &[]).1.v[0..4]);
            let x = op(EE_CP_HERD_RANDOM_LONG, &[0, i64::from(b.mRight.wrapping_sub(b.mLeft))]).0 as c_long;
            result.mX = b.mLeft.wrapping_add(x);
            let b = rect_of(&op(EE_CP_HERD_HULL_BOUNDS, &[]).1.v[0..4]);
            let y = op(EE_CP_HERD_RANDOM_LONG, &[0, i64::from(b.mBottom.wrapping_sub(b.mTop))]).0 as c_long;
            result.mY = b.mTop.wrapping_add(y);
            if op(EE_CP_HERD_MAP_TILE_AT, &[i64::from(result.mX), i64::from(result.mY)]).0 != 0 {
                found = op(EE_CP_HERD_HULL_CONTAINS, &[i64::from(result.mX), i64::from(result.mY)]).0 != 0;
            }
            i -= 1;
        }
    }
    if !found {
        let io = op(EE_CP_HERD_BASE_POINT, &[]).1;
        result = EeHerdPoint::new(io.v[0] as c_long, io.v[1] as c_long);
    }
    result
}

/// A new record's lists.
pub fn new_lists() -> *mut EeHerdLists {
    Box::into_raw(Box::default())
}

/// # Safety
/// `lists` came from [`new_lists`] and is not used again.
pub unsafe fn free_lists(lists: *mut EeHerdLists) {
    if !lists.is_null() {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(lists) });
    }
}
