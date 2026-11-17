//! The Create Units effect (`EESMEffect::CreateUnits`): how many units an object's effect makes and for whom. A
//! dynamic object, or one with no units that does not pick on the map, creates a random count (its minimum to its
//! maximum) of a picked class for one chosen owner each; otherwise each of its units is copied once for the first
//! existing unit's player. Placing and finishing each unit (`FindPlacement`, `CreateUnit` or a wall, the flight,
//! power and attribute set-up, the sound) stays the C++'s, one write per unit (`EE_SCENARIO_WRITE_CREATE_UNIT`).

#![allow(clippy::unnecessary_cast)]

use std::os::raw::{c_long, c_ulong};

use crate::scenario::condition::choose_player_recorded;
use crate::scenario::fields::EeScenarioObjectFields;
use crate::scenario::host::*;
use crate::scenario::objeval::{
    EE_SCENARIO_READ_UNIT_CLASS_TYPE, EE_SCENARIO_READ_UNIT_FLAGS, EE_SCENARIO_READ_UNIT_PLAYER,
};
use crate::scenario::placement::{EE_SCENARIO_READ_OBJECT_UNIT_AT, EE_SCENARIO_READ_OBJECT_UNIT_COUNT};

/// One unit made for player `player` (the C++ `CreateUnits`' loop body): type `b`; `c` 1 for a created unit (the
/// class `gUnitClassTable[b]`, the id kept for the object), 0 for a copy of the object's unit `d` (its class).
pub const EE_SCENARIO_WRITE_CREATE_UNIT: u32 = 18;

