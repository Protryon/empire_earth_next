use super::*;
use crate::reads::QueryHost;
use crate::world::EeCombatWorldArgs;
use ee_ge::types::Ge3DLine;
use std::collections::HashMap;

/// Reads by (code, object), handles by (code, object), operations answered from `answers` and logged; locations by
/// object (translation only).
#[derive(Default)]
struct World {
    reads: HashMap<(i32, u64), u32>,
    handles: HashMap<(i32, u64), u64>,
    answers: HashMap<i32, u64>,
    locations: HashMap<u64, [f32; 3]>,
    path_start: [f32; 3],
    /// the points operations write out in `a[0..3]`, by code
    points: HashMap<i32, [f32; 3]>,
    log: Vec<(i32, u64, u64, EeCombatWorldArgs)>,
    captures: Vec<(u32, Vec<u8>)>,
}

impl World {
    fn set(&mut self, code: i32, object: u64, value: u32) -> &mut Self {
        self.reads.insert((code, object), value);
        self
    }
    fn real(&mut self, code: i32, object: u64, value: f32) -> &mut Self {
        self.set(code, object, value.to_bits())
    }
    fn ops(&self) -> Vec<i32> {
        self.log.iter().map(|l| l.0).collect()
    }
}

impl QueryHost for World {
    fn read(&mut self, code: i32, object: EeCombatHandle, _arg: i32) -> u32 {
        self.reads.get(&(code, object)).copied().unwrap_or(0)
    }
    fn handle(&mut self, code: i32, object: EeCombatHandle, _arg: i32) -> EeCombatHandle {
        self.handles.get(&(code, object)).copied().unwrap_or(0)
    }
    fn act(&mut self, _code: i32, _object: EeCombatHandle, _arg: i32) {}
    fn location(&mut self, object: EeCombatHandle) -> GeTransformation {
        let mut t = GeTransformation::IDENTITY;
        let p = self.locations.get(&object).copied().unwrap_or_default();
        t.m[0][3] = p[0];
        t.m[1][3] = p[1];
        t.m[2][3] = p[2];
        t
    }
    fn intersects(&mut self, _: EeCombatHandle, _: &Ge3DLine, _: bool, _: u32, _: f32) -> bool {
        false
    }
    fn footprint_tile(&mut self, _object: EeCombatHandle) -> (i32, i32) {
        (0, 0)
    }
}

impl WorldHost for World {
    fn op(&mut self, code: i32, object: EeCombatHandle, other: EeCombatHandle, args: &mut EeCombatWorldArgs) -> u64 {
        self.log.push((code, object, other, *args));
        if code == EE_COMBAT_W_CAPTURE {
            // the record's bytes as the C++ writes them: the input's, then the output's
            let mut bytes = Vec::new();
            for (at, size) in [(args[1], args[2]), (args[3], args[4])] {
                // SAFETY: the operation's contract: the addresses are live records of these sizes
                bytes.extend_from_slice(unsafe { std::slice::from_raw_parts(at as usize as *const u8, size as usize) });
            }
            self.captures.push((args[0] as u32, bytes));
        }
        if let Some(p) = self.points.get(&code) {
            for (slot, v) in args.iter_mut().zip(p) {
                *slot = i64::from(v.to_bits());
            }
        }
        if code == EE_COMBAT_W_PATH_START {
            for (slot, v) in args.iter_mut().zip(self.path_start) {
                *slot = i64::from(v.to_bits());
            }
        }
        self.answers.get(&code).copied().unwrap_or(0)
    }
}

const UNIT: u64 = 0x10;
const TARGET: u64 = 0x20;

fn ranged() -> World {
    let mut w = World::default();
    w.real(EE_COMBAT_Q_RANGE, UNIT, 5.0).real(EE_COMBAT_Q_HIT_DISTANCE_SQUARED, TARGET, 16.0);
    w
}

