use super::*;
use std::collections::BTreeMap;

/// A recording host: each statement asked (op, a, b, c, d, e), its answer from `answers` keyed by (op, a) (0 else).
#[derive(Default)]
struct Mock {
    log: Vec<(i32, u64, u64, i64, i64, i64)>,
    answers: BTreeMap<(i32, u64), i64>,
    /// answers that change with each ask of (op, a): popped front first
    sequences: BTreeMap<(i32, u64), Vec<i64>>,
    /// answers keyed by (op, c)
    by_c: BTreeMap<(i32, i64), i64>,
    floats: BTreeMap<i32, [f32; 4]>,
}

impl Mock {
    fn answer(mut self, op: i32, a: u64, value: i64) -> Self {
        self.answers.insert((op, a), value);
        self
    }
    fn answer_c(mut self, op: i32, c: i64, value: i64) -> Self {
        self.by_c.insert((op, c), value);
        self
    }
    fn sequence(mut self, op: i32, a: u64, values: &[i64]) -> Self {
        self.sequences.insert((op, a), values.to_vec());
        self
    }
    fn floats(mut self, op: i32, f: [f32; 4]) -> Self {
        self.floats.insert(op, f);
        self
    }
    fn ops(&self) -> Vec<i32> {
        self.log.iter().map(|e| e.0).collect()
    }
    fn count(&self, op: i32) -> usize {
        self.log.iter().filter(|e| e.0 == op).count()
    }
    fn find(&self, op: i32) -> Vec<(i32, u64, u64, i64, i64, i64)> {
        self.log.iter().filter(|e| e.0 == op).cloned().collect()
    }
}

impl UbHost for Mock {
    fn ub(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64, e: i64, f: &mut [f32; 4]) -> i64 {
        self.log.push((op, a, b, c, d, e));
        if let Some(v) = self.floats.get(&op) {
            *f = *v;
        }
        if let Some(values) = self.sequences.get_mut(&(op, a)) {
            if !values.is_empty() {
                return values.remove(0);
            }
        }
        if let Some(v) = self.by_c.get(&(op, c)) {
            return *v;
        }
        *self.answers.get(&(op, a)).unwrap_or(&0)
    }
}

const UNIT: u64 = 10;
const NEW_UNIT: u64 = 20;
const CLASS: u64 = 30;
const PLAYER: u64 = 40;

#[test]
fn a_flaming_archer_converts_to_the_plain_class_without_the_power() {
    let mut h = Mock::default()
        .answer_c(EE_UNITS_UB_PLAYER_AT, 2, PLAYER as i64)
        .answer(EE_UNITS_UB_UNIT_CLASS, UNIT, CLASS as i64)
        .answer(EE_UNITS_UB_CLASS_TYPE, CLASS, UNIT_TYPE_ARCH05_COMPOSITE_BOW_FLAMING)
        .answer_c(EE_UNITS_UB_CLASS_OF_TYPE, UNIT_TYPE_ARCH05_COMPOSITE_BOW, 31)
        .answer(EE_UNITS_UB_CREATE_UNIT, 31, NEW_UNIT as i64)
        .answer(EE_UNITS_UB_HIT_FRACTION, UNIT, bits(0.5));
    let new = convert_unit(&mut h, UNIT, 2, false);
    // the copy is made of the plain class; no complex unit (the mock's): nothing else of the new unit is asked
    assert_eq!(new, 0);
    assert_eq!(h.find(EE_UNITS_UB_CREATE_UNIT)[0].1, 31);
    assert_eq!(h.count(EE_UNITS_UB_GROUP_CONTAINS), 20);
    // Kill(false, false, true) after the copy, no statistics without a conversion
    let kill = h.find(EE_UNITS_UB_KILL);
    assert_eq!(kill, vec![(EE_UNITS_UB_KILL, UNIT, 0, 4, 0, 0)]);
    assert_eq!(h.count(EE_UNITS_UB_STATS_RESET), 0);
    let ops = h.ops();
    let media = ops.iter().position(|&o| o == EE_UNITS_UB_SET_CLASS_MEDIA).unwrap();
    let create = ops.iter().position(|&o| o == EE_UNITS_UB_CREATE_UNIT).unwrap();
    let unload = ops.iter().position(|&o| o == EE_UNITS_UB_UNLOAD_CLASS_MEDIA).unwrap();
    assert!(media < create && create < unload);
}

