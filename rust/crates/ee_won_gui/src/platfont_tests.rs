//! platfont: the font over the platform fonts with the test face of ee_plat_ui's goldens.
use super::*;

const FONT: &[u8] = include_bytes!("../../ee_base/src/plat_ui/golden_font.ttf");

fn register() {
    // SAFETY: a static face's data
    unsafe {
        ee_base::plat_ui::font::PlatFontRegister(
            c"Synth".as_ptr(),
            0,
            0,
            FONT.as_ptr() as *const c_void,
            FONT.len() as u32,
        )
    };
}

fn wide(s: &str) -> Vec<WChar> {
    s.encode_utf16().map(|c| c as WChar).collect()
}

#[test]
fn mul_div_rounds_half_away_from_zero() {
    assert_eq!(mul_div_round(9, 96, 72), 12);
    assert_eq!(mul_div_round(10, 96, 72), 13); // 13.33
    assert_eq!(mul_div_round(-10, 96, 72), -13);
    assert_eq!(mul_div_round(3, 1, 2), 2); // 1.5
}

#[test]
fn metrics_widths_and_the_underline() {
    register();
    let f = EeWonPlatFont::new(b"Synth", 12, UNDERLINE);
    assert!(f.height() > 0 && f.ascent() > 0);
    assert_eq!(f.height(), f.ascent() + f.descent());
    let w = f.width(&wide("abc"));
    assert!(w > 0);
    assert_eq!(f.width(&[]), 0);
    let d = f.descent();
    let m = if d < 3 {
        0
    } else if d == 3 {
        1
    } else {
        d >> 1
    };
    assert_eq!(f.underline(&wide("abc"), 5, 7), Some((5, 7 + f.height() - m - 1, 5 + w)));
    assert_eq!(EeWonPlatFont::new(b"Synth", 12, BOLD).underline(&wide("abc"), 0, 0), None);
}

#[test]
fn scaled_fonts_are_kept_and_drawing_stays_in_the_clip() {
    register();
    let mut f = EeWonPlatFont::new(b"Synth", 10, 0);
    let a = f.for_scale(2.0);
    assert_eq!(f.for_scale(2.0), a);
    assert_eq!(f.for_scale(1.0), f.font as *const PlatFont);
    let mut s = EeWonSurface::new(crate::surface::EE_WON_SURFACE_32).unwrap();
    s.create(40, 20);
    s.set_clip(0, 0, 10, 20);
    f.draw(&mut s, 1.0, 0xffffff, &wide("WWWWWWWW"), 0, 0);
    let px = s.words_mut().unwrap();
    assert!(px.iter().enumerate().any(|(i, &p)| i % 40 < 10 && p != 0));
    assert!(px.iter().enumerate().all(|(i, &p)| i % 40 < 10 || p == 0));
}

#[test]
fn utf8_of_wide_text() {
    assert_eq!(to_utf8(&wide("a\u{e9}")), "a\u{e9}".as_bytes());
    assert!(to_utf8(&[]).is_empty());
}

#[test]
fn the_underline_by_descent() {
    register();
    let mut f = EeWonPlatFont::new(b"Synth", 12, UNDERLINE);
    f.height = 20;
    for (d, m) in [(0, 0), (2, 0), (3, 1), (4, 2), (7, 3)] {
        f.descent = d;
        assert_eq!(f.underline(&[], 0, 0), Some((0, 20 - m - 1, 0)), "descent {d}");
    }
}

#[test]
fn a_scaled_font_is_smoothed_and_the_logical_one_not() {
    register();
    let mut f = EeWonPlatFont::new(b"Synth", 12, 0);
    let grey = |f: &mut EeWonPlatFont, k: f32| {
        let mut s = EeWonSurface::new(crate::surface::EE_WON_SURFACE_32).unwrap();
        s.create(120, 60);
        f.draw(&mut s, k, 0xffffff, &wide("Wave"), 1, 1);
        s.words_mut().unwrap().iter().any(|&p| p & 0xff != 0 && p & 0xff != 0xff)
    };
    assert!(grey(&mut f, 2.0));
    assert!(!grey(&mut f, 1.0));
}
