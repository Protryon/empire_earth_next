//! The physics models in the air: `EEPMHelicopter`, `EEPMAirplane` and `EEPMAirAnimal` (the airplane's bodies with
//! its own `Step0`, speed mode and circling turn). pm.rs has the dispatch and the base model.

#![allow(
    clippy::neg_cmp_op_on_partial_ord,
    clippy::assign_op_pattern,
    clippy::manual_clamp,
    clippy::approx_constant,
    clippy::collapsible_if,
    clippy::collapsible_else_if,
    clippy::needless_late_init,
    clippy::unnecessary_cast
)]

use ee_ge::types::{Ge3DPoint, GeOrientation, GeVector};
use ee_ge::um::{floor, um_arccos, um_arctan2, um_sin};

use crate::pm::*;
use crate::pm_ground::step1_target;

// ---- the displayed pitch and roll

/// `EEPMAirplane::ComputePitch` (`cap` 15 degrees; `EEPMSeaAnimal`'s the same with 25): a descending unit's dive
/// is shown nose up, capped.
pub(crate) fn displayed_dive(c: &mut Ctx, u: EeUnitsPmUnit, v: f32, cap: f32) -> f32 {
    if !(v < 0.0) {
        return v;
    }
    let command = c.h.command_type(u.obj);
    if command != ACTION_DESCEND && command != ACTION_CONTINUE_TO_DESCEND {
        return v;
    }
    if -v > cap {
        return cap;
    }
    -v
}

/// `EEPMAirplane::ComputeRoll` (`EEPMSeaAnimal`'s the same body): the bank for the turn rate up to the class's
/// maximum roll, none below 10% of the stats' top turn rate.
pub(crate) fn bank_for_turn(c: &mut Ctx, u: EeUnitsPmUnit, v: f32) -> f32 {
    let mut ratio = v / c.h.stats(u.obj).max_angular_velocity;
    if v < 0.0 {
        ratio = ratio + 0.100_000_024;
        if ratio > 0.0 {
            ratio = 0.0;
        }
    } else {
        ratio = ratio - 0.100_000_024;
        if ratio < 0.0 {
            ratio = 0.0;
        }
    }
    (ratio * 1.111_111_2) * c.h.class(u.obj).max_roll
}

/// `EEPMHelicopter::ComputePitch`: the pitch scaled by the share of top speed (0 when the class does not pitch).
pub(crate) fn heli_compute_pitch(c: &mut Ctx, u: EeUnitsPmUnit, v: f32) -> f32 {
    let ratio = rd!(u, mVelocity) / c.h.max_velocity(u.obj);
    if c.h.class(u.obj).pitches_with_terrain != 0 {
        ratio * v
    } else {
        0.0
    }
}

/// `EEPMHelicopter::ComputeRoll`: as the airplane's over the unit's (its group's) top turn rate, 0 when the class
/// does not roll.
pub(crate) fn heli_compute_roll(c: &mut Ctx, u: EeUnitsPmUnit, v: f32) -> f32 {
    let class = c.h.class(u.obj);
    if class.rolls_with_terrain == 0 {
        return 0.0;
    }
    let ratio = if v < 0.0 {
        let mut r = v / c.h.max_angular_velocity(u.obj) + 0.100_000_024;
        if r > 0.0 {
            r = 0.0;
        }
        r
    } else {
        let mut r = v / c.h.max_angular_velocity(u.obj) - 0.100_000_024;
        if r < 0.0 {
            r = 0.0;
        }
        r
    };
    (ratio * 1.111_111_2) * class.max_roll
}

/// `EEPMHelicopter::ClipStep`: a move onto a tile the unit cannot enter or an edge tile undone
/// (false), else clipped to the map's edges.
pub(crate) fn air_clip_step(c: &mut Ctx, u: EeUnitsPmUnit, mv: GeVector, io: &mut Ge3DPoint) -> bool {
    let x = floor(io.x);
    let y = floor(io.y);
    let tile = c.h.tile(x, y);
    if tile != 0 && tile != c.h.tile_of(u.obj) {
        let info = c.h.tile_info(tile);
        if !c.h.can_move_on_terrain(u.obj, info.terrain_type) || c.h.is_edge_tile(x, y) {
            io.x = io.x - mv.x;
            io.y = io.y - mv.y;
            return false;
        }
    }
    clip_step_base(c, mv, io)
}

