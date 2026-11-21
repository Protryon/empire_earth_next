//! The area effects' own bodies (game core lane, round 10, lane C; was EEAreaEffect.cpp's, EEAreaEffectTemplate.cpp's
//! and EEAEManager.cpp's C++): an effect's construction, release and slots over its Rust kinds (ee_sim tech
//! area_effect.rs, each slot given an [`AeHost`] the slot's world calls go to), the template's construction, media and
//! tests, a unit's area-effect list (EEUnitAreaEffectList) and the manager's tick passes over every player's units.
//! Each C++ call or member is a one-statement case of EEAreaEffect.cpp's `EECore10OpC`; the plain members are read in
//! place through the Rust records (`EeAreaEffectFields`, `EeAreaEffectTemplateFields`).
//!
//! Simulation code (RUST_PLAN 2.1): the lists are walked in the C++'s order, every call made in the C++'s order.

use std::ffi::{c_long, c_ulong, c_void};

use ee_sim::tech::area_effect::{
    ee_area_effect_affects_player, ee_area_effect_applied, ee_area_effect_checksum, ee_area_effect_covers,
    ee_area_effect_init, ee_area_effect_is_within, ee_area_effect_level, ee_area_effect_mark_dirty,
    ee_area_effect_modify, ee_area_effect_radius, ee_area_effect_recount, ee_area_effect_same_group,
    ee_area_effect_scale, ee_area_effect_strength, ee_area_effect_template_init, ee_area_effect_template_reset,
    ee_area_effect_tick, EeAreaEffectCall, EeAreaEffectData, EeAreaEffectFields, EeAreaEffectHostFfi,
    EeAreaEffectReply, EeAreaEffectTemplateFields, EE_AREA_EFFECT_MORALE_FROM_BUILDINGS,
};

use super::{h, hf, hp, pa, rec, Owned, O, P};

/// The world of an area effect's slot (was EEAreaEffect.cpp's EEAEHost; its cases read the first two members as
/// `EEAEHost { EEAreaEffect* mEffect; EEComplexUnit* mTarget; }`): the effect (NULL for a template's test) and the
/// unit the slot was given (NULL when none). `ffi.ctx` points at the host itself.
#[repr(C)]
pub struct AeHost {
    pub effect: P,
    pub target: P,
    pub ffi: EeAreaEffectHostFfi,
}

/// The host's calls, answered by civ.rs's `ee_gamecpp_ae_host`.
unsafe extern "C" fn ae_call(ctx: *mut c_void, call: *const EeAreaEffectCall, reply: *mut EeAreaEffectReply) {
    // SAFETY: the slot's live records
    unsafe { super::civ::ee_gamecpp_ae_host(ctx, call as *const c_void, reply as *mut c_void) }
}

/// Runs `f` with a host of an effect and a target (its `ffi` pointing at itself for the call's duration).
pub fn with_host<R>(effect: P, target: P, f: impl FnOnce(*const EeAreaEffectHostFfi) -> R) -> R {
    let mut host = AeHost { effect, target, ffi: EeAreaEffectHostFfi { ctx: std::ptr::null_mut(), call: ae_call } };
    host.ffi.ctx = &mut host as *mut AeHost as *mut c_void;
    f(&host.ffi)
}

fn fields(e: P) -> *mut EeAreaEffectFields {
    // SAFETY: the effect's base record's address
    unsafe { rec(O::AeFields, e) }
}

fn template_of(e: P) -> *mut EeAreaEffectTemplateFields {
    // SAFETY: the effect's template's base record's address
    unsafe { rec(O::AeTemplateFields, e) }
}

fn tpl_fields(t: P) -> *mut EeAreaEffectTemplateFields {
    // SAFETY: the template's base record's address
    unsafe { rec(O::AeTplFields, t) }
}

fn as_p(x: *mut c_void) -> P {
    x as P
}

// ---- EEAreaEffect

