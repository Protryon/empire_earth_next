//! The calamity classes' world steps (was EECalamityUpdate.cpp's host: the calls every class shares and each class's
//! own steps, what the twenty EECC* units held, 0xaebb7..0xb5b7d): the answer to every call the Rust starts, stops
//! and updates (ee_sim tech calamity_start.rs, calamity_update.rs) make of the world. The C++ keeps the run's host
//! object (`EECalamityUpdateHost`: the class, the calamity, the two sounds, the units of the last search) and answers
//! each C++ call of these bodies in one case ([`super::EeGameCpp`]).
//!
//! The calamity's numeric members are written from the update's state before every call and read back after it, as
//! the C++ host did. Every capture record (CALW, CALT, CONT) is written by the call the C++ made, in its order.

// the C++'s own casts (`long` and `unsigned long` are 32-bit here, 64-bit on LP64), literals, products and
// loops, kept as written
#![allow(
    clippy::unnecessary_cast,
    clippy::excessive_precision,
    clippy::approx_constant,
    clippy::neg_multiply,
    clippy::assign_op_pattern,
    clippy::needless_range_loop,
    clippy::too_many_arguments
)]
// the C++'s own tests and literals, kept as written
#![allow(clippy::neg_cmp_op_on_partial_ord)]

use std::ffi::{c_long, c_ulong, c_void};

use ee_base::math::transform::RotationOrder;
use ee_base::math::types::{Ge3DPoint, GeOrientation, GeRectangle, GeTransformation, GeVector};
use ee_base::math::um::{floor, lrintf, um_arctan2, um_cos};
use ee_sim::tech::calamity_class::EeCalamityClassFields;
use ee_sim::tech::calamity_record::EeCalamityFields;
use ee_sim::tech::calamity_update::{EeCalamityCall, EeCalamityOp, EeCalamityReply, EeCalamityState};
use ee_sim::units::uu_host::{tiles_of, units_of};
use ee_sim::world::player_fields::{EeUnitsPlayerFields, EeUnitsPlayerSaved, EeUnitsRtsPlayerFields};
use ee_sim::world::unit_class_fields::EeUnitsUnitClassFields;
use ee_sim::world::unit_fields::{EeUnitsComplexUnitFields, EeUnitsRtsUnitFields, EeUnitsUnitFields};

use super::{cptr, h, hd, hf, hp, pa, position, ptr, rec, set_position, O, P};

/// The calamity classes (`EECalamityEnum`, ee/EECalamityManager.h).
mod kind {
    pub(crate) const CAL_K_EARTHQUAKE: i32 = 3;
    pub(crate) const CAL_K_HURRICANE: i32 = 4;
    pub(crate) const CAL_K_PLAGUE: i32 = 7;
    pub(crate) const CAL_K_LEPROSY: i32 = 8;
    pub(crate) const CAL_K_VOLCANO: i32 = 9;
    pub(crate) const CAL_K_FIRE: i32 = 10;
    pub(crate) const CAL_K_MORALE: i32 = 13;
    pub(crate) const CAL_K_TIME_WARP: i32 = 14;
    pub(crate) const CAL_K_TELEPORT: i32 = 15;
    pub(crate) const CAL_K_AREA_EFFECT: i32 = 16;
    pub(crate) const CAL_K_AE_EXTRA_DAMAGE: i32 = 17;
    pub(crate) const CAL_K_AE_MIND_CONTROL: i32 = 19;
    pub(crate) const CAL_K_CONSCRIPTION: i32 = 20;
    pub(crate) const CAL_K_LOGIC_BOMB: i32 = 21;
    pub(crate) const CAL_K_DROP_OBJECT: i32 = 22;
    pub(crate) const CAL_K_CLOAKING: i32 = 23;
}

/// The C++ constants the steps pass (EEGameOps.cpp `static_assert`s each).
mod k {
    pub(crate) const CAL_C_ACTION_ATTACK_ENEMY_UNIT: i64 = 2001;
    pub(crate) const CAL_C_ACTION_CALAMITY_PRE_CAST: i64 = 2030;
    pub(crate) const CAL_C_ACTION_SPECIAL_LOCATION: i64 = 2032;
    pub(crate) const CAL_C_ACTION_IDLE: i64 = 5000;
    pub(crate) const CAL_C_ACTION_OPEN: i64 = 15000;
    pub(crate) const CAL_C_MOVEMENT_AIR: u8 = 1;
    pub(crate) const CAL_C_MOVEMENT_SEA: u8 = 4;
    pub(crate) const CAL_C_UNIT_TYPE_CAPITOL: u64 = 0x29;
    pub(crate) const CAL_C_UNIT_CATEGORY_GATE: u64 = 0x19;
    pub(crate) const CAL_C_GOAL_CAST_CALAMITY: i64 = 11;
    pub(crate) const CAL_C_TECH_CONSCRIPTION_SPELL: i64 = 1920;
    pub(crate) const CAL_C_EFFECT_2D_LIGHTNING: i64 = 0x8;
    pub(crate) const CAL_C_EFFECT_3D_DISEASE: i64 = 0x23;
    pub(crate) const CAL_C_EFFECT_3D_TRACER_BEAM: i64 = 0x40;
    /// `kCalamityStartSound` (EECalamityStartOp).
    pub(crate) const CAL_C_START_SOUND: i64 = 2;
}

/// The CALW events (were ee/EETechCapture.h's `EECalamityCaptureOp`: the helper update 1 .. the destroyed effect 26).
mod cap {
    pub(crate) const CAL_W_HELPER_UPDATE: i64 = 1;
    pub(crate) const CAL_W_UNITS_NEAR: i64 = 2;
    pub(crate) const CAL_W_TAKE_HIT: i64 = 3;
    pub(crate) const CAL_W_IMMUNE: i64 = 4;
    pub(crate) const CAL_W_STRIKE: i64 = 5;
    pub(crate) const CAL_W_CRACKS: i64 = 6;
    pub(crate) const CAL_W_RANDOM: i64 = 7;
    pub(crate) const CAL_W_SHAKE: i64 = 8;
    pub(crate) const CAL_W_CREATE_STORM: i64 = 9;
    pub(crate) const CAL_W_LAUNCH_METEOR: i64 = 10;
    pub(crate) const CAL_W_CREATE_VOLCANO: i64 = 11;
    pub(crate) const CAL_W_WARP_TINT: i64 = 12;
    pub(crate) const CAL_W_WARP_DEPART: i64 = 13;
    pub(crate) const CAL_W_WARP_RETURN: i64 = 14;
    pub(crate) const CAL_W_WARP_BLINK: i64 = 15;
    pub(crate) const CAL_W_HOLD: i64 = 16;
    pub(crate) const CAL_W_TELEPORT: i64 = 17;
    pub(crate) const CAL_W_LOGIC_BOMB_MARK: i64 = 18;
    pub(crate) const CAL_W_LOGIC_BOMB_DISABLE: i64 = 19;
    pub(crate) const CAL_W_CREATE_HELPER: i64 = 20;
    pub(crate) const CAL_W_APPLY_EFFECT: i64 = 21;
    pub(crate) const CAL_W_PLACED_EFFECT: i64 = 22;
    pub(crate) const CAL_W_MORALE: i64 = 23;
    pub(crate) const CAL_W_SPECIAL_ATTACK: i64 = 24;
    pub(crate) const CAL_W_SPECIAL_LOCATION: i64 = 25;
    pub(crate) const CAL_W_DESTROY_EFFECT: i64 = 26;
}

// ---- the records and members the steps read

fn rts<'a>(u: P) -> &'a mut EeUnitsRtsUnitFields {
    // SAFETY: a live unit's record, used before the next C++ call
    unsafe { &mut *rec(O::UnRts, u) }
}

fn uf<'a>(u: P) -> &'a mut EeUnitsUnitFields {
    // SAFETY: as rts
    unsafe { &mut *rec(O::UnFields, u) }
}

fn cf<'a>(cx: P) -> &'a mut EeUnitsComplexUnitFields {
    // SAFETY: a live complex unit's record, as rts
    unsafe { &mut *rec(O::CxFields, cx) }
}

fn ucf<'a>(uc: P) -> &'a EeUnitsUnitClassFields {
    // SAFETY: a live unit class's record, as rts
    unsafe { &*rec(O::UcFields, uc) }
}

fn player_id(pl: P) -> c_ulong {
    // SAFETY: a live player's record, as rts
    unsafe { (*rec::<EeUnitsRtsPlayerFields>(O::PlRts, pl)).mPlayerID }
}

fn enemy(pl: P, other: c_ulong) -> c_ulong {
    // SAFETY: as player_id
    unsafe { (*rec::<EeUnitsPlayerSaved>(O::PlSaved, pl)).mEnemies[other as usize] }
}

fn cal<'a>(c: P) -> &'a mut EeCalamityFields {
    // SAFETY: the run's live calamity, as rts
    unsafe { &mut *rec(O::CalFields, c) }
}

fn klass<'a>(kl: P) -> &'a EeCalamityClassFields {
    // SAFETY: the run's live class, as rts
    unsafe { &*rec(O::KlFields, kl) }
}

fn point_at<'a>(op: O, p: P) -> &'a mut Ge3DPoint {
    // SAFETY: a GE3DPoint member of a live object, as rts
    unsafe { &mut *rec(op, p) }
}

