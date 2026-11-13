//! The unit utilities' combat leaves (R5b.2, thirteenth round): `EEIsInRangeToHit`, `EEIsInContactRange`,
//! `EEIsAttackedBy`, `EEGetLeadPosition`, `EEGetFiringPosition`, `EEIsSeenBy` over the world host ([`WorldHost`]: the reads, store-backed through
//! [`Stored`], and the operations), and `EEUUGetIsUnitClassTargetable` with `EECanTargetClass` over a plain record of
//! the class's fields (their C++ callers hand classes, not units).
//!
//! Each body is the original's statements in their order; the formulas' captures (CBRH, CBRC) are an operation at the
//! place the C++ wrapper wrote them. Determinism: the original's `f32` operations in order, its square roots through
//! `f64`, `UMQuadraticEquation` as ee_ge's; no random draw.

use crate::damage_table::ee_combat_damage_factor;
use crate::decide::{drop_point, EeCombatDropIn, DROP_KIND_GOAL, DROP_KIND_MODEL, DROP_KIND_NONE, DROP_KIND_TILE};
use crate::reads::{
    EE_COMBAT_CLASS_MELEE, EE_COMBAT_CLASS_WALL, EE_COMBAT_H_COMPLEX, EE_COMBAT_H_GOAL_TARGET, EE_COMBAT_H_PLAYER,
    EE_COMBAT_Q_ATTACK_GOAL, EE_COMBAT_Q_BALLISTIC_HIT, EE_COMBAT_Q_CATEGORY, EE_COMBAT_Q_CLASS_FLAGS,
    EE_COMBAT_Q_ENEMY, EE_COMBAT_Q_FIRE_OVER_WALLS, EE_COMBAT_Q_HEAL_RANGE, EE_COMBAT_Q_HIT_DISTANCE_SQUARED,
    EE_COMBAT_Q_NONE, EE_COMBAT_Q_PLAYER_ID, EE_COMBAT_Q_PROJECTILE_CATEGORY, EE_COMBAT_Q_RANGE,
    EE_COMBAT_Q_SPECIAL_ATTACK, EE_COMBAT_Q_TILE_X, EE_COMBAT_Q_TILE_Y, EE_COMBAT_Q_UNIT_TYPE,
};
use crate::reads::{
    EE_COMBAT_Q_BUILT, EE_COMBAT_Q_HIT_POINTS, EE_COMBAT_Q_ID, EE_COMBAT_Q_MAX_HIT_POINTS, EE_COMBAT_Q_SCALE,
};
use crate::screen::{hit_distance as screen_hit_distance, EeCombatHitDistanceIn};
use crate::walks::EeCombatHandle;
use crate::world::{
    WorldHost, EE_COMBAT_Q_GOAL_KIND, EE_COMBAT_Q_LAND_BUILDING, EE_COMBAT_Q_TILE_SIZE, EE_COMBAT_W_LINE_BLOCKED,
    EE_COMBAT_W_OTHER_CONTINENT,
};
use crate::{can_reach, range, EeCombatRangeIn, EeCombatReachIn, CONTACT_MARGIN};
use ee_ge::types::{Ge3DPoint, GeTransformation};
use ee_ge::um::{sqrt, um_quadratic};

// ---- the reads (reads.rs's numbering goes on from damage.rs's 72)

/// the class's `GetInnerCollisionSphere()` (float bits).
pub const EE_COMBAT_Q_INNER_SPHERE: i32 = 80;
/// the class's `mShootsHigh`.
pub const EE_COMBAT_Q_SHOOTS_HIGH: i32 = 81;
/// the class's `IsCrusader()` (the unit store's).
pub const EE_COMBAT_Q_CRUSADER: i32 = 82;
/// the class's `mIsTower`.
pub const EE_COMBAT_Q_TOWER: i32 = 83;
/// a complex unit's `mWaypoints.size()`.
pub const EE_COMBAT_Q_WAYPOINTS: i32 = 84;
/// a complex unit's `GetMaxVelocity()` (float bits).
pub const EE_COMBAT_Q_MAX_VELOCITY: i32 = 85;
/// a complex unit's `mIsCloakable`, `mIsCloaked` (the unit store's).
pub const EE_COMBAT_Q_CLOAKABLE: i32 = 86;
pub const EE_COMBAT_Q_CLOAKED: i32 = 87;

