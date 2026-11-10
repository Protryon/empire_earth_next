//! WON's `StringParser`: a cursor over a NUL-terminated narrow or wide string, the scanner of the lobby's resource
//! files, the config objects and the chat commands. The reads (`ReadString` in both output widths, `ReadValue` for
//! integers and booleans), the whitespace and prefix scans, and the decimal formats.
//!
//! The text is read by index as the C++ did (`Text`): a cursor may step one past the terminator and back (an escape
//! at the end of a quoted string), so the length is never needed. Character classes go through `Ctype`, because the
//! original called the C runtime's `isspace` / `isdigit` / `isxdigit` / `isalnum` / `toupper` / `tolower` and the
//! game's UI thread sets a locale (`setlocale(LC_ALL, "English")`), in which 0xA0 is a space; the FFI passes the C
//! runtime's functions with the original's exact arguments (`isalnum` of a `toupper` result cast to `char`, so a
//! byte above 0x7f arrives negative). The tests use the C locale (`CLocale`).
//!
//! Integer reads accumulate in a wrapping 32-bit `int` (the 64-bit read too), as the original.

/// The string under the cursor, read at the cursor's offsets.
pub trait Text {
    /// `GetChar`: the unit as a byte; a wide unit above 255 reads as 0xff.
    fn char_at(&self, index: i32) -> u8;
    /// `GetWChar`: the unit (a narrow byte zero-extended).
    fn wchar_at(&self, index: i32) -> u32;
}

/// The C runtime's character classes and case maps, called with the original's arguments.
pub trait Ctype {
    fn is_space(&self, c: i32) -> bool;
    fn is_digit(&self, c: i32) -> bool;
    fn is_xdigit(&self, c: i32) -> bool;
    fn is_alnum(&self, c: i32) -> bool;
    fn to_upper(&self, c: i32) -> i32;
    fn to_lower(&self, c: i32) -> i32;
}

/// The "C" locale's classes (the tests' `Ctype`): ASCII only.
pub struct CLocale;

impl Ctype for CLocale {
    fn is_space(&self, c: i32) -> bool {
        matches!(c, 0x09..=0x0d | 0x20)
    }
    fn is_digit(&self, c: i32) -> bool {
        (0x30..=0x39).contains(&c)
    }
    fn is_xdigit(&self, c: i32) -> bool {
        (0x30..=0x39).contains(&c) || (0x41..=0x46).contains(&c) || (0x61..=0x66).contains(&c)
    }
    fn is_alnum(&self, c: i32) -> bool {
        (0x30..=0x39).contains(&c) || (0x41..=0x5a).contains(&c) || (0x61..=0x7a).contains(&c)
    }
    fn to_upper(&self, c: i32) -> i32 {
        if (0x61..=0x7a).contains(&c) {
            c - 0x20
        } else {
            c
        }
    }
    fn to_lower(&self, c: i32) -> i32 {
        if (0x41..=0x5a).contains(&c) {
            c + 0x20
        } else {
            c
        }
    }
}

/// A narrow string (a test's or a borrowed one), NUL past its end.
pub struct Narrow<'a>(pub &'a [u8]);

impl Text for Narrow<'_> {
    fn char_at(&self, index: i32) -> u8 {
        usize::try_from(index).ok().and_then(|i| self.0.get(i).copied()).unwrap_or(0)
    }
    fn wchar_at(&self, index: i32) -> u32 {
        u32::from(self.char_at(index))
    }
}

/// A wide string's units, NUL past its end.
pub struct Wide<'a>(pub &'a [u32]);

impl Text for Wide<'_> {
    fn char_at(&self, index: i32) -> u8 {
        let unit = self.wchar_at(index);
        if unit < 256 {
            unit as u8
        } else {
            0xff
        }
    }
    fn wchar_at(&self, index: i32) -> u32 {
        usize::try_from(index).ok().and_then(|i| self.0.get(i).copied()).unwrap_or(0)
    }
}

/// The cursor: a text, a position and the character classes.
pub struct Parser<'a, T: Text, C: Ctype> {
    pub text: &'a T,
    pub ctype: &'a C,
    pub pos: i32,
}

fn hex_value<C: Ctype>(ctype: &C, ch: u8) -> i32 {
    // ScannerHexValue(char): the byte as a signed char
    let c = i32::from(ch as i8);
    if ctype.is_digit(c) {
        c - i32::from(b'0')
    } else {
        ctype.to_upper(c) - i32::from(b'A') + 10
    }
}

