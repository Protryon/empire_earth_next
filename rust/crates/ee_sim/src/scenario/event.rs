//! The data-driven events and their effects (`src/Empire Earth/Scenario Manager/EEEvent.cpp`; the scenario editor
//! lane's round 4): `EEEvent::LoadEvents`, `Trigger`, `RunEffect` with its EVRN record, the handlers (the button,
//! the tech tree's media, the unit model and sounds, the conversions, the attributes with their EVAT records and
//! the rescale walks, the sound, research, obsolescence and prerequisites, the gather rates) and the pathfinding
//! functor's step, over the world's single C++ statements by operation (`EE_SCENARIO_EV_*`, `ee_scenariocpp_event`
//! in EEEvent.cpp), in the C++'s order. The arithmetic and the decisions are ee_sim::tech::effects'. The C++ class
//! keeps its members and the effect table (`EEEvent::sEvents`, `gEventEffects`); its members forward here.
//!
//! The capture records (EVRN, EVAT, EVUN's unit steps and walks, EVHP) are built here with the C++'s bytes: the
//! player's and the upgrade's blocks are the records `EETechCapturePlayer` and `EETechCaptureUpgrade` wrote.

// the C++ member names; c_long/c_ulong are 32 bits on Windows
#![allow(non_snake_case, clippy::unnecessary_cast)]

use std::os::raw::{c_long, c_ulong};

use crate::app::capture_record::Record;
use crate::tech::effects::{
    self, EeTechAttributeOp, EeTechEffectHandler, EeTechEffectValues, EeTechUnitView, EeTechUnitWalk,
    EE_TECH_UNIT_BUILT, EE_TECH_UNIT_CARRIED, EE_TECH_UNIT_COMPLEX, EE_TECH_UNIT_ON_TILE,
};
use crate::world::player_fields::EeUnitsPlayerFields;
use crate::world::stats_fields::EeUnitsUnitClassUpgradeFields;
use crate::world::unit_class_fields::EeUnitsUnitClassFields;

/// One effect of an event (`EEEventEffect`, ee/EEEvent.h's typedef of this record: a dbeffects row of 88 bytes on
/// every target; provisional names from the tech tree's readers).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScenarioEvEffect {
    pub mUnused1: i32,
    pub mUnusedID: i32,
    /// +0x08 the absolute value (0x1ca591/0x1ca5df: used when +0x0c/+0x10 are 0)
    pub mValue: f32,
    /// +0x0c the offset added to the base
    pub mOffset: f32,
    /// +0x10 the factor of the value
    pub mFactor: f32,
    /// +0x14 a gUnitClassTable index (< EEUnitClassCount())
    pub mUnitClass: i32,
    /// +0x18 a unit class (kind 0x13: its media)
    pub mNewUnitClass: i32,
    pub mUnitSet: i32,
    /// +0x20
    pub mKind: i32,
    /// +0x24 an EEMedia index (kind 6)
    pub mMediaIndex: i32,
    /// +0x28 a tech tree id (kind 9)
    pub mTechnology: i32,
    /// +0x2c (kind 2: 0x28 = an area effect)
    pub mAttribute: i32,
    /// +0x30 an EESoundObject index (kinds 0xf..0x12, 0x14, 0x15)
    pub mSoundID: i32,
    /// +0x34 an EEButtonObject index (kind 1)
    pub mButton: i32,
    /// +0x38 an EEAEManager index (kind 2)
    pub mAreaEffect: i32,
    pub mTerrainType: i32,
    pub mUnused2: i32,
    pub mRunOnLoad: u8,
    pub mUnused3: u8,
    pub mUnused4: u16,
    pub mUnused5: i32,
    pub mUnused6: i32,
    pub mUnused7: i32,
    /// (sizeof 0x58)
    pub mUnused8: i32,
}

