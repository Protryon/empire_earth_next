//! The platform manager's event pass and its out-parameter calls (R6 WON GUI round 35).
//!
//! `PlatWindowManager::GetNextEvent`, `HandlePlatEvent`, `LockFrame`, `GetScreenResolutionHook`, `GetScreenRect` and
//! `GetSystemColorScheme` were C++ statements over [`crate::platwm`]'s calls; with the class folded into
//! WindowManager they are composed here. The platform's wait, clock and sleep come as function pointers (the
//! platform layer's `PlatWindowWaitEvent`, `PlatTimeMilliseconds64`, `PlatSleep`), so the tests script them.
use crate::platwm::{
    ee_won_plat_wm, ee_won_plat_wm_event, ee_won_plat_wm_lock_frame, EeWonPlatEvent, EeWonPlatWm, EeWonPlatWmCall as C,
    CLASSIC,
};
use crate::wmobj::{ee_won_wm_call, EeWonWm, EeWonWmCall};
use std::ffi::c_void;

/// The platform's calls of one event pass.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeWonPlatLoopFns {
    /// `PlatWindowWaitEvent(out, timeout)`: nonzero with an event.
    pub wait: unsafe extern "C" fn(out: *mut c_void, timeout_ms: u32) -> i32,
    /// `PlatTimeMilliseconds64()`.
    pub now: unsafe extern "C" fn() -> u64,
    /// `PlatSleep(ms)`.
    pub sleep: unsafe extern "C" fn(ms: u32),
}

/// The game's forward function (`SetEventForward`): the platform events the lobby does not consume.
pub type EeWonPlatForward = Option<unsafe extern "C" fn(event: *const c_void)>;

/// One pass, its steps given: the wait's timeout from `begin`; an event handled (`handle`: true when it goes on to
/// `forward`), or, with none (no window system, or a spurious return), the rest of the timeout slept so the loop
/// never spins; then `end`, the queued events (`process`) and the paint.
#[allow(clippy::too_many_arguments)]
pub fn pass(
    begin: impl FnOnce() -> u32,
    mut now: impl FnMut() -> u32,
    wait: impl FnOnce(&mut EeWonPlatEvent, u32) -> bool,
    handle: impl FnOnce(&EeWonPlatEvent) -> bool,
    forward: impl FnOnce(&EeWonPlatEvent),
    sleep: impl FnOnce(u32),
    end: impl FnOnce(),
    process: impl FnOnce(),
    paint: impl FnOnce(),
) {
    let timeout = begin();
    let start = now();
    let mut event = EeWonPlatEvent { kind: 0, time: 0, window: std::ptr::null_mut(), data: [0; 18] };
    if wait(&mut event, timeout) {
        if handle(&event) {
            forward(&event);
        }
    } else {
        let waited = now().wrapping_sub(start);
        if waited < timeout {
            sleep(timeout - waited);
        }
    }
    end();
    process();
    paint();
}

/// `GetNextEvent()`: waits for one platform event (GetMessage; at most until 50 ms after the last pass, the
/// original's window timer), handles it, runs the manager's ProcessEvents (the window procedure did after each
/// message), then paints and composites. `wm` is the manager's Rust object (wmobj.rs), `forward` the game's.
///
/// # Safety
/// `m` and `wm` are the live manager's two objects; `fns` is live; the calls out may re-enter both.
#[no_mangle]
pub unsafe extern "C" fn ee_won_plat_wm_next_event(
    m: *const EeWonPlatWm,
    fns: *const EeWonPlatLoopFns,
    wm: *const EeWonWm,
    forward: EeWonPlatForward,
) {
    // SAFETY: live (the contract); copied: no reference lives across a call out
    let f = unsafe { *fns };
    // SAFETY: the contract, for each call out
    unsafe {
        pass(
            || ee_won_plat_wm(m, C::BeginPass, 0, 0) as u32,
            || (f.now)() as u32,
            |e, t| (f.wait)(e as *mut EeWonPlatEvent as *mut c_void, t) != 0,
            |e| ee_won_plat_wm_event(m, e),
            |e| {
                if let Some(g) = forward {
                    g(e as *const EeWonPlatEvent as *const c_void)
                }
            },
            |ms| (f.sleep)(ms),
            || {
                ee_won_plat_wm(m, C::EndWait, 0, 0);
            },
            || {
                ee_won_wm_call(wm, EeWonWmCall::ProcessEvents, 0, 0, 0, 0);
            },
            || {
                ee_won_plat_wm(m, C::PaintAndComposite, 0, 0);
            },
        )
    }
}

/// `LockFrame(outPixels, outWidth, outHeight, outPitch, outLogicalWidth, outLogicalHeight, outSerial)`: nothing
/// written without a frame.
///
/// # Safety
/// `m` is a live manager; the seven outs are writable.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_plat_wm_lock_frame_out(
    m: *const EeWonPlatWm,
    pixels: *mut *const u32,
    width: *mut i32,
    height: *mut i32,
    pitch: *mut i32,
    logical_width: *mut i32,
    logical_height: *mut i32,
    serial: *mut u32,
) -> bool {
    let mut v = [0u32; 6];
    // SAFETY: the contract; six ints
    if !unsafe { ee_won_plat_wm_lock_frame(m, pixels, v.as_mut_ptr()) } {
        return false;
    }
    // SAFETY: the contract
    unsafe {
        *width = v[0] as i32;
        *height = v[1] as i32;
        *pitch = v[2] as i32;
        *logical_width = v[3] as i32;
        *logical_height = v[4] as i32;
        *serial = v[5];
    }
    true
}

/// `GetScreenResolutionHook(outWidth, outHeight)`: the surface's size.
///
/// # Safety
/// `m` is a live manager; the outs are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_plat_wm_surface_size(m: *const EeWonPlatWm, width: *mut i32, height: *mut i32) {
    // SAFETY: the contract
    unsafe {
        *width = ee_won_plat_wm(m, C::SurfaceWidth, 0, 0) as i32;
        *height = ee_won_plat_wm(m, C::SurfaceHeight, 0, 0) as i32;
    }
}

/// `GetScreenRect(outRect)`: the lobby surface is the screen (x, y, width, height at `rect`).
///
/// # Safety
/// `m` is a live manager; `rect` holds four ints.
#[no_mangle]
pub unsafe extern "C" fn ee_won_plat_wm_screen_rect(m: *const EeWonPlatWm, rect: *mut i32) {
    // SAFETY: the contract
    unsafe {
        *rect = 0;
        *rect.add(1) = 0;
        *rect.add(2) = ee_won_plat_wm(m, C::SurfaceWidth, 0, 0) as i32;
        *rect.add(3) = ee_won_plat_wm(m, C::SurfaceHeight, 0, 0) as i32;
    }
}

/// `GetSystemColorScheme()`: the scheme's first standard colours from the classic Windows scheme (`CLASSIC`); the
/// others stay.
///
/// # Safety
/// `colors` holds the scheme's 17 colours.
#[no_mangle]
pub unsafe extern "C" fn ee_won_plat_wm_classic_colors(colors: *mut u32) {
    for (i, &c) in CLASSIC.iter().enumerate() {
        // SAFETY: 15 of the 17 (the contract)
        unsafe { *colors.add(i) = c };
    }
}

#[cfg(test)]
#[path = "platloop_tests.rs"]
mod tests;
