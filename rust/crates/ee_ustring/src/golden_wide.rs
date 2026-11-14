//! Replays the original UWideString's trace (`tests/string/uws_golden.cpp`, format in its header): every call again
//! on Rust records through the C interface the class forwards to, each line regenerated. `Load`, `Save`,
//! `GetChecksum`, `Tokenize`'s vector and `sprintf` are replayed as their C++ forwarders (`UWideString.cpp`) make
//! the calls. `golden_wide.txt` holds the first 60 sequences; `EE_UWSTRING_GOLDEN=<file>` replays a longer run.

use super::*;
use std::ffi::{CStr, CString};
use std::mem::MaybeUninit;

fn unhex(s: &str) -> Vec<u8> {
    if s == "-" {
        return Vec::new();
    }
    (0..s.len()).step_by(2).map(|i| u8::from_str_radix(&s[i..i + 2], 16).unwrap()).collect()
}

fn hex(b: &[u8]) -> String {
    if b.is_empty() {
        return "-".into();
    }
    b.iter().map(|c| format!("{c:02x}")).collect()
}

fn state(s: &EeUWideString) -> String {
    let bytes = unsafe { std::slice::from_raw_parts(s.string.cast::<u8>(), s.capacity as usize) };
    format!(" = {} {} {} {}", s.byte_length, s.length, s.capacity, hex(bytes))
}

fn c_text(s: &str) -> CString {
    CString::new(unhex(s)).unwrap()
}

unsafe fn fresh(f: impl FnOnce(*mut EeUWideString)) -> EeUWideString {
    let mut s = MaybeUninit::<EeUWideString>::uninit();
    f(s.as_mut_ptr());
    s.assume_init()
}

struct Replay {
    slots: Vec<EeUWideString>,
}

impl Replay {
    fn slot(&mut self, k: &str) -> *mut EeUWideString {
        &mut self.slots[k.parse::<usize>().unwrap()]
    }

    unsafe fn replace(&mut self, k: &str, s: EeUWideString) -> String {
        let p = self.slot(k);
        if !(*p).string.is_null() {
            ee_uwstring_free(p);
        }
        *p = s;
        state(&*p)
    }

