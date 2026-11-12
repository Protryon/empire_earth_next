use super::*;
use crate::reads::{EE_COMBAT_CLASS_PROJECTILE, EE_COMBAT_Q_ENEMY};
use ee_ge::types::{Ge3DLine, GeTransformation};
use std::collections::HashMap;

/// A world of plain values: reads by (code, object) (0 when unset), handles by (code, object), the ignored lists as
/// vectors, every operation logged with its answer from `answers` (0 when unset; an argument block out from `outs`).
#[derive(Default)]
struct World {
    reads: HashMap<(i32, u64), u32>,
    handles: HashMap<(i32, u64), u64>,
    ignored: HashMap<u64, Vec<Ignored>>,
    answers: HashMap<(i32, u64, u64), u64>,
    outs: HashMap<(i32, u64, u64), EeCombatWorldArgs>,
    footprint: (i32, i32),
    log: Vec<(i32, u64, u64, EeCombatWorldArgs)>,
    acts: Vec<(i32, u64, i32)>,
}

impl World {
    fn set(&mut self, code: i32, object: u64, value: u32) -> &mut Self {
        self.reads.insert((code, object), value);
        self
    }
    fn real(&mut self, code: i32, object: u64, value: f32) -> &mut Self {
        self.set(code, object, value.to_bits())
    }
    fn answer(&mut self, code: i32, object: u64, other: u64, value: u64) -> &mut Self {
        self.answers.insert((code, object, other), value);
        self
    }
    fn tile(&mut self, object: u64, x: i32, y: i32) -> &mut Self {
        self.set(EE_COMBAT_Q_TILE_X, object, x as u32).set(EE_COMBAT_Q_TILE_Y, object, y as u32)
    }
    /// the codes of the logged operations
    fn ops(&self) -> Vec<i32> {
        self.log.iter().map(|l| l.0).collect()
    }
}

impl QueryHost for World {
    fn read(&mut self, code: i32, object: EeCombatHandle, _arg: i32) -> u32 {
        if code == EE_COMBAT_Q_IGNORED_COUNT {
            return self.ignored.get(&object).map_or(0, |l| l.len() as u32);
        }
        if code == EE_COMBAT_Q_ID && !self.reads.contains_key(&(code, object)) {
            return object as u32;
        }
        self.reads.get(&(code, object)).copied().unwrap_or(0)
    }
    fn handle(&mut self, code: i32, object: EeCombatHandle, _arg: i32) -> EeCombatHandle {
        self.handles.get(&(code, object)).copied().unwrap_or(0)
    }
    fn act(&mut self, code: i32, object: EeCombatHandle, arg: i32) {
        self.acts.push((code, object, arg));
    }
    fn location(&mut self, _object: EeCombatHandle) -> GeTransformation {
        GeTransformation::IDENTITY
    }
    fn intersects(&mut self, _: EeCombatHandle, _: &Ge3DLine, _: bool, _: u32, _: f32) -> bool {
        false
    }
    fn footprint_tile(&mut self, _object: EeCombatHandle) -> (i32, i32) {
        self.footprint
    }
}

impl WorldHost for World {
    fn op(&mut self, code: i32, object: EeCombatHandle, other: EeCombatHandle, args: &mut EeCombatWorldArgs) -> u64 {
        self.log.push((code, object, other, *args));
        let list = self.ignored.entry(object).or_default();
        match code {
            EE_COMBAT_W_IGNORED_ENTRY => {
                let e = list[args[0] as usize];
                *args = [0; 12];
                args[..6].copy_from_slice(&[
                    i64::from(e.tile.0),
                    i64::from(e.tile.1),
                    i64::from(e.from.0),
                    i64::from(e.from.1),
                    i64::from(e.unit_id),
                    i64::from(e.time),
                ]);
                return 0;
            }
            EE_COMBAT_W_IGNORED_ERASE => {
                let to = if args[1] < 0 { list.len() } else { args[1] as usize };
                list.drain(args[0] as usize..to);
                return 0;
            }
            EE_COMBAT_W_IGNORED_PUSH => {
                list.insert(
                    0,
                    Ignored {
                        tile: (args[0] as i32, args[1] as i32),
                        from: (args[2] as i32, args[3] as i32),
                        unit_id: args[4] as u32,
                        time: args[5] as u32,
                    },
                );
                return 0;
            }
            EE_COMBAT_W_IGNORED_TIME => {
                list[args[0] as usize].time = args[1] as u32;
                return 0;
            }
            _ => {}
        }
        if let Some(out) = self.outs.get(&(code, object, other)) {
            *args = *out;
        }
        self.answers.get(&(code, object, other)).copied().unwrap_or(0)
    }
}

const UNIT: u64 = 0x100;
const TARGET: u64 = 0x200;

fn entry(tile: (i32, i32), from: (i32, i32), unit_id: u32, time: u32) -> Ignored {
    Ignored { tile, from, unit_id, time }
}

// ---- the ignored targets

#[test]
fn an_unlisted_target_is_not_ignored_and_reads_no_tile() {
    let mut w = World::default();
    w.ignored.insert(UNIT, vec![entry((1, 1), (2, 2), 7, 900)]);
    assert!(!is_target_ignored(&mut w, UNIT, TARGET, true));
    assert_eq!(w.ignored[&UNIT].len(), 1);
    assert_eq!(w.ops(), vec![EE_COMBAT_W_IGNORED_ENTRY]);
}

#[test]
fn an_expired_entry_goes_with_every_one_behind_it() {
    let mut w = World::default();
    let id = TARGET as u32;
    w.ignored.insert(
        UNIT,
        vec![entry((0, 0), (0, 0), 5, 9000), entry((1, 1), (2, 2), id, 1000), entry((0, 0), (0, 0), 6, 50)],
    );
    w.set(EE_COMBAT_Q_GAME_TIME, 0, 1000);
    assert!(!is_target_ignored(&mut w, UNIT, TARGET, false));
    assert_eq!(w.ignored[&UNIT], vec![entry((0, 0), (0, 0), 5, 9000)]);
}

