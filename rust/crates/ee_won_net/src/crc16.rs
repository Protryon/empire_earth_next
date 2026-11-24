//! WON's CRC-16 (`WONCommon/CRC16.cpp`): polynomial 0x8005, most significant bit first, no reflection, with a
//! caller's initial and final XOR values. CD keys and the lobby's persistent data use it.
//!
//! The table and the register's step are the `crc` crate's (docs/progress/deps.md): its CRC-16/UMTS is this
//! polynomial with no reflection, a zero initial value and no final XOR; the caller's initial value goes in through
//! `digest_with_initial` and the final XOR stays here.

use crc::{Crc, Table, CRC_16_UMTS};

static UMTS: Crc<u16, Table<1>> = Crc::<u16, Table<1>>::new(&CRC_16_UMTS);

#[cfg(test)]
const TABLE: [u16; 256] = Crc::<u16, Table<1>>::new(&CRC_16_UMTS).table()[0];

/// Feeds bytes into a CRC register and returns the new register.
pub fn put(register: u16, data: &[u8]) -> u16 {
    let mut digest = UMTS.digest_with_initial(register);
    digest.update(data);
    digest.finalize()
}

/// A CRC with the C++ class's state: register, initial value, final XOR.
#[derive(Clone, Copy, Debug)]
pub struct Crc16 {
    register: u16,
    init: u16,
    xor_out: u16,
}

impl Crc16 {
    /// `CRC16(init, xorOut)`.
    pub fn new(init: u16, xor_out: u16) -> Crc16 {
        Crc16 { register: init, init, xor_out }
    }

    /// `Put`.
    pub fn put(&mut self, data: &[u8]) {
        self.register = put(self.register, data);
    }

    /// `Get`.
    pub fn get(&self) -> u16 {
        self.register ^ self.xor_out
    }

    /// `Reset`.
    pub fn reset(&mut self) {
        self.register = self.init;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn table_matches_the_cpp_table_corners() {
        assert_eq!(&TABLE[..4], &[0x0000, 0x8005, 0x800f, 0x000a]);
        assert_eq!(TABLE[255], 0x0202);
        assert_eq!(TABLE[128], 0x8303);
    }

    #[test]
    fn known_values() {
        // CRC-16/UMTS ("123456789", init 0, no XOR) is 0xfee8; CRC-16/CMS (init 0xffff) is 0xaee7
        assert_eq!(put(0, b"123456789"), 0xfee8);
        assert_eq!(put(0xffff, b"123456789"), 0xaee7);
        let mut c = Crc16::new(0xffff, 0xffff);
        c.put(b"1234");
        c.put(b"56789");
        assert_eq!(c.get(), 0xaee7 ^ 0xffff);
        c.reset();
        assert_eq!(c.get(), 0);
    }
}
