//! `EECivilizationManager.cpp`'s C++ logic in Rust over one-statement shims (UI lane round 11): the simulation's
//! host whole (the old `EECivHostCall`: the unit sets, the player's class upgrades read and written, the units'
//! sight, the player's fields, the powers' research, media, sight pulse, camouflage and tech nodes, for
//! `ee_sim::tech::civ_apply`), and `FillCivilizationList` (the premade civilizations' list, one header per era).

use core::ffi::{c_long, c_ulong};

use ee_sim::tech::civ;
use ee_sim::tech::civ_apply::*;
use ee_sim::world::player_fields::{EeUnitsPlayerFields, EeUnitsPlayerSaved};
use ee_sim::world::stats_fields::EeUnitsUnitClassUpgradeFields;
use ee_sim::world::unit_class_fields::EeUnitsUnitClassFields;

use crate::screens::civ_whole::CbHost;
use crate::screens::game_cpp::{Cpp, EeScreensShimOut};

/// The class upgrade's record (`EEUnitClassUpgrade`'s `EeUnitsUnitClassUpgradeFields`), read and written in place.
fn upgrade_fields<C: Cpp + ?Sized>(c: &mut C, u: usize) -> *mut EeUnitsUnitClassUpgradeFields {
    c.civh_upgrade_fields(u) as *mut EeUnitsUnitClassUpgradeFields
}

