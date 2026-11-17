//! WON's CD keys (`WONAuth/CDKey.cpp`): 8 raw bytes, written as 20 letters and digits in groups of four.
//!
//! The text form packs 64 bits little-endian-first: the pattern "CVCN" four times, then "NNNN"; C is one of 16
//! consonants (4 bits), V one of the vowels A E U Y (2 bits), N a digit 2..9 (3 bits). Byte 3 is a check byte: bits
//! 4..11 of the CRC-16 of the product name and the other seven bytes. Stored keys are Blowfish-encrypted with a key
//! made from CRCs of the product name, optionally behind the machine id.

use crate::blowfish::Blowfish;
use crate::crc16::Crc16;

const KEY_MAP: &[u8; 20] = b"CVCNCVCNCVCNCVCNNNNN";
const CONSONANTS: &[u8; 16] = b"BCDFGJLMNPRSTWXZ";
const VOWELS: &[u8; 4] = b"AEUY";
const BETA_MASK: u8 = 0x01;
const FORCE_MASK: u8 = 0x80;

/// The check byte of a raw key for a product.
pub fn check_byte(product: &[u8], raw: &[u8; 8]) -> u8 {
    let mut crc = Crc16::new(0, 0);
    crc.put(product);
    crc.put(&raw[..3]);
    crc.put(&raw[4..]);
    ((crc.get() & 0x0fff) >> 4) as u8
}

/// `CalcKeyFromBuffer`'s test: the check byte matches.
pub fn is_valid(product: &[u8], raw: &[u8; 8]) -> bool {
    check_byte(product, raw) == raw[3]
}

fn add_bits(buf: &mut [u8; 8], offset: &mut usize, bits: u32, count: usize) {
    let (byte, bit) = (*offset / 8, *offset % 8);
    buf[byte] |= ((bits << bit) & 0xff) as u8;
    let extra = bit as i32 + count as i32 - 8;
    if extra > 0 {
        buf[byte + 1] |= ((bits >> (count as i32 - extra)) & 0xff) as u8;
    }
    *offset += count;
}

fn bits_at(buf: &[u8; 8], offset: &mut usize, count: usize) -> u32 {
    let (byte, bit) = (*offset / 8, *offset % 8);
    let mut v = (u32::from(buf[byte]) >> bit) & ((1 << count) - 1) & 0xff;
    let extra = count as i32 + bit as i32 - 8;
    if extra > 0 {
        v |= (u32::from(buf[byte + 1]) & ((1 << extra) - 1)) << (count as i32 - extra);
    }
    *offset += count;
    v
}

/// `Init(string)`: the raw bytes of a key's text (dashes, spaces, tabs and NULs skipped, letters in either case); None
/// when the text is not a key's shape. The check byte is not tested here (`is_valid`).
pub fn parse(text: &[u8]) -> Option<[u8; 8]> {
    // (the C++'s strchr also found the terminator: a NUL byte is skipped too)
    let key: Vec<u8> = text.iter().copied().filter(|c| !b"- \t\0".contains(c)).collect();
    if key.len() != KEY_MAP.len() {
        return None;
    }
    let mut buf = [0u8; 8];
    let mut offset = 0;
    for (&kind, &c) in KEY_MAP.iter().zip(&key) {
        let upper = c.to_ascii_uppercase();
        match kind {
            b'C' => add_bits(&mut buf, &mut offset, CONSONANTS.iter().position(|&x| x == upper)? as u32, 4),
            b'V' => add_bits(&mut buf, &mut offset, VOWELS.iter().position(|&x| x == upper)? as u32, 2),
            _ => {
                if !(b'2'..=b'9').contains(&c) {
                    return None;
                }
                add_bits(&mut buf, &mut offset, u32::from(c - b'2'), 3)
            }
        }
    }
    Some(buf)
}

/// `GetString`: the text of raw bytes, "XXXX-XXXX-XXXX-XXXX-XXXX".
pub fn format(raw: &[u8; 8]) -> String {
    let mut out = String::with_capacity(24);
    let mut offset = 0;
    for (i, &kind) in KEY_MAP.iter().enumerate() {
        let c = match kind {
            b'C' => CONSONANTS[bits_at(raw, &mut offset, 4) as usize],
            b'V' => VOWELS[bits_at(raw, &mut offset, 2) as usize],
            _ => b'2' + bits_at(raw, &mut offset, 3) as u8,
        };
        out.push(char::from(c));
        if (i + 1) % 4 == 0 && i + 1 < KEY_MAP.len() {
            out.push('-');
        }
    }
    out
}

/// `GetSymmetricKey`: the machine id (when there is one and it is asked for), the CRC of the product name, then
/// the running CRC after each of its first three letters, all little-endian.
pub fn symmetric_key(product: &[u8], machine_id: Option<u32>) -> Vec<u8> {
    let mut bytes = Vec::new();
    if let Some(id) = machine_id {
        bytes.extend_from_slice(&id.to_le_bytes());
    }
    let mut crc = Crc16::new(0, 0);
    crc.put(product);
    bytes.extend_from_slice(&crc.get().to_le_bytes());
    for &letter in product.iter().take(3) {
        crc.put(&[letter]);
        bytes.extend_from_slice(&crc.get().to_le_bytes());
    }
    bytes
}

