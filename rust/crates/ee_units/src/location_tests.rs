//! The location goal's bodies over a mock host (each test its own goal handles: the tests share the static table).

use super::*;
use crate::rust_goal::*;
use ee_ge::types::GeTransformation;

const U: u64 = 0x7100;

#[derive(Default)]
struct World {
    log: Vec<String>,
    category: i64,
    transformation: GeTransformation,
    queue_len: i64,
    file: Vec<u32>,
    reads: Vec<u32>,
    fail_read_at: Option<usize>,
    closed: bool,
    text: String,
    next_goal: u64,
}

impl GoalHost for World {
    fn reference(&mut self, o: u64, g: bool) {
        self.log.push(format!("ref {o:x} {g}"));
    }
    fn dereference(&mut self, o: u64, g: bool) {
        self.log.push(format!("deref {o:x} {g}"));
    }
    fn unit_id(&mut self, u: u64) -> u32 {
        u as u32
    }
    fn unit_tile(&mut self, _u: u64) -> (i64, i64) {
        (0, 0)
    }
    fn unit_valid(&mut self, _u: u64) -> bool {
        true
    }
    fn unit_command(&mut self, _u: u64) -> i64 {
        0
    }
    fn reset_path_attempts(&mut self, _g: u64) {
        self.log.push("attempts 0".into());
    }
    fn start_action(&mut self, u: u64, a: i64) {
        self.log.push(format!("start {u:x} {a}"));
    }
    fn find_unit(&mut self, id: u32, _c: bool) -> u64 {
        u64::from(id)
    }
    fn unit_position(&mut self, _u: u64) -> [f32; 3] {
        [0.0; 3]
    }
    fn tile_location(&mut self, _x: i64, _y: i64) -> [f32; 3] {
        [0.0; 3]
    }
    fn tell_group(&mut self, u: u64, p: [f32; 3]) {
        self.log.push(format!("tell {u:x} {p:?}"));
    }
    fn set_physics_action(&mut self, _u: u64, _a: i64) {}
    fn set_animation(&mut self, _u: u64, _a: i64) {}
    fn drop_path(&mut self, _u: u64) {}
    fn new_goal(&mut self, kind: u8, arg: bool) -> u64 {
        self.log.push(format!("new {kind} {arg}"));
        self.next_goal
    }
    fn file_open(&mut self) -> bool {
        !self.closed
    }
    fn write_u32(&mut self, v: u32) -> i32 {
        self.file.push(v);
        0
    }
    fn write_i32(&mut self, v: i32) -> i32 {
        self.file.push(v as u32);
        0
    }
    fn write_bool(&mut self, v: bool) -> i32 {
        self.file.push(u32::from(v));
        0
    }
    fn read_u32(&mut self, v: &mut u32) -> i32 {
        self.read(v)
    }
    fn read_i32(&mut self, v: &mut i32) -> i32 {
        let mut x = *v as u32;
        let r = self.read(&mut x);
        *v = x as i32;
        r
    }
    fn read_bool(&mut self, v: &mut bool) -> i32 {
        let mut x = u32::from(*v);
        let r = self.read(&mut x);
        *v = x != 0;
        r
    }
    fn append(&mut self, text: &str) {
        self.text.push_str(text);
    }
    fn append_class_name(&mut self, _u: u64) {}
}

impl World {
    /// one read of the file; `fail_read_at` fails that read (status 2, the field unchanged)
    fn read(&mut self, v: &mut u32) -> i32 {
        if self.fail_read_at == Some(0) {
            return 2;
        }
        self.fail_read_at = self.fail_read_at.map(|n| n - 1);
        *v = self.reads.remove(0);
        0
    }
}