#[test]
fn a_converted_town_center_grants_a_missing_power_and_becomes_a_settlement() {
    let mut h = Mock::default()
        .answer_c(EE_UNITS_UB_PLAYER_AT, 1, PLAYER as i64)
        .answer(EE_UNITS_UB_UNIT_CLASS, UNIT, CLASS as i64)
        .answer(EE_UNITS_UB_CLASS_TYPE, CLASS, UNIT_TYPE_TOWN_CENTER)
        .answer(EE_UNITS_UB_IS_BUILT, UNIT, 1)
        .answer_c(EE_UNITS_UB_HAS_POWER, CIV_POWER_ADAPTATION, 1)
        .answer(EE_UNITS_UB_NUM_POWERS, 0, 3)
        .answer_c(EE_UNITS_UB_UNIT_HAS_POWER, 0x27, 1)
        .answer_c(EE_UNITS_UB_MAKE_POWER_UPGRADE, 0x27, 77)
        .answer(EE_UNITS_UB_AVAILABLE_POINTS, PLAYER, 12)
        .answer_c(EE_UNITS_UB_CURRENT_PLAYER, 0, PLAYER as i64)
        .answer_c(EE_UNITS_UB_CLASS_OF_TYPE, UNIT_TYPE_SETTLEMENT, 32);
    convert_unit(&mut h, UNIT, 1, true);
    assert_eq!(h.find(EE_UNITS_UB_BUY)[0].3, 77);
    assert_eq!(h.find(EE_UNITS_UB_SET_POINTS)[0].3, 12);
    assert_eq!(h.count(EE_UNITS_UB_UPGRADES_SHUFFLE), 0);
    assert_eq!(h.count(EE_UNITS_UB_ADAPTATION_CHAT), 1);
    assert_eq!(h.find(EE_UNITS_UB_CREATE_UNIT)[0].1, 32);
    // the conversion's statistics: the new owner's series 5, then the building series of the old owner
    assert_eq!(h.find(EE_UNITS_UB_STATS_RESET)[0].3, 5);
    assert_eq!(h.find(EE_UNITS_UB_UNIT_STATS_RESET)[0].3, 4);
}

#[test]
fn without_a_power_to_grant_a_shuffled_upgrade_is_given() {
    let mut h = Mock::default()
        .answer_c(EE_UNITS_UB_PLAYER_AT, 1, PLAYER as i64)
        .answer(EE_UNITS_UB_UNIT_CLASS, UNIT, CLASS as i64)
        .answer(EE_UNITS_UB_CLASS_TYPE, CLASS, UNIT_TYPE_SETTLEMENT)
        .answer(EE_UNITS_UB_IS_BUILT, UNIT, 1)
        .answer_c(EE_UNITS_UB_HAS_POWER, CIV_POWER_ADAPTATION, 1)
        .answer(EE_UNITS_UB_NUM_POWERS, 0, 2)
        .answer(EE_UNITS_UB_UPGRADES_COUNT, 0, 3)
        .answer_c(EE_UNITS_UB_UPGRADE_AT, 0, 54)
        .answer_c(EE_UNITS_UB_UPGRADE_AT, 1, 55)
        .answer_c(EE_UNITS_UB_HAS_UPGRADE, 54, 1);
    convert_unit(&mut h, UNIT, 1, false);
    let ops = h.ops();
    let copy = ops.iter().position(|&o| o == EE_UNITS_UB_UPGRADES_COPY).unwrap();
    assert_eq!(ops[copy + 1], EE_UNITS_UB_UPGRADES_SHUFFLE);
    assert_eq!(h.find(EE_UNITS_UB_UPGRADE_NAME)[0].3, 55);
    assert_eq!(h.find(EE_UNITS_UB_BUY)[0].3, 55);
    assert_eq!(h.count(EE_UNITS_UB_ADAPTATION_CHAT), 0);
}

