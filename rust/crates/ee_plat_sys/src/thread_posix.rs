//! PlatThread on POSIX.
//!
//! Events: every event shares one process-wide mutex and condition variable. A wait checks its events under the
//! mutex and sleeps on the condition; PlatEventSet broadcasts it. That makes PlatEventWaitAny exact (the lowest set
//! event wins, an automatic-reset event is cleared by the one wait it releases) at the cost of waking every waiter
//! on every set, which is cheap for the game's dozen threads. Timed waits run on the monotonic clock.
//!
//! Threads: a PlatThread is a reference-counted record (the thread holds one reference, each handle one), so a handle
//! stays valid after the thread ends and PlatThreadClose never touches a running thread. A thread created suspended
//! waits at a start gate until PlatThreadResume; suspending a running thread is not possible with pthreads, so
//! PlatThreadSuspend fails there (ERROR_NOT_SUPPORTED), and so does PlatThreadTerminate. PlatThreadWait waits on a
//! done event the thread sets as it ends. Priorities are kept and applied as a nice value where the system allows it.

use crate::constants::*;
use crate::cstr::try_box;
use crate::last_error::{
    self, set as set_last_error, ERROR_INVALID_HANDLE, ERROR_INVALID_PARAMETER, ERROR_NOT_ENOUGH_MEMORY,
    ERROR_NOT_SUPPORTED,
};
use std::cell::{Cell, UnsafeCell};
use std::ffi::c_void;
use std::ptr::{addr_of_mut, null, null_mut};
use std::sync::Once;

pub struct PlatMutex {
    mutex: libc::pthread_mutex_t,
}

pub type PlatThreadMain = unsafe extern "C" fn(parameter: *mut c_void) -> u32;

#[no_mangle]
pub unsafe extern "C" fn PlatGetLastError() -> u32 {
    last_error::get()
}

#[no_mangle]
pub unsafe extern "C" fn PlatCurrentThreadId() -> u32 {
    #[cfg(target_os = "linux")]
    {
        libc::syscall(libc::SYS_gettid) as u32
    }
    #[cfg(target_vendor = "apple")]
    {
        let mut id = 0u64;
        libc::pthread_threadid_np(libc::pthread_self(), &mut id);
        id as u32
    }
    #[cfg(not(any(target_os = "linux", target_vendor = "apple")))]
    {
        libc::pthread_self() as usize as u32
    }
}

#[no_mangle]
pub unsafe extern "C" fn PlatMutexCreate() -> *mut PlatMutex {
    let mutex = try_box(std::mem::zeroed::<PlatMutex>());
    if mutex.is_null() {
        set_last_error(ERROR_NOT_ENOUGH_MEMORY);
        return null_mut();
    }
    let mut attributes: libc::pthread_mutexattr_t = std::mem::zeroed();
    libc::pthread_mutexattr_init(&mut attributes);
    libc::pthread_mutexattr_settype(&mut attributes, libc::PTHREAD_MUTEX_RECURSIVE);
    let error = libc::pthread_mutex_init(addr_of_mut!((*mutex).mutex), &attributes);
    libc::pthread_mutexattr_destroy(&mut attributes);
    if error != 0 {
        drop(Box::from_raw(mutex));
        set_last_error(last_error::from_errno(error));
        return null_mut();
    }
    mutex
}

#[no_mangle]
pub unsafe extern "C" fn PlatMutexDestroy(mutex: *mut PlatMutex) {
    if mutex.is_null() {
        return;
    }
    libc::pthread_mutex_destroy(addr_of_mut!((*mutex).mutex));
    drop(Box::from_raw(mutex));
}

#[no_mangle]
pub unsafe extern "C" fn PlatMutexLock(mutex: *mut PlatMutex) {
    libc::pthread_mutex_lock(addr_of_mut!((*mutex).mutex));
}

