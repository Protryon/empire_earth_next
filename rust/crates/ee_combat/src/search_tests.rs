//! Unit tests of `search.rs` over a fake world (no game, no data).

use std::collections::{BTreeMap, BTreeSet};

use crate::pursuer::pursuer_of;
use crate::search::*;
use crate::walks::EeCombatHandle;

#[test]
fn permute_is_the_original_shuffle() {
    let mut v = ['a', 'b', 'c', 'd'];
    permute(&mut v, 1);
    assert_eq!(v, ['c', 'd', 'a', 'b']);
    // odd sizes end with the swap of the first
    let mut v = [0, 1, 2];
    permute(&mut v, 4);
    // i 2: index 1, swap(2, 1) -> [0, 2, 1]; i 1: opposite 1, none; index 0; i 0: last swap skipped (index 0)
    assert_eq!(v, [0, 2, 1]);
    let mut empty: [u8; 0] = [];
    permute(&mut empty, 7);
    let mut one = [9];
    permute(&mut one, 7);
    assert_eq!(one, [9]);
    // a permutation for every size and start
    for n in 0..12usize {
        for start in 0..30 {
            let mut v: Vec<usize> = (0..n).collect();
            permute(&mut v, start);
            let mut sorted = v.clone();
            sorted.sort();
            assert_eq!(sorted, (0..n).collect::<Vec<_>>());
        }
    }
}

// ---- the searches over a fake world

#[derive(Clone, Default)]
struct Unit {
    tile: (i32, i32),
    position: (f32, f32),
    target: EeCombatSearchTarget,
    distance: f32,
    to_hit: f32,
    blocked: bool,
    reachable: bool,
    footprint: (i32, i32),
}

#[derive(Default)]
struct World {
    units: Vec<Unit>,
    /// (test, unit) pairs that are true; every other test is false, but `CANDIDATE`, `CAN_TARGET_CLASS`, `SEEN`
    /// and `ALIVE` are true unless listed in `negated`
    tests: BTreeSet<(u32, EeCombatHandle)>,
    negated: BTreeSet<(u32, EeCombatHandle)>,
    regions: BTreeMap<(i32, i32), Vec<EeCombatHandle>>,
    /// the x-sorted list (handles; 99 is the dummy head)
    list: Vec<EeCombatHandle>,
    log: Vec<String>,
    traces: Vec<(EeCombatHandle, i32, i32)>,
}

const POSITIVE: [u32; 4] = [
    EE_COMBAT_SEARCH_TEST_CANDIDATE,
    EE_COMBAT_SEARCH_TEST_CAN_TARGET_CLASS,
    EE_COMBAT_SEARCH_TEST_SEEN,
    EE_COMBAT_SEARCH_TEST_ALIVE,
];

impl World {
    fn unit(&self, h: EeCombatHandle) -> &Unit {
        &self.units[h as usize - 1]
    }
    fn add(&mut self, u: Unit) -> EeCombatHandle {
        self.units.push(u);
        self.units.len() as EeCombatHandle
    }
}

impl SearchHost for World {
    fn regions(&mut self, left: i32, top: i32, right: i32, bottom: i32) -> (i32, i32, i32, i32) {
        (left.div_euclid(4), top.div_euclid(4), right.div_euclid(4), bottom.div_euclid(4))
    }
    fn region_len(&mut self, x: i32, y: i32) -> i32 {
        self.regions.get(&(x, y)).map_or(-1, |r| r.len() as i32)
    }
    fn region_unit(&mut self, x: i32, y: i32, index: i32) -> EeCombatHandle {
        self.regions[&(x, y)][index as usize]
    }
    fn unit_tile(&mut self, unit: EeCombatHandle) -> (i32, i32) {
        assert_ne!(unit, 99, "the dummy head's tile is never read");
        self.unit(unit).tile
    }
    fn unit_position(&mut self, unit: EeCombatHandle) -> (f32, f32) {
        self.unit(unit).position
    }
    fn list_next(&mut self, unit: EeCombatHandle, prev: bool) -> EeCombatHandle {
        let at = self.list.iter().position(|&u| u == unit).unwrap() as i32;
        let next = if prev { at - 1 } else { at + 1 };
        if next < 0 || next as usize >= self.list.len() {
            0
        } else {
            self.list[next as usize]
        }
    }
    fn dummy_head(&mut self, unit: EeCombatHandle) -> bool {
        unit == 99
    }
    fn test(&mut self, test: u32, unit: EeCombatHandle) -> bool {
        self.log.push(format!("test {test} {unit}"));
        if POSITIVE.contains(&test) {
            !self.negated.contains(&(test, unit))
        } else {
            self.tests.contains(&(test, unit))
        }
    }
    fn target(&mut self, unit: EeCombatHandle) -> EeCombatSearchTarget {
        self.unit(unit).target
    }
    fn hit_distance_squared(&mut self, unit: EeCombatHandle, x: f32, y: f32) -> f32 {
        self.log.push(format!("distance {unit} {x} {y}"));
        self.unit(unit).distance
    }
    fn attack_factor(&mut self, unit: EeCombatHandle) -> f32 {
        self.log.push(format!("factor {unit}"));
        self.unit(unit).to_hit
    }
    fn line_blocked(&mut self, unit: EeCombatHandle) -> bool {
        self.log.push(format!("line {unit}"));
        self.unit(unit).blocked
    }
    fn reachable(&mut self, unit: EeCombatHandle) -> bool {
        self.log.push(format!("reachable {unit}"));
        self.unit(unit).reachable
    }
    fn ignore(&mut self, unit: EeCombatHandle) {
        self.log.push(format!("ignore {unit}"));
    }
    fn footprint_tile(&mut self, unit: EeCombatHandle) -> (i32, i32) {
        self.unit(unit).footprint
    }
    fn trace(&mut self, unit: EeCombatHandle, code: i32, score: i32) {
        self.traces.push((unit, code, score));
    }
}

