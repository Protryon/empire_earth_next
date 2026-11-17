//! `EEEMStorm`'s decisions (src/Empire Earth/Empire Earth/EEEMStorm.cpp): the type's name, the intensity's fades, the
//! per-step state machine (route, move, linger, die away) and the two route planners' step costs, over a host of the
//! C++ parts (the route request, the path, the weather effect, the physics' random draws, the game speed).
//!
//! The C++ keeps the storm's lifetime, its path and areas, `Begin` (the world's towns and continents, the path
//! manager), the route request, `Load`/`Save` and `Restore`; `EEEMStorm` derives from [`EeAppStormFields`].
#![allow(clippy::unnecessary_cast)] // c_long and c_ulong are 32 bits on Windows, 64 on LP64

use ee_base::math::um::lrintf;
use std::ffi::CStr;
use std::os::raw::{c_long, c_ulong};

/// `EEStormType`'s kinds.
pub(crate) const STORM_UNKNOWN: c_long = 0;
pub(crate) const STORM_RAIN: c_long = 1;
pub(crate) const STORM_SNOW: c_long = 2;
pub(crate) const STORM_HURRICANE: c_long = 3;
pub(crate) const STORM_BLIZZARD: c_long = 4;
/// `kStormMaximumStay`.
const MAXIMUM_STAY: f32 = 45000.0;

/// The storm's plain members (names and order the C++ class's).
#[repr(C)]
#[allow(non_snake_case)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeAppStormFields {
    /// = 1; clamped to [0, 2]
    pub mTargetIntensity: f32,
    /// = 1; the weather effect's intensity follows it
    pub mIntensity: f32,
    /// = 0.02: the fade's relative step
    pub mStep: f32,
    /// the kind (1..4)
    pub mType: c_long,
    /// = 1; the step's state (2 routing, 3 moving, 4 pausing, 5 dying, 6 over, 7 lingering, 8 to route again)
    pub mState: c_long,
    pub mNextMoveTime: c_ulong,
    /// = -1
    pub mEndTime: c_ulong,
    /// a hurricane or a blizzard
    pub mIsSevere: bool,
}

/// The C++ parts a step reaches.
pub trait StormHost {
    /// the route request: `None` none, else whether it is done
    fn request(&mut self) -> Option<bool>;
    /// `TakePath`: the finished route reversed, the request released
    fn take_path(&mut self);
    /// `RequestRoute`: the old request cancelled, a new one with the kind's planner
    fn request_route(&mut self);
    /// the path's length
    fn path_len(&mut self) -> usize;
    /// the effect's centre's squared distance to the path's last point (`goal` false) or to the goal
    fn distance_squared(&mut self, goal: bool) -> c_long;
    /// `mPath.pop_back()`
    fn pop_path(&mut self);
    /// the effect moved to the path's last point (`goal` false) or to the goal, within the storm's area
    fn move_to(&mut self, goal: bool);
    /// the effect's tile count
    fn tile_count(&mut self) -> c_ulong;
    /// the effect ended (its elapsed time its duration)
    fn end_effect(&mut self);
    /// the effect's intensity
    fn set_effect_intensity(&mut self, intensity: f32);
    /// `gWorld.GetGameSpeed()`
    fn game_speed(&mut self) -> f32;
    /// `gPhysics->GetRandom(min, max)`; `gPhysics->mRandom.Generate()`
    fn random_range(&mut self, min: f32, max: f32) -> f32;
    fn random(&mut self) -> f32;
    /// the environment manager's colour back to the storm colour (a rain storm's end)
    fn reset_color(&mut self);
}

/// `GetTypeName`.
pub fn type_name(kind: c_long) -> &'static CStr {
    match kind {
        STORM_UNKNOWN => c"Unknown",
        STORM_RAIN => c"Rain",
        STORM_SNOW => c"Snow",
        STORM_HURRICANE => c"Hurricane",
        STORM_BLIZZARD => c"Blizzard",
        5 => c"PlaceHolder1",
        6 => c"PlaceHolder2",
        7 => c"PlaceHolder3",
        8 => c"PlaceHolder4",
        _ => c"Type Not defined",
    }
}

/// The constructor's members for a kind (a known kind then begins).
pub fn new_fields(kind: c_long) -> EeAppStormFields {
    EeAppStormFields {
        mTargetIntensity: 1.0,
        mIntensity: 1.0,
        mStep: 0.02,
        mType: kind,
        mState: 1,
        mNextMoveTime: 0,
        mEndTime: 0xffff_ffff,
        mIsSevere: kind == STORM_HURRICANE || kind == STORM_BLIZZARD,
    }
}

/// `SetIntensity`: the storm's and its effect's.
pub fn set_intensity(s: &mut EeAppStormFields, h: &mut impl StormHost, intensity: f32) {
    s.mIntensity = intensity;
    h.set_effect_intensity(intensity);
}

/// `SetTargetIntensity`: clamped to [0, 2] (a NaN kept, as the C++'s tests).
pub fn set_target_intensity(s: &mut EeAppStormFields, intensity: f32) {
    #[allow(clippy::neg_cmp_op_on_partial_ord)]
    let v = if !(intensity > 2.0) {
        if intensity < 0.0 {
            0.0
        } else {
            intensity
        }
    } else {
        2.0
    };
    s.mTargetIntensity = v;
}

/// `Grow`: one step up toward the target (from nothing straight to 0.15).
pub fn grow(s: &mut EeAppStormFields, h: &mut impl StormHost) {
    let mut v = (if s.mIntensity != 0.0 { s.mStep * s.mIntensity } else { 0.15 }) + s.mIntensity;
    if v > s.mTargetIntensity {
        v = s.mTargetIntensity;
    }
    set_intensity(s, h, v);
}

