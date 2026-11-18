//! The per-unit Lua interface's state (`EECPUnitsState`, EECPNextUnits.cpp before; docs/AI_LUA_UNITS.md): the
//! held set, the events, the swap forwarding, the enemy memory, the reflex switches, the per-step limits, the danger
//! layers (threat, lost, influence) and the route over their cells, and the save block's state. Rust owns it; the C++
//! holds it as `EEComputerPlayer::mUnitsState` (a `*mut UnitsState`) and calls `units_ffi`.
//!
//! Everything here is plain data and arithmetic: the game's reads come in as arguments (`units_game.rs` reads them
//! through the world views). Values keep the C++'s `long` (`c_long`: 32 bits on Windows), and the containers keep the
//! C++'s orders: `std::set`/`std::map` are `BTreeSet`/`BTreeMap` (the same key order), the vectors stay vectors.
//! The route's open list pops its distinct (cost, cell) pairs in ascending order whatever the heap, so a
//! `BinaryHeap` pops them as `std::pop_heap` with `std::greater` did.

// c_long is i32 on Windows and i64 on LP64: the casts to i32 are the save's, needed on the one
#![allow(clippy::unnecessary_cast)]

use std::cmp::Reverse;
use std::collections::{BTreeMap, BTreeSet, BinaryHeap};
use std::ffi::{c_long, CString};

use crate::ai::units_lua::EeAiUnitsSortEntry;
use crate::ai::units_save::{self, EeAiUnitsEvent, EeAiUnitsLimits, EeAiUnitsMark, EeAiUnitsRemembered, UnitsSave};

pub const EV_ATTACKED: c_long = 1;
pub const EV_DIED: c_long = 2;
pub const EV_CREATED: c_long = 3;
pub const EV_BUILT: c_long = 4;
pub const EV_IDLE: c_long = 5;
pub const EV_SWAPPED: c_long = 6;
pub const EV_CONVERTED: c_long = 7;
pub const EV_ATTACK_PLAYER: c_long = 8;
pub const EV_DEFEND_PLAYER: c_long = 9;
pub const EV_FLARE: c_long = 10;
/// The events' names by kind (`kEventNames`).
pub const EVENT_NAMES: [&std::ffi::CStr; 11] = [
    c"",
    c"attacked",
    c"died",
    c"created",
    c"built",
    c"idle",
    c"swapped",
    c"converted",
    c"attack_player",
    c"defend_player",
    c"flare",
];

/// The danger layers' and the route's cell (tiles).
pub const DANGER_CELL: c_long = 4;
/// The lost layer remembers a bad trade this long (Age of AI's 20 minutes).
pub const LOST_MS: c_long = 1_200_000;
/// (minute, cell) records of the lost layer.
pub const MAX_TRADES: usize = 1024;
/// The influence layer's spread (cells).
pub const INFLUENCE_SPREAD: c_long = 3;
pub const MAX_EVENTS: usize = 512;
pub const MAX_REMEMBERED: usize = 4000;
/// The micro step's period (game ms).
pub const MICRO_MS: c_long = 250;
pub const MICRO_BUDGET: i64 = 300_000;
/// A swap's forwarding entry lives 10 minutes.
pub const FORWARD_MS: c_long = 600_000;

/// One queued event (`UnitsEvent`).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct UnitsEvent {
    pub kind: c_long,
    pub unit: c_long,
    pub other: c_long,
    pub unit_type: c_long,
    pub amount: c_long,
    pub time: c_long,
}

/// The enemy memory's entry (`Remembered`, AI_LUA_UNITS §2.5).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Remembered {
    pub player: c_long,
    pub unit_type: c_long,
    pub x: c_long,
    pub y: c_long,
    pub hp: c_long,
    pub seen: c_long,
    pub building: c_long,
}

/// `ai.mark`: the script's own danger.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct DangerMark {
    pub x: c_long,
    pub y: c_long,
    pub r: c_long,
    pub value: c_long,
    pub until: c_long,
}

