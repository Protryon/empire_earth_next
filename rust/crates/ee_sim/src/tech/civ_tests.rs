//! The civilization manager's tables, its simulation and the civilization files (civ.rs, civ_apply.rs,
//! civ_file.rs) over the default rules (civbuilder.yaml's and civpowers.yaml's vanilla values) and test hosts.

use crate::tech::civ::*;
use crate::tech::civ_apply::*;
use crate::tech::civ_file::*;
use std::os::raw::{c_char, c_long};
use std::sync::Mutex;

/// The tables are the process's one instance: the tests that use them run one at a time.
static LOCK: Mutex<()> = Mutex::new(());

fn locked() -> std::sync::MutexGuard<'static, ()> {
    let g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    *tables_mut() = CivTables::default();
    g
}

fn category(index: i32, set: i32, cost: i32, levels: &[(f32, f32)]) -> EeTechCivCategory {
    let mut c = EeTechCivCategory {
        mDatabaseID: index + 100,
        mIndex: index,
        mUnitSet: set,
        mCost: cost,
        mNameID: 9000 + index,
        ..Default::default()
    };
    for (k, (v, b)) in levels.iter().enumerate() {
        c.mLevels[k] = EeTechCivLevel { mValue: *v, mBonus: *b };
    }
    c
}

fn bytes_of<T>(records: &[T]) -> Vec<u8> {
    let mut out = (records.len() as u32).to_le_bytes().to_vec();
    for r in records {
        // SAFETY: plain records
        out.extend_from_slice(unsafe {
            std::slice::from_raw_parts(r as *const T as *const u8, std::mem::size_of::<T>())
        });
    }
    out
}

fn name(text: &str) -> [c_char; 100] {
    let mut out = [0 as c_char; 100];
    for (i, b) in text.bytes().enumerate() {
        out[i] = b as c_char;
    }
    out
}

fn power(id: i32, bonus: f32) -> EeTechCivPower {
    EeTechCivPower {
        mID: id,
        mBonus: bonus,
        mTechnology: 7000 + id,
        mMiningLimit: 4,
        mAmount: 12.5,
        ..Default::default()
    }
}

/// 23 powers (ids 0x26..0x3c) and three categories (the second with index 0: skipped)
fn load_standard() {
    let mut t = tables_mut();
    let powers: Vec<EeTechCivPower> = (0..23).map(|i| power(0x26 + i, 0.25 + i as f32)).collect();
    assert_eq!(t.load_powers(&bytes_of(&powers)), CIV_OK);
    let cats = [category(1, 5, 3, &[(0.1, 2.0), (0.2, 3.0)]), category(0, 6, 4, &[]), category(2, 7, 5, &[(0.5, 6.0)])];
    assert_eq!(t.load(&bytes_of(&cats), rules(), 77), CIV_OK);
}

#[test]
fn records_keep_the_cpp_sizes() {
    assert_eq!(std::mem::size_of::<EeTechCivCategory>(), 600);
    assert_eq!(std::mem::size_of::<EeTechCivEntry>(), 120);
    assert_eq!(std::mem::size_of::<EeTechCivPower>(), 152);
    assert_eq!(std::mem::offset_of!(EeTechCivCategory, mLevels), 0x78);
    assert_eq!(std::mem::offset_of!(EeTechCivEntry, mChecksum), 0x6c);
    assert_eq!(std::mem::offset_of!(EeTechCivPower, mAmount), 0x84);
    assert_eq!(std::mem::offset_of!(EeTechCivPower, mUnitType), 0x94);
}

#[test]
fn reader_reads_nothing_past_the_end() {
    let mut r = Reader::new(&[1, 0, 0, 0, 9]);
    assert_eq!(r.u32(), Ok(1));
    let mut e = EeTechCivEntry { mIndex: 4, ..Default::default() };
    assert_eq!(r.record(&mut e), CIV_END_OF_FILE);
    assert_eq!(e, EeTechCivEntry::default());
    assert_eq!(r.u32(), Err(CIV_END_OF_FILE));
    let mut one = [7u8];
    assert_eq!(r.bytes(&mut one), CIV_OK);
    assert_eq!(one, [9]);
}