#[test]
fn in_range_to_hit_tests_range_continent_and_line_in_order() {
    // in range, same continent, a clear line: true; CBRH recorded before the continent test
    let mut w = ranged();
    assert!(in_range_to_hit(&mut w, UNIT, TARGET));
    assert_eq!(w.ops(), [EE_COMBAT_W_CAPTURE, EE_COMBAT_W_OTHER_CONTINENT, EE_COMBAT_W_LINE_BLOCKED]);
    assert_eq!(w.captures[0].0, u32::from_le_bytes(*b"CBRH"));
    assert_eq!(w.captures[0].1.len(), std::mem::size_of::<EeCombatRangeIn>() + 4);
    assert_eq!(&w.captures[0].1[24..], &1i32.to_le_bytes());
    // out of range: nothing after the capture
    let mut w = ranged();
    w.real(EE_COMBAT_Q_HIT_DISTANCE_SQUARED, TARGET, 26.0);
    assert!(!in_range_to_hit(&mut w, UNIT, TARGET));
    assert_eq!(w.ops(), [EE_COMBAT_W_CAPTURE]);
    // a land building's half inner sphere comes off the distance
    w.set(EE_COMBAT_Q_LAND_BUILDING, TARGET, 1).real(EE_COMBAT_Q_INNER_SPHERE, TARGET, 4.0);
    assert!(in_range_to_hit(&mut w, UNIT, TARGET));
    // a crusader's heal range
    let mut w = ranged();
    w.set(EE_COMBAT_Q_CRUSADER, UNIT, 1).real(EE_COMBAT_Q_HEAL_RANGE, UNIT, 3.0);
    assert!(!in_range_to_hit(&mut w, UNIT, TARGET));
    // another continent; a blocked line; shooting high skips the line
    let mut w = ranged();
    w.answers.insert(EE_COMBAT_W_OTHER_CONTINENT, 1);
    assert!(!in_range_to_hit(&mut w, UNIT, TARGET));
    assert_eq!(w.ops(), [EE_COMBAT_W_CAPTURE, EE_COMBAT_W_OTHER_CONTINENT]);
    let mut w = ranged();
    w.answers.insert(EE_COMBAT_W_LINE_BLOCKED, 1);
    assert!(!in_range_to_hit(&mut w, UNIT, TARGET));
    w.set(EE_COMBAT_Q_SHOOTS_HIGH, UNIT, 1);
    assert!(in_range_to_hit(&mut w, UNIT, TARGET));
    let mut w = ranged();
    w.answers.insert(EE_COMBAT_W_LINE_BLOCKED, 1);
    w.set(EE_COMBAT_Q_FIRE_OVER_WALLS, UNIT, 1);
    assert!(in_range_to_hit(&mut w, UNIT, TARGET));
}

#[test]
fn contact_range_reads_the_hit_distance_only_when_negative_and_walls_count_as_one() {
    let mut w = World::default();
    w.real(EE_COMBAT_Q_INNER_SPHERE, UNIT, 1.0).real(EE_COMBAT_Q_INNER_SPHERE, TARGET, 3.0);
    // reach (1.42 * 0.05 + 1 + 3) * 1.42 = 5.781; d * 4 < reach^2 (33.42): d 8.3 in, 8.4 out
    assert!(in_contact_range(&mut w, UNIT, TARGET, 8.3));
    assert!(!in_contact_range(&mut w, UNIT, TARGET, 8.4));
    // a wall: (0.071 + 2) * 1.42 = 2.941; 2.941^2 / 4 = 2.162
    w.set(EE_COMBAT_Q_CLASS_FLAGS, TARGET, EE_COMBAT_CLASS_WALL);
    assert!(in_contact_range(&mut w, UNIT, TARGET, 2.1));
    assert!(!in_contact_range(&mut w, UNIT, TARGET, 2.2));
    // negative: the hit distance
    w.real(EE_COMBAT_Q_HIT_DISTANCE_SQUARED, TARGET, 2.0);
    assert!(in_contact_range(&mut w, UNIT, TARGET, -1.0));
    assert!(w.captures.iter().all(|c| c.0 == u32::from_le_bytes(*b"CBRC")));
    let last = &w.captures.last().unwrap().1;
    assert_eq!(&last[..4], &2.0f32.to_le_bytes());
    assert_eq!(&last[12..16], &CONTACT_MARGIN.to_le_bytes());
}

