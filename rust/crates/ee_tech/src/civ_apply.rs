//! The civilization builder's simulation in Rust (the civilization builder lane, round 2): an upgrade applied to a
//! player (`EECivilizationManager::ApplyUpgrade`, 0x281e4d, with its `EECBProcessUpgrade` over the category's unit
//! set, 0x2823e2/0x282409), one level on a class's upgrade (`ApplyUpgradeLevel`, 0x281ed3: civbuilder.yaml's applies,
//! `EECBUpgradeArmor` 0x28239e) and a civilization power granted (`EECivPowersManager::ApplyPower`, 0x2832a3:
//! civpowers.yaml's effects).
//!
//! The stats come in as a plain view ([`EeTechCivStats`]: the class upgrade's stats a level changes and the class's
//! base values), read by the host before each stat and written back one stat at a time (the range through the
//! upgrade's `SetRange`, the speed through `SetMaxVelocity`, both ee_units'). The world (the unit set, the game
//! variant, the player's fields, the units' sight, the hit points' rescale, the power's research, media, sight pulse
//! and camouflage, the tech tree's nodes) is the host's too ([`CivHost`]; EECivilizationManager.cpp).
//!
//! Determinism (RUST_PLAN 2.1): the C++'s `f32` operations in its order (`(float)long * value`, then the add), its
//! truncating casts (`(long)` as [`to_long`]: the x86-64 conversion, the "indefinite" value out of range), wrapping
//! integer adds in the C++ `long`'s width.

use crate::civ::{self, find_attribute, find_power_rule, EeTechCivUpgrade, LEVEL_BITS, LEVEL_MASK, POWER_FIRST};
use ee_data::generated::rules_gen::*;
use std::ffi::c_void;
use std::os::raw::c_long;

/// The host's operations (`EeTechCivCall::op`): whether unit set `a` exists
pub const EE_TECH_CIV_OP_SET_EXISTS: u32 = 1;
/// `EEUnitClassCount()`
pub const EE_TECH_CIV_OP_CLASS_COUNT: u32 = 2;
/// `a` the set, `b` the unit type: whether the set holds it
pub const EE_TECH_CIV_OP_SET_CONTAINS: u32 = 3;
/// the player's upgrade of unit type `a` and its class: `value` 1 with `stats` filled, 0 without a class or upgrade
pub const EE_TECH_CIV_OP_CLASS: u32 = 4;
/// `gWorld.GetGameVariant()->GetFactor(a)` into `real`
pub const EE_TECH_CIV_OP_VARIANT_FACTOR: u32 = 5;
/// `EEUpgradeHitPoints(the player's upgrade of a, a, real, the player)`
pub const EE_TECH_CIV_OP_HIT_POINTS: u32 = 6;
/// the player's units of type `a`: `SightOffHere` (`b` 0) or `SightOnHere` (`b` 1)
pub const EE_TECH_CIV_OP_SIGHT: u32 = 7;
/// a player field (`a`: [`EE_TECH_CIV_FIELD_CONVERSION`] ..) into `real` / `value`
pub const EE_TECH_CIV_OP_GET_FIELD: u32 = 8;
/// a player field set from `real` / `b`
pub const EE_TECH_CIV_OP_SET_FIELD: u32 = 9;
/// `GetPopulationLimit(0)` into `value`
pub const EE_TECH_CIV_OP_POPULATION_LIMIT: u32 = 10;
/// `SuggestRecomputeAllAreaEffects()`, then `mSelectedGroup->ShowSelectionAreas()`
pub const EE_TECH_CIV_OP_AREA_EFFECTS: u32 = 11;
/// `GetTechTree()->Research(a, NULL)`
pub const EE_TECH_CIV_OP_RESEARCH: u32 = 12;
/// node `a`: `value` -1 without one, else whether it is researched
pub const EE_TECH_CIV_OP_NODE_RESEARCHED: u32 = 13;
/// node `a`'s `ResearchNode(NULL)` (when there is one)
pub const EE_TECH_CIV_OP_RESEARCH_NODE: u32 = 14;
/// `EESoundObject::Get(a)`: its media loaded and added to the player's
pub const EE_TECH_CIV_OP_LOAD_SOUND: u32 = 15;
/// `EEGraphicEffect::Get(a)`: likewise
pub const EE_TECH_CIV_OP_LOAD_EFFECT: u32 = 16;
/// `PulseSight(false, real, a)`
pub const EE_TECH_CIV_OP_PULSE_SIGHT: u32 = 17;
/// `mMiningLimit = a`
pub const EE_TECH_CIV_OP_MINING_LIMIT: u32 = 18;
/// `EnableCamouflagePower()`
pub const EE_TECH_CIV_OP_CAMOUFLAGE: u32 = 19;
/// node `a` allowed (`b` 1) or disallowed and obsolete (`b` 0)
pub const EE_TECH_CIV_OP_ALLOW_TECH: u32 = 20;
/// the stat `b` ([`EE_TECH_CIV_STAT_ATTACK`] ..) of the player's upgrade of unit type `a` written from `stats`
pub const EE_TECH_CIV_OP_PUT: u32 = 21;

