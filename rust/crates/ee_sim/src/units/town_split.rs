//! The towns' save host's compound operations as one-statement C++ operations (the world lane, round 2: were
//! EEPlayer.cpp's `EETownSaveHost` cases): the walks of town_save.rs keep asking `EE_UNITS_TOWN_GET`,
//! `EE_UNITS_TOWN_MAKE` and the like; in the game [`TownSplit`] makes each the C++ statements it was, the player's and
//! the town's resolution, the loops and the made towns' list in Rust, the town objects' reads and writes through the
//! player's body host (EEPlayer.cpp's `ee_units_player_body_host`, one statement a case), the file's primitives
//! through save_prim.rs.

use std::os::raw::c_long;

use crate::units::player_rest::{
    EE_UNITS_PLAYER_BODY_HULL_COUNT, EE_UNITS_PLAYER_BODY_HULL_POINT, EE_UNITS_PLAYER_BODY_TOWN_ADD_AREA,
    EE_UNITS_PLAYER_BODY_TOWN_CONTINENT, EE_UNITS_PLAYER_BODY_TOWN_SERIAL,
};
use crate::units::save_prim::FilePrims;
use crate::units::town_save::{
    EeUnitsTownIo, TownHost, EE_UNITS_TOWN_AREA, EE_UNITS_TOWN_GET, EE_UNITS_TOWN_HULL_POINT, EE_UNITS_TOWN_MAKE,
    EE_UNITS_TOWN_MAKE_COMMIT, EE_UNITS_TOWN_MAKE_DISCARD, EE_UNITS_TOWN_PLAYERS, EE_UNITS_TOWN_READ_BYTES,
    EE_UNITS_TOWN_REINDEX, EE_UNITS_TOWN_REORDER, EE_UNITS_TOWN_SET_SAVED, EE_UNITS_TOWN_SLOTS,
    EE_UNITS_TOWN_WRITE_BYTES,
};

// ---- the towns' host's own one-statement operations (EETownSaveHost; the object in `prim.i`, the answer too)
/// the context's player (`mPlayer`, 0 for the save's and the load's)
pub const EE_UNITS_TOWN_X_CONTEXT_PLAYER: i32 = 320;
/// `gWorld.mPlayers[player]`
pub const EE_UNITS_TOWN_X_WORLD_PLAYER: i32 = 321;
/// the context's area bytes cleared, town `prim.i`'s area written into them (`EEWriteAreaBytes`), their address and
/// size
pub const EE_UNITS_TOWN_X_AREA_CLEAR: i32 = 322;
pub const EE_UNITS_TOWN_X_AREA_DATA: i32 = 323;
pub const EE_UNITS_TOWN_X_AREA_SIZE: i32 = 324;
pub const EE_UNITS_TOWN_X_AREA_WRITE: i32 = 325;
/// the context's file (an `FSFile*`)
pub const EE_UNITS_TOWN_X_FILE: i32 = 326;

