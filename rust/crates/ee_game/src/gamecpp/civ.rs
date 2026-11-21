//! The area effects' manager (was EEAEManager.cpp's bodies): collecting the effects around a unit, finding an effect
//! that covers a point, the templates' file, load and media, the active list's rebuild, and the running state a
//! saved game keeps (save 0.256). Each C++ call or member is a one-statement case of EEAEManager.cpp's
//! `EECore9OpD4` ([`super::EeGameCpp`]'s `Ae` ops); the effects' and templates' plain members are read in place
//! through their Rust records (`EeAreaEffectFields`, `EeAreaEffectTemplateFields`).
//!
//! Simulation code (RUST_PLAN 2.1): the rectangle's floors are `UMFloor`'s ([`ee_base::math::um::floor`]) over the
//! same f32 sums; the loops visit the C++ lists in their order.

use std::ffi::{c_long, c_ulong, c_void};

use ee_base::math::types::Ge3DPoint;
use ee_base::math::um;
use ee_sim::tech::area_effect::{
    ee_area_effect_known_kind, EeAreaEffectCall, EeAreaEffectData, EeAreaEffectFields, EeAreaEffectOp,
    EeAreaEffectReply, EeAreaEffectTemplateFields, EE_AREA_EFFECT_ALLIED, EE_AREA_EFFECT_AT_WAR,
    EE_AREA_EFFECT_HEALING, EE_AREA_EFFECT_MORALE_FROM_BUILDINGS, EE_AREA_EFFECT_SAME,
};
use ee_sim::tech::area_effect_save::{
    ee_area_effect_running_free, ee_area_effect_running_load, ee_area_effect_running_save, EeAreaEffectRunning,
    EeAreaEffectSavedOwn, EeAreaEffectSavedRef, SavedUnit,
};
use ee_sim::tech::calamity_record::EeCalamityIoFfi;

use super::{cptr, h, hd, hf, hp, ptr, Owned, O, P};

/// `kErrorOK`.
const AE_OK: c_long = 0;

/// The rectangle the collection searches: `UMFloor` of the centre plus or minus the radius, in U2DRectangle<long>'s
/// member order (bottom, left, right, top).
pub fn collect_rect(x: f32, y: f32, radius: f32) -> [c_long; 4] {
    let bottom = um::floor(y + radius);
    let right = um::floor(x + radius);
    let top = um::floor(y - radius);
    let left = um::floor(x - radius);
    [bottom as c_long, left as c_long, right as c_long, top as c_long]
}

fn fields(e: P) -> EeAreaEffectFields {
    // SAFETY: the effect's record (EEAreaEffect's base), copied out
    unsafe { *super::rec::<EeAreaEffectFields>(O::AeFields, e) }
}

fn template(e: P) -> EeAreaEffectTemplateFields {
    // SAFETY: the effect's template's record, copied out
    unsafe { *super::rec::<EeAreaEffectTemplateFields>(O::AeTemplateFields, e) }
}

fn list_count(list: P) -> i64 {
    h(O::AeListCount, list, &[])
}

fn list_at(list: P, i: i64) -> P {
    hp(O::AeListAt, list, &[i])
}

/// Whether an effect's owner unit is protected (an effect without one is not).
fn owner_protected(e: P) -> bool {
    let owner = hp(O::AeOwner, e, &[]);
    owner != 0 && h(O::AeUnitProtected, owner, &[]) != 0
}

/// EEAEManager::CollectEffects: the list's effects added to every living unit they cover around a location (a
/// radius-0 effect: to every unit of every player); a healing effect skips its own unit and shows on the units it
/// covers.
#[no_mangle]
pub extern "C" fn ee_gamecpp_ae_collect_effects(
    _manager: *mut c_void,
    radius: f32,
    location: *const c_void,
    player: c_ulong,
    list: *mut c_void,
    unit: *mut c_void,
) {
    // SAFETY: the caller's GE2DPoint
    let [x, y] = unsafe { *(location as *const [f32; 2]) };
    let rect = collect_rect(x, y, radius);
    let units = Owned::new(O::AeNewUnitVec, O::AeDeleteUnitVec);
    h(O::AeUnitsInRect, units.0, &[rect.as_ptr() as usize as i64]);
    let (list, unit, loc) = (list as P, unit as P, location as usize as i64);
    for i in 0..list_count(list) {
        let e = list_at(list, i);
        if owner_protected(e) {
            continue;
        }
        let kind = template(e).mKind;
        if template(e).mRadius <= 0.0 {
            super::ae_effect::set_affected_all(e, player as i64, loc);
        } else {
            let count = h(O::AeVecCount, units.0, &[]);
            for k in 0..count {
                let target = hp(O::AeVecComplexAt, units.0, &[k]);
                if h(O::AeCovers, e, &[player as i64, target as i64, loc]) == 0
                    || (target == unit && kind == EE_AREA_EFFECT_HEALING)
                {
                    continue;
                }
                h(O::AeUnitAffectingAdd, target, &[e as i64]);
                h(O::AeAppliedTo, e, &[target as i64]);
                if kind == EE_AREA_EFFECT_HEALING {
                    h(O::AeShowOn, e, &[target as i64]);
                }
            }
        }
    }
}

