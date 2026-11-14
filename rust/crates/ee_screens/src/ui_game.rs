//! The UI thread's game starts and its loop (`EEUserInterface`, `User Interface Thread/EEUserInterface.cpp`):
//! `TestScenario` and `SaveAndTestScenario` (the edited scenario restarted as a game: its continents, herds, towns,
//! AI centres and units, then the main game form), `LoadSavedGame` (a saved game's header checked, a local server
//! started and the load posted) and `Run` (the thread's loop: its messages collected under the queue's lock, then
//! handled, the interface updated, the free camera moved).
//!
//! Every call on the world, the UI manager, the threads and the UI object is one host op ([`UiHost`],
//! `ee_screenscpp_ui_game`), asked where and as often as the original asked it.

use crate::ui_thread::UiHost;

// ---- the host's ops (EE_SCREENS_UG_OP_*): a, b and c are the op's arguments

/// `PauseGraphics()` (a 1) or `ResumeGraphics()` (a 0).
pub const EE_SCREENS_UG_OP_GRAPHICS: u32 = 1;
/// The world's `mLock->WriteLock()` (a 1) or `WriteUnlock()` (a 0).
pub const EE_SCREENS_UG_OP_WORLD_LOCK: u32 = 2;
/// `gGameScreenActive = true`.
pub const EE_SCREENS_UG_OP_SCREEN_ACTIVE: u32 = 3;
/// `gContinentManager.Clear(map)`, `BuildRegions(map)`.
pub const EE_SCREENS_UG_OP_CONTINENTS: u32 = 4;
/// `gWorld.mNumPlayers`.
pub const EE_SCREENS_UG_OP_NUM_PLAYERS: u32 = 5;
/// Player a's call b (`EE_SCREENS_UG_PLAYER_*`).
pub const EE_SCREENS_UG_OP_PLAYER: u32 = 6;
/// `gPathManager.Reset()`.
pub const EE_SCREENS_UG_OP_PATH_RESET: u32 = 7;
/// `gSightUpdatesEnabled = true`.
pub const EE_SCREENS_UG_OP_SIGHT_ON: u32 = 8;
/// `gWorld.RestartUnits()`.
pub const EE_SCREENS_UG_OP_RESTART_UNITS: u32 = 9;
/// `EEUpdateLandContinents()`.
pub const EE_SCREENS_UG_OP_LAND_CONTINENTS: u32 = 10;
/// `ResizeGameScreen(gGameWindowWidth, gGameWindowHeight, a != 0, b != 0)`.
pub const EE_SCREENS_UG_OP_RESIZE_GAME: u32 = 11;
/// `gUIManager->SwapForm(gUIManager->mMainGameForm)`.
pub const EE_SCREENS_UG_OP_SWAP_MAIN: u32 = 12;
/// `gMiniMap->Redraw()`, `Refresh()`.
pub const EE_SCREENS_UG_OP_MINI_MAP: u32 = 13;
/// `EESetGameViewRectangle` of the game view (`kGameViewLeft`, `kGameViewTop`, `kGameViewRight`, `kGameViewBottom`).
pub const EE_SCREENS_UG_OP_GAME_VIEW: u32 = 14;
/// `gAmbientSoundManager.SetEnabled(the ambient sounds setting)`, `Restart()`.
pub const EE_SCREENS_UG_OP_AMBIENT: u32 = 15;
/// `gPhysics->mThread->Post(a, NULL, NULL, true)`.
pub const EE_SCREENS_UG_OP_PHYSICS_POST: u32 = 16;
/// `gMusicManager->Start()`.
pub const EE_SCREENS_UG_OP_MUSIC: u32 = 17;
/// `gWorld.mMap->PrepareSave()`.
pub const EE_SCREENS_UG_OP_PREPARE_SAVE: u32 = 18;
/// `SaveGame("ScenDefault" + gScenarioExtension, TMGetTime(), true, "data\\Scenarios\\")`: its result.
pub const EE_SCREENS_UG_OP_SAVE_SCENARIO: u32 = 19;
/// `gUIManager->DisplayMessage(gLoadSaveErrorMessage, true, NULL, NULL, true)`.
pub const EE_SCREENS_UG_OP_SAVE_ERROR: u32 = 20;
/// The world's game options: load type 1, map type "ScenDefault" + `gScenarioExtension`.
pub const EE_SCREENS_UG_OP_SCENARIO_OPTIONS: u32 = 21;
/// `gWorld.SetNumPlayers(gWorld.mMaxPlayers)`.
pub const EE_SCREENS_UG_OP_MAX_PLAYERS: u32 = 22;
/// `gArtificialIntelligence->CollectComputerPlayers()`.
pub const EE_SCREENS_UG_OP_COLLECT_COMPUTERS: u32 = 23;
/// The saved game header's `LoadFromFile(*file a)` (the save version's) == `kErrorOK`.
pub const EE_SCREENS_UG_OP_LOAD_HEADER: u32 = 24;
/// `gPhysics->mTimerRunning`.
pub const EE_SCREENS_UG_OP_TIMER_RUNNING: u32 = 25;
/// `PlatSleep(a)`.
pub const EE_SCREENS_UG_OP_SLEEP: u32 = 26;
/// The old communications deleted, a local `EEServer` made and its threads started.
pub const EE_SCREENS_UG_OP_NEW_SERVER: u32 = 27;
/// `gUIManager->mB13D = false`.
pub const EE_SCREENS_UG_OP_B13D_OFF: u32 = 28;
/// `gGameState.Clear()`.
pub const EE_SCREENS_UG_OP_STATE_CLEAR: u32 = 29;
/// `EEStopAllSounds()`.
pub const EE_SCREENS_UG_OP_STOP_SOUNDS: u32 = 30;
/// `gUIManager->SwapForm(gUIManager->GetFormByID(kFormSplash))`.
pub const EE_SCREENS_UG_OP_SWAP_SPLASH: u32 = 31;
/// The server's load setup: no new players, no load responses, load state 1, game state 2, one human, not
/// multiplayer.
pub const EE_SCREENS_UG_OP_SERVER_LOAD: u32 = 32;
/// `gGameOptions.mLock->WriteLock()` (a 1) or `WriteUnlock()` (a 0).
pub const EE_SCREENS_UG_OP_OPTIONS_LOCK: u32 = 33;
/// `gGameOptions.SetDefaults()`.
pub const EE_SCREENS_UG_OP_OPTIONS_DEFAULTS: u32 = 34;
/// File a's name has `gScenarioExtension` (`Find(extension, 0) < GetLength()`).
pub const EE_SCREENS_UG_OP_FILE_IS_SCENARIO: u32 = 35;
/// `gGameOptions`' load type b and map type file a's name.
pub const EE_SCREENS_UG_OP_LOAD_OPTIONS: u32 = 36;
/// `gDataPreLoading->PostLoadGame(a != 0, b != 0)`.
pub const EE_SCREENS_UG_OP_POST_LOAD: u32 = 37;
/// `IDS_SAVE_GAME_ISNT_VALID` shown by `gUIManager->DisplayMessage(text, true, NULL, NULL, true)`.
pub const EE_SCREENS_UG_OP_INVALID_SAVE: u32 = 38;
/// `delete` file a (an `FSFile*`; none for 0).
pub const EE_SCREENS_UG_OP_DELETE_FILE: u32 = 39;
/// `gISMouse->Resume()`, `gISKeyboard->Resume()`.
pub const EE_SCREENS_UG_OP_INPUT_RESUME: u32 = 40;
/// `TSThread::SetThreadName(PlatCurrentThreadId(), gUserInterfaceThreadName)`.
pub const EE_SCREENS_UG_OP_THREAD_NAME: u32 = 41;
/// The locales: `IDS_ENGLISH` for all, "English" numbers, the "C" character classes (decision D6).
pub const EE_SCREENS_UG_OP_LOCALE: u32 = 42;
/// `CreateRenderObject()`: its result.
pub const EE_SCREENS_UG_OP_CREATE_RENDER_OBJECT: u32 = 43;
/// `gMousePointer->mThread->Post(a, the primary surface (0x405) or NULL, NULL, true)`.
pub const EE_SCREENS_UG_OP_POINTER_POST: u32 = 44;
/// `gGraphics->PostSetFlag54(false)`, `PostResume()`.
pub const EE_SCREENS_UG_OP_GRAPHICS_START: u32 = 45;
/// The FPU: a 0 `UMSetFloatingPointPrecision(kFPPrecision53)`; a 1 the precision and rounding put back when they
/// changed.
pub const EE_SCREENS_UG_OP_FPU: u32 = 46;
/// `mIsQuitting`.
pub const EE_SCREENS_UG_OP_QUITTING: u32 = 47;
/// `mThread->LockMessageQueue()` (a 1) or `UnlockMessageQueue()` (a 0).
pub const EE_SCREENS_UG_OP_QUEUE_LOCK: u32 = 48;
/// `mThread->Receive(message, parameter 1, parameter 2)`: its result; the three into `c` (an `[i64; 3]`).
pub const EE_SCREENS_UG_OP_RECEIVE: u32 = 49;
/// `HandleMessage(a, b, c)`.
pub const EE_SCREENS_UG_OP_HANDLE: u32 = 50;
/// `mIsUpdating != 0`.
pub const EE_SCREENS_UG_OP_UPDATING: u32 = 51;
/// `UpdateInterface()`.
pub const EE_SCREENS_UG_OP_UPDATE_INTERFACE: u32 = 52;
/// `gFreeCamera`.
pub const EE_SCREENS_UG_OP_FREE_CAMERA: u32 = 53;
/// `TMGetTime()`.
pub const EE_SCREENS_UG_OP_TIME: u32 = 54;
/// `gWorld.mCameraControl.Update(f)`, f the float of a's bits.
pub const EE_SCREENS_UG_OP_CAMERA: u32 = 55;
/// `gPlatSimRun`.
pub const EE_SCREENS_UG_OP_SIM_RUN: u32 = 56;

