//! The status ticks of `EEUnitClass` (the unit classes cluster, round 1; EEUnitClass.cpp until this round):
//! `UpdateCamouflageTimer` (the camouflage timers' events applied, the cloak's fade) and `RegeneratePower`. Their
//! decisions were already ee_units step.rs' (`step_camouflage`, `cloak_fade`, `step_power`, called here as the C++
//! called them); the events' application, the reads and the USTM notes are now here too. The C++ members are
//! one-line calls of the entry points in cls_status_host.rs.
//!
//! The world is the [`StatusHost`]: the unit's records (read and stored in place), the local player's view of a
//! cloaked unit and the mini map, the chained setters (`SetCloakableChain`, `SetCloakedChain`, `SetOpacityChain`),
//! the physics' clock, the rules and the USTM notes (TEST ONLY: only while the capture runs).

// `c_long` casts are needed on LP64.
#![allow(clippy::unnecessary_cast)]

use crate::units::step::{
    EeUnitsCamoEvent, EeUnitsPowerIn, EeUnitsPowerOut, EE_UNITS_CAMO_CAMOUFLAGED_OFF, EE_UNITS_CAMO_CAMOUFLAGED_ON,
    EE_UNITS_CAMO_CLOAKABLE_OFF, EE_UNITS_CAMO_CLOAKABLE_ON, EE_UNITS_CAMO_CLOAKED_OFF, EE_UNITS_CAMO_CLOAKED_ON,
    EE_UNITS_CAMO_HIDE, EE_UNITS_CAMO_MAX_EVENTS, EE_UNITS_CAMO_SET_SECOND, EE_UNITS_CAMO_SET_TIMER,
    EE_UNITS_CAMO_SHOW,
};
use crate::units::unit_fields::EeUnitsComplexUnitFields;
use crate::units::uu::EeUnitsUuUnit;
use crate::world::stats_fields::EeUnitsUnitClassUpgradeFields;

/// The USTM kinds (ee/EEUnitStore.h `kUnitStep*`) and the capitol's unit type.
/// cbindgen:ignore
mod k {
    pub const STEP_POWER: u32 = 4;
    pub const STEP_CAMOUFLAGE: u32 = 7;
    pub const STEP_CLOAK_FADE: u32 = 8;
    pub const T_CAPITOL: u64 = 0x29;
}
use k::*;

/// A float's bits (the notes' words).
fn bits(f: f32) -> u32 {
    f.to_bits()
}

/// The physics' clock: `mTickTimeMs`, `GetTickTime()`, `mGameTime`.
#[derive(Clone, Copy, Debug, Default)]
pub struct Clock {
    pub tick_ms: i64,
    pub tick_time: f32,
    pub game_time: i64,
}

