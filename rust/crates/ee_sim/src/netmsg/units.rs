//! The box selection's compact unit list (`EEMSelectUnits`' buffer; `CompressUnits`/`DecompressUnits` in
//! `EEMessage.cpp`).
//!
//! Byte 0 is the owner (it replaces the ids' top byte on decoding). Then runs: a count byte whose low 6 bits count
//! the run's ids and whose top 2 bits say how many bytes of the ids' middle 16 bits follow (0, 1 or 2; 3 is the
//! empty list's 0xff), then each id's low byte. The runs go by ascending middle bits (the original's `std::map`), and
//! at most 63 ids are kept, in each group's insertion order.

use std::collections::BTreeMap;

/// The most ids a buffer keeps.
pub const MAX_IDS: usize = 63;
/// The largest buffer: the owner, then for 63 ids in 63 groups a three-byte run head and the id.
pub const MAX_BUFFER: usize = 1 + MAX_IDS * 4;

/// Packs `ids` for `owner` into `out`; returns the byte count (`CompressUnits`). Only each id's low 24 bits count.
pub fn compress(ids: &[u32], owner: u32, out: &mut [u8; 256]) -> u8 {
    let mut groups: BTreeMap<u32, Vec<u8>> = BTreeMap::new();
    for &id in ids {
        groups.entry((id >> 8) & 0xffff).or_default().push(id as u8);
    }
    out[0] = owner as u8;
    let mut at = 1usize;
    let mut remaining = MAX_IDS;
    for (&group, lows) in &groups {
        if remaining == 0 {
            break;
        }
        let count = lows.len().min(remaining);
        if group == 0 {
            out[at] = count as u8;
            at += 1;
        } else if group <= 0xff {
            out[at] = count as u8 + 0x40;
            out[at + 1] = group as u8;
            at += 2;
        } else {
            out[at] = count as u8 + 0x80;
            out[at + 1] = (group >> 8) as u8;
            out[at + 2] = group as u8;
            at += 3;
        }
        out[at..at + count].copy_from_slice(&lows[..count]);
        at += count;
        remaining -= count;
    }
    if at == 1 {
        out[at] = 0xff;
        at += 1;
    }
    at as u8
}

/// The ids of a buffer of `size` bytes (`DecompressUnits`), as the 32-bit values the original's `long` holds on
/// Windows (on LP64 its `long` is the same value sign-extended, which the C++ caller does).
///
/// The original reads on until its index equals `size` exactly, trusting the buffer; on a buffer `compress` did not
/// make (an index past `size` or past the 256 bytes) this stops where the original would read out of bounds.
pub fn decompress(buffer: &[u8; 256], size: u8, out: &mut Vec<i32>) {
    out.clear();
    let size = size as usize;
    let owner = (buffer[0] as u32) << 24;
    let mut i = 1usize;
    let byte = |i: usize| buffer.get(i).copied();
    loop {
        let Some(head) = byte(i) else { return };
        let mut count = (head & 0x3f) as usize;
        let base = match head >> 6 {
            0 => owner,
            1 => {
                i += 1;
                let Some(b) = byte(i) else { return };
                ((b as u32) << 8) | owner
            }
            2 => {
                let (Some(hi), Some(lo)) = (byte(i + 1), byte(i + 2)) else { return };
                i += 2;
                let group = ((hi as u32) << 8).wrapping_add(lo as u32);
                ((group << 8) & 0x00ff_ff00) | owner
            }
            _ => {
                count = 0;
                0
            }
        };
        i += 1;
        for _ in 0..count {
            let Some(low) = byte(i) else { return };
            out.push((low as u32 | base) as i32);
            i += 1;
        }
        // `while (i != inSize)`; past the size the original reads on out of bounds: stop at the buffer's end instead
        if i == size || i >= buffer.len() {
            return;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn round(ids: &[u32], owner: u32) -> (Vec<u8>, Vec<i32>) {
        let mut buf = [0u8; 256];
        let n = compress(ids, owner, &mut buf);
        let mut out = Vec::new();
        decompress(&buf, n, &mut out);
        (buf[..n as usize].to_vec(), out)
    }

    #[test]
    fn the_empty_list_is_the_owner_and_0xff() {
        assert_eq!(round(&[], 3), (vec![3, 0xff], vec![]));
    }

    #[test]
    fn runs_by_middle_bits() {
        // group 0, group 0x12 (one byte), group 0x1234 (two bytes)
        let (b, out) = round(&[0x05, 0x1206, 0x123407, 0x08], 2);
        assert_eq!(b, vec![2, 2, 0x05, 0x08, 0x41, 0x12, 0x06, 0x81, 0x12, 0x34, 0x07]);
        assert_eq!(out, vec![0x0200_0005, 0x0200_0008, 0x0200_1206, 0x0212_3407]);
    }

    #[test]
    fn the_top_byte_is_the_owners() {
        let (_, out) = round(&[0xff00_0001], 0x80);
        assert_eq!(out, vec![0x8000_0001u32 as i32]);
        // the owner is a byte
        let (b, _) = round(&[1], 0x1_02);
        assert_eq!(b[0], 2);
    }

    #[test]
    fn at_most_63_ids() {
        let ids: Vec<u32> = (0..70).map(|i| 0x4200 + i).collect();
        let (b, out) = round(&ids, 1);
        assert_eq!(out.len(), 63);
        assert_eq!(b[1], 63 + 0x40);
        assert_eq!(b.len(), 1 + 2 + 63);
        // many groups: the 63 come from the lowest groups first
        let ids: Vec<u32> = (0..70).rev().map(|i| (i << 8) | (i * 3)).collect();
        let (b, out) = round(&ids, 0);
        assert_eq!(out.len(), 63);
        assert_eq!(out[0], 0); // group 0's id 0
        assert_eq!(out[62], (62 << 8) | (62 * 3));
        assert!(b.len() <= MAX_BUFFER);
    }

    #[test]
    fn repeated_ids_stay() {
        let (_, out) = round(&[7, 7, 0x107, 7], 0);
        assert_eq!(out, vec![7, 7, 7, 0x107]);
    }

    #[test]
    fn a_malformed_buffer_stops_at_its_end() {
        let mut buf = [0u8; 256];
        buf[0] = 1;
        buf[1] = 0x3f; // 63 ids, but the size says 3
        let mut out = Vec::new();
        decompress(&buf, 3, &mut out);
        assert_eq!(out.len(), 63);
        // a run head with every byte 0x3f to the end: no panic
        let buf = [0x3fu8; 256];
        decompress(&buf, 0, &mut out);
        assert!(!out.is_empty());
    }
}
