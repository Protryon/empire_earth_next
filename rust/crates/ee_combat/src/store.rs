//! The combat reads' view of the core lane's unit store (R5b.2, ninth round): a unit's plain fields by its object (the
//! `EEUnit*` every combat handle is) and a player's diplomacy table by its object, through ee_rust's bridges
//! (`ee_combat_store_unit_of`, `ee_combat_store_player_of`: ee_combat cannot depend on ee_units, whose crate imports
//! C++ functions no test binary of this crate has).
//!
//! A unit in a player's list has its store record (written through at every write of a field it mirrors, so it equals
//! the object at any point); for any other unit (one out of every list: a killed unit a goal still names) the bridge
//! fills the same record from the object (the store's own C++ fill), so a read of a unit never needs the reads' host.
//! The same holds for a player object without a table.
//!
//! The fills of the goals' records (`EECombatFillCondition`, `EECombatFillStep`, the heal goal's work) are here too: every field the store
//! holds, read from it; the C++ adds the rest (the stats, which upgrades change in place, and `IsHuman`, a virtual).
//!
//! Determinism: plain copies of the C++ values; no float operation, no random draw.

use crate::goals::{EeCombatConditionIn, EeCombatStepIn};
use crate::shots::EeCombatHealWorkIn;
use crate::walks::EeCombatHandle;

/// The unit store's fields the combat reads take (ee_units' `EeUnitRecord`'s, copied by ee_rust's bridge).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeCombatStoredUnit {
    /// the `RTSUnit*` the record is of (the `EEUnit*`'s address)
    pub object: u64,
    /// `mPlayer` (the player object of the list that holds the unit: `RTSPlayer::InsertUnit` sets both)
    pub player: u64,
    /// `RTSUnit::mUnitID`
    pub id: u32,
    /// ee_units' `EE_OBJECT_*` (`EE_COMBAT_STORED_COMPLEX_UNIT`: an `EEComplexUnit`)
    pub object_kind: u8,
    /// `mComplexUnit`: `EE_COMBAT_STORED_COMPLEX_*`
    pub complex: u8,
    pub built: u8,
    /// `mIsCloakable`, `mIsCloaked`, `mHerd != NULL`, `mContainer != NULL`
    pub cloakable: u8,
    pub cloaked: u8,
    pub herd: u8,
    pub container: u8,
    /// the class's `IsCrusader()`, `mIsParatrooperBomber`, `EE_COMBAT_CLASS_*` flags, `mDomains`, `IsLandBuilding()`
    /// (0: no class)
    pub crusader: u8,
    pub paratrooper_bomber: u8,
    pub class_flags: u8,
    pub domains: u8,
    pub land_building: u8,
    /// `mGoalState->mKind`, -1 for none
    pub goal_kind: i32,
    pub tile_x: i64,
    pub tile_y: i64,
    pub hit_points: i64,
    /// -1: no class
    pub unit_type: i64,
    pub category: i64,
    /// `mCommand ? mCommand->mType : 0`
    pub command: i64,
    /// the location's x, y, z as float bits
    pub location: [u32; 3],
    /// `mVelocity`, the class's `GetHealThreshold()` (0: no class) as float bits
    pub velocity: u32,
    pub heal_threshold: u32,
    pub target_search_timer: i64,
    pub reload_timer: i64,
    pub rotting_away_time: i64,
    pub flight_time: i64,
    /// `mCloaked` (the cloak's timer, an `unsigned long`)
    pub cloak_timer: i64,
    /// the class's `mFamily`, `mCloakRadius` (0: no class)
    pub family: i64,
    pub cloak_radius: i64,
    /// `EEUnit::mResourceAmount`
    pub resource_amount: i64,
    /// `mStats` (the `EEUnitClassUpgrade*`'s address, 0: none): the stats table's key ([`stats_of`])
    pub stats: u64,
}

/// A stats object's fields the combat reads take (ee_units stats.rs's `EeUnitStatsRecord`'s, copied by ee_rust's
/// bridge; floats as their bits).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeCombatStoredStats {
    /// `mScale`, `mRange`, `mMaxVelocity`, `mAttack`
    pub scale: u32,
    pub range: u32,
    pub max_velocity: u32,
    pub attack: u32,
    /// `mFlightTime`, `mReloadTime`, `mMaxHitPoints`, `mHealingPoints`
    pub flight_time: i64,
    pub reload_time: i64,
    pub max_hit_points: i64,
    pub healing_points: i64,
    /// `mArmor[6]` (float bits)
    pub armor: [u32; 6],
    /// `mCanFireOverWalls`
    pub can_fire_over_walls: u8,
}

