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

use crate::camera::*;
use crate::hot_keys::*;
use crate::main_game::*;
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

// ---- the form's construction and modes (round 25)

/// `TSScheduler::CreateCriticalSection(mErrorCriticalSection)`, verified (`ESVERIFY_AT(..., 633)`).
pub const EE_SCREENS_MF_OP_CREATE_ERROR_LOCK: u32 = 77;
/// `CreateGroup(a)`.
pub const EE_SCREENS_MF_OP_CREATE_GROUP: u32 = 78;
/// `mUnitHack = new EEUIFMGUnitHack(form)`.
pub const EE_SCREENS_MF_OP_NEW_UNIT_HACK: u32 = 79;
/// `mPoll = new TMPoll(a, 0, false)`, then `mPoll->Start()`.
pub const EE_SCREENS_MF_OP_NEW_POLL: u32 = 80;
/// The game window's size into io.n: a 0 `gGameWindowWidth`, `gGameWindowHeight`; a 1 `gGameWindow`'s
/// `GetWidth()`, `GetHeight()`.
pub const EE_SCREENS_MF_OP_WINDOW_SIZE: u32 = 81;
/// `mScroll = new EEAScroll` with `mEdges` io.f (left, top, right, bottom).
pub const EE_SCREENS_MF_OP_NEW_SCROLL: u32 = 82;
/// a 0: `mMouseClick = new EEAMouseClick`; 1: `mLasso = new EEALasso`; 2: `mBuild = new EEABuild`; 3: `mCalamity =
/// new EEACalamity`.
pub const EE_SCREENS_MF_OP_NEW_ACTION: u32 = 83;
/// `GetEntityGroup(a)->Enable()` (b 1) or `Disable()` (b 0).
pub const EE_SCREENS_MF_OP_ENTITY_GROUP: u32 = 84;
/// `mMouseTests[a]` = the host's mouse test b (`EE_SCREENS_MF_MOUSE_*`).
pub const EE_SCREENS_MF_OP_MOUSE_TEST: u32 = 85;
/// `mKeyboardTests[a]` = the host's keyboard test b (`EE_SCREENS_MF_KEYBOARD_*`).
pub const EE_SCREENS_MF_OP_KEYBOARD_TEST: u32 = 86;
/// `mWheelZoom = EECreateScreenEvent(a)`; `SetMouseWheelMoveEvent(mWheelZoom)`.
pub const EE_SCREENS_MF_OP_WHEEL_ZOOM: u32 = 87;
/// `mGrabScroll = EECreateGrabScroll(a)`; the middle button's pressed and unpressed event.
pub const EE_SCREENS_MF_OP_GRAB_SCROLL: u32 = 88;
/// `AddHotKey`: a the key and three modifiers (16 bits each, low first), b the group (low 32 bits) and the always
/// flag (bit 32), io.n[0] the just pressed (low) and pressed events, io.n[1] the just unpressed (low) and double
/// pressed events (each `EECreateScreenEvent(code)`, none for [`EE_SCREENS_MF_NO_EVENT`]).
pub const EE_SCREENS_MF_OP_ADD_HOT_KEY: u32 = 89;
/// `SetHotKeyGroupEnabled(a, b != 0)`.
pub const EE_SCREENS_MF_OP_HOT_KEY_GROUP: u32 = 90;
/// `GetManager()->ForceUIUpdate()`.
pub const EE_SCREENS_MF_OP_FORCE_UI_UPDATE: u32 = 91;
/// `gUserInterface->UpdateJITManufacturing()` when there is a `gUserInterface`.
pub const EE_SCREENS_MF_OP_UPDATE_JIT: u32 = 92;
/// a 0: `mBuildWall.Terminate()`; a 1: `mBuildGate.Terminate()`.
pub const EE_SCREENS_MF_OP_BUILD_TERMINATE: u32 = 93;
/// `SetAllControlsDirty()`.
pub const EE_SCREENS_MF_OP_ALL_DIRTY: u32 = 94;
/// The current player's tech tree node a, held: != NULL.
pub const EE_SCREENS_MF_OP_NODE: u32 = 95;
/// `gUnitClassTable[node->mData->mUnitClass] != NULL` of the held node (0 without one).
pub const EE_SCREENS_MF_OP_NODE_CLASS: u32 = 96;
/// The current player's unit a, held: 0 none, 1 no complex unit, 2 a complex unit.
pub const EE_SCREENS_MF_OP_UNIT: u32 = 97;
/// `CanProduce(a, the held unit's complex class's type, 1, true, 3, &error)`, the error the held text.
pub const EE_SCREENS_MF_OP_CAN_PRODUCE: u32 = 98;
/// `gSoundManager->Play(EESoundObject::Get(a)->GetSampleID(), 1, 0, 1, false, NULL)`.
pub const EE_SCREENS_MF_OP_PLAY_SOUND: u32 = 99;
/// `EEGhostUnit::Create(GetMouseLocation(), class, player, 0, false)`: a 0 the held node's unit class, a 1 the held
/// calamity's volcano class.
pub const EE_SCREENS_MF_OP_GHOST: u32 = 100;
/// `gUserInterface->AddChatLogEntry(held text, 0)`.
pub const EE_SCREENS_MF_OP_CHAT_LOG_ERROR: u32 = 101;
/// The gate placement of tech a: `mBuildGate`'s tech, wall type (the node's `GetUnitClass(0)`), class (the node's),
/// `Initialize(the player's id)`.
pub const EE_SCREENS_MF_OP_GATE_BEGIN: u32 = 102;
/// The wall placement of tech a: `mBuildWall.mTechnology`, `Initialize(the player's id)`.
pub const EE_SCREENS_MF_OP_WALL_BEGIN: u32 = 103;
/// The current player's selected group's `GetNumUnits()`.
pub const EE_SCREENS_MF_OP_SELECTED_COUNT: u32 = 104;
/// The caster (the selected group's first unit when it has units), held: != NULL.
pub const EE_SCREENS_MF_OP_CASTER: u32 = 105;
/// The held node's `mData->mCalamity`.
pub const EE_SCREENS_MF_OP_NODE_CALAMITY: u32 = 106;
/// The calamity manager's class of calamity a, held: != NULL.
pub const EE_SCREENS_MF_OP_CALAMITY_CLASS: u32 = 107;
/// `mCalamity->SetCalamity(a, class)`, `SetCasterID(the caster's id)`: the class's `GetTargetKind()`.
pub const EE_SCREENS_MF_OP_CALAMITY_SET: u32 = 108;
/// `mCalamity->Cast(GetMouseLocation())`.
pub const EE_SCREENS_MF_OP_CALAMITY_CAST: u32 = 109;
/// `gUIManager->mMainGameForm->mRefreshGroup = true` when there is a main game form.
pub const EE_SCREENS_MF_OP_REFRESH_MAIN_FORM: u32 = 110;

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

