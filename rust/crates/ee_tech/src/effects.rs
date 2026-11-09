//! The event effects' arithmetic and dispatch (`src/Empire Earth/Scenario Manager/EEEvent.cpp`). An effect record
//! (`EEEventEffect`, a dbeffects row) changes a value in one of three ways, tested in this order: an offset added to
//! the current value, a factor of the class's base value added to the current value, or a value set outright; with
//! all three 0 the float form gives 0 and the integer form 0.

use crate::{ftol, um_ceil};

/// The three ways an effect changes a value (`EEEventEffect::mValue`, `mOffset`, `mFactor`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeTechEffectValues {
    pub value: f32,
    pub offset: f32,
    pub factor: f32,
}

/// `EEApplyEventEffect(float, float, EEEventEffect*)`: `base + offset`, `value * factor + base`, or the set value.
pub fn apply_f32(value: f32, base: f32, effect: &EeTechEffectValues) -> f32 {
    if effect.offset != 0.0 {
        base + effect.offset
    } else if effect.factor != 0.0 {
        value * effect.factor + base
    } else if effect.value != 0.0 {
        effect.value
    } else {
        0.0
    }
}

/// `EEApplyEventEffect(long, long, EEEventEffect*)`: as [`apply_f32`] on integers; the offset and the set value are cast
/// toward zero, the factor's product rounded up by 0.0001 first.
pub fn apply_i32(value: i32, base: i32, effect: &EeTechEffectValues) -> i32 {
    if effect.offset != 0.0 {
        ftol(effect.offset).wrapping_add(base)
    } else if effect.factor != 0.0 {
        ftol(value as f32 * effect.factor + 0.0001).wrapping_add(base)
    } else if effect.value != 0.0 {
        ftol(effect.value)
    } else {
        0
    }
}

/// `ApplyEventGatherRate`: one gather time of an upgrade: the offset added, the factor of the class's time added, or
/// the value set; unchanged when all three are 0.
pub fn gather_time(current: i32, class_time: i32, effect: &EeTechEffectValues) -> i32 {
    if effect.offset != 0.0 {
        current.wrapping_add(ftol(effect.offset))
    } else if effect.factor != 0.0 {
        current.wrapping_add(ftol(class_time as f32 * effect.factor))
    } else if effect.value != 0.0 {
        ftol(effect.value)
    } else {
        current
    }
}

/// Attribute 6: the new maximum hit points. The base is the class's hit points scaled by the game variant's factor for
/// the unit type, the current maximum the upgrade's; never below 1.
pub fn max_hit_points(
    class_hit_points: i32,
    variant_factor: f32,
    old_maximum: u32,
    effect: &EeTechEffectValues,
) -> i32 {
    let base = ftol((variant_factor + 1.0) * class_hit_points as f32);
    let maximum = apply_i32(base, old_maximum as i32, effect);
    if maximum <= 1 {
        1
    } else {
        maximum
    }
}

/// Attributes 6 and 10: a unit's hit points (or flight time) kept in proportion to the new maximum,
/// `(float)UMCeil((float)value / oldMaximum * newMaximum)`; the caller casts it toward zero.
pub fn rescale(value: i32, old_maximum: u32, new_maximum: i32) -> f32 {
    um_ceil(value as f32 / old_maximum as f32 * new_maximum as f32) as f32
}

/// Attribute 23: the player's population limit bonus plus the offset.
pub fn population_bonus(bonus: i32, effect: &EeTechEffectValues) -> i32 {
    bonus.wrapping_add(ftol(effect.offset))
}

/// Attribute 42's terrain types (`mTerrainType`): 0..16.
pub fn terrain_type_valid(terrain: i32) -> bool {
    (0..17).contains(&terrain)
}

/// Attribute 42 and the pathfinding functor: a terrain type allowed (its bit set) or forbidden in an upgrade's mask.
pub fn terrain_mask(mask: u32, terrain: i32, allowed: bool) -> u32 {
    let bit = 1u32 << (terrain as u32 & 31);
    if allowed {
        mask | bit
    } else {
        mask & !bit
    }
}