/// ee_units' `EE_OBJECT_COMPLEX_UNIT`.
pub const EE_COMBAT_STORED_COMPLEX_UNIT: u8 = 2;
/// ee_units' `EE_UNIT_COMPLEX_*`: no complex unit, the unit itself, another object.
pub const EE_COMBAT_STORED_COMPLEX_NONE: u8 = 0;
pub const EE_COMBAT_STORED_COMPLEX_SELF: u8 = 1;
pub const EE_COMBAT_STORED_COMPLEX_OTHER: u8 = 2;

/// A player object's diplomacy table: `mPlayerID` and `mEnemies` (the 17 ids).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeCombatStoredPlayer {
    pub id: u32,
    pub enemies: [u32; 17],
}

/// ee_rust defines the bridges (and declares them in its header): cbindgen must not declare them here again.
/// cbindgen:ignore
#[cfg(not(test))]
mod bridge {
    use super::{EeCombatStoredPlayer, EeCombatStoredStats, EeCombatStoredUnit};
    extern "C" {
        /// The stats table's record of the stats object into `out`; false for none.
        pub fn ee_combat_store_stats_of(object: u64, out: *mut EeCombatStoredStats) -> bool;
        /// The record of the unit object into `out` (the store's, else filled from the object); false for none.
        pub fn ee_combat_store_unit_of(object: u64, out: *mut EeCombatStoredUnit) -> bool;
        /// The table of the player object into `out` (the store's, else read from the object); false for none.
        pub fn ee_combat_store_player_of(object: u64, out: *mut EeCombatStoredPlayer) -> bool;
    }
}

/// The record of a unit object (None for 0; in the unit tests their own map).
pub fn unit_of(object: EeCombatHandle) -> Option<EeCombatStoredUnit> {
    if object == 0 {
        return None;
    }
    #[cfg(not(test))]
    {
        let mut out = EeCombatStoredUnit::default();
        // SAFETY: the bridge writes one record into `out`
        unsafe { bridge::ee_combat_store_unit_of(object, &mut out) }.then_some(out)
    }
    #[cfg(test)]
    {
        tests::unit(object)
    }
}

/// The table of a player object (None for 0; in the unit tests their own map).
pub fn player_of(object: EeCombatHandle) -> Option<EeCombatStoredPlayer> {
    if object == 0 {
        return None;
    }
    #[cfg(not(test))]
    {
        let mut out = EeCombatStoredPlayer::default();
        // SAFETY: the bridge writes one table into `out`
        unsafe { bridge::ee_combat_store_player_of(object, &mut out) }.then_some(out)
    }
    #[cfg(test)]
    {
        tests::player(object)
    }
}

/// The stats table's record of a stats object (a unit record's `stats`; None for 0 or one the table lacks; in the
/// unit tests their own map).
pub fn stats_of(object: u64) -> Option<EeCombatStoredStats> {
    if object == 0 {
        return None;
    }
    #[cfg(not(test))]
    {
        let mut out = EeCombatStoredStats::default();
        // SAFETY: the bridge writes one record into `out`
        unsafe { bridge::ee_combat_store_stats_of(object, &mut out) }.then_some(out)
    }
    #[cfg(test)]
    {
        tests::stats(object)
    }
}

/// The stats of a unit object (`GetStats()`): its record's stats object's fields (None without either).
pub fn unit_stats(object: EeCombatHandle) -> Option<EeCombatStoredStats> {
    unit_of(object).and_then(|r| stats_of(r.stats))
}

/// `mEnemies[other] != 0` of the player object (false without its table).
fn at_war(player: EeCombatHandle, other: u32) -> bool {
    player_of(player).and_then(|p| p.enemies.get(other as usize).copied()).is_some_and(|e| e != 0)
}

