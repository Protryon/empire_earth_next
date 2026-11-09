//! The PKWARE Data Compression Library stream ("implode"/"explode"): this project's implementation of the format, as
//! the former `src/ThirdParty/pkdcl` C code. Stream: byte 0 = literal mode, byte 1 = log2(dictionary) - 6 (4, 5, 6 for
//! 1024, 2048, 4096), then a bit stream read from bit 0 of each byte upwards. Each token starts with one bit:
//! 0: a literal; 1: a copy (length code + extra bits, then distance code + low bits).
//!
//! Not byte-matched to PKWARE's encoder, but byte-identical to the former C encoder (the golden vectors in this
//! crate's tests come from it).

use crate::dcl_tables::*;

/// Results (the values the original library and `UDataCompression` use).
pub(crate) const PK_OK: u32 = 0;
pub(crate) const PK_BAD_DICTIONARY: u32 = 1;
pub(crate) const PK_BAD_MODE: u32 = 2;
pub(crate) const PK_BAD_DATA: u32 = 3;
pub(crate) const PK_ABORT: u32 = 4;

pub(crate) const PK_BINARY: u32 = 0;
pub(crate) const PK_ASCII: u32 = 1;

const PK_MIN_COPY: u32 = 2;
const PK_MAX_COPY: u32 = 518;
const PK_END_LENGTH: u32 = 519;

const HISTORY: u32 = 0x1000;
const WINDOW: usize = HISTORY as usize + 0x1000 + PK_MAX_COPY as usize;
const HASH_SIZE: usize = 0x800;
const NO_POS: u16 = 0xffff;
const MAX_CHAIN: u32 = 0x1000;
const OUT_PIECE: usize = 0x800;

struct Imploder<'a> {
    input: &'a [u8],
    input_pos: usize,
    write: &'a mut dyn FnMut(&[u8]),
    mode: u32,
    dict_size: u32,
    dict_bits: u32,
    bits: u64,
    bit_count: u32,
    out: Vec<u8>,
    end: u32,
    next_insert: u32,
    head: Box<[u16; HASH_SIZE]>,
    prev: Vec<u16>,
    window: Vec<u8>,
}

