//! Recorded statement streams of the composed host cases (`editor_seax.rs`).

use super::*;
use crate::screens::editor_actions::{EE_SCREENS_SEA_TILE, EE_SCREENS_SEU_SELECT};
use crate::screens::editor_whole::EE_SCREENS_SEW_OP_POST;

type Answer = Box<dyn FnMut(u32, &mut SeaxArgs) -> isize>;

/// Records every statement (op, p, q, the first integers) and answers it from the closure.
struct Rec {
    calls: Vec<(u32, usize, usize, [i32; 5])>,
    answer: Answer,
}

impl Rec {
    fn new(answer: impl FnMut(u32, &mut SeaxArgs) -> isize + 'static) -> Self {
        Rec { calls: Vec::new(), answer: Box::new(answer) }
    }
    fn ops(&self) -> Vec<u32> {
        self.calls.iter().map(|c| c.0).collect()
    }
}

impl SeaxHost for Rec {
    fn x(&mut self, op: u32, a: &mut SeaxArgs) -> isize {
        self.calls.push((op, a.p, a.q, [a.i[0], a.i[1], a.i[2], a.i[3], a.i[4]]));
        (self.answer)(op, a)
    }
}

fn identity() -> GeTransformation {
    GeTransformation::IDENTITY
}

#[test]
fn sea_follow_point_terrain_and_plant() {
    let mut h = Rec::new(|op, a| match op {
        EE_SCREENS_SEAX_MESH_VALID_POINT => isize::from(a.i[0] == 1),
        EE_SCREENS_SEAX_MESH_Z => {
            a.f[0] = 2.5;
            0
        }
        EE_SCREENS_SEAX_TERRAIN_AMBIENTS_ONLY => 1,
        EE_SCREENS_SEAX_TERRAIN_NUM_CLASSES => 4,
        EE_SCREENS_SEAX_TERRAIN_CHANCE => 30,
        EE_SCREENS_SEAX_PLANT_CAN_PLACE => 0,
        _ => 0,
    });
    let (mut i, mut f) = ([0i32; 6], [0f32; 5]);
    assert_eq!(sea(&mut h, EE_SCREENS_SEA_STOP_FOLLOW, &mut i, &mut f), Some(0));
    assert_eq!(
        h.ops(),
        [EE_SCREENS_SEAX_FOLLOW_529066, EE_SCREENS_SEAX_CAMERA_FOLLOWING, EE_SCREENS_SEAX_FOLLOW_529068]
    );
    h.calls.clear();
    // an invalid point reads no height
    i[0] = 0;
    assert_eq!(sea(&mut h, EE_SCREENS_SEA_POINT, &mut i, &mut f), Some(0));
    assert_eq!(h.ops(), [EE_SCREENS_SEAX_MESH_VALID_POINT]);
    i[0] = 1;
    assert_eq!(sea(&mut h, EE_SCREENS_SEA_POINT, &mut i, &mut f), Some(1));
    assert_eq!(f[0], 2.5);
    h.calls.clear();
    i = [7, 0, 0, 0, 0, 0];
    assert_eq!(sea(&mut h, EE_SCREENS_SEA_TERRAIN, &mut i, &mut f), Some(30));
    assert_eq!(&i[..3], &[7, 1, 4]);
    h.calls.clear();
    // a refused place makes nothing
    i = [5, 6, 7, 0, 0, 0];
    sea(&mut h, EE_SCREENS_SEA_PLANT, &mut i, &mut f);
    assert_eq!(h.calls, [(EE_SCREENS_SEAX_PLANT_CAN_PLACE, 0, 0, [5, 6, 7, 0, 0])]);
    assert_eq!(sea(&mut h, EE_SCREENS_SEA_TILE, &mut i, &mut f), None);
}

#[test]
fn sea_elevation_mode_and_raise_through_the_longs() {
    let mut longs = [0i64; 4];
    let mut h = Rec::new(move |op, a| match op {
        EE_SCREENS_SEAX_RADIO_CHECKED => isize::from(a.i[0] == EE_SCREENS_SE_ELEVATION_LOWER_BUTTON),
        EE_SCREENS_SEAX_LONG_SET => {
            longs[a.i[0] as usize] = i64::from(a.i[1]);
            0
        }
        EE_SCREENS_SEAX_RAISE => {
            assert_eq!((a.f, a.i[0]), ([1.0, 2.0, 3.0, 4.0], 1));
            longs = [longs[0] - 1, longs[1] + 1, longs[2] - 2, longs[3] + 2];
            0
        }
        EE_SCREENS_SEAX_LONG_GET => longs[a.i[0] as usize] as isize,
        _ => 0,
    });
    let (mut i, mut f) = ([10, 20, 30, 40, 1, 0], [1.0, 2.0, 3.0, 4.0, 0.0]);
    assert_eq!(sea(&mut h, EE_SCREENS_SEA_ELEVATION_MODE, &mut i, &mut f), Some(1));
    assert_eq!(sea(&mut h, EE_SCREENS_SEA_RAISE, &mut i, &mut f), Some(0));
    assert_eq!(&i[..4], &[9, 21, 28, 42]);
}

