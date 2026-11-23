//! The shots' impact tests over their C++ statements (the units' lane, round 8; were EEUnitClassShots.cpp):
//! `ProjectileCheckGroundImpact`, `ProjectileCheckWaterImpact`, `MissileCheckGroundImpact`, `MissileCheckWaterImpact`,
//! `TorpedoCheckGroundImpact` and their `EECombatImpact`: the record's reads, ee_combat impact.rs's test with its lazy
//! reads as statements, the CBGI/CBWI capture (a C++ statement in place) and the goal, placement and shell hole
//! after it. Each C++ statement is one `EE_UNITS_UCS_*` case of `ee_units_ucs_host` (EEUnitClassShots.cpp; the
//! class host's statements pass through to EEUnitClass.cpp's).

// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use crate::combat::impact::*;
use crate::units::uc_body::*;
use crate::units::uc_create::*;
use ee_base::math::types::GeTransformation;
use ee_base::math::um::floor;

// ---- the statements (1230..1299)
/// `unit->mGoalState`
pub const EE_UNITS_UCS_GOAL_STATE: i32 = 1230;
/// `class->mCategory == kUnitCategoryBomb`
pub const EE_UNITS_UCS_IS_BOMB: i32 = 1231;
/// `unit->mFlightTime`
pub const EE_UNITS_UCS_IMPACT_FLIGHT_TIME: i32 = 1232;
/// `f[0] = unit->mProjectileStart.GetZ()`
pub const EE_UNITS_UCS_START_Z: i32 = 1233;
/// `unit->GetPosition(f[0..3])`
pub const EE_UNITS_UCS_POSITION: i32 = 1234;
/// `::gWorld.mMap->GetTile(b, c) != NULL`
pub const EE_UNITS_UCS_TILE_PRESENT: i32 = 1235;
/// goal `a`'s `mKind`
pub const EE_UNITS_UCS_GOAL_KIND: i32 = 1236;
/// `unit->GetUnitClass()->mShellHoleEffect != NULL` (`b` 0), `unit->mUnitClass->...` (`b` 1)
pub const EE_UNITS_UCS_SHELL_HOLE: i32 = 1237;
/// `EECombatCapture(CBWI when b != 0, else CBGI; the record at a, the outcome at c)`
pub const EE_UNITS_UCS_CAPTURE_IMPACT: i32 = 1238;
/// `unit->SetGoal(EEUGRust::NewAttackGround(f[0..3], false), 0, 0)`
pub const EE_UNITS_UCS_ATTACK_GROUND: i32 = 1239;
/// `EEPlaceImpactDecal(unit, f[0..3])`
pub const EE_UNITS_UCS_DECAL: i32 = 1240;
/// `f[0..3] = ` the unit's location's X, Y, Z
pub const EE_UNITS_UCS_LOCATION: i32 = 1241;
/// `unit->mLauncherClass->mUnitType == kUnitTypeCpMissileBase`
pub const EE_UNITS_UCS_LAUNCHER_CP_BASE: i32 = 1242;
/// `unit->mLauncherClass->mUnitType == kUnitTypeMissileBase`
pub const EE_UNITS_UCS_LAUNCHER_MISSILE_BASE: i32 = 1243;
/// `unit->PlaceOnGround(f[0..3])`
pub const EE_UNITS_UCS_PLACE_ON_GROUND: i32 = 1244;
/// `::gWorld.mMap->GetGroundPoint(GE3DPoint(f[0], f[1], f[2]), f[0..3])`
pub const EE_UNITS_UCS_GROUND_POINT: i32 = 1245;
/// `f[0] = unit->mTile->GetHeight()`
pub const EE_UNITS_UCS_TILE_HEIGHT: i32 = 1246;
/// `static_cast<EEUGRust*>(unit->mGoalState)->GetAttackTarget()`
pub const EE_UNITS_UCS_ATTACK_TARGET: i32 = 1247;
/// unit `a`'s class `IsLandBuilding() || mIsWall`
pub const EE_UNITS_UCS_TARGET_BUILDING: i32 = 1248;
/// unit `a`'s `GetTileX()`
pub const EE_UNITS_UCS_IMPACT_TILE_X: i32 = 1249;
/// unit `a`'s `GetTileY()`
pub const EE_UNITS_UCS_IMPACT_TILE_Y: i32 = 1250;
/// unit `a`'s class's `mTileSize`
pub const EE_UNITS_UCS_IMPACT_TILE_SIZE: i32 = 1251;
/// `unit->mGoalState->GetGoalLocation(f[0..3])`
pub const EE_UNITS_UCS_GOAL_LOCATION: i32 = 1252;
/// `EEIsOnTargetTiles(unit, static_cast<EEUGRust*>(unit->GetGoal())->GetAttackTarget())`
pub const EE_UNITS_UCS_ON_TARGET_TILES: i32 = 1253;
/// `::gWorld.mMap->GetTile(UMFloatToLong(f[0]), UMFloatToLong(f[1])) == unit->mTile`
pub const EE_UNITS_UCS_SAME_TILE: i32 = 1254;
/// `class->CanMoveOnTerrain(unit, unit->mTile->mTerrainType)`
pub const EE_UNITS_UCS_CAN_MOVE: i32 = 1255;

