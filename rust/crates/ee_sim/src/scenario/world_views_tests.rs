//! The world channel's view answers over the world views' fake objects (world/view_tests.rs' fixture).

use std::ffi::c_void;

use super::*;
use crate::world::layout::with_test_layout;
use crate::world::view_tests::{addr, fake_layout, fixture, Fixture};

/// The fixture's units by id; one effect record.
struct Look<'f> {
    f: &'f Fixture,
    effect: EeScenarioEffectFields,
}

impl Lookups for Look<'_> {
    fn unit(&self, id: i64) -> u64 {
        match id as u32 {
            0x0100_0001 => addr(&*self.f.tree) as u64,
            0x0100_0002 => addr(&*self.f.soldier) as u64,
            _ => 0,
        }
    }
    fn effect(&self, _: *mut c_void) -> *const EeScenarioEffectFields {
        &self.effect
    }
    fn fields(&self, _: u32, _: *mut c_void) -> *const c_void {
        std::ptr::null()
    }
    fn area_center(&self, index: i64) -> i64 {
        index + 1
    }
    fn area(&self, _: i64) -> *const crate::scenario::area_fields::EeScenarioAreaFields {
        std::ptr::null()
    }
    fn object(&self, _: i64) -> *const crate::scenario::fields::EeScenarioObjectFields {
        std::ptr::null()
    }
    fn trigger(&self, _: i64) -> *const crate::scenario::fields::EeScenarioTriggerFields {
        std::ptr::null()
    }
}

/// `answer(op, a)` over the fixture (the world registered), on a context whose unit is `unit`.
fn ask(f: &Fixture, unit: usize, op: u32, a: [i64; 6]) -> Option<i64> {
    let _guard = with_test_layout(EeWorldLayout { world_address: addr(&*f.world) as u64, ..fake_layout() });
    let look =
        Look { f, effect: EeScenarioEffectFields { mTechnology: -3, mTaskArea: 0xffff_fff0, ..Default::default() } };
    let mut ctx = EeScenarioHostContext { unit: unit as *mut c_void, ..Default::default() };
    // SAFETY: the fixture's objects live through the call
    unsafe { answer(&mut ctx as *mut EeScenarioHostContext as *mut c_void, op, &a, &look) }
}

use crate::world::EeWorldLayout;

const SOLDIER: i64 = 0x0100_0002;
const TREE: i64 = 0x0100_0001;

#[test]
fn a_unit_s_values_by_id() {
    let f = fixture();
    let v = |field| ask(&f, 0, EE_SCENARIO_WORLD_UNIT, [SOLDIER, field, 0, 0, 0, 0]);
    assert_eq!(v(EE_SCENARIO_UV_EXISTS), Some(1));
    assert_eq!(ask(&f, 0, EE_SCENARIO_WORLD_UNIT, [7, EE_SCENARIO_UV_EXISTS, 0, 0, 0, 0]), Some(0));
    assert_eq!(v(EE_SCENARIO_UV_COMPLEX), Some(1));
    assert_eq!(v(EE_SCENARIO_UV_HIT_POINTS), Some(90));
    assert_eq!(v(EE_SCENARIO_UV_DAMAGED), Some(1));
    assert_eq!((v(EE_SCENARIO_UV_TILE_X), v(EE_SCENARIO_UV_TILE_Y)), (Some(1), Some(1)));
    assert_eq!(v(EE_SCENARIO_UV_ON_TILE), Some(1));
    assert_eq!((v(EE_SCENARIO_UV_PLAYER), v(EE_SCENARIO_UV_PLAYER_INDEX)), (Some(1), Some(1)));
    assert_eq!((v(EE_SCENARIO_UV_TYPE), v(EE_SCENARIO_UV_CATEGORY)), (Some(0x2e7), Some(6)));
    assert_eq!(v(EE_SCENARIO_UV_COMMAND), Some(5000));
    assert_eq!((v(EE_SCENARIO_UV_WALL), v(EE_SCENARIO_UV_HAS_GOAL)), (Some(1), Some(1)));
    assert_eq!(v(EE_SCENARIO_UV_ID), Some(SOLDIER));
    assert_eq!(v(EE_SCENARIO_UV_CONTAINER), Some(TREE));
    assert_eq!(v(EE_SCENARIO_UV_CARGO), Some(2));
    assert_eq!(v(EE_SCENARIO_UV_TRANSPORT), Some(1));
    // the tree is an EEUnit without a class: the class's values are 0, as no read reached them
    let tree = |field| ask(&f, 0, EE_SCENARIO_WORLD_UNIT, [TREE, field, 0, 0, 0, 0]);
    assert_eq!(
        (tree(EE_SCENARIO_UV_COMPLEX), tree(EE_SCENARIO_UV_TYPE), tree(EE_SCENARIO_UV_HIT_POINTS)),
        (Some(0), Some(0), Some(40))
    );
}

