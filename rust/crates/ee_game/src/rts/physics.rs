//! `RTSPhysics`' bodies (LLE round 8, from `src/RTS Engine/Physics/RTSPhysics.cpp`): the simulation thread's
//! construction and teardown, its main loop, its messages, the saved timing state and the speed.
//!
//! The class stays C++ (`ee/EEPhysics.h`: EEPhysics derives from it and overrides its virtuals); its plain timing
//! members are the record [`EeRtsPhysicsFields`], a base of the class, so the bodies here read and write them in
//! place. The thread, the events, the timer and the lock are Rust objects (ee_base ts, time) reached through
//! their slots in the C++ object; every other C++ call (the virtuals, the file's typed reads and writes, the
//! random generator, the communications) is a one-statement shim of `RTSPhysics.cpp` ([`PhysicsCpp`]).
//!
//! Volatile: the original's `mTimerRunning`, `mTicksOwed` and `mStopThread` are volatile (other threads write
//! them); the loop here reads and writes them with volatile accesses.

use core::ffi::{c_char, c_long, c_ulong, c_void};
use core::ptr::{addr_of, addr_of_mut, read_volatile, write_volatile};

use ee_base::shim::ShimOut;

/// `RTSPhysics`' plain members (`ee/EEPhysics.h`), in the class's order: a base of the class.
#[repr(C)]
#[allow(non_snake_case)]
pub struct EeRtsPhysicsFields {
    /// saved, otherwise unused (= 0)
    pub mL28: c_ulong,
    /// the timer runs (= false)
    pub mTimerRunning: bool,
    /// seconds per tick (= 1)
    pub mSpeed: f32,
    /// the timer's interval in milliseconds (= 1000)
    pub mTimerInterval: c_ulong,
    /// (= 1)
    pub mTicksPerSecond: c_ulong,
    /// the ticks run since the timer started
    pub mTicksRun: c_ulong,
    /// the ticks the loop still owes
    pub mTicksOwed: c_ulong,
    /// the constructor's ticks per second
    pub mDefaultTicksPerSecond: c_ulong,
    /// the thread is to end (message 0x401)
    pub mStopThread: bool,
    /// (= true before the thread starts)
    pub mSlowPlayback: bool,
}

ee_base::cpp_shims! {
    trait PhysicsCpp;
    fn fields(physics: usize) -> *mut EeRtsPhysicsFields = ee_rtsphyscpp_fields;
    fn thread_slot(physics: usize) -> *mut *mut c_void = ee_rtsphyscpp_thread_slot;
    fn created_slot(physics: usize) -> *mut *mut c_void = ee_rtsphyscpp_created_slot;
    fn start_slot(physics: usize) -> *mut *mut c_void = ee_rtsphyscpp_start_slot;
    fn done_slot(physics: usize) -> *mut *mut c_void = ee_rtsphyscpp_done_slot;
    fn tick_slot(physics: usize) -> *mut *mut c_void = ee_rtsphyscpp_tick_slot;
    fn lock_slot(physics: usize) -> *mut *mut c_void = ee_rtsphyscpp_lock_slot;
    fn set_graphics(physics: usize, graphics: usize) = ee_rtsphyscpp_set_graphics;
    fn render_now(physics: usize) -> *mut c_void = ee_rtsphyscpp_render_now;
    fn set_timer(physics: usize, timer: usize) = ee_rtsphyscpp_set_timer;
    fn timer(physics: usize) -> *mut c_void = ee_rtsphyscpp_timer;
    fn thread_name() -> *const c_char = ee_rtsphyscpp_thread_name;
    fn random_seed(physics: usize) -> u32 = ee_rtsphyscpp_random_seed;
    fn set_random_seed(physics: usize, seed: u32) = ee_rtsphyscpp_set_random_seed;
    fn random(physics: usize) -> f32 = ee_rtsphyscpp_random;
    fn messages_size(physics: usize) -> usize = ee_rtsphyscpp_messages_size;
    fn messages_front(physics: usize) -> usize = ee_rtsphyscpp_messages_front;
    fn messages_pop_front(physics: usize) = ee_rtsphyscpp_messages_pop_front;
    fn message_dereference(message: usize) = ee_rtsphyscpp_message_dereference;
    fn handle_message(physics: usize, message: c_ulong, first: usize, second: usize) = ee_rtsphyscpp_handle_message;
    fn lock(physics: usize) = ee_rtsphyscpp_lock;
    fn unlock(physics: usize) = ee_rtsphyscpp_unlock;
    fn tick(physics: usize) = ee_rtsphyscpp_tick;
    fn idle(physics: usize) = ee_rtsphyscpp_idle;
    fn update_timing(physics: usize) = ee_rtsphyscpp_update_timing;
    fn ee_physics(physics: usize) -> usize = ee_rtsphyscpp_ee_physics;
    fn ticks_to_process(ee_physics: usize) -> c_ulong = ee_rtsphyscpp_ticks_to_process;
    fn sim_server() -> usize = ee_rtsphyscpp_sim_server;
    fn sim_communications_tick(server: usize) = ee_rtsphyscpp_sim_communications_tick;
    fn after_tick_hook() -> Option<unsafe extern "C" fn()> = ee_rtsphyscpp_after_tick_hook;
    fn cts_processed() -> c_ulong = ee_rtsphyscpp_cts_processed;
    fn current_ct() -> c_ulong = ee_rtsphyscpp_current_ct;
    fn debug_out(text: *const c_char) = ee_rtsphyscpp_debug_out;
    fn read_ulong(file: usize, out: ShimOut<c_ulong>) -> c_long = ee_rtscpp_read_ulong;
    fn read_bool(file: usize, out: ShimOut<bool>) -> c_long = ee_rtscpp_read_bool;
    fn read_float(file: usize, out: ShimOut<f32>) -> c_long = ee_rtscpp_read_float;
    fn write_ulong(file: usize, value: c_ulong) -> c_long = ee_rtscpp_write_ulong;
    fn write_bool(file: usize, value: bool) -> c_long = ee_rtscpp_write_bool;
    fn write_float(file: usize, value: f32) -> c_long = ee_rtscpp_write_float;
}

