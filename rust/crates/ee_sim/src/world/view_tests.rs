//! The layout and views over fake objects: Rust structs laid out like the C++ classes, their offsets the layout's.

use std::mem::offset_of;
use std::os::raw::{c_long, c_ulong};

use super::check::{digest, EeWorldViewDigest};
use super::layout::{self, with_test_layout, EeWorldLayout};
use super::map_fields::{EeUnitsMapFields, EeUnitsRtsMapFields};
use super::player_fields::{EeUnitsPlayerFields, EeUnitsPlayerSaved, EeUnitsRtsPlayerFields};
use super::stats_fields::EeUnitsUnitClassUpgradeFields;
use super::unit_class_fields::EeUnitsUnitClassFields;
use super::unit_fields::{EeUnitsComplexUnitFields, EeUnitsRtsUnitFields, EeUnitsUnitFields};
use super::view::{
    read_depth, ClassView, FieldRead, MapView, PlayerView, TileView, UnitView, WorldRead, WorldView,
};

#[repr(C)]
#[derive(Default)]
struct FakeUnit {
    vptr: usize,
    tile_node: usize,
    next: usize,
    tile: usize,
    player: usize,
    rts: EeUnitsRtsUnitFields,
    kind: u8,
    class: usize,
    complex_unit: usize,
    stats: usize,
    unit: EeUnitsUnitFields,
    location: [[f32; 4]; 4],
    active_group: usize,
    container: usize,
    command: usize,
    goal: usize,
    active_goal: usize,
    herd: usize,
    cargo: [usize; 3],
    previous_part: usize,
    next_part: usize,
    build_queue: [usize; 3],
    complex: EeUnitsComplexUnitFields,
}

#[repr(C)]
#[derive(Default)]
struct FakeClass {
    count: u64,
    fields: EeUnitsUnitClassFields,
}

#[repr(C)]
#[derive(Default)]
struct FakeStats {
    pad: [u64; 3],
    fields: EeUnitsUnitClassUpgradeFields,
}

#[repr(C)]
#[derive(Default)]
struct FakeGoal {
    vptr: usize,
    pad: [u8; 5],
    kind: u8,
    id: c_ulong,
}

#[repr(C)]
struct FakePlayer {
    vptr: usize,
    rts: EeUnitsRtsPlayerFields,
    color: [f32; 3],
    first_unit: usize,
    saved: EeUnitsPlayerSaved,
    fields: EeUnitsPlayerFields,
    selected_group: usize,
}

impl Default for FakePlayer {
    fn default() -> Self {
        FakePlayer {
            vptr: 0,
            rts: EeUnitsRtsPlayerFields::default(),
            color: [1.0; 3],
            first_unit: 0,
            saved: EeUnitsPlayerSaved::default(),
            fields: EeUnitsPlayerFields::default(),
            selected_group: 0,
        }
    }
}

#[repr(C)]
#[derive(Default)]
struct FakeTile {
    vptr: usize,
    terrain: c_long,
    units: usize,
    visibility: usize,
    height: f32,
    ai: i8,
}

#[repr(C)]
#[derive(Default)]
struct FakeNode {
    vptr: usize,
    unit: usize,
    next: usize,
}

#[repr(C)]
#[derive(Default)]
struct FakeMap {
    vptr: usize,
    tiles: usize,
    rts: EeUnitsRtsMapFields,
    fields: EeUnitsMapFields,
}

#[repr(C)]
#[derive(Default)]
struct FakeWorld {
    vptr: usize,
    players: [usize; 17],
    map: usize,
    num_players: c_ulong,
    current: c_long,
    speed: f32,
    population_limit: c_long,
    wonders: c_ulong,
    options: [u8; 16],
    ghosts: [usize; 3],
    flags: [bool; 7],
}

