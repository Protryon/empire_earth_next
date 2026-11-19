//! The fixture's texts (TEST ONLY): the C library's scanning of a command line (`sscanf`'s `%lu`, `%ld`, `%31s`),
//! the numbers of the test switches (`strtoull`, `strtol`), the ui-state file's escaped texts and id lists, the
//! frame's BMP header and the files written in the C library's text mode.

use std::os::raw::{c_long, c_ulong};

fn is_space(c: u8) -> bool {
    c == b' ' || (b'\t'..=b'\r').contains(&c)
}

/// A `sscanf` over a line: each conversion skips blanks first; the first one that fails ends the scan.
pub struct Scan<'a> {
    b: &'a [u8],
    at: usize,
    /// the conversions assigned so far (`sscanf`'s result, when it is not EOF)
    pub fields: i32,
    failed: bool,
}

impl<'a> Scan<'a> {
    pub fn new(text: &'a [u8]) -> Scan<'a> {
        let end = text.iter().position(|&c| c == 0).unwrap_or(text.len());
        Scan { b: &text[..end], at: 0, fields: 0, failed: false }
    }
    fn skip(&mut self) {
        while self.at < self.b.len() && is_space(self.b[self.at]) {
            self.at += 1;
        }
    }
    /// The digits of a decimal number with its sign -> (negative, the magnitude saturated at u128's range).
    fn number(&mut self) -> Option<(bool, u128)> {
        if self.failed {
            return None;
        }
        self.skip();
        let start = self.at;
        let mut i = self.at;
        let negative = i < self.b.len() && self.b[i] == b'-';
        if i < self.b.len() && (self.b[i] == b'-' || self.b[i] == b'+') {
            i += 1;
        }
        let digits = i;
        let mut v: u128 = 0;
        while i < self.b.len() && self.b[i].is_ascii_digit() {
            v = v.saturating_mul(10).saturating_add(u128::from(self.b[i] - b'0'));
            i += 1;
        }
        if i == digits {
            self.at = start;
            self.failed = true;
            return None;
        }
        self.at = i;
        self.fields += 1;
        Some((negative, v))
    }
    /// `%lu`: a minus negates (wrapping); saturated at `ULONG_MAX`.
    pub fn ulong(&mut self) -> Option<c_ulong> {
        let (negative, v) = self.number()?;
        let v = if v > u128::from(c_ulong::MAX) { c_ulong::MAX } else { v as c_ulong };
        Some(if negative { v.wrapping_neg() } else { v })
    }
    /// `%ld`: saturated at the type's range.
    pub fn long(&mut self) -> Option<c_long> {
        let (negative, v) = self.number()?;
        let v = v.min(u128::from(c_long::MAX as u64) + 1) as i128;
        let v = if negative { -v } else { v };
        Some(v.clamp(i128::from(c_long::MIN), i128::from(c_long::MAX)) as c_long)
    }
    /// `%Ns`: up to `max` characters that are not blanks.
    pub fn word(&mut self, max: usize) -> Option<String> {
        if self.failed {
            return None;
        }
        self.skip();
        let start = self.at;
        while self.at < self.b.len() && !is_space(self.b[self.at]) && self.at - start < max {
            self.at += 1;
        }
        if self.at == start {
            self.failed = true;
            return None;
        }
        self.fields += 1;
        Some(String::from_utf8_lossy(&self.b[start..self.at]).into_owned())
    }
}

/// A command line: "<seq> <op> <a> <b> <c> <d>" (`"%lu %31s %lu %lu %lu %lu"`) -> the fields read and the values
/// (0 for those not read).
#[derive(Clone, Debug, Default, PartialEq)]
pub struct Command {
    pub fields: i32,
    pub seq: u64,
    pub op: String,
    pub a: u64,
    pub b: u64,
    pub c: u64,
    pub d: u64,
}

pub fn command(line: &[u8]) -> Command {
    let mut s = Scan::new(line);
    let seq = u64::from(s.ulong().unwrap_or(0));
    let op = s.word(31).unwrap_or_default();
    let mut next = || u64::from(s.ulong().unwrap_or(0));
    let (a, b, c, d) = (next(), next(), next(), next());
    Command { fields: s.fields, seq, op, a, b, c, d }
}

/// The first line of a file as `fgets(.., 200, ..)` reads it (at most 199 bytes, through its new line).
pub fn first_line(bytes: &[u8]) -> &[u8] {
    let cap = bytes.len().min(199);
    match bytes[..cap].iter().position(|&c| c == b'\n') {
        Some(k) => &bytes[..=k],
        None => &bytes[..cap],
    }
}

/// `strtoull(text, &end, 0)` -> the value (saturated) and whether it read the whole text.
pub fn strtoull0(text: &str) -> (u64, bool) {
    let b = text.as_bytes();
    let mut i = 0;
    while i < b.len() && is_space(b[i]) {
        i += 1;
    }
    let negative = i < b.len() && b[i] == b'-';
    if i < b.len() && (b[i] == b'-' || b[i] == b'+') {
        i += 1;
    }
    let mut base = 10u32;
    if i + 1 < b.len()
        && b[i] == b'0'
        && (b[i + 1] | 0x20) == b'x'
        && b.get(i + 2).is_some_and(|c| c.is_ascii_hexdigit())
    {
        base = 16;
        i += 2;
    } else if i < b.len() && b[i] == b'0' {
        base = 8;
    }
    let digits = i;
    let mut v: u64 = 0;
    let mut overflow = false;
    while i < b.len() {
        let Some(d) = (b[i] as char).to_digit(base) else { break };
        match v.checked_mul(u64::from(base)).and_then(|x| x.checked_add(u64::from(d))) {
            Some(x) => v = x,
            None => overflow = true,
        }
        i += 1;
    }
    if i == digits {
        return (0, b.is_empty());
    }
    let v = if overflow {
        u64::MAX
    } else if negative {
        v.wrapping_neg()
    } else {
        v
    };
    (v, i == b.len())
}

/// `strtol(text, &end, 10)` -> the value (saturated) and whether it read the whole text.
pub fn strtol10(text: &str) -> (i64, bool) {
    let mut s = Scan::new(text.as_bytes());
    match s.number() {
        Some((negative, v)) => {
            let v = v.min(u128::from(u64::MAX)) as i128;
            let v = if negative { -v } else { v };
            (v.clamp(i128::from(c_long::MIN), i128::from(c_long::MAX)) as i64, s.at == text.len())
        }
        None => (0, text.is_empty()),
    }
}

/// EE_TEST_TIME_FACTOR's text -> (numerator, denominator); None when it is not "N", "N/D" or a decimal with up to
/// three places counted (`"%lu/%lu"`, then `"%lu.%7[0-9]"`).
pub fn time_factor(text: &str) -> Option<(c_ulong, c_ulong)> {
    let mut s = Scan::new(text.as_bytes());
    let num = s.ulong();
    let mut den: c_ulong = 1;
    if num.is_some() && s.at < s.b.len() && s.b[s.at] == b'/' {
        s.at += 1;
        if let Some(d) = s.ulong() {
            den = d;
        }
    }
    let fields = s.fields;
    let mut num = num.unwrap_or(0);
    if fields == 1 && text.contains('.') {
        den = 1;
        num = 0;
        let mut t = Scan::new(text.as_bytes());
        if let Some(whole) = t.ulong() {
            num = whole;
            let mut places = Vec::new();
            if t.at < t.b.len() && t.b[t.at] == b'.' {
                let mut k = t.at + 1;
                while k < t.b.len() && t.b[k].is_ascii_digit() && places.len() < 7 {
                    places.push(t.b[k]);
                    k += 1;
                }
            }
            for &d in places.iter().take(3) {
                num = num.wrapping_mul(10).wrapping_add(c_ulong::from(d - b'0'));
                den = den.wrapping_mul(10);
            }
        }
    }
    (fields >= 1).then_some((num, den))
}

/// A UI text as the ui-state file writes it: its first 80 bytes, the control characters, those past 0x7e, '|' and
/// '\' as "\xNN".
pub fn escaped(text: &[u8]) -> String {
    let mut out = String::new();
    for &c in text.iter().take_while(|&&c| c != 0).take(80) {
        if !(0x20..0x7f).contains(&c) || c == b'|' || c == b'\\' {
            out.push_str(&format!("\\x{c:02x}"));
        } else {
            out.push(c as char);
        }
    }
    out
}

/// A list's lines as the ui-state file writes them: its first 64, each after ' ' (the first) or '|', then a new line.
pub fn lines(texts: &[Vec<u8>]) -> String {
    let mut out = String::new();
    for (k, t) in texts.iter().take(64).enumerate() {
        out.push(if k == 0 { ' ' } else { '|' });
        out.push_str(&escaped(t));
    }
    out.push('\n');
    out
}

/// Ids as "<n> <a>,<b>,.." sorted ("<n> -" for none).
pub fn ids(mut ids: Vec<i64>) -> String {
    ids.sort_unstable();
    let mut out = format!("{} ", ids.len());
    if ids.is_empty() {
        out.push('-');
    }
    for (k, id) in ids.iter().enumerate() {
        if k > 0 {
            out.push(',');
        }
        out.push_str(&id.to_string());
    }
    out
}

/// A top-down 32-bit BMP's header for a frame of `width` x `height`.
pub fn bmp_header(width: u64, height: u64) -> [u8; 54] {
    let (width, height) = (width as u32, height as u32);
    let mut h = [0u8; 54];
    let size = 54u32.wrapping_add(width.wrapping_mul(height).wrapping_mul(4));
    let top_down = (height as i32).wrapping_neg();
    h[0] = b'B';
    h[1] = b'M';
    h[2..6].copy_from_slice(&size.to_le_bytes());
    h[10] = 54;
    h[14] = 40;
    h[18..22].copy_from_slice(&width.to_le_bytes());
    h[22..26].copy_from_slice(&top_down.to_le_bytes());
    h[26] = 1;
    h[28] = 32;
    h
}

/// Text as the C library's text mode writes it: each new line as CR LF on Windows.
pub fn text_mode(text: &str) -> Vec<u8> {
    if cfg!(windows) {
        text.replace('\n', "\r\n").into_bytes()
    } else {
        text.as_bytes().to_vec()
    }
}

/// Replaces `to` with `from` (Windows' rename does not replace an existing file: `MoveFileEx`'s replace) -> true on
/// success.
pub fn replace_with(from: &str, to: &str) -> bool {
    std::fs::rename(from, to).is_ok()
}
