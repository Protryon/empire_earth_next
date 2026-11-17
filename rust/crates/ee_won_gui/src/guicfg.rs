//! WONGUI's config readers' integer expressions (ComponentConfigRead.cpp: every number in the lobby's 63 .cfg
//! files that a component reads, `Width = 200`, `Pos = (Parent.Width - 20) / 2, 4` ...). The C++ keeps the
//! readers, their StringParser (already ee_won's `parser`) and the operands' lookups (numbers, `@ID_` control ids,
//! `@` int resources, named values and functions of this or a dotted config), which can throw; these answer:
//!
//! - `ParseExpression`'s recursive descent: parentheses, `+ -` then `* /` left to right, an operand from the C++
//!   (`GetIntValue`), the "Expecting ')'" and "Division by 0." errors;
//! - `GetIntValue`'s name: which characters it takes and what kind of operand the name is.
//!
//! The parser reads through a host of callbacks; an operand that threw stops it, and the C++ rethrows. Ints wrap
//! (the game builds with -fwrapv); `INT_MIN / -1`, a divide fault in the original, gives `INT_MIN`.

use std::ffi::c_void;

/// What the expression parser reads through (the C++ config's StringParser and `GetIntValue`).
pub trait ExprSource {
    fn skip_whitespace(&mut self);
    /// `GetChar(0)`: the next character as an unsigned char (0 at the end).
    fn peek(&mut self) -> u8;
    fn advance(&mut self, n: i32);
    /// `GetIntValue`; `None` when it threw.
    fn int_value(&mut self) -> Option<i32>;
}

/// Why an expression failed.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ExprError {
    /// "Expecting ')'"
    Paren = 1,
    /// "Division by 0."
    DivZero = 2,
    /// an operand threw (the C++ holds the exception)
    Operand = 3,
}

/// `ParseExpression(level)`: level 0 takes `+ -` and below, 1 `* /`, 2 an operand alone.
pub fn parse_expression(src: &mut impl ExprSource, level: i32) -> Result<i32, ExprError> {
    src.skip_whitespace();
    let mut value = if src.peek() == b'(' {
        src.advance(1);
        let v = parse_expression(src, 0)?;
        src.skip_whitespace();
        if src.peek() != b')' {
            return Err(ExprError::Paren);
        }
        src.advance(1);
        v
    } else {
        src.int_value().ok_or(ExprError::Operand)?
    };
    loop {
        src.skip_whitespace();
        let op = src.peek();
        src.advance(1);
        if op == b'+' && level < 1 {
            value = value.wrapping_add(parse_expression(src, 1)?);
        } else if op == b'-' && level < 1 {
            value = value.wrapping_sub(parse_expression(src, 1)?);
        } else if op == b'*' && level < 2 {
            value = value.wrapping_mul(parse_expression(src, 2)?);
        } else if op == b'/' && level < 2 {
            let d = parse_expression(src, 2)?;
            if d == 0 {
                return Err(ExprError::DivZero);
            }
            value = value.wrapping_div(d);
        } else {
            src.advance(-1);
            return Ok(value);
        }
    }
}

/// `GetIntValue`'s name scan: whether the (narrow, signed) character continues a name: an ASCII letter or digit,
/// '@', '_' or '.'; the original's ctype calls saw only 0..127 (a negative char became a huge unsigned long).
pub fn name_char(c: i8) -> bool {
    let c = c as u8;
    c < 128 && (c.is_ascii_alphanumeric() || c == b'@' || c == b'_' || c == b'.')
}

/// What a name is (`GetIntValue`): 0 a control id (`@ID_`, upper case: the original's `toupper` took the
/// comparison's result), its name from `at`; 1 an int resource (`@name`), from `at`; 2 a dotted name, the config
/// before the last '.' at `at`, the key after it; 3 a plain key.
pub fn name_kind(name: &[u8]) -> (i32, usize) {
    // the original read name[0] of an empty name: the terminator
    if name.first() == Some(&b'@') {
        if name.len() > 3 && name[1] == b'I' && name[2] == b'D' && name[3] == b'_' {
            return (0, 4);
        }
        return (1, 1);
    }
    match name.iter().rposition(|&c| c == b'.') {
        Some(d) => (2, d),
        None => (3, 0),
    }
}

// ---- the C ABI

/// The C++ side of an expression: its StringParser and `GetIntValue` (which returns false after catching what
/// the operand threw).
#[repr(C)]
pub struct EeWonExprHost {
    pub ctx: *mut c_void,
    pub skip_whitespace: extern "C" fn(ctx: *mut c_void),
    pub peek: extern "C" fn(ctx: *mut c_void) -> u8,
    pub advance: extern "C" fn(ctx: *mut c_void, n: i32),
    pub int_value: extern "C" fn(ctx: *mut c_void, out: *mut i32) -> bool,
}

struct Host<'a>(&'a EeWonExprHost);

