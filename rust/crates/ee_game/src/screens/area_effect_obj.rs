//! `EEAreaEffect`'s, `EEAreaEffectTemplate`'s and `EEAEManager`'s C++ bodies whole in Rust over one-statement shims
//! (UI lane round 12): the effect's constructor and destructor, its host-asking slots (applied, covers, strength,
//! level, radius, scale, modify, recount, tick, the range test), its graphic effect and owner; the template's
//! constructor, destructor, player and area tests and `Init`; the manager's `Tick` and its unit passes (one C++
//! functor, `EEAEPassFunctor`, whose `Apply` is [`pass`]), `Clear`, `ClearEffects`, `CreateAreaEffect` and the
//! template media by index. The simulation (`ee_sim::tech::area_effect`) asks its world calls of [`Host`], which
//! answers them through `area_effect_host.rs` (the old C++ `EEAEHost` is gone). The C++ objects' numbers are their
//! Rust records (`EeAreaEffectFields`, `EeAreaEffectTemplateFields`), read through the shims' base pointers and
//! copied for each call, as the old forwarders copied them.
#![allow(clippy::unnecessary_cast)]
// c_long and c_ulong are 32 bits on Windows, 64 on LP64

// the forwards are the FFI module's (absent from a test binary)
#![cfg_attr(test, allow(dead_code))]

use core::ffi::c_long;

use ee_base::math::types::Ge2DPoint;
use ee_sim::tech::area_effect::{
    self as ae, AreaEffectHost, EeAreaEffectCall, EeAreaEffectData, EeAreaEffectFields, EeAreaEffectReply,
    EeAreaEffectTemplateFields, EE_AREA_EFFECT_MORALE_FROM_BUILDINGS,
};

use crate::screens::area_effect_lists;
use crate::screens::game_cpp::Cpp;

/// The world calls of one slot: `effect` the `EEAreaEffect*` (0 for a template's test), `target` the
/// `EEComplexUnit*` it was given (0 for none).
pub(crate) struct Host<'a, C: Cpp + ?Sized> {
    pub c: &'a mut C,
    pub effect: usize,
    pub target: usize,
}

impl<C: Cpp + ?Sized> AreaEffectHost for Host<'_, C> {
    fn call(&mut self, call: &EeAreaEffectCall) -> EeAreaEffectReply {
        let mut reply = EeAreaEffectReply::default();
        crate::screens::area_effect_host::host(self.c, self.effect, self.target, call, &mut reply);
        reply
    }
}

/// The effect's numbers (a copy).
fn fields<C: Cpp + ?Sized>(c: &mut C, effect: usize) -> EeAreaEffectFields {
    let p = c.ae_fields(effect) as *const EeAreaEffectFields;
    // SAFETY: the live effect's record (the shim's base pointer)
    unsafe { *p }
}

/// The template's numbers (a copy).
fn tfields<C: Cpp + ?Sized>(c: &mut C, t: usize) -> EeAreaEffectTemplateFields {
    let p = c.ae_template_fields(t) as *const EeAreaEffectTemplateFields;
    // SAFETY: the live template's record
    unsafe { *p }
}

/// The effect's template's numbers.
fn template_of<C: Cpp + ?Sized>(c: &mut C, effect: usize) -> EeAreaEffectTemplateFields {
    let t = c.ae_template(effect);
    tfields(c, t)
}

/// The effect's location.
fn location<C: Cpp + ?Sized>(c: &mut C, effect: usize) -> Ge2DPoint {
    Ge2DPoint { x: c.aes_effect_x(effect), y: c.aes_effect_y(effect) }
}

/// `EEAreaEffect(template)` 0x27bf4b: the template referenced, no owner, the kind's numbers (the morale from
/// buildings' cap: the template's maximum level plus the game variant's morale bonus), the media loaded.
pub(crate) fn construct<C: Cpp + ?Sized>(c: &mut C, effect: usize, template: usize) {
    c.ae_set_template(effect, template);
    c.ae_template_reference(template);
    c.ae_set_unit(effect, 0);
    let t = tfields(c, template);
    let bonus = if t.mKind == EE_AREA_EFFECT_MORALE_FROM_BUILDINGS { c.ae_morale_bonus() as c_long } else { 0 };
    let p = c.ae_fields(effect) as *mut EeAreaEffectFields;
    // SAFETY: the live effect's record; no C++ call between the read and the write
    unsafe { *p = ae::init(t.mKind, t.mMaxLevel, bonus) };
    area_effect_lists::template_load(c, template, false);
}

