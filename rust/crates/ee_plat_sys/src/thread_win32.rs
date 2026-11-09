//! PlatThread on Win32. A PlatMutex is a heap critical section (the harness casts a `PlatMutex*` to a
//! `CRITICAL_SECTION*`), a `PlatEvent*` is the event HANDLE itself (PlatTimerStartEvent relies on it), a `PlatThread*`
//! the thread HANDLE. The waits call WaitForSingleObject / WaitForMultipleObjects as the engine did: the harness's
//! time shim patches those imports (a PlatEventWait must stay WaitForSingleObject, not a multi-wait of one handle).
//! With the harness clock's factor set (platform/PlatSim.h PlatSimSetTimeFactor) a finite timeout is a multi-wait
//! with a high-resolution timer instead (ee_plat_core::time::sim_wait).

use crate::constants::*;
use crate::cstr::try_box;
use std::ffi::c_void;
use std::ptr::{null, null_mut};
use windows_sys::Win32::Foundation::{
    CloseHandle, DuplicateHandle, GetLastError, SetLastError, DUPLICATE_SAME_ACCESS, ERROR_INVALID_PARAMETER,
    ERROR_NOT_ENOUGH_MEMORY, HANDLE, S_FALSE, S_OK, WAIT_ABANDONED_0, WAIT_FAILED, WAIT_OBJECT_0, WAIT_TIMEOUT,
};
use windows_sys::Win32::System::Com::{CoInitialize, CoUninitialize};
use windows_sys::Win32::System::Threading::{
    CreateEventA, CreateThread, DeleteCriticalSection, EnterCriticalSection, GetCurrentProcess, GetCurrentThread,
    GetCurrentThreadId, GetExitCodeThread, GetThreadPriority, InitializeCriticalSection, LeaveCriticalSection,
    ResetEvent, ResumeThread, SetEvent, SetThreadPriority, SuspendThread, TerminateThread, TryEnterCriticalSection,
    WaitForMultipleObjects, WaitForSingleObject, CREATE_SUSPENDED, CRITICAL_SECTION, THREAD_ALL_ACCESS,
};

#[repr(C)]
pub struct PlatMutex {
    section: CRITICAL_SECTION,
}

/// Opaque: the event HANDLE.
pub struct PlatEvent {
    _opaque: u8,
}

/// Opaque: the thread HANDLE.
pub struct PlatThread {
    _opaque: u8,
}

pub type PlatThreadMain = unsafe extern "C" fn(parameter: *mut c_void) -> u32;

fn event_handle(event: *mut PlatEvent) -> HANDLE {
    event.cast()
}

fn thread_handle(thread: *mut PlatThread) -> HANDLE {
    thread.cast()
}

#[no_mangle]
pub unsafe extern "C" fn PlatGetLastError() -> u32 {
    GetLastError()
}

#[no_mangle]
pub unsafe extern "C" fn PlatCurrentThreadId() -> u32 {
    GetCurrentThreadId()
}

#[no_mangle]
pub unsafe extern "C" fn PlatMutexCreate() -> *mut PlatMutex {
    let mutex = try_box(std::mem::zeroed::<PlatMutex>());
    if !mutex.is_null() {
        InitializeCriticalSection(&raw mut (*mutex).section);
    }
    mutex
}

#[no_mangle]
pub unsafe extern "C" fn PlatMutexDestroy(mutex: *mut PlatMutex) {
    if mutex.is_null() {
        return;
    }
    DeleteCriticalSection(&raw mut (*mutex).section);
    drop(Box::from_raw(mutex));
}

#[no_mangle]
pub unsafe extern "C" fn PlatMutexLock(mutex: *mut PlatMutex) {
    EnterCriticalSection(&raw mut (*mutex).section);
}

#[no_mangle]
pub unsafe extern "C" fn PlatMutexUnlock(mutex: *mut PlatMutex) {
    LeaveCriticalSection(&raw mut (*mutex).section);
}

#[no_mangle]
pub unsafe extern "C" fn PlatMutexTryLock(mutex: *mut PlatMutex) -> i32 {
    (TryEnterCriticalSection(&raw mut (*mutex).section) != 0) as i32
}

#[no_mangle]
pub unsafe extern "C" fn PlatEventCreate(manual_reset: i32, initially_set: i32) -> *mut PlatEvent {
    CreateEventA(null(), (manual_reset != 0) as i32, (initially_set != 0) as i32, null()).cast()
}

#[no_mangle]
pub unsafe extern "C" fn PlatEventDestroy(event: *mut PlatEvent) {
    CloseHandle(event_handle(event));
}

#[no_mangle]
pub unsafe extern "C" fn PlatEventSet(event: *mut PlatEvent) -> i32 {
    (SetEvent(event_handle(event)) != 0) as i32
}

#[no_mangle]
pub unsafe extern "C" fn PlatEventReset(event: *mut PlatEvent) -> i32 {
    (ResetEvent(event_handle(event)) != 0) as i32
}

/// A wait's result as the header's numbers.
pub fn wait_result(result: u32, count: u32) -> i32 {
    if result == WAIT_TIMEOUT {
        return PLAT_WAIT_TIMEOUT;
    }
    if result == WAIT_FAILED {
        return PLAT_WAIT_FAILED;
    }
    if result.wrapping_sub(WAIT_OBJECT_0) < count {
        return result.wrapping_sub(WAIT_OBJECT_0) as i32;
    }
    if result.wrapping_sub(WAIT_ABANDONED_0) < count {
        return result.wrapping_sub(WAIT_ABANDONED_0) as i32; // mutexes only: an event is never abandoned
    }
    PLAT_WAIT_FAILED
}

