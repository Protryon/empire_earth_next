//! The goldens: `golden_ai.txt` is what a scratch driver over the unmodified C++ printed (the functions of
//! `EECPNextLua.cpp`, `EECPNext.cpp`, `EECPNextUnits.cpp` and `EECPEngine.cpp` at the `rust` branch's 78035043,
//! compiled into one exe with fakes of what they reach: PORT_LEARNINGS "R5a.8"). Its generator is replayed here step for
//! step, so every line is checked from the same inputs:
//!
//! - `code`/`reason`/`event`: the name tables; `name`/`key`: the class and tech names of generated data ids;
//! - `nens`: 60 generated next states (every tenth past the decoder's limits): the block's length and FNV-1a 64, then
//!   16 changed blocks (the older versions, bad versions, cuts, a byte more, a bad magic, flipped bytes) decoded onto
//!   another state: whether it reads, and the state then encoded again;
//! - `nenu`: the same for 80 per-unit states (sets and maps as the C++ containers keep them);
//! - `man`: 427 manifest documents (`golden_manifest_cases.txt`, hex): the log lines (the C++ summary line included,
//!   900 bytes at most) and the entries; `choice`/`seat`: choices by name, EE_AI_ENGINE's lists and a hash of 15,000
//!   resolves per manifest.

use std::collections::{BTreeMap, BTreeSet};

use crate::manifest;
use crate::names::{self, Names};
use crate::next_save::{self, EeAiNextEvent, EeAiNextSpot, Group, NextSave, OrderResult};
use crate::seat;
use crate::units_save::{self, EeAiUnitsEvent, EeAiUnitsLimits, EeAiUnitsMark, EeAiUnitsRemembered, UnitsSave};

const GOLDEN: &str = include_str!("golden_ai.txt");
const CASES: &str = include_str!("golden_manifest_cases.txt");

fn fnv64(b: &[u8]) -> u64 {
    let mut h: u64 = 0xcbf29ce484222325;
    for &c in b {
        h = (h ^ c as u64).wrapping_mul(0x100000001b3);
    }
    h
}

fn hex(b: &[u8]) -> String {
    if b.is_empty() {
        return "-".into();
    }
    b.iter().map(|c| format!("{c:02x}")).collect()
}

fn unhex(h: &str) -> Vec<u8> {
    if h == "-" {
        return Vec::new();
    }
    (0..h.len() / 2).map(|i| u8::from_str_radix(&h[2 * i..2 * i + 2], 16).unwrap()).collect()
}

/// The driver's generator.
struct Gen(u32);

impl Gen {
    fn new(seed: u32) -> Gen {
        Gen(seed.wrapping_mul(2654435761).wrapping_add(1))
    }
    fn next(&mut self) -> u32 {
        let mut s = self.0;
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        self.0 = s;
        s
    }
    fn val(&mut self) -> i32 {
        let r = self.next();
        match r % 4 {
            0 => 0,
            1 => (self.next() % 200) as i32 - 50,
            2 => self.next() as i32,
            _ => (self.next() % 100000) as i32,
        }
    }
    fn count(&mut self, max: u32) -> u32 {
        self.next() % (max + 1)
    }
    fn str(&mut self, max: u32, binary: bool) -> Vec<u8> {
        let n = self.count(max);
        (0..n)
            .map(|_| {
                let c = self.next();
                if binary {
                    (c & 0xff) as u8
                } else {
                    b'a' + (c % 26) as u8
                }
            })
            .collect()
    }
    fn spots(&mut self, n: u32) -> Vec<EeAiNextSpot> {
        (0..n)
            .map(|_| EeAiNextSpot {
                player: self.val(),
                x: self.val(),
                y: self.val(),
                time: self.val(),
                unit: self.val(),
            })
            .collect()
    }
}

fn expected(kind: &str) -> Vec<&'static str> {
    GOLDEN.lines().filter(|l| l.split(' ').next() == Some(kind)).collect()
}