#[test]
fn attacked_by_needs_the_targets_attack_goal_on_the_unit() {
    const COMPLEX: u64 = 0x30;
    let mut w = World::default();
    assert!(!is_attacked_by(&mut w, UNIT, TARGET));
    w.handles.insert((EE_COMBAT_H_COMPLEX, TARGET), COMPLEX);
    w.set(EE_COMBAT_Q_GOAL_KIND, COMPLEX, 2).set(EE_COMBAT_Q_ATTACK_GOAL, COMPLEX, 1);
    w.handles.insert((EE_COMBAT_H_GOAL_TARGET, COMPLEX), UNIT);
    assert!(is_attacked_by(&mut w, UNIT, TARGET));
    w.set(EE_COMBAT_Q_TOWER, UNIT, 1);
    assert!(!is_attacked_by(&mut w, UNIT, TARGET));
    w.set(EE_COMBAT_Q_TOWER, UNIT, 0).set(EE_COMBAT_Q_GOAL_KIND, COMPLEX, u32::MAX);
    assert!(!is_attacked_by(&mut w, UNIT, TARGET));
    w.set(EE_COMBAT_Q_GOAL_KIND, COMPLEX, 2).set(EE_COMBAT_Q_ATTACK_GOAL, COMPLEX, 0);
    assert!(!is_attacked_by(&mut w, UNIT, TARGET));
}

fn moving() -> World {
    let mut w = World::default();
    w.handles.insert((EE_COMBAT_H_COMPLEX, TARGET), TARGET);
    w.locations.insert(UNIT, [0.0, 0.0, 0.0]);
    w.locations.insert(TARGET, [10.0, 0.0, 0.0]);
    w.set(EE_COMBAT_Q_WAYPOINTS, TARGET, 2);
    w.real(EE_COMBAT_Q_MAX_VELOCITY, UNIT, 2.0).real(EE_COMBAT_Q_MAX_VELOCITY, TARGET, 1.0);
    w.answers.insert(EE_COMBAT_W_TILE_AT, 1);
    w
}

#[test]
fn lead_position_intercepts_a_moving_target() {
    // the target moves along +y (its next waypoint), the attacker at the origin twice as fast
    let mut w = moving();
    w.path_start = [10.0, 5.0, 0.0];
    let mut out = Ge3DPoint { x: 7.0, y: 7.0, z: 7.0 };
    lead_position(&mut w, UNIT, TARGET, &mut out);
    // the quadratic 3 t^2 + 0 t - 100: t = 10 / sqrt(3)
    let (first, _) = um_quadratic(3.0, 0.0, -100.0).unwrap();
    assert_eq!(out, Ge3DPoint { x: 10.0, y: first, z: 0.0 });
    assert_eq!(w.log.iter().find(|l| l.0 == EE_COMBAT_W_TILE_AT).unwrap().3[..2], [10, first as i64]);
    // off the map: the target's location
    w.answers.insert(EE_COMBAT_W_TILE_AT, 0);
    lead_position(&mut w, UNIT, TARGET, &mut out);
    assert_eq!(out, Ge3DPoint { x: 10.0, y: 0.0, z: 0.0 });
    // one waypoint: no path read, the location
    let mut w = moving();
    w.set(EE_COMBAT_Q_WAYPOINTS, TARGET, 1);
    let mut out = Ge3DPoint::default();
    lead_position(&mut w, UNIT, TARGET, &mut out);
    assert_eq!(out, Ge3DPoint { x: 10.0, y: 0.0, z: 0.0 });
    assert!(w.log.is_empty());
}