#[test]
fn a_replaced_unit_keeps_its_hit_point_share_and_groups() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UB_CREATE_UNIT, CLASS, NEW_UNIT as i64)
        .answer(EE_UNITS_UB_COMPLEX_OF, NEW_UNIT, NEW_UNIT as i64)
        .answer(EE_UNITS_UB_COMPLEX_OF, UNIT, UNIT as i64)
        .answer(EE_UNITS_UB_HIT_FRACTION, UNIT, bits(0.25))
        .answer(EE_UNITS_UB_MAX_HIT_POINTS, NEW_UNIT, 10)
        .answer(EE_UNITS_UB_PLAYER_OF, UNIT, PLAYER as i64)
        .answer(EE_UNITS_UB_PLAYER_OF, NEW_UNIT, PLAYER as i64)
        .answer_c(EE_UNITS_UB_GROUP_AT, 3, 90)
        .answer(EE_UNITS_UB_GROUP_NUM_UNITS, 90, 2)
        .answer_c(EE_UNITS_UB_GROUP_UNIT, 1, UNIT as i64);
    let new = replace_unit(&mut h, UNIT, 1, CLASS);
    assert_eq!(new, NEW_UNIT);
    // lrintf(10 * 0.25) = 2 (round half to even)
    assert_eq!(h.find(EE_UNITS_UB_SET_HIT_POINTS)[0], (EE_UNITS_UB_SET_HIT_POINTS, NEW_UNIT, 0, 2, 0, 0));
    assert_eq!(h.find(EE_UNITS_UB_GROUP_ADD), vec![(EE_UNITS_UB_GROUP_ADD, 90, NEW_UNIT, 0, 0, 0)]);
    assert_eq!(h.find(EE_UNITS_UB_KILL)[0].3, 2);
    assert_eq!(h.find(EE_UNITS_UB_GATE_REPLACED).len(), 1);
}

#[test]
fn an_airport_site_sets_every_part_to_one_hit_point() {
    let mut h = Mock::default()
        .answer_c(EE_UNITS_UB_CLASS_OF_TYPE, UNIT_TYPE_AIRPORT, CLASS as i64)
        .answer(EE_UNITS_UB_CREATE_UNIT, CLASS, UNIT as i64)
        .answer(EE_UNITS_UB_NEXT_PART, UNIT, 11)
        .answer(EE_UNITS_UB_NEXT_PART, 11, 12)
        .sequence(EE_UNITS_UB_TILE_VISIBILITY, UNIT, &[1, 1])
        .answer(EE_UNITS_UB_UNIT_CLASS, UNIT, CLASS as i64)
        .answer(EE_UNITS_UB_CLASS_TYPE, CLASS, UNIT_TYPE_GRANARY);
    make_building(&mut h, PLAYER, UNIT_TYPE_AIRPORT, 3, 4);
    let hp: Vec<u64> = h.find(EE_UNITS_UB_SET_HIT_POINTS).iter().map(|e| e.1).collect();
    assert_eq!(hp, vec![UNIT, 11, 12]);
    assert_eq!(h.count(EE_UNITS_UB_ADD_FOR_UNSEEN), 1);
    assert_eq!(h.count(EE_UNITS_UB_PAY_CLASS), 1);
    assert_eq!(h.count(EE_UNITS_UB_REPLANT_FARMS), 1);
    let create = h.find(EE_UNITS_UB_CREATE_UNIT)[0];
    assert_eq!((create.2, create.3, create.4), (1, 3, 4));
}

#[test]
fn find_target_near_searches_the_ceiling_radius_when_enemies_are_near() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UB_RANGE, UNIT, bits(2.2))
        .answer(EE_UNITS_UB_LINE_OF_SIGHT, UNIT, bits(1.5))
        .answer(EE_UNITS_UB_ATTACK, UNIT, bits(3.0))
        .answer(EE_UNITS_UB_SUM_OTHERS_INFLUENCE, 0, 5)
        .answer(EE_UNITS_UB_SEARCH_TARGET, UNIT, 99);
    assert_eq!(find_target_near(&mut h, UNIT, 10, 20, 1.0), 99);
    assert_eq!(h.find(EE_UNITS_UB_AREA_TOP)[0].3, 17);
    assert_eq!(h.find(EE_UNITS_UB_AREA_BOTTOM)[0].3, 23);
    assert_eq!(h.find(EE_UNITS_UB_AREA_LEFT)[0].3, 7);
    assert_eq!(h.find(EE_UNITS_UB_AREA_RIGHT)[0].3, 13);
    let search = h.find(EE_UNITS_UB_SEARCH_TARGET)[0];
    assert_eq!((search.3, search.4, flt(search.5)), (10, 20, 1.0));
}

