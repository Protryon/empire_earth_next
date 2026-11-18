//! The next engine's per-player state (`EECPNextState`, EECPNext.cpp before; lane A, round 3): the epoch and income
//! bookkeeping, the scouting memory (the enemies' armies, towns, gather spots and towers), the gather safety's
//! threat memory, the events since the last step, the strategy's Lua state and its script, and the last step's order
//! results. Rust owns the record; the C++ computer player holds it as an opaque pointer (`mNextState`).
//!
//! This module holds what is pure over the record (the tested logic); `next_game` reads the world into it.

#![allow(clippy::unnecessary_cast)] // c_long is i32 on Windows, i64 on LP64
#![allow(clippy::needless_range_loop)] // the C++'s loops over parallel arrays

use std::os::raw::{c_long, c_ulong};

use crate::ai::lua_host::Host;
use crate::ai::next_save::{EeAiNextEvent, EeAiNextSpot, NextSave, OrderResult, PLAYERS, RESOURCES, RING};

/// `kNextThreatRange`: tiles: seen enemy soldiers this near a resource keep gatherers off it
pub const THREAT_RANGE: c_long = 9;
/// `kNextThreatMs`: ... remembered this long after they were last seen
pub const THREAT_MS: c_long = 20000;
/// `kNextTowerMargin`: tiles past a tower's range (the gatherers stand round the mine)
pub const TOWER_MARGIN: c_long = 3;
/// `kNextSpotRange`: gather spots this near are one spot
const SPOT_RANGE: c_long = 8;
const SPOTS_PER_PLAYER: c_long = 8;
const TOWERS_PER_PLAYER: c_long = 32;
/// `kNextTowerAvoid`: a gather spot this near a known tower is not a raid's (the view's citizen_x)
const TOWER_AVOID: c_long = 9;

/// `EECPNextBlocked*`
pub const BLOCKED_NONE: c_long = 0;
pub const BLOCKED_UNEXPLORED: c_long = 1;
pub const BLOCKED_ENEMY_NEAR: c_long = 2;
pub const BLOCKED_DANGER: c_long = 3;
pub const BLOCKED_AREA: c_long = 4;
pub const BLOCKED_TOWER: c_long = 5;

/// `EECPNextSeenKind`
pub const SEEN_SOLDIER: c_long = 0;
pub const SEEN_CITIZEN: c_long = 1;
pub const SEEN_BOAT: c_long = 2;
pub const SEEN_TOWER: c_long = 3;
pub const SEEN_BUILDING: c_long = 4;
pub const SEEN_SHIP: c_long = 5;

/// What the army remembers of an enemy's place (`EECPNextSpot`): a gather spot (unit 0), a tower, a threat.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Spot {
    pub player: c_long,
    pub x: c_long,
    pub y: c_long,
    pub time: c_long,
    pub unit: c_long,
}

/// An enemy unit the player sees now (`EECPNextSeen`; rebuilt each step, not saved).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Seen {
    pub unit: c_long,
    pub player: c_long,
    pub x: c_long,
    pub y: c_long,
    pub kind: c_long,
    pub strength: c_long,
    pub hit_points: c_long,
    pub continent: u16,
}

/// An event since the last step (`EECPNextEvent`).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Event {
    pub kind: c_long,
    pub unit_type: c_long,
    pub count: c_long,
}

/// The record (`EECPNextState`).
pub struct NextState {
    pub epoch: c_long,
    pub epoch_start: c_ulong,
    pub income_total: [c_long; RESOURCES],
    pub income_ring: [[c_long; RESOURCES]; RING],
    pub income_steps: c_long,
    pub seen_army: [c_long; PLAYERS],
    pub seen_strength: [c_long; PLAYERS],
    pub last_seen: [c_ulong; PLAYERS],
    pub seen_town_x: [c_long; PLAYERS],
    pub seen_town_y: [c_long; PLAYERS],
    pub events: Vec<Event>,
    pub spots: Vec<Spot>,
    pub towers: Vec<Spot>,
    pub seen_citizens: [c_long; PLAYERS],
    pub army_x: [c_long; PLAYERS],
    pub army_y: [c_long; PLAYERS],
    pub seen: Vec<Seen>,
    pub threats: Vec<Spot>,
    pub threats_time: c_long,
    pub home: Vec<c_long>,
    pub forests: Vec<u16>,
    pub forests_x: c_long,
    pub forests_y: c_long,
    pub forests_time: c_ulong,
    /// the script's Lua state (null until the first step)
    pub lua: *mut Host,
    pub ai: Vec<u8>,
    pub script: Vec<u8>,
    pub started: bool,
    pub failed: bool,
    pub restore: bool,
    pub restore_bytes: Vec<u8>,
    pub results: Vec<OrderResult>,
    pub errors: c_long,
    /// TEST ONLY (EE_AI_SAVE_CHECK): a Lua state restored from the last save's bytes
    pub shadow: *mut Host,
}

