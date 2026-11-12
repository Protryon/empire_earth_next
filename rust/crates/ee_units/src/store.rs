//! The unit store, stage one (RUST_PLAN R5b.4 leaf 5): each unit's plain state as a Rust record, readable by id
//! without a call into C++ (`include/ee/EEUnitStore.h`).
//!
//! A record lives while its unit is in a player's list: inserted by `RTSPlayer::InsertUnit` (seeded whole from the
//! object), removed by `RTSPlayer::RemoveKilledUnits`, a player's records dropped by `RTSPlayer::Reset`. That is
//! the domain of `RTSPlayer::GetUnit` / `gWorld.GetUnit`. Between, the C++ members it mirrors are write-through
//! holders (`EEUnitStoreField<T, F>`): every write of one reports the new value here (`set`), so the record is
//! current at every point of the tick.
//!
//! Determinism: the game only looks records up by id. The map is a `BTreeMap` (no hash order); the walks
//! (`clear_player`, `check`) are in id order and observe nothing the game reads. A write is a plain store of the
//! C++ value (the `long`s as 64-bit values): no float operation, no random draw.

use std::collections::BTreeMap;
use std::sync::Mutex;

/// The record's fields that a holder reports (`EEUnitStoreFieldID` in ee/EEUnitStore.h: the same values).
pub const EE_UNIT_FIELD_TILE_X: u32 = 1;
pub const EE_UNIT_FIELD_TILE_Y: u32 = 2;
pub const EE_UNIT_FIELD_VALID: u32 = 3;
pub const EE_UNIT_FIELD_HIT_POINTS: u32 = 4;
/// `EEUnit::mUnitClass`: the value is the class's category, `EE_UNIT_FIELD_UNIT_TYPE` its type (both -1: none)
pub const EE_UNIT_FIELD_CATEGORY: u32 = 5;
pub const EE_UNIT_FIELD_UNIT_TYPE: u32 = 6;
/// `EEUnit::mComplexUnit`: `EE_UNIT_COMPLEX_*`
pub const EE_UNIT_FIELD_COMPLEX: u32 = 7;
pub const EE_UNIT_FIELD_BUILT: u32 = 8;
/// `EEComplexUnit::mGoalState`: the goal's `mKind`, -1 for none
pub const EE_UNIT_FIELD_GOAL_KIND: u32 = 9;
/// `EEUnit::mUnitClass` too: the class's `mDomains` (`kMovement*` bits) and whether it is a land building
/// (`EEUnitClass::IsLandBuilding`), both fixed by the class's constructor (0: no class)
pub const EE_UNIT_FIELD_DOMAINS: u32 = 10;
pub const EE_UNIT_FIELD_LAND_BUILDING: u32 = 11;
/// `EEComplexUnit::mIsCommandable`
pub const EE_UNIT_FIELD_COMMANDABLE: u32 = 12;
/// `EEComplexUnit::mFormationSlot` (the unit's place in its group's formation)
pub const EE_UNIT_FIELD_FORMATION_SLOT: u32 = 13;
/// `EEUnit::mUnitClass` too: the class's `mCanAttackGround`, fixed by its constructor (0: no class)
pub const EE_UNIT_FIELD_CAN_ATTACK_GROUND: u32 = 14;
/// `EEUnit::mResourceAmount` (what a resource has left)
pub const EE_UNIT_FIELD_RESOURCE_AMOUNT: u32 = 15;
/// on the map: linked into the world's unit chain (`gWorld.mUnits`, x-sorted, through `mTileNode`), reported by
/// `RTSUnit::Link` and `Unlink`; the domain of the out-of-sync checksum's unit walk
pub const EE_UNIT_FIELD_ON_MAP: u32 = 16;
/// `EEComplexUnit::mPhysicalModel.GetLocation()`'s x, y and z, as their float bits (`EEUnitStoreModel`'s setters)
pub const EE_UNIT_FIELD_LOCATION_X: u32 = 17;
pub const EE_UNIT_FIELD_LOCATION_Y: u32 = 18;
pub const EE_UNIT_FIELD_LOCATION_Z: u32 = 19;
/// `EEComplexUnit::mHeight` (float bits)
pub const EE_UNIT_FIELD_HEIGHT: u32 = 20;
/// `EEComplexUnit::mTimesUpdated`
pub const EE_UNIT_FIELD_TIMES_UPDATED: u32 = 21;
/// `EEComplexUnit::mActiveGroup != NULL`
pub const EE_UNIT_FIELD_ACTIVE_GROUP: u32 = 22;
/// `EEComplexUnit::mCommand`'s `mType` (0: none; an action's type is fixed by its constructor)
pub const EE_UNIT_FIELD_COMMAND: u32 = 23;
/// `EEComplexUnit::mReloadTimer`
pub const EE_UNIT_FIELD_RELOAD_TIMER: u32 = 24;
/// `EEComplexUnit::mTargetSearchTimer`
pub const EE_UNIT_FIELD_TARGET_SEARCH_TIMER: u32 = 25;
/// `EEComplexUnit::mHealSearchTimer` (float bits)
pub const EE_UNIT_FIELD_HEAL_SEARCH_TIMER: u32 = 26;
/// `EEComplexUnit::mNextTriggerTime` (an `unsigned long`)
pub const EE_UNIT_FIELD_NEXT_TRIGGER_TIME: u32 = 27;
/// `EEComplexUnit::mRottingAwayTime`
pub const EE_UNIT_FIELD_ROTTING_AWAY_TIME: u32 = 28;
/// `EEComplexUnit::mPower`
pub const EE_UNIT_FIELD_POWER: u32 = 29;
/// `EEComplexUnit::mPowerRegenTimer`
pub const EE_UNIT_FIELD_POWER_REGEN_TIMER: u32 = 30;
/// `EEComplexUnit::mCamouflageTimer` (an `unsigned long`)
pub const EE_UNIT_FIELD_CAMOUFLAGE_TIMER: u32 = 31;
/// `EEComplexUnit::mVelocity` (float bits)
pub const EE_UNIT_FIELD_VELOCITY: u32 = 32;
/// `EEComplexUnit::mPushVelocityX` (float bits)
pub const EE_UNIT_FIELD_PUSH_VELOCITY_X: u32 = 33;
/// `EEComplexUnit::mPushVelocityY` (float bits)
pub const EE_UNIT_FIELD_PUSH_VELOCITY_Y: u32 = 34;
/// `EEComplexUnit::mRoll` (float bits)
pub const EE_UNIT_FIELD_ROLL: u32 = 35;

