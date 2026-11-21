//! The classic_lua units host's operations whose bodies moved to Rust in the AI Thread cluster's round 8 (were
//! EECPNextUnits.cpp's): the wall path (the path manager's FindPath with the wall pather), the wall routes (the
//! path manager's requests, held by handle in the player's state until read), the enemies near a tile through the
//! fog (the influence table's region sums and the regions' units), the citizen upgrade test, the tribute. What
//! only C++ holds is one `ee_ai_units_host_cpp` case each (EE_AI_UNITS_HOST_* from 125).

#![allow(clippy::unnecessary_cast)] // c_long is i32 on Windows, i64 on LP64

use std::ffi::c_void;
use std::os::raw::c_long;

use super::more::{self, EeAiUnitsTiles};
use super::*;
use crate::astar::modifier_obj::{
    ee_astar_mod_host_influence, ee_astar_modifier_new_wall_pather, ee_astar_modifier_new_wall_planner,
    ee_astar_modifier_release,
};
use crate::world::{PlayerView, UnitView, WorldRead, WorldView};

// ---- the C++ host's cases these bodies ask (EECPNextUnits.cpp ee_ai_units_host_cpp)
/// `gPathManager.RequestPath(a, from, to, (EECPASGFlags*)b, 2, -1, (EeAStarModifier*)c, 1)` (`out` the
/// `EeAiUnitsTiles` with the ends) -> the request (0: none)
pub const EE_AI_UNITS_HOST_ROUTE_NEW: i32 = 125;
/// `gUnitClassTable[kUnitTypeCitizen]->GetTerrainFlags(NULL, *(EECPASGFlags*)out)`
pub const EE_AI_UNITS_HOST_CITIZEN_FLAGS: i32 = 126;
/// the request `a`'s `mIsDone`, `mPath.size()`, `mPath[b]` (x low, y high), `Dereference()`
pub const EE_AI_UNITS_HOST_ROUTE_DONE: i32 = 127;
pub const EE_AI_UNITS_HOST_ROUTE_LEN: i32 = 128;
pub const EE_AI_UNITS_HOST_ROUTE_TILE: i32 = 129;
pub const EE_AI_UNITS_HOST_ROUTE_DEREF: i32 = 130;
/// `gPathManager.FindPath(a, gWorld.mMap, from, to, *(EEAIAStarBounds*)b, path, 1, 0x32, (EeAStarModifier*)c,
/// NULL)` into the C++'s wall path (`out` the `EeAiUnitsTiles` with the ends); its size, its tile `a`
pub const EE_AI_UNITS_HOST_WALL_FIND: i32 = 131;
pub const EE_AI_UNITS_HOST_WALL_LEN: i32 = 132;
pub const EE_AI_UNITS_HOST_WALL_TILE: i32 = 133;
/// the tech node `a`'s researched event; the event `a`'s effect count (-1: no event), its effect `b`; the effect
/// `a`'s kind (bits 48..63), unit class (16..47) and attribute (0..15) (-1: none)
pub const EE_AI_UNITS_HOST_NODE_EVENT: i32 = 134;
pub const EE_AI_UNITS_HOST_EVENT_EFFECTS: i32 = 135;
pub const EE_AI_UNITS_HOST_EVENT_EFFECT: i32 = 136;
pub const EE_AI_UNITS_HOST_EFFECT_FIELDS: i32 = 137;
/// the player `a`'s `TakeResource(b, c)`, `AddResource(b, c)`, `AddTribute(b >> 32, (int32_t)b, c)`
pub const EE_AI_UNITS_HOST_TAKE_RESOURCE: i32 = 138;
pub const EE_AI_UNITS_HOST_ADD_RESOURCE: i32 = 139;
pub const EE_AI_UNITS_HOST_ADD_TRIBUTE: i32 = 140;

/// `EECPASGFlags` (ee/EECPASGFlags.h: the player id, the 17 terrain flags), zeroed as its constructor.
#[repr(C)]
struct AsgFlags {
    player_id: c_long,
    flags: [bool; 17],
}

