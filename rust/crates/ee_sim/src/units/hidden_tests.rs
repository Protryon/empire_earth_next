//! The hidden walk over a mock map: the original's tiles, order, phases and writes.

use super::*;
use std::collections::BTreeMap;

#[derive(Default)]
struct Mock {
    cinematic: bool,
    time: i32,
    tiles: BTreeMap<(i64, i64), Vec<u64>>,
    facts: BTreeMap<u64, HiddenFacts>,
    flags: BTreeMap<u64, u8>,
    /// the tiles read, in order
    read: std::cell::RefCell<Vec<(i64, i64)>>,
}

impl HiddenHost for Mock {
    fn in_cinematic(&self) -> bool {
        self.cinematic
    }
    fn game_time(&self) -> i32 {
        self.time
    }
    fn tile_units(&self, x: i64, y: i64) -> Option<Vec<u64>> {
        self.read.borrow_mut().push((x, y));
        if x < 0 || y < 0 {
            return None;
        }
        Some(self.tiles.get(&(x, y)).cloned().unwrap_or_default())
    }
    fn facts(&self, unit: u64) -> HiddenFacts {
        self.facts.get(&unit).copied().unwrap_or_default()
    }
    fn flags(&self, unit: u64) -> u8 {
        *self.flags.get(&unit).unwrap_or(&0)
    }
    fn set_flags(&mut self, unit: u64, flags: u8) {
        self.flags.insert(unit, flags);
    }
}

fn complex_me(x: i64, y: i64) -> HiddenSelf {
    HiddenSelf { complex: true, tile: (x, y), tile_size: 1, can_be_hidden: true, seen_by_local: true, ..Default::default() }
}

#[test]
fn check_tiles_order() {
    assert_eq!(check_tiles(5, 6, 1), vec![(5, 6), (5, 7), (4, 7), (4, 6)]);
    assert_eq!(
        check_tiles(5, 6, 2),
        vec![(5, 6), (4, 8), (4, 7), (4, 6), (6, 8), (5, 8)]
    );
}

#[test]
fn moved_unit_hides_the_obstacles_around_it() {
    let mut m = Mock { time: 4999, ..Default::default() };
    // a tree north of the unit (obstacle) with a two-part complex chain; a soldier east (no obstacle)
    m.tiles.insert((3, 3), vec![1]);
    m.tiles.insert((3, 2), vec![10]);
    m.tiles.insert((4, 3), vec![20]);
    m.facts.insert(10, HiddenFacts { obstacle: true, complex: 11, ..Default::default() });
    m.facts.insert(11, HiddenFacts { next_part: 12, ..Default::default() });
    m.facts.insert(12, HiddenFacts::default());
    m.facts.insert(20, HiddenFacts::default());
    m.flags.insert(1, EE_UNITS_HIDDEN_MOVED | 0x40);
    m.flags.insert(10, 0x01);
    let hidden = update_hidden(&mut m, 1, &complex_me(3, 3));
    // phase ((4999 + 2) / 1000) % 3 = 2
    assert_eq!(m.flags[&10], EE_UNITS_HIDDEN_HIDDEN | 2);
    assert_eq!(m.flags.get(&11), None, "the chain starts at the complex unit's next part");
    assert_eq!(m.flags[&12], EE_UNITS_HIDDEN_HIDDEN | 2);
    assert_eq!(m.flags.get(&20), None);
    assert_eq!(m.flags[&1], 0x40, "the moved bit cleared, the others kept");
    assert!(!hidden);
    assert_eq!(*m.read.borrow(), vec![(3, 3), (3, 2), (4, 2), (4, 3)]);
}

#[test]
fn still_unit_clears_its_hidden_bit_and_cinematics_leave_it() {
    let mut m = Mock::default();
    m.flags.insert(1, EE_UNITS_HIDDEN_HIDDEN | 1);
    assert!(!update_hidden(&mut m, 1, &complex_me(3, 3)));
    assert_eq!(m.flags[&1], 1);
    m.cinematic = true;
    m.flags.insert(1, EE_UNITS_HIDDEN_HIDDEN | EE_UNITS_HIDDEN_MOVED);
    assert!(update_hidden(&mut m, 1, &complex_me(3, 3)));
    assert_eq!(m.flags[&1], EE_UNITS_HIDDEN_HIDDEN | EE_UNITS_HIDDEN_MOVED);
}