#[test]
fn load_skips_unindexed_categories_and_appends_the_powers() {
    let _g = locked();
    load_standard();
    let t = tables();
    assert_eq!(t.category_count(), 3);
    assert_eq!(t.powers_category, 2);
    let p = t.category(2).unwrap();
    // the last indexed category's ids plus one; the rules' name, set and level value; the C++'s cost
    assert_eq!((p.mDatabaseID, p.mIndex, p.mNameID, p.mUnitSet, p.mCost), (103, 3, 32850, 102, 77));
    // flaming arrows at level 38: its power's bonus, the rules' level value
    assert_eq!(t.category_level(2, 38), Some(EeTechCivLevel { mValue: 0.01, mBonus: 0.25 }));
    assert_eq!(t.category_level(2, 60), Some(EeTechCivLevel { mValue: 0.01, mBonus: 22.25 }));
    assert_eq!(t.category_level(2, 1), Some(EeTechCivLevel::default()));
    assert_eq!(t.level_count(0), 60);
    assert_eq!(t.lookup_level(make_upgrade(0, 2)), Some(EeTechCivLevel { mValue: 0.2, mBonus: 3.0 }));
    assert_eq!(t.lookup_level(make_upgrade(0, 61)), Some(EeTechCivLevel::default()));
    assert_eq!(t.lookup_level(make_upgrade(3, 1)), None);
    assert_eq!(t.extra_cost(make_upgrade(1, 1)), 5);
    assert_eq!(t.extra_cost(make_upgrade(9, 1)), 0);
    assert_eq!(t.upgrades().len(), 180);
    assert_eq!(t.upgrades()[61], (1 << 16) | 2);
    assert_eq!(t.power_upgrade(0x26), (2 << 16) | 0x26);
    // the default rules have no category texts: no ids, no keys
    assert!(t.ids.iter().all(|id| id.is_empty()));
    assert_eq!(t.upgrade_key(rules(), make_upgrade(0, 1)), None);
}

#[test]
fn load_of_a_short_count_is_its_error() {
    let _g = locked();
    assert_eq!(tables_mut().load(&[1, 0], rules(), 0), CIV_END_OF_FILE);
    assert_eq!(tables().category_count(), 0);
    assert_eq!(tables_mut().load_powers(&[]), CIV_END_OF_FILE);
}

#[test]
fn keys_use_the_category_ids() {
    let _g = locked();
    load_standard();
    let mut t = tables_mut();
    t.ids[0] = b"vanilla".to_vec();
    t.ids[2] = b"special_powers".to_vec();
    let r = rules();
    assert_eq!(t.upgrade_key(r, make_upgrade(0, 5)), Some(b"vanilla.range".to_vec()));
    assert_eq!(t.upgrade_key(r, make_upgrade(2, 0x31)), Some(b"special_powers.camouflage".to_vec()));
    assert_eq!(t.upgrade_key(r, make_upgrade(1, 1)), None);
    assert_eq!(t.upgrade_key(r, make_upgrade(0, 22)), None); // no attribute at level 22
    assert_eq!(t.find_upgrade_key(r, b"vanilla.range"), Some(make_upgrade(0, 5)));
    assert_eq!(t.find_upgrade_key(r, b"special_powers.sas"), Some(make_upgrade(2, 51)));
    assert_eq!(t.find_upgrade_key(r, b"vanilla"), None);
    assert_eq!(t.find_upgrade_key(r, b"vanilla.nothing"), None);
    assert_eq!(t.find_upgrade_key(r, b"nothing.range"), None);
}

