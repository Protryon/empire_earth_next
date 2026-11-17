//! The Set AI Variable effect (`EESMEffect::SetAIVariable`) as data: for each AI behaviour (`kCPBehavior*`,
//! ee/EECPBehavior.h) the computer player's tuning fields it stores and the value each gets from the effect's two
//! integers and its float, in the original's order. The C++ stores them by field number (`EE_SCENARIO_AI_*`, the
//! table in `EESMEffect.cpp`; the 34 building-count fields also into the civilization manager's copy, as the inline
//! setters did) on each chosen computer player.

use std::os::raw::c_long;

use crate::scenario::fields::EeScenarioEffectFields;
use crate::scenario::ftol;

/// `UMFloatToLong` as the Windows build computes it (`UMRint`: `cvtss2si` inside 2^30, the UCRT's `lrintf` beyond):
/// round half to even, exactly 2^31 as `i32::MIN`, and 0 for NaN or a value beyond (the UCRT's answers: the golden
/// `golden_aivar.txt` pins 0 beyond; ee_ge um.rs `lrintf`, measured over the UCRT, the 2^31 case).
#[inline]
pub fn lrintf(value: f32) -> i32 {
    let rounded = value.round_ties_even();
    // exactly 2^31 is the conversion's i32::MIN, as the UCRT's lrintf answers (ee_ge um.rs lrintf, measured)
    if (-2_147_483_648.0..=2_147_483_648.0).contains(&rounded) {
        rounded as i64 as i32
    } else {
        0
    }
}

/// What a write is: a `long` field, a `float` field, a building count (`long`, and the civilization manager's
/// `mMaxCounts`/`mUnitsPer` entry of `unit_type`), or a call.
pub const EE_SCENARIO_AI_KIND_LONG: u32 = 0;
pub const EE_SCENARIO_AI_KIND_FLOAT: u32 = 1;
pub const EE_SCENARIO_AI_KIND_MAX_COUNT: u32 = 2;
pub const EE_SCENARIO_AI_KIND_UNITS_PER: u32 = 3;
pub const EE_SCENARIO_AI_KIND_CALL: u32 = 4;

/// One store on a computer player: `field` the position of the field in its kind's list (`EE_SM_AI_LONGS`,
/// `EE_SM_AI_FLOATS`, `EE_SM_AI_BUILDINGS`, `EE_SM_AI_CALLS` in ee/EESMAIFields.h: the modules below in order).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScenarioAiWrite {
    pub kind: u32,
    pub field: u32,
    pub long_value: c_long,
    pub float_value: f32,
}

/// The most writes one behaviour makes.
pub const EE_SCENARIO_AI_MAX_WRITES: usize = 4;

macro_rules! fields {
    ($kind:ident: $($name:ident = $n:expr,)*) => {
        $(#[doc = concat!("`EEComputerPlayer::m", stringify!($name), "` (", stringify!($kind), ").")]
        pub(crate) const $name: u32 = $n;)*
    };
}

