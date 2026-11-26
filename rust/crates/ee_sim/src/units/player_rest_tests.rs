use super::*;
use crate::units::player_body::EE_UNITS_PLAYER_BODY_IS_LOCAL;
use crate::units::player_lists::{
    apply_vec, EE_UNITS_LISTS_ALLIES, EE_UNITS_LISTS_HERDS, EE_UNITS_LISTS_PATROLS, EE_UNITS_LISTS_TOWNS,
    EE_UNITS_LISTS_WONDERS,
};

/// A player's world: the answers by operation (the last argument-matching rule wins), and the calls made.
#[derive(Default)]
struct World {
    towns: Vec<u64>,
    herds: Vec<u64>,
    allies: Vec<u64>,
    patrols: Vec<u64>,
    wonders: Vec<u64>,
    warnings: Vec<std::ffi::CString>,
    saved: Option<Box<EeUnitsPlayerSaved>>,
    answers: Vec<(i32, Option<u64>, Option<i64>, i64)>,
    calls: Vec<(i32, u64, i64, i64)>,
    list: Vec<u64>,
    refs: Vec<(char, u64)>,
    complex: bool,
    on_tile: bool,
    points: Vec<u64>,
}

impl World {
    fn answer(mut self, op: i32, a: Option<u64>, b: Option<i64>, v: i64) -> Self {
        self.answers.push((op, a, b, v));
        self
    }
    fn ops(&self) -> Vec<i32> {
        self.calls.iter().map(|c| c.0).collect()
    }
}

impl RestHost for World {
    fn body(&mut self, op: i32, _: u64, a: u64, b: i64, c: i64) -> i64 {
        self.calls.push((op, a, b, c));
        self.answers
            .iter()
            .rev()
            .find(|r| r.0 == op && r.1.is_none_or(|x| x == a) && r.2.is_none_or(|x| x == b))
            .map_or(0, |r| r.3)
    }
    fn list_op(&mut self, _: u64, list: u32, op: ListOp) -> u64 {
        let l = match list {
            EE_UNITS_LISTS_TOWNS => &mut self.towns,
            EE_UNITS_LISTS_HERDS => &mut self.herds,
            EE_UNITS_LISTS_ALLIES => &mut self.allies,
            EE_UNITS_LISTS_PATROLS => &mut self.patrols,
            EE_UNITS_LISTS_WONDERS => &mut self.wonders,
            _ => &mut self.list,
        };
        apply_vec(l, list, op)
    }
    fn media_warning(&mut self, _: u64, text: Vec<u8>) {
        self.warnings.push(std::ffi::CString::from_vec_with_nul(text).unwrap());
    }
    fn media_warnings(&mut self, _: u64) -> Vec<std::ffi::CString> {
        self.warnings.clone()
    }
    fn saved(&mut self, _: u64) -> *mut EeUnitsPlayerSaved {
        let s = self.saved.get_or_insert_with(|| Box::new(unsafe { std::mem::zeroed() }));
        &mut **s
    }
    fn is_complex(&mut self, _: u64) -> bool {
        self.complex
    }
    fn on_tile(&mut self, _: u64) -> bool {
        self.on_tile
    }
    fn tile(&mut self, _: u64) -> (c_long, c_long) {
        (3, 4)
    }
    fn influence(&mut self, _: u64) -> c_long {
        1
    }
    fn pg_reference(&mut self, g: u64) {
        self.refs.push(('+', g));
    }
    fn pg_dereference(&mut self, g: u64) {
        self.refs.push(('-', g));
    }
    fn pg_is_town_patrol(&mut self, g: u64) -> bool {
        g != 0
    }
    fn pg_points(&mut self, _: u64) -> Vec<u64> {
        self.points.clone()
    }
    fn class_size_type(&mut self, _: u64) -> (c_long, c_long) {
        (2, 5)
    }
    fn complex_of(&mut self, u: u64) -> u64 {
        u
    }
    fn location_z(&mut self, _: u64) -> f32 {
        1.0
    }
    fn units_in_rectangle(&mut self, bounds: [f32; 4], _: u64) {
        self.refs.push(('r', bounds[2] as u64));
    }
    fn block_ring(&mut self, _: c_long, _: c_long, _: c_long, _: u64) {
        self.refs.push(('b', 0));
    }
    fn can_stand_on(&mut self, x: c_long, _: c_long, _: u64) -> bool {
        x != 9
    }
    fn movable(&mut self, u: u64) -> bool {
        u != 0x66
    }
    fn loaded_media_classes(&mut self, _: u64) -> Vec<c_long> {
        vec![31, 32]
    }
    fn epoch_range(&mut self, _: u64) -> (c_long, c_long) {
        (2, 9)
    }
    fn reset_tech_tree(&mut self, _: u64) {
        self.refs.push(('t', 0));
    }
    fn refresh_upgrades(&mut self, _: u64) {
        self.refs.push(('u', 0));
    }
    fn player_id(&mut self, _: u64) -> u32 {
        3
    }
    fn unit_player(&mut self, u: u64) -> u64 {
        u & 0xff
    }
    fn c_string(&mut self, _: u64) -> Vec<u8> {
        b"Archer".to_vec()
    }
}