/// Mode 1 (range 6 within sight 8), the table's rows: bonus attacker 1000, in range 100, faster 10, weaker 1, preferred
/// type 77, hit 50, near 64, still 5, done at 100000.
fn input() -> EeCombatSearchIn {
    let mut table = [0i32; 30];
    for row in 0..3 {
        table[row * 10..row * 10 + 10].copy_from_slice(&[1000, 0, 100, 10, 1, 77, 50, 64, 5, 100000]);
    }
    EeCombatSearchIn {
        attack: 10.0,
        max_velocity: 2.0,
        range: 6.0,
        reach: 8.0,
        scale: 1.0,
        min_range: 1.0,
        melee: 0,
        shoots_high: 0,
        over_walls: 0,
        unit_x: 5,
        unit_y: 5,
        x: 5,
        y: 5,
        left: 0,
        top: 0,
        right: 11,
        bottom: 11,
        region_size: 4,
        tick_random: 0,
        position_x: 5.5,
        position_y: 5.5,
        table,
    }
}

fn enemy(tile: (i32, i32), distance: f32) -> Unit {
    Unit {
        tile,
        position: (tile.0 as f32 + 0.5, tile.1 as f32 + 0.5),
        target: EeCombatSearchTarget { multiple: 0, unit_type: 1, attack: 20.0, max_velocity: 3.0 },
        distance,
        to_hit: 1.0,
        blocked: false,
        reachable: true,
        footprint: tile,
    }
}

#[test]
fn search_target_scores_and_keeps_the_best() {
    let mut w = World::default();
    let near = w.add(enemy((6, 5), 4.0)); // in range: 100 + near 16 + hit 50 = 166
    let far = w.add(enemy((9, 5), 49.0)); // out of range 36: near lrintf(64/49) = 1, + 50 = 51
    let weak =
        w.add(Unit { target: EeCombatSearchTarget { attack: 5.0, ..enemy((5, 6), 4.0).target }, ..enemy((5, 6), 4.0) });
    w.regions.insert((1, 1), vec![near, weak]);
    w.regions.insert((2, 1), vec![far]);
    let out = search_target(&input(), &mut w);
    // every region is one of the nine near the unit's; the two of them permuted from 0 keep their order and are
    // walked from the back: (2, 1) first, then (1, 1) from its back
    assert_eq!(
        w.traces,
        vec![
            (far, EE_COMBAT_SEARCH_TRACE_BEST, 51),
            (weak, EE_COMBAT_SEARCH_TRACE_BEST, 167),
            (near, EE_COMBAT_SEARCH_TRACE_NOT_BETTER, 166),
        ]
    );
    assert_eq!(out, EeCombatSearchOut { best: weak, score: 167, done: 0 });
    // the unit is on the search tile: the distance from it
    assert!(w.log.contains(&format!("distance {weak} -10000 -10000")));
}

#[test]
fn search_target_rejects_ignores_and_stops_at_the_preferred_type() {
    let mut w = World::default();
    let blocked = w.add(Unit { blocked: true, ..enemy((6, 6), 4.0) });
    let unreachable = w.add(Unit { reachable: false, ..enemy((6, 7), 4.0) });
    let preferred = w.add(Unit {
        target: EeCombatSearchTarget { unit_type: 77, ..enemy((7, 7), 9.0).target },
        ..enemy((7, 7), 9.0)
    });
    let after = w.add(enemy((7, 6), 4.0));
    w.regions.insert((1, 1), vec![after, preferred, unreachable, blocked]);
    let mut i = input();
    i.x = 6; // the search tile is not the unit's: distances from the tile's centre
    let out = search_target(&i, &mut w);
    assert_eq!(
        w.traces,
        vec![
            (blocked, EE_COMBAT_SEARCH_TRACE_REJECTED, 0),
            (unreachable, EE_COMBAT_SEARCH_TRACE_IGNORED, 166),
            (preferred, EE_COMBAT_SEARCH_TRACE_BEST, 0),
        ]
    );
    assert!(w.log.contains(&format!("ignore {unreachable}")));
    assert!(w.log.contains(&format!("distance {blocked} 6.5 5.5")));
    assert_eq!(out, EeCombatSearchOut { best: preferred, score: 0, done: 1 });
}

