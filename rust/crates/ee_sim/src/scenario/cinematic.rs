//! The manager's cinematics and dialog round trips (the scenario editor lane's round 3, from EEScenarioManager.cpp):
//! `EndCinematic`, `StartCinematic`, `ExitCinematic`, the round's shown message, hidden messages and human test, the
//! dropped messages, `ApplyPendingEffects`, `Prepare`'s and the destructor's media unload, `ReplaceUnit`; over the
//! round host's operations (`EE_SCENARIO_ROUND_*`, each one C++ statement in `RoundOp`), in the C++'s order.

use std::os::raw::{c_long, c_ulong};

use crate::scenario::fields::EeScenarioTriggerFields;
use crate::scenario::ftol;
use crate::scenario::round::*;

// ---- the statements (on the manager `m`, the world, the camera `c`)
/// `gSoundManager->SetLatency(a)`.
pub const EE_SCENARIO_ROUND_LATENCY: u32 = 20;
/// Trigger `a`'s record (`EeScenarioTriggerFields*`).
pub const EE_SCENARIO_ROUND_TRIGGER_FIELDS: u32 = 21;
/// `gUserInterface->PostHideMessages()`; `gSoundManager->StopAllOneShotSounds()`.
pub const EE_SCENARIO_ROUND_POST_HIDE: u32 = 22;
pub const EE_SCENARIO_ROUND_STOP_SOUNDS: u32 = 23;
/// `m->mMessages.erase(begin + a)`.
pub const EE_SCENARIO_ROUND_MESSAGE_ERASE: u32 = 24;
/// `gWorld.mNumPlayers`, `GetPlayer(a) != NULL`, its `DelayWonderTimers(b)`, `IsHuman()`, `mObserverMode`,
/// `UnselectAllUnits()`, `mSelectedGroup->CopySelection(mActiveGroup)`.
pub const EE_SCENARIO_ROUND_NUM_PLAYERS: u32 = 25;
pub const EE_SCENARIO_ROUND_PLAYER_EXISTS: u32 = 26;
pub const EE_SCENARIO_ROUND_DELAY_WONDERS: u32 = 27;
pub const EE_SCENARIO_ROUND_PLAYER_HUMAN: u32 = 28;
pub const EE_SCENARIO_ROUND_PLAYER_OBSERVER: u32 = 29;
pub const EE_SCENARIO_ROUND_UNSELECT: u32 = 30;
pub const EE_SCENARIO_ROUND_COPY_SELECTION: u32 = 31;
/// `gGraphics->PostLetterboxOut(3000)`, `PostLetterboxIn(3000)`, `PostFadeIn(0)`.
pub const EE_SCENARIO_ROUND_LETTERBOX_OUT: u32 = 32;
pub const EE_SCENARIO_ROUND_LETTERBOX_IN: u32 = 33;
pub const EE_SCENARIO_ROUND_FADE_IN: u32 = 34;
/// The camera: `ResetPath()`, `ClearUnitB()`, `ClearUnitA()`, `mTurnYaw = 0`, `SetMode(a)`, `SetZoom(m->mSavedZoom)`,
/// `SetOffset(m->mK008.mOffset, true)`, `SetLocation(m->mK008.mFocus, true, false)`, `SetViewTurn(m->mK008.mT00,
/// true)`, `SetOffsetTurn(m->mK008.mT40, true)`, `SetChanged()`, `Update(0)`; `GetOffset(mFraction,
/// m->mK008.mOffset)`, `GetLocation`, `GetViewTurn`, `GetOffsetTurn` (into the key), `mMode`, `GetZoom()` (bits).
pub const EE_SCENARIO_ROUND_CAMERA_RESET_PATH: u32 = 35;
pub const EE_SCENARIO_ROUND_CAMERA_CLEAR_B: u32 = 36;
pub const EE_SCENARIO_ROUND_CAMERA_CLEAR_A: u32 = 37;
pub const EE_SCENARIO_ROUND_CAMERA_TURN_RESET: u32 = 38;
pub const EE_SCENARIO_ROUND_CAMERA_SET_MODE: u32 = 39;
pub const EE_SCENARIO_ROUND_CAMERA_SET_ZOOM: u32 = 40;
pub const EE_SCENARIO_ROUND_CAMERA_SET_OFFSET: u32 = 41;
pub const EE_SCENARIO_ROUND_CAMERA_SET_LOCATION: u32 = 42;
pub const EE_SCENARIO_ROUND_CAMERA_SET_VIEW_TURN: u32 = 43;
pub const EE_SCENARIO_ROUND_CAMERA_SET_OFFSET_TURN: u32 = 44;
pub const EE_SCENARIO_ROUND_CAMERA_SET_CHANGED: u32 = 45;
pub const EE_SCENARIO_ROUND_CAMERA_UPDATE: u32 = 46;
pub const EE_SCENARIO_ROUND_CAMERA_GET_OFFSET: u32 = 47;
pub const EE_SCENARIO_ROUND_CAMERA_GET_LOCATION: u32 = 48;
pub const EE_SCENARIO_ROUND_CAMERA_GET_VIEW_TURN: u32 = 49;
pub const EE_SCENARIO_ROUND_CAMERA_GET_OFFSET_TURN: u32 = 50;
pub const EE_SCENARIO_ROUND_CAMERA_MODE: u32 = 51;
pub const EE_SCENARIO_ROUND_CAMERA_ZOOM: u32 = 52;
/// `gWorld.mInCinematic = a != 0`; `gWorld.mSavedGameSpeed = gWorld.GetGameSpeed()`; `gWorld.SetGameSpeed(
/// gWorld.mSavedGameSpeed)`; `gUserInterface->CloseForm(the main game form)`; the main game form's `SetUIState(a)`,
/// `mIsCursorLocked = a != 0`; `gMouse->mPointer = a`; the mouse at the window's centre.
pub const EE_SCENARIO_ROUND_SET_IN_CINEMATIC: u32 = 53;
pub const EE_SCENARIO_ROUND_SAVE_GAME_SPEED: u32 = 54;
pub const EE_SCENARIO_ROUND_RESTORE_GAME_SPEED: u32 = 55;
pub const EE_SCENARIO_ROUND_CLOSE_MAIN_FORM: u32 = 56;
pub const EE_SCENARIO_ROUND_FORM_UI_STATE: u32 = 57;
pub const EE_SCENARIO_ROUND_FORM_CURSOR_LOCK: u32 = 58;
pub const EE_SCENARIO_ROUND_MOUSE_POINTER: u32 = 59;
pub const EE_SCENARIO_ROUND_MOUSE_CENTER: u32 = 60;
/// Dialog message `a`: its sound's length `> 0`, `gUserInterface->PostDialogMessage(it)`, its unit (`gWorld.GetUnit(
/// mUnitID) != NULL`), `mScrollSpeed`, the camera's `MoveTo` its unit at the scroll speed (`_FACING`: turned to face
/// it, scroll speed 0).
pub const EE_SCENARIO_ROUND_MESSAGE_VOICED: u32 = 61;
pub const EE_SCENARIO_ROUND_POST_DIALOG: u32 = 62;
pub const EE_SCENARIO_ROUND_MESSAGE_UNIT: u32 = 63;
pub const EE_SCENARIO_ROUND_MESSAGE_SCROLL: u32 = 64;
pub const EE_SCENARIO_ROUND_MESSAGE_MOVE_TO: u32 = 65;
pub const EE_SCENARIO_ROUND_MESSAGE_MOVE_TO_FACING: u32 = 66;
/// `mPendingEffects.size()`, `mPendingEffects[a] != NULL`, its `Execute(0)`.
pub const EE_SCENARIO_ROUND_PENDING_COUNT: u32 = 67;
pub const EE_SCENARIO_ROUND_PENDING_EXISTS: u32 = 68;
pub const EE_SCENARIO_ROUND_PENDING_EXECUTE: u32 = 69;
/// `mUnitClassMedia.size()`, entry `a`'s upgrade's `UnloadMedia(its player)`, its trace line (`Prepare`'s),
/// `mUnitClassMedia.clear()`.
pub const EE_SCENARIO_ROUND_MEDIA_COUNT: u32 = 70;
pub const EE_SCENARIO_ROUND_MEDIA_UNLOAD: u32 = 71;
pub const EE_SCENARIO_ROUND_MEDIA_TRACE: u32 = 72;
pub const EE_SCENARIO_ROUND_MEDIA_CLEAR: u32 = 73;
/// `mObjects.size()`, object `a`'s `ReplaceUnit(b >> 32, (uint32_t)b)` (both as `long`s).
pub const EE_SCENARIO_ROUND_OBJECT_COUNT: u32 = 74;
pub const EE_SCENARIO_ROUND_OBJECT_REPLACE: u32 = 75;