fn check(kind: &str, got: &[String]) {
    let want = expected(kind);
    assert_eq!(want.len(), got.len(), "{kind}: line count");
    for (k, (w, g)) in want.iter().zip(got).enumerate() {
        assert_eq!(w, g, "{kind}: line {k}");
    }
}

#[test]
fn name_tables() {
    let mut got = Vec::new();
    for c in -2..=4 {
        got.push(format!("code {c} {}", names::code_name(c).to_str().unwrap()));
    }
    check("code", &got);
    got.clear();
    for r in -1..=15 {
        got.push(format!("reason {r} {}", names::reason_name(r).to_str().unwrap()));
    }
    check("reason", &got);
    got.clear();
    for e in -1..=6 {
        got.push(format!(
            "event {e} {} {}",
            names::event_name(e, c"?").to_str().unwrap(),
            names::event_name(e, c"unknown").to_str().unwrap()
        ));
    }
    check("event", &got);
}

#[test]
fn class_and_tech_names() {
    let mut g = Gen::new(4242);
    let mut rows: [BTreeMap<i64, Vec<u8>>; 2] = [BTreeMap::new(), BTreeMap::new()];
    for table in rows.iter_mut() {
        for _ in 0..120 {
            let key = (g.next() % 400) as i64;
            let id = if g.next().is_multiple_of(9) {
                format!("dbobjects_{}", g.next() % 500).into_bytes()
            } else {
                g.str(3, false)
            };
            table.insert(key, id);
        }
    }
    let classes = Names::new(rows[0].clone(), b"dbobjects_");
    let techs = Names::new(rows[1].clone(), b"dbtechtree_");
    let mut got = Vec::new();
    for k in -3..=420 {
        got.push(format!("name {k} {} {}", hex(&classes.name_of(k)), hex(&techs.name_of(k))));
    }
    check("name", &got);
    let mut queries: Vec<Vec<u8>> = rows.iter().flat_map(|t| t.values().cloned()).collect();
    for q in [
        "dbobjects_",
        "dbobjects_7",
        "dbobjects_12",
        "dbobjects_1x",
        "dbobjects_0",
        "dbobjects_00042",
        "dbobjects_1000000009",
        "dbobjects_10000000090",
        "dbtechtree_",
        "dbtechtree_3",
        "dbtechtree_401",
        "",
        "DBOBJECTS_5",
        "dbobjects_-1",
    ] {
        queries.push(q.as_bytes().to_vec());
    }
    for _ in 0..300 {
        let prefix = if !g.next().is_multiple_of(2) { "dbobjects_" } else { "dbtechtree_" };
        let key = g.next() % 600;
        queries.push(format!("{prefix}{key}").into_bytes());
    }
    // EECPNextClassByName: a key of a class the table has (the driver's: 500 classes, every seventh from 3 missing)
    let class_by_name = |q: &[u8]| {
        let k = classes.key_of(q);
        if (0..500).contains(&k) && k % 7 != 3 {
            k
        } else {
            -1
        }
    };
    got.clear();
    for q in &queries {
        got.push(format!("key {} {} {}", hex(q), class_by_name(q), techs.key_of(q)));
    }
    check("key", &got);
}

// ---- EENS

