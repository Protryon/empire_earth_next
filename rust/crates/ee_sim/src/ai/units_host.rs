//! The classic_lua units host's operations whose bodies moved to Rust in the AI Thread cluster's round 7 (were
//! EECPNextUnits.cpp's cases and helpers): a unit's facts (`EE_AI_UNITS_HOST_FACTS`), ai.tuning's values, the
//! continent manager's random points (an area's, a forest's, a hill's, a choke point's), the largest sea's shore, the
//! wall lines and their blocking test, the gate side. What only C++ holds is one `ee_ai_units_host_cpp` case each
//! (EE_AI_UNITS_HOST_* from 95). The capture notes (UUNT, CNTQ) are the same Rust calls the C++ made, in its order.

#![allow(clippy::unnecessary_cast)] // c_long is i32 on Windows, i64 on LP64

use std::ffi::c_void;
use std::os::raw::c_long;

use super::more::{self, EeAiUnitsArea, EeAiUnitsAreaPick, EeAiUnitsTiles, EeAiUnitsTuning};
use super::*;
use crate::ai::personality;
use crate::astar::cm::{EeAStarCmRecord, EeAStarCmTable};
use crate::astar::continent::EeAStarContinentList;
use crate::world::{UnitView, WorldRead};

// ---- the C++ host's cases these bodies ask (EECPNextUnits.cpp ee_ai_units_host_cpp)
/// `((EEUnitGoal*)a)->GetGoalLocation(*(GE3DPoint*)out)`
pub const EE_AI_UNITS_HOST_GOAL_LOCATION: i32 = 95;
/// the complex unit `a`'s class is no building and moves (`GetMaxVelocity() > 0`)
pub const EE_AI_UNITS_HOST_MOVES: i32 = 96;
/// the complex unit `a`'s `GetTerrainFlags`: forest (bit 0) and cliff (bit 1)
pub const EE_AI_UNITS_HOST_CROSSES: i32 = 97;
/// `EEUnitBehavior::sTable.size()`, `sTable[a]`
pub const EE_AI_UNITS_HOST_BEHAVIORS: i32 = 98;
pub const EE_AI_UNITS_HOST_BEHAVIOR_AT: i32 = 99;
/// `EEDoesWallGateFitOnUnit(a, b, c, units)`
pub const EE_AI_UNITS_HOST_GATE_FITS: i32 = 100;
/// unit `a` is a living piece of the player's finished wall (`GetIsWall()`, `GetHitPoints() > 0`, its player, built)
pub const EE_AI_UNITS_HOST_GATE_WALL: i32 = 124;
/// ai.tuning's C++ values: `IsDeathmatch()`, the game options' resources, epoch start, epoch end, the space map
pub const EE_AI_UNITS_HOST_T_DEATHMATCH: i32 = 101;
pub const EE_AI_UNITS_HOST_T_RESOURCES: i32 = 102;
pub const EE_AI_UNITS_HOST_T_EPOCH_START: i32 = 103;
pub const EE_AI_UNITS_HOST_T_EPOCH_END: i32 = 104;
pub const EE_AI_UNITS_HOST_T_SPACE: i32 = 105;
/// `EEPlayer::GetPopulationLimit(0)`
pub const EE_AI_UNITS_HOST_T_POP_LIMIT: i32 = 106;
/// `gCPBehaviors`
pub const EE_AI_UNITS_HOST_T_BEHAVIORS: i32 = 123;

/// cbindgen:ignore
mod k {
    pub(super) const GOAL_ATTACK_UNIT: u8 = 2;
    pub(super) const GOAL_BUILD_REPAIR: u8 = 4;
    pub(super) const GOAL_GATHER: u8 = 6;
    pub(super) const ACTION_WAIT_FOR_SITE: u32 = 4012;
    pub(super) const CATEGORY_WATER: std::os::raw::c_long = 2;
    pub(super) const CATEGORY_GATE: u64 = 0x19;
}

