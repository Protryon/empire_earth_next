//! The groups' host in the game (the groups lane, round 1) and the entry points the C++ names of the groups
//! (`EEGroup`, `EESavedGroup`, `EESelectedGroup`, `EEActiveGroup`; ee/EEPlayer.h) call. The units, classes, stats,
//! players and the world are read through the world views; what is C++ (the reference counts, the formation, the path
//! and continent managers, the user interface, the map's outlines, the saved game's path requests) is one switch,
//! `ee_units_group_host` (EEGroup.cpp). Absent from a test binary.

#![cfg(not(test))]
#![allow(clippy::unnecessary_cast)] // `c_long` is 32 bits on Windows and 64 elsewhere

use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};

use crate::units::group_obj::{
    self as go, EeUnitsGroup, FormationState, GroupHost, GroupUnit, PathWaits, SavedGroup, SavedPaths, G,
};
use crate::units::player_save::{EeUnitsPlayerSaveFn, EeUnitsPlayerSaveIo, PlayerSaveHost};
use crate::world::layout::layout;
use crate::world::{UnitView, WorldRead, WorldView};
use crate::world_field;

/// One host operation's values.
#[repr(C)]
pub struct EeUnitsGroupIo {
    pub a: i64,
    pub b: i64,
    pub c: i64,
    pub d: i64,
    pub f: f32,
    pub p: [f32; 3],
    pub flags: [u8; 17],
    /// the items an operation reads (`len` of them)
    pub list: *const c_void,
    pub len: usize,
    /// a second list it reads
    pub list2: *const c_void,
    /// the list it fills (`ee_units_group_push`)
    pub out: *mut c_void,
}

impl Default for EeUnitsGroupIo {
    fn default() -> Self {
        EeUnitsGroupIo {
            a: 0,
            b: 0,
            c: 0,
            d: 0,
            f: 0.0,
            p: [0.0; 3],
            flags: [0; 17],
            list: std::ptr::null(),
            len: 0,
            list2: std::ptr::null(),
            out: std::ptr::null_mut(),
        }
    }
}

/// The list an operation fills.
#[derive(Default)]
pub struct EeUnitsGroupList {
    items: Vec<(i64, i64, [f32; 3])>,
}

/// Pushes an item onto an operation's list.
///
/// # Safety
/// `list` is the `out` of a running operation; `p` is three floats.
#[no_mangle]
pub unsafe extern "C" fn ee_units_group_push(list: *mut c_void, a: i64, b: i64, p: *const [f32; 3]) {
    // SAFETY: the caller's contract
    unsafe { (*(list as *mut EeUnitsGroupList)).items.push((a, b, if p.is_null() { [0.0; 3] } else { *p })) }
}