/// `EEUnit::mComplexUnit`: NULL, the unit itself, another object (never seen: `Init` sets it to the unit itself,
/// `ResetEEUnit` clears it)
pub const EE_UNIT_COMPLEX_NONE: u8 = 0;
pub const EE_UNIT_COMPLEX_SELF: u8 = 1;
pub const EE_UNIT_COMPLEX_OTHER: u8 = 2;

/// One unit's plain state. Fields an object of its kind does not have (an `RTSUnit`'s hit points, an `EEUnit`'s
/// built flag and goal) are 0 / -1.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub struct EeUnitRecord {
    /// `RTSUnit::mUnitID`, the key
    pub id: u32,
    /// the player id of the list the unit is in (`RTSPlayer::mPlayerID` at the insert)
    pub owner: u32,
    /// the object's address: a write from an object that is not the one inserted under its id is ignored
    pub object: u64,
    /// the player object whose list holds it (`~RTSPlayer` drops its records by the object: temporary players of
    /// the editor's forms may share an id)
    pub player: u64,
    /// `RTSUnit::mObjectKind` (ee_units' `EE_OBJECT_*`)
    pub object_kind: u8,
    /// `EE_UNIT_COMPLEX_*`
    pub complex: u8,
    /// `RTSUnit::mIsValid`
    pub valid: u8,
    /// `EEComplexUnit::mIsBuilt`
    pub built: u8,
    /// `EEComplexUnit::mGoalState->mKind`, -1 for none
    pub goal_kind: i32,
    /// `EEUnit::mHitPoints`
    pub hit_points: i64,
    /// `RTSUnit::mTileX`, `mTileY`
    pub tile_x: i64,
    pub tile_y: i64,
    /// `EEUnit::mUnitClass->mCategory`, `->mUnitType` (-1: no class)
    pub category: i64,
    pub unit_type: i64,
    /// `EEUnit::mUnitClass->mDomains`, `->IsLandBuilding()` (0: no class)
    pub domains: u8,
    pub land_building: u8,
    /// `EEUnit::mUnitClass->mCanAttackGround` (0: no class)
    pub can_attack_ground: u8,
    /// `EEComplexUnit::mIsCommandable`
    pub commandable: u8,
    /// `EEComplexUnit::mFormationSlot` (an `unsigned long`)
    pub formation_slot: i64,
    /// `EEUnit::mResourceAmount`
    pub resource_amount: i64,
    /// in the world's unit chain (`EE_UNIT_FIELD_ON_MAP`)
    pub on_map: u8,
    /// `EEComplexUnit::mActiveGroup != NULL`
    pub active_group: u8,
    /// the location's x, y, z and `mHeight` as float bits
    pub location: [u32; 3],
    pub height: u32,
    /// `EEComplexUnit::mTimesUpdated` (an `unsigned long`)
    pub times_updated: i64,
    /// `mCommand ? mCommand->mType : 0` (an `unsigned long`)
    pub command: i64,
    /// `EEComplexUnit::mReloadTimer` (the step's leaves read it: `step`)
    pub reload_timer: i64,
    /// `EEComplexUnit::mTargetSearchTimer` (the step's leaves read it: `step`)
    pub target_search_timer: i64,
    /// `EEComplexUnit::mHealSearchTimer` as float bits (the step's leaves read it: `step`)
    pub heal_search_timer: u32,
    /// `EEComplexUnit::mNextTriggerTime` (the step's leaves read it: `step`)
    pub next_trigger_time: i64,
    /// `EEComplexUnit::mRottingAwayTime` (the step's leaves read it: `step`)
    pub rotting_away_time: i64,
    /// `EEComplexUnit::mPower` (the step's leaves read it: `step`)
    pub power: i64,
    /// `EEComplexUnit::mPowerRegenTimer` (the step's leaves read it: `step`)
    pub power_regen_timer: i64,
    /// `EEComplexUnit::mCamouflageTimer` (the step's leaves read it: `step`)
    pub camouflage_timer: i64,
    /// `EEComplexUnit::mVelocity` as float bits (the physics models' arithmetic reads it: `step`)
    pub velocity: u32,
    /// `EEComplexUnit::mPushVelocityX` as float bits (the physics models' arithmetic reads it: `step`)
    pub push_velocity_x: u32,
    /// `EEComplexUnit::mPushVelocityY` as float bits (the physics models' arithmetic reads it: `step`)
    pub push_velocity_y: u32,
    /// `EEComplexUnit::mRoll` as float bits (the physics models' arithmetic reads it: `step`)
    pub roll: u32,
}