/// `~EEAreaEffect`: the template and the owner dereferenced, the media unloaded (in the original's order).
pub(crate) fn destroy<C: Cpp + ?Sized>(c: &mut C, effect: usize) {
    let template = c.ae_template(effect);
    c.ae_template_dereference(template);
    let owner = c.aeh_owner(effect);
    if owner != 0 {
        c.ae_unit_dereference(owner);
    }
    let template = c.ae_template(effect);
    area_effect_lists::template_unload(c, template);
}

/// `AppliedTo(unit)` (`removed`: `RemovedFrom(unit)`).
pub(crate) fn applied<C: Cpp + ?Sized>(c: &mut C, effect: usize, unit: usize, removed: bool) {
    let e = fields(c, effect);
    let mut h = Host { c, effect, target: unit };
    if removed {
        ae::removed_from(&mut h, &e);
    } else {
        ae::applied_to(&mut h, &e);
    }
}

/// `Covers(player, unit, point)`: the player as the original's `(int32_t)` of an unsigned long.
pub(crate) fn covers<C: Cpp + ?Sized>(c: &mut C, effect: usize, player: u64, unit: usize, x: f32, y: f32) -> bool {
    let e = fields(c, effect);
    let t = template_of(c, effect);
    let owner = c.ae_unit_player_id(unit);
    let mut h = Host { c, effect, target: unit };
    ae::covers(&mut h, &e, &t, player as i32, owner, Ge2DPoint { x, y })
}

/// `GetStrength`.
pub(crate) fn strength<C: Cpp + ?Sized>(c: &mut C, effect: usize) -> c_long {
    let e = fields(c, effect);
    let t = template_of(c, effect);
    ae::strength(&mut Host { c, effect, target: 0 }, &e, &t)
}

/// `GetLevel`.
pub(crate) fn level<C: Cpp + ?Sized>(c: &mut C, effect: usize) -> c_long {
    let e = fields(c, effect);
    let t = template_of(c, effect);
    ae::level(&mut Host { c, effect, target: 0 }, &e, &t)
}

/// `GetRadius`.
pub(crate) fn radius<C: Cpp + ?Sized>(c: &mut C, effect: usize) -> f32 {
    let t = template_of(c, effect);
    ae::radius(&mut Host { c, effect, target: 0 }, &t)
}

/// `GetScale`.
pub(crate) fn scale<C: Cpp + ?Sized>(c: &mut C, effect: usize) -> f32 {
    ae::scale(&mut Host { c, effect, target: 0 })
}

/// `Modify(kind, value, out)`: the modified value, None when the kind leaves it.
pub(crate) fn modify<C: Cpp + ?Sized>(c: &mut C, effect: usize, kind: c_long, value: c_long) -> Option<c_long> {
    let e = fields(c, effect);
    let t = template_of(c, effect);
    ae::modify(&mut Host { c, effect, target: 0 }, &e, &t, kind, value)
}

/// `MarkDirty`.
pub(crate) fn mark_dirty<C: Cpp + ?Sized>(c: &mut C, effect: usize) {
    let p = c.ae_fields(effect) as *mut EeAreaEffectFields;
    // SAFETY: the live effect's record
    ae::mark_dirty(unsafe { &mut *p });
}

/// `Recount(arg)` (the argument unread): the record recounted on a copy, then written back.
pub(crate) fn recount<C: Cpp + ?Sized>(c: &mut C, effect: usize) {
    let mut e = fields(c, effect);
    let t = template_of(c, effect);
    let at = location(c, effect);
    ae::recount(&mut Host { c: &mut *c, effect, target: 0 }, &mut e, &t, at);
    let p = c.ae_fields(effect) as *mut EeAreaEffectFields;
    // SAFETY: the live effect's record, after the host's calls
    unsafe { *p = e };
}

