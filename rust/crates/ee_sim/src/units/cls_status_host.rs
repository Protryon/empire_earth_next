//! The status ticks' host in the game ([`StatusHost`] over the unit utilities' reads, the class bodies' C++ switch
//! `EEUnitClassMoveOp` and step.rs' decisions) and the entry points the `EEUnitClass` members call (cls_status.rs).
//! Absent from a test binary.

#![cfg(not(test))]

use crate::units::cls_host::{ClsGame, EeUnitsClsIo};
use crate::units::cls_status::{self as st, Clock, StatusHost};
use crate::units::step::{
    ee_units_step_camouflage, ee_units_step_power, EeUnitsCamoEvent, EeUnitsPowerIn, EeUnitsPowerOut,
    EE_UNITS_CAMO_MAX_EVENTS,
};
use crate::units::uu::{EeUnitsUuUnit, UuHost};
use crate::units::uu_host::Game;
use crate::world::stats_fields::EeUnitsUnitClassUpgradeFields;
use crate::world::{UnitView, WorldRead};

/// unit `a`'s player's `mNumVitalBuildings < 1`
pub const EE_UNITS_CLS_ST_NO_VITAL: i32 = 93;
/// `gPhysics`' `mTickTimeMs` into `io.a`, `GetTickTime()` into `io.f`, `mGameTime` into `io.b`
pub const EE_UNITS_CLS_ST_CLOCK: i32 = 94;
/// `gWorld.mInCinematic` into `io.a`, the rules' power regeneration interval into `io.b`
pub const EE_UNITS_CLS_ST_CINEMATIC_INTERVAL: i32 = 95;
/// `EEIsCloakedEnemyOf(unit a, the current player)`
pub const EE_UNITS_CLS_ST_CLOAKED_ENEMY: i32 = 96;
/// `gMiniMap->MoveUnit(a, ...)` onto its tile (c 1) or off it (c 0)
pub const EE_UNITS_CLS_ST_MINI_MAP: i32 = 97;
/// unit `a`'s `SetCloakableChain(d)` (c 0), `SetCloakedChain(d)` (c 1), `SetOpacityChain(io.f)` (c 2)
pub const EE_UNITS_CLS_ST_CHAIN: i32 = 98;
/// class `b`'s `UpdateCloakDetection(unit a)`
pub const EE_UNITS_CLS_ST_CLOAK_DETECTION: i32 = 99;
/// TEST ONLY: `EESimCaptureRecord::Active()`
pub const EE_UNITS_CLS_ST_NOTING: i32 = 100;
/// TEST ONLY: `EEUnitStepNote(c, d, the io.a words at io.list)`
pub const EE_UNITS_CLS_ST_NOTE: i32 = 101;

/// unit `a`'s `HasEffect(8)`: 1 with `ApplyModifiers(3, 0, io.a, a)` of its area effects, else 0
pub const EE_UNITS_CLS_ST_AREA_CLOAK: i32 = 102;
/// `gWorldTable.GetFloatValue(kWorldCamouflageTiming)` into `io.f`
pub const EE_UNITS_CLS_ST_CAMOUFLAGE_TIMING: i32 = 103;
/// `gPhysics->GetRandom(io.p[0], io.p[1])` into `io.f`
pub const EE_UNITS_CLS_ST_RANDOM_RANGE: i32 = 104;

impl ClsGame {
    fn st(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64, f: f32) -> (i64, EeUnitsClsIo) {
        let mut io = EeUnitsClsIo { f, ..Default::default() };
        let r = self.call(op, a, b, c, d, &mut io);
        (r, io)
    }
}

