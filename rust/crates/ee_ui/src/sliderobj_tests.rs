//! The slider's golden (`golden_sliderobj.txt`): the first sequences the round-10 driver (`build/ui10`) printed over
//! the unmodified UISlider.cpp (a plain slider or the elevation tab's ChangeRange override, trigger events reading
//! the value back), replayed op for op against the Rust object.

use super::*;
use crate::obj::replay::Replay;

const GOLDEN: &str = include_str!("golden_sliderobj.txt");

unsafe fn dispatch(target: *const c_void, call: u32, a: isize, b: isize, f: *mut f32) -> isize {
    // SAFETY: the call numbers are EeUiSliderCall's (the golden's); the target is the live slider
    unsafe {
        ee_ui_slider_call(target as *const EeUiSlider, core::mem::transmute::<u32, EeUiSliderCall>(call), a, b, f)
    }
}

fn replay(golden: &str) -> usize {
    let r = Replay::new(golden, dispatch);
    let mut obj: *mut EeUiSlider = core::ptr::null_mut();
    r.run(|r, w| match w[0] {
        "new" => {
            let (o, h) = r.host();
            obj = ee_ui_slider_new(o, h, w[1].parse().unwrap());
            r.target.set(obj as *const c_void);
            true
        }
        "free" => {
            // SAFETY: made by "new"
            unsafe { ee_ui_slider_free(obj) };
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
    // the first ThumbDraw's answer changed: the DrawPolygons call's result no longer matches its ret line
    let at = GOLDEN.find("\nop 26 ").expect("a thumb draw");
    let eq = at + 1 + GOLDEN[at + 1..].find("\n= ").unwrap() + 1;
    let rest = &GOLDEN[eq + 2..];
    let g = format!("{}= 7{}", &GOLDEN[..eq], &rest[rest.find(' ').unwrap()..]);
    replay(&g);
}

/// An ad-hoc ChangeRange override in the host (the elevation tab's): the value read back before the base runs.
#[test]
fn the_virtual_change_range_reenters() {
    use core::cell::RefCell;
    thread_local!(static LOG: RefCell<Vec<String>> = const { RefCell::new(Vec::new()) });
    static mut OBJ: *const EeUiSlider = core::ptr::null();
    unsafe extern "C" fn h(_: *mut c_void, op: Op, a: isize, _: isize, _: isize, _: isize, f: *mut f32) -> isize {
        LOG.with(|l| l.borrow_mut().push(format!("{op:?} {a}")));
        match op {
            // SAFETY: four floats
            Op::Rect => unsafe { write_buf(f, [20.0, 10.0, 110.0, 0.0]) },
            Op::SliderChangeRange => {
                // SAFETY: the slider is live while it calls out
                let v = unsafe { ee_ui_slider_call(OBJ, EeUiSliderCall::CurrentRangeValue, 0, 0, f) };
                LOG.with(|l| l.borrow_mut().push(format!("override read {v}")));
                // SAFETY: as above
                unsafe { ee_ui_slider_call(OBJ, EeUiSliderCall::ChangeRange, a, 0, f) };
            }
            _ => {}
        }
        0
    }
    let s = ee_ui_slider_new(core::ptr::null_mut(), Some(h), 0);
    // SAFETY: the test's own slider
    unsafe {
        OBJ = s;
        let mut f = [0.0f32; 4];
        ee_ui_slider_call(s, EeUiSliderCall::SetRange, 0, 50, f.as_mut_ptr());
        ee_ui_slider_call(s, EeUiSliderCall::SetCurrentRangeValue, 30, 0, f.as_mut_ptr());
        assert_eq!(ee_ui_slider_call(s, EeUiSliderCall::CurrentRangeValue, 0, 0, f.as_mut_ptr()), 30);
        ee_ui_slider_free(s);
    }
    let log = LOG.with(|l| l.borrow().clone());
    assert!(log.contains(&"override read 0".to_string()), "{log:?}");
    assert!(log.contains(&"FireTrigger 1000".to_string()), "{log:?}");
    assert_eq!(log.last().unwrap(), "BitmapRelease 0");
}