/// `EECombatFillCondition`'s store part: the record zeroed, then the unit's and the target's (`target`; its complex
/// unit `complex`, 0 for none) fields in the original's order, the unit's and the target's stats from the stats table
/// (0 without a stats object). The C++ adds `human`.
pub fn fill_condition(
    i: &mut EeCombatConditionIn,
    kind: i32,
    unit: EeCombatHandle,
    event: u32,
    target: EeCombatHandle,
    complex: EeCombatHandle,
) {
    *i = EeCombatConditionIn { kind, event, ..Default::default() };
    let u = unit_of(unit).unwrap_or_default();
    i.unit_id = u.id;
    i.goal_kind = u.goal_kind;
    i.reload_timer = u.reload_timer as i32;
    i.herd = i32::from(u.herd);
    i.crusader = i32::from(u.crusader);
    i.paratrooper_bomber = i32::from(u.paratrooper_bomber);
    i.heal_threshold = f32::from_bits(u.heal_threshold);
    i.cloaked = u.cloak_timer as u32;
    i.flight_time = u.flight_time as i32;
    if let Some(s) = stats_of(u.stats) {
        i.range = f32::from_bits(s.range);
        i.reload_time = s.reload_time as i32;
        i.fire_over_walls = i32::from(s.can_fire_over_walls);
        i.flight_time_max = s.flight_time as i32;
    }
    let Some(t) = unit_of(target) else { return };
    i.has_target = 1;
    if let Some(s) = stats_of(t.stats) {
        i.target_attack = f32::from_bits(s.attack);
        i.target_max_hp = s.max_hit_points as i32;
    }
    i.target_id = t.id;
    i.target_hp = t.hit_points as i32;
    i.target_tile = [t.tile_x as i32, t.tile_y as i32];
    if let Some(c) = unit_of(complex) {
        i.target_complex = 1;
        i.target_velocity = f32::from_bits(c.velocity);
        i.target_cloakable = i32::from(c.cloakable);
        // `GetIsCloaked()`: `mIsCloakable ? mIsCloaked : false`
        i.target_cloaked = i32::from(c.cloakable != 0 && c.cloaked != 0);
        i.target_container = i32::from(c.container);
    }
    i.target_same_player = i32::from(t.player == u.player);
    if t.player != 0 && u.player != 0 {
        let (own, other) = (player_of(u.player).unwrap_or_default(), player_of(t.player).unwrap_or_default());
        i.target_enemy = i32::from(at_war(u.player, other.id));
        i.target_enemy_of = i32::from(at_war(t.player, own.id));
    }
    if t.unit_type != -1 {
        i.target_cloak_radius = t.cloak_radius as i32;
        i.target_family = t.family as u32;
        i.target_category = t.category as u32;
        i.target_land_building = i32::from(t.land_building);
    }
}

/// `EECombatFillStep`: the record zeroed, the unit's command and id, the target's id (0 without one), the unit's
/// reload time and fire over walls (its stats).
pub fn fill_step(i: &mut EeCombatStepIn, kind: i32, unit: EeCombatHandle, target: EeCombatHandle) {
    *i = EeCombatStepIn { kind, ..Default::default() };
    let u = unit_of(unit).unwrap_or_default();
    i.command = u.command as u32;
    i.unit_id = u.id;
    i.target_id = unit_of(target).map_or(0, |t| t.id);
    if let Some(s) = stats_of(u.stats) {
        i.reload_time = s.reload_time as i32;
        i.fire_over_walls = i32::from(s.can_fire_over_walls);
    }
}

/// `EEUGHealUnit::Work`'s record, its store part: zeroed, the healer's command, the target's hit points and players
/// (the enemy table only for another player's target, both players there), the healer's healing points and the
/// target's max hit points (their stats). The C++ adds the cinematic flag, the goal's elapsed time and the tick.
pub fn fill_heal_work(i: &mut EeCombatHealWorkIn, unit: EeCombatHandle, target: EeCombatHandle) {
    *i = EeCombatHealWorkIn::default();
    let u = unit_of(unit).unwrap_or_default();
    i.command = u.command as u32;
    i.healing = stats_of(u.stats).map_or(0, |s| s.healing_points as i32);
    let Some(t) = unit_of(target) else { return };
    i.has_target = 1;
    i.target_hp = t.hit_points as i32;
    i.max_hp = stats_of(t.stats).map_or(0, |s| s.max_hit_points as i32);
    i.same_player = i32::from(u.player == t.player);
    if i.same_player == 0 && t.player != 0 && u.player != 0 {
        let own = player_of(u.player).unwrap_or_default();
        i.target_enemy_of = i32::from(at_war(t.player, own.id));
    }
}

/// `EEUGHealUnit::Work`'s store part over `input` (see [`fill_heal_work`]).
///
/// # Safety
/// `input` must point to a writable record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_fill_heal_work(input: *mut EeCombatHealWorkIn, unit: u64, target: u64) {
    fill_heal_work(unsafe { &mut *input }, unit, target);
}

/// `EECombatFillCondition`'s store part over `input` (see [`fill_condition`]).
///
/// # Safety
/// `input` must point to a writable record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_fill_condition(
    input: *mut EeCombatConditionIn,
    kind: i32,
    unit: u64,
    event: u32,
    target: u64,
    complex: u64,
) {
    fill_condition(unsafe { &mut *input }, kind, unit, event, target, complex);
}

/// `EECombatFillStep`'s store part over `input` (see [`fill_step`]).
///
/// # Safety
/// `input` must point to a writable record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_fill_step(input: *mut EeCombatStepIn, kind: i32, unit: u64, target: u64) {
    fill_step(unsafe { &mut *input }, kind, unit, target);
}

#[cfg(test)]
#[path = "store_tests.rs"]
pub(crate) mod tests;
