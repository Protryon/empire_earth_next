//! `UEncryption`: strings as 8-digit hex blocks. Each 32-bit value is rotated left and xor'ed with one of four keys;
//! every record but the authorization code ends in a byte-sum checksum block.
//!
//! The UString behaviours the C++ relied on are reproduced where a caller can see them: a padding `+=` never grows the
//! string (a full buffer drops it), reading past the length yields 0, characters are signed, `UEDecryptUString`'s
//! output is capped by a 256-byte `UString`, and a value with a zero byte ends its block's text at that byte.
//! The checks that used uninitialised memory in C++ (a block with no hex digits) read as 0, and scanning past the end
//! of the input stops there, so garbage input just fails its checksum.

struct Key {
    shift: u32,
    key: u32,
}

const KEYS: [Key; 4] = [
    Key { shift: 3, key: 0xb7b2_26bd },
    Key { shift: 5, key: 0x3501_3685 },
    Key { shift: 11, key: 0x0074_45a5 },
    Key { shift: 21, key: 0x5c46_fcc5 },
];

/// `UString`'s default capacity, which caps the working strings of the decrypters.
const DEFAULT_CAPACITY: usize = 256;

fn byte_sum(v: u32) -> u32 {
    (v & 0xff) + ((v >> 8) & 0xff) + ((v >> 16) & 0xff) + (v >> 24)
}

fn key_of(index: u32) -> &'static Key {
    &KEYS[(index & 3) as usize]
}

fn encrypt_block(value: u32, key: u32) -> String {
    let k = key_of(key);
    format!("{:08X}", value.rotate_left(k.shift) ^ k.key)
}

fn decrypt_value(value: u32, key: u32) -> u32 {
    let k = key_of(key);
    (value ^ k.key).rotate_right(k.shift)
}

/// `sscanf(block, "%X", &value)`: whitespace, a sign, an optional 0x, hex digits up to the first other character.
/// A block with no digits reads as 0 (C++ left the value uninitialised).
pub fn scan_hex(block: &[u8]) -> u32 {
    let is_space = |c: u8| matches!(c, b' ' | b'\t' | b'\n' | 0x0b | 0x0c | b'\r');
    let mut i = 0;
    while i < block.len() && is_space(block[i]) {
        i += 1;
    }
    let mut negative = false;
    if i < block.len() && (block[i] == b'-' || block[i] == b'+') {
        negative = block[i] == b'-';
        i += 1;
    }
    if i + 2 < block.len() && block[i] == b'0' && (block[i + 1] | 0x20) == b'x' && block[i + 2].is_ascii_hexdigit() {
        i += 2;
    }
    let mut value = 0u32;
    while i < block.len() && block[i].is_ascii_hexdigit() {
        value = value.wrapping_mul(16).wrapping_add((block[i] as char).to_digit(16).expect("a hex digit"));
        i += 1;
    }
    if negative {
        value.wrapping_neg()
    } else {
        value
    }
}

fn substring(input: &[u8], start: usize, count: usize) -> &[u8] {
    if start >= input.len() {
        return &[];
    }
    &input[start..(start + count).min(input.len())]
}

fn decrypt_block(block: &[u8], key: u32) -> u32 {
    decrypt_value(scan_hex(block), key)
}

fn char_at(s: &[u8], i: usize) -> i32 {
    s.get(i).map_or(0, |&b| i32::from(b as i8))
}

/// A 4-character block's big-endian value, with signed characters (as written, high-bit characters smear).
fn pack_chars(s: &[u8], i: usize) -> u32 {
    let shift_or = |high: i32, low: i32| high.wrapping_shl(8) | low;
    shift_or(shift_or(shift_or(char_at(s, i), char_at(s, i + 1)), char_at(s, i + 2)), char_at(s, i + 3)) as u32
}

fn sum_chars(s: &[u8]) -> u32 {
    s.iter().fold(0u32, |sum, &b| sum.wrapping_add(i32::from(b as i8) as u32))
}

/// `UString::operator+=`: truncates to the buffer's free space, never grows.
fn append_capped(target: &mut Vec<u8>, capacity: usize, text: &[u8]) {
    let free = capacity.saturating_sub(target.len() + 1);
    if free != 0 {
        let count = text.len().min(free);
        target.extend_from_slice(&text[..count]);
    }
}

/// The block text `sprintf("%c%c%c%c")` left: the bytes before the first zero.
fn block_text(value: u32) -> Vec<u8> {
    let bytes = value.to_be_bytes();
    let end = bytes.iter().position(|&b| b == 0).unwrap_or(4);
    bytes[..end].to_vec()
}

