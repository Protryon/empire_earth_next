//! TEST ONLY: the units sweep of EE-SIM --unit-sweep (the simulation capture's units profile, tools/sim_capture.py;
//! the physics models' lane, round 2), moved whole from EEPhysicsSweep.cpp (the units' lane, round 5). No game of
//! the other capture profiles steps an airplane, a submarine, a sea animal or a spaceship (and a helicopter only in
//! the calamity game), so every five game minutes this sweep makes every player such units and sends them at the
//! next player: the models' bodies (pm_air.rs, pm_ground.rs) and the carrier planes' deck walk run in a played game,
//! and PMST records them.
//!
//! Each call, for every player with a unit on the map (its anchor: its first living building there, else its first
//! living unit): first its producers, each made once and again after its death, built, at the first tile around the
//! anchor (rings out to 40) where `EECanPlaceOnTile` takes it: an airport, a dock, a navy yard, an aircraft carrier
//! (on water) and a space carrier (in space; a map without water or space has none of those). Then one unit of
//! every second class of the swept models (airplane, helicopter, submarine, sea animal, spaceship; not the
//! buildings, shots, bombs, missiles and torpedoes), the half by the call and player, produced as the game produces
//! it (`EEUnitClass::ProduceUnit`): aircraft into the producer of their air traffic type (airport, carrier, navy
//! yard, space carrier), sea units at the dock, space units at the space carrier, the rest at the anchor. Each new
//! unit then gets one goal at the next player: armed units attack (aircraft and helicopters the anchor, or the
//! carrier every second class; sea units the carrier, else the dock; space units the space carrier), the others move
//! to it. Everything is in the unit and class tables' order and draws no random number of its own.
//!
//! The world is read through the views; the C++ host (EEPhysicsSweep.cpp, `ee_units_sweep_host`) makes the units,
//! produces them, gives the goals and answers the class's virtual tests.

use std::sync::Mutex;

use crate::world::{ClassView, PlayerView, UnitView, WorldRead, WorldView};

// The sweeps' host operations (`ee_units_sweep_host(op, a, b, c, d, point)`):

/// class `a`: bit 0 `IsBuilding()`, bit 1 `IsLandBuilding()`, bit 2 `IsAircraft()`, bits 8.. the index of its
/// `mPhysicsModel` in `gPhysicsModels` (0xff: none of them, or no model)
pub const EE_UNITS_SWEEP_OP_CLASS_INFO: i32 = 1;
/// class `a`'s `CreateUnit(c, d, player b's id, false, NULL)`, the player's pending units added and the queued
/// placements flushed, the unit built (when it has a tile node) and put on the ground -> its id (0 none)
pub const EE_UNITS_SWEEP_OP_CREATE: i32 = 2;
/// producer `a`'s class `ProduceUnit(a, class b, false)`, then its player's pending units added and the queued
/// placements flushed -> 1 done
pub const EE_UNITS_SWEEP_OP_PRODUCE: i32 = 3;
/// unit `a`'s goal at `b`: `NewAttackUnit(a, b, true, false)` when `c`, else `NewLocation` at b's position;
/// `SetGoal(goal, 0, 0)`
pub const EE_UNITS_SWEEP_OP_ORDER: i32 = 4;
/// the active groups' sweep (ag_sweep.rs): a new `EEActiveGroup` of player `a`, units `b` (`c` handles) that
/// live and are valid added (`AddMember(u, true)`), formation kind `d` set -> the group
pub const EE_UNITS_SWEEP_OP_NEW_GROUP: i32 = 5;
/// group `a`'s `SetGoal(goal b, c != 0)`; `d` >= 0: then `mAction = d`, into its player's pending groups, `Plan()`
pub const EE_UNITS_SWEEP_OP_RUN: i32 = 6;
/// a goal by its kind `a` (ag_sweep.rs `EE_UNITS_SWEEP_GOAL_*`): unit `b`, target `c` (a unit or a patrol group),
/// the point in the call's floats, `d` the flag word -> the goal
pub const EE_UNITS_SWEEP_OP_GOAL: i32 = 7;
/// the AGPL record of case `a`, player `b`, group `c`, the units (`d` handles), the answers word
pub const EE_UNITS_SWEEP_OP_RECORD: i32 = 8;
/// group `a`'s `TellGroup(unit b, its position)`
pub const EE_UNITS_SWEEP_OP_TELL: i32 = 9;

