//! The Low-Level Engine's Thread System whole in Rust (`src/Low-Level Engine/Thread System` before; LLE lane round 3):
//! the recursive critical section, the event, the read/write lock, the thread object with its two message queues,
//! the scheduler's thread registry and window-event pump, and `TSInitialize`'s main-thread registration.
//!
//! C++ keeps the class names callers use (about 200 files): `TSCriticalSection`, `TSEvent`, `TSReadWriteLock` and
//! `TSThread` have no data and no constructor; a pointer to one is a pointer to the Rust object
//! ([`EeTsSection`], [`EeTsEvent`], [`EeTsLock`], [`EeTsThread`]) and every member is a one-line forwarder.
//! `TSScheduler`'s statics create and destroy them. Every platform call is the original's, in its order, with the
//! same arguments (the golden checks the sequence), so the threads interleave as before.
//!
//! Every function takes a [`Host`]: the platform layer and the Error System in the game ([`PlatHost`]), a recorded
//! trace in the golden (`golden_ts.txt`, from `tests/thread/ts_golden.cpp`; `golden.rs` replays it).

#[cfg(test)]
mod golden;

use std::cell::UnsafeCell;
use std::collections::VecDeque;
use std::ffi::{c_long, c_ulong, c_void, CString};
use std::sync::atomic::{AtomicBool, AtomicI32, AtomicPtr, AtomicU32, AtomicUsize, Ordering::Relaxed};
use std::sync::Mutex;

use ee_plat_sys::thread::imp as sys;

/// The Thread System's results (lle/LowLevelEngineCore.h).
pub const EE_TS_OK: c_long = 0;
pub const EE_TS_NATIVE_ERROR: c_long = 1;
pub const EE_TS_NOT_SUSPENDED: c_long = 2;
pub const EE_TS_NOT_FOUND: c_long = 3;
pub const EE_TS_TIMEOUT: c_long = 0x11;
/// `PLAT_WAIT_*` (platform/PlatThread.h).
pub const EE_TS_WAIT_INFINITE: u32 = 0xFFFF_FFFF;
pub const EE_TS_WAIT_TIMEOUT: i32 = -1;
pub const EE_TS_WAIT_FAILED: i32 = -2;
const WAIT_MAX: usize = 64;
const PRIORITY_ERROR: i32 = 0x7FFF_FFFF;

/// A window-event pump (`TSThread::SetEventPump`): true when it dispatched an event.
pub type EeTsPump = Option<unsafe extern "C" fn() -> bool>;
/// A thread's function (`TSScheduler::CreateThread`): its result is the thread's exit code.
pub type EeTsThreadFunction = Option<unsafe extern "C" fn(*mut c_void) -> c_ulong>;

/// What the Thread System calls: the platform layer (`platform/PlatThread.h`, `PlatSleep`), the Error System's
/// `ESRecordNativeError` and `ESVerify`, and a window-event pump. Handles are opaque pointers.
pub trait Host {
    fn current_thread_id(&self) -> u32;
    fn last_error(&self) -> u32;
    /// `ESRecordNativeError` (the original then called `ESGetNativeError` and dropped its result).
    fn record_native(&self, error: u32);
    /// `ESVerify` (`line` 0: the original's `ESVERIFY`, which reported `__LINE__`).
    fn verify(&self, line: u32, expression: &str);
    fn mutex_create(&self) -> *mut c_void;
    fn mutex_destroy(&self, mutex: *mut c_void);
    fn mutex_lock(&self, mutex: *mut c_void);
    fn mutex_unlock(&self, mutex: *mut c_void);
    fn mutex_try_lock(&self, mutex: *mut c_void) -> bool;
    fn event_create(&self, manual: bool, set: bool) -> *mut c_void;
    fn event_destroy(&self, event: *mut c_void);
    fn event_set(&self, event: *mut c_void) -> bool;
    fn event_reset(&self, event: *mut c_void) -> bool;
    fn event_wait(&self, event: *mut c_void, timeout: u32) -> i32;
    fn event_wait_any(&self, events: &[*mut c_void], timeout: u32) -> i32;
    /// `PlatThreadCreate` with the thread procedure [`ee_ts_thread_main`] and `thread` as its parameter.
    fn thread_create(&self, thread: *mut c_void, stack: u32, suspended: bool, id: &mut u32) -> *mut c_void;
    fn thread_open_current(&self) -> *mut c_void;
    fn thread_close(&self, thread: *mut c_void);
    fn thread_exit_code(&self, thread: *mut c_void, code: &mut u32) -> bool;
    fn thread_priority(&self, thread: *mut c_void) -> i32;
    fn thread_set_priority(&self, thread: *mut c_void, priority: i32) -> bool;
    fn thread_suspend(&self, thread: *mut c_void) -> u32;
    fn thread_resume(&self, thread: *mut c_void) -> u32;
    fn thread_terminate(&self, thread: *mut c_void) -> bool;
    fn sleep(&self, milliseconds: u32);
    /// Calls the window-event pump once.
    fn pump(&self, pump: unsafe extern "C" fn() -> bool) -> bool;
}

/// The game's host: the platform layer and the Error System.
pub struct PlatHost;