fn pack(x: i64, y: i64) -> i64 {
    (x << 32) | (y & 0xffff_ffff)
}

#[test]
fn the_nearest_town_is_the_last_of_equals_from_the_end() {
    let mut w = World { towns: vec![10, 0, 11, 12], ..Default::default() }
        .answer(EE_UNITS_PLAYER_BODY_TOWN_CLOSEST, Some(10), None, pack(2, 0))
        .answer(EE_UNITS_PLAYER_BODY_TOWN_CLOSEST, Some(11), None, pack(0, -2))
        .answer(EE_UNITS_PLAYER_BODY_TOWN_CLOSEST, Some(12), None, pack(5, 5))
        .answer(EE_UNITS_PLAYER_BODY_TOWN_CONTINENT, Some(11), None, 7);
    // town 11 (distance 4) is found before town 10 (also 4): the strict compare keeps 11
    assert_eq!(nearest_area(&mut w, 1, 0, 0, false), 11);
    // on continent 7 only town 11
    let mut w = World { answers: w.answers.clone(), towns: w.towns.clone(), ..Default::default() }.answer(
        EE_UNITS_PLAYER_BODY_CONTINENT_ID,
        None,
        None,
        7,
    );
    assert_eq!(nearest_area(&mut w, 1, 0, 0, true), 11);
    assert_eq!(nearest_area(&mut World::default(), 1, 0, 0, false), 0);
}

#[test]
fn the_checksum_sums_the_resources_and_population_unsigned() {
    let mut w = World::default();
    unsafe {
        let s = w.saved(1);
        (*s).mResourceCaps = [1, 2, 3, 4, 5, 6, -1];
        (*s).mPopulationCount = 10;
    }
    checksum(&mut w, 1, 99);
    assert_eq!(
        w.ops(),
        vec![EE_UNITS_PLAYER_BODY_RTS_CHECKSUM, EE_UNITS_PLAYER_BODY_TECH_CHECKSUM, EE_UNITS_PLAYER_BODY_ADD_CHECKSUM]
    );
    assert_eq!(w.calls[2], (EE_UNITS_PLAYER_BODY_ADD_CHECKSUM, 99, 30, 0));
}

#[test]
fn herds_of_a_type_and_registering_walk_from_the_end() {
    let mut w = World { herds: vec![5, 6, 7], ..Default::default() }
        .answer(EE_UNITS_PLAYER_BODY_ANIMAL_OF_TYPE, None, None, 90)
        .answer(EE_UNITS_PLAYER_BODY_ANIMAL_ID, Some(90), None, 3)
        .answer(EE_UNITS_PLAYER_BODY_HERD_ANIMAL, None, None, 3)
        .answer(EE_UNITS_PLAYER_BODY_HERD_ANIMAL, Some(6), None, 4);
    herds_of_type(&mut w, 1, 50, 9);
    let pushes: Vec<i64> = w.calls.iter().filter(|c| c.0 == EE_UNITS_PLAYER_BODY_HERDS_PUSH).map(|c| c.2).collect();
    assert_eq!(pushes, vec![7, 5]);
    let mut w = World { herds: vec![5], ..Default::default() };
    herds_of_type(&mut w, 1, 50, 9);
    assert_eq!(w.calls.len(), 1, "no animal: nothing more");
    let mut w = World { towns: vec![1, 0, 2], ..Default::default() };
    register(&mut w, 1, 0);
    let order: Vec<(i32, u64)> = w.calls.iter().map(|c| (c.0, c.1)).collect();
    assert_eq!(
        order,
        vec![
            (EE_UNITS_PLAYER_BODY_TOWN_SERIAL, 2),
            (EE_UNITS_PLAYER_BODY_TOWN_ADD_AREA, 2),
            (EE_UNITS_PLAYER_BODY_TOWN_SERIAL, 1),
            (EE_UNITS_PLAYER_BODY_TOWN_ADD_AREA, 1)
        ]
    );
}

