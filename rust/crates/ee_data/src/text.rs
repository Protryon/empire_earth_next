//! The text forms' grammar (tools/eedata textio.py, scalars.py; the C++ `EEDataText.cpp` before it): the strict YAML
//! subset, CSV and the scalars, all over raw bytes. Every rule and message is the C++ compiler's, checked byte for
//! byte by `eedata.py cppgate` and pinned by this module's tests.

use crate::msg;
use crate::node::{Kind, Node};
use crate::spec::Type;
use std::collections::{BTreeMap, HashMap};
use std::rc::Rc;
use std::sync::OnceLock;

/// A problem in a text, with its place ("combat line 12: minimum: 'x' is not an integer").
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Failure {
    pub text: Vec<u8>,
}

impl Failure {
    pub fn new(text: Vec<u8>) -> Failure {
        Failure { text }
    }
}

pub type Res<T> = Result<T, Failure>;

#[macro_export]
macro_rules! fail {
    ($($part:expr),+ $(,)?) => { return Err($crate::text::Failure::new($crate::msg!($($part),+))) };
}

/// `strchr(set, c)` for a NUL-terminated `set`: a NUL byte matches the terminator (`s[i-1]` of a text with embedded
/// NUL behaves as the C++ does).
fn strchr_set(set: &[u8], c: u8) -> bool {
    c == 0 || set.contains(&c)
}

// -------------------------------------------------------------------------------------------- helpers

/// The right-trimmed text (`RTrim`), by default of blanks and tabs.
pub fn rtrim<'a>(s: &'a [u8], chars: &[u8]) -> &'a [u8] {
    let mut e = s.len();
    while e > 0 && chars.contains(&s[e - 1]) {
        e -= 1;
    }
    &s[..e]
}

/// The text with leading and trailing blanks and tabs removed (`Trim`).
pub fn trim(s: &[u8]) -> &[u8] {
    let mut b = 0;
    while b < s.len() && (s[b] == b' ' || s[b] == b'\t') {
        b += 1;
    }
    if b == s.len() {
        return &[];
    }
    let mut e = s.len();
    while s[e - 1] == b' ' || s[e - 1] == b'\t' {
        e -= 1;
    }
    &s[b..e]
}

fn find(s: &[u8], b: u8) -> Option<usize> {
    s.iter().position(|&x| x == b)
}

// ------------------------------------------------------------------------------------------- YAML subset

struct Line {
    indent: i64,
    content: Vec<u8>,
    number: i64,
}

fn strip_comment(s: &[u8]) -> &[u8] {
    if s.first() == Some(&b'#') {
        return &[];
    }
    let mut q = 0u8;
    let mut i = 0;
    while i < s.len() {
        let c = s[i];
        if q == b'"' {
            if c == b'\\' {
                i += 1;
            } else if c == b'"' {
                q = 0;
            }
        } else if q == b'\'' {
            if c == b'\'' {
                q = 0;
            }
        } else if (c == b'"' || c == b'\'') && (i == 0 || strchr_set(b" [,:-", s[i - 1])) {
            q = c;
        } else if c == b'#' && i > 0 && (s[i - 1] == b' ' || s[i - 1] == b'\t') {
            return &s[..i];
        }
        i += 1;
    }
    s
}

const BOM: &[u8] = b"\xEF\xBB\xBF";

fn split_lines(text: &[u8]) -> Res<Vec<Line>> {
    let mut out: Vec<Line> = Vec::new();
    let mut p = if text.starts_with(BOM) { 3 } else { 0 };
    let mut no = 0i64;
    while p <= text.len() {
        let e = find(&text[p..], b'\n').map_or(text.len(), |x| x + p);
        let mut raw = &text[p..e];
        p = e + 1;
        no += 1;
        if raw.last() == Some(&b'\r') {
            raw = &raw[..raw.len() - 1];
        }
        let Some(ind) = raw.iter().position(|&x| x != b' ') else { continue };
        if raw[ind] == b'\t' {
            fail!("line ", no, ": tab in indentation");
        }
        let content = rtrim(strip_comment(&raw[ind..]), b" \t");
        if content.is_empty() {
            continue;
        }
        if content == b"---" && out.is_empty() {
            continue;
        }
        if content == b"---" || content == b"..." {
            fail!("line ", no, ": only one document per file");
        }
        out.push(Line { indent: ind as i64, content: content.to_vec(), number: no });
        if e == text.len() {
            break;
        }
    }
    Ok(out)
}

fn append_utf8(out: &mut Vec<u8>, cp: u32) {
    if cp < 0x80 {
        out.push(cp as u8);
    } else if cp < 0x800 {
        out.push((0xc0 | (cp >> 6)) as u8);
        out.push((0x80 | (cp & 0x3f)) as u8);
    } else if cp < 0x10000 {
        out.push((0xe0 | (cp >> 12)) as u8);
        out.push((0x80 | ((cp >> 6) & 0x3f)) as u8);
        out.push((0x80 | (cp & 0x3f)) as u8);
    } else {
        out.push((0xf0 | (cp >> 18)) as u8);
        out.push((0x80 | ((cp >> 12) & 0x3f)) as u8);
        out.push((0x80 | ((cp >> 6) & 0x3f)) as u8);
        out.push((0x80 | (cp & 0x3f)) as u8);
    }
}

fn hex_digit(c: u8) -> i32 {
    match c {
        b'0'..=b'9' => (c - b'0') as i32,
        b'a'..=b'f' => (c - b'a' + 10) as i32,
        b'A'..=b'F' => (c - b'A' + 10) as i32,
        _ => -1,
    }
}