impl EeUnitRecord {
    /// Writes one reported field. False for a field this record does not know.
    pub fn set(&mut self, field: u32, value: i64) -> bool {
        match field {
            EE_UNIT_FIELD_TILE_X => self.tile_x = value,
            EE_UNIT_FIELD_TILE_Y => self.tile_y = value,
            EE_UNIT_FIELD_VALID => self.valid = value as u8,
            EE_UNIT_FIELD_HIT_POINTS => self.hit_points = value,
            EE_UNIT_FIELD_CATEGORY => self.category = value,
            EE_UNIT_FIELD_UNIT_TYPE => self.unit_type = value,
            EE_UNIT_FIELD_COMPLEX => self.complex = value as u8,
            EE_UNIT_FIELD_BUILT => self.built = value as u8,
            EE_UNIT_FIELD_GOAL_KIND => self.goal_kind = value as i32,
            EE_UNIT_FIELD_DOMAINS => self.domains = value as u8,
            EE_UNIT_FIELD_LAND_BUILDING => self.land_building = value as u8,
            EE_UNIT_FIELD_COMMANDABLE => self.commandable = value as u8,
            EE_UNIT_FIELD_FORMATION_SLOT => self.formation_slot = value,
            EE_UNIT_FIELD_CAN_ATTACK_GROUND => self.can_attack_ground = value as u8,
            EE_UNIT_FIELD_RESOURCE_AMOUNT => self.resource_amount = value,
            EE_UNIT_FIELD_ON_MAP => self.on_map = value as u8,
            EE_UNIT_FIELD_LOCATION_X => self.location[0] = value as u32,
            EE_UNIT_FIELD_LOCATION_Y => self.location[1] = value as u32,
            EE_UNIT_FIELD_LOCATION_Z => self.location[2] = value as u32,
            EE_UNIT_FIELD_HEIGHT => self.height = value as u32,
            EE_UNIT_FIELD_TIMES_UPDATED => self.times_updated = value,
            EE_UNIT_FIELD_ACTIVE_GROUP => self.active_group = value as u8,
            EE_UNIT_FIELD_COMMAND => self.command = value,
            EE_UNIT_FIELD_RELOAD_TIMER => self.reload_timer = value,
            EE_UNIT_FIELD_TARGET_SEARCH_TIMER => self.target_search_timer = value,
            EE_UNIT_FIELD_HEAL_SEARCH_TIMER => self.heal_search_timer = value as u32,
            EE_UNIT_FIELD_NEXT_TRIGGER_TIME => self.next_trigger_time = value,
            EE_UNIT_FIELD_ROTTING_AWAY_TIME => self.rotting_away_time = value,
            EE_UNIT_FIELD_POWER => self.power = value,
            EE_UNIT_FIELD_POWER_REGEN_TIMER => self.power_regen_timer = value,
            EE_UNIT_FIELD_CAMOUFLAGE_TIMER => self.camouflage_timer = value,
            EE_UNIT_FIELD_VELOCITY => self.velocity = value as u32,
            EE_UNIT_FIELD_PUSH_VELOCITY_X => self.push_velocity_x = value as u32,
            EE_UNIT_FIELD_PUSH_VELOCITY_Y => self.push_velocity_y = value as u32,
            EE_UNIT_FIELD_ROLL => self.roll = value as u32,
            _ => return false,
        }
        true
    }

