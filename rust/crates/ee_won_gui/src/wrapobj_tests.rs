//! wrapobj's WrappedText: the golden of the original WrappedText statements (`golden_wrapobj.txt`, recorded by
//! tests/won/won_wrapobj_test.cpp `--print`, the C++ before the port) replayed over the driver's stand-in fonts, and
//! unit tests of the corners.

use super::*;

struct Text<'a> {
    units: &'a [u32],
    w: &'a [i32; 16],
}

unsafe extern "C" fn string_width(ctx: *mut c_void, off: i32, len: i32) -> i32 {
    // SAFETY: the test's measure
    let t = unsafe { &*(ctx as *const Text) };
    let (off, len) = (off as u16 as i32, len as u16 as i32);
    let mut w = 0;
    let mut i = off;
    while i < off + len && i < t.units.len() as i32 {
        w += t.w[(t.units[i as usize] % 16) as usize];
        i += 1;
    }
    w
}

unsafe extern "C" fn char_width(ctx: *mut c_void, index: i32) -> i32 {
    // SAFETY: the test's measure
    let t = unsafe { &*(ctx as *const Text) };
    let ch = if index < 0 { u32::from(b'M') } else { t.units[index as usize] };
    t.w[(ch % 16) as usize]
}

fn measure(t: &Text) -> EeWonTextMeasure {
    EeWonTextMeasure {
        ctx: t as *const Text as *mut c_void,
        string_width: Some(string_width),
        char_width: Some(char_width),
    }
}

fn nums(s: &str) -> Vec<i32> {
    s.split_whitespace().map(|x| x.parse().unwrap()).collect()
}

fn ops_text(ops: &[EeWonDrawOp]) -> String {
    if ops.is_empty() {
        return "-".into();
    }
    ops.iter()
        .map(|p| format!("{}:{},{},{},{},{},{},{},{}", p.kind, p.a, p.b, p.c, p.d, p.e, p.f, p.g, p.h))
        .collect::<Vec<_>>()
        .join(" ")
}

#[test]
fn golden_replay() {
    let text = include_str!("golden_wrapobj.txt");
    let lines: Vec<&str> = text.lines().collect();
    let mut i = 0;
    let (mut seqs, mut calls) = (0, 0);
    while i < lines.len() {
        assert!(lines[i].starts_with("S "));
        let mut fonts = [(0i32, [0i32; 16]); 2];
        for (f, font) in fonts.iter_mut().enumerate() {
            let v = nums(&lines[i + 1 + f][2..]);
            font.0 = v[0];
            font.1.copy_from_slice(&v[1..17]);
        }
        i += 3;
        let mut o = EeWonWrapText::new();
        let mut units: Vec<u32> = Vec::new();
        let mut font: Option<usize> = None;
        while i < lines.len() && lines[i].starts_with("O ") {
            let f: Vec<&str> = lines[i][2..].split(' ').collect();
            let (op, a, b): (i32, i32, i32) = (f[0].parse().unwrap(), f[1].parse().unwrap(), f[2].parse().unwrap());
            let calc = |o: &mut EeWonWrapText, units: &[u32], font: Option<usize>| {
                let w = font.map(|k| fonts[k].1).unwrap_or([0; 16]);
                let t = Text { units, w: &w };
                o.calc(font.is_some(), units, &measure(&t), font.map(|k| fonts[k].0).unwrap_or(0));
            };
            let mut ops = Vec::new();
            match op {
                0 => {
                    units = if f[3] == "-" {
                        Vec::new()
                    } else {
                        f[3].split(',').map(|x| u32::from_str_radix(x, 16).unwrap()).collect()
                    };
                    calc(&mut o, &units, font);
                }
                1 => {
                    if o.set_wrap(a != 0) {
                        calc(&mut o, &units, font);
                    }
                }
                2 => o.centered = a != 0,
                3 => {
                    font = if a == 0 { None } else { Some(a as usize - 1) };
                    calc(&mut o, &units, font);
                }
                4 => {
                    if o.set_max_width(a) {
                        calc(&mut o, &units, font);
                    }
                }
                _ => {
                    if let Some(k) = font {
                        let t = Text { units: &units, w: &fonts[k].1 };
                        ops = o.paint(units.len() as i32, fonts[k].0, a, b, &measure(&t));
                    }
                }
            }
            let mut got = format!("{} {}", o.width, o.height);
            for x in &o.breaks {
                got += &format!(" {}/{}/{}", x.break_pos, x.next_start, x.width);
            }
            got += &format!(" | {}", ops_text(&ops));
            assert_eq!(got, &lines[i + 1][2..], "line {}", i + 1);
            i += 2;
            calls += 1;
        }
        seqs += 1;
    }
    assert_eq!((seqs, calls), (300, 300 * 40));
}

#[test]
fn a_new_text_wraps_with_no_size() {
    let o = EeWonWrapText::new();
    assert!(o.wrap && !o.centered);
    assert_eq!((o.width, o.height, o.max_width), (0, 0, 0));
}

#[test]
fn no_font_changes_nothing_and_unwrapped_text_measures_whole() {
    let mut o = EeWonWrapText::new();
    let w = [3; 16];
    let units = [u32::from(b'a'); 5];
    let t = Text { units: &units, w: &w };
    o.calc(false, &units, &measure(&t), 9);
    assert_eq!((o.width, o.height), (0, 0));
    assert!(o.set_wrap(false));
    assert!(!o.set_wrap(false));
    o.calc(true, &units, &measure(&t), 9);
    assert_eq!((o.width, o.height), (15, 9));
}