/// `EEPMAirplane::ClipStep`: as the helicopter's, but a move off the map is undone here in x and y (the base undoes
/// it along the whole move). No step of the airplanes calls it; kept for the dispatch.
pub(crate) fn airplane_clip_step(c: &mut Ctx, u: EeUnitsPmUnit, mv: GeVector, io: &mut Ge3DPoint) -> bool {
    let x = floor(io.x);
    let y = floor(io.y);
    let tile = c.h.tile(x, y);
    let mut free = tile != 0;
    if tile != 0 && tile != c.h.tile_of(u.obj) {
        let info = c.h.tile_info(tile);
        if !c.h.can_move_on_terrain(u.obj, info.terrain_type) || c.h.is_edge_tile(x, y) {
            free = false;
        }
    }
    if free {
        return clip_step_base(c, mv, io);
    }
    io.x = io.x - mv.x;
    io.y = io.y - mv.y;
    false
}

// ---- EEPMHelicopter

/// `EEPMHelicopter::ApplyBob`: the hover phase advanced (kept in -pi..pi), the helicopter lowered by the new bob.
pub(crate) fn heli_apply_bob(c: &mut Ctx, u: EeUnitsPmUnit) {
    let mut t = c.h.location(u.obj);
    let mut p = position(&t);
    let mut angle = c.t.tick_time * 0.001_570_796_5 + rd!(u, mF220);
    while angle > PI {
        angle -= TWO_PI;
    }
    if rd!(u, mF220) != angle {
        wr!(u, mF220, angle);
    }
    let bob = (um_sin(angle) + 1.0) * 0.5;
    p.z = p.z - c.h.class(u.obj).bob_height * bob;
    set_position(&mut t, p);
    c.h.set_transformation(u.obj, &t);
}

/// `EEPMHelicopter::RemoveBob`: raised by its class's bob height times (sin(phase) + 1) / 2.
pub(crate) fn heli_remove_bob(c: &mut Ctx, u: EeUnitsPmUnit) {
    let mut t = c.h.location(u.obj);
    let mut p = position(&t);
    let bob = (um_sin(rd!(u, mF220)) + 1.0) * 0.5;
    p.z = c.h.class(u.obj).bob_height * bob + p.z;
    set_position(&mut t, p);
    c.h.set_transformation(u.obj, &t);
}

/// `EEPMHelicopter::Step0`: toward the next waypoint at its height (hovering without one), climbing, turning and
/// accelerating, the hover bob taken out and put back.
pub(crate) fn heli_step0(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    remove_bob(c, k, u);
    let mut t = c.h.location(u.obj);
    let mut l = position(&t);
    let step = c.t.tick_seconds;
    let height = rd!(u, mHeight);
    let o = c.h.orientation(u.obj);
    ypr(&mut t, o.yaw, o.pitch, o.roll);
    let mut target = Ge3DPoint::default();
    let distance;
    let climb;
    if c.h.waypoint_count(u.obj) > 0 {
        c.h.next_waypoint(u.obj, &mut target);
        target.z = height;
        let d = l.distance_to(target);
        if d >= 0.05 {
            climb = um_arccos(-((target.z - l.z) / d)) - HALF_PI;
            distance = d;
        } else {
            distance = 0.0;
            l = target;
            climb = 0.0;
        }
    } else {
        distance = 0.0;
        target = l;
        climb = 0.0;
    }
    let (turn, yaw_rate, yaw) = turn_toward(c, k, u, l, target, false);
    let current = c.h.orientation(u.obj).pitch;
    let (pitch_rate, pitch) = approach_pitch(c, k, u, current, climb);
    c.set_angular(u, yaw_rate, pitch_rate, 0.0);
    c.h.set_orientation(u.obj, GeOrientation { yaw, pitch, roll: 0.0 });
    ypr(&mut t, yaw, pitch, 0.0);
    let speed = accelerate(c, k, u, distance, turn);
    set_velocity(u, speed);
    let mv = t.apply_vector(fwd(speed * step));
    l = add(l, mv);
    clip_step(c, k, u, mv, &mut l);
    set_position(&mut t, l);
    let bank = compute_pitch(c, k, u, pitch);
    let roll = compute_roll(c, k, u, yaw_rate);
    ypr(&mut t, yaw, bank, roll);
    c.set_has_arrived(u, distance);
    c.h.set_transformation(u.obj, &t);
    apply_bob(c, k, u);
}