// ---- the player's body host's town operations (player_rest.rs's numbering; `a` the town)
/// `mTowns.size()`, `mTowns[b]`
pub const EE_UNITS_PLAYER_BODY_TOWN_SLOTS: i32 = 244;
pub const EE_UNITS_PLAYER_BODY_TOWN_SLOT: i32 = 245;
/// town `a`'s `mTownID` and its store (`b`)
pub const EE_UNITS_PLAYER_BODY_TOWN_ID: i32 = 246;
pub const EE_UNITS_PLAYER_BODY_TOWN_SET_ID: i32 = 247;
/// town `a`'s counter `b` (mValue, mNumWalls, mL80, mNumWonders, mNumDocks, mNumAirports, mNumUnits, mReach) and its
/// store (`c`)
pub const EE_UNITS_PLAYER_BODY_TOWN_COUNTER: i32 = 248;
pub const EE_UNITS_PLAYER_BODY_TOWN_SET_COUNTER: i32 = 249;
/// town `a`'s `mNearWater`, `mNearMapEdge` and their stores (`b`)
pub const EE_UNITS_PLAYER_BODY_TOWN_NEAR_WATER: i32 = 250;
pub const EE_UNITS_PLAYER_BODY_TOWN_SET_NEAR_WATER: i32 = 251;
pub const EE_UNITS_PLAYER_BODY_TOWN_NEAR_EDGE: i32 = 252;
pub const EE_UNITS_PLAYER_BODY_TOWN_SET_NEAR_EDGE: i32 = 253;
/// town `a`'s `mContinent = b`
pub const EE_UNITS_PLAYER_BODY_TOWN_SET_CONTINENT: i32 = 254;
/// `new EETown(this, b)`, `delete` town `a`
pub const EE_UNITS_PLAYER_BODY_TOWN_NEW: i32 = 255;
pub const EE_UNITS_PLAYER_BODY_TOWN_DELETE: i32 = 256;
/// `mTowns.push_back(a)`, `mTowns.clear()`
pub const EE_UNITS_PLAYER_BODY_TOWNS_PUSH: i32 = 257;
pub const EE_UNITS_PLAYER_BODY_TOWNS_CLEAR: i32 = 258;
/// `EEReadAreaBytes(the b bytes at c, *town a)` (b and c: an address and a length)
pub const EE_UNITS_PLAYER_BODY_TOWN_READ_AREA: i32 = 259;

/// The towns' C++: its own host's operations, the player's body host, the file.
pub trait TownSplitHost {
    fn town(&mut self, op: i32, player: u32, index: u32, io: &mut EeUnitsTownIo) -> i32;
    fn body(&mut self, op: i32, player: u64, a: u64, b: i64, c: i64) -> i64;
    fn file(&mut self) -> Box<dyn FilePrims + '_>;
}

/// The made towns (`EE_UNITS_TOWN_MAKE` .. `COMMIT`) and the host.
pub struct TownSplit<H: TownSplitHost> {
    pub h: H,
    pub made: Vec<u64>,
}

const COUNTERS: usize = 8;

impl<H: TownSplitHost> TownSplit<H> {
    pub fn new(h: H) -> Self {
        TownSplit { h, made: Vec::new() }
    }
    /// a one-statement operation on `object` (`prim.i`): its answer (`prim.i` out)
    fn ask(&mut self, op: i32, object: i64) -> i64 {
        let mut io = EeUnitsTownIo::default();
        io.prim.i = object;
        self.h.town(op, 0, 0, &mut io);
        io.prim.i
    }
    /// the context's player, else the world's player `index` (0 past the players)
    fn player(&mut self, index: u32) -> u64 {
        let p = self.ask(EE_UNITS_TOWN_X_CONTEXT_PLAYER, 0) as u64;
        if p != 0 {
            return p;
        }
        let mut io = EeUnitsTownIo::default();
        self.h.town(EE_UNITS_TOWN_PLAYERS, 0, 0, &mut io);
        if (index as i64) < io.prim.i {
            self.ask(EE_UNITS_TOWN_X_WORLD_PLAYER, index as i64) as u64
        } else {
            0
        }
    }
    /// the player's town in slot `index` (0 for none)
    fn town(&mut self, p: u64, index: u32) -> u64 {
        if p != 0 && (index as i64) < self.h.body(EE_UNITS_PLAYER_BODY_TOWN_SLOTS, p, 0, 0, 0) {
            self.h.body(EE_UNITS_PLAYER_BODY_TOWN_SLOT, p, 0, index as i64, 0) as u64
        } else {
            0
        }
    }
    fn set_saved(&mut self, p: u64, town: u64, io: &EeUnitsTownIo) {
        for c in 0..COUNTERS {
            self.h.body(EE_UNITS_PLAYER_BODY_TOWN_SET_COUNTER, p, town, c as i64, io.counters[c] as i64);
        }
        self.h.body(EE_UNITS_PLAYER_BODY_TOWN_SET_NEAR_WATER, p, town, i64::from(io.near_water != 0), 0);
        self.h.body(EE_UNITS_PLAYER_BODY_TOWN_SET_NEAR_EDGE, p, town, i64::from(io.near_map_edge != 0), 0);
    }
}

