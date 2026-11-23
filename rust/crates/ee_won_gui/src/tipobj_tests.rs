use super::*;
use std::cell::RefCell;

#[derive(Default)]
struct World {
    item: isize,
    parent: bool,
    own_scheme: bool,
    log: Vec<String>,
}

thread_local! {
    static W: RefCell<World> = RefCell::new(World::default());
}

unsafe extern "C" fn host(_me: *mut c_void, op: EeWonTipOp, a: isize, b: isize) -> isize {
    use EeWonTipOp::*;
    W.with(|w| {
        let mut w = w.borrow_mut();
        match op {
            ItemIs => (w.item == a) as isize,
            HasItem => (w.item != 0) as isize,
            HasParent => w.parent as isize,
            SetItem => {
                w.item = a;
                0
            }
            ItemTop => 30,
            ItemWidth => 40,
            ItemHeight => 12,
            HasOwnScheme => w.own_scheme as isize,
            ListScheme => 5,
            Scheme => 9,
            ListLeftPad | ListRightPad | ListTopPad => 2,
            ListVertOffset => 10,
            ListWidth => 100,
            Width => 100,
            Height => 16,
            ListLocalToRoot => {
                // SAFETY: two ints at a
                let p = unsafe { std::slice::from_raw_parts_mut(a as *mut i32, 2) };
                p[0] += 7;
                p[1] += 50;
                0
            }
            Draw => {
                // SAFETY: one op at b
                let o = unsafe { *(b as *const EeWonDrawOp) };
                w.log.push(format!("draw {} {} {} {} {}", o.kind, o.a, o.b, o.c, o.d));
                0
            }
            OpenPopup => {
                w.parent = true;
                w.log.push(format!("{op:?} {a} {b}"));
                0
            }
            ClosePopup => {
                w.parent = false;
                w.log.push(format!("{op:?}"));
                0
            }
            _ => {
                w.log.push(format!("{op:?} {b}"));
                0
            }
        }
    })
}

fn log() -> Vec<String> {
    W.with(|w| std::mem::take(&mut w.borrow_mut().log))
}

#[test]
fn a_tip_opens_over_its_item_at_least_as_wide_as_the_list_and_the_same_item_stays() {
    let t = EeWonTip::new(std::ptr::null_mut(), Some(host));
    t.do_tip(77);
    // y: top pad 2 - offset 10 = -8, +50 at the root, +30 - 2: 70; width max(40 + 2 + 2, 100)
    assert_eq!(log(), ["SetSize 16", "OpenPopup 7 70"]);
    t.do_tip(77);
    assert!(log().is_empty());
    t.do_tip(0);
    assert_eq!(log(), ["ClosePopup"]);
}

#[test]
fn the_paint_draws_the_item_through_the_list_in_its_scheme_and_puts_the_scheme_back() {
    let t = EeWonTip::new(std::ptr::null_mut(), Some(host));
    W.with(|w| w.borrow_mut().item = 3);
    t.paint(1);
    let k = |c: i32, a, b, cc, d| format!("draw {c} {a} {b} {cc} {d}");
    assert_eq!(
        log(),
        [
            "BasePaint 0".to_string(),
            "SetScheme 5".into(),
            k(EE_WON_DRAW_SET_SYS_COLOR, EE_WON_TIP_COLOR_BACK, 0, 0, 0),
            k(EE_WON_DRAW_FILL_RECT, 0, 0, 100, 16),
            k(EE_WON_DRAW_PUSH_CLIP, 0, 2, 100, 12),
            k(EE_WON_DRAW_TRANSLATE, 2, -28, 0, 0),
            "ScrollPaint 0".into(),
            k(EE_WON_DRAW_TRANSLATE, -2, 28, 0, 0),
            k(EE_WON_DRAW_POP_CLIP, 0, 0, 0, 0),
            k(EE_WON_DRAW_SET_SYS_COLOR, EE_WON_TIP_COLOR_TEXT, 0, 0, 0),
            k(EE_WON_DRAW_RECT, 0, 0, 100, 16),
            "SetScheme 9".into(),
        ]
    );
}
