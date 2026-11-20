//! The cinematics and the round's dialog statements as streams over a recording round.

use std::collections::HashMap;

use super::*;
use crate::scenario::manager::{EeScenarioManagerFields, EeScenarioTimedQueue};

#[derive(Default)]
struct Log {
    f: EeScenarioManagerFields,
    q: EeScenarioTimedQueue,
    answers: HashMap<(u32, i64), i64>,
    triggers: Vec<EeScenarioTriggerFields>,
    ops: Vec<(u32, i64, i64)>,
}

impl Round for Log {
    fn op(&mut self, op: u32, a: i64, b: i64, _time: f32) -> i64 {
        self.ops.push((op, a, b));
        if op == EE_SCENARIO_ROUND_TRIGGER_FIELDS {
            return &mut self.triggers[a as usize] as *mut _ as usize as i64;
        }
        if op == EE_SCENARIO_ROUND_TRIGGER_COUNT {
            return self.triggers.len() as i64;
        }
        if let Some(v) = compose_for_test(self, op, a) {
            return v;
        }
        *self.answers.get(&(op, a)).or_else(|| self.answers.get(&(op, -1))).unwrap_or(&0)
    }
    fn fields<R>(&mut self, f: impl FnOnce(&mut EeScenarioManagerFields) -> R) -> R {
        f(&mut self.f)
    }
    fn queue<R>(&mut self, f: impl FnOnce(&mut EeScenarioTimedQueue) -> R) -> R {
        f(&mut self.q)
    }
}

/// The table answers without composing (the composed operations are tested by calling [`compose`]).
fn compose_for_test(_h: &mut Log, _op: u32, _a: i64) -> Option<i64> {
    None
}

fn ops(l: &Log) -> Vec<u32> {
    l.ops.iter().map(|o| o.0).collect()
}

#[test]
fn an_end_outside_a_cinematic_changes_nothing() {
    let mut l = Log::default();
    end_cinematic(&mut l);
    assert_eq!(ops(&l), vec![EE_SCENARIO_ROUND_IN_CINEMATIC]);
}

#[test]
fn the_end_delays_the_other_triggers_and_restores_the_camera() {
    let mut l = Log::default();
    l.f.mTime = 10_500.0;
    l.f.mCinematicStartTime = 2_000.0;
    l.f.mSavedCameraMode = 2;
    l.f.mSavedZoom = 0.5;
    l.answers.insert((EE_SCENARIO_ROUND_IN_CINEMATIC, 0), 1);
    l.triggers = vec![
        EeScenarioTriggerFields { mConditionDuration: 5, mConditionStartTime: 100, ..Default::default() },
        EeScenarioTriggerFields {
            mConditionDuration: 5,
            mConditionStartTime: 100,
            mIsInCinematic: true,
            ..Default::default()
        },
        EeScenarioTriggerFields { mConditionDuration: 0, mConditionStartTime: 100, ..Default::default() },
    ];
    l.q.add(0, 9_000, false);
    l.q.add(1, 9_000, true);
    end_cinematic(&mut l);
    assert_eq!(
        l.triggers.iter().map(|t| t.mConditionStartTime).collect::<Vec<_>>(),
        vec![100 + 8_500 + 3_000, 100, 100]
    );
    assert!(l.f.mIsEndingCinematic);
    assert_eq!(l.f.mCinematicStartTime, 0.0);
    assert!(l.ops.contains(&(EE_SCENARIO_ROUND_CAMERA_SET_MODE, 2, 0)));
    assert!(l.ops.contains(&(EE_SCENARIO_ROUND_CAMERA_SET_ZOOM, 0, 0)));
    assert_eq!(*ops(&l).last().unwrap(), EE_SCENARIO_ROUND_CAMERA_UPDATE);
    // the latency first, the letterbox after the wonders
    assert_eq!(l.ops[1], (EE_SCENARIO_ROUND_LATENCY, 100, 0));
}

#[test]
fn dropped_messages_go_from_the_last() {
    let mut l = Log::default();
    l.answers.insert((EE_SCENARIO_ROUND_MESSAGE_COUNT, 0), 3);
    l.answers.insert((EE_SCENARIO_ROUND_MESSAGE_CINEMATIC, 2), 1);
    l.answers.insert((EE_SCENARIO_ROUND_MESSAGE_CINEMATIC, 0), 1);
    start_cinematic(&mut l);
    let erased: Vec<i64> = l.ops.iter().filter(|o| o.0 == EE_SCENARIO_ROUND_MESSAGE_ERASE).map(|o| o.1).collect();
    // the start drops the messages that are not a cinematic's
    assert_eq!(erased, vec![1]);
    assert_eq!(l.f.mCinematicStartTime, l.f.mTime);
}

