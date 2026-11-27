//! An effect run's own values (game core lane, round 17, second pass): the points and transformations
//! `EESMEffectRun` (Scenario Manager/EESMEffect.cpp) kept between its world calls, the run's record now (its second
//! base, right after the host context). The statements that only set or read them are [`op`]'s; the C++ statements
//! that hand them to a C++ call read the record in place.

// the record keeps the C++ member names
#![allow(non_snake_case)]

use ee_base::math::types::{Ge2DPoint, Ge3DPoint, GeTransformation};

use crate::scenario::effect::EE_SCENARIO_WORLD_CAMERA_POINT;
use crate::scenario::world_compose::*;
use crate::scenario::world_route::{ee_scenario_bits_f32 as float, ee_scenario_f32_bits as bits};

/// The run's values (`EESMEffectRun`'s second base): the calamity's point, the camera move's end and start, the task
/// placement's point, the unit's transformation and the terrain's roll and pitch, the facing's transformation and
/// point, the ground's transformation, Create Units' location and transformation.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeScenarioRunState {
    pub mPoint: Ge3DPoint,
    pub mEnd: Ge3DPoint,
    pub mStart: Ge3DPoint,
    pub mPlace: Ge2DPoint,
    pub mTransformation: GeTransformation,
    pub mRoll: f32,
    pub mPitch: f32,
    pub mFace: GeTransformation,
    pub mFacePoint: Ge3DPoint,
    pub mGround: GeTransformation,
    pub mCreateLocation: Ge2DPoint,
    pub mCreateTransform: GeTransformation,
}

impl Default for EeScenarioRunState {
    /// The C++ members' defaults: the points 0, the transformations the identity.
    fn default() -> Self {
        let i = GeTransformation::IDENTITY;
        let p = Ge3DPoint::default();
        EeScenarioRunState {
            mPoint: p,
            mEnd: p,
            mStart: p,
            mPlace: Ge2DPoint::default(),
            mTransformation: i,
            mRoll: 0.0,
            mPitch: 0.0,
            mFace: i,
            mFacePoint: p,
            mGround: i,
            mCreateLocation: Ge2DPoint::default(),
            mCreateTransform: i,
        }
    }
}

/// A new run's values (`EESMEffectRun`'s constructor).
#[no_mangle]
pub extern "C" fn ee_scenario_run_state_new() -> EeScenarioRunState {
    EeScenarioRunState::default()
}

/// `GE3DPoint(t)`: a transformation's translation.
fn position(t: &GeTransformation) -> [f32; 3] {
    [t.m[0][3], t.m[1][3], t.m[2][3]]
}

/// Whether [`op`] answers the world operation `op` (it is an effect run's).
pub fn is_op(op: u32) -> bool {
    matches!(
        op,
        EE_SCENARIO_WC_LOCATION_X
            | EE_SCENARIO_WC_LOCATION_Y
            | EE_SCENARIO_WC_LOCATION_SET
            | EE_SCENARIO_WC_T_FROM
            | EE_SCENARIO_WX_PLACE_SET
            | EE_SCENARIO_WX_PLACE_X
            | EE_SCENARIO_WX_PLACE_Y
            | EE_SCENARIO_WX_FACE_FROM_UNIT
            | EE_SCENARIO_WX_FACE_POINT
            | EE_SCENARIO_WX_POINT_SET
            | EE_SCENARIO_WX_T_FROM_UNIT
            | EE_SCENARIO_WX_T_SET_X
            | EE_SCENARIO_WX_T_SET_Y
            | EE_SCENARIO_WX_T_SET_Z
            | EE_SCENARIO_WX_T_X
            | EE_SCENARIO_WX_T_Y
            | EE_SCENARIO_WX_GROUND_IDENTITY
            | EE_SCENARIO_WX_GROUND_POSITION
            | EE_SCENARIO_WORLD_CAMERA_POINT
    )
}

