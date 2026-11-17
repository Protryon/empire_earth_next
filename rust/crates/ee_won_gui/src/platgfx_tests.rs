//! platgfx's hooks: unit tests and the replay of the driver's golden (`golden_platgfx.txt`, recorded by
//! tests/won/won_platgfx_test.cpp `--print` over the original statements' twin: each sequence's surface, then each
//! hook with its arguments, colours, points, answer and the surface's pixel hash, clip and colour after it).
use super::*;
use crate::surface::{EeWonSurface, EeWonSurfaceField as F, EE_WON_SURFACE_32};

fn surface(w: i32, h: i32) -> EeWonSurface {
    let mut s = EeWonSurface::new(EE_WON_SURFACE_32).unwrap();
    s.create(w, h);
    s
}

fn px(s: &mut EeWonSurface, x: i32, y: i32) -> u32 {
    s.pixel(x, y)
}

#[test]
fn pixels_round_half_up() {
    assert_eq!(pixels(7, 1.0), 7);
    assert_eq!(pixels(3, 1.5), 5); // 4.5 + 0.5
    assert_eq!(pixels(1, 1.25), 1); // 1.75
    assert_eq!(pixels(2, 1.25), 3); // 3.0
    assert_eq!(pixels(-1, 2.0), -2);
    assert_eq!(pixels(-3, 1.5), -4); // -4.5 + 0.5 = -4
}

#[test]
fn fill_rect_normalises_and_scales() {
    let mut s = surface(10, 10);
    let mut g = Gfx { s: &mut s, k: 2.0, color: 0xff12_3456 };
    g.hook(EeWonPlatGfx::FillRect, 3, 3, -2, -1, 0, 0); // x 1..2, y 2..2 -> pixels 2..5, 4..5
    assert_eq!(px(&mut s, 2, 4), 0x12_3456);
    assert_eq!(px(&mut s, 5, 5), 0x12_3456);
    assert_eq!(px(&mut s, 6, 4), 0);
    assert_eq!(px(&mut s, 2, 3), 0);
}

#[test]
fn get_pixel_is_a_colorref() {
    let mut s = surface(4, 4);
    s.draw(crate::surface::EeWonSurfaceDraw::SetPixel, 1, 1, 0x0011_2233, 0, 0, 0);
    let mut g = Gfx { s: &mut s, k: 1.0, color: 0 };
    assert_eq!(g.hook(EeWonPlatGfx::GetPixel, 1, 1, 0, 0, 0, 0), 0x0033_2211);
}

#[test]
fn blend_rounds_and_keeps_alpha() {
    let mut s = surface(2, 1);
    if let Some(w) = s.words_mut() {
        w[0] = 0xAA00_0000;
    }
    let mut g = Gfx { s: &mut s, k: 1.0, color: 0x00FF_FFFF };
    g.hook(EeWonPlatGfx::FillRectBlend, 0, 0, 1, 1, 128, 0);
    // (255 * 128 + 0 * 127 + 127) / 255 = 128
    assert_eq!(s.words_mut().unwrap()[0], 0xAA80_8080);
}

#[test]
fn set_clip_intersects_the_current_clip() {
    let mut s = surface(20, 20);
    let mut g = Gfx { s: &mut s, k: 1.0, color: 0 };
    g.hook(EeWonPlatGfx::SetClip, 5, 5, 10, 10, 0, 0);
    g.hook(EeWonPlatGfx::SetClip, 0, 8, 7, 30, 0, 0);
    assert_eq!((s.field(F::ClipX1), s.field(F::ClipY1), s.field(F::ClipX2), s.field(F::ClipY2)), (5, 8, 6, 14));
}

#[test]
fn scaled_line_is_one_logical_pixel_wide() {
    let mut s = surface(12, 12);
    let mut g = Gfx { s: &mut s, k: 2.0, color: 0xffffff };
    g.hook(EeWonPlatGfx::Line, 0, 0, 2, 1, 0, 0);
    // every column of pixels 0..6 has a span of two pixels
    for x in 0..6 {
        let n = (0..12).filter(|&y| px(&mut s, x, y) != 0).count();
        assert_eq!(n, 2, "column {x}");
    }
}

#[test]
fn polygon_fill_even_odd_with_outline() {
    let mut s = surface(10, 10);
    let mut g = Gfx { s: &mut s, k: 1.0, color: 0x0000_00ff };
    g.fill_polygon(&[(1, 1), (6, 1), (6, 6), (1, 6)]);
    assert_eq!(px(&mut s, 3, 3), 0xff);
    assert_eq!(px(&mut s, 6, 6), 0xff); // the outline's corner
    assert_eq!(px(&mut s, 7, 7), 0);
}

