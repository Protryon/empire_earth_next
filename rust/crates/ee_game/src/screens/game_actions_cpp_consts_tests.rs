//! The C++ constants the game's actions' Rust numbers equal (once `Actions/EEARust.cpp`'s `static_assert`s): the
//! kinds (`ee/EEARust.h`'s enum), the message types (`ee/EEMessageID.h`), the unit types, categories, families and
//! kinds (`ee/EEUnitClassIDs.h`), the graphic effects, technologies, slots, key modifiers and the resource none, the
//! string ids (`ee/EEStringIDs.h`), read from the headers themselves.

// the Rust numbers' types differ (u32, i32, i64): each is widened to compare
#![allow(clippy::unnecessary_cast)]

use std::collections::HashMap;
use std::sync::OnceLock;

use crate::screens::game_actions::*;
use crate::screens::game_actions_host::*;

/// The headers the C++ names come from.
const HEADERS: [&str; 10] = [
    "include/ee/EEMessageID.h",
    "include/ee/EEUnitClassIDs.h",
    "include/rust/sim/unit_ids.h",
    "include/ee/EEGraphicEffect.h",
    "include/ee/EETechTreeTechID.h",
    "include/rust/net/msgid.h",
    "include/rust/base/lle_enums.h",
    "include/ee/EEResourceType.h",
    "include/ee/EEStringIDs.h",
    "include/ee/EEARust.h",
];

fn text(h: &str) -> String {
    let path = format!("{}/../../../{h}", env!("CARGO_MANIFEST_DIR"));
    String::from_utf8_lossy(&std::fs::read(&path).unwrap_or_else(|e| panic!("{path}: {e}"))).into_owned()
}

fn number(v: &str) -> Option<i64> {
    match v.strip_prefix("0x") {
        Some(h) => i64::from_str_radix(h, 16).ok(),
        None => v.parse().ok(),
    }
}

/// A C++ constant's value: `#define NAME value` or an enumerator `NAME = value,` of the headers, or one of
/// `ee/EEARust.h`'s kinds (an enum counting up from `kEEAChat = 1`).
fn c(name: &str) -> i64 {
    static TABLE: OnceLock<HashMap<String, i64>> = OnceLock::new();
    let table = TABLE.get_or_init(|| {
        let mut t = HashMap::new();
        for h in HEADERS {
            for line in text(h).lines() {
                let words: Vec<&str> = line.split_whitespace().collect();
                let (n, v) = match words.as_slice() {
                    ["#define", n, v, ..] => (*n, *v),
                    [n, "=", v, ..] => (*n, v.trim_end_matches(',')),
                    _ => continue,
                };
                if let Some(v) = number(v) {
                    t.entry(n.to_string()).or_insert(v);
                }
            }
        }
        // the kinds: the enum after "// the kinds" in ee/EEARust.h
        let h = text("include/ee/EEARust.h");
        let start = h.find("// the kinds").expect("the kinds");
        let body = &h[start..];
        let body = &body[body.find('{').unwrap() + 1..body.find("};").unwrap()];
        let mut next = 0;
        for item in body.lines().map(|l| l.split("//").next().unwrap()).collect::<Vec<_>>().join(" ").split(',') {
            let item = item.trim();
            if item.is_empty() {
                continue;
            }
            let (n, v) = match item.split_once('=') {
                Some((n, v)) => (n.trim(), number(v.trim()).unwrap()),
                None => (item, next),
            };
            t.insert(n.to_string(), v);
            next = v + 1;
        }
        t
    });
    *table.get(name).unwrap_or_else(|| panic!("{name} not in the headers"))
}