// ---- the operations (world.rs's numbering goes on from damage.rs's 53)

/// a formula's capture record (TEST ONLY): `a[0]` the tag, `a[1]`/`a[2]` the input's address and size, `a[3]`/`a[4]`
/// the output's.
pub const EE_COMBAT_W_CAPTURE: i32 = 60;
/// a complex unit's `GetPathStart(point, index)` from a default point: the point's bits out in `a[0..3]`.
pub const EE_COMBAT_W_PATH_START: i32 = 61;
/// `gWorld.GetMap()->GetTileAtLocation(U2DPoint<long>(a[0], a[1])) != NULL`.
pub const EE_COMBAT_W_TILE_AT: i32 = 62;
/// `EEGetWallSpot(object, other, point)`: the point's bits in and out in `a[0..3]`, the answer.
pub const EE_COMBAT_W_WALL_SPOT: i32 = 63;
/// `EEFindNewTargetNearOld(object, other)`.
pub const EE_COMBAT_W_NEW_TARGET_NEAR_OLD: i32 = 64;
/// `EEGetFiringPosition`'s ring: the sorted ring tiles at `a[0]` and `a[0] + 1` around `other` from `object`'s
/// position, nearest to `object` first, and `EEPickTileNear` of them (the point's bits in and out in `a[1..4]`).
pub const EE_COMBAT_W_FIRING_RING: i32 = 65;
/// `EEGetFootprintTiles(object, tiles)` into the host's list: its size.
pub const EE_COMBAT_W_FOOTPRINT: i32 = 66;
/// of the player `object`: `gWorld.mMap != NULL && mSaveVisMap` (the seen test's start; an operation: a player is no
/// unit the store's reads may take).
pub const EE_COMBAT_W_SEES_MAP: i32 = 69;
/// the host's footprint tile `a[0]`: x, y out in `a[0]`, `a[1]`.
pub const EE_COMBAT_W_FOOTPRINT_TILE: i32 = 67;
/// whether the player `object` sees tile (`a[0]`, `a[1]`) (`GetTileAtLocation` when `a[3]`, else `GetTile`): a NULL
/// tile never; `IsSeenBy(player)` when `a[2]` (explored), else `GetVisibility(id) > 1`.
pub const EE_COMBAT_W_TILE_SEEN: i32 = 68;

/// The tags of the captures made here.
const TAG_RANGE: u32 = u32::from_le_bytes(*b"CBRH");
const TAG_REACH: u32 = u32::from_le_bytes(*b"CBRC");

/// The categories and unit types the bodies compare (ee/EEUnitClassIDs.h).
const CATEGORY_PROJECTILE: i32 = 9;
const CATEGORY_BOMB: i32 = 0xb;
const CATEGORY_MISSILE: i32 = 0xc;
const CATEGORY_TORPEDO: i32 = 0x14;
const CATEGORY_GATE: u32 = 0x19;
const TYPE_ROMAN_LEGIONNAIRE: u32 = 0x2dc;
const TYPE_BALLOON_OBSERVATION: u32 = 0x5a;
const TYPE_HURRICANE: u32 = 0xb6;
const TYPE_WAYPOINT: u32 = 0x104;
const TYPE_TANK_OBSTACLE: u32 = 0x148;
const TYPE_VOLCANO: u32 = 0x16e;
const TYPE_X11_PROPHET: u32 = 0x1db;
const TYPE_FLARE2: u32 = 0x1dd;
const TYPE_BALLOON_HOT_AIR: u32 = 0x1ff;
const TYPE_ANTI_MATTER_STORM: u32 = 0x294;
/// the anti-air ammunition (`mAmmoType` 0x3c): its targets by the damage table
const AMMO_ANTI_AIR: i32 = 0x3c;

fn real<H: WorldHost>(h: &mut H, code: i32, object: EeCombatHandle) -> f32 {
    f32::from_bits(h.read(code, object, 0))
}

fn flag<H: WorldHost>(h: &mut H, code: i32, object: EeCombatHandle) -> bool {
    h.read(code, object, 0) != 0
}