// ---- the bodies (`ee_units_ucs_impact`'s `what`)
pub const EE_UNITS_UCS_DO_PROJECTILE_GROUND: i32 = 1;
pub const EE_UNITS_UCS_DO_PROJECTILE_WATER: i32 = 2;
pub const EE_UNITS_UCS_DO_MISSILE_GROUND: i32 = 3;
pub const EE_UNITS_UCS_DO_MISSILE_WATER: i32 = 4;
pub const EE_UNITS_UCS_DO_TORPEDO_GROUND: i32 = 5;

/// The impact test's lazy reads as the shot's statements
struct Lazy<'s, 'h, H: UcHost + ?Sized>(&'s mut S<'h, H>);

impl<H: UcHost + ?Sized> ImpactHost for Lazy<'_, '_, H> {
    fn ground_z(&mut self, p: [f32; 3]) -> f32 {
        let mut q = p;
        self.0.point(EE_UNITS_UCS_GROUND_POINT, 0, 0, 0, &mut q);
        q[2]
    }
    fn tile_height(&mut self) -> f32 {
        self.0.float(EE_UNITS_UCS_TILE_HEIGHT, 0, 0)
    }
    fn target(&mut self) -> EeCombatImpactTarget {
        let s = &mut *self.0;
        let target = s.s(EE_UNITS_UCS_ATTACK_TARGET) as u64;
        EeCombatImpactTarget {
            building: s.a(EE_UNITS_UCS_TARGET_BUILDING, target) as i32,
            tile: [s.a(EE_UNITS_UCS_IMPACT_TILE_X, target) as i32, s.a(EE_UNITS_UCS_IMPACT_TILE_Y, target) as i32],
            size: s.a(EE_UNITS_UCS_IMPACT_TILE_SIZE, target) as i32,
            unit_tile: [s.a(EE_UNITS_UCS_IMPACT_TILE_X, s.unit) as i32, s.a(EE_UNITS_UCS_IMPACT_TILE_Y, s.unit) as i32],
        }
    }
    fn goal_location(&mut self) -> [f32; 3] {
        let mut p = [0.0f32; 3];
        self.0.point(EE_UNITS_UCS_GOAL_LOCATION, 0, 0, 0, &mut p);
        p
    }
    fn on_target_tiles(&mut self) -> bool {
        self.0.s(EE_UNITS_UCS_ON_TARGET_TILES) != 0
    }
    fn same_tile(&mut self, x: f32, y: f32) -> bool {
        let mut p = [x, y, 0.0];
        self.0.point(EE_UNITS_UCS_SAME_TILE, 0, 0, 0, &mut p) != 0
    }
    fn can_move(&mut self) -> bool {
        self.0.s(EE_UNITS_UCS_CAN_MOVE) != 0
    }
}

/// `EECombatImpact(class, unit, in)`: the test of `input.kind` over the shot's lazy reads, captured as CBGI or CBWI.
fn test<H: UcHost + ?Sized>(s: &mut S<'_, H>, input: &mut EeCombatImpactIn) -> EeCombatImpactOut {
    let mut out = impact(input, &mut Lazy(s));
    let water = input.kind == EE_COMBAT_IMPACT_PROJECTILE_WATER || input.kind == EE_COMBAT_IMPACT_MISSILE_WATER;
    s.abc(EE_UNITS_UCS_CAPTURE_IMPACT, addr(input), i64::from(water), addr(&mut out) as i64);
    out
}