fn gen_next(seed: u32, big: bool) -> NextSave {
    let mut g = Gen::new(seed);
    let mut s = NextSave { claim_range: g.val(), ..NextSave::default() };
    for v in s.reserve.iter_mut() {
        *v = g.val();
    }
    s.food_prefer = g.val();
    s.food_land = g.val();
    s.epoch = g.val();
    s.epoch_start = g.val() as u32;
    s.next_repair_time = g.val() as u32;
    for v in s.income_total.iter_mut() {
        *v = g.val();
    }
    for step in s.income_ring.iter_mut() {
        for v in step.iter_mut() {
            *v = g.val();
        }
    }
    s.income_steps = g.val();
    for p in 0..64 {
        s.seen_army[p] = g.val();
        s.seen_strength[p] = g.val();
        s.last_seen[p] = g.val() as u32;
        s.seen_town_x[p] = g.val();
        s.seen_town_y[p] = g.val();
    }
    let n = if big { 260 } else { g.count(6) };
    s.events = (0..n).map(|_| EeAiNextEvent { kind: g.val(), unit_type: g.val(), count: g.val() }).collect();
    for v in s.test.iter_mut() {
        *v = g.val();
    }
    s.script = g.str(12, false);
    s.started = g.next() & 1 != 0;
    s.failed = g.next() & 1 != 0;
    let n = if big { 205 } else { g.count(4) };
    s.results = (0..n)
        .map(|_| OrderResult { verb: g.str(8, false), code: g.val(), reason: g.val(), count: g.val(), group: g.val() })
        .collect();
    s.memory = g.str(if big { 3000 } else { 300 }, true);
    s.next_group = g.val();
    let n = if big { 1002 } else { g.count(3) };
    for _ in 0..n {
        let mut group = Group::default();
        for f in group.fields.iter_mut() {
            *f = g.val();
        }
        for _ in 0..g.count(4) {
            group.units.push(g.val());
            group.aims.push(g.val());
        }
        for _ in 0..g.count(2) {
            group.skips.push(g.val());
        }
        s.groups.push(group);
    }
    let n = if big { 2050 } else { g.count(4) };
    s.spots = g.spots(n);
    let n = g.count(3);
    s.towers = g.spots(n);
    for p in 0..64 {
        s.seen_citizens[p] = g.val();
        s.army_x[p] = g.val();
        s.army_y[p] = g.val();
    }
    let n = g.count(3);
    s.threats = g.spots(n);
    s.threats_time = g.val();
    let n = g.count(3);
    s.explore = g.spots(n);
    s
}

fn put_le(b: &mut [u8], at: usize, v: u32) {
    b[at..at + 4].copy_from_slice(&v.to_le_bytes());
}

/// The driver's changed blocks.
fn mutations(seed: u32, b: &[u8], tails: &[usize]) -> Vec<(String, Vec<u8>)> {
    let mut out = vec![("same".to_string(), b.to_vec())];
    let version = b[4] as i32;
    let mut cut = 0;
    for (k, t) in tails.iter().enumerate() {
        cut += t;
        let mut o = b[..b.len() - cut].to_vec();
        let v = version - 1 - k as i32;
        put_le(&mut o, 4, v as u32);
        out.push((format!("v{v}"), o));
    }
    for v in [0u32, version as u32 + 1, 0xffff_ffff] {
        let mut o = b.to_vec();
        put_le(&mut o, 4, v);
        out.push((format!("ver{v}"), o));
    }
    out.push(("cut1".into(), b[..b.len() - 1].to_vec()));
    out.push(("cuthalf".into(), b[..b.len() / 2].to_vec()));
    out.push(("cut7".into(), b[..7].to_vec()));
    out.push(("extra".into(), [b, &[0]].concat()));
    let mut m = b.to_vec();
    m[0] = b'X';
    out.push(("magic".into(), m));
    let mut g = Gen::new(seed.wrapping_mul(7).wrapping_add(3));
    for _ in 0..4 {
        let mut o = b.to_vec();
        let at = g.next() % o.len() as u32;
        let x = 1 + g.next() % 255;
        o[at as usize] ^= x as u8;
        out.push((format!("xor{at}_{x}"), o));
    }
    out
}

/// What `EECPNextLoadState`'s decode leaves in a state: the block's fields, and below version 2 the state's own army.
fn next_apply(target: &NextSave, d: &NextSave) -> NextSave {
    let mut s = d.clone();
    if d.version < 2 {
        s.next_group = target.next_group;
        s.groups = target.groups.clone();
        s.spots = target.spots.clone();
        s.towers = target.towers.clone();
        s.seen_citizens = target.seen_citizens;
        s.army_x = target.army_x;
        s.army_y = target.army_y;
    }
    s
}

