//! The world's statistics history (game core lane, round 16, fork B; was `EEWorld::mStatistics`, the classes
//! EEStatisticsHistory and EEStatisticsRecord of ee/EEStatisticsHistory.h and ee/EEStatisticsRecord.h): the
//! end-of-game records `EEWorld::RecordStatistics` fills (world_stats.rs), one per player in the summary's order,
//! and the history's own fields. One world, so one history: a static. Its readers are the statistics sweep
//! (stats_capture.rs, WSTA), the post-game summary (screens post_game_calls.rs through the `ee_screenscpp_pgs_*` and
//! `ee_screenscpp_pr_*` shims, which were EEScreens.cpp's one-statement reads of the C++ records and are these
//! reads now) and the game view's texts.
//!
//! A record's statistics are the player's series copied (`EeUnitsPlayerStats`, ee_sim units player_stats.rs); the
//! strings are the UTF-8 bytes a UWideString holds.

use std::ffi::{c_ulong, c_void};
use std::sync::Mutex;

use ee_sim::units::player_stats::{
    EeUnitsPlayerStats, EeUnitsStatPoint, EeUnitsStatsHistoryTail, EeUnitsStatsRecordTail, STATS_RECORD_CLEAR,
};

/// One player's end-of-game record (`EEStatisticsRecord`; record 0 is gaia's).
#[derive(Clone, Debug, PartialEq)]
pub struct StatsRecord {
    /// `mStatistics`: the player's series copied
    pub statistics: EeUnitsPlayerStats,
    /// `mStatistics.mEndTime`
    pub statistics_end_time: c_ulong,
    /// `mName` (the player's name and the numbered suffix)
    pub name: Vec<u8>,
    /// `mNameSuffix` (never set: cleared only)
    pub name_suffix: Vec<u8>,
    /// `mColor` .. `mIsHuman`
    pub tail: EeUnitsStatsRecordTail,
}

impl Default for StatsRecord {
    /// `EEStatisticsRecord::Clear`: no names, cleared statistics, white, no slot.
    fn default() -> Self {
        StatsRecord {
            statistics: EeUnitsPlayerStats::default(),
            statistics_end_time: 0,
            name: Vec::new(),
            name_suffix: Vec::new(),
            tail: STATS_RECORD_CLEAR,
        }
    }
}

/// The history (`EEStatisticsHistory`).
#[derive(Clone, Debug, Default, PartialEq)]
pub struct StatsHistory {
    pub records: Vec<StatsRecord>,
    /// `mCampaignName`
    pub campaign_name: Vec<u8>,
    /// `mStartTime` .. `mIsCampaign`
    pub tail: EeUnitsStatsHistoryTail,
}

impl StatsHistory {
    /// `EEStatisticsHistory::Reset`: the plain members zeroed, no campaign name, no records.
    pub fn reset(&mut self) {
        *self = StatsHistory::default();
    }

    /// `GetCurrentRecord().mSlot` (0xff, a cleared record's, when the index names none).
    pub fn current_slot(&self) -> u8 {
        self.records.get(self.tail.current_record_index as usize).map_or(STATS_RECORD_CLEAR.slot, |r| r.tail.slot)
    }
}

static HISTORY: Mutex<StatsHistory> =
    Mutex::new(StatsHistory { records: Vec::new(), campaign_name: Vec::new(), tail: HISTORY_CLEAR });

const HISTORY_CLEAR: EeUnitsStatsHistoryTail = EeUnitsStatsHistoryTail {
    start_time: 0,
    current_record_index: 0,
    end_time: 0,
    tile_count: 0,
    teams_locked: false,
    victory_enabled: false,
    is_scenario: false,
    is_campaign: false,
};

/// The series `ee_screenscpp_pr_series_load` copied (the C++'s scratch EEStatisticsSeries).
static SERIES: Mutex<Vec<EeUnitsStatPoint>> = Mutex::new(Vec::new());

/// The world's history.
pub fn with<R>(f: impl FnOnce(&mut StatsHistory) -> R) -> R {
    f(&mut HISTORY.lock().unwrap_or_else(|e| e.into_inner()))
}

/// Record `record`'s plain members (a cleared record's past the end).
fn tail(record: u32) -> EeUnitsStatsRecordTail {
    with(|h| h.records.get(record as usize).map_or(STATS_RECORD_CLEAR, |r| r.tail))
}

