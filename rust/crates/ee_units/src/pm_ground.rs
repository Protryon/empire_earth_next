//! The physics models on the ground and in the water: `EEPMLand` (and `EEPMHovercraft`, the same bodies), `EEPMNaval`,
//! `EEPMSubmergedNaval`, `EEPMSeaAnimal`, `EEPMSpaceship` (its bodies are the ship's with the roll kept and the
//! height of its class) and `EEPMBuilding`'s collapse (pm.rs has the dispatch and the base model).

#![allow(
    clippy::neg_cmp_op_on_partial_ord,
    clippy::assign_op_pattern,
    clippy::manual_clamp,
    clippy::approx_constant,
    clippy::collapsible_if,
    clippy::collapsible_else_if,
    clippy::needless_late_init,
    // the C++'s range tests as written; `c_long` is `i32` on Windows and `i64` on Linux
    clippy::manual_range_contains,
    clippy::unnecessary_cast
)]

use ee_ge::types::{Ge2DPoint, Ge3DLine, Ge3DPoint, GeVector};
use ee_ge::um::{floor, um_arccos, um_arctan2, um_cos};

use crate::pm::*;

/// The unit or point a step turns to (`Step1`): the attacked unit (its location, or its tile's centre when it is no
/// complex unit: `with_tile`), the calamity's location (`calamity`: the helicopter's), the healed unit, else the
/// goal's location; z 0. The second value: the attacked unit is a planet spaceship (the spaceship's test).
pub(crate) fn step1_target(c: &mut Ctx, u: EeUnitsPmUnit, with_tile: bool, calamity: bool) -> (Ge3DPoint, bool) {
    let mut target = Ge3DPoint::default();
    let mut air = false;
    let attack = c.h.attack_target(u.obj);
    let complex = if attack != 0 { c.h.unit_complex(attack) } else { 0 };
    if attack != 0 && (complex != 0 || with_tile) {
        if complex != 0 {
            air = c.h.class(complex).category == CATEGORY_PLANET_SPACESHIP;
            let l = c.h.location(complex);
            target.x = l.x();
            target.y = l.y();
        } else {
            let (x, y) = c.h.unit_tile(attack);
            target.x = x as f32 + 0.5;
            target.y = y as f32 + 0.5;
        }
    } else if calamity && c.h.is_cast_goal(u.obj) {
        target = c.h.calamity_target_location(u.obj);
    } else {
        let heal = c.h.heal_target(u.obj);
        if heal != 0 {
            let l = c.h.location(heal);
            target.x = l.x();
            target.y = l.y();
        } else {
            target = c.h.goal_location(u.obj);
        }
    }
    (target, air)
}

/// The ground under the land unit's new point (its class's cruise altitude when it swims), the heading's
/// orientation kept or tilted with the terrain, the step's tail of every land step.
fn land_place(
    c: &mut Ctx,
    u: EeUnitsPmUnit,
    mut t: ee_ge::types::GeTransformation,
    mut new: ee_ge::types::GeTransformation,
    l: Ge3DPoint,
    yaw: f32,
    moved: bool,
) -> ee_ge::types::GeTransformation {
    let class = c.h.class(u.obj);
    let mut ground = c.h.ground_point(l);
    if c.h.is_swimming(u.obj) {
        ground.z = class.cruise_altitude;
    }
    let (mut pitch, mut roll) = (0.0f32, 0.0f32);
    if class.pitches_with_terrain == 0 && class.rolls_with_terrain == 0 {
        if moved {
            t = new;
        } else {
            ypr(&mut t, yaw, 0.0, 0.0);
        }
    } else {
        set_position(&mut new, ground);
        if class.pitches_with_terrain != 0 {
            pitch = c.h.terrain_pitch(&new);
        }
        if class.rolls_with_terrain != 0 {
            roll = c.h.terrain_roll(&new);
        }
        ypr(&mut t, yaw, pitch, roll);
    }
    set_position(&mut t, ground);
    t
}

/// `EEPMLand::Step0`: toward the next waypoint: turned, accelerated, the step clipped against blocked tiles and set
/// on the ground (tilted with the terrain when the class does); arrived when close.
pub(crate) fn land_step0(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    let t = c.h.location(u.obj);
    let mut l = Ge3DPoint { x: t.x(), y: t.y(), z: 0.0 };
    let step = c.t.tick_seconds;
    let mut target = Ge3DPoint::default();
    if c.h.next_waypoint(u.obj, &mut target) != 0 {
        return;
    }
    target.z = 0.0;
    let distance = flat_distance(target, l);
    let (turn, yaw_rate, yaw) = turn_toward(c, k, u, l, target, false);
    c.set_angular(u, yaw_rate, 0.0, 0.0);
    let new = rotation(yaw, 0.0, 0.0);
    let speed = accelerate(c, k, u, distance, turn);
    set_velocity(u, speed);
    let mv = new.apply_vector(fwd(speed * step));
    l = add(l, mv);
    let moved = clip_step(c, k, u, mv, &mut l);
    let t = land_place(c, u, t, new, l, yaw, moved);
    c.set_has_arrived(u, distance);
    c.h.set_transformation(u.obj, &t);
}

/// `EEPMLand::Step1`: turned toward the unit it attacks or heals (or its goal location) while braking, on the
/// ground; a unit with model parts only spins its turn down.
pub(crate) fn land_step1(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    let t = c.h.location(u.obj);
    let mut l = Ge3DPoint { x: t.x(), y: t.y(), z: 0.0 };
    let brake = c.h.acceleration(u.obj);
    let speed = rd!(u, mVelocity);
    let step = c.t.tick_seconds;
    let (mut target, _) = step1_target(c, u, true, false);
    target.z = 0.0;
    let (yaw_rate, yaw) = if c.h.model_parts(u.obj) == 0 {
        let (_, r, y) = turn_toward(c, k, u, l, target, true);
        (r, y)
    } else {
        spin_down(c, u)
    };
    c.set_angular(u, yaw_rate, 0.0, 0.0);
    let new = rotation(yaw, 0.0, 0.0);
    // `kPMLandBrakeFactor * theStep`: 1.0f times the step is the step
    let mut new_speed = speed - step * brake;
    if new_speed < 0.0 {
        new_speed = 0.0;
    }
    set_velocity(u, new_speed);
    let mv = new.apply_vector(fwd(new_speed * step));
    l = add(l, mv);
    let moved = clip_step(c, k, u, mv, &mut l);
    let t = land_place(c, u, t, new, l, yaw, moved);
    c.h.set_transformation(u.obj, &t);
}