/// The `long` fields (`EE_SCENARIO_AI_KIND_LONG`).
pub mod long {
    fields! { long:
        INITIAL_ATTACK_DELAY_MAX = 0, INITIAL_ATTACK_DELAY_MIN = 1, ATTACK_DELAY_MODIFIER_EASY = 2,
        ATTACK_DELAY_MODIFIER_HARD = 3, ASSIST_ALLIES_DELAY_EASY = 4, ASSIST_ALLIES_DELAY_MEDIUM = 5,
        ASSIST_ALLIES_DELAY_HARD = 6, ATTACK_FREQUENCY_MIN = 7, ATTACK_FREQUENCY_MAX = 8,
        ATTACK_FREQUENCY_MODIFIER_EASY = 9, ATTACK_FREQUENCY_MODIFIER_HARD = 10, NUM_LAND_EXPLORERS = 11,
        NUM_SEA_EXPLORERS = 12, NUM_AIR_EXPLORERS = 13, ATTACK_GROUP_MODIFIER_EASY = 14,
        ATTACK_GROUP_MODIFIER_MEDIUM = 15, ATTACK_GROUP_MODIFIER_HARD = 16, ATTACK_GROUP_SIZE_MIN = 17,
        ATTACK_GROUP_SIZE_MAX = 18, DROP_OFF_DISTANCE = 19, RESEARCH_FREQUENCY = 20, NUM_IMPROVED_UNIT_TYPES = 21,
        NUM_IMPROVED_ATTRIBUTES = 22, LD7C = 23, LD78 = 24, INITIAL_ATTACK_DELAY_PER_EPOCH = 25,
        ATTACK_DELAY_FLOOR = 26, ATTACK_DELAY_CEILING = 27, ATTACK_FREQUENCY_FLOOR = 28,
        ATTACK_FREQUENCY_CEILING = 29, RESEARCH_FREQUENCY_MODIFIER_EASY = 30, RESEARCH_FREQUENCY_MODIFIER_HARD = 31,
        ATTACK_ENEMY_CHANCE = 32, ATTACK_TO_CLAIM_RESOURCE_CHANCE = 33, ATTACK_FOR_REVENGE_CHANCE = 34,
        EPOCH_FREQUENCY_MIN = 35, EPOCH_FREQUENCY_MAX = 36, PRIEST_MAX_PER_GROUP = 37, PROPHET_MAX_PER_GROUP = 38,
        HERO_MAX_PER_GROUP = 39, CARRIER_MAX_PER_GROUP = 40, ENABLE_STOCK_CIVILIZATION = 41,
        ENABLE_STOCK_RESOURCE = 42, ENABLE_STOCK_RESEARCH = 43, ENABLE_STOCK_PRODUCTION = 44,
        ENABLE_STOCK_BUILD = 45, LE80 = 46, ENABLE_STOCK_DEFENSE = 47, ENABLE_STOCK_EXPLORATION = 48,
        ENABLE_STOCK_COMBAT = 49, MANAGER_EPOCH_ADVANCE_WAIT = 50, MANAGER_ATTACK_WAIT = 51,
        MAX_ENABLED_MANAGERS = 52, NUM_CHOKE_POINTS_TO_GUARD = 53, DEFENSE_GROUP_SIZE_MIN = 54,
        DEFENSE_GROUP_SIZE_MAX = 55, DEFENSIVE_UNIT_UPDATE_EASY = 56, DEFENSIVE_UNIT_UPDATE_MEDIUM = 57,
        DEFENSIVE_UNIT_UPDATE_HARD = 58, REPLENISH_DELAY_MIN = 59, REPLENISH_DELAY_MAX = 60,
        UNIT_COST_REDUCTION_EASY = 61, UNIT_COST_REDUCTION_HARD = 62, EPOCH_FREQUENCY_MODIFIER_EASY = 63,
        EPOCH_FREQUENCY_MODIFIER_HARD = 64, EXPANSION_TIMER_EASY = 65, EXPANSION_TIMER_MEDIUM = 66,
        EXPANSION_TIMER_HARD = 67, POP_LIMIT_ADJUSTMENT_EASY = 68, POP_LIMIT_ADJUSTMENT_MEDIUM = 69,
        POP_LIMIT_ADJUSTMENT_HARD = 70, TOWN_CENTER_UPGRADE_TIMER = 71, CAPITOL_UPGRADE_TIMER = 72,
        EPOCH_CATCH_UP_MIN = 73, EPOCH_CATCH_UP_MAX = 74, MIN_MILITARY_BUILDINGS_EASY = 75,
        MIN_MILITARY_BUILDINGS_HARD = 76, MIN_MILITARY_BUILDINGS_MEDIUM = 77, MAX_FLAME_THROWERS = 78,
        TOWN_GUARD_UPDATE_EASY = 79, TOWN_GUARD_UPDATE_MEDIUM = 80, TOWN_GUARD_UPDATE_HARD = 81,
        DM_TOWN_GUARD_UPDATE_EASY = 82, DM_TOWN_GUARD_UPDATE_MEDIUM = 83, DM_TOWN_GUARD_UPDATE_HARD = 84,
        DM_DEFENSIVE_UNIT_UPDATE_EASY = 85, DM_DEFENSIVE_UNIT_UPDATE_MEDIUM = 86,
        DM_DEFENSIVE_UNIT_UPDATE_HARD = 87, SHIP_MAINTENANCE_TIMER = 88, SHIPS_TO_MAINTAIN_EASY = 89,
        SHIPS_TO_MAINTAIN_MEDIUM = 90, SHIPS_TO_MAINTAIN_HARD = 91, SHIPS_TO_MAINTAIN_DM_EASY = 92,
        SHIPS_TO_MAINTAIN_DM_MEDIUM = 93, SHIPS_TO_MAINTAIN_DM_HARD = 94, BONZAI_ATTACK_LEVEL = 95,
    }
}

/// The `float` fields (`EE_SCENARIO_AI_KIND_FLOAT`).
pub mod float {
    fields! { float:
        DEFENSIVE_UNIT_CAP = 0, ATTACK_FREQUENCY_PER_EPOCH = 1, MILITARY_COUNT_WEIGHT = 2, WORKER_COUNT_WEIGHT = 3,
        MILITARY_POP_PERCENTAGE = 4, SPECIALIZED_UNIT_PERCENTAGE = 5, MAX_ATTACK_RESPONSE_RANGE = 6,
        PRIEST_CHANCE = 7, PRIEST_POP_PERCENTAGE = 8, PROPHET_CHANCE = 9, PROPHET_POP_PERCENTAGE = 10,
        HERO_CHANCE = 11, HERO_POP_PERCENTAGE = 12, CARRIER_CHANCE = 13, CARRIER_POP_PERCENTAGE = 14,
        RPS_SELECTION_NON_ISLANDS = 15, UNIT_COST_MEDIUM = 16, ENABLE_INTERVAL = 17, AID_THREAT_THRESHOLD = 18,
        GUARD_CHOKEPOINT_PERCENTAGE = 19, GUARD_CONTINENT_PERCENTAGE = 20, GUARD_HIGH_GROUND_PERCENTAGE = 21,
        GUARD_TOWN_LAND_PERCENTAGE = 22, FC44 = 23, FC50 = 24, FC58 = 25, GUARD_CHOKEPOINT_AIR_PERCENTAGE = 26,
        GUARD_TOWN_PERCENTAGE = 27, FC38 = 28, FC40 = 29, FC4C = 30, GUARD_WATERWAY_PERCENTAGE = 31, FC3C = 32,
        FC48 = 33, FC54 = 34, TOWN_GUARD_PERCENTAGE = 35, COMPUTER_AGGRESSION = 36, DM_EARLY_ATTACK_SCALE = 37,
        SPECIALIZED_UNIT_CHANCE = 38, RPS_SELECTION_ISLANDS = 39, NAVAL_UNIT_WEIGHT = 40, AIR_UNIT_WEIGHT = 41,
        SIEGE_WEAPON_PERCENTAGE = 42,
    }
}

/// The building counts (`EE_SCENARIO_AI_KIND_MAX_COUNT`: `m<Building>Max`; `EE_SCENARIO_AI_KIND_UNITS_PER`:
/// `mUnitsPer<Building>`), each with its unit type in the C++ table.
pub mod building {
    fields! { building:
        BARRACKS = 0, ARCHERY_RANGE = 1, STABLE = 2, DOCK = 3, SIEGE_FACTORY = 4, NAVAL_YARD = 5, AIRPORT = 6,
        CYBER_FACTORY = 7, UNIVERSITY = 8, TEMPLE = 9, HOSPITAL = 10, GRANARY = 11, CYBER_LAB = 12, SPACE_DOCK = 13,
        TOWER = 14, TANK_FACTORY = 15, ANTI_AIR = 16,
    }
}

