use super::*;

const WS: usize = 2;

fn new_buf() -> Box<EeWonGuiStrBuf> {
    Box::new(EeWonGuiStrBuf::new())
}

fn state(b: &EeWonGuiStrBuf) -> String {
    if b.text.is_null() {
        return format!("{} {} -", b.length, b.capacity);
    }
    let u: Vec<String> = (0..=usize::from(b.length)).map(|i| format!("{:x}", b.unit(i, WS))).collect();
    format!("{} {} {}", b.length, b.capacity, u.join(","))
}

fn units(s: &str) -> Vec<u32> {
    s.encode_utf16().map(u32::from).collect()
}

fn op(o: EeWonGstrOp, b: &mut EeWonGuiStrBuf, other: *const c_void, a: i32, bb: i32, c: i32) -> i32 {
    // SAFETY: live blocks of this module
    unsafe { ee_won_gstr(o, b, other, a, bb, c, WS as u32) }
}

#[test]
fn reserve_keeps_the_terminator_and_doubles_when_growing() {
    let mut b = new_buf();
    b.resize(3, false, WS);
    assert_eq!((b.length, b.capacity), (3, 4));
    b.assign_units(&units("abc"), WS);
    b.reserve(5, true, WS);
    assert_eq!(b.capacity, 12, "(5 + 1) * 2");
    assert_eq!(b.units(WS), units("abc"));
    assert_eq!(b.unit(3, WS), 0);
    // a smaller reservation keeps the buffer
    let p = b.text;
    b.reserve(2, true, WS);
    assert_eq!(b.text, p);
    b.free();
}

#[test]
fn the_capacity_wraps_as_an_unsigned_short() {
    let mut b = new_buf();
    b.reserve(40_000, true, WS);
    assert_eq!(b.capacity, ((40_001 * 2) & 0xffff) as u16);
    // the real allocation still holds the units
    b.resize(30_000, false, WS);
    assert_eq!(b.length, 30_000);
    b.free();
}

#[test]
fn erase_and_insert_rules() {
    let mut b = new_buf();
    b.assign_units(&units("hello"), WS);
    b.erase(6, 1, WS);
    assert_eq!(b.units(WS), units("hello"), "a start past the length does nothing");
    b.erase(-1, 1, WS);
    assert_eq!(b.units(WS), units("hello"));
    b.erase(1, 0, WS);
    assert_eq!(b.units(WS), units("h"), "length 0 erases the rest");
    let mut o = new_buf();
    o.assign_units(&units("ey"), WS);
    // SAFETY: live blocks
    unsafe { insert(&mut *b, &*o, 1, WS) };
    assert_eq!(b.units(WS), units("hey"));
    unsafe { insert(&mut *b, &*o, 9, WS) };
    assert_eq!(b.units(WS), units("hey"), "past the length: nothing");
    b.free();
    o.free();
}

#[test]
fn set_at_and_append() {
    let mut b = new_buf();
    b.append_unit(i32::from(b'x'), WS);
    b.append_unit(i32::from(b'y'), WS);
    assert_eq!(b.units(WS), units("xy"));
    assert_eq!(b.capacity, 4, "the first append: (1 + 1) * 2, kept by the second");
    assert_eq!(b.set_at(1, 0x1_0041, WS), 0x1_0041);
    assert_eq!(b.unit(1, WS), 0x41, "set as an unsigned short");
    assert_eq!(b.set_at(2, i32::from(b'z'), WS), i32::from(b'z'));
    assert_eq!(b.unit(2, WS), 0, "at the length: nothing");
    b.free();
}

#[test]
fn the_narrow_assignment_is_utf8() {
    let mut b = new_buf();
    b.assign_narrow("\u{e9}t\u{263a}".as_bytes(), WS);
    assert_eq!(b.units(WS), vec![0xe9, u32::from(b't'), 0x263a]);
    b.assign_narrow(b"\xffa", WS);
    assert_eq!(b.units(WS), vec![0xfffd, u32::from(b'a')]);
    b.assign_narrow(b"", WS);
    assert_eq!((b.length, b.units(WS)), (0, vec![]));
    b.free();
}

