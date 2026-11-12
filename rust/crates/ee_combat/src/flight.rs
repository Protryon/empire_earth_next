//! The physics models' steps of projectiles and missiles (R5b.2, fourth round): `EEPMProjectile::Step0` (the
//! launched shot placed on its arc, `arc` in lib.rs giving the move and the pitch), `EEPMProjectile::Step2` (a tick
//! along the heading, the nose pulled down by gravity), `EEPMMissile::Accelerate` and `EEPMMissile::Home` (the turn
//! toward the drop target by at most the turn rate, the speed by how far off target it points). Pure: the C++ keeps
//! the objects, gathers the inputs (the ground point, the drop target) and sets the unit's location and tile from
//! the result (`EEUpdateUnitTile`).

use crate::geometry::matrix;
use ee_ge::transform::RotationOrder;
use ee_ge::types::{GeQuaternion, GeTransformation, GeVector};
use ee_ge::um::{um_arccos, um_arctan2, um_cos};

/// A flight step's inputs (the capture's CBPJ, CBPS, CBMA, CBMH layout; each step reads its own fields).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatFlightIn {
    /// the unit's location (`mMatrix`, row by row) (Step2, Accelerate, Home)
    pub location: [f32; 16],
    /// the projectile's launch point (`mProjectileStart`) (Step0)
    pub start: [f32; 3],
    /// the drop target's point (Home)
    pub target: [f32; 3],
    /// the projectile's yaw (Step0)
    pub yaw: f32,
    /// the arc's move along the heading and up, and its pitch (`ee_combat_arc`) (Step0)
    pub move_y: f32,
    pub move_z: f32,
    pub pitch: f32,
    /// the class's gravity (`mAcceleration`) (Step2)
    pub gravity: f32,
    /// the tick's seconds
    pub step: f32,
    /// the unit's velocity
    pub speed: f32,
    /// the stats' top speed, acceleration and turn rate (Accelerate, Home)
    pub max_speed: f32,
    pub accel: f32,
    pub max_angular: f32,
    /// the ground point was read (Step2: only at or below height 0) and its height
    pub ground_read: i32,
    pub ground_z: f32,
}

/// A flight step's outputs.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatFlightOut {
    /// the unit moved (Step2 may not; its location is then the input's)
    pub moved: i32,
    /// the new velocity (Accelerate, Home)
    pub speed: f32,
    /// the new location
    pub matrix: [f32; 16],
}

/// the C++'s 1.5707964f and 0.31830987f (1 / pi): the same f32s
const HALF_PI: f32 = std::f32::consts::FRAC_PI_2;
const ONE_OVER_PI: f32 = std::f32::consts::FRAC_1_PI;
/// `kMissileMinimumFactor` (0x4428f0): the slowest a missile turns at, a quarter of its top speed.
const MISSILE_MINIMUM_FACTOR: f32 = 0.25;

fn rows(t: &GeTransformation) -> [f32; 16] {
    let mut out = [0.0f32; 16];
    for (r, row) in t.m.iter().enumerate() {
        out[r * 4..r * 4 + 4].copy_from_slice(row);
    }
    out
}

/// A rotation alone: `SetOrientationYPR`, the position zeroed, the bottom row (0, 0, 0, 1).
fn rotation(yaw: f32, pitch: f32) -> GeTransformation {
    let mut t = GeTransformation::default();
    t.set_orientation(RotationOrder::Ypr, yaw, pitch, 0.0);
    t.m[0][3] = 0.0;
    t.m[1][3] = 0.0;
    t.m[2][3] = 0.0;
    t.m[3] = [0.0, 0.0, 0.0, 1.0];
    t
}

fn translate(t: &mut GeTransformation, v: GeVector) {
    t.m[0][3] += v.x;
    t.m[1][3] += v.y;
    t.m[2][3] += v.z;
}

/// `UMAbs`: `v < 0 ? -v : v`.
fn um_abs(v: f32) -> f32 {
    if v < 0.0 {
        -v
    } else {
        v
    }
}

/// `EEPMProjectile::Step0`: the launch point moved by the arc's move along the yaw, nose at the arc's pitch.
pub fn projectile_launch(i: &EeCombatFlightIn) -> EeCombatFlightOut {
    let mut t = rotation(i.yaw, 0.0);
    let m = t.apply_vector(GeVector { x: 0.0, y: i.move_y, z: i.move_z });
    t.m[0][3] = i.start[0] + m.x;
    t.m[1][3] = i.start[1] + m.y;
    t.m[2][3] = i.start[2] + m.z;
    t.set_orientation(RotationOrder::Ypr, i.yaw, i.pitch, 0.0);
    EeCombatFlightOut { moved: 1, speed: 0.0, matrix: rows(&t) }
}

