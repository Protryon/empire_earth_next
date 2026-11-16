use super::*;
use std::cell::RefCell;

// ---- a world: the C++ object's members, its clip stack, a scheme, a font, and a log of the hooks (the base
// rectangle hooks run Graphics' own, as tests/won/won_gfxobj_test.cpp's traced Graphics does)

struct World {
    st: EeWonGfxState,
    obj: EeWonGfx,
    scheme: Option<[u32; 17]>,
    font: Option<i32>,
    log: RefCell<Vec<String>>,
}

fn names(p: *const c_void) -> i32 {
    p as usize as i32
}

unsafe extern "C" fn host(g: *mut c_void, hook: EeWonGfxHook, a: *const i32, p: *const c_void) -> u32 {
    use EeWonGfxHook as H;
    let w = g as *mut World;
    // SAFETY: seven ints (the contract); the world is live
    let a = unsafe { std::slice::from_raw_parts(a, 7) };
    let base = |c: EeWonGfxCall| unsafe { call(w, c, [a[0], a[1], a[2], a[3], 0, 0, 0], p) };
    let line = match hook {
        H::ClearClipRect => "clearclip".to_string(),
        H::SetClipRect => format!("setclip {} {} {} {}", a[0], a[1], a[2], a[3]),
        H::SetPixel => format!("pixel {} {} {:x}", a[0], a[1], a[2] as u32),
        // SAFETY: the world is live; read only
        H::SetColor => format!("color {:x}", unsafe { (*w).st.color }),
        H::DrawLine => format!("line {} {} {} {}", a[0], a[1], a[2], a[3]),
        H::DrawRect => return base(EeWonGfxCall::BaseRect),
        H::FillRect => return base(EeWonGfxCall::BaseFillRect),
        H::DrawDottedRect => return base(EeWonGfxCall::BaseDottedRect),
        H::DrawEllipse => format!("ellipse {} {} {} {}", a[0], a[1], a[2], a[3]),
        H::FillEllipse => format!("fillellipse {} {} {} {}", a[0], a[1], a[2], a[3]),
        H::DrawArc => format!("arc {} {} {} {} {} {}", a[0], a[1], a[2], a[3], a[4], a[5]),
        H::FillRectBlend => format!("blend {} {} {} {} {}", a[0], a[1], a[2], a[3], a[4]),
        H::ImageEntire => format!("entire {} {} {}", names(p), a[0], a[1]),
        H::ImagePart => format!("part {} {} {} {} {} {} {}", names(p), a[0], a[1], a[2], a[3], a[4], a[5]),
        // SAFETY: the world is live; read only
        H::String => {
            format!("string {} {} {} {} {} {}", unsafe { (*w).font.unwrap() }, names(p), a[0], a[1], a[2], a[3])
        }
    };
    // SAFETY: the world is live
    unsafe { (*w).log.borrow_mut().push(line) };
    0
}

/// # Safety
/// `w` is live.
unsafe fn call(w: *mut World, c: EeWonGfxCall, a: [i32; 7], p: *const c_void) -> u32 {
    // SAFETY: the contract; the fields' pointers stay valid across the hooks
    unsafe {
        let scheme = (*w).scheme.as_ref().map_or(std::ptr::null(), |s| s.as_ptr());
        ee_won_gfx_call(w.cast(), host, &mut (*w).obj, &mut (*w).st, scheme, c, a.as_ptr(), p)
    }
}

fn world() -> Box<World> {
    Box::new(World {
        st: EeWonGfxState::default(),
        obj: EeWonGfx::default(),
        scheme: None,
        font: None,
        log: RefCell::new(vec![]),
    })
}

fn take(w: &World) -> Vec<String> {
    std::mem::take(&mut *w.log.borrow_mut())
}

fn scheme_color(k: u32, i: u32) -> u32 {
    match i {
        3 => 0x8000_0003,
        5 => 0x8000_0000,
        _ => 0x10101 * i + 0x1000 * k,
    }
}

fn scheme(k: u32) -> [u32; 17] {
    std::array::from_fn(|i| scheme_color(k, i as u32))
}