/// The world the status ticks ask (the game's: cls_status_host.rs).
pub trait StatusHost {
    /// a unit's records; its stats record; a class's `IsLandBuilding()`
    fn unit(&mut self, obj: u64) -> EeUnitsUuUnit;
    fn stats(&mut self, unit: u64) -> *const EeUnitsUnitClassUpgradeFields;
    fn land_building(&mut self, class: u64) -> bool;
    fn class_fields(&mut self, class: u64) -> *const crate::units::unit_class_fields::EeUnitsUnitClassFields;
    /// the unit's player's `mNumVitalBuildings < 1`
    fn no_vital_buildings(&mut self, unit: u64) -> bool;
    fn clock(&mut self) -> Clock;
    /// `gWorld.mInCinematic`, `EEGetRules().mCombat.mPowerRegenerationInterval`
    fn in_cinematic(&mut self) -> bool;
    fn power_interval(&mut self) -> i64;
    /// `EEIsCloakedEnemyOf(unit, gWorld.GetCurrentPlayer())`; the mini map's move of the unit onto (`show`) or off
    /// its tile
    fn cloaked_enemy(&mut self, unit: u64) -> bool;
    fn mini_map_move(&mut self, unit: u64, show: bool);
    /// `SetCloakableChain(on)`, `SetCloakedChain(on)`, `SetOpacityChain(opacity)`
    fn set_cloakable_chain(&mut self, unit: u64, on: bool);
    fn set_cloaked_chain(&mut self, unit: u64, on: bool);
    fn set_opacity_chain(&mut self, unit: u64, opacity: f32);
    /// the class's `UpdateCloakDetection(unit)`
    fn cloak_detection(&mut self, class: u64, unit: u64);
    /// TEST ONLY: whether the capture runs; `EEUnitStepNote(kind, id, words)`
    fn noting(&mut self) -> bool;
    fn note(&mut self, kind: u32, id: u32, words: &[u32]);
    /// ee_units step.rs' `ee_units_step_camouflage` and `ee_units_step_power` (over the unit store)
    /// `HasEffect(8)`: the area effects' cloak radius (`mAffectingAreaEffects.ApplyModifiers(3, 0, out, unit)`)
    fn area_cloak_radius(&mut self, unit: u64) -> Option<i64>;
    /// `gWorldTable.GetFloatValue(kWorldCamouflageTiming)`
    fn camouflage_timing(&mut self) -> f32;
    /// `obj->GetPosition(out)`; `EEGetUnitsNear(point, radius, out)`; the player's `mEnemies[id] != 0`
    fn position(&mut self, obj: u64) -> [f32; 3];
    fn units_near(&mut self, point: [f32; 3], radius: f32) -> Vec<u64>;
    fn enemy(&mut self, player: u64, id: i32) -> bool;
    /// the unit's `GetMaxVelocity()`; `gPhysics->mRandom.Generate()`, `gPhysics->GetRandom(min, max)`
    fn max_velocity(&mut self, unit: u64) -> f32;
    fn random(&mut self) -> f32;
    fn random_range(&mut self, min: f32, max: f32) -> f32;
    #[allow(clippy::too_many_arguments)] // the decision's inputs
    fn step_camouflage(
        &mut self,
        id: u32,
        unit: u64,
        timer: u32,
        second: u32,
        no_vital: bool,
        tick_ms: i64,
    ) -> Vec<EeUnitsCamoEvent>;
    #[allow(clippy::too_many_arguments)] // the decision's inputs
    fn step_power(
        &mut self,
        id: u32,
        unit: u64,
        power: i64,
        timer: i64,
        built: bool,
        land: bool,
        inp: &EeUnitsPowerIn,
    ) -> EeUnitsPowerOut;
}

struct U(EeUnitsUuUnit);

impl U {
    fn cx(&self) -> &EeUnitsComplexUnitFields {
        // SAFETY: a live complex unit's record (read at once)
        unsafe { &*self.0.cx }
    }
    // the record is the C++ object's: a store through it is the C++'s member store
    #[allow(clippy::mut_from_ref)]
    fn cx_mut(&self) -> &mut EeUnitsComplexUnitFields {
        // SAFETY: as cx
        unsafe { &mut *(self.0.cx as *mut EeUnitsComplexUnitFields) }
    }
    fn id(&self) -> u32 {
        // SAFETY: the unit's RTSUnit record
        unsafe { (*self.0.rts).mUnitID as u32 }
    }
    fn unit_type(&self) -> u64 {
        // SAFETY: the unit's class record
        unsafe { (*self.0.class_fields).mUnitType as u64 }
    }
}

/// `CamouflageNote`'s 48 words at most.
struct Words(Vec<u32>);

impl Words {
    fn add(&mut self, w: u32) {
        if self.0.len() < 48 {
            self.0.push(w);
        }
    }
}