/// EEAreaEffect(template): the template referenced, no unit or player yet (the morale from buildings' cap: the
/// template's maximum level plus the game variant's morale bonus), then the template's media.
#[no_mangle]
pub extern "C" fn ee_gamecpp_aefx_construct(effect: *mut c_void, template: *mut c_void) {
    let (e, t) = (as_p(effect), as_p(template));
    h(O::LcAeSetTemplate, e, &[pa(t)]);
    h(O::LcTplReference, t, &[]);
    h(O::LcAeSetOwner, e, &[0]);
    // SAFETY: the template's record, copied
    let tf = unsafe { *tpl_fields(t) };
    let bonus =
        if tf.mKind == EE_AREA_EFFECT_MORALE_FROM_BUILDINGS { h(O::LcMoraleBonus, 0, &[]) as c_long } else { 0 };
    // SAFETY: the effect's record
    unsafe { ee_area_effect_init(fields(e), tf.mKind, tf.mMaxLevel, bonus) };
    ee_gamecpp_aetpl_media(template, false, false);
}

/// ~EEAreaEffect: the template and the unit let go, then the template's media unloaded.
#[no_mangle]
pub extern "C" fn ee_gamecpp_aefx_destruct(effect: *mut c_void) {
    let e = as_p(effect);
    h(O::LcTplDereference, hp(O::LcAeTemplate, e, &[]), &[]);
    let unit = hp(O::AeOwner, e, &[]);
    if unit != 0 {
        h(O::LcCxDereference, unit, &[]);
    }
    ee_gamecpp_aetpl_media(hp(O::LcAeTemplate, e, &[]) as *mut c_void, true, false);
}

/// AppliedTo / RemovedFrom: the kind's slot on a unit (the cloaking's).
#[no_mangle]
pub extern "C" fn ee_gamecpp_aefx_applied(effect: *mut c_void, unit: *mut c_void, removed: bool) {
    let e = as_p(effect);
    // SAFETY: the effect's record and the host
    with_host(e, as_p(unit), |host| unsafe { ee_area_effect_applied(fields(e), host, removed) });
}

/// Covers: whether the effect, by its kind, covers the unit from a point for a player.
#[no_mangle]
pub extern "C" fn ee_gamecpp_aefx_covers(
    effect: *mut c_void,
    player: c_ulong,
    unit: *mut c_void,
    point: *const c_void,
) -> bool {
    let (e, u) = (as_p(effect), as_p(unit));
    // SAFETY: the caller's GE2DPoint
    let [x, y] = unsafe { *(point as *const [f32; 2]) };
    let owner = h(O::AeUnitPlayerId, u, &[]) as i32;
    // SAFETY: the effect's and template's records and the host
    with_host(e, u, |host| unsafe {
        ee_area_effect_covers(fields(e), template_of(e), host, player as i32, owner, x, y)
    })
}

/// ShowOn: the template's graphic effect played on the unit, if it has one.
#[no_mangle]
pub extern "C" fn ee_gamecpp_aefx_show_on(effect: *mut c_void, unit: *mut c_void) {
    let gfx = hp(O::LcTplGfx, hp(O::LcAeTemplate, as_p(effect), &[]), &[]);
    if gfx != 0 {
        h(O::LcGfxCreateUnitEffect, gfx, &[pa(as_p(unit))]);
    }
}

/// The location's two floats.
fn location(e: P) -> [f32; 2] {
    // SAFETY: the effect's GE2DPoint member
    unsafe { *(hp(O::AeLocation, e, &[]) as *const [f32; 2]) }
}

/// The out-of-sync dump's four lines.
pub fn dump_text(x: f32, y: f32, unit_id: c_ulong, player: c_long) -> String {
    let d = "OUT OF SYNCH DUMP:  Area Effect";
    let mut s = format!("{d} Position Location X: {:6.6}\n", x as f64);
    s += &format!("{d} Position Location Y: {:6.6}\n", y as f64);
    s += &format!("{d} Owning Unit ID #{unit_id:08x}\n");
    s += &format!("{d} Player Number {player}\n");
    s
}

/// Dump: the effect's location, owning unit id (-1 without one) and player in the out-of-sync dump.
#[no_mangle]
pub extern "C" fn ee_gamecpp_aefx_dump(effect: *mut c_void, file: *mut c_void) {
    let e = as_p(effect);
    let [x, y] = location(e);
    let unit = hp(O::AeOwner, e, &[]);
    let id = if unit != 0 { h(O::AeUnitId, unit, &[]) as c_ulong } else { c_ulong::MAX };
    // SAFETY: the effect's record
    let player = unsafe { (*fields(e)).mPlayer };
    let text = std::ffi::CString::new(dump_text(x, y, id, player)).expect("no NUL");
    h(O::FilePuts, as_p(file), &[text.as_ptr() as usize as i64]);
}