#[test]
fn a_unit_without_attack_searches_nothing() {
    let mut h = Mock::default().answer(EE_UNITS_UB_RANGE, UNIT, bits(2.0));
    assert_eq!(find_target_near(&mut h, UNIT, 1, 1, 1.0), 0);
    assert_eq!(h.count(EE_UNITS_UB_SUM_OTHERS_INFLUENCE), 0);
}

#[test]
fn update_unit_tile_snaps_an_off_map_unit_and_follows_its_waypoint_height() {
    let mut h = Mock::default()
        .floats(EE_UNITS_UB_TRANSFORMATION_POSITION, [-1.5, 2.5, 0.0, 0.0])
        .answer(EE_UNITS_UB_READ_LONG, 100, 0)
        .answer(EE_UNITS_UB_READ_LONG, 101, 3)
        .answer(EE_UNITS_UB_UNIT_CLASS, UNIT, CLASS as i64)
        .answer(EE_UNITS_UB_WAYPOINT_COUNT, UNIT, 1)
        .answer(EE_UNITS_UB_DOMAINS, CLASS, MOVEMENT_AIR)
        .answer(EE_UNITS_UB_HEIGHT_AT, UNIT, bits(4.0));
    update_unit_tile(&mut h, 50, UNIT, 100, 101);
    // UMFloor(-1.5) = -2, UMFloor(2.5) = 2
    assert_eq!(
        h.find(EE_UNITS_UB_SET_LONG),
        vec![(EE_UNITS_UB_SET_LONG, 100, 0, -2, 0, 0), (EE_UNITS_UB_SET_LONG, 101, 0, 2, 0, 0)]
    );
    assert_eq!(h.count(EE_UNITS_UB_SET_TRANSFORMATION), 0);
    assert_eq!(h.find(EE_UNITS_UB_MOVE_TO)[0].3, 0);
    assert_eq!(h.find(EE_UNITS_UB_MOVE_TO)[0].4, 3);
    assert_eq!(h.find(EE_UNITS_UB_SET_HEIGHT)[0].3, bits(4.0));
}

#[test]
fn update_unit_tile_moves_a_unit_on_the_map() {
    let mut h = Mock::default().floats(EE_UNITS_UB_TRANSFORMATION_POSITION, [1.5, 2.5, 0.0, 0.0]).answer(
        EE_UNITS_UB_TILE_EXISTS,
        0,
        1,
    );
    update_unit_tile(&mut h, 50, UNIT, 100, 101);
    assert_eq!(h.find(EE_UNITS_UB_SET_TRANSFORMATION), vec![(EE_UNITS_UB_SET_TRANSFORMATION, UNIT, 50, 0, 0, 0)]);
    assert_eq!(h.count(EE_UNITS_UB_CLOSEST_TILE), 0);
}

#[test]
fn for_each_unit_of_walks_the_units_then_the_pending_ones() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UB_FIRST_UNIT, PLAYER, 1)
        .answer(EE_UNITS_UB_NEXT_UNIT, 1, 2)
        .answer(EE_UNITS_UB_COMPLEX_OF, 1, 1)
        .answer(EE_UNITS_UB_NUM_PENDING, PLAYER, 1)
        .answer_c(EE_UNITS_UB_PENDING_UNIT, 0, 7)
        .answer(EE_UNITS_UB_COMPLEX_OF, 7, 7);
    for_each_unit_of(&mut h, PLAYER, 5, true);
    let applied: Vec<(i32, u64)> = h
        .log
        .iter()
        .filter(|e| e.0 == EE_UNITS_UB_APPLY || e.0 == EE_UNITS_UB_APPLY_TO_UNIT)
        .map(|e| (e.0, e.2))
        .collect();
    assert_eq!(
        applied,
        vec![
            (EE_UNITS_UB_APPLY_TO_UNIT, 1),
            (EE_UNITS_UB_APPLY, 1),
            (EE_UNITS_UB_APPLY_TO_UNIT, 2),
            (EE_UNITS_UB_APPLY_TO_UNIT, 7),
            (EE_UNITS_UB_APPLY, 7)
        ]
    );
}

