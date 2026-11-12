//! The next engine's state block in a saved game ("EENS"; `EECPNextLua.cpp` `EECPNextSaveState` /
//! `EECPNextLoadState`, saved with the computer player from save version 0.252). Little-endian int32 fields, strings
//! as a uint32 length and their bytes:
//!
//! - "EENS", version (written: 4; read: 1-4);
//! - the tactics' state: claim range, reserve[5], food preference, land food gatherers, epoch, epoch start, next
//!   repair time, income totals[5], the income ring[60][5], its step count; per player id 0..63: seen army, seen
//!   strength, last seen, seen town x, y; the events (count, then kind, type, count each); the C++ test opening's
//!   memory (4);
//! - the strategy: the script name; started, failed (one byte each); the last step's results (count, then verb, code,
//!   reason, count each); the Lua memory bytes (length 0: no Lua state);
//! - version 2: the army: the next group id; the groups (count, then 14 fields, the units (count, then id and aim
//!   each), the skips (count, then each)); the gather spots and the towers (count, then player, x, y, time, unit each);
//!   per player id 0..63: citizens seen, army x, y; the last step's results' group ids (count, then each);
//! - version 3: the gather safety's spots and the time of its last scan;
//! - version 4 (save 0.274): the walkers sent to see a site, as spots.
//!
//! The decoder keeps the C++ reader's limits: at most 256 events, `max_results` results, 1,000 groups and 2,048 spots
//! of each list are kept (the rest are read and dropped), a group id past the results is read and dropped.

/// The resources of a per-resource field.
pub(crate) const RESOURCES: usize = 5;
/// The player ids of a per-player field.
pub(crate) const PLAYERS: usize = 64;
/// The steps of the income ring.
pub(crate) const RING: usize = 60;
/// The fields of a group, in the block's order (id, task, state, target, x, y, radius, created, state time, way x,
/// way y, way time, started, lost).
pub(crate) const GROUP_FIELDS: usize = 14;

const EVENTS_KEPT: usize = 256;
const GROUPS_KEPT: i64 = 1000;
const SPOTS_KEPT: i64 = 64 * 32;
/// The version the encoder writes.
pub(crate) const VERSION: i32 = 4;

/// An enemy place the army remembers (a gather spot, a tower, a threat, a walker's site).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeAiNextSpot {
    pub player: i32,
    pub x: i32,
    pub y: i32,
    pub time: i32,
    pub unit: i32,
}

/// An event since the last step: kind, unit type, count.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeAiNextEvent {
    pub kind: i32,
    pub unit_type: i32,
    pub count: i32,
}

/// An order's result of the last step.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct OrderResult {
    pub verb: Vec<u8>,
    pub code: i32,
    pub reason: i32,
    pub count: i32,
    pub group: i32,
}

/// A group of the next engine's army.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Group {
    pub fields: [i32; GROUP_FIELDS],
    pub units: Vec<i32>,
    pub aims: Vec<i32>,
    pub skips: Vec<i32>,
}

/// The saved state. On a decode, `version` is the block's: below 2 the army fields (`next_group` to `army_y`) were
/// not in the block and the C++ keeps its own; `threats`/`threats_time` are empty/-1 below 3 and `explore` empty
/// below 4, as the C++ sets them.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct NextSave {
    pub version: i32,
    pub claim_range: i32,
    pub reserve: [i32; RESOURCES],
    pub food_prefer: i32,
    pub food_land: i32,
    pub epoch: i32,
    pub epoch_start: u32,
    pub next_repair_time: u32,
    pub income_total: [i32; RESOURCES],
    pub income_ring: [[i32; RESOURCES]; RING],
    pub income_steps: i32,
    pub seen_army: [i32; PLAYERS],
    pub seen_strength: [i32; PLAYERS],
    pub last_seen: [u32; PLAYERS],
    pub seen_town_x: [i32; PLAYERS],
    pub seen_town_y: [i32; PLAYERS],
    pub events: Vec<EeAiNextEvent>,
    pub test: [i32; 4],
    pub script: Vec<u8>,
    pub started: bool,
    pub failed: bool,
    pub results: Vec<OrderResult>,
    pub memory: Vec<u8>,
    pub next_group: i32,
    pub groups: Vec<Group>,
    pub spots: Vec<EeAiNextSpot>,
    pub towers: Vec<EeAiNextSpot>,
    pub seen_citizens: [i32; PLAYERS],
    pub army_x: [i32; PLAYERS],
    pub army_y: [i32; PLAYERS],
    pub threats: Vec<EeAiNextSpot>,
    pub threats_time: i32,
    pub explore: Vec<EeAiNextSpot>,
}

