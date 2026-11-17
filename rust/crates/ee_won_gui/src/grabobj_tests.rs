//! grabobj's grabber: the golden of the original BackgroundGrabber.cpp (`golden_grabobj.txt`, recorded by
//! tests/won/won_grabobj_test.cpp `--print` over the C++ before the port) replayed over the driver's world rebuilt in
//! Rust (its component, window, images, Graphics and debug window, traced as the driver traces them), and unit
//! tests of the corners.

use super::*;
use crate::guihost::EeWonGuiOp;

#[derive(Debug, Default)]
struct World {
    w: i32,
    h: i32,
    root_x: i32,
    root_y: i32,
    flags: i32,
    screen_x: i32,
    screen_y: i32,
    clip: [i32; 4],
    blend: bool,
    next_image: i32,
    /// (id, width, height, generation)
    images: Vec<(i32, i32, i32, i32)>,
    /// the port grabber's C++ members: its background image (an index into `images`) and whether it has a debug
    /// window
    bg: Option<usize>,
    debug: bool,
    trace: Vec<String>,
}

thread_local! {
    static WORLD: RefCell<World> = RefCell::new(World::default());
}

fn fnv(px: &[u32]) -> u64 {
    let mut h: u64 = 1469598103934665603;
    for p in px {
        for b in p.to_le_bytes() {
            h ^= b as u64;
            h = h.wrapping_mul(1099511628211);
        }
    }
    h
}

fn grab_of(n: isize) -> EeWonGrab {
    use EeWonGrab as G;
    [
        G::LocalToRoot,
        G::DrawImage,
        G::Clip,
        G::CompSize,
        G::ImageSize,
        G::NewImage,
        G::FlagSet,
        G::DebugExists,
        G::DebugCreate,
        G::DebugShow,
        G::DebugInvalidate,
        G::Capture,
        G::BlendSupported,
        G::Blend,
        G::Tint,
    ][(n - 1) as usize]
}

