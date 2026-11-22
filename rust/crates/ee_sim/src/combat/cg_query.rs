//! The combat queries' C++ entry points (EECombatGoals.cpp's `EECombatWorld`, `EECombatComputeDamage`,
//! `EECombatAttackPosition`, `EECombatFindDropLocation`, `EECombatHitDistanceSquared`, `EECombatIsSeenBy`,
//! `EECombatFindImpactUnit`, `EECombatIsInReach`, `EECombatIsStillReachable`, `EECombatIsGateClosed`,
//! `EECombatHealInRange`, `EECombatIsEnemyWithinRange`): each built the call's query context and host and called
//! ee_combat's function over it. The context and host are now made by one C++ trampoline (`ee_units_cg_query`: the
//! unit, goal and class of the call, the host handed to a Rust body); the tile, point and amount marshalling and
//! the host-read captures (CBQR) of IsStillReachable and IsGateClosed are here. The host-read scope's depth (the
//! C++ `EECombatHostReadScope`, a thread's nesting of the captured host reads) is Rust's too.

use crate::combat::damage::EeCombatDamageCall;
use core::ffi::c_long;
use std::cell::Cell;

/// `EECombatHostReadScope`'s slots
const SLOTS: usize = 4;

thread_local! {
    static HOST_READ_DEPTH: [Cell<i32>; SLOTS] = const { [Cell::new(0), Cell::new(0), Cell::new(0), Cell::new(0)] };
}

/// `EECombatHostReadScope(slot)`: one more nesting of the slot; whether it is the outermost (`depth++ == 0`).
#[no_mangle]
pub extern "C" fn ee_combat_host_read_enter(slot: i32) -> bool {
    HOST_READ_DEPTH.with(|d| {
        let c = &d[slot as usize];
        let v = c.get();
        c.set(v + 1);
        v == 0
    })
}

/// `~EECombatHostReadScope`: `depth--`.
#[no_mangle]
pub extern "C" fn ee_combat_host_read_leave(slot: i32) {
    HOST_READ_DEPTH.with(|d| {
        let c = &d[slot as usize];
        c.set(c.get() - 1);
    });
}

/// A host-read scope over a closure (its outermost flag handed in).
fn scoped<R>(slot: i32, f: impl FnOnce(bool) -> R) -> R {
    let outer = ee_combat_host_read_enter(slot);
    let r = f(outer);
    ee_combat_host_read_leave(slot);
    r
}

// ---- the TEST ONLY capture records' function codes (ee/EECombatGoals.h's enums before; the C++ uses these)

/// CBQR: one call of a read behind the hosts, its outcome: the function, the unit's and target's ids (0 without
/// one), the result and what the call left behind. A read made inside another captured read records nothing.
/// FindImpactUnit: extra 0 the unit hit's id, 1 the owner's mIsFiringHigh
pub const EE_COMBAT_HR_IMPACT_UNIT: u32 = 1;
/// IsStillReachable: extra 0 mPathAttempts, 1 the ignored targets' count
pub const EE_COMBAT_HR_STILL_REACHABLE: u32 = 2;
/// IsGateClosed
pub const EE_COMBAT_HR_GATE_CLOSED: u32 = 3;
/// EEIsEnemyWithinRange: extra 0 mTargetSearchTimer
pub const EE_COMBAT_HR_ENEMY_IN_RANGE: u32 = 4;
/// IsInReach: extra 0 mWeaponTagPoint before, 1 after
pub const EE_COMBAT_HR_IN_REACH: u32 = 5;
/// FacesPoint: extra 0, 1 the point's x, y bits, 2 inUseTolerance
pub const EE_COMBAT_HR_FACES_POINT: u32 = 6;
/// FacesTarget: extra 2 inUseTolerance
pub const EE_COMBAT_HR_FACES_TARGET: u32 = 7;

/// CBSO: the outcome of a shot's ActionChanged or Update or of the heal goal's Work (only the outermost call of
/// each kind, scope slots 1 to 3)
pub const EE_COMBAT_SO_PROJECTILE_ACTION: u32 = 1;
pub const EE_COMBAT_SO_MISSILE_ACTION: u32 = 2;
pub const EE_COMBAT_SO_PROJECTILE_UPDATE: u32 = 3;
pub const EE_COMBAT_SO_MISSILE_UPDATE: u32 = 4;
pub const EE_COMBAT_SO_TORPEDO_UPDATE: u32 = 5;
pub const EE_COMBAT_SO_HEAL_WORK: u32 = 6;

/// CBXR: one call of the eighth round's reads (every call records). IsTargetIgnored: extra 0 inCheckLocation, 1
/// the list's size after, 2 its hash
pub const EE_COMBAT_XR_IS_IGNORED: u32 = 1;
/// IgnoreTarget: extra 1 the list's size after, 2 its hash
pub const EE_COMBAT_XR_IGNORE: u32 = 2;
/// CanReachUnit
pub const EE_COMBAT_XR_CAN_REACH_UNIT: u32 = 3;
/// EEIsAttackReachable: extra 0, 1 the tile's x, y after
pub const EE_COMBAT_XR_ATTACK_REACHABLE: u32 = 4;
/// IsOnTileBorderingTarget
pub const EE_COMBAT_XR_BORDERING: u32 = 5;

/// CBXA: the outcome of FireShot, Attacked, ApplyHit and ApplyAreaDamage at each call's end
pub const EE_COMBAT_XA_FIRE_SHOT: u32 = 1;
/// inBefore: the last attacker's id before
pub const EE_COMBAT_XA_ATTACKED: u32 = 2;
pub const EE_COMBAT_XA_APPLY_HIT: u32 = 3;
/// inBefore: the victims' FNV-1a 32 (id, hit points, resource amount)
pub const EE_COMBAT_XA_AREA_DAMAGE: u32 = 4;

/// CBGO: the attack, attack-ground and heal goals' GetDestination (extra 0, 1: the attack goal's last target tile
/// after) and Suits (attack ground: the target point's bits)
pub const EE_COMBAT_GO_DESTINATION: u32 = 1;
pub const EE_COMBAT_GO_SUITS: u32 = 2;

/// `EECombatCaptureHostRead` (`which` 0, three extras) and `EECombatCaptureXRead` (1, two) with the extras as
/// arguments.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub extern "C" fn ee_combat_capture_read_of(
    which: u32,
    func: u32,
    unit: u64,
    target: u64,
    result: i32,
    e0: u32,
    e1: u32,
    e2: u32,
) {
    #[cfg(not(test))]
    crate::combat::capture_units::ee_combat_capture_read(which, func, unit, target, result, &[e0, e1, e2]);
    #[cfg(test)]
    let _ = (which, func, unit, target, result, e0, e1, e2);
}

