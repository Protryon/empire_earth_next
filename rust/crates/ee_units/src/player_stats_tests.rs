use super::*;

fn p(value: c_ulong, time: c_ulong) -> EeUnitsStatPoint {
    EeUnitsStatPoint { value, time }
}

#[test]
fn a_clear_leaves_one_zero_point_per_series_and_12_to_16_keep_their_history() {
    let s = EeUnitsPlayerStats::default();
    assert_eq!(s.series.len(), 31);
    for (i, x) in s.series.iter().enumerate() {
        assert_eq!(x.points, vec![p(0, 0)]);
        assert_eq!(x.max, 0);
        assert_eq!(x.keep, (12..=16).contains(&i), "series {i}");
    }
}

#[test]
fn set_replaces_the_last_point_unless_the_series_keeps_its_history() {
    let mut s = EeUnitsPlayerStats::default();
    s.set(3, 7, 100, 0);
    s.set(3, 5, 200, 0);
    assert_eq!(s.series[3].points, vec![p(5, 200)]);
    assert_eq!(s.max(3), 7);
    s.set(13, 7, 100, 0);
    s.set(13, 5, 200, 0);
    assert_eq!(s.series[13].points, vec![p(0, 0), p(7, 100), p(5, 200)]);
    assert_eq!(s.last(13), 5);
    assert_eq!(s.count_increases(13), 1);
}

#[test]
fn the_gate_drops_a_point_while_shutting_down_or_loading_and_most_series_in_the_editor() {
    let mut s = EeUnitsPlayerStats::default();
    s.set(3, 7, 1, EE_UNITS_STATS_SHUTDOWN);
    s.set(3, 7, 1, EE_UNITS_STATS_LOADING);
    s.set(3, 7, 1, EE_UNITS_STATS_EDITOR);
    assert_eq!(s.last(3), 0);
    s.set(19, 7, 1, EE_UNITS_STATS_EDITOR);
    s.set(12, 8, 1, EE_UNITS_STATS_EDITOR);
    assert_eq!((s.last(19), s.last(12)), (7, 8));
    // a series past the 31 is ignored
    s.set(40, 1, 1, 0);
}

#[test]
fn raise_and_lower_move_the_last_value_and_lower_never_below_zero() {
    let mut s = EeUnitsPlayerStats::default();
    s.raise(0x18, 3, 10, 0);
    s.raise(0x18, 2, 20, 0);
    assert_eq!(s.series[0x18].points, vec![p(5, 20)]);
    s.lower(0x18, 6, 30, 0);
    assert_eq!(s.last(0x18), 5);
    s.lower(0x18, 5, 30, 0);
    assert_eq!(s.series[0x18].points, vec![p(0, 30)]);
    assert_eq!(s.max(0x18), 5);
    // a negative amount is the C++ unsigned long's wrap
    s.raise(1, (-1i64) as c_ulong, 1, 0);
    assert_eq!(s.last(1), c_ulong::MAX);
}

#[test]
fn the_last_value_of_a_series_without_points_is_zero() {
    let mut s = EeUnitsPlayerStats::default();
    s.series[4].points.clear();
    assert_eq!(s.last(4), 0);
    s.raise(4, 2, 5, 0);
    assert_eq!(s.series[4].points, vec![p(2, 5)]);
}

#[test]
fn the_epoch_compaction_drops_zeros_and_repeats() {
    let mut s = EeUnitsPlayerStats::default();
    let points = [p(0, 0), p(1, 0), p(1, 50), p(2, 60), p(2, 70), p(0, 80), p(3, 90), p(3, 95)];
    s.series[12].points = points.to_vec();
    s.series[12].max = 3;
    s.compact_epochs();
    // kept: 1 at 50 (differs from the next 2), 2 at 70, 3 at 95 (the last, differs from the kept 2); the starting zero
    // point of the new series stays
    assert_eq!(s.series[12].points, vec![p(0, 0), p(1, 50), p(2, 70), p(3, 95)]);
    assert_eq!(s.series[12].max, 3);
    assert!(s.series[12].keep);
}

#[test]
fn the_compaction_replaces_a_second_kept_point_at_time_zero() {
    let mut s = EeUnitsPlayerStats::default();
    s.series[12].points = vec![p(0, 0), p(1, 0), p(2, 0), p(3, 0)];
    s.compact_epochs();
    // 1 at 0 kept (two points: the zero and it), 2 at 0 replaces it, 3 at 0 (the last) replaces that
    assert_eq!(s.series[12].points, vec![p(0, 0), p(3, 0)]);
    // a single point: unchanged
    let mut t = EeUnitsPlayerStats::default();
    t.series[12].points = vec![p(4, 4)];
    t.compact_epochs();
    assert_eq!(t.series[12].points, vec![p(4, 4)]);
}

