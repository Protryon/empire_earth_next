//! The engine's clock, events and threads, called directly by the game's server and client (their hosts' `native`,
//! LLE Network Engine round 7): what NESynchronousCommunications::QueueHostCommon's Time, Event* and Thread* ops and
//! the server's and client's WaitAny, ThreadPriority and Sleep ops did (`TMGetTime`, `TSScheduler::CreateEvent` with
//! its manual reset, `TSEvent`, `TSScheduler::CreateThread`, `TSThread::SetName`/`SetPriority`, `PlatSleep`), all of
//! them Rust's (ee_time, ee_ts, the platform layer) behind their C ABI. A thread kind (`QHost::thread_new`) is the
//! entry function of that object's thread.

use std::ffi::{c_char, c_ulong, c_void, CString};

use crate::net::qhost::{Ev, Thr};

type ThreadFn = Option<unsafe extern "C" fn(*mut c_void) -> c_ulong>;

/// The engine's C ABI, imported
/// cbindgen:ignore
#[cfg(not(test))]
mod sys {
    use super::ThreadFn;
    use std::ffi::{c_char, c_long, c_void};
    extern "C" {
        pub fn ee_time_get() -> u32;
        pub fn ee_ts_event_new(out: *mut *mut c_void, manual: bool) -> c_long;
        pub fn ee_ts_event_free(event: *mut c_void);
        pub fn ee_ts_event_set(event: *const c_void) -> c_long;
        pub fn ee_ts_event_clear(event: *const c_void) -> c_long;
        pub fn ee_ts_event_wait(event: *const c_void, timeout: u32) -> c_long;
        pub fn ee_ts_event_wait_any(events: *const *const c_void, count: u32, timeout: u32) -> i32;
        pub fn ee_ts_thread_new(
            out: *mut *mut c_void,
            f: ThreadFn,
            p: *mut c_void,
            stack: u32,
            suspended: bool,
        ) -> c_long;
        pub fn ee_ts_thread_free(thread: *mut c_void) -> c_long;
        pub fn ee_ts_thread_set_priority(thread: *const c_void, priority: i32) -> c_long;
        pub fn ee_ts_thread_set_name(thread: *const c_void, name: *const c_char);
        pub fn PlatSleep(milliseconds: u32);
    }
}

/// Under `cargo test` the hosts are the goldens' worlds (never native): nothing calls these.
#[cfg(test)]
#[allow(non_snake_case)]
mod sys {
    use super::ThreadFn;
    use std::ffi::{c_char, c_long, c_void};
    pub unsafe fn ee_time_get() -> u32 {
        unreachable!()
    }
    pub unsafe fn ee_ts_event_new(_: *mut *mut c_void, _: bool) -> c_long {
        unreachable!()
    }
    pub unsafe fn ee_ts_event_free(_: *mut c_void) {
        unreachable!()
    }
    pub unsafe fn ee_ts_event_set(_: *const c_void) -> c_long {
        unreachable!()
    }
    pub unsafe fn ee_ts_event_clear(_: *const c_void) -> c_long {
        unreachable!()
    }
    pub unsafe fn ee_ts_event_wait(_: *const c_void, _: u32) -> c_long {
        unreachable!()
    }
    pub unsafe fn ee_ts_event_wait_any(_: *const *const c_void, _: u32, _: u32) -> i32 {
        unreachable!()
    }
    pub unsafe fn ee_ts_thread_new(_: *mut *mut c_void, _: ThreadFn, _: *mut c_void, _: u32, _: bool) -> c_long {
        unreachable!()
    }
    pub unsafe fn ee_ts_thread_free(_: *mut c_void) -> c_long {
        unreachable!()
    }
    pub unsafe fn ee_ts_thread_set_priority(_: *const c_void, _: i32) -> c_long {
        unreachable!()
    }
    pub unsafe fn ee_ts_thread_set_name(_: *const c_void, _: *const c_char) {
        unreachable!()
    }
    pub unsafe fn PlatSleep(_: u32) {
        unreachable!()
    }
}

use sys::*;

/// `PLAT_PRIORITY_TIME_CRITICAL` (platform/PlatThread.h)
const PRIORITY_TIME_CRITICAL: i32 = 15;

/// `TMGetTime()`
pub fn time() -> u32 {
    unsafe { ee_time_get() }
}

/// `TSScheduler::CreateEvent(out)` (manually reset): NULL when it failed
pub fn event_new() -> Ev {
    let mut e = std::ptr::null_mut();
    if unsafe { ee_ts_event_new(&mut e, true) } != 0 {
        return 0;
    }
    e as Ev
}