fn effect_handle<'a>(c: P) -> &'a mut u32 {
    // SAFETY: the calamity's EEEffectHandle (its mValue), as rts
    unsafe { &mut *rec(O::CalEffectHandle, c) }
}

fn complex(u: P) -> P {
    hp(O::UnComplex, u, &[])
}

fn unit_player(u: P) -> P {
    hp(O::UnPlayer, u, &[])
}

fn unit_class(u: P) -> P {
    hp(O::UnClass, u, &[])
}

fn target(c: P) -> P {
    hp(O::CalTarget, c, &[])
}

fn caster(c: P) -> P {
    hp(O::CalCaster, c, &[])
}

fn target_location(c: P) -> Ge3DPoint {
    *point_at(O::CalTargetLoc, c)
}

fn unit_position(u: P) -> Ge3DPoint {
    let mut p = Ge3DPoint::default();
    h(O::UnPosition, u, &[ptr(&mut p)]);
    p
}

fn location(cx: P) -> GeTransformation {
    let mut t = GeTransformation::IDENTITY;
    h(O::CxLocation, cx, &[ptr(&mut t)]);
    t
}

fn set_transformation(cx: P, t: &GeTransformation) {
    h(O::CxSetTransformation, cx, &[cptr(t), 1]);
}

fn effect(kl: P, n: u32) -> P {
    let op = match n {
        1 => O::KlEffect1,
        2 => O::KlEffect2,
        3 => O::KlEffect3,
        _ => O::KlEffect4,
    };
    hp(op, kl, &[])
}

fn random() -> f32 {
    hd(O::PhysGetRandom, 0, &[], &[])
}

fn generate() -> f32 {
    hd(O::PhysGenerate, 0, &[], &[])
}

fn mesh_valid(x: i64, y: i64) -> bool {
    h(O::MeshValidTile, 0, &[x, y]) != 0
}

fn mesh_z(x: f32, y: f32) -> f32 {
    hd(O::MeshZ, 0, &[], &[x, y])
}

fn capture(op: i64, a: i64, b: i64, c: f32, d: f32) {
    hf(O::CapEvent, 0, &[op, a, b], &[c, d]);
}

fn start_point(p: &Ge3DPoint) {
    hf(O::CapStartPoint, 0, &[1], &[p.x, p.y, p.z]);
}

// the effect manager's calls (a handle out when asked for)
fn eff_unit(u: P, effect: P, handle: Option<&mut u32>) -> i64 {
    h(O::EffUnit, u, &[pa(effect), handle.map_or(0, ptr), 0, 0, 0])
}

fn eff_between_units(from: P, to: P, effect: P, handle: Option<&mut u32>) -> i64 {
    h(O::EffBetweenUnits, from, &[pa(to), pa(effect), handle.map_or(0, ptr)])
}

fn eff_at(u: P, at: &Ge3DPoint, effect: P, handle: Option<&mut u32>) -> i64 {
    h(O::EffAt, u, &[cptr(at), pa(effect), handle.map_or(0, ptr), 0])
}

fn eff_placed(at: &Ge3DPoint, effect: P, handle: Option<&mut u32>) -> i64 {
    h(O::EffPlaced, 0, &[cptr(at), pa(effect), handle.map_or(0, ptr)])
}

fn eff_between(from: &Ge3DPoint, to: &Ge3DPoint, effect: P) -> i64 {
    h(O::EffBetween, 0, &[cptr(from), cptr(to), pa(effect), 0])
}

/// `gSoundManager->PlayAndReturnReferencedNode(sound's sample, volume, pan, pitch, sound's looping, NULL)`.
fn play_node(sound: P, volume: f32, pan: f32, pitch: f32) -> P {
    hf(O::SoPlayNode, sound, &[], &[volume, pan, pitch]) as P
}

fn volume(sound: P) -> f32 {
    hd(O::SoVolume, sound, &[], &[])
}

/// `SetSound(node)` and the node's own reference dropped (the calamity keeps one).
fn keep_sound(c: P, node: P, secondary: bool) {
    if secondary {
        super::calamity_objects::ee_gamecpp_calamity_set_sound(
            c as *mut std::ffi::c_void,
            node as *mut std::ffi::c_void,
            true,
        );
        let n = hp(O::CalSecondary, c, &[]);
        if n != 0 {
            h(O::NodeDereference, n, &[]);
        }
    } else {
        super::calamity_objects::ee_gamecpp_calamity_set_sound(
            c as *mut std::ffi::c_void,
            node as *mut std::ffi::c_void,
            false,
        );
        let n = hp(O::CalSound, c, &[]);
        if n != 0 {
            h(O::NodeDereference, n, &[]);
        }
    }
}

fn start_action(cx: P, action: i64) {
    h(O::CxStartAction, cx, &[action]);
    h(O::UcActionChanged, unit_class(cx), &[pa(cx)]);
}

fn set_disabled(cx: P, disabled: bool, reason: i64) {
    h(O::CxSetDisabled, cx, &[disabled as i64, reason]);
}

fn set_target(c: P, u: P) {
    super::calamity_objects::ee_gamecpp_calamity_set_unit(c as *mut std::ffi::c_void, u as *mut std::ffi::c_void, true);
}

fn off_map(cx: P, reason: i64) {
    h(O::CxTakeOffMap, cx, &[]);
    let info = hp(O::PlOffMapInfo, unit_player(cx), &[pa(cx)]);
    if info != 0 {
        h(O::OffMapSetReason, info, &[reason]);
    }
}

/// `ee_units_uu_find_placement` for the unit near the tile (the warp's and the teleport's): the transformation found.
fn find_placement(kl: P, cx: P, x: c_ulong, y: c_ulong) -> Option<GeTransformation> {
    let mut t = GeTransformation::IDENTITY;
    let found = Ge3DPoint::default();
    let size = ucf(unit_class(cx)).mTileSize as i32;
    // SAFETY: the unit and its class are live; the point and matrix are ours for the call
    let ok = unsafe {
        ee_sim::units::uu_ffi::ee_units_uu_find_placement(
            &[found.x, found.y, found.z],
            cx as u64,
            0,
            x as u32,
            y as u32,
            size,
            klass(kl).mPlacementRange as i32,
            1,
            1,
            0,
            0,
            &mut t.m,
            0,
        )
    };
    (ok != 0).then_some(t)
}

fn host_tiles(host: P) -> *mut c_void {
    let v = hp(O::HostTiles, host, &[]);
    h(O::VecTilesClear, v, &[]);
    v as *mut c_void
}

fn host_scratch(host: P) -> *mut c_void {
    let v = hp(O::HostScratch, host, &[]);
    h(O::VecUnitsClear, v, &[]);
    v as *mut c_void
}

// ---- the state around each call

fn state_to(s: &EeCalamityState, c: P) {
    cal(c).mTimeAlive = s.time_alive;
    cal(c).mFloatPlaceHolder1 = s.float1;
    cal(c).mFloatPlaceHolder2 = s.float2;
    cal(c).mIntPlaceHolder1 = s.int1 as c_long;
    cal(c).mIntPlaceHolder2 = s.int2 as c_long;
    *point_at(O::CalTargetLoc, c) = Ge3DPoint::new(s.target[0], s.target[1], s.target[2]);
    cal(c).mEpicenterFactor = s.epicenter_factor;
    *effect_handle(c) = s.effect_handle;
    cal(c).mBoolPlaceHolder1 = s.bool1;
    cal(c).mBoolPlaceHolder2 = s.bool2;
}

fn state_from(c: P, s: &mut EeCalamityState) {
    let f = *cal(c);
    let t = target_location(c);
    s.time_alive = f.mTimeAlive;
    s.float1 = f.mFloatPlaceHolder1;
    s.float2 = f.mFloatPlaceHolder2;
    s.int1 = f.mIntPlaceHolder1 as i32;
    s.int2 = f.mIntPlaceHolder2 as i32;
    s.target = [t.x, t.y, t.z];
    s.epicenter_factor = f.mEpicenterFactor;
    s.effect_handle = *effect_handle(c);
    s.bool1 = f.mBoolPlaceHolder1;
    s.bool2 = f.mBoolPlaceHolder2;
}

// ---- the classes' world steps (what each EECC* unit held; the original's addresses in each comment)

/// The class's first effect from the caster to (x, y, z) of the call (CALT records the point); its handle (none
/// asked for without `with_handle`).
fn caster_effect(kl: P, c: P, call: &EeCalamityCall, with_handle: bool) -> u32 {
    let at = Ge3DPoint::new(call.x, call.y, call.z);
    let mut handle = 0u32;
    start_point(&at);
    eff_at(caster(c), &at, effect(kl, 1), if with_handle { Some(&mut handle) } else { None });
    handle
}

/// A unit's orders cleared and the unit enabled (the time warp's start, the mind control's cure).
fn clear_orders(cx: P) {
    h(O::CxClearBuildTargets, cx, &[]);
    let mut i: c_ulong = 0;
    while i < h(O::CxNumGoalQueues, cx, &[]) as c_ulong {
        h(O::CxClearGoalQueue, cx, &[i as i64]);
        i = i.wrapping_add(1);
    }
    h(O::CxClearGoals, cx, &[0]);
    set_disabled(cx, false, 0);
}

