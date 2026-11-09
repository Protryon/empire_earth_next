//! The plug-in's datagram cipher: RC4 keyed from a 0x105-byte buffer that a fixed-seed `URandom` fills.

use ee_random::URandom;

const KEY_BUFFER_SIZE: usize = 0x105;
const KEY_LENGTH: usize = 7;

/// The key buffer and the RC4 state. RC4 is keyed anew for each packet (`PrepareCipher`), so a packet's output
/// depends only on the key byte and the data.
pub struct Cipher {
    key_buffer: Option<Box<[u8; KEY_BUFFER_SIZE]>>,
    state: [u8; 256],
}

impl Cipher {
    pub fn new() -> Self {
        Cipher { key_buffer: None, state: [0; 256] }
    }

    /// `NPIMGetStatus`: fills the key buffer from `URandom(0xffffffff)` (`Generate() * 255.0` truncated).
    pub fn fill_key_buffer(&mut self) {
        let mut random = URandom::new(0xffff_ffff);
        let buffer = self.key_buffer.get_or_insert_with(|| Box::new([0; KEY_BUFFER_SIZE]));
        for byte in buffer.iter_mut() {
            *byte = (random.generate() * 255.0) as i32 as u8;
        }
    }

    /// `NPIMDeactivate`: frees the key buffer (ciphers do nothing until the next activation).
    pub fn clear(&mut self) {
        self.key_buffer = None;
    }

    pub fn key_buffer(&self) -> Option<&[u8; KEY_BUFFER_SIZE]> {
        self.key_buffer.as_deref()
    }

    fn prepare(&mut self, key_offset: usize) {
        for (i, s) in self.state.iter_mut().enumerate() {
            *s = i as u8;
        }
        let Some(buffer) = self.key_buffer.as_ref() else {
            return;
        };
        let key = &buffer[key_offset..key_offset + KEY_LENGTH];
        let mut j = 0u8;
        for i in 0..256 {
            j = self.state[i].wrapping_add(key[i % KEY_LENGTH]).wrapping_add(j);
            self.state.swap(i, j as usize);
        }
    }

    /// Enciphers/deciphers `data` in place. No key buffer or a key of 0: nothing happens.
    pub fn apply(&mut self, data: &mut [u8], key: u8) {
        if self.key_buffer.is_none() || key == 0 {
            return;
        }
        self.prepare(key as usize - 1);
        let (mut x, mut y) = (0u8, 0u8);
        for byte in data.iter_mut() {
            x = x.wrapping_add(1);
            y = self.state[x as usize].wrapping_add(y);
            self.state.swap(x as usize, y as usize);
            let index = self.state[x as usize].wrapping_add(self.state[y as usize]);
            *byte ^= self.state[index as usize];
        }
    }
}

impl Default for Cipher {
    fn default() -> Self {
        Cipher::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const GOLDEN: &str = include_str!("golden.txt");

    fn fnv(data: &[u8]) -> u64 {
        data.iter().fold(0xcbf29ce484222325u64, |h, &b| (h ^ u64::from(b)).wrapping_mul(0x100000001b3))
    }

    fn cases(tag: &str) -> Vec<Vec<&'static str>> {
        GOLDEN.lines().map(|l| l.split('\t').collect::<Vec<_>>()).filter(|t| t[0] == tag).collect()
    }

    fn filled() -> Cipher {
        let mut c = Cipher::new();
        c.fill_key_buffer();
        c
    }

    #[test]
    fn key_buffer_matches_the_cpp() {
        let c = filled();
        let t = &cases("random-buffer")[0];
        let buffer = c.key_buffer().unwrap();
        assert_eq!(format!("{:016x}", fnv(&buffer[..])), t[1]);
        let hex: String = buffer[..16].iter().map(|b| format!("{b:02x}")).collect();
        assert_eq!(hex, t[2]);
    }

    #[test]
    fn enciphering_matches_the_cpp() {
        let mut c = filled();
        for t in cases("encipher") {
            let key: u32 = t[1].parse().unwrap();
            let len: usize = t[2].parse().unwrap();
            let mut data: Vec<u8> = (0..len).map(|i| (i as u32 * 7 + key) as u8).collect();
            c.apply(&mut data, key as u8);
            assert_eq!(format!("{:016x}", fnv(&data)), t[3], "{t:?}");
            let hex: String = data.iter().take(16).map(|b| format!("{b:02x}")).collect();
            assert_eq!(hex, t[4], "{t:?}");
        }
    }

    #[test]
    fn key_zero_changes_nothing() {
        let mut c = filled();
        let mut data = [1u8, 2, 3, 4];
        c.apply(&mut data, 0);
        assert_eq!(data, [1, 2, 3, 4]);
        let t = &cases("encipher0")[0];
        assert_eq!(t[1], "01020304");
    }

    #[test]
    fn no_key_buffer_changes_nothing() {
        let mut c = Cipher::new();
        let mut data = [9u8; 4];
        c.apply(&mut data, 7);
        assert_eq!(data, [9u8; 4]);
    }

    #[test]
    fn applying_twice_restores_the_data() {
        let mut c = filled();
        let mut data: Vec<u8> = (0..500u32).map(|i| i as u8).collect();
        let original = data.clone();
        c.apply(&mut data, 200);
        assert_ne!(data, original);
        c.apply(&mut data, 200);
        assert_eq!(data, original);
    }

    #[test]
    fn clearing_the_buffer_disables_the_cipher() {
        let mut c = filled();
        c.clear();
        let mut data = [5u8; 3];
        c.apply(&mut data, 3);
        assert_eq!(data, [5u8; 3]);
    }
}
