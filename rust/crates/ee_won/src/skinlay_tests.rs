//! skinlay's layouts: the golden of the original statements (`golden_skinlay.txt`, recorded by
//! tests/won/won_skinlay_test.cpp `--print` over the C++ before the port) replayed over a world rebuilt from each
//! line, through a host that answers as the driver's (and the ported units') does, and unit tests of the corners.

use super::*;
use crate::guihost::EeWonGuiOp;
use std::cell::RefCell;

#[derive(Clone, Copy, Debug, Default)]
struct Comp {
    id: i32,
    x: i32,
    y: i32,
    w: i32,
    h: i32,
    vis: bool,
    dw: i32,
    dh: i32,
    count: i32,
    total: i32,
    col_desired: i32,
}

#[derive(Debug, Default)]
struct World {
    me: Comp,
    draw_frame: bool,
    draw_3d: bool,
    button_in_frame: bool,
    header_h: i32,
    frame: [i32; 4],
    parts: Vec<Comp>,
    /// button, edit, list area, scroll area, vertical bar, horizontal bar, header, label, contained (-1: NULL)
    held: [i32; 9],
    /// each image's width (-1: NULL) and height
    images: [(i32, i32); 4],
    trace: Vec<String>,
}

thread_local! {
    static WORLD: RefCell<World> = RefCell::new(World::default());
}

/// A component's handle: 1 the receiver, 100 + i part i.
fn comp_mut<R>(w: &mut World, h: isize, f: impl FnOnce(&mut Comp) -> R) -> R {
    if h == 1 {
        f(&mut w.me)
    } else {
        f(&mut w.parts[(h - 100) as usize])
    }
}

fn held(w: &World, k: usize) -> isize {
    if w.held[k] < 0 {
        0
    } else {
        100 + w.held[k] as isize
    }
}

unsafe extern "C" fn host(_me: *mut c_void, op: EeWonGuiOp, a: isize, b: isize, c: isize, d: isize) -> isize {
    assert_eq!(op, EeWonGuiOp::SkinLay);
    WORLD.with(|cell| {
        let mut w = cell.borrow_mut();
        let lay = match a {
            1 => EeWonSkinLay::Part,
            2 => EeWonSkinLay::Field,
            3 => EeWonSkinLay::SetHeaderHeight,
            4 => EeWonSkinLay::Width,
            5 => EeWonSkinLay::Height,
            6 => EeWonSkinLay::Left,
            7 => EeWonSkinLay::Top,
            8 => EeWonSkinLay::Right,
            9 => EeWonSkinLay::Visible,
            10 => EeWonSkinLay::SetPosSize,
            11 => EeWonSkinLay::DesiredSize,
            12 => EeWonSkinLay::SetLeftPad,
            13 => EeWonSkinLay::ListCount,
            14 => EeWonSkinLay::ListTotalWidth,
            15 => EeWonSkinLay::SetColumn,
            16 => EeWonSkinLay::SetColumnItem,
            17 => EeWonSkinLay::SetColumnWidth,
            18 => EeWonSkinLay::Image,
            _ => panic!("lay {a}"),
        };
        match lay {
            EeWonSkinLay::Part => held(&w, (c - 1) as usize),
            EeWonSkinLay::Field => match c {
                1 => w.draw_frame as isize,
                2 => w.draw_3d as isize,
                3 => w.button_in_frame as isize,
                4 => w.header_h as isize,
                5..=8 => w.frame[(c - 5) as usize] as isize,
                _ => panic!("field {c}"),
            },
            EeWonSkinLay::SetHeaderHeight => {
                w.header_h = b as i32;
                w.trace.push(format!("headerheight {}", b));
                0
            }
            EeWonSkinLay::Width => comp_mut(&mut w, b, |c| c.w as isize),
            EeWonSkinLay::Height => comp_mut(&mut w, b, |c| c.h as isize),
            EeWonSkinLay::Left => comp_mut(&mut w, b, |c| c.x as isize),
            EeWonSkinLay::Top => comp_mut(&mut w, b, |c| c.y as isize),
            EeWonSkinLay::Right => comp_mut(&mut w, b, |c| c.x.wrapping_add(c.w) as isize),
            EeWonSkinLay::Visible => comp_mut(&mut w, b, |c| c.vis as isize),
            EeWonSkinLay::SetPosSize => {
                // SAFETY: four ints
                let r = unsafe { std::slice::from_raw_parts(c as *const i32, 4) };
                let id = comp_mut(&mut w, b, |x| {
                    x.x = r[0];
                    x.y = r[1];
                    x.w = r[2];
                    x.h = r[3];
                    x.id
                });
                w.trace.push(format!("pos {} {} {} {} {}", id, r[0], r[1], r[2], r[3]));
                0
            }
            EeWonSkinLay::DesiredSize => {
                let (id, dw, dh) = comp_mut(&mut w, b, |x| (x.id, x.dw, x.dh));
                w.trace.push(format!("desired {id}"));
                // SAFETY: two ints
                unsafe {
                    *(c as *mut i32) = dw;
                    *(c as *mut i32).add(1) = dh;
                }
                0
            }
            EeWonSkinLay::SetLeftPad => {
                let id = comp_mut(&mut w, b, |x| x.id);
                w.trace.push(format!("leftpad {id} {c}"));
                0
            }
            EeWonSkinLay::ListCount => comp_mut(&mut w, b, |x| x.count as isize),
            EeWonSkinLay::ListTotalWidth => comp_mut(&mut w, b, |x| x.total as isize),
            EeWonSkinLay::SetColumn => {
                let hd = held(&w, 6);
                let (id, des) = comp_mut(&mut w, hd, |x| (x.id, x.col_desired));
                w.trace.push(format!("column {id} {b} {c}"));
                if d != 0 {
                    // SAFETY: an int
                    unsafe { *(d as *mut i32) = des };
                }
                0
            }
            EeWonSkinLay::SetColumnItem => {
                let hd = held(&w, 6);
                let id = comp_mut(&mut w, hd, |x| x.id);
                let item = if c == 0 { 0 } else { comp_mut(&mut w, c, |x| x.id) };
                w.trace.push(format!("columnitem {id} {b} {item}"));
                0
            }
            EeWonSkinLay::SetColumnWidth => {
                let ar = held(&w, 3);
                let id = comp_mut(&mut w, ar, |x| x.id);
                w.trace.push(format!("colwidth {id} {b} {c} 0"));
                0
            }
            EeWonSkinLay::Image => {
                let (iw, ih) = w.images[b as usize];
                if iw < 0 {
                    return 0;
                }
                // SAFETY: two ints
                unsafe {
                    *(c as *mut i32) = iw;
                    *(c as *mut i32).add(1) = ih;
                }
                1
            }
        }
    })
}