#[test]
fn lists_and_groups() {
    let mut w = World { allies: vec![4, 5, 5], ..Default::default() };
    remove_ally(&mut w, 1, 5);
    assert_eq!(w.allies, vec![4, 5]);
    let mut w = World { patrols: vec![76, 77, 78], ..Default::default() };
    remove_patrol_group(&mut w, 1, 77);
    assert_eq!(w.patrols, vec![76, 78]);
    assert_eq!(w.refs, vec![('-', 77)]);
    let mut w = World { complex: true, ..Default::default() }.answer(EE_UNITS_PLAYER_BODY_GOAL_QUEUES, None, None, 2);
    unit_removed(&mut w, 1, 9);
    assert_eq!(w.calls.iter().filter(|c| c.0 == EE_UNITS_PLAYER_BODY_CLEAR_GOAL_QUEUE).count(), 2);
    let mut w = World::default()
        .answer(EE_UNITS_PLAYER_BODY_DETACHED, None, None, 1)
        .answer(EE_UNITS_PLAYER_BODY_GOALS0_SIZE, None, None, 1)
        .answer(EE_UNITS_PLAYER_BODY_ACTIVE_GROUP, None, None, 40);
    unit_goal_done(&mut w, 1, 9);
    assert!(w.calls.contains(&(EE_UNITS_PLAYER_BODY_SET_DETACHED, 9, 0, 0)));
    assert!(w.calls.contains(&(EE_UNITS_PLAYER_BODY_GROUP_REMOVE, 40, 9, 0)));
}

#[test]
fn class_media_load_and_unload() {
    let mut w = World::default()
        .answer(EE_UNITS_PLAYER_BODY_CLASS_COUNT, None, None, 3)
        .answer(EE_UNITS_PLAYER_BODY_CLASS_STATS, None, Some(1), 100)
        .answer(EE_UNITS_PLAYER_BODY_CLASS_STATS, None, Some(2), 200)
        .answer(EE_UNITS_PLAYER_BODY_STATS_MEDIA_COUNT, Some(200), None, 1)
        .answer(EE_UNITS_PLAYER_BODY_STATS_TYPE, Some(200), None, 2);
    load_all_class_media(&mut w, 1);
    assert_eq!(w.list, vec![2]);
    unload_all_class_media(&mut w, 1);
    assert!(w.list.is_empty());
    assert!(w.calls.contains(&(EE_UNITS_PLAYER_BODY_CLASS_UNLOAD_MEDIA, 0, 2, 0)));
}

#[test]
fn wonder_timers_and_resources() {
    let mut w = World { wonders: vec![30], ..Default::default() }
        .answer(EE_UNITS_PLAYER_BODY_CLOAKED, None, None, 100)
        .answer(EE_UNITS_PLAYER_BODY_GAME_TIME, None, None, 555);
    delay_wonder_timers(&mut w, 1, -10);
    assert!(w.calls.contains(&(EE_UNITS_PLAYER_BODY_SET_CLOAKED, 30, 90, 0)));
    reset_wonder_timers(&mut w, 1, 2);
    assert_eq!(w.calls.iter().filter(|c| *c == &(EE_UNITS_PLAYER_BODY_SET_CLOAKED, 30, 555, 0)).count(), 2);
    assert_eq!(run(&mut w, 1, EE_UNITS_PLAYER_REST_WONDER_TIME, 0, 0, 0), 100);
    set_all_resources(&mut w, 1, 7);
    assert_eq!(unsafe { (*w.saved(1)).mResourceCaps }, [0, 0, 7, 7, 7, 7, 7]);
}

#[test]
fn removing_a_unit_leaves_its_region_only_on_a_tile() {
    let mut w = World::default();
    removing_unit(&mut w, 1, 9);
    assert_eq!(w.ops(), vec![EE_UNITS_PLAYER_BODY_REMOVE_FROM_TILE]);
    let mut w = World { on_tile: true, ..Default::default() };
    removing_unit(&mut w, 1, 9);
    assert_eq!(w.calls.last(), Some(&(EE_UNITS_PLAYER_BODY_INFLUENCE_REMOVE, 9, 3, 4)));
}