fn cpp(op: i32, me: *mut c_void, a: i64, b: i64, c: i64, out: *mut c_void) -> i64 {
    // SAFETY: the C++ switch over the live game, on the simulation thread
    unsafe { ee_ai_units_host_cpp(op, me, a, b, c, out) }
}

fn unpack(v: i64) -> (c_long, c_long) {
    (v as i32 as c_long, (v >> 32) as i32 as c_long)
}

/// The operations whose bodies are Rust (None: the others').
///
/// # Safety
/// As `ee_ai_units_host`.
pub(super) unsafe fn op(op: i32, me: *mut c_void, a: i64, b: i64, c: i64, out: *mut c_void) -> Option<i64> {
    let _ = c;
    Some(match op {
        // SAFETY: the caller's record
        more::EE_AI_UNITS_HOST_WALL_PATH => wall_path(me, a, unsafe { &mut *(out as *mut EeAiUnitsTiles) }),
        // SAFETY: as above
        more::EE_AI_UNITS_HOST_ROUTE_REQUEST => route_request(me, a, unsafe { &mut *(out as *mut EeAiUnitsTiles) }),
        more::EE_AI_UNITS_HOST_ROUTE_STATE => {
            let Some(r) = route(me, a as c_long) else { return Some(-2) };
            if cpp(EE_AI_UNITS_HOST_ROUTE_DONE, me, r as i64, 0, 0, std::ptr::null_mut()) != 0 {
                cpp(EE_AI_UNITS_HOST_ROUTE_LEN, me, r as i64, 0, 0, std::ptr::null_mut())
            } else {
                -1
            }
        }
        // SAFETY: as above
        more::EE_AI_UNITS_HOST_ROUTE_TAKE => route_take(me, a as c_long, unsafe { &mut *(out as *mut EeAiUnitsTiles) }),
        more::EE_AI_UNITS_HOST_ENEMY_NEAR_ALL => enemy_near_all(me, a as c_long, b as c_long) as i64,
        more::EE_AI_UNITS_HOST_CITIZEN_UPGRADE => i64::from(citizen_upgrade(me, a)),
        more::EE_AI_UNITS_HOST_TRIBUTE => {
            let to = {
                let t = unsafe { WorldRead::begin() };
                WorldView::global(&t).and_then(|w| w.player(a as usize)).map_or(0, |p| p.handle())
            } as i64;
            let me64 = me as usize as i64;
            cpp(EE_AI_UNITS_HOST_TAKE_RESOURCE, me, me64, b, c, std::ptr::null_mut());
            cpp(EE_AI_UNITS_HOST_ADD_TRIBUTE, me, me64, b & 0xffff_ffff, c, std::ptr::null_mut());
            cpp(EE_AI_UNITS_HOST_ADD_RESOURCE, me, to, b, c, std::ptr::null_mut());
            cpp(EE_AI_UNITS_HOST_ADD_TRIBUTE, me, to, (1 << 32) | (b & 0xffff_ffff), c, std::ptr::null_mut());
            0
        }
        EE_AI_UNITS_HOST_WALL_CLASS => epoch_class(me, PLCE_WALL_CLASS, crate::cp::place_game::ee_cp_wall_class),
        more::EE_AI_UNITS_HOST_GATE_CLASS => epoch_class(me, PLCE_GATE_CLASS, crate::cp::place_game::ee_cp_gate_class),
        // SAFETY: the caller's record
        more::EE_AI_UNITS_HOST_PLACE => i64::from(unsafe { place(me, &mut *(out as *mut more::EeAiUnitsPlace)) }),
        _ => return None,
    })
}

/// PLCE's methods (ee/EECPPlaceCapture.h kPlce*)
const PLCE_GATE_CLASS: u32 = 43;
const PLCE_WALL_CLASS: u32 = 45;
const PLCE_NEXT_PLACE_SEARCH: u32 = 47;

/// A PLCE scope (TEST ONLY), closed when dropped.
struct Plce(*mut crate::cp::place_capture::EeCpPlaceScope);

