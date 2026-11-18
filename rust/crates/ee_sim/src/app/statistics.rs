//! `EEWorld::RecordStatistics` (0x13a845): the post-game statistics. The computer players' gathered totals topped up
//! against the best human's, every player scored, the players sorted by score, the records' order, local and human
//! flags and the computers' play times (scaled from the longest human one). The C++ (EEWorld.cpp) gathers the players,
//! keeps the history's records (names, colours, the statistics' copies) and applies the result.
//!
//! Integer arithmetic is the C++'s: `long` and `unsigned long` (`c_long`, `c_ulong`) with their wrapping mixes; the
//! floats are the C++'s `float` and `double` steps in order; `UMFloatToLong` is `lrintf`. The random draws
//! (`gPhysics->GetRandom`) are made in the C++'s order and count.

use std::os::raw::{c_long, c_ulong};

/// One player of the world (`mPlayers[slot]`).
#[derive(Clone, Debug, Default, PartialEq)]
pub struct StatsPlayer {
    /// `GetPlayerID()`
    pub id: c_ulong,
    /// `IsHuman()`
    pub human: bool,
    /// a computer whose name is the resource's "Computer" (numbered " #n")
    pub default_name: bool,
    /// `mObserverMode`
    pub observer: bool,
    /// `mGameResult`
    pub result: c_ulong,
    /// `mDisconnected`
    pub disconnected: bool,
}

/// What the statistics read and write of the players (a slot is the world's player index).
pub trait StatsHost {
    /// `gPhysics->GetRandom(lo, hi)`
    fn random(&mut self, lo: f32, hi: f32) -> f32;
    /// `mSavedGroups`' readers: 0 `GetValue`, 1 `GetLastValue`, 2 `CountIncreases`
    fn value(&self, slot: usize, what: u32, series: c_ulong) -> c_ulong;
    /// `mSavedGroups.GetSeries(series).mPoints`: (time, value)
    fn points(&self, slot: usize, series: c_ulong) -> Vec<(c_ulong, c_ulong)>;
    /// `mSavedGroups.CompactEpochHistory()`
    fn compact(&mut self, slot: usize);
    /// `mSavedGroups.Set(series, value)`
    fn set(&mut self, slot: usize, series: c_long, value: c_long);
    /// `mResourceCaps[r]`
    fn cap(&self, slot: usize, r: usize) -> c_long;
    fn set_cap(&mut self, slot: usize, r: usize, value: c_long);
}

/// The world's part of the statistics.
#[derive(Clone, Debug, Default)]
pub struct StatsWorld {
    /// `mPlayers[0 .. mNumPlayers]`
    pub players: Vec<StatsPlayer>,
    /// `mCurrentPlayer`
    pub current: c_long,
    /// `mStatistics.mIsScenario` (already set by the C++)
    pub is_scenario: bool,
    /// `mStatistics.mEndTime` (the game ms)
    pub end_time: c_long,
    /// `EEStartingResources::sTable[mGameOptions.mResources]->mAmounts` (food, wood, stone, gold, iron)
    pub base: [c_long; 5],
    /// `EEGetResourceLevel(mGameOptions.mResources)->mAmounts`
    pub level: [c_long; 5],
}

/// A record in the history's order.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct StatsRecord {
    /// the player's world slot
    pub slot: usize,
    pub score: c_ulong,
    pub economy_score: c_ulong,
    pub epoch_score: c_ulong,
    pub military_score: c_ulong,
    pub tech_score: c_ulong,
    /// the name's suffix number (" #n"; None: no suffix)
    pub suffix: Option<c_long>,
    pub is_local: bool,
    pub is_human: bool,
    /// the record's statistics' series 0x18 (the play time) set to this after its copy (None: kept)
    pub play_time: Option<c_long>,
}

/// The statistics' result.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct StatsResult {
    pub records: Vec<StatsRecord>,
    /// `mCurrentRecordIndex` set to this (None: kept)
    pub current_index: Option<usize>,
    /// `mIsScenario` after (a scenario with more than one human is none)
    pub is_scenario: bool,
}

fn float_to_long(v: f32) -> c_long {
    ee_base::math::um::lrintf(v) as c_long
}

/// `(long)` of a `double`: `cvttsd2si` (the integer indefinite, `c_long::MIN`, for NaN and out of range: a long
/// game's epoch score overflows it).
pub fn trunc_long_f64(v: f64) -> c_long {
    let min = c_long::MIN as f64;
    if v.is_nan() || v < min || v >= -min {
        c_long::MIN
    } else {
        v as c_long
    }
}

