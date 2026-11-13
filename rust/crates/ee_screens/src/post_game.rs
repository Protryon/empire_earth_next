//! The post-game summary (form 25) whole: the original `EEUIEPostGameSummaryEvents.cpp` (its open event: the
//! statistics graphs, every player's name and bars on the seven tabs with the gold stars, the stats server's reports,
//! the victory or defeat backdrop and the end-game screenshot, the last tab; its close event; the seven tabs, Exit,
//! the players' check boxes and Next Graph; the helpers `HideTabs`, `HideBars`, `BuildGraphs`, `AddStatusBar`,
//! `Show*`, the reports' `ExportMessage`) over a host ([`PostGameHost`]) whose calls are each one toolkit or game call
//! (`UI Events/EEScreens.cpp`). The statistics reach Rust as records ([`EeScreensPgsHistory`],
//! [`EeScreensPgsRecord`]); the graphs stay the toolkit's `UGraph`s (the line graph's 31 and the form manager's
//! totals and scores), changed through the host. The bars and stars the summary adds at run time are made by the
//! host and remembered here (by control index and entity group) until the close event deletes them.

use core::ffi::{c_long, c_ulong};
use std::sync::Mutex;

use crate::menus::{EE_SCREENS_OP_SHOW, EE_SCREENS_TEXT_LABEL, EE_SCREENS_TEXT_STRING};
use crate::scenario_screens::EE_SCREENS_OP_REMOVE_BACKGROUND;
use crate::settings_form::{
    EE_SCREENS_OP_BACKGROUND, EE_SCREENS_OP_CHECK_RADIO, EE_SCREENS_OP_CLOSE_FORM, EE_SCREENS_OP_CONTROL_ID,
    EE_SCREENS_OP_GROUP, EE_SCREENS_OP_LABEL_NUMBER,
};

// ---- the events (EEScreenEvent::mEvent)

/// A tab: `EE_SCREENS_PGS_TAB + page - 1`, pages 1..7 (summary, time line, military, economy, epoch, society, misc).
pub const EE_SCREENS_PGS_TAB: u32 = 0x1901;
pub const EE_SCREENS_PGS_EXIT: u32 = 0x1908;
/// A player's check box checked / unchecked: `+ series` (0..15).
pub const EE_SCREENS_PGS_CHECKED: u32 = 0x1910;
pub const EE_SCREENS_PGS_UNCHECKED: u32 = 0x1920;
pub const EE_SCREENS_PGS_OPEN: u32 = 0x1930;
pub const EE_SCREENS_PGS_CLOSE: u32 = 0x1931;
pub const EE_SCREENS_PGS_NEXT_GRAPH: u32 = 0x1932;

pub fn is_post_game_event(event: u32) -> bool {
    (EE_SCREENS_PGS_TAB..=EE_SCREENS_PGS_EXIT).contains(&event)
        || (EE_SCREENS_PGS_CHECKED..EE_SCREENS_PGS_UNCHECKED + 16).contains(&event)
        || (EE_SCREENS_PGS_OPEN..=EE_SCREENS_PGS_NEXT_GRAPH).contains(&event)
}

/// The summary's event code for a control event id (144..150 the tabs, 151 Exit, 152..167 the check boxes checked,
/// 168..183 unchecked, 184 Next Graph), 0 for another form's.
pub fn event_for_control(id: c_ulong) -> u32 {
    const TABS: [c_ulong; 7] = [144, 145, 146, 147, 148, 149, 150];
    if let Some(k) = TABS.iter().position(|&t| t == id) {
        return EE_SCREENS_PGS_TAB + k as u32;
    }
    for (k, checked) in (152..168).enumerate() {
        if id == checked {
            return EE_SCREENS_PGS_CHECKED + k as u32;
        }
        if id == checked + 16 {
            return EE_SCREENS_PGS_UNCHECKED + k as u32;
        }
    }
    match id {
        151 => EE_SCREENS_PGS_EXIT,
        184 => EE_SCREENS_PGS_NEXT_GRAPH,
        _ => 0,
    }
}

// ---- the host's ops (ee_screenscpp_call on the summary's form; graphs are numbered PGS_GRAPH_*)