/// The world the queries ask: ee_combat's functions over the call's C++ host (the trampoline), the reads the
/// captures take and the captures.
pub trait Queries {
    fn world(&mut self, function: i32, unit: u64, target: u64, class: u64, tile: &mut [i32; 2]) -> i32;
    fn compute_damage(&mut self, unit: u64, target: u64, class: u64, call: &mut EeCombatDamageCall) -> bool;
    fn attack_position(&mut self, firing: bool, attacker: u64, target: u64, point: &mut [f32; 3]);
    fn find_drop_location(&mut self, unit: u64, target: u64, point: &mut [f32; 3]) -> u64;
    fn hit_distance_squared(&mut self, unit: u64, target: u64, x: f32, y: f32) -> f32;
    fn is_seen_by(&mut self, unit: u64, player: u64, explored: bool) -> bool;
    fn impact_unit(&mut self, unit: u64) -> u64;
    fn in_reach(&mut self, unit: u64, target: u64) -> bool;
    /// `ee_combat_still_reachable` over a host of `unit` and `goal`; then the unit's `mIgnoredTargets.size()`
    fn still_reachable(&mut self, unit: u64, goal: u64) -> (bool, u32);
    fn gate_closed(&mut self, unit: u64, goal: u64) -> bool;
    fn heal_in_range(&mut self, healer: u64, target: u64) -> bool;
    fn enemy_in_range(&mut self, unit: u64) -> bool;
    /// the attack goal's `GetAttackTarget()`
    fn attack_target(&mut self, goal: u64) -> u64;
    /// the goal's `mPathAttempts`
    fn path_attempts(&mut self, goal: u64) -> u32;
    /// the unit's `mTargetSearchTimer`
    fn search_timer(&mut self, unit: u64) -> u32;
    /// `EECombatCaptureHostRead(func, unit, target, result, extra0, extra1)` (`which` 0) or `EECombatCaptureXRead`
    /// (1)
    fn capture_read(&mut self, which: u32, func: u32, unit: u64, target: u64, result: bool, extra: [u32; 2]);
    /// a unit's `mHitPoints`
    fn hit_points(&mut self, unit: u64) -> i32;
    /// `candidate->DistanceSquaredTo(target)` (two complex units)
    fn distance_squared_to(&mut self, candidate: u64, target: u64) -> f32;
    /// `EECombatCapture(tag, in, out)`: the record of the bytes when the capture runs
    fn capture(&mut self, tag: u32, bytes: &[u8]);
    /// the unit's class's facts the reach tests read
    fn class_facts(&mut self, unit: u64) -> ClassFacts;
    /// whether the capture runs (`EESimCaptureRecord::Active()`)
    fn capture_active(&mut self) -> bool;
    /// the unit's `mIgnoredTargets` (each entry's tile, from tile, unit id, time)
    fn ignored_targets(&mut self, unit: u64) -> Vec<crate::combat::world::Ignored>;
    /// a unit's `mUnitID` (0 for none)
    fn unit_id(&mut self, unit: u64) -> u32;
    /// `ee_combat_capture_fold(which, bytes)`
    fn fold(&mut self, which: u32, bytes: &[u8]);
    /// the unit's `mGoalState` and its `mKind` (0, 0 for none)
    fn goal_state(&mut self, unit: u64) -> (u64, u8);
}

/// `EECombatCaptureIgnored(func, unit, target, result, checkLocation)`: the X-reads fold of the call (CBXR) with
/// the ignored targets' count and FNV-1a 32 hash, when the capture runs.
pub fn capture_ignored<Q: Queries + ?Sized>(q: &mut Q, func: u32, unit: u64, target: u64, result: i32, check: u32) {
    if !q.capture_active() {
        return;
    }
    let entries = q.ignored_targets(unit);
    let mut hash = 0x811c_9dc5u32;
    for e in &entries {
        let fields = [e.tile.0 as u32, e.tile.1 as u32, e.from.0 as u32, e.from.1 as u32, e.unit_id, e.time];
        for b in fields.iter().flat_map(|f| f.to_le_bytes()) {
            hash = (hash ^ u32::from(b)).wrapping_mul(0x0100_0193);
        }
    }
    let unit_id = q.unit_id(unit);
    let target_id = if target != 0 { q.unit_id(target) } else { 0 };
    let record = [func, unit_id, target_id, result as u32, check, entries.len() as u32, hash];
    let bytes: Vec<u8> = record.iter().flat_map(|w| w.to_le_bytes()).collect();
    q.fold(crate::combat::capture_fold::EE_COMBAT_FOLD_X_READS, &bytes);
}

/// `kGoalAttackUnit`
const GOAL_ATTACK_UNIT: u8 = 2;

/// `ee_units_cg_host`'s `GetModel()->GetNumParts()` of the unit
pub const EE_UNITS_CG_MODEL_PARTS: i32 = 5000;
/// `mStats->GetScale()` of the unit (`f[0]`)
pub const EE_UNITS_CG_STATS_SCALE: i32 = 5001;

/// What `EECombatFacesPoint` reads of the class, the unit and the C++.
pub trait Facing {
    /// the class's `mFiringArc`, `mFacingOffset`, `mFiresBroadside`
    fn class_arc(&mut self, class: u64) -> (f32, f32, bool);
    /// the unit's location (`mMatrix` rows), its class's unit type, `mIsSpecialAttack`
    fn unit_facts(&mut self, unit: u64) -> ([[f32; 4]; 4], i32, bool);
    fn model_parts(&mut self, unit: u64) -> i32;
    fn stats_scale(&mut self, unit: u64) -> f32;
    fn turret(&mut self, unit: u64, index: i32) -> crate::combat::reads::EeCombatTurret;
}

/// `EECombatFacesPoint(class, unit, location, useTolerance)`: ee_combat's facing test over the class's arc, the
/// unit's location and turrets (the scale read only with turrets).
pub fn faces_point<F: Facing + ?Sized>(f: &mut F, class: u64, unit: u64, x: f32, y: f32, use_tolerance: bool) -> bool {
    let (location, unit_type, special) = f.unit_facts(unit);
    let (firing_arc, facing_offset, broadside) = f.class_arc(class);
    let mut i = crate::combat::reads::EeCombatFacingIn {
        location: [0.0; 16],
        point: [x, y],
        use_tolerance: i32::from(use_tolerance),
        firing_arc,
        facing_offset,
        fires_broadside: i32::from(broadside),
        unit_type,
        special_attack: i32::from(special),
        turrets: 0,
        scale: 0.0,
    };
    for (r, row) in location.iter().enumerate() {
        i.location[r * 4..r * 4 + 4].copy_from_slice(row);
    }
    i.turrets = f.model_parts(unit);
    if i.turrets > 0 {
        i.scale = f.stats_scale(unit);
    }
    crate::combat::reads::faces_point(&i, |index| f.turret(unit, index))
}

