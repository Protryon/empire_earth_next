//! The Lua runtime's goldens (`golden_lua.txt`): what a scratch driver over the unmodified `EECPLua.cpp` printed
//! (build/ai2_golden, git 419b4b81). Part A (the value tree, the memory format, the seed, SplitMix64) is replayed
//! here step for step with the driver's generator; part B (the state) by `lua_host`'s test.

use crate::lua_value::{decode, encode, equal, seed, splitmix, to_text, Key, OwnedKey, Value};

pub(crate) const GOLDEN: &str = include_str!("golden_lua.txt");

pub(crate) struct Gen(pub(crate) u64);

impl Gen {
    fn next(&mut self) -> u64 {
        splitmix(&mut self.0)
    }
    fn below(&mut self, n: u32) -> u32 {
        (self.next() % n as u64) as u32
    }
}

const ALPHABET: [u8; 12] = [b'a', b'b', 0, 0xff, b'_', b'Z', b'9', b' ', b'"', b'\\', b'.', 0x80];

fn gen_text(g: &mut Gen) -> Vec<u8> {
    let n = g.below(5);
    (0..n).map(|_| ALPHABET[g.below(12) as usize]).collect()
}

fn small_int(g: &mut Gen) -> i64 {
    const INTS: [i64; 10] = [0, 1, 2, 3, -1, -5, 10, i64::MIN, i64::MAX, 4];
    let i = g.below(11);
    if i < 10 {
        INTS[i as usize]
    } else {
        g.below(100) as i64
    }
}

fn gen_key(g: &mut Gen) -> OwnedKey {
    if g.below(3) == 0 {
        OwnedKey::Int(small_int(g))
    } else {
        OwnedKey::Str(gen_text(g))
    }
}

/// `EECPLuaValue::Set` on a Rust map: inserts in key order or replaces.
pub(crate) fn set(m: &mut Vec<(OwnedKey, Value)>, k: OwnedKey, v: Value) {
    match m.binary_search_by(|(x, _)| x.as_key().cmp(&k.as_key())) {
        Ok(i) => m[i].1 = v,
        Err(i) => m.insert(i, (k, v)),
    }
}

pub(crate) fn gen_value(g: &mut Gen, depth: i32) -> Value {
    match g.below(if depth >= 3 { 5 } else { 7 }) {
        0 => Value::Nil,
        1 => Value::Bool(g.below(2) == 1),
        2 => Value::Int(g.next() as i64),
        3 => Value::Str(gen_text(g)),
        4 => Value::Int(small_int(g)),
        5 => {
            let n = g.below(5);
            Value::Array((0..n).map(|_| gen_value(g, depth + 1)).collect())
        }
        _ => {
            let mut m = Vec::new();
            let n = g.below(5);
            for _ in 0..n {
                let k = gen_key(g);
                let v = gen_value(g, depth + 1);
                set(&mut m, k, v);
            }
            Value::Map(m)
        }
    }
}

pub(crate) fn text(v: &Value) -> String {
    let mut o = Vec::new();
    to_text(v, &mut o);
    String::from_utf8(o).unwrap()
}

fn hex(b: &[u8]) -> String {
    b.iter().map(|c| format!("{c:02x}")).collect()
}

fn decode_line(what: &str, b: &[u8], original: Option<&Value>) -> String {
    let mut at = 0;
    let (ok, err, val) = match decode(b, &mut at) {
        Ok(v) => (true, "none", v),
        Err(e) => (false, e, Value::Str(b"sentinel".to_vec())),
    };
    let mut s = format!("  {what} {} off={at} err={err} val={}", if ok { "ok" } else { "fail" }, text(&val));
    if let Some(o) = original {
        s += &format!(" same={}", equal(&val, o) as i32);
    }
    s
}

fn part_a() -> Vec<String> {
    let mut out = vec!["== part A".to_string()];
    let mut g = Gen(20261112);
    for c in 0..400 {
        let v = gen_value(&mut g, 0);
        let mut b = Vec::new();
        encode(&v, &mut b);
        out.push(format!("tree {c}: {}", text(&v)));
        out.push(format!("  bytes {}", hex(&b)));
        out.push(decode_line("dec", &b, Some(&v)));
        let w = gen_value(&mut g, 0);
        out.push(format!("  eq {} {} {}", equal(&v, &w) as i32, equal(&v, &v) as i32, !equal(&v, &w) as i32));
        out.push("  copyeq 1".to_string());
        let k = g.below(b.len() as u32) as usize;
        out.push(decode_line("cut", &b[..k], None));
        let mut f = b.clone();
        let p = g.below(b.len() as u32) as usize;
        f[p] ^= (1 + g.below(255)) as u8;
        out.push(decode_line("flip", &f, Some(&v)));
        let mut more = b.clone();
        more.push(g.below(256) as u8);
        out.push(decode_line("more", &more, Some(&v)));
        let x = gen_key(&mut g);
        let y = gen_key(&mut g);
        let (x, y) = (x.as_key(), y.as_key());
        out.push(format!("  key {} {} {} {}", (x < y) as i32, (y < x) as i32, (x == y) as i32, 1));
        let a = g.next() as u32;
        let p = g.next() as u32;
        out.push(format!("  seed {}", seed(a, p)));
    }
    for depth in 62..=66 {
        let mut b = Vec::new();
        for _ in 0..depth {
            b.extend_from_slice(&[5, 1, 0, 0, 0, 3, 1, 0, 0, 0, 0, 0, 0, 0]);
        }
        b.push(2);
        out.push(decode_line(&format!("depth{depth}"), &b, None));
    }
    let hand = [
        "",
        "06",
        "05",
        "0500000000",
        "050100000002",
        "0501000000020102",
        "05010000000401000000",
        "0501000000040100000061",
        "050100000004010000006100",
        "05020000000301000000000000000203010000000000000002",
        "05020000000302000000000000000203010000000000000002",
        "0400000000",
        "04ffffffff",
        "05ffffffff",
        "03010203",
        "0501000000000102",
    ];
    for h in hand {
        let full = h;
        let b: Vec<u8> =
            (0..full.len() / 2).map(|i| u8::from_str_radix(&full[2 * i..2 * i + 2], 16).unwrap()).collect();
        out.push(decode_line(if full.is_empty() { "(empty)" } else { full }, &b, None));
    }
    for s in [0u64, 1, 0x123456789abcdef, !0, seed(1234, 2)] {
        let mut st = s;
        let draws: Vec<String> = (0..6).map(|_| splitmix(&mut st).to_string()).collect();
        out.push(format!("splitmix {s}: {} state {st}", draws.join(" ")));
    }
    out
}

#[test]
fn part_a_as_the_cpp() {
    let want: Vec<&str> = GOLDEN.lines().take_while(|l| *l != "== part B").collect();
    let got = part_a();
    assert_eq!(got.len(), want.len());
    for (i, (g, w)) in got.iter().zip(&want).enumerate() {
        assert_eq!(g, w, "line {}", i + 1);
    }
}

#[test]
fn key_equality_is_the_order() {
    let mut g = Gen(5);
    for _ in 0..2000 {
        let (a, b) = (gen_key(&mut g), gen_key(&mut g));
        let (x, y): (Key, Key) = (a.as_key(), b.as_key());
        assert_eq!(x == y, !(x < y) && !(y < x));
    }
}