/// The state (`EECPUnitsState`).
pub struct UnitsState {
    pub held: BTreeSet<c_long>,
    pub events: Vec<UnitsEvent>,
    pub dropped: c_long,
    /// unit -> its index in `events` (one attacked event per delivery)
    pub attacked_at: BTreeMap<c_long, c_long>,
    /// old id -> (new id, game ms)
    pub forward: BTreeMap<c_long, (c_long, c_long)>,
    pub memory: BTreeMap<c_long, Remembered>,
    pub memory_ms: c_long,
    pub memory_next: c_long,
    pub reflex_naval_flee: bool,
    pub reflex_danger: bool,
    /// the engine's citizen militia (EECPRMMilitia); off when the script fights its own
    pub reflex_militia: bool,
    /// the militia and the danger zones act on the script's held citizens too (lib/verbs.lua)
    pub reflex_held: bool,
    /// a binding was called (the save's shadow step cannot replay commands)
    pub used: bool,
    /// -1: not started; 0x7fffffff: the script has no micro
    pub micro_next: c_long,
    pub orders_this_step: c_long,
    pub unit_orders_this_step: c_long,
    /// the game ms of the counters above
    pub step_stamp: c_long,
    /// the threat layer (AI_LUA_UNITS §4.4): DANGER_CELL cells, rebuilt every game second
    pub threat: Vec<c_long>,
    pub danger_w: c_long,
    pub danger_h: c_long,
    /// own citizens hurt: (x + y * 65536, game ms), 2 minutes
    pub hurt: Vec<(c_long, c_long)>,
    pub marks: Vec<DangerMark>,
    /// the lost layer (§4.4): (minute, cell) -> (lost, dealt), 20 minutes
    pub trades: BTreeMap<(c_long, c_long), (c_long, c_long)>,
    pub lost: Vec<c_long>,
    /// own strength less the remembered enemies', spread over INFLUENCE_SPREAD cells
    pub influence: Vec<c_long>,
    // the route's scratch
    pub g: Vec<c_long>,
    pub from: Vec<c_long>,
    pub open: Vec<u8>,
    // the bindings' scratch (members: a Lua error never skips a destructor)
    pub ids: Vec<c_long>,
    pub ids2: Vec<c_long>,
    pub sorted: Vec<EeAiUnitsSortEntry>,
    pub names: BTreeMap<c_long, CString>,
    /// tiles as (x, y) (the continent and wall bindings: units_more.rs), the water sites (distance, y, x)
    pub tiles: Vec<(c_long, c_long)>,
    pub sites: Vec<(c_long, c_long, c_long)>,
}

impl Default for UnitsState {
    fn default() -> Self {
        UnitsState {
            held: BTreeSet::new(),
            events: Vec::new(),
            dropped: 0,
            attacked_at: BTreeMap::new(),
            forward: BTreeMap::new(),
            memory: BTreeMap::new(),
            memory_ms: 180_000,
            memory_next: 0,
            reflex_naval_flee: true,
            reflex_danger: true,
            reflex_militia: true,
            reflex_held: false,
            used: false,
            micro_next: -1,
            orders_this_step: 0,
            unit_orders_this_step: 0,
            step_stamp: -1,
            threat: Vec::new(),
            danger_w: 0,
            danger_h: 0,
            hurt: Vec::new(),
            marks: Vec::new(),
            trades: BTreeMap::new(),
            lost: Vec::new(),
            influence: Vec::new(),
            g: Vec::new(),
            from: Vec::new(),
            open: Vec::new(),
            ids: Vec::new(),
            ids2: Vec::new(),
            sorted: Vec::new(),
            names: BTreeMap::new(),
            tiles: Vec::new(),
            sites: Vec::new(),
        }
    }
}

/// The squared distance (`EECPUnitsD2`).
pub fn d2(x1: c_long, y1: c_long, x2: c_long, y2: c_long) -> c_long {
    let dx = x1.wrapping_sub(x2);
    let dy = y1.wrapping_sub(y2);
    dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy))
}

/// The integer square root as the C++ counted it up (`while ((d + 1) * (d + 1) <= q) d++`).
pub fn isqrt_up(q: c_long) -> c_long {
    let mut d: c_long = 0;
    while (d + 1) * (d + 1) <= q {
        d += 1;
    }
    d
}

/// A layer as an optional string argument names it.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Layer {
    Threat,
    Lost,
    Influence,
}

impl UnitsState {
    /// `EECPUnitsForward`: a swapped unit's id followed (8 steps at most).
    pub fn forward_id(&self, mut id: c_long) -> c_long {
        for _ in 0..8 {
            match self.forward.get(&id) {
                Some(&(new, _)) => id = new,
                None => break,
            }
        }
        id
    }