/// `EEPMHelicopter::Step1`: turned toward the unit it attacks or heals, its calamity's target or its goal location,
/// braking to a stop, the hover bob taken out and put back.
pub(crate) fn heli_step1(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    remove_bob(c, k, u);
    let mut t = c.h.location(u.obj);
    let mut l = position(&t);
    let speed = rd!(u, mVelocity);
    let brake = c.h.acceleration(u.obj);
    let step = c.t.tick_seconds;
    let height = rd!(u, mHeight);
    let o = c.h.orientation(u.obj);
    ypr(&mut t, o.yaw, o.pitch, o.roll);
    let (mut target, _) = step1_target(c, u, true, true);
    target.z = height;
    let distance = l.distance_to(target);
    let (_, yaw_rate, yaw) = turn_toward(c, k, u, l, target, false);
    let current = c.h.orientation(u.obj).pitch;
    let (pitch_rate, pitch) = approach_pitch(c, k, u, current, 0.0);
    c.set_angular(u, yaw_rate, pitch_rate, 0.0);
    c.h.set_orientation(u.obj, GeOrientation { yaw, pitch, roll: 0.0 });
    ypr(&mut t, yaw, pitch, 0.0);
    let mut new = speed - step * brake;
    if new < 0.0 {
        new = 0.0;
    }
    set_velocity(u, new);
    let mv = t.apply_vector(fwd(new * step));
    l = add(l, mv);
    clip_step(c, k, u, mv, &mut l);
    set_position(&mut t, l);
    let bank = compute_pitch(c, k, u, pitch);
    let roll = compute_roll(c, k, u, yaw_rate);
    ypr(&mut t, yaw, bank, roll);
    c.set_has_arrived(u, distance);
    c.h.set_transformation(u.obj, &t);
    apply_bob(c, k, u);
}

/// `EEPMHelicopter::Move`: the push (capped at its speed) applied onto a tile it can stand on; the push cleared.
pub(crate) fn heli_move(c: &mut Ctx, u: EeUnitsPmUnit) {
    let speed = rd!(u, mVelocity);
    let p = push_step(c, u, speed);
    if p.moving != 0 {
        let ty = floor(p.y);
        let tx = floor(p.x);
        if c.h.can_stand_on(u.obj, tx, ty) {
            c.h.set_location_xy(u.obj, p.x, p.y);
            if rd!(u, mCurrentPhysicsAction) == 5 && c.h.class(u.obj).inner_collision_sphere >= MINIMUM_SPEED {
                c.set_angular(u, 0.0, 0.0, 0.0);
            }
            wr!(u, mCollisionResolutionCount, rd!(u, mCollisionResolutionCount) + c.t.tick_time);
        }
    }
    if rd!(u, mPushVelocityX) != 0.0 {
        wr!(u, mPushVelocityX, 0.0);
    }
    if rd!(u, mPushVelocityY) != 0.0 {
        wr!(u, mPushVelocityY, 0.0);
    }
}

