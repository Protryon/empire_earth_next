//! The groups' host in the game (the groups lane, round 1) and the entry points the C++ names of the groups
//! (`EEGroup`, `EESavedGroup`, `EESelectedGroup`, `EEActiveGroup`; ee/EEPlayer.h) call. The units, classes, stats,
//! players and the world are read through the world views; what is C++ (the reference counts, the formation, the path
//! and continent managers, the user interface, the map's outlines, the saved game's path requests) is one switch,
//! `ee_units_group_host` (EEGroup.cpp). Absent from a test binary.

#![cfg(not(test))]
#![allow(clippy::unnecessary_cast)] // `c_long` is 32 bits on Windows and 64 elsewhere

use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};

use crate::astar::pm::{FileIn, FileOut, Item, RequestRecord};
use crate::astar::request;
use crate::formations::units::{formation_fields, formation_place};
use crate::units::group_obj::{
    self as go, EeUnitsGroup, EeUnitsPlayerGroups, FormationState, GroupHost, GroupUnit, PathWaits, SavedGroup,
    SavedPaths, G,
};
use crate::units::player_save::{
    EeUnitsPlayerSaveFn, EeUnitsPlayerSaveIo, PlayerSaveHost, EE_UNITS_PLAYER_SAVE_READ_BOOL,
    EE_UNITS_PLAYER_SAVE_READ_LONG, EE_UNITS_PLAYER_SAVE_READ_ULONG, EE_UNITS_PLAYER_SAVE_WRITE_BOOL,
    EE_UNITS_PLAYER_SAVE_WRITE_LONG, EE_UNITS_PLAYER_SAVE_WRITE_ULONG,
};
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
        }
    }
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
// (13 was terrain_flags: Rust since the paths lane's round 2)
/// `gContinentManager.GetContinentID(a)`
pub const EE_UNITS_GROUP_OP_CONTINENT_OF: i32 = 14;
/// `gContinentManager.GetContinent((c, d), kAIPathNonSpace)`
pub const EE_UNITS_GROUP_OP_CONTINENT_AT: i32 = 15;
/// `gContinentManager.GetPathAreaIDAt(c, d)`
pub const EE_UNITS_GROUP_OP_PATH_AREA_AT: i32 = 17;
// (19 was path_request: Rust since the paths lane's round 2)
// (20 was path_cancel: Rust since the paths lane's round 2)
// (21 was path_reference: Rust since the paths lane's round 2)
// (22 was path_dereference: Rust since the paths lane's round 2)
// (23 was path_done: Rust since the paths lane's round 2)
// (24 was path_id: Rust since the paths lane's round 2)
// (25 was path_tiles: Rust since the paths lane's round 2)
/// unit `a`'s `ClearBuildTargets()`
pub const EE_UNITS_GROUP_OP_CLEAR_BUILD_TARGETS: i32 = 27;
/// `TMGetTime()`
pub const EE_UNITS_GROUP_OP_TIME: i32 = 28;
/// `gUserInterface->PostMessage424Once()`
pub const EE_UNITS_GROUP_OP_POST_SELECTION: i32 = 29;
/// player `a`'s `mGroups[c]` (0 past them)
pub const EE_UNITS_GROUP_OP_PLAYER_GROUP: i32 = 31;
/// `gWorld.mMap->ClearU1955DC()`
pub const EE_UNITS_GROUP_OP_CLEAR_AREAS: i32 = 34;
/// `gWorld.mMap->BuildAreaOutlines`: the `len` areas at `list` (each a `U2DRectangle<long>`: bottom, left, right,
/// top), the continents at `list2`
pub const EE_UNITS_GROUP_OP_AREA_OUTLINES: i32 = 35;
/// `gAEManager.mTemplates[c]->mRadius` into `io.f`
pub const EE_UNITS_GROUP_OP_AREA_EFFECT_RADIUS: i32 = 36;
// (37 was save_path: Rust since the paths lane's round 2)
// (38 was load_path: Rust since the paths lane's round 2)
// (39 was await_path: Rust since the paths lane's round 2)
/// `EEAfterLoad(...)` calling `ee_units_group_restore_saved(a)`
pub const EE_UNITS_GROUP_OP_AFTER_LOAD: i32 = 40;
/// player `a`'s `mActiveGroup` (c 0) or `mSelectedGroup` (1)
pub const EE_UNITS_GROUP_OP_PLAYER_SELECTION: i32 = 43;
/// player `a`'s `GetUnit(c)`
pub const EE_UNITS_GROUP_OP_PLAYER_UNIT: i32 = 44;
/// unit `b`'s class's `IsSelectable(b)`
pub const EE_UNITS_GROUP_OP_SELECTABLE: i32 = 45;
/// `gDoppelgangerManager.HasDoppelganger(b's tile, b, player a's id)`
pub const EE_UNITS_GROUP_OP_DOPPELGANGER: i32 = 46;
/// `gWorld.mExecutingLocalSelection`
pub const EE_UNITS_GROUP_OP_LOCAL_SELECTION: i32 = 47;
/// `EEPlayForLocalPlayer(a, b->mStats->GetSelectionSound(-1))` (c 0) or `GetAcknowledgeSound(-1)` (1)
pub const EE_UNITS_GROUP_OP_PLAY_SOUND: i32 = 48;
/// `gWorldTable.GetLongValue(kWorldAcknowledgementDelay)`
pub const EE_UNITS_GROUP_OP_ACK_DELAY: i32 = 49;
/// player `a`'s `mViewedUnit = b`
pub const EE_UNITS_GROUP_OP_SET_VIEWED_UNIT: i32 = 50;
/// `EEUnitClassCount()`
pub const EE_UNITS_GROUP_OP_CLASS_COUNT: i32 = 51;
/// player `a`'s `GetUpgrade(c)`
pub const EE_UNITS_GROUP_OP_UPGRADE: i32 = 52;
/// upgrade `a`'s `Init()`
pub const EE_UNITS_GROUP_OP_UPGRADE_INIT: i32 = 53;
/// unit `a`'s `SetHitPoints(c)`
pub const EE_UNITS_GROUP_OP_SET_HIT_POINTS: i32 = 54;
/// complex unit `a`'s `SetFlightTime(c)`
pub const EE_UNITS_GROUP_OP_SET_FLIGHT_TIME: i32 = 55;
/// complex unit `a`'s `MakeAreaEffects()`
pub const EE_UNITS_GROUP_OP_MAKE_AREA_EFFECTS: i32 = 56;
/// player `a`'s `mCivilization.AddUpgrades(mCivilization.GetPurchasedUpgrades(), GetPlayerID(), false)`
pub const EE_UNITS_GROUP_OP_CIVILIZATION_UPGRADES: i32 = 57;
// the compound statements as one-statement operations (the groups lane, round 2)
/// the path subsector at (c, d)'s location's x
pub const EE_UNITS_GROUP_OP_SUBSECTOR_X: i32 = 66;
/// its y
pub const EE_UNITS_GROUP_OP_SUBSECTOR_Y: i32 = 67;
/// `gMapWidth`
pub const EE_UNITS_GROUP_OP_MAP_WIDTH: i32 = 68;
/// `gMapHeight`
pub const EE_UNITS_GROUP_OP_MAP_HEIGHT: i32 = 69;
/// `gUserInterface != NULL`
pub const EE_UNITS_GROUP_OP_INTERFACE: i32 = 70;
/// `gUserInterface->PostRefreshSavedGroup(c)`
pub const EE_UNITS_GROUP_OP_POST_SAVED_GROUP_DO: i32 = 71;