/// The player fields of [`EE_TECH_CIV_OP_GET_FIELD`]: `mConversionFactor`, `mHighGroundBonus` (floats),
/// `mShareBonus` (a long).
pub const EE_TECH_CIV_FIELD_CONVERSION: i32 = 0;
pub const EE_TECH_CIV_FIELD_HIGH_GROUND: i32 = 1;
pub const EE_TECH_CIV_FIELD_SHARE_BONUS: i32 = 2;

/// The stats [`EE_TECH_CIV_OP_PUT`] writes: the upgrade's `mAttack`, `mReloadTime`, `mArmor` (all six), the range
/// (`SetRange`), `mLineOfSight`, the speed (`SetMaxVelocity`), `mFlightTime`, `mAreaEffect`, `mGatherTimes` (all
/// seven), `mBuildTimeScale`, `mRepairRate`, `mAreaEffectScale`, `mCostScale`.
pub const EE_TECH_CIV_STAT_ATTACK: i64 = 0;
pub const EE_TECH_CIV_STAT_RELOAD_TIME: i64 = 1;
pub const EE_TECH_CIV_STAT_ARMOR: i64 = 2;
pub const EE_TECH_CIV_STAT_RANGE: i64 = 3;
pub const EE_TECH_CIV_STAT_LINE_OF_SIGHT: i64 = 4;
pub const EE_TECH_CIV_STAT_SPEED: i64 = 5;
pub const EE_TECH_CIV_STAT_FLIGHT_TIME: i64 = 6;
pub const EE_TECH_CIV_STAT_AREA_EFFECT: i64 = 7;
pub const EE_TECH_CIV_STAT_GATHER_TIMES: i64 = 8;
pub const EE_TECH_CIV_STAT_BUILD_TIME: i64 = 9;
pub const EE_TECH_CIV_STAT_REPAIR_RATE: i64 = 10;
pub const EE_TECH_CIV_STAT_AREA_EFFECT_SCALE: i64 = 11;
pub const EE_TECH_CIV_STAT_COST: i64 = 12;

/// A class upgrade's stats a level changes (`up_*`; the C++ `long`s as i64) and its class's base values (`class_*`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeTechCivStats {
    pub up_attack: f32,
    pub up_reload_time: i64,
    pub up_armor: [f32; 6],
    pub up_range: f32,
    pub up_line_of_sight: f32,
    pub up_max_velocity: f32,
    pub up_flight_time: i64,
    pub up_area_effect: f32,
    pub up_gather_times: [i64; 7],
    pub up_build_time_scale: f32,
    pub up_repair_rate: i64,
    pub up_area_effect_scale: f32,
    pub up_cost_scale: f32,
    pub class_attack: i64,
    pub class_reload_time: i64,
    pub class_armor: [i64; 6],
    pub class_range: f32,
    pub class_line_of_sight: f32,
    pub class_max_velocity: f32,
    pub class_flight_time: i64,
    pub class_area_effect: f32,
    pub class_gather_times: [i64; 7],
    pub class_repair_rate: i64,
    pub class_area_effect_scale: f32,
    /// the class's `mCategory` as the C++ `long`
    pub class_category: i64,
}

/// One host call: `player` the player's index (`gWorld.mPlayers`); `stats` what [`EE_TECH_CIV_OP_PUT`] writes.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeTechCivCall {
    pub op: u32,
    pub player: i32,
    pub a: i64,
    pub b: i64,
    pub real: f32,
    pub stats: EeTechCivStats,
}

/// The host's answer.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeTechCivReply {
    pub value: i64,
    pub real: f32,
    pub stats: EeTechCivStats,
}

