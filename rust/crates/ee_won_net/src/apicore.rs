//! WONAPICore as a Rust object (R6 WONLobby round 12; WONAPI.cpp, 0x2f2750..0x2f2f3f, deleted): the start-up and
//! shutdown of the WON library's threads, its op queue and timer, the pump and the settings. The core holds the op
//! container and the timer thread (thread.rs) and runs its own pump thread; the socket thread stays a C++
//! `SocketThread*` the game may replace (`WONAPICoreEx::SetSocketThread`), reached through [`EeWonApiHooks`]
//! (its virtual calls, its deletion and the default `new SocketThreadSimple`). An op crosses as its `RefCount*`. The C++ class is a shim keeping the
//! vtable (`~WONAPICore`, `Startup`, `Shutdown`) and the static calls the ops make.

use std::ffi::c_void;
use std::ptr::null_mut;
use std::sync::atomic::{AtomicBool, AtomicPtr, AtomicUsize, Ordering};

use crate::thread::{
    ee_won_container_free, ee_won_container_new, ee_won_container_pump, ee_won_container_purge, ee_won_container_queue,
    ee_won_thread_free, ee_won_thread_is_stopped, ee_won_thread_mark_stopped, ee_won_thread_new, ee_won_thread_signal,
    ee_won_thread_start, ee_won_thread_wait_for_stop, ee_won_timer_thread_add, ee_won_timer_thread_free,
    ee_won_timer_thread_new, ee_won_timer_thread_pump, ee_won_timer_thread_purge, ee_won_timer_thread_remove,
    ee_won_timer_thread_start, ee_won_timer_thread_stop, ee_won_timer_thread_wait_for_stop,
    ee_won_timer_thread_wait_time, EeWonOpContainer, EeWonThread, EeWonThreadBody, EeWonTimerThread,
};

/// The C++ calls of the core: the socket thread's (a `SocketThread*`), its deletion and the default one.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeWonApiHooks {
    /// `new SocketThreadSimple`
    pub socket_new: Option<unsafe extern "C" fn() -> *mut c_void>,
    /// `delete thread` (virtual)
    pub socket_delete: Option<unsafe extern "C" fn(thread: *mut c_void)>,
    /// `SetSingleThreaded(on)`, `NeedThread()`, `Start()`, `Stop(wait)`, `WaitForStop()`, `PurgeOps()`
    pub socket_set_single: Option<unsafe extern "C" fn(thread: *mut c_void, on: bool)>,
    pub socket_need_thread: Option<unsafe extern "C" fn(thread: *mut c_void) -> bool>,
    pub socket_start: Option<unsafe extern "C" fn(thread: *mut c_void)>,
    pub socket_stop: Option<unsafe extern "C" fn(thread: *mut c_void, wait: bool)>,
    pub socket_wait_for_stop: Option<unsafe extern "C" fn(thread: *mut c_void)>,
    pub socket_purge: Option<unsafe extern "C" fn(thread: *mut c_void)>,
    /// `Pump(wait)`, `AddSocketOp(op)`, `RemoveSocketOp(op)` (an op as its `SocketOp*`)
    pub socket_pump: Option<unsafe extern "C" fn(thread: *mut c_void, wait: u32)>,
    pub socket_add: Option<unsafe extern "C" fn(thread: *mut c_void, op: *mut c_void)>,
    pub socket_remove: Option<unsafe extern "C" fn(thread: *mut c_void, op: *mut c_void)>,
}

/// The library's one core (`WONAPICore::mInstance`).
static INSTANCE: AtomicPtr<EeWonApiCore> = AtomicPtr::new(null_mut());
/// `mHaveWrittenVersion`: the version file is written once per run.
static WROTE_VERSION: AtomicBool = AtomicBool::new(false);

/// The core (`WONAPICore`'s members).
pub struct EeWonApiCore {
    cpp: *mut c_void,
    hooks: &'static EeWonApiHooks,
    container: *mut EeWonOpContainer,
    timer: *mut EeWonTimerThread,
    socket: AtomicUsize,
    started: AtomicBool,
    threads: AtomicBool,
    pump_thread_on: AtomicBool,
    version: Vec<u8>,
    pump: *mut EeWonThread,
}

// SAFETY: the raw pointers are the core's own objects (thread-safe Rust objects) and a C++ object the C++ owns
unsafe impl Send for EeWonApiCore {}
// SAFETY: as Send
unsafe impl Sync for EeWonApiCore {}

impl EeWonApiCore {
    fn socket(&self) -> *mut c_void {
        self.socket.load(Ordering::Acquire) as *mut c_void
    }
    fn sock(&self, f: Option<unsafe extern "C" fn(*mut c_void)>) {
        if let Some(h) = f {
            // SAFETY: the table's contract, the core's socket thread
            unsafe { h(self.socket()) };
        }
    }
    fn is_started(&self) -> bool {
        self.started.load(Ordering::Acquire)
    }

