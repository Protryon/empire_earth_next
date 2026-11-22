//! `EEAEManager.cpp`'s C++ logic in Rust over one-statement shims (UI lane round 11): `CollectEffects` (a unit's
//! effects onto the living units around a location; a radius-0 effect onto every player's units) and
//! `FindEffect` (the active effect of a kind covering a point for a player), in the original's order. The unit
//! passes are `area_effect_obj.rs`' (one C++ functor); the collector stays C++.

use ee_sim::tech::area_effect::EE_AREA_EFFECT_HEALING;

use crate::screens::game_cpp::Cpp;

/// `CollectEffects(radius, location, player, list, unit)`: `list` the `EEUnitAreaEffectList*`, `unit` the
/// generating `EEComplexUnit*`.
#[allow(clippy::too_many_arguments)]
pub(crate) fn collect_effects<C: Cpp + ?Sized>(
    c: &mut C,
    radius: f32,
    x: f32,
    y: f32,
    player: u64,
    list: usize,
    unit: usize,
) {
    let bottom = c.aem_floor(y + radius);
    let right = c.aem_floor(x + radius);
    let top = c.aem_floor(y - radius);
    let left = c.aem_floor(x - radius);
    c.aem_units_clear();
    c.aem_rect_top(top);
    c.aem_rect_left(left);
    c.aem_rect_bottom(bottom);
    c.aem_rect_right(right);
    c.aem_collect_alive();
    let count = c.aem_list_count(list);
    for i in 0..count {
        let effect = c.aem_list_at(list, i);
        let owner = c.aeh_owner(effect);
        if owner != 0 && c.aem_unit_protected(owner) {
            continue;
        }
        if c.aem_effect_radius(effect) <= 0.0 {
            c.aem_for_each(
                crate::screens::area_effect_obj::EE_SCREENS_AEM_PASS_SET_AFFECTED,
                effect,
                player,
                0.0,
                x,
                y,
            );
            continue;
        }
        let units = c.aem_units_count();
        for j in 0..units {
            let target = c.aem_unit_complex(j);
            if !c.aem_covers(effect, player, target, x, y)
                || (target == unit && c.aem_effect_kind(effect) == EE_AREA_EFFECT_HEALING as i64)
            {
                continue;
            }
            c.aem_affecting_add(target, effect);
            c.aem_applied_to(effect, target);
            if c.aem_effect_kind(effect) == EE_AREA_EFFECT_HEALING as i64 {
                c.ael_show_on(effect, target);
            }
        }
    }
}

/// `FindEffect(kind, x, y, player, mode, exclude)` on the manager `manager`: the first active effect, skipping
/// protected owners' and the excluded ones (`exclude` an `EEUnitAreaEffectList*` or 0), of the kind (immunities
/// only in immunity mode `mode`) that affects the player (by the reversed test in immunity mode) and covers the
/// point; 0 without one.
#[allow(clippy::too_many_arguments)]
pub(crate) fn find_effect<C: Cpp + ?Sized>(
    c: &mut C,
    manager: usize,
    kind: i64,
    x: f32,
    y: f32,
    player: u64,
    mode: bool,
    exclude: usize,
) -> usize {
    let count = c.aem_effects_count(manager);
    for i in 0..count {
        let effect = c.aem_effects_at(manager, i);
        let owner = c.aeh_owner(effect);
        if owner != 0 && c.aem_unit_protected(owner) {
            continue;
        }
        if exclude != 0 {
            let excluded = c.aem_list_count(exclude);
            if (0..excluded).any(|j| c.aem_list_at(exclude, j) == effect) {
                continue;
            }
        }
        if mode && !c.aem_is_immunity(effect) {
            continue;
        }
        if c.aem_effect_kind(effect) != kind {
            continue;
        }
        let owner_player = c.aem_effect_player(effect);
        let affects = if !mode {
            c.aem_affects_player(effect, owner_player, player)
        } else {
            c.aem_affects_player_reversed(effect, owner_player, player)
        };
        if !affects {
            continue;
        }
        if c.aem_in_range(effect, x, y) {
            return effect;
        }
    }
    0
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::screens::game_cpp::Fake;

    #[test]
    fn a_radius_0_effect_goes_to_every_players_units() {
        let mut fake = Fake::default();
        fake.answer("aem_list_count", &[1]).answer("aem_list_at", &[50]);
        collect_effects(&mut fake, 2.0, 10.5, 20.5, 3, 7, 9);
        let calls = fake.calls();
        assert!(calls.starts_with("aem_floor(22.5)\naem_floor(12.5)\naem_floor(18.5)\naem_floor(8.5)"));
        assert!(calls.ends_with("aem_effect_radius(50)\naem_for_each(1, 50, 3, 0.0, 10.5, 20.5)"));
    }

    #[test]
    fn a_healing_effect_skips_its_unit_and_shows_on_the_others() {
        let mut fake = Fake::default();
        fake.answer("aem_list_count", &[1]).answer("aem_list_at", &[50]).answer("aem_effect_radius", &[4]);
        fake.answer("aem_units_count", &[2]).answer("aem_unit_complex", &[9, 11]).answer("aem_covers", &[1, 1]);
        fake.answer("aem_effect_kind", &[EE_AREA_EFFECT_HEALING as i64; 3]);
        collect_effects(&mut fake, 2.0, 10.0, 20.0, 3, 7, 9);
        assert!(fake.calls().ends_with(
            "aem_unit_complex(1)\naem_covers(50, 3, 11, 10.0, 20.0)\naem_affecting_add(11, 50)\naem_applied_to(50, 11)\n\
             aem_effect_kind(50)\nael_show_on(50, 11)"
        ));
    }

    #[test]
    fn find_skips_the_excluded_and_tests_the_reversed_players_in_immunity_mode() {
        let mut fake = Fake::default();
        fake.answer("aem_effects_count", &[2]).answer("aem_effects_at", &[50, 51]);
        fake.answer("aem_list_count", &[1, 1]).answer("aem_list_at", &[50, 50]);
        fake.answer("aem_is_immunity", &[1]).answer("aem_effect_kind", &[4]).answer("aem_effect_player", &[2]);
        fake.answer("aem_affects_player_reversed", &[1]).answer("aem_in_range", &[1]);
        assert_eq!(find_effect(&mut fake, 1, 4, 3.0, 4.0, 5, true, 8), 51);
        assert!(fake.calls().contains("aem_affects_player_reversed(51, 2, 5)\naem_in_range(51, 3.0, 4.0)"));
    }
}
