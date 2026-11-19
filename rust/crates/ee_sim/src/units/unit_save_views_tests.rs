//! The save's view answers over fake units laid out by a test layout, against a recording host.

use std::mem::offset_of;
use std::os::raw::{c_long, c_ulong};

use super::*;
use crate::units::unit_fields::{EeUnitsComplexUnitFields, EeUnitsRtsUnitFields, EeUnitsUnitFields};
use crate::world::layout::{with_test_layout, EeWorldLayout};
use crate::world::stats_fields::EeUnitsUnitClassUpgradeFields;
use crate::world::unit_class_fields::EeUnitsUnitClassFields;

#[repr(C)]
#[derive(Default)]
struct Fake {
    rts: EeUnitsRtsUnitFields,
    kind: u8,
    class: usize,
    stats: usize,
    unit: EeUnitsUnitFields,
    container: usize,
    command: usize,
    herd: usize,
    cargo: [usize; 3],
    previous_part: usize,
    next_part: usize,
    build_queue: [usize; 3],
    ptrs: [usize; 10],
    held: [usize; 3],
    vars: [usize; 3],
    tiles: [usize; 3],
    contacts_head: usize,
    contacts_count: c_ulong,
    collision: [c_long; 2],
    contact: [f32; 3],
    scale: f32,
    frame: c_ulong,
    effects: [u32; 2],
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
    fields: EeUnitsUnitClassUpgradeFields,
}

#[repr(C)]
struct Node {
    data: usize,
    prev: usize,
    next: usize,
}

#[repr(C)]
struct Herd {
    pad: u16,
    id: u16,
}

fn layout() -> EeWorldLayout {
    let p = offset_of!(Fake, ptrs) as u64;
    let w = std::mem::size_of::<usize>() as u64;
    EeWorldLayout {
        size: std::mem::size_of::<EeWorldLayout>() as u64,
        rts_unit_fields: offset_of!(Fake, rts) as u64,
        unit_fields: offset_of!(Fake, unit) as u64,
        complex_unit_fields: offset_of!(Fake, complex) as u64,
        unit_class_fields: offset_of!(FakeClass, fields) as u64,
        stats_fields: offset_of!(FakeStats, fields) as u64,
        unit_object_kind: offset_of!(Fake, kind) as u64,
        unit_class: offset_of!(Fake, class) as u64,
        unit_stats: offset_of!(Fake, stats) as u64,
        complex_container: offset_of!(Fake, container) as u64,
        complex_command: offset_of!(Fake, command) as u64,
        complex_herd: offset_of!(Fake, herd) as u64,
        complex_cargo: offset_of!(Fake, cargo) as u64,
        complex_previous_part: offset_of!(Fake, previous_part) as u64,
        complex_next_part: offset_of!(Fake, next_part) as u64,
        complex_build_queue: offset_of!(Fake, build_queue) as u64,
        build_entry_stride: 2 * std::mem::size_of::<c_long>() as u64,
        build_entry_technology: 0,
        build_entry_flag: std::mem::size_of::<c_long>() as u64,
        complex_launcher_class: p,
        complex_patrol_group: p + w,
        complex_home_base: p + 2 * w,
        complex_gathered_resource: p + 3 * w,
        complex_last_attacker: p + 4 * w,
        complex_owner: p + 5 * w,
        complex_mine: p + 6 * w,
        complex_holder: p + 7 * w,
        complex_behavior: p + 8 * w,
        complex_last_resource: p + 9 * w,
        complex_held_units: offset_of!(Fake, held) as u64,
        complex_unit_variables: offset_of!(Fake, vars) as u64,
        complex_bounded_path_tiles: offset_of!(Fake, tiles) as u64,
        complex_contacts_head: offset_of!(Fake, contacts_head) as u64,
        complex_contacts_count: offset_of!(Fake, contacts_count) as u64,
        contact_node_data: offset_of!(Node, data) as u64,
        contact_node_next: offset_of!(Node, next) as u64,
        complex_collision_tile: offset_of!(Fake, collision) as u64,
        complex_contact_location: offset_of!(Fake, contact) as u64,
        complex_scale: offset_of!(Fake, scale) as u64,
        complex_frame: offset_of!(Fake, frame) as u64,
        complex_conversion_effect: offset_of!(Fake, effects) as u64,
        complex_heal_effect: offset_of!(Fake, effects) as u64 + 4,
        herd_id: offset_of!(Herd, id) as u64,
        ..Default::default()
    }
}