/// The calls (`EE_SCENARIO_AI_KIND_CALL`): `SetNextAttackTime(gPhysics->mGameTime)`, `SetIslandExpansionTimer`
/// (the `long`), `SetDMAttackFrequencyModifier` and `SetDMInitialAttackModifier` (the `float`).
pub const EE_SCENARIO_AI_CALL_NEXT_ATTACK_TIME_NOW: u32 = 0;
pub const EE_SCENARIO_AI_CALL_ISLAND_EXPANSION_TIMER: u32 = 1;
pub const EE_SCENARIO_AI_CALL_DM_ATTACK_FREQUENCY_MODIFIER: u32 = 2;
pub const EE_SCENARIO_AI_CALL_DM_INITIAL_ATTACK_MODIFIER: u32 = 3;

mod call {
    pub(crate) use super::EE_SCENARIO_AI_CALL_DM_ATTACK_FREQUENCY_MODIFIER as DM_ATTACK_FREQUENCY_MODIFIER;
    pub(crate) use super::EE_SCENARIO_AI_CALL_DM_INITIAL_ATTACK_MODIFIER as DM_INITIAL_ATTACK_MODIFIER;
    pub(crate) use super::EE_SCENARIO_AI_CALL_ISLAND_EXPANSION_TIMER as ISLAND_EXPANSION_TIMER;
    pub(crate) use super::EE_SCENARIO_AI_CALL_NEXT_ATTACK_TIME_NOW as NEXT_ATTACK_TIME_NOW;
}

/// A value of the effect: its first or second integer, their minimum or maximum, a minutes count in ms, the float
/// cut toward zero, rounded, as it is, squared, or an integer percent as a fraction.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum V {
    I1,
    I2,
    Min,
    Max,
    I1Minutes,
    I2Minutes,
    FMinutes,
    FCut,
    FRound,
    F,
    FSquared,
    I1Percent,
    I2Percent,
    None,
}

use V::*;

const L: u32 = EE_SCENARIO_AI_KIND_LONG;
const FL: u32 = EE_SCENARIO_AI_KIND_FLOAT;
const MC: u32 = EE_SCENARIO_AI_KIND_MAX_COUNT;
const UP: u32 = EE_SCENARIO_AI_KIND_UNITS_PER;
const C: u32 = EE_SCENARIO_AI_KIND_CALL;

/// A building count behaviour: the units per building from the first integer, the maximum the float rounded.
const fn count(b: u32) -> [(u32, u32, V); 2] {
    [(UP, b, I1), (MC, b, FRound)]
}

