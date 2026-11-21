//! The projectile's and the missile's steps and the unit's group-aware rates (the world lane, round 2; were
//! Physics Thread/EEPhysicsModel.cpp's `PMProjectileStep0`, `PMProjectileStep2`, `PMMissileAccelerate`,
//! `PMMissileHome`, `PMMissileStep0` and `EEComplexUnit::GetMaxAngularVelocity` .. `GetInverseAngularAcceleration`).
//!
//! The steps gather the flight's inputs from the unit (through [`FlightHost`], each read where the C++ read it), run
//! ee_combat's arithmetic (combat::arc, flight.rs) and write the combat lane's capture records (CBPA, CBPJ, CBPS,
//! CBMA, CBMH: the input's and the output's bytes, `EECombatCapture`) at the C++'s points, before the tile update.

// the C++'s negated tests (NaN goes their way)
#![allow(clippy::neg_cmp_op_on_partial_ord)]

use crate::combat::flight::{
    missile_accelerate, missile_home, projectile_launch, projectile_step, EeCombatFlightIn, EeCombatFlightOut,
};
use crate::combat::{arc, EeCombatArcIn, EeCombatArcOut};
use crate::units::ag_fields::EeUnitsActiveGroupFields;
use crate::world::stats_fields::EeUnitsUnitClassUpgradeFields;

/// The capture tags (`EE_COMBAT_TAG`: the four characters, little-endian).
const TAG_ARC: u32 = u32::from_le_bytes(*b"CBPA");
const TAG_LAUNCH: u32 = u32::from_le_bytes(*b"CBPJ");
const TAG_PROJECTILE_STEP: u32 = u32::from_le_bytes(*b"CBPS");
const TAG_ACCELERATE: u32 = u32::from_le_bytes(*b"CBMA");
const TAG_HOME: u32 = u32::from_le_bytes(*b"CBMH");

/// The unit's reads and writes the steps make (the game's: pm_host.rs).
pub trait FlightHost {
    /// `mPhysicalModel.GetLocation().mMatrix`, row by row
    fn location(&mut self, obj: u64) -> [f32; 16];
    /// `mInitialContactLocation = p`
    fn set_contact_location(&mut self, obj: u64, p: [f32; 3]);
    /// `mFlightTime`
    fn flight_time(&mut self, obj: u64) -> i32;
    /// `mUnitClass->mAcceleration` (the projectile's gravity)
    fn class_gravity(&mut self, obj: u64) -> f32;
    /// `mOrientation`: yaw, pitch, roll
    fn orientation(&mut self, obj: u64) -> [f32; 3];
    /// `mUnitClass->mUnitType == kUnitTypeMeteor`
    fn is_meteor(&mut self, obj: u64) -> bool;
    /// `GetMaxVelocity()` (its active group's when it moves together)
    fn max_velocity(&mut self, obj: u64) -> f32;
    /// `mPlayer->mUnitClassUpgrades[the launcher's (else its own) class's type]->mLaunchSpeed`
    fn launch_speed(&mut self, obj: u64, launcher: bool) -> f32;
    /// `mProjectileStart`
    fn projectile_start(&mut self, obj: u64) -> [f32; 3];
    /// `gPhysics->mTickSeconds`
    fn tick_seconds(&mut self) -> f32;
    /// `gWorld.mMap->GetGroundPoint(p, ground)`'s z
    fn ground_z(&mut self, p: [f32; 3]) -> f32;
    /// `mVelocity`
    fn velocity(&mut self, obj: u64) -> f32;
    /// `mStats->GetMaxVelocity()`, `GetAcceleration()`, `GetMaxAngularVelocity()`
    fn stats(&mut self, obj: u64) -> [f32; 3];
    /// `EECombatFindDropLocation(unit, NULL, target)`'s target
    fn drop_target(&mut self, obj: u64) -> [f32; 3];
    /// `SetVelocity(v)`
    fn set_velocity(&mut self, obj: u64, v: f32);
    /// `EEUpdateUnitTile(&matrix, unit, &x, &y)`
    fn update_tile(&mut self, obj: u64, m: &[f32; 16]);
    /// `EECombatCapture(tag, in, out)`: a record of the two byte ranges while the capture runs
    fn capture(&mut self, tag: u32, input: &[u8], output: &[u8]);
}

