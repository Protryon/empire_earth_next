use super::*;
use crate::units::player::EeUnitsQueued;

/// A player: herds (next update, active, state), units (hit points, max, healed), the records' fields and queue
/// (`fields`), and the calls made.
#[derive(Default)]
struct Mock {
    herds: Vec<(u32, bool, u32)>,
    units: Vec<(i32, i32, bool)>,
    tick_ms: i64,
    tick_time: f32,
    game_time: u32,
    epoch: i64,
    advance_empty: bool,
    queue: Box<EeUnitsQueued>,
    instant: bool,
    heals: bool,
    log: Vec<String>,
}

impl Mock {
    fn with_queue(entries: &[(c_ulong, f32, Vec<c_ulong>)]) -> Self {
        let mut m = Mock::default();
        for (k, f, v) in entries {
            m.queue.map.insert(*k, (*f, v.clone()));
        }
        m
    }
    fn queued(&self) -> Vec<(c_ulong, f32, Vec<c_ulong>)> {
        self.queue.map.iter().map(|(k, e)| (*k, e.0, e.1.clone())).collect()
    }
}

impl PlayerTickHost for Mock {
    fn call(&mut self, op: i32, index: u32, io: &mut EeUnitsPlayerSaveIo) -> i32 {
        match op {
            EE_UNITS_PLAYER_TICK_VALIDATE => self.log.push("validate".into()),
            EE_UNITS_PLAYER_TICK_HERD_COUNT => return self.herds.len() as i32,
            EE_UNITS_PLAYER_TICK_GAME_TIME => io.i = i64::from(self.game_time),
            EE_UNITS_PLAYER_TICK_HERD_DUE => {
                io.i = i64::from(self.herds[index as usize].0);
                io.b = u8::from(self.herds[index as usize].1);
            }
            EE_UNITS_PLAYER_TICK_HERD_UPDATE => self.log.push(format!("herd {index} update {}", io.i)),
            EE_UNITS_PLAYER_TICK_HERD_STATE => return self.herds[index as usize].2 as i32,
            EE_UNITS_PLAYER_TICK_HERD_KILL => self.log.push(format!("herd {index} kill")),
            EE_UNITS_PLAYER_TICK_AIR_AND_EVENTS => self.log.push("air events".into()),
            EE_UNITS_PLAYER_TICK_TICK_TIME => io.f = self.tick_time,
            EE_UNITS_PLAYER_TICK_QUEUED_APPLY => self.log.push(format!("apply {} {index}", io.i)),
            EE_UNITS_PLAYER_TICK_QUEUED_ADVANCE => {
                // the advance (player.rs) on the queue the walk reads
                let e = self.queue.map.get_mut(&(io.i as c_ulong)).expect("the walk's entry");
                if e.1.is_empty() || self.advance_empty {
                    self.log.push(format!("erase {}", io.i));
                    return 1;
                }
                e.0 = 30.0;
            }
            EE_UNITS_PLAYER_TICK_TICK_MS => io.i = self.tick_ms,
            EE_UNITS_PLAYER_TICK_HEAL_VALUE => match index {
                0 => io.i = 10,
                1 => io.i = self.epoch,
                2 => io.i = 3,
                _ => io.f = 0.5,
            },
            EE_UNITS_PLAYER_TICK_UNIT_NEXT => {
                let next = io.i as usize; // ids 1..=n
                if next >= self.units.len() {
                    io.i = 0;
                    return 0;
                }
                io.i = next as i64 + 1;
                return i32::from(self.units[next].2);
            }
            EE_UNITS_PLAYER_TICK_UNIT_HIT_POINTS => {
                let u = self.units[io.i as usize - 1];
                return if index == 0 { u.0 } else { u.1 };
            }
            EE_UNITS_PLAYER_TICK_UNIT_SET_HIT_POINTS => {
                self.units[io.i as usize - 1].0 = index as i32;
                self.log.push(format!("unit {} hp {}", io.i, index as i32));
            }
            _ => panic!("op {op}"),
        }
        0
    }
}

fn run(m: &mut Mock, rec: &mut EeUnitsPlayerSaved) {
    let mut fields = EeUnitsPlayerFields {
        mInstantBuild: m.instant,
        mHealsUnits: m.heals,
        mQueued: &mut *m.queue,
        ..Default::default()
    };
    // SAFETY: the records and the queue live through the call
    unsafe { update_player(m, rec, &mut fields) };
}

#[test]
fn herds_update_when_due_and_end_in_state_0xb() {
    let mut m =
        Mock { herds: vec![(100, true, 0), (200, true, 0xb), (50, false, 0)], game_time: 150, ..Default::default() };
    run(&mut m, &mut EeUnitsPlayerSaved::default());
    assert_eq!(m.log, vec!["validate", "herd 1 kill", "herd 0 update 150", "air events"]);
}