// SAFETY (every call below): the platform layer's functions on the handles it created, as the C++ called them.
#[allow(clippy::not_unsafe_ptr_arg_deref)]
impl Host for PlatHost {
    fn current_thread_id(&self) -> u32 {
        unsafe { sys::PlatCurrentThreadId() }
    }
    fn last_error(&self) -> u32 {
        unsafe { sys::PlatGetLastError() }
    }
    fn record_native(&self, error: u32) {
        ee_error::ee_error_record_native(error as c_ulong);
    }
    fn verify(&self, line: u32, expression: &str) {
        let file = CString::new("rust/crates/ee_ts/src/lib.rs").unwrap_or_default();
        let expression = CString::new(expression).unwrap_or_default();
        unsafe { ee_error::ee_error_verify(file.as_ptr(), line as c_ulong, expression.as_ptr()) }
    }
    fn mutex_create(&self) -> *mut c_void {
        unsafe { sys::PlatMutexCreate() as *mut c_void }
    }
    fn mutex_destroy(&self, mutex: *mut c_void) {
        unsafe { sys::PlatMutexDestroy(mutex as *mut sys::PlatMutex) }
    }
    fn mutex_lock(&self, mutex: *mut c_void) {
        unsafe { sys::PlatMutexLock(mutex as *mut sys::PlatMutex) }
    }
    fn mutex_unlock(&self, mutex: *mut c_void) {
        unsafe { sys::PlatMutexUnlock(mutex as *mut sys::PlatMutex) }
    }
    fn mutex_try_lock(&self, mutex: *mut c_void) -> bool {
        unsafe { sys::PlatMutexTryLock(mutex as *mut sys::PlatMutex) != 0 }
    }
    fn event_create(&self, manual: bool, set: bool) -> *mut c_void {
        unsafe { sys::PlatEventCreate(manual as i32, set as i32) as *mut c_void }
    }
    fn event_destroy(&self, event: *mut c_void) {
        unsafe { sys::PlatEventDestroy(event as *mut sys::PlatEvent) }
    }
    fn event_set(&self, event: *mut c_void) -> bool {
        unsafe { sys::PlatEventSet(event as *mut sys::PlatEvent) != 0 }
    }
    fn event_reset(&self, event: *mut c_void) -> bool {
        unsafe { sys::PlatEventReset(event as *mut sys::PlatEvent) != 0 }
    }
    fn event_wait(&self, event: *mut c_void, timeout: u32) -> i32 {
        unsafe { sys::PlatEventWait(event as *mut sys::PlatEvent, timeout) }
    }
    fn event_wait_any(&self, events: &[*mut c_void], timeout: u32) -> i32 {
        unsafe { sys::PlatEventWaitAny(events.as_ptr() as *const *mut sys::PlatEvent, events.len() as u32, timeout) }
    }
    fn thread_create(&self, thread: *mut c_void, stack: u32, suspended: bool, id: &mut u32) -> *mut c_void {
        unsafe { sys::PlatThreadCreate(ee_ts_thread_main, thread, stack, suspended as i32, id) as *mut c_void }
    }
    fn thread_open_current(&self) -> *mut c_void {
        unsafe { sys::PlatThreadOpenCurrent() as *mut c_void }
    }
    fn thread_close(&self, thread: *mut c_void) {
        unsafe { sys::PlatThreadClose(thread as *mut sys::PlatThread) }
    }
    fn thread_exit_code(&self, thread: *mut c_void, code: &mut u32) -> bool {
        unsafe { sys::PlatThreadGetExitCode(thread as *mut sys::PlatThread, code) != 0 }
    }
    fn thread_priority(&self, thread: *mut c_void) -> i32 {
        unsafe { sys::PlatThreadGetPriority(thread as *mut sys::PlatThread) }
    }
    fn thread_set_priority(&self, thread: *mut c_void, priority: i32) -> bool {
        unsafe { sys::PlatThreadSetPriority(thread as *mut sys::PlatThread, priority) != 0 }
    }
    fn thread_suspend(&self, thread: *mut c_void) -> u32 {
        unsafe { sys::PlatThreadSuspend(thread as *mut sys::PlatThread) }
    }
    fn thread_resume(&self, thread: *mut c_void) -> u32 {
        unsafe { sys::PlatThreadResume(thread as *mut sys::PlatThread) }
    }
    fn thread_terminate(&self, thread: *mut c_void) -> bool {
        unsafe { sys::PlatThreadTerminate(thread as *mut sys::PlatThread) != 0 }
    }
    fn sleep(&self, milliseconds: u32) {
        ee_plat_core::time::PlatSleep(milliseconds)
    }
    fn pump(&self, pump: unsafe extern "C" fn() -> bool) -> bool {
        unsafe { pump() }
    }
}

/// Records the platform's last error (`ESRecordNativeError(PlatGetLastError())`) -> `EE_TS_NATIVE_ERROR`.
fn native(host: &impl Host) -> c_long {
    host.record_native(host.last_error());
    EE_TS_NATIVE_ERROR
}

// ------------------------------------------------------------------------------------------- TSCriticalSection

/// `TSCriticalSection`: a recursive platform mutex with its owner and recursion count (written under the lock).
pub struct EeTsSection {
    mutex: *mut c_void,
    owner: AtomicU32,
    count: AtomicU32,
}

// SAFETY: the mutex handle is the platform's, usable from any thread; the rest is atomic.
unsafe impl Send for EeTsSection {}
unsafe impl Sync for EeTsSection {}

impl EeTsSection {
    pub fn new(host: &impl Host) -> Self {
        let mutex = host.mutex_create();
        if mutex.is_null() {
            host.verify(0, "this->mSection != NULL");
        }
        EeTsSection { mutex, owner: AtomicU32::new(0), count: AtomicU32::new(0) }
    }
    pub fn destroy(self, host: &impl Host) {
        host.mutex_destroy(self.mutex);
    }
    fn entered(&self, host: &impl Host) {
        if self.count.fetch_add(1, Relaxed).wrapping_add(1) == 1 {
            self.owner.store(host.current_thread_id(), Relaxed);
        }
    }
    pub fn enter(&self, host: &impl Host) {
        host.mutex_lock(self.mutex);
        self.entered(host);
    }
    pub fn leave(&self, host: &impl Host) {
        if self.count.fetch_sub(1, Relaxed).wrapping_sub(1) == 0 {
            self.owner.store(0, Relaxed);
        }
        host.mutex_unlock(self.mutex);
    }
    /// Enters only if no other thread holds the section (the harness's UI fixture).
    pub fn try_enter(&self, host: &impl Host) -> bool {
        if !host.mutex_try_lock(self.mutex) {
            return false;
        }
        self.entered(host);
        true
    }
    /// (mutex, owner, count)
    pub fn state(&self) -> (*mut c_void, u32, u32) {
        (self.mutex, self.owner.load(Relaxed), self.count.load(Relaxed))
    }
}