impl Imploder<'_> {
    fn flush_piece(&mut self) {
        let piece = std::mem::take(&mut self.out);
        (self.write)(&piece);
    }

    fn put_bits(&mut self, value: u32, count: u32) {
        self.bits |= u64::from(value) << self.bit_count;
        self.bit_count += count;
        while self.bit_count >= 8 {
            self.out.push(self.bits as u8);
            self.bits >>= 8;
            self.bit_count -= 8;
            if self.out.len() == OUT_PIECE {
                self.flush_piece();
            }
        }
    }

    fn put_literal(&mut self, c: u8) {
        self.put_bits(0, 1);
        if self.mode == PK_BINARY {
            self.put_bits(u32::from(c), 8);
        } else {
            self.put_bits(u32::from(ASCII_CODE[c as usize]), u32::from(ASCII_BITS[c as usize]));
        }
    }

    /// A copy of `length` bytes (2..518, or `PK_END_LENGTH` without a distance) from `distance` bytes back.
    fn put_copy(&mut self, length: u32, distance: u32) {
        let value = length - PK_MIN_COPY;
        let mut code = 15usize;
        while u32::from(LENGTH_BASE[code]) > value {
            code -= 1;
        }
        self.put_bits(1, 1);
        self.put_bits(u32::from(LENGTH_CODE[code]), u32::from(LENGTH_BITS[code]));
        self.put_bits(value - u32::from(LENGTH_BASE[code]), u32::from(LENGTH_EXTRA_BITS[code]));
        if length == PK_END_LENGTH {
            return;
        }
        let distance = distance - 1;
        let low_bits = if length == PK_MIN_COPY { 2 } else { self.dict_bits };
        let high = (distance >> low_bits) as usize;
        self.put_bits(u32::from(DIST_CODE[high]), u32::from(DIST_BITS[high]));
        self.put_bits(distance & ((1 << low_bits) - 1), low_bits);
    }

    fn hash(&self, pos: u32) -> usize {
        ((u32::from(self.window[pos as usize]) << 3 ^ u32::from(self.window[pos as usize + 1])) as usize)
            & (HASH_SIZE - 1)
    }

    /// Adds the positions up to `pos` (inclusive) that have a following byte to the hash chains.
    fn insert_up_to(&mut self, pos: u32) {
        while self.next_insert <= pos && self.next_insert + 1 < self.end {
            let h = self.hash(self.next_insert);
            self.prev[self.next_insert as usize] = self.head[h];
            self.head[h] = self.next_insert as u16;
            self.next_insert += 1;
        }
    }

    /// The longest copy for position `pos` (its length, 0 if none; the distance in `distance`). Only positions before
    /// `pos` are in the chains.
    fn find_copy(&self, pos: u32, distance: &mut u32) -> u32 {
        let here = pos as usize;
        let mut limit = self.end - pos;
        let mut best = 0u32;
        if limit > PK_MAX_COPY {
            limit = PK_MAX_COPY;
        }
        if limit < PK_MIN_COPY {
            return 0;
        }
        let mut tries = MAX_CHAIN;
        let mut candidate = self.head[self.hash(pos)];
        while candidate != NO_POS && tries != 0 {
            let there = candidate as usize;
            let back = pos - u32::from(candidate);
            if back > self.dict_size {
                break; // the chains run from the newest position back
            }
            let w = &self.window;
            if w[there + best as usize] == w[here + best as usize] && w[there] == w[here] && w[there + 1] == w[here + 1]
            {
                let mut length = 2u32;
                while length < limit && w[there + length as usize] == w[here + length as usize] {
                    length += 1;
                }
                // a 2-byte copy has only 8 distance bits
                if !(length == PK_MIN_COPY && back > 256) && length > best {
                    best = length;
                    *distance = back;
                    if length == limit {
                        break;
                    }
                }
            }
            candidate = self.prev[there];
            tries -= 1;
        }
        best
    }

    /// Drops the window's oldest bytes so that `pos` keeps `HISTORY` bytes of history; returns the shift.
    fn slide(&mut self, pos: u32) -> u32 {
        let shift = pos - HISTORY;
        let (shift_usize, end) = (shift as usize, self.end as usize);
        self.window.copy_within(shift_usize..end, 0);
        self.prev.copy_within(shift_usize..end, 0);
        self.end -= shift;
        self.next_insert -= shift;
        let fix = |v: u16| if v == NO_POS || u32::from(v) < shift { NO_POS } else { v - shift as u16 };
        for v in self.head.iter_mut() {
            *v = fix(*v);
        }
        for v in &mut self.prev[..self.end as usize] {
            *v = fix(*v);
        }
        shift
    }

    fn read_more(&mut self, input_ended: &mut bool) {
        while !*input_ended && (self.end as usize) < WINDOW {
            let size = WINDOW - self.end as usize;
            let got = size.min(self.input.len() - self.input_pos);
            if got == 0 {
                *input_ended = true;
            } else {
                let start = self.end as usize;
                self.window[start..start + got].copy_from_slice(&self.input[self.input_pos..self.input_pos + got]);
                self.input_pos += got;
                self.end += got as u32;
            }
        }
    }

    fn compress(&mut self) {
        let mut pos = 0u32;
        let mut input_ended = false;
        loop {
            self.read_more(&mut input_ended);
            // before the input ends, only positions with a full longest copy of lookahead
            let stop = if input_ended { self.end } else { self.end - PK_MAX_COPY };
            while pos < stop {
                let mut distance = 0u32;
                let mut next_distance = 0u32;
                let mut length = self.find_copy(pos, &mut distance);
                if length != 0 && length < 8 && pos + 1 < self.end {
                    // lazy: a longer copy one byte later wins (by one byte only when this copy is far)
                    self.insert_up_to(pos);
                    let next = self.find_copy(pos + 1, &mut next_distance);
                    if next > length && (next > length + 1 || distance > 0x81) {
                        length = 0;
                    }
                }
                if length == 0 {
                    let c = self.window[pos as usize];
                    self.put_literal(c);
                    self.insert_up_to(pos);
                    pos += 1;
                } else {
                    self.put_copy(length, distance);
                    self.insert_up_to(pos + length - 1);
                    pos += length;
                }
            }
            if input_ended {
                break;
            }
            if pos > HISTORY {
                pos -= self.slide(pos);
            }
        }
        self.put_copy(PK_END_LENGTH, 0);
        if self.bit_count != 0 {
            self.put_bits(0, 8 - self.bit_count);
        }
        if !self.out.is_empty() {
            self.flush_piece();
        }
    }
}