#[test]
fn tile_units_climb_to_the_first_part_and_reference_each() {
    // tile 100: nodes 1 -> 2; node 1's unit 11 (on a tile, parts 11 <- 12 <- 13 means 11's previous is 12, 12's 13),
    // node 2's unit 21 (no tile node: not climbed)
    let mut h = Rec::new(|op, a| match (op, a.p) {
        (EE_SCREENS_SEAX_TILE, _) => 100,
        (EE_SCREENS_SEAX_TILE_FIRST_NODE, 100) => 1,
        (EE_SCREENS_SEAX_NODE_NEXT, 1) => 2,
        (EE_SCREENS_SEAX_NODE_UNIT, 1) => 11,
        (EE_SCREENS_SEAX_NODE_UNIT, 2) => 21,
        (EE_SCREENS_SEAX_UNIT_ON_TILE, 11) => 1,
        (EE_SCREENS_SEAX_UNIT_PREVIOUS_PART, 11) => 12,
        (EE_SCREENS_SEAX_UNIT_PREVIOUS_PART, 12) => 13,
        (EE_SCREENS_SEAX_UNIT_PATH_POINT, 21) => 1,
        _ => 0,
    });
    assert_eq!(tile_units(&mut h, 3, 4), [(13, false), (21, true)]);
    let refs: Vec<usize> = h.calls.iter().filter(|c| c.0 == EE_SCREENS_SEAX_UNIT_REFERENCE).map(|c| c.1).collect();
    assert_eq!(refs, [13, 21]);
    // off the map: nothing
    let mut none = Rec::new(|_, _| 0);
    assert!(tile_units(&mut none, 3, 4).is_empty());
    assert_eq!(none.ops(), [EE_SCREENS_SEAX_TILE]);
}

#[test]
fn removal_empties_a_resource_first_and_releases_last() {
    let mut h = Rec::new(|op, _| isize::from(op == EE_SCREENS_SEAX_UNIT_RESOURCE));
    remove_unit(&mut h, 9);
    assert_eq!(
        h.ops(),
        [
            EE_SCREENS_SEAX_UNIT_RESOURCE,
            EE_SCREENS_SEAX_UNIT_EMPTY,
            EE_SCREENS_SEAX_UNIT_KILL,
            EE_SCREENS_SEAX_UNIT_PLAYER_KILL,
            EE_SCREENS_SEAX_UNIT_DEREFERENCE
        ]
    );
    let mut h = Rec::new(|_, _| 0);
    assert_eq!(seu(&mut h, EE_SCREENS_SEU_REMOVE_SIMPLE, 9, 0, 0.0), Some(0));
    assert_eq!(h.ops(), [EE_SCREENS_SEAX_UNIT_RESOURCE, EE_SCREENS_SEAX_UNIT_KILL, EE_SCREENS_SEAX_UNIT_PLAYER_KILL]);
}

#[test]
fn sew_pause_waits_with_both_locks_let_go() {
    let mut polls = 0;
    let mut h = Rec::new(move |op, _| {
        if op == EE_SCREENS_SEAX_GRAPHICS_PAUSED {
            polls += 1;
            return isize::from(polls == 3);
        }
        0
    });
    let (mut i, mut f) = ([0i32; 8], [0f32; 4]);
    assert_eq!(sew(&mut h, EE_SCREENS_SEW_OP_PAUSE, 0, &mut i, &mut f), Some(0));
    assert_eq!(
        h.ops(),
        [
            EE_SCREENS_SEAX_PAUSE_POST,
            EE_SCREENS_SEAX_WORLD_WRITE_UNLOCK,
            EE_SCREENS_SEAX_UI_UNLOCK,
            EE_SCREENS_SEAX_GRAPHICS_PAUSED,
            EE_SCREENS_SEAX_SLEEP,
            EE_SCREENS_SEAX_GRAPHICS_PAUSED,
            EE_SCREENS_SEAX_SLEEP,
            EE_SCREENS_SEAX_GRAPHICS_PAUSED,
            EE_SCREENS_SEAX_UI_LOCK,
            EE_SCREENS_SEAX_WORLD_WRITE_LOCK
        ]
    );
}

