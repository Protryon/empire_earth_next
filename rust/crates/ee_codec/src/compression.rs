//! `UDataCompression`: a DCL stream behind a "PK01" + uncompressed length + reserved dword header. The result codes
//! and the output-capacity rules are the C++ class's: a piece of output that does not fit is dropped but still counted.

use crate::dcl;

pub(crate) const CODE_OK: i32 = 0;
pub(crate) const CODE_NOT_COMPRESSED: i32 = 0x13;
pub(crate) const CODE_ALREADY_COMPRESSED: i32 = 0x14;
pub(crate) const CODE_FAILURE: i32 = 0x15;
pub(crate) const CODE_OUTPUT_TOO_SMALL: i32 = 0x16;

const SIGNATURE: &[u8; 4] = b"PK01";
const HEADER_SIZE: usize = 12;

/// Writes pieces into a fixed buffer; a piece that does not fit is dropped but its size still counts.
struct Sink<'a> {
    out: &'a mut [u8],
    position: usize,
}

impl Sink<'_> {
    fn put(&mut self, piece: &[u8]) {
        if self.position + piece.len() <= self.out.len() {
            self.out[self.position..self.position + piece.len()].copy_from_slice(piece);
        }
        self.position += piece.len();
    }
}

/// Whether the data is longer than the header and starts with "PK01".
pub fn is_compressed(input: &[u8]) -> bool {
    input.len() > HEADER_SIZE && input.starts_with(SIGNATURE)
}

/// The uncompressed size from a "PK01" header; `(CODE_NOT_COMPRESSED, 0)` when the data is not compressed.
pub fn decompressed_size(input: &[u8]) -> (i32, u32) {
    if !is_compressed(input) {
        return (CODE_NOT_COMPRESSED, 0);
    }
    (CODE_OK, u32::from_le_bytes([input[4], input[5], input[6], input[7]]))
}

/// Compresses `input` behind a "PK01" header into `out`; returns `(code, length)`. `level` is the dictionary size.
pub fn compress(input: &[u8], out: &mut [u8], level: u32) -> (i32, u32) {
    if is_compressed(input) {
        return (CODE_ALREADY_COMPRESSED, 0);
    }
    if out.len() < HEADER_SIZE {
        // C++ wrote the header past a smaller buffer; here it is reported as too small and nothing is written
        return (CODE_OUTPUT_TOO_SMALL, 0);
    }
    out[..4].copy_from_slice(SIGNATURE);
    out[4..8].copy_from_slice(&(input.len() as u32).to_le_bytes());
    out[8..12].copy_from_slice(&[0, 0, 0, 0]);
    let mut sink = Sink { out: &mut out[HEADER_SIZE..], position: 0 };
    if dcl::implode(input, dcl::PK_BINARY, level, &mut |p| sink.put(p)) != dcl::PK_OK {
        return (CODE_FAILURE, 0);
    }
    if sink.position <= sink.out.len() {
        (CODE_OK, (sink.position + HEADER_SIZE) as u32)
    } else {
        (CODE_OUTPUT_TOO_SMALL, 0)
    }
}

/// Decompresses a "PK01" block into `out`; returns `(code, length)`.
pub fn decompress(input: &[u8], out: &mut [u8]) -> (i32, u32) {
    if !is_compressed(input) {
        return (CODE_NOT_COMPRESSED, 0);
    }
    let mut sink = Sink { out, position: 0 };
    if dcl::explode(&input[HEADER_SIZE..], &mut |p| sink.put(p)) != dcl::PK_OK {
        return (CODE_FAILURE, 0);
    }
    if sink.position <= sink.out.len() {
        (CODE_OK, sink.position as u32)
    } else {
        (CODE_OUTPUT_TOO_SMALL, 0)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn header_and_round_trip() {
        let data = b"Empire Earth Empire Earth Empire Earth";
        let mut out = [0u8; 200];
        let (code, len) = compress(data, &mut out, 0x1000);
        assert_eq!(code, CODE_OK);
        assert_eq!(&out[..4], b"PK01");
        assert_eq!(u32::from_le_bytes(out[4..8].try_into().unwrap()), data.len() as u32);
        assert_eq!(&out[8..12], &[0, 0, 0, 0]);
        assert_eq!(decompressed_size(&out[..len as usize]), (CODE_OK, data.len() as u32));
        let mut back = [0u8; 100];
        assert_eq!(decompress(&out[..len as usize], &mut back), (CODE_OK, data.len() as u32));
        assert_eq!(&back[..data.len()], data);
    }

    #[test]
    fn already_compressed_is_refused() {
        let mut block = vec![0u8; 20];
        block[..4].copy_from_slice(b"PK01");
        let mut out = [9u8; 40];
        assert_eq!(compress(&block, &mut out, 0x1000), (CODE_ALREADY_COMPRESSED, 0));
        // the output buffer is untouched
        assert_eq!(out, [9u8; 40]);
    }

    #[test]
    fn not_compressed() {
        let mut out = [0u8; 8];
        assert_eq!(decompress(b"hello", &mut out), (CODE_NOT_COMPRESSED, 0));
        assert_eq!(decompressed_size(b"PK02xxxxxxxxx"), (CODE_NOT_COMPRESSED, 0));
        assert!(!is_compressed(b"PK01xxxxxxxx")); // exactly the header: not compressed
        assert!(is_compressed(b"PK01xxxxxxxxx"));
    }

    #[test]
    fn output_too_small() {
        let data = vec![b'x'; 5000];
        let mut tiny = [0u8; 30];
        assert_eq!(compress(&data, &mut tiny, 0x1000), (CODE_OUTPUT_TOO_SMALL, 0));
        let mut big = vec![0u8; 8000];
        let (code, len) = compress(&data, &mut big, 0x1000);
        assert_eq!(code, CODE_OK);
        let mut small_out = [0u8; 100];
        assert_eq!(decompress(&big[..len as usize], &mut small_out), (CODE_OUTPUT_TOO_SMALL, 0));
    }

    #[test]
    fn header_alone_does_not_fit_in_a_tiny_output() {
        let mut out = [1u8; 11];
        assert_eq!(compress(b"abc", &mut out, 0x1000), (CODE_OUTPUT_TOO_SMALL, 0));
        assert_eq!(out, [1u8; 11]);
    }

    #[test]
    fn bad_dictionary_is_a_failure() {
        let mut out = [0u8; 100];
        assert_eq!(compress(b"abc", &mut out, 512), (CODE_FAILURE, 0));
    }

    #[test]
    fn corrupt_body_is_a_failure() {
        let mut block = vec![0u8; 16];
        block[..4].copy_from_slice(b"PK01");
        block[13] = 7; // an invalid dictionary byte (too short a body is BAD_DATA, also a failure)
        let mut out = [0u8; 8];
        assert_eq!(decompress(&block, &mut out), (CODE_FAILURE, 0));
    }

    #[test]
    fn empty_input_compresses_but_does_not_decompress() {
        // the C++ implode writes a 4-byte stream for empty input, and explode's first read refuses 4 bytes
        let mut out = [0u8; 40];
        let (code, len) = compress(b"", &mut out, 0x1000);
        assert_eq!((code, len), (CODE_OK, 16));
        let mut back = [0u8; 8];
        assert_eq!(decompress(&out[..len as usize], &mut back), (CODE_FAILURE, 0));
    }
}
