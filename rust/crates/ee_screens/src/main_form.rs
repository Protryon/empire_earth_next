//! The main game form's per-frame work (`EEUIFMainGame`, `User Interface Thread/EEUIFMainGame.cpp`): `CopyData`
//! (the selection flag, the error log, the status refresh each poll, the panel group, edge scrolling, the gate,
//! wall and placement previews), `RefreshStatus` (the resources, population, wonder countdowns, clock or frame
//! rate, pause and result texts, the lag icons, the briefing and previous-messages buttons, the cursor),
//! `DisplayChatLine` (the seven chat lines, a long message wrapped onto further lines) and `ShowInformation` (a
//! scenario's dialog message: text, portrait, sound, scrolling).
//!
//! Every call on the form, its controls and the world is one host op ([`FormHost`], `ee_screenscpp_main_form`),
//! asked where and as often as the original asked it. Texts cross as UTF-8 bytes through the host's one held text
//! (a `UWideString`): [`FormHost::set_text`] fills it, [`FormHost::text`] reads it, the label ops show it.

use ee_ge::types::Ge2DPoint;
use ee_ge::um::lrintf;

// ---- the host's ops (EE_SCREENS_MF_OP_*): a and b are the op's arguments, io its in/out values

/// `mErrorCriticalSection->Enter()` (a 1) or `Leave()` (a 0).
pub const EE_SCREENS_MF_OP_ERROR_LOCK: u32 = 1;
/// `gWorld.GetCurrentPlayer() != NULL`.
pub const EE_SCREENS_MF_OP_HAS_PLAYER: u32 = 2;
/// The current player's selected group exists and has a first unit.
pub const EE_SCREENS_MF_OP_GROUP_HAS_FIRST: u32 = 3;
/// The current player's `mSelectedViewedUnit != NULL`.
pub const EE_SCREENS_MF_OP_HAS_VIEWED: u32 = 4;
/// `gUnitSelected = a != 0`.
pub const EE_SCREENS_MF_OP_SET_UNIT_SELECTED: u32 = 5;
/// The form's field a (`EE_SCREENS_MF_FIELD_*`).
pub const EE_SCREENS_MF_OP_FIELD: u32 = 6;
/// The form's field a = b.
pub const EE_SCREENS_MF_OP_SET_FIELD: u32 = 7;
/// The error log's (control 0x1b) `SetLine(a, mStrings[a])`.
pub const EE_SCREENS_MF_OP_LOG_LINE: u32 = 8;
/// `gViewport.GetViewRectangle()` into io.f (left, top, right, bottom).
pub const EE_SCREENS_MF_OP_VIEW_RECT: u32 = 9;
/// `GetManager()->GetCurrentForm() == this`.
pub const EE_SCREENS_MF_OP_IS_CURRENT: u32 = 10;
/// `GetManager()->AddDirtyRectangle(io.f, this)`.
pub const EE_SCREENS_MF_OP_ADD_DIRTY: u32 = 11;
/// `mPoll->Poll()`: the elapsed ticks.
pub const EE_SCREENS_MF_OP_POLL: u32 = 12;
/// The current panel group's (`mGroups[mCurrentGroup]`) call a (`EE_SCREENS_MF_GROUP_*`); `WantsRefresh`'s answer.
pub const EE_SCREENS_MF_OP_GROUP: u32 = 13;
/// `GetMouseCaptured() != NULL`.
pub const EE_SCREENS_MF_OP_MOUSE_CAPTURED: u32 = 14;
/// `mScroll->Update(gMouse->mPosition)`.
pub const EE_SCREENS_MF_OP_SCROLL: u32 = 15;
/// `gUnitPicker.PickUnit(gMouse->mPosition, 0)`, held by the host: 0 none, 1 a unit, 2 a wall.
pub const EE_SCREENS_MF_OP_PICK: u32 = 16;
/// The picked unit's tile into io.n.
pub const EE_SCREENS_MF_OP_PICKED_TILE: u32 = 17;
/// `EEPointUnitOrTileXY(gMouse->mPosition, ..., true)`'s tile into io.n.
pub const EE_SCREENS_MF_OP_MOUSE_TILE: u32 = 18;
/// The picked unit's `Dereference()`.
pub const EE_SCREENS_MF_OP_RELEASE: u32 = 19;
/// `EEFindGateWall((a, b), io.n[0])`: found; the direction back in io.n[0].
pub const EE_SCREENS_MF_OP_FIND_GATE_WALL: u32 = 20;
/// `EEGhostUnit::CreateGate` of the gate mode's class and player, then `PreviewGate(mouse, io.n[0])`.
pub const EE_SCREENS_MF_OP_PREVIEW_GATE: u32 = 21;
/// `gWorld.mGhostUnitsLock->Enter()` (a 1) or `Leave()` (a 0).
pub const EE_SCREENS_MF_OP_GHOST_LOCK: u32 = 22;
/// `gWorld.ClearGhostUnits()`.
pub const EE_SCREENS_MF_OP_CLEAR_GHOSTS: u32 = 23;
/// The wall mode's `mStart` into io.f[0..2].
pub const EE_SCREENS_MF_OP_WALL_START: u32 = 24;
/// `EEPreviewWall` of the a tiles at b (x, y pairs of i32) for the wall mode's technology and player.
pub const EE_SCREENS_MF_OP_PREVIEW_WALL: u32 = 25;
/// `EEPreviewPlacement(gMouse->mPosition)`.
pub const EE_SCREENS_MF_OP_PREVIEW_PLACEMENT: u32 = 26;
/// `UIForm::CopyData()`.
pub const EE_SCREENS_MF_OP_BASE_COPY_DATA: u32 = 27;
/// The current player's `mResourceCaps[a + 2]`.
pub const EE_SCREENS_MF_OP_RESOURCE: u32 = 28;
/// `atoi` of the label a's text.
pub const EE_SCREENS_MF_OP_LABEL_INT: u32 = 29;
/// The label a's `SetText((long)b)`.
pub const EE_SCREENS_MF_OP_LABEL_SET_LONG: u32 = 30;
/// The current player's `GetTotalPopulation(false, false)`.
pub const EE_SCREENS_MF_OP_POPULATION: u32 = 31;
/// The current player's `GetPopulationLimit(false)`.
pub const EE_SCREENS_MF_OP_POPULATION_LIMIT: u32 = 32;
/// The held text = the a bytes at b (UTF-8).
pub const EE_SCREENS_MF_OP_TEXT_SET: u32 = 33;
/// The held text's length in bytes; with b not 0, its bytes copied to b (a bytes at most).
pub const EE_SCREENS_MF_OP_TEXT_GET: u32 = 34;
/// The label a's text differs from the held text.
pub const EE_SCREENS_MF_OP_LABEL_DIFFERS: u32 = 35;
/// The label a's `SetText(held text)`.
pub const EE_SCREENS_MF_OP_LABEL_SET_TEXT: u32 = 36;
/// `gWorld.mVictoryEnabled`.
pub const EE_SCREENS_MF_OP_VICTORY_ENABLED: u32 = 37;
/// `gWorld.GetNumTeams()`.
pub const EE_SCREENS_MF_OP_NUM_TEAMS: u32 = 38;
/// The team a's `mVictoryCondition->GetWonderCountdown(io.n[0])`: the winner (a pointer, 0 for none).
pub const EE_SCREENS_MF_OP_COUNTDOWN: u32 = 39;
/// The control a's `SetDisplayMode(b)`.
pub const EE_SCREENS_MF_OP_SET_MODE: u32 = 40;
/// The control a's `GetDisplayMode()`.
pub const EE_SCREENS_MF_OP_MODE: u32 = 41;
/// The label a's text colour = the player b's (a pointer) `mColor`.
pub const EE_SCREENS_MF_OP_LABEL_PLAYER_COLOR: u32 = 42;
/// The held text = `IDS_TIME_LEFT_UNTIL_WINS_WONDER_VICTORY` printed with the player a's (a pointer) name and b.
pub const EE_SCREENS_MF_OP_TEXT_WONDER: u32 = 43;
/// `gWorld.mInCinematic`.
pub const EE_SCREENS_MF_OP_IN_CINEMATIC: u32 = 44;
/// The held text = `gPhysics->GetTimeString()`.
pub const EE_SCREENS_MF_OP_TEXT_TIME: u32 = 45;
/// The held text = the string a (`gResource->GetString`).
pub const EE_SCREENS_MF_OP_TEXT_STRING: u32 = 46;
/// `gRasterizer->GetFramesPerSecond(GetManager()->GetScreen())` into io.f[0].
pub const EE_SCREENS_MF_OP_FPS: u32 = 47;
/// `gCommunications != NULL`.
pub const EE_SCREENS_MF_OP_HAS_COMMS: u32 = 48;
/// `gCommunications->GetIsGamePaused()`.
pub const EE_SCREENS_MF_OP_COMMS_PAUSED: u32 = 49;
/// `gWorld.mTeams.size() > 0`.
pub const EE_SCREENS_MF_OP_HAS_TEAMS: u32 = 50;
/// `gSoundManager->SetIsMuted(a != 0)`.
pub const EE_SCREENS_MF_OP_SET_MUTED: u32 = 51;
/// `gSoundManager->GetIsMuted()`.
pub const EE_SCREENS_MF_OP_MUTED: u32 = 52;
/// The current player's `mGameResult`.
pub const EE_SCREENS_MF_OP_GAME_RESULT: u32 = 53;
/// `SetPauseText(a != 0)`.
pub const EE_SCREENS_MF_OP_PAUSE_TEXT: u32 = 54;
/// The control a's `SetDirty(true)`.
pub const EE_SCREENS_MF_OP_SET_DIRTY: u32 = 55;
/// `gWorld.mLagStatus`.
pub const EE_SCREENS_MF_OP_LAG_STATUS: u32 = 56;
/// The bitmap control a's bitmap colour = `gWorld.mPlayers[b]->mColor`.
pub const EE_SCREENS_MF_OP_LAG_COLOR: u32 = 57;
/// The scenario's instructions are not empty.
pub const EE_SCREENS_MF_OP_HAS_INSTRUCTIONS: u32 = 58;
/// `EEUIECLEFormManager::GetEntryCount()`.
pub const EE_SCREENS_MF_OP_LOG_ENTRIES: u32 = 59;
/// `UpdateCursorKind()`.
pub const EE_SCREENS_MF_OP_UPDATE_CURSOR: u32 = 60;
/// The chat line a's `Display` of the call's colour, name, text and flags: the characters shown.
pub const EE_SCREENS_MF_OP_CHAT_DISPLAY: u32 = 61;
/// The chat line a's `Copy` of the line a + 1.
pub const EE_SCREENS_MF_OP_CHAT_COPY: u32 = 62;
/// The call's text's `GetLength()` (characters).
pub const EE_SCREENS_MF_OP_CHAT_LENGTH: u32 = 63;
/// The call's text = its rest after a characters (and the line feed after them); its name = "".
pub const EE_SCREENS_MF_OP_CHAT_REST: u32 = 64;
/// The information's scrolling text control (0xd3) exists.
pub const EE_SCREENS_MF_OP_INFO_HAS_TEXT: u32 = 65;
/// The scrolling text's `SetText` of the message's text, then shown.
pub const EE_SCREENS_MF_OP_INFO_SET_TEXT: u32 = 66;
/// The information's portrait control (0xdf) exists.
pub const EE_SCREENS_MF_OP_INFO_HAS_PORTRAIT: u32 = 67;
/// The message's unit type has a class and the current player's upgrade of it a button object.
pub const EE_SCREENS_MF_OP_INFO_HAS_BUTTON: u32 = 68;
/// The portrait = the button object's face (a new bitmap), shown, its opacity set again.
pub const EE_SCREENS_MF_OP_INFO_PORTRAIT: u32 = 69;
/// `EELoadSound("Sounds\\" + the message's sound, 0x31, io.n[0], io.n[1], 1)`.
pub const EE_SCREENS_MF_OP_INFO_SOUND: u32 = 70;
/// `gSoundManager->GetTotalPlayTimeInMilli(a)`.
pub const EE_SCREENS_MF_OP_PLAY_TIME: u32 = 71;
/// The message's `mDuration`.
pub const EE_SCREENS_MF_OP_INFO_DURATION: u32 = 72;
/// The scrolling text's `ScrollText((unsigned long)a, (unsigned long)b)`.
pub const EE_SCREENS_MF_OP_INFO_SCROLL: u32 = 73;
/// The scrolling text's rectangle's bottom and top into io.f[0..2].
pub const EE_SCREENS_MF_OP_INFO_RECT: u32 = 74;
/// `SetChatTop(a)`.
pub const EE_SCREENS_MF_OP_CHAT_TOP: u32 = 75;
/// `HideMessages()`.
pub const EE_SCREENS_MF_OP_HIDE_MESSAGES: u32 = 76;

