//! The simulation capture's sweep payloads (game messages round 6; the capture is `EEMessageCapture.cpp`, test only):
//! the NMSW game messages over edge payloads (all 0x00, all 0xff, 0x7f, 0x80, a counting pattern, random bytes;
//! strings of 0..300 bytes, unit lists of 0..63 ids, upgrade lists), the NMCU box selections (`CompressUnits` and
//! `DecompressUnits` over id lists, each record with the selection message built from it), the box selection's two
//! other types, and the NMNE engine messages (0x20..0x3c by their shapes, the two communications ticks with nested
//! players' inputs). The C++ capture builds each payload with the messages' factory and records what it built; the
//! payloads and their order are the ones the capture's C++ made (one xorshift stream through all of them).

use crate::netmsg::records::{ee_netmsg_engine_sweep_shape, ee_netmsg_sweep_shape};

/// A game message's payload: the capture builds it (`EEMessage::BuildEEMessage`, an NMIM record)
pub const EE_NETMSG_SWEEP_ITEM_GAME: u8 = 1;
/// An engine message's payload: the capture builds it and records it (NMNE)
pub const EE_NETMSG_SWEEP_ITEM_ENGINE: u8 = 2;
/// A box selection's NMCU record, whole: the capture commits its bytes
pub const EE_NETMSG_SWEEP_ITEM_COMPRESS: u8 = 3;

/// The sweep's game types, in the capture's order (the original classes')
const SWEEP_TYPES: [u8; 107] = [
    0x50, 0x72, 0x9b, 0x4f, 0x49, 0x76, 0x9f, 0x70, 0x4c, 0x86, 0x56, 0x59, 0x6e, 0x75, 0x8d, 0x78, 0x5a, 0x85, 0xa7,
    0x7f, 0x7e, 0x87, 0xa4, 0x61, 0x41, 0x42, 0x46, 0x47, 0x88, 0x89, 0x79, 0xa9, 0x60, 0x91, 0x5f, 0x51, 0xac, 0x6b,
    0x58, 0x92, 0x95, 0x6c, 0x8e, 0x7d, 0xad, 0x5c, 0xa8, 0x97, 0x98, 0x64, 0x9a, 0x94, 0x71, 0x7b, 0x67, 0x8c, 0xab,
    0x68, 0x55, 0x7c, 0x43, 0x4d, 0x73, 0x44, 0x5b, 0xaa, 0x6a, 0x6d, 0x4a, 0x57, 0x81, 0x53, 0x8b, 0x52, 0xa6, 0xa3,
    0x90, 0x82, 0x93, 0x83, 0x84, 0xa0, 0xa2, 0x74, 0xa1, 0x96, 0x48, 0x4e, 0xa5, 0x9c, 0x80, 0x99, 0x66, 0x54, 0x8f,
    0x5d, 0x5e, 0x8a, 0x63, 0x65, 0x62, 0x4b, 0x7a, 0x77, 0x6f, 0x9e, 0xae,
];
const STRING_LENGTHS: [u32; 8] = [0, 1, 2, 7, 31, 64, 255, 300];
const IDS: [u32; 15] = [
    0, 1, 0xff, 0x100, 0x1ff, 0xffff, 0x10000, 0xffff00, 0xffffff, 0x1000000, 0x7fffffff, 0x80000000, 0xffffffff,
    0x123456, 0xabcdef,
];
/// ASCII and UTF-8
const TEXT: &[u8] = b"Empire Earth \xc3\xa9\xc3\xa8 \xe2\x82\xac chat";
const SELECT_UNITS: u8 = 0x4e;
const ATTACK_UNIT: u8 = 0x4c;
const CHAT: u8 = 0x5a;
const BUILD_WALL: u8 = 0x59;
const GAME_SPEED: u8 = 0x5f;
const EXPLORE: u8 = 0x79;
const NE_PAUSE_GAME: u8 = 0x33;
const NE_COMMUNICATIONS_TICK: u8 = 0x26;
const NE_SERVER_IS_ALIVE: u8 = 0x3a;

/// The payloads in order, each with its kind
pub struct Sweep {
    random: u32,
    pub items: Vec<(u8, Vec<u8>)>,
}

impl Default for Sweep {
    fn default() -> Self {
        Sweep { random: 0x2545f491, items: Vec::new() }
    }
}

impl Sweep {
    fn next_random(&mut self) -> u8 {
        self.random ^= self.random.wrapping_shl(13);
        self.random ^= self.random >> 17;
        self.random ^= self.random.wrapping_shl(5);
        (self.random >> 8) as u8
    }

