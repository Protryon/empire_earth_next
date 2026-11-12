//! The per-unit Lua interface's block in a saved game ("EENU"; `EECPNextUnits.cpp` `EECPUnitsSave` /
//! `EECPUnitsLoad`, after the next engine's block from save version 0.280). Little-endian int32 fields:
//!
//! - "EENU", version (written: 3; read: 1-3);
//! - the held units (count, then each), the pending events (count, then kind, unit, other, type, amount, time each),
//!   the swaps' forwarding (count, then old id, new id, game ms each), the enemy memory (count, then id, player, type,
//!   x, y, hit points, seen, building each); the memory's ms and next time, the naval flee and danger reflexes (0/1),
//!   the micro step's next time;
//! - version 2: the hurt spots (count, then place, time each) and the marks (count, then x, y, r, value, until each);
//! - version 3: the lost layer's trades (count, then minute, cell, lost, dealt each).
//!
//! The lists are in the C++ containers' order on both sides: the encoder writes them as given, the decoder returns
//! them as read, and the C++ puts them back into its sets and maps (a repeated key: the last wins, as before). The
//! limits of the counts the C++ owns (`kMaxEvents`, `kMaxRemembered`, `kMaxTrades`, the event kinds) come with each
//! decode; the format's own are 2^20 held units and forwards, 256 hurt spots, 64 marks.

/// The version the encoder writes.
pub(crate) const VERSION: i32 = 3;
const MAX_HELD: i32 = 1 << 20;
const MAX_FORWARD: i32 = 1 << 20;
const MAX_HURT: i32 = 256;
const MAX_MARKS: i32 = 64;

/// A pending event.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeAiUnitsEvent {
    pub kind: i32,
    pub unit: i32,
    pub other: i32,
    pub unit_type: i32,
    pub amount: i32,
    pub time: i32,
}

/// An enemy unit remembered.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeAiUnitsRemembered {
    pub id: i32,
    pub player: i32,
    pub unit_type: i32,
    pub x: i32,
    pub y: i32,
    pub hit_points: i32,
    pub seen: i32,
    pub building: i32,
}

/// A script's danger mark.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeAiUnitsMark {
    pub x: i32,
    pub y: i32,
    pub r: i32,
    pub value: i32,
    pub until: i32,
}

/// The saved state (the lists in the C++ containers' order).
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct UnitsSave {
    pub version: i32,
    pub held: Vec<i32>,
    pub events: Vec<EeAiUnitsEvent>,
    /// old id, new id, game ms
    pub forward: Vec<[i32; 3]>,
    pub memory: Vec<EeAiUnitsRemembered>,
    pub memory_ms: i32,
    pub memory_next: i32,
    pub reflex_naval_flee: bool,
    pub reflex_danger: bool,
    pub micro_next: i32,
    /// place (x + y * 65536), game ms
    pub hurt: Vec<[i32; 2]>,
    pub marks: Vec<EeAiUnitsMark>,
    /// minute, cell, lost, dealt
    pub trades: Vec<[i32; 4]>,
}

/// The limits of a decode the C++ owns.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct EeAiUnitsLimits {
    pub max_events: i32,
    pub max_remembered: i32,
    pub max_trades: i32,
    /// the event kinds a block may hold (kEvAttacked .. kEvConverted)
    pub event_kind_min: i32,
    pub event_kind_max: i32,
}

fn put(out: &mut Vec<u8>, v: i32) {
    out.extend_from_slice(&v.to_le_bytes());
}