extern "C" {
    fn ee_units_group_host(op: i32, a: u64, b: u64, c: i64, d: i64, io: *mut EeUnitsGroupIo) -> i64;
}
// the fill's USFL record (TEST ONLY)
use crate::units::pick_sweep::game::ee_units_fill_capture;

/// The game's world for the groups.
pub struct GameGroups;

impl GameGroups {
    fn io(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64, io: &mut EeUnitsGroupIo) -> i64 {
        if let Some(v) = crate::units::dm_group_view::group_plain(op, a, c) {
            return v;
        }
        // SAFETY: the C++ switch reads live objects the bodies hand it
        unsafe { ee_units_group_host(op, a, b, c, d, io) }
    }
    fn op(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64) -> i64 {
        let mut io = EeUnitsGroupIo::default();
        self.io(op, a, b, c, d, &mut io)
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
        has_tile_node: v.has_tile_node(),
        resource_amount: if v.is_unit() { v.resource_amount() } else { 0 },
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

/// A player's group lists (its `mGroupLists`, the layout's offset).
///
/// # Safety
/// `player` a live player; the borrow ends before any host call.
unsafe fn lists<'a>(player: u64) -> &'a mut EeUnitsPlayerGroups {
    // SAFETY: the caller's contract: the player's constructor made the lists
    unsafe { &mut **((player as usize + layout().player_group_lists as usize) as *const *mut EeUnitsPlayerGroups) }
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
    // the formation's members: its Rust record (formations/units.rs; units round 14)
    fn formation_kind(&mut self, formation: u64) -> i64 {
        // SAFETY: a live formation
        unsafe { formation_fields(formation) }.mType as i64
    }
    fn formation_center(&mut self, formation: u64) -> [f32; 3] {
        // SAFETY: as above
        unsafe { formation_fields(formation) }.mCenter
    }
    fn set_formation_center(&mut self, formation: u64, center: [f32; 3]) {
        // SAFETY: as above
        unsafe { formation_fields(formation) }.mCenter = center;
    }
    fn formation_place(&mut self, formation: u64, units: &[u64]) -> Vec<[f32; 3]> {
        // SAFETY: a live formation and the group's live units
        unsafe { formation_place(formation, units) }.into_iter().map(|s| s.position).collect()
    }
    fn formation_state(&mut self, formation: u64) -> FormationState {
        // SAFETY: as above
        let f = unsafe { formation_fields(formation) };
        let mut orientation = [0.0f32; 16];
        for (k, v) in f.mOrientation.iter().flatten().enumerate() {
            orientation[k] = *v;
        }
        FormationState { orientation, center: f.mCenter, heading: f.mHeading, b5c: f.mB5C }
    }
    fn set_formation_state(&mut self, formation: u64, s: &FormationState) {
        // SAFETY: as above
        let f = unsafe { formation_fields(formation) };
        for (k, v) in f.mOrientation.iter_mut().flatten().enumerate() {
            *v = s.orientation[k];
        }
        f.mCenter = s.center;
        f.mHeading = s.heading;
        f.mB5C = s.b5c;
    }
    fn terrain_flags(&mut self, unit: u64) -> [u8; 17] {
        // the unit's class's GetTerrainFlags (gather_spot.rs)
        // SAFETY: a live unit
        let class = read(|t| unsafe { UnitView::of(t, unit) }.and_then(|u| u.class()).map_or(0, |c| c.handle()));
        crate::units::gather_spot::class_terrain_flags(class, unit).map(u8::from)
    }
    fn continent_of(&mut self, unit: u64) -> u16 {
        self.op(EE_UNITS_GROUP_OP_CONTINENT_OF, unit, 0, 0, 0) as u16
    }
    fn continent_at(&mut self, x: i64, y: i64) -> u16 {
        self.op(EE_UNITS_GROUP_OP_CONTINENT_AT, 0, 0, x, y) as u16
    }
    fn subsector_at(&mut self, x: i64, y: i64) -> (i64, i64) {
        (self.op(EE_UNITS_GROUP_OP_SUBSECTOR_X, 0, 0, x, y), self.op(EE_UNITS_GROUP_OP_SUBSECTOR_Y, 0, 0, x, y))
    }
    fn path_area_at(&mut self, x: i64, y: i64) -> u16 {
        self.op(EE_UNITS_GROUP_OP_PATH_AREA_AT, 0, 0, x, y) as u16
    }
    fn slot_area_at(&mut self, x: i64, y: i64) -> Option<u16> {
        // `(unsigned long)` of the tile's `long` coordinates against the map's size
        let (ux, uy) = (x as c_long as c_ulong, y as c_long as c_ulong);
        let (w, h) =
            (self.op(EE_UNITS_GROUP_OP_MAP_WIDTH, 0, 0, 0, 0), self.op(EE_UNITS_GROUP_OP_MAP_HEIGHT, 0, 0, 0, 0));
        if (ux as u64) < (w as c_ulong as u64) && (uy as u64) < (h as c_ulong as u64) {
            Some(self.path_area_at(x, y))
        } else {
            None
        }
    }
    fn path_request(&mut self, player_id: i64, from: (i64, i64), to: (i64, i64), flags: &[u8; 17]) -> u64 {
        // `RequestPath(player, from, to, flags, 1, -1, NULL, 0)` (the flags' player 0)
        let f = request::EePathAsgFlags { player: 0, flags: flags.map(|b| b != 0) };
        request::request_path(player_id as c_long as i64, from, to, &f, 1, -1, 0, false)
    }
    fn path_cancel(&mut self, path: u64) {
        request::cancel(path);
    }
    fn path_reference(&mut self, path: u64) {
        // SAFETY: the group's live request
        unsafe { request::reference(path) };
    }
    fn path_dereference(&mut self, path: u64) {
        // SAFETY: the group's reference
        unsafe { request::dereference(path) };
    }
    fn path_done(&mut self, path: u64) -> bool {
        // SAFETY: the group's live request
        unsafe { request::get(path) }.r.done
    }
    fn path_id(&mut self, path: u64) -> i64 {
        // SAFETY: the group's live request
        i64::from(unsafe { request::get(path) }.r.id)
    }
    fn path_tiles(&mut self, path: u64) -> Vec<(i64, i64)> {
        // SAFETY: the group's live request
        unsafe { request::get(path) }.path.iter().map(|t| (i64::from(t[0]), i64::from(t[1]))).collect()
    }
    fn set_group_path(&mut self, unit: u64, points: &[[f32; 3]]) {
        // `unit->SetGroupPathGoals(points)` (waypoints.rs)
        // SAFETY: a live complex unit of the body's world
        unsafe {
            crate::units::waypoints::ee_units_waypoints_group_path_goals(unit, points.as_ptr().cast(), points.len())
        }
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
        if self.op(EE_UNITS_GROUP_OP_INTERFACE, 0, 0, 0, 0) != 0 {
            self.op(EE_UNITS_GROUP_OP_POST_SAVED_GROUP_DO, 0, 0, index as i64, 0);
        }
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
        // SAFETY: a live player's lists
        unsafe { lists(player) }.get(list).to_vec()
    }
    fn player_groups_clear(&mut self, player: u64, list: u32) {
        // SAFETY: as above
        unsafe { lists(player) }.clear(list)
    }
    fn player_groups_erase(&mut self, player: u64, list: u32, group: u64) {
        // SAFETY: as above
        unsafe { lists(player) }.erase_first(list, group)
    }
    fn player_selection(&mut self, player: u64, which: u32) -> u64 {
        self.op(EE_UNITS_GROUP_OP_PLAYER_SELECTION, player, 0, which as i64, 0) as u64
    }
    fn player_groups_push(&mut self, player: u64, list: u32, group: u64) {
        // SAFETY: as above
        unsafe { lists(player) }.push(list, group)
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
        // (left, right, top, bottom) as `U2DRectangle<long>` holds them: bottom, left, right, top
        let areas: Vec<[c_long; 4]> =
            areas.iter().map(|a| [a[3] as c_long, a[0] as c_long, a[1] as c_long, a[2] as c_long]).collect();
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

impl crate::units::selection::SelectionHost for GameGroups {
    fn player_unit(&mut self, player: u64, id: i64) -> u64 {
        self.op(EE_UNITS_GROUP_OP_PLAYER_UNIT, player, 0, id, 0) as u64
    }
    fn selectable(&mut self, unit: u64) -> bool {
        self.op(EE_UNITS_GROUP_OP_SELECTABLE, 0, unit, 0, 0) != 0
    }
    fn has_doppelganger(&mut self, player: u64, unit: u64) -> bool {
        self.op(EE_UNITS_GROUP_OP_DOPPELGANGER, player, unit, 0, 0) != 0
    }
    fn executing_local_selection(&mut self) -> bool {
        self.op(EE_UNITS_GROUP_OP_LOCAL_SELECTION, 0, 0, 0, 0) != 0
    }
    fn play_sound(&mut self, player: u64, unit: u64, which: u32) {
        self.op(EE_UNITS_GROUP_OP_PLAY_SOUND, player, unit, which as i64, 0);
    }
    fn acknowledgement_delay(&mut self) -> c_ulong {
        self.op(EE_UNITS_GROUP_OP_ACK_DELAY, 0, 0, 0, 0) as c_long as c_ulong
    }
    fn viewed_unit(&mut self, player: u64) -> u64 {
        // SAFETY: a live player
        read(|t| unsafe { crate::world::PlayerView::of(t, player) }.map_or(0, |p| p.viewed_unit_handle()))
    }
    fn set_viewed_unit(&mut self, player: u64, unit: u64) {
        self.op(EE_UNITS_GROUP_OP_SET_VIEWED_UNIT, player, unit, 0, 0);
    }
    fn sees_all(&mut self, player: u64) -> bool {
        // SAFETY: a live player
        read(|t| unsafe { crate::world::PlayerView::of(t, player) }.is_some_and(|p| p.sees_all()))
    }
    fn acknowledged(&mut self, player: u64) -> (c_long, c_long) {
        // SAFETY: a live player's record (the layout's offset)
        let f = unsafe { player_fields(player) };
        (f.mLastAcknowledgedGroup, f.mLastAcknowledgementTime)
    }
    fn set_acknowledged(&mut self, player: u64, group: c_long, time: c_long) {
        // SAFETY: as above
        let f = unsafe { player_fields(player) };
        f.mLastAcknowledgedGroup = group;
        f.mLastAcknowledgementTime = time;
    }
    fn select_pick(&mut self, player: u64, ids: &[u32]) -> Vec<u32> {
        let mut picked = vec![0u32; ids.len() + 1];
        // SAFETY: the slices' lengths
        let n =
            unsafe { crate::units::ffi::ee_units_select_pick(player, ids.as_ptr(), ids.len(), picked.as_mut_ptr()) };
        // SAFETY: as above (TEST ONLY: off by default)
        unsafe {
            crate::units::pick_sweep::game::ee_units_select_pick_capture(
                100,
                player,
                ids.as_ptr(),
                ids.len(),
                picked.as_ptr(),
                n,
            )
        };
        picked.truncate(n);
        picked
    }
}

impl crate::units::selection::UpgradesHost for GameGroups {
    fn class_count(&mut self) -> u64 {
        self.op(EE_UNITS_GROUP_OP_CLASS_COUNT, 0, 0, 0, 0) as u64
    }
    fn upgrade(&mut self, player: u64, class: u64) -> u64 {
        // GetUpgrade(class) (player_upgrades.rs: the vector read in place)
        crate::units::player_upgrades::at(player, class as i64 as c_long as usize)
    }
    fn upgrade_values(&mut self, upgrade: u64) -> (c_long, c_long, c_long, i64) {
        // SAFETY: a live upgrade
        read(|t| match unsafe { crate::world::StatsView::of(t, upgrade) } {
            Some(s) => {
                let f = s.fields();
                let unit_type = s.class().map_or(0, |c| c.unit_type() as c_ulong as c_long as i64);
                (
                    world_field!(f, mFlightTime),
                    world_field!(f, mMaxHitPoints),
                    world_field!(f, mAreaEffectTemplates)[0],
                    unit_type,
                )
            }
            None => (0, 0, 0, 0),
        })
    }
    fn init_upgrade(&mut self, upgrade: u64) {
        self.op(EE_UNITS_GROUP_OP_UPGRADE_INIT, upgrade, 0, 0, 0);
    }
    fn flight_time(&mut self, complex: u64) -> c_long {
        // SAFETY: a live complex unit
        read(|t| unsafe { UnitView::of(t, complex) }.map_or(0, |v| world_field!(v.complex(), mFlightTime)))
    }
    fn set_hit_points(&mut self, unit: u64, value: c_long) {
        self.op(EE_UNITS_GROUP_OP_SET_HIT_POINTS, unit, 0, value as i64, 0);
    }
    fn set_flight_time(&mut self, complex: u64, value: c_long) {
        self.op(EE_UNITS_GROUP_OP_SET_FLIGHT_TIME, complex, 0, value as i64, 0);
    }
    fn make_area_effects(&mut self, complex: u64) {
        self.op(EE_UNITS_GROUP_OP_MAKE_AREA_EFFECTS, complex, 0, 0, 0);
    }
    fn add_civilization_upgrades(&mut self, player: u64) {
        self.op(EE_UNITS_GROUP_OP_CIVILIZATION_UPGRADES, player, 0, 0, 0);
    }
}

/// A player's record (`EeUnitsPlayerFields` at the layout's offset), for one access.
///
/// # Safety
/// `player` is a live player; the borrow ends before any host call.
unsafe fn player_fields<'a>(player: u64) -> &'a mut crate::world::player_fields::EeUnitsPlayerFields {
    // SAFETY: the caller's contract
    unsafe {
        &mut *((player as usize + layout().player_fields as usize)
            as *mut crate::world::player_fields::EeUnitsPlayerFields)
    }
}

/// The player's selection members (`op`, ee_units selection.rs): 0 `UpdateSelection(unit, flag)`, 1
/// `TrySelectUnit(unit, flag)`, 2 `SelectUnit(id, flag, flag2)`, 3 `AddUnitToSelectedGroup(id, flag)`, 4
/// `AddUnitsToSelectedGroup(ids)`, 5 `AddAndRemoveUnitsFromSelectedGroup(ids)`, 6 `SelectUnits(ids)`, 7
/// `DeselectUnit(unit)`, 8 `RemoveUnitFromSelectedGroup(id)`, 9 `PlayAcknowledgement(unit)`, 10 `RefreshUpgrades()`;
/// TrySelectUnit's answer.
///
/// # Safety
/// `player` a live player; `unit` 0 or a live unit; `ids` `n` ids.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_units_selection(
    player: *mut c_void,
    op: i32,
    unit: *mut c_void,
    id: i64,
    ids: *const c_long,
    n: usize,
    flag: bool,
    flag2: bool,
) -> bool {
    use crate::units::selection as sel;
    let (h, p, u) = (&mut GameGroups, player as u64, unit as u64);
    // SAFETY: the caller's contract
    let list: &[c_long] = if n == 0 || ids.is_null() { &[] } else { unsafe { std::slice::from_raw_parts(ids, n) } };
    match op {
        0 => sel::update_selection(h, p, u, flag),
        1 => return sel::try_select_unit(h, p, u, flag),
        2 => sel::select_unit(h, p, id, flag, flag2),
        3 => sel::add_unit_to_selected(h, p, id, flag),
        4 | 5 => sel::add_units(h, p, list, op == 5),
        6 => sel::select_units(h, p, list),
        7 => sel::deselect_unit(h, p, u),
        8 => sel::remove_unit_from_selected(h, p, id),
        9 => sel::play_acknowledgement(h, p, u),
        10 => sel::refresh_upgrades(h, p),
        _ => {}
    }
    false
}

impl PathWaits for GameGroups {
    fn await_path(&mut self, request_id: i64, group: G, key: i64) {
        // the group referenced for the wait (group_obj.rs restore_active); the attach releases it
        let g = group as u64;
        request::await_request(
            request_id as c_long as i64,
            Box::new(move |r| {
                // SAFETY: the waiting group, referenced until now
                unsafe { go::path_resolved(&mut GameGroups, g as G, key, r) }
            }),
        );
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

impl SaveFile {
    fn read_value(&mut self, op: i32) -> Result<EeUnitsPlayerSaveIo, i32> {
        let mut io = EeUnitsPlayerSaveIo::default();
        match self.call(op, 0, &mut io) {
            0 => Ok(io),
            e => Err(e),
        }
    }
}

/// A request's record through the file's primitives (`FSFile`'s `long`, `unsigned long` and `bool`: the bytes of
/// the original's typed calls).
impl FileOut for SaveFile {
    fn put(&mut self, item: Item) {
        let mut io = EeUnitsPlayerSaveIo::default();
        let op = match item {
            Item::I32(v) => {
                io.i = i64::from(v);
                EE_UNITS_PLAYER_SAVE_WRITE_LONG
            }
            Item::U32(v) => {
                io.i = i64::from(v);
                EE_UNITS_PLAYER_SAVE_WRITE_ULONG
            }
            Item::Bool(v) => {
                io.b = u8::from(v);
                EE_UNITS_PLAYER_SAVE_WRITE_BOOL
            }
            _ => unreachable!("a request's record is longs, unsigned longs and bools"),
        };
        self.call(op, 0, &mut io);
    }
}

impl FileIn for SaveFile {
    fn u32(&mut self) -> Result<u32, i32> {
        self.read_value(EE_UNITS_PLAYER_SAVE_READ_ULONG).map(|io| io.i as u32)
    }
    fn i32(&mut self) -> Result<i32, i32> {
        self.read_value(EE_UNITS_PLAYER_SAVE_READ_LONG).map(|io| io.i as i32)
    }
    fn u16(&mut self) -> Result<u16, i32> {
        unreachable!("not in a request's record")
    }
    fn bool(&mut self) -> Result<bool, i32> {
        self.read_value(EE_UNITS_PLAYER_SAVE_READ_BOOL).map(|io| io.b != 0)
    }
    fn f32(&mut self) -> Result<f32, i32> {
        unreachable!("not in a request's record")
    }
    fn bytes(&mut self, _len: usize) -> Result<Vec<u8>, i32> {
        unreachable!("not in a request's record")
    }
}

impl SavedPaths for SaveFile {
    fn save_path(&mut self, path: u64) {
        // `EESaveAutoPathRequest`: every field (save 0.277)
        // SAFETY: the group's live request
        let rr = unsafe { request::record(path) };
        rr.write(self);
    }
    fn load_path(&mut self) -> (i32, u64) {
        // `EELoadAutoPathRequest`: a new request (one reference, the caller's), none on an error
        match RequestRecord::read(self) {
            Ok(rr) => (0, request::from_record(&rr)),
            Err(e) => (e, 0),
        }
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

/// The player's groups (`op`): 0 the running groups emptied, released and cleared (`~EEPlayer`, `Reset`, `Clear`), 1
/// `~EEPlayer`'s other groups released, 2 `Reset`'s and `Clear`'s selection and hot-key groups emptied, 3
/// `RemovalsDone`'s, 4 the tick's validation.
///
/// # Safety
/// `player` a live player whose groups are live.
#[no_mangle]
pub unsafe extern "C" fn ee_units_groups_player(player: *mut c_void, op: i32) {
    let (h, p) = (&mut GameGroups, player as u64);
    match op {
        0 => go::release_running(h, p),
        1 => go::release_others(h, p),
        2 => go::clear_selections(h, p),
        3 => go::removals_done(h, p),
        4 => go::validate_player(h, p),
        _ => {}
    }
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