/// s[*i] is a quote -> the text; *i is left after the closing quote.
fn parse_quoted(s: &[u8], i: &mut usize, no: i64) -> Res<Vec<u8>> {
    let q = s[*i];
    *i += 1;
    let mut out = Vec::new();
    loop {
        if *i >= s.len() {
            fail!("line ", no, ": unterminated string");
        }
        let c = s[*i];
        if q == b'\'' {
            if c == b'\'' {
                if *i + 1 < s.len() && s[*i + 1] == b'\'' {
                    out.push(b'\'');
                    *i += 2;
                    continue;
                }
                *i += 1;
                return Ok(out);
            }
            out.push(c);
            *i += 1;
            continue;
        }
        if c == b'"' {
            *i += 1;
            return Ok(out);
        }
        if c == b'\\' {
            let e = if *i + 1 < s.len() { s[*i + 1] } else { 0 };
            const SIMPLE: &[u8] = b"\\\"/0abtnvfre ";
            const VALUE: &[u8] = b"\\\"/\0\x07\x08\t\n\x0b\x0c\r\x1b ";
            if e != 0 && SIMPLE.contains(&e) {
                let k = SIMPLE.iter().position(|&x| x == e).unwrap();
                out.push(VALUE[k]);
                *i += 2;
            } else if e == b'x' || e == b'u' || e == b'U' {
                let n: usize = if e == b'x' {
                    2
                } else if e == b'u' {
                    4
                } else {
                    8
                };
                if *i + 2 + n > s.len() {
                    fail!("line ", no, ": bad \\", e, " escape");
                }
                let mut cp: u64 = 0;
                for k in 0..n {
                    let d = hex_digit(s[*i + 2 + k]);
                    if d < 0 {
                        fail!("line ", no, ": bad \\", e, " escape");
                    }
                    cp = cp * 16 + d as u64;
                }
                if cp > 0x10ffff {
                    fail!("line ", no, ": escape out of range");
                }
                append_utf8(&mut out, cp as u32);
                *i += 2 + n;
            } else {
                fail!("line ", no, ": unknown escape \\", e);
            }
            continue;
        }
        out.push(c);
        *i += 1;
    }
}

fn truncated(t: &[u8], n: usize) -> &[u8] {
    &t[..t.len().min(n)]
}

fn check_plain(t: &[u8], no: i64) -> Res<()> {
    if t.is_empty() {
        return Ok(());
    }
    if strchr_set(b"&*!|>%@`{}", t[0]) {
        fail!(
            "line ",
            no,
            ": ",
            truncated(t, 20),
            ": anchors, aliases, tags, block scalars and flow mappings are not allowed"
        );
    }
    if t.windows(2).any(|w| w == b": ") || t.last() == Some(&b':') {
        fail!("line ", no, ": ", truncated(t, 30), ": a mapping inside a value (quote the string)");
    }
    Ok(())
}

fn parse_flow(s: &[u8], i: &mut usize, no: i64) -> Res<Node> {
    let mut out = Node::collection(Kind::List, no);
    *i += 1;
    let mut expect_item = true;
    loop {
        while *i < s.len() && s[*i] == b' ' {
            *i += 1;
        }
        if *i >= s.len() {
            fail!("line ", no, ": unterminated [ (flow sequences must close on their line)");
        }
        let c = s[*i];
        if c == b']' {
            if expect_item && !out.items.is_empty() {
                fail!("line ", no, ": trailing comma in a flow sequence");
            }
            *i += 1;
            return Ok(out);
        }
        if !expect_item {
            if c != b',' {
                fail!("line ", no, ": expected , or ] in a flow sequence");
            }
            *i += 1;
            expect_item = true;
            continue;
        }
        if c == b'[' {
            out.items.push(Rc::new(parse_flow(s, i, no)?));
        } else if c == b'"' || c == b'\'' {
            let t = parse_quoted(s, i, no)?;
            out.items.push(Rc::new(Node::scalar(t, true, no)));
        } else {
            let mut j = *i;
            while j < s.len() && s[j] != b',' && s[j] != b']' {
                j += 1;
            }
            let t = trim(&s[*i..j]);
            if t.is_empty() {
                fail!("line ", no, ": empty item in a flow sequence");
            }
            if t[0] == b'{' || t[0] == b'[' {
                fail!("line ", no, ": flow mappings are not allowed");
            }
            check_plain(t, no)?;
            out.items.push(Rc::new(Node::scalar(t.to_vec(), false, no)));
            *i = j;
        }
        expect_item = false;
    }
}

fn parse_value(s: &[u8], no: i64) -> Res<Node> {
    if s == b"{}" {
        return Ok(Node::collection(Kind::Map, no));
    }
    let mut i = 0;
    if s[0] == b'[' {
        let v = parse_flow(s, &mut i, no)?;
        if !trim(&s[i..]).is_empty() {
            fail!("line ", no, ": text after ]");
        }
        return Ok(v);
    }
    if s[0] == b'"' || s[0] == b'\'' {
        let t = parse_quoted(s, &mut i, no)?;
        if !trim(&s[i..]).is_empty() {
            fail!("line ", no, ": text after the closing quote");
        }
        return Ok(Node::scalar(t, true, no));
    }
    check_plain(s, no)?;
    Ok(Node::scalar(s.to_vec(), false, no))
}

/// "key: rest" / "key:" -> the key and the rest; None when s is not a mapping entry.
fn split_key(s: &[u8], no: i64) -> Res<Option<(Vec<u8>, Vec<u8>)>> {
    if s[0] == b'"' || s[0] == b'\'' {
        let mut i = 0;
        let k = parse_quoted(s, &mut i, no)?;
        if i >= s.len() || s[i] != b':' || (i + 1 < s.len() && s[i + 1] != b' ') {
            return Ok(None);
        }
        return Ok(Some((k, trim(&s[i + 1..]).to_vec())));
    }
    if s[0] == b'[' || s[0] == b'{' || (s[0] == b'-' && !(s.len() > 1 && s[1] != b' ')) {
        return Ok(None);
    }
    // ^([^:#]*?):(?: (.*))?$
    let Some(c) = find(s, b':') else { return Ok(None) };
    if let Some(h) = find(s, b'#') {
        if h < c {
            return Ok(None);
        }
    }
    if c + 1 < s.len() && s[c + 1] != b' ' {
        return Ok(None);
    }
    let k = trim(&s[..c]);
    if k.is_empty() {
        return Ok(None);
    }
    let rest = if c + 1 < s.len() { trim(&s[c + 2..]).to_vec() } else { Vec::new() };
    Ok(Some((k.to_vec(), rest)))
}