impl Default for NextSave {
    fn default() -> Self {
        NextSave {
            version: VERSION,
            claim_range: 0,
            reserve: [0; RESOURCES],
            food_prefer: 0,
            food_land: 0,
            epoch: 0,
            epoch_start: 0,
            next_repair_time: 0,
            income_total: [0; RESOURCES],
            income_ring: [[0; RESOURCES]; RING],
            income_steps: 0,
            seen_army: [0; PLAYERS],
            seen_strength: [0; PLAYERS],
            last_seen: [0; PLAYERS],
            seen_town_x: [0; PLAYERS],
            seen_town_y: [0; PLAYERS],
            events: Vec::new(),
            test: [0; 4],
            script: Vec::new(),
            started: false,
            failed: false,
            results: Vec::new(),
            memory: Vec::new(),
            next_group: 0,
            groups: Vec::new(),
            spots: Vec::new(),
            towers: Vec::new(),
            seen_citizens: [0; PLAYERS],
            army_x: [0; PLAYERS],
            army_y: [0; PLAYERS],
            threats: Vec::new(),
            threats_time: -1,
            explore: Vec::new(),
        }
    }
}

fn put(out: &mut Vec<u8>, v: i32) {
    out.extend_from_slice(&v.to_le_bytes());
}

fn put_bytes(out: &mut Vec<u8>, b: &[u8]) {
    put(out, b.len() as i32);
    out.extend_from_slice(b);
}

fn put_spots(out: &mut Vec<u8>, spots: &[EeAiNextSpot]) {
    put(out, spots.len() as i32);
    for s in spots {
        for v in [s.player, s.x, s.y, s.time, s.unit] {
            put(out, v);
        }
    }
}

/// The army block (version 2's part, after the memory bytes).
pub fn encode_army(s: &NextSave, out: &mut Vec<u8>) {
    put(out, s.next_group);
    put(out, s.groups.len() as i32);
    for g in &s.groups {
        for &f in &g.fields {
            put(out, f);
        }
        put(out, g.units.len() as i32);
        for (k, &u) in g.units.iter().enumerate() {
            put(out, u);
            put(out, g.aims[k]);
        }
        put(out, g.skips.len() as i32);
        for &k in &g.skips {
            put(out, k);
        }
    }
    put_spots(out, &s.spots);
    put_spots(out, &s.towers);
    for p in 0..PLAYERS {
        put(out, s.seen_citizens[p]);
        put(out, s.army_x[p]);
        put(out, s.army_y[p]);
    }
    put(out, s.results.len() as i32);
    for r in &s.results {
        put(out, r.group);
    }
}

/// The block of a state (always version 4; `s.version` is not read). A group's `aims` must be as long as its units.
pub fn encode(s: &NextSave) -> Vec<u8> {
    let mut out = Vec::with_capacity(4096);
    out.extend_from_slice(b"EENS");
    put(&mut out, VERSION);
    put(&mut out, s.claim_range);
    for &v in &s.reserve {
        put(&mut out, v);
    }
    put(&mut out, s.food_prefer);
    put(&mut out, s.food_land);
    put(&mut out, s.epoch);
    put(&mut out, s.epoch_start as i32);
    put(&mut out, s.next_repair_time as i32);
    for &v in &s.income_total {
        put(&mut out, v);
    }
    for step in &s.income_ring {
        for &v in step {
            put(&mut out, v);
        }
    }
    put(&mut out, s.income_steps);
    for p in 0..PLAYERS {
        put(&mut out, s.seen_army[p]);
        put(&mut out, s.seen_strength[p]);
        put(&mut out, s.last_seen[p] as i32);
        put(&mut out, s.seen_town_x[p]);
        put(&mut out, s.seen_town_y[p]);
    }
    put(&mut out, s.events.len() as i32);
    for e in &s.events {
        put(&mut out, e.kind);
        put(&mut out, e.unit_type);
        put(&mut out, e.count);
    }
    for &v in &s.test {
        put(&mut out, v);
    }
    put_bytes(&mut out, &s.script);
    out.push(s.started as u8);
    out.push(s.failed as u8);
    put(&mut out, s.results.len() as i32);
    for r in &s.results {
        put_bytes(&mut out, &r.verb);
        put(&mut out, r.code);
        put(&mut out, r.reason);
        put(&mut out, r.count);
    }
    put_bytes(&mut out, &s.memory);
    encode_army(s, &mut out);
    put_spots(&mut out, &s.threats);
    put(&mut out, s.threats_time);
    put_spots(&mut out, &s.explore);
    out
}

