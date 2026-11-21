use super::*;
use crate::units::uc_mock::Mock;

impl UcpRust for Mock {
    fn countdown(&mut self, _id: u32, _object: u64, which: u32, member: i64, tick_ms: i64) -> Option<i64> {
        self.log.push((-1, u64::from(which), member, tick_ms));
        (member > 0).then_some((member - tick_ms).max(0))
    }
    fn heal_step(&mut self, _id: u32, _object: u64, member: f32, tick_time: f32) -> Option<f32> {
        (member > 0.0).then_some((member - tick_time).max(0.0))
    }
    fn rotting(&mut self, _id: u32, _object: u64, member: i64, tick_ms: i64) -> i64 {
        (member - tick_ms).max(0)
    }
    fn resources_rotting(&mut self, _id: u32, _object: u64, time: i64, amount: i64, tick_ms: i64) -> (i64, Option<i64>) {
        let t = time + tick_ms;
        if t > 4000 {
            (t - 4000, Some(amount - 1))
        } else {
            (t, None)
        }
    }
    fn note_step(&mut self, kind: u32, id: u32, words: &[u32]) {
        self.log.push((-2, u64::from(kind), i64::from(id), words.len() as i64));
    }
}

#[test]
fn a_researched_effect_idles_a_unit_with_other_goals_and_spares_a_survivor() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_HIT_POINTS, 10)
        .answer(EE_UNITS_UCP_QUEUE0_LEN, 2)
        .sequence(EE_UNITS_UCP_QUEUE0_KIND, &[1, 4]);
    play_unit_effect(&mut h, 1, 2, 77);
    assert_eq!(h.args(EE_UNITS_UC_START_ACTION_SELF), [(0, ACTION_IDLE, 0)]);
    assert_eq!(h.args(EE_UNITS_UCP_RESEARCH), [(0, 77, 0)]);
    assert!(!h.has(EE_UNITS_UCP_REMOVAL_SOUND));
}

#[test]
fn a_unit_removed_by_its_research_gets_the_epoch_media() {
    let mut h = Mock::default()
        .sequence(EE_UNITS_UC_HIT_POINTS, &[10, 0])
        .answer(EE_UNITS_UCP_REMOVAL_SOUND, 5)
        .answer(EE_UNITS_UCP_REMOVAL_EFFECT_COUNT, 2)
        .float(EE_UNITS_UCP_SOUND_AT, [0.5, 0.0, 1.0, 0.0])
        .float(EE_UNITS_UCP_REMOVAL_PITCH_OFFSET, [-3.0, 0.0, 0.0, 0.0])
        .answer(EE_UNITS_UCP_THUNDER_EVERY, 2)
        .answer(EE_UNITS_UC_UNIT_ID, 4)
        .answer(EE_UNITS_UCP_THUNDER_SOUND, 6);
    play_unit_effect(&mut h, 1, 2, 77);
    assert_eq!(h.count(EE_UNITS_UCP_REMOVAL_EFFECT), 2);
    // the pitch offset is held at -1
    assert_eq!(h.floats_of(EE_UNITS_UCP_SOUND_PLAY)[0][2], -1.0);
    assert_eq!(h.args(EE_UNITS_UCP_SOUND_PLAY).iter().map(|e| e.0).collect::<Vec<_>>(), [5, 6]);
}

#[test]
fn the_work_timer_counts_down_and_a_due_gather_is_worked() {
    let mut h = Mock::default()
        .sequence(EE_UNITS_UCP_WORK_TIMER, &[50, 0, 300, 300])
        .answer(EE_UNITS_UC_TICK_MS, 100)
        .answer(EE_UNITS_UC_COMMAND_KIND, ACTION_GATHER_RESOURCE)
        .answer(EE_UNITS_UC_GOAL_STATE_SELF, 9)
        .answer(EE_UNITS_UCP_CURRENT_RESEARCH, TECH_NONE);
    update_citizen(&mut h, 1, 2);
    // 50 - 100: the timer stops at 0; the work's new 300 keeps the overshoot (-50)
    assert_eq!(h.args(EE_UNITS_UCP_SET_WORK_TIMER), [(0, 0, 0), (0, 250, 0)]);
    assert!(h.has(EE_UNITS_UCP_GOAL_WORK));
    assert_eq!(*h.ops().last().unwrap(), EE_UNITS_UCP_CURRENT_RESEARCH);
}