#[test]
fn with_the_location_only_the_same_tiles_count_else_the_entry_goes() {
    let id = TARGET as u32;
    let mut w = World::default();
    w.ignored.insert(UNIT, vec![entry((3, 4), (5, 6), id, 2000), entry((0, 0), (0, 0), 9, 2000)]);
    w.set(EE_COMBAT_Q_GAME_TIME, 0, 1999).tile(UNIT, 3, 4).tile(TARGET, 5, 6);
    assert!(is_target_ignored(&mut w, UNIT, TARGET, true));
    assert_eq!(w.ignored[&UNIT].len(), 2);
    // the target moved: this entry alone goes
    w.tile(TARGET, 5, 7);
    assert!(!is_target_ignored(&mut w, UNIT, TARGET, true));
    assert_eq!(w.ignored[&UNIT], vec![entry((0, 0), (0, 0), 9, 2000)]);
    // without the location the tiles do not matter
    w.ignored.insert(UNIT, vec![entry((3, 4), (5, 6), id, 2000)]);
    assert!(is_target_ignored(&mut w, UNIT, TARGET, false));
}

#[test]
fn the_time_compares_unsigned() {
    let id = TARGET as u32;
    let mut w = World::default();
    w.ignored.insert(UNIT, vec![entry((0, 0), (0, 0), id, 0x8000_0000)]);
    w.set(EE_COMBAT_Q_GAME_TIME, 0, 5);
    assert!(is_target_ignored(&mut w, UNIT, TARGET, false));
}

#[test]
fn ignoring_pushes_a_new_entry_at_the_front_or_renews_the_old_one() {
    let id = TARGET as u32;
    let mut w = World::default();
    w.ignored.insert(UNIT, vec![entry((0, 0), (0, 0), 9, 10)]);
    w.set(EE_COMBAT_Q_GAME_TIME, 0, 4000).tile(UNIT, 3, 4).tile(TARGET, -1, 6);
    ignore_target(&mut w, UNIT, TARGET);
    assert_eq!(w.ignored[&UNIT], vec![entry((3, 4), (-1, 6), id, 34000), entry((0, 0), (0, 0), 9, 10)]);
    w.set(EE_COMBAT_Q_GAME_TIME, 0, 5000).tile(UNIT, 8, 8);
    ignore_target(&mut w, UNIT, TARGET);
    assert_eq!(w.ignored[&UNIT][0], entry((3, 4), (-1, 6), id, 35000));
    assert_eq!(w.ignored[&UNIT].len(), 2);
}

// ---- reach

#[test]
fn reach_tries_the_direct_test_the_link_then_the_connection_with_the_target() {
    let (from, to) = (0x10, 0x20);
    let mut w = World::default();
    w.answer(EE_COMBAT_W_CAN_GET_TO_UNIT, UNIT, TARGET, 1);
    assert!(can_reach_unit(&mut w, UNIT, TARGET));
    assert_eq!(w.ops(), vec![EE_COMBAT_W_CAN_GET_TO_UNIT]);

    let mut w = World::default();
    w.answer(EE_COMBAT_W_CONTINENT_AT, UNIT, 0, from);
    assert!(!can_reach_unit(&mut w, UNIT, TARGET));
    assert_eq!(w.ops(), vec![EE_COMBAT_W_CAN_GET_TO_UNIT, EE_COMBAT_W_CONTINENT_AT, EE_COMBAT_W_CONTINENT_AT]);

    w.answer(EE_COMBAT_W_CONTINENT_AT, TARGET, 0, to).answer(EE_COMBAT_W_CONTINENT_LINK, from, to, 1);
    assert!(can_reach_unit(&mut w, UNIT, TARGET));

    w.answer(EE_COMBAT_W_CONTINENT_LINK, from, to, 0).answer(EE_COMBAT_W_CONNECTED_FOR_UNIT, from, to, 1);
    w.log.clear();
    assert!(can_reach_unit(&mut w, UNIT, TARGET));
    let last = w.log.last().unwrap();
    assert_eq!((last.0, last.1, last.2, last.3[0]), (EE_COMBAT_W_CONNECTED_FOR_UNIT, from, to, TARGET as i64));
}

fn bordering(unit: (i32, i32), target: (i32, i32), size: u32, range: f32, blocked: &[usize]) -> (bool, World) {
    let mut w = World::default();
    w.tile(UNIT, unit.0, unit.1).tile(TARGET, target.0, target.1).set(EE_COMBAT_Q_TILE_SIZE, TARGET, size);
    w.real(EE_COMBAT_Q_RANGE, UNIT, range);
    // the corners' blocked answers by their order (the mock answers by the argument block's first two values)
    let r = {
        struct Corners<'a> {
            w: &'a mut World,
            blocked: Vec<usize>,
            seen: usize,
        }
        impl QueryHost for Corners<'_> {
            fn read(&mut self, c: i32, o: u64, a: i32) -> u32 {
                self.w.read(c, o, a)
            }
            fn handle(&mut self, c: i32, o: u64, a: i32) -> u64 {
                self.w.handle(c, o, a)
            }
            fn act(&mut self, c: i32, o: u64, a: i32) {
                self.w.act(c, o, a)
            }
            fn location(&mut self, o: u64) -> GeTransformation {
                self.w.location(o)
            }
            fn intersects(&mut self, c: u64, l: &Ge3DLine, s: bool, lod: u32, sc: f32) -> bool {
                self.w.intersects(c, l, s, lod, sc)
            }
            fn footprint_tile(&mut self, o: u64) -> (i32, i32) {
                self.w.footprint_tile(o)
            }
        }
        impl WorldHost for Corners<'_> {
            fn op(&mut self, code: i32, o: u64, other: u64, args: &mut EeCombatWorldArgs) -> u64 {
                self.w.op(code, o, other, args);
                let i = self.seen;
                self.seen += 1;
                u64::from(self.blocked.contains(&i))
            }
        }
        let mut c = Corners { w: &mut w, blocked: blocked.to_vec(), seen: 0 };
        on_tile_bordering(&mut c, UNIT, TARGET)
    };
    (r, w)
}