/// `EEUnitClass::UpdateCamouflageTimer(unit)`: the timers' events (the mini map's move only when the unit is then
/// a cloaked enemy of the local player), then for a cloakable unit the detection and the cloak's fade.
pub fn update_camouflage_timer<H: StatusHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let u = U(h.unit(unit));
    let timer = u.cx().mCamouflageTimer as u32;
    let second = u.cx().mU32C as u32;
    let mut note = Words(Vec::new());
    let noting = h.noting() && ((timer & 0x7fff_ffff) > 0 || second > 0);
    let no_vital = h.no_vital_buildings(unit);
    let clock = h.clock();
    note.add(timer);
    note.add(second);
    note.add(u32::from(no_vital));
    note.add(clock.tick_ms as u32);
    let events = h.step_camouflage(u.id(), unit, timer, second, no_vital, clock.tick_ms);
    for e in events.iter().take(EE_UNITS_CAMO_MAX_EVENTS) {
        match e.code {
            EE_UNITS_CAMO_SHOW | EE_UNITS_CAMO_HIDE => {
                let moved = h.cloaked_enemy(unit);
                if moved {
                    h.mini_map_move(unit, e.code == EE_UNITS_CAMO_SHOW);
                }
                note.add(e.code | if moved { 0x100 } else { 0 });
            }
            EE_UNITS_CAMO_CLOAKABLE_OFF | EE_UNITS_CAMO_CLOAKABLE_ON => {
                h.set_cloakable_chain(unit, e.code == EE_UNITS_CAMO_CLOAKABLE_ON);
                note.add(e.code);
            }
            EE_UNITS_CAMO_CLOAKED_OFF | EE_UNITS_CAMO_CLOAKED_ON => {
                h.set_cloaked_chain(unit, e.code == EE_UNITS_CAMO_CLOAKED_ON);
                note.add(e.code);
            }
            EE_UNITS_CAMO_CAMOUFLAGED_OFF | EE_UNITS_CAMO_CAMOUFLAGED_ON => {
                // `SetCamouflaged`: the timer's top bit
                let t = u.cx().mCamouflageTimer as u32;
                let t = if e.code == EE_UNITS_CAMO_CAMOUFLAGED_ON { t | 0x8000_0000 } else { t & 0x7fff_ffff };
                u.cx_mut().mCamouflageTimer = t as _;
                note.add(e.code);
            }
            EE_UNITS_CAMO_SET_TIMER => {
                u.cx_mut().mCamouflageTimer = e.value as _;
                note.add(e.code);
                note.add(e.value);
            }
            EE_UNITS_CAMO_SET_SECOND => {
                u.cx_mut().mU32C = e.value as _;
                note.add(e.code);
                note.add(e.value);
            }
            _ => {}
        }
    }
    if noting {
        note.add(u.cx().mCamouflageTimer as u32);
        note.add(u.cx().mU32C as u32);
        h.note(STEP_CAMOUFLAGE, u.id(), &note.0);
    }
    if !u.cx().mIsCloakable {
        return;
    }
    h.cloak_detection(class, unit);
    let before = u.cx().mOpacity;
    let cloaked = if u.cx().mIsCloakable { u.cx().mIsCloaked } else { false };
    // SAFETY: the unit's live stats record
    let power = unsafe { (*h.stats(unit)).mPower };
    let pulse = u.unit_type() == T_CAPITOL && power > 0 && !h.cloaked_enemy(unit) && u.cx().mCloaked != 0;
    let clock = h.clock();
    let opacity = crate::units::step::ee_units_step_cloak_fade(
        before,
        u8::from(cloaked),
        clock.tick_time,
        u8::from(pulse),
        clock.game_time,
    );
    if h.noting() {
        let words = [
            bits(before),
            u32::from(cloaked),
            bits(clock.tick_time),
            u32::from(pulse),
            clock.game_time as u32,
            bits(opacity),
        ];
        h.note(STEP_CLOAK_FADE, u.id(), &words);
    }
    h.set_opacity_chain(unit, opacity);
}