#[test]
fn a_due_research_finishes_and_a_bundeswehr_tech_plays_its_effect() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UCP_CURRENT_RESEARCH, 400)
        .answer(EE_UNITS_UCP_RESEARCH_NODE, 8)
        .answer(EE_UNITS_UCP_NODE_KIND, 3)
        .answer(EE_UNITS_UCP_GET_BUNDESWEHR, 1)
        .answer(EE_UNITS_UCP_POWER_IS_PREREQUISITE, 1);
    update_citizen(&mut h, 1, 2);
    // the effect's own research only (the unit's hit points are 0: it bails out before researching)
    assert!(!h.has(EE_UNITS_UCP_RESEARCH));
    assert!(h.has(EE_UNITS_UCP_RESET_HANDLE));
    let mut h = Mock::default()
        .answer(EE_UNITS_UCP_CURRENT_RESEARCH, 400)
        .answer(EE_UNITS_UC_PRODUCTION_TIME, 5000)
        .answer(EE_UNITS_UC_TICK_MS, 100);
    update_citizen(&mut h, 1, 2);
    assert!(!h.has(EE_UNITS_UCP_RESEARCH_NODE));
    let mut h = Mock::default()
        .answer(EE_UNITS_UCP_CURRENT_RESEARCH, 400)
        .answer(EE_UNITS_UC_PRODUCTION_TIME, 5000)
        .answer(EE_UNITS_UC_INSTANT_BUILD, 1)
        .answer(EE_UNITS_UCP_RESEARCH_NODE, 8);
    update_citizen(&mut h, 1, 2);
    assert_eq!(h.args(EE_UNITS_UCP_RESEARCH), [(0, 400, 0)]);
}

#[test]
fn a_priest_stops_converting_an_ally() {
    let mut h = Mock::default().answer(EE_UNITS_UCP_CONVERSION_TARGET, 7).answer(EE_UNITS_UCP_SAME_PLAYER, 1);
    update_conversion(&mut h, 1, 2);
    assert!(h.has(EE_UNITS_UCP_CLEAR_GOALS));
    assert_eq!(h.args(EE_UNITS_UC_START_ACTION_SELF), [(0, ACTION_IDLE, 0)]);
    assert!(!h.has(EE_UNITS_UCP_CONVERSION));
}

#[test]
fn a_priest_warns_the_owner_then_waits_out_the_minimum_time() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UCP_CONVERSION_TARGET, 7)
        .answer(EE_UNITS_UCP_ENEMY_STATE, 1)
        .answer(EE_UNITS_UCP_NO_CONVERSION_EFFECT, 1)
        .answer(EE_UNITS_UCP_HANDLE_DIFFERS, 1)
        .answer(EE_UNITS_UCP_TOO_SOON, 1);
    update_conversion(&mut h, 1, 2);
    assert!(h.has(EE_UNITS_UCP_KEEP_HANDLE));
    assert_eq!(h.args(EE_UNITS_UCP_CONVERSION_EVENT), [(7, 0, 0)]);
    assert!(!h.has(EE_UNITS_UCP_CONVERSION));
}

#[test]
fn a_conversion_not_due_rewinds_the_work_timer() {
    let mut h = Mock::default().answer(EE_UNITS_UCP_CONVERSION_TARGET, 7).answer(EE_UNITS_UCP_ENEMY_STATE, 1);
    update_conversion(&mut h, 1, 2);
    // the host wrote no outcome: not due, timer 0
    assert_eq!(h.args(EE_UNITS_UCP_SET_WORK_TIMER), [(0, 0, 0)]);
}

#[test]
fn a_conversion_converts_and_spends_the_faith() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UCP_CONVERT_UNIT, 50)
        .answer(EE_UNITS_UCP_STATS_OF, 60)
        .answer(EE_UNITS_UCP_STATS_CREATION_SOUND, 70)
        .answer(EE_UNITS_UCP_UNIT_PLAYER_CURRENT, 1);
    convert_target(&mut h, 1, 2, 7);
    assert_eq!(h.args(EE_UNITS_UCP_CONVERT_UNIT), [(7, 0, 0)]);
    assert!(h.at(EE_UNITS_UCP_SPEND_FAITH) > h.at(EE_UNITS_UCP_CONVERT_UNIT));
    assert_eq!(h.args(EE_UNITS_UCP_PLAY_FULL), [(70, 0, 0)]);
}

#[test]
fn area_conversion_walks_the_seen_units_backwards() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UCP_AREA_CONVERSION, 1)
        .sequence(EE_UNITS_UCP_CONVERT_UNIT, &[50])
        .answer(EE_UNITS_UCP_UNITS_LEN, 2)
        .sequence(EE_UNITS_UCP_UNITS_AT, &[11, 12])
        .answer(EE_UNITS_UC_COMPLEX_OF, 13)
        .answer(EE_UNITS_UCP_TILE_SEEN, 1);
    convert_target(&mut h, 1, 2, 7);
    // the can-convert decision ran for each (the second listed first); none converts with no faith
    assert_eq!(h.count(EE_UNITS_UCP_CAN_CONVERT_CAPTURE), 2);
    assert_eq!(h.args(EE_UNITS_UC_COMPLEX_OF)[0].0, 12);
}