#[test]
fn sew_paint_quick_save_tech_fill_and_dimensions() {
    let mut h = Rec::new(|op, _| match op {
        EE_SCREENS_SEAX_PAINT_NEW => 55,
        EE_SCREENS_SEAX_OPTIONS_NEW => 66,
        EE_SCREENS_SEAX_LONG_GET => 160,
        _ => 0,
    });
    let (mut i, mut f) = ([3, 128, 0, 0, 0, 0, 0, 0], [0f32; 4]);
    sew(&mut h, EE_SCREENS_SEW_OP_POST_AMBIENT_PAINT, 0, &mut i, &mut f);
    assert_eq!(
        h.calls,
        [
            (EE_SCREENS_SEAX_PAINT_NEW, 0, 0, [3, 0, 0, 0, 0]),
            (EE_SCREENS_SEAX_PAINT_SET_AREA, 55, 0, [0, 127, 127, 0, 0]),
            (EE_SCREENS_SEAX_POST_ACTION, 55, 0, [0; 5]),
            (EE_SCREENS_SEAX_DEREFERENCE_ACTION, 55, 0, [0; 5])
        ]
    );
    h.calls.clear();
    sew(&mut h, EE_SCREENS_SEW_OP_QUICK_SAVE, 77, &mut i, &mut f);
    assert_eq!(
        h.ops(),
        [EE_SCREENS_SEAX_QUICK_SAVE_NOT_LOAD, EE_SCREENS_SEAX_QUICK_SAVE_WRITE, EE_SCREENS_SEAX_POST_ACTION]
    );
    assert!(h.calls.iter().all(|c| c.1 == 77));
    h.calls.clear();
    let mut i = [0, 4, 9, 0, 0, 0, 0, 0];
    sew(&mut h, EE_SCREENS_SEW_OP_TECH_FILL, 0, &mut i, &mut f);
    assert_eq!(h.calls, [(EE_SCREENS_SEAX_TECH_FILL, 0, 0, [EE_SCREENS_SE_RB_UNIT_TYPES_OTHER_TECH_LIST, 4, 9, 0, 0])]);
    h.calls.clear();
    let mut i = [2, 0, 0, 0, 0, 0, 0, 0];
    sew(&mut h, EE_SCREENS_SEW_OP_MAP_DIMENSIONS, 0, &mut i, &mut f);
    assert_eq!(&i[..2], &[160, 160]);
    assert_eq!(*h.ops().last().unwrap(), EE_SCREENS_SEAX_OPTIONS_DELETE);
    assert_eq!(sew(&mut h, EE_SCREENS_SEW_OP_POST, 0, &mut i, &mut f), None);
}

#[test]
fn sew_players_pending_units_and_ghosts() {
    // player 0 is missing; player 1 (handle 10) has two pending units 20, 21, the second a built building
    let mut h = Rec::new(|op, a| match op {
        EE_SCREENS_SEAX_PLAYER => {
            if a.i[0] == 1 {
                10
            } else {
                0
            }
        }
        EE_SCREENS_SEAX_PLAYER_PENDING_COUNT => 2,
        EE_SCREENS_SEAX_PENDING => 20 + a.i[0] as isize,
        EE_SCREENS_SEAX_UNIT_BUILT_LAND_BUILDING => isize::from(a.p == 21),
        EE_SCREENS_SEAX_PLAYER_HUMAN => 1,
        EE_SCREENS_SEAX_PLAYER_START_EPOCH => 3,
        EE_SCREENS_SEAX_GHOST_COUNT => 2,
        EE_SCREENS_SEAX_GHOST => 40 + a.i[0] as isize,
        EE_SCREENS_SEAX_GHOST_CAN_PLACE => isize::from(a.p == 40),
        _ => 0,
    });
    let (mut i, mut f) = ([0i32; 8], [0f32; 4]);
    assert_eq!(sew(&mut h, EE_SCREENS_SEW_OP_PLAYER, 0, &mut i, &mut f), Some(0));
    i[0] = 1;
    assert_eq!(sew(&mut h, EE_SCREENS_SEW_OP_PLAYER, 0, &mut i, &mut f), Some(1));
    assert_eq!(&i[..4], &[1, 0, 1, 3]);
    h.calls.clear();
    sew(&mut h, EE_SCREENS_SEW_OP_ADD_PENDING, 0, &mut i, &mut f);
    let buttons: Vec<_> = h.calls.iter().filter(|c| c.0 == EE_SCREENS_SEAX_PLAYER_LOAD_BUTTONS).collect();
    assert_eq!(buttons, [&(EE_SCREENS_SEAX_PLAYER_LOAD_BUTTONS, 10, 21, [0; 5])]);
    assert_eq!(h.calls.iter().filter(|c| c.0 == EE_SCREENS_SEAX_PLAYER_PENDING_COUNT).count(), 3);
    assert_eq!(
        h.calls.iter().filter(|c| c.0 == EE_SCREENS_SEAX_ADD_UNIT_ENTRY).map(|c| c.3[0]).collect::<Vec<_>>(),
        [0, 1]
    );
    // the second ghost cannot be placed
    assert_eq!(sew(&mut h, EE_SCREENS_SEW_OP_GHOSTS_PLACEABLE, 0, &mut i, &mut f), Some(0));
}

