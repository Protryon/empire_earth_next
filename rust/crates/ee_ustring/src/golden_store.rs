//! Replays the original UString's storage trace (`tests/string/us_store_golden.cpp`, format in its header): every
//! call again on Rust records through the C interface the class forwards to, each line regenerated. `Load`, `Save`,
//! the tokenizers' vectors and `sprintf` are replayed as their C++ forwarders (`UString.cpp`) make the calls.
//! `golden_store.txt` holds the first 60 sequences; `EE_USTRING_STORE_GOLDEN=<file>` replays a longer run.

use super::*;
use std::ffi::CString;
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

fn state(s: &EeUString) -> String {
    let bytes = unsafe { std::slice::from_raw_parts(s.string.cast::<u8>(), s.capacity as usize) };
    format!(" = {} {} {}", s.length, s.capacity, hex(bytes))
}

fn c_text(s: &str) -> CString {
    CString::new(unhex(s)).unwrap()
}

unsafe fn fresh(f: impl FnOnce(*mut EeUString)) -> EeUString {
    let mut s = MaybeUninit::<EeUString>::uninit();
    f(s.as_mut_ptr());
    s.assume_init()
}

struct Replay {
    slots: Vec<EeUString>,
}

impl Replay {
    fn slot(&mut self, k: &str) -> *mut EeUString {
        &mut self.slots[k.parse::<usize>().unwrap()]
    }

    /// Slot `k` becomes `s` (the old one freed): its state.
    unsafe fn replace(&mut self, k: &str, s: EeUString) -> String {
        let p = self.slot(k);
        if !(*p).string.is_null() {
            ee_ustring_free(p);
        }
        *p = s;
        state(&*p)
    }