// ---- the statements (`ee_scenariocpp_event(op, a)`: a[0..4] the arguments, pointers as integers; a float is its
// bits in the low 32 bits)
/// `gEventEffects.data()` (`EeScenarioEvEffect*`), `.size()`; `gLoadingSavedGame`; `gEventsEnabled`.
pub const EE_SCENARIO_EV_EFFECTS_DATA: u32 = 0;
pub const EE_SCENARIO_EV_EFFECTS_COUNT: u32 = 1;
pub const EE_SCENARIO_EV_LOADING: u32 = 2;
pub const EE_SCENARIO_EV_EVENTS_ENABLED: u32 = 3;
/// `gWorld.mPlayers[a0]`, `gWorld.GetCurrentPlayer()`, player a0's `mPlayerID`, its `EeUnitsPlayerFields*`.
pub const EE_SCENARIO_EV_PLAYER: u32 = 4;
pub const EE_SCENARIO_EV_CURRENT_PLAYER: u32 = 5;
pub const EE_SCENARIO_EV_PLAYER_ID: u32 = 6;
pub const EE_SCENARIO_EV_PLAYER_FIELDS: u32 = 7;
/// `gUnitClassTable[a0]`, class a0's `EeUnitsUnitClassFields*`, `EEUnitClassCount()`; player a0's
/// `mUnitClassUpgrades[a1]`, its `GetUpgrade(a1)`; upgrade a0's `EeUnitsUnitClassUpgradeFields*`, its `mUnitClass`.
pub const EE_SCENARIO_EV_CLASS: u32 = 8;
pub const EE_SCENARIO_EV_CLASS_FIELDS: u32 = 9;
pub const EE_SCENARIO_EV_CLASS_COUNT: u32 = 10;
pub const EE_SCENARIO_EV_UPGRADE: u32 = 11;
pub const EE_SCENARIO_EV_GET_UPGRADE: u32 = 12;
pub const EE_SCENARIO_EV_UPGRADE_FIELDS: u32 = 13;
pub const EE_SCENARIO_EV_UPGRADE_CLASS: u32 = 14;
/// Player a0's `mFirstUnit`, unit a0's `mNext`; the player's `GetNumPendingUnits()`, `GetPendingUnit(a1)`.
pub const EE_SCENARIO_EV_FIRST_UNIT: u32 = 20;
pub const EE_SCENARIO_EV_NEXT_UNIT: u32 = 21;
pub const EE_SCENARIO_EV_PENDING_COUNT: u32 = 22;
pub const EE_SCENARIO_EV_PENDING_UNIT: u32 = 23;
/// Unit a0's `mUnitClass->mUnitType`, `GetHitPoints()`, `mComplexUnit`; complex unit a0's `mFlightTime`,
/// `mIsBuilt == true`; the unit's `mTileNode != NULL`; the complex unit's `mPreviousPart != NULL`; the unit's
/// `mUnitID`.
pub const EE_SCENARIO_EV_UNIT_TYPE: u32 = 24;
pub const EE_SCENARIO_EV_UNIT_HIT_POINTS: u32 = 25;
pub const EE_SCENARIO_EV_UNIT_COMPLEX: u32 = 26;
pub const EE_SCENARIO_EV_COMPLEX_FLIGHT: u32 = 27;
pub const EE_SCENARIO_EV_COMPLEX_BUILT: u32 = 28;
pub const EE_SCENARIO_EV_UNIT_ON_TILE: u32 = 29;
pub const EE_SCENARIO_EV_COMPLEX_CARRIED: u32 = 30;
pub const EE_SCENARIO_EV_UNIT_ID: u32 = 31;
/// Unit a0's `mComplexUnit->MakeAreaEffects()`, `SightOffHere()`, `SightOnHere()`, `mComplexUnit->SetFlightTime(a1)`,
/// `SetHitPoints(a1)`, `GetComplexUnit()`; complex unit a0's `GetUnitClass()`, `GetClass()->mUnitType`, as an
/// `EEUnit*`; unit a0's `mUnitClass`.
pub const EE_SCENARIO_EV_MAKE_AREA_EFFECTS: u32 = 32;
pub const EE_SCENARIO_EV_SIGHT_OFF: u32 = 33;
pub const EE_SCENARIO_EV_SIGHT_ON: u32 = 34;
pub const EE_SCENARIO_EV_SET_FLIGHT: u32 = 35;
pub const EE_SCENARIO_EV_SET_HIT_POINTS: u32 = 36;
pub const EE_SCENARIO_EV_GET_COMPLEX: u32 = 37;
pub const EE_SCENARIO_EV_COMPLEX_CLASS: u32 = 38;
pub const EE_SCENARIO_EV_COMPLEX_CLASS_TYPE: u32 = 39;
pub const EE_SCENARIO_EV_COMPLEX_AS_UNIT: u32 = 40;
pub const EE_SCENARIO_EV_UNIT_CLASS: u32 = 41;
/// Player a0's `GetTechTree()->Get(a1)`; node a0's `SetButton(a1)`, `mData->mUnitClass`, `mData->mKind`,
/// `mResearchDelay++`, `ResearchNode(NULL)`; `EEButtonObject::Get(a0)`; player a0's `SetClassButton(a1, a2)`, its
/// `mGroups[a1]->GetFirstUnit()`; `gUserInterface != NULL`, its `PostRefreshSavedGroup(a0)`; player a0's
/// `GetTechTree()->mMediaID = a1`; `gUserInterface->PostUpdateBackgroundMedia()`.
pub const EE_SCENARIO_EV_TREE_NODE: u32 = 50;
pub const EE_SCENARIO_EV_NODE_SET_BUTTON: u32 = 51;
pub const EE_SCENARIO_EV_NODE_UNIT_CLASS: u32 = 52;
pub const EE_SCENARIO_EV_NODE_KIND: u32 = 53;
pub const EE_SCENARIO_EV_NODE_DELAY: u32 = 54;
pub const EE_SCENARIO_EV_RESEARCH_NODE: u32 = 55;
pub const EE_SCENARIO_EV_BUTTON: u32 = 56;
pub const EE_SCENARIO_EV_SET_CLASS_BUTTON: u32 = 57;
pub const EE_SCENARIO_EV_GROUP_FIRST: u32 = 58;
pub const EE_SCENARIO_EV_UI_EXISTS: u32 = 59;
pub const EE_SCENARIO_EV_POST_REFRESH_GROUP: u32 = 60;
pub const EE_SCENARIO_EV_TREE_SET_MEDIA: u32 = 61;
pub const EE_SCENARIO_EV_POST_BACKGROUND: u32 = 62;
/// `EETechTree::IsLatestEffect(player a0's tree, a1, a2, a3)`; `EEMedia::Get(a0)`; upgrade a0's `mModel`; player
/// a0's `SetClassModel(a1, a2)`; complex unit a0's `SwapGraphic(a1, a2)`; `EESoundObject::Get(a0)`; player a0's
/// `SetClassAcknowledgeSound(a1, a2, a3)`, `SetClassCreationSound(a1, a2)`, `SetClassDeathSound(a1, a2)`,
/// `SetClassSelectionSound(a1, a2, a3)`; sound a0's `Play()`; player a0's `mTechnologies->MakeObsolete(a1)`,
/// `->SetPrerequisite(a1, a2)`.
pub const EE_SCENARIO_EV_IS_LATEST: u32 = 63;
pub const EE_SCENARIO_EV_MEDIA: u32 = 64;
pub const EE_SCENARIO_EV_UPGRADE_MODEL: u32 = 65;
pub const EE_SCENARIO_EV_SET_CLASS_MODEL: u32 = 66;
pub const EE_SCENARIO_EV_SWAP_GRAPHIC: u32 = 67;
pub const EE_SCENARIO_EV_SOUND: u32 = 68;
pub const EE_SCENARIO_EV_SET_ACKNOWLEDGE: u32 = 69;
pub const EE_SCENARIO_EV_SET_CREATION: u32 = 70;
pub const EE_SCENARIO_EV_SET_DEATH: u32 = 71;
pub const EE_SCENARIO_EV_SET_SELECTION: u32 = 72;
pub const EE_SCENARIO_EV_PLAY: u32 = 73;
pub const EE_SCENARIO_EV_MAKE_OBSOLETE: u32 = 74;
pub const EE_SCENARIO_EV_SET_PREREQUISITE: u32 = 75;
/// `gUnitSetManager.GetSet(a0)->Contains(a1)`; upgrade a0's `CopyUpgradeLevels(a1)`; `EEReplaceQueuedTechnology(a0,
/// a1, a2)`; player a0's `MoveQueuedImprovements(a1, a2)`; `EEReplaceUnit(a0 as an EEComplexUnit*, a1, a2)`;
/// complex unit a0's `SetBehavior(GetClass()->mBehavior)`.
pub const EE_SCENARIO_EV_SET_CONTAINS: u32 = 80;
pub const EE_SCENARIO_EV_COPY_LEVELS: u32 = 81;
pub const EE_SCENARIO_EV_REPLACE_QUEUED: u32 = 82;
pub const EE_SCENARIO_EV_MOVE_QUEUED: u32 = 83;
pub const EE_SCENARIO_EV_REPLACE_UNIT: u32 = 84;
pub const EE_SCENARIO_EV_SET_BEHAVIOR: u32 = 85;
/// The game variant's `GetFactor(a0)` (bits); a class list (`new std::vector<EEUnitClass*>`), its
/// `EEUnitClass::GetClassesInSet(a1, *a0)`, `size()`, `[a1]`, `delete`; upgrade a0's `SetRange(a1)`,
/// `SetMaxVelocity(a1)` (bits); `gUnitSetManager.GetSet(a0)`; set a0's `ForEach` of the pathfinding functor (player
/// a1, terrain a2, enabled a3); upgrade a0's name set to string `UMFloatToLong(a1)` (bits).
pub const EE_SCENARIO_EV_VARIANT_FACTOR: u32 = 90;
pub const EE_SCENARIO_EV_CLASSES_NEW: u32 = 91;
pub const EE_SCENARIO_EV_CLASSES_IN_SET: u32 = 92;
pub const EE_SCENARIO_EV_CLASSES_SIZE: u32 = 93;
pub const EE_SCENARIO_EV_CLASSES_AT: u32 = 94;
pub const EE_SCENARIO_EV_CLASSES_FREE: u32 = 95;
pub const EE_SCENARIO_EV_SET_RANGE: u32 = 96;
pub const EE_SCENARIO_EV_SET_MAX_VELOCITY: u32 = 97;
pub const EE_SCENARIO_EV_UNIT_SET: u32 = 98;
pub const EE_SCENARIO_EV_PATHFINDING: u32 = 99;
pub const EE_SCENARIO_EV_SET_NAME: u32 = 100;
/// `EEEvent::sEvents.size()`, `[a0].get()`; event a0's `mEffects` (`int32_t*`), `mNumEffects`.
pub const EE_SCENARIO_EV_EVENT_COUNT: u32 = 110;
pub const EE_SCENARIO_EV_EVENT_AT: u32 = 111;
pub const EE_SCENARIO_EV_EVENT_EFFECTS: u32 = 112;
pub const EE_SCENARIO_EV_EVENT_NUM: u32 = 113;
/// The load (files a0 `FSFileSpec*`): `Open(kFileAccessRead)`; `GetLength` (into a1, kept on failure);
/// `gEventEffects.assign(a0, EEEventEffect())`; the effects read from a0 (the table's bytes); `sEvents.clear()`;
/// `sEvents.emplace_back(new EEEvent)` (the event); a0's `Read` of the 100-byte name, of event a1's `mID`, of its
/// `mNumEffects` (the result); event a0's `mEffects = new int32_t[a1]`; a0's `Read` of event a1's indices (the
/// result); event a0's `mEffects = NULL`.
pub const EE_SCENARIO_EV_FILE_OPEN: u32 = 120;
pub const EE_SCENARIO_EV_FILE_LENGTH: u32 = 121;
pub const EE_SCENARIO_EV_EFFECTS_ASSIGN: u32 = 122;
pub const EE_SCENARIO_EV_EFFECTS_READ: u32 = 123;
pub const EE_SCENARIO_EV_EVENTS_CLEAR: u32 = 124;
pub const EE_SCENARIO_EV_EVENT_NEW: u32 = 125;
pub const EE_SCENARIO_EV_READ_NAME: u32 = 126;
pub const EE_SCENARIO_EV_READ_ID: u32 = 127;
pub const EE_SCENARIO_EV_READ_COUNT: u32 = 128;
pub const EE_SCENARIO_EV_EVENT_ALLOC: u32 = 129;
pub const EE_SCENARIO_EV_READ_EFFECTS: u32 = 130;
pub const EE_SCENARIO_EV_EVENT_NO_EFFECTS: u32 = 131;