/// The world, as the C++ answers (a test host in the tests).
pub trait CivHost {
    fn call(&mut self, call: &EeTechCivCall) -> EeTechCivReply;
}

/// The C++ host: a callback.
#[repr(C)]
pub struct EeTechCivHost {
    pub ctx: *mut c_void,
    pub call: unsafe extern "C" fn(ctx: *mut c_void, call: *const EeTechCivCall, reply: *mut EeTechCivReply),
}

impl CivHost for EeTechCivHost {
    fn call(&mut self, call: &EeTechCivCall) -> EeTechCivReply {
        let mut reply = EeTechCivReply::default();
        // SAFETY: the C++ host answers every op on its own objects
        unsafe { (self.call)(self.ctx, call, &mut reply) };
        reply
    }
}

fn ask<H: CivHost>(h: &mut H, op: u32, player: i32, a: i64, b: i64, real: f32) -> EeTechCivReply {
    h.call(&EeTechCivCall { op, player, a, b, real, stats: EeTechCivStats::default() })
}

/// `(long)value` as the x86-64 build compiles it: toward zero, the conversion's "indefinite" value (the type's
/// minimum) for NaN or out of range.
pub fn to_long(value: f32) -> c_long {
    if std::mem::size_of::<c_long>() == 4 {
        crate::ftol(value) as c_long
    } else if (-9.223_372e18..9.223_372e18).contains(&value) {
        value as i64 as c_long
    } else {
        c_long::MIN
    }
}

/// A C++ `long` held as i64, as the C++ reads it.
fn long(v: i64) -> c_long {
    v as c_long
}

/// A float stat by a non-share mode: + the value, or x (1 + the value).
pub fn mode_f32(current: f32, mode: i32, value: f32) -> f32 {
    if mode == K_CIV_MODE_ADD {
        current + value
    } else {
        current * (1.0f32 + value)
    }
}

/// An integer stat by a non-share mode (rounded).
pub fn mode_long(current: c_long, mode: i32, value: f32) -> c_long {
    if mode == K_CIV_MODE_ADD {
        return current.wrapping_add(to_long(value + if value < 0.0 { -0.5f32 } else { 0.5f32 }));
    }
    to_long(current as f32 * (1.0f32 + value) + 0.5f32)
}

/// A share of a class's integer base, rounded, at least 1 (`EECBUpgradeArmor`, the flight time and repair rate).
fn share_at_least_one(base: i64, value: f32) -> c_long {
    let add = to_long(long(base) as f32 * value + 0.5f32);
    if add < 1 {
        1
    } else {
        add
    }
}

/// A share of a class's float base, at least 1.
fn share_f32_at_least_one(base: f32, value: f32) -> f32 {
    let add = base * value;
    if add < 1.0f32 {
        1.0f32
    } else {
        add
    }
}

/// The player's upgrade of a unit class and the class, as they are now; None without them.
fn class_stats<H: CivHost>(h: &mut H, player: i32, unit_type: i64) -> Option<EeTechCivStats> {
    let r = ask(h, EE_TECH_CIV_OP_CLASS, player, unit_type, 0, 0.0);
    (r.value != 0).then_some(r.stats)
}

/// One stat written back.
fn put<H: CivHost>(h: &mut H, player: i32, unit_type: i64, stat: i64, stats: &EeTechCivStats) {
    h.call(&EeTechCivCall { op: EE_TECH_CIV_OP_PUT, player, a: unit_type, b: stat, real: 0.0, stats: *stats });
}

fn get_f32<H: CivHost>(h: &mut H, player: i32, field: i32) -> f32 {
    ask(h, EE_TECH_CIV_OP_GET_FIELD, player, field as i64, 0, 0.0).real
}

fn set_f32<H: CivHost>(h: &mut H, player: i32, field: i32, value: f32) {
    ask(h, EE_TECH_CIV_OP_SET_FIELD, player, field as i64, 0, value);
}

fn get_long<H: CivHost>(h: &mut H, player: i32, field: i32) -> c_long {
    long(ask(h, EE_TECH_CIV_OP_GET_FIELD, player, field as i64, 0, 0.0).value)
}

fn set_long<H: CivHost>(h: &mut H, player: i32, field: i32, value: c_long) {
    ask(h, EE_TECH_CIV_OP_SET_FIELD, player, field as i64, value as i64, 0.0);
}