/// Checksum: the location's float bits, the player and the owning unit's id (0 without one).
#[no_mangle]
pub extern "C" fn ee_gamecpp_aefx_checksum(effect: *mut c_void) -> c_long {
    let e = as_p(effect);
    let [x, y] = location(e);
    let unit = hp(O::AeOwner, e, &[]);
    let id = if unit != 0 { h(O::AeUnitId, unit, &[]) as c_long } else { 0 };
    // SAFETY: the effect's record
    unsafe { ee_area_effect_checksum(fields(e), x, y, id) }
}

/// GetStrength: by the kind.
#[no_mangle]
pub extern "C" fn ee_gamecpp_aefx_strength(effect: *mut c_void) -> c_long {
    let e = as_p(effect);
    // SAFETY: the records and the host
    with_host(e, 0, |host| unsafe { ee_area_effect_strength(fields(e), template_of(e), host) })
}

/// GetLevel: a morale kind's level.
#[no_mangle]
pub extern "C" fn ee_gamecpp_aefx_level(effect: *mut c_void) -> c_long {
    let e = as_p(effect);
    // SAFETY: the records and the host
    with_host(e, 0, |host| unsafe { ee_area_effect_level(fields(e), template_of(e), host) })
}

/// GetRadius: the template's radius times the effect's scale.
#[no_mangle]
pub extern "C" fn ee_gamecpp_aefx_radius(effect: *mut c_void) -> f32 {
    let e = as_p(effect);
    // SAFETY: the template's record and the host
    with_host(e, 0, |host| unsafe { ee_area_effect_radius(template_of(e), host) })
}

/// GetScale: the owning unit's area-effect scale, 1 without a unit.
#[no_mangle]
pub extern "C" fn ee_gamecpp_aefx_scale(effect: *mut c_void) -> f32 {
    // SAFETY: the host
    with_host(as_p(effect), 0, |host| unsafe { ee_area_effect_scale(host) })
}

/// SetUnit: the owning unit, referenced.
#[no_mangle]
pub extern "C" fn ee_gamecpp_aefx_set_unit(effect: *mut c_void, unit: *mut c_void) {
    let u = as_p(unit);
    h(O::LcAeSetOwner, as_p(effect), &[pa(u)]);
    if u != 0 {
        h(O::LcCxReference, u, &[]);
    }
}

/// Modify: a value of a kind by the effect's kind into `out`; false when it leaves it.
///
/// # Safety
/// `out` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_aefx_modify(
    effect: *mut c_void,
    kind: c_long,
    value: c_long,
    out: *mut c_long,
) -> bool {
    let e = as_p(effect);
    // SAFETY: the records, the host and the caller's out value
    with_host(e, 0, |host| unsafe { ee_area_effect_modify(fields(e), template_of(e), host, kind, value, out) })
}

/// MarkDirty: the morale from buildings marked for a recount.
fn mark_dirty(e: P) {
    // SAFETY: the effect's record
    unsafe { ee_area_effect_mark_dirty(fields(e)) };
}

/// Recount: the morale from buildings' buildings recounted when marked.
#[no_mangle]
pub extern "C" fn ee_gamecpp_aefx_recount(effect: *mut c_void) {
    let e = as_p(effect);
    let [x, y] = location(e);
    // SAFETY: the records and the host
    with_host(e, 0, |host| unsafe { ee_area_effect_recount(fields(e), template_of(e), host, x, y) });
}

/// Tick: the effect on a unit it affects (the healing heals it).
#[no_mangle]
pub extern "C" fn ee_gamecpp_aefx_tick(effect: *mut c_void, unit: *mut c_void, elapsed: f32) {
    let e = as_p(effect);
    // SAFETY: the records and the host
    with_host(e, as_p(unit), |host| unsafe { ee_area_effect_tick(fields(e), template_of(e), host, elapsed) });
}