/// The quake's area around the target: `GERectangle(top left, bottom right)`.
fn set_area(c: P, kl: P) {
    let at = target_location(c);
    let r = klass(kl).mRadius;
    let rect = GeRectangle { bottom: at.y + r, left: at.x - r, right: at.x + r, top: at.y - r };
    // SAFETY: the calamity's GERectangle member
    unsafe { *rec::<GeRectangle>(O::CalRect, c) = rect };
}

/// EECCEarthquake::CreateCracks 0xb0502 (with SetAffectedArea 0xb06b5 and CreateCrack 0xb071c): the quake's area,
/// then cracks radiating from the target at roughly even, randomly jittered angles until one fails.
fn quake_cracks(kl: P, c: P) {
    let mut t = GeTransformation::IDENTITY;
    let mut direction = GeVector::default();
    let count = klass(kl).mCrackCount;
    let step = 6.2831855f32 / count as f32;
    set_area(c, kl);
    let center = target_location(c);
    let mut i: c_ulong = 0;
    while i < count as c_ulong {
        direction.y = generate() - 0.5 + klass(kl).mRadius;
        t.rotate(((generate() * 0.5 - 0.25) * step) + step, 0.0, 0.0);
        let offset = t.apply_vector(direction);
        let to = Ge3DPoint::new(center.x + offset.x, offset.y + center.y, 0.0);
        if eff_between(&center, &to, effect(kl, 1)) != 0 {
            break;
        }
        i = i.wrapping_add(1);
    }
}

/// EECCHurricane::CreateStorm 0xb1226: the storm unit centred on the target at the calamity's height, with the storm
/// effect on it, as the calamity's target; false if the unit could not be created.
fn create_storm(kl: P, c: P) -> bool {
    let mut t = GeTransformation::IDENTITY;
    let at = target_location(c);
    let index = klass(kl).mUnitClassIndex as i64;
    let half = ucf(hp(O::UcAt, 0, &[index])).mTileSize as f32 * 0.5;
    set_position(&mut t, at.x - half, at.y - half, cal(c).mHeight);
    let u = hp(O::CalCreateUnit, c, &[index, ptr(&mut t), -1]);
    if u == 0 {
        return false;
    }
    eff_unit(u, effect(kl, 2), None);
    h(O::CxSetBuilt, complex(u), &[1, 0, 1]);
    set_target(c, u);
    true
}

/// The map tiles within the class's radius of the target (EECCAreaEffect 0xaf411: minus those an area effect makes
/// immune when immunity blocks it; EECCLeprosy 0xb1a97: all).
fn affected_tiles(host: P, kl: P, c: P, immunity: bool) -> Vec<[i32; 2]> {
    let at = target_location(c);
    let v = host_tiles(host);
    // SAFETY: the host's tile vector
    let tiles = unsafe {
        ee_sim::units::uu_ffi::ee_units_uu_tiles_in_radius(
            floor(at.x),
            floor(at.y),
            klass(kl).mRadiusTiles as u32,
            1,
            v,
        );
        tiles_of(v)
    };
    let mut out = Vec::new();
    for t in tiles {
        if h(O::MapTile, 0, &[t[0] as i64, t[1] as i64]) == 0 {
            continue;
        }
        if !immunity
            || !klass(kl).mIsBlockedByImmunity
            || h(O::AeHas, 0, &[3, t[0] as i64, t[1] as i64, cal(c).mPlayerNumber as i64]) == 0
        {
            out.push(t);
        }
    }
    out
}

/// EECCAreaEffect::ApplyEffect 0xaf726 (also the extra damage's and the mind control's, which affects the caster's
/// own units too) and EECCLeprosy::ApplyEffect 0xb1dd3: the units in range whose type is in the unit set handed to
/// the contagion helper; the area effects take enemies of the caster (of nature without one), the leprosy every unit
/// but an immune one (which shows its immunity when immunity blocks the leprosy).
fn apply_effect(host: P, kl: P, c: P) {
    let mut effect_out: P = 0;
    let leprosy = klass(kl).mClassEnum as i32 == kind::CAL_K_LEPROSY;
    let at = target_location(c);
    affected_tiles(host, kl, c, !leprosy);
    let v = host_scratch(host);
    // SAFETY: the host's unit vector
    let units = unsafe {
        ee_sim::units::uu_ffi::ee_units_uu_units_in_radius(at.x, at.y, klass(kl).mRadiusTiles as f32, v);
        units_of(v)
    };
    let helper = hp(O::ContagionHelper, 0, &[cal(c).mIntPlaceHolder1 as i64]);
    for &u in units.iter().rev() {
        let u = u as P;
        let unit_type = ucf(unit_class(u)).mUnitType as i64;
        if h(O::UnitSetContains, 0, &[unit_type, h(O::KlUnitSet, kl, &[])]) == 0 {
            continue;
        }
        if leprosy {
            if klass(kl).mIsBlockedByImmunity && h(O::CxGetEffect, u, &[3, ptr(&mut effect_out)]) != 0 {
                if effect_out != 0 {
                    eff_unit(u, effect_out, None);
                }
            } else {
                h(O::HelperAddUnit, helper, &[pa(u), 1]);
            }
        } else if caster(c) != 0 {
            let caster_player = unit_player(caster(c));
            let player = unit_player(u);
            if (player != caster_player && enemy(caster_player, player_id(player)) != 0)
                || klass(kl).mClassEnum as i32 == kind::CAL_K_AE_MIND_CONTROL
            {
                h(O::HelperAddUnit, helper, &[pa(u), 1]);
            }
        } else {
            let gaia = hp(O::WorldPlayer, 0, &[0]);
            let player = unit_player(u);
            if player != gaia && enemy(gaia, player_id(player)) != 0 {
                h(O::HelperAddUnit, helper, &[pa(u), 1]);
            }
        }
    }
}

/// EECCLeprosy::ShowEffects 0xb1cca: the leprosy effect from the caster (or the target without one; 0xb1a52) to a
/// random point on every affected tile.
fn leprosy_effects(host: P, kl: P, c: P) {
    let tiles = affected_tiles(host, kl, c, false);
    let source = if caster(c) != 0 { position(&location(complex(caster(c)))) } else { target_location(c) };
    for t in tiles {
        let x = t[0] as f32 + (random() - 0.5) + 0.5;
        let y = t[1] as f32 + (generate() - 0.5) + 0.5;
        let z = hd(O::TileHeight, hp(O::MapTile, 0, &[t[0] as i64, t[1] as i64]), &[], &[]);
        eff_between(&source, &Ge3DPoint::new(x, y, z), effect(kl, 1));
    }
}

/// EECCVolcano::ClearArea 0xb46bc: movable units off the volcano's footprint, to their next waypoint outside it, else
/// onto the first free ring tile (nearest the sort corner), their path dropped.
fn volcano_clear_area(host: P, uc: P, x: c_long, y: c_long) {
    let size = ucf(uc).mTileSize;
    let half = size / 2;
    let left = x.wrapping_sub(half);
    let top = y.wrapping_sub(half);
    let extent = half - if size % 2 == 0 { 1 } else { 0 };
    let area = GeRectangle {
        bottom: y.wrapping_add(extent) as f32,
        left: left as f32,
        right: x.wrapping_add(extent) as f32,
        top: top as f32,
    };
    let v = host_scratch(host);
    // SAFETY: the host's unit vector
    let units = unsafe {
        ee_sim::units::uu_ffi::ee_units_uu_units_in_rectangle(area.left, area.top, area.right, area.bottom, v);
        units_of(v)
    };
    for &u in &units {
        let cx = complex(u as P);
        if cx == 0 || ee_sim::units::uu_ffi::ee_units_uu_is_unit_movable(cx as u64) == 0 {
            continue;
        }
        let mut moved = false;
        if h(O::CxWaypointCount, cx, &[]) > 0 {
            while h(O::CxNextWaypoint, cx, &[]) == 0 {
                let mut waypoint = Ge3DPoint::default();
                let mut length = 0f32;
                if h(O::CxGetPathStart, cx, &[ptr(&mut waypoint), ptr(&mut length)]) != 0 {
                    continue;
                }
                let (wx, wy) = (waypoint.x, waypoint.y);
                if wx >= area.left && wx <= area.right && wy >= area.top && wy <= area.bottom {
                    continue;
                }
                let mut t = location(cx);
                set_position(&mut t, waypoint.x, waypoint.y, waypoint.z);
                set_transformation(cx, &t);
                moved = true;
                break;
            }
        }
        if moved {
            continue;
        }
        let tv = hp(O::HostTiles, host, &[]) as *mut c_void;
        // SAFETY: the host's tile vector (replaced: not appended)
        let tiles = unsafe {
            ee_sim::units::uu_ffi::ee_units_uu_block_ring(
                0,
                left as i32,
                top as i32,
                size as i32,
                size as i32,
                tv,
                0,
                1,
            );
            h(O::SortTilesNear, tv as P, &[left.wrapping_add(size).wrapping_sub(1) as i64, top as i64]);
            tiles_of(tv)
        };
        for t in tiles {
            let (tx, ty) = (t[0] as i64, t[1] as i64);
            let tile = hp(O::MapTile, 0, &[tx, ty]);
            if tile == 0
                || h(O::TileUnits, tile, &[]) != 0
                || ee_sim::units::uu_ffi::ee_units_uu_can_stand_on(t[0], t[1], cx as u64) == 0
            {
                continue;
            }
            let mut tr = location(cx);
            let mut at = Ge3DPoint::new(t[0] as f32 + 0.5, t[1] as f32 + 0.5, tr.z());
            let class = unit_class(cx);
            if ucf(class).mDomains & k::CAL_C_MOVEMENT_AIR != 0 {
                at.z = hd(O::UcHeightAt, class, &[pa(cx), cptr(&at)], &[]);
            } else if mesh_valid(tx, ty) {
                at.z = mesh_z(at.x, at.y);
            }
            set_position(&mut tr, at.x, at.y, at.z);
            set_transformation(cx, &tr);
            h(O::CxDropPath, cx, &[]);
            start_action(cx, k::CAL_C_ACTION_IDLE);
            break;
        }
    }
}

