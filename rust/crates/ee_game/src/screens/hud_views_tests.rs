//! The HUD's reads over fake objects laid out like the C++ classes (their offsets this test thread's layout,
//! `ee_sim`'s `test_layout` feature).

use std::mem::offset_of;
use std::os::raw::{c_long, c_ulong};

use ee_sim::world::layout::{self, EeWorldLayout};
use ee_sim::world::player_fields::{EeUnitsPlayerFields, EeUnitsPlayerSaved, EeUnitsRtsPlayerFields};
use ee_sim::world::stats_fields::EeUnitsUnitClassUpgradeFields;
use ee_sim::world::unit_class_fields::EeUnitsUnitClassFields;
use ee_sim::world::unit_fields::{EeUnitsComplexUnitFields, EeUnitsRtsUnitFields, EeUnitsUnitFields};

use super::*;

#[repr(C)]
#[derive(Default)]
struct FakeUnit {
    kind: u8,
    tile_node: usize,
    player: usize,
    class: usize,
    complex_unit: usize,
    stats: usize,
    cargo: [usize; 3],
    build_queue: [usize; 3],
    next: usize,
    tile: usize,
    container: usize,
    previous_part: usize,
    goal: usize,
    command: usize,
    location: [[f32; 4]; 4],
    rts: EeUnitsRtsUnitFields,
    unit: EeUnitsUnitFields,
    complex: EeUnitsComplexUnitFields,
}

#[repr(C)]
#[derive(Default)]
struct FakeClass {
    fields: EeUnitsUnitClassFields,
}

#[repr(C)]
#[derive(Default)]
struct FakeStats {
    class: usize,
    button: usize,
    fields: EeUnitsUnitClassUpgradeFields,
}

#[repr(C)]
#[derive(Default)]
struct FakePlayer {
    first_unit: usize,
    selected_group: usize,
    selected_viewed: usize,
    technologies: usize,
    rts: EeUnitsRtsPlayerFields,
    saved: EeUnitsPlayerSaved,
    fields: EeUnitsPlayerFields,
}

#[repr(C)]
#[derive(Default)]
struct FakeGroup {
    units: [usize; 3],
}

#[repr(C)]
#[derive(Default)]
struct FakeGoal {
    kind: u8,
    id: c_ulong,
}

#[repr(C)]
#[derive(Default)]
struct FakeWorld {
    players: [usize; 17],
    current: c_long,
    flags: [bool; 8],
}

fn addr<T>(v: &T) -> usize {
    v as *const T as usize
}