// ---- EEAreaEffectTemplate

/// EEAreaEffectTemplate(): square, radius 1, allied players, no unit set, white tint; no graphics or media.
#[no_mangle]
pub extern "C" fn ee_gamecpp_aetpl_construct(template: *mut c_void) {
    let t = as_p(template);
    // SAFETY: the template's record
    unsafe { ee_area_effect_template_reset(tpl_fields(t)) };
    h(O::LcTplSetGfx, t, &[0]);
    h(O::LcTplSetGfx18, t, &[0]);
    h(O::LcTplSetMedia, t, &[0]);
}

/// ~EEAreaEffectTemplate: its two graphic effects let go.
#[no_mangle]
pub extern "C" fn ee_gamecpp_aetpl_destruct(template: *mut c_void) {
    let t = as_p(template);
    for op in [O::LcTplGfx, O::LcTplGfx18] {
        let g = hp(op, t, &[]);
        if g != 0 {
            h(O::LcGfxDereference, g, &[]);
        }
    }
}

/// Load (`unload` false) or Unload: both graphic effects, then the media.
#[no_mangle]
pub extern "C" fn ee_gamecpp_aetpl_media(template: *mut c_void, unload: bool, on_demand: bool) {
    let t = as_p(template);
    for op in [O::LcTplGfx, O::LcTplGfx18] {
        let g = hp(op, t, &[]);
        if g != 0 {
            if unload {
                h(O::LcGfxUnload, g, &[]);
            } else {
                h(O::LcGfxLoad, g, &[on_demand as i64]);
            }
        }
    }
    let m = hp(O::LcTplMedia, t, &[]);
    if m != 0 {
        h(if unload { O::LcMediaUnload } else { O::LcMediaLoad }, m, &[]);
    }
}

/// AffectsPlayer (`reversed` false) / AffectsPlayerReversed: the template's player mode.
#[no_mangle]
pub extern "C" fn ee_gamecpp_aetpl_affects(
    template: *mut c_void,
    player: c_long,
    other: c_long,
    reversed: bool,
) -> bool {
    let t = as_p(template);
    // SAFETY: the template's record and the host
    with_host(0, 0, |host| unsafe {
        ee_area_effect_affects_player(tpl_fields(t), host, player as i32, other as i32, reversed)
    })
}

/// IsWithin: a point in the square or circular area around a center (and on its continent; a carried space dock or
/// turret also on its carrier's planet).
#[no_mangle]
pub extern "C" fn ee_gamecpp_aetpl_is_within(
    template: *mut c_void,
    center: *const c_void,
    point: *const c_void,
    scale: f32,
    unit: *mut c_void,
) -> bool {
    let (t, u) = (as_p(template), as_p(unit));
    // SAFETY: the caller's two GE2DPoints
    let ([cx, cy], [px, py]) = unsafe { (*(center as *const [f32; 2]), *(point as *const [f32; 2])) };
    // SAFETY: the template's record and the host
    with_host(0, u, |host| unsafe { ee_area_effect_is_within(tpl_fields(t), host, cx, cy, px, py, scale, u != 0) })
}

/// Init: the members from an area-effect file record, its graphic effects (referenced) and media resolved.
#[no_mangle]
pub extern "C" fn ee_gamecpp_aetpl_init(template: *mut c_void, data: *const c_void) -> bool {
    let t = as_p(template);
    let d = data as *const EeAreaEffectData;
    // SAFETY: the template's record and the caller's record
    let (gfx, gfx18, media) = unsafe {
        ee_area_effect_template_init(tpl_fields(t), d);
        ((*d).mGraphicEffect, (*d).mGraphicEffect18, (*d).mMedia)
    };
    for (index, get, set) in [(gfx, O::LcTplGfx, O::LcTplSetGfx), (gfx18, O::LcTplGfx18, O::LcTplSetGfx18)] {
        if index > 0 {
            h(set, t, &[pa(hp(O::LcGfxGet, 0, &[index as i64]))]);
            let g = hp(get, t, &[]);
            if g != 0 {
                h(O::LcGfxReference, g, &[]);
            }
        }
    }
    let m = if media > 0 { hp(O::LcMediaGet, 0, &[media as i64]) } else { 0 };
    h(O::LcTplSetMedia, t, &[pa(m)]);
    true
}

