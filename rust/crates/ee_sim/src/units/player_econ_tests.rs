use super::*;
use std::sync::Mutex;

/// The capture thread is one per process: the tests that set it run one at a time.
static SERIAL: Mutex<()> = Mutex::new(());
use crate::units::player::{EeUnitsPlayerIo, EE_UNITS_PLAYER_NODE, EE_UNITS_PLAYER_NODE_COST};
use crate::units::player_save::EeUnitsPlayerSaved;
use crate::world::player_fields::EeUnitsPlayerFields;

/// A player whose every node costs 10 of each resource.
#[derive(Default)]
struct Host {
    calls: Vec<i32>,
}

impl PlayerHost for Host {
    fn call(&mut self, op: i32, io: &mut EeUnitsPlayerIo) -> i64 {
        self.calls.push(op);
        match op {
            EE_UNITS_PLAYER_NODE => io.i[0] + 1000,
            EE_UNITS_PLAYER_NODE_COST => 10,
            crate::units::player::EE_UNITS_PLAYER_SCALE_COST => io.i[0],
            _ => 0,
        }
    }
}

#[derive(Default)]
struct Econ {
    on: bool,
    records: Vec<Vec<i32>>,
    units: Vec<(u32, bool, bool)>,
}

impl EconHost for Econ {
    fn capturing(&mut self) -> bool {
        self.on
    }
    fn commit(&mut self, record: &[i32]) {
        self.records.push(record.to_vec());
    }
    fn node_id(&mut self, node: u64) -> i32 {
        if node == 0 {
            -1
        } else {
            node as i32 - 1000
        }
    }
    fn class_type(&mut self, class: u64) -> i64 {
        class as i64 * 2
    }
    fn unit_max_hit_points(&mut self, _: u64) -> c_ulong {
        200
    }
    fn unit_technology(&mut self, _: u64) -> c_long {
        7
    }
    fn unit_type(&mut self, unit: u64) -> i64 {
        unit as i64 + 1
    }
    fn text_length(&mut self, _: u64) -> i32 {
        12
    }
    fn player_record(&mut self) -> (i32, [i32; 5], i32) {
        (3, [1, 2, 3, 4, 5], 9)
    }
    fn units(&mut self, what: u32, a: bool, b: bool) -> c_long {
        self.units.push((what, a, b));
        42
    }
    fn apply_improvement(&mut self, _: c_long, _: c_long, _: bool) {}
}

fn with_player(f: impl FnOnce(&mut Player<Host>)) {
    let mut saved: EeUnitsPlayerSaved = unsafe { std::mem::zeroed() };
    saved.mResourceCaps = [0, 0, 100, 100, 100, 100, 100];
    let mut fields: EeUnitsPlayerFields = unsafe { std::mem::zeroed() };
    let mut h = Host::default();
    let mut p = Player { h: &mut h, saved: &mut saved, fields: &mut fields, stats: std::ptr::null() };
    f(&mut p);
}

#[test]
fn the_outermost_decision_on_the_ticks_thread_records_its_fields() {
    let _serial = SERIAL.lock().unwrap_or_else(|e| e.into_inner());
    note_capture_thread();
    with_player(|p| {
        let mut e = Econ { on: true, ..Default::default() };
        let x = EconArgs { object: 1005, a: 4, b: 1, ..Default::default() };
        assert_eq!(run(p, &mut e, EE_UNITS_ECON_CAN_AFFORD, &x), 1);
        assert_eq!(e.records, vec![vec![2, 3, 5, 4, 1, 1, -1, 1, 2, 3, 4, 5, 9]]);
        // the hero count asks player_units.rs
        let x = EconArgs { a: 1, b: 0, ..Default::default() };
        assert_eq!(run(p, &mut e, EE_UNITS_ECON_NUM_HEROES, &x), 42);
        assert_eq!(e.units, vec![(11, true, false)]);
        assert_eq!(e.records[1], vec![21, 3, 1, 0, 42, -1, 1, 2, 3, 4, 5, 9]);
    });
}

#[test]
fn a_nested_decision_and_an_idle_capture_record_nothing() {
    let _serial = SERIAL.lock().unwrap_or_else(|e| e.into_inner());
    note_capture_thread();
    with_player(|p| {
        let mut e = Econ { on: false, ..Default::default() };
        run(p, &mut e, EE_UNITS_ECON_TOTAL_POPULATION, &EconArgs::default());
        assert!(e.records.is_empty());
        e.on = true;
        DEPTH.with(|d| d.set(1));
        run(p, &mut e, EE_UNITS_ECON_TOTAL_POPULATION, &EconArgs::default());
        DEPTH.with(|d| d.set(0));
        assert!(e.records.is_empty());
    });
}

#[test]
fn another_thread_does_not_record() {
    let _serial = SERIAL.lock().unwrap_or_else(|e| e.into_inner());
    with_player(|p| {
        let mut e = Econ { on: true, ..Default::default() };
        std::thread::spawn(note_capture_thread).join().unwrap();
        run(p, &mut e, EE_UNITS_ECON_TOTAL_POPULATION, &EconArgs::default());
        assert!(e.records.is_empty());
    });
}

#[test]
fn repair_scales_by_the_hit_points_and_a_classless_payment_records_minus_one() {
    let _serial = SERIAL.lock().unwrap_or_else(|e| e.into_inner());
    note_capture_thread();
    with_player(|p| {
        let mut e = Econ { on: true, ..Default::default() };
        // 100 of 200 hit points at a cost of 10 each: affordable
        let x = EconArgs { object: 5, a: 8, b: 100, ..Default::default() };
        assert_eq!(run(p, &mut e, EE_UNITS_ECON_CAN_AFFORD_REPAIR, &x), 1);
        assert_eq!(&e.records[0][..6], &[18, 3, 6, 9, 100, 1]);
        assert!(p.h.calls.contains(&EE_UNITS_PLAYER_NODE));
        let calls = p.h.calls.len();
        let x = EconArgs { object: 0, a: 3, ..Default::default() };
        run(p, &mut e, EE_UNITS_ECON_PAY_IMPROVEMENT, &x);
        assert_eq!(&e.records[1][..5], &[7, 3, -1, 3, 0]);
        assert_eq!(p.h.calls.len(), calls, "no class: nothing paid");
    });
}

#[test]
fn the_reason_text_is_measured_for_the_three_text_decisions() {
    let _serial = SERIAL.lock().unwrap_or_else(|e| e.into_inner());
    note_capture_thread();
    with_player(|p| {
        let mut e = Econ { on: true, ..Default::default() };
        let x = EconArgs { a: 1, b: 2, text: 77, ..Default::default() };
        run(p, &mut e, EE_UNITS_ECON_CAN_IMPROVE, &x);
        let r = &e.records[0];
        assert_eq!(r[2..5], [1, 2, 1]);
        assert_eq!(r[6], 12);
    });
}