/// `EEPMLand::Step4`: speed and turn braked toward a stop, still moved by what is left, on the ground; nothing more
/// once both are zero.
pub(crate) fn land_step4(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    let t = c.h.location(u.obj);
    let mut l = Ge3DPoint { x: t.x(), y: t.y(), z: 0.0 };
    let step = c.t.tick_seconds;
    let (yaw_rate, yaw) = spin_down(c, u);
    c.set_angular(u, yaw_rate, 0.0, 0.0);
    let new = rotation(yaw, 0.0, 0.0);
    let speed = brake_speed(c, u, step);
    set_velocity(u, speed);
    if yaw_rate == 0.0 && speed == 0.0 {
        return;
    }
    let mv = new.apply_vector(fwd(speed * step));
    l = add(l, mv);
    let moved = clip_step(c, k, u, mv, &mut l);
    let t = land_place(c, u, t, new, l, yaw, moved);
    c.h.set_transformation(u.obj, &t);
}

/// `Step4`'s braked speed (`GetAcceleration() * kPMLandBrakeFactor * theStep`: the factor 1 changes no bit), capped
/// at the top speed; 0 when not moving forward.
fn brake_speed(c: &mut Ctx, u: EeUnitsPmUnit, step: f32) -> f32 {
    if rd!(u, mVelocity) > 0.0 {
        let mut speed = rd!(u, mVelocity);
        speed = speed - c.h.acceleration(u.obj) * step;
        if speed < 0.0 {
            speed = 0.0;
        }
        let max = c.h.max_velocity(u.obj);
        if max < speed {
            speed = max;
        }
        speed
    } else {
        0.0
    }
}

/// `EEPMLand::Step5`: a stopped land unit turned in place to its location goal's facing (tilted with the terrain);
/// a moving one brakes first; nothing without a location goal.
pub(crate) fn land_step5(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    let mut t = c.h.location(u.obj);
    let mut goal_yaw = 0.0f32;
    if !c.h.goal_facing(u.obj, &mut goal_yaw) {
        return;
    }
    if rd!(u, mVelocity) != 0.0 {
        step4(c, k, u);
        return;
    }
    let current = heading(&t);
    let (_, yaw_rate, yaw) = turn_toward_angle(c, u, current, goal_yaw, false);
    c.set_angular(u, yaw_rate, 0.0, 0.0);
    ypr(&mut t, yaw, 0.0, 0.0);
    let class = c.h.class(u.obj);
    if class.pitches_with_terrain != 0 || class.rolls_with_terrain != 0 {
        let (mut pitch, mut roll) = (0.0f32, 0.0f32);
        if class.pitches_with_terrain != 0 {
            pitch = c.h.terrain_pitch(&t);
        }
        if class.rolls_with_terrain != 0 {
            roll = c.h.terrain_roll(&t);
        }
        ypr(&mut t, yaw, pitch, roll);
    }
    c.h.set_transformation(u.obj, &t);
}

/// The ship models' tile test (`EEPMNaval`, `EEPMSubmergedNaval`): the shallows' AI value against the unit's water
/// state, then the tile's units.
#[derive(Clone, Copy, PartialEq, Eq)]
enum Shore {
    /// `EEPMLand`: no water test; an unbuilt land building does not block
    Land,
    /// the ships: the AI value's water test
    Water,
}

/// The land and ships' `ClipStep`: the step onto a new tile checked (terrain, edge, the shallows, blocking units,
/// diagonal corners); blocked, retried without the x, then without the y part, else undone; a blocker on the
/// waypoint's own tile becomes the blocking unit (collision state 2) and the step is undone. Whether the step
/// stands; `slowed_time` is what a retried step adds to the collision time (the land's tick, the ships' 0.33 of it).
fn tile_clip_step(
    c: &mut Ctx,
    u: EeUnitsPmUnit,
    mv: GeVector,
    io: &mut Ge3DPoint,
    shore: Shore,
) -> (bool, bool, bool, bool) {
    let mut blocker = 0u64;
    let mut x = floor(io.x);
    let mut y = floor(io.y);
    let mut tried_x = false;
    let mut tried_y = false;
    let mut slowed = true;
    let mut tile = c.h.tile(x, y);
    let mut result = tile != 0;
    if tile != 0 {
        loop {
            if !result {
                break;
            }
            if tile == c.h.tile_of(u.obj) {
                break;
            }
            let info = c.h.tile_info(tile);
            if !c.h.can_move_on_terrain(u.obj, info.terrain_type) || c.h.is_edge_tile(x, y) {
                result = false;
            } else {
                result = true;
                if shore == Shore::Water && info.ai_value != 0 {
                    let water = rd!(u, mWaterState);
                    result = if info.ai_value < 0 { water == 0 } else { water != 3 };
                }
                if result {
                    let mut node = c.h.tile_unit_first(tile);
                    while node != 0 {
                        blocker = c.h.tile_unit(node);
                        result = c.h.passable_by(blocker, u.obj)
                            || (shore == Shore::Land && c.h.unbuilt_land_building(blocker));
                        node = c.h.tile_unit_next(node);
                        if !result {
                            break;
                        }
                    }
                    if result {
                        if tile_x(u) != x && tile_y(u) != y {
                            let (ux, uy) = (tile_x(u), tile_y(u));
                            if !c.h.can_stand_on(u.obj, x, uy) && !c.h.can_stand_on(u.obj, ux, y) {
                                result = false;
                            }
                        }
                        if result {
                            break;
                        }
                    }
                }
            }
            if blocker != 0 {
                let mut target = Ge3DPoint::default();
                if c.h.next_waypoint(u.obj, &mut target) == 0 && floor(target.x) == x && floor(target.y) == y {
                    c.h.set_blocking_unit(u.obj, blocker);
                    if rd!(u, mCollisionState) != 2 {
                        wr!(u, mCollisionState, 2);
                    }
                    io.x = io.x - mv.x;
                    io.y = io.y - mv.y;
                    io.z = io.z - mv.z;
                    tile = 0;
                    slowed = false;
                }
            }
            if slowed {
                if !tried_x {
                    io.x = io.x - mv.x;
                    x = floor(io.x);
                    tile = c.h.tile(x, y);
                    result = tile != 0;
                    tried_x = true;
                } else if !tried_y {
                    io.x = io.x + mv.x;
                    io.y = io.y - mv.y;
                    x = floor(io.x);
                    y = floor(io.y);
                    tile = c.h.tile(x, y);
                    result = tile != 0;
                    tried_y = true;
                } else {
                    io.x = io.x - mv.x;
                    tile = 0;
                }
            }
            if tile == 0 {
                break;
            }
        }
    } else {
        io.x = io.x - mv.x;
        io.y = io.y - mv.y;
        result = false;
    }
    (result, slowed, tried_x, tried_y)
}