/// SetAffectedByAreaEffectFunctor::Apply: the effect added to a covered unit's affecting effects and applied.
#[no_mangle]
pub extern "C" fn ee_gamecpp_ae_set_affected(
    effect: *mut c_void,
    player: c_ulong,
    unit: *mut c_void,
    location: *const c_void,
) {
    let (e, unit) = (effect as P, unit as P);
    if h(O::AeCovers, e, &[player as i64, unit as i64, location as usize as i64]) != 0 {
        h(O::AeUnitAffectingAdd, unit, &[e as i64]);
        h(O::AeAppliedTo, e, &[unit as i64]);
    }
}

/// A container emptied (its references let go, then its items).
#[no_mangle]
pub extern "C" fn ee_gamecpp_ae_container_empty(list: *mut c_void) {
    h(O::AeListDereference, list as P, &[]);
    h(O::AeListEmpty, list as P, &[]);
}

/// EEAEManager::ClearEffects: the active list emptied.
#[no_mangle]
pub extern "C" fn ee_gamecpp_ae_clear_effects(manager: *mut c_void) {
    ee_gamecpp_ae_container_empty(hp(O::AeManagerEffects, manager as P, &[]) as *mut c_void);
}

/// EEAEManager::Clear: every template released, then the active list emptied.
#[no_mangle]
pub extern "C" fn ee_gamecpp_ae_clear(manager: *mut c_void) {
    let m = manager as P;
    for i in 0..h(O::AeTemplateCount, m, &[]) {
        let t = hp(O::AeTemplateAt, m, &[i]);
        if t != 0 {
            h(O::AeTemplateDereference, m, &[t as i64]);
        }
    }
    h(O::AeTemplatesClear, m, &[]);
    ee_gamecpp_ae_clear_effects(manager);
}

/// EEAEManager::Load: the templates of the area-effect file (an empty slot for records with id 0); 3 when a template
/// fails to initialize.
#[no_mangle]
pub extern "C" fn ee_gamecpp_ae_load(manager: *mut c_void, file: *mut c_void) -> c_long {
    let (m, file) = (manager as P, file as P);
    let mut count: c_ulong = 0;
    h(O::AeFileOpenRead, file, &[]);
    let mut result = h(O::AeFileReadULong, file, &[ptr(&mut count)]) as c_long;
    if result == AE_OK {
        for _ in 0..count {
            // SAFETY: a plain record of integers, floats and characters: all zeros is the C++'s memset
            let mut data: EeAreaEffectData = unsafe { std::mem::zeroed() };
            let mut read: c_ulong = 0;
            result = h(O::AeFileReadData, file, &[ptr(&mut data), ptr(&mut read)]) as c_long;
            let t = if data.mID > 0 {
                let t = hp(O::AeNewTemplate, m, &[]);
                if h(O::AeTemplateInit, m, &[t as i64, ptr(&mut data)]) == 0 {
                    h(O::AeTemplateError, m, &[ptr(&mut data)]);
                    result = 3;
                }
                t
            } else {
                0
            };
            h(O::AeTemplatePush, m, &[t as i64]);
        }
    }
    h(O::AeFileClose, file, &[]);
    result
}