// ---- the operations (`ee_units_group_host`'s `op`; `a`, `b` objects, `c`, `d` values)
/// unit `a`'s `Reference()`
pub const EE_UNITS_GROUP_OP_UNIT_REFERENCE: i32 = 1;
/// unit `a`'s `Dereference()`
pub const EE_UNITS_GROUP_OP_UNIT_DEREFERENCE: i32 = 2;
/// `gWorld.GetUnit(c)`
pub const EE_UNITS_GROUP_OP_WORLD_UNIT: i32 = 3;
/// `EEFormation::Create(c)`
pub const EE_UNITS_GROUP_OP_FORMATION_NEW: i32 = 4;
/// formation `a`'s `Reference()`
pub const EE_UNITS_GROUP_OP_FORMATION_REFERENCE: i32 = 5;
/// formation `a`'s `Dereference()`
pub const EE_UNITS_GROUP_OP_FORMATION_DEREFERENCE: i32 = 6;
/// formation `a`'s `mType`
pub const EE_UNITS_GROUP_OP_FORMATION_KIND: i32 = 7;
/// formation `a`'s `mCenter` into `io.p`
pub const EE_UNITS_GROUP_OP_FORMATION_CENTER: i32 = 8;
/// formation `a`'s `mCenter = io.p`
pub const EE_UNITS_GROUP_OP_FORMATION_SET_CENTER: i32 = 9;
/// formation `a`'s `Place(units, slots)`: the `len` units at `list`; the slots' positions pushed
pub const EE_UNITS_GROUP_OP_FORMATION_PLACE: i32 = 10;
/// formation `a`'s orientation (16), centre (3), heading, `mB5C` (0 or 1) into the 21 floats at `list`
pub const EE_UNITS_GROUP_OP_FORMATION_STATE: i32 = 11;
/// formation `a`'s state from the 21 floats at `list`
pub const EE_UNITS_GROUP_OP_FORMATION_SET_STATE: i32 = 12;
/// unit `a`'s class's `GetTerrainFlags(a, flags)` into `io.flags`
pub const EE_UNITS_GROUP_OP_TERRAIN_FLAGS: i32 = 13;
/// `gContinentManager.GetContinentID(a)`
pub const EE_UNITS_GROUP_OP_CONTINENT_OF: i32 = 14;
/// `gContinentManager.GetContinent((c, d), kAIPathNonSpace)`
pub const EE_UNITS_GROUP_OP_CONTINENT_AT: i32 = 15;
/// `gContinentManager.GetPathSubSectorAt(c, d)->GetLocation()` into `io.a`, `io.b`
pub const EE_UNITS_GROUP_OP_SUBSECTOR_AT: i32 = 16;
/// `gContinentManager.GetPathAreaIDAt(c, d)`
pub const EE_UNITS_GROUP_OP_PATH_AREA_AT: i32 = 17;
/// as the previous for a tile on the map, else -1
pub const EE_UNITS_GROUP_OP_SLOT_AREA_AT: i32 = 18;
/// `gPathManager.RequestPath(c, (io.a, io.b), (io.c, io.d), flags io.flags, 1, -1, NULL, 0)`
pub const EE_UNITS_GROUP_OP_PATH_REQUEST: i32 = 19;
/// `gPathManager.CancelAutoPathRequest(a)`
pub const EE_UNITS_GROUP_OP_PATH_CANCEL: i32 = 20;
/// path `a`'s `Reference()`
pub const EE_UNITS_GROUP_OP_PATH_REFERENCE: i32 = 21;
/// path `a`'s `Dereference()`
pub const EE_UNITS_GROUP_OP_PATH_DEREFERENCE: i32 = 22;
/// path `a`'s `mIsDone`
pub const EE_UNITS_GROUP_OP_PATH_DONE: i32 = 23;
/// path `a`'s `mRequestID`
pub const EE_UNITS_GROUP_OP_PATH_ID: i32 = 24;
/// path `a`'s `mPath` tiles pushed
pub const EE_UNITS_GROUP_OP_PATH_TILES: i32 = 25;
/// unit `a`'s `SetGroupPathGoals` of the `len` points at `list`
pub const EE_UNITS_GROUP_OP_SET_GROUP_PATH: i32 = 26;
/// unit `a`'s `ClearBuildTargets()`
pub const EE_UNITS_GROUP_OP_CLEAR_BUILD_TARGETS: i32 = 27;
/// `TMGetTime()`
pub const EE_UNITS_GROUP_OP_TIME: i32 = 28;
/// `gUserInterface->PostMessage424Once()`
pub const EE_UNITS_GROUP_OP_POST_SELECTION: i32 = 29;
/// `gUserInterface->PostRefreshSavedGroup(c)` (when there is one)
pub const EE_UNITS_GROUP_OP_POST_SAVED_GROUP: i32 = 30;
/// player `a`'s `mGroups[c]` (0 past them)
pub const EE_UNITS_GROUP_OP_PLAYER_GROUP: i32 = 31;
/// player `a`'s running (c 0) or pending (1) groups pushed
pub const EE_UNITS_GROUP_OP_PLAYER_GROUPS: i32 = 32;
/// group `b` pushed onto player `a`'s running (c 0), pending (1) or finished (2) groups
pub const EE_UNITS_GROUP_OP_PLAYER_GROUPS_PUSH: i32 = 33;
/// `gWorld.mMap->ClearU1955DC()`
pub const EE_UNITS_GROUP_OP_CLEAR_AREAS: i32 = 34;
/// `gWorld.mMap->BuildAreaOutlines`: the `len` areas (left, right, top, bottom) at `list`, the continents at `list2`
pub const EE_UNITS_GROUP_OP_AREA_OUTLINES: i32 = 35;
/// `gAEManager.mTemplates[c]->mRadius` into `io.f`
pub const EE_UNITS_GROUP_OP_AREA_EFFECT_RADIUS: i32 = 36;
/// `EESaveAutoPathRequest(file a, path b)`
pub const EE_UNITS_GROUP_OP_SAVE_PATH: i32 = 37;
/// `EELoadAutoPathRequest(file a, io.a)`: the error
pub const EE_UNITS_GROUP_OP_LOAD_PATH: i32 = 38;
/// `EEAwaitAutoPathRequest(c, ...)` calling `ee_units_group_path_resolved(b, d, request)`
pub const EE_UNITS_GROUP_OP_AWAIT_PATH: i32 = 39;
/// `EEAfterLoad(...)` calling `ee_units_group_restore_saved(a)`
pub const EE_UNITS_GROUP_OP_AFTER_LOAD: i32 = 40;

extern "C" {
    fn ee_units_group_host(op: i32, a: u64, b: u64, c: i64, d: i64, io: *mut EeUnitsGroupIo) -> i64;
}
// the fill's USFL record (TEST ONLY)
use crate::units::pick_sweep::game::ee_units_fill_capture;

/// The game's world for the groups.
pub struct GameGroups;