/// The world operation `op` (`EE_SCENARIO_WX_*`, `_WC_*`, `_WORLD_CAMERA_POINT`) on the run's values `s` with the
/// arguments `a` (floats as their bits), or None for another operation. `location(op, a0)` is the unit's
/// `mPhysicalModel.GetLocation()` the operation names (None: the C++'s NULL complex unit).
pub fn op(
    s: &mut EeScenarioRunState,
    op: u32,
    a: &[i64; 6],
    location: &mut dyn FnMut(u32, i64) -> Option<GeTransformation>,
) -> Option<i64> {
    let f = |i: usize| float(a[i]);
    match op {
        EE_SCENARIO_WC_LOCATION_X => return Some(bits(s.mCreateLocation.x)),
        EE_SCENARIO_WC_LOCATION_Y => return Some(bits(s.mCreateLocation.y)),
        EE_SCENARIO_WC_LOCATION_SET => s.mCreateLocation = Ge2DPoint { x: f(0), y: f(1) },
        EE_SCENARIO_WC_T_FROM => s.mCreateTransform = location(op, a[0])?,
        EE_SCENARIO_WX_PLACE_SET => s.mPlace = Ge2DPoint { x: f(0), y: f(1) },
        EE_SCENARIO_WX_PLACE_X => return Some(bits(s.mPlace.x)),
        EE_SCENARIO_WX_PLACE_Y => return Some(bits(s.mPlace.y)),
        EE_SCENARIO_WX_FACE_FROM_UNIT => s.mFace = location(op, a[0])?,
        EE_SCENARIO_WX_FACE_POINT => {
            let [x, y, z] = position(&s.mFace);
            s.mFacePoint = Ge3DPoint { x, y, z };
        }
        EE_SCENARIO_WX_POINT_SET => s.mPoint = Ge3DPoint { x: f(0), y: f(1), z: f(2) },
        EE_SCENARIO_WX_T_FROM_UNIT => s.mTransformation = location(op, a[0])?,
        EE_SCENARIO_WX_T_SET_X => s.mTransformation.m[0][3] = f(0),
        EE_SCENARIO_WX_T_SET_Y => s.mTransformation.m[1][3] = f(0),
        EE_SCENARIO_WX_T_SET_Z => {
            s.mTransformation.m[2][3] = f(0);
            return Some(bits(s.mTransformation.m[2][3]));
        }
        EE_SCENARIO_WX_T_X => return Some(bits(s.mTransformation.m[0][3])),
        EE_SCENARIO_WX_T_Y => return Some(bits(s.mTransformation.m[1][3])),
        EE_SCENARIO_WX_GROUND_IDENTITY => s.mGround = GeTransformation::IDENTITY,
        EE_SCENARIO_WX_GROUND_POSITION => {
            let p = position(&s.mTransformation);
            for (row, v) in s.mGround.m.iter_mut().zip(p) {
                row[3] = v;
            }
        }
        EE_SCENARIO_WORLD_CAMERA_POINT => {
            let p = Ge3DPoint { x: f(1), y: f(2), z: f(3) };
            if a[0] != 0 {
                s.mStart = p;
            } else {
                s.mEnd = p;
            }
        }
        _ => return None,
    }
    Some(0)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn args(v: &[i64]) -> [i64; 6] {
        let mut a = [0; 6];
        a[..v.len()].copy_from_slice(v);
        a
    }

    #[test]
    fn the_run_s_values_are_set_and_read_as_the_cpp_did() {
        let mut s = ee_scenario_run_state_new();
        assert_eq!(s.mGround, GeTransformation::IDENTITY);
        let mut t = GeTransformation::IDENTITY;
        t.m[0][3] = 4.0;
        t.m[1][3] = 5.0;
        t.m[2][3] = 6.0;
        let mut loc = |_: u32, id: i64| (id == 7).then_some(t);
        let mut run = |s: &mut EeScenarioRunState, o, v: &[i64]| op(s, o, &args(v), &mut loc);
        assert_eq!(run(&mut s, EE_SCENARIO_WX_PLACE_SET, &[bits(1.5), bits(2.5)]), Some(0));
        assert_eq!(run(&mut s, EE_SCENARIO_WX_PLACE_Y, &[]), Some(bits(2.5)));
        assert_eq!(run(&mut s, EE_SCENARIO_WX_T_FROM_UNIT, &[8]), None);
        assert_eq!(run(&mut s, EE_SCENARIO_WX_T_FROM_UNIT, &[7]), Some(0));
        assert_eq!(run(&mut s, EE_SCENARIO_WX_T_SET_Z, &[bits(9.0)]), Some(bits(9.0)));
        assert_eq!(run(&mut s, EE_SCENARIO_WX_T_X, &[]), Some(bits(4.0)));
        assert_eq!(run(&mut s, EE_SCENARIO_WX_GROUND_POSITION, &[]), Some(0));
        assert_eq!(position(&s.mGround), [4.0, 5.0, 9.0]);
        assert_eq!(run(&mut s, EE_SCENARIO_WX_FACE_FROM_UNIT, &[7]), Some(0));
        assert_eq!(run(&mut s, EE_SCENARIO_WX_FACE_POINT, &[]), Some(0));
        assert_eq!(s.mFacePoint, Ge3DPoint { x: 4.0, y: 5.0, z: 6.0 });
        run(&mut s, EE_SCENARIO_WORLD_CAMERA_POINT, &[1, bits(1.0), bits(2.0), bits(3.0)]);
        run(&mut s, EE_SCENARIO_WORLD_CAMERA_POINT, &[0, bits(4.0), bits(5.0), bits(6.0)]);
        assert_eq!((s.mStart.z, s.mEnd.x), (3.0, 4.0));
        run(&mut s, EE_SCENARIO_WC_LOCATION_SET, &[bits(0.5), bits(0.25)]);
        assert_eq!(run(&mut s, EE_SCENARIO_WC_LOCATION_X, &[]), Some(bits(0.5)));
        assert_eq!(run(&mut s, EE_SCENARIO_WX_GROUND_IDENTITY, &[]), Some(0));
        assert_eq!(s.mGround, GeTransformation::IDENTITY);
        assert_eq!(run(&mut s, EE_SCENARIO_WX_DETACH, &[]), None);
        assert!(is_op(EE_SCENARIO_WX_T_Y) && !is_op(EE_SCENARIO_WX_DETACH));
    }
}