#[no_mangle]
pub unsafe extern "C" fn PlatMutexUnlock(mutex: *mut PlatMutex) {
    libc::pthread_mutex_unlock(addr_of_mut!((*mutex).mutex));
}

#[no_mangle]
pub unsafe extern "C" fn PlatMutexTryLock(mutex: *mut PlatMutex) -> i32 {
    (libc::pthread_mutex_trylock(addr_of_mut!((*mutex).mutex)) == 0) as i32
}

// ---- events

pub struct PlatEvent {
    manual_reset: bool,
    set: bool,
}

struct EventSync {
    lock: UnsafeCell<libc::pthread_mutex_t>,
    condition: UnsafeCell<libc::pthread_cond_t>,
}
// SAFETY: only used through pthread calls.
unsafe impl Sync for EventSync {}

static EVENTS: EventSync = EventSync {
    lock: UnsafeCell::new(libc::PTHREAD_MUTEX_INITIALIZER),
    condition: UnsafeCell::new(libc::PTHREAD_COND_INITIALIZER),
};
static EVENT_ONCE: Once = Once::new();

fn init_event_condition() {
    EVENT_ONCE.call_once(|| unsafe {
        let mut attributes: libc::pthread_condattr_t = std::mem::zeroed();
        libc::pthread_condattr_init(&mut attributes);
        #[cfg(not(target_vendor = "apple"))]
        libc::pthread_condattr_setclock(&mut attributes, libc::CLOCK_MONOTONIC);
        libc::pthread_cond_init(EVENTS.condition.get(), &attributes);
        libc::pthread_condattr_destroy(&mut attributes);
    });
}

unsafe fn monotonic_now() -> libc::timespec {
    let mut now: libc::timespec = std::mem::zeroed();
    libc::clock_gettime(libc::CLOCK_MONOTONIC, &mut now);
    now
}

fn add_milliseconds(mut time: libc::timespec, ms: u32) -> libc::timespec {
    time.tv_sec += (ms / 1000) as libc::time_t;
    time.tv_nsec += ((ms % 1000) as libc::c_long) * 1_000_000;
    if time.tv_nsec >= 1_000_000_000 {
        time.tv_sec += 1;
        time.tv_nsec -= 1_000_000_000;
    }
    time
}

/// Sleeps on the event condition (the event lock held) until `deadline` -> false once the deadline has passed.
unsafe fn wait_until(deadline: &libc::timespec) -> bool {
    #[cfg(target_vendor = "apple")]
    {
        // macOS has no monotonic condition clock: wait relative to what is left
        let now = monotonic_now();
        if now.tv_sec > deadline.tv_sec || (now.tv_sec == deadline.tv_sec && now.tv_nsec >= deadline.tv_nsec) {
            return false;
        }
        let mut left = libc::timespec { tv_sec: deadline.tv_sec - now.tv_sec, tv_nsec: deadline.tv_nsec - now.tv_nsec };
        if left.tv_nsec < 0 {
            left.tv_sec -= 1;
            left.tv_nsec += 1_000_000_000;
        }
        libc::pthread_cond_timedwait_relative_np(EVENTS.condition.get(), EVENTS.lock.get(), &left);
        true
    }
    #[cfg(not(target_vendor = "apple"))]
    {
        libc::pthread_cond_timedwait(EVENTS.condition.get(), EVENTS.lock.get(), deadline) != libc::ETIMEDOUT
    }
}

#[no_mangle]
pub unsafe extern "C" fn PlatEventCreate(manual_reset: i32, initially_set: i32) -> *mut PlatEvent {
    init_event_condition();
    let event = try_box(PlatEvent { manual_reset: manual_reset != 0, set: initially_set != 0 });
    if event.is_null() {
        set_last_error(ERROR_NOT_ENOUGH_MEMORY);
    }
    event
}

#[no_mangle]
pub unsafe extern "C" fn PlatEventDestroy(event: *mut PlatEvent) {
    if !event.is_null() {
        drop(Box::from_raw(event));
    }
}