impl GameGroups {
    fn io(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64, io: &mut EeUnitsGroupIo) -> i64 {
        // SAFETY: the C++ switch reads live objects the bodies hand it
        unsafe { ee_units_group_host(op, a, b, c, d, io) }
    }
    fn op(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64) -> i64 {
        let mut io = EeUnitsGroupIo::default();
        self.io(op, a, b, c, d, &mut io)
    }
    fn fill(&mut self, op: i32, a: u64, c: i64, io: &mut EeUnitsGroupIo) -> Vec<(i64, i64, [f32; 3])> {
        let mut list = EeUnitsGroupList::default();
        io.out = std::ptr::addr_of_mut!(list).cast();
        self.io(op, a, 0, c, 0, io);
        list.items
    }
}

fn read<R>(f: impl FnOnce(&WorldRead) -> R) -> R {
    // SAFETY: the groups' bodies run inside the C++ call that holds the world
    let t = unsafe { WorldRead::begin() };
    f(&t)
}

/// A unit's fields the bodies read, over the views (zeros for 0).
fn unit_of(v: &UnitView) -> GroupUnit {
    let (x, y) = v.tile_xy();
    let mut u = GroupUnit {
        id: v.id(),
        hit_points: v.hit_points(),
        valid: v.valid(),
        has_tile: v.tile().is_some(),
        tile_x: x,
        tile_y: y,
        player: v.player().map_or(0, |p| p.handle()),
        complex: if v.is_unit() { v.complex_unit_handle() } else { 0 },
        ..GroupUnit::default()
    };
    if v.is_complex() {
        let c = v.complex();
        u.location = v.location();
        u.disabled = world_field!(c, mIsDisabled);
        u.built = v.is_built();
        u.container = v.container_handle();
        u.active_group = v.active_group_handle();
    }
    if let Some(c) = v.class() {
        let f = c.fields();
        u.class = c.handle();
        u.unit_type = c.unit_type() as c_ulong as c_long as i64;
        u.kind_flags = world_field!(f, mKindFlags) as i64;
        u.category = c.category() as c_ulong as c_long as i64;
        u.land_building = c.land_building();
        u.domains = world_field!(f, mDomains);
        u.melee = world_field!(f, mIsMelee);
    }
    if let Some(s) = v.stats() {
        let f = s.fields();
        u.stats = s.handle();
        u.max_hit_points = s.max_hit_points();
        u.range = s.range();
        u.scale = s.scale();
        u.attack = world_field!(f, mAttack);
        u.max_velocity = world_field!(f, mMaxVelocity);
        let t = world_field!(f, mAreaEffectTemplates);
        u.ae_templates = [t[0] as i64, t[1] as i64, t[2] as i64, t[3] as i64];
        u.ae_scale = world_field!(f, mAreaEffectScale);
    }
    u
}