#[test]
fn premade_civilizations_sort_by_index_and_match_their_checksums() {
    let _g = locked();
    let civs = [
        EeTechCivEntry {
            mIndex: 3,
            mFileName: name("Rome"),
            mChecksum: 0,
            mNameID: 11,
            mEraNameID: 501,
            ..Default::default()
        },
        EeTechCivEntry {
            mIndex: 1,
            mFileName: name("Greece"),
            mChecksum: 0,
            mNameID: 12,
            mEraNameID: 500,
            ..Default::default()
        },
        EeTechCivEntry {
            mIndex: 2,
            mFileName: name("Egypt"),
            mChecksum: 0,
            mNameID: 13,
            mEraNameID: 501,
            ..Default::default()
        },
    ];
    let mut t = tables_mut();
    assert_eq!(t.load_premade(&bytes_of(&civs)), CIV_OK);
    assert_eq!(t.civilizations.iter().map(|c| c.mIndex).collect::<Vec<_>>(), vec![1, 2, 3]);
    assert_eq!(t.eras, vec![500, 501]);
    assert_eq!(t.civilizations_of_type(501).iter().map(|c| c.mNameID).collect::<Vec<_>>(), vec![13, 11]);
    // checksum: the count, the characters, the legacy ids
    let upgrades = [make_upgrade(1, 3), make_upgrade(0, 38)];
    let characters: Vec<c_char> = "Ab".bytes().map(|b| b as c_char).collect();
    let sum = 2 + 65 + 98 + ((1 << 6) | 3) + 38;
    t.civilizations[2].mChecksum = sum;
    assert_eq!(t.matches_premade(b"Rome", &characters, &upgrades), (true, 11));
    assert_eq!(t.matches_premade(b"Rome", &characters, &upgrades[..1]), (false, 11));
    assert_eq!(t.matches_premade(b"Nowhere", &characters, &upgrades), (false, 0));
    // a load clears the old ones
    assert_eq!(t.load_premade(&bytes_of::<EeTechCivEntry>(&[])), CIV_OK);
    assert!(t.civilizations.is_empty() && t.eras.is_empty());
}

#[test]
fn character_bytes_read_each_characters_first_byte() {
    let text = "a\u{e9}b\0";
    // SAFETY: a NUL-terminated string
    let got = unsafe { character_bytes(text.as_ptr() as *const c_char, 3) };
    assert_eq!(got, vec![b'a' as c_char, 0xc3u8 as c_char, b'b' as c_char]);
    // past the end: the terminator
    // SAFETY: as above
    assert_eq!(unsafe { character_bytes(text.as_ptr() as *const c_char, 4) }[3], 0);
}

#[test]
fn legacy_ids_and_levels() {
    assert_eq!(from_legacy((3 << 6) | 7), (3 << 16) | 7);
    assert_eq!(to_legacy((3 << 16) | 7), (true, (3 << 6) | 7));
    assert_eq!(to_legacy(5 << 16 | 0x40), (false, (5 << 6)));
    assert_eq!(to_legacy(0x400 << 16), (false, 0x400 << 6));
    let r = rules();
    assert_eq!(level_name_id(r, 1), 29717);
    assert_eq!(level_name_id(r, 22), 22 + IDS_CIV_BONUS_BASE);
    assert_eq!(level_name_id(r, 0x50), 0x50 + IDS_CIV_POWER_BASE);
    assert!(!is_power_level(r, 21) && !is_power_level(r, 22) && is_power_level(r, 0x26) && is_power_level(r, 0x3d));
    assert_eq!(power_level(r, 0x31), 49);
    assert_eq!(power_level(r, 0x70), 0x70);
    assert!(has_power(r, &[make_upgrade(4, 1), make_upgrade(2, 49)], 0x31));
    assert!(!has_power(r, &[make_upgrade(2, 49)], 0x30));
    assert!(!has_power(r, &[], 0x31));
}

#[test]
fn powers_by_id() {
    let _g = locked();
    load_standard();
    let t = tables();
    assert_eq!(t.power(0x27).map(|p| p.mTechnology), Some(7039));
    assert_eq!(t.power(0x25), None);
    assert_eq!(t.power(0x3d), None);
    drop(t);
    tables_mut().powers[1].mID = 0;
    assert_eq!(tables().power(0x27), None);
}

// ------------------------------------------------------------------------------------------------ the simulation

/// A test world: one player, unit types 1..=4 with stats; the set 5 holds types 2 and 3.
struct World {
    stats: Vec<EeTechCivStats>,
    log: Vec<(u32, i64, i64, f32)>,
    conversion: f32,
    high_ground: f32,
    share_bonus: c_long,
    population: c_long,
    researched: bool,
}