#[test]
fn bordering_needs_range_one_and_counts_the_own_tile() {
    assert!(!bordering((5, 5), (5, 5), 1, 1.5, &[]).0);
    let (r, w) = bordering((5, 5), (5, 5), 1, 1.0, &[]);
    assert!(r);
    assert!(w.log.is_empty());
}

#[test]
fn bordering_takes_the_side_tiles_without_a_corner_test() {
    // a 2x2 target at (10, 20): the sides are x 9 and 12 for y 20..21, y 19 and 22 for x 10..11
    for side in [(9, 20), (12, 21), (10, 19), (11, 22)] {
        let (r, w) = bordering(side, (10, 20), 2, 1.0, &[]);
        assert!(r, "{side:?}");
        assert!(w.log.is_empty());
    }
    // the tiles inside the footprint (not the corner tile) are no bordering tile
    assert!(!bordering((11, 21), (10, 20), 2, 1.0, &[]).0);
}

#[test]
fn bordering_corners_are_tested_in_order_and_a_blocked_one_does_not_count() {
    let (r, w) = bordering((9, 19), (10, 20), 2, 1.0, &[]);
    assert!(r);
    let args: Vec<[i64; 4]> = w.log.iter().map(|l| [l.3[0], l.3[1], l.3[2], l.3[3]]).collect();
    // the second test passes `inHeight + 1` (3) as its last y, as the original
    assert_eq!(args, vec![[9, 20, 10, 19], [9, 21, 10, 3], [12, 20, 11, 19], [11, 22, 12, 21]]);
    assert!(!bordering((9, 19), (10, 20), 2, 1.0, &[0]).0);
    assert!(bordering((12, 22), (10, 20), 2, 1.0, &[0, 1, 2]).0);
    assert!(!bordering((12, 22), (10, 20), 2, 1.0, &[3]).0);
    assert!(!bordering((13, 22), (10, 20), 2, 1.0, &[]).0);
}

fn reachable_world() -> World {
    let mut w = World::default();
    w.tile(UNIT, 10, 10).tile(TARGET, 20, 14).set(EE_COMBAT_Q_TILE_SIZE, TARGET, 1);
    w.real(EE_COMBAT_Q_RANGE, UNIT, 3.0).real(EE_COMBAT_Q_CLASS_MAX_VELOCITY, UNIT, 2.0);
    w.set(EE_COMBAT_Q_DOMAINS, UNIT, 2).real(EE_COMBAT_Q_DISTANCE_SQUARED, TARGET, 116.0);
    w.real(EE_COMBAT_Q_LINE_OF_SIGHT, UNIT, 6.2).set(EE_COMBAT_Q_MAP_SIZE, 0, 25);
    w
}

#[test]
fn an_attack_off_the_map_or_on_another_continent_leaves_the_tile() {
    let mut w = reachable_world();
    w.tile(TARGET, -10000, 3);
    assert_eq!(attack_reachable(&mut w, UNIT, TARGET), (false, None));
    let mut w = reachable_world();
    w.answer(EE_COMBAT_W_OTHER_CONTINENT, UNIT, TARGET, 1);
    assert_eq!(attack_reachable(&mut w, UNIT, TARGET), (false, None));
}

#[test]
fn a_unit_that_does_not_move_or_is_close_reaches_at_once() {
    let mut w = reachable_world();
    w.real(EE_COMBAT_Q_CLASS_MAX_VELOCITY, UNIT, 0.0);
    assert_eq!(attack_reachable(&mut w, UNIT, TARGET), (true, Some((0, 0))));
    let mut w = reachable_world();
    w.set(EE_COMBAT_Q_DOMAINS, UNIT, 1);
    assert!(attack_reachable(&mut w, UNIT, TARGET).0);
    let mut w = reachable_world();
    w.real(EE_COMBAT_Q_DISTANCE_SQUARED, TARGET, 9.0);
    assert!(attack_reachable(&mut w, UNIT, TARGET).0);
    // a range below the square root of 2 counts as 2
    let mut w = reachable_world();
    w.real(EE_COMBAT_Q_RANGE, UNIT, 0.5).real(EE_COMBAT_Q_DISTANCE_SQUARED, TARGET, 2.0);
    assert!(attack_reachable(&mut w, UNIT, TARGET).0);
    assert!(w.log.iter().all(|l| l.0 != EE_COMBAT_W_FIND_PATH));
}

#[test]
fn a_contained_unit_or_target_searches_no_path() {
    let mut w = reachable_world();
    w.set(EE_COMBAT_Q_CONTAINER, TARGET, 1);
    assert_eq!(attack_reachable(&mut w, UNIT, TARGET), (false, Some((0, 0))));
    assert!(w.log.iter().all(|l| l.0 != EE_COMBAT_W_FIND_PATH));
}

