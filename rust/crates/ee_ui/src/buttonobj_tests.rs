//! The button's golden (`golden_buttonobj.txt`): the first sequences the round-11 driver (`build/ui11`) printed over
//! the unmodified UIButton.cpp and UIDualButton.cpp (a plain button, or a subclass whose virtuals read the state
//! before the base; trigger events reading the button back), replayed op for op against the Rust object.

use super::*;
use crate::obj::replay::Replay;

const GOLDEN: &str = include_str!("golden_buttonobj.txt");

unsafe fn dispatch(target: *const c_void, call: u32, a: isize, b: isize, f: *mut f32) -> isize {
    // SAFETY: the call numbers are EeUiButtonCall's (the golden's); the target is the live button
    unsafe {
        ee_ui_button_call(target as *const EeUiButton, core::mem::transmute::<u32, EeUiButtonCall>(call), a, b, f)
    }
}

fn replay(golden: &str) -> usize {
    let r = Replay::new(golden, dispatch);
    let mut obj: *mut EeUiButton = core::ptr::null_mut();
    r.run(|r, w| match w[0] {
        "new" => {
            let (o, h) = r.host();
            let p = |i: usize| w[i].parse::<isize>().unwrap() as *mut c_void;
            obj = ee_ui_button_new(o, h, p(1), p(2), p(3));
            r.target.set(obj as *const c_void);
            true
        }
        "free" => {
            // SAFETY: made by "new"
            unsafe { ee_ui_button_free(obj) };
            obj = core::ptr::null_mut();
            true
        }
        _ => false,
    });
    r.checked.get()
}

#[test]
fn golden_replays() {
    assert!(replay(GOLDEN) > 1_000);
}

#[test]
#[should_panic(expected = "golden line")]
fn a_changed_answer_fails() {
    // the first bitmap draw's answer changed: the DrawPolygons call's result no longer matches its ret line
    let at = GOLDEN.find("\nop 26 ").expect("a bitmap draw");
    let eq = at + 1 + GOLDEN[at + 1..].find("\n= ").unwrap() + 1;
    let rest = &GOLDEN[eq + 2..];
    let g = format!("{}= 7{}", &GOLDEN[..eq], &rest[rest.find(' ').unwrap()..]);
    replay(&g);
}

/// A subclass's `ButtonUp` override in the host: it calls back into the button (the state read, the base run) while
/// the base's state machine is between ops, and the button-up sound plays once.
#[test]
fn a_virtual_button_up_reenters() {
    use core::cell::RefCell;
    thread_local!(static LOG: RefCell<Vec<String>> = const { RefCell::new(Vec::new()) });
    static mut OBJ: *const EeUiButton = core::ptr::null();
    unsafe extern "C" fn h(_: *mut c_void, op: Op, a: isize, _: isize, _: isize, _: isize, f: *mut f32) -> isize {
        LOG.with(|l| l.borrow_mut().push(format!("{op:?} {a}")));
        match op {
            // SAFETY: four floats
            Op::Rect => unsafe { write_buf(f, [20.0, 10.0, 110.0, 0.0]) },
            // SAFETY: four floats
            Op::MouseLocation => unsafe { write_buf(f, [15.0, 5.0, 0.0, 0.0]) },
            Op::SoundLoad => return 7,
            Op::ButtonUp => {
                // SAFETY: the button is live while it calls out
                let s = unsafe { ee_ui_button_call(OBJ, EeUiButtonCall::State, 0, 0, f) };
                LOG.with(|l| l.borrow_mut().push(format!("override read {s}")));
                // SAFETY: as above
                unsafe { ee_ui_button_call(OBJ, EeUiButtonCall::ButtonUp, 0, 0, f) };
            }
            _ => {}
        }
        0
    }
    let null = core::ptr::null_mut();
    let b = ee_ui_button_new(null, Some(h), null, null, null);
    // SAFETY: the test's own button
    unsafe {
        OBJ = b;
        let mut f = [0.0f32; 4];
        ee_ui_button_call(b, EeUiButtonCall::SetButtonUpSound, 0x99, 0, f.as_mut_ptr());
        ee_ui_button_call(b, EeUiButtonCall::MouseLButtonDown, 0, 0, f.as_mut_ptr());
        assert_eq!(ee_ui_button_call(b, EeUiButtonCall::IsButtonDown, 0, 0, f.as_mut_ptr()), 1);
        ee_ui_button_call(b, EeUiButtonCall::MouseLButtonUp, 0, 0, f.as_mut_ptr());
        assert_eq!(ee_ui_button_call(b, EeUiButtonCall::IsButtonDown, 0, 0, f.as_mut_ptr()), 0);
        ee_ui_button_free(b);
    }
    let log = LOG.with(|l| l.borrow().clone());
    assert!(log.contains(&"override read 0".to_string()), "{log:?}");
    assert_eq!(log.iter().filter(|l| *l == "SoundPlay 7").count(), 1, "{log:?}");
    assert!(log.contains(&"FireTrigger 1008".to_string()), "{log:?}");
    assert_eq!(log.last().unwrap(), "SoundUnload 7");
}
