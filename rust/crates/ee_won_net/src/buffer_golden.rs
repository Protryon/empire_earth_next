//! `golden_buffer.txt`: the C++ ByteBuffer's, WriteBuffer's and ReadBuffer's answers to
//! `tests/won/won_buffer_test.cpp`'s script, recorded over the original classes before their move to `buffer`. This
//! test reads the script out of that source and replays it over the Rust objects; the C++ shims' own steps (the
//! fixed-size appends as their little-endian bytes, a string read erased before its bytes are taken, the wide text's
//! UTF-16 bytes) are modelled here as the shims do them.

// (c_ulong and c_long are 32 bits on Windows, 64 elsewhere: the casts are needed off Windows)
#![allow(clippy::unnecessary_cast)]

use crate::buffer::{EeWonByteBuffer, EeWonReadBuffer, EeWonWriteBuffer};
use crate::text;

const GOLDEN: &str = include_str!("golden_buffer.txt");
const SOURCE: &str = include_str!("../../../../tests/won/won_buffer_test.cpp");
const PAST_END: &str = "Attempt to read past end of buffer.";

fn script() -> Vec<String> {
    let start = SOURCE.find("kScript[] = {").expect("the script");
    let end = start + SOURCE[start..].find("};").expect("its end");
    SOURCE[start..end].split('"').skip(1).step_by(2).map(str::to_string).collect()
}

fn hex(data: &[u8]) -> String {
    if data.is_empty() {
        return "-".into();
    }
    data.iter().map(|b| format!("{b:02x}")).collect()
}

fn units_text(units: &[u16]) -> String {
    if units.is_empty() {
        return "-".into();
    }
    units.iter().map(|u| format!("{u:04x}")).collect::<Vec<_>>().join(".")
}

fn number(arg: &str) -> i64 {
    let (neg, a) = match arg.strip_prefix('-') {
        Some(r) => (true, r),
        None => (false, arg),
    };
    let v = if let Some(h) = a.strip_prefix("0x") { i64::from_str_radix(h, 16).unwrap() } else { a.parse().unwrap() };
    if neg {
        -v
    } else {
        v
    }
}

fn bytes_arg(arg: &str) -> Vec<u8> {
    if arg == "-" {
        return Vec::new();
    }
    (0..arg.len() / 2).map(|i| u8::from_str_radix(&arg[2 * i..2 * i + 2], 16).unwrap()).collect()
}

fn units_arg(arg: &str) -> Vec<u16> {
    if arg == "-" {
        return Vec::new();
    }
    arg.split('.').map(|u| u16::from_str_radix(u, 16).unwrap()).collect()
}

fn fnv(data: &[u8]) -> u64 {
    let mut h = 0xcbf2_9ce4_8422_2325u64;
    for &b in data {
        h ^= u64::from(b);
        h = h.wrapping_mul(0x100_0000_01b3);
    }
    h
}

fn summary(data: Option<&[u8]>, length: usize) -> String {
    match data {
        None => format!("null {length}"),
        Some(d) => format!("{} {:016x} {}", d.len(), fnv(d), hex(&d[..d.len().min(16)])),
    }
}

fn byte_summary(b: &EeWonByteBuffer) -> String {
    summary(if b.is_null() { None } else { Some(b.bytes()) }, b.bytes().len())
}

struct Run {
    writer: EeWonWriteBuffer,
    buffer: EeWonByteBuffer,
    /// the bytes the reader reads (a copy of what the C++ reader pointed at)
    read_bytes: Vec<u8>,
    reader: EeWonReadBuffer,
}

enum Thrown {
    Past,
}

impl Run {
    fn writer_state(&self) -> String {
        format!("{} {}", self.writer.capacity(), summary(self.writer.data(), self.writer.length() as usize))
    }

    fn point_reader(&mut self, data: Vec<u8>, null: bool) -> String {
        self.read_bytes = data;
        let p = if null { std::ptr::null() } else { self.read_bytes.as_ptr() };
        self.reader = EeWonReadBuffer::new(p, self.read_bytes.len() as std::ffi::c_ulong);
        self.reader.len().to_string()
    }

