//! The combat world host's operations answered in Rust (the units' behaviour lane, round 2): the effects the bodies
//! make (through ee_sim's effect interface, `world::effects`), the stores of one record member and the plain reads
//! of the map's tiles and nodes, the unit's position and its last attacker. They were EECombatGoals.cpp
//! `EEWorldOp`'s cases; `EeCombatWorldHost::op` asks here first, the C++ answers the rest.

// `c_ulong` and `c_long` are 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::os::raw::{c_long, c_ulong};

use crate::combat::damage::{
    EE_COMBAT_W_AREA_CAPTURE, EE_COMBAT_W_DAMAGE_CAPTURE, EE_COMBAT_W_EXPERIENCE, EE_COMBAT_W_HIT_CAPTURE,
    EE_COMBAT_W_NODE_NEXT, EE_COMBAT_W_NODE_UNIT, EE_COMBAT_W_SET_RESOURCE, EE_COMBAT_W_TILE_UNITS,
};
use crate::combat::leaves::{
    EE_COMBAT_W_CAPTURE, EE_COMBAT_W_POSITION, EE_COMBAT_W_SEES_MAP, EE_COMBAT_W_TILE_AT, EE_COMBAT_W_TILE_HEIGHT,
};
use crate::combat::world::{
    EeCombatWorldArgs, EE_COMBAT_W_CORNER_BLOCKED, EE_COMBAT_W_LAST_ATTACKER, EE_COMBAT_W_MUZZLE_FLASH,
    EE_COMBAT_W_SET_AMMUNITION, EE_COMBAT_W_SET_FLIGHT_TIME, EE_COMBAT_W_SHOT_EFFECT_BETWEEN, EE_COMBAT_W_SPARKS_AT,
    EE_COMBAT_W_SPARKS_ON, EE_COMBAT_W_SPECIAL_EFFECT, EE_COMBAT_W_SPECIAL_OFF,
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
            | EE_COMBAT_W_TILE_AT
            | EE_COMBAT_W_CORNER_BLOCKED
            | EE_COMBAT_W_CAPTURE
            | EE_COMBAT_W_DAMAGE_CAPTURE
            | EE_COMBAT_W_AREA_CAPTURE
            | EE_COMBAT_W_HIT_CAPTURE
    ) || cfg!(not(test))
        && matches!(
            code,
            crate::combat::world::EE_COMBAT_W_CAN_GET_TO_UNIT
                | crate::combat::world::EE_COMBAT_W_CONTINENT_AT
                | crate::combat::world::EE_COMBAT_W_CONTINENT_LINK
                | crate::combat::world::EE_COMBAT_W_CONNECTED_FOR_UNIT
                | crate::combat::world::EE_COMBAT_W_FIND_PATH
        )
}