    /// `EECPUnitsEventAdd`.
    pub fn event_add(
        &mut self,
        kind: c_long,
        unit: c_long,
        other: c_long,
        unit_type: c_long,
        amount: c_long,
        now: c_long,
    ) {
        if self.events.len() >= MAX_EVENTS {
            self.dropped += 1;
            return;
        }
        self.events.push(UnitsEvent { kind, unit, other, unit_type, amount, time: now });
    }

    /// `EECPUnitsTrade` with the unit's tile (`on_map`: `EECPUnitsOnMap`).
    pub fn trade(&mut self, on_map: bool, x: c_long, y: c_long, lost: c_long, dealt: c_long, now: c_long) {
        if self.danger_w <= 0 || !on_map || (lost <= 0 && dealt <= 0) {
            return;
        }
        let cx = x / DANGER_CELL;
        let cy = y / DANGER_CELL;
        if cx < 0 || cy < 0 || cx >= self.danger_w || cy >= self.danger_h {
            return;
        }
        let key = (now / 60000, cy * self.danger_w + cx);
        if !self.trades.contains_key(&key) && self.trades.len() >= MAX_TRADES {
            self.trades.pop_first(); // the oldest minute goes first
        }
        let e = self.trades.entry(key).or_insert((0, 0));
        e.0 = e.0.wrapping_add(lost);
        e.1 = e.1.wrapping_add(dealt);
    }

    /// `EECPUnitsOnDamage`'s bookkeeping once its reads are made: `lost` the trade's own loss (0 for an animal's or
    /// an ally's hit), `attacker` the seen attacker's id (0 none), `citizen` whether the unit is a citizen or a
    /// fishing boat, `(x, y)` its tile when `on_map`.
    #[allow(clippy::too_many_arguments)]
    pub fn on_damage(
        &mut self,
        id: c_long,
        attacker_player: c_long,
        amount: c_long,
        lost: c_long,
        attacker: c_long,
        citizen: bool,
        on_map: bool,
        x: c_long,
        y: c_long,
        now: c_long,
    ) {
        if let Some(&at) = self.attacked_at.get(&id) {
            if at < self.events.len() as c_long
                && self.events[at as usize].unit == id
                && self.events[at as usize].kind == EV_ATTACKED
            {
                let e = &mut self.events[at as usize];
                e.amount = e.amount.wrapping_add(amount);
                if attacker != 0 {
                    e.other = attacker;
                }
                e.unit_type = attacker_player;
                e.time = now;
                self.trade(on_map, x, y, lost, 0, now);
                return;
            }
        }
        self.attacked_at.insert(id, self.events.len() as c_long);
        self.event_add(EV_ATTACKED, id, attacker, attacker_player, amount, now);
        self.trade(on_map, x, y, lost, 0, now);
        if citizen && attacker_player > 0 && on_map && self.hurt.len() < 256 {
            self.hurt.push((x + y * 65536, now));
        }
    }

    /// `EECPUnitsOnSwapped` for one swap (`mine`: one of the two is the player's; `new_mine`: the new one is).
    pub fn on_swap(&mut self, old: c_long, new: c_long, new_mine: bool, new_type: c_long, now: c_long) {
        self.forward.insert(old, (new, now));
        if self.held.remove(&old) && new_mine {
            self.held.insert(new);
        }
        self.event_add(EV_SWAPPED, old, new, new_type, 0, now);
    }

    /// The forwarding entries older than FORWARD_MS go (after a batch of swaps).
    pub fn forward_expire(&mut self, now: c_long) {
        self.forward.retain(|_, v| now.wrapping_sub(v.1) <= FORWARD_MS);
    }

    /// `EECPUnitsWithinLimits` (AI_LUA_UNITS §3.4).
    pub fn within_limits(&mut self, units: c_long, now: c_long) -> bool {
        if self.step_stamp != now {
            self.step_stamp = now;
            self.orders_this_step = 0;
            self.unit_orders_this_step = 0;
        }
        if self.orders_this_step + 1 > 400 || self.unit_orders_this_step + units > 4000 {
            return false;
        }
        self.orders_this_step += 1;
        self.unit_orders_this_step += units;
        true
    }

    /// The layer's vector.
    pub fn layer(&self, layer: Layer) -> &Vec<c_long> {
        match layer {
            Layer::Threat => &self.threat,
            Layer::Lost => &self.lost,
            Layer::Influence => &self.influence,
        }
    }

