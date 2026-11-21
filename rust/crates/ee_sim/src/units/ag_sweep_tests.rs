//! ag_sweep.rs: the sweep's host calls over a world with no player (the views need the game's objects; the sweep's
//! cases are the groups capture profile's golden).

use super::*;

struct Calls(Vec<(i32, u64, u64, i64, i64)>);

impl SweepHost for Calls {
    fn op(&mut self, o: i32, a: u64, b: u64, c: i64, d: i64) -> i64 {
        self.0.push((o, a, b, c, d));
        if o == op::EE_UNITS_SWEEP_OP_GOAL_MOVE {
            7
        } else {
            0
        }
    }
}

#[test]
fn a_record_draws_once_after_the_pending_groups() {
    let mut h = Calls(Vec::new());
    record_call(&mut h, 5, 0, 11, &[], 0b1011);
    let ops: Vec<i32> = h.0.iter().map(|c| c.0).collect();
    assert_eq!(ops, [op::EE_UNITS_SWEEP_OP_PENDING_GROUPS, op::EE_UNITS_SWEEP_OP_RANDOM]);
}

#[test]
fn a_record_goal_is_its_kind_id_and_location() {
    let mut h = Calls(Vec::new());
    let mut r = capture_record::Record::new(TAG);
    record_goal(&mut h, &mut r, 0);
    assert_eq!(r.data, (-1i32).to_le_bytes());
    let mut r = capture_record::Record::new(TAG);
    record_goal(&mut h, &mut r, 0x0102);
    assert_eq!(r.data.len(), 4 * 5);
    assert_eq!(h.0[0].0, op::EE_UNITS_SWEEP_OP_GOAL_LOCATION);
}

#[test]
fn a_move_carries_its_yaw_and_together_flag() {
    let mut h = Calls(Vec::new());
    let mut s = Sweep { h: &mut h, player: 4, case: 0 };
    assert_eq!(s.moving([1.0, 2.0, 3.0], true, 0.5), 7);
    assert_eq!(h.0[0].0, op::EE_UNITS_SWEEP_OP_GOAL_MOVE);
    // the group moves together: the goal's flag
    assert_eq!((h.0[1].0, h.0[1].1, h.0[1].2), (op::EE_UNITS_SWEEP_OP_GROUP_MOVEMENT, 7, 1));
}

#[test]
fn the_tag_is_agpl() {
    assert_eq!(TAG, (b'A' as u32) | (b'G' as u32) << 8 | (b'P' as u32) << 16 | (b'L' as u32) << 24);
}