/// The platform's calls (the Rust Thread System, Time Manager and simulation flags), a trait so that the tests can
/// stand in for them.
pub trait PhysicsPlat {
    fn event_new(&mut self, slot: *mut *mut c_void, manual: bool) -> c_long;
    fn event_free(&mut self, event: *mut c_void);
    fn event_set(&mut self, event: *mut c_void);
    fn event_clear(&mut self, event: *mut c_void);
    fn event_wait(&mut self, event: *mut c_void, timeout: u32) -> c_long;
    fn section_new(&mut self, slot: *mut *mut c_void);
    fn section_enter(&mut self, section: *mut c_void);
    fn section_leave(&mut self, section: *mut c_void);
    fn section_free(&mut self, section: *mut c_void);
    fn thread_new(&mut self, slot: *mut *mut c_void, parameter: usize) -> c_long;
    fn thread_free(&mut self, thread: *mut c_void);
    fn thread_set_priority(&mut self, thread: *mut c_void, priority: i32) -> c_long;
    fn thread_set_name(&mut self, thread: *mut c_void, name: *const c_char);
    fn thread_receive(&mut self, thread: *mut c_void, message: &mut c_ulong, first: &mut usize, second: &mut usize)
        -> c_long;
    fn timer_stop(&mut self, timer: *mut c_void);
    fn timer_start(&mut self, timer: *mut c_void) -> c_long;
    fn timer_set_interval(&mut self, timer: *mut c_void, interval: c_ulong);
    fn timer_set_type(&mut self, timer: *mut c_void, kind: u32);
    fn timer_set_event(&mut self, timer: *mut c_void, event: *mut c_void);
    fn fp_precision53(&mut self);
    fn fp_check53(&mut self);
    fn time(&mut self) -> c_ulong;
    fn sleep(&mut self, milliseconds: u32);
    fn sim_mode(&mut self) -> bool;
    fn sim_run(&mut self) -> bool;
    fn sim_after_tick(&mut self);
    fn verify(&mut self, ok: bool, line: c_ulong, expression: &'static [u8]);
}

/// `kErrorOK`, `keseOK`.
const OK: c_long = 0;
/// `TSEvent::Wait`'s timeout.
const WAIT_TIMEOUT: c_long = 0x11;
/// `kThreadMessageQuit`.
const MESSAGE_QUIT: c_ulong = 0x401;
/// `kFPPrecision53`'s `_PC_53`.
#[cfg(not(test))]
const FP_PRECISION_53: c_ulong = 0x10000;

/// `RTSPhysics::RTSPhysics`: the events, a stopped timer, the thread (waited for until it starts) and the server
/// messages' lock.
///
/// # Safety
/// `physics` is the C++ object under construction.
pub unsafe fn construct<C: PhysicsCpp, P: PhysicsPlat>(
    c: &mut C,
    p: &mut P,
    physics: usize,
    graphics: usize,
    timer: usize,
    default_ticks_per_second: c_long,
    priority: c_long,
) {
    let f = c.fields(physics);
    let r = p.event_new(c.created_slot(physics), true);
    p.verify(r == OK, 49, b"TSScheduler::CreateEvent(this->mThreadCreated) == keseOK");
    let r = p.event_new(c.done_slot(physics), true);
    p.verify(r == OK, 50, b"TSScheduler::CreateEvent(this->mThreadDone) == keseOK");
    let r = p.event_new(c.start_slot(physics), true);
    p.verify(r == OK, 51, b"TSScheduler::CreateEvent(this->mStartThreadMainLoop) == keseOK");
    let r = p.event_new(c.tick_slot(physics), false);
    p.verify(r == OK, 52, b"TSScheduler::CreateEvent(this->mTimerTick,false) == keseOK");
    c.set_graphics(physics, graphics);
    unsafe {
        (*f).mSpeed = 1.0;
        (*f).mDefaultTicksPerSecond = default_ticks_per_second as c_ulong;
        (*f).mTicksPerSecond = 1;
        (*f).mTimerInterval = 1000;
        (*f).mTicksRun = 0;
        (*f).mTicksOwed = 0;
        (*f).mL28 = 0;
        (*f).mTimerRunning = false;
        (*f).mStopThread = false;
    }
    c.set_random_seed(physics, 1);
    c.set_timer(physics, timer);
    let t = c.timer(physics);
    p.timer_stop(t);
    p.timer_set_type(t, 1);
    p.timer_set_event(t, unsafe { *c.tick_slot(physics) });
    p.timer_set_interval(t, unsafe { (*f).mTimerInterval });
    unsafe { (*f).mSlowPlayback = true };
    let r = p.thread_new(c.thread_slot(physics), physics);
    p.verify(r == OK, 81, b"TSScheduler::CreateThread(this->mThread, RTSPhysics::Main, (void*)this) == keseOK");
    let thread = unsafe { *c.thread_slot(physics) };
    let r = p.thread_set_priority(thread, priority as i32);
    p.verify(r == OK, 82, b"this->mThread->SetPriority(inThreadPriority) == keseOK");
    p.event_wait(unsafe { *c.created_slot(physics) }, u32::MAX);
    p.thread_set_name(thread, c.thread_name());
    p.section_new(c.lock_slot(physics));
    let lock = unsafe { *c.lock_slot(physics) };
    p.verify(!lock.is_null(), 91, b"TSScheduler::CreateCriticalSection(this->mServerMessageQueueLock) == keseOK");
}

/// `RTSPhysics::~RTSPhysics`: the timer stopped, the events and the thread destroyed, the queued server messages
/// dereferenced under their lock, the lock destroyed.
///
/// # Safety
/// `physics` is the C++ object being destroyed.
pub unsafe fn destruct<C: PhysicsCpp, P: PhysicsPlat>(c: &mut C, p: &mut P, physics: usize) {
    p.timer_stop(c.timer(physics));
    unsafe {
        p.event_free(*c.created_slot(physics));
        p.event_free(*c.done_slot(physics));
        p.event_free(*c.start_slot(physics));
        p.event_free(*c.tick_slot(physics));
        p.thread_free(*c.thread_slot(physics));
    }
    let lock = unsafe { *c.lock_slot(physics) };
    p.section_enter(lock);
    while c.messages_size(physics) > 0 {
        let m = c.messages_front(physics);
        c.messages_pop_front(physics);
        c.message_dereference(m);
    }
    p.section_leave(lock);
    p.section_free(lock);
}

/// `RTSPhysics::GetRandom(long, long)`: an integer from `min` to `max` inclusive from one draw (`UMFloatToLong`, a
/// result past `max` clamped).
pub fn random_range<C: PhysicsCpp>(c: &mut C, physics: usize, min: c_long, max: c_long) -> c_long {
    let limit = max.wrapping_add(1);
    let value = c.random(physics) * limit.wrapping_sub(min) as f32 + min as f32;
    let result = ee_base::math::um::lrintf(value) as c_long;
    if result == limit {
        max
    } else {
        result
    }
}

/// `RTSPhysics::Load`: the seed, the counters and flags (the saved running flag read and dropped), then the timer
/// reset at the saved interval and restarted only when it already ran; the first read error.
///
/// # Safety
/// `physics` is the live object, `file` its `FSFile`.
pub unsafe fn load<C: PhysicsCpp, P: PhysicsPlat>(c: &mut C, p: &mut P, physics: usize, file: usize) -> c_long {
    let f = c.fields(physics);
    let mut seed: c_ulong = 0;
    let mut running = false;
    let mut stop = false;
    let mut r = c.read_ulong(file, ShimOut::of(&mut seed));
    unsafe {
        if r == OK {
            c.set_random_seed(physics, seed as u32);
            r = c.read_ulong(file, ShimOut(addr_of_mut!((*f).mL28)));
        }
        if r == OK {
            r = c.read_bool(file, ShimOut::of(&mut running));
        }
        if r == OK {
            r = c.read_float(file, ShimOut(addr_of_mut!((*f).mSpeed)));
        }
        if r == OK {
            r = c.read_ulong(file, ShimOut(addr_of_mut!((*f).mTimerInterval)));
        }
        if r == OK {
            r = c.read_ulong(file, ShimOut(addr_of_mut!((*f).mTicksPerSecond)));
        }
        if r == OK {
            r = c.read_ulong(file, ShimOut(addr_of_mut!((*f).mTicksRun)));
        }
        if r == OK {
            r = c.read_ulong(file, ShimOut(addr_of_mut!((*f).mDefaultTicksPerSecond)));
        }
        if r == OK {
            r = c.read_bool(file, ShimOut::of(&mut stop));
            if r == OK {
                write_volatile(addr_of_mut!((*f).mStopThread), stop);
                r = c.read_bool(file, ShimOut(addr_of_mut!((*f).mSlowPlayback)));
            }
        }
        write_volatile(addr_of_mut!((*f).mTicksOwed), 0);
        let t = c.timer(physics);
        p.timer_stop(t);
        p.timer_set_interval(t, (*f).mTimerInterval);
        if read_volatile(addr_of!((*f).mTimerRunning)) {
            r = p.timer_start(t);
        }
    }
    r
}

/// `RTSPhysics::Save`: the seed, the counters and flags; the first write error.
///
/// # Safety
/// As [`load`].
pub unsafe fn save<C: PhysicsCpp>(c: &mut C, physics: usize, file: usize) -> c_long {
    let f = c.fields(physics);
    unsafe {
        let running = read_volatile(addr_of!((*f).mTimerRunning));
        let stop = read_volatile(addr_of!((*f).mStopThread));
        let seed = c.random_seed(physics) as c_ulong;
        let steps: [&dyn Fn(&mut C) -> c_long; 9] = [
            &|c: &mut C| c.write_ulong(file, seed),
            &|c: &mut C| c.write_ulong(file, (*f).mL28),
            &|c: &mut C| c.write_bool(file, running),
            &|c: &mut C| c.write_float(file, (*f).mSpeed),
            &|c: &mut C| c.write_ulong(file, (*f).mTimerInterval),
            &|c: &mut C| c.write_ulong(file, (*f).mTicksPerSecond),
            &|c: &mut C| c.write_ulong(file, (*f).mTicksRun),
            &|c: &mut C| c.write_ulong(file, (*f).mDefaultTicksPerSecond),
            &|c: &mut C| c.write_bool(file, stop),
        ];
        for step in steps {
            let r = step(c);
            if r != OK {
                return r;
            }
        }
        c.write_bool(file, (*f).mSlowPlayback)
    }
}

/// `RTSPhysics::SetSpeed`: seconds per tick; a change reprograms the timer (restarted when it runs); then the
/// timing is updated (a virtual).
///
/// # Safety
/// As [`load`].
pub unsafe fn set_speed<C: PhysicsCpp, P: PhysicsPlat>(c: &mut C, p: &mut P, physics: usize, speed: f32) {
    let f = c.fields(physics);
    unsafe {
        if speed != (*f).mSpeed {
            (*f).mSpeed = speed;
            (*f).mTimerInterval = ee_base::math::um::floor_unsigned(speed * 1000.0) as c_ulong;
            (*f).mTicksPerSecond = ee_base::math::um::floor_unsigned(1.0 / speed) as c_ulong;
            let t = c.timer(physics);
            p.timer_stop(t);
            p.timer_set_interval(t, (*f).mTimerInterval);
            if read_volatile(addr_of!((*f).mTimerRunning)) {
                p.timer_start(t);
            }
        }
    }
    c.update_timing(physics);
}

/// `RTSPhysics::HandleMessage`: quit; stop the timer; stop it and reset the speed; start it.
///
/// # Safety
/// As [`load`].
pub unsafe fn handle_message<C: PhysicsCpp, P: PhysicsPlat>(c: &mut C, p: &mut P, physics: usize, message: c_ulong) {
    let f = c.fields(physics);
    unsafe {
        match message {
            MESSAGE_QUIT => write_volatile(addr_of_mut!((*f).mStopThread), true),
            0x402 | 0x403 => {
                write_volatile(addr_of_mut!((*f).mTimerRunning), false);
                p.timer_stop(c.timer(physics));
                p.event_clear(*c.tick_slot(physics));
                if message == 0x403 {
                    set_speed(c, p, physics, 1.0 / (*f).mDefaultTicksPerSecond as f32);
                }
                (*f).mTicksRun = 0;
                write_volatile(addr_of_mut!((*f).mTicksOwed), 0);
            }
            0x404 => {
                (*f).mTicksRun = 0;
                write_volatile(addr_of_mut!((*f).mTicksOwed), 0);
                p.event_clear(*c.tick_slot(physics));
                p.timer_start(c.timer(physics));
                write_volatile(addr_of_mut!((*f).mTimerRunning), true);
            }
            _ => {}
        }
    }
}

/// One step of the loop's tick: a tick run and counted (`Tick`, `mTicksRun++`).
fn tick_once<C: PhysicsCpp>(c: &mut C, physics: usize, f: *mut EeRtsPhysicsFields) {
    c.tick(physics);
    // SAFETY: the live record
    unsafe { (*f).mTicksRun = (*f).mTicksRun.wrapping_add(1) };
}

/// `RTSPhysics::Main`: the simulation thread. Handles its messages and, while the timer runs, ticks the game (at
/// most twice per timer tick, limited by the communication ticks) and wakes the renderer; idles otherwise. In the
/// headless simulation (`gPlatSimMode`) it ticks as fast as the server's ticks allow, while `gPlatSimRun` is set.
///
/// # Safety
/// `physics` is the live object (the thread's parameter).
pub unsafe fn main_loop<C: PhysicsCpp, P: PhysicsPlat>(c: &mut C, p: &mut P, physics: usize) -> c_ulong {
    let f = c.fields(physics);
    let mut last_time: c_ulong = 0;
    let (mut message, mut first, mut second) = (0 as c_ulong, 0usize, 0usize);
    let stop = || unsafe { read_volatile(addr_of!((*f).mStopThread)) };
    let running = || unsafe { read_volatile(addr_of!((*f).mTimerRunning)) };
    let owed = || unsafe { read_volatile(addr_of!((*f).mTicksOwed)) };
    let set_owed = |v: c_ulong| unsafe { write_volatile(addr_of_mut!((*f).mTicksOwed), v) };
    unsafe {
        p.event_set(*c.created_slot(physics));
        p.event_wait(*c.start_slot(physics), u32::MAX);
    }
    p.fp_precision53();
    while !stop() {
        let thread = unsafe { *c.thread_slot(physics) };
        let r = p.thread_receive(thread, &mut message, &mut first, &mut second);
        p.fp_check53();
        if r == OK {
            c.handle_message(physics, message, first, second);
        }
        if running() && !stop() && p.sim_mode() {
            let ee = c.ee_physics(physics);
            let server = c.sim_server();
            if !p.sim_run() || ee == 0 || server == 0 {
                p.sleep(1);
                continue;
            }
            c.sim_communications_tick(server);
            loop {
                c.lock(physics);
                tick_once(c, physics, f);
                if let Some(hook) = c.after_tick_hook() {
                    // SAFETY: the TEST ONLY hook EE-SIM set
                    unsafe { hook() };
                }
                let more = c.ticks_to_process(ee) > 0 && running() && !stop();
                if !more {
                    p.sim_after_tick();
                }
                c.unlock(physics);
                if !more {
                    break;
                }
                let thread = unsafe { *c.thread_slot(physics) };
                if p.thread_receive(thread, &mut message, &mut first, &mut second) == OK {
                    c.handle_message(physics, message, first, second);
                }
            }
        } else if running() && !stop() {
            let interval = unsafe { (*f).mTimerInterval };
            let ticks = p.time().wrapping_sub(last_time) / interval;
            if ticks > 1 {
                set_owed(owed().wrapping_add(ticks - 1));
            }
            loop {
                let r = p.event_wait(unsafe { *c.tick_slot(physics) }, 3000);
                last_time = p.time();
                if r == WAIT_TIMEOUT {
                    let t = c.timer(physics);
                    p.timer_stop(t);
                    p.timer_set_interval(t, unsafe { (*f).mTimerInterval });
                    p.timer_start(t);
                    c.debug_out(c"!RTSPhysics::Main: the timer wasn't ticking. Performed a hard reset on the timer.\n".as_ptr());
                }
                if r != WAIT_TIMEOUT {
                    break;
                }
            }
            c.lock(physics);
            set_owed(owed().wrapping_add(1));
            let ee = c.ee_physics(physics);
            let behind = c.cts_processed().wrapping_add(1) < c.current_ct();
            if behind {
                set_owed(100000);
            }
            if !behind && ee != 0 && c.ticks_to_process(ee) < owed() {
                set_owed(c.ticks_to_process(ee));
                if owed() == 0 {
                    set_owed(1);
                }
            }
            if owed() > 0 && running() {
                tick_once(c, physics, f);
                set_owed(owed().wrapping_sub(1));
                if owed() > 0 && running() {
                    tick_once(c, physics, f);
                    set_owed(owed().wrapping_sub(1));
                }
            }
            p.event_set(c.render_now(physics));
            c.unlock(physics);
        } else {
            c.idle(physics);
            p.event_set(c.render_now(physics));
            p.sleep(1);
        }
    }
    unsafe { p.event_set(*c.done_slot(physics)) };
    0
}

/// The platform calls on the Rust objects themselves.
#[cfg(not(test))]
pub struct Plat;

#[cfg(not(test))]
impl PhysicsPlat for Plat {
    fn event_new(&mut self, slot: *mut *mut c_void, manual: bool) -> c_long {
        // SAFETY: the C++ object's event slot
        unsafe { ee_base::ts::ee_ts_event_new(slot.cast(), manual) }
    }
    fn event_free(&mut self, event: *mut c_void) {
        // SAFETY: an event the constructor made
        unsafe { ee_base::ts::ee_ts_event_free(event.cast()) }
    }
    fn event_set(&mut self, event: *mut c_void) {
        // SAFETY: a live event
        unsafe { ee_base::ts::ee_ts_event_set(event.cast()) };
    }
    fn event_clear(&mut self, event: *mut c_void) {
        // SAFETY: a live event
        unsafe { ee_base::ts::ee_ts_event_clear(event.cast()) };
    }
    fn event_wait(&mut self, event: *mut c_void, timeout: u32) -> c_long {
        // SAFETY: a live event
        unsafe { ee_base::ts::ee_ts_event_wait(event.cast(), timeout) }
    }
    fn section_new(&mut self, slot: *mut *mut c_void) {
        // SAFETY: the C++ object's lock slot
        unsafe { *slot = ee_base::ts::ee_ts_section_new().cast() };
    }
    fn section_enter(&mut self, section: *mut c_void) {
        // SAFETY: a live section
        unsafe { ee_base::ts::ee_ts_section_enter(section.cast()) }
    }
    fn section_leave(&mut self, section: *mut c_void) {
        // SAFETY: a live section
        unsafe { ee_base::ts::ee_ts_section_leave(section.cast()) }
    }
    fn section_free(&mut self, section: *mut c_void) {
        // SAFETY: the constructor's section
        unsafe { ee_base::ts::ee_ts_section_free(section.cast()) }
    }
    fn thread_new(&mut self, slot: *mut *mut c_void, parameter: usize) -> c_long {
        // SAFETY: the C++ object's thread slot; the thread's parameter is the object
        unsafe { ee_base::ts::ee_ts_thread_new(slot.cast(), Some(ee_rts_physics_main), parameter as *mut c_void, 0, false) }
    }
    fn thread_free(&mut self, thread: *mut c_void) {
        // SAFETY: the constructor's thread
        unsafe { ee_base::ts::ee_ts_thread_free(thread.cast()) };
    }
    fn thread_set_priority(&mut self, thread: *mut c_void, priority: i32) -> c_long {
        // SAFETY: a live thread
        unsafe { ee_base::ts::ee_ts_thread_set_priority(thread.cast(), priority) }
    }
    fn thread_set_name(&mut self, thread: *mut c_void, name: *const c_char) {
        // SAFETY: a live thread, a C string
        unsafe { ee_base::ts::ee_ts_thread_set_name(thread.cast(), name) }
    }
    fn thread_receive(
        &mut self,
        thread: *mut c_void,
        message: &mut c_ulong,
        first: &mut usize,
        second: &mut usize,
    ) -> c_long {
        let (mut a, mut b) = (core::ptr::null_mut::<c_void>(), core::ptr::null_mut::<c_void>());
        // SAFETY: a live thread
        let r = unsafe { ee_base::ts::ee_ts_thread_receive(thread.cast(), message, &mut a, &mut b) };
        *first = a as usize;
        *second = b as usize;
        r
    }
    fn timer_stop(&mut self, timer: *mut c_void) {
        // SAFETY: the C++ TMTimer's Rust timer
        unsafe { ee_base::time::ee_time_timer_stop(timer.cast()) };
    }
    fn timer_start(&mut self, timer: *mut c_void) -> c_long {
        // SAFETY: as timer_stop
        unsafe { ee_base::time::ee_time_timer_start(timer.cast()) }
    }
    fn timer_set_interval(&mut self, timer: *mut c_void, interval: c_ulong) {
        // SAFETY: as timer_stop
        unsafe { ee_base::time::ee_time_timer_set_interval(timer.cast(), interval as u32, 0) };
    }
    fn timer_set_type(&mut self, timer: *mut c_void, kind: u32) {
        // SAFETY: as timer_stop
        unsafe { ee_base::time::ee_time_timer_set_type(timer.cast(), kind) };
    }
    fn timer_set_event(&mut self, timer: *mut c_void, event: *mut c_void) {
        // SAFETY: as timer_stop; TMTimer::SetEvent hands the event's platform handle
        unsafe {
            let handle = if event.is_null() { core::ptr::null_mut() } else { ee_base::ts::ee_ts_event_handle(event.cast()) };
            ee_base::time::ee_time_timer_set_event(timer.cast(), handle.cast());
        }
    }
    fn fp_precision53(&mut self) {
        ee_base::util::ee_util_fp_set_precision(FP_PRECISION_53);
    }
    fn fp_check53(&mut self) {
        if ee_base::util::ee_util_fp_precision() != FP_PRECISION_53 {
            ee_base::util::ee_util_fp_set_precision(FP_PRECISION_53);
        }
        if ee_base::util::ee_util_fp_rounding() != 0 {
            ee_base::util::ee_util_fp_set_rounding(0);
        }
    }
    fn time(&mut self) -> c_ulong {
        ee_base::time::ee_time_get() as c_ulong
    }
    fn sleep(&mut self, milliseconds: u32) {
        ee_base::plat_core::time::PlatSleep(milliseconds);
    }
    fn sim_mode(&mut self) -> bool {
        ee_base::plat_core::sim::gPlatSimMode.get() != 0
    }
    fn sim_run(&mut self) -> bool {
        ee_base::plat_core::sim::gPlatSimRun.get() != 0
    }
    fn sim_after_tick(&mut self) {
        if let Some(f) = ee_base::plat_core::sim::gPlatSimAfterTick.get() {
            // SAFETY: the simulation driver's hook
            unsafe { f() };
        }
    }
    fn verify(&mut self, ok: bool, line: c_ulong, expression: &'static [u8]) {
        if !ok {
            let mut e = expression.to_vec();
            e.push(0);
            // SAFETY: C strings
            unsafe {
                ee_base::error::ee_error_verify(c"C:\\EEX\\RTS Engine\\Physics\\RTSPhysics.cpp".as_ptr(), line, e.as_ptr().cast())
            };
        }
    }
}

#[cfg(not(test))]
use ee_base::shim::ShimFfi as Ffi;

/// `RTSPhysics::RTSPhysics`.
///
/// # Safety
/// `physics` is the C++ object under construction (`RTSPhysics*`).
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_rts_physics_construct(
    physics: *mut c_void,
    graphics: *mut c_void,
    timer: *mut c_void,
    default_ticks_per_second: c_long,
    priority: c_long,
) {
    unsafe {
        construct(&mut Ffi, &mut Plat, physics as usize, graphics as usize, timer as usize, default_ticks_per_second, priority)
    }
}

/// `RTSPhysics::~RTSPhysics`.
///
/// # Safety
/// `physics` is the C++ object being destroyed.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_rts_physics_destruct(physics: *mut c_void) {
    unsafe { destruct(&mut Ffi, &mut Plat, physics as usize) }
}