#[test]
fn the_path_window_is_the_two_tiles_grown_by_twice_the_sight() {
    let mut w = reachable_world();
    let mut out = [0; 12];
    out[0] = 18;
    out[1] = 13;
    w.outs.insert((EE_COMBAT_W_FIND_PATH, UNIT, TARGET), out);
    w.answer(EE_COMBAT_W_FIND_PATH, UNIT, TARGET, 4);
    // the first tile (18, 13) is 5 from (20, 14): within the range squared 9
    assert_eq!(attack_reachable(&mut w, UNIT, TARGET), (true, Some((18, 13))));
    let path = w.log.iter().find(|l| l.0 == EE_COMBAT_W_FIND_PATH).unwrap();
    // sight 6.2 > 9? no: the range squared 9 is the radius (ceil 9), margin 18; bounds top 10-18 -> 0, left 0,
    // bottom min(14 + 18, 24), right min(20 + 18, 24); the limit max(116, 18) * 2 capped at 35
    assert_eq!(&path.3[..9], &[10, 10, 20, 14, 0, 0, 24, 24, 35]);
    // the first tile out of range
    out[0] = 10;
    w.outs.insert((EE_COMBAT_W_FIND_PATH, UNIT, TARGET), out);
    assert_eq!(attack_reachable(&mut w, UNIT, TARGET), (false, Some((10, 13))));
    // no path: the tile stays (0, 0)
    w.answer(EE_COMBAT_W_FIND_PATH, UNIT, TARGET, 0);
    assert_eq!(attack_reachable(&mut w, UNIT, TARGET), (false, Some((0, 0))));
}

#[test]
fn the_sight_sets_the_margin_when_beyond_the_range_and_a_big_target_its_nearest_tile() {
    let mut w = reachable_world();
    w.real(EE_COMBAT_Q_RANGE, UNIT, 1.0).real(EE_COMBAT_Q_LINE_OF_SIGHT, UNIT, 2.2);
    w.set(EE_COMBAT_Q_TILE_SIZE, TARGET, 3);
    w.footprint = (19, 13);
    w.set(EE_COMBAT_Q_MAP_SIZE, 0, 200);
    attack_reachable(&mut w, UNIT, TARGET);
    let path = w.log.iter().find(|l| l.0 == EE_COMBAT_W_FIND_PATH).unwrap();
    // the range 1 counts 2; the sight 2.2 > 2: radius ceil(2.2) 3, margin 6; the limit 9*9+3*3 = 90 -> 35
    assert_eq!(&path.3[..9], &[10, 10, 19, 13, 4, 4, 19, 25, 35]);
}

#[test]
fn a_legionnaires_special_attack_reaches_by_its_heal_range() {
    let mut w = reachable_world();
    w.set(EE_COMBAT_Q_UNIT_TYPE, UNIT, W_TYPE_ROMAN_LEGIONNAIRE).set(EE_COMBAT_Q_SPECIAL_ATTACK, UNIT, 1);
    w.real(EE_COMBAT_Q_HEAL_RANGE, UNIT, 11.0);
    assert_eq!(attack_reachable(&mut w, UNIT, TARGET), (true, Some((0, 0))));
}

#[test]
fn the_short_limit_is_doubled() {
    let mut w = reachable_world();
    w.tile(TARGET, 12, 10).real(EE_COMBAT_Q_RANGE, UNIT, 1.0).real(EE_COMBAT_Q_LINE_OF_SIGHT, UNIT, 1.0);
    w.real(EE_COMBAT_Q_DISTANCE_SQUARED, TARGET, 4.0);
    attack_reachable(&mut w, UNIT, TARGET);
    let path = w.log.iter().find(|l| l.0 == EE_COMBAT_W_FIND_PATH).unwrap();
    // margin 2 * ceil(2) = 4, the squared distance 4: limit 4 * 2 = 8
    assert_eq!(path.3[8], 8);
}

// ---- Attacked

const PLAYER: u64 = 0x1;
const ENEMY: u64 = 0x2;
const GOAL_TARGET: u64 = 0x300;

fn attacked_world() -> World {
    let mut w = World::default();
    w.answer(EE_COMBAT_W_LAST_ATTACKER, UNIT, 0, TARGET).answer(EE_COMBAT_W_SEEN_BY_OWNER, TARGET, UNIT, 1);
    w.handles.insert((EE_COMBAT_H_PLAYER, UNIT), PLAYER);
    w.handles.insert((EE_COMBAT_H_PLAYER, TARGET), ENEMY);
    w.set(EE_COMBAT_Q_PLAYER_ID, PLAYER, 2).set(EE_COMBAT_Q_PLAYER_ID, ENEMY, 3);
    w
}

fn helper(w: &mut World, near: u64) {
    w.handles.insert((EE_COMBAT_H_PLAYER, near), PLAYER);
    w.set(EE_COMBAT_Q_HIT_POINTS, near, 10).set(EE_COMBAT_Q_TARGETS_ENEMIES, near, 1);
    w.set(EE_COMBAT_Q_MOVES_FREELY, near, 1).set(EE_COMBAT_Q_GOAL_KIND, near, EE_COMBAT_Q_NONE);
    w.answer(EE_COMBAT_W_CAN_ATTACK_CLASS, near, TARGET, 1);
}

#[test]
fn an_attacked_unit_attacks_a_seen_attacker_and_goes_to_an_unseen_one() {
    let mut w = attacked_world();
    w.set(EE_COMBAT_Q_PLAYER_ID, PLAYER, 0);
    attacked(&mut w);
    assert_eq!(
        w.ops(),
        vec![
            EE_COMBAT_W_LAST_ATTACKER,
            EE_COMBAT_W_SEEN_BY_OWNER,
            EE_COMBAT_W_ATTACKED_GOAL,
            EE_COMBAT_W_RELEASE_ATTACKER
        ]
    );
    assert_eq!(w.log[2].3[0], 0);
    let mut w = attacked_world();
    w.set(EE_COMBAT_Q_PLAYER_ID, PLAYER, 0).answer(EE_COMBAT_W_SEEN_BY_OWNER, TARGET, UNIT, 0);
    attacked(&mut w);
    assert_eq!(w.log[2].3[0], 0, "an unseen attacker of a unit that does not move: attacked");
    w.real(EE_COMBAT_Q_STATS_MAX_VELOCITY, UNIT, 1.5);
    w.log.clear();
    attacked(&mut w);
    assert_eq!(w.log[2].3[0], 1);
    let (goal, unit, target) = (w.log[2].0, w.log[2].1, w.log[2].2);
    assert_eq!((goal, unit, target), (EE_COMBAT_W_ATTACKED_GOAL, UNIT, TARGET));
}