impl World {
    fn new() -> World {
        let s = EeTechCivStats {
            up_attack: 10.0,
            up_reload_time: 1000,
            up_armor: [1.0, 2.0, 3.0, 4.0, 5.0, 0.0],
            up_range: 0.5,
            up_line_of_sight: 8.0,
            up_max_velocity: 2.0,
            up_flight_time: 3,
            up_area_effect: 2.0,
            up_gather_times: [100, 200, 300, 400, 500, 600, 700],
            up_build_time_scale: 1.0,
            up_repair_rate: 7,
            up_area_effect_scale: 4.0,
            up_cost_scale: 1.0,
            class_attack: 10,
            class_reload_time: 1000,
            class_armor: [1, 2, 3, 4, 5, 0],
            class_range: 0.5,
            class_line_of_sight: 8.0,
            class_max_velocity: 2.0,
            class_flight_time: 3,
            class_area_effect: 2.0,
            class_gather_times: [100, 200, 300, 400, 500, 600, 700],
            class_repair_rate: 7,
            class_area_effect_scale: 4.0,
            class_category: 5,
        };
        World {
            stats: vec![s; 5],
            log: Vec::new(),
            conversion: 1.0,
            high_ground: 0.25,
            share_bonus: 10,
            population: 75,
            researched: true,
        }
    }
}

impl CivHost for World {
    fn call(&mut self, c: &EeTechCivCall) -> EeTechCivReply {
        assert_eq!(c.player, 1);
        let mut r = EeTechCivReply::default();
        match c.op {
            EE_TECH_CIV_OP_CLASS_COUNT => r.value = 5,
            EE_TECH_CIV_OP_SET_EXISTS => r.value = (c.a == 5) as i64,
            EE_TECH_CIV_OP_SET_CONTAINS => r.value = (c.a == 5 && (c.b == 2 || c.b == 3)) as i64,
            EE_TECH_CIV_OP_CLASS => {
                if (1..5).contains(&c.a) {
                    r.value = 1;
                    r.stats = self.stats[c.a as usize];
                }
                return r;
            }
            EE_TECH_CIV_OP_PUT => {
                // the class's values never change; the whole view stands for the stat written
                self.stats[c.a as usize] = c.stats;
            }
            EE_TECH_CIV_OP_VARIANT_FACTOR => r.real = 0.5,
            EE_TECH_CIV_OP_GET_FIELD => match c.a as i32 {
                EE_TECH_CIV_FIELD_CONVERSION => r.real = self.conversion,
                EE_TECH_CIV_FIELD_HIGH_GROUND => r.real = self.high_ground,
                _ => r.value = self.share_bonus as i64,
            },
            EE_TECH_CIV_OP_SET_FIELD => match c.a as i32 {
                EE_TECH_CIV_FIELD_CONVERSION => self.conversion = c.real,
                EE_TECH_CIV_FIELD_HIGH_GROUND => self.high_ground = c.real,
                _ => self.share_bonus = c.b as c_long,
            },
            EE_TECH_CIV_OP_POPULATION_LIMIT => r.value = self.population as i64,
            EE_TECH_CIV_OP_NODE_RESEARCHED => r.value = self.researched as i64,
            _ => {}
        }
        if c.op != EE_TECH_CIV_OP_PUT {
            self.log.push((c.op, c.a, c.b, c.real));
        }
        r
    }
}

fn ops(w: &World) -> Vec<u32> {
    w.log.iter().map(|l| l.0).collect()
}

#[test]
fn an_upgrade_walks_its_category_set() {
    let _g = locked();
    load_standard();
    let mut w = World::new();
    // category 0 (set 5), level 1 (attack, 0.1): types 2 and 3, a share of the base (at least 1)
    assert!(apply_upgrade(&mut w, make_upgrade(0, 1), 1));
    assert_eq!(w.stats[2].up_attack, 11.0);
    assert_eq!(w.stats[3].up_attack, 11.0);
    assert_eq!(w.stats[1].up_attack, 10.0);
    // a category whose set is missing still answers true; an unknown category false
    assert!(apply_upgrade(&mut w, make_upgrade(1, 1), 1));
    assert!(!apply_upgrade(&mut w, make_upgrade(7, 1), 1));
    assert_eq!(w.stats[2].up_attack, 11.0);
}

#[test]
fn hit_points_scale_by_the_game_variant() {
    let _g = locked();
    load_standard();
    let mut w = World::new();
    assert!(apply_upgrade(&mut w, make_upgrade(0, 2), 1));
    let hits: Vec<_> = w.log.iter().filter(|l| l.0 == EE_TECH_CIV_OP_HIT_POINTS).map(|l| (l.1, l.3)).collect();
    // (0.5 + 1) * 0.2
    assert_eq!(hits, vec![(2, 1.5f32 * 0.2f32), (3, 1.5f32 * 0.2f32)]);
}

