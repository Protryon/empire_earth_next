use super::*;
use crate::units::player_save::is_save_read;

/// A file of values read in turn (a read past the end fails, leaving the old value); every operation recorded as
/// (op, index, io.i); the gets answer from `turrets` and `contacts_empty`.
#[derive(Default)]
struct File {
    data: std::collections::VecDeque<i64>,
    floats: std::collections::VecDeque<f32>,
    log: Vec<(i32, u32, i64)>,
    turrets: Vec<f32>,
    contacts_empty: bool,
}

impl SaveHost for File {
    fn op(&mut self, op: i32, index: u32, io: &mut EeUnitsUnitSaveIo) -> i32 {
        self.log.push((op, index, io.i));
        if op == EE_UNITS_UNIT_LOAD_GET {
            match index {
                EE_UNITS_UNIT_LOAD_GET_TURRET_COUNT => io.i = self.turrets.len() as i64,
                EE_UNITS_UNIT_LOAD_GET_TURRET_ANGLE => io.f = self.turrets[io.i as usize],
                _ => io.i = i64::from(self.contacts_empty),
            }
            return 0;
        }
        if op == EE_UNITS_UNIT_LOAD_SET && index == EE_UNITS_UNIT_LOAD_SET_LIVE_REGISTER {
            // the C++ shared pointer's deleter
            // SAFETY: the Box live_state made
            unsafe { ee_units_unit_live_free(io.i as usize as *mut std::ffi::c_void) };
            return 0;
        }
        if is_save_read(op) {
            if op == EE_UNITS_UNIT_LOAD_READ_FLOAT {
                return match self.floats.pop_front() {
                    Some(f) => {
                        io.f = f;
                        0
                    }
                    None => 1,
                };
            }
            return match self.data.pop_front() {
                Some(v) => {
                    io.i = v;
                    0
                }
                None => 1,
            };
        }
        0
    }
}

impl File {
    fn sets(&self) -> Vec<(u32, i64)> {
        self.log.iter().filter(|e| e.0 == EE_UNITS_UNIT_LOAD_SET).map(|e| (e.1, e.2)).collect()
    }
    fn reads(&self) -> Vec<i32> {
        self.log.iter().filter(|e| is_save_read(e.0)).map(|e| e.0).collect()
    }
}

#[test]
fn the_build_queue_reads_its_flags_from_0xf8() {
    let mut f = File { data: [2, 10, 1, 11, 0].into(), ..Default::default() };
    assert_eq!(build_queue(&mut f, 0xf8), 0);
    assert_eq!(
        f.sets(),
        [
            (EE_UNITS_UNIT_LOAD_SET_BUILD_QUEUE_CLEAR, 0),
            (EE_UNITS_UNIT_LOAD_SET_BUILD_QUEUE_SECOND, 1),
            (EE_UNITS_UNIT_LOAD_SET_BUILD_QUEUE_PUSH, 10),
            (EE_UNITS_UNIT_LOAD_SET_BUILD_QUEUE_SECOND, 0),
            (EE_UNITS_UNIT_LOAD_SET_BUILD_QUEUE_PUSH, 11),
        ]
    );
    let (l, u) = (EE_UNITS_UNIT_LOAD_READ_LONG, EE_UNITS_UNIT_LOAD_READ_ULONG);
    assert_eq!(f.reads(), [l, l, u, l, u]);
    // before 0xf8 no flag: two entries of one long each
    let mut f = File { data: [2, 10, 11].into(), ..Default::default() };
    assert_eq!(build_queue(&mut f, 0xf7), 0);
    assert_eq!(f.sets().iter().filter(|s| s.0 == EE_UNITS_UNIT_LOAD_SET_BUILD_QUEUE_PUSH).count(), 2);
    // a failed entry read ends the loop with its error, nothing pushed
    let mut f = File { data: [3, 10].into(), ..Default::default() };
    assert_eq!(build_queue(&mut f, 0xf8), 1);
    assert!(!f.sets().iter().any(|s| s.0 == EE_UNITS_UNIT_LOAD_SET_BUILD_QUEUE_PUSH));
}

#[test]
fn the_id_lists_carry_the_value_through() {
    let mut f = File { data: [2, 7, 8].into(), ..Default::default() };
    let mut value = 99;
    let clear = EE_UNITS_UNIT_LOAD_SET_CARGO_CLEAR;
    let push = EE_UNITS_UNIT_LOAD_SET_CARGO_PUSH;
    assert_eq!(ids(&mut f, clear, push, &mut value), 0);
    assert_eq!(value, 8);
    assert_eq!(f.sets(), [(clear, 0), (push, 7), (push, 8)]);
    // a failed id read keeps the old value and pushes nothing
    let mut f = File { data: [2].into(), ..Default::default() };
    let mut value = 5;
    assert_eq!(ids(&mut f, clear, push, &mut value), 1);
    assert_eq!((value, f.sets().len()), (5, 1));
}

#[test]
fn the_held_units_set_even_a_failed_read_and_leave_on_the_next() {
    let mut f = File { data: [3, 7].into(), ..Default::default() };
    let mut value = 4;
    assert_eq!(held_units(&mut f, &mut value), (1, true));
    assert_eq!(
        f.sets(),
        [
            (EE_UNITS_UNIT_LOAD_SET_HELD_RESIZE, 3),
            (EE_UNITS_UNIT_LOAD_SET_SLOT, 0),
            (EE_UNITS_UNIT_LOAD_SET_HELD_UNIT, 7),
            (EE_UNITS_UNIT_LOAD_SET_SLOT, 1),
            (EE_UNITS_UNIT_LOAD_SET_HELD_UNIT, 7),
        ]
    );
    // the last read failing does not leave
    let mut f = File { data: [1].into(), ..Default::default() };
    assert_eq!(held_units(&mut f, &mut value), (1, false));
}