/// `EECombatShotTarget(unit)`: the attack goal's target (0 without an attack-unit goal).
pub fn shot_target<Q: Queries + ?Sized>(q: &mut Q, unit: u64) -> u64 {
    match q.goal_state(unit) {
        (goal, GOAL_ATTACK_UNIT) if goal != 0 => q.attack_target(goal),
        _ => 0,
    }
}

/// A unit class's members the reach tests read.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct ClassFacts {
    pub inner_sphere: f32,
    pub is_wall: bool,
    pub is_tree: bool,
    pub unit_type: u32,
    pub category: u32,
}

/// "CBRW", "CBRT"
const CBRW: u32 = u32::from_le_bytes(*b"CBRW");
const CBRT: u32 = u32::from_le_bytes(*b"CBRT");
/// `kUnitTypeRoboticFarm`, `kUnitCategoryNavalCitizen` (ee/EEUnitClassIDs.h)
const UNIT_TYPE_ROBOTIC_FARM: u32 = 0x33a;
const CATEGORY_NAVAL_CITIZEN: u32 = 0x16;

/// The reach record's bytes and the result (`EECombatCapture`'s payload).
fn reach_bytes(i: &crate::combat::EeCombatReachIn, out: i32) -> Vec<u8> {
    let words = [
        i.distance_squared.to_bits(),
        i.radius.to_bits(),
        i.other_radius.to_bits(),
        i.scale.to_bits(),
        i.flag_a as u32,
        i.flag_b as u32,
        out as u32,
    ];
    words.iter().flat_map(|w| w.to_le_bytes()).collect()
}

/// `EEIsInWorkReach(unit, target)`: ee_combat's work reach over the two classes and the units' squared distance, its
/// CBRW record.
pub fn in_work_reach<Q: Queries + ?Sized>(q: &mut Q, unit: u64, target: u64) -> bool {
    let (u, t) = (q.class_facts(unit), q.class_facts(target));
    let i = crate::combat::EeCombatReachIn {
        distance_squared: q.distance_squared_to(unit, target),
        radius: u.inner_sphere,
        other_radius: t.inner_sphere,
        scale: 0.0,
        flag_a: i32::from(t.is_wall),
        flag_b: i32::from(t.unit_type == UNIT_TYPE_ROBOTIC_FARM),
    };
    let out = crate::combat::work_reach(&i);
    q.capture(CBRW, &reach_bytes(&i, out));
    out != 0
}

/// `EEIsInGatherReach(distance, unit, target)`: ee_combat's gather reach, its CBRT record.
pub fn in_gather_reach<Q: Queries + ?Sized>(q: &mut Q, distance_squared: f32, unit: u64, target: u64) -> bool {
    let (u, t) = (q.class_facts(unit), q.class_facts(target));
    let i = crate::combat::EeCombatReachIn {
        distance_squared,
        radius: u.inner_sphere,
        other_radius: t.inner_sphere,
        scale: 0.0,
        flag_a: i32::from(t.is_tree),
        flag_b: i32::from(u.category == CATEGORY_NAVAL_CITIZEN),
    };
    let out = crate::combat::gather_reach(&i);
    q.capture(CBRT, &reach_bytes(&i, out));
    out != 0
}

/// `EEIsInContactRange(unit, target, distance)`: the world's contact range, the distance's bits handed as both of
/// the tile's coordinates (ee_combat leaves.rs in_contact_range).
pub fn in_contact_range<Q: Queries + ?Sized>(q: &mut Q, unit: u64, target: u64, distance_squared: f32) -> bool {
    let bits = distance_squared.to_bits() as i32;
    let mut tile = [bits, bits];
    q.world(crate::combat::world::EE_COMBAT_WORLD_CONTACT_RANGE, unit, target, 0, &mut tile) != 0
}

/// "CBFS", as `EE_COMBAT_TAG` builds it (the first letter in the low byte)
const CBFS: u32 = u32::from_le_bytes(*b"CBFS");

/// `EEFOFindNewTargetNearOldTarget::Virtual04/08` (EENearOldScreen): ee_combat's screen of a living candidate
/// within `range` (squared) of the old target, its CBFS record (the screen record, the result).
pub fn near_old_screen<Q: Queries + ?Sized>(q: &mut Q, candidate: u64, target: u64, range: f32) -> bool {
    let hit_points = q.hit_points(candidate);
    let (accepted, read) =
        crate::combat::screen::screen_accepts(hit_points, range, || q.distance_squared_to(candidate, target));
    let words = [
        crate::combat::screen::EE_COMBAT_SCREEN_KIND_NEAR_OLD as u32,
        hit_points as u32,
        range.to_bits(),
        u32::from(read.is_some()),
        read.unwrap_or(0.0).to_bits(),
        u32::from(accepted),
    ];
    let bytes: Vec<u8> = words.iter().flat_map(|w| w.to_le_bytes()).collect();
    q.capture(CBFS, &bytes);
    accepted
}

/// `EECombatWorld(function, unit, target, class, ioX, ioY)`: the tile in from the pointers (0 for none), out to
/// them when `io_x` is set (both written, as the original).
pub fn world<Q: Queries + ?Sized>(
    q: &mut Q,
    function: i32,
    unit: u64,
    target: u64,
    class: u64,
    io_x: Option<&mut c_long>,
    io_y: Option<&mut c_long>,
) -> i32 {
    let mut tile = [io_x.as_deref().map_or(0, |x| *x as i32), io_y.as_deref().map_or(0, |y| *y as i32)];
    let r = q.world(function, unit, target, class, &mut tile);
    if let Some(x) = io_x {
        *x = tile[0] as c_long;
        if let Some(y) = io_y {
            *y = tile[1] as c_long;
        }
    }
    r
}

/// `EECombatComputeDamage`: the amount written only when the call answers true.
#[allow(clippy::too_many_arguments)]
pub fn compute_damage<Q: Queries + ?Sized>(
    q: &mut Q,
    form: i32,
    unit: u64,
    target: u64,
    class: u64,
    attack: f32,
    ammo: c_long,
    weapon: c_long,
    friendly_factor: f32,
    out_amount: &mut c_long,
) -> bool {
    let mut call =
        EeCombatDamageCall { form, attack, ammo: ammo as i32, weapon: weapon as i32, friendly_factor, amount: 0 };
    if !q.compute_damage(unit, target, class, &mut call) {
        return false;
    }
    *out_amount = call.amount as c_long;
    true
}

/// `EECombatIsStillReachable`: its CBQR record (the target before, the goal's path attempts and the ignored
/// targets' count after) when the outermost host read.
pub fn still_reachable<Q: Queries + ?Sized>(q: &mut Q, unit: u64, goal: u64) -> bool {
    scoped(0, |outer| {
        let target = q.attack_target(goal);
        let (reachable, ignored) = q.still_reachable(unit, goal);
        if outer {
            let attempts = q.path_attempts(goal);
            q.capture_read(0, EE_COMBAT_HR_STILL_REACHABLE, unit, target, reachable, [attempts, ignored]);
        }
        reachable
    })
}