/// The block of a state (always version 3; `s.version` is not read).
pub fn encode(s: &UnitsSave) -> Vec<u8> {
    let mut out = Vec::with_capacity(64 + 4 * s.held.len() + 32 * s.memory.len());
    out.extend_from_slice(b"EENU");
    put(&mut out, VERSION);
    put(&mut out, s.held.len() as i32);
    for &h in &s.held {
        put(&mut out, h);
    }
    put(&mut out, s.events.len() as i32);
    for e in &s.events {
        for v in [e.kind, e.unit, e.other, e.unit_type, e.amount, e.time] {
            put(&mut out, v);
        }
    }
    put(&mut out, s.forward.len() as i32);
    for f in &s.forward {
        for &v in f {
            put(&mut out, v);
        }
    }
    put(&mut out, s.memory.len() as i32);
    for r in &s.memory {
        for v in [r.id, r.player, r.unit_type, r.x, r.y, r.hit_points, r.seen, r.building] {
            put(&mut out, v);
        }
    }
    put(&mut out, s.memory_ms);
    put(&mut out, s.memory_next);
    put(&mut out, s.reflex_naval_flee as i32);
    put(&mut out, s.reflex_danger as i32);
    put(&mut out, s.micro_next);
    put(&mut out, s.hurt.len() as i32);
    for h in &s.hurt {
        put(&mut out, h[0]);
        put(&mut out, h[1]);
    }
    put(&mut out, s.marks.len() as i32);
    for m in &s.marks {
        for v in [m.x, m.y, m.r, m.value, m.until] {
            put(&mut out, v);
        }
    }
    put(&mut out, s.trades.len() as i32);
    for t in &s.trades {
        for &v in t {
            put(&mut out, v);
        }
    }
    out
}

/// The C++ reader of this block: a read past the end fails it, a bad count fails it and reads as 0; reads go on
/// after a failure.
struct Reader<'a> {
    b: &'a [u8],
    at: usize,
    ok: bool,
}

impl Reader<'_> {
    fn get(&mut self) -> i32 {
        if self.at + 4 > self.b.len() {
            self.ok = false;
            return 0;
        }
        let v = i32::from_le_bytes([self.b[self.at], self.b[self.at + 1], self.b[self.at + 2], self.b[self.at + 3]]);
        self.at += 4;
        v
    }
    fn count(&mut self, max: i32) -> i32 {
        let n = self.get();
        if n < 0 || n > max {
            self.ok = false;
        }
        if self.ok {
            n
        } else {
            0
        }
    }
}

/// The state of a block, None when it is not one (no "EENU", a version not 1-3, a count past its limit, an event of
/// another kind, a field past the end, bytes left over).
pub fn decode(bytes: &[u8], limits: &EeAiUnitsLimits) -> Option<UnitsSave> {
    if bytes.len() < 8 || &bytes[0..4] != b"EENU" {
        return None;
    }
    let mut g = Reader { b: bytes, at: 4, ok: true };
    let version = g.get();
    if !(1..=3).contains(&version) {
        return None;
    }
    let mut s = UnitsSave { version, ..UnitsSave::default() };
    let n = g.count(MAX_HELD);
    let mut k = 0;
    while k < n && g.ok {
        s.held.push(g.get());
        k += 1;
    }
    let n = g.count(limits.max_events);
    let mut k = 0;
    while k < n && g.ok {
        let e = EeAiUnitsEvent {
            kind: g.get(),
            unit: g.get(),
            other: g.get(),
            unit_type: g.get(),
            amount: g.get(),
            time: g.get(),
        };
        if e.kind < limits.event_kind_min || e.kind > limits.event_kind_max {
            g.ok = false;
        } else {
            s.events.push(e);
        }
        k += 1;
    }
    let n = g.count(MAX_FORWARD);
    let mut k = 0;
    while k < n && g.ok {
        s.forward.push([g.get(), g.get(), g.get()]);
        k += 1;
    }
    let n = g.count(limits.max_remembered);
    let mut k = 0;
    while k < n && g.ok {
        s.memory.push(EeAiUnitsRemembered {
            id: g.get(),
            player: g.get(),
            unit_type: g.get(),
            x: g.get(),
            y: g.get(),
            hit_points: g.get(),
            seen: g.get(),
            building: g.get(),
        });
        k += 1;
    }
    s.memory_ms = g.get();
    s.memory_next = g.get();
    s.reflex_naval_flee = g.get() != 0;
    s.reflex_danger = g.get() != 0;
    s.micro_next = g.get();
    if version >= 2 {
        let n = g.count(MAX_HURT);
        let mut k = 0;
        while k < n && g.ok {
            s.hurt.push([g.get(), g.get()]);
            k += 1;
        }
        let n = g.count(MAX_MARKS);
        let mut k = 0;
        while k < n && g.ok {
            s.marks.push(EeAiUnitsMark { x: g.get(), y: g.get(), r: g.get(), value: g.get(), until: g.get() });
            k += 1;
        }
    }
    if version >= 3 {
        let n = g.count(limits.max_trades);
        let mut k = 0;
        while k < n && g.ok {
            s.trades.push([g.get(), g.get(), g.get(), g.get()]);
            k += 1;
        }
    }
    if !g.ok || g.at != bytes.len() {
        return None;
    }
    Some(s)
}