    /// `EECPUnitsLayerAt`.
    pub fn layer_at(&self, layer: Layer, x: c_long, y: c_long) -> c_long {
        let v = self.layer(layer);
        if x < 0 || y < 0 || v.is_empty() {
            return 0;
        }
        let cx = x / DANGER_CELL;
        let cy = y / DANGER_CELL;
        if cx >= self.danger_w || cy >= self.danger_h {
            return 0;
        }
        v[(cy * self.danger_w + cx) as usize]
    }

    /// `EECPUnitsDangerAdd`: `value` added to every cell whose centre is within `range` tiles of (x, y).
    pub fn danger_add(&mut self, x: c_long, y: c_long, range: c_long, value: c_long) {
        let c = DANGER_CELL;
        let (x0, x1, y0, y1) = ((x - range) / c, (x + range) / c, (y - range) / c, (y + range) / c);
        let mut cy = if y0 < 0 { 0 } else { y0 };
        while cy <= y1 && cy < self.danger_h {
            let mut cx = if x0 < 0 { 0 } else { x0 };
            while cx <= x1 && cx < self.danger_w {
                let dx = cx * c + c / 2 - x;
                let dy = cy * c + c / 2 - y;
                let r = range + c / 2;
                if dx * dx + dy * dy <= r * r {
                    let v = &mut self.threat[(cy * self.danger_w + cx) as usize];
                    *v = if *v + value > 30000 { 30000 } else { *v + value };
                }
                cx += 1;
            }
            cy += 1;
        }
    }

    /// `EECPUnitsInfluenceAdd`.
    pub fn influence_add(&mut self, x: c_long, y: c_long, value: c_long) {
        let cx0 = x / DANGER_CELL;
        let cy0 = y / DANGER_CELL;
        let r = INFLUENCE_SPREAD;
        for cy in cy0 - r..=cy0 + r {
            for cx in cx0 - r..=cx0 + r {
                if cx < 0 || cy < 0 || cx >= self.danger_w || cy >= self.danger_h {
                    continue;
                }
                let dx = if cx > cx0 { cx - cx0 } else { cx0 - cx };
                let dy = if cy > cy0 { cy - cy0 } else { cy0 - cy };
                let d = if dx > dy { dx } else { dy };
                let v = value as i64 * (r + 1 - d) as i64 / (r + 1) as i64;
                let at = (cy * self.danger_w + cx) as usize;
                let sum = self.influence[at] as i64 + v;
                self.influence[at] = if sum > 1_000_000_000 {
                    1_000_000_000
                } else if sum < -1_000_000_000 {
                    -1_000_000_000
                } else {
                    sum as c_long
                };
            }
        }
    }

    /// `EECPUnitsDangerBuild`'s start: the layers sized for the map and the threat cleared.
    pub fn danger_begin(&mut self, map_w: c_long, map_h: c_long) {
        self.danger_w = (map_w + DANGER_CELL - 1) / DANGER_CELL;
        self.danger_h = (map_h + DANGER_CELL - 1) / DANGER_CELL;
        self.threat.clear();
        self.threat.resize((self.danger_w * self.danger_h) as usize, 0);
    }

    /// A remembered enemy's threat (`attack`: its owner's class upgrade's attack, `range` its floored range).
    pub fn danger_remembered(&mut self, r: &Remembered, attack: c_long, range: c_long, now: c_long) {
        if attack <= 0 {
            return;
        }
        let range = range + if r.building != 0 { 2 } else { 1 };
        let age = now - r.seen;
        let mut value = attack;
        if r.building == 0 && age > 0 {
            // a soldier seen a while ago counts less (none after memory_ms)
            let ms = self.memory_ms;
            value = attack * (ms - if age < ms { age } else { ms }) / if ms > 0 { ms } else { 1 };
        }
        if value > 0 {
            self.danger_add(r.x, r.y, if range < 2 { 2 } else { range }, value);
        }
    }