fn fake_layout() -> EeWorldLayout {
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
        rts_map_fields: offset_of!(FakeMap, rts) as u64,
        map_fields: offset_of!(FakeMap, fields) as u64,
        unit_object_kind: offset_of!(FakeUnit, kind) as u64,
        unit_tile_node: offset_of!(FakeUnit, tile_node) as u64,
        unit_next: offset_of!(FakeUnit, next) as u64,
        unit_tile: offset_of!(FakeUnit, tile) as u64,
        unit_player: offset_of!(FakeUnit, player) as u64,
        unit_class: offset_of!(FakeUnit, class) as u64,
        unit_complex_unit: offset_of!(FakeUnit, complex_unit) as u64,
        unit_stats: offset_of!(FakeUnit, stats) as u64,
        complex_location: offset_of!(FakeUnit, location) as u64,
        complex_active_group: offset_of!(FakeUnit, active_group) as u64,
        complex_container: offset_of!(FakeUnit, container) as u64,
        complex_command: offset_of!(FakeUnit, command) as u64,
        complex_goal_state: offset_of!(FakeUnit, goal) as u64,
        complex_active_goal: offset_of!(FakeUnit, active_goal) as u64,
        complex_herd: offset_of!(FakeUnit, herd) as u64,
        complex_cargo: offset_of!(FakeUnit, cargo) as u64,
        complex_previous_part: offset_of!(FakeUnit, previous_part) as u64,
        complex_next_part: offset_of!(FakeUnit, next_part) as u64,
        complex_build_queue: offset_of!(FakeUnit, build_queue) as u64,
        build_entry_stride: 2 * std::mem::size_of::<c_long>() as u64,
        build_entry_technology: 0,
        goal_kind: offset_of!(FakeGoal, kind) as u64,
        goal_id: offset_of!(FakeGoal, id) as u64,
        action_type: 0,
        player_color: offset_of!(FakePlayer, color) as u64,
        player_first_unit: offset_of!(FakePlayer, first_unit) as u64,
        player_selected_group: offset_of!(FakePlayer, selected_group) as u64,
        world_players: offset_of!(FakeWorld, players) as u64,
        world_map: offset_of!(FakeWorld, map) as u64,
        world_num_players: offset_of!(FakeWorld, num_players) as u64,
        world_current_player: offset_of!(FakeWorld, current) as u64,
        world_game_speed: offset_of!(FakeWorld, speed) as u64,
        world_population_limit: offset_of!(FakeWorld, population_limit) as u64,
        world_wonders_to_win: offset_of!(FakeWorld, wonders) as u64,
        world_game_options: offset_of!(FakeWorld, options) as u64,
        world_ghost_units: offset_of!(FakeWorld, ghosts) as u64,
        world_victory_enabled: offset_of!(FakeWorld, flags) as u64,
        world_game_screen_active: offset_of!(FakeWorld, flags) as u64 + 1,
        world_loading_saved_game: offset_of!(FakeWorld, flags) as u64 + 2,
        world_cheats_enabled: offset_of!(FakeWorld, flags) as u64 + 3,
        world_in_cinematic: offset_of!(FakeWorld, flags) as u64 + 4,
        world_user_paused: offset_of!(FakeWorld, flags) as u64 + 5,
        world_using_carbon: offset_of!(FakeWorld, flags) as u64 + 6,
        map_tiles: offset_of!(FakeMap, tiles) as u64,
        tile_stride: std::mem::size_of::<FakeTile>() as u64,
        tile_terrain_type: offset_of!(FakeTile, terrain) as u64,
        tile_units: offset_of!(FakeTile, units) as u64,
        tile_visibility: offset_of!(FakeTile, visibility) as u64,
        tile_height: offset_of!(FakeTile, height) as u64,
        tile_ai_value: offset_of!(FakeTile, ai) as u64,
        unit_node_unit: offset_of!(FakeNode, unit) as u64,
        unit_node_next: offset_of!(FakeNode, next) as u64,
    }
}

fn addr<T>(v: &T) -> usize {
    v as *const T as usize
}