#[test]
fn next_state_block() {
    let mut got = Vec::new();
    for seed in 1..=60u32 {
        let s = gen_next(seed, seed % 10 == 9);
        let b = next_save::encode(&s);
        got.push(format!("nens enc {seed} {} {:016x}", b.len(), fnv64(&b)));
        if seed <= 3 {
            got.push(format!("nens hex {seed} {}", hex(&b)));
        }
        let mut army = Vec::new();
        next_save::encode_army(&s, &mut army);
        let tails = [4 + 20 * s.explore.len(), 4 + 20 * s.threats.len() + 4, army.len()];
        for (name, m) in mutations(seed, &b, &tails) {
            let target = gen_next(seed + 1000, false);
            match next_save::decode(&m, 200) {
                None => got.push(format!("nens dec {seed} {name} 0")),
                Some(d) => {
                    let again = next_save::encode(&next_apply(&target, &d));
                    got.push(format!(
                        "nens dec {seed} {name} 1 {} 1 {} {:016x}",
                        !d.memory.is_empty() as i32,
                        again.len(),
                        fnv64(&again)
                    ));
                }
            }
        }
    }
    check("nens", &got);
}

// ---- EENU

/// A per-unit state as the C++ containers keep it.
#[derive(Default)]
struct Units {
    held: BTreeSet<i32>,
    events: Vec<EeAiUnitsEvent>,
    forward: BTreeMap<i32, (i32, i32)>,
    memory: BTreeMap<i32, EeAiUnitsRemembered>,
    memory_ms: i32,
    memory_next: i32,
    naval: bool,
    danger: bool,
    micro: i32,
    hurt: Vec<[i32; 2]>,
    marks: Vec<EeAiUnitsMark>,
    trades: BTreeMap<(i32, i32), (i32, i32)>,
}

impl Units {
    fn save(&self) -> UnitsSave {
        UnitsSave {
            version: units_save::VERSION,
            held: self.held.iter().copied().collect(),
            events: self.events.clone(),
            forward: self.forward.iter().map(|(&a, &(b, t))| [a, b, t]).collect(),
            memory: self.memory.values().copied().collect(),
            memory_ms: self.memory_ms,
            memory_next: self.memory_next,
            reflex_naval_flee: self.naval,
            reflex_danger: self.danger,
            micro_next: self.micro,
            hurt: self.hurt.clone(),
            marks: self.marks.clone(),
            trades: self.trades.iter().map(|(&(a, b), &(c, d))| [a, b, c, d]).collect(),
        }
    }
    /// `EECPUnitsLoadState`'s puts.
    fn load(d: &UnitsSave) -> Units {
        let mut u = Units {
            held: d.held.iter().copied().collect(),
            events: d.events.clone(),
            memory_ms: d.memory_ms,
            memory_next: d.memory_next,
            naval: d.reflex_naval_flee,
            danger: d.reflex_danger,
            micro: d.micro_next,
            hurt: d.hurt.clone(),
            marks: d.marks.clone(),
            ..Units::default()
        };
        for f in &d.forward {
            u.forward.insert(f[0], (f[1], f[2]));
        }
        for r in &d.memory {
            u.memory.insert(r.id, *r);
        }
        for t in &d.trades {
            u.trades.insert((t[0], t[1]), (t[2], t[3]));
        }
        u
    }
}