/// `EEPMLand::ClipStep` (a retried step counts a tick toward the collision time).
pub(crate) fn land_clip_step(c: &mut Ctx, u: EeUnitsPmUnit, mv: GeVector, io: &mut Ge3DPoint) -> bool {
    let (mut result, slowed, tried_x, tried_y) = tile_clip_step(c, u, mv, io, Shore::Land);
    if result {
        result = clip_step_base(c, mv, io);
    }
    if slowed && (tried_x || tried_y) {
        wr!(u, mCollisionResolutionCount, rd!(u, mCollisionResolutionCount) + c.t.tick_time);
    }
    result
}

/// `EEPMNaval::ClipStep` (a retried step counts 0.33 tick).
pub(crate) fn naval_clip_step(c: &mut Ctx, u: EeUnitsPmUnit, mv: GeVector, io: &mut Ge3DPoint) -> bool {
    let (mut result, slowed, tried_x, tried_y) = tile_clip_step(c, u, mv, io, Shore::Water);
    if result {
        result = clip_step_base(c, mv, io);
    }
    if slowed && (tried_x || tried_y) {
        wr!(u, mCollisionResolutionCount, rd!(u, mCollisionResolutionCount) + c.t.tick_time * 0.33);
    }
    result
}

/// `EEPMSubmergedNaval::ClipStep`: the ship's test; the depth kept (the dive depth in physics action 0); a retried
/// step counts a tick.
pub(crate) fn sub_clip_step(c: &mut Ctx, u: EeUnitsPmUnit, mv: GeVector, io: &mut Ge3DPoint) -> bool {
    let (mut result, slowed, tried_x, tried_y) = tile_clip_step(c, u, mv, io, Shore::Water);
    if result {
        result = clip_step_base(c, mv, io);
    }
    io.z = c.h.location(u.obj).z();
    if rd!(u, mCurrentPhysicsAction) == 0 {
        c.h.adjust_dive_depth(u.obj, io);
    }
    if slowed && (tried_x || tried_y) {
        wr!(u, mCollisionResolutionCount, rd!(u, mCollisionResolutionCount) + c.t.tick_time);
    }
    result
}

/// `EEPMSpaceship::ClipStep`: terrain, edge and diagonal corners (units never block), retried without x, then y,
/// else undone; a retried step counts 0.33 tick.
pub(crate) fn spaceship_clip_step(c: &mut Ctx, u: EeUnitsPmUnit, mv: GeVector, io: &mut Ge3DPoint) -> bool {
    let mut x = floor(io.x);
    let mut y = floor(io.y);
    let mut tried_x = false;
    let mut tried_y = false;
    let mut tile = c.h.tile(x, y);
    let mut result = tile != 0;
    if tile != 0 {
        loop {
            if !result {
                break;
            }
            if tile == c.h.tile_of(u.obj) {
                break;
            }
            let info = c.h.tile_info(tile);
            if c.h.can_move_on_terrain(u.obj, info.terrain_type) && !c.h.is_edge_tile(x, y) {
                if tile_x(u) == x || tile_y(u) == y {
                    break;
                }
                let (ux, uy) = (tile_x(u), tile_y(u));
                if c.h.can_stand_on(u.obj, x, uy) || c.h.can_stand_on(u.obj, ux, y) {
                    break;
                }
            }
            result = false;
            if !tried_x {
                io.x = io.x - mv.x;
                x = floor(io.x);
                tile = c.h.tile(x, y);
                result = tile != 0;
                tried_x = true;
            } else if !tried_y {
                io.x = io.x + mv.x;
                io.y = io.y - mv.y;
                x = floor(io.x);
                y = floor(io.y);
                tile = c.h.tile(x, y);
                result = tile != 0;
                tried_y = true;
            } else {
                io.x = io.x - mv.x;
                tile = 0;
            }
            if tile == 0 {
                break;
            }
        }
    } else {
        io.x = io.x - mv.x;
        io.y = io.y - mv.y;
        result = false;
    }
    if result {
        result = clip_step_base(c, mv, io);
    }
    if tried_x || tried_y {
        wr!(u, mCollisionResolutionCount, rd!(u, mCollisionResolutionCount) + c.t.tick_time * 0.33);
    }
    result
}

// ---- EEPMNaval

/// `EEPMNaval::Step0`: toward the next waypoint: turned, accelerated, clipped, arrived when close, bobbed.
pub(crate) fn naval_step0(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    let mut t = c.h.location(u.obj);
    let mut l = Ge3DPoint { x: t.x(), y: t.y(), z: 0.0 };
    let step = c.t.tick_seconds;
    let mut target = Ge3DPoint::default();
    if c.h.next_waypoint(u.obj, &mut target) != 0 {
        return;
    }
    target.z = 0.0;
    let distance = flat_distance(target, l);
    let (turn, yaw_rate, yaw) = turn_toward(c, k, u, l, target, false);
    c.set_angular(u, yaw_rate, 0.0, 0.0);
    ypr(&mut t, yaw, 0.0, 0.0);
    let speed = accelerate(c, k, u, distance, turn);
    set_velocity(u, speed);
    let mv = t.apply_vector(fwd(speed * step));
    l = add(l, mv);
    let moved = clip_step(c, k, u, mv, &mut l);
    set_position(&mut t, l);
    if !moved {
        ypr(&mut t, yaw, 0.0, 0.0);
    }
    c.set_has_arrived(u, distance);
    c.h.set_transformation(u.obj, &t);
    apply_bob(c, k, u);
}

/// `EEPMNaval::ApplyBob`: the bob phase advanced with game time, the pitch and depth from the class's bob.
pub(crate) fn naval_apply_bob(c: &mut Ctx, u: EeUnitsPmUnit) {
    let mut t = c.h.location(u.obj);
    let yaw = heading(&t);
    let mut phase = c.t.tick_time * 0.001_570_796_5 + rd!(u, mF220);
    while phase > PI {
        phase -= TWO_PI;
    }
    if phase != rd!(u, mF220) {
        wr!(u, mF220, phase);
    }
    let bob = (um_cos(HALF_PI - phase) + 1.0) * 0.5;
    let class = c.h.class(u.obj);
    let pitch = (bob - 0.5) * class.bob_pitch;
    ypr(&mut t, yaw, pitch, 0.0);
    let p = Ge3DPoint { x: t.x(), y: t.y(), z: -(class.bob_height * bob) };
    set_position(&mut t, p);
    c.h.set_transformation(u.obj, &t);
}

