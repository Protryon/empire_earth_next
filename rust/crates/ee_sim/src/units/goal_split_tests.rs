use super::*;
use std::collections::BTreeMap;

/// A recording host: each statement asked (op, a, b, the first ints), its answer from `answers` (0 by default).
#[derive(Default)]
struct Mock {
    log: Vec<(i32, u64, u64, [i64; 4])>,
    answers: BTreeMap<(i32, u64), i64>,
    reads: BTreeMap<(u64, u8), i64>,
    positions: BTreeMap<u64, [f32; 3]>,
    deposits: Vec<(u64, u64)>,
    float_answer: f32,
}

impl Mock {
    fn answer(mut self, op: i32, a: u64, value: i64) -> Self {
        self.answers.insert((op, a), value);
        self
    }
    fn with_read(mut self, unit: u64, what: UnitRead, value: i64) -> Self {
        self.reads.insert((unit, what as u8), value);
        self
    }
    fn ops(&self) -> Vec<i32> {
        self.log.iter().map(|e| e.0).collect()
    }
}

impl GoalSplitHost for Mock {
    fn raw(&mut self, op: i32, a: u64, b: u64, io: &mut EeUnitsGoalIo) -> i64 {
        self.log.push((op, a, b, [io.i[0], io.i[1], io.i[2], io.i[3]]));
        match op {
            EE_UNITS_GOAL_HOST_X_WORK_TIME_FACTOR | EE_UNITS_GOAL_HOST_X_SOUND_PARAMETERS => {
                io.f[0] = self.float_answer;
            }
            EE_UNITS_GOAL_HOST_X_TRANSPORT_UNLOAD_TILES | EE_UNITS_GOAL_HOST_X_FIND_RENDEZVOUS => {
                // the C++ writes its tiles in place
                let tiles = io.text as *mut [c_long; 2];
                // SAFETY: the split's buffer of four tiles
                unsafe {
                    (*tiles.add(1))[0] += 10;
                }
            }
            EE_UNITS_GOAL_HOST_X_AREA_RANDOM_POINT => {
                io.i[0] = 7;
                io.i[1] = 8;
            }
            _ => {}
        }
        *self.answers.get(&(op, a)).unwrap_or(&0)
    }
    fn read(&mut self, unit: u64, what: UnitRead) -> i64 {
        *self.reads.get(&(unit, what as u8)).unwrap_or(&0)
    }
    fn position(&mut self, unit: u64) -> [f32; 3] {
        *self.positions.get(&unit).unwrap_or(&[0.0; 3])
    }
    fn deposit_load(&mut self, unit: u64, container: u64) {
        self.deposits.push((unit, container));
    }
    fn resource_index(&mut self, unit: u64) -> i64 {
        unit as i64 % 7
    }
}

fn io_ints(ints: &[i64]) -> EeUnitsGoalIo {
    let mut io = EeUnitsGoalIo::default();
    io.i[..ints.len()].copy_from_slice(ints);
    io
}

#[test]
fn a_plain_operation_is_not_split() {
    let mut h = Mock::default();
    assert_eq!(split(&mut h, EE_UNITS_GOAL_HOST_DROP_PATH, 1, 0, &mut EeUnitsGoalIo::default()), None);
    assert!(h.log.is_empty());
}

#[test]
fn start_action_changes_the_action_unless_asked_not_to() {
    let mut h = Mock::default();
    split(&mut h, EE_UNITS_GOAL_HOST_START_ACTION, 5, 0, &mut io_ints(&[6000, 0]));
    assert_eq!(h.ops(), [EE_UNITS_GOAL_HOST_X_START_ACTION, EE_UNITS_GOAL_HOST_ACTION_CHANGED]);
    assert_eq!(h.log[0].3[0], 6000);
    let mut h = Mock::default();
    split(&mut h, EE_UNITS_GOAL_HOST_START_ACTION, 5, 0, &mut io_ints(&[6000, 1]));
    assert_eq!(h.ops(), [EE_UNITS_GOAL_HOST_X_START_ACTION]);
}