/// `Tick(unit, elapsed)`.
pub(crate) fn tick<C: Cpp + ?Sized>(c: &mut C, effect: usize, unit: usize, elapsed: f32) {
    let e = fields(c, effect);
    let t = template_of(c, effect);
    ae::tick(&mut Host { c, effect, target: unit }, &e, &t, elapsed);
}

/// `ShowOn(unit)` 0x27bff1: the template's graphic effect on the unit, when it has one.
pub(crate) fn show_on<C: Cpp + ?Sized>(c: &mut C, effect: usize, unit: usize) {
    let template = c.ae_template(effect);
    let graphic = c.ael_t_effect(template);
    if graphic != 0 {
        c.ae_create_unit_effect(unit, graphic);
    }
}

/// `SetUnit(unit)` 0x27c0cb: the owner, referenced.
pub(crate) fn set_unit<C: Cpp + ?Sized>(c: &mut C, effect: usize, unit: usize) {
    c.ae_set_unit(effect, unit);
    if unit != 0 {
        c.ae_unit_reference(unit);
    }
}

/// `IsInRange(point)` 0x27b480: the template's test of the point around the location, at the effect's scale.
pub(crate) fn in_range<C: Cpp + ?Sized>(c: &mut C, effect: usize, x: f32, y: f32) -> bool {
    let template = c.ae_template(effect);
    let at = location(c, effect);
    let s = scale(c, effect);
    template_is_within(c, template, Ge2DPoint { x, y }, at, s, 0)
}

/// `EEAreaEffectTemplate()`: the numbers reset, no graphic effects or media.
pub(crate) fn template_construct<C: Cpp + ?Sized>(c: &mut C, t: usize) {
    let p = c.ae_template_fields(t) as *mut EeAreaEffectTemplateFields;
    // SAFETY: the live template's record
    unsafe { *p = ae::template_reset() };
    c.aet_set_effect(t, 0);
    c.aet_set_effect18(t, 0);
    c.aet_set_media(t, 0);
}

/// `~EEAreaEffectTemplate` 0x27c199: both graphic effects dereferenced.
pub(crate) fn template_destroy<C: Cpp + ?Sized>(c: &mut C, t: usize) {
    let effect = c.ael_t_effect(t);
    if effect != 0 {
        c.ae_graphic_effect_dereference(effect);
    }
    let effect = c.ael_t_effect18(t);
    if effect != 0 {
        c.ae_graphic_effect_dereference(effect);
    }
}

/// `AffectsPlayer(player, other)` (`reversed`: `AffectsPlayerReversed`).
pub(crate) fn template_affects<C: Cpp + ?Sized>(c: &mut C, t: usize, player: i32, other: i32, reversed: bool) -> bool {
    let tf = tfields(c, t);
    let mut h = Host { c, effect: 0, target: 0 };
    if reversed {
        ae::affects_player_reversed(&mut h, &tf, player, other)
    } else {
        ae::affects_player(&mut h, &tf, player, other)
    }
}

/// `IsWithin(center, point, scale, unit)`.
pub(crate) fn template_is_within<C: Cpp + ?Sized>(
    c: &mut C,
    t: usize,
    center: Ge2DPoint,
    point: Ge2DPoint,
    scale: f32,
    unit: usize,
) -> bool {
    let tf = tfields(c, t);
    ae::is_within(&mut Host { c, effect: 0, target: unit }, &tf, center, point, scale, unit != 0)
}