/// A small world: player 1 with an EEUnit (a tree) and a complex unit carrying cargo, a 2 x 2 map with one tile
/// missing; all boxed so the addresses stay.
struct Fixture {
    world: Box<FakeWorld>,
    player: Box<FakePlayer>,
    tree: Box<FakeUnit>,
    soldier: Box<FakeUnit>,
    cargo: Box<FakeUnit>,
    cargo_list: Box<[usize; 2]>,
    class: Box<FakeClass>,
    stats: Box<FakeStats>,
    goal: Box<FakeGoal>,
    action: Box<u32>,
    map: Box<FakeMap>,
    tiles: Vec<FakeTile>,
    tile_array: Box<[usize; 4]>,
    visibility: Box<[u32; 6]>,
    nodes: Vec<FakeNode>,
}

fn fixture() -> Fixture {
    let mut f = Fixture {
        world: Box::default(),
        player: Box::default(),
        tree: Box::default(),
        soldier: Box::default(),
        cargo: Box::default(),
        cargo_list: Box::new([0; 2]),
        class: Box::default(),
        stats: Box::default(),
        goal: Box::default(),
        action: Box::new(5000),
        map: Box::default(),
        tiles: (0..3).map(|_| FakeTile::default()).collect(),
        tile_array: Box::new([0; 4]),
        visibility: Box::new([0; 6]),
        nodes: (0..2).map(|_| FakeNode::default()).collect(),
    };
    // the class and stats
    f.class.fields.mCategory = 6;
    f.class.fields.mUnitType = 0x2e7;
    f.class.fields.mFamily = 9;
    f.class.fields.mRange = 4.5;
    f.class.fields.mUnloadRange = 2.0;
    f.stats.fields.mRange = 5.5;
    f.stats.fields.mScale = 1.25;
    f.stats.fields.mMaxHitPoints = 150;
    f.goal.kind = 11;
    f.goal.id = 0x1234;
    // the map: 2 x 2, tile (1, 0) missing
    f.map.rts.mSize = 2;
    f.map.rts.mU19561C = 3;
    f.map.rts.mL195610 = 7;
    f.map.rts.mL195614 = 8;
    f.map.fields.mMapCharacteristics = 0x40;
    let tile_addrs: Vec<usize> = f.tiles.iter().map(addr).collect();
    *f.tile_array = [tile_addrs[0], 0, tile_addrs[1], tile_addrs[2]];
    f.map.tiles = f.tile_array.as_ptr() as usize;
    for (i, t) in f.tiles.iter_mut().enumerate() {
        t.terrain = i as c_long + 1;
        t.height = 0.5 * i as f32;
        t.ai = -3 + i as i8;
        t.visibility = f.visibility.as_ptr() as usize + 8 * i;
    }
    f.visibility[0] = 0x0002_0102;
    f.visibility[1] = 0x0000_00ff;
    // the units
    f.tree.kind = 1;
    f.tree.rts.mUnitID = 0x0100_0001;
    f.tree.rts.mTileX = 0;
    f.tree.rts.mTileY = 0;
    f.tree.rts.mIsValid = true;
    f.tree.unit.mHitPoints = 40;
    f.tree.unit.mResourceAmount = 300;
    f.tree.unit.mResourceType = 2;
    f.tree.unit.mHiddenFlags = 0x10;
    f.soldier.kind = 2;
    f.soldier.rts.mUnitID = 0x0100_0002;
    f.soldier.rts.mTileX = 1;
    f.soldier.rts.mTileY = 1;
    f.soldier.rts.mIsValid = true;
    f.soldier.unit.mHitPoints = 90;
    f.soldier.class = addr(&*f.class);
    f.soldier.stats = addr(&*f.stats);
    f.soldier.complex_unit = addr(&*f.soldier);
    f.soldier.complex.mHeight = 1.5;
    f.soldier.complex.mSight = 8.0;
    f.soldier.complex.mFacing = 3;
    f.soldier.complex.mExperience = 7;
    f.soldier.complex.mPower = -2;
    f.soldier.location[0][3] = 10.0;
    f.soldier.location[1][3] = 20.0;
    f.soldier.location[2][3] = 30.0;
    f.soldier.command = addr(&*f.action);
    f.soldier.goal = addr(&*f.goal);
    f.soldier.container = addr(&*f.tree);
    f.cargo.kind = 2;
    f.cargo.rts.mUnitID = 0x0100_0003;
    f.cargo.complex_unit = addr(&*f.cargo);
    f.cargo_list[0] = addr(&*f.cargo);
    let queue: &'static [c_long; 4] = Box::leak(Box::new([40, 1, 41, 0]));
    let qb = queue.as_ptr() as usize;
    f.soldier.build_queue = [qb, qb + 4 * std::mem::size_of::<c_long>(), 0];
    f.soldier.next_part = addr(&*f.cargo);
    f.soldier.complex.mCurrentResearch = 40;
    f.class.fields.mTechnology = 12;
    f.class.fields.mIsWall = true;
    f.soldier.cargo = [f.cargo_list.as_ptr() as usize, f.cargo_list.as_ptr() as usize + 16, 0];
    // a bool byte that is neither 0 nor 1 (a hand-edited save's)
    // SAFETY: the member is a byte of the box
    unsafe { std::ptr::write(std::ptr::addr_of_mut!(f.soldier.complex.mIsBuilt).cast::<u8>(), 2) };
    // the player's list: the tree, then the soldier (the cargo is in no list)
    f.player.rts.mPlayerID = 1;
    f.player.rts.mSlot = 0xff;
    f.player.rts.mSeesAll = true;
    f.player.color = [0.25, 0.5, 0.75];
    f.player.saved.mEnemies[2] = 1;
    f.player.saved.mResourceCaps[3] = 500;
    f.player.fields.mTributeFee = 0.4;
    f.player.selected_group = 0x77;
    f.player.first_unit = addr(&*f.tree);
    f.tree.next = addr(&*f.soldier);
    let player = addr(&*f.player);
    f.tree.player = player;
    f.soldier.player = player;
    f.tree.tile = tile_addrs[0];
    f.soldier.tile = tile_addrs[2];
    // tile 0 holds the tree and a node of no unit; tile 2 the soldier
    f.nodes[0].unit = addr(&*f.tree);
    f.nodes[0].next = addr(&f.nodes[1]);
    f.tiles[0].units = addr(&f.nodes[0]);
    // the world: slot 1 and the map
    f.world.players[1] = player;
    f.world.map = addr(&*f.map);
    f.world.num_players = 2;
    f.world.current = 1;
    f.world.speed = 1.5;
    f.world.population_limit = 800;
    f.world.wonders = 1;
    f.world.flags = [true, false, false, true, false, false, true];
    let ghosts = Box::leak(Box::new([0x99usize]));
    f.world.ghosts = [ghosts.as_ptr() as usize, ghosts.as_ptr() as usize + 8, 0];
    f
}

