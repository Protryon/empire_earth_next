//! MD5 (RFC 1321), the digest behind both of WON's MD5 classes: `WONCommon/MD5.cpp` (`MD5`: Update, Digest) and
//! `WONCrypt/MD5Digest.cpp` (`MD5Digest`: update, digest, used by the signatures and the random generator). Both
//! compute plain MD5; a finished digest resets the state.

/// A running MD5 digest. `#[repr(C)]` and plain data: the C++ forwarders embed it by value.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct Md5 {
    state: [u32; 4],
    /// bytes hashed so far
    count: u64,
    pending: [u8; 64],
}

const S: [u32; 64] = [
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15,
    21,
];

const K: [u32; 64] = [
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501, 0x698098d8,
    0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340,
    0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8, 0x21e1cde6, 0xc33707d6, 0xf4d50d87,
    0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
    0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039,
    0xe6db99e5, 0x1fa27cf8, 0xc4ac5665, 0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92,
    0xffeff47d, 0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb,
    0xeb86d391,
];

const INIT: [u32; 4] = [0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476];

impl Default for Md5 {
    fn default() -> Self {
        Md5::new()
    }
}

impl Md5 {
    /// A digest in its initial state.
    pub const fn new() -> Md5 {
        Md5 { state: INIT, count: 0, pending: [0; 64] }
    }

    /// Back to the initial state.
    pub fn reset(&mut self) {
        *self = Md5::new();
    }

    fn transform(&mut self, block: &[u8]) {
        let mut m = [0u32; 16];
        for (i, w) in m.iter_mut().enumerate() {
            *w = u32::from_le_bytes([block[4 * i], block[4 * i + 1], block[4 * i + 2], block[4 * i + 3]]);
        }
        let [mut a, mut b, mut c, mut d] = self.state;
        for i in 0..64 {
            let (f, g) = match i / 16 {
                0 => (d ^ (b & (c ^ d)), i),
                1 => (c ^ (d & (b ^ c)), (5 * i + 1) % 16),
                2 => (b ^ c ^ d, (3 * i + 5) % 16),
                _ => (c ^ (b | !d), (7 * i) % 16),
            };
            let t = a.wrapping_add(f).wrapping_add(K[i]).wrapping_add(m[g]).rotate_left(S[i]);
            a = d;
            d = c;
            c = b;
            b = b.wrapping_add(t);
        }
        self.state[0] = self.state[0].wrapping_add(a);
        self.state[1] = self.state[1].wrapping_add(b);
        self.state[2] = self.state[2].wrapping_add(c);
        self.state[3] = self.state[3].wrapping_add(d);
    }

    /// Adds bytes.
    pub fn update(&mut self, mut data: &[u8]) {
        let have = (self.count % 64) as usize;
        self.count = self.count.wrapping_add(data.len() as u64);
        if have != 0 {
            let take = (64 - have).min(data.len());
            self.pending[have..have + take].copy_from_slice(&data[..take]);
            data = &data[take..];
            if have + take < 64 {
                return;
            }
            let block = self.pending;
            self.transform(&block);
        }
        while data.len() >= 64 {
            self.transform(&data[..64]);
            data = &data[64..];
        }
        self.pending[..data.len()].copy_from_slice(data);
    }

    /// Pads, returns the 16-byte digest and resets.
    pub fn finish(&mut self) -> [u8; 16] {
        let bits = self.count.wrapping_mul(8);
        let have = (self.count % 64) as usize;
        let mut tail = [0u8; 128];
        tail[..have].copy_from_slice(&self.pending[..have]);
        tail[have] = 0x80;
        let total = if have < 56 { 64 } else { 128 };
        tail[total - 8..total].copy_from_slice(&bits.to_le_bytes());
        for block in tail[..total].chunks(64) {
            self.transform(block);
        }
        let mut out = [0u8; 16];
        for (i, w) in self.state.iter().enumerate() {
            out[4 * i..4 * i + 4].copy_from_slice(&w.to_le_bytes());
        }
        self.reset();
        out
    }

    /// The digest of one buffer.
    pub fn digest(data: &[u8]) -> [u8; 16] {
        let mut m = Md5::new();
        m.update(data);
        m.finish()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn hex(d: &[u8]) -> String {
        d.iter().map(|b| format!("{b:02x}")).collect()
    }

    #[test]
    fn rfc1321_vectors() {
        let cases: [(&[u8], &str); 7] = [
            (b"", "d41d8cd98f00b204e9800998ecf8427e"),
            (b"a", "0cc175b9c0f1b6a831c399e269772661"),
            (b"abc", "900150983cd24fb0d6963f7d28e17f72"),
            (b"message digest", "f96b697d7cb7938d525a2f31aaf161d0"),
            (b"abcdefghijklmnopqrstuvwxyz", "c3fcd3d76192e4007dfb496cca67e13b"),
            (b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", "d174ab98d277d9f5a5611c2c9f419d9f"),
            (
                b"12345678901234567890123456789012345678901234567890123456789012345678901234567890",
                "57edf4a22be3c955ac49da2e2107b67a",
            ),
        ];
        for (input, want) in cases {
            assert_eq!(hex(&Md5::digest(input)), want);
        }
    }

    #[test]
    fn pieces_equal_the_whole_and_finish_resets() {
        let data: Vec<u8> = (0..200u32).map(|i| (i * 7) as u8).collect();
        for cut in [0, 1, 55, 56, 63, 64, 65, 128, 199] {
            let mut m = Md5::new();
            m.update(&data[..cut]);
            m.update(&data[cut..]);
            assert_eq!(m.finish(), Md5::digest(&data), "cut {cut}");
            assert_eq!(m.finish(), Md5::digest(b""));
        }
    }
}