// ---- the form's construction and modes: its fields, the panel group's select, the tests, the hot keys

/// `mInfoBlinking`, `mIsButtonBlinking`, `mDrawToolTipShadow`, `mEntityGroup`, `mCurrentGroup`, `mMediaID`,
/// `mEntityGroupSet`, `mB5F0`, `mHotKeyMode`, `mBuild->mTechnology`.
pub const EE_SCREENS_MF_FIELD_INFO_BLINKING: i64 = 9;
pub const EE_SCREENS_MF_FIELD_BUTTON_BLINKING: i64 = 10;
pub const EE_SCREENS_MF_FIELD_TOOLTIP_SHADOW: i64 = 11;
pub const EE_SCREENS_MF_FIELD_ENTITY_GROUP: i64 = 12;
pub const EE_SCREENS_MF_FIELD_CURRENT_GROUP: i64 = 13;
pub const EE_SCREENS_MF_FIELD_MEDIA_ID: i64 = 14;
pub const EE_SCREENS_MF_FIELD_ENTITY_GROUP_SET: i64 = 15;
pub const EE_SCREENS_MF_FIELD_B5F0: i64 = 16;
pub const EE_SCREENS_MF_FIELD_HOT_KEY_MODE: i64 = 17;
pub const EE_SCREENS_MF_FIELD_BUILD_TECH: i64 = 18;
/// The panel group's `Select()` (EE_SCREENS_MF_OP_GROUP).
pub const EE_SCREENS_MF_GROUP_SELECT: i64 = 3;
/// No event in a hot key's slot (EE_SCREENS_MF_OP_ADD_HOT_KEY).
pub const EE_SCREENS_MF_NO_EVENT: u32 = 0xffff_ffff;
/// The cancel-mode button (`kMainGameCancelUIMode`).
pub const EE_SCREENS_MF_CTL_CANCEL_UI_MODE: i64 = 244;
/// `kSoundButton1`, `kSoundButton2`.
pub const EE_SCREENS_MF_SOUND_BUTTON1: i64 = 0x207;
pub const EE_SCREENS_MF_SOUND_BUTTON2: i64 = 0x208;
/// `mUIState`: the build, calamity, patrol and flare modes.
pub const EE_SCREENS_MF_STATE_BUILD: i64 = 1;
pub const EE_SCREENS_MF_STATE_CALAMITY: i64 = 5;
pub const EE_SCREENS_MF_STATE_PATROL: i64 = 0x10;
pub const EE_SCREENS_MF_STATE_FLARE: i64 = 0x15;
/// The volcano's tech (its calamity mode shows a volcano ghost).
pub const EE_SCREENS_MF_TECH_VOLCANO: i64 = 0x3f9;