/// `EEPMHelicopter::Step2`: the shot-down helicopter spun and pitched down, a splash on reaching water, stopped
/// with its death effect on hitting the ground; a landed one sinks.
pub(crate) fn heli_step2(c: &mut Ctx, _k: Kind, u: EeUnitsPmUnit) {
    let mut t = c.h.location(u.obj);
    let l = position(&t);
    let mut new = Ge3DPoint::default();
    let step = c.t.tick_seconds;
    let stats = c.h.stats(u.obj);
    let mut max_rate = stats.max_angular_velocity;
    let accel = stats.angular_acceleration;
    let brake = c.h.acceleration(u.obj);
    let mut speed = rd!(u, mVelocity);
    let rate = c.h.angular_velocity(u.obj);
    let mut yaw_rate = rate.yaw;
    let mut pitch_rate = rate.pitch;
    let landed = l.z <= 0.0;
    let y = floor(l.y);
    let x = floor(l.x);
    let mut tile = c.h.tile(x, y);
    let class = c.h.class(u.obj);
    if landed {
        ypr(&mut t, 0.0, 0.0, 0.0);
        new.x = l.x;
        new.y = l.y;
        new.z = l.z - step * 0.5;
        set_position(&mut t, new);
    } else {
        let turn = accel * 7.5;
        max_rate = max_rate * 7.5;
        pitch_rate = pitch_rate - turn * step;
        if pitch_rate < -HALF_PI {
            pitch_rate = -HALF_PI;
        }
        if class.rolls_with_terrain == 0 || class.pitches_with_terrain == 0 {
            yaw_rate = yaw_rate - turn * step;
            if yaw_rate < 0.0 {
                yaw_rate = 0.0;
            }
        } else if yaw_rate >= 0.0 {
            yaw_rate = turn * step + yaw_rate;
            if max_rate < yaw_rate {
                yaw_rate = max_rate;
            }
        } else {
            let left = yaw_rate - turn * step;
            yaw_rate = -max_rate;
            if yaw_rate <= left {
                yaw_rate = left;
            }
        }
        c.set_angular(u, yaw_rate, pitch_rate, 0.0);
        t.revolve(yaw_rate * step, 0.0, 0.0);
        let r = forward(&t);
        let course = rotation(um_arctan2(-r.x, r.y), pitch_rate, 0.0);
        speed = brake * step + speed;
        let max = c.h.max_velocity(u.obj);
        if max < speed {
            speed = max;
        }
        set_velocity(u, speed);
        let r = course.apply_vector(fwd(speed * step));
        new = add(l, r);
        let ny = floor(new.y);
        let nx = floor(new.x);
        let new_tile = c.h.tile(nx, ny);
        if new_tile == 0 {
            new.y = l.y;
            new.x = l.x;
            set_position(&mut t, new);
        } else {
            tile = new_tile;
            set_position(&mut t, new);
        }
        if new.z <= 0.0 {
            c.h.set_animation(u.obj, 5, -1, false);
            if c.h.tile_info(tile).height < 0.0 && class.has_splash_effect != 0 {
                c.h.splash(u.obj, Ge3DPoint { x: new.x, y: new.y, z: 0.0 });
            }
        }
    }
    let height = c.h.tile_info(tile).height;
    if new.z < height {
        set_velocity(u, 0.0);
        if height >= 0.0 && class.has_death_effect != 0 {
            c.h.death_effect(u.obj);
        }
    }
    c.h.set_transformation(u.obj, &t);
}

/// `EEPMHelicopter::Collide`: the helicopter pushed off the living collidable flying units on the tiles it covers
/// (`Push`, which never ends the walk).
pub(crate) fn heli_collide(c: &mut Ctx, _k: Kind, u: EeUnitsPmUnit) {
    let (min_x, max_x, min_y, max_y, _) = crate::pm_ground::sphere_tiles(c, u);
    'tiles: for i in min_x..=max_x {
        for j in min_y..=max_y {
            let tile = c.h.tile(i, j);
            if tile == 0 {
                continue;
            }
            let mut node = c.h.tile_unit_first(tile);
            while node != 0 {
                let unit = c.h.tile_unit(node);
                let other = if unit == u.obj { 0 } else { c.h.unit_complex(unit) };
                if other != 0 && c.h.flies_over(unit) {
                    let o = c.h.view(other);
                    if rd!(o, mIsCollidable) != 0 && hit_points(o) > 0 && push(c, u, o) {
                        break 'tiles;
                    }
                }
                node = c.h.tile_unit_next(node);
            }
        }
    }
}