/// EEAEManager::CreateAreaEffect: the effect of the template at an index (a known kind) for a unit, owned by the
/// unit's player; NULL otherwise.
#[no_mangle]
pub extern "C" fn ee_gamecpp_ae_create(manager: *mut c_void, index: c_ulong, unit: *mut c_void) -> *mut c_void {
    let (m, unit) = (manager as P, unit as P);
    let mut e: P = 0;
    if (index as i64) < h(O::AeTemplateCount, m, &[]) {
        let t = hp(O::AeTemplateAt, m, &[index as i64]);
        // SAFETY: the template's record (a NULL slot faults in the cast's read as the C++'s did)
        let kind = unsafe { (*super::rec::<EeAreaEffectTemplateFields>(O::AeTplFields, t)).mKind };
        if ee_area_effect_known_kind(kind) {
            e = hp(O::AeNew, m, &[t as i64]);
        }
        if e != 0 {
            h(O::AeSetUnit, e, &[unit as i64]);
            let player = h(O::AeUnitPlayerId, unit, &[]) as c_long;
            // SAFETY: the new effect's record
            unsafe { (*super::rec::<EeAreaEffectFields>(O::AeFields, e)).mPlayer = player };
        }
    }
    e as *mut c_void
}

/// EEAEManager::FindEffect: an active effect of a kind covering a point that affects the player (immunities only, by
/// the reversed player test, in immunity mode), skipping protected units' effects and those in the exclusion list.
#[no_mangle]
pub extern "C" fn ee_gamecpp_ae_find(
    manager: *mut c_void,
    kind: c_long,
    x: f32,
    y: f32,
    player: c_ulong,
    mode: bool,
    exclude: *mut c_void,
) -> *mut c_void {
    let effects = hp(O::AeManagerEffects, manager as P, &[]);
    let exclude = exclude as P;
    for i in 0..list_count(effects) {
        let e = list_at(effects, i);
        if owner_protected(e) {
            continue;
        }
        if exclude != 0 && (0..list_count(exclude)).any(|k| list_at(exclude, k) == e) {
            continue;
        }
        if mode && h(O::AeIsImmunity, e, &[]) == 0 {
            continue;
        }
        if template(e).mKind != kind {
            continue;
        }
        let owner = fields(e).mPlayer as i64;
        let test = if !mode { O::AeAffectsPlayer } else { O::AeAffectsPlayerReversed };
        if h(test, e, &[owner, player as c_long as i64]) == 0 {
            continue;
        }
        if hf(O::AeIsInRange, e, &[], &[x, y]) != 0 {
            return e as *mut c_void;
        }
    }
    std::ptr::null_mut()
}

/// EEAEManager::LoadTemplateMedia / UnloadTemplateMedia: the template at an index, if any.
#[no_mangle]
pub extern "C" fn ee_gamecpp_ae_template_media(manager: *mut c_void, index: c_ulong, load: bool) {
    let m = manager as P;
    if index as i64 >= h(O::AeTemplateCount, m, &[]) {
        return;
    }
    let t = hp(O::AeTemplateAt, m, &[index as i64]);
    if t != 0 {
        h(if load { O::AeTemplateLoad } else { O::AeTemplateUnload }, m, &[t as i64]);
    }
}

/// RebuildAreaEffectRegionListFunctor::Apply: a built unit's own effects added to the active list.
#[no_mangle]
pub extern "C" fn ee_gamecpp_ae_rebuild_add(effects: *mut c_void, unit: *mut c_void) {
    let unit = unit as P;
    if h(O::AeUnitBuilt, unit, &[]) == 0 {
        return;
    }
    let own = hp(O::AeUnitOwnList, unit, &[]);
    for i in 0..list_count(own) {
        h(O::AeListAdd, effects as P, &[list_at(own, i) as i64]);
    }
}

/// The saved reference of an effect: its owner unit's id and its index in the owner's own effects (the last match),
/// none without an owner.
fn ref_of(e: P) -> EeAreaEffectSavedRef {
    let mut r = EeAreaEffectSavedRef::default();
    let owner = hp(O::AeOwner, e, &[]);
    if owner == 0 {
        return r;
    }
    r.mOwner = h(O::AeUnitId, owner, &[]) as c_ulong;
    let own = hp(O::AeUnitOwnList, owner, &[]);
    for i in 0..list_count(own) {
        if list_at(own, i) == e {
            r.mIndex = i as c_long;
        }
    }
    r
}

/// The effect a saved reference names, NULL when its owner or index is gone.
fn effect_of(r: &EeAreaEffectSavedRef) -> P {
    if r.mIndex < 0 {
        return 0;
    }
    let owner = complex_of(r.mOwner);
    if owner == 0 {
        return 0;
    }
    let own = hp(O::AeUnitOwnList, owner, &[]);
    if r.mIndex as i64 >= list_count(own) {
        return 0;
    }
    list_at(own, r.mIndex as i64)
}