/// The cinematic's end delay past its length (ms).
const CINEMATIC_DELAY: c_long = 3000;

fn trigger_fields<H: Round>(h: &mut H, t: i64) -> *mut EeScenarioTriggerFields {
    h.op(EE_SCENARIO_ROUND_TRIGGER_FIELDS, t, 0, 0.0) as usize as *mut EeScenarioTriggerFields
}

/// `EESMDropMessages`: the dialog messages of a cinematic (`cinematic`) or the others dropped, the last first.
fn drop_messages<H: Round>(h: &mut H, cinematic: bool) {
    let mut j = h.op(EE_SCENARIO_ROUND_MESSAGE_COUNT, 0, 0, 0.0) as c_long - 1;
    while j >= 0 {
        if (h.op(EE_SCENARIO_ROUND_MESSAGE_CINEMATIC, j as i64, 0, 0.0) != 0) == cinematic {
            h.op(EE_SCENARIO_ROUND_MESSAGE_ERASE, j as i64, 0, 0.0);
        }
        j -= 1;
    }
}

/// `EEScenarioManager::EndCinematic`: the other timed effects and trigger timers delayed by the cinematic's length
/// and 3 s, its unrun effects and dialog messages dropped, the wonder timers delayed and the camera StartCinematic
/// saved restored.
pub fn end_cinematic<H: Round>(h: &mut H) {
    let elapsed = h.fields(|f| f.mTime - f.mCinematicStartTime);
    if h.op(EE_SCENARIO_ROUND_IN_CINEMATIC, 0, 0, 0.0) == 0 || h.fields(|f| f.mIsEndingCinematic) {
        return;
    }
    h.op(EE_SCENARIO_ROUND_LATENCY, 100, 0, 0.0);
    let shift = ftol(elapsed).wrapping_add(CINEMATIC_DELAY);
    h.queue(|q| q.end_cinematic(shift));
    let triggers = h.op(EE_SCENARIO_ROUND_TRIGGER_COUNT, 0, 0, 0.0) as c_ulong;
    for t in 0..triggers {
        let f = trigger_fields(h, t as i64);
        // SAFETY: the live trigger's record, no callback between the read and the write
        unsafe {
            if !(*f).mIsInCinematic && (*f).mConditionDuration > 0 {
                (*f).mConditionStartTime = (*f)
                    .mConditionStartTime
                    .wrapping_add(ftol(elapsed) as c_ulong)
                    .wrapping_add(CINEMATIC_DELAY as c_ulong);
            }
        }
    }
    h.op(EE_SCENARIO_ROUND_POST_HIDE, 0, 0, 0.0);
    drop_messages(h, true);
    let players = h.op(EE_SCENARIO_ROUND_NUM_PLAYERS, 0, 0, 0.0) as c_ulong;
    for i in 0..players {
        if h.op(EE_SCENARIO_ROUND_PLAYER_EXISTS, i as i64, 0, 0.0) != 0 {
            h.op(EE_SCENARIO_ROUND_DELAY_WONDERS, i as i64, ftol(elapsed) as i64, 0.0);
        }
    }
    h.fields(|f| f.mCinematicStartTime = 0.0);
    h.op(EE_SCENARIO_ROUND_LETTERBOX_OUT, 0, 0, 0.0);
    h.fields(|f| f.mIsEndingCinematic = true);
    for op in [
        EE_SCENARIO_ROUND_CAMERA_RESET_PATH,
        EE_SCENARIO_ROUND_CAMERA_CLEAR_B,
        EE_SCENARIO_ROUND_CAMERA_CLEAR_A,
        EE_SCENARIO_ROUND_CAMERA_TURN_RESET,
    ] {
        h.op(op, 0, 0, 0.0);
    }
    let mode = h.fields(|f| f.mSavedCameraMode);
    h.op(EE_SCENARIO_ROUND_CAMERA_SET_MODE, mode as i64, 0, 0.0);
    h.op(EE_SCENARIO_ROUND_CAMERA_SET_ZOOM, 0, 0, 0.0);
    for op in [
        EE_SCENARIO_ROUND_CAMERA_SET_OFFSET,
        EE_SCENARIO_ROUND_CAMERA_SET_LOCATION,
        EE_SCENARIO_ROUND_CAMERA_SET_VIEW_TURN,
        EE_SCENARIO_ROUND_CAMERA_SET_OFFSET_TURN,
        EE_SCENARIO_ROUND_CAMERA_SET_CHANGED,
        EE_SCENARIO_ROUND_CAMERA_UPDATE,
    ] {
        h.op(op, 0, 0, 0.0);
    }
}