/// (page) `gGamePreferences.mSettings.mLastOpenPostGameStatsPage = page`; -> the page with `EE_SCREENS_GAME_PGS_PAGE`.
pub const EE_SCREENS_GAME_PGS_SET_PAGE: u32 = 0x2d0;
pub const EE_SCREENS_GAME_PGS_PAGE: u32 = 0x2d1;
/// (control, coordinate 0 left, 1 top, 2 right, 3 bottom) -> its rectangle's coordinate (float bits).
pub const EE_SCREENS_OP_PGS_RECT: u32 = 0x2d2;
/// (coordinate, float bits) the host's rectangle register.
pub const EE_SCREENS_OP_PGS_SET_R: u32 = 0x2d3;
/// (control) `SetRectangle(the register)`.
pub const EE_SCREENS_OP_PGS_APPLY_R: u32 = 0x2d4;
/// (control, float bits) `SetBgOpacity` (virtual).
pub const EE_SCREENS_OP_PGS_BG_OPACITY: u32 = 0x2d5;
/// (control) `SetDirty(true)`.
pub const EE_SCREENS_OP_PGS_DIRTY: u32 = 0x2d6;
/// (graph, series, enabled) the graph's `SetIsSeriesEnabled`.
pub const EE_SCREENS_OP_PGS_SERIES_ENABLED: u32 = 0x2d7;
/// (graph) the line graph's `ShowGraph`.
pub const EE_SCREENS_OP_PGS_SHOW_GRAPH: u32 = 0x2d8;
/// (graph) `ClearAllSeries`.
pub const EE_SCREENS_OP_PGS_CLEAR_GRAPH: u32 = 0x2d9;
/// (record, series) the line graph's `AddGraphSeries` of the record's statistics series in its colour.
pub const EE_SCREENS_GAME_PGS_ADD_SERIES: u32 = 0x2da;
/// (graph) `PruneGraph`.
pub const EE_SCREENS_OP_PGS_PRUNE: u32 = 0x2db;
/// The host's series register: `ClearAllDataPoints`; (x, y) `AddDataPoint`; (graph) the graph's `AddSeries` of it.
pub const EE_SCREENS_OP_PGS_SERIES_CLEAR: u32 = 0x2dc;
pub const EE_SCREENS_OP_PGS_SERIES_POINT: u32 = 0x2dd;
pub const EE_SCREENS_OP_PGS_SERIES_ADD: u32 = 0x2de;
/// Graph 0x17 rebuilt as `CombineGraph` of 0x0e and 0x0d, each series in its record's colour.
pub const EE_SCREENS_GAME_PGS_COMBINE: u32 = 0x2df;
/// The line graph's axis labels: no y, x.
pub const EE_SCREENS_OP_PGS_AXES: u32 = 0x2e0;
/// (graph) -> its `GetMaxY` / `GetMinY`; (graph, series) -> the series' `GetMaxY`; (graph) -> `GetSeriesCount`;
/// (graph, series) -> its point count; (graph, series, point, axis 0 x 1 y) -> the point's coordinate.
pub const EE_SCREENS_OP_PGS_MAX_Y: u32 = 0x2e1;
pub const EE_SCREENS_OP_PGS_MIN_Y: u32 = 0x2e2;
pub const EE_SCREENS_OP_PGS_SERIES_MAX_Y: u32 = 0x2e3;
pub const EE_SCREENS_OP_PGS_SERIES_COUNT: u32 = 0x2e4;
pub const EE_SCREENS_OP_PGS_POINT_COUNT: u32 = 0x2e5;
pub const EE_SCREENS_OP_PGS_POINT: u32 = 0x2e6;
/// `SetFormManager(new EEPGSFormManager)` (its graphs empty).
pub const EE_SCREENS_GAME_PGS_NEW_MANAGER: u32 = 0x2e7;
/// (control, red, green, blue bits) `SetBgColor` (virtual).
pub const EE_SCREENS_OP_PGS_BG_COLOUR: u32 = 0x2e8;
/// (0 height, 1 width) -> the manager's screen's window size; the form's designed size.
pub const EE_SCREENS_GAME_PGS_WINDOW: u32 = 0x2e9;
pub const EE_SCREENS_OP_PGS_DESIGNED: u32 = 0x2ea;
/// (control) -> 1 when it is a status bar / a bitmap (`dynamic_cast`) / not NULL; -> its `GetIndex`.
pub const EE_SCREENS_OP_PGS_IS_BAR: u32 = 0x2eb;
pub const EE_SCREENS_OP_PGS_IS_BITMAP: u32 = 0x2ec;
pub const EE_SCREENS_OP_PGS_EXISTS: u32 = 0x2ed;
pub const EE_SCREENS_OP_PGS_INDEX: u32 = 0x2ee;
/// (template, player, group) a new status bar like the template's (id and index + player - 1, its border) at the
/// register, no text in Tahoma 8, added to the group -> its index.
pub const EE_SCREENS_OP_PGS_NEW_BAR: u32 = 0x2ef;
/// (template, bar) the bar's string id and text style the template's.
pub const EE_SCREENS_OP_PGS_TEXT_STYLE: u32 = 0x2f0;
/// (id, index, group) a new gold star bitmap (the template's rectangle, `textures\gold star`) added to the group.
pub const EE_SCREENS_OP_PGS_NEW_STAR: u32 = 0x2f1;
/// (bar, float bits) `SetStatusBarOpacity`; (bar, max, value) `SetRange(0, max)` and `SetCurrentRangeValue`;
/// (control) -> `GetBgOpacity` bits; (bar, red, green, blue bits) `SetStatusBarColor`.
pub const EE_SCREENS_OP_PGS_BAR_OPACITY: u32 = 0x2f2;
pub const EE_SCREENS_OP_PGS_BAR_RANGE: u32 = 0x2f3;
pub const EE_SCREENS_OP_PGS_GET_BG_OPACITY: u32 = 0x2f4;
pub const EE_SCREENS_OP_PGS_BAR_COLOUR: u32 = 0x2f5;
/// `NEProtocol::GetProtocol(0)` for the reports.
pub const EE_SCREENS_GAME_PGS_PROTOCOL: u32 = 0x2f6;
/// -> `mTakeEndGameScreenshots`; the end-game screenshot's backdrop (the last `EEPostGame-M-D-Y-???.jpg` of today,
/// when there is one).
pub const EE_SCREENS_GAME_PGS_SCREENSHOTS: u32 = 0x2f7;
pub const EE_SCREENS_GAME_PGS_SCREENSHOT: u32 = 0x2f8;
/// `EEUISCIFormManager::Get()->DeleteObjects()`.
pub const EE_SCREENS_GAME_PGS_SCI_DELETE: u32 = 0x2f9;
/// (control, group) `RemoveControl` and delete it.
pub const EE_SCREENS_OP_PGS_REMOVE: u32 = 0x2fa;
/// `gGamePreferences.Save()`; WON's `RejoinRoom(false)` when there is WON.
pub const EE_SCREENS_GAME_PGS_SAVE: u32 = 0x2fb;
pub const EE_SCREENS_GAME_PGS_REJOIN: u32 = 0x2fc;
/// The campaign manager's first scenario and description from its loaded campaign, and all its scenarios open for
/// the demo and learning campaigns; (none) its name, first scenario and description cleared.
pub const EE_SCREENS_GAME_PGS_CAMPAIGN_SETUP: u32 = 0x2fd;
pub const EE_SCREENS_GAME_PGS_CAMPAIGN_CLEAR: u32 = 0x2fe;
/// Texts in: record `i`'s name; the history's campaign name; the stats server's file ("host\0port"), missing when it
/// does not open or there are no communications.
pub const EE_SCREENS_TEXT_PGS_NAME: u32 = 0x88;
pub const EE_SCREENS_TEXT_PGS_CAMPAIGN: u32 = 0x89;
pub const EE_SCREENS_TEXT_PGS_SERVER: u32 = 0x8a;
/// Put (graph): its title, y label, x label.
pub const EE_SCREENS_TEXT_PGS_TITLE: u32 = 0x8b;
pub const EE_SCREENS_TEXT_PGS_Y_LABEL: u32 = 0x8c;
pub const EE_SCREENS_TEXT_PGS_X_LABEL: u32 = 0x8d;
/// Put (bar): `SetText(text, NULL)`.
pub const EE_SCREENS_TEXT_PGS_BAR: u32 = 0x8e;
/// Put (port): "host\0payload" sent with the protocol's `SendSingle`.
pub const EE_SCREENS_TEXT_PGS_SEND: u32 = 0x8f;
/// Put: the campaign `Campaigns\NAME.ext` loaded -> 1 when it loads; its name set on the campaign manager.
pub const EE_SCREENS_TEXT_PGS_CAMPAIGN_LOAD: u32 = 0x90;
pub const EE_SCREENS_TEXT_PGS_CAMPAIGN_NAME: u32 = 0x91;

/// The graphs: the line graph's stored 0..=30, its shown one, the form manager's.
pub const PGS_GRAPH_SHOWN: u32 = 31;
const PGS_GRAPH_ECONOMY_SCORE: u32 = 32;
const PGS_GRAPH_TOTAL_COLLECTED: u32 = 33;
const PGS_GRAPH_TOTAL_SPENT: u32 = 34;
const PGS_GRAPH_EPOCH_SCORE: u32 = 35;
const PGS_GRAPH_MILITARY_SCORE: u32 = 36;
const PGS_GRAPH_SPEED_RATING: u32 = 37;
const PGS_GRAPH_SOCIETY_SCORE: u32 = 38;
const PGS_GRAPH_SCORE: u32 = 39;

