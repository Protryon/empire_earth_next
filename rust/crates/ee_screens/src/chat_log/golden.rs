//! The chat log against the real C++: a scratch driver (`build/sc11/cl_driver.cpp`) ran the unmodified
//! `EEUIEChatLogEvents.cpp` (`EEUIECLEFormManager`) with the real `UWideString` and `GEColor` loads and saves over a
//! fake `FSFile` (reads from a drawn script of values, strings and colours, at times failing or a string short by a
//! byte; writes printed, at times failing): one session of 2,000 cases, each one add, clear, count, load or save,
//! every read and write printed, then the entries (`golden_chat_log.txt`). The replay's fake file plays the printed
//! reads (a string is its size, then its bytes, as `UWideString::Load`) and prints the same lines. The driver relinked
//! over `EEScreens.cpp` and this crate prints the same file, and the same on seeds 2-4 at 20,000 cases.

use std::collections::VecDeque;
use std::sync::Mutex;

use super::*;

const GOLDEN: &[u8] = include_bytes!("../golden_chat_log.txt");

/// A printed read: its kind, result, value (a float's bits, a string's byte count read) and a string's bytes.
type Read = (String, c_long, u32, Vec<u8>);

struct File {
    out: Vec<String>,
    reads: VecDeque<Read>,
    fail_at: i64,
    writes: i64,
}

impl File {
    fn next(&mut self, kind: &str) -> Read {
        let read = self.reads.pop_front().expect("a read in the golden");
        assert_eq!(read.0, kind);
        read
    }
    fn write_result(&mut self) -> c_long {
        let r = if self.writes == self.fail_at { 0xd } else { 0 };
        self.writes += 1;
        r
    }
}

fn s(bytes: &[u8]) -> String {
    String::from_utf8(bytes.to_vec()).unwrap()
}

impl ChatLogFile for File {
    fn read_u32(&mut self) -> (c_long, u32) {
        let (_, r, v, _) = self.next("readu32");
        self.out.push(format!("  readu32 -> {r} {v}"));
        (r, v)
    }
    fn read_string(&mut self) -> (c_long, Vec<u8>) {
        let (r, size) = self.read_u32();
        if r != 0 {
            return (r, Vec::new());
        }
        let (_, r, read, bytes) = self.next("readchars");
        self.out.push(format!("  readchars {size} -> {r} {read} '{}'", s(&bytes)));
        // UWideString::Load: a failed read, or one short of the size (kErrorFailed, 9), empties the string
        if r != 0 {
            return (r, Vec::new());
        }
        if read != size {
            return (9, Vec::new());
        }
        (0, bytes)
    }
    fn read_colour(&mut self) -> (c_long, [f32; 3]) {
        let mut c = [0.0f32; 3];
        for v in c.iter_mut() {
            let (_, r, bits, _) = self.next("readfloat");
            self.out.push(format!("  readfloat -> {r} {bits:08x}"));
            *v = f32::from_bits(bits);
            if r != 0 {
                return (r, c);
            }
        }
        (0, c)
    }
    fn write_u32(&mut self, value: u32) -> c_long {
        let r = self.write_result();
        self.out.push(format!("  writeu32 {value} -> {r}"));
        r
    }
    fn write_string(&mut self, text: &[u8]) -> c_long {
        let r = self.write_u32(text.len() as u32 + 1);
        if r != 0 {
            return r;
        }
        let r = self.write_result();
        self.out.push(format!("  writechars {} '{}' -> {r}", text.len() + 1, s(text)));
        r
    }
    fn write_colour(&mut self, colour: [f32; 3]) -> c_long {
        for c in colour {
            let r = self.write_result();
            self.out.push(format!("  writefloat {:08x} -> {r}", c.to_bits()));
            if r != 0 {
                return r;
            }
        }
        0
    }
}

fn dump(log: &ChatLog) -> String {
    let mut line = format!("  log {}", log.entries.len());
    for e in &log.entries {
        line += &format!(
            " [{:08x} {:08x} {:08x} {} {} '{}' '{}']",
            e.colour[0].to_bits(),
            e.colour[1].to_bits(),
            e.colour[2].to_bits(),
            e.kind,
            e.time,
            s(&e.name),
            s(&e.text)
        );
    }
    line
}

/// A line's quoted texts (`'...'`), in order.
fn quoted(line: &str) -> Vec<String> {
    line.split('\'').skip(1).step_by(2).map(str::to_string).collect()
}

#[test]
fn golden_chat_log() {
    let text = String::from_utf8(GOLDEN.to_vec()).unwrap().replace("\r\n", "\n");
    let mut cases: Vec<Vec<&str>> = Vec::new();
    for line in text.lines() {
        if line.starts_with("case ") {
            cases.push(Vec::new());
        }
        cases.last_mut().unwrap().push(line);
    }
    assert_eq!(cases.len(), 2000);
    let log = Mutex::new(ChatLog::new());
    let (mut loads, mut saves) = (0, 0);
    for (n, case) in cases.iter().enumerate() {
        let head: Vec<&str> = case[0].split(' ').collect();
        let mut out = vec![case[0].to_string()];
        match head[2] {
            "add" => {
                // case N add r g b 'name' 'text' kind time
                let q = quoted(case[0]);
                let tail: Vec<&str> = case[0].rsplit(' ').take(2).collect();
                let bits = |k: usize| f32::from_bits(u32::from_str_radix(head[3 + k], 16).unwrap());
                log.lock().unwrap().add(ChatLogEntry {
                    colour: [bits(0), bits(1), bits(2)],
                    name: q[0].clone().into_bytes(),
                    text: q[1].clone().into_bytes(),
                    kind: tail[1].parse().unwrap(),
                    time: tail[0].parse().unwrap(),
                });
            }
            "clear" => log.lock().unwrap().entries.clear(),
            "count" => assert_eq!(head[4].parse::<usize>().unwrap(), log.lock().unwrap().entries.len()),
            "load" | "save" => {
                let mut file = File {
                    out: Vec::new(),
                    reads: VecDeque::new(),
                    fail_at: if head[2] == "save" { head[4].parse().unwrap() } else { -1 },
                    writes: 0,
                };
                for line in &case[1..] {
                    let t: Vec<&str> = line.split_whitespace().collect();
                    let read = match t[0] {
                        "readu32" => (t[2].parse().unwrap(), t[3].parse().unwrap(), Vec::new()),
                        "readchars" => {
                            (t[3].parse().unwrap(), t[4].parse().unwrap(), quoted(line)[0].clone().into_bytes())
                        }
                        "readfloat" => (t[2].parse().unwrap(), u32::from_str_radix(t[3], 16).unwrap(), Vec::new()),
                        _ => continue,
                    };
                    file.reads.push_back((t[0].to_string(), read.0, read.1, read.2));
                }
                let result = if head[2] == "load" {
                    loads += 1;
                    load(&log, &mut file)
                } else {
                    saves += 1;
                    save(&log, &mut file)
                };
                assert!(file.reads.is_empty(), "case {n}: reads left");
                out.extend(file.out);
                out.push(format!("  result {result}"));
            }
            other => panic!("case {n}: {other}"),
        }
        out.push(dump(&log.lock().unwrap()));
        assert_eq!(out, *case, "case {n}");
    }
    assert!(loads > 200 && saves > 300);
}