    /// This record's part of the checksum's flag 2 (`Store::checksum_flag2`).
    pub fn flag2(&self) -> i64 {
        let bits = |b: u32| i64::from(b as i32);
        self.times_updated
            .wrapping_add(i64::from(self.active_group))
            .wrapping_add(self.command)
            .wrapping_add(bits(self.location[0]))
            .wrapping_add(bits(self.location[1]))
            .wrapping_add(bits(self.location[2]))
            .wrapping_add(bits(self.height))
    }

    /// The first field (by `EE_UNIT_FIELD_*`, 0 for the key fields) where two records differ.
    pub fn first_difference(&self, other: &EeUnitRecord) -> Option<u32> {
        if self.id != other.id
            || self.owner != other.owner
            || self.object != other.object
            || self.player != other.player
            || self.object_kind != other.object_kind
        {
            return Some(0);
        }
        let pairs = [
            (EE_UNIT_FIELD_TILE_X, self.tile_x == other.tile_x),
            (EE_UNIT_FIELD_TILE_Y, self.tile_y == other.tile_y),
            (EE_UNIT_FIELD_VALID, self.valid == other.valid),
            (EE_UNIT_FIELD_HIT_POINTS, self.hit_points == other.hit_points),
            (EE_UNIT_FIELD_CATEGORY, self.category == other.category),
            (EE_UNIT_FIELD_UNIT_TYPE, self.unit_type == other.unit_type),
            (EE_UNIT_FIELD_COMPLEX, self.complex == other.complex),
            (EE_UNIT_FIELD_BUILT, self.built == other.built),
            (EE_UNIT_FIELD_GOAL_KIND, self.goal_kind == other.goal_kind),
            (EE_UNIT_FIELD_DOMAINS, self.domains == other.domains),
            (EE_UNIT_FIELD_LAND_BUILDING, self.land_building == other.land_building),
            (EE_UNIT_FIELD_COMMANDABLE, self.commandable == other.commandable),
            (EE_UNIT_FIELD_FORMATION_SLOT, self.formation_slot == other.formation_slot),
            (EE_UNIT_FIELD_CAN_ATTACK_GROUND, self.can_attack_ground == other.can_attack_ground),
            (EE_UNIT_FIELD_RESOURCE_AMOUNT, self.resource_amount == other.resource_amount),
            (EE_UNIT_FIELD_ON_MAP, self.on_map == other.on_map),
            (EE_UNIT_FIELD_LOCATION_X, self.location[0] == other.location[0]),
            (EE_UNIT_FIELD_LOCATION_Y, self.location[1] == other.location[1]),
            (EE_UNIT_FIELD_LOCATION_Z, self.location[2] == other.location[2]),
            (EE_UNIT_FIELD_HEIGHT, self.height == other.height),
            (EE_UNIT_FIELD_TIMES_UPDATED, self.times_updated == other.times_updated),
            (EE_UNIT_FIELD_ACTIVE_GROUP, self.active_group == other.active_group),
            (EE_UNIT_FIELD_COMMAND, self.command == other.command),
            (EE_UNIT_FIELD_RELOAD_TIMER, self.reload_timer == other.reload_timer),
            (EE_UNIT_FIELD_TARGET_SEARCH_TIMER, self.target_search_timer == other.target_search_timer),
            (EE_UNIT_FIELD_HEAL_SEARCH_TIMER, self.heal_search_timer == other.heal_search_timer),
            (EE_UNIT_FIELD_NEXT_TRIGGER_TIME, self.next_trigger_time == other.next_trigger_time),
            (EE_UNIT_FIELD_ROTTING_AWAY_TIME, self.rotting_away_time == other.rotting_away_time),
            (EE_UNIT_FIELD_POWER, self.power == other.power),
            (EE_UNIT_FIELD_POWER_REGEN_TIMER, self.power_regen_timer == other.power_regen_timer),
            (EE_UNIT_FIELD_CAMOUFLAGE_TIMER, self.camouflage_timer == other.camouflage_timer),
            (EE_UNIT_FIELD_VELOCITY, self.velocity == other.velocity),
            (EE_UNIT_FIELD_PUSH_VELOCITY_X, self.push_velocity_x == other.push_velocity_x),
            (EE_UNIT_FIELD_PUSH_VELOCITY_Y, self.push_velocity_y == other.push_velocity_y),
            (EE_UNIT_FIELD_ROLL, self.roll == other.roll),
        ];
        pairs.iter().find(|(_, same)| !same).map(|(f, _)| *f)
    }
}