#[test]
fn find_unit_and_ghost_pick_their_statement() {
    let mut h = Mock::default().answer(EE_UNITS_GOAL_HOST_X_FIND_COMPLEX, 9, 99);
    assert_eq!(split(&mut h, EE_UNITS_GOAL_HOST_FIND_UNIT, 9, 1, &mut EeUnitsGoalIo::default()), Some(99));
    split(&mut h, EE_UNITS_GOAL_HOST_FIND_UNIT, 9, 0, &mut EeUnitsGoalIo::default());
    split(&mut h, EE_UNITS_GOAL_HOST_FIND_GHOST, 9, 1, &mut EeUnitsGoalIo::default());
    split(&mut h, EE_UNITS_GOAL_HOST_FIND_GHOST, 9, 0, &mut EeUnitsGoalIo::default());
    assert_eq!(
        h.ops(),
        [
            EE_UNITS_GOAL_HOST_X_FIND_COMPLEX,
            EE_UNITS_GOAL_HOST_X_FIND_PLAIN,
            EE_UNITS_GOAL_HOST_X_FIND_LOADED_GHOST,
            EE_UNITS_GOAL_HOST_X_FIND_LOADED_GHOST_UNIT
        ]
    );
}

#[test]
fn tell_group_only_with_a_group() {
    let mut h = Mock::default();
    split(&mut h, EE_UNITS_GOAL_HOST_TELL_GROUP, 3, 0, &mut EeUnitsGoalIo::default());
    assert!(h.log.is_empty());
    let mut h = Mock::default().with_read(3, UnitRead::ActiveGroup, 77);
    split(&mut h, EE_UNITS_GOAL_HOST_TELL_GROUP, 3, 0, &mut EeUnitsGoalIo::default());
    assert_eq!(h.ops(), [EE_UNITS_GOAL_HOST_X_TELL_GROUP]);
}

#[test]
fn the_file_calls_go_by_kind() {
    let mut h = Mock::default();
    for kind in [0, 1, 3, 2] {
        split(&mut h, EE_UNITS_GOAL_HOST_WRITE, 0, 0, &mut io_ints(&[kind, 5]));
        split(&mut h, EE_UNITS_GOAL_HOST_READ, 0, 0, &mut io_ints(&[kind, 5]));
    }
    assert_eq!(
        h.ops(),
        [
            EE_UNITS_GOAL_HOST_X_WRITE_U32,
            EE_UNITS_GOAL_HOST_X_READ_U32,
            EE_UNITS_GOAL_HOST_X_WRITE_I32,
            EE_UNITS_GOAL_HOST_X_READ_I32,
            EE_UNITS_GOAL_HOST_X_WRITE_F32,
            EE_UNITS_GOAL_HOST_X_READ_F32,
            EE_UNITS_GOAL_HOST_X_WRITE_BOOL,
            EE_UNITS_GOAL_HOST_X_READ_BOOL
        ]
    );
    let mut h = Mock::default().answer(EE_UNITS_GOAL_HOST_X_HAS_TEXT, 0, 1);
    split(&mut h, EE_UNITS_GOAL_HOST_APPEND, 0, 0, &mut EeUnitsGoalIo::default());
    assert_eq!(h.ops(), [EE_UNITS_GOAL_HOST_X_HAS_TEXT, EE_UNITS_GOAL_HOST_X_APPEND_TEXT]);
    let mut h = Mock::default();
    split(&mut h, EE_UNITS_GOAL_HOST_APPEND, 0, 0, &mut EeUnitsGoalIo::default());
    assert_eq!(h.ops(), [EE_UNITS_GOAL_HOST_X_HAS_TEXT, EE_UNITS_GOAL_HOST_X_APPEND_DUMP]);
}