fn goal_kind<H: UcHost + ?Sized>(s: &mut S<'_, H>, goal: u64) -> i32 {
    if goal != 0 {
        s.a(EE_UNITS_UCS_GOAL_KIND, goal) as i32
    } else {
        0
    }
}

/// `ProjectileCheckGroundImpact(unit)`: whether a projectile hit the ground (bombs, low launches, after 1 s of
/// flight): it attacks the ground there unless it came down on its target building or wall, and leaves a shell hole
/// on land.
pub fn projectile_ground<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) -> bool {
    let mut s = S { h, class, unit };
    let mut position = [0.0f32; 3];
    let goal = s.s(EE_UNITS_UCS_GOAL_STATE) as u64;
    let mut input = EeCombatImpactIn {
        kind: EE_COMBAT_IMPACT_PROJECTILE_GROUND,
        bomb: s.s(EE_UNITS_UCS_IS_BOMB) as i32,
        flight_time: s.s(EE_UNITS_UCS_IMPACT_FLIGHT_TIME) as i32,
        start_z: s.float(EE_UNITS_UCS_START_Z, 0, 0),
        ..Default::default()
    };
    s.point(EE_UNITS_UCS_POSITION, 0, 0, 0, &mut position);
    input.position = position;
    input.tile_present =
        s.abc(EE_UNITS_UCS_TILE_PRESENT, 0, i64::from(floor(position[0])), i64::from(floor(position[1]))) as i32;
    input.has_goal = i32::from(goal != 0);
    input.goal_kind = goal_kind(&mut s, goal);
    input.shell_hole = s.b(EE_UNITS_UCS_SHELL_HOLE, 0) as i32;
    let out = test(&mut s, &mut input);
    if out.result == 0 {
        return false;
    }
    // the ground point: the position at the ground's height (the unit's tile's off the map)
    let mut ground =
        [position[0], position[1], if input.ground_read != 0 { input.ground_z } else { input.tile_height }];
    if out.goal == EE_COMBAT_IMPACT_GOAL_GROUND {
        s.point(EE_UNITS_UCS_ATTACK_GROUND, 0, 0, 0, &mut ground);
    }
    if out.decal != 0 {
        s.point(EE_UNITS_UCS_DECAL, 0, 0, 0, &mut ground);
    }
    true
}

/// A water test (`kind`) at `position`: an attack on the ground at its point when it says so.
fn water<H: UcHost + ?Sized>(s: &mut S<'_, H>, mut input: EeCombatImpactIn) -> bool {
    let out = test(s, &mut input);
    if out.goal == EE_COMBAT_IMPACT_GOAL_WATER {
        let mut p = out.goal_point;
        s.point(EE_UNITS_UCS_ATTACK_GROUND, 0, 0, 0, &mut p);
    }
    out.result != 0
}

/// `ProjectileCheckWaterImpact(unit)`: whether a bomb, or a projectile after 1 s of flight, hit water it cannot cross;
/// if so, it attacks the ground.
pub fn projectile_water<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) -> bool {
    let mut s = S { h, class, unit };
    let mut position = [0.0f32; 3];
    let mut input = EeCombatImpactIn {
        kind: EE_COMBAT_IMPACT_PROJECTILE_WATER,
        bomb: s.s(EE_UNITS_UCS_IS_BOMB) as i32,
        flight_time: s.s(EE_UNITS_UCS_IMPACT_FLIGHT_TIME) as i32,
        ..Default::default()
    };
    s.point(EE_UNITS_UCS_POSITION, 0, 0, 0, &mut position);
    input.position = position;
    water(&mut s, input)
}

/// `MissileCheckWaterImpact(unit)`: whether the missile hit water (z 0 or below on terrain it cannot cross); if so,
/// it attacks the ground there.
pub fn missile_water<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) -> bool {
    let mut s = S { h, class, unit };
    let mut position = [0.0f32; 3];
    s.point(EE_UNITS_UCS_LOCATION, 0, 0, 0, &mut position);
    water(&mut s, EeCombatImpactIn { kind: EE_COMBAT_IMPACT_MISSILE_WATER, position, ..Default::default() })
}