/// What a decision reads: a unit's record by id. The store implements it; tests use plain maps.
pub trait UnitView {
    fn unit(&self, id: u32) -> Option<&EeUnitRecord>;
}

impl UnitView for BTreeMap<u32, EeUnitRecord> {
    fn unit(&self, id: u32) -> Option<&EeUnitRecord> {
        self.get(&id)
    }
}

/// The result of a consistency check (`Store::check`).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub struct EeUnitStoreCheck {
    /// the objects compared
    pub compared: u32,
    /// the objects whose record is missing or differs, plus the records no object named
    pub mismatches: u32,
    /// the records in the store
    pub records: u32,
    /// the first mismatch: its id (0xffffffff: none, or a record no object named) and field (`first_difference`;
    /// 0xff: no record)
    pub first_id: u32,
    pub first_field: u32,
}

/// The records by id.
#[derive(Default)]
pub struct Store {
    records: BTreeMap<u32, EeUnitRecord>,
}

impl UnitView for Store {
    fn unit(&self, id: u32) -> Option<&EeUnitRecord> {
        self.records.get(&id)
    }
}

impl Store {
    pub const fn new() -> Self {
        Store { records: BTreeMap::new() }
    }

    /// A unit entering a player's list: its record replaces any under its id.
    pub fn insert(&mut self, record: EeUnitRecord) {
        self.records.insert(record.id, record);
    }

    /// A unit leaving its player's list; only the record of that object. True when there was one.
    pub fn remove(&mut self, id: u32, object: u64) -> bool {
        if self.records.get(&id).is_some_and(|r| r.object == object) {
            self.records.remove(&id);
            true
        } else {
            false
        }
    }

    /// A player object goes: every record of its list goes. The count removed.
    pub fn clear_player(&mut self, player: u64) -> usize {
        let before = self.records.len();
        self.records.retain(|_, r| r.player != player);
        before - self.records.len()
    }

    /// Everything (the world's teardown).
    pub fn clear(&mut self) {
        self.records.clear();
    }

    /// A holder's write. Ignored when no record of that object is under the id (a pending unit, a removed one).
    pub fn set(&mut self, id: u32, object: u64, field: u32, value: i64) -> bool {
        match self.records.get_mut(&id) {
            Some(r) if r.object == object => r.set(field, value),
            _ => false,
        }
    }

    pub fn get(&self, id: u32) -> Option<&EeUnitRecord> {
        self.records.get(&id)
    }

    pub fn len(&self) -> usize {
        self.records.len()
    }

    pub fn is_empty(&self) -> bool {
        self.records.is_empty()
    }

    /// The out-of-sync checksum's flag 1 (`EEComplexUnit::GetChecksum`: hit points + tile y + tile x; 0 for a unit
    /// that is not complex) summed over the units on the map, wrapping (the C++ `long`s and `unsigned long` sum:
    /// the low 32 bits equal on every data model, all 64 where `long` has 64). The sum commutes, so the records' id
    /// order gives the chain walk's value.
    pub fn checksum_flag1(&self) -> u64 {
        self.records
            .values()
            .filter(|r| r.on_map != 0 && r.object_kind == crate::kinds::EE_OBJECT_COMPLEX_UNIT)
            .fold(0u64, |sum, r| sum.wrapping_add(r.hit_points.wrapping_add(r.tile_y).wrapping_add(r.tile_x) as u64))
    }

    /// The out-of-sync checksum's flag 2 (`EEComplexUnit::GetChecksum`: the times updated, whether the unit has an
    /// active group, its command's type, the location's x, y, z and the height as `int32` bits, each sign-extended
    /// into the `long` sum) over the complex units on the map, wrapping like `checksum_flag1`. The C++ reads these on
    /// `mComplexUnit`, which is the unit itself for every unit on the map (USCK checks the sum against the chain's).
    pub fn checksum_flag2(&self) -> u64 {
        self.records
            .values()
            .filter(|r| r.on_map != 0 && r.object_kind == crate::kinds::EE_OBJECT_COMPLEX_UNIT)
            .fold(0u64, |sum, r| sum.wrapping_add(r.flag2() as u64))
    }