#[test]
fn the_hit_point_upgrade_scales_the_upgrade_then_the_units() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UB_UPGRADE_BASE_HIT_POINTS, 60, 100)
        .answer(EE_UNITS_UB_UPGRADE_MAX_HIT_POINTS, 60, 200)
        .answer(EE_UNITS_UB_MAKE_UPGRADER, 0, 8);
    upgrade_hit_points(&mut h, 60, 3, 0.5, 0);
    let make = h.find(EE_UNITS_UB_MAKE_UPGRADER)[0];
    assert_eq!((make.3, flt(make.4)), (3, 1.25));
    assert_eq!(h.find(EE_UNITS_UB_SET_UPGRADE_MAX_HIT_POINTS)[0].3, 250);
    // a NULL player: nothing walked
    assert_eq!(h.count(EE_UNITS_UB_FIRST_UNIT), 0);
}

#[test]
fn the_functor_scales_a_living_unit_of_its_type() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UB_UNIT_CLASS, UNIT, CLASS as i64)
        .answer(EE_UNITS_UB_CLASS_TYPE, CLASS, 3)
        .answer(EE_UNITS_UB_UPGRADER_TYPE, 8, 3)
        .answer(EE_UNITS_UB_HIT_POINTS, UNIT, 3)
        .answer(EE_UNITS_UB_UPGRADER_RATIO, 8, bits(1.5));
    hit_point_apply(&mut h, 8, UNIT);
    // lrintf(4.5) = 4
    assert_eq!(h.find(EE_UNITS_UB_SET_HIT_POINTS)[0].3, 4);
}

#[test]
fn the_upgrade_text_needs_both_strings() {
    let mut h = Mock::default().floats(EE_UNITS_UB_LOOKUP_LEVEL, [0.105, 0.0, 0.0, 0.0]).answer(
        EE_UNITS_UB_CATEGORY_NAME_ID,
        0,
        1,
    );
    assert!(upgrade_text(&mut h, 9, 70));
    // (long)(0.105 * 100 + 0.5) = 11
    assert_eq!(h.find(EE_UNITS_UB_LEVEL_TEXT), vec![(EE_UNITS_UB_LEVEL_TEXT, 70, 0, 11, 0, 0)]);
    let mut h = Mock::default().answer(EE_UNITS_UB_CATEGORY_NAME_ID, 0, 1).answer(EE_UNITS_UB_ATTRIBUTE_STRING, 0, 1);
    assert!(!upgrade_text(&mut h, 9, 70));
    assert_eq!(h.count(EE_UNITS_UB_LEVEL_TEXT) + h.count(EE_UNITS_UB_POWER_TEXT), 0);
}

#[test]
fn the_waiting_gatherer_is_the_first_waiting_gather_goal_on_the_unit() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UB_LIST_SIZE, 0, 2)
        .answer_c(EE_UNITS_UB_LIST_UNIT, 0, 1)
        .answer_c(EE_UNITS_UB_LIST_UNIT, 1, 2)
        .answer(EE_UNITS_UB_COMPLEX_OF, 1, 1)
        .answer(EE_UNITS_UB_COMPLEX_OF, 2, 2)
        .answer(EE_UNITS_UB_COMMAND, 2, ACTION_WAIT_FOR_OPEN_POSITION)
        .answer(EE_UNITS_UB_GOAL_STATE, 2, 80)
        .answer(EE_UNITS_UB_GOAL_KIND, 80, GOAL_GATHER)
        .answer(EE_UNITS_UB_GATHER_GOAL, 80, 81)
        .answer(EE_UNITS_UB_GATHER_RESOURCE, 81, UNIT as i64)
        .answer(EE_UNITS_UB_RANDOM_GENERATE, 0, bits(2.5));
    assert_eq!(waiting_gatherer(&mut h, UNIT), 2);
    assert_eq!(h.find(EE_UNITS_UB_PERMUTE_UNITS)[0].3, 2);
}