fn gen_units(seed: u32, big: bool) -> Units {
    let mut g = Gen::new(seed);
    let mut s = Units::default();
    let n = if big { 300 } else { g.count(6) };
    for _ in 0..n {
        s.held.insert(g.val());
    }
    let n = if big { 513 } else { g.count(4) };
    for _ in 0..n {
        let mut kind = 1 + (g.next() % 7) as i32;
        if g.next().is_multiple_of(16) {
            kind = if !g.next().is_multiple_of(2) { 0 } else { 8 };
        }
        s.events.push(EeAiUnitsEvent {
            kind,
            unit: g.val(),
            other: g.val(),
            unit_type: g.val(),
            amount: g.val(),
            time: g.val(),
        });
    }
    for _ in 0..g.count(4) {
        let (a, b, t) = (g.val(), g.val(), g.val());
        s.forward.insert(a, (b, t));
    }
    let n = if big { 4001 } else { g.count(4) };
    for _ in 0..n {
        let id = g.val();
        let r = EeAiUnitsRemembered {
            id,
            player: g.val(),
            unit_type: g.val(),
            x: g.val(),
            y: g.val(),
            hit_points: g.val(),
            seen: g.val(),
            building: g.val(),
        };
        s.memory.insert(id, r);
    }
    s.memory_ms = g.val();
    s.memory_next = g.val();
    s.naval = g.next() & 1 != 0;
    s.danger = g.next() & 1 != 0;
    s.micro = g.val();
    let n = if big { 257 } else { g.count(3) };
    for _ in 0..n {
        s.hurt.push([g.val(), g.val()]);
    }
    let n = if big { 65 } else { g.count(3) };
    for _ in 0..n {
        s.marks.push(EeAiUnitsMark { x: g.val(), y: g.val(), r: g.val(), value: g.val(), until: g.val() });
    }
    let n = if big { 1025 } else { g.count(3) };
    for _ in 0..n {
        let (a, b, c, d) = (g.val(), g.val(), g.val(), g.val());
        s.trades.insert((a, b), (c, d));
    }
    s
}

const LIMITS: EeAiUnitsLimits =
    EeAiUnitsLimits { max_events: 512, max_remembered: 4000, max_trades: 1024, event_kind_min: 1, event_kind_max: 7 };

#[test]
fn units_block() {
    let mut got = Vec::new();
    for seed in 1..=80u32 {
        let s = gen_units(seed, seed % 10 == 9);
        let b = units_save::encode(&s.save());
        got.push(format!("nenu enc {seed} {} {:016x}", b.len(), fnv64(&b)));
        if seed <= 3 {
            got.push(format!("nenu hex {seed} {}", hex(&b)));
        }
        let tails = [4 + 16 * s.trades.len(), 4 + 8 * s.hurt.len() + 4 + 20 * s.marks.len()];
        for (name, m) in mutations(seed, &b, &tails) {
            match units_save::decode(&m, &LIMITS) {
                None => got.push(format!("nenu dec {seed} {name} 0")),
                Some(d) => {
                    let again = units_save::encode(&Units::load(&d).save());
                    got.push(format!("nenu dec {seed} {name} 1 {} {:016x} 0 0 0 0 0 0", again.len(), fnv64(&again)));
                }
            }
        }
    }
    check("nenu", &got);
}

// ---- the manifest and the seats

/// A line as EECPEngineLog writes it: "EE-AI engine " and at most 899 bytes of the text, then a line end.
fn log_line(text: &[u8]) -> Vec<u8> {
    let text = &text[..text.iter().position(|&c| c == 0).unwrap_or(text.len())];
    [b"EE-AI engine ".as_slice(), &text[..text.len().min(899)], b"\n"].concat()
}