// the host's mouse tests, in its table's order (EE_SCREENS_MF_OP_MOUSE_TEST's b): EEModeNormal, EEModeBomberRally,
// EEModeFighterRally, EEModeAtomicBomberRally, EEModeBuild, EEModeRally, EEModeUnload, EEModeGarrison,
// EEModeCalamity, EEModeWall, EEModeGate, EEModeTimedAtomicBomberRally, EEModeTimedBomberRally,
// EEModeTimedFighterRally, EEModeTimedRally, EEModeCamera, EEModeHeal, EEModeMouseNone, EEModeFlare,
// EEModeAttackGround, EEModeParatrooperRally, EEModeConvert; its keyboard tests: EEModeKeyboardNone, EEFn27C0B4B,
// EEModeKeyboardCinematic
/// Each interface mode's mouse test (mode, the host's test), in the constructor's order.
const MOUSE_TESTS: [(i64, i64); 22] = [
    (0, 0),
    (9, 1),
    (8, 2),
    (10, 3),
    (1, 4),
    (2, 5),
    (3, 6),
    (7, 7),
    (5, 8),
    (6, 9),
    (11, 10),
    (12, 11),
    (13, 12),
    (14, 13),
    (15, 14),
    (4, 15),
    (16, 16),
    (17, 17),
    (18, 18),
    (19, 19),
    (20, 20),
    (21, 21),
];
/// Each interface mode's keyboard test: none, but the camera's (its own `return false`) and the cinematic's.
const KEYBOARD_TESTS: [(i64, i64); 22] = [
    (0, 0),
    (9, 0),
    (8, 0),
    (10, 0),
    (1, 0),
    (2, 0),
    (3, 0),
    (7, 0),
    (5, 0),
    (6, 0),
    (11, 0),
    (12, 0),
    (13, 0),
    (14, 0),
    (15, 0),
    (4, 1),
    (16, 0),
    (17, 2),
    (18, 0),
    (19, 0),
    (20, 0),
    (21, 0),
];

// the keys (ISKeyCode.h: DirectInput's codes and the three modifiers)
const K_ESCAPE: u16 = 0x01;
const K_BACK: u16 = 0x0e;
const K_TAB: u16 = 0x0f;
const K_Q: u16 = 0x10;
const K_W: u16 = 0x11;
const K_R: u16 = 0x13;
const K_Y: u16 = 0x15;
const K_U: u16 = 0x16;
const K_I: u16 = 0x17;
const K_LBRACKET: u16 = 0x1a;
const K_RBRACKET: u16 = 0x1b;
const K_RETURN: u16 = 0x1c;
const K_A: u16 = 0x1e;
const K_S: u16 = 0x1f;
const K_D: u16 = 0x20;
const K_F: u16 = 0x21;
const K_G: u16 = 0x22;
const K_H: u16 = 0x23;
const K_GRAVE: u16 = 0x29;
const K_BACKSLASH: u16 = 0x2b;
const K_Z: u16 = 0x2c;
const K_X: u16 = 0x2d;
const K_C: u16 = 0x2e;
const K_V: u16 = 0x2f;
const K_B: u16 = 0x30;
const K_N: u16 = 0x31;
const K_M: u16 = 0x32;
const K_COMMA: u16 = 0x33;
const K_PERIOD: u16 = 0x34;
const K_SLASH: u16 = 0x35;
const K_SPACE: u16 = 0x39;
const K_F1: u16 = 0x3b;
const K_F2: u16 = 0x3c;
const K_F3: u16 = 0x3d;
const K_F4: u16 = 0x3e;
const K_F5: u16 = 0x3f;
const K_F10: u16 = 0x44;
const K_SUBTRACT: u16 = 0x4a;
const K_ADD: u16 = 0x4e;
const K_F11: u16 = 0x57;
const K_NUMPAD_ENTER: u16 = 0x9c;
const K_UP: u16 = 0xc8;
const K_PRIOR: u16 = 0xc9;
const K_LEFT: u16 = 0xcb;
const K_RIGHT: u16 = 0xcd;
const K_DOWN: u16 = 0xd0;
const K_DELETE: u16 = 0xd3;
const K_CONTROL: u16 = 0xe0;
const K_SHIFT: u16 = 0xe1;
const K_ALT: u16 = 0xe2;
const K_NONE: u16 = 0x7fff;