#[test]
fn lead_position_falls_back_when_no_root_is_ahead() {
    // the target runs away along +x at the attacker's speed: a = 0, b > 0, root -c / b < 0
    let mut w = moving();
    w.real(EE_COMBAT_Q_MAX_VELOCITY, TARGET, 2.0);
    w.path_start = [20.0, 0.0, 0.0];
    let mut out = Ge3DPoint::default();
    lead_position(&mut w, UNIT, TARGET, &mut out);
    assert_eq!(out, Ge3DPoint { x: 10.0, y: 0.0, z: 0.0 });
}

fn class(category: i32, unit_type: u32) -> EeCombatTargetableIn {
    EeCombatTargetableIn { category, unit_type, family: 1, can_be_attacked: 1, ..Default::default() }
}

#[test]
fn targetable_excludes_shots_storms_and_markers() {
    assert!(targetable(&class(1, 0x10)));
    for category in [9, 0xb, 0xc, 0x14] {
        assert!(!targetable(&class(category, 0x10)));
    }
    for t in [0xb6, 0x104, 0x148, 0x16e, 0x1dc, 0x1dd, 0x294] {
        assert!(!targetable(&class(1, t)));
    }
    for t in [0x1db, 0x1de, 0x293, 0x295] {
        assert!(targetable(&class(1, t)));
    }
    // an attacker without anti-air ammunition: the shot categories still out
    let mut i = class(9, 0x10);
    i.attacker = 1;
    i.ammo_type = 3;
    assert!(!targetable(&i));
    // anti-air ammunition: a flare class never; else by the damage table (a row past any table: factor 0)
    i.ammo_type = AMMO_ANTI_AIR;
    i.family = 100_000;
    i.can_be_attacked = 0;
    assert!(!targetable(&i));
    i.can_be_attacked = 1;
    assert!(!targetable(&i));
}

#[test]
fn can_target_class_keeps_balloons_for_buildings_and_computers() {
    let mut i = class(1, TYPE_BALLOON_HOT_AIR);
    assert!(can_target_class(&i));
    i.attacker = 1;
    assert!(!can_target_class(&i));
    i.computer = 1;
    assert!(can_target_class(&i));
    i.computer = 0;
    i.land_building = 1;
    i.unit_type = TYPE_BALLOON_OBSERVATION;
    assert!(can_target_class(&i));
    i.land_building = 0;
    assert!(!can_target_class(&i));
    i.unit_type = 0x10;
    assert!(can_target_class(&i));
    // SAFETY: a live record
    assert!(!unsafe { ee_combat_targetable(&class(9, 0x10), 1) });
}

#[test]
fn firing_position_stays_put_when_in_reach() {
    // a ranged unit in range: its own location; nothing past the range test's operations
    let mut w = ranged();
    w.locations.insert(UNIT, [1.0, 2.0, 3.0]);
    let mut out = Ge3DPoint { x: 9.0, y: 9.0, z: 9.0 };
    firing_position(&mut w, UNIT, TARGET, &mut out);
    assert_eq!(out, Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 });
    assert_eq!(w.ops(), [EE_COMBAT_W_CAPTURE, EE_COMBAT_W_OTHER_CONTINENT, EE_COMBAT_W_LINE_BLOCKED]);
    // a launcher of projectiles: the ballistic reach (inner sphere + range)^2 and the ballistic test
    let mut w = World::default();
    w.locations.insert(UNIT, [1.0, 2.0, 3.0]);
    w.set(EE_COMBAT_Q_PROJECTILE_CATEGORY, UNIT, 9).real(EE_COMBAT_Q_RANGE, UNIT, 4.0);
    w.real(EE_COMBAT_Q_INNER_SPHERE, UNIT, 1.0).real(EE_COMBAT_Q_HIT_DISTANCE_SQUARED, TARGET, 25.0);
    w.set(EE_COMBAT_Q_BALLISTIC_HIT, TARGET, 1);
    let mut out = Ge3DPoint::default();
    firing_position(&mut w, UNIT, TARGET, &mut out);
    assert_eq!(out, Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 });
    assert!(w.log.is_empty());
}