#[test]
fn four_byte_units() {
    let mut b = new_buf();
    b.assign_units(&[0x1f600, 0x41], 4);
    b.fill(i32::from(b'-'), 2, 2, 4);
    assert_eq!(b.units(4), vec![0x1f600, 0x41, 0x2d, 0x2d]);
    b.free();
}

// ---- the golden: tests/won/won_guistrobj_test.cpp's first 300 sequences over the original's statements

const GOLDEN: &str = include_str!("golden_guistrobj.txt");
const WIDE: &[&str] = &["", "a", "hello", "\u{e9}t\u{e9}", "\u{263a} smile", "0123456789abcdef"];
const NARROW: &[&[u8]] = &[b"", b"x", b"abc", b"\xc3\xa9t", b"\xe2\x98\xba!", b"\xffbad", b"a longer narrow text"];

#[test]
fn golden_replay() {
    let mut s: Vec<Box<EeWonGuiStrBuf>> = Vec::new();
    let mut lines = 0;
    for line in GOLDEN.lines() {
        let (head, want) = match line.split_once(" : ") {
            Some((h, w)) => (h, Some(w)),
            None => (line, None),
        };
        let f: Vec<&str> = head.split(' ').collect();
        let n = |k: usize| -> i32 { f[k].parse().unwrap() };
        if f[1] == "start" {
            if n(2) == 0 {
                for x in s.iter_mut() {
                    x.free();
                }
                s.clear();
            }
            let mut b = new_buf();
            op(EeWonGstrOp::NewCapacity, &mut b, std::ptr::null(), n(3), 0, 0);
            s.push(b);
            continue;
        }
        let (i, j, a, bb, c) = (n(2) as usize, n(3) as usize, n(4), n(5), n(6));
        let other: *const c_void = (&*s[j] as *const EeWonGuiStrBuf).cast();
        let fresh = |o: EeWonGstrOp, other: *const c_void, a: i32, b: i32| {
            let mut x = new_buf();
            op(o, &mut x, other, a, b, 0);
            x
        };
        match f[1] {
            "newcap" | "newcopy" | "newsub" | "newwide" | "newnarrow" => {
                let x = match f[1] {
                    "newcap" => fresh(EeWonGstrOp::NewCapacity, std::ptr::null(), a, 0),
                    "newcopy" => fresh(EeWonGstrOp::NewCopy, other, 0, 0),
                    "newsub" => fresh(EeWonGstrOp::NewSub, other, a, bb),
                    "newwide" => {
                        let u: Vec<u16> = WIDE[a as usize].encode_utf16().collect();
                        fresh(EeWonGstrOp::AssignUnits, u.as_ptr().cast(), u.len() as i32, 0)
                    }
                    _ => {
                        let t = NARROW[a as usize];
                        fresh(EeWonGstrOp::AssignNarrow, t.as_ptr().cast(), t.len() as i32, 0)
                    }
                };
                s[i].free();
                s[i] = x;
            }
            "reserve" => {
                op(EeWonGstrOp::Reserve, &mut s[i], std::ptr::null(), a, bb, 0);
            }
            "resize" => {
                op(EeWonGstrOp::Resize, &mut s[i], std::ptr::null(), a, bb, 0);
            }
            "wstr" => {
                let u: Vec<u16> = WIDE[a as usize].encode_utf16().collect();
                op(EeWonGstrOp::AssignUnits, &mut s[i], u.as_ptr().cast(), bb, 0, 0);
            }
            "str" => {
                let t = NARROW[a as usize];
                op(EeWonGstrOp::AssignNarrow, &mut s[i], t.as_ptr().cast(), bb, 0, 0);
            }
            "fill" => {
                op(EeWonGstrOp::Fill, &mut s[i], std::ptr::null(), a, bb, c);
            }
            "assign" | "append" | "insert" => {
                let k = match f[1] {
                    "assign" => EeWonGstrOp::Assign,
                    "append" => EeWonGstrOp::Append,
                    _ => EeWonGstrOp::Insert,
                };
                let me: *mut EeWonGuiStrBuf = &mut *s[i];
                // SAFETY: live blocks (`other` may be `me`)
                unsafe { ee_won_gstr(k, me, other, a, bb, 0, WS as u32) };
            }
            "appendc" => {
                op(EeWonGstrOp::AppendUnit, &mut s[i], std::ptr::null(), a, 0, 0);
            }
            "erase" => {
                op(EeWonGstrOp::Erase, &mut s[i], std::ptr::null(), a, bb, 0);
            }
            "setat" => assert_eq!(op(EeWonGstrOp::SetAt, &mut s[i], std::ptr::null(), a, bb, 0), c, "{line}"),
            "substr" => {}
            other => panic!("unknown edit {other}"),
        }
        assert_eq!(state(&s[i]), want.unwrap(), "{line}");
        lines += 1;
    }
    for x in s.iter_mut() {
        x.free();
    }
    assert!(lines > 10_000, "{lines} edits");
}

