use super::*;
use crate::units::uc_mock::Mock;

#[test]
fn a_paused_unit_only_counts_its_hold() {
    // a paused, non-looping state: the clock asks nothing more than the reads; no sound
    let mut h = Mock::default().answer(EE_UNITS_UC_PAUSED, 1).answer(EE_UNITS_UC_TICK_MS, 100);
    let steps = animate(&mut h, 1, 2);
    assert_eq!(steps, 0);
    assert!(!h.has(EE_UNITS_UC_IS_FOUNDATION));
    assert!(!h.has(EE_UNITS_UC_GET_VELOCITY));
    assert!(!h.has(EE_UNITS_UC_SET_NEXT_FRAME));
}

#[test]
fn a_running_unit_steps_its_frames_and_sets_the_next() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_TICK_MS, 100)
        .answer(EE_UNITS_UC_MODEL_FRAME_TIME, 50)
        .answer(EE_UNITS_UC_MODEL_FRAME_COUNT, 10)
        .answer(EE_UNITS_UC_MODEL_LAST_FRAME, 9)
        .answer(EE_UNITS_UC_MODEL_LOOPS, 1);
    animate(&mut h, 1, 2);
    assert!(h.has(EE_UNITS_UC_SET_NEXT_FRAME));
    assert!(!h.has(EE_UNITS_UC_IS_FOUNDATION));
}

#[test]
fn a_shot_fires_when_the_frame_crosses_the_sound_frame() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_ATTACK_FRAME_SET, 3)
        .answer(EE_UNITS_UC_ANIMATION, 3)
        .answer(EE_UNITS_UC_MODEL_SOUND1_FRAME, 5)
        .answer(EE_UNITS_UC_MODEL_SOUND2_FRAME, 50)
        .answer(EE_UNITS_UC_FRAME, 4);
    fire_on_attack_frame(&mut h, 1, 2, 1, 6);
    assert!(h.has(EE_UNITS_UC_FIRE_SHOT));
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_ATTACK_FRAME_SET, 3)
        .answer(EE_UNITS_UC_ANIMATION, 3)
        .answer(EE_UNITS_UC_MODEL_SOUND1_FRAME, 5)
        .answer(EE_UNITS_UC_MODEL_SOUND2_FRAME, 50)
        .answer(EE_UNITS_UC_FRAME, 6);
    fire_on_attack_frame(&mut h, 1, 2, 0, 7);
    assert!(!h.has(EE_UNITS_UC_FIRE_SHOT));
}

#[test]
fn turrets_without_a_combat_goal_turn_back_and_only_changed_angles_are_stored() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_HIT_POINTS, 10)
        .answer(EE_UNITS_UC_TURRET_PART_COUNT, 2)
        .float(EE_UNITS_UC_TURRET_RATE, [10.0, 0.0, 0.0, 0.0])
        .float(EE_UNITS_UC_TICK_SECONDS, [0.1, 0.0, 0.0, 0.0]);
    turn_turrets(&mut h, 1, 2);
    assert!(!h.has(EE_UNITS_UC_GOAL_KIND));
    assert_eq!(h.count(EE_UNITS_UC_TURRET_PART_POSITION), 2);
    // both angles start at 0 and stay there
    assert!(!h.has(EE_UNITS_UC_SET_TURRET_ANGLE));
}

#[test]
fn a_dead_unit_does_not_turn_its_turrets_back() {
    let mut h = Mock::default().answer(EE_UNITS_UC_TURRET_PART_COUNT, 2);
    turn_turrets_back(&mut h, 1, 2);
    assert!(!h.has(EE_UNITS_UC_TURRET_PART_POSITION));
}

#[test]
fn turrets_aim_at_a_simple_target_tile() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_GOAL_STATE_SELF, 5)
        .answer(EE_UNITS_UC_GOAL_KIND, GOAL_ATTACK_UNIT)
        .answer(EE_UNITS_UC_ATTACK_TARGET, 9)
        .answer(EE_UNITS_UC_UNIT_TILE_X, 3);
    turn_turrets(&mut h, 1, 2);
    assert!(h.has(EE_UNITS_UC_UNIT_TILE_X));
    assert!(!h.has(EE_UNITS_UC_LOCATION));
}

#[test]
fn the_trail_timer_counts_down_then_fires() {
    let mut h = Mock::default().answer(EE_UNITS_UC_TRAIL_TIMER, 30).answer(EE_UNITS_UC_TICK_MS, 100);
    update_trail(&mut h, 1, 2);
    assert_eq!(h.args(EE_UNITS_UC_SET_TRAIL_TIMER), [(0, 0, 0)]);
    let mut h = Mock::default();
    update_trail(&mut h, 1, 2);
    assert_eq!(h.ops()[1..], [EE_UNITS_UC_TRAIL_EFFECT_ON, EE_UNITS_UC_RESET_TRAIL_TIMER]);
}