/// The capture record of a formula (the input's and the output's bytes), as `EECombatCapture` wrote it.
fn capture<H: WorldHost, I, O>(h: &mut H, tag: u32, input: &I, output: &O) {
    let mut a = [0i64; 12];
    a[0] = i64::from(tag);
    a[1] = input as *const I as i64;
    a[2] = std::mem::size_of::<I>() as i64;
    a[3] = output as *const O as i64;
    a[4] = std::mem::size_of::<O>() as i64;
    h.op(EE_COMBAT_W_CAPTURE, 0, 0, &mut a);
}

/// `EEGetHitDistanceSquared(unit, target, -10000, -10000)` (the off-map point: from the unit's own position): the
/// host's (the call's unit is the attacker).
fn hit_distance<H: WorldHost>(h: &mut H, target: EeCombatHandle) -> f32 {
    real(h, EE_COMBAT_Q_HIT_DISTANCE_SQUARED, target)
}

/// `EEIsInRangeToHit(unit, target)`: within range (a crusader's heal range; half a land building's inner sphere
/// off), on the same continent, and (unless the unit shoots high or over walls) a clear line.
pub fn in_range_to_hit<H: WorldHost>(h: &mut H, unit: EeCombatHandle, target: EeCombatHandle) -> bool {
    let mut input = EeCombatRangeIn { range: real(h, EE_COMBAT_Q_RANGE, unit), ..Default::default() };
    input.crusader = i32::from(flag(h, EE_COMBAT_Q_CRUSADER, unit));
    if input.crusader != 0 {
        input.heal_range = real(h, EE_COMBAT_Q_HEAL_RANGE, unit);
    }
    input.distance_squared = hit_distance(h, target);
    input.building = i32::from(flag(h, EE_COMBAT_Q_LAND_BUILDING, target));
    if input.building != 0 {
        input.inner_sphere = real(h, EE_COMBAT_Q_INNER_SPHERE, target);
    }
    let out = range(&input);
    capture(h, TAG_RANGE, &input, &out);
    if out == 0 {
        return false;
    }
    let mut result = h.op(EE_COMBAT_W_OTHER_CONTINENT, unit, target, &mut [0; 12]) == 0;
    if result && !flag(h, EE_COMBAT_Q_SHOOTS_HIGH, unit) && !flag(h, EE_COMBAT_Q_FIRE_OVER_WALLS, unit) {
        result = h.op(EE_COMBAT_W_LINE_BLOCKED, unit, target, &mut [0; 12]) == 0;
    }
    result
}

/// `EEIsInContactRange(unit, target, distance_squared)`: the two inner spheres (a wall's 1) reach across the
/// distance (the hit distance when it is negative) with the contact margin.
pub fn in_contact_range<H: WorldHost>(h: &mut H, unit: EeCombatHandle, target: EeCombatHandle, distance: f32) -> bool {
    let mut d = distance;
    let mut radius = real(h, EE_COMBAT_Q_INNER_SPHERE, target);
    if distance < 0.0 {
        d = hit_distance(h, target);
    }
    if h.read(EE_COMBAT_Q_CLASS_FLAGS, target, 0) & EE_COMBAT_CLASS_WALL != 0 {
        radius = 1.0;
    }
    let input = EeCombatReachIn {
        distance_squared: d,
        radius: real(h, EE_COMBAT_Q_INNER_SPHERE, unit),
        other_radius: radius,
        scale: CONTACT_MARGIN,
        flag_a: 0,
        flag_b: 0,
    };
    let out = can_reach(&input);
    capture(h, TAG_REACH, &input, &out);
    out != 0
}

/// `EEIsAttackedBy(unit, target)`: the target's complex unit attacks the unit (never for a tower).
pub fn is_attacked_by<H: WorldHost>(h: &mut H, unit: EeCombatHandle, target: EeCombatHandle) -> bool {
    if flag(h, EE_COMBAT_Q_TOWER, unit) {
        return false;
    }
    let complex = h.handle(EE_COMBAT_H_COMPLEX, target, 0);
    if complex == 0 || h.read(EE_COMBAT_Q_GOAL_KIND, complex, 0) as i32 == -1 {
        return false;
    }
    flag(h, EE_COMBAT_Q_ATTACK_GOAL, complex) && h.handle(EE_COMBAT_H_GOAL_TARGET, complex, 0) == unit
}