/// `EEScenarioManager::ExitCinematic`: once the end's letterbox is gone, the game speed, the main game form and the
/// mouse back, faded in, the one-shot sounds stopped.
pub fn exit_cinematic<H: Round>(h: &mut H) {
    h.op(EE_SCENARIO_ROUND_SET_IN_CINEMATIC, 0, 0, 0.0);
    h.op(EE_SCENARIO_ROUND_RESTORE_GAME_SPEED, 0, 0, 0.0);
    h.op(EE_SCENARIO_ROUND_FORM_UI_STATE, 0, 0, 0.0);
    h.op(EE_SCENARIO_ROUND_FORM_CURSOR_LOCK, 0, 0, 0.0);
    h.op(EE_SCENARIO_ROUND_MOUSE_POINTER, 0, 0, 0.0);
    h.op(EE_SCENARIO_ROUND_MOUSE_CENTER, 0, 0, 0.0);
    h.op(EE_SCENARIO_ROUND_FADE_IN, 0, 0, 0.0);
    h.op(EE_SCENARIO_ROUND_STOP_SOUNDS, 0, 0, 0.0);
}

/// `EEScenarioManager::StartCinematic`: the game speed and camera saved, the main game form closed, the cursor
/// locked, every player's units unselected, the non-cinematic dialog messages dropped, the screen letterboxed.
pub fn start_cinematic<H: Round>(h: &mut H) {
    if h.op(EE_SCENARIO_ROUND_IN_CINEMATIC, 0, 0, 0.0) != 0 {
        return;
    }
    h.op(EE_SCENARIO_ROUND_LATENCY, 300, 0, 0.0);
    h.fields(|f| f.mCinematicStartTime = f.mTime);
    h.op(EE_SCENARIO_ROUND_SET_IN_CINEMATIC, 1, 0, 0.0);
    h.op(EE_SCENARIO_ROUND_SAVE_GAME_SPEED, 0, 0, 0.0);
    h.op(EE_SCENARIO_ROUND_CLOSE_MAIN_FORM, 0, 0, 0.0);
    h.op(EE_SCENARIO_ROUND_STOP_SOUNDS, 0, 0, 0.0);
    for op in [
        EE_SCENARIO_ROUND_CAMERA_GET_OFFSET,
        EE_SCENARIO_ROUND_CAMERA_GET_LOCATION,
        EE_SCENARIO_ROUND_CAMERA_GET_VIEW_TURN,
        EE_SCENARIO_ROUND_CAMERA_GET_OFFSET_TURN,
    ] {
        h.op(op, 0, 0, 0.0);
    }
    let mode = h.op(EE_SCENARIO_ROUND_CAMERA_MODE, 0, 0, 0.0) as c_long;
    h.fields(|f| f.mSavedCameraMode = mode);
    let zoom = f32::from_bits(h.op(EE_SCENARIO_ROUND_CAMERA_ZOOM, 0, 0, 0.0) as u32);
    h.fields(|f| f.mSavedZoom = zoom);
    for op in [EE_SCENARIO_ROUND_CAMERA_RESET_PATH, EE_SCENARIO_ROUND_CAMERA_CLEAR_B, EE_SCENARIO_ROUND_CAMERA_CLEAR_A]
    {
        h.op(op, 0, 0, 0.0);
    }
    h.op(EE_SCENARIO_ROUND_FORM_UI_STATE, 0x11, 0, 0.0);
    h.op(EE_SCENARIO_ROUND_FORM_CURSOR_LOCK, 1, 0, 0.0);
    h.op(EE_SCENARIO_ROUND_MOUSE_POINTER, 6, 0, 0.0);
    let players = h.op(EE_SCENARIO_ROUND_NUM_PLAYERS, 0, 0, 0.0) as c_ulong;
    for i in 1..players {
        h.op(EE_SCENARIO_ROUND_UNSELECT, i as i64, 0, 0.0);
        h.op(EE_SCENARIO_ROUND_COPY_SELECTION, i as i64, 0, 0.0);
    }
    h.op(EE_SCENARIO_ROUND_POST_HIDE, 0, 0, 0.0);
    drop_messages(h, false);
    h.op(EE_SCENARIO_ROUND_LETTERBOX_IN, 0, 0, 0.0);
}