fn population_limit<H: CivHost>(h: &mut H, player: i32) -> c_long {
    long(ask(h, EE_TECH_CIV_OP_POPULATION_LIMIT, player, 0, 0, 0.0).value)
}

/// `EECivilizationManager::ApplyUpgrade`: the upgrade's level for the player on every unit class of its category's
/// unit set; false without a level value.
pub fn apply_upgrade<H: CivHost>(h: &mut H, upgrade: EeTechCivUpgrade, player: i32) -> bool {
    let (set, value) = {
        let t = civ::tables();
        // an unknown category: the original read past its table and found no level
        let Some(category) = t.category(upgrade >> LEVEL_BITS) else {
            return false;
        };
        let Some(level) = t.lookup_level(upgrade) else {
            return false;
        };
        (category.mUnitSet, level.mValue)
    };
    let index = upgrade & LEVEL_MASK;
    // EEUnitSetManager::ForEach: the set's EEUnitSet::ForEach when there is one (every unit type 1.. it holds)
    if ask(h, EE_TECH_CIV_OP_SET_EXISTS, player, set as i64, 0, 0.0).value != 0 {
        let count = ask(h, EE_TECH_CIV_OP_CLASS_COUNT, player, 0, 0, 0.0).value;
        for unit_type in 1..count {
            if ask(h, EE_TECH_CIV_OP_SET_CONTAINS, player, set as i64, unit_type, 0.0).value != 0 {
                process_upgrade(h, player, index, value, unit_type);
            }
        }
    }
    true
}

/// `EECBProcessUpgrade::Execute`: the level on the player's upgrade of one unit class (hit points scaled by the game
/// variant).
fn process_upgrade<H: CivHost>(h: &mut H, player: i32, index: u32, value: f32, unit_type: i64) {
    if class_stats(h, player, unit_type).is_none() {
        return;
    }
    let value = if index == 2 {
        (ask(h, EE_TECH_CIV_OP_VARIANT_FACTOR, player, unit_type, 0, 0.0).real + 1.0f32) * value
    } else {
        value
    };
    apply_upgrade_level(h, index, unit_type, value, player);
}

/// `ApplyUpgradeLevel`: what the level's attribute raises (civbuilder.yaml's applies) on the player's upgrade of a
/// unit class. An attribute that applies a power applies only its power; one that raises the line of sight takes
/// the class's units' sight off around it.
pub fn apply_upgrade_level<H: CivHost>(h: &mut H, index: u32, unit_type: i64, value: f32, player: i32) {
    let Some(attribute) = find_attribute(civ::rules(), index as i32) else {
        return;
    };
    let mut sight = false;
    for apply in &attribute.applies {
        if apply.stat == K_CIV_STAT_POWER {
            apply_power(h, player, apply.power.wrapping_add(POWER_FIRST));
            return;
        }
        sight = sight || apply.stat == K_CIV_STAT_LINE_OF_SIGHT;
    }
    if sight {
        ask(h, EE_TECH_CIV_OP_SIGHT, player, unit_type, 0, 0.0);
    }
    for apply in &attribute.applies {
        let Some(stats) = class_stats(h, player, unit_type) else {
            break;
        };
        let category = stats.class_category;
        if (apply.only_category >= 0 && category != apply.only_category as i64)
            || (apply.except_category >= 0 && category == apply.except_category as i64)
        {
            continue;
        }
        apply_stat(h, apply, stats, unit_type, value, player);
    }
    if sight {
        ask(h, EE_TECH_CIV_OP_SIGHT, player, unit_type, 1, 0.0);
    }
}