#[test]
fn can_convert_records_its_decision() {
    let mut h = Mock::default();
    let (result, _) = can_convert(&mut h, 2, 7);
    assert!(!result);
    assert!(h.has(EE_UNITS_UCP_CAN_CONVERT_CAPTURE));
    // the damage is read only for a valid ammunition column
    let reads: Vec<i64> = h.args(EE_UNITS_UCP_CAN_CONVERT_READ).iter().map(|e| e.1).collect();
    assert!(reads.contains(&EE_UNITS_UCP_CC_DAMAGE));
    let mut h = Mock::default().answer(EE_UNITS_UCP_CAN_CONVERT_READ, -1);
    can_convert(&mut h, 2, 7);
    let reads: Vec<i64> = h.args(EE_UNITS_UCP_CAN_CONVERT_READ).iter().map(|e| e.1).collect();
    assert!(!reads.contains(&EE_UNITS_UCP_CC_DAMAGE));
}

#[test]
fn only_a_temple_shows_and_hides_its_tower_effects() {
    let mut h = Mock::default();
    show_conversion(&mut h, 1, 2);
    hide_conversion(&mut h, 1, 2);
    assert_eq!(h.ops(), [EE_UNITS_UCP_CLASS_CATEGORY, EE_UNITS_UCP_CLASS_CATEGORY]);
    let mut h = Mock::default()
        .answer(EE_UNITS_UCP_CLASS_CATEGORY, CATEGORY_BUILDING_PRIEST)
        .answer(EE_UNITS_UCP_HAS_CLOAKED_EFFECT, 1)
        .answer(EE_UNITS_UCP_HAS_GROW_EFFECT, 1);
    show_conversion(&mut h, 1, 2);
    assert_eq!(h.args(EE_UNITS_UCP_CREATE_EFFECT), [(0, 0xd9, 0), (0, 0xda, 0)]);
    assert!(h.has(EE_UNITS_UCP_PLAY_TOWER_SOUND));
    hide_conversion(&mut h, 1, 2);
    assert!(h.has(EE_UNITS_UCP_CLEAR_CLOAKED));
    assert!(h.has(EE_UNITS_UCP_CLEAR_GROW_TIME));
}

#[test]
fn the_timers_step_and_note_while_capturing() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UCP_TIMER, 250)
        .answer(EE_UNITS_UC_TICK_MS, 100)
        .answer(EE_UNITS_UCP_CAPTURE_ACTIVE, 1);
    countdown_timer(&mut h, 2, false);
    assert_eq!(h.args(EE_UNITS_UCP_SET_TIMER), [(0, TIMER_RELOAD, 150)]);
    assert_eq!(h.args(-2), [(u64::from(STEP_KIND_RELOAD), 0, 4)]);
    let mut h = Mock::default();
    countdown_timer(&mut h, 2, true);
    assert!(!h.has(EE_UNITS_UCP_SET_TIMER));
    assert!(!h.has(-2));
    let mut h = Mock::default().answer(EE_UNITS_UCP_TIMER, 250).answer(EE_UNITS_UC_TICK_MS, 100);
    assert_eq!(rotting_time(&mut h, 2), 150);
    let mut h = Mock::default()
        .answer(EE_UNITS_UCP_TIMER, 3950)
        .answer(EE_UNITS_UC_TICK_MS, 100)
        .answer(EE_UNITS_UCP_CAPTURE_ACTIVE, 1);
    resources_rotting(&mut h, 2);
    assert_eq!(h.args(EE_UNITS_UCP_SET_TIMER), [(0, TIMER_RESOURCE_AMOUNT, 3949), (0, TIMER_ROTTING, 50)]);
    assert_eq!(h.args(-2), [(u64::from(STEP_KIND_RESOURCES_ROTTING), 0, 5)]);
    let mut h = Mock::default()
        .float(EE_UNITS_UCP_TIMER, [1.5, 0.0, 0.0, 0.0])
        .float(EE_UNITS_UCP_TICK_TIME, [0.5, 0.0, 0.0, 0.0]);
    heal_search_timer(&mut h, 2);
    assert_eq!(h.floats_of(EE_UNITS_UCP_SET_TIMER)[0][0], 1.0);
}