// ------------------------------------------------------------------------------------------------- TSEvent

/// `TSEvent`: a platform event (manual or automatic reset).
pub struct EeTsEvent {
    handle: *mut c_void,
    manual: bool,
}

// SAFETY: the platform's event handle, usable from any thread.
unsafe impl Send for EeTsEvent {}
unsafe impl Sync for EeTsEvent {}

impl EeTsEvent {
    /// `TSScheduler::CreateEvent` -> the event, or the error (the native error recorded).
    pub fn create(host: &impl Host, manual: bool) -> Result<Box<Self>, c_long> {
        let handle = host.event_create(manual, false);
        if handle.is_null() {
            return Err(native(host));
        }
        Ok(Box::new(EeTsEvent { handle, manual }))
    }
    #[allow(clippy::boxed_local)]
    pub fn destroy(self: Box<Self>, host: &impl Host) {
        host.event_destroy(self.handle);
    }
    pub fn handle(&self) -> *mut c_void {
        self.handle
    }
    pub fn is_manual(&self) -> bool {
        self.manual
    }
    pub fn set(&self, host: &impl Host) -> c_long {
        if host.event_set(self.handle) {
            EE_TS_OK
        } else {
            native(host)
        }
    }
    pub fn clear(&self, host: &impl Host) -> c_long {
        if host.event_reset(self.handle) {
            EE_TS_OK
        } else {
            native(host)
        }
    }
    /// `EE_TS_TIMEOUT` on time-out.
    pub fn wait(&self, host: &impl Host, timeout: u32) -> c_long {
        match host.event_wait(self.handle, timeout) {
            EE_TS_WAIT_TIMEOUT => EE_TS_TIMEOUT,
            EE_TS_WAIT_FAILED => native(host),
            _ => EE_TS_OK,
        }
    }
    /// `TSEvent::WaitAny` -> the index of the event that was set, `EE_TS_WAIT_TIMEOUT` or `EE_TS_WAIT_FAILED`.
    pub fn wait_any(host: &impl Host, events: &[&EeTsEvent], timeout: u32) -> i32 {
        if events.len() > WAIT_MAX {
            return EE_TS_WAIT_FAILED;
        }
        let handles: Vec<*mut c_void> = events.iter().map(|e| e.handle).collect();
        host.event_wait_any(&handles, timeout)
    }
}

// ----------------------------------------------------------------------------------------- TSReadWriteLock

/// `TSReadWriteLock`: a writer section, an automatic-reset "no readers" event and a manual-reset "reading" event.
pub struct EeTsLock {
    write: EeTsSection,
    no_readers: Box<EeTsEvent>,
    reading: Box<EeTsEvent>,
    readers: AtomicI32,
}

impl EeTsLock {
    /// `TSScheduler::CreateReadWriteLock`: the parts made so far are destroyed when one fails.
    pub fn create(host: &impl Host) -> Result<Box<Self>, c_long> {
        let write = EeTsSection::new(host);
        let no_readers = match EeTsEvent::create(host, false) {
            Ok(e) => e,
            Err(r) => {
                write.destroy(host);
                return Err(r);
            }
        };
        let reading = match EeTsEvent::create(host, true) {
            Ok(e) => e,
            Err(r) => {
                write.destroy(host);
                no_readers.destroy(host);
                return Err(r);
            }
        };
        no_readers.set(host);
        reading.clear(host);
        Ok(Box::new(EeTsLock { write, no_readers, reading, readers: AtomicI32::new(0) }))
    }
    #[allow(clippy::boxed_local)]
    pub fn destroy(self: Box<Self>, host: &impl Host) {
        let lock = *self;
        lock.write.destroy(host);
        lock.no_readers.destroy(host);
        lock.reading.destroy(host);
    }
    pub fn read_count(&self) -> i32 {
        self.readers.load(Relaxed)
    }
    /// The first reader waits until no writer holds the lock and opens reading; every reader waits for reading.
    pub fn read_lock(&self, host: &impl Host) {
        if self.readers.fetch_add(1, Relaxed).wrapping_add(1) == 1 {
            self.no_readers.wait(host, EE_TS_WAIT_INFINITE);
            self.reading.set(host);
        }
        self.reading.wait(host, EE_TS_WAIT_INFINITE);
    }
    /// The last reader closes reading and lets writers in.
    pub fn read_unlock(&self, host: &impl Host) {
        if self.readers.fetch_sub(1, Relaxed).wrapping_sub(1) == 0 {
            self.reading.clear(host);
            self.no_readers.set(host);
        }
    }
    pub fn write_lock(&self, host: &impl Host) {
        self.write.enter(host);
        self.no_readers.wait(host, EE_TS_WAIT_INFINITE);
    }
    pub fn write_unlock(&self, host: &impl Host) {
        self.no_readers.set(host);
        self.write.leave(host);
    }
    /// (the writer section's state, the two events' handles)
    pub fn state(&self) -> ((*mut c_void, u32, u32), *mut c_void, *mut c_void) {
        (self.write.state(), self.no_readers.handle, self.reading.handle)
    }
}

// ------------------------------------------------------------------------------------------------ TSThread

/// A message posted to a thread (the original's Win32 thread message).
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct EeTsPosted {
    pub message: c_ulong,
    pub parameter1: *mut c_void,
    pub parameter2: *mut c_void,
}