#[no_mangle]
pub unsafe extern "C" fn PlatEventSet(event: *mut PlatEvent) -> i32 {
    if event.is_null() {
        return last_error::fail(ERROR_INVALID_HANDLE);
    }
    libc::pthread_mutex_lock(EVENTS.lock.get());
    (*event).set = true;
    libc::pthread_cond_broadcast(EVENTS.condition.get());
    libc::pthread_mutex_unlock(EVENTS.lock.get());
    1
}

#[no_mangle]
pub unsafe extern "C" fn PlatEventReset(event: *mut PlatEvent) -> i32 {
    if event.is_null() {
        return last_error::fail(ERROR_INVALID_HANDLE);
    }
    libc::pthread_mutex_lock(EVENTS.lock.get());
    (*event).set = false;
    libc::pthread_mutex_unlock(EVENTS.lock.get());
    1
}

/// Finds the lowest set event (clearing an automatic-reset one) -> its index, -1 when none is set. The event lock is
/// held.
unsafe fn take_first_set(events: *const *mut PlatEvent, count: u32) -> i32 {
    for i in 0..count as usize {
        let event = *events.add(i);
        if (*event).set {
            if !(*event).manual_reset {
                (*event).set = false;
            }
            return i as i32;
        }
    }
    -1
}

#[no_mangle]
pub unsafe extern "C" fn PlatEventWaitAny(events: *const *mut PlatEvent, count: u32, timeout_ms: u32) -> i32 {
    if count == 0 || count > PLAT_WAIT_MAX || events.is_null() {
        set_last_error(ERROR_INVALID_PARAMETER);
        return PLAT_WAIT_FAILED;
    }
    for i in 0..count as usize {
        if (*events.add(i)).is_null() {
            set_last_error(ERROR_INVALID_HANDLE);
            return PLAT_WAIT_FAILED;
        }
    }
    init_event_condition();
    let deadline = add_milliseconds(monotonic_now(), if timeout_ms == PLAT_WAIT_INFINITE { 0 } else { timeout_ms });
    libc::pthread_mutex_lock(EVENTS.lock.get());
    let mut result = PLAT_WAIT_TIMEOUT;
    loop {
        let index = take_first_set(events, count);
        if index >= 0 {
            result = index;
            break;
        }
        if timeout_ms == 0 {
            break;
        }
        if timeout_ms == PLAT_WAIT_INFINITE {
            libc::pthread_cond_wait(EVENTS.condition.get(), EVENTS.lock.get());
        } else if !wait_until(&deadline) {
            // one last look: a set that raced with the deadline still counts
            let index = take_first_set(events, count);
            if index >= 0 {
                result = index;
            }
            break;
        }
    }
    libc::pthread_mutex_unlock(EVENTS.lock.get());
    result
}

#[no_mangle]
pub unsafe extern "C" fn PlatEventWait(event: *mut PlatEvent, timeout_ms: u32) -> i32 {
    let events = [event];
    PlatEventWaitAny(events.as_ptr(), 1, timeout_ms)
}

// ---- threads

pub struct PlatThread {
    lock: libc::pthread_mutex_t,
    start_condition: libc::pthread_cond_t,
    references: i32,
    start_gate: u32, // suspend count before the thread's first instruction
    running: bool,
    exit_code: u32,
    priority: i32,
    thread_id: u32,
    main: Option<PlatThreadMain>,
    parameter: *mut c_void,
    done: *mut PlatEvent, // manual reset, set when inMain has returned (PlatThreadWait)
}

thread_local! {
    static CURRENT_THREAD: Cell<*mut PlatThread> = const { Cell::new(null_mut()) };
}