// the controls, groups, forms, strings
const PGS_LINE_GRAPH: u32 = 0;
const PGS_TAB_SUMMARY: u32 = 34;
const PGS_TAB_TIME_LINE: u32 = 35;
const PGS_TAB_MILITARY: u32 = 36;
const PGS_TAB_ECONOMY: u32 = 37;
const PGS_TAB_EPOCH: u32 = 38;
const PGS_TAB_SOCIETY: u32 = 39;
const PGS_TAB_MISC: u32 = 40;
const PGS_MAIN_TITLE: u32 = 43;
const PGS_GOLD_STAR: u32 = 790;
const PGS_FORM_CAMPAIGN_SELECT: u32 = 33;
const PGS_IDS_POST_GAME_STATISTICS: u32 = 31232;
const PGS_IDS_SPACED_DASH: u32 = 31282;
const PGS_IDS_TABS: [u32; 7] = [31237, 31238, 31239, 31240, 31241, 31242, 31244];
const PGS_IDS_STATISTIC_SERIES_FIRST: u32 = 31200;
const PGS_IDS_OVER_TIME: u32 = 31233;
const PGS_IDS_GAME_TIME_MINUTES_SECONDS: u32 = 31234;
const PGS_IDS_N: u32 = 31281;
const PGS_HIDDEN: u32 = 0;
const PGS_SHOWN: u32 = 1;
/// The bars' opacity (`kBarOpacity`).
const PGS_BAR_OPACITY: f32 = 0.38;

/// The statistics' history as the summary reads it (`gWorld.mStatistics`, `ee_screenscpp_pgs_history`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScreensPgsHistory {
    /// The number of records (the first is no player's).
    pub records: u32,
    pub start_time: i32,
    pub end_time: i32,
    pub current_record: i32,
    pub tile_count: i32,
    pub victory_enabled: bool,
    pub is_scenario: bool,
    pub is_campaign: bool,
    /// The current record's slot (`GetCurrentRecord().mSlot`).
    pub current_slot: u8,
    /// The starting resources' level amounts (`EEGetResourceLevel(gGameResources)->mAmounts[0..5]`).
    pub levels: [u32; 5],
}

/// A player's statistics record (`ee_screenscpp_pgs_record`).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeScreensPgsRecord {
    pub colour: [f32; 3],
    pub result: i32,
    pub team: u32,
    pub end_time: u32,
    pub resources: [u32; 7],
    pub score: u32,
    pub economy_score: u32,
    pub epoch_score: u32,
    pub military_score: u32,
    pub tech_score: u32,
    pub slot: u8,
    pub human: bool,
    /// `mStatistics.GetValue(j)` for the series 0..30.
    pub values: [u32; 31],
    /// `GetLastValue(0x18)`, `CountIncreases(0x0f)`.
    pub territory: u32,
    pub increases: u32,
    /// Series 12's point count and its second point's value (0 without one).
    pub epoch_points: u32,
    pub epoch_second: u32,
}

impl Default for EeScreensPgsRecord {
    fn default() -> Self {
        EeScreensPgsRecord {
            colour: [0.0; 3],
            result: 0,
            team: 0,
            end_time: 0,
            resources: [0; 7],
            score: 0,
            economy_score: 0,
            epoch_score: 0,
            military_score: 0,
            tech_score: 0,
            slot: 0,
            human: false,
            values: [0; 31],
            territory: 0,
            increases: 0,
            epoch_points: 0,
            epoch_second: 0,
        }
    }
}

/// A player's report to the stats server (`EEPGSPlayerReport`).
#[derive(Clone, Debug, Default, PartialEq)]
struct PlayerReport {
    name: Vec<u8>,
    start_time: i32,
    units_lost: i32,
    units_killed: i32,
    result: i32,
    human: bool,
}

/// The game's report (`EEPGSGameReport`).
#[derive(Clone, Copy, Debug, Default, PartialEq)]
struct GameReport {
    start_time: i32,
    end_time: i32,
    players: u32,
    humans: u32,
}

/// The summary's state: the shown graph's cycle, the controls it added, the time line's epochs, the reports.
#[derive(Debug, Default)]
pub struct PostGame {
    graph_index: u32,
    /// The bars and stars added (control index, entity group), in order.
    controls: Vec<(u32, u32)>,
    first_time: u32,
    last_time: u32,
    reports: Vec<PlayerReport>,
    game: GameReport,
}

impl PostGame {
    pub const fn new() -> Self {
        PostGame {
            graph_index: 0,
            controls: Vec::new(),
            first_time: 0,
            last_time: 0,
            reports: Vec::new(),
            game: GameReport { start_time: 0, end_time: 0, players: 0, humans: 0 },
        }
    }
}

/// The calls of the post-game summary.
pub trait PostGameHost {
    /// `EE_SCREENS_OP_*` / `EE_SCREENS_GAME_*` with up to four arguments and an answer.
    fn call(&mut self, op: u32, a: u32, b: u32, c: u32, d: u32) -> u32;
    /// A text of the game; `None` when it is missing.
    fn text(&mut self, which: u32, arg: u32) -> Option<Vec<u8>>;
    /// A text into the game, and the host's answer.
    fn put_text(&mut self, which: u32, arg: u32, text: &[u8]) -> u32;
    /// The statistics' history.
    fn history(&mut self) -> EeScreensPgsHistory;
    /// Record `index`.
    fn record(&mut self, index: u32) -> EeScreensPgsRecord;
}

fn op<H: PostGameHost + ?Sized>(h: &mut H, op: u32, a: u32) -> u32 {
    h.call(op, a, 0, 0, 0)
}
fn op2<H: PostGameHost + ?Sized>(h: &mut H, op: u32, a: u32, b: u32) -> u32 {
    h.call(op, a, b, 0, 0)
}
fn string<H: PostGameHost + ?Sized>(h: &mut H, id: u32) -> Vec<u8> {
    h.text(EE_SCREENS_TEXT_STRING, id).unwrap_or_default()
}
fn f(bits: u32) -> f32 {
    f32::from_bits(bits)
}
/// A float's truncation as x64's `cvttss2si` (the original's `(long)` casts): out of range or NaN gives `i32::MIN`.
fn truncate(value: f32) -> i32 {
    if value.is_nan() || value >= 2147483648.0 || value < -2147483648.0 {
        i32::MIN
    } else {
        value as i32
    }
}
/// `sprintf("%.2ld")`: at least two digits.
fn two_digits(value: i32) -> String {
    if value < 0 {
        format!("-{:02}", value.unsigned_abs())
    } else {
        format!("{value:02}")
    }
}
/// `atoi` as the UCRT's: leading blanks, a sign, digits; out of range clamps.
fn atoi(text: &[u8]) -> i32 {
    let mut i = 0;
    while i < text.len() && (text[i] == b' ' || (b'\t'..=b'\r').contains(&text[i])) {
        i += 1;
    }
    let negative = i < text.len() && text[i] == b'-';
    if i < text.len() && (text[i] == b'-' || text[i] == b'+') {
        i += 1;
    }
    let mut value: i64 = 0;
    while i < text.len() && text[i].is_ascii_digit() {
        value = (value * 10 + (text[i] - b'0') as i64).min(1 << 40);
        i += 1;
    }
    let value = if negative { -value } else { value };
    value.clamp(i32::MIN as i64, i32::MAX as i64) as i32
}

