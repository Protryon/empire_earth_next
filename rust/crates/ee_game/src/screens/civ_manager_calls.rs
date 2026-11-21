//! `EECivilizationManager.cpp`'s C++ logic in Rust over one-statement shims (UI lane round 11): the simulation's
//! host whole (the old `EECivHostCall`: the unit sets, the player's class upgrades read and written, the units'
//! sight, the player's fields, the powers' research, media, sight pulse, camouflage and tech nodes, for
//! `ee_sim::tech::civ_apply`), and `FillCivilizationList` (the premade civilizations' list, one header per era).

use core::ffi::{c_long, c_ulong};

use ee_sim::tech::civ;
use ee_sim::tech::civ_apply::*;

use crate::screens::civ_whole::CbHost;
use crate::screens::game_cpp::{Cpp, EeScreensShimOut};

/// One call of the civilization simulation's host (`EeTechCivHost`'s): its reply in `r`.
pub(crate) fn host<C: Cpp + ?Sized>(c: &mut C, call: &EeTechCivCall, r: &mut EeTechCivReply) {
    let player = call.player;
    let kind = call.a;
    match call.op {
        EE_TECH_CIV_OP_SET_EXISTS => r.value = c.civh_set_exists(call.a) as i64,
        EE_TECH_CIV_OP_CLASS_COUNT => r.value = c.civh_class_count(),
        EE_TECH_CIV_OP_SET_CONTAINS => r.value = c.civh_set_contains(call.a, call.b) as i64,
        EE_TECH_CIV_OP_CLASS => {
            if !c.civh_class_listed(kind) {
                return;
            }
            let u = c.civh_upgrade(player, kind);
            if u == 0 {
                return;
            }
            // the upgrade's class: the base values and the category
            let k = c.civh_upgrade_class(u);
            let s = &mut r.stats;
            s.up_attack = c.civh_up_attack(u);
            s.up_reload_time = c.civh_up_reload_time(u);
            s.up_range = c.civh_up_range(u);
            s.up_line_of_sight = c.civh_up_line_of_sight(u);
            s.up_max_velocity = c.civh_up_max_velocity(u);
            s.up_flight_time = c.civh_up_flight_time(u);
            s.up_area_effect = c.civh_up_area_effect(u);
            s.up_build_time_scale = c.civh_up_build_time_scale(u);
            s.up_repair_rate = c.civh_up_repair_rate(u);
            s.up_area_effect_scale = c.civh_up_area_effect_scale(u);
            s.up_cost_scale = c.civh_up_cost_scale(u);
            s.class_attack = c.civh_class_attack(k);
            s.class_reload_time = c.civh_class_reload_time(k);
            s.class_range = c.civh_class_range(k);
            s.class_line_of_sight = c.civh_class_line_of_sight(k);
            s.class_max_velocity = c.civh_class_max_velocity(k);
            s.class_flight_time = c.civh_class_flight_time(k);
            s.class_area_effect = c.civh_class_area_effect(k);
            s.class_repair_rate = c.civh_class_repair_rate(k);
            s.class_area_effect_scale = c.civh_class_area_effect_scale(k);
            s.class_category = c.civh_class_category(k);
            for i in 0..6 {
                s.up_armor[i] = c.civh_up_armor(u, i as u32);
                s.class_armor[i] = c.civh_class_armor(k, i as u32);
            }
            for i in 0..7 {
                s.up_gather_times[i] = c.civh_up_gather_time(u, i as u32);
                s.class_gather_times[i] = c.civh_class_gather_time(k, i as u32);
            }
            r.value = 1;
        }
        EE_TECH_CIV_OP_PUT => {
            let u = c.civh_upgrade(player, kind);
            let s = &call.stats;
            match call.b {
                b if b == EE_TECH_CIV_STAT_ATTACK as i64 => c.civh_put_attack(u, s.up_attack),
                b if b == EE_TECH_CIV_STAT_RELOAD_TIME as i64 => c.civh_put_reload_time(u, s.up_reload_time),
                b if b == EE_TECH_CIV_STAT_ARMOR as i64 => {
                    for (i, &v) in s.up_armor.iter().enumerate() {
                        c.civh_put_armor(u, i as u32, v);
                    }
                }
                b if b == EE_TECH_CIV_STAT_RANGE as i64 => c.civh_put_range(u, s.up_range),
                b if b == EE_TECH_CIV_STAT_LINE_OF_SIGHT as i64 => c.civh_put_line_of_sight(u, s.up_line_of_sight),
                b if b == EE_TECH_CIV_STAT_SPEED as i64 => c.civh_put_max_velocity(u, s.up_max_velocity),
                b if b == EE_TECH_CIV_STAT_FLIGHT_TIME as i64 => c.civh_put_flight_time(u, s.up_flight_time),
                b if b == EE_TECH_CIV_STAT_AREA_EFFECT as i64 => c.civh_put_area_effect(u, s.up_area_effect),
                b if b == EE_TECH_CIV_STAT_GATHER_TIMES as i64 => {
                    for (i, &v) in s.up_gather_times.iter().enumerate() {
                        c.civh_put_gather_time(u, i as u32, v);
                    }
                }
                b if b == EE_TECH_CIV_STAT_BUILD_TIME as i64 => c.civh_put_build_time_scale(u, s.up_build_time_scale),
                b if b == EE_TECH_CIV_STAT_REPAIR_RATE as i64 => c.civh_put_repair_rate(u, s.up_repair_rate),
                b if b == EE_TECH_CIV_STAT_AREA_EFFECT_SCALE as i64 => {
                    c.civh_put_area_effect_scale(u, s.up_area_effect_scale)
                }
                b if b == EE_TECH_CIV_STAT_COST as i64 => c.civh_put_cost_scale(u, s.up_cost_scale),
                _ => {}
            }
        }
        EE_TECH_CIV_OP_VARIANT_FACTOR => r.real = c.civh_variant_factor(kind),
        EE_TECH_CIV_OP_HIT_POINTS => c.civh_hit_points(player, kind, call.real),
        EE_TECH_CIV_OP_SIGHT => {
            // the units of the class type (`(unsigned long)(long)a`)
            let ty = kind as c_long as c_ulong as u64;
            let mut unit = c.civh_first_unit(player);
            while unit != 0 {
                if c.civh_unit_type(unit) == ty {
                    if call.b != 0 {
                        c.civh_sight_on(unit);
                    } else {
                        c.civh_sight_off(unit);
                    }
                }
                unit = c.civh_next_unit(unit);
            }
        }
        EE_TECH_CIV_OP_GET_FIELD => {
            if call.a == EE_TECH_CIV_FIELD_CONVERSION as i64 {
                r.real = c.civh_conversion(player);
            } else if call.a == EE_TECH_CIV_FIELD_HIGH_GROUND as i64 {
                r.real = c.civh_high_ground(player);
            } else {
                r.value = c.civh_share_bonus(player);
            }
        }
        EE_TECH_CIV_OP_SET_FIELD => {
            if call.a == EE_TECH_CIV_FIELD_CONVERSION as i64 {
                c.civh_set_conversion(player, call.real);
            } else if call.a == EE_TECH_CIV_FIELD_HIGH_GROUND as i64 {
                c.civh_set_high_ground(player, call.real);
            } else {
                c.civh_set_share_bonus(player, call.b);
            }
        }
        EE_TECH_CIV_OP_POPULATION_LIMIT => r.value = c.civh_population_limit(player),
        EE_TECH_CIV_OP_AREA_EFFECTS => {
            c.civh_recompute_area_effects(player);
            c.civh_show_selection_areas(player);
        }
        EE_TECH_CIV_OP_RESEARCH => c.civh_research(player, kind),
        EE_TECH_CIV_OP_NODE_RESEARCHED => {
            let node = c.civh_node(player, kind);
            r.value = if node == 0 {
                -1
            } else if c.civh_node_researched(node) {
                1
            } else {
                0
            };
        }
        EE_TECH_CIV_OP_RESEARCH_NODE => {
            let node = c.civh_node(player, kind);
            if node != 0 {
                c.civh_node_research(node);
            }
        }
        EE_TECH_CIV_OP_LOAD_SOUND => {
            let sound = c.civh_sound(kind);
            if sound != 0 {
                c.civh_sound_load(sound);
                c.civh_add_sound(player, sound);
            }
        }
        EE_TECH_CIV_OP_LOAD_EFFECT => {
            let effect = c.civh_effect(kind);
            if effect != 0 {
                c.civh_effect_load(effect);
                c.civh_add_effect(player, effect);
            }
        }
        EE_TECH_CIV_OP_PULSE_SIGHT => c.civh_pulse_sight(player, call.real, kind),
        EE_TECH_CIV_OP_MINING_LIMIT => c.civh_mining_limit(player, kind),
        EE_TECH_CIV_OP_CAMOUFLAGE => c.civh_camouflage(player),
        EE_TECH_CIV_OP_ALLOW_TECH => {
            let node = c.civh_node(player, kind);
            if node == 0 {
                return;
            }
            c.civh_node_set_allowed(node, call.b != 0);
            if call.b == 0 {
                c.civh_node_set_obsolete(node);
            }
        }
        _ => {}
    }
}