/// The Windows `max(a, b)`: `(a > b) ? a : b`.
fn wmax(a: f32, b: f32) -> f32 {
    if a > b {
        a
    } else {
        b
    }
}

/// EECCVolcano::GetMaximumHeight 0xb4b62: the highest terrain mesh height in the 7 by 7 tile block around the
/// location.
fn volcano_maximum_height(at: &Ge3DPoint) -> f32 {
    let z = |x: i32, y: i32| hd(O::MeshGetZ, 0, &[x as i64, y as i64], &[]);
    let mut maximum = -3.4028235e38f32;
    let radius = floor(3.5);
    let end_x = floor(at.x).wrapping_add(radius);
    let start_y = floor(at.y).wrapping_sub(radius);
    let end_y = floor(at.y).wrapping_add(radius);
    let mut y = end_y;
    while y > start_y {
        let mut x = floor(at.x).wrapping_sub(radius);
        while x < end_x {
            maximum = wmax(maximum, z(x, y + 1));
            maximum = wmax(maximum, z(x + 1, y + 1));
            maximum = wmax(maximum, z(x + 1, y));
            maximum = wmax(maximum, z(x, y));
            x += 1;
        }
        y -= 1;
    }
    maximum
}

/// EECCVolcano::CreateVolcano 0xb4d19: the volcano unit for the calamity's player centred on the target, units cleared
/// off its footprint, as the calamity's target.
fn create_volcano(host: P, kl: P, c: P) {
    let mut t = GeTransformation::IDENTITY;
    let at = target_location(c);
    let index = klass(kl).mUnitClassIndex as i64;
    let uc = hp(O::UcAt, 0, &[index]);
    let half = ucf(uc).mTileSize as f32 * 0.5;
    set_position(&mut t, at.x - half, at.y - half, cal(c).mHeight - 0.9);
    let u = hp(O::CalCreateUnit, c, &[index, ptr(&mut t), cal(c).mPlayerNumber as i64]);
    if u != 0 {
        h(O::CxSetBuilt, complex(u), &[1, 0, 1]);
        volcano_clear_area(host, uc, floor(at.x - 0.5) as c_long, floor(at.y - 0.5) as c_long);
        set_target(c, u);
    }
}

/// EECCTimeWarp::PlayEffect 0xb3ede: the departure (main sound; the secondary one without it) or arrival (secondary
/// sound) effect at the location.
fn warp_effect(kl: P, at: &Ge3DPoint, c: P, departing: bool) {
    let mut handle = 0u32;
    let e = if departing { effect(kl, 2) } else { effect(kl, 3) };
    if e != 0 {
        eff_placed(at, e, Some(&mut handle));
        *effect_handle(c) = handle;
    }
    // the departure plays the main sound (the secondary one used to play here too, so the main sound was never heard)
    let main = hp(O::KlSound, kl, &[]);
    let sound = if departing && main != 0 { main } else { hp(O::KlSecondary, kl, &[]) };
    let node = play_node(sound, volume(sound), 0.0, 1.0);
    keep_sound(c, node, !departing);
}

/// The ground under (x, y), 0 off the mesh.
fn ground_at(x: f32, y: f32) -> (f32, bool) {
    let valid = mesh_valid(floor(x) as i64, floor(y) as i64);
    (if valid { mesh_z(x, y) } else { 0.0 }, valid)
}

/// EECCTimeWarp::Return 0xb3fc9: the target back near its old tile (at its old height above ground if it flies, a
/// ship on the water, facing as before) with the arrival effects; killed if no place is found.
fn warp_return(kl: P, c: P) {
    let target_unit = target(c);
    if target_unit == 0 {
        return;
    }
    let cx = complex(target_unit);
    h(O::CxSetIsTinted, cx, &[1]);
    // SAFETY: the calamity's U2DPoint<unsigned long> member
    let tile = unsafe { *rec::<[c_ulong; 2]>(O::CalTile, c) };
    let Some(mut t) = find_placement(kl, cx, tile[0], tile[1]) else {
        h(O::UnKill, cx, &[1, 0, 0]);
        return;
    };
    let (ground, valid) = ground_at(t.x(), t.y());
    let class = unit_class(cx);
    let domains = ucf(class).mDomains;
    if domains & k::CAL_C_MOVEMENT_AIR != 0 {
        t.m[2][3] = cal(c).mFloatPlaceHolder1 + ground;
    } else if domains & k::CAL_C_MOVEMENT_SEA != 0 && valid {
        // put the ship back on the water surface
        let mut g = position(&t);
        h(O::UcGroundLocation, class, &[ptr(&mut g)]);
        t.m[2][3] = g.z;
    }
    let mut at = position(&t);
    at.z = hd(O::CxEffectHeight, cx, &[1], &[]) * 0.5 + at.z;
    eff_placed(&at, effect(kl, 4), None);
    let mut at = position(&t);
    at.z = hd(O::CxEffectHeight, cx, &[1], &[]) + at.z + 0.2;
    warp_effect(kl, &at, c, false);
    t.set_orientation(RotationOrder::Ypr, cal(c).mFloatPlaceHolder2, 0.0, 0.0);
    set_transformation(cx, &t);
}

/// EECCTimeWarp::Depart 0xb41dd: the live target off the map (off-map reason 3), its height above ground, facing and
/// position remembered; otherwise the calamity is marked finished.
fn warp_depart(c: P) {
    let target_unit = target(c);
    if target_unit == 0 {
        return;
    }
    let cx = complex(target_unit);
    if rts(cx).mTileX == -10000 || !(uf(cx).mHitPoints > 0) {
        let f = cal(c);
        f.mIntPlaceHolder2 = -1;
        f.mBoolPlaceHolder1 = true;
        f.mBoolPlaceHolder2 = true;
        return;
    }
    h(O::CxSetIsTinted, cx, &[0]);
    let p = unit_position(cx);
    let (ground, _) = ground_at(p.x, p.y);
    cal(c).mFloatPlaceHolder1 = p.z - ground;
    let o: GeOrientation = location(cx).orientation();
    cal(c).mFloatPlaceHolder2 = o.yaw;
    *point_at(O::CalTargetLoc, c) = p;
    off_map(cx, 3);
}

/// The teleported caster placed at the tile (EECCTeleport::Teleport's two tries): its group told, the arrival
/// effect, its hit points scaled by the damage share (at least 1), enabled; false if no place is found.
fn teleport_to(kl: P, caster_unit: P, x: c_long, y: c_long) -> bool {
    let cx = complex(caster_unit);
    let Some(t) = find_placement(kl, cx, x as c_ulong, y as c_ulong) else { return false };
    set_transformation(cx, &t);
    let group = hp(O::CxActiveGroup, cx, &[]);
    if group != 0 {
        let at = position(&t);
        h(O::GroupTell, group, &[pa(cx), cptr(&at)]);
    }
    eff_unit(caster_unit, effect(kl, 2), None);
    let hit_points = lrintf(uf(cx).mHitPoints as f32 * klass(kl).mDamageShare);
    h(O::CxSetHitPoints, cx, &[if 1 > hit_points { 1 } else { hit_points as i64 }]);
    set_disabled(cx, false, 0);
    true
}

/// EECCTeleport::Teleport 0xb3665: the live caster near the target, else near where it left, else killed; the
/// arrival sound.
fn teleport(kl: P, c: P) {
    let caster_unit = caster(c);
    if caster_unit == 0 || !(uf(caster_unit).mHitPoints > 0) {
        return;
    }
    let sound = hp(O::KlSecondary, kl, &[]);
    let node = play_node(sound, volume(sound), 0.0, 1.0);
    keep_sound(c, node, true);
    let cx = complex(caster_unit);
    let at = target_location(c);
    // SAFETY: the unit's U2DPoint<float> member
    let left = unsafe { *rec::<[f32; 2]>(O::CxU0DC, cx) };
    if !teleport_to(kl, caster_unit, floor(at.x) as c_long, floor(at.y) as c_long)
        && !teleport_to(kl, caster_unit, floor(left[0]) as c_long, floor(left[1]) as c_long)
    {
        h(O::UnKill, cx, &[1, 0, 0]);
    }
}

/// EECCTeleport::HoldCaster 0xb390b: the live caster off the map at its position, disabled, off-map reason 4.
fn teleport_hold(c: P) {
    let caster_unit = caster(c);
    if caster_unit == 0 || !(uf(caster_unit).mHitPoints > 0) {
        return;
    }
    let cx = complex(caster_unit);
    set_disabled(cx, true, 0);
    let t = location(cx);
    let p = [t.x(), t.y()];
    h(O::CxSet2DPosition, cx, &[cptr(&p)]);
    off_map(cx, 4);
}