fn is_seq(c: &[u8]) -> bool {
    c == b"-" || c.starts_with(b"- ")
}

struct YamlParser<'a> {
    items: &'a [Line],
}

impl YamlParser<'_> {
    fn block(&self, i: &mut usize, indent: i64) -> Res<Node> {
        if is_seq(&self.items[*i].content) {
            return self.seq(i, indent);
        }
        self.map(i, indent, None)
    }

    fn map(&self, i: &mut usize, indent: i64, first: Option<&[u8]>) -> Res<Node> {
        let mut out = Node::collection(Kind::Map, self.items.get(*i).map_or(0, |l| l.number));
        let mut first = first;
        while *i < self.items.len() {
            let ln = &self.items[*i];
            let ind = if first.is_some() { indent } else { ln.indent };
            let content: &[u8] = first.unwrap_or(&ln.content);
            let no = ln.number;
            if first.is_none() && ind < indent {
                break;
            }
            if ind != indent {
                fail!("line ", no, ": unexpected indentation");
            }
            if first.is_none() && is_seq(content) {
                break;
            }
            first = None;
            let Some((k, rest)) = split_key(content, no)? else {
                fail!("line ", no, ": expected \"key: value\"");
            };
            if out.find(&k).is_some() {
                fail!("line ", no, ": duplicate key '", &k, "'");
            }
            *i += 1;
            out.keys.push(k);
            if !rest.is_empty() {
                out.items.push(Rc::new(parse_value(&rest, no)?));
            } else if *i < self.items.len()
                && (self.items[*i].indent > indent
                    || (self.items[*i].indent == indent && is_seq(&self.items[*i].content)))
            {
                let sub = self.items[*i].indent;
                out.items.push(Rc::new(self.block(i, sub)?));
            } else {
                out.items.push(Rc::new(Node::scalar(Vec::new(), false, no)));
            }
        }
        Ok(out)
    }

    fn seq(&self, i: &mut usize, indent: i64) -> Res<Node> {
        let mut out = Node::collection(Kind::List, self.items[*i].number);
        while *i < self.items.len() {
            let ln = &self.items[*i];
            if ln.indent < indent || (ln.indent == indent && !is_seq(&ln.content)) {
                break;
            }
            if ln.indent != indent {
                fail!("line ", ln.number, ": unexpected indentation");
            }
            let c = &ln.content;
            let rest: &[u8] = match c.iter().skip(1).position(|&x| x != b' ') {
                Some(r) => &c[r + 1..],
                None => &[],
            };
            if rest.is_empty() {
                let no = ln.number;
                *i += 1;
                if *i < self.items.len() && self.items[*i].indent > indent {
                    let sub = self.items[*i].indent;
                    out.items.push(Rc::new(self.block(i, sub)?));
                } else {
                    out.items.push(Rc::new(Node::scalar(Vec::new(), false, no)));
                }
                continue;
            }
            let sub = indent + (c.len() - rest.len()) as i64;
            if is_seq(rest) {
                fail!("line ", ln.number, ": nested \"- -\" sequences are not allowed");
            }
            if split_key(rest, ln.number)?.is_some() {
                out.items.push(Rc::new(self.map(i, sub, Some(rest))?));
                continue;
            }
            out.items.push(Rc::new(parse_value(rest, ln.number)?));
            *i += 1;
        }
        Ok(out)
    }
}

/// A whole YAML text -> its tree.
pub fn parse_yaml(text: &[u8]) -> Res<Node> {
    let items = split_lines(text)?;
    if items.is_empty() {
        return Ok(Node::collection(Kind::Map, 0));
    }
    let p = YamlParser { items: &items };
    let mut i = 0;
    let n = p.block(&mut i, items[0].indent)?;
    if i != items.len() {
        fail!("line ", items[i].number, ": unexpected indentation");
    }
    Ok(n)
}

// ------------------------------------------------------------------------------------------------- CSV

pub struct Csv {
    pub directives: BTreeMap<Vec<u8>, Vec<u8>>,
    pub header: Vec<Vec<u8>>,
    pub rows: Vec<Vec<Vec<u8>>>,
    pub row_lines: Vec<i64>,
}