#[test]
fn the_effect_run_s_reads_of_units_tiles_and_the_effect() {
    let f = fixture();
    let x = |op, a0| ask(&f, 0, op, [a0, 0, 0, 0, 0, 0]);
    assert_eq!((x(EE_SCENARIO_WX_UNIT_TILE_X, SOLDIER), x(EE_SCENARIO_WX_UNIT_ON_TILE, SOLDIER)), (Some(1), Some(1)));
    assert_eq!(x(EE_SCENARIO_WX_UNIT_IN_CONTAINER, SOLDIER), Some(1));
    assert_eq!(x(EE_SCENARIO_WX_CONTAINER_NULL, SOLDIER), Some(0));
    assert_eq!(x(EE_SCENARIO_WX_CONTAINER_ON_TILE, SOLDIER), Some(1));
    assert_eq!(x(EE_SCENARIO_WX_CATEGORY, SOLDIER), Some(6));
    assert_eq!(x(EE_SCENARIO_WX_GOAL_KIND, SOLDIER), Some(11));
    assert_eq!(x(EE_SCENARIO_WX_UNIT_Z, SOLDIER), Some(bits(30.0)));
    assert_eq!(ask(&f, 0, EE_SCENARIO_WX_LOCATION_AT, [SOLDIER, 1, 3, 0, 0, 0]), Some(bits(20.0)));
    assert_eq!(ask(&f, 0, EE_SCENARIO_WX_LOCATION_AT, [SOLDIER, 4, 0, 0, 0, 0]), None);
    // the map: tile (1, 0) missing, (0, 0) deep water (terrain 1)
    let tile = |op, x, y| ask(&f, 0, op, [x, y, 0, 0, 0, 0]);
    assert_eq!((tile(EE_SCENARIO_WX_TILE_EXISTS, 0, 0), tile(EE_SCENARIO_WX_TILE_EXISTS, 1, 0)), (Some(1), Some(0)));
    assert_eq!(tile(EE_SCENARIO_WX_TILE_WATER, 0, 0), Some(1));
    assert_eq!(tile(EE_SCENARIO_WX_TILE_HEIGHT, 1, 1), Some(bits(1.0)));
    assert_eq!(x(EE_SCENARIO_WX_EFFECT_TECH, 0), Some(-3));
    assert_eq!(x(EE_SCENARIO_WX_EFFECT_TASK_AREA, 0), Some(0xffff_fff0));
    assert_eq!(x(EE_SCENARIO_WX_PLAYER_IS_CURRENT, 1), Some(1));
    assert_eq!(x(EE_SCENARIO_WX_PLAYER_IS_CURRENT, 0), Some(0));
    assert_eq!(x(EE_SCENARIO_WX_CLASS_COUNT, 0), Some(0));
    // a statement on C++ objects stays the C++ host's
    assert_eq!(x(EE_SCENARIO_WX_DETACH, SOLDIER), None);
}

#[test]
fn the_class_attribute_reads_on_handles_and_the_world_s() {
    let f = fixture();
    let k = |op, a0: usize| ask(&f, 0, op, [a0 as i64, 0, 0, 0, 0, 0]);
    assert_eq!(k(EE_SCENARIO_WK_PLAYER, 1), Some(addr(&*f.player) as i64));
    assert_eq!(k(EE_SCENARIO_WK_PLAYER_ID, addr(&*f.player)), Some(1));
    assert_eq!(k(EE_SCENARIO_WK_FIRST_UNIT, addr(&*f.player)), Some(addr(&*f.tree) as i64));
    assert_eq!(k(EE_SCENARIO_WK_NEXT_UNIT, addr(&*f.tree)), Some(addr(&*f.soldier) as i64));
    assert_eq!(k(EE_SCENARIO_WK_UNIT_CLASS, addr(&*f.soldier)), Some(addr(&*f.class) as i64));
    assert_eq!(k(EE_SCENARIO_WK_UNIT_COMPLEX, addr(&*f.tree)), Some(0));
    assert_eq!(k(EE_SCENARIO_WK_CLASS_FIELDS, addr(&*f.class)), Some(addr(&f.class.fields) as i64));
    assert_eq!(k(EE_SCENARIO_WK_UPGRADE_FIELDS, addr(&*f.stats)), Some(addr(&f.stats.fields) as i64));
    assert_eq!(k(EE_SCENARIO_WORLD_CURRENT_PLAYER, 0), Some(1));
    assert_eq!(k(EE_SCENARIO_WC_MAP_SIZE, 0), Some(2));
    assert_eq!(k(EE_SCENARIO_WY_UNIT_ID, addr(&*f.soldier)), Some(SOLDIER));
    assert_eq!(k(EE_SCENARIO_WORLD_AREA_CENTER, 4), Some(5));
}