    unsafe fn line(&mut self, f: &[&str]) -> String {
        let n = |i: usize| f[i].parse::<c_ulong>().unwrap();
        let tail = match f[0] {
            "seq" | "def" | "leg" => {
                match f[0] {
                    "def" => ee_ustring_set_default_allocation(n(1)),
                    "leg" => ee_ustring_set_legacy_text_load(f[1] == "1"),
                    _ => {}
                }
                String::new()
            }
            "new" => {
                let s = match f[2] {
                    "d" => fresh(|p| ee_ustring_init(p)),
                    "n" => fresh(|p| ee_ustring_init_capacity(p, n(3))),
                    "s" => {
                        let t = c_text(f[3]);
                        fresh(|p| ee_ustring_init_text(p, t.as_ptr()))
                    }
                    _ => {
                        let j = self.slot(f[3]);
                        fresh(|p| ee_ustring_init_copy(p, j))
                    }
                };
                self.replace(f[1], s)
            }
            "asg" | "add" => {
                let (k, j) = (self.slot(f[1]), self.slot(f[2]));
                if f[0] == "asg" {
                    ee_ustring_assign(k, j)
                } else {
                    ee_ustring_append(k, j)
                }
                state(&*k)
            }
            "asgs" | "adds" => {
                let (k, t) = (self.slot(f[1]), c_text(f[2]));
                if f[0] == "asgs" {
                    ee_ustring_assign_text(k, t.as_ptr())
                } else {
                    ee_ustring_append_text(k, t.as_ptr())
                }
                state(&*k)
            }
            "plus" => {
                let (i, j) = (self.slot(f[2]), self.slot(f[3]));
                let s = fresh(|p| ee_ustring_concat(p, i, j));
                self.replace(f[1], s)
            }
            "pluss" => {
                let (i, t) = (self.slot(f[2]), c_text(f[3]));
                let s = fresh(|p| ee_ustring_concat_text(p, i, t.as_ptr()));
                self.replace(f[1], s)
            }
            "sub" | "isub" => {
                let i = self.slot(f[2]);
                let s = if f[0] == "sub" {
                    fresh(|p| ee_ustring_substring(p, i, n(3), n(4)))
                } else {
                    fresh(|p| ee_ustring_indexed_substring(p, i, n(3), n(4)))
                };
                self.replace(f[1], s)
            }
            "copy" => {
                let (k, j) = (self.slot(f[1]), self.slot(f[2]));
                ee_ustring_copy(k, j, n(3), n(4), n(5), f[6] == "1");
                state(&*k)
            }
            "copys" => {
                let (k, t) = (self.slot(f[1]), c_text(f[2]));
                ee_ustring_copy_text(k, t.as_ptr(), n(3), n(4), n(5), f[6] == "1");
                state(&*k)
            }
            "compact" => {
                let k = self.slot(f[1]);
                ee_ustring_compact(k);
                state(&*k)
            }
            "at" => format!(" = {}", ee_ustring_at(self.slot(f[1]), f[2].parse().unwrap()) as i8),
            "size" => {
                let k = self.slot(f[1]);
                format!(" = {} {}", ee_ustring_size_of(k), ee_ustring_bytes_to_save(k))
            }
            "find" | "back" | "finds" | "backs" => {
                let k = self.slot(f[1]);
                let back = f[0].starts_with("back");
                let r = if f[0].ends_with('s') {
                    let t = c_text(f[2]);
                    ee_ustring_find(k, t.as_ptr(), t.as_bytes().len() as c_ulong, n(3), back)
                } else {
                    let j = self.slot(f[2]);
                    ee_ustring_find(k, (*j).string, (*j).length, n(3), back)
                };
                format!(" = {r}")
            }
            "cmp" | "cmps" => {
                let a = (*self.slot(f[1])).string;
                let t;
                let b = if f[0] == "cmps" {
                    t = c_text(f[2]);
                    t.as_ptr()
                } else {
                    (*self.slot(f[2])).string
                };
                let c = ee_ustring_compare(a, b);
                let bits =
                    [c == 0, c != 0, c < 0, c <= 0, c > 0, c >= 0, c == 0, ee_ustring_compare_count(a, b, n(3)) == 0];
                format!(" = {}", bits.iter().map(|&x| if x { '1' } else { '0' }).collect::<String>())
            }
            "tok" | "nest" => {
                let (k, j) = (self.slot(f[1]), self.slot(f[2]));
                let mut brackets = if f[0] == "nest" {
                    let t = c_text(f[3]);
                    Some(fresh(|p| ee_ustring_init_text(p, t.as_ptr())))
                } else {
                    None
                };
                let mut pairs = vec![0u32; 2 * ((*k).length as usize + 1)];
                let b = brackets.as_ref().map_or(std::ptr::null(), |b| b as *const EeUString);
                let count = ee_ustring_tokenize(k, j, b, pairs.as_mut_ptr());
                if let Some(b) = brackets.as_mut() {
                    ee_ustring_free(b);
                }
                let mut out = String::new();
                for t in 0..count as usize {
                    let (first, last) = (c_ulong::from(pairs[2 * t]), c_ulong::from(pairs[2 * t + 1]));
                    let mut s = fresh(|p| ee_ustring_indexed_substring(p, k, first, last));
                    out += &state(&s);
                    ee_ustring_free(&mut s);
                }
                // the head ends at the first "=" (a token's state has its own)
                let mut h = f[..if f[0] == "nest" { 4 } else { 3 }].join(" ");
                h += " =";
                return h + &out;
            }
            "load" => {
                // UString::Load over the driver's scripted file
                let k = self.slot(f[1]);
                let (mode, length, data) = (f[2], n(3), unhex(f[4]));
                let mut result: i64 = if mode == "0" { 5 } else { 0 };
                if result == 0 && length != 0 {
                    let buffer = ee_ustring_load_buffer(k, length);
                    let read;
                    if mode == "2" {
                        read = 0;
                        result = 5;
                    } else {
                        read = (length as usize).min(data.len());
                        std::ptr::copy_nonoverlapping(data.as_ptr(), buffer.cast::<u8>(), read);
                    }
                    if result == 0 && read as c_ulong != length {
                        result = 9;
                    }
                }
                ee_ustring_load_end(k, result == 0 && length != 0, length);
                format!(" = {result}{}", state(&*k))
            }
            "save" => {
                // UString::Save over the driver's scripted file
                let k = self.slot(f[1]);
                let mode = f[2];
                let length = (*k).length + 1;
                let mut written = Vec::new();
                let result = if mode == "1" {
                    5
                } else {
                    written.extend_from_slice(&(length as u32).to_le_bytes());
                    if mode == "2" {
                        5
                    } else {
                        let w = if mode == "3" { length - 1 } else { length };
                        written.extend_from_slice(std::slice::from_raw_parts((*k).string.cast::<u8>(), w as usize));
                        if w < length {
                            9
                        } else {
                            0
                        }
                    }
                };
                format!(" = {result} {}", hex(&written))
            }
            "fmt" => {
                // sprintf(s, "%s|%d", text, number): sized, formatted into the buffer it gives, measured
                let k = self.slot(f[1]);
                let t = c_text(f[2]);
                let mut text = t.as_bytes().to_vec();
                text.extend_from_slice(format!("|{}", f[3]).as_bytes());
                let size = text.len() as c_ulong + 1;
                let buffer = ee_ustring_format_buffer(k, size);
                text.push(0);
                std::ptr::copy(text.as_ptr(), buffer.cast::<u8>(), text.len());
                ee_ustring_format_end(k, buffer, size);
                format!(" = {}{}", size - 1, state(&*k))
            }
            other => panic!("unknown line {other}"),
        };
        // the head is everything before the first " = " (a state's own "=" comes after it)
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
    let null = || EeUString { string: std::ptr::null_mut(), length: 0, capacity: 0 };
    let mut r = Replay { slots: (0..4).map(|_| null()).collect() };
    for line in golden.lines() {
        count += 1;
        let f: Vec<&str> = line.split(' ').collect();
        if f[0] == "seq" {
            for s in r.slots.iter_mut() {
                if !s.string.is_null() {
                    unsafe { ee_ustring_free(s) };
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
            unsafe { ee_ustring_free(s) };
        }
    }
    (count, failed, first)
}

#[test]
fn golden_store_trace_replays() {
    let (count, failed, first) = replay(include_str!("golden_store.txt"));
    assert!(count > 1000, "{count} lines");
    assert_eq!(failed, 0, "{failed} of {count} lines differ; {}", first.unwrap_or_default());
}

#[test]
fn golden_store_trace_replays_long_run() {
    let Ok(path) = std::env::var("EE_USTRING_STORE_GOLDEN") else { return };
    let text = std::fs::read_to_string(path).unwrap();
    let (count, failed, first) = replay(&text);
    println!("{count} lines, {failed} differ");
    assert_eq!(failed, 0, "{failed} of {count} lines differ; {}", first.unwrap_or_default());
}