/// The first part of a unit's model, LOD 0's bounding radius (None without a model or part).
fn model_radius(u: P) -> Option<f32> {
    let model = hp(O::UnGetModel, u, &[]);
    if model == 0 {
        return None;
    }
    let part = hp(O::ModelPart0, model, &[]);
    if part == 0 {
        return None;
    }
    Some(hd(O::GeModelRadius, part, &[0], &[]))
}

/// EECCLogicBomb's AdjustLocation 0xb21c2: the location raised by the target model's first part's LOD-0 bounding
/// radius.
fn bomb_adjust(c: P, at: &mut Ge3DPoint) {
    let target_unit = target(c);
    if target_unit == 0 {
        return;
    }
    if let Some(r) = model_radius(target_unit) {
        at.z = r + at.z;
    }
}

/// EECCLogicBomb's disabling of a live target (its update): the disable effect above it, disabled (0x1c), a wonder
/// no more; halted, a gate opened.
fn bomb_disable(kl: P, c: P) {
    let target_unit = target(c);
    if target_unit == 0 {
        return;
    }
    let cx = hp(O::UnGetComplex, target_unit, &[]);
    if !(uf(cx).mHitPoints > 0) {
        return;
    }
    let e = effect(kl, 3);
    if e != 0 {
        let mut handle = 0u32;
        let model = hp(O::UnGetModel, cx, &[]);
        let height = hd(O::GeModelRadius, hp(O::ModelPart0, model, &[]), &[0], &[]);
        let mut p = unit_position(cx);
        p.z = p.z + height * 1.5;
        eff_placed(&p, e, Some(&mut handle));
        *effect_handle(c) = handle;
    }
    set_disabled(cx, true, 0x1c);
    let node = hp(O::UnTechNode, cx, &[]);
    if node != 0 && h(O::NodeDataKind, node, &[]) == 4 {
        h(O::PlRemoveWonder, unit_player(cx), &[pa(cx)]);
    }
    if ucf(unit_class(target_unit)).mCategory as u64 != k::CAL_C_UNIT_CATEGORY_GATE {
        h(O::CxHalt, cx, &[]);
    } else {
        start_action(cx, k::CAL_C_ACTION_OPEN);
        let class = unit_class(target_unit);
        hf(O::CxSetOpenFraction, cx, &[], &[1.0]);
        hf(O::CxSetOpenFraction, hp(O::UcLastLinkedCarrier, class, &[pa(cx)]), &[], &[1.0]);
        hf(O::CxSetOpenFraction, hp(O::UcLastLinked, class, &[pa(cx)]), &[], &[1.0]);
    }
}

/// EECCMeteorStorm::LaunchMeteor 0xb2795: a nature-owned meteor 7 tiles north of and above a random point near the
/// target, diving onto that point.
fn launch_meteor(kl: P, c: P) {
    let at = target_location(c);
    let offset_x = (0.5 - random()) * klass(kl).mRadius;
    let offset_y = (0.5 - random()) * klass(kl).mRadius;
    let class = hp(O::UcAt, 0, &[klass(kl).mUnitClassIndex as i64]);
    let start = Ge3DPoint::new(at.x + offset_x, offset_y + (at.y + 7.0), at.z + 7.0);
    let goal_at = Ge3DPoint::new(start.x, at.y + offset_y, at.z);
    let gaia = hp(O::WorldPlayer, 0, &[0]);
    let u = hp(
        O::UcCreateUnit,
        class,
        &[floor(start.x) as i64, floor(start.y) as i64, player_id(gaia) as i64, 0, pa(caster(c))],
    );
    let meteor = complex(u);
    h(O::UcPlayCreationEffect, class, &[pa(meteor)]);
    h(O::CxSetLauncherClass, meteor, &[pa(class)]);
    h(O::CxSetOwner, meteor, &[pa(meteor)]);
    let goal = hp(O::NewAttackGround, 0, &[cptr(&goal_at), 0]);
    h(O::CxSetGoal, meteor, &[pa(goal), 0, 0]);
    hf(O::CxSetVelocity, meteor, &[], &[hd(O::CxMaxVelocity, meteor, &[], &[])]);
    h(O::CxSetProjectileStart, meteor, &[cptr(&start)]);
    h(O::CxSetLocation, meteor, &[cptr(&start), 1]);
    hf(O::CxSetHeight, meteor, &[], &[start.z]);
    let mut t = location(meteor);
    let p = position(&t);
    let direction = t.apply_vector(GeVector::new(0.0, 1.0, 0.0));
    let yaw = um_arctan2(-direction.x, direction.y);
    h(O::CxSetFlightTime, meteor, &[0]);
    hf(O::CxSetOrientation, meteor, &[], &[yaw, -0.707, 0.0]);
    h(O::CxSetProjectileStart, meteor, &[cptr(&p)]);
    hf(O::CxSetVelocity, meteor, &[], &[um_cos(-0.707) * hd(O::CxMaxVelocity, meteor, &[], &[])]);
    t.set_orientation(RotationOrder::Ypr, yaw, -0.707, 0.0);
    let mut tile_x: c_long = 0;
    let mut tile_y: c_long = 0;
    h(O::UpdateUnitTile, meteor, &[ptr(&mut t), ptr(&mut tile_x), ptr(&mut tile_y)]);
}

/// EECCCloaking::CloakUnit 0xafd37: a living capitol that is not already casting casts cloaking on itself, its power
/// topped up to the cost first.
fn cloak_unit(c: P, cx: P) {
    let goal = hp(O::CxGoalState, cx, &[]);
    if !(uf(cx).mHitPoints > 0) || (goal != 0 && h(O::GoalKind, goal, &[]) == k::CAL_C_GOAL_CAST_CALAMITY) {
        return;
    }
    let cost = h(O::KlCost, hp(O::CalClass, c, &[]), &[]) as c_long;
    if cf(cx).mPower < cost && cf(cx).mPower != cost {
        cf(cx).mPower = cost;
    }
    let (x, y) = (rts(cx).mTileX, rts(cx).mTileY);
    let goal = hp(O::NewCastCalamity, cx, &[0, x as i64, y as i64, 0x43d, 1]);
    h(O::CxSetGoal, cx, &[pa(goal), 0, 0]);
    start_action(cx, k::CAL_C_ACTION_CALAMITY_PRE_CAST);
}

/// EECCCloaking::CloakGroup 0xafb38: the player's capitols chained within twice the cloak radius of the caster, each
/// other one cloaked too (a capitol near several members is listed more than once).
fn cloak_group(host: P, c: P, cx: P, area_effect: P) {
    let v = host_scratch(host);
    let player = player_id(unit_player(cx));
    // SAFETY: the host's unit vector
    let candidates = unsafe {
        ee_sim::units::uu_ffi::ee_units_uu_player_units_of_type(k::CAL_C_UNIT_TYPE_CAPITOL as i32, player as i32, v);
        units_of(v)
    };
    let range = hd(O::AeRadius, area_effect, &[], &[]) * 2.0;
    let mut group: Vec<P> = vec![cx];
    loop {
        let mut added = false;
        for &cand in &candidates {
            let cand = hp(O::UnCastComplex, cand as P, &[]);
            if group.contains(&cand) {
                continue;
            }
            let cl = unit_position(cand);
            let size = group.len();
            for m in 0..size {
                let ml = unit_position(group[m]);
                if (cl.x - ml.x).abs() <= range && (cl.y - ml.y).abs() <= range {
                    group.push(cand);
                    added = true;
                }
            }
        }
        if !added {
            break;
        }
    }
    for &m in &group {
        let m = hp(O::UnCastComplex, m, &[]);
        if m != cx {
            cloak_unit(c, m);
        }
    }
}

/// The run's objects.
struct Run {
    host: P,
    kl: P,
    c: P,
}

