//! `EEAIPathSubSector::IsPassable(player)` whole (was EEAIPathSubSector.cpp's, the game's leaves round 2): whether
//! the gate on a path sub-sector's tiles lets a player through, over the world views. The tiles are walked from the
//! last; a tile's first gate unit (category `kUnitCategoryGate`) becomes the gate, and a tile without one keeps the
//! gate of the tiles before it. No gate (or a dead one) passes; a side part never does; an open gate does, and a
//! closed one only for its owner or a player at peace with it, while it is unlocked.

use std::ffi::c_long;

use crate::astar::ffi::EeSparseArray;
use crate::world::view::{UnitView, WorldRead, WorldView};

/// `kUnitCategoryGate`
const CATEGORY_GATE: u64 = 0x19;
/// `kActionClosed`
const ACTION_CLOSED: u32 = 15001;

/// What the test reads of a gate.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Gate {
    pub hit_points: i64,
    /// `IsSidePart`: the last part of a linked gate, or its middle part
    pub side_part: bool,
    pub closed: bool,
    /// its owner is the player, or at peace with it
    pub friendly: bool,
    pub locked: bool,
}

/// One tile's part of the walk: `None` off the map (skipped), `Some(None)` a tile without a gate unit (the gate
/// carries over), `Some(Some(g))` a gate unit, whose complex unit is `g` (None: none).
pub type TileGate = Option<Option<Option<Gate>>>;

/// The walk over the tiles (last first), as the C++ loop.
pub fn passable(tiles: impl Iterator<Item = TileGate>) -> bool {
    let mut gate: Option<Gate> = None;
    for t in tiles {
        let Some(found) = t else { continue };
        if let Some(g) = found {
            gate = g;
        }
        let Some(g) = gate else { return true };
        if g.hit_points <= 0 {
            return true;
        }
        if !g.side_part && (!g.closed || (g.friendly && !g.locked)) {
            return true;
        }
    }
    false
}

/// The gate's reads (the player's slot `player` in the world).
fn gate_of(token: &WorldRead, world: &WorldView<'_>, gate: &UnitView<'_>, player: c_long) -> Gate {
    let next = gate.next_part_handle();
    let previous = gate.previous_part_handle();
    let middle = next != 0
        // SAFETY: a live gate's next part is live
        && unsafe { UnitView::of(token, next) }.is_some_and(|n| n.next_part_handle() == 0)
        && previous != 0;
    let closed = gate.command_type() == ACTION_CLOSED;
    let mut friendly = false;
    if closed {
        let me = world.player(player as usize);
        let owner = gate.player();
        friendly = match (me, owner) {
            (Some(me), Some(owner)) => me.handle() == owner.handle() || owner.enemy(me.id() as usize) == 0,
            _ => false,
        };
    }
    Gate {
        hit_points: gate.hit_points(),
        side_part: (next == 0 && previous != 0) || middle,
        closed,
        friendly,
        locked: crate::world_field!(gate.complex(), mIsLocked),
    }
}

/// `IsPassable(player)` of the sub-sector whose tile set is `tiles`.
///
/// # Safety
/// `tiles` is a live set; the caller holds the world (the AI's path searches).
#[no_mangle]
pub unsafe extern "C" fn ee_astar_sub_sector_is_passable(tiles: *const EeSparseArray, player: c_long) -> bool {
    // SAFETY: the caller's contract
    let set = unsafe { &(*tiles).set };
    // SAFETY: as above
    let token = unsafe { WorldRead::begin() };
    let Some(world) = WorldView::global(&token) else { return false };
    let map = world.map();
    let walk = (0..set.size()).rev().map(|i| {
        let p = set.get(i);
        let tile = map.as_ref().and_then(|m| m.tile(i64::from(p.x), i64::from(p.y)))?;
        let unit = tile.units().flatten().find(|u| u.class().is_some_and(|c| c.category() == CATEGORY_GATE));
        Some(unit.map(|u| u.complex_unit().map(|g| gate_of(&token, &world, &g, player))))
    });
    passable(walk)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn g(hit_points: i64, side_part: bool, closed: bool, friendly: bool, locked: bool) -> Gate {
        Gate { hit_points, side_part, closed, friendly, locked }
    }

    #[test]
    fn the_walk_as_the_cpp_loop() {
        // no tiles, or only tiles off the map: not passable
        assert!(!passable(std::iter::empty()));
        assert!(!passable([None, None].into_iter()));
        // a tile without a gate before any gate: passable
        assert!(passable([Some(None)].into_iter()));
        // a gate unit without a complex unit, or a dead gate: passable
        assert!(passable([Some(Some(None))].into_iter()));
        assert!(passable([Some(Some(Some(g(0, false, true, false, true))))].into_iter()));
        // an open gate, a closed friendly unlocked one: passable; a side part never
        assert!(passable([Some(Some(Some(g(5, false, false, false, true))))].into_iter()));
        assert!(passable([Some(Some(Some(g(5, false, true, true, false))))].into_iter()));
        assert!(!passable([Some(Some(Some(g(5, true, false, true, false))))].into_iter()));
        // closed and locked, or closed and hostile: not; the gate carries over a tile without one
        let shut = Some(Some(Some(g(5, false, true, true, true))));
        assert!(!passable([shut, Some(None), None].into_iter()));
        assert!(passable([shut, Some(Some(Some(g(5, false, false, false, false))))].into_iter()));
    }
}