/// The writes of a behaviour, and whether the applied effect is also kept for a saved game's replay
/// (`AddV134Effect`): the building counts but towers, tank factories and anti-aircraft buildings.
fn plan(behaviour: c_long) -> (&'static [(u32, u32, V)], bool) {
    use float as f;
    use long as l;
    macro_rules! w {
        ($($e:expr),* $(,)?) => { &[$($e),*] };
    }
    const COUNTS: [[(u32, u32, V); 2]; 17] = [
        count(building::BARRACKS),
        count(building::ARCHERY_RANGE),
        count(building::STABLE),
        count(building::DOCK),
        count(building::SIEGE_FACTORY),
        count(building::NAVAL_YARD),
        count(building::AIRPORT),
        count(building::CYBER_FACTORY),
        count(building::UNIVERSITY),
        count(building::TEMPLE),
        count(building::HOSPITAL),
        count(building::GRANARY),
        count(building::CYBER_LAB),
        count(building::SPACE_DOCK),
        count(building::TOWER),
        count(building::TANK_FACTORY),
        count(building::ANTI_AIR),
    ];
    let writes: &'static [(u32, u32, V)] = match behaviour {
        1 => w![(L, l::INITIAL_ATTACK_DELAY_MAX, Max), (L, l::INITIAL_ATTACK_DELAY_MIN, Min)],
        2 => w![(L, l::ATTACK_DELAY_MODIFIER_EASY, I1), (L, l::ATTACK_DELAY_MODIFIER_HARD, I2)],
        4 => w![
            (L, l::ASSIST_ALLIES_DELAY_EASY, I1Minutes),
            (L, l::ASSIST_ALLIES_DELAY_MEDIUM, I2Minutes),
            (L, l::ASSIST_ALLIES_DELAY_HARD, FMinutes),
        ],
        6 => w![
            (L, l::ATTACK_FREQUENCY_MIN, Min),
            (L, l::ATTACK_FREQUENCY_MAX, Max),
            (C, call::NEXT_ATTACK_TIME_NOW, None),
        ],
        7 => w![(L, l::ATTACK_FREQUENCY_MODIFIER_EASY, I1), (L, l::ATTACK_FREQUENCY_MODIFIER_HARD, I2)],
        8 => w![(L, l::NUM_LAND_EXPLORERS, I1)],
        9 => w![(L, l::NUM_SEA_EXPLORERS, I1)],
        10 => w![(L, l::NUM_AIR_EXPLORERS, I1)],
        11 => w![
            (L, l::ATTACK_GROUP_MODIFIER_EASY, I1),
            (L, l::ATTACK_GROUP_MODIFIER_MEDIUM, I2),
            (L, l::ATTACK_GROUP_MODIFIER_HARD, FCut),
        ],
        12 => w![(FL, f::MILITARY_COUNT_WEIGHT, I1Percent), (FL, f::WORKER_COUNT_WEIGHT, I2Percent)],
        13 => w![(L, l::ATTACK_GROUP_SIZE_MIN, Min), (L, l::ATTACK_GROUP_SIZE_MAX, Max)],
        14 => w![(L, l::DROP_OFF_DISTANCE, I1)],
        15 => w![(L, l::RESEARCH_FREQUENCY, I1)],
        46 => w![(L, l::NUM_IMPROVED_UNIT_TYPES, I1)],
        47 => w![(L, l::NUM_IMPROVED_ATTRIBUTES, I1)],
        0x10 => w![(L, l::LD7C, Min), (L, l::LD78, Max)],
        18 => w![(L, l::INITIAL_ATTACK_DELAY_PER_EPOCH, I1)],
        19 => w![(L, l::ATTACK_DELAY_FLOOR, Min), (L, l::ATTACK_DELAY_CEILING, Max)],
        20 => w![(FL, f::DEFENSIVE_UNIT_CAP, F)],
        21 => w![(L, l::ATTACK_FREQUENCY_FLOOR, Min), (L, l::ATTACK_FREQUENCY_CEILING, Max)],
        22 => w![(FL, f::ATTACK_FREQUENCY_PER_EPOCH, F)],
        23 => w![(L, l::RESEARCH_FREQUENCY_MODIFIER_EASY, I1), (L, l::RESEARCH_FREQUENCY_MODIFIER_HARD, I2)],
        25 => w![(L, l::ATTACK_ENEMY_CHANCE, I1)],
        26 => w![(L, l::ATTACK_TO_CLAIM_RESOURCE_CHANCE, I1)],
        27 => w![(L, l::ATTACK_FOR_REVENGE_CHANCE, I1)],
        28 => w![(L, l::EPOCH_FREQUENCY_MIN, Min), (L, l::EPOCH_FREQUENCY_MAX, Max)],
        30..=42 => &COUNTS[(behaviour - 30) as usize],
        117 => &COUNTS[13],
        43 => w![(FL, f::MILITARY_POP_PERCENTAGE, F)],
        45 => w![(FL, f::SPECIALIZED_UNIT_PERCENTAGE, F)],
        // the triggers compare it with squared distances: squared as EECPBehaviorTable::Load squares the database's
        48 => w![(FL, f::MAX_ATTACK_RESPONSE_RANGE, FSquared)],
        49 => {
            w![(L, l::PRIEST_MAX_PER_GROUP, I1), (FL, f::PRIEST_CHANCE, I2Percent), (FL, f::PRIEST_POP_PERCENTAGE, F),]
        }
        50 => w![
            (L, l::PROPHET_MAX_PER_GROUP, I1),
            (FL, f::PROPHET_CHANCE, I2Percent),
            (FL, f::PROPHET_POP_PERCENTAGE, F),
        ],
        51 => w![(L, l::HERO_MAX_PER_GROUP, I1), (FL, f::HERO_CHANCE, I2Percent), (FL, f::HERO_POP_PERCENTAGE, F)],
        52 => w![
            (L, l::CARRIER_MAX_PER_GROUP, I1),
            (FL, f::CARRIER_CHANCE, I2Percent),
            (FL, f::CARRIER_POP_PERCENTAGE, F),
        ],
        53 => w![(FL, f::RPS_SELECTION_NON_ISLANDS, F)],
        55 => w![(L, l::ENABLE_STOCK_CIVILIZATION, I1)],
        56 => w![(L, l::ENABLE_STOCK_RESOURCE, I1)],
        57 => w![(L, l::ENABLE_STOCK_RESEARCH, I1)],
        58 => w![(L, l::ENABLE_STOCK_PRODUCTION, I1)],
        59 => w![(L, l::ENABLE_STOCK_BUILD, I1)],
        0x3c => w![(L, l::LE80, I1)],
        61 => w![(L, l::ENABLE_STOCK_DEFENSE, I1)],
        62 => w![(L, l::ENABLE_STOCK_EXPLORATION, I1)],
        63 => w![(L, l::ENABLE_STOCK_COMBAT, I1)],
        64 => w![(L, l::MANAGER_EPOCH_ADVANCE_WAIT, I1)],
        65 => w![(L, l::MANAGER_ATTACK_WAIT, I1)],
        66 => w![(L, l::MAX_ENABLED_MANAGERS, I1)],
        // a percentage, as Init (the user scales it by 0.01)
        67 => w![(FL, f::UNIT_COST_MEDIUM, F)],
        68 => w![(FL, f::ENABLE_INTERVAL, F)],
        69 => w![(FL, f::AID_THREAT_THRESHOLD, F)],
        70 => w![(FL, f::GUARD_CHOKEPOINT_PERCENTAGE, F)],
        71 => w![(FL, f::GUARD_CONTINENT_PERCENTAGE, F)],
        72 => w![(FL, f::GUARD_HIGH_GROUND_PERCENTAGE, F)],
        73 => w![(FL, f::GUARD_TOWN_LAND_PERCENTAGE, F)],
        0x4a => w![(FL, f::FC44, F)],
        0x4b => w![(FL, f::FC50, F)],
        0x4c => w![(FL, f::FC58, F)],
        77 => w![(FL, f::GUARD_CHOKEPOINT_AIR_PERCENTAGE, F)],
        78 => w![(FL, f::GUARD_TOWN_PERCENTAGE, F)],
        0x4f => w![(FL, f::FC38, F)],
        0x50 => w![(FL, f::FC40, F)],
        0x51 => w![(FL, f::FC4C, F)],
        82 => w![(FL, f::GUARD_WATERWAY_PERCENTAGE, F)],
        0x53 => w![(FL, f::FC3C, F)],
        0x54 => w![(FL, f::FC48, F)],
        0x55 => w![(FL, f::FC54, F)],
        86 => w![(L, l::NUM_CHOKE_POINTS_TO_GUARD, I1)],
        87 => w![(L, l::DEFENSE_GROUP_SIZE_MIN, Min), (L, l::DEFENSE_GROUP_SIZE_MAX, Max)],
        88 => w![
            (L, l::DEFENSIVE_UNIT_UPDATE_EASY, I1),
            (L, l::DEFENSIVE_UNIT_UPDATE_MEDIUM, I2),
            (L, l::DEFENSIVE_UNIT_UPDATE_HARD, FCut),
        ],
        89 => w![(L, l::REPLENISH_DELAY_MIN, Min), (L, l::REPLENISH_DELAY_MAX, Max)],
        90 => w![(FL, f::TOWN_GUARD_PERCENTAGE, F)],
        91 => w![(FL, f::COMPUTER_AGGRESSION, F)],
        95 => &COUNTS[14],
        96 => w![(L, l::UNIT_COST_REDUCTION_EASY, I1), (L, l::UNIT_COST_REDUCTION_HARD, I2)],
        29 => w![(L, l::EPOCH_FREQUENCY_MODIFIER_EASY, I1), (L, l::EPOCH_FREQUENCY_MODIFIER_HARD, I2)],
        97 => w![
            (L, l::EXPANSION_TIMER_EASY, I1),
            (L, l::EXPANSION_TIMER_MEDIUM, I2),
            (L, l::EXPANSION_TIMER_HARD, FRound),
        ],
        98 => w![
            (L, l::POP_LIMIT_ADJUSTMENT_EASY, I1),
            (L, l::POP_LIMIT_ADJUSTMENT_MEDIUM, I2),
            (L, l::POP_LIMIT_ADJUSTMENT_HARD, FRound),
        ],
        99 => w![(L, l::TOWN_CENTER_UPGRADE_TIMER, I1)],
        100 => w![(L, l::CAPITOL_UPGRADE_TIMER, I1)],
        102 => w![(L, l::EPOCH_CATCH_UP_MIN, I1), (L, l::EPOCH_CATCH_UP_MAX, I2)],
        103 => w![(L, l::MIN_MILITARY_BUILDINGS_EASY, I1)],
        104 => w![(L, l::MIN_MILITARY_BUILDINGS_HARD, I1)],
        105 => w![(FL, f::DM_EARLY_ATTACK_SCALE, I1Percent)],
        106 => w![(L, l::MIN_MILITARY_BUILDINGS_MEDIUM, I1)],
        108 => &COUNTS[15],
        109 => &COUNTS[16],
        54 => w![(FL, f::SPECIALIZED_UNIT_CHANCE, F)],
        110 => w![(FL, f::RPS_SELECTION_ISLANDS, F)],
        111 => w![(FL, f::NAVAL_UNIT_WEIGHT, F)],
        112 => w![(FL, f::AIR_UNIT_WEIGHT, F)],
        113 => w![(L, l::MAX_FLAME_THROWERS, I1)],
        114 => w![
            (L, l::TOWN_GUARD_UPDATE_EASY, I1),
            (L, l::TOWN_GUARD_UPDATE_MEDIUM, I2),
            (L, l::TOWN_GUARD_UPDATE_HARD, FCut),
        ],
        115 => w![
            (L, l::DM_TOWN_GUARD_UPDATE_EASY, I1),
            (L, l::DM_TOWN_GUARD_UPDATE_MEDIUM, I2),
            (L, l::DM_TOWN_GUARD_UPDATE_HARD, FCut),
        ],
        116 => w![
            (L, l::DM_DEFENSIVE_UNIT_UPDATE_EASY, I1),
            (L, l::DM_DEFENSIVE_UNIT_UPDATE_MEDIUM, I2),
            (L, l::DM_DEFENSIVE_UNIT_UPDATE_HARD, FCut),
        ],
        118 => w![(L, l::SHIP_MAINTENANCE_TIMER, I1)],
        5 => w![
            (L, l::SHIPS_TO_MAINTAIN_EASY, I1),
            (L, l::SHIPS_TO_MAINTAIN_MEDIUM, I2),
            (L, l::SHIPS_TO_MAINTAIN_HARD, FCut),
        ],
        44 => w![
            (L, l::SHIPS_TO_MAINTAIN_DM_EASY, I1),
            (L, l::SHIPS_TO_MAINTAIN_DM_MEDIUM, I2),
            (L, l::SHIPS_TO_MAINTAIN_DM_HARD, FCut),
        ],
        119 => w![(FL, f::SIEGE_WEAPON_PERCENTAGE, F)],
        120 => w![(L, l::BONZAI_ATTACK_LEVEL, I1)],
        121 => w![(C, call::ISLAND_EXPANSION_TIMER, I1)],
        122 => w![(C, call::DM_ATTACK_FREQUENCY_MODIFIER, F)],
        123 => w![(C, call::DM_INITIAL_ATTACK_MODIFIER, F)],
        _ => w![],
    };
    (writes, (30..=42).contains(&behaviour) || behaviour == 117)
}