/// `RTSPhysics::Main` (the thread's function).
///
/// # Safety
/// `physics` is the live `RTSPhysics*`.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_rts_physics_main(physics: *mut c_void) -> c_ulong {
    unsafe { main_loop(&mut Ffi, &mut Plat, physics as usize) }
}

/// `RTSPhysics::GetRandom(long, long)`.
///
/// # Safety
/// `physics` is the live `RTSPhysics*`.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_rts_physics_random_range(physics: *mut c_void, min: c_long, max: c_long) -> c_long {
    random_range(&mut Ffi, physics as usize, min, max)
}

/// `RTSPhysics::Load`.
///
/// # Safety
/// `physics` is the live `RTSPhysics*`, `file` an `FSFile*`.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_rts_physics_load(physics: *mut c_void, file: *mut c_void) -> c_long {
    unsafe { load(&mut Ffi, &mut Plat, physics as usize, file as usize) }
}

/// `RTSPhysics::Save`.
///
/// # Safety
/// As [`ee_rts_physics_load`].
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_rts_physics_save(physics: *mut c_void, file: *mut c_void) -> c_long {
    unsafe { save(&mut Ffi, physics as usize, file as usize) }
}

/// `RTSPhysics::SetSpeed`.
///
/// # Safety
/// `physics` is the live `RTSPhysics*`.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_rts_physics_set_speed(physics: *mut c_void, speed: f32) {
    unsafe { set_speed(&mut Ffi, &mut Plat, physics as usize, speed) }
}