#[test]
fn a_scheme_reference_resolves_once() {
    let mut w = world();
    w.scheme = Some(scheme(0));
    let p: *mut World = &mut *w;
    // SAFETY: a live world
    unsafe { call(p, EeWonGfxCall::SetColor, [0x8000_0003u32 as i32, 0, 0, 0, 0, 0, 0], std::ptr::null()) };
    assert_eq!(w.st.color, 0x8000_0003, "the self-reference is the colour, not followed");
    assert_eq!(take(&w), vec!["color 80000003"]);
    // the same colour again: no hook
    unsafe { call(p, EeWonGfxCall::SetColor, [0x8000_0003u32 as i32, 0, 0, 0, 0, 0, 0], std::ptr::null()) };
    assert!(take(&w).is_empty());
    // without a scheme a reference is a colour
    w.scheme = None;
    unsafe { call(p, EeWonGfxCall::SetColor, [0x8000_0001u32 as i32, 0, 0, 0, 0, 0, 0], std::ptr::null()) };
    assert_eq!(w.st.color, 0x8000_0001);
    assert_eq!(unsafe { call(p, EeWonGfxCall::StandardColor, [1, 0, 0, 0, 0, 0, 0], std::ptr::null()) }, 0);
}

#[test]
fn the_clip_is_the_stacks_intersection() {
    let mut w = world();
    let p: *mut World = &mut *w;
    // SAFETY: a live world
    unsafe {
        call(p, EeWonGfxCall::Translate, [5, 5, 0, 0, 0, 0, 0], std::ptr::null());
        call(p, EeWonGfxCall::PushClip, [0, 0, 20, 20, 0, 0, 0], std::ptr::null());
        call(p, EeWonGfxCall::PushClip, [10, 10, 20, 20, 0, 0, 0], std::ptr::null());
    }
    assert_eq!(w.st.clip, EeWonRect { x: 15, y: 15, width: 10, height: 10 });
    assert_eq!(take(&w), vec!["clearclip", "setclip 5 5 20 20", "clearclip", "setclip 15 15 10 10"]);
    unsafe {
        call(p, EeWonGfxCall::PopClip, [0; 7], std::ptr::null());
        call(p, EeWonGfxCall::PopClip, [0; 7], std::ptr::null());
        call(p, EeWonGfxCall::PopClip, [0; 7], std::ptr::null());
    }
    assert_eq!(w.st.clip, NO_CLIP, "an empty stack: no clip");
    assert_eq!(take(&w), vec!["clearclip", "setclip 5 5 20 20", "clearclip", "clearclip"]);
}

#[test]
fn the_base_hooks() {
    let mut w = world();
    w.st.color = 0xab;
    let p: *mut World = &mut *w;
    // SAFETY: a live world
    unsafe { call(p, EeWonGfxCall::DrawRect, [1, 2, 3, 4, 0, 0, 0], std::ptr::null()) };
    assert_eq!(take(&w), vec!["line 1 2 4 2", "line 4 2 4 6", "line 4 6 1 6", "line 1 6 1 2"]);
    unsafe { call(p, EeWonGfxCall::FillRect, [0, 0, 5, 2, 0, 0, 0], std::ptr::null()) };
    assert_eq!(take(&w), vec!["line 0 0 5 0", "line 0 1 5 1"]);
    unsafe { call(p, EeWonGfxCall::DrawDottedRect, [0, 0, 3, 1, 0, 0, 0], std::ptr::null()) };
    assert_eq!(
        take(&w),
        vec!["pixel 0 0 ab", "pixel 0 0 ab", "pixel 2 0 ab", "pixel 2 0 ab", "pixel 0 0 ab", "pixel 2 0 ab"]
    );
}

// ---- the golden: tests/won/won_gfxobj_test.cpp's first 300 sequences over the original's statements

const GOLDEN: &str = include_str!("golden_gfxobj.txt");