#[test]
fn firing_position_of_melee_contact_wall_spot_and_new_target() {
    const COMPLEX: u64 = 0x30;
    let mut w = World::default();
    w.handles.insert((EE_COMBAT_H_COMPLEX, TARGET), COMPLEX);
    w.set(EE_COMBAT_Q_CLASS_FLAGS, UNIT, EE_COMBAT_CLASS_MELEE);
    w.real(EE_COMBAT_Q_INNER_SPHERE, UNIT, 1.0).real(EE_COMBAT_Q_INNER_SPHERE, TARGET, 1.0);
    w.real(EE_COMBAT_Q_HIT_DISTANCE_SQUARED, TARGET, 100.0);
    // out of contact, no wall spot: the point back as the host left it, then a new target near the old one
    let mut out = Ge3DPoint { x: 5.0, y: 6.0, z: 7.0 };
    firing_position(&mut w, UNIT, TARGET, &mut out);
    assert_eq!(out, Ge3DPoint { x: 5.0, y: 6.0, z: 7.0 });
    assert_eq!(w.ops(), [EE_COMBAT_W_CAPTURE, EE_COMBAT_W_WALL_SPOT, EE_COMBAT_W_NEW_TARGET_NEAR_OLD]);
    assert_eq!(w.log[2].2, COMPLEX);
    // a found wall spot: no new target
    w.log.clear();
    w.answers.insert(EE_COMBAT_W_WALL_SPOT, 1);
    firing_position(&mut w, UNIT, TARGET, &mut out);
    assert_eq!(w.ops(), [EE_COMBAT_W_CAPTURE, EE_COMBAT_W_WALL_SPOT]);
    // the Roman legionnaire's special attack is no melee: it goes on to the range test
    w.log.clear();
    w.set(EE_COMBAT_Q_UNIT_TYPE, UNIT, TYPE_ROMAN_LEGIONNAIRE).set(EE_COMBAT_Q_SPECIAL_ATTACK, UNIT, 1);
    firing_position(&mut w, UNIT, TARGET, &mut out);
    assert_eq!(w.ops()[..2], [EE_COMBAT_W_CAPTURE, EE_COMBAT_W_FIRING_RING]);
}

#[test]
fn firing_position_ring_at_half_the_range_then_the_target() {
    const COMPLEX: u64 = 0x30;
    let mut w = World::default();
    w.real(EE_COMBAT_Q_RANGE, UNIT, 7.9).set(EE_COMBAT_Q_TILE_SIZE, TARGET, 2);
    w.real(EE_COMBAT_Q_HIT_DISTANCE_SQUARED, TARGET, 1000.0);
    w.set(EE_COMBAT_Q_TILE_X, TARGET, 12).set(EE_COMBAT_Q_TILE_Y, TARGET, 20);
    // no ring tile, no complex unit: the tile's centre, z as it was
    let mut out = Ge3DPoint { x: 0.0, y: 0.0, z: 4.0 };
    firing_position(&mut w, UNIT, TARGET, &mut out);
    let ring = w.log.iter().find(|l| l.0 == EE_COMBAT_W_FIRING_RING).unwrap();
    // (7 - 2) / 2
    assert_eq!(ring.3[0], 2);
    assert_eq!(out, Ge3DPoint { x: 12.5, y: 20.5, z: 4.0 });
    // a complex target: its position
    w.handles.insert((EE_COMBAT_H_COMPLEX, TARGET), COMPLEX);
    w.locations.insert(COMPLEX, [3.0, 4.0, 5.0]);
    firing_position(&mut w, UNIT, TARGET, &mut out);
    assert_eq!(out, Ge3DPoint { x: 3.0, y: 4.0, z: 5.0 });
    // a range under the tile size: 0
    w.log.clear();
    w.real(EE_COMBAT_Q_RANGE, UNIT, 1.0);
    w.answers.insert(EE_COMBAT_W_FIRING_RING, 1);
    let before = out;
    firing_position(&mut w, UNIT, TARGET, &mut out);
    assert_eq!(w.log.iter().find(|l| l.0 == EE_COMBAT_W_FIRING_RING).unwrap().3[0], 0);
    assert_eq!(out, before);
}