    /// The hurt spots and the marks into the threat layer (the old ones go), then the lost layer.
    pub fn danger_spots_and_lost(&mut self, now: c_long) {
        let mut k = 0;
        while k < self.hurt.len() {
            if now - self.hurt[k].1 > 120_000 {
                self.hurt.remove(k);
                continue;
            }
            let (place, _) = self.hurt[k];
            self.danger_add(place % 65536, place / 65536, 6, 5);
            k += 1;
        }
        let mut k = 0;
        while k < self.marks.len() {
            if now >= self.marks[k].until {
                self.marks.remove(k);
                continue;
            }
            let m = self.marks[k];
            self.danger_add(m.x, m.y, m.r, m.value);
            k += 1;
        }
        // the lost layer: where own units lost more hit points than they dealt in the last LOST_MS, the difference
        // over 10 on the cell and half of it on the cells round it (Age of AI's DANGER_LOST, cover.c)
        let n = (self.danger_w * self.danger_h) as usize;
        self.lost.clear();
        self.lost.resize(n, 0);
        let minute = now / 60000 - LOST_MS / 60000;
        while let Some((&(m, _), _)) = self.trades.first_key_value() {
            if m >= minute {
                break;
            }
            self.trades.pop_first();
        }
        for (&(_, c), &(lost, dealt)) in &self.trades {
            let net = lost - dealt;
            if net <= 0 || c < 0 || c >= self.danger_w * self.danger_h {
                continue;
            }
            let v = net / 10;
            let v = if v > 30000 { 30000 } else { v };
            let cx = c % self.danger_w;
            let cy = c / self.danger_w;
            for dy in -1..=1 {
                for dx in -1..=1 {
                    let nx = cx + dx;
                    let ny = cy + dy;
                    if nx < 0 || ny < 0 || nx >= self.danger_w || ny >= self.danger_h {
                        continue;
                    }
                    let cell = &mut self.lost[(ny * self.danger_w + nx) as usize];
                    let add = if dx == 0 && dy == 0 { v } else { v / 2 };
                    *cell = if *cell + add > 30000 { 30000 } else { *cell + add };
                }
            }
        }
        self.influence.clear();
        self.influence.resize(n, 0);
    }

    /// A remembered enemy's weight on the influence layer (`attack` as in `danger_remembered`).
    pub fn influence_remembered(&mut self, r: &Remembered, attack: c_long, now: c_long) {
        if attack <= 0 {
            return;
        }
        let age = now - r.seen;
        let mut v = r.hp.wrapping_mul(attack);
        if r.building == 0 && age > 0 {
            let ms = self.memory_ms;
            v = (v as i64 * (ms - if age < ms { age } else { ms }) as i64 / if ms > 0 { ms } else { 1 } as i64)
                as c_long;
        }
        if v > 0 {
            self.influence_add(r.x, r.y, -v);
        }
    }

    /// `ai.mark`'s add (the mark is kept; at once in the threat layer when there is one).
    pub fn mark(&mut self, m: DangerMark) {
        self.marks.push(m);
        if self.danger_w > 0 {
            self.danger_add(m.x, m.y, m.r, m.value);
        }
    }

    /// `ai.danger_box`: the highest and the summed value of the cells over the box (in tiles) and the cells read.
    pub fn danger_box(&self, layer: Layer, x1: c_long, y1: c_long, x2: c_long, y2: c_long) -> (c_long, c_long, c_long) {
        let v = self.layer(layer);
        let (mut x1, mut y1, mut x2, mut y2) = (x1 / DANGER_CELL, y1 / DANGER_CELL, x2 / DANGER_CELL, y2 / DANGER_CELL);
        if x1 > x2 {
            std::mem::swap(&mut x1, &mut x2);
        }
        if y1 > y2 {
            std::mem::swap(&mut y1, &mut y2);
        }
        x1 = x1.max(0);
        y1 = y1.max(0);
        if x2 >= self.danger_w {
            x2 = self.danger_w - 1;
        }
        if y2 >= self.danger_h {
            y2 = self.danger_h - 1;
        }
        let (mut max, mut sum, mut cells) = (0 as c_long, 0 as c_long, 0 as c_long);
        for cy in y1..=y2 {
            for cx in x1..=x2 {
                let c = v[(cy * self.danger_w + cx) as usize];
                if c > max {
                    max = c;
                }
                sum = sum.wrapping_add(c);
                cells += 1;
            }
        }
        (max, sum, cells)
    }

