//! The effect run's world operations that were C++ statement sequences over texts, sounds, messages and the camera
//! (Scenario Manager/EESMEffect.cpp's `EESMEffectWorld` and `SMEChangeUnitWorld`; the scenario editor lane's round
//! 4): the flare, Play Sound, Change Text, the dialogue message, the chat log and box, the calamity class and type
//! tests, the pending effect for another player, a camera key, and Change Unit's garrison wait and kill. Composed here
//! of single C++ statements (`EE_SCENARIO_WY_*`, `SMEYWorld` in EESMEffect.cpp; the effect copies are ca_compose.rs'
//! and read_compose.rs' statements), in the C++'s order; the effect's numbers are read through the record the C++
//! class derives from.

// the records keep the C++ member names; c_long/c_ulong are 32 bits on Windows
#![allow(non_snake_case, clippy::unnecessary_cast)]

use std::os::raw::{c_long, c_ulong};

use crate::scenario::ca_compose::{
    EE_SCENARIO_WK_EFFECT_ADD, EE_SCENARIO_WK_EFFECT_FIELDS, EE_SCENARIO_WK_EFFECT_FREE, EE_SCENARIO_WK_EFFECT_NEW,
    EE_SCENARIO_WK_PLAYER,
};
use crate::scenario::effect::*;
use crate::scenario::fields::EeScenarioEffectFields;
use crate::scenario::read_compose::*;
use crate::scenario::task::EE_SCENARIO_WORLD_CAL_TARGET_KIND;
use crate::scenario::unit_change::{EE_SCENARIO_CU_GARRISON_WAIT, EE_SCENARIO_CU_KILL, EE_SCENARIO_WORLD_CU_SET};

