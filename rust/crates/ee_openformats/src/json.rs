//! The strict JSON value reader of GEOpenFormats.cpp (`JParser`): the EE_model extension and material extras, and the
//! unescaping of names and URIs (cgltf hands strings over raw, escapes included). A byte-for-byte port: its rules
//! (no trailing commas, 64 levels, duplicates resolved by the last) are not those of cgltf's tokenizer.

/// A parsed JSON value.
#[derive(Clone, Debug, Default, PartialEq)]
pub(crate) enum Value {
    #[default]
    Null,
    Bool(bool),
    /// a JSON integer token
    Int(IntValue),
    /// any other number
    Number(f64),
    String(Vec<u8>),
    Array(Vec<Value>),
    Object(Vec<(Vec<u8>, Value)>),
}

/// An integer token: its value as a double (like any number) and, when it fits in 0..2^63-1, exactly.
#[derive(Clone, Copy, Debug, PartialEq, Default)]
pub(crate) struct IntValue {
    pub number: f64,
    pub fits: Option<i64>,
}

impl Value {
    /// The value of a key: the last one wins, as in Python's json.
    pub(crate) fn get(&self, key: &str) -> Option<&Value> {
        match self {
            Value::Object(m) => m.iter().rev().find(|(k, _)| k.as_slice() == key.as_bytes()).map(|(_, v)| v),
            _ => None,
        }
    }

    pub(crate) fn is_number(&self) -> bool {
        matches!(self, Value::Int(_) | Value::Number(_))
    }

    /// The number as a double: the double parse of an integer token too (an out-of-range one is 0, from_chars' rule).
    pub(crate) fn number(&self) -> f64 {
        match self {
            Value::Int(i) => i.number,
            Value::Number(n) => *n,
            _ => 0.0,
        }
    }

    /// A JSON integer in 0..limit (not a bool, not a float), its value.
    pub(crate) fn index(&self, limit: u64) -> Option<u64> {
        match self {
            Value::Int(IntValue { fits: Some(v), .. }) if (*v as u64) < limit => Some(*v as u64),
            _ => None,
        }
    }
}

struct Parser<'a> {
    s: &'a [u8],
    p: usize,
}

