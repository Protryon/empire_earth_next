//! The font table against the real C++ (`golden_font.txt`): a scratch driver built over the original
//! `UIFontTable.cpp` (a fake file of random font records, a fake string table, a fake `GEFont` whose construction is
//! a trace line) printed 40 tables' entries and 12 random `CreateClosestFont`, `GetFont` and `FindOrCreateFont`
//! calls each, then `DeleteAll`. The replay below is the same world over this module; the same driver over the
//! forwarders prints the same file.

use super::*;

fn record(name: &[u8], bits: [u8; 5]) -> DbFont {
    let mut r = DbFont {
        font_name: [0; 0x6c],
        string_id: 7,
        size: 12,
        quality: 3,
        bold: bits[0],
        italic: bits[1],
        underline: bits[2],
        style10: bits[3],
        style08: bits[4],
        pad: [0; 3],
    };
    for (d, s) in r.font_name.iter_mut().zip(name) {
        *d = *s as c_char;
    }
    r
}

#[test]
fn records_become_entries() {
    assert!(!entry(&record(b"", [1, 1, 1, 1, 1])).present);
    let e = entry(&record(b"Arial", [1, 0, 1, 0, 1]));
    assert_eq!(e, FontEntry { present: true, string_id: 7, style: 8 | 4 | 1, quality: 3, size: 12 });
    assert_eq!(entry(&record(b"A", [0, 1, 0, 1, 0])).style, 0x12);
    // any non-zero byte is true, as the C++ bool read it
    assert_eq!(entry(&record(b"A", [2, 0, 0, 0, 0])).style, 1);
}

#[test]
fn closest_size_truncates() {
    assert_eq!(closest_size(0), 1);
    assert_eq!(closest_size(1), 1);
    assert_eq!(closest_size(16), 13);
    assert_eq!(closest_size(15), 12);
    assert_eq!(closest_size(96), 73);
    assert_eq!(closest_size(c_ulong::MAX), (c_ulong::MAX as i64 * 72 / 96) as c_long + 1);
}

struct Fonts {
    list: Vec<(u32, u32)>, // (key, references)
    want: u32,
    log: Vec<String>,
}

impl FontHost for Fonts {
    fn count(&self) -> usize {
        self.list.len()
    }
    fn matches(&mut self, i: usize) -> bool {
        self.log.push(format!("compare {i}"));
        self.list[i].0 == self.want
    }
    fn take(&mut self, i: usize) -> usize {
        self.list[i].1 += 1;
        i
    }
    fn create(&mut self) -> usize {
        self.list.push((self.want, 1));
        self.list.len() - 1
    }
}

#[test]
fn the_first_match_is_shared() {
    let mut f = Fonts { list: vec![(1, 1), (2, 1), (2, 1)], want: 2, log: Vec::new() };
    assert_eq!(find_or_create(&mut f), 1);
    assert_eq!(f.list[1].1, 2);
    assert_eq!(f.log, ["compare 0", "compare 1"]);
    f.want = 9;
    assert_eq!(find_or_create(&mut f), 3);
    assert_eq!(f.list[3], (9, 1));
}

const GOLDEN: &str = include_str!("../golden_font.txt");
const FACES: [&str; 5] = ["Arial", "Times New Roman", "arial", "Tahoma", "MS Sans Serif"];

/// The golden's world: the shared fonts (id, face, style, quality, size, references).
struct Shared {
    list: Vec<(u64, String, c_ulong, c_ulong, c_ulong, u64)>,
    query: (String, c_ulong, c_ulong, c_ulong),
    next_id: u64,
    log: Vec<String>,
}

impl FontHost for Shared {
    fn count(&self) -> usize {
        self.list.len()
    }
    fn matches(&mut self, i: usize) -> bool {
        let f = &self.list[i];
        let q = &self.query;
        f.3 == q.2 && f.2 == q.1 && f.4 == q.3 && f.1 == q.0
    }
    fn take(&mut self, i: usize) -> usize {
        self.list[i].5 += 1;
        i
    }
    fn create(&mut self) -> usize {
        let (name, style, quality, size) = self.query.clone();
        self.log.push(format!("  new-font {} [{name}] {size} {style} {quality}", self.next_id));
        self.list.push((self.next_id, name, style, quality, size, 2));
        self.next_id += 1;
        self.list.len() - 1
    }
}

