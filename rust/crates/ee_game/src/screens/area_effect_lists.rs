//! `EEAreaEffectTemplate.cpp`'s logic in Rust over one-statement shims (UI lane round 11): the template's media
//! load and unload, and a unit's area-effect list members (`EEUnitAreaEffectList`: the strongest of a group kept,
//! the modifiers, the removal by kind, the recount marks, the move, the tick), in the original's order; the lists
//! are walked by index with the count asked afresh each step (the original's `!= end()`).

use core::ffi::{c_long, c_ulong};

use ee_sim::tech::area_effect::same_group;

use crate::screens::game_cpp::Cpp;

/// The template's graphic effects' and sound media's load.
pub(crate) fn template_load<C: Cpp + ?Sized>(c: &mut C, t: usize, on_demand: bool) {
    let effect = c.ael_t_effect(t);
    if effect != 0 {
        c.ael_effect_load(effect, on_demand);
    }
    let effect = c.ael_t_effect18(t);
    if effect != 0 {
        c.ael_effect_load(effect, on_demand);
    }
    let media = c.ael_t_media(t);
    if media != 0 {
        c.ael_media_load(media);
    }
}

/// The template's graphic effects' and sound media's unload.
pub(crate) fn template_unload<C: Cpp + ?Sized>(c: &mut C, t: usize) {
    let effect = c.ael_t_effect(t);
    if effect != 0 {
        c.ael_effect_unload(effect);
    }
    let effect = c.ael_t_effect18(t);
    if effect != 0 {
        c.ael_effect_unload(effect);
    }
    let media = c.ael_t_media(t);
    if media != 0 {
        c.ael_media_unload(media);
    }
}

/// `AddAreaEffect`: unless the list holds an effect of the same group at least as strong (as unsigned longs), the
/// effect added in place of a weaker one.
pub(crate) fn add_area_effect<C: Cpp + ?Sized>(c: &mut C, list: usize, effect: usize) {
    let kind = c.aem_effect_kind(effect) as c_long;
    let mut i = 0;
    let mut found = 0;
    while i < c.ael_count(list) {
        let other = c.ael_at(list, i);
        if same_group(kind, c.aem_effect_kind(other) as c_long) {
            found = other;
            break;
        }
        i += 1;
    }
    if found != 0 {
        let strength = c.ael_strength(effect) as c_long as c_ulong;
        if strength <= c.ael_strength(found) as c_long as c_ulong {
            return;
        }
        c.ael_remove(list, found);
    }
    if effect != 0 {
        c.ael_add(list, effect);
    }
}

/// `ApplyModifiers(kind, value, out, unit)`: each effect's `Modify` (the value read afresh: `value` and `out` are the
/// caller's longs, maybe the same), showing each that applied on the unit.
pub(crate) fn apply_modifiers<C: Cpp + ?Sized>(
    c: &mut C,
    list: usize,
    kind: i64,
    value: usize,
    out: usize,
    unit: usize,
) {
    let mut i = 0;
    while i < c.ael_count(list) {
        let effect = c.ael_at(list, i);
        if c.ael_modify(effect, kind, value, out) {
            c.ael_show_on(effect, unit);
        }
        i += 1;
    }
}

/// `Remove(kind)`: the first effect of the kind removed; whether there was one.
pub(crate) fn remove<C: Cpp + ?Sized>(c: &mut C, list: usize, kind: i64) -> bool {
    let mut i = 0;
    while i < c.ael_count(list) {
        let effect = c.ael_at(list, i);
        if c.aem_effect_kind(effect) == kind {
            c.ael_remove(list, effect);
            return true;
        }
        i += 1;
    }
    false
}

/// `MarkDirty`, `MoveAndUpdate(x, y, value)` and `Tick(unit, elapsed)` over every effect.
pub(crate) fn mark_dirty<C: Cpp + ?Sized>(c: &mut C, list: usize) {
    let mut i = 0;
    while i < c.ael_count(list) {
        let effect = c.ael_at(list, i);
        c.ael_mark_dirty(effect);
        i += 1;
    }
}

pub(crate) fn move_and_update<C: Cpp + ?Sized>(c: &mut C, list: usize, x: f32, y: f32, value: f32) {
    let mut i = 0;
    while i < c.ael_count(list) {
        let effect = c.ael_at(list, i);
        c.ael_set_location(effect, x, y);
        c.ael_recount(effect, value);
        i += 1;
    }
}