/// `EEPMHelicopter::Step4`: speed, turn and pitch slowed to a hover (levelled once stopped), the bob taken out and
/// put back.
pub(crate) fn heli_step4(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    remove_bob(c, k, u);
    let mut t = c.h.location(u.obj);
    if rd!(u, mVelocity) == 0.0 && c.h.angular_velocity(u.obj).yaw == 0.0 {
        let yaw = heading(&t);
        ypr(&mut t, yaw, 0.0, 0.0);
    } else {
        let mut l = position(&t);
        let brake = c.h.acceleration(u.obj);
        let mut speed = rd!(u, mVelocity);
        let step = c.t.tick_seconds;
        let (yaw_rate, yaw) = spin_down(c, u);
        let current = c.h.orientation(u.obj).pitch;
        let (pitch_rate, pitch) = approach_pitch(c, k, u, current, 0.0);
        c.set_angular(u, yaw_rate, pitch_rate, 0.0);
        ypr(&mut t, yaw, pitch, 0.0);
        speed = speed - step * brake;
        if speed < 0.0 {
            speed = 0.0;
        }
        let max = c.h.max_velocity(u.obj);
        if max < speed {
            speed = max;
        }
        set_velocity(u, speed);
        let mv = t.apply_vector(fwd(speed * step));
        l = add(l, mv);
        clip_step(c, k, u, mv, &mut l);
        set_position(&mut t, l);
        let bank = compute_pitch(c, k, u, pitch);
        let roll = compute_roll(c, k, u, yaw_rate);
        ypr(&mut t, yaw, bank, roll);
    }
    c.h.set_transformation(u.obj, &t);
    apply_bob(c, k, u);
}

/// `EEPMHelicopter::Step5`: a stopped helicopter turned to its location goal's facing (a moving one slows first).
pub(crate) fn heli_step5(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    let mut goal_yaw = 0.0f32;
    if !c.h.goal_facing(u.obj, &mut goal_yaw) {
        return;
    }
    if rd!(u, mVelocity) != 0.0 {
        step4(c, k, u);
        return;
    }
    remove_bob(c, k, u);
    let mut t = c.h.location(u.obj);
    let o = c.h.orientation(u.obj);
    ypr(&mut t, o.yaw, o.pitch, o.roll);
    let (pitch_rate, pitch) = approach_pitch(c, k, u, o.pitch, 0.0);
    let current = heading(&t);
    let (_, yaw_rate, yaw) = turn_toward_angle(c, u, current, goal_yaw, false);
    c.set_angular(u, yaw_rate, pitch_rate, 0.0);
    let bank = compute_pitch(c, k, u, pitch);
    let roll = compute_roll(c, k, u, yaw_rate);
    c.h.set_orientation(u.obj, GeOrientation { yaw, pitch, roll: 0.0 });
    ypr(&mut t, yaw, bank, roll);
    c.h.set_transformation(u.obj, &t);
    apply_bob(c, k, u);
}

// ---- EEPMAirplane, EEPMAirAnimal

/// The carrier aircraft's size by height (`mAirTrafficType` 2 or 4; the air animal's 2 alone, with no floor at 0):
/// full at cruise altitude, shrinking to half below it.
fn carrier_scale(c: &mut Ctx, u: EeUnitsPmUnit, z: f32, animal: bool) {
    let class = c.h.class(u.obj);
    let carried =
        if animal { class.air_traffic_type == 2 } else { class.air_traffic_type == 2 || class.air_traffic_type == 4 };
    if !carried {
        return;
    }
    let scale = c.h.stats(u.obj).scale;
    if z >= class.cruise_altitude {
        c.h.set_scale(u.obj, scale);
    } else if animal || z >= 0.0 {
        let mut s = z / class.cruise_altitude * 0.5;
        s += 0.5;
        c.h.set_scale(u.obj, s * scale);
    } else {
        c.h.set_scale(u.obj, scale * 0.5);
    }
}