/// Player calls (EE_SCREENS_UG_OP_PLAYER's b): there is player a; its `RegisterHerds()`, `AssignHerds()`; its
/// `RegisterTowns()`, `AssignTowns()`; it has a computer player; that one's `RefreshResourceCenters()`; its
/// `RefreshTownGroups()`.
pub const EE_SCREENS_UG_PLAYER_EXISTS: i64 = 0;
pub const EE_SCREENS_UG_PLAYER_HERDS: i64 = 1;
pub const EE_SCREENS_UG_PLAYER_TOWNS: i64 = 2;
pub const EE_SCREENS_UG_PLAYER_COMPUTER: i64 = 3;
pub const EE_SCREENS_UG_PLAYER_CENTRES: i64 = 4;
pub const EE_SCREENS_UG_PLAYER_TOWN_GROUPS: i64 = 5;

/// The physics thread's start message (0x404) and its stop of the timer (0x402); the mouse pointer thread's surface
/// (0x405) and start (0x404) messages.
const PHYSICS_START: i64 = 0x404;
const PHYSICS_STOP: i64 = 0x402;
const POINTER_SURFACE: i64 = 0x405;
const POINTER_START: i64 = 0x404;

fn op(h: &mut impl UiHost, op: u32, a: i64) -> i64 {
    h.op(op, a, 0, 0)
}