/// `EECombatIsGateClosed`: its CBQR record (the target after) when the outermost host read.
pub fn gate_closed<Q: Queries + ?Sized>(q: &mut Q, unit: u64, goal: u64) -> bool {
    scoped(0, |outer| {
        let closed = q.gate_closed(unit, goal);
        if outer {
            let target = q.attack_target(goal);
            q.capture_read(0, EE_COMBAT_HR_GATE_CLOSED, unit, target, closed, [0, 0]);
        }
        closed
    })
}

/// `EEIsEnemyWithinRange(unit)` (EEFOFindNewTargetNearOldTarget.cpp): its CBQR record (the search timer after)
/// when the outermost host read.
pub fn is_enemy_within_range<Q: Queries + ?Sized>(q: &mut Q, unit: u64) -> bool {
    scoped(0, |outer| {
        let in_range = q.enemy_in_range(unit);
        if outer {
            let timer = q.search_timer(unit);
            q.capture_read(0, EE_COMBAT_HR_ENEMY_IN_RANGE, unit, 0, in_range, [timer, 0]);
        }
        in_range
    })
}

/// `EEIsAttackReachable(unit, target, outTile)` (EEFOFindNewTargetNearOldTarget.cpp): the world's attack-reachable
/// test from the tile and into it, its CBXR record (the tile after).
pub fn is_attack_reachable<Q: Queries + ?Sized>(
    q: &mut Q,
    unit: u64,
    target: u64,
    x: &mut c_long,
    y: &mut c_long,
) -> bool {
    let reachable =
        world(q, crate::combat::world::EE_COMBAT_WORLD_ATTACK_REACHABLE, unit, target, 0, Some(&mut *x), Some(&mut *y))
            != 0;
    q.capture_read(1, EE_COMBAT_XR_ATTACK_REACHABLE, unit, target, reachable, [*x as u32, *y as u32]);
    reachable
}

/// cbindgen:ignore
#[cfg(not(test))]
mod facing {
    use super::*;

    extern "C" {
        fn ee_units_cg_host(op: i32, unit: u64, a: u64, b: i64, c: i64, f: *mut [f32; 4]) -> i64;
        /// EECombatGoals.cpp: turret part `index` of the unit's model
        fn ee_units_cg_facing_turret(unit: u64, index: i32, out: *mut crate::combat::reads::EeCombatTurret);
    }

    pub struct Game;

    impl Facing for Game {
        fn class_arc(&mut self, class: u64) -> (f32, f32, bool) {
            // SAFETY: the C++ caller holds the world; its class is live
            let t = unsafe { crate::world::WorldRead::begin() };
            // SAFETY: as above
            let c = unsafe { crate::world::view::ClassView::of(&t, class) }.expect("a live class");
            let cf = c.fields();
            (
                crate::world_field!(cf, mFiringArc),
                crate::world_field!(cf, mFacingOffset),
                crate::world_field!(cf, mFiresBroadside),
            )
        }
        fn unit_facts(&mut self, unit: u64) -> ([[f32; 4]; 4], i32, bool) {
            crate::units::unit_geom::with_unit(unit, |u| {
                let class = u.class().expect("a unit's class");
                (u.transform(), class.unit_type() as i32, crate::world_field!(u.complex(), mIsSpecialAttack))
            })
            .expect("a live unit")
        }
        fn model_parts(&mut self, unit: u64) -> i32 {
            // SAFETY: the caller's live unit
            unsafe { ee_units_cg_host(EE_UNITS_CG_MODEL_PARTS, unit, 0, 0, 0, &mut [0.0; 4]) as i32 }
        }
        fn stats_scale(&mut self, unit: u64) -> f32 {
            let mut f = [0.0; 4];
            // SAFETY: as above
            unsafe { ee_units_cg_host(EE_UNITS_CG_STATS_SCALE, unit, 0, 0, 0, &mut f) };
            f[0]
        }
        fn turret(&mut self, unit: u64, index: i32) -> crate::combat::reads::EeCombatTurret {
            let mut out = crate::combat::reads::EeCombatTurret::default();
            // SAFETY: an index below the model's part count; a local record
            unsafe { ee_units_cg_facing_turret(unit, index, &mut out) };
            out
        }
    }
}

/// `EECombatFacesPoint(class, unit, location, useTolerance)` (the point's x, y)
/// # Safety
/// Inside the game's call; the class and unit live.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_units_cgq_faces_point(class: u64, unit: u64, x: f32, y: f32, use_tolerance: bool) -> bool {
    faces_point(&mut facing::Game, class, unit, x, y, use_tolerance)
}

/// cbindgen:ignore
#[cfg(not(test))]
mod game {
    use super::*;
    use crate::combat::reads::EeCombatQueryHost;
    use crate::combat::world::EeCombatWorldHost;
    use core::ffi::c_void;

    type Body = unsafe extern "C" fn(data: *mut c_void, host: *const EeCombatWorldHost);

    extern "C" {
        /// EECombatGoals.cpp: the call's query context (`unit`, `goal`, `class`) and host, handed to `body`
        fn ee_units_cg_query(unit: u64, goal: u64, class: u64, data: *mut c_void, body: Body);
    }

    unsafe extern "C" fn trampoline<F: FnMut(&EeCombatWorldHost)>(data: *mut c_void, host: *const EeCombatWorldHost) {
        // SAFETY: `data` is the closure `with` handed over, `host` the trampoline's live host
        unsafe { (*(data as *mut F))(&*host) }
    }

    /// `body` over a C++ query context and host of `unit`, `goal` and `class`.
    fn call<F: FnMut(&EeCombatWorldHost)>(unit: u64, goal: u64, class: u64, body: &mut F) {
        // SAFETY: the trampoline calls `body` once with a host live for the call
        unsafe { ee_units_cg_query(unit, goal, class, body as *mut F as *mut c_void, trampoline::<F>) };
    }

    /// `f` over a C++ query context and host of `unit`, `goal` and `class`.
    fn with<R>(unit: u64, goal: u64, class: u64, f: impl FnOnce(&EeCombatWorldHost) -> R) -> R {
        let mut f = Some(f);
        let mut r = None;
        call(unit, goal, class, &mut |h: &EeCombatWorldHost| r = Some((f.take().expect("called once"))(h)));
        r.expect("the trampoline called the body")
    }

    fn query(h: &EeCombatWorldHost) -> *const EeCombatQueryHost {
        &h.query
    }

    pub struct Game;