#[test]
fn ripples_wait_on_land_and_count_on_water() {
    let mut h = Mock::default().answer(EE_UNITS_UC_UNIT_TILE, 4).answer(EE_UNITS_UC_TILE_TERRAIN, 0);
    update_ripples(&mut h, 1, 2);
    assert!(!h.has(EE_UNITS_UC_WAKE_TIMER));
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_UNIT_TILE, 4)
        .answer(EE_UNITS_UC_TILE_TERRAIN, TERRAIN_DEEP_WATER)
        .answer(EE_UNITS_UC_RIPPLE_TIME2, 1000)
        .answer(EE_UNITS_UC_TICK_MS, 100);
    update_ripples(&mut h, 1, 2);
    assert_eq!(h.args(EE_UNITS_UC_SET_WAKE_TIMER), [(0, 100, 0)]);
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_UNIT_TILE, 4)
        .answer(EE_UNITS_UC_TILE_TERRAIN, TERRAIN_SHALLOW_WATER)
        .answer(EE_UNITS_UC_WAKE_TIMER, 5);
    update_ripples(&mut h, 1, 2);
    assert!(h.has(EE_UNITS_UC_RIPPLE_TAGS));
    assert_eq!(h.args(EE_UNITS_UC_SET_WAKE_TIMER), [(0, 0, 0)]);
}

#[test]
fn ripples_appear_only_under_water() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_RIPPLE_TAGS, 3)
        .sequence(EE_UNITS_UC_TAGS_SIZE, &[2, 2, 2, 2])
        .answer(EE_UNITS_UC_SEEN_BY_CURRENT, 1)
        .answer(EE_UNITS_UC_GET_TILE, 8)
        .float(EE_UNITS_UC_TILE_HEIGHT_OF, [-1.0, 0.0, 0.0, 0.0])
        .float(EE_UNITS_UC_TAG_POINT_AT, [1.0, 2.0, -0.5, 0.0]);
    create_ripples(&mut h, 1, 2);
    assert_eq!(h.count(EE_UNITS_UC_RIPPLE_EFFECT_AT), 2);
    let mut h = Mock::default().answer(EE_UNITS_UC_RIPPLE_TAGS, 3).answer(EE_UNITS_UC_TAGS_SIZE, 2);
    create_ripples(&mut h, 1, 2);
    assert!(!h.has(EE_UNITS_UC_LOCATION_MATRIX));
}

#[test]
fn a_garrison_heals_each_due_tick_and_refreshes_the_panel() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_HEALING_POINTS, 5)
        .answer(EE_UNITS_UC_CARGO_COUNT, 2)
        .answer(EE_UNITS_UC_COMBAT_TIMER, 1)
        .answer(EE_UNITS_UC_GET_HIT_POINTS, 10)
        .answer(EE_UNITS_UC_COMBAT_HEAL, 1)
        .answer(EE_UNITS_UC_HEAL_HIT_POINTS, 7)
        .answer(EE_UNITS_UC_IS_CURRENT_PLAYER, 1)
        .answer(EE_UNITS_UC_GROUP_CONTAINS, 1)
        .answer(EE_UNITS_UC_TIMER_VALUE, 3);
    heal_garrison(&mut h, 1, 2);
    assert_eq!(h.count(EE_UNITS_UC_SET_HIT_POINTS_OF), 2);
    assert!(h.has(EE_UNITS_UC_RESET_GROUP_PANEL));
    assert_eq!(h.args(EE_UNITS_UC_SET_REGENERATION_TIMER), [(0, 3, 0)]);
}

#[test]
fn no_garrison_healing_in_a_cinematic() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_HEALING_POINTS, 5)
        .answer(EE_UNITS_UC_CARGO_COUNT, 2)
        .answer(EE_UNITS_UC_WORLD_IN_CINEMATIC, 1);
    heal_garrison(&mut h, 1, 2);
    assert!(!h.has(EE_UNITS_UC_COMBAT_TIMER));
}

#[test]
fn regeneration_sets_the_hit_points_and_the_timer() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_COMBAT_REGENERATE, 1)
        .answer(EE_UNITS_UC_REGENERATE_HIT_POINTS, 9)
        .answer(EE_UNITS_UC_REGENERATE_TIMER, 4);
    regenerate(&mut h, 1, 2);
    assert_eq!(h.args(EE_UNITS_UC_SET_HIT_POINTS_SELF), [(0, 9, 0)]);
    assert!(!h.has(EE_UNITS_UC_RESET_GROUP_PANEL));
    assert_eq!(h.args(EE_UNITS_UC_SET_REGENERATION_TIMER), [(0, 4, 0)]);
}

#[test]
fn a_placed_unit_joins_the_region_of_its_tile() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_TILE_NODE, 1)
        .answer(EE_UNITS_UC_UNIT_TILE_X, 17)
        .answer(EE_UNITS_UC_UNIT_TILE_Y, 9)
        .answer(EE_UNITS_UC_COMPLEX_OF, 55);
    h.region_size = 8;
    region(&mut h, 1, 2, true);
    region(&mut h, 1, 2, false);
    assert_eq!(h.regions, [(true, 2, 1, 55), (false, 2, 1, 55)]);
    let mut h = Mock::default().answer(EE_UNITS_UC_TILE_NODE, 1).answer(EE_UNITS_UC_IS_PATH_MARKER, 1);
    region(&mut h, 1, 2, true);
    assert!(h.regions.is_empty());
}

#[test]
fn the_classes_in_a_set_are_listed() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_CLASS_COUNT, 3)
        .sequence(EE_UNITS_UC_CLASS_AT, &[0, 11, 12])
        .sequence(EE_UNITS_UC_SET_CONTAINS, &[1, 0]);
    classes_in_set(&mut h, 7, 99);
    assert_eq!(h.args(EE_UNITS_UC_CLASSES_PUSH), [(99, 11, 0)]);
    assert_eq!(h.ops()[0], EE_UNITS_UC_CLASSES_CLEAR);
}