/// The reach tests' continent manager queries (the paths lane's round 1: ee_sim::astar cm_unit.rs and the manager's
/// record): `CanGetToUnit`, `GetContinentAt(unit)`, `AreAdjacent` of two continents' ids, `AreConnectedForUnit`.
#[cfg(not(test))]
fn reach_op(t: &WorldRead, code: i32, object: u64, other: u64, a: &mut EeCombatWorldArgs) -> Option<u64> {
    use crate::astar::cm::EeAStarCmTable;
    use crate::combat::world::{
        EE_COMBAT_W_CAN_GET_TO_UNIT, EE_COMBAT_W_CONNECTED_FOR_UNIT, EE_COMBAT_W_CONTINENT_AT,
        EE_COMBAT_W_CONTINENT_LINK, EE_COMBAT_W_FIND_PATH,
    };
    let r = crate::astar::world_view::cm();
    Some(match code {
        EE_COMBAT_W_CAN_GET_TO_UNIT => {
            // SAFETY: the operation's live units
            let (x, y) = unsafe { UnitView::of(t, other) }?.tile_xy();
            // SAFETY: as above
            u64::from(unsafe { crate::astar::cm_unit::can_get_to_unit(object, x as c_long, y as c_long) })
        }
        EE_COMBAT_W_CONTINENT_AT => {
            // SAFETY: as above
            let (x, y) = unsafe { UnitView::of(t, object) }?.tile_xy();
            let r = r?;
            if (x as c_ulong) >= r.width || (y as c_ulong) >= r.height {
                return Some(0);
            }
            match crate::astar::cm_io::continent_id_at(r, x as c_long, y as c_long) {
                0 => 0,
                id => r.tables().tables[EeAStarCmTable::Continents as usize].get(id as usize).copied().unwrap_or(0),
            }
        }
        EE_COMBAT_W_CONTINENT_LINK => {
            let r = r?;
            // SAFETY: the operation's live continents; the flags grid is mFlagsWidth squared
            unsafe {
                let (from, to) = (crate::astar::cm_io::continent(object).id, crate::astar::cm_io::continent(other).id);
                u64::from(*r.tile_flags.add(from as usize * r.flags_width as usize + to as usize))
            }
        }
        EE_COMBAT_W_FIND_PATH => {
            // FindPath(object, the map, a0 a1, a2 a3, the window a4..a7, kind 0xb, revs a8, an attackable modifier):
            // the first tile into a0 a1, the path's length
            let map = WorldView::global(t).and_then(|w| w.map()).map_or(0, |m| m.handle());
            // SAFETY: the operation's live units
            let modifier = unsafe { crate::astar::modifier_obj::ee_astar_modifier_new_attackable(object, other) };
            let window = crate::astar::pm_entry::EePathWindow {
                top: a[4] as c_long as i32,
                left: a[5] as c_long as i32,
                bottom: a[6] as c_long as i32,
                right: a[7] as c_long as i32,
            };
            let (from, to) =
                ((a[0] as c_long as i32, a[1] as c_long as i32), (a[2] as c_long as i32, a[3] as c_long as i32));
            // SAFETY: as above; the live map and the modifier made here
            let path = unsafe {
                crate::astar::pm_entry::find_path(
                    object,
                    map,
                    from,
                    to,
                    window,
                    0xb,
                    a[8] as c_long as i32,
                    modifier as u64,
                )
            };
            if let Some(p) = path.first() {
                a[0] = i64::from(p.x);
                a[1] = i64::from(p.y);
            }
            // SAFETY: made above
            unsafe { crate::astar::modifier_obj::ee_astar_modifier_release(modifier) };
            path.len() as u64
        }
        // SAFETY: as above (the unit in a[0])
        EE_COMBAT_W_CONNECTED_FOR_UNIT => {
            u64::from(unsafe { crate::astar::cm_unit::connected_for_unit(a[0] as u64, object, other) })
        }
        _ => return None,
    })
}

#[cfg(test)]
fn reach_op(_t: &WorldRead, _code: i32, _object: u64, _other: u64, _a: &mut EeCombatWorldArgs) -> Option<u64> {
    None
}

/// `EE_COMBAT_XA_APPLY_HIT`, `EE_COMBAT_XA_AREA_DAMAGE` (ee/EECombatGoals.h)
const XA_APPLY_HIT: u32 = 3;
const XA_AREA_DAMAGE: u32 = 4;