// ---- the form's fields (EE_SCREENS_MF_OP_FIELD / _SET_FIELD)

/// `mStringsChanged`.
pub const EE_SCREENS_MF_FIELD_STRINGS_CHANGED: i64 = 1;
/// `mElapsed`.
pub const EE_SCREENS_MF_FIELD_ELAPSED: i64 = 2;
/// `mRefreshGroup`.
pub const EE_SCREENS_MF_FIELD_REFRESH_GROUP: i64 = 3;
/// `mUIState`.
pub const EE_SCREENS_MF_FIELD_UI_STATE: i64 = 4;
/// `mBuildGate.mDirection`.
pub const EE_SCREENS_MF_FIELD_GATE_DIRECTION: i64 = 5;
/// `mBuildWall.mState`.
pub const EE_SCREENS_MF_FIELD_WALL_STATE: i64 = 6;
/// `mClockMode`.
pub const EE_SCREENS_MF_FIELD_CLOCK_MODE: i64 = 7;
/// `mIsCursorLocked`.
pub const EE_SCREENS_MF_FIELD_CURSOR_LOCKED: i64 = 8;

// ---- the panel group's calls (EE_SCREENS_MF_OP_GROUP)

/// `Reset()`.
pub const EE_SCREENS_MF_GROUP_RESET: i64 = 0;
/// `Refresh()`.
pub const EE_SCREENS_MF_GROUP_REFRESH: i64 = 1;
/// `WantsRefresh() == true`.
pub const EE_SCREENS_MF_GROUP_WANTS_REFRESH: i64 = 2;