#[no_mangle]
pub unsafe extern "C" fn PlatEventWait(event: *mut PlatEvent, timeout_ms: u32) -> i32 {
    if let Some(result) = sim_wait(&[event_handle(event)], timeout_ms) {
        return wait_result(result, 1);
    }
    wait_result(WaitForSingleObject(event_handle(event), timeout_ms), 1)
}

/// The harness clock (platform/PlatSim.h PlatSimSetTimeFactor): a finite timeout lasts 1/F of its real time. None
/// without a factor: the Win32 wait as always.
unsafe fn sim_wait(handles: &[HANDLE], timeout_ms: u32) -> Option<u32> {
    ee_plat_core::time::sim_wait(handles, timeout_ms)
}

#[no_mangle]
pub unsafe extern "C" fn PlatEventWaitAny(events: *const *mut PlatEvent, count: u32, timeout_ms: u32) -> i32 {
    if count == 0 || count > PLAT_WAIT_MAX {
        SetLastError(ERROR_INVALID_PARAMETER);
        return PLAT_WAIT_FAILED;
    }
    let mut handles: [HANDLE; PLAT_WAIT_MAX as usize] = [null_mut(); PLAT_WAIT_MAX as usize];
    for (i, slot) in handles.iter_mut().enumerate().take(count as usize) {
        *slot = event_handle(*events.add(i));
    }
    if let Some(result) = sim_wait(&handles[..count as usize], timeout_ms) {
        return wait_result(result, count);
    }
    wait_result(WaitForMultipleObjects(count, handles.as_ptr(), 0, timeout_ms), count)
}

struct ThreadStart {
    main: PlatThreadMain,
    parameter: *mut c_void,
}

/// The thread procedure: COM around the thread function, as the engine's TSThread::WIN32ThreadMain did.
unsafe extern "system" fn thread_proc(start: *mut c_void) -> u32 {
    let start = Box::from_raw(start.cast::<ThreadStart>());
    let ThreadStart { main, parameter } = *start;
    let initialize = CoInitialize(null());
    if initialize == S_OK || initialize == S_FALSE {
        let result = main(parameter);
        CoUninitialize();
        result
    } else {
        0xFFFF_FFFF
    }
}

#[no_mangle]
pub unsafe extern "C" fn PlatThreadCreate(
    main: PlatThreadMain,
    parameter: *mut c_void,
    stack_size: u32,
    suspended: i32,
    thread_id: *mut u32,
) -> *mut PlatThread {
    let start = try_box(ThreadStart { main, parameter });
    if start.is_null() {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return null_mut();
    }
    let mut id = 0u32;
    let flags = if suspended != 0 { CREATE_SUSPENDED } else { 0 };
    let handle = CreateThread(null(), stack_size as usize, Some(thread_proc), start.cast(), flags, &mut id);
    if handle.is_null() {
        let error = GetLastError();
        drop(Box::from_raw(start));
        SetLastError(error);
        return null_mut();
    }
    *thread_id = id;
    handle.cast()
}

#[no_mangle]
pub unsafe extern "C" fn PlatThreadOpenCurrent() -> *mut PlatThread {
    let mut thread: HANDLE = null_mut();
    if DuplicateHandle(
        GetCurrentProcess(),
        GetCurrentThread(),
        GetCurrentProcess(),
        &mut thread,
        THREAD_ALL_ACCESS,
        0,
        DUPLICATE_SAME_ACCESS,
    ) == 0
    {
        return null_mut();
    }
    thread.cast()
}

#[no_mangle]
pub unsafe extern "C" fn PlatThreadClose(thread: *mut PlatThread) {
    CloseHandle(thread_handle(thread));
}

#[no_mangle]
pub unsafe extern "C" fn PlatThreadGetExitCode(thread: *mut PlatThread, exit_code: *mut u32) -> i32 {
    let mut code = 0u32;
    if GetExitCodeThread(thread_handle(thread), &mut code) == 0 {
        return 0;
    }
    *exit_code = code;
    1
}

#[no_mangle]
pub unsafe extern "C" fn PlatThreadWait(thread: *mut PlatThread, timeout_ms: u32) -> i32 {
    if let Some(result) = sim_wait(&[thread_handle(thread)], timeout_ms) {
        return wait_result(result, 1);
    }
    wait_result(WaitForSingleObject(thread_handle(thread), timeout_ms), 1)
}

#[no_mangle]
pub unsafe extern "C" fn PlatThreadGetPriority(thread: *mut PlatThread) -> i32 {
    GetThreadPriority(thread_handle(thread))
}

#[no_mangle]
pub unsafe extern "C" fn PlatThreadSetPriority(thread: *mut PlatThread, priority: i32) -> i32 {
    (SetThreadPriority(thread_handle(thread), priority) != 0) as i32
}

#[no_mangle]
pub unsafe extern "C" fn PlatThreadSuspend(thread: *mut PlatThread) -> u32 {
    SuspendThread(thread_handle(thread))
}

#[no_mangle]
pub unsafe extern "C" fn PlatThreadResume(thread: *mut PlatThread) -> u32 {
    ResumeThread(thread_handle(thread))
}

#[no_mangle]
pub unsafe extern "C" fn PlatThreadTerminate(thread: *mut PlatThread) -> i32 {
    (TerminateThread(thread_handle(thread), 0) != 0) as i32
}