    /// pattern p of n bytes: 0 all 0x00, 1 all 0xff, 2 all 0x7f, 3 all 0x80, 4 counting, 5.. random
    fn pattern(&mut self, data: &mut Vec<u8>, count: u32, p: u32) {
        for i in 0..count {
            let byte = match p {
                0 => 0x00,
                1 => 0xff,
                2 => 0x7f,
                3 => 0x80,
                4 => (i.wrapping_mul(37).wrapping_add(11)) as u8,
                _ => self.next_random(),
            };
            data.push(byte);
        }
    }

    /// a new message's header with pattern p: reserved, type, target slot, originating slot
    fn header(&mut self, message_type: u8, p: u32) -> Vec<u8> {
        let mut data = Vec::new();
        self.pattern(&mut data, 4, p);
        data[1] = message_type;
        data
    }

    /// a wire string (UWideString::Export): u32 length + 2, the bytes, u16 0
    fn string(&mut self, data: &mut Vec<u8>, length: u32, p: u32) {
        data.extend_from_slice(&(length + 2).to_le_bytes());
        for i in 0..length {
            let byte = if p == 0 { TEXT[i as usize % TEXT.len()] } else { 0x21 + (self.next_random() % 0x5e) };
            data.push(byte);
        }
        data.extend_from_slice(&[0, 0]);
    }

    fn game(&mut self, data: Vec<u8>) {
        self.items.push((EE_NETMSG_SWEEP_ITEM_GAME, data));
    }

    /// `CompressUnits` and `DecompressUnits` over a list: the buffer and the ids decoded
    fn compress(units: &[u32], owner: u32) -> (Vec<u8>, Vec<i32>) {
        let mut buffer = [0u8; 0x100];
        let size = crate::netmsg::units::compress(units, owner, &mut buffer);
        // the selection's own copy of the buffer decoded (`EESelectUnitsIDs`: at most 256 ids)
        let mut copy = [0u8; 0x100];
        copy[..size as usize].copy_from_slice(&buffer[..size as usize]);
        let mut ids = Vec::new();
        crate::netmsg::units::decompress(&copy, size, &mut ids);
        ids.truncate(0x100);
        (buffer[..size as usize].to_vec(), ids)
    }

    /// an NMCU record (the owner, the ids, the buffer, the ids decoded), then the selection message built from it
    fn compress_case(&mut self, units: &[u32], owner: u32) {
        let (buffer, ids) = Self::compress(units, owner);
        let mut record = Vec::new();
        record.extend_from_slice(&owner.to_le_bytes());
        record.extend_from_slice(&(units.len() as u32).to_le_bytes());
        for id in units {
            record.extend_from_slice(&id.to_le_bytes());
        }
        record.extend_from_slice(&(buffer.len() as u32).to_le_bytes());
        record.extend_from_slice(&buffer);
        record.extend_from_slice(&(ids.len() as u32).to_le_bytes());
        for id in &ids {
            record.extend_from_slice(&id.to_le_bytes());
        }
        self.items.push((EE_NETMSG_SWEEP_ITEM_COMPRESS, record));
        let mut data = self.header(SELECT_UNITS, 4);
        data.push(0x5a);
        data.push(buffer.len() as u8);
        data.extend_from_slice(&buffer);
        self.game(data);
    }

    fn unit_lists(&mut self) {
        let mut owner = 0u32;
        while owner < 0x101 {
            self.compress_case(&[], owner); // the empty selection
            for &id in &IDS {
                self.compress_case(&[id], owner);
            }
            self.compress_case(&IDS, owner);
            let mut count = 2u32;
            while count <= 80 {
                let mut units = Vec::new();
                for _ in 0..count {
                    // ids spread over a few groups, some repeated
                    let mut id = ((self.next_random() as u32) << 16) % 0x30000;
                    id |= ((self.next_random() as u32) << 8) & 0x300;
                    id |= self.next_random() as u32;
                    units.push(id | (owner << 24));
                }
                self.compress_case(&units, owner);
                count += 13;
            }
            let units: Vec<u32> = (0..70).map(|i| 0x4200 + i).collect(); // one group over the 63-id limit
            self.compress_case(&units, owner);
            let units: Vec<u32> = (0..70u32).map(|i| (i << 8) | (i * 7)).collect(); // many groups over the limit
            self.compress_case(&units, owner);
            owner += 0x40;
        }
    }