#[test]
fn search_target_scores_a_large_unit_once_and_asks_lazily() {
    let mut w = World::default();
    let mut big = enemy((6, 6), 100.0); // beyond the reach of 64
    big.target.multiple = 1;
    let big = w.add(big);
    let nature = w.add(enemy((5, 7), 4.0));
    w.tests.insert((EE_COMBAT_SEARCH_TEST_NATURE, nature));
    w.regions.insert((1, 1), vec![nature, big, big]);
    search_target(&input(), &mut w);
    assert_eq!(
        w.traces,
        vec![
            (big, EE_COMBAT_SEARCH_TRACE_REJECTED, 0),
            (big, EE_COMBAT_SEARCH_TRACE_SCORED, 0),
            (nature, EE_COMBAT_SEARCH_TRACE_REJECTED, 0),
        ]
    );
    // the far unit was never tested past its distance, nature's attacker test was asked and the factor never
    assert!(!w.log.iter().any(|l| l.starts_with(&format!("test {} {big}", EE_COMBAT_SEARCH_TEST_NATURE))));
    assert!(w.log.contains(&format!("test {} {nature}", EE_COMBAT_SEARCH_TEST_ATTACKED_BY)));
    assert!(!w.log.iter().any(|l| l.starts_with("factor")));
}

#[test]
fn search_target_melee_scores_the_near_units_by_distance() {
    let mut w = World::default();
    let a = w.add(Unit { position: (9.0, 5.5), ..enemy((6, 5), 9.0) });
    let b = w.add(Unit { position: (6.0, 5.5), ..enemy((6, 6), 4.0) });
    let c = w.add(Unit { position: (6.0, 5.5), ..enemy((5, 6), 4.0) });
    w.regions.insert((1, 1), vec![a, b, c]);
    let mut i = input();
    i.melee = 1;
    search_target(&i, &mut w);
    // by distance from (5.5, 5.5); c and b tie, in insertion order (the region from its back: c first)
    let order: Vec<EeCombatHandle> = w.traces.iter().map(|t| t.0).collect();
    assert_eq!(order, vec![c, b, a]);
}

#[test]
fn search_along_list_walks_both_ways() {
    let mut w = World::default();
    let unit = w.add(enemy((5, 5), 0.0));
    let left = w.add(enemy((4, 5), 4.0));
    let right = w.add(Unit { blocked: true, ..enemy((6, 5), 4.0) });
    let far = w.add(enemy((30, 5), 4.0));
    w.list = vec![99, left, unit, right, far];
    let out = search_along_list(&input(), unit, &mut w);
    // forward (prev) to `left`, then the backward side: `right` is blocked (no shoots-high test there)
    assert_eq!(w.traces, vec![(left, EE_COMBAT_SEARCH_TRACE_BEST, 166), (right, EE_COMBAT_SEARCH_TRACE_REJECTED, 0)]);
    assert_eq!(out.best, left);
    // the technology's kind is asked before the enemy test
    let kind = w.log.iter().position(|l| *l == format!("test {} {left}", EE_COMBAT_SEARCH_TEST_TECH_KIND_6)).unwrap();
    let enemy_test =
        w.log.iter().position(|l| *l == format!("test {} {left}", EE_COMBAT_SEARCH_TEST_NOT_ENEMY)).unwrap();
    assert!(kind < enemy_test);
}

#[test]
fn search_along_list_ends_without_a_forward_neighbour() {
    let mut w = World::default();
    let unit = w.add(enemy((5, 5), 0.0));
    let right = w.add(enemy((6, 5), 4.0));
    w.list = vec![unit, right];
    let out = search_along_list(&input(), unit, &mut w);
    assert!(w.traces.is_empty());
    assert_eq!(out.best, 0);
    // no attack: nothing is asked
    let mut w = World::default();
    let unit = w.add(enemy((5, 5), 0.0));
    let left = w.add(enemy((4, 5), 4.0));
    w.list = vec![left, unit];
    let mut i = input();
    i.attack = 0.0;
    search_along_list(&i, unit, &mut w);
    assert!(w.log.is_empty());
}

#[test]
fn pursuer_of_keeps_the_first_nearest() {
    let mut w = World::default();
    let unit = w.add(enemy((5, 5), 0.0));
    let a = w.add(Unit { footprint: (7, 5), ..Unit::default() });
    let b = w.add(Unit { footprint: (5, 3), ..Unit::default() });
    let c = w.add(Unit { footprint: (5, 6), ..Unit::default() });
    w.tests.insert((EE_COMBAT_SEARCH_TEST_PURSUES, a));
    w.tests.insert((EE_COMBAT_SEARCH_TEST_PURSUES, b));
    w.list = vec![99, b, a, unit, c];
    assert_eq!(pursuer_of(unit, 5, 5, &mut w), a);
    // a (4) forward first, then c does not pursue, then b ties a and is not taken
    assert_eq!(w.traces, vec![(a, 4, 0), (b, 4, 0)]);
}