/// The classes' own steps of their Rust Start and Stop (EeCalamityOp::ClassStep, its index), by class.
fn class_step(r: &Run, call: &EeCalamityCall, out: &mut EeCalamityReply) -> bool {
    let (kl, c) = (r.kl, r.c);
    let target_unit = target(c);
    let caster_unit = caster(c);
    let class_enum = klass(kl).mClassEnum as i32;
    let step = class_enum.wrapping_mul(8).wrapping_add(call.index);
    let is = |kind: i32, index: i32| step == kind * 8 + index;
    if is(kind::CAL_K_EARTHQUAKE, 1) {
        // CreateSourceEffect's caster part: the quake's area around the target, the source effect from the caster
        let at = target_location(c);
        set_area(c, kl);
        let cx = complex(caster_unit);
        if cx != 0 {
            let source = position(&location(cx));
            out.ok = true;
            out.value = eff_between(&source, &at, effect(kl, 2)) as i32;
            out.x = source.distance_to(at);
        }
    } else if is(kind::CAL_K_HURRICANE, 1) {
        caster_effect(kl, c, call, false);
    } else if is(kind::CAL_K_LOGIC_BOMB, 4) || is(kind::CAL_K_DROP_OBJECT, 2) {
        caster_effect(kl, c, call, true);
    } else if is(kind::CAL_K_AREA_EFFECT, 1) || is(kind::CAL_K_AE_EXTRA_DAMAGE, 1) || is(kind::CAL_K_AE_MIND_CONTROL, 1)
    {
        // the area effects keep the handle
        out.id = caster_effect(kl, c, call, true);
    } else if is(kind::CAL_K_PLAGUE, 1) {
        // EECCPlague::ShowEffect 0xb3131: a disease effect between the caster's and the target's positions, or a
        // tracer beam between them
        if caster_unit != 0 {
            let e = effect(kl, 1);
            let kind = h(O::EffectKind, e, &[]);
            if kind == k::CAL_C_EFFECT_3D_DISEASE {
                let from = unit_position(caster_unit);
                let to = unit_position(target_unit);
                eff_between(&from, &to, e);
            } else if kind == k::CAL_C_EFFECT_3D_TRACER_BEAM {
                eff_between_units(caster_unit, target_unit, e, None);
            }
        }
    } else if is(kind::CAL_K_LEPROSY, 1) {
        leprosy_effects(r.host, kl, c);
    } else if is(kind::CAL_K_VOLCANO, 1) {
        // the placed effect at the volcano, then the highest ground around it
        let at = Ge3DPoint::new(call.x, call.y, call.z);
        start_point(&at);
        eff_placed(&at, effect(kl, 1), None);
        out.x = volcano_maximum_height(&at);
    } else if is(kind::CAL_K_FIRE, 1) {
        eff_unit(target_unit, effect(kl, 1), None);
    } else if is(kind::CAL_K_MORALE, 1) {
        let mut handle = 0u32;
        eff_between_units(caster_unit, complex(target_unit), effect(kl, 1), Some(&mut handle));
        out.id = handle;
    } else if is(kind::CAL_K_MORALE, 2) {
        let f = cf(complex(target_unit));
        f.mPositiveMorale = f.mPositiveMorale.wrapping_sub(1);
    } else if is(kind::CAL_K_MORALE, 3) {
        let f = cf(complex(target_unit));
        f.mNegativeMorale = f.mNegativeMorale.wrapping_sub(1);
    } else if is(kind::CAL_K_TIME_WARP, 1) {
        let cx = complex(target_unit);
        set_disabled(cx, true, 0xf);
        h(O::CxHalt, complex(target_unit), &[]);
    } else if is(kind::CAL_K_TIME_WARP, 2) {
        // the warp's effect above the unit
        let cx = complex(target_unit);
        let mut handle = 0u32;
        let mut at = unit_position(cx);
        at.z = hd(O::CxEffectHeight, cx, &[1], &[]) + at.z + 0.2;
        start_point(&at);
        eff_at(caster_unit, &at, effect(kl, 1), Some(&mut handle));
        out.id = handle;
    } else if is(kind::CAL_K_TIME_WARP, 3) {
        // untinted, its orders cleared, enabled; its category
        let cx = complex(target_unit);
        h(O::CxSetIsTinted, cx, &[0]);
        clear_orders(cx);
        out.value = ucf(unit_class(cx)).mCategory as i32;
    } else if is(kind::CAL_K_TIME_WARP, 4) {
        start_action(complex(target_unit), call.value as i64);
    } else if is(kind::CAL_K_TELEPORT, 1) {
        set_disabled(complex(caster_unit), true, 0x10);
        eff_unit(caster_unit, effect(kl, 1), None);
    } else if is(kind::CAL_K_TELEPORT, 2) {
        set_disabled(complex(caster_unit), false, 0);
    } else if is(kind::CAL_K_CONSCRIPTION, 1) {
        let player = unit_player(caster_unit);
        let tech = hp(O::PlTechnology, player, &[k::CAL_C_TECH_CONSCRIPTION_SPELL]);
        let unit_type = ucf(unit_class(caster_unit)).mUnitType as i64;
        out.ok = h(O::PlCanAfford, player, &[pa(tech), unit_type, 1]) != 0;
    } else if is(kind::CAL_K_CONSCRIPTION, 2) {
        let target_cx = complex(target_unit);
        let player = unit_player(caster_unit);
        let tech = hp(O::PlTechnology, player, &[k::CAL_C_TECH_CONSCRIPTION_SPELL]);
        let unit_type = ucf(unit_class(caster_unit)).mUnitType as i64;
        hf(O::PlPay, player, &[pa(tech), unit_type], &[1.0]);
        h(O::CxPlayEffect, target_cx, &[0x138d]);
        h(O::PlayUnitEffect, target_cx, &[0x138d]);
    } else if is(kind::CAL_K_LOGIC_BOMB, 1) {
        let mut at = Ge3DPoint::new(call.x, call.y, call.z);
        bomb_adjust(c, &mut at);
        out.x = at.x;
        out.y = at.y;
        out.z = at.z;
    } else if is(kind::CAL_K_LOGIC_BOMB, 2) {
        // the target enabled again; its node's kind (-1 without one)
        set_disabled(complex(target_unit), false, 0);
        h(O::UcActionChanged, unit_class(target_unit), &[pa(complex(target_unit))]);
        let node = hp(O::UnTechNode, target_unit, &[]);
        out.value = if node != 0 { h(O::NodeDataKind, node, &[]) as i32 } else { -1 };
    } else if is(kind::CAL_K_LOGIC_BOMB, 3) {
        h(O::PlAddWonder, unit_player(target_unit), &[pa(complex(target_unit)), 0]);
    } else if is(kind::CAL_K_DROP_OBJECT, 1) {
        // the dropped unit next to the target, attacking it
        let dropper = complex(caster_unit);
        let mut at = Ge3DPoint::default();
        let drop_target = hp(O::CombatFindDrop, dropper, &[pa(target_unit), ptr(&mut at)]);
        let class = hp(O::UcAt, 0, &[klass(kl).mUnitClassIndex as i64]);
        let player = player_id(unit_player(dropper));
        let u = hp(O::UcCreateUnit, class, &[floor(at.x) as i64, floor(at.y) as i64, player as i64, 0, pa(dropper)]);
        h(O::CxSetLauncherClass, complex(u), &[pa(class)]);
        h(O::CxSetOwner, complex(u), &[pa(complex(u))]);
        let goal = hp(O::NewAttackUnit, hp(O::UnGetComplex, u, &[]), &[pa(drop_target), 0, 0]);
        h(O::CxSetGoal, complex(u), &[pa(goal), 0, 0]);
        hf(O::CxSetVelocity, complex(u), &[], &[0.0]);
        h(O::CxSetProjectileStart, complex(u), &[cptr(&at)]);
        hf(O::CxSetHeight, complex(u), &[], &[at.z]);
    } else if is(kind::CAL_K_CLOAKING, 1) {
        // the cloak's area effect on the caster, the target its position, the sound, the cloak flag; a capitol's
        // group
        let cx = hp(O::UnCastComplex, caster_unit, &[]);
        let area_effect = hp(O::AeCreate, 0, &[0x2e, pa(cx)]);
        h(O::CxAreaEffectsAdd, cx, &[pa(area_effect)]);
        h(O::RefDereference, area_effect, &[]);
        *point_at(O::CalTargetLoc, c) = unit_position(cx);
        let sound = hp(O::KlSound, kl, &[]);
        let node = play_node(sound, volume(sound), 0.0, 1.0);
        keep_sound(c, node, false);
        cf(cx).mCloaked = 1;
        h(O::HostSetPending, r.host, &[pa(area_effect)]);
        out.ok = ucf(unit_class(cx)).mUnitType as u64 == k::CAL_C_UNIT_TYPE_CAPITOL;
    } else if is(kind::CAL_K_CLOAKING, 2) {
        let cx = hp(O::UnCastComplex, caster_unit, &[]);
        cloak_group(r.host, c, cx, hp(O::HostPending, r.host, &[]));
    } else if is(kind::CAL_K_CLOAKING, 3) {
        eff_unit(caster_unit, effect(kl, 1), None);
    } else if is(kind::CAL_K_CLOAKING, 4) {
        h(O::CxAreaEffectsRemove, hp(O::UnCastComplex, caster_unit, &[]), &[8]);
    } else if is(kind::CAL_K_CLOAKING, 5) {
        cf(hp(O::UnCastComplex, caster_unit, &[])).mCloaked = 0;
    } else {
        return false;
    }
    true
}

fn host_unit(host: P, index: i32) -> P {
    hp(O::HostUnitAt, host, &[index as i64])
}