    /// `EECPUnitsCellRoute`'s A* over the cells, from (x1, y1) to (x2, y2): 10 a straight step, 14 a diagonal, plus
    /// `avoid` x the threat and `avoid_lost` x the lost layer of the cell entered; `open(cx, cy)` whether a cell is
    /// on the start's area (the goal cell always is). The cells of the way end in `ids` (start to goal) -> the cells
    /// closed, or None: no way.
    #[allow(clippy::too_many_arguments)]
    pub fn cell_route(
        &mut self,
        x1: c_long,
        y1: c_long,
        x2: c_long,
        y2: c_long,
        avoid: c_long,
        avoid_lost: c_long,
        mut cell_open: impl FnMut(c_long, c_long) -> bool,
    ) -> (bool, c_long) {
        let mut expanded: c_long = 0;
        let w = self.danger_w;
        let h = self.danger_h;
        let n = w * h;
        let start = (y1 / DANGER_CELL) * w + x1 / DANGER_CELL;
        let goal = (y2 / DANGER_CELL) * w + x2 / DANGER_CELL;
        if start < 0 || start >= n || goal < 0 || goal >= n {
            return (false, 0);
        }
        if self.lost.len() as c_long != n {
            self.lost.clear();
            self.lost.resize(n as usize, 0);
        }
        self.g.clear();
        self.g.resize(n as usize, -1);
        self.from.clear();
        self.from.resize(n as usize, -1);
        self.open.clear();
        self.open.resize(n as usize, 0); // 2 closed, 3 blocked
        let mut heap: BinaryHeap<Reverse<(c_long, c_long)>> = BinaryHeap::new();
        self.g[start as usize] = 0;
        let gx = goal % w;
        let gy = goal / w;
        heap.push(Reverse((0, start)));
        const STEP: [[c_long; 3]; 8] =
            [[1, 0, 10], [-1, 0, 10], [0, 1, 10], [0, -1, 10], [1, 1, 14], [1, -1, 14], [-1, 1, 14], [-1, -1, 14]];
        let mut found = false;
        while let Some(Reverse((_, c))) = heap.pop() {
            if self.open[c as usize] == 2 {
                continue;
            }
            self.open[c as usize] = 2;
            expanded += 1;
            if c == goal {
                found = true;
                break;
            }
            let cx = c % w;
            let cy = c / w;
            for s in &STEP {
                let nx = cx + s[0];
                let ny = cy + s[1];
                if nx < 0 || ny < 0 || nx >= w || ny >= h {
                    continue;
                }
                let d = ny * w + nx;
                if self.open[d as usize] >= 2 {
                    continue;
                }
                if d != goal && !cell_open(nx, ny) {
                    self.open[d as usize] = 3;
                    continue;
                }
                let g = self.g[c as usize]
                    .wrapping_add(s[2])
                    .wrapping_add(avoid.wrapping_mul(self.threat_or_zero(d)))
                    .wrapping_add(avoid_lost.wrapping_mul(self.lost[d as usize]));
                if self.g[d as usize] >= 0 && self.g[d as usize] <= g {
                    continue;
                }
                self.g[d as usize] = g;
                self.from[d as usize] = c;
                let dx = if nx > gx { nx - gx } else { gx - nx };
                let dy = if ny > gy { ny - gy } else { gy - ny };
                let hh = 10 * if dx > dy { dx } else { dy } + 4 * if dx < dy { dx } else { dy };
                heap.push(Reverse((g + hh, d)));
            }
        }
        if !found {
            return (false, expanded);
        }
        self.ids.clear();
        let mut c = goal;
        while c >= 0 {
            self.ids.push(c);
            c = self.from[c as usize];
        }
        self.ids.reverse();
        (true, expanded)
    }

    // the C++ indexed mThreat[d] directly: the layer always has the cells when the route runs (DangerBuild before)
    fn threat_or_zero(&self, d: c_long) -> c_long {
        self.threat.get(d as usize).copied().unwrap_or(0)
    }

    /// `ai.route`'s waypoints from `ids` (the cells of the way): where the direction changes, at most 15 (every
    /// second corner goes), into `ids2`.
    pub fn route_corners(&mut self) {
        self.ids2.clear();
        let mut k = 1;
        while k + 1 < self.ids.len() {
            let (a, b, c) = (self.ids[k - 1], self.ids[k], self.ids[k + 1]);
            if b - a != c - b {
                self.ids2.push(b);
            }
            k += 1;
        }
        while self.ids2.len() > 15 {
            let mut k = 1;
            while k < self.ids2.len() {
                self.ids2.remove(k);
                k += 1;
            }
        }
    }