/// The world (the C++ statements) and the simulation capture.
pub trait EvHost {
    /// One statement (`EE_SCENARIO_EV_*`).
    fn op(&mut self, op: u32, a: [i64; 4]) -> i64;
    /// `EESimCaptureRecord::Active()`.
    fn capturing(&mut self) -> bool;
    /// A record committed.
    fn commit(&mut self, record: &Record);
    /// EVUN: one unit of a walk (`ee_tech_capture_unit_step`: dropped when not capturing).
    #[allow(clippy::too_many_arguments)]
    fn unit_step(
        &mut self,
        id: c_ulong,
        unit_type: c_long,
        hp: c_long,
        flags: u32,
        flight: c_long,
        applies: bool,
        value: c_long,
    );
    /// EVUN: the walk's record (`ee_tech_capture_unit_walk`).
    fn unit_walk(&mut self, walk: c_long, effect: c_long, unit_type: c_long, a: c_long, b: c_long);
    /// EVHP (`ee_tech_capture_rescale`).
    fn rescale(&mut self, what: c_long, unit: c_ulong, value: c_long, old: c_ulong, new: c_long, amount: f32);
    /// `ee_units_uu_player_units_of_type(unit_type, index)` into an empty list.
    fn units_of_type(&mut self, unit_type: i32, index: i32) -> Vec<u64>;
}

fn tag(name: &[u8; 4]) -> u32 {
    u32::from_le_bytes(*name)
}

fn bits(v: f32) -> i64 {
    v.to_bits() as i64
}

struct Ev<'a, H: EvHost + ?Sized> {
    h: &'a mut H,
}

impl<'a, H: EvHost + ?Sized> Ev<'a, H> {
    fn x(&mut self, op: u32, a0: i64) -> i64 {
        self.h.op(op, [a0, 0, 0, 0])
    }
    fn x2(&mut self, op: u32, a0: i64, a1: i64) -> i64 {
        self.h.op(op, [a0, a1, 0, 0])
    }
    fn x3(&mut self, op: u32, a0: i64, a1: i64, a2: i64) -> i64 {
        self.h.op(op, [a0, a1, a2, 0])
    }
    fn x4(&mut self, op: u32, a0: i64, a1: i64, a2: i64, a3: i64) -> i64 {
        self.h.op(op, [a0, a1, a2, a3])
    }
    fn loading(&mut self) -> bool {
        self.x(EE_SCENARIO_EV_LOADING, 0) != 0
    }
    fn effect(&mut self, k: u64) -> EeScenarioEvEffect {
        let data = self.x(EE_SCENARIO_EV_EFFECTS_DATA, 0) as usize as *const EeScenarioEvEffect;
        // SAFETY: k is a known effect (the callers test it against the table's size); the table is live
        unsafe { *data.add(k as usize) }
    }
    fn player_or(&mut self, player: i64, index: c_long) -> i64 {
        if player == 0 {
            self.x(EE_SCENARIO_EV_PLAYER, index as i64)
        } else {
            player
        }
    }
    fn player_id(&mut self, player: i64) -> c_ulong {
        self.x(EE_SCENARIO_EV_PLAYER_ID, player) as c_ulong
    }
    fn player_fields(&mut self, player: i64) -> *mut EeUnitsPlayerFields {
        self.x(EE_SCENARIO_EV_PLAYER_FIELDS, player) as usize as *mut EeUnitsPlayerFields
    }
    fn class(&mut self, index: i64) -> i64 {
        self.x(EE_SCENARIO_EV_CLASS, index)
    }
    fn class_fields(&mut self, class: i64) -> EeUnitsUnitClassFields {
        let p = self.x(EE_SCENARIO_EV_CLASS_FIELDS, class) as usize as *const EeUnitsUnitClassFields;
        // SAFETY: a live class's record (the C++ dereferenced it the same way)
        unsafe { *p }
    }
    fn upgrade_fields(&mut self, upgrade: i64) -> *mut EeUnitsUnitClassUpgradeFields {
        self.x(EE_SCENARIO_EV_UPGRADE_FIELDS, upgrade) as usize as *mut EeUnitsUnitClassUpgradeFields
    }
    /// `EETechUnitViewOf`.
    fn view(&mut self, unit: i64) -> EeTechUnitView {
        let unit_type = self.x(EE_SCENARIO_EV_UNIT_TYPE, unit) as i32;
        let hit_points = self.x(EE_SCENARIO_EV_UNIT_HIT_POINTS, unit) as i32;
        let complex = self.x(EE_SCENARIO_EV_UNIT_COMPLEX, unit);
        let flight_time = if complex != 0 { self.x(EE_SCENARIO_EV_COMPLEX_FLIGHT, complex) as i32 } else { 0 };
        let mut flags = 0;
        if complex != 0 {
            flags |= EE_TECH_UNIT_COMPLEX;
            if self.x(EE_SCENARIO_EV_COMPLEX_BUILT, complex) != 0 {
                flags |= EE_TECH_UNIT_BUILT;
            }
        }
        if self.x(EE_SCENARIO_EV_UNIT_ON_TILE, unit) != 0 {
            flags |= EE_TECH_UNIT_ON_TILE;
        }
        if complex != 0 && self.x(EE_SCENARIO_EV_COMPLEX_CARRIED, complex) != 0 {
            flags |= EE_TECH_UNIT_CARRIED;
        }
        EeTechUnitView { unit_type, hit_points, flight_time, flags }
    }
    /// `EETechCaptureUnitStep` (the unit's values read only while capturing, as the C++ hook).
    fn step(&mut self, unit: i64, applies: bool, value: c_long) {
        if !self.h.capturing() {
            return;
        }
        let id = self.x(EE_SCENARIO_EV_UNIT_ID, unit) as c_ulong;
        let v = self.view(unit);
        self.h.unit_step(
            id,
            v.unit_type as c_long,
            v.hit_points as c_long,
            v.flags,
            v.flight_time as c_long,
            applies,
            value,
        );
    }
    fn units(&mut self, player: i64) -> Vec<i64> {
        let mut units = Vec::new();
        let mut u = self.x(EE_SCENARIO_EV_FIRST_UNIT, player);
        while u != 0 {
            units.push(u);
            u = self.x(EE_SCENARIO_EV_NEXT_UNIT, u);
        }
        units
    }