fn translation(t: &GeTransformation) -> Ge3DPoint {
    Ge3DPoint { x: t.x(), y: t.y(), z: t.z() }
}

/// `EEGetLeadPosition(attacker, target, out)`: where the attacker aims at a moving target (more than one waypoint):
/// the intercept point along the target's heading (the quadratic of the two speeds) when it lies on the map, else the
/// target's location. `out` is the caller's point (read by the map test when no root is found, as the original's).
pub fn lead_position<H: WorldHost>(h: &mut H, attacker: EeCombatHandle, target: EeCombatHandle, out: &mut Ge3DPoint) {
    let from = translation(&h.location(attacker));
    let complex = h.handle(EE_COMBAT_H_COMPLEX, target, 0);
    if complex != 0 && attacker != 0 && h.read(EE_COMBAT_Q_WAYPOINTS, complex, 0) as i32 > 1 {
        let position = h.position(complex);
        let distance = from.distance_to(position);
        let speed = real(h, EE_COMBAT_Q_MAX_VELOCITY, attacker);
        let target_speed = real(h, EE_COMBAT_Q_MAX_VELOCITY, complex);
        let mut a = [0i64; 12];
        h.op(EE_COMBAT_W_PATH_START, complex, 0, &mut a);
        let next = Ge3DPoint {
            x: f32::from_bits(a[0] as u32),
            y: f32::from_bits(a[1] as u32),
            z: f32::from_bits(a[2] as u32),
        };
        let (move_x, move_y, move_z) = (next.x - position.x, next.y - position.y, next.z - position.z);
        let (to_x, to_y, to_z) = (from.x - position.x, from.y - position.y, from.z - position.z);
        let to_length = sqrt(to_y * to_y + to_x * to_x + to_z * to_z);
        let dot = move_y * to_y + move_z * to_z + move_x * to_x;
        let cosine = dot / (sqrt(move_z * move_z + move_y * move_y + move_x * move_x) * to_length);
        let qa = if speed != target_speed { speed * speed - target_speed * target_speed } else { 0.0 };
        let mut result = false;
        if let Some((first, second)) = um_quadratic(qa, cosine * target_speed * distance * 2.0, -(distance * distance))
        {
            result = true;
            if first >= 0.0 {
                *out = h.location(complex).apply_point(Ge3DPoint { x: 0.0, y: first * target_speed, z: 0.0 });
            } else if second >= 0.0 {
                *out = h.location(complex).apply_point(Ge3DPoint { x: 0.0, y: second * target_speed, z: 0.0 });
            } else {
                result = false;
            }
        }
        // `U2DPoint<long>((long)x, (long)y)`: truncation
        let mut t = [0i64; 12];
        t[0] = i64::from(out.x as i32);
        t[1] = i64::from(out.y as i32);
        if h.op(EE_COMBAT_W_TILE_AT, 0, 0, &mut t) == 0 {
            result = false;
        }
        if result {
            return;
        }
    }
    *out = translation(&h.location(complex));
}

fn point_args(a: &mut [i64; 12], at: usize, p: &Ge3DPoint) {
    a[at] = i64::from(p.x.to_bits());
    a[at + 1] = i64::from(p.y.to_bits());
    a[at + 2] = i64::from(p.z.to_bits());
}

fn args_point(a: &[i64; 12], at: usize) -> Ge3DPoint {
    Ge3DPoint {
        x: f32::from_bits(a[at] as u32),
        y: f32::from_bits(a[at + 1] as u32),
        z: f32::from_bits(a[at + 2] as u32),
    }
}