impl<H: TownSplitHost> TownHost for TownSplit<H> {
    fn call(&mut self, op: i32, player: u32, index: u32, io: &mut EeUnitsTownIo) -> i32 {
        if crate::units::save_prim::is_primitive(op) {
            let mut file = self.h.file();
            return crate::units::save_prim::primitive(&mut *file, op, &mut io.prim);
        }
        match op {
            EE_UNITS_TOWN_SLOTS => {
                let p = self.player(player);
                if p != 0 {
                    self.h.body(EE_UNITS_PLAYER_BODY_TOWN_SLOTS, p, 0, 0, 0) as i32
                } else {
                    0
                }
            }
            EE_UNITS_TOWN_GET => {
                let p = self.player(player);
                let town = self.town(p, index);
                if town == 0 {
                    return 0;
                }
                io.id = self.h.body(EE_UNITS_PLAYER_BODY_TOWN_ID, p, town, 0, 0) as c_long;
                io.points = self.h.body(EE_UNITS_PLAYER_BODY_HULL_COUNT, p, town, 0, 0) as u32;
                for c in 0..COUNTERS {
                    io.counters[c] = self.h.body(EE_UNITS_PLAYER_BODY_TOWN_COUNTER, p, town, c as i64, 0) as c_long;
                }
                io.near_water = self.h.body(EE_UNITS_PLAYER_BODY_TOWN_NEAR_WATER, p, town, 0, 0) as u8;
                io.near_map_edge = self.h.body(EE_UNITS_PLAYER_BODY_TOWN_NEAR_EDGE, p, town, 0, 0) as u8;
                io.continent = self.h.body(EE_UNITS_PLAYER_BODY_TOWN_CONTINENT, p, town, 0, 0) as c_long;
                1
            }
            EE_UNITS_TOWN_HULL_POINT => {
                let p = self.player(player);
                let town = self.town(p, index);
                let point = self.h.body(EE_UNITS_PLAYER_BODY_HULL_POINT, p, town, io.prim.i as u32 as i64, 0);
                io.x = (point >> 32) as i32 as c_long;
                io.y = point as i32 as c_long;
                0
            }
            EE_UNITS_TOWN_AREA => {
                let p = self.player(player);
                let town = self.town(p, index) as i64;
                self.ask(EE_UNITS_TOWN_X_AREA_CLEAR, 0);
                self.ask(EE_UNITS_TOWN_X_AREA_WRITE, town);
                io.len = self.ask(EE_UNITS_TOWN_X_AREA_SIZE, 0) as usize;
                io.bytes = if io.len == 0 {
                    std::ptr::null_mut()
                } else {
                    self.ask(EE_UNITS_TOWN_X_AREA_DATA, 0) as usize as *mut u8
                };
                0
            }
            EE_UNITS_TOWN_READ_BYTES => {
                let mut read: std::os::raw::c_ulong = 0;
                let r = self.h.file().read_bytes(io.bytes, io.len as std::os::raw::c_ulong, &mut read);
                io.prim.i = read as i64;
                r as i32
            }
            EE_UNITS_TOWN_WRITE_BYTES => {
                let mut written: std::os::raw::c_ulong = 0;
                self.h.file().write_bytes(io.bytes, io.len as std::os::raw::c_ulong, &mut written) as i32
            }
            EE_UNITS_TOWN_MAKE => {
                let p = self.player(player);
                let town = self.h.body(EE_UNITS_PLAYER_BODY_TOWN_NEW, p, 0, io.id as i64, 0) as u64;
                if self.h.body(EE_UNITS_PLAYER_BODY_TOWN_READ_AREA, p, town, io.bytes as i64, io.len as i64) == 0 {
                    self.h.body(EE_UNITS_PLAYER_BODY_TOWN_DELETE, p, town, 0, 0);
                    return 0;
                }
                self.h.body(EE_UNITS_PLAYER_BODY_TOWN_SET_CONTINENT, p, town, io.continent as u16 as i64, 0);
                self.set_saved(p, town, io);
                self.made.push(town);
                1
            }
            EE_UNITS_TOWN_MAKE_DISCARD => {
                let p = self.player(player);
                for town in std::mem::take(&mut self.made) {
                    self.h.body(EE_UNITS_PLAYER_BODY_TOWN_DELETE, p, town, 0, 0);
                }
                0
            }
            EE_UNITS_TOWN_MAKE_COMMIT => {
                let p = self.player(player);
                for town in std::mem::take(&mut self.made) {
                    self.h.body(EE_UNITS_PLAYER_BODY_TOWN_SERIAL, p, town, 0, 0);
                    self.h.body(EE_UNITS_PLAYER_BODY_TOWN_ADD_AREA, p, town, 0, 0);
                    self.h.body(EE_UNITS_PLAYER_BODY_TOWNS_PUSH, p, town, 0, 0);
                }
                0
            }
            EE_UNITS_TOWN_SET_SAVED => {
                let p = self.player(player);
                let town = self.town(p, index);
                self.h.body(EE_UNITS_PLAYER_BODY_TOWN_SET_ID, p, town, io.id as i64, 0);
                self.set_saved(p, town, io);
                0
            }
            // (the towns by id: written, never read; gone)
            EE_UNITS_TOWN_REINDEX => 0,
            EE_UNITS_TOWN_REORDER => {
                let p = self.player(player);
                let slots: Vec<i32> = if io.len == 0 || io.bytes.is_null() {
                    Vec::new()
                } else {
                    // SAFETY: the walk's `len` slots at `bytes`
                    unsafe { std::slice::from_raw_parts(io.bytes as *const i32, io.len) }.to_vec()
                };
                let order: Vec<i64> =
                    slots.iter().map(|&s| self.h.body(EE_UNITS_PLAYER_BODY_TOWN_SLOT, p, 0, s as i64, 0)).collect();
                self.h.body(EE_UNITS_PLAYER_BODY_TOWNS_CLEAR, p, 0, 0, 0);
                for town in order {
                    self.h.body(EE_UNITS_PLAYER_BODY_TOWNS_PUSH, p, town as u64, 0, 0);
                }
                0
            }
            _ => self.h.town(op, player, index, io),
        }
    }
}