// the unit types of the selection hot keys (EEUnitClassIDs.h)
const T_CITIZEN: i32 = 0x20;
const T_BARRACKS: i32 = 0x22;
const T_TOWN_CENTER: i32 = 0x23;
const T_TEMPLE: i32 = 0x27;
const T_SETTLEMENT: i32 = 0x2b;
const T_GRANARY: i32 = 0x34;
const T_ARCHERY_RANGE: i32 = 0x36;
const T_UNIVERSITY: i32 = 0x3c;
const T_HOSPITAL: i32 = 0x3e;
const T_SIEGE_FACTORY: i32 = 0x3f;
const T_STABLE: i32 = 0x40;
const T_NAVY_YARD: i32 = 0x41;
const T_AIRPORT: i32 = 0x42;
const T_MISSILE_BASE: i32 = 0x43;
const T_TANK_FACTORY: i32 = 0x44;
const T_CYBER_FACTORY: i32 = 0x45;
const T_CYBER_LABORATORY: i32 = 0x24a;
const T_CP_MISSILE_BASE: i32 = 0x2ed;
const T_PRIEST_TOWER: i32 = 0x2f0;
const T_MARKET_CP: i32 = 0x321;

/// One hot key of the form: the key, its three modifiers, its four events (just pressed, pressed, just unpressed,
/// double pressed; [`EE_SCREENS_MF_NO_EVENT`] for none) and whether it always fires (group 0).
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct FormHotKey {
    pub key: u16,
    pub modifiers: [u16; 3],
    pub events: [u32; 4],
    pub always: bool,
}