/// `Shrink`: one step down toward the target, nothing at 0.005 or below.
pub fn shrink(s: &mut EeAppStormFields, h: &mut impl StormHost) {
    let mut v = s.mIntensity - s.mStep * s.mIntensity;
    if v < s.mTargetIntensity {
        v = s.mTargetIntensity;
    }
    if v <= 0.005 {
        v = 0.0;
    }
    set_intensity(s, h, v);
}

/// `lrintf(speed * f) + time`, the C++'s unsigned sum.
fn later(h: &mut impl StormHost, f: f32, time: c_ulong) -> c_ulong {
    let speed = h.game_speed();
    (lrintf(speed * f) as c_long as c_ulong).wrapping_add(time)
}

/// `Step(time)`: follows the route to the goal, lingers, then dies away (also at the end time); the intensity fades
/// toward the target (always 0.4 while alive: the hour is fixed at 10).
pub fn step(s: &mut EeAppStormFields, h: &mut impl StormHost, time: c_ulong) {
    let hour = 10.0f32;
    let mut intensity = 0.4f32;
    let severe = s.mType == STORM_BLIZZARD || s.mType == STORM_HURRICANE;
    if hour > 12.0 {
        intensity = if hour < 24.0 {
            (if severe { 2.0 } else { 1.0 }) * (hour / 24.0)
        } else if severe {
            2.0
        } else {
            1.0
        };
    }
    match s.mState {
        2 => match h.request() {
            Some(true) => {
                h.take_path();
                s.mState = 3;
            }
            Some(false) => {}
            None => s.mState = 3,
        },
        3 => {
            if s.mNextMoveTime <= time {
                if h.path_len() != 0 {
                    if h.distance_squared(false) <= 9 {
                        h.pop_path();
                    }
                    let goal = h.path_len() == 0;
                    h.move_to(goal);
                }
                if h.distance_squared(true) <= 9 {
                    s.mState = 7;
                    let stay = h.random_range(15000.0, MAXIMUM_STAY);
                    s.mNextMoveTime = later(h, stay, time);
                } else {
                    if h.random() <= 0.25 {
                        s.mState = 4;
                    }
                    s.mNextMoveTime = later(h, 15000.0, time);
                }
            }
            if h.tile_count() < 45 {
                s.mState = 5;
            }
        }
        4 => {
            if h.random() <= 0.4 {
                s.mState = 3;
            }
            s.mNextMoveTime = later(h, 15000.0, time);
        }
        5 => {
            intensity = 0.0;
            s.mState = 6;
        }
        6 => {
            intensity = 0.0;
            h.end_effect();
        }
        7 => {
            if s.mNextMoveTime <= time {
                s.mState = 5;
            }
        }
        8 => {
            h.request_route();
            s.mState = 2;
        }
        _ => {}
    }
    if s.mEndTime <= time {
        if s.mType == STORM_RAIN || s.mType == STORM_HURRICANE {
            h.reset_color();
        }
        s.mState = 5;
    }
    set_target_intensity(s, intensity);
    if s.mIntensity < s.mTargetIntensity {
        grow(s, h);
    } else if s.mIntensity > s.mTargetIntensity {
        shrink(s, h);
    }
}

/// The AI's tile kinds (ee/EEAIKindIDs.h) the planners price.
const LAND: c_long = 1;
const WATER: c_long = 2;
const CLIFF: c_long = 3;
const FOREST: c_long = 4;
const MAP_EDGE: c_long = 6;
const SHALLOWS: c_long = 7;
const SHALLOWS_SHORE: c_long = 8;
const SHALLOWS_NEAR_SHORE: c_long = 9;
const SHORE: c_long = 10;
const NEAR_SHORE: c_long = 11;
const WALL: c_long = 0xd;
const GATE: c_long = 0xe;
const SPACE: c_long = 0xf;

/// A route step's base: 1 within a sector, else 10 straight or 14 diagonal between sector centres.
fn base_cost(other_sector: bool, straight: bool) -> c_long {
    if other_sector {
        if straight {
            10
        } else {
            14
        }
    } else {
        1
    }
}

/// `EEAIGPGModifierStormRoutePlannerSnow::GetCost`: cheap over cliffs and hills, dear over forest, shallows and the
/// map edge (`hill`: the step's tile is on a hill; asked only for land, walls and gates).
pub fn snow_cost(other_sector: bool, straight: bool, kind: c_long, hill: &mut impl FnMut() -> bool) -> c_long {
    let cost = base_cost(other_sector, straight);
    match kind {
        LAND => {
            if hill() {
                1
            } else {
                cost + 2
            }
        }
        CLIFF => 1,
        FOREST | SHALLOWS => cost + 20,
        MAP_EDGE => cost + 500,
        WALL | GATE => {
            if hill() {
                1
            } else {
                cost + 5
            }
        }
        _ => cost + 100,
    }
}

/// `EEAIGPGModifierStormRoutePlannerRain::GetCost`: cheap along shores, dear over water, cliffs and the map edge.
pub fn rain_cost(other_sector: bool, straight: bool, kind: c_long) -> c_long {
    let cost = base_cost(other_sector, straight);
    match kind {
        WATER | SHALLOWS | CLIFF => cost + 20,
        MAP_EDGE | SPACE => cost + 500,
        SHALLOWS_SHORE | SHALLOWS_NEAR_SHORE | SHORE | NEAR_SHORE => 1,
        _ => cost + 100,
    }
}
