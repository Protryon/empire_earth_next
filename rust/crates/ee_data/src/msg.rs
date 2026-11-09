//! Byte-string messages: the C++ compiler's error texts are bytes (a scalar's text is echoed as it was written), and
//! the Rust messages must stay byte-identical to them.

/// One part of a message (`msg!`).
pub trait Part {
    fn put(&self, out: &mut Vec<u8>);
}

impl Part for &str {
    fn put(&self, out: &mut Vec<u8>) {
        out.extend_from_slice(self.as_bytes());
    }
}
impl Part for String {
    fn put(&self, out: &mut Vec<u8>) {
        out.extend_from_slice(self.as_bytes());
    }
}
impl Part for &[u8] {
    fn put(&self, out: &mut Vec<u8>) {
        out.extend_from_slice(self);
    }
}
impl<const N: usize> Part for &[u8; N] {
    fn put(&self, out: &mut Vec<u8>) {
        out.extend_from_slice(self.as_slice());
    }
}
impl Part for &Vec<u8> {
    fn put(&self, out: &mut Vec<u8>) {
        out.extend_from_slice(self);
    }
}
impl Part for Vec<u8> {
    fn put(&self, out: &mut Vec<u8>) {
        out.extend_from_slice(self);
    }
}
impl Part for i64 {
    fn put(&self, out: &mut Vec<u8>) {
        out.extend_from_slice(self.to_string().as_bytes());
    }
}
impl Part for i32 {
    fn put(&self, out: &mut Vec<u8>) {
        out.extend_from_slice(self.to_string().as_bytes());
    }
}
impl Part for usize {
    fn put(&self, out: &mut Vec<u8>) {
        out.extend_from_slice(self.to_string().as_bytes());
    }
}
impl Part for u8 {
    fn put(&self, out: &mut Vec<u8>) {
        out.push(*self);
    }
}

/// Concatenates parts (string literals, byte slices, integers) into a `Vec<u8>`.
#[macro_export]
macro_rules! msg {
    ($($part:expr),+ $(,)?) => {{
        let mut out: Vec<u8> = Vec::new();
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
    }
}
