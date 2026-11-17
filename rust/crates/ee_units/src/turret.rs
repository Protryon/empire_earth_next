//! `EEUnitClass::TurnTurrets` (0x638948) and `TurnTurretsBack` (0x6364f3): a unit's turret parts turned for one
//! tick. With an attack, heal or calamity goal each part turns toward the goal's point by at most its turn rate for
//! the tick; with any other goal (or none) each part turns back toward straight ahead, a dead unit's not at all.
//!
//! The C++ gathers the unit's location matrix, its stats' scale and turret turn rate, the tick's seconds, the goal's
//! point and each part's position, transformation and current angle ([`EeUnitsTurretPart`]); this turns the angles
//! in place and the C++ stores each back where it differs (the original's `if (theAngle != old) set`).
//!
//! Determinism: the original's `float` operations in order, the transformations' products in the header inlines'
//! summation order (ee_ge `apply_point`, `apply_vector`), `SetOrientationYPR` and `UMArcTan2` as ee_ge's (the C++
//! forwards both there), `GEVector::Normalize`'s 0.0001 cut.

use ee_render::ge::transform::RotationOrder;
use ee_render::ge::types::{Ge3DPoint, GeTransformation, GeVector};
use ee_render::ge::um::um_arctan2;

/// The original's pi and two pi (`3.1415927f`, `6.2831855f`: the same floats).
const PI: f32 = std::f32::consts::PI;
const TWO_PI: f32 = std::f32::consts::TAU;

/// One turret part.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsTurretPart {
    /// the part's `mPosition` (model space, before the stats' scale)
    pub position: [f32; 3],
    /// the part's `mTransformation`
    pub transform: [[f32; 4]; 4],
    /// the unit's `mTurretAngles[i]`: the angle before the tick in, after it out
    pub angle: f32,
}

/// The unit's numbers for the tick.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsTurretsIn {
    /// true: the goal is an attack, heal or calamity goal and `target_x`, `target_y` its point; false: turn back
    pub aim: bool,
    /// `mHitPoints > 0` (turning back only)
    pub alive: bool,
    /// `mStats->mTurretMaxAngularVelocity`
    pub turn_rate: f32,
    /// `gPhysics->mTickSeconds`
    pub tick_seconds: f32,
    /// `mStats->GetScale()`
    pub scale: f32,
    /// `mPhysicalModel.GetLocation()`
    pub location: [[f32; 4]; 4],
    pub target_x: f32,
    pub target_y: f32,
}

/// The original's wrap of an angle into [-pi, pi] (one step either way).
fn wrap(angle: f32) -> f32 {
    if angle < -PI {
        angle + TWO_PI
    } else if angle > PI {
        angle - TWO_PI
    } else {
        angle
    }
}

/// `TurnTurretsBack`: each angle toward 0 by at most `step`, not past it.
fn turn_back(step: f32, parts: &mut [EeUnitsTurretPart]) {
    for part in parts {
        let mut value = part.angle;
        if value > 0.0 {
            value -= step;
            if value < 0.0 {
                value = 0.0;
            }
        } else if value < 0.0 {
            value += step;
            if value > 0.0 {
                value = 0.0;
            }
        }
        if part.angle != value {
            part.angle = value;
        }
    }
}

/// `TurnTurrets`' turn of one part toward the target point.
fn aim_part(location: &GeTransformation, input: &EeUnitsTurretsIn, step: f32, part: &EeUnitsTurretPart) -> f32 {
    let offset = Ge3DPoint {
        x: input.scale * part.position[0],
        y: input.scale * part.position[1],
        z: input.scale * part.position[2],
    };
    let world = location.apply_point(offset);
    let mut direction = GeVector { x: input.target_x - world.x, y: input.target_y - world.y, z: 0.0 };
    direction.normalize();
    let heading = um_arctan2(-direction.x, direction.y);
    let old = part.angle;
    let mut rotation = GeTransformation::IDENTITY;
    rotation.set_orientation(RotationOrder::Ypr, old, 0.0, 0.0);
    let rotated = rotation.apply_vector(GeVector { x: 0.0, y: 1.0, z: 0.0 });
    let part_facing = GeTransformation { m: part.transform }.apply_vector(rotated);
    let mut facing = location.apply_vector(part_facing);
    facing.z = 0.0;
    facing.normalize();
    let mut angle = wrap(heading - um_arctan2(-facing.x, facing.y));
    if angle > 0.0 {
        angle = if angle > step { old + step } else { angle + old };
    } else if angle < 0.0 {
        angle = -angle;
        angle = if angle > step { old - step } else { old - angle };
    } else {
        angle = old;
    }
    wrap(angle)
}

/// One tick of the unit's turrets: `parts[i].angle` is the value the unit's `mTurretAngles[i]` holds after it.
pub fn turn_turrets(input: &EeUnitsTurretsIn, parts: &mut [EeUnitsTurretPart]) {
    let step = input.turn_rate * input.tick_seconds;
    if !input.aim {
        if input.alive {
            turn_back(step, parts);
        }
        return;
    }
    let location = GeTransformation { m: input.location };
    for part in parts.iter_mut() {
        let angle = aim_part(&location, input, step, part);
        if angle != part.angle {
            part.angle = angle;
        }
    }
}

/// `TurnTurrets` / `TurnTurretsBack` for one unit (see [`turn_turrets`]).
///
/// # Safety
/// `input` is valid; `parts` holds `count` parts (NULL when `count` is 0).
#[no_mangle]
pub unsafe extern "C" fn ee_units_turn_turrets(
    input: *const EeUnitsTurretsIn,
    parts: *mut EeUnitsTurretPart,
    count: u32,
) {
    if count == 0 {
        return;
    }
    // SAFETY: the caller's contract
    let (input, parts) = unsafe { (&*input, std::slice::from_raw_parts_mut(parts, count as usize)) };
    turn_turrets(input, parts);
}

#[cfg(test)]
#[path = "turret_tests.rs"]
mod tests;