    /// TEST ONLY: the world's unit chain (its units' ids and objects, in the chain's order) against the records'
    /// on-map flags: the chain's units with no record of that object or one not on the map, plus the records on the
    /// map the chain does not hold. 0: the two domains are the same.
    pub fn check_on_map(&self, chain: &[(u32, u64)]) -> u32 {
        let mut mismatches = 0u32;
        let mut found = 0usize;
        for &(id, object) in chain {
            match self.records.get(&id) {
                Some(r) if r.object == object && r.on_map != 0 => found += 1,
                _ => mismatches += 1,
            }
        }
        let on_map = self.records.values().filter(|r| r.on_map != 0).count();
        mismatches + on_map.saturating_sub(found) as u32
    }

    /// TEST ONLY: compares every object's fields (filled by C++ from every player's list) with the records; the
    /// records no object named count as mismatches too.
    pub fn check(&self, objects: &[EeUnitRecord]) -> EeUnitStoreCheck {
        let mut out = EeUnitStoreCheck {
            compared: objects.len() as u32,
            records: self.records.len() as u32,
            first_id: u32::MAX,
            first_field: 0,
            ..Default::default()
        };
        let mut named = 0usize;
        for o in objects {
            let field = match self.records.get(&o.id) {
                Some(r) => {
                    named += 1;
                    r.first_difference(o)
                }
                None => Some(0xff),
            };
            if let Some(f) = field {
                if out.mismatches == 0 {
                    out.first_id = o.id;
                    out.first_field = f;
                }
                out.mismatches += 1;
            }
        }
        // a unit listed twice names its record twice: then the counts do not tell, the duplicate is a mismatch of
        // its own only if its fields differ (the C++ check also compares the list counts)
        if named < self.records.len() {
            if out.mismatches == 0 {
                out.first_id = u32::MAX;
                out.first_field = 0xff;
            }
            out.mismatches += (self.records.len() - named) as u32;
        }
        out
    }
}

/// The process's store. Written under the world's write lock (the physics tick, the loads, the editor); the mutex
/// keeps a read from another thread whole.
pub static STORE: Mutex<Store> = Mutex::new(Store::new());