/// The player's records (`EEPlayer`'s `EeUnitsPlayerSaved` and `EeUnitsPlayerFields`), read and written in place.
fn player_records<'a, C: Cpp + ?Sized>(c: &mut C, player: i32) -> (&'a mut EeUnitsPlayerSaved, &'a mut EeUnitsPlayerFields) {
    let pl = c.civc_player(player as i64);
    // SAFETY: the live player's records (no other reference to them lives across the caller's use)
    unsafe { (&mut *(c.civc_player_saved(pl) as *mut _), &mut *(c.civc_player_fields(pl) as *mut _)) }
}

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
            // SAFETY: the live upgrade's and its class's records, read in place
            let (up, cl) =
                unsafe { (&*upgrade_fields(c, u), &*(c.civh_class_fields(k) as *const EeUnitsUnitClassFields)) };
            let s = &mut r.stats;
            s.up_attack = up.mAttack;
            s.up_reload_time = up.mReloadTime as i64;
            s.up_range = up.mRange;
            s.up_line_of_sight = up.mLineOfSight;
            s.up_max_velocity = up.mMaxVelocity;
            s.up_flight_time = up.mFlightTime as i64;
            s.up_area_effect = up.mAreaEffect;
            s.up_build_time_scale = up.mBuildTimeScale;
            s.up_repair_rate = up.mRepairRate as i64;
            s.up_area_effect_scale = up.mAreaEffectScale;
            s.up_cost_scale = up.mCostScale;
            s.class_attack = cl.mAttack as i64;
            s.class_reload_time = cl.mReloadTime as i64;
            s.class_range = cl.mRange;
            s.class_line_of_sight = cl.mLineOfSight;
            s.class_max_velocity = cl.mMaxVelocity;
            s.class_flight_time = cl.mFlightTime as i64;
            s.class_area_effect = cl.mAreaEffect;
            s.class_repair_rate = cl.mRepairRate as i64;
            s.class_area_effect_scale = cl.mAreaEffectScale;
            s.class_category = cl.mCategory as c_long as i64;
            for i in 0..6 {
                s.up_armor[i] = up.mArmor[i];
                s.class_armor[i] = cl.mArmor[i] as i64;
            }
            for i in 0..7 {
                s.up_gather_times[i] = up.mGatherTimes[i] as i64;
                s.class_gather_times[i] = cl.mGatherTimes[i] as i64;
            }
            r.value = 1;
        }
        EE_TECH_CIV_OP_PUT => {
            let u = c.civh_upgrade(player, kind);
            let s = &call.stats;
            // the range and the speed through their setters; the rest written in place
            match call.b {
                EE_TECH_CIV_STAT_RANGE => return c.civh_put_range(u, s.up_range),
                EE_TECH_CIV_STAT_SPEED => return c.civh_put_max_velocity(u, s.up_max_velocity),
                _ => {}
            }
            // SAFETY: the live upgrade's record
            let up = unsafe { &mut *upgrade_fields(c, u) };
            match call.b {
                EE_TECH_CIV_STAT_ATTACK => up.mAttack = s.up_attack,
                EE_TECH_CIV_STAT_RELOAD_TIME => up.mReloadTime = s.up_reload_time as c_long,
                EE_TECH_CIV_STAT_ARMOR => up.mArmor = s.up_armor,
                EE_TECH_CIV_STAT_LINE_OF_SIGHT => up.mLineOfSight = s.up_line_of_sight,
                EE_TECH_CIV_STAT_FLIGHT_TIME => up.mFlightTime = s.up_flight_time as c_long,
                EE_TECH_CIV_STAT_AREA_EFFECT => up.mAreaEffect = s.up_area_effect,
                EE_TECH_CIV_STAT_GATHER_TIMES => {
                    for (i, &v) in s.up_gather_times.iter().enumerate() {
                        up.mGatherTimes[i] = v as c_long;
                    }
                }
                EE_TECH_CIV_STAT_BUILD_TIME => up.mBuildTimeScale = s.up_build_time_scale,
                EE_TECH_CIV_STAT_REPAIR_RATE => up.mRepairRate = s.up_repair_rate as c_long,
                EE_TECH_CIV_STAT_AREA_EFFECT_SCALE => up.mAreaEffectScale = s.up_area_effect_scale,
                EE_TECH_CIV_STAT_COST => up.mCostScale = s.up_cost_scale,
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
            let (saved, fields) = player_records(c, player);
            if call.a == EE_TECH_CIV_FIELD_CONVERSION as i64 {
                r.real = fields.mConversionFactor;
            } else if call.a == EE_TECH_CIV_FIELD_HIGH_GROUND as i64 {
                r.real = saved.mHighGroundBonus;
            } else {
                r.value = fields.mShareBonus as i64;
            }
        }
        EE_TECH_CIV_OP_SET_FIELD => {
            let (saved, fields) = player_records(c, player);
            if call.a == EE_TECH_CIV_FIELD_CONVERSION as i64 {
                fields.mConversionFactor = call.real;
            } else if call.a == EE_TECH_CIV_FIELD_HIGH_GROUND as i64 {
                saved.mHighGroundBonus = call.real;
            } else {
                fields.mShareBonus = call.b as c_long;
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
        EE_TECH_CIV_OP_MINING_LIMIT => player_records(c, player).1.mMiningLimit = kind as c_long,
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
        // SAFETY: plain records of numbers
        let (mut up, mut cl): (EeUnitsUnitClassUpgradeFields, EeUnitsUnitClassFields) =
            unsafe { (std::mem::zeroed(), std::mem::zeroed()) };
        (up.mAttack, up.mArmor[2], cl.mCategory, cl.mGatherTimes[1]) = (3.0, 4.0, 5, 9);
        let mut fake = Fake::default();
        fake.answer("civh_class_listed", &[1]).answer("civh_upgrade", &[100]).answer("civh_upgrade_class", &[200]);
        fake.answer("civh_upgrade_fields", &[&mut up as *mut _ as i64]);
        fake.answer("civh_class_fields", &[&mut cl as *mut _ as i64]);
        let mut r = EeTechCivReply::default();
        host(&mut fake, &call(EE_TECH_CIV_OP_CLASS, 7, 0), &mut r);
        assert_eq!(r.value, 1);
        assert_eq!((r.stats.up_attack, r.stats.class_category), (3.0, 5));
        assert_eq!((r.stats.up_armor[2], r.stats.class_gather_times[1]), (4.0, 9));
        assert_eq!(fake.log.len(), 5);
    }

    #[test]
    fn a_put_writes_its_stat_only() {
        // SAFETY: a plain record of numbers
        let mut up: EeUnitsUnitClassUpgradeFields = unsafe { std::mem::zeroed() };
        let mut fake = Fake::default();
        fake.answer("civh_upgrade", &[100]).answer("civh_upgrade_fields", &[&mut up as *mut _ as i64]);
        let mut c = call(EE_TECH_CIV_OP_PUT, 7, EE_TECH_CIV_STAT_GATHER_TIMES);
        c.stats.up_gather_times[6] = 12;
        host(&mut fake, &c, &mut EeTechCivReply::default());
        assert_eq!(fake.calls(), "civh_upgrade(2, 7)\ncivh_upgrade_fields(100)");
        assert_eq!((up.mGatherTimes[6], up.mGatherTimes[5]), (12, 0));
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
        let (mut saved, mut fields) = (EeUnitsPlayerSaved::default(), EeUnitsPlayerFields::default());
        fields.mShareBonus = 3;
        let (s, f) = (&mut saved as *mut _ as i64, &mut fields as *mut _ as i64);
        let mut fake = Fake::default();
        fake.answer("civc_player", &[9, 9]).answer("civc_player_saved", &[s, s]).answer("civc_player_fields", &[f, f]);
        let mut r = EeTechCivReply::default();
        host(&mut fake, &call(EE_TECH_CIV_OP_GET_FIELD, EE_TECH_CIV_FIELD_SHARE_BONUS as i64, 0), &mut r);
        assert_eq!(r.value, 3);
        host(&mut fake, &call(EE_TECH_CIV_OP_SET_FIELD, EE_TECH_CIV_FIELD_HIGH_GROUND as i64, 0), &mut r);
        assert_eq!(saved.mHighGroundBonus, 1.5);
    }
}