fn cpp(op: i32, me: *mut c_void, a: i64, b: i64, c: i64, out: *mut c_void) -> i64 {
    // SAFETY: the C++ switch over the live game, on the simulation thread
    unsafe { ee_ai_units_host_cpp(op, me, a, b, c, out) }
}

/// The computer player's id (`me` its `EEPlayer*`; 0: none).
fn me_id(me: *mut c_void) -> u64 {
    let t = unsafe { WorldRead::begin() };
    // SAFETY: the host's live computer player
    unsafe { crate::world::PlayerView::of(&t, me as u64) }.map_or(0, |p| p.id() as u64)
}

fn record() -> Option<&'static EeAStarCmRecord> {
    crate::astar::world_view::cm()
}

fn table(r: &EeAStarCmRecord, t: EeAStarCmTable) -> &[u64] {
    &r.tables().tables[t as usize][..]
}

/// The operations whose bodies are Rust (None: the C++ host's).
///
/// # Safety
/// As `ee_ai_units_host`.
pub(super) unsafe fn op(op: i32, me: *mut c_void, a: i64, b: i64, c: i64, out: *mut c_void) -> Option<i64> {
    let _ = c;
    Some(match op {
        EE_AI_UNITS_HOST_FACTS => {
            // SAFETY: the caller's record and live unit
            unsafe { facts(me, a as u64, &mut *(out as *mut EeAiUnitsFacts)) };
            0
        }
        // SAFETY: the caller's record
        more::EE_AI_UNITS_HOST_TUNING => unsafe {
            tuning(me, &mut *(out as *mut EeAiUnitsTuning));
            0
        },
        // SAFETY: as above (a live area)
        more::EE_AI_UNITS_HOST_AREA_POINT => unsafe { area_point(a as u64, b, &mut *(out as *mut EeAiUnitsArea)) },
        more::EE_AI_UNITS_HOST_AREA_PICK => {
            // SAFETY: as above
            unsafe { area_pick(&mut *(out as *mut EeAiUnitsAreaPick)) };
            0
        }
        // SAFETY: as above
        more::EE_AI_UNITS_HOST_WATER_SHORE => unsafe { water_shore(&mut *(out as *mut EeAiUnitsTiles)) },
        more::EE_AI_UNITS_HOST_WALL_LINE => {
            // SAFETY: as above
            let t = unsafe { &mut *(out as *mut EeAiUnitsTiles) };
            let (from, to) = ([t.from[0] as i32, t.from[1] as i32], [t.to[0] as i32, t.to[1] as i32]);
            let n = crate::units::uu_note::Note::begin(37);
            n.i(from[0].into()).i(from[1].into()).i(to[0].into()).i(to[1].into());
            let tiles = crate::units::uu::wall_line(&mut crate::units::uu_host::Game, from, to);
            n.t(&tiles);
            drop(n);
            tiles_out(t, tiles.iter().map(|p| (p[0] as c_long, p[1] as c_long)).collect())
        }
        // SAFETY: as above
        more::EE_AI_UNITS_HOST_WALL_BLOCKED => i64::from(unsafe { wall_blocked(me, &*(out as *const EeAiUnitsTiles)) }),
        more::EE_AI_UNITS_HOST_TILES_AGAIN => {
            let tiles = LAST_TILES.with(|l| l.borrow_mut().take())?;
            // SAFETY: as above
            tiles_out(unsafe { &mut *(out as *mut EeAiUnitsTiles) }, tiles)
        }
        more::EE_AI_UNITS_HOST_GATE_SIDE => gate_side(me, a as c_long, b as c_long),
        _ => return None,
    })
}

thread_local! {
    /// The last list handed out past the caller's room, for its second read (`EE_AI_UNITS_HOST_TILES_AGAIN`): a list
    /// is never computed twice (the shore's GetTilesNextTo is captured).
    static LAST_TILES: std::cell::RefCell<Option<Vec<(c_long, c_long)>>> = const { std::cell::RefCell::new(None) };
}