impl LocationHost for World {
    fn unit_category(&mut self, _u: u64) -> i64 {
        self.category
    }
    fn unit_transformation(&mut self, _u: u64) -> GeTransformation {
        self.transformation
    }
    fn set_transformation(&mut self, u: u64, t: &GeTransformation) {
        self.log.push(format!("transform {u:x}"));
        self.transformation = *t;
    }
    fn set_target_search_timer(&mut self, u: u64, v: i64) {
        self.log.push(format!("search timer {u:x} {v}"));
    }
    fn goal_queue_len(&mut self, _u: u64) -> i64 {
        self.log.push("queue".into());
        self.queue_len
    }
    fn write_f32(&mut self, v: f32) -> i32 {
        self.file.push(v.to_bits());
        0
    }
    fn read_f32(&mut self, v: &mut f32) -> i32 {
        let mut x = v.to_bits();
        let r = self.read(&mut x);
        *v = f32::from_bits(x);
        r
    }
}

fn new(goal: u64, location: [f32; 3], yaw: f32, flags: [i64; 4]) -> World {
    let mut w = World::default();
    let mut io = EeUnitsGoalIo::default();
    io.f[..3].copy_from_slice(&location);
    io.f[3] = yaw;
    io.i[..4].copy_from_slice(&flags);
    body(EE_UNITS_GOAL_NEW_LOCATION, goal, 0, 0, &mut io, &mut w);
    w
}

fn run(op: i32, goal: u64, a: u64, b: u64, w: &mut World) -> (i64, EeUnitsGoalIo) {
    let mut io = EeUnitsGoalIo::default();
    let r = body(op, goal, a, b, &mut io, w);
    (r, io)
}

#[test]
fn create_clone_destroy() {
    let g = 0x71_0001;
    let mut w = new(g, [1.5, 2.5, 3.0], 0.75, [1, 1, 0, 4]);
    let x = get(g);
    assert_eq!(x.location, [1.5, 2.5, 3.0]);
    assert_eq!(x.yaw, 0.75);
    assert!(x.forced_move && x.should_face && !x.auto_waypoint && !x.group_movement);
    assert_eq!(x.group_path_state, 4);
    // the group movement flag is copied by Clone, not set by a constructor
    // SAFETY: a valid record
    unsafe { ee_units_location_goal_set(g, &EeUnitsLocationGoal { group_movement: true, ..x }) };
    w.next_goal = 0x71_0002;
    let (copy, _) = run(EE_UNITS_GOAL_CLONE, g, 0, 1, &mut w);
    assert_eq!(copy as u64, 0x71_0002);
    assert_eq!(w.log, ["new 1 true"]);
    assert_eq!(get(0x71_0002), EeUnitsLocationGoal { group_movement: true, ..x });
    let (loc, io) = run(EE_UNITS_GOAL_LOCATION, g, 0, 0, &mut w);
    assert_eq!(loc, 0);
    assert_eq!(io.f[..3], [1.5, 2.5, 3.0]);
    run(EE_UNITS_GOAL_DESTINATION, g, U, 0, &mut w);
    assert_eq!(w.log.last().unwrap(), "tell 7100 [1.5, 2.5, 3.0]");
    assert_eq!(run(EE_UNITS_GOAL_START_ACTION, g, 0, 0, &mut w).0, 8000);
    run(EE_UNITS_GOAL_DESTROY, g, 0, 0, &mut w);
    run(EE_UNITS_GOAL_DESTROY, 0x71_0002, 0, 0, &mut w);
    assert_eq!(get(g), EeUnitsLocationGoal::default());
    let mut out = EeUnitsLocationGoal { yaw: 9.0, ..Default::default() };
    // SAFETY: a writable record
    unsafe { ee_units_location_goal_get(0x71_0002, &mut out) };
    assert_eq!(out, EeUnitsLocationGoal::default());
}