fn player(h: &mut impl UiHost, i: u32, call: i64) -> bool {
    h.op(EE_SCREENS_UG_OP_PLAYER, i as i64, call, 0) != 0
}

/// The restart of the world shared by the two scenario tests: the herds, the towns, the paths, the AI centres
/// (or town groups), sight, the units, the land continents.
fn restart_world(h: &mut impl UiHost) {
    if player(h, 0, EE_SCREENS_UG_PLAYER_EXISTS) {
        player(h, 0, EE_SCREENS_UG_PLAYER_HERDS);
    }
    let mut i = 1u32;
    while i < op(h, EE_SCREENS_UG_OP_NUM_PLAYERS, 0) as u32 {
        if player(h, i, EE_SCREENS_UG_PLAYER_EXISTS) {
            player(h, i, EE_SCREENS_UG_PLAYER_TOWNS);
        }
        i += 1;
    }
    op(h, EE_SCREENS_UG_OP_PATH_RESET, 0);
    let mut j = 1u32;
    while j < op(h, EE_SCREENS_UG_OP_NUM_PLAYERS, 0) as u32 {
        if player(h, j, EE_SCREENS_UG_PLAYER_EXISTS) {
            if player(h, j, EE_SCREENS_UG_PLAYER_COMPUTER) {
                player(h, j, EE_SCREENS_UG_PLAYER_CENTRES);
            } else {
                player(h, j, EE_SCREENS_UG_PLAYER_TOWN_GROUPS);
            }
        }
        j += 1;
    }
    op(h, EE_SCREENS_UG_OP_SIGHT_ON, 0);
    op(h, EE_SCREENS_UG_OP_RESTART_UNITS, 0);
    op(h, EE_SCREENS_UG_OP_LAND_CONTINENTS, 0);
}

/// The game's sounds and threads after a scenario test's start: the ambient sounds, the graphics, the physics, the
/// music.
fn start_game(h: &mut impl UiHost) {
    op(h, EE_SCREENS_UG_OP_AMBIENT, 0);
    op(h, EE_SCREENS_UG_OP_GRAPHICS, 0);
    op(h, EE_SCREENS_UG_OP_PHYSICS_POST, PHYSICS_START);
    op(h, EE_SCREENS_UG_OP_MUSIC, 0);
}

/// `EEUserInterface::TestScenario` (0x276ab0): the edited scenario's world restarted, the main game form shown.
pub fn test_scenario(h: &mut impl UiHost) {
    op(h, EE_SCREENS_UG_OP_GRAPHICS, 1);
    op(h, EE_SCREENS_UG_OP_WORLD_LOCK, 1);
    op(h, EE_SCREENS_UG_OP_SCREEN_ACTIVE, 0);
    op(h, EE_SCREENS_UG_OP_CONTINENTS, 0);
    restart_world(h);
    op(h, EE_SCREENS_UG_OP_WORLD_LOCK, 0);
    h.op(EE_SCREENS_UG_OP_RESIZE_GAME, 1, 1, 0);
    op(h, EE_SCREENS_UG_OP_SWAP_MAIN, 0);
    op(h, EE_SCREENS_UG_OP_WORLD_LOCK, 1);
    op(h, EE_SCREENS_UG_OP_MINI_MAP, 0);
    op(h, EE_SCREENS_UG_OP_WORLD_LOCK, 0);
    op(h, EE_SCREENS_UG_OP_GAME_VIEW, 0);
    start_game(h);
}