#[test]
fn a_layout_of_another_size_is_refused() {
    let l = EeWorldLayout { size: 8, ..Default::default() };
    assert!(!layout::set(l));
    // the registered layout is the C++'s only: none in the unit tests
    assert_eq!(layout::get(), None);
    let _guard = with_test_layout(fake_layout());
    assert_eq!(layout::get(), Some(&fake_layout()));
}

#[test]
fn tokens_are_counted_per_thread_in_debug_builds() {
    let before = read_depth();
    // SAFETY: nothing is read
    let a = unsafe { WorldRead::begin() };
    // SAFETY: as above
    let b = unsafe { WorldRead::begin() };
    super::ee_world_read_begin();
    if cfg!(debug_assertions) {
        assert_eq!(read_depth(), before + 3);
    }
    super::ee_world_read_end();
    drop(a);
    drop(b);
    assert_eq!(read_depth(), before);
}

#[test]
fn bool_members_read_as_bytes() {
    let bytes = [0u8, 2, 1, 0xff];
    // SAFETY: four readable bytes
    let read: [bool; 4] = unsafe { FieldRead::read_field(bytes.as_ptr().cast::<[bool; 4]>()) };
    assert_eq!(read, [false, true, true, true]);
}

#[test]
fn a_unit_view_reads_its_records_and_pointer_members() {
    let _guard = with_test_layout(fake_layout());
    let f = fixture();
    // SAFETY: the fixture lives
    let t = unsafe { WorldRead::begin() };
    // SAFETY: a live fake unit
    let s = unsafe { UnitView::of(&t, addr(&*f.soldier) as u64) }.unwrap();
    assert!(s.is_unit() && s.is_complex());
    assert_eq!((s.id(), s.tile_xy(), s.valid(), s.hit_points()), (0x0100_0002, (1, 1), true, 90));
    assert!(s.is_built(), "the byte 2 is true");
    assert_eq!((s.height(), s.sight(), s.facing(), s.experience(), s.power()), (1.5, 8.0, 3, 7, -2));
    assert_eq!(s.location(), [10.0, 20.0, 30.0]);
    assert_eq!((s.command_type(), s.goal(), s.active_goal()), (5000, Some((11, 0x1234)), None));
    assert_eq!(s.complex_unit().map(|u| u.handle()), Some(s.handle()));
    assert_eq!(s.container().map(|u| u.id()), Some(0x0100_0001));
    assert_eq!((s.cargo_len(), s.cargo(0).map(|u| u.id()), s.cargo_handle(1), s.cargo_handle(2)), (2, Some(0x0100_0003), 0, 0));
    let c = s.class().unwrap();
    assert_eq!((c.category(), c.unit_type(), c.family(), c.range(), c.unload_range()), (6, 0x2e7, 9, 4.5, 2.0));
    assert_eq!((s.build_queue_len(), s.build_queue_technology(1), s.build_queue_technology(2)), (2, Some(41), None));
    assert_eq!((s.previous_part_handle(), s.next_part_handle(), s.current_research()), (0, addr(&*f.cargo) as u64, 40));
    assert_eq!((c.technology(), c.wall(), c.tree(), c.vital()), (12, true, false, false));
    let st = s.stats().unwrap();
    assert_eq!((st.range(), st.scale(), st.max_hit_points()), (5.5, 1.25, 150));
    assert_eq!(s.player().map(|p| p.id()), Some(1));
    assert_eq!(s.tile().map(|t| t.terrain_type()), Some(3));
    // a record member through the macro
    let flags: c_ulong = crate::world_field!(s.complex(), mFacing);
    assert_eq!(flags, 3);
    // an EEUnit: no class, its plain members
    // SAFETY: as above
    let tree = unsafe { UnitView::of(&t, addr(&*f.tree) as u64) }.unwrap();
    assert!(tree.is_unit() && !tree.is_complex());
    assert_eq!((tree.resource_amount(), tree.resource_type(), tree.hidden_flags()), (300, 2, 0x10));
    assert!(tree.class().is_none() && tree.stats().is_none() && tree.complex_unit().is_none());
    // SAFETY: none
    assert!(unsafe { UnitView::of(&t, 0) }.is_none());
    // the class and stats views of their own handles
    // SAFETY: the fixture's
    assert_eq!(unsafe { ClassView::of(&t, addr(&*f.class) as u64) }.map(|c| c.family()), Some(9));
}