// ---- the form's controls (`EEUIManager.h`'s kMainGame*), the states and the strings

/// The first resource label (`GetControl(i + 6)`).
pub const EE_SCREENS_MF_CTL_RESOURCE: i64 = 6;
pub const EE_SCREENS_MF_CTL_WIN_LOSE: i64 = 13;
/// The first wonder countdown label (to 0x12).
pub const EE_SCREENS_MF_CTL_WONDER: i64 = 0x0e;
pub const EE_SCREENS_MF_CTL_POPULATION: i64 = 24;
pub const EE_SCREENS_MF_CTL_MINI_MAP: i64 = 26;
pub const EE_SCREENS_MF_CTL_CHAT_LINE1: i64 = 101;
pub const EE_SCREENS_MF_CTL_CHAT_LINE7: i64 = 107;
pub const EE_SCREENS_MF_CTL_GAME_CLOCK: i64 = 108;
pub const EE_SCREENS_MF_CTL_BRIEFING: i64 = 109;
pub const EE_SCREENS_MF_CTL_DIALOG_PORTRAIT: i64 = 223;
pub const EE_SCREENS_MF_CTL_BRIEFING_ALERT: i64 = 235;
/// The last lag icon (player 8); player 1's is 0xfc.
pub const EE_SCREENS_MF_CTL_LAG_LAST: i64 = 0xf5;
pub const EE_SCREENS_MF_CTL_PREVIOUS_MESSAGES: i64 = 253;
/// The error log's lines (`mStrings`).
pub const EE_SCREENS_MF_ERROR_LINES: i64 = 0x24;
/// `mUIState`: the wall mode, the gate mode, no scrolling.
pub const EE_SCREENS_MF_STATE_WALL: i64 = 6;
pub const EE_SCREENS_MF_STATE_GATE: i64 = 0xb;
pub const EE_SCREENS_MF_STATE_NO_SCROLL: i64 = 0x11;
pub const EE_SCREENS_MF_IDS_FPS: i64 = 30324;
pub const EE_SCREENS_MF_IDS_GAME_PAUSED: i64 = 30614;