pub(super) fn tiles_out(t: &mut EeAiUnitsTiles, tiles: Vec<(c_long, c_long)>) -> i64 {
    for (k, &(x, y)) in tiles.iter().enumerate().take(t.cap) {
        // SAFETY: the caller's room of `cap` pairs
        unsafe {
            *t.xy.add(2 * k) = x;
            *t.xy.add(2 * k + 1) = y;
        }
    }
    let n = tiles.len() as i64;
    if tiles.len() > t.cap {
        LAST_TILES.with(|l| *l.borrow_mut() = Some(tiles));
    }
    n
}

/// `EE_AI_UNITS_HOST_FACTS`: the goal's place and target, a gatherer's kind and wait, a stalled builder, the home
/// base, the terrain it crosses, the garrison, the stance.
unsafe fn facts(me: *mut c_void, unit: u64, f: &mut EeAiUnitsFacts) {
    // SAFETY: the caller's record
    unsafe { ee_ai_units_facts_reset(f) };
    let t = unsafe { WorldRead::begin() };
    // SAFETY: the caller's live complex unit
    let Some(u) = (unsafe { UnitView::of(&t, unit) }) else { return };
    let g = u.goal_handle();
    if g != 0 {
        f.has_goal = true;
        let mut place = [0.0f32; 3];
        cpp(EE_AI_UNITS_HOST_GOAL_LOCATION, me, g as i64, 0, 0, place.as_mut_ptr().cast());
        f.tx = ee_base::math::um::floor(place[0]) as c_long;
        f.ty = ee_base::math::um::floor(place[1]) as c_long;
        let call = |op: i32| {
            let mut io: crate::units::rust_goal::EeUnitsGoalIo = unsafe { std::mem::zeroed() };
            // SAFETY: the unit's live goal (the C++ inline Call)
            let r = unsafe {
                crate::units::goal_obj::ee_units_goal_call(
                    op,
                    g as usize as *const crate::units::goal_obj::EeUnitsGoalBase,
                    0,
                    0,
                    &mut io,
                )
            };
            (r, io)
        };
        let target = match u.goal().map(|k| k.0) {
            Some(k::GOAL_GATHER) => {
                let target = call(crate::units::rust_goal::EE_UNITS_GOAL_GATHER_FIELDS).0 as u64;
                let r = u.resource_type() as c_long - 2;
                if (0..5).contains(&r) {
                    f.gathers = r;
                }
                f.waiting = call(crate::units::rust_goal::EE_UNITS_GOAL_GATHER_FIELDS).1.i[2] != 0
                    && crate::world_field!(u.complex(), mCurrentPhysicsAction) != 0;
                target
            }
            Some(k::GOAL_BUILD_REPAIR) => {
                let target = call(crate::units::rust_goal::EE_UNITS_GOAL_BUILD_FIELDS).0 as u64;
                // SAFETY: the goal's live target
                let unbuilt =
                    unsafe { UnitView::of(&t, target) }.and_then(|v| v.complex_unit()).is_some_and(|c| !c.is_built());
                f.stalled = u.command_type() == k::ACTION_WAIT_FOR_SITE
                    && target != 0
                    && unbuilt
                    && call(crate::units::rust_goal::EE_UNITS_GOAL_BUILD_SITE_TAKEN).0 != 0;
                target
            }
            Some(k::GOAL_ATTACK_UNIT) => {
                let mut a: crate::combat::goal_attack::EeCombatAttackGoal = unsafe { std::mem::zeroed() };
                // SAFETY: the live attack goal
                unsafe { crate::combat::goal_attack::ee_combat_attack_get(g, &mut a) };
                a.target
            }
            _ => 0,
        };
        // SAFETY: as above
        if let Some(v) = unsafe { UnitView::of(&t, target) }.filter(|v| v.valid()) {
            f.target = v.id() as c_long;
        }
    }
    // SAFETY: the unit's home base (live or 0)
    if let Some(v) = unsafe { UnitView::of(&t, u.home_base_handle()) }.filter(|v| v.valid()) {
        f.home_base = v.id() as c_long;
    }
    if cpp(EE_AI_UNITS_HOST_MOVES, me, unit as i64, 0, 0, std::ptr::null_mut()) != 0 {
        f.moves = true;
        let crosses = cpp(EE_AI_UNITS_HOST_CROSSES, me, unit as i64, 0, 0, std::ptr::null_mut());
        f.crosses_forest = crosses & 1 != 0;
        f.crosses_cliff = crosses & 2 != 0;
    }
    f.garrisoned = crate::world_field!(u.complex(), mGarrisonCount) as c_long;
    let behavior = u.behavior_handle();
    let n = cpp(EE_AI_UNITS_HOST_BEHAVIORS, me, 0, 0, 0, std::ptr::null_mut());
    for k in 0..n {
        if cpp(EE_AI_UNITS_HOST_BEHAVIOR_AT, me, k, 0, 0, std::ptr::null_mut()) as u64 == behavior {
            f.stance = k as c_long;
        }
    }
}