#[test]
#[cfg(debug_assertions)]
#[should_panic(expected = "not a unit")]
fn debug_builds_refuse_an_object_of_no_unit_kind() {
    let _guard = with_test_layout(fake_layout());
    let mut u = Box::<FakeUnit>::default();
    u.kind = 7;
    // SAFETY: a readable object
    let t = unsafe { WorldRead::begin() };
    // SAFETY: as above
    let _ = unsafe { UnitView::of(&t, addr(&*u) as u64) };
}

#[test]
#[cfg(debug_assertions)]
#[should_panic(expected = "an EEComplexUnit member")]
fn debug_builds_refuse_a_complex_member_of_an_eeunit() {
    let _guard = with_test_layout(fake_layout());
    let f = fixture();
    // SAFETY: the fixture lives
    let t = unsafe { WorldRead::begin() };
    // SAFETY: as above
    let tree = unsafe { UnitView::of(&t, addr(&*f.tree) as u64) }.unwrap();
    let _ = tree.height();
}

#[test]
fn players_world_map_and_tiles() {
    let _guard = with_test_layout(fake_layout());
    let f = fixture();
    // SAFETY: the fixture lives
    let t = unsafe { WorldRead::begin() };
    // SAFETY: as above
    let w = unsafe { WorldView::of(&t, addr(&*f.world) as u64) }.unwrap();
    assert_eq!((w.num_players(), w.current_player_index()), (2, 1));
    assert!(w.player(0).is_none() && w.player(17).is_none());
    assert_eq!((w.game_speed(), w.population_limit(), w.wonders_to_win()), (1.5, 800, 1));
    assert_eq!(w.game_options_handle(), addr(&f.world.options) as u64);
    assert_eq!((w.ghost_units_len(), w.ghost_unit_handle(0), w.ghost_unit_handle(1)), (1, 0x99, 0));
    let flags = [w.victory_enabled(), w.game_screen_active(), w.loading_saved_game(), w.cheats_enabled(),
        w.in_cinematic(), w.user_paused(), w.using_carbon()];
    assert_eq!(flags, [true, false, false, true, false, false, true]);
    let p = w.current_player().unwrap();
    assert_eq!((p.id(), p.slot(), p.sees_all(), p.observer()), (1, 0xff, true, false));
    assert_eq!((p.color(), p.selected_group_handle(), p.enemy(2), p.enemy(3), p.enemy(99)), ([0.25, 0.5, 0.75], 0x77, 1, 0, 0));
    let ids: Vec<u32> = p.units().map(|u| u.id()).collect();
    assert_eq!(ids, [0x0100_0001, 0x0100_0002]);
    // SAFETY: the fixture's
    assert_eq!(unsafe { PlayerView::of(&t, addr(&*f.player) as u64) }.map(|p| p.id()), Some(1));
    let m = w.map().unwrap();
    assert_eq!((m.size(), m.player_count(), m.visibility_words()), (2, 3, 2));
    assert!(m.tile(1, 0).is_none() && m.tile(2, 0).is_none() && m.tile(-1, 0).is_none() && m.tile(0, 2).is_none());
    let t0 = m.tile(0, 0).unwrap();
    assert_eq!((t0.terrain_type(), t0.height(), t0.ai_value()), (1, 0.0, -3));
    // the players' counts, two to a word, masked to a byte as GetVisibility does
    assert_eq!((t0.visibility(0), t0.visibility(1), t0.visibility(2), t0.visibility(3)), (2, 2, 0xff, 0));
    let on_tile: Vec<Option<u32>> = t0.units().map(|u| u.map(|u| u.id())).collect();
    assert_eq!(on_tile, [Some(0x0100_0001), None]);
    assert_eq!(m.tile(1, 1).unwrap().units().count(), 0);
    // SAFETY: the fixture's
    assert_eq!(unsafe { TileView::of(&t, f.tile_array[3] as u64) }.map(|t| t.terrain_type()), Some(3));
    // SAFETY: as above
    assert_eq!(unsafe { MapView::of(&t, addr(&*f.map) as u64) }.map(|m| m.size()), Some(2));
}

