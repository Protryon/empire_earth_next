//! The fixture's texts.

use super::text::*;

#[test]
fn command_lines_scan_as_sscanf() {
    let c = command(b"12 click 5 3\n");
    assert_eq!((c.fields, c.seq, c.op.as_str(), c.a, c.b), (4, 12, "click", 5, 3));
    let c = command(b"13 pointer 11 640 400 1500");
    assert_eq!((c.fields, c.b, c.c, c.d), (6, 640, 400, 1500));
    // the first conversion that fails ends the scan
    let c = command(b"14 zoom 11 x 9");
    assert_eq!((c.fields, c.b, c.c), (3, 0, 0));
    // a word of more than 31 characters: the rest is not a number
    let c = command(format!("15 {} 1 2", "a".repeat(40)).as_bytes());
    assert_eq!((c.fields, c.op.len()), (2, 31));
    // a minus wraps the unsigned value
    let c = command(b"16 lselect 11 -1 2");
    assert_eq!(c.b, (-1i64) as std::os::raw::c_ulong as u64);
    assert_eq!(command(b"").fields, 0);
}

#[test]
fn signed_fields_and_the_box_line() {
    let mut s = Scan::new(b"9 boxselect 11 0 -5 6 +7 8");
    assert_eq!(s.ulong(), Some(9));
    assert_eq!(s.word(31).as_deref(), Some("boxselect"));
    assert_eq!(s.ulong(), Some(11));
    assert_eq!((s.long(), s.long(), s.long(), s.long(), s.long()), (Some(0), Some(-5), Some(6), Some(7), Some(8)));
    assert_eq!(s.fields, 8);
}

#[test]
fn first_line_as_fgets() {
    assert_eq!(first_line(b"1 click 2 3\n4 x"), b"1 click 2 3\n");
    assert_eq!(first_line(&[b'a'; 300]).len(), 199);
    assert_eq!(first_line(b"no new line"), b"no new line");
}

#[test]
fn the_switches_numbers() {
    assert_eq!(strtoull0("0x10"), (16, true));
    assert_eq!(strtoull0(" 017"), (15, true));
    assert_eq!(strtoull0("12x"), (12, false));
    assert_eq!(strtoull0("x"), (0, false));
    assert_eq!(strtoull0("-1"), (u64::MAX, true));
    assert_eq!(strtol10("16"), (16, true));
    assert_eq!(strtol10("16 "), (16, false));
    assert_eq!(strtol10("-3"), (-3, true));
    assert_eq!(time_factor("32"), Some((32, 1)));
    assert_eq!(time_factor("3/2"), Some((3, 2)));
    assert_eq!(time_factor("1.5"), Some((15, 10)));
    assert_eq!(time_factor("0.1255"), Some((125, 1000)));
    assert_eq!(time_factor("2."), Some((2, 1)));
    assert_eq!(time_factor("x"), None);
}

#[test]
fn escaped_texts_lines_and_ids() {
    assert_eq!(escaped(b"a|b\\c\x01\xe9"), "a\\x7cb\\x5cc\\x01\\xe9");
    assert_eq!(escaped(&[b'x'; 100]).len(), 80);
    assert_eq!(lines(&[b"one".to_vec(), b"two".to_vec()]), " one|two\n");
    assert_eq!(lines(&[]), "\n");
    assert_eq!(ids(vec![3, 1, 2]), "3 1,2,3");
    assert_eq!(ids(vec![]), "0 -");
}

#[test]
fn bmp_header_top_down() {
    let h = bmp_header(2, 3);
    assert_eq!(&h[0..2], b"BM");
    assert_eq!(u32::from_le_bytes(h[2..6].try_into().unwrap()), 54 + 24);
    assert_eq!(i32::from_le_bytes(h[22..26].try_into().unwrap()), -3);
    assert_eq!((h[10], h[14], h[26], h[28]), (54, 40, 1, 32));
}

#[test]
fn text_mode_new_lines() {
    let expected: &[u8] = if cfg!(windows) { b"a\r\nb\r\n" } else { b"a\nb\n" };
    assert_eq!(text_mode("a\nb\n"), expected);
}
