//! The hot keys' setters and `CreateHotKeys` against the real C++ (`golden_hotkey_setters.txt`): a scratch driver
//! (`build/u9_golden`, `driver_hotkey.cpp`) ran the unmodified `UIHotKey.cpp` (`UIEvent::Initialize`/`Uninitialize`
//! as log lines, a fake keyboard whose `GetEntity` logs the code and fails for 0x99, a fake database file) over 150
//! hot keys of 12 random operations (the four event setters over a pool of events and NULL, the same event again
//! included; `SetModifier` over shift, control, alt and plain codes) and 200 random database files (unused
//! records, empty modifiers, short files, failing reads, failing key lookups), printing every event call, key lookup
//! and file call and the hot keys' state after each. The replay below runs the same through this module; the driver
//! relinked over the forwarders prints the same file, and the same on two more seeds at ten times the size.

use super::*;

const GOLDEN: &str = include_str!("../golden_hotkey_setters.txt");

/// `ISKeyboard::ComputeModifierState` (`ISKeyboard.h`, inline).
fn compute_modifier_state(code: c_ulong) -> c_ulong {
    let mut s = 0;
    if code == 0x2a || code == 0x36 || code == 0xe1 {
        s = 1;
    }
    if code == 0x38 || code == 0xb8 || code == 0xe2 {
        s |= 4;
    }
    if code == 0x1d || code == 0x9d || code == 0xe0 {
        s |= 2;
    }
    s
}

#[derive(Clone, Copy)]
struct Key {
    key: i32,
    mods: [i32; 3],
    state: c_ulong,
    count: c_ulong,
    events: [i32; 4],
}

impl Key {
    fn new() -> Key {
        Key { key: -1, mods: [-1; 3], state: 0, count: 0, events: [-1; 4] }
    }
    fn text(&self, name: &str) -> String {
        format!(
            "  {name} key {} mods {} {} {} state {} count {} events {} {} {} {}",
            self.key,
            self.mods[0],
            self.mods[1],
            self.mods[2],
            self.state,
            self.count,
            self.events[0],
            self.events[1],
            self.events[2],
            self.events[3]
        )
    }
}

/// The driver's world: the hot keys (a `HotKey` is an index + 1 into `keys`), the events' reference counts, the
/// file, the log.
#[derive(Default)]
struct World {
    keys: Vec<Key>,
    refs: [u32; 4],
    out: Vec<String>,
    // the file: the records' bytes, the read that fails, the reads so far
    data: Vec<u8>,
    at: usize,
    fail_at: i64,
    reads: i64,
    table: Vec<HotKey>,
    slot: usize,
}

fn handle(i: usize) -> HotKey {
    (i + 1) as HotKey
}

impl World {
    fn key(&mut self, k: HotKey) -> &mut Key {
        &mut self.keys[k as usize - 1]
    }
    fn read(&mut self, out: &mut [u8]) -> c_long {
        let r = self.reads;
        self.reads += 1;
        if r == self.fail_at || self.at + out.len() > self.data.len() {
            self.out.push(format!("  read {} fails", out.len()));
            return 9;
        }
        out.copy_from_slice(&self.data[self.at..self.at + out.len()]);
        self.at += out.len();
        0
    }
}

impl EventSlotHost for World {
    fn get(&self) -> Event {
        let e = self.keys[0].events[self.slot];
        if e < 0 {
            core::ptr::null_mut()
        } else {
            (e + 1) as Event
        }
    }
    fn set(&mut self, e: Event) {
        let slot = self.slot;
        self.keys[0].events[slot] = e as i32 - 1;
    }
    fn call(&mut self, e: Event, c: EventCall) {
        let i = e as usize - 1;
        match c {
            EventCall::Uninitialize => self.out.push(format!("  uninit {i}")),
            EventCall::Dereference => self.refs[i] -= 1,
            EventCall::Reference => self.refs[i] += 1,
            EventCall::Initialize => self.out.push(format!("  init {i}")),
        }
    }
}

impl KeysHost for World {
    fn get_entity(&mut self, key: HotKey, code: c_ulong, member: usize) -> c_long {
        self.out.push(format!("  entity {code}"));
        let id = (code & 0xff) as i32;
        let k = self.key(key);
        if member == KEY_SLOT {
            k.key = id;
        } else {
            k.mods[member] = id;
        }
        if code == 0x99 {
            7
        } else {
            0
        }
    }
    fn modifier_state(&mut self, code: c_ulong) -> c_ulong {
        compute_modifier_state(code)
    }
    fn state(&self, key: HotKey) -> c_ulong {
        self.keys[key as usize - 1].state
    }
    fn set_state(&mut self, key: HotKey, state: c_ulong) {
        self.key(key).state = state;
    }
    fn clear_modifier(&mut self, key: HotKey, slot: usize) {
        self.key(key).mods[slot] = -1;
    }
    fn count_keys(&mut self, key: HotKey) {
        let k = self.key(key);
        k.count = key_count(k.key >= 0, [k.mods[0] >= 0, k.mods[1] >= 0, k.mods[2] >= 0]);
    }
}