    /// `EEEvent::Trigger`.
    fn trigger(&mut self, event: c_long, index: c_long, id: c_long, player: i64, unit: i64) -> c_long {
        let mut result = 0;
        let player_index = if player != 0 { self.player_id(player) as c_long } else { index };
        if event >= 0 && (event as c_ulong as u64) < self.x(EE_SCENARIO_EV_EVENT_COUNT, 0) as u64 {
            let e = self.x(EE_SCENARIO_EV_EVENT_AT, event as i64);
            if e != 0 {
                let effects = self.x(EE_SCENARIO_EV_EVENT_EFFECTS, e) as usize as *const i32;
                if !effects.is_null() {
                    let n = self.x(EE_SCENARIO_EV_EVENT_NUM, e) as c_ulong;
                    for i in 0..n {
                        // SAFETY: the event's live index list of mNumEffects entries
                        let k = unsafe { *effects.add(i as usize) };
                        result = self.run_effect(k as c_ulong, player_index, id, player, unit);
                    }
                }
            }
        }
        result
    }

    /// `EEEvent::RunEffect`: the EVRN record around the run.
    fn run_effect(&mut self, k: c_ulong, index: c_long, id: c_long, player: i64, unit: i64) -> c_long {
        if !self.h.capturing() {
            return self.run_uncaptured(k, index, id, player, unit);
        }
        let mut r = Record::new(tag(b"EVRN"));
        r.u32(k as u32);
        let known = (k as u64) < self.x(EE_SCENARIO_EV_EFFECTS_COUNT, 0) as u64;
        let e = if known { Some(self.effect(k as u64)) } else { None };
        r.i32(e.map_or(-1, |e| e.mKind));
        r.i32(e.map_or(-1, |e| e.mAttribute));
        r.i32(if player != 0 { self.player_id(player) as i32 } else { index as i32 });
        r.i32(id as i32);
        r.i32(if unit != 0 { self.x(EE_SCENARIO_EV_UNIT_ID, unit) as c_long as i32 } else { -1 });
        let loading = self.loading();
        let enabled = self.x(EE_SCENARIO_EV_EVENTS_ENABLED, 0) != 0;
        r.u32((if loading { 1 } else { 0 }) | (if enabled { 2 } else { 0 }));
        let result = self.run_uncaptured(k, index, id, player, unit);
        r.i32(result as i32);
        self.h.commit(&r);
        result
    }

    /// `EEEvent::RunEffectUncaptured`: the handler of the effect's kind.
    fn run_uncaptured(&mut self, k: c_ulong, index: c_long, id: c_long, player: i64, unit: i64) -> c_long {
        let player_index = if player != 0 { self.player_id(player) as c_long } else { index };
        let known = (k as u64) < self.x(EE_SCENARIO_EV_EFFECTS_COUNT, 0) as u64;
        let e = if known { self.effect(k as u64) } else { EeScenarioEvEffect::default() };
        let loading = self.loading();
        let enabled = self.x(EE_SCENARIO_EV_EVENTS_ENABLED, 0) != 0;
        let handler = effects::effect_handler(known, e.mKind, loading, e.mRunOnLoad, enabled);
        match handler {
            EeTechEffectHandler::Refused => 9,
            EeTechEffectHandler::UnknownKind => 3,
            EeTechEffectHandler::Nothing | EeTechEffectHandler::Kind7 => 0,
            EeTechEffectHandler::Attribute => self.attribute(k as c_long, &e, player_index, player),
            EeTechEffectHandler::Prerequisite => {
                let p = self.player_or(player, player_index);
                if p == 0 {
                    return 9;
                }
                self.x3(EE_SCENARIO_EV_SET_PREREQUISITE, p, e.mTechnology as i64, id as i64);
                0
            }
            EeTechEffectHandler::Obsolete => {
                let p = self.player_or(player, player_index);
                if p == 0 {
                    return 9;
                }
                self.x2(EE_SCENARIO_EV_MAKE_OBSOLETE, p, e.mTechnology as i64);
                0
            }
            EeTechEffectHandler::UnitModel => self.unit_model(&e, player_index, id, player),
            EeTechEffectHandler::TechTreeMedia => {
                let p = self.player_or(player, player_index);
                if p == 0 {
                    return 0;
                }
                self.x2(EE_SCENARIO_EV_TREE_SET_MEDIA, p, e.mMediaIndex as i64);
                self.x(EE_SCENARIO_EV_POST_BACKGROUND, 0);
                if p == self.x(EE_SCENARIO_EV_CURRENT_PLAYER, 0) {
                    self.x(EE_SCENARIO_EV_POST_BACKGROUND, 0);
                }
                0
            }
            EeTechEffectHandler::Research => {
                let p = self.player_or(player, player_index);
                if p == 0 {
                    return 9;
                }
                let node = self.x2(EE_SCENARIO_EV_TREE_NODE, p, e.mTechnology as i64);
                if node != 0 {
                    self.x(EE_SCENARIO_EV_RESEARCH_NODE, node) as c_long
                } else {
                    0
                }
            }
            EeTechEffectHandler::Button => self.button(&e, player_index, player),
            EeTechEffectHandler::Sound => {
                let sound = self.x(EE_SCENARIO_EV_SOUND, e.mSoundID as i64);
                if sound == 0 {
                    return 9;
                }
                self.x(EE_SCENARIO_EV_PLAY, sound);
                0
            }
            EeTechEffectHandler::UnitSound => self.unit_sound(&e, player_index, id, player),
            EeTechEffectHandler::ConvertUnits => self.convert(k as c_long, &e, player_index, id, unit),
        }
    }

    /// `SetEventButton`.
    fn button(&mut self, e: &EeScenarioEvEffect, index: c_long, player: i64) -> c_long {
        let p = self.player_or(player, index);
        let node = self.x2(EE_SCENARIO_EV_TREE_NODE, p, e.mTechnology as i64);
        let button = self.x(EE_SCENARIO_EV_BUTTON, e.mButton as i64);
        if node == 0 || button == 0 {
            return 9;
        }
        self.x2(EE_SCENARIO_EV_NODE_SET_BUTTON, node, e.mButton as i64);
        let class_index = self.x(EE_SCENARIO_EV_NODE_UNIT_CLASS, node);
        let class = self.class(class_index);
        if class != 0 {
            let unit_type = self.class_fields(class).mUnitType;
            self.x3(EE_SCENARIO_EV_SET_CLASS_BUTTON, p, unit_type as i64, button);
            if !self.loading() && p == self.x(EE_SCENARIO_EV_CURRENT_PLAYER, 0) {
                for i in 0..20i64 {
                    if self.x2(EE_SCENARIO_EV_GROUP_FIRST, p, i) != 0 {
                        let first = self.x2(EE_SCENARIO_EV_GROUP_FIRST, p, i);
                        if self.x(EE_SCENARIO_EV_UNIT_CLASS, first) == class && self.x(EE_SCENARIO_EV_UI_EXISTS, 0) != 0
                        {
                            self.x(EE_SCENARIO_EV_POST_REFRESH_GROUP, i + 1);
                        }
                    }
                }
            }
        }
        0
    }

