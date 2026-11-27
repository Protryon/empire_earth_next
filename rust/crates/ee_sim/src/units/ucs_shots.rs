//! The shots' unit bodies over their C++ statements (the units' lane, round 7; were EEUnitClassShots.cpp):
//! `ProjectileUnitCreated`, `MissileUnitCreated`, `ProjectileLaunchProjectile` (the ballistic pitch stays a C++
//! statement), `BombLaunchProjectile`, `MissileLaunchProjectile`, `ProjectileUpdateEffects`, `BombUpdate`,
//! `LaunchStoredMissile`, `EEIsOnTargetTiles`. Each C++ statement one `EE_UNITS_UCS_*` case of `ee_units_uc_host`
//! (EEUnitClassShots.cpp; the class host's statements, `EE_UNITS_UC_*`, reach EEUnitClass.cpp's through it), in the
//! original's order; the transformations and their math are ee_base's, as the C++ inlines called them.

// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use crate::units::uc_anim::*;
use crate::units::uc_body::*;
use crate::units::uc_create::*;
use crate::units::uc_produce::*;
use ee_base::math::transform::RotationOrder;
use ee_base::math::types::{GeTransformation, GeVector};
use ee_base::math::um::{um_arccos, um_arctan2, um_cos};

// ---- the statements (from 1200)
/// `unit->mInitialContactLocation = GE3DPoint(f[0], f[1], f[2])`
pub const EE_UNITS_UCS_SET_CONTACT_LOCATION: i32 = 1200;
/// `unit->SetVelocity(unit->mStats->GetMaxVelocity())`
pub const EE_UNITS_UCS_FULL_SPEED: i32 = 1201;
/// `unit->mUnitClass->mUnitType`
pub const EE_UNITS_UCS_UNIT_TYPE: i32 = 1202;
/// `EEGetBallisticPitch(unit->mOwner, unit->mPlayer->mUnitClassUpgrades[unit->mLauncherClass->mUnitType], the point
/// at a, the point at b, &f[0])`
pub const EE_UNITS_UCS_BALLISTIC_PITCH: i32 = 1203;
/// `unit->SetFlightTime(b)`
pub const EE_UNITS_UCS_SET_FLIGHT_TIME: i32 = 1204;
/// `f[0] = unit->mPlayer->mUnitClassUpgrades[unit->mLauncherClass->mUnitType]->mLaunchSpeed`
pub const EE_UNITS_UCS_LAUNCH_SPEED: i32 = 1205;
/// `f[0] = unit->mPlayer->GetUpgrade(unit->mLauncherClass->mUnitType)->mLaunchSpeed`
pub const EE_UNITS_UCS_UPGRADE_LAUNCH_SPEED: i32 = 1206;
/// `f[0] = unit->mLauncherClass->mLaunchPitch`
pub const EE_UNITS_UCS_LAUNCH_PITCH: i32 = 1207;
/// `class->mTrailEffect != NULL`
pub const EE_UNITS_UCS_HAS_TRAIL: i32 = 1208;
/// `class->mTrailEffect->mDetailLevel <= gEffectDetail`
pub const EE_UNITS_UCS_TRAIL_DETAILED: i32 = 1209;
/// `class->mRippleEffect != NULL`
pub const EE_UNITS_UCS_HAS_RIPPLE: i32 = 1210;
/// `class->mRippleEffect->mDetailLevel <= gEffectDetail`
pub const EE_UNITS_UCS_RIPPLE_DETAILED: i32 = 1211;
/// `EEUnitCast<EEComplexUnit>(unit a)`
pub const EE_UNITS_UCS_COMPLEX_CAST: i32 = 1212;
/// `unit->mFlightTime`
pub const EE_UNITS_UCS_FLIGHT_TIME: i32 = 1213;
/// complex unit `a`'s `SetIsTinted(b != 0)`
pub const EE_UNITS_UCS_SET_TINTED: i32 = 1214;
/// complex unit `a`'s `SetColor(the SAS explosive's blink colour)`
pub const EE_UNITS_UCS_SET_BLINK_COLOR: i32 = 1215;
/// `class->ProjectileUpdate(unit)`
pub const EE_UNITS_UCS_PROJECTILE_UPDATE: i32 = 1216;
/// `EECombatFindDropLocation(unit, NULL, f[0..3])`
pub const EE_UNITS_UCS_FIND_DROP: i32 = 1217;
/// `unit->mCargo[0]`
pub const EE_UNITS_UCS_FIRST_CARGO: i32 = 1218;
/// `unit->UnloadCargoNow(complex unit a)`
pub const EE_UNITS_UCS_UNLOAD_NOW: i32 = 1219;
/// `class->LaunchShot(unit, unit a, f[0..3], complex unit b)`
pub const EE_UNITS_UCS_LAUNCH_SHOT: i32 = 1220;
/// unit `a`'s `mComplexUnit`
pub const EE_UNITS_UCS_COMPLEX_FIELD: i32 = 1221;
/// unit `a`'s `mTile`
pub const EE_UNITS_UCS_TILE_OF: i32 = 1222;
/// complex unit `a`'s `mPreviousPart`
pub const EE_UNITS_UCS_PREVIOUS_PART: i32 = 1223;
/// complex unit `a`'s `mNextPart`
pub const EE_UNITS_UCS_NEXT_PART: i32 = 1224;
/// complex unit `a`'s `mUnitClass->mTileSize`
pub const EE_UNITS_UCS_TILE_SIZE_OF: i32 = 1225;
/// complex unit `a`'s `mTileX`
pub const EE_UNITS_UCS_TILE_X_OF: i32 = 1226;
/// complex unit `a`'s `mTileY`
pub const EE_UNITS_UCS_TILE_Y_OF: i32 = 1227;
/// `unit->mCloaked = 300000`
pub const EE_UNITS_UCS_SET_LIFETIME: i32 = 1228;
/// `unit->SetFlightTime(unit->mLauncherClass->mMissileDelayTime)`
pub const EE_UNITS_UCS_SET_LAUNCH_DELAY: i32 = 1229;