#[test]
fn manifest_and_seats() {
    let path: &[u8] = b"Data\\ai\\ais.json";
    let mut man = Vec::new();
    let mut choice = Vec::new();
    let mut seats = Vec::new();
    for (case, line) in CASES.lines().filter(|l| !l.is_empty()).enumerate() {
        let doc = unhex(line);
        let m = manifest::read(&doc, path, &mut |module| {
            // EECPAIScriptExists over the driver's files: every path without "missing"
            let file = [
                b"Data\\ai\\".as_slice(),
                &module.iter().map(|&c| if c == b'.' { b'\\' } else { c }).collect::<Vec<_>>(),
                b".lua",
            ]
            .concat();
            !file.windows(7).any(|w| w == b"missing")
        });
        let mut log: Vec<Vec<u8>> = m.log.iter().map(|l| log_line(l)).collect();
        if m.listed {
            let ids: Vec<String> = m
                .entries
                .iter()
                .map(|e| format!("{}{}", String::from_utf8_lossy(&e.id), if e.random { "" } else { "(not random)" }))
                .collect();
            let ids = if ids.is_empty() { "none".to_string() } else { ids.join(",") };
            let summary = [path, format!(": {} AIs ({ids}) folder hash 12345678", m.entries.len()).as_bytes()].concat();
            log.push(log_line(&summary));
        }
        for l in &log {
            man.push(format!("man {case} log {}", hex(l)));
        }
        for e in &m.entries {
            man.push(format!(
                "man {case} ai {} {} {} {} {}{}{}",
                hex(&e.id),
                hex(&e.name),
                hex(&e.script),
                hex(&e.description),
                e.random as i32,
                e.sees_all as i32,
                e.fills_garrison as i32
            ));
        }
        man.push(format!("man {case} count {}", 2 + m.entries.len()));
        if case >= 12 {
            continue;
        }
        let ids: Vec<&[u8]> = m.entries.iter().map(|e| e.id.as_slice()).collect();
        let random: Vec<bool> = m.entries.iter().map(|e| e.random).collect();
        for name in ["classic", "random", "next", "", "boom", "rush", "classic_lua", "x", "Boom"] {
            choice.push(format!(
                "choice {case} {} {}",
                hex(name.as_bytes()),
                seat::choice_by_name(name.as_bytes(), &ids)
            ));
        }
        for env in ["", "classic", "rush,,x,random", "next", "boom,classic_lua,random", "random", ","] {
            let (overrides, unknown) = seat::overrides(env.as_bytes(), &ids);
            let mut results = String::new();
            for c in 0..(2 + ids.len() as u64 + 3) {
                for s in [0u64, 1, 2, 3, 7, 12345, 0xffff_ffff, 0x8000_0000] {
                    for slot in 0..16u64 {
                        for p in [0u64, 1, 2, 5, 9] {
                            results += &format!("{},", seat::resolve(c, s, slot, p, &overrides, &random));
                        }
                    }
                }
            }
            let choices: String = overrides.iter().map(|c| format!("{c},")).collect();
            let logs: Vec<u8> = unknown
                .iter()
                .flat_map(|n| log_line(&[b"EE_AI_ENGINE: unknown engine '".as_slice(), n, b"' ignored"].concat()))
                .collect();
            seats.push(format!(
                "seat {case} {} {} {} {:016x}",
                hex(env.as_bytes()),
                if choices.is_empty() { "-" } else { &choices },
                hex(&logs),
                fnv64(results.as_bytes())
            ));
        }
    }
    check("man", &man);
    check("choice", &choice);
    check("seat", &seats);
}

/// The blocks of real saves (TEST ONLY, not in the repository: EE_AI_BLOCKS names a folder of `block-*-EENS.bin` /
/// `block-*-EENU.bin` files, e.g. from saved games' players chunks): each decodes, and encodes to the same bytes.
#[test]
#[ignore]
fn real_blocks() {
    let dir = std::env::var("EE_AI_BLOCKS").expect("EE_AI_BLOCKS");
    let mut n = 0;
    for e in std::fs::read_dir(dir).unwrap() {
        let p = e.unwrap().path();
        let name = p.file_name().unwrap().to_string_lossy().into_owned();
        let b = std::fs::read(&p).unwrap();
        if name.ends_with("EENS.bin") {
            let d = next_save::decode(&b, 200).unwrap_or_else(|| panic!("{name}"));
            assert_eq!(next_save::encode(&d), b, "{name}");
        } else if name.ends_with("EENU.bin") {
            let d = units_save::decode(&b, &LIMITS).unwrap_or_else(|| panic!("{name}"));
            assert_eq!(units_save::encode(&d), b, "{name}");
        } else {
            continue;
        }
        n += 1;
    }
    assert!(n > 0);
    eprintln!("{n} real blocks");
}