/// A record's bytes (the records are `#[repr(C)]`, every field 4 bytes: no padding).
fn bytes<T: Copy>(v: &T) -> &[u8] {
    // SAFETY: a plain record of 4-byte fields, read as its own bytes
    unsafe { std::slice::from_raw_parts((v as *const T).cast::<u8>(), std::mem::size_of::<T>()) }
}

fn capture<H: FlightHost + ?Sized, I: Copy, O: Copy>(h: &mut H, tag: u32, input: &I, output: &O) {
    h.capture(tag, bytes(input), bytes(output));
}

fn position(m: &[f32; 16]) -> [f32; 3] {
    [m[3], m[7], m[11]]
}

/// `EECombatArc(in)`: the arc and its CBPA record.
fn combat_arc<H: FlightHost + ?Sized>(h: &mut H, input: &EeCombatArcIn) -> EeCombatArcOut {
    let out = arc(input);
    capture(h, TAG_ARC, input, &out);
    out
}

/// `EEPMProjectile::Step0`: the launched projectile on its ballistic arc from the launch point for its flight time so
/// far, nose along the arc (a meteor straight at its top speed, without gravity).
pub fn projectile_step0<H: FlightHost + ?Sized>(h: &mut H, obj: u64) {
    let location = h.location(obj);
    h.set_contact_location(obj, position(&location));
    let mut input = EeCombatArcIn {
        flight_time_ms: h.flight_time(obj),
        gravity: h.class_gravity(obj),
        pitch: h.orientation(obj)[1],
        ..Default::default()
    };
    input.meteor = i32::from(h.is_meteor(obj));
    if input.meteor != 0 {
        input.max_velocity = h.max_velocity(obj);
    } else {
        input.launcher_speed = h.launch_speed(obj, true);
        if !(input.launcher_speed > 0.001f32) {
            // read only for the fallback
            input.own_speed = h.launch_speed(obj, false);
        }
    }
    let arc = combat_arc(h, &input);
    let flight = EeCombatFlightIn {
        start: h.projectile_start(obj),
        yaw: h.orientation(obj)[0],
        move_y: arc.move_y,
        move_z: arc.move_z,
        pitch: arc.pitch,
        ..Default::default()
    };
    let out = projectile_launch(&flight);
    capture(h, TAG_LAUNCH, &flight, &out);
    h.update_tile(obj, &out.matrix);
}

/// `EEPMProjectile::Step2`: a tick along the heading, the nose pulled down by the class's gravity; only at or below
/// height 0 and above the ground point (read only at or below height 0).
pub fn projectile_step2<H: FlightHost + ?Sized>(h: &mut H, obj: u64) {
    let location = h.location(obj);
    let mut input = EeCombatFlightIn {
        location,
        gravity: h.class_gravity(obj),
        step: h.tick_seconds(),
        speed: h.velocity(obj),
        ..Default::default()
    };
    if !(location[11] > 0.0f32) {
        input.ground_read = 1;
        input.ground_z = h.ground_z(position(&location));
    }
    let out: EeCombatFlightOut = projectile_step(&input);
    capture(h, TAG_PROJECTILE_STEP, &input, &out);
    if out.moved == 0 {
        return;
    }
    h.update_tile(obj, &out.matrix);
}

/// `EEPMMissile::Accelerate`: faster along its heading (capped at its top speed), moved by one tick.
fn missile_accelerate_step<H: FlightHost + ?Sized>(h: &mut H, obj: u64) {
    let location = h.location(obj);
    let step = h.tick_seconds();
    let speed = h.velocity(obj);
    let [max_speed, accel, _] = h.stats(obj);
    let input = EeCombatFlightIn { location, step, speed, max_speed, accel, ..Default::default() };
    let out = missile_accelerate(&input);
    h.set_velocity(obj, out.speed);
    capture(h, TAG_ACCELERATE, &input, &out);
    h.update_tile(obj, &out.matrix);
}

/// `EEPMMissile::Home`: turned toward its drop target by at most its turn rate a tick, slowed the further it points
/// off target, moved by one tick.
fn missile_home_step<H: FlightHost + ?Sized>(h: &mut H, obj: u64) {
    let target = h.drop_target(obj);
    let location = h.location(obj);
    let step = h.tick_seconds();
    let speed = h.velocity(obj);
    let [max_speed, accel, max_angular] = h.stats(obj);
    let input = EeCombatFlightIn { location, target, step, speed, max_speed, accel, max_angular, ..Default::default() };
    let out = missile_home(&input);
    h.set_velocity(obj, out.speed);
    capture(h, TAG_HOME, &input, &out);
    h.update_tile(obj, &out.matrix);
}