#[cfg(test)]
mod tests {
    use super::*;

    const LIMITS: EeAiUnitsLimits = EeAiUnitsLimits {
        max_events: 512,
        max_remembered: 4000,
        max_trades: 1024,
        event_kind_min: 1,
        event_kind_max: 7,
    };

    fn sample() -> UnitsSave {
        UnitsSave {
            version: VERSION,
            held: vec![3, 9, 12],
            events: vec![EeAiUnitsEvent { kind: 2, unit: 5, other: 6, unit_type: 7, amount: -1, time: 1000 }],
            forward: vec![[4, 8, 600]],
            memory: vec![EeAiUnitsRemembered {
                id: 40,
                player: 2,
                unit_type: 3,
                x: 4,
                y: 5,
                hit_points: 6,
                seen: 7,
                building: 1,
            }],
            memory_ms: 180000,
            memory_next: 2000,
            reflex_naval_flee: true,
            reflex_danger: false,
            micro_next: 0x7fff_ffff,
            hurt: vec![[65537, 99]],
            marks: vec![EeAiUnitsMark { x: 1, y: 2, r: 3, value: 4, until: 5 }],
            trades: vec![[1, 2, 3, 4]],
        }
    }

    #[test]
    fn round_trip() {
        let s = sample();
        let b = encode(&s);
        assert_eq!(&b[0..8], b"EENU\x03\0\0\0");
        assert_eq!(decode(&b, &LIMITS).unwrap(), s);
    }

    #[test]
    fn rejects_bad_blocks() {
        let s = sample();
        let b = encode(&s);
        assert!(decode(&b[..b.len() - 1], &LIMITS).is_none());
        assert!(decode(&[b.as_slice(), &[0]].concat(), &LIMITS).is_none());
        let mut bad = s.clone();
        bad.events[0].kind = 8;
        assert!(decode(&encode(&bad), &LIMITS).is_none());
        let mut bad = s.clone();
        bad.marks = vec![EeAiUnitsMark::default(); 65];
        assert!(decode(&encode(&bad), &LIMITS).is_none());
        let mut bad = s;
        bad.trades = vec![[0; 4]; 3];
        assert!(decode(&encode(&bad), &EeAiUnitsLimits { max_trades: 2, ..LIMITS }).is_none());
    }

    #[test]
    fn older_versions() {
        let s = sample();
        let b = encode(&s);
        // version 2: without the trades
        let mut v2 = b[..b.len() - 4 - 16].to_vec();
        v2[4] = 2;
        let d = decode(&v2, &LIMITS).unwrap();
        assert_eq!(d.version, 2);
        assert!(d.trades.is_empty());
        assert_eq!(d.marks, s.marks);
        // version 1: without the hurt spots and marks too
        let mut v1 = b[..b.len() - 4 - 16 - 4 - 20 - 4 - 8].to_vec();
        v1[4] = 1;
        let d = decode(&v1, &LIMITS).unwrap();
        assert!(d.hurt.is_empty() && d.marks.is_empty());
        assert_eq!(d.micro_next, s.micro_next);
    }
}