/// The main game form's hot keys in the constructor's order: the ten control groups' four keys each, then the rest.
pub fn hot_keys() -> Vec<FormHotKey> {
    const NO: u32 = EE_SCREENS_MF_NO_EVENT;
    let key = |key: u16, m1: u16, m2: u16, events: [u32; 4], always: bool| FormHotKey {
        key,
        modifiers: [m1, m2, K_NONE],
        events,
        always,
    };
    let press = |k: u16, m: u16, event: u32| key(k, m, K_NONE, [event, NO, NO, NO], false);
    let held = |k: u16, event: u32| key(k, K_NONE, K_NONE, [NO, event, NO, NO], true);
    let select =
        |t: i32, workers: bool, military: bool, all: bool| ee_screens_hot_key_unit_selection(t, workers, military, all);
    let mut out = Vec::new();
    for i in 0..10u16 {
        let g = i as u32;
        out.push(key(
            i + 2,
            K_NONE,
            K_NONE,
            [EE_SCREENS_HK_SELECT_GROUP + g, NO, NO, EE_SCREENS_HK_SELECT_GROUP_ADD + g],
            false,
        ));
        out.push(press(i + 2, K_SHIFT, EE_SCREENS_HK_ADD_TO_GROUP + g));
        out.push(press(i + 2, K_CONTROL, EE_SCREENS_HK_CREATE_GROUP + g));
        out.push(press(i + 2, K_ALT, EE_SCREENS_HK_SELECT_GROUP_ADD + g));
    }
    out.push(press(K_S, K_ALT, EE_SCREENS_MG_BEHAVIOR_SCOUT));
    out.push(press(K_D, K_ALT, EE_SCREENS_MG_BEHAVIOR_STAND_GROUND));
    out.push(press(K_G, K_ALT, EE_SCREENS_MG_BEHAVIOR_GUARD));
    out.push(press(K_A, K_ALT, EE_SCREENS_MG_BEHAVIOR_AGGRESSIVE));
    let aircraft = [K_B, K_A, K_F, K_D, K_S, K_W];
    for (n, k) in aircraft.iter().enumerate() {
        out.push(press(*k, K_NONE, EE_SCREENS_HK_AIRCRAFT + n as u32));
    }
    out.push(key(K_PRIOR, K_NONE, K_NONE, [EE_SCREENS_MG_PREVIOUS_MESSAGES, NO, NO, NO], true));
    for (n, k) in aircraft.iter().enumerate() {
        out.push(press(*k, K_SHIFT, EE_SCREENS_HK_AIRCRAFT + n as u32));
    }
    out.push(held(K_UP, EE_SCREENS_CAM_SCROLL_UP));
    out.push(held(K_DOWN, EE_SCREENS_CAM_SCROLL_DOWN));
    out.push(held(K_LEFT, EE_SCREENS_CAM_SCROLL_LEFT));
    out.push(held(K_RIGHT, EE_SCREENS_CAM_SCROLL_RIGHT));
    out.push(key(K_SHIFT, K_NONE, K_NONE, [EE_SCREENS_MG_ENTER_SHIFT, NO, EE_SCREENS_MG_EXIT_SHIFT, NO], true));
    out.push(press(K_DELETE, K_NONE, EE_SCREENS_MG_KILL));
    out.push(press(K_DELETE, K_SHIFT, EE_SCREENS_MG_KILL_ALL));
    // New: Backspace deletes too (a Mac keyboard's "delete" key is Backspace; it has no forward Delete key)
    out.push(press(K_BACK, K_NONE, EE_SCREENS_MG_KILL));
    out.push(press(K_BACK, K_SHIFT, EE_SCREENS_MG_KILL_ALL));
    out.push(press(K_ADD, K_NONE, EE_SCREENS_MG_FASTER));
    out.push(press(K_SUBTRACT, K_NONE, EE_SCREENS_MG_SLOWER));
    out.push(press(K_ESCAPE, K_NONE, EE_SCREENS_MG_CANCEL_UPDATE_STATE));
    out.push(press(K_RETURN, K_NONE, EE_SCREENS_MG_DISPLAY_CHAT));
    out.push(press(K_NUMPAD_ENTER, K_NONE, EE_SCREENS_MG_DISPLAY_CHAT));
    out.push(held(K_RBRACKET, EE_SCREENS_CAM_ZOOM_IN));
    out.push(held(K_LBRACKET, EE_SCREENS_CAM_ZOOM_OUT));
    out.push(press(K_COMMA, K_NONE, select(0, false, true, false)));
    out.push(press(K_COMMA, K_SHIFT, select(0, false, true, false)));
    out.push(press(K_COMMA, K_CONTROL, select(0, false, true, true)));
    out.push(key(
        K_GRAVE,
        K_NONE,
        K_NONE,
        [EE_SCREENS_CAM_DISABLE_MOUSE_MOVE, EE_SCREENS_CAM_ROTATE, EE_SCREENS_CAM_ENABLE_MOUSE_MOVE, NO],
        true,
    ));
    out.push(press(K_PERIOD, K_NONE, EE_SCREENS_MG_FOLLOW));
    let building = |k: u16, t: i32| press(k, K_CONTROL, select(t, false, false, false));
    out.push(building(K_A, T_ARCHERY_RANGE));
    out.push(building(K_B, T_BARRACKS));
    out.push(building(K_C, T_SIEGE_FACTORY));
    out.push(press(K_F, K_ALT, EE_SCREENS_MG_FLARE));
    out.push(building(K_F, T_TANK_FACTORY));
    out.push(building(K_G, T_GRANARY));
    out.push(press(K_H, K_NONE, select(T_TOWN_CENTER, false, false, false)));
    out.push(building(K_I, T_MISSILE_BASE));
    out.push(building(K_N, T_SETTLEMENT));
    out.push(building(K_Q, T_AIRPORT));
    out.push(building(K_R, T_CYBER_FACTORY));
    out.push(building(K_S, T_STABLE));
    out.push(building(K_V, T_NAVY_YARD));
    out.push(building(K_X, T_CYBER_LABORATORY));
    out.push(building(K_Y, T_TEMPLE));
    out.push(key(K_Z, K_CONTROL, K_ALT, [EE_SCREENS_MG_ATTACK, NO, NO, NO], false));
    out.push(key(K_Z, K_CONTROL, K_SHIFT, [EE_SCREENS_MG_ATTACK_ALLIES, NO, NO, NO], false));
    out.push(press(K_TAB, K_NONE, select(T_CITIZEN, true, false, false)));
    out.push(press(K_TAB, K_SHIFT, select(T_CITIZEN, true, false, false)));
    out.push(press(K_TAB, K_CONTROL, select(T_CITIZEN, true, false, true)));
    out.push(press(K_SPACE, K_NONE, EE_SCREENS_HK_PLAYER_EVENT));
    out.push(press(K_F1, K_NONE, EE_SCREENS_MG_ENTER_EDITOR));
    out.push(press(K_F2, K_NONE, EE_SCREENS_MG_PERSPECTIVE));
    out.push(press(K_F3, K_NONE, EE_SCREENS_MG_PAUSE));
    out.push(building(K_BACKSLASH, T_CP_MISSILE_BASE));
    out.push(building(K_SLASH, T_PRIEST_TOWER));
    out.push(building(K_M, T_MARKET_CP));
    out.push(building(K_U, T_UNIVERSITY));
    out.push(building(K_Z, T_HOSPITAL));
    out.push(press(K_F4, K_NONE, EE_SCREENS_MG_QUICK_SAVE));
    out.push(press(K_F4, K_SHIFT, EE_SCREENS_MG_QUICK_LOAD));
    out.push(press(K_F4, K_CONTROL, EE_SCREENS_MG_AUTO_LOAD));
    out.push(press(K_F5, K_NONE, EE_SCREENS_MG_TOGGLE_HIDDEN));
    out.push(key(K_F10, K_NONE, K_NONE, [NO, NO, EE_SCREENS_MG_DISPLAY_OPTIONS, NO], false));
    out.push(press(K_F11, K_NONE, EE_SCREENS_MG_TOGGLE_CLOCK));
    out
}

