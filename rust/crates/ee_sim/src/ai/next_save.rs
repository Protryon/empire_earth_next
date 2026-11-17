//! The next engine's state block in a saved game ("EENS"; `EECPNextLua.cpp` `EECPNextSaveState` /
//! `EECPNextLoadState`, saved with the computer player from save version 0.252). Little-endian int32 fields, strings
//! as a uint32 length and their bytes:
//!
//! - "EENS", version 6 (main's stage 6: only this version loads; the economy verbs' state is the script's memory,
//!   and the C++ army groups, the claim range, the reserve, the food preference, the gather settings, the repair
//!   time, the test opening and the site walkers went with the C++ verbs);
//! - the state: epoch, epoch start, income totals[5], the income ring[60][5], its step count; per player id 0..63:
//!   seen army, seen strength, last seen, seen town x, y; the events (count, then kind, type, count each);
//! - the strategy: the script name; started, failed (one byte each); the last step's results (count, then verb, code,
//!   reason, count each); the Lua memory bytes (length 0: no Lua state);
//! - the army: the gather spots and the towers (count, then player, x, y, time, unit each); per player id 0..63:
//!   citizens seen, army x, y;
//! - the gather safety's spots and the time of its last scan.
//!
//! The decoder keeps the C++ reader's limits: at most 256 events, `max_results` results and 2,048 spots of each list
//! are kept (the rest are read and dropped).

/// The resources of a per-resource field.
pub(crate) const RESOURCES: usize = 5;
/// The player ids of a per-player field.
pub(crate) const PLAYERS: usize = 64;
/// The steps of the income ring.
pub(crate) const RING: usize = 60;

const EVENTS_KEPT: usize = 256;
const SPOTS_KEPT: i64 = 64 * 32;
/// The version the encoder writes and the only one the decoder reads.
pub(crate) const VERSION: i32 = 6;

/// An enemy place the army remembers (a gather spot, a tower, a threat).
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
}

/// The saved state.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct NextSave {
    pub epoch: i32,
    pub epoch_start: u32,
    pub income_total: [i32; RESOURCES],
    pub income_ring: [[i32; RESOURCES]; RING],
    pub income_steps: i32,
    pub seen_army: [i32; PLAYERS],
    pub seen_strength: [i32; PLAYERS],
    pub last_seen: [u32; PLAYERS],
    pub seen_town_x: [i32; PLAYERS],
    pub seen_town_y: [i32; PLAYERS],
    pub events: Vec<EeAiNextEvent>,
    pub script: Vec<u8>,
    pub started: bool,
    pub failed: bool,
    pub results: Vec<OrderResult>,
    pub memory: Vec<u8>,
    pub spots: Vec<EeAiNextSpot>,
    pub towers: Vec<EeAiNextSpot>,
    pub seen_citizens: [i32; PLAYERS],
    pub army_x: [i32; PLAYERS],
    pub army_y: [i32; PLAYERS],
    pub threats: Vec<EeAiNextSpot>,
    pub threats_time: i32,
}