/// Compresses `input` (`mode` = `PK_BINARY` or `PK_ASCII`, `dict` = 1024, 2048 or 4096). `write` receives the stream in
/// 0x800-byte pieces, the rest at the end. The dictionary size is checked before the mode, and nothing is written on
/// failure (as the original library).
pub fn implode(input: &[u8], mode: u32, dict: u32, write: &mut dyn FnMut(&[u8])) -> u32 {
    let dict_bits = match dict {
        0x400 => 4,
        0x800 => 5,
        0x1000 => 6,
        _ => return PK_BAD_DICTIONARY,
    };
    if mode != PK_BINARY && mode != PK_ASCII {
        return PK_BAD_MODE;
    }
    let mut imploder = Imploder {
        input,
        input_pos: 0,
        write,
        mode,
        dict_size: dict,
        dict_bits,
        bits: 0,
        bit_count: 0,
        out: Vec::with_capacity(OUT_PIECE + 4),
        end: 0,
        next_insert: 0,
        head: Box::new([NO_POS; HASH_SIZE]),
        prev: vec![0u16; WINDOW],
        window: vec![0u8; WINDOW],
    };
    imploder.out.push(mode as u8);
    imploder.out.push(dict_bits as u8);
    imploder.compress();
    PK_OK
}

const LONG_LITERAL: u16 = 0xffff;
const WINDOW_HISTORY: usize = 0x1000; // output kept for copies
const WINDOW_FLUSH: usize = 0x2000; // hand out WINDOW_FLUSH - WINDOW_HISTORY bytes once output reaches this

struct Exploder<'a> {
    input: &'a [u8],
    input_pos: usize,
    write: &'a mut dyn FnMut(&[u8]),
    bits: u64,
    bit_count: u32,
    mode: u32,
    dict_bits: u32,
    out_pos: usize,
    length_of: [u8; 256],
    dist_of: [u8; 256],
    literal_of: [u16; 256],
    long_literals: Vec<u8>,
    window: Vec<u8>,
}