pub fn parse_csv(text: &[u8]) -> Res<Csv> {
    let mut directives = BTreeMap::new();
    let mut p = if text.starts_with(BOM) { 3 } else { 0 };
    let mut no = 0i64;
    // directives and blank lines before the header
    while p < text.len() {
        let e = find(&text[p..], b'\n').map_or(text.len(), |x| x + p);
        let s = rtrim(&text[p..e], b"\r");
        let blank = trim(s).is_empty();
        if !blank && s[0] != b'#' {
            break;
        }
        no += 1;
        if !blank && s.len() > 1 && s[1] != b' ' && s[1] != b'#' {
            let sp = find(s, b' ');
            let name = s[1..sp.unwrap_or(s.len())].to_vec();
            let value = sp.map_or(Vec::new(), |x| trim(&s[x + 1..]).to_vec());
            if directives.contains_key(&name) {
                fail!("line ", no, ": directive #", &name, " twice");
            }
            directives.insert(name, value);
        }
        p = e + 1;
    }
    // RFC 4180 records (as Python's csv module reads them: a quote inside an unquoted field is a character)
    let mut rows: Vec<Vec<Vec<u8>>> = Vec::new();
    let mut starts: Vec<i64> = Vec::new();
    let mut line = no + 1;
    while p < text.len() {
        let start = line;
        let mut row: Vec<Vec<u8>> = Vec::new();
        let mut cell: Vec<u8> = Vec::new();
        let mut any = false;
        loop {
            if p < text.len() && text[p] == b'"' {
                any = true;
                p += 1;
                loop {
                    if p >= text.len() {
                        fail!("CSV: unterminated quoted field");
                    }
                    if text[p] == b'"' {
                        if p + 1 < text.len() && text[p + 1] == b'"' {
                            cell.push(b'"');
                            p += 2;
                            continue;
                        }
                        p += 1;
                        break;
                    }
                    if text[p] == b'\n' {
                        line += 1;
                    }
                    cell.push(text[p]);
                    p += 1;
                }
                if p < text.len()
                    && text[p] != b','
                    && text[p] != b'\n'
                    && !(text[p] == b'\r' && p + 1 < text.len() && text[p + 1] == b'\n')
                    && text[p] != b'\r'
                {
                    fail!("CSV: text after a closing quote");
                }
            } else {
                while p < text.len()
                    && text[p] != b','
                    && text[p] != b'\n'
                    && !(text[p] == b'\r' && (p + 1 >= text.len() || text[p + 1] == b'\n'))
                {
                    cell.push(text[p]);
                    p += 1;
                    any = true;
                }
            }
            row.push(std::mem::take(&mut cell));
            if p < text.len() && text[p] == b',' {
                p += 1;
                any = true;
                continue;
            }
            if p < text.len() && text[p] == b'\r' {
                p += 1;
            }
            if p < text.len() && text[p] == b'\n' {
                p += 1;
                line += 1;
            }
            break;
        }
        if any {
            rows.push(row);
            starts.push(start);
        }
    }
    if rows.is_empty() {
        fail!("CSV: no header row");
    }
    let header = rows[0].clone();
    for i in 0..header.len() {
        for j in 0..i {
            if header[i] == header[j] {
                fail!("CSV: duplicate column names");
            }
        }
    }
    let mut out_rows = Vec::new();
    let mut row_lines = Vec::new();
    for i in 1..rows.len() {
        if rows[i].len() != header.len() {
            fail!("line ", starts[i], ": ", rows[i].len(), " cells, the header has ", header.len());
        }
        out_rows.push(std::mem::take(&mut rows[i]));
        row_lines.push(starts[i]);
    }
    Ok(Csv { directives, header, rows: out_rows, row_lines })
}

// ----------------------------------------------------------------------------------------------- scalars

fn int_range(t: Type) -> (i64, i64, u32) {
    match t {
        Type::S8 => (-128, 127, 8),
        Type::U8 | Type::Bool => (0, 255, 8),
        Type::S16 => (-32768, 32767, 16),
        Type::U16 => (0, 65535, 16),
        Type::S32 => (-2147483648, 2147483647, 32),
        _ => (0, 4294967295, 32),
    }
}

fn is_hex_int(s: &[u8]) -> bool {
    s.len() >= 3 && s[0] == b'0' && (s[1] == b'x' || s[1] == b'X') && s[2..].iter().all(|&c| hex_digit(c) >= 0)
}

/// An integer in the field's range.
pub fn parse_int(text: &[u8], t: Type) -> Res<i64> {
    let s = trim(text);
    let (lo, hi, width) = int_range(t);
    if t == Type::Bool && (s == b"true" || s == b"false") {
        return Ok((s == b"true") as i64);
    }
    if is_hex_int(s) {
        let mut v: u64 = 0;
        for &c in &s[2..] {
            v = v.wrapping_mul(16).wrapping_add(hex_digit(c) as u64);
            if v >= (1u64 << width) {
                fail!("'", text, "' does not fit ", "the field");
            }
        }
        let mut r = v as i64;
        if lo < 0 && r > hi {
            r -= 1i64 << width;
        }
        return Ok(r);
    }
    let mut i = 0;
    let mut neg = false;
    if i < s.len() && (s[i] == b'+' || s[i] == b'-') {
        neg = s[i] == b'-';
        i += 1;
    }
    if i >= s.len() {
        fail!("'", text, "' is not an integer");
    }
    let mut v: i64 = 0;
    while i < s.len() {
        if !s[i].is_ascii_digit() {
            fail!("'", text, "' is not an integer");
        }
        v = v * 10 + (s[i] - b'0') as i64;
        if v > 5000000000 {
            fail!("'", text, "' out of range");
        }
        i += 1;
    }
    if neg {
        v = -v;
    }
    if v < lo || v > hi {
        fail!("'", text, "' out of range");
    }
    Ok(v)
}

fn is_decimal(s: &[u8]) -> bool {
    // ^[+-]?([0-9]+\.?[0-9]*|\.[0-9]+)([eE][+-]?[0-9]+)?$
    let (mut i, n) = (0, s.len());
    if i < n && (s[i] == b'+' || s[i] == b'-') {
        i += 1;
    }
    let d0 = i;
    while i < n && s[i].is_ascii_digit() {
        i += 1;
    }
    let int_digits = i > d0;
    if i < n && s[i] == b'.' {
        i += 1;
        let f0 = i;
        while i < n && s[i].is_ascii_digit() {
            i += 1;
        }
        if !int_digits && i == f0 {
            return false;
        }
    } else if !int_digits {
        return false;
    }
    if i < n && (s[i] == b'e' || s[i] == b'E') {
        i += 1;
        if i < n && (s[i] == b'+' || s[i] == b'-') {
            i += 1;
        }
        let e0 = i;
        while i < n && s[i].is_ascii_digit() {
            i += 1;
        }
        if i == e0 {
            return false;
        }
    }
    i == n
}

