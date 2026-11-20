//! The readers over a scripted host (the statements themselves: tests/won/won_compcfg_test.cpp).

use super::*;

#[test]
fn the_maps_keep_the_first_spelling_and_order_ignoring_case() {
    let mut m: NoCaseMap<i32> = NoCaseMap::new();
    assert!(m.insert(b"Beta", 1));
    assert!(m.insert(b"alpha", 2));
    assert!(!m.insert(b"ALPHA", 3));
    assert_eq!(m.set(b"BETA", 4), Some(1));
    assert_eq!(m.at(0), Some((&b"alpha"[..], &2)));
    assert_eq!(m.at(1), Some((&b"Beta"[..], &4)));
    assert_eq!(m.get(b"bEtA"), Some(&4));
}

#[test]
fn resource_files_keep_drives_and_strip_dot_slash() {
    set_resource_folder(b"res");
    assert_eq!(resource_file(b"c:x.png"), b"c:x.png");
    assert_eq!(resource_file(b"./x.png"), b"x.png");
    assert_eq!(resource_file(b".\\x.png"), b"x.png");
    assert_eq!(resource_file(b"x.png"), b"res/x.png");
    set_resource_folder(b"res\\");
    assert_eq!(resource_file(b"x"), b"res\\x");
    set_resource_folder(b"");
    assert_eq!(resource_file(b"x"), b"/x");
}

#[test]
fn max_and_min_start_from_zero() {
    assert_eq!(call_named_function(b"MAX", &[-3, 5, 2]), Some(5));
    assert_eq!(call_named_function(b"MIN", &[3, 5]), Some(0));
    assert_eq!(call_named_function(b"MIN", &[3, -5]), Some(-5));
    assert_eq!(call_named_function(b"MAX", &[]), Some(0));
    assert_eq!(call_named_function(b"AVG", &[1]), None);
}

fn parse<T>(text: &str, f: impl FnOnce(Sp) -> T) -> (T, i32) {
    let mut t = text.as_bytes().to_vec();
    t.push(0);
    let mut p = EeWonCfgStringParser { text: t.as_ptr() as *const c_void, pos: 0, wide: false };
    let r = f(Sp(&mut p));
    (r, p.pos)
}

#[test]
fn colors_read_hex_dec_and_standard_names() {
    assert_eq!(parse("  ff8001", safe_read_color).0, Ok(0xff8001));
    assert_eq!(parse("(DEC, 1, 2, 300)", safe_read_color).0, Ok(0x01022c));
    assert_eq!(parse("(std, hilight)", safe_read_color).0, Ok(0x8000_0007_u32 as i32));
    assert_eq!(parse("(STD, Nope)", safe_read_color).0, Err(CcErr::Msg(b"Invalid standard color: STD".to_vec())));
    assert_eq!(parse("(XYZ)", safe_read_color).0, Ok(0));
    assert_eq!(parse("12g456", safe_read_color).0, Err(CcErr::Msg(b"Invalid color specification.".to_vec())));
    assert_eq!(parse("(DEC; 1", safe_read_color).0, Err(CcErr::Msg(b"Expecting ')'".to_vec())));
}

#[test]
fn font_styles_add_up_until_an_empty_word() {
    assert_eq!(parse("Bold Italic underline", safe_read_font_style).0, Ok(7));
    assert_eq!(parse("plain", safe_read_font_style).0, Ok(0));
    assert_eq!(parse("Bold Heavy", safe_read_font_style).0, Err(CcErr::Msg(b"Unknown font style: Heavy".to_vec())));
}