    /// a fixed game message of the sweep's rows with pattern p appended
    fn put_game_message(&mut self, data: &mut Vec<u8>, message_type: u8, p: u32) {
        let mut message = self.header(message_type, p);
        let prefix = ee_netmsg_sweep_shape(message_type).prefix;
        self.pattern(&mut message, prefix - 4, p);
        data.extend_from_slice(&message);
    }

    fn put_header(&mut self, data: &mut Vec<u8>, message_type: u8, p: u32) {
        let header = self.header(message_type, p);
        data.extend_from_slice(&header);
    }

    /// the players' inputs list number l (a count byte, then the messages)
    fn put_inputs(&mut self, data: &mut Vec<u8>, list: u32, p: u32) {
        match list {
            0 => data.push(0), // none
            1 => {
                // one game message
                data.push(1);
                self.put_game_message(data, ATTACK_UNIT, p);
            }
            2 => {
                // a chat, an engine message, a wall
                data.push(3);
                self.put_header(data, CHAT, p);
                self.pattern(data, 2, p);
                self.string(data, STRING_LENGTHS[(p % 8) as usize], p & 1);
                self.put_header(data, NE_PAUSE_GAME, p);
                self.pattern(data, 8, p);
                self.put_game_message(data, BUILD_WALL, p);
            }
            3 => {
                // an unknown type first: the import does not advance, so every input is skipped
                data.push(2);
                self.put_header(data, 0x05, p);
                self.put_game_message(data, ATTACK_UNIT, p);
            }
            4 => {
                // a known input, then an unknown type, then one the import never reaches
                data.push(3);
                self.put_game_message(data, GAME_SPEED, p);
                self.put_header(data, 0xfe, p);
                self.put_game_message(data, ATTACK_UNIT, p);
            }
            5 => {
                // a communications tick in the inputs, with its own input
                data.push(2);
                self.put_header(data, NE_COMMUNICATIONS_TICK, p);
                self.pattern(data, 8, p);
                self.put_inputs(data, 1, (p + 1) % 9);
                self.put_game_message(data, EXPLORE, p);
            }
            _ => {
                // 255 inputs, the most the list holds
                data.push(255);
                for i in 0..255 {
                    self.put_header(data, NE_SERVER_IS_ALIVE, (p + i) % 5);
                }
            }
        }
    }

    /// every engine type (0x20..0x3c) by its shape: the fixed bytes before its strings or its players' inputs (a
    /// 4-byte member follows two strings)
    fn engine_sweep(&mut self) {
        for message_type in 0x20u8..=0x3c {
            if crate::netmsg::ffi::ee_netmsg_class_of_type(message_type) == 0xffffffff {
                continue;
            }
            let row = ee_netmsg_engine_sweep_shape(message_type);
            for p in 0..9u32 {
                let variants = if row.inputs != 0 {
                    7
                } else if row.strings > 0 {
                    8
                } else {
                    1
                };
                for v in 0..variants {
                    let mut data = self.header(message_type, p);
                    self.pattern(&mut data, row.fixed, p);
                    if row.inputs != 0 {
                        self.put_inputs(&mut data, v, p);
                    } else if row.strings > 0 {
                        self.string(&mut data, STRING_LENGTHS[v as usize], p & 1);
                        if row.strings > 1 {
                            self.string(&mut data, STRING_LENGTHS[((v + 3) % 8) as usize], (p + 1) & 1);
                            self.pattern(&mut data, 4, p);
                        }
                    }
                    self.items.push((EE_NETMSG_SWEEP_ITEM_ENGINE, data));
                }
            }
        }
    }

