//! `RecordStatistics`' unit tests over a fake world (the C++'s golden is the capture's WSTA, the app profile).

use std::collections::HashMap;
use std::os::raw::{c_long, c_ulong};

use super::statistics::*;

#[derive(Default)]
struct Fake {
    /// (slot, series) -> (max, last, increases)
    values: HashMap<(usize, c_ulong), (c_ulong, c_ulong, c_ulong)>,
    points: HashMap<(usize, c_ulong), Vec<(c_ulong, c_ulong)>>,
    caps: HashMap<(usize, usize), c_long>,
    draws: Vec<f32>,
    drawn: Vec<(f32, f32)>,
    sets: Vec<(usize, c_long, c_long)>,
    compacted: Vec<usize>,
}

impl StatsHost for Fake {
    fn random(&mut self, lo: f32, hi: f32) -> f32 {
        self.drawn.push((lo, hi));
        let t = if self.draws.is_empty() { 0.5 } else { self.draws.remove(0) };
        lo + (hi - lo) * t
    }
    fn value(&self, slot: usize, what: u32, series: c_ulong) -> c_ulong {
        let v = self.values.get(&(slot, series)).copied().unwrap_or_default();
        match what {
            0 => v.0,
            1 => v.1,
            _ => v.2,
        }
    }
    fn points(&self, slot: usize, series: c_ulong) -> Vec<(c_ulong, c_ulong)> {
        self.points.get(&(slot, series)).cloned().unwrap_or_else(|| vec![(0, 0)])
    }
    fn compact(&mut self, slot: usize) {
        self.compacted.push(slot);
    }
    fn set(&mut self, slot: usize, series: c_long, value: c_long) {
        self.sets.push((slot, series, value));
        let e = self.values.entry((slot, series as c_ulong)).or_default();
        e.0 = e.0.max(value as c_ulong);
        e.1 = value as c_ulong;
    }
    fn cap(&self, slot: usize, r: usize) -> c_long {
        self.caps.get(&(slot, r)).copied().unwrap_or_default()
    }
    fn set_cap(&mut self, slot: usize, r: usize, value: c_long) {
        self.caps.insert((slot, r), value);
    }
}

fn player(id: c_ulong, human: bool) -> StatsPlayer {
    StatsPlayer { id, human, default_name: !human, ..StatsPlayer::default() }
}

fn world() -> StatsWorld {
    StatsWorld {
        players: vec![player(0, false), player(1, true), player(2, false), player(3, false)],
        current: 1,
        is_scenario: false,
        end_time: 600000,
        base: [200, 200, 100, 100, 0],
        level: [200, 200, 100, 100, 0],
    }
}

#[test]
fn the_scores_order_the_records() {
    let mut f = Fake::default();
    // player 2 researched (series 14: x10), player 3 built more (series 2: x50)
    f.values.insert((2, 14), (30, 30, 0));
    f.values.insert((3, 2), (20, 20, 0));
    f.values.insert((1, 0x18), (500, 500, 0));
    let r = record_statistics(&mut f, &world());
    // nature (player 0) first, then by score, descending
    let order: Vec<usize> = r.records.iter().map(|x| x.slot).collect();
    assert_eq!(order, vec![0, 3, 2, 1]);
    assert_eq!(r.records[0].score, 0x7fffffff);
    assert_eq!(r.records[1].military_score, 1000); // series 2 x 50 (the C++'s "economy" local)
    assert_eq!(r.records[2].tech_score, 300);
    // the computers numbered in the players' order; the human not
    assert_eq!(
        (r.records[0].suffix, r.records[2].suffix, r.records[1].suffix, r.records[3].suffix),
        (Some(1), Some(2), Some(3), None)
    );
    assert_eq!(r.current_index, Some(3));
    assert!(r.records[3].is_local && r.records[3].is_human && !r.records[1].is_local);
    // every player but nature compacted; series 14 and 13 set to their last values, in the records' order
    assert_eq!(f.compacted, vec![1, 2, 3]);
    assert_eq!(f.sets.iter().filter(|s| s.1 == 14 || s.1 == 13).count(), 8);
    // the computers' play times scaled from the human's: 0.75 of 500 (the draws at their middle)
    assert_eq!(r.records[1].play_time, Some(375));
    assert_eq!(r.records[3].play_time, None);
    assert!(!r.is_scenario);
}