/// The host (`ee_units_sweep_host`).
pub trait SweepHost {
    fn op(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64) -> i64;
}

/// The game's host.
pub struct Game;

#[cfg(not(test))]
extern "C" {
    fn ee_units_sweep_host(op: i32, a: u64, b: u64, c: i64, d: i64, f: *const f32) -> i64;
}

#[cfg(test)]
unsafe fn ee_units_sweep_host(_op: i32, _a: u64, _b: u64, _c: i64, _d: i64, _f: *const f32) -> i64 {
    0
}

/// The floats the next host call reads (a goal's point).
static POINT: Mutex<[f32; 3]> = Mutex::new([0.0; 3]);

impl SweepHost for Game {
    fn op(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64) -> i64 {
        let p = *POINT.lock().unwrap_or_else(std::sync::PoisonError::into_inner);
        // SAFETY: the host's contract: live objects of the world, the point three floats
        unsafe { ee_units_sweep_host(op, a, b, c, d, p.as_ptr()) }
    }
}

/// Sets the point the next `Goal` call reads.
pub fn set_point(p: [f32; 3]) {
    *POINT.lock().unwrap_or_else(std::sync::PoisonError::into_inner) = p;
}

// ee/EEUnitClassIDs.h
const TYPE_AIRPORT: i64 = 0x42;
const TYPE_DOCK: i64 = 0x24;
const TYPE_NAVY_YARD: i64 = 0x41;
const TYPE_CARRIER: i64 = 0x7e;
const TYPE_SPACE_CARRIER: i64 = 0x2f3;
const CATEGORY_PROJECTILE: u64 = 9;
const CATEGORY_BOMB: u64 = 0xb;
const CATEGORY_MISSILE: u64 = 0xc;
const CATEGORY_TORPEDO: u64 = 0x14;
const CATEGORY_NAVAL_BUILDING: u64 = 0x15;
const CATEGORY_SPACE_BUILDING: u64 = 0x20;
const MOVEMENT_SEA: u8 = 4;
const MOVEMENT_SPACE: u8 = 8;

/// The producers, by their index in a player's row; an aircraft's air traffic type t is produced by `TRAFFIC[t]`.
const AIRPORT: usize = 0;
const DOCK: usize = 1;
const CARRIER: usize = 3;
const SPACE_CARRIER: usize = 4;
const PRODUCER_TYPES: [i64; 5] = [TYPE_AIRPORT, TYPE_DOCK, TYPE_NAVY_YARD, TYPE_CARRIER, TYPE_SPACE_CARRIER];
const TRAFFIC: [Option<usize>; 5] = [None, Some(AIRPORT), Some(CARRIER), Some(2), Some(SPACE_CARRIER)];

/// Each player's producers' unit ids (0: none yet).
static PRODUCERS: Mutex<Vec<[u32; 5]>> = Mutex::new(Vec::new());

/// The tiles of the rings around (cx, cy) in the sweep's order: each ring's row y - r, then its sides, then row y + r.
pub fn ring_tiles(cx: i64, cy: i64, rings: i64) -> Vec<(i64, i64)> {
    let mut out = Vec::new();
    for r in 1..=rings {
        for y in cy - r..=cy + r {
            let step = if y == cy - r || y == cy + r { 1 } else { 2 * r };
            let mut x = cx - r;
            while x <= cx + r {
                out.push((x, y));
                x += step;
            }
        }
    }
    out
}

/// Whether a class (its host info, category) is one of the swept models'.
pub fn swept(info: u32, category: u64) -> bool {
    if info & 3 != 0 {
        return false;
    }
    if !matches!((info >> 8) & 0xff, 1 | 2 | 5 | 9 | 11) {
        return false;
    }
    !matches!(
        category,
        CATEGORY_PROJECTILE
            | CATEGORY_BOMB
            | CATEGORY_MISSILE
            | CATEGORY_TORPEDO
            | CATEGORY_NAVAL_BUILDING
            | CATEGORY_SPACE_BUILDING
    )
}