/// `TSThread`: a platform thread with its message-id queue (under the message lock, which callers may hold too) and
/// its posted-message queue (under the post lock).
pub struct EeTsThread {
    message_lock: EeTsSection,
    messages: UnsafeCell<VecDeque<c_ulong>>,
    name: Mutex<Vec<u8>>,
    function: EeTsThreadFunction,
    parameter: *mut c_void,
    handle: AtomicPtr<c_void>,
    thread_id: AtomicU32,
    suspended: AtomicBool,
    post_lock: *mut c_void,
    posted: UnsafeCell<VecDeque<EeTsPosted>>,
}

// SAFETY: `messages` is touched only under the message lock, `posted` only under the post lock; handles are the
// platform's; the rest is atomic or set before the thread object is shared.
unsafe impl Send for EeTsThread {}
unsafe impl Sync for EeTsThread {}

impl EeTsThread {
    /// The members in the original's order: the post lock, then the message lock's section.
    fn new(
        host: &impl Host,
        function: EeTsThreadFunction,
        parameter: *mut c_void,
        handle: *mut c_void,
        thread_id: u32,
        suspended: bool,
    ) -> Self {
        let post_lock = host.mutex_create();
        EeTsThread {
            message_lock: EeTsSection::new(host),
            messages: UnsafeCell::default(),
            name: Mutex::default(),
            function,
            parameter,
            handle: AtomicPtr::new(handle),
            thread_id: AtomicU32::new(thread_id),
            suspended: AtomicBool::new(suspended),
            post_lock,
            posted: UnsafeCell::default(),
        }
    }
    /// The destructor: the handle closed (the thread runs on), the message and post locks destroyed.
    fn destroy(self, host: &impl Host) {
        host.thread_close(self.handle.load(Relaxed));
        self.message_lock.destroy(host);
        host.mutex_destroy(self.post_lock);
    }
    fn handle(&self) -> *mut c_void {
        self.handle.load(Relaxed)
    }
    pub fn thread_id(&self) -> u32 {
        self.thread_id.load(Relaxed)
    }
    pub fn is_suspended(&self) -> bool {
        self.suspended.load(Relaxed)
    }
    pub fn message_lock(&self) -> &EeTsSection {
        &self.message_lock
    }

    /// The exit code (`PLAT_THREAD_STILL_ACTIVE` while running); 0 with the native error recorded on failure.
    pub fn exit_code(&self, host: &impl Host) -> u32 {
        let mut code = 0;
        if !host.thread_exit_code(self.handle(), &mut code) {
            native(host);
            code = 0;
        }
        code
    }
    pub fn priority(&self, host: &impl Host) -> i32 {
        let priority = host.thread_priority(self.handle());
        if priority == PRIORITY_ERROR {
            native(host);
        }
        priority
    }
    pub fn set_priority(&self, host: &impl Host, priority: i32) -> c_long {
        if host.thread_set_priority(self.handle(), priority) {
            EE_TS_OK
        } else {
            native(host)
        }
    }

    /// Posts a message; `queue` also queues its id (under the message lock).
    pub fn post(&self, host: &impl Host, message: c_ulong, p1: *mut c_void, p2: *mut c_void, queue: bool) -> c_long {
        host.mutex_lock(self.post_lock);
        // SAFETY: under the post lock
        unsafe { (*self.posted.get()).push_back(EeTsPosted { message, parameter1: p1, parameter2: p2 }) };
        host.mutex_unlock(self.post_lock);
        if !queue {
            return EE_TS_OK;
        }
        self.message_lock.enter(host);
        // SAFETY: under the message lock
        unsafe { (*self.messages.get()).push_back(message) };
        self.message_lock.leave(host);
        EE_TS_OK
    }

    /// Takes one posted message, else (on the window thread) dispatches one window event -> (result, the message;
    /// None when nothing was taken and the outputs stay untouched). A posted message counts when it is WM_QUIT
    /// (0x12) or a WM_USER (0x400+) one; anything else counts only while queued ids are left (one is dropped).
    pub fn receive(&self, host: &impl Host, scheduler: &Scheduler) -> (c_long, Option<EeTsPosted>) {
        let mut message = EeTsPosted { message: 0, parameter1: std::ptr::null_mut(), parameter2: std::ptr::null_mut() };
        host.mutex_lock(self.post_lock);
        // SAFETY: under the post lock
        let taken = unsafe { (*self.posted.get()).pop_front() };
        host.mutex_unlock(self.post_lock);
        let posted = taken.is_some();
        if let Some(m) = taken {
            message = m;
        }
        if !posted && !scheduler.pump_here(host) {
            return (EE_TS_NOT_FOUND, None);
        }
        self.message_lock.enter(host);
        // SAFETY: under the message lock
        let messages = unsafe { &mut *self.messages.get() };
        let result = if (posted && (message.message == 0x12 || message.message >= 0x400)) || !messages.is_empty() {
            EE_TS_OK
        } else {
            EE_TS_NOT_FOUND
        };
        messages.pop_front();
        self.message_lock.leave(host);
        (result, Some(message))
    }

    /// `EE_TS_NOT_SUSPENDED` when it was not suspended; the thread counts as running once its count reaches 0.
    pub fn resume(&self, host: &impl Host) -> c_long {
        match host.thread_resume(self.handle()) {
            0 => EE_TS_NOT_SUSPENDED,
            1 => {
                self.suspended.store(false, Relaxed);
                EE_TS_OK
            }
            0xFFFF_FFFF => native(host),
            _ => EE_TS_OK,
        }
    }
    pub fn suspend(&self, host: &impl Host) -> c_long {
        if host.thread_suspend(self.handle()) != 0xFFFF_FFFF {
            self.suspended.store(true, Relaxed);
            EE_TS_OK
        } else {
            native(host)
        }
    }
    pub fn terminate(&self, host: &impl Host) -> c_long {
        if host.thread_terminate(self.handle()) {
            EE_TS_OK
        } else {
            native(host)
        }
    }
    /// Stores the name (the debugger's thread name was a no-op in this build).
    pub fn set_name(&self, name: &[u8]) {
        *self.name.lock().unwrap_or_else(|e| e.into_inner()) = name.to_vec();
    }
    pub fn name(&self) -> Vec<u8> {
        self.name.lock().unwrap_or_else(|e| e.into_inner()).clone()
    }

