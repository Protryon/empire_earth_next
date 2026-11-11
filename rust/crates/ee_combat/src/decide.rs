//! The unit decisions over plain records (R5b.2, second round): whether a priest can convert a target
//! (`EECanConvert`, `EEUCBasicPriest.cpp`), whether a unit can heal one (`EECanHeal`,
//! `EEFOFindNewTargetNearOldTarget.cpp`) and where a shot or a dropped unit lands on a target
//! (`EEFindDropLocation`'s drop point). The C++ gathers every input (pure reads of the world) and acts on the
//! result; the field order is the CBCN/CBCH/CBDP capture's layout.

use ee_ge::types::{Ge3DPoint, GeTransformation};

/// `EECanConvert`'s inputs (`inPriest`, `inTarget`; every flag 0 or 1).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatCanConvertIn {
    /// the priest's class `IsCrusader`
    pub crusader: i32,
    /// the priest's class is a building priest (`kUnitCategoryBuildingPriest`)
    pub building_priest: i32,
    /// the priest's player has the adaptation power
    pub adaptation: i32,
    pub same_player: i32,
    /// the priest's player is the target's enemy
    pub enemy: i32,
    /// the priest's `mConversionRate`
    pub conversion_rate: f32,
    pub target_disabled: i32,
    /// the target `IsProtected`
    pub target_protected: i32,
    /// the target's `mConversionResistance`
    pub resistance: f32,
    pub target_town_center: i32,
    pub target_settlement: i32,
    pub target_hit_points: i32,
    /// the target's class `CanBeAttacked`
    pub attackable: i32,
    /// `gDamageTable[the target class's family][the priest class's ammo]` (0 for a negative ammo)
    pub damage: f32,
    /// both classes have the same category
    pub same_category: i32,
    pub converts_priests: i32,
    /// the priest's faith (`GetPower`)
    pub power: i32,
    /// `EEGetConversionCost(priest)`
    pub cost: i32,
    /// the target's class is a land priest
    pub target_land_priest: i32,
    pub converts_buildings: i32,
    /// the target's class `IsLandBuilding`
    pub target_land_building: i32,
    /// the priest's player is in the Copper Age or later
    pub at_copper: i32,
    /// both locations (a building priest's range test)
    pub priest_x: f32,
    pub priest_y: f32,
    pub priest_z: f32,
    pub target_x: f32,
    pub target_y: f32,
    pub target_z: f32,
    /// the priest's `mRange`
    pub range: f32,
    pub priest_cyber: i32,
    pub target_cyber: i32,
    /// the target `HasEffect(4)`
    pub shielded: i32,
}

/// `EECanConvert`'s result and its out flag (written only when `flag_written`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatCanConvertOut {
    pub result: i32,
    pub flag_written: i32,
    /// the target is shielded (0 when not written)
    pub flag: i32,
}

/// `EECanHeal`'s inputs (`inUnit` heals `inTarget`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatCanHealIn {
    /// the healer's `mWonderUnitSet` is not 0
    pub has_set: i32,
    /// the target `IsRepairable`
    pub repairable: i32,
    pub hit_points: i32,
    pub maximum: i32,
    /// the healer's class `GetHealThreshold`
    pub threshold: f32,
    pub same_unit: i32,
    pub same_player: i32,
    /// the target's player is not the healer's enemy
    pub allied: i32,
    /// the healer's unit set holds the target's type (0 for a set out of the manager's range)
    pub in_set: i32,
}

/// `EEFindDropLocation`'s point kinds (`EeCombatDropIn::kind`).
/// The goal's location (the C++ asks the goal; not computed here).
pub const DROP_KIND_GOAL: i32 = 0;
/// The target model's drop point.
pub const DROP_KIND_MODEL: i32 = 1;
/// The middle of the target's tile.
pub const DROP_KIND_TILE: i32 = 2;
/// Neither: no target.
pub const DROP_KIND_NONE: i32 = 3;