impl<'a, T: Text, C: Ctype> Parser<'a, T, C> {
    pub fn new(text: &'a T, ctype: &'a C, pos: i32) -> Self {
        Parser { text, ctype, pos }
    }

    pub fn get_char(&self, offset: i32) -> u8 {
        self.text.char_at(self.pos.wrapping_add(offset))
    }

    pub fn get_wchar(&self, offset: i32) -> u32 {
        self.text.wchar_at(self.pos.wrapping_add(offset))
    }

    fn step(&mut self, amount: i32) {
        self.pos = self.pos.wrapping_add(amount);
    }

    pub fn end_of_string(&self) -> bool {
        self.get_wchar(0) == 0
    }

    fn space_here(&self) -> bool {
        self.ctype.is_space(i32::from(self.get_char(0)))
    }

    pub fn skip_whitespace(&mut self) {
        while !self.end_of_string() && self.space_here() {
            self.step(1);
        }
    }

    pub fn skip_non_whitespace(&mut self) {
        while !self.end_of_string() && !self.space_here() {
            self.step(1);
        }
    }

    /// `CheckNextChar`: skips whitespace (kept on a mismatch) and consumes `ch` if it is next.
    pub fn check_next_char(&mut self, ch: i32) -> bool {
        self.skip_whitespace();
        if ch == i32::from(self.get_char(0)) {
            self.step(1);
            true
        } else {
            false
        }
    }

    /// `StartsWith`: consumes `prefix` if the text continues with it, else leaves the cursor.
    pub fn starts_with(&mut self, prefix: &[u8]) -> bool {
        let start = self.pos;
        let mut i = 0;
        while i < prefix.len() && prefix[i] != 0 && !self.end_of_string() {
            if prefix[i] != self.get_char(0) {
                self.pos = start;
                return false;
            }
            i += 1;
            self.step(1);
        }
        if self.end_of_string() && i < prefix.len() && prefix[i] != 0 {
            self.pos = start;
            return false;
        }
        true
    }

    /// `Is`: `starts_with` and then the end (a matched prefix stays consumed when more follows).
    pub fn is(&mut self, word: &[u8]) -> bool {
        self.starts_with(word) && self.end_of_string()
    }

    /// The escape after a backslash in a quoted string; `max_digits` for `\x`. The cursor is past the backslash.
    fn escape(&mut self, max_digits: u32) -> u32 {
        let escaped = self.get_char(0);
        self.step(1);
        match escaped {
            b't' => 0x09,
            b'n' => 0x0a,
            b'r' => 0x0d,
            b'"' => 0x22,
            b'$' => 0x24,
            b'\\' => 0x5c,
            b'x' => {
                let mut value: u32 = 0;
                for _ in 0..max_digits {
                    if !self.ctype.is_xdigit(i32::from(self.get_char(0))) {
                        break;
                    }
                    let digit = hex_value(self.ctype, self.get_char(0));
                    value = (value << 4) | digit as u32;
                    self.step(1);
                }
                value
            }
            _ => {
                self.step(-1);
                0x5c
            }
        }
    }

    /// `ReadString(std::string&, stopAtWhitespace)`: a bare or quoted token (escapes, `L"..."`, `"a" + "b"`),
    /// trailing unquoted whitespace trimmed. In a wide literal every unit is appended as its two low bytes
    /// (little-endian, not UTF-8). Always true.
    pub fn read_string(&mut self, stop_at_whitespace: bool) -> Vec<u8> {
        let mut out = Vec::new();
        self.skip_whitespace();
        let mut quoted = false;
        let mut wide_literal = false;
        let mut keep = 0usize;
        fn push_wide(out: &mut Vec<u8>, unit: u32) {
            out.extend_from_slice(&(unit as u16).to_le_bytes());
        }
        while !self.end_of_string() {
            let ch = self.get_char(0);
            let wide_char = if wide_literal { self.get_wchar(0) } else { 0 };
            self.step(1);
            if stop_at_whitespace && self.ctype.is_space(i32::from(ch)) && !quoted {
                break;
            }
            if ch == b'\\' && quoted {
                let value = self.escape(if wide_literal { 4 } else { 2 });
                if wide_literal {
                    push_wide(&mut out, value);
                } else {
                    out.push(value as u8);
                }
                keep = out.len();
            } else if !quoted && ch == b'L' && self.get_char(0) == b'"' {
                self.step(1);
                wide_literal = true;
                quoted = true;
            } else if ch == b'"' {
                if quoted {
                    quoted = false;
                    if self.get_char(0) != b'+' {
                        break;
                    }
                    self.step(1);
                    self.skip_whitespace();
                } else {
                    wide_literal = false;
                    quoted = true;
                }
            } else if !quoted && (ch == b',' || ch == b')') {
                self.step(-1);
                break;
            } else {
                if wide_literal {
                    push_wide(&mut out, wide_char);
                } else {
                    out.push(ch);
                }
                if quoted || !self.ctype.is_space(i32::from(ch)) {
                    keep = out.len();
                }
            }
        }
        out.truncate(keep);
        out
    }

