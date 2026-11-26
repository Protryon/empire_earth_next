use super::*;
use crate::guidraw::EE_WON_DRAW_CALL;
use std::cell::RefCell;

thread_local! {
    static LOG: RefCell<Vec<String>> = const { RefCell::new(Vec::new()) };
}

unsafe extern "C" fn log_host(g: *mut c_void, h: H, a: *const i32, p: *const c_void) -> u32 {
    // SAFETY: seven ints
    let a = unsafe { std::slice::from_raw_parts(a, 7) };
    LOG.with(|l| l.borrow_mut().push(format!("{:?} {} {} {} {:x}", h, a[0], a[1], a[2], p as usize)));
    let _ = g;
    0
}

/// A ColorScheme as the C++ lays it out: a vtable, the count, the 17 colours.
#[repr(C)]
struct Scheme {
    vtable: usize,
    count: i32,
    colors: [u32; 17],
}

fn graphics(scheme: *const Scheme) -> EeWonGraphics {
    let mut g = tests_graphics();
    g.scheme = scheme as *const c_void;
    g
}

fn op(kind: i32, a: [i32; 6]) -> EeWonDrawOp {
    EeWonDrawOp { kind, a: a[0], b: a[1], c: a[2], d: a[3], e: a[4], f: a[5], ..EeWonDrawOp::default() }
}

#[test]
fn the_scheme_offset_and_the_plans() {
    ee_won_graphics_register(log_host, std::mem::offset_of!(Scheme, colors));
    let mut s = Scheme { vtable: 0, count: 0, colors: [0; 17] };
    for (i, c) in s.colors.iter_mut().enumerate() {
        *c = 0x100 + i as u32;
    }
    let mut g = graphics(&s);
    let gp: *mut EeWonGraphics = &mut g;
    let slots = EeWonDrawSlots {
        images: [0x10 as _, 0x20 as _, 0x30 as _, 0x40 as _],
        font: 0x50 as _,
        string: std::ptr::null(),
        text: 0x70 as _,
    };
    let ops = [
        op(EE_WON_DRAW_TRANSLATE, [5, 7, 0, 0, 0, 0]),
        op(EE_WON_DRAW_SET_SYS_COLOR, [3, 0, 0, 0, 0, 0]),
        op(EE_WON_DRAW_IMAGE, [2, 1, 1, 0, 0, 0]),
        op(EE_WON_DRAW_IMAGE_SIZE, [1, 9, 8, 0, 0, 0]),
        op(EE_WON_DRAW_SET_FONT, [0; 6]),
        op(EE_WON_DRAW_TEXT, [0, 3, 4, 0, 0, 0]),
        op(EE_WON_DRAW_CALL, [1, 2, 3, 0, 0, 0]),
    ];
    // SAFETY: a live record, the ops, the slots
    unsafe { ee_won_graphics_draw(gp, ops.as_ptr(), ops.len(), &slots) };
    assert_eq!((g.st.trans_x, g.st.trans_y, g.st.color), (5, 7, 0x103));
    let log = LOG.with(|l| l.take());
    assert_eq!(
        log,
        ["ImageEntire 6 8 0 30", "ImageSize 9 8 0 20", "SetFont 0 0 0 50", "Text 3 4 0 70"],
        "the slots' kinds go to the host, the translation applied to the image"
    );
    // the scheme saved and set on the record
    assert_eq!(game_op(crate::guihost::EeWonGuiOp::CtScheme, gp as isize, 0), Some(&s as *const Scheme as isize));
    assert_eq!(game_op(crate::guihost::EeWonGuiOp::CtSetScheme, gp as isize, 0), Some(0));
    assert!(g.scheme.is_null());
    // a plain draw is the Graphics'; a slot's kind and a call are the host's
    let set = op(EE_WON_DRAW_SET_COLOR, [0x8000_0003u32 as i32, 0, 0, 0, 0, 0]);
    let d = crate::guihost::EeWonGuiOp::Draw;
    assert_eq!(game_op(d, gp as isize, &set as *const EeWonDrawOp as isize), Some(0));
    assert_eq!(g.st.color, 0x8000_0003, "no scheme: the reference kept");
    for k in [EE_WON_DRAW_IMAGE, EE_WON_DRAW_STRING, EE_WON_DRAW_TEXT, EE_WON_DRAW_CALL, EE_WON_DRAW_SET_FONT] {
        let o = op(k, [0; 6]);
        assert_eq!(game_op(d, gp as isize, &o as *const EeWonDrawOp as isize), None);
    }
    // the grabber's clip and blend
    let mut clip = [0i32; 4];
    g.st.clip = crate::guilayout::EeWonRect { x: 1, y: 2, width: 3, height: 4 };
    assert_eq!(grab_op(crate::grabobj::EeWonGrab::Clip, gp as isize, clip.as_mut_ptr() as isize, 0), Some(0));
    assert_eq!(clip, [1, 2, 3, 4]);
    assert_eq!(grab_op(crate::grabobj::EeWonGrab::BlendSupported, gp as isize, 0, 0), Some(1));
    assert_eq!(grab_op(crate::grabobj::EeWonGrab::HasImage, gp as isize, 0, 0), None);
    // SAFETY: the record's clip stack
    unsafe { crate::gfxobj::ee_won_gfx_free(g.gfx) };
}