    /// The block's state (`EECPUnitsSaveState`).
    pub fn to_save(&self) -> UnitsSave {
        UnitsSave {
            version: 0,
            held: self.held.iter().map(|&v| v as i32).collect(),
            events: self
                .events
                .iter()
                .map(|e| EeAiUnitsEvent {
                    kind: e.kind as i32,
                    unit: e.unit as i32,
                    other: e.other as i32,
                    unit_type: e.unit_type as i32,
                    amount: e.amount as i32,
                    time: e.time as i32,
                })
                .collect(),
            forward: self.forward.iter().map(|(&k, &(a, b))| [k as i32, a as i32, b as i32]).collect(),
            memory: self
                .memory
                .iter()
                .map(|(&id, r)| EeAiUnitsRemembered {
                    id: id as i32,
                    player: r.player as i32,
                    unit_type: r.unit_type as i32,
                    x: r.x as i32,
                    y: r.y as i32,
                    hit_points: r.hp as i32,
                    seen: r.seen as i32,
                    building: r.building as i32,
                })
                .collect(),
            memory_ms: self.memory_ms as i32,
            memory_next: self.memory_next as i32,
            reflex_naval_flee: self.reflex_naval_flee,
            reflex_danger: self.reflex_danger,
            micro_next: self.micro_next as i32,
            hurt: self.hurt.iter().map(|&(a, b)| [a as i32, b as i32]).collect(),
            marks: self
                .marks
                .iter()
                .map(|m| EeAiUnitsMark {
                    x: m.x as i32,
                    y: m.y as i32,
                    r: m.r as i32,
                    value: m.value as i32,
                    until: m.until as i32,
                })
                .collect(),
            trades: self.trades.iter().map(|(&(m, c), &(l, d))| [m as i32, c as i32, l as i32, d as i32]).collect(),
            reflex_militia: self.reflex_militia,
            reflex_held: self.reflex_held,
        }
    }

    /// The "EENU" block (`EECPUnitsSaveState`).
    pub fn save(&self) -> Vec<u8> {
        units_save::encode(&self.to_save())
    }

