//! The UI cluster's host cases answered on Rust objects directly (round 2): the main game form's host
//! (`ee_screenscpp_main_form`) ops on the UI manager's and the main form's records, the toolkit's objects, the sound
//! manager (an `SESoundManager*` is its `EeSoundManager`) and the UI thread's posts. Each answer is its C++ case's
//! statement; an op that is not here goes on to the C++ host (`None`). Only the hosts' adapters call this module.

// (the casts of the C `unsigned long`s are no-ops on Windows and conversions on LP64)
#![allow(clippy::unnecessary_cast)]

use std::os::raw::c_ulong;
use std::sync::atomic::{AtomicBool, Ordering};

use ee_audio::sound::managerobj::{self as sound, EeSoundManager};
use ee_ui::bgobj::EeUiBgCall;
use ee_ui::direct;
use ee_ui::formobj::EeUiFormCall;
use ee_ui::groupobj::EeUiEntityGroupCall;
use ee_ui::managerobj::EeUiManagerCall;

use crate::screens::main_form::*;
use crate::screens::ui_objects;

/// `gUnitSelected` (0x56488c): a unit is selected (the main form draws the unit panels' backgrounds).
static UNIT_SELECTED: AtomicBool = AtomicBool::new(false);

/// The UI manager's sound manager (`gSoundManager`, the one the manager was made with).
fn sound_manager() -> *const EeSoundManager {
    direct::manager_call(direct::manager_of(core::ptr::null_mut()), EeUiManagerCall::SoundManager, 0, 0, 0)
        as *const EeSoundManager
}

/// The main game form's ops on the records, the main form's toolkit objects, the sound manager and the posts.
pub(crate) fn main_form(op: u32, a: i64, b: i64) -> Option<i64> {
    let main = ui_objects::main_game_form;
    Some(match op {
        EE_SCREENS_MF_OP_SET_UNIT_SELECTED => {
            UNIT_SELECTED.store(a != 0, Ordering::Relaxed);
            0
        }
        EE_SCREENS_MF_OP_UNIT_SELECTED => UNIT_SELECTED.load(Ordering::Relaxed) as i64,
        EE_SCREENS_MF_OP_REFRESH_MAIN_FORM => {
            if let Some(r) = crate::screens::main_form_fields::record(main()) {
                // SAFETY: the main game form's live record
                unsafe { (*r).mRefreshGroup = true };
            }
            0
        }
        // SAFETY: as above (the C++ read the form unchecked: a form is there whenever the HUD runs)
        EE_SCREENS_MF_OP_MAIN_UI_STATE => {
            crate::screens::main_form_fields::record(main()).map_or(0, |r| unsafe { (*r).mUIState } as i64)
        }
        EE_SCREENS_MF_OP_MAIN_ENTITY_GROUP => {
            let g = direct::form_call(main(), EeUiFormCall::EntityGroup, a as isize, 0) as direct::Cpp;
            let call = if b != 0 { EeUiEntityGroupCall::Enable } else { EeUiEntityGroupCall::Disable };
            direct::group_call(g, call, 0);
            0
        }
        EE_SCREENS_MF_OP_MAIN_BACKGROUND => {
            let bg = direct::form_call(main(), EeUiFormCall::BackgroundById, a as c_ulong as isize, 0) as direct::Cpp;
            direct::background_call(bg, EeUiBgCall::SetVisibility, (b != 0) as isize, 0);
            0
        }
        EE_SCREENS_MF_OP_UPDATE_JIT => {
            // (gUserInterface->UpdateJITManufacturing() when there is the UI object)
            crate::screens::ui_objects_ffi::post_if_ui(0x416);
            0
        }
        // SAFETY (the four): the live sound manager the UI manager holds
        EE_SCREENS_MF_OP_SET_MUTED => {
            unsafe { sound::ee_sound_manager_set_muted(sound_manager(), a != 0) };
            0
        }
        EE_SCREENS_MF_OP_MUTED => i64::from(unsafe { sound::ee_sound_manager_muted(sound_manager()) }),
        EE_SCREENS_MF_OP_PLAY_TIME => {
            i64::from(unsafe { sound::ee_sound_manager_total_play_time(sound_manager(), a as u32) })
        }
        EE_SCREENS_MF_OP_LATENCY => {
            unsafe { sound::ee_sound_manager_set_latency(sound_manager(), a as c_ulong as u32) };
            0
        }
        _ => return None,
    })
}

/// `kFormSplash`, its `kSplashLoadLabel`; the main game form's `kMainGameTitle` (ee/EEUIManager.h).
const FORM_SPLASH: c_ulong = 23;
const SPLASH_LOAD_LABEL: c_ulong = 3;
const MAIN_GAME_TITLE: c_ulong = 25;

/// The UI thread messages' handlers that only read the world and set a label (`ee_screenscpp_ui_message`).
pub(crate) fn ui_message(op: u32, a: i64) -> Option<i64> {
    use crate::screens::hud_direct::{label_set_text, resource_string};
    use crate::screens::ui_thread::*;
    let m = direct::manager_of(core::ptr::null_mut());
    match op {
        EE_SCREENS_UI_OP_SHOW_EPOCH_NAME => {
            // ShowEpochName (0x2769da): the local player's epoch name in the main game form's title label
            // SAFETY: the UI thread's read of the world, as the original's (no lock)
            let t = unsafe { ee_sim::world::WorldRead::begin() };
            let id = ee_sim::world::WorldView::global(&t).and_then(|w| {
                let tree = w.player(w.current_player_index() as usize)?.tech_tree()?;
                Some(ee_sim::world_field!(tree.fields(), mEpochNameID) as i64)
            });
            if let Some(id) = id {
                let label = direct::control(ui_objects::main_game_form(), MAIN_GAME_TITLE);
                if !label.is_null() {
                    label_set_text(label, resource_string(id).unwrap_or(b""));
                }
            }
        }
        EE_SCREENS_UI_OP_LOADING_TEXT => {
            // ShowLoadingText (0x279fbc): the string on the splash form's load label while it is the current form
            direct::manager_call(m, EeUiManagerCall::LockForCopy, 0, 0, 0);
            let splash = direct::form_by_id(m, FORM_SPLASH);
            if !m.is_null() && direct::manager_call(m, EeUiManagerCall::CurrentForm, 0, 0, 0) as direct::Cpp == splash {
                label_set_text(direct::control(splash, SPLASH_LOAD_LABEL), resource_string(a).unwrap_or(b""));
            }
            direct::manager_call(m, EeUiManagerCall::LockForCopy, 1, 0, 0);
        }
        _ => return None,
    }
    Some(0)
}