/// `EEPMNaval::Step1`: turned toward the unit it attacks or heals (or its goal location) while braking, bobbed; a
/// ship with model parts only spins down.
pub(crate) fn naval_step1(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    let mut t = c.h.location(u.obj);
    let mut l = Ge3DPoint { x: t.x(), y: t.y(), z: 0.0 };
    let brake = c.h.acceleration(u.obj);
    let speed = rd!(u, mVelocity);
    let step = c.t.tick_seconds;
    let (mut target, _) = step1_target(c, u, false, false);
    target.z = 0.0;
    let (yaw_rate, yaw) = if c.h.model_parts(u.obj) == 0 {
        let (_, r, y) = turn_toward(c, k, u, l, target, true);
        (r, y)
    } else {
        spin_down(c, u)
    };
    c.set_angular(u, yaw_rate, 0.0, 0.0);
    ypr(&mut t, yaw, 0.0, 0.0);
    let mut new = speed - step * brake;
    if new < 0.0 {
        new = 0.0;
    }
    set_velocity(u, new);
    let mv = t.apply_vector(fwd(new * step));
    l = add(l, mv);
    let moved = clip_step(c, k, u, mv, &mut l);
    set_position(&mut t, l);
    if !moved {
        ypr(&mut t, yaw, 0.0, 0.0);
    }
    c.h.set_transformation(u.obj, &t);
    apply_bob(c, k, u);
}

/// The ships' sinking (`EEPMNaval::Step2`, `EEPMSubmergedNaval::Step2`): the bow tipped toward `tip` up or down,
/// rolled (`roll_scale` times the pitch; the ship's only at a class scale up to 3), lowered faster the steeper.
fn sink(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit, submarine: bool) {
    let mut t = c.h.location(u.obj);
    let l = position(&t);
    let mut rate = c.h.angular_velocity(u.obj);
    let class = c.h.class(u.obj);
    let step = c.t.tick_seconds;
    let r = forward(&t);
    let yaw = um_arctan2(-r.x, r.y);
    let mut pitch = um_arccos(-r.z) - HALF_PI;
    let mut roll = 0.0f32;
    if submarine {
        if yaw >= -0.785_398_2 && yaw <= 2.356_194_5 && pitch < 0.314_159_27 {
            let (pr, p) = approach_pitch(c, k, u, pitch, 0.314_159_27);
            rate.pitch = pr;
            pitch = p;
        } else if pitch > -0.314_159_27 {
            let (pr, p) = approach_pitch(c, k, u, pitch, -0.314_159_27);
            rate.pitch = pr;
            pitch = p;
        }
        roll = if yaw >= -2.356_194_5 && yaw <= 0.785_398_2 { um_abs(pitch) * -5.0 } else { um_abs(pitch) * 5.0 };
    } else {
        if yaw < -0.785_398_2 || yaw > 2.356_194_5 || rate.pitch < 0.0 {
            if pitch > -1.256_637_1 {
                let (pr, p) = approach_pitch(c, k, u, pitch, -1.256_637_1);
                rate.pitch = pr;
                pitch = p;
            }
        } else if pitch < 1.256_637_1 {
            let (pr, p) = approach_pitch(c, k, u, pitch, 1.256_637_1);
            rate.pitch = pr;
            pitch = p;
        }
        if class.scale <= 3.0 {
            roll = if yaw >= -2.356_194_5 && yaw <= 0.785_398_2 { um_abs(pitch) * -0.5 } else { um_abs(pitch) * 0.5 };
        }
    }
    ypr(&mut t, yaw, pitch, roll);
    c.h.set_angular_velocity(u.obj, rate);
    let sink = um_abs(pitch);
    let p = Ge3DPoint { x: l.x, y: l.y, z: l.z - (sink * 0.318_309_87) * class.max_velocity * step };
    set_position(&mut t, p);
    c.h.set_transformation(u.obj, &t);
}

pub(crate) fn naval_step2(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    sink(c, k, u, false);
}

pub(crate) fn sub_step2(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    sink(c, k, u, true);
}

