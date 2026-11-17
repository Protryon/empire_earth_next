//! The tile sight (the object-storage plan's lane P): `RTSTile::AddSight`/`RemoveSight`/`SetVisibility`, EEPlayer's
//! shared and own sight of a tile, the sight circles and ring steps (EESightRings.cpp), `RTSMap::FogSeen`/
//! `ExploreUnseen` and `EEMap::PrepareSave`'s visibility pass. The C++ bodies in their statement order over a
//! [`SightWorld`]: the game's reads the world views (ee_sim::world; tile_sight_host.rs), the world calls that stay
//! C++ (the doppelganger manager, the mini map, the terrain mesh, the statistics) are its host operations.
//!
//! The visibility words: two players a word, each player's count in its 16-bit half (`GetVisibility` masks the low
//! byte of it).

use crate::units::load::um_floor;
use crate::units::physics::lrintf;
use std::sync::OnceLock;

/// What the tile sight reads and calls (players and tiles are their addresses, 0 none).
pub trait SightWorld {
    /// `gWorld.mMap->GetTile(x, y)` (0 off the map)
    fn tile(&mut self, x: i64, y: i64) -> u64;
    /// the map's `mSize`, its `mU19561C` (the players its words are for)
    fn map_size(&mut self) -> i64;
    fn map_players(&mut self) -> u64;
    /// the tile's `mVisibility[w]` (0 while it has none), and the write
    fn word(&mut self, tile: u64, w: usize) -> u32;
    fn set_word(&mut self, tile: u64, w: usize, value: u32);
    /// the player's `mPlayerID`, `mSharedLOS`, `mEnemies[id]`, `mAllies` (its length, entry i)
    fn player_id(&mut self, player: u64) -> u32;
    fn shared_los(&mut self, player: u64) -> bool;
    fn enemy(&mut self, player: u64, id: u32) -> u64;
    fn allies(&mut self, player: u64) -> usize;
    fn ally(&mut self, player: u64, i: usize) -> u64;
    /// `gWorld.GetCurrentPlayer()`; `gWorld.mPlayers[i]`
    fn current_player(&mut self) -> u64;
    fn world_player(&mut self, i: usize) -> u64;
    /// `gDoppelgangerManager.TileShown(tile, x, y, player)` / `TileHidden(tile, player)`
    fn tile_shown(&mut self, tile: u64, x: i64, y: i64, player: u64);
    fn tile_hidden(&mut self, tile: u64, player: u64);
    /// the player's `mSavedGroups.Reset(0x18, 1)` (a first sighting)
    fn first_sighting(&mut self, player: u64);
    /// `gMiniMap->CountCellOn/Off(x, y)`, then the terrain mesh's `ReferenceTile/DereferenceTile(x, y)`
    fn show(&mut self, on: bool, x: i64, y: i64);
}

/// The largest radius the rings cover (`kMaxSightRadius`)
const MAX_SIGHT_RADIUS: f32 = 32.0;

/// `gSightRings[r]`: the tile offsets at a ceiling distance of r (0..=32) from a centre, built as
/// `EEBuildSightRings` built them (x outer, y inner, -32..=32; the distance through a double `sqrt`).
pub fn sight_rings() -> &'static [Vec<(f32, f32)>; 33] {
    static RINGS: OnceLock<[Vec<(f32, f32)>; 33]> = OnceLock::new();
    RINGS.get_or_init(|| {
        let mut rings: [Vec<(f32, f32)>; 33] = std::array::from_fn(|_| Vec::new());
        for x in -32i32..=32 {
            for y in -32i32..=32 {
                let distance = f64::from(x * x + y * y).sqrt() as f32;
                let mut ring = um_floor(distance);
                if distance != ring as f32 {
                    ring += 1;
                }
                if ring <= 32 {
                    rings[ring as usize].push((x as f32, y as f32));
                }
            }
        }
        rings
    })
}

/// `GetVisibility(id)`
fn visibility<W: SightWorld + ?Sized>(w: &mut W, tile: u64, id: u32) -> u32 {
    (w.word(tile, (id >> 1) as usize) >> ((id & 1) << 4)) & 0xff
}