/// `Init(data)` 0x27c489: the numbers from the record, the graphic effects (referenced) and the media by index.
pub(crate) fn template_init<C: Cpp + ?Sized>(c: &mut C, t: usize, d: &EeAreaEffectData) -> bool {
    let p = c.ae_template_fields(t) as *mut EeAreaEffectTemplateFields;
    // SAFETY: the live template's record
    unsafe { ae::template_init(&mut *p, d) };
    if d.mGraphicEffect > 0 {
        let effect = c.ae_graphic_effect_get(d.mGraphicEffect);
        c.aet_set_effect(t, effect);
        if effect != 0 {
            c.ae_graphic_effect_reference(effect);
        }
    }
    if d.mGraphicEffect18 > 0 {
        let effect = c.ae_graphic_effect_get(d.mGraphicEffect18);
        c.aet_set_effect18(t, effect);
        if effect != 0 {
            c.ae_graphic_effect_reference(effect);
        }
    }
    let media = if d.mMedia > 0 { c.ae_media_get(d.mMedia) } else { 0 };
    c.aet_set_media(t, media);
    true
}

/// The manager's unit passes (`EEAEPassFunctor`'s pass): the original's functors, by their RTTI names.
pub const EE_SCREENS_AEM_PASS_APPLY_GENERATED: u32 = 0; // ApplyGeneratedEffectsToOthersFunctor
pub const EE_SCREENS_AEM_PASS_SET_AFFECTED: u32 = 1; // SetAffectedByAreaEffectFunctor
pub const EE_SCREENS_AEM_PASS_CLEAR: u32 = 2; // ClearAreaEffectsFunctor
pub const EE_SCREENS_AEM_PASS_REBUILD: u32 = 3; // RebuildAreaEffectRegionListFunctor
pub const EE_SCREENS_AEM_PASS_REMOVE_REDUNDANT: u32 = 4; // RemoveRedundantAreaEffectsFunctor
pub const EE_SCREENS_AEM_PASS_UPDATE_GENERATED: u32 = 5; // UpdateGeneratedAreaEffectsFunctor
pub const EE_SCREENS_AEM_PASS_UPDATE_AFFECTED: u32 = 6; // UpdateAffectedByAreaEffectsFunctor

/// One unit of a pass (`EEAEPassFunctor::Apply`): `arg` the effect (set affected) or the manager (rebuild),
/// `player`, `x`, `y` the set-affected pass's, `elapsed` the updates'.
#[allow(clippy::too_many_arguments)]
pub(crate) fn pass<C: Cpp + ?Sized>(
    c: &mut C,
    which: u32,
    arg: usize,
    player: u64,
    elapsed: f32,
    x: f32,
    y: f32,
    unit: usize,
) {
    match which {
        EE_SCREENS_AEM_PASS_APPLY_GENERATED => c.aem_apply_area_effects(unit),
        EE_SCREENS_AEM_PASS_SET_AFFECTED => {
            if c.aem_covers(arg, player, unit, x, y) {
                c.aem_affecting_add(unit, arg);
                c.aem_applied_to(arg, unit);
            }
        }
        EE_SCREENS_AEM_PASS_CLEAR => c.aem_remove_affecting(unit),
        EE_SCREENS_AEM_PASS_REBUILD => {
            if !c.aeh_built(unit) {
                return;
            }
            let mut i = 0;
            while i < c.aes_own_count(unit) {
                let effect = c.aes_own_at(unit, i);
                c.aem_effects_add(arg, effect);
                i += 1;
            }
        }
        EE_SCREENS_AEM_PASS_REMOVE_REDUNDANT => {
            let list = c.aem_affecting_list(unit);
            area_effect_lists::keep_strongest(c, list);
        }
        EE_SCREENS_AEM_PASS_UPDATE_GENERATED => c.aem_update_area_effects(unit, elapsed),
        EE_SCREENS_AEM_PASS_UPDATE_AFFECTED => {
            let list = c.aem_affecting_list(unit);
            area_effect_lists::tick(c, list, unit, elapsed);
        }
        _ => {}
    }
}

/// `ClearEffects` 0x27b064: the active list released and emptied.
pub(crate) fn clear_effects<C: Cpp + ?Sized>(c: &mut C, manager: usize) {
    c.aem_effects_release(manager);
    c.aem_effects_erase(manager);
}