/// A control's rectangle (left, top, right, bottom).
fn rect<H: PostGameHost + ?Sized>(h: &mut H, control: u32) -> [f32; 4] {
    let mut r = [0.0; 4];
    for (k, v) in r.iter_mut().enumerate() {
        *v = f(op2(h, EE_SCREENS_OP_PGS_RECT, control, k as u32));
    }
    r
}
fn set_rect<H: PostGameHost + ?Sized>(h: &mut H, control: u32, r: [f32; 4]) {
    load_r(h, r);
    op(h, EE_SCREENS_OP_PGS_APPLY_R, control);
}
fn load_r<H: PostGameHost + ?Sized>(h: &mut H, r: [f32; 4]) {
    for (k, v) in r.iter().enumerate() {
        op2(h, EE_SCREENS_OP_PGS_SET_R, k as u32, v.to_bits());
    }
}
fn show<H: PostGameHost + ?Sized>(h: &mut H, control: u32, mode: u32) {
    op2(h, EE_SCREENS_OP_SHOW, control, mode);
}
fn colour_op<H: PostGameHost + ?Sized>(h: &mut H, op: u32, control: u32, c: [f32; 3]) {
    h.call(op, control, c[0].to_bits(), c[1].to_bits(), c[2].to_bits());
}

/// `SetTabOpacity`: the 16 player name labels' background opacity, each redrawn.
fn tab_opacity<H: PostGameHost + ?Sized>(h: &mut H, opacity: f32) {
    for i in 1..17 {
        op2(h, EE_SCREENS_OP_PGS_BG_OPACITY, i, opacity.to_bits());
        op(h, EE_SCREENS_OP_PGS_DIRTY, i);
    }
}

/// `HideTabs`: every tab's group hidden; in a scenario the Economy tab hidden and the Summary, Military and Society
/// tabs moved into the Time Line, Epoch and Misc tabs' columns; the name labels transparent.
fn hide_tabs<H: PostGameHost + ?Sized>(h: &mut H) {
    for i in 1..=7 {
        h.call(EE_SCREENS_OP_GROUP, i, 0, 0, 0);
    }
    if h.history().is_scenario {
        show(h, PGS_TAB_ECONOMY, PGS_HIDDEN);
        for (column, moved) in
            [(PGS_TAB_TIME_LINE, PGS_TAB_SUMMARY), (PGS_TAB_EPOCH, PGS_TAB_MILITARY), (PGS_TAB_MISC, PGS_TAB_SOCIETY)]
        {
            let mut r = rect(h, column);
            let m = rect(h, moved);
            r[1] = m[1];
            r[3] = m[3];
            set_rect(h, moved, r);
        }
    }
    tab_opacity(h, 0.0);
}

/// A tab's event: the page remembered, the title "Post Game Statistics - TAB", the tabs hidden, the page's group
/// shown (the time line's names tinted).
fn tab<H: PostGameHost + ?Sized>(h: &mut H, page: u32) {
    op(h, EE_SCREENS_GAME_PGS_SET_PAGE, page);
    let summary = string(h, PGS_IDS_POST_GAME_STATISTICS);
    let separator = string(h, PGS_IDS_SPACED_DASH);
    let name = string(h, PGS_IDS_TABS[page as usize - 1]);
    let mut title = summary;
    title.push(b' ');
    title.extend_from_slice(&separator);
    title.push(b' ');
    title.extend_from_slice(&name);
    h.put_text(EE_SCREENS_TEXT_LABEL, PGS_MAIN_TITLE, &title);
    hide_tabs(h);
    h.call(EE_SCREENS_OP_GROUP, page, 1, 0, 0);
    if page == 2 {
        tab_opacity(h, PGS_BAR_OPACITY);
    }
}

/// `SetSeriesEnabled`: one player's series shown or hidden on the shown graph and the 30 statistics graphs.
fn series_enabled<H: PostGameHost + ?Sized>(h: &mut H, series: u32, enabled: bool) {
    h.call(EE_SCREENS_OP_PGS_SERIES_ENABLED, PGS_GRAPH_SHOWN, series, enabled as u32, 0);
    for i in 1..31 {
        h.call(EE_SCREENS_OP_PGS_SERIES_ENABLED, i, series, enabled as u32, 0);
    }
    op(h, EE_SCREENS_OP_PGS_DIRTY, PGS_LINE_GRAPH);
}

/// `HideBars`: every name, check box and team number, every bar and star, the epoch labels hidden.
fn hide_bars<H: PostGameHost + ?Sized>(h: &mut H) {
    for i in 1..17 {
        show(h, i, PGS_HIDDEN);
        show(h, i + 16, PGS_HIDDEN);
        show(h, i + 0x5eb, PGS_HIDDEN);
    }
    for i in 0x32..=0x315 {
        let star = op(h, EE_SCREENS_OP_PGS_IS_BITMAP, i + 0x2e4) != 0;
        let bar = op(h, EE_SCREENS_OP_PGS_IS_BAR, i) != 0;
        if bar {
            show(h, i, PGS_HIDDEN);
        }
        if star {
            show(h, i + 0x2e4, PGS_HIDDEN);
        }
    }
    for i in 0x208..=0x216 {
        if op(h, EE_SCREENS_OP_PGS_EXISTS, i) != 0 {
            show(h, i, PGS_HIDDEN);
        }
    }
}