#[test]
fn the_calamity_class_of_a_technology() {
    let mut h = Mock::default();
    let mut io = io_ints(&[12]);
    assert_eq!(split(&mut h, EE_UNITS_GOAL_HOST_CALAMITY_CLASS, 4, 0, &mut io), Some(0));
    assert_eq!(h.ops(), [EE_UNITS_GOAL_HOST_X_PLAYER_TECH]);
    let mut h = Mock::default()
        .answer(EE_UNITS_GOAL_HOST_X_PLAYER_TECH, 4, 40)
        .answer(EE_UNITS_GOAL_HOST_X_NODE_CALAMITY, 40, 23)
        .answer(EE_UNITS_GOAL_HOST_X_CALAMITY_CLASS_OF, 0, 230);
    assert_eq!(split(&mut h, EE_UNITS_GOAL_HOST_CALAMITY_CLASS, 4, 0, &mut io), Some(230));
    assert_eq!((io.i[1], io.i[2]), (23, 23));
    assert_eq!(h.log[2].3[0], 23);
}

#[test]
fn create_calamity_by_target_kind() {
    let mut h = Mock::default().with_read(4, UnitRead::Id, 0x104).with_read(6, UnitRead::Id, 0x206).answer(
        EE_UNITS_GOAL_HOST_X_UNIT_PLAYER_ID,
        8,
        3,
    );
    split(&mut h, EE_UNITS_GOAL_HOST_CREATE_CALAMITY, 4, 6, &mut io_ints(&[3, 17, 0, 0, 8]));
    split(&mut h, EE_UNITS_GOAL_HOST_CREATE_CALAMITY, 4, 6, &mut io_ints(&[2, 17, 30, 31, 8]));
    split(&mut h, EE_UNITS_GOAL_HOST_CREATE_CALAMITY, 4, 6, &mut io_ints(&[1, 17, 0, 0, 8]));
    assert_eq!(h.log[1], (EE_UNITS_GOAL_HOST_X_CREATE_UNIT_CALAMITY, 0, 0, [17, 0x104, 0x206, 3]));
    assert_eq!(h.log[3], (EE_UNITS_GOAL_HOST_X_CREATE_LOCATION_CALAMITY, 0, 0, [17, 0x104, 30, 31]));
    assert_eq!(h.log[5], (EE_UNITS_GOAL_HOST_X_CREATE_GLOBAL_CALAMITY, 0, 0, [17, 0x104, 0, 3]));
}

#[test]
fn contact_range_hands_the_squared_distance() {
    let mut h = Mock::default();
    h.positions.insert(1, [1.0, 2.0, 3.0]);
    h.positions.insert(2, [4.0, 6.0, 3.0]);
    struct Spy(Mock, f32);
    impl GoalSplitHost for Spy {
        fn raw(&mut self, op: i32, a: u64, b: u64, io: &mut EeUnitsGoalIo) -> i64 {
            self.1 = io.f[0];
            self.0.raw(op, a, b, io)
        }
        fn read(&mut self, unit: u64, what: UnitRead) -> i64 {
            self.0.read(unit, what)
        }
        fn position(&mut self, unit: u64) -> [f32; 3] {
            self.0.position(unit)
        }
        fn deposit_load(&mut self, unit: u64, container: u64) {
            self.0.deposit_load(unit, container)
        }
        fn resource_index(&mut self, unit: u64) -> i64 {
            self.0.resource_index(unit)
        }
    }
    let mut s = Spy(h, 0.0);
    split(&mut s, EE_UNITS_GOAL_HOST_CONTACT_RANGE, 1, 2, &mut EeUnitsGoalIo::default());
    assert_eq!(s.1, 25.0);
    assert_eq!(s.0.ops(), [EE_UNITS_GOAL_HOST_X_IN_CONTACT_RANGE]);
}