/// Once the host's static_assert "ee/EEARust.h's kinds are game_actions.rs's".
#[test]
fn eearust_h_s_kinds_are_game_actions_rs_s() {
    assert_eq!(EE_SCREENS_GA_CHAT as i64, c("kEEAChat"));
    assert_eq!(EE_SCREENS_GA_BOOT as i64, c("kEEABoot"));
    assert_eq!(EE_SCREENS_GA_BUILD as i64, c("kEEABuild"));
    assert_eq!(EE_SCREENS_GA_CALAMITY as i64, c("kEEACalamity"));
    assert_eq!(EE_SCREENS_GA_CENTER_CAMERA as i64, c("kEEACenterCamera"));
    assert_eq!(EE_SCREENS_GA_JOIN as i64, c("kEEAJoin"));
    assert_eq!(EE_SCREENS_GA_LASSO as i64, c("kEEALasso"));
    assert_eq!(EE_SCREENS_GA_MINI_MAP as i64, c("kEEAMiniMap"));
    assert_eq!(EE_SCREENS_GA_MOUSE_CLICK as i64, c("kEEAMouseClick"));
    assert_eq!(EE_SCREENS_GA_PLAYER_EVENT as i64, c("kEEAPlayerEvent"));
    assert_eq!(EE_SCREENS_GA_REMOVE_PLAYER as i64, c("kEEARemovePlayer"));
    assert_eq!(EE_SCREENS_GA_RESET_PLAYERS as i64, c("kEEAResetPlayers"));
    assert_eq!(EE_SCREENS_GA_SAVE_GAME as i64, c("kEEASaveGame"));
    assert_eq!(EE_SCREENS_GA_SCROLL as i64, c("kEEAScroll"));
    assert_eq!(EE_SCREENS_GA_MIGRATION as i64, c("kEEAMigration"));
    assert_eq!(EE_SCREENS_GA_UPGRADE as i64, c("kEEAUpgrade"));
    assert_eq!(EE_SCREENS_GA_PAUSE as i64, c("kEEAPause"));
    assert_eq!(EE_SCREENS_GA_QUICK as i64, c("kEEAQuick"));
    assert_eq!(EE_SCREENS_GA_EDITOR_QUICK as i64, c("kEEAEditorQuick"));
    assert_eq!(EE_SCREENS_GA_EDITOR_CLICK as i64, c("kEEAEditorClick"));
    assert_eq!(EE_SCREENS_GA_EDITOR_LASSO as i64, c("kEEAEditorLasso"));
    assert_eq!(EE_SCREENS_GA_TRIGGER_CLICK as i64, c("kEEATriggerClick"));
    assert_eq!(EE_SCREENS_GA_TRIGGER_LASSO as i64, c("kEEATriggerLasso"));
    assert_eq!(EE_SCREENS_GA_LOAD_TEXT as i64, c("kEEALoadText"));
    assert_eq!(EE_SCREENS_GA_EDITOR_PAINT as i64, c("kEEAEditorPaint"));
    assert_eq!(EE_SCREENS_GA_EDITOR_WHOLE as i64, c("kEEAEditorWhole"));
    assert_eq!(EE_SCREENS_GA_EDITOR_UNDO as i64, c("kEEAEditorUndo"));
    assert_eq!(EE_SCREENS_GA_EDITOR_REDO as i64, c("kEEAEditorRedo"));
}