/// `EEPMAirplane::Step0` (`EEPMAirAnimal::Step0` with `animal`: no descent floor, no roll kept): toward its next
/// waypoint at its height (straight on without one), following its carrier while docked; arrived when close.
fn fly_step0(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit, animal: bool) {
    let mut t = c.h.location(u.obj);
    let command = c.h.command_type(u.obj);
    let step = c.t.tick_seconds;
    if c.h.docked(u.obj) {
        c.h.follow_carrier(u.obj);
        t = c.h.location(u.obj);
    }
    let mut l = position(&t);
    carrier_scale(c, u, l.z, animal);
    let o = c.h.orientation(u.obj);
    ypr(&mut t, o.yaw, o.pitch, o.roll);
    c.h.set_location(u.obj, &t);
    let mut target = Ge3DPoint::default();
    let distance;
    if c.h.waypoint_count(u.obj) > 0 {
        c.h.next_waypoint(u.obj, &mut target);
        target.z = rd!(u, mHeight);
        let d = l.distance_to(target);
        if d < 0.05 {
            distance = 0.0;
            l = target;
        } else {
            distance = d;
        }
    } else {
        let mv = t.apply_vector(fwd(1.0));
        target = add(l, mv);
        target.z = rd!(u, mHeight);
        distance = l.distance_to(target);
    }
    let (turn, yaw_rate, yaw) = turn_toward(c, k, u, l, target, false);
    let (pitch_rate, pitch) = pitch_toward_slope(c, k, u, l, target, distance);
    c.set_angular(u, yaw_rate, pitch_rate, 0.0);
    let roll = if animal { 0.0 } else { rd!(u, mRoll) };
    c.h.set_orientation(u.obj, GeOrientation { yaw, pitch, roll });
    ypr(&mut t, yaw, pitch, 0.0);
    let speed = accelerate(c, k, u, distance, turn);
    set_velocity(u, speed);
    let mv = t.apply_vector(fwd(speed * step));
    l = add(l, mv);
    if !animal && (command == ACTION_DESCEND || command == ACTION_CONTINUE_TO_DESCEND) {
        let ground = c.h.mesh_ground(l.x, l.y);
        if l.z < ground {
            l.z = ground;
        }
    }
    set_position(&mut t, l);
    let bank = compute_pitch(c, k, u, pitch);
    let roll_offset = compute_roll(c, k, u, yaw_rate);
    let roll = if animal { roll_offset } else { rd!(u, mRoll) + roll_offset };
    ypr(&mut t, yaw, bank, roll);
    c.h.update_tile(u.obj, &t);
    c.set_has_arrived(u, distance);
}

pub(crate) fn airplane_step0(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    fly_step0(c, k, u, false);
}

pub(crate) fn air_animal_step0(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    fly_step0(c, k, u, true);
}

/// `EEPMAirplane::Step1` (the air animal's too): toward the unit it attacks (or five tiles straight on without one),
/// turning with its facing offset.
pub(crate) fn airplane_step1(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    let mut t = c.h.location(u.obj);
    let mut l = position(&t);
    let step = c.t.tick_seconds;
    carrier_scale(c, u, l.z, false);
    let attack = c.h.attack_target(u.obj);
    let mut target = Ge3DPoint::default();
    if attack != 0 {
        let complex = c.h.unit_complex(attack);
        if complex != 0 {
            let tl = c.h.location(complex);
            target.x = tl.x();
            target.y = tl.y();
        } else {
            let (x, y) = c.h.unit_tile(attack);
            target.x = x as f32 + 0.5;
            target.y = y as f32 + 0.5;
        }
        target.z = rd!(u, mHeight);
    } else {
        let mut mv = t.apply_vector(fwd(5.0));
        mv.z = 0.0;
        target.z = l.z;
        target.x = mv.x + l.x;
        target.y = mv.y + l.y;
    }
    let distance = l.distance_to(target);
    let (turn, yaw_rate, yaw) = turn_toward(c, k, u, l, target, true);
    let (pitch_rate, pitch) = pitch_toward_slope(c, k, u, l, target, distance);
    c.set_angular(u, yaw_rate, pitch_rate, 0.0);
    c.h.set_orientation(u.obj, GeOrientation { yaw, pitch, roll: 0.0 });
    ypr(&mut t, yaw, pitch, 0.0);
    let speed = accelerate(c, k, u, distance, turn);
    set_velocity(u, speed);
    let mv = t.apply_vector(fwd(speed * step));
    l = add(l, mv);
    set_position(&mut t, l);
    let bank = compute_pitch(c, k, u, pitch);
    let roll = rd!(u, mRoll) + compute_roll(c, k, u, yaw_rate);
    ypr(&mut t, yaw, bank, roll);
    c.h.update_tile(u.obj, &t);
}