impl Default for NextSave {
    fn default() -> Self {
        NextSave {
            epoch: 0,
            epoch_start: 0,
            income_total: [0; RESOURCES],
            income_ring: [[0; RESOURCES]; RING],
            income_steps: 0,
            seen_army: [0; PLAYERS],
            seen_strength: [0; PLAYERS],
            last_seen: [0; PLAYERS],
            seen_town_x: [0; PLAYERS],
            seen_town_y: [0; PLAYERS],
            events: Vec::new(),
            script: Vec::new(),
            started: false,
            failed: false,
            results: Vec::new(),
            memory: Vec::new(),
            spots: Vec::new(),
            towers: Vec::new(),
            seen_citizens: [0; PLAYERS],
            army_x: [0; PLAYERS],
            army_y: [0; PLAYERS],
            threats: Vec::new(),
            threats_time: -1,
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

/// The army block (after the memory bytes).
pub fn encode_army(s: &NextSave, out: &mut Vec<u8>) {
    put_spots(out, &s.spots);
    put_spots(out, &s.towers);
    for p in 0..PLAYERS {
        put(out, s.seen_citizens[p]);
        put(out, s.army_x[p]);
        put(out, s.army_y[p]);
    }
}

/// The block of a state (version 6).
pub fn encode(s: &NextSave) -> Vec<u8> {
    let mut out = Vec::with_capacity(4096);
    out.extend_from_slice(b"EENS");
    put(&mut out, VERSION);
    put(&mut out, s.epoch);
    put(&mut out, s.epoch_start as i32);
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

/// The state of a block, None when it is not one (no "EENS", a version not 6, a field past the end, bytes left
/// over). `max_results` is the C++ `kNextMaxOrders`: the results past it are read and dropped.
pub fn decode(bytes: &[u8], max_results: usize) -> Option<NextSave> {
    if bytes.len() < 8 || &bytes[0..4] != b"EENS" {
        return None;
    }
    let mut g = Reader { b: bytes, at: 4, ok: true };
    if g.get() != VERSION {
        return None;
    }
    let mut s = NextSave { epoch: g.get(), epoch_start: g.get() as u32, ..NextSave::default() };
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
    s.script = g.get_bytes();
    s.started = g.get_byte();
    s.failed = g.get_byte();
    let results = g.get() as i64;
    let mut k = 0i64;
    while g.ok && k < results {
        let r = OrderResult { verb: g.get_bytes(), code: g.get(), reason: g.get(), count: g.get() };
        if k < max_results as i64 {
            s.results.push(r);
        }
        k += 1;
    }
    s.memory = g.get_bytes();
    s.spots = g.spots();
    s.towers = g.spots();
    for p in 0..PLAYERS {
        s.seen_citizens[p] = g.get();
        s.army_x[p] = g.get();
        s.army_y[p] = g.get();
    }
    s.threats = g.spots();
    s.threats_time = g.get();
    if !g.ok || g.at != bytes.len() {
        return None;
    }
    Some(s)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn sample() -> NextSave {
        let mut s = NextSave { epoch: 3, epoch_start: 0xffff_fff0, ..NextSave::default() };
        s.events.push(EeAiNextEvent { kind: 1, unit_type: 7, count: 2 });
        s.script = b"boom".to_vec();
        s.started = true;
        s.results.push(OrderResult { verb: b"train".to_vec(), code: 1, reason: -1, count: 4 });
        s.memory = vec![0, 1, 2, 255];
        s.spots.push(EeAiNextSpot { player: 2, x: 3, y: 4, time: 5, unit: 0 });
        s.towers.push(EeAiNextSpot { player: 3, x: 9, y: 8, time: 7, unit: 6 });
        s.army_x[5] = -1;
        s.threats.push(EeAiNextSpot { player: 0, x: 1, y: 1, time: 1, unit: 99 });
        s.threats_time = 77;
        s
    }

    #[test]
    fn round_trip() {
        let s = sample();
        let b = encode(&s);
        assert_eq!(&b[0..8], b"EENS\x06\0\0\0");
        let d = decode(&b, 200).unwrap();
        assert_eq!(d, s);
        assert_eq!(encode(&d), b);
    }

    #[test]
    fn layout_as_main_writes_it() {
        // the fixed part: magic, version, epoch, start, 5 totals, 300 ring values, steps, 64 x 5 per player, 0 events,
        // then the empty script, two bytes, 0 results, the empty memory, 0 spots, 0 towers, 64 x 3, 0 threats, time
        let b = encode(&NextSave::default());
        let fixed = 8 + 4 * (2 + 5 + 300 + 1 + 64 * 5 + 1) + 4 + 2 + 4 + 4 + 4 + 4 + 4 * 64 * 3 + 4 + 4;
        assert_eq!(b.len(), fixed);
        assert_eq!(&b[b.len() - 4..], &(-1i32).to_le_bytes());
    }

    #[test]
    fn rejects_bad_blocks() {
        let b = encode(&sample());
        assert!(decode(&b[..b.len() - 1], 200).is_none());
        let mut extra = b.clone();
        extra.push(0);
        assert!(decode(&extra, 200).is_none());
        for v in [1u8, 5, 7] {
            let mut other = b.clone();
            other[4] = v;
            assert!(decode(&other, 200).is_none(), "version {v}");
        }
        let mut magic = b.clone();
        magic[0] = b'X';
        assert!(decode(&magic, 200).is_none());
        assert!(decode(b"EENS", 200).is_none());
    }

    #[test]
    fn caps_drop_but_read() {
        let mut s = sample();
        s.results =
            (0..5).map(|k| OrderResult { verb: vec![b'a'; k], count: k as i32, ..Default::default() }).collect();
        let d = decode(&encode(&s), 3).unwrap();
        assert_eq!(d.results.len(), 3);
        assert_eq!(d.results[2].count, 2);
        s.events = vec![EeAiNextEvent::default(); 300];
        assert_eq!(decode(&encode(&s), 200).unwrap().events.len(), 256);
    }
}