/// A float32 or `.inf`/`.nan`/a hex bit pattern, as its bits (correctly rounded).
pub fn parse_f32(text: &[u8]) -> Res<u32> {
    let s = trim(text);
    let low: Vec<u8> = s.iter().map(u8::to_ascii_lowercase).collect();
    match low.as_slice() {
        b".inf" | b"+.inf" | b"inf" | b"+inf" | b"infinity" => return Ok(0x7f800000),
        b"-.inf" | b"-inf" | b"-infinity" => return Ok(0xff800000),
        b".nan" | b"nan" => return Ok(0x7fc00000),
        _ => {}
    }
    if is_hex_int(s) {
        let mut v: u64 = 0;
        for &c in &s[2..] {
            v = v.wrapping_mul(16).wrapping_add(hex_digit(c) as u64);
            if v >> 32 != 0 {
                fail!("'", text, "': more than 32 bits");
            }
        }
        return Ok(v as u32);
    }
    if !is_decimal(s) {
        fail!("'", text, "' is not a number");
    }
    let neg = s[0] == b'-';
    let body = if s[0] == b'+' || s[0] == b'-' { &s[1..] } else { s };
    // f32::from_str is correctly rounded, as std::from_chars is; its overflow and underflow results (infinity, zero)
    // are caught below as from_chars' out-of-range. is_decimal checked the syntax.
    let f: f32 = std::str::from_utf8(body).expect("ASCII").parse().expect("is_decimal checked the syntax");
    let bits = f.to_bits();
    if bits == 0x7f800000 {
        fail!("'", text, "' is outside the float range");
    }
    if bits == 0 {
        for &c in body {
            if (b'1'..=b'9').contains(&c) {
                fail!("'", text, "' is outside the float range");
            }
            if c == b'e' || c == b'E' {
                break;
            }
        }
    }
    Ok(if neg { bits | 0x80000000 } else { bits })
}

fn code_page_inverse() -> &'static HashMap<u16, u8> {
    static MAP: OnceLock<HashMap<u16, u8>> = OnceLock::new();
    MAP.get_or_init(|| {
        let mut m = HashMap::new();
        for b in 0..256usize {
            m.insert(crate::generated::specs_gen::CODE_PAGE[b], b as u8);
        }
        m
    })
}

/// UTF-8 text -> CP-1252 bytes.
pub fn encode_text(text: &[u8]) -> Res<Vec<u8>> {
    let inverse = code_page_inverse();
    let mut out = Vec::with_capacity(text.len());
    let mut i = 0;
    while i < text.len() {
        let c = text[i];
        let (mut cp, n): (u32, usize) = if c < 0x80 {
            (c as u32, 1)
        } else if c & 0xe0 == 0xc0 {
            ((c & 0x1f) as u32, 2)
        } else if c & 0xf0 == 0xe0 {
            ((c & 0x0f) as u32, 3)
        } else if c & 0xf8 == 0xf0 {
            ((c & 0x07) as u32, 4)
        } else {
            fail!("not UTF-8 text");
        };
        if i + n > text.len() {
            fail!("not UTF-8 text");
        }
        for k in 1..n {
            let d = text[i + k];
            if d & 0xc0 != 0x80 {
                fail!("not UTF-8 text");
            }
            cp = (cp << 6) | (d & 0x3f) as u32;
        }
        i += n;
        match cp.try_into().ok().and_then(|cp16: u16| inverse.get(&cp16)) {
            Some(b) => out.push(*b),
            None => fail!("character U+", cp as i64, " has no CP-1252 byte"),
        }
    }
    Ok(out)
}

/// CP-1252 bytes -> a double-quoted YAML scalar (UTF-8): what `encode_text` reads back.
pub fn quote_text(bytes: &[u8]) -> Vec<u8> {
    let mut out = vec![b'"'];
    for &b in bytes {
        let cp = crate::generated::specs_gen::CODE_PAGE[b as usize] as u32;
        if cp == b'"' as u32 || cp == b'\\' as u32 {
            out.push(b'\\');
            out.push(cp as u8);
        } else if cp < 0x20 {
            const HEX: &[u8; 16] = b"0123456789abcdef";
            out.extend_from_slice(b"\\x");
            out.push(HEX[(cp >> 4) as usize]);
            out.push(HEX[(cp & 15) as usize]);
        } else {
            append_utf8(&mut out, cp);
        }
    }
    out.push(b'"');
    out
}

/// An id: a lowercase letter, then a-z, 0-9 or _ (tools/eedata/ids.py ID_RE).
pub fn is_id(s: &[u8]) -> bool {
    !s.is_empty()
        && s[0].is_ascii_lowercase()
        && s.iter().all(|&c| c.is_ascii_lowercase() || c.is_ascii_digit() || c == b'_')
}

// -------------------------------------------------------------------------------------------------- reading

/// A mapping read key by key; `leftover` fails on a key nobody asked for.
pub struct Getter<'a> {
    pub map: &'a Node,
    pub where_: Vec<u8>,
    used: Vec<bool>,
    cursor: usize,
}

impl<'a> Getter<'a> {
    pub fn new(map: &'a Node, where_: &[u8]) -> Getter<'a> {
        Getter { map, where_: where_.to_vec(), used: vec![false; map.keys.len()], cursor: 0 }
    }

    /// The value, or None when the key is absent or holds a null.
    pub fn get(&mut self, key: &str) -> Option<&'a Node> {
        let n = self.map.keys.len();
        for k in 0..n {
            let i = (self.cursor + k) % n;
            if self.map.keys[i] == key.as_bytes() {
                self.used[i] = true;
                self.cursor = i + 1;
                let v = &self.map.items[i];
                return if v.is_missing() { None } else { Some(v) };
            }
        }
        None
    }

    pub fn leftover(&self) -> Res<()> {
        let mut extra: Vec<u8> = Vec::new();
        let mut count = 0;
        for i in 0..self.used.len() {
            if !self.used[i] {
                if count < 5 {
                    if !extra.is_empty() {
                        extra.extend_from_slice(b", ");
                    }
                    extra.extend_from_slice(&self.map.keys[i]);
                }
                count += 1;
            }
        }
        if count > 0 {
            fail!(&self.where_, ": unknown key(s) ", &extra);
        }
        Ok(())
    }
}

