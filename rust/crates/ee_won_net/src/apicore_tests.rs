use super::*;
use std::sync::Mutex as TestMutex;

/// The calls the stand-in socket threads saw, in order ("<thread> <call>").
static LOG: TestMutex<Vec<String>> = TestMutex::new(Vec::new());
static NEXT: AtomicUsize = AtomicUsize::new(1);
/// The core tests share the one instance: one at a time.
static SERIAL: TestMutex<()> = TestMutex::new(());

fn log(s: String) {
    LOG.lock().unwrap().push(s);
}
fn take() -> Vec<String> {
    std::mem::take(&mut *LOG.lock().unwrap())
}
fn id(t: *mut c_void) -> usize {
    t as usize
}

unsafe extern "C" fn s_new() -> *mut c_void {
    let t = NEXT.fetch_add(1, Ordering::Relaxed) * 16;
    log(format!("new {t}"));
    t as *mut c_void
}
unsafe extern "C" fn s_delete(t: *mut c_void) {
    log(format!("{} delete", id(t)));
}
unsafe extern "C" fn s_single(t: *mut c_void, on: bool) {
    log(format!("{} single {on}", id(t)));
}
unsafe extern "C" fn s_need(t: *mut c_void) -> bool {
    log(format!("{} need", id(t)));
    true
}
unsafe extern "C" fn s_start(t: *mut c_void) {
    log(format!("{} start", id(t)));
}
unsafe extern "C" fn s_stop(t: *mut c_void, wait: bool) {
    log(format!("{} stop {wait}", id(t)));
}
unsafe extern "C" fn s_wait(t: *mut c_void) {
    log(format!("{} wait", id(t)));
}
unsafe extern "C" fn s_purge(t: *mut c_void) {
    log(format!("{} purge", id(t)));
}
unsafe extern "C" fn s_pump(t: *mut c_void, wait: u32) {
    log(format!("{} pump {}", id(t), wait.min(1)));
}
unsafe extern "C" fn s_add(t: *mut c_void, op: *mut c_void) {
    log(format!("{} add {}", id(t), op as usize));
}
unsafe extern "C" fn s_remove(t: *mut c_void, op: *mut c_void) {
    log(format!("{} remove {}", id(t), op as usize));
}

static HOOKS: EeWonApiHooks = EeWonApiHooks {
    socket_new: Some(s_new),
    socket_delete: Some(s_delete),
    socket_set_single: Some(s_single),
    socket_need_thread: Some(s_need),
    socket_start: Some(s_start),
    socket_stop: Some(s_stop),
    socket_wait_for_stop: Some(s_wait),
    socket_purge: Some(s_purge),
    socket_pump: Some(s_pump),
    socket_add: Some(s_add),
    socket_remove: Some(s_remove),
};

fn core() -> *mut EeWonApiCore {
    // SAFETY: a static table; no C++ core (no op is made)
    unsafe { ee_won_api_new(0x1000 as *mut c_void, std::ptr::null(), &HOOKS) }
}

#[test]
fn startup_makes_the_socket_thread_once_and_shutdown_stops_it() {
    let _s = SERIAL.lock().unwrap();
    take();
    let c = core();
    assert_eq!(ee_won_api_instance() as usize, 0x1000);
    assert!(!ee_won_api_is_started());
    // SAFETY (the calls): the live core
    unsafe {
        ee_won_api_set(c, 1, false);
        ee_won_api_startup(c);
        ee_won_api_startup(c);
    }
    assert!(ee_won_api_is_started());
    let l = take();
    let t = l[0].strip_prefix("new ").unwrap().to_string();
    assert_eq!(l, vec![format!("new {t}"), format!("{t} single true")]);
    unsafe {
        ee_won_api_pump(c, 0, 0);
        ee_won_api_shutdown(c);
        ee_won_api_shutdown(c);
    }
    assert!(!ee_won_api_is_started());
    assert_eq!(
        take(),
        vec![format!("{t} pump 0"), format!("{t} stop false"), format!("{t} wait"), format!("{t} purge")]
    );
    unsafe { ee_won_api_free(c) };
    assert_eq!(take(), vec![format!("{t} delete")]);
    assert_eq!(ee_won_api_instance() as usize, 0);
}

#[test]
fn with_threads_the_socket_thread_starts_when_it_needs_one() {
    let _s = SERIAL.lock().unwrap();
    take();
    let c = core();
    // SAFETY (the calls): the live core
    unsafe {
        ee_won_api_startup(c);
        // after start-up the settings do not change
        ee_won_api_set(c, 1, false);
        ee_won_api_shutdown(c);
        ee_won_api_free(c);
    }
    let l = take();
    let t = l[0].strip_prefix("new ").unwrap().to_string();
    let want: Vec<String> = ["single false", "need", "start", "stop false", "wait", "purge", "delete"]
        .iter()
        .map(|x| format!("{t} {x}"))
        .collect();
    assert_eq!(&l[1..], &want[..]);
}

#[test]
fn a_socket_thread_set_before_start_up_replaces_the_old_one_and_after_is_deleted() {
    let _s = SERIAL.lock().unwrap();
    take();
    let c = core();
    // SAFETY (the calls): the live core; the stand-in threads are numbers
    unsafe {
        ee_won_api_set(c, 1, false);
        ee_won_api_set_socket_thread(c, 0x500 as *mut c_void);
        ee_won_api_set_socket_thread(c, 0x600 as *mut c_void);
        ee_won_api_startup(c);
        ee_won_api_set_socket_thread(c, 0x700 as *mut c_void);
        ee_won_api_socket_op(0x99 as *mut c_void, true);
        ee_won_api_socket_op(0x99 as *mut c_void, false);
        ee_won_api_free(c);
    }
    assert_eq!(
        take(),
        vec![
            "1280 delete",
            "1536 single true",
            "1792 delete",
            "1536 add 153",
            "1536 remove 153",
            "1536 stop false",
            "1536 wait",
            "1536 purge",
            "1536 delete"
        ]
    );
}

#[test]
fn the_static_calls_do_nothing_without_a_started_core() {
    let _s = SERIAL.lock().unwrap();
    take();
    // no core: nothing is called (a NULL op is never killed)
    // SAFETY: NULL ops are not touched without a core
    unsafe {
        ee_won_api_socket_op(std::ptr::null_mut(), false);
        ee_won_api_remove_timer_op(std::ptr::null_mut());
        ee_won_api_queue_op(std::ptr::null_mut());
    }
    assert!(take().is_empty());
    assert!(!ee_won_api_is_started());
}