/// Every call of the classes' Rust starts, stops and updates; false for one no class answers.
fn answer(r: &Run, call: &EeCalamityCall, out: &mut EeCalamityReply) -> bool {
    use EeCalamityOp as C;
    let (host, kl, c) = (r.host, r.kl, r.c);
    match call.op {
        // the handles the world views read through (calamity_views.rs: the target's, the caster's and the tiles'
        // plain members never reach this host)
        C::TargetHandle => out.key = target(c) as u64,
        C::CasterHandle => out.key = caster(c) as u64,
        C::WorldHandle => out.key = hp(O::WorldAddr, 0, &[]) as u64,
        C::HelperUpdate => {
            let helper = hp(O::ContagionHelper, 0, &[call.index as i64]);
            capture(cap::CAL_W_HELPER_UPDATE, call.index as i64, (helper != 0) as i64, 0.0, 0.0);
            if helper != 0 {
                hf(O::CapContagion, helper, &[], &[call.x]);
            }
        }
        C::Sound => {
            let sound = hp(O::HostSound, host, &[(call.index != 0) as i64]);
            hf(O::KlUpdateSound, kl, &[pa(c), pa(sound), (call.value != 0) as i64], &[call.x, call.y]);
        }
        C::UnitsNear => {
            let at = [call.x, call.y, call.z];
            let units = hp(O::HostUnits, host, &[]) as *mut c_void;
            // SAFETY: the host's unit vector
            let near = unsafe { ee_sim::units::uu_ffi::ee_units_uu_units_near(&at, call.w, units) };
            capture(cap::CAL_W_UNITS_NEAR, near as i64, 0, 0.0, 0.0);
            out.value = near;
            out.id = h(O::HostUnitCount, host, &[]) as u32;
        }
        C::CasterContinent => out.value = h(O::ContinentOfUnit, caster(c), &[]) as i32,
        C::QuakeUnit => {
            // the earthquake's tests in its order: a land building, alive, then the continent
            let u = host_unit(host, call.index);
            out.id = rts(u).mUnitID as u32;
            if h(O::UcIsLandBuilding, unit_class(u), &[]) != 0 {
                out.flags |= 1;
                if uf(u).mHitPoints > 0 {
                    out.flags |= 2;
                    out.value = h(O::ContinentOfUnit, u, &[]) as i32;
                }
            }
        }
        C::Immune => {
            let u = host_unit(host, call.index);
            let mut e: P = 0;
            if h(O::CxGetEffect, u, &[3, ptr(&mut e)]) != 0 {
                capture(cap::CAL_W_IMMUNE, rts(u).mUnitID as i64, 0, 0.0, 0.0);
                if e != 0 {
                    eff_unit(u, e, None);
                }
                out.ok = true;
            }
        }
        C::Carried => {
            let u = host_unit(host, call.index);
            if h(O::CxIsCarried, u, &[]) != 0 {
                out.ok = true;
                out.key = hp(O::CxOutermostCarrier, u, &[]) as u64;
            }
        }
        C::TakeHit => {
            let u = host_unit(host, call.index);
            let attacker = if call.x != 0.0 { target(c) } else { caster(c) };
            let mode: i64 = if klass(kl).mIsBlockedByImmunity { 2 } else { 3 };
            let player = cal(c).mPlayerNumber;
            capture(cap::CAL_W_TAKE_HIT, rts(u).mUnitID as i64, call.value as i64, player as f32, mode as f32);
            h(O::UnTakeHit, u, &[pa(attacker), call.value as i64, player as i64, mode]);
        }
        C::StormUnit => {
            // the hurricane's tests in its order, stopping at the first that skips the unit
            let storm = target(c);
            let storm_unit = complex(storm);
            let u = host_unit(host, call.index);
            out.id = rts(u).mUnitID as u32;
            let player = unit_player(u);
            let storm_player = unit_player(storm_unit);
            if rts(u).mUnitID == rts(storm).mUnitID {
                out.flags |= 1;
            } else if player == storm_player {
                out.flags |= 2;
            } else if enemy(storm_player, player_id(player)) != 0 {
                out.flags |= 4;
                let unit_type = ucf(unit_class(u)).mUnitType as i64;
                if h(O::UnitSetContains, 0, &[unit_type, h(O::KlUnitSet, kl, &[])]) != 0 {
                    out.flags |= 8;
                    if uf(u).mHitPoints > 0 {
                        out.flags |= 16;
                        // SAFETY: live units
                        unsafe {
                            out.x = ee_sim::units::uu_ffi::ee_units_uu_distance_squared(u as u64, storm_unit as u64);
                            out.y = ee_sim::units::uu_ffi::ee_units_uu_angle(
                                0,
                                u as u64,
                                storm_unit as u64,
                                std::ptr::null(),
                                std::ptr::null(),
                            );
                        }
                        if player == hp(O::WorldPlayer, 0, &[0]) {
                            out.flags |= 32;
                        }
                    }
                }
            }
        }
        C::Random => {
            let x = random();
            capture(cap::CAL_W_RANDOM, 0, 0, x, 0.0);
            out.x = x;
        }
        C::RandomGenerate => {
            let x = generate();
            capture(cap::CAL_W_RANDOM, 0, 0, x, 0.0);
            out.x = x;
        }
        C::RandomRange => {
            let x = hd(O::PhysRandomRange, 0, &[], &[call.x, call.y]);
            capture(cap::CAL_W_RANDOM, 1, 0, x, 0.0);
            out.x = x;
        }
        C::PlayClassSound => {
            h(O::KlPlaySound, kl, &[pa(c)]);
        }
        C::DestroyEffect => {
            let handle = call.value as u32;
            capture(cap::CAL_W_DESTROY_EFFECT, handle as c_long as i64, 0, 0.0, 0.0);
            h(O::EffDestroy, 0, &[handle as i64]);
        }
        C::MeshZ => out.x = mesh_z(call.x, call.y),
        // the starts' and stops' (calamity_start.rs)
        C::StopSound => {
            super::calamity_objects::ee_gamecpp_calamity_stop_sound(c as *mut std::ffi::c_void, call.index != 0);
        }
        C::DestroyHelper => {
            h(O::ContagionDestroyVerify, 0, &[call.index as i64, call.value as i64]);
        }
        C::StartHelper => {
            let functor = hp(O::KlContagionFunctor, kl, &[]);
            let id = h(O::ContagionCreate, functor, &[pa(c), 0]);
            out.ok = hp(O::ContagionHelper, 0, &[id]) != 0;
            out.value = id as i32;
        }
        C::StartSound => {
            let sound = hp(O::HostSound, host, &[0]);
            let mut vol = volume(sound);
            let mut pan = 0.0f32;
            let mut pitch = 1.0f32;
            if call.value != 0 {
                let at = Ge3DPoint::new(call.x, call.y, call.z);
                hf(
                    O::SoCalamityParams,
                    0,
                    &[cptr(&at), ptr(&mut vol), ptr(&mut pan), ptr(&mut pitch)],
                    &[volume(sound)],
                );
                hf(O::CapStartEvent, 0, &[k::CAL_C_START_SOUND, 0, 0], &[vol, pan]);
            }
            let node = play_node(sound, vol, pan, pitch);
            keep_sound(c, node, false);
        }
        C::SetHeight => cal(c).mHeight = call.x,
        C::KillTarget => {
            let u = target(c);
            h(O::UnKill, u, &[0, 0, 0]);
            h(O::PlDeselect, unit_player(u), &[pa(u)]);
            set_target(c, 0);
        }
        C::CasterSightOff => {
            h(O::CxSightOffHere, complex(caster(c)), &[]);
        }
        C::CasterSight => out.x = cf(complex(caster(c))).mSight,
        C::CasterSightOn => {
            let cx = complex(caster(c));
            hf(O::CxSetSight, cx, &[], &[call.x]);
            h(O::CxSightOnHere, cx, &[]);
        }
        C::TargetProtected => out.ok = h(O::CxIsProtected, complex(target(c)), &[]) != 0,
        // the classes' own (each answered for the classes whose runs send it)
        C::ClassStep => return class_step(r, call, out),
        C::Cracks => {
            capture(cap::CAL_W_CRACKS, 0, 0, 0.0, 0.0);
            quake_cracks(kl, c);
        }
        C::CameraDistance => {
            // the earthquake's UpdateEpicenterFactor: the distance from the camera to the target, in the view plane
            let at = target_location(c);
            let mut view = Ge3DPoint::default();
            h(O::ViewportInverseApply, 0, &[cptr(&at), ptr(&mut view)]);
            view.y = 0.0;
            out.x = view.distance_to(Ge3DPoint::default());
        }
        C::Shake => {
            // the earthquake's ShakeCamera: the local player's camera (once per player and manager update)
            let player = hp(O::WorldCurrentPlayer, 0, &[]);
            // SAFETY: the live player's record
            let shaken = unsafe { &mut (*rec::<EeUnitsPlayerFields>(O::PlFields, player)).mEarthquakeShaken };
            if !*shaken {
                *shaken = true;
                let (angle, amplitude) = (call.x, call.y);
                capture(cap::CAL_W_SHAKE, 0, 0, angle, amplitude);
                let mut at = Ge3DPoint::default();
                h(O::CameraGetLocation, 0, &[ptr(&mut at)]);
                at.x += um_cos(angle) * amplitude;
                at.y = at.y + um_cos(1.5707964 - angle) * amplitude;
                h(O::CameraSetLocation, 0, &[cptr(&at), 1, 1]);
                *point_at(O::CalShakeLoc, c) = at;
            }
        }
        C::CreateStorm => {
            let created = create_storm(kl, c);
            capture(cap::CAL_W_CREATE_STORM, created as i64, 0, 0.0, 0.0);
        }
        C::StormPrepare => {
            let storm_unit = complex(target(c));
            h(O::CxSetIsDeletable, storm_unit, &[0]);
            let p = position(&location(storm_unit));
            out.x = p.x;
            out.y = p.y;
            out.z = p.z;
        }
        C::Strike => {
            let storm_unit = complex(target(c));
            let u = host_unit(host, call.index);
            capture(cap::CAL_W_STRIKE, rts(u).mUnitID as i64, 0, 0.0, 0.0);
            let e = effect(kl, 3);
            if h(O::EffectKind, e, &[]) == k::CAL_C_EFFECT_2D_LIGHTNING {
                eff_unit(u, e, None);
            } else {
                eff_between_units(storm_unit, u, e, None);
            }
        }
        C::LaunchMeteor => {
            capture(cap::CAL_W_LAUNCH_METEOR, 0, 0, 0.0, 0.0);
            launch_meteor(kl, c);
        }
        C::CreateVolcano => {
            create_volcano(host, kl, c);
            let volcano = target(c);
            eff_unit(volcano, effect(kl, 3), None);
            let result = eff_unit(volcano, effect(kl, 2), None);
            capture(cap::CAL_W_CREATE_VOLCANO, result, 0, 0.0, 0.0);
            out.value = result as i32;
        }
        C::WarpTint | C::WarpAwayEffect => {
            // the time warp's departure: the unit tinted, its effect and sound above it; or the placed effect where
            // it leaves
            let cx = complex(target(c));
            let mut at = unit_position(cx);
            if call.op == C::WarpAwayEffect {
                at.z = hd(O::CxEffectHeight, cx, &[1], &[]) * 0.5 + at.z;
                eff_placed(&at, effect(kl, 4), None);
                return true;
            }
            at.z = hd(O::CxEffectHeight, cx, &[1], &[]) + at.z + 0.2;
            capture(cap::CAL_W_WARP_TINT, 0, 0, 0.0, 0.0);
            h(O::CxSetIsTinted, cx, &[1]);
            warp_effect(kl, &at, c, true);
        }
        C::WarpDepart => {
            capture(cap::CAL_W_WARP_DEPART, 0, 0, 0.0, 0.0);
            warp_depart(c);
        }
        C::WarpReturn => {
            capture(cap::CAL_W_WARP_RETURN, 0, 0, 0.0, 0.0);
            warp_return(kl, c);
        }
        C::WarpBlink => {
            let cx = complex(target(c));
            capture(cap::CAL_W_WARP_BLINK, 0, 0, 0.0, 0.0);
            if h(O::CxIsWarpColor, cx, &[]) != 0 {
                h(O::CxSetBlackColor, cx, &[]);
            } else {
                h(O::CxSetWarpColor, cx, &[]);
            }
        }
        C::Hold => {
            capture(cap::CAL_W_HOLD, 0, 0, 0.0, 0.0);
            teleport_hold(c);
        }
        C::Teleport => {
            capture(cap::CAL_W_TELEPORT, 0, 0, 0.0, 0.0);
            teleport(kl, c);
        }
        C::LogicBombMark => {
            capture(cap::CAL_W_LOGIC_BOMB_MARK, 0, 0, 0.0, 0.0);
            let mut at = target_location(c);
            bomb_adjust(c, &mut at);
            at.z = mesh_z(at.x, at.y) + 0.05;
            eff_placed(&at, effect(kl, 2), None);
        }
        C::LogicBombDisable => {
            capture(cap::CAL_W_LOGIC_BOMB_DISABLE, 0, 0, 0.0, 0.0);
            bomb_disable(kl, c);
        }
        C::CreateHelper => {
            let id = h(O::ContagionCreate, hp(O::KlFunctor, kl, &[]), &[pa(c), 0]);
            capture(cap::CAL_W_CREATE_HELPER, id, 0, 0.0, 0.0);
            out.value = id as i32;
        }
        C::ApplyEffect => {
            capture(cap::CAL_W_APPLY_EFFECT, 0, 0, 0.0, 0.0);
            apply_effect(host, kl, c);
        }
        C::PlacedEffect => {
            let at = Ge3DPoint::new(call.x, call.y, call.z);
            let mut handle = 0u32;
            eff_placed(&at, effect(kl, 2), Some(&mut handle));
            capture(cap::CAL_W_PLACED_EFFECT, handle as c_long as i64, 0, at.x, at.z);
            out.id = handle;
        }
        C::Morale => {
            capture(cap::CAL_W_MORALE, 0, 0, 0.0, 0.0);
            let t = target(c);
            let e = hp(O::UcMoraleEffect, unit_class(t), &[]);
            if e != 0 {
                let mut handle = 0u32;
                eff_unit(t, e, Some(&mut handle));
                if *effect_handle(c) != 0 {
                    h(O::EffDestroy, 0, &[*effect_handle(c) as i64]);
                }
                *effect_handle(c) = handle;
            }
            let f = cf(complex(t));
            if klass(kl).mPositive {
                f.mPositiveMorale = f.mPositiveMorale.wrapping_add(1);
            } else {
                f.mNegativeMorale = f.mNegativeMorale.wrapping_add(1);
            }
            let sound = hp(O::KlSecondary, kl, &[]);
            if sound != 0 {
                hf(O::SoPlay, sound, &[], &[volume(sound), 0.0, 1.0]);
            }
        }
        C::SpecialAttack | C::SpecialLocation => {
            // the caster's order: a special attack on the target unit, or to the target location
            let cx = complex(caster(c));
            if call.op == C::SpecialAttack {
                let t = target(c);
                capture(cap::CAL_W_SPECIAL_ATTACK, rts(t).mUnitID as i64, 0, 0.0, 0.0);
                h(O::CxClearGoals, cx, &[0]);
                let goal = hp(O::NewAttackUnit, cx, &[pa(t), 1, 0]);
                h(O::CxSetGoal, cx, &[pa(goal), 0, 0]);
                cf(cx).mIsSpecialAttack = true;
                start_action(cx, k::CAL_C_ACTION_ATTACK_ENEMY_UNIT);
            } else {
                let at = target_location(c);
                capture(cap::CAL_W_SPECIAL_LOCATION, 0, 0, 0.0, 0.0);
                h(O::CxClearGoals, cx, &[0]);
                let goal = hf(O::NewLocation, 0, &[cptr(&at), 1, 1, 0, 0, 0], &[0.0]) as P;
                h(O::CxSetGoal, cx, &[pa(goal), 0, 0]);
                start_action(cx, k::CAL_C_ACTION_SPECIAL_LOCATION);
            }
        }
        _ => return false,
    }
    true
}