#[test]
fn the_explore_goals_reads() {
    let mut h =
        Mock::default().answer(EE_UNITS_GOAL_HOST_X_PATH_AREA_AT, 2, 50).answer(EE_UNITS_GOAL_HOST_X_AREA_LEFT, 50, 9);
    let mut io = EeUnitsGoalIo::default();
    assert_eq!(split(&mut h, EE_UNITS_GOAL_HOST_PATH_AREA_BOUNDS, 2, 0, &mut io), Some(1));
    assert_eq!(io.i[2], 9);
    // a visible tile is screened; a hidden one only for a flier off a space map
    let mut h =
        Mock::default().answer(EE_UNITS_GOAL_HOST_X_MAP_TILE, 0, 70).answer(EE_UNITS_GOAL_HOST_X_TILE_VISIBLE, 2, 1);
    assert_eq!(split(&mut h, EE_UNITS_GOAL_HOST_TILE_SCREEN, 2, 0, &mut io_ints(&[3, 4])), Some(1));
    let mut h =
        Mock::default().answer(EE_UNITS_GOAL_HOST_X_MAP_TILE, 0, 70).answer(EE_UNITS_GOAL_HOST_X_CLASS_DOMAINS, 2, 1);
    assert_eq!(split(&mut h, EE_UNITS_GOAL_HOST_TILE_SCREEN, 2, 0, &mut io_ints(&[3, 4])), Some(1));
    assert_eq!(h.ops().last(), Some(&EE_UNITS_GOAL_HOST_X_SPACE_MAP));
    // a ground unit's point on its continent, a flier's on a random one (the draw)
    let mut h = Mock::default().answer(EE_UNITS_GOAL_HOST_X_CONTINENT_AT, 2, 80);
    let mut io = EeUnitsGoalIo::default();
    assert_eq!(split(&mut h, EE_UNITS_GOAL_HOST_CONTINENT_POINT, 2, 0, &mut io), Some(1));
    assert_eq!((io.i[0], io.i[1]), (7, 8));
    let mut h = Mock::default().answer(EE_UNITS_GOAL_HOST_X_CLASS_DOMAINS, 2, 1);
    assert_eq!(split(&mut h, EE_UNITS_GOAL_HOST_CONTINENT_POINT, 2, 0, &mut io), Some(0));
    assert!(h.ops().contains(&EE_UNITS_GOAL_HOST_X_RANDOM_CONTINENT));
}

#[test]
fn the_unload_tiles_go_through_the_buffer() {
    let mut h = Mock::default().answer(EE_UNITS_GOAL_HOST_X_IS_TRANSPORT, 2, 1);
    let mut io = io_ints(&[1, 2, 3, 4, 5, 6, 7, 8]);
    split(&mut h, EE_UNITS_GOAL_HOST_FIND_UNLOAD_TILES, 2, 0, &mut io);
    assert_eq!(io.i, [1, 2, 13, 4, 5, 6, 7, 8]);
    assert!(io.text.is_null());
    let mut h = Mock::default();
    split(&mut h, EE_UNITS_GOAL_HOST_FIND_UNLOAD_TILES, 2, 0, &mut io);
    assert_eq!(h.ops(), [EE_UNITS_GOAL_HOST_X_IS_TRANSPORT, EE_UNITS_GOAL_HOST_X_AIRPLANE_UNLOAD_TILES]);
    // the transport base's rendezvous: the unit's own tile twice
    let mut h = Mock::default().with_read(2, UnitRead::TileX, 11).with_read(2, UnitRead::TileY, 12);
    let mut io = io_ints(&[0, 0, 0, 0, 1]);
    split(&mut h, EE_UNITS_GOAL_HOST_RENDEZVOUS, 2, 0, &mut io);
    assert_eq!(&io.i[..4], &[11, 12, 11, 12]);
    assert!(h.log.is_empty());
}

#[test]
fn sounds_play_with_the_units_parameters() {
    let mut h = Mock::default().answer(EE_UNITS_GOAL_HOST_X_SOUND_GET, 0, 33);
    split(&mut h, EE_UNITS_GOAL_HOST_UNLOAD_SOUND, 2, 0, &mut io_ints(&[0]));
    assert_eq!(h.log[0].3[0], 0x184);
    assert_eq!(
        h.ops(),
        [EE_UNITS_GOAL_HOST_X_SOUND_GET, EE_UNITS_GOAL_HOST_X_SOUND_PARAMETERS, EE_UNITS_GOAL_HOST_X_SOUND_PLAY]
    );
    let mut h = Mock::default().answer(EE_UNITS_GOAL_HOST_X_SPACE_UNLOAD_SOUND, 0, 498);
    split(&mut h, EE_UNITS_GOAL_HOST_UNLOAD_SOUND, 2, 0, &mut io_ints(&[1]));
    assert_eq!(h.ops(), [EE_UNITS_GOAL_HOST_X_SPACE_UNLOAD_SOUND, EE_UNITS_GOAL_HOST_X_SOUND_GET]);
    assert_eq!(h.log[1].3[0], 498);
}