/// ai.tuning's values beyond the computer player's record.
unsafe fn tuning(me: *mut c_void, t: &mut EeAiUnitsTuning) {
    let q = |op: i32, a: i64| cpp(op, me, a, 0, 0, std::ptr::null_mut());
    t.deathmatch = q(EE_AI_UNITS_HOST_T_DEATHMATCH, 0) != 0;
    t.all_managers = q(EE_AI_UNITS_HOST_T_RESOURCES, 0) == 5;
    t.epoch_start = q(EE_AI_UNITS_HOST_T_EPOCH_START, 0) as c_long;
    t.epoch_end = q(EE_AI_UNITS_HOST_T_EPOCH_END, 0) as c_long;
    t.space = q(EE_AI_UNITS_HOST_T_SPACE, 0) != 0;
    if let Some(r) = record() {
        t.water_tiles = r.num_water_tiles;
        t.land_tiles = r.num_land_tiles;
        t.space_tiles = r.num_space_tiles;
        // SAFETY: the manager's record (its CNTQ record, as the C++ inline)
        t.largest_sea = unsafe { crate::astar::cm_queries::ee_astar_cmq_largest_continent(r, k::CATEGORY_WATER) };
    }
    t.fills_garrison = q(more::EE_AI_UNITS_HOST_FILLS_GARRISON, 0) != 0;
    t.sees_all = q(EE_AI_UNITS_HOST_SEES_ALL, 0) != 0;
    t.population_limit = q(EE_AI_UNITS_HOST_T_POP_LIMIT, 0) as c_long;
    let id = me_id(me);
    for k in 0..7 {
        t.cheats[k] = personality::cheat_on(id, 1 << k);
    }
    let p = personality::rule(id);
    let n = p.id.len().min(127);
    for (slot, &c) in t.id.iter_mut().zip(&p.id[..n]) {
        *slot = c as _;
    }
    t.id[n] = 0;
    t.epoch_pressure_start = p.epoch_pressure_start;
    t.epoch_pressure_full = p.epoch_pressure_full;
    t.stone_span = personality::stone_span(id);
    t.safe_bonus = personality::safe_bonus(id);
    t.surplus = p.surplus as c_long;
    t.surplus_cut = p.surplus_cut;
    t.low_stock = p.low_stock as c_long;
    t.floor = p.floor;
    t.max_citizen_share = p.max_citizen_share;
    t.wood_slots = p.wood_slots as c_long;
    t.moves_per_cycle = p.moves_per_cycle as c_long;
    t.demand_split = personality::demand_split_on(id);
    t.coast_fishing = personality::coast_fishing_on(id);
    t.forage_patches = personality::forage_patches_on(id);
    t.claim_range = personality::claim_range_on(id);
    t.claim_needed = personality::claim_needed_on(id);
    t.early_town_centers = personality::early_town_centers(id);
    t.early_trainers = personality::early_trainers(id);
    t.patch_expand = personality::patch_expand_on(id);
    t.standing_army = p.standing_army;
    t.army_floor = p.army_floor;
    t.militia = personality::militia_on(id);
    t.militia_towers = personality::militia_towers_on(id);
    t.prehistoric_army = personality::prehistoric_army(id);
    t.prehistoric_peace = personality::prehistoric_peace_on(id);
    t.stone_peace = personality::stone_peace_on(id);
    t.naval_flee = personality::naval_flee_on(id);
    let b = q(EE_AI_UNITS_HOST_T_BEHAVIORS, 0) as usize as *const crate::cp::init::EeCpBehavior;
    // SAFETY: gCPBehaviors' 129 records (ee_sim::cp::init's layout, checked by the C++)
    let row = |i: usize| unsafe { *b.add(i) };
    t.epoch_advance_time_scale = row(94).fv;
    let (f, d) = (row(124), row(125));
    let fighters = [f.i1, f.i2, f.fv as c_long as i32, d.i1, d.i2, d.fv as c_long as i32];
    for (slot, n) in t.carrier_fighters.iter_mut().zip(fighters) {
        *slot = n as c_long;
    }
    t.population_cap_own_weight = row(93).fv;
    t.population_cap_other_weight = row(92).fv;
}

