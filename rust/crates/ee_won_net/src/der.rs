//! The key encoding of WONCrypt (`IntegerExtractor.cpp`, `IntegerInserter` in `ElGamalKey.cpp`): a DER-like
//! SEQUENCE of INTEGERs. The reader skips the sequence header and reads each INTEGER's big-endian magnitude; the
//! writer writes a two-byte length low byte first (the reader only ever skips that length, so its keys still load).

use crate::bigint::BigInt;

const INTEGER: u8 = 0x02;
const SEQUENCE_CONSTRUCTED: u8 = 0x30;

/// `IntegerExtractor`: a cursor over the key bytes and a validity flag that reading past the end clears.
pub struct Extractor<'a> {
    input: &'a [u8],
    offset: usize,
    valid: bool,
}

impl<'a> Extractor<'a> {
    /// `IntegerExtractor(data, size, buffered)`: with `buffered`, skips the sequence tag and its length (a wrong
    /// tag only stops the skipping; reading goes on after it).
    pub fn new(input: &'a [u8], buffered: bool) -> Extractor<'a> {
        let mut e = Extractor { input, offset: 0, valid: true };
        if buffered && e.get_byte() == SEQUENCE_CONSTRUCTED {
            e.length_decode();
        }
        e
    }

    fn get_byte(&mut self) -> u8 {
        if self.offset < self.input.len() {
            self.offset += 1;
            return self.input[self.offset - 1];
        }
        self.valid = false;
        0
    }

    fn peek_byte(&mut self) -> u8 {
        if self.offset < self.input.len() {
            return self.input[self.offset];
        }
        self.valid = false;
        0
    }

    /// Skips a length field: the short form, or a long form with at most two significant bytes.
    fn length_decode(&mut self) -> bool {
        let b = self.get_byte();
        if b & 0x80 == 0 {
            return self.valid;
        }
        let mut count = i32::from(b & 0x7f);
        if (self.input.len() as i64 - self.offset as i64) < i64::from(count) {
            return false;
        }
        let mut b = self.get_byte();
        while b == 0 && count > 1 {
            b = self.get_byte();
            count -= 1;
        }
        match count {
            1 => self.valid,
            2 => {
                self.get_byte();
                self.valid
            }
            _ => false,
        }
    }

    /// `Decode`: `length` bytes (leading zero bytes dropped) as an unsigned value.
    pub fn decode(&mut self, length: i32) -> Option<BigInt> {
        if !self.valid || self.input.len() <= self.offset {
            return None;
        }
        let mut length = length;
        self.peek_byte();
        while length > 0 && self.peek_byte() == 0 {
            self.offset += 1;
            length -= 1;
        }
        let bytes: Vec<u8> = (0..length.max(0)).map(|_| self.get_byte()).collect();
        let value = BigInt::from_binary(&bytes);
        self.valid.then_some(value)
    }

    /// `Extract`: the next INTEGER; None for another tag, a length of more than two bytes, or data past the end.
    pub fn extract(&mut self) -> Option<BigInt> {
        if !self.valid || self.get_byte() != INTEGER {
            return None;
        }
        let count = if self.peek_byte() & 0x80 == 0 {
            i32::from(self.get_byte())
        } else {
            let width = self.get_byte() & 0x7f;
            if width > 2 {
                return None;
            }
            let mut count = i32::from(self.get_byte());
            if width > 1 {
                count = (count << 8) | i32::from(self.get_byte());
            }
            count
        };
        self.decode(count)
    }
}

fn add_length(out: &mut Vec<u8>, len: usize) {
    if len > 0x7f {
        if len > 0xff {
            out.extend_from_slice(&[0x82, len as u8, (len >> 8) as u8]);
        } else {
            out.extend_from_slice(&[0x81, len as u8]);
        }
    } else {
        out.push(len as u8);
    }
}

/// `IntegerInserter`: the sequence of the values' `to_binary` forms.
pub fn insert(values: &[&BigInt]) -> Vec<u8> {
    let mut body = Vec::new();
    for v in values {
        let bytes = v.to_binary();
        body.push(INTEGER);
        add_length(&mut body, bytes.len());
        body.extend_from_slice(&bytes);
    }
    let mut out = vec![SEQUENCE_CONSTRUCTED];
    add_length(&mut out, body.len());
    out.extend_from_slice(&body);
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn insert_then_extract() {
        let values = [BigInt::from_i32(5), BigInt::from_binary(&[0x80; 200]), BigInt::zero()];
        let bytes = insert(&values.iter().collect::<Vec<_>>());
        // 3 + (3 + 201) + 3 body bytes: the one-byte long form
        assert_eq!(&bytes[..2], &[0x30, 0x81]);
        assert_eq!(bytes[2], 210);
        let mut e = Extractor::new(&bytes, true);
        assert_eq!(e.extract(), Some(BigInt::from_i32(5)));
        assert_eq!(e.extract(), Some(BigInt::from_binary(&[0x80; 200])));
        assert_eq!(e.extract(), Some(BigInt::zero()));
        assert_eq!(e.extract(), None);
    }

    #[test]
    fn two_byte_lengths_are_written_low_byte_first() {
        let big = BigInt::from_binary(&[0x80; 300]);
        let bytes = insert(&[&big]);
        // the sequence: 4 + 301 = 305 = 0x131 bytes; the integer: 301 = 0x12d bytes
        assert_eq!(&bytes[..7], &[0x30, 0x82, 0x31, 0x01, 0x02, 0x82, 0x2d]);
        // the reader takes the integer's length big-endian (0x2d01) and runs past the end
        assert_eq!(Extractor::new(&bytes, true).extract(), None);
    }

    #[test]
    fn short_and_bad_input() {
        assert_eq!(Extractor::new(&[0x30, 0x03, 0x02, 0x01, 0x07], true).extract(), Some(BigInt::from_i32(7)));
        // past the end
        assert_eq!(Extractor::new(&[0x30, 0x03, 0x02, 0x05, 0x07], true).extract(), None);
        // a wrong tag
        assert_eq!(Extractor::new(&[0x30, 0x03, 0x04, 0x01, 0x07], true).extract(), None);
        // a three-byte length is refused
        assert_eq!(Extractor::new(&[0x02, 0x83, 0, 0, 1, 7], false).extract(), None);
        // no sequence header: the first byte is still consumed
        assert_eq!(Extractor::new(&[0x99, 0x02, 0x01, 0x07], true).extract(), Some(BigInt::from_i32(7)));
        assert_eq!(Extractor::new(&[], true).extract(), None);
    }

    #[test]
    fn decode_drops_leading_zeros() {
        let mut e = Extractor::new(&[0, 0, 1, 2, 3], false);
        assert_eq!(e.decode(4), Some(BigInt::from_binary(&[1, 2])));
        let mut e = Extractor::new(&[0, 0], false);
        assert_eq!(e.decode(4), None);
    }
}