    /// `SetEventUnitModel`.
    fn unit_model(&mut self, e: &EeScenarioEvEffect, index: c_long, id: c_long, player: i64) -> c_long {
        let p = self.player_or(player, index);
        let class = self.class(e.mUnitClass as i64);
        if class == 0 {
            return 9;
        }
        let unit_type = self.class_fields(class).mUnitType as i64;
        if self.x4(EE_SCENARIO_EV_IS_LATEST, p, id as i64, unit_type, e.mKind as i64) == 0 {
            return 9;
        }
        let upgrade = self.x2(EE_SCENARIO_EV_UPGRADE, p, unit_type);
        if upgrade == 0 {
            return 9;
        }
        let model = self.x(EE_SCENARIO_EV_MEDIA, e.mMediaIndex as i64);
        if model == 0 {
            return 9;
        }
        let old = self.x(EE_SCENARIO_EV_UPGRADE_MODEL, upgrade);
        self.x3(EE_SCENARIO_EV_SET_CLASS_MODEL, p, unit_type, model);
        let mut u = self.x(EE_SCENARIO_EV_FIRST_UNIT, p);
        while u != 0 {
            if self.x(EE_SCENARIO_EV_GET_COMPLEX, u) != 0 {
                let c = self.x(EE_SCENARIO_EV_GET_COMPLEX, u);
                if self.x(EE_SCENARIO_EV_COMPLEX_CLASS, c) == class {
                    let fields = self.upgrade_fields(upgrade);
                    // SAFETY: the live upgrade's record
                    let media = unsafe { (*fields).mMediaCount } as c_ulong;
                    if media > 0 {
                        let c = self.x(EE_SCENARIO_EV_GET_COMPLEX, u);
                        self.x3(EE_SCENARIO_EV_SWAP_GRAPHIC, c, old, model);
                    }
                }
            }
            u = self.x(EE_SCENARIO_EV_NEXT_UNIT, u);
        }
        0
    }

    /// `SetEventUnitSound`.
    fn unit_sound(&mut self, e: &EeScenarioEvEffect, index: c_long, id: c_long, player: i64) -> c_long {
        let p = self.player_or(player, index);
        let class = self.class(e.mUnitClass as i64);
        if class == 0 {
            return 9;
        }
        let unit_type = self.class_fields(class).mUnitType as i64;
        if self.x4(EE_SCENARIO_EV_IS_LATEST, p, id as i64, unit_type, e.mKind as i64) == 0 {
            return 9;
        }
        if self.x2(EE_SCENARIO_EV_GET_UPGRADE, p, unit_type) == 0 {
            return 9;
        }
        let sound = self.x(EE_SCENARIO_EV_SOUND, e.mSoundID as i64);
        if sound == 0 {
            return 9;
        }
        match e.mKind {
            0xf => self.x4(EE_SCENARIO_EV_SET_ACKNOWLEDGE, p, unit_type, sound, 0),
            0x10 => self.x3(EE_SCENARIO_EV_SET_CREATION, p, unit_type, sound),
            0x11 => self.x3(EE_SCENARIO_EV_SET_DEATH, p, unit_type, sound),
            0x12 => self.x4(EE_SCENARIO_EV_SET_SELECTION, p, unit_type, sound, 0),
            0x14 => self.x4(EE_SCENARIO_EV_SET_ACKNOWLEDGE, p, unit_type, sound, 1),
            0x15 => self.x4(EE_SCENARIO_EV_SET_SELECTION, p, unit_type, sound, 1),
            _ => 0,
        };
        0
    }

    /// `ConvertEventUnits`.
    fn convert(&mut self, k: c_long, e: &EeScenarioEvEffect, index: c_long, id: c_long, unit: i64) -> c_long {
        let p = self.x(EE_SCENARIO_EV_PLAYER, index as i64);
        let mut from = e.mUnitClass as c_ulong;
        let mut in_set = false;
        let to = e.mNewUnitClass as c_ulong;
        let mut result = 0;
        if unit != 0 && p != 0 && self.x2(EE_SCENARIO_EV_TREE_NODE, p, id as i64) != 0 && {
            let node = self.x2(EE_SCENARIO_EV_TREE_NODE, p, id as i64);
            self.x(EE_SCENARIO_EV_NODE_KIND, node) == 3
        } {
            from = self.x(EE_SCENARIO_EV_COMPLEX_CLASS_TYPE, unit) as c_ulong;
            in_set = self.x2(EE_SCENARIO_EV_SET_CONTAINS, e.mUnitSet as i64, from as i64) != 0;
        }
        let conversion = effects::conversion(unit != 0, in_set, true, id as i32);
        let units: Vec<i64> = if conversion.single {
            vec![self.x(EE_SCENARIO_EV_COMPLEX_AS_UNIT, unit)]
        } else {
            let to_upgrade = self.x2(EE_SCENARIO_EV_UPGRADE, p, to as i64);
            let from_upgrade = self.x2(EE_SCENARIO_EV_UPGRADE, p, from as i64);
            self.x2(EE_SCENARIO_EV_COPY_LEVELS, from_upgrade, to_upgrade);
            let from_class = self.x(EE_SCENARIO_EV_UPGRADE_CLASS, from_upgrade);
            let to_class = self.x(EE_SCENARIO_EV_UPGRADE_CLASS, to_upgrade);
            let technology = self.class_fields(to_class).mTechnology;
            self.x3(EE_SCENARIO_EV_REPLACE_QUEUED, p, from_class, technology as i64);
            self.h.units_of_type(from as i32, index as i32).into_iter().map(|u| u as i64).collect()
        };
        if !units.is_empty() {
            let class = self.class(to as i64);
            if class != 0 && conversion.delay_new_class {
                let technology = self.class_fields(class).mTechnology;
                let node = self.x2(EE_SCENARIO_EV_TREE_NODE, p, technology as i64);
                if node != 0 {
                    self.x(EE_SCENARIO_EV_NODE_DELAY, node);
                }
            }
            if conversion.move_queued {
                self.x3(EE_SCENARIO_EV_MOVE_QUEUED, p, from as i64, to as i64);
            }
            let mut new = 0;
            for u in units {
                let view = self.view(u);
                let step = effects::conversion_unit(&view);
                if step == 0 {
                    self.step(u, false, 0);
                    continue;
                }
                let replace = step == 2;
                self.step(u, replace, (new != 0) as c_long);
                if replace {
                    new = self.x3(EE_SCENARIO_EV_REPLACE_UNIT, u, index as i64, class);
                }
                if conversion.set_behavior && new != 0 {
                    self.x(EE_SCENARIO_EV_SET_BEHAVIOR, new);
                }
            }
        } else {
            result = 3;
        }
        let flags = (if unit != 0 { 1 } else { 0 })
            | (if in_set { 2 } else { 0 })
            | (if result == 3 { 4 } else { 0 })
            | (if id == 0x413 || id == 0x138d { 8 } else { 0 });
        self.h.unit_walk(7, k, from as c_long, flags, to as c_long);
        result
    }