/// `RTSTile::SetVisibility(value, player)`: the value in the player's half, the other half kept
pub fn set_visibility<W: SightWorld + ?Sized>(w: &mut W, tile: u64, value: u32, player: u64) {
    let id = w.player_id(player);
    let at = (id >> 1) as usize;
    let keep = 0xffffu32 << (((id.wrapping_add(1)) & 1) << 4);
    let word = (w.word(tile, at) & keep) | (value << ((id & 1) << 4));
    w.set_word(tile, at, word);
}

/// `RTSTile::AddSight(player)` (`on`) and `RemoveSight(player)`
pub fn tile_sight<W: SightWorld + ?Sized>(w: &mut W, on: bool, tile: u64, player: u64) {
    let id = w.player_id(player);
    let v = visibility(w, tile, id);
    let at = (id >> 1) as usize;
    let shift = (id & 1) << 4;
    if on {
        if v != 0 {
            let word = w.word(tile, at).wrapping_add(1u32 << shift);
            w.set_word(tile, at, word);
        } else {
            set_visibility(w, tile, 2, player);
        }
    } else if v > 1 {
        let word = w.word(tile, at).wrapping_add((-1i32 << shift) as u32);
        w.set_word(tile, at, word);
    }
}

/// EEPlayer's sight of a tile: `AddSharedSight(tile, x, y)`, `RemoveSharedSight(tile)` (`shared`), `AddOwnSight`,
/// `RemoveOwnSight`
pub fn player_sight<W: SightWorld + ?Sized>(w: &mut W, on: bool, shared: bool, player: u64, tile: u64, x: i64, y: i64) {
    if tile == 0 {
        return;
    }
    if on {
        let id = w.player_id(player);
        if visibility(w, tile, id) == 0 {
            w.first_sighting(player);
        }
    }
    apply(w, on, player, tile, x, y);
    if !shared || !w.shared_los(player) {
        return;
    }
    let count = w.allies(player);
    for i in (0..count).rev() {
        let ally = w.ally(player, i);
        if w.shared_los(ally) {
            apply(w, on, ally, tile, x, y);
        }
    }
}

/// one player's count at the tile and the doppelganger manager's view of it
fn apply<W: SightWorld + ?Sized>(w: &mut W, on: bool, player: u64, tile: u64, x: i64, y: i64) {
    tile_sight(w, on, tile, player);
    if on {
        w.tile_shown(tile, x, y, player);
    } else {
        w.tile_hidden(tile, player);
    }
}

/// `EEPlayer::IsAlliedWith(b)` of `a`
fn allied<W: SightWorld + ?Sized>(w: &mut W, a: u64, b: u64) -> bool {
    if a == b {
        return true;
    }
    let (ia, ib) = (w.player_id(a), w.player_id(b));
    w.enemy(a, ib) == 0 && w.enemy(b, ia) == 0
}

/// whether the current player sees `player`'s sight: it is that player, or an ally both sharing line of sight
fn shows<W: SightWorld + ?Sized>(w: &mut W, player: u64) -> bool {
    let current = w.current_player();
    current == player || (allied(w, current, player) && w.shared_los(current) && w.shared_los(player))
}

/// one ring entry's tile around (cx, cy): its coordinates and handle
fn ring_tile<W: SightWorld + ?Sized>(w: &mut W, offset: (f32, f32), cx: f32, cy: f32) -> (f32, f32, i64, i64, u64) {
    let x = offset.0 + cx;
    let y = offset.1 + cy;
    let tx = i64::from(um_floor(x));
    let ty = i64::from(um_floor(y));
    (x, y, tx, ty, w.tile(tx, ty))
}