/// An area's random point (`GetTileSet().GetRandomPoint(URandom(seed))`); false for an empty area.
///
/// # Safety
/// `area` a live area.
unsafe fn random_point(area: u64, seed: i64) -> Option<(c_long, c_long)> {
    // SAFETY: the caller's contract
    let tiles = unsafe { crate::astar::cm_io::area_tiles(area) };
    // SAFETY: the area's live set
    if unsafe { ee_base::sparse::ee_sparse_size(tiles) } == 0 {
        return None;
    }
    extern "C" fn generate(r: *mut c_void) -> f32 {
        // SAFETY: the generator below
        unsafe { (*(r as *mut ee_base::random::URandom)).generate() }
    }
    let mut random = ee_base::random::URandom::new(seed as u32);
    // SAFETY: the live set; the generator does not reach it
    let p = unsafe {
        ee_base::sparse::ee_sparse_random_point(tiles, generate, (&mut random as *mut ee_base::random::URandom).cast())
    };
    Some((p.x as c_long, p.y as c_long))
}

unsafe fn area_point(area: u64, seed: i64, out: &mut EeAiUnitsArea) -> i64 {
    // SAFETY: the caller's contract
    match unsafe { random_point(area, seed) } {
        Some((x, y)) => {
            out.x = x;
            out.y = y;
            1
        }
        None => 0,
    }
}

fn hull_point(x: c_long, y: c_long) -> crate::astar::hull::EeHullPoint {
    crate::astar::hull::EeHullPoint { x, y }
}

fn distance2(a: crate::astar::hull::EeHullPoint, x: c_long, y: c_long) -> c_long {
    let (dx, dy) = (a.x.wrapping_sub(x), a.y.wrapping_sub(y));
    dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy))
}

/// `EECPUnitsPickOf`: the pick of the found areas, its random point.
unsafe fn pick_of(found: &[u64], p: &mut EeAiUnitsAreaPick) {
    p.found = found.len() as c_long;
    if found.is_empty() {
        return;
    }
    let mut pick = (p.pick % found.len() as i64) as c_long;
    if pick < 0 {
        pick += found.len() as c_long;
    }
    // SAFETY: a listed live area
    if let Some((x, y)) = unsafe { random_point(found[pick as usize], p.seed) } {
        p.ok = true;
        p.px = x;
        p.py = y;
    }
}