    unsafe fn line(&mut self, f: &[&str]) -> String {
        let n = |i: usize| f[i].parse::<c_ulong>().unwrap();
        let tail = match f[0] {
            "seq" | "def" | "leg" => {
                match f[0] {
                    "def" => ee_uwstring_set_default_allocation(n(1)),
                    "leg" => ee_ustring_set_legacy_text_load(f[1] == "1"),
                    _ => {}
                }
                String::new()
            }
            "new" => {
                let s = match f[2] {
                    "d" => fresh(|p| ee_uwstring_init(p)),
                    "n" => fresh(|p| ee_uwstring_init_characters(p, n(3))),
                    "s" => {
                        let t = c_text(f[3]);
                        fresh(|p| ee_uwstring_init_text(p, t.as_ptr()))
                    }
                    _ => {
                        let j = self.slot(f[3]);
                        fresh(|p| ee_uwstring_init_copy(p, j))
                    }
                };
                self.replace(f[1], s)
            }
            "asg" | "add" | "cpy" | "insw" => {
                let (k, j) = (self.slot(f[1]), self.slot(f[2]));
                match f[0] {
                    "asg" => ee_uwstring_assign(k, j),
                    "add" => ee_uwstring_append(k, (*j).string),
                    "cpy" => ee_uwstring_copy(k, j, n(3)),
                    _ => ee_uwstring_insert(k, (*j).string, n(3), f[4] == "1"),
                }
                state(&*k)
            }
            "asgs" | "adds" | "cpys" | "ins" => {
                let (k, t) = (self.slot(f[1]), c_text(f[2]));
                match f[0] {
                    "asgs" => ee_uwstring_assign_text(k, t.as_ptr()),
                    "adds" => ee_uwstring_append(k, t.as_ptr()),
                    "cpys" => ee_uwstring_copy_text(k, t.as_ptr(), n(3)),
                    _ => ee_uwstring_insert(k, t.as_ptr(), n(3), f[4] == "1"),
                }
                state(&*k)
            }
            "plus" => {
                let (i, j) = (self.slot(f[2]), self.slot(f[3]));
                let s = fresh(|p| ee_uwstring_concat(p, (*i).length + (*j).length + 1, i, (*j).string));
                self.replace(f[1], s)
            }
            "pluss" => {
                let (i, t) = (self.slot(f[2]), c_text(f[3]));
                let extra = t.as_bytes().len() as c_ulong;
                let s = fresh(|p| ee_uwstring_concat(p, (*i).length + extra + 1, i, t.as_ptr()));
                self.replace(f[1], s)
            }
            "isub" => {
                let i = self.slot(f[2]);
                let s = fresh(|p| ee_uwstring_indexed_substring(p, i, n(3), n(4)));
                self.replace(f[1], s)
            }
            "at" => format!(" = {}", ee_uwstring_at(self.slot(f[1]), f[2].parse().unwrap()) as i8),
            "size" => {
                let k = self.slot(f[1]);
                format!(" = {} {}", ee_uwstring_bytes_to_save(k), ee_uwstring_export_size(k))
            }
            "find" | "finds" => {
                let k = self.slot(f[1]);
                let r = if f[0] == "finds" {
                    let t = c_text(f[2]);
                    ee_uwstring_find_text(k, t.as_ptr(), n(3))
                } else {
                    ee_uwstring_find(k, self.slot(f[2]), n(3))
                };
                format!(" = {r}")
            }
            "tok" => {
                let (k, j) = (self.slot(f[1]), self.slot(f[2]));
                let mut pairs = vec![0u32; 2 * ((*k).length as usize + 1)];
                let count = ee_uwstring_tokenize(k, j, pairs.as_mut_ptr());
                let mut out = format!("tok {} {} =", f[1], f[2]);
                for t in 0..count as usize {
                    let (first, last) = (c_ulong::from(pairs[2 * t]), c_ulong::from(pairs[2 * t + 1]));
                    // push_back copies the substring (the copy holds the text up to its terminator)
                    let mut s = fresh(|p| ee_uwstring_indexed_substring(p, k, first, last));
                    let mut c = fresh(|p| ee_uwstring_init_copy(p, &s));
                    out += &state(&c);
                    ee_uwstring_free(&mut s);
                    ee_uwstring_free(&mut c);
                }
                return out;
            }
            "imp" => {
                let (k, data) = (self.slot(f[1]), unhex(f[2]));
                let r = ee_uwstring_import(k, data.as_ptr());
                format!(" = {r}{}", state(&*k))
            }
            "exp" => {
                let k = self.slot(f[1]);
                let mut data = vec![0xeeu8; ee_uwstring_export_size(k) as usize + 3];
                ee_uwstring_export(k, data.as_mut_ptr());
                return format!("exp {} = {}", f[1], hex(&data));
            }
            "sum" => {
                // GetChecksum: the entry's name is "UWideString (<text>)", its value the checksum
                let k = self.slot(f[1]);
                let mut name = b"UWideString (".to_vec();
                name.extend_from_slice(CStr::from_ptr((*k).string).to_bytes());
                name.push(b')');
                return format!("sum {} = {} {}", f[1], hex(&name), ee_uwstring_checksum(k));
            }
            "clr" | "real" => {
                let k = self.slot(f[1]);
                if f[0] == "clr" {
                    ee_uwstring_clear(k)
                } else {
                    ee_uwstring_reallocate(k, n(2))
                }
                state(&*k)
            }
            "load" => {
                // UWideString::Load over the driver's scripted file
                let k = self.slot(f[1]);
                let (mode, size, data) = (f[2], n(3), unhex(f[4]));
                let mut result: i64 = if mode == "0" { 5 } else { 0 };
                if result == 0 {
                    let buffer = ee_uwstring_load_buffer(k, size);
                    if mode == "2" {
                        result = 5;
                    } else {
                        let read = (size as usize).min(data.len());
                        std::ptr::copy_nonoverlapping(data.as_ptr(), buffer.cast::<u8>(), read);
                        ee_uwstring_load_end(k, size);
                        if read as c_ulong != size {
                            result = 9;
                        }
                    }
                }
                if result != 0 {
                    ee_uwstring_clear(k);
                }
                format!(" = {result}{}", state(&*k))
            }
            "save" => {
                let k = self.slot(f[1]);
                let mode = f[2];
                let size = ee_uwstring_bytes_to_save(k) - 4;
                let mut written = Vec::new();
                let result = if mode == "1" {
                    5
                } else {
                    written.extend_from_slice(&(size as u32).to_le_bytes());
                    if mode == "2" {
                        5
                    } else {
                        let w = if mode == "3" { size - 1 } else { size };
                        written.extend_from_slice(std::slice::from_raw_parts((*k).string.cast::<u8>(), w as usize));
                        if w < size {
                            9
                        } else {
                            0
                        }
                    }
                };
                format!(" = {result} {}", hex(&written))
            }
            "fmt" => {
                let k = self.slot(f[1]);
                let t = c_text(f[2]);
                let mut text = t.as_bytes().to_vec();
                text.extend_from_slice(format!("|{}", f[3]).as_bytes());
                let size = text.len() as c_ulong + 1;
                let buffer = ee_uwstring_format_buffer(k, size);
                text.push(0);
                std::ptr::copy(text.as_ptr(), buffer.cast::<u8>(), text.len());
                ee_uwstring_format_end(k, buffer, size);
                format!(" = {}{}", size - 1, state(&*k))
            }
            other => panic!("unknown line {other}"),
        };
        let mut h = String::new();
        for (i, x) in f.iter().enumerate() {
            if *x == "=" {
                break;
            }
            if i > 0 {
                h.push(' ');
            }
            h += x;
        }
        h + &tail
    }
}