#[test]
fn seen_by_skips_without_a_map_and_hides_cloaked_enemies() {
    const COMPLEX: u64 = 0x30;
    const PLAYER: u64 = 0x100;
    const OWNER: u64 = 0x200;
    // no map (or a player without visibility): seen, nothing else asked
    let mut w = World::default();
    assert!(is_seen_by(&mut w, TARGET, PLAYER, true));
    assert_eq!(w.ops(), [EE_COMBAT_W_SEES_MAP]);
    // no complex unit (a simple unit): its tile
    w.answers.insert(EE_COMBAT_W_SEES_MAP, 1);
    w.answers.insert(EE_COMBAT_W_TILE_SEEN, 1);
    assert!(is_seen_by(&mut w, TARGET, PLAYER, true));
    // a cloaked enemy: never; a cloaked ally: its tile
    w.handles.insert((EE_COMBAT_H_COMPLEX, TARGET), COMPLEX);
    w.handles.insert((EE_COMBAT_H_PLAYER, TARGET), OWNER);
    w.set(EE_COMBAT_Q_CLOAKABLE, COMPLEX, 1).set(EE_COMBAT_Q_CLOAKED, COMPLEX, 1);
    w.set(EE_COMBAT_Q_PLAYER_ID, OWNER, 3).set(EE_COMBAT_Q_ENEMY, PLAYER, 1);
    assert!(!is_seen_by(&mut w, TARGET, PLAYER, true));
    w.set(EE_COMBAT_Q_ENEMY, PLAYER, 0);
    w.set(EE_COMBAT_Q_TILE_X, TARGET, 7).set(EE_COMBAT_Q_TILE_Y, TARGET, 9);
    w.log.clear();
    assert!(is_seen_by(&mut w, TARGET, PLAYER, false));
    let seen = w.log.iter().find(|l| l.0 == EE_COMBAT_W_TILE_SEEN).unwrap();
    assert_eq!((seen.1, seen.3[..4].to_vec()), (PLAYER, vec![7, 9, 0, 1]));
    w.answers.insert(EE_COMBAT_W_TILE_SEEN, 0);
    assert!(!is_seen_by(&mut w, TARGET, PLAYER, false));
}

#[test]
fn seen_by_walks_a_large_units_footprint_until_a_tile_is_seen() {
    const COMPLEX: u64 = 0x30;
    const PLAYER: u64 = 0x100;
    let mut w = World::default();
    w.answers.insert(EE_COMBAT_W_SEES_MAP, 1);
    w.handles.insert((EE_COMBAT_H_COMPLEX, TARGET), COMPLEX);
    w.set(EE_COMBAT_Q_TILE_SIZE, TARGET, 2);
    w.answers.insert(EE_COMBAT_W_FOOTPRINT, 4);
    w.answers.insert(EE_COMBAT_W_TILE_SEEN, 1);
    assert!(is_seen_by(&mut w, TARGET, PLAYER, true));
    // the first tile seen ends the walk
    assert_eq!(
        w.ops(),
        [EE_COMBAT_W_SEES_MAP, EE_COMBAT_W_FOOTPRINT, EE_COMBAT_W_FOOTPRINT_TILE, EE_COMBAT_W_TILE_SEEN]
    );
    assert_eq!(w.log[3].3[2..4], [1, 0]);
    // none seen: every tile asked; a gate's footprint too
    w.log.clear();
    w.answers.insert(EE_COMBAT_W_TILE_SEEN, 0);
    w.set(EE_COMBAT_Q_TILE_SIZE, TARGET, 1).set(EE_COMBAT_Q_CATEGORY, TARGET, 0x19);
    assert!(!is_seen_by(&mut w, TARGET, PLAYER, true));
    assert_eq!(w.ops().iter().filter(|&&c| c == EE_COMBAT_W_TILE_SEEN).count(), 4);
}

const DROP_COMPLEX: u64 = 0x40;