#[cfg_attr(test, allow(dead_code))]
/// `FillCivilizationList`: the list `list` emptied, then per era with a name its header line
/// (`<era>/0/Category`) and a line per premade civilization of that era with a name (`<era>/<name>/ / / / / `),
/// then line 0 selected when the list takes tabs.
pub(crate) fn fill_civilization_list<C: Cpp + ?Sized, H: CbHost>(c: &mut C, h: &mut H, list: usize) {
    let (eras, civilizations): (Vec<i32>, Vec<(i32, i32)>) = {
        let tables = civ::tables();
        (tables.eras.clone(), tables.civilizations.iter().map(|e| (e.mEraNameID, e.mNameID)).collect())
    };
    c.cbl_delete_all(list);
    for era in eras {
        if c.cbh_string_load(era as u32) != 0 {
            continue;
        }
        let mut category = Vec::new();
        c.cbh_string_out(EeScreensShimOut::of(&mut category));
        let mut header = category.clone();
        header.extend_from_slice(b"/0/Category");
        crate::screens::civ_list::add_line(h, list, &header);
        for &(_, name_id) in civilizations.iter().filter(|&&(e, _)| e == era) {
            if c.cbh_string_load(name_id as u32) != 0 {
                continue;
            }
            let mut line = category.clone();
            line.push(b'/');
            c.cbh_string_out(EeScreensShimOut::of(&mut line));
            line.extend_from_slice(b"/ / / / / ");
            crate::screens::civ_list::add_line(h, list, &line);
        }
    }
    if c.cbl_tabable(list) {
        crate::screens::civ_list::change_selection(h, list, 0, false, false);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::screens::game_cpp::Fake;

    fn call(op: u32, a: i64, b: i64) -> EeTechCivCall {
        EeTechCivCall { op, player: 2, a, b, real: 1.5, stats: EeTechCivStats::default() }
    }

    #[test]
    fn a_class_without_an_upgrade_answers_nothing() {
        let mut fake = Fake::default();
        fake.answer("civh_class_listed", &[1]);
        let mut r = EeTechCivReply::default();
        host(&mut fake, &call(EE_TECH_CIV_OP_CLASS, 7, 0), &mut r);
        assert_eq!(r, EeTechCivReply::default());
        assert_eq!(fake.calls(), "civh_class_listed(7)\ncivh_upgrade(2, 7)");
    }

    #[test]
    fn a_class_reads_the_upgrade_and_its_class() {
        let mut fake = Fake::default();
        fake.answer("civh_class_listed", &[1]).answer("civh_upgrade", &[100]).answer("civh_upgrade_class", &[200]);
        fake.answer("civh_up_attack", &[3]).answer("civh_class_category", &[5]);
        fake.answer("civh_up_armor", &[0, 0, 4]).answer("civh_class_gather_time", &[0, 9]);
        let mut r = EeTechCivReply::default();
        host(&mut fake, &call(EE_TECH_CIV_OP_CLASS, 7, 0), &mut r);
        assert_eq!(r.value, 1);
        assert_eq!((r.stats.up_attack, r.stats.class_category), (3.0, 5));
        assert_eq!((r.stats.up_armor[2], r.stats.class_gather_times[1]), (4.0, 9));
        assert_eq!(fake.log.len(), 3 + 21 + 12 + 14);
    }

    #[test]
    fn a_put_writes_its_stat_only() {
        let mut fake = Fake::default();
        fake.answer("civh_upgrade", &[100]);
        let mut c = call(EE_TECH_CIV_OP_PUT, 7, EE_TECH_CIV_STAT_GATHER_TIMES as i64);
        c.stats.up_gather_times[6] = 12;
        host(&mut fake, &c, &mut EeTechCivReply::default());
        assert_eq!(fake.log.len(), 8);
        assert_eq!(fake.log[7], "civh_put_gather_time(100, 6, 12)");
    }

    #[test]
    fn the_sight_walks_the_units_of_the_type() {
        let mut fake = Fake::default();
        fake.answer("civh_first_unit", &[10]).answer("civh_next_unit", &[11, 0]).answer("civh_unit_type", &[7, 8]);
        host(&mut fake, &call(EE_TECH_CIV_OP_SIGHT, 7, 1), &mut EeTechCivReply::default());
        assert_eq!(
            fake.calls(),
            "civh_first_unit(2)\ncivh_unit_type(10)\ncivh_sight_on(10)\ncivh_next_unit(10)\ncivh_unit_type(11)\n\
             civh_next_unit(11)"
        );
    }

    #[test]
    fn the_nodes_answers_and_the_allowed_tech() {
        let mut fake = Fake::default();
        let mut r = EeTechCivReply::default();
        host(&mut fake, &call(EE_TECH_CIV_OP_NODE_RESEARCHED, 7, 0), &mut r);
        assert_eq!(r.value, -1);
        fake.answer("civh_node", &[40]);
        host(&mut fake, &call(EE_TECH_CIV_OP_ALLOW_TECH, 7, 0), &mut r);
        assert!(fake
            .calls()
            .ends_with("civh_node(2, 7)\ncivh_node_set_allowed(40, false)\ncivh_node_set_obsolete(40)"));
    }

    #[test]
    fn the_fields_by_their_code() {
        let mut fake = Fake::default();
        fake.answer("civh_share_bonus", &[3]);
        let mut r = EeTechCivReply::default();
        host(&mut fake, &call(EE_TECH_CIV_OP_GET_FIELD, EE_TECH_CIV_FIELD_SHARE_BONUS as i64, 0), &mut r);
        assert_eq!(r.value, 3);
        host(&mut fake, &call(EE_TECH_CIV_OP_SET_FIELD, EE_TECH_CIV_FIELD_HIGH_GROUND as i64, 0), &mut r);
        assert_eq!(fake.calls(), "civh_share_bonus(2)\ncivh_set_high_ground(2, 1.5)");
    }
}
