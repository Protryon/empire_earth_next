//! The AI manifest's small JSON reader (RFC 8259: objects, arrays, strings with escapes, numbers, true, false, null),
//! as `EECPEngine.cpp`'s `JReader` reads: the same documents accepted, the same first error and its line. Objects keep
//! their fields in file order, a repeated key too. Numbers are checked as C `strtod` reads a decimal one and kept as
//! their text (the manifest has none it reads).

/// A JSON value.
#[derive(Clone, Debug, PartialEq)]
pub enum Value {
    Null,
    Bool(bool),
    Number(Vec<u8>),
    String(Vec<u8>),
    Array(Vec<Value>),
    Object(Vec<(Vec<u8>, Value)>),
}

impl Value {
    /// The first field of that name of an object (None: not an object, or no such field).
    pub fn field(&self, name: &[u8]) -> Option<&Value> {
        match self {
            Value::Object(fields) => fields.iter().find(|f| f.0 == name).map(|f| &f.1),
            _ => None,
        }
    }
}

const MAX_DEPTH: i32 = 32;

struct Reader<'a> {
    t: &'a [u8],
    at: usize,
    error: Option<String>,
    depth: i32,
}

/// The C `strtod`'s decimal form, whole: [-+]? (digits [. digits*] | . digits) ([eE] [-+]? digits)?
fn strtod_whole(s: &[u8]) -> bool {
    let mut i = 0;
    if i < s.len() && (s[i] == b'-' || s[i] == b'+') {
        i += 1;
    }
    let int_start = i;
    while i < s.len() && s[i].is_ascii_digit() {
        i += 1;
    }
    let mut digits = i - int_start;
    if i < s.len() && s[i] == b'.' {
        i += 1;
        let frac = i;
        while i < s.len() && s[i].is_ascii_digit() {
            i += 1;
        }
        digits += i - frac;
    }
    if digits == 0 {
        return false;
    }
    if i < s.len() && (s[i] == b'e' || s[i] == b'E') {
        let mut j = i + 1;
        if j < s.len() && (s[j] == b'-' || s[j] == b'+') {
            j += 1;
        }
        let exp = j;
        while j < s.len() && s[j].is_ascii_digit() {
            j += 1;
        }
        if j > exp {
            i = j;
        }
    }
    i == s.len()
}

impl Reader<'_> {
    fn fail(&mut self, what: &str) -> bool {
        if self.error.is_none() {
            let end = self.at.min(self.t.len());
            let line = 1 + self.t[..end].iter().filter(|&&c| c == b'\n').count();
            self.error = Some(format!("line {line}: {what}"));
        }
        false
    }
    fn space(&mut self) {
        while self.at < self.t.len() && matches!(self.t[self.at], b' ' | b'\t' | b'\r' | b'\n') {
            self.at += 1;
        }
    }
    fn word(&mut self, w: &[u8]) -> bool {
        if !self.t[self.at..].starts_with(w) {
            return self.fail("unexpected text");
        }
        self.at += w.len();
        true
    }
    fn put_utf8(out: &mut Vec<u8>, c: u32) {
        if c < 0x80 {
            out.push(c as u8);
        } else if c < 0x800 {
            out.push((0xc0 | (c >> 6)) as u8);
            out.push((0x80 | (c & 0x3f)) as u8);
        } else if c < 0x10000 {
            out.push((0xe0 | (c >> 12)) as u8);
            out.push((0x80 | ((c >> 6) & 0x3f)) as u8);
            out.push((0x80 | (c & 0x3f)) as u8);
        } else {
            out.push((0xf0 | (c >> 18)) as u8);
            out.push((0x80 | ((c >> 12) & 0x3f)) as u8);
            out.push((0x80 | ((c >> 6) & 0x3f)) as u8);
            out.push((0x80 | (c & 0x3f)) as u8);
        }
    }
    fn hex4(&mut self, out: &mut u32) -> bool {
        *out = 0;
        for _ in 0..4 {
            if self.at >= self.t.len() {
                return self.fail("unterminated \\u escape");
            }
            let c = self.t[self.at];
            *out <<= 4;
            match c {
                b'0'..=b'9' => *out |= (c - b'0') as u32,
                b'a'..=b'f' => *out |= (c - b'a' + 10) as u32,
                b'A'..=b'F' => *out |= (c - b'A' + 10) as u32,
                _ => return self.fail("bad \\u escape"),
            }
            self.at += 1;
        }
        true
    }
    fn string(&mut self, out: &mut Vec<u8>) -> bool {
        self.at += 1; // "
        out.clear();
        loop {
            if self.at >= self.t.len() {
                return self.fail("unterminated string");
            }
            let c = self.t[self.at];
            self.at += 1;
            if c == b'"' {
                return true;
            }
            if c < 0x20 {
                return self.fail("a control character in a string");
            }
            if c != b'\\' {
                out.push(c);
                continue;
            }
            if self.at >= self.t.len() {
                return self.fail("unterminated string");
            }
            let e = self.t[self.at];
            self.at += 1;
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
                    let mut u = 0;
                    if !self.hex4(&mut u) {
                        return false;
                    }
                    if (0xd800..0xdc00).contains(&u) && self.t[self.at..].starts_with(b"\\u") {
                        self.at += 2;
                        let mut v = 0;
                        if !self.hex4(&mut v) {
                            return false;
                        }
                        if (0xdc00..0xe000).contains(&v) {
                            u = 0x10000 + ((u - 0xd800) << 10) + (v - 0xdc00);
                        }
                    }
                    Self::put_utf8(out, u);
                }
                _ => return self.fail("bad escape in a string"),
            }
        }
    }
    fn number(&mut self) -> Option<Value> {
        let start = self.at;
        if self.t[self.at] == b'-' {
            self.at += 1;
        }
        while self.at < self.t.len() && b"0123456789.eE+-".contains(&self.t[self.at]) {
            self.at += 1;
        }
        let text = &self.t[start..self.at];
        if text.is_empty() || !strtod_whole(text) {
            self.fail("bad number");
            return None;
        }
        Some(Value::Number(text.to_vec()))
    }
    fn value(&mut self) -> Option<Value> {
        self.space();
        if self.at >= self.t.len() {
            self.fail("unexpected end");
            return None;
        }
        self.depth += 1;
        if self.depth > MAX_DEPTH {
            self.fail("nested too deep");
            return None;
        }
        let c = self.t[self.at];
        let out = match c {
            b'{' => self.object(),
            b'[' => self.array(),
            b'"' => {
                let mut s = Vec::new();
                self.string(&mut s).then_some(Value::String(s))
            }
            b't' => self.word(b"true").then_some(Value::Bool(true)),
            b'f' => self.word(b"false").then_some(Value::Bool(false)),
            b'n' => self.word(b"null").then_some(Value::Null),
            b'-' | b'0'..=b'9' => self.number(),
            _ => {
                self.fail("unexpected character");
                None
            }
        };
        self.depth -= 1;
        out
    }
    fn object(&mut self) -> Option<Value> {
        let mut fields = Vec::new();
        self.at += 1;
        self.space();
        if self.at < self.t.len() && self.t[self.at] == b'}' {
            self.at += 1;
            return Some(Value::Object(fields));
        }
        loop {
            self.space();
            if self.at >= self.t.len() || self.t[self.at] != b'"' {
                self.fail("expected a key");
                return None;
            }
            let mut key = Vec::new();
            if !self.string(&mut key) {
                return None;
            }
            self.space();
            if self.at >= self.t.len() || self.t[self.at] != b':' {
                self.fail("expected :");
                return None;
            }
            self.at += 1;
            let v = self.value()?;
            fields.push((key, v));
            self.space();
            if self.at < self.t.len() && self.t[self.at] == b',' {
                self.at += 1;
                continue;
            }
            if self.at < self.t.len() && self.t[self.at] == b'}' {
                self.at += 1;
                return Some(Value::Object(fields));
            }
            self.fail("expected , or }");
            return None;
        }
    }
    fn array(&mut self) -> Option<Value> {
        let mut items = Vec::new();
        self.at += 1;
        self.space();
        if self.at < self.t.len() && self.t[self.at] == b']' {
            self.at += 1;
            return Some(Value::Array(items));
        }
        loop {
            let v = self.value()?;
            items.push(v);
            self.space();
            if self.at < self.t.len() && self.t[self.at] == b',' {
                self.at += 1;
                continue;
            }
            if self.at < self.t.len() && self.t[self.at] == b']' {
                self.at += 1;
                return Some(Value::Array(items));
            }
            self.fail("expected , or ]");
            return None;
        }
    }
}