#[test]
fn the_unit_variables_go_to_their_slots() {
    let mut f = File { data: [2, 30, 31].into(), ..Default::default() };
    let mut state = 0;
    assert_eq!(unit_variables(&mut f, &mut state), 0);
    assert_eq!(state, 31);
    assert_eq!(
        f.sets(),
        [
            (EE_UNITS_UNIT_LOAD_SET_SLOT, 0),
            (EE_UNITS_UNIT_LOAD_SET_UNIT_VARIABLE, 30),
            (EE_UNITS_UNIT_LOAD_SET_SLOT, 1),
            (EE_UNITS_UNIT_LOAD_SET_UNIT_VARIABLE, 31),
        ]
    );
}

#[test]
fn the_live_state_registers_its_contacts_and_itself() {
    // next frame, AI state, trigger time, 1 attacker (6), 1 tile (2), then the running state's 7, 2 contacts
    let longs = [5, 6, 7, 1, 1, 2, 3, 4, 50, 60, 1, 8, 9, 100, 0, 3, 100000, 20, 0, 40, 2, 71, 72];
    let mut f = File {
        data: longs.into(),
        floats: [0.5, 1.0, 0.25, 0.75, 2.0].into(),
        turrets: vec![0.5],
        contacts_empty: true,
        ..Default::default()
    };
    assert_eq!(live_state(&mut f, true), 0);
    assert!(f.data.is_empty() && f.floats.is_empty());
    let sets = f.sets();
    assert_eq!(
        sets,
        [
            (EE_UNITS_UNIT_LOAD_SET_CONTACTS_REGISTER, 0),
            (EE_UNITS_UNIT_LOAD_SET_CONTACTS_BEGIN, 0),
            (EE_UNITS_UNIT_LOAD_SET_CONTACT_ID, 71),
            (EE_UNITS_UNIT_LOAD_SET_CONTACT_ID, 72),
            (EE_UNITS_UNIT_LOAD_SET_LIVE_REGISTER, sets[4].1),
        ]
    );
    // a later unit does not register the restore again; no running state, no contacts
    let mut f = File { data: [5, 6, 7, 0, 0].into(), floats: [0.0; 5].into(), ..Default::default() };
    assert_eq!(live_state(&mut f, false), 0);
    assert_eq!(f.sets().len(), 1);
    // a failed read registers nothing
    let mut f = File { data: [5].into(), floats: [0.0; 5].into(), ..Default::default() };
    assert_eq!(live_state(&mut f, true), 1);
    assert!(f.sets().is_empty());
}

#[derive(Default)]
struct Dop {
    log: Vec<(i32, u64, i64, i64)>,
    stale: bool,
    parts: i64,
}

impl DopHost for Dop {
    fn dop(&mut self, op: i32, unit: u64, a: u64, b: i64, c: i64) -> i64 {
        assert_eq!(unit, 9);
        if op == EE_UNITS_DOP_IGNORED_TARGETS_PUSH {
            // SAFETY: the six values the apply hands over
            let v = unsafe { std::slice::from_raw_parts(a as usize as *const i64, 6) };
            self.log.push((op, 0, v[0], v[5]));
            return 0;
        }
        self.log.push((op, a, b, c));
        match op {
            EE_UNITS_DOP_IS_WORLD_UNIT => i64::from(!self.stale),
            EE_UNITS_DOP_MODEL_PARTS_OR_ZERO => self.parts,
            EE_UNITS_DOP_HAS_TURRET_ANGLES => 1,
            _ => 0,
        }
    }
}

#[test]
fn the_live_state_is_put_back_on_the_world_unit_only() {
    let s = EeUnitsLiveState {
        animation_fraction: 0.5,
        ignored_targets: vec![[1, 2, 3, 4, 5, 6]],
        bounded_path_tiles: vec![(8, 9)],
        running: true,
        frame: 33,
        turret_angles: vec![0.25, 0.75],
        ..Default::default()
    };
    let mut h = Dop { stale: true, ..Default::default() };
    live_apply(&mut h, 9, 44, &s);
    assert_eq!(h.log, [(EE_UNITS_DOP_IS_WORLD_UNIT, 0, 44, 0)]);
    let mut h = Dop { parts: 1, ..Default::default() };
    live_apply(&mut h, 9, 44, &s);
    let ops: Vec<i32> = h.log.iter().map(|e| e.0).collect();
    assert_eq!(ops[1], EE_UNITS_DOP_SET_ANIMATION_FRACTION);
    assert_eq!(h.log[1].2, i64::from(0.5f32.to_bits()));
    assert!(h.log.contains(&(EE_UNITS_DOP_IGNORED_TARGETS_PUSH, 0, 1, 6)));
    assert!(h.log.contains(&(EE_UNITS_DOP_BOUNDED_PATH_TILES_PUSH, 0, 8, 9)));
    assert!(h.log.contains(&(EE_UNITS_DOP_SET_FRAME, 0, 33, 0)));
    // one part: one turret angle of the two
    let turrets: Vec<_> = h.log.iter().filter(|e| e.0 == EE_UNITS_DOP_SET_TURRET_ANGLE).collect();
    assert_eq!(turrets.len(), 1);
    assert_eq!(turrets[0].3, i64::from(0.25f32.to_bits()));
}