unsafe fn new_thread_record() -> *mut PlatThread {
    let done = PlatEventCreate(1, 0);
    if done.is_null() {
        return null_mut();
    }
    let thread = try_box(PlatThread {
        lock: libc::PTHREAD_MUTEX_INITIALIZER,
        start_condition: libc::PTHREAD_COND_INITIALIZER,
        references: 1,
        start_gate: 0,
        running: true,
        exit_code: PLAT_THREAD_STILL_ACTIVE,
        priority: PLAT_PRIORITY_NORMAL,
        thread_id: 0,
        main: None,
        parameter: null_mut(),
        done,
    });
    if thread.is_null() {
        PlatEventDestroy(done);
        return null_mut();
    }
    libc::pthread_mutex_init(addr_of_mut!((*thread).lock), null());
    libc::pthread_cond_init(addr_of_mut!((*thread).start_condition), null());
    thread
}

unsafe fn release(thread: *mut PlatThread) {
    let lock = addr_of_mut!((*thread).lock);
    libc::pthread_mutex_lock(lock);
    (*thread).references -= 1;
    let left = (*thread).references;
    libc::pthread_mutex_unlock(lock);
    if left == 0 {
        PlatEventDestroy((*thread).done);
        libc::pthread_cond_destroy(addr_of_mut!((*thread).start_condition));
        libc::pthread_mutex_destroy(lock);
        drop(Box::from_raw(thread));
    }
}

unsafe fn retain(thread: *mut PlatThread) {
    let lock = addr_of_mut!((*thread).lock);
    libc::pthread_mutex_lock(lock);
    (*thread).references += 1;
    libc::pthread_mutex_unlock(lock);
}

/// The nice value for a PLAT_PRIORITY_* value (Linux).
#[cfg(target_os = "linux")]
pub fn nice_of(priority: i32) -> i32 {
    if priority <= PLAT_PRIORITY_IDLE {
        19
    } else if priority >= PLAT_PRIORITY_TIME_CRITICAL {
        -10
    } else {
        -priority * 2
    }
}

/// Applies a PLAT_PRIORITY_* value to the calling thread, as far as the system lets an unprivileged process.
unsafe fn apply_priority(priority: i32) {
    #[cfg(target_vendor = "apple")]
    {
        use libc::qos_class_t::*;
        let class = if priority >= PLAT_PRIORITY_HIGHEST {
            QOS_CLASS_USER_INTERACTIVE
        } else if priority >= PLAT_PRIORITY_NORMAL {
            QOS_CLASS_USER_INITIATED
        } else if priority >= PLAT_PRIORITY_LOWEST {
            QOS_CLASS_UTILITY
        } else {
            QOS_CLASS_BACKGROUND
        };
        libc::pthread_set_qos_class_self_np(class, 0);
    }
    #[cfg(target_os = "linux")]
    {
        // a per-thread nice value: lowering always works, raising above the start value needs privileges (ignored)
        libc::setpriority(libc::PRIO_PROCESS, libc::syscall(libc::SYS_gettid) as libc::id_t, nice_of(priority));
    }
    #[cfg(not(any(target_vendor = "apple", target_os = "linux")))]
    let _ = priority;
}

extern "C" fn thread_entry(argument: *mut c_void) -> *mut c_void {
    unsafe {
        let thread = argument.cast::<PlatThread>();
        CURRENT_THREAD.with(|c| c.set(thread));
        let lock = addr_of_mut!((*thread).lock);
        let condition = addr_of_mut!((*thread).start_condition);
        libc::pthread_mutex_lock(lock);
        (*thread).thread_id = PlatCurrentThreadId();
        libc::pthread_cond_broadcast(condition);
        // start_gate changes under the lock, through the condition wait's opaque call
        #[allow(clippy::while_immutable_condition)]
        while (*thread).start_gate > 0 {
            libc::pthread_cond_wait(condition, lock);
        }
        let priority = (*thread).priority;
        let main = (*thread).main.expect("a created thread has its main");
        let parameter = (*thread).parameter;
        libc::pthread_mutex_unlock(lock);
        if priority != PLAT_PRIORITY_NORMAL {
            apply_priority(priority);
        }
        let exit_code = main(parameter);
        libc::pthread_mutex_lock(lock);
        (*thread).exit_code = exit_code;
        (*thread).running = false;
        libc::pthread_mutex_unlock(lock);
        PlatEventSet((*thread).done);
        CURRENT_THREAD.with(|c| c.set(null_mut()));
        release(thread);
    }
    null_mut()
}