#[test]
fn golden_replay() {
    let mut w = world();
    let mut seq = String::new();
    let mut n = 0;
    for line in GOLDEN.lines() {
        let (head, tail) = line.split_once(" : ").unwrap();
        let f: Vec<&str> = head.split(' ').collect();
        if f[0] != seq {
            seq = f[0].to_string();
            w = world();
        }
        let a: Vec<i32> = f[2..9].iter().map(|x| x.parse().unwrap()).collect();
        let a: [i32; 7] = a.try_into().unwrap();
        let p: *mut World = &mut *w;
        let image = |i: i32| i as usize as *const c_void;
        let text = |k: i32| (10 + (k & 1)) as usize as *const c_void;
        use EeWonGfxCall as C;
        let mut r = 0u32;
        // SAFETY: a live world
        unsafe {
            match f[1] {
                "setcolor" => {
                    call(p, C::SetColor, [a[6], 0, 0, 0, 0, 0, 0], std::ptr::null());
                }
                "stdcolor" => r = call(p, C::StandardColor, [a[2] & 15, 0, 0, 0, 0, 0, 0], std::ptr::null()),
                "syscolor" => {
                    call(p, C::SetSysColor, [a[2] & 15, 0, 0, 0, 0, 0, 0], std::ptr::null());
                }
                "translate" => {
                    call(p, C::Translate, [a[0] / 3, a[1] / 3, 0, 0, 0, 0, 0], std::ptr::null());
                }
                "pixel" => {
                    call(p, C::SetPixel, a, std::ptr::null());
                }
                "image" => {
                    call(p, C::DrawImage, a, image(a[2] & 1));
                }
                "imagepart" => {
                    call(p, C::DrawImagePart, a, image(2));
                }
                "line" => {
                    call(p, C::DrawLine, a, std::ptr::null());
                }
                "rect" => {
                    call(p, C::DrawRect, a, std::ptr::null());
                }
                "fill" => {
                    call(p, C::FillRect, [a[0], a[1], a[2] / 4, a[3] / 4, 0, 0, 0], std::ptr::null());
                }
                "blend" => {
                    call(p, C::FillRectBlend, a, std::ptr::null());
                }
                "dotted" => {
                    call(p, C::DrawDottedRect, [a[0], a[1], a[2] / 2, a[3] / 2, 0, 0, 0], std::ptr::null());
                }
                "ellipse" => {
                    call(p, C::DrawEllipse, a, std::ptr::null());
                }
                "fillellipse" => {
                    call(p, C::FillEllipse, a, std::ptr::null());
                }
                "arc" => {
                    call(p, C::DrawArc, a, std::ptr::null());
                }
                "string" => {
                    if (*p).font.is_some() {
                        let len = if a[2] & 1 == 0 { 2 } else { 5 };
                        call(p, C::DrawString, [a[0], a[1], len, 0, 0, 0, 0], text(a[2]));
                    }
                }
                "stringpart" => {
                    if (*p).font.is_some() {
                        call(p, C::DrawStringPart, [a[3] & 3, a[4] & 7, a[0], a[1], 0, 0, 0], text(a[2]));
                    }
                }
                "perform" => {
                    call(p, C::PerformClip, a, std::ptr::null());
                }
                "push" => {
                    call(p, C::PushClip, a, std::ptr::null());
                }
                "pop" => {
                    call(p, C::PopClip, a, std::ptr::null());
                }
                "scheme" => (*p).scheme = if a[2] & 3 == 0 { None } else { Some(scheme((a[2] & 1) as u32)) },
                "font" => {
                    // Graphics::SetFont (C++): the font prepared when it changes
                    let size = if a[2] & 1 == 0 { 7 } else { 9 };
                    if (*p).font != Some(size) {
                        (*p).font = Some(size);
                        (*p).log.borrow_mut().push(format!("prepare {size}"));
                    }
                }
                other => panic!("unknown call {other}"),
            }
        }
        let s = &w.st;
        let state = format!(
            "{} {} {:x} {} {} {} {} r{:x}",
            s.trans_x, s.trans_y, s.color, s.clip.x, s.clip.y, s.clip.width, s.clip.height, r
        );
        let log: String = take(&w).iter().map(|l| format!(" {l};")).collect();
        assert_eq!(format!("{state} |{log}"), tail, "{line}");
        n += 1;
    }
    assert!(n > 10_000, "{n} calls");
}
