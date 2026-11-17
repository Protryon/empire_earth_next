//! surface's walks: the golden of the WON SDK's Surface.cpp (`golden_surface.txt`, recorded by
//! tests/won/won_surface_test.cpp `--print` over the original statements) replayed over the Rust surfaces, each
//! operation's fields and pixel hash compared, and unit tests of the corners.

use super::*;

fn new(format: i32, w: i32, h: i32) -> EeWonSurface {
    let mut s = EeWonSurface::new(format).unwrap();
    s.create(w, h);
    s
}

fn fnv(b: &[u8]) -> u64 {
    let mut h: u64 = 1469598103934665603;
    for &x in b {
        h ^= x as u64;
        h = h.wrapping_mul(1099511628211);
    }
    h
}

/// The driver's fields and hash line (`= w h bpr ppr cx1 cy1 cx2 cy2 color pixels hash`).
fn state(s: &mut EeWonSurface) -> String {
    use EeWonSurfaceField as F;
    let has = !s.pixels().is_null();
    let n = if has { (s.field(F::BytesPerRow) as i64 * s.field(F::Height) as i64).max(0) as usize } else { 0 };
    let b = s.bytes();
    let hash = fnv(&b[..n.min(b.len())]);
    format!(
        "= {} {} {} {} {} {} {} {} {:x} {} {:016x}",
        s.field(F::Width),
        s.field(F::Height),
        s.field(F::BytesPerRow),
        s.field(F::PixelsPerRow),
        s.field(F::ClipX1),
        s.field(F::ClipY1),
        s.field(F::ClipX2),
        s.field(F::ClipY2),
        s.field(F::Color) as u32,
        has as i32,
        hash
    )
}

/// The driver's mask for a source `w` x `h` and a salt.
fn mask(w: i32, h: i32, salt: u32) -> Vec<u8> {
    let bpr = w / 8 + if w % 8 != 0 { 1 } else { 0 };
    let n = (bpr * h).max(0) as usize + 1;
    (0..n)
        .map(|k| {
            let v = ((k as u32).wrapping_mul(37).wrapping_add(salt * 11).wrapping_add((k as u32 >> 3) * 5)) & 0xff;
            (match salt & 3 {
                0 => v & (v >> 1),
                1 => v | (v << 3),
                _ => v,
            }) as u8
        })
        .collect()
}

fn draw_of(n: i32) -> EeWonSurfaceDraw {
    use EeWonSurfaceDraw as D;
    [
        D::SetColor,
        D::FillRect,
        D::DrawRect,
        D::HorzLine,
        D::VertLine,
        D::Line,
        D::Circle,
        D::FillCircle,
        D::Ellipse,
        D::FillEllipse,
        D::Arc,
        D::PutPixel,
        D::SetPixel,
        D::SetClipRect,
        D::ClearClipRect,
    ][(n - 1) as usize]
}

#[test]
fn golden_replay() {
    let text = include_str!("golden_surface.txt");
    let mut slots: Vec<Option<EeWonSurface>> = vec![None, None, None];
    let mut ops = 0;
    for line in text.lines() {
        let (head, want) = match line.find(" = ") {
            Some(i) => (&line[..i], &line[i + 1..]),
            None => (line, ""),
        };
        let v: Vec<&str> = head.split(' ').collect();
        let n = |i: usize| -> i32 { v[i].parse::<i64>().unwrap() as i32 };
        if v[0] == "S" {
            continue;
        }
        let k = n(1) as usize;
        match v[0] {
            "N" => {
                let mut s = EeWonSurface::new(n(2)).unwrap();
                s.create(n(3), n(4));
                slots[k] = Some(s);
            }
            "C" => slots[k].as_mut().unwrap().create(n(2), n(3)),
            "F" => slots[k].as_mut().unwrap().release(),
            "D" => {
                let a: Vec<i32> = (3..9).map(|i| if i < v.len() { n(i) } else { 0 }).collect();
                slots[k].as_mut().unwrap().draw(draw_of(n(2)), a[0], a[1], a[2], a[3], a[4], a[5]);
            }
            "P" => {
                let got = slots[k].as_mut().unwrap().pixel(n(2), n(3));
                assert_eq!(format!("{:x}", got), v[4], "{line}");
            }
            "K" => {
                let from = n(2) as usize;
                let (sw, sh) = {
                    let f = slots[from].as_ref().unwrap();
                    (f.width, f.height)
                };
                let m = mask(sw, sh, n(4) as u32);
                let mp = if n(3) != 0 { m.as_ptr() } else { std::ptr::null() };
                let r = (n(5), n(6), n(7), n(8), n(9), n(10));
                if from == k {
                    // SAFETY: the mask covers the source's rows
                    unsafe { slots[k].as_mut().unwrap().copy(None, mp, r.0, r.1, r.2, r.3, r.4, r.5) };
                } else {
                    let src = slots[from].take().unwrap();
                    // SAFETY: as above
                    unsafe { slots[k].as_mut().unwrap().copy(Some(&src), mp, r.0, r.1, r.2, r.3, r.4, r.5) };
                    slots[from] = Some(src);
                }
            }
            "U" => {
                let d = slots[n(2) as usize].as_ref().unwrap().duplicate();
                slots[k] = Some(d);
            }
            other => panic!("unknown line {other}"),
        }
        if v[0] != "P" {
            assert_eq!(state(slots[k].as_mut().unwrap()), want, "{line}");
        }
        ops += 1;
    }
    assert!(ops > 9000, "{ops}");
}