/// `EEGetFiringPosition(attacker, target, out)`: where the attacker stands to fire: where it is when it already
/// reaches (contact for melee, the ballistic reach for a projectile or bomb launcher, else the range to hit), a melee
/// unit's wall spot (else it looks for a new target near the old one), else a tile of the ring at half its range
/// around the target, else the target's position (an unplaced target's tile centre). `out` is the caller's point:
/// the paths that set only part of it (the tile centre, a failed wall spot) leave the rest as it was.
pub fn firing_position<H: WorldHost>(h: &mut H, attacker: EeCombatHandle, target: EeCombatHandle, out: &mut Ge3DPoint) {
    let target_unit = h.handle(EE_COMBAT_H_COMPLEX, target, 0);
    let healer = h.read(EE_COMBAT_Q_UNIT_TYPE, attacker, 0) == TYPE_ROMAN_LEGIONNAIRE
        && flag(h, EE_COMBAT_Q_SPECIAL_ATTACK, attacker);
    if h.read(EE_COMBAT_Q_CLASS_FLAGS, attacker, 0) & EE_COMBAT_CLASS_MELEE != 0 && !healer {
        if in_contact_range(h, attacker, target, -1.0) {
            *out = translation(&h.location(attacker));
        } else {
            let mut a = [0i64; 12];
            point_args(&mut a, 0, out);
            let found = h.op(EE_COMBAT_W_WALL_SPOT, attacker, target, &mut a) != 0;
            *out = args_point(&a, 0);
            if !found {
                h.op(EE_COMBAT_W_NEW_TARGET_NEAR_OLD, attacker, target_unit, &mut [0; 12]);
            }
        }
        return;
    }
    let projectile = h.read(EE_COMBAT_Q_PROJECTILE_CATEGORY, attacker, 0);
    if projectile != EE_COMBAT_Q_NONE
        && (projectile as i32 == CATEGORY_PROJECTILE || projectile as i32 == CATEGORY_BOMB)
    {
        let reach =
            if healer { real(h, EE_COMBAT_Q_HEAL_RANGE, attacker) } else { real(h, EE_COMBAT_Q_RANGE, attacker) };
        let radius = real(h, EE_COMBAT_Q_INNER_SPHERE, attacker);
        if hit_distance(h, target) <= (radius + reach) * (real(h, EE_COMBAT_Q_INNER_SPHERE, attacker) + reach)
            && flag(h, EE_COMBAT_Q_BALLISTIC_HIT, target)
        {
            *out = translation(&h.location(attacker));
            return;
        }
    } else if in_range_to_hit(h, attacker, target) {
        *out = translation(&h.location(attacker));
        return;
    }
    // `(long)mStats->mRange - mTileSize`, at least 0, halved
    let mut range =
        (real(h, EE_COMBAT_Q_RANGE, attacker) as i32).wrapping_sub(h.read(EE_COMBAT_Q_TILE_SIZE, target, 0) as i32);
    if range < 0 {
        range = 0;
    }
    range /= 2;
    let mut a = [0i64; 12];
    a[0] = i64::from(range);
    point_args(&mut a, 1, out);
    let found = h.op(EE_COMBAT_W_FIRING_RING, attacker, target, &mut a) != 0;
    *out = args_point(&a, 1);
    if found {
        return;
    }
    if target_unit != 0 {
        *out = h.position(target_unit);
    } else {
        out.x = h.read(EE_COMBAT_Q_TILE_X, target, 0) as i32 as f32 + 0.5;
        out.y = h.read(EE_COMBAT_Q_TILE_Y, target, 0) as i32 as f32 + 0.5;
    }
}

/// `EEGetFiringPosition` (`firing` 1) or `EEGetLeadPosition` (0) of `attacker` on `target` over `host` (the point in
/// and out).
///
/// # Safety
/// `host` must point to a valid table whose callbacks may be called with its context, `point` to three floats.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_attack_position(
    firing: i32,
    attacker: u64,
    target: u64,
    host: *const crate::world::EeCombatWorldHost,
    point: *mut [f32; 3],
) {
    let mut world = unsafe { *host };
    let h = &mut crate::reads::Stored::new(&mut world);
    // SAFETY: the function's contract
    let p = unsafe { &mut *point };
    let mut out = Ge3DPoint { x: p[0], y: p[1], z: p[2] };
    if firing != 0 {
        firing_position(h, attacker, target, &mut out);
    } else {
        lead_position(h, attacker, target, &mut out);
    }
    *p = [out.x, out.y, out.z];
}

