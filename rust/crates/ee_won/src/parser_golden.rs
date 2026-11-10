//! `golden_parse.txt`: the C++ StringParser's reads over `tests/won/won_parse_test.cpp`'s inputs, recorded before
//! the port. This test replays the C-locale pass (the `c` lines) and the formats line through `parser`; the list
//! reads and formats are the C++ glue's loops (StringParserCollections.cpp, StringParserLists.cpp), replayed here
//! over the same primitives. The `english` pass needs the C runtime's locale and is checked by `won_parse_test`
//! itself, which runs over the Rust port. `WStringToString` (the code page) is modelled for the inputs' units only:
//! ASCII and 0xa0-0xff as themselves, anything else '?', the text ending at a NUL (`std::string(buffer)`).

use crate::parser::{format_bool, CLocale, Narrow, Parser, Text, Wide};

const GOLDEN: &str = include_str!("golden_parse.txt");

const INPUTS: &[&str] = &[
    "",
    "  hello world  ",
    r#""quoted \"x\" \t\n\r\$\\ \x41\x4142" rest"#,
    r#"L"wide\x263a\x41 z" tail"#,
    r#""a" + "b""#,
    r#""a"+ "b" , c"#,
    r#""a" "b""#,
    "abc, def)",
    "abc  , def",
    "  -123 ",
    "0x1F",
    "0XaB",
    "0x",
    "-",
    "-0xff",
    "2147483648",
    "99999999999",
    "0xfffffffff",
    "12abc",
    "true",
    "Yes",
    "off",
    "N",
    "maybe",
    "1",
    r#""unterminated"#,
    r#""back\q""#,
    r#""end\"#,
    r#""\x""#,
    r#""\xg""#,
    "(1,2),(3)",
    "(1,2",
    "(1, x)",
    r#"("a", b), (c)"#,
    "1, 2, x",
    "1 ,2 , 3",
    "a\tb\u{b}c",
    "\u{e9}t\u{e9} \u{fc}",
    "tru\u{e9}",
    "\u{a0} x",
    "\u{263a} smile",
    "L\"\u{263a}\" x",
    "ab",
    "abc",
    "a",
    "  ,x",
    "L x",
    r#""""#,
    r#""a"+"#,
    "\u{b2}\u{b3} 1\u{b9}",
    "0x\u{b2}",
    "\"\\x4\u{b2}\"",
    "\u{e9}X, \u{ff}",
];

fn hex(bytes: &[u8]) -> String {
    if bytes.is_empty() {
        return "-".into();
    }
    bytes.iter().map(|b| format!("{b:02x}")).collect()
}

fn hex_units(units: &[u32]) -> String {
    if units.is_empty() {
        return "-".into();
    }
    units.iter().map(|u| format!("{u:04x}")).collect::<Vec<_>>().join(".")
}

fn narrow_of(units: &[u32]) -> Vec<u8> {
    units
        .iter()
        .take_while(|&&u| u != 0)
        .map(|&u| if u < 0x80 || (0xa0..=0xff).contains(&u) { u as u8 } else { b'?' })
        .collect()
}

type P<'a, T> = Parser<'a, T, CLocale>;

fn read_list<T: Text, V>(p: &mut P<'_, T>, mut one: impl FnMut(&mut P<'_, T>) -> Option<V>) -> (bool, Vec<V>) {
    let mut out = Vec::new();
    loop {
        match one(p) {
            Some(v) => out.push(v),
            None => return (false, out),
        }
        if !p.check_next_char(b',' as i32) {
            return (true, out);
        }
    }
}