    /// `ReadString(std::wstring&, stopAtWhitespace)`: as `read_string`, every unit kept as it is; `\x` takes up
    /// to four digits. Always true.
    pub fn read_wstring(&mut self, stop_at_whitespace: bool) -> Vec<u32> {
        let mut out = Vec::new();
        self.skip_whitespace();
        let mut quoted = false;
        let mut keep = 0usize;
        while !self.end_of_string() {
            let ch = self.get_char(0);
            let wide_char = self.get_wchar(0);
            self.step(1);
            if stop_at_whitespace && self.ctype.is_space(i32::from(ch)) && !quoted {
                break;
            }
            if ch == b'\\' && quoted {
                out.push(self.escape(4));
                keep = out.len();
            } else if !quoted && ch == b'L' && self.get_char(0) == b'"' {
                self.step(1);
                quoted = true;
            } else if ch == b'"' {
                if quoted {
                    quoted = false;
                    if self.get_char(0) != b'+' {
                        break;
                    }
                    self.step(1);
                    self.skip_whitespace();
                } else {
                    quoted = true;
                }
            } else if !quoted && (ch == b',' || ch == b')') {
                self.step(-1);
                break;
            } else {
                out.push(wide_char);
                if quoted || !self.ctype.is_space(i32::from(ch)) {
                    keep = out.len();
                }
            }
        }
        out.truncate(keep);
        out
    }

    /// `ReadValue(int&)`: a decimal or `0x` hex integer with an optional '-', accumulated in a wrapping `int`.
    /// `Some` when the cursor moved past the whitespace (a lone '-' or `0x` counts, with the value 0).
    pub fn read_int(&mut self) -> Option<i32> {
        self.skip_whitespace();
        let start = self.pos;
        let mut value: i32 = 0;
        let mut negative = false;
        if self.get_char(0) == b'-' {
            negative = true;
            self.step(1);
        }
        if self.get_char(0) == b'0' && self.ctype.to_lower(i32::from(self.get_char(1))) == i32::from(b'x') {
            self.step(2);
            while self.ctype.is_xdigit(i32::from(self.get_char(0))) {
                value = value.wrapping_shl(4);
                // char ch = toupper(...): the result as a signed char
                let ch = i32::from(self.ctype.to_upper(i32::from(self.get_char(0))) as i8);
                if (0x30..=0x39).contains(&ch) {
                    value = value.wrapping_add(ch - 0x30);
                } else if (0x41..=0x46).contains(&ch) {
                    value = value.wrapping_add(ch - 0x41 + 10);
                } else {
                    break;
                }
                self.step(1);
            }
        } else {
            while self.ctype.is_digit(i32::from(self.get_char(0))) {
                value = value.wrapping_mul(10).wrapping_add(i32::from(self.get_char(0)) - 0x30);
                self.step(1);
            }
        }
        if negative {
            value = value.wrapping_neg();
        }
        if self.pos != start {
            Some(value)
        } else {
            None
        }
    }