/// The resource series a computer's total is topped up in (food, wood, stone, gold, iron by `mResourceCaps` index).
fn series_of(r: usize) -> c_ulong {
    match r {
        2 => 7,
        3 => 8,
        4 => 10,
        5 => 9,
        _ => 11,
    }
}

/// A computer's gathered totals topped up against the recorded human with the most of each.
fn top_up(h: &mut dyn StatsHost, w: &StatsWorld, players: &[usize], slot: usize) {
    for r in 2..7usize {
        let base = w.base[r - 2];
        let series = series_of(r);
        let mut best: Option<usize> = None;
        for &j in players {
            if w.players[j].human && best.is_none_or(|b| h.value(b, 0, series) < h.value(j, 0, series)) {
                best = Some(j);
            }
        }
        // no human among the recorded players: there is no one to top up against
        let Some(best) = best else { continue };
        let best_value = h.value(best, 0, series) as c_long;
        let gathered = best_value.wrapping_sub(h.cap(best, r)).wrapping_add(base);
        let factor = h.random(0.5, 1.5);
        let (mut value, target): (c_ulong, c_ulong) = if best_value < 1 {
            let v = h.value(slot, 0, series);
            (v, v)
        } else {
            (float_to_long(best_value as f32 * factor) as c_ulong, float_to_long(gathered as f32 * factor) as c_ulong)
        };
        let excess = target.wrapping_sub(base as c_ulong).wrapping_sub(value) as c_long;
        if excess > 0 {
            value = value.wrapping_add(excess as c_ulong);
        }
        let spent_part = float_to_long(h.random(0.03, 0.06) * target as f32);
        let spent = target.wrapping_sub(spent_part as c_ulong) as c_long;
        if h.value(slot, 0, series) < value {
            h.set(slot, series as c_long, value as c_long);
        } else {
            value = h.value(slot, 0, series);
        }
        h.set_cap(slot, r, (base.wrapping_sub(spent) as c_ulong).wrapping_add(value) as c_long);
    }
}

/// A player's score parts: (score, the "military" (economy record), "units" (epoch record), "economy" (military
/// record), tech), named as the C++'s locals.
fn score(h: &mut dyn StatsHost, w: &StatsWorld, slot: usize) -> (c_ulong, c_long, c_long, c_long, c_long) {
    let mut score: c_ulong = 0;
    let (mut military, mut units, mut economy, mut tech, mut gathered, mut start): (
        c_long,
        c_long,
        c_long,
        c_long,
        c_long,
        c_long,
    ) = (0, 0, 0, 0, 0, 0);
    if w.players[slot].id == 0 {
        return (0x7fffffff, 0, 0, 0, 0);
    }
    h.compact(slot);
    let add = |a: c_long, b: c_ulong| (a as c_ulong).wrapping_add(b) as c_long;
    for s in 0..31 as c_ulong {
        let value = h.value(slot, 0, s);
        let points = h.points(slot, s);
        let resource = |r: usize,
                        at: usize,
                        shift: bool,
                        h: &dyn StatsHost,
                        military: &mut c_long,
                        score: &mut c_ulong,
                        gathered: &mut c_long,
                        start: &mut c_long| {
            *gathered = add(*gathered, value);
            *military = add(*military, if shift { value >> 3 } else { value / 10 });
            *score = score.wrapping_add(h.cap(slot, r) as c_ulong);
            *start = start.wrapping_add(w.level[at]);
        };
        match s {
            1 => economy = add(economy, value.wrapping_mul((-15i32) as c_ulong)),
            2 => economy = add(economy, value.wrapping_mul(50)),
            3 | 5 => economy = add(economy, value.wrapping_mul(20)),
            4 => economy = add(economy, value.wrapping_mul((-5i32) as c_ulong)),
            7 => resource(2, 0, false, h, &mut military, &mut score, &mut gathered, &mut start),
            8 => resource(3, 1, false, h, &mut military, &mut score, &mut gathered, &mut start),
            9 => resource(5, 3, true, h, &mut military, &mut score, &mut gathered, &mut start),
            10 => resource(4, 2, false, h, &mut military, &mut score, &mut gathered, &mut start),
            11 => resource(6, 4, true, h, &mut military, &mut score, &mut gathered, &mut start),
            12 => {
                let first = if points.len() <= 1 { 0 } else { points[1].1 };
                for &(time, value) in &points {
                    let epoch = time.wrapping_add(1);
                    let at = if value < first { first } else { value };
                    let part =
                        (w.end_time as c_ulong) as f64 / epoch as f64 * at.wrapping_sub(first) as f64 * 100.0 + 0.5;
                    units = units.wrapping_add(trunc_long_f64(part));
                }
            }
            13 => economy = add(economy, value.wrapping_mul(10)),
            14 => tech = add(tech, value.wrapping_mul(10)),
            15 => tech = add(tech, h.value(slot, 2, s).wrapping_mul(100)),
            16 => tech = add(tech, value.wrapping_mul(500)),
            18 => gathered = add(gathered, value),
            19 => tech = add(tech, value.wrapping_mul(2)),
            _ => {}
        }
    }
    let bonus = (gathered as c_ulong).wrapping_sub(score).wrapping_add(start as c_ulong) as c_long / 4;
    let floor = if w.is_scenario { score as c_long } else { 0 };
    let bonus = if bonus <= floor { floor } else { bonus };
    military = military.wrapping_add(bonus);
    let score = (tech.wrapping_add(economy).wrapping_add(units).wrapping_add(military)) as c_ulong;
    (score, military, units, economy, tech)
}