fn fake(id: c_ulong, kind: u8) -> Box<Fake> {
    let mut f = Box::<Fake>::default();
    f.rts.mUnitID = id;
    f.kind = kind;
    f
}

fn addr<T>(v: &T) -> usize {
    v as *const T as usize
}

/// Records every op the views pass on; answers 0 (and 77 for the queries, so a pass-through shows).
#[derive(Default)]
struct Rec {
    ops: Vec<(i32, u32, i64)>,
}

impl SaveHost for Rec {
    fn op(&mut self, op: i32, index: u32, io: &mut EeUnitsUnitSaveIo) -> i32 {
        self.ops.push((op, index, io.i));
        io.i = 77;
        0
    }
}

fn ask(unit: &Fake, op: i32, index: u32) -> (i64, Vec<(i32, u32, i64)>) {
    let mut rec = Rec::default();
    let mut io = EeUnitsUnitSaveIo::default();
    {
        let mut h = ViewSaveHost { inner: &mut rec, unit: addr(unit) as u64 };
        h.op(op, index, &mut io);
    }
    (io.i, rec.ops)
}

const NONE_ID: i64 = c_ulong::MAX as i64;

#[test]
fn ids_from_pointer_members() {
    let _l = with_test_layout(layout());
    let mut u = fake(5, 2);
    let base = fake(9, 2);
    let mut transport = fake(11, 2);
    let mut tclass = Box::<FakeClass>::default();
    tclass.fields.mTransportCapacity = 4;
    transport.class = addr(&*tclass);
    let herd = Box::new(Herd { pad: 0, id: 321 });
    u.ptrs[2] = addr(&*base);
    u.container = addr(&*transport);
    u.herd = addr(&*herd);
    u.frame = 6;
    u.effects = [0xffff_ffff, 3];
    assert_eq!(ask(&u, EE_UNITS_UNIT_SAVE_ID, EE_UNITS_UNIT_SAVE_ID_HOME_BASE).0, 9);
    assert_eq!(ask(&u, EE_UNITS_UNIT_SAVE_ID, EE_UNITS_UNIT_SAVE_ID_OWNER).0, NONE_ID);
    assert_eq!(ask(&u, EE_UNITS_UNIT_SAVE_ID, EE_UNITS_UNIT_SAVE_ID_CONTAINER).0, 11);
    tclass.fields.mTransportCapacity = 0;
    assert_eq!(ask(&u, EE_UNITS_UNIT_SAVE_ID, EE_UNITS_UNIT_SAVE_ID_CONTAINER).0, NONE_ID);
    assert_eq!(ask(&u, EE_UNITS_UNIT_SAVE_ID, EE_UNITS_UNIT_SAVE_ID_HERD).0, 321);
    assert_eq!(ask(&u, EE_UNITS_UNIT_SAVE_ID, EE_UNITS_UNIT_SAVE_ID_FRAME).0, 6);
    assert_eq!(ask(&u, EE_UNITS_UNIT_SAVE_ID, EE_UNITS_UNIT_SAVE_ID_COMMAND).0, 0);
    assert_eq!(ask(&u, EE_UNITS_UNIT_SAVE_ID, EE_UNITS_UNIT_SAVE_ID_LAUNCHER_CLASS).0, 0);
    // `(long)` of the uint32_t value: -1 where long is 32 bits
    assert_eq!(ask(&u, EE_UNITS_UNIT_SAVE_ID, EE_UNITS_UNIT_SAVE_ID_CONVERSION_EFFECT).0, 0xffff_ffffu32 as c_long as i64);
    assert_eq!(ask(&u, EE_UNITS_UNIT_SAVE_ID, EE_UNITS_UNIT_SAVE_ID_HEAL_EFFECT).0, 3);
    // no behaviour table registered: no entry matches a set pointer
    u.ptrs[8] = 0x1234;
    assert_eq!(ask(&u, EE_UNITS_UNIT_SAVE_ID, EE_UNITS_UNIT_SAVE_ID_BEHAVIOR).0, 8);
}