impl Default for NextState {
    fn default() -> Self {
        NextState {
            epoch: -1,
            epoch_start: 0,
            income_total: [0; RESOURCES],
            income_ring: [[0; RESOURCES]; RING],
            income_steps: 0,
            seen_army: [0; PLAYERS],
            seen_strength: [0; PLAYERS],
            last_seen: [0; PLAYERS],
            seen_town_x: [-1; PLAYERS],
            seen_town_y: [-1; PLAYERS],
            events: Vec::new(),
            spots: Vec::new(),
            towers: Vec::new(),
            seen_citizens: [0; PLAYERS],
            army_x: [-1; PLAYERS],
            army_y: [-1; PLAYERS],
            seen: Vec::new(),
            threats: Vec::new(),
            threats_time: -1,
            home: Vec::new(),
            forests: Vec::new(),
            forests_x: -1,
            forests_y: -1,
            forests_time: 0,
            lua: std::ptr::null_mut(),
            ai: Vec::new(),
            script: Vec::new(),
            started: false,
            failed: false,
            restore: false,
            restore_bytes: Vec::new(),
            results: Vec::new(),
            errors: 0,
            shadow: std::ptr::null_mut(),
        }
    }
}

impl Drop for NextState {
    fn drop(&mut self) {
        for h in [self.lua, self.shadow] {
            if !h.is_null() {
                // SAFETY: the record owns its Lua states
                unsafe { crate::ai::lua_host::free(h) };
            }
        }
    }
}

/// The squared distance (`EECPNextDistance2`: the differences in `long`, the squares in `long long`).
pub fn d2(x: c_long, y: c_long, x2: c_long, y2: c_long) -> i64 {
    let dx = x.wrapping_sub(x2) as i64;
    let dy = y.wrapping_sub(y2) as i64;
    dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy))
}

/// `EECPNextNear`
pub fn near(x: c_long, y: c_long, x2: c_long, y2: c_long, range: c_long) -> bool {
    d2(x, y, x2, y2) <= (range as i64) * (range as i64)
}