/// One stat of an attribute: the share mode is the original's rule for that stat (the base value is the class's).
fn apply_stat<H: CivHost>(h: &mut H, apply: &CivApply, mut s: EeTechCivStats, unit_type: i64, value: f32, player: i32) {
    let mode = apply.mode;
    let share = mode == K_CIV_MODE_SHARE;
    let stat = match apply.stat {
        K_CIV_STAT_ATTACK => {
            if !share {
                s.up_attack = mode_f32(s.up_attack, mode, value);
            } else if long(s.class_attack) != 0 {
                s.up_attack += share_f32_at_least_one(long(s.class_attack) as f32, value);
            }
            EE_TECH_CIV_STAT_ATTACK
        }
        K_CIV_STAT_HIT_POINTS => {
            ask(h, EE_TECH_CIV_OP_HIT_POINTS, player, unit_type, 0, value);
            return;
        }
        K_CIV_STAT_RELOAD_TIME => {
            let reload = long(s.up_reload_time);
            let reload = if !share {
                mode_long(reload, mode, value)
            } else {
                reload.wrapping_sub(to_long(long(s.class_reload_time) as f32 * value + 0.5f32))
            };
            s.up_reload_time = reload as i64;
            EE_TECH_CIV_STAT_RELOAD_TIME
        }
        K_CIV_STAT_ARMOR => {
            // EECBUpgradeArmor for the six kinds (whatever the mode)
            for k in 0..6 {
                s.up_armor[k] += share_at_least_one(s.class_armor[k], value) as f32;
            }
            EE_TECH_CIV_STAT_ARMOR
        }
        K_CIV_STAT_RANGE => {
            s.up_range = if !share {
                mode_f32(s.up_range, mode, value)
            } else {
                s.up_range + share_f32_at_least_one(s.class_range, value)
            };
            EE_TECH_CIV_STAT_RANGE
        }
        K_CIV_STAT_LINE_OF_SIGHT => {
            if !share {
                s.up_line_of_sight = mode_f32(s.up_line_of_sight, mode, value);
            } else {
                s.up_line_of_sight += share_f32_at_least_one(s.class_line_of_sight, value);
            }
            EE_TECH_CIV_STAT_LINE_OF_SIGHT
        }
        K_CIV_STAT_SPEED => {
            s.up_max_velocity = if !share {
                mode_f32(s.up_max_velocity, mode, value)
            } else {
                s.class_max_velocity * value + s.up_max_velocity
            };
            EE_TECH_CIV_STAT_SPEED
        }
        K_CIV_STAT_FLIGHT_TIME => {
            let flight = long(s.up_flight_time);
            let flight = if !share {
                mode_long(flight, mode, value)
            } else {
                flight.wrapping_add(share_at_least_one(s.class_flight_time, value))
            };
            s.up_flight_time = flight as i64;
            EE_TECH_CIV_STAT_FLIGHT_TIME
        }
        K_CIV_STAT_AREA_EFFECT => {
            if !share {
                s.up_area_effect = mode_f32(s.up_area_effect, mode, value);
            } else {
                s.up_area_effect += s.class_area_effect * value;
            }
            EE_TECH_CIV_STAT_AREA_EFFECT
        }
        K_CIV_STAT_GATHER_TIME => {
            // a slot outside the seven: the original wrote past the array; nothing here
            let Some(slot) = usize::try_from(apply.slot).ok().filter(|&k| k < 7) else {
                return;
            };
            let gather = long(s.up_gather_times[slot]);
            let gather = if !share {
                mode_long(gather, mode, value)
            } else {
                gather.wrapping_sub(to_long(long(s.class_gather_times[slot]) as f32 * value + 0.5f32))
            };
            s.up_gather_times[slot] = gather as i64;
            EE_TECH_CIV_STAT_GATHER_TIMES
        }
        K_CIV_STAT_BUILD_TIME => {
            s.up_build_time_scale = if share {
                s.up_build_time_scale * (1.0f32 - value)
            } else {
                mode_f32(s.up_build_time_scale, mode, value)
            };
            EE_TECH_CIV_STAT_BUILD_TIME
        }
        K_CIV_STAT_CONVERSION => {
            let factor = get_f32(h, player, EE_TECH_CIV_FIELD_CONVERSION);
            let factor = if share { factor * value } else { mode_f32(factor, mode, value) };
            set_f32(h, player, EE_TECH_CIV_FIELD_CONVERSION, factor);
            return;
        }
        K_CIV_STAT_REPAIR_RATE => {
            let repair = long(s.up_repair_rate);
            let repair = if !share {
                mode_long(repair, mode, value)
            } else {
                repair.wrapping_add(share_at_least_one(s.class_repair_rate, value))
            };
            s.up_repair_rate = repair as i64;
            EE_TECH_CIV_STAT_REPAIR_RATE
        }
        K_CIV_STAT_AREA_EFFECT_SCALE => {
            if !share {
                s.up_area_effect_scale = mode_f32(s.up_area_effect_scale, mode, value);
            } else {
                s.up_area_effect_scale += share_f32_at_least_one(s.class_area_effect_scale, value);
            }
            put(h, player, unit_type, EE_TECH_CIV_STAT_AREA_EFFECT_SCALE, &s);
            ask(h, EE_TECH_CIV_OP_AREA_EFFECTS, player, 0, 0, 0.0);
            return;
        }
        K_CIV_STAT_HIGH_GROUND => {
            let bonus = get_f32(h, player, EE_TECH_CIV_FIELD_HIGH_GROUND);
            let bonus = if share { bonus + value } else { mode_f32(bonus, mode, value) };
            set_f32(h, player, EE_TECH_CIV_FIELD_HIGH_GROUND, bonus);
            return;
        }
        K_CIV_STAT_POPULATION => {
            population_limit(h, player);
            if share {
                // `mShareBonus += (long)(GetPopulationLimit(0) * value + 0.5f)`: the right side first
                let add = to_long(population_limit(h, player) as f32 * value + 0.5f32);
                let bonus = get_long(h, player, EE_TECH_CIV_FIELD_SHARE_BONUS);
                set_long(h, player, EE_TECH_CIV_FIELD_SHARE_BONUS, bonus.wrapping_add(add));
            } else {
                let bonus = get_long(h, player, EE_TECH_CIV_FIELD_SHARE_BONUS);
                set_long(h, player, EE_TECH_CIV_FIELD_SHARE_BONUS, mode_long(bonus, mode, value));
            }
            population_limit(h, player);
            return;
        }
        K_CIV_STAT_COST => {
            s.up_cost_scale =
                if share { s.up_cost_scale * (1.0f32 - value) } else { mode_f32(s.up_cost_scale, mode, value) };
            EE_TECH_CIV_STAT_COST
        }
        _ => return,
    };
    put(h, player, unit_type, stat, &s);
}