/// `TorpedoCheckGroundImpact(unit)`: at ground at or above sea level, an attack on the ground there; true.
pub fn torpedo_ground<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) -> bool {
    let mut s = S { h, class, unit };
    let mut position = [0.0f32; 3];
    let mut input = EeCombatImpactIn { kind: EE_COMBAT_IMPACT_TORPEDO_GROUND, ..Default::default() };
    s.point(EE_UNITS_UCS_POSITION, 0, 0, 0, &mut position);
    input.position = position;
    let out = test(&mut s, &mut input);
    if out.goal == EE_COMBAT_IMPACT_GOAL_GROUND {
        let mut p = out.goal_point;
        s.point(EE_UNITS_UCS_ATTACK_GROUND, 0, 0, 0, &mut p);
    }
    out.result != 0
}

/// `MissileCheckGroundImpact(unit)`: whether the missile hit the ground after its flight time (0.3 higher over its
/// target): set down there and, for an attack-ground goal, a shell hole (never for a missile base's missiles).
pub fn missile_ground<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) -> bool {
    let mut s = S { h, class, unit };
    let goal = s.s(EE_UNITS_UCS_GOAL_STATE) as u64;
    let mut position = [0.0f32; 3];
    s.point(EE_UNITS_UCS_LOCATION, 0, 0, 0, &mut position);
    let mut input = EeCombatImpactIn {
        kind: EE_COMBAT_IMPACT_MISSILE_GROUND,
        flight_time: s.s(EE_UNITS_UCS_IMPACT_FLIGHT_TIME) as i32,
        has_goal: i32::from(goal != 0),
        ..Default::default()
    };
    input.goal_kind = goal_kind(&mut s, goal);
    input.launcher_cp_base = s.s(EE_UNITS_UCS_LAUNCHER_CP_BASE) as i32;
    input.launcher_missile_base = s.s(EE_UNITS_UCS_LAUNCHER_MISSILE_BASE) as i32;
    input.shell_hole = s.b(EE_UNITS_UCS_SHELL_HOLE, 1) as i32;
    input.position = position;
    let out = test(&mut s, &mut input);
    if out.place != 0 {
        s.point(EE_UNITS_UCS_PLACE_ON_GROUND, 0, 0, 0, &mut position);
        let mut t = GeTransformation::IDENTITY;
        let unit = s.unit;
        s.ab(EE_UNITS_UC_LOCATION_MATRIX, unit, addr(&mut t.m) as i64);
        t.m[2][3] += out.offset;
        s.a(EE_UNITS_UC_UPDATE_UNIT_TILE, addr(&mut t.m));
    }
    if out.decal != 0 {
        s.point(EE_UNITS_UCS_DECAL, 0, 0, 0, &mut position);
    }
    out.result != 0
}

/// Runs test `what` (`EE_UNITS_UCS_DO_*`) of `class` on `unit`; its answer.
pub fn run<H: UcHost + ?Sized>(h: &mut H, what: i32, class: u64, unit: u64) -> bool {
    match what {
        EE_UNITS_UCS_DO_PROJECTILE_GROUND => projectile_ground(h, class, unit),
        EE_UNITS_UCS_DO_PROJECTILE_WATER => projectile_water(h, class, unit),
        EE_UNITS_UCS_DO_MISSILE_GROUND => missile_ground(h, class, unit),
        EE_UNITS_UCS_DO_MISSILE_WATER => missile_water(h, class, unit),
        EE_UNITS_UCS_DO_TORPEDO_GROUND => torpedo_ground(h, class, unit),
        _ => false,
    }
}

#[cfg(not(test))]
mod game {
    use super::*;
    use crate::units::ucs_shots::game::ee_units_ucs_host;
    use std::os::raw::c_void;

    struct GameI;

    impl UcHost for GameI {
        #[allow(clippy::too_many_arguments)]
        fn uc(&mut self, op: i32, class: u64, unit: u64, a: u64, b: i64, c: i64, f: &mut [f32; 4]) -> i64 {
            // SAFETY: the test runs inside the C++ member that handed over its live class and unit
            unsafe { ee_units_ucs_host(op, class as usize as *mut c_void, unit as usize as *mut c_void, a, b, c, f) }
        }
    }

    /// The shots' impact test `what` (`EE_UNITS_UCS_DO_*`) of `class` on `unit`.
    ///
    /// # Safety
    /// `class` is a live `EEUnitClass`, `unit` a live `EEComplexUnit`, on the simulation thread.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_ucs_impact(what: i32, class: *mut c_void, unit: *mut c_void) -> bool {
        run(&mut GameI, what, class as usize as u64, unit as usize as u64)
    }
}

#[cfg(test)]
#[path = "ucs_impact_tests.rs"]
mod tests;