/// `EEScenarioManager::ApplyPendingEffects`: every pending effect of a loaded game run again, the triggers evaluated
/// on the next tick, new pending effects recorded from now on.
pub fn apply_pending<H: Round>(h: &mut H) {
    h.fields(|f| f.mTriggerTimer = TRIGGER_INTERVAL);
    let count = h.op(EE_SCENARIO_ROUND_PENDING_COUNT, 0, 0, 0.0) as c_ulong;
    for i in 0..count {
        if h.op(EE_SCENARIO_ROUND_PENDING_EXISTS, i as i64, 0, 0.0) != 0 {
            h.op(EE_SCENARIO_ROUND_PENDING_EXECUTE, i as i64, 0, 0.0);
        }
    }
    h.fields(|f| f.mB234 = false);
}

/// The media the scenario loaded unloaded (`trace`: `Prepare`'s, which traces each and clears the list; the
/// destructor's does neither).
pub fn unload_media<H: Round>(h: &mut H, prepare: bool) {
    let count = h.op(EE_SCENARIO_ROUND_MEDIA_COUNT, 0, 0, 0.0) as c_ulong;
    for i in 0..count {
        h.op(EE_SCENARIO_ROUND_MEDIA_UNLOAD, i as i64, 0, 0.0);
        if prepare {
            h.op(EE_SCENARIO_ROUND_MEDIA_TRACE, i as i64, 0, 0.0);
        }
    }
    if prepare {
        h.op(EE_SCENARIO_ROUND_MEDIA_CLEAR, 0, 0, 0.0);
    }
}