// ---- the bodies (`ee_units_ucs_call`'s `what`)
pub const EE_UNITS_UCS_DO_PROJECTILE_CREATED: i32 = 1;
pub const EE_UNITS_UCS_DO_MISSILE_CREATED: i32 = 2;
pub const EE_UNITS_UCS_DO_PROJECTILE_LAUNCH: i32 = 3;
pub const EE_UNITS_UCS_DO_BOMB_LAUNCH: i32 = 4;
pub const EE_UNITS_UCS_DO_MISSILE_LAUNCH: i32 = 5;
pub const EE_UNITS_UCS_DO_PROJECTILE_EFFECTS: i32 = 6;
pub const EE_UNITS_UCS_DO_BOMB_UPDATE: i32 = 7;
pub const EE_UNITS_UCS_DO_LAUNCH_STORED: i32 = 8;
pub const EE_UNITS_UCS_DO_ON_TARGET_TILES: i32 = 9;

const ACTION_ADVANCE: i64 = 10003;
const UNIT_TYPE_CP_ICBM: i64 = 0x2ee;
const UNIT_TYPE_BOMB_FOR_SAS_EXPLOSIVE_EXPERT: i64 = 0x305;
const GOAL_ATTACK_UNIT: i64 = 2;
/// the flight animation
const ANIMATION_FLIGHT: i64 = 0xd;
const HALF_PI: f32 = std::f32::consts::FRAC_PI_2;

fn position(t: &GeTransformation) -> [f32; 3] {
    [t.m[0][3], t.m[1][3], t.m[2][3]]
}

/// The new shot's start: `BaseUnitCreated`, its contact location
fn created<H: UcHost + ?Sized>(s: &mut S<'_, H>) {
    let (class, unit) = (s.class, s.unit);
    base_unit_created(s.h, class, unit);
    let mut here = [0.0f32; 3];
    s.point(EE_UNITS_UC_LOCATION, unit, 0, 0, &mut here);
    s.point(EE_UNITS_UCS_SET_CONTACT_LOCATION, 0, 0, 0, &mut here);
}

/// The flight's start: the flight animation, advancing
fn advance<H: UcHost + ?Sized>(s: &mut S<'_, H>) {
    s.b(EE_UNITS_UC_SET_ANIMATION, ANIMATION_FLIGHT);
    s.b(EE_UNITS_UC_START_ACTION_SELF, ACTION_ADVANCE);
    s.s(EE_UNITS_UC_CLASS_ACTION_CHANGED);
}

/// `ProjectileUnitCreated(unit)`: its start point recorded, full speed, advancing.
pub fn projectile_created<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = S { h, class, unit };
    created(&mut s);
    s.s(EE_UNITS_UCS_FULL_SPEED);
    advance(&mut s);
}