// ---- EEUnitAreaEffectList (p a UReferenceContainer<EEAreaEffect*>*)

fn count(list: P) -> i64 {
    h(O::AeListCount, list, &[])
}

fn at(list: P, i: i64) -> P {
    hp(O::AeListAt, list, &[i])
}

fn kind_of(e: P) -> c_long {
    // SAFETY: the effect's template's record
    unsafe { (*template_of(e)).mKind }
}

fn add_area_effect(list: P, effect: P) {
    let kind = kind_of(effect);
    let old = (0..count(list)).map(|i| at(list, i)).find(|&o| ee_area_effect_same_group(kind, kind_of(o)));
    if let Some(old) = old {
        let strength = |e: P| ee_gamecpp_aefx_strength(e as *mut c_void) as c_ulong;
        if strength(effect) <= strength(old) {
            return;
        }
        h(O::LcListRemove, list, &[pa(old)]);
    }
    if effect != 0 {
        h(O::AeListAdd, list, &[pa(effect)]);
    }
}

/// AddAreaEffect: the effect, replacing one of the same kind (the morale kinds count as one) whose strength is
/// smaller; nothing when the old one's is at least as large.
#[no_mangle]
pub extern "C" fn ee_gamecpp_aelist_add(list: *mut c_void, effect: *mut c_void) {
    add_area_effect(as_p(list), as_p(effect));
}

/// ApplyModifiers: each effect's Modify of the kind (the damage read anew each time: it may be the out value),
/// showing each that applied on the unit.
///
/// # Safety
/// `damage` is readable and `out` writable (they may be the same value).
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_aelist_modifiers(
    list: *mut c_void,
    kind: c_long,
    damage: *const c_long,
    out: *mut c_long,
    unit: *mut c_void,
) {
    let list = as_p(list);
    let mut i = 0;
    while i < count(list) {
        let e = at(list, i) as *mut c_void;
        // SAFETY: the caller's values
        if unsafe { ee_gamecpp_aefx_modify(e, kind, *damage, out) } {
            ee_gamecpp_aefx_show_on(e, unit);
        }
        i += 1;
    }
}

/// Remove: the first effect of the kind; whether one was found.
#[no_mangle]
pub extern "C" fn ee_gamecpp_aelist_remove(list: *mut c_void, kind: c_long) -> bool {
    let list = as_p(list);
    match (0..count(list)).map(|i| at(list, i)).find(|&e| kind_of(e) == kind) {
        Some(e) => {
            h(O::LcListRemove, list, &[pa(e)]);
            true
        }
        None => false,
    }
}

/// KeepStrongest: the list rebuilt through AddAreaEffect.
#[no_mangle]
pub extern "C" fn ee_gamecpp_aelist_keep_strongest(list: *mut c_void) {
    let list = as_p(list);
    let kept = Owned::new(O::LcListNew, O::LcListDelete);
    for i in 0..count(list) {
        add_area_effect(kept.0, at(list, i));
    }
    h(O::LcListAssign, list, &[pa(kept.0)]);
}

/// MarkDirty: each effect's.
#[no_mangle]
pub extern "C" fn ee_gamecpp_aelist_mark_dirty(list: *mut c_void) {
    let list = as_p(list);
    for i in 0..count(list) {
        mark_dirty(at(list, i));
    }
}

/// MoveAndUpdate: each effect moved to the point and recounted.
#[no_mangle]
pub extern "C" fn ee_gamecpp_aelist_move(list: *mut c_void, point: *const c_void) {
    let list = as_p(list);
    // SAFETY: the caller's GE2DPoint
    let p = unsafe { *(point as *const [f32; 2]) };
    for i in 0..count(list) {
        let e = at(list, i);
        // SAFETY: the effect's GE2DPoint member
        unsafe { *(hp(O::AeLocation, e, &[]) as *mut [f32; 2]) = p };
        ee_gamecpp_aefx_recount(e as *mut c_void);
    }
}