fn series<R: Default>(record: u32, f: impl FnOnce(&EeUnitsPlayerStats) -> R) -> R {
    with(|h| h.records.get(record as usize).map(|r| f(&r.statistics)).unwrap_or_default())
}

/// Appends to a shim's text (`EEShimOut`).
///
/// # Safety
/// `out` is an `EeScreensShimOut` of a live `Vec<u8>`.
unsafe fn shim_out(out: *mut c_void, text: &[u8]) {
    // SAFETY: the caller's contract
    unsafe { crate::screens::game_cpp::ee_screens_shim_out(out, text.as_ptr(), text.len()) };
}

// ---- the game view's texts and the game form (were EEScreens.cpp's and EEUserInterface.cpp's shims)

/// `gWorld.mStatistics.mCampaignName`.
///
/// # Safety
/// `out` is an `EeScreensShimOut` of a live `Vec<u8>`.
#[no_mangle]
pub unsafe extern "C" fn ee_screenscpp_gt_world_campaign_name(out: *mut c_void) {
    let name = with(|h| h.campaign_name.clone());
    // SAFETY: the caller's contract
    unsafe { shim_out(out, &name) };
}

/// The records' count.
#[no_mangle]
pub extern "C" fn ee_screenscpp_gt_record_count() -> u32 {
    with(|h| h.records.len() as u32)
}

/// Record `index`'s name.
///
/// # Safety
/// `out` is an `EeScreensShimOut` of a live `Vec<u8>`.
#[no_mangle]
pub unsafe extern "C" fn ee_screenscpp_gt_record_name(index: u32, out: *mut c_void) {
    let name = with(|h| h.records.get(index as usize).map(|r| r.name.clone()).unwrap_or_default());
    // SAFETY: the caller's contract
    unsafe { shim_out(out, &name) };
}

/// `gWorld.mStatistics.GetRecords().size() == 1`.
#[no_mangle]
pub extern "C" fn ee_screenscpp_ug_one_record() -> bool {
    with(|h| h.records.len() == 1)
}

// ---- the post-game summary's records and series (post_game_calls.rs; were EEScreens.cpp's)

#[no_mangle]
pub extern "C" fn ee_screenscpp_pgs_records() -> u32 {
    with(|h| h.records.len() as u32)
}

#[no_mangle]
pub extern "C" fn ee_screenscpp_pgs_start_time() -> i32 {
    with(|h| h.tail.start_time as i32)
}

#[no_mangle]
pub extern "C" fn ee_screenscpp_pgs_end_time() -> i32 {
    with(|h| h.tail.end_time as i32)
}

#[no_mangle]
pub extern "C" fn ee_screenscpp_pgs_current_record() -> i32 {
    with(|h| h.tail.current_record_index as i32)
}

#[no_mangle]
pub extern "C" fn ee_screenscpp_pgs_tile_count() -> i32 {
    with(|h| h.tail.tile_count as i32)
}

#[no_mangle]
pub extern "C" fn ee_screenscpp_pgs_victory_enabled() -> bool {
    with(|h| h.tail.victory_enabled)
}

#[no_mangle]
pub extern "C" fn ee_screenscpp_pgs_is_scenario() -> bool {
    with(|h| h.tail.is_scenario)
}

#[no_mangle]
pub extern "C" fn ee_screenscpp_pgs_is_campaign() -> bool {
    with(|h| h.tail.is_campaign)
}

#[no_mangle]
pub extern "C" fn ee_screenscpp_pgs_current_slot() -> u8 {
    with(|h| h.current_slot())
}

#[no_mangle]
pub extern "C" fn ee_screenscpp_pr_red(record: u32) -> f32 {
    tail(record).color[0]
}

#[no_mangle]
pub extern "C" fn ee_screenscpp_pr_green(record: u32) -> f32 {
    tail(record).color[1]
}

#[no_mangle]
pub extern "C" fn ee_screenscpp_pr_blue(record: u32) -> f32 {
    tail(record).color[2]
}

#[no_mangle]
pub extern "C" fn ee_screenscpp_pr_result(record: u32) -> i32 {
    tail(record).result as i32
}

#[no_mangle]
pub extern "C" fn ee_screenscpp_pr_team(record: u32) -> u32 {
    tail(record).team as u32
}

#[no_mangle]
pub extern "C" fn ee_screenscpp_pr_end_time(record: u32) -> u32 {
    tail(record).end_time as u32
}