/// `kDisplayModeHidden` / `kDisplayModeShown`.
pub const EE_SCREENS_MF_HIDDEN: i64 = 0;
pub const EE_SCREENS_MF_SHOWN: i64 = 1;

/// The host's in/out values of one op.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScreensMainFormIo {
    pub f: [f32; 4],
    pub n: [i64; 2],
}

pub trait FormHost {
    fn op(&mut self, op: u32, a: i64, b: i64, io: &mut EeScreensMainFormIo) -> i64;
    /// The held text = `text`.
    fn set_text(&mut self, text: &[u8]);
    /// The held text's bytes.
    fn text(&mut self) -> Vec<u8>;
    /// `EEPreviewWall` of `tiles`.
    fn preview_wall(&mut self, tiles: &[[i32; 2]]);

    fn a(&mut self, op: u32, a: i64) -> i64 {
        self.op(op, a, 0, &mut EeScreensMainFormIo::default())
    }
    fn ab(&mut self, op: u32, a: i64, b: i64) -> i64 {
        self.op(op, a, b, &mut EeScreensMainFormIo::default())
    }
    fn t(&mut self, op: u32) -> bool {
        self.a(op, 0) != 0
    }
    fn field(&mut self, field: i64) -> i64 {
        self.a(EE_SCREENS_MF_OP_FIELD, field)
    }
    fn set_field(&mut self, field: i64, value: i64) {
        self.ab(EE_SCREENS_MF_OP_SET_FIELD, field, value);
    }
    fn io(&mut self, op: u32, a: i64, b: i64, io: EeScreensMainFormIo) -> EeScreensMainFormIo {
        let mut io = io;
        self.op(op, a, b, &mut io);
        io
    }
}

/// `UMCeil`: rounded to nearest (`lrintf`), then up when below the value.
fn um_ceil(value: f32) -> i32 {
    let r = lrintf(value);
    if (r as f32) < value {
        r.wrapping_add(1)
    } else {
        r
    }
}

