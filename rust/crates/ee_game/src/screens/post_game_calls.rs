//! The post-game summary's C++ reads (`UI Events/EEScreens.cpp`: the original's `ee_screenscpp_pgs_history`,
//! `_pgs_record` and the `EE_SCREENS_GAME_PGS_ADD_SERIES` / `_WINDOW` cases) in Rust over one-statement C++ shims
//! (UI lane round 9; the shims are `ee_screenscpp_pgs_*` and `_pr_*` in `EEScreens.cpp`).

use core::ffi::c_long;

use crate::screens::game_cpp::Cpp;
use crate::screens::post_game::{EeScreensPgsHistory, EeScreensPgsRecord};

/// The series of the epochs (its points: the epochs reached) and the territory's, the one whose rises count.
const PR_SERIES_EPOCHS: u32 = 12;
const PR_SERIES_TERRITORY: u32 = 0x18;
const PR_SERIES_INCREASES: u32 = 0x0f;

/// `gWorld.mStatistics` as the summary reads it.
pub(crate) fn history<C: Cpp + ?Sized>(c: &mut C) -> EeScreensPgsHistory {
    let mut h = EeScreensPgsHistory {
        records: c.pgs_records(),
        start_time: c.pgs_start_time(),
        end_time: c.pgs_end_time(),
        current_record: c.pgs_current_record(),
        tile_count: c.pgs_tile_count(),
        victory_enabled: c.pgs_victory_enabled(),
        is_scenario: c.pgs_is_scenario(),
        is_campaign: c.pgs_is_campaign(),
        ..Default::default()
    };
    // (the record index as an unsigned long: a negative one is past the records)
    if (h.current_record as u32) < h.records {
        h.current_slot = c.pgs_current_slot();
    }
    for (i, level) in h.levels.iter_mut().enumerate() {
        *level = c.pgs_level(i as u32);
    }
    h
}

/// Record `index` of the statistics.
pub(crate) fn record<C: Cpp + ?Sized>(c: &mut C, index: u32) -> EeScreensPgsRecord {
    let mut r = EeScreensPgsRecord {
        colour: [c.pr_red(index), c.pr_green(index), c.pr_blue(index)],
        result: c.pr_result(index),
        team: c.pr_team(index),
        end_time: c.pr_end_time(index),
        ..Default::default()
    };
    for (k, resource) in r.resources.iter_mut().enumerate() {
        *resource = c.pr_resource(index, k as u32);
    }
    r.score = c.pr_score(index);
    r.economy_score = c.pr_economy_score(index);
    r.epoch_score = c.pr_epoch_score(index);
    r.military_score = c.pr_military_score(index);
    r.tech_score = c.pr_tech_score(index);
    r.slot = c.pr_slot(index);
    r.human = c.pr_human(index);
    for (j, value) in r.values.iter_mut().enumerate() {
        *value = c.pr_value(index, j as u32);
    }
    r.territory = c.pr_last_value(index, PR_SERIES_TERRITORY);
    r.increases = c.pr_increases(index, PR_SERIES_INCREASES);
    c.pr_series_load(index, PR_SERIES_EPOCHS);
    r.epoch_points = c.pr_series_count();
    r.epoch_second = if r.epoch_points > 1 { c.pr_series_value(1) } else { 0 };
    r
}

/// `EE_SCREENS_GAME_PGS_ADD_SERIES`: record `record`'s series `series` (time, value pairs) into the line graph's
/// stored graph `series`, in the record's colour.
pub(crate) fn add_series<C: Cpp + ?Sized>(c: &mut C, form: usize, record: u32, series: u32) {
    c.pr_series_load(record, series);
    let count = c.pr_series_count();
    let mut pairs: Vec<c_long> = Vec::with_capacity(2 * count as usize);
    for i in 0..count {
        pairs.push(c.pr_series_time(i) as c_long);
        pairs.push(c.pr_series_value(i) as c_long);
    }
    c.pgs_graph_add_series(form, pairs.as_ptr() as usize, count, series, record);
}

/// `EE_SCREENS_GAME_PGS_WINDOW`: the game window's width (`which` non-zero) or height.
pub(crate) fn window<C: Cpp + ?Sized>(c: &mut C, form: usize, which: u32) -> u32 {
    if which != 0 {
        c.pgs_window_width(form)
    } else {
        c.pgs_window_height(form)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::screens::game_cpp::Fake;

    #[test]
    fn the_history_reads_the_current_slot_only_for_a_record_in_range() {
        let mut f = Fake::default();
        f.answer("pgs_records", &[3]).answer("pgs_current_record", &[2]).answer("pgs_current_slot", &[5]);
        f.answer("pgs_level", &[1, 2, 3, 4, 5]);
        let h = history(&mut f);
        assert_eq!((h.records, h.current_slot, h.levels), (3, 5, [1, 2, 3, 4, 5]));
        let mut f = Fake::default();
        f.answer("pgs_records", &[3]).answer("pgs_current_record", &[-1]);
        assert_eq!(history(&mut f).current_slot, 0);
        assert!(!f.calls().contains("pgs_current_slot"));
    }

    #[test]
    fn a_record_reads_every_value_and_the_epochs_second_point() {
        let mut f = Fake::default();
        f.answer("pr_result", &[1]).answer("pr_resource", &[7, 6]).answer("pr_value", &[9]);
        f.answer("pr_series_count", &[3]).answer("pr_series_value", &[4]);
        let r = record(&mut f, 2);
        assert_eq!((r.result, r.resources[0], r.resources[1], r.values[0], r.epoch_points, r.epoch_second), (1, 7, 6, 9, 3, 4));
        let calls = f.calls();
        assert!(calls.contains("pr_last_value(2, 24)\npr_increases(2, 15)\npr_series_load(2, 12)"));
        assert!(calls.ends_with("pr_series_count()\npr_series_value(1)"));
        let mut f = Fake::default();
        f.answer("pr_series_count", &[1]);
        assert_eq!(record(&mut f, 0).epoch_second, 0);
    }

    #[test]
    fn a_series_goes_to_the_graph_as_time_value_pairs() {
        let mut f = Fake::default();
        f.answer("pr_series_count", &[2]).answer("pr_series_time", &[10, 20]).answer("pr_series_value", &[1, 2]);
        add_series(&mut f, 0x40, 3, 7);
        let calls = f.calls();
        assert!(calls.starts_with("pr_series_load(3, 7)\npr_series_count()\npr_series_time(0)\npr_series_value(0)"));
        assert!(calls.lines().last().unwrap().starts_with("pgs_graph_add_series(64, "));
        assert!(calls.ends_with(", 2, 7, 3)"));
        let mut f = Fake::default();
        f.answer("pgs_window_width", &[800]).answer("pgs_window_height", &[600]);
        assert_eq!((window(&mut f, 1, 1), window(&mut f, 1, 0)), (800, 600));
    }
}