#[no_mangle]
pub extern "C" fn ee_screenscpp_pr_resource(record: u32, index: u32) -> u32 {
    tail(record).resources.get(index as usize).map_or(0, |v| *v as u32)
}

#[no_mangle]
pub extern "C" fn ee_screenscpp_pr_score(record: u32) -> u32 {
    tail(record).score as u32
}

#[no_mangle]
pub extern "C" fn ee_screenscpp_pr_economy_score(record: u32) -> u32 {
    tail(record).economy_score as u32
}

#[no_mangle]
pub extern "C" fn ee_screenscpp_pr_epoch_score(record: u32) -> u32 {
    tail(record).epoch_score as u32
}

#[no_mangle]
pub extern "C" fn ee_screenscpp_pr_military_score(record: u32) -> u32 {
    tail(record).military_score as u32
}

#[no_mangle]
pub extern "C" fn ee_screenscpp_pr_tech_score(record: u32) -> u32 {
    tail(record).tech_score as u32
}

#[no_mangle]
pub extern "C" fn ee_screenscpp_pr_slot(record: u32) -> u8 {
    tail(record).slot
}

#[no_mangle]
pub extern "C" fn ee_screenscpp_pr_human(record: u32) -> bool {
    tail(record).is_human
}

/// `mStatistics.GetValue(series)`: the series' largest value.
#[no_mangle]
pub extern "C" fn ee_screenscpp_pr_value(record: u32, index: u32) -> u32 {
    series(record, |s| s.max(index as usize)) as u32
}

/// `mStatistics.GetLastValue(series)`.
#[no_mangle]
pub extern "C" fn ee_screenscpp_pr_last_value(record: u32, index: u32) -> u32 {
    series(record, |s| s.last(index as usize)) as u32
}

/// `mStatistics.CountIncreases(series)`.
#[no_mangle]
pub extern "C" fn ee_screenscpp_pr_increases(record: u32, index: u32) -> u32 {
    series(record, |s| s.count_increases(index as usize)) as u32
}

/// `mStatistics.GetSeries(series)` kept for the three reads below.
#[no_mangle]
pub extern "C" fn ee_screenscpp_pr_series_load(record: u32, index: u32) {
    let points = series(record, |s| s.series.get(index as usize).map(|x| x.points.clone()).unwrap_or_default());
    *SERIES.lock().unwrap_or_else(|e| e.into_inner()) = points;
}

#[no_mangle]
pub extern "C" fn ee_screenscpp_pr_series_count() -> u32 {
    SERIES.lock().unwrap_or_else(|e| e.into_inner()).len() as u32
}

#[no_mangle]
pub extern "C" fn ee_screenscpp_pr_series_time(index: u32) -> u32 {
    SERIES.lock().unwrap_or_else(|e| e.into_inner()).get(index as usize).map_or(0, |p| p.time as u32)
}

#[no_mangle]
pub extern "C" fn ee_screenscpp_pr_series_value(index: u32) -> u32 {
    SERIES.lock().unwrap_or_else(|e| e.into_inner()).get(index as usize).map_or(0, |p| p.value as u32)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_cleared_record_is_white_without_a_slot() {
        let r = StatsRecord::default();
        assert_eq!(r.tail.color, [1.0, 1.0, 1.0]);
        assert_eq!(r.tail.slot, 0xff);
        assert!(r.name.is_empty() && r.name_suffix.is_empty());
        assert_eq!(r.statistics, EeUnitsPlayerStats::default());
        assert_eq!(r.statistics_end_time, 0);
    }

    #[test]
    fn reset_drops_the_records_the_name_and_the_fields() {
        let mut h = StatsHistory::default();
        h.records.resize(3, StatsRecord::default());
        h.campaign_name = b"Greek".to_vec();
        h.tail.end_time = 9;
        h.tail.is_campaign = true;
        h.reset();
        assert_eq!(h, StatsHistory::default());
        assert_eq!(h.tail, HISTORY_CLEAR);
    }

    #[test]
    fn the_current_slot_is_the_current_records() {
        let mut h = StatsHistory::default();
        assert_eq!(h.current_slot(), 0xff);
        h.records.resize(2, StatsRecord::default());
        h.records[1].tail.slot = 4;
        h.tail.current_record_index = 1;
        assert_eq!(h.current_slot(), 4);
        h.tail.current_record_index = -1;
        assert_eq!(h.current_slot(), 0xff);
    }
}
