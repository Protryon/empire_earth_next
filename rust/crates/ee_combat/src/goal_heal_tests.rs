use super::*;
use crate::goal_attack::{
    EE_COMBAT_G_COMPLEX, EE_COMBAT_G_DEREFERENCE, EE_COMBAT_G_FIND_UNIT, EE_COMBAT_G_ID, EE_COMBAT_G_READ_BOOL,
    EE_COMBAT_G_READ_U32, EE_COMBAT_G_REFERENCE, EE_COMBAT_G_WRITE_BOOL, EE_COMBAT_G_WRITE_U32,
};
use crate::goal_dispatch::{
    body, EE_COMBAT_GOAL_NEW_HEAL, EE_COMBAT_G_CAPTURE, EE_COMBAT_G_CLASS_NAME, EE_COMBAT_G_EVALUATE,
    EE_COMBAT_G_MAX_HIT_POINTS, EE_COMBAT_G_STEP, HOST_HIT_POINTS, HOST_NEW_GOAL, HOST_UNIT_ID, HOST_UNIT_TILE,
    OP_CHECKSUM, OP_CLONE, OP_DEBUG_LINE, OP_DEBUG_TEXT, OP_DESTINATION, OP_DESTROY, OP_DUMP, OP_EVALUATE, OP_LOCATION,
    OP_MAX_PATH_ATTEMPTS, OP_REPAIR, OP_REPLACE, OP_SAVE, OP_START, OP_START_ACTION, OP_STEP, OP_SUITS, OP_WORK,
};
use std::collections::BTreeMap;
use std::sync::atomic::{AtomicU64, Ordering};

// goal handles unique across the crate's tests (they run at once over the same record table)
static NEXT_GOAL: AtomicU64 = AtomicU64::new(0x7_0000);

fn new_goal() -> u64 {
    NEXT_GOAL.fetch_add(0x10, Ordering::Relaxed)
}

/// A mock world: units by handle (location, id, references, complex unit, hit points), the file, every operation.
#[derive(Default)]
struct World {
    units: BTreeMap<u64, Unit>,
    found: BTreeMap<i64, u64>,
    file: Vec<(i32, i64)>,
    read_at: usize,
    fail_read_at: Option<usize>,
    log: Vec<(i32, u64, [i64; 8])>,
    can_heal: bool,
    env: (i64, i64),
}

#[derive(Clone, Default)]
struct Unit {
    location: [f32; 3],
    id: i64,
    refs: i64,
    released: i64,
    complex: u64,
    hp: i64,
}

impl World {
    fn unit(&mut self, h: u64) -> &mut Unit {
        self.units.entry(h).or_default()
    }
}

impl GoalHost for World {
    fn op(&mut self, code: i32, unit: u64, args: &mut EeCombatGoalArgs) -> i64 {
        self.op_ab(code, unit, 0, args)
    }
    fn op_ab(&mut self, code: i32, unit: u64, _b: u64, args: &mut EeCombatGoalArgs) -> i64 {
        self.log.push((code, unit, *args));
        let a0 = args[0];
        match code {
            EE_COMBAT_G_UNIT_LOCATION => {
                let l = self.unit(unit).location;
                for (slot, v) in args.iter_mut().zip(l) {
                    *slot = i64::from(v.to_bits());
                }
                0
            }
            EE_COMBAT_G_REFERENCE => {
                self.unit(unit).refs += 1;
                0
            }
            EE_COMBAT_G_DEREFERENCE => {
                self.unit(unit).refs -= 1;
                0
            }
            EE_COMBAT_G_RELEASE_HEAL_EFFECT => {
                self.unit(unit).released += 1;
                0
            }
            EE_COMBAT_G_ID | HOST_UNIT_ID => self.unit(unit).id,
            EE_COMBAT_G_COMPLEX => self.unit(unit).complex as i64,
            EE_COMBAT_G_FIND_UNIT => *self.found.get(&a0).unwrap_or(&0) as i64,
            EE_COMBAT_G_SET_HIT_POINTS => {
                self.unit(unit).hp = a0;
                0
            }
            EE_COMBAT_G_CAN_HEAL => i64::from(self.can_heal),
            EE_COMBAT_G_HEAL_ENV => {
                args[0] = self.env.0;
                args[1] = self.env.1;
                0
            }
            HOST_HIT_POINTS => self.unit(unit).hp,
            HOST_UNIT_TILE => {
                args[0] = 12;
                args[1] = 34;
                0
            }
            EE_COMBAT_G_MAX_HIT_POINTS => 200,
            EE_COMBAT_G_CLASS_NAME => c"Medic".as_ptr() as i64,
            HOST_NEW_GOAL => 0x5_0000 + unit as i64,
            EE_COMBAT_G_WRITE_BOOL | EE_COMBAT_G_WRITE_U32 => {
                self.file.push((code, a0));
                0
            }
            EE_COMBAT_G_READ_BOOL | EE_COMBAT_G_READ_U32 => {
                if self.fail_read_at == Some(self.read_at) {
                    return 7;
                }
                let (c, v) = self.file[self.read_at];
                assert_eq!(c + 1, code, "read {} of a written {}", code, c);
                self.read_at += 1;
                args[0] = v;
                0
            }
            _ => 0,
        }
    }
}