/// `EEPMAirplane::SpeedMode` (`EEPMAirAnimal`'s with `animal`: only traffic type 2 is carried, any path length):
/// slowing (4) above 3/4 of top speed plus the carrier's on a descent or brake (the base mode asked all the same),
/// the base mode when taxiing (not carried), else cruising (3).
pub(crate) fn airplane_speed_mode(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit, distance: f32, turn: f32) -> i32 {
    let animal = k == Kind::AirAnimal;
    let home = c.h.home_base(u.obj);
    let home_speed = if home != 0 {
        let h = c.h.view(home);
        rd!(h, mVelocity)
    } else {
        0.0
    };
    let limit = c.h.stats(u.obj).max_velocity * 0.75 + home_speed;
    let class = c.h.class(u.obj);
    let carried =
        if animal { class.air_traffic_type == 2 } else { class.air_traffic_type == 2 || class.air_traffic_type == 4 };
    let waypoints = c.h.waypoint_count(u.obj);
    let command = c.h.command_type(u.obj);
    let near_end = animal || (!carried && waypoints <= 6) || (carried && waypoints <= 3);
    if near_end
        && (command == ACTION_DESCEND
            || command == ACTION_CONTINUE_TO_DESCEND
            || command == ACTION_BRAKE
            || command == ACTION_CONTINUE_TO_BRAKE)
    {
        speed_mode_with(c, u, false, distance, turn);
        return if rd!(u, mVelocity) > limit { 4 } else { 3 };
    }
    if command == ACTION_TAXI && !carried {
        return speed_mode_with(c, u, false, distance, turn);
    }
    3
}