#[test]
fn queries_and_fallbacks() {
    let _l = with_test_layout(layout());
    let mut u = fake(5, 2);
    let mut stats = Box::<FakeStats>::default();
    stats.fields.mMaxHitPoints = 250;
    stats.fields.mPower = 12;
    u.stats = addr(&*stats);
    u.collision = [3, -4];
    u.contact = [1.0, 2.0, 3.5];
    u.scale = 1.25;
    assert_eq!(ask(&u, EE_UNITS_UNIT_SAVE_MAX, EE_UNITS_UNIT_SAVE_MAX_HIT_POINTS).0, 250);
    assert_eq!(ask(&u, EE_UNITS_UNIT_SAVE_MAX, EE_UNITS_UNIT_SAVE_MAX_POWER).0, 12);
    assert_eq!(ask(&u, EE_UNITS_UNIT_SAVE_COLLISION_TILE, 1).0, -4);
    let mut rec = Rec::default();
    let mut io = EeUnitsUnitSaveIo::default();
    ViewSaveHost { inner: &mut rec, unit: addr(&*u) as u64 }.op(
        EE_UNITS_UNIT_SAVE_FLOAT,
        EE_UNITS_UNIT_SAVE_FLOAT_CONTACT_Z,
        &mut io,
    );
    assert_eq!(io.f, 3.5);
    assert!(rec.ops.is_empty());
    // no stats: the C++ host's (its own outcome)
    u.stats = 0;
    let (v, ops) = ask(&u, EE_UNITS_UNIT_SAVE_MAX, EE_UNITS_UNIT_SAVE_MAX_POWER);
    assert_eq!((v, ops.len()), (77, 1));
    // a section of a deque and an EEUnit's id: the C++ host's
    assert_eq!(ask(&u, EE_UNITS_UNIT_SAVE_SECTION, EE_UNITS_UNIT_SAVE_SECTION_WAYPOINTS).1.len(), 1);
    let plain = fake(1, 1);
    assert_eq!(ask(&plain, EE_UNITS_UNIT_SAVE_ID, EE_UNITS_UNIT_SAVE_ID_HOME_BASE).0, 77);
}

#[test]
fn sections_write_with_the_original_overloads() {
    let _l = with_test_layout(layout());
    let mut u = fake(5, 2);
    let a = fake(21, 2);
    let b = fake(22, 2);
    let held = Box::new([addr(&*a), addr(&*b)]);
    u.held = [addr(&held[0]), addr(&held[0]) + 2 * std::mem::size_of::<usize>(), 0];
    let queue: Box<[c_long; 4]> = Box::new([7, 1, 8, 0]);
    u.build_queue = [addr(&queue[0]), addr(&queue[0]) + 4 * std::mem::size_of::<c_long>(), 0];
    let n2 = Box::new(Node { data: addr(&*b), prev: 0, next: 0 });
    let n1 = Box::new(Node { data: addr(&*a), prev: 0, next: addr(&*n2) });
    u.contacts_head = addr(&*n1);
    u.contacts_count = 2;
    let long = EE_UNITS_UNIT_SAVE_WRITE_LONG;
    let ulong = EE_UNITS_UNIT_SAVE_WRITE_ULONG;
    let ops = ask(&u, EE_UNITS_UNIT_SAVE_SECTION, EE_UNITS_UNIT_SAVE_SECTION_HELD_UNITS).1;
    assert_eq!(ops, vec![(long, 0, 2), (ulong, 0, 21), (ulong, 0, 22)]);
    let ops = ask(&u, EE_UNITS_UNIT_SAVE_SECTION, EE_UNITS_UNIT_SAVE_SECTION_BUILD_QUEUE).1;
    assert_eq!(ops, vec![(long, 0, 2), (long, 0, 7), (ulong, 0, 1), (long, 0, 8), (ulong, 0, 0)]);
    let ops = ask(&u, EE_UNITS_UNIT_SAVE_SECTION, EE_UNITS_UNIT_SAVE_SECTION_CONTACTS).1;
    assert_eq!(ops, vec![(ulong, 0, 2), (ulong, 0, 21), (ulong, 0, 22)]);
    let ops = ask(&u, EE_UNITS_UNIT_SAVE_SECTION, EE_UNITS_UNIT_SAVE_SECTION_UNIT_VARIABLES).1;
    assert_eq!(ops, vec![(long, 0, 0)]);
    let ops = ask(&u, EE_UNITS_UNIT_SAVE_SECTION, EE_UNITS_UNIT_SAVE_SECTION_BOUNDED_PATH_TILES).1;
    assert_eq!(ops, vec![(ulong, 0, 0)]);
}