#[test]
fn the_garrison_upgrade() {
    let mut h = Mock::default()
        .answer(EE_UNITS_GOAL_HOST_X_TECH_TREE_GET, 2, 40)
        .answer(EE_UNITS_GOAL_HOST_X_NODE_NEW_CLASS, 40, 15)
        .answer(EE_UNITS_GOAL_HOST_X_CLASS_TABLE, 0, 150);
    let mut io = io_ints(&[9]);
    split(&mut h, EE_UNITS_GOAL_HOST_GARRISON_UPGRADE, 2, 0, &mut io);
    assert_eq!(&io.i[..3], &[1, 15, 0]);
    assert_eq!(
        h.ops(),
        [
            EE_UNITS_GOAL_HOST_X_TECH_TREE_GET,
            EE_UNITS_GOAL_HOST_X_NODE_NEW_CLASS,
            EE_UNITS_GOAL_HOST_X_CLASS_TABLE,
            EE_UNITS_GOAL_HOST_X_CLASS_TECHNOLOGY,
            EE_UNITS_GOAL_HOST_X_TECH_TREE_GET,
            EE_UNITS_GOAL_HOST_X_NODE_AVAILABLE
        ]
    );
}

#[test]
fn a_deposit_needs_a_load_the_container_takes() {
    let mut h = Mock::default().with_read(2, UnitRead::ResourceAmount, 5).with_read(2, UnitRead::ResourceType, 3);
    split(&mut h, EE_UNITS_GOAL_HOST_DEPOSIT, 2, 9, &mut EeUnitsGoalIo::default());
    assert_eq!(h.log[0].3[0], 3);
    assert!(h.deposits.is_empty());
    let mut h = h.answer(EE_UNITS_GOAL_HOST_X_CAN_TAKE, 2, 1);
    split(&mut h, EE_UNITS_GOAL_HOST_DEPOSIT, 2, 9, &mut EeUnitsGoalIo::default());
    assert_eq!(h.deposits, [(2, 9)]);
}

#[test]
fn completing_a_building() {
    let mut h = Mock::default()
        .answer(EE_UNITS_GOAL_HOST_X_UNIT_PLAYER, 2, 60)
        .answer(EE_UNITS_GOAL_HOST_X_PLAYER_HAS_TECH, 60, 1)
        .answer(EE_UNITS_GOAL_HOST_X_CLASS_UNIT_TYPE, 9, 0x2fe)
        .answer(EE_UNITS_GOAL_HOST_X_PLAYER_TECH_AVAILABLE, 60, 1)
        .answer(EE_UNITS_GOAL_HOST_X_CURRENT_PLAYER, 0, 60)
        .answer(EE_UNITS_GOAL_HOST_X_CREATION_SOUND, 9, 5)
        .answer(EE_UNITS_GOAL_HOST_X_MAIN_GAME_FORM, 0, 1);
    split(&mut h, EE_UNITS_GOAL_HOST_COMPLETE_BUILDING, 2, 9, &mut io_ints(&[1]));
    assert_eq!(
        h.ops(),
        [
            EE_UNITS_GOAL_HOST_X_UNIT_PLAYER,
            EE_UNITS_GOAL_HOST_X_PLAYER_HAS_TECH,
            EE_UNITS_GOAL_HOST_X_RESEARCH_CLASS,
            EE_UNITS_GOAL_HOST_X_CHECK_BUTTONS,
            EE_UNITS_GOAL_HOST_X_SET_BUILT,
            EE_UNITS_GOAL_HOST_X_CLASS_UNIT_TYPE,
            EE_UNITS_GOAL_HOST_X_PLAYER_TECH_AVAILABLE,
            EE_UNITS_GOAL_HOST_X_SPACE_WALL,
            EE_UNITS_GOAL_HOST_X_CURRENT_PLAYER,
            EE_UNITS_GOAL_HOST_X_CREATION_SOUND,
            EE_UNITS_GOAL_HOST_X_PLAY_SAMPLE,
            EE_UNITS_GOAL_HOST_X_MAIN_GAME_FORM,
            EE_UNITS_GOAL_HOST_X_REFRESH_GROUP
        ]
    );
    assert_eq!(h.log[6].3[0], 15013);
}