    /// `WriteVersion`: once per run, `_wonapiversion.txt` in the WON directory rewritten with the version, only
    /// when the file exists.
    fn write_version(&self) {
        if WROTE_VERSION.swap(true, Ordering::AcqRel) {
            return;
        }
        let mut path = crate::hostnative::file_directory();
        path.extend_from_slice(b"_wonapiversion.txt");
        let file = crate::hostnative::plat_fopen(&path, c"r");
        if file.is_null() {
            return;
        }
        // SAFETY (the files): opened here, closed once
        unsafe { crate::hostnative::fclose(file) };
        let file = crate::hostnative::plat_fopen(&path, c"w");
        if !file.is_null() {
            // `fprintf(file, "%s\n", version)`
            // SAFETY: as above
            unsafe {
                crate::hostnative::fwrite(file, &[&self.version[..], b"\n"].concat());
                crate::hostnative::fclose(file);
            }
        }
    }

    /// `Startup`
    fn startup(&self) {
        if self.is_started() {
            return;
        }
        self.started.store(true, Ordering::Release);
        self.write_version();
        if self.socket().is_null() {
            // SAFETY: the table's contract
            let t = self.hooks.socket_new.map_or(null_mut(), |h| unsafe { h() });
            self.socket.store(t as usize, Ordering::Release);
        }
        let threads = self.threads.load(Ordering::Acquire);
        if let Some(h) = self.hooks.socket_set_single {
            // SAFETY: as above
            unsafe { h(self.socket(), !threads) };
        }
        if threads {
            // SAFETY: as above
            if self.hooks.socket_need_thread.is_some_and(|h| unsafe { h(self.socket()) }) {
                self.sock(self.hooks.socket_start);
            }
            // SAFETY: the core's timer
            unsafe { ee_won_timer_thread_start(self.timer) };
        }
        if self.pump_thread_on.load(Ordering::Acquire) {
            // SAFETY: the core's pump thread, at its address while it runs
            unsafe { ee_won_thread_start(self.pump) };
        }
    }

    /// `Shutdown`
    fn shutdown(&self) {
        if !self.is_started() {
            return;
        }
        self.started.store(false, Ordering::Release);
        // SAFETY (the threads): the core's
        unsafe {
            // `mPumpThread.Stop(false)`: marked, signalled
            if ee_won_thread_mark_stopped(self.pump) {
                ee_won_thread_signal(self.pump);
            }
            ee_won_timer_thread_stop(self.timer, false);
        }
        if let Some(h) = self.hooks.socket_stop {
            // SAFETY: the table's contract
            unsafe { h(self.socket(), false) };
        }
        // SAFETY: as above
        unsafe {
            ee_won_thread_wait_for_stop(self.pump);
            ee_won_timer_thread_wait_for_stop(self.timer);
        }
        self.sock(self.hooks.socket_wait_for_stop);
        // SAFETY: as above
        unsafe { ee_won_timer_thread_purge(self.timer) };
        self.sock(self.hooks.socket_purge);
        // SAFETY: the core's container
        unsafe { ee_won_container_purge(self.container) };
    }

    /// `Pump(milliseconds, maxOps)`: queued ops started and completed for that long; without the core's own
    /// threads the socket and timer ops pumped here too.
    fn pump(&self, milliseconds: u32, max_ops: i32) {
        let start = crate::ffi_net::ee_won_tick();
        let mut elapsed: u32 = 0;
        loop {
            let mut wait = milliseconds.wrapping_sub(elapsed);
            // SAFETY (the objects): the core's
            unsafe {
                if self.threads.load(Ordering::Acquire) {
                    ee_won_container_pump(self.container, wait, max_ops);
                } else {
                    let timer_wait = ee_won_timer_thread_wait_time(self.timer);
                    if timer_wait < wait {
                        wait = timer_wait;
                    }
                    if let Some(h) = self.hooks.socket_pump {
                        h(self.socket(), wait);
                    }
                    ee_won_timer_thread_pump(self.timer);
                    ee_won_container_pump(self.container, 0, max_ops);
                }
            }
            elapsed = crate::ffi_net::ee_won_tick().wrapping_sub(start);
            if elapsed >= milliseconds {
                break;
            }
        }
    }
}

fn instance<'a>() -> Option<&'a EeWonApiCore> {
    // SAFETY: the instance lives until its C++ core's destructor clears it
    unsafe { INSTANCE.load(Ordering::Acquire).as_ref() }
}