#[test]
fn checksum_and_texts() {
    let g = 0x72_0001;
    let mut w = new(g, [1.0, -2.0, 0.5], 0.25, [0, 1, 1, 3]);
    // SAFETY: a valid record
    unsafe { ee_units_location_goal_set(g, &EeUnitsLocationGoal { group_movement: true, ..get(g) }) };
    let want = (1.0f32.to_bits() as i32)
        .wrapping_add((-2.0f32).to_bits() as i32)
        .wrapping_add(0.5f32.to_bits() as i32)
        .wrapping_add(0.25f32.to_bits() as i32 + 1 + 1 + 1 + 3);
    assert_eq!(run(EE_UNITS_GOAL_CHECKSUM, g, 0, 0, &mut w).0, i64::from(want));
    run(EE_UNITS_GOAL_DEBUG_LINE, g, 0, 0, &mut w);
    assert_eq!(w.text, "loc 1.00,-2.00,0.50: auto(1)");
    w.text.clear();
    run(EE_UNITS_GOAL_DEBUG_TEXT, g, 0, 0, &mut w);
    assert_eq!(w.text, "Location: 1.00,-2.00,0.50\nAutoWaypoint:true\nYaw:0.2500\nAttackMove:true\nShouldFace:true\n");
    w.text.clear();
    run(EE_UNITS_GOAL_DUMP, g, 0, 0, &mut w);
    let p = "OUT OF SYNCH DUMP:  EEUGLocation - ";
    assert_eq!(
        w.text,
        format!(
            "{p}Location is (1.000000, -2.000000, 0.500000)\n{p}Yaw is 0.250000\n{p}Forced Move Flag is 0\n\
             {p}Should Face Flag is 1\n{p}Is Auto Waypoint Flag is 1\n{p}Use Group Movement Flag is 1\n\
             {p}Group Path State is 3\n"
        )
    );
    run(EE_UNITS_GOAL_DESTROY, g, 0, 0, &mut w);
}

fn at(x: f32, y: f32, z: f32, yaw: f32) -> GeTransformation {
    let mut t = GeTransformation::IDENTITY;
    t.set_orientation(RotationOrder::Ypr, yaw, 0.0, 0.0);
    t.m[0][3] = x;
    t.m[1][3] = y;
    t.m[2][3] = z;
    t
}

#[test]
fn start_by_place_and_flags() {
    let g = 0x73_0001;
    // elsewhere, unforced: the target search timer reset, then prepare to move
    let mut w = new(g, [4.0, 5.0, 0.0], 0.0, [0, 1, 0, 0]);
    w.category = 1;
    w.transformation = at(1.0, 1.0, 0.0, 0.0);
    run(EE_UNITS_GOAL_START, g, U, 0, &mut w);
    assert_eq!(w.log, ["search timer 7100 0", "start 7100 8000"]);
    // on the location: turn to face (the facing flag), else idle
    w.log.clear();
    w.transformation = at(4.0, 5.0, 0.0, 1.0);
    run(EE_UNITS_GOAL_START, g, U, 0, &mut w);
    assert_eq!(w.log, ["start 7100 9006"]);
    let g2 = 0x73_0002;
    let mut w2 = new(g2, [4.0, 5.0, 0.0], 0.0, [1, 0, 0, 0]);
    w2.transformation = at(4.0, 5.0, 0.0, 0.0);
    run(EE_UNITS_GOAL_START, g2, U, 0, &mut w2);
    assert_eq!(w2.log, ["start 7100 5000"]);
    // forced elsewhere: no timer reset
    w2.log.clear();
    w2.transformation = at(4.0, 5.5, 0.0, 0.0);
    run(EE_UNITS_GOAL_START, g2, U, 0, &mut w2);
    assert_eq!(w2.log, ["start 7100 8000"]);
    // an aircraft prepares to move even on the location, its timer untouched
    for category in [CATEGORY_AIRPLANE, CATEGORY_PLANET_SPACESHIP] {
        w.log.clear();
        w.category = category;
        w.transformation = at(4.0, 5.0, 0.0, 0.0);
        run(EE_UNITS_GOAL_START, g, U, 0, &mut w);
        assert_eq!(w.log, ["start 7100 8000"]);
    }
    run(EE_UNITS_GOAL_DESTROY, g, 0, 0, &mut w);
    run(EE_UNITS_GOAL_DESTROY, g2, 0, 0, &mut w2);
}