#[test]
fn range_takes_the_sight_off_around_it() {
    let _g = locked();
    let mut w = World::new();
    apply_upgrade_level(&mut w, 5, 2, 0.1, 1);
    assert_eq!(ops(&w), vec![EE_TECH_CIV_OP_SIGHT, EE_TECH_CIV_OP_SIGHT]);
    assert_eq!((w.log[0].2, w.log[1].2), (0, 1));
    // range: 0.5 + max(0.05, 1) (through SetRange); line of sight 8 + max(0.8, 1)
    assert_eq!(w.stats[2].up_range, 1.5);
    assert_eq!(w.stats[2].up_line_of_sight, 9.0);
}

#[test]
fn share_rules_per_stat() {
    let _g = locked();
    let mut w = World::new();
    apply_upgrade_level(&mut w, 3, 1, 0.25, 1); // reload: 1000 - (long)(250.5)
    assert_eq!(w.stats[1].up_reload_time, 750);
    apply_upgrade_level(&mut w, 4, 1, 0.25, 1); // armor: + max((long)(base * 0.25 + 0.5), 1)
    assert_eq!(w.stats[1].up_armor, [2.0, 3.0, 4.0, 5.0, 6.0, 1.0]);
    apply_upgrade_level(&mut w, 6, 1, 0.5, 1); // speed: 2 * 0.5 + 2
    assert_eq!(w.stats[1].up_max_velocity, 3.0);
    apply_upgrade_level(&mut w, 7, 1, 0.1, 1); // flight time: + max((long)0.8, 1)
    assert_eq!(w.stats[1].up_flight_time, 4);
    apply_upgrade_level(&mut w, 8, 1, 0.5, 1); // area effect: + 2 * 0.5
    assert_eq!(w.stats[1].up_area_effect, 3.0);
    apply_upgrade_level(&mut w, 9, 1, 0.1, 1); // gather slots 5 and 4, not the naval citizens
    assert_eq!(w.stats[1].up_gather_times, [100, 200, 300, 400, 450, 540, 700]);
    w.stats[1].class_category = 22;
    apply_upgrade_level(&mut w, 9, 1, 0.1, 1);
    assert_eq!(w.stats[1].up_gather_times, [100, 200, 300, 400, 450, 540, 700]);
    apply_upgrade_level(&mut w, 17, 1, 0.1, 1); // fishing: only the naval citizens
    assert_eq!(w.stats[1].up_gather_times[4], 400);
    apply_upgrade_level(&mut w, 15, 1, 0.2, 1); // build time: x (1 - 0.2)
    assert_eq!(w.stats[1].up_build_time_scale, 0.8);
    apply_upgrade_level(&mut w, 16, 1, 0.5, 1); // conversion: x 0.5
    assert_eq!(w.conversion, 0.5);
    apply_upgrade_level(&mut w, 18, 1, 0.1, 1); // repair: + max((long)1.2, 1)
    assert_eq!(w.stats[1].up_repair_rate, 8);
    w.log.clear();
    apply_upgrade_level(&mut w, 19, 1, 0.1, 1); // area effect scale: + max(0.4, 1), then the area effects
    assert_eq!(w.stats[1].up_area_effect_scale, 5.0);
    assert_eq!(ops(&w), vec![EE_TECH_CIV_OP_AREA_EFFECTS]);
    apply_upgrade_level(&mut w, 20, 1, 0.5, 1); // high ground: + 0.5
    assert_eq!(w.high_ground, 0.75);
    w.log.clear();
    apply_upgrade_level(&mut w, 21, 1, 0.1, 1); // population: + (long)(75 * 0.1 + 0.5)
    assert_eq!(w.share_bonus, 18);
    assert_eq!(
        ops(&w),
        vec![
            EE_TECH_CIV_OP_POPULATION_LIMIT,
            EE_TECH_CIV_OP_POPULATION_LIMIT,
            EE_TECH_CIV_OP_GET_FIELD,
            EE_TECH_CIV_OP_SET_FIELD,
            EE_TECH_CIV_OP_POPULATION_LIMIT
        ]
    );
    apply_upgrade_level(&mut w, 23, 1, 0.25, 1); // cost: x (1 - 0.25)
    assert_eq!(w.stats[1].up_cost_scale, 0.75);
    apply_upgrade_level(&mut w, 1, 1, 0.01, 1); // attack: a share below 1 adds 1
    assert_eq!(w.stats[1].up_attack, 11.0);
    w.stats[1].class_attack = 0;
    apply_upgrade_level(&mut w, 1, 1, 0.5, 1); // no base attack: nothing
    assert_eq!(w.stats[1].up_attack, 11.0);
    w.log.clear();
    apply_upgrade_level(&mut w, 22, 1, 0.5, 1); // no attribute at level 22
    assert!(w.log.is_empty());
}