/// `EEScenarioManager::ReplaceUnit`: a unit id replaced in every object.
pub fn replace_unit<H: Round>(h: &mut H, old: c_long, new: c_long) {
    let count = h.op(EE_SCENARIO_ROUND_OBJECT_COUNT, 0, 0, 0.0) as c_ulong;
    let pair = ((old as i64) << 32) | (new as u32 as i64);
    for i in 0..count {
        h.op(EE_SCENARIO_ROUND_OBJECT_REPLACE, i as i64, pair, 0.0);
    }
}

/// The round's statement sequences composed (for [`EeScenarioRoundHost`]): the shown message, the hidden messages,
/// the human test, the exit; `None` for the C++'s own operations.
pub fn compose<H: Round>(h: &mut H, op: u32, a: i64) -> Option<i64> {
    Some(match op {
        EE_SCENARIO_ROUND_SHOW_MESSAGE => {
            if h.op(EE_SCENARIO_ROUND_MESSAGE_VOICED, a, 0, 0.0) != 0 {
                h.op(EE_SCENARIO_ROUND_STOP_SOUNDS, 0, 0, 0.0);
            }
            h.op(EE_SCENARIO_ROUND_POST_DIALOG, a, 0, 0.0);
            if h.op(EE_SCENARIO_ROUND_MESSAGE_UNIT, a, 0, 0.0) != 0 {
                let scroll = h.op(EE_SCENARIO_ROUND_MESSAGE_SCROLL, a, 0, 0.0) as c_long;
                if scroll >= 0 {
                    h.op(EE_SCENARIO_ROUND_CAMERA_CLEAR_A, 0, 0, 0.0);
                    h.op(EE_SCENARIO_ROUND_CAMERA_CLEAR_B, 0, 0, 0.0);
                    h.op(EE_SCENARIO_ROUND_CAMERA_SET_MODE, 0, 0, 0.0);
                    let move_to = if scroll == 0 {
                        EE_SCENARIO_ROUND_MESSAGE_MOVE_TO_FACING
                    } else {
                        EE_SCENARIO_ROUND_MESSAGE_MOVE_TO
                    };
                    h.op(move_to, a, 0, 0.0);
                }
            }
            h.op(EE_SCENARIO_ROUND_MESSAGE_ERASE, a, 0, 0.0);
            0
        }
        EE_SCENARIO_ROUND_HIDE_MESSAGES => {
            h.op(EE_SCENARIO_ROUND_POST_HIDE, 0, 0, 0.0);
            h.op(EE_SCENARIO_ROUND_STOP_SOUNDS, 0, 0, 0.0);
            0
        }
        EE_SCENARIO_ROUND_HUMAN_PLAYING => {
            let players = h.op(EE_SCENARIO_ROUND_NUM_PLAYERS, 0, 0, 0.0) as c_long;
            let mut human = false;
            for i in 0..players {
                if h.op(EE_SCENARIO_ROUND_PLAYER_EXISTS, i as i64, 0, 0.0) != 0
                    && h.op(EE_SCENARIO_ROUND_PLAYER_HUMAN, i as i64, 0, 0.0) != 0
                    && h.op(EE_SCENARIO_ROUND_PLAYER_OBSERVER, i as i64, 0, 0.0) == 0
                {
                    human = true;
                }
            }
            human as i64
        }
        EE_SCENARIO_ROUND_EXIT_CINEMATIC => {
            exit_cinematic(h);
            0
        }
        _ => return None,
    })
}