impl StatusHost for ClsGame {
    fn unit(&mut self, obj: u64) -> EeUnitsUuUnit {
        self.0.unit(obj)
    }
    fn stats(&mut self, unit: u64) -> *const EeUnitsUnitClassUpgradeFields {
        // SAFETY: the bodies' caller holds the world; a live unit
        let t = unsafe { WorldRead::begin() };
        // SAFETY: as above
        unsafe { UnitView::of(&t, unit) }.and_then(|u| u.stats()).map_or(std::ptr::null(), |s| s.fields().ptr())
    }
    fn land_building(&mut self, class: u64) -> bool {
        self.0.class(class).land_building != 0
    }
    fn class_fields(&mut self, class: u64) -> *const crate::units::unit_class_fields::EeUnitsUnitClassFields {
        self.0.class(class).fields
    }
    fn area_cloak_radius(&mut self, unit: u64) -> Option<i64> {
        let (r, io) = self.st(EE_UNITS_CLS_ST_AREA_CLOAK, unit, 0, 0, 0, 0.0);
        (r != 0).then_some(io.a)
    }
    fn camouflage_timing(&mut self) -> f32 {
        self.st(EE_UNITS_CLS_ST_CAMOUFLAGE_TIMING, 0, 0, 0, 0, 0.0).1.f
    }
    fn position(&mut self, obj: u64) -> [f32; 3] {
        self.0.position(obj)
    }
    fn units_near(&mut self, point: [f32; 3], radius: f32) -> Vec<u64> {
        crate::units::cls_building::BuildingHost::units_near(self, point, radius)
    }
    fn enemy(&mut self, player: u64, id: i32) -> bool {
        self.0.enemy(player, id)
    }
    fn max_velocity(&mut self, unit: u64) -> f32 {
        self.st(crate::units::cls_move_host::EE_UNITS_CLS_MV_MAX_VELOCITY, unit, 0, 0, 0, 0.0).1.p[0]
    }
    fn random(&mut self) -> f32 {
        self.st(crate::units::cls_carrier_host::EE_UNITS_CLS_CR_RANDOM, 0, 0, 0, 0, 0.0).1.f
    }
    fn random_range(&mut self, min: f32, max: f32) -> f32 {
        let mut io = EeUnitsClsIo { p: [min, max, 0.0], ..Default::default() };
        self.call(EE_UNITS_CLS_ST_RANDOM_RANGE, 0, 0, 0, 0, &mut io);
        io.f
    }
    fn no_vital_buildings(&mut self, unit: u64) -> bool {
        self.st(EE_UNITS_CLS_ST_NO_VITAL, unit, 0, 0, 0, 0.0).0 != 0
    }
    fn clock(&mut self) -> Clock {
        let io = self.st(EE_UNITS_CLS_ST_CLOCK, 0, 0, 0, 0, 0.0).1;
        Clock { tick_ms: io.a, tick_time: io.f, game_time: io.b }
    }
    fn in_cinematic(&mut self) -> bool {
        self.st(EE_UNITS_CLS_ST_CINEMATIC_INTERVAL, 0, 0, 0, 0, 0.0).1.a != 0
    }
    fn power_interval(&mut self) -> i64 {
        self.st(EE_UNITS_CLS_ST_CINEMATIC_INTERVAL, 0, 0, 0, 0, 0.0).1.b
    }
    fn cloaked_enemy(&mut self, unit: u64) -> bool {
        self.st(EE_UNITS_CLS_ST_CLOAKED_ENEMY, unit, 0, 0, 0, 0.0).0 != 0
    }
    fn mini_map_move(&mut self, unit: u64, show: bool) {
        self.st(EE_UNITS_CLS_ST_MINI_MAP, unit, 0, i64::from(show), 0, 0.0);
    }
    fn set_cloakable_chain(&mut self, unit: u64, on: bool) {
        self.st(EE_UNITS_CLS_ST_CHAIN, unit, 0, 0, i64::from(on), 0.0);
    }
    fn set_cloaked_chain(&mut self, unit: u64, on: bool) {
        self.st(EE_UNITS_CLS_ST_CHAIN, unit, 0, 1, i64::from(on), 0.0);
    }
    fn set_opacity_chain(&mut self, unit: u64, opacity: f32) {
        self.st(EE_UNITS_CLS_ST_CHAIN, unit, 0, 2, 0, opacity);
    }
    fn cloak_detection(&mut self, class: u64, unit: u64) {
        self.st(EE_UNITS_CLS_ST_CLOAK_DETECTION, unit, class, 0, 0, 0.0);
    }
    fn noting(&mut self) -> bool {
        self.st(EE_UNITS_CLS_ST_NOTING, 0, 0, 0, 0, 0.0).0 != 0
    }
    fn note(&mut self, kind: u32, id: u32, words: &[u32]) {
        let mut io =
            EeUnitsClsIo { a: words.len() as i64, list: words.as_ptr() as *mut std::ffi::c_void, ..Default::default() };
        self.call(EE_UNITS_CLS_ST_NOTE, 0, 0, i64::from(kind), i64::from(id), &mut io);
    }
    fn step_camouflage(
        &mut self,
        id: u32,
        unit: u64,
        timer: u32,
        second: u32,
        no_vital: bool,
        tick_ms: i64,
    ) -> Vec<EeUnitsCamoEvent> {
        let mut events = [EeUnitsCamoEvent::default(); EE_UNITS_CAMO_MAX_EVENTS];
        // SAFETY: room for EE_UNITS_CAMO_MAX_EVENTS events
        let n = unsafe {
            ee_units_step_camouflage(id, unit, timer, second, u8::from(no_vital), tick_ms, events.as_mut_ptr())
        };
        events[..n as usize].to_vec()
    }
    fn step_power(
        &mut self,
        id: u32,
        unit: u64,
        power: i64,
        timer: i64,
        built: bool,
        land: bool,
        inp: &EeUnitsPowerIn,
    ) -> EeUnitsPowerOut {
        let mut out = EeUnitsPowerOut::default();
        // SAFETY: a valid input and a writable output
        unsafe { ee_units_step_power(id, unit, power, timer, u8::from(built), u8::from(land), inp, &mut out) };
        out
    }
}

fn game() -> ClsGame {
    ClsGame(Game)
}

/// `EEUnitClass::UpdateCamouflageTimer(unit)`.
#[no_mangle]
pub extern "C" fn ee_units_st_camouflage(class: u64, unit: u64) {
    st::update_camouflage_timer(&mut game(), class, unit);
}

/// `EEUnitClass::RegeneratePower(unit)`.
#[no_mangle]
pub extern "C" fn ee_units_st_power(class: u64, unit: u64) {
    st::regenerate_power(&mut game(), class, unit);
}

/// `EEUnitClass::UpdateCloakDetection(unit)`.
#[no_mangle]
pub extern "C" fn ee_units_st_cloak_detection(class: u64, unit: u64) {
    st::update_cloak_detection(&mut game(), class, unit);
}

/// `EEUnitClass::UnitDamaged(unit, attacker, damage)`.
#[no_mangle]
pub extern "C" fn ee_units_st_unit_damaged(class: u64, unit: u64) {
    st::unit_damaged(&mut game(), class, unit);
}