/// The game's towns' host: EETownSaveHost (`f` over `ctx`), the player's body host, the context's file.
#[cfg(not(test))]
pub mod game {
    use super::*;
    use crate::units::town_save::EeUnitsTownFn;
    use std::ffi::c_void;

    pub struct GameTowns {
        pub ctx: *mut c_void,
        pub f: EeUnitsTownFn,
    }

    impl TownSplitHost for GameTowns {
        fn town(&mut self, op: i32, player: u32, index: u32, io: &mut EeUnitsTownIo) -> i32 {
            // SAFETY: the C++ host's contract (ee_units_towns_*)
            unsafe { (self.f)(self.ctx, op, player, index, io) }
        }
        fn body(&mut self, op: i32, player: u64, a: u64, b: i64, c: i64) -> i64 {
            crate::units::player_body::body_host(op, player, a, b, c)
        }
        fn file(&mut self) -> Box<dyn FilePrims + '_> {
            let mut io = EeUnitsTownIo::default();
            self.town(EE_UNITS_TOWN_X_FILE, 0, 0, &mut io);
            Box::new(crate::units::save_prim::game::GameFile(io.prim.i as usize as *mut c_void))
        }
    }

    /// The towns' host of a save, a load or a repair.
    pub fn host(ctx: *mut c_void, f: EeUnitsTownFn) -> TownSplit<GameTowns> {
        TownSplit::new(GameTowns { ctx, f })
    }
}

#[cfg(test)]
#[path = "town_split_tests.rs"]
mod tests;