/// `EEUnitClass::RegeneratePower(unit)`: the regeneration over the unit store (step.rs `step_power`), its writes
/// through the C++'s setters' stores.
pub fn regenerate_power<H: StatusHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let u = U(h.unit(unit));
    // SAFETY: the unit's live stats record (copied)
    let s = unsafe { *h.stats(unit) };
    let in_cinematic = h.in_cinematic();
    let land = h.land_building(class);
    let clock = h.clock();
    let interval = h.power_interval();
    let mut words = [
        s.mPowerRegeneration as u32,
        s.mPower as u32,
        u32::from(in_cinematic),
        u32::from(land),
        u32::from(u.cx().mIsBuilt),
        u.cx().mPower as u32,
        u.cx().mPowerRegenTimer as u32,
        clock.tick_ms as u32,
        interval as u32,
        0,
        0,
        0,
        0,
    ];
    let inp = EeUnitsPowerIn {
        regeneration: s.mPowerRegeneration as i64,
        max_power: s.mPower as i64,
        in_cinematic: u8::from(in_cinematic),
        interval,
        tick_ms: clock.tick_ms,
    };
    let out =
        h.step_power(u.id(), unit, u.cx().mPower as i64, u.cx().mPowerRegenTimer as i64, u.cx().mIsBuilt, land, &inp);
    if out.write != 0 {
        if out.set_power != 0 && u.cx().mPower as i64 != out.power {
            u.cx_mut().mPower = out.power as _;
        }
        if u.cx().mPowerRegenTimer as i64 != out.regen_timer {
            u.cx_mut().mPowerRegenTimer = out.regen_timer as _;
        }
    }
    if h.noting() {
        words[9] = u32::from(out.write);
        words[10] = u32::from(out.set_power);
        words[11] = if words[10] != 0 { u.cx().mPower as u32 } else { 0 };
        words[12] = if words[9] != 0 { u.cx().mPowerRegenTimer as u32 } else { 0 };
        h.note(STEP_POWER, u.id(), &words);
    }
}

/// `EEUnitClass::UpdateCloakDetection(unit)`: a living cloaked unit is uncloaked while an enemy is within its cloak
/// radius (an area effect's, the camouflage's share of its range, else its class's); the mini map moves it when the
/// local player's view of it changes.
pub fn update_cloak_detection<H: StatusHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let u = U(h.unit(unit));
    let player = u.0.player;
    let old_concealed = if u.cx().mIsCloakable { u.cx().mIsCloaked } else { false };
    let mut concealed = true;
    // SAFETY: the unit's EEUnit record
    if !(unsafe { (*u.0.unit).mHitPoints } > 0) {
        return;
    }
    let position = h.position(unit);
    let old_visible = h.cloaked_enemy(unit);
    h.set_cloaked_chain(unit, true);
    // SAFETY: a live class's record
    let cloak_radius = unsafe { (*h.class_fields(class)).mCloakRadius } as i64;
    let radius = if let Some(r) = h.area_cloak_radius(unit) {
        r.max(cloak_radius)
    } else if u.cx().mU32C != 0 {
        let timing = h.camouflage_timing();
        // SAFETY: the unit's live stats record
        i64::from(ee_base::math::um::lrintf(timing * unsafe { (*h.stats(unit)).mRange }))
    } else {
        cloak_radius
    };
    if radius as f32 > 0.0 {
        for other in h.units_near(position, radius as f32 + 0.5) {
            let o = U(h.unit(other));
            // SAFETY: the other unit's records (read at once)
            let (hp, projectile) = unsafe { ((*o.0.unit).mHitPoints, (*o.0.class_fields).mIsProjectile) };
            if other == unit
                || !(hp > 0)
                || o.0.player_id == 0
                || projectile
                || o.0.player == player
                || !h.enemy(player, o.0.player_id)
            {
                continue;
            }
            h.set_cloaked_chain(unit, false);
            concealed = false;
            break;
        }
    }
    let visible = h.cloaked_enemy(unit);
    if old_concealed == concealed {
        return;
    }
    if old_concealed && old_visible && !visible {
        h.mini_map_move(unit, true);
    } else if concealed && !old_visible && visible {
        h.mini_map_move(unit, false);
    }
}

