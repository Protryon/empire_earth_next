//! The combat world host's operations answered in Rust (the units' behaviour lane, round 2): the effects the bodies
//! make (through ee_sim's effect interface, `world::effects`), the stores of one record member and the plain reads
//! of the map's tiles and nodes, the unit's position and its last attacker. They were EECombatGoals.cpp
//! `EEWorldOp`'s cases; `EeCombatWorldHost::op` asks here first, the C++ answers the rest.

// `c_ulong` and `c_long` are 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::os::raw::{c_long, c_ulong};

use crate::combat::damage::{
    EE_COMBAT_W_EXPERIENCE, EE_COMBAT_W_NODE_NEXT, EE_COMBAT_W_NODE_UNIT, EE_COMBAT_W_SET_RESOURCE,
    EE_COMBAT_W_TILE_UNITS,
};
use crate::combat::leaves::{EE_COMBAT_W_POSITION, EE_COMBAT_W_SEES_MAP, EE_COMBAT_W_TILE_HEIGHT};
use crate::combat::world::{
    EeCombatWorldArgs, EE_COMBAT_W_LAST_ATTACKER, EE_COMBAT_W_MUZZLE_FLASH, EE_COMBAT_W_SET_AMMUNITION,
    EE_COMBAT_W_SET_FLIGHT_TIME, EE_COMBAT_W_SHOT_EFFECT_BETWEEN, EE_COMBAT_W_SPARKS_AT, EE_COMBAT_W_SPARKS_ON,
    EE_COMBAT_W_SPECIAL_EFFECT, EE_COMBAT_W_SPECIAL_OFF,
};
use crate::world::effects::{effects, EffectPlace, EffectRequest};
use crate::world::layout::layout;
use crate::world::unit_fields::{EeUnitsComplexUnitFields, EeUnitsUnitFields};
use crate::world::view::{ClassView, PlayerView, UnitView, WorldRead, WorldView};
use ee_base::math::types::Ge3DPoint;

/// The class's effects (`GetEffects()` indexes)
const CLASS_EFFECT_MUZZLE_FLASH: usize = 6;
const CLASS_EFFECT_IMPACT_SPARKS: usize = 9;
const CLASS_EFFECT_SPECIAL_ATTACK: usize = 18;
const CLASS_EFFECT_SHOT: usize = 20;
/// `kGraphicEffectImpactSparks`
const GRAPHIC_EFFECT_IMPACT_SPARKS: u32 = 0xf;

/// The operations [`world_op`] answers.
pub fn answers(code: i32) -> bool {
    matches!(
        code,
        EE_COMBAT_W_SPARKS_ON
            | EE_COMBAT_W_SPARKS_AT
            | EE_COMBAT_W_SPECIAL_EFFECT
            | EE_COMBAT_W_MUZZLE_FLASH
            | EE_COMBAT_W_SHOT_EFFECT_BETWEEN
            | EE_COMBAT_W_EXPERIENCE
            | EE_COMBAT_W_SET_RESOURCE
            | EE_COMBAT_W_SPECIAL_OFF
            | EE_COMBAT_W_SET_AMMUNITION
            | EE_COMBAT_W_SET_FLIGHT_TIME
            | EE_COMBAT_W_TILE_UNITS
            | EE_COMBAT_W_NODE_NEXT
            | EE_COMBAT_W_NODE_UNIT
            | EE_COMBAT_W_LAST_ATTACKER
            | EE_COMBAT_W_SEES_MAP
            | EE_COMBAT_W_TILE_HEIGHT
            | EE_COMBAT_W_POSITION
    )
}

/// A float's bits as an argument word (`EEConditionBits`)
fn bits(v: f32) -> i64 {
    i64::from(v.to_bits())
}

/// `EEPointArgs(a, point, result)`
fn point_args(a: &mut EeCombatWorldArgs, p: [f32; 3], result: u64) -> u64 {
    a[0] = bits(p[0]);
    a[1] = bits(p[1]);
    a[2] = bits(p[2]);
    result
}

/// # Safety
/// `object + offset` holds a live pointer member.
unsafe fn pointer(object: u64, offset: u64) -> u64 {
    // SAFETY: the caller's contract
    unsafe { std::ptr::read((object as usize + offset as usize) as *const usize) as u64 }
}

fn complex_mut(u: &UnitView<'_>) -> *mut EeUnitsComplexUnitFields {
    u.complex().ptr() as *mut EeUnitsComplexUnitFields
}