impl ExprSource for Host<'_> {
    fn skip_whitespace(&mut self) {
        (self.0.skip_whitespace)(self.0.ctx)
    }
    fn peek(&mut self) -> u8 {
        (self.0.peek)(self.0.ctx)
    }
    fn advance(&mut self, n: i32) {
        (self.0.advance)(self.0.ctx, n)
    }
    fn int_value(&mut self) -> Option<i32> {
        let mut v = 0;
        (self.0.int_value)(self.0.ctx, &mut v).then_some(v)
    }
}

/// `parse_expression` through `host`: 0 with the value in `out`, else the `ExprError`.
///
/// # Safety
/// `host` is valid and its callbacks do not unwind; `out` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfg_expr(host: *const EeWonExprHost, level: i32, out: *mut i32) -> i32 {
    // SAFETY: valid (the contract)
    let mut h = Host(unsafe { &*host });
    match parse_expression(&mut h, level) {
        Ok(v) => {
            // SAFETY: writable (the contract)
            unsafe { *out = v };
            0
        }
        Err(e) => e as i32,
    }
}

#[no_mangle]
pub extern "C" fn ee_won_cfg_name_char(c: i8) -> bool {
    name_char(c)
}

/// `name_kind` over `len` bytes: the kind, the offset into `out_at`.
///
/// # Safety
/// `name` holds `len` bytes (or is NULL with 0); `out_at` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfg_name_kind(name: *const u8, len: usize, out_at: *mut usize) -> i32 {
    let n: &[u8] = if len == 0 || name.is_null() {
        &[]
    } else {
        // SAFETY: `len` bytes (the contract)
        unsafe { std::slice::from_raw_parts(name, len) }
    };
    let (k, at) = name_kind(n);
    // SAFETY: writable (the contract)
    unsafe { *out_at = at };
    k
}

#[cfg(test)]
mod tests {
    use super::*;

    /// A StringParser over bytes whose operands are decimal numbers (a name throws).
    struct Text {
        s: Vec<u8>,
        pos: i32,
    }

    impl ExprSource for Text {
        fn skip_whitespace(&mut self) {
            while self.peek() == b' ' {
                self.pos += 1;
            }
        }
        fn peek(&mut self) -> u8 {
            self.s.get(self.pos as usize).copied().unwrap_or(0)
        }
        fn advance(&mut self, n: i32) {
            self.pos += n;
        }
        fn int_value(&mut self) -> Option<i32> {
            let start = self.pos;
            let neg = self.peek() == b'-';
            if neg {
                self.pos += 1;
            }
            let mut v = 0i32;
            while self.peek().is_ascii_digit() {
                v = v.wrapping_mul(10).wrapping_add(i32::from(self.peek() - b'0'));
                self.pos += 1;
            }
            if self.pos == start + i32::from(neg) {
                return None;
            }
            Some(if neg { v.wrapping_neg() } else { v })
        }
    }

    fn eval(s: &str) -> (Result<i32, ExprError>, i32) {
        let mut t = Text { s: s.as_bytes().to_vec(), pos: 0 };
        let r = parse_expression(&mut t, 0);
        (r, t.pos)
    }

    #[test]
    fn expressions() {
        assert_eq!(eval("1 + 2 * 3"), (Ok(7), 9));
        assert_eq!(eval("(1 + 2) * 3"), (Ok(9), 11));
        assert_eq!(eval("10 - 4 - 3"), (Ok(3), 10));
        assert_eq!(eval("100 / 10 / 5"), (Ok(2), 12));
        // the parse stops before what it does not take
        assert_eq!(eval("7, 8"), (Ok(7), 1));
        assert_eq!(eval("(7"), (Err(ExprError::Paren), 2));
        assert_eq!(eval("7 / (3 - 3)"), (Err(ExprError::DivZero), 11));
        assert_eq!(eval("2 * x"), (Err(ExprError::Operand), 4));
        assert_eq!(eval("-2147483648 / -1"), (Ok(i32::MIN), 16));
        assert_eq!(eval("2147483647 + 1"), (Ok(i32::MIN), 14));
    }

    #[test]
    fn names() {
        assert!(name_char(b'a' as i8) && name_char(b'@' as i8) && name_char(b'.' as i8) && name_char(b'_' as i8));
        assert!(!name_char(b' ' as i8) && !name_char(b'(' as i8) && !name_char(-23));
        assert_eq!(name_kind(b"@ID_Ok"), (0, 4));
        assert_eq!(name_kind(b"@id_Ok"), (1, 1));
        assert_eq!(name_kind(b"@ID_"), (0, 4));
        assert_eq!(name_kind(b"@ID"), (1, 1));
        assert_eq!(name_kind(b"Parent.Inner.Width"), (2, 12));
        assert_eq!(name_kind(b"WIDTH"), (3, 0));
        assert_eq!(name_kind(b""), (3, 0));
    }
}