/// `Tick(elapsed)` 0x27b634: the passes in the original's order (clear, apply, keep the strongest, update the
/// generators, rebuild the active list, tick the effects), then the capture's record.
pub(crate) fn manager_tick<C: Cpp + ?Sized>(c: &mut C, manager: usize, elapsed: f32) {
    c.aem_for_each(EE_SCREENS_AEM_PASS_CLEAR, 0, 0, 0.0, 0.0, 0.0);
    c.aem_for_each(EE_SCREENS_AEM_PASS_APPLY_GENERATED, 0, 0, 0.0, 0.0, 0.0);
    c.aem_for_each(EE_SCREENS_AEM_PASS_REMOVE_REDUNDANT, 0, 0, 0.0, 0.0, 0.0);
    c.aem_for_each(EE_SCREENS_AEM_PASS_UPDATE_GENERATED, 0, 0, elapsed, 0.0, 0.0);
    clear_effects(c, manager);
    c.aem_for_each(EE_SCREENS_AEM_PASS_REBUILD, manager, 0, 0.0, 0.0, 0.0);
    c.aem_for_each(EE_SCREENS_AEM_PASS_UPDATE_AFFECTED, 0, 0, elapsed, 0.0, 0.0);
    #[cfg(not(test))]
    crate::screens::area_effect_ffi::ee_screens_ae_capture_tick(elapsed);
}

/// `Clear` 0x27b5d1: every template dereferenced, the templates and the active list emptied.
pub(crate) fn manager_clear<C: Cpp + ?Sized>(c: &mut C, manager: usize) {
    let mut i = 0;
    while i < c.aem_templates_count(manager) {
        let t = c.aem_template_at(manager, i);
        if t != 0 {
            c.ae_template_dereference(t);
        }
        i += 1;
    }
    c.aem_templates_clear(manager);
    clear_effects(c, manager);
}

/// `CreateAreaEffect(index, unit)` 0x27b16e: an effect of template `index`'s kind owned by the unit, of its player;
/// 0 past the templates or for an unknown kind.
pub(crate) fn create<C: Cpp + ?Sized>(c: &mut C, manager: usize, index: u64, unit: usize) -> usize {
    if index >= c.aem_templates_count(manager) as u64 {
        return 0;
    }
    let t = c.aem_template_at(manager, index as usize);
    let kind = tfields(c, t).mKind;
    if !ae::known_kind(kind) {
        return 0;
    }
    let effect = c.aem_effect_new(t);
    if effect != 0 {
        set_unit(c, effect, unit);
        c.aem_set_effect_player(effect, unit);
    }
    effect
}