/// Once the host's static_asserts "game_actions.rs's message types" and "game_actions.rs's world constants".
#[test]
fn game_actions_rs_s_message_types_and_world_constants() {
    assert_eq!(EE_SCREENS_GA_MSG_SELECT_UNIT as i64, c("kMessageSelectUnit"));
    assert_eq!(EE_SCREENS_GA_MSG_ADD_UNIT as i64, c("kMessageAddUnitToSelectedGroup"));
    assert_eq!(EE_SCREENS_GA_MSG_UNSELECT_ALL as i64, c("kMessageUnselectAllUnits"));
    assert_eq!(EE_SCREENS_GA_MSG_ATTACK as i64, c("kMessageAttackUnit"));
    assert_eq!(EE_SCREENS_GA_MSG_PATH_TO as i64, c("kMessagePathTo"));
    assert_eq!(EE_SCREENS_GA_MSG_SELECT_UNITS as i64, c("kMessageSelectUnits"));
    assert_eq!(EE_SCREENS_GA_MSG_ADD_UNITS as i64, c("kMessageAddUnitsToSelectedGroup"));
    assert_eq!(EE_SCREENS_GA_MSG_ADD_REMOVE_UNITS as i64, c("kMessageAddAndRemoveUnitsFromSelectedGroup"));
    assert_eq!(EE_SCREENS_GA_MSG_GATHER as i64, c("kMessageGatherResource"));
    assert_eq!(EE_SCREENS_GA_MSG_BUILD_UNIT as i64, c("kMessageBuildUnit"));
    assert_eq!(EE_SCREENS_GA_MSG_REPAIR as i64, c("kMessageRepairUnit"));
    assert_eq!(EE_SCREENS_GA_MSG_BUILD_WALL as i64, c("kMessageBuildWall"));
    assert_eq!(EE_SCREENS_GA_MSG_DROPOFF as i64, c("kMessageDropoffResource"));
    assert_eq!(EE_SCREENS_GA_MSG_RALLY as i64, c("kMessageSetRallyGoal"));
    assert_eq!(EE_SCREENS_GA_MSG_LOAD_TRANSPORT as i64, c("kMessageLoadTransport"));
    assert_eq!(EE_SCREENS_GA_MSG_PATROL_GROUP as i64, c("kMessageSelectPatrolGroup"));
    assert_eq!(EE_SCREENS_GA_MSG_CAST_CALAMITY as i64, c("kMessageCastCalamity"));
    assert_eq!(EE_SCREENS_GA_MSG_ALWAYS_ADD_UNIT as i64, c("kMessageAlwaysAddUnitToSelectedGroup"));
    assert_eq!(EE_SCREENS_GA_MSG_IMPROVE as i64, c("kMessageImproveUnit"));
    assert_eq!(EE_SCREENS_GA_MSG_BUILD_GATE as i64, c("kMessageBuildGate"));
    assert_eq!(EE_SCREENS_GA_MSG_REMOVE_IMPROVEMENT as i64, c("kMessageRemoveImprovement"));
    assert_eq!(EE_SCREENS_GA_MSG_SMART_FARMERS as i64, c("kMessageSmartFarmers"));
    assert_eq!(EE_SCREENS_GA_MSG_RETURN_TO_BASE as i64, c("kMessageReturnToBase"));
    assert_eq!(EE_SCREENS_GA_MSG_HEAL as i64, c("kMessageHealUnit"));
    assert_eq!(EE_SCREENS_GA_TYPE_TOWN_CENTER as i64, c("kUnitTypeTownCenter"));
    assert_eq!(EE_SCREENS_GA_TYPE_DOCK as i64, c("kUnitTypeDock"));
    assert_eq!(EE_SCREENS_GA_TYPE_CAPITOL as i64, c("kUnitTypeCapitol"));
    assert_eq!(EE_SCREENS_GA_TYPE_FARM as i64, c("kUnitTypeFarm"));
    assert_eq!(EE_SCREENS_GA_TYPE_SETTLEMENT as i64, c("kUnitTypeSettlement"));
    assert_eq!(EE_SCREENS_GA_TYPE_GRANARY as i64, c("kUnitTypeGranary"));
    assert_eq!(EE_SCREENS_GA_TYPE_NAVY_YARD as i64, c("kUnitTypeNavyYard"));
    assert_eq!(EE_SCREENS_GA_TYPE_AIRPORT as i64, c("kUnitTypeAirport"));
    assert_eq!(EE_SCREENS_GA_TYPE_CAMERA_OBJECT as i64, c("kUnitTypeCameraObject"));
    assert_eq!(EE_SCREENS_GA_TYPE_PATH_POINT as i64, c("kUnitTypePathPoint"));
    assert_eq!(EE_SCREENS_GA_TYPE_GRANARY_IMPROVED as i64, c("kUnitTypeGranaryImproved"));
    assert_eq!(EE_SCREENS_GA_TYPE_CP_MISSILE_BASE as i64, c("kUnitTypeCpMissileBase"));
    assert_eq!(EE_SCREENS_GA_CATEGORY_LAND_CITIZEN as i64, c("kUnitCategoryLandCitizen"));
    assert_eq!(EE_SCREENS_GA_CATEGORY_NAVAL_BUILDING as i64, c("kUnitCategoryNavalBuilding"));
    assert_eq!(EE_SCREENS_GA_CATEGORY_NAVAL_CITIZEN as i64, c("kUnitCategoryNavalCitizen"));
    assert_eq!(EE_SCREENS_GA_CATEGORY_SPACE_BUILDING as i64, c("kUnitCategorySpaceBuilding"));
    assert_eq!(EE_SCREENS_GA_FAMILY_FISH as i64, c("kUnitFamilyFish"));
    assert_eq!(EE_SCREENS_GA_KIND_CITIZEN as i64, c("kUnitKindCitizen"));
    assert_eq!(EE_SCREENS_GA_KIND_BUILDING as i64, c("kUnitKindBuilding"));
    assert_eq!(EE_SCREENS_GA_KIND_NAVAL_CITIZEN as i64, c("kUnitKindNavalCitizen"));
    assert_eq!(EE_SCREENS_GA_KIND_HELICOPTER_TRANSPORT as i64, c("kUnitKindHelicopterTransport"));
    assert_eq!(EE_SCREENS_GA_KIND_LAND_TRANSPORT as i64, c("kUnitKindLandTransport"));
    assert_eq!(EE_SCREENS_GA_KIND_NAVAL_TRANSPORT as i64, c("kUnitKindNavalTransport"));
    assert_eq!(EE_SCREENS_GA_KIND_FORTRESS as i64, c("kUnitKindFortress"));
    assert_eq!(EE_SCREENS_GA_KIND_SPACE_TRANSPORT as i64, c("kUnitKindSpaceTransport"));
    assert_eq!(EE_SCREENS_GA_EFFECT_CLICK_UNIT as i64, c("kGraphicEffectCommandConfirmationMouseClickUnit"));
    assert_eq!(EE_SCREENS_GA_EFFECT_CLICK_NO_UNIT as i64, c("kGraphicEffectCommandConfirmationMouseClickNoUnit"));
    assert_eq!(EE_SCREENS_GA_TECH_CONQUISTADORS_SPELL as i64, c("kTechConquistadorsSpell"));
    assert_eq!(EE_SCREENS_GA_TECH_NONE as i64, c("kTechNone"));
    assert_eq!(EE_SCREENS_GA_SLOT_SERVER as i64, c("kSlotServer"));
    assert_eq!(EE_SCREENS_GA_SLOT_NONE as i64, c("kSlotNone"));
    assert_eq!(1, c("kModifierShift"));
    assert_eq!(2, c("kModifierControl"));
    assert_eq!(0, c("kResourceNone"));
    assert_eq!(EE_SCREENS_GA_IDS_PLAYER_NAME_IS_INVALID as i64, c("IDS_PLAYER_NAME_IS_INVALID"));
    assert_eq!(EE_SCREENS_GA_IDS_GAME_NAME_IS_INVALID as i64, c("IDS_GAME_NAME_IS_INVALID"));
    assert_eq!(EE_SCREENS_GA_IDS_GAME_NAME_EXISTS as i64, c("IDS_GAME_NAME_ALREADY_EXISTS_ON_SUBNET"));
}

/// The string ids game_actions_host.rs's sequences show.
#[test]
fn game_actions_host_rs_s_string_ids() {
    assert_eq!(EE_SCREENS_GA_IDS_HOST_IS_GONE, c("IDS_HOST_IS_GONE"));
    assert_eq!(EE_SCREENS_GA_IDS_YOU_ARE_BEING_PROMOTED_TO_HOST, c("IDS_YOU_ARE_BEING_PROMOTED_TO_HOST"));
    assert_eq!(EE_SCREENS_GA_IDS_ATTEMPTING_TO_CONNECT_TO_NEW_HOST, c("IDS_ATTEMPTING_TO_CONNECT_TO_NEW_HOST"));
}
