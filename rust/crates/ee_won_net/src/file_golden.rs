//! `golden_file.txt`: the C++ FileReader's and FileWriter's answers to `tests/won/won_file_test.cpp`'s script,
//! recorded over the original classes before the port. This test reads the script out of that source and replays it
//! through `file` over real files in a temporary directory; the string reads and writes are the C++ glue's loops
//! (FileReader.cpp, FileReaderWideStrings.cpp, FileWriter.cpp), modelled here over the same primitives.

use std::ffi::{c_ulong, CString};

use super::*;

const GOLDEN: &str = include_str!("golden_file.txt");
const SOURCE: &str = include_str!("../../../../tests/won/won_file_test.cpp");

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

fn units(text: &[u16]) -> String {
    if text.is_empty() {
        return "-".into();
    }
    text.iter().map(|u| format!("{u:04x}")).collect::<Vec<_>>().join(".")
}

fn number(arg: &str) -> i64 {
    if let Some(h) = arg.strip_prefix("0x") {
        i64::from_str_radix(h, 16).unwrap()
    } else {
        arg.parse().unwrap()
    }
}

struct Run {
    dir: std::path::PathBuf,
    reader: EeWonFileReader,
    writer: EeWonFileWriter,
    memory: Vec<u8>,
}

fn text(status: i32) -> String {
    status_text(status).to_str().unwrap().to_string()
}

impl Run {
    fn path(&self, name: &str) -> CString {
        let p = match name {
            "-" => return CString::new("").unwrap(),
            "N" => self.dir.join("won_file_missing_dir").join("none.bin"),
            n if n.starts_with("N/") => self.dir.join("won_file_missing_dir").join(&n[2..]),
            n => self.dir.join(format!("won_file_{n}.bin")),
        };
        CString::new(p.to_str().unwrap()).unwrap()
    }

    // ReadRawString / ReadRawWString: the output is erased, then filled unit by unit; a failure keeps the part read
    fn raw_string(&mut self, count: i32, out: &mut Vec<u8>) -> i32 {
        out.clear();
        for _ in 0..count {
            let (b, s) = self.reader.read_u8();
            if s != WON_FILE_OK {
                return s;
            }
            out.push(b);
        }
        WON_FILE_OK
    }

    fn raw_wstring(&mut self, count: i32, out: &mut Vec<u16>) -> i32 {
        out.clear();
        for _ in 0..count {
            let (u, s) = self.reader.read_u16();
            if s != WON_FILE_OK {
                return s;
            }
            out.push(u);
        }
        WON_FILE_OK
    }