impl Shared {
    fn find(&mut self, name: &str, style: c_ulong, quality: c_ulong, size: c_ulong) {
        self.query = (name.to_string(), style, quality, size);
        let i = find_or_create(self);
        let f = &self.list[i];
        let line = format!("  = {} refs {}", f.0, f.5);
        self.log.push(line);
    }
}

#[test]
fn replays_the_cpp_font_table() {
    let mut out: Vec<String> = Vec::new();
    let mut fonts = Shared { list: Vec::new(), query: (String::new(), 0, 0, 0), next_id: 0, log: Vec::new() };
    let mut records: Vec<DbFont> = Vec::new();
    let mut entries: Vec<Option<(String, FontEntry)>> = Vec::new();
    for line in GOLDEN.lines() {
        if line.starts_with("  ") {
            continue;
        }
        out.append(&mut fonts.log);
        out.push(line.to_string());
        let t: Vec<&str> = line.split(' ').collect();
        match t[0] {
            "table" => records.clear(),
            "record" => {
                let b = t[6].as_bytes();
                let bit = |i: usize| b[i] - b'0';
                let mut r = record(if t[2] == "1" { b"font" } else { b"" }, [bit(0), bit(1), bit(2), bit(3), bit(4)]);
                r.string_id = t[3].parse().unwrap();
                r.size = t[4].parse().unwrap();
                r.quality = t[5].parse().unwrap();
                records.push(r);
            }
            "create" => {
                entries = records
                    .iter()
                    .map(|r| {
                        let e = entry(r);
                        e.present.then(|| (FACES[e.string_id as usize % 5].to_string(), e))
                    })
                    .collect();
                for (i, e) in entries.iter().enumerate() {
                    fonts.log.push(match e {
                        Some((name, e)) => format!("  entry {i} [{name}] {} {} {}", e.style, e.quality, e.size),
                        None => format!("  entry {i} none"),
                    });
                }
            }
            "closest" => {
                let i: usize = t[1].parse().unwrap();
                match entries[i].clone() {
                    Some((name, e)) => {
                        let size = closest_size(t[2].parse().unwrap()) as c_ulong;
                        fonts.find(&name, e.style, e.quality, size)
                    }
                    None => fonts.log.push("  = none".into()),
                }
            }
            "get" => {
                let (name, e) = entries[t[1].parse::<usize>().unwrap()].clone().unwrap();
                fonts.find(&name, e.style, e.quality, e.size);
            }
            "find" => {
                // "find [Times New Roman] 3 1 10": the face may hold spaces
                let face = &line[line.find('[').unwrap() + 1..line.find(']').unwrap()];
                let rest: Vec<&str> = line[line.find(']').unwrap() + 2..].split(' ').collect();
                fonts.find(face, rest[0].parse().unwrap(), rest[1].parse().unwrap(), rest[2].parse().unwrap());
            }
            "delete-all" => {
                // the driver holds a reference to each, DeleteAll drops one; both lists end empty
                let list = core::mem::take(&mut fonts.list);
                entries.clear();
                fonts.log.push("  tables 0 fonts 0".into());
                for f in list {
                    fonts.log.push(format!("  font {} refs {}", f.0, f.5));
                }
            }
            other => panic!("unknown line {other}"),
        }
    }
    out.append(&mut fonts.log);
    let golden: Vec<&str> = GOLDEN.lines().collect();
    assert!(golden.len() > 1000, "the golden is the driver's output");
    for (i, (a, b)) in out.iter().zip(golden.iter()).enumerate() {
        assert_eq!(a, b, "line {}", i + 1);
    }
    assert_eq!(out.len(), golden.len());
}