/// Tick: each effect's on the unit.
#[no_mangle]
pub extern "C" fn ee_gamecpp_aelist_tick(list: *mut c_void, unit: *mut c_void, elapsed: f32) {
    let list = as_p(list);
    for i in 0..count(list) {
        ee_gamecpp_aefx_tick(at(list, i) as *mut c_void, unit, elapsed);
    }
}

// ---- EEAEManager's passes (EEForEachPlayersUnit: every player's units but nature's)

/// Runs `f` on the complex unit of each unit of every player but nature (the next unit read after `f`).
pub fn for_each_players_unit(mut f: impl FnMut(P)) {
    let mut p = 1;
    while p < h(O::AeWorldNumPlayers, 0, &[]) {
        let player = hp(O::AeWorldPlayer, 0, &[p]);
        if player != 0 {
            let mut unit = hp(O::AePlFirstUnit, player, &[]);
            while unit != 0 {
                let cx = hp(O::AeUnitComplex, unit, &[]);
                if cx != 0 {
                    f(cx);
                }
                unit = hp(O::AeUnitNext, unit, &[]);
            }
        }
        p += 1;
    }
}

/// EEAEManager::Tick: every unit's affecting effects cleared, each unit's own effects applied to the units around
/// it, the strongest of each kind kept, the generators updated, the active list rebuilt from the built units' own
/// effects, the affecting effects ticked; then the capture's AEFX (TEST ONLY).
#[no_mangle]
pub extern "C" fn ee_gamecpp_aem_tick(manager: *mut c_void, elapsed: f32) {
    for_each_players_unit(|cx| {
        h(O::LcCxRemoveAffecting, cx, &[]);
    });
    for_each_players_unit(|cx| {
        h(O::LcCxApplyAreaEffects, cx, &[]);
    });
    for_each_players_unit(|cx| ee_gamecpp_aelist_keep_strongest(hp(O::AeUnitAffectingList, cx, &[]) as *mut c_void));
    for_each_players_unit(|cx| {
        hf(O::LcCxUpdateAreaEffects, cx, &[], &[elapsed]);
    });
    let effects = hp(O::AeManagerEffects, as_p(manager), &[]) as *mut c_void;
    super::civ::ee_gamecpp_ae_container_empty(effects);
    for_each_players_unit(|cx| super::civ::ee_gamecpp_ae_rebuild_add(effects, cx as *mut c_void));
    for_each_players_unit(|cx| {
        ee_gamecpp_aelist_tick(hp(O::AeUnitAffectingList, cx, &[]) as *mut c_void, cx as *mut c_void, elapsed)
    });
    hf(O::LcAeCaptureTick, 0, &[], &[elapsed]);
}

/// The effect added to every unit it covers (SetAffectedByAreaEffectFunctor over every player's units).
pub fn set_affected_all(effect: P, player: i64, location: i64) {
    for_each_players_unit(|cx| {
        super::civ::ee_gamecpp_ae_set_affected(
            effect as *mut c_void,
            player as c_ulong,
            cx as *mut c_void,
            location as usize as *const c_void,
        )
    });
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_dump_lines_are_the_c_formats() {
        let s = dump_text(1.5, -2.25, 0x2a, 3);
        assert_eq!(
            s,
            "OUT OF SYNCH DUMP:  Area Effect Position Location X: 1.500000\n\
             OUT OF SYNCH DUMP:  Area Effect Position Location Y: -2.250000\n\
             OUT OF SYNCH DUMP:  Area Effect Owning Unit ID #0000002a\n\
             OUT OF SYNCH DUMP:  Area Effect Player Number 3\n"
        );
        assert!(dump_text(0.0, 0.0, c_ulong::MAX, -1).contains("#ffffffff"));
    }

    #[test]
    fn the_host_points_at_itself_and_keeps_the_two_objects_first() {
        let (objects, itself) = with_host(7, 9, |ffi| {
            // SAFETY: the host lives for the closure
            let host = unsafe { &*((*ffi).ctx as *const AeHost) };
            ((host.effect, host.target), std::ptr::eq(&host.ffi, ffi))
        });
        assert_eq!(objects, (7, 9));
        assert!(itself);
        assert_eq!(std::mem::offset_of!(AeHost, target), std::mem::size_of::<usize>());
    }
}