/// A value of the effect's numbers, as the original's expression (`long` wrapping arithmetic, the C cast's
/// truncation, `lrintf`, `f32` products).
fn value(v: V, e: &EeScenarioEffectFields) -> (c_long, f32) {
    let (i1, i2, fl) = (e.mIntValue1, e.mIntValue2, e.mFloatValue);
    match v {
        I1 => (i1, 0.0),
        I2 => (i2, 0.0),
        Min => (i1.min(i2), 0.0),
        Max => (i1.max(i2), 0.0),
        I1Minutes => (i1.wrapping_mul(60000), 0.0),
        I2Minutes => (i2.wrapping_mul(60000), 0.0),
        FMinutes => (ftol(fl * 60000.0) as c_long, 0.0),
        FCut => (ftol(fl) as c_long, 0.0),
        FRound => (lrintf(fl) as c_long, 0.0),
        F => (0, fl),
        FSquared => (0, fl * fl),
        I1Percent => (0, i1 as f32 * 0.01),
        I2Percent => (0, i2 as f32 * 0.01),
        None => (0, 0.0),
    }
}

/// `EESMEffect::SetAIVariable`'s stores for the effect's behaviour (`mAIAttribute`) into `out`, in order; returns
/// whether the effect is kept for a saved game (`AddV134Effect` once per computer player it applies to).
pub fn ai_variable(e: &EeScenarioEffectFields, out: &mut Vec<EeScenarioAiWrite>) -> bool {
    let (writes, deferred) = plan(e.mAIAttribute as c_long);
    for &(kind, field, v) in writes {
        let (long_value, float_value) = value(v, e);
        out.push(EeScenarioAiWrite { kind, field, long_value, float_value });
    }
    deferred
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::scenario::fields::effect_defaults;

    fn effect(attribute: u32, i1: c_long, i2: c_long, f: f32) -> EeScenarioEffectFields {
        let mut e = effect_defaults();
        e.mAIAttribute = attribute as _;
        e.mIntValue1 = i1;
        e.mIntValue2 = i2;
        e.mFloatValue = f;
        e
    }

    #[test]
    fn lrintf_rounds_half_to_even_and_gives_0_out_of_range() {
        assert_eq!((lrintf(2.5), lrintf(3.5), lrintf(-2.5), lrintf(f32::NAN)), (2, 4, -2, 0));
        assert_eq!((lrintf(1.5e9), lrintf(-2.5e9), lrintf(f32::INFINITY)), (1_500_000_000, 0, 0));
        assert_eq!((lrintf(2_147_483_648.0), lrintf(-2_147_483_648.0), lrintf(4.3e9)), (i32::MIN, i32::MIN, 0));
    }

    /// The C++ names of the field numbers (ee/EESMAIFields.h's lists), for the golden's lines.
    const LONG_NAMES: [&str; 96] = [
        "mInitialAttackDelayMax",
        "mInitialAttackDelayMin",
        "mAttackDelayModifierEasy",
        "mAttackDelayModifierHard",
        "mAssistAlliesDelayEasy",
        "mAssistAlliesDelayMedium",
        "mAssistAlliesDelayHard",
        "mAttackFrequencyMin",
        "mAttackFrequencyMax",
        "mAttackFrequencyModifierEasy",
        "mAttackFrequencyModifierHard",
        "mNumLandExplorers",
        "mNumSeaExplorers",
        "mNumAirExplorers",
        "mAttackGroupModifierEasy",
        "mAttackGroupModifierMedium",
        "mAttackGroupModifierHard",
        "mAttackGroupSizeMin",
        "mAttackGroupSizeMax",
        "mDropOffDistance",
        "mResearchFrequency",
        "mNumImprovedUnitTypes",
        "mNumImprovedAttributes",
        "mLD7C",
        "mLD78",
        "mInitialAttackDelayPerEpoch",
        "mAttackDelayFloor",
        "mAttackDelayCeiling",
        "mAttackFrequencyFloor",
        "mAttackFrequencyCeiling",
        "mResearchFrequencyModifierEasy",
        "mResearchFrequencyModifierHard",
        "mAttackEnemyChance",
        "mAttackToClaimResourceChance",
        "mAttackForRevengeChance",
        "mEpochFrequencyMin",
        "mEpochFrequencyMax",
        "mPriestMaxPerGroup",
        "mProphetMaxPerGroup",
        "mHeroMaxPerGroup",
        "mCarrierMaxPerGroup",
        "mEnableStockCivilization",
        "mEnableStockResource",
        "mEnableStockResearch",
        "mEnableStockProduction",
        "mEnableStockBuild",
        "mLE80",
        "mEnableStockDefense",
        "mEnableStockExploration",
        "mEnableStockCombat",
        "mManagerEpochAdvanceWait",
        "mManagerAttackWait",
        "mMaxEnabledManagers",
        "mNumChokePointsToGuard",
        "mDefenseGroupSizeMin",
        "mDefenseGroupSizeMax",
        "mDefensiveUnitUpdateEasy",
        "mDefensiveUnitUpdateMedium",
        "mDefensiveUnitUpdateHard",
        "mReplenishDelayMin",
        "mReplenishDelayMax",
        "mUnitCostReductionEasy",
        "mUnitCostReductionHard",
        "mEpochFrequencyModifierEasy",
        "mEpochFrequencyModifierHard",
        "mExpansionTimerEasy",
        "mExpansionTimerMedium",
        "mExpansionTimerHard",
        "mPopLimitAdjustmentEasy",
        "mPopLimitAdjustmentMedium",
        "mPopLimitAdjustmentHard",
        "mTownCenterUpgradeTimer",
        "mCapitolUpgradeTimer",
        "mEpochCatchUpMin",
        "mEpochCatchUpMax",
        "mMinMilitaryBuildingsEasy",
        "mMinMilitaryBuildingsHard",
        "mMinMilitaryBuildingsMedium",
        "mMaxFlameThrowers",
        "mTownGuardUpdateEasy",
        "mTownGuardUpdateMedium",
        "mTownGuardUpdateHard",
        "mDMTownGuardUpdateEasy",
        "mDMTownGuardUpdateMedium",
        "mDMTownGuardUpdateHard",
        "mDMDefensiveUnitUpdateEasy",
        "mDMDefensiveUnitUpdateMedium",
        "mDMDefensiveUnitUpdateHard",
        "mShipMaintenanceTimer",
        "mShipsToMaintainEasy",
        "mShipsToMaintainMedium",
        "mShipsToMaintainHard",
        "mShipsToMaintainDMEasy",
        "mShipsToMaintainDMMedium",
        "mShipsToMaintainDMHard",
        "mBonzaiAttackLevel",
    ];
    const FLOAT_NAMES: [&str; 43] = [
        "mDefensiveUnitCap",
        "mAttackFrequencyPerEpoch",
        "mMilitaryCountWeight",
        "mWorkerCountWeight",
        "mMilitaryPopPercentage",
        "mSpecializedUnitPercentage",
        "mMaxAttackResponseRange",
        "mPriestChance",
        "mPriestPopPercentage",
        "mProphetChance",
        "mProphetPopPercentage",
        "mHeroChance",
        "mHeroPopPercentage",
        "mCarrierChance",
        "mCarrierPopPercentage",
        "mRPSSelectionNonIslands",
        "mUnitCostMedium",
        "mEnableInterval",
        "mAidThreatThreshold",
        "mGuardChokepointPercentage",
        "mGuardContinentPercentage",
        "mGuardHighGroundPercentage",
        "mGuardTownLandPercentage",
        "mFC44",
        "mFC50",
        "mFC58",
        "mGuardChokepointAirPercentage",
        "mGuardTownPercentage",
        "mFC38",
        "mFC40",
        "mFC4C",
        "mGuardWaterwayPercentage",
        "mFC3C",
        "mFC48",
        "mFC54",
        "mTownGuardPercentage",
        "mComputerAggression",
        "mDMEarlyAttackScale",
        "mSpecializedUnitChance",
        "mRPSSelectionIslands",
        "mNavalUnitWeight",
        "mAirUnitWeight",
        "mSiegeWeaponPercentage",
    ];
    /// The buildings' `m<Building>Max`, `mUnitsPer<Building>` and unit type.
    const BUILDINGS: [(&str, &str, u32); 17] = [
        ("mBarracksMax", "mUnitsPerBarracks", 0x22),
        ("mArcheryRangeMax", "mUnitsPerArcheryRange", 0x36),
        ("mStableMax", "mUnitsPerStable", 0x40),
        ("mDockMax", "mUnitsPerDock", 0x24),
        ("mSiegeFactoryMax", "mUnitsPerSiegeFactory", 0x3f),
        ("mNavalYardMax", "mUnitsPerNavalYard", 0x41),
        ("mAirportMax", "mUnitsPerAirport", 0x42),
        ("mCyberFactoryMax", "mUnitsPerCyberFactory", 0x45),
        ("mUniversityMax", "mUnitsPerUniversity", 0x3c),
        ("mTempleMax", "mUnitsPerTemple", 0x27),
        ("mHospitalMax", "mUnitsPerHospital", 0x3e),
        ("mGranaryMax", "mUnitsPerGranary", 0x2a),
        ("mCyberLabMax", "mUnitsPerCyberLab", 0x24a),
        ("mSpaceDockMax", "mUnitsPerSpaceDock", 0x2d8),
        ("mTowerMax", "mUnitsPerTower", 0x2f),
        ("mTankFactoryMax", "mUnitsPerTankFactory", 0x44),
        ("mAntiAirMax", "mUnitsPerAntiAirBuilding", 0x91),
    ];
    const CALL_NAMES: [&str; 4] =
        ["SetNextAttackTime", "SetIslandExpansionTimer", "SetDMAttackFrequencyModifier", "SetDMInitialAttackModifier"];

    /// A golden value: a `long` or the bits of a `float` (any NaN one value).
    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    enum Val {
        L(i64),
        F(u32),
    }

    fn float_of(text: &str) -> f32 {
        match text {
            "nan" | "-nan" | "-nan(ind)" | "nan(ind)" => f32::NAN,
            "inf" => f32::INFINITY,
            "-inf" => f32::NEG_INFINITY,
            _ => text.parse::<f32>().unwrap(),
        }
    }

    fn fbits(v: f32) -> u32 {
        if v.is_nan() {
            0x7fc0_0000
        } else {
            v.to_bits()
        }
    }

    /// `golden_aivar.txt` (TEST ONLY scratch driver over the original switch and its 34 inline setters, a stand-in
    /// computer player whose fields start at a sentinel: per behaviour 0..130 and value set, the calls made, the
    /// deferred flag, every field stored and the civilization manager's counts) against the plan applied to a model.
    #[test]
    fn golden_matches_the_original_switch() {
        let text = include_str!("golden_aivar.txt");
        let mut lines = 0;
        for line in text.lines() {
            let (head, rest) = line.split_once(':').unwrap();
            let mut it = head.split(' ');
            let b: u32 = it.next().unwrap()[2..].parse().unwrap();
            let i1 = it.next().unwrap()[3..].parse::<i64>().unwrap() as c_long;
            let i2 = it.next().unwrap()[3..].parse::<i64>().unwrap() as c_long;
            let f = float_of(&it.next().unwrap()[2..]);
            // the golden: calls in order, then the deferred flag and the stores by name
            let mut want_calls = Vec::new();
            let mut want = std::collections::BTreeMap::new();
            let mut want_deferred = false;
            for tok in rest.split(' ').filter(|t| !t.is_empty()) {
                if tok == "call" {
                    continue;
                }
                if let Some((name, arg)) = tok.strip_suffix(')').and_then(|t| t.split_once('(')) {
                    want_calls.push((name.to_string(), arg.to_string()));
                } else if let Some(d) = tok.strip_prefix("deferred=") {
                    want_deferred = d == "1";
                } else {
                    let (name, value) = tok.split_once('=').unwrap();
                    let v = if FLOAT_NAMES.contains(&name) {
                        Val::F(fbits(float_of(value)))
                    } else {
                        Val::L(value.parse().unwrap())
                    };
                    want.insert(name.to_string(), v);
                }
            }
            // the plan applied to the model
            let mut out = Vec::new();
            let deferred = ai_variable(&effect(b, i1, i2, f), &mut out);
            let mut got_calls = Vec::new();
            let mut got = std::collections::BTreeMap::new();
            for w in &out {
                let lv = Val::L(w.long_value as i64);
                match w.kind {
                    L => {
                        got.insert(LONG_NAMES[w.field as usize].to_string(), lv);
                    }
                    FL => {
                        got.insert(FLOAT_NAMES[w.field as usize].to_string(), Val::F(fbits(w.float_value)));
                    }
                    MC | UP => {
                        let (max, per, t) = BUILDINGS[w.field as usize];
                        let (name, civ) = if w.kind == MC { (max, "max") } else { (per, "per") };
                        got.insert(name.to_string(), lv);
                        got.insert(format!("{civ}[{t}]"), lv);
                    }
                    _ => {
                        let arg = match w.field {
                            call::NEXT_ATTACK_TIME_NOW => "777".to_string(),
                            call::ISLAND_EXPANSION_TIMER => (w.long_value as u32).to_string(),
                            _ => format!("{}", w.float_value),
                        };
                        got_calls.push((CALL_NAMES[w.field as usize].to_string(), arg));
                    }
                }
            }
            assert_eq!(deferred, want_deferred, "{line}");
            assert_eq!(got, want, "{line}");
            // the float calls' arguments are printed %.9g: compared as floats
            assert_eq!(got_calls.len(), want_calls.len(), "{line}");
            for ((gn, ga), (wn, wa)) in got_calls.iter().zip(&want_calls) {
                assert_eq!(gn, wn, "{line}");
                if gn.starts_with("SetDM") {
                    assert_eq!(fbits(float_of(ga)), fbits(float_of(wa)), "{line}");
                } else {
                    assert_eq!(ga, wa, "{line}");
                }
            }
            lines += 1;
        }
        assert_eq!(lines, 130 * 6 * 11);
    }

    #[test]
    fn ranges_take_min_and_max_and_minutes_wrap() {
        let mut out = Vec::new();
        assert!(!ai_variable(&effect(1, 9, 4, 0.0), &mut out));
        assert_eq!(
            out[0],
            EeScenarioAiWrite { kind: L, field: long::INITIAL_ATTACK_DELAY_MAX, long_value: 9, float_value: 0.0 }
        );
        assert_eq!(out[1].long_value, 4);
        out.clear();
        ai_variable(&effect(4, 2, 100_000, 1.5), &mut out);
        let got: Vec<c_long> = out.iter().map(|w| w.long_value).collect();
        assert_eq!(got, vec![120_000, 100_000i32.wrapping_mul(60000) as c_long, 90_000]);
    }

    #[test]
    fn building_counts_round_the_float_and_most_are_kept() {
        let mut out = Vec::new();
        assert!(ai_variable(&effect(30, 6, 0, 2.5), &mut out));
        assert_eq!(out.len(), 2);
        assert_eq!((out[0].kind, out[0].field, out[0].long_value), (UP, building::BARRACKS, 6));
        assert_eq!((out[1].kind, out[1].field, out[1].long_value), (MC, building::BARRACKS, 2));
        for (behaviour, b, kept) in [
            (42, building::CYBER_LAB, true),
            (117, building::SPACE_DOCK, true),
            (95, building::TOWER, false),
            (108, building::TANK_FACTORY, false),
            (109, building::ANTI_AIR, false),
        ] {
            out.clear();
            assert_eq!(ai_variable(&effect(behaviour, 1, 0, 3.5), &mut out), kept);
            assert_eq!((out[1].field, out[1].long_value), (b, 4));
        }
    }

    #[test]
    fn floats_percentages_squares_cuts_and_calls() {
        let mut out = Vec::new();
        ai_variable(&effect(48, 0, 0, 3.0), &mut out);
        assert_eq!((out[0].kind, out[0].float_value), (FL, 9.0));
        out.clear();
        ai_variable(&effect(49, 3, 25, 0.5), &mut out);
        assert_eq!(out[1].float_value, 25.0f32 * 0.01);
        assert_eq!(out[2].float_value, 0.5);
        out.clear();
        ai_variable(&effect(11, 1, 2, -2.7), &mut out);
        assert_eq!(out[2].long_value, -2);
        out.clear();
        ai_variable(&effect(6, 5, 3, 0.0), &mut out);
        assert_eq!(
            out.iter().map(|w| (w.kind, w.field)).collect::<Vec<_>>(),
            vec![(L, long::ATTACK_FREQUENCY_MIN), (L, long::ATTACK_FREQUENCY_MAX), (C, call::NEXT_ATTACK_TIME_NOW),]
        );
        out.clear();
        ai_variable(&effect(121, 7, 0, 0.0), &mut out);
        assert_eq!((out[0].kind, out[0].field, out[0].long_value), (C, call::ISLAND_EXPANSION_TIMER, 7));
        // the unhandled behaviours store nothing
        for b in [0, 3, 17, 24, 92, 93, 94, 101, 107, 124, 125, 200] {
            out.clear();
            assert!(!ai_variable(&effect(b, 1, 2, 3.0), &mut out));
            assert!(out.is_empty(), "{b}");
        }
    }

    #[test]
    fn no_behaviour_writes_more_than_the_buffer() {
        for b in 0..256 {
            let mut out = Vec::new();
            ai_variable(&effect(b, 1, 2, 3.0), &mut out);
            assert!(out.len() <= EE_SCENARIO_AI_MAX_WRITES);
        }
    }
}