/// `EEUserInterface::SaveAndTestScenario` (0x276cbc): the edited scenario saved as ScenDefault (a failed save
/// reported) and started as a game like `TestScenario`.
pub fn save_and_test_scenario(h: &mut impl UiHost) {
    op(h, EE_SCREENS_UG_OP_GRAPHICS, 1);
    op(h, EE_SCREENS_UG_OP_WORLD_LOCK, 1);
    op(h, EE_SCREENS_UG_OP_SCREEN_ACTIVE, 0);
    op(h, EE_SCREENS_UG_OP_PREPARE_SAVE, 0);
    op(h, EE_SCREENS_UG_OP_CONTINENTS, 0);
    if op(h, EE_SCREENS_UG_OP_SAVE_SCENARIO, 0) as i32 != 0 {
        op(h, EE_SCREENS_UG_OP_SAVE_ERROR, 0);
    }
    op(h, EE_SCREENS_UG_OP_SCENARIO_OPTIONS, 0);
    op(h, EE_SCREENS_UG_OP_MAX_PLAYERS, 0);
    op(h, EE_SCREENS_UG_OP_COLLECT_COMPUTERS, 0);
    restart_world(h);
    op(h, EE_SCREENS_UG_OP_WORLD_LOCK, 0);
    h.op(EE_SCREENS_UG_OP_RESIZE_GAME, 0, 1, 0);
    op(h, EE_SCREENS_UG_OP_WORLD_LOCK, 1);
    op(h, EE_SCREENS_UG_OP_SWAP_MAIN, 0);
    op(h, EE_SCREENS_UG_OP_GAME_VIEW, 0);
    op(h, EE_SCREENS_UG_OP_MINI_MAP, 0);
    op(h, EE_SCREENS_UG_OP_WORLD_LOCK, 0);
    start_game(h);
}

/// `EEUserInterface::LoadSavedGame` (0x27757c) of a file (an `FSFile*`, deleted at the end): with a valid header,
/// the physics' timer stopped, a local server made for the load, the splash form shown, the options' load type
/// (1 a scenario, 2 a saved game) and map, the load posted; else the invalid file's message.
pub fn load_saved_game(h: &mut impl UiHost, file: i64) {
    if op(h, EE_SCREENS_UG_OP_LOAD_HEADER, file) != 0 {
        op(h, EE_SCREENS_UG_OP_GRAPHICS, 1);
        op(h, EE_SCREENS_UG_OP_PHYSICS_POST, PHYSICS_STOP);
        while op(h, EE_SCREENS_UG_OP_TIMER_RUNNING, 0) != 0 {
            op(h, EE_SCREENS_UG_OP_SLEEP, 1);
        }
        op(h, EE_SCREENS_UG_OP_NEW_SERVER, 0);
        op(h, EE_SCREENS_UG_OP_B13D_OFF, 0);
        op(h, EE_SCREENS_UG_OP_STATE_CLEAR, 0);
        op(h, EE_SCREENS_UG_OP_STOP_SOUNDS, 0);
        op(h, EE_SCREENS_UG_OP_SWAP_SPLASH, 0);
        op(h, EE_SCREENS_UG_OP_SERVER_LOAD, 0);
        op(h, EE_SCREENS_UG_OP_OPTIONS_LOCK, 1);
        op(h, EE_SCREENS_UG_OP_OPTIONS_DEFAULTS, 0);
        let load_type = if op(h, EE_SCREENS_UG_OP_FILE_IS_SCENARIO, file) != 0 { 1 } else { 2 };
        h.op(EE_SCREENS_UG_OP_LOAD_OPTIONS, file, load_type, 0);
        op(h, EE_SCREENS_UG_OP_OPTIONS_LOCK, 0);
        h.op(EE_SCREENS_UG_OP_POST_LOAD, 1, 1, 0);
        op(h, EE_SCREENS_UG_OP_GRAPHICS, 0);
    } else {
        op(h, EE_SCREENS_UG_OP_INVALID_SAVE, 0);
    }
    if file != 0 {
        op(h, EE_SCREENS_UG_OP_DELETE_FILE, file);
    }
}

/// The FPU's precision and rounding put back (the loop's check after each receive and each pass).
fn fpu_check(h: &mut impl UiHost) {
    op(h, EE_SCREENS_UG_OP_FPU, 1);
}