/// `MissileUnitCreated(unit)`: its start point recorded, stopped; all but the ICBM advancing.
pub fn missile_created<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = S { h, class, unit };
    created(&mut s);
    s.a(EE_UNITS_UC_STOP, unit);
    if s.s(EE_UNITS_UCS_UNIT_TYPE) == UNIT_TYPE_CP_ICBM {
        return;
    }
    advance(&mut s);
}

/// The launch's end: the flight time reset, the orientation, the start point, the speed, the turned placement
fn launch<H: UcHost + ?Sized>(s: &mut S<'_, H>, t: &mut GeTransformation, yaw: f32, pitch: f32, velocity: f32) {
    let unit = s.unit;
    let start = position(t);
    s.b(EE_UNITS_UCS_SET_FLIGHT_TIME, 0);
    let mut f = [yaw, pitch, 0.0, 0.0];
    s.f(EE_UNITS_UC_SET_ORIENTATION, unit, 0, 0, &mut f);
    let mut p = start;
    s.point(EE_UNITS_UC_SET_PROJECTILE_START, unit, 0, 0, &mut p);
    s.with_float(EE_UNITS_UC_SET_VELOCITY_OF, unit, velocity);
    t.set_orientation(RotationOrder::Ypr, yaw, pitch, 0.0);
    s.a(EE_UNITS_UC_UPDATE_UNIT_TILE, addr(&mut t.m));
}

/// The unit's forward direction (its transformation's y axis) and the yaw it gives
fn forward(t: &GeTransformation) -> (GeVector, f32) {
    let direction = t.apply_vector(GeVector { x: 0.0, y: 1.0, z: 0.0 });
    (direction, um_arctan2(-direction.x, direction.y))
}

/// `ProjectileLaunchProjectile(unit, location)`: the projectile launched at the ballistic pitch for its launcher's
/// stats; whether there is one.
pub fn projectile_launch<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64, location: [f32; 3]) -> bool {
    let mut s = S { h, class, unit };
    let mut t = GeTransformation::IDENTITY;
    s.ab(EE_UNITS_UC_LOCATION_MATRIX, unit, addr(&mut t.m) as i64);
    let mut from = position(&t);
    let mut to = location;
    let mut f = [0.0f32; 4];
    let result = s.f(EE_UNITS_UCS_BALLISTIC_PITCH, addr(&mut from), addr(&mut to) as i64, 0, &mut f) != 0;
    let angle = f[0];
    let (_, yaw) = forward(&t);
    let speed = s.float(EE_UNITS_UCS_LAUNCH_SPEED, 0, 0);
    launch(&mut s, &mut t, yaw, angle, um_cos(angle) * speed);
    result
}

/// `BombLaunchProjectile(unit, location)`: the bomb dropped along its carrier's heading, pitched down by the
/// launcher's tilt (0 to -90 degrees), with the launch speed's horizontal part; true.
pub fn bomb_launch<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) -> bool {
    let mut s = S { h, class, unit };
    let mut t = GeTransformation::IDENTITY;
    s.ab(EE_UNITS_UC_LOCATION_MATRIX, unit, addr(&mut t.m) as i64);
    let speed = s.float(EE_UNITS_UCS_UPGRADE_LAUNCH_SPEED, 0, 0);
    let (direction, yaw) = forward(&t);
    let tilt = s.float(EE_UNITS_UCS_LAUNCH_PITCH, 0, 0);
    // held to 0..-90 degrees (a NaN stays one, as in the original's two tests)
    let pitch = (um_arccos(-direction.z) - HALF_PI + tilt).clamp(-HALF_PI, 0.0);
    launch(&mut s, &mut t, yaw, pitch, um_cos(pitch) * speed);
    true
}

/// `MissileLaunchProjectile(unit, location)`: the missile tilted to its launcher's launch pitch, its launch delay and
/// a 300 s lifetime started; true.
pub fn missile_launch<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) -> bool {
    let mut s = S { h, class, unit };
    let mut rotation = GeTransformation::IDENTITY;
    let pitch = s.float(EE_UNITS_UCS_LAUNCH_PITCH, 0, 0);
    rotation.set_orientation(RotationOrder::Ypr, 0.0, pitch, 0.0);
    let mut here = GeTransformation::IDENTITY;
    s.ab(EE_UNITS_UC_LOCATION_MATRIX, unit, addr(&mut here.m) as i64);
    let mut t = here.compose(&rotation);
    s.a(EE_UNITS_UC_UPDATE_UNIT_TILE, addr(&mut t.m));
    s.s(EE_UNITS_UCS_SET_LAUNCH_DELAY);
    s.s(EE_UNITS_UCS_SET_LIFETIME);
    s.a(EE_UNITS_UC_STOP, unit);
    true
}

