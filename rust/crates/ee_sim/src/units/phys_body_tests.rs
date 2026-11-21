use super::*;
use std::collections::BTreeMap;

/// A recording host: each statement asked (op, a, b, c, d, f[0]), its answer from `answers` keyed by (op, a) or
/// (op, c) (0 else); `sequences` pop answers per op.
#[derive(Default)]
struct Mock {
    log: Vec<(i32, u64, u64, i64, i64, f32)>,
    answers: BTreeMap<(i32, u64), i64>,
    by_c: BTreeMap<(i32, i64), i64>,
    sequences: BTreeMap<i32, Vec<i64>>,
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
    fn sequence(mut self, op: i32, values: &[i64]) -> Self {
        self.sequences.insert(op, values.to_vec());
        self
    }
    fn ops(&self) -> Vec<i32> {
        self.log.iter().map(|e| e.0).collect()
    }
    fn count(&self, op: i32) -> usize {
        self.log.iter().filter(|e| e.0 == op).count()
    }
    fn find(&self, op: i32) -> Vec<(i32, u64, u64, i64, i64, f32)> {
        self.log.iter().filter(|e| e.0 == op).cloned().collect()
    }
}

impl PhHost for Mock {
    fn ph(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64, f: &mut [f32; 4]) -> i64 {
        self.log.push((op, a, b, c, d, f[0]));
        if let Some(values) = self.sequences.get_mut(&op) {
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

fn record() -> Box<EeUnitsPhysicsSaved> {
    Box::default()
}

#[test]
fn a_tick_advances_the_clock_and_runs_the_world_in_order() {
    let mut saved = record();
    saved.mTicksToProcess = 2;
    saved.mTickTime = 250.0;
    saved.mGameClock = 1000.0;
    saved.mTickStartTime = 40;
    saved.mResourceTimerPending = false;
    let r = Rec(&mut *saved);
    let mut h = Mock::default()
        .answer(EE_UNITS_PH_RANDOM_GENERATE, 0, i64::from(2.5f32.to_bits()))
        .sequence(EE_UNITS_PH_TIME, &[100, 1300]);
    tick(&mut h, &r);
    assert_eq!(saved.mGameClock, 1250.0);
    assert_eq!(saved.mGameTime, 1250);
    assert_eq!(saved.mTickRandom, 2);
    assert_eq!(saved.mTicksToProcess, 1);
    assert_eq!((saved.mPreviousTickStartTime, saved.mTickStartTime), (40, 100));
    assert_eq!(saved.mNumTicksDone, 1);
    // the work time 1300 - 100 reaches a second: its average taken, the sums reset
    assert_eq!(saved.mAveragePhysicsUpdateTime, 1200);
    assert_eq!((saved.mTickWorkSum, saved.mTickWorkCount), (0, 0));
    assert_eq!((saved.mTickGapSum, saved.mTickGapCount), (60, 1));
    let ops = h.ops();
    let order = [
        EE_UNITS_PH_UPDATE_ACTIVE_UNITS,
        EE_UNITS_PH_UPDATE_WEATHER,
        EE_UNITS_PH_PATH_UPDATE,
        EE_UNITS_PH_AREA_EFFECTS_CLOCK,
        EE_UNITS_PH_CHECK_SHIFT_MODE,
        EE_UNITS_PH_UPDATE_UNITS,
        EE_UNITS_PH_EFFECTS_UPDATE,
        EE_UNITS_PH_CALAMITIES_UPDATE,
        EE_UNITS_PH_AI_UPDATE,
        EE_UNITS_PH_SCENARIO_UPDATE,
        EE_UNITS_PH_CHECK_GAME_OVER,
        EE_UNITS_PH_FLUSH_WORLD,
    ];
    let at: Vec<usize> = order.iter().map(|o| ops.iter().position(|x| x == o).unwrap()).collect();
    assert!(at.windows(2).all(|w| w[0] < w[1]), "{at:?}");
    // owed ticks: no drain
    assert_eq!(h.count(EE_UNITS_PH_DRAIN_INPUT), 0);
    assert_eq!(h.count(EE_UNITS_PH_END_GAME), 0);
}

#[test]
fn a_tick_without_owed_ticks_drains_and_stops() {
    let mut saved = record();
    let r = Rec(&mut *saved);
    let mut h = Mock::default().answer(EE_UNITS_PH_FIXUPS_EMPTY, 0, 1);
    tick(&mut h, &r);
    assert_eq!(h.count(EE_UNITS_PH_DRAIN_INPUT), 1);
    assert_eq!(h.count(EE_UNITS_PH_SERVE_LOAD_SAVE), 1);
    assert_eq!(h.count(EE_UNITS_PH_CAMERA_UPDATE), 0);
    assert_eq!(h.count(EE_UNITS_PH_UPDATE_ACTIVE_UNITS), 0);
}

#[test]
fn a_loaded_game_runs_its_fixups_first() {
    let mut saved = record();
    let r = Rec(&mut *saved);
    let mut h = Mock::default()
        .answer(EE_UNITS_PH_LOADED_SEED_PENDING, 0, 1)
        .answer_c(EE_UNITS_PH_FIXUPS_EMPTY, 1, 1)
        .answer(EE_UNITS_PH_FIXUPS_COUNT, 0, 2);
    tick(&mut h, &r);
    let ops = h.ops();
    assert_eq!(
        &ops[..9],
        &[
            EE_UNITS_PH_CAPTURE_ACTIVE,
            EE_UNITS_PH_LOADED_SEED_PENDING,
            EE_UNITS_PH_CLEAR_LOADED_SEED_PENDING,
            EE_UNITS_PH_SET_LOADED_SEED,
            EE_UNITS_PH_FIXUPS_EMPTY,
            EE_UNITS_PH_FIXUPS_TAKE,
            EE_UNITS_PH_FIXUPS_COUNT,
            EE_UNITS_PH_FIXUP_RUN,
            EE_UNITS_PH_FIXUP_RUN,
        ]
    );
    assert_eq!(ops[9], EE_UNITS_PH_UNIT_ORDER_RESET);
}

#[test]
fn the_game_over_check_ends_the_game() {
    let mut saved = record();
    saved.mTicksToProcess = 1;
    saved.mResourceTimerPending = false;
    let r = Rec(&mut *saved);
    let mut h = Mock::default().answer(EE_UNITS_PH_CHECK_GAME_OVER, 0, 1).answer(EE_UNITS_PH_FIXUPS_EMPTY, 0, 1);
    tick(&mut h, &r);
    assert_eq!(h.find(EE_UNITS_PH_SET_SCENARIO_RESULT)[0].3, 1);
    assert_eq!(h.find(EE_UNITS_PH_END_GAME)[0].3, 1);
}

#[test]
fn drain_input_takes_a_tick_and_executes_its_inputs() {
    let mut saved = record();
    let r = Rec(&mut *saved);
    const TICK: u64 = 7;
    const SELECT: u64 = 8;
    const ORDER: u64 = 9;
    let mut h = Mock::default()
        .sequence(EE_UNITS_PH_SERVER_MESSAGES, &[3, 0])
        .sequence(EE_UNITS_PH_FRONT, &[TICK as i64, SELECT as i64, ORDER as i64])
        .answer(EE_UNITS_PH_MESSAGE_TYPE, TICK, NET_MESSAGE_PHYSICS_TICK)
        .answer(EE_UNITS_PH_MESSAGE_TYPE, SELECT, 0x48)
        .answer(EE_UNITS_PH_MESSAGE_TYPE, ORDER, 0x10)
        .answer(EE_UNITS_PH_RECORD_U32, TICK, 2)
        .answer(EE_UNITS_PH_RECORD_BYTE, TICK, 4)
        .answer(EE_UNITS_PH_GAME_MESSAGE, ORDER, 90);
    drain_input(&mut h, &r);
    assert_eq!((saved.mCurrentTick, saved.mTicksToProcess), (4, 4));
    assert_eq!(h.find(EE_UNITS_PH_SET_SPEED)[0].5, 0.25);
    assert_eq!(h.find(EE_UNITS_PH_EXECUTE_SELECTION)[0].1, SELECT);
    assert_eq!(h.find(EE_UNITS_PH_LOG).iter().map(|e| (e.1, e.3)).collect::<Vec<_>>(), vec![(SELECT, 1), (ORDER, 0)]);
    assert_eq!(h.find(EE_UNITS_PH_EXECUTE_GAME_MESSAGE)[0].1, 90);
    assert_eq!(h.count(EE_UNITS_PH_EXECUTE), 0);
    assert_eq!(h.count(EE_UNITS_PH_DEREFERENCE), 3);
    assert_eq!(h.count(EE_UNITS_PH_OUT_OF_SYNCH_CHECKSUM), 0);
    let ops = h.ops();
    assert_eq!(ops[0], EE_UNITS_PH_QUEUE_LOCK_ENTER);
    assert_eq!(*ops.last().unwrap(), EE_UNITS_PH_QUEUE_LOCK_LEAVE);
}

#[test]
fn a_checksum_tick_on_a_server_logs_the_response() {
    let mut saved = record();
    let r = Rec(&mut *saved);
    let mut h = Mock::default()
        .sequence(EE_UNITS_PH_SERVER_MESSAGES, &[1, 0])
        .sequence(EE_UNITS_PH_FRONT, &[7])
        .answer(EE_UNITS_PH_MESSAGE_TYPE, 7, NET_MESSAGE_PHYSICS_TICK)
        .answer_c(EE_UNITS_PH_RECORD_BOOL, 2, 1)
        .answer(EE_UNITS_PH_IS_SERVER, 0, 1)
        .answer(EE_UNITS_PH_OUT_OF_SYNCH_CHECKSUM, 0, 1234)
        .answer(EE_UNITS_PH_CTS_PROCESSED, 0, 55)
        .answer(EE_UNITS_PH_CHECKSUM_RESPONSE, 0, 66)
        .answer(EE_UNITS_PH_RECORD_BYTE, 7, 1);
    drain_input(&mut h, &r);
    assert_eq!(h.find(EE_UNITS_PH_CHECKSUM_RESPONSE)[0].3, 55);
    assert_eq!(h.find(EE_UNITS_PH_CHECKSUM_RESPONSE)[0].4, 1234);
    assert_eq!(h.find(EE_UNITS_PH_LOG_CHECKSUM_RESPONSE)[0].1, 66);
    assert_eq!(h.count(EE_UNITS_PH_LOCAL_INPUT), 0);
}

#[test]
fn the_checksum_sums_its_parts_in_unsigned_long() {
    let mut saved = record();
    saved.mCurrentTick = 10;
    let r = Rec(&mut *saved);
    let mut h = Mock::default()
        .answer(EE_UNITS_PH_SEED_AND_INPUTS, 0, 5)
        .answer(EE_UNITS_PH_CP_CHECKSUM, 0, 100)
        .answer(EE_UNITS_PH_MAP_CHECKSUM, 0, 1000)
        .sequence(EE_UNITS_PH_NUM_PLAYERS, &[1, 0])
        .answer_c(EE_UNITS_PH_PLAYER_AT, 0, 40)
        .answer(EE_UNITS_PH_ACTIVE_GROUP, 40, 41)
        .sequence(EE_UNITS_PH_GROUP_NUM_UNITS, &[1, 0])
        .answer(EE_UNITS_PH_GROUP_UNIT_ID, 41, 7)
        .answer(EE_UNITS_PH_PLAYER_ID, 40, 3);
    let checksum = out_of_synch_checksum(&mut h, &r, false, 4 | 0x200);
    assert_eq!(checksum, 10 + 5 + 100 + 1000 + 21);
    assert_eq!(h.find(EE_UNITS_PH_SET_LAST_CHECKSUM)[0].3, checksum as i64);
    assert_eq!(h.count(EE_UNITS_PH_DUMP_OPEN), 0);
    assert_eq!(h.count(EE_UNITS_PH_CALAMITY_CHECKSUM), 0);
}

#[test]
fn the_dump_walks_the_players_then_the_units() {
    let mut saved = record();
    let r = Rec(&mut *saved);
    let mut h = Mock::default()
        .sequence(EE_UNITS_PH_NUM_PLAYERS, &[1, 0, 0])
        .answer(EE_UNITS_PH_FIRST_UNIT, 0, 70);
    out_of_synch_checksum(&mut h, &r, true, 0x10);
    assert_eq!(h.count(EE_UNITS_PH_DUMP_AIR_TRAFFIC), 4);
    assert_eq!(h.count(EE_UNITS_PH_DUMP_CALAMITIES), 1);
    assert_eq!(h.count(EE_UNITS_PH_DUMP_MAP), 0);
    assert_eq!(h.find(EE_UNITS_PH_DUMP_UNIT_STATE)[0].1, 70);
    let ops = h.ops();
    let close = ops.iter().position(|&o| o == EE_UNITS_PH_DUMP_CLOSE).unwrap();
    let units = ops.iter().position(|&o| o == EE_UNITS_PH_DUMP_UNIT_STATE).unwrap();
    assert!(units < close);
}

#[test]
fn the_swap_kinds() {
    // one swap: old 1 -> new 2, the same player; 1 a citizen, 2 not
    let mk = || {
        Mock::default()
            .answer(EE_UNITS_PH_SWAPS, 0, 1)
            .answer_c(EE_UNITS_PH_SWAP_OLD, 0, 1)
            .answer_c(EE_UNITS_PH_SWAP_NEW, 0, 2)
            .answer(EE_UNITS_PH_UNIT_PLAYER, 1, 40)
            .answer(EE_UNITS_PH_UNIT_PLAYER, 2, 40)
            .answer(EE_UNITS_PH_IS_CITIZEN, 1, 1)
    };
    assert_eq!(unit_swap_kind(&mut mk(), 1), 5);
    assert_eq!(unit_swap_kind(&mut mk(), 2), 7);
    assert_eq!(unit_swap_kind(&mut mk(), 3), 0);
    let mut h = mk().answer(EE_UNITS_PH_UNIT_PLAYER, 2, 41);
    assert_eq!(unit_swap_kind(&mut h, 2), 4);
    let mut h = mk().answer(EE_UNITS_PH_IS_CITIZEN, 1, 0).answer(EE_UNITS_PH_IS_CITIZEN, 2, 1);
    assert_eq!(unit_swap_kind(&mut h, 1), 6);
}

#[test]
fn the_civ_timer_warns_then_expires() {
    let mut saved = record();
    saved.mCivTimerRuns = true;
    saved.mGameTime = 250_000;
    saved.mTickWorkCount = 3;
    let r = Rec(&mut *saved);
    let mk = || {
        Mock::default()
            .answer(EE_UNITS_PH_HAS_COMMUNICATIONS, 0, 1)
            .answer(EE_UNITS_PH_IS_SERVER, 0, 1)
            .answer(EE_UNITS_PH_CAN_BUY_CIVILIZATION, 0, 1)
            .answer(EE_UNITS_PH_CIV_TIMER_LIMIT, 0, 300_000)
            .answer(EE_UNITS_PH_CIV_TIMER_SECOND, 0, 270_000)
            .answer(EE_UNITS_PH_CIV_TIMER_FIRST, 0, 240_000)
    };
    let mut h = mk();
    update_civ_timer(&mut h, &r);
    assert!(saved.mCivTimerFirstWarning);
    assert_eq!(h.find(EE_UNITS_PH_CIV_TIMER_MESSAGE)[0].3, 1);
    let mut h = mk();
    update_civ_timer(&mut h, &r);
    assert_eq!(h.count(EE_UNITS_PH_CIV_TIMER_MESSAGE), 0);
    saved.mGameTime = 300_001;
    let r = Rec(&mut *saved);
    let mut h = mk();
    update_civ_timer(&mut h, &r);
    assert!(!saved.mCivTimerRuns);
    assert_eq!(h.find(EE_UNITS_PH_CIV_TIMER_MESSAGE)[0].3, 0);
}

#[test]
fn the_weather_walks_the_wind() {
    let mut saved = record();
    saved.mTickTime = 100.0;
    saved.mTimeOfDayRuns = true;
    saved.mTickSeconds = 0.1;
    let r = Rec(&mut *saved);
    let mut h = Mock::default()
        .answer(EE_UNITS_PH_GET_TIME_OF_DAY, 0, i64::from(0.5f32.to_bits()))
        .sequence(EE_UNITS_PH_WIND_YAW, &[i64::from(1.0f32.to_bits()), i64::from(1.25f32.to_bits())])
        .answer(EE_UNITS_PH_RANDOM_GENERATE, 0, i64::from(0.75f32.to_bits()));
    update_weather(&mut h, &r);
    let expected = ((0.75f32 * 2.0 - 1.0) * 100.0) * 0.005 + 1.0;
    assert_eq!(h.find(EE_UNITS_PH_SET_WIND_YAW)[0].5, expected);
    assert_eq!(h.find(EE_UNITS_PH_WIND_ORIENT)[0].5, 1.25);
    assert_eq!(h.find(EE_UNITS_PH_SET_TIME_OF_DAY)[0].5, 0.1f32 * 0.0016666667 + 0.5);
    assert_eq!(saved.mTimeOfDayStep, 0.0);
}

#[test]
fn the_area_effects_and_shift_clocks_wrap() {
    let mut saved = record();
    saved.mTickTime = 1500.0;
    saved.mAreaEffectsClock = 1000.0;
    saved.mShiftModeClock = 3000.0;
    let r = Rec(&mut *saved);
    let mut h = Mock::default().answer(EE_UNITS_PH_SHIFT_MODE, 0, 1).answer(EE_UNITS_PH_SHIFT_UP, 0, 1);
    update_area_effects_clock(&mut h, &r);
    check_shift_mode(&mut h, &r);
    assert_eq!(saved.mAreaEffectsClock, 500.0);
    assert_eq!(saved.mShiftModeClock, 500.0);
    assert_eq!(h.count(EE_UNITS_PH_AREA_EFFECTS_TICK), 1);
    assert_eq!(h.count(EE_UNITS_PH_EXIT_SHIFT_MODE), 1);
}

#[test]
fn the_timing_follows_the_speed() {
    let mut saved = record();
    let r = Rec(&mut *saved);
    let mut h = Mock::default()
        .answer(EE_UNITS_PH_TICKS_PER_SECOND, 0, 8)
        .answer(EE_UNITS_PH_GAME_SPEED, 0, i64::from(2.0f32.to_bits()));
    update_timing(&mut h, &r);
    assert_eq!(saved.mTicksPerGameSecond, 4.0);
    assert_eq!(saved.mTickSeconds, 0.25);
    assert_eq!(saved.mTickTime, 250.0);
    assert_eq!(saved.mTickTimeMs, 250);
}

#[test]
fn the_time_string_splits_the_clock() {
    let mut h = Mock::default().answer(EE_UNITS_PH_GAME_CLOCK, 0, i64::from(3_725_500.0f32.to_bits()));
    time_string(&mut h, 5);
    let e = h.find(EE_UNITS_PH_TIME_TEXT)[0];
    assert_eq!((e.1, e.2, e.3), (5, 0, 1));
    assert_eq!((e.4 & 0xffff_ffff, e.4 >> 32), (2, 5));
}

#[test]
fn init_resets_and_drops_the_requests() {
    let mut saved = record();
    saved.mGameOver = true;
    saved.mNumTicksDone = 9;
    let r = Rec(&mut *saved);
    let mut h = Mock::default().sequence(EE_UNITS_PH_REQUESTS, &[2, 2, 2]);
    init(&mut h, &r);
    assert!(!saved.mGameOver && saved.mCivTimerRuns && saved.mResourceTimerPending);
    assert_eq!(saved.mNumTicksDone, 0);
    assert_eq!(h.count(EE_UNITS_PH_REQUEST_DEREFERENCE), 2);
    assert_eq!(*h.ops().last().unwrap(), EE_UNITS_PH_SET_SPEED);
}

#[test]
fn the_unit_swaps_are_applied_then_cleared() {
    let mut h = Mock::default()
        .sequence(EE_UNITS_PH_SWAPS, &[1, 1, 0])
        .sequence(EE_UNITS_PH_NUM_PLAYERS, &[2, 2])
        .answer_c(EE_UNITS_PH_SWAP_OLD, 0, 1)
        .answer_c(EE_UNITS_PH_SWAP_NEW, 0, 2);
    process_unit_swaps(&mut h);
    assert_eq!(h.count(EE_UNITS_PH_UNITS_SWAPPED), 1);
    assert_eq!(h.find(EE_UNITS_PH_SWAP_UNIT)[0], (EE_UNITS_PH_SWAP_UNIT, 1, 2, 0, 0, 0.0));
    assert_eq!(h.count(EE_UNITS_PH_UNIT_DEREFERENCE), 2);
    assert_eq!(*h.ops().last().unwrap(), EE_UNITS_PH_SWAPS_CLEAR);
}

#[test]
fn the_camera_follows_a_swapped_unit_it_watched() {
    let mut h = Mock::default().answer(EE_UNITS_PH_CAMERA_UNIT_A, 0, 1).answer(EE_UNITS_PH_CAMERA_UNIT_B, 0, 1);
    swap_unit(&mut h, 1, 2);
    assert_eq!(h.find(EE_UNITS_PH_CAMERA_FOLLOW)[0].1, 2);
    let mut h = Mock::default().answer(EE_UNITS_PH_CAMERA_UNIT_A, 0, 1);
    swap_unit(&mut h, 1, 2);
    assert_eq!(h.count(EE_UNITS_PH_CAMERA_CLEAR_A), 1);
    assert_eq!(h.count(EE_UNITS_PH_CAMERA_CLEAR_B), 0);
}

#[test]
fn messages_reach_their_handlers() {
    let mut saved = record();
    let r = Rec(&mut *saved);
    let mut h = Mock::default();
    handle_message(&mut h, &r, 0x502, 0, 0);
    assert!(saved.mTimeOfDayRuns);
    handle_message(&mut h, &r, 0x50c, 77, 0);
    assert_eq!((saved.mLD4, saved.mBE7), (77, true));
    handle_message(&mut h, &r, 0x999, 3, 4);
    assert_eq!(h.find(EE_UNITS_PH_RTS_HANDLE_MESSAGE)[0], (EE_UNITS_PH_RTS_HANDLE_MESSAGE, 3, 4, 0x999, 0, 0.0));
}

#[test]
fn the_end_game_records_a_won_campaign() {
    let mut saved = record();
    let r = Rec(&mut *saved);
    let mut h = Mock::default()
        .sequence(EE_UNITS_PH_NUM_PLAYERS, &[3, 3, 3])
        .answer(EE_UNITS_PH_CAMPAIGN_NAME_LENGTH, 0, 4)
        .answer(EE_UNITS_PH_CAMPAIGN_CHECKSUM, 0, 9)
        .answer(EE_UNITS_PH_CURRENT_GAME_RESULT, 0, 1);
    end_game(&mut h, &r, true);
    assert_eq!(h.count(EE_UNITS_PH_REMOVE_FROM_GAME), 2);
    assert_eq!(h.find(EE_UNITS_PH_SET_GAME_RESULT)[0].3, 1);
    assert_eq!(h.count(EE_UNITS_PH_PROGRESS_SAVE), 1);
    assert!(saved.mGameOver);
}