#[test]
fn random_map_builds_the_players_and_reads_the_location() {
    let mut h = Rec::new(|op, a| {
        match op {
            EE_SCREENS_SEAX_OPTIONS_NEW => return 5,
            EE_SCREENS_SEAX_POINT_X => a.f[0] = 1.0,
            EE_SCREENS_SEAX_POINT_Y => a.f[0] = 2.0,
            EE_SCREENS_SEAX_POINT_Z => a.f[0] = 3.0,
            _ => {}
        }
        0
    });
    assert_eq!(random_map(&mut h, &[2, 4], 1, 6, 3, 2), [1.0, 2.0, 3.0]);
    assert_eq!(
        h.calls,
        [
            (EE_SCREENS_SEAX_OPTIONS_NEW, 0, 0, [0; 5]),
            (EE_SCREENS_SEAX_OPTIONS_MAP_SIZE, 5, 0, [2, 0, 0, 0, 0]),
            (EE_SCREENS_SEAX_OPTIONS_EPOCH, 5, 0, [3, 0, 0, 0, 0]),
            (EE_SCREENS_SEAX_PLAYERS_CLEAR, 0, 0, [0; 5]),
            (EE_SCREENS_SEAX_PLAYERS_PUSH, 0, 0, [2, 0, 0, 0, 0]),
            (EE_SCREENS_SEAX_PLAYERS_PUSH, 0, 0, [4, 0, 0, 0, 0]),
            (EE_SCREENS_SEAX_POINT_ZERO, 0, 0, [0; 5]),
            (EE_SCREENS_SEAX_RANDOM_MAP, 5, 0, [1, 6, 0, 0, 0]),
            (EE_SCREENS_SEAX_POINT_X, 0, 0, [0; 5]),
            (EE_SCREENS_SEAX_POINT_Y, 0, 0, [0; 5]),
            (EE_SCREENS_SEAX_POINT_Z, 0, 0, [0; 5]),
            (EE_SCREENS_SEAX_OPTIONS_DELETE, 5, 0, [0; 5])
        ]
    );
}

#[test]
fn scan_reads_two_longs_as_sscanf() {
    assert_eq!(scan_two(b"128 64\r\n"), (128, 64));
    assert_eq!(scan_two(b"  12\t-1"), (12, u32::MAX));
    assert_eq!(scan_two(b"7"), (7, 0));
    assert_eq!(scan_two(b"x 7"), (0, 0));
}