impl GroupHost for GameGroups {
    fn unit(&mut self, unit: u64) -> GroupUnit {
        // SAFETY: a live unit the bodies hold
        read(|t| unsafe { UnitView::of(t, unit) }.map_or_else(GroupUnit::default, |v| unit_of(&v)))
    }
    fn reference_unit(&mut self, unit: u64) {
        self.op(EE_UNITS_GROUP_OP_UNIT_REFERENCE, unit, 0, 0, 0);
    }
    fn dereference_unit(&mut self, unit: u64) {
        self.op(EE_UNITS_GROUP_OP_UNIT_DEREFERENCE, unit, 0, 0, 0);
    }
    fn set_active_group_member(&mut self, unit: u64, group: u64) {
        // SAFETY: a live complex unit's `mActiveGroup` pointer (the layout's offset)
        unsafe { *((unit as usize + layout().complex_active_group as usize) as *mut usize) = group as usize };
    }
    fn world_unit(&mut self, id: i64) -> u64 {
        self.op(EE_UNITS_GROUP_OP_WORLD_UNIT, 0, 0, id, 0) as u64
    }
    fn is_complex(&mut self, unit: u64) -> bool {
        // SAFETY: a live unit
        read(|t| unsafe { UnitView::of(t, unit) }.is_some_and(|v| v.is_complex()))
    }
    fn formation_new(&mut self, kind: i64) -> u64 {
        self.op(EE_UNITS_GROUP_OP_FORMATION_NEW, 0, 0, kind, 0) as u64
    }
    fn formation_reference(&mut self, formation: u64) {
        self.op(EE_UNITS_GROUP_OP_FORMATION_REFERENCE, formation, 0, 0, 0);
    }
    fn formation_dereference(&mut self, formation: u64) {
        self.op(EE_UNITS_GROUP_OP_FORMATION_DEREFERENCE, formation, 0, 0, 0);
    }
    fn formation_kind(&mut self, formation: u64) -> i64 {
        self.op(EE_UNITS_GROUP_OP_FORMATION_KIND, formation, 0, 0, 0)
    }
    fn formation_center(&mut self, formation: u64) -> [f32; 3] {
        let mut io = EeUnitsGroupIo::default();
        self.io(EE_UNITS_GROUP_OP_FORMATION_CENTER, formation, 0, 0, 0, &mut io);
        io.p
    }
    fn set_formation_center(&mut self, formation: u64, center: [f32; 3]) {
        let mut io = EeUnitsGroupIo { p: center, ..Default::default() };
        self.io(EE_UNITS_GROUP_OP_FORMATION_SET_CENTER, formation, 0, 0, 0, &mut io);
    }
    fn formation_place(&mut self, formation: u64, units: &[u64]) -> Vec<[f32; 3]> {
        let mut io = EeUnitsGroupIo { list: units.as_ptr().cast(), len: units.len(), ..Default::default() };
        self.fill(EE_UNITS_GROUP_OP_FORMATION_PLACE, formation, 0, &mut io).into_iter().map(|(_, _, p)| p).collect()
    }
    fn formation_state(&mut self, formation: u64) -> FormationState {
        let mut v = [0.0f32; 21];
        let mut io = EeUnitsGroupIo { list: v.as_mut_ptr().cast(), len: 21, ..Default::default() };
        self.io(EE_UNITS_GROUP_OP_FORMATION_STATE, formation, 0, 0, 0, &mut io);
        let mut s = FormationState::default();
        s.orientation.copy_from_slice(&v[..16]);
        s.center = [v[16], v[17], v[18]];
        s.heading = v[19];
        s.b5c = v[20] != 0.0;
        s
    }
    fn set_formation_state(&mut self, formation: u64, s: &FormationState) {
        let mut v = [0.0f32; 21];
        v[..16].copy_from_slice(&s.orientation);
        v[16..19].copy_from_slice(&s.center);
        v[19] = s.heading;
        v[20] = if s.b5c { 1.0 } else { 0.0 };
        let mut io = EeUnitsGroupIo { list: v.as_ptr().cast(), len: 21, ..Default::default() };
        self.io(EE_UNITS_GROUP_OP_FORMATION_SET_STATE, formation, 0, 0, 0, &mut io);
    }
    fn terrain_flags(&mut self, unit: u64) -> [u8; 17] {
        let mut io = EeUnitsGroupIo::default();
        self.io(EE_UNITS_GROUP_OP_TERRAIN_FLAGS, unit, 0, 0, 0, &mut io);
        io.flags
    }
    fn continent_of(&mut self, unit: u64) -> u16 {
        self.op(EE_UNITS_GROUP_OP_CONTINENT_OF, unit, 0, 0, 0) as u16
    }
    fn continent_at(&mut self, x: i64, y: i64) -> u16 {
        self.op(EE_UNITS_GROUP_OP_CONTINENT_AT, 0, 0, x, y) as u16
    }
    fn subsector_at(&mut self, x: i64, y: i64) -> (i64, i64) {
        let mut io = EeUnitsGroupIo::default();
        self.io(EE_UNITS_GROUP_OP_SUBSECTOR_AT, 0, 0, x, y, &mut io);
        (io.a, io.b)
    }
    fn path_area_at(&mut self, x: i64, y: i64) -> u16 {
        self.op(EE_UNITS_GROUP_OP_PATH_AREA_AT, 0, 0, x, y) as u16
    }
    fn slot_area_at(&mut self, x: i64, y: i64) -> Option<u16> {
        let r = self.op(EE_UNITS_GROUP_OP_SLOT_AREA_AT, 0, 0, x, y);
        (r >= 0).then_some(r as u16)
    }
    fn path_request(&mut self, player_id: i64, from: (i64, i64), to: (i64, i64), flags: &[u8; 17]) -> u64 {
        let mut io = EeUnitsGroupIo { a: from.0, b: from.1, c: to.0, d: to.1, flags: *flags, ..Default::default() };
        self.io(EE_UNITS_GROUP_OP_PATH_REQUEST, 0, 0, player_id, 0, &mut io) as u64
    }
    fn path_cancel(&mut self, path: u64) {
        self.op(EE_UNITS_GROUP_OP_PATH_CANCEL, path, 0, 0, 0);
    }
    fn path_reference(&mut self, path: u64) {
        self.op(EE_UNITS_GROUP_OP_PATH_REFERENCE, path, 0, 0, 0);
    }
    fn path_dereference(&mut self, path: u64) {
        self.op(EE_UNITS_GROUP_OP_PATH_DEREFERENCE, path, 0, 0, 0);
    }
    fn path_done(&mut self, path: u64) -> bool {
        self.op(EE_UNITS_GROUP_OP_PATH_DONE, path, 0, 0, 0) != 0
    }
    fn path_id(&mut self, path: u64) -> i64 {
        self.op(EE_UNITS_GROUP_OP_PATH_ID, path, 0, 0, 0)
    }
    fn path_tiles(&mut self, path: u64) -> Vec<(i64, i64)> {
        let mut io = EeUnitsGroupIo::default();
        self.fill(EE_UNITS_GROUP_OP_PATH_TILES, path, 0, &mut io).into_iter().map(|(x, y, _)| (x, y)).collect()
    }
    fn set_group_path(&mut self, unit: u64, points: &[[f32; 3]]) {
        let mut io = EeUnitsGroupIo { list: points.as_ptr().cast(), len: points.len(), ..Default::default() };
        self.io(EE_UNITS_GROUP_OP_SET_GROUP_PATH, unit, 0, 0, 0, &mut io);
    }
    fn clear_build_targets(&mut self, unit: u64) {
        self.op(EE_UNITS_GROUP_OP_CLEAR_BUILD_TARGETS, unit, 0, 0, 0);
    }
    fn time(&mut self) -> c_long {
        self.op(EE_UNITS_GROUP_OP_TIME, 0, 0, 0, 0) as c_long
    }
    fn post_selection(&mut self) {
        self.op(EE_UNITS_GROUP_OP_POST_SELECTION, 0, 0, 0, 0);
    }
    fn post_saved_group(&mut self, index: c_long) {
        self.op(EE_UNITS_GROUP_OP_POST_SAVED_GROUP, 0, 0, index as i64, 0);
    }
    fn current_player(&mut self) -> u64 {
        read(|t| WorldView::global(t).and_then(|w| w.current_player()).map_or(0, |p| p.handle()))
    }
    fn selected_group_of(&mut self, player: u64) -> u64 {
        // SAFETY: a live player
        read(|t| unsafe { crate::world::PlayerView::of(t, player) }.map_or(0, |p| p.selected_group_handle()))
    }
    fn player_group(&mut self, player: u64, index: i64) -> u64 {
        self.op(EE_UNITS_GROUP_OP_PLAYER_GROUP, player, 0, index, 0) as u64
    }
    fn player_id(&mut self, player: u64) -> i64 {
        // SAFETY: a live player
        read(|t| unsafe { crate::world::PlayerView::of(t, player) }.map_or(0, |p| p.id() as i64))
    }
    fn player_groups(&mut self, player: u64, list: u32) -> Vec<u64> {
        let mut io = EeUnitsGroupIo::default();
        self.fill(EE_UNITS_GROUP_OP_PLAYER_GROUPS, player, list as i64, &mut io)
            .into_iter()
            .map(|(g, _, _)| g as u64)
            .collect()
    }
    fn player_groups_push(&mut self, player: u64, list: u32, group: u64) {
        self.op(EE_UNITS_GROUP_OP_PLAYER_GROUPS_PUSH, player, group, list as i64, 0);
    }
    fn compute_values(&mut self, group: u64) {
        crate::units::ag::compute_values(&mut crate::units::ag_host::AgGame, group)
    }
    fn goal_location(&mut self, goal: u64) -> [f32; 3] {
        crate::units::ag::AgHost::goal_location(&mut crate::units::ag_host::AgGame, goal)
    }
    fn delete_goal(&mut self, goal: u64) {
        // SAFETY: the group's own goal, used by no one after
        unsafe { crate::units::goal_obj::goal_delete(goal) }
    }
    fn goal_moves_together(&mut self, goal: u64) -> bool {
        // SAFETY: a live goal's kind byte
        let kind = unsafe { *((goal as usize + layout().goal_kind as usize) as *const u8) };
        kind == GOAL_LOCATION && crate::units::location::get(goal).group_movement
    }
    fn keeps(&mut self, test: u32, id: u32) -> bool {
        match test {
            0 => crate::units::ffi::ee_units_keeps_non_citizen(id),
            1 => crate::units::ffi::ee_units_keeps_ground_attacker(id),
            _ => crate::units::ffi::ee_units_keeps_citizen(id),
        }
    }
    fn fill_members(&mut self, player: u64, ids: &[u32], slot: i64) -> Vec<u32> {
        let mut taken = Vec::with_capacity(ids.len());
        crate::units::store::with_store(|s| crate::units::group::fill_members(s, ids, slot, &mut taken));
        // SAFETY: the slices' lengths (TEST ONLY: off by default)
        unsafe { ee_units_fill_capture(100, player, ids.as_ptr(), ids.len(), slot, taken.as_ptr(), taken.len()) };
        taken
    }
    fn clear_area_outlines(&mut self) {
        self.op(EE_UNITS_GROUP_OP_CLEAR_AREAS, 0, 0, 0, 0);
    }
    fn area_outlines(&mut self, areas: &[[i64; 4]], continents: &[u16]) {
        let mut io = EeUnitsGroupIo {
            list: areas.as_ptr().cast(),
            len: areas.len(),
            list2: continents.as_ptr().cast(),
            ..Default::default()
        };
        self.io(EE_UNITS_GROUP_OP_AREA_OUTLINES, 0, 0, 0, 0, &mut io);
    }
    fn area_effect_radius(&mut self, template: i64) -> f32 {
        let mut io = EeUnitsGroupIo::default();
        self.io(EE_UNITS_GROUP_OP_AREA_EFFECT_RADIUS, 0, 0, template, 0, &mut io);
        io.f
    }
    fn player_units(&mut self, player: u64) -> Vec<u64> {
        // SAFETY: a live player
        read(|t| {
            unsafe { crate::world::PlayerView::of(t, player) }
                .map_or_else(Vec::new, |p| p.units().map(|u| u.handle()).collect())
        })
    }
    fn world_players(&mut self) -> Vec<u64> {
        read(|t| {
            WorldView::global(t).map_or_else(Vec::new, |w| {
                (0..w.num_players() as usize).map(|i| w.player(i).map_or(0, |p| p.handle())).collect()
            })
        })
    }
}