unsafe extern "C" fn pump_run(core: *mut c_void) {
    // SAFETY: the core outlives its pump thread (the destructor's Shutdown waits for it)
    let c = unsafe { &*(core as *const EeWonApiCore) };
    loop {
        c.pump(20, 0);
        // SAFETY: the core's pump thread
        if unsafe { ee_won_thread_is_stopped(c.pump) } {
            return;
        }
    }
}

// ---- the C ABI

/// `WONAPICore(name)` (`Init`): the version "wonapi121401", socket and timer threads on, no pump thread, no socket
/// thread yet; the WON directory cleared; Winsock started. It becomes the instance.
///
/// # Safety
/// `cpp` is the C++ core; `core` and `hooks` static tables.
#[no_mangle]
pub unsafe extern "C" fn ee_won_api_new(
    cpp: *mut c_void,
    core: *const crate::cppcore::EeWonCoreHooks,
    hooks: *const EeWonApiHooks,
) -> *mut EeWonApiCore {
    crate::cppcore::bind(core);
    let mut c = Box::new(EeWonApiCore {
        cpp,
        // SAFETY: a static table
        hooks: unsafe { &*hooks },
        container: ee_won_container_new(core),
        timer: ee_won_timer_thread_new(core),
        socket: AtomicUsize::new(0),
        started: AtomicBool::new(false),
        threads: AtomicBool::new(true),
        pump_thread_on: AtomicBool::new(false),
        version: b"wonapi121401".to_vec(),
        pump: null_mut(),
    });
    let p: *mut EeWonApiCore = &mut *c;
    c.pump = ee_won_thread_new(EeWonThreadBody { run: Some(pump_run), destroy: None, context: p.cast() });
    let raw = Box::into_raw(c);
    INSTANCE.store(raw, Ordering::Release);
    // SAFETY: NULL clears it
    unsafe { crate::hostnative::ee_won_api_set_file_directory(std::ptr::null(), 0) };
    crate::ffi_net::ee_won_socket_start();
    raw
}

/// `~WONAPICore`: shut down, the socket thread deleted, Winsock stopped, no instance, the WON directory cleared.
///
/// # Safety
/// `c` came from `ee_won_api_new`, freed once.
#[no_mangle]
pub unsafe extern "C" fn ee_won_api_free(c: *mut EeWonApiCore) {
    if c.is_null() {
        return;
    }
    // SAFETY: the caller's contract
    let core = unsafe { &*c };
    core.shutdown();
    let s = core.socket.swap(0, Ordering::AcqRel) as *mut c_void;
    if !s.is_null() {
        if let Some(h) = core.hooks.socket_delete {
            // SAFETY: the table's contract
            unsafe { h(s) };
        }
    }
    crate::ffi_net::ee_won_socket_stop();
    let _ = INSTANCE.compare_exchange(c, null_mut(), Ordering::AcqRel, Ordering::Acquire);
    // SAFETY: as above
    unsafe { crate::hostnative::ee_won_api_set_file_directory(std::ptr::null(), 0) };
    // SAFETY: the box; the threads stopped
    let core = unsafe { Box::from_raw(c) };
    // SAFETY: the core's objects, freed once (the member order of the original's destructor: the pump thread, the
    // timer, the container)
    unsafe {
        ee_won_thread_free(core.pump);
        ee_won_timer_thread_free(core.timer);
        ee_won_container_free(core.container);
    }
}

fn with_core<R>(c: *const EeWonApiCore, f: impl FnOnce(&EeWonApiCore) -> R) -> Option<R> {
    // SAFETY: a live core from ee_won_api_new (the C++ shim's)
    unsafe { c.as_ref() }.map(f)
}

/// `Startup()`: once; the version file, the socket thread (a `SocketThreadSimple` unless one was set) single-threaded
/// or started with the timer thread, the pump thread when asked for.
///
/// # Safety
/// `c` is a live core.
#[no_mangle]
pub unsafe extern "C" fn ee_won_api_startup(c: *const EeWonApiCore) {
    with_core(c, EeWonApiCore::startup);
}

/// `Shutdown()`: the pump, timer and socket threads stopped and waited for, every op they or the queue hold killed.
///
/// # Safety
/// `c` is a live core.
#[no_mangle]
pub unsafe extern "C" fn ee_won_api_shutdown(c: *const EeWonApiCore) {
    with_core(c, EeWonApiCore::shutdown);
}

/// `Pump(milliseconds, maxOps)`.
///
/// # Safety
/// `c` is a live core.
#[no_mangle]
pub unsafe extern "C" fn ee_won_api_pump(c: *const EeWonApiCore, milliseconds: u32, max_ops: i32) {
    with_core(c, |x| x.pump(milliseconds, max_ops));
}