/// `EESMEffect::CreateUnits` over object `object` (not NULL) with its numbers `f` (read on entry, as the original
/// read them; its units are read live): whether it created units (the C++ then makes them the object's).
pub fn create_units<H: Host>(h: &mut H, object: i32, f: &EeScenarioObjectFields) -> bool {
    let create = !f.mIsSelectOnMap && (h.read(EE_SCENARIO_READ_OBJECT_UNIT_COUNT, object, 0) <= 0 || f.mIsDynamic);
    let count: c_ulong = if create {
        h.read(EE_SCENARIO_READ_RANDOM, f.mMinCount as c_long as i32, f.mMaxCount as c_long as i32) as c_long as c_ulong
    } else {
        h.read(EE_SCENARIO_READ_OBJECT_UNIT_COUNT, object, 0) as c_ulong
    };
    let mut i: c_ulong = 0;
    while i < count {
        let players: Vec<i32> = if create {
            choose_player_recorded(h, f.mOwner as c_long as i32, true, true).1
        } else {
            // the first existing unit's player
            let mut first = Vec::new();
            let mut j: c_ulong = 0;
            while j < count {
                let unit = h.read(EE_SCENARIO_READ_OBJECT_UNIT_AT, object, j as i32) as i32;
                j += 1;
                if h.read(EE_SCENARIO_READ_UNIT_FLAGS, unit, 0) & 1 != 0 {
                    first.push(h.read(EE_SCENARIO_READ_UNIT_PLAYER, unit, 0) as i32);
                    break;
                }
            }
            first
        };
        for &player in &players {
            if h.read(EE_SCENARIO_READ_PLAYER_EXISTS, player, 0) == 0 {
                continue;
            }
            let unit_type;
            if create {
                unit_type = h.read(EE_SCENARIO_READ_OBJECT_PICK, object, 0) as i32;
                if unit_type < 0 || h.read(EE_SCENARIO_READ_CLASS_EXISTS, unit_type, 0) == 0 {
                    continue;
                }
            } else {
                let unit = h.read(EE_SCENARIO_READ_OBJECT_UNIT_AT, object, i as i32) as i32;
                if h.read(EE_SCENARIO_READ_UNIT_FLAGS, unit, 0) & 3 != 3 {
                    continue;
                }
                unit_type = h.read(EE_SCENARIO_READ_UNIT_CLASS_TYPE, unit, 0) as i32;
            }
            h.write(EE_SCENARIO_WRITE_CREATE_UNIT, player as c_long, unit_type as i64, create as i64, i as i64);
        }
        i += 1;
    }
    create
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Units by id: (flags, class type, player); the object's list; players 1..=3 exist; the log of the draws, picks
    /// and writes.
    struct World {
        units: Vec<(i64, i64, i64)>,
        list: Vec<i32>,
        picks: Vec<i64>,
        log: Vec<String>,
    }

    impl Host for World {
        fn read4(&mut self, op: u32, a: i32, b: i32, _c: i32, _d: i32) -> i64 {
            let u = self.units.get(a.max(0) as usize).copied().unwrap_or((0, 0, 0));
            match op {
                EE_SCENARIO_READ_OBJECT_UNIT_COUNT => self.list.len() as i64,
                EE_SCENARIO_READ_OBJECT_UNIT_AT => self.list[b as usize] as i64,
                EE_SCENARIO_READ_RANDOM => {
                    self.log.push(format!("random {a} {b}"));
                    b as i64
                }
                EE_SCENARIO_READ_NUM_PLAYERS => 4,
                EE_SCENARIO_READ_PLAYER_EXISTS => (1..=3).contains(&a) as i64,
                EE_SCENARIO_READ_PLAYER_HUMAN => (a == 1) as i64,
                EE_SCENARIO_READ_PLAYER_COMPUTER => (a >= 2) as i64,
                EE_SCENARIO_READ_CAPTURE_SEED => -1,
                EE_SCENARIO_READ_OBJECT_PICK => self.picks.remove(0),
                EE_SCENARIO_READ_CLASS_EXISTS => (a != 9) as i64,
                EE_SCENARIO_READ_UNIT_FLAGS => u.0,
                EE_SCENARIO_READ_UNIT_CLASS_TYPE => u.1,
                EE_SCENARIO_READ_UNIT_PLAYER => u.2,
                _ => panic!("read {op}"),
            }
        }
        fn write(&mut self, op: u32, player: c_long, b: i64, c: i64, d: i64) {
            assert_eq!(op, EE_SCENARIO_WRITE_CREATE_UNIT);
            self.log.push(format!("unit {player} {b} {c} {d}"));
        }
    }

    fn world() -> World {
        World { units: vec![(0, 0, 0), (3, 20, 2), (1, 21, 3), (3, 22, 3)], list: vec![], picks: vec![], log: vec![] }
    }

    #[test]
    fn an_empty_object_creates_a_random_count_for_the_chosen_owner() {
        let mut w = world();
        w.picks = vec![5, -1, 9];
        // owner 9: player 3; the draw answers the maximum
        let f = EeScenarioObjectFields { mOwner: 9, mMinCount: 1, mMaxCount: 3, ..Default::default() };
        assert!(create_units(&mut w, 4, &f));
        // no class (-1) and a class missing from the table (9) make nothing
        assert_eq!(w.log, ["random 1 3", "unit 3 5 1 0"]);
        // a missing owner makes nothing either
        let mut w = world();
        let f = EeScenarioObjectFields { mOwner: 11, mMinCount: 2, mMaxCount: 2, ..Default::default() };
        assert!(create_units(&mut w, 4, &f));
        assert_eq!(w.log, ["random 2 2"]);
    }

    #[test]
    fn an_object_with_units_copies_the_living_ones_for_the_first_units_player() {
        let mut w = world();
        // unit 0 is gone, unit 2 dead: the player is unit 1's (2); units 1 and 3 copied
        w.list = vec![0, 1, 2, 3];
        let f = EeScenarioObjectFields { mIsSelectOnMap: true, mIsDynamic: true, ..Default::default() };
        assert!(!create_units(&mut w, 4, &f));
        assert_eq!(w.log, ["unit 2 20 0 1", "unit 2 22 0 3"]);
        // a static object with units that does not pick on the map copies too; a dynamic one creates
        let mut w = world();
        w.list = vec![3];
        assert!(!create_units(&mut w, 4, &EeScenarioObjectFields::default()));
        assert_eq!(w.log, ["unit 3 22 0 0"]);
        let mut w = world();
        w.list = vec![3];
        w.picks = vec![7];
        let f =
            EeScenarioObjectFields { mIsDynamic: true, mOwner: 8, mMinCount: 1, mMaxCount: 1, ..Default::default() };
        assert!(create_units(&mut w, 4, &f));
        assert_eq!(w.log, ["random 1 1", "unit 2 7 1 0"]);
    }
}