/// `EEPMProjectile::Step2`: only at or below height 0 and above the ground point, a tick along the heading (the
/// speed over the pitch's cosine, a tenth), the nose pulled down by the class's gravity.
pub fn projectile_step(i: &EeCombatFlightIn) -> EeCombatFlightOut {
    let mut t = matrix(&i.location);
    let unmoved = EeCombatFlightOut { moved: 0, speed: 0.0, matrix: i.location };
    if t.z() > 0.0 || t.z() <= i.ground_z {
        return unmoved;
    }
    let direction = t.apply_vector(GeVector { x: 0.0, y: 1.0, z: 0.0 });
    let pitch = um_arccos(-direction.z) - HALF_PI;
    let yaw = um_arctan2(-direction.x, direction.y);
    let forward = GeVector { x: 0.0, y: i.speed / um_cos(pitch) * 0.1, z: 0.0 };
    let mut direction = t.apply_vector(forward);
    let step = GeVector { x: direction.x * i.step, y: direction.y * i.step, z: direction.z * i.step };
    t.m[0][3] += step.x;
    t.m[1][3] += step.y;
    t.m[2][3] += step.z;
    direction.z -= i.step * i.gravity * 0.2;
    direction.normalize();
    let delta = um_arccos(-direction.z) - HALF_PI - pitch;
    t.set_orientation(RotationOrder::Ypr, yaw, pitch + delta, 0.0);
    EeCombatFlightOut { moved: 1, speed: 0.0, matrix: rows(&t) }
}

/// `EEPMMissile::Accelerate`: faster by the acceleration (capped at the top speed), a tick along the heading.
pub fn missile_accelerate(i: &EeCombatFlightIn) -> EeCombatFlightOut {
    let mut t = matrix(&i.location);
    let mut speed = i.accel * i.step + i.speed;
    if i.max_speed < speed {
        speed = i.max_speed;
    }
    let m = t.apply_vector(GeVector { x: 0.0, y: speed * i.step, z: 0.0 });
    translate(&mut t, m);
    EeCombatFlightOut { moved: 1, speed, matrix: rows(&t) }
}

/// `EEPMMissile::Home`: turned toward the target by at most the turn rate a tick (the quaternions' slerp), its
/// speed eased toward the top speed scaled by how far off target it points (at least a quarter), a tick ahead.
// the dot product's clamp as the C++ writes it
#[allow(clippy::manual_clamp)]
pub fn missile_home(i: &EeCombatFlightIn) -> EeCombatFlightOut {
    let location = matrix(&i.location);
    let current = GeQuaternion::from_transformation(&location);
    let turn = i.max_angular * i.step;
    let mut direction =
        GeVector { x: i.target[0] - location.x(), y: i.target[1] - location.y(), z: i.target[2] - location.z() };
    direction.normalize();
    let heading = um_arctan2(-direction.x, direction.y);
    let pitch = um_arccos(-direction.z) - HALF_PI;
    let wanted = GeQuaternion::from_transformation(&rotation(heading, pitch));
    let mut difference = um_abs(wanted.x - current.x);
    for d in [um_abs(wanted.y - current.y), um_abs(wanted.z - current.z), um_abs(wanted.w - current.w)] {
        if d > difference {
            difference = d;
        }
    }
    let t = if difference > turn { turn / difference } else { 1.0 };
    let mut new = current.slerp(&wanted, t).to_transformation();
    new.m[0][3] = location.x();
    new.m[1][3] = location.y();
    new.m[2][3] = location.z();
    let ahead = new.apply_vector(GeVector { x: 0.0, y: 1.0, z: 0.0 });
    let mut dot = ahead.z * direction.z + ahead.y * direction.y + ahead.x * direction.x;
    if dot > 1.0 {
        dot = 1.0;
    } else if dot < -1.0 {
        dot = -1.0;
    }
    let angle = um_arccos(dot) * ONE_OVER_PI;
    let mut factor = 1.0 - angle * angle;
    if factor < MISSILE_MINIMUM_FACTOR {
        factor = MISSILE_MINIMUM_FACTOR;
    }
    let limit = factor * i.max_speed;
    let mut speed = i.speed;
    if speed < limit {
        speed += i.accel * i.step;
        if speed > limit {
            speed = limit;
        }
    } else {
        speed -= i.accel * i.step;
        if speed < limit {
            speed = limit;
        }
    }
    let m = new.apply_vector(GeVector { x: 0.0, y: speed * i.step, z: 0.0 });
    translate(&mut new, m);
    EeCombatFlightOut { moved: 1, speed, matrix: rows(&new) }
}

// ---- the C ABI

// (one function each: cbindgen does not expand macros)
/// `EEPMProjectile::Step0`'s placement on the arc.
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_projectile_launch(input: *const EeCombatFlightIn) -> EeCombatFlightOut {
    projectile_launch(unsafe { &*input })
}

/// `EEPMProjectile::Step2`'s tick.
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_projectile_step(input: *const EeCombatFlightIn) -> EeCombatFlightOut {
    projectile_step(unsafe { &*input })
}

/// `EEPMMissile::Accelerate`'s tick.
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_missile_accelerate(input: *const EeCombatFlightIn) -> EeCombatFlightOut {
    missile_accelerate(unsafe { &*input })
}

