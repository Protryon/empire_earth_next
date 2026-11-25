//! MD5 (RFC 1321), the digest behind both of WON's MD5 classes: `WONCommon/MD5.cpp` (`MD5`: Update, Digest) and
//! `WONCrypt/MD5Digest.cpp` (`MD5Digest`: update, digest, used by the signatures and the random generator). Both
//! compute plain MD5; a finished digest resets the state.
//!
//! The digest is the `md-5` crate's (docs/progress/deps.md). The C++ forwarders embed the running state by value and
//! copy it with their objects, and the crate's types have no public layout: [`Md5`] keeps the pending bytes itself
//! and the crate's core (`Md5Core`: the four state words and the block count, 24 bytes of integers) in storage of
//! that size, which it hands whole blocks and, at the end, the rest.

use std::mem::{align_of, size_of};

use md5::digest::core_api::{Buffer, FixedOutputCore, UpdateCore};
use md5::digest::generic_array::GenericArray;
use md5::Md5Core;

// three words with no padding (8 + 4 * 4 bytes): a copy of the storage is a copy of the core
const _: () = assert!(size_of::<Md5Core>() == 24 && align_of::<Md5Core>() <= 8 && !std::mem::needs_drop::<Md5Core>());

/// A running MD5 digest. `#[repr(C)]` and plain data: the C++ forwarders embed it by value.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct Md5 {
    /// the `md-5` crate's `Md5Core`
    core: [u64; 3],
    /// bytes hashed so far
    count: u64,
    pending: [u8; 64],
}

impl Default for Md5 {
    fn default() -> Self {
        Md5::new()
    }
}

impl Md5 {
    /// A digest in its initial state.
    pub fn new() -> Md5 {
        let mut m = Md5 { core: [0; 3], count: 0, pending: [0; 64] };
        m.reset();
        m
    }

    /// Back to the initial state. (The one way a state is made: a C++ object's storage is first written here.)
    pub fn reset(&mut self) {
        self.count = 0;
        self.pending = [0; 64];
        // SAFETY: the storage has `Md5Core`'s size and at least its alignment (the assertion above)
        unsafe { (&raw mut self.core).cast::<Md5Core>().write(Md5Core::default()) }
    }

    fn core(&mut self) -> &mut Md5Core {
        // SAFETY: as `reset`, which every `Md5` went through once (`new`, the C++ constructors' `ee_won_md5_reset`)
        unsafe { &mut *(&raw mut self.core).cast::<Md5Core>() }
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
            self.core().update_blocks(&[block.into()]);
        }
        let (blocks, rest) = data.as_chunks::<64>();
        for block in blocks {
            self.core().update_blocks(&[(*block).into()]);
        }
        self.pending[..rest.len()].copy_from_slice(rest);
    }

    /// Pads, returns the 16-byte digest and resets.
    pub fn finish(&mut self) -> [u8; 16] {
        let have = (self.count % 64) as usize;
        let mut rest = Buffer::<Md5Core>::new(&self.pending[..have]);
        let mut out = GenericArray::default();
        self.core().finalize_fixed_core(&mut rest, &mut out);
        self.reset();
        out.into()
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