#[test]
fn queued_timers_count_down_apply_and_erase() {
    let mut m = Mock::with_queue(&[(3, 2.0, vec![7, 8]), (5, 0.5, vec![9]), (6, 0.0, vec![]), (9, 0.0, vec![4])]);
    m.tick_time = 1.0;
    run(&mut m, &mut EeUnitsPlayerSaved::default());
    // 3 still runs (1.0 left); 5 runs out: 9 applied, its list empty, erased; 6 empty: erased; 9 idle: started
    assert_eq!(m.log, vec!["validate", "air events", "apply 5 9", "erase 5", "erase 6"]);
    assert_eq!(m.queued(), vec![(3, 1.0, vec![7, 8]), (9, 30.0, vec![4])]);
}

#[test]
fn instant_build_caps_a_timer_at_one() {
    let mut m = Mock::with_queue(&[(3, 20.0, vec![7])]);
    m.tick_time = 1.0;
    m.instant = true;
    run(&mut m, &mut EeUnitsPlayerSaved::default());
    assert_eq!(m.queued()[0].1, 1.0);
    // and one more tick applies it
    run(&mut m, &mut EeUnitsPlayerSaved::default());
    assert!(m.log.contains(&"apply 3 7".to_string()));
}

#[test]
fn the_heal_waits_for_its_timer() {
    let mut rec = EeUnitsPlayerSaved { mHealTimer: 500.0, ..Default::default() };
    let mut m = Mock { heals: true, tick_ms: 100, units: vec![(5, 50, true)], ..Default::default() };
    run(&mut m, &mut rec);
    assert_eq!(rec.mHealTimer, 400.0);
    assert_eq!(m.units[0].0, 5);
    // without healing the timer is left alone
    let mut m = Mock { heals: false, tick_ms: 100, ..Default::default() };
    run(&mut m, &mut rec);
    assert_eq!(rec.mHealTimer, 400.0);
}

#[test]
fn the_heal_raises_living_healed_units_to_their_maximum() {
    // epoch 7: 10 + (long)((7 / 3) * 0.5 * 10) = 20
    let mut rec = EeUnitsPlayerSaved { mHealTimer: 50.0, ..Default::default() };
    let mut m = Mock {
        heals: true,
        tick_ms: 100,
        epoch: 7,
        units: vec![(5, 50, true), (45, 50, true), (0, 50, true), (5, 50, false)],
        ..Default::default()
    };
    run(&mut m, &mut rec);
    assert_eq!(rec.mHealTimer, EE_UNITS_PLAYER_HEAL_INTERVAL);
    let hp: Vec<i32> = m.units.iter().map(|u| u.0).collect();
    assert_eq!(hp, vec![25, 50, 0, 5]);
}

#[test]
fn a_negative_tick_counts_as_zero() {
    let mut rec = EeUnitsPlayerSaved { mHealTimer: 50.0, ..Default::default() };
    let mut m = Mock { heals: true, tick_ms: -100, ..Default::default() };
    run(&mut m, &mut rec);
    assert_eq!(rec.mHealTimer, 50.0);
}

#[test]
fn the_heal_amount_truncates_its_float_product() {
    assert_eq!(heal_amount(10, 7, 3, 0.5), 20);
    assert_eq!(heal_amount(10, 2, 3, 0.5), 10);
    assert_eq!(heal_amount(7, 5, 1, 0.33), 7 + 11);
    assert_eq!(truncate(f32::NAN), i32::MIN);
    assert_eq!(truncate(3e9), i32::MIN);
    assert_eq!(truncate(-2.9), -2);
}

#[test]
fn reset_sets_the_records_fields_in_two_parts() {
    let mut r = EeUnitsPlayerSaved {
        mResourceCaps: [9; 7],
        mTributes: [3; 10],
        mResourceTrade: [4; 5],
        mInsuranceRefunds: [5; 5],
        mNumVitalBuildings: 2,
        mChatType: 3,
        mFreeUnitInterval: 6,
        mHealTimer: 9.0,
        ..Default::default()
    };
    reset(&mut r, 2, 0);
    assert_eq!(r.mResourceCaps, [0; 7], "all seven (0 and 1 too: the checksum sums them)");
    let mut enemies = [1; 17];
    enemies[2] = 0;
    assert_eq!(r.mEnemies, enemies);
    assert_eq!((r.mChatType, r.mFreeUnitInterval, r.mHealTimer), (1, 1, 0.0));
    assert!(r.mSharedLOS && r.mSendChatMessage.iter().all(|&s| s));
    assert_eq!(r.mTributes, [0; 10]);
    assert_eq!((r.mResourceTrade, r.mNumVitalBuildings), ([4; 5], 2));
    reset(&mut r, 2, 1);
    assert_eq!((r.mResourceTrade, r.mInsuranceRefunds, r.mNumVitalBuildings), ([0; 5], [0; 5], 0));
}