/// `EEUIFMainGame::CopyData` (0x264dc4): `UIForm::CopyData`'s result.
pub fn copy_data(h: &mut impl FormHost) -> i64 {
    let view = h.io(EE_SCREENS_MF_OP_VIEW_RECT, 0, 0, EeScreensMainFormIo::default());
    h.a(EE_SCREENS_MF_OP_ERROR_LOCK, 1);
    let selected =
        h.t(EE_SCREENS_MF_OP_HAS_PLAYER) && (h.t(EE_SCREENS_MF_OP_GROUP_HAS_FIRST) || h.t(EE_SCREENS_MF_OP_HAS_VIEWED));
    h.a(EE_SCREENS_MF_OP_SET_UNIT_SELECTED, selected as i64);
    if h.field(EE_SCREENS_MF_FIELD_STRINGS_CHANGED) != 0 {
        for i in 0..EE_SCREENS_MF_ERROR_LINES {
            h.a(EE_SCREENS_MF_OP_LOG_LINE, i);
        }
        h.set_field(EE_SCREENS_MF_FIELD_STRINGS_CHANGED, 0);
    }
    h.a(EE_SCREENS_MF_OP_ERROR_LOCK, 0);
    if !h.t(EE_SCREENS_MF_OP_IS_CURRENT) {
        let mut dirty = view;
        dirty.f[2] -= 1.0;
        dirty.f[3] -= 1.0;
        h.io(EE_SCREENS_MF_OP_ADD_DIRTY, 0, 0, dirty);
    }
    let elapsed = h.a(EE_SCREENS_MF_OP_POLL, 0) as u32;
    if elapsed > 0 {
        let total = (h.field(EE_SCREENS_MF_FIELD_ELAPSED) as u32).wrapping_add(elapsed);
        h.set_field(EE_SCREENS_MF_FIELD_ELAPSED, total as i64);
        refresh_status(h);
    }
    if h.field(EE_SCREENS_MF_FIELD_REFRESH_GROUP) != 0 {
        h.a(EE_SCREENS_MF_OP_GROUP, EE_SCREENS_MF_GROUP_RESET);
        h.a(EE_SCREENS_MF_OP_GROUP, EE_SCREENS_MF_GROUP_REFRESH);
        h.set_field(EE_SCREENS_MF_FIELD_REFRESH_GROUP, 0);
    } else if h.a(EE_SCREENS_MF_OP_GROUP, EE_SCREENS_MF_GROUP_WANTS_REFRESH) != 0 || elapsed > 0 {
        h.a(EE_SCREENS_MF_OP_GROUP, EE_SCREENS_MF_GROUP_REFRESH);
    }
    if !h.t(EE_SCREENS_MF_OP_MOUSE_CAPTURED) && h.field(EE_SCREENS_MF_FIELD_UI_STATE) != EE_SCREENS_MF_STATE_NO_SCROLL {
        h.a(EE_SCREENS_MF_OP_SCROLL, 0);
    }
    match h.field(EE_SCREENS_MF_FIELD_UI_STATE) {
        EE_SCREENS_MF_STATE_GATE => preview_gate(h),
        EE_SCREENS_MF_STATE_WALL => preview_wall(h),
        // every other state (5, the calamity's, included)
        _ => {
            h.a(EE_SCREENS_MF_OP_PREVIEW_PLACEMENT, 0);
        }
    }
    h.a(EE_SCREENS_MF_OP_BASE_COPY_DATA, 0)
}

/// The gate mode's preview: the wall under the mouse (a picked wall's tile, else the mouse's tile) and its direction.
fn preview_gate(h: &mut impl FormHost) {
    let picked = h.a(EE_SCREENS_MF_OP_PICK, 0);
    let tile = if picked == 2 {
        h.io(EE_SCREENS_MF_OP_PICKED_TILE, 0, 0, EeScreensMainFormIo::default()).n
    } else {
        h.io(EE_SCREENS_MF_OP_MOUSE_TILE, 0, 0, EeScreensMainFormIo::default()).n
    };
    if picked != 0 {
        h.a(EE_SCREENS_MF_OP_RELEASE, 0);
    }
    let mut io = EeScreensMainFormIo::default();
    io.n[0] = h.field(EE_SCREENS_MF_FIELD_GATE_DIRECTION);
    if h.op(EE_SCREENS_MF_OP_FIND_GATE_WALL, tile[0], tile[1], &mut io) != 0 {
        h.op(EE_SCREENS_MF_OP_PREVIEW_GATE, 0, 0, &mut io);
        h.set_field(EE_SCREENS_MF_FIELD_GATE_DIRECTION, io.n[0]);
    } else {
        h.a(EE_SCREENS_MF_OP_GHOST_LOCK, 1);
        h.a(EE_SCREENS_MF_OP_CLEAR_GHOSTS, 0);
        h.a(EE_SCREENS_MF_OP_GHOST_LOCK, 0);
    }
}

