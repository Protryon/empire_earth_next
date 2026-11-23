use super::*;
use std::collections::HashMap;

/// A recording mock: each statement (kind, which, a, x); answers by (which, a), else by which.
#[derive(Default)]
struct Mock {
    log: Vec<(char, i32, u64, i32)>,
    world: HashMap<(i32, u64), u64>,
    world_any: HashMap<i32, u64>,
    floats: HashMap<i32, f32>,
    spots: HashMap<i32, i32>,
    notes: Vec<(i32, i32, i32)>,
    shaped: Vec<(u64, i32, i32)>,
}

impl UuOps for Mock {
    fn world(&mut self, which: i32, a: u64, _b: u64, _c: u64, x: i32, _y: i32, _f: f32) -> u64 {
        self.log.push(('w', which, a, x));
        self.world.get(&(which, a)).or_else(|| self.world_any.get(&which)).copied().unwrap_or(0)
    }
    fn world_f(&mut self, which: i32, a: u64, _b: u64, _f: f32) -> f32 {
        self.log.push(('f', which, a, 0));
        self.floats.get(&which).copied().unwrap_or(0.0)
    }
    fn spot(&mut self, which: i32, unit: u64, x: i32, _y: i32, out: &mut [f32; 6]) -> i32 {
        self.log.push(('s', which, unit, x));
        match which {
            US_TOP_POINT => out[..3].copy_from_slice(&[1.0, 2.0, 3.0]),
            US_SCALE => out[0] = 2.0,
            US_APPLY => {
                for k in 0..3 {
                    out[3 + k] = out[k] + 10.0;
                }
            }
            US_POSITION | US_GATE_CARRIER_POSITION => out[..3].copy_from_slice(&[7.0, 8.0, 9.0]),
            US_GATE_LINKED_POSITION => out[..3].copy_from_slice(&[4.0, 5.0, 6.0]),
            _ => {}
        }
        self.spots.get(&which).copied().unwrap_or(0)
    }
    fn note_event(&mut self, a: i32, b: i32, c: i32) {
        self.notes.push((a, b, c));
    }
    fn shape_wall(&mut self, complex: u64, corner: i32, raised: i32) {
        self.shaped.push((complex, corner, raised));
    }
}

impl Mock {
    fn which(&self) -> Vec<i32> {
        self.log.iter().map(|l| l.1).collect()
    }
}

#[test]
fn notes_come_before_their_statements() {
    let mut m = Mock::default();
    m.world.insert((UW_UNIT_ID, 5), 50);
    m.world.insert((UW_UNIT_ID, 6), 60);
    add_build_target(&mut m, 5, 6);
    assert_eq!(m.notes, vec![(50, 60, 1)]);
    assert_eq!(m.which(), vec![UW_UNIT_ID, UW_UNIT_ID, UW_ADD_BUILD_TARGET]);
    let mut m = Mock::default();
    restart_idle(&mut m, 5);
    assert_eq!(m.which(), vec![UW_UNIT_ID, UW_CLEAR_GOALS, UW_START_IDLE, UW_ACTION_CHANGED]);
    let mut m = Mock::default();
    step_aside(&mut m, 5, 1, 2, 3, 4, true);
    assert_eq!(m.notes, vec![(0, 3, 4)]);
    assert_eq!(*m.which().last().unwrap(), UW_STEP_ASIDE);
}

#[test]
fn clear_ambient_only_a_deletable_ambient() {
    let mut m = Mock::default();
    clear_ambient(&mut m, 5);
    assert_eq!(m.which(), vec![UW_IS_AMBIENT]);
    let mut m = Mock::default();
    m.world_any.insert(UW_IS_AMBIENT, 1);
    m.world_any.insert(UW_DELETABLE, 1);
    m.world_any.insert(UW_RESOURCE_AMOUNT, 3);
    clear_ambient(&mut m, 5);
    assert_eq!(
        m.which(),
        vec![
            UW_IS_AMBIENT,
            UW_CLASS_TYPE,
            UW_DELETABLE,
            UW_RESOURCE_AMOUNT,
            UW_CLEAR_RESOURCE_AMOUNT,
            UW_KILL,
            UW_PLAYER_KILL
        ]
    );
    let mut m = Mock::default();
    m.world_any.insert(UW_IS_AMBIENT, 1);
    m.world_any.insert(UW_CLASS_TYPE, 0xdf);
    clear_ambient(&mut m, 5);
    assert_eq!(m.which(), vec![UW_IS_AMBIENT, UW_CLASS_TYPE]);
}

#[test]
fn tile_unit_adds_or_removes() {
    let mut m = Mock::default();
    tile_unit(&mut m, 1, 2, true);
    tile_unit(&mut m, 1, 2, false);
    assert_eq!(m.which(), vec![UW_TILE_ADD, UW_TILE_REMOVE]);
}