/// TEST ONLY: the area damage's records: CBAD (the attacker, the centre, the radius, the victims) and CBXA (the
/// victims' FNV-1a 32 of id, hit points and resource amount after the hits).
///
/// # Safety
/// `a[0]` holds `a[1]` live unit handles; `object` (and `other`, when not 0) live units.
unsafe fn area_capture(t: &WorldRead, object: u64, other: u64, a: &EeCombatWorldArgs) {
    if !crate::app::capture_record::active() {
        return;
    }
    // SAFETY: the caller's contract
    let victims: Vec<UnitView<'_>> = unsafe { std::slice::from_raw_parts(a[0] as usize as *const u64, a[1] as usize) }
        .iter()
        // SAFETY: as above
        .map(|&h| unsafe { UnitView::of(t, h) }.expect("a live victim"))
        .collect();
    // SAFETY: as above
    let (unit, target) = unsafe { (UnitView::of(t, object).expect("a live attacker"), UnitView::of(t, other)) };
    let mut r = crate::app::capture_record::Record::new(u32::from_le_bytes(*b"CBAD"));
    r.i32(unit.id() as i32);
    r.i32(a[2] as i32);
    r.i32(a[3] as i32);
    r.f32(f32::from_bits(a[4] as u32));
    r.i32(a[5] as i32);
    r.i32(target.filter(|t| t.complex_unit_handle() != 0).map_or(-1, |t| t.id() as i32));
    r.i32(a[1] as i32);
    for v in &victims {
        r.i32(v.id() as i32);
    }
    r.commit();
    let mut hash: u32 = 0x811c_9dc5;
    for v in &victims {
        for w in [v.id(), v.hit_points() as u32, v.resource_amount() as u32] {
            for b in w.to_le_bytes() {
                hash = (hash ^ u32::from(b)).wrapping_mul(0x0100_0193);
            }
        }
    }
    x_action(XA_AREA_DAMAGE, object, other, hash);
}

/// `EECombatCaptureXAction` (capture_units.rs; the game's only).
fn x_action(func: u32, unit: u64, target: u64, before: u32) {
    #[cfg(not(test))]
    crate::combat::capture_units::ee_combat_capture_x_action(func, unit, target, before);
    #[cfg(test)]
    let _ = (func, unit, target, before);
}

/// TEST ONLY: a record of `tag` from two byte ranges (`EECombatCapture`'s and the formulas' records).
///
/// # Safety
/// The ranges are readable.
unsafe fn capture(tag: u32, first: (*const u8, usize), second: (*const u8, usize)) {
    if !crate::app::capture_record::active() {
        return;
    }
    let mut r = crate::app::capture_record::Record::new(tag);
    // SAFETY: the caller's contract
    unsafe {
        r.bytes(std::slice::from_raw_parts(first.0, first.1));
        r.bytes(std::slice::from_raw_parts(second.0, second.1));
    }
    r.commit();
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
    if let Some(v) = reach_op(t, code, object, other, a) {
        return Some(v);
    }
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
        EE_COMBAT_W_TILE_AT => {
            // gWorld.GetMap()->GetTileAtLocation((long)a[0], (long)a[1]) != NULL
            let (x, y) = (a[0] as c_long as i64, a[1] as c_long as i64);
            return Some(u64::from(WorldView::global(t)?.map()?.tile(x, y).is_some()));
        }
        EE_COMBAT_W_CORNER_BLOCKED => {
            #[cfg(not(test))]
            return Some(u64::from(
                crate::units::uu_ffi::ee_units_uu_is_corner_blocked(a[0] as i32, a[1] as i32, a[2] as i32, a[3] as i32)
                    != 0,
            ));
            #[cfg(test)]
            return None;
        }
        // TEST ONLY: a formula's record (a[0] the tag, then two byte ranges)
        // SAFETY: the host's contract: the ranges the formula handed in
        EE_COMBAT_W_CAPTURE => unsafe {
            capture(
                a[0] as u32,
                (a[1] as usize as *const u8, a[2] as usize),
                (a[3] as usize as *const u8, a[4] as usize),
            )
        },
        // TEST ONLY: CBAD and CBXA (the victims in a[0], a[1] of them)
        // SAFETY: the damage body's live units
        EE_COMBAT_W_AREA_CAPTURE => unsafe { area_capture(t, object, other, a) },
        // TEST ONLY: CBXA of ApplyHit
        EE_COMBAT_W_HIT_CAPTURE => x_action(XA_APPLY_HIT, object, other, 0),
        // TEST ONLY: CBDM, the damage's input and output records
        // SAFETY: as above: the damage body's records
        EE_COMBAT_W_DAMAGE_CAPTURE => unsafe {
            capture(
                u32::from_le_bytes(*b"CBDM"),
                (a[0] as usize as *const u8, std::mem::size_of::<crate::combat::EeCombatDamageIn>()),
                (a[1] as usize as *const u8, std::mem::size_of::<crate::combat::EeCombatDamageOut>()),
            )
        },
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