#[test]
fn the_class_graphic_follows_the_tile_size() {
    for (size, upgrade) in [(1, FOUNDATION_1), (2, FOUNDATION_2), (4, FOUNDATION_3), (5, UNIT_TYPE_FOUNDATION6X6)] {
        let mut h = Mock::default().answer(EE_UNITS_UB_TILE_SIZE, CLASS, size);
        player_class_graphic(&mut h, PLAYER, CLASS);
        assert_eq!(h.find(EE_UNITS_UB_UPGRADE_MODEL)[0].3, upgrade);
    }
    let mut h = Mock::default().answer(EE_UNITS_UB_CLASS_TYPE, CLASS, UNIT_TYPE_ROBOTIC_FARM);
    player_class_graphic(&mut h, PLAYER, CLASS);
    assert_eq!(h.find(EE_UNITS_UB_TABLE_MODEL)[0].3, UNIT_TYPE_SPACE_FALLOW_FARM);
}

#[test]
fn a_linked_target_hands_the_attack_to_its_next_live_piece() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UB_NEXT_PART, 60, 61)
        .answer(EE_UNITS_UB_OUTERMOST_CARRIER, 60, 60)
        .answer(EE_UNITS_UB_NEXT_PART, 61, 62)
        .answer(EE_UNITS_UB_LOAD_ATTACK_GOAL, UNIT, 70)
        .answer(EE_UNITS_UB_ATTACK_TARGET, 70, 60);
    find_new_target_near_old(&mut h, UNIT, 60);
    assert_eq!(h.find(EE_UNITS_UB_GOAL_REPLACE_UNIT), vec![(EE_UNITS_UB_GOAL_REPLACE_UNIT, 70, 60, 61, 0, 0)]);
    assert_eq!(h.find(EE_UNITS_UB_START_ACTION)[0].3, ACTION_CHECK_RANGE);
    assert_eq!(h.count(EE_UNITS_UB_FIND_NEW_TARGET), 0);
}

#[test]
fn a_lone_target_searches_two_tiles_around_it() {
    let mut h = Mock::default().answer(EE_UNITS_UB_TILE_X, 60, 5).answer(EE_UNITS_UB_TILE_Y, 60, 6);
    find_new_target_near_old(&mut h, UNIT, 60);
    assert_eq!(h.find(EE_UNITS_UB_FIND_NEW_TARGET), vec![(EE_UNITS_UB_FIND_NEW_TARGET, 60, UNIT, 5, 6, 2)]);
    assert_eq!(h.count(EE_UNITS_UB_NO_TARGET_TRACE), 1);
}

#[test]
fn the_constants_are_the_headers() {
    // CIV_POWER_* are checked here (header_consts.rs does not know the prefix)
    let text =
        std::fs::read_to_string(concat!(env!("CARGO_MANIFEST_DIR"), "/../../../include/ee/EECivPowerIDs.h")).unwrap();
    for (name, value) in [
        ("kCivPowerFlamingArrows", CIV_POWER_FLAMING_ARROWS),
        ("kCivPowerCrusaders", CIV_POWER_CRUSADERS),
        ("kCivPowerEmissaries", CIV_POWER_EMISSARIES),
        ("kCivPowerAdaptation", CIV_POWER_ADAPTATION),
    ] {
        let line = text.lines().find(|l| l.trim_start().starts_with(&format!("{name} ="))).unwrap();
        let v = line.split('=').nth(1).unwrap().split(',').next().unwrap().trim();
        let parsed = if let Some(hex) = v.strip_prefix("0x") {
            i64::from_str_radix(hex, 16).unwrap()
        } else {
            v.parse().unwrap()
        };
        assert_eq!(parsed, value, "{name}");
    }
}