#[test]
fn editor_transport_checks_in_order() {
    let mut m = Mock::default();
    m.world_any.insert(UW_UNIT_UNDER_MOUSE, (-1i64) as u64);
    assert!(!editor_transport_takes(&mut m, 9, 3));
    assert_eq!(m.which(), vec![UW_UNIT_UNDER_MOUSE]);
    let mut m = Mock::default();
    m.world_any.insert(UW_UNIT_UNDER_MOUSE, 4);
    m.world_any.insert(UW_WORLD_UNIT, 40);
    m.world_any.insert(UW_COMPLEX, 41);
    m.world_any.insert(UW_PLAYER, 9);
    m.world_any.insert(UW_TRANSPORT_CAPACITY, 2);
    m.world_any.insert(UW_CARGO_ROOM, 5);
    m.world_any.insert(UW_TYPE_WEIGHT, 5);
    m.world_any.insert(UW_TYPE_CAN_ENTER, 1);
    assert!(editor_transport_takes(&mut m, 9, 3));
    assert_eq!(m.log[1], ('w', UW_WORLD_UNIT, 0, 4));
    assert_eq!(m.log.last().unwrap(), &('w', UW_TYPE_CAN_ENTER, 41, 3));
}

#[test]
fn wall_piece_foundation_and_built() {
    let mut m = Mock::default();
    m.world_any.insert(UW_CREATE_UNIT, 70);
    m.world_any.insert(UW_COMPLEX, 71);
    m.world_any.insert(UW_RAISED_CORNERS, 2);
    m.world_any.insert(UW_TILE_VISIBILITY, 1);
    make_wall_piece(&mut m, 1, 2, 3, 4, 5, true);
    assert_eq!(m.shaped, vec![(71, 5, 2)]);
    let w = m.which();
    assert_eq!(w.iter().filter(|&&c| c == UW_TILE_VISIBILITY).count(), 2);
    assert!(w.contains(&UW_ADD_UNSEEN) && w.contains(&UW_PAY) && !w.contains(&UW_SET_BUILT));
    assert_eq!(&w[w.len() - 2..], &[UW_PAID, UW_SET_PAID]);
    let mut m = Mock::default();
    m.world_any.insert(UW_MAX_HIT_POINTS, 300);
    m.world_any.insert(UW_PAID, 1);
    m.world_any.insert(UW_CLASS_TYPE, 0x2fe);
    m.world_any.insert(UW_SPACE_WALL_AVAILABLE, 1);
    make_wall_piece(&mut m, 1, 2, 3, 4, 5, false);
    assert!(m.log.contains(&('w', UW_SET_HIT_POINTS, 0, 300)));
    let w = m.which();
    assert!(w.contains(&UW_SET_BUILT) && w.contains(&UW_SPACE_ENERGY_WALL) && !w.contains(&UW_SET_PAID));
}

#[test]
fn wall_rise_of_a_gate_and_of_a_site() {
    let mut m = Mock::default();
    m.world_any.insert(UW_CATEGORY, 0x19);
    m.world_any.insert(UW_COMPLEX_BUILT, 1);
    m.floats.insert(UWF_STATS_SCALE, 2.0);
    assert_eq!(wall_rise(&mut m, 5), 2.4);
    assert!(!m.which().contains(&UWF_BOX_TOP_Z));
    let mut m = Mock::default();
    m.floats.insert(UWF_BOX_TOP_Z, 3.0);
    m.floats.insert(UWF_STATS_SCALE, 2.0);
    m.floats.insert(UWF_HIT_POINTS, 1.0);
    m.floats.insert(UWF_MAX_HIT_POINTS, 4.0);
    assert_eq!(wall_rise(&mut m, 5), 1.5);
}

#[test]
fn drop_point_of_the_dead_and_the_living() {
    let mut m = Mock::default();
    m.spots.insert(US_HIT_POINTS, 3);
    assert_eq!(drop_point(&mut m, 5), [7.0, 8.0, 9.0]);
    let mut m = Mock::default();
    m.spots.insert(US_HAS_COMPLEX, 1);
    m.spots.insert(US_HAS_MODEL, 1);
    assert_eq!(drop_point(&mut m, 5), [12.0, 14.0, 13.0]);
}

#[test]
fn path_area_and_gate_ends() {
    let mut m = Mock::default();
    assert_eq!(path_area_id(&mut m, 1, 2), 0);
    assert_eq!(m.which(), vec![UW_PATH_AREA]);
    m.world_any.insert(UW_PATH_AREA, 0x1234);
    m.spots.insert(US_PATH_AREA_ID, 9);
    assert_eq!(path_area_id(&mut m, 1, 2), 9);
    assert_eq!(m.log.last().unwrap(), &('s', US_PATH_AREA_ID, 0x1234, 0));
    let mut m = Mock::default();
    assert_eq!(gate_ends(&mut m, 5), [[7.0, 8.0, 9.0], [4.0, 5.0, 6.0]]);
}