fn fake_layout(world: usize) -> EeWorldLayout {
    EeWorldLayout {
        size: std::mem::size_of::<EeWorldLayout>() as u64,
        rts_unit_fields: offset_of!(FakeUnit, rts) as u64,
        unit_fields: offset_of!(FakeUnit, unit) as u64,
        complex_unit_fields: offset_of!(FakeUnit, complex) as u64,
        unit_class_fields: offset_of!(FakeClass, fields) as u64,
        stats_fields: offset_of!(FakeStats, fields) as u64,
        rts_player_fields: offset_of!(FakePlayer, rts) as u64,
        player_saved: offset_of!(FakePlayer, saved) as u64,
        player_fields: offset_of!(FakePlayer, fields) as u64,
        unit_object_kind: offset_of!(FakeUnit, kind) as u64,
        unit_tile_node: offset_of!(FakeUnit, tile_node) as u64,
        unit_player: offset_of!(FakeUnit, player) as u64,
        unit_class: offset_of!(FakeUnit, class) as u64,
        unit_complex_unit: offset_of!(FakeUnit, complex_unit) as u64,
        unit_stats: offset_of!(FakeUnit, stats) as u64,
        complex_cargo: offset_of!(FakeUnit, cargo) as u64,
        complex_build_queue: offset_of!(FakeUnit, build_queue) as u64,
        unit_next: offset_of!(FakeUnit, next) as u64,
        unit_tile: offset_of!(FakeUnit, tile) as u64,
        complex_container: offset_of!(FakeUnit, container) as u64,
        complex_previous_part: offset_of!(FakeUnit, previous_part) as u64,
        complex_goal_state: offset_of!(FakeUnit, goal) as u64,
        complex_command: offset_of!(FakeUnit, command) as u64,
        action_type: 0,
        complex_location: offset_of!(FakeUnit, location) as u64,
        goal_kind: offset_of!(FakeGoal, kind) as u64,
        goal_id: offset_of!(FakeGoal, id) as u64,
        player_first_unit: offset_of!(FakePlayer, first_unit) as u64,
        build_entry_stride: 2 * std::mem::size_of::<c_long>() as u64,
        build_entry_technology: 0,
        player_selected_group: offset_of!(FakePlayer, selected_group) as u64,
        player_selected_viewed_unit: offset_of!(FakePlayer, selected_viewed) as u64,
        player_technologies: offset_of!(FakePlayer, technologies) as u64,
        world_players: offset_of!(FakeWorld, players) as u64,
        world_current_player: offset_of!(FakeWorld, current) as u64,
        world_victory_enabled: offset_of!(FakeWorld, flags) as u64,
        world_in_cinematic: offset_of!(FakeWorld, flags) as u64 + 4,
        world_using_carbon: offset_of!(FakeWorld, flags) as u64 + 6,
        world_address: world as u64,
        group_units: offset_of!(FakeGroup, units) as u64,
        stats_unit_class: offset_of!(FakeStats, class) as u64,
        stats_button: offset_of!(FakeStats, button) as u64,
        ..Default::default()
    }
}