impl PathWaits for GameGroups {
    fn await_path(&mut self, request_id: i64, group: G, key: i64) {
        self.op(EE_UNITS_GROUP_OP_AWAIT_PATH, 0, group as u64, request_id, key);
    }
}

/// `kGoalLocation`
const GOAL_LOCATION: u8 = 1;

/// The saved game's file (the player save's primitives, `EESaveFilePrimitive` over the `FSFile`) and its path
/// requests.
struct SaveFile {
    file: *mut c_void,
    f: EeUnitsPlayerSaveFn,
}

impl PlayerSaveHost for SaveFile {
    fn call(&mut self, op: i32, index: u32, io: &mut EeUnitsPlayerSaveIo) -> i32 {
        // SAFETY: the C++ primitive over its file
        unsafe { (self.f)(self.file, op, index, io) }
    }
}

impl SavedPaths for SaveFile {
    fn save_path(&mut self, path: u64) {
        GameGroups.op(EE_UNITS_GROUP_OP_SAVE_PATH, self.file as u64, path, 0, 0);
    }
    fn load_path(&mut self) -> (i32, u64) {
        let mut io = EeUnitsGroupIo::default();
        let e = GameGroups.io(EE_UNITS_GROUP_OP_LOAD_PATH, self.file as u64, 0, 0, 0, &mut io);
        (e as i32, io.a as u64)
    }
}