#[test]
fn elevation_reads_the_pair_and_frees_both_specs() {
    // map spec 1, info spec 2; the info file reads "2 3"; the map file reads 12 bytes
    let mut specs = 0;
    let mut last_read = 0;
    let mut h = Rec::new(move |op, a| match op {
        EE_SCREENS_SEAX_SELECTION_GET => 4,
        EE_SCREENS_SEAX_SPEC_NEW => {
            specs += 1;
            specs
        }
        EE_SCREENS_SEAX_SPEC_READ if a.p == 2 => {
            let line = b"2 3";
            // SAFETY: q is the composition's 32-byte line
            unsafe { core::ptr::copy_nonoverlapping(line.as_ptr(), a.q as *mut u8, line.len()) };
            last_read = 3;
            0
        }
        EE_SCREENS_SEAX_SPEC_READ => {
            last_read = a.i[0] as isize;
            0
        }
        EE_SCREENS_SEAX_READ_COUNT => last_read,
        EE_SCREENS_SEAX_EDIT_INT => a.i[0] as isize,
        _ => 0,
    });
    let e = elevation(&mut h).unwrap();
    assert_eq!((e.width, e.height, e.data.len()), (2, 3, 6));
    assert_eq!(
        [e.water_cutoff, e.water_depth, e.land_height, e.land_cutoff],
        [
            EE_SCREENS_SE_MAP_WATER_CUTOFF,
            EE_SCREENS_SE_MAP_WATER_DEPTH,
            EE_SCREENS_SE_MAP_LAND_HEIGHT,
            EE_SCREENS_SE_MAP_LAND_CUTOFF
        ]
    );
    let map_read = h.calls.iter().find(|c| c.0 == EE_SCREENS_SEAX_SPEC_READ && c.1 == 1).unwrap();
    assert_eq!(map_read.3[0], 12);
    let tail: Vec<_> = h.calls.iter().rev().take(4).map(|c| (c.0, c.1)).collect();
    assert_eq!(
        tail,
        [
            (EE_SCREENS_SEAX_SPEC_DELETE, 1),
            (EE_SCREENS_SEAX_SPEC_DELETE, 2),
            (EE_SCREENS_SEAX_SPEC_CLOSE, 1),
            (EE_SCREENS_SEAX_SPEC_CLOSE, 2)
        ]
    );
    // nothing picked: nothing made
    let mut none = Rec::new(|_, _| 0);
    assert!(elevation(&mut none).is_none());
    assert_eq!(none.ops(), [EE_SCREENS_SEAX_ELEVATION_SELECTION, EE_SCREENS_SEAX_SELECTION_GET]);
}

#[test]
fn sep_ghost_flags_and_transformation() {
    let mut h = Rec::new(|op, a| match op {
        EE_SCREENS_SEAX_GHOST_COUNT => 1,
        EE_SCREENS_SEAX_GHOST => 30,
        EE_SCREENS_SEAX_GHOST_TYPE => EE_SCREENS_SEAX_TYPE_AIRPORT as isize,
        EE_SCREENS_SEAX_CLASS => 31,
        EE_SCREENS_SEAX_CLASS_FLIES => 1,
        EE_SCREENS_SEAX_CLASS_CRUISE => {
            a.f[0] = 9.0;
            0
        }
        EE_SCREENS_SEAX_GHOST_TILE_X => 5,
        EE_SCREENS_SEAX_GHOST_TILE_Y => 6,
        EE_SCREENS_SEAX_GHOST_CLASS_TILE_SIZE => 3,
        EE_SCREENS_SEAX_GHOST_ANIMATION => 2,
        EE_SCREENS_SEAX_GHOST_TRANSFORMATION => {
            a.t[3] = 42.0;
            0
        }
        EE_SCREENS_SEAX_GHOST_CAN_PLACE => 1,
        _ => 0,
    });
    let (mut t, mut i, mut f) = (identity(), [0i32; 8], [0f32; 4]);
    assert_eq!(sep(&mut h, EE_SCREENS_SEP_GHOST, &mut t, &mut i, &mut f), Some(1));
    assert_eq!(
        &i[..7],
        &[
            0,
            EE_SCREENS_SEAX_TYPE_AIRPORT,
            5,
            6,
            3,
            2,
            EE_SCREENS_SEP_F_CLASS | EE_SCREENS_SEP_F_FLIES | EE_SCREENS_SEP_F_AIRPORT
        ]
    );
    assert_eq!((f[0], t.m[0][3]), (9.0, 42.0));
    // no such ghost
    i[0] = 1;
    assert_eq!(sep(&mut h, EE_SCREENS_SEP_GHOST, &mut t, &mut i, &mut f), Some(0));
    i[0] = -1;
    assert_eq!(sep(&mut h, EE_SCREENS_SEP_GHOST, &mut t, &mut i, &mut f), Some(0));
}