    /// Starts the thread (suspended if asked), trying up to 100 times 1 ms apart.
    fn start(&self, host: &impl Host, stack: u32) -> c_long {
        let mut handle: *mut c_void = std::ptr::null_mut();
        let mut id: u32 = 0;
        for _ in 0..100 {
            if !handle.is_null() {
                break;
            }
            handle = host.thread_create(self as *const Self as *mut c_void, stack, self.is_suspended(), &mut id);
            if handle.is_null() {
                host.last_error();
                host.sleep(1);
            }
        }
        if handle.is_null() {
            host.verify(397, "\"Thread could not be created\" == NULL");
            return EE_TS_NATIVE_ERROR;
        }
        self.handle.store(handle, Relaxed);
        self.thread_id.store(id, Relaxed);
        EE_TS_OK
    }

    /// The thread's procedure: its function on its parameter.
    pub fn run(&self) -> u32 {
        match self.function {
            // SAFETY: the function and parameter the creator gave
            #[allow(clippy::unnecessary_cast)]
            Some(f) => unsafe { f(self.parameter) as u32 },
            None => 0,
        }
    }

    /// (handle, id, suspended, post lock, message-lock state, queued ids, posted messages) for the golden.
    #[cfg(test)]
    #[allow(clippy::type_complexity)]
    fn state(&self) -> (*mut c_void, u32, bool, *mut c_void, (*mut c_void, u32, u32), Vec<c_ulong>, Vec<EeTsPosted>) {
        // SAFETY: the golden is single-threaded
        let (m, p) = unsafe {
            ((*self.messages.get()).iter().copied().collect(), (*self.posted.get()).iter().copied().collect())
        };
        (self.handle(), self.thread_id(), self.is_suspended(), self.post_lock, self.message_lock.state(), m, p)
    }
}

// ---------------------------------------------------------------------------------------------- TSScheduler

/// `TSScheduler`'s thread registry (in registration order) and `TSThread`'s window-event pump.
pub struct Scheduler {
    threads: Mutex<Vec<usize>>,
    pump: AtomicUsize,
    pump_thread: AtomicU32,
}

impl Scheduler {
    pub const fn new() -> Self {
        Scheduler { threads: Mutex::new(Vec::new()), pump: AtomicUsize::new(0), pump_thread: AtomicU32::new(0) }
    }
    fn threads(&self) -> std::sync::MutexGuard<'_, Vec<usize>> {
        self.threads.lock().unwrap_or_else(|e| e.into_inner())
    }
    /// The registered threads, in order.
    pub fn list(&self) -> Vec<*mut EeTsThread> {
        self.threads().iter().map(|&t| t as *mut EeTsThread).collect()
    }

    /// `CreateThread(function, ...)`: registers and starts a thread object -> (it, the start's result).
    pub fn create_thread(
        &self,
        host: &impl Host,
        function: EeTsThreadFunction,
        parameter: *mut c_void,
        stack: u32,
        suspended: bool,
    ) -> (*mut EeTsThread, c_long) {
        let thread = EeTsThread::new(host, function, parameter, std::ptr::null_mut(), 0, suspended);
        let thread = Box::into_raw(Box::new(thread));
        self.threads().push(thread as usize);
        // SAFETY: just made; the registry owns it until DestroyThread
        let result = unsafe { (*thread).start(host, stack) };
        (thread, result)
    }

    /// The private `CreateThread(handle, id, suspended)`: registers an existing thread.
    pub fn register_thread(&self, host: &impl Host, handle: *mut c_void, thread_id: u32, suspended: bool) -> c_long {
        let thread = EeTsThread::new(host, None, std::ptr::null_mut(), handle, thread_id, suspended);
        let thread = Box::into_raw(Box::new(thread));
        self.threads().push(thread as usize);
        EE_TS_OK
    }

    /// Unregisters and destroys a thread object; `EE_TS_NOT_FOUND` when it is not registered (NULL is ignored).
    ///
    /// # Safety
    /// `thread` is NULL or a thread object of this registry (a registered one is freed).
    pub unsafe fn destroy_thread(&self, host: &impl Host, thread: *mut EeTsThread) -> c_long {
        if thread.is_null() {
            return EE_TS_OK;
        }
        let mut threads = self.threads();
        let Some(index) = threads.iter().position(|&t| t == thread as usize) else { return EE_TS_NOT_FOUND };
        Box::from_raw(thread).destroy(host);
        threads.remove(index);
        EE_TS_OK
    }

    /// The calling thread's registry index.
    pub fn current_index(&self, host: &impl Host) -> Option<usize> {
        let id = host.current_thread_id();
        // SAFETY: registered threads are live until DestroyThread
        self.threads().iter().position(|&t| unsafe { (*(t as *const EeTsThread)).thread_id() } == id)
    }
    /// The calling thread's object (NULL when it is not registered).
    pub fn current(&self, host: &impl Host) -> *mut EeTsThread {
        let id = host.current_thread_id();
        let threads = self.threads();
        // SAFETY: as above
        threads
            .iter()
            .find(|&&t| unsafe { (*(t as *const EeTsThread)).thread_id() } == id)
            .map_or(std::ptr::null_mut(), |&t| t as *mut EeTsThread)
    }

    pub fn set_pump(&self, pump: EeTsPump, thread_id: u32) {
        self.pump.store(pump.map_or(0, |f| f as usize), Relaxed);
        self.pump_thread.store(thread_id, Relaxed);
    }
    /// One pump call on the pump's thread -> it dispatched an event (false elsewhere or without a pump).
    fn pump_here(&self, host: &impl Host) -> bool {
        let pump = self.pump.load(Relaxed);
        if pump == 0 || host.current_thread_id() != self.pump_thread.load(Relaxed) {
            return false;
        }
        // SAFETY: stored from an `EeTsPump` by set_pump
        let f: unsafe extern "C" fn() -> bool = unsafe { std::mem::transmute(pump) };
        host.pump(f)
    }
    /// `DispatchWindowEvents`: the window events waiting now, on the pump's thread.
    pub fn dispatch(&self, host: &impl Host) {
        while self.pump_here(host) {}
    }

    /// `TSInitialize` after its shutdown registration: registers the calling (main) thread.
    pub fn initialize(&self, host: &impl Host) -> c_long {
        let handle = host.thread_open_current();
        if handle.is_null() {
            return native(host);
        }
        self.register_thread(host, handle, host.current_thread_id(), false)
    }
    /// `TSShutdown`: destroys the calling thread's object.
    pub fn shutdown(&self, host: &impl Host) {
        let thread = self.current(host);
        if !thread.is_null() {
            // SAFETY: registered
            unsafe { self.destroy_thread(host, thread) };
        }
    }
}