/// A unit id's complex unit, NULL when none.
fn complex_of(id: c_ulong) -> P {
    let unit = hp(O::AeWorldUnit, 0, &[id as i64]);
    if unit != 0 {
        hp(O::AeUnitComplex, unit, &[])
    } else {
        0
    }
}

fn io(file: *mut c_void) -> EeCalamityIoFfi {
    let call = hp(O::AeIoCall, 0, &[]);
    // SAFETY: EEAEManager.cpp's EEAEIoCall, an I/O callback of this type
    let call = unsafe { std::mem::transmute::<P, unsafe extern "C" fn(*mut c_void, u32, *mut c_void) -> c_long>(call) };
    EeCalamityIoFfi { ctx: file, call: Some(call) }
}

/// EEAESaveRunningState: per unit (of every player but the world's) with effects, its own effects' location (and
/// the morale from buildings' count), then the effects acting on it, each as its owner and index; then the active
/// list the same way.
#[no_mangle]
pub extern "C" fn ee_gamecpp_ae_save_running(file: *mut c_void) {
    let mut state = EeAreaEffectRunning::default();
    for p in 1..h(O::AeWorldNumPlayers, 0, &[]) {
        let player = hp(O::AeWorldPlayer, 0, &[p]);
        if player == 0 {
            continue;
        }
        let mut unit = hp(O::AePlFirstUnit, player, &[]);
        while unit != 0 {
            let u = hp(O::AeUnitComplex, unit, &[]);
            let next = hp(O::AeUnitNext, unit, &[]);
            if u == 0 {
                unit = next;
                continue;
            }
            let own = hp(O::AeUnitOwnList, u, &[]);
            let affecting = hp(O::AeUnitAffectingList, u, &[]);
            if list_count(own) == 0 && list_count(affecting) == 0 {
                unit = next;
                continue;
            }
            let mut saved = SavedUnit { id: h(O::AeUnitId, u, &[]) as c_ulong, ..SavedUnit::default() };
            for i in 0..list_count(own) {
                let e = list_at(own, i);
                let f = fields(e);
                // SAFETY: the effect's GE2DPoint location
                let [x, y] = unsafe { *(hp(O::AeLocation, e, &[]) as *const [f32; 2]) };
                let morale = f.mKind == EE_AREA_EFFECT_MORALE_FROM_BUILDINGS;
                saved.own.push(EeAreaEffectSavedOwn {
                    mX: x,
                    mY: y,
                    mIsMorale: morale,
                    mDirty: morale && f.mDirty,
                    mCount: if morale { f.mCount } else { 0 },
                });
            }
            for i in 0..list_count(affecting) {
                saved.affecting.push(ref_of(list_at(affecting, i)));
            }
            state.units.push(saved);
            unit = next;
        }
    }
    let manager = hp(O::AeManager, 0, &[]);
    let active = hp(O::AeManagerEffects, manager, &[]);
    for i in 0..list_count(active) {
        state.active.push(ref_of(list_at(active, i)));
    }
    // SAFETY: the record and the file's I/O callback
    unsafe { ee_area_effect_running_save(&state, io(file)) };
}

/// EEAELoadRunningState: the saved running state read, and put back at the loaded game's first tick.
#[no_mangle]
pub extern "C" fn ee_gamecpp_ae_load_running(file: *mut c_void) -> c_long {
    let mut result: c_long = 0;
    // SAFETY: the file's I/O callback; the record is the Rust's
    let state = unsafe { ee_area_effect_running_load(io(file), &mut result) };
    if result == 0 {
        h(O::AeAfterLoad, 0, &[state as usize as i64]);
    } else {
        // SAFETY: the record ee_area_effect_running_load made
        unsafe { ee_area_effect_running_free(state) };
    }
    result
}