/// `EEPMNaval::PushMoving` and its twin `EEPMSpaceship::PushMoving`: the base's with a push immunity and a stop for
/// two resting ships.
pub(crate) fn push_moving_naval(c: &mut Ctx, u: EeUnitsPmUnit, o: EeUnitsPmUnit) -> bool {
    let mut result = false;
    let mut speed = rd!(u, mCollisionRadius);
    let other_speed = rd!(o, mCollisionRadius);
    let mut slow = 0.75f32;
    let mut t = 0.0f32;
    let mut ot = 0.0f32;
    let mut sign = 1.0f32;
    let mut other_sign = 1.0f32;
    let location = c.h.location(u.obj);
    let mut direction = forward(&location);
    let other_location = c.h.location(o.obj);
    let mut other_direction = forward(&other_location);
    direction.normalize();
    other_direction.normalize();
    let mut angle = um_abs(um_arccos(direction.dot(other_direction)));
    while angle > PI {
        angle -= PI;
    }
    if angle > HALF_PI {
        angle = PI - angle;
    }
    direction.x = direction.x * speed;
    direction.y = direction.y * speed;
    direction.z = direction.z * speed;
    other_direction.x = other_direction.x * other_speed;
    other_direction.y = other_direction.y * other_speed;
    other_direction.z = other_direction.z * other_speed;
    let line = Ge3DLine { start: position(&location), direction };
    let other_line = Ge3DLine { start: position(&other_location), direction: other_direction };
    let crossing = intersect_2d(&line, &other_line, &mut t, &mut ot);
    let x = -other_direction.x;
    other_direction.x = other_direction.y;
    other_direction.y = x;
    let cross_line = Ge3DLine { start: other_line.start, direction: other_direction };
    let (mut cross_t, mut cross_ot) = (0.0f32, 0.0f32);
    let near = if intersect_2d(&line, &cross_line, &mut cross_t, &mut cross_ot)
        && um_abs(cross_ot) * other_speed <= 0.75
        && (cross_t > 0.0 || rd!(u, mVelocity) == 0.0)
        && angle <= FIVE_DEGREES
    {
        t = if cross_t > 0.0 { 0.5 } else { -0.5 };
        true
    } else {
        false
    };
    if crossing {
        if t > 0.0 && t < CONTACT_WINDOW {
            t = -t;
        }
        if ot > 0.0 && ot < CONTACT_WINDOW {
            ot = -ot;
        }
        sign = if t > 0.0 { -1.0 } else { 1.0 };
        other_sign = if ot > 0.0 { -1.0 } else { 1.0 };
        if t < 0.0 {
            t = -t;
        }
        if ot < 0.0 {
            ot = -ot;
        }
    }
    if !near && (!crossing || t >= 0.75 || ot >= 0.75) {
        return result;
    }
    if other_speed <= speed
        // unsigned long against long: an unsigned comparison
        && (rd!(o, mPushImmuneUntil) as u32) < c.t.game_time as u32
        && c.h.goal_kind(o.obj) != GOAL_ATTACK_UNIT
        && !near
    {
        let dx = c.h.max_velocity(o.obj) * other_line.direction.x * other_sign;
        let mut dy = c.h.max_velocity(o.obj);
        dy *= other_line.direction.y * other_sign;
        if other_sign < 0.0 && ot > t {
            set_velocity(o, 0.0);
            c.set_angular(o, 0.0, 0.0, 0.0);
        }
        wr!(o, mPushVelocityX, rd!(o, mPushVelocityX) + dx);
        wr!(o, mPushVelocityY, rd!(o, mPushVelocityY) + dy);
        return result;
    }
    if rd!(u, mVelocity) > 0.0 && near {
        if rd!(u, mCollisionState) == 2 {
            give_up(c, u);
            result = true;
        } else if rd!(u, mCollisionState) == 0 && rd!(o, mVelocity) == 0.0 {
            c.h.set_blocking_unit(u.obj, o.obj);
        } else {
            let v = rd!(u, mVelocity);
            slow = (if v < t { t } else { v }) - 0.5;
        }
    }
    if rd!(u, mVelocity) == 0.0
        && c.h.angular_velocity(u.obj).yaw == 0.0
        && rd!(o, mVelocity) == 0.0
        && c.h.angular_velocity(o.obj).yaw == 0.0
    {
        wr!(u, mPushVelocityX, TURN_EPSILON);
        wr!(u, mPushVelocityY, TURN_EPSILON);
        wr!(u, mPushImmuneUntil, c.t.game_time.wrapping_add(5000) as std::os::raw::c_ulong);
    } else {
        speed = slow + 0.5;
        let dx = c.h.max_velocity(u.obj) * (speed - t) * line.direction.x * sign;
        let dy = c.h.max_velocity(u.obj) * (speed - t) * line.direction.y * sign;
        wr!(u, mPushVelocityX, rd!(u, mPushVelocityX) + dx);
        wr!(u, mPushVelocityY, rd!(u, mPushVelocityY) + dy);
        if sign < 0.0 {
            set_velocity(u, 0.0);
            c.set_angular(u, 0.0, 0.0, 0.0);
        }
    }
    if slow > 0.0 {
        set_velocity(u, rd!(u, mVelocity) * 0.5);
    } else {
        set_velocity(u, 0.0);
        wr!(u, mCollisionResolutionCount, rd!(u, mCollisionResolutionCount) + c.t.tick_time);
    }
    result
}

/// `EEPMNaval::RemoveBob`: back at water level with no pitch or roll.
pub(crate) fn naval_remove_bob(c: &mut Ctx, u: EeUnitsPmUnit) {
    let mut t = c.h.location(u.obj);
    t.m[2][3] = 0.0;
    let yaw = heading(&t);
    ypr(&mut t, yaw, 0.0, 0.0);
    c.h.set_transformation(u.obj, &t);
}

/// The ships' `Step4` (`EEPMNaval`, `EEPMSubmergedNaval`, `EEPMSpaceship`): speed and turn braked toward a stop,
/// moved by what is left; at a stop the ship is levelled (`RemoveBob`) or the submarine left as it is. The ship and
/// the spaceship are bobbed after (`ApplyBob`), the spaceship keeps its roll.
fn ship_step4(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    let t = c.h.location(u.obj);
    let mut l = Ge3DPoint { x: t.x(), y: t.y(), z: if k == Kind::Spaceship { t.z() } else { 0.0 } };
    let step = c.t.tick_seconds;
    let (yaw_rate, yaw) = spin_down(c, u);
    c.set_angular(u, yaw_rate, 0.0, 0.0);
    let spaceship = k == Kind::Spaceship;
    let mut new = rotation(yaw, 0.0, if spaceship { rd!(u, mRoll) } else { 0.0 });
    let mut speed = rd!(u, mVelocity);
    speed = speed - c.h.acceleration(u.obj) * step;
    if speed < 0.0 {
        speed = 0.0;
    }
    let max = c.h.max_velocity(u.obj);
    if max < speed {
        speed = max;
    }
    set_velocity(u, speed);
    if yaw_rate == 0.0 && speed == 0.0 {
        if k.is_submerged() {
            return;
        }
        remove_bob(c, k, u);
    } else {
        let mv = new.apply_vector(fwd(speed * step));
        l = add(l, mv);
        let moved = clip_step(c, k, u, mv, &mut l);
        set_position(&mut new, l);
        if !moved {
            ypr(&mut new, yaw, 0.0, if spaceship { rd!(u, mRoll) } else { 0.0 });
        }
        c.h.set_transformation(u.obj, &new);
    }
    apply_bob(c, k, u);
}

pub(crate) fn naval_step4(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    ship_step4(c, k, u);
}

pub(crate) fn sub_step4(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    ship_step4(c, k, u);
}

pub(crate) fn spaceship_step4(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    ship_step4(c, k, u);
}

/// The ships' `Step5`: a stopped ship turned in place to its location goal's facing (the ship at water level, the
/// spaceship with its roll; both bobbed); a moving one brakes first; nothing without a location goal.
fn ship_step5(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    let mut t = c.h.location(u.obj);
    let mut goal_yaw = 0.0f32;
    if !c.h.goal_facing(u.obj, &mut goal_yaw) {
        return;
    }
    if rd!(u, mVelocity) != 0.0 {
        step4(c, k, u);
        return;
    }
    let current = heading(&t);
    let (_, yaw_rate, yaw) = turn_toward_angle(c, u, current, goal_yaw, false);
    c.set_angular(u, yaw_rate, 0.0, 0.0);
    let roll = if k == Kind::Spaceship { rd!(u, mRoll) } else { 0.0 };
    ypr(&mut t, yaw, 0.0, roll);
    if k == Kind::Naval {
        t.m[2][3] = 0.0;
    }
    c.h.set_transformation(u.obj, &t);
    apply_bob(c, k, u);
}

pub(crate) fn naval_step5(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    ship_step5(c, k, u);
}

