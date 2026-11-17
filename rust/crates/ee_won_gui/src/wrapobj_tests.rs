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