/// A whole document (a UTF-8 byte order mark first is skipped) -> its value, or the first error ("line N: what").
pub fn parse(text: &[u8]) -> Result<Value, String> {
    let mut r = Reader { t: text, at: 0, error: None, depth: 0 };
    if text.starts_with(b"\xef\xbb\xbf") {
        r.at = 3;
    }
    let v = r.value();
    if let Some(v) = v {
        r.space();
        if r.at == text.len() {
            return Ok(v);
        }
        r.fail("text after the document");
    }
    Err(r.error.unwrap_or_default())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn values() {
        let v = parse(br#" {"a": [1, -2.5e3, true, false, null, "x\u00e9\ud83d\ude00\n"], "a": {}} "#).unwrap();
        let a = v.field(b"a").unwrap();
        match a {
            Value::Array(items) => {
                assert_eq!(items[1], Value::Number(b"-2.5e3".to_vec()));
                assert_eq!(items[5], Value::String("xé😀\n".as_bytes().to_vec()));
            }
            _ => panic!(),
        }
        assert!(parse(b"\xef\xbb\xbf[]").is_ok());
    }

    #[test]
    fn errors_name_the_line() {
        assert_eq!(parse(b"{\n\"a\": tru}").unwrap_err(), "line 2: unexpected text");
        assert_eq!(parse(b"[1,\n\n2 3]").unwrap_err(), "line 3: expected , or ]");
        assert_eq!(parse(b"[1-2]").unwrap_err(), "line 1: bad number");
        assert_eq!(parse(b"[-]").unwrap_err(), "line 1: bad number");
        assert_eq!(parse(b"[1e]").unwrap_err(), "line 1: bad number");
        assert_eq!(parse(b"[5.]").unwrap(), Value::Array(vec![Value::Number(b"5.".to_vec())]));
        assert_eq!(parse(b"\"\\u12\"").unwrap_err(), "line 1: bad \\u escape");
        assert_eq!(parse(b"\"\\u12").unwrap_err(), "line 1: unterminated \\u escape");
        assert_eq!(parse(b"\"a\tb\"").unwrap_err(), "line 1: a control character in a string");
        assert_eq!(parse(b"[] x").unwrap_err(), "line 1: text after the document");
        assert_eq!(parse(b"").unwrap_err(), "line 1: unexpected end");
        let deep = [vec![b'['; 32], vec![b']'; 32]].concat();
        assert!(parse(&deep).is_ok());
        let deeper = [vec![b'['; 32], b"1".to_vec(), vec![b']'; 32]].concat();
        assert_eq!(parse(&deeper).unwrap_err(), "line 1: nested too deep");
    }
}