/// The running state put back (EEAfterLoad): each unit's own effects' location and morale count (when the unit
/// has the saved number of own effects), each unit's affecting effects in their saved order, the active list;
/// the references no longer found are counted and reported.
///
/// # Safety
/// `state` is the record ee_area_effect_running_load made (the caller keeps it).
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_ae_restore_running(state: *const EeAreaEffectRunning) {
    let s = &*state;
    let mut lost: c_ulong = 0;
    for u in &s.units {
        let unit = complex_of(u.id);
        if unit == 0 {
            lost += 1;
            continue;
        }
        let own = hp(O::AeUnitOwnList, unit, &[]);
        if list_count(own) != u.own.len() as i64 {
            lost += 1;
            continue;
        }
        for (i, saved) in u.own.iter().enumerate() {
            let e = list_at(own, i as i64);
            let location = hp(O::AeLocation, e, &[]) as *mut [f32; 2];
            *location = [saved.mX, saved.mY];
            let f = super::rec::<EeAreaEffectFields>(O::AeFields, e);
            if (*f).mKind == EE_AREA_EFFECT_MORALE_FROM_BUILDINGS && saved.mIsMorale {
                (*f).mDirty = saved.mDirty;
                (*f).mCount = saved.mCount;
            }
        }
    }
    // the effects acting on each unit (every list is empty after a load), in their saved order
    for u in &s.units {
        let unit = complex_of(u.id);
        if unit == 0 {
            continue;
        }
        let affecting = hp(O::AeUnitAffectingList, unit, &[]);
        h(O::AeListDereference, affecting, &[]);
        for r in &u.affecting {
            let e = effect_of(r);
            if e != 0 {
                h(O::AeUnitAffectingAdd, unit, &[e as i64]);
            } else {
                lost += 1;
            }
        }
    }
    let manager = hp(O::AeManager, 0, &[]);
    ee_gamecpp_ae_clear_effects(manager as *mut c_void);
    let active = hp(O::AeManagerEffects, manager, &[]);
    for r in &s.active {
        let e = effect_of(r);
        if e != 0 {
            h(O::AeListAdd, active, &[e as i64]);
        } else {
            lost += 1;
        }
    }
    if lost != 0 {
        h(O::AeLostRefs, 0, &[lost as i64]);
    }
}

/// `kUnitTypeSpaceDock`, `kUnitTypeSpaceTurret`.
const AH_SPACE_DOCK: i64 = 0x2d8;
const AH_SPACE_TURRET: i64 = 0x302;

/// The relation flags of two players (EE_AREA_EFFECT_SAME, _ALLIED, _AT_WAR).
pub fn relation(same: bool, allied: bool, at_war: bool) -> i32 {
    (if same { EE_AREA_EFFECT_SAME } else { 0 })
        | (if allied { EE_AREA_EFFECT_ALLIED } else { 0 })
        | (if at_war { EE_AREA_EFFECT_AT_WAR } else { 0 })
}