/// -> (lines, lines that differ, the first difference)
fn replay(golden: &str) -> (usize, usize, Option<String>) {
    // the legacy text flag and the default capacities are process-wide: one replay at a time
    let _held = crate::GOLDEN_LOCK.lock().unwrap_or_else(|e| e.into_inner());
    let (mut failed, mut first) = (0, None);
    let mut count = 0;
    let null = || EeUWideString { string: std::ptr::null_mut(), byte_length: 0, length: 0, capacity: 0 };
    let mut r = Replay { slots: (0..4).map(|_| null()).collect() };
    for line in golden.lines() {
        count += 1;
        let f: Vec<&str> = line.split(' ').collect();
        if f[0] == "seq" {
            for s in r.slots.iter_mut() {
                if !s.string.is_null() {
                    unsafe { ee_uwstring_free(s) };
                }
            }
        }
        let got = unsafe { r.line(&f) };
        if got != line {
            failed += 1;
            if first.is_none() {
                first = Some(format!("line {count}:\n  want {line}\n  got  {got}"));
            }
        }
    }
    for s in r.slots.iter_mut() {
        if !s.string.is_null() {
            unsafe { ee_uwstring_free(s) };
        }
    }
    (count, failed, first)
}

#[test]
fn golden_wide_trace_replays() {
    let (count, failed, first) = replay(include_str!("golden_wide.txt"));
    assert!(count > 1000, "{count} lines");
    assert_eq!(failed, 0, "{failed} of {count} lines differ; {}", first.unwrap_or_default());
}

#[test]
fn golden_wide_trace_replays_long_run() {
    let Ok(path) = std::env::var("EE_UWSTRING_GOLDEN") else { return };
    let text = std::fs::read_to_string(path).unwrap();
    let (count, failed, first) = replay(&text);
    println!("{count} lines, {failed} differ");
    assert_eq!(failed, 0, "{failed} of {count} lines differ; {}", first.unwrap_or_default());
}