/// `LoadTemplateMedia(index)` 0x27b54b / `UnloadTemplateMedia(index)` 0x27b60b (`load` false): template `index`'s
/// media, when it has a template.
pub(crate) fn template_media<C: Cpp + ?Sized>(c: &mut C, manager: usize, index: u64, load: bool) {
    if index >= c.aem_templates_count(manager) as u64 {
        return;
    }
    let t = c.aem_template_at(manager, index as usize);
    if t == 0 {
        return;
    }
    if load {
        area_effect_lists::template_load(c, t, false);
    } else {
        area_effect_lists::template_unload(c, t);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::screens::game_cpp::Fake;
    use ee_sim::tech::area_effect::{EE_AREA_EFFECT_HEALING, EE_AREA_EFFECT_MORALE};

    fn template(kind: c_long) -> EeAreaEffectTemplateFields {
        EeAreaEffectTemplateFields {
            mKind: kind,
            mMaxLevel: 4,
            mLevel: 3,
            mRadius: 2.0,
            mMode: 4,
            ..Default::default()
        }
    }

    #[test]
    fn construction_takes_the_template_and_the_variants_bonus() {
        let t = template(EE_AREA_EFFECT_MORALE_FROM_BUILDINGS);
        let mut e = EeAreaEffectFields::default();
        let mut fake = Fake::default();
        fake.answer("ae_template_fields", &[&t as *const _ as i64]).answer("ae_morale_bonus", &[2]);
        fake.answer("ae_fields", &[&mut e as *mut _ as i64]);
        construct(&mut fake, 10, 20);
        assert_eq!(e, ae::init(EE_AREA_EFFECT_MORALE_FROM_BUILDINGS, 4, 2));
        let calls = fake.calls();
        assert!(calls.starts_with("ae_set_template(10, 20)\nae_template_reference(20)\nae_set_unit(10, 0)\n"));
        assert!(calls.ends_with("ael_t_effect(20)\nael_t_effect18(20)\nael_t_media(20)"), "{calls}");
    }

    #[test]
    fn another_kind_has_no_bonus_and_destruction_keeps_the_order() {
        let t = template(EE_AREA_EFFECT_HEALING);
        let mut e = EeAreaEffectFields::default();
        let mut fake = Fake::default();
        fake.answer("ae_template_fields", &[&t as *const _ as i64]);
        fake.answer("ae_fields", &[&mut e as *mut _ as i64]);
        construct(&mut fake, 10, 20);
        assert!(!fake.calls().contains("ae_morale_bonus"));
        let mut fake = Fake::default();
        fake.answer("ae_template", &[20, 20]).answer("aeh_owner", &[30]);
        destroy(&mut fake, 10);
        assert!(fake.calls().starts_with(
            "ae_template(10)\nae_template_dereference(20)\naeh_owner(10)\nae_unit_dereference(30)\nae_template(10)\n"
        ));
    }

    #[test]
    fn the_strength_of_a_kind_reads_the_records() {
        let t = template(EE_AREA_EFFECT_MORALE);
        let e = ae::init(EE_AREA_EFFECT_MORALE, 4, 0);
        let mut fake = Fake::default();
        fake.answer("ae_fields", &[&e as *const _ as i64]).answer("ae_template", &[20]);
        fake.answer("ae_template_fields", &[&t as *const _ as i64]);
        let want = ae::strength(&mut Host { c: &mut Fake::default(), effect: 10, target: 0 }, &e, &t);
        assert_eq!(strength(&mut fake, 10), want);
    }

    #[test]
    fn the_unit_and_the_graphic_effect() {
        let mut fake = Fake::default();
        set_unit(&mut fake, 10, 0);
        set_unit(&mut fake, 10, 30);
        fake.answer("ae_template", &[20, 20]).answer("ael_t_effect", &[0, 40]);
        show_on(&mut fake, 10, 30);
        show_on(&mut fake, 10, 30);
        assert_eq!(
            fake.calls(),
            "ae_set_unit(10, 0)\nae_set_unit(10, 30)\nae_unit_reference(30)\nae_template(10)\nael_t_effect(20)\n\
             ae_template(10)\nael_t_effect(20)\nae_create_unit_effect(30, 40)"
        );
    }

    #[test]
    fn init_takes_the_records_effects_and_media() {
        let mut t = EeAreaEffectTemplateFields::default();
        let mut d = EeAreaEffectData {
            mKind: EE_AREA_EFFECT_HEALING as i32,
            mGraphicEffect18: 5,
            mMedia: 7,
            ..Default::default()
        };
        d.mRadius = 3.0;
        let mut fake = Fake::default();
        fake.answer("ae_template_fields", &[&mut t as *mut _ as i64]).answer("ae_graphic_effect_get", &[0]);
        fake.answer("ae_media_get", &[70]);
        assert!(template_init(&mut fake, 20, &d));
        assert_eq!(t.mRadius, 3.0);
        assert!(fake
            .calls()
            .ends_with("ae_graphic_effect_get(5)\naet_set_effect18(20, 0)\nae_media_get(7)\naet_set_media(20, 70)"));
        let mut fake = Fake::default();
        fake.answer("ael_t_effect", &[40]);
        template_destroy(&mut fake, 20);
        assert_eq!(fake.calls(), "ael_t_effect(20)\nae_graphic_effect_dereference(40)\nael_t_effect18(20)");
    }

    #[test]
    fn the_tick_runs_the_passes_in_order() {
        let mut fake = Fake::default();
        manager_tick(&mut fake, 9, 2000.0);
        assert_eq!(
            fake.calls(),
            "aem_for_each(2, 0, 0, 0.0, 0.0, 0.0)\naem_for_each(0, 0, 0, 0.0, 0.0, 0.0)\n\
             aem_for_each(4, 0, 0, 0.0, 0.0, 0.0)\naem_for_each(5, 0, 0, 2000.0, 0.0, 0.0)\n\
             aem_effects_release(9)\naem_effects_erase(9)\naem_for_each(3, 9, 0, 0.0, 0.0, 0.0)\n\
             aem_for_each(6, 0, 0, 2000.0, 0.0, 0.0)"
        );
    }

    #[test]
    fn the_passes_units() {
        let mut fake = Fake::default();
        fake.answer("aem_covers", &[0, 1]);
        pass(&mut fake, EE_SCREENS_AEM_PASS_SET_AFFECTED, 40, 2, 0.0, 1.0, 2.0, 30);
        pass(&mut fake, EE_SCREENS_AEM_PASS_SET_AFFECTED, 40, 2, 0.0, 1.0, 2.0, 31);
        assert_eq!(
            fake.calls(),
            "aem_covers(40, 2, 30, 1.0, 2.0)\naem_covers(40, 2, 31, 1.0, 2.0)\naem_affecting_add(31, 40)\naem_applied_to(40, 31)"
        );
        let mut fake = Fake::default();
        fake.answer("aeh_built", &[0, 1]).answer("aes_own_count", &[1, 1]).answer("aes_own_at", &[40]);
        pass(&mut fake, EE_SCREENS_AEM_PASS_REBUILD, 9, 0, 0.0, 0.0, 0.0, 30);
        pass(&mut fake, EE_SCREENS_AEM_PASS_REBUILD, 9, 0, 0.0, 0.0, 0.0, 31);
        assert_eq!(
            fake.calls(),
            "aeh_built(30)\naeh_built(31)\naes_own_count(31)\naes_own_at(31, 0)\naem_effects_add(9, 40)\naes_own_count(31)"
        );
        let mut fake = Fake::default();
        pass(&mut fake, EE_SCREENS_AEM_PASS_UPDATE_GENERATED, 0, 0, 1500.0, 0.0, 0.0, 30);
        pass(&mut fake, EE_SCREENS_AEM_PASS_CLEAR, 0, 0, 0.0, 0.0, 0.0, 30);
        pass(&mut fake, EE_SCREENS_AEM_PASS_APPLY_GENERATED, 0, 0, 0.0, 0.0, 0.0, 30);
        assert_eq!(
            fake.calls(),
            "aem_update_area_effects(30, 1500.0)\naem_remove_affecting(30)\naem_apply_area_effects(30)"
        );
    }

    #[test]
    fn create_and_clear_and_the_media_by_index() {
        let t = template(EE_AREA_EFFECT_HEALING);
        let bad = template(99);
        let mut fake = Fake::default();
        fake.answer("aem_templates_count", &[2, 2, 2]).answer("aem_template_at", &[20, 21]);
        fake.answer("ae_template_fields", &[&t as *const _ as i64, &bad as *const _ as i64]);
        fake.answer("aem_effect_new", &[50]);
        assert_eq!(create(&mut fake, 9, 0, 30), 50);
        assert_eq!(create(&mut fake, 9, 1, 30), 0);
        assert_eq!(create(&mut fake, 9, 2, 30), 0);
        assert!(fake
            .calls()
            .contains("aem_effect_new(20)\nae_set_unit(50, 30)\nae_unit_reference(30)\naem_set_effect_player(50, 30)"));
        let mut fake = Fake::default();
        fake.answer("aem_templates_count", &[2, 2, 2]).answer("aem_template_at", &[0, 21]);
        manager_clear(&mut fake, 9);
        assert!(fake.calls().ends_with(
            "aem_template_at(9, 1)\nae_template_dereference(21)\naem_templates_count(9)\naem_templates_clear(9)\naem_effects_release(9)\naem_effects_erase(9)"
        ));
        let mut fake = Fake::default();
        fake.answer("aem_templates_count", &[1, 1]).answer("aem_template_at", &[0]);
        template_media(&mut fake, 9, 1, true);
        template_media(&mut fake, 9, 0, false);
        assert_eq!(fake.calls(), "aem_templates_count(9)\naem_templates_count(9)\naem_template_at(9, 0)");
    }
}