#[test]
fn the_parts_loops() {
    let mut h =
        Mock::default().answer(EE_UNITS_GOAL_HOST_X_NEXT_PART, 2, 3).answer(EE_UNITS_GOAL_HOST_X_NEXT_PART, 3, 4);
    split(&mut h, EE_UNITS_GOAL_HOST_FILL_PARTS, 2, 0, &mut EeUnitsGoalIo::default());
    let filled: Vec<u64> = h.log.iter().filter(|e| e.0 == EE_UNITS_GOAL_HOST_X_FULL_HIT_POINTS).map(|e| e.1).collect();
    assert_eq!(filled, [3, 4]);
    let mut h = Mock::default()
        .answer(EE_UNITS_GOAL_HOST_X_NEXT_PART, 2, 3)
        .answer(EE_UNITS_GOAL_HOST_X_UNIT_PLAYER, 2, 1)
        .answer(EE_UNITS_GOAL_HOST_X_SOUND_GET, 0, 8);
    split(&mut h, EE_UNITS_GOAL_HOST_FOUNDATION_STARTED, 2, 9, &mut io_ints(&[1]));
    assert_eq!(
        h.ops(),
        [
            EE_UNITS_GOAL_HOST_X_UNIT_PLAYER,
            EE_UNITS_GOAL_HOST_X_CURRENT_PLAYER,
            EE_UNITS_GOAL_HOST_X_WONDER_CHAT,
            EE_UNITS_GOAL_HOST_X_SOUND_GET,
            EE_UNITS_GOAL_HOST_X_SOUND_PLAY_PLAIN,
            EE_UNITS_GOAL_HOST_X_LOAD_CLASS_MEDIA,
            EE_UNITS_GOAL_HOST_X_LOAD_MEDIA,
            EE_UNITS_GOAL_HOST_X_POST_UNLOAD_CLASS,
            EE_UNITS_GOAL_HOST_X_NEXT_PART,
            EE_UNITS_GOAL_HOST_SET_HIT_POINTS,
            EE_UNITS_GOAL_HOST_X_LOAD_CLASS_MEDIA,
            EE_UNITS_GOAL_HOST_X_LOAD_MEDIA,
            EE_UNITS_GOAL_HOST_X_POST_UNLOAD_CLASS,
            EE_UNITS_GOAL_HOST_X_PART_FINISHED,
            EE_UNITS_GOAL_HOST_X_NEXT_PART,
            EE_UNITS_GOAL_HOST_X_PART_FINISHED
        ]
    );
    assert_eq!((h.log[10].1, h.log[10].2), (3, 3));
}

#[test]
fn cancelling_a_foundation_refunds_five_resources() {
    let mut h = Mock::default().answer(EE_UNITS_GOAL_HOST_X_SCALED_COST, 9, 25);
    split(&mut h, EE_UNITS_GOAL_HOST_CANCEL_FOUNDATION, 2, 9, &mut io_ints(&[40]));
    assert_eq!(h.log.len(), 11);
    assert_eq!(h.log[1], (EE_UNITS_GOAL_HOST_X_SCALED_COST, 9, 40, [2, 0, 0, 0]));
    assert_eq!(h.log[10], (EE_UNITS_GOAL_HOST_X_ADD_RESOURCE, 9, 0, [6, 25, 0, 0]));
}