/// Runs `f` over the process's store (a poisoned lock is taken over: the store holds plain values only).
pub fn with_store<R>(f: impl FnOnce(&mut Store) -> R) -> R {
    let mut guard = STORE.lock().unwrap_or_else(|e| e.into_inner());
    f(&mut guard)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn rec(id: u32, owner: u32) -> EeUnitRecord {
        EeUnitRecord {
            id,
            owner,
            object: 0x1000 + id as u64,
            player: 0x5000 + owner as u64,
            object_kind: 2,
            complex: EE_UNIT_COMPLEX_SELF,
            valid: 1,
            built: 1,
            goal_kind: -1,
            hit_points: 100,
            tile_x: 5,
            tile_y: 6,
            category: 1,
            unit_type: 42,
            domains: 2,
            land_building: 0,
            can_attack_ground: 1,
            commandable: 1,
            formation_slot: 3,
            resource_amount: 0,
            on_map: 1,
            active_group: 0,
            location: [0; 3],
            height: 0,
            times_updated: 0,
            command: 0,
            reload_timer: 0,
            target_search_timer: 0,
            heal_search_timer: 0,
            next_trigger_time: 0,
            rotting_away_time: 0,
            power: 0,
            power_regen_timer: 0,
            camouflage_timer: 0,
            velocity: 0,
            push_velocity_x: 0,
            push_velocity_y: 0,
            roll: 0,
        }
    }

    #[test]
    fn insert_set_remove() {
        let mut s = Store::new();
        s.insert(rec(0x0100_0001, 1));
        assert!(s.set(0x0100_0001, 0x1000 + 0x0100_0001, EE_UNIT_FIELD_HIT_POINTS, -3));
        assert_eq!(s.get(0x0100_0001).unwrap().hit_points, -3);
        // another object under the same id: ignored
        assert!(!s.set(0x0100_0001, 0x9999, EE_UNIT_FIELD_HIT_POINTS, 7));
        assert_eq!(s.get(0x0100_0001).unwrap().hit_points, -3);
        // an unknown id or field
        assert!(!s.set(0x0100_0002, 0, EE_UNIT_FIELD_TILE_X, 1));
        assert!(!s.set(0x0100_0001, 0x1000 + 0x0100_0001, 99, 1));
        assert!(!s.remove(0x0100_0001, 0x9999));
        assert!(s.remove(0x0100_0001, 0x1000 + 0x0100_0001));
        assert!(s.is_empty());
    }

    #[test]
    fn every_field_round_trips() {
        let mut r = rec(1, 0);
        for (f, v) in [
            (EE_UNIT_FIELD_TILE_X, -10000i64),
            (EE_UNIT_FIELD_TILE_Y, 255),
            (EE_UNIT_FIELD_VALID, 0),
            (EE_UNIT_FIELD_HIT_POINTS, i64::from(i32::MIN)),
            (EE_UNIT_FIELD_CATEGORY, -1),
            (EE_UNIT_FIELD_UNIT_TYPE, 0xffff_ffff),
            (EE_UNIT_FIELD_COMPLEX, EE_UNIT_COMPLEX_NONE as i64),
            (EE_UNIT_FIELD_BUILT, 0),
            (EE_UNIT_FIELD_GOAL_KIND, 15),
            (EE_UNIT_FIELD_DOMAINS, 5),
            (EE_UNIT_FIELD_LAND_BUILDING, 1),
            (EE_UNIT_FIELD_COMMANDABLE, 0),
            (EE_UNIT_FIELD_FORMATION_SLOT, 0xffff_ffff),
            (EE_UNIT_FIELD_CAN_ATTACK_GROUND, 0),
            (EE_UNIT_FIELD_RESOURCE_AMOUNT, 1500),
            (EE_UNIT_FIELD_ON_MAP, 0),
            (EE_UNIT_FIELD_LOCATION_X, i64::from(1.5f32.to_bits())),
            (EE_UNIT_FIELD_LOCATION_Y, i64::from((-2.0f32).to_bits())),
            (EE_UNIT_FIELD_LOCATION_Z, 3),
            (EE_UNIT_FIELD_HEIGHT, 4),
            (EE_UNIT_FIELD_TIMES_UPDATED, 0xffff_ffff),
            (EE_UNIT_FIELD_ACTIVE_GROUP, 1),
            (EE_UNIT_FIELD_COMMAND, 5000),
            (EE_UNIT_FIELD_RELOAD_TIMER, 2500),
            (EE_UNIT_FIELD_TARGET_SEARCH_TIMER, -1),
            (EE_UNIT_FIELD_HEAL_SEARCH_TIMER, i64::from(0.5f32.to_bits())),
            (EE_UNIT_FIELD_NEXT_TRIGGER_TIME, 0xffff_ffff),
            (EE_UNIT_FIELD_ROTTING_AWAY_TIME, 9000),
            (EE_UNIT_FIELD_POWER, 77),
            (EE_UNIT_FIELD_POWER_REGEN_TIMER, 5000),
            (EE_UNIT_FIELD_CAMOUFLAGE_TIMER, 0x8000_0100),
            (EE_UNIT_FIELD_VELOCITY, i64::from(2.5f32.to_bits())),
            (EE_UNIT_FIELD_PUSH_VELOCITY_X, i64::from((-0.25f32).to_bits())),
            (EE_UNIT_FIELD_PUSH_VELOCITY_Y, i64::from(0.125f32.to_bits())),
            (EE_UNIT_FIELD_ROLL, i64::from(0.3f32.to_bits())),
        ] {
            let before = r;
            assert!(r.set(f, v));
            assert_eq!(before.first_difference(&r), Some(f), "field {f}");
        }
        assert_eq!(r.tile_x, -10000);
        assert_eq!(r.unit_type, 0xffff_ffff);
        assert_eq!(r.goal_kind, 15);
        assert_eq!((r.domains, r.land_building), (5, 1));
        assert_eq!((r.commandable, r.formation_slot, r.can_attack_ground), (0, 0xffff_ffff, 0));
        let mut k = r;
        k.owner = 3;
        assert_eq!(r.first_difference(&k), Some(0));
        assert_eq!(r.first_difference(&r), None);
    }

    #[test]
    fn clear_player_keeps_the_others() {
        let mut s = Store::new();
        for i in 0..10 {
            s.insert(rec(i, i % 3));
        }
        // another player object with the same id: its records only
        let mut twin = rec(20, 1);
        twin.player = 0x7777;
        s.insert(twin);
        assert_eq!(s.clear_player(0x5001), 3);
        assert_eq!(s.len(), 8);
        assert!((0..10).all(|i| s.get(i).is_none_or(|r| r.owner != 1)));
        assert!(s.get(20).is_some());
        s.clear();
        assert!(s.is_empty());
    }

    #[test]
    fn check_counts_differences_missing_and_stale_records() {
        let mut s = Store::new();
        let objects: Vec<_> = (0..5).map(|i| rec(i, 1)).collect();
        for o in &objects {
            s.insert(*o);
        }
        let c = s.check(&objects);
        assert_eq!((c.compared, c.mismatches, c.records, c.first_id), (5, 0, 5, u32::MAX));
        // a field differs
        let mut changed = objects.clone();
        changed[2].built = 0;
        let c = s.check(&changed);
        assert_eq!((c.mismatches, c.first_id, c.first_field), (1, 2, EE_UNIT_FIELD_BUILT));
        // an object without a record
        let mut more = objects.clone();
        more.push(rec(9, 1));
        let c = s.check(&more);
        assert_eq!((c.mismatches, c.first_id, c.first_field), (1, 9, 0xff));
        // a record no object names
        let c = s.check(&objects[..4]);
        assert_eq!((c.mismatches, c.first_id, c.first_field), (1, u32::MAX, 0xff));
    }

    #[test]
    fn the_checksum_flag1_sums_the_complex_units_on_the_map() {
        let mut s = Store::new();
        s.insert(EeUnitRecord { hit_points: 100, tile_x: 5, tile_y: 6, ..rec(1, 1) });
        s.insert(EeUnitRecord { hit_points: -3, tile_x: -10000, tile_y: -10000, ..rec(2, 1) });
        // not on the map, not complex: not summed
        s.insert(EeUnitRecord { on_map: 0, hit_points: 7, ..rec(3, 1) });
        s.insert(EeUnitRecord { object_kind: crate::kinds::EE_OBJECT_UNIT, hit_points: 9, ..rec(4, 0) });
        let sum = s.checksum_flag1();
        assert_eq!(sum, (111i64 + (-20003i64)) as u64);
        assert_eq!(sum as u32, (111i32).wrapping_add(-20003) as u32);
        // a wrap in 32 bits gives the same low word
        s.insert(EeUnitRecord { hit_points: i64::from(i32::MAX), tile_x: 1, tile_y: 0, ..rec(5, 1) });
        let low = s.checksum_flag1() as u32;
        assert_eq!(low, (111i32).wrapping_add(-20003).wrapping_add(i32::MAX).wrapping_add(1) as u32);
    }

    #[test]
    fn the_checksum_flag2_sums_the_complex_units_on_the_map() {
        let mut s = Store::new();
        let neg = (-2.0f32).to_bits();
        s.insert(EeUnitRecord {
            times_updated: 3,
            active_group: 1,
            command: 5000,
            location: [1.5f32.to_bits(), neg, 0],
            height: 0.25f32.to_bits(),
            ..rec(1, 1)
        });
        // not on the map, not complex: not summed
        s.insert(EeUnitRecord { on_map: 0, command: 7, ..rec(3, 1) });
        s.insert(EeUnitRecord { object_kind: crate::kinds::EE_OBJECT_UNIT, command: 9, ..rec(4, 0) });
        let words = [3i32, 1, 5000, 1.5f32.to_bits() as i32, neg as i32, 0, 0.25f32.to_bits() as i32];
        // the 32-bit sum (Windows' long) is the low word; the 64-bit one sign-extends each int32
        let low = words.iter().fold(0i32, |a, &w| a.wrapping_add(w));
        let wide = words.iter().fold(0i64, |a, &w| a.wrapping_add(i64::from(w)));
        assert_eq!(s.checksum_flag2() as u32, low as u32);
        assert_eq!(s.checksum_flag2(), wide as u64);
    }

    #[test]
    fn check_on_map_names_both_differences() {
        let mut s = Store::new();
        for i in 0..4 {
            s.insert(rec(i, 1));
        }
        s.insert(EeUnitRecord { on_map: 0, ..rec(9, 1) });
        let chain: Vec<_> = (0..4).map(|i| (i, 0x1000 + i as u64)).collect();
        assert_eq!(s.check_on_map(&chain), 0);
        // a record on the map the chain lacks
        assert_eq!(s.check_on_map(&chain[..3]), 1);
        // a chain unit off the map in the store, and one of another object
        let mut more = chain.clone();
        more.push((9, 0x1009));
        assert_eq!(s.check_on_map(&more), 1);
        more[0].1 = 0x7777;
        assert_eq!(s.check_on_map(&more), 3);
        assert_eq!(s.check_on_map(&[]), 4);
    }

    #[test]
    fn the_process_store_is_reachable() {
        // an id no other test of this crate uses (the static is shared by the test binary's threads)
        let id = 0xfd00_0001;
        with_store(|s| s.insert(rec(id, 0xfd)));
        assert_eq!(with_store(|s| s.get(id).copied()).unwrap().unit_type, 42);
        assert_eq!(with_store(|s| s.clear_player(0x5000 + 0xfd)), 1);
    }
}
