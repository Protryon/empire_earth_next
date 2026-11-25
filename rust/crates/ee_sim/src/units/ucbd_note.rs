//! TEST ONLY: the unit classes' bodies' record (UCBD; ee/EEUnitClassKinds.h `EEUCNote`; the units' lane, round 9:
//! its state and fold in Rust, the notes' words still gathered by the C++ wrappers). The outermost note of a thread
//! folds its words into the physics tick's FNV-1a 64; the record (the tick, the calls, the digest) is written when a
//! later tick's first note ends. Every 20,000 ticks the kinds' counts go to stderr (`EE-UCBD-COUNT`). Nothing is
//! noted unless the capture runs.

use std::cell::Cell;
use std::sync::Mutex;

/// "UCBD"
pub const UCBD: u32 = 0x4442_4355;
const FNV_OFFSET: u64 = 0xcbf2_9ce4_8422_2325;
const FNV_PRIME: u64 = 0x0000_0100_0000_01b3;

thread_local! {
    /// The thread's open notes.
    static DEPTH: Cell<i32> = const { Cell::new(0) };
}

/// The folded notes of the current physics tick.
struct State {
    digest: u64,
    calls: u32,
    tick: u32,
    kinds: [u32; 64],
}

static STATE: Mutex<State> = Mutex::new(State { digest: FNV_OFFSET, calls: 0, tick: 0, kinds: [0; 64] });

/// The world a note asks: whether the capture runs, the physics' ticks done, a record written.
pub trait UcbdWorld {
    fn active(&mut self) -> bool;
    fn ticks(&mut self) -> u32;
    fn record(&mut self, tick: u32, calls: u32, digest: u64);
    fn counts(&mut self, tick: u32, kinds: &[u32; 64]);
}

/// A note opens: whether it records (the thread's outermost, with the capture running).
pub fn begin<W: UcbdWorld + ?Sized>(w: &mut W) -> bool {
    let depth = DEPTH.with(|d| {
        d.set(d.get() + 1);
        d.get()
    });
    depth == 1 && w.active()
}

/// A note closes (`on` its `begin`'s answer): its words folded into the tick's digest (the last tick's record
/// written first when the tick moved on).
pub fn end<W: UcbdWorld + ?Sized>(w: &mut W, on: bool, words: &[u32]) {
    DEPTH.with(|d| d.set(d.get() - 1));
    if !on {
        return;
    }
    let mut state = STATE.lock().unwrap_or_else(|e| e.into_inner());
    let tick = w.ticks();
    if tick != state.tick {
        if state.calls != 0 {
            w.record(state.tick, state.calls, state.digest);
        }
        if tick / 20000 != state.tick / 20000 {
            w.counts(tick, &state.kinds);
        }
        state.digest = FNV_OFFSET;
        state.calls = 0;
        state.tick = tick;
    }
    if let Some(&kind) = words.first() {
        state.kinds[(kind & 63) as usize] += 1;
    }
    for &word in words {
        for b in 0..4 {
            state.digest ^= u64::from((word >> (8 * b)) & 0xff);
            state.digest = state.digest.wrapping_mul(FNV_PRIME);
        }
    }
    state.calls += 1;
}

/// What a note reads of the world: the units' ids, the classes' types, a unit's path, location and goal, the C++
/// vectors' elements (the game: the views; the tests: a mock).
pub trait NoteWorld {
    /// `((const RTSUnit*)unit)->mUnitID` of a live unit
    fn unit_id(&mut self, unit: u64) -> u32;
    /// `class->mUnitType` of a live class
    fn class_type(&mut self, class: u64) -> u32;
    /// the complex unit's waypoints: location x, y, z, distance, start distance
    fn path(&mut self, unit: u64) -> Vec<[f32; 5]>;
    /// `mPhysicalModel.GetLocation()`'s x, y, z
    fn location(&mut self, unit: u64) -> [f32; 3];
    /// `mGoalState`: its kind and, a location (or heal) goal's, its point
    fn goal(&mut self, unit: u64) -> Option<(u8, Option<[f32; 3]>)>;
    /// a `std::vector<U2DPoint<long> >`'s tiles
    fn tiles(&mut self, v: u64) -> Vec<[i32; 2]>;
    /// a `std::vector<EEUnit*>`'s (or `<EEComplexUnit*>`'s) units
    fn units(&mut self, v: u64) -> Vec<u64>;
    /// the unit's `mHomeBase` (0: none)
    fn home_base(&mut self, unit: u64) -> u64;
    /// the unit's `mResourceAmount` and `mResourceType`
    fn resource(&mut self, unit: u64) -> (i32, u32);
}

/// A note (`EEUCNote`): its words gathered while it is on (the thread's outermost note with the capture running), in
/// the order the C++ wrote them; [`Note::end`] folds them.
pub struct Note {
    on: bool,
    words: Vec<u32>,
}