unsafe extern "C" fn host(_me: *mut c_void, op: EeWonGuiOp, a: isize, b: isize, c: isize, d: isize) -> isize {
    assert_eq!(op, EeWonGuiOp::Grab);
    let _ = b;
    WORLD.with(|cell| {
        let mut w = cell.borrow_mut();
        let bg_id = |w: &World| w.bg.map_or(0, |i| w.images[i].0);
        match grab_of(a) {
            EeWonGrab::LocalToRoot => {
                // SAFETY: two ints
                unsafe {
                    *(c as *mut i32) += w.root_x;
                    *(c as *mut i32).add(1) += w.root_y;
                }
                0
            }
            EeWonGrab::DrawImage => {
                let id = bg_id(&w);
                w.trace.push(format!("draw {id} at {c} {d}"));
                0
            }
            EeWonGrab::Clip => {
                // SAFETY: four ints
                unsafe { std::ptr::copy_nonoverlapping(w.clip.as_ptr(), c as *mut i32, 4) };
                0
            }
            EeWonGrab::CompSize => {
                // SAFETY: two ints
                unsafe {
                    *(c as *mut i32) = w.w;
                    *(c as *mut i32).add(1) = w.h;
                }
                0
            }
            EeWonGrab::ImageSize => match w.bg {
                None => 0,
                Some(i) => {
                    let (_, iw, ih, _) = w.images[i];
                    // SAFETY: two ints
                    unsafe {
                        *(c as *mut i32) = iw as u16 as i32;
                        *(c as *mut i32).add(1) = ih as u16 as i32;
                    }
                    1
                }
            },
            EeWonGrab::NewImage => {
                w.next_image += 1;
                let id = w.next_image;
                w.trace.push(format!("createimage {id} {c} {d}"));
                w.images.push((id, c as i32, d as i32, 0));
                w.bg = Some(w.images.len() - 1);
                0
            }
            EeWonGrab::FlagSet => (w.flags as isize & c != 0) as isize,
            EeWonGrab::DebugExists => w.debug as isize,
            EeWonGrab::DebugCreate => {
                let (_, iw, ih, _) = w.images[w.bg.unwrap()];
                w.trace.push("debug new".into());
                w.debug = true;
                w.trace.push("debug parent 1".into());
                w.trace.push("debug style 40".into());
                w.trace.push(format!("debug create 0 0 {} {}", iw as u16, ih as u16));
                0
            }
            EeWonGrab::DebugShow => {
                let id = bg_id(&w);
                w.trace.push("debug removeall".into());
                w.trace.push(format!("debug add {id}"));
                let (x, y) = (w.root_x + w.screen_x, w.root_y + w.screen_y);
                w.trace.push(format!("debug pos {x} {y} -1 -1"));
                0
            }
            EeWonGrab::DebugInvalidate => {
                w.trace.push("debug invalidate".into());
                0
            }
            EeWonGrab::Capture => {
                w.trace.push("offscreen".into());
                let i = w.bg.unwrap();
                let (id, rx, ry) = (w.images[i].0, -w.root_x, -w.root_y);
                w.trace.push(format!("bgdraw {id} from 999 at {rx} {ry}"));
                w.images[i].3 += 1;
                0
            }
            EeWonGrab::BlendSupported => w.blend as isize,
            EeWonGrab::Blend => {
                // SAFETY: three ints
                let p = unsafe { std::slice::from_raw_parts(d as *const i32, 3) };
                w.trace.push(format!("setcolor {:x}", c as i32 as u32));
                w.trace.push(format!("blend 0 0 {} {} {}", p[0], p[1], p[2]));
                0
            }
            EeWonGrab::Tint => {
                let (id, iw, ih, gen) = w.images[w.bg.unwrap()];
                w.trace.push(format!("getraw {id} {iw} {ih}"));
                let n = (iw * ih).max(0) as usize;
                let mut s = (id as u32).wrapping_mul(2654435761).wrapping_add((gen as u32).wrapping_mul(40503));
                let mut px: Vec<u32> = (0..n)
                    .map(|_| {
                        s = s.wrapping_mul(1103515245).wrapping_add(12345);
                        s ^ (s << 13)
                    })
                    .collect();
                if n > 0 {
                    tint(&mut px, c as i32, d as i32);
                }
                w.trace.push(format!("drawraw {iw} {ih} at 0 0 hash {:016x}", fnv(&px)));
                0
            }
        }
    })
}

fn nums(s: &str) -> Vec<i64> {
    s.split_whitespace().map(|x| x.parse::<i64>().unwrap()).collect()
}