fn bits(p: [f32; 3]) -> [u32; 3] {
    [p[0].to_bits(), p[1].to_bits(), p[2].to_bits()]
}

fn world() -> World {
    let mut w = World::default();
    *w.unit(0x100) = Unit { location: [3.5, -1.25, 2.0], id: 0x0200_0041, hp: 50, ..Default::default() };
    *w.unit(0x200) = Unit { location: [7.0, 8.0, 0.0], id: 0x0200_0042, hp: 90, ..Default::default() };
    w
}

#[test]
fn the_goal_is_made_at_its_targets_position_and_holds_it() {
    let mut w = world();
    let goal = new_goal();
    create(&mut w, goal, 0x100);
    assert_eq!(heal_of(goal), EeCombatHealGoal { location: bits([3.5, -1.25, 2.0]), target: 0x100, elapsed: 0 });
    assert_eq!(w.unit(0x100).refs, 1);
    // the load's goal: nothing
    let empty = new_goal();
    create_empty(empty);
    assert_eq!(heal_of(empty), EeCombatHealGoal::default());
    // a new target: the reference moved, the clock restarted, the location kept
    put(goal, EeCombatHealGoal { elapsed: 300, ..heal_of(goal) });
    replace_unit(&mut w, goal, 0x999, 0x200);
    assert_eq!(heal_of(goal).target, 0x100, "another unit replaced: nothing");
    replace_unit(&mut w, goal, 0x100, 0x200);
    let x = heal_of(goal);
    assert_eq!((x.target, x.elapsed, x.location), (0x200, 0, bits([3.5, -1.25, 2.0])));
    assert_eq!((w.unit(0x100).refs, w.unit(0x200).refs), (0, 1));
    // the destructor ends the target's heal effect and lets it go
    destroy(&mut w, goal);
    assert_eq!((w.unit(0x200).refs, w.unit(0x200).released), (0, 1));
    destroy(&mut w, empty);
    assert_eq!(w.log.iter().filter(|o| o.0 == EE_COMBAT_G_RELEASE_HEAL_EFFECT).count(), 1);
    with_goals(|g| {
        g.heal.remove(&goal);
        g.heal.remove(&empty);
    });
}

#[test]
fn the_checksum_and_the_location() {
    let mut w = world();
    let goal = new_goal();
    create(&mut w, goal, 0x100);
    put(goal, EeCombatHealGoal { elapsed: 0x7fff_ffff, ..heal_of(goal) });
    // the id plus the elapsed time, wrapping as the original's long sum
    assert_eq!(checksum(&mut w, goal), 0x0200_0041i32.wrapping_add(0x7fff_ffff));
    // the goal location is where the target is now; the destination tells the group
    w.unit(0x100).location = [9.0, 9.5, 1.0];
    assert_eq!(location(&mut w, goal, [1, 2, 3]), bits([9.0, 9.5, 1.0]));
    assert_eq!(destination(&mut w, goal, 0x300, [1, 2, 3]), bits([9.0, 9.5, 1.0]));
    let told = w.log.last().unwrap();
    assert_eq!((told.0, told.1, told.2[0]), (EE_COMBAT_G_TELL_GROUP, 0x300, i64::from(9.0f32.to_bits())));
    // without a target: 0, and the point as it was
    let empty = new_goal();
    create_empty(empty);
    assert_eq!(checksum(&mut w, empty), 0);
    assert_eq!(location(&mut w, empty, [1, 2, 3]), [1, 2, 3]);
    with_goals(|g| {
        g.heal.remove(&goal);
        g.heal.remove(&empty);
    });
}