/// The host's io of one hot key (EE_SCREENS_MF_OP_ADD_HOT_KEY's a, b and io).
fn add_hot_key(h: &mut impl FormHost, k: &FormHotKey) {
    let a =
        k.key as i64 | (k.modifiers[0] as i64) << 16 | (k.modifiers[1] as i64) << 32 | (k.modifiers[2] as i64) << 48;
    let b = (k.always as i64) << 32;
    let mut io = EeScreensMainFormIo::default();
    io.n[0] = (k.events[0] as u64 | (k.events[1] as u64) << 32) as i64;
    io.n[1] = (k.events[2] as u64 | (k.events[3] as u64) << 32) as i64;
    h.op(EE_SCREENS_MF_OP_ADD_HOT_KEY, a, b, &mut io);
}

/// `EEUIFMainGame`'s constructor (0x262e73) after `UIForm`'s: the error lock, the flags, the six panel groups, the
/// unit hack, the 200 ms poll, the mouse actions, the entity groups, every mode's mouse and keyboard test, the wheel
/// zoom and grab scroll, the hot keys. It reads no control (the form's controls are made after it).
pub fn construct(h: &mut impl FormHost) {
    h.a(EE_SCREENS_MF_OP_CREATE_ERROR_LOCK, 0);
    h.set_field(EE_SCREENS_MF_FIELD_INFO_BLINKING, 0);
    h.set_field(EE_SCREENS_MF_FIELD_BUTTON_BLINKING, 0);
    h.set_field(EE_SCREENS_MF_FIELD_TOOLTIP_SHADOW, 0);
    h.set_field(EE_SCREENS_MF_FIELD_CURSOR_LOCKED, 0);
    h.set_field(EE_SCREENS_MF_FIELD_UI_STATE, 0);
    h.set_field(EE_SCREENS_MF_FIELD_ENTITY_GROUP, 0);
    h.set_field(EE_SCREENS_MF_FIELD_CURRENT_GROUP, 0);
    for i in 0..6 {
        h.a(EE_SCREENS_MF_OP_CREATE_GROUP, i);
    }
    h.a(EE_SCREENS_MF_OP_NEW_UNIT_HACK, 0);
    h.set_field(EE_SCREENS_MF_FIELD_ELAPSED, 0);
    h.a(EE_SCREENS_MF_OP_NEW_POLL, 200);
    let size = h.io(EE_SCREENS_MF_OP_WINDOW_SIZE, 0, 0, EeScreensMainFormIo::default()).n;
    let edges = EeScreensMainFormIo {
        f: [0.0, 0.0, size[0] as u32 as f32 - 1.0, size[1] as u32 as f32 - 1.0],
        ..Default::default()
    };
    h.io(EE_SCREENS_MF_OP_NEW_SCROLL, 0, 0, edges);
    for action in 0..4 {
        h.a(EE_SCREENS_MF_OP_NEW_ACTION, action);
    }
    h.set_field(EE_SCREENS_MF_FIELD_MEDIA_ID, -1);
    h.set_field(EE_SCREENS_MF_FIELD_ENTITY_GROUP_SET, 0);
    h.set_field(EE_SCREENS_MF_FIELD_STRINGS_CHANGED, 0);
    h.set_field(EE_SCREENS_MF_FIELD_CLOCK_MODE, 0);
    h.ab(EE_SCREENS_MF_OP_ENTITY_GROUP, 0, 1);
    set_entity_group(h, 1);
    h.ab(EE_SCREENS_MF_OP_ENTITY_GROUP, 9, 0);
    h.ab(EE_SCREENS_MF_OP_ENTITY_GROUP, 8, 0);
    for (mode, test) in MOUSE_TESTS {
        h.ab(EE_SCREENS_MF_OP_MOUSE_TEST, mode, test);
    }
    for (mode, test) in KEYBOARD_TESTS {
        h.ab(EE_SCREENS_MF_OP_KEYBOARD_TEST, mode, test);
    }
    h.a(EE_SCREENS_MF_OP_WHEEL_ZOOM, EE_SCREENS_CAM_WHEEL_ZOOM as i64);
    h.a(EE_SCREENS_MF_OP_GRAB_SCROLL, 0);
    for k in hot_keys() {
        add_hot_key(h, &k);
    }
    set_hot_key_mode(h, 0);
    h.set_field(EE_SCREENS_MF_FIELD_B5F0, 0);
    h.set_field(EE_SCREENS_MF_FIELD_REFRESH_GROUP, 1);
}