impl Plce {
    fn open(me: *mut c_void, method: u32) -> Plce {
        // SAFETY: the live computer player
        Plce(unsafe { crate::cp::place_capture::ee_cp_scope_begin(me as u64, method, false) })
    }
    fn i(&self, w: i64) -> &Self {
        // SAFETY: this open scope
        unsafe { crate::cp::place_capture::ee_cp_scope_word(self.0, w as u32) };
        self
    }
}

impl Drop for Plce {
    fn drop(&mut self) {
        // SAFETY: the scope this opened
        unsafe { crate::cp::place_capture::ee_cp_scope_end(self.0, false) }
    }
}

/// `EECPWallClass`, `EECPGateClass`: the class of the player's epoch.
fn epoch_class(me: *mut c_void, method: u32, class: extern "C" fn(i32) -> i32) -> i64 {
    let scope = Plce::open(me, method);
    let epoch = {
        let t = unsafe { WorldRead::begin() };
        // SAFETY: the live computer player
        unsafe { PlayerView::of(&t, me as u64) }.and_then(|p| p.tech_tree()).map_or(0, |tree| tree.current_epoch())
    };
    let r = class(epoch as i32) as i64;
    scope.i(r);
    r
}

/// `EECPNextPlaceSearch`: the Lua verbs' placement search (ee_sim::cp::placement next_place_search).
///
/// # Safety
/// `me` the live computer player.
unsafe fn place(me: *mut c_void, p: &mut more::EeAiUnitsPlace) -> bool {
    let scope = Plce::open(me, PLCE_NEXT_PLACE_SEARCH);
    let units: Vec<i64> = if p.units_len == 0 {
        Vec::new()
    } else {
        // SAFETY: the caller's `units_len` ids
        unsafe { std::slice::from_raw_parts(p.units, p.units_len) }.iter().map(|&u| u as i64).collect()
    };
    scope.i(p.unit_type as i64).i(p.mode as i64).i(p.x as i64).i(p.y as i64).i(p.first as i64).i(p.last as i64);
    scope.i(units.len() as i64);
    for &u in &units {
        scope.i(u);
    }
    scope.i(p.site as i64);
    // SAFETY: the live computer player
    let ctx = unsafe { crate::cp::player_game::ctx_of(me as u64) };
    let units_at = if units.is_empty() { std::ptr::null() } else { units.as_ptr() };
    // SAFETY: the context and the ids above
    let found = unsafe {
        crate::cp::place_game::ee_cp_next_place_search(
            &ctx,
            p.unit_type as i32,
            p.mode as i32,
            p.x as i32,
            p.y as i32,
            p.first as i32,
            p.last as i32,
            units_at,
            units.len(),
            p.site as i32,
        )
    };
    p.out_x = found.x as c_long;
    p.out_y = found.y as c_long;
    p.out_gap = found.gap as c_long;
    let result = found.found != 0;
    scope.i(i64::from(result)).i(p.out_x as i64).i(p.out_y as i64).i(p.out_gap as i64);
    // SAFETY: the open scope; the live computer player
    unsafe { crate::cp::place_capture::ee_cp_scope_centers(scope.0, me as u64) };
    result
}

/// The player's state (`EECPUnitsGet`: made when missing).
fn state(me: *mut c_void) -> &'static mut crate::ai::units_state::UnitsState {
    let s = cpp(EE_AI_UNITS_HOST_STATE, me, 0, 0, 0, std::ptr::null_mut()) as usize;
    // SAFETY: the player's live state; no other reference to it lives across these bodies
    unsafe { &mut *(s as *mut crate::ai::units_state::UnitsState) }
}

fn route(me: *mut c_void, handle: c_long) -> Option<u64> {
    state(me).routes.get(&handle).copied().filter(|&r| r != 0)
}

/// Drops every route the state holds (a new request, the state's end).
pub(super) fn release_routes(s: &mut crate::ai::units_state::UnitsState) {
    for (_, r) in std::mem::take(&mut s.routes) {
        if r != 0 {
            cpp(EE_AI_UNITS_HOST_ROUTE_DEREF, std::ptr::null_mut(), r as i64, 0, 0, std::ptr::null_mut());
        }
    }
}