fn dropping() -> World {
    let mut w = World::default();
    w.handles.insert((EE_COMBAT_H_COMPLEX, TARGET), DROP_COMPLEX);
    w.answers.insert(EE_COMBAT_W_DROP_MODEL, 1);
    w.points.insert(EE_COMBAT_W_DROP_MODEL, [1.0, 2.0, 4.0]);
    w.real(EE_COMBAT_Q_SCALE, DROP_COMPLEX, 2.0).set(EE_COMBAT_Q_BUILT, DROP_COMPLEX, 1);
    w.set(EE_COMBAT_Q_HIT_POINTS, TARGET, 50).set(EE_COMBAT_Q_MAX_HIT_POINTS, TARGET, 100);
    w.set(EE_COMBAT_Q_ID, TARGET, 77);
    w.locations.insert(DROP_COMPLEX, [10.0, 20.0, 30.0]);
    w
}

fn drop_record(w: &World) -> (EeCombatDropIn, [f32; 3], i32) {
    let (tag, bytes) = w.captures.iter().find(|c| c.0 == u32::from_le_bytes(*b"CBDP")).unwrap();
    assert_eq!(*tag, u32::from_le_bytes(*b"CBDP"));
    let n = std::mem::size_of::<EeCombatDropIn>();
    assert_eq!(bytes.len(), n + 16);
    // SAFETY: the record's first bytes are the input record
    let input = unsafe { std::ptr::read_unaligned(bytes.as_ptr() as *const EeCombatDropIn) };
    let f = |at: usize| f32::from_le_bytes(bytes[at..at + 4].try_into().unwrap());
    (input, [f(n), f(n + 4), f(n + 8)], i32::from_le_bytes(bytes[n + 12..n + 16].try_into().unwrap()))
}

#[test]
fn the_drop_location_is_the_models_drop_point_through_its_location() {
    let mut w = dropping();
    let (target, p) = find_drop_location(&mut w, UNIT, TARGET);
    let expected = drop_point(&EeCombatDropIn {
        kind: DROP_KIND_MODEL,
        offset: [1.0, 2.0, 4.0],
        scale: 2.0,
        built: 1,
        hit_points: 50,
        max_hit_points: 100,
        transformation: w.location(DROP_COMPLEX).m,
        ..Default::default()
    });
    assert_eq!(target, TARGET);
    assert_eq!((p.x, p.y, p.z), (expected.x, expected.y, expected.z));
    let (input, out, id) = drop_record(&w);
    assert_eq!((input.kind, input.scale, input.built, input.hit_points), (DROP_KIND_MODEL, 2.0, 1, 50));
    assert_eq!((out, id), ([p.x, p.y, p.z], 77));
    // an unbuilt unit's point is lowered by its health
    let mut w = dropping();
    w.set(EE_COMBAT_Q_BUILT, DROP_COMPLEX, 0);
    let (_, q) = find_drop_location(&mut w, UNIT, TARGET);
    assert_eq!(drop_record(&w).0.built, 0);
    assert!(q.z != p.z);
}

#[test]
fn the_drop_location_falls_back_to_the_tile_then_to_none() {
    // no model: the tile's centre at its height
    let mut w = dropping();
    w.answers.insert(EE_COMBAT_W_DROP_MODEL, 0);
    w.answers.insert(EE_COMBAT_W_TILE_HEIGHT, 1);
    w.points.insert(EE_COMBAT_W_TILE_HEIGHT, [3.5, 0.0, 0.0]);
    w.set(EE_COMBAT_Q_TILE_X, TARGET, 4).set(EE_COMBAT_Q_TILE_Y, TARGET, 6);
    let (target, p) = find_drop_location(&mut w, UNIT, TARGET);
    assert_eq!((target, p.x, p.y, p.z), (TARGET, 4.5, 6.5, 3.5));
    assert_eq!(drop_record(&w).0.kind, DROP_KIND_TILE);
    // no complex unit: the model is never asked
    let mut w = dropping();
    w.handles.clear();
    w.answers.insert(EE_COMBAT_W_TILE_HEIGHT, 1);
    find_drop_location(&mut w, UNIT, TARGET);
    assert!(!w.ops().contains(&EE_COMBAT_W_DROP_MODEL));
    // neither: no target, the point 0, the record's id -1
    let mut w = dropping();
    w.answers.insert(EE_COMBAT_W_DROP_MODEL, 0);
    let (target, p) = find_drop_location(&mut w, UNIT, TARGET);
    assert_eq!((target, p.x, p.y, p.z), (0, 0.0, 0.0, 0.0));
    let (input, out, id) = drop_record(&w);
    assert_eq!((input.kind, out, id), (DROP_KIND_NONE, [0.0; 3], -1));
}