// ---- the statements (handles are the objects' addresses; "the run" is the context's effect run)
/// The context's effect as its `EeScenarioEffectFields*`.
pub const EE_SCENARIO_WY_EFFECT_FIELDS: u32 = 810;
/// Player a0's `mEvents.AddEvent(U2DPoint<long>(a1, a2), 3, a0, -1, true)`.
pub const EE_SCENARIO_WY_FLARE: u32 = 811;
/// `EELoadSound("Sounds\\" + SMESoundPath() + the effect's mFileName, 0x31, *a0, *a1, 0)` (a0, a1 `unsigned
/// long*`), `gSoundManager->RegisterOneShotSound(a0, a1)`.
pub const EE_SCENARIO_WY_SOUND_LOAD: u32 = 812;
pub const EE_SCENARIO_WY_SOUND_REGISTER: u32 = 813;
/// `EEUISCIFormManager::Get()`; the media strings' section a0 emptied, or appended `"\n"` and the effect's
/// `mDescription`; `EESoundObject::Get(a0)`, its `Play()`; form a0's `SetBlinking(true)`.
pub const EE_SCENARIO_WY_SCI_FORM: u32 = 814;
pub const EE_SCENARIO_WY_SECTION_CLEAR: u32 = 815;
pub const EE_SCENARIO_WY_SECTION_APPEND: u32 = 816;
pub const EE_SCENARIO_WY_SOUND_OBJECT: u32 = 817;
pub const EE_SCENARIO_WY_SOUND_PLAY: u32 = 818;
pub const EE_SCENARIO_WY_FORM_BLINK: u32 = 819;
/// `new EESMDialogMessage`; message a0's `mText += the run's mText`, `mDuration`, `mIsCinematic`, `mUnitType`,
/// `mScrollSpeed`, `mUnitID` = a1, `mSound = SMESoundPath() + the effect's mFileName`; the manager's
/// `AddMessage(*a0)`; `delete` a0; unit a0's `mUnitID`.
pub const EE_SCENARIO_WY_MSG_NEW: u32 = 820;
pub const EE_SCENARIO_WY_MSG_TEXT: u32 = 821;
pub const EE_SCENARIO_WY_MSG_DURATION: u32 = 822;
pub const EE_SCENARIO_WY_MSG_CINEMATIC: u32 = 823;
pub const EE_SCENARIO_WY_MSG_UNIT_TYPE: u32 = 824;
pub const EE_SCENARIO_WY_MSG_SCROLL: u32 = 825;
pub const EE_SCENARIO_WY_MSG_UNIT_ID: u32 = 826;
pub const EE_SCENARIO_WY_MSG_SOUND: u32 = 827;
pub const EE_SCENARIO_WY_MSG_ADD: u32 = 828;
pub const EE_SCENARIO_WY_MSG_FREE: u32 = 829;
pub const EE_SCENARIO_WY_UNIT_ID: u32 = 830;
/// `gUserInterface->AddChatLogEntry(GEColor(), UWideString(), the run's mText, a0, 1)`; the chat text (a copy of
/// the run's `mText`); `gUIManager->DisplayMessage(it, 1, NULL, NULL, true)`.
pub const EE_SCENARIO_WY_CHAT_LOG: u32 = 831;
pub const EE_SCENARIO_WY_CHAT_TEXT: u32 = 832;
pub const EE_SCENARIO_WY_CHAT_BOX: u32 = 833;
/// The run's `mCalamity = GetCalamityClass(a0)` (`!= NULL`); `GetCalamityType(a0, *a1)` (a1 a `long*`); the run's
/// `mCalamity->GetTargetKind()`.
pub const EE_SCENARIO_WY_CAL_CLASS: u32 = 834;
pub const EE_SCENARIO_WY_CAL_TYPE: u32 = 835;
pub const EE_SCENARIO_WY_CAL_TARGET_KIND: u32 = 836;
/// `new EECameraKey`; key a0's `mOffset = the camera's GetEndOffset()`, `GetUnitPoint(unit a1, mFocus)`,
/// `GetUnitTurn(unit a1, mT40)`, `mWait` (float bits a1), `mChanged = a1 != 0`, `mRate` (float bits a1); the
/// camera's `AddKey(*a0)`; `delete` a0.
pub const EE_SCENARIO_WY_KEY_NEW: u32 = 837;
pub const EE_SCENARIO_WY_KEY_OFFSET: u32 = 838;
pub const EE_SCENARIO_WY_KEY_FOCUS: u32 = 839;
pub const EE_SCENARIO_WY_KEY_TURN: u32 = 840;
pub const EE_SCENARIO_WY_KEY_WAIT: u32 = 841;
pub const EE_SCENARIO_WY_KEY_CHANGED: u32 = 842;
pub const EE_SCENARIO_WY_KEY_RATE: u32 = 843;
pub const EE_SCENARIO_WY_KEY_ADD: u32 = 844;
pub const EE_SCENARIO_WY_KEY_FREE: u32 = 845;
/// Change Unit's, on the context's unit: the empty unit list (the C++'s local) cleared; `QueueGoal(EEUGRust::
/// NewLoad(kGoalLoadGarrison, it, the list, false), 0)`; `StartAction(a0)`; its class's `ActionChanged(it)`;
/// `Kill(0, 0, 0)`; `SetResourceAmount(0)`; its player's `UnitTaken(it, false)`, `KillUnit(it, false)`.
pub const EE_SCENARIO_WY_CU_UNITS_CLEAR: u32 = 846;
pub const EE_SCENARIO_WY_CU_QUEUE_GARRISON: u32 = 847;
pub const EE_SCENARIO_WY_CU_START_ACTION: u32 = 848;
pub const EE_SCENARIO_WY_CU_ACTION_CHANGED: u32 = 849;
pub const EE_SCENARIO_WY_CU_KILL: u32 = 850;
pub const EE_SCENARIO_WY_CU_RESOURCE_ZERO: u32 = 851;
pub const EE_SCENARIO_WY_CU_TAKEN: u32 = 852;
pub const EE_SCENARIO_WY_CU_KILL_UNIT: u32 = 853;

/// `kSoundTrumpets`
pub(crate) const SOUND_TRUMPETS: i64 = 0x1b7;
/// `kActionWaitForGarrisonCost`
pub(crate) const ACTION_WAIT_FOR_GARRISON_COST: i64 = 12008;
/// Change Text's sections by action: hints, instructions, history (`GetSection`'s slots).
const SECTIONS: [i64; 3] = [0, 3, 2];

fn bits(v: f32) -> i64 {
    v.to_bits() as i64
}