    /// `EERescaleUnit`.
    #[allow(clippy::too_many_arguments)]
    fn rescale_unit(
        &mut self,
        u: i64,
        flight: bool,
        walk: EeTechUnitWalk,
        class_type: c_ulong,
        old: c_ulong,
        new: c_long,
        capture: bool,
    ) {
        let view = self.view(u);
        let step = effects::unit_step(walk, &view, class_type as i32, old as u32, new as i32);
        if !step.rescaled {
            self.step(u, false, 0);
            return;
        }
        if capture {
            let id = self.x(EE_SCENARIO_EV_UNIT_ID, u) as c_ulong;
            let value = if flight {
                let c = self.x(EE_SCENARIO_EV_UNIT_COMPLEX, u);
                self.x(EE_SCENARIO_EV_COMPLEX_FLIGHT, c) as c_long
            } else {
                self.x(EE_SCENARIO_EV_UNIT_HIT_POINTS, u) as c_long
            };
            self.h.rescale(if flight { 1 } else { 0 }, id, value, old, new, step.amount);
        }
        self.step(u, step.applies, step.value as c_long);
        if step.applies && flight {
            self.x2(EE_SCENARIO_EV_SET_FLIGHT, u, step.value as i64);
        } else if step.applies {
            self.x2(EE_SCENARIO_EV_SET_HIT_POINTS, u, step.value as i64);
        }
    }

    /// `EERescaleUnits`: the player's units, then its pending units.
    #[allow(clippy::too_many_arguments)]
    fn rescale_units(
        &mut self,
        p: i64,
        flight: bool,
        class_type: c_ulong,
        old: c_ulong,
        new: c_long,
        capture: bool,
        k: c_long,
    ) {
        let mut u = self.x(EE_SCENARIO_EV_FIRST_UNIT, p);
        while u != 0 {
            let walk = if flight { EeTechUnitWalk::FlightTime } else { EeTechUnitWalk::HitPoints };
            self.rescale_unit(u, flight, walk, class_type, old, new, capture);
            u = self.x(EE_SCENARIO_EV_NEXT_UNIT, u);
        }
        self.h.unit_walk(if flight { 4 } else { 2 }, k, class_type as c_long, old as c_long, new);
        let mut j: c_ulong = 0;
        while (j as u64) < self.x(EE_SCENARIO_EV_PENDING_COUNT, p) as c_ulong as u64 {
            let u = self.x2(EE_SCENARIO_EV_PENDING_UNIT, p, j as i64);
            let walk = if flight { EeTechUnitWalk::FlightTime } else { EeTechUnitWalk::PendingHitPoints };
            self.rescale_unit(u, flight, walk, class_type, old, new, capture);
            j += 1;
        }
        self.h.unit_walk(if flight { 5 } else { 3 }, k, class_type as c_long, old as c_long, new);
    }

    /// `EETechCapturePlayer`.
    fn capture_player(&mut self, r: &mut Record, p: i64) {
        if p == 0 {
            r.i32(-1);
            return;
        }
        r.i32(self.player_id(p) as i32);
        let f = self.player_fields(p);
        // SAFETY: the live player's record
        let f = unsafe { &*f };
        r.i32(f.mPopulationLimitBonus as i32);
        r.f32(f.mTributeFee);
        r.u32(f.mBB44 as u32);
    }

    /// `EETechCaptureUpgrade`: the record's bytes up to `mIsInitialized`, the two media counts written as 0.
    fn capture_upgrade(&mut self, r: &mut Record, upgrade: i64) {
        if upgrade == 0 {
            r.u32(0);
            return;
        }
        let f = self.upgrade_fields(upgrade);
        // SAFETY: the live upgrade's record
        r.bytes(&upgrade_bytes(unsafe { &*f }));
    }

    /// `ApplyEventAttribute` with its EVAT records.
    fn attribute(&mut self, k: c_long, e: &EeScenarioEvEffect, index: c_long, player: i64) -> c_long {
        let p = self.player_or(player, index);
        let mut classes: Vec<i64> = Vec::new();
        let mut result: c_long = 0;
        let capture = self.h.capturing();
        let mut call = Record::new(tag(b"EVAT"));
        if capture {
            call.u32(0);
            call.i32(k as i32);
            call.i32(e.mAttribute);
            call.i32(index as i32);
            self.capture_player(&mut call, p);
        }
        let values = EeTechEffectValues { value: e.mValue, offset: e.mOffset, factor: e.mFactor };
        let op = effects::attribute(e.mAttribute);
        if p != 0 {
            let pf = self.player_fields(p);
            match op.op {
                // SAFETY (the three): the live player's record
                EeTechAttributeOp::PopulationBonus => unsafe {
                    (*pf).mPopulationLimitBonus =
                        effects::population_bonus((*pf).mPopulationLimitBonus as i32, &values) as c_long;
                },
                EeTechAttributeOp::TributeFee => unsafe { (*pf).mTributeFee = e.mFactor },
                EeTechAttributeOp::PlayerFlag => unsafe { (*pf).mBB44 = true },
                _ => {
                    let mut class = 0;
                    if e.mUnitClass != -1 {
                        class = self.class(e.mUnitClass as i64);
                        classes.push(class);
                    }
                    if class == 0 && e.mUnitSet != -1 {
                        let v = self.x(EE_SCENARIO_EV_CLASSES_NEW, 0);
                        self.x2(EE_SCENARIO_EV_CLASSES_IN_SET, v, e.mUnitSet as i64);
                        let n = self.x(EE_SCENARIO_EV_CLASSES_SIZE, v);
                        classes = (0..n).map(|i| self.x2(EE_SCENARIO_EV_CLASSES_AT, v, i)).collect();
                        self.x(EE_SCENARIO_EV_CLASSES_FREE, v);
                    }
                }
            }
            for &class in &classes {
                let cf = self.class_fields(class);
                let unit_type = cf.mUnitType;
                let upgrade = self.x2(EE_SCENARIO_EV_UPGRADE, p, unit_type as i64);
                let mut rec = Record::new(tag(b"EVAT"));
                if capture {
                    rec.u32(1);
                    rec.i32(k as i32);
                    rec.i32(unit_type as i32);
                    self.capture_upgrade(&mut rec, upgrade);
                }
                let uf = self.upgrade_fields(upgrade);
                result = self.attribute_class(
                    k,
                    e,
                    &values,
                    op.op,
                    op.index as usize,
                    p,
                    index,
                    player,
                    class,
                    &cf,
                    upgrade,
                    uf,
                    capture,
                    result,
                );
                if capture {
                    rec.i32(result as i32);
                    self.capture_upgrade(&mut rec, upgrade);
                    self.h.commit(&rec);
                }
            }
        } else {
            result = 9;
        }
        if capture {
            call.u32(classes.len() as u32);
            call.i32(result as i32);
            self.capture_player(&mut call, p);
            self.h.commit(&call);
        }
        result
    }