/// `EEUserInterface::Run` (0x2779b8): the UI thread until it quits (its result 0). Each pass takes every queued
/// message under the queue's lock, handles them in order, updates the interface (when updating and not quitting)
/// and the free camera (by the real time since the last pass), then sleeps 1 ms (20 under the headless simulation).
pub fn run(h: &mut impl UiHost) -> i64 {
    // (the original's function static, set on its first call: the thread runs once)
    let mut last = op(h, EE_SCREENS_UG_OP_TIME, 0) as u32;
    op(h, EE_SCREENS_UG_OP_INPUT_RESUME, 0);
    op(h, EE_SCREENS_UG_OP_THREAD_NAME, 0);
    op(h, EE_SCREENS_UG_OP_LOCALE, 0);
    if op(h, EE_SCREENS_UG_OP_CREATE_RENDER_OBJECT, 0) as i32 != 0 {
        return 0;
    }
    op(h, EE_SCREENS_UG_OP_POINTER_POST, POINTER_SURFACE);
    op(h, EE_SCREENS_UG_OP_POINTER_POST, POINTER_START);
    op(h, EE_SCREENS_UG_OP_GRAPHICS_START, 0);
    op(h, EE_SCREENS_UG_OP_FPU, 0);
    let mut messages: Vec<[i64; 3]> = Vec::new();
    while op(h, EE_SCREENS_UG_OP_QUITTING, 0) == 0 {
        op(h, EE_SCREENS_UG_OP_QUEUE_LOCK, 1);
        loop {
            let mut message = [0i64; 3];
            let result = h.op(EE_SCREENS_UG_OP_RECEIVE, 0, 0, message.as_mut_ptr() as i64) as i32;
            fpu_check(h);
            if result != 0 {
                break;
            }
            messages.push(message);
            op(h, EE_SCREENS_UG_OP_SLEEP, 1);
        }
        op(h, EE_SCREENS_UG_OP_QUEUE_LOCK, 0);
        for m in &messages {
            h.op(EE_SCREENS_UG_OP_HANDLE, m[0], m[1], m[2]);
        }
        messages.clear();
        fpu_check(h);
        if op(h, EE_SCREENS_UG_OP_UPDATING, 0) != 0 && op(h, EE_SCREENS_UG_OP_QUITTING, 0) == 0 {
            op(h, EE_SCREENS_UG_OP_UPDATE_INTERFACE, 0);
        }
        if op(h, EE_SCREENS_UG_OP_FREE_CAMERA, 0) == 0 {
            last = op(h, EE_SCREENS_UG_OP_TIME, 0) as u32;
        } else {
            let time = op(h, EE_SCREENS_UG_OP_TIME, 0) as u32;
            let seconds = time.wrapping_sub(last) as f32 * 0.001;
            op(h, EE_SCREENS_UG_OP_CAMERA, seconds.to_bits() as i64);
            last = time;
        }
        let pause = if op(h, EE_SCREENS_UG_OP_SIM_RUN, 0) != 0 { 20 } else { 1 };
        op(h, EE_SCREENS_UG_OP_SLEEP, pause);
    }
    0
}

// ---- leaving a map, the editor's entry and exit, the restart, the civilization builder (round 28)

/// `gGraphics->PostReleaseScene()`, `PostMessage50AAndWait()`.
pub const EE_SCREENS_UG_OP_RELEASE_SCENE: u32 = 57;
/// `ee_screens_actions_clear(a)` (the action queues: 0 the actions, 1 undo, 2 redo).
pub const EE_SCREENS_UG_OP_ACTIONS_CLEAR: u32 = 58;
/// `gDataPreLoading->WaitForMessageQueue()`.
pub const EE_SCREENS_UG_OP_WAIT_PRELOAD: u32 = 59;
/// `gWorld.Clear()`.
pub const EE_SCREENS_UG_OP_WORLD_CLEAR: u32 = 60;
/// a 0: `gPhysics->PostInit()`; a 1: `gPhysics->RunOwedTicks()`, `Init()`.
pub const EE_SCREENS_UG_OP_PHYSICS_INIT: u32 = 61;
/// `gGameScreenActive = false`.
pub const EE_SCREENS_UG_OP_SCREEN_INACTIVE: u32 = 62;
/// `ResizeGameScreen(EEMenuWidth(), EEMenuHeight(), a != 0, b != 0)`.
pub const EE_SCREENS_UG_OP_RESIZE_MENU: u32 = 63;
/// `gUIManager->GetCurrentForm() != gUIManager->GetMainGameForm()`.
pub const EE_SCREENS_UG_OP_NOT_MAIN: u32 = 64;
/// `gUIManager->Pop()`.
pub const EE_SCREENS_UG_OP_POP: u32 = 65;
/// `gUIManager->SwapForm(form a)` (a `UIForm*`).
pub const EE_SCREENS_UG_OP_SWAP: u32 = 66;
/// `gUIManager->GetFormByID(a)` (a `UIForm*`).
pub const EE_SCREENS_UG_OP_FORM: u32 = 67;
/// `gWorld.mStatistics.GetRecords().size() == 1`.
pub const EE_SCREENS_UG_OP_ONE_RECORD: u32 = 68;
/// `gUIManager->Push(form a)`.
pub const EE_SCREENS_UG_OP_PUSH: u32 = 69;
/// The communications are a client's that lost its server after the loading.
pub const EE_SCREENS_UG_OP_SERVER_LOST: u32 = 70;
/// `EEUnloadGameMedia()`.
pub const EE_SCREENS_UG_OP_UNLOAD_MEDIA: u32 = 71;
/// With communications: they are deleted, a new `EEClient` made and its threads started.
pub const EE_SCREENS_UG_OP_NEW_CLIENT: u32 = 72;
/// `EEIsLobbyOpen()`.
pub const EE_SCREENS_UG_OP_LOBBY_OPEN: u32 = 73;
/// `EESetQuitFlag()`.
pub const EE_SCREENS_UG_OP_QUIT_FLAG: u32 = 74;
/// A new `EEABootClientFromCanceledGame(a, true)` posted (`PostAction`) and released.
pub const EE_SCREENS_UG_OP_BOOT_CLIENT: u32 = 75;
/// `gUIManager->CreateScenarioEditor()`.
pub const EE_SCREENS_UG_OP_CREATE_EDITOR: u32 = 76;
/// `EELoadSavedGameFile("ScenDefault" + gScenarioExtension, true, true, "data\\Scenarios\\", 0)`: its result.
pub const EE_SCREENS_UG_OP_LOAD_SCENDEFAULT: u32 = 77;
/// `gSoundManager->StopAllOneShotSounds()`.
pub const EE_SCREENS_UG_OP_STOP_ONE_SHOTS: u32 = 78;
/// `gUIManager->SwapForm(gUIManager->mScenarioEditorForm)`.
pub const EE_SCREENS_UG_OP_SWAP_EDITOR: u32 = 79;
/// `IDS_THERE_WAS_ERROR_TRYING_TO_LOAD` shown by `gUIManager->DisplayMessage(text, true, NULL, NULL, true)`.
pub const EE_SCREENS_UG_OP_LOAD_ERROR: u32 = 80;
/// The editor's pauses: the server's `PauseGame()`, the music's `Pause()`, the ambient sounds' `StopSounds()` and
/// `Pause()`, the chat log's `EEUIECLEFormManager::Clear()`.
pub const EE_SCREENS_UG_OP_EDITOR_PAUSES: u32 = 81;
/// `gGraphics->mIsPaused`.
pub const EE_SCREENS_UG_OP_GRAPHICS_PAUSED: u32 = 82;
/// The civilization builder form's music: a 0 kept (`GetMusicFileName()`), a 1 none (`SetMusicFileName("")`), a 2
/// the kept one put back.
pub const EE_SCREENS_UG_OP_CB_MUSIC: u32 = 83;
/// `gUIManager->LockUIForCopyData()` (a 1) or `UnlockUIForCopyData()` (a 0).
pub const EE_SCREENS_UG_OP_COPY_LOCK: u32 = 84;
/// `gWorld.mGameOptions.CanBuyCivilization()`.
pub const EE_SCREENS_UG_OP_CAN_BUY_CIV: u32 = 85;
/// `gWorld.mGameOptions.mAllowCustomCivs`.
pub const EE_SCREENS_UG_OP_CUSTOM_CIVS: u32 = 86;
/// The current player's (`gWorld.mPlayers[gWorld.mCurrentPlayer]`) `mCivilizationChosen`.
pub const EE_SCREENS_UG_OP_CIV_CHOSEN: u32 = 87;
/// The predefined civilization's choice: `EESetCivBuilderButtonBlinking(false)`, the player's civilization chosen
/// and locked, the predefined civilization form's open (`EE_SCREENS_PDCS_OPEN`) and close events, the form pushed.
pub const EE_SCREENS_UG_OP_CIV_CHOOSE: u32 = 88;
/// `CivBuilderEditCivilization()`.
pub const EE_SCREENS_UG_OP_CIV_EDIT: u32 = 89;
/// `CivBuilderCreateCivilization((EECivilization*)a, b != 0)`.
pub const EE_SCREENS_UG_OP_CIV_CREATE: u32 = 90;