    impl Queries for Game {
        fn world(&mut self, function: i32, unit: u64, target: u64, class: u64, tile: &mut [i32; 2]) -> i32 {
            // SAFETY (every call below): the trampoline's live host
            with(unit, 0, class, |h| unsafe { crate::combat::world::ee_combat_world(function, unit, target, h, tile) })
        }
        fn compute_damage(&mut self, unit: u64, target: u64, class: u64, call: &mut EeCombatDamageCall) -> bool {
            with(unit, 0, class, |h| unsafe { crate::combat::damage::ee_combat_compute_damage(call, unit, target, h) })
        }
        fn attack_position(&mut self, firing: bool, attacker: u64, target: u64, point: &mut [f32; 3]) {
            with(attacker, 0, 0, |h| unsafe {
                crate::combat::leaves::ee_combat_attack_position(i32::from(firing), attacker, target, h, point)
            })
        }
        fn find_drop_location(&mut self, unit: u64, target: u64, point: &mut [f32; 3]) -> u64 {
            with(unit, 0, 0, |h| unsafe { crate::combat::leaves::ee_combat_find_drop_location(unit, target, h, point) })
        }
        fn hit_distance_squared(&mut self, unit: u64, target: u64, x: f32, y: f32) -> f32 {
            with(unit, 0, 0, |h| unsafe {
                crate::combat::leaves::ee_combat_hit_distance_squared(unit, target, x, y, h)
            })
        }
        fn is_seen_by(&mut self, unit: u64, player: u64, explored: bool) -> bool {
            with(0, 0, 0, |h| unsafe {
                crate::combat::leaves::ee_combat_is_seen_by(unit, player, i32::from(explored), h)
            })
        }
        fn impact_unit(&mut self, unit: u64) -> u64 {
            with(unit, 0, 0, |h| unsafe { crate::combat::reads::ee_combat_impact_unit(unit, query(h)) })
        }
        fn in_reach(&mut self, unit: u64, target: u64) -> bool {
            with(unit, 0, 0, |h| unsafe { crate::combat::reads::ee_combat_in_reach(unit, target, query(h)) != 0 })
        }
        fn still_reachable(&mut self, unit: u64, goal: u64) -> (bool, u32) {
            with(unit, goal, 0, |h| {
                let r = unsafe { crate::combat::reads::ee_combat_still_reachable(unit, query(h)) != 0 };
                let mut q = h.query;
                (
                    r,
                    crate::combat::reads::QueryHost::read(
                        &mut q,
                        crate::combat::world::EE_COMBAT_Q_IGNORED_COUNT,
                        unit,
                        0,
                    ),
                )
            })
        }
        fn gate_closed(&mut self, unit: u64, goal: u64) -> bool {
            with(unit, goal, 0, |h| unsafe { crate::combat::reads::ee_combat_gate_closed(unit, query(h)) != 0 })
        }
        fn heal_in_range(&mut self, healer: u64, target: u64) -> bool {
            with(healer, 0, 0, |h| unsafe {
                crate::combat::reads::ee_combat_heal_in_range(healer, target, query(h)) != 0
            })
        }
        fn enemy_in_range(&mut self, unit: u64) -> bool {
            with(unit, 0, 0, |h| unsafe { crate::combat::reads::ee_combat_enemy_in_range(unit, query(h)) != 0 })
        }
        fn attack_target(&mut self, goal: u64) -> u64 {
            let mut r = crate::combat::goal_attack::EeCombatAttackGoal::default();
            // SAFETY: the caller's live attack goal
            unsafe { crate::combat::goal_attack::ee_combat_attack_get(goal, &mut r) };
            r.target
        }
        fn path_attempts(&mut self, goal: u64) -> u32 {
            // SAFETY: the caller's live goal (headed by its base record)
            unsafe { (*(goal as usize as *const crate::units::goal_obj::EeUnitsGoalBase)).path_attempts as u32 }
        }
        fn search_timer(&mut self, unit: u64) -> u32 {
            // SAFETY: the C++ caller holds the world; its complex unit is live
            let t = unsafe { crate::world::WorldRead::begin() };
            // SAFETY: as above
            let u = unsafe { crate::world::UnitView::of(&t, unit) }.expect("a live unit");
            let c = u.complex();
            crate::world_field!(c, mTargetSearchTimer) as u32
        }
        fn hit_points(&mut self, unit: u64) -> i32 {
            crate::units::unit_geom::with_unit(unit, |u| u.hit_points() as i32).expect("a live unit")
        }
        fn distance_squared_to(&mut self, candidate: u64, target: u64) -> f32 {
            crate::units::unit_geom::with_unit(candidate, |c| {
                crate::units::unit_geom::with_unit(target, |t| crate::units::unit_geom::distance_squared_to(c, t))
            })
            .flatten()
            .expect("live units")
        }
        fn class_facts(&mut self, unit: u64) -> ClassFacts {
            crate::units::unit_geom::with_unit(unit, |u| {
                let class = u.class().expect("a unit's class");
                let cf = class.fields();
                ClassFacts {
                    inner_sphere: crate::world_field!(cf, mInnerCollisionSphere),
                    is_wall: crate::world_field!(cf, mIsWall),
                    is_tree: crate::world_field!(cf, mIsTree),
                    unit_type: crate::world_field!(cf, mUnitType) as u32,
                    category: crate::world_field!(cf, mCategory) as u32,
                }
            })
            .expect("a live unit")
        }
        fn capture_active(&mut self) -> bool {
            crate::app::capture_record::active()
        }
        fn ignored_targets(&mut self, unit: u64) -> Vec<crate::combat::world::Ignored> {
            with(unit, 0, 0, |h| {
                let mut w = *h;
                let count = crate::combat::reads::QueryHost::read(
                    &mut w,
                    crate::combat::world::EE_COMBAT_Q_IGNORED_COUNT,
                    unit,
                    0,
                );
                (0..count).map(|i| crate::combat::world::ignored_entry(&mut w, unit, i)).collect()
            })
        }
        fn unit_id(&mut self, unit: u64) -> u32 {
            crate::units::unit_geom::with_unit(unit, |u| u.id()).unwrap_or(0)
        }
        fn fold(&mut self, which: u32, bytes: &[u8]) {
            crate::combat::capture_fold::fold(which, bytes)
        }
        fn goal_state(&mut self, unit: u64) -> (u64, u8) {
            crate::units::unit_geom::with_unit(unit, |u| (u.goal_handle(), u.goal().map_or(0, |(k, _)| k)))
                .unwrap_or((0, 0))
        }
        fn capture(&mut self, tag: u32, bytes: &[u8]) {
            if crate::app::capture_record::active() {
                crate::app::capture_record::commit(tag, bytes);
            }
        }
        fn capture_read(&mut self, which: u32, func: u32, unit: u64, target: u64, result: bool, extra: [u32; 2]) {
            let e = [extra[0], extra[1], 0];
            crate::combat::capture_units::ee_combat_capture_read(which, func, unit, target, i32::from(result), &e);
        }
    }
}

// The entry points (each the C++ function's whole body; every handle a live object of its kind or 0, every pointer
// writable where the original wrote it).