/// The producer row a class comes from (None: the anchor; Some(None): none).
pub fn producer_for(aircraft: bool, traffic: i64, domains: u8) -> Option<Option<usize>> {
    if aircraft {
        return Some(if (1..=4).contains(&traffic) { TRAFFIC[traffic as usize] } else { None });
    }
    if domains & MOVEMENT_SPACE != 0 {
        return Some(Some(SPACE_CARRIER));
    }
    if domains & MOVEMENT_SEA != 0 {
        return Some(Some(DOCK));
    }
    None
}

/// The target of a new unit: (space carrier, carrier, dock, town) as the next player's.
pub fn target_of(domains: u8, class_index: usize, space: u64, carrier: u64, dock: u64, town: u64) -> u64 {
    let t = if domains & MOVEMENT_SPACE != 0 {
        space
    } else if domains & MOVEMENT_SEA != 0 {
        if carrier != 0 {
            carrier
        } else {
            dock
        }
    } else if class_index & 2 != 0 && carrier != 0 {
        carrier
    } else {
        town
    };
    if t == 0 {
        town
    } else {
        t
    }
}

fn on_map(u: &UnitView<'_>) -> bool {
    u.complex_unit_handle() != 0 && u.hit_points() > 0 && u.has_tile_node() && u.tile_xy().0 != -10000
}

fn class_info(h: &mut dyn SweepHost, class: u64) -> u32 {
    h.op(EE_UNITS_SWEEP_OP_CLASS_INFO, class, 0, 0, 0) as u32
}

fn anchor(h: &mut dyn SweepHost, p: &PlayerView<'_>) -> u64 {
    let mut first = 0;
    for u in p.units() {
        if !on_map(&u) {
            continue;
        }
        let complex = u.complex_unit_handle();
        let class = u.complex_unit().and_then(|c| c.class()).map_or(0, |c| c.handle());
        if class_info(h, class) & 1 != 0 {
            return complex;
        }
        if first == 0 {
            first = complex;
        }
    }
    first
}

fn producer(t: &WorldRead, producers: &[[u32; 5]], p: usize, i: usize) -> u64 {
    let id = producers[p][i];
    if id == 0 {
        return 0;
    }
    UnitView::by_id(t, id).filter(on_map).map_or(0, |u| u.complex_unit_handle())
}

/// `EECanPlaceOnTile(player, NULL, type, x, y, false, 1, NULL)` (uu_ffi.rs; never in a unit test).
fn can_place(player: u64, ty: i64, x: i64, y: i64) -> bool {
    #[cfg(not(test))]
    // SAFETY: a live player, no flag wanted
    return unsafe {
        crate::units::uu_ffi::ee_units_uu_can_place_unit_on_tile(
            player,
            0,
            ty as i32,
            x as i32,
            y as i32,
            0,
            1,
            std::ptr::null_mut(),
        )
    } != 0;
    #[cfg(test)]
    {
        let _ = (player, ty, x, y);
        unreachable!("the game's placement test")
    }
}

fn find_spot(t: &WorldRead, player: u64, ty: i64, anchor: u64) -> Option<(i64, i64)> {
    // SAFETY: a live anchor
    let (cx, cy) = unsafe { UnitView::of(t, anchor) }?.tile_xy();
    let map = WorldView::global(t)?.map()?;
    ring_tiles(cx, cy, 40)
        .into_iter()
        .find(|&(x, y)| x >= 0 && y >= 0 && map.tile(x, y).is_some() && can_place(player, ty, x, y))
}

