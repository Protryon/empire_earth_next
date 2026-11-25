//! The C++ constants the Graphics Thread's and the Map's Rust bodies take as numbers (once `static_assert`s in
//! EERenderObject.cpp, EEEffectHost.cpp, EEMiniMap.cpp, EEMap.cpp and EETile.cpp: render_object.rs, effect_host.rs,
//! minimap.rs, map.rs, tile.rs), read from the headers themselves.

use std::collections::HashMap;
use std::sync::OnceLock;

/// The headers the C++ names come from.
const HEADERS: [&str; 8] = [
    "include/ee/EEUnitClassIDs.h",
    "include/rust/sim/unit_ids.h",
    "include/rust/sim/tech_consts.h",
    "include/rust/render/gestate.h",
    "include/ee/EEMap.h",
    "include/ee/EEUnitGoal.h",
    "include/rust/sim/tech_tree_ids.h",
    "include/ee/EEResourceType.h",
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

fn check(pairs: &[(&str, i64)]) {
    for (name, value) in pairs {
        assert_eq!(c(name), *value, "{name}");
    }
}

/// Once EERenderObject.cpp's static_assert "render_object.rs constants".
#[test]
fn render_object_rs_constants() {
    check(&[
        ("kUnitTypeRoboticFarm", 0x33a),
        ("kUnitTypeSpaceTurret", 0x302),
        ("kUnitTypeOrbitalSpaceStation", 0x31f),
        ("kAreaEffectMorale", 2),
        ("kAreaEffectMoraleFromBuildings", 5),
        ("kAreaEffectMoraleFromMilitaryHeroes", 6),
        ("kStageOpModulate", 2),
    ]);
    assert_eq!(ee_render::gfx::lighting::EeGfxLighting::DimUnit as i32, 10);
}

/// Once EEEffectHost.cpp's static_assert "gamecpp effect_host.rs".
#[test]
fn effect_host_rs_constants() {
    check(&[
        ("kTerrainDeepWater", 1),
        ("kTerrainShallowWater", 10),
        ("kUnitCategoryGate", 0x19),
        ("kUnitTypeWallSpace", 0x2fe),
        ("kUnitTypeGateSpace", 0x2ff),
        ("kUnitTypeTownCenter", 0x23),
        ("kUnitTypeCapitol", 0x29),
        ("kUnitTypeResGiraffe", 0x124),
        ("kUnitTypeResHorse", 0x175),
        ("kGoalLocation", 1),
        ("kGoalAttackUnit", 2),
        ("kGoalUnloadTransport", 8),
        ("kGoalLoadGarrison", 12),
        ("kTechEpochAtomicWW2", 10),
        ("kPixelFormatA8R8G8B8", 0),
        ("kSurfaceSystemMemory", 4),
    ]);
}

/// Once EEMiniMap.cpp's "gamecpp minimap.rs", EEMap.cpp's "gamecpp map.rs" and EETile.cpp's "gamecpp tile.rs".
#[test]
fn minimap_map_and_tile_rs_constants() {
    check(&[
        ("kUnitCategoryResource", 1),
        ("kUnitTypeResForageBush", 0x37),
        ("kUnitTypeAirport", 0x42),
        ("kResourceStone", 4),
        ("kResourceGold", 5),
        ("kResourceIron", 6),
        ("kUnitTypeCameraObject", 0x167),
        ("kTerrainSpace", 12),
    ]);
}