#[test]
fn modes_add_and_multiply() {
    assert_eq!(mode_f32(2.0, K_MODE_ADD, 0.5), 2.5);
    assert_eq!(mode_f32(2.0, K_MODE_MULTIPLY, 0.5), 3.0);
    assert_eq!(mode_long(10, K_MODE_ADD, 2.4), 12);
    assert_eq!(mode_long(10, K_MODE_ADD, -2.6), 7);
    assert_eq!(mode_long(10, K_MODE_MULTIPLY, 0.25), 13);
    assert_eq!(to_long(f32::NAN), c_long::MIN);
    assert_eq!(to_long(-1.9), -1);
}

const K_MODE_ADD: i32 = ee_data::generated::rules_gen::K_CIV_MODE_ADD;
const K_MODE_MULTIPLY: i32 = ee_data::generated::rules_gen::K_CIV_MODE_MULTIPLY;

#[test]
fn a_power_level_applies_only_its_power() {
    let _g = locked();
    load_standard();
    let mut w = World::new();
    // flaming arrows (0x26): its technology, then each upgrade whose tech is researched
    apply_upgrade_level(&mut w, 38, 2, 1.0, 1);
    let expected: Vec<(u32, i64)> = vec![
        (EE_TECH_CIV_OP_RESEARCH, 7038),
        (EE_TECH_CIV_OP_NODE_RESEARCHED, 3060),
        (EE_TECH_CIV_OP_RESEARCH_NODE, 3059),
        (EE_TECH_CIV_OP_NODE_RESEARCHED, 5080),
        (EE_TECH_CIV_OP_RESEARCH_NODE, 5081),
        (EE_TECH_CIV_OP_NODE_RESEARCHED, 6080),
        (EE_TECH_CIV_OP_RESEARCH_NODE, 6078),
    ];
    assert_eq!(w.log.iter().map(|l| (l.0, l.1)).collect::<Vec<_>>(), expected);
    w.log.clear();
    w.researched = false;
    apply_power(&mut w, 1, 0x26);
    assert_eq!(w.log.len(), 4);
    // exploration: a pulse of the power's amount per unit type; advanced mining: its limit; camouflage
    w.log.clear();
    apply_power(&mut w, 1, 0x2c);
    assert_eq!(
        w.log[1..].to_vec(),
        vec![(EE_TECH_CIV_OP_PULSE_SIGHT, 35, 0, 12.5), (EE_TECH_CIV_OP_PULSE_SIGHT, 41, 0, 12.5)]
    );
    w.log.clear();
    apply_power(&mut w, 1, 0x2f);
    assert_eq!(w.log[1], (EE_TECH_CIV_OP_MINING_LIMIT, 4, 0, 0.0));
    w.log.clear();
    apply_power(&mut w, 1, 0x31);
    assert_eq!(w.log[1].0, EE_TECH_CIV_OP_CAMOUFLAGE);
    // expansionism: disallow, then allow
    w.log.clear();
    apply_power(&mut w, 1, 0x3a);
    assert_eq!(
        w.log[1..].to_vec(),
        vec![(EE_TECH_CIV_OP_ALLOW_TECH, 1028, 0, 0.0), (EE_TECH_CIV_OP_ALLOW_TECH, 1025, 1, 0.0)]
    );
    // a power without a rule: only its technology; an unknown power: nothing
    w.log.clear();
    apply_power(&mut w, 1, 0x27 + 3);
    assert_eq!(ops(&w)[0], EE_TECH_CIV_OP_RESEARCH);
    w.log.clear();
    apply_power(&mut w, 1, 0x70);
    assert!(w.log.is_empty());
}

// ------------------------------------------------------------------------------------------------ the files

fn legacy_file(name: &[u8], ids: &[u32]) -> Vec<u8> {
    let mut out = (name.len() as u32).to_le_bytes().to_vec();
    out.extend_from_slice(name);
    out.extend_from_slice(&(ids.len() as u32).to_le_bytes());
    for id in ids {
        out.extend_from_slice(&id.to_le_bytes());
    }
    out
}