/// `EEUIFMainGame::SetHotKeyMode` (0x2668a0): group 2 always, groups 0 and 1 by the mode (0 the first, 1 the second,
/// 2 both).
pub fn set_hot_key_mode(h: &mut impl FormHost, mode: i64) {
    h.set_field(EE_SCREENS_MF_FIELD_HOT_KEY_MODE, mode);
    h.ab(EE_SCREENS_MF_OP_HOT_KEY_GROUP, 2, 1);
    let (first, second) = match h.field(EE_SCREENS_MF_FIELD_HOT_KEY_MODE) as u32 {
        0 => (1, 0),
        1 => (0, 1),
        2 => (1, 1),
        _ => return,
    };
    h.ab(EE_SCREENS_MF_OP_HOT_KEY_GROUP, 0, first);
    h.ab(EE_SCREENS_MF_OP_HOT_KEY_GROUP, 1, second);
}

/// `EEUIFMainGame::SetUIState` (0x2668ee): the ghosts cleared, the manager's update forced, the JIT manufacturing
/// updated, a wall or gate placement ended, the new mode.
pub fn set_ui_state(h: &mut impl FormHost, state: i64) {
    h.a(EE_SCREENS_MF_OP_GHOST_LOCK, 1);
    h.a(EE_SCREENS_MF_OP_CLEAR_GHOSTS, 0);
    h.a(EE_SCREENS_MF_OP_GHOST_LOCK, 0);
    h.a(EE_SCREENS_MF_OP_FORCE_UI_UPDATE, 0);
    h.a(EE_SCREENS_MF_OP_UPDATE_JIT, 0);
    match h.field(EE_SCREENS_MF_FIELD_UI_STATE) {
        EE_SCREENS_MF_STATE_WALL => {
            h.a(EE_SCREENS_MF_OP_BUILD_TERMINATE, 0);
        }
        EE_SCREENS_MF_STATE_GATE => {
            h.a(EE_SCREENS_MF_OP_BUILD_TERMINATE, 1);
        }
        _ => {}
    }
    h.set_field(EE_SCREENS_MF_FIELD_UI_STATE, state);
}

/// `EEUIFMainGame::SetEntityGroup` (0x2655da): the old entity group off (once one was set), the new one on, the
/// current panel group selected, mode 0, the bottom panel redrawn.
pub fn set_entity_group(h: &mut impl FormHost, group: i64) {
    if h.field(EE_SCREENS_MF_FIELD_ENTITY_GROUP_SET) != 0 {
        let old = h.field(EE_SCREENS_MF_FIELD_ENTITY_GROUP);
        h.ab(EE_SCREENS_MF_OP_ENTITY_GROUP, old, 0);
    }
    h.ab(EE_SCREENS_MF_OP_ENTITY_GROUP, group, 1);
    h.set_field(EE_SCREENS_MF_FIELD_ENTITY_GROUP_SET, 1);
    h.set_field(EE_SCREENS_MF_FIELD_ENTITY_GROUP, group);
    h.a(EE_SCREENS_MF_OP_GROUP, EE_SCREENS_MF_GROUP_SELECT);
    set_ui_state(h, 0);
    let view = h.io(EE_SCREENS_MF_OP_VIEW_RECT, 0, 0, EeScreensMainFormIo::default()).f;
    let size = h.io(EE_SCREENS_MF_OP_WINDOW_SIZE, 1, 0, EeScreensMainFormIo::default()).n;
    let dirty =
        EeScreensMainFormIo { f: [0.0, view[3], size[0] as u32 as f32, size[1] as u32 as f32], ..Default::default() };
    h.io(EE_SCREENS_MF_OP_ADD_DIRTY, 0, 0, dirty);
    h.a(EE_SCREENS_MF_OP_ALL_DIRTY, 0);
}

/// `EEUIFMainGame::BuildUnit` (0x266384): the build mode for a unit's tech when the player can produce it (a click,
/// the tech, mode 1, the building's ghost at the mouse), else the refusal's sound and its text in the chat log.
pub fn build_unit(h: &mut impl FormHost, unit: i64, tech: i64) {
    let node = h.a(EE_SCREENS_MF_OP_NODE, tech) != 0;
    let unit = h.a(EE_SCREENS_MF_OP_UNIT, unit);
    if unit != 2 || !node {
        return;
    }
    if h.a(EE_SCREENS_MF_OP_CAN_PRODUCE, tech) != 0 {
        h.a(EE_SCREENS_MF_OP_PLAY_SOUND, EE_SCREENS_MF_SOUND_BUTTON1);
        h.set_field(EE_SCREENS_MF_FIELD_BUILD_TECH, tech);
        set_ui_state(h, EE_SCREENS_MF_STATE_BUILD);
        h.a(EE_SCREENS_MF_OP_GHOST, 0);
    } else {
        h.a(EE_SCREENS_MF_OP_PLAY_SOUND, EE_SCREENS_MF_SOUND_BUTTON2);
        h.a(EE_SCREENS_MF_OP_CHAT_LOG_ERROR, 0);
    }
}

