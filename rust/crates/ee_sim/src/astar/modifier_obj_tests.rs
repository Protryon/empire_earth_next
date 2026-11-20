use super::*;
use crate::astar::continent_grid::EePoint;

fn refs(m: *mut EeAStarModifier) -> u32 {
    unsafe { (*m).refs }
}

#[test]
fn a_modifier_lives_until_its_last_reference() {
    let m = new(Kind::WallsDoNotBlock);
    assert_eq!(refs(m), 1);
    unsafe { reference(m) };
    assert_eq!(refs(m), 2);
    unsafe { release(m) };
    assert_eq!(refs(m), 1);
    unsafe { release(m) };
    // null is no modifier
    unsafe { reference(ptr::null_mut()) };
    unsafe { release(ptr::null_mut()) };
}

#[test]
fn the_attackable_modifier_drops_both_units_last() {
    cpp::UNIT_REFS.with(|r| r.borrow_mut().clear());
    let m = new(Kind::Attackable { unit: 0x10, target: 0x20, range_squared: 25.0 });
    unsafe { reference(m) };
    unsafe { release(m) };
    assert!(cpp::UNIT_REFS.with(|r| r.borrow().is_empty()));
    unsafe { release(m) };
    assert_eq!(cpp::UNIT_REFS.with(|r| r.borrow().clone()), vec![(0x10, -1), (0x20, -1)]);
}

#[test]
fn the_machine_types_modifiers_and_their_goals() {
    let mut goal = EeAStarPathGoalRec::new();
    let g: *mut EeAStarPathGoalRec = &mut goal;
    assert!(new_for_machine(1, false, g).is_null());
    let plain = new_for_machine(2, false, g);
    let bounded = new_for_machine(0x10, true, g);
    unsafe {
        assert_eq!((*plain).kind, Kind::PlainSea);
        assert_eq!((*plain).goal, g);
        assert!((*plain).bounded_goal.is_null());
        assert_eq!((*bounded).kind, Kind::BoundedSpace);
        assert_eq!((*bounded).bounded_goal, g);
        // a plain modifier is no bounded one: the bounded goal's attach gets none
        let mut other = EeAStarPathGoalRec::new();
        assert_eq!(attach(plain, &mut other, false), plain);
        assert_eq!((*plain).goal, &mut other as *mut _);
        assert!(attach(plain, &mut other, true).is_null());
        assert_eq!(attach(bounded, &mut other, true), bounded);
        assert_eq!((*bounded).goal, &mut other as *mut _);
        assert_eq!((*bounded).bounded_goal, &mut other as *mut _);
        release(plain);
        release(bounded);
    }
}

#[test]
fn the_flat_costs() {
    let a = new(Kind::Attackable { unit: 0, target: 0, range_squared: 0.0 });
    let w = new(Kind::WallsDoNotBlock);
    let p = new(Kind::WallPather);
    unsafe {
        assert_eq!(cost(a, 0, 0, 1, 1, 0, 0), 10);
        assert_eq!(cost(w, 0, 0, 1, 0, 0, 0), 10);
        assert_eq!(cost(w, 0, 0, 1, 1, 0, 0), 14);
        assert_eq!(cost(p, 3, 3, 3, 4, 0, 0), 10);
        assert_eq!(cost(p, 3, 3, 4, 4, 0, 0), 14);
        // the storm planners are no tile modifiers: the base's answers
        let r = new(Kind::StormRain);
        assert_eq!(cost(r, 0, 0, 1, 1, 0, 0), 1);
        assert!(can_enter(r, 7, 0, 0, false));
        assert!(!can_enter(r, 0, 0, 0, false));
        assert!(graph_can_enter(r, 5, 0));
        release(r);
        release(a);
        release(w);
        release(p);
    }
}

#[test]
fn the_attackable_modifier_moves_the_goal_onto_a_node_in_range() {
    let mut goal = EeAStarPathGoalRec::new();
    goal.destination = EePoint { x: 10, y: 10 };
    goal.goal = EePoint { x: 10, y: 10 };
    let m = new(Kind::Attackable { unit: 0, target: 0, range_squared: 9.0 });
    unsafe {
        attach(m, &mut goal, false);
        attach(m, &mut goal, true);
        assert!(check_node(m, None));
        assert!(!check_node(m, Some((14, 10))), "16 is out of range");
        assert_eq!(goal.destination, EePoint { x: 10, y: 10 });
        assert!(check_node(m, Some((13, 10))), "9 is in range");
        assert_eq!(goal.destination, EePoint { x: 13, y: 10 });
        assert_eq!(goal.goal, EePoint { x: 13, y: 10 });
        release(m);
    }
    // the plain bounded ones only test for the end
    let s = new(Kind::BoundedSea);
    unsafe {
        assert!(check_node(s, None));
        assert!(!check_node(s, Some((1, 1))));
        release(s);
    }
    let storm = new(Kind::Storm(StormShaper { max_steps: 0, ..Default::default() }));
    unsafe {
        assert!(check_node(storm, Some((1, 1))), "no steps left");
        release(storm);
    }
}

#[test]
fn the_squared_distance_is_two_float_terms() {
    assert_eq!(distance_squared_f(0, 0, 3, 4), 25.0);
    assert_eq!(distance_squared_f(-2, 5, 1, 1), 25.0);
    // large terms each rounded to a float before the sum
    assert_eq!(distance_squared_f(0, 0, 16_777_217, 0), 16_777_217i64.pow(2) as i32 as f32);
}

#[test]
fn the_saved_route_planners_come_back() {
    let mut out = EePathSavedGraph::default();
    unsafe {
        assert_eq!(save(ptr::null_mut(), &mut out), SAVED_NONE);
        let mut i = Influence { player: 3, enemy: 2, ..Default::default() };
        i.weights[4] = 1.5;
        let e = new(Kind::EnemyAvoidance(i));
        assert_eq!(save(e, &mut out), SAVED_ENEMY_AVOIDANCE);
        assert_eq!((out.modifier_player, out.modifier_enemy, out.weights[4]), (3, 2, 1.5));
        out.modifier = SAVED_ENEMY_AVOIDANCE as i32;
        let back = from_saved(&out);
        assert_eq!((*back).kind, Kind::EnemyAvoidance(i));
        release(back);
        release(e);
        let r = new(Kind::StormSnow);
        assert_eq!(save(r, &mut out), SAVED_STORM_SNOW);
        release(r);
        let t = new(Kind::WallsDoNotBlock);
        assert_eq!(save(t, &mut out), -1, "a tile modifier is no route planner");
        release(t);
    }
    for (kind, want) in [(2, Some(Kind::WallPlanner(Influence::default()))), (9, Some(Kind::StormRain)), (5, None)] {
        let s = EePathSavedGraph { modifier: kind, ..Default::default() };
        let m = from_saved(&s);
        match want {
            Some(k) => unsafe {
                assert_eq!((*m).kind, k);
                release(m);
            },
            None => assert!(m.is_null()),
        }
    }
}