/// `EEPMMissile::Step0`: the missile's position as its contact point, then straight on while its boost time lasts,
/// else homing on its target.
pub fn missile_step0<H: FlightHost + ?Sized>(h: &mut H, obj: u64) {
    let location = h.location(obj);
    h.set_contact_location(obj, position(&location));
    if h.flight_time(obj) > 0 {
        missile_accelerate_step(h, obj);
    } else {
        missile_home_step(h, obj);
    }
}

// ---- the trample's minimum squared distance (world table entry 79, set with the models; was `gPhysicsModelsF`)

static TRAMPLE_MIN: std::sync::atomic::AtomicU32 = std::sync::atomic::AtomicU32::new(0x3f00_0000);

/// The trample's minimum squared distance (0.5 until the models are made).
pub fn trample_min() -> f32 {
    f32::from_bits(TRAMPLE_MIN.load(std::sync::atomic::Ordering::Relaxed))
}

/// `EEInitPhysicsModels`: the world table's trample minimum.
#[no_mangle]
pub extern "C" fn ee_units_pm_set_trample_min(value: f32) {
    TRAMPLE_MIN.store(value.to_bits(), std::sync::atomic::Ordering::Relaxed);
}

// ---- the unit's group-aware rates (`EEComplexUnit::GetMaxVelocity` .. and `EEActiveGroup::Get*(unit)`)

/// `GetMaxVelocity()`
pub const EE_UNITS_PM_RATE_MAX_VELOCITY: i32 = 0;
/// `GetAcceleration()`
pub const EE_UNITS_PM_RATE_ACCELERATION: i32 = 1;
/// `GetInverseAcceleration()`
pub const EE_UNITS_PM_RATE_INVERSE_ACCELERATION: i32 = 2;
/// `GetAngularAcceleration()`
pub const EE_UNITS_PM_RATE_ANGULAR_ACCELERATION: i32 = 3;
/// `GetMaxAngularVelocity()`
pub const EE_UNITS_PM_RATE_MAX_ANGULAR_VELOCITY: i32 = 4;
/// `GetInverseAngularAcceleration()`
pub const EE_UNITS_PM_RATE_INVERSE_ANGULAR_ACCELERATION: i32 = 5;

/// The unit's rate `which`: its active group's when the group moves together, else its stats'.
pub fn rate(group: Option<&EeUnitsActiveGroupFields>, stats: &EeUnitsUnitClassUpgradeFields, which: i32) -> f32 {
    let together = group.filter(|g| g.mMovesTogether);
    match which {
        EE_UNITS_PM_RATE_MAX_VELOCITY => together.map_or(stats.mMaxVelocity, |g| g.mSpeed),
        EE_UNITS_PM_RATE_ACCELERATION => together.map_or(stats.mAcceleration, |g| g.mAcceleration),
        EE_UNITS_PM_RATE_INVERSE_ACCELERATION => {
            together.map_or(stats.mInverseAcceleration, |g| g.mInverseAcceleration)
        }
        EE_UNITS_PM_RATE_ANGULAR_ACCELERATION => together.map_or(stats.mAngularAcceleration, |g| g.mTurnAcceleration),
        EE_UNITS_PM_RATE_MAX_ANGULAR_VELOCITY => together.map_or(stats.mMaxAngularVelocity, |g| g.mTurnSpeed),
        _ => together.map_or(stats.mInverseAngularAcceleration, |g| g.mInverseTurnAcceleration),
    }
}

/// The live unit's rate `which` (`EEComplexUnit::GetMaxAngularVelocity` ..: `mActiveGroup`'s record, `mStats`'s).
///
/// # Safety
/// `unit` is a live `EEComplexUnit` with its stats; the world layout is registered.
#[cfg(not(test))]
pub unsafe fn unit_rate(unit: u64, which: i32) -> f32 {
    let l = crate::world::layout::layout();
    let at = unit as usize;
    // SAFETY: the caller's contract: the unit's pointer members and the records they point at
    unsafe {
        let group = std::ptr::read((at + l.complex_active_group as usize) as *const usize);
        let stats = std::ptr::read((at + l.unit_stats as usize) as *const usize);
        let stats = &*((stats + l.stats_fields as usize) as *const EeUnitsUnitClassUpgradeFields);
        let group =
            (group != 0).then(|| &*((group + l.active_group_fields as usize) as *const EeUnitsActiveGroupFields));
        rate(group, stats, which)
    }
}