// ---- the C ABI (the C++ members forward here with their round host, `EESMRoundHost(this)`)

/// `EEScenarioManager::EndCinematic`.
#[no_mangle]
pub extern "C" fn ee_scenario_end_cinematic(mut host: EeScenarioRoundHost) {
    end_cinematic(&mut host);
}

/// `EEScenarioManager::StartCinematic`.
#[no_mangle]
pub extern "C" fn ee_scenario_start_cinematic(mut host: EeScenarioRoundHost) {
    start_cinematic(&mut host);
}

/// `EEScenarioManager::ExitCinematic`.
#[no_mangle]
pub extern "C" fn ee_scenario_exit_cinematic(mut host: EeScenarioRoundHost) {
    exit_cinematic(&mut host);
}

/// `EEScenarioManager::ApplyPendingEffects`.
#[no_mangle]
pub extern "C" fn ee_scenario_apply_pending(mut host: EeScenarioRoundHost) {
    apply_pending(&mut host);
}

/// The scenario's unit class media unloaded (`prepare`: `Prepare`'s, traced and cleared; else the destructor's).
#[no_mangle]
pub extern "C" fn ee_scenario_unload_media(mut host: EeScenarioRoundHost, prepare: bool) {
    unload_media(&mut host, prepare);
}

/// `EEScenarioManager::ReplaceUnit`.
#[no_mangle]
pub extern "C" fn ee_scenario_replace_unit(mut host: EeScenarioRoundHost, old: c_long, new: c_long) {
    replace_unit(&mut host, old, new);
}

#[cfg(test)]
#[path = "cinematic_tests.rs"]
mod tests;