fn heading(t: &GeTransformation) -> f32 {
    let d = t.apply_vector(GeVector { x: 0.0, y: 1.0, z: 0.0 });
    um_arctan2(-d.x, d.y)
}

#[test]
fn facing_triggers() {
    let g = 0x74_0001;
    let mut w = new(g, [0.0; 3], 0.0, [1, 1, 0, 0]);
    let t = at(2.0, 3.0, 0.5, 0.5);
    let h = heading(&t);
    // within a degree: snapped to the goal's yaw, the position kept
    let yaw = h + 0.01;
    // SAFETY: a valid record
    unsafe { ee_units_location_goal_set(g, &EeUnitsLocationGoal { yaw, ..get(g) }) };
    w.transformation = t;
    assert_eq!(run(EE_UNITS_GOAL_EVALUATE, g, U, 9015, &mut w).0, 1);
    assert_eq!(w.log, ["transform 7100"]);
    let mut want = t;
    want.set_orientation(RotationOrder::Ypr, yaw, 0.0, 0.0);
    assert_eq!(w.transformation, want);
    assert_eq!([w.transformation.x(), w.transformation.y(), w.transformation.z()], [2.0, 3.0, 0.5]);
    // two degrees off: false, nothing set
    w.log.clear();
    w.transformation = t;
    // SAFETY: a valid record
    unsafe { ee_units_location_goal_set(g, &EeUnitsLocationGoal { yaw: h + 0.035, ..get(g) }) };
    assert_eq!(run(EE_UNITS_GOAL_FACE_YAW, g, U, 0, &mut w).0, 0);
    assert!(w.log.is_empty());
    // the difference wraps around the half turn
    let t = at(0.0, 0.0, 0.0, 3.13);
    let h = heading(&t);
    w.transformation = t;
    // SAFETY: a valid record
    unsafe { ee_units_location_goal_set(g, &EeUnitsLocationGoal { yaw: h - TAU + 0.005, ..get(g) }) };
    assert_eq!(run(EE_UNITS_GOAL_FACE_YAW, g, U, 0, &mut w).0, 1);
    // facing not required: no facing, or more than one goal queued (read only when facing)
    w.log.clear();
    w.queue_len = 1;
    assert_eq!(run(EE_UNITS_GOAL_EVALUATE, g, U, 9014, &mut w).0, 0);
    w.queue_len = 2;
    assert_eq!(run(EE_UNITS_GOAL_EVALUATE, g, U, 9014, &mut w).0, 1);
    assert_eq!(w.log, ["queue", "queue"]);
    // SAFETY: a valid record
    unsafe { ee_units_location_goal_set(g, &EeUnitsLocationGoal { should_face: false, ..get(g) }) };
    w.log.clear();
    assert_eq!(run(EE_UNITS_GOAL_EVALUATE, g, U, 9014, &mut w).0, 1);
    assert!(w.log.is_empty());
    // any other event: the base goal's
    assert_eq!(run(EE_UNITS_GOAL_EVALUATE, g, U, 9013, &mut w).0, 0);
    run(EE_UNITS_GOAL_DESTROY, g, 0, 0, &mut w);
}

