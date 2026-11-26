use super::*;
use std::collections::BTreeMap;

/// A recording host: each statement asked (op, a, b, c), its answer from `answers` keyed by (op, a) (0 else).
#[derive(Default)]
struct Mock {
    log: Vec<(i32, u64, i64, i64)>,
    answers: BTreeMap<(i32, u64), i64>,
}

impl Mock {
    fn answer(mut self, op: i32, a: u64, value: i64) -> Self {
        self.answers.insert((op, a), value);
        self
    }
    fn ops(&self) -> Vec<i32> {
        self.log.iter().map(|e| e.0).collect()
    }
    fn count(&self, op: i32) -> usize {
        self.log.iter().filter(|e| e.0 == op).count()
    }
    fn find(&self, op: i32) -> Option<(i32, u64, i64, i64)> {
        self.log.iter().copied().find(|e| e.0 == op)
    }
}

impl EuHost for Mock {
    fn eu(&mut self, op: i32, unit: u64, a: u64, b: i64, c: i64) -> i64 {
        assert_eq!(unit, 1);
        if op == EE_UNITS_EU_PART_TAKE_HIT {
            // SAFETY: the four values the body hands over
            let v = unsafe { std::slice::from_raw_parts(b as usize as *const i64, 4) };
            self.log.push((op, a, v[1], v[3]));
            return 0;
        }
        self.log.push((op, a, b, c));
        *self.answers.get(&(op, a)).unwrap_or(&0)
    }
}

const ATTACKER: u64 = 50;

fn alive() -> Mock {
    Mock::default()
        .answer(EE_UNITS_EU_COMPLEX, 0, 1)
        .answer(EE_UNITS_EU_HIT_POINTS, 0, 100)
        .answer(EE_UNITS_EU_APPLY_MORALE, 0, 8)
        .answer(EE_UNITS_EU_HIT_NEW_POINTS, 0, 92)
        .answer(EE_UNITS_EU_NUM_PLAYERS, 0, 4)
        .answer(EE_UNITS_EU_TILE_X, 0, 10)
        .answer(EE_UNITS_EU_TILE_Y, 0, 11)
}

#[test]
fn a_linked_part_passes_the_hit_to_its_first_part() {
    let mut h = Mock::default().answer(EE_UNITS_EU_COMPLEX, 0, 1).answer(EE_UNITS_EU_PREVIOUS_PART, 0, 9);
    take_hit(&mut h, 1, ATTACKER, 10, 2, 1);
    assert_eq!(h.ops(), [EE_UNITS_EU_COMPLEX, EE_UNITS_EU_PREVIOUS_PART, EE_UNITS_EU_PART_TAKE_HIT]);
    assert_eq!(h.log[2], (EE_UNITS_EU_PART_TAKE_HIT, 9, 10, 1));
}

#[test]
fn a_cinematic_hit_only_sparks() {
    let mut h = Mock::default().answer(EE_UNITS_EU_IN_CINEMATIC, 0, 1).answer(EE_UNITS_EU_HIT_POINTS, 0, 5).answer(
        EE_UNITS_EU_HAS_BLOOD,
        0,
        1,
    );
    take_hit(&mut h, 1, 0, 10, 2, 1);
    assert_eq!(h.count(EE_UNITS_EU_BLOOD), 1);
    assert_eq!(h.count(EE_UNITS_EU_HIT_FORMULA), 0);
}

