//! The C++ constants the editor's Rust numbers equal (once the editor's C++ `static_assert`s): the string ids
//! (`ee/EEStringIDs.h`), the UI framework's alignments, display modes and control triggers (`lle/GETextAlignment.h`,
//! `lle/UIControl.h`), the unit categories and movements (`ee/EEUnitClassIDs.h`), read from the headers themselves.

use std::collections::HashMap;
use std::sync::OnceLock;

use ee_sim::scenario::ids::*;

use crate::screens::editor_events::EE_SCREENS_SEE_PER_PLAYER_LABEL_ID;
use crate::screens::editor_form::*;
use crate::screens::editor_ids::*;
use crate::screens::editor_object::*;
use crate::screens::editor_players::*;
use crate::screens::editor_selt::*;
use crate::screens::editor_seplx::*;
use crate::screens::editor_trigger::*;
use crate::screens::editor_tscreen::*;
use crate::screens::editor_unit::*;

/// The headers the C++ names come from.
const EE_SCREENS_SECC_HEADERS: [&str; 4] =
    ["include/ee/EEStringIDs.h", "include/lle/GETextAlignment.h", "include/lle/UIControl.h", "include/ee/EEUnitClassIDs.h"];

/// A C++ constant's value: `#define NAME value` or an enumerator `NAME = value,` of the headers.
fn c(name: &str) -> i64 {
    static TABLE: OnceLock<HashMap<String, i64>> = OnceLock::new();
    let table = TABLE.get_or_init(|| {
        let mut t = HashMap::new();
        for h in EE_SCREENS_SECC_HEADERS {
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

/// Once EESELoadThread.cpp's static_assert "editor_selt.rs's numbers".
#[test]
fn eeseloadthread_cpp_editor_selt_rs_s_numbers() {
    assert_eq!(EE_SCREENS_SELTX_IDS_LOADING_EPOCH_FOR_PLAYER as i64, c("IDS_LOADING_EPOCH_FOR_PLAYER"));
    assert_eq!(EE_SCREENS_SELTX_IDS_LOADING_FOR_PLAYER as i64, c("IDS_LOADING_FOR_PLAYER"));
    assert_eq!(EE_SCREENS_SELTX_IDS_LOADING_TERRAIN as i64, c("IDS_LOADING_TERRAIN"));
    assert_eq!(EE_SCREENS_SELTX_IDS_YOU_MUST_ENTER_FILE_NAME_BEFORE as i64, c("IDS_YOU_MUST_ENTER_FILE_NAME_BEFORE"));
    assert_eq!(EE_SCREENS_SELTX_IDS_REPLACE_FILE_ON_DISK as i64, c("IDS_REPLACE_FILE_ON_DISK"));
    assert_eq!(EE_SCREENS_SELTX_IDS_FILE_NAME_NOT_FOUND as i64, c("IDS_FILE_NAME_NOT_FOUND"));
    assert_eq!(EE_SCREENS_SELTX_IDS_FILE_ERROR as i64, c("IDS_FILE_ERROR"));
    assert_eq!(EE_SCREENS_SELTX_IDS_FILE_EXPORTED_SUCCESSFULLY as i64, c("IDS_FILE_EXPORTED_SUCCESSFULLY"));
    assert_eq!(EE_SCREENS_SELTX_IDS_EXPORTED_FILE_NAME_HAS_CHANGED as i64, c("IDS_EXPORTED_FILE_NAME_HAS_CHANGED"));
    assert_eq!(EE_SCREENS_SELTX_IDS_FILE_IMPORTED_SUCCESSFULLY as i64, c("IDS_FILE_IMPORTED_SUCCESSFULLY"));
    assert_eq!(EE_SCREENS_SELTX_IDS_IMPORTING_FILE_REPLACES_ALL_TEXT as i64, c("IDS_IMPORTING_FILE_REPLACES_ALL_TEXT"));
    assert_eq!(EE_SCREENS_SELTX_IDS_FOLLOWING_ERRORS_OCCURRED_WHILE_IMPORTING as i64, c("IDS_FOLLOWING_ERRORS_OCCURRED_WHILE_IMPORTING"));
    assert_eq!(EE_SCREENS_SELTX_IDS_LCL_FILE_HAS_INVALID_VERSION as i64, c("IDS_LCL_FILE_HAS_INVALID_VERSION"));
    assert_eq!(EE_SCREENS_SELTX_LOCALIZATION_EDIT as i64, EE_SCREENS_SE_INSTRUCTIONS_LOCALIZATION_EDIT_BOX as i64);
}

/// Once EESERBUnitTypes.cpp's static_assert "the alignments".
#[test]
fn eeserbunittypes_cpp_the_alignments() {
    assert_eq!(EE_SCREENS_SEPL_ALIGN_LEFT as i64, c("kTextAlignLeft"));
    assert_eq!(EE_SCREENS_SEPL_ALIGN_CENTER as i64, c("kTextAlignCenter"));
}

/// Once EESERBUnitTypes.cpp's static_assert "modes".
#[test]
fn eeserbunittypes_cpp_modes() {
    assert_eq!(EE_SCREENS_SEPL_SHOWN as i64, c("kDisplayModeShown"));
    assert_eq!(EE_SCREENS_SEPL_HIDDEN as i64, c("kDisplayModeHidden"));
}

/// Once EESERBUnitTypes.cpp's static_assert "the triggers".
#[test]
fn eeserbunittypes_cpp_the_triggers() {
    assert_eq!(EE_SCREENS_SEPL_TRIGGER_RANGE as i64, c("kControlTriggerRangeChanged"));
    assert_eq!(EE_SCREENS_SEPL_TRIGGER_CHECKED as i64, c("kControlTriggerChecked"));
    assert_eq!(EE_SCREENS_SEPL_TRIGGER_SELECTION as i64, c("kControlTriggerSelectionChanged"));
}

/// Once EESERBUnitTypes.cpp's static_assert "the strings".
#[test]
fn eeserbunittypes_cpp_the_strings() {
    assert_eq!(EE_SCREENS_SEPL_IDS_MAX_PLAYERS as i64, c("IDS_MAX_PLAYERS"));
    assert_eq!(EE_SCREENS_SEPL_IDS_START_EPOCH as i64, c("IDS_START_EPOCH"));
    assert_eq!(EE_SCREENS_SEPL_IDS_ALL_PLAYERS as i64, c("IDS_ALL_PLAYERS"));
    assert_eq!(EE_SCREENS_SEPL_IDS_SE_PLAYERS as i64, c("IDS_SE_PLAYERS"));
    assert_eq!(EE_SCREENS_SEPL_IDS_SE_PLAYER as i64, c("IDS_SE_PLAYER"));
    assert_eq!(EE_SCREENS_SEPL_IDS_SE_CIVILIZATION as i64, c("IDS_SE_CIVILIZATION"));
    assert_eq!(EE_SCREENS_SEPL_IDS_ENABLE_TECHNOLOGIES as i64, c("IDS_ENABLE_TECHNOLOGIES"));
    assert_eq!(EE_SCREENS_SEPL_IDS_DISABLE_TECHNOLOGIES as i64, c("IDS_DISABLE_TECHNOLOGIES"));
    assert_eq!(EE_SCREENS_SEPL_IDS_UNIT_TYPES as i64, c("IDS_UNIT_TYPES"));
    assert_eq!(EE_SCREENS_SEPL_IDS_SE_UNIT_GROUP_FIRST as i64, c("IDS_SE_UNIT_GROUP_FIRST"));
    assert_eq!(EE_SCREENS_SEPL_IDS_DISPLAY_EPOCHS as i64, c("IDS_DISPLAY_EPOCHS"));
    assert_eq!(EE_SCREENS_SEPL_IDS_END_EPOCH as i64, c("IDS_END_EPOCH"));
    assert_eq!(EE_SCREENS_SEPL_IDS_SE_HUMAN as i64, c("IDS_SE_HUMAN"));
    assert_eq!(EE_SCREENS_SEPL_IDS_SE_ACTIVE as i64, c("IDS_SE_ACTIVE"));
    assert_eq!(EE_SCREENS_SEPL_IDS_SE_ACTIVE as i64 + 4, c("IDS_SE_BUILD_REPAIR"));
    assert_eq!(EE_SCREENS_SEPL_IDS_SE_ACTIVE as i64 + 8, c("IDS_CITIZEN_PERCENTAGE"));
    assert_eq!(EE_SCREENS_SEPL_IDS_RESOURCE_NAME_FIRST as i64, c("IDS_RESOURCE_NAME_FIRST"));
    assert_eq!(EE_SCREENS_SEPL_IDS_GAME_OPTIONS as i64, c("IDS_GAME_OPTIONS"));
    assert_eq!(EE_SCREENS_SEPL_IDS_LOBBY_RESOURCES as i64, c("IDS_LOBBY_RESOURCES"));
    assert_eq!(EE_SCREENS_SEPL_IDS_GAME_GAME_UNIT_LIMIT as i64, c("IDS_GAME_GAME_UNIT_LIMIT"));
    assert_eq!(EE_SCREENS_SEPL_IDS_GAME_GAME_UNIT_LIMIT as i64 + 6, c("IDS_UNIT_IMPROVEMENTS"));
    assert_eq!(EE_SCREENS_SEPL_IDS_GAME_GAME_UNIT_LIMIT as i64 + 7, c("IDS_CPU"));
    assert_eq!(EE_SCREENS_SEPL_IDS_GAME_GAME_UNIT_LIMIT as i64 + 10, c("IDS_GAME_LOS"));
    assert_eq!(EE_SCREENS_SEPL_IDS_MUTUAL_DIPLOMACY_SETTINGS as i64, c("IDS_MUTUAL_DIPLOMACY_SETTINGS"));
    assert_eq!(EE_SCREENS_SEPL_IDS_USE_CARBON_RESOURCE as i64, c("IDS_USE_CARBON_RESOURCE"));
}

/// Once EESERBUnitTypes.cpp's static_assert "editor_seplx.rs's strings".
#[test]
fn eeserbunittypes_cpp_editor_seplx_rs_s_strings() {
    assert_eq!(EE_SCREENS_SEPX_IDS_NONE as i64, c("IDS_NONE"));
    assert_eq!(EE_SCREENS_SEPX_IDS_UNKNOWN_CIVILIZATION as i64, c("IDS_UNKNOWN_CIVILIZATION"));
    assert_eq!(EE_SCREENS_SEPX_IDS_PLAYERS_CURRENTLY_ALLIES as i64, c("IDS_PLAYERS_CURRENTLY_ALLIES"));
    assert_eq!(EE_SCREENS_SEPX_IDS_PLAYERS_CURRENTLY_ENEMIES as i64, c("IDS_PLAYERS_CURRENTLY_ENEMIES"));
    assert_eq!(EE_SCREENS_SEPX_IDS_SET as i64, c("IDS_SET"));
    assert_eq!(EE_SCREENS_SEPX_IDS_UNIT_FAMILY_FIRST as i64, c("IDS_UNIT_FAMILY_FIRST"));
}

/// Once EESETSEffect.cpp's static_assert "editor_effects.rs's values".
#[test]
fn eesetseffect_cpp_editor_effects_rs_s_values() {
    assert_eq!(EE_SCREENS_SE_TRIGGER_SCREEN_TRIGGER as i64, 0);
    assert_eq!(EE_SCREENS_SE_TRIGGER_SCREEN_OBJECT as i64, 3);
    assert_eq!(EE_SCREENS_SE_TRIGGER_SCREEN_AREA as i64, 4);
    assert_eq!(EE_SCREENS_SE_TS_EFFECT_TITLE_LABEL as i64, 0x960);
    assert_eq!(EE_SCREENS_SE_TS_EFFECT_TYPE_COMBO_BOX as i64, 0x961);
    assert_eq!(EE_SCREENS_SE_TRIGGER_SCREEN_NAME_EDIT_BOX as i64, 0x7db);
    assert_eq!(EE_SCREENS_SE_TRIGGER_SCREEN_NAME_LABEL as i64, 0x7dc);
    assert_eq!(EE_SCREENS_SE_TRIGGER_SCREEN_DUPLICATE_BUTTON as i64, 0x7d6);
    assert_eq!(EE_SCREENS_SE_TRIGGER_SCREEN_DELETE_BUTTON as i64, 0x7d7);
    assert_eq!(c("kControlTriggerRangeChanged"), 0x3e8);
    assert_eq!(c("kControlTriggerChecked"), 0x3e9);
    assert_eq!(c("kControlTriggerSelectionChanged"), 0x3eb);
    assert_eq!(c("kControlTriggerButtonUp"), 0x3f0);
    assert_eq!(c("kTextAlignLeft"), 0);
    assert_eq!(c("kTextAlignCenter"), 1);
    assert_eq!(c("kTextAlignRight"), 2);
    assert_eq!(EE_SCENARIO_SM_EFFECT_NONE as i64, 0);
    assert_eq!(EE_SCENARIO_SM_EFFECT_OBJECT as i64, 1);
    assert_eq!(EE_SCENARIO_SM_EFFECT_PLAYER as i64, 2);
    assert_eq!(EE_SCENARIO_SM_EFFECT_TRIGGER as i64, 3);
    assert_eq!(EE_SCENARIO_SM_EFFECT_MEDIA as i64, 4);
    assert_eq!(EE_SCENARIO_SM_EFFECT_GAME as i64, 5);
    assert_eq!(EE_SCENARIO_SM_EFFECT_AI as i64, 6);
}

/// Once EESETSEffect.cpp's static_assert "editor_conditions.rs's values".
#[test]
fn eesetseffect_cpp_editor_conditions_rs_s_values() {
    assert_eq!(EE_SCENARIO_SM_CONDITION_ALWAYS_TRUE as i64, 0);
    assert_eq!(EE_SCENARIO_SM_CONDITION_PLAYER_ATTRIBUTE as i64, 2);
    assert_eq!(EE_SCENARIO_SM_CONDITION_PLAYER_TECHNOLOGY as i64, 3);
    assert_eq!(EE_SCENARIO_SM_CONDITION_TRIGGER_ATTRIBUTE as i64, 4);
    assert_eq!(EE_SCENARIO_SM_CONDITION_PLAYER_DIPLOMACY as i64, 5);
    assert_eq!(EE_SCENARIO_SM_CONDITION_GAME_ATTRIBUTE as i64, 6);
    assert_eq!(EE_SCENARIO_SM_CONDITION_CHAT_MESSAGE_CONTAINS as i64, 7);
    assert_eq!(EE_SCREENS_SE_TS_CONDITION_TITLE_LABEL as i64, 0x8fc);
    assert_eq!(EE_SCREENS_SE_TS_CONDITION_TYPE_COMBO_BOX as i64, 0x8fd);
    assert_eq!(c("kControlTriggerUnchecked"), 0x3ea);
    assert_eq!(c("kControlTriggerEnterPressed"), 0x3f1);
}

/// Once EESETSEffect.cpp's static_assert "editor_trigx.rs's values".
#[test]
fn eesetseffect_cpp_editor_trigx_rs_s_values() {
    assert_eq!(c("IDS_TRIGGER_SCREEN_TAB_OBJECT"), 30302);
    assert_eq!(c("IDS_AREA"), 30303);
    assert_eq!(c("kMovementAir"), 1);
    assert_eq!(c("kMovementSea"), 4);
    assert_eq!(c("kUnitCategoryProjectile"), 9);
    assert_eq!(c("kUnitCategoryMissile"), 0xc);
}

/// Once EESETSObject.cpp's static_assert "the strings".
#[test]
fn eesetsobject_cpp_the_strings() {
    assert_eq!(EE_SCREENS_SEOB_IDS_ANY_CLASS as i64, c("IDS_ANY_CLASS"));
    assert_eq!(EE_SCREENS_SEOB_IDS_NONE as i64, c("IDS_NONE"));
    assert_eq!(EE_SCREENS_SEOB_IDS_SE_UNIT_GROUP_UNITS as i64, c("IDS_SE_UNIT_GROUP_UNITS"));
    assert_eq!(EE_SCREENS_SEOB_IDS_FAMILY as i64, c("IDS_FAMILY"));
    assert_eq!(EE_SCREENS_SEOB_IDS_SELECT_ON_MAP as i64, c("IDS_SELECT_ON_MAP"));
    assert_eq!(EE_SCREENS_SEOB_IDS_SELECT_ON_MAP as i64 + 1, c("IDS_OBJECT_SPECIFICATION"));
    assert_eq!(EE_SCREENS_SEOB_IDS_SELECT_ON_MAP as i64 + 2, c("IDS_TRIGGER_EDITOR_OWNER"));
    assert_eq!(EE_SCREENS_SEOB_IDS_SELECT_ON_MAP as i64 + 3, c("IDS_CLASS"));
    assert_eq!(EE_SCREENS_SEOB_IDS_SELECT_ON_MAP as i64 + 5, c("IDS_TRIGGER_EDITOR_MIN"));
    assert_eq!(EE_SCREENS_SEOB_IDS_SELECT_ON_MAP as i64 + 6, c("IDS_TRIGGER_EDITOR_MAX"));
    assert_eq!(EE_SCREENS_SEOB_IDS_SELECT_ON_MAP as i64 + 7, c("IDS_DYNAMIC"));
    assert_eq!(EE_SCREENS_SEOB_IDS_SELECT_ON_MAP as i64 + 8, c("IDS_IN_AREA"));
    assert_eq!(EE_SCREENS_SEOB_IDS_SELECT_ON_MAP as i64 + 9, c("IDS_VISIBLE_BY"));
    assert_eq!(EE_SCREENS_SEOB_IDS_SELECT_ON_MAP as i64 + 10, c("IDS_HAS_STATE"));
    assert_eq!(EE_SCREENS_SEOB_IDS_SELECT_ON_MAP as i64 + 11, c("IDS_SELECTED_BY"));
    assert_eq!(EE_SCREENS_SEOB_IDS_SELECT_ON_MAP as i64 + 12, c("IDS_HAS_ATTRIBUTE"));
    assert_eq!(EE_SCREENS_SEOB_IDS_UNIT_SELECTED_LIST as i64, c("IDS_UNIT_SELECTED_LIST"));
    assert_eq!(EE_SCREENS_SEOB_IDS_TRIGGER_EDITOR_USE_OBJECT as i64, c("IDS_TRIGGER_EDITOR_USE_OBJECT"));
    assert_eq!(EE_SCREENS_SEOB_IDS_TRIGGER_PROXIMITY_FIRST as i64, c("IDS_TRIGGER_PROXIMITY_FIRST"));
    assert_eq!(EE_SCREENS_SEOB_IDS_TRIGGER_ATTRIBUTE_FIRST as i64, c("IDS_TRIGGER_ATTRIBUTE_FIRST"));
}

/// Once EESETSObject.cpp's static_assert "the ids".
#[test]
fn eesetsobject_cpp_the_ids() {
    assert_eq!(EE_SCREENS_SEOB_AREA_CHECK as i64, EE_SCREENS_SE_TS_OBJECT_AREA_CHECK as i64);
    assert_eq!(EE_SCREENS_SEOB_NEAR_CHECK as i64, EE_SCREENS_SE_TS_OBJECT_NEAR_CHECK as i64);
    assert_eq!(EE_SCREENS_SEOB_VISIBLE_BY_CHECK as i64, EE_SCREENS_SE_TS_OBJECT_VISIBLE_BY_CHECK as i64);
    assert_eq!(EE_SCREENS_SEOB_HAS_STATE_CHECK as i64, EE_SCREENS_SE_TS_OBJECT_HAS_STATE_CHECK as i64);
    assert_eq!(EE_SCREENS_SEOB_SELECTED_BY_CHECK as i64, EE_SCREENS_SE_TS_OBJECT_SELECTED_BY_CHECK as i64);
    assert_eq!(EE_SCREENS_SEOB_SELECT_UNITS as i64, EE_SCREENS_SE_TS_OBJECT_SELECT_UNITS as i64);
}

/// Once EESETSTrigger.cpp's static_assert "the strings".
#[test]
fn eesetstrigger_cpp_the_strings() {
    assert_eq!(EE_SCREENS_SETR_IDS_SE_REMOVE as i64, c("IDS_SE_REMOVE"));
    assert_eq!(EE_SCREENS_SETR_IDS_CONDITION as i64, c("IDS_CONDITION"));
    assert_eq!(EE_SCREENS_SETR_IDS_EFFECT as i64, c("IDS_EFFECT"));
    assert_eq!(EE_SCREENS_SETR_IDS_SECONDS as i64, c("IDS_SECONDS"));
    assert_eq!(EE_SCREENS_SETR_IDS_IF as i64, c("IDS_IF"));
    assert_eq!(EE_SCREENS_SETR_IDS_IF as i64 + 1, c("IDS_THEN"));
    assert_eq!(EE_SCREENS_SETR_IDS_IF as i64 + 2, c("IDS_NOT"));
    assert_eq!(EE_SCREENS_SETR_IDS_IF as i64 + 3, c("IDS_DELAY"));
    assert_eq!(EE_SCREENS_SETR_IDS_IF as i64 + 4, c("IDS_AND"));
    assert_eq!(EE_SCREENS_SETR_IDS_IF as i64 + 5, c("IDS_OR"));
    assert_eq!(EE_SCREENS_SETR_IDS_IF as i64 + 6, c("IDS_TRIGGER_STATEMENT"));
    assert_eq!(EE_SCREENS_SETR_IDS_IF as i64 + 7, c("IDS_TRIGGER_IS_ON"));
    assert_eq!(EE_SCREENS_SETR_IDS_IF as i64 + 8, c("IDS_CONDITIONS_TRUE_FOR"));
    assert_eq!(EE_SCREENS_SETR_IDS_IF as i64 + 9, c("IDS_LOOPING"));
    assert_eq!(EE_SCREENS_SETR_IDS_INCLUDE_IN_CINEMATIC as i64, c("IDS_INCLUDE_IN_CINEMATIC"));
}

/// Once EESETSTrigger.cpp's static_assert "the operators".
#[test]
fn eesetstrigger_cpp_the_operators() {
    assert_eq!(EE_SCREENS_SETR_END as i64, EE_SCENARIO_SM_TRIGGER_OPERATOR_END as i64);
    assert_eq!(EE_SCREENS_SETR_AND as i64, EE_SCENARIO_SM_TRIGGER_OPERATOR_AND as i64);
    assert_eq!(EE_SCREENS_SETR_OR as i64, EE_SCENARIO_SM_TRIGGER_OPERATOR_OR as i64);
}

/// Once EESETriggerScreen.cpp's static_assert "the strings".
#[test]
fn eesetriggerscreen_cpp_the_strings() {
    assert_eq!(EE_SCREENS_SETS_IDS_NEW as i64, c("IDS_TRIGGER_EDITOR_NEW"));
    assert_eq!(EE_SCREENS_SETS_IDS_NEW as i64 + 1, c("IDS_DUPLICATE"));
    assert_eq!(EE_SCREENS_SETS_IDS_NEW as i64 + 2, c("IDS_TRIGGER_EDITOR_DELETE"));
    assert_eq!(EE_SCREENS_SETS_IDS_NEW as i64 + 3, c("IDS_TRIGGER_EDITOR_NAME"));
    assert_eq!(EE_SCREENS_SETS_IDS_DESCRIPTION as i64, c("IDS_DESCRIPTION_76A8"));
}

/// Once EESETriggerScreen.cpp's static_assert "the sub-tabs".
#[test]
fn eesetriggerscreen_cpp_the_sub_tabs() {
    assert_eq!(EE_SCREENS_SETS_TAB_FIRST as i64, EE_SCREENS_SE_TRIGGER_TAB_TRIGGER as i64);
    assert_eq!(EE_SCREENS_SE_TRIGGER_TAB_AREA as i64, EE_SCREENS_SE_TRIGGER_TAB_TRIGGER as i64 + 4);
    assert_eq!(EE_SCREENS_SETS_GROUPS[0] as i64, EE_SCREENS_SE_ENTITY_GROUP_TRIGGER as i64);
    assert_eq!(EE_SCREENS_SETS_GROUPS[1] as i64, EE_SCREENS_SE_ENTITY_GROUP_CONDITION as i64);
    assert_eq!(EE_SCREENS_SETS_GROUPS[2] as i64, EE_SCREENS_SE_ENTITY_GROUP_EFFECT as i64);
    assert_eq!(EE_SCREENS_SETS_GROUPS[3] as i64, EE_SCREENS_SE_ENTITY_GROUP_OBJECT as i64);
    assert_eq!(EE_SCREENS_SETS_GROUPS[4] as i64, EE_SCREENS_SE_ENTITY_GROUP_AREA as i64);
}

/// Once EESEUnit.cpp's static_assert "the strings".
#[test]
fn eeseunit_cpp_the_strings() {
    assert_eq!(EE_SCREENS_SEUN_IDS_ALPHA as i64, c("IDS_ALPHA"));
    assert_eq!(EE_SCREENS_SEUN_IDS_ALPHA as i64 + 1, c("IDS_SE_EPOCH"));
    assert_eq!(EE_SCREENS_SEUN_IDS_ALPHA as i64 + 2, c("IDS_FAMILY"));
    assert_eq!(EE_SCREENS_SEUN_IDS_CARGO as i64, c("IDS_CARGO"));
    assert_eq!(EE_SCREENS_SEUN_IDS_SORT as i64, c("IDS_SORT"));
    assert_eq!(EE_SCREENS_SEUN_IDS_WORLD_PLAYER as i64, c("IDS_WORLD_PLAYER"));
}

/// Once EESEUnit.cpp's static_assert "the palettes".
#[test]
fn eeseunit_cpp_the_palettes() {
    assert_eq!(EE_SCREENS_SEUN_PALETTE_LAND as i64, EE_SCREENS_SE_UNIT_PALETTE_LAND_UNITS as i64);
    assert_eq!(EE_SCREENS_SEUN_PALETTE_AIR as i64, EE_SCREENS_SE_UNIT_PALETTE_AIR_UNITS as i64);
    assert_eq!(EE_SCREENS_SEUN_PALETTE_NAVAL as i64, EE_SCREENS_SE_UNIT_PALETTE_NAVAL_UNITS as i64);
    assert_eq!(EE_SCREENS_SEUN_PALETTE_STRUCTURES as i64, EE_SCREENS_SE_UNIT_PALETTE_STRUCTURES as i64);
    assert_eq!(EE_SCREENS_SEUN_PALETTE_HEROES as i64, EE_SCREENS_SE_UNIT_PALETTE_HEROES as i64);
    assert_eq!(EE_SCREENS_SEUN_PALETTE_RESOURCES as i64, EE_SCREENS_SE_UNIT_PALETTE_RESOURCES as i64);
}

/// Once EEUIFSEElevation.cpp's static_assert "editor_elevation.rs's ids".
#[test]
fn eeuifseelevation_cpp_editor_elevation_rs_s_ids() {
    assert_eq!(EE_SCREENS_SE_ELEVATION_PAINT as i64, 0xa1);
    assert_eq!(EE_SCREENS_SE_ELEVATION_ELEVATION_SLIDER as i64, 0x9d);
    assert_eq!(EE_SCREENS_SE_ELEVATION_STRENGTH_SLIDER as i64, 0xa2);
    assert_eq!(EE_SCREENS_SE_ELEVATION_RAISE_BUTTON as i64, 0x96);
    assert_eq!(EE_SCREENS_SE_ELEVATION_LEVEL_BUTTON as i64, 0xa0);
    assert_eq!(EE_SCREENS_SE_ELEVATION_STRENGTH_LABEL as i64, 0x9f);
    assert_eq!(c("IDS_PAINT_HILLS"), 30465);
    assert_eq!(c("IDS_SE_MAP_TERRAIN"), 20018);
}

/// Once EEUIFSEMap.cpp's static_assert "editor_map.rs's ids".
#[test]
fn eeuifsemap_cpp_editor_map_rs_s_ids() {
    assert_eq!(EE_SCREENS_SE_MAP_ELEVATION_FILE as i64, 0x75);
    assert_eq!(EE_SCREENS_SE_MAP_WIDTH as i64, 0x6b);
    assert_eq!(EE_SCREENS_SE_MAP_LAND_CUTOFF as i64, 0x78);
    assert_eq!(EE_SCREENS_SE_MAP_DEFAULT_TERRAIN_LABEL as i64, 0x64);
    assert_eq!(EE_SCREENS_SE_MAP_WATER_CUTOFF as i64, 0x7d);
}

/// Once EEUIFSETRBPaintType.cpp's static_assert "ee/EESEARust.h's event kinds".
#[test]
fn eeuifsetrbpainttype_cpp_ee_eesearust_h_s_event_kinds() {
    assert_eq!(EE_SCREENS_SEE_PER_PLAYER_LABEL_ID as i64, EE_SCREENS_SE_RB_UNIT_TYPES_PER_PLAYER_COUNT_LABEL as i64);
}

/// Once EEUIFSETerrain.cpp's static_assert "editor_terrain.rs's ids".
#[test]
fn eeuifseterrain_cpp_editor_terrain_rs_s_ids() {
    assert_eq!(c("IDS_SE_TERRAIN"), 20508);
    assert_eq!(c("IDS_BULLDOZER"), 20510);
    assert_eq!(c("IDS_TERRAIN_TYPE"), 20511);
    assert_eq!(c("IDS_TERRAIN_COLOR"), 20542);
    assert_eq!(EE_SCREENS_SE_TERRAIN_TERRAIN_BUTTON as i64, 0xc8);
    assert_eq!(EE_SCREENS_SE_TERRAIN_BITMAP as i64, 0xc9);
    assert_eq!(EE_SCREENS_SE_TERRAIN_MOUNTAINS_BUTTON as i64, 0xca);
    assert_eq!(EE_SCREENS_SE_TERRAIN_TERRAIN_TYPE_LABEL as i64, 0xcb);
    assert_eq!(EE_SCREENS_SE_TERRAIN_TERRAIN_TYPE as i64, 0xcc);
    assert_eq!(EE_SCREENS_SE_TERRAIN_TERRAIN_COLOR_TYPE_LABEL as i64, 0xce);
    assert_eq!(EE_SCREENS_SE_TERRAIN_TERRAIN_COLOR_TYPE as i64, 0xcf);
    assert_eq!(c("IDS_BRUSH_SIZE"), 20500);
    assert_eq!(c("IDS_SE_TINY"), 20501);
    assert_eq!(c("IDS_SE_HUGE"), 20505);
}

/// Once EEUIFScenarioEditor.cpp's static_assert "editor_form.rs's numbers".
#[test]
fn eeuifscenarioeditor_cpp_editor_form_rs_s_numbers() {
    assert_eq!(EE_SCENARIO_SM_CONDITION_OBJECT as i64, 1);
    assert_eq!(EE_SCENARIO_SM_CONDITION_PLAYER_ATTRIBUTE as i64, 2);
    assert_eq!(EE_SCENARIO_SM_EFFECT_NONE as i64, 0);
    assert_eq!(EE_SCENARIO_SM_EFFECT_OBJECT as i64, 1);
    assert_eq!(EE_SCENARIO_SM_EFFECT_PLAYER as i64, 2);
    assert_eq!(EE_SCENARIO_SM_OBJECT_ACTION_CLASS_NAME as i64, 10);
    assert_eq!(EE_SCENARIO_SM_OBJECT_ACTION_CLASS_ATTRIBUTE as i64, 12);
    assert_eq!(EE_SCENARIO_SM_PLAYER_ACTION_MODIFY_TECH_TREE as i64, 5);
    assert_eq!(EE_SCENARIO_SM_TRIGGER_OPERATOR_AND as i64, 1);
    assert_eq!(EE_SCENARIO_SM_TRIGGER_OPERATOR_OR as i64, 2);
    assert_eq!(EE_SCREENS_SE_TOOLBAR_BUTTON_EXIT as i64, 15);
    assert_eq!(c("IDS_CIVILIZATION_BUILDER_TOOL"), EE_SCREENS_SEFM_IDS_CIVILIZATION_BUILDER_TOOL as i64);
}

/// The constructor's music file name (`ee_screens_sefm_construct`).
#[test]
fn the_form_music_none() {
    assert_eq!(EE_SCREENS_SEFM_IDS_SE_MAP_NONE as i64, c("IDS_SE_MAP_NONE"));
}