/// `BuildGraphs`: the 30 statistics graphs from each player's history and the totals (collected, net spent, the
/// speed rating, the scores); graph 0x17 as 0x0e combined with 0x0d, shown; the time line's first and last epochs.
fn build_graphs<H: PostGameHost + ?Sized>(h: &mut H, state: &mut PostGame) {
    let history = h.history();
    let mut last: u32 = 0;
    let mut first_time: u32 = 15;
    for i in 1..history.records {
        let record = h.record(i);
        let seconds = record.end_time as f32 * 0.001f32;
        let mut produced: u32 = 0;
        let mut gathered: u32 = 0;
        let mut spent: u32 = 0;
        let mut tribute: u32 = 0;
        let mut clicks: u32 = 0;
        let mut hot_keys: u32 = 0;
        for j in 1..31u32 {
            let value = record.values[j as usize];
            match j {
                12 => {
                    if value > last {
                        last = value;
                    }
                    let start = if record.epoch_points <= 1 { 0 } else { record.epoch_second };
                    if start < first_time {
                        first_time = start;
                    }
                }
                18 => tribute = value,
                20 => hot_keys = value,
                21 => clicks = value,
                _ => {}
            }
            // the resources' series 7..11: food, wood, gold, stone, iron
            let resource = match j {
                7 => Some((2, 0)),
                8 => Some((3, 1)),
                9 => Some((5, 3)),
                10 => Some((4, 2)),
                11 => Some((6, 4)),
                _ => None,
            };
            if let Some((kind, level)) = resource {
                gathered = gathered.wrapping_add(value);
                spent = spent.wrapping_add(record.resources[kind]);
                produced = produced.wrapping_add(history.levels[level]);
            }
            op2(h, EE_SCREENS_GAME_PGS_ADD_SERIES, i, j);
            let name = string(h, j + PGS_IDS_STATISTIC_SERIES_FIRST);
            let over = string(h, PGS_IDS_OVER_TIME);
            let mut title = name.clone();
            title.push(b' ');
            title.extend_from_slice(&over);
            h.put_text(EE_SCREENS_TEXT_PGS_TITLE, j, &title);
            let label = string(h, PGS_IDS_GAME_TIME_MINUTES_SECONDS);
            h.put_text(EE_SCREENS_TEXT_PGS_Y_LABEL, j, &name);
            h.put_text(EE_SCREENS_TEXT_PGS_X_LABEL, j, &label);
            op(h, EE_SCREENS_OP_PGS_PRUNE, j);
        }
        gathered = gathered.wrapping_add(tribute);
        let net = produced.wrapping_sub(spent).wrapping_add(gathered) as i32;
        let speed = truncate((hot_keys as f32 * 3.0f32 + clicks as f32) * 1000.0f32 / seconds + 0.5f32);
        for (graph, value) in [
            (PGS_GRAPH_TOTAL_COLLECTED, gathered),
            (PGS_GRAPH_TOTAL_SPENT, net.max(0) as u32),
            (PGS_GRAPH_SPEED_RATING, speed as u32),
            (PGS_GRAPH_SCORE, record.score),
            (PGS_GRAPH_ECONOMY_SCORE, record.economy_score),
            (PGS_GRAPH_EPOCH_SCORE, record.epoch_score),
            (PGS_GRAPH_MILITARY_SCORE, record.military_score),
            (PGS_GRAPH_SOCIETY_SCORE, record.tech_score),
        ] {
            op(h, EE_SCREENS_OP_PGS_SERIES_CLEAR, 0);
            op2(h, EE_SCREENS_OP_PGS_SERIES_POINT, 0, value);
            op(h, EE_SCREENS_OP_PGS_SERIES_ADD, graph);
        }
    }
    op(h, EE_SCREENS_GAME_PGS_COMBINE, 0);
    state.first_time = first_time;
    state.last_time = last;
    op(h, EE_SCREENS_OP_PGS_AXES, 0);
    op(h, EE_SCREENS_OP_PGS_SHOW_GRAPH, 0x17);
}

/// `AddStatusBar`: player `player`'s bar beside the template bar (made on first use, then moved), its value (or the
/// text) on it; the best value gets a gold star (the lowest for the losses' bars); a time line bar is full and flat.
#[allow(clippy::too_many_arguments)]
fn add_status_bar<H: PostGameHost + ?Sized>(
    h: &mut H,
    state: &mut PostGame,
    template: u32,
    graph: u32,
    colour: [f32; 3],
    player: u32,
    group: u32,
    text: &[u8],
) {
    let window_height = op(h, EE_SCREENS_GAME_PGS_WINDOW, 0);
    let designed_height = op(h, EE_SCREENS_OP_PGS_DESIGNED, 0);
    let template_index = op(h, EE_SCREENS_OP_PGS_INDEX, template);
    let bar = template_index.wrapping_add(player).wrapping_sub(1);
    let exists = op(h, EE_SCREENS_OP_PGS_IS_BAR, bar) != 0;
    let mut r = rect(h, template);
    let height = r[3] - r[1];
    let series = player.wrapping_sub(1);
    r[1] += (10.0f32 / designed_height as f32 * window_height as f32 + height) * series as f32;
    r[3] = r[1] + height;
    load_r(h, r);
    let bar = if !exists {
        let index = h.call(EE_SCREENS_OP_PGS_NEW_BAR, template, player, group, 0);
        state.controls.push((index, group));
        index
    } else {
        op(h, EE_SCREENS_OP_PGS_APPLY_R, bar);
        bar
    };
    let bar_index = op(h, EE_SCREENS_OP_PGS_INDEX, bar);
    let time_line = (0x226..=0x315).contains(&bar_index);
    let low_is_best = template_index == 0x42 || template_index == 0x62;
    op2(h, EE_SCREENS_OP_PGS_TEXT_STYLE, template, bar);
    let value = op2(h, EE_SCREENS_OP_PGS_SERIES_MAX_Y, graph, series) as i32;
    let shown = if !text.is_empty() { text.to_vec() } else { value.to_string().into_bytes() };
    h.put_text(EE_SCREENS_TEXT_PGS_BAR, bar, &shown);
    let star = bar_index.wrapping_add(0x2e5);
    let has_star = op(h, EE_SCREENS_OP_PGS_IS_BITMAP, star) != 0;
    if has_star {
        show(h, star, PGS_HIDDEN);
    }
    if !time_line {
        let max = op(h, EE_SCREENS_OP_PGS_MAX_Y, graph) as i32;
        let best = (!low_is_best && value == max && value != 0)
            || (low_is_best && value == op(h, EE_SCREENS_OP_PGS_MIN_Y, graph) as i32);
        if best && op(h, EE_SCREENS_OP_PGS_IS_BITMAP, PGS_GOLD_STAR) != 0 {
            let id = op(h, EE_SCREENS_OP_CONTROL_ID, PGS_GOLD_STAR).wrapping_add(bar_index).wrapping_sub(0x31);
            let bar_rect = rect(h, bar);
            let side = bar_rect[3] - bar_rect[1];
            let star_rect = [r[0], r[1], r[0] + side, r[1] + side];
            if !has_star {
                h.call(EE_SCREENS_OP_PGS_NEW_STAR, id, star, group, 0);
                state.controls.push((star, group));
            }
            set_rect(h, star, star_rect);
            show(h, star, PGS_SHOWN);
        }
        op2(h, EE_SCREENS_OP_PGS_BG_OPACITY, bar, PGS_BAR_OPACITY.to_bits());
        op2(h, EE_SCREENS_OP_PGS_BAR_OPACITY, bar, PGS_BAR_OPACITY.to_bits());
        h.call(EE_SCREENS_OP_PGS_BAR_RANGE, bar, max as u32, value as u32, 0);
    } else {
        let opacity = op(h, EE_SCREENS_OP_PGS_GET_BG_OPACITY, template);
        op2(h, EE_SCREENS_OP_PGS_BG_OPACITY, bar, opacity);
        op2(h, EE_SCREENS_OP_PGS_BAR_OPACITY, bar, opacity);
        h.call(EE_SCREENS_OP_PGS_BAR_RANGE, bar, 1, 1, 0);
    }
    colour_op(h, EE_SCREENS_OP_PGS_BAR_COLOUR, bar, colour);
    show(h, bar, PGS_SHOWN);
}