// ---- the entry points (the C++ names' members; `g` the group)

/// `new EESavedGroup(player, index)` (kind 0), `new EESelectedGroup(player)` (1), `new EEActiveGroup(player)` (2).
#[no_mangle]
pub extern "C" fn ee_units_group_new(kind: u8, player: *mut c_void, index: c_long) -> *mut EeUnitsGroup {
    go::new(&mut GameGroups, kind, player as u64, index)
}

/// `Reference()`.
///
/// # Safety
/// `g` is a live group (as for every entry point below).
#[no_mangle]
pub unsafe extern "C" fn ee_units_group_reference(g: *mut EeUnitsGroup) {
    // SAFETY: the caller's contract
    unsafe { go::reference(g) }
}

/// `Dereference()`.
///
/// # Safety
/// As [`ee_units_group_reference`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_group_dereference(g: *mut EeUnitsGroup) {
    // SAFETY: the caller's contract
    unsafe { go::dereference(&mut GameGroups, g) }
}

/// `GetNumUnits()`.
///
/// # Safety
/// As [`ee_units_group_reference`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_group_len(g: *mut EeUnitsGroup) -> c_ulong {
    // SAFETY: the caller's contract
    unsafe { go::len(g) as c_ulong }
}

/// `GetUnit(i)`.
///
/// # Safety
/// As [`ee_units_group_reference`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_group_unit(g: *mut EeUnitsGroup, i: c_long) -> *mut c_void {
    // SAFETY: the caller's contract
    unsafe { go::unit_at(g, i as usize) as usize as *mut c_void }
}

/// `Contains(unit)`.
///
/// # Safety
/// As [`ee_units_group_reference`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_group_contains(g: *mut EeUnitsGroup, unit: *mut c_void) -> bool {
    // SAFETY: the caller's contract
    unsafe { go::contains(g, unit as u64) }
}

/// The group members whose arguments are a unit or nothing (`op`): 0 `Add`, 1 `RemoveUnit`, 2 `Clear`, 3
/// `Validate`, 4 `UpdateKindFlags`, 5 `MoveAlongFoundPaths`, 6 `SortUnits`, 7 `ShowSelectionAreas`, 8
/// `RefreshSelectionAreas`, 9 `AddIfAbsent(unit, true)`, 10 `AddIfAbsent(unit, false)`, 11 `AddUnit(unit, true)`, 12
/// `AddUnit(unit, false)`, 13 `AddMember(unit, true)`, 14 `AddMember(unit, false)`, 15 `Finish`, 16 `ClearGoal`, 17
/// `KeepNonCitizens`, 18 `KeepGroundAttackers`, 19 `KeepCitizens`, 20 `ResetUnitsPaths`, 21 `RequestAllPaths`, 22
/// `EEComplexUnit::SetActiveGroup` (the unit's group this one).
///
/// # Safety
/// As [`ee_units_group_reference`]; `unit` a live complex unit where one is named.
#[no_mangle]
pub unsafe extern "C" fn ee_units_group_do(g: *mut EeUnitsGroup, op: i32, unit: *mut c_void) {
    let h = &mut GameGroups;
    let u = unit as u64;
    // SAFETY: the caller's contract
    unsafe {
        match op {
            0 => go::add(h, g, u),
            1 => go::remove_unit(h, g, u),
            2 => go::clear(h, g),
            3 => go::validate(h, g),
            4 => go::update_kind_flags(h, g),
            5 => go::move_along_found_paths(h, g),
            6 => go::sort_units(h, g),
            7 => go::show_selection_areas(h, g),
            8 => go::refresh_selection_areas(h, g, u),
            9 | 10 => go::add_if_absent(h, g, u, op == 9),
            11 | 12 => go::add_unit(h, g, u, op == 11),
            13 | 14 => go::add_member(h, g, u, op == 13),
            15 => go::finish(h, g),
            16 => go::clear_goal(h, g),
            17..=19 => go::keep(h, g, (op - 17) as u32),
            20 => go::reset_units_paths(h, g),
            21 => go::request_all_paths(h, g),
            22 => go::unit_set_active_group(h, u, g),
            _ => {}
        }
    }
}