impl Parser<'_> {
    // the C string's NUL: past the end reads as 0
    fn at(&self, p: usize) -> u8 {
        self.s.get(p).copied().unwrap_or(0)
    }

    fn cur(&self) -> u8 {
        self.at(self.p)
    }

    fn space(&mut self) {
        while matches!(self.cur(), b' ' | b'\t' | b'\n' | b'\r') {
            self.p += 1;
        }
    }

    fn literal(&mut self, word: &[u8]) -> bool {
        if self.s[self.p.min(self.s.len())..].starts_with(word) {
            self.p += word.len();
            true
        } else {
            false
        }
    }

    fn hex4(&mut self) -> Option<u32> {
        let mut code = 0u32;
        for _ in 0..4 {
            let c = self.cur();
            self.p += 1;
            let v = match c {
                b'0'..=b'9' => c - b'0',
                b'a'..=b'f' => c - b'a' + 10,
                b'A'..=b'F' => c - b'A' + 10,
                _ => return None,
            };
            code = (code << 4) | v as u32;
        }
        Some(code)
    }

    fn string(&mut self, out: &mut Vec<u8>) -> bool {
        let c0 = self.cur();
        self.p += 1;
        if c0 != b'"' {
            return false;
        }
        while self.cur() != b'"' {
            let c = self.cur();
            if c == 0 || c < 0x20 {
                return false;
            }
            if c != b'\\' {
                out.push(c);
                self.p += 1;
                continue;
            }
            self.p += 1;
            let e = self.cur();
            self.p += 1;
            match e {
                b'"' => out.push(b'"'),
                b'\\' => out.push(b'\\'),
                b'/' => out.push(b'/'),
                b'b' => out.push(8),
                b'f' => out.push(12),
                b'n' => out.push(b'\n'),
                b'r' => out.push(b'\r'),
                b't' => out.push(b'\t'),
                b'u' => {
                    let Some(mut code) = self.hex4() else { return false };
                    if (0xd800..0xdc00).contains(&code) && self.cur() == b'\\' && self.at(self.p + 1) == b'u' {
                        let save = self.p;
                        self.p += 2;
                        match self.hex4() {
                            Some(low) if (0xdc00..0xe000).contains(&low) => {
                                code = 0x10000 + ((code - 0xd800) << 10) + (low - 0xdc00);
                            }
                            _ => self.p = save,
                        }
                    }
                    utf8(out, code);
                }
                _ => return false,
            }
        }
        self.p += 1;
        true
    }

    fn number(&mut self) -> Option<Value> {
        let start = self.p;
        let mut is_int = true;
        if self.cur() == b'-' {
            self.p += 1;
        }
        if !self.cur().is_ascii_digit() {
            return None;
        }
        while self.cur().is_ascii_digit() {
            self.p += 1;
        }
        if self.cur() == b'.' {
            is_int = false;
            self.p += 1;
            if !self.cur().is_ascii_digit() {
                return None;
            }
            while self.cur().is_ascii_digit() {
                self.p += 1;
            }
        }
        if matches!(self.cur(), b'e' | b'E') {
            is_int = false;
            self.p += 1;
            if matches!(self.cur(), b'+' | b'-') {
                self.p += 1;
            }
            if !self.cur().is_ascii_digit() {
                return None;
            }
            while self.cur().is_ascii_digit() {
                self.p += 1;
            }
        }
        // the token is pure ASCII digits and punctuation
        let text = std::str::from_utf8(&self.s[start..self.p]).ok()?;
        let parsed = text.parse::<f64>().ok()?;
        // from_chars reports overflow and underflow to zero as out of range and leaves the value 0 (subnormals pass)
        let has_digit = text.split(['e', 'E']).next().unwrap().bytes().any(|b| (b'1'..=b'9').contains(&b));
        let number = if parsed.is_infinite() || (parsed == 0.0 && has_digit) { 0.0 } else { parsed };
        Some(if is_int {
            Value::Int(IntValue { number, fits: text.parse::<i64>().ok().filter(|v| *v >= 0) })
        } else {
            Value::Number(number)
        })
    }

    fn value(&mut self, depth: i32) -> Option<Value> {
        if depth > 64 {
            return None;
        }
        self.space();
        match self.cur() {
            b'{' => {
                self.p += 1;
                let mut members = Vec::new();
                self.space();
                if self.cur() == b'}' {
                    self.p += 1;
                    return Some(Value::Object(members));
                }
                loop {
                    self.space();
                    let mut key = Vec::new();
                    if !self.string(&mut key) {
                        return None;
                    }
                    self.space();
                    let c = self.cur();
                    self.p += 1;
                    if c != b':' {
                        return None;
                    }
                    let v = self.value(depth + 1)?;
                    members.push((key, v));
                    self.space();
                    if self.cur() == b',' {
                        self.p += 1;
                        continue;
                    }
                    let c = self.cur();
                    self.p += 1;
                    return if c == b'}' { Some(Value::Object(members)) } else { None };
                }
            }
            b'[' => {
                self.p += 1;
                let mut items = Vec::new();
                self.space();
                if self.cur() == b']' {
                    self.p += 1;
                    return Some(Value::Array(items));
                }
                loop {
                    items.push(self.value(depth + 1)?);
                    self.space();
                    if self.cur() == b',' {
                        self.p += 1;
                        continue;
                    }
                    let c = self.cur();
                    self.p += 1;
                    return if c == b']' { Some(Value::Array(items)) } else { None };
                }
            }
            b'"' => {
                let mut s = Vec::new();
                if self.string(&mut s) {
                    Some(Value::String(s))
                } else {
                    None
                }
            }
            _ => {
                if self.literal(b"true") {
                    return Some(Value::Bool(true));
                }
                if self.literal(b"false") {
                    return Some(Value::Bool(false));
                }
                if self.literal(b"null") {
                    return Some(Value::Null);
                }
                self.number()
            }
        }
    }
}