#[test]
fn rows_padded_and_clip_whole() {
    let s = new(EE_WON_SURFACE_555, 3, 2);
    assert_eq!(s.field(EeWonSurfaceField::BytesPerRow), 8);
    assert_eq!(s.field(EeWonSurfaceField::PixelsPerRow), 4);
    assert_eq!(s.field(EeWonSurfaceField::ClipX2), 2);
    assert_eq!(s.field(EeWonSurfaceField::ClipY2), 1);
}

#[test]
fn unknown_format_and_release() {
    assert!(EeWonSurface::new(3).is_none());
    let mut s = new(EE_WON_SURFACE_32, 5, 4);
    assert!(!s.pixels().is_null());
    s.release();
    assert!(s.pixels().is_null());
    // the pixels per row are kept, as the original kept them
    assert_eq!(s.field(EeWonSurfaceField::PixelsPerRow), 5);
    assert_eq!(s.field(EeWonSurfaceField::ClipX2), -1);
    // an empty surface still has pixels (the original's `new char[0]`)
    let mut e = new(EE_WON_SURFACE_32, 0, 3);
    assert!(!e.pixels().is_null());
}

#[test]
fn colours_convert_per_format() {
    assert_eq!(color_from_32(0x00ff8040, EE_WON_SURFACE_555), (0x40 >> 3) | ((0x80 >> 3) << 5) | ((0xff >> 3) << 10));
    assert_eq!(color_from_32(0x00ff8040, EE_WON_SURFACE_565), (0x40 >> 3) | ((0x80 >> 2) << 5) | ((0xff >> 3) << 11));
    assert_eq!(color_to_32(color_from_32(0x00f8f8f8, EE_WON_SURFACE_555), EE_WON_SURFACE_555), 0x00f8f8f8);
    let mut s = new(EE_WON_SURFACE_565, 2, 2);
    s.draw(EeWonSurfaceDraw::SetColor, 0x7fffffff, 0, 0, 0, 0, 0);
    assert_eq!(s.field(EeWonSurfaceField::Color), 0xffff);
}

#[test]
fn line_draws_both_ends_and_clipped_line_only_inside() {
    let mut s = new(EE_WON_SURFACE_32, 8, 8);
    s.draw(EeWonSurfaceDraw::SetColor, 7, 0, 0, 0, 0, 0);
    s.draw(EeWonSurfaceDraw::Line, 1, 1, 5, 3, 0, 0);
    assert_eq!(s.pixel(1, 1), 7);
    assert_eq!(s.pixel(5, 3), 7);
    s.draw(EeWonSurfaceDraw::SetClipRect, 0, 0, 4, 8, 0, 0);
    s.draw(EeWonSurfaceDraw::SetColor, 9, 0, 0, 0, 0, 0);
    s.draw(EeWonSurfaceDraw::Line, 0, 7, 7, 0, 0, 0);
    assert_eq!(s.pixel(3, 4), 9);
    assert_eq!(s.pixel(4, 3), 0);
    // both ends left of the clip: nothing
    s.draw(EeWonSurfaceDraw::SetClipRect, 4, 0, 4, 8, 0, 0);
    s.draw(EeWonSurfaceDraw::Line, 0, 0, 2, 6, 0, 0);
    assert_eq!(s.pixel(1, 3), 0);
}

