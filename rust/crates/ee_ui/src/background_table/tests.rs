//! `UIBackgroundTable::Create` against the real C++ (`golden_bg.txt`, its `bgtable` cases): a scratch driver
//! (`build/ui_bg_golden`) fed the unmodified `UIBackgroundTable.cpp` 100 files of up to 8 random 308-byte records
//! (named and nameless, full-length and UTF-8 texture names, padding bytes after a name's terminator, colours past
//! 255, priorities below 1, a record read failing part way, the count read failing) through a fake file whose reads
//! are trace lines, then printed every slot's getters. The replay reads the same records through [`create`]; the
//! driver relinked over the forwarders prints the same file, and the same on three more seeds at ten times the size.

use super::*;

const GOLDEN: &str = include_str!("../golden_bg.txt");

pub(crate) fn unhex(s: &str) -> Vec<u8> {
    if s == "-" {
        return Vec::new();
    }
    (0..s.len()).step_by(2).map(|i| u8::from_str_radix(&s[i..i + 2], 16).unwrap()).collect()
}

pub(crate) fn hex(s: &[u8]) -> String {
    let t: String = s.iter().map(|b| format!("{b:02x}")).collect();
    if t.is_empty() {
        "-".into()
    } else {
        t
    }
}

fn h(v: f32) -> String {
    format!("{:08x}", v.to_bits())
}

/// The driver's file: the count and the records, one read (`fail`, 0 the count) failing; every call a line.
pub(crate) struct FakeFile {
    pub data: Vec<u8>,
    pub at: usize,
    pub reads: i64,
    pub fail: i64,
    pub log: Vec<String>,
}

impl FakeFile {
    pub fn new(count: u32, records: &[Vec<u8>], fail: i64) -> FakeFile {
        let mut data = count.to_le_bytes().to_vec();
        for r in records {
            data.extend_from_slice(r);
        }
        FakeFile { data, at: 0, reads: 0, fail, log: Vec::new() }
    }
    pub fn open(&mut self) {
        self.log.push("  open 1".into());
    }
    pub fn close(&mut self) {
        self.log.push("  close".into());
    }
    pub fn read(&mut self, out: &mut [u8]) -> c_long {
        let n = self.reads;
        self.reads += 1;
        if n == self.fail || self.at + out.len() > self.data.len() {
            self.log.push(format!("  read {} fails", out.len()));
            return 9;
        }
        out.copy_from_slice(&self.data[self.at..self.at + out.len()]);
        self.at += out.len();
        0
    }
    pub fn read_count(&mut self, count: &mut c_ulong) -> c_long {
        let mut b = [0u8; 4];
        let r = self.read(&mut b);
        if r == 0 {
            *count = u32::from_le_bytes(b) as c_ulong;
        }
        r
    }
}

/// The record's bytes as a `DbBackground`.
fn record_of(bytes: &[u8]) -> DbBackground {
    assert_eq!(bytes.len(), core::mem::size_of::<DbBackground>());
    let mut r = DbBackground::zeroed();
    // SAFETY: the record is plain bytes of exactly its size
    unsafe { core::ptr::copy_nonoverlapping(bytes.as_ptr(), (&mut r as *mut DbBackground).cast::<u8>(), bytes.len()) };
    r
}

struct Fake {
    file: FakeFile,
    slots: Vec<Option<(BackgroundTable, Vec<u8>)>>,
}

impl BackgroundTablesHost for Fake {
    fn open(&mut self) {
        self.file.open()
    }
    fn read_count(&mut self, count: &mut c_ulong) -> c_long {
        self.file.read_count(count)
    }
    fn resize(&mut self, count: c_ulong) {
        self.slots.resize(count as usize, None);
    }
    fn read_record(&mut self, record: &mut DbBackground) -> c_long {
        let mut b = [0u8; 308];
        let r = self.file.read(&mut b);
        if r == 0 {
            *record = record_of(&b);
        }
        r
    }
    fn set(&mut self, index: c_ulong, record: Option<&DbBackground>) {
        self.slots[index as usize] = record.map(|r| {
            let name = r.texture_file.iter().take_while(|&&b| b != 0).copied().collect();
            (from_record(r), name)
        });
    }
    fn close(&mut self) {
        self.file.close()
    }
}