fn make_producers(
    h: &mut dyn SweepHost,
    t: &WorldRead,
    producers: &mut [[u32; 5]],
    p: usize,
    player: u64,
    anchor: u64,
) {
    for (i, &ty) in PRODUCER_TYPES.iter().enumerate() {
        if producer(t, producers, p, i) != 0 {
            continue;
        }
        let Some(class) = ClassView::by_index(t, ty as usize) else { continue };
        let Some((x, y)) = find_spot(t, player, ty, anchor) else { continue };
        let id = h.op(EE_UNITS_SWEEP_OP_CREATE, class.handle(), player, x, y);
        if id != 0 {
            producers[p][i] = id as u32;
        }
    }
}

/// `EEPhysicsUnitSweep(call)`: one call of the sweep -> (made, failed).
pub fn sweep(h: &mut dyn SweepHost, call: i64) -> (i64, i64) {
    // SAFETY: the physics thread's sweep reads the world it steps
    let t = unsafe { WorldRead::begin() };
    let Some(world) = WorldView::global(&t) else { return (0, 0) };
    let n = world.num_players() as usize;
    let mut producers = PRODUCERS.lock().unwrap_or_else(std::sync::PoisonError::into_inner);
    if producers.len() < n {
        producers.resize(n, [0; 5]);
    }
    let mut anchors = vec![0u64; n];
    for (p, a) in anchors.iter_mut().enumerate().skip(1) {
        let Some(player) = world.player(p) else { continue };
        *a = anchor(h, &player);
        if *a != 0 {
            make_producers(h, &t, &mut producers, p, player.handle(), *a);
        }
    }
    let (mut made, mut failed) = (0, 0);
    let classes = ClassView::count(&t);
    for p in 1..n {
        if anchors[p] == 0 {
            continue;
        }
        let Some(player) = world.player(p) else { continue };
        let mut q = p;
        loop {
            q = if q + 1 < n { q + 1 } else { 1 };
            if q == p || anchors[q] != 0 {
                break;
            }
        }
        if q == p {
            continue;
        }
        let town = anchors[q];
        let carrier = producer(&t, &producers, q, CARRIER);
        let dock = producer(&t, &producers, q, DOCK);
        let space = producer(&t, &producers, q, SPACE_CARRIER);
        for c in 0..classes {
            let Some(class) = ClassView::by_index(&t, c) else { continue };
            let f = class.fields();
            let info = class_info(h, class.handle());
            if !swept(info, class.category()) || (c + p + call as usize) & 1 != 0 {
                continue;
            }
            let traffic: std::os::raw::c_long = crate::world_field!(f, mAirTrafficType);
            let domains: u8 = crate::world_field!(f, mDomains);
            let from = match producer_for(info & 4 != 0, i64::from(traffic), domains) {
                None => anchors[p],
                Some(Some(i)) => producer(&t, &producers, p, i),
                Some(None) => 0,
            };
            if from == 0 {
                failed += 1;
                continue;
            }
            let before = player.units().map(|u| u.id()).max().unwrap_or(0);
            let done = h.op(EE_UNITS_SWEEP_OP_PRODUCE, from, class.handle(), 0, 0) != 0;
            let mut new = 0;
            for u in player.units() {
                if u.id() > before
                    && u.complex_unit_handle() != 0
                    && u.class().map(|k| k.handle()) == Some(class.handle())
                {
                    new = u.complex_unit_handle();
                }
            }
            if !done || new == 0 {
                failed += 1;
                continue;
            }
            made += 1;
            let target = target_of(domains, c, space, carrier, dock, town);
            // SAFETY: the new live unit
            let attack = unsafe { UnitView::of(&t, new) }
                .and_then(|u| u.stats())
                .is_some_and(|s| crate::world_field!(s.fields(), mAttack) > 0.0f32);
            h.op(EE_UNITS_SWEEP_OP_ORDER, new, target, i64::from(attack), 0);
        }
    }
    (made, failed)
}

/// `EEPhysicsUnitSweep(call)` (EESim.cpp's every five game minutes).
#[no_mangle]
pub extern "C" fn ee_units_unit_sweep(call: i64) {
    let (made, failed) = sweep(&mut Game, call);
    eprintln!("EE-UNIT-SWEEP {call} made {made} failed {failed}");
}

#[cfg(test)]
#[path = "unit_sweep_tests.rs"]
mod tests;