/// `EEFindDropLocation`'s inputs: the kind, then the model's (`DROP_KIND_MODEL`) or the tile's (`DROP_KIND_TILE`) values.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatDropIn {
    pub kind: i32,
    /// the target model's `mDefaultPoint`
    pub offset: [f32; 3],
    /// the target's physical model's scale
    pub scale: f32,
    pub built: i32,
    pub hit_points: i32,
    pub max_hit_points: i32,
    /// the target's physical model's location (`GETransformation::mMatrix`)
    pub transformation: [[f32; 4]; 4],
    pub tile_x: i32,
    pub tile_y: i32,
    /// the target tile's height
    pub tile_height: f32,
}

/// The drop point.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatDropOut {
    pub x: f32,
    pub y: f32,
    pub z: f32,
}

const _: () = assert!(std::mem::size_of::<EeCombatCanConvertIn>() == 128);
const _: () = assert!(std::mem::size_of::<EeCombatCanHealIn>() == 36);
const _: () = assert!(std::mem::size_of::<EeCombatDropIn>() == 108);

/// `EECanConvert`: an enemy, convertible target (resistance, faith, the priests' and buildings' rules, a building
/// priest's range), not shielded.
pub fn can_convert(i: &EeCombatCanConvertIn) -> EeCombatCanConvertOut {
    let mut out = EeCombatCanConvertOut::default();
    let power = i.crusader == 0 && i.building_priest == 0 && i.adaptation != 0;
    // `theStats->mConversionRate <= 0.0f` (NaN goes on)
    if i.same_player != 0
        || i.enemy == 0
        || i.conversion_rate <= 0.0
        || (i.target_disabled != 0 && i.target_protected == 0)
    {
        return out;
    }
    let mut result = (i.resistance < 1.0 || (i.target_town_center != 0 && power))
        && i.target_hit_points > 0
        && i.attackable != 0
        && i.damage != 0.0
        && (i.same_category == 0 || i.converts_priests != 0)
        && i.power >= i.cost
        && (i.building_priest == 0 || i.target_land_priest == 0 || i.converts_priests != 0);
    if i.target_land_building != 0 && i.converts_buildings == 0 {
        result = result && power && (i.target_town_center != 0 || i.target_settlement != 0) && i.at_copper != 0;
    }
    if i.building_priest != 0 {
        let priest = Ge3DPoint::new(i.priest_x, i.priest_y, i.priest_z);
        let target = Ge3DPoint::new(i.target_x, i.target_y, i.target_z);
        if priest.distance_to_squared(target) >= i.range * i.range {
            result = false;
        }
    }
    if result && i.priest_cyber == 0 && i.target_cyber == 0 {
        out.flag_written = 1;
        out.flag = i.shielded;
        result = i.shielded == 0;
    }
    out.result = result as i32;
    out
}

/// `EECanHeal`: a wounded (at most the threshold's share of its hit points), repairable, friendly other unit in the
/// healer's unit set.
pub fn can_heal(i: &EeCombatCanHealIn) -> i32 {
    // `(float)theHitPoints / theMaximum`: both converted to float
    (i.has_set != 0
        && i.repairable != 0
        && i.hit_points as f32 / i.maximum as f32 <= i.threshold
        && i.same_unit == 0
        && (i.same_player != 0 || i.allied != 0)
        && i.in_set != 0) as i32
}

/// `EEFindDropLocation`'s point for `DROP_KIND_MODEL` (the model's drop point scaled by the unit's size, lowered by an
/// unbuilt unit's health, through its location) and `DROP_KIND_TILE` (the tile's middle at its height); 0 otherwise.
pub fn drop_point(i: &EeCombatDropIn) -> EeCombatDropOut {
    match i.kind {
        DROP_KIND_MODEL => {
            let mut offset = Ge3DPoint::new(i.offset[0] * i.scale, i.offset[1] * i.scale, i.offset[2] * i.scale);
            if i.built == 0 {
                offset.z *= i.hit_points as f32 / i.max_hit_points as f32;
            }
            let p = GeTransformation { m: i.transformation }.apply_point(offset);
            EeCombatDropOut { x: p.x, y: p.y, z: p.z }
        }
        DROP_KIND_TILE => EeCombatDropOut { x: i.tile_x as f32 + 0.5, y: i.tile_y as f32 + 0.5, z: i.tile_height },
        _ => EeCombatDropOut::default(),
    }
}
