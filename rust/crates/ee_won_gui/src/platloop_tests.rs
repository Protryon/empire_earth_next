//! platloop: the event pass against PlatWindowManager::GetNextEvent's statements (71c097464), and the out-parameter
//! calls over a manager with a host that answers "the UI thread".
use super::*;
use crate::platwm::{ee_won_plat_wm_free, ee_won_plat_wm_new, EeWonPlatWmOp};
use std::cell::RefCell;

/// GetNextEvent as it was written in C++ (the same scripted steps, logged):
/// ```text
/// uint32_t theTimeout = (uint32_t)this->Call(EeWonPlatWmCall::BeginPass);
/// uint32_t theStart = TickNow();
/// PlatInputEvent theEvent;
/// if (PlatWindowWaitEvent(&theEvent, theTimeout))
///     this->HandlePlatEvent(theEvent);   // if (ee_won_plat_wm_event(..) && mEventForward != NULL) mEventForward(e)
/// else {
///     uint32_t theWaited = TickNow() - theStart;
///     if (theWaited < theTimeout)
///         PlatSleep(theTimeout - theWaited);
/// }
/// this->Call(EeWonPlatWmCall::EndWait);
/// this->ProcessEvents();
/// this->PaintAndComposite();
/// ```
fn original(timeout: u32, ticks: [u32; 2], event: Option<u32>, consumed: bool, forward: bool) -> Vec<String> {
    let mut log = vec!["begin".to_string()];
    let start = ticks[0];
    log.push(format!("wait {timeout}"));
    if let Some(kind) = event {
        log.push(format!("handle {kind}"));
        if !consumed && forward {
            log.push(format!("forward {kind}"));
        }
    } else {
        let waited = ticks[1].wrapping_sub(start);
        if waited < timeout {
            log.push(format!("sleep {}", timeout - waited));
        }
    }
    log.extend(["end", "process", "paint"].map(String::from));
    log
}

fn ported(timeout: u32, ticks: [u32; 2], event: Option<u32>, consumed: bool, forward: bool) -> Vec<String> {
    let log = RefCell::new(Vec::new());
    let mut tick = 0;
    pass(
        || {
            log.borrow_mut().push("begin".to_string());
            timeout
        },
        || {
            tick += 1;
            ticks[tick - 1]
        },
        |e, t| {
            log.borrow_mut().push(format!("wait {t}"));
            if let Some(kind) = event {
                e.kind = kind;
            }
            event.is_some()
        },
        |e| {
            log.borrow_mut().push(format!("handle {}", e.kind));
            !consumed
        },
        |e| {
            if forward {
                log.borrow_mut().push(format!("forward {}", e.kind))
            }
        },
        |ms| log.borrow_mut().push(format!("sleep {ms}")),
        || log.borrow_mut().push("end".to_string()),
        || log.borrow_mut().push("process".to_string()),
        || log.borrow_mut().push("paint".to_string()),
    );
    log.into_inner()
}

#[test]
fn a_pass_runs_get_next_events_statements() {
    let mut cases = 0;
    for timeout in [0u32, 1, 17, 50] {
        for ticks in [[100u32, 100], [100, 110], [100, 117], [100, 150], [100, 400], [u32::MAX - 3, 5], [7, 3]] {
            for event in [None, Some(3u32), Some(9)] {
                for consumed in [false, true] {
                    for forward in [false, true] {
                        assert_eq!(
                            ported(timeout, ticks, event, consumed, forward),
                            original(timeout, ticks, event, consumed, forward),
                            "timeout {timeout} ticks {ticks:?} event {event:?} consumed {consumed} forward {forward}"
                        );
                        cases += 1;
                    }
                }
            }
        }
    }
    assert_eq!(cases, 4 * 7 * 3 * 4);
    // the sleep is the rest of the timeout, never more; a clock that went back sleeps nothing
    assert_eq!(ported(50, [100, 117], None, false, false)[2], "sleep 33");
    assert!(!ported(50, [7, 3], None, false, false).iter().any(|l| l.starts_with("sleep")));
}

unsafe extern "C" fn ui_host(_me: *mut c_void, op: EeWonPlatWmOp, _a: isize, _b: isize, _c: isize, _d: isize) -> isize {
    isize::from(op == EeWonPlatWmOp::IsUiThread)
}