impl Default for Scheduler {
    fn default() -> Self {
        Self::new()
    }
}

/// The game's scheduler.
pub static SCHEDULER: Scheduler = Scheduler::new();

// ------------------------------------------------------------------------------------------- the C interface

/// The platform thread procedure of every created thread: `thread` is its [`EeTsThread`].
///
/// # Safety
/// `thread` is a live thread object.
#[no_mangle]
pub unsafe extern "C" fn ee_ts_thread_main(thread: *mut c_void) -> u32 {
    (*(thread as *const EeTsThread)).run()
}

/// `TSScheduler::CreateCriticalSection` (never fails: a NULL platform mutex fails its verification).
#[no_mangle]
pub extern "C" fn ee_ts_section_new() -> *mut EeTsSection {
    Box::into_raw(Box::new(EeTsSection::new(&PlatHost)))
}
/// # Safety
/// `section` is NULL or from [`ee_ts_section_new`], not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_ts_section_free(section: *mut EeTsSection) {
    if !section.is_null() {
        Box::from_raw(section).destroy(&PlatHost);
    }
}
/// # Safety
/// `section` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ts_section_enter(section: *const EeTsSection) {
    (*section).enter(&PlatHost)
}
/// # Safety
/// `section` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ts_section_leave(section: *const EeTsSection) {
    (*section).leave(&PlatHost)
}
/// # Safety
/// `section` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ts_section_try_enter(section: *const EeTsSection) -> bool {
    (*section).try_enter(&PlatHost)
}

/// `TSScheduler::CreateEvent` -> the result; `*out` is set only on success.
///
/// # Safety
/// `out` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_ts_event_new(out: *mut *mut EeTsEvent, manual: bool) -> c_long {
    match EeTsEvent::create(&PlatHost, manual) {
        Ok(e) => {
            *out = Box::into_raw(e);
            EE_TS_OK
        }
        Err(r) => r,
    }
}
/// # Safety
/// `event` is NULL or from [`ee_ts_event_new`], not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_ts_event_free(event: *mut EeTsEvent) {
    if !event.is_null() {
        Box::from_raw(event).destroy(&PlatHost);
    }
}
/// # Safety
/// `event` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ts_event_set(event: *const EeTsEvent) -> c_long {
    (*event).set(&PlatHost)
}
/// # Safety
/// `event` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ts_event_clear(event: *const EeTsEvent) -> c_long {
    (*event).clear(&PlatHost)
}
/// # Safety
/// `event` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ts_event_wait(event: *const EeTsEvent, timeout: u32) -> c_long {
    (*event).wait(&PlatHost, timeout)
}
/// # Safety
/// `event` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ts_event_is_manual(event: *const EeTsEvent) -> bool {
    (*event).is_manual()
}
/// The event's platform handle (`TSEvent::mHandle`, `operator void*`).
///
/// # Safety
/// `event` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ts_event_handle(event: *const EeTsEvent) -> *mut c_void {
    (*event).handle()
}
/// `TSEvent::WaitAny`.
///
/// # Safety
/// `events` holds `count` live events.
#[no_mangle]
pub unsafe extern "C" fn ee_ts_event_wait_any(events: *const *const EeTsEvent, count: u32, timeout: u32) -> i32 {
    if count as usize > WAIT_MAX {
        return EE_TS_WAIT_FAILED;
    }
    let list: Vec<&EeTsEvent> = if count == 0 {
        Vec::new()
    } else {
        std::slice::from_raw_parts(events, count as usize).iter().map(|&e| &*e).collect()
    };
    EeTsEvent::wait_any(&PlatHost, &list, timeout)
}

/// `TSScheduler::CreateReadWriteLock` -> the result; `*out` is set only on success.
///
/// # Safety
/// `out` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_ts_lock_new(out: *mut *mut EeTsLock) -> c_long {
    match EeTsLock::create(&PlatHost) {
        Ok(l) => {
            *out = Box::into_raw(l);
            EE_TS_OK
        }
        Err(r) => r,
    }
}
/// # Safety
/// `lock` is NULL or from [`ee_ts_lock_new`], not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_ts_lock_free(lock: *mut EeTsLock) {
    if !lock.is_null() {
        Box::from_raw(lock).destroy(&PlatHost);
    }
}
/// # Safety
/// `lock` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ts_lock_read_count(lock: *const EeTsLock) -> i32 {
    (*lock).read_count()
}
/// # Safety
/// `lock` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ts_lock_read(lock: *const EeTsLock) {
    (*lock).read_lock(&PlatHost)
}
/// # Safety
/// `lock` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ts_lock_read_unlock(lock: *const EeTsLock) {
    (*lock).read_unlock(&PlatHost)
}
/// # Safety
/// `lock` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ts_lock_write(lock: *const EeTsLock) {
    (*lock).write_lock(&PlatHost)
}
/// # Safety
/// `lock` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ts_lock_write_unlock(lock: *const EeTsLock) {
    (*lock).write_unlock(&PlatHost)
}