/// The setters of `WONAPICoreEx`, each only before start-up: 0 `SetDoPumpThread`, 1 `SetDoSocketAndTimerThreads`.
///
/// # Safety
/// `c` is a live core.
#[no_mangle]
pub unsafe extern "C" fn ee_won_api_set(c: *const EeWonApiCore, which: i32, on: bool) {
    with_core(c, |x| {
        if !x.is_started() {
            if which == 0 { &x.pump_thread_on } else { &x.threads }.store(on, Ordering::Release);
        }
    });
}

/// `SetSocketThread(thread)`: before start-up the old one deleted and this one kept; after, this one deleted.
///
/// # Safety
/// `c` is a live core; `thread` a `SocketThread*` it takes (or NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_won_api_set_socket_thread(c: *const EeWonApiCore, thread: *mut c_void) {
    with_core(c, |x| {
        let delete = |t: *mut c_void| {
            if let (false, Some(h)) = (t.is_null(), x.hooks.socket_delete) {
                // SAFETY: the table's contract
                unsafe { h(t) };
            }
        };
        if x.is_started() {
            delete(thread);
        } else {
            delete(x.socket.swap(thread as usize, Ordering::AcqRel) as *mut c_void);
        }
    });
}

/// `IsStarted()`: a core exists and is started.
#[no_mangle]
pub extern "C" fn ee_won_api_is_started() -> bool {
    instance().is_some_and(EeWonApiCore::is_started)
}

/// `GetInstance()`: the C++ core (NULL for none).
#[no_mangle]
pub extern "C" fn ee_won_api_instance() -> *mut c_void {
    instance().map_or(null_mut(), |c| c.cpp)
}

/// `QueueOp(op)`: started, into the queue; not started, only a finished op; no core, nothing.
///
/// # Safety
/// `op` is a live `AsyncOp*` (as its `RefCount*`).
#[no_mangle]
pub unsafe extern "C" fn ee_won_api_queue_op(op: *mut c_void) {
    let Some(c) = instance() else { return };
    if c.is_started() || crate::cppcore::op(op).is_some_and(|o| o.status() != crate::status::WS_PENDING) {
        // SAFETY: the core's container; a live op
        unsafe { ee_won_container_queue(c.container, op) };
    }
}

/// `AddTimerOp(op, timeout)`: started, to the timer thread (a reference held while it waits); else killed.
///
/// # Safety
/// As `ee_won_api_queue_op`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_api_add_timer_op(op: *mut c_void, timeout: u32) {
    match instance().filter(|c| c.is_started()) {
        Some(c) => {
            crate::cppcore::ref_add(op);
            // SAFETY: the core's timer; a live op
            if unsafe { ee_won_timer_thread_add(c.timer, op, timeout) } {
                crate::cppcore::ref_release(op);
            }
        }
        None => {
            if let Some(o) = crate::cppcore::op(op) {
                o.kill(true);
            }
        }
    }
}

/// `RemoveTimerOp(op)`: started, taken back from the timer thread (its reference given back).
///
/// # Safety
/// As `ee_won_api_queue_op`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_api_remove_timer_op(op: *mut c_void) {
    if let Some(c) = instance().filter(|c| c.is_started()) {
        // SAFETY: as above
        if unsafe { ee_won_timer_thread_remove(c.timer, op) } {
            crate::cppcore::ref_release(op);
        }
    }
}

/// `AddToSocketThread(op)` / `RemoveFromSocketThread(op)` (`add`): started, to (from) the socket thread; an op added
/// to no started core is killed. `op` is the `SocketOp*`.
///
/// # Safety
/// As `ee_won_api_queue_op`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_api_socket_op(op: *mut c_void, add: bool) {
    match instance().filter(|c| c.is_started()) {
        Some(c) => {
            if let Some(h) = if add { c.hooks.socket_add } else { c.hooks.socket_remove } {
                // SAFETY: the table's contract
                unsafe { h(c.socket(), op) };
            }
        }
        None if add => {
            if let Some(o) = crate::cppcore::op(op) {
                o.kill(true);
            }
        }
        None => {}
    }
}

/// `WONAPICoreEx::SetDefaultFileDirectory(dir)`: before start-up, the WON directory (a separator added; empty: the
/// data root again, hostnative.rs).
///
/// # Safety
/// `c` is a live core; `dir` a C string (or NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_won_api_set_directory(c: *const EeWonApiCore, dir: *const std::ffi::c_char) {
    with_core(c, |x| {
        if x.is_started() {
            return;
        }
        // SAFETY: the caller's string
        let d = if dir.is_null() { &[][..] } else { unsafe { std::ffi::CStr::from_ptr(dir) }.to_bytes() };
        // SAFETY: the bytes above
        unsafe { crate::hostnative::ee_won_api_set_file_directory(d.as_ptr(), d.len()) };
    });
}

#[cfg(test)]
#[path = "apicore_tests.rs"]
mod tests;