#[test]
fn a_game_result_is_announced_once_for_another_player() {
    let mut w = World::default();
    set_game_result(&mut w, 1, 1);
    set_game_result(&mut w, 1, 1);
    assert_eq!(w.calls.iter().filter(|c| c.0 == EE_UNITS_PLAYER_BODY_CHAT_RESULT).count(), 1);
    assert_eq!(unsafe { (*w.saved(1)).mGameResult }, 1);
    set_game_result(&mut w, 1, 0);
    assert_eq!(unsafe { (*w.saved(1)).mGameResult }, 0);
    // the local player is never announced; the headless simulation keeps it undefeated
    let mut w = World::default().answer(EE_UNITS_PLAYER_BODY_IS_LOCAL, None, None, 1).answer(
        EE_UNITS_PLAYER_BODY_NO_HUMAN_DEFEAT,
        None,
        None,
        1,
    );
    set_game_result(&mut w, 1, 2);
    assert!(w.ops().contains(&EE_UNITS_PLAYER_BODY_NO_DEFEAT_HIT));
    assert_eq!(unsafe { (*w.saved(1)).mGameResult }, 0);
}

#[test]
fn a_town_adopts_a_free_town_patrol_on_its_hull() {
    let mut w = World { towns: vec![20, 21], patrols: vec![50, 51], points: vec![60], ..Default::default() }
        .answer(EE_UNITS_PLAYER_BODY_TOWN_PATROL, Some(21), None, 50)
        .answer(EE_UNITS_PLAYER_BODY_HULL_COUNT, None, None, 2)
        .answer(EE_UNITS_PLAYER_BODY_HULL_POINT, None, Some(1), pack(3, 4))
        .answer(EE_UNITS_PLAYER_BODY_TILE_X, None, None, 3)
        .answer(EE_UNITS_PLAYER_BODY_TILE_Y, None, None, 4);
    assert!(adopt_town_patrol(&mut w, 1, 20));
    assert!(w.calls.contains(&(EE_UNITS_PLAYER_BODY_TOWN_SET_PATROL, 20, 51, 0)), "50 is town 21's");
    assert_eq!(w.refs, vec![('+', 51)]);
    let mut w = World::default();
    assert!(!adopt_town_patrol(&mut w, 1, 20));
}

#[test]
fn a_dead_unit_counts_by_its_class() {
    assert_eq!(dead_unit_statistic(1, 0, 5, 1.0), 0, "a resource");
    assert_eq!(dead_unit_statistic(6, 0x11, 5, 1.0), 0, "an animal first");
    assert_eq!(dead_unit_statistic(0x16, 8, 5, 0.0), 14, "a citizen before a hero");
    assert_eq!(dead_unit_statistic(3, 8, 5, 0.0), 15);
    assert_eq!(dead_unit_statistic(3, 2, 5, 2.5), 13);
    assert_eq!(dead_unit_statistic(3, 2, 5, 0.0), 0, "a building does not move");
    assert_eq!(dead_unit_statistic(0xc, 2, 5, 9.0), 0, "a missile");
    assert_eq!(dead_unit_statistic(3, 2, 0xdf, 9.0), 0, "a path point");
}