    /// The state from a block (`EECPUnitsLoadState`; false: not one, the state is then as it was): the counts past
    /// the limits, or an event of no kind, refuse it. The layers rebuild at the next game second.
    pub fn load(&mut self, bytes: &[u8]) -> bool {
        let limits = EeAiUnitsLimits {
            max_events: MAX_EVENTS as i32,
            max_remembered: MAX_REMEMBERED as i32,
            max_trades: MAX_TRADES as i32,
            event_kind_min: EV_ATTACKED as i32,
            event_kind_max: EV_CONVERTED as i32,
        };
        let Some(v) = units_save::decode(bytes, &limits) else {
            return false;
        };
        self.held = v.held.iter().map(|&h| h as c_long).collect();
        self.events.clear();
        self.attacked_at.clear();
        for e in &v.events {
            let e = UnitsEvent {
                kind: e.kind as c_long,
                unit: e.unit as c_long,
                other: e.other as c_long,
                unit_type: e.unit_type as c_long,
                amount: e.amount as c_long,
                time: e.time as c_long,
            };
            if e.kind == EV_ATTACKED {
                self.attacked_at.insert(e.unit, self.events.len() as c_long); // a later hit adds to it
            }
            self.events.push(e);
        }
        self.forward = v.forward.iter().map(|f| (f[0] as c_long, (f[1] as c_long, f[2] as c_long))).collect();
        self.memory = v
            .memory
            .iter()
            .map(|m| {
                (
                    m.id as c_long,
                    Remembered {
                        player: m.player as c_long,
                        unit_type: m.unit_type as c_long,
                        x: m.x as c_long,
                        y: m.y as c_long,
                        hp: m.hit_points as c_long,
                        seen: m.seen as c_long,
                        building: m.building as c_long,
                    },
                )
            })
            .collect();
        self.memory_ms = v.memory_ms as c_long;
        self.memory_next = v.memory_next as c_long;
        self.reflex_naval_flee = v.reflex_naval_flee;
        self.reflex_danger = v.reflex_danger;
        self.reflex_militia = v.reflex_militia; // (a version 1-3 block: the defaults, militia on and held off)
        self.reflex_held = v.reflex_held;
        self.micro_next = v.micro_next as c_long;
        self.hurt = v.hurt.iter().map(|h| (h[0] as c_long, h[1] as c_long)).collect();
        self.marks = v
            .marks
            .iter()
            .map(|m| DangerMark {
                x: m.x as c_long,
                y: m.y as c_long,
                r: m.r as c_long,
                value: m.value as c_long,
                until: m.until as c_long,
            })
            .collect();
        self.trades =
            v.trades.iter().map(|t| ((t[0] as c_long, t[1] as c_long), (t[2] as c_long, t[3] as c_long))).collect();
        self.lost.clear();
        self.influence.clear();
        self.threat.clear();
        self.danger_w = 0;
        self.danger_h = 0; // rebuilt at the next game second
        true
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn forward_follows_eight_steps() {
        let mut s = UnitsState::default();
        for k in 1..20 {
            s.forward.insert(k, (k + 1, 0));
        }
        assert_eq!(s.forward_id(1), 9);
        assert_eq!(s.forward_id(100), 100);
    }

    #[test]
    fn events_stop_at_the_cap_and_count_the_dropped() {
        let mut s = UnitsState::default();
        for k in 0..(MAX_EVENTS + 3) {
            s.event_add(EV_CREATED, k as c_long, 0, 5, 0, 7);
        }
        assert_eq!(s.events.len(), MAX_EVENTS);
        assert_eq!(s.dropped, 3);
    }

    #[test]
    fn a_second_hit_adds_to_the_pending_attacked_event() {
        let mut s = UnitsState::default();
        s.danger_begin(40, 40);
        s.on_damage(5, 2, 10, 10, 77, true, true, 9, 9, 1000);
        s.on_damage(5, 3, 4, 4, 0, true, true, 9, 9, 1500);
        assert_eq!(s.events.len(), 1);
        let e = s.events[0];
        assert_eq!((e.kind, e.unit, e.other, e.unit_type, e.amount, e.time), (EV_ATTACKED, 5, 77, 3, 14, 1500));
        assert_eq!(s.hurt, vec![(9 + 9 * 65536, 1000)]);
        assert_eq!(s.trades.get(&(0, 2 * 10 + 2)), Some(&(14, 0)));
    }

    #[test]
    fn trades_drop_the_oldest_minute_at_the_cap() {
        let mut s = UnitsState::default();
        s.danger_begin(40, 40);
        for k in 0..(MAX_TRADES as c_long + 1) {
            s.trade(true, 0, 0, 1, 0, k * 60000);
        }
        assert_eq!(s.trades.len(), MAX_TRADES);
        assert!(!s.trades.contains_key(&(0, 0)));
    }

    #[test]
    fn danger_adds_saturate_and_cover_a_disc() {
        let mut s = UnitsState::default();
        s.danger_begin(40, 40);
        s.danger_add(20, 20, 4, 20000);
        s.danger_add(20, 20, 4, 20000);
        assert_eq!(s.layer_at(Layer::Threat, 20, 20), 30000);
        assert_eq!(s.layer_at(Layer::Threat, 0, 0), 0);
        let (max, _, cells) = s.danger_box(Layer::Threat, 0, 0, 39, 39);
        assert_eq!((max, cells), (30000, 100));
    }

    #[test]
    fn the_route_goes_round_a_closed_wall() {
        let mut s = UnitsState::default();
        s.danger_begin(40, 40);
        // a wall of closed cells at cx = 5 for cy 0..8
        let (found, expanded) = s.cell_route(2, 20, 38, 20, 0, 0, |cx, cy| !(cx == 5 && cy < 9));
        assert!(found);
        assert!(expanded > 0);
        assert_eq!(*s.ids.first().unwrap(), 5 * 10);
        assert_eq!(*s.ids.last().unwrap(), 5 * 10 + 9);
        assert!(s.ids.contains(&(9 * 10 + 5)));
        s.route_corners();
        assert!(!s.ids2.is_empty());
    }

    #[test]
    fn save_and_load_round_trip() {
        let mut s = UnitsState::default();
        s.held.insert(3);
        s.held.insert(9);
        s.event_add(EV_ATTACKED, 3, 4, 2, 10, 500);
        s.event_add(EV_DIED, 9, 0, 7, 0, 600);
        s.forward.insert(1, (2, 300));
        s.memory.insert(40, Remembered { player: 2, unit_type: 12, x: 5, y: 6, hp: 80, seen: 700, building: 1 });
        s.hurt.push((7, 800));
        s.marks.push(DangerMark { x: 1, y: 2, r: 3, value: 4, until: 5000 });
        s.trades.insert((1, 2), (30, 4));
        s.reflex_militia = false;
        let bytes = s.save();
        let mut t = UnitsState::default();
        assert!(t.load(&bytes));
        assert_eq!(t.save(), bytes);
        assert_eq!(t.attacked_at.get(&3), Some(&0));
        assert!(!t.reflex_militia);
        assert!(!UnitsState::default().load(b"nope"));
    }
}