/// `ProjectileUpdateEffects(unit)`: a living projectile's trail (when moving) and ripples, as the effect detail
/// allows them.
pub fn projectile_effects<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = S { h: &mut *h, class, unit };
    if s.s(EE_UNITS_UC_HIT_POINTS) <= 0 {
        return;
    }
    if s.s(EE_UNITS_UCS_HAS_TRAIL) != 0
        && s.float(EE_UNITS_UC_VELOCITY_FIELD, 0, 0) != 0.0
        && s.s(EE_UNITS_UCS_TRAIL_DETAILED) != 0
    {
        update_trail(s.h, class, unit);
    }
    let mut s = S { h, class, unit };
    if s.s(EE_UNITS_UCS_HAS_RIPPLE) != 0 && s.s(EE_UNITS_UCS_RIPPLE_DETAILED) != 0 {
        update_ripples(s.h, class, unit);
    }
}

/// `BombUpdate(unit)`: an SAS explosive's target blinks red every half second while it attacks; then the
/// projectile's update.
pub fn bomb_update<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = S { h, class, unit };
    if s.s(EE_UNITS_UCS_UNIT_TYPE) == UNIT_TYPE_BOMB_FOR_SAS_EXPLOSIVE_EXPERT {
        let goal = s.s(EE_UNITS_UC_GOAL_STATE_SELF) as u64;
        if goal != 0 && s.a(EE_UNITS_UC_GOAL_KIND, goal) == GOAL_ATTACK_UNIT {
            let attacked = s.a(EE_UNITS_UC_ATTACK_TARGET, goal) as u64;
            let target = s.a(EE_UNITS_UCS_COMPLEX_CAST, attacked) as u64;
            if target != 0 {
                let time = (s.s(EE_UNITS_UCS_FLIGHT_TIME) as u32) % 1000;
                if time > 500 {
                    s.ab(EE_UNITS_UCS_SET_TINTED, target, 1);
                    s.a(EE_UNITS_UCS_SET_BLINK_COLOR, target);
                } else {
                    s.ab(EE_UNITS_UCS_SET_TINTED, target, 0);
                }
            }
        }
    }
    s.s(EE_UNITS_UCS_PROJECTILE_UPDATE);
}

/// `LaunchStoredMissile(unit)`: a carrier's first cargo unit (a bomb) dropped at its target and set moving.
pub fn launch_stored<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = S { h, class, unit };
    if s.s(EE_UNITS_UC_CARGO_COUNT) <= 0 {
        return;
    }
    let mut location = [0.0f32; 3];
    let target = s.point(EE_UNITS_UCS_FIND_DROP, 0, 0, 0, &mut location);
    let cargo = s.s(EE_UNITS_UCS_FIRST_CARGO) as u64;
    s.a(EE_UNITS_UCS_UNLOAD_NOW, cargo);
    let mut p = location;
    s.point(EE_UNITS_UCS_LAUNCH_SHOT, target as u64, cargo as i64, 0, &mut p);
    s.ab(EE_UNITS_UC_SET_ANIMATION_OF, cargo, ANIMATION_FLIGHT);
    s.ab(EE_UNITS_UC_START_ACTION, cargo, ACTION_ADVANCE);
    s.a(EE_UNITS_UC_ACTION_CHANGED, cargo);
}