/// `EEUnitClass::UnitDamaged(unit, attacker, damage)`: a moving, living space unit rocks by a random roll to
/// either side, clamped to the maximum roll.
pub fn unit_damaged<H: StatusHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    const MIN_ROLL: f32 = 0.05;
    const MAX_ROLL: f32 = 0.2;
    // SAFETY: a live class's record
    let domains = unsafe { (*h.class_fields(class)).mDomains };
    if domains & 8 == 0 || unit == 0 || !(h.max_velocity(unit) > 0.0) {
        return;
    }
    let u = U(h.unit(unit));
    // SAFETY: the unit's EEUnit record
    if !(unsafe { (*u.0.unit).mHitPoints } > 0) {
        return;
    }
    let delta =
        if h.random() < 0.5 { h.random_range(MIN_ROLL, MAX_ROLL) } else { h.random_range(-MAX_ROLL, -MIN_ROLL) };
    let mut value = u.cx().mRoll;
    value += delta;
    if value > MAX_ROLL {
        value = MAX_ROLL;
    } else if value < -MAX_ROLL {
        value = -MAX_ROLL;
    }
    u.cx_mut().mRoll = value;
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::units::unit_class_fields::EeUnitsUnitClassFields;
    use crate::units::unit_fields::{EeUnitsRtsUnitFields, EeUnitsUnitFields};

    #[derive(Default)]
    struct Mock {
        rts: Box<EeUnitsRtsUnitFields>,
        unit: Box<EeUnitsUnitFields>,
        cx: Box<EeUnitsComplexUnitFields>,
        class: Box<EeUnitsUnitClassFields>,
        stats: Box<EeUnitsUnitClassUpgradeFields>,
        events: Vec<EeUnitsCamoEvent>,
        power: EeUnitsPowerOut,
        enemy: bool,
        near: Vec<u64>,
        log: Vec<String>,
        notes: Vec<(u32, Vec<u32>)>,
    }

    impl StatusHost for Mock {
        fn unit(&mut self, obj: u64) -> EeUnitsUuUnit {
            EeUnitsUuUnit {
                obj,
                rts: &*self.rts,
                unit: &*self.unit,
                cx: &*self.cx,
                class_fields: &*self.class,
                ..Default::default()
            }
        }
        fn stats(&mut self, _unit: u64) -> *const EeUnitsUnitClassUpgradeFields {
            &*self.stats
        }
        fn land_building(&mut self, _class: u64) -> bool {
            true
        }
        fn class_fields(&mut self, _class: u64) -> *const EeUnitsUnitClassFields {
            &*self.class
        }
        fn area_cloak_radius(&mut self, _unit: u64) -> Option<i64> {
            None
        }
        fn camouflage_timing(&mut self) -> f32 {
            0.5
        }
        fn position(&mut self, _obj: u64) -> [f32; 3] {
            [1.0, 1.0, 0.0]
        }
        fn units_near(&mut self, _point: [f32; 3], radius: f32) -> Vec<u64> {
            self.log.push(format!("near {radius}"));
            self.near.clone()
        }
        fn enemy(&mut self, _player: u64, _id: i32) -> bool {
            true
        }
        fn max_velocity(&mut self, _unit: u64) -> f32 {
            1.0
        }
        fn random(&mut self) -> f32 {
            0.25
        }
        fn random_range(&mut self, min: f32, max: f32) -> f32 {
            (min + max) * 0.5
        }
        fn no_vital_buildings(&mut self, _unit: u64) -> bool {
            false
        }
        fn clock(&mut self) -> Clock {
            Clock { tick_ms: 125, tick_time: 0.125, game_time: 9000 }
        }
        fn in_cinematic(&mut self) -> bool {
            false
        }
        fn power_interval(&mut self) -> i64 {
            5000
        }
        fn cloaked_enemy(&mut self, _unit: u64) -> bool {
            self.enemy
        }
        fn mini_map_move(&mut self, _unit: u64, show: bool) {
            self.log.push(format!("mini map {show}"));
        }
        fn set_cloakable_chain(&mut self, _unit: u64, on: bool) {
            self.cx.mIsCloakable = on;
            self.log.push(format!("cloakable {on}"));
        }
        fn set_cloaked_chain(&mut self, _unit: u64, on: bool) {
            self.log.push(format!("cloaked {on}"));
        }
        fn set_opacity_chain(&mut self, _unit: u64, opacity: f32) {
            self.log.push(format!("opacity {opacity}"));
        }
        fn cloak_detection(&mut self, _class: u64, _unit: u64) {
            self.log.push("detection".into());
        }
        fn noting(&mut self) -> bool {
            true
        }
        fn note(&mut self, kind: u32, _id: u32, words: &[u32]) {
            self.notes.push((kind, words.to_vec()));
        }
        fn step_camouflage(
            &mut self,
            _id: u32,
            _unit: u64,
            _t: u32,
            _s: u32,
            _v: bool,
            _ms: i64,
        ) -> Vec<EeUnitsCamoEvent> {
            self.events.clone()
        }
        fn step_power(
            &mut self,
            _id: u32,
            _u: u64,
            _p: i64,
            _t: i64,
            _b: bool,
            _l: bool,
            _i: &EeUnitsPowerIn,
        ) -> EeUnitsPowerOut {
            self.power
        }
    }

    #[test]
    fn camouflage_events_are_applied_in_order() {
        let mut m = Mock::default();
        m.cx.mCamouflageTimer = 7;
        m.enemy = true;
        m.events = vec![
            EeUnitsCamoEvent { code: EE_UNITS_CAMO_SET_TIMER, value: 3 },
            EeUnitsCamoEvent { code: EE_UNITS_CAMO_CAMOUFLAGED_ON, value: 0 },
            EeUnitsCamoEvent { code: EE_UNITS_CAMO_HIDE, value: 0 },
            EeUnitsCamoEvent { code: EE_UNITS_CAMO_CLOAKABLE_ON, value: 0 },
        ];
        update_camouflage_timer(&mut m, 0, 1);
        assert_eq!(m.cx.mCamouflageTimer as u32, 0x8000_0003);
        assert_eq!(m.log[..3], ["mini map false", "cloakable true", "detection"]);
        assert!(m.log[3].starts_with("opacity "));
        assert_eq!(m.notes[0].0, STEP_CAMOUFLAGE);
        assert_eq!(m.notes[0].1, [7, 0, 0, 125, 9, 3, 8, 0x102, 4, 0x8000_0003, 0]);
        assert_eq!(m.notes[1].0, STEP_CLOAK_FADE);
    }

    #[test]
    fn not_cloakable_stops_after_the_timers() {
        let mut m = Mock::default();
        update_camouflage_timer(&mut m, 0, 1);
        assert!(m.log.is_empty());
        assert!(m.notes.is_empty());
    }

    #[test]
    fn power_writes_and_notes() {
        let mut m = Mock::default();
        m.stats.mPowerRegeneration = 2;
        m.stats.mPower = 10;
        m.cx.mPower = 4;
        m.cx.mPowerRegenTimer = 100;
        m.cx.mIsBuilt = true;
        m.power = EeUnitsPowerOut { write: 1, set_power: 1, power: 6, regen_timer: 5000 };
        regenerate_power(&mut m, 0, 1);
        assert_eq!(m.cx.mPower, 6);
        assert_eq!(m.cx.mPowerRegenTimer, 5000);
        assert_eq!(m.notes[0], (STEP_POWER, vec![2, 10, 0, 1, 1, 4, 100, 125, 5000, 1, 1, 6, 5000]));
    }

    #[test]
    fn an_enemy_near_uncloaks() {
        let mut m = Mock::default();
        m.unit.mHitPoints = 5;
        m.class.mCloakRadius = 3;
        m.cx.mIsCloakable = true;
        m.cx.mIsCloaked = true;
        // the other unit is the mock's same records: a living enemy (player id 0 in the mock: skipped)
        m.near = vec![1, 2];
        update_cloak_detection(&mut m, 0, 1);
        assert_eq!(m.log, ["cloaked true", "near 3.5"]);
        m.class.mCloakRadius = 0;
        m.log.clear();
        update_cloak_detection(&mut m, 0, 1);
        assert_eq!(m.log, ["cloaked true"]);
    }

    #[test]
    fn a_space_unit_rocks() {
        let mut m = Mock::default();
        m.unit.mHitPoints = 1;
        m.class.mDomains = 8;
        m.cx.mRoll = 0.1;
        unit_damaged(&mut m, 0, 1);
        assert_eq!(m.cx.mRoll, 0.2);
        m.class.mDomains = 2;
        m.cx.mRoll = 0.0;
        unit_damaged(&mut m, 0, 1);
        assert_eq!(m.cx.mRoll, 0.0);
    }
}