#[test]
fn rect_of_width_one_is_a_line() {
    let mut s = new(EE_WON_SURFACE_32, 6, 6);
    s.draw(EeWonSurfaceDraw::SetColor, 3, 0, 0, 0, 0, 0);
    s.draw(EeWonSurfaceDraw::DrawRect, 2, 1, 1, 4, 0, 0);
    assert_eq!((1..5).map(|y| s.pixel(2, y)).collect::<Vec<_>>(), vec![3, 3, 3, 3]);
    assert_eq!(s.pixel(3, 1), 0);
}

#[test]
fn masked_copy_keeps_transparent_pixels() {
    let mut src = new(EE_WON_SURFACE_32, 9, 1);
    src.draw(EeWonSurfaceDraw::SetColor, 5, 0, 0, 0, 0, 0);
    src.draw(EeWonSurfaceDraw::FillRect, 0, 0, 9, 1, 0, 0);
    let mut dst = new(EE_WON_SURFACE_32, 9, 1);
    // the first and the ninth pixel transparent (bit 7 of byte 0, bit 7 of byte 1)
    let m = [0x80u8, 0x80];
    // SAFETY: the mask covers the source's row
    unsafe { dst.copy(Some(&src), m.as_ptr(), 0, 0, 0, 0, 9, 1) };
    let row: Vec<u32> = (0..9).map(|x| dst.pixel(x, 0)).collect();
    assert_eq!(row, vec![0, 5, 5, 5, 5, 5, 5, 5, 0]);
}

#[test]
fn arc_of_a_full_turn_is_the_ellipse() {
    let mut a = new(EE_WON_SURFACE_32, 20, 20);
    let mut b = new(EE_WON_SURFACE_32, 20, 20);
    for s in [&mut a, &mut b] {
        s.draw(EeWonSurfaceDraw::SetColor, 1, 0, 0, 0, 0, 0);
    }
    a.draw(EeWonSurfaceDraw::Arc, 2, 3, 15, 11, 30, 360);
    b.draw(EeWonSurfaceDraw::Ellipse, 2, 3, 15, 11, 0, 0);
    assert_eq!(a.bytes(), b.bytes());
    // a quarter arc draws part of it
    let mut q = new(EE_WON_SURFACE_32, 20, 20);
    q.draw(EeWonSurfaceDraw::SetColor, 1, 0, 0, 0, 0, 0);
    q.draw(EeWonSurfaceDraw::Arc, 2, 3, 15, 11, 0, 90);
    let n = |s: &EeWonSurface| s.bytes().iter().filter(|&&x| x != 0).count();
    assert!(n(&q) > 0 && n(&q) < n(&b));
}

#[test]
fn arc_start_far_below_zero_keeps_the_c_remainder() {
    // -800 + 360 = -440, whose C remainder by 360 is -80: the arc from -80 to 40 degrees, which draws in the first
    // octant (kept at -440, the arc would end at -320 and draw nothing); -1160 comes to the same -80
    let draw = |start: i32| {
        let mut s = new(EE_WON_SURFACE_32, 30, 30);
        s.draw(EeWonSurfaceDraw::SetColor, 1, 0, 0, 0, 0, 0);
        s.draw(EeWonSurfaceDraw::Arc, 1, 1, 27, 25, start, 120);
        s.bytes().to_vec()
    };
    assert!(draw(-800).iter().any(|&b| b != 0));
    assert_eq!(draw(-800), draw(-1160));
}

#[test]
fn pixel_past_the_right_edge_is_zero() {
    // a 32-bit row's pixel at x == width would be the next row's first: the original read 0 there
    let mut s = new(EE_WON_SURFACE_32, 3, 2);
    s.draw(EeWonSurfaceDraw::SetColor, 0x123456, 0, 0, 0, 0, 0);
    s.draw(EeWonSurfaceDraw::FillRect, 0, 0, 3, 2, 0, 0);
    assert_eq!(s.pixel(2, 0), 0x123456);
    assert_eq!(s.pixel(3, 0), 0);
    assert_eq!(s.pixel(0, 2), 0);
}