/// An array value -> n element references (None = missing).
pub fn elems<'a>(v: Option<&'a Node>, n: usize, where_: &[u8], name: &str) -> Res<Vec<Option<&'a Node>>> {
    let mut out = vec![None; n];
    let Some(v) = v else { return Ok(out) };
    if v.kind != Kind::List {
        fail!(where_, ": ", name, " must be a list");
    }
    if v.items.len() > n {
        fail!(where_, ": ", name, " has ", v.items.len(), " elements, the field has ", n);
    }
    for (i, item) in v.items.iter().enumerate() {
        out[i] = if item.is_missing() { None } else { Some(&**item) };
    }
    Ok(out)
}

pub fn scalar_text<'a>(v: &'a Node, where_: &[u8], name: &str, what: &str) -> Res<&'a [u8]> {
    if v.kind != Kind::Scalar {
        fail!(where_, ": ", name, ": a list where ", what, " belongs");
    }
    Ok(&v.text)
}

/// A number field's bits: `Number` in the C++ (a missing value is 0).
pub fn number(field: &str, ty: Type, v: Option<&Node>, where_: &[u8]) -> Res<u32> {
    let Some(v) = v else { return Ok(0) };
    let attempt = (|| -> Res<u32> {
        let t = scalar_text(v, where_, field, "a number")?;
        if ty == Type::F32 {
            parse_f32(t)
        } else {
            Ok(parse_int(t, ty)? as u32)
        }
    })();
    attempt.map_err(
        |e| {
            if e.text.starts_with(where_) {
                e
            } else {
                Failure::new(msg!(where_, ": ", field, ": ", &e.text))
            }
        },
    )
}

/// A string field: empty when missing, encoded as CP-1252.
pub fn text_value(v: Option<&Node>, where_: &[u8], name: &str) -> Res<Vec<u8>> {
    let Some(v) = v else { return Ok(Vec::new()) };
    let t = scalar_text(v, where_, name, "a string")?;
    encode_text(t).map_err(|e| Failure::new(msg!(where_, ": ", name, ": ", &e.text)))
}

/// A document's value that must be there: its text (`NodeScalar`).
pub fn node_scalar<'a>(v: Option<&'a Node>, where_: &[u8], name: &str) -> Res<&'a [u8]> {
    match v {
        Some(n) if !n.is_missing() => scalar_text(n, where_, name, "a value"),
        _ => {
            fail!(where_, ": ", name, ": missing");
        }
    }
}

/// A signed 32-bit integer of a document (`NodeInt`).
pub fn node_int(v: Option<&Node>, where_: &[u8], name: &str) -> Res<i64> {
    let t = node_scalar(v, where_, name)?;
    parse_int(t, Type::S32).map_err(|e| Failure::new(msg!(where_, ": ", name, ": ", &e.text)))
}

/// A float of a document (`NodeFloat`).
pub fn node_float(v: Option<&Node>, where_: &[u8], name: &str) -> Res<f32> {
    let t = node_scalar(v, where_, name)?;
    let bits = parse_f32(t).map_err(|e| Failure::new(msg!(where_, ": ", name, ": ", &e.text)))?;
    Ok(f32::from_bits(bits))
}

/// A boolean of a document (`NodeBool`): 0/1, true/false.
pub fn node_bool(v: Option<&Node>, where_: &[u8], name: &str) -> Res<bool> {
    let t = node_scalar(v, where_, name)?;
    parse_int(t, Type::Bool).map(|x| x != 0).map_err(|e| Failure::new(msg!(where_, ": ", name, ": ", &e.text)))
}