#[no_mangle]
pub unsafe extern "C" fn PlatThreadCreate(
    main: PlatThreadMain,
    parameter: *mut c_void,
    stack_size: u32,
    suspended: i32,
    thread_id: *mut u32,
) -> *mut PlatThread {
    let thread = new_thread_record();
    if thread.is_null() {
        set_last_error(ERROR_NOT_ENOUGH_MEMORY);
        return null_mut();
    }
    (*thread).references = 2; // the thread's and the caller's handle
    (*thread).start_gate = u32::from(suspended != 0);
    (*thread).main = Some(main);
    (*thread).parameter = parameter;
    let mut attributes: libc::pthread_attr_t = std::mem::zeroed();
    libc::pthread_attr_init(&mut attributes);
    libc::pthread_attr_setdetachstate(&mut attributes, libc::PTHREAD_CREATE_DETACHED);
    if stack_size != 0 {
        // Win32 rounds a stack size up to its granularity; pthreads refuses one below PTHREAD_STACK_MIN
        let size = (stack_size.max(65536) as usize + 65535) & !65535usize;
        libc::pthread_attr_setstacksize(&mut attributes, size);
    }
    let lock = addr_of_mut!((*thread).lock);
    libc::pthread_mutex_lock(lock);
    let mut handle: libc::pthread_t = std::mem::zeroed();
    let error = libc::pthread_create(&mut handle, &attributes, thread_entry, thread.cast());
    libc::pthread_attr_destroy(&mut attributes);
    if error != 0 {
        libc::pthread_mutex_unlock(lock);
        (*thread).references = 1;
        release(thread);
        set_last_error(last_error::from_errno(error));
        return null_mut();
    }
    // the id is the system's thread id, known once the thread runs: wait for it (the start gate holds it after)
    #[allow(clippy::while_immutable_condition)]
    while (*thread).thread_id == 0 {
        libc::pthread_cond_wait(addr_of_mut!((*thread).start_condition), lock);
    }
    *thread_id = (*thread).thread_id;
    libc::pthread_mutex_unlock(lock);
    thread
}

#[no_mangle]
pub unsafe extern "C" fn PlatThreadOpenCurrent() -> *mut PlatThread {
    let mut current = CURRENT_THREAD.with(Cell::get);
    if current.is_null() {
        // a thread not started here (the main thread): a record that lives as long as the thread
        let thread = new_thread_record();
        if thread.is_null() {
            set_last_error(ERROR_NOT_ENOUGH_MEMORY);
            return null_mut();
        }
        (*thread).thread_id = PlatCurrentThreadId();
        CURRENT_THREAD.with(|c| c.set(thread)); // the thread's own reference (never released: such threads do not end here)
        current = thread;
    }
    retain(current);
    current
}

#[no_mangle]
pub unsafe extern "C" fn PlatThreadClose(thread: *mut PlatThread) {
    if !thread.is_null() {
        release(thread);
    }
}

#[no_mangle]
pub unsafe extern "C" fn PlatThreadGetExitCode(thread: *mut PlatThread, exit_code: *mut u32) -> i32 {
    if thread.is_null() {
        return last_error::fail(ERROR_INVALID_HANDLE);
    }
    let lock = addr_of_mut!((*thread).lock);
    libc::pthread_mutex_lock(lock);
    *exit_code = if (*thread).running { PLAT_THREAD_STILL_ACTIVE } else { (*thread).exit_code };
    libc::pthread_mutex_unlock(lock);
    1
}