#[test]
fn a_legacy_file_reads_its_name_and_ids() {
    let _g = locked();
    let t = tables();
    let f = read_file(&t, &legacy_file(b"Rome\0junk", &[(1 << 6) | 2, 38]), b"Rome.civ");
    assert_eq!(f, CivFile { result: 0, name: Some(b"Rome".to_vec()), upgrades: vec![(1 << 16) | 2, 38], error: None });
    let f = read_file(&t, &legacy_file(b"", &[]), b"x.civ");
    assert_eq!(f.name, Some(Vec::new()));
    // short: the ids read so far, the read's error
    let mut b = legacy_file(b"Ab", &[1, 2]);
    b.truncate(b.len() - 2);
    let f = read_file(&t, &b, b"x.civ");
    assert_eq!((f.result, f.upgrades.clone()), (CIV_END_OF_FILE, vec![1]));
    // a name longer than the file: not set, and the count is read where the name was
    let f = read_file(&t, &[9, 0, 0, 0, 1, 0, 0, 0], b"x.civ");
    assert_eq!((f.result, f.name, f.upgrades), (CIV_END_OF_FILE, None, vec![]));
    let f = read_file(&t, &[20, 0, 0, 0, 1, 0, 0, 0, 0x45, 0, 0, 0, 0, 0, 0, 0], b"x.civ");
    assert_eq!((f.result, f.name, f.upgrades), (0, None, vec![(1 << 16) | 5]));
    assert_eq!(read_file(&t, &[], b"x.civ").result, CIV_END_OF_FILE);
}

#[test]
fn a_keyed_file_reads_its_keys_until_one_is_not_an_upgrade() {
    let _g = locked();
    load_standard();
    tables_mut().ids[0] = b"vanilla".to_vec();
    let t = tables();
    let text = b"eedata: civilization\nname: Sparta\nupgrades:\n  - vanilla.attack\n  - vanilla.armor\n";
    let f = read_file(&t, text, b"Sparta.civ");
    assert_eq!(f, CivFile { result: 0, name: Some(b"Sparta".to_vec()), upgrades: vec![1, 4], error: None });
    let text = b"# mine\nname: Sparta\nupgrades:\n  - vanilla.attack\n  - vanilla.nothing\n  - vanilla.armor\n";
    let f = read_file(&t, text, b"Sparta.civ");
    assert_eq!(f.result, CIV_FAILED);
    assert_eq!(f.upgrades, vec![1]);
    assert_eq!(f.name, Some(b"Sparta".to_vec()));
    assert_eq!(
        f.error.unwrap(),
        b"Sparta.civ line 5: 'vanilla.nothing' is not an upgrade (<dbcivilization id>.<civbuilder attribute id>)"
            .to_vec()
    );
    let f = read_file(&t, b"# just a list\n- a\n- b\n", b"L.civ");
    assert_eq!((f.result, f.name, f.error), (CIV_FAILED, None, Some(b"L.civ: not a civilization".to_vec())));
    assert!(!is_keyed(b"#abc") && is_keyed(b"#abcde") && !is_keyed(b"eedat\0"));
}

/// A test file host: its name and upgrades, its loads' results, the steps logged.
struct Files {
    name: Vec<u8>,
    upgrades: Vec<u32>,
    loads: Vec<(i64, Vec<u32>)>,
    log: Vec<(u32, i64)>,
    flags: [bool; 4],
}

impl CivFileHost for Files {
    fn call(&mut self, op: u32, a: i64) -> EeTechCivFileReply {
        self.log.push((op, a));
        let mut r = EeTechCivFileReply::default();
        match op {
            EE_TECH_CIVF_OP_LOAD => {
                let (result, ids) = self.loads.remove(0);
                self.upgrades.extend(ids);
                r.value = result;
            }
            EE_TECH_CIVF_OP_STATE => {
                r.text = self.name.as_ptr() as *const c_char;
                r.characters = self.name.len() as u32 - 1;
                r.upgrades = self.upgrades.as_ptr();
                r.count = self.upgrades.len();
            }
            EE_TECH_CIVF_OP_CAN_BUY => r.value = self.flags[0] as i64,
            EE_TECH_CIVF_OP_PURCHASED => r.value = self.flags[1] as i64,
            EE_TECH_CIVF_OP_ANY_CIV => r.value = self.flags[2] as i64,
            EE_TECH_CIVF_OP_CAN_AFFORD_ALL => r.value = self.flags[3] as i64,
            _ => {}
        }
        r
    }
}