#[test]
fn a_computer_is_topped_up_against_the_best_human() {
    let mut f = Fake::default();
    let mut w = world();
    w.players.truncate(3);
    // the human gathered 1000 food (series 7) and holds 300 of it; the computer 100
    f.values.insert((1, 7), (1000, 1000, 0));
    f.caps.insert((1, 2), 300);
    f.values.insert((2, 7), (100, 100, 0));
    // the factor 1.0 (0.5 + 1.0 x 0.5), the spent share 0.045
    f.draws = vec![0.5, 0.5];
    record_statistics(&mut f, &w);
    // value = lrintf(1000 x 1.0) = 1000, target = lrintf((1000 - 300 + 200) x 1.0) = 900; the excess 900 - 200 - 1000
    // is negative; spent = 900 - lrintf(0.045 x 900) = 900 - 40 = 860 (40.5 rounds to even); the cap 200 - 860 + 1000
    assert!(f.sets.contains(&(2, 7, 1000)));
    assert_eq!(f.caps[&(2, 2)], 340);
    // the draws: the factor and the spent share per resource (nature first, then the computer), then a play time each
    assert_eq!(f.drawn[0], (0.5, 1.5));
    assert_eq!(f.drawn[1], (0.03, 0.06));
    assert_eq!(f.drawn.len(), 2 * 5 * 2 + 2);
}

#[test]
fn an_observing_local_player_takes_the_first_other_record() {
    let mut f = Fake::default();
    let mut w = world();
    w.players[1].observer = true; // the local human observes, no result: not recorded
    let r = record_statistics(&mut f, &w);
    assert_eq!(r.records.len(), 3);
    assert_eq!(r.current_index, Some(0));
    assert!(r.records[0].is_local);
    // no human recorded: no top-up draws, the play times from 0
    assert_eq!(r.records.iter().filter_map(|x| x.play_time).collect::<Vec<_>>(), vec![0, 0, 0]);
    w.players[1].disconnected = true; // a disconnected observer is recorded
    let r = record_statistics(&mut Fake::default(), &w);
    assert_eq!(r.records.len(), 4);
}

#[test]
fn a_scenario_with_two_humans_is_none() {
    let mut w = world();
    w.is_scenario = true;
    w.players[2].human = true;
    let mut f = Fake::default();
    f.values.insert((2, 2), (10, 10, 0));
    let r = record_statistics(&mut f, &w);
    assert!(!r.is_scenario);
    w.players[2].human = false;
    assert!(record_statistics(&mut Fake::default(), &w).is_scenario);
}

#[test]
fn the_epochs_score_by_time() {
    let mut f = Fake::default();
    let mut w = world();
    w.players.truncate(2);
    w.end_time = 6000;
    // series 12: the epochs reached, (epoch - 1, game ms)
    f.points.insert((1, 12), vec![(0, 0), (1, 600), (2, 3000)]);
    let r = record_statistics(&mut f, &w);
    let human = r.records.iter().find(|x| x.slot == 1).unwrap();
    // first = 600: (6000 / 1 x 0) + (6000 / 2 x 0) + (6000 / 3 x 2400) x 100
    assert_eq!(human.epoch_score, 480_000_000);
    // a part past the type's range is the conversion's indefinite value
    assert_eq!(trunc_long_f64(4.8e12 * 1e10), c_long::MIN);
    assert_eq!(trunc_long_f64(f64::NAN), c_long::MIN);
    assert_eq!(trunc_long_f64(-2.5), -2);
}