#[test]
fn an_enemy_hit_adjusts_the_damage_and_remembers_the_attacker() {
    let mut h = alive()
        .answer(EE_UNITS_EU_ON_MAP, ATTACKER, 1)
        .answer(EE_UNITS_EU_IS_ENEMY, ATTACKER, 1)
        .answer(EE_UNITS_EU_TARGETABLE, ATTACKER, 1)
        .answer(EE_UNITS_EU_COMPLEX_OF, ATTACKER, 51)
        .answer(EE_UNITS_EU_SHOOTS_TRACERS, 51, 1)
        .answer(EE_UNITS_EU_HAS_SPARKS, 0, 1)
        .answer(EE_UNITS_EU_HAS_BLOOD, 0, 1)
        .answer(EE_UNITS_EU_TILE_X, ATTACKER, 3)
        .answer(EE_UNITS_EU_TILE_Y, ATTACKER, 4)
        .answer(EE_UNITS_EU_HAS_UNITS_STATE, 0, 1);
    take_hit(&mut h, 1, ATTACKER, 10, 2, 1);
    // the modifiers, then the morale's damage into the formula
    assert_eq!(h.find(EE_UNITS_EU_HIT_FORMULA), Some((EE_UNITS_EU_HIT_FORMULA, 0, 100, 8)));
    assert_eq!(h.find(EE_UNITS_EU_SET_LAST_ATTACKER).map(|e| e.1), Some(51));
    assert_eq!(h.find(EE_UNITS_EU_DAMAGE_TAKEN), Some((EE_UNITS_EU_DAMAGE_TAKEN, 0, 2, 8)));
    assert_eq!(h.find(EE_UNITS_EU_ON_DEALT), Some((EE_UNITS_EU_ON_DEALT, 0, 2, 8)));
    assert_eq!(h.find(EE_UNITS_EU_ADD_ATTACK_EVENT), Some((EE_UNITS_EU_ADD_ATTACK_EVENT, 0, 3, 4)));
    // tracers: no sparks, blood instead
    assert_eq!(h.count(EE_UNITS_EU_SPARKS), 0);
    assert_eq!(h.count(EE_UNITS_EU_BLOOD), 1);
    assert_eq!(h.find(EE_UNITS_EU_SET_HIT_POINTS), Some((EE_UNITS_EU_SET_HIT_POINTS, 0, 92, 0)));
    assert_eq!(h.count(EE_UNITS_EU_KILL), 0);
}

#[test]
fn a_ground_hit_skips_the_modifiers() {
    let mut h = alive();
    take_hit(&mut h, 1, 0, 10, 0, 2);
    assert_eq!(h.count(EE_UNITS_EU_APPLY_MODIFIERS), 0);
    assert_eq!(h.find(EE_UNITS_EU_HIT_FORMULA), Some((EE_UNITS_EU_HIT_FORMULA, 0, 100, 10)));
    // player 0 has no Lua count; the event is at the unit's own tile
    assert_eq!(h.count(EE_UNITS_EU_ON_DEALT), 0);
    assert_eq!(h.find(EE_UNITS_EU_ADD_ATTACK_EVENT), Some((EE_UNITS_EU_ADD_ATTACK_EVENT, 0, 10, 11)));
}

#[test]
fn a_killing_hit_kills_and_counts_the_loss() {
    let mut h = alive()
        .answer(EE_UNITS_EU_HIT_NEW_POINTS, 0, 0)
        .answer(EE_UNITS_EU_CATEGORY, 0, CATEGORY_AIRPLANE)
        .answer(EE_UNITS_EU_COMPLEX_CATEGORY, 0, CATEGORY_GATE)
        .answer(EE_UNITS_EU_PLAYER_ID, 0, 3)
        .answer(EE_UNITS_EU_FIRST_PLAYER_ID, 0, 0)
        .answer(EE_UNITS_EU_UNIT_TYPE, 0, UNIT_TYPE_CP_ICBM)
        .answer(EE_UNITS_EU_IS_CURRENT_PLAYER, 0, 1)
        .answer(EE_UNITS_EU_SELECTED, 0, 1);
    take_hit(&mut h, 1, 0, 200, 2, 2);
    assert_eq!(h.find(EE_UNITS_EU_STATS_RESET), Some((EE_UNITS_EU_STATS_RESET, 0, 2, 2)));
    let ops = h.ops();
    let kill = ops.iter().position(|&o| o == EE_UNITS_EU_KILL).unwrap();
    assert_eq!(ops[kill + 1], EE_UNITS_EU_SET_HIT_POINTS);
    assert_eq!(h.count(EE_UNITS_EU_ICBM_EXPLOSION), 1);
    assert_eq!(h.count(EE_UNITS_EU_MINIMAP_PING), 1);
    assert_eq!(ops.last(), Some(&EE_UNITS_EU_POST_424));
}

#[test]
fn a_dead_resource_just_drops_to_zero() {
    let mut h = alive().answer(EE_UNITS_EU_HIT_NEW_POINTS, 0, -5).answer(EE_UNITS_EU_CATEGORY, 0, CATEGORY_RESOURCE);
    take_hit(&mut h, 1, 0, 200, 2, 2);
    assert_eq!(h.find(EE_UNITS_EU_SET_HIT_POINTS), Some((EE_UNITS_EU_SET_HIT_POINTS, 0, 0, 0)));
    assert_eq!(h.count(EE_UNITS_EU_KILL), 0);
}