// ---- the text object (R6 WON GUI round 39)

fn text_of(p: *const EeWonGstr) -> Vec<u32> {
    // SAFETY: a live text
    unsafe { data_units(p.cast()) }
}

#[test]
fn a_text_is_counted_copied_on_write_and_freed() {
    let w = |s: &str| -> Vec<u32> { s.chars().map(|c| c as u32).collect() };
    let a = gstr_from_units(&w("abc"));
    // SAFETY: the texts made here, each slot holding one reference
    unsafe {
        assert_eq!((*a).refs, 0);
        assert!((*a).vtable.is_null(), "RefCount::Release tells a text by its NULL vtable");
        gstr_ref(a);
        let mut s1 = a;
        gstr_ref(a);
        let s2 = a;
        // shared: the edit copies first; the other keeps the old text
        ee_won_gstr_edit(EeWonGstrOp::AppendUnit, &mut s1, std::ptr::null(), 'd' as i32, 0, 0);
        assert_ne!(s1, s2);
        assert_eq!(text_of(s1), w("abcd"));
        assert_eq!(text_of(s2), w("abc"));
        assert_eq!(((*s1).refs, (*s2).refs), (1, 1));
        // an append of itself reads the copy (the slot after the copy)
        let p2: *const *mut EeWonGstr = &s2;
        gstr_ref(s2);
        let mut s3 = s2;
        ee_won_gstr_edit(EeWonGstrOp::Append, &mut s3, p2.cast(), 0, 3, 0);
        assert_eq!(text_of(s3), w("abcabc"));
        // a reservation keeps the shared text
        gstr_ref(s3);
        let mut s4 = s3;
        ee_won_gstr_edit(EeWonGstrOp::Reserve, &mut s4, std::ptr::null(), 40, 0, 0);
        assert_eq!(s4, s3);
        assert!((*s4).buf.capacity >= 41);
        ee_won_gstr_edit(EeWonGstrOp::Upper, &mut s4, std::ptr::null(), 0, 0, 0);
        assert_eq!(text_of(s4), w("ABCABC"));
        assert_eq!(text_of(s3), w("abcabc"));
        assert_eq!(ee_won_gstr_read(EeWonGstrRead::CompareNoCase, s3, s4.cast(), 0, 0), 0);
        assert_eq!(ee_won_gstr_read(EeWonGstrRead::Find, s3, s1.cast(), 0, 0), -1);
        assert_eq!(ee_won_gstr_read(EeWonGstrRead::FindChar, s3, std::ptr::null(), 'c' as i32, 3), 5);
        let n = b"ABCABC\0";
        assert_eq!(ee_won_gstr_read(EeWonGstrRead::CompareNarrowNoCase, s3, n.as_ptr().cast(), 0, 0), 0);
        for s in [s1, s2, s3, s4] {
            gstr_release(s);
        }
    }
}

#[test]
fn the_empty_text_is_shared_and_held() {
    let e = ee_won_gstr_empty();
    assert_eq!(e, ee_won_gstr_empty());
    // SAFETY: the shared text
    unsafe {
        assert!((*e).refs >= 1);
        assert!(text_of(e).is_empty());
        assert_eq!((*e).buf.capacity, 1, "the empty wide string's reservation");
    }
}