#[test]
fn save_and_load_write_no_base_goal_and_load_the_saved_id() {
    let mut w = world();
    let goal = new_goal();
    create(&mut w, goal, 0x100);
    put(goal, EeCombatHealGoal { elapsed: 640, ..heal_of(goal) });
    assert_eq!(save(&mut w, goal), 0);
    assert_eq!(
        w.file,
        vec![(EE_COMBAT_G_WRITE_BOOL, 1), (EE_COMBAT_G_WRITE_U32, 0x0200_0041), (EE_COMBAT_G_WRITE_U32, 640)]
    );
    let loaded = new_goal();
    create_empty(loaded);
    assert_eq!(load(&mut w, loaded), 0);
    // the id in the target's place, the location not saved (0, 0, 0 as the load's goal had)
    assert_eq!(heal_of(loaded), EeCombatHealGoal { location: [0; 3], target: 0x0200_0041, elapsed: 640 });
    // a goal without a target writes the flag only
    let empty = new_goal();
    create_empty(empty);
    let mut v = World::default();
    assert_eq!(save(&mut v, empty), 0);
    assert_eq!(v.file, vec![(EE_COMBAT_G_WRITE_BOOL, 0)]);
    assert_eq!(load(&mut v, empty), 0);
    assert_eq!(heal_of(empty), EeCombatHealGoal::default());
    // a failed read of the elapsed time keeps the id read before it
    w.read_at = 0;
    w.fail_read_at = Some(2);
    let failed = new_goal();
    create_empty(failed);
    assert_eq!(load(&mut w, failed), 7);
    assert_eq!((heal_of(failed).target, heal_of(failed).elapsed), (0x0200_0041, 0));
    // a failed id read keeps no target
    w.read_at = 0;
    w.fail_read_at = Some(1);
    let failed2 = new_goal();
    create_empty(failed2);
    assert_eq!(load(&mut w, failed2), 7);
    assert_eq!(heal_of(failed2).target, 0);
    with_goals(|g| {
        for k in [goal, loaded, empty, failed, failed2] {
            g.heal.remove(&k);
        }
    });
}

#[test]
fn repair_finds_the_complex_unit_or_answers_three() {
    let mut w = world();
    w.found.insert(0x41, 0x500);
    w.unit(0x500).complex = 0x100;
    w.found.insert(0x43, 0x600);
    let goal = new_goal();
    put(goal, EeCombatHealGoal { target: 0x41, elapsed: 5, ..Default::default() });
    assert_eq!(repair(&mut w, goal), 0);
    assert_eq!((heal_of(goal).target, w.unit(0x100).refs), (0x100, 1));
    // the unit gone: 3, no target
    put(goal, EeCombatHealGoal { target: 0x42, ..Default::default() });
    assert_eq!(repair(&mut w, goal), 3);
    assert_eq!(heal_of(goal).target, 0);
    // a unit without a complex unit: 3, no target
    put(goal, EeCombatHealGoal { target: 0x43, ..Default::default() });
    assert_eq!(repair(&mut w, goal), 3);
    assert_eq!(heal_of(goal).target, 0);
    // no target: nothing looked up
    let before = w.log.len();
    assert_eq!(repair(&mut w, goal), 0);
    assert_eq!(w.log.len(), before);
    with_goals(|g| g.heal.remove(&goal));
}

