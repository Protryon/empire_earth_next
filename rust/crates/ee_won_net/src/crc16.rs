//! WON's CRC-16 (`WONCommon/CRC16.cpp`): polynomial 0x8005, most significant bit first, no reflection, with a
//! caller's initial and final XOR values. CD keys and the lobby's persistent data use it.

const fn table() -> [u16; 256] {
    let mut t = [0u16; 256];
    let mut i = 0;
    while i < 256 {
        let mut r = (i as u16) << 8;
        let mut b = 0;
        while b < 8 {
            r = if r & 0x8000 != 0 { (r << 1) ^ 0x8005 } else { r << 1 };
            b += 1;
        }
        t[i] = r;
        i += 1;
    }
    t
}

const TABLE: [u16; 256] = table();

/// Feeds bytes into a CRC register and returns the new register.
pub fn put(mut register: u16, data: &[u8]) -> u16 {
    for &b in data {
        register = TABLE[usize::from((register >> 8) as u8 ^ b)] ^ (register << 8);
    }
    register
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