#[test]
fn save_and_load_by_version() {
    let g = 0x75_0001;
    let mut w = new(g, [1.0, 2.0, 3.0], 0.5, [1, 1, 1, 7]);
    // SAFETY: a valid record
    unsafe { ee_units_location_goal_set(g, &EeUnitsLocationGoal { group_movement: true, ..get(g) }) };
    assert_eq!(run(EE_UNITS_GOAL_SAVE, g, 0, 0x10c, &mut w).0, 0);
    let all = [1.0f32.to_bits(), 2.0f32.to_bits(), 3.0f32.to_bits(), 1, 0.5f32.to_bits(), 1, 1, 1];
    assert_eq!(w.file, all);
    // the current version reads everything back (the group path state is not saved)
    let g2 = 0x75_0002;
    let mut w2 = new(g2, [0.0; 3], 0.0, [0, 0, 0, 0]);
    w2.reads = all.to_vec();
    assert_eq!(run(EE_UNITS_GOAL_LOAD, g2, 0, 0x10c, &mut w2).0, 0);
    assert_eq!(get(g2), EeUnitsLocationGoal { group_path_state: 0, ..get(g) });
    // older files: the fields their version has
    for (minor, count) in [(14u64, 3usize), (15, 4), (25, 4), (26, 6), (134, 6), (135, 7), (172, 7), (173, 8)] {
        let g3 = 0x75_0100 + minor;
        let mut w3 = new(g3, [0.0; 3], 0.0, [0, 0, 0, 0]);
        w3.reads = all[..count].to_vec();
        w3.reads.push(0xdead);
        assert_eq!(run(EE_UNITS_GOAL_LOAD, g3, 0, minor, &mut w3).0, 0);
        assert_eq!(w3.reads, [0xdead], "minor {minor}");
        let x = get(g3);
        assert_eq!(x.forced_move, count >= 4);
        assert_eq!(x.yaw, if count >= 6 { 0.5 } else { 0.0 });
        assert_eq!(x.auto_waypoint, count >= 7);
        assert_eq!(x.group_movement, count >= 8);
        run(EE_UNITS_GOAL_DESTROY, g3, 0, 0, &mut w3);
    }
    // a later major version reads every field
    let g4 = 0x75_0003;
    let mut w4 = new(g4, [0.0; 3], 0.0, [0, 0, 0, 0]);
    w4.reads = all.to_vec();
    assert_eq!(run(EE_UNITS_GOAL_LOAD, g4, 1, 0, &mut w4).0, 0);
    assert!(w4.reads.is_empty());
    // a failing read stops the rest, keeping what was read before it
    let g5 = 0x75_0004;
    let mut w5 = new(g5, [0.0; 3], 0.0, [0, 0, 0, 0]);
    w5.reads = all.to_vec();
    w5.fail_read_at = Some(1);
    assert_eq!(run(EE_UNITS_GOAL_LOAD, g5, 0, 0x10c, &mut w5).0, 2);
    assert_eq!(get(g5).location, [1.0, 0.0, 0.0]);
    // a closed file: 4, nothing read or written
    let mut w6 = new(g5, [0.0; 3], 0.0, [0, 0, 0, 0]);
    w6.closed = true;
    assert_eq!(run(EE_UNITS_GOAL_SAVE, g5, 0, 0x10c, &mut w6).0, 4);
    assert_eq!(run(EE_UNITS_GOAL_LOAD, g5, 0, 0x10c, &mut w6).0, 4);
    assert!(w6.file.is_empty());
    for goal in [g, g2, g4, g5] {
        run(EE_UNITS_GOAL_DESTROY, goal, 0, 0, &mut w);
    }
}

#[test]
fn dispatch_defaults() {
    let g = 0x76_0001;
    let mut w = new(g, [0.0; 3], 0.0, [0, 0, 0, 0]);
    for op in [
        EE_UNITS_GOAL_STEP,
        EE_UNITS_GOAL_SET_OWNER,
        EE_UNITS_GOAL_ABANDON,
        EE_UNITS_GOAL_ADD_CREATED_UNIT,
        EE_UNITS_GOAL_WORK,
        EE_UNITS_GOAL_PATH_TAKEN,
        EE_UNITS_GOAL_REPLACE_UNIT,
        EE_UNITS_GOAL_REPAIR,
    ] {
        assert_eq!(run(op, g, U, 0, &mut w).0, 0);
    }
    assert!(w.log.is_empty());
    run(EE_UNITS_GOAL_DESTROY, g, 0, 0, &mut w);
}