/// `EECombatWorld(function, unit, target, class, ioX, ioY)`
/// # Safety
/// As above.
#[cfg(not(test))]
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_units_cgq_world(
    function: i32,
    unit: u64,
    target: u64,
    class: u64,
    io_x: *mut c_long,
    io_y: *mut c_long,
) -> i32 {
    let q = &mut game::Game;
    world(q, function, unit, target, class, unsafe { io_x.as_mut() }, unsafe { io_y.as_mut() })
}

/// `EECombatComputeDamage(form, unit, target, attack, ammo, weapon, friendly, outAmount)` (`class` the unit's)
/// # Safety
/// As above.
#[cfg(not(test))]
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_units_cgq_compute_damage(
    form: i32,
    unit: u64,
    target: u64,
    class: u64,
    attack: f32,
    ammo: c_long,
    weapon: c_long,
    friendly_factor: f32,
    out_amount: *mut c_long,
) -> bool {
    let q = &mut game::Game;
    compute_damage(q, form, unit, target, class, attack, ammo, weapon, friendly_factor, unsafe { &mut *out_amount })
}

/// `EECombatAttackPosition(firing, attacker, target, ioLocation)` (the point's three floats)
/// # Safety
/// As above.
#[cfg(not(test))]
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_units_cgq_attack_position(firing: bool, attacker: u64, target: u64, point: *mut [f32; 3]) {
    let q = &mut game::Game;
    q.attack_position(firing, attacker, target, unsafe { &mut *point })
}

/// `EECombatFindDropLocation(unit, target, outLocation)`
/// # Safety
/// As above.
#[cfg(not(test))]
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_units_cgq_find_drop_location(unit: u64, target: u64, point: *mut [f32; 3]) -> u64 {
    let q = &mut game::Game;
    q.find_drop_location(unit, target, unsafe { &mut *point })
}

/// `EECombatHitDistanceSquared(unit, target, x, y)`
/// # Safety
/// As above.
#[cfg(not(test))]
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_units_cgq_hit_distance_squared(unit: u64, target: u64, x: f32, y: f32) -> f32 {
    let q = &mut game::Game;
    q.hit_distance_squared(unit, target, x, y)
}

/// `EECombatIsSeenBy(unit, player, explored)`
/// # Safety
/// As above.
#[cfg(not(test))]
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_units_cgq_is_seen_by(unit: u64, player: u64, explored: bool) -> bool {
    let q = &mut game::Game;
    q.is_seen_by(unit, player, explored)
}

/// `EECombatFindImpactUnit(unit)`
/// # Safety
/// As above.
#[cfg(not(test))]
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_units_cgq_impact_unit(unit: u64) -> u64 {
    let q = &mut game::Game;
    q.impact_unit(unit)
}

/// `EECombatIsInReach(unit, target)`
/// # Safety
/// As above.
#[cfg(not(test))]
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_units_cgq_in_reach(unit: u64, target: u64) -> bool {
    let q = &mut game::Game;
    q.in_reach(unit, target)
}

/// `EECombatIsStillReachable(unit, goal)` with its CBQR record
/// # Safety
/// As above.
#[cfg(not(test))]
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_units_cgq_still_reachable(unit: u64, goal: u64) -> bool {
    let q = &mut game::Game;
    still_reachable(q, unit, goal)
}

/// `EECombatIsGateClosed(unit, goal)` with its CBQR record
/// # Safety
/// As above.
#[cfg(not(test))]
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_units_cgq_gate_closed(unit: u64, goal: u64) -> bool {
    let q = &mut game::Game;
    gate_closed(q, unit, goal)
}

/// `EEIsEnemyWithinRange(unit)` with its CBQR record
/// # Safety
/// As above.
#[cfg(not(test))]
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_units_cgq_is_enemy_within_range(unit: u64) -> bool {
    let q = &mut game::Game;
    is_enemy_within_range(q, unit)
}

/// `EEIsAttackReachable(unit, target, outTile)` with its CBXR record (`x`, `y` the tile's longs)
/// # Safety
/// As above.
#[cfg(not(test))]
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_units_cgq_is_attack_reachable(
    unit: u64,
    target: u64,
    x: *mut c_long,
    y: *mut c_long,
) -> bool {
    let q = &mut game::Game;
    is_attack_reachable(q, unit, target, unsafe { &mut *x }, unsafe { &mut *y })
}

/// `EENearOldScreen`: a candidate of the old target's screen (`range` its `mDistanceSquared`) with its CBFS record
/// # Safety
/// As above.
#[cfg(not(test))]
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_units_cgq_near_old_screen(candidate: u64, target: u64, range: f32) -> bool {
    let q = &mut game::Game;
    near_old_screen(q, candidate, target, range)
}

/// `EEIsInWorkReach(unit, target)` with its CBRW record
/// # Safety
/// As above.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_units_cgq_in_work_reach(unit: u64, target: u64) -> bool {
    in_work_reach(&mut game::Game, unit, target)
}

/// `EEIsInGatherReach(distance, unit, target)` with its CBRT record
/// # Safety
/// As above.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_units_cgq_in_gather_reach(distance_squared: f32, unit: u64, target: u64) -> bool {
    in_gather_reach(&mut game::Game, distance_squared, unit, target)
}

/// `EEIsInContactRange(unit, target, distance)`
/// # Safety
/// As above.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_units_cgq_in_contact_range(unit: u64, target: u64, distance_squared: f32) -> bool {
    in_contact_range(&mut game::Game, unit, target, distance_squared)
}

/// `EECombatCaptureIgnored(func, unit, target, result, checkLocation)` (CBXR's fold)
/// # Safety
/// As above.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_units_cgq_capture_ignored(func: u32, unit: u64, target: u64, result: i32, check: u32) {
    capture_ignored(&mut game::Game, func, unit, target, result, check)
}

/// `EECombatShotTarget(unit)`
/// # Safety
/// As above.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_units_cgq_shot_target(unit: u64) -> u64 {
    shot_target(&mut game::Game, unit)
}

/// The query host's `ATTACK_REACHABLE` (`EEIsAttackReachable` from a zero tile, its CBXR record)
/// # Safety
/// As above.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_units_cgq_attack_reachable_from_zero(unit: u64, target: u64) -> bool {
    let (mut x, mut y): (c_long, c_long) = (0, 0);
    is_attack_reachable(&mut game::Game, unit, target, &mut x, &mut y)
}

/// The query host's `CONTACT_RANGE` (`EEIsInContactRange` of the distance's bits)
/// # Safety
/// As above.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_units_cgq_in_contact_range_bits(unit: u64, target: u64, bits: i32) -> bool {
    in_contact_range(&mut game::Game, unit, target, f32::from_bits(bits as u32))
}