/// `EEUIFMainGame::StartGate` (0x2664b4): the gate placement (mode 0xb) of a tech whose node has a unit class.
pub fn start_gate(h: &mut impl FormHost, tech: i64) {
    let node = h.a(EE_SCREENS_MF_OP_NODE, tech) != 0;
    if !h.t(EE_SCREENS_MF_OP_NODE_CLASS) || !node {
        return;
    }
    h.set_field(EE_SCREENS_MF_FIELD_BUILD_TECH, tech);
    h.a(EE_SCREENS_MF_OP_GATE_BEGIN, tech);
    set_ui_state(h, EE_SCREENS_MF_STATE_GATE);
    h.a(EE_SCREENS_MF_OP_PLAY_SOUND, EE_SCREENS_MF_SOUND_BUTTON1);
}

/// `EEUIFMainGame::StartWall` (0x266594): the wall placement (mode 6) of a tech whose node has a unit class.
pub fn start_wall(h: &mut impl FormHost, tech: i64) {
    let node = h.a(EE_SCREENS_MF_OP_NODE, tech) != 0;
    if !h.t(EE_SCREENS_MF_OP_NODE_CLASS) || !node {
        return;
    }
    h.set_field(EE_SCREENS_MF_FIELD_BUILD_TECH, tech);
    h.a(EE_SCREENS_MF_OP_WALL_BEGIN, tech);
    set_ui_state(h, EE_SCREENS_MF_STATE_WALL);
    h.a(EE_SCREENS_MF_OP_PLAY_SOUND, EE_SCREENS_MF_SOUND_BUTTON1);
}

/// The cancel-mode button shown and the main game form's panel group refreshed (the targeting modes' start).
fn show_cancel(h: &mut impl FormHost) {
    h.ab(EE_SCREENS_MF_OP_SET_MODE, EE_SCREENS_MF_CTL_CANCEL_UI_MODE, EE_SCREENS_MF_SHOWN);
    h.a(EE_SCREENS_MF_OP_REFRESH_MAIN_FORM, 0);
}

/// `EEUIFMainGame::StartCalamity` (0x26662d): a tech's calamity with the selection's first unit as its caster, cast
/// at once at the mouse when it targets its caster's spot (kind 1), else the calamity mode (5) with the cancel
/// button (and the volcano's ghost).
pub fn start_calamity(h: &mut impl FormHost, tech: i64) {
    let node = h.a(EE_SCREENS_MF_OP_NODE, tech) != 0;
    if !h.t(EE_SCREENS_MF_OP_CASTER) || !node {
        return;
    }
    let calamity = h.a(EE_SCREENS_MF_OP_NODE_CALAMITY, 0) as i32;
    if calamity <= 0 || h.a(EE_SCREENS_MF_OP_CALAMITY_CLASS, calamity as i64) == 0 {
        return;
    }
    if h.a(EE_SCREENS_MF_OP_CALAMITY_SET, tech) as i32 == 1 {
        h.a(EE_SCREENS_MF_OP_CALAMITY_CAST, 0);
        return;
    }
    set_ui_state(h, EE_SCREENS_MF_STATE_CALAMITY);
    h.ab(EE_SCREENS_MF_OP_SET_MODE, EE_SCREENS_MF_CTL_CANCEL_UI_MODE, EE_SCREENS_MF_SHOWN);
    if tech == EE_SCREENS_MF_TECH_VOLCANO {
        h.a(EE_SCREENS_MF_OP_GHOST, 1);
    }
    h.a(EE_SCREENS_MF_OP_REFRESH_MAIN_FORM, 0);
}

/// `EEUIFMainGame::StartFlare` (0x26679a) and `StartPatrol` (0x26681d): the targeting mode when units are
/// selected.
pub fn start_targeting(h: &mut impl FormHost, state: i64) {
    if h.a(EE_SCREENS_MF_OP_SELECTED_COUNT, 0) as u32 == 0 {
        return;
    }
    set_ui_state(h, state);
    h.a(EE_SCREENS_MF_OP_PLAY_SOUND, EE_SCREENS_MF_SOUND_BUTTON1);
    show_cancel(h);
}

#[cfg(test)]
mod tests;