/// The path's tiles (`n` of them, through `tile(k)`) into the caller's room.
fn write_tiles(t: &mut EeAiUnitsTiles, n: i64, tile: impl Fn(i64) -> i64) {
    for k in 0..(n.max(0) as usize).min(t.cap) {
        let (x, y) = unpack(tile(k as i64));
        // SAFETY: the caller's room of `cap` pairs
        unsafe {
            *t.xy.add(2 * k) = x;
            *t.xy.add(2 * k + 1) = y;
        }
    }
}

/// `ai.wall_route`'s request: one a player at a time (a new one drops the old); the citizens' terrain flags and the
/// wall planner for the enemy `enemy`.
fn route_request(me: *mut c_void, enemy: i64, t: &mut EeAiUnitsTiles) -> i64 {
    release_routes(state(me));
    let mut flags = AsgFlags { player_id: 0, flags: [false; 17] };
    cpp(EE_AI_UNITS_HOST_CITIZEN_FLAGS, me, 0, 0, 0, (&mut flags as *mut AsgFlags).cast());
    let planner = ee_astar_modifier_new_wall_planner(enemy as u32, 0);
    let flags_at = &mut flags as *mut AsgFlags as usize as i64;
    let request = cpp(
        EE_AI_UNITS_HOST_ROUTE_NEW,
        me,
        enemy,
        flags_at,
        planner as usize as i64,
        (t as *mut EeAiUnitsTiles).cast(),
    );
    // SAFETY: the planner made above (the request holds its own reference)
    unsafe { ee_astar_modifier_release(planner) };
    if request == 0 {
        return 0;
    }
    let s = state(me);
    let handle = s.next_route;
    s.next_route += 1;
    s.routes.insert(handle, request as u64);
    handle as i64
}

/// `ai.wall_route_result`'s path: its tiles into the caller's room, then the route dropped -> the path's length.
fn route_take(me: *mut c_void, handle: c_long, t: &mut EeAiUnitsTiles) -> i64 {
    let Some(r) = route(me, handle) else { return 0 };
    let n = cpp(EE_AI_UNITS_HOST_ROUTE_LEN, me, r as i64, 0, 0, std::ptr::null_mut());
    write_tiles(t, n, |k| cpp(EE_AI_UNITS_HOST_ROUTE_TILE, me, r as i64, k, 0, std::ptr::null_mut()));
    cpp(EE_AI_UNITS_HOST_ROUTE_DEREF, me, r as i64, 0, 0, std::ptr::null_mut());
    state(me).routes.remove(&handle);
    n
}

/// `ai.wall_path`: the unit's path inside the box of the two ends (U2DRectangle's Set then Extend) with the wall
/// pather -> its length (a list past the caller's room is kept for TILES_AGAIN).
fn wall_path(me: *mut c_void, unit: i64, t: &mut EeAiUnitsTiles) -> i64 {
    let (from, to) = (t.from, t.to);
    // EEAIAStarBounds: mBottom, mLeft, mRight, mTop
    let bounds: [c_long; 4] = [from[1].max(to[1]), from[0].min(to[0]), from[0].max(to[0]), from[1].min(to[1])];
    let pather = ee_astar_modifier_new_wall_pather();
    let out = (t as *mut EeAiUnitsTiles).cast();
    cpp(EE_AI_UNITS_HOST_WALL_FIND, me, unit, bounds.as_ptr() as usize as i64, pather as usize as i64, out);
    // SAFETY: the pather made above
    unsafe { ee_astar_modifier_release(pather) };
    let n = cpp(EE_AI_UNITS_HOST_WALL_LEN, me, 0, 0, 0, std::ptr::null_mut());
    let tiles: Vec<(c_long, c_long)> =
        (0..n).map(|k| unpack(cpp(EE_AI_UNITS_HOST_WALL_TILE, me, k, 0, 0, std::ptr::null_mut()))).collect();
    super::host_ops::tiles_out(t, tiles)
}