/// What an attribute effect (`ApplyEventAttribute`, kind 2) changes. The player's attributes (23, 38, 39) apply once,
/// the others to the upgrade of every unit class the effect names.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeTechAttributeOp {
    /// an attribute no case names: the result 9
    Unknown = 0,
    /// 22, 24, 25, 34, 35: nothing
    Nothing,
    /// 23: the population limit bonus ([`population_bonus`])
    PopulationBonus,
    /// 38: the tribute fee, set to the factor
    TributeFee,
    /// 39: the player's flag `mBB44`
    PlayerFlag,
    /// 1: attack (float; the class's integer attack as the base value)
    Attack,
    /// 2: reload time (integer)
    ReloadTime,
    /// 3: range (float, through `SetRange`)
    Range,
    /// 4: line of sight (float), the player's units of the type off the sight grid and back
    LineOfSight,
    /// 5: area effect (float)
    AreaEffect,
    /// 6: maximum hit points ([`max_hit_points`]), the living built units rescaled
    MaxHitPoints,
    /// 9: maximum velocity (float, through `SetMaxVelocity`)
    MaxVelocity,
    /// 10: flight time (integer), the flying units rescaled
    FlightTime,
    /// 11: leadership (float; the class's integer leadership as the base value)
    Leadership,
    /// 16..21: one armour (float; the class's integer armour as the base); `index` is the armour's slot
    Armor,
    /// 26..29, 31..33: one gather time ([`gather_time`]); `index` is the resource's slot
    GatherTime,
    /// 36: converts priests
    ConvertsPriests,
    /// 37: converts buildings
    ConvertsBuildings,
    /// 40: the area effect template, the living units' area effects rebuilt
    AreaEffectTemplate,
    /// 41: power regeneration (integer)
    PowerRegeneration,
    /// 42: a terrain type allowed or forbidden ([`terrain_mask`])
    Terrain,
    /// 43: regeneration (integer)
    Regeneration,
    /// 44: power (integer)
    Power,
    /// 45: the upgrade's name, the string `lrintf(value)`
    Name,
}

/// An attribute's operation and slot (the armour or gather time index; 0 otherwise).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct EeTechAttribute {
    pub op: EeTechAttributeOp,
    pub index: i32,
}

/// `ApplyEventAttribute`'s switch: what attribute `attribute` changes.
pub fn attribute(attribute: i32) -> EeTechAttribute {
    use EeTechAttributeOp as Op;
    let (op, index) = match attribute {
        1 => (Op::Attack, 0),
        2 => (Op::ReloadTime, 0),
        3 => (Op::Range, 0),
        4 => (Op::LineOfSight, 0),
        5 => (Op::AreaEffect, 0),
        6 => (Op::MaxHitPoints, 0),
        9 => (Op::MaxVelocity, 0),
        10 => (Op::FlightTime, 0),
        11 => (Op::Leadership, 0),
        // the armour slots in the record's order of the attributes
        16 => (Op::Armor, 0),
        17 => (Op::Armor, 2),
        18 => (Op::Armor, 1),
        19 => (Op::Armor, 3),
        20 => (Op::Armor, 5),
        21 => (Op::Armor, 4),
        22 | 24 | 25 | 34 | 35 => (Op::Nothing, 0),
        23 => (Op::PopulationBonus, 0),
        26 => (Op::GatherTime, 0),
        27 => (Op::GatherTime, 1),
        28 => (Op::GatherTime, 2),
        29 => (Op::GatherTime, 3),
        31 => (Op::GatherTime, 4),
        32 => (Op::GatherTime, 5),
        33 => (Op::GatherTime, 6),
        36 => (Op::ConvertsPriests, 0),
        37 => (Op::ConvertsBuildings, 0),
        38 => (Op::TributeFee, 0),
        39 => (Op::PlayerFlag, 0),
        40 => (Op::AreaEffectTemplate, 0),
        41 => (Op::PowerRegeneration, 0),
        42 => (Op::Terrain, 0),
        43 => (Op::Regeneration, 0),
        44 => (Op::Power, 0),
        45 => (Op::Name, 0),
        _ => (Op::Unknown, 0),
    };
    EeTechAttribute { op, index }
}

/// Whether the operation applies to the player rather than to unit classes (`ApplyEventAttribute` builds no class list
/// for these).
pub fn attribute_is_player(op: EeTechAttributeOp) -> bool {
    matches!(op, EeTechAttributeOp::PopulationBonus | EeTechAttributeOp::TributeFee | EeTechAttributeOp::PlayerFlag)
}