pub(crate) fn sub_step5(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    ship_step5(c, k, u);
}

pub(crate) fn spaceship_step5(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    ship_step5(c, k, u);
}

// ---- EEPMSubmergedNaval, EEPMSeaAnimal

/// `EEPMSubmergedNaval::Step0`: as the ship's without the bob.
pub(crate) fn sub_step0(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    let mut t = c.h.location(u.obj);
    let mut l = Ge3DPoint { x: t.x(), y: t.y(), z: 0.0 };
    let step = c.t.tick_seconds;
    let mut target = Ge3DPoint::default();
    if c.h.next_waypoint(u.obj, &mut target) != 0 {
        return;
    }
    target.z = 0.0;
    let distance = flat_distance(target, l);
    let (turn, yaw_rate, yaw) = turn_toward(c, k, u, l, target, false);
    c.set_angular(u, yaw_rate, 0.0, 0.0);
    ypr(&mut t, yaw, 0.0, 0.0);
    let speed = accelerate(c, k, u, distance, turn);
    set_velocity(u, speed);
    let mv = t.apply_vector(fwd(speed * step));
    l = add(l, mv);
    let moved = clip_step(c, k, u, mv, &mut l);
    set_position(&mut t, l);
    if !moved {
        ypr(&mut t, yaw, 0.0, 0.0);
    }
    c.set_has_arrived(u, distance);
    c.h.set_transformation(u.obj, &t);
}

/// `EEPMSubmergedNaval::Step1` (the sea animal's too): as the ship's without the bob.
pub(crate) fn sub_step1(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    let mut t = c.h.location(u.obj);
    let mut l = Ge3DPoint { x: t.x(), y: t.y(), z: 0.0 };
    let brake = c.h.acceleration(u.obj);
    let speed = rd!(u, mVelocity);
    let step = c.t.tick_seconds;
    let (mut target, _) = step1_target(c, u, false, false);
    target.z = 0.0;
    let (yaw_rate, yaw) = if c.h.model_parts(u.obj) == 0 {
        let (_, r, y) = turn_toward(c, k, u, l, target, true);
        (r, y)
    } else {
        spin_down(c, u)
    };
    c.set_angular(u, yaw_rate, 0.0, 0.0);
    ypr(&mut t, yaw, 0.0, 0.0);
    let mut new = speed - step * brake;
    if new < 0.0 {
        new = 0.0;
    }
    set_velocity(u, new);
    let mv = t.apply_vector(fwd(new * step));
    l = add(l, mv);
    let moved = clip_step(c, k, u, mv, &mut l);
    set_position(&mut t, l);
    if !moved {
        ypr(&mut t, yaw, 0.0, 0.0);
    }
    c.h.set_transformation(u.obj, &t);
}

/// `EEPMSubmergedNaval::Move`: the push applied (capped at the top speed) onto a tile it can stand on, at its dive
/// depth there; the push cleared.
pub(crate) fn sub_move(c: &mut Ctx, u: EeUnitsPmUnit) {
    let max = c.h.max_velocity(u.obj);
    let p = push_step(c, u, max);
    if p.moving != 0 {
        let ty = floor(p.y);
        let tx = floor(p.x);
        let mut ok = c.h.can_stand_on(u.obj, tx, ty);
        if ok
            && (tile_x(u) as i64 - tx as i64).abs() == 1
            && (tile_y(u) as i64 - ty as i64).abs() == 1
            && !c.h.can_stand_on(u.obj, tx, tile_y(u))
            && !c.h.can_stand_on(u.obj, tile_x(u), ty)
        {
            ok = false;
        }
        if ok {
            c.h.set_location_xy(u.obj, p.x, p.y);
            if rd!(u, mCurrentPhysicsAction) == 5 && rd!(u, mCollisionRadius) >= MINIMUM_SPEED {
                c.set_angular(u, 0.0, 0.0, 0.0);
            }
            wr!(u, mCollisionResolutionCount, rd!(u, mCollisionResolutionCount) + c.t.tick_time);
            let mut l = position(&c.h.location(u.obj));
            c.h.adjust_dive_depth(u.obj, &mut l);
            c.h.set_location_point(u.obj, l);
        }
    }
    if rd!(u, mPushVelocityX) != 0.0 {
        wr!(u, mPushVelocityX, 0.0);
    }
    if rd!(u, mPushVelocityY) != 0.0 {
        wr!(u, mPushVelocityY, 0.0);
    }
}

/// The tiles a unit's inner collision sphere covers (`Collide` of the submarine and the helicopter): the min and
/// max tiles in x and y, and the radius.
pub(crate) fn sphere_tiles(c: &mut Ctx, u: EeUnitsPmUnit) -> (i32, i32, i32, i32, f32) {
    let radius = c.h.class(u.obj).inner_collision_sphere;
    let l = c.h.location(u.obj);
    let center = Ge2DPoint { x: l.x(), y: l.y() };
    let mut size = radius;
    if radius >= MINIMUM_SPEED {
        size = radius + radius;
    }
    // `theSize * 0.5` and `< 0.5` in double: halving is exact, the same in f32
    let mut reach = size * 0.5;
    if reach < 0.5 {
        reach = 0.5;
    }
    (floor(center.x - reach), floor(center.x + reach), floor(center.y - reach), floor(center.y + reach), radius)
}

/// `EEPMSubmergedNaval::Collide`: against the living collidable submarines and sea animals on the covered tiles,
/// until a contact is handled.
pub(crate) fn sub_collide(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    let (min_x, max_x, min_y, max_y, radius) = sphere_tiles(c, u);
    'tiles: for i in min_x..=max_x {
        for j in min_y..=max_y {
            let tile = c.h.tile(i, j);
            if tile == 0 {
                continue;
            }
            let mut node = c.h.tile_unit_first(tile);
            while node != 0 {
                let unit = c.h.tile_unit(node);
                let other = c.h.unit_complex(unit);
                let skip = unit == u.obj
                    || other == 0
                    || {
                        let category = c.h.unit_category(unit);
                        category != CATEGORY_SUBMERGED_NAVAL && category != CATEGORY_SEA_ANIMAL
                    }
                    || {
                        let o = c.h.view(other);
                        rd!(o, mIsCollidable) == 0 || !(hit_points(o) > 0)
                    };
                if !skip {
                    let o = c.h.view(other);
                    let hit = if c.h.class(other).inner_collision_sphere >= MINIMUM_SPEED && radius >= MINIMUM_SPEED {
                        push_moving(c, k, u, o)
                    } else {
                        push(c, u, o)
                    };
                    if hit {
                        break 'tiles;
                    }
                }
                node = c.h.tile_unit_next(node);
            }
        }
    }
}