/// The classic build manager's HasEnemyNear through the fog (the manifest's "sees_all"): every armed unit not the
/// player's in the influence regions round the tile (one tile on small island and planet maps), idle searchers left
/// out -> their count (the enemy influence when it is 0 or less).
fn enemy_near_all(me: *mut c_void, x: c_long, y: c_long) -> c_long {
    let mut size = 0i32;
    // SAFETY: the world's influence table
    let table = unsafe { ee_astar_mod_host_influence(&mut size) } as *const crate::cp::influence::EeCpInfluence;
    if table.is_null() {
        return 0;
    }
    let size = size as c_long;
    let t = unsafe { WorldRead::begin() };
    let Some(w) = WorldView::global(&t) else { return 0 };
    let Some(map) = w.map() else { return 0 };
    let flags: std::os::raw::c_ulong = crate::world_field!(map.fields(), mMapCharacteristics);
    let height: c_long = crate::world_field!(map.rts(), mL195610);
    let small = (flags & 1 == 1 || flags & 4 == 4) && height <= 0x2a;
    let r = if small { 1 } else { size };
    let (left, right, top, bottom) = (x - r, x + r, y - r, y + r);
    let regions = crate::cp::influence::EeCpRect {
        left: (left / size) as i32,
        top: (top / size) as i32,
        right: (right / size) as i32,
        bottom: (bottom / size) as i32,
    };
    // SAFETY: the player's live record
    let Some(p) = (unsafe { PlayerView::of(&t, me as u64) }) else { return 0 };
    // SAFETY: the live table and world
    let count = unsafe { crate::cp::ffi::ee_cp_influence_enemies(table, w.handle(), p.id(), regions, true, true) };
    if count <= 0 {
        return count;
    }
    let units_table = map.unit_regions();
    if units_table.is_null() {
        return 0;
    }
    // SAFETY: the map's live regions
    let units = unsafe { &*units_table }.collect(left as i32, top as i32, right as i32, bottom as i32);
    let mut n: c_long = 0;
    for &u in &units {
        // SAFETY: the regions' live complex units
        let Some(v) = (unsafe { UnitView::of(&t, u as u64) }) else { continue };
        let theirs = v.player().is_none_or(|q| q.handle() != me as u64);
        let attack: f32 = v.stats().map_or(0.0, |s| crate::world_field!(s.fields(), mAttack));
        let timer: c_long = crate::world_field!(v.complex(), mTargetSearchTimer);
        if theirs && attack != 0.0 && timer <= 60000 {
            n += 1;
        }
    }
    n
}

/// The classic research manager's citizen test (EECPResearchManager::ChooseResearch): a tech whose researched event
/// improves a citizen's gathering (effect kind 2 on the citizen classes 0x20, 0x1e0, attributes 0x1a-0x1d, 0x1f-0x21).
fn citizen_upgrade(me: *mut c_void, node: i64) -> bool {
    let event = cpp(EE_AI_UNITS_HOST_NODE_EVENT, me, node, 0, 0, std::ptr::null_mut());
    let n = cpp(EE_AI_UNITS_HOST_EVENT_EFFECTS, me, event, 0, 0, std::ptr::null_mut());
    for e in 0..n.max(0) {
        let effect = cpp(EE_AI_UNITS_HOST_EVENT_EFFECT, me, event, e, 0, std::ptr::null_mut());
        let f = cpp(EE_AI_UNITS_HOST_EFFECT_FIELDS, me, effect, 0, 0, std::ptr::null_mut());
        if f != -1 && effect_improves_citizen(f) {
            return true;
        }
    }
    false
}

/// [`EE_AI_UNITS_HOST_EFFECT_FIELDS`]'s packed fields: a citizen's gathering improved.
fn effect_improves_citizen(f: i64) -> bool {
    let (kind, class, attribute) = ((f >> 48) & 0xffff, (f >> 16) as u32, f & 0xffff);
    kind == 2 && (class == 0x20 || class == 0x1e0) && matches!(attribute, 0x1a..=0x1d | 0x1f..=0x21)
}