/// Encrypts `input` (`capacity` is its `UString` capacity, which limits its padding).
pub fn encrypt_ustring(input: &[u8], capacity: usize) -> Vec<u8> {
    let mut text = input.to_vec();
    let mut out = encrypt_block(input.len() as u32, 0);
    match input.len() & 3 {
        1 => text.extend_from_slice(b"   "), // through a wide string: never truncated
        2 => append_capped(&mut text, capacity, b"  "),
        3 => append_capped(&mut text, capacity, b" "),
        _ => {}
    }
    out.push_str(&encrypt_block(text.len() as u32, 0));
    let mut i = 0;
    while i < text.len() {
        out.push_str(&encrypt_block(pack_chars(&text, i), 0));
        i += 4;
    }
    let sum = byte_sum(text.len() as u32).wrapping_add(byte_sum(input.len() as u32)).wrapping_add(sum_chars(&text));
    out.push_str(&encrypt_block(sum, 0));
    out.into_bytes()
}

/// Decrypts a string written by [`encrypt_ustring`]; returns the text and whether the checksum matches.
pub fn decrypt_ustring(input: &[u8]) -> (Vec<u8>, bool) {
    let length = decrypt_block(substring(input, 0, 8), 0);
    let padded = decrypt_block(substring(input, 8, 8), 0);
    let mut text = Vec::new();
    let end = (u64::from(padded) * 2 + 16).min(input.len() as u64) as usize;
    let mut i = 16;
    while i < end {
        append_capped(&mut text, DEFAULT_CAPACITY, &block_text(decrypt_block(substring(input, i, 8), 0)));
        i += 8;
    }
    let sum = byte_sum(padded).wrapping_add(byte_sum(length)).wrapping_add(sum_chars(&text));
    let checksum_at = (u64::from(padded) * 2 + 16).min(input.len() as u64) as usize;
    let checksum = decrypt_block(substring(input, checksum_at, 8), 0);
    text.truncate(length as usize);
    (text, checksum == sum)
}

/// Encrypts a 16-digit hex authorization code as two key-2 blocks.
pub fn encrypt_authorization_code(code: &[u8]) -> Vec<u8> {
    let mut out = encrypt_block(scan_hex(substring(code, 0, 8)), 2);
    out.push_str(&encrypt_block(scan_hex(substring(code, 8, 8)), 2));
    out.into_bytes()
}

/// Decrypts a 16-digit authorization code into its two values.
pub fn decrypt_authorization_code(code: &[u8]) -> (u32, u32) {
    (decrypt_block(substring(code, 0, 8), 2), decrypt_block(substring(code, 8, 8), 2))
}

/// Encrypts two expiration values plus a byte-sum checksum as three key-3 blocks.
pub fn encrypt_expiration(first: u32, second: u32) -> Vec<u8> {
    let mut out = encrypt_block(first, 3);
    out.push_str(&encrypt_block(second, 3));
    out.push_str(&encrypt_block(byte_sum(first).wrapping_add(byte_sum(second)), 3));
    out.into_bytes()
}

/// Decrypts two expiration values; the last field is whether the checksum block matches.
pub fn decrypt_expiration(input: &[u8]) -> (u32, u32, bool) {
    let first = decrypt_block(substring(input, 0, 8), 3);
    let second = decrypt_block(substring(input, 8, 8), 3);
    let sum = byte_sum(first).wrapping_add(byte_sum(second));
    let checksum = decrypt_block(substring(input, 16, 8), 3);
    (first, second, checksum == sum)
}

/// Encrypts a value and a GUID string (padded with spaces to a multiple of 4, as far as `capacity` allows). Returns the
/// padded GUID, which C++ wrote back into the caller's string, and the encrypted text.
pub fn encrypt_guid(value: u32, guid: &[u8], capacity: usize, key: u32) -> (Vec<u8>, Vec<u8>) {
    let mut text = guid.to_vec();
    match text.len() & 3 {
        1 => append_capped(&mut text, capacity, b"   "),
        2 => append_capped(&mut text, capacity, b"  "),
        3 => append_capped(&mut text, capacity, b" "),
        _ => {}
    }
    let mut out = encrypt_block(value, key);
    out.push_str(&encrypt_block(text.len() as u32, key));
    let mut i = 0;
    while i < text.len() {
        out.push_str(&encrypt_block(pack_chars(&text, i), key));
        i += 4;
    }
    let sum = byte_sum(value).wrapping_add(byte_sum(text.len() as u32)).wrapping_add(sum_chars(&text));
    out.push_str(&encrypt_block(sum, key));
    (text, out.into_bytes())
}