/// `RTSPhysics::HandleMessage` (the base's: EEPhysics calls it for the messages it does not take).
///
/// # Safety
/// `physics` is the live `RTSPhysics*`.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_rts_physics_handle_message(physics: *mut c_void, message: c_ulong) {
    unsafe { handle_message(&mut Ffi, &mut Plat, physics as usize, message) }
}

#[cfg(test)]
mod tests {
    use super::*;
    use ee_base::shim::ShimFake;

    /// The platform's calls, logged into the same log as the C++ shims' (`plat:` lines).
    struct FakePlat<'a> {
        log: &'a std::cell::RefCell<Vec<String>>,
        waits: Vec<c_long>,
        receives: Vec<(c_long, c_ulong)>,
        times: Vec<c_ulong>,
        sim: bool,
    }
    impl FakePlat<'_> {
        fn note(&self, s: String) {
            self.log.borrow_mut().push(format!("plat:{s}"));
        }
    }
    impl PhysicsPlat for FakePlat<'_> {
        fn event_new(&mut self, slot: *mut *mut c_void, manual: bool) -> c_long {
            self.note(format!("event_new({manual})"));
            unsafe { *slot = 0x100 as *mut c_void };
            0
        }
        fn event_free(&mut self, _e: *mut c_void) {
            self.note("event_free".into());
        }
        fn event_set(&mut self, e: *mut c_void) {
            self.note(format!("event_set({:x})", e as usize));
        }
        fn event_clear(&mut self, _e: *mut c_void) {
            self.note("event_clear".into());
        }
        fn event_wait(&mut self, _e: *mut c_void, t: u32) -> c_long {
            self.note(format!("event_wait({t})"));
            if self.waits.is_empty() { 0 } else { self.waits.remove(0) }
        }
        fn section_new(&mut self, slot: *mut *mut c_void) {
            self.note("section_new".into());
            unsafe { *slot = 0x200 as *mut c_void };
        }
        fn section_enter(&mut self, _s: *mut c_void) {
            self.note("section_enter".into());
        }
        fn section_leave(&mut self, _s: *mut c_void) {
            self.note("section_leave".into());
        }
        fn section_free(&mut self, _s: *mut c_void) {
            self.note("section_free".into());
        }
        fn thread_new(&mut self, slot: *mut *mut c_void, _p: usize) -> c_long {
            self.note("thread_new".into());
            unsafe { *slot = 0x300 as *mut c_void };
            0
        }
        fn thread_free(&mut self, _t: *mut c_void) {
            self.note("thread_free".into());
        }
        fn thread_set_priority(&mut self, _t: *mut c_void, p: i32) -> c_long {
            self.note(format!("priority({p})"));
            0
        }
        fn thread_set_name(&mut self, _t: *mut c_void, _n: *const c_char) {
            self.note("name".into());
        }
        fn thread_receive(&mut self, _t: *mut c_void, m: &mut c_ulong, _a: &mut usize, _b: &mut usize) -> c_long {
            let (r, msg) = if self.receives.is_empty() { (1, 0) } else { self.receives.remove(0) };
            *m = msg;
            r
        }
        fn timer_stop(&mut self, _t: *mut c_void) {
            self.note("timer_stop".into());
        }
        fn timer_start(&mut self, _t: *mut c_void) -> c_long {
            self.note("timer_start".into());
            0
        }
        fn timer_set_interval(&mut self, _t: *mut c_void, i: c_ulong) {
            self.note(format!("timer_interval({i})"));
        }
        fn timer_set_type(&mut self, _t: *mut c_void, k: u32) {
            self.note(format!("timer_type({k})"));
        }
        fn timer_set_event(&mut self, _t: *mut c_void, _e: *mut c_void) {
            self.note("timer_event".into());
        }
        fn fp_precision53(&mut self) {}
        fn fp_check53(&mut self) {}
        fn time(&mut self) -> c_ulong {
            if self.times.is_empty() { 0 } else { self.times.remove(0) }
        }
        fn sleep(&mut self, ms: u32) {
            self.note(format!("sleep({ms})"));
        }
        fn sim_mode(&mut self) -> bool {
            self.sim
        }
        fn sim_run(&mut self) -> bool {
            true
        }
        fn sim_after_tick(&mut self) {
            self.note("sim_after_tick".into());
        }
        fn verify(&mut self, ok: bool, line: c_ulong, _e: &'static [u8]) {
            assert!(ok, "verify at {line}");
        }
    }

    fn record() -> Box<EeRtsPhysicsFields> {
        Box::new(EeRtsPhysicsFields {
            mL28: 0,
            mTimerRunning: false,
            mSpeed: 1.0,
            mTimerInterval: 1000,
            mTicksPerSecond: 1,
            mTicksRun: 0,
            mTicksOwed: 0,
            mDefaultTicksPerSecond: 0,
            mStopThread: false,
            mSlowPlayback: false,
        })
    }

    /// A fake whose `fields` is the record and whose slots are cells.
    fn fake(f: &mut EeRtsPhysicsFields, slots: &mut [*mut c_void; 6]) -> ShimFake {
        let mut c = ShimFake::default();
        c.answer("fields", &[f as *mut _ as i64; 64]);
        for (i, name) in ["thread_slot", "created_slot", "start_slot", "done_slot", "tick_slot", "lock_slot"]
            .into_iter()
            .enumerate()
        {
            c.answer(name, &[&mut slots[i] as *mut _ as i64; 64]);
        }
        c
    }

    #[test]
    fn construction_makes_the_events_timer_thread_and_lock_in_order() {
        let mut f = record();
        let mut slots = [core::ptr::null_mut(); 6];
        let mut c = fake(&mut f, &mut slots);
        let log = std::cell::RefCell::new(Vec::new());
        let mut p = FakePlat { log: &log, waits: vec![], receives: vec![], times: vec![], sim: false };
        unsafe { construct(&mut c, &mut p, 7, 8, 9, 15, 3) };
        assert_eq!(f.mDefaultTicksPerSecond, 15);
        assert!(f.mSlowPlayback);
        assert_eq!(f.mTimerInterval, 1000);
        assert_eq!(
            log.borrow().join(" "),
            "plat:event_new(true) plat:event_new(true) plat:event_new(true) plat:event_new(false) plat:timer_stop \
             plat:timer_type(1) plat:timer_event plat:timer_interval(1000) plat:thread_new plat:priority(3) \
             plat:event_wait(4294967295) plat:name plat:section_new"
        );
        assert!(c.calls().contains("set_graphics(7, 8)\nset_random_seed(7, 1)\nset_timer(7, 9)"));
    }

    #[test]
    fn load_stops_at_the_first_error_and_resets_the_timer() {
        let mut f = record();
        let mut slots = [core::ptr::null_mut(); 6];
        let mut c = fake(&mut f, &mut slots);
        c.out("read_ulong", 77).out("read_ulong", 5);
        c.answer("read_bool", &[0, 4]);
        c.out("read_bool", 1);
        c.out("read_float", 0);
        let log = std::cell::RefCell::new(Vec::new());
        let mut p = FakePlat { log: &log, waits: vec![], receives: vec![], times: vec![], sim: false };
        f.mTicksOwed = 9;
        let r = unsafe { load(&mut c, &mut p, 1, 2) };
        assert_eq!(r, 4, "the second bool's error");
        assert!(c.calls().contains("set_random_seed(1, 77)"));
        assert_eq!(f.mL28, 5);
        assert_eq!(f.mTicksOwed, 0);
        assert_eq!(c.count("read_bool"), 2, "the slow playback flag is not read after an error");
        assert_eq!(log.borrow().join(" "), "plat:timer_stop plat:timer_interval(0)");
        // an error stops the reads
        let mut g = record();
        let mut c2 = fake(&mut g, &mut slots);
        c2.answer("read_ulong", &[0, 3]);
        let r = unsafe { load(&mut c2, &mut p, 1, 2) };
        assert_eq!(r, 3);
        assert_eq!(c2.count("read_bool"), 0);
    }

    #[test]
    fn save_writes_ten_fields_and_stops_at_an_error() {
        let mut f = record();
        f.mL28 = 4;
        let mut slots = [core::ptr::null_mut(); 6];
        let mut c = fake(&mut f, &mut slots);
        c.answer("random_seed", &[99]);
        assert_eq!(unsafe { save(&mut c, 1, 2) }, 0);
        let writes: Vec<_> = c.log.iter().filter(|l| l.starts_with("write")).cloned().collect();
        assert_eq!(writes.len(), 10);
        assert_eq!(writes[0], "write_ulong(2, 99)");
        assert_eq!(writes[1], "write_ulong(2, 4)");
        assert_eq!(writes[3], "write_float(2, 1.0)");
        let mut c2 = fake(&mut f, &mut slots);
        c2.answer("write_bool", &[6]);
        assert_eq!(unsafe { save(&mut c2, 1, 2) }, 6);
        assert_eq!(c2.log.iter().filter(|l| l.starts_with("write")).count(), 3);
    }

    #[test]
    fn set_speed_and_messages() {
        let mut f = record();
        let mut slots = [core::ptr::null_mut(); 6];
        let mut c = fake(&mut f, &mut slots);
        let log = std::cell::RefCell::new(Vec::new());
        let mut p = FakePlat { log: &log, waits: vec![], receives: vec![], times: vec![], sim: false };
        unsafe { set_speed(&mut c, &mut p, 1, 0.25) };
        assert_eq!((f.mTimerInterval, f.mTicksPerSecond), (250, 4));
        unsafe { set_speed(&mut c, &mut p, 1, 0.25) };
        assert_eq!(c.count("update_timing"), 2);
        assert_eq!(log.borrow().iter().filter(|l| *l == "plat:timer_stop").count(), 1);
        f.mDefaultTicksPerSecond = 10;
        f.mTicksRun = 5;
        unsafe { handle_message(&mut c, &mut p, 1, 0x403) };
        assert_eq!((f.mSpeed, f.mTicksRun, f.mTimerRunning), (0.1, 0, false));
        unsafe { handle_message(&mut c, &mut p, 1, 0x404) };
        assert!(f.mTimerRunning);
        unsafe { handle_message(&mut c, &mut p, 1, 0x401) };
        assert!(f.mStopThread);
    }

    #[test]
    fn random_range_draws_once() {
        let mut z = ShimFake::default();
        assert_eq!(random_range(&mut z, 1, 3, 9), 3, "a draw of 0 is the minimum");
        assert_eq!(z.calls(), "random(1)");
    }

    #[test]
    fn the_loop_ticks_twice_at_most_and_stops_on_quit() {
        let mut f = record();
        f.mTimerRunning = true;
        f.mTimerInterval = 10;
        let mut slots = [core::ptr::null_mut(); 6];
        let mut c = fake(&mut f, &mut slots);
        c.answer("ee_physics", &[1, 1]);
        c.answer("ticks_to_process", &[5, 5, 5, 5]);
        c.answer("cts_processed", &[3]);
        c.answer("current_ct", &[4]);
        c.answer("render_now", &[0x55]);
        let log = std::cell::RefCell::new(Vec::new());
        // no message; the time 35 after 0 at an interval of 10 is 3 ticks: 2 owed late
        let mut p = FakePlat { log: &log, waits: vec![0], receives: vec![(1, 0)], times: vec![35, 40], sim: false };
        let fp = &mut *f as *mut EeRtsPhysicsFields;
        let mut pass = PassOnce { inner: c, f: fp };
        let n = unsafe { main_loop(&mut pass, &mut p, 1) };
        assert_eq!(n, 0);
        assert_eq!(pass.inner.count("tick"), 2, "two ticks at most per timer tick");
        assert_eq!(f.mTicksRun, 2);
        // 2 owed late + 1, two run: 1 left
        assert_eq!(f.mTicksOwed, 1);
        assert!(log.borrow().iter().any(|l| l == "plat:event_set(55)"));
    }

    /// The fake, with `unlock` ending the loop (sets the stop flag) so that one pass runs.
    struct PassOnce {
        inner: ShimFake,
        f: *mut EeRtsPhysicsFields,
    }
    macro_rules! forward {
        ($($name:ident($($a:ident: $t:ty),*) $(-> $r:ty)?;)*) => {
            $(fn $name(&mut self, $($a: $t),*) $(-> $r)? { self.inner.$name($($a),*) })*
        };
    }
    impl PhysicsCpp for PassOnce {
        forward! {
            fields(physics: usize) -> *mut EeRtsPhysicsFields;
            thread_slot(physics: usize) -> *mut *mut c_void;
            created_slot(physics: usize) -> *mut *mut c_void;
            start_slot(physics: usize) -> *mut *mut c_void;
            done_slot(physics: usize) -> *mut *mut c_void;
            tick_slot(physics: usize) -> *mut *mut c_void;
            lock_slot(physics: usize) -> *mut *mut c_void;
            set_graphics(physics: usize, graphics: usize);
            render_now(physics: usize) -> *mut c_void;
            set_timer(physics: usize, timer: usize);
            timer(physics: usize) -> *mut c_void;
            thread_name() -> *const c_char;
            random_seed(physics: usize) -> u32;
            set_random_seed(physics: usize, seed: u32);
            random(physics: usize) -> f32;
            messages_size(physics: usize) -> usize;
            messages_front(physics: usize) -> usize;
            messages_pop_front(physics: usize);
            message_dereference(message: usize);
            handle_message(physics: usize, message: c_ulong, first: usize, second: usize);
            lock(physics: usize);
            tick(physics: usize);
            idle(physics: usize);
            update_timing(physics: usize);
            ee_physics(physics: usize) -> usize;
            ticks_to_process(ee_physics: usize) -> c_ulong;
            sim_server() -> usize;
            sim_communications_tick(server: usize);
            after_tick_hook() -> Option<unsafe extern "C" fn()>;
            cts_processed() -> c_ulong;
            current_ct() -> c_ulong;
            debug_out(text: *const c_char);
            read_ulong(file: usize, out: ShimOut<c_ulong>) -> c_long;
            read_bool(file: usize, out: ShimOut<bool>) -> c_long;
            read_float(file: usize, out: ShimOut<f32>) -> c_long;
            write_ulong(file: usize, value: c_ulong) -> c_long;
            write_bool(file: usize, value: bool) -> c_long;
            write_float(file: usize, value: f32) -> c_long;
        }
        fn unlock(&mut self, physics: usize) {
            self.inner.unlock(physics);
            unsafe { (*self.f).mStopThread = true };
        }
    }
}