fn read_list_list<T: Text, V>(
    p: &mut P<'_, T>,
    mut one: impl FnMut(&mut P<'_, T>) -> Option<V>,
) -> (bool, Vec<Vec<V>>) {
    let mut out = Vec::new();
    loop {
        if !p.check_next_char(b'(' as i32) {
            return (true, out);
        }
        let (ok, inner) = read_list(p, &mut one);
        out.push(inner);
        if !ok || !p.check_next_char(b')' as i32) {
            return (false, out);
        }
        if !p.check_next_char(b',' as i32) {
            return (true, out);
        }
    }
}

fn join(items: Vec<Vec<u8>>) -> Vec<u8> {
    items.join(&b", "[..])
}

fn join_lists(items: Vec<Vec<u8>>) -> Vec<u8> {
    join(items.into_iter().map(|i| [b"(".to_vec(), i, b")".to_vec()].concat()).collect())
}

fn int_text(v: &i32) -> Vec<u8> {
    v.to_string().into_bytes()
}

fn bool_text(v: &bool) -> Vec<u8> {
    format_bool(*v).to_vec()
}

fn run_case<T: Text>(t: &T, name: &str, lines: &mut Vec<String>) {
    let fresh = || P::new(t, &CLocale, 0);
    for (op, ws) in [("str", false), ("str-ws", true)] {
        let mut p = fresh();
        let s = p.read_string(ws);
        lines.push(format!("{name} {op}: r=1 pos={} out={}", p.pos, hex(&s)));
    }
    for (op, ws) in [("wstr", false), ("wstr-ws", true)] {
        let mut p = fresh();
        let s = p.read_wstring(ws);
        lines.push(format!("{name} {op}: r=1 pos={} out={}", p.pos, hex_units(&s)));
    }
    for op in ["int", "int64", "ushort"] {
        let mut p = fresh();
        let r = p.read_int();
        let v = match (op, r) {
            (_, None) => 7,
            ("ushort", Some(v)) => i64::from(v as u16),
            (_, Some(v)) => i64::from(v),
        };
        lines.push(format!("{name} {op}: r={} pos={} v={v}", u8::from(r.is_some()), p.pos));
    }
    {
        let mut p = fresh();
        let r = p.read_bool();
        lines.push(format!("{name} bool: r={} pos={} v={}", u8::from(r.is_some()), p.pos, u8::from(r == Some(true))));
    }
    {
        let mut p = fresh();
        let r = p.check_next_char(b',' as i32);
        lines.push(format!("{name} comma: r={} pos={}", u8::from(r), p.pos));
    }
    {
        let mut p = fresh();
        p.skip_whitespace();
        let ws = p.pos;
        p.skip_non_whitespace();
        lines.push(format!("{name} skip: ws={ws} word={} end={}", p.pos, u8::from(p.end_of_string())));
    }
    {
        let mut p = fresh();
        let r1 = p.starts_with(b"ab");
        let mut q = fresh();
        let r2 = q.is(b"ab");
        lines.push(format!("{name} starts: r={} pos={} is={} pos={}", u8::from(r1), p.pos, u8::from(r2), q.pos));
    }
    {
        let mut p = fresh();
        let (r, l) = read_list(&mut p, |p| p.read_int());
        let v = join(l.iter().map(int_text).collect());
        lines.push(format!("{name} ints: r={} pos={} n={} v={}", u8::from(r), p.pos, l.len(), hex(&v)));
    }
    {
        let mut p = fresh();
        let (r, l) = read_list(&mut p, |p| Some(p.read_string(false)));
        let v = if l.is_empty() { "-".into() } else { l.iter().map(|s| hex(s)).collect::<Vec<_>>().join("|") };
        lines.push(format!("{name} strs: r={} pos={} v={v}", u8::from(r), p.pos));
    }
    {
        let mut p = fresh();
        let (r, l) = read_list(&mut p, |p| Some(p.read_wstring(false)));
        let v = join(l.iter().map(|s| narrow_of(s)).collect());
        lines.push(format!("{name} wstrs: r={} pos={} n={} v={}", u8::from(r), p.pos, l.len(), hex(&v)));
    }
    {
        let mut p = fresh();
        let (r, l) = read_list_list(&mut p, |p| p.read_int());
        let v = join_lists(l.iter().map(|i| join(i.iter().map(int_text).collect())).collect());
        lines.push(format!("{name} intlists: r={} pos={} n={} v={}", u8::from(r), p.pos, l.len(), hex(&v)));
    }
    {
        let mut p = fresh();
        let (r, l) = read_list_list(&mut p, |p| Some(p.read_string(false)));
        let v = join_lists(l.iter().map(|i| join(i.clone())).collect());
        lines.push(format!("{name} strlists: r={} pos={} n={} v={}", u8::from(r), p.pos, l.len(), hex(&v)));
    }
    {
        let mut p = fresh();
        let (r, l) = read_list(&mut p, |p| p.read_bool());
        let v = join(l.iter().map(bool_text).collect());
        lines.push(format!("{name} bools: r={} pos={} n={} v={}", u8::from(r), p.pos, l.len(), hex(&v)));
    }
}

fn formats() -> String {
    let mut s: Vec<u8> = Vec::new();
    let mut put = |b: &[u8]| {
        s.extend_from_slice(b);
        s.push(b';');
    };
    put(format_bool(true));
    put(format_bool(false));
    for v in [0i32, -1, i32::MAX, i32::MIN] {
        put(v.to_string().as_bytes());
    }
    put(b"65535");
    put(b"-32768");
    put(b"-23"); // char 0xe9 through long
    put(b"233");
    put(b"-5");
    put(b"4000000000");
    put(b"-1234567890123");
    put(u64::MAX.to_string().as_bytes());
    put(&narrow_of(&[0x77, 0xe9]));
    put(b"x, , y");
    put(b"(True, False), ()");
    s.extend_from_slice(b"(a, b)");
    format!("formats: {}", hex(&s))
}

#[test]
fn the_c_locale_pass_and_the_formats_match_the_cpp() {
    let mut lines = Vec::new();
    for (i, input) in INPUTS.iter().enumerate() {
        let units: Vec<u32> = input.chars().map(|c| c as u32).collect();
        run_case(&Wide(&units), &format!("c w{i:02}"), &mut lines);
        if units.iter().all(|&u| u < 256) {
            let bytes: Vec<u8> = units.iter().map(|&u| u as u8).collect();
            run_case(&Narrow(&bytes), &format!("c n{i:02}"), &mut lines);
        }
    }
    lines.push(formats());
    let want: Vec<&str> = GOLDEN.lines().take(lines.len()).collect();
    assert_eq!(want.len(), lines.len());
    let mut bad = 0;
    for (got, want) in lines.iter().zip(&want) {
        if got != want {
            bad += 1;
            if bad < 10 {
                eprintln!("got  {got}\nwant {want}");
            }
        }
    }
    assert_eq!(bad, 0, "{bad} lines differ");
    assert!(GOLDEN.lines().nth(lines.len()).unwrap().starts_with("english "));
}
