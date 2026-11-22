//! `EEAreaEffect.cpp`'s host whole in Rust over one-statement shims (UI lane round 11): the old `EEAEHost::Call`,
//! the world calls of `ee_sim::tech::area_effect` (the target unit's set, tech, relation, state, position,
//! continent, carrier, hit points and cloak; the owner's scale, hit points, leadership and garrison bonus; the
//! morale buildings' count), in the original's order. `effect` is the `EEAreaEffect*` (0 for a template's test),
//! `target` the `EEComplexUnit*` the slot was given (0 when none).

use ee_sim::tech::area_effect::{EeAreaEffectCall, EeAreaEffectOp, EeAreaEffectReply};

use crate::screens::game_cpp::Cpp;

/// `kUnitTypeSpaceDock` and `kUnitTypeSpaceTurret` (`ee/EEUnitClassIDs.h`).
const SPACE_DOCK: u64 = 0x2d8;
const SPACE_TURRET: u64 = 0x302;

/// The effect's owner unit (0 without an effect or an owner).
fn owner<C: Cpp + ?Sized>(c: &mut C, effect: usize) -> usize {
    if effect != 0 {
        c.aeh_owner(effect)
    } else {
        0
    }
}

/// One area effect host call: its reply in `r`.
pub(crate) fn host<C: Cpp + ?Sized>(
    c: &mut C,
    effect: usize,
    target: usize,
    call: &EeAreaEffectCall,
    r: &mut EeAreaEffectReply,
) {
    let t = target;
    match call.op {
        EeAreaEffectOp::TargetInSet => r.ok = c.aeh_set_contains(t, call.a),
        EeAreaEffectOp::TargetTechKind4 => {
            let node = c.aeh_tech_node(call.a, t);
            r.ok = node != 0 && c.aeh_node_kind(node) == 4;
        }
        EeAreaEffectOp::Relation => {
            let player = c.aeh_player(call.a);
            let other = c.aeh_player(call.b);
            let same = if other == player { 1 } else { 0 };
            let allied = if c.aeh_allied(player, other) { 2 } else { 0 };
            let enemy = if c.aeh_enemy(player, other) { 4 } else { 0 };
            r.value = same | allied | enemy;
        }
        EeAreaEffectOp::TargetBuilt => r.ok = c.aeh_built(t),
        EeAreaEffectOp::TargetRepairable => r.ok = c.aeh_repairable(t),
        EeAreaEffectOp::TargetPosition => {
            r.x = c.aeh_position_x(t);
            r.y = c.aeh_position_y(t);
        }
        EeAreaEffectOp::Continent => {
            c.aeh_tile_x(call.a);
            c.aeh_tile_y(call.b);
            r.value = c.aeh_continent();
        }
        EeAreaEffectOp::TargetCarrierPlanet => {
            let carrier = c.aeh_carrier(t);
            let kind = if carrier != 0 { c.aeh_unit_type(t) } else { 0 };
            r.ok = carrier != 0 && (kind == SPACE_DOCK || kind == SPACE_TURRET);
            r.value = if r.ok { c.aeh_planet(carrier) } else { 0 };
        }
        EeAreaEffectOp::TargetHitPoints => r.value = c.aeh_hit_points(t),
        EeAreaEffectOp::TargetMaxHitPoints => r.value = c.aeh_max_hit_points(t),
        EeAreaEffectOp::InCinematic => r.ok = c.aeh_in_cinematic(),
        EeAreaEffectOp::TargetSetHitPoints => c.aeh_set_hit_points(t, call.a),
        EeAreaEffectOp::TargetCloak => c.aeh_cloak(t),
        // the cloaking's RemovedFrom 0x27ab53: a Camouflage civ power keeps an idle or hidden unit cloaked; a visible
        // cloaked enemy leaves the mini map
        EeAreaEffectOp::TargetUncloak => {
            if c.aeh_has_camouflage(t)
                && c.aeh_camouflage_set_exists()
                && c.aeh_camouflage_set_contains(t)
                && (c.aeh_idle(t) && c.aeh_camouflage_timer(t) & 0x7fff_ffff == 0 || c.aeh_u32c(t) > 0)
            {
                return;
            }
            let player = c.aeh_current_player();
            let visible = ee_sim::units::uu_ffi::ee_units_uu_cloaked_enemy(t as u64, player as u64) != 0;
            c.aeh_set_cloakable_own(t);
            if visible {
                c.aeh_minimap_hide(t);
            }
        }
        EeAreaEffectOp::OwnerScale => {
            let owner = owner(c, effect);
            r.ok = owner != 0;
            r.x = if owner != 0 { c.aeh_owner_scale(owner) } else { 1.0 };
        }
        EeAreaEffectOp::OwnerHitPoints => {
            let owner = owner(c, effect);
            r.value = c.aeh_hit_points(owner);
        }
        EeAreaEffectOp::OwnerLeadership => {
            let owner = owner(c, effect);
            r.x = c.aeh_leadership(owner);
        }
        EeAreaEffectOp::OwnerGarrisonBonus => {
            let owner = owner(c, effect);
            r.x = c.aeh_garrison_bonus(owner);
        }
        // the morale from buildings' collection 0x27b95b (EEGetUnitsInRectangle 0x27bae8 / 0x27bcfc)
        EeAreaEffectOp::CountBuildings => {
            c.aeh_units_clear();
            c.aeh_rect_top(call.b);
            c.aeh_rect_left(call.a);
            c.aeh_rect_right(call.c);
            c.aeh_rect_bottom(call.d);
            r.value = if call.e == 1 {
                c.aeh_count_in_rect(effect)
            } else {
                c.aeh_count_in_range(effect, call.f, call.g, call.x)
            };
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::screens::game_cpp::Fake;

    fn run(fake: &mut Fake, effect: usize, call: EeAreaEffectCall) -> EeAreaEffectReply {
        let mut r = EeAreaEffectReply::default();
        host(fake, effect, 20, &call, &mut r);
        r
    }

    #[test]
    fn the_relation_bits() {
        let mut fake = Fake::default();
        fake.answer("aeh_player", &[5, 6]).answer("aeh_enemy", &[1]);
        let r = run(&mut fake, 0, EeAreaEffectCall { a: 1, b: 2, ..EeAreaEffectCall::new(EeAreaEffectOp::Relation) });
        assert_eq!(r.value, 4);
        assert_eq!(fake.calls(), "aeh_player(1)\naeh_player(2)\naeh_allied(5, 6)\naeh_enemy(5, 6)");
    }

    #[test]
    fn the_owner_scale_is_1_without_an_owner() {
        let mut fake = Fake::default();
        let r = run(&mut fake, 0, EeAreaEffectCall::new(EeAreaEffectOp::OwnerScale));
        assert_eq!((r.ok, r.x), (false, 1.0));
        assert_eq!(fake.calls(), "");
        fake.answer("aeh_owner", &[9]).answer("aeh_owner_scale", &[2]);
        let r = run(&mut fake, 3, EeAreaEffectCall::new(EeAreaEffectOp::OwnerScale));
        assert_eq!((r.ok, r.x), (true, 2.0));
    }

    #[test]
    fn a_carrier_planet_needs_a_dock_or_turret() {
        let mut fake = Fake::default();
        fake.answer("aeh_carrier", &[7]).answer("aeh_unit_type", &[0x302]).answer("aeh_planet", &[3]);
        let r = run(&mut fake, 0, EeAreaEffectCall::new(EeAreaEffectOp::TargetCarrierPlanet));
        assert_eq!((r.ok, r.value), (true, 3));
    }

    #[test]
    fn the_buildings_counted_in_the_rectangle_or_range() {
        let mut fake = Fake::default();
        fake.answer("aeh_count_in_range", &[2]);
        let call = EeAreaEffectCall {
            a: 1,
            b: 2,
            c: 3,
            d: 4,
            e: 2,
            f: 5,
            g: 6,
            x: 9.0,
            ..EeAreaEffectCall::new(EeAreaEffectOp::CountBuildings)
        };
        assert_eq!(run(&mut fake, 8, call).value, 2);
        assert!(fake.calls().ends_with(
            "aeh_rect_top(2)\naeh_rect_left(1)\naeh_rect_right(3)\naeh_rect_bottom(4)\naeh_count_in_range(8, 5, 6, 9.0)"
        ));
    }

    #[test]
    fn a_camouflaged_idle_unit_stays_cloaked() {
        let mut fake = Fake::default();
        fake.answer("aeh_has_camouflage", &[1]).answer("aeh_camouflage_set_exists", &[1]);
        fake.answer("aeh_camouflage_set_contains", &[1])
            .answer("aeh_idle", &[1])
            .answer("aeh_camouflage_timer", &[0x8000_0000]);
        run(&mut fake, 0, EeAreaEffectCall::new(EeAreaEffectOp::TargetUncloak));
        assert!(fake.calls().ends_with("aeh_camouflage_timer(20)"));
    }
}