/// `code` on `object` and `other` (their handles as the C++ host takes them; `class` the host's `mClass`); None for
/// an operation the C++ host does (or one whose object is missing: the C++ statement's own outcome).
///
/// # Safety
/// Inside the host's call on the simulation thread: the handles are live objects of the kinds the operation names.
pub unsafe fn world_op(
    t: &WorldRead,
    code: i32,
    object: u64,
    other: u64,
    class: u64,
    a: &mut EeCombatWorldArgs,
) -> Option<u64> {
    if !answers(code) {
        return None;
    }
    let unit = |h: u64| {
        // SAFETY: the caller's contract
        unsafe { UnitView::of(t, h) }
    };
    // SAFETY: as above
    let host_class = || unsafe { ClassView::of(t, class) };
    let unit_effect = |unit: u64, graphic_effect: u64| {
        // `CreateUnitEffect(unit, effect, NULL, 0, 0, 0)`
        effects().create(&EffectRequest {
            graphic_effect,
            place: EffectPlace::Unit { unit, keeps_unit: false, tag_point: 0, goal_index: 0 },
        });
    };
    let between = |from: u64, to: u64, graphic_effect: u64| {
        // `CreateEffectBetweenUnits(from, to, effect, NULL)`
        effects().create(&EffectRequest { graphic_effect, place: EffectPlace::BetweenUnits { from, to } });
    };
    match code {
        EE_COMBAT_W_SPARKS_ON => {
            let effect = unit(object)?.class()?.effect_handle(CLASS_EFFECT_IMPACT_SPARKS);
            unit_effect(object, effect);
        }
        EE_COMBAT_W_SPARKS_AT => {
            let (x, y) = (a[0] as c_long as i64, a[1] as c_long as i64);
            let height = WorldView::global(t)?.map()?.tile(x, y)?.height();
            let at = Ge3DPoint { x: x as f32 + 0.5, y: y as f32 + 0.5, z: height };
            let effect = effects().graphic_effect(GRAPHIC_EFFECT_IMPACT_SPARKS);
            effects().create(&EffectRequest { graphic_effect: effect, place: EffectPlace::Placed { at } });
        }
        EE_COMBAT_W_SPECIAL_EFFECT => between(object, other, host_class()?.effect_handle(CLASS_EFFECT_SPECIAL_ATTACK)),
        EE_COMBAT_W_MUZZLE_FLASH => unit_effect(object, host_class()?.effect_handle(CLASS_EFFECT_MUZZLE_FLASH)),
        EE_COMBAT_W_SHOT_EFFECT_BETWEEN => between(object, other, host_class()?.effect_handle(CLASS_EFFECT_SHOT)),
        EE_COMBAT_W_EXPERIENCE => {
            let c = complex_mut(&unit(object)?);
            // SAFETY: the live unit's record; no other reference to it is held across the store
            unsafe { (*c).mExperience = (*c).mExperience.wrapping_add(1) };
        }
        EE_COMBAT_W_SET_RESOURCE => {
            let f = unit(object)?.fields().ptr() as *mut EeUnitsUnitFields;
            // SAFETY: as above (the EEUnit's record)
            unsafe { (*f).mResourceAmount = a[0] as c_long };
        }
        EE_COMBAT_W_SPECIAL_OFF => {
            let c = complex_mut(&unit(object)?);
            // SAFETY: as above
            unsafe { (*c).mIsSpecialAttack = false };
        }
        EE_COMBAT_W_SET_AMMUNITION => {
            let c = complex_mut(&unit(object)?);
            // SAFETY: as above (`SetAmmunition`'s store)
            unsafe { (*c).mAmmunition = a[0] as c_ulong };
        }
        EE_COMBAT_W_SET_FLIGHT_TIME => {
            let c = complex_mut(&unit(object)?);
            // SAFETY: as above (`SetFlightTime`'s store)
            unsafe { (*c).mFlightTime = a[0] as c_long };
        }
        EE_COMBAT_W_TILE_UNITS => {
            let (x, y) = (a[0] as c_long as i64, a[1] as c_long as i64);
            return Some(match WorldView::global(t)?.map()?.tile(x, y) {
                // SAFETY: a live tile's node list head
                Some(tile) => unsafe { pointer(tile.handle(), layout().tile_units) },
                None => 0,
            });
        }
        // SAFETY: a live node's next
        EE_COMBAT_W_NODE_NEXT => return Some(unsafe { pointer(object, layout().unit_node_next) }),
        EE_COMBAT_W_NODE_UNIT => {
            // SAFETY: a live node's unit (an `EEUnit*`)
            let node_unit = unsafe { pointer(object, layout().unit_node_unit) };
            return Some(unit(node_unit)?.complex_unit_handle());
        }
        EE_COMBAT_W_LAST_ATTACKER => return Some(unit(object)?.last_attacker_handle()),
        EE_COMBAT_W_SEES_MAP => {
            let has_map = WorldView::global(t)?.map().is_some();
            // SAFETY: the host's player handle
            let player = unsafe { PlayerView::of(t, object) }?;
            return Some(u64::from(has_map && player.save_vis_map()));
        }
        EE_COMBAT_W_TILE_HEIGHT => {
            let Some(tile) = unit(object)?.tile() else { return Some(0) };
            a[0] = bits(tile.height());
            return Some(1);
        }
        EE_COMBAT_W_POSITION => {
            let p = crate::units::unit_geom::position(&unit(object)?);
            return Some(point_args(a, p, 0));
        }
        _ => return None,
    }
    Some(0)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_answered_operations() {
        assert!(answers(EE_COMBAT_W_MUZZLE_FLASH) && answers(EE_COMBAT_W_POSITION) && answers(EE_COMBAT_W_NODE_UNIT));
        assert!(!answers(crate::combat::world::EE_COMBAT_W_LAUNCH_SHOT));
    }

    #[test]
    fn a_point_is_three_float_words() {
        let mut a = [0i64; 12];
        assert_eq!(point_args(&mut a, [1.0, -2.5, 0.0], 7), 7);
        assert_eq!(&a[..3], &[0x3f80_0000, i64::from((-2.5f32).to_bits()), 0]);
    }

    #[test]
    fn no_object_goes_to_the_cpp() {
        // SAFETY: no object is read for handle 0
        let t = unsafe { WorldRead::begin() };
        let mut a = [0i64; 12];
        // SAFETY: as above
        assert_eq!(unsafe { world_op(&t, EE_COMBAT_W_LAST_ATTACKER, 0, 0, 0, &mut a) }, None);
        // SAFETY: as above
        assert_eq!(unsafe { world_op(&t, 9999, 0, 0, 0, &mut a) }, None);
    }
}