/// A two-unit selection of the local player (1): a land citizen (complex, on the map, cargo of one, a queue of two)
/// and an enemy's airport (player 2) without a complex unit.
#[test]
fn hud_reads_over_the_views() {
    let mut world = Box::<FakeWorld>::default();
    let mut me = Box::<FakePlayer>::default();
    let mut enemy = Box::<FakePlayer>::default();
    me.rts.mPlayerID = 1;
    enemy.rts.mPlayerID = 2;
    enemy.saved.mEnemies[1] = 1;
    me.saved.mResourceCaps = [0, 0, 10, 20, 30, 40, 50];
    me.saved.mGameResult = 3;
    world.players[1] = addr(&*me);
    world.players[2] = addr(&*enemy);
    world.current = 1;
    world.flags[4] = true;
    let _layout = layout::with_test_layout(fake_layout(addr(&*world)));

    let mut citizen_class = Box::<FakeClass>::default();
    citizen_class.fields.mCategory = CATEGORY_LAND_CITIZEN as c_ulong;
    citizen_class.fields.mUnitType = 7;
    citizen_class.fields.mL294 = -5;
    citizen_class.fields.mCommands = [1, 2, 3, 4, 5, 6, 7, 8, 9];
    citizen_class.fields.mNameStringID = 4321;
    citizen_class.fields.mHitPoints = 25;
    citizen_class.fields.mTransportCapacity = 2;
    citizen_class.fields.mUpgradesGunArmor = true;
    let mut base_class = Box::<FakeClass>::default();
    base_class.fields.mAttack = 9;
    base_class.fields.mRange = 1.5;
    let mut stats = Box::<FakeStats>::default();
    stats.class = addr(&*base_class);
    stats.button = 0xb00;
    stats.fields.mMaxHitPoints = 40;
    stats.fields.mAttack = 3.25;
    stats.fields.mPower = 11;
    let mut airport_class = Box::<FakeClass>::default();
    airport_class.fields.mUnitType = TYPE_AIRPORT as c_ulong;
    airport_class.fields.mCategory = CATEGORY_GATE as c_ulong;

    let mut cargo = Box::<FakeUnit>::default();
    cargo.kind = 2;
    let cargo_list: Vec<usize> = vec![addr(&*cargo)];
    let queue: Vec<c_long> = vec![101, 0, 202, 0];
    let mut citizen = Box::<FakeUnit>::default();
    citizen.kind = 2;
    citizen.tile_node = 1;
    citizen.player = addr(&*me);
    citizen.class = addr(&*citizen_class);
    citizen.stats = addr(&*stats);
    citizen.rts.mUnitID = 77;
    citizen.rts.mTileX = 5;
    citizen.rts.mTileY = -3;
    citizen.unit.mHitPoints = 19;
    citizen.complex.mProductionTime = 1234;
    citizen.complex.mCurrentResearch = -1;
    citizen.complex.mPower = 6;
    let cb = cargo_list.as_ptr() as usize;
    citizen.cargo = [cb, cb + std::mem::size_of::<usize>(), 0];
    let qb = queue.as_ptr() as usize;
    citizen.build_queue = [qb, qb + 2 * 2 * std::mem::size_of::<c_long>(), 0];
    citizen.complex_unit = addr(&*citizen);
    let mut airport = Box::<FakeUnit>::default();
    airport.kind = 1;
    airport.player = addr(&*enemy);
    airport.class = addr(&*airport_class);
    airport.stats = addr(&*stats);
    let selection: Vec<usize> = vec![addr(&*citizen), addr(&*airport)];
    let mut group = Box::<FakeGroup>::default();
    let sb = selection.as_ptr() as usize;
    group.units = [sb, sb + 2 * std::mem::size_of::<usize>(), 0];
    me.selected_group = addr(&*group);
    me.selected_viewed = addr(&*airport);
    let c = addr(&*citizen) as i64;
    let p = addr(&*airport) as i64;

    // the main panel group
    assert_eq!(main_group(EE_SCREENS_MGR_OP_IN_CINEMATIC, 0, 0, 0), Some(1));
    assert_eq!(main_group(EE_SCREENS_MGR_OP_SELECTED_COUNT, 0, 0, 0), Some(2));
    assert_eq!(main_group(EE_SCREENS_MGR_OP_SELECTED_UNIT, 1, 0, 0), Some(p));
    assert_eq!(main_group(EE_SCREENS_MGR_OP_VIEWED_UNIT, 0, 0, 0), Some(p));
    assert_eq!(main_group(EE_SCREENS_MGR_OP_UNIT_HP, c, 0, 0), Some(19));
    assert_eq!(main_group(EE_SCREENS_MGR_OP_UNIT_ID, c, 0, 0), Some(77));
    assert_eq!(main_group(EE_SCREENS_MGR_OP_UNIT_KIND, c, 0, 0), Some(1 | 2 | 4));
    assert_eq!(main_group(EE_SCREENS_MGR_OP_UNIT_KIND, p, 0, 0), Some(0));
    assert_eq!(main_group(EE_SCREENS_MGR_OP_UNIT_L294, c, 0, 0), Some(-5));
    assert_eq!(main_group(EE_SCREENS_MGR_OP_AIRPORT, c, 0, 0), Some(0));
    assert_eq!(main_group(EE_SCREENS_MGR_OP_COMPLEX_LOCAL, c, 0, 0), Some(1));
    assert_eq!(main_group(EE_SCREENS_MGR_OP_COMMAND, c, 8, 0), Some(9));
    assert_eq!(main_group(EE_SCREENS_MGR_OP_ON_MAP_LOCAL, c, 0, 0), Some(1));
    assert_eq!(main_group(EE_SCREENS_MGR_OP_ON_MAP_LOCAL, p, 0, 0), Some(0));
    assert_eq!(main_group(EE_SCREENS_MGR_OP_WORKER, c, 0, 0), Some(1));
    assert_eq!(main_group(EE_SCREENS_MGR_OP_QUEUE_SIZE, c, 0, 0), Some(2));
    assert_eq!(main_group(EE_SCREENS_MGR_OP_QUEUE_KEY, c, 0, 1), Some(202));
    // the citizens' queue and the C++'s own ops stay the host's
    assert_eq!(main_group(EE_SCREENS_MGR_OP_QUEUE_SIZE, c, 1, 0), None);
    assert_eq!(main_group(EE_SCREENS_MGR_OP_FIELD, 0, 0, 0), None);

    // the icon bar, the portrait, the improvement and command buttons
    assert_eq!(icon_bar(EE_SCREENS_IB_OP_GROUP_COUNT, 0, 0), Some(2));
    assert_eq!(icon_bar(EE_SCREENS_IB_OP_GROUP_UNIT, 0, 0), Some(c));
    assert_eq!(icon_bar(EE_SCREENS_IB_OP_CARGO_COUNT, c, 0), Some(1));
    assert_eq!(icon_bar(EE_SCREENS_IB_OP_CARGO, c, 0), Some(addr(&*cargo) as i64));
    assert_eq!(icon_bar(EE_SCREENS_IB_OP_UNIT_TYPE, c, 0), Some(7));
    assert_eq!(icon_bar(EE_SCREENS_IB_OP_TILE, c, 0), Some(5 | -3i64 << 32));
    assert_eq!(icon_bar(EE_SCREENS_IB_OP_MAX_HIT_POINTS, c, 0), Some(40));
    assert_eq!(icon_bar(EE_SCREENS_IB_OP_BUTTON, c, 0), Some(0xb00));
    assert_eq!(portrait(EE_SCREENS_PT_OP_LOCAL, c), Some(1));
    assert_eq!(portrait(EE_SCREENS_PT_OP_PRODUCTION, c), Some(1234));
    assert_eq!(portrait(EE_SCREENS_PT_OP_RESEARCH, c), Some(-1));
    assert_eq!(upgrade(EE_SCREENS_UP_OP_FIRST_UNIT), Some(c));
    assert_eq!(upgrade(EE_SCREENS_UP_OP_GROUP_COUNT), Some(2));
    assert_eq!(status_button(EE_SCREENS_SB_OP_UNIT_TYPE, 1), Some(TYPE_AIRPORT as i64));
    assert_eq!(status_button(EE_SCREENS_SB_OP_UNIT, 2), Some(0));

    // the main form
    assert_eq!(main_form(EE_SCREENS_MF_OP_HAS_PLAYER, 0), Some(1));
    assert_eq!(main_form(EE_SCREENS_MF_OP_GROUP_HAS_FIRST, 0), Some(1));
    assert_eq!(main_form(EE_SCREENS_MF_OP_RESOURCE, 1), Some(20));
    assert_eq!(main_form(EE_SCREENS_MF_OP_GAME_RESULT, 0), Some(3));
    assert_eq!(main_form(EE_SCREENS_MF_OP_VICTORY_ENABLED, 0), Some(0));

    // the status box's view: the class's, the stats', the stats' class's values; the enemy is not friendly
    let v = status_box_view(c, true, true);
    assert_eq!((v.category, v.unit_type, v.class_hit_points, v.hit_points), (6, 7, 25, 19));
    assert_eq!((v.max_hit_points, v.stats_attack, v.stats_class_attack, v.stats_class_range), (40, 3.25, 9, 1.5));
    assert_eq!(v.upgrades_armor, [false, false, false, true, false, false]);
    assert!(v.friendly && v.complex && v.two_bars);
    assert_eq!((v.power, v.stats_power, v.complex_class_transport), (6, 11, 2));
    let mut enemy_box = Box::<FakeUnit>::default();
    enemy_box.kind = 1;
    enemy_box.player = addr(&*enemy);
    enemy_box.class = addr(&*citizen_class);
    enemy_box.stats = addr(&*stats);
    let e = status_box_view(addr(&*enemy_box) as i64, false, true);
    assert!(!e.friendly && !e.complex && !e.two_bars);
    assert!(status_box_view(addr(&*enemy_box) as i64, false, false) == EeScreensHudUnit { friendly: false, ..e });
    // the read view's airport question: player 2 has none selected, player 1 has the airport in its selection
    assert!(airport_selected(None) && airport_selected(Some(1)));
    assert!(!airport_selected(Some(2)) && !airport_selected(Some(17)));

    // the diplomacy form's player: the enemy's enemies, resources from food on, the tribute fee
    enemy.saved.mResourceCaps = [9, 9, 1, 2, 3, 4, 5];
    enemy.saved.mSendChatMessage[4] = true;
    enemy.fields.mTributeFee = 0.25;
    enemy.rts.mSlot = 3;
    let d = diplomacy_player(2, || true).unwrap();
    assert_eq!((d.player_id, d.slot, d.human, d.resources, d.enemies[1]), (2, 3, true, [1, 2, 3, 4, 5], 1));
    assert!(d.send_chat[4] && !d.send_chat[3] && d.tribute_fee == 0.25);
    assert!(diplomacy_player(5, || true).is_none());

    // the hot keys' snapshot: the citizen (alive, on a tile, selected, carried, with a goal) then a dead unit
    // without a complex unit; the aircraft's distance from the target
    let goal = Box::new(FakeGoal { kind: 7, id: 3 });
    let mut container = Box::<FakeUnit>::default();
    container.kind = 2;
    container.location[0][3] = 4.0;
    container.location[1][3] = 6.0;
    let mut dead = Box::<FakeUnit>::default();
    dead.kind = 1;
    dead.rts.mUnitID = 88;
    citizen.next = addr(&*dead);
    citizen.tile = 0x40;
    citizen.goal = addr(&*goal);
    let action = Box::new(5u32);
    citizen.command = addr(&*action);
    citizen.container = addr(&*container);
    citizen.complex.mIsCommandable = true;
    citizen.rts.mTileX = -10000;
    stats.fields.mMaxVelocity = 2.5;
    me.first_unit = addr(&*citizen);
    let mut keep = Vec::new();
    let units = hot_key_units(true, [1.0, 2.0], &mut keep);
    assert_eq!(keep, vec![c as u64, 0]);
    assert_eq!(units.len(), 2);
    let u = units[0];
    let want = EE_SCREENS_HK_F_COMPLEX
        | EE_SCREENS_HK_F_ALIVE
        | EE_SCREENS_HK_F_ON_TILE
        | EE_SCREENS_HK_F_SELECTED
        | EE_SCREENS_HK_F_C_COMMANDABLE
        | EE_SCREENS_HK_F_C_CONTAINER
        | EE_SCREENS_HK_F_C_GOAL;
    assert_eq!((u.unit_id, u.complex_id, u.flags, u.unit_type, u.category), (77, 77, want, 7, 6));
    assert_eq!((u.max_velocity, u.c_goal_kind, u.c_max_hit_points, u.command), (2.5f32.to_bits(), 7, 40, 5));
    assert_eq!(u.distance, (9.0f32 + 16.0).to_bits());
    assert_eq!(units[1], EeScreensHotKeyUnit { unit_id: 88, ..Default::default() });
    assert_eq!(hot_key_units(false, [0.0; 2], &mut keep)[0].distance, 0);

    // the sends' reads: the current player's index, a unit's id
    assert_eq!((current_player_index(), unit_id(c), unit_id(0)), (1, 77, 0));

    // no current player, no group
    world.current = 3;
    assert_eq!(main_form(EE_SCREENS_MF_OP_HAS_PLAYER, 0), Some(0));
    assert_eq!(icon_bar(EE_SCREENS_IB_OP_GROUP_COUNT, 0, 0), Some(-1));
    assert_eq!(upgrade(EE_SCREENS_UP_OP_HAS_GROUP), Some(0));
    assert!(hot_key_units(false, [0.0; 2], &mut keep).is_empty());
    drop((selection, cargo_list, queue));
}