/// `EEPMAirplane::Step2` (the air animal's too): the shot-down plane pitched down and rolled, accelerating, a
/// splash on reaching water, stopped with its death effect and impact decal on hitting the ground.
pub(crate) fn airplane_step2(c: &mut Ctx, _k: Kind, u: EeUnitsPmUnit) {
    let mut t = c.h.location(u.obj);
    let l = position(&t);
    let mut new = Ge3DPoint::default();
    let step = c.t.tick_seconds;
    let stats = c.h.stats(u.obj);
    let mut max_rate = stats.max_angular_velocity;
    let mut accel = stats.angular_acceleration;
    let brake = c.h.acceleration(u.obj);
    let mut speed = rd!(u, mVelocity);
    let rate = c.h.angular_velocity(u.obj);
    let mut roll_rate = rate.roll;
    let mut pitch_rate = rate.pitch;
    let yaw_rate = rate.yaw;
    let airborne = l.z > 0.0;
    let landing = c.h.command_type(u.obj) == ACTION_FADING_OUT_IN_SPACE;
    let class = c.h.class(u.obj);
    carrier_scale(c, u, l.z, false);
    let mut tile = c.h.tile_of(u.obj);
    let r = forward(&t);
    let heading = um_arctan2(-r.x, r.y);
    let pitch = um_arccos(-r.z) - HALF_PI;
    let mut roll = rd!(u, mF220);
    if airborne || landing {
        pitch_rate = pitch_rate - accel * step;
        if pitch_rate < -max_rate {
            pitch_rate = -max_rate;
        }
        let mut new_pitch = pitch_rate * step + pitch;
        if new_pitch < -1.256_637_1 {
            new_pitch = -1.256_637_1;
        }
        accel = accel * 4.0;
        max_rate = max_rate * 4.0;
        if yaw_rate < 0.0 {
            roll_rate = roll_rate - accel * step;
            accel = -max_rate;
            if roll_rate < accel {
                roll_rate = accel;
            }
        } else {
            roll_rate = accel * step + roll_rate;
            if roll_rate > max_rate {
                roll_rate = max_rate;
            }
        }
        roll = roll_rate * step + roll;
        c.set_angular(u, yaw_rate, pitch_rate, roll_rate);
        ypr(&mut t, heading, new_pitch, roll);
        if rd!(u, mF220) != roll {
            wr!(u, mF220, roll);
        }
        speed = brake * step + speed;
        if stats.max_velocity < speed {
            speed = stats.max_velocity;
        }
        set_velocity(u, speed);
        let r = t.apply_vector(fwd(speed * step));
        new = add(l, r);
        let y = floor(new.y);
        let x = floor(new.x);
        let new_tile = c.h.tile(x, y);
        if new_tile != 0 {
            tile = new_tile;
        } else {
            new.y = l.y;
            new.x = l.x;
        }
        set_position(&mut t, new);
        let above = new.z > 0.0;
        if airborne && !above && !landing {
            c.h.set_animation(u.obj, 5, -1, false);
            if c.h.tile_info(tile).height < 0.0 && class.has_splash_effect != 0 {
                c.h.splash(u.obj, Ge3DPoint { x: new.x, y: new.y, z: 0.0 });
            }
        }
    } else {
        ypr(&mut t, 0.0, -HALF_PI, 0.0);
        new.x = l.x;
        new.y = l.y;
        new.z = l.z - step * 0.5;
        set_position(&mut t, new);
    }
    let info = c.h.tile_info(tile);
    if new.z < info.height && info.terrain_type != TERRAIN_SPACE {
        set_velocity(u, 0.0);
        if info.height >= 0.0 {
            if class.has_death_effect != 0 {
                c.h.death_effect(u.obj);
            }
            if class.has_shell_hole_effect != 0 {
                c.h.impact_decal(u.obj, new);
            }
        }
    }
    c.h.update_tile(u.obj, &t);
}

/// `EEPMAirplane::Step4` (the air animal's too): speed and turn slowed to a stop, moved and turned by what is left.
pub(crate) fn airplane_step4(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    let mut t = c.h.location(u.obj);
    let step = c.t.tick_seconds;
    let mut yaw_rate = c.h.angular_velocity(u.obj).yaw;
    if c.h.docked(u.obj) {
        c.h.follow_carrier(u.obj);
        t = c.h.location(u.obj);
    }
    let mut l = position(&t);
    carrier_scale(c, u, l.z, false);
    let current = heading(&t);
    let angular = c.h.stats(u.obj).angular_acceleration;
    if yaw_rate < 0.0 {
        yaw_rate = angular * step + yaw_rate;
        if yaw_rate > 0.0 {
            yaw_rate = 0.0;
        }
    } else if yaw_rate > 0.0 {
        yaw_rate = yaw_rate - angular * step;
        if yaw_rate < 0.0 {
            yaw_rate = 0.0;
        }
    } else {
        yaw_rate = 0.0;
    }
    c.set_angular(u, yaw_rate, 0.0, 0.0);
    let new_heading = yaw_rate * step + current;
    let mut turn = rotation(new_heading, 0.0, rd!(u, mRoll));
    let speed = if rd!(u, mVelocity) > 0.0 {
        let mut s = rd!(u, mVelocity) - c.h.acceleration(u.obj) * step;
        if s < 0.0 {
            s = 0.0;
        }
        let max = c.h.max_velocity(u.obj);
        if max < s {
            s = max;
        }
        s
    } else {
        0.0
    };
    set_velocity(u, speed);
    if yaw_rate == 0.0 && speed == 0.0 {
        return;
    }
    let r = turn.apply_vector(fwd(speed * step));
    l = add(l, r);
    set_position(&mut turn, l);
    let bank = compute_pitch(c, k, u, 0.0);
    let roll_offset = compute_roll(c, k, u, yaw_rate);
    ypr(&mut turn, new_heading, bank, rd!(u, mRoll) + roll_offset);
    c.h.update_tile(u.obj, &turn);
}