fn attacked(w: &mut World) {
    super::attacked(w, UNIT);
}

#[test]
fn a_citizen_calls_for_help_only_against_a_player_not_nature() {
    let mut w = attacked_world();
    w.set(EE_COMBAT_Q_CATEGORY, UNIT, W_CATEGORY_NAVAL_CITIZEN).set(EE_COMBAT_Q_PLAYER_ID, ENEMY, 0);
    attacked(&mut w);
    assert!(!w.ops().contains(&EE_COMBAT_W_UNITS_NEAR));
    w.set(EE_COMBAT_Q_PLAYER_ID, ENEMY, 3);
    attacked(&mut w);
    assert!(w.ops().contains(&EE_COMBAT_W_UNITS_NEAR));
}

#[test]
fn the_helpers_go_backwards_and_join_or_ignore_by_reach() {
    let (a, b, c) = (0x401, 0x402, 0x403);
    let mut w = attacked_world();
    w.answer(EE_COMBAT_W_UNITS_NEAR, UNIT, 0, 3);
    for near in [a, b, c] {
        helper(&mut w, near);
    }
    // the near units by index: the mock answers NEAR_UNIT by object 0 and other 0, so chain them through a log check
    struct Near<'a> {
        w: &'a mut World,
        units: [u64; 3],
    }
    impl QueryHost for Near<'_> {
        fn read(&mut self, c: i32, o: u64, a: i32) -> u32 {
            self.w.read(c, o, a)
        }
        fn handle(&mut self, c: i32, o: u64, a: i32) -> u64 {
            self.w.handle(c, o, a)
        }
        fn act(&mut self, c: i32, o: u64, a: i32) {
            self.w.act(c, o, a)
        }
        fn location(&mut self, o: u64) -> GeTransformation {
            self.w.location(o)
        }
        fn intersects(&mut self, c: u64, l: &Ge3DLine, s: bool, lod: u32, sc: f32) -> bool {
            self.w.intersects(c, l, s, lod, sc)
        }
        fn footprint_tile(&mut self, o: u64) -> (i32, i32) {
            self.w.footprint_tile(o)
        }
    }
    impl WorldHost for Near<'_> {
        fn op(&mut self, code: i32, o: u64, other: u64, args: &mut EeCombatWorldArgs) -> u64 {
            let index = args[0];
            let r = self.w.op(code, o, other, args);
            if code == EE_COMBAT_W_NEAR_UNIT {
                return self.units[index as usize];
            }
            r
        }
    }
    // b cannot reach: it ignores the attacker; c is ignoring it already; a joins
    w.answer(EE_COMBAT_W_ATTACK_REACHABLE, a, TARGET, 1).answer(EE_COMBAT_W_IS_IGNORED, c, TARGET, 1);
    let mut n = Near { w: &mut w, units: [a, b, c] };
    super::attacked(&mut n, UNIT);
    let done: Vec<(i32, u64)> = w
        .log
        .iter()
        .filter(|l| matches!(l.0, EE_COMBAT_W_NEAR_UNIT | EE_COMBAT_W_HELP | EE_COMBAT_W_IGNORE))
        .map(|l| (l.0, if l.0 == EE_COMBAT_W_NEAR_UNIT { l.3[0] as u64 } else { l.1 }))
        .collect();
    assert_eq!(
        done,
        vec![
            (EE_COMBAT_W_NEAR_UNIT, 2),
            (EE_COMBAT_W_NEAR_UNIT, 1),
            (EE_COMBAT_W_IGNORE, b),
            (EE_COMBAT_W_NEAR_UNIT, 0),
            (EE_COMBAT_W_HELP, a),
        ]
    );
    assert_eq!(*w.ops().last().unwrap(), EE_COMBAT_W_RELEASE_ATTACKER);
}

#[test]
fn a_helper_must_be_idle_or_besieging_an_unarmed_building() {
    let near = 0x401;
    let screen = |setup: &dyn Fn(&mut World)| {
        let mut w = attacked_world();
        helper(&mut w, near);
        w.answer(EE_COMBAT_W_ATTACK_REACHABLE, near, TARGET, 1);
        setup(&mut w);
        helps(&mut w, near, PLAYER, TARGET)
    };
    assert!(screen(&|_| {}));
    assert!(!screen(&|w| {
        w.set(EE_COMBAT_Q_HIT_POINTS, near, 0);
    }));
    assert!(!screen(&|w| {
        w.handles.insert((EE_COMBAT_H_PLAYER, near), ENEMY);
    }));
    assert!(!screen(&|w| {
        w.set(EE_COMBAT_Q_TARGETS_ENEMIES, near, 0);
    }));
    assert!(!screen(&|w| {
        w.set(EE_COMBAT_Q_MOVES_FREELY, near, 0);
    }));
    assert!(!screen(&|w| {
        w.set(EE_COMBAT_Q_CLASS_FLAGS, near, EE_COMBAT_CLASS_PROJECTILE);
    }));
    assert!(!screen(&|w| {
        w.set(EE_COMBAT_Q_CATEGORY, near, W_CATEGORY_LAND_CITIZEN);
    }));
    assert!(!screen(&|w| {
        w.answer(EE_COMBAT_W_CAN_ATTACK_CLASS, near, TARGET, 0);
    }));
    let goal = |b1c: u32, kind: u32, building: u32, attack: f32| {
        move |w: &mut World| {
            w.set(EE_COMBAT_Q_GOAL_KIND, near, kind).set(EE_COMBAT_Q_GOAL_B1C, near, b1c);
            w.handles.insert((EE_COMBAT_H_GOAL_TARGET, near), GOAL_TARGET);
            w.set(EE_COMBAT_Q_LAND_BUILDING, GOAL_TARGET, building).real(EE_COMBAT_Q_ATTACK, GOAL_TARGET, attack);
        }
    };
    assert!(screen(&goal(0, W_GOAL_ATTACK_UNIT, 1, 0.0)));
    assert!(!screen(&goal(1, W_GOAL_ATTACK_UNIT, 1, 0.0)));
    assert!(!screen(&goal(0, 3, 1, 0.0)));
    assert!(!screen(&goal(0, W_GOAL_ATTACK_UNIT, 0, 0.0)));
    assert!(!screen(&goal(0, W_GOAL_ATTACK_UNIT, 1, 4.0)));
}