#[no_mangle]
pub unsafe extern "C" fn PlatThreadWait(thread: *mut PlatThread, timeout_ms: u32) -> i32 {
    if thread.is_null() {
        set_last_error(ERROR_INVALID_HANDLE);
        return PLAT_WAIT_FAILED;
    }
    // a thread not started here (PlatThreadOpenCurrent) never sets it: as Win32, a thread waiting on itself waits
    PlatEventWait((*thread).done, timeout_ms)
}

#[no_mangle]
pub unsafe extern "C" fn PlatThreadGetPriority(thread: *mut PlatThread) -> i32 {
    if thread.is_null() {
        set_last_error(ERROR_INVALID_HANDLE);
        return PLAT_PRIORITY_ERROR;
    }
    let lock = addr_of_mut!((*thread).lock);
    libc::pthread_mutex_lock(lock);
    let priority = (*thread).priority;
    libc::pthread_mutex_unlock(lock);
    priority
}

#[no_mangle]
pub unsafe extern "C" fn PlatThreadSetPriority(thread: *mut PlatThread, priority: i32) -> i32 {
    if thread.is_null() {
        return last_error::fail(ERROR_INVALID_HANDLE);
    }
    if !(PLAT_PRIORITY_IDLE..=PLAT_PRIORITY_TIME_CRITICAL).contains(&priority) {
        return last_error::fail(ERROR_INVALID_PARAMETER);
    }
    let lock = addr_of_mut!((*thread).lock);
    libc::pthread_mutex_lock(lock);
    (*thread).priority = priority;
    let started = (*thread).start_gate == 0;
    let running = (*thread).running;
    let thread_id = (*thread).thread_id;
    libc::pthread_mutex_unlock(lock);
    // the calling thread applies its own priority now; another thread's goes through the start gate (not yet
    // running) or, on Linux, through its thread id
    if thread == CURRENT_THREAD.with(Cell::get) {
        apply_priority(priority);
    } else {
        #[cfg(target_os = "linux")]
        if started && running {
            libc::setpriority(libc::PRIO_PROCESS, thread_id as libc::id_t, nice_of(priority));
        }
        #[cfg(not(target_os = "linux"))]
        let _ = (started, running, thread_id);
    }
    1
}

#[no_mangle]
pub unsafe extern "C" fn PlatThreadSuspend(thread: *mut PlatThread) -> u32 {
    if thread.is_null() {
        set_last_error(ERROR_INVALID_HANDLE);
        return 0xFFFF_FFFF;
    }
    let lock = addr_of_mut!((*thread).lock);
    libc::pthread_mutex_lock(lock);
    let mut result = 0xFFFF_FFFF;
    if (*thread).start_gate > 0 {
        result = (*thread).start_gate; // not started yet: one more resume needed
        (*thread).start_gate += 1;
    }
    libc::pthread_mutex_unlock(lock);
    if result == 0xFFFF_FFFF {
        set_last_error(ERROR_NOT_SUPPORTED);
    }
    result
}

#[no_mangle]
pub unsafe extern "C" fn PlatThreadResume(thread: *mut PlatThread) -> u32 {
    if thread.is_null() {
        set_last_error(ERROR_INVALID_HANDLE);
        return 0xFFFF_FFFF;
    }
    let lock = addr_of_mut!((*thread).lock);
    libc::pthread_mutex_lock(lock);
    let count = (*thread).start_gate;
    if count > 0 {
        (*thread).start_gate -= 1;
        if (*thread).start_gate == 0 {
            libc::pthread_cond_broadcast(addr_of_mut!((*thread).start_condition));
        }
    }
    libc::pthread_mutex_unlock(lock);
    count
}

#[no_mangle]
pub unsafe extern "C" fn PlatThreadTerminate(_thread: *mut PlatThread) -> i32 {
    last_error::fail(ERROR_NOT_SUPPORTED)
}
