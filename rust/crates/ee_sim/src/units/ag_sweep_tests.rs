//! ag_sweep.rs: the sweep's host calls over a world with no player (the views need the game's objects; the sweep's
//! cases are the groups capture profile's golden).

use super::*;

struct Calls(Vec<(i32, u64, u64, i64, i64)>);

impl SweepHost for Calls {
    fn op(&mut self, o: i32, a: u64, b: u64, c: i64, d: i64) -> i64 {
        self.0.push((o, a, b, c, d));
        if o == op::EE_UNITS_SWEEP_OP_GOAL {
            7
        } else {
            0
        }
    }
}

#[test]
fn a_record_packs_its_case_count_and_answers() {
    let mut h = Calls(Vec::new());
    let units = [1u64, 2, 3];
    record_call(&mut h, 5, 9, 11, &units, 0b1011);
    let (o, a, b, c, d) = h.0[0];
    assert_eq!(o, op::EE_UNITS_SWEEP_OP_RECORD);
    assert_eq!(a, 5 | 3 << 32);
    assert_eq!((b, c), (9, 11));
    assert_eq!(d, units.as_ptr() as i64);
}

#[test]
fn a_move_carries_its_yaw_and_together_flag() {
    let mut h = Calls(Vec::new());
    let mut s = Sweep { h: &mut h, player: 4, case: 0 };
    assert_eq!(s.moving([1.0, 2.0, 3.0], true, 0.5), 7);
    let (o, a, _, _, d) = h.0[0];
    assert_eq!((o, a), (op::EE_UNITS_SWEEP_OP_GOAL, EE_UNITS_SWEEP_GOAL_MOVE));
    assert_eq!(d & 1, 1);
    assert_eq!((d >> 32) as u32, 0.5f32.to_bits());
}

#[test]
fn the_tag_is_agpl() {
    assert_eq!(TAG, (b'A' as u32) | (b'G' as u32) << 8 | (b'P' as u32) << 16 | (b'L' as u32) << 24);
}