#[test]
fn the_drop_location_without_a_target_asks_the_active_goal() {
    // an attack goal: its target
    let mut w = dropping();
    w.set(EE_COMBAT_Q_ACTIVE_GOAL_KIND, UNIT, 2);
    w.handles.insert((EE_COMBAT_H_ACTIVE_GOAL_TARGET, UNIT), TARGET);
    let (target, _) = find_drop_location(&mut w, UNIT, 0);
    assert_eq!(target, TARGET);
    assert!(!w.ops().contains(&EE_COMBAT_W_GOAL_LOCATION));
    // another goal: its location, no target, a record of kind 0 and id -1
    let mut w = dropping();
    w.set(EE_COMBAT_Q_ACTIVE_GOAL_KIND, UNIT, 1);
    w.points.insert(EE_COMBAT_W_GOAL_LOCATION, [5.0, 6.0, 7.0]);
    let (target, p) = find_drop_location(&mut w, UNIT, 0);
    assert_eq!((target, p.x, p.y, p.z), (0, 5.0, 6.0, 7.0));
    assert_eq!(w.ops(), [EE_COMBAT_W_GOAL_LOCATION, EE_COMBAT_W_CAPTURE]);
    let (input, out, id) = drop_record(&w);
    assert_eq!((input, out, id), (EeCombatDropIn::default(), [5.0, 6.0, 7.0], -1));
    // a given target: the goal is not read
    let mut w = dropping();
    w.set(EE_COMBAT_Q_ACTIVE_GOAL_KIND, UNIT, 1);
    assert_eq!(find_drop_location(&mut w, UNIT, TARGET).0, TARGET);
}

#[test]
fn the_hit_distance_measures_to_the_drop_point_or_a_melee_targets_position() {
    // ranged: from the unit's position to the drop point, on the ground; CBDP then CBHD
    let mut w = dropping();
    w.locations.insert(UNIT, [0.0, 0.0, 9.0]);
    let (_, p) = find_drop_location(&mut w, UNIT, TARGET);
    w.captures.clear();
    let d = hit_distance_squared(&mut w, UNIT, TARGET, -10000.0, -10000.0);
    let unit = translation(&w.location(UNIT));
    let input = EeCombatHitDistanceIn { from: [unit.x, unit.y, unit.z], to: [p.x, p.y, p.z], x: -10000.0, y: -10000.0 };
    assert_eq!(d, screen_hit_distance(&input));
    let tags: Vec<u32> = w.captures.iter().map(|c| c.0).collect();
    assert_eq!(tags, [u32::from_le_bytes(*b"CBDP"), u32::from_le_bytes(*b"CBHD")]);
    // melee: the target's position (the virtual), no drop record; a given point replaces the unit's
    let mut w = dropping();
    w.set(EE_COMBAT_Q_CLASS_FLAGS, UNIT, EE_COMBAT_CLASS_MELEE);
    w.points.insert(EE_COMBAT_W_POSITION, [3.0, 4.0, 100.0]);
    assert_eq!(hit_distance_squared(&mut w, UNIT, TARGET, -10000.0, -10000.0), 25.0);
    assert_eq!(hit_distance_squared(&mut w, UNIT, TARGET, 3.0, 0.0), 16.0);
    assert!(w.captures.iter().all(|c| c.0 == u32::from_le_bytes(*b"CBHD")));
}