/// `EEAddSightCircle` (`on`) / `EERemoveSightCircle(x, y, radius, player)`: every tile within the radius (at most
/// 32)
pub fn circle<W: SightWorld + ?Sized>(w: &mut W, on: bool, x: f32, y: f32, radius: f32, player: u64) {
    let mut r = i64::from(lrintf(radius));
    let show = shows(w, player);
    let max = i64::from(lrintf(MAX_SIGHT_RADIUS));
    if r > max {
        r = max;
    }
    let rings = sight_rings();
    for ring in 0..=r {
        for &offset in &rings[ring as usize] {
            let (_, _, tx, ty, tile) = ring_tile(w, offset, x, y);
            if tile != 0 {
                player_sight(w, on, true, player, tile, tx, ty);
                if show {
                    w.show(on, tx, ty);
                }
            }
        }
    }
}

/// `EEAddSightRingStep` (`on`: the outer two rings around the new centre the old one did not see) and
/// `EERemoveSightRingStep` (around the old centre, the tiles the new one does not see)
#[allow(clippy::too_many_arguments)] // the C++ signature's
pub fn ring_step<W: SightWorld + ?Sized>(
    w: &mut W,
    on: bool,
    old_x: f32,
    old_y: f32,
    new_x: f32,
    new_y: f32,
    radius: f32,
    player: u64,
) {
    let show = shows(w, player);
    let radius = if radius > MAX_SIGHT_RADIUS { MAX_SIGHT_RADIUS } else { radius };
    let r = i64::from(lrintf(radius));
    let squared = r as f32 * r as f32;
    let ((cx, cy), (ox, oy)) = if on { ((new_x, new_y), (old_x, old_y)) } else { ((old_x, old_y), (new_x, new_y)) };
    let rings = sight_rings();
    for ring in r - 1..=r {
        if ring < 0 {
            continue;
        }
        for &offset in &rings[ring as usize] {
            let (x, y, tx, ty, tile) = ring_tile(w, offset, cx, cy);
            if tile != 0 {
                let dx = x - ox;
                let dy = y - oy;
                if dx * dx + dy * dy > squared {
                    player_sight(w, on, true, player, tile, tx, ty);
                    if show {
                        w.show(on, tx, ty);
                    }
                }
            }
        }
    }
}

/// `EEMoveSightCircle`: a step of at most one tile updates only the outer rings, a longer one redoes the circle
pub fn move_circle<W: SightWorld + ?Sized>(
    w: &mut W,
    old_x: f32,
    old_y: f32,
    new_x: f32,
    new_y: f32,
    radius: f32,
    player: u64,
) {
    let dx = old_x - new_x;
    let dy = old_y - new_y;
    if dx < -1.0 || dx > 1.0 || dy < -1.0 || dy > 1.0 {
        circle(w, true, new_x, new_y, radius, player);
        circle(w, false, old_x, old_y, radius, player);
    } else {
        ring_step(w, true, old_x, old_y, new_x, new_y, radius, player);
        ring_step(w, false, old_x, old_y, new_x, new_y, radius, player);
    }
}

/// `RTSMap::FogSeen(player)` (`fog`: a seen or explored tile becomes explored) and `ExploreUnseen(player)` (an
/// unexplored tile becomes explored); x outer, as the C++ walked
pub fn fog_or_explore<W: SightWorld + ?Sized>(w: &mut W, fog: bool, player: u64) {
    let size = w.map_size();
    for x in 0..size {
        for y in 0..size {
            let tile = w.tile(x, y);
            if tile == 0 {
                continue;
            }
            let id = w.player_id(player);
            let v = visibility(w, tile, id);
            if (fog && v > 0) || (!fog && v == 0) {
                set_visibility(w, tile, 1, player);
            }
        }
    }
}

/// `EEMap::PrepareSave`'s pass: every explored but unseen tile of each player but player 0 back to unexplored (y
/// outer)
pub fn prepare_save<W: SightWorld + ?Sized>(w: &mut W) {
    let size = w.map_size();
    for y in 0..size {
        for x in 0..size {
            let tile = w.tile(x, y);
            if tile == 0 {
                continue;
            }
            let players = w.map_players();
            for i in 1..players {
                let player = w.world_player(i as usize);
                let id = w.player_id(player);
                if visibility(w, tile, id) == 1 {
                    set_visibility(w, tile, 0, player);
                }
            }
        }
    }
}

#[cfg(test)]
#[path = "tile_sight_tests.rs"]
mod tests;
