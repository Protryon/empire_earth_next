//! Byte-string messages: the C++ compiler's error texts are bytes (a scalar's text is echoed as it was written), and
//! the Rust messages must stay byte-identical to them.

use std::io::Write;

/// One part of a message (`msg!`).
pub trait Part {
    fn put(&self, out: &mut Vec<u8>);
}

impl<T: Part + ?Sized> Part for &T {
    fn put(&self, out: &mut Vec<u8>) {
        (**self).put(out);
    }
}
impl Part for str {
    fn put(&self, out: &mut Vec<u8>) {
        out.extend_from_slice(self.as_bytes());
    }
}
impl Part for String {
    fn put(&self, out: &mut Vec<u8>) {
        out.extend_from_slice(self.as_bytes());
    }
}
impl Part for [u8] {
    fn put(&self, out: &mut Vec<u8>) {
        out.extend_from_slice(self);
    }
}
impl<const N: usize> Part for [u8; N] {
    fn put(&self, out: &mut Vec<u8>) {
        out.extend_from_slice(self.as_slice());
    }
}
impl Part for Vec<u8> {
    fn put(&self, out: &mut Vec<u8>) {
        out.extend_from_slice(self);
    }
}
impl Part for i64 {
    fn put(&self, out: &mut Vec<u8>) {
        let _ = write!(out, "{self}"); // no String in between
    }
}
impl Part for i32 {
    fn put(&self, out: &mut Vec<u8>) {
        let _ = write!(out, "{self}"); // no String in between
    }
}
impl Part for usize {
    fn put(&self, out: &mut Vec<u8>) {
        let _ = write!(out, "{self}"); // no String in between
    }
}
impl Part for u8 {
    fn put(&self, out: &mut Vec<u8>) {
        out.push(*self);
    }
}

/// A place in a text, "<what> line <n>", written only when a message needs it (the success paths build no text).
#[derive(Clone, Copy, Debug)]
pub struct At<'a> {
    pub what: &'a str,
    pub line: i64,
}

impl Part for At<'_> {
    fn put(&self, out: &mut Vec<u8>) {
        out.extend_from_slice(self.what.as_bytes());
        out.extend_from_slice(b" line ");
        self.line.put(out);
    }
}

/// Concatenates parts (string literals, byte slices, integers) into a `Vec<u8>`.
#[macro_export]
macro_rules! msg {
    ($($part:expr),+ $(,)?) => {{
        let mut out: Vec<u8> = Vec::with_capacity(64); // a place or a message: one allocation, no regrowth
        $( $crate::msg::Part::put(&$part, &mut out); )+
        out
    }};
}

#[cfg(test)]
mod tests {
    #[test]
    fn concatenates_every_kind_of_part() {
        let s: &[u8] = b"bytes";
        let v = b"vec".to_vec();
        assert_eq!(msg!("a", s, &v, 7i64, -3i32, b'!'), b"abytesvec7-3!".to_vec());
        let at = super::At { what: "combat", line: 12 };
        assert_eq!(msg!(at, ": x"), b"combat line 12: x".to_vec());
        assert_eq!(msg!(&&at, b"!"), b"combat line 12!".to_vec());
    }
}