#[test]
fn the_work_heals_each_full_second_over_the_stores_reads() {
    use crate::store::tests::{put_stats, put_unit};
    use crate::store::{EeCombatStoredStats, EeCombatStoredUnit};
    let (healer, target) = (0x7_1100u64, 0x7_1200u64);
    put_unit(EeCombatStoredUnit { object: healer, player: 0x9, command: 16001, stats: 0x7_1300, ..Default::default() });
    put_unit(EeCombatStoredUnit {
        object: target,
        player: 0x9,
        hit_points: 150,
        stats: 0x7_1400,
        ..Default::default()
    });
    put_stats(0x7_1300, EeCombatStoredStats { healing_points: 30, ..Default::default() });
    put_stats(0x7_1400, EeCombatStoredStats { max_hit_points: 170, ..Default::default() });
    let mut w = World { env: (0, 400), ..Default::default() };
    let goal = new_goal();
    put(goal, EeCombatHealGoal { target, elapsed: 700, ..Default::default() });
    work(&mut w, goal, healer);
    // 700 + 400 past a second: 100 left, the target healed to its maximum
    assert_eq!(heal_of(goal).elapsed, 100);
    assert_eq!(w.unit(target).hp, 170);
    let capture = w.log.last().unwrap();
    assert_eq!(
        (capture.0, capture.1, capture.2[0], capture.2[1]),
        (EE_COMBAT_G_CAPTURE_HEAL_WORK, healer, target as i64, 100)
    );
    // not yet a second: the clock only
    w.unit(target).hp = 0;
    work(&mut w, goal, healer);
    assert_eq!((heal_of(goal).elapsed, w.unit(target).hp), (500, 0));
    // in a cinematic: nothing, the capture still made
    w.env = (1, 400);
    work(&mut w, goal, healer);
    assert_eq!(heal_of(goal).elapsed, 500);
    assert_eq!(w.log.last().unwrap().0, EE_COMBAT_G_CAPTURE_HEAL_WORK);
    with_goals(|g| g.heal.remove(&goal));
}

fn run(op: i32, goal: u64, a: u64, b: u64, w: &mut World, point: &mut [f32; 3]) -> (i64, String) {
    let mut ints = [0i64; 8];
    let mut text = String::new();
    let r = body(op, GOAL_HEAL_UNIT, goal, a, b, &mut ints, point, w, &mut |t: &str| text.push_str(t));
    (r, text)
}

#[test]
fn the_heal_goal_through_the_dispatch() {
    let mut w = world();
    let goal = new_goal();
    let mut p = [0.0f32; 3];
    run(EE_COMBAT_GOAL_NEW_HEAL, goal, 0x100, 0, &mut w, &mut p);
    assert_eq!(heal_of(goal).target, 0x100);
    put(goal, EeCombatHealGoal { elapsed: 250, ..heal_of(goal) });
    // the inherited debug line (the location goal's, the auto-waypoint flag false) and the texts
    assert_eq!(run(OP_DEBUG_LINE, goal, 0, 0, &mut w, &mut p).1, "loc 3.50,-1.25,2.00: auto(0)");
    assert_eq!(
        run(OP_DUMP, goal, 0, 0, &mut w, &mut p).1,
        "OUT OF SYNCH DUMP:  EEUGHealUnit - Healee unit id is #02000041\n\
         OUT OF SYNCH DUMP:  EEUGHealUnit - Elasped Time in Milliseconds 250\n"
    );
    assert_eq!(
        run(OP_DEBUG_TEXT, goal, 0, 0, &mut w, &mut p).1,
        "Heal\nTarget Unit: Medic at (12,34)\nTarget Health: 50/200 (25.00%)\n"
    );
    assert_eq!(run(OP_CHECKSUM, goal, 0, 0, &mut w, &mut p).0, 0x0200_0041 + 250);
    assert_eq!(run(OP_START_ACTION, goal, 0, 0, &mut w, &mut p).0, 16002);
    assert_eq!(run(OP_MAX_PATH_ATTEMPTS, goal, 0, 0, &mut w, &mut p).0, 3);
    // Start does nothing
    let before = w.log.len();
    run(OP_START, goal, 0x300, 0, &mut w, &mut p);
    assert_eq!(w.log.len(), before);
    // the location and destination are the target's position
    run(OP_LOCATION, goal, 0, 0, &mut w, &mut p);
    assert_eq!(p, [3.5, -1.25, 2.0]);
    let mut q = [0.0f32; 3];
    run(OP_DESTINATION, goal, 0x300, 0, &mut w, &mut q);
    assert_eq!(q, p);
    // the step and the trigger test go to the C++ with the heal kind (2), the target and the goal
    run(OP_STEP, goal, 0x300, 0, &mut w, &mut p);
    let step = w.log.last().unwrap();
    assert_eq!((step.0, step.1, step.2[0], step.2[1], step.2[2]), (EE_COMBAT_G_STEP, 0x300, 2, 0x100, goal as i64));
    run(OP_EVALUATE, goal, 0x300, 9015, &mut w, &mut p);
    let eval = w.log.last().unwrap();
    assert_eq!(
        (eval.0, eval.2[0], eval.2[1], eval.2[2], eval.2[3]),
        (EE_COMBAT_G_EVALUATE, 2, 9015, 0x100, goal as i64)
    );
    // Suits: EECanHeal on the target, CBGO with the heal kind
    w.can_heal = true;
    assert_eq!(run(OP_SUITS, goal, 0x300, 0, &mut w, &mut p).0, 1);
    let capture = w.log.last().unwrap();
    assert_eq!((capture.0, capture.2[1], capture.2[2], capture.2[3]), (EE_COMBAT_G_CAPTURE, 15, 0x100, 1));
    // the clone: a new goal of the kind for the same target at its position now
    w.unit(0x100).location = [1.0, 2.0, 3.0];
    let copy = run(OP_CLONE, goal, 0, 1, &mut w, &mut p).0 as u64;
    assert!(w.log.iter().any(|o| o.0 == HOST_NEW_GOAL && o.1 == 15));
    assert_eq!(heal_of(copy), EeCombatHealGoal { location: bits([1.0, 2.0, 3.0]), target: 0x100, elapsed: 0 });
    assert_eq!(w.unit(0x100).refs, 2);
    // replace and save through the dispatch
    run(OP_REPLACE, copy, 0x100, 0x200, &mut w, &mut p);
    assert_eq!(heal_of(copy).target, 0x200);
    assert_eq!(run(OP_SAVE, copy, 0, 0, &mut w, &mut p).0, 0);
    assert_eq!(w.file[1], (EE_COMBAT_G_WRITE_U32, 0x0200_0042));
    // work goes to the body (no store records: the target's command is none, nothing healed)
    run(OP_WORK, copy, 0x300, 0, &mut w, &mut p);
    assert_eq!(w.log.last().unwrap().0, EE_COMBAT_G_CAPTURE_HEAL_WORK);
    put(copy, EeCombatHealGoal { target: 0x77, ..heal_of(copy) });
    assert_eq!(run(OP_REPAIR, copy, 0, 0, &mut w, &mut p).0, 3);
    // the destructor: the effect released, the record gone
    run(OP_DESTROY, goal, 0, 0, &mut w, &mut p);
    run(OP_DESTROY, copy, 0, 0, &mut w, &mut p);
    assert_eq!(w.unit(0x100).released, 1);
    assert_eq!(with_goals(|g| (g.heal.contains_key(&goal), g.heal.contains_key(&copy))), (false, false));
}

