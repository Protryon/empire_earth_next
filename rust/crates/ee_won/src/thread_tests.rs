//! `thread`'s unit tests: the container over fake ops, the timer thread, a thread's start and stop.

use super::*;
use std::sync::Mutex;

/// A fake op: a reference count, a pending flag, a completion flag.
#[derive(Default)]
struct FakeOp {
    name: &'static str,
    refs: i32,
    pending: bool,
    completion_pending: bool,
    finish_in_run: bool,
}

struct World {
    ops: Vec<FakeOp>,
    log: Vec<String>,
    container: *const EeWonOpContainer,
}

// the tests share the process-wide statics below
static SERIAL: Mutex<()> = Mutex::new(());

fn serial() -> std::sync::MutexGuard<'static, ()> {
    SERIAL.lock().unwrap_or_else(std::sync::PoisonError::into_inner)
}

fn world(c: *mut c_void) -> &'static mut World {
    // SAFETY: the tests pass their World as the context
    unsafe { &mut *(c as *mut World) }
}

fn op(w: &mut World, h: *mut c_void) -> &mut FakeOp {
    &mut w.ops[h as usize - 1]
}

unsafe extern "C" fn add_ref(c: *mut c_void, h: *mut c_void) {
    op(world(c), h).refs += 1;
}
unsafe extern "C" fn release(c: *mut c_void, h: *mut c_void) {
    op(world(c), h).refs -= 1;
}
unsafe extern "C" fn is_pending(c: *mut c_void, h: *mut c_void) -> bool {
    op(world(c), h).pending
}
unsafe extern "C" fn completion_pending(c: *mut c_void, h: *mut c_void) -> bool {
    op(world(c), h).completion_pending
}
unsafe extern "C" fn start_run_async(c: *mut c_void, h: *mut c_void) {
    let w = world(c);
    let o = op(w, h);
    let name = o.name;
    let finished = o.finish_in_run;
    if finished {
        o.pending = false;
        o.completion_pending = true;
    }
    w.log.push(format!("{name} run"));
    if finished {
        // the op's Finish queues its completion in its container (re-entering under the pump)
        // SAFETY: the test's container
        unsafe { (*w.container).queue(h as OpHandle) };
    }
}
unsafe extern "C" fn complete(c: *mut c_void, h: *mut c_void) {
    let w = world(c);
    let o = op(w, h);
    o.completion_pending = false;
    let name = o.name;
    w.log.push(format!("{name} complete"));
}
unsafe extern "C" fn kill(c: *mut c_void, h: *mut c_void) {
    let w = world(c);
    let o = op(w, h);
    o.pending = false;
    o.completion_pending = true;
    let name = o.name;
    w.log.push(format!("{name} killed"));
}

fn hooks(w: &mut World) -> EeWonContainerHooks {
    EeWonContainerHooks {
        add_ref: Some(add_ref),
        release: Some(release),
        is_pending: Some(is_pending),
        completion_pending: Some(completion_pending),
        start_run_async: Some(start_run_async),
        complete: Some(complete),
        kill: Some(kill),
        context: w as *mut World as *mut c_void,
    }
}