/// `EECombatHealInRange(healer, target)`
/// # Safety
/// As above.
#[cfg(not(test))]
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_units_cgq_heal_in_range(healer: u64, target: u64) -> bool {
    let q = &mut game::Game;
    q.heal_in_range(healer, target)
}

/// `EECombatIsEnemyWithinRange(unit)`
/// # Safety
/// As above.
#[cfg(not(test))]
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_units_cgq_enemy_in_range(unit: u64) -> bool {
    let q = &mut game::Game;
    q.enemy_in_range(unit)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(Default)]
    struct Mock {
        log: Vec<String>,
        tile_out: [i32; 2],
        answer: bool,
        ignored: u32,
        damage: Option<i32>,
        hp: i32,
        captured: Vec<(u32, Vec<u8>)>,
    }

    impl Queries for Mock {
        fn world(&mut self, function: i32, unit: u64, target: u64, class: u64, tile: &mut [i32; 2]) -> i32 {
            self.log.push(format!("world {function} {unit} {target} {class} {tile:?}"));
            *tile = self.tile_out;
            7
        }
        fn compute_damage(&mut self, unit: u64, target: u64, class: u64, call: &mut EeCombatDamageCall) -> bool {
            self.log.push(format!("damage {unit} {target} {class} {} {} {}", call.form, call.ammo, call.weapon));
            match self.damage {
                Some(a) => {
                    call.amount = a;
                    true
                }
                None => false,
            }
        }
        fn attack_position(&mut self, _: bool, _: u64, _: u64, _: &mut [f32; 3]) {}
        fn find_drop_location(&mut self, _: u64, _: u64, _: &mut [f32; 3]) -> u64 {
            0
        }
        fn hit_distance_squared(&mut self, _: u64, _: u64, _: f32, _: f32) -> f32 {
            0.0
        }
        fn is_seen_by(&mut self, _: u64, _: u64, _: bool) -> bool {
            false
        }
        fn impact_unit(&mut self, _: u64) -> u64 {
            0
        }
        fn in_reach(&mut self, _: u64, _: u64) -> bool {
            false
        }
        fn still_reachable(&mut self, unit: u64, goal: u64) -> (bool, u32) {
            self.log.push(format!("still {unit} {goal}"));
            // a nested scoped read is not the outermost
            assert!(!ee_combat_host_read_enter(0));
            ee_combat_host_read_leave(0);
            (self.answer, self.ignored)
        }
        fn gate_closed(&mut self, unit: u64, goal: u64) -> bool {
            self.log.push(format!("gate {unit} {goal}"));
            self.answer
        }
        fn heal_in_range(&mut self, _: u64, _: u64) -> bool {
            false
        }
        fn enemy_in_range(&mut self, unit: u64) -> bool {
            self.log.push(format!("enemy {unit}"));
            self.answer
        }
        fn attack_target(&mut self, goal: u64) -> u64 {
            self.log.push(format!("target {goal}"));
            goal + 100
        }
        fn path_attempts(&mut self, goal: u64) -> u32 {
            self.log.push(format!("attempts {goal}"));
            3
        }
        fn search_timer(&mut self, unit: u64) -> u32 {
            self.log.push(format!("timer {unit}"));
            250
        }
        fn hit_points(&mut self, unit: u64) -> i32 {
            self.log.push(format!("hp {unit}"));
            self.hp
        }
        fn distance_squared_to(&mut self, candidate: u64, target: u64) -> f32 {
            self.log.push(format!("distance {candidate} {target}"));
            9.0
        }
        fn capture(&mut self, tag: u32, bytes: &[u8]) {
            self.captured.push((tag, bytes.to_vec()));
        }
        fn capture_active(&mut self) -> bool {
            true
        }
        fn ignored_targets(&mut self, unit: u64) -> Vec<crate::combat::world::Ignored> {
            self.log.push(format!("ignored {unit}"));
            vec![crate::combat::world::Ignored { tile: (1, 2), from: (3, 4), unit_id: 5, time: 6 }]
        }
        fn unit_id(&mut self, unit: u64) -> u32 {
            unit as u32 + 1000
        }
        fn fold(&mut self, which: u32, bytes: &[u8]) {
            self.captured.push((which, bytes.to_vec()));
        }
        fn goal_state(&mut self, unit: u64) -> (u64, u8) {
            (unit + 50, if unit == 1 { 2 } else { 4 })
        }
        fn class_facts(&mut self, unit: u64) -> ClassFacts {
            self.log.push(format!("class {unit}"));
            if unit == 1 {
                ClassFacts { inner_sphere: 0.5, category: 0x16, ..Default::default() }
            } else {
                ClassFacts { inner_sphere: 1.0, is_wall: true, is_tree: true, unit_type: 0x33a, ..Default::default() }
            }
        }
        fn capture_read(&mut self, which: u32, func: u32, unit: u64, target: u64, result: bool, extra: [u32; 2]) {
            let w = if which == 0 { "capture" } else { "xcapture" };
            self.log.push(format!("{w} {func} {unit} {target} {result} {extra:?}"));
        }
    }

    #[test]
    fn the_world_call_reads_and_writes_the_tile_through_the_pointers() {
        let mut m = Mock { tile_out: [4, 5], ..Default::default() };
        let (mut x, mut y): (c_long, c_long) = (1, 2);
        assert_eq!(world(&mut m, 9, 10, 11, 12, Some(&mut x), Some(&mut y)), 7);
        assert_eq!((x, y), (4, 5));
        assert_eq!(m.log, ["world 9 10 11 12 [1, 2]"]);
        // no pointers: a zero tile in, nothing written
        let mut m = Mock { tile_out: [4, 5], ..Default::default() };
        assert_eq!(world(&mut m, 1, 2, 0, 0, None, None), 7);
        assert_eq!(m.log, ["world 1 2 0 0 [0, 0]"]);
    }

    #[test]
    fn the_damage_amount_is_written_only_on_true() {
        let mut out: c_long = -1;
        let mut m = Mock::default();
        assert!(!compute_damage(&mut m, 1, 2, 3, 4, 5.0, 6, 7, 0.5, &mut out));
        assert_eq!(out, -1);
        let mut m = Mock { damage: Some(42), ..Default::default() };
        assert!(compute_damage(&mut m, 1, 2, 3, 4, 5.0, 6, 7, 0.5, &mut out));
        assert_eq!(out, 42);
        assert_eq!(m.log, ["damage 2 3 4 1 6 7"]);
    }

    #[test]
    fn still_reachable_records_the_target_before_and_the_counts_after() {
        let mut m = Mock { answer: true, ignored: 2, ..Default::default() };
        assert!(still_reachable(&mut m, 5, 6));
        assert_eq!(m.log, ["target 6", "still 5 6", "attempts 6", "capture 2 5 106 true [3, 2]"]);
        // nested inside another host read: no record
        let mut m = Mock::default();
        assert!(ee_combat_host_read_enter(0));
        assert!(!still_reachable(&mut m, 5, 6));
        ee_combat_host_read_leave(0);
        assert_eq!(m.log, ["target 6", "still 5 6"]);
    }

    #[test]
    fn gate_closed_records_the_target_after() {
        let mut m = Mock { answer: true, ..Default::default() };
        assert!(gate_closed(&mut m, 5, 6));
        assert_eq!(m.log, ["gate 5 6", "target 6", "capture 3 5 106 true [0, 0]"]);
    }

    #[test]
    fn the_enemy_test_records_the_timer_after() {
        let mut m = Mock { answer: true, ..Default::default() };
        assert!(is_enemy_within_range(&mut m, 8));
        assert_eq!(m.log, ["enemy 8", "timer 8", "capture 4 8 0 true [250, 0]"]);
    }

    #[test]
    fn attack_reachable_asks_the_world_from_the_tile_and_records_it_after() {
        let mut m = Mock { tile_out: [12, 13], ..Default::default() };
        let (mut x, mut y): (c_long, c_long) = (3, 4);
        assert!(is_attack_reachable(&mut m, 8, 9, &mut x, &mut y));
        assert_eq!((x, y), (12, 13));
        assert_eq!(m.log, ["world 6 8 9 0 [3, 4]", "xcapture 4 8 9 true [12, 13]"]);
    }

    #[test]
    fn the_near_old_screen_reads_the_distance_of_a_living_candidate_only() {
        let mut m = Mock { hp: 10, ..Default::default() };
        assert!(near_old_screen(&mut m, 1, 2, 25.0));
        assert_eq!(m.log, ["hp 1", "distance 1 2"]);
        let w: Vec<u32> = m.captured[0].1.chunks(4).map(|c| u32::from_le_bytes(c.try_into().unwrap())).collect();
        assert_eq!(m.captured[0].0, 0x5346_4243);
        assert_eq!(w, [1, 10, 25.0f32.to_bits(), 1, 9.0f32.to_bits(), 1]);
        let mut m = Mock { hp: 0, ..Default::default() };
        assert!(!near_old_screen(&mut m, 1, 2, 25.0));
        assert_eq!(m.log, ["hp 1"]);
        let w: Vec<u32> = m.captured[0].1.chunks(4).map(|c| u32::from_le_bytes(c.try_into().unwrap())).collect();
        assert_eq!(w, [1, 0, 25.0f32.to_bits(), 0, 0, 0]);
    }

    fn words(b: &[u8]) -> Vec<u32> {
        b.chunks(4).map(|c| u32::from_le_bytes(c.try_into().unwrap())).collect()
    }

    #[test]
    fn the_reach_tests_record_their_inputs_and_result() {
        let mut m = Mock::default();
        let r = in_work_reach(&mut m, 1, 2);
        assert_eq!(m.log, ["class 1", "class 2", "distance 1 2"]);
        let (tag, b) = &m.captured[0];
        assert_eq!(*tag, CBRW);
        assert_eq!(words(b), [9.0f32.to_bits(), 0.5f32.to_bits(), 1.0f32.to_bits(), 0, 1, 1, u32::from(r)]);
        let mut m = Mock::default();
        let r = in_gather_reach(&mut m, 100.0, 1, 2);
        let (tag, b) = &m.captured[0];
        assert_eq!(*tag, CBRT);
        assert_eq!(words(b)[..6], [100.0f32.to_bits(), 0.5f32.to_bits(), 1.0f32.to_bits(), 0, 1, 1]);
        assert_eq!(words(b)[6], u32::from(r));
    }

    #[test]
    fn the_contact_range_hands_the_distance_bits_as_the_tile() {
        let mut m = Mock::default();
        assert!(in_contact_range(&mut m, 3, 4, 2.0));
        let b = 2.0f32.to_bits() as i32;
        assert_eq!(m.log, [format!("world 10 3 4 0 [{b}, {b}]")]);
    }

    #[test]
    fn the_ignored_capture_hashes_the_entries() {
        let mut m = Mock::default();
        capture_ignored(&mut m, 2, 7, 0, 1, 0);
        let (which, b) = &m.captured[0];
        assert_eq!(*which, crate::combat::capture_fold::EE_COMBAT_FOLD_X_READS);
        let mut hash = 0x811c_9dc5u32;
        for f in [1u32, 2, 3, 4, 5, 6] {
            for byte in f.to_le_bytes() {
                hash = (hash ^ u32::from(byte)).wrapping_mul(0x0100_0193);
            }
        }
        assert_eq!(words(b), [2, 1007, 0, 1, 0, 1, hash]);
    }

    #[test]
    fn the_shot_target_is_an_attack_unit_goals_target() {
        let mut m = Mock::default();
        assert_eq!(shot_target(&mut m, 1), 151);
        assert_eq!(shot_target(&mut m, 2), 0);
    }

    struct FacingMock {
        log: Vec<String>,
        parts: i32,
    }

    impl Facing for FacingMock {
        fn class_arc(&mut self, class: u64) -> (f32, f32, bool) {
            self.log.push(format!("class {class}"));
            (1.0, 0.5, true)
        }
        fn unit_facts(&mut self, unit: u64) -> ([[f32; 4]; 4], i32, bool) {
            self.log.push(format!("unit {unit}"));
            ([[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0], [0.0, 0.0, 0.0, 1.0]], 7, false)
        }
        fn model_parts(&mut self, unit: u64) -> i32 {
            self.log.push(format!("parts {unit}"));
            self.parts
        }
        fn stats_scale(&mut self, unit: u64) -> f32 {
            self.log.push(format!("scale {unit}"));
            1.0
        }
        fn turret(&mut self, unit: u64, index: i32) -> crate::combat::reads::EeCombatTurret {
            self.log.push(format!("turret {unit} {index}"));
            crate::combat::reads::EeCombatTurret::default()
        }
    }

    #[test]
    fn facing_reads_the_scale_only_with_turrets() {
        let mut m = FacingMock { log: Vec::new(), parts: 0 };
        let _ = faces_point(&mut m, 1, 2, 3.0, 4.0, true);
        assert_eq!(m.log, ["unit 2", "class 1", "parts 2"]);
        let mut m = FacingMock { log: Vec::new(), parts: 1 };
        let _ = faces_point(&mut m, 1, 2, 3.0, 4.0, true);
        assert_eq!(&m.log[..4], ["unit 2", "class 1", "parts 2", "scale 2"]);
    }

    #[test]
    fn the_scope_depth_is_per_slot() {
        assert!(ee_combat_host_read_enter(1));
        assert!(!ee_combat_host_read_enter(1));
        assert!(ee_combat_host_read_enter(2));
        ee_combat_host_read_leave(2);
        ee_combat_host_read_leave(1);
        ee_combat_host_read_leave(1);
        assert!(ee_combat_host_read_enter(1));
        ee_combat_host_read_leave(1);
    }
}