impl Exploder<'_> {
    /// Tops up the bit buffer to at least 25 bits while input lasts.
    fn fill(&mut self) {
        while self.bit_count <= 24 {
            let Some(&byte) = self.input.get(self.input_pos) else {
                return;
            };
            self.input_pos += 1;
            self.bits |= u64::from(byte) << self.bit_count;
            self.bit_count += 8;
        }
    }

    /// The next n (<= 16) bits; bits past the end of the input read as 0.
    fn peek(&mut self, n: u32) -> u32 {
        if self.bit_count < n {
            self.fill();
        }
        (self.bits & ((1u64 << n) - 1)) as u32
    }

    /// Consumes n bits; false when the input ends first.
    fn drop_bits(&mut self, n: u32) -> bool {
        if self.bit_count < n {
            self.fill();
            if self.bit_count < n {
                return false;
            }
        }
        self.bits >>= n;
        self.bit_count -= n;
        true
    }

    fn build_lookups(&mut self) {
        // an n-bit code value c owns the 8-bit lookup slots c, c + 2^n, c + 2 * 2^n, ...
        fn build(lookup: &mut [u8; 256], code: &[u8], bits: &[u8]) {
            for (i, (&c, &b)) in code.iter().zip(bits).enumerate() {
                let mut slot = usize::from(c);
                while slot < 256 {
                    lookup[slot] = i as u8;
                    slot += 1 << b;
                }
            }
        }
        build(&mut self.length_of, &LENGTH_CODE, &LENGTH_BITS);
        build(&mut self.dist_of, &DIST_CODE, &DIST_BITS);
    }

    fn build_ascii_lookup(&mut self) {
        self.literal_of = [LONG_LITERAL; 256];
        self.long_literals.clear();
        for c in 0..256usize {
            if ASCII_BITS[c] <= 8 {
                let mut slot = usize::from(ASCII_CODE[c]);
                while slot < 256 {
                    self.literal_of[slot] = c as u16;
                    slot += 1 << ASCII_BITS[c];
                }
            } else {
                self.long_literals.push(c as u8);
            }
        }
    }

    /// Decodes one literal (after its 0 flag bit); None when the input ends.
    fn read_literal(&mut self) -> Option<u8> {
        if self.mode == PK_BINARY {
            let c = self.peek(8);
            return self.drop_bits(8).then_some(c as u8);
        }
        let slot = self.peek(8) as usize;
        let mut c = self.literal_of[slot];
        if c == LONG_LITERAL {
            let next = self.peek(16);
            let long = self.long_literals.iter().find(|&&l| {
                let bits = ASCII_BITS[l as usize];
                (next & ((1u32 << bits) - 1)) == u32::from(ASCII_CODE[l as usize])
            });
            c = u16::from(*long?);
        }
        let bits = u32::from(ASCII_BITS[c as usize]);
        self.drop_bits(bits).then_some(c as u8)
    }

    /// Decodes a copy length (after its 1 flag bit): 2..518, `PK_END_LENGTH`, or None when the input ends.
    fn read_length(&mut self) -> Option<u32> {
        let slot = self.peek(8) as usize;
        let code = usize::from(self.length_of[slot]);
        let extra = u32::from(LENGTH_EXTRA_BITS[code]);
        if !self.drop_bits(u32::from(LENGTH_BITS[code])) {
            return None;
        }
        let mut value = u32::from(LENGTH_BASE[code]);
        if extra != 0 {
            value += self.peek(extra);
            // the end code counts even when the input stops inside its extra bits
            if !self.drop_bits(extra) && value + PK_MIN_COPY != PK_END_LENGTH {
                return None;
            }
        }
        Some(value + PK_MIN_COPY)
    }

    /// Decodes a copy distance (1..dictionary size) for a copy of `length`; None when the input ends.
    fn read_distance(&mut self, length: u32) -> Option<usize> {
        let slot = self.peek(8) as usize;
        let high = usize::from(self.dist_of[slot]);
        let low_bits = if length == PK_MIN_COPY { 2 } else { self.dict_bits };
        if !self.drop_bits(u32::from(DIST_BITS[high])) {
            return None;
        }
        let low = self.peek(low_bits) as usize;
        if !self.drop_bits(low_bits) {
            return None;
        }
        Some(((high << low_bits) | low) + 1)
    }

    fn write_out(&mut self, count: usize) {
        (self.write)(&self.window[WINDOW_HISTORY..WINDOW_HISTORY + count]);
    }

    /// Decodes tokens up to the end code: `PK_OK`, or `PK_ABORT` when the input ends first.
    fn expand(&mut self) -> u32 {
        let mut result = PK_ABORT;
        self.out_pos = WINDOW_HISTORY;
        loop {
            let flag = self.peek(1);
            if !self.drop_bits(1) {
                break;
            }
            if flag == 0 {
                let Some(c) = self.read_literal() else {
                    break;
                };
                self.window[self.out_pos] = c;
                self.out_pos += 1;
            } else {
                let Some(length) = self.read_length() else {
                    break;
                };
                if length == PK_END_LENGTH {
                    result = PK_OK;
                    break;
                }
                let Some(distance) = self.read_distance(length) else {
                    break;
                };
                // byte by byte: a copy may overlap its own output
                for from in (self.out_pos - distance..).take(length as usize) {
                    self.window[self.out_pos] = self.window[from];
                    self.out_pos += 1;
                }
            }
            if self.out_pos >= WINDOW_FLUSH {
                self.write_out(WINDOW_FLUSH - WINDOW_HISTORY);
                self.window.copy_within(WINDOW_FLUSH - WINDOW_HISTORY..self.out_pos, 0);
                self.out_pos -= WINDOW_FLUSH - WINDOW_HISTORY;
            }
        }
        self.write_out(self.out_pos - WINDOW_HISTORY);
        result
    }
}

/// Decompresses a DCL stream. `write` receives the output in 0x1000-byte pieces and the rest at the end, also after an
/// abort. Behaviour kept from the original library: input of at most 4 bytes is `PK_BAD_DATA` (the first read); the
/// dictionary byte is checked before the mode; input that ends before the end code is `PK_ABORT` (except inside the
/// end code's last extra bits); a copy may reach back before the first output byte into zeroes.
pub fn explode(input: &[u8], write: &mut dyn FnMut(&[u8])) -> u32 {
    if input.len() <= 4 {
        return PK_BAD_DATA;
    }
    let mode = u32::from(input[0]);
    let dict_bits = u32::from(input[1]);
    if !(4..=6).contains(&dict_bits) {
        return PK_BAD_DICTIONARY;
    }
    if mode != PK_BINARY && mode != PK_ASCII {
        return PK_BAD_MODE;
    }
    let mut exploder = Exploder {
        input,
        input_pos: 2,
        write,
        bits: 0,
        bit_count: 0,
        mode,
        dict_bits,
        out_pos: WINDOW_HISTORY,
        length_of: [0; 256],
        dist_of: [0; 256],
        literal_of: [0; 256],
        long_literals: Vec::new(),
        window: vec![0u8; WINDOW_FLUSH + PK_MAX_COPY as usize],
    };
    if mode != PK_BINARY {
        exploder.build_ascii_lookup();
    }
    exploder.build_lookups();
    exploder.expand()
}

