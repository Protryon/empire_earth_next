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

#[cfg(test)]
mod tests;