/// The C++ reader (`EECPNextGetter`): once a read fails every later one gives 0.
struct Reader<'a> {
    b: &'a [u8],
    at: usize,
    ok: bool,
}

impl Reader<'_> {
    fn get(&mut self) -> i32 {
        if !self.ok || self.at + 4 > self.b.len() {
            self.ok = false;
            return 0;
        }
        let v = i32::from_le_bytes([self.b[self.at], self.b[self.at + 1], self.b[self.at + 2], self.b[self.at + 3]]);
        self.at += 4;
        v
    }
    fn get_bytes(&mut self) -> Vec<u8> {
        let n = self.get();
        if !self.ok || n < 0 || self.at + n as usize > self.b.len() {
            self.ok = false;
            return Vec::new();
        }
        let v = self.b[self.at..self.at + n as usize].to_vec();
        self.at += n as usize;
        v
    }
    fn get_byte(&mut self) -> bool {
        if !self.ok || self.at >= self.b.len() {
            self.ok = false;
            return false;
        }
        let v = self.b[self.at] != 0;
        self.at += 1;
        v
    }
    fn spots(&mut self) -> Vec<EeAiNextSpot> {
        let n = self.get() as i64;
        let mut out = Vec::new();
        let mut k = 0i64;
        while self.ok && k < n {
            let s =
                EeAiNextSpot { player: self.get(), x: self.get(), y: self.get(), time: self.get(), unit: self.get() };
            if k < SPOTS_KEPT {
                out.push(s);
            }
            k += 1;
        }
        out
    }
}