#[test]
fn a_footprint_is_cleared_onto_waypoints_or_the_free_ring() {
    // the building 0x40 at (3, 4), size 2: units 0x40 (itself), 0x66 (not movable), 0x70 (a waypoint outside), 0x71
    // (no path: every free tile but x 9)
    let mut w = World::default()
        .answer(EE_UNITS_PLAYER_BODY_UNITS_SIZE, None, None, 4)
        .answer(EE_UNITS_PLAYER_BODY_UNITS_AT, None, Some(0), 0x40)
        .answer(EE_UNITS_PLAYER_BODY_UNITS_AT, None, Some(1), 0x66)
        .answer(EE_UNITS_PLAYER_BODY_UNITS_AT, None, Some(2), 0x70)
        .answer(EE_UNITS_PLAYER_BODY_UNITS_AT, None, Some(3), 0x71)
        .answer(EE_UNITS_PLAYER_BODY_WAYPOINTS, Some(0x70), None, 1)
        .answer(EE_UNITS_PLAYER_BODY_TILES_SIZE, None, None, 3)
        .answer(EE_UNITS_PLAYER_BODY_TILES_AT, None, Some(0), pack(9, 4))
        .answer(EE_UNITS_PLAYER_BODY_TILES_AT, None, Some(1), pack(5, 4))
        .answer(EE_UNITS_PLAYER_BODY_TILES_AT, None, Some(2), pack(5, 5))
        .answer(EE_UNITS_PLAYER_BODY_TILE_EMPTY, None, None, 1);
    // the building's tile is (3, 4) (World::tile)
    clear_footprint(&mut w, 1, 0x40);
    assert_eq!(w.refs[0], ('r', 4), "the rectangle's right: 3 + 2 - 1");
    let moves: Vec<u64> = w.calls.iter().filter(|c| c.0 == EE_UNITS_PLAYER_BODY_MOVE_TO).map(|c| c.1).collect();
    // 0x70's next waypoint (0, 0, 0) is outside the rectangle: moved once; 0x71 to both free tiles it can stand on
    assert_eq!(moves, vec![0x70, 0x71, 0x71]);
    assert_eq!(w.calls.iter().filter(|c| c.0 == EE_UNITS_PLAYER_BODY_START_IDLE).count(), 2);
    assert_eq!(w.ops().last(), Some(&EE_UNITS_PLAYER_BODY_TILES_DELETE));
}

#[test]
fn the_tech_tree_reloads_through_a_memory_file() {
    let mut w = World::default().answer(EE_UNITS_PLAYER_BODY_MEMORY_FILE_NEW, None, None, 0x500);
    reload_tech_tree(&mut w, 1, false);
    assert_eq!(w.refs, vec![('t', 0), ('u', 0)]);
    assert!(w.calls.contains(&(EE_UNITS_PLAYER_BODY_TECH_RESET_RANGE, 0, 2, 9)));
    assert!(w.calls.contains(&(EE_UNITS_PLAYER_BODY_TECH_APPLY_SAVED, 0, 1, 0)));
    let unloads: Vec<i64> =
        w.calls.iter().filter(|c| c.0 == EE_UNITS_PLAYER_BODY_UNLOAD_CLASS_MEDIA).map(|c| c.2).collect();
    assert_eq!(unloads, vec![31, 32]);
    assert_eq!(w.calls.last(), Some(&(EE_UNITS_PLAYER_BODY_MEMORY_FILE_DELETE, 0x500, 0, 0)));
    // a failed load applies nothing
    let mut w = World::default().answer(EE_UNITS_PLAYER_BODY_TECH_LOAD, None, None, 5);
    reload_tech_tree(&mut w, 1, true);
    assert!(!w.ops().contains(&EE_UNITS_PLAYER_BODY_TECH_RESET_RANGE));
}

#[test]
fn a_class_not_preloaded_leaves_a_warning() {
    let mut w = World::default().answer(crate::units::player_body::EE_UNITS_PLAYER_BODY_CLASS_STATS, None, None, 0x90);
    media_check(&mut w, 1, 7, true);
    assert_eq!(w.warnings.len(), 1);
    assert_eq!(
        media_warning(b"Archer", true, 3),
        b"The unit Archer BUTTONS might not have been preloaded for player 3 - have a programmer verify this.\n\0"
            .to_vec()
    );
    assert_eq!(&media_warning(b"Archer", false, 3)[..22], b"The unit Archer might ");
    // loaded media: no warning
    let mut w = World::default()
        .answer(crate::units::player_body::EE_UNITS_PLAYER_BODY_CLASS_STATS, None, None, 0x90)
        .answer(EE_UNITS_PLAYER_BODY_STATS_MEDIA_COUNT, None, None, 2);
    media_check(&mut w, 1, 7, false);
    assert!(w.warnings.is_empty());
}

/// Parts 1 -> 2 -> 3 with their flags.
struct Chain {
    flags: Vec<[bool; 2]>,
    writes: Vec<(u64, u32, bool)>,
}

impl CloakHost for Chain {
    fn flag(&mut self, u: u64, which: u32) -> bool {
        self.flags[u as usize - 1][which as usize]
    }
    fn set_flag(&mut self, u: u64, which: u32, value: bool) {
        self.writes.push((u, which, value));
        self.flags[u as usize - 1][which as usize] = value;
    }
    fn next_part(&mut self, u: u64) -> u64 {
        if u < 3 {
            u + 1
        } else {
            0
        }
    }
}