fn kind_of(n: i32) -> EeWonSkinLayKind {
    use EeWonSkinLayKind as K;
    [
        K::MsCombo,
        K::MsScroll,
        K::MsList,
        K::WonComboSize,
        K::WonCombo,
        K::WonScroll,
        K::WonList,
        K::WonListTextHeader,
        K::WonListHeader,
        K::EtchedSize,
        K::SimpleButtonSize,
    ][(n - 1) as usize]
}

fn nums(s: &str) -> Vec<i32> {
    s.split_whitespace().map(|x| x.parse::<i64>().unwrap() as i32).collect()
}

/// The world of a golden line's `#` field.
fn world_of(text: &str) -> World {
    let groups: Vec<&str> = text.split(" / ").collect();
    let r = nums(groups[0]);
    let mut w = World {
        me: Comp { id: 1, x: r[0], y: r[1], w: r[2], h: r[3], vis: true, ..Default::default() },
        draw_frame: r[4] != 0,
        draw_3d: r[5] != 0,
        button_in_frame: r[6] != 0,
        header_h: r[7],
        frame: [r[8], r[9], r[10], r[11]],
        ..Default::default()
    };
    for (i, g) in groups[1..13].iter().enumerate() {
        let p = nums(g);
        w.parts.push(Comp {
            id: 10 + i as i32,
            x: p[0],
            y: p[1],
            w: p[2],
            h: p[3],
            vis: p[4] != 0,
            dw: p[5],
            dh: p[6],
            count: p[7],
            total: p[8],
            col_desired: p[9],
        });
    }
    let hd = nums(groups[13]);
    w.held.copy_from_slice(&hd[..9]);
    let im = nums(groups[14]);
    for i in 0..4 {
        w.images[i] = (im[2 * i], im[2 * i + 1]);
    }
    w
}