#[test]
fn the_storage_search_clears_a_computers_selection() {
    let mut h = Mock::default()
        .with_read(2, UnitRead::CommandType, i64::from(ACTION_FIND_NEW_STORAGE))
        .answer(EE_UNITS_GOAL_HOST_X_COMPUTER_PLAYER, 2, 50)
        .answer(EE_UNITS_GOAL_HOST_X_CP_ACTIVE_GROUP, 50, 51)
        .answer(EE_UNITS_GOAL_HOST_X_GROUP_NUM_UNITS, 51, 2);
    split(&mut h, EE_UNITS_GOAL_HOST_STORAGE_SEARCH, 2, 0, &mut EeUnitsGoalIo::default());
    assert_eq!(
        h.ops(),
        [
            EE_UNITS_GOAL_HOST_RELEASE_FIRE,
            EE_UNITS_GOAL_HOST_X_IS_HUMAN,
            EE_UNITS_GOAL_HOST_X_COMPUTER_PLAYER,
            EE_UNITS_GOAL_HOST_X_CP_ACTIVE_GROUP,
            EE_UNITS_GOAL_HOST_X_GROUP_NUM_UNITS,
            EE_UNITS_GOAL_HOST_X_GROUP_CLEAR,
            EE_UNITS_GOAL_HOST_X_CITIZEN_FIND_STORAGE
        ]
    );
    assert_eq!(h.log[6].3[0], 1);
}

#[test]
fn the_hunt_and_the_gathering() {
    let mut h = Mock::default().with_read(9, UnitRead::HitPoints, 10);
    assert_eq!(split(&mut h, EE_UNITS_GOAL_HOST_HUNT, 2, 9, &mut io_ints(&[1])), Some(0));
    let mut h = h.answer(EE_UNITS_GOAL_HOST_X_FIND_ACTION, 2, 1);
    assert_eq!(split(&mut h, EE_UNITS_GOAL_HOST_HUNT, 2, 9, &mut io_ints(&[1])), Some(1));
    assert_eq!(
        &h.ops()[1..],
        [
            EE_UNITS_GOAL_HOST_X_FIND_ACTION,
            EE_UNITS_GOAL_HOST_X_SET_ATTACK_GOAL,
            EE_UNITS_GOAL_HOST_X_START_ACTION,
            EE_UNITS_GOAL_HOST_ACTION_CHANGED
        ]
    );
    let mut h = Mock::default();
    split(&mut h, EE_UNITS_GOAL_HOST_TAKE_ONE, 9, 2, &mut EeUnitsGoalIo::default());
    assert_eq!(h.log[2], (EE_UNITS_GOAL_HOST_X_SET_LAST_RESOURCE, 2, 9, [0; 4]));
    // the work timer: the variant's factor for all but stone
    let mut h = Mock { float_answer: 1.5, ..Default::default() }.answer(EE_UNITS_GOAL_HOST_X_GATHER_TIME, 2, 3);
    split(&mut h, EE_UNITS_GOAL_HOST_WORK_TIMER, 2, 9, &mut EeUnitsGoalIo::default());
    assert_eq!(h.log.last().unwrap(), &(EE_UNITS_GOAL_HOST_X_SET_WORK_TIMER, 2, 0, [4, 0, 0, 0]));
    assert_eq!(h.log[0].3[0], 2);
    let mut h = Mock { float_answer: 1.5, ..Default::default() }
        .answer(EE_UNITS_GOAL_HOST_X_GATHER_TIME, 2, 3)
        .with_read(9, UnitRead::ResourceType, 4);
    split(&mut h, EE_UNITS_GOAL_HOST_WORK_TIMER, 2, 9, &mut EeUnitsGoalIo::default());
    assert_eq!(h.ops(), [EE_UNITS_GOAL_HOST_X_GATHER_TIME, EE_UNITS_GOAL_HOST_X_SET_WORK_TIMER]);
    assert_eq!(h.log[1].3[0], 3);
}

#[test]
fn the_statements_do_not_overlap_other_operations() {
    let ours: Vec<i32> = (600..=709).collect();
    for op in [
        EE_UNITS_GOAL_HOST_START_ACTION,
        EE_UNITS_GOAL_HOST_RESOURCE_OF_KIND,
        EE_UNITS_GOAL_HOST_BUILD_UNIT,
        crate::combat::goal_objects::EE_COMBAT_G_TELL_GROUP,
    ] {
        assert!(!ours.contains(&op));
    }
}