#[test]
fn the_cloak_chains_walk_the_parts_until_one_is_so() {
    let mut c = Chain { flags: vec![[true, true], [true, true], [false, false]], writes: Vec::new() };
    set_cloakable_chain(&mut c, 1, false);
    // cloakable off down the chain (part 3 already off stops it), then each part uncloaked from its own call up
    assert_eq!(c.writes, vec![(1, 0, false), (2, 0, false), (2, 1, false), (1, 1, false)]);
    let mut c = Chain { flags: vec![[false, false], [false, true], [false, false]], writes: Vec::new() };
    set_cloaked_chain(&mut c, 1, true);
    assert_eq!(c.writes, vec![(1, 1, true)], "part 2 already cloaked stops the walk");
    set_cloakable_chain(&mut c, 2, true);
    assert_eq!(c.writes[1..], [(2, 0, true), (3, 0, true)]);
}

#[test]
fn a_herd_dies_from_its_last_unit_and_the_dump_lists_what_is_there() {
    let mut w = World::default()
        .answer(EE_UNITS_PLAYER_BODY_COMPLEXES_NEW, None, None, 0x80)
        .answer(EE_UNITS_PLAYER_BODY_COMPLEXES_SIZE, None, None, 2)
        .answer(EE_UNITS_PLAYER_BODY_COMPLEXES_AT, None, Some(0), 0x41)
        .answer(EE_UNITS_PLAYER_BODY_COMPLEXES_AT, None, Some(1), 0x42);
    kill_herd_units(&mut w, 1, 0x30, false);
    let kills: Vec<u64> = w.calls.iter().filter(|c| c.0 == EE_UNITS_PLAYER_BODY_KILL_KILLED).map(|c| c.1).collect();
    assert_eq!(kills, vec![0x42, 0x41]);
    assert_eq!(w.calls[1], (EE_UNITS_PLAYER_BODY_HERD_UNITS, 0x30, 0x80, 0));
    let mut w = World::default().answer(EE_UNITS_PLAYER_BODY_UPGRADE_SLOTS, None, None, 3).answer(
        EE_UNITS_PLAYER_BODY_UPGRADE_AT,
        None,
        Some(1),
        0x90,
    );
    w.warnings = vec![c"one".to_owned(), c"two".to_owned()];
    dump_media(&mut w, 1, 0x500);
    assert_eq!(w.calls.iter().filter(|c| c.0 == EE_UNITS_PLAYER_BODY_DUMP_COUNT).count(), 1);
    // the title, then each warning
    assert_eq!(w.calls.iter().filter(|c| c.0 == EE_UNITS_PLAYER_BODY_DUMP_TEXT).count(), 3);
}

#[test]
fn only_own_units_repatrol() {
    let mut w = World::default();
    add_to_repatrol(&mut w, 1, 0x201);
    add_to_repatrol(&mut w, 1, 0x202);
    add_to_repatrol(&mut w, 1, 0);
    assert_eq!(w.list, vec![0x201]);
}

#[test]
fn a_color_and_its_shade() {
    let (mut color, mut shade) = ([0f32; 3], [1f32; 3]);
    unsafe { ee_units_player_set_color(&mut color, &mut shade, &[1.0, 0.5, 0.25]) };
    assert_eq!(color, [1.0, 0.5, 0.25]);
    assert_eq!(shade, [1.0f32 * 0.6, 0.5 * 0.6, 0.25 * 0.6]);
}

#[test]
fn a_computer_players_result_names_its_civilization() {
    let mut w = World::default();
    announce_result(&mut w, 1, true);
    let ops = w.ops();
    assert_eq!(ops.iter().filter(|&&o| o == EE_UNITS_PLAYER_BODY_TEXT_NEW).count(), 4);
    assert_eq!(ops.iter().filter(|&&o| o == EE_UNITS_PLAYER_BODY_TEXT_DELETE).count(), 4);
    let at = |op| ops.iter().position(|&o| o == op).unwrap();
    assert!(at(EE_UNITS_PLAYER_BODY_TEXT_CIV_NAME) < at(EE_UNITS_PLAYER_BODY_TEXT_FORMAT));
    assert!(at(EE_UNITS_PLAYER_BODY_TEXT_FORMAT) < at(EE_UNITS_PLAYER_BODY_CHAT_RESULT));
    assert!(!ops.contains(&EE_UNITS_PLAYER_BODY_TEXT_NAME));
}