#[test]
fn sep_board_stops_at_the_first_failed_test() {
    let mut h = Rec::new(|op, _| match op {
        EE_SCREENS_SEAX_SEP_UNIT => 10,
        EE_SCREENS_SEAX_UNIT_COMPLEX => 11,
        EE_SCREENS_SEAX_CLASS => 12,
        EE_SCREENS_SEAX_PLAYER => 13,
        EE_SCREENS_SEAX_COMPLEX_PLAYER_IS => 1,
        EE_SCREENS_SEAX_UNIT_TRANSPORTS => 0,
        _ => 1,
    });
    let (mut t, mut i, mut f) = (identity(), [1, 2, 3, 0, 0, 0, 0, 0], [0f32; 4]);
    assert_eq!(sep(&mut h, EE_SCREENS_SEP_BOARD, &mut t, &mut i, &mut f), Some(0));
    assert_eq!(*h.ops().last().unwrap(), EE_SCREENS_SEAX_UNIT_TRANSPORTS);
    let mut h = Rec::new(|op, _| match op {
        EE_SCREENS_SEAX_SEP_UNIT => 10,
        EE_SCREENS_SEAX_UNIT_COMPLEX => 11,
        EE_SCREENS_SEAX_COMPLEX_ID => 77,
        _ => 1,
    });
    i[1] = -1;
    assert_eq!(sep(&mut h, EE_SCREENS_SEP_BOARD, &mut t, &mut i, &mut f), Some(1));
    assert_eq!(i[4], 77);
    assert!(h.ops().contains(&EE_SCREENS_SEAX_UNIT_PLAYER));
}

#[test]
fn sep_create_into_a_transport_and_a_camera_object() {
    let mut h = Rec::new(|op, _| match op {
        EE_SCREENS_SEAX_CLASS => 1,
        EE_SCREENS_SEAX_PLAYER => 2,
        EE_SCREENS_SEAX_SEP_COMPLEX => 3,
        EE_SCREENS_SEAX_CLASS_TYPE => 10,
        EE_SCREENS_SEAX_CREATE_AT => 4,
        EE_SCREENS_SEAX_UNIT_ON_TILE => 0,
        _ => 0,
    });
    let (mut t, mut i, mut f) = (identity(), [0, 10, 0, 9, 0, 0, 0, 0], [0f32; 4]);
    sep(&mut h, EE_SCREENS_SEP_CREATE, &mut t, &mut i, &mut f);
    assert_eq!((t.m[0][3], t.m[1][3]), (-10000.0, -10000.0));
    let tail: Vec<_> = h.ops().into_iter().skip_while(|&o| o != EE_SCREENS_SEAX_PLAYER_ADD_PENDING).collect();
    assert_eq!(
        tail,
        [
            EE_SCREENS_SEAX_PLAYER_ADD_PENDING,
            EE_SCREENS_SEAX_COMPLEX_LOAD_UNIT,
            EE_SCREENS_SEAX_COMPLEX_EDITOR_SELECTED,
            EE_SCREENS_SEAX_EDITOR_SELECT_COMPLEX,
            EE_SCREENS_SEAX_EDITOR_UPDATE_SELECTION,
            EE_SCREENS_SEAX_COMPLEX_DEREFERENCE
        ]
    );
    let mut h = Rec::new(|op, _| match op {
        EE_SCREENS_SEAX_CLASS_TYPE => EE_SCREENS_SEAX_TYPE_CAMERA_OBJECT as isize,
        EE_SCREENS_SEAX_CREATE_AT_WORLD => 4,
        EE_SCREENS_SEAX_UNIT_ON_TILE => 1,
        EE_SCREENS_SEAX_UNIT_POWERED => 1,
        _ => 0,
    });
    let (mut t, mut i) = (identity(), [0, 10, 0, -1, 0, 0, 0, 0]);
    sep(&mut h, EE_SCREENS_SEP_CREATE, &mut t, &mut i, &mut f);
    assert_eq!(t, identity());
    let ops = h.ops();
    assert!(!ops.contains(&EE_SCREENS_SEAX_SEP_COMPLEX));
    let at = ops.iter().position(|&o| o == EE_SCREENS_SEAX_CREATE_AT_WORLD).unwrap();
    assert_eq!(
        &ops[at..],
        &[
            EE_SCREENS_SEAX_CREATE_AT_WORLD,
            EE_SCREENS_SEAX_UNIT_CLOAKABLE,
            EE_SCREENS_SEAX_UNIT_CLOAKED,
            EE_SCREENS_SEAX_CLOSEST_TILE,
            EE_SCREENS_SEAX_UNIT_SET_TILE_X,
            EE_SCREENS_SEAX_UNIT_SET_TILE_Y,
            EE_SCREENS_SEAX_UNIT_UPDATE_TILE,
            EE_SCREENS_SEAX_PLAYER_UNLOAD_MEDIA,
            EE_SCREENS_SEAX_UNIT_ON_TILE,
            EE_SCREENS_SEAX_CLASS_LAND_BUILDING,
            EE_SCREENS_SEAX_UNIT_ON_TILE,
            EE_SCREENS_SEAX_UNIT_POWERED,
            EE_SCREENS_SEAX_UNIT_SET_POWER
        ]
    );
}