// ---- FireShot

const DROP: u64 = 0x500;
const BLOCKER: u64 = 0x600;

fn shot_world(class: u32) -> World {
    let mut w = World::default();
    w.set(EE_COMBAT_Q_TAG_COUNT, UNIT, 3).set(EE_COMBAT_Q_SHOT_CLASS, 0, class);
    w.set(EE_COMBAT_Q_RELOAD_TIMER, UNIT, 100).answer(EE_COMBAT_W_FIND_DROP, UNIT, 0, DROP);
    w.real(EE_COMBAT_Q_RANGE, UNIT, 6.0);
    w
}

#[test]
fn a_missile_base_only_launches_its_missile() {
    let mut w = shot_world(0);
    w.set(EE_COMBAT_Q_UNIT_TYPE, UNIT, W_TYPE_CP_MISSILE_BASE);
    fire_shot(&mut w, UNIT);
    assert_eq!(w.ops(), vec![EE_COMBAT_W_LAUNCH_STORED_MISSILE]);
    assert!(w.acts.is_empty());
}

#[test]
fn the_reload_timer_is_drawn_when_run_out_and_half_again_when_firing_high_over_walls() {
    let mut w = shot_world(0);
    w.set(EE_COMBAT_Q_RELOAD_TIMER, UNIT, 0).set(EE_COMBAT_Q_RELOAD_TIME, UNIT, 2500);
    fire_shot(&mut w, UNIT);
    assert_eq!((w.log[0].0, w.log[0].3[0]), (EE_COMBAT_W_RELOAD, 2500));
    let mut w = shot_world(0);
    w.set(EE_COMBAT_Q_FIRE_OVER_WALLS, UNIT, 1).set(EE_COMBAT_Q_FIRING_HIGH, UNIT, 1).set(
        EE_COMBAT_Q_RELOAD_TIMER,
        UNIT,
        1001,
    );
    fire_shot(&mut w, UNIT);
    // lrintf(1001 * 1.5) = 1502 (1501.5 to even)
    assert_eq!((w.log[0].0, w.log[0].3[0]), (EE_COMBAT_W_RELOAD, 1502));
    // over walls the line is not tested
    assert!(!w.ops().contains(&EE_COMBAT_W_LINE_BLOCKED));
}

#[test]
fn a_direct_shot_hits_the_target_or_its_blocker() {
    let mut w = shot_world(0);
    fire_shot(&mut w, UNIT);
    assert_eq!(w.ops(), vec![EE_COMBAT_W_FIND_DROP, EE_COMBAT_W_LINE_BLOCKED, EE_COMBAT_W_APPLY_HIT]);
    assert_eq!(w.log[2].2, DROP);
    // the weapon tag point moves on (0 + 1) % 3
    assert_eq!(w.acts, vec![(EE_COMBAT_A_WEAPON_TAG, UNIT, 1)]);

    // an enemy blocker takes the hit
    let mut w = shot_world(0);
    let mut out = [0; 12];
    out[0] = BLOCKER as i64;
    w.outs.insert((EE_COMBAT_W_LINE_BLOCKED, UNIT, DROP), out);
    w.answer(EE_COMBAT_W_LINE_BLOCKED, UNIT, DROP, 1);
    w.handles.insert((EE_COMBAT_H_PLAYER, UNIT), PLAYER);
    w.handles.insert((EE_COMBAT_H_PLAYER, BLOCKER), ENEMY);
    w.set(EE_COMBAT_Q_PLAYER_ID, ENEMY, 3);
    w.reads.insert((EE_COMBAT_Q_ENEMY, PLAYER), 1);
    fire_shot(&mut w, UNIT);
    assert_eq!(w.log.last().map(|l| (l.0, l.2)), Some((EE_COMBAT_W_APPLY_HIT, BLOCKER)));

    // an allied blocker only sparks (when its class has sparks)
    w.reads.insert((EE_COMBAT_Q_ENEMY, PLAYER), 0);
    w.log.clear();
    fire_shot(&mut w, UNIT);
    assert!(!w.ops().contains(&EE_COMBAT_W_SPARKS_ON) && !w.ops().contains(&EE_COMBAT_W_APPLY_HIT));
    w.set(EE_COMBAT_Q_SPARKS_EFFECT, BLOCKER, 1);
    w.log.clear();
    fire_shot(&mut w, UNIT);
    assert_eq!(w.log.last().map(|l| (l.0, l.1)), Some((EE_COMBAT_W_SPARKS_ON, BLOCKER)));

    // open ground blocks: the sparks on its tile
    let mut w = shot_world(0);
    let mut out = [0; 12];
    out[1] = 7;
    out[2] = 9;
    w.outs.insert((EE_COMBAT_W_LINE_BLOCKED, UNIT, DROP), out);
    w.answer(EE_COMBAT_W_LINE_BLOCKED, UNIT, DROP, 1);
    fire_shot(&mut w, UNIT);
    let last = w.log.last().unwrap();
    assert_eq!((last.0, last.3[0], last.3[1]), (EE_COMBAT_W_SPARKS_AT, 7, 9));

    // a melee unit (range 1) or no target: no line test
    let mut w = shot_world(0);
    w.real(EE_COMBAT_Q_RANGE, UNIT, 1.0);
    fire_shot(&mut w, UNIT);
    assert_eq!(w.ops(), vec![EE_COMBAT_W_FIND_DROP, EE_COMBAT_W_APPLY_HIT]);
}

