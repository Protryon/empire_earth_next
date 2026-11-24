//! The C++ constants the game UI's Rust numbers equal (once the `static_assert`s of EEUIFMainGame.cpp,
//! EEUIFMGStatusBox.cpp, EEUIFMGStatusButton.cpp and EEUIManager.cpp): the sounds, string ids, unit types and
//! categories, world constants, improvements, keys, control events and the db control and background types, read
//! from the headers themselves.

// the Rust numbers' types differ (u32, i64): each is widened to compare
#![allow(clippy::unnecessary_cast)]

use std::collections::HashMap;
use std::sync::OnceLock;

use crate::screens::main_form::*;
use crate::screens::mouse_modes::*;
use crate::screens::status_box::*;
use crate::screens::status_button::*;
use crate::screens::ui_manager_calls::*;

/// The headers the C++ names come from.
const HEADERS: [&str; 9] = [
    "include/ee/EEStringIDs.h",
    "include/rust/sim/game_ids.h",
    "include/ee/EEUnitClassIDs.h",
    "include/rust/sim/unit_ids.h",
    "include/ee/EEWorldConstants.h",
    "include/ee/EEControlEventID.h",
    "include/rust/base/lle_enums.h",
    "include/rust/ui/ids.h",
    "include/lle/UIControl.h",
];

/// A C++ constant's value: `#define NAME value` or an enumerator `NAME = value,` of the headers.
fn c(name: &str) -> i64 {
    static TABLE: OnceLock<HashMap<String, i64>> = OnceLock::new();
    let table = TABLE.get_or_init(|| {
        let mut t = HashMap::new();
        for h in HEADERS {
            let path = format!("{}/../../../{h}", env!("CARGO_MANIFEST_DIR"));
            let text = std::fs::read(&path).unwrap_or_else(|e| panic!("{path}: {e}"));
            for line in String::from_utf8_lossy(&text).lines() {
                let words: Vec<&str> = line.split_whitespace().collect();
                let (n, v) = match words.as_slice() {
                    ["#define", n, v, ..] => (*n, *v),
                    [n, "=", v, ..] => (*n, v.trim_end_matches(',')),
                    _ => continue,
                };
                let v = match v.strip_prefix("0x") {
                    Some(h) => i64::from_str_radix(h, 16),
                    None => v.parse(),
                };
                if let Ok(v) = v {
                    t.entry(n.to_string()).or_insert(v);
                }
            }
        }
        t
    });
    *table.get(name).unwrap_or_else(|| panic!("{name} not in the headers"))
}

/// Once EEUIFMainGame.cpp's "main_form.rs' sounds and strings" and "mouse_modes.rs' farms and keys".
#[test]
fn main_form_sounds_strings_farms_and_keys() {
    assert_eq!(EE_SCREENS_MF_SOUND_BUTTON1 as i64, c("kSoundButton1"));
    assert_eq!(EE_SCREENS_MF_SOUND_BUTTON2 as i64, c("kSoundButton2"));
    assert_eq!(EE_SCREENS_MF_IDS_FPS as i64, c("IDS_FPS"));
    assert_eq!(EE_SCREENS_MF_IDS_GAME_PAUSED as i64, c("IDS_GAME_PAUSED"));
    assert_eq!(EE_SCREENS_MF_TYPE_FARM as i64, c("kUnitTypeFarm"));
    assert_eq!(EE_SCREENS_MF_TYPE_ROBOTIC_FARM as i64, c("kUnitTypeRoboticFarm"));
    assert_eq!(EE_SCREENS_MF_KEY_ESCAPE as i64, c("kKeyEscape"));
    assert_eq!(EE_SCREENS_MF_KEY_F1 as i64, c("kKeyF1"));
    assert_eq!(EE_SCREENS_MF_KEY_F3 as i64, c("kKeyF3"));
    assert_eq!(EE_SCREENS_MF_KEY_F9 as i64, c("kKeyF9"));
}

/// Once EEUIFMGStatusBox.cpp's "status_box.rs's constants".
#[test]
fn status_box_constants() {
    assert_eq!(EE_SCREENS_BOX_IMPROVEMENT_FLIGHT_OR_POWER as i64, c("kImprovementFlightTimeOrPower"));
    assert_eq!(EE_SCREENS_BOX_IMPROVEMENT_HIT_POINTS as i64, c("kImprovementHitPoints"));
    assert_eq!(EE_SCREENS_BOX_IMPROVEMENT_TRANSPORT as i64, c("kImprovementTransport"));
    assert_eq!(EE_SCREENS_BOX_IDS_GARRISON as i64, c("IDS_IMPROVEMENT_GARRISON"));
    assert_eq!(c("kDisplayModeHidden"), 0);
    assert_eq!(c("kDisplayModeShown"), 1);
}