#[test]
fn golden_replay() {
    let text = include_str!("golden_grabobj.txt");
    let mut g: Option<EeWonGrabber> = None;
    let mut calls = 0;
    for line in text.lines() {
        if let Some(rest) = line.strip_prefix("S ") {
            let _ = rest;
            WORLD.with(|c| *c.borrow_mut() = World::default());
            g = Some(EeWonGrabber::new(std::ptr::null_mut(), Some(host)));
            continue;
        }
        let gr = g.as_ref().unwrap();
        if let Some(rest) = line.strip_prefix("W ") {
            let v: Vec<&str> = rest.split(' ').collect();
            let n = |i: usize| v[i].parse::<i32>().unwrap();
            WORLD.with(|c| {
                let mut w = c.borrow_mut();
                w.w = n(0);
                w.h = n(1);
                w.root_x = n(2);
                w.root_y = n(3);
                w.flags = i32::from_str_radix(v[4], 16).unwrap();
                w.screen_x = n(5);
                w.screen_y = n(6);
                w.clip = [n(7), n(8), n(9), n(10)];
                w.blend = n(11) != 0;
            });
        } else if let Some(rest) = line.strip_prefix("C ") {
            let (head, want) = rest.split_once(" | ").unwrap_or((rest.trim_end_matches(" |"), ""));
            let h: Vec<&str> = head.split(' ').collect();
            let got = match h[0] {
                "paint" => {
                    gr.call(EeWonGrabCall::Paint, 1, 2);
                    None
                }
                "prepaint" => {
                    gr.call(EeWonGrabCall::PrePaint, 1, 2);
                    None
                }
                "invalidate" => {
                    gr.call(EeWonGrabCall::Invalidate, 1, 0);
                    None
                }
                "clear" => {
                    gr.call(EeWonGrabCall::Clear, 0, 0);
                    None
                }
                "translucent" => {
                    let v = nums(&h[1..].join(" "));
                    gr.call(EeWonGrabCall::SetTranslucentColor, v[0] as isize, v[1] as isize);
                    None
                }
                "sizechanged" => {
                    gr.call(EeWonGrabCall::SizeChanged, 1, 0);
                    None
                }
                "needdraw" => Some(gr.call(EeWonGrabCall::NeedDrawBG, 0, 0).to_string()),
                other => panic!("call {other}"),
            };
            let trace = got.unwrap_or_else(|| WORLD.with(|c| std::mem::take(&mut c.borrow_mut().trace).join("; ")));
            assert_eq!(trace, want, "{line}");
            calls += 1;
        } else if let Some(rest) = line.strip_prefix("R ") {
            let mut q = EeWonRootGrab::default();
            gr.read(&mut q);
            let got = format!(
                "need {} order {} at {} {} clip {} {} {} {}",
                q.need, q.order, q.x, q.y, q.clip.x, q.clip.y, q.clip.w, q.clip.h
            );
            assert_eq!(got, rest, "{line}");
        } else {
            panic!("line {line}");
        }
    }
    assert!(calls > 10000, "{calls}");
}

#[test]
fn color_8th_shifts_an_int_arithmetically() {
    // a negative colour's top byte keeps its sign bits through the shift (the original's `int` shift)
    assert_eq!(color_8th(0x80000000u32 as i32, 1) as u32, 0xf0000000);
    assert_eq!(color_8th(0x00804020, 4), 0x00402010);
    assert_eq!(color_8th(0x00ffffff, 0), 0);
    assert_eq!(color_8th(0x00ffffff, 8), 0);
}

#[test]
fn tint_scales_pixels_and_adds_the_colour() {
    let mut px = [0xff808080u32, 0x00000008];
    tint(&mut px, 4, 0x00010203);
    // a DWORD's shift is logical: alpha halves too
    assert_eq!(px, [0x7f404040 + 0x00010203, 0x00000004 + 0x00010203]);
    let mut same = [7u32];
    tint(&mut same, 8, 5);
    assert_eq!(same, [7]);
    tint(&mut [], 3, 1);
}

#[test]
fn state_before_and_after_a_capture() {
    WORLD
        .with(|c| *c.borrow_mut() = World { w: 2, h: 2, root_x: 5, root_y: 7, clip: [1, 2, 3, 4], ..World::default() });
    let g = EeWonGrabber::new(std::ptr::null_mut(), Some(host));
    assert!(!g.need_draw_bg());
    g.invalidate();
    assert!(!g.need_draw_bg());
    g.paint(1, 2);
    let mut q = EeWonRootGrab::default();
    g.read(&mut q);
    assert_eq!((q.x, q.y, q.clip.w), (5, 7, 3));
    g.invalidate();
    assert!(g.need_draw_bg());
    // a second paint before PrePaint does nothing
    WORLD.with(|c| c.borrow_mut().trace.clear());
    g.paint(1, 2);
    assert!(WORLD.with(|c| c.borrow().trace.is_empty()));
    g.pre_paint(1, 2);
    assert!(!g.need_draw_bg());
    let t = WORLD.with(|c| c.borrow().trace.join("; "));
    assert_eq!(t, "draw 1 at 0 0");
}
