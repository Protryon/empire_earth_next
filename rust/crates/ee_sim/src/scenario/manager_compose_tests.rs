use super::*;
use crate::scenario::manager::{EeScenarioManagerFields, EeScenarioTimedEntry, EeScenarioTimedQueue};

/// A round host recording its ops; `answer` gives the C++'s answers.
struct Mock {
    f: EeScenarioManagerFields,
    q: EeScenarioTimedQueue,
    ops: Vec<(u32, i64, i64)>,
    answer: fn(u32, i64) -> i64,
}

impl Round for Mock {
    fn op(&mut self, op: u32, a: i64, b: i64, _time: f32) -> i64 {
        self.ops.push((op, a, b));
        (self.answer)(op, a)
    }
    fn fields<R>(&mut self, f: impl FnOnce(&mut EeScenarioManagerFields) -> R) -> R {
        f(&mut self.f)
    }
    fn queue<R>(&mut self, f: impl FnOnce(&mut EeScenarioTimedQueue) -> R) -> R {
        f(&mut self.q)
    }
}

fn mock(answer: fn(u32, i64) -> i64) -> Mock {
    Mock { f: EeScenarioManagerFields::default(), q: EeScenarioTimedQueue::default(), ops: Vec::new(), answer }
}

fn ops(m: &Mock) -> Vec<u32> {
    m.ops.iter().map(|o| o.0).collect()
}

#[test]
fn the_destructor_unloads_then_deletes_in_order() {
    // no media to unload (count 0)
    let mut m = mock(|_, _| 0);
    destroy(&mut m);
    let got = ops(&m);
    let tail = &got[got.len() - 8..];
    assert_eq!(
        tail,
        [
            EE_SCENARIO_ROUND_DELETE_TRIGGERS,
            EE_SCENARIO_ROUND_DELETE_CONDITIONS,
            EE_SCENARIO_ROUND_DELETE_EFFECTS,
            EE_SCENARIO_ROUND_DELETE_OBJECTS,
            EE_SCENARIO_ROUND_DELETE_AREAS,
            EE_SCENARIO_ROUND_TIMED_FREE,
            EE_SCENARIO_ROUND_TIMED_FORGET,
            EE_SCENARIO_ROUND_DELETE_PENDING
        ]
    );
}

#[test]
fn a_pending_copy_unless_blocked() {
    let mut m = mock(|op, _| if op == EE_SCENARIO_ROUND_EFFECT_NEW { 700 } else { 0 });
    add_pending(&mut m, 55);
    assert_eq!(
        m.ops,
        vec![
            (EE_SCENARIO_ROUND_EFFECT_NEW, 0, 0),
            (EE_SCENARIO_ROUND_EFFECT_COPY, 700, 55),
            (EE_SCENARIO_ROUND_PENDING_PUSH, 700, 0)
        ]
    );
    let mut m = mock(|_, _| 0);
    m.f.mB234 = true;
    add_pending(&mut m, 55);
    assert!(m.ops.is_empty());
}

#[test]
fn the_timed_flag_by_the_effects_index() {
    // effects 10, 20, 30
    let at = |op: u32, a: i64| match op {
        EE_SCENARIO_ROUND_EFFECTS => 3,
        EE_SCENARIO_ROUND_EFFECT_AT => (a + 1) * 10,
        _ => 0,
    };
    let mut m = mock(at);
    m.q.add(1, 100, false);
    m.q.add(2, 200, false);
    set_timed_has_run(&mut m, 20, true);
    let runs: Vec<bool> = (0..2).map(|i| m.q.entries[i].mHasRun).collect();
    assert_eq!(runs, vec![true, false]);
    // not in the list, and null: nothing found
    let mut m = mock(at);
    m.q.add(1, 100, false);
    set_timed_has_run(&mut m, 99, true);
    set_timed_has_run(&mut m, 0, true);
    let e: EeScenarioTimedEntry = m.q.entries[0];
    assert!(!e.mHasRun);
}

static mut POINT: [f32; 3] = [0.0; 3];

#[test]
fn the_camera_moves_one_step_or_stops() {
    let at = |op: u32, _: i64| match op {
        EE_SCENARIO_ROUND_LINE_START => std::ptr::addr_of_mut!(POINT) as usize as i64,
        EE_SCENARIO_ROUND_CAMERA_ZOOM_FIELD => 4.5f32.to_bits() as i64,
        EE_SCENARIO_ROUND_SIDE_SCROLL_SPEED => 1.0f32.to_bits() as i64,
        _ => 0,
    };
    // SAFETY: only this test touches the point
    unsafe { POINT = [13.0, 24.0, 7.0] };
    let mut m = mock(at);
    m.f.mStartX = 10.0;
    m.f.mStartY = 20.0;
    m.f.mIsMovingCamera = true;
    move_camera_to_start(&mut m);
    assert_eq!(m.ops.last().unwrap().0, EE_SCENARIO_ROUND_CAMERA_SET_START);
    // SAFETY: as above
    assert_eq!(unsafe { POINT }, [13.0 - 3.0 * 5.0 / 7.0, 24.0 - 4.0 * 5.0 / 7.0, 7.0]);
    // at the start: no move
    unsafe { POINT = [10.0, 20.0, 7.0] };
    let mut m = mock(at);
    m.f.mStartX = 10.0;
    m.f.mStartY = 20.0;
    move_camera_to_start(&mut m);
    assert!(!ops(&m).contains(&EE_SCENARIO_ROUND_CAMERA_SET_START));
}