fn utf8(out: &mut Vec<u8>, code: u32) {
    if code < 0x80 {
        out.push(code as u8);
    } else if code < 0x800 {
        out.push(0xc0 | (code >> 6) as u8);
        out.push(0x80 | (code & 0x3f) as u8);
    } else if code < 0x10000 {
        out.push(0xe0 | (code >> 12) as u8);
        out.push(0x80 | ((code >> 6) & 0x3f) as u8);
        out.push(0x80 | (code & 0x3f) as u8);
    } else {
        out.push(0xf0 | (code >> 18) as u8);
        out.push(0x80 | ((code >> 12) & 0x3f) as u8);
        out.push(0x80 | ((code >> 6) & 0x3f) as u8);
        out.push(0x80 | (code & 0x3f) as u8);
    }
}

/// Parses a whole text as one JSON value (`JParser::Parse`: trailing spaces only after it).
pub(crate) fn parse(text: &[u8]) -> Option<Value> {
    let mut p = Parser { s: text, p: 0 };
    let v = p.value(0)?;
    p.space();
    if p.cur() == 0 {
        Some(v)
    } else {
        None
    }
}

/// `JParser::Unescape`: a raw JSON string's escapes resolved; `None` for a bad escape.
pub(crate) fn unescape(raw: &[u8]) -> Option<Vec<u8>> {
    let mut quoted = Vec::with_capacity(raw.len() + 2);
    quoted.push(b'"');
    quoted.extend_from_slice(raw);
    quoted.push(b'"');
    let mut p = Parser { s: &quoted, p: 0 };
    let mut out = Vec::new();
    if p.string(&mut out) {
        Some(out)
    } else {
        None
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn values() {
        let v = parse(br#" {"a":[1,2.5,"x\n",true,false,null],"a":3} "#).unwrap();
        assert_eq!(v.get("a").unwrap().index(10), Some(3)); // the last one wins
    }

    #[test]
    fn integers_are_indexed_only_if_in_range_and_whole() {
        assert_eq!(parse(b"5").unwrap().index(6), Some(5));
        assert_eq!(parse(b"5").unwrap().index(5), None);
        assert_eq!(parse(b"5.0").unwrap().index(6), None);
        assert_eq!(parse(b"-1").unwrap().index(6), None);
        assert_eq!(parse(b"-0").unwrap().index(6), Some(0));
        assert_eq!(parse(b"true").unwrap().index(6), None);
        assert_eq!(parse(b"9223372036854775808").unwrap().index(u64::MAX), None);
    }

    #[test]
    fn refusals_are_strict() {
        for bad in [
            &b"[1,]"[..],
            b"{\"a\":1,}",
            b"01x",
            b"1.",
            b".5",
            b"-",
            b"tru",
            b"\"a",
            b"\"\t\"",
            b"\"\\q\"",
            b"[1 2]",
            b"{\"a\" 1}",
            b"1 2",
        ] {
            assert_eq!(parse(bad), None, "{bad:?}");
        }
    }

    #[test]
    fn nesting_limit_is_64() {
        let mut deep = vec![b'['; 65];
        deep.extend(std::iter::repeat_n(b']', 65));
        assert!(parse(&deep).is_some());
        let mut too_deep = vec![b'['; 66];
        too_deep.extend(std::iter::repeat_n(b']', 66));
        assert_eq!(parse(&too_deep), None);
    }

    #[test]
    fn numbers_like_from_chars() {
        assert_eq!(parse(b"1e400").unwrap().number(), 0.0); // out of range: unmodified 0
        assert_eq!(parse(b"1e-400").unwrap().number(), 0.0);
        assert_eq!(parse(b"4.684328e-39").unwrap().number(), 4.684328e-39); // subnormal in f32, fine in f64
        assert_eq!(parse(b"0.1").unwrap().number(), 0.1);
    }

    #[test]
    fn unicode_escapes() {
        assert_eq!(unescape(br"\u00e9"), Some("\u{e9}".as_bytes().to_vec()));
        assert_eq!(unescape(br"\ud83d\ude00"), Some("\u{1F600}".as_bytes().to_vec()));
        // a lone high surrogate is kept as its 3-byte form, the C++ rule
        assert_eq!(unescape(br"\ud83d"), Some(vec![0xed, 0xa0, 0xbd]));
        assert_eq!(unescape(br"a\qb"), None);
    }
}