impl Note {
    /// `EEUCNote theNote(kind)`.
    pub fn begin<W: UcbdWorld + ?Sized>(w: &mut W, kind: u32) -> Note {
        let mut n = Note { on: begin(w), words: Vec::new() };
        n.i(i64::from(kind));
        n
    }
    pub fn is_on(&self) -> bool {
        self.on
    }
    pub fn words(&self) -> &[u32] {
        &self.words
    }
    /// `I`: a value (its low 32 bits)
    pub fn i(&mut self, v: i64) -> &mut Self {
        if self.on {
            self.words.push(v as u32);
        }
        self
    }
    /// `F`: a float's bits
    pub fn f(&mut self, v: f32) -> &mut Self {
        self.i(i64::from(v.to_bits()))
    }
    /// `P`: a point
    pub fn p(&mut self, p: [f32; 3]) -> &mut Self {
        self.f(p[0]).f(p[1]).f(p[2])
    }
    /// `T`: a tile
    pub fn t(&mut self, t: [i32; 2]) -> &mut Self {
        self.i(i64::from(t[0])).i(i64::from(t[1]))
    }
    /// `U`: a unit's id (-2: none)
    pub fn u<N: NoteWorld + ?Sized>(&mut self, n: &mut N, unit: u64) -> &mut Self {
        if !self.on {
            return self;
        }
        let v = if unit == 0 { -2 } else { i64::from(n.unit_id(unit)) };
        self.i(v)
    }
    /// `C`: a class's unit type (-2: none)
    pub fn c<N: NoteWorld + ?Sized>(&mut self, n: &mut N, class: u64) -> &mut Self {
        if !self.on {
            return self;
        }
        let v = if class == 0 { -2 } else { i64::from(n.class_type(class)) };
        self.i(v)
    }
    /// `Ts`: a tile vector's length and tiles
    pub fn ts<N: NoteWorld + ?Sized>(&mut self, n: &mut N, v: u64) -> &mut Self {
        if !self.on {
            return self;
        }
        let tiles = n.tiles(v);
        self.i(tiles.len() as i64);
        for t in tiles {
            self.t(t);
        }
        self
    }
    /// `Us`: a unit vector's length and ids
    pub fn us<N: NoteWorld + ?Sized>(&mut self, n: &mut N, v: u64) -> &mut Self {
        if !self.on {
            return self;
        }
        let units = n.units(v);
        self.i(units.len() as i64);
        for u in units {
            self.u(n, u);
        }
        self
    }
    /// `S`: a unit's path, location, goal kind (-1: none) and a location goal's point (-2: no unit)
    pub fn s<N: NoteWorld + ?Sized>(&mut self, n: &mut N, unit: u64) -> &mut Self {
        if !self.on || unit == 0 {
            return self.i(-2);
        }
        let path = n.path(unit);
        self.i(path.len() as i64);
        for w in path {
            self.f(w[0]).f(w[1]).f(w[2]).f(w[3]).f(w[4]);
        }
        let at = n.location(unit);
        self.p(at);
        match n.goal(unit) {
            None => {
                self.i(-1);
            }
            Some((kind, point)) => {
                self.i(i64::from(kind));
                if let Some(p) = point {
                    self.p(p);
                }
            }
        }
        self
    }
    /// `U` of the unit's home base
    pub fn home<N: NoteWorld + ?Sized>(&mut self, n: &mut N, unit: u64) -> &mut Self {
        if !self.on {
            return self;
        }
        let base = n.home_base(unit);
        self.u(n, base)
    }
    /// `I(mResourceAmount).I((long)mResourceType)`
    pub fn resource<N: NoteWorld + ?Sized>(&mut self, n: &mut N, unit: u64) -> &mut Self {
        if !self.on {
            return self;
        }
        let (amount, kind) = n.resource(unit);
        self.i(i64::from(amount)).i(i64::from(kind))
    }
    /// The note's end (`~EEUCNote`): its words folded.
    pub fn end<W: UcbdWorld + ?Sized>(self, w: &mut W) {
        end(w, self.on, &self.words);
    }
}

#[cfg(not(test))]
pub(crate) use game::Game as UcbdGame;

#[cfg(not(test))]
mod game {
    use super::*;

    /// The capture and the physics' ticks (the notes' world).
    pub(crate) struct Game;

    impl UcbdWorld for Game {
        fn active(&mut self) -> bool {
            crate::app::capture_record::active()
        }
        fn ticks(&mut self) -> u32 {
            // the physics' ticks done (the unit utilities' note host, EEFOFindNewTargetNearOldTarget.cpp)
            crate::units::uu_note::host(1, 0, 0, 0, 0)
        }
        fn record(&mut self, tick: u32, calls: u32, digest: u64) {
            let mut r = crate::app::capture_record::Record::new(UCBD);
            r.u32(tick);
            r.u32(calls);
            r.u32(digest as u32);
            r.u32((digest >> 32) as u32);
            r.commit();
        }
        fn counts(&mut self, tick: u32, kinds: &[u32; 64]) {
            let mut line = format!("EE-UCBD-COUNT {tick}");
            for (i, n) in kinds.iter().enumerate().take(50).skip(1) {
                line.push_str(&format!(" {i}:{n}"));
            }
            eprintln!("{line}");
        }
    }
}

#[cfg(test)]
#[path = "ucbd_note_tests.rs"]
mod tests;