fn new_world(names: &[&'static str]) -> Box<World> {
    Box::new(World {
        ops: names.iter().map(|&name| FakeOp { name, pending: true, ..FakeOp::default() }).collect(),
        log: Vec::new(),
        container: std::ptr::null(),
    })
}

#[test]
fn the_container_pumps_queued_ops_and_their_completions() {
    let _s = serial();
    let mut w = new_world(&["a", "b", "c"]);
    w.ops[0].finish_in_run = true;
    let c = EeWonOpContainer::new(hooks(&mut w));
    w.container = &c;
    c.queue(1);
    c.queue(2);
    assert_eq!((c.len(), w.ops[0].refs, w.ops[1].refs), (2, 1, 1));
    c.pump(0, 0);
    // a ran and finished: as the op being pumped its completion is not queued again but delivered at once
    assert_eq!(w.log, ["a run", "a complete", "b run"]);
    assert_eq!((w.ops[0].refs, w.ops[1].refs, c.len()), (0, 0, 0));
    // the op limit
    c.queue(3);
    c.queue(2);
    c.pump(0, 1);
    assert_eq!(c.len(), 1);
    c.pump(0, 0);
    assert_eq!(c.len(), 0);
    assert_eq!(c.current(), 0);
}

#[test]
fn the_purge_kills_pending_ops_and_completes_every_one() {
    let _s = serial();
    let mut w = new_world(&["a", "b"]);
    w.ops[1].pending = false;
    w.ops[1].completion_pending = true;
    let c = EeWonOpContainer::new(hooks(&mut w));
    w.container = &c;
    c.queue(1);
    c.queue(2);
    c.purge();
    assert_eq!(w.log, ["a killed", "a complete", "b complete"]);
    // the last stays the current op with its reference, as the C++ left mCurPumpOp
    assert_eq!((c.current(), w.ops[0].refs, w.ops[1].refs), (2, 0, 1));
    drop(c);
    assert_eq!(w.ops[1].refs, 0);
}

#[test]
fn the_op_being_pumped_is_queued_again_only_while_it_runs() {
    let _s = serial();
    let mut w = new_world(&["a"]);
    let c = EeWonOpContainer::new(hooks(&mut w));
    w.container = &c;
    c.queue(1);
    assert_eq!(c.state().queue.pop(), Some(1));
    c.set_current(1);
    // still pending: queued again
    c.queue(1);
    assert_eq!(c.len(), 1);
    w.ops[0].pending = false;
    c.queue(1);
    assert_eq!(c.len(), 1);
    c.purge();
    drop(c);
    assert_eq!(w.ops[0].refs, 0);
}

static TIMER_LOG: Mutex<Vec<(u8, usize)>> = Mutex::new(Vec::new());

unsafe extern "C" fn due(_: *mut c_void, h: *mut c_void) {
    TIMER_LOG.lock().unwrap().push((b'd', h as usize));
}
unsafe extern "C" fn purged(_: *mut c_void, h: *mut c_void) {
    TIMER_LOG.lock().unwrap().push((b'p', h as usize));
}

fn timer_log() -> Vec<(u8, usize)> {
    TIMER_LOG.lock().unwrap().clone()
}

#[test]
fn the_timer_thread_times_out_replaces_removes_and_purges() {
    let _s = serial();
    TIMER_LOG.lock().unwrap().clear();
    let t = EeWonTimerThread::new(EeWonTimerHooks { due: Some(due), purged: Some(purged), context: null_mut() });
    assert!(!t.add(1, 0));
    assert!(!t.add(2, 3_600_000));
    assert!(t.add(2, 3_600_000), "an op queued again replaces its entry");
    assert_eq!(t.len(), 2);
    std::thread::sleep(std::time::Duration::from_millis(5));
    t.pump();
    assert_eq!(timer_log(), [(b'd', 1)]);
    assert!(t.wait_time() <= 3_600_000);
    assert!(t.remove(2));
    assert!(!t.remove(2));
    assert!(!t.add(3, 3_600_000));
    t.purge();
    assert_eq!(timer_log(), [(b'd', 1), (b'p', 3)]);
    assert!(t.is_empty());
    // on its own thread
    // SAFETY: `t` is boxed and outlives the thread (stopped below)
    unsafe { t.thread().start() };
    t.add(4, 1);
    let end = std::time::Instant::now() + std::time::Duration::from_secs(5);
    while timer_log().len() < 3 && std::time::Instant::now() < end {
        std::thread::sleep(std::time::Duration::from_millis(1));
    }
    assert_eq!(timer_log()[2], (b'd', 4));
    t.stop(true);
    assert!(!t.thread().is_running());
    t.add(5, 3_600_000);
    drop(t);
    assert_eq!(timer_log()[3], (b'p', 5), "the destructor purges");
}

static RAN: AtomicU32 = AtomicU32::new(0);

unsafe extern "C" fn body_run(context: *mut c_void) {
    // SAFETY: the test passes its thread
    let t = unsafe { &*(context as *const EeWonThread) };
    if t.in_this_thread() {
        RAN.fetch_add(1, Ordering::SeqCst);
    }
    while !t.is_stopped() {
        t.wait_signal(1000);
    }
}

#[test]
fn a_thread_starts_once_and_stops_on_its_signal() {
    let _s = serial();
    RAN.store(0, Ordering::SeqCst);
    let mut t = Box::new(EeWonThread::new(EeWonThreadBody { run: None, destroy: None, context: null_mut() }));
    let me = &*t as *const EeWonThread as *mut c_void;
    t.body = EeWonThreadBody { run: Some(body_run), destroy: None, context: me };
    assert!(t.is_stopped() && !t.in_this_thread());
    // SAFETY: boxed, waited for below
    unsafe {
        t.start();
        t.start();
    }
    let end = std::time::Instant::now() + std::time::Duration::from_secs(5);
    while RAN.load(Ordering::SeqCst) == 0 && std::time::Instant::now() < end {
        std::thread::sleep(std::time::Duration::from_millis(1));
    }
    assert!(!t.in_this_thread());
    assert!(t.mark_stopped());
    assert!(!t.mark_stopped());
    t.signal();
    t.wait_for_stop();
    assert!(!t.is_running());
    assert_eq!(RAN.load(Ordering::SeqCst), 1, "one thread, which saw itself as the thread");
    assert_eq!(platform_priority(4), PLAT_PRIORITY_LOWEST);
    assert_eq!(platform_priority(9), PLAT_PRIORITY_NORMAL);
}