/// Once EEUIFMGStatusButton.cpp's "status_button.rs's constants".
#[test]
fn status_button_constants() {
    assert_eq!(EE_SCREENS_SB_TYPE_TELEPORTER as i64, c("kUnitTypeTeleporter"));
    assert_eq!(EE_SCREENS_SB_CATEGORY_FORTRESS as i64, c("kUnitCategoryFortress"));
    assert_eq!(EE_SCREENS_SB_SOUND_BUTTON1 as i64, c("kSoundButton1"));
    assert_eq!(EE_SCREENS_SB_SOUND_BUTTON2 as i64, c("kSoundButton2"));
    assert_eq!(EE_SCREENS_SB_WORLD_JIT_DELAY as i64, c("kWorldDeathmatchJitManufacturingDelay"));
    assert_eq!(EE_SCREENS_SB_WORLD_REFUSAL_INTERVAL as i64, c("kWorldRefusalMessageInterval"));
    assert_eq!(EE_SCREENS_SB_IDS_JIT_NOT_YET as i64, c("IDS_JUST_IN_TIME_MANUFACTURING_IS_NOT"));
    assert_eq!(EE_SCREENS_SB_IDS_INSUFFICIENT_POWER as i64, c("IDS_INSUFFICIENT_POWER"));
    assert_eq!(EE_SCREENS_SB_IDS_TRADE_BUY as i64, c("IDS_TRADE_BUY"));
    assert_eq!(EE_SCREENS_SB_IDS_TRADE_SELL as i64, c("IDS_TRADE_SELL"));
    assert_eq!(EE_SCREENS_SB_IDS_TRADE_FOR as i64, c("IDS_TRADE_FOR"));
    assert_eq!(EE_SCREENS_SB_IDS_RESOURCE_NAME_BASE as i64, c("IDS_RESOURCE_NAME_BASE"));
    assert_eq!(EE_SCREENS_SB_IDS_RESOURCE_NAME_GOLD as i64, c("IDS_RESOURCE_NAME_GOLD"));
    assert_eq!(EE_SCREENS_SB_IDS_IMPROVEMENT_POWER as i64, c("IDS_IMPROVEMENT_POWER"));
    assert_eq!(EE_SCREENS_SB_IDS_POP_COST as i64, c("IDS_POP_COST"));
}

/// Once EEUIManager.cpp's "ui_manager_calls.rs' ids".
#[test]
fn ui_manager_ids() {
    assert_eq!(EE_SCREENS_UMC_WNS_CANCEL as i64, c("kControlEventWNSCancel"));
    assert_eq!(EE_SCREENS_UMC_BACKGROUND_GENERIC as i64, c("kBackgroundGeneric"));
    for (rust, cpp) in [
        (EE_SCREENS_UMC_STATUS_BUTTON, "kDbControlStatusButton"),
        (EE_SCREENS_UMC_MINI_MAP, "kDbControlMiniMap"),
        (EE_SCREENS_UMC_ERROR_LOG, "kDbControlErrorLog"),
        (EE_SCREENS_UMC_ATTRIBUTE, "kDbControlAttribute"),
        (EE_SCREENS_UMC_MAIN_GAME_STATUS_BAR, "kDbControlMainGameStatusBar"),
        (EE_SCREENS_UMC_SELECT_UNITS, "kDbControlSelectUnits"),
        (EE_SCREENS_UMC_COMMAND_UNITS, "kDbControlCommandUnits"),
        (EE_SCREENS_UMC_PORTRAIT, "kDbControlPortrait"),
        (EE_SCREENS_UMC_CHAT_LINE, "kDbControlChatLine"),
        (EE_SCREENS_UMC_CB_LIST_BOX, "kDbControlCBListBox"),
        (EE_SCREENS_UMC_WON_STAT_BOX, "kDbControlWonNetStatBox"),
        (EE_SCREENS_UMC_UNIT_ICON_BAR, "kDbControlUnitIconBar"),
        (EE_SCREENS_UMC_SAVED_GROUP, "kDbControlSavedGroup"),
        (EE_SCREENS_UMC_PGS_LINE_GRAPH, "kDbControlPGSLineGraph"),
        (EE_SCREENS_UMC_PING, "kDbControlPing"),
        (EE_SCREENS_UMC_WON_ROOMS_BOX, "kDbControlWonNetRoomsBox"),
        (EE_SCREENS_UMC_WON_GAMES_BOX, "kDbControlWonNetGamesBox"),
    ] {
        assert_eq!(rust as i64, c(cpp), "{cpp}");
    }
}