/// `EEPMSeaAnimal::Step0` (and the air animal's circling: `circling_turn`): toward the next waypoint in 3D (straight
/// on at its height without one), turning, pitching and banking; arrived when close; a ripple where it dives.
pub(crate) fn sea_animal_step0(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    let mut t = c.h.location(u.obj);
    let step = c.t.tick_seconds;
    let mut l = position(&t);
    let old_z = l.z;
    let o = c.h.orientation(u.obj);
    ypr(&mut t, o.yaw, o.pitch, o.roll);
    c.h.set_location(u.obj, &t);
    let mut target = Ge3DPoint::default();
    let distance;
    if c.h.waypoint_count(u.obj) > 0 {
        c.h.next_waypoint(u.obj, &mut target);
        let mut d = l.distance_to(target);
        if d < 0.05 {
            l = target;
            d = 0.0;
        }
        distance = d;
    } else {
        let mv = t.apply_vector(fwd(1.0));
        target = add(l, mv);
        target.z = rd!(u, mHeight);
        distance = l.distance_to(target);
    }
    let (turn, yaw_rate, yaw) = turn_toward(c, k, u, l, target, false);
    let (pitch_rate, pitch) = pitch_toward_slope(c, k, u, l, target, distance);
    c.set_angular(u, yaw_rate, pitch_rate, 0.0);
    c.h.set_orientation(u.obj, ee_ge::types::GeOrientation { yaw, pitch, roll: 0.0 });
    ypr(&mut t, yaw, pitch, 0.0);
    let speed = accelerate(c, k, u, distance, turn);
    set_velocity(u, speed);
    let mv = t.apply_vector(fwd(speed * step));
    l = add(l, mv);
    set_position(&mut t, l);
    let bank = compute_pitch(c, k, u, pitch);
    let roll = compute_roll(c, k, u, yaw_rate);
    ypr(&mut t, yaw, bank, roll);
    c.h.update_tile(u.obj, &t);
    c.set_has_arrived(u, distance);
    if old_z > 0.0 && l.z <= 0.0 {
        c.h.land_ripple(Ge3DPoint { x: l.x, y: l.y, z: 0.0 });
    }
}

/// `EEPMSeaAnimal::TurnToward` (the air animal's is the same body): toward the point in physics order 5; else
/// circling, the turn rate sped up one way in order 0, the other way else.
pub(crate) fn circling_turn(
    c: &mut Ctx,
    u: EeUnitsPmUnit,
    from: Ge3DPoint,
    to: Ge3DPoint,
    facing: bool,
) -> (f32, f32, f32) {
    let step = c.t.tick_seconds;
    let stats = c.h.stats(u.obj);
    let (accel, max) = (stats.angular_acceleration, stats.max_angular_velocity);
    let state = rd!(u, mCurrentPhysicsOrder);
    let rate = c.h.angular_velocity(u.obj).yaw;
    if state == 5 {
        return turn_toward_base(c, u, from, to, facing);
    }
    let current = heading(&c.h.location(u.obj));
    let new = if state == 0 {
        let mut new = rate - accel * step;
        if new < -max {
            new = -max;
        }
        new
    } else {
        let mut new = accel * step + rate;
        if new > max {
            new = max;
        }
        new
    };
    (0.0, new, new * step + current)
}

/// The leap's boost (`EEPMSeaAnimal::Accelerate` and `SpeedMode`): toward a raised first waypoint, the height to
/// climb times 1.15, at least 1, doubled.
fn leap_factor(height: f32) -> f32 {
    let mut factor = height * 1.15;
    if factor <= 1.0 {
        factor = 1.0;
    }
    factor + factor
}

/// `EEPMSeaAnimal::Accelerate`: the base's arithmetic over the top speed and acceleration a leap toward a raised
/// waypoint from near the surface boosts.
pub(crate) fn sea_animal_accelerate(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit, distance: f32, turn: f32) -> f32 {
    let speed = rd!(u, mVelocity);
    let mut max_speed = c.h.max_velocity(u.obj);
    let mut accel = c.h.acceleration(u.obj);
    if c.h.waypoint_count(u.obj) > 1 && c.h.first_waypoint_z(u.obj) > 0.0 && c.h.location(u.obj).z() < 0.3 {
        let factor = leap_factor(c.h.first_waypoint_z(u.obj) - c.h.location(u.obj).z());
        max_speed *= factor;
        accel *= factor;
    }
    let mode = speed_mode(c, k, u, distance, turn);
    accelerate_with(c, u, speed, max_speed, accel, distance, mode)
}

/// `EEPMSeaAnimal::SpeedMode`: brakes for sharp turns, near the path's end and on steep terrain; heights above or
/// below the surface scale its top speed and braking.
pub(crate) fn sea_animal_speed_mode(c: &mut Ctx, u: EeUnitsPmUnit, distance: f32, turn: f32) -> i32 {
    let step = c.t.tick_seconds;
    let mut brake = c.h.acceleration(u.obj);
    let mut max_speed = c.h.max_velocity(u.obj);
    let speed = rd!(u, mVelocity);
    let mut limit = distance;
    let penalty = c.h.class(u.obj).slope_penalty;
    if c.h.waypoint_count(u.obj) > 1 {
        let first = c.h.first_waypoint_z(u.obj);
        let z = c.h.location(u.obj).z();
        if first > 0.0 {
            let factor = leap_factor(first - z);
            max_speed *= factor;
            brake *= factor;
        } else if z > 0.015 {
            let mut factor = (z - 1.15) * 1.05;
            if factor <= 1.0 {
                factor = 1.0;
            }
            factor = factor + factor;
            max_speed *= factor;
            brake *= factor;
        }
    }
    let time = c.t.ticks_per_second;
    let ticks = c.h.inverse_acceleration(u.obj) * time * speed;
    let mut stop = (ticks + 1.0) * ticks * 0.5;
    stop = stop * (step * step) * brake;
    if stop < 0.0001 {
        stop = 0.0;
    }
    if distance > 0.5 {
        limit = 0.5;
    }
    let mut slide = c.h.angular_velocity(u.obj).yaw * (limit * 0.5) * step;
    if slide < 0.0 {
        slide = -slide;
    }
    let mut result = if turn < -0.785_398_2 || turn > 0.785_398_2 {
        if speed * step < slide {
            if (brake * step + speed) * step < slide {
                3
            } else {
                2
            }
        } else {
            4
        }
    } else {
        3
    };
    let mut remaining = distance;
    if c.h.waypoint_count(u.obj) > 1 {
        remaining = 0.0;
        c.h.path_length(u.obj, &mut remaining);
    }
    if remaining <= stop {
        result = 4;
    }
    if !(penalty < 1.0) {
        return result;
    }
    let location = c.h.location(u.obj);
    let mut slope = c.h.terrain_pitch(&location) * 1.909_859_3;
    if slope < 0.0 {
        slope = 0.0;
    } else if slope > 1.0 {
        slope = 1.0;
    }
    if rd!(u, mVelocity) > (1.0 - slope * penalty) * max_speed {
        result = 4;
    }
    result
}

