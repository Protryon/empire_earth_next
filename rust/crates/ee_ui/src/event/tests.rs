//! `UIEvent` against the real C++ (`golden_bg.txt`, its `ctlevents`, `formevents` and `init` cases): the scratch
//! driver of `background_table/tests.rs` fed the unmodified `UIEvent.cpp` 100 control-events and 100 form-events
//! files of up to 9 random 116-byte records (negative event ids, owners that are NULL slots or unknown forms, a read
//! failing part way, the count read failing) over a fake controls array and a fake manager whose `GetFormByID` is a
//! trace line, then printed every owner's events; and 100 random `Initialize`/`Uninitialize` sequences over an event
//! whose virtuals answer what the driver says. The replay runs the same through [`create_events`], [`initialize`]
//! and [`uninitialize`]; the driver relinked over the forwarders prints the same file.

use core::ffi::{c_long, c_ulong, c_void};

use super::*;
use crate::background_table::tests::{unhex, FakeFile};

const GOLDEN: &str = include_str!("../golden_bg.txt");

/// The owners: control tables by array index (some NULL) or forms by id; each owner's events in order.
struct Fake {
    file: FakeFile,
    forms: bool,
    /// (id, present) per owner: an array slot (the index is the id) or a form id
    owners: Vec<(c_ulong, bool)>,
    events: Vec<Vec<(c_ulong, c_ulong)>>,
}

impl EventsHost for Fake {
    fn open(&mut self) {
        self.file.open()
    }
    fn read_count(&mut self, count: &mut c_ulong) -> c_long {
        self.file.read_count(count)
    }
    fn read_record(&mut self, record: &mut DbEvent) -> c_long {
        let mut b = [0u8; 116];
        let r = self.file.read(&mut b);
        if r == 0 {
            // SAFETY: the record is plain bytes of exactly its size
            unsafe { core::ptr::copy_nonoverlapping(b.as_ptr(), (record as *mut DbEvent).cast::<u8>(), 116) };
        }
        r
    }
    fn owner(&mut self, owner: c_ulong) -> *mut c_void {
        if self.forms {
            self.file.log.push(format!("  form? {owner}"));
            match self.owners.iter().position(|&(id, _)| id == owner) {
                Some(i) => (i + 1) as *mut c_void,
                None => core::ptr::null_mut(),
            }
        } else if self.owners[owner as usize].1 {
            (owner as usize + 1) as *mut c_void
        } else {
            core::ptr::null_mut()
        }
    }
    fn add(&mut self, owner: *mut c_void, event_id: c_ulong, event_type: c_ulong) {
        self.events[owner as usize - 1].push((event_id, event_type));
    }
    fn close(&mut self) {
        self.file.close()
    }
}

struct Count {
    count: c_ulong,
    answer: c_long,
    log: Vec<String>,
}

impl EventCountHost for Count {
    fn count(&self) -> c_ulong {
        self.count
    }
    fn set_count(&mut self, count: c_ulong) {
        self.count = count;
    }
    fn initialize_event(&mut self) -> c_long {
        self.log.push(format!("  initevent = {}", self.answer));
        self.answer
    }
    fn uninitialize_event(&mut self) -> c_long {
        self.log.push(format!("  uninitevent = {}", self.answer));
        self.answer
    }
}

#[test]
fn replays_the_cpp_golden() {
    let lines: Vec<&str> = GOLDEN.lines().collect();
    let (mut events_cases, mut init_cases) = (0, 0);
    let mut i = 0;
    while i < lines.len() {
        let t: Vec<&str> = lines[i].split(' ').collect();
        let start = i;
        let mut out = vec![lines[i].to_string()];
        if t[0] == "ctlevents" || t[0] == "formevents" {
            events_cases += 1;
            let forms = t[0] == "formevents";
            let at = t.iter().position(|w| *w == "count").unwrap();
            let owners: Vec<(c_ulong, bool)> = t[3..at]
                .iter()
                .enumerate()
                .map(|(n, w)| if forms { (w[1..].parse().unwrap(), true) } else { (n as c_ulong, *w == "t") })
                .collect();
            let count: u32 = t[at + 1].parse().unwrap();
            let fail: i64 = t[at + 3].parse().unwrap();
            i += 1;
            let mut records = Vec::new();
            while lines[i].starts_with("  record ") {
                out.push(lines[i].to_string());
                records.push(unhex(&lines[i][9..]));
                i += 1;
            }
            let n = owners.len();
            let mut fake =
                Fake { file: FakeFile::new(count, &records, fail), forms, owners, events: vec![Vec::new(); n] };
            let r = create_events(&mut fake);
            out.append(&mut fake.file.log);
            out.push(format!("  = {r}"));
            for (k, (_, present)) in fake.owners.iter().enumerate() {
                if *present {
                    let list: String = fake.events[k].iter().map(|(id, ty)| format!(" {id}:{ty}")).collect();
                    out.push(format!("  owner {k}{list}"));
                }
            }
        } else if t[0] == "init" {
            init_cases += 1;
            // the operations and the virtuals' answers, from the result lines
            let mut ops = Vec::new();
            i += 1;
            while i < lines.len() && lines[i].starts_with("  ") {
                let w: Vec<&str> = lines[i].split(' ').collect();
                if w[2] == "init" || w[2] == "uninit" {
                    ops.push((w[2] == "init", w[3].parse::<c_long>().unwrap()));
                }
                i += 1;
            }
            let mut c = Count { count: 0, answer: 0, log: Vec::new() };
            for (init, answer) in ops {
                c.answer = answer;
                let r = if init { initialize(&mut c) } else { uninitialize(&mut c) };
                out.append(&mut c.log);
                // %lu of the 32-bit unsigned long the golden was printed with (a wrapped count is 2^32 - 1)
                #[allow(clippy::unnecessary_cast)] // u32 on Windows, u64 on LP64
                out.push(format!("  {} {answer} = {r} count {}", if init { "init" } else { "uninit" }, c.count as u32));
            }
        } else {
            i += 1;
            continue;
        }
        for (k, want) in out.iter().enumerate() {
            assert_eq!(want, lines[start + k], "{} line {k}", lines[start]);
        }
        i = start + out.len();
    }
    assert_eq!((events_cases, init_cases), (200, 100));
}

#[test]
fn uninitialize_wraps_a_count_already_at_zero() {
    let mut c = Count { count: 0, answer: 0, log: Vec::new() };
    assert_eq!(uninitialize(&mut c), 0);
    assert_eq!(c.count, c_ulong::MAX);
    assert!(c.log.is_empty(), "the count was not 1: no UninitializeEvent");
    let mut c = Count { count: 0, answer: 9, log: Vec::new() };
    assert_eq!(initialize(&mut c), 9);
    assert_eq!(c.count, 0, "a failed InitializeEvent counts no user");
}