#[test]
fn a_start_unselects_every_player_but_nature() {
    let mut l = Log::default();
    l.answers.insert((EE_SCENARIO_ROUND_NUM_PLAYERS, 0), 3);
    l.answers.insert((EE_SCENARIO_ROUND_CAMERA_MODE, 0), 4);
    start_cinematic(&mut l);
    let unselected: Vec<i64> = l.ops.iter().filter(|o| o.0 == EE_SCENARIO_ROUND_UNSELECT).map(|o| o.1).collect();
    assert_eq!(unselected, vec![1, 2]);
    assert_eq!(l.f.mSavedCameraMode, 4);
}

#[test]
fn a_shown_message_moves_the_camera_only_with_a_unit_and_a_scroll_speed() {
    let mut l = Log::default();
    l.answers.insert((EE_SCENARIO_ROUND_MESSAGE_UNIT, 1), 1);
    l.answers.insert((EE_SCENARIO_ROUND_MESSAGE_SCROLL, 1), 0);
    compose(&mut l, EE_SCENARIO_ROUND_SHOW_MESSAGE, 1);
    assert!(ops(&l).contains(&EE_SCENARIO_ROUND_MESSAGE_MOVE_TO_FACING));
    assert_eq!(*ops(&l).last().unwrap(), EE_SCENARIO_ROUND_MESSAGE_ERASE);
    let mut l = Log::default();
    l.answers.insert((EE_SCENARIO_ROUND_MESSAGE_UNIT, 1), 1);
    l.answers.insert((EE_SCENARIO_ROUND_MESSAGE_SCROLL, 1), -1);
    compose(&mut l, EE_SCENARIO_ROUND_SHOW_MESSAGE, 1);
    assert!(!ops(&l).contains(&EE_SCENARIO_ROUND_CAMERA_SET_MODE));
}

#[test]
fn a_human_plays_unless_observing() {
    let mut l = Log::default();
    l.answers.insert((EE_SCENARIO_ROUND_NUM_PLAYERS, 0), 2);
    l.answers.insert((EE_SCENARIO_ROUND_PLAYER_EXISTS, -1), 1);
    l.answers.insert((EE_SCENARIO_ROUND_PLAYER_HUMAN, 1), 1);
    assert_eq!(compose(&mut l, EE_SCENARIO_ROUND_HUMAN_PLAYING, 0), Some(1));
    l.answers.insert((EE_SCENARIO_ROUND_PLAYER_OBSERVER, 1), 1);
    assert_eq!(compose(&mut l, EE_SCENARIO_ROUND_HUMAN_PLAYING, 0), Some(0));
}

#[test]
fn the_pending_effects_replay_and_the_media_and_units_walk() {
    let mut l = Log::default();
    l.f.mB234 = true;
    l.answers.insert((EE_SCENARIO_ROUND_PENDING_COUNT, 0), 2);
    l.answers.insert((EE_SCENARIO_ROUND_PENDING_EXISTS, 1), 1);
    apply_pending(&mut l);
    assert_eq!(l.ops.iter().filter(|o| o.0 == EE_SCENARIO_ROUND_PENDING_EXECUTE).count(), 1);
    assert_eq!((l.f.mTriggerTimer, l.f.mB234), (TRIGGER_INTERVAL, false));
    let mut l = Log::default();
    l.answers.insert((EE_SCENARIO_ROUND_MEDIA_COUNT, 0), 2);
    unload_media(&mut l, false);
    assert!(!ops(&l).contains(&EE_SCENARIO_ROUND_MEDIA_CLEAR));
    unload_media(&mut l, true);
    assert_eq!(*ops(&l).last().unwrap(), EE_SCENARIO_ROUND_MEDIA_CLEAR);
    let mut l = Log::default();
    l.answers.insert((EE_SCENARIO_ROUND_OBJECT_COUNT, 0), 1);
    replace_unit(&mut l, 7, -1);
    assert_eq!(l.ops[1], (EE_SCENARIO_ROUND_OBJECT_REPLACE, 0, (7i64 << 32) | 0xffff_ffff));
}