fn cipher(product: &[u8], machine_id: Option<u32>) -> Blowfish {
    let mut bf = Blowfish::new();
    bf.set_key(&symmetric_key(product, machine_id));
    bf
}

/// `GetEncrypted`: the raw key encrypted for storage.
pub fn encrypt(product: &[u8], machine_id: Option<u32>, raw: &[u8; 8]) -> Option<Vec<u8>> {
    cipher(product, machine_id).encrypt(raw)
}

/// What `InitEncrypted` decoded.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Decoded {
    /// the raw key the C++ kept (the last one decrypted), if any
    pub raw: Option<[u8; 8]>,
    pub valid: bool,
    /// whether the method tried last used the machine id (the C++ notified "old method" when it did not)
    pub used_machine: bool,
}

/// `InitEncrypted`: decrypts with the preferred method (with or without the machine id), and with the other one
/// when that gives no valid key. A decrypted key shorter than 8 bytes is zero-filled (the C++ read past it).
pub fn decrypt(product: &[u8], machine_id: Option<u32>, prefer_machine: bool, data: &[u8]) -> Decoded {
    let mut result = Decoded { raw: None, valid: false, used_machine: prefer_machine };
    let try_method = |use_machine: bool, result: &mut Decoded| {
        result.used_machine = use_machine;
        if let Some(plain) = cipher(product, if use_machine { machine_id } else { None }).decrypt(data) {
            let mut raw = [0u8; 8];
            let n = plain.len().min(8);
            raw[..n].copy_from_slice(&plain[..n]);
            result.raw = Some(raw);
            result.valid = is_valid(product, &raw);
        }
    };
    try_method(prefer_machine, &mut result);
    if !result.valid {
        try_method(!prefer_machine, &mut result);
    }
    result
}

/// `IsBeta`.
pub fn is_beta(raw: &[u8]) -> bool {
    raw.first().is_some_and(|&b| b & BETA_MASK != 0)
}

/// `IsForceCheck`.
pub fn is_force_check(raw: &[u8]) -> bool {
    raw.len() >= 3 && raw[2] & FORCE_MASK != 0
}

#[cfg(test)]
mod tests {
    use super::*;

    fn valid_key(product: &[u8], seed: u8) -> [u8; 8] {
        let mut raw = [seed, seed.wrapping_mul(3), 0x81, 0, 7, seed ^ 0x5a, 0xff, 0x10];
        raw[3] = check_byte(product, &raw);
        raw
    }

    #[test]
    fn text_round_trips() {
        for seed in 0..50u8 {
            let raw = valid_key(b"EmpireEarth", seed);
            let text = format(&raw);
            assert_eq!(text.len(), 24);
            assert_eq!(parse(text.as_bytes()), Some(raw));
            assert_eq!(parse(text.to_ascii_lowercase().replace('-', " ").as_bytes()), Some(raw));
            assert!(is_valid(b"EmpireEarth", &raw));
        }
    }

    #[test]
    fn malformed_text() {
        assert_eq!(parse(b"BABB-2BAB-2BAB-2BAB-222"), None);
        assert_eq!(parse(b"BOBB-2BAB-2BAB-2BAB-2222"), None);
        assert_eq!(parse(b"BABB-1BAB-2BAB-2BAB-2222"), None);
        assert_eq!(parse(b"BAB2-BAB2-BAB2-BAB2-2222"), Some([0; 8]));
        assert_eq!(parse(b"BAB2-2BAB-2BAB-2BAB-2222"), None);
        assert_eq!(format(&[0; 8]), "BAB2-BAB2-BAB2-BAB2-2222");
    }

    #[test]
    fn flags() {
        assert!(is_beta(&[1]) && !is_beta(&[2]) && !is_beta(&[]));
        assert!(is_force_check(&[0, 0, 0x80]) && !is_force_check(&[0, 0x80]));
    }

    #[test]
    fn storage_round_trip_and_fallback() {
        let raw = valid_key(b"AoC", 9);
        let with_machine = encrypt(b"AoC", Some(0x1234), &raw).unwrap();
        let d = decrypt(b"AoC", Some(0x1234), true, &with_machine);
        assert_eq!(d, Decoded { raw: Some(raw), valid: true, used_machine: true });
        // stored without the machine id: the second try finds it
        let old = encrypt(b"AoC", None, &raw).unwrap();
        let d = decrypt(b"AoC", Some(0x1234), true, &old);
        assert_eq!((d.raw, d.valid, d.used_machine), (Some(raw), true, false));
        // garbage: nothing valid, the last method tried was the other one
        let d = decrypt(b"AoC", Some(0x1234), true, &[0; 5]);
        assert_eq!(d, Decoded { raw: None, valid: false, used_machine: false });
        assert_eq!(symmetric_key(b"AoC", None).len(), 8);
        assert_eq!(symmetric_key(b"", Some(1)), vec![1, 0, 0, 0, 0, 0]);
    }
}