/// `RecordStatistics` after the history's own fields.
pub fn record_statistics(h: &mut dyn StatsHost, w: &StatsWorld) -> StatsResult {
    // the recorded players: every player not observing or with a result, and the disconnected observers; the local
    // player observing (without a result) takes the first record that is not its own
    let mut players = Vec::new();
    let mut local_pending = false;
    for (i, p) in w.players.iter().enumerate() {
        if !p.observer || p.result != 0 {
            players.push(i);
        } else {
            if w.current >= 0 && w.current as usize == i {
                local_pending = true;
            }
            if p.disconnected {
                players.push(i);
            }
        }
    }
    let mut computers: c_long = 0;
    // (score key, slot, record) in insertion order; sorted by the key (signed, descending), stable
    let mut scored: Vec<(c_long, StatsRecord)> = Vec::new();
    for &slot in &players {
        let p = &w.players[slot];
        let mut suffix = None;
        if !p.human {
            if p.default_name {
                computers += 1;
                suffix = Some(computers);
            }
            top_up(h, w, &players, slot);
        }
        let (score, military, units, economy, tech) = score(h, w, slot);
        scored.push((
            score as c_long,
            StatsRecord {
                slot,
                score,
                economy_score: military as c_ulong,
                epoch_score: units as c_ulong,
                military_score: economy as c_ulong,
                tech_score: tech as c_ulong,
                suffix,
                ..StatsRecord::default()
            },
        ));
    }
    scored.sort_by_key(|s| std::cmp::Reverse(s.0)); // (stable: equal scores keep their order)
    let mut result = StatsResult { records: Vec::new(), current_index: None, is_scenario: w.is_scenario };
    let mut humans = 0;
    let mut max_time: c_ulong = 0;
    for (index, (_, mut record)) in scored.into_iter().enumerate() {
        let slot = record.slot;
        let p = &w.players[slot];
        let last14 = h.value(slot, 1, 14);
        h.set(slot, 14, last14 as c_long);
        let last13 = h.value(slot, 1, 13);
        h.set(slot, 13, last13 as c_long);
        if w.current >= 0 && p.id == w.current as c_ulong {
            record.is_local = true;
            result.current_index = Some(index);
        } else if local_pending {
            local_pending = false;
            record.is_local = true;
            result.current_index = Some(index);
        }
        if p.human {
            record.is_human = true;
            if index > 0 {
                humans += 1;
            }
            max_time = max_time.max(h.value(slot, 0, 0x18));
        }
        result.records.push(record);
    }
    for record in result.records.iter_mut() {
        let p = &w.players[record.slot];
        if p.human {
            continue;
        }
        let factor = match p.result {
            1 => h.random(0.75, 0.9),
            2 => h.random(0.6, 0.75),
            _ => h.random(0.6, 0.9),
        };
        record.play_time = Some(crate::units::player::trunc_long(factor * max_time as f32 + 0.5));
    }
    if humans > 1 && result.is_scenario {
        result.is_scenario = false;
    }
    result
}