#[test]
fn a_failed_write_stops_the_loop() {
    struct Fail(Vec<i32>);
    impl SaveHost for Fail {
        fn op(&mut self, op: i32, _index: u32, _io: &mut EeUnitsUnitSaveIo) -> i32 {
            self.0.push(op);
            if self.0.len() == 2 {
                5
            } else {
                0
            }
        }
    }
    let _l = with_test_layout(layout());
    let mut u = fake(5, 2);
    let vars: Box<[c_long; 3]> = Box::new([1, 2, 3]);
    u.vars = [addr(&vars[0]), addr(&vars[0]) + 3 * std::mem::size_of::<c_long>(), 0];
    let mut f = Fail(Vec::new());
    let r = ViewSaveHost { inner: &mut f, unit: addr(&*u) as u64 }.op(
        EE_UNITS_UNIT_SAVE_SECTION,
        EE_UNITS_UNIT_SAVE_SECTION_UNIT_VARIABLES,
        &mut EeUnitsUnitSaveIo::default(),
    );
    assert_eq!((r, f.0.len()), (5, 2));
}

#[test]
fn load_stores_in_place() {
    let _l = with_test_layout(layout());
    let mut u = fake(5, 2);
    let mut rec = Rec::default();
    let at = &mut *u as *mut Fake as usize as u64;
    let mut set = |index: u32, i: i64| {
        let mut io = EeUnitsUnitSaveIo { i, ..Default::default() };
        ViewLoadHost { inner: &mut rec, unit: at }.op(EE_UNITS_UNIT_LOAD_SET, index, &mut io);
    };
    set(EE_UNITS_UNIT_LOAD_SET_HOME_BASE, 0xffff_ffff);
    set(EE_UNITS_UNIT_LOAD_SET_OWNER, 0xffff_ffff);
    set(EE_UNITS_UNIT_LOAD_SET_MINE_UNSET, 0);
    set(EE_UNITS_UNIT_LOAD_SET_COLLISION_TILE_Y, -7);
    set(EE_UNITS_UNIT_LOAD_SET_FRAME, 4);
    set(EE_UNITS_UNIT_LOAD_SET_HERD, 12);
    // the scale's rule and the actions are the C++ host's
    set(EE_UNITS_UNIT_LOAD_SET_SCALE, 0);
    set(EE_UNITS_UNIT_LOAD_SET_ACTION, 3);
    // reinterpret_cast of the unsigned long; USavedPointer sign-extends the low 32 bits
    assert_eq!(u.ptrs[2], 0xffff_ffffu32 as c_ulong as usize);
    assert_eq!(u.ptrs[5], usize::MAX);
    assert_eq!(u.ptrs[6], usize::MAX);
    assert_eq!((u.collision[1], u.frame, u.herd), (-7, 4, 12));
    assert_eq!(rec.ops.iter().map(|o| o.1).collect::<Vec<_>>(), vec![EE_UNITS_UNIT_LOAD_SET_SCALE, EE_UNITS_UNIT_LOAD_SET_ACTION]);
}
