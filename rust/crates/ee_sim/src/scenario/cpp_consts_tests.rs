//! The C++ constants the scenario manager's Rust numbers equal (once the Scenario Manager's C++ `static_assert`s): the
//! string ids of the lists' built-ins and of the text import's errors (`ee/EEStringIDs.h`), the effect texts' sound
//! and goal action (`ee/EESoundObject.h`, `ee/EEUnitAction.h`), the object reads' water terrains (`ee/EEMap.h`),
//! read from the headers themselves.

use std::collections::HashMap;
use std::sync::OnceLock;

use super::effect_text::{ACTION_WAIT_FOR_GARRISON_COST, SOUND_TRUMPETS};
use super::lists::EE_SCENARIO_STRING_IDS;
use super::object_reads::{TERRAIN_DEEP_WATER, TERRAIN_SHALLOW_WATER};
use super::text::ERROR_FORMATS;

/// The headers the C++ names come from.
const EE_SCENARIO_CPPC_HEADERS: [&str; 4] =
    ["include/ee/EEStringIDs.h", "include/ee/EESoundObject.h", "include/ee/EEUnitAction.h", "include/ee/EEMap.h"];

/// A C++ constant's value: `#define NAME value` or an enumerator `NAME = value,` of the headers.
fn c(name: &str) -> i64 {
    static TABLE: OnceLock<HashMap<String, i64>> = OnceLock::new();
    let table = TABLE.get_or_init(|| {
        let mut t = HashMap::new();
        for h in EE_SCENARIO_CPPC_HEADERS {
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

#[test]
fn list_built_in_names_are_the_string_ids() {
    let ids = [
        "IDS_COPY",
        "IDS_EXISTS",
        "IDS_ALWAYS_TRUE",
        "IDS_DO_NOTHING",
        "IDS_NONE",
        "IDS_OFF_MAP",
        "IDS_PREVIOUS_LOCATION",
    ];
    for (k, name) in ids.iter().enumerate() {
        assert_eq!(EE_SCENARIO_STRING_IDS[k] as i64, c(name), "{name}");
    }
}

#[test]
fn text_import_errors_are_the_string_ids() {
    let ids = [
        "IDS_IS_INVALID_TOKEN",
        "IDS_IS_INVALID_EFFECT_ID_FOR_TOKEN",
        "IDS_IS_INVALID_EFFECT_TYPE_TO_BE_TOKEN",
        "IDS_TEXT_FILE_EFFECT_NAME_DOES_NOT_MATCH_SCENARIOS",
        "IDS_IS_INVALID_CONDITION_ID_FOR_TOKEN",
        "IDS_IS_INVALID_CONDITION_TYPE_TO_BE_TOKEN",
        "IDS_TEXT_FILE_EFFECT_NAME_DOES_NOT_MATCH_SCENARIOS_2747",
        "IDS_INVALID_PLAYER_NUMBER_FOR_THIS_SCENARIO",
    ];
    for (k, name) in ids.iter().enumerate() {
        assert_eq!(ERROR_FORMATS[k] as i64, c(name), "{name}");
    }
}

#[test]
fn effect_texts_and_object_reads_use_the_game_numbers() {
    assert_eq!(SOUND_TRUMPETS, c("kSoundTrumpets"));
    assert_eq!(ACTION_WAIT_FOR_GARRISON_COST, c("kActionWaitForGarrisonCost"));
    assert_eq!(TERRAIN_DEEP_WATER, c("kTerrainDeepWater"));
    assert_eq!(TERRAIN_SHALLOW_WATER, c("kTerrainShallowWater"));
}