    /// One class of `ApplyEventAttribute`'s switch; the result as it stands after the case.
    #[allow(clippy::too_many_arguments)]
    fn attribute_class(
        &mut self,
        k: c_long,
        e: &EeScenarioEvEffect,
        v: &EeTechEffectValues,
        op: EeTechAttributeOp,
        i: usize,
        p: i64,
        index: c_long,
        player: i64,
        class: i64,
        cf: &EeUnitsUnitClassFields,
        upgrade: i64,
        uf: *mut EeUnitsUnitClassUpgradeFields,
        capture: bool,
        result: c_long,
    ) -> c_long {
        // SAFETY (every access of `u` below): the live upgrade's record; no reference lives across a C++ call
        macro_rules! u {
            () => {
                (*uf)
            };
        }
        match op {
            EeTechAttributeOp::Attack => unsafe {
                u!().mAttack = effects::apply_f32(cf.mAttack as f32, u!().mAttack, v)
            },
            EeTechAttributeOp::ReloadTime => unsafe {
                u!().mReloadTime = effects::apply_i32(cf.mReloadTime as i32, u!().mReloadTime as i32, v) as c_long
            },
            EeTechAttributeOp::Range => {
                let range = unsafe { effects::apply_f32(cf.mRange, u!().mRange, v) };
                self.x2(EE_SCENARIO_EV_SET_RANGE, upgrade, bits(range));
            }
            EeTechAttributeOp::LineOfSight => {
                for u in self.units(p) {
                    let view = self.view(u);
                    let applies = effects::sight_applies(&view, cf.mUnitType as i32);
                    self.step(u, applies, 0);
                    if applies {
                        self.x(EE_SCENARIO_EV_SIGHT_OFF, u);
                    }
                }
                self.h.unit_walk(0, k, cf.mUnitType as c_long, 0, 0);
                unsafe { u!().mLineOfSight = effects::apply_f32(cf.mLineOfSight, u!().mLineOfSight, v) };
                for u in self.units(p) {
                    let view = self.view(u);
                    let applies = effects::sight_applies(&view, cf.mUnitType as i32);
                    self.step(u, applies, 0);
                    if applies {
                        self.x(EE_SCENARIO_EV_SIGHT_ON, u);
                    }
                }
                self.h.unit_walk(1, k, cf.mUnitType as c_long, 0, 0);
            }
            EeTechAttributeOp::AreaEffect => unsafe {
                u!().mAreaEffect = effects::apply_f32(cf.mAreaEffect, u!().mAreaEffect, v)
            },
            EeTechAttributeOp::MaxHitPoints => {
                let old = unsafe { u!().mMaxHitPoints } as c_ulong;
                let upgrade_class = self.x(EE_SCENARIO_EV_UPGRADE_CLASS, upgrade);
                let upgrade_type = self.class_fields(upgrade_class).mUnitType;
                let factor = f32::from_bits(self.x(EE_SCENARIO_EV_VARIANT_FACTOR, upgrade_type as i64) as u32);
                let new = effects::max_hit_points(cf.mHitPoints as i32, factor, old as u32, v) as c_long;
                unsafe { u!().mMaxHitPoints = new };
                if old != 0 && !self.loading() {
                    self.rescale_units(p, false, cf.mUnitType, old, new, capture, k);
                }
            }
            EeTechAttributeOp::MaxVelocity => {
                let velocity = unsafe { effects::apply_f32(cf.mMaxVelocity, u!().mMaxVelocity, v) };
                self.x2(EE_SCENARIO_EV_SET_MAX_VELOCITY, upgrade, bits(velocity));
            }
            EeTechAttributeOp::FlightTime => {
                let old = unsafe { u!().mFlightTime } as c_ulong;
                let new = unsafe { effects::apply_i32(cf.mFlightTime as i32, u!().mFlightTime as i32, v) } as c_long;
                unsafe { u!().mFlightTime = new };
                if old != 0 && !self.loading() {
                    self.rescale_units(p, true, cf.mUnitType, old, new, capture, k);
                }
            }
            EeTechAttributeOp::Leadership => unsafe {
                u!().mLeadership = effects::apply_f32(cf.mLeadership as f32, u!().mLeadership, v)
            },
            EeTechAttributeOp::Armor => unsafe {
                u!().mArmor[i] = effects::apply_f32(cf.mArmor[i] as f32, u!().mArmor[i], v)
            },
            EeTechAttributeOp::Nothing => {}
            EeTechAttributeOp::GatherTime => {
                // ApplyEventGatherRate
                if upgrade != 0 {
                    unsafe {
                        u!().mGatherTimes[i] =
                            effects::gather_time(u!().mGatherTimes[i] as i32, cf.mGatherTimes[i] as i32, v) as c_long
                    };
                }
            }
            EeTechAttributeOp::ConvertsPriests => unsafe { u!().mConvertsPriests = true },
            EeTechAttributeOp::ConvertsBuildings => unsafe { u!().mConvertsBuildings = true },
            EeTechAttributeOp::AreaEffectTemplate => return self.area_effect(k, e, index, player, class, uf),
            EeTechAttributeOp::PowerRegeneration => unsafe {
                u!().mPowerRegeneration =
                    effects::apply_i32(cf.mPowerRegeneration as i32, u!().mPowerRegeneration as i32, v) as c_long
            },
            EeTechAttributeOp::Terrain => {
                if effects::terrain_type_valid(e.mTerrainType) {
                    if upgrade != 0 {
                        unsafe {
                            u!().mTerrainMask =
                                effects::terrain_mask(u!().mTerrainMask as u32, e.mTerrainType, e.mValue != 0.0)
                                    as c_ulong
                        };
                    }
                    let set = self.x(EE_SCENARIO_EV_UNIT_SET, e.mUnitSet as i64);
                    if set != 0 {
                        self.x4(EE_SCENARIO_EV_PATHFINDING, set, p, e.mTerrainType as i64, (e.mValue == 1.0) as i64);
                    }
                }
            }
            EeTechAttributeOp::Regeneration => unsafe {
                u!().mRegeneration = effects::apply_i32(cf.mRegeneration as i32, u!().mRegeneration as i32, v) as c_long
            },
            EeTechAttributeOp::Power => unsafe {
                u!().mPower = effects::apply_i32(cf.mPower as i32, u!().mPower as i32, v) as c_long
            },
            EeTechAttributeOp::Name => {
                self.x2(EE_SCENARIO_EV_SET_NAME, upgrade, bits(e.mValue));
            }
            _ => return 9,
        }
        result
    }

    /// `SetEventAreaEffect` (the player the caller's own, else the index's).
    fn area_effect(
        &mut self,
        k: c_long,
        e: &EeScenarioEvEffect,
        index: c_long,
        player: i64,
        class: i64,
        uf: *mut EeUnitsUnitClassUpgradeFields,
    ) -> c_long {
        let p = self.player_or(player, index);
        if class != 0 {
            // SAFETY: the live upgrade's record
            unsafe { (*uf).mAreaEffectTemplates[0] = e.mAreaEffect as c_long };
            if !self.loading() {
                let class_type = self.class_fields(class).mUnitType;
                for u in self.units(p) {
                    let view = self.view(u);
                    let applies = effects::area_effect_applies(&view, class_type as i32);
                    self.step(u, applies, 0);
                    if applies {
                        self.x(EE_SCENARIO_EV_MAKE_AREA_EFFECTS, u);
                    }
                }
                self.h.unit_walk(6, k, class_type as c_long, 0, 0);
            }
        }
        0
    }

    /// `EEUnitSetFunctorModifyPathfindingBehavior::Execute`.
    fn pathfinding(&mut self, player: i64, terrain: c_long, enabled: bool, unit_type: c_long) -> bool {
        if (unit_type as c_ulong as u64) < self.x(EE_SCENARIO_EV_CLASS_COUNT, 0) as c_ulong as u64 {
            let upgrade = self.x2(EE_SCENARIO_EV_GET_UPGRADE, player, unit_type as i64);
            if upgrade != 0 {
                let f = self.upgrade_fields(upgrade);
                // SAFETY: the live upgrade's record
                unsafe {
                    (*f).mTerrainMask =
                        effects::terrain_mask((*f).mTerrainMask as u32, terrain as i32, enabled) as c_ulong
                };
            }
        }
        true
    }