/// A string of a document as CP-1252 bytes; a missing one is empty (`NodeText`).
pub fn node_text(v: Option<&Node>, where_: &[u8], name: &str) -> Res<Vec<u8>> {
    match v {
        Some(n) if !n.is_missing() => text_value(Some(n), where_, name),
        _ => Ok(Vec::new()),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn yaml(text: &str) -> Res<Node> {
        parse_yaml(text.as_bytes())
    }
    fn err(r: Res<Node>) -> Vec<u8> {
        r.unwrap_err().text
    }

    #[test]
    fn trim_like_the_cpp() {
        assert_eq!(trim(b"  a b\t"), b"a b");
        assert_eq!(trim(b" \t"), b"");
        assert_eq!(rtrim(b"a\r", b"\r"), b"a");
        assert_eq!(rtrim(b"\r\r", b"\r"), b"");
    }

    #[test]
    fn comments_stop_at_blank_hash_and_respect_quotes() {
        assert_eq!(strip_comment(b"a: 1 # no"), b"a: 1 ");
        assert_eq!(strip_comment(b"# no"), b"");
        assert_eq!(strip_comment(b"a#b"), b"a#b");
        assert_eq!(strip_comment(b"x: \"a # b\" # c"), b"x: \"a # b\" ");
        assert_eq!(strip_comment(b"x: 'a # b' # c"), b"x: 'a # b' ");
        // a quote only opens after one of " [,:-" (or a NUL, as strchr matches the terminator)
        assert_eq!(strip_comment(b"it's # c"), b"it's ");
    }

    #[test]
    fn yaml_scalars_maps_and_lists() {
        let n = yaml("a: 1\nb:\n  c: [1, 'x', \"y\"]\n  d:\n    - 3\n    - e: 4\n      f: 5\n").unwrap();
        assert_eq!(n.kind, Kind::Map);
        assert_eq!(n.find(b"a").unwrap().text, b"1");
        let b = n.find(b"b").unwrap();
        let c = b.find(b"c").unwrap();
        assert_eq!(c.kind, Kind::List);
        assert_eq!(c.items[1].text, b"x");
        assert!(c.items[1].quoted);
        let d = b.find(b"d").unwrap();
        assert_eq!(d.items[0].text, b"3");
        assert_eq!(d.items[1].find(b"f").unwrap().text, b"5");
        assert_eq!(d.items[1].line, 6);
    }

    #[test]
    fn yaml_quoting_and_escapes() {
        let n = yaml(
            r#"a: "x\t\u00e9\x41\"\\"
b: 'it''s'
"#,
        )
        .unwrap();
        assert_eq!(n.find(b"a").unwrap().text, b"x\t\xC3\xA9A\"\\".to_vec());
        assert_eq!(n.find(b"b").unwrap().text, b"it's");
    }

    #[test]
    fn yaml_errors_name_the_line() {
        assert_eq!(err(yaml("a: 1\n\tb: 2\n")), b"line 2: tab in indentation".to_vec());
        assert_eq!(err(yaml("a: 1\na: 2\n")), b"line 2: duplicate key 'a'".to_vec());
        assert_eq!(err(yaml("a: 1\n b: 2\n")), b"line 2: unexpected indentation".to_vec());
        assert_eq!(
            err(yaml("a: [1, 2\n")),
            b"line 1: unterminated [ (flow sequences must close on their line)".to_vec()
        );
        assert_eq!(err(yaml("a: [1,]\n")), b"line 1: trailing comma in a flow sequence".to_vec());
        assert_eq!(err(yaml("a: \"x\n")), b"line 1: unterminated string".to_vec());
        assert_eq!(err(yaml("a: \"\\q\"\n")), b"line 1: unknown escape \\q".to_vec());
        assert!(err(yaml("a: &b 3\n")).starts_with(b"line 1: &b 3: anchors"));
        assert!(err(yaml("a: x: y\n")).starts_with(b"line 1: x: y: a mapping inside a value"));
        assert!(err(yaml("- - a\n")).starts_with(b"line 1: nested"));
        assert!(err(yaml("a: {b: 1}\n")).starts_with(b"line 1: {b: 1}: anchors"));
        assert_eq!(err(yaml("---\na: 1\n---\nb: 2\n")), b"line 3: only one document per file".to_vec());
    }

    #[test]
    fn yaml_empty_document_is_an_empty_map() {
        let n = yaml("# nothing\n\n").unwrap();
        assert_eq!(n.kind, Kind::Map);
        assert!(n.keys.is_empty());
        assert_eq!(yaml("---\na: 1\n").unwrap().keys.len(), 1);
    }

    #[test]
    fn yaml_bom_and_crlf() {
        let n = parse_yaml(b"\xEF\xBB\xBFa: 1\r\nb: 2\r\n").unwrap();
        assert_eq!(n.keys.len(), 2);
        assert_eq!(n.find(b"b").unwrap().line, 2);
    }

    #[test]
    fn csv_records_quotes_and_directives() {
        let c = parse_csv(b"#eedata t\n#format 3\n\nid,x\na,\"1,5\"\n,\"q\"\"r\"\n").unwrap();
        assert_eq!(c.directives.get(&b"eedata"[..]).map(Vec::as_slice), Some(&b"t"[..]));
        assert_eq!(c.header, vec![b"id".to_vec(), b"x".to_vec()]);
        assert_eq!(c.rows[0], vec![b"a".to_vec(), b"1,5".to_vec()]);
        assert_eq!(c.rows[1], vec![b"".to_vec(), b"q\"r".to_vec()]);
        assert_eq!(c.row_lines, vec![5, 6]);
    }

    #[test]
    fn csv_crlf_and_blank_lines_are_skipped() {
        let c = parse_csv(b"id,x\r\n\r\na,1\r\n").unwrap();
        assert_eq!(c.rows.len(), 1);
        assert_eq!(c.rows[0][1], b"1");
    }

    #[test]
    fn csv_errors() {
        assert_eq!(parse_csv(b"").err().unwrap().text, b"CSV: no header row".to_vec());
        assert_eq!(parse_csv(b"a,a\n").err().unwrap().text, b"CSV: duplicate column names".to_vec());
        assert_eq!(parse_csv(b"a,b\n1\n").err().unwrap().text, b"line 2: 1 cells, the header has 2".to_vec());
        assert_eq!(parse_csv(b"a\n\"x").err().unwrap().text, b"CSV: unterminated quoted field".to_vec());
        assert_eq!(parse_csv(b"#x 1\n#x 2\na\n").err().unwrap().text, b"line 2: directive #x twice".to_vec());
    }

    #[test]
    fn integers_as_the_cpp_parses_them() {
        assert_eq!(parse_int(b" 007 ", Type::S32).unwrap(), 7);
        assert_eq!(parse_int(b"-2147483648", Type::S32).unwrap(), -2147483648);
        assert_eq!(parse_int(b"0xffffffff", Type::U32).unwrap(), 0xffffffff);
        assert_eq!(parse_int(b"0xffffffff", Type::S32).unwrap(), -1);
        assert_eq!(parse_int(b"0X1F", Type::S32).unwrap(), 31);
        assert_eq!(parse_int(b"true", Type::Bool).unwrap(), 1);
        assert_eq!(parse_int(b"+5", Type::U8).unwrap(), 5);
        assert_eq!(parse_int(b"-0", Type::S8).unwrap(), 0);
    }

    #[test]
    fn integer_errors() {
        assert_eq!(parse_int(b"x", Type::S32).unwrap_err().text, b"'x' is not an integer".to_vec());
        assert_eq!(parse_int(b"256", Type::U8).unwrap_err().text, b"'256' out of range".to_vec());
        assert_eq!(parse_int(b"5000000001", Type::S32).unwrap_err().text, b"'5000000001' out of range".to_vec());
        assert_eq!(parse_int(b"0x100", Type::U8).unwrap_err().text, b"'0x100' does not fit the field".to_vec());
        assert_eq!(parse_int(b"true", Type::U8).unwrap_err().text, b"'true' is not an integer".to_vec());
        assert_eq!(parse_int(b"1e3", Type::S32).unwrap_err().text, b"'1e3' is not an integer".to_vec());
        assert_eq!(parse_int(b"", Type::S32).unwrap_err().text, b"'' is not an integer".to_vec());
    }

    // golden bits: what the C++ compiler (std::from_chars) gives for these strings (cppgate's float sweep)
    #[test]
    fn floats_golden_bits() {
        let golden: &[(&[u8], u32)] = &[
            (b"0.1", 0x3dcccccd),
            (b"1e2", 0x42c80000),
            (b" 3.5 ", 0x40600000),
            (b"+2", 0x40000000),
            (b".5", 0x3f000000),
            (b"5.", 0x40a00000),
            (b"-0", 0x80000000),
            (b"-0.0", 0x80000000),
            (b"0x3f800000", 0x3f800000),
            (b"0.1000000001", 0x3dcccccd),
            (b"16777217", 0x4b800000),
            (b"1.17549435e-38", 0x00800000),
            (b"1.4e-45", 0x00000001),
            (b"3.4028235e38", 0x7f7fffff),
            (b"1.5e-7", 0x34210fb0),
            (b".inf", 0x7f800000),
            (b"-.inf", 0xff800000),
            (b".nan", 0x7fc00000),
            (b"0x7fc00001", 0x7fc00001),
        ];
        for (text, bits) in golden {
            assert_eq!(parse_f32(text).unwrap(), *bits, "{:?}", String::from_utf8_lossy(text));
        }
    }

    #[test]
    fn float_errors() {
        assert_eq!(parse_f32(b"1e39").unwrap_err().text, b"'1e39' is outside the float range".to_vec());
        assert_eq!(parse_f32(b"1e-50").unwrap_err().text, b"'1e-50' is outside the float range".to_vec());
        assert_eq!(parse_f32(b"7e-46").unwrap_err().text, b"'7e-46' is outside the float range".to_vec());
        assert_eq!(parse_f32(b"abc").unwrap_err().text, b"'abc' is not a number".to_vec());
        assert_eq!(parse_f32(b"1e").unwrap_err().text, b"'1e' is not a number".to_vec());
        assert_eq!(parse_f32(b"").unwrap_err().text, b"'' is not a number".to_vec());
        assert_eq!(parse_f32(b"0x100000000").unwrap_err().text, b"'0x100000000': more than 32 bits".to_vec());
        assert_eq!(parse_f32(b"0e-999").unwrap(), 0);
    }

    #[test]
    fn encode_text_maps_utf8_to_cp1252() {
        assert_eq!(encode_text("€é".as_bytes()).unwrap(), vec![0x80, 0xe9]);
        assert_eq!(encode_text(b"\xC2\x81").unwrap(), vec![0x81]); // a C1 control maps to its byte
        assert_eq!(encode_text("中".as_bytes()).unwrap_err().text, b"character U+20013 has no CP-1252 byte".to_vec());
        assert_eq!(encode_text(b"\xC3").unwrap_err().text, b"not UTF-8 text".to_vec());
        assert_eq!(encode_text(b"\xFF").unwrap_err().text, b"not UTF-8 text".to_vec());
    }

    #[test]
    fn quote_text_round_trips() {
        assert_eq!(quote_text(b"a\"b\\c\x01\xe9"), "\"a\\\"b\\\\c\\x01é\"".as_bytes().to_vec());
    }

    #[test]
    fn ids() {
        assert!(is_id(b"a_1"));
        assert!(!is_id(b"1a"));
        assert!(!is_id(b""));
        assert!(!is_id(b"aB"));
    }

    #[test]
    fn getter_reads_and_reports_unknown_keys() {
        let n = yaml("a: 1\nb: 2\nc: ~\n").unwrap();
        let mut g = Getter::new(&n, b"where");
        assert_eq!(g.get("b").unwrap().text, b"2");
        assert!(g.get("c").is_none()); // a null is absent, but used
        assert_eq!(g.get("a").unwrap().text, b"1");
        assert!(g.leftover().is_ok());
        let mut g = Getter::new(&n, b"where");
        g.get("a");
        assert_eq!(g.leftover().unwrap_err().text, b"where: unknown key(s) b, c".to_vec());
    }

    #[test]
    fn elems_checks_shape() {
        let n = yaml("a: [1, ~, 3]\nb: 7\n").unwrap();
        let v = elems(n.find(b"a").map(|r| &**r), 4, b"w", "a").unwrap();
        assert_eq!(v.len(), 4);
        assert!(v[1].is_none());
        assert_eq!(v[2].unwrap().text, b"3");
        assert!(v[3].is_none());
        assert_eq!(
            elems(n.find(b"a").map(|r| &**r), 2, b"w", "a").unwrap_err().text,
            b"w: a has 3 elements, the field has 2".to_vec()
        );
        assert_eq!(elems(n.find(b"b").map(|r| &**r), 2, b"w", "b").unwrap_err().text, b"w: b must be a list".to_vec());
        assert_eq!(elems(None, 2, b"w", "a").unwrap(), vec![None, None]);
    }

    #[test]
    fn number_prefixes_a_bare_parse_error() {
        let n = yaml("a: x\n").unwrap();
        let v = n.find(b"a").unwrap();
        assert_eq!(number("a", Type::S32, Some(v), b"w").unwrap_err().text, b"w: a: 'x' is not an integer".to_vec());
        assert_eq!(number("a", Type::F32, None, b"w").unwrap(), 0);
        let list = yaml("a: [1]\n").unwrap();
        assert_eq!(
            number("a", Type::S32, Some(list.find(b"a").unwrap()), b"w").unwrap_err().text,
            b"w: a: a list where a number belongs".to_vec()
        );
    }

    #[test]
    fn node_readers() {
        let n = yaml("a: 5\nb: 1.5\nc: true\nd: ~\n").unwrap();
        let get = |k: &[u8]| n.find(k).map(|r| &**r);
        assert_eq!(node_int(get(b"a"), b"w", "a").unwrap(), 5);
        assert_eq!(node_float(get(b"b"), b"w", "b").unwrap(), 1.5);
        assert!(node_bool(get(b"c"), b"w", "c").unwrap());
        assert_eq!(node_int(get(b"d"), b"w", "d").unwrap_err().text, b"w: d: missing".to_vec());
        assert_eq!(node_int(get(b"b"), b"w", "b").unwrap_err().text, b"w: b: '1.5' is not an integer".to_vec());
        assert_eq!(node_text(None, b"w", "x").unwrap(), Vec::<u8>::new());
    }
}