/// `TSScheduler::DestroyEvent`
pub fn event_free(e: Ev) {
    unsafe { ee_ts_event_free(e as *mut c_void) }
}

/// `Set()`
pub fn event_set(e: Ev) {
    unsafe { ee_ts_event_set(e as *const c_void) };
}

/// `Clear()`
pub fn event_clear(e: Ev) {
    unsafe { ee_ts_event_clear(e as *const c_void) };
}

/// `Wait(timeout)`
pub fn event_wait(e: Ev, timeout: u32) -> i32 {
    unsafe { ee_ts_event_wait(e as *const c_void, timeout) as i32 }
}

/// `TSEvent::WaitAny(events, count, timeout)` (ee_ts fails more than PLAT_WAIT_MAX itself)
pub fn wait_any(events: &[Ev], timeout: u32) -> i32 {
    unsafe { ee_ts_event_wait_any(events.as_ptr() as *const *const c_void, events.len() as u32, timeout) }
}

/// `SetPriority(PLAT_PRIORITY_TIME_CRITICAL)`
pub fn thread_priority(t: Thr) {
    unsafe { ee_ts_thread_set_priority(t as *const c_void, PRIORITY_TIME_CRITICAL) };
}

/// `PlatSleep(ms)`
pub fn sleep(ms: u32) {
    unsafe { PlatSleep(ms) }
}

/// `SetName(name)`
pub fn thread_name(t: Thr, name: &str) {
    let name = CString::new(name).unwrap_or_default();
    unsafe { ee_ts_thread_set_name(t as *const c_void, name.as_ptr() as *const c_char) }
}

/// `TSScheduler::DestroyThread`
pub fn thread_free(t: Thr) {
    unsafe { ee_ts_thread_free(t as *mut c_void) };
}

macro_rules! entries {
    ($($name:ident => $body:path as $t:ty),* $(,)?) => {
        $(unsafe extern "C" fn $name(p: *mut c_void) -> c_ulong {
            $body(p as $t) as c_ulong
        })*
    };
}

entries! {
    lag_thread => crate::net::ffi::ee_net_ll_thread as *const crate::net::ffi::EeNetLagLoss,
    playback_thread => crate::net::ffi::ee_net_rp_thread as *const crate::net::ffi::EeNetRecPlay,
    client_lag_thread => crate::net::ffi_client::ee_net_cl_lag_thread as *const c_void,
    client_playback_thread => crate::net::ffi_client::ee_net_cl_playback_thread as *const c_void,
    client_main_thread => crate::net::ffi_client::ee_net_cl_main_thread as *const crate::net::ffi_client::EeNetClient,
    client_receive_thread => crate::net::ffi_client::ee_net_cl_receive_thread as *const crate::net::ffi_client::EeNetClient,
    server_playback_thread => crate::net::ffi_server::ee_net_sv_playback_thread as *const c_void,
    server_main_thread => crate::net::ffi_server::ee_net_sv_main_thread as *const crate::net::ffi_server::EeNetServer,
    server_send_thread => crate::net::ffi_server::ee_net_sv_send_thread as *const crate::net::ffi_server::EeNetServer,
    server_xfer_thread => crate::net::ffi_server::ee_net_sv_xfer_thread as *const crate::net::ffi_server::EeNetServer,
}

/// The entry function of each thread kind (the queues' 0, 1; the client's 2..5; the server's 6..9), 0 out of range
const THREADS: [unsafe extern "C" fn(*mut c_void) -> c_ulong; 10] = [
    lag_thread,
    playback_thread,
    client_lag_thread,
    client_playback_thread,
    client_main_thread,
    client_receive_thread,
    server_playback_thread,
    server_main_thread,
    server_send_thread,
    server_xfer_thread,
];

/// `TSScheduler::CreateThread(out, the kind's entry, obj, 0, false)`
pub fn thread_new(kind: u32, obj: usize, out: &mut Thr) -> i32 {
    let f = THREADS[if (kind as usize) < THREADS.len() { kind as usize } else { 0 }];
    let mut t = std::ptr::null_mut();
    let r = unsafe { ee_ts_thread_new(&mut t, Some(f), obj as *mut c_void, 0, false) };
    *out = t as Thr;
    r as i32
}

#[cfg(test)]
mod tests {
    #[test]
    fn every_thread_kind_has_its_entry() {
        // the kinds the objects start (qhost/client/server's THREAD_* constants) are the table's indexes
        assert_eq!(super::THREADS.len(), 10);
        assert_eq!(crate::net::server::THREAD_SV_XFER, 9);
        assert_eq!(crate::net::server::THREAD_SV_PLAYBACK, 6);
    }
}