/// ai.forest_point, ai.hill_point, ai.choke_point (`kind` 0, 1, 2) on the land `land`.
unsafe fn area_pick(p: &mut EeAiUnitsAreaPick) {
    let Some(r) = record() else { return };
    let land = p.land;
    let (x, y, radius) = (p.x, p.y, p.r);
    if p.kind == 0 || p.kind == 1 {
        let (list, objects) = if p.kind == 0 {
            (EeAStarContinentList::Forests, table(r, EeAStarCmTable::Forests))
        } else {
            (EeAStarContinentList::Hills, table(r, EeAStarCmTable::Hills))
        };
        let mut count = 0usize;
        // SAFETY: the live continent's id list
        let ids = unsafe {
            crate::astar::continent::ee_astar_continent_ids(
                crate::astar::cm_io::continent(land) as *const _,
                list,
                &mut count,
            )
        };
        let ids: &[u16] = if count == 0 { &[] } else { unsafe { std::slice::from_raw_parts(ids, count) } };
        let mut found = Vec::new();
        for &id in ids {
            let area = objects.get(id as usize).copied().unwrap_or(0);
            if area == 0 {
                continue;
            }
            let a = area as usize as *const crate::astar::area::EeAStarAreaRecord;
            // SAFETY: the listed live area's record
            let inside = unsafe { crate::astar::area_ops::ee_astar_area_hull_contains(a, hull_point(x, y)) };
            if inside
                || distance2(
                    unsafe { crate::astar::area_ops::ee_astar_area_closest_hull_point(a, hull_point(x, y)) },
                    x,
                    y,
                ) <= radius.wrapping_mul(radius)
            {
                found.push(area);
            }
        }
        p.total = ids.len() as c_long;
        // SAFETY: the found live areas
        unsafe { pick_of(&found, p) };
        return;
    }
    // the choke points of the land; of the `r` nearest (by their closest point) when r is above 0
    extern "C" fn push(list: *mut c_void, choke: u64) {
        // SAFETY: the vector below
        unsafe { (*(list as *mut Vec<u64>)).push(choke) }
    }
    let mut chokes: Vec<u64> = Vec::new();
    let id = crate::astar::cm_io::continent(land).id as u16;
    // SAFETY: the manager's record (its CNTQ record, as the C++ inline)
    unsafe {
        crate::astar::cm_queries::ee_astar_cmq_choke_points(
            r,
            id,
            false,
            std::ptr::null(),
            0,
            (&mut chokes as *mut Vec<u64>).cast(),
            push,
        )
    };
    p.total = chokes.len() as c_long;
    if radius > 0 && !chokes.is_empty() {
        // a multimap by distance: ties keep their insertion order (from the back)
        let mut keyed: Vec<(c_long, u64)> = chokes
            .iter()
            .rev()
            .map(|&ch| {
                // SAFETY: the listed live choke point's tiles
                let c = unsafe {
                    ee_base::sparse::ee_sparse_closest_point(crate::astar::cm_io::area_tiles(ch), x as i32, y as i32)
                };
                (distance2(hull_point(c.x as c_long, c.y as c_long), x, y), ch)
            })
            .collect();
        keyed.sort_by_key(|e| e.0);
        chokes = keyed.into_iter().take(radius as usize).map(|e| e.1).collect();
    }
    // SAFETY: the found live areas
    unsafe { pick_of(&chokes, p) };
}

