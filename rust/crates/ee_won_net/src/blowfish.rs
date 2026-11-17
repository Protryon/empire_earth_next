//! WONCrypt's Blowfish (`Blowfish.cpp`): the standard cipher (keys of 5..=56 bytes, big-endian blocks) in WON's
//! framing: CBC from an all-0xff mask, a 4-byte little-endian length in front of the data, zero padding to whole
//! blocks.

use crate::blowfish_tables::{P_INIT, S_INIT};
use crate::random::Random;

/// Shortest key.
pub(crate) const MIN_KEY_LEN: usize = 5;
/// Longest key.
pub(crate) const MAX_KEY_LEN: usize = 56;

/// A keyed cipher. `#[repr(C)]` plain data: the C++ `Blowfish` embeds it by value and copies it with the class.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Blowfish {
    s: [u32; 1024],
    p: [u32; 18],
    key: [u8; 56],
    key_len: u32,
    valid: u32,
}

/// The size of `Blowfish`, which the C++ class reserves.
pub(crate) const BLOWFISH_STATE_SIZE: usize = std::mem::size_of::<Blowfish>();
const _: () = assert!(BLOWFISH_STATE_SIZE == 4232);

impl Default for Blowfish {
    fn default() -> Self {
        Blowfish::new()
    }
}

impl Blowfish {
    /// `Blowfish()`: no key.
    pub const fn new() -> Blowfish {
        Blowfish { s: [0; 1024], p: [0; 18], key: [0; MAX_KEY_LEN], key_len: 0, valid: 0 }
    }

    /// Whether a key is set.
    pub fn is_valid(&self) -> bool {
        self.valid != 0
    }

    /// The raw key of the last successful `set_key`.
    pub fn key(&self) -> &[u8] {
        &self.key[..self.key_len as usize]
    }

    fn f(&self, x: u32) -> u32 {
        let s = &self.s;
        ((s[(x >> 24) as usize].wrapping_add(s[256 + ((x >> 16) & 0xff) as usize]))
            ^ s[512 + ((x >> 8) & 0xff) as usize])
            .wrapping_add(s[768 + (x & 0xff) as usize])
    }

    fn encrypt_pair(&self, l: u32, r: u32) -> (u32, u32) {
        let mut l = l ^ self.p[0];
        let mut r = r;
        let mut i = 0;
        while i < 16 {
            i += 1;
            r ^= self.f(l) ^ self.p[i];
            i += 1;
            l ^= self.f(r) ^ self.p[i];
        }
        (r ^ self.p[17], l)
    }

    fn decrypt_pair(&self, l: u32, r: u32) -> (u32, u32) {
        let mut l = l ^ self.p[17];
        let mut r = r;
        let mut i = 17;
        while i > 1 {
            i -= 1;
            r ^= self.f(l) ^ self.p[i];
            i -= 1;
            l ^= self.f(r) ^ self.p[i];
        }
        (r ^ self.p[0], l)
    }

    fn encrypt_block(&self, block: &[u8; 8]) -> [u8; 8] {
        let (l, r) = self.encrypt_pair(be(&block[..4]), be(&block[4..]));
        join(l, r)
    }

    fn decrypt_block(&self, block: &[u8]) -> [u8; 8] {
        let (l, r) = self.decrypt_pair(be(&block[..4]), be(&block[4..8]));
        join(l, r)
    }

    /// `SetKey`: the key schedule; false (and no key) when the length is outside 5..=56.
    pub fn set_key(&mut self, key: &[u8]) -> bool {
        if key.len() < MIN_KEY_LEN || key.len() > MAX_KEY_LEN {
            self.valid = 0;
            return false;
        }
        self.p = P_INIT;
        self.s = S_INIT;
        let mut j = 0;
        for i in 0..18 {
            let mut l = 0u32;
            for _ in 0..4 {
                j %= key.len();
                l = (l << 8) | u32::from(key[j]);
                j += 1;
            }
            self.p[i] ^= l;
        }
        let (mut l, mut r) = (0, 0);
        for i in (0..18).step_by(2) {
            (l, r) = self.encrypt_pair(l, r);
            self.p[i] = l;
            self.p[i + 1] = r;
        }
        for i in (0..1024).step_by(2) {
            (l, r) = self.encrypt_pair(l, r);
            self.s[i] = l;
            self.s[i + 1] = r;
        }
        self.valid = 1;
        self.key = [0; MAX_KEY_LEN];
        self.key[..key.len()].copy_from_slice(key);
        self.key_len = key.len() as u32;
        true
    }

    /// `Create`: a random key of the given length.
    pub fn create(&mut self, length: usize) -> bool {
        if !(MIN_KEY_LEN..=MAX_KEY_LEN).contains(&length) {
            return false;
        }
        self.create_with(length, Random::new())
    }

    /// `Create` with a given generator (tests and golden comparisons).
    pub fn create_with(&mut self, length: usize, mut random: Random) -> bool {
        if !(MIN_KEY_LEN..=MAX_KEY_LEN).contains(&length) {
            return false;
        }
        let mut key = vec![0u8; length];
        random.next_bytes(&mut key);
        self.set_key(&key)
    }