/// The bars of a tab: `count` templates from `first` 16 apart, each with its graph (`None`: skipped), in `group`.
#[allow(clippy::too_many_arguments)]
fn show_bars<H: PostGameHost + ?Sized>(
    h: &mut H,
    state: &mut PostGame,
    first: u32,
    graphs: &[u32],
    colour: [f32; 3],
    player: u32,
    group: u32,
    texts: &dyn Fn(&mut H, u32) -> Vec<u8>,
) {
    for (i, &graph) in graphs.iter().enumerate() {
        let template = first + i as u32 * 16;
        if op(h, EE_SCREENS_OP_PGS_EXISTS, template) == 0 {
            continue;
        }
        let text = texts(h, i as u32);
        add_status_bar(h, state, template, graph, colour, player, group, &text);
    }
}

/// `ShowMilitary`: the military tab's six bars; the units killed and lost into the player's report.
fn show_military<H: PostGameHost + ?Sized>(h: &mut H, state: &mut PostGame, colour: [f32; 3], player: u32) {
    let graphs = [3, 4, 2, 1, 5, 0x0d];
    for (i, &graph) in graphs.iter().enumerate() {
        let template = 0x32 + i as u32 * 16;
        if op(h, EE_SCREENS_OP_PGS_EXISTS, template) == 0 {
            continue;
        }
        if i < 2 {
            let value = op2(h, EE_SCREENS_OP_PGS_SERIES_MAX_Y, graph, player.wrapping_sub(1)) as i32;
            let report = &mut state.reports[player as usize - 1];
            if i == 0 {
                report.units_killed = value;
            } else {
                report.units_lost = value;
            }
        }
        add_status_bar(h, state, template, graph, colour, player, 3, b"");
    }
}

/// `ShowEpochs`: the epoch tab's bars and labels spread over the epochs reached, each bar labelled with when the
/// player reached that epoch ("N/A" when it did not).
fn show_epochs<H: PostGameHost + ?Sized>(h: &mut H, state: &mut PostGame, colour: [f32; 3], player: u32) {
    let window_width = op(h, EE_SCREENS_GAME_PGS_WINDOW, 1);
    let designed_width = op(h, EE_SCREENS_OP_PGS_DESIGNED, 1);
    let scale = window_width as f32 / designed_width as f32;
    let left = scale * 250.0f32;
    let right = scale * 945.0f32;
    let count = state.last_time.wrapping_sub(state.first_time).wrapping_add(1);
    let step = (right - left) / count as f32;
    let first = state.first_time;
    let series = player.wrapping_sub(1);
    let mut i: u32 = 0;
    while i < count {
        let epoch = first.wrapping_add(i);
        let bar = epoch.wrapping_mul(16).wrapping_add(0x226);
        let label = epoch.wrapping_add(0x208);
        let mut text = string(h, PGS_IDS_N);
        let x0 = i as f32 * step + left;
        let x1 = i.wrapping_add(1) as f32 * step + left;
        if op(h, EE_SCREENS_OP_PGS_EXISTS, bar) != 0 {
            if op(h, EE_SCREENS_OP_PGS_SERIES_COUNT, 0x0c) > series {
                let points = op2(h, EE_SCREENS_OP_PGS_POINT_COUNT, 0x0c, series);
                for j in 0..points {
                    if h.call(EE_SCREENS_OP_PGS_POINT, 0x0c, series, j, 1) != epoch {
                        continue;
                    }
                    let time = h.call(EE_SCREENS_OP_PGS_POINT, 0x0c, series, j, 0);
                    let hours = if time >= 3600000 { time / 3600000 } else { 0 };
                    let minutes =
                        if time >= 60000 { time.wrapping_sub(hours.wrapping_mul(3600000)) / 60000 } else { 0 };
                    let seconds =
                        time.wrapping_sub(hours.wrapping_mul(3600000)).wrapping_sub(minutes.wrapping_mul(60000)) / 1000;
                    text = format!("{}:{}:{}", hours as i32, two_digits(minutes as i32), two_digits(seconds as i32))
                        .into_bytes();
                }
            }
            let r = rect(h, bar);
            set_rect(h, bar, [x0, r[1], x1, r[3]]);
            add_status_bar(h, state, bar, 0x0c, colour, player, 5, &text);
        }
        if op(h, EE_SCREENS_OP_PGS_EXISTS, label) != 0 {
            let r = rect(h, label);
            set_rect(h, label, [x0, r[1], x1, r[3]]);
            show(h, label, PGS_SHOWN);
        }
        i = i.wrapping_add(1);
    }
}

/// `ShowName`: the player's name in its colour with its check box, its team number when it has one.
fn show_name<H: PostGameHost + ?Sized>(h: &mut H, record: &EeScreensPgsRecord, name: &[u8], player: u32) {
    h.put_text(EE_SCREENS_TEXT_LABEL, player, name);
    colour_op(h, EE_SCREENS_OP_PGS_BG_COLOUR, player, record.colour);
    if record.team > 0 {
        let value = player + 0x5eb;
        op2(h, EE_SCREENS_OP_LABEL_NUMBER, value, record.team);
        show(h, value, PGS_SHOWN);
        op(h, EE_SCREENS_OP_PGS_DIRTY, value);
    }
    show(h, player, PGS_SHOWN);
    show(h, player + 16, PGS_SHOWN);
    op(h, EE_SCREENS_OP_PGS_DIRTY, player);
    op(h, EE_SCREENS_OP_PGS_DIRTY, player + 16);
}

/// The game report's message (type 1000: start, end, players, humans; 20 bytes).
fn game_message(report: &GameReport) -> Vec<u8> {
    let mut out = Vec::with_capacity(20);
    out.extend_from_slice(&1000i32.to_le_bytes());
    out.extend_from_slice(&report.start_time.to_le_bytes());
    out.extend_from_slice(&report.end_time.to_le_bytes());
    out.extend_from_slice(&report.players.to_le_bytes());
    out.extend_from_slice(&report.humans.to_le_bytes());
    out
}