#[test]
fn cloaked_enemy_rechecks_behind_obstacles_in_phase() {
    let mut m = Mock::default();
    let me = HiddenSelf { cloaked: true, seen_by_local: false, next_part: 5, ..complex_me(3, 3) };
    // a live unit that can be hidden on the west tile (read last of the four, so first)
    m.tiles.insert((2, 3), vec![30]);
    m.facts.insert(30, HiddenFacts { complex: 31, ..Default::default() });
    m.facts.insert(31, HiddenFacts { can_be_hidden: true, hit_points: 5, ..Default::default() });
    m.facts.insert(5, HiddenFacts::default());
    // not due at game time 0: the phase 1 is ahead of (0 / 1000) % 3
    m.time = 0;
    m.flags.insert(1, EE_UNITS_HIDDEN_HIDDEN | 1);
    assert!(update_hidden(&mut m, 1, &me), "phase 1 > 0: not due, still hidden");
    assert!(m.read.borrow().is_empty());
    m.time = 1000;
    assert!(update_hidden(&mut m, 1, &me));
    // (1000 + 2) / 1000 % 3 = 1
    assert_eq!(m.flags[&1], EE_UNITS_HIDDEN_HIDDEN | 1);
    assert_eq!(m.flags[&5], EE_UNITS_HIDDEN_HIDDEN | 1, "the parts follow");
    assert_eq!(*m.read.borrow(), vec![(2, 3)]);
    // nothing around: cleared
    m.tiles.clear();
    m.read.borrow_mut().clear();
    assert!(!update_hidden(&mut m, 1, &me));
    assert_eq!(m.flags[&1], 0);
    assert_eq!(*m.read.borrow(), vec![(2, 3), (2, 4), (3, 4), (3, 3)]);
}

#[test]
fn simple_unit_hidden_by_a_moving_object_or_an_unowned_food_source() {
    let mut m = Mock { time: 3000, ..Default::default() };
    let me = HiddenSelf { tile: (3, 3), tile_size: 1, foundation: false, ..Default::default() };
    m.flags.insert(1, EE_UNITS_HIDDEN_HIDDEN);
    // a moving object (no complex unit, no obstacle)
    m.tiles.insert((3, 4), vec![40]);
    m.facts.insert(40, HiddenFacts { max_velocity: 1.0, ..Default::default() });
    assert!(update_hidden(&mut m, 1, &me));
    // (3000 + 2) / 1000 % 3 = 0
    assert_eq!(m.flags[&1], EE_UNITS_HIDDEN_HIDDEN);
    // a dead food source of player 0 that is no citizen still hides; a citizen's does not
    m.time = 4000;
    m.facts.insert(40, HiddenFacts { complex: 41, ..Default::default() });
    m.facts.insert(
        41,
        HiddenFacts { complex: 41, can_be_hidden: true, resource_type: 2, resource_amount: 3, ..Default::default() },
    );
    assert!(update_hidden(&mut m, 1, &me));
    assert_eq!(m.flags[&1], EE_UNITS_HIDDEN_HIDDEN | 1);
    m.time = 5000;
    m.facts.get_mut(&41).unwrap().category = 6;
    assert!(!update_hidden(&mut m, 1, &me));
    assert_eq!(m.flags[&1], 0);
    // a cinematic clears it
    m.cinematic = true;
    m.flags.insert(1, EE_UNITS_HIDDEN_HIDDEN | 2);
    assert!(!update_hidden(&mut m, 1, &me));
    assert_eq!(m.flags[&1], 2);
}

#[test]
fn foundations_are_never_reported_hidden() {
    let mut m = Mock::default();
    let me = HiddenSelf { foundation: true, can_be_hidden: false, ..complex_me(3, 3) };
    m.flags.insert(1, EE_UNITS_HIDDEN_HIDDEN | 2);
    assert!(!update_hidden(&mut m, 1, &me));
    assert_eq!(m.flags[&1], EE_UNITS_HIDDEN_HIDDEN | 2, "not due at phase 0");
}