pub(crate) fn tick<C: Cpp + ?Sized>(c: &mut C, list: usize, unit: usize, elapsed: f32) {
    let mut i = 0;
    while i < c.ael_count(list) {
        let effect = c.ael_at(list, i);
        c.ael_tick(effect, unit, elapsed);
        i += 1;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::screens::game_cpp::Fake;
    use ee_sim::tech::area_effect::{
        EE_AREA_EFFECT_HEALING, EE_AREA_EFFECT_MORALE, EE_AREA_EFFECT_MORALE_FROM_BUILDINGS,
    };

    #[test]
    fn a_weaker_effect_of_the_group_is_replaced() {
        let mut fake = Fake::default();
        fake.answer("aem_effect_kind", &[EE_AREA_EFFECT_MORALE as i64, EE_AREA_EFFECT_HEALING as i64]);
        fake.answer("aem_effect_kind", &[EE_AREA_EFFECT_MORALE_FROM_BUILDINGS as i64]);
        fake.answer("ael_count", &[2, 2]).answer("ael_at", &[40, 41]).answer("ael_strength", &[5, 3]);
        add_area_effect(&mut fake, 1, 50);
        assert!(fake.calls().ends_with("ael_strength(50)\nael_strength(41)\nael_remove(1, 41)\nael_add(1, 50)"));
    }

    #[test]
    fn an_equal_or_stronger_one_stays() {
        let mut fake = Fake::default();
        fake.answer("aem_effect_kind", &[EE_AREA_EFFECT_HEALING as i64; 2]).answer("ael_count", &[1]);
        fake.answer("ael_at", &[40]).answer("ael_strength", &[-1, 3]);
        add_area_effect(&mut fake, 1, 50);
        // -1 as an unsigned long is the largest: it replaces
        assert!(fake.calls().ends_with("ael_add(1, 50)"));
        let mut fake = Fake::default();
        fake.answer("aem_effect_kind", &[EE_AREA_EFFECT_HEALING as i64; 2]).answer("ael_count", &[1]);
        fake.answer("ael_at", &[40]).answer("ael_strength", &[3, 3]);
        add_area_effect(&mut fake, 1, 50);
        assert!(fake.calls().ends_with("ael_strength(40)"));
    }

    #[test]
    fn the_modifiers_show_where_they_applied_and_remove_finds_the_kind() {
        let mut fake = Fake::default();
        fake.answer("ael_count", &[2, 2, 2]).answer("ael_at", &[40, 41]).answer("ael_modify", &[0, 1]);
        apply_modifiers(&mut fake, 1, 2, 100, 200, 9);
        assert!(fake.calls().ends_with("ael_modify(41, 2, 100, 200)\nael_show_on(41, 9)\nael_count(1)"));
        let mut fake = Fake::default();
        fake.answer("ael_count", &[1]).answer("ael_at", &[40]).answer("aem_effect_kind", &[3]);
        assert!(remove(&mut fake, 1, 3));
        assert!(!remove(&mut fake, 1, 3));
    }

    #[test]
    fn the_template_media_skip_the_missing() {
        let mut fake = Fake::default();
        fake.answer("ael_t_effect18", &[7]);
        template_load(&mut fake, 1, true);
        assert_eq!(fake.calls(), "ael_t_effect(1)\nael_t_effect18(1)\nael_effect_load(7, true)\nael_t_media(1)");
    }

    #[test]
    fn the_walks_over_every_effect() {
        let mut fake = Fake::default();
        fake.answer("ael_count", &[1, 1, 1, 1, 1, 1]).answer("ael_at", &[40, 40, 40]);
        mark_dirty(&mut fake, 1);
        move_and_update(&mut fake, 1, 2.0, 3.0, 4.5);
        tick(&mut fake, 1, 9, 0.5);
        template_unload(&mut fake, 2);
        assert_eq!(fake.calls(), "ael_count(1)\nael_at(1, 0)\nael_mark_dirty(40)\nael_count(1)\nael_count(1)\nael_at(1, 0)\nael_set_location(40, 2.0, 3.0)\nael_recount(40, 4.5)\nael_count(1)\nael_count(1)\nael_at(1, 0)\nael_tick(40, 9, 0.5)\nael_count(1)\nael_t_effect(2)\nael_t_effect18(2)\nael_t_media(2)");
    }
}