    /// `ReadValue(bool&)`: the next alphanumeric word, upper-cased: TRUE YES ON Y T 1 or FALSE NO OFF N F 0.
    /// `None` for any other word (consumed).
    pub fn read_bool(&mut self) -> Option<bool> {
        self.skip_whitespace();
        let mut token = Vec::new();
        while !self.end_of_string() {
            // char ch = toupper(GetChar(0)); isalnum(ch): a byte above 0x7f arrives negative
            let ch = self.ctype.to_upper(i32::from(self.get_char(0))) as i8;
            if !self.ctype.is_alnum(i32::from(ch)) {
                break;
            }
            token.push(ch as u8);
            self.step(1);
        }
        match token.as_slice() {
            b"TRUE" | b"YES" | b"ON" | b"Y" | b"T" | b"1" => Some(true),
            b"FALSE" | b"NO" | b"OFF" | b"N" | b"F" | b"0" => Some(false),
            _ => None,
        }
    }
}

/// `ValueToString(const bool&)`.
pub fn format_bool(value: bool) -> &'static [u8] {
    if value {
        b"True"
    } else {
        b"False"
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn narrow(s: &str) -> Vec<u8> {
        s.bytes().collect()
    }

    #[test]
    fn whitespace_and_words() {
        let t = Narrow(b"  ab cd");
        let mut p = Parser::new(&t, &CLocale, 0);
        p.skip_whitespace();
        assert_eq!(p.pos, 2);
        p.skip_non_whitespace();
        assert_eq!(p.pos, 4);
        assert!(!p.check_next_char(b'x' as i32));
        assert_eq!(p.pos, 5);
        assert!(p.check_next_char(b'c' as i32));
        assert_eq!(p.pos, 6);
    }

    #[test]
    fn starts_with_rewinds_on_mismatch_only() {
        let t = Narrow(b"abc");
        let mut p = Parser::new(&t, &CLocale, 0);
        assert!(!p.starts_with(b"ax"));
        assert_eq!(p.pos, 0);
        assert!(!p.is(b"ab"));
        assert_eq!(p.pos, 2, "a matched prefix stays consumed");
        let t = Narrow(b"a");
        let mut p = Parser::new(&t, &CLocale, 0);
        assert!(!p.starts_with(b"ab"));
        assert_eq!(p.pos, 0);
    }

    #[test]
    fn strings() {
        let t = Narrow(br#""a\tb"+ "c\x41" "d" rest"#);
        let mut p = Parser::new(&t, &CLocale, 0);
        assert_eq!(p.read_string(false), narrow("a\tbcA"));
        let t = Narrow(b"abc  , def");
        let mut p = Parser::new(&t, &CLocale, 0);
        assert_eq!(p.read_string(false), narrow("abc"));
        assert_eq!(p.pos, 5, "stops on the comma");
        let t = Narrow(br#"L"w\x263a""#);
        let mut p = Parser::new(&t, &CLocale, 0);
        assert_eq!(p.read_string(false), vec![b'w', 0, 0x3a, 0x26]);
        let mut p = Parser::new(&t, &CLocale, 0);
        assert_eq!(p.read_wstring(false), vec![0x77, 0x263a]);
        let t = Narrow(br#""end\"#);
        let mut p = Parser::new(&t, &CLocale, 0);
        assert_eq!(p.read_string(false), narrow("end\\"));
    }

    #[test]
    fn wide_units_above_255_read_as_0xff() {
        let units = [0x263a, 0x20, 0x41];
        let t = Wide(&units);
        let p = Parser::new(&t, &CLocale, 0);
        assert_eq!(p.get_char(0), 0xff);
        assert_eq!(p.get_wchar(0), 0x263a);
    }

    #[test]
    fn integers_wrap_in_32_bits() {
        for (text, want, pos) in [
            ("  -123 ", Some(-123), 6),
            ("0x1F", Some(31), 4),
            ("0x", Some(0), 2),
            ("-", Some(0), 1),
            ("2147483648", Some(i32::MIN), 10),
            ("0xfffffffff", Some(-1), 11),
            ("x", None, 0),
        ] {
            let b = narrow(text);
            let t = Narrow(&b);
            let mut p = Parser::new(&t, &CLocale, 0);
            assert_eq!(p.read_int(), want, "{text}");
            assert_eq!(p.pos, pos, "{text}");
        }
    }

    #[test]
    fn booleans() {
        for (text, want) in [("yes", Some(true)), ("Off", Some(false)), ("maybe", None), ("1", Some(true))] {
            let b = narrow(text);
            let t = Narrow(&b);
            let mut p = Parser::new(&t, &CLocale, 0);
            assert_eq!(p.read_bool(), want, "{text}");
        }
    }
}
