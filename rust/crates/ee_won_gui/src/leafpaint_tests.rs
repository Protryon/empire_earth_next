use super::*;
use std::cell::RefCell;

#[derive(Default)]
struct World {
    images: [Option<(i32, i32)>; 5],
    disabled: bool,
    checked: bool,
    pressed: bool,
    stretch: bool,
    size: (i32, i32),
    log: Vec<String>,
}

thread_local! {
    static W: RefCell<World> = RefCell::new(World::default());
}

unsafe extern "C" fn host(_me: *mut c_void, op: EeWonLeafOp, a: isize, b: isize) -> isize {
    use EeWonLeafOp::*;
    W.with(|w| {
        let mut w = w.borrow_mut();
        let img = |i: isize| w.images[i as usize];
        match op {
            BtnHasImage => img(a).is_some() as isize,
            BtnImageWidth => img(a).unwrap().0 as isize,
            BtnImageHeight => img(a).unwrap().1 as isize,
            BtnDisabled => w.disabled as isize,
            BtnChecked => w.checked as isize,
            BtnPressed => w.pressed as isize,
            BtnStretch => w.stretch as isize,
            BtnWidth => w.size.0 as isize,
            BtnHeight => w.size.1 as isize,
            BtnMouseOver => 0,
            _ => {
                w.log.push(format!("{op:?} {a} {b}"));
                0
            }
        }
    })
}

fn log() -> Vec<String> {
    W.with(|w| std::mem::take(&mut w.borrow_mut().log))
}

#[test]
fn the_desired_size_is_the_largest_state_image_and_the_paint_falls_back_to_the_normal_image() {
    W.with(|w| {
        let mut w = w.borrow_mut();
        w.images = [Some((10, 30)), None, Some((25, 5)), None, Some((4, 4))];
        w.size = (10, 30);
    });
    let b = EeWonLeaf::new(std::ptr::null_mut(), Some(host));
    assert_eq!(b.image_button_size(), (25, 30));
    W.with(|w| w.borrow_mut().disabled = true);
    b.image_button_paint(1);
    // no disabled image: the normal one, at the button's size already
    assert_eq!(log(), ["BtnBasePaint 1 0", "BtnDrawImage 1 0"]);
    W.with(|w| {
        let mut w = w.borrow_mut();
        w.disabled = false;
        w.checked = true;
        w.stretch = true;
    });
    b.image_button_paint(1);
    assert_eq!(log(), ["BtnBasePaint 1 0", "BtnImageFit 4 0", "BtnDrawImage 1 4"]);
    W.with(|w| {
        let mut w = w.borrow_mut();
        w.checked = false;
        w.pressed = true;
        w.images[0] = None;
    });
    b.image_button_paint(1);
    assert_eq!(log(), ["BtnBasePaint 1 0", "BtnImageFit 2 0", "BtnDrawImage 1 2"]);
    W.with(|w| w.borrow_mut().pressed = false);
    b.image_button_paint(1);
    assert_eq!(log(), ["BtnBasePaint 1 0"]);
}