/// The members between two groups (`op`): 0 `CopyUnits(other)`, 1
/// `CopyGroup(other)`, 2 `CopySelection(other)`, 3 `EESavedGroup::Assign(selection other)`, 4
/// `SelectSavedGroup(other, true)`, 5 `SelectSavedGroup(other, false)`.
///
/// # Safety
/// `g` and `other` are live groups.
#[no_mangle]
pub unsafe extern "C" fn ee_units_group_with(g: *mut EeUnitsGroup, op: i32, other: *mut EeUnitsGroup) {
    let h = &mut GameGroups;
    // SAFETY: the caller's contract
    unsafe {
        match op {
            0 => go::copy_units(h, g, other),
            1 => go::copy_group(h, g, other),
            2 => go::copy_selection(h, g, other),
            3 => go::assign(h, g, other),
            4 | 5 => go::select_saved_group(h, g, other, op == 4),
            _ => {}
        }
    }
}

/// The members that answer a value (`op`, argument `a`): 0 `CountUnitsOfKind(a)`, 1 `HasUnitType(a)`, 2
/// `GetUnitTypeCount(a)`, 3 `FindUnitOfCategory(a)`, 4 `FindUnitWithFlags(a)`, 5 `GetContinent()`, 6 the path of the
/// request carrying unit id `a` (`FindPathRequest(unit)->mPath`), 7 `RepairPointers()`, 8 the kind.
///
/// # Safety
/// As [`ee_units_group_reference`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_group_get(g: *mut EeUnitsGroup, op: i32, a: i64) -> i64 {
    let h = &mut GameGroups;
    // SAFETY: the caller's contract
    unsafe {
        match op {
            0 => go::count_units_of_kind(h, g, a) as i64,
            1 => go::has_unit_type(g, a) as i64,
            2 => go::unit_type_count(g, a),
            3 => go::find_unit_of_category(h, g, a) as i64,
            4 => go::find_unit_with_flags(h, g, a as c_ulong) as i64,
            5 => go::continent(h, g) as i64,
            6 => go::path_of_unit(g, a as u32) as i64,
            7 => go::repair_pointers(h, g) as i64,
            8 => (*g).mGroupKind as i64,
            _ => 0,
        }
    }
}

/// `SetFormation(formation)`.
///
/// # Safety
/// As [`ee_units_group_reference`]; `formation` 0 or a live formation.
#[no_mangle]
pub unsafe extern "C" fn ee_units_group_set_formation(g: *mut EeUnitsGroup, formation: *mut c_void) {
    // SAFETY: the caller's contract
    unsafe { go::set_formation(&mut GameGroups, g, formation as u64) }
}

/// `Remove(id)` and `CancelPathRequest(id)`.
///
/// # Safety
/// As [`ee_units_group_reference`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_group_cancel_path(g: *mut EeUnitsGroup, id: c_long) {
    // SAFETY: the caller's contract
    unsafe { go::cancel_path_request(&mut GameGroups, g, id as i64) }
}

/// `GetBounds(out)`: `out` the rectangle's bottom, left, right, top.
///
/// # Safety
/// As [`ee_units_group_reference`]; `out` four floats.
#[no_mangle]
pub unsafe extern "C" fn ee_units_group_bounds(g: *mut EeUnitsGroup, out: *mut [f32; 4]) {
    // SAFETY: the caller's contract
    unsafe { go::bounds(&mut GameGroups, g, &mut *out) }
}

/// `GetFlags()`'s 17 flags into `out`.
///
/// # Safety
/// As [`ee_units_group_reference`]; `out` 17 bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_units_group_flags(g: *mut EeUnitsGroup, out: *mut [u8; 17]) {
    // SAFETY: the caller's contract
    unsafe { *out = go::flags(&mut GameGroups, g) }
}