/// The wall mode's preview: the mouse's tile alone (state 0), or the tiles from the start to it (state 1).
fn preview_wall(h: &mut impl FormHost) {
    let tile = h.io(EE_SCREENS_MF_OP_MOUSE_TILE, 0, 0, EeScreensMainFormIo::default()).n;
    let mut end = Ge2DPoint::new(tile[0] as i32 as f32, tile[1] as i32 as f32);
    if h.field(EE_SCREENS_MF_FIELD_WALL_STATE) == 0 {
        end = Ge2DPoint::new(end.x + 0.5, end.y + 0.5);
        let tiles = [[end.x as i32, end.y as i32]];
        h.a(EE_SCREENS_MF_OP_GHOST_LOCK, 1);
        h.preview_wall(&tiles);
        h.a(EE_SCREENS_MF_OP_GHOST_LOCK, 0);
    }
    if h.field(EE_SCREENS_MF_FIELD_WALL_STATE) == 1 {
        end = Ge2DPoint::new(end.x + 0.5, end.y + 0.5);
        let start = h.io(EE_SCREENS_MF_OP_WALL_START, 0, 0, EeScreensMainFormIo::default()).f;
        let mut tiles = Vec::new();
        Ge2DPoint::new(start[0], start[1]).interpolate_to(end, 1.0, |p| tiles.push([p.x as i32, p.y as i32]));
        h.a(EE_SCREENS_MF_OP_GHOST_LOCK, 1);
        h.preview_wall(&tiles);
        h.a(EE_SCREENS_MF_OP_GHOST_LOCK, 0);
    }
}

/// `EEUIFMainGame::RefreshStatus` (0x266a3b).
pub fn refresh_status(h: &mut impl FormHost) {
    // the five resources, set when the label's number differs
    for i in 0..5 {
        let value = h.a(EE_SCREENS_MF_OP_RESOURCE, i) as i32;
        let control = EE_SCREENS_MF_CTL_RESOURCE + i;
        if value != h.a(EE_SCREENS_MF_OP_LABEL_INT, control) as i32 {
            h.ab(EE_SCREENS_MF_OP_LABEL_SET_LONG, control, value as i64);
        }
    }
    // the population "%ld/%ld", set when the text differs
    let population = h.a(EE_SCREENS_MF_OP_POPULATION, 0) as i32;
    let limit = h.a(EE_SCREENS_MF_OP_POPULATION_LIMIT, 0) as i32;
    h.set_text(format!("{population}/{limit}").as_bytes());
    if h.a(EE_SCREENS_MF_OP_LABEL_DIFFERS, EE_SCREENS_MF_CTL_POPULATION) != 0 {
        h.a(EE_SCREENS_MF_OP_LABEL_SET_TEXT, EE_SCREENS_MF_CTL_POPULATION);
    }
    if h.t(EE_SCREENS_MF_OP_VICTORY_ENABLED) {
        wonder_countdowns(h);
    }
    // the clock or the frame rate
    let clock = EE_SCREENS_MF_CTL_GAME_CLOCK;
    if !h.t(EE_SCREENS_MF_OP_IN_CINEMATIC) {
        match h.field(EE_SCREENS_MF_FIELD_CLOCK_MODE) as i32 {
            1 => {
                h.ab(EE_SCREENS_MF_OP_SET_MODE, clock, EE_SCREENS_MF_SHOWN);
                h.a(EE_SCREENS_MF_OP_TEXT_TIME, 0);
                h.a(EE_SCREENS_MF_OP_LABEL_SET_TEXT, clock);
            }
            2 => {
                h.ab(EE_SCREENS_MF_OP_SET_MODE, clock, EE_SCREENS_MF_SHOWN);
                h.a(EE_SCREENS_MF_OP_TEXT_STRING, EE_SCREENS_MF_IDS_FPS);
                let mut text = h.text();
                let fps = h.io(EE_SCREENS_MF_OP_FPS, 0, 0, EeScreensMainFormIo::default()).f[0];
                // "%s: %ld" of the (unsigned long) ceiling
                text.extend_from_slice(format!(": {}", um_ceil(fps)).as_bytes());
                h.set_text(&text);
                h.a(EE_SCREENS_MF_OP_LABEL_SET_TEXT, clock);
            }
            _ => {
                h.ab(EE_SCREENS_MF_OP_SET_MODE, clock, EE_SCREENS_MF_HIDDEN);
            }
        }
    } else {
        h.ab(EE_SCREENS_MF_OP_SET_MODE, clock, EE_SCREENS_MF_HIDDEN);
    }
    // pause, then the game's result
    if h.t(EE_SCREENS_MF_OP_HAS_COMMS) && h.t(EE_SCREENS_MF_OP_COMMS_PAUSED) && h.t(EE_SCREENS_MF_OP_HAS_TEAMS) {
        h.a(EE_SCREENS_MF_OP_TEXT_STRING, EE_SCREENS_MF_IDS_GAME_PAUSED);
        h.a(EE_SCREENS_MF_OP_LABEL_SET_TEXT, EE_SCREENS_MF_CTL_WIN_LOSE);
        h.ab(EE_SCREENS_MF_OP_SET_MODE, EE_SCREENS_MF_CTL_WIN_LOSE, EE_SCREENS_MF_SHOWN);
        h.a(EE_SCREENS_MF_OP_SET_MUTED, 1);
    } else {
        let result = h.a(EE_SCREENS_MF_OP_GAME_RESULT, 0) as i32;
        if result != 0 {
            h.a(EE_SCREENS_MF_OP_SET_MUTED, 1);
            h.a(EE_SCREENS_MF_OP_PAUSE_TEXT, (result == 1) as i64);
        } else {
            h.ab(EE_SCREENS_MF_OP_SET_MODE, EE_SCREENS_MF_CTL_WIN_LOSE, EE_SCREENS_MF_HIDDEN);
            if h.t(EE_SCREENS_MF_OP_MUTED) {
                h.a(EE_SCREENS_MF_OP_SET_MUTED, 0);
            }
        }
    }
    if h.field(EE_SCREENS_MF_FIELD_ELAPSED) as u32 > 2 {
        h.a(EE_SCREENS_MF_OP_SET_DIRTY, EE_SCREENS_MF_CTL_MINI_MAP);
        h.set_field(EE_SCREENS_MF_FIELD_ELAPSED, 0);
    }
    // the lag icons: player 1's 0xfc down to player 8's 0xf5
    let mask = h.a(EE_SCREENS_MF_OP_LAG_STATUS, 0) as u16 as u32;
    let mut bit = 1u32;
    let mut player = 1;
    let mut icon = EE_SCREENS_MF_CTL_LAG_LAST + 7;
    while icon >= EE_SCREENS_MF_CTL_LAG_LAST {
        if mask & bit != 0 {
            h.ab(EE_SCREENS_MF_OP_LAG_COLOR, icon, player);
            h.ab(EE_SCREENS_MF_OP_SET_MODE, icon, EE_SCREENS_MF_SHOWN);
        } else {
            h.ab(EE_SCREENS_MF_OP_SET_MODE, icon, EE_SCREENS_MF_HIDDEN);
        }
        bit <<= 1;
        player += 1;
        icon -= 1;
    }
    if h.t(EE_SCREENS_MF_OP_HAS_INSTRUCTIONS) {
        h.ab(EE_SCREENS_MF_OP_SET_MODE, EE_SCREENS_MF_CTL_BRIEFING, EE_SCREENS_MF_SHOWN);
    } else {
        h.ab(EE_SCREENS_MF_OP_SET_MODE, EE_SCREENS_MF_CTL_BRIEFING, EE_SCREENS_MF_HIDDEN);
        h.ab(EE_SCREENS_MF_OP_SET_MODE, EE_SCREENS_MF_CTL_BRIEFING_ALERT, EE_SCREENS_MF_HIDDEN);
    }
    let entries = h.a(EE_SCREENS_MF_OP_LOG_ENTRIES, 0) as u32;
    h.ab(EE_SCREENS_MF_OP_SET_MODE, EE_SCREENS_MF_CTL_PREVIOUS_MESSAGES, (entries > 0) as i64);
    if h.field(EE_SCREENS_MF_FIELD_CURSOR_LOCKED) == 0 {
        h.a(EE_SCREENS_MF_OP_UPDATE_CURSOR, 0);
    }
}