    fn take(&mut self, n: u64) -> Result<Vec<u8>, Thrown> {
        // SAFETY: the reader points at `read_bytes`
        unsafe { self.reader.read(n) }.map(<[u8]>::to_vec).ok_or(Thrown::Past)
    }

    fn length(&mut self, size: u8) -> Result<u32, Thrown> {
        // SAFETY: as `take`
        unsafe { self.reader.read_length(size) }.ok_or(Thrown::Past)
    }

    fn step(&mut self, op: &str, a: &str, b: &str) -> Result<String, Thrown> {
        if op == "w.new" {
            self.writer = EeWonWriteBuffer::new(number(a) as u8, number(b) != 0);
            return Ok(self.writer_state());
        }
        if op.starts_with("w.") {
            let w = &mut self.writer;
            match op {
                "w.state" => {}
                "w.byte" => w.append_bytes(&[number(a) as u8]),
                "w.bool" => w.append_bytes(&[u8::from(number(a) != 0)]),
                "w.short" => w.append_bytes(&(number(a) as i16).to_le_bytes()),
                "w.long" => w.append_bytes(&(number(a) as i32).to_le_bytes()),
                "w.length" => w.append_length(number(a) as std::ffi::c_ulong, number(b) as u8),
                "w.string" => w.append_field(&bytes_arg(a), number(b) as u8),
                "w.rawstring" => w.append_field(&bytes_arg(a), 0),
                "w.wstring" => w.append_utf16le(&text::utf16_to_le_bytes(&units_arg(a)), number(b) as u8),
                "w.buffer" => {
                    let d = if a == "--" { Vec::new() } else { bytes_arg(a) };
                    w.append_field(&d, number(b) as u8)
                }
                "w.bytes" => w.append_bytes(&bytes_arg(a)),
                "w.skip" => w.skip_bytes(number(a) as std::ffi::c_ulong),
                "w.setsize" => w.set_size(number(a) as std::ffi::c_ulong),
                "w.reserve" => w.reserve(number(a) as std::ffi::c_ulong),
                "w.chunk" => w.reserve_chunk(number(a) as std::ffi::c_ulong, number(b) as std::ffi::c_ulong),
                "w.setbyte" => w.set_bytes(number(a) as std::ffi::c_ulong, &[number(b) as u8]),
                "w.setshort" => w.set_bytes(number(a) as std::ffi::c_ulong, &(number(b) as i16).to_le_bytes()),
                "w.setbytes" => w.set_bytes(number(a) as std::ffi::c_ulong, &bytes_arg(b)),
                "w.reset" => w.reset(),
                "w.tobuf" => {
                    self.buffer = w.to_byte_buffer(number(a) != 0);
                    return Ok(format!("{} / {}", byte_summary(&self.buffer), self.writer_state()));
                }
                _ => return Ok("unknown op".into()),
            }
            return Ok(self.writer_state());
        }
        match op {
            "b.state" => return Ok(byte_summary(&self.buffer)),
            "b.text" => {
                self.buffer = if a == "NULL" {
                    EeWonByteBuffer::from_text(None)
                } else {
                    let mut t = bytes_arg(a);
                    if let Some(z) = t.iter().position(|&c| c == 0) {
                        t.truncate(z);
                    }
                    t.push(0);
                    EeWonByteBuffer::from_text(Some(std::ffi::CStr::from_bytes_with_nul(&t).unwrap()))
                };
                return Ok(byte_summary(&self.buffer));
            }
            "b.copy" => {
                self.buffer = EeWonByteBuffer::copy(&bytes_arg(a));
                return Ok(byte_summary(&self.buffer));
            }
            "r.overb" => {
                let null = self.buffer.is_null();
                let d = self.buffer.bytes().to_vec();
                return Ok(self.point_reader(d, null));
            }
            "r.overw" => {
                let d = self.writer.data().map(<[u8]>::to_vec);
                let null = d.is_none();
                return Ok(self.point_reader(d.unwrap_or_default(), null));
            }
            "r.pos" => return Ok(self.reader.pos.to_string()),
            "r.avail" => return Ok((self.reader.len() as i32).wrapping_sub(self.reader.pos as i32).to_string()),
            "r.length" => return Ok(self.reader.len().to_string()),
            "r.rewind" => {
                self.reader.pos = 0;
                return Ok("0".into());
            }
            "r.setpos" => {
                if number(a) > i64::from(self.reader.len() as u32) {
                    return Ok("past".into());
                }
                self.reader.pos = number(a) as std::ffi::c_long;
                return Ok(self.reader.pos.to_string());
            }
            "r.byte" => return Ok(self.take(1)?[0].to_string()),
            "r.bool" => return Ok(if self.take(1)?[0] != 0 { "true" } else { "false" }.into()),
            "r.short" => {
                let d = self.take(2)?;
                return Ok(i16::from_le_bytes([d[0], d[1]]).to_string());
            }
            "r.long" => {
                let d = self.take(4)?;
                return Ok(i32::from_le_bytes([d[0], d[1], d[2], d[3]]).to_string());
            }
            "r.rlen" => return Ok(self.length(number(a) as u8)?.to_string()),
            "r.bytes" => {
                let d = self.take(number(a) as u64)?;
                return Ok(format!("{} {}", hex(&d), self.reader.pos));
            }
            "r.buf" => {
                let n = self.length(number(a) as u8)?;
                let d = self.take(u64::from(n))?;
                return Ok(format!("{} {}", byte_summary(&EeWonByteBuffer::copy(&d)), self.reader.pos));
            }
            _ => {}
        }
        // the string reads: what the throw left in the strings ("unset" until erased)
        let mut string: Option<Vec<u8>> = None;
        let mut wide: Option<Vec<u16>> = None;
        let unset_s = b"unset".to_vec();
        let unset_w: Vec<u16> = "unset".encode_utf16().collect();
        let result: Result<String, Thrown> = (|| match op {
            "r.rawstring" => {
                string = Some(Vec::new());
                let d = self.take(number(a) as u64)?;
                Ok(hex(&d))
            }
            "r.string" => {
                let n = self.length(number(a) as u8)?;
                string = Some(Vec::new());
                let d = self.take(u64::from(n))?;
                Ok(hex(&d))
            }
            "r.rawwstring" | "r.wstring" => {
                let n = if op == "r.wstring" { u64::from(self.length(number(a) as u8)?) } else { number(a) as u64 };
                wide = Some(Vec::new());
                let d = self.take(n * 2)?;
                Ok(units_text(&text::utf16_from_le_bytes(&d)))
            }
            _ => Ok("unknown op".into()),
        })();
        match result {
            Ok(s) => Ok(s),
            Err(Thrown::Past) => Ok(format!(
                "throw {PAST_END} / {} / {} / {}",
                hex(string.as_deref().unwrap_or(&unset_s)),
                units_text(wide.as_deref().unwrap_or(&unset_w)),
                self.reader.pos
            )),
        }
    }
}

#[test]
fn the_cpp_buffers_answers_replayed() {
    let mut run = Run {
        writer: EeWonWriteBuffer::new(0, false),
        buffer: EeWonByteBuffer::default(),
        read_bytes: Vec::new(),
        reader: EeWonReadBuffer::new(std::ptr::null(), 0),
    };
    let want: Vec<&str> = GOLDEN.lines().collect();
    let steps = script();
    assert_eq!(steps.len(), want.len());
    let mut failures = 0;
    for (i, step) in steps.iter().enumerate() {
        let mut parts = step.splitn(3, ' ');
        let op = parts.next().unwrap();
        let a = parts.next().unwrap_or("");
        let b = parts.next().unwrap_or("");
        let got = match run.step(op, a, b) {
            Ok(r) => format!("{op} -> {r}"),
            Err(Thrown::Past) => format!("{op} -> throw {PAST_END} / {}", run.reader.pos),
        };
        if got != want[i] {
            failures += 1;
            if failures < 10 {
                eprintln!("line {} ({step})\n  got  {got}\n  want {}", i + 1, want[i]);
            }
        }
    }
    assert_eq!(failures, 0);
}