/// `EECivPowersManager::ApplyPower`: a bought power for a player: its technology researched, then civpowers.yaml's
/// effects (C++ primitives the data selects and parameterises).
pub fn apply_power<H: CivHost>(h: &mut H, player: i32, id: i32) {
    let Some(power) = civ::tables().power(id).copied() else {
        return;
    };
    ask(h, EE_TECH_CIV_OP_RESEARCH, player, power.mTechnology as i64, 0, 0.0);
    let Some(rule) = find_power_rule(civ::rules(), id.wrapping_sub(POWER_FIRST)) else {
        return;
    };
    for e in &rule.effects {
        match e.action {
            K_POWER_RESEARCH_IF_RESEARCHED => {
                if ask(h, EE_TECH_CIV_OP_NODE_RESEARCHED, player, e.if_tech as i64, 0, 0.0).value > 0 {
                    ask(h, EE_TECH_CIV_OP_RESEARCH_NODE, player, e.tech as i64, 0, 0.0);
                }
            }
            K_POWER_LOAD_SOUND => {
                ask(h, EE_TECH_CIV_OP_LOAD_SOUND, player, e.sound as i64, 0, 0.0);
            }
            K_POWER_LOAD_EFFECT => {
                ask(h, EE_TECH_CIV_OP_LOAD_EFFECT, player, e.effect as i64, 0, 0.0);
            }
            K_POWER_PULSE_SIGHT => {
                ask(h, EE_TECH_CIV_OP_PULSE_SIGHT, player, e.unit_type as i64, 0, power.mAmount);
            }
            K_POWER_SET_MINING_LIMIT => {
                ask(h, EE_TECH_CIV_OP_MINING_LIMIT, player, power.mMiningLimit as i64, 0, 0.0);
            }
            K_POWER_ENABLE_CAMOUFLAGE => {
                ask(h, EE_TECH_CIV_OP_CAMOUFLAGE, player, 0, 0, 0.0);
            }
            K_POWER_DISALLOW_TECH => {
                ask(h, EE_TECH_CIV_OP_ALLOW_TECH, player, e.tech as i64, 0, 0.0);
            }
            K_POWER_ALLOW_TECH => {
                ask(h, EE_TECH_CIV_OP_ALLOW_TECH, player, e.tech as i64, 1, 0.0);
            }
            _ => {}
        }
    }
}

/// `ApplyUpgrade` over the C++ host ([`apply_upgrade`]).
///
/// # Safety
/// `host` names a live host whose callback answers every op.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_civ_apply_upgrade(
    host: *mut EeTechCivHost,
    upgrade: EeTechCivUpgrade,
    player: i32,
) -> bool {
    // SAFETY: the caller's contract
    apply_upgrade(unsafe { &mut *host }, upgrade, player)
}