    /// `EEEvent::LoadEvents`: every effect record, then every event (its 100-byte name, id, count and indices) to
    /// the end of the file; the last read's result.
    fn load(&mut self, events: i64, effects_file: i64) -> c_long {
        let mut result: c_long = 0;
        self.x(EE_SCENARIO_EV_FILE_OPEN, events);
        self.x(EE_SCENARIO_EV_FILE_OPEN, effects_file);
        let mut length = self.x2(EE_SCENARIO_EV_FILE_LENGTH, effects_file, 0) as c_ulong;
        let size = std::mem::size_of::<EeScenarioEvEffect>() as c_ulong;
        self.x(EE_SCENARIO_EV_EFFECTS_ASSIGN, (length / size) as i64);
        if self.x(EE_SCENARIO_EV_EFFECTS_COUNT, 0) != 0 {
            self.x(EE_SCENARIO_EV_EFFECTS_READ, effects_file);
        }
        length = self.x2(EE_SCENARIO_EV_FILE_LENGTH, events, length as i64) as c_ulong;
        self.x(EE_SCENARIO_EV_EVENTS_CLEAR, 0);
        let mut position: c_ulong = 0;
        while position.wrapping_add(108) <= length {
            let event = self.x(EE_SCENARIO_EV_EVENT_NEW, 0);
            self.x(EE_SCENARIO_EV_READ_NAME, events);
            self.x2(EE_SCENARIO_EV_READ_ID, events, event);
            result = self.x2(EE_SCENARIO_EV_READ_COUNT, events, event) as c_long;
            let n = self.x(EE_SCENARIO_EV_EVENT_NUM, event) as c_ulong;
            position = position.wrapping_add(108).wrapping_add(n.wrapping_mul(4));
            if n > 0 {
                self.x2(EE_SCENARIO_EV_EVENT_ALLOC, event, n as i64);
                result = self.x2(EE_SCENARIO_EV_READ_EFFECTS, events, event) as c_long;
            } else {
                self.x(EE_SCENARIO_EV_EVENT_NO_EFFECTS, event);
            }
        }
        result
    }
}

/// `EETechCaptureUpgrade`'s block: its size, then the record's bytes before `mIsInitialized` with `mMediaCount` and
/// `mButtonLoadCount` written as 0.
pub fn upgrade_bytes(f: &EeUnitsUnitClassUpgradeFields) -> Vec<u8> {
    let mut copy = *f;
    copy.mMediaCount = 0;
    copy.mButtonLoadCount = 0;
    let size = std::mem::offset_of!(EeUnitsUnitClassUpgradeFields, mIsInitialized);
    let mut out = Vec::with_capacity(size + 4);
    out.extend_from_slice(&(size as u32).to_le_bytes());
    // SAFETY: the record is plain data with no padding before mIsInitialized (4-byte fields, then four bools)
    out.extend_from_slice(unsafe { std::slice::from_raw_parts(&copy as *const _ as *const u8, size) });
    out
}

/// `EEEvent::Trigger` over a host.
pub fn trigger<H: EvHost + ?Sized>(
    h: &mut H,
    event: c_long,
    index: c_long,
    id: c_long,
    player: i64,
    unit: i64,
) -> c_long {
    Ev { h }.trigger(event, index, id, player, unit)
}

/// `EEEvent::LoadEvents` over a host.
pub fn load<H: EvHost + ?Sized>(h: &mut H, events: i64, effects: i64) -> c_long {
    Ev { h }.load(events, effects)
}

/// `EEUnitSetFunctorModifyPathfindingBehavior::Execute` over a host.
pub fn pathfinding<H: EvHost + ?Sized>(
    h: &mut H,
    player: i64,
    terrain: c_long,
    enabled: bool,
    unit_type: c_long,
) -> bool {
    Ev { h }.pathfinding(player, terrain, enabled, unit_type)
}

// ---- the game's host and the C ABI (EEEvent.cpp's members forward here)

#[cfg(not(test))]
mod sys {
    extern "C" {
        pub fn ee_scenariocpp_event(op: u32, a: *const i64) -> i64;
    }
}

/// The game's host: the C++ statements, the capture writer, the unit utilities.
pub struct EeScenarioEvGame;

impl EvHost for EeScenarioEvGame {
    fn op(&mut self, op: u32, a: [i64; 4]) -> i64 {
        #[cfg(not(test))]
        {
            // SAFETY: the C++ host's contract (EEEvent.cpp): the arguments its op names
            unsafe { sys::ee_scenariocpp_event(op, a.as_ptr()) }
        }
        #[cfg(test)]
        {
            let _ = (op, a);
            unreachable!("the event host's game statements")
        }
    }
    fn capturing(&mut self) -> bool {
        crate::app::capture_record::active()
    }
    fn commit(&mut self, record: &Record) {
        record.commit();
    }
    fn unit_step(
        &mut self,
        id: c_ulong,
        unit_type: c_long,
        hp: c_long,
        flags: u32,
        flight: c_long,
        applies: bool,
        value: c_long,
    ) {
        crate::tech::capture_rec::ee_tech_capture_unit_step(id, unit_type, hp, flags, flight, applies, value);
    }
    fn unit_walk(&mut self, walk: c_long, effect: c_long, unit_type: c_long, a: c_long, b: c_long) {
        crate::tech::capture_rec::ee_tech_capture_unit_walk(walk, effect, unit_type, a, b);
    }
    fn rescale(&mut self, what: c_long, unit: c_ulong, value: c_long, old: c_ulong, new: c_long, amount: f32) {
        crate::tech::capture_rec::ee_tech_capture_rescale(what, unit, value, old, new, amount);
    }
    fn units_of_type(&mut self, unit_type: i32, index: i32) -> Vec<u64> {
        crate::units::uu_direct::player_units_of_type(unit_type, index)
    }
}

/// `EEEvent::Trigger`: runs every effect of event `event` for a player (a given player's own index wins); the last
/// effect's result.
#[no_mangle]
pub extern "C" fn ee_scenario_ev_trigger(
    event: c_long,
    index: c_long,
    id: c_long,
    player: *mut std::ffi::c_void,
    unit: *mut std::ffi::c_void,
) -> c_long {
    trigger(&mut EeScenarioEvGame, event, index, id, player as usize as i64, unit as usize as i64)
}

/// `EEEvent::LoadEvents` (`events`, `effects` the two `FSFileSpec*`).
#[no_mangle]
pub extern "C" fn ee_scenario_ev_load(events: *mut std::ffi::c_void, effects: *mut std::ffi::c_void) -> c_long {
    load(&mut EeScenarioEvGame, events as usize as i64, effects as usize as i64)
}

/// `EEUnitSetFunctorModifyPathfindingBehavior::Execute` (always continues the set's walk).
#[no_mangle]
pub extern "C" fn ee_scenario_ev_pathfinding(
    player: *mut std::ffi::c_void,
    terrain: c_long,
    enabled: bool,
    unit_type: c_long,
) -> bool {
    pathfinding(&mut EeScenarioEvGame, player as usize as i64, terrain, enabled, unit_type)
}

#[cfg(test)]
#[path = "event_tests.rs"]
mod tests;