/// Decrypts a value and a GUID string, appended to `prefix` (capped at `capacity`, like the C++ `+=`); the checksum
/// covers the prefix too. Returns the value, the GUID with its prefix, and whether the checksum matches.
pub fn decrypt_guid(input: &[u8], key: u32, prefix: &[u8], capacity: usize) -> (u32, Vec<u8>, bool) {
    let value = decrypt_block(substring(input, 0, 8), key);
    let length = decrypt_block(substring(input, 8, 8), key);
    let mut guid = prefix.to_vec();
    let end = (u64::from(length) * 2 + 16).min(input.len() as u64) as usize;
    let mut i = 16;
    while i < end {
        append_capped(&mut guid, capacity, &block_text(decrypt_block(substring(input, i, 8), key)));
        i += 8;
    }
    let sum = byte_sum(value).wrapping_add(byte_sum(length)).wrapping_add(sum_chars(&guid));
    let checksum_at = (u64::from(length) * 2 + 16).min(input.len() as u64) as usize;
    let checksum = decrypt_block(substring(input, checksum_at, 8), key);
    (value, guid, checksum == sum)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn block_round_trips_each_key() {
        for key in 0..4 {
            for value in [0u32, 1, 0xffff_ffff, 0x1234_5678] {
                assert_eq!(decrypt_block(encrypt_block(value, key).as_bytes(), key), value);
            }
        }
    }

    #[test]
    fn scan_hex_is_sscanf_percent_x() {
        assert_eq!(scan_hex(b"1A"), 0x1a);
        assert_eq!(scan_hex(b"  ff"), 0xff);
        assert_eq!(scan_hex(b"-1"), 0xffff_ffff);
        assert_eq!(scan_hex(b"+7"), 7);
        assert_eq!(scan_hex(b"0x10"), 0x10);
        assert_eq!(scan_hex(b"0xZZ"), 0);
        assert_eq!(scan_hex(b"12GH"), 0x12);
        assert_eq!(scan_hex(b""), 0);
        assert_eq!(scan_hex(b"zz"), 0);
    }

    #[test]
    fn string_round_trip_pads_to_four() {
        let (text, ok) = decrypt_ustring(&encrypt_ustring(b"abc", 256));
        assert_eq!(text, b"abc");
        assert!(ok);
    }

    #[test]
    fn exact_fit_string_stays_unpadded_and_fails_its_checksum() {
        // "ab" in a 3-byte buffer has no room for padding: the single block (with zero bytes, dropped by decryption)
        // gives back the text, but the checksum is looked for at padded length * 2 + 16 = 20, inside the block
        let enc = encrypt_ustring(b"ab", 3);
        assert_eq!(enc.len(), 32);
        let (text, ok) = decrypt_ustring(&enc);
        assert_eq!(text, b"ab");
        assert!(!ok);
    }

    #[test]
    fn decrypted_text_stops_at_255_characters() {
        let long = vec![b'q'; 300];
        let (text, ok) = decrypt_ustring(&encrypt_ustring(&long, 301));
        assert_eq!(text.len(), 255);
        assert!(!ok, "the checksum saw the capped text");
    }

    #[test]
    fn garbage_input_fails_quietly() {
        for garbage in [&b""[..], b"FF", b"not hex at all", b"zzzzzzzzzzzzzzzzzzzzzzzz"] {
            let (_, ok) = decrypt_ustring(garbage);
            assert!(!ok || garbage.len() >= 16);
        }
        // a length block claiming more than the input does not read past it
        let mut lying = encrypt_block(0xffff_fff0, 0);
        lying.push_str(&encrypt_block(0xffff_fff0, 0));
        let (text, ok) = decrypt_ustring(lying.as_bytes());
        assert!(text.is_empty());
        assert!(!ok);
    }

    #[test]
    fn expiration_detects_a_changed_checksum() {
        let mut enc = encrypt_expiration(1234, 5678);
        let (a, b, ok) = decrypt_expiration(&enc);
        assert_eq!((a, b, ok), (1234, 5678, true));
        enc[23] = if enc[23] == b'0' { b'1' } else { b'0' };
        assert!(!decrypt_expiration(&enc).2);
    }

    #[test]
    fn guid_appends_to_the_prefix_and_checks_it() {
        let (padded, enc) = encrypt_guid(99, b"GUID1", 64, 1);
        assert_eq!(padded, b"GUID1   ");
        let (value, guid, ok) = decrypt_guid(&enc, 1, b"pre", 256);
        assert_eq!(value, 99);
        assert_eq!(guid, b"preGUID1   ");
        assert!(!ok); // the prefix is in the checksum
        assert!(decrypt_guid(&enc, 1, b"", 256).2);
    }

    #[test]
    fn key_index_is_masked() {
        assert_eq!(encrypt_block(5, 7), encrypt_block(5, 3));
    }
}