#[test]
fn parts_of_a_simple_and_a_complex_unit() {
    let mut h = Rec::new(|op, a| match (op, a.p) {
        (EE_SCREENS_SEAX_SEP_UNIT, _) => 10,
        (EE_SCREENS_SEAX_UNIT_COMPLEX, _) => 20,
        (EE_SCREENS_SEAX_COMPLEX_NEXT, 20) => 21,
        (EE_SCREENS_SEAX_COMPLEX_NEXT, 21) => 22,
        (EE_SCREENS_SEAX_COMPLEX_ID, p) => p as isize + 100,
        _ => 0,
    });
    let mut out = [0i32; 8];
    assert_eq!(parts(&mut h, 5, 2, &mut out), 2);
    assert_eq!(&out[..2], &[120, 121]);
    assert_eq!(parts(&mut h, -1, 2, &mut out), 0);
    let mut h = Rec::new(|op, _| match op {
        EE_SCREENS_SEAX_SEP_UNIT => 10,
        EE_SCREENS_SEAX_UNIT_ID => 7,
        _ => 0,
    });
    assert_eq!(parts(&mut h, 5, 8, &mut out), 1);
    assert_eq!(
        h.ops(),
        [
            EE_SCREENS_SEAX_SEP_UNIT,
            EE_SCREENS_SEAX_UNIT_COMPLEX,
            EE_SCREENS_SEAX_UNIT_REFERENCE,
            EE_SCREENS_SEAX_UNIT_ID
        ]
    );
}

#[test]
fn sep_unit_flags_airport_facing_and_move() {
    let mut h = Rec::new(|op, a| match (op, a.p) {
        (EE_SCREENS_SEAX_SEP_UNIT, _) => 10,
        (EE_SCREENS_SEAX_UNIT_CLASS, _) => 11,
        (EE_SCREENS_SEAX_CLASS_PITCHES, _) => 1,
        (EE_SCREENS_SEAX_UNIT_COMPLEX, _) => 12,
        (EE_SCREENS_SEAX_COMPLEX_CLASS, _) => 13,
        (EE_SCREENS_SEAX_CLASS_TYPE, _) => EE_SCREENS_SEAX_TYPE_AIRPORT as isize,
        (EE_SCREENS_SEAX_CLASS_LAND_BUILDING, _) => 1,
        (EE_SCREENS_SEAX_FACING_QUARTER, _) => 2,
        (EE_SCREENS_SEAX_FACING, 12) => 1,
        (EE_SCREENS_SEAX_FACING, _) => 2,
        (EE_SCREENS_SEAX_COMPLEX_NEXT, 12) => 14,
        (EE_SCREENS_SEAX_LEVEL_GROUND, _) => {
            a.f[0] += 1.0;
            0
        }
        _ => 0,
    });
    let (mut t, mut i, mut f) = (identity(), [0i32; 8], [0f32; 4]);
    assert_eq!(
        sep(&mut h, EE_SCREENS_SEP_UNIT, &mut t, &mut i, &mut f),
        Some(
            EE_SCREENS_SEP_U_EXISTS
                | EE_SCREENS_SEP_U_PITCHES
                | EE_SCREENS_SEP_U_COMPLEX
                | EE_SCREENS_SEP_U_AIRPORT
                | EE_SCREENS_SEP_U_BUILDING
        )
    );
    h.calls.clear();
    sep(&mut h, EE_SCREENS_SEP_AIRPORT, &mut t, &mut i, &mut f);
    let sets: Vec<_> = h.calls.iter().filter(|c| c.0 == EE_SCREENS_SEAX_SET_FACING).map(|c| (c.1, c.3[0])).collect();
    // the airport turned to its quarter (2); the part (14) to the airport's facing as the host reads it (this host
    // keeps every facing)
    assert_eq!(sets, [(12, 2), (14, 1)]);
    assert_eq!(f[0], 1.0);
    h.calls.clear();
    i = [0, 4, 5, 0, 0, 0, 0, 0];
    sep(&mut h, EE_SCREENS_SEP_MOVE_TO, &mut t, &mut i, &mut f);
    assert_eq!(*h.calls.last().unwrap(), (EE_SCREENS_SEAX_COMPLEX_MOVE_TO, 12, 0, [4, 5, 0, 0, 0]));
}