/// The world of an area effect's Rust slots (ee_sim tech area_effect.rs's host; was EEAreaEffect.cpp's
/// EEAEHost::Call): `ctx` the EEAEHost (the effect and the unit the slot was given).
///
/// # Safety
/// `call` and `reply` are the slot's live records.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_ae_host(ctx: *mut c_void, call: *const c_void, reply: *mut c_void) {
    let host = ctx as P;
    // SAFETY: the caller's records
    let (c, r) = unsafe { (&*(call as *const EeAreaEffectCall), &mut *(reply as *mut EeAreaEffectReply)) };
    let (ca, cb) = (c.a as i64, c.b as i64);
    match c.op {
        EeAreaEffectOp::TargetInSet => r.ok = h(O::AhTargetInSet, host, &[ca]) != 0,
        EeAreaEffectOp::TargetTechKind4 => {
            let node = hp(O::AhTargetTechNode, host, &[ca]);
            r.ok = node != 0 && h(O::AhNodeKind, host, &[node as i64]) == 4;
        }
        EeAreaEffectOp::Relation => {
            let player = hp(O::AhPlayer, host, &[ca]);
            let other = hp(O::AhPlayer, host, &[cb]);
            let allied = h(O::AhAllied, host, &[player as i64, other as i64]) != 0;
            let at_war = h(O::AhAtWar, host, &[player as i64, other as i64]) != 0;
            r.value = relation(other == player, allied, at_war);
        }
        EeAreaEffectOp::TargetBuilt => r.ok = h(O::AhTargetBuilt, host, &[]) != 0,
        EeAreaEffectOp::TargetRepairable => r.ok = h(O::AhTargetRepairable, host, &[]) != 0,
        EeAreaEffectOp::TargetPosition => {
            let mut position = Ge3DPoint::new(0.0, 0.0, 0.0);
            h(O::AhTargetPosition, host, &[ptr(&mut position)]);
            r.x = position.x;
            r.y = position.y;
        }
        EeAreaEffectOp::Continent => {
            let tile: [c_long; 2] = [c.a as c_long, c.b as c_long];
            r.value = h(O::AhContinent, host, &[cptr(&tile)]) as i32;
        }
        EeAreaEffectOp::TargetCarrierPlanet => {
            let carrier = hp(O::AhTargetCarrier, host, &[]);
            let kind = if carrier != 0 { h(O::AhTargetType, host, &[]) } else { 0 };
            r.ok = carrier != 0 && (kind == AH_SPACE_DOCK || kind == AH_SPACE_TURRET);
            r.value = if r.ok { h(O::AhPlanet, host, &[carrier as i64]) as i32 } else { 0 };
        }
        EeAreaEffectOp::TargetHitPoints => r.value = h(O::AhTargetHitPoints, host, &[]) as i32,
        EeAreaEffectOp::TargetMaxHitPoints => r.value = h(O::AhTargetMaxHitPoints, host, &[]) as i32,
        EeAreaEffectOp::InCinematic => r.ok = h(O::AhInCinematic, host, &[]) != 0,
        EeAreaEffectOp::TargetSetHitPoints => {
            h(O::AhTargetSetHitPoints, host, &[ca]);
        }
        EeAreaEffectOp::TargetCloak => {
            h(O::AhTargetCloak, host, &[]);
        }
        EeAreaEffectOp::TargetUncloak => {
            // the cloaking's RemovedFrom 0x27ab53: a Camouflage civ power keeps an idle or hidden unit cloaked; a
            // visible cloaked enemy leaves the mini map
            if h(O::AhCamouflaged, host, &[]) != 0 && h(O::AhStaysHidden, host, &[]) != 0 {
                return;
            }
            let visible = h(O::AhCloakedEnemy, host, &[]) != 0;
            h(O::AhTargetCloakOwn, host, &[]);
            if visible {
                h(O::AhMiniMapHide, host, &[]);
            }
        }
        EeAreaEffectOp::OwnerScale => {
            let owner = hp(O::AhOwner, host, &[]);
            r.ok = owner != 0;
            r.x = if owner != 0 { hd(O::AhOwnerScale, host, &[owner as i64], &[]) } else { 1.0 };
        }
        EeAreaEffectOp::OwnerHitPoints => {
            let owner = hp(O::AhOwner, host, &[]);
            r.value = h(O::AhOwnerHitPoints, host, &[owner as i64]) as i32;
        }
        EeAreaEffectOp::OwnerLeadership => {
            let owner = hp(O::AhOwner, host, &[]);
            r.x = hd(O::AhOwnerLeadership, host, &[owner as i64], &[]);
        }
        EeAreaEffectOp::OwnerGarrisonBonus => {
            let owner = hp(O::AhOwner, host, &[]);
            r.x = hd(O::AhOwnerGarrisonBonus, host, &[owner as i64], &[]);
        }
        EeAreaEffectOp::CountBuildings => {
            // the morale from buildings' collection 0x27b95b: the rectangle (left a, top b, right c, bottom d) in
            // U2DRectangle<long>'s order, by overlap (e 1) or by range (f, g, x)
            let rect: [c_long; 4] = [c.d as c_long, c.a as c_long, c.c as c_long, c.b as c_long];
            r.value = if c.e == 1 {
                h(O::AhCountInRect, host, &[cptr(&rect)])
            } else {
                hf(O::AhCountInRange, host, &[cptr(&rect), c.f as i64, c.g as i64], &[c.x])
            } as i32;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_collection_rectangle_floors_as_umfloor() {
        // bottom, left, right, top
        assert_eq!(collect_rect(10.5, 20.25, 2.0), [22, 8, 12, 18]);
        assert_eq!(collect_rect(-0.5, 0.5, 1.0), [1, -2, 0, -1]);
        assert_eq!(collect_rect(3.0, 3.0, 0.0), [3, 3, 3, 3]);
    }

    #[test]
    fn a_saved_reference_without_an_owner_is_none() {
        let r = EeAreaEffectSavedRef::default();
        assert_eq!(r.mOwner, c_ulong::MAX);
        assert_eq!(r.mIndex, -1);
        // effect_of answers none before any C++ call
        assert_eq!(effect_of(&r), 0);
    }

    #[test]
    fn the_relation_flags() {
        assert_eq!(relation(true, false, false), 1);
        assert_eq!(relation(false, true, true), 6);
        assert_eq!(relation(false, false, false), 0);
    }
}