/// The wonder countdowns: each team's winner and time left, shortest first (a stable sort: `std::sort`'s insertion
/// sort for so few rows), in labels 0xe..0x12; the labels left over hidden.
fn wonder_countdowns(h: &mut impl FormHost) {
    let teams = h.a(EE_SCREENS_MF_OP_NUM_TEAMS, 0) as u32;
    let mut rows: Vec<(i64, u32)> = Vec::new();
    for t in 0..teams {
        let mut io = EeScreensMainFormIo::default();
        let winner = h.op(EE_SCREENS_MF_OP_COUNTDOWN, t as i64, 0, &mut io);
        if winner != 0 {
            rows.push((winner, io.n[0] as u32));
        }
    }
    rows.sort_by_key(|r| r.1);
    let mut row = 0usize;
    let mut control = EE_SCREENS_MF_CTL_WONDER;
    while row < rows.len() {
        if control >= EE_SCREENS_MF_CTL_WONDER + 5 {
            break;
        }
        h.ab(EE_SCREENS_MF_OP_SET_MODE, control, EE_SCREENS_MF_SHOWN);
        h.ab(EE_SCREENS_MF_OP_LABEL_PLAYER_COLOR, control, rows[row].0);
        h.ab(EE_SCREENS_MF_OP_TEXT_WONDER, rows[row].0, rows[row].1 as i64);
        h.a(EE_SCREENS_MF_OP_LABEL_SET_TEXT, control);
        control += 1;
        row += 1;
    }
    if row < 5 {
        for unused in row as i64 + EE_SCREENS_MF_CTL_WONDER..EE_SCREENS_MF_CTL_WONDER + 5 {
            h.ab(EE_SCREENS_MF_OP_SET_MODE, unused, EE_SCREENS_MF_HIDDEN);
        }
    }
}