/// FNV-1a 64 of the words, little-endian.
fn fold(words: &[u32]) -> u64 {
    let mut h = 0xcbf2_9ce4_8422_2325u64;
    for w in words {
        for b in w.to_le_bytes() {
            h = (h ^ u64::from(b)).wrapping_mul(0x100_0000_01b3);
        }
    }
    h
}

#[test]
fn the_check_digests_the_cpp_list_in_its_order() {
    let _guard = with_test_layout(fake_layout());
    // an empty world: the counts, the 17 empty slots, no map
    let w0 = Box::<FakeWorld>::default();
    // SAFETY: a live fake world
    let t = unsafe { WorldRead::begin() };
    // SAFETY: as above
    let d = digest(&unsafe { WorldView::of(&t, addr(&*w0) as u64) }.unwrap());
    let mut want = vec![0u32, 0];
    want.extend([0u32; 17]);
    want.push(0);
    assert_eq!(d, EeWorldViewDigest { players: 0, units: 0, tiles: 0, digest: fold(&want) });

    // the fixture, word by word as EEWorldViewCheck.cpp lists them
    let f = fixture();
    // SAFETY: the fixture lives
    let d = digest(&unsafe { WorldView::of(&t, addr(&*f.world) as u64) }.unwrap());
    let b = |v: f32| v.to_bits();
    let mut w: Vec<u32> = vec![2, 1, 0, 1];
    // the player
    w.extend([1, 0, 0, 0xff, 1, 0, 0, b(0.25), b(0.5), b(0.75), 1]);
    w.extend([0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);
    w.extend([0, 0, 0, 500, 0, 0, 0, 0, 0, 0, 0, b(0.4), 0, 0]);
    // the tree: RTSUnit, then EEUnit (no class, no stats, no complex unit)
    w.extend([0x0100_0001, 0, 0, 1, 0, 0, 1, 1, 1, 1]);
    w.extend([2, 40, 300, 0x10, 0xffff_ffff, 0xffff_ffff, 0]);
    // the soldier
    w.extend([0x0100_0002, 1, 1, 1, 0, 0, 2, 1, 3, 1]);
    w.extend([0, 90, 0, 0, 6, 0x2e7, 9, b(4.5), b(5.5), b(1.25), 150, 1]);
    w.extend([b(1.5), b(8.0), 3, 1, 0, 0, 7, 0, (-2i32) as u32, b(10.0), b(20.0), b(30.0), 5000, 11, 0x1234]);
    w.extend([0xffff_ffff, 2, 0x0100_0003, 0]);
    w.push(2);
    // 16 empty slots, the map
    w.extend([0u32; 15]);
    w.push(1);
    w.extend([2, 3, 7, 8, 0x40]);
    // (0, 0): terrain 1, its two words, the tree and an empty node
    w.extend([1, 1, b(0.0), (-3i32) as u32, 0x0002_0102, 0xff, 0x0100_0001, 0, 2]);
    // (1, 0) missing; (0, 1) and (1, 1)
    w.push(0);
    w.extend([1, 2, b(0.5), (-2i32) as u32, 0, 0, 0]);
    w.extend([1, 3, b(1.0), (-1i32) as u32, 0, 0, 0]);
    assert_eq!(d, EeWorldViewDigest { players: 1, units: 2, tiles: 3, digest: fold(&w) });
}

#[test]
fn a_unit_by_id_is_the_stores_object() {
    let _guard = with_test_layout(fake_layout());
    let f = fixture();
    let id = 0x7e00_0001;
    let object = addr(&*f.soldier) as u64;
    crate::units::store::with_store(|s| {
        s.insert(crate::units::store::EeUnitRecord { id, object, ..Default::default() })
    });
    // SAFETY: the fixture lives
    let t = unsafe { WorldRead::begin() };
    assert_eq!(UnitView::by_id(&t, id).map(|u| u.handle()), Some(object));
    assert!(UnitView::by_id(&t, id + 1).is_none());
    crate::units::store::with_store(|s| s.remove(id, object));
}