/// `EEIsOnTargetTiles(unit, target)`: whether the unit stands on one of the target's tiles (every part of a
/// multi-part target, its whole footprint).
pub fn on_target_tiles<H: UcHost + ?Sized>(h: &mut H, unit: u64, target: u64) -> bool {
    let mut s = S { h, class: 0, unit: 0 };
    let mut part = s.a(EE_UNITS_UCS_COMPLEX_FIELD, target) as u64;
    let tile = s.a(EE_UNITS_UCS_TILE_OF, unit);
    let mut result = false;
    if part == 0 {
        return tile == s.a(EE_UNITS_UCS_TILE_OF, target);
    }
    loop {
        let previous = s.a(EE_UNITS_UCS_PREVIOUS_PART, part) as u64;
        if previous == 0 {
            break;
        }
        part = previous;
    }
    while part != 0 && !result {
        let size = s.a(EE_UNITS_UCS_TILE_SIZE_OF, part);
        if size == 1 {
            result = tile == s.a(EE_UNITS_UCS_TILE_OF, part);
        } else {
            let mut x = s.a(EE_UNITS_UCS_TILE_X_OF, part) as u32;
            let end_x = x.wrapping_add(size as u32);
            while x < end_x && !result {
                let mut y = s.a(EE_UNITS_UCS_TILE_Y_OF, part) as u32;
                let end_y = y.wrapping_add(size as u32);
                while y < end_y && !result {
                    result = tile == s.abc(EE_UNITS_UC_GET_TILE, 0, i64::from(x as i32), i64::from(y as i32));
                    y = y.wrapping_add(1);
                }
                x = x.wrapping_add(1);
            }
        }
        part = s.a(EE_UNITS_UCS_NEXT_PART, part) as u64;
    }
    result
}

/// Runs body `what` of `class` on `unit` (`a` an object, `f` a point); the choice, or 0.
pub fn run<H: UcHost + ?Sized>(h: &mut H, what: i32, class: u64, unit: u64, a: u64, f: [f32; 3]) -> i64 {
    match what {
        EE_UNITS_UCS_DO_PROJECTILE_CREATED => projectile_created(h, class, unit),
        EE_UNITS_UCS_DO_MISSILE_CREATED => missile_created(h, class, unit),
        EE_UNITS_UCS_DO_PROJECTILE_LAUNCH => return i64::from(projectile_launch(h, class, unit, f)),
        EE_UNITS_UCS_DO_BOMB_LAUNCH => return i64::from(bomb_launch(h, class, unit)),
        EE_UNITS_UCS_DO_MISSILE_LAUNCH => return i64::from(missile_launch(h, class, unit)),
        EE_UNITS_UCS_DO_PROJECTILE_EFFECTS => projectile_effects(h, class, unit),
        EE_UNITS_UCS_DO_BOMB_UPDATE => bomb_update(h, class, unit),
        EE_UNITS_UCS_DO_LAUNCH_STORED => launch_stored(h, class, unit),
        EE_UNITS_UCS_DO_ON_TARGET_TILES => return i64::from(on_target_tiles(h, unit, a)),
        _ => {}
    }
    0
}

#[cfg(not(test))]
pub(crate) mod game {
    use super::*;
    use std::os::raw::c_void;

    pub(crate) use crate::units::uc_body::game::ee_units_uc_host;

    /// The shots' host (`ee_units_uc_host`; the class host's statements pass through it).
    struct GameS;

    impl UcHost for GameS {
        #[allow(clippy::too_many_arguments)]
        fn uc(&mut self, op: i32, class: u64, unit: u64, a: u64, b: i64, c: i64, f: &mut [f32; 4]) -> i64 {
            if let Some(v) = crate::units::tech_cases::uc(op, unit, a) {
                return v;
            }
            if let Some(v) = crate::units::player_upgrades::ucs(op, unit, f) {
                return v;
            }
            if let Some(v) =
                crate::units::uc_view::uc_view(crate::units::uc_view::Kind::Shots, op, class, unit, a, b, c, f)
            {
                return v;
            }
            // SAFETY: the body runs inside the C++ member that handed over its live class and unit
            unsafe { ee_units_uc_host(op, class as usize as *mut c_void, unit as usize as *mut c_void, a, b, c, f) }
        }
    }

    /// The shots' body `what` (`EE_UNITS_UCS_DO_*`) of `class` on `unit`: `a` an object, `f` a point (or NULL);
    /// the choice, or 0.
    ///
    /// # Safety
    /// `class` is a live `EEUnitClass` (or NULL), `unit` a live `EEComplexUnit`, `f` NULL or live, on the simulation
    /// thread.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_ucs_call(
        what: i32,
        class: *mut c_void,
        unit: *mut c_void,
        a: u64,
        f: *const [f32; 3],
    ) -> i64 {
        // SAFETY: the caller's point
        let point = if f.is_null() { [0.0; 3] } else { unsafe { *f } };
        run(&mut GameS, what, class as usize as u64, unit as usize as u64, a, point)
    }
}

#[cfg(test)]
#[path = "ucs_shots_tests.rs"]
mod tests;