/// `EEUIFMainGame::DisplayChatLine` (0x2649a2) of the host's call (colour, name, text, flags): the next free chat
/// line (the lines scrolled up when all seven are used), the rest of a long text on further lines (the original's
/// recursion, a loop here).
pub fn display_chat_line(h: &mut impl FormHost) {
    loop {
        let mut i = EE_SCREENS_MF_CTL_CHAT_LINE7 - 1;
        let mut free = false;
        let shown = if h.a(EE_SCREENS_MF_OP_MODE, EE_SCREENS_MF_CTL_CHAT_LINE7) == EE_SCREENS_MF_HIDDEN {
            // below the first used line from the bottom, else the first line
            let mut at = None;
            while i >= EE_SCREENS_MF_CTL_CHAT_LINE1 {
                free = h.a(EE_SCREENS_MF_OP_MODE, i) == EE_SCREENS_MF_HIDDEN;
                if !free {
                    at = Some(i + 1);
                    break;
                }
                i -= 1;
            }
            h.a(EE_SCREENS_MF_OP_CHAT_DISPLAY, at.unwrap_or(EE_SCREENS_MF_CTL_CHAT_LINE1))
        } else {
            // all seven used: the lines from the last free one (or all) scrolled up, the text on the last
            while i >= EE_SCREENS_MF_CTL_CHAT_LINE1 {
                free = h.a(EE_SCREENS_MF_OP_MODE, i) == EE_SCREENS_MF_HIDDEN;
                if free {
                    break;
                }
                i -= 1;
            }
            if free {
                while i < EE_SCREENS_MF_CTL_CHAT_LINE7 {
                    h.a(EE_SCREENS_MF_OP_CHAT_COPY, i);
                    i += 1;
                }
            }
            if !free {
                i = EE_SCREENS_MF_CTL_CHAT_LINE1;
                while i < EE_SCREENS_MF_CTL_CHAT_LINE7 {
                    h.a(EE_SCREENS_MF_OP_CHAT_COPY, i);
                    i += 1;
                }
            }
            h.a(EE_SCREENS_MF_OP_CHAT_DISPLAY, i)
        };
        if (shown as u32) < h.a(EE_SCREENS_MF_OP_CHAT_LENGTH, 0) as u32 {
            h.a(EE_SCREENS_MF_OP_CHAT_REST, shown);
        } else {
            return;
        }
    }
}

/// `EEUIFMainGame::ShowInformation` (0x26533e) of the host's dialog message.
pub fn show_information(h: &mut impl FormHost) {
    if !h.t(EE_SCREENS_MF_OP_INFO_HAS_TEXT) {
        h.a(EE_SCREENS_MF_OP_HIDE_MESSAGES, 0);
        return;
    }
    h.a(EE_SCREENS_MF_OP_INFO_SET_TEXT, 0);
    if h.t(EE_SCREENS_MF_OP_INFO_HAS_PORTRAIT) {
        if h.t(EE_SCREENS_MF_OP_INFO_HAS_BUTTON) {
            h.a(EE_SCREENS_MF_OP_INFO_PORTRAIT, 0);
        } else {
            h.ab(EE_SCREENS_MF_OP_SET_MODE, EE_SCREENS_MF_CTL_DIALOG_PORTRAIT, EE_SCREENS_MF_HIDDEN);
        }
    }
    let sound = h.io(EE_SCREENS_MF_OP_INFO_SOUND, 0, 0, EeScreensMainFormIo::default()).n;
    if sound[1] as u32 != u32::MAX {
        let time = (h.a(EE_SCREENS_MF_OP_PLAY_TIME, sound[0]) as i32).wrapping_add(300);
        let duration = h.a(EE_SCREENS_MF_OP_INFO_DURATION, 0);
        h.ab(EE_SCREENS_MF_OP_INFO_SCROLL, time as u32 as i64, duration);
    } else {
        let duration = h.a(EE_SCREENS_MF_OP_INFO_DURATION, 0);
        h.ab(EE_SCREENS_MF_OP_INFO_SCROLL, duration, 0);
    }
    let rect = h.io(EE_SCREENS_MF_OP_INFO_RECT, 0, 0, EeScreensMainFormIo::default()).f;
    h.a(EE_SCREENS_MF_OP_CHAT_TOP, lrintf(rect[0] - rect[1] + 5.0) as i64);
}

#[cfg(test)]
mod tests;