    /// `Encrypt`: None without a key. For 4 input bytes the C++ left the second block uninitialised; it is zero
    /// here.
    pub fn encrypt(&self, input: &[u8]) -> Option<Vec<u8>> {
        if !self.is_valid() {
            return None;
        }
        let len = input.len() as i64;
        let padding = 8 - (len + 4) % 8;
        let out_len = (4 + len + padding) as usize;
        let mut out = vec![0u8; out_len];
        let at = |i: i64| if i < len { input[i as usize] } else { 0 };
        let mut mask = [0xffu8; 8];
        for (j, b) in (len as i32).to_le_bytes().iter().enumerate() {
            mask[j] ^= b;
        }
        for j in 0..4 {
            mask[4 + j] ^= at(j as i64);
        }
        mask = self.encrypt_block(&mask);
        out[..8].copy_from_slice(&mask);
        let mut out_off = 8usize;
        let mut in_off = 4i64;
        while len - in_off >= 8 {
            for j in 0..8 {
                mask[j] ^= input[in_off as usize + j];
            }
            mask = self.encrypt_block(&mask);
            out[out_off..out_off + 8].copy_from_slice(&mask);
            in_off += 8;
            out_off += 8;
        }
        if len > 4 {
            for (j, m) in mask.iter_mut().enumerate() {
                *m ^= at(j as i64 + in_off);
            }
            mask = self.encrypt_block(&mask);
            out[out_off..out_off + 8].copy_from_slice(&mask);
        }
        Some(out)
    }

    /// `Decrypt`: None without a key, for a length that is not whole blocks, or when the length field does not fit
    /// the input.
    pub fn decrypt(&self, input: &[u8]) -> Option<Vec<u8>> {
        let len = input.len();
        if !self.is_valid() || !len.is_multiple_of(8) || len == 0 {
            return None;
        }
        let mut mask = [0xffu8; 8];
        let mut buf = self.decrypt_block(&input[..8]);
        for (b, m) in buf.iter_mut().zip(mask.iter()) {
            *b ^= m;
        }
        mask.copy_from_slice(&input[..8]);
        let out_len = i32::from_le_bytes([buf[0], buf[1], buf[2], buf[3]]);
        if out_len < 0 {
            return None;
        }
        let (out_len, ilen) = (out_len as i64, len as i64);
        if out_len > ilen - 4 || (ilen > 12 && out_len < ilen - 12) {
            return None;
        }
        let out_len = out_len as usize;
        let mut out = vec![0u8; out_len];
        let first = out_len.min(4);
        out[..first].copy_from_slice(&buf[4..4 + first]);
        let mut in_off = 8;
        let mut out_off = 4;
        while out_len >= out_off + 8 {
            let block = self.decrypt_block(&input[in_off..in_off + 8]);
            for j in 0..8 {
                out[out_off + j] = block[j] ^ mask[j];
                mask[j] = input[in_off + j];
            }
            in_off += 8;
            out_off += 8;
        }
        if out_off < out_len && in_off < len {
            let mut block = self.decrypt_block(&input[in_off..in_off + 8]);
            for j in 0..8 {
                block[j] ^= mask[j];
            }
            out[out_off..].copy_from_slice(&block[..out_len - out_off]);
        }
        Some(out)
    }
}

fn be(b: &[u8]) -> u32 {
    u32::from_be_bytes([b[0], b[1], b[2], b[3]])
}

fn join(l: u32, r: u32) -> [u8; 8] {
    let mut out = [0u8; 8];
    out[..4].copy_from_slice(&l.to_be_bytes());
    out[4..].copy_from_slice(&r.to_be_bytes());
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn schneier_test_vector() {
        // Eric Young's set: key 0123456789abcdef... first 8 bytes, the ECB vector for key fedcba9876543210
        let mut bf = Blowfish::new();
        assert!(bf.set_key(&[0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10]));
        let ct = bf.encrypt_block(&[0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef]);
        assert_eq!(ct, [0x0a, 0xce, 0xab, 0x0f, 0xc6, 0xa0, 0xa2, 0x8d]);
        assert_eq!(bf.decrypt_block(&ct), [0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef]);
    }

    #[test]
    fn key_lengths() {
        let mut bf = Blowfish::new();
        assert!(!bf.set_key(&[1, 2, 3, 4]));
        assert!(!bf.is_valid());
        assert!(!bf.set_key(&[0; 57]));
        assert!(bf.set_key(&[9; 56]));
        assert_eq!(bf.key(), &[9; 56]);
        assert!(bf.encrypt(b"x").is_some());
        assert!(Blowfish::new().encrypt(b"x").is_none());
        assert!(Blowfish::new().decrypt(&[0; 8]).is_none());
        let mut c = Blowfish::new();
        assert!(c.create(16) && c.key().len() == 16);
        assert!(!c.create(4));
    }

    #[test]
    fn framing_round_trips_and_refuses_bad_lengths() {
        let mut bf = Blowfish::new();
        bf.set_key(b"secret key");
        for n in 0..40 {
            let data: Vec<u8> = (0..n as u8).collect();
            let enc = bf.encrypt(&data).unwrap();
            assert_eq!(enc.len() % 8, 0);
            assert_eq!(enc.len(), 4 + n + 8 - (n + 4) % 8);
            assert_eq!(bf.decrypt(&enc).unwrap(), data, "n {n}");
        }
        assert!(bf.decrypt(&[]).is_none());
        assert!(bf.decrypt(&[0; 7]).is_none());
        let enc = bf.encrypt(&[7; 30]).unwrap();
        // a longer input than the length field claims, by more than the padding: refused
        let mut longer = enc.clone();
        longer.extend_from_slice(&[0; 8]);
        assert!(bf.decrypt(&longer).is_none());
    }
}