/// The largest water body's shore tiles next to the land mass at the tile; -1 when it does not touch it.
unsafe fn water_shore(t: &mut EeAiUnitsTiles) -> i64 {
    let Some(r) = record() else { return -1 };
    // SAFETY: the manager's record (CNTQ, as the C++ inline)
    let water_id = unsafe { crate::astar::cm_queries::ee_astar_cmq_largest_continent(r, k::CATEGORY_WATER) } as u16;
    let continents = table(r, EeAStarCmTable::Continents);
    let water = if water_id != 0 { continents[water_id as usize] } else { 0 };
    let land_id = crate::astar::cm_io::continent_id_at(r, t.from[0], t.from[1]);
    let land = if land_id != 0 { continents[land_id as usize] } else { 0 };
    if water == 0 || land == 0 {
        return -1;
    }
    let land_id = crate::astar::cm_io::continent(land).id as c_long;
    // SAFETY: the record's adjacency flags
    let adjacent = unsafe { *r.tile_flags.add((water_id as c_long * r.flags_width + land_id) as usize) };
    if !adjacent {
        return -1;
    }
    extern "C" fn push(list: *mut c_void, x: c_long, y: c_long) {
        // SAFETY: the vector below
        unsafe { (*(list as *mut Vec<(c_long, c_long)>)).push((x, y)) }
    }
    let mut shore: Vec<(c_long, c_long)> = Vec::new();
    // SAFETY: the live water body
    unsafe {
        crate::astar::area_shore::ee_astar_continent_shore_next_to(
            water,
            land_id as u16,
            (&mut shore as *mut Vec<(c_long, c_long)>).cast(),
            push,
        )
    };
    tiles_out(t, shore)
}

/// IsPathBlocked (EECPWallManager::PlanWall): a land building on the line or within 2 tiles of it, the player's own
/// wall pieces on the line excepted.
unsafe fn wall_blocked(me: *mut c_void, t: &EeAiUnitsTiles) -> bool {
    // SAFETY: the caller's pairs
    let line: Vec<[i32; 2]> =
        (0..t.len).map(|k| unsafe { [*t.xy.add(2 * k) as i32, *t.xy.add(2 * k + 1) as i32] }).collect();
    let mut all: Vec<[i32; 2]> = Vec::new();
    for p in &line {
        all.push(*p);
        for radius in 1..=2 {
            all.extend(crate::units::uu_ffi::square_ring_tiles(p[0], p[1], radius));
        }
    }
    // U2DPoint's order: lower y first, then larger x
    all.sort_by(|a, b| a[1].cmp(&b[1]).then(b[0].cmp(&a[0])));
    all.dedup();
    let mut units = Vec::new();
    crate::units::uu_ffi::units_on(&all, &mut units);
    let w = unsafe { WorldRead::begin() };
    for &u in &units {
        // SAFETY: the tiles' live units
        let Some(v) = (unsafe { UnitView::of(&w, u) }) else { continue };
        let Some(class) = v.class() else { continue };
        if !class.land_building() {
            continue;
        }
        let (x, y) = v.tile_xy();
        let own_wall = v.player().is_some_and(|p| p.handle() == me as u64)
            && class.wall()
            && class.category() != k::CATEGORY_GATE
            && line.contains(&[x as i32, y as i32]);
        if !own_wall {
            return true;
        }
    }
    false
}

/// The side (1..4) a gate fits on over the player's finished wall at the tile, as the classic wall manager picks it.
fn gate_side(me: *mut c_void, x: c_long, y: c_long) -> i64 {
    let n = crate::units::uu_note::Note::begin(41);
    n.i(x as i64).i(y as i64);
    let mut units = Vec::new();
    let count = crate::units::uu::units_on(&mut crate::units::uu_host::Game, &[[x as i32, y as i32]], &mut units);
    n.i(count.into()).us(&units);
    // the note closes here, as EEGetUnitsOnTile returned: the gate fits' own notes are outermost
    drop(n);
    // GetHitPoints() and the rest are the unit's own (a wall piece answers for its whole wall)
    let wall =
        units.iter().copied().find(|&u| cpp(EE_AI_UNITS_HOST_GATE_WALL, me, u as i64, 0, 0, std::ptr::null_mut()) != 0);
    let Some(wall) = wall else { return 0 };
    let w = unsafe { WorldRead::begin() };
    // SAFETY: the tile's live unit
    let wall_type = unsafe { UnitView::of(&w, wall) }.and_then(|v| v.class()).map_or(0, |c| c.unit_type()) as i64;
    drop(w);
    for side in 1..=4 {
        if cpp(EE_AI_UNITS_HOST_GATE_FITS, me, wall as i64, side, wall_type, std::ptr::null_mut()) != 0 {
            return side;
        }
    }
    0
}
