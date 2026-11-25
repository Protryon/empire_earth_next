use super::*;
use std::collections::HashMap;
use std::os::raw::c_ulong;

/// A player (0x100) whose towns are objects with fields; the calls made.
#[derive(Default)]
struct World {
    context_player: u64,
    towns: Vec<u64>,
    fields: HashMap<(u64, i32, i64), i64>,
    log: Vec<(i32, u64, i64, i64)>,
    next: u64,
    readable: bool,
}

struct NoFile;

impl FilePrims for NoFile {
    fn read_u32(&mut self, _: &mut u32) -> c_long {
        7
    }
    fn read_i32(&mut self, _: &mut i32) -> c_long {
        7
    }
    fn read_f32(&mut self, _: &mut f32) -> c_long {
        7
    }
    fn read_bool(&mut self, _: &mut u8) -> c_long {
        7
    }
    fn read_u16(&mut self, _: &mut u16) -> c_long {
        7
    }
    fn read_bytes(&mut self, _: *mut u8, len: c_ulong, read: &mut c_ulong) -> c_long {
        *read = len;
        0
    }
    fn write_u32(&mut self, _: u32) -> c_long {
        0
    }
    fn write_i32(&mut self, _: i32) -> c_long {
        0
    }
    fn write_f32(&mut self, _: f32) -> c_long {
        0
    }
    fn write_bool(&mut self, _: bool) -> c_long {
        0
    }
    fn write_u16(&mut self, _: u16) -> c_long {
        0
    }
    fn write_bytes(&mut self, _: *const u8, _: c_ulong, _: &mut c_ulong) -> c_long {
        0
    }
}

impl TownSplitHost for &mut World {
    fn town(&mut self, op: i32, _: u32, _: u32, io: &mut EeUnitsTownIo) -> i32 {
        match op {
            EE_UNITS_TOWN_X_CONTEXT_PLAYER => io.prim.i = self.context_player as i64,
            EE_UNITS_TOWN_PLAYERS => io.prim.i = 2,
            EE_UNITS_TOWN_X_WORLD_PLAYER => io.prim.i = 0x100,
            _ => {}
        }
        0
    }
    fn body(&mut self, op: i32, p: u64, a: u64, b: i64, c: i64) -> i64 {
        self.log.push((op, a, b, c));
        assert_eq!(p, 0x100);
        match op {
            EE_UNITS_PLAYER_BODY_TOWN_NEW => {
                self.next += 1;
                self.next as i64
            }
            EE_UNITS_PLAYER_BODY_TOWN_READ_AREA => i64::from(self.readable),
            EE_UNITS_PLAYER_BODY_HULL_POINT => (5 << 32) | 6,
            _ => *self.fields.get(&(a, op, b)).unwrap_or(&0),
        }
    }
    fn file(&mut self) -> Box<dyn FilePrims + '_> {
        Box::new(NoFile)
    }
    fn towns(&mut self, p: u64, op: ListOp) -> u64 {
        assert_eq!(p, 0x100);
        crate::units::player_lists::apply_vec(&mut self.towns, crate::units::player_lists::EE_UNITS_LISTS_TOWNS, op)
    }
}

#[test]
fn a_town_is_read_by_slot_of_the_world_player() {
    let mut w = World { towns: vec![0x10, 0], ..Default::default() };
    w.fields.insert((0x10, EE_UNITS_PLAYER_BODY_TOWN_ID, 0), 42);
    w.fields.insert((0x10, EE_UNITS_PLAYER_BODY_TOWN_COUNTER, 3), 9);
    w.fields.insert((0x10, EE_UNITS_PLAYER_BODY_TOWN_NEAR_WATER, 0), 1);
    let mut s = TownSplit::new(&mut w);
    let mut io = EeUnitsTownIo::default();
    assert_eq!(s.call(EE_UNITS_TOWN_SLOTS, 1, 0, &mut io), 2);
    assert_eq!(s.call(EE_UNITS_TOWN_GET, 1, 0, &mut io), 1);
    assert_eq!((io.id, io.counters[3], io.near_water), (42, 9, 1));
    assert_eq!(s.call(EE_UNITS_TOWN_GET, 1, 1, &mut io), 0, "an empty slot");
    assert_eq!(s.call(EE_UNITS_TOWN_GET, 1, 2, &mut io), 0, "past the slots");
    s.call(EE_UNITS_TOWN_HULL_POINT, 1, 0, &mut io);
    assert_eq!((io.x, io.y), (5, 6));
    // past the world's players: none
    assert_eq!(s.call(EE_UNITS_TOWN_SLOTS, 2, 0, &mut io), 0);
}

#[test]
fn made_towns_are_committed_in_order_or_discarded() {
    let mut w = World { context_player: 0x100, readable: true, ..Default::default() };
    let mut s = TownSplit::new(&mut w);
    let mut io = EeUnitsTownIo { id: 7, continent: 0x1_0003, ..Default::default() };
    io.counters[0] = 11;
    assert_eq!(s.call(EE_UNITS_TOWN_MAKE, 0, 0, &mut io), 1);
    assert_eq!(s.call(EE_UNITS_TOWN_MAKE, 0, 0, &mut io), 1);
    s.call(EE_UNITS_TOWN_MAKE_COMMIT, 0, 0, &mut io);
    assert!(s.made.is_empty());
    assert_eq!(s.h.towns, vec![1, 2]);
    assert!(s.h.log.contains(&(EE_UNITS_PLAYER_BODY_TOWN_SET_CONTINENT, 1, 3, 0)));
    assert!(s.h.log.contains(&(EE_UNITS_PLAYER_BODY_TOWN_SET_COUNTER, 2, 0, 11)));
    s.h.readable = false;
    assert_eq!(s.call(EE_UNITS_TOWN_MAKE, 0, 0, &mut io), 0);
    assert_eq!(s.h.log.last().unwrap().0, EE_UNITS_PLAYER_BODY_TOWN_DELETE);
}

#[test]
fn the_order_is_the_slots_and_the_primitives_are_the_file() {
    let mut w = World { context_player: 0x100, towns: vec![0x10, 0x20, 0x30], ..Default::default() };
    let mut s = TownSplit::new(&mut w);
    let slots = [2i32, 0];
    let mut io = EeUnitsTownIo { bytes: slots.as_ptr() as *mut u8, len: 2, ..Default::default() };
    s.call(EE_UNITS_TOWN_REORDER, 0, 0, &mut io);
    assert_eq!(s.h.towns, vec![0x30, 0x10]);
    let mut io = EeUnitsTownIo::default();
    assert_eq!(s.call(crate::units::player_save::EE_UNITS_PLAYER_SAVE_READ_LONG, 0, 0, &mut io), 7);
    let mut buffer = [0u8; 4];
    let mut io = EeUnitsTownIo { bytes: buffer.as_mut_ptr(), len: 4, ..Default::default() };
    assert_eq!(s.call(EE_UNITS_TOWN_READ_BYTES, 0, 0, &mut io), 0);
    assert_eq!(io.prim.i, 4);
}