fn tile_seen<H: WorldHost>(
    h: &mut H,
    player: EeCombatHandle,
    x: i64,
    y: i64,
    explored: bool,
    at_location: bool,
) -> bool {
    let mut a = [0i64; 12];
    a[0] = x;
    a[1] = y;
    a[2] = i64::from(explored);
    a[3] = i64::from(at_location);
    h.op(EE_COMBAT_W_TILE_SEEN, player, 0, &mut a) != 0
}

/// `EEIsSeenBy(unit, player, explored)`: true without a map or for a player that keeps no visibility; never a cloaked
/// enemy; else any tile of a large unit's or a gate's footprint, or the unit's
/// tile, seen (`explored`: explored is enough) by the player.
pub fn is_seen_by<H: WorldHost>(h: &mut H, unit: EeCombatHandle, player: EeCombatHandle, explored: bool) -> bool {
    let mut result = true;
    if h.op(EE_COMBAT_W_SEES_MAP, player, 0, &mut [0; 12]) == 0 {
        return result;
    }
    if unit != 0 {
        let complex = h.handle(EE_COMBAT_H_COMPLEX, unit, 0);
        if complex == 0
            || (flag(h, EE_COMBAT_Q_CLOAKABLE, complex) && flag(h, EE_COMBAT_Q_CLOAKED, complex) && {
                let owner = h.handle(EE_COMBAT_H_PLAYER, unit, 0);
                owner != player && {
                    let id = h.read(EE_COMBAT_Q_PLAYER_ID, owner, 0);
                    h.read(EE_COMBAT_Q_ENEMY, player, id as i32) != 0
                }
            })
        {
            result = complex == 0;
        }
    }
    if !result {
        return result;
    }
    if h.read(EE_COMBAT_Q_TILE_SIZE, unit, 0) as i32 > 1 || h.read(EE_COMBAT_Q_CATEGORY, unit, 0) == CATEGORY_GATE {
        let count = h.op(EE_COMBAT_W_FOOTPRINT, unit, 0, &mut [0; 12]);
        let mut found = false;
        for i in 0..count {
            if found {
                break;
            }
            let mut a = [0i64; 12];
            a[0] = i as i64;
            h.op(EE_COMBAT_W_FOOTPRINT_TILE, unit, 0, &mut a);
            found = tile_seen(h, player, a[0], a[1], explored, false);
        }
        return found;
    }
    let (x, y) = (h.read(EE_COMBAT_Q_TILE_X, unit, 0) as i32, h.read(EE_COMBAT_Q_TILE_Y, unit, 0) as i32);
    tile_seen(h, player, i64::from(x), i64::from(y), explored, true)
}

/// `EEIsSeenBy(unit, player, explored)` over `host`.
///
/// # Safety
/// `host` must point to a valid table whose callbacks may be called with its context.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_is_seen_by(
    unit: u64,
    player: u64,
    explored: i32,
    host: *const crate::world::EeCombatWorldHost,
) -> bool {
    let mut world = unsafe { *host };
    let h = &mut crate::reads::Stored::new(&mut world);
    is_seen_by(h, unit, player, explored != 0)
}

// ---- the drop location and the hit distance (R5b.2, fifteenth round)

/// a complex unit's `mActiveGoal->mKind` ([`EE_COMBAT_Q_NONE`] without one).
pub const EE_COMBAT_Q_ACTIVE_GOAL_KIND: i32 = 88;
/// a complex unit's active goal's attack target (`static_cast<EEUGRust*>(mActiveGoal)->GetAttackTarget()`).
pub const EE_COMBAT_H_ACTIVE_GOAL_TARGET: i32 = 10;
/// `object->mActiveGoal->GetGoalLocation(point)`: the point's bits out in `a[0..3]`.
pub const EE_COMBAT_W_GOAL_LOCATION: i32 = 70;
/// `object->GetModel()->mModel != NULL`; the model's `mDefaultPoint` bits out in `a[0..3]`.
pub const EE_COMBAT_W_DROP_MODEL: i32 = 71;
/// `object->mTile != NULL`; the tile's `GetHeight()` bits out in `a[0]`.
pub const EE_COMBAT_W_TILE_HEIGHT: i32 = 72;
/// `object->GetPosition(point)` (the virtual: a simple unit's tile centre): the point's bits out in `a[0..3]`.
pub const EE_COMBAT_W_POSITION: i32 = 73;