#[test]
fn a_debug_text_without_a_target_and_the_load_kind() {
    let mut w = World::default();
    let goal = new_goal();
    let mut ints = [2i64, 0, 0, 0, 0, 0, 0, 0];
    let mut p = [0.0f32; 3];
    body(EE_COMBAT_GOAL_NEW_HEAL, GOAL_HEAL_UNIT, goal, 0x100, 0, &mut ints, &mut p, &mut w, &mut |_| {});
    assert_eq!(heal_of(goal), EeCombatHealGoal::default());
    assert_eq!(run(OP_DEBUG_TEXT, goal, 0, 0, &mut w, &mut p).1, "Heal\n");
    assert_eq!(
        run(OP_DUMP, goal, 0, 0, &mut w, &mut p).1,
        "OUT OF SYNCH DUMP:  EEUGHealUnit - Healee unit id is #ffffffff\n\
         OUT OF SYNCH DUMP:  EEUGHealUnit - Elasped Time in Milliseconds 0\n"
    );
    assert_eq!(run(OP_DEBUG_LINE, goal, 0, 0, &mut w, &mut p).1, "loc 0.00,0.00,0.00: auto(0)");
    // the carrier landing's SetLocation
    ee_combat_heal_set_location(goal, 1.0f32.to_bits(), 0, 0);
    assert_eq!(run(OP_DEBUG_LINE, goal, 0, 0, &mut w, &mut p).1, "loc 1.00,0.00,0.00: auto(0)");
    let mut out = EeCombatHealGoal::default();
    // SAFETY: a valid record
    unsafe { ee_combat_heal_get(goal, &mut out) };
    assert_eq!(out.location[0], 1.0f32.to_bits());
    with_goals(|g| g.heal.remove(&goal));
}