/// A player report's message (type 1001: start, lost, killed, result, the human flag, the name and its terminator).
fn player_message(report: &PlayerReport) -> Vec<u8> {
    let mut out = Vec::with_capacity(22 + report.name.len());
    out.extend_from_slice(&1001i32.to_le_bytes());
    out.extend_from_slice(&report.start_time.to_le_bytes());
    out.extend_from_slice(&report.units_lost.to_le_bytes());
    out.extend_from_slice(&report.units_killed.to_le_bytes());
    out.extend_from_slice(&report.result.to_le_bytes());
    out.push(report.human as u8);
    out.extend_from_slice(&report.name);
    out.push(0);
    out
}

/// `EEUIEPGSEOpen`: the graphs built, each player's name and bars on every tab, the reports sent to the stats server
/// (a victory game with several humans, from the server's slot, every player with a result), the victory or defeat
/// backdrop, the end-game screenshot, the last page's tab checked.
fn open<H: PostGameHost + ?Sized>(h: &mut H, state: &Mutex<PostGame>) {
    let mut s = std::mem::take(&mut *state.lock().unwrap());
    s.graph_index = 0;
    s.controls.clear();
    op(h, EE_SCREENS_GAME_PGS_NEW_MANAGER, 0);
    hide_tabs(h);
    hide_bars(h);
    build_graphs(h, &mut s);
    let history = h.history();
    let players = history.records.wrapping_sub(1) as usize;
    s.reports.resize(players, PlayerReport::default());
    let mut humans: u32 = 0;
    let mut others: u32 = 0;
    for i in 1..history.records {
        let record = h.record(i);
        let name = h.text(EE_SCREENS_TEXT_PGS_NAME, i).unwrap_or_default();
        let counted = if record.human {
            humans = humans.wrapping_add(1);
            true
        } else {
            others = others.wrapping_add(1);
            false
        };
        let report = &mut s.reports[i as usize - 1];
        report.name = name.clone();
        report.start_time = history.start_time;
        report.result = record.result;
        report.human = record.human;
        if !counted {
            report.name.extend_from_slice(two_digits(others as i32).as_bytes());
        }
        show_name(h, &record, &name, i);
        show_military(h, &mut s, record.colour, i);
        show_bars(
            h,
            &mut s,
            0x99,
            &[7, 8, 9, 0x0a, 0x0b, PGS_GRAPH_TOTAL_COLLECTED, PGS_GRAPH_TOTAL_SPENT],
            record.colour,
            i,
            4,
            &|_, _| Vec::new(),
        );
        let increases = record.increases;
        show_bars(h, &mut s, 0x10f, &[0x10, 0x0f, 0x0e, 0x11, 0x12, 0x13], record.colour, i, 6, &|_, k| {
            if k == 1 {
                (increases as i32).to_string().into_bytes()
            } else {
                Vec::new()
            }
        });
        let percent = record.territory as f32 / history.tile_count as f32 * 100.0f32;
        show_bars(h, &mut s, 0x173, &[0x18, 0x14, 0x15, PGS_GRAPH_SPEED_RATING], record.colour, i, 7, &|_, k| {
            if k == 0 {
                format!("{}%", truncate(percent + 0.5f32)).into_bytes()
            } else {
                Vec::new()
            }
        });
        show_epochs(h, &mut s, record.colour, i);
        show_bars(
            h,
            &mut s,
            0x1b8,
            &[PGS_GRAPH_MILITARY_SCORE, PGS_GRAPH_ECONOMY_SCORE, PGS_GRAPH_EPOCH_SCORE, PGS_GRAPH_SOCIETY_SCORE],
            record.colour,
            i,
            1,
            &|_, _| Vec::new(),
        );
    }
    s.game = GameReport { start_time: history.start_time, end_time: history.end_time, players: players as u32, humans };
    if history.victory_enabled && humans > 1 && history.current_slot == 0xfe {
        if let Some(server) = h.text(EE_SCREENS_TEXT_PGS_SERVER, 0) {
            let mut parts = server.splitn(2, |&c| c == 0);
            let host = parts.next().unwrap_or_default().to_vec();
            let port = atoi(parts.next().unwrap_or_default());
            let finished = (1..history.records).all(|i| h.record(i).result != 0);
            if finished {
                op(h, EE_SCREENS_GAME_PGS_PROTOCOL, 0);
                let mut payload = host.clone();
                payload.push(0);
                payload.extend_from_slice(&game_message(&s.game));
                h.put_text(EE_SCREENS_TEXT_PGS_SEND, port as u32, &payload);
                for report in &s.reports[..players] {
                    let mut payload = host.clone();
                    payload.push(0);
                    payload.extend_from_slice(&player_message(report));
                    h.put_text(EE_SCREENS_TEXT_PGS_SEND, port as u32, &payload);
                }
            }
        }
    }
    // (back before the tab's radio button fires its tab event)
    *state.lock().unwrap() = s;
    let result = h.record(history.current_record as u32).result;
    h.call(EE_SCREENS_OP_BACKGROUND, 0x87, (result == 1) as u32, 0, 0);
    h.call(EE_SCREENS_OP_BACKGROUND, 0x8e, (result != 1) as u32, 0, 0);
    op(h, EE_SCREENS_OP_REMOVE_BACKGROUND, (-2i32) as u32);
    if op(h, EE_SCREENS_GAME_PGS_SCREENSHOTS, 0) != 0 && (result == 1 || result == 2) {
        op(h, EE_SCREENS_GAME_PGS_SCREENSHOT, 0);
    }
    let page = op(h, EE_SCREENS_GAME_PGS_PAGE, 0);
    let mut tab = page.wrapping_add(0x21);
    tab = tab.clamp(0x22, 0x28);
    if tab == 0x25 && h.history().is_scenario {
        tab = 0x22;
    }
    op(h, EE_SCREENS_OP_CHECK_RADIO, tab);
    op(h, EE_SCREENS_GAME_PGS_SCI_DELETE, 0);
}

/// `EEUIEPGSEClose`: the statistics graphs emptied, the bars and stars the summary added removed and deleted, the
/// preferences saved, WON's room rejoined.
fn close<H: PostGameHost + ?Sized>(h: &mut H, state: &Mutex<PostGame>) {
    for i in 1..31 {
        op(h, EE_SCREENS_OP_PGS_CLEAR_GRAPH, i);
    }
    let controls = std::mem::take(&mut state.lock().unwrap().controls);
    for (index, group) in controls {
        op2(h, EE_SCREENS_OP_PGS_REMOVE, index, group);
    }
    op(h, EE_SCREENS_GAME_PGS_SAVE, 0);
    op(h, EE_SCREENS_GAME_PGS_REJOIN, 0);
}