#[test]
fn the_unit_change_reads_the_context_s_unit() {
    let f = fixture();
    let soldier = addr(&*f.soldier);
    let cu = |unit, field| ask(&f, unit, EE_SCENARIO_WORLD_CU_VALUE, [field, 0, 0, 0, 0, 0]);
    assert_eq!(cu(soldier, EE_SCENARIO_CU_EXISTS), Some(1));
    assert_eq!(cu(0, EE_SCENARIO_CU_EXISTS), Some(0));
    assert_eq!(
        (cu(soldier, EE_SCENARIO_CU_HIT_POINTS), cu(soldier, EE_SCENARIO_CU_MAX_HIT_POINTS)),
        (Some(90), Some(150))
    );
    assert_eq!(cu(soldier, EE_SCENARIO_CU_POWER), Some(-2));
    assert_eq!(cu(soldier, EE_SCENARIO_CU_CATEGORY), Some(6));
}

#[test]
fn a_player_s_index_is_the_last_slot_holding_it() {
    let mut f = fixture();
    let p = addr(&*f.player) as u64;
    f.world.players[0] = p as usize;
    let _guard = with_test_layout(fake_layout());
    // SAFETY: the fixture lives
    let t = unsafe { WorldRead::begin() };
    // SAFETY: as above
    let w = unsafe { WorldView::of(&t, addr(&*f.world) as u64) }.unwrap();
    assert_eq!(player_index(&w, p), 1);
    assert_eq!(player_index(&w, 0x10), -1);
}

#[test]
fn the_events_rounds_and_host_reads_from_the_views() {
    let f = fixture();
    let _guard = with_test_layout(EeWorldLayout { world_address: addr(&*f.world) as u64, ..fake_layout() });
    let (player, tree, soldier) = (addr(&*f.player) as i64, addr(&*f.tree) as i64, addr(&*f.soldier) as i64);
    // SAFETY: the fixture's objects live through the calls
    let ev = |op, a0, a1| unsafe { event(op, &[a0, a1, 0, 0]) };
    assert_eq!(ev(EE_SCENARIO_EV_PLAYER, 1, 0), Some(player));
    assert_eq!(ev(EE_SCENARIO_EV_PLAYER_ID, player, 0), Some(1));
    assert_eq!(ev(EE_SCENARIO_EV_FIRST_UNIT, player, 0), Some(tree));
    assert_eq!(ev(EE_SCENARIO_EV_NEXT_UNIT, tree, 0), Some(soldier));
    assert_eq!(ev(EE_SCENARIO_EV_UNIT_TYPE, soldier, 0), Some(0x2e7));
    assert_eq!(ev(EE_SCENARIO_EV_UNIT_COMPLEX, tree, 0), Some(0));
    assert_eq!(ev(EE_SCENARIO_EV_COMPLEX_AS_UNIT, soldier, 0), Some(soldier));
    assert_eq!(ev(EE_SCENARIO_EV_SOUND, 3, 0), None);
    // SAFETY: as above
    let round = |op, a| unsafe { round(op, a) };
    assert_eq!(round(EE_SCENARIO_ROUND_NUM_PLAYERS, 0), Some(2));
    assert_eq!(
        (round(EE_SCENARIO_ROUND_PLAYER_EXISTS, 1), round(EE_SCENARIO_ROUND_PLAYER_EXISTS, 0)),
        (Some(1), Some(0))
    );
    assert_eq!(round(EE_SCENARIO_ROUND_GAME_SPEED, 0), Some(bits(1.5)));
    let mut unit = |id: i64| if id == SOLDIER { soldier as u64 } else { 0 };
    let none = |_: i32| std::ptr::null::<EeScenarioObjectFields>();
    // SAFETY: as above
    let mut rd = |op, a, b| unsafe { read(op, a, b, &mut unit, &none) };
    assert_eq!(rd(EE_SCENARIO_READ_MAP_SIZE, 0, 0), Some(7 | (8 << 32)));
    assert_eq!(rd(EE_SCENARIO_READ_UNIT_TILE_OF, SOLDIER as i32, 0), Some(1 | (1 << 32)));
    assert_eq!(rd(EE_SCENARIO_READ_UNIT_CLASS_TYPE, SOLDIER as i32, 0), Some(0x2e7));
    assert_eq!(rd(EE_SCENARIO_READ_UNIT_RANGE, SOLDIER as i32, 0), Some(bits(5.5)));
    assert_eq!(rd(EE_SCENARIO_READ_OBJECT_EXISTS, 4, 0), Some(0));
    assert_eq!(
        (rd(EE_SCENARIO_READ_MAP_TILE_EXISTS, 0, 0), rd(EE_SCENARIO_READ_MAP_TILE_EXISTS, -1, 0)),
        (Some(1), Some(0))
    );
    assert_eq!(rd(EE_SCENARIO_READ_RANDOM, 0, 9), None);
}