#[test]
fn seu_rotation_kill_unload_and_pick() {
    let mut h = Rec::new(|op, a| match op {
        EE_SCREENS_SEAX_T_YPR => {
            assert_eq!(a.f[0], 1.5);
            0
        }
        EE_SCREENS_SEAX_IDS_COUNT => 2,
        EE_SCREENS_SEAX_ID => 50 + a.i[0] as isize,
        _ => 0,
    });
    seu(&mut h, EE_SCREENS_SEU_ROTATE, 8, 0, 1.5);
    assert_eq!(h.ops(), [EE_SCREENS_SEAX_T_FROM_LOCATION, EE_SCREENS_SEAX_T_YPR, EE_SCREENS_SEAX_T_APPLY]);
    h.calls.clear();
    seu(&mut h, EE_SCREENS_SEU_KILL_PART, 8, 9, 0.0);
    assert_eq!(
        h.calls,
        [(EE_SCREENS_SEAX_COMPLEX_KILL, 8, 0, [0; 5]), (EE_SCREENS_SEAX_PLAYER_KILL_COMPLEX, 9, 8, [0; 5])]
    );
    h.calls.clear();
    seu(&mut h, EE_SCREENS_SEU_UNLOAD, 8, 3, 0.0);
    assert_eq!(h.calls, [(EE_SCREENS_SEAX_COMPLEX_UNLOAD, 8, 3, [0; 5])]);
    h.calls.clear();
    seu(&mut h, EE_SCREENS_SEU_LEAVE_AIR_TRAFFIC, 8, 0, 0.0);
    assert_eq!(h.ops(), [EE_SCREENS_SEAX_COMPLEX_ATC_AIRPORT]);
    assert_eq!(pick(&mut h, 99, 4), [50, 51]);
    assert_eq!(seu(&mut h, EE_SCREENS_SEU_SELECT, 8, 0, 0.0), None);
}

/// A C++ enumerator's value (`NAME = value,`) in the header `path` (from the repository's root).
fn cpp_enumerator(path: &str, name: &str) -> i64 {
    let path = format!("{}/../../../{path}", env!("CARGO_MANIFEST_DIR"));
    let text = std::fs::read(&path).unwrap_or_else(|e| panic!("{path}: {e}"));
    for line in String::from_utf8_lossy(&text).lines() {
        if let [n, "=", v, ..] = line.split_whitespace().collect::<Vec<_>>().as_slice() {
            if *n == name {
                let v = v.trim_end_matches(',');
                return match v.strip_prefix("0x") {
                    Some(h) => i64::from_str_radix(h, 16).unwrap(),
                    None => v.parse().unwrap(),
                };
            }
        }
    }
    panic!("{name} not in {path}")
}

#[test]
fn the_message_ids_and_unit_types_equal_the_cpp_enumerators() {
    // (once EESEARust.cpp's static_asserts)
    use crate::screens::editor_actions::*;
    let m = |n| cpp_enumerator("include/rust/sim/message_ids.h", n);
    assert_eq!(m("kMessageSelectUnit"), EE_SCREENS_SEU_MSG_SELECT_UNIT as i64);
    assert_eq!(m("kMessageAddUnitToSelectedGroup"), EE_SCREENS_SEU_MSG_ADD_UNIT as i64);
    assert_eq!(m("kMessageAlwaysAddUnitToSelectedGroup"), EE_SCREENS_SEU_MSG_ALWAYS_ADD_UNIT as i64);
    assert_eq!(m("kMessageUnselectAllUnits"), EE_SCREENS_SEU_MSG_UNSELECT_ALL as i64);
    assert_eq!(m("kMessageSelectUnits"), EE_SCREENS_SEU_MSG_SELECT_UNITS as i64);
    assert_eq!(m("kMessageAddUnitsToSelectedGroup"), EE_SCREENS_SEU_MSG_ADD_UNITS as i64);
    assert_eq!(m("kMessageAddAndRemoveUnitsFromSelectedGroup"), EE_SCREENS_SEU_MSG_ADD_REMOVE_UNITS as i64);
    let u = |n| cpp_enumerator("include/rust/sim/unit_ids.h", n);
    assert_eq!(u("kUnitTypeAirport"), i64::from(EE_SCREENS_SEAX_TYPE_AIRPORT));
    assert_eq!(u("kUnitTypeGranary"), i64::from(EE_SCREENS_SEAX_TYPE_GRANARY));
    assert_eq!(u("kUnitTypeGranaryImproved"), i64::from(EE_SCREENS_SEAX_TYPE_GRANARY_IMPROVED));
    assert_eq!(u("kUnitTypeCameraObject"), i64::from(EE_SCREENS_SEAX_TYPE_CAMERA_OBJECT));
}