#[test]
fn a_projectile_class_launches_from_the_model_or_its_turrets() {
    let mut w = shot_world(EE_COMBAT_SHOT_CLASS_PROJECTILE);
    fire_shot(&mut w, UNIT);
    assert_eq!(w.ops(), vec![EE_COMBAT_W_FIND_DROP, EE_COMBAT_W_LAUNCH_SHOT]);
    w.set(EE_COMBAT_Q_TURRET_PARTS, UNIT, 2);
    w.log.clear();
    fire_shot(&mut w, UNIT);
    assert_eq!(w.ops(), vec![EE_COMBAT_W_FIND_DROP, EE_COMBAT_W_LAUNCH_FROM_TURRET]);
    // a legionnaire without its special attack hits directly
    w.set(EE_COMBAT_Q_UNIT_TYPE, UNIT, W_TYPE_ROMAN_LEGIONNAIRE);
    w.log.clear();
    fire_shot(&mut w, UNIT);
    assert_eq!(w.ops(), vec![EE_COMBAT_W_FIND_DROP, EE_COMBAT_W_APPLY_HIT]);
    // with it, it throws (and plays no shot effect)
    w.set(EE_COMBAT_Q_SPECIAL_ATTACK, UNIT, 1).set(
        EE_COMBAT_Q_SHOT_CLASS,
        0,
        EE_COMBAT_SHOT_CLASS_PROJECTILE | EE_COMBAT_SHOT_CLASS_SHOT_EFFECT,
    );
    w.log.clear();
    fire_shot(&mut w, UNIT);
    assert_eq!(w.ops(), vec![EE_COMBAT_W_FIND_DROP, EE_COMBAT_W_LAUNCH_SHOT]);
}

#[test]
fn a_capital_ships_special_attack_fires_from_its_last_tag_point_once() {
    let mut w = shot_world(EE_COMBAT_SHOT_CLASS_PROJECTILE);
    w.set(EE_COMBAT_Q_UNIT_TYPE, UNIT, W_TYPE_SP15_CAPITAL_SHIP).set(EE_COMBAT_Q_SPECIAL_ATTACK, UNIT, 1);
    w.answer(EE_COMBAT_W_CAN_ATTACK_CLASS, UNIT, DROP, 0);
    fire_shot(&mut w, UNIT);
    assert_eq!(
        w.ops(),
        vec![
            EE_COMBAT_W_FIND_DROP,
            EE_COMBAT_W_SPECIAL_EFFECT,
            EE_COMBAT_W_APPLY_HIT,
            EE_COMBAT_W_CAN_ATTACK_CLASS,
            EE_COMBAT_W_START_ACTION,
            EE_COMBAT_W_SPECIAL_OFF,
        ]
    );
    assert_eq!(w.log[4].3[0], W_ACTION_IDLE);
    // the last tag point (2) set; the mock keeps the read at 0, so the next is (0 + 1) % 3 = 1
    assert_eq!(w.acts, vec![(EE_COMBAT_A_WEAPON_TAG, UNIT, 2), (EE_COMBAT_A_WEAPON_TAG, UNIT, 1)]);
    // a capital ship skips its last (special) tag point in the turn
    let mut w = shot_world(EE_COMBAT_SHOT_CLASS_PROJECTILE);
    w.set(EE_COMBAT_Q_UNIT_TYPE, UNIT, W_TYPE_CAPITAL_SHIP_YAMATO).set(EE_COMBAT_Q_WEAPON_TAG, UNIT, 1);
    fire_shot(&mut w, UNIT);
    assert_eq!(w.ops(), vec![EE_COMBAT_W_FIND_DROP, EE_COMBAT_W_APPLY_HIT]);
    assert_eq!(w.acts, vec![(EE_COMBAT_A_WEAPON_TAG, UNIT, 0)]);
}

#[test]
fn the_effects_follow_the_tag_points_and_a_low_tech_citizen_plays_no_shot() {
    let all = EE_COMBAT_SHOT_CLASS_MUZZLE_FLASH | EE_COMBAT_SHOT_CLASS_SHOT_EFFECT;
    let mut w = shot_world(all);
    w.real(EE_COMBAT_Q_RANGE, UNIT, 1.0);
    fire_shot(&mut w, UNIT);
    assert_eq!(&w.ops()[2..], &[EE_COMBAT_W_MUZZLE_FLASH, EE_COMBAT_W_SHOT_EFFECT_BETWEEN]);
    w.answer(EE_COMBAT_W_FIND_DROP, UNIT, 0, 0);
    w.log.clear();
    fire_shot(&mut w, UNIT);
    assert_eq!(&w.ops()[2..], &[EE_COMBAT_W_MUZZLE_FLASH, EE_COMBAT_W_SHOT_EFFECT_AT]);
    w.set(EE_COMBAT_Q_CATEGORY, UNIT, W_CATEGORY_LAND_CITIZEN);
    w.log.clear();
    fire_shot(&mut w, UNIT);
    assert_eq!(&w.ops()[2..], &[EE_COMBAT_W_MUZZLE_FLASH]);
    w.set(EE_COMBAT_Q_HIGH_TECH_TOOLS, UNIT, 1);
    w.log.clear();
    fire_shot(&mut w, UNIT);
    assert_eq!(&w.ops()[2..], &[EE_COMBAT_W_MUZZLE_FLASH, EE_COMBAT_W_SHOT_EFFECT_AT]);
    // no tag points: no effect and the weapon tag point stays 0
    w.set(EE_COMBAT_Q_TAG_COUNT, UNIT, 0);
    w.log.clear();
    w.acts.clear();
    fire_shot(&mut w, UNIT);
    assert_eq!(w.ops(), vec![EE_COMBAT_W_FIND_DROP, EE_COMBAT_W_APPLY_HIT]);
    assert!(w.acts.is_empty());
}

