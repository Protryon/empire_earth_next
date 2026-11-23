use super::*;

/// A little-endian file in memory (a failed read leaves the value as it was and returns 1).
#[derive(Default)]
struct Mem {
    bytes: Vec<u8>,
    at: usize,
}

impl Mem {
    fn take(&mut self, n: usize) -> Option<Vec<u8>> {
        (self.at + n <= self.bytes.len()).then(|| {
            self.at += n;
            self.bytes[self.at - n..self.at].to_vec()
        })
    }
}

impl FilePrims for Mem {
    fn read_u32(&mut self, v: &mut u32) -> c_long {
        self.take(4).map_or(1, |b| {
            *v = u32::from_le_bytes([b[0], b[1], b[2], b[3]]);
            0
        })
    }
    fn read_i32(&mut self, v: &mut i32) -> c_long {
        self.take(4).map_or(1, |b| {
            *v = i32::from_le_bytes([b[0], b[1], b[2], b[3]]);
            0
        })
    }
    fn read_f32(&mut self, v: &mut f32) -> c_long {
        self.take(4).map_or(1, |b| {
            *v = f32::from_le_bytes([b[0], b[1], b[2], b[3]]);
            0
        })
    }
    fn read_bool(&mut self, v: &mut u8) -> c_long {
        self.take(1).map_or(1, |b| {
            *v = b[0];
            0
        })
    }
    fn read_u16(&mut self, v: &mut u16) -> c_long {
        self.take(2).map_or(1, |b| {
            *v = u16::from_le_bytes([b[0], b[1]]);
            0
        })
    }
    fn read_bytes(&mut self, data: *mut u8, len: c_ulong, read: &mut c_ulong) -> c_long {
        self.take(len as usize).map_or(1, |b| {
            // SAFETY: the test's buffer of `len` bytes
            unsafe { std::ptr::copy_nonoverlapping(b.as_ptr(), data, b.len()) };
            *read = len;
            0
        })
    }
    fn write_u32(&mut self, v: u32) -> c_long {
        self.bytes.extend_from_slice(&v.to_le_bytes());
        0
    }
    fn write_i32(&mut self, v: i32) -> c_long {
        self.bytes.extend_from_slice(&v.to_le_bytes());
        0
    }
    fn write_f32(&mut self, v: f32) -> c_long {
        self.bytes.extend_from_slice(&v.to_le_bytes());
        0
    }
    fn write_bool(&mut self, v: bool) -> c_long {
        self.bytes.push(u8::from(v));
        0
    }
    fn write_u16(&mut self, v: u16) -> c_long {
        self.bytes.extend_from_slice(&v.to_le_bytes());
        0
    }
    fn write_bytes(&mut self, data: *const u8, len: c_ulong, written: &mut c_ulong) -> c_long {
        // SAFETY: the test's buffer of `len` bytes
        self.bytes.extend_from_slice(unsafe { std::slice::from_raw_parts(data, len as usize) });
        *written = len;
        0
    }
}

fn io(i: i64) -> EeUnitsPlayerSaveIo {
    EeUnitsPlayerSaveIo { i, ..Default::default() }
}

#[test]
fn values_round_trip_with_the_long_adapters_widths() {
    let mut m = Mem::default();
    assert_eq!(primitive(&mut m, EE_UNITS_PLAYER_SAVE_WRITE_ULONG, &mut io(0x1_ffff_fffe)), 0);
    assert_eq!(primitive(&mut m, EE_UNITS_PLAYER_SAVE_WRITE_LONG, &mut io(-3)), 0);
    let mut f = EeUnitsPlayerSaveIo { f: 1.5, ..Default::default() };
    assert_eq!(primitive(&mut m, EE_UNITS_PLAYER_SAVE_WRITE_FLOAT, &mut f), 0);
    let mut b = EeUnitsPlayerSaveIo { b: 7, ..Default::default() };
    assert_eq!(primitive(&mut m, EE_UNITS_PLAYER_SAVE_WRITE_BOOL, &mut b), 0);
    assert_eq!(primitive(&mut m, EE_UNITS_PLAYER_SAVE_WRITE_USHORT, &mut io(0x1_2345)), 0);
    assert_eq!(m.bytes.len(), 4 + 4 + 4 + 1 + 2);

    let mut v = io(0);
    assert_eq!(primitive(&mut m, EE_UNITS_PLAYER_SAVE_READ_ULONG, &mut v), 0);
    assert_eq!(v.i, 0xffff_fffe, "4 bytes, zero-extended");
    assert_eq!(primitive(&mut m, EE_UNITS_PLAYER_SAVE_READ_LONG, &mut v), 0);
    assert_eq!(v.i, -3, "sign-extended");
    let mut f = EeUnitsPlayerSaveIo::default();
    primitive(&mut m, EE_UNITS_PLAYER_SAVE_READ_FLOAT, &mut f);
    assert_eq!(f.f, 1.5);
    let mut b = EeUnitsPlayerSaveIo::default();
    primitive(&mut m, EE_UNITS_PLAYER_SAVE_READ_BOOL, &mut b);
    assert_eq!(b.b, 1);
    let mut s = io(0);
    primitive(&mut m, EE_UNITS_PLAYER_SAVE_READ_USHORT, &mut s);
    assert_eq!(s.i, 0x2345);
}

#[test]
fn a_failed_read_keeps_the_old_value_through_the_copy() {
    let mut m = Mem::default();
    let mut v = io(-1);
    assert_eq!(primitive(&mut m, EE_UNITS_PLAYER_SAVE_READ_ULONG, &mut v), 1);
    assert_eq!(v.i, 0xffff_ffff, "the old value through the 32-bit copy");
    let mut l = io(0x1_0000_0005);
    primitive(&mut m, EE_UNITS_PLAYER_SAVE_READ_LONG, &mut l);
    assert_eq!(l.i, 5);
    let mut b = EeUnitsPlayerSaveIo { b: 9, ..Default::default() };
    primitive(&mut m, EE_UNITS_PLAYER_SAVE_READ_BOOL, &mut b);
    assert_eq!(b.b, 1, "a bool as 0 or 1");
}

#[test]
fn levels_are_0x40_raw_bytes_and_other_operations_answer_minus_one() {
    let mut m = Mem::default();
    let out: Vec<u8> = (0..0x40).collect();
    assert_eq!(primitive(&mut m, EE_UNITS_PLAYER_SAVE_WRITE_LEVELS, &mut io(out.as_ptr() as i64)), 0);
    let mut back = [0u8; 0x40];
    assert_eq!(primitive(&mut m, EE_UNITS_PLAYER_SAVE_READ_LEVELS, &mut io(back.as_mut_ptr() as i64)), 0);
    assert_eq!(back.to_vec(), out);
    assert_eq!(primitive(&mut m, 50, &mut io(0)), -1);
}