/// `EEPMMissile::Home`'s tick.
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_missile_home(input: *const EeCombatFlightIn) -> EeCombatFlightOut {
    missile_home(unsafe { &*input })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn identity_at(x: f32, y: f32, z: f32) -> [f32; 16] {
        [1.0, 0.0, 0.0, x, 0.0, 1.0, 0.0, y, 0.0, 0.0, 1.0, z, 0.0, 0.0, 0.0, 1.0]
    }

    fn close(a: f32, b: f32) -> bool {
        (a - b).abs() < 1e-3
    }

    #[test]
    fn a_projectile_starts_on_its_arc() {
        let i = EeCombatFlightIn {
            start: [5.0, 6.0, 1.0],
            yaw: 0.0,
            move_y: 2.0,
            move_z: 0.5,
            pitch: 0.0,
            ..Default::default()
        };
        let out = projectile_launch(&i);
        assert!(
            close(out.matrix[3], 5.0) && close(out.matrix[7], 8.0) && close(out.matrix[11], 1.5),
            "{:?}",
            out.matrix
        );
        assert_eq!(&out.matrix[12..], &[0.0, 0.0, 0.0, 1.0]);
        assert_eq!(out.moved, 1);
    }

    #[test]
    fn a_projectile_steps_only_below_zero_and_above_the_ground() {
        let mut i = EeCombatFlightIn {
            location: identity_at(1.0, 1.0, 0.5),
            gravity: 9.8,
            step: 0.1,
            speed: 10.0,
            ground_read: 0,
            ..Default::default()
        };
        assert_eq!(projectile_step(&i), EeCombatFlightOut { moved: 0, speed: 0.0, matrix: i.location });
        i.location = identity_at(1.0, 1.0, -0.5);
        i.ground_read = 1;
        i.ground_z = -0.4;
        assert_eq!(projectile_step(&i).moved, 0);
        i.ground_z = -2.0;
        let out = projectile_step(&i);
        assert_eq!(out.moved, 1);
        // level: a tenth of the speed for a tick along +y, the nose then pitched down
        assert!(close(out.matrix[7], 1.1) && close(out.matrix[11], -0.5), "{:?}", out.matrix);
        assert!(out.matrix[9] < 0.0, "{:?}", out.matrix);
    }

    #[test]
    fn a_missile_accelerates_to_its_top_speed() {
        let i = EeCombatFlightIn {
            location: identity_at(0.0, 0.0, 2.0),
            accel: 10.0,
            step: 0.1,
            speed: 3.0,
            max_speed: 3.5,
            ..Default::default()
        };
        let out = missile_accelerate(&i);
        assert_eq!(out.speed, 3.5);
        assert!(close(out.matrix[7], 0.35) && close(out.matrix[11], 2.0));
        let i = EeCombatFlightIn { max_speed: 10.0, ..i };
        assert!(close(missile_accelerate(&i).speed, 4.0));
    }

    #[test]
    fn a_missile_homes_on_its_target() {
        // straight ahead: no turn, faster toward the top speed
        let i = EeCombatFlightIn {
            location: identity_at(0.0, 0.0, 0.0),
            target: [0.0, 10.0, 0.0],
            accel: 5.0,
            step: 0.1,
            speed: 2.0,
            max_speed: 4.0,
            max_angular: 1.0,
            ..Default::default()
        };
        let out = missile_home(&i);
        assert!(close(out.speed, 2.5), "{}", out.speed);
        assert!(close(out.matrix[3], 0.0) && close(out.matrix[7], 0.25), "{:?}", out.matrix);
        // a target behind: the turn is limited, the speed held at a quarter of the top
        let i = EeCombatFlightIn { target: [0.0, -10.0, 0.0], speed: 4.0, ..i };
        let out = missile_home(&i);
        assert!(close(out.speed, 3.5), "{}", out.speed);
        assert!(out.matrix[5] > 0.5, "{:?}", out.matrix);
    }

    #[test]
    fn the_capture_replays_flight_steps() {
        use crate::capture::{encode, records, replay};
        let i = EeCombatFlightIn {
            location: identity_at(0.0, 0.0, -1.0),
            target: [3.0, 4.0, 1.0],
            yaw: 0.3,
            move_y: 1.0,
            move_z: 0.2,
            pitch: 0.1,
            gravity: 9.8,
            step: 0.1,
            speed: 2.0,
            max_speed: 4.0,
            accel: 5.0,
            max_angular: 1.0,
            ground_read: 1,
            ground_z: -3.0,
            ..Default::default()
        };
        type Step = fn(&EeCombatFlightIn) -> EeCombatFlightOut;
        let steps: [(&[u8; 4], Step); 4] = [
            (b"CBPJ", projectile_launch),
            (b"CBPS", projectile_step),
            (b"CBMA", missile_accelerate),
            (b"CBMH", missile_home),
        ];
        for (name, step) in steps {
            let mut bytes = encode(name, &i, &step(&i));
            assert_eq!(replay(&records(&bytes).unwrap()[0]), Some(Ok(true)));
            let n = bytes.len();
            bytes[n - 1] ^= 0x40;
            assert_eq!(replay(&records(&bytes).unwrap()[0]), Some(Ok(false)));
        }
    }
}