    /// The whole sweep, in the capture's order
    pub fn run(&mut self) {
        use crate::netmsg::records::{
            EE_NETMSG_SWEEP_CIV_UPGRADES, EE_NETMSG_SWEEP_FIXED, EE_NETMSG_SWEEP_STRING, EE_NETMSG_SWEEP_THESE_UNITS,
        };
        for &message_type in &SWEEP_TYPES {
            let row = ee_netmsg_sweep_shape(message_type);
            for p in 0..9u32 {
                match row.kind {
                    EE_NETMSG_SWEEP_FIXED => {
                        let mut data = self.header(message_type, p);
                        self.pattern(&mut data, row.prefix - 4, p);
                        self.game(data);
                    }
                    EE_NETMSG_SWEEP_STRING => {
                        for &length in &STRING_LENGTHS {
                            let mut data = self.header(message_type, p);
                            self.pattern(&mut data, row.prefix - 4, p);
                            self.string(&mut data, length, p & 1);
                            self.pattern(&mut data, row.suffix, p);
                            self.game(data);
                        }
                    }
                    EE_NETMSG_SWEEP_THESE_UNITS => {
                        let mut n = 0u32;
                        while n <= 63 {
                            let mut data = self.header(message_type, p);
                            self.pattern(&mut data, 1, p);
                            data.extend_from_slice(&n.to_le_bytes());
                            self.pattern(&mut data, 4 * n, p);
                            self.game(data);
                            n += if n < 4 { 1 } else { 19 };
                        }
                    }
                    EE_NETMSG_SWEEP_CIV_UPGRADES => {
                        let mut n = 0u32;
                        while n <= 40 {
                            let mut data = self.header(message_type, p);
                            self.pattern(&mut data, row.prefix - 4, p);
                            data.push(n as u8);
                            self.pattern(&mut data, 4 * n, p);
                            self.string(&mut data, STRING_LENGTHS[((n + p) % 8) as usize], p & 1);
                            self.game(data);
                            n += if n < 3 { 1 } else { 37 };
                        }
                    }
                    _ => {} // the box selection: in unit_lists (only CompressUnits' buffers decode)
                }
            }
        }
        self.unit_lists();
        // the box selection's other two types over one buffer each
        let (buffer, _) = Self::compress(&IDS[..5], 3);
        for message_type in [0x4fu8, 0x50] {
            let mut data = self.header(message_type, 5);
            data.push(0x11);
            data.push(buffer.len() as u8);
            data.extend_from_slice(&buffer);
            self.game(data);
        }
        self.engine_sweep();
    }
}

/// The NMCU record's tag (the capture commits an `EE_NETMSG_SWEEP_ITEM_COMPRESS` item's bytes under it)
pub const EE_NETMSG_SWEEP_NMCU: u32 = 0x55434d4e;

/// The sweep for the capture: each payload handed to `sink` in order (kind, bytes); no Rust state is borrowed
/// across the calls (the payloads are made first).
///
/// # Safety
/// `sink` is the capture's C++ function.
#[no_mangle]
pub unsafe extern "C" fn ee_netmsg_capture_sweep(sink: unsafe extern "C" fn(kind: u8, data: *const u8, size: usize)) {
    let mut sweep = Sweep::default();
    sweep.run();
    for (kind, data) in std::mem::take(&mut sweep.items) {
        sink(kind, data.as_ptr(), data.len());
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_stream_and_the_patterns() {
        let mut s = Sweep::default();
        // xorshift32 from 0x2545f491: the C++ capture's first draws
        let mut x: u32 = 0x2545f491;
        for _ in 0..4 {
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            assert_eq!(s.next_random(), (x >> 8) as u8);
        }
        let mut d = Vec::new();
        s.pattern(&mut d, 3, 4);
        assert_eq!(d, vec![11, 48, 85]);
        let h = s.header(0x5a, 1);
        assert_eq!(h, vec![0xff, 0x5a, 0xff, 0xff]);
        let mut w = Vec::new();
        s.string(&mut w, 3, 0);
        assert_eq!(w, vec![5, 0, 0, 0, b'E', b'm', b'p', 0, 0]);
    }

    #[test]
    fn the_sweep_is_whole_and_in_order() {
        let mut s = Sweep::default();
        s.run();
        let count = |k| s.items.iter().filter(|i| i.0 == k).count();
        // five owners of 15 single ids, the empty list, all 15, 7 random lists, two over the limit
        assert_eq!(count(EE_NETMSG_SWEEP_ITEM_COMPRESS), 5 * (1 + 15 + 1 + 7 + 2));
        assert!(count(EE_NETMSG_SWEEP_ITEM_GAME) > 1000);
        assert!(count(EE_NETMSG_SWEEP_ITEM_ENGINE) > 200);
        // every NMCU record is followed by the selection message built from its buffer
        for (i, item) in s.items.iter().enumerate() {
            if item.0 == EE_NETMSG_SWEEP_ITEM_COMPRESS {
                let next = &s.items[i + 1];
                assert_eq!(next.0, EE_NETMSG_SWEEP_ITEM_GAME);
                assert_eq!(next.1[1], SELECT_UNITS);
            }
        }
        // the engine payloads come last, the first of type 0x20
        let first = s.items.iter().position(|i| i.0 == EE_NETMSG_SWEEP_ITEM_ENGINE).unwrap();
        assert!(s.items[first..].iter().all(|i| i.0 == EE_NETMSG_SWEEP_ITEM_ENGINE));
        assert_eq!(s.items[first].1[1], 0x20);
        // the same payloads every time
        let mut again = Sweep::default();
        again.run();
        assert_eq!(again.items, s.items);
    }
}