/// `TSScheduler::CreateThread(function, ...)` -> the start's result; `*out` is the registered thread object.
///
/// # Safety
/// `out` is writable; the function may run on the new thread with its parameter.
#[no_mangle]
pub unsafe extern "C" fn ee_ts_thread_new(
    out: *mut *mut EeTsThread,
    function: EeTsThreadFunction,
    parameter: *mut c_void,
    stack: u32,
    suspended: bool,
) -> c_long {
    let (thread, result) = SCHEDULER.create_thread(&PlatHost, function, parameter, stack, suspended);
    *out = thread;
    result
}
/// `TSScheduler::DestroyThread`.
///
/// # Safety
/// `thread` is NULL or a thread object (a registered one is freed).
#[no_mangle]
pub unsafe extern "C" fn ee_ts_thread_free(thread: *mut EeTsThread) -> c_long {
    SCHEDULER.destroy_thread(&PlatHost, thread)
}
/// `TSScheduler::GetCurrentThread`.
#[no_mangle]
pub extern "C" fn ee_ts_thread_current() -> *mut EeTsThread {
    SCHEDULER.current(&PlatHost)
}
/// `TSScheduler::GetCurrentThreadIndex` (-1 when the calling thread is not registered).
#[no_mangle]
pub extern "C" fn ee_ts_thread_current_index() -> u32 {
    SCHEDULER.current_index(&PlatHost).map_or(u32::MAX, |i| i as u32)
}
/// # Safety
/// `thread` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ts_thread_exit_code(thread: *const EeTsThread) -> u32 {
    (*thread).exit_code(&PlatHost)
}
/// # Safety
/// `thread` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ts_thread_priority(thread: *const EeTsThread) -> i32 {
    (*thread).priority(&PlatHost)
}
/// # Safety
/// `thread` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ts_thread_set_priority(thread: *const EeTsThread, priority: i32) -> c_long {
    (*thread).set_priority(&PlatHost, priority)
}
/// # Safety
/// `thread` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ts_thread_post(
    thread: *const EeTsThread,
    message: c_ulong,
    p1: *mut c_void,
    p2: *mut c_void,
    queue: bool,
) -> c_long {
    (*thread).post(&PlatHost, message, p1, p2, queue)
}
/// `Receive`: the outputs are written unless nothing was taken.
///
/// # Safety
/// `thread` is live; the outputs are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_ts_thread_receive(
    thread: *const EeTsThread,
    message: *mut c_ulong,
    p1: *mut *mut c_void,
    p2: *mut *mut c_void,
) -> c_long {
    let (result, taken) = (*thread).receive(&PlatHost, &SCHEDULER);
    if let Some(m) = taken {
        *message = m.message;
        *p1 = m.parameter1;
        *p2 = m.parameter2;
    }
    result
}
/// # Safety
/// `thread` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ts_thread_resume(thread: *const EeTsThread) -> c_long {
    (*thread).resume(&PlatHost)
}
/// # Safety
/// `thread` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ts_thread_suspend(thread: *const EeTsThread) -> c_long {
    (*thread).suspend(&PlatHost)
}
/// # Safety
/// `thread` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ts_thread_terminate(thread: *const EeTsThread) -> c_long {
    (*thread).terminate(&PlatHost)
}
/// # Safety
/// `thread` is live; `name` is NULL or NUL-terminated.
#[no_mangle]
pub unsafe extern "C" fn ee_ts_thread_set_name(thread: *const EeTsThread, name: *const std::ffi::c_char) {
    let bytes = if name.is_null() { &b""[..] } else { std::ffi::CStr::from_ptr(name).to_bytes() };
    (*thread).set_name(bytes)
}
/// # Safety
/// `thread` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ts_thread_is_suspended(thread: *const EeTsThread) -> bool {
    (*thread).is_suspended()
}
/// The thread's message lock (`LockMessageQueue`/`UnlockMessageQueue` enter and leave it).
///
/// # Safety
/// `thread` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ts_thread_message_lock(thread: *const EeTsThread) -> *const EeTsSection {
    (*thread).message_lock()
}
/// `TSThread::SetEventPump`.
#[no_mangle]
pub extern "C" fn ee_ts_set_event_pump(pump: EeTsPump, thread_id: u32) {
    SCHEDULER.set_pump(pump, thread_id)
}
/// `TSThread::DispatchWindowEvents`.
#[no_mangle]
pub extern "C" fn ee_ts_dispatch_window_events() {
    SCHEDULER.dispatch(&PlatHost)
}
/// `TSInitialize` after C++ registered [`ee_ts_shutdown`]: registers the calling (main) thread.
#[no_mangle]
pub extern "C" fn ee_ts_initialize() -> c_long {
    SCHEDULER.initialize(&PlatHost)
}
/// `TSInitialize`: registers [`ee_ts_shutdown`] with the engine's shutdown list (ee_util), then the calling (main)
/// thread (`kErrorFailed` when the registration fails, as the original; it never does).
#[no_mangle]
pub extern "C" fn ee_ts_system_initialize() -> c_long {
    if ee_util::ee_util_register_shutdown(ee_ts_shutdown) != 0 {
        return 9;
    }
    ee_ts_initialize()
}
/// The Thread System's shutdown (`URegisterShutdown`): destroys the calling thread's object.
#[no_mangle]
pub extern "C" fn ee_ts_shutdown(_forced: bool) {
    SCHEDULER.shutdown(&PlatHost)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::{Cell, RefCell};

    /// A platform that hands out handles and records the calls.
    #[derive(Default)]
    struct Fake {
        next: Cell<usize>,
        current: Cell<u32>,
        calls: RefCell<Vec<String>>,
    }
    impl Fake {
        fn handle(&self) -> *mut c_void {
            self.next.set(self.next.get() + 0x10);
            self.next.get() as *mut c_void
        }
        fn log(&self, s: String) {
            self.calls.borrow_mut().push(s);
        }
    }
    impl Host for Fake {
        fn current_thread_id(&self) -> u32 {
            self.current.get()
        }
        fn last_error(&self) -> u32 {
            5
        }
        fn record_native(&self, error: u32) {
            self.log(format!("native {error}"));
        }
        fn verify(&self, line: u32, expression: &str) {
            self.log(format!("verify {line} {expression}"));
        }
        fn mutex_create(&self) -> *mut c_void {
            self.handle()
        }
        fn mutex_destroy(&self, m: *mut c_void) {
            self.log(format!("mdestroy {}", m as usize));
        }
        fn mutex_lock(&self, _: *mut c_void) {}
        fn mutex_unlock(&self, _: *mut c_void) {}
        fn mutex_try_lock(&self, _: *mut c_void) -> bool {
            true
        }
        fn event_create(&self, _: bool, _: bool) -> *mut c_void {
            self.handle()
        }
        fn event_destroy(&self, _: *mut c_void) {}
        fn event_set(&self, _: *mut c_void) -> bool {
            true
        }
        fn event_reset(&self, _: *mut c_void) -> bool {
            false
        }
        fn event_wait(&self, _: *mut c_void, _: u32) -> i32 {
            EE_TS_WAIT_TIMEOUT
        }
        fn event_wait_any(&self, e: &[*mut c_void], _: u32) -> i32 {
            e.len() as i32 - 1
        }
        fn thread_create(&self, _: *mut c_void, _: u32, _: bool, id: &mut u32) -> *mut c_void {
            *id = 77;
            self.handle()
        }
        fn thread_open_current(&self) -> *mut c_void {
            self.handle()
        }
        fn thread_close(&self, t: *mut c_void) {
            self.log(format!("tclose {}", t as usize));
        }
        fn thread_exit_code(&self, _: *mut c_void, c: &mut u32) -> bool {
            *c = 259;
            true
        }
        fn thread_priority(&self, _: *mut c_void) -> i32 {
            PRIORITY_ERROR
        }
        fn thread_set_priority(&self, _: *mut c_void, _: i32) -> bool {
            true
        }
        fn thread_suspend(&self, _: *mut c_void) -> u32 {
            0
        }
        fn thread_resume(&self, _: *mut c_void) -> u32 {
            1
        }
        fn thread_terminate(&self, _: *mut c_void) -> bool {
            true
        }
        fn sleep(&self, _: u32) {}
        fn pump(&self, p: unsafe extern "C" fn() -> bool) -> bool {
            unsafe { p() }
        }
    }

    #[test]
    fn a_section_counts_its_owner() {
        let h = Fake { current: Cell::new(9), ..Fake::default() };
        let s = EeTsSection::new(&h);
        s.enter(&h);
        s.enter(&h);
        assert_eq!(s.state().1, 9);
        s.leave(&h);
        assert_eq!(s.state().1, 9);
        s.leave(&h);
        assert_eq!(s.state().1..=s.state().2, 0..=0);
    }

    #[test]
    fn receive_counts_quit_and_user_messages_or_queued_ids() {
        let h = Fake::default();
        let sch = Scheduler::new();
        let t = EeTsThread::new(&h, None, std::ptr::null_mut(), std::ptr::null_mut(), 1, false);
        assert_eq!(t.receive(&h, &sch).0, EE_TS_NOT_FOUND);
        t.post(&h, 0x11, std::ptr::null_mut(), std::ptr::null_mut(), false);
        assert_eq!(t.receive(&h, &sch).0, EE_TS_NOT_FOUND);
        t.post(&h, 0x11, std::ptr::null_mut(), std::ptr::null_mut(), true);
        assert_eq!(
            t.receive(&h, &sch),
            (
                EE_TS_OK,
                Some(EeTsPosted { message: 0x11, parameter1: std::ptr::null_mut(), parameter2: std::ptr::null_mut() })
            )
        );
        t.post(&h, 0x401, 3 as *mut c_void, std::ptr::null_mut(), false);
        assert_eq!(t.receive(&h, &sch).0, EE_TS_OK);
        t.destroy(&h);
    }

    #[test]
    fn the_registry_finds_and_destroys_threads() {
        let h = Fake { current: Cell::new(77), ..Fake::default() };
        let sch = Scheduler::new();
        let (t, r) = sch.create_thread(&h, None, std::ptr::null_mut(), 0, true);
        assert_eq!(r, EE_TS_OK);
        assert_eq!(sch.current(&h), t);
        assert_eq!(sch.current_index(&h), Some(0));
        assert_eq!(unsafe { (*t).resume(&h) }, EE_TS_OK);
        assert!(!unsafe { (*t).is_suspended() });
        assert_eq!(unsafe { sch.destroy_thread(&h, t) }, EE_TS_OK);
        assert!(sch.current(&h).is_null());
        assert_eq!(unsafe { sch.destroy_thread(&h, std::ptr::null_mut()) }, EE_TS_OK);
    }

    #[test]
    fn a_failed_reset_records_the_native_error() {
        let h = Fake::default();
        let e = EeTsEvent::create(&h, true).unwrap();
        assert_eq!(e.clear(&h), EE_TS_NATIVE_ERROR);
        assert_eq!(e.wait(&h, 5), EE_TS_TIMEOUT);
        assert_eq!(*h.calls.borrow(), ["native 5"]);
        let many: Vec<&EeTsEvent> = (0..65).map(|_| &*e).collect();
        assert_eq!(EeTsEvent::wait_any(&h, &many, 0), EE_TS_WAIT_FAILED);
        assert_eq!(EeTsEvent::wait_any(&h, &many[..3], 0), 2);
    }
}