/// `EEUIEPGSEExit`: after a campaign scenario its campaign reloaded and its scenario select screen opened; otherwise
/// (or when it does not load) the form closed.
fn exit<H: PostGameHost + ?Sized>(h: &mut H) {
    let campaign = h.text(EE_SCREENS_TEXT_PGS_CAMPAIGN, 0).unwrap_or_default();
    if h.history().is_campaign {
        if h.put_text(EE_SCREENS_TEXT_PGS_CAMPAIGN_LOAD, 0, &campaign) != 0 {
            h.put_text(EE_SCREENS_TEXT_PGS_CAMPAIGN_NAME, 0, &campaign);
            op(h, EE_SCREENS_GAME_PGS_CAMPAIGN_SETUP, 0);
            op2(
                h,
                crate::menus::EE_SCREENS_OP_OPEN_EVENT,
                PGS_FORM_CAMPAIGN_SELECT,
                crate::scenario_screens::EE_SCREENS_CS_OPEN,
            );
            op2(
                h,
                crate::menus::EE_SCREENS_OP_CLOSE_EVENT,
                PGS_FORM_CAMPAIGN_SELECT,
                crate::scenario_screens::EE_SCREENS_CS_CLOSE,
            );
            op(h, crate::menus::EE_SCREENS_OP_OPEN_FORM, PGS_FORM_CAMPAIGN_SELECT);
            return;
        }
        op(h, EE_SCREENS_GAME_PGS_CAMPAIGN_CLEAR, 0);
    }
    op(h, EE_SCREENS_OP_CLOSE_FORM, 0);
}

/// One of the summary's events; `None` for another form's, else its `ProcessEvent` result (always 0).
pub fn dispatch<H: PostGameHost + ?Sized>(h: &mut H, state: &Mutex<PostGame>, event: u32) -> Option<c_long> {
    if !is_post_game_event(event) {
        return None;
    }
    match event {
        EE_SCREENS_PGS_EXIT => exit(h),
        EE_SCREENS_PGS_OPEN => open(h, state),
        EE_SCREENS_PGS_CLOSE => close(h, state),
        EE_SCREENS_PGS_NEXT_GRAPH => {
            let index = state.lock().unwrap().graph_index;
            match index {
                0 => op(h, EE_SCREENS_OP_PGS_SHOW_GRAPH, 0x0d),
                1 => op(h, EE_SCREENS_OP_PGS_SHOW_GRAPH, 0x0e),
                2 => op(h, EE_SCREENS_OP_PGS_SHOW_GRAPH, 0x17),
                _ => 0,
            };
            state.lock().unwrap().graph_index = (index + 1) % 3;
        }
        e if e < EE_SCREENS_PGS_CHECKED => tab(h, e - EE_SCREENS_PGS_TAB + 1),
        e if e < EE_SCREENS_PGS_UNCHECKED => series_enabled(h, e - EE_SCREENS_PGS_CHECKED, true),
        e => series_enabled(h, e - EE_SCREENS_PGS_UNCHECKED, false),
    }
    Some(0)
}

/// `EEUIPGSLineGraph::FormatYValues`: a y axis value rounded (`(long)(value + 0.5f)`, x64's `cvttss2si`).
pub fn format_y(value: f32) -> Vec<u8> {
    truncate(value + 0.5f32).to_string().into_bytes()
}

/// `EEUIPGSLineGraph::FormatXValues`: a game time in milliseconds as "minutes:seconds", at least 1 ms; the time's
/// `(unsigned long)` conversion is x64's 64-bit `cvttss2si` kept to its low 32 bits (out of range or NaN: 0, which
/// `EEPhysics::GetMinutesAndSeconds` replaces with the game's time).
pub fn format_x(value: f32, game_time: u32) -> Vec<u8> {
    let value = if value < 1.0f32 { 1.0f32 } else { value };
    let mut time = if value.is_nan() || value >= 9.223372e18 { 0 } else { value as i64 as u32 };
    if time == 0 {
        time = game_time;
    }
    let minutes = time / 1000 / 60;
    let seconds = time.wrapping_sub(minutes.wrapping_mul(60000)) / 1000;
    format!("{}:{}", minutes as i32, two_digits(seconds as i32)).into_bytes()
}

#[cfg(test)]
mod golden;

#[cfg(test)]
mod tests {
    use super::*;

    /// The line graph's axis texts against the real C++ (`build/sc11/lg_driver.cpp` over the unmodified
    /// `EEUIPGSLineGraph.cpp`: 3,000 drawn floats, the conversions' edges, NaN and infinities, the physics' game time;
    /// `golden_pgs_axes.txt`). The driver relinked over the new unit prints the same, and on seeds 2-4 at 30,000.
    #[test]
    fn golden_axes() {
        let text = String::from_utf8(include_bytes!("golden_pgs_axes.txt").to_vec()).unwrap().replace("\r\n", "\n");
        let mut n = 0;
        for line in text.lines() {
            let q: Vec<&str> = line.split('\'').collect();
            let head: Vec<&str> = q[0].split_whitespace().collect();
            let value = f32::from_bits(u32::from_str_radix(head[0], 16).unwrap());
            let time: u32 = head[1].parse().unwrap();
            assert_eq!(String::from_utf8(format_y(value)).unwrap(), q[1], "{line}");
            assert_eq!(String::from_utf8(format_x(value, time)).unwrap(), q[3], "{line}");
            n += 1;
        }
        assert_eq!(n, 3000);
    }

    #[test]
    fn events_and_messages() {
        assert_eq!(event_for_control(144), EE_SCREENS_PGS_TAB);
        assert_eq!(event_for_control(150), EE_SCREENS_PGS_TAB + 6);
        assert_eq!(event_for_control(151), EE_SCREENS_PGS_EXIT);
        assert_eq!(event_for_control(152), EE_SCREENS_PGS_CHECKED);
        assert_eq!(event_for_control(167), EE_SCREENS_PGS_CHECKED + 15);
        assert_eq!(event_for_control(168), EE_SCREENS_PGS_UNCHECKED);
        assert_eq!(event_for_control(183), EE_SCREENS_PGS_UNCHECKED + 15);
        assert_eq!(event_for_control(184), EE_SCREENS_PGS_NEXT_GRAPH);
        assert_eq!(event_for_control(143), 0);
        assert_eq!(two_digits(3), "03");
        assert_eq!(two_digits(-3), "-03");
        assert_eq!(two_digits(123), "123");
        let report = PlayerReport {
            name: b"Ann".to_vec(),
            start_time: 1,
            units_lost: 2,
            units_killed: 3,
            result: 1,
            human: true,
        };
        let m = player_message(&report);
        assert_eq!(m.len(), 22 + 3);
        assert_eq!(&m[..8], &[0xe9, 3, 0, 0, 1, 0, 0, 0]);
        assert_eq!(game_message(&GameReport::default()).len(), 20);
        assert_eq!(atoi(b" 6667x"), 6667);
        assert_eq!(truncate(f32::NAN), i32::MIN);
    }
}