const TAG_DROP: u32 = u32::from_le_bytes(*b"CBDP");
const TAG_HIT_DISTANCE: u32 = u32::from_le_bytes(*b"CBHD");
/// `kGoalAttackUnit`
const GOAL_ATTACK_UNIT: u32 = 2;

/// CBDP's output: the point, then the target's id (-1: none).
#[repr(C)]
struct DropRecord {
    x: f32,
    y: f32,
    z: f32,
    target: i32,
}

/// `EEFindDropLocation(unit, &target, &point, target_in)`: where a unit's drop or shot lands on `target_in` (its
/// active attack goal's target when 0; without either, its active goal's location): the target model's drop point
/// (lower with an unbuilt unit's health), else the target's tile centre at its height. The target out is 0 when it
/// has neither (the point then 0, 0, 0).
pub fn find_drop_location<H: WorldHost>(
    h: &mut H,
    unit: EeCombatHandle,
    target_in: EeCombatHandle,
) -> (EeCombatHandle, Ge3DPoint) {
    let mut input = EeCombatDropIn::default();
    let mut location = Ge3DPoint::default();
    let mut target = target_in;
    if target == 0 && h.read(EE_COMBAT_Q_ACTIVE_GOAL_KIND, unit, 0) != GOAL_ATTACK_UNIT {
        input.kind = DROP_KIND_GOAL;
        let mut a = [0i64; 12];
        h.op(EE_COMBAT_W_GOAL_LOCATION, unit, 0, &mut a);
        location = args_point(&a, 0);
    } else {
        if target == 0 {
            target = h.handle(EE_COMBAT_H_ACTIVE_GOAL_TARGET, unit, 0);
        }
        let complex = h.handle(EE_COMBAT_H_COMPLEX, target, 0);
        let mut model = [0i64; 12];
        let mut tile = [0i64; 12];
        if complex != 0 && h.op(EE_COMBAT_W_DROP_MODEL, target, 0, &mut model) != 0 {
            input.kind = DROP_KIND_MODEL;
            let offset = args_point(&model, 0);
            input.offset = [offset.x, offset.y, offset.z];
            input.scale = real(h, EE_COMBAT_Q_SCALE, complex);
            input.built = i32::from(flag(h, EE_COMBAT_Q_BUILT, complex));
            input.hit_points = h.read(EE_COMBAT_Q_HIT_POINTS, target, 0) as i32;
            input.max_hit_points = h.read(EE_COMBAT_Q_MAX_HIT_POINTS, target, 0) as i32;
            input.transformation = h.location(complex).m;
        } else if h.op(EE_COMBAT_W_TILE_HEIGHT, target, 0, &mut tile) != 0 {
            input.kind = DROP_KIND_TILE;
            input.tile_x = h.read(EE_COMBAT_Q_TILE_X, target, 0) as i32;
            input.tile_y = h.read(EE_COMBAT_Q_TILE_Y, target, 0) as i32;
            input.tile_height = f32::from_bits(tile[0] as u32);
        } else {
            input.kind = DROP_KIND_NONE;
            target = 0;
        }
        if input.kind != DROP_KIND_NONE {
            let p = drop_point(&input);
            location = Ge3DPoint { x: p.x, y: p.y, z: p.z };
        }
    }
    let id = if target != 0 { h.read(EE_COMBAT_Q_ID, target, 0) as i32 } else { -1 };
    capture(h, TAG_DROP, &input, &DropRecord { x: location.x, y: location.y, z: location.z, target: id });
    (target, location)
}

/// `EEGetHitDistanceSquared(unit, target, x, y)`: the ground distance squared from the unit's position (or (x, y)
/// when x is not -10000) to where it hits the target: its drop location, a melee unit's the target's position.
pub fn hit_distance_squared<H: WorldHost>(
    h: &mut H,
    unit: EeCombatHandle,
    target: EeCombatHandle,
    x: f32,
    y: f32,
) -> f32 {
    let from = translation(&h.location(unit));
    let to = if h.read(EE_COMBAT_Q_CLASS_FLAGS, unit, 0) & EE_COMBAT_CLASS_MELEE == 0 {
        find_drop_location(h, unit, target).1
    } else {
        let mut a = [0i64; 12];
        h.op(EE_COMBAT_W_POSITION, target, 0, &mut a);
        args_point(&a, 0)
    };
    let input = EeCombatHitDistanceIn { from: [from.x, from.y, from.z], to: [to.x, to.y, to.z], x, y };
    let distance = screen_hit_distance(&input);
    capture(h, TAG_HIT_DISTANCE, &input, &distance);
    distance
}

