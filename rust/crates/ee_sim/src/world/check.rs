//! TEST ONLY: the world views' check (the simulation capture's VWCK record; `EEWorldViewCheck.cpp`). Every physics
//! tick while the capture runs, the C++ folds a fixed list of the world's, every player's, every listed unit's and
//! every map tile's fields, read through its members, into an FNV-1a 64 of 32-bit words; [`digest`] folds the same
//! list read through the views. The C++ stops EE-SIM (exit 3) when the two differ. The list and its order are the
//! C++'s, word for word (a `long` as its low 32 bits, a `bool` as 0 or 1, a float as its bits, a missing object
//! as 0xffffffff where the C++ writes it).

// `c_ulong` and `c_long` are 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::os::raw::{c_long, c_ulong};

use crate::world::view::{MapView, PlayerView, UnitView, WorldRead, WorldView, EE_WORLD_PLAYER_SLOTS};

/// The check's counts and digest.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWorldViewDigest {
    pub players: u32,
    pub units: u32,
    pub tiles: u32,
    pub digest: u64,
}

/// FNV-1a 64 over little-endian 32-bit words.
struct Fold(u64);

impl Fold {
    fn word(&mut self, v: u32) {
        for b in v.to_le_bytes() {
            self.0 ^= u64::from(b);
            self.0 = self.0.wrapping_mul(0x100_0000_01b3);
        }
    }
    fn float(&mut self, v: f32) {
        self.word(v.to_bits());
    }
    fn flag(&mut self, v: bool) {
        self.word(u32::from(v));
    }
}

const NONE: u32 = 0xffff_ffff;

fn unit(f: &mut Fold, u: &UnitView) {
    f.word(u.id());
    let (x, y) = u.tile_xy();
    f.word(x as u32);
    f.word(y as u32);
    f.flag(u.valid());
    f.flag(u.queued_for_delete());
    f.flag(u.node_freed());
    f.word(u32::from(u.kind()));
    let tile = u.tile();
    f.flag(tile.is_some());
    f.word(tile.map_or(0, |t| t.terrain_type() as u32));
    f.word(u.player().map_or(NONE, |p| p.id()));
    if !u.is_unit() {
        return;
    }
    f.word(u.resource_type() as u32);
    f.word(u.hit_points() as u32);
    f.word(u.resource_amount() as u32);
    f.word(u32::from(u.hidden_flags()));
    match u.class() {
        Some(c) => {
            f.word(c.category() as u32);
            f.word(c.unit_type() as u32);
            f.word(c.family() as u32);
            f.float(c.range());
        }
        None => f.word(NONE),
    }
    match u.stats() {
        Some(s) => {
            f.float(s.range());
            f.float(s.scale());
            f.word(s.max_hit_points() as u32);
        }
        None => f.word(NONE),
    }
    let complex = u.complex_unit_handle();
    f.word(if complex == 0 {
        0
    } else if complex == u.handle() {
        1
    } else {
        2
    });
    if !u.is_complex() {
        return;
    }
    f.float(u.height());
    f.float(u.sight());
    f.word(u.facing() as u32);
    f.flag(u.is_built());
    f.flag(u.commandable());
    f.flag(u.cloaked());
    f.word(u32::from(u.experience()));
    f.word(u.formation_slot() as u32);
    f.word(u.power() as u32);
    for v in u.location() {
        f.float(v);
    }
    f.word(u.command_type());
    match u.goal() {
        Some((kind, id)) => {
            f.word(u32::from(kind));
            f.word(id);
        }
        None => f.word(NONE),
    }
    f.word(u.active_goal().map_or(NONE, |(kind, _)| u32::from(kind)));
    let cargo = u.cargo_len();
    f.word(cargo as u32);
    for i in 0..cargo {
        f.word(u.cargo(i).map_or(0, |c| c.id()));
    }
}

