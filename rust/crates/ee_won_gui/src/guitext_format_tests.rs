//! `ee_won_gui_text_append_formatted` against TextFormat.cpp's loop as it was (R6 WON GUI round 36): the same
//! formats through both, each call on the area logged.
use super::*;
use ee_base::plat_core::string::WChar;
use std::cell::RefCell;

thread_local! {
    static LOG: RefCell<Vec<String>> = const { RefCell::new(Vec::new()) };
}
fn log(s: String) {
    LOG.with(|l| l.borrow_mut().push(s));
}
fn take() -> Vec<String> {
    LOG.with(|l| std::mem::take(&mut *l.borrow_mut()))
}

unsafe extern "C" fn add_text(
    area: *mut c_void,
    units: *const c_void,
    len: usize,
    color: i32,
    font: *mut c_void,
    lb: bool,
) {
    // SAFETY: `len` units
    let t: String = unsafe { std::slice::from_raw_parts(units as *const WChar, len) }
        .iter()
        .map(|&u| char::from_u32(u as u32).unwrap())
        .collect();
    log(format!("{} text [{t}] {color:x} {} {lb}", area as usize, font as usize));
}
unsafe extern "C" fn line_break(area: *mut c_void) {
    log(format!("{} break", area as usize));
}
unsafe extern "C" fn add_string(area: *mut c_void, s: *const c_void, color: i32, font: *mut c_void) {
    log(format!("{} string {} {color:x} {}", area as usize, s as usize, font as usize));
}
unsafe extern "C" fn add_segment(area: *mut c_void, s: *mut c_void) {
    log(format!("{} segment {}", area as usize, s as usize));
}
const SINK: EeWonFormatSink = EeWonFormatSink { add_text, line_break, add_string, add_segment };

/// TextFormat.cpp's loop, its statements in order (the area's calls logged as the sink logs them).
#[allow(clippy::too_many_arguments)]
fn old_cpp(
    area: usize,
    format: &[WChar],
    strings: &[usize],
    colors: &[i32],
    fonts: &[usize],
    segments: &[usize],
    line_break: bool,
    text_color: i32,
    area_font: usize,
) {
    let mut color = text_color; // int color=area->mTextColor;
    let mut font = area_font; // Font* font=area->mFont.get();
    let mut text = String::new(); // GUIString text;
    let wide: Vec<u32> = format.iter().map(|&u| u as u32).collect();
    let counts = [strings.len() as i32, colors.len() as i32, fonts.len() as i32, segments.len() as i32];
    for op in format_ops(&wide, counts, line_break) {
        let i = op.a as usize;
        match op.kind {
            0 => {
                let (s, l) = (op.a as u16 as usize, op.b as u16 as usize);
                text.extend(format[s..s + l].iter().map(|&u| char::from_u32(u as u32).unwrap()));
            }
            1 => text.push('<'),
            2 => {
                log(format!("{area} text [{text}] {color:x} {font} false"));
                text.clear();
            }
            3 => log(format!("{area} break")),
            4 => color = colors[i],
            5 => {
                if fonts[i] != 0 {
                    font = fonts[i];
                }
            }
            6 => {
                if strings[i] != 0 {
                    log(format!("{area} string {} {color:x} {font}", strings[i]));
                }
            }
            7 => {
                if segments[i] != 0 {
                    log(format!("{area} segment {}", segments[i]));
                }
            }
            8 => log(format!("{area} text [{text}] {color:x} {font} {}", op.b != 0)),
            _ => {}
        }
    }
}

#[test]
fn the_formats_steps_reach_the_area_as_the_cpp_loop_made_them() {
    let formats = [
        "",
        "plain",
        "a<<b",
        "<n>",
        "one<n>two<N>three",
        "<c1>red<c2>blue<c9>none<c0>",
        "<f1>big<f2>null font<f3>third",
        "<s1> joined <s2><s3>.",
        "x<l1>y<l2>z<l3>",
        "<c2><f1><s1>: <c1>hello <<world>> <n><l1>tail",
        "unterminated <c1",
        "<",
        "<<",
        "<>",
        "<q5>?",
        "<S1><C1><F1><L1><N>",
        "text<n>",
        "<s1><s1><s1>",
    ];
    let strings = [1001usize, 0, 1003];
    let colors = [0xff0000, 0x00ff00];
    let fonts = [2001usize, 0, 2003];
    let segments = [3001usize, 0];
    let mut runs = 0;
    let mut calls = 0;
    for f in formats {
        let w: Vec<WChar> = f.encode_utf16().map(|c| c as WChar).collect();
        for lb in [false, true] {
            // every count from none to all, so the indices past the counts are skipped alike
            for cut in 0..=3usize {
                let (s, c, fo, se) = (
                    &strings[..strings.len().min(cut)],
                    &colors[..colors.len().min(cut)],
                    &fonts[..fonts.len().min(cut)],
                    &segments[..segments.len().min(cut)],
                );
                old_cpp(7, &w, s, c, fo, se, lb, 0x123456, 9000);
                let old = take();
                let sp: Vec<*const c_void> = s.iter().map(|&x| x as *const c_void).collect();
                let fp: Vec<*mut c_void> = fo.iter().map(|&x| x as *mut c_void).collect();
                let gp: Vec<*mut c_void> = se.iter().map(|&x| x as *mut c_void).collect();
                let args = EeWonFormatArgs {
                    strings: sp.as_ptr(),
                    num_strings: sp.len() as i32,
                    colors: c.as_ptr(),
                    num_colors: c.len() as i32,
                    fonts: fp.as_ptr(),
                    num_fonts: fp.len() as i32,
                    segments: gp.as_ptr(),
                    num_segments: gp.len() as i32,
                };
                // SAFETY: the format's units, the arrays at their counts, the logging sink
                unsafe {
                    ee_won_gui_text_append_formatted(
                        7 as *mut c_void,
                        &SINK,
                        w.as_ptr() as *const c_void,
                        w.len() as u16,
                        &args,
                        lb,
                        0x123456,
                        9000 as *mut c_void,
                    )
                };
                let new = take();
                calls += new.len();
                assert_eq!(new, old, "{f:?} line break {lb} counts {cut}");
                runs += 1;
            }
        }
    }
    assert_eq!(runs, 18 * 2 * 4);
    assert!(calls > 300, "{calls}");
    // the steps do reach the area: a known format's calls
    let w: Vec<WChar> = "<c1>a<n><s1>b".encode_utf16().map(|c| c as WChar).collect();
    old_cpp(1, &w, &[5], &[0xab], &[], &[], true, 0, 2);
    assert_eq!(take(), ["1 text [a] ab 2 false", "1 break", "1 string 5 ab 2", "1 text [b] ab 2 true"]);
}