fn files(loads: Vec<(i64, Vec<u32>)>) -> Files {
    Files { name: b"Ab\0".to_vec(), upgrades: Vec::new(), loads, log: Vec::new(), flags: [false, false, false, true] }
}

#[test]
fn a_file_applied_buys_its_upgrades_at_no_cost() {
    let _g = locked();
    let civs = [EeTechCivEntry {
        mIndex: 1,
        mFileName: name("Rome"),
        mChecksum: 2 + 65 + 98 + 3 + 4,
        mNameID: 44,
        ..Default::default()
    }];
    tables_mut().load_premade(&bytes_of(&civs));
    // the game's file: the premade name's string, then the reset and the buys
    let mut h = files(vec![(0, vec![3, 4])]);
    assert_eq!(apply_file(&mut h, b"Rome"), 0);
    assert_eq!(
        h.log,
        vec![
            (EE_TECH_CIVF_OP_LOAD, 0),
            (EE_TECH_CIVF_OP_STATE, 0),
            (EE_TECH_CIVF_OP_NAME_STRING, 44),
            (EE_TECH_CIVF_OP_CIV_RESET, 0),
            (EE_TECH_CIVF_OP_STATE, 0),
            (EE_TECH_CIVF_OP_CIV_BUY_FREE, 3),
            (EE_TECH_CIVF_OP_CIV_BUY_FREE, 4),
            (EE_TECH_CIVF_OP_CIV_DONE, 0)
        ]
    );
    // the user's file after the game's failed (what the first load read is kept)
    let mut h = files(vec![(5, vec![9]), (0, vec![3])]);
    assert_eq!(apply_file(&mut h, b"Mine"), 0);
    assert_eq!(h.log[1], (EE_TECH_CIVF_OP_LOAD, 1));
    assert_eq!(h.log.iter().filter(|l| l.0 == EE_TECH_CIVF_OP_CIV_BUY_FREE).count(), 2);
    // neither: nothing changes
    let mut h = files(vec![(5, vec![]), (6, vec![])]);
    assert_eq!(apply_file(&mut h, b"None"), 6);
    assert_eq!(h.log.len(), 2);
    // a game file that is not premade: no string
    let mut h = files(vec![(0, vec![])]);
    apply_file(&mut h, b"Other");
    assert!(!h.log.iter().any(|l| l.0 == EE_TECH_CIVF_OP_NAME_STRING));
}

#[test]
fn a_request_is_granted_only_when_allowed_valid_and_affordable() {
    let _g = locked();
    let civs = [EeTechCivEntry {
        mIndex: 1,
        mFileName: name("Ab"),
        mChecksum: 1 + 65 + 98 + 7,
        mNameID: 55,
        ..Default::default()
    }];
    tables_mut().load_premade(&bytes_of(&civs));
    let grant = |flags: [bool; 4], upgrades: Vec<u32>| {
        let mut h = files(Vec::new());
        h.flags = flags;
        h.upgrades = upgrades;
        let granted = request_upgrades(&mut h);
        (granted, h.log.last().copied().unwrap())
    };
    // premade and matching: granted with its name id
    assert_eq!(grant([false, false, false, true], vec![7]), (true, (EE_TECH_CIVF_OP_GRANT, 55)));
    // not matching
    assert_eq!(grant([false, false, false, true], vec![8]), (false, (EE_TECH_CIVF_OP_REFUSE, 0)));
    // any civilization: name id 0
    assert_eq!(grant([false, false, true, true], vec![8]), (true, (EE_TECH_CIVF_OP_GRANT, 0)));
    // one purchase and done already
    assert_eq!(grant([true, true, true, true], vec![8]), (false, (EE_TECH_CIVF_OP_REFUSE, 0)));
    assert_eq!(grant([true, false, true, true], vec![8]), (true, (EE_TECH_CIVF_OP_GRANT, 0)));
    // not affordable
    assert_eq!(grant([false, false, true, false], vec![8]), (false, (EE_TECH_CIVF_OP_REFUSE, 0)));
}