#[cfg(test)]
mod tests {
    use super::*;

    fn pack(input: &[u8], mode: u32, dict: u32) -> Vec<u8> {
        let mut out = Vec::new();
        assert_eq!(implode(input, mode, dict, &mut |p: &[u8]| out.extend_from_slice(p)), PK_OK);
        out
    }

    fn unpack(input: &[u8]) -> (u32, Vec<u8>) {
        let mut out = Vec::new();
        let result = explode(input, &mut |p: &[u8]| out.extend_from_slice(p));
        (result, out)
    }

    #[test]
    fn round_trips_every_mode_and_dictionary() {
        let mut x = 1u32;
        let data: Vec<u8> = (0..30_000)
            .map(|_| {
                x = x.wrapping_mul(1103515245).wrapping_add(12345);
                (x >> 16) as u8 & 7
            })
            .collect();
        for mode in [PK_BINARY, PK_ASCII] {
            for dict in [0x400, 0x800, 0x1000] {
                let z = pack(&data, mode, dict);
                let (result, back) = unpack(&z);
                assert_eq!(result, PK_OK);
                assert_eq!(back, data);
                assert!(z.len() < data.len());
            }
        }
    }

    #[test]
    fn empty_input_is_an_end_code_only_stream() {
        // 2 header bytes, the end code (15 bits) makes the stream 4 bytes: too short for explode's first read
        let z = pack(b"", PK_BINARY, 0x1000);
        assert_eq!(z.len(), 4);
        let (result, _) = unpack(&z);
        assert_eq!(result, PK_BAD_DATA);
    }

    #[test]
    fn bad_arguments() {
        let mut calls = 0;
        assert_eq!(implode(b"x", PK_BINARY, 0x300, &mut |_| calls += 1), PK_BAD_DICTIONARY);
        // the dictionary is checked first
        assert_eq!(implode(b"x", 9, 0x300, &mut |_| calls += 1), PK_BAD_DICTIONARY);
        assert_eq!(implode(b"x", 9, 0x400, &mut |_| calls += 1), PK_BAD_MODE);
        assert_eq!(calls, 0);
        assert_eq!(unpack(&[0, 9, 0, 0, 0]).0, PK_BAD_DICTIONARY);
        assert_eq!(unpack(&[9, 5, 0, 0, 0]).0, PK_BAD_MODE);
        assert_eq!(unpack(&[1, 2, 3]).0, PK_BAD_DATA);
    }

    #[test]
    fn truncated_stream_aborts_and_keeps_what_decoded() {
        let data = b"hello hello hello hello world, hello again";
        let z = pack(data, PK_BINARY, 0x1000);
        let (result, out) = unpack(&z[..z.len() - 3]);
        assert_eq!(result, PK_ABORT);
        assert!(data.starts_with(&out) && !out.is_empty());
    }

    #[test]
    fn output_pieces_have_the_documented_sizes() {
        let data = vec![7u8; 20_000];
        let z = pack(&data, PK_BINARY, 0x400);
        let mut sizes = Vec::new();
        assert_eq!(explode(&z, &mut |p: &[u8]| sizes.push(p.len())), PK_OK);
        assert!(sizes[..sizes.len() - 1].iter().all(|&s| s == 0x1000), "{sizes:?}");
        assert_eq!(sizes.iter().sum::<usize>(), 20_000);
    }

    #[test]
    fn slide_keeps_matches_valid_across_many_windows() {
        // a long repeating pattern forces many slides; every copy must still be right
        let data: Vec<u8> = (0..200_000u32).map(|i| (i / 5) as u8).collect();
        let z = pack(&data, PK_BINARY, 0x1000);
        assert_eq!(unpack(&z), (PK_OK, data));
    }
}