/// The integer square root (`EECPNextSqrt`, `EECPNextRoot`: no float in the view).
pub fn isqrt(v: i64) -> c_long {
    if v <= 0 {
        return 0;
    }
    let mut value = v as u64;
    let mut root = 0u64;
    let mut bit = 1u64 << 62;
    while bit > value {
        bit >>= 2;
    }
    while bit != 0 {
        if value >= root + bit {
            value -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }
    root as c_long
}

impl NextState {
    /// `EECPNextIncome`'s body: a resource index 0..4 got `amount`
    pub fn income(&mut self, i: c_long, amount: c_long) {
        if (0..RESOURCES as c_long).contains(&i) && amount > 0 {
            self.income_total[i as usize] = self.income_total[i as usize].wrapping_add(amount);
        }
    }

    /// `EECPNextEventAdd`'s body: merged with an event of the same kind and type, at most 256
    pub fn event_add(&mut self, kind: c_long, unit_type: c_long, count: c_long) {
        if let Some(e) = self.events.iter_mut().find(|e| e.kind == kind && e.unit_type == unit_type) {
            e.count = e.count.wrapping_add(count);
            return;
        }
        if self.events.len() < 256 {
            self.events.push(Event { kind, unit_type, count });
        }
    }

    /// The epoch at this step: a change after the first sets the epoch start (`EECPNextUpdate`)
    pub fn note_epoch(&mut self, epoch: c_long, now: c_ulong) {
        if self.epoch != epoch {
            if self.epoch >= 0 {
                self.epoch_start = now;
            }
            self.epoch = epoch;
        }
    }

    /// The income ring after a step: the totals at this step's slot
    pub fn ring_step(&mut self) {
        let slot = (self.income_steps % RING as c_long) as usize;
        self.income_ring[slot] = self.income_total;
        self.income_steps = self.income_steps.wrapping_add(1);
    }

    /// What came in over the last 60 steps, by resource (the view's income)
    pub fn income_last(&self, i: usize) -> c_long {
        let slot = (self.income_steps % RING as c_long) as usize;
        self.income_total[i].wrapping_sub(self.income_ring[slot][i])
    }

    /// `EECPNextUpdateThreats`: the enemy soldiers in sight this step go into the memory (by unit id); the ones not
    /// seen for `THREAT_MS` are forgotten
    pub fn update_threats(&mut self, now: c_long) {
        for k in 0..self.seen.len() {
            let s = self.seen[k];
            if s.kind != SEEN_SOLDIER {
                continue;
            }
            let mut known = false;
            for t in self.threats.iter_mut() {
                if t.unit == s.unit {
                    t.x = s.x;
                    t.y = s.y;
                    t.time = now;
                    known = true;
                    break;
                }
            }
            if !known && self.threats.len() < 512 {
                self.threats.push(Spot { player: s.player, x: s.x, y: s.y, time: now, unit: s.unit });
            }
        }
        let mut t = self.threats.len();
        while t > 0 {
            t -= 1;
            if now.wrapping_sub(self.threats[t].time) > THREAT_MS {
                self.threats.remove(t);
            }
        }
        self.threats_time = now;
    }

    /// The first part of `EECPNextSiteThreat`: the remembered soldier nearest the tile within `THREAT_RANGE` (only
    /// the ones in sight at the last scan with `now_only`) -> its unit id (0: none)
    pub fn threat_near(&self, x: c_long, y: c_long, now_only: bool) -> c_long {
        let mut unit = 0;
        let mut best = 0i64;
        for s in &self.threats {
            if now_only && s.time != self.threats_time {
                continue;
            }
            let d = d2(x, y, s.x, s.y);
            if d <= (THREAT_RANGE * THREAT_RANGE) as i64 && (unit == 0 || d < best) {
                unit = s.unit;
                best = d;
            }
        }
        unit
    }

    /// `EECPNextTowerAt`: the enemy tower (remembered, or an armed building in sight) whose range + half its size +
    /// `margin` tiles covers the tile -> its unit id, 0 none. `look(unit)` answers a unit's shooting range
    /// (`EECPNextShootRange`, 0: none or gone) and half its tile size (None: no such unit).
    pub fn tower_at(
        &self,
        x: c_long,
        y: c_long,
        margin: c_long,
        look: &mut dyn FnMut(c_long) -> (c_long, Option<c_long>),
    ) -> c_long {
        for s in &self.towers {
            let (mut range, half) = look(s.unit);
            let half = half.unwrap_or(1);
            if range == 0 {
                range = 6; // (gone from the world unseen: the memory keeps it, at a guard tower's reach)
            }
            let reach = range.wrapping_add(half).wrapping_add(margin);
            if d2(x, y, s.x.wrapping_add(half), s.y.wrapping_add(half)) <= (reach as i64) * (reach as i64) {
                return s.unit;
            }
        }
        for s in &self.seen {
            if s.kind != SEEN_BUILDING {
                continue;
            }
            let (range, half) = look(s.unit);
            if range == 0 {
                continue;
            }
            let half = half.unwrap_or(0);
            let reach = range.wrapping_add(half).wrapping_add(margin);
            if d2(x, y, s.x.wrapping_add(half), s.y.wrapping_add(half)) <= (reach as i64) * (reach as i64) {
                return s.unit;
            }
        }
        0
    }

    /// `EECPNextIsHome`: one of the start resources by the town
    pub fn is_home(&self, unit: c_long) -> bool {
        self.home.binary_search(&unit).is_ok()
    }

    /// `EECPNextUpdateHome`'s pick: the nearest of each resource within reach (`found[r]`: (distance, unit id)), as
    /// many as `WANTED`, the ids sorted
    pub fn set_home(&mut self, mut found: [Vec<(i64, c_long)>; RESOURCES]) {
        const WANTED: [usize; RESOURCES] = [2, 0, 1, 2, 2];
        self.home.clear();
        for r in 0..RESOURCES {
            found[r].sort();
            for k in 0..found[r].len().min(WANTED[r]) {
                self.home.push(found[r][k].1);
            }
        }
        self.home.sort();
    }

    /// The scan's start: the units seen and the citizens in sight are rebuilt
    pub fn scan_begin(&mut self) {
        self.seen.clear();
        self.seen_citizens = [0; PLAYERS];
    }

    /// A citizen or fishing boat seen: a gather spot (one within `SPOT_RANGE` is the same spot, its time renewed;
    /// at most `SPOTS_PER_PLAYER` a player, the oldest replaced)
    pub fn note_gatherer(&mut self, p: c_long, x: c_long, y: c_long, now: c_long) {
        self.seen_citizens[p as usize] = self.seen_citizens[p as usize].wrapping_add(1);
        for s in self.spots.iter_mut() {
            if s.player == p && near(s.x, s.y, x, y, SPOT_RANGE) {
                s.time = now;
                return;
            }
        }
        let mut count = 0;
        let mut oldest: Option<usize> = None;
        for (k, s) in self.spots.iter().enumerate() {
            if s.player == p {
                count += 1;
                if oldest.is_none_or(|o| s.time < self.spots[o].time) {
                    oldest = Some(k);
                }
            }
        }
        let spot = Spot { player: p, x, y, time: now, unit: 0 };
        if count < SPOTS_PER_PLAYER {
            self.spots.push(spot);
        } else if let Some(o) = oldest {
            self.spots[o] = spot;
        }
    }

    /// A tower seen: remembered (its time renewed), at most `TOWERS_PER_PLAYER` a player
    pub fn note_tower(&mut self, p: c_long, x: c_long, y: c_long, now: c_long, unit: c_long) {
        let mut known = false;
        let mut count = 0;
        for t in self.towers.iter_mut() {
            if t.player == p {
                count += 1;
            }
            if t.unit == unit {
                t.time = now;
                known = true;
            }
        }
        if !known && count < TOWERS_PER_PLAYER {
            self.towers.push(Spot { player: p, x, y, time: now, unit });
        }
    }

    /// The scan's end: the armies' mean places; forgotten: a tower whose tile is in sight with the tower gone, a
    /// gather spot in sight with no gatherer near for a minute (`seen_tile(x, y)`: no such tile, or the player sees
    /// it)
    pub fn scan_end(
        &mut self,
        now: c_long,
        army: &[(i64, i64, c_long); PLAYERS],
        seen_tile: &mut dyn FnMut(c_long, c_long) -> bool,
    ) {
        for p in 0..PLAYERS {
            let (sx, sy, n) = army[p];
            if n > 0 {
                self.army_x[p] = (sx / n as i64) as c_long;
                self.army_y[p] = (sy / n as i64) as c_long;
            }
        }
        let mut t = self.towers.len();
        while t > 0 {
            t -= 1;
            let s = self.towers[t];
            if s.time == now {
                continue;
            }
            if seen_tile(s.x, s.y) {
                self.towers.remove(t);
            }
        }
        let mut k = self.spots.len();
        while k > 0 {
            k -= 1;
            let s = self.spots[k];
            if now.wrapping_sub(s.time) < 60000 {
                continue;
            }
            if seen_tile(s.x, s.y) {
                self.spots.remove(k);
            }
        }
    }

    fn near_tower(&self, x: c_long, y: c_long) -> bool {
        self.towers.iter().any(|t| near(x, y, t.x, t.y, TOWER_AVOID))
    }

    /// `EECPNextBestSpot`: the latest gather spot of a player with no known tower near (away from `not` when set)
    pub fn best_spot(&self, player: c_long, not: Option<(c_long, c_long)>) -> Option<(c_long, c_long)> {
        let mut best: Option<usize> = None;
        for (k, s) in self.spots.iter().enumerate() {
            if s.player != player || self.near_tower(s.x, s.y) || not.is_some_and(|(nx, ny)| near(s.x, s.y, nx, ny, 10))
            {
                continue;
            }
            if best.is_none_or(|b| s.time > self.spots[b].time) {
                best = Some(k);
            }
        }
        best.map(|b| (self.spots[b].x, self.spots[b].y))
    }

    /// The towers remembered of a player
    pub fn towers_of(&self, player: c_long) -> c_long {
        self.towers.iter().filter(|t| t.player == player).count() as c_long
    }

    /// The saved block's fields (`EECPNextEncodeState`; `memory` the Lua memory bytes)
    pub fn to_save(&self, memory: &[u8]) -> NextSave {
        let spots = |v: &[Spot]| {
            v.iter()
                .map(|s| EeAiNextSpot {
                    player: s.player as i32,
                    x: s.x as i32,
                    y: s.y as i32,
                    time: s.time as i32,
                    unit: s.unit as i32,
                })
                .collect::<Vec<_>>()
        };
        let mut s = NextSave {
            epoch: self.epoch as i32,
            epoch_start: self.epoch_start as u32,
            income_steps: self.income_steps as i32,
            events: self
                .events
                .iter()
                .map(|e| EeAiNextEvent { kind: e.kind as i32, unit_type: e.unit_type as i32, count: e.count as i32 })
                .collect(),
            script: self.script.clone(),
            started: self.started,
            failed: self.failed,
            results: self.results.clone(),
            memory: memory.to_vec(),
            spots: spots(&self.spots),
            towers: spots(&self.towers),
            threats: spots(&self.threats),
            threats_time: self.threats_time as i32,
            ..NextSave::default()
        };
        for i in 0..RESOURCES {
            s.income_total[i] = self.income_total[i] as i32;
        }
        for k in 0..RING {
            for i in 0..RESOURCES {
                s.income_ring[k][i] = self.income_ring[k][i] as i32;
            }
        }
        for p in 0..PLAYERS {
            s.seen_army[p] = self.seen_army[p] as i32;
            s.seen_strength[p] = self.seen_strength[p] as i32;
            s.last_seen[p] = self.last_seen[p] as u32;
            s.seen_town_x[p] = self.seen_town_x[p] as i32;
            s.seen_town_y[p] = self.seen_town_y[p] as i32;
            s.seen_citizens[p] = self.seen_citizens[p] as i32;
            s.army_x[p] = self.army_x[p] as i32;
            s.army_y[p] = self.army_y[p] as i32;
        }
        s
    }

    /// A decoded block into the record (`EECPNextDecodeState`; the Lua state goes, the memory waits in
    /// `restore_bytes` for the first step). Leaves the Lua state pointer to the caller (it frees it first).
    pub fn from_save(&mut self, v: NextSave) {
        let spots = |v: &[EeAiNextSpot]| {
            v.iter()
                .map(|s| Spot {
                    player: s.player as c_long,
                    x: s.x as c_long,
                    y: s.y as c_long,
                    time: s.time as c_long,
                    unit: s.unit as c_long,
                })
                .collect::<Vec<_>>()
        };
        self.epoch = v.epoch as c_long;
        self.epoch_start = v.epoch_start as c_ulong;
        for i in 0..RESOURCES {
            self.income_total[i] = v.income_total[i] as c_long;
        }
        for k in 0..RING {
            for i in 0..RESOURCES {
                self.income_ring[k][i] = v.income_ring[k][i] as c_long;
            }
        }
        self.income_steps = v.income_steps as c_long;
        for p in 0..PLAYERS {
            self.seen_army[p] = v.seen_army[p] as c_long;
            self.seen_strength[p] = v.seen_strength[p] as c_long;
            self.last_seen[p] = v.last_seen[p] as c_ulong;
            self.seen_town_x[p] = v.seen_town_x[p] as c_long;
            self.seen_town_y[p] = v.seen_town_y[p] as c_long;
            self.seen_citizens[p] = v.seen_citizens[p] as c_long;
            self.army_x[p] = v.army_x[p] as c_long;
            self.army_y[p] = v.army_y[p] as c_long;
        }
        self.events = v
            .events
            .iter()
            .map(|e| Event { kind: e.kind as c_long, unit_type: e.unit_type as c_long, count: e.count as c_long })
            .collect();
        self.script = v.script;
        self.started = v.started;
        self.failed = v.failed;
        self.results = v.results;
        self.spots = spots(&v.spots);
        self.towers = spots(&v.towers);
        self.threats = spots(&v.threats);
        self.threats_time = v.threats_time as c_long;
        self.restore = !v.memory.is_empty();
        self.restore_bytes = v.memory;
    }
}

/// FNV-1a 32: the save line's hash of the memory bytes (`EECPNextHash`)
pub fn fnv32(bytes: &[u8]) -> u32 {
    let mut h = 2166136261u32;
    for &b in bytes {
        h = (h ^ b as u32).wrapping_mul(16777619);
    }
    h
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn roots_and_distances() {
        for v in [0i64, 1, 2, 3, 4, 15, 16, 17, 99, 100, 101, 1 << 40, (1 << 40) + 5] {
            let r = isqrt(v) as i64;
            assert!(r * r <= v && (r + 1) * (r + 1) > v, "{v}");
        }
        assert_eq!(isqrt(-5), 0);
        assert_eq!(d2(1, 2, 4, 6), 25);
        assert!(near(0, 0, 3, 4, 5) && !near(0, 0, 3, 5, 5));
    }

    #[test]
    fn events_merge_and_cap() {
        let mut s = NextState::default();
        s.event_add(0, 7, 1);
        s.event_add(0, 7, 2);
        s.event_add(1, 7, 1);
        assert_eq!(
            s.events,
            vec![Event { kind: 0, unit_type: 7, count: 3 }, Event { kind: 1, unit_type: 7, count: 1 }]
        );
        for k in 0..300 {
            s.event_add(2, k, 1);
        }
        assert_eq!(s.events.len(), 256);
    }

    #[test]
    fn epoch_and_income() {
        let mut s = NextState::default();
        s.note_epoch(0, 500);
        assert_eq!((s.epoch, s.epoch_start), (0, 0));
        s.note_epoch(1, 700);
        assert_eq!((s.epoch, s.epoch_start), (1, 700));
        s.income(0, 10);
        s.income(0, -3);
        s.income(5, 10);
        assert_eq!(s.income_total[0], 10);
        assert_eq!(s.income_last(0), 10);
        s.ring_step();
        s.income(0, 4);
        assert_eq!(s.income_last(0), 14); // the slot of step 1 is still 0
        for _ in 0..59 {
            s.ring_step();
        }
        assert_eq!(s.income_last(0), 4); // 60 steps back: the slot of the first step (10)
    }

    #[test]
    fn threats_are_remembered_and_forgotten() {
        let mut s = NextState::default();
        s.seen.push(Seen { unit: 5, player: 2, x: 10, y: 10, kind: SEEN_SOLDIER, ..Default::default() });
        s.seen.push(Seen { unit: 6, player: 2, x: 40, y: 40, kind: SEEN_CITIZEN, ..Default::default() });
        s.update_threats(1000);
        assert_eq!(s.threats, vec![Spot { player: 2, x: 10, y: 10, time: 1000, unit: 5 }]);
        assert_eq!(s.threat_near(12, 12, true), 5);
        assert_eq!(s.threat_near(30, 30, false), 0);
        s.seen.clear();
        s.update_threats(5000);
        assert_eq!(s.threat_near(12, 12, true), 0);
        assert_eq!(s.threat_near(12, 12, false), 5);
        s.update_threats(21001);
        assert!(s.threats.is_empty());
    }

    #[test]
    fn towers_reach() {
        let mut s = NextState::default();
        s.towers.push(Spot { player: 2, x: 20, y: 20, time: 0, unit: 9 });
        // range 5, half 1: reach 5 + 1 + 3 = 9 from (21, 21)
        let mut look = |u: c_long| if u == 9 { (5, Some(1)) } else { (0, None) };
        assert_eq!(s.tower_at(30, 21, 3, &mut look), 9);
        assert_eq!(s.tower_at(31, 21, 3, &mut look), 0);
        // gone: the guard tower's 6, half 1
        let mut gone = |_: c_long| (0, None);
        assert_eq!(s.tower_at(31, 21, 3, &mut gone), 9);
        s.towers.clear();
        s.seen.push(Seen { unit: 4, x: 0, y: 0, kind: SEEN_BUILDING, ..Default::default() });
        let mut armed = |u: c_long| if u == 4 { (2, Some(2)) } else { (0, None) };
        // range 2, half 2: reach 7 from (2, 2)
        assert_eq!(s.tower_at(9, 2, 3, &mut armed), 4);
        assert_eq!(s.tower_at(10, 2, 3, &mut armed), 0);
    }

    #[test]
    fn spots_and_scan() {
        let mut s = NextState::default();
        s.scan_begin();
        s.note_gatherer(2, 10, 10, 100);
        s.note_gatherer(2, 14, 10, 200); // the same spot
        assert_eq!(s.spots.len(), 1);
        assert_eq!(s.spots[0].time, 200);
        for k in 1..9 {
            s.note_gatherer(2, 10 + 20 * k, 10, 300 + k);
        }
        assert_eq!(s.spots.len(), 8); // the oldest (time 200) replaced
        assert!(s.spots.iter().all(|p| p.time != 200));
        assert_eq!(s.seen_citizens[2], 10);
        s.note_tower(3, 5, 5, 100, 77);
        s.note_tower(3, 5, 5, 150, 77);
        assert_eq!(s.towers, vec![Spot { player: 3, x: 5, y: 5, time: 150, unit: 77 }]);
        let mut army = [(0i64, 0i64, 0 as c_long); PLAYERS];
        army[3] = (30, 41, 2);
        s.scan_end(70000, &army, &mut |_, _| true);
        assert_eq!((s.army_x[3], s.army_y[3]), (15, 20));
        assert!(s.towers.is_empty()); // seen and gone
        assert!(s.spots.is_empty()); // a minute old and seen
        assert_eq!(s.best_spot(2, None), None);
    }

    #[test]
    fn best_spot_skips_towers_and_the_place() {
        let mut s = NextState::default();
        s.spots.push(Spot { player: 2, x: 10, y: 10, time: 5, unit: 0 });
        s.spots.push(Spot { player: 2, x: 50, y: 50, time: 9, unit: 0 });
        assert_eq!(s.best_spot(2, None), Some((50, 50)));
        assert_eq!(s.best_spot(2, Some((52, 52))), Some((10, 10)));
        s.towers.push(Spot { player: 2, x: 12, y: 10, time: 0, unit: 3 });
        assert_eq!(s.best_spot(2, Some((52, 52))), None);
        assert_eq!(s.towers_of(2), 1);
    }

    #[test]
    fn home_pick() {
        let mut s = NextState::default();
        let mut found: [Vec<(i64, c_long)>; RESOURCES] = Default::default();
        found[0] = vec![(9, 30), (4, 31), (1, 32)];
        found[1] = vec![(1, 40)];
        found[2] = vec![(5, 50), (2, 51)];
        found[3] = vec![(3, 60)];
        s.set_home(found);
        assert_eq!(s.home, vec![31, 32, 51, 60]);
        assert!(s.is_home(51) && !s.is_home(50));
    }

    #[test]
    fn save_round_trip() {
        let mut s = NextState::default();
        s.epoch = 2;
        s.event_add(1, 3, 4);
        s.spots.push(Spot { player: 2, x: 1, y: 2, time: 3, unit: 0 });
        s.threats.push(Spot { player: 2, x: 5, y: 6, time: 7, unit: 8 });
        s.script = b"boom".to_vec();
        s.started = true;
        let b = crate::ai::next_save::encode(&s.to_save(b"mem"));
        let mut t = NextState::default();
        t.from_save(crate::ai::next_save::decode(&b, 200).unwrap());
        assert_eq!(crate::ai::next_save::encode(&t.to_save(&t.restore_bytes.clone())), b);
        assert!(t.restore && t.restore_bytes == b"mem" && t.events == s.events && t.army_x[0] == -1);
        assert_eq!(fnv32(b""), 2166136261);
    }
}