impl CreateHost for World {
    fn open(&mut self) {
        self.out.push("  open 1".into());
    }
    fn read_count(&mut self, count: &mut c_ulong) -> c_long {
        let mut b = [0u8; 4];
        let r = self.read(&mut b);
        if r == 0 {
            *count = u32::from_le_bytes(b) as c_ulong;
        }
        r
    }
    fn read_record(&mut self, record: &mut DbHotKey) -> c_long {
        let mut b = [0u8; 124];
        let r = self.read(&mut b);
        if r == 0 {
            let w = |i: usize| u32::from_le_bytes(b[i..i + 4].try_into().unwrap());
            record.id = w(4) as i32;
            record.modifiers = [w(108), w(112), w(116)];
            record.key = w(120);
        }
        r
    }
    fn create(&mut self) -> HotKey {
        self.keys.push(Key::new());
        handle(self.keys.len() - 1)
    }
    fn push(&mut self, key: HotKey) {
        self.table.push(key);
    }
    fn close(&mut self) {
        self.out.push("  close".into());
    }
}

fn refs(w: &mut World) {
    let r = w.refs;
    w.out.push(format!("  refs {} {} {} {}", r[0], r[1], r[2], r[3]));
}

fn replay(golden: &str) -> (usize, usize) {
    let lines: Vec<&str> = golden.lines().collect();
    let mut w = World { refs: [1000; 4], ..World::default() };
    let (mut keys, mut files) = (0, 0);
    let mut i = 0;
    while i < lines.len() {
        let line = lines[i];
        i += 1;
        let t: Vec<&str> = line.split(' ').collect();
        match t[0] {
            "hotkey" => {
                keys += 1;
                w.out.push(line.to_string());
                w.keys = vec![Key::new()];
                let k = w.keys[0];
                w.out.push(k.text("k"));
                refs(&mut w);
            }
            "op" => {
                w.out.push(line.to_string());
                if t[1] == "modifier" {
                    set_modifier(&mut w, handle(0), t[2].parse().unwrap(), t[3].parse().unwrap());
                } else {
                    w.slot = t[2].parse().unwrap();
                    let e: i32 = t[3].parse().unwrap();
                    set_event(&mut w, (e + 1) as Event);
                }
                let k = w.keys[0];
                w.out.push(k.text("k"));
                refs(&mut w);
            }
            "end" => {
                // the hot key's events released (the driver's four SetX(NULL)), before the line
                for slot in 0..4 {
                    w.slot = slot;
                    set_event(&mut w, core::ptr::null_mut());
                }
                w.out.push("end".into());
                refs(&mut w);
            }
            "create" => {
                files += 1;
                w.out.push(line.to_string());
                let count: u32 = t[3].parse().unwrap();
                let records: usize = t[5].parse().unwrap();
                w.data = count.to_le_bytes().to_vec();
                w.at = 0;
                w.reads = 0;
                w.fail_at = t[7].parse().unwrap();
                for _ in 0..records {
                    let r: Vec<&str> = lines[i].split(' ').collect();
                    w.out.push(lines[i].to_string());
                    i += 1;
                    let mut b = vec![0u8; 124];
                    b[4..8].copy_from_slice(&r[3].parse::<i32>().unwrap().to_le_bytes());
                    for m in 0..3 {
                        b[108 + 4 * m..112 + 4 * m].copy_from_slice(&r[5 + m].parse::<u32>().unwrap().to_le_bytes());
                    }
                    b[120..124].copy_from_slice(&r[9].parse::<u32>().unwrap().to_le_bytes());
                    w.data.extend_from_slice(&b);
                }
                w.keys.clear();
                w.table.clear();
                let result = create(&mut w);
                w.out.push(format!("  result {result} table {}", w.table.len()));
                for (n, k) in w.table.clone().iter().enumerate() {
                    let l =
                        if k.is_null() { format!("  {n} null") } else { w.keys[*k as usize - 1].text(&n.to_string()) };
                    w.out.push(l);
                }
            }
            _ => {}
        }
    }
    let out = w.out;
    for (n, (a, g)) in out.iter().zip(lines.iter()).enumerate() {
        assert_eq!(a, g, "line {}", n + 1);
    }
    assert_eq!(out.len(), lines.len());
    (keys, files)
}

#[test]
fn replays_the_cpp_golden() {
    assert_eq!(replay(GOLDEN), (150, 200));
}

/// Another seed's golden (seeds 2 and 3, ten times the size): `EE_UI_GOLDEN=<file> cargo test -- --ignored setters`.
#[test]
#[ignore]
fn replays_another_seed() {
    let path = std::env::var("EE_UI_GOLDEN").expect("EE_UI_GOLDEN");
    assert_eq!(replay(&std::fs::read_to_string(path).unwrap()), (1500, 2000));
}