#[test]
fn no_pixels_draws_nothing() {
    let mut s = EeWonSurface::new(EE_WON_SURFACE_32).unwrap();
    let mut g = Gfx { s: &mut s, k: 2.0, color: 0x123 };
    g.hook(EeWonPlatGfx::FillRect, 0, 0, 5, 5, 0, 0);
    g.hook(EeWonPlatGfx::SetPixel, 0, 0, 5, 0, 0, 0);
    assert_eq!(s.field(F::Color), 0);
}

const GOLDEN: &str = include_str!("golden_platgfx.txt");

fn fill(s: &mut EeWonSurface, mut seed: u32) {
    let n = (s.field(F::PixelsPerRow) as usize) * (s.field(F::Height) as usize);
    if let Some(w) = s.words_mut() {
        for p in w.iter_mut().take(n) {
            seed = seed.wrapping_mul(1103515245).wrapping_add(12345);
            *p = seed ^ (seed >> 15);
        }
    }
}

fn state(s: Option<&mut EeWonSurface>) -> String {
    let Some(s) = s else {
        return "none".into();
    };
    let n = (s.field(F::BytesPerRow) as usize) * (s.field(F::Height).max(0) as usize);
    let none = s.pixels().is_null();
    let b = s.bytes();
    let h = if none {
        0
    } else {
        b[..n.min(b.len())].iter().fold(2166136261u32, |h, &c| (h ^ c as u32).wrapping_mul(16777619))
    };
    format!(
        "{:08x} clip {} {} {} {} color {:08x}",
        h,
        s.field(F::ClipX1),
        s.field(F::ClipY1),
        s.field(F::ClipX2),
        s.field(F::ClipY2),
        s.field(F::Color) as u32
    )
}

fn hook_of(o: u32) -> EeWonPlatGfx {
    match o {
        1 => EeWonPlatGfx::GetPixel,
        2 => EeWonPlatGfx::SetPixel,
        3 => EeWonPlatGfx::Line,
        4 => EeWonPlatGfx::Rect,
        5 => EeWonPlatGfx::FillRect,
        6 => EeWonPlatGfx::FillRectBlend,
        8 => EeWonPlatGfx::Ellipse,
        9 => EeWonPlatGfx::Arc,
        10 => EeWonPlatGfx::FillEllipse,
        11 => EeWonPlatGfx::ClearClip,
        _ => EeWonPlatGfx::SetClip,
    }
}

#[test]
fn golden_replay() {
    let mut surf: Option<EeWonSurface> = None;
    let mut k = 1.0f32;
    let (mut seqs, mut ops) = (0, 0);
    for line in GOLDEN.lines() {
        let (head, want) = line.split_once(" = ").unwrap_or((line, ""));
        let v: Vec<&str> = head.split(' ').collect();
        if v[0] == "seq" {
            let (kind, w, h): (i32, i32, i32) = (v[2].parse().unwrap(), v[3].parse().unwrap(), v[4].parse().unwrap());
            k = v[5].parse().unwrap();
            let seed: u32 = v[6].parse().unwrap();
            surf = match kind {
                0 => None,
                1 => EeWonSurface::new(EE_WON_SURFACE_32),
                _ => {
                    let mut s = surface(w, h);
                    fill(&mut s, seed);
                    Some(s)
                }
            };
            seqs += 1;
            continue;
        }
        let n: Vec<i64> = v[1..].iter().map(|t| t.parse().unwrap()).collect();
        let (op, a) = (n[0] as u32, [n[1], n[2], n[3], n[4], n[5], n[6]].map(|x| x as i32));
        let (pixel_color, color, count) = (n[7] as u32, n[8] as u32, n[9] as usize);
        let pts: Vec<(i32, i32)> = (0..count).map(|i| (n[10 + 2 * i] as i32, n[11 + 2 * i] as i32)).collect();
        let mut got = 0u32;
        if let Some(s) = surf.as_mut() {
            let mut g = Gfx { s, k, color };
            match op {
                14 => g.polygon(&pts),
                15 => g.fill_polygon(&pts),
                2 => {
                    g.hook(EeWonPlatGfx::SetPixel, a[0], a[1], pixel_color as i32, 0, 0, 0);
                }
                o => got = g.hook(hook_of(o), a[0], a[1], a[2], a[3], a[4], a[5]),
            }
        }
        let (want_ret, want_state) = want.split_once(' ').unwrap();
        assert_eq!(got.to_string(), want_ret, "sequence {seqs} line {line}");
        assert_eq!(state(surf.as_mut()), want_state, "sequence {seqs} line {line}");
        ops += 1;
    }
    assert_eq!((seqs, ops), (300, 12000));
}