/// The world operation `op` composed of the C++ statements `cpp` runs, or `None` for the others.
pub fn compose(cpp: &mut dyn FnMut(u32, [i64; 6]) -> i64, op: u32, a: [i64; 6]) -> Option<i64> {
    let mut w = |op: u32, args: &[i64]| -> i64 {
        let mut v = [0i64; 6];
        v[..args.len()].copy_from_slice(args);
        cpp(op, v)
    };
    // the context effect's numbers, read at once
    let effect = |w: &mut dyn FnMut(u32, &[i64]) -> i64| -> EeScenarioEffectFields {
        let e = w(EE_SCENARIO_WY_EFFECT_FIELDS, &[]) as usize as *const EeScenarioEffectFields;
        // SAFETY: the run's live effect record, copied before the next C++ call
        unsafe { *e }
    };
    Some(match op {
        EE_SCENARIO_WORLD_FLARE => {
            let player = w(EE_SCENARIO_WK_PLAYER, &[a[0]]);
            w(EE_SCENARIO_WY_FLARE, &[player, a[1], a[2]]);
            0
        }
        EE_SCENARIO_WORLD_PLAY_SOUND => {
            let (mut sample, mut instance): (c_ulong, c_ulong) = (0, 0);
            let at = |v: &mut c_ulong| v as *mut c_ulong as usize as i64;
            w(EE_SCENARIO_WY_SOUND_LOAD, &[at(&mut sample), at(&mut instance)]);
            // UIsNone32: the low 32 bits all set
            if instance as u32 != u32::MAX {
                w(EE_SCENARIO_WY_SOUND_REGISTER, &[sample as i64, instance as i64]);
            }
            0
        }
        EE_SCENARIO_WORLD_CHANGE_TEXT => {
            let form = w(EE_SCENARIO_WY_SCI_FORM, &[]);
            if (0..3).contains(&a[0]) {
                let slot = SECTIONS[a[0] as usize];
                if a[1] != 0 {
                    w(EE_SCENARIO_WY_SECTION_CLEAR, &[slot]);
                } else {
                    w(EE_SCENARIO_WY_SECTION_APPEND, &[slot]);
                }
            }
            // SMEPlayTrumpets
            let sound = w(EE_SCENARIO_WY_SOUND_OBJECT, &[SOUND_TRUMPETS]);
            if sound != 0 {
                w(EE_SCENARIO_WY_SOUND_PLAY, &[sound]);
            }
            w(EE_SCENARIO_WY_FORM_BLINK, &[form]);
            0
        }
        EE_SCENARIO_WORLD_DIALOGUE => {
            let e = effect(&mut w);
            let m = w(EE_SCENARIO_WY_MSG_NEW, &[]);
            w(EE_SCENARIO_WY_MSG_TEXT, &[m]);
            w(EE_SCENARIO_WY_MSG_DURATION, &[m, e.mDuration.wrapping_mul(1000) as i64]);
            w(EE_SCENARIO_WY_MSG_CINEMATIC, &[m, (a[0] != 0) as i64]);
            let object = w(EE_SCENARIO_RX_OBJECT, &[e.mObject as i64]);
            w(EE_SCENARIO_RX_OBJECT_EVALUATE, &[object]);
            let unit = if w(EE_SCENARIO_RX_OBJECT_UNIT_COUNT, &[object]) > 0 {
                let id = w(EE_SCENARIO_RX_OBJECT_UNIT_AT, &[object, 0]);
                w(EE_SCENARIO_RX_UNIT, &[id])
            } else {
                0
            };
            if unit != 0 {
                let unit_type = w(EE_SCENARIO_RX_UNIT_TYPE, &[unit]);
                w(EE_SCENARIO_WY_MSG_UNIT_TYPE, &[m, unit_type]);
                w(EE_SCENARIO_WY_MSG_SCROLL, &[m, e.mIntValue1 as i64]);
                let id = w(EE_SCENARIO_WY_UNIT_ID, &[unit]);
                w(EE_SCENARIO_WY_MSG_UNIT_ID, &[m, id]);
            } else {
                w(EE_SCENARIO_WY_MSG_UNIT_TYPE, &[m, 0]);
            }
            w(EE_SCENARIO_WY_MSG_SOUND, &[m]);
            w(EE_SCENARIO_WY_MSG_ADD, &[m]);
            w(EE_SCENARIO_WY_MSG_FREE, &[m]);
            0
        }
        EE_SCENARIO_WORLD_CHAT_LOG => {
            let e = effect(&mut w);
            w(EE_SCENARIO_WY_CHAT_LOG, &[e.mDuration.wrapping_mul(1000) as i64]);
            0
        }
        EE_SCENARIO_WORLD_CHAT_BOX => {
            w(EE_SCENARIO_WY_CHAT_TEXT, &[]);
            w(EE_SCENARIO_WY_CHAT_BOX, &[]);
            0
        }
        EE_SCENARIO_WORLD_CAL_CLASS => w(EE_SCENARIO_WY_CAL_CLASS, &[a[0]]),
        EE_SCENARIO_WORLD_CAL_TYPE => {
            let mut kind: c_long = 0;
            let found = w(EE_SCENARIO_WY_CAL_TYPE, &[a[0], &mut kind as *mut c_long as usize as i64]);
            if found != 0 {
                kind as i64
            } else {
                -1
            }
        }
        EE_SCENARIO_WORLD_CAL_TARGET_KIND => {
            if w(EE_SCENARIO_WY_CAL_CLASS, &[a[0]]) != 0 {
                w(EE_SCENARIO_WY_CAL_TARGET_KIND, &[])
            } else {
                -1
            }
        }
        EE_SCENARIO_WORLD_ADD_PENDING_PLAYER => {
            let copy = w(EE_SCENARIO_WK_EFFECT_NEW, &[]);
            w(EE_SCENARIO_RX_EFFECT_COPY, &[copy]);
            let f = w(EE_SCENARIO_WK_EFFECT_FIELDS, &[copy]) as usize as *mut EeScenarioEffectFields;
            // SAFETY: the copy's record, written before the next C++ call
            unsafe { (*f).mPlayer = a[0] as c_ulong };
            w(EE_SCENARIO_WK_EFFECT_ADD, &[copy]);
            w(EE_SCENARIO_WK_EFFECT_FREE, &[copy]);
            0
        }
        EE_SCENARIO_WORLD_CAMERA_KEY => {
            // the camera APIs take the original base-unit pointer, its optional complex-unit link included
            let duration = a[1] as c_long;
            let unit = w(EE_SCENARIO_RX_UNIT, &[a[0]]);
            let key = w(EE_SCENARIO_WY_KEY_NEW, &[]);
            w(EE_SCENARIO_WY_KEY_OFFSET, &[key]);
            w(EE_SCENARIO_WY_KEY_FOCUS, &[key, unit]);
            w(EE_SCENARIO_WY_KEY_TURN, &[key, unit]);
            w(EE_SCENARIO_WY_KEY_WAIT, &[key, bits(0.0)]);
            w(EE_SCENARIO_WY_KEY_CHANGED, &[key, (duration == 0) as i64]);
            let rate = if duration == 0 { 0.0 } else { 1.0f32 / duration as f32 };
            w(EE_SCENARIO_WY_KEY_RATE, &[key, bits(rate)]);
            w(EE_SCENARIO_WY_KEY_ADD, &[key]);
            w(EE_SCENARIO_WY_KEY_FREE, &[key]);
            0
        }
        EE_SCENARIO_WORLD_CU_SET if a[0] == EE_SCENARIO_CU_GARRISON_WAIT => {
            w(EE_SCENARIO_WY_CU_UNITS_CLEAR, &[]);
            w(EE_SCENARIO_WY_CU_QUEUE_GARRISON, &[]);
            // SMEAction
            w(EE_SCENARIO_WY_CU_START_ACTION, &[ACTION_WAIT_FOR_GARRISON_COST]);
            w(EE_SCENARIO_WY_CU_ACTION_CHANGED, &[]);
            0
        }
        EE_SCENARIO_WORLD_CU_SET if a[0] == EE_SCENARIO_CU_KILL => {
            if a[1] == 0 {
                w(EE_SCENARIO_WY_CU_KILL, &[]);
            } else {
                w(EE_SCENARIO_WY_CU_RESOURCE_ZERO, &[]);
                w(EE_SCENARIO_WY_CU_TAKEN, &[]);
                w(EE_SCENARIO_WY_CU_KILL_UNIT, &[]);
            }
            0
        }
        _ => return None,
    })
}

#[cfg(test)]
#[path = "effect_text_tests.rs"]
mod tests;