/// The state of a block, None when it is not one (no "EENS", a version not 1-4, a field past the end, bytes left
/// over). `max_results` is the C++ `kNextMaxOrders`: the results past it are read and dropped.
pub fn decode(bytes: &[u8], max_results: usize) -> Option<NextSave> {
    if bytes.len() < 8 || &bytes[0..4] != b"EENS" {
        return None;
    }
    let mut g = Reader { b: bytes, at: 4, ok: true };
    let version = g.get();
    if !(1..=4).contains(&version) {
        return None;
    }
    let mut s = NextSave { version, ..NextSave::default() };
    s.claim_range = g.get();
    for v in s.reserve.iter_mut() {
        *v = g.get();
    }
    s.food_prefer = g.get();
    s.food_land = g.get();
    s.epoch = g.get();
    s.epoch_start = g.get() as u32;
    s.next_repair_time = g.get() as u32;
    for v in s.income_total.iter_mut() {
        *v = g.get();
    }
    for step in s.income_ring.iter_mut() {
        for v in step.iter_mut() {
            *v = g.get();
        }
    }
    s.income_steps = g.get();
    for p in 0..PLAYERS {
        s.seen_army[p] = g.get();
        s.seen_strength[p] = g.get();
        s.last_seen[p] = g.get() as u32;
        s.seen_town_x[p] = g.get();
        s.seen_town_y[p] = g.get();
    }
    let events = g.get() as i64;
    let mut k = 0i64;
    while g.ok && k < events {
        let e = EeAiNextEvent { kind: g.get(), unit_type: g.get(), count: g.get() };
        if k < EVENTS_KEPT as i64 {
            s.events.push(e);
        }
        k += 1;
    }
    for v in s.test.iter_mut() {
        *v = g.get();
    }
    s.script = g.get_bytes();
    s.started = g.get_byte();
    s.failed = g.get_byte();
    let results = g.get() as i64;
    let mut k = 0i64;
    while g.ok && k < results {
        let r = OrderResult { verb: g.get_bytes(), code: g.get(), reason: g.get(), count: g.get(), group: 0 };
        if k < max_results as i64 {
            s.results.push(r);
        }
        k += 1;
    }
    s.memory = g.get_bytes();
    if version >= 2 {
        s.next_group = g.get();
        let groups = g.get() as i64;
        let mut n = 0i64;
        while g.ok && n < groups {
            let mut group = Group::default();
            for f in group.fields.iter_mut() {
                *f = g.get();
            }
            let units = g.get() as i64;
            let mut k = 0i64;
            while g.ok && k < units {
                group.units.push(g.get());
                group.aims.push(g.get());
                k += 1;
            }
            let skips = g.get() as i64;
            let mut k = 0i64;
            while g.ok && k < skips {
                group.skips.push(g.get());
                k += 1;
            }
            if n < GROUPS_KEPT {
                s.groups.push(group);
            }
            n += 1;
        }
        s.spots = g.spots();
        s.towers = g.spots();
        for p in 0..PLAYERS {
            s.seen_citizens[p] = g.get();
            s.army_x[p] = g.get();
            s.army_y[p] = g.get();
        }
        let ids = g.get() as i64;
        let mut k = 0i64;
        while g.ok && k < ids {
            let id = g.get();
            if let Some(r) = s.results.get_mut(k as usize) {
                r.group = id;
            }
            k += 1;
        }
    }
    if version >= 3 {
        s.threats = g.spots();
        s.threats_time = g.get();
    }
    if version >= 4 {
        s.explore = g.spots();
    }
    if !g.ok || g.at != bytes.len() {
        return None;
    }
    Some(s)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn sample() -> NextSave {
        let mut s = NextSave { claim_range: 25, epoch: 3, epoch_start: 0xffff_fff0, ..NextSave::default() };
        s.events.push(EeAiNextEvent { kind: 1, unit_type: 7, count: 2 });
        s.script = b"boom".to_vec();
        s.started = true;
        s.results.push(OrderResult { verb: b"train".to_vec(), code: 1, reason: -1, count: 4, group: 9 });
        s.memory = vec![0, 1, 2, 255];
        s.groups.push(Group { fields: [3; GROUP_FIELDS], units: vec![10, 11], aims: vec![0, 12], skips: vec![5] });
        s.spots.push(EeAiNextSpot { player: 2, x: 3, y: 4, time: 5, unit: 0 });
        s.threats_time = 77;
        s.explore.push(EeAiNextSpot { player: 0, x: 1, y: 1, time: 1, unit: 99 });
        s
    }

    #[test]
    fn round_trip() {
        let s = sample();
        let b = encode(&s);
        assert_eq!(&b[0..8], b"EENS\x04\0\0\0");
        let d = decode(&b, 200).unwrap();
        assert_eq!(d, s);
        assert_eq!(encode(&d), b);
    }

    #[test]
    fn rejects_bad_blocks() {
        let b = encode(&sample());
        assert!(decode(&b[..b.len() - 1], 200).is_none());
        let mut extra = b.clone();
        extra.push(0);
        assert!(decode(&extra, 200).is_none());
        let mut v5 = b.clone();
        v5[4] = 5;
        assert!(decode(&v5, 200).is_none());
        let mut magic = b.clone();
        magic[0] = b'X';
        assert!(decode(&magic, 200).is_none());
        assert!(decode(b"EENS", 200).is_none());
    }

    #[test]
    fn caps_drop_but_read() {
        let mut s = sample();
        s.results =
            (0..5).map(|k| OrderResult { verb: vec![b'a'; k], group: k as i32, ..Default::default() }).collect();
        let d = decode(&encode(&s), 3).unwrap();
        assert_eq!(d.results.len(), 3);
        assert_eq!(d.results[2].group, 2);
        s.events = vec![EeAiNextEvent::default(); 300];
        assert_eq!(decode(&encode(&s), 200).unwrap().events.len(), 256);
    }
}