thread_local! {
    static LOOP: RefCell<Vec<String>> = const { RefCell::new(Vec::new()) };
}
unsafe extern "C" fn no_event(_out: *mut c_void, timeout: u32) -> i32 {
    LOOP.with(|l| l.borrow_mut().push(format!("wait {}", timeout.min(50))));
    0
}
unsafe extern "C" fn clock() -> u64 {
    (1u64 << 32) + 5
}
unsafe extern "C" fn nap(ms: u32) {
    LOOP.with(|l| l.borrow_mut().push(format!("sleep {}", ms.min(50))));
}

#[test]
fn the_sizes_the_screen_and_the_frame() {
    let m = ee_won_plat_wm_new(std::ptr::null_mut(), Some(ui_host));
    let (mut w, mut h, mut r) = (-1, -1, [9i32; 4]);
    // SAFETY: a live manager and writable outs
    unsafe {
        ee_won_plat_wm(m, C::SetSurfaceSize, 640, 480);
        ee_won_plat_wm_surface_size(m, &mut w, &mut h);
        ee_won_plat_wm_screen_rect(m, r.as_mut_ptr());
    }
    assert_eq!((w, h), (640, 480));
    assert_eq!(r, [0, 0, 640, 480]);
    // no frame composited yet: nothing written, nothing held
    let (mut px, mut v, mut serial) = (std::ptr::null(), [-7i32; 5], 77u32);
    // SAFETY: as above
    let locked = unsafe {
        let [a, b, c, d, e] = &mut v;
        ee_won_plat_wm_lock_frame_out(m, &mut px, a, b, c, d, e, &mut serial)
    };
    assert!(!locked);
    assert_eq!((px, v, serial), (std::ptr::null(), [-7; 5], 77));
    // a pass without a window system: the wait, the rest of the timeout slept, a frame composited
    let fns = EeWonPlatLoopFns { wait: no_event, now: clock, sleep: nap };
    let wm = crate::wmobj::ee_won_wm_new(std::ptr::null_mut(), None);
    // SAFETY: both objects live
    unsafe { ee_won_plat_wm_next_event(m, &fns, wm, None) };
    let log = LOOP.with(|l| l.borrow().clone());
    assert_eq!(log.len(), 2, "{log:?}");
    assert!(log[0].starts_with("wait ") && log[1].starts_with("sleep "), "{log:?}");
    // SAFETY: as above
    let locked = unsafe {
        let [a, b, c, d, e] = &mut v;
        ee_won_plat_wm_lock_frame_out(m, &mut px, a, b, c, d, e, &mut serial)
    };
    if locked {
        assert!(!px.is_null());
        assert_eq!((v[0], v[1], v[2], v[3], v[4]), (640, 480, 640 * 4, 640, 480));
        assert_ne!(serial, 77);
        // SAFETY: held
        unsafe { ee_won_plat_wm(m, C::UnlockFrame, 0, 0) };
    }
    // SAFETY: from the constructors
    unsafe {
        crate::wmobj::ee_won_wm_free(wm);
        ee_won_plat_wm_free(m);
    }
}

#[test]
fn the_classic_colours_fill_the_first_fifteen() {
    let mut c = [0xdead_beefu32; 17];
    // SAFETY: 17 colours
    unsafe { ee_won_plat_wm_classic_colors(c.as_mut_ptr()) };
    assert_eq!(&c[..15], &CLASSIC[..]);
    assert_eq!(&c[15..], &[0xdead_beef; 2]);
    // the original's loop: `for (i = 0; Call(Classic, i) >= 0; i++) SetStandardColor(i, Call(Classic, i))`
    let mut o = [0xdead_beefu32; 17];
    let mut i = 0;
    // SAFETY: Classic reads no manager
    while unsafe { ee_won_plat_wm(std::ptr::null(), C::Classic, i, 0) } >= 0 {
        // SAFETY: as above
        o[i as usize] = unsafe { ee_won_plat_wm(std::ptr::null(), C::Classic, i, 0) } as u32;
        i += 1;
    }
    assert_eq!(c, o);
}