#[test]
fn the_ammunition_runs_down_and_the_last_round_ends_the_flight() {
    let mut w = shot_world(EE_COMBAT_SHOT_CLASS_AMMUNITION);
    w.set(EE_COMBAT_Q_AMMUNITION, UNIT, 4);
    fire_shot(&mut w, UNIT);
    assert_eq!(w.log.last().map(|l| (l.0, l.3[0])), Some((EE_COMBAT_W_SET_AMMUNITION, 3)));
    w.set(EE_COMBAT_Q_AMMUNITION, UNIT, 1);
    w.log.clear();
    fire_shot(&mut w, UNIT);
    let tail: Vec<(i32, i64)> = w.log.iter().rev().take(2).map(|l| (l.0, l.3[0])).collect();
    assert_eq!(tail, vec![(EE_COMBAT_W_SET_FLIGHT_TIME, 0), (EE_COMBAT_W_SET_AMMUNITION, 0)]);
}

#[test]
fn the_world_entry_point_dispatches_and_writes_the_tile_only_when_set() {
    // the C ABI's dispatch over a table of extern functions answering from a thread-local world
    use std::cell::RefCell;
    thread_local!(static W: RefCell<World> = RefCell::new(World::default()));
    unsafe extern "C" fn read(_: *mut c_void, code: i32, object: u64, arg: i32) -> u32 {
        W.with(|w| QueryHost::read(&mut *w.borrow_mut(), code, object, arg))
    }
    unsafe extern "C" fn handle(_: *mut c_void, code: i32, object: u64, arg: i32) -> u64 {
        W.with(|w| QueryHost::handle(&mut *w.borrow_mut(), code, object, arg))
    }
    unsafe extern "C" fn act(_: *mut c_void, _: i32, _: u64, _: i32) {}
    unsafe extern "C" fn location(_: *mut c_void, _: u64, out: *mut [f32; 16]) {
        unsafe { *out = [0.0; 16] };
    }
    unsafe extern "C" fn intersects(_: *mut c_void, _: u64, _: *const [f32; 6], _: i32, _: u32, _: f32) -> i32 {
        0
    }
    unsafe extern "C" fn footprint(_: *mut c_void, _: u64, out: *mut [i32; 2]) {
        unsafe { *out = [0, 0] };
    }
    unsafe extern "C" fn op(_: *mut c_void, code: i32, object: u64, other: u64, args: *mut EeCombatWorldArgs) -> u64 {
        W.with(|w| WorldHost::op(&mut *w.borrow_mut(), code, object, other, unsafe { &mut *args }))
    }
    let host = EeCombatWorldHost {
        query: EeCombatQueryHost {
            ctx: std::ptr::null_mut(),
            read,
            handle,
            act,
            location,
            intersects,
            footprint_tile: footprint,
        },
        op,
    };
    W.with(|w| *w.borrow_mut() = reachable_world());
    let mut tile = [7, 7];
    assert_eq!(unsafe { ee_combat_world(EE_COMBAT_WORLD_ATTACK_REACHABLE, UNIT, TARGET, &host, &mut tile) }, 0);
    assert_eq!(tile, [0, 0]);
    W.with(|w| {
        *w.borrow_mut() = reachable_world().tile(TARGET, -10000, 0).reads.clone().into_iter().fold(
            World::default(),
            |mut a, (k, v)| {
                a.reads.insert(k, v);
                a
            },
        )
    });
    let mut tile = [7, 7];
    assert_eq!(unsafe { ee_combat_world(EE_COMBAT_WORLD_ATTACK_REACHABLE, UNIT, TARGET, &host, &mut tile) }, 0);
    assert_eq!(tile, [7, 7]);
    W.with(|w| {
        let mut w = w.borrow_mut();
        *w = World::default();
        w.ignored.insert(UNIT, vec![entry((0, 0), (0, 0), TARGET as u32, 10)]);
        w.set(EE_COMBAT_Q_GAME_TIME, 0, 5);
    });
    assert_eq!(unsafe { ee_combat_world(EE_COMBAT_WORLD_IS_IGNORED_ANYWHERE, UNIT, TARGET, &host, &mut tile) }, 1);
    // with the location: the tiles (0, 0) match
    assert_eq!(unsafe { ee_combat_world(EE_COMBAT_WORLD_IS_IGNORED, UNIT, TARGET, &host, &mut tile) }, 1);
    assert_eq!(unsafe { ee_combat_world(EE_COMBAT_WORLD_IGNORE, UNIT, 0x999, &host, &mut tile) }, 0);
    W.with(|w| assert_eq!(w.borrow().ignored[&UNIT].len(), 2));
    assert_eq!(unsafe { ee_combat_world(99, UNIT, TARGET, &host, &mut tile) }, 0);
}