    fn step(&mut self, op: &str, arg: &str, arg2: &str) -> Result<String, i32> {
        let ok = |s: i32| if s == WON_FILE_OK { Ok("ok".to_string()) } else { Err(s) };
        let b = |v: bool| Ok(if v { "true" } else { "false" }.to_string());
        let wide = |a: &str| a.split('.').map(|u| u16::from_str_radix(u, 16).unwrap()).collect::<Vec<u16>>();
        let le = |u: &[u16]| u.iter().flat_map(|x| x.to_le_bytes()).collect::<Vec<u8>>();
        let s = |a: &str| if a == "-" { String::new() } else { a.to_string() };
        match op {
            "dump" => Ok(match std::fs::read(self.path(arg).to_str().unwrap()) {
                Ok(d) => hex(&d),
                Err(_) => "none".into(),
            }),
            "w.open" => {
                let p = self.path(arg);
                // the read-only retry is the C++'s (WONFile); the files here are never read-only
                b(unsafe { self.writer.open(p.as_ptr()) })
            }
            "w.close" => {
                self.writer.close();
                Ok("ok".into())
            }
            "w.pos" => Ok(self.writer.pos().to_string()),
            "w.setpos" => ok(self.writer.set_pos(number(arg) as c_ulong)),
            "w.byte" => ok(self.writer.write(&[number(arg) as u8])),
            "w.short" => ok(self.writer.write(&(number(arg) as u16).to_ne_bytes())),
            "w.long" => ok(self.writer.write(&(number(arg) as i32).to_ne_bytes())),
            "w.length" => ok(self.writer.write_length(number(arg) as c_ulong, number(arg2) as u8)),
            "w.string" => {
                let t = s(arg);
                ok(self.writer.write_length(t.len() as c_ulong, number(arg2) as u8))?;
                ok(self.writer.write(t.as_bytes()))
            }
            "w.rawstring" => ok(self.writer.write(s(arg).as_bytes())),
            "w.wstring" => {
                let u = wide(arg);
                ok(self.writer.write_length(u.len() as c_ulong, number(arg2) as u8))?;
                ok(self.writer.write(&le(&u)))
            }
            "w.rawwstring" => ok(self.writer.write(&le(&wide(arg)))),
            "r.open" => {
                let p = self.path(arg);
                b(unsafe { self.reader.open(p.as_ptr()) })
            }
            "r.opentext" => {
                let p = self.path(arg);
                b(unsafe { self.reader.open_text(p.as_ptr()) })
            }
            "r.mem" => {
                self.memory = if arg == "-" {
                    Vec::new()
                } else {
                    (0..arg.len() / 2).map(|i| u8::from_str_radix(&arg[2 * i..2 * i + 2], 16).unwrap()).collect()
                };
                // an empty buffer is still a buffer (the C++ passes "")
                static EMPTY: [u8; 1] = [0];
                let p = if self.memory.is_empty() { EMPTY.as_ptr() } else { self.memory.as_ptr() };
                self.reader.open_memory(p, self.memory.len() as c_ulong);
                Ok("ok".into())
            }
            "r.close" => {
                self.reader.close();
                Ok("ok".into())
            }
            "r.isopen" => b(self.reader.is_open()),
            "r.pos" => Ok(self.reader.pos().to_string()),
            "r.length" => {
                let (l, st) = self.reader.length();
                ok(st)?;
                Ok(l.to_string())
            }
            "r.avail" => {
                let (l, st) = self.reader.length();
                ok(st)?;
                Ok(l.wrapping_sub(self.reader.pos()).to_string())
            }
            "r.eof" => b(self.reader.end_of_file()),
            "r.tryb" => Ok(self.reader.try_read_byte().to_string()),
            "r.setpos" => ok(self.reader.set_pos(number(arg) as c_ulong)),
            "r.skip" => ok(self.reader.skip(number(arg) as c_ulong)),
            "r.rewind" => {
                self.reader.rewind();
                Ok("ok".into())
            }
            "r.intomem" => ok(self.reader.read_into_memory()),
            "r.maxbytes" => {
                let mut data = [0u8; 256];
                let (count, st) = unsafe { self.reader.read_max(data.as_mut_ptr(), number(arg) as i32) };
                ok(st)?;
                Ok(format!("{count} {}", hex(&data[..count.max(0) as usize])))
            }
            "r.byte" => self.reader.read_u8().pipe(|(v, st)| ok(st).map(|_| v.to_string())),
            "r.short" => self.reader.read_u16().pipe(|(v, st)| ok(st).map(|_| v.to_string())),
            "r.long" => self.reader.read_i32().pipe(|(v, st)| ok(st).map(|_| v.to_string())),
            "r.rlen" => self.reader.read_length(number(arg) as i32).pipe(|(v, st)| ok(st).map(|_| v.to_string())),
            "r.rawstring" | "r.string" | "r.rawwstring" | "r.wstring" => {
                let mut narrow = b"unset".to_vec();
                let mut wide_out: Vec<u16> = "unset".encode_utf16().collect();
                let mut st = WON_FILE_OK;
                let mut count = number(arg) as i32;
                if op == "r.string" || op == "r.wstring" {
                    let (l, s2) = self.reader.read_length(count);
                    st = s2;
                    count = l;
                }
                if st == WON_FILE_OK {
                    st = if op == "r.rawwstring" || op == "r.wstring" {
                        self.raw_wstring(count, &mut wide_out)
                    } else {
                        self.raw_string(count, &mut narrow)
                    };
                }
                if st != WON_FILE_OK {
                    return Ok(format!("throw {} / {} / {}", text(st), hex(&narrow), units(&wide_out)));
                }
                Ok(if op == "r.rawwstring" || op == "r.wstring" { units(&wide_out) } else { hex(&narrow) })
            }
            _ => Ok("unknown op".into()),
        }
    }
}

trait Pipe: Sized {
    fn pipe<R>(self, f: impl FnOnce(Self) -> R) -> R {
        f(self)
    }
}
impl<T> Pipe for T {}

#[test]
fn file_golden() {
    let dir = std::env::temp_dir().join(format!("ee_won_file_golden_{}", std::process::id()));
    std::fs::create_dir_all(&dir).unwrap();
    let mut run =
        Run { dir: dir.clone(), reader: EeWonFileReader::new(), writer: EeWonFileWriter::new(), memory: Vec::new() };
    let mut got = Vec::new();
    for step in script() {
        let mut parts = step.splitn(3, ' ');
        let op = parts.next().unwrap();
        let arg = parts.next().unwrap_or("");
        let arg2 = parts.next().unwrap_or("");
        let result = match run.step(op, arg, arg2) {
            Ok(r) => r,
            Err(st) => format!("throw {}", text(st)),
        };
        got.push(format!("{step} -> {result}"));
    }
    run.reader.close();
    run.writer.close();
    let _ = std::fs::remove_dir_all(&dir);
    let want: Vec<&str> = GOLDEN.lines().collect();
    assert_eq!(got.len(), want.len());
    for (g, w) in got.iter().zip(want) {
        assert_eq!(g, w);
    }
}

#[test]
fn status_texts_have_no_nul() {
    for s in 0..8 {
        assert!(!status_text(s).to_bytes().contains(&0));
    }
    assert_eq!(text(WON_FILE_NULL), "NULL File.");
}

#[test]
fn memory_reads_without_a_file() {
    let data = [1u8, 2, 3, 4, 5, 6];
    let mut r = EeWonFileReader::new();
    r.open_memory(data.as_ptr(), 6);
    assert_eq!(r.read_i32(), (0x0403_0201, WON_FILE_OK));
    assert_eq!(r.read_u16(), (0x0605, WON_FILE_OK));
    assert!(r.end_of_file());
    assert_eq!(r.read_u8().1, WON_FILE_PAST_END_MEM);
    assert_eq!(r.set_pos(6), WON_FILE_SETPOS_MEM);
    r.close();
    assert!(!r.is_open());
    assert_eq!(r.read_u8().1, WON_FILE_NULL);
}