/// A file of words: writes append, reads take the next or fail at the end (the old value kept).
#[derive(Default)]
struct File {
    words: Vec<i64>,
    at: usize,
}

impl File {
    fn prim(&mut self, op: i32, io: &mut EeUnitsPlayerSaveIo) -> i32 {
        match op {
            EE_UNITS_PLAYER_SAVE_WRITE_ULONG => self.words.push(io.i),
            EE_UNITS_PLAYER_SAVE_WRITE_BOOL => self.words.push(i64::from(io.b)),
            EE_UNITS_PLAYER_SAVE_READ_ULONG | EE_UNITS_PLAYER_SAVE_READ_BOOL => {
                let Some(&w) = self.words.get(self.at) else { return 5 };
                self.at += 1;
                if op == EE_UNITS_PLAYER_SAVE_READ_ULONG {
                    io.i = w;
                } else {
                    io.b = w as u8;
                }
            }
            _ => return -1,
        }
        0
    }
}

#[test]
fn a_save_loads_back_equal_with_the_history_flags_as_they_were() {
    let mut s = EeUnitsPlayerStats::default();
    s.set(13, 4, 10, 0);
    s.set(2, 9, 11, 0);
    let mut f = File::default();
    assert_eq!(save(&s, 1234, |op, io| f.prim(op, io)), 0);
    // the end time, 31, then per series max, flag, count, points
    assert_eq!(&f.words[..2], &[1234, 31]);
    assert_eq!(f.words.len(), 2 + 31 * 3 + 2 * 31 + 2);
    let mut t = EeUnitsPlayerStats::default();
    t.series[2].keep = true;
    let mut end = 0;
    assert_eq!(load(&mut t, &mut end, |op, io| f.prim(op, io)), 0);
    assert_eq!(end, 1234);
    assert!(t.series[2].keep, "the saved flag is read and dropped");
    t.series[2].keep = false;
    assert_eq!(t, s);
}

#[test]
fn a_load_past_31_series_reads_into_a_scratch_series_and_a_short_file_stops_it() {
    let mut f = File { words: vec![7, 33], at: 0 };
    for i in 0..33 {
        f.words.extend([i, 1, 1, i * 10, i]);
    }
    let mut t = EeUnitsPlayerStats::default();
    let mut end = 0;
    assert_eq!(load(&mut t, &mut end, |op, io| f.prim(op, io)), 0);
    assert_eq!(t.series[30].points, vec![p(300, 30)]);
    assert_eq!(f.at, f.words.len());
    // a file ending inside the points: the read error, the points resized with zeros
    let mut g = File { words: vec![1, 1, 5, 0, 3, 8], at: 0 };
    let mut u = EeUnitsPlayerStats::default();
    assert_eq!(load(&mut u, &mut end, |op, io| g.prim(op, io)), 5);
    assert_eq!(u.series[0].points, vec![p(8, 0), p(0, 0), p(0, 0)]);
    assert_eq!(u.series[0].max, 5);
}

#[test]
fn the_ffi_copies_points_and_numbers() {
    let s = ee_units_stats_new();
    // SAFETY: a live object from ee_units_stats_new, freed at the end
    unsafe {
        ee_units_stats_set(s, 0, 14, 3, 100, 0);
        ee_units_stats_set(s, 1, 14, 2, 200, 0);
        ee_units_stats_set(s, 2, 14, 1, 300, 0);
        assert_eq!(ee_units_stats_value(s, 14, 0), 5);
        assert_eq!(ee_units_stats_value(s, 14, 1), 4);
        assert_eq!(ee_units_stats_value(s, 14, 2), 2);
        assert_eq!(ee_units_stats_value(s, 14, 3), 4);
        assert_eq!(ee_units_stats_value(s, 14, 4), 1);
        let mut out = [EeUnitsStatPoint::default(); 3];
        assert_eq!(ee_units_stats_points(s, 14, out.as_mut_ptr(), 3), 3);
        assert_eq!(out, [p(0, 0), p(3, 100), p(5, 200)]);
        let t = ee_units_stats_new();
        ee_units_stats_copy(t, s);
        assert_eq!(*t, *s);
        ee_units_stats_clear(t);
        assert_eq!(ee_units_stats_value(t, 14, 3), 1);
        ee_units_stats_free(t);
        ee_units_stats_free(s);
    }
}