// ---- EEPMSpaceship

/// `EEPMSpaceship::Step0`: toward its next waypoint at its height (a planet spaceship without one flies straight on
/// and pitches with the slope); arrived when close; its next height set.
pub(crate) fn spaceship_step0(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    let mut t = c.h.location(u.obj);
    let mut l = position(&t);
    let step = c.t.tick_seconds;
    let mut pitch_rate = 0.0f32;
    let mut pitch = 0.0f32;
    let planet = c.h.class(u.obj).category == CATEGORY_PLANET_SPACESHIP;
    let mut target = Ge3DPoint::default();
    let distance;
    if c.h.next_waypoint(u.obj, &mut target) == 0 {
        target.z = rd!(u, mHeight);
        distance = flat_distance(target, l);
    } else {
        if !planet {
            return;
        }
        let mv = t.apply_vector(fwd(1.0));
        target = add(l, mv);
        target.z = rd!(u, mHeight);
        distance = l.distance_to(target);
    }
    let (turn, yaw_rate, yaw) = turn_toward(c, k, u, l, target, false);
    if planet {
        // `EEPhysicsModel::PitchTowardSlope`: the base's, over this model's ApproachPitch and PitchMode
        let (r, p) = pitch_toward_slope(c, k, u, l, target, distance);
        pitch_rate = r;
        pitch = p;
    }
    c.set_angular(u, yaw_rate, pitch_rate, 0.0);
    ypr(&mut t, yaw, pitch, rd!(u, mRoll));
    let speed = accelerate(c, k, u, distance, turn);
    set_velocity(u, speed);
    let mv = t.apply_vector(fwd(speed * step));
    l = add(l, mv);
    let moved = clip_step(c, k, u, mv, &mut l);
    set_position(&mut t, l);
    if !moved {
        ypr(&mut t, yaw, pitch, rd!(u, mRoll));
    }
    c.set_has_arrived(u, distance);
    c.h.set_transformation(u.obj, &t);
    if c.h.waypoint_count(u.obj) > 0 {
        let mut next = Ge3DPoint::default();
        c.h.next_waypoint(u.obj, &mut next);
        let height = c.h.height_at(u.obj, next);
        if rd!(u, mHeight) != height {
            wr!(u, mHeight, height);
        }
    }
}

/// `EEPMSpaceship::Step1`: turned toward the unit it attacks or heals (or its goal location) while braking; against
/// a planet spaceship (except a capital ship's special attack) or with model parts it only spins down.
pub(crate) fn spaceship_step1(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    let mut t = c.h.location(u.obj);
    let mut l = position(&t);
    let brake = c.h.acceleration(u.obj);
    let speed = rd!(u, mVelocity);
    let step = c.t.tick_seconds;
    let (mut target, mut air) = step1_target(c, u, false, false);
    target.z = t.z();
    let class = c.h.class(u.obj);
    if (class.unit_type == UNIT_TYPE_SP15_CAPITAL_SHIP || class.unit_type == UNIT_TYPE_CAPITAL_SHIP_YAMATO)
        && rb!(u, mIsSpecialAttack)
    {
        air = false;
    }
    let (yaw_rate, yaw) = if c.h.model_parts(u.obj) == 0 && !air {
        let (_, r, y) = turn_toward(c, k, u, l, target, true);
        (r, y)
    } else {
        spin_down(c, u)
    };
    c.set_angular(u, yaw_rate, 0.0, 0.0);
    ypr(&mut t, yaw, 0.0, rd!(u, mRoll));
    let mut new = speed - step * brake;
    if new < 0.0 {
        new = 0.0;
    }
    set_velocity(u, new);
    let mv = t.apply_vector(fwd(new * step));
    l = add(l, mv);
    let moved = clip_step(c, k, u, mv, &mut l);
    set_position(&mut t, l);
    if !moved {
        ypr(&mut t, yaw, 0.0, rd!(u, mRoll));
    }
    c.h.set_transformation(u.obj, &t);
}

/// `EEPMSpaceship::RemoveBob`: the pitch levelled, the heading, height and roll kept.
pub(crate) fn spaceship_remove_bob(c: &mut Ctx, u: EeUnitsPmUnit) {
    let mut t = c.h.location(u.obj);
    let yaw = heading(&t);
    ypr(&mut t, yaw, 0.0, rd!(u, mRoll));
    c.h.set_transformation(u.obj, &t);
}

// ---- EEPMBuilding

/// `EEPMBuilding::Step2`: a collapsing building tilted (not an airport), jittered within its tile (two draws) and
/// lowered; a gate's first part also sinks its last linked carrier and its last linked part.
pub(crate) fn building_step2(c: &mut Ctx, u: EeUnitsPmUnit) {
    let original = c.h.location(u.obj);
    let mut t = original;
    let mut l = position(&t);
    let step = c.t.tick_seconds;
    let class = c.h.class(u.obj);
    if class.unit_type != UNIT_TYPE_AIRPORT {
        let r = forward(&t);
        let yaw = um_arctan2(-r.x, r.y);
        let pitch = um_arccos(-r.z) - HALF_PI - step * 0.03;
        ypr(&mut t, yaw, pitch, 0.0);
    }
    let epsilon = c.t.height_epsilon;
    l.x += (0.5 - c.h.random()) * epsilon;
    l.y = (0.5 - c.h.random()) * epsilon + l.y;
    l.z = l.z - step * 0.13;
    if floor(l.x) == floor(original.x()) && floor(l.y) == floor(original.y()) {
        set_position(&mut t, l);
    }
    c.h.set_location(u.obj, &t);
    if class.category != CATEGORY_GATE {
        return;
    }
    let (mut carrier, mut last) = (0u64, 0u64);
    if c.h.gate_links(u.obj, &mut carrier, &mut last) {
        if carrier != 0 {
            let part = c.h.view(carrier);
            building_step2(c, part);
        }
        if last != 0 {
            let part = c.h.view(last);
            building_step2(c, part);
        }
    }
}