/// The forms (`kFormSplash`, `kFormPostGameSummary`).
pub const EE_SCREENS_UG_FORM_SPLASH: i64 = 23;
pub const EE_SCREENS_UG_FORM_POST_GAME_SUMMARY: i64 = 25;

/// The members `ee_screens_ui_member` runs (its `which`; a, b, c their arguments).
pub const EE_SCREENS_UG_DO_LEAVE_MAP: u32 = 1;
pub const EE_SCREENS_UG_DO_CANCEL_GAME: u32 = 2;
pub const EE_SCREENS_UG_DO_CLOSE_EDITOR: u32 = 3;
pub const EE_SCREENS_UG_DO_ENTER_EDITOR: u32 = 4;
pub const EE_SCREENS_UG_DO_LOAD_FROM_EDITOR: u32 = 5;
pub const EE_SCREENS_UG_DO_RESTART: u32 = 6;
pub const EE_SCREENS_UG_DO_CIV_BUILDER: u32 = 7;

/// Runs the member `which` (`EE_SCREENS_UG_DO_*`) with its arguments.
pub fn member(h: &mut impl UiHost, which: u32, a: i64, b: i64, c: i64) {
    match which {
        EE_SCREENS_UG_DO_LEAVE_MAP => leave_map(h, a, b != 0),
        EE_SCREENS_UG_DO_CANCEL_GAME => cancel_game(h),
        EE_SCREENS_UG_DO_CLOSE_EDITOR => close_scenario_editor(h),
        EE_SCREENS_UG_DO_ENTER_EDITOR => enter_scenario_editor(h, a != 0),
        EE_SCREENS_UG_DO_LOAD_FROM_EDITOR => start_loading_from_editor(h),
        EE_SCREENS_UG_DO_RESTART => restart_game(h),
        EE_SCREENS_UG_DO_CIV_BUILDER => show_civilization_builder(h, a, b != 0, c != 0),
        _ => {}
    }
}

/// The physics' timer stopped (message 0x402) and waited for.
fn stop_physics(h: &mut impl UiHost) {
    op(h, EE_SCREENS_UG_OP_PHYSICS_POST, PHYSICS_STOP);
    while op(h, EE_SCREENS_UG_OP_TIMER_RUNNING, 0) != 0 {
        op(h, EE_SCREENS_UG_OP_SLEEP, 1);
    }
}