/// `EEEvent::RunEffect`'s dispatch: which handler runs an effect of a kind.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeTechEffectHandler {
    /// a bad effect index, or an effect that does not run while a saved game loads: the result 9
    Refused = 0,
    /// an unknown kind: the result 3
    UnknownKind,
    /// the result 0 without a handler (kind 22; kind 19 while events are off or a saved game loads)
    Nothing,
    /// kind 7: `EEEventKind7` (an empty handler)
    Kind7,
    /// kind 2: `ApplyEventAttribute`
    Attribute,
    /// kind 8: `SetEventPrerequisite`
    Prerequisite,
    /// kind 9: `MakeEventTechnologyObsolete`
    Obsolete,
    /// kind 6: `SetEventUnitModel`
    UnitModel,
    /// kinds 12..14: `SetEventTechTreeMedia`
    TechTreeMedia,
    /// kind 10: `ResearchEventTechnology`
    Research,
    /// kind 1: `SetEventButton`
    Button,
    /// kind 11: `PlayEventSound`
    Sound,
    /// kinds 15..18, 20, 21: `SetEventUnitSound`
    UnitSound,
    /// kind 19: `ConvertEventUnits`
    ConvertUnits,
}

/// `RunEffect`: the handler of an effect. `known`: the index names an effect; `loading`: `gLoadingSavedGame`;
/// `run_on_load`: the effect's `mRunOnLoad`; `events_enabled`: `gEventsEnabled`.
pub fn effect_handler(
    known: bool,
    kind: i32,
    loading: bool,
    run_on_load: u8,
    events_enabled: bool,
) -> EeTechEffectHandler {
    use EeTechEffectHandler as H;
    if !known || (loading && run_on_load != 1) {
        return H::Refused;
    }
    match kind {
        2 => H::Attribute,
        8 => H::Prerequisite,
        9 => H::Obsolete,
        6 => H::UnitModel,
        12..=14 => H::TechTreeMedia,
        10 => H::Research,
        1 => H::Button,
        7 => H::Kind7,
        22 => H::Nothing,
        11 => H::Sound,
        15..=18 | 20 | 21 => H::UnitSound,
        19 if events_enabled && !loading => H::ConvertUnits,
        19 => H::Nothing,
        _ => H::UnknownKind,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn fx(value: f32, offset: f32, factor: f32) -> EeTechEffectValues {
        EeTechEffectValues { value, offset, factor }
    }

    #[test]
    fn the_float_form_tests_offset_then_factor_then_value() {
        assert_eq!(apply_f32(10.0, 12.0, &fx(3.0, 2.0, 0.5)), 14.0);
        assert_eq!(apply_f32(10.0, 12.0, &fx(3.0, 0.0, 0.5)), 17.0);
        assert_eq!(apply_f32(10.0, 12.0, &fx(3.0, 0.0, 0.0)), 3.0);
        assert_eq!(apply_f32(10.0, 12.0, &fx(0.0, 0.0, 0.0)), 0.0);
        assert_eq!(apply_f32(10.0, 12.0, &fx(0.0, -12.0, 0.0)), 0.0);
        // -0.0 compares equal to 0: the next way is taken
        assert_eq!(apply_f32(10.0, 12.0, &fx(1.0, -0.0, 0.0)), 1.0);
        // the product, then the sum, in f32
        let v = 0.1f32;
        assert_eq!(apply_f32(v, 0.2, &fx(0.0, 0.0, 0.3)).to_bits(), (v * 0.3 + 0.2).to_bits());
    }

    #[test]
    fn the_integer_form_truncates_and_rounds_the_product_up_a_little() {
        assert_eq!(apply_i32(10, 12, &fx(3.0, 2.9, 0.5)), 14);
        assert_eq!(apply_i32(10, 12, &fx(3.0, -2.9, 0.5)), 10);
        assert_eq!(apply_i32(10, 12, &fx(3.0, 0.0, 0.5)), 17);
        // 0.7 * 10 = 6.9999995 in f32; + 0.0001 makes it 7
        assert_eq!(apply_i32(10, 0, &fx(0.0, 0.0, 0.7)), 7);
        assert_eq!(apply_i32(10, 12, &fx(3.7, 0.0, 0.0)), 3);
        assert_eq!(apply_i32(10, 12, &fx(0.0, 0.0, 0.0)), 0);
        assert_eq!(apply_i32(10, i32::MAX, &fx(0.0, 1.0, 0.0)), i32::MIN);
        assert_eq!(apply_i32(10, 0, &fx(0.0, f32::NAN, 0.0)), i32::MIN);
    }

    #[test]
    fn gather_times_add_or_set() {
        assert_eq!(gather_time(100, 80, &fx(0.0, -10.5, 0.0)), 90);
        assert_eq!(gather_time(100, 80, &fx(0.0, 0.0, -0.25)), 80);
        assert_eq!(gather_time(100, 80, &fx(55.9, 0.0, 0.0)), 55);
        assert_eq!(gather_time(100, 80, &fx(0.0, 0.0, 0.0)), 100);
    }

    #[test]
    fn maximum_hit_points_scale_the_class_and_stay_positive() {
        assert_eq!(max_hit_points(100, 0.0, 100, &fx(0.0, 0.0, 0.2)), 120);
        assert_eq!(max_hit_points(100, 0.5, 150, &fx(0.0, 0.0, 0.2)), 180);
        assert_eq!(max_hit_points(100, 0.0, 100, &fx(0.0, -500.0, 0.0)), 1);
        assert_eq!(max_hit_points(100, 0.0, 100, &fx(0.0, 0.0, 0.0)), 1);
        assert_eq!(max_hit_points(100, 0.0, 100, &fx(250.0, 0.0, 0.0)), 250);
    }

    #[test]
    fn rescaling_keeps_the_proportion_rounded_up() {
        assert_eq!(rescale(50, 100, 120), 60.0);
        assert_eq!(rescale(33, 100, 120), 40.0); // 39.6 -> 40
        assert_eq!(rescale(1, 3, 2), 1.0); // 0.6666 -> 1
        assert_eq!(rescale(0, 100, 120), 0.0);
        assert_eq!(rescale(7, 10, 15).to_bits(), 11.0f32.to_bits()); // 10.5: lrintf 10, then up
    }

    #[test]
    fn terrain_masks() {
        assert_eq!(terrain_mask(0, 3, true), 8);
        assert_eq!(terrain_mask(8, 3, true), 8);
        assert_eq!(terrain_mask(0xff, 3, false), 0xf7);
        assert_eq!(terrain_mask(0, 16, true), 0x10000);
        assert!(terrain_type_valid(0) && terrain_type_valid(16));
        assert!(!terrain_type_valid(-1) && !terrain_type_valid(17));
        assert_eq!(population_bonus(5, &fx(0.0, 10.9, 0.0)), 15);
    }

    #[test]
    fn attributes_map_to_the_stats() {
        use EeTechAttributeOp as Op;
        assert_eq!(attribute(1), EeTechAttribute { op: Op::Attack, index: 0 });
        assert_eq!(attribute(17), EeTechAttribute { op: Op::Armor, index: 2 });
        assert_eq!(attribute(18), EeTechAttribute { op: Op::Armor, index: 1 });
        assert_eq!(attribute(20), EeTechAttribute { op: Op::Armor, index: 5 });
        assert_eq!(attribute(21), EeTechAttribute { op: Op::Armor, index: 4 });
        assert_eq!(attribute(29), EeTechAttribute { op: Op::GatherTime, index: 3 });
        assert_eq!(attribute(30).op, Op::Unknown);
        assert_eq!(attribute(31), EeTechAttribute { op: Op::GatherTime, index: 4 });
        assert_eq!(attribute(33), EeTechAttribute { op: Op::GatherTime, index: 6 });
        for a in [7, 8, 12, 13, 14, 15, 46, -1, 0] {
            assert_eq!(attribute(a).op, Op::Unknown, "{a}");
        }
        for a in [22, 24, 25, 34, 35] {
            assert_eq!(attribute(a).op, Op::Nothing);
        }
        assert!(attribute_is_player(attribute(23).op));
        assert!(attribute_is_player(attribute(38).op));
        assert!(attribute_is_player(attribute(39).op));
        assert!(!attribute_is_player(attribute(1).op));
    }

    #[test]
    fn effects_dispatch_by_kind() {
        use EeTechEffectHandler as H;
        assert_eq!(effect_handler(false, 2, false, 0, true), H::Refused);
        assert_eq!(effect_handler(true, 2, true, 0, true), H::Refused);
        assert_eq!(effect_handler(true, 2, true, 1, true), H::Attribute);
        assert_eq!(effect_handler(true, 19, false, 0, true), H::ConvertUnits);
        assert_eq!(effect_handler(true, 19, false, 0, false), H::Nothing);
        assert_eq!(effect_handler(true, 19, true, 1, true), H::Nothing);
        assert_eq!(effect_handler(true, 7, false, 0, true), H::Kind7);
        assert_eq!(effect_handler(true, 22, false, 0, true), H::Nothing);
        assert_eq!(effect_handler(true, 13, false, 0, true), H::TechTreeMedia);
        assert_eq!(effect_handler(true, 20, false, 0, true), H::UnitSound);
        assert_eq!(effect_handler(true, 19 + 4, false, 0, true), H::UnknownKind);
        assert_eq!(effect_handler(true, 0, false, 0, true), H::UnknownKind);
        assert_eq!(effect_handler(true, 3, false, 0, true), H::UnknownKind);
    }
}
