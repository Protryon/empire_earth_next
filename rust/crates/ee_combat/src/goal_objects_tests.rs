use super::*;

/// A mock world: a reachable flag, the unit's location, a file of float bits; every operation logged.
#[derive(Default)]
struct Mock {
    reach: bool,
    location: [u32; 3],
    file: Vec<u32>,
    read_at: usize,
    fail_at: Option<usize>,
    log: Vec<(i32, u64, [i64; 3])>,
}

impl GoalHost for Mock {
    fn op(&mut self, code: i32, unit: u64, args: &mut EeCombatGoalArgs) -> i64 {
        self.log.push((code, unit, [args[0], args[1], args[2]]));
        match code {
            EE_COMBAT_G_CAN_REACH_TILE => i64::from(self.reach),
            EE_COMBAT_G_UNIT_LOCATION => {
                for (slot, b) in args.iter_mut().zip(self.location) {
                    *slot = i64::from(b);
                }
                0
            }
            EE_COMBAT_G_TELL_GROUP => 1,
            EE_COMBAT_G_WRITE_F32 => {
                if self.fail_at == Some(self.file.len()) {
                    return 3;
                }
                self.file.push(args[0] as u32);
                0
            }
            EE_COMBAT_G_READ_F32 => {
                if self.fail_at == Some(self.read_at) {
                    return 3;
                }
                args[0] = i64::from(self.file[self.read_at]);
                self.read_at += 1;
                0
            }
            _ => 0,
        }
    }
}

const GOAL: u64 = 0x5000;
const UNIT: u64 = 0x600;

#[test]
fn the_destination_is_the_unit_when_the_tile_is_reachable_else_the_target() {
    ground_create(GOAL, [10.75, -3.25, 1.5]);
    let mut h =
        Mock { reach: true, location: [1.0f32.to_bits(), 2.0f32.to_bits(), 0.5f32.to_bits()], ..Default::default() };
    let out = ground_destination(&mut h, GOAL, UNIT);
    assert_eq!(out, h.location);
    // UMFloor of the target's x and y: 10 and -4
    assert_eq!(h.log[0], (EE_COMBAT_G_CAN_REACH_TILE, UNIT, [10, -4, 0]));
    assert_eq!(h.log[1].0, EE_COMBAT_G_UNIT_LOCATION);
    assert_eq!(
        h.log[2],
        (EE_COMBAT_G_TELL_GROUP, UNIT, [1.0f32.to_bits() as i64, 2.0f32.to_bits() as i64, 0.5f32.to_bits() as i64])
    );
    let mut h = Mock::default();
    let out = ground_destination(&mut h, GOAL, UNIT);
    assert_eq!(out, bits([10.75, -3.25, 1.5]));
    assert_eq!(h.log.iter().map(|l| l.0).collect::<Vec<_>>(), vec![EE_COMBAT_G_CAN_REACH_TILE, EE_COMBAT_G_TELL_GROUP]);
    goal_drop(GOAL);
}

#[test]
fn the_checksum_sums_the_raw_bits() {
    let goal = GOAL + 1;
    ground_create(goal, [-1.0, 2.0, 3.0]);
    let want = ((-1.0f32).to_bits() as i32).wrapping_add(2.0f32.to_bits() as i32).wrapping_add(3.0f32.to_bits() as i32);
    assert_eq!(ground_checksum(goal), want);
    goal_drop(goal);
    assert_eq!(ground_checksum(goal), 0, "no record: 0, 0, 0");
}

#[test]
fn save_and_load_round_trip_and_stop_at_the_first_error() {
    let goal = GOAL + 2;
    ground_create(goal, [4.0, 5.0, 6.0]);
    let mut h = Mock::default();
    assert_eq!(ground_save(&mut h, goal), 0);
    assert_eq!(h.file, bits([4.0, 5.0, 6.0]).to_vec());
    let other = GOAL + 3;
    ground_create(other, [0.0, 0.0, 0.0]);
    assert_eq!(ground_load(&mut h, other), 0);
    let mut t = [0.0f32; 3];
    unsafe { ee_combat_ground_target(other, &mut t) };
    assert_eq!(t, [4.0, 5.0, 6.0]);
    // a failed second write: one coordinate written, the error answered
    let mut h = Mock { fail_at: Some(1), ..Default::default() };
    assert_eq!(ground_save(&mut h, goal), 3);
    assert_eq!(h.file.len(), 1);
    // a failed second read: the first coordinate read is kept, the rest as before
    let mut h = Mock { file: bits([7.0, 8.0, 9.0]).to_vec(), fail_at: Some(1), ..Default::default() };
    assert_eq!(ground_load(&mut h, other), 3);
    unsafe { ee_combat_ground_target(other, &mut t) };
    assert_eq!(t, [7.0, 5.0, 6.0]);
    goal_drop(goal);
    goal_drop(other);
}