/// `EEUserInterface::LeaveMap` (0x277023) back to a form (a `UIForm*`, 0 for none): the scene released, the action
/// queues, the world and the game state cleared under the world's lock, the physics reset, the menu's size when
/// asked; then back to the main game form and the form swapped in (the post-game summary of a one-record game only
/// popped), or one form popped.
pub fn leave_map(h: &mut impl UiHost, form: i64, resize: bool) {
    op(h, EE_SCREENS_UG_OP_RELEASE_SCENE, 0);
    for queue in 0..3 {
        op(h, EE_SCREENS_UG_OP_ACTIONS_CLEAR, queue);
    }
    op(h, EE_SCREENS_UG_OP_WORLD_LOCK, 1);
    op(h, EE_SCREENS_UG_OP_WAIT_PRELOAD, 0);
    op(h, EE_SCREENS_UG_OP_WORLD_CLEAR, 0);
    op(h, EE_SCREENS_UG_OP_STATE_CLEAR, 0);
    op(h, EE_SCREENS_UG_OP_PHYSICS_INIT, 0);
    op(h, EE_SCREENS_UG_OP_SCREEN_INACTIVE, 0);
    op(h, EE_SCREENS_UG_OP_WORLD_LOCK, 0);
    if resize {
        h.op(EE_SCREENS_UG_OP_RESIZE_MENU, 1, 0, 0);
    }
    if form != 0 {
        while op(h, EE_SCREENS_UG_OP_NOT_MAIN, 0) != 0 {
            op(h, EE_SCREENS_UG_OP_POP, 0);
        }
        if form == op(h, EE_SCREENS_UG_OP_FORM, EE_SCREENS_UG_FORM_POST_GAME_SUMMARY)
            && op(h, EE_SCREENS_UG_OP_ONE_RECORD, 0) != 0
        {
            op(h, EE_SCREENS_UG_OP_POP, 0);
        } else {
            op(h, EE_SCREENS_UG_OP_SWAP, form);
        }
    } else {
        op(h, EE_SCREENS_UG_OP_POP, 0);
    }
    op(h, EE_SCREENS_UG_OP_B13D_OFF, 0);
}

/// `EEUserInterface::CancelGame` (0x277170): the physics stopped, the game's media unloaded, a fresh client; a
/// client whose server was lost leaves to no form (quitting an open lobby) and is booted, others leave to the
/// post-game summary.
pub fn cancel_game(h: &mut impl UiHost) {
    let lost = op(h, EE_SCREENS_UG_OP_SERVER_LOST, 0) != 0;
    op(h, EE_SCREENS_UG_OP_GRAPHICS, 1);
    stop_physics(h);
    op(h, EE_SCREENS_UG_OP_UNLOAD_MEDIA, 0);
    op(h, EE_SCREENS_UG_OP_NEW_CLIENT, 0);
    if lost {
        let lobby = op(h, EE_SCREENS_UG_OP_LOBBY_OPEN, 0) != 0;
        if lobby {
            op(h, EE_SCREENS_UG_OP_QUIT_FLAG, 0);
        }
        leave_map(h, 0, true);
        op(h, EE_SCREENS_UG_OP_BOOT_CLIENT, if lobby { 0 } else { EE_SCREENS_UG_IDS_HOST_GONE });
    } else {
        let summary = op(h, EE_SCREENS_UG_OP_FORM, EE_SCREENS_UG_FORM_POST_GAME_SUMMARY);
        leave_map(h, summary, true);
    }
    op(h, EE_SCREENS_UG_OP_GRAPHICS, 0);
}

/// `IDS_HOST_OF_THIS_GAME_HAS_GONE`.
pub const EE_SCREENS_UG_IDS_HOST_GONE: i64 = 10011;

/// `EEUserInterface::CloseScenarioEditor` (0x277290): the media unloaded, the physics stopped, a fresh client, the
/// map left.
pub fn close_scenario_editor(h: &mut impl UiHost) {
    op(h, EE_SCREENS_UG_OP_GRAPHICS, 1);
    op(h, EE_SCREENS_UG_OP_UNLOAD_MEDIA, 0);
    stop_physics(h);
    op(h, EE_SCREENS_UG_OP_NEW_CLIENT, 0);
    leave_map(h, 0, true);
    op(h, EE_SCREENS_UG_OP_GRAPHICS, 0);
}