fn player(f: &mut Fold, p: &PlayerView) -> u32 {
    f.word(p.id());
    let next: c_ulong = crate::world_field!(p.rts(), mNextUniqueID);
    let count: c_ulong = crate::world_field!(p.rts(), mUnitCount);
    f.word(next as u32);
    f.word(count as u32);
    f.word(u32::from(p.slot()));
    f.flag(p.sees_all());
    f.flag(crate::world_field!(p.rts(), mSaveVisMap));
    f.flag(p.observer());
    for v in p.color() {
        f.float(v);
    }
    f.flag(p.selected_group_handle() != 0);
    for i in 0..17 {
        f.word(p.enemy(i) as u32);
    }
    let caps: [c_long; 7] = crate::world_field!(p.saved(), mResourceCaps);
    for v in caps {
        f.word(v as u32);
    }
    let pop_limit: c_long = crate::world_field!(p.saved(), mPopLimit);
    let population: c_long = crate::world_field!(p.saved(), mPopulationCount);
    let result: c_ulong = crate::world_field!(p.saved(), mGameResult);
    f.word(pop_limit as u32);
    f.word(population as u32);
    f.word(result as u32);
    f.flag(crate::world_field!(p.saved(), mSharedLOS));
    f.float(crate::world_field!(p.fields(), mTributeFee));
    let hero_limit: c_long = crate::world_field!(p.fields(), mHeroLimit);
    let heroes: c_long = crate::world_field!(p.fields(), mNumHeroes);
    f.word(hero_limit as u32);
    f.word(heroes as u32);
    let mut units = 0u32;
    for u in p.units() {
        unit(f, &u);
        units += 1;
    }
    f.word(units);
    units
}

fn map(f: &mut Fold, m: &MapView) -> u32 {
    let size = m.size();
    f.word(size as u32);
    f.word(m.player_count() as u32);
    let height: c_long = crate::world_field!(m.rts(), mL195610);
    let width: c_long = crate::world_field!(m.rts(), mL195614);
    let characteristics: c_ulong = crate::world_field!(m.fields(), mMapCharacteristics);
    f.word(height as u32);
    f.word(width as u32);
    f.word(characteristics as u32);
    let words = m.visibility_words();
    let mut tiles = 0u32;
    for y in 0..size {
        for x in 0..size {
            let Some(t) = m.tile(x, y) else {
                f.word(0);
                continue;
            };
            tiles += 1;
            f.word(1);
            f.word(t.terrain_type() as u32);
            f.float(t.height());
            f.word(i32::from(t.ai_value()) as u32);
            if t.has_visibility() {
                for w in 0..words {
                    f.word(t.visibility_word(w).unwrap_or(0));
                }
            }
            let mut count = 0u32;
            for u in t.units() {
                f.word(u.map_or(0, |u| u.id()));
                count += 1;
            }
            f.word(count);
        }
    }
    tiles
}

/// The VWCK digest of the world `w`, through the views.
pub fn digest(w: &WorldView) -> EeWorldViewDigest {
    let mut f = Fold(0xcbf2_9ce4_8422_2325);
    let mut out = EeWorldViewDigest::default();
    f.word(w.num_players() as u32);
    f.word(w.current_player_index() as u32);
    for i in 0..EE_WORLD_PLAYER_SLOTS {
        let p = w.player(i);
        f.flag(p.is_some());
        if let Some(p) = p {
            out.players += 1;
            out.units += player(&mut f, &p);
        }
    }
    let m = w.map();
    f.flag(m.is_some());
    if let Some(m) = m {
        out.tiles = map(&mut f, &m);
    }
    out.digest = f.0;
    out
}

/// TEST ONLY: the VWCK digest of the world `world` (`&gWorld`) through the views (`EEWorldViewCheck`).
///
/// # Safety
/// `world` is the live world; the caller holds it (the physics thread at its tick); `out` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_world_view_digest(world: u64, out: *mut EeWorldViewDigest) {
    // SAFETY: the caller holds the world
    let token = unsafe { WorldRead::begin() };
    // SAFETY: the caller's contract
    let d = unsafe { WorldView::of(&token, world) }.map_or_else(EeWorldViewDigest::default, |w| digest(&w));
    // SAFETY: as above
    unsafe { *out = d };
}