/// `EEFindDropLocation` of `unit` on `target` (0: its goal's) over `host`: the target out, the point in `point`.
///
/// # Safety
/// `host` must point to a valid table whose callbacks may be called with its context, `point` to three floats.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_find_drop_location(
    unit: u64,
    target: u64,
    host: *const crate::world::EeCombatWorldHost,
    point: *mut [f32; 3],
) -> u64 {
    let mut world = unsafe { *host };
    let h = &mut crate::reads::Stored::new(&mut world);
    let (target, location) = find_drop_location(h, unit, target);
    // SAFETY: the function's contract
    unsafe { *point = [location.x, location.y, location.z] };
    target
}

/// `EEGetHitDistanceSquared(unit, target, x, y)` over `host`.
///
/// # Safety
/// `host` must point to a valid table whose callbacks may be called with its context.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_hit_distance_squared(
    unit: u64,
    target: u64,
    x: f32,
    y: f32,
    host: *const crate::world::EeCombatWorldHost,
) -> f32 {
    let mut world = unsafe { *host };
    let h = &mut crate::reads::Stored::new(&mut world);
    hit_distance_squared(h, unit, target, x, y)
}

/// A class's fields the targetable tests read (`EEUUGetIsUnitClassTargetable`, `EECanTargetClass`), filled by the
/// C++ from the attacker and the class.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeCombatTargetableIn {
    /// an attacker is given; its class's `mAmmoType`, `IsLandBuilding()`, its player's `mComputerPlayer != NULL`
    pub attacker: i32,
    pub ammo_type: i32,
    pub land_building: i32,
    pub computer: i32,
    /// the class's `mCategory`, `mUnitType`, `mFamily`, `CanBeAttacked()`
    pub category: i32,
    pub unit_type: u32,
    pub family: u32,
    pub can_be_attacked: i32,
}

/// `EEUUGetIsUnitClassTargetable(attacker, class)` of a class: anti-air ammunition only what it can damage, any other
/// no shot; never the storms, waypoints, tank obstacles, volcanoes, flares.
pub fn targetable(i: &EeCombatTargetableIn) -> bool {
    if i.attacker != 0 && i.ammo_type == AMMO_ANTI_AIR {
        if i.can_be_attacked == 0 || ee_combat_damage_factor(i.family as i32, AMMO_ANTI_AIR) <= 0.0 {
            return false;
        }
    } else if matches!(i.category, CATEGORY_PROJECTILE | CATEGORY_BOMB | CATEGORY_MISSILE | CATEGORY_TORPEDO) {
        return false;
    }
    let t = i.unit_type;
    if t == TYPE_HURRICANE || t == TYPE_WAYPOINT || t == TYPE_TANK_OBSTACLE || t == TYPE_VOLCANO {
        return false;
    }
    !(t > TYPE_X11_PROPHET && (t <= TYPE_FLARE2 || t == TYPE_ANTI_MATTER_STORM))
}

/// `EECanTargetClass(unit, class)`: targetable, and the balloons only by buildings or computer players.
pub fn can_target_class(i: &EeCombatTargetableIn) -> bool {
    let result = targetable(i);
    if !result || i.attacker == 0 || (i.unit_type != TYPE_BALLOON_OBSERVATION && i.unit_type != TYPE_BALLOON_HOT_AIR) {
        return result;
    }
    i.land_building != 0 || i.computer != 0
}

/// `EEUUGetIsUnitClassTargetable` (`can_target` 0) or `EECanTargetClass` (1) of the record.
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_targetable(input: *const EeCombatTargetableIn, can_target: i32) -> bool {
    // SAFETY: the function's contract
    let i = unsafe { &*input };
    if can_target != 0 {
        can_target_class(i)
    } else {
        targetable(i)
    }
}

#[cfg(test)]
#[path = "leaves_tests.rs"]
mod tests;