/// `EEUserInterface::EnterScenarioEditor` (0x277315): the editor form made and swapped in at the menu's size
/// (ScenDefault reloaded when asked, a failed load reported; else the one-shot sounds stopped), the mini map
/// redrawn, the game, music and ambient sounds paused, the graphics resumed when paused.
pub fn enter_scenario_editor(h: &mut impl UiHost, load: bool) {
    let mut result = 0;
    op(h, EE_SCREENS_UG_OP_GRAPHICS, 1);
    op(h, EE_SCREENS_UG_OP_CREATE_EDITOR, 0);
    h.op(EE_SCREENS_UG_OP_RESIZE_MENU, 0, 1, 0);
    op(h, EE_SCREENS_UG_OP_WORLD_LOCK, 1);
    op(h, EE_SCREENS_UG_OP_SCREEN_INACTIVE, 0);
    if load {
        op(h, EE_SCREENS_UG_OP_PHYSICS_INIT, 1);
        result = op(h, EE_SCREENS_UG_OP_LOAD_SCENDEFAULT, 0) as i32;
    } else {
        op(h, EE_SCREENS_UG_OP_STOP_ONE_SHOTS, 0);
    }
    if op(h, EE_SCREENS_UG_OP_NOT_MAIN, 0) != 0 {
        op(h, EE_SCREENS_UG_OP_POP, 0);
    }
    op(h, EE_SCREENS_UG_OP_SWAP_EDITOR, 0);
    if result != 0 {
        op(h, EE_SCREENS_UG_OP_LOAD_ERROR, 0);
    }
    op(h, EE_SCREENS_UG_OP_MINI_MAP, 0);
    op(h, EE_SCREENS_UG_OP_WORLD_LOCK, 0);
    op(h, EE_SCREENS_UG_OP_EDITOR_PAUSES, 0);
    if op(h, EE_SCREENS_UG_OP_GRAPHICS_PAUSED, 0) != 0 {
        op(h, EE_SCREENS_UG_OP_GRAPHICS, 0);
    }
}

/// `EEUserInterface::StartLoadingFromEditor` (0x2777cd): the menu's size, the splash form pushed, the game screen
/// off, the edited scenario's load posted.
pub fn start_loading_from_editor(h: &mut impl UiHost) {
    op(h, EE_SCREENS_UG_OP_GRAPHICS, 1);
    h.op(EE_SCREENS_UG_OP_RESIZE_MENU, 1, 1, 0);
    let splash = op(h, EE_SCREENS_UG_OP_FORM, EE_SCREENS_UG_FORM_SPLASH);
    op(h, EE_SCREENS_UG_OP_PUSH, splash);
    op(h, EE_SCREENS_UG_OP_WORLD_LOCK, 1);
    op(h, EE_SCREENS_UG_OP_SCREEN_INACTIVE, 0);
    op(h, EE_SCREENS_UG_OP_WORLD_LOCK, 0);
    h.op(EE_SCREENS_UG_OP_POST_LOAD, 0, 0, 0);
    op(h, EE_SCREENS_UG_OP_GRAPHICS, 0);
}

/// `EEUserInterface::RestartGame` (0x279013): the physics stopped, a fresh local server, the map left to the splash
/// form, the server set up for the load, the game reloaded.
pub fn restart_game(h: &mut impl UiHost) {
    op(h, EE_SCREENS_UG_OP_GRAPHICS, 1);
    stop_physics(h);
    op(h, EE_SCREENS_UG_OP_NEW_SERVER, 0);
    let splash = op(h, EE_SCREENS_UG_OP_FORM, EE_SCREENS_UG_FORM_SPLASH);
    leave_map(h, splash, false);
    op(h, EE_SCREENS_UG_OP_SCREEN_ACTIVE, 0);
    op(h, EE_SCREENS_UG_OP_SERVER_LOAD, 0);
    h.op(EE_SCREENS_UG_OP_POST_LOAD, 1, 0, 0);
    op(h, EE_SCREENS_UG_OP_GRAPHICS, 0);
}

/// `EEUserInterface::ShowCivilizationBuilder` (0x277c1f) of a civilization (an `EECivilization*`) or the player's
/// own: under the copy and world locks, the builder's music off for either flag; the player's (`edit`) is the
/// predefined civilization's choice when one can be bought, custom ones are not allowed and none was chosen, else
/// the edit of the player's; otherwise the builder for `civ`. The music put back.
pub fn show_civilization_builder(h: &mut impl UiHost, civ: i64, flag: bool, edit: bool) {
    op(h, EE_SCREENS_UG_OP_CB_MUSIC, 0);
    op(h, EE_SCREENS_UG_OP_GRAPHICS, 1);
    op(h, EE_SCREENS_UG_OP_COPY_LOCK, 1);
    op(h, EE_SCREENS_UG_OP_WORLD_LOCK, 1);
    if flag || edit {
        op(h, EE_SCREENS_UG_OP_CB_MUSIC, 1);
    }
    if edit {
        if op(h, EE_SCREENS_UG_OP_CAN_BUY_CIV, 0) != 0
            && op(h, EE_SCREENS_UG_OP_CUSTOM_CIVS, 0) == 0
            && op(h, EE_SCREENS_UG_OP_CIV_CHOSEN, 0) == 0
        {
            op(h, EE_SCREENS_UG_OP_CIV_CHOOSE, 0);
        } else {
            op(h, EE_SCREENS_UG_OP_CIV_EDIT, 0);
        }
    } else {
        h.op(EE_SCREENS_UG_OP_CIV_CREATE, civ, flag as i64, 0);
    }
    op(h, EE_SCREENS_UG_OP_CB_MUSIC, 2);
    op(h, EE_SCREENS_UG_OP_WORLD_LOCK, 0);
    op(h, EE_SCREENS_UG_OP_COPY_LOCK, 0);
    op(h, EE_SCREENS_UG_OP_GRAPHICS, 0);
}

#[cfg(test)]
mod tests;