/// `EEComplexUnit::GetMaxAngularVelocity()` .. `GetInverseAngularAcceleration()` (the `EE_UNITS_PM_RATE_*`).
///
/// # Safety
/// As [`unit_rate`].
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_units_pm_unit_rate(unit: *const std::ffi::c_void, which: i32) -> f32 {
    // SAFETY: the caller's contract
    unsafe { unit_rate(unit as u64, which) }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// A unit whose reads are fields and whose calls are logged.
    #[derive(Default)]
    struct Unit {
        location: [f32; 16],
        contact: Option<[f32; 3]>,
        flight_time: i32,
        gravity: f32,
        orientation: [f32; 3],
        meteor: bool,
        max_velocity: f32,
        launcher: f32,
        own: f32,
        start: [f32; 3],
        tick: f32,
        ground: f32,
        velocity: f32,
        stats: [f32; 3],
        target: [f32; 3],
        log: Vec<String>,
        captures: Vec<(u32, usize, usize)>,
        tile: Option<[f32; 16]>,
    }

    impl FlightHost for Unit {
        fn location(&mut self, _: u64) -> [f32; 16] {
            self.location
        }
        fn set_contact_location(&mut self, _: u64, p: [f32; 3]) {
            self.contact = Some(p);
        }
        fn flight_time(&mut self, _: u64) -> i32 {
            self.flight_time
        }
        fn class_gravity(&mut self, _: u64) -> f32 {
            self.gravity
        }
        fn orientation(&mut self, _: u64) -> [f32; 3] {
            self.orientation
        }
        fn is_meteor(&mut self, _: u64) -> bool {
            self.meteor
        }
        fn max_velocity(&mut self, _: u64) -> f32 {
            self.log.push("max_velocity".into());
            self.max_velocity
        }
        fn launch_speed(&mut self, _: u64, launcher: bool) -> f32 {
            self.log.push(format!("launch {launcher}"));
            if launcher {
                self.launcher
            } else {
                self.own
            }
        }
        fn projectile_start(&mut self, _: u64) -> [f32; 3] {
            self.start
        }
        fn tick_seconds(&mut self) -> f32 {
            self.tick
        }
        fn ground_z(&mut self, _: [f32; 3]) -> f32 {
            self.log.push("ground".into());
            self.ground
        }
        fn velocity(&mut self, _: u64) -> f32 {
            self.velocity
        }
        fn stats(&mut self, _: u64) -> [f32; 3] {
            self.stats
        }
        fn drop_target(&mut self, _: u64) -> [f32; 3] {
            self.log.push("drop".into());
            self.target
        }
        fn set_velocity(&mut self, _: u64, v: f32) {
            self.log.push(format!("velocity {v}"));
            self.velocity = v;
        }
        fn update_tile(&mut self, _: u64, m: &[f32; 16]) {
            self.log.push("tile".into());
            self.tile = Some(*m);
        }
        fn capture(&mut self, tag: u32, input: &[u8], output: &[u8]) {
            self.log.push(String::from_utf8_lossy(&tag.to_le_bytes()).into_owned());
            self.captures.push((tag, input.len(), output.len()));
        }
    }

    fn identity_at(x: f32, y: f32, z: f32) -> [f32; 16] {
        [1.0, 0.0, 0.0, x, 0.0, 1.0, 0.0, y, 0.0, 0.0, 1.0, z, 0.0, 0.0, 0.0, 1.0]
    }

    #[test]
    fn projectile_step0_launches_on_the_arc_of_the_launcher_speed() {
        let mut u = Unit {
            location: identity_at(1.0, 2.0, 3.0),
            flight_time: 500,
            gravity: 9.8,
            orientation: [0.25, 0.5, 0.0],
            launcher: 0.0005,
            own: 20.0,
            start: [10.0, 20.0, 5.0],
            ..Default::default()
        };
        projectile_step0(&mut u, 1);
        assert_eq!(u.contact, Some([1.0, 2.0, 3.0]));
        // the launcher's speed is too small: the projectile's own is read
        assert_eq!(u.log, ["launch true", "launch false", "CBPA", "CBPJ", "tile"]);
        let arc_in = EeCombatArcIn {
            flight_time_ms: 500,
            gravity: 9.8,
            pitch: 0.5,
            own_speed: 20.0,
            launcher_speed: 0.0005,
            ..Default::default()
        };
        let a = arc(&arc_in);
        let expect = projectile_launch(&EeCombatFlightIn {
            start: [10.0, 20.0, 5.0],
            yaw: 0.25,
            move_y: a.move_y,
            move_z: a.move_z,
            pitch: a.pitch,
            ..Default::default()
        });
        assert_eq!(u.tile, Some(expect.matrix));
        assert_eq!(u.captures[0], (TAG_ARC, 28, 12));
        assert_eq!(
            u.captures[1],
            (TAG_LAUNCH, std::mem::size_of::<EeCombatFlightIn>(), std::mem::size_of::<EeCombatFlightOut>())
        );
    }

    #[test]
    fn a_meteor_reads_its_top_speed_and_no_launch_speed() {
        let mut u = Unit { meteor: true, max_velocity: 30.0, ..Default::default() };
        projectile_step0(&mut u, 1);
        assert_eq!(u.log, ["max_velocity", "CBPA", "CBPJ", "tile"]);
    }

    #[test]
    fn projectile_step2_reads_the_ground_only_at_or_below_height_0() {
        let mut u = Unit { location: identity_at(5.0, 5.0, 2.0), ..Default::default() };
        projectile_step2(&mut u, 1);
        // above height 0: no ground read, no move
        assert_eq!(u.log, ["CBPS"]);
        let mut u = Unit {
            location: identity_at(5.0, 5.0, -1.0),
            ground: -5.0,
            velocity: 10.0,
            tick: 0.1,
            ..Default::default()
        };
        projectile_step2(&mut u, 1);
        assert_eq!(u.log, ["ground", "CBPS", "tile"]);
    }

    #[test]
    fn the_missile_boosts_then_homes() {
        let mut u = Unit {
            location: identity_at(0.0, 0.0, 10.0),
            flight_time: 3,
            tick: 0.1,
            velocity: 1.0,
            stats: [50.0, 20.0, 1.0],
            ..Default::default()
        };
        missile_step0(&mut u, 1);
        assert_eq!(u.contact, Some([0.0, 0.0, 10.0]));
        assert_eq!(u.log, ["velocity 3", "CBMA", "tile"]);
        let mut u = Unit {
            location: identity_at(0.0, 0.0, 10.0),
            target: [5.0, 5.0, 0.0],
            tick: 0.1,
            velocity: 1.0,
            stats: [50.0, 20.0, 1.0],
            ..Default::default()
        };
        missile_step0(&mut u, 1);
        assert_eq!(u.log[0], "drop");
        assert_eq!(&u.log[2..], ["CBMH", "tile"]);
    }

    #[test]
    fn the_trample_minimum_starts_at_a_half() {
        assert_eq!(trample_min(), 0.5);
    }

    #[test]
    fn the_rates_are_the_groups_only_when_it_moves_together() {
        let stats = EeUnitsUnitClassUpgradeFields {
            mMaxVelocity: 1.0,
            mAcceleration: 2.0,
            mInverseAcceleration: 3.0,
            mAngularAcceleration: 4.0,
            mMaxAngularVelocity: 5.0,
            mInverseAngularAcceleration: 6.0,
            ..Default::default()
        };
        let mut group = EeUnitsActiveGroupFields {
            mSpeed: 11.0,
            mAcceleration: 12.0,
            mInverseAcceleration: 13.0,
            mTurnAcceleration: 14.0,
            mTurnSpeed: 15.0,
            mInverseTurnAcceleration: 16.0,
            ..Default::default()
        };
        for which in 0..6 {
            assert_eq!(rate(None, &stats, which), (which + 1) as f32);
            assert_eq!(rate(Some(&group), &stats, which), (which + 1) as f32);
        }
        group.mMovesTogether = true;
        for which in 0..6 {
            assert_eq!(rate(Some(&group), &stats, which), (which + 11) as f32);
        }
    }
}