#[test]
fn the_capture_op_records_only_while_capturing() {
    let mut m = mock(|_, _| 0);
    assert_eq!(compose(&mut m, EE_SCENARIO_ROUND_CAPTURE, 1), Some(0));
    // no capture file in the tests
    assert!(m.ops.is_empty());
    assert_eq!(compose(&mut m, EE_SCENARIO_ROUND_CAPTURE + 1, 0), None);
}

/// A stream recording its ops: elements are handles 1000 + n; `fail` makes every Load fail.
struct File {
    ops: Vec<(u32, u32, c_ulong)>,
    count: c_ulong,
    fail_at: Option<c_ulong>,
    object_zero: bool,
    loading_scenario: bool,
}

impl File {
    fn io(&mut self, op: u32, slot: u32, index: c_ulong, value: *mut c_void) -> i32 {
        self.ops.push((op, slot, index));
        match op {
            EE_SCENARIO_IO_ELEMENT_NEW => {
                // SAFETY: the op's contract: value points to a void*
                unsafe { *(value as *mut *mut c_void) = 1000 as *mut c_void };
                0
            }
            EE_SCENARIO_IO_ELEMENT_READ => self.fail_at.map_or(0, |_| 9),
            EE_SCENARIO_IO_CONDITION_OBJECT_ZERO => self.object_zero as i32,
            EE_SCENARIO_IO_LOADING_SCENARIO => self.loading_scenario as i32,
            EE_SCENARIO_IO_READ_ULONG | EE_SCENARIO_IO_MESSAGES_COUNT => {
                // SAFETY: an unsigned long
                unsafe { *(value as *mut c_ulong) = self.count };
                0
            }
            EE_SCENARIO_IO_MESSAGE_LOAD | EE_SCENARIO_IO_MESSAGE_SAVE if self.fail_at == Some(index) => 9,
            _ => 0,
        }
    }
}

fn file() -> File {
    File { ops: Vec::new(), count: 0, fail_at: None, object_zero: false, loading_scenario: false }
}

fn kept(f: &File) -> bool {
    f.ops.iter().any(|o| o.0 == EE_SCENARIO_IO_ELEMENT_KEEP)
}

#[test]
fn elements_are_kept_or_freed_as_the_original() {
    let mut f = file();
    assert_eq!(element_load(&mut |a, b, c, d| f.io(a, b, c, d), EE_SCENARIO_ELEMENT_TRIGGERS), 0);
    assert!(kept(&f));
    // a failed load: freed, its result
    let mut f = file();
    f.fail_at = Some(0);
    assert_eq!(element_load(&mut |a, b, c, d| f.io(a, b, c, d), EE_SCENARIO_ELEMENT_EFFECTS), 9);
    assert!(!kept(&f));
    assert_eq!(f.ops.last().unwrap().0, EE_SCENARIO_IO_ELEMENT_FREE);
    // an "object 0" condition is dropped
    let mut f = file();
    f.object_zero = true;
    element_load(&mut |a, b, c, d| f.io(a, b, c, d), EE_SCENARIO_ELEMENT_CONDITIONS);
    assert!(!kept(&f));
    // a pending effect only while a saved game loads
    let mut f = file();
    f.loading_scenario = true;
    element_load(&mut |a, b, c, d| f.io(a, b, c, d), EE_SCENARIO_ELEMENT_PENDING);
    assert!(!kept(&f));
    let mut f = file();
    element_load(&mut |a, b, c, d| f.io(a, b, c, d), EE_SCENARIO_ELEMENT_PENDING);
    assert!(kept(&f));
    // the other lists never ask
    assert!(!f.ops.iter().any(|o| o.0 == EE_SCENARIO_IO_CONDITION_OBJECT_ZERO));
}

#[test]
fn messages_stop_at_the_first_error() {
    let mut f = file();
    f.count = 3;
    f.fail_at = Some(1);
    assert_eq!(messages(&mut |a, b, c, d| f.io(a, b, c, d), false), 9);
    let got: Vec<(u32, c_ulong)> = f.ops.iter().map(|o| (o.0, o.2)).collect();
    assert_eq!(
        got,
        vec![
            (EE_SCENARIO_IO_READ_ULONG, 0),
            (EE_SCENARIO_IO_MESSAGES_RESIZE, 0),
            (EE_SCENARIO_IO_MESSAGE_LOAD, 0),
            (EE_SCENARIO_IO_MESSAGE_LOAD, 1)
        ]
    );
    let mut f = file();
    f.count = 2;
    assert_eq!(messages(&mut |a, b, c, d| f.io(a, b, c, d), true), 0);
    let got: Vec<u32> = f.ops.iter().map(|o| o.0).collect();
    assert_eq!(
        got,
        vec![
            EE_SCENARIO_IO_MESSAGES_COUNT,
            EE_SCENARIO_IO_WRITE_ULONG,
            EE_SCENARIO_IO_MESSAGE_SAVE,
            EE_SCENARIO_IO_MESSAGE_SAVE
        ]
    );
}