#[test]
fn golden_replay() {
    let text = include_str!("golden_skinlay.txt");
    let mut n = 0;
    for line in text.lines() {
        let f: Vec<&str> = line.split(" # ").collect();
        let a = nums(f[0]);
        let (kind, col, title, width, item, in_w, in_h) = (a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
        let kind = kind_of(kind);
        WORLD.with(|c| *c.borrow_mut() = world_of(f[1]));
        let arg = match kind {
            EeWonSkinLayKind::WonListTextHeader => title as isize,
            EeWonSkinLayKind::WonListHeader => 100 + item as isize,
            _ => 0,
        };
        let (mut w, mut h) = (in_w, in_h);
        layout(kind, std::ptr::dangling_mut::<c_void>(), Some(host), col, arg, width, &mut w, &mut h);
        let trace = WORLD.with(|c| c.borrow().trace.join("; "));
        assert_eq!(format!("{trace} | {w} {h}"), f[2], "{line}");
        n += 1;
    }
    assert_eq!(n, 400);
}

fn run(kind: EeWonSkinLayKind, w: World) -> (String, World) {
    WORLD.with(|c| *c.borrow_mut() = w);
    let (mut ww, mut hh) = (0, 0);
    layout(kind, std::ptr::dangling_mut::<c_void>(), Some(host), 0, 0, 0, &mut ww, &mut hh);
    let w = WORLD.with(|c| std::mem::take(&mut *c.borrow_mut()));
    (w.trace.join("; "), w)
}

fn parts(n: usize) -> Vec<Comp> {
    (0..n).map(|i| Comp { id: 10 + i as i32, w: 15, h: 15, vis: true, ..Default::default() }).collect()
}

#[test]
fn ms_combo_puts_the_button_inside_the_frame() {
    let w = World {
        me: Comp { id: 1, w: 100, h: 22, ..Default::default() },
        draw_frame: true,
        parts: parts(2),
        held: [0, 1, -1, -1, -1, -1, -1, -1, -1],
        ..Default::default()
    };
    let (t, _) = run(EeWonSkinLayKind::MsCombo, w);
    assert_eq!(t, "pos 10 83 2 15 18; pos 11 0 0 83 22");
}

#[test]
fn won_combo_button_outside_the_frame_takes_the_full_height() {
    let w = World {
        me: Comp { id: 1, w: 100, h: 22, ..Default::default() },
        draw_frame: true,
        frame: [3, 1, 4, 1],
        parts: parts(2),
        held: [0, 1, -1, -1, -1, -1, -1, -1, -1],
        ..Default::default()
    };
    let (t, _) = run(EeWonSkinLayKind::WonCombo, w);
    assert_eq!(t, "leftpad 11 5; pos 11 0 0 81 22; pos 10 85 0 15 22");
}

#[test]
fn scroll_container_without_a_scroll_area_does_nothing() {
    let w = World { parts: parts(6), held: [-1, -1, -1, -1, 4, 5, -1, -1, -1], ..Default::default() };
    let (t, _) = run(EeWonSkinLayKind::MsScroll, w);
    assert_eq!(t, "");
}

#[test]
fn ms_scroll_frame_needs_both_flags() {
    let mk = |d3| World {
        me: Comp { id: 1, w: 50, h: 40, ..Default::default() },
        draw_frame: true,
        draw_3d: d3,
        parts: parts(6),
        held: [-1, -1, -1, 3, 4, 5, -1, -1, -1],
        ..Default::default()
    };
    let (t, _) = run(EeWonSkinLayKind::MsScroll, mk(false));
    assert_eq!(t, "pos 14 35 0 15 25; pos 15 0 25 35 15; pos 13 0 0 35 25");
    let (t, _) = run(EeWonSkinLayKind::MsScroll, mk(true));
    assert_eq!(t, "pos 14 33 2 15 21; pos 15 2 23 31 15; pos 13 2 2 31 21");
}

#[test]
fn won_combo_desired_size_without_parts() {
    let w = World { held: [-1, -1, -1, -1, -1, -1, -1, -1, -1], ..Default::default() };
    WORLD.with(|c| *c.borrow_mut() = w);
    let (mut ww, mut hh) = (7, 7);
    layout(EeWonSkinLayKind::WonComboSize, std::ptr::dangling_mut::<c_void>(), Some(host), 0, 0, 0, &mut ww, &mut hh);
    assert_eq!((ww, hh), (100, 22));
}

#[test]
fn simple_button_size_is_the_largest_image() {
    let w = World { images: [(-1, 90), (12, 5), (7, 30), (-1, 0)], ..Default::default() };
    WORLD.with(|c| *c.borrow_mut() = w);
    let (mut ww, mut hh) = (99, 99);
    layout(
        EeWonSkinLayKind::SimpleButtonSize,
        std::ptr::dangling_mut::<c_void>(),
        Some(host),
        0,
        0,
        0,
        &mut ww,
        &mut hh,
    );
    assert_eq!((ww, hh), (12, 30));
}

#[test]
fn first_column_header_fixes_the_height() {
    let mut p = parts(10);
    p[6].col_desired = 0;
    let w = World { parts: p, held: [-1, -1, -1, 3, -1, -1, 6, -1, -1], ..Default::default() };
    WORLD.with(|c| *c.borrow_mut() = w);
    let (mut ww, mut hh) = (0, 0);
    layout(
        EeWonSkinLayKind::WonListTextHeader,
        std::ptr::dangling_mut::<c_void>(),
        Some(host),
        2,
        0x1003,
        40,
        &mut ww,
        &mut hh,
    );
    let t = WORLD.with(|c| c.borrow().trace.join("; "));
    assert_eq!(t, "column 16 2 4099; headerheight 15; colwidth 13 2 40 0");
}