fn slot_line(i: usize, t: &BackgroundTable, file: &[u8]) -> String {
    let f = &t.fields;
    let l = &t.layout;
    let r = &t.texture_rect;
    format!(
        "  slot {i} id {} bg {} form {} file {} rect {} {} {} {} type {} layout {}:{},{}:{},{}:{},{}:{} vis {} \
         alpha {} cols {} rows {} tile {} {} attr {}{}{} op {} color {} {} {} prio {}",
        f.id,
        f.background_id,
        f.form_id,
        hex(file),
        h(r.left),
        h(r.top),
        h(r.right),
        h(r.bottom),
        f.background_type,
        l.top_type,
        h(l.top_offset),
        l.bottom_type,
        h(l.bottom_offset),
        l.left_type,
        h(l.left_offset),
        l.right_type,
        h(l.right_offset),
        f.visible as u8,
        f.has_alpha as u8,
        f.num_cols,
        f.num_rows,
        h(f.tile_width_percent),
        h(f.tile_height_percent),
        f.attribute1 as u8,
        f.attribute2 as u8,
        f.attribute3 as u8,
        h(f.opacity),
        h(t.color.red),
        h(t.color.green),
        h(t.color.blue),
        f.draw_priority
    )
}

#[test]
fn replays_the_cpp_golden() {
    let lines: Vec<&str> = GOLDEN.lines().collect();
    let mut i = 0;
    let mut cases = 0;
    while i < lines.len() {
        let t: Vec<&str> = lines[i].split(' ').collect();
        if t[0] != "bgtable" {
            i += 1;
            continue;
        }
        cases += 1;
        let count: u32 = t[3].parse().unwrap();
        let fail: i64 = t[5].parse().unwrap();
        let mut out = vec![lines[i].to_string()];
        i += 1;
        let mut records = Vec::new();
        while lines[i].starts_with("  record ") {
            out.push(lines[i].to_string());
            records.push(unhex(&lines[i][9..]));
            i += 1;
        }
        let mut fake = Fake { file: FakeFile::new(count, &records, fail), slots: Vec::new() };
        let r = create(&mut fake);
        out.append(&mut fake.file.log);
        out.push(format!("  = {r} size {}", fake.slots.len()));
        for (n, s) in fake.slots.iter().enumerate() {
            match s {
                None => out.push(format!("  slot {n} null")),
                Some((t, file)) => out.push(slot_line(n, t, file)),
            }
        }
        for (k, want) in out.iter().enumerate() {
            assert_eq!(want, lines[i - records.len() - 1 + k], "case {} line {k}", cases - 1);
        }
        i += out.len() - records.len() - 1;
    }
    assert_eq!(cases, 100);
}

#[test]
fn a_nameless_record_leaves_its_slot_empty_and_priorities_start_at_one() {
    let mut named = DbBackground::zeroed();
    named.name[0] = b'x';
    named.draw_priority = -4;
    named.blue = 255;
    named.texture_left = 3;
    named.texture_bottom = -2;
    let bytes = |r: &DbBackground| {
        // SAFETY: plain bytes of the record
        unsafe { core::slice::from_raw_parts((r as *const DbBackground).cast::<u8>(), 308) }.to_vec()
    };
    let nameless = DbBackground::zeroed();
    let mut fake = Fake { file: FakeFile::new(2, &[bytes(&named), bytes(&nameless)], -1), slots: Vec::new() };
    assert_eq!(create(&mut fake), 0);
    let t = &fake.slots[0].as_ref().unwrap().0;
    assert_eq!(t.fields.draw_priority, 1);
    assert_eq!(t.color.blue, 1.0);
    assert_eq!((t.texture_rect.left, t.texture_rect.bottom), (3.0, -2.0));
    assert!(fake.slots[1].is_none());
}

#[test]
fn a_failed_read_keeps_the_previous_record() {
    let mut named = DbBackground::zeroed();
    named.name[0] = b'x';
    named.id = 7;
    let b = unsafe { core::slice::from_raw_parts((&named as *const DbBackground).cast::<u8>(), 308) }.to_vec();
    // the second record's read (the third read) fails: its slot repeats the first
    let mut fake = Fake { file: FakeFile::new(2, &[b.clone(), b], 2), slots: Vec::new() };
    assert_eq!(create(&mut fake), 9);
    assert_eq!(fake.slots[1].as_ref().unwrap().0.fields.id, 7);
}