/// The host of the classes' Rust runs (`EeCalamityHostFfi::call`, set by EECalamityUpdate.cpp's runs): the calamity
/// as the Rust update has it for the world step, and back after it.
///
/// # Safety
/// `ctx` is the run's `EECalamityUpdateHost`, the pointers valid for the call (ee/EECalamityUpdate.h).
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_calamity_call(
    ctx: *mut c_void,
    state: *mut EeCalamityState,
    call: *const EeCalamityCall,
    reply: *mut EeCalamityReply,
) {
    let host = ctx as P;
    let r = Run { host, kl: hp(O::HostClass, host, &[]), c: hp(O::HostCalamity, host, &[]) };
    // SAFETY: the caller's contract; the state and call are copied, the reply written once
    unsafe {
        let mut s = *state;
        let call = *call;
        state_to(&s, r.c);
        let mut out = EeCalamityReply::default();
        let answered = answer(&r, &call, &mut out);
        h(O::Verify, 0, &[answered as i64, 1]);
        state_from(r.c, &mut s);
        *state = s;
        *reply = out;
    }
}

/// The calamity's numeric members as a run's state (the run's start).
///
/// # Safety
/// `calamity` is a live `EECalamity`.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_calamity_state(calamity: *mut c_void) -> EeCalamityState {
    let mut s = EeCalamityState::default();
    state_from(calamity as P, &mut s);
    s
}

/// The run's state back into the calamity (the run's end).
///
/// # Safety
/// As [`ee_gamecpp_calamity_state`]; `state` valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_calamity_set_state(state: *const EeCalamityState, calamity: *mut c_void) {
    // SAFETY: the caller's contract
    state_to(unsafe { &*state }, calamity as P);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_windows_max_keeps_the_first_on_a_tie_and_takes_the_second_against_nan() {
        assert_eq!(wmax(1.0, 2.0), 2.0);
        assert_eq!(wmax(2.0, 1.0), 2.0);
        assert!(wmax(f32::NAN, 1.0) == 1.0);
        assert!(wmax(1.0, f32::NAN).is_nan());
        assert_eq!(wmax(-0.0, 0.0).to_bits(), 0.0f32.to_bits());
    }

    #[test]
    fn the_class_steps_are_keyed_as_the_cpp_switch() {
        // ClassStep's `mClassEnum * 8 + index`: no two keys of the classes collide
        let keys = [
            kind::CAL_K_EARTHQUAKE * 8 + 1,
            kind::CAL_K_HURRICANE * 8 + 1,
            kind::CAL_K_LOGIC_BOMB * 8 + 4,
            kind::CAL_K_DROP_OBJECT * 8 + 2,
            kind::CAL_K_AREA_EFFECT * 8 + 1,
            kind::CAL_K_CLOAKING * 8 + 5,
        ];
        let mut sorted = keys.to_vec();
        sorted.sort();
        sorted.dedup();
        assert_eq!(sorted.len(), keys.len());
        assert_eq!(cap::CAL_W_DESTROY_EFFECT, 26);
        assert_eq!(k::CAL_C_ACTION_OPEN, 15000);
    }
}