/// `Load(major, minor, file)` / `Save(major, minor, file)` (`save`) over the player save's file primitive `f`.
///
/// # Safety
/// As [`ee_units_group_reference`]; `file` the primitive's open file.
#[no_mangle]
pub unsafe extern "C" fn ee_units_group_file(
    g: *mut EeUnitsGroup,
    save: bool,
    file: *mut c_void,
    f: EeUnitsPlayerSaveFn,
) -> c_long {
    let mut s = SaveFile { file, f };
    // SAFETY: the caller's contract
    unsafe {
        if save {
            go::save(&mut GameGroups, g, &mut s) as c_long
        } else {
            go::load(&mut GameGroups, g, &mut s) as c_long
        }
    }
}

/// `Fill(selection, sort, slot)`.
///
/// # Safety
/// `g` a live active group, `selection` a live selection.
#[no_mangle]
pub unsafe extern "C" fn ee_units_group_fill(
    g: *mut EeUnitsGroup,
    selection: *mut EeUnitsGroup,
    sort: c_long,
    slot: c_long,
) {
    // SAFETY: the caller's contract
    unsafe { go::fill(&mut GameGroups, g, selection, sort as i64, slot as i64) }
}

/// `SetGoal(goal, queued)`.
///
/// # Safety
/// `g` a live active group, `goal` a live goal.
#[no_mangle]
pub unsafe extern "C" fn ee_units_group_set_goal(g: *mut EeUnitsGroup, goal: *mut c_void, queued: bool) {
    // SAFETY: the caller's contract
    unsafe { go::set_goal(&mut GameGroups, g, goal as u64, queued) }
}

/// `RequestPaths(units)`: the `n` units at `units`.
///
/// # Safety
/// `g` a live active group with a goal; `units` `n` live complex units.
#[no_mangle]
pub unsafe extern "C" fn ee_units_group_request_paths(
    g: *mut EeUnitsGroup,
    units: *const *mut c_void,
    n: usize,
) -> bool {
    // SAFETY: the caller's contract
    let list: Vec<u64> = if n == 0 {
        Vec::new()
    } else {
        unsafe { std::slice::from_raw_parts(units, n) }.iter().map(|&u| u as u64).collect()
    };
    // SAFETY: as above
    unsafe { go::request_paths(&mut GameGroups, g, &list) }
}

/// `TellGroup(unit, location)`.
///
/// # Safety
/// `g` a live active group; `location` three floats.
#[no_mangle]
pub unsafe extern "C" fn ee_units_group_tell(g: *mut EeUnitsGroup, unit: *mut c_void, location: *const [f32; 3]) {
    // SAFETY: the caller's contract
    unsafe { go::tell_group(&mut GameGroups, g, unit as u64, *location) }
}

/// `EEPlayer::GetActiveGroupOf(unit)`.
///
/// # Safety
/// `player` a live player.
#[no_mangle]
pub unsafe extern "C" fn ee_units_group_active_of(player: *mut c_void, unit: *mut c_void) -> *mut EeUnitsGroup {
    go::active_group_of(&mut GameGroups, player as u64, unit as u64) as usize as *mut EeUnitsGroup
}

/// A loaded path request's resolution (`EEAwaitAutoPathRequest`'s attach of [`EE_UNITS_GROUP_OP_AWAIT_PATH`]).
///
/// # Safety
/// `g` the waiting group (referenced); `request` 0 or a request with the caller's reference.
#[no_mangle]
pub unsafe extern "C" fn ee_units_group_path_resolved(g: *mut EeUnitsGroup, key: i64, request: *mut c_void) {
    // SAFETY: the caller's contract
    unsafe { go::path_resolved(&mut GameGroups, g, key, request as u64) }
}

/// `EESaveActiveGroups(file)` over the player save's primitive `f`.
///
/// # Safety
/// `file` the primitive's open file.
#[no_mangle]
pub unsafe extern "C" fn ee_units_groups_save_active(file: *mut c_void, f: EeUnitsPlayerSaveFn) {
    go::save_all(&mut GameGroups, &mut SaveFile { file, f })
}

/// `EELoadActiveGroups(file)`: the first read error; the groups come back at the loaded game's first tick.
///
/// # Safety
/// As [`ee_units_groups_save_active`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_groups_load_active(file: *mut c_void, f: EeUnitsPlayerSaveFn) -> c_long {
    match go::load_all(&mut GameGroups, &mut SaveFile { file, f }) {
        Err(e) => e as c_long,
        Ok(lists) => {
            let saved = Box::into_raw(Box::new(lists));
            GameGroups.op(EE_UNITS_GROUP_OP_AFTER_LOAD, saved as u64, 0, 0, 0);
            0
        }
    }
}

/// The loaded groups back (`EELoadActiveGroups`' `EEAfterLoad` of [`EE_UNITS_GROUP_OP_AFTER_LOAD`]).
///
/// # Safety
/// `saved` the operation's value, used once.
#[no_mangle]
pub unsafe extern "C" fn ee_units_group_restore_saved(saved: *mut c_void) {
    // SAFETY: the caller's contract
    let mut lists = unsafe { Box::from_raw(saved as *mut Vec<Vec<SavedGroup>>) };
    go::restore_all(&mut GameGroups, &mut lists);
}