#[test]
fn the_height_counts_the_lines() {
    let mut o = EeWonWrapText::new();
    let w = [4; 16];
    let units: Vec<u32> = "ab cd ef".bytes().map(u32::from).collect();
    let t = Text { units: &units, w: &w };
    assert!(!o.set_max_width(0));
    assert!(o.set_max_width(10));
    o.calc(true, &units, &measure(&t), 7);
    assert_eq!(o.breaks.len(), 2);
    assert_eq!(o.height, 21);
    // narrower: rewrap; the same: nothing
    assert!(o.set_max_width(9));
    assert!(!o.set_max_width(9));
}

// ---- the font's own measure (R6 WON GUI round 36): the C++ WONDrawMeasure's statements, kept here, against
// `ee_won_wrap_calc_font` / `ee_won_wrap_paint_font` over a real font

struct OldMeasure {
    font: *const EeWonPlatFont,
    text: Vec<WChar>,
}
unsafe extern "C" fn old_string_width(ctx: *mut c_void, offset: i32, length: i32) -> i32 {
    // SAFETY: the test's record
    let m = unsafe { &*(ctx as *const OldMeasure) };
    // Font::GetStringWidth(text, (unsigned short)offset, (unsigned short)length)
    let (o, n) = (offset as u16 as usize, length as u16);
    if n == 0 {
        0
    } else {
        // SAFETY: a live font, the units inside the text
        unsafe { ee_won_plat_font_width(m.font, m.text[o..].as_ptr() as *const c_void, n) }
    }
}
unsafe extern "C" fn old_char_width(ctx: *mut c_void, index: i32) -> i32 {
    // SAFETY: the test's record
    let m = unsafe { &*(ctx as *const OldMeasure) };
    // Font::GetCharacterWidth(index < 0 ? 'M' : text[index])
    // SAFETY: a live font
    unsafe { ee_won_plat_font_char_width(m.font, if index < 0 { 'M' as i32 } else { m.text[index as usize] as i32 }) }
}

#[test]
fn the_fonts_measure_is_the_old_cpp_measure() {
    const FONT: &[u8] = include_bytes!("../../ee_base/src/plat_ui/golden_font.ttf");
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
    let texts = [
        "",
        "a",
        "The quick brown fox jumps over the lazy dog",
        "one\ntwo  three\n\nfour five six seven eight nine ten",
        "MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM",
        "a b c d e f g h i j k l m n o p q r s t u v w x y z",
        "  leading and trailing  ",
    ];
    let mut checks = 0;
    for size in [8, 12, 20] {
        // SAFETY: a face name
        let font = unsafe { crate::platfont::ee_won_plat_font_new(c"Synth".as_ptr(), size, 0) };
        // SAFETY: the live font
        let height = unsafe { ee_won_plat_font_metric(font, 0) };
        assert!(height > 0);
        for t in texts {
            let text: Vec<WChar> = t.encode_utf16().map(|c| c as WChar).collect();
            let old = OldMeasure { font, text: text.clone() };
            let m = EeWonTextMeasure {
                ctx: &old as *const OldMeasure as *mut c_void,
                string_width: Some(old_string_width),
                char_width: Some(old_char_width),
            };
            for max_width in [0, 1, 17, 40, 90, 250, 1000] {
                for (wrap, centered) in [(true, false), (true, true), (false, false)] {
                    let (a, b) = (ee_won_wrap_new(), ee_won_wrap_new());
                    // SAFETY: live objects, the text's units, a live font
                    unsafe {
                        for o in [a, b] {
                            ee_won_wrap_set_wrap(o, wrap);
                            ee_won_wrap_set_max_width(o, max_width);
                            ee_won_wrap_set_centered(o, centered);
                        }
                        let p = text.as_ptr() as *const c_void;
                        let n = text.len() as i32;
                        ee_won_wrap_calc(a, true, p, n, std::mem::size_of::<WChar>() as u32, &m, height);
                        ee_won_wrap_calc_font(b, font, p, n);
                        assert_eq!(*a, *b, "{t:?} size {size} width {max_width}");
                        let (mut oa, mut ob) = (vec![EeWonDrawOp::default(); 256], vec![EeWonDrawOp::default(); 256]);
                        let na = ee_won_wrap_paint(a, n, height, 3, 5, &m, oa.as_mut_ptr(), oa.len());
                        let nb = ee_won_wrap_paint_font(b, font, p, n, 3, 5, ob.as_mut_ptr(), ob.len());
                        assert_eq!(na, nb);
                        assert!(na <= 256);
                        assert_eq!(ops_text(&oa[..na]), ops_text(&ob[..nb]), "{t:?} size {size} width {max_width}");
                        // no font: nothing changes and nothing is measured
                        let before = (*b).clone();
                        ee_won_wrap_calc_font(b, std::ptr::null(), p, n);
                        let c = ee_won_wrap_new();
                        ee_won_wrap_set_wrap(c, wrap);
                        ee_won_wrap_set_max_width(c, max_width);
                        ee_won_wrap_set_centered(c, centered);
                        ee_won_wrap_calc(c, true, p, n, std::mem::size_of::<WChar>() as u32, &m, height);
                        ee_won_wrap_calc(c, false, p, n, std::mem::size_of::<WChar>() as u32, &m, 0);
                        assert_eq!(*b, *c, "no font after {before:?}");
                        ee_won_wrap_free(a);
                        ee_won_wrap_free(b);
                        ee_won_wrap_free(c);
                    }
                    checks += 1;
                }
            }
        }
        // SAFETY: the font made above
        unsafe { crate::platfont::ee_won_plat_font_free(font) };
    }
    assert_eq!(checks, 3 * 7 * 7 * 3);
}
