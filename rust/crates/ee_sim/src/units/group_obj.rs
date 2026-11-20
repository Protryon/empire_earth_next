//! The unit groups as Rust objects (the groups lane, round 1): `EEGroup` and its three kinds, `EESavedGroup` (a
//! hot-key group), `EESelectedGroup` (a player's selection) and `EEActiveGroup` (the group a command acts on), with
//! their path requests (`EEGroupPathRequest`) and clusters (`EEGroupCluster`). Was EEGroup.cpp, EESavedGroup.cpp,
//! EESelectedGroup.cpp and EEActiveGroup.cpp (0x1ebc47..0x20a3c0).
//!
//! A group is one Rust allocation headed by [`EeUnitsGroup`], the members the C++ reads in place (the player, the
//! formation, the kind flags, the selection's saved group and change time, the active group's goal and plain
//! record); `EEGroup` and its kinds are C++ names for it (ee/EEPlayer.h: no data of their own, every member one call
//! here). The rest (the units, the type counts, the path requests, the destinations) is Rust's only. The original's
//! virtuals dispatch on [`EeUnitsGroup::mGroupKind`]; a virtual call inside a body is the kind's, as it was.
//!
//! The bodies run over [`GroupHost`]: the units' reads, their reference counts and the world's C++ (the formation,
//! the path manager, the continent manager, the user interface, the map's outlines); the game's host is
//! group_host.rs. Units, formations, paths and players are handles (their addresses).

#![allow(non_snake_case)]

use crate::units::ag_fields::EeUnitsActiveGroupFields;
use crate::units::msvc_sort;
use crate::units::player_save::PlayerSaveHost;
use std::collections::BTreeMap;
use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};

/// A group's kind (`mGroupKind`): the class the original made.
pub const EE_UNITS_GROUP_KIND_SAVED: u8 = 0;
pub const EE_UNITS_GROUP_KIND_SELECTED: u8 = 1;
pub const EE_UNITS_GROUP_KIND_ACTIVE: u8 = 2;

/// The members of a group the C++ reads in place (the head of the Rust allocation; `EEGroup` derives from it).
#[repr(C)]
pub struct EeUnitsGroup {
    /// the player (`EEPlayer*`, the constructor's)
    pub mPlayer: *mut c_void,
    /// the formation (`EEFormation*`, referenced; `EEFormation::Create(0)` when made)
    pub mFormation: *mut c_void,
    /// the units' classes' kind flags or'ed
    pub mKindFlags: c_long,
    /// a selection's saved group (`EESelectedGroup::mSavedGroup`; its index while loading)
    pub mSavedGroup: *mut EeUnitsGroup,
    /// a selection's last change (`TMGetTime()`)
    pub mChangeTime: c_long,
    /// a selection asks to be sorted (`EESelectedGroup::mNeedsSort`)
    pub mNeedsSort: bool,
    /// the kind (`EE_UNITS_GROUP_KIND_*`)
    pub mGroupKind: u8,
    /// a hot-key group's index (`EESavedGroup::mIndex`: 1..20, posted to the user interface)
    pub mIndex: c_long,
    /// an active group's goal (`EEUnitGoal*`, owned)
    pub mGoal: *mut c_void,
    /// an active group's plain record (`EEActiveGroup`'s action, speeds and flags)
    pub mFields: EeUnitsActiveGroupFields,
}

/// A group path request (`EEGroupPathRequest`): the path manager's request (referenced), the ids of the units that
/// follow it, the units' flags (zero but for a loaded request) and a word (0 but for a loaded request).
#[derive(Clone, Debug, Default, PartialEq)]
pub struct PathRequest {
    pub path: u64,
    pub unit_ids: Vec<c_long>,
    pub flags_player: c_long,
    pub flags: [u8; 17],
    pub l30: c_long,
}

/// A cluster `MakeClusters` splits units into (`EEGroupCluster`).
#[derive(Clone, Debug, Default, PartialEq)]
pub struct Cluster {
    pub units: Vec<u64>,
    pub flags: [u8; 17],
    /// `mBounds` (`U2DRectangle<long>`): bottom, left, right, top
    pub bottom: i64,
    pub left: i64,
    pub right: i64,
    pub top: i64,
    pub continent: u16,
    /// 1: units near each other; 2: flyers by continent; 3: land buildings by continent
    pub kind: i32,
}

/// The rest of a group: Rust's only.
#[derive(Default)]
struct GroupData {
    references: u32,
    units: Vec<u64>,
    type_counts: BTreeMap<i64, i64>,
    requests: BTreeMap<i64, PathRequest>,
    destinations: BTreeMap<u32, [f32; 3]>,
    u48: Vec<[f32; 3]>,
    u58: Vec<[f32; 3]>,
}

#[repr(C)]
struct Alloc {
    head: EeUnitsGroup,
    data: GroupData,
}

/// A group (its head's address).
pub type G = *mut EeUnitsGroup;

/// The group's head.
///
/// # Safety
/// `g` is a live group; the borrow ends before any host call.
unsafe fn head<'a>(g: G) -> &'a mut EeUnitsGroup {
    // SAFETY: the caller's contract
    unsafe { &mut *g }
}

/// The group's Rust part.
///
/// # Safety
/// As [`head`].
unsafe fn data<'a>(g: G) -> &'a mut GroupData {
    // SAFETY: the caller's contract: the head is the first field of its allocation
    unsafe { &mut (*(g as *mut Alloc)).data }
}

/// What the bodies read of a unit (an `EEComplexUnit`, its class and its stats; zeros for 0).
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct GroupUnit {
    pub id: u32,
    pub hit_points: i64,
    pub valid: bool,
    pub has_tile: bool,
    /// `mTileNode != NULL` (on the map's tile lists)
    pub has_tile_node: bool,
    pub resource_amount: i64,
    pub tile_x: i64,
    pub tile_y: i64,
    pub location: [f32; 3],
    pub disabled: bool,
    pub built: bool,
    pub container: u64,
    pub player: u64,
    pub complex: u64,
    pub active_group: u64,
    pub class: u64,
    pub unit_type: i64,
    pub kind_flags: i64,
    pub category: i64,
    pub land_building: bool,
    pub domains: u8,
    pub melee: bool,
    pub stats: u64,
    pub max_hit_points: i64,
    pub range: f32,
    pub scale: f32,
    pub attack: f32,
    pub max_velocity: f32,
    pub ae_templates: [i64; 4],
    pub ae_scale: f32,
}

/// A formation's saved state (its orientation, centre, heading and `mB5C`).
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct FormationState {
    pub orientation: [f32; 16],
    pub center: [f32; 3],
    pub heading: f32,
    pub b5c: bool,
}

/// The C++ the bodies call.
pub trait GroupHost {
    fn unit(&mut self, unit: u64) -> GroupUnit;
    fn reference_unit(&mut self, unit: u64);
    fn dereference_unit(&mut self, unit: u64);
    /// the unit's `mActiveGroup` pointer, stored (no reference counted)
    fn set_active_group_member(&mut self, unit: u64, group: u64);
    /// `gWorld.GetUnit(id)`
    fn world_unit(&mut self, id: i64) -> u64;
    /// whether `unit` is an `EEComplexUnit` (`EEUnitCast`, `dynamic_cast`)
    fn is_complex(&mut self, unit: u64) -> bool;
    fn formation_new(&mut self, kind: i64) -> u64;
    fn formation_reference(&mut self, formation: u64);
    fn formation_dereference(&mut self, formation: u64);
    fn formation_kind(&mut self, formation: u64) -> i64;
    fn formation_center(&mut self, formation: u64) -> [f32; 3];
    fn set_formation_center(&mut self, formation: u64, center: [f32; 3]);
    /// `Place(units, slots)`: the slots' positions in order
    fn formation_place(&mut self, formation: u64, units: &[u64]) -> Vec<[f32; 3]>;
    fn formation_state(&mut self, formation: u64) -> FormationState;
    fn set_formation_state(&mut self, formation: u64, state: &FormationState);
    /// `mUnitClass->GetTerrainFlags(unit, flags)`
    fn terrain_flags(&mut self, unit: u64) -> [u8; 17];
    /// `gContinentManager.GetContinentID(unit)`
    fn continent_of(&mut self, unit: u64) -> u16;
    /// `gContinentManager.GetContinent(tile, kAIPathNonSpace)`
    fn continent_at(&mut self, x: i64, y: i64) -> u16;
    /// `gContinentManager.GetPathSubSectorAt(x, y)->GetLocation()`
    fn subsector_at(&mut self, x: i64, y: i64) -> (i64, i64);
    /// `gContinentManager.GetPathAreaIDAt(x, y)`
    fn path_area_at(&mut self, x: i64, y: i64) -> u16;
    /// as `path_area_at` when the tile is on the map (`< gMapWidth`, `< gMapHeight` unsigned), else None
    fn slot_area_at(&mut self, x: i64, y: i64) -> Option<u16>;
    /// `gPathManager.RequestPath(player, from, to, flags, 1, -1, NULL, 0)`
    fn path_request(&mut self, player_id: i64, from: (i64, i64), to: (i64, i64), flags: &[u8; 17]) -> u64;
    fn path_cancel(&mut self, path: u64);
    fn path_reference(&mut self, path: u64);
    fn path_dereference(&mut self, path: u64);
    fn path_done(&mut self, path: u64) -> bool;
    fn path_id(&mut self, path: u64) -> i64;
    fn path_tiles(&mut self, path: u64) -> Vec<(i64, i64)>;
    /// the unit's `SetGroupPathGoals(points)`
    fn set_group_path(&mut self, unit: u64, points: &[[f32; 3]]);
    fn clear_build_targets(&mut self, unit: u64);
    /// `TMGetTime()`
    fn time(&mut self) -> c_long;
    /// `gUserInterface->PostMessage424Once()`
    fn post_selection(&mut self);
    /// `if (gUserInterface != NULL) gUserInterface->PostRefreshSavedGroup(index)`
    fn post_saved_group(&mut self, index: c_long);
    fn current_player(&mut self) -> u64;
    /// the player's `mSelectedGroup`
    fn selected_group_of(&mut self, player: u64) -> u64;
    /// the player's `mGroups[index]` (0 past them)
    fn player_group(&mut self, player: u64, index: i64) -> u64;
    fn player_id(&mut self, player: u64) -> i64;
    /// the player's `mRunningGroups` (0), `mPendingGroups` (1) or `mFinishedGroups` (2)
    fn player_groups(&mut self, player: u64, list: u32) -> Vec<u64>;
    /// the list emptied; its first entry `group` erased
    fn player_groups_clear(&mut self, player: u64, list: u32);
    fn player_groups_erase(&mut self, player: u64, list: u32, group: u64);
    /// the player's `mActiveGroup` (0) or `mSelectedGroup` (1)
    fn player_selection(&mut self, player: u64, which: u32) -> u64;
    /// `push_back` onto the player's running (0), pending (1) or finished (2) groups
    fn player_groups_push(&mut self, player: u64, list: u32, group: u64);
    /// `EEActiveGroup::ComputeValues`
    fn compute_values(&mut self, group: u64);
    /// the goal's `GetGoalLocation`
    fn goal_location(&mut self, goal: u64) -> [f32; 3];
    /// `delete goal`
    fn delete_goal(&mut self, goal: u64);
    /// the goal is a location goal using group movement (`SetGoal`)
    fn goal_moves_together(&mut self, goal: u64) -> bool;
    /// `ee_units_keeps_non_citizen` (0), `ee_units_keeps_ground_attacker` (1), `ee_units_keeps_citizen` (2)
    fn keeps(&mut self, test: u32, id: u32) -> bool;
    /// `ee_units_fill_members` and its capture (TEST ONLY)
    fn fill_members(&mut self, player: u64, ids: &[u32], slot: i64) -> Vec<u32>;
    /// `gWorld.mMap->ClearU1955DC()`
    fn clear_area_outlines(&mut self);
    /// `gWorld.mMap->BuildAreaOutlines(areas, continents)` (area: left, right, top, bottom)
    fn area_outlines(&mut self, areas: &[[i64; 4]], continents: &[u16]);
    /// `gAEManager.mTemplates[template]->mRadius`
    fn area_effect_radius(&mut self, template: i64) -> f32;
    /// the player's units from `mFirstUnit` by `mNext` (`EEUnit*`)
    fn player_units(&mut self, player: u64) -> Vec<u64>;
    /// the world's players `gWorld.mPlayers[0 .. gWorld.mNumPlayers]` (0 for an empty slot)
    fn world_players(&mut self) -> Vec<u64>;
}

const UNIT_GONE: i64 = -10000;
const KEEP_FORMATION: c_ulong = 8;
const OK: i32 = 0;
/// `kActionIdle`
const ACTION_IDLE: c_long = 5000;
/// `kUnitTypeS10NavalMine`, `kUnitTypeAirport`, `kUnitTypeSpaceDock`
const TYPE_NAVAL_MINE: i64 = 0x182;
const TYPE_AIRPORT: i64 = 0x42;
const TYPE_SPACE_DOCK: i64 = 0x2d8;
/// `kUnitCategoryBuilding`, `kUnitCategoryAirplane`, `kUnitCategoryPlanetSpaceship`
const CATEGORY_BUILDING: i64 = 2;
const CATEGORY_AIRPLANE: i64 = 3;
const CATEGORY_PLANET_SPACESHIP: i64 = 0x22;
/// `kMovementAir`
const MOVEMENT_AIR: u8 = 1;
/// `EESGUnitSorter`'s wounded share
const WOUNDED: f32 = 0.1;

// ---- life

/// A new group of `kind` for `player` (`new EESavedGroup(player, index)`, `new EESelectedGroup(player)`, `new
/// EEActiveGroup(player)`): one reference.
pub fn new<H: GroupHost>(h: &mut H, kind: u8, player: u64, index: c_long) -> G {
    let alloc = Box::new(Alloc {
        head: EeUnitsGroup {
            mPlayer: player as usize as *mut c_void,
            mFormation: std::ptr::null_mut(),
            mKindFlags: 0,
            mSavedGroup: std::ptr::null_mut(),
            mChangeTime: 0,
            mNeedsSort: false,
            mGroupKind: kind,
            mIndex: index,
            mGoal: std::ptr::null_mut(),
            mFields: EeUnitsActiveGroupFields::default(),
        },
        data: GroupData { references: 1, ..Default::default() },
    });
    let g = Box::into_raw(alloc) as G;
    let formation = h.formation_new(0);
    // SAFETY: the new group
    unsafe { head(g) }.mFormation = formation as usize as *mut c_void;
    if kind == EE_UNITS_GROUP_KIND_ACTIVE {
        // SAFETY: as above
        let f = &mut unsafe { head(g) }.mFields;
        f.mAction = ACTION_IDLE;
        f.mIsFinished = false;
        f.mIsQueued = false;
        f.mMovesTogether = false;
        f.mIsSpecialAttack = false;
        h.compute_values(g as u64);
    }
    g
}

/// `Reference()`.
///
/// # Safety
/// `g` is a live group.
pub unsafe fn reference(g: G) {
    // SAFETY: the caller's contract
    let d = unsafe { data(g) };
    d.references = d.references.wrapping_add(1);
}

/// `Dereference()`: the last one destroys the group.
///
/// # Safety
/// `g` is a live group; after the last reference no one uses it.
pub unsafe fn dereference<H: GroupHost>(h: &mut H, g: G) {
    // SAFETY: the caller's contract
    let d = unsafe { data(g) };
    d.references = d.references.wrapping_sub(1);
    if d.references == 0 {
        // SAFETY: the last reference
        unsafe { destroy(h, g) }
    }
}

/// The reference count (tests, `GetReferenceCount`).
///
/// # Safety
/// `g` is a live group.
pub unsafe fn references(g: G) -> u32 {
    // SAFETY: the caller's contract
    unsafe { data(g) }.references
}

/// The kind's destructor, then `~EEGroup` (its path requests cancelled and deleted, its formation released), then
/// the memory.
unsafe fn destroy<H: GroupHost>(h: &mut H, g: G) {
    // SAFETY: a live group being destroyed
    unsafe {
        match head(g).mGroupKind {
            EE_UNITS_GROUP_KIND_SELECTED => clear(h, g),
            EE_UNITS_GROUP_KIND_ACTIVE => {
                clear(h, g);
                clear_goal(h, g);
                data(g).u48.clear();
                data(g).u58.clear();
            }
            _ => clear(h, g),
        }
        let requests = std::mem::take(&mut data(g).requests);
        for (_, r) in requests {
            h.path_cancel(r.path);
            release_request(h, r);
        }
        let formation = head(g).mFormation as u64;
        if formation != 0 {
            h.formation_dereference(formation);
        }
        drop(Box::from_raw(g as *mut Alloc));
    }
}

/// `delete` a path request: its path released.
fn release_request<H: GroupHost>(h: &mut H, r: PathRequest) {
    if r.path != 0 {
        h.path_dereference(r.path);
    }
}

/// `EEGroupPathRequest::Cancel`: the path manager drops the path, which is released.
fn cancel_request<H: GroupHost>(h: &mut H, r: &mut PathRequest) {
    if r.path == 0 {
        return;
    }
    h.path_cancel(r.path);
    h.path_dereference(r.path);
    r.path = 0;
}

// ---- the plain group (EEGroup)

/// `GetNumUnits()`.
///
/// # Safety
/// `g` is a live group.
pub unsafe fn len(g: G) -> usize {
    // SAFETY: the caller's contract
    unsafe { data(g) }.units.len()
}

/// `GetUnit(i)` (0 past the end, where the original read past it).
///
/// # Safety
/// As [`len`].
pub unsafe fn unit_at(g: G, i: usize) -> u64 {
    // SAFETY: the caller's contract
    unsafe { data(g) }.units.get(i).copied().unwrap_or(0)
}

/// The units (a copy).
///
/// # Safety
/// As [`len`].
pub unsafe fn units(g: G) -> Vec<u64> {
    // SAFETY: the caller's contract
    unsafe { data(g) }.units.clone()
}

/// `GetFirstUnit()`: 0 when empty.
///
/// # Safety
/// As [`len`].
pub unsafe fn first_unit(g: G) -> u64 {
    // SAFETY: the caller's contract
    unsafe { unit_at(g, 0) }
}

/// `Contains(unit)`.
///
/// # Safety
/// As [`len`].
pub unsafe fn contains(g: G, unit: u64) -> bool {
    // SAFETY: the caller's contract
    unsafe { data(g) }.units.contains(&unit)
}

/// `HasUnitType(type)`.
///
/// # Safety
/// As [`len`].
pub unsafe fn has_unit_type(g: G, unit_type: i64) -> bool {
    // SAFETY: the caller's contract
    unsafe { data(g) }.type_counts.contains_key(&unit_type)
}

/// `GetUnitTypeCount(type)`: 0 when absent.
///
/// # Safety
/// As [`len`].
pub unsafe fn unit_type_count(g: G, unit_type: i64) -> i64 {
    // SAFETY: the caller's contract
    unsafe { data(g) }.type_counts.get(&unit_type).copied().unwrap_or(0)
}

/// The type counts in their order (the active groups' save, tests).
///
/// # Safety
/// As [`len`].
pub unsafe fn type_counts(g: G) -> Vec<(i64, i64)> {
    // SAFETY: the caller's contract
    unsafe { data(g) }.type_counts.iter().map(|(&k, &v)| (k, v)).collect()
}

/// `EEGroup::Add`: the unit (referenced) counted by type and kind flags.
///
/// # Safety
/// `g` is a live group, `unit` a live complex unit.
pub(crate) unsafe fn base_add<H: GroupHost>(h: &mut H, g: G, unit: u64) {
    let u = h.unit(unit);
    // SAFETY: a live group
    unsafe { data(g) }.units.push(unit);
    h.reference_unit(unit);
    // SAFETY: as above
    unsafe {
        head(g).mKindFlags |= u.kind_flags as c_long;
        *data(g).type_counts.entry(u.unit_type).or_insert(0) += 1;
    }
}

/// `Add(unit)` (virtual): a hot-key group takes a living unit and refreshes its button when it became the first; a
/// selection adds a valid unit not in it yet (`AddIfAbsent(unit, true)`); an active group takes it as a member
/// without moving it out of its old group (`AddMember(unit, false)`).
///
/// # Safety
/// `g` is a live group, `unit` a live complex unit.
pub unsafe fn add<H: GroupHost>(h: &mut H, g: G, unit: u64) {
    // SAFETY: the caller's contract
    unsafe {
        match head(g).mGroupKind {
            EE_UNITS_GROUP_KIND_SAVED => {
                if h.unit(unit).hit_points <= 0 {
                    return;
                }
                base_add(h, g, unit);
                if unit == first_unit(g) {
                    let index = head(g).mIndex;
                    h.post_saved_group(index);
                }
            }
            EE_UNITS_GROUP_KIND_SELECTED => add_if_absent(h, g, unit, true),
            _ => add_member(h, g, unit, false),
        }
    }
}

/// `EEGroup::Clear`: every unit released (from the first), the type counts and kind flags reset.
unsafe fn base_clear<H: GroupHost>(h: &mut H, g: G) {
    // SAFETY: a live group
    unsafe {
        while let Some(&u) = data(g).units.first() {
            h.dereference_unit(u);
            data(g).units.remove(0);
        }
        head(g).mKindFlags = 0;
        data(g).type_counts.clear();
    }
}

/// `Clear()` (virtual): a selection also drops its sort flag, refreshes the area outlines and stamps the time; an
/// active group first unlinks its units (their `mActiveGroup` released) and recomputes its speeds after.
///
/// # Safety
/// `g` is a live group.
pub unsafe fn clear<H: GroupHost>(h: &mut H, g: G) {
    // SAFETY: the caller's contract
    unsafe {
        match head(g).mGroupKind {
            EE_UNITS_GROUP_KIND_SELECTED => {
                base_clear(h, g);
                head(g).mNeedsSort = false;
                show_selection_areas(h, g);
                head(g).mChangeTime = h.time();
            }
            EE_UNITS_GROUP_KIND_ACTIVE => {
                let count = len(g);
                for i in 0..count {
                    let unit = unit_at(g, i);
                    let old = h.unit(unit).active_group;
                    if old != 0 {
                        dereference(h, old as G);
                    }
                    h.set_active_group_member(unit, 0);
                }
                base_clear(h, g);
                h.compute_values(g as u64);
            }
            _ => base_clear(h, g),
        }
    }
}

/// `UpdateKindFlags()`.
///
/// # Safety
/// `g` is a live group.
pub unsafe fn update_kind_flags<H: GroupHost>(h: &mut H, g: G) {
    // SAFETY: the caller's contract
    unsafe {
        head(g).mKindFlags = 0;
        for i in 0..len(g) {
            let u = unit_at(g, i);
            head(g).mKindFlags |= h.unit(u).kind_flags as c_long;
        }
    }
}

/// `RemoveAt(i)`: the unit released; its type's last unit leaving recomputes the kind flags.
unsafe fn remove_at<H: GroupHost>(h: &mut H, g: G, i: usize) {
    // SAFETY: a live group
    unsafe {
        let unit = data(g).units[i];
        let unit_type = h.unit(unit).unit_type;
        h.dereference_unit(unit);
        data(g).units.remove(i);
        let last = match data(g).type_counts.get_mut(&unit_type) {
            Some(n) => {
                *n -= 1;
                *n == 0
            }
            None => false,
        };
        if last {
            data(g).type_counts.remove(&unit_type);
            update_kind_flags(h, g);
        }
    }
}

/// `EEGroup::RemoveUnit`.
unsafe fn base_remove_unit<H: GroupHost>(h: &mut H, g: G, unit: u64) {
    // SAFETY: a live group
    unsafe {
        if let Some(i) = data(g).units.iter().position(|&u| u == unit) {
            remove_at(h, g, i);
        }
    }
}

/// `RemoveUnit(unit)` (virtual): a hot-key group refreshes its button first when the unit was its first; a selection
/// refreshes the outlines and the user interface after and forgets its saved group; an active group also unlinks
/// the unit, forgets its destination, recomputes its speeds and takes it out of its path request (cancelling one
/// left empty). The unit is held across (the original's use after free of a dead unit's last reference).
///
/// # Safety
/// `g` is a live group, `unit` a live complex unit.
pub unsafe fn remove_unit<H: GroupHost>(h: &mut H, g: G, unit: u64) {
    // SAFETY: the caller's contract
    unsafe {
        match head(g).mGroupKind {
            EE_UNITS_GROUP_KIND_SAVED => {
                if unit == first_unit(g) {
                    let index = head(g).mIndex;
                    h.post_saved_group(index);
                }
                base_remove_unit(h, g, unit);
            }
            EE_UNITS_GROUP_KIND_SELECTED => {
                base_remove_unit(h, g, unit);
                refresh_selection_areas(h, g, unit);
                if head(g).mPlayer as u64 == h.current_player() {
                    h.post_selection();
                }
                head(g).mSavedGroup = std::ptr::null_mut();
                head(g).mChangeTime = h.time();
            }
            _ => {
                h.reference_unit(unit);
                base_remove_unit(h, g, unit);
                let old = h.unit(unit).active_group;
                if old != 0 {
                    dereference(h, old as G);
                }
                h.set_active_group_member(unit, 0);
                let id = h.unit(unit).id;
                data(g).destinations.remove(&id);
                h.compute_values(g as u64);
                if !data(g).requests.is_empty() {
                    if let Some(key) = find_path_request(g, id) {
                        let r = data(g).requests.get_mut(&key).expect("the request found");
                        if let Some(at) = r.unit_ids.iter().position(|&x| x == id as c_long) {
                            r.unit_ids.remove(at);
                        }
                        if r.unit_ids.is_empty() {
                            let path = r.path;
                            let path_id = h.path_id(path);
                            cancel_path_request(h, g, path_id);
                        }
                    }
                }
                h.dereference_unit(unit);
            }
        }
    }
}

/// `Remove(id)` and `CancelPathRequest(id)`: the request of that path id cancelled and deleted.
///
/// # Safety
/// `g` is a live group.
pub unsafe fn cancel_path_request<H: GroupHost>(h: &mut H, g: G, id: i64) {
    // SAFETY: the caller's contract
    let Some(mut r) = (unsafe { data(g) }).requests.remove(&id) else { return };
    cancel_request(h, &mut r);
    release_request(h, r);
}

/// `FindPathRequest(unit)`: the key of the first request (in key order) that carries the unit id.
///
/// # Safety
/// `g` is a live group.
pub unsafe fn find_path_request(g: G, id: u32) -> Option<i64> {
    // SAFETY: the caller's contract
    unsafe { data(g) }.requests.iter().find(|(_, r)| r.unit_ids.contains(&(id as c_long))).map(|(&k, _)| k)
}

/// The path of the request carrying `id` (`FindPathRequest(unit)->mPath`, 0 for none).
///
/// # Safety
/// `g` is a live group.
pub unsafe fn path_of_unit(g: G, id: u32) -> u64 {
    // SAFETY: the caller's contract
    unsafe { find_path_request(g, id).map_or(0, |k| data(g).requests[&k].path) }
}

/// The path requests (tests, the save).
///
/// # Safety
/// `g` is a live group.
pub unsafe fn requests(g: G) -> Vec<(i64, PathRequest)> {
    // SAFETY: the caller's contract
    unsafe { data(g) }.requests.iter().map(|(&k, r)| (k, r.clone())).collect()
}

/// `SetFormation(formation)`: the reference moves.
///
/// # Safety
/// `g` is a live group.
pub unsafe fn set_formation<H: GroupHost>(h: &mut H, g: G, formation: u64) {
    // SAFETY: the caller's contract
    let old = unsafe { head(g) }.mFormation as u64;
    if formation == old {
        return;
    }
    if old != 0 {
        h.formation_dereference(old);
    }
    // SAFETY: as above
    unsafe { head(g) }.mFormation = formation as usize as *mut c_void;
    if formation != 0 {
        h.formation_reference(formation);
    }
}

/// `GetBounds(out)`: `out` (bottom, left, right, top) set to the first unit on the map's point and extended by the
/// others'; untouched when none is on the map.
///
/// # Safety
/// `g` is a live group.
pub unsafe fn bounds<H: GroupHost>(h: &mut H, g: G, out: &mut [f32; 4]) {
    let mut first = false;
    // SAFETY: the caller's contract
    for i in 0..unsafe { len(g) } {
        // SAFETY: as above
        let u = h.unit(unsafe { unit_at(g, i) });
        let (x, y) = (u.location[0], u.location[1]);
        if !u.has_tile || u.tile_x == UNIT_GONE {
            continue;
        }
        if first {
            out[1] = if out[1] < x { out[1] } else { x };
            out[2] = if out[2] > x { out[2] } else { x };
            out[3] = if out[3] < y { out[3] } else { y };
            out[0] = if out[0] > y { out[0] } else { y };
        } else {
            first = true;
            out[0] = y;
            out[2] = x;
            out[3] = y;
            out[1] = x;
        }
    }
}

/// `GetFlags()`: the terrain flags every unit shares (from the last unit).
///
/// # Safety
/// `g` is a live group.
pub unsafe fn flags<H: GroupHost>(h: &mut H, g: G) -> [u8; 17] {
    let mut result = [1u8; 17];
    // SAFETY: the caller's contract
    for i in (0..unsafe { len(g) }).rev() {
        // SAFETY: as above
        let f = h.terrain_flags(unsafe { unit_at(g, i) });
        for k in (0..17).rev() {
            result[k] = u8::from(result[k] != 0 && f[k] != 0);
        }
    }
    result
}

/// `FindUnitOfCategory(category)`: the first such unit, or 0.
///
/// # Safety
/// `g` is a live group.
pub unsafe fn find_unit_of_category<H: GroupHost>(h: &mut H, g: G, category: i64) -> u64 {
    // SAFETY: the caller's contract
    unsafe { units(g) }.into_iter().find(|&u| h.unit(u).category == category).unwrap_or(0)
}

/// `FindUnitWithFlags(flags)`: the first unit whose class has one of the kind flags (none unless the group has one).
///
/// # Safety
/// `g` is a live group.
pub unsafe fn find_unit_with_flags<H: GroupHost>(h: &mut H, g: G, flags: c_ulong) -> u64 {
    // SAFETY: the caller's contract
    if (unsafe { head(g) }.mKindFlags as c_ulong & flags) == 0 {
        return 0;
    }
    // SAFETY: as above
    unsafe { units(g) }.into_iter().find(|&u| (h.unit(u).kind_flags as c_long as c_ulong & flags) != 0).unwrap_or(0)
}

/// `CountUnitsOfKind(category)`.
///
/// # Safety
/// `g` is a live group.
pub unsafe fn count_units_of_kind<H: GroupHost>(h: &mut H, g: G, category: i64) -> c_long {
    // SAFETY: the caller's contract
    unsafe { units(g) }.into_iter().filter(|&u| h.unit(u).category == category).count() as c_long
}

/// `CopyGroup(other)`: the other group's units (references moved), type counts and kind flags.
///
/// # Safety
/// `g` and `other` are live groups.
pub unsafe fn copy_group<H: GroupHost>(h: &mut H, g: G, other: G) {
    // SAFETY: the caller's contract
    unsafe {
        for u in units(g) {
            h.dereference_unit(u);
        }
        data(g).type_counts = data(other).type_counts.clone();
        data(g).units = data(other).units.clone();
        head(g).mKindFlags = head(other).mKindFlags;
        for u in units(g) {
            h.reference_unit(u);
        }
    }
}

/// `CopyUnits(other)`: emptied (its kind's Clear), the other's formation, then each of the other's valid units on
/// the map added (its kind's Add).
///
/// # Safety
/// `g` and `other` are live groups.
pub unsafe fn copy_units<H: GroupHost>(h: &mut H, g: G, other: G) {
    // SAFETY: the caller's contract
    unsafe {
        let count = len(other);
        clear(h, g);
        let formation = head(other).mFormation as u64;
        set_formation(h, g, formation);
        data(g).units.reserve(count);
        for i in 0..count {
            let unit = unit_at(other, i);
            let u = h.unit(unit);
            if u.valid && u.tile_x != UNIT_GONE && u.tile_y != UNIT_GONE {
                add(h, g, unit);
            }
        }
    }
}

/// `MoveAlong(request)`: each living, enabled unit on the map of the request still in the group follows its
/// formation slot at each step of the found path (the step's centre when the slot lies in another path area); an
/// empty path when none was found. The formation's centre is put back after.
unsafe fn move_along<H: GroupHost>(h: &mut H, g: G, r: &PathRequest) {
    let mut units = Vec::new();
    let mut paths: Vec<Vec<[f32; 3]>> = Vec::new();
    for &id in r.unit_ids.iter().rev() {
        let unit = h.world_unit(id as i64);
        if unit == 0 || !h.is_complex(unit) {
            continue;
        }
        let u = h.unit(unit);
        // SAFETY: a live group
        if u.hit_points <= 0 || u.disabled || !u.has_tile || !unsafe { contains(g, unit) } {
            continue;
        }
        units.push(unit);
        paths.push(Vec::new());
    }
    // SAFETY: as above
    let formation = unsafe { head(g) }.mFormation as u64;
    let original = if formation != 0 { h.formation_center(formation) } else { [0.0; 3] };
    let tiles = if r.path != 0 { h.path_tiles(r.path) } else { Vec::new() };
    if r.path != 0 && tiles.len() > 1 {
        if !units.is_empty() {
            let mut steps = Vec::new();
            for &(x, y) in &tiles[1..tiles.len() - 1] {
                let (sx, sy) = h.subsector_at(x, y);
                steps.push([sx as c_long as f32 + 0.5, sy as c_long as f32 + 0.5, 0.0]);
            }
            for &point in &steps {
                let mut slots = Vec::new();
                if formation != 0 {
                    h.set_formation_center(formation, point);
                    slots = h.formation_place(formation, &units);
                }
                if slots.is_empty() {
                    slots.push(point);
                }
                let area = h.path_area_at(um_floor(point[0]) as i64, um_floor(point[1]) as i64);
                for path in paths.iter_mut().rev() {
                    let slot = *slots.last().expect("a slot");
                    let (sx, sy) = (um_floor(slot[0]) as i64, um_floor(slot[1]) as i64);
                    match h.slot_area_at(sx, sy) {
                        Some(a) if a == area => path.push(slot),
                        _ => path.push(point),
                    }
                    if slots.len() > 1 {
                        slots.pop();
                    }
                }
            }
            for (u, path) in units.iter().zip(paths.iter()).rev() {
                h.set_group_path(*u, path);
            }
        }
    } else {
        for &u in units.iter().rev() {
            h.set_group_path(u, &[]);
        }
    }
    if formation != 0 {
        h.set_formation_center(formation, original);
    }
}

use crate::units::load::um_floor;

/// `MoveAlongFoundPaths()`: the units of every request whose path is found follow it, and the request goes.
///
/// # Safety
/// `g` is a live group.
pub unsafe fn move_along_found_paths<H: GroupHost>(h: &mut H, g: G) {
    let mut from: Option<i64> = None;
    loop {
        // SAFETY: the caller's contract
        let next = {
            let d = unsafe { data(g) };
            match from {
                None => d.requests.iter().next(),
                Some(k) => d.requests.range(k + 1..).next(),
            }
            .map(|(&k, r)| (k, r.path))
        };
        let Some((key, path)) = next else { break };
        from = Some(key);
        if path == 0 || !h.path_done(path) {
            continue;
        }
        // SAFETY: as above
        let r = unsafe { data(g) }.requests.remove(&key).expect("the request");
        // SAFETY: as above
        unsafe { move_along(h, g, &r) };
        release_request(h, r);
    }
}

/// `RequestPath(from, to, cluster)`: one path for the cluster's units; followed at once when already found, else
/// kept by its id.
unsafe fn request_path<H: GroupHost>(h: &mut H, g: G, from: (i64, i64), to: (i64, i64), cluster: &Cluster) {
    if cluster.units.is_empty() {
        return;
    }
    let ids: Vec<c_long> = cluster.units.iter().rev().map(|&u| h.unit(u).id as c_long).collect();
    // SAFETY: a live group
    let player = unsafe { head(g) }.mPlayer as u64;
    let player_id = h.player_id(player);
    let path = h.path_request(player_id, from, to, &cluster.flags);
    if path != 0 {
        h.path_reference(path);
    }
    let r = PathRequest { path, unit_ids: ids, ..Default::default() };
    h.path_dereference(path);
    if path != 0 && h.path_done(path) {
        // SAFETY: as above
        unsafe { move_along(h, g, &r) };
        release_request(h, r);
    } else {
        let key = h.path_id(path);
        // SAFETY: as above (std::map::insert keeps an existing key: the new request is never freed, its path kept)
        if let std::collections::btree_map::Entry::Vacant(e) = unsafe { data(g) }.requests.entry(key) {
            e.insert(r);
        }
    }
}

/// `EEGroup::MakeClusters(units, clusters, distance, square)` (from the last unit): land buildings by continent
/// (kind 3), flyers by continent (kind 2), the others into the latest cluster with the same terrain flags whose
/// bounds contain the tile or lie within the distance of it (kind 1, a new one else). The continents' and the
/// flyers' clusters follow, in continent order.
pub fn make_clusters<H: GroupHost>(h: &mut H, units: &[u64], distance: i64, square: bool) -> Vec<Cluster> {
    let distance = if square { distance.wrapping_mul(distance) } else { distance };
    let mut continents: BTreeMap<u16, Vec<u64>> = BTreeMap::new();
    let mut flyers: BTreeMap<u16, Vec<u64>> = BTreeMap::new();
    let mut out: Vec<Cluster> = Vec::new();
    for &unit in units.iter().rev() {
        let continent = h.continent_of(unit);
        let u = h.unit(unit);
        if u.land_building {
            continents.entry(continent).or_default().push(unit);
        } else if u.domains & MOVEMENT_AIR != 0 {
            flyers.entry(continent).or_default().push(unit);
        } else {
            let flags = h.terrain_flags(unit);
            let (tx, ty) = (u.tile_x, u.tile_y);
            let mut new_cluster = true;
            for c in out.iter_mut().rev() {
                if c.flags != flags {
                    continue;
                }
                let mut near = !square || distance < 1;
                if !near {
                    let cx = (c.right.wrapping_sub(c.left)) / 2 + c.left;
                    let cy = (c.bottom.wrapping_sub(c.top)) / 2 + c.top;
                    let contained = tx >= c.left && tx <= c.right && ty >= c.top && ty <= c.bottom;
                    if contained || square_distance((cx, cy), (tx, ty)) <= distance {
                        near = true;
                    } else {
                        let corner = closest_corner(c, (tx, ty));
                        near = square_distance((tx, ty), corner) <= distance;
                    }
                }
                if !near {
                    continue;
                }
                new_cluster = false;
                c.units.push(unit);
                c.left = c.left.min(tx);
                c.right = c.right.max(tx);
                c.top = c.top.min(ty);
                c.bottom = c.bottom.max(ty);
                break;
            }
            if new_cluster {
                out.push(Cluster {
                    units: vec![unit],
                    flags,
                    kind: 1,
                    bottom: ty,
                    left: tx,
                    right: tx,
                    top: ty,
                    continent: 0,
                });
            }
        }
    }
    for (continent, units) in continents {
        out.push(Cluster { units, kind: 3, continent, ..Default::default() });
    }
    for (continent, units) in flyers {
        out.push(Cluster { units, kind: 2, continent, ..Default::default() });
    }
    out
}

/// `U2DPoint<long>::DistanceToSquared`.
fn square_distance(a: (i64, i64), b: (i64, i64)) -> i64 {
    let dx = a.0.wrapping_sub(b.0);
    let dy = a.1.wrapping_sub(b.1);
    dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy))
}

/// `U2DRectangle<long>::GetClosestCorner(point)`: top left, bottom left, top right, bottom right; the first on ties
/// (the distances as `DistanceToSquaredF`: the x part converted, then the y part added as a float).
fn closest_corner(c: &Cluster, p: (i64, i64)) -> (i64, i64) {
    let distance = |q: (i64, i64)| -> f32 {
        let dx = p.0.wrapping_sub(q.0);
        let dy = p.1.wrapping_sub(q.1);
        dx.wrapping_mul(dx) as c_long as f32 + dy.wrapping_mul(dy) as c_long as f32
    };
    let mut closest = (c.left, c.top);
    let mut best = distance(closest);
    for corner in [(c.left, c.bottom), (c.right, c.top), (c.right, c.bottom)] {
        let d = distance(corner);
        if d < best {
            best = d;
            closest = corner;
        }
    }
    closest
}

/// `Load(major, minor, file)` (virtual): the formation kind (8 keeps it) and the unit ids (`RepairPointers` makes
/// them units); a selection then its saved group's index. The first read error.
///
/// # Safety
/// `g` is a live group; `file`'s primitives are the saved game's.
pub unsafe fn load<H: GroupHost, F: PlayerSaveHost + ?Sized>(h: &mut H, g: G, file: &mut F) -> i32 {
    let mut kind: c_ulong = KEEP_FORMATION;
    file.read_ulong(&mut kind);
    if kind != KEEP_FORMATION {
        let formation = h.formation_new(kind as c_long as i64);
        if formation != 0 {
            // SAFETY: the caller's contract
            let old = unsafe { head(g) }.mFormation as u64;
            if old != 0 {
                h.formation_dereference(old);
            }
            // SAFETY: as above
            unsafe { head(g) }.mFormation = formation as usize as *mut c_void;
        }
    }
    let mut count: c_ulong = 0;
    let mut result = file.read_ulong(&mut count);
    if result == OK {
        let mut i: c_ulong = 0;
        while i < count && result == OK {
            let mut id: c_ulong = 0;
            result = file.read_ulong(&mut id);
            // SAFETY: as above (the id in the pointer's place until RepairPointers)
            unsafe { data(g) }.units.push(id as u64);
            i += 1;
        }
    }
    // SAFETY: as above
    if unsafe { head(g) }.mGroupKind != EE_UNITS_GROUP_KIND_SELECTED || result != OK {
        return result;
    }
    let mut index: c_ulong = 0;
    result = file.read_ulong(&mut index);
    // SAFETY: as above (`USavedPointer`: the low 32 bits sign-extended)
    unsafe { head(g) }.mSavedGroup = (index as u32 as i32 as isize) as G;
    result
}

/// `Save(major, minor, file)` (virtual): the formation kind (8 for none), the unit count and ids; a selection then
/// its saved group's index (the last hot-key slot holding it, -1 for none). The first write error.
///
/// # Safety
/// `g` is a live group.
pub unsafe fn save<H: GroupHost, F: PlayerSaveHost + ?Sized>(h: &mut H, g: G, file: &mut F) -> i32 {
    // SAFETY: the caller's contract
    let formation = unsafe { head(g) }.mFormation as u64;
    let kind = if formation != 0 { h.formation_kind(formation) as c_ulong } else { KEEP_FORMATION };
    // SAFETY: as above
    let units = unsafe { units(g) };
    let mut result = file.write_ulong(kind);
    if result != OK {
        return result;
    }
    result = file.write_ulong(units.len() as c_ulong);
    if result != OK {
        return result;
    }
    for &u in &units {
        if result != OK {
            break;
        }
        result = file.write_ulong(h.unit(u).id as c_ulong);
    }
    // SAFETY: as above
    if unsafe { head(g) }.mGroupKind != EE_UNITS_GROUP_KIND_SELECTED || result != OK {
        return result;
    }
    // SAFETY: as above
    let (player, saved) = unsafe { (head(g).mPlayer as u64, head(g).mSavedGroup as u64) };
    let mut index = c_ulong::MAX;
    for i in 0..20 {
        if h.player_group(player, i) == saved {
            index = i as c_ulong;
        }
    }
    file.write_ulong(index)
}

/// `RepairPointers(major, minor)` (virtual): the loaded ids back to units (its kind's Add; the ids no unit has
/// dropped); a selection then its saved group from its index (-1: none) and its outlines. 0.
///
/// # Safety
/// `g` is a live group after a [`load`].
pub unsafe fn repair_pointers<H: GroupHost>(h: &mut H, g: G) -> i32 {
    // SAFETY: the caller's contract
    unsafe {
        let ids: Vec<i32> = data(g).units.iter().map(|&p| p as u32 as i32).collect();
        data(g).units.clear();
        for id in ids {
            let unit = h.world_unit(id as i64);
            if unit == 0 {
                continue;
            }
            let complex = h.unit(unit).complex;
            if complex != 0 {
                add(h, g, complex);
            }
        }
        if head(g).mGroupKind == EE_UNITS_GROUP_KIND_SELECTED {
            let saved = head(g).mSavedGroup as usize;
            head(g).mSavedGroup = if saved == usize::MAX {
                std::ptr::null_mut()
            } else {
                let player = head(g).mPlayer as u64;
                h.player_group(player, saved as u32 as i32 as i64) as G
            };
            show_selection_areas(h, g);
        }
    }
    OK
}

/// `Validate()` (virtual): a hot-key group drops dead, invalid and given-away units (refreshing its button when the
/// first went); a selection drops units that are neither valid nor on a tile (but for aircraft and spaceships in a
/// carrier), follows its found paths and refreshes its outlines; an active group drops invalid and dead units, then
/// follows its found paths or finishes when left empty.
///
/// # Safety
/// `g` is a live group.
pub unsafe fn validate<H: GroupHost>(h: &mut H, g: G) {
    // SAFETY: the caller's contract
    unsafe {
        match head(g).mGroupKind {
            EE_UNITS_GROUP_KIND_SAVED => {
                let mut first_removed = false;
                let mut i = 0;
                while i < len(g) {
                    let unit = unit_at(g, i);
                    let u = h.unit(unit);
                    if u.valid && u.hit_points > 0 && u.player == head(g).mPlayer as u64 {
                        i += 1;
                    } else {
                        remove_unit(h, g, unit);
                        if i == 0 {
                            first_removed = true;
                        }
                    }
                }
                if first_removed {
                    let index = head(g).mIndex;
                    h.post_saved_group(index);
                }
            }
            EE_UNITS_GROUP_KIND_SELECTED => {
                let mut i = 0;
                while i < len(g) {
                    let unit = unit_at(g, i);
                    let u = h.unit(unit);
                    let carried = (u.category == CATEGORY_AIRPLANE || u.category == CATEGORY_PLANET_SPACESHIP)
                        && u.container != 0;
                    if u.valid || u.has_tile || carried {
                        i += 1;
                    } else {
                        remove_unit(h, g, unit);
                    }
                }
                if !data(g).requests.is_empty() {
                    move_along_found_paths(h, g);
                }
                if len(g) > 0 {
                    show_selection_areas(h, g);
                }
            }
            _ => {
                let mut i = 0;
                while i < len(g) {
                    let unit = unit_at(g, i);
                    let u = h.unit(unit);
                    if u.valid && u.hit_points > 0 {
                        i += 1;
                    } else {
                        remove_unit(h, g, unit);
                    }
                }
                if !data(g).requests.is_empty() {
                    move_along_found_paths(h, g);
                } else if len(g) == 0 {
                    finish(h, g);
                }
            }
        }
    }
}

// ---- the hot-key group (EESavedGroup)

/// `EESavedGroup::Assign(selection)` (`EEGroup::Assign`): the sorted selection's units, the button refreshed, and the
/// selection's saved group this one.
///
/// # Safety
/// `g` is a live hot-key group, `selection` a live selection.
pub unsafe fn assign<H: GroupHost>(h: &mut H, g: G, selection: G) {
    // SAFETY: the caller's contract
    unsafe {
        sort_units(h, selection);
        copy_units(h, g, selection);
        let index = head(g).mIndex;
        h.post_saved_group(index);
        head(selection).mSavedGroup = g;
    }
}

// ---- the selection (EESelectedGroup)

/// `CopySelection(other)`: its units, sort flag and saved group; the time stamped.
///
/// # Safety
/// `g` and `other` are live selections.
pub unsafe fn copy_selection<H: GroupHost>(h: &mut H, g: G, other: G) {
    // SAFETY: the caller's contract
    unsafe {
        copy_group(h, g, other);
        head(g).mNeedsSort = head(other).mNeedsSort;
        head(g).mSavedGroup = head(other).mSavedGroup;
        head(g).mChangeTime = h.time();
    }
}

/// `AddUnit(unit, sort)`: added (marked for sorting when asked); for the current player the user interface told
/// and the outlines refreshed; the time stamped.
///
/// # Safety
/// `g` is a live selection, `unit` a live complex unit.
pub unsafe fn add_unit<H: GroupHost>(h: &mut H, g: G, unit: u64, sort: bool) {
    // SAFETY: the caller's contract
    unsafe {
        base_add(h, g, unit);
        if sort {
            head(g).mNeedsSort = true;
        }
        if head(g).mPlayer as u64 == h.current_player() {
            h.post_selection();
            refresh_selection_areas(h, g, unit);
        }
        head(g).mChangeTime = h.time();
    }
}

/// `AddIfAbsent(unit, sort)`: a valid unit not selected yet added; the saved group forgotten.
///
/// # Safety
/// As [`add_unit`].
pub unsafe fn add_if_absent<H: GroupHost>(h: &mut H, g: G, unit: u64, sort: bool) {
    // SAFETY: the caller's contract
    unsafe {
        if h.unit(unit).valid && !contains(g, unit) {
            add_unit(h, g, unit, sort);
            head(g).mSavedGroup = std::ptr::null_mut();
        }
    }
}

/// `SelectSavedGroup(group, remember)`: the saved group's units; the group remembered when asked; the time stamped.
///
/// # Safety
/// `g` is a live selection, `group` a live group.
pub unsafe fn select_saved_group<H: GroupHost>(h: &mut H, g: G, group: G, remember: bool) {
    // SAFETY: the caller's contract
    unsafe {
        copy_units(h, g, group);
        head(g).mSavedGroup = if remember { group } else { std::ptr::null_mut() };
        head(g).mChangeTime = h.time();
    }
}

/// `SortUnits()`: sorted by `EESGUnitSorter` (MSVC's `std::sort`) when a change asked for it.
///
/// # Safety
/// `g` is a live selection.
pub unsafe fn sort_units<H: GroupHost>(h: &mut H, g: G) {
    // SAFETY: the caller's contract
    if !unsafe { head(g) }.mNeedsSort {
        return;
    }
    // SAFETY: as above
    let mut keyed: Vec<(u64, GroupUnit)> = unsafe { units(g) }.into_iter().map(|u| (u, h.unit(u))).collect();
    msvc_sort::sort_by(&mut keyed, |a, b| selection_less(&a.1, &b.1));
    // SAFETY: as above
    unsafe {
        data(g).units = keyed.into_iter().map(|(u, _)| u).collect();
        head(g).mNeedsSort = false;
    }
}

/// `EESGUnitSorter::Compare`: same class healthy before wounded (10% or less of the hit points), then by id; else
/// shorter range first, then (both ranged) smaller scale, then unit type; else stronger attack, then unit type.
pub fn selection_less(a: &GroupUnit, b: &GroupUnit) -> bool {
    let second_health = b.hit_points as c_long as f32 / b.max_hit_points as c_long as f32;
    let first_health = a.hit_points as c_long as f32 / a.max_hit_points as c_long as f32;
    let first_wounded = first_health <= WOUNDED;
    let second_wounded = second_health <= WOUNDED;
    if a.class == b.class {
        if !first_wounded && second_wounded {
            true
        } else if first_wounded && !second_wounded {
            false
        } else {
            a.id < b.id
        }
    } else if a.range < b.range {
        true
    } else if a.range > b.range {
        false
    } else if !a.melee && !b.melee {
        if a.scale < b.scale {
            true
        } else if a.scale > b.scale {
            false
        } else {
            (a.unit_type as c_ulong) < (b.unit_type as c_ulong)
        }
    } else if a.attack > b.attack {
        true
    } else if a.attack < b.attack {
        false
    } else {
        (a.unit_type as c_ulong) < (b.unit_type as c_ulong)
    }
}

/// `EEAGIncreasingVelocityUnitSorter`: slower first; the same stats by fewer hit points.
pub fn velocity_less(a: &GroupUnit, b: &GroupUnit) -> bool {
    if a.stats == b.stats {
        a.hit_points < b.hit_points
    } else {
        a.max_velocity < b.max_velocity
    }
}

/// Whether the area outlines are shown (`sAreasShown`).
static AREAS_SHOWN: std::sync::atomic::AtomicBool = std::sync::atomic::AtomicBool::new(false);

/// `ShowSelectionAreas()`: for the current player's selection, the area-effect range and continent of every built
/// building of the type of the selection's first built building (not an airport) with an area effect; else the
/// outlines cleared (once).
///
/// # Safety
/// `g` is a live group.
pub unsafe fn show_selection_areas<H: GroupHost>(h: &mut H, g: G) {
    use std::sync::atomic::Ordering;
    let player = h.current_player();
    if player == 0 || g as u64 != h.selected_group_of(player) {
        return;
    }
    let mut found = None;
    // SAFETY: the caller's contract
    for unit in unsafe { units(g) } {
        let u = h.unit(unit);
        if u.category != CATEGORY_BUILDING || !u.built || u.unit_type == TYPE_AIRPORT {
            continue;
        }
        if let Some(&template) = u.ae_templates.iter().find(|&&t| t as c_long as c_ulong != 0) {
            found = Some((u.unit_type, u.ae_scale * h.area_effect_radius(template)));
            break;
        }
    }
    let Some((unit_type, radius)) = found else {
        if AREAS_SHOWN.load(Ordering::Relaxed) {
            h.clear_area_outlines();
            AREAS_SHOWN.store(false, Ordering::Relaxed);
        }
        return;
    };
    let mut areas = Vec::new();
    let mut continents = Vec::new();
    for other in h.player_units(player) {
        if h.unit(other).unit_type != unit_type {
            continue;
        }
        let complex = h.unit(other).complex;
        let c = h.unit(complex);
        if !c.built {
            continue;
        }
        let (x, y) = (c.location[0], c.location[1]);
        areas.push([
            um_floor(x - radius) as i64,
            um_floor(x + radius) as i64,
            um_floor(y - radius) as i64,
            um_floor(y + radius) as i64,
        ]);
        continents.push(if c.unit_type == TYPE_SPACE_DOCK {
            0
        } else {
            h.continent_at(um_floor(x) as i64, um_floor(y) as i64)
        });
    }
    h.area_outlines(&areas, &continents);
    AREAS_SHOWN.store(true, Ordering::Relaxed);
}

/// `RefreshSelectionAreas(unit)`: the outlines refreshed when the unit is a building.
///
/// # Safety
/// `g` is a live group.
pub unsafe fn refresh_selection_areas<H: GroupHost>(h: &mut H, g: G, unit: u64) {
    if h.unit(unit).category == CATEGORY_BUILDING {
        // SAFETY: the caller's contract
        unsafe { show_selection_areas(h, g) };
    }
}

/// `GetContinent()`: the continent most units are on (the lowest id on a tie).
///
/// # Safety
/// `g` is a live group.
pub unsafe fn continent<H: GroupHost>(h: &mut H, g: G) -> u16 {
    let mut counts: BTreeMap<u16, u32> = BTreeMap::new();
    // SAFETY: the caller's contract
    for unit in unsafe { units(g) } {
        *counts.entry(h.continent_of(unit)).or_insert(0) += 1;
    }
    let mut best: Option<(u16, u32)> = None;
    for (&k, &n) in &counts {
        if best.is_none_or(|(_, b)| n > b) {
            best = Some((k, n));
        }
    }
    best.map_or(0, |(k, _)| k)
}

// ---- the active group (EEActiveGroup)

/// `EEComplexUnit::SetActiveGroup(group)`: the unit's group reference moves to `group`.
///
/// # Safety
/// `unit` is a live complex unit, `group` a live group.
pub unsafe fn unit_set_active_group<H: GroupHost>(h: &mut H, unit: u64, group: G) {
    let old = h.unit(unit).active_group;
    if old == group as u64 {
        return;
    }
    if old != 0 {
        // SAFETY: the unit's live group
        unsafe { dereference(h, old as G) };
    }
    h.set_active_group_member(unit, group as u64);
    // SAFETY: the caller's contract
    unsafe { reference(group) };
}

/// `EEPlayer::GetActiveGroupOf(unit)`: the running group holding the unit (from the last), else the pending one.
pub fn active_group_of<H: GroupHost>(h: &mut H, player: u64, unit: u64) -> u64 {
    for list in 0..2 {
        for g in h.player_groups(player, list).into_iter().rev() {
            // SAFETY: the player's live groups
            if g != 0 && unsafe { contains(g as G, unit) } {
                return g;
            }
        }
    }
    0
}

/// `AddMember(unit, remove_old)`: a living unit (not a naval mine) joins, first leaving its old active group when
/// asked; the speeds recomputed.
///
/// # Safety
/// `g` is a live active group, `unit` a live complex unit.
pub unsafe fn add_member<H: GroupHost>(h: &mut H, g: G, unit: u64, remove_old: bool) {
    let u = h.unit(unit);
    if u.hit_points <= 0 || u.class == 0 || u.unit_type == TYPE_NAVAL_MINE {
        return;
    }
    // SAFETY: the caller's contract
    unsafe {
        if remove_old {
            let old = active_group_of(h, u.player, unit);
            if old != 0 {
                remove_unit(h, old as G, unit);
            }
        }
        unit_set_active_group(h, unit, g);
        base_add(h, g, unit);
    }
    h.compute_values(g as u64);
}

/// `ClearGoal()`.
///
/// # Safety
/// `g` is a live active group.
pub unsafe fn clear_goal<H: GroupHost>(h: &mut H, g: G) {
    // SAFETY: the caller's contract
    let goal = unsafe { head(g) }.mGoal as u64;
    if goal != 0 {
        h.delete_goal(goal);
    }
    // SAFETY: as above
    unsafe { head(g) }.mGoal = std::ptr::null_mut();
}

/// `SetGoal(goal, queued)`: the group moves together for a location goal asking for it.
///
/// # Safety
/// `g` is a live active group, `goal` a live goal.
pub unsafe fn set_goal<H: GroupHost>(h: &mut H, g: G, goal: u64, queued: bool) {
    let together = h.goal_moves_together(goal);
    // SAFETY: the caller's contract
    let hd = unsafe { head(g) };
    hd.mGoal = goal as usize as *mut c_void;
    hd.mFields.mIsQueued = queued;
    hd.mFields.mMovesTogether = together;
}

/// `Finish()`: the goal dropped, the group (referenced) into the player's finished groups.
///
/// # Safety
/// `g` is a live active group.
pub unsafe fn finish<H: GroupHost>(h: &mut H, g: G) {
    // SAFETY: the caller's contract
    unsafe {
        clear_goal(h, g);
        let player = head(g).mPlayer as u64;
        h.player_groups_push(player, 2, g as u64);
        reference(g);
        head(g).mFields.mIsFinished = true;
    }
}

/// `Fill(selection, sort, slot)`: emptied, idle, the selection's formation; the selection's valid living units of the
/// formation slot taken in its order (sorted first for sort 0), each out of its old active group; the speeds; for
/// sort 1 sorted slowest first.
///
/// # Safety
/// `g` is a live active group, `selection` a live selection.
pub unsafe fn fill<H: GroupHost>(h: &mut H, g: G, selection: G, sort: i64, slot: i64) {
    // SAFETY: the caller's contract
    unsafe {
        let count = len(selection);
        clear_goal(h, g);
        data(g).u48.clear();
        data(g).u58.clear();
        clear(h, g);
        head(g).mFields.mAction = ACTION_IDLE;
        head(g).mGoal = std::ptr::null_mut();
        let formation = head(selection).mFormation as u64;
        set_formation(h, g, formation);
        if sort == 0 {
            sort_units(h, selection);
        }
        let from = units(selection);
        let ids: Vec<u32> = from[..count.min(from.len())].iter().map(|&u| h.unit(u).id).collect();
        let player = head(selection).mPlayer as u64;
        let taken = h.fill_members(player, &ids, slot);
        for k in taken {
            let unit = unit_at(selection, k as usize);
            let old = h.unit(unit).active_group;
            if old != 0 {
                remove_unit(h, old as G, unit);
            }
            unit_set_active_group(h, unit, g);
            add(h, g, unit);
        }
        h.compute_values(g as u64);
        if sort == 1 {
            let mut keyed: Vec<(u64, GroupUnit)> = units(g).into_iter().map(|u| (u, h.unit(u))).collect();
            msvc_sort::sort_by(&mut keyed, |a, b| velocity_less(&a.1, &b.1));
            data(g).units = keyed.into_iter().map(|(u, _)| u).collect();
        }
    }
}

/// `RequestPaths(units)`: a path to the goal's tile for each cluster of the units (from its first unit's tile, the
/// last cluster first); whether there was a cluster.
///
/// # Safety
/// `g` is a live active group with a goal.
pub unsafe fn request_paths<H: GroupHost>(h: &mut H, g: G, units: &[u64]) -> bool {
    // SAFETY: the caller's contract
    let goal = unsafe { head(g) }.mGoal as u64;
    let location = h.goal_location(goal);
    let to = (um_floor(location[0]) as i64, um_floor(location[1]) as i64);
    if units.is_empty() {
        return false;
    }
    let clusters = make_clusters(h, units, 10, true);
    for c in clusters.iter().rev() {
        let first = h.unit(c.units[0]);
        // SAFETY: as above
        unsafe { request_path(h, g, (first.tile_x, first.tile_y), to, c) };
    }
    !clusters.is_empty()
}

/// `RequestAllPaths()`.
///
/// # Safety
/// As [`request_paths`].
pub unsafe fn request_all_paths<H: GroupHost>(h: &mut H, g: G) {
    // SAFETY: the caller's contract
    unsafe {
        let units = units(g);
        request_paths(h, g, &units);
    }
}

/// `KeepNonCitizens()` (0), `KeepGroundAttackers()` (1), `KeepCitizens()` (2): the units failing the store's test
/// removed.
///
/// # Safety
/// `g` is a live active group.
pub unsafe fn keep<H: GroupHost>(h: &mut H, g: G, test: u32) {
    let mut i = 0;
    // SAFETY: the caller's contract
    unsafe {
        while i < len(g) {
            let unit = unit_at(g, i);
            let id = h.unit(unit).id;
            if h.keeps(test, id) {
                i += 1;
            } else {
                remove_unit(h, g, unit);
            }
        }
    }
}

/// `ResetUnitsPaths()`: every unit's build targets cleared.
///
/// # Safety
/// `g` is a live group.
pub unsafe fn reset_units_paths<H: GroupHost>(h: &mut H, g: G) {
    // SAFETY: the caller's contract
    for unit in unsafe { units(g) } {
        if unit != 0 {
            h.clear_build_targets(unit);
        }
    }
}

/// `TellGroup(unit, location)`: the unit's reserved destination.
///
/// # Safety
/// `g` is a live active group.
pub unsafe fn tell_group<H: GroupHost>(h: &mut H, g: G, unit: u64, location: [f32; 3]) {
    let id = h.unit(unit).id;
    // SAFETY: the caller's contract
    unsafe { data(g) }.destinations.insert(id, location);
}

/// The reserved destinations in id order (`mDestinations`).
///
/// # Safety
/// `g` is a live active group.
pub unsafe fn destinations(g: G) -> Vec<(u32, [f32; 3])> {
    // SAFETY: the caller's contract
    unsafe { data(g) }.destinations.iter().map(|(&k, &p)| (k, p)).collect()
}

/// A path request's resolution after a load (`EEAwaitAutoPathRequest`'s attach): the rebuilt request takes the
/// loaded copy's place, else the group request is dropped; the group's reference released.
///
/// # Safety
/// `g` is a live group (referenced for the wait); `request` 0 or a request with one reference, the caller's.
pub unsafe fn path_resolved<H: GroupHost>(h: &mut H, g: G, key: i64, request: u64) {
    let mut request = request;
    // SAFETY: the caller's contract
    let old = unsafe { data(g) }.requests.get(&key).map(|r| r.path);
    if let Some(path) = old {
        if request != 0 {
            h.path_dereference(path);
            // SAFETY: as above
            unsafe { data(g) }.requests.get_mut(&key).expect("the request").path = request;
            request = 0;
        } else {
            // SAFETY: as above
            let r = unsafe { data(g) }.requests.remove(&key).expect("the request");
            release_request(h, r);
        }
    }
    if request != 0 {
        h.path_dereference(request);
    }
    // SAFETY: as above
    unsafe { dereference(h, g) };
}

// ---- the active groups in a saved game (save 0.279, EESaveHasActiveGroups)

/// A path request as the saved game holds it.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct SavedRequest {
    pub key: c_long,
    pub unit_ids: Vec<c_long>,
    pub flags_player: c_long,
    pub flags: [bool; 17],
    pub l30: c_long,
    /// the load's request (one reference)
    pub request: u64,
}

/// An active group as the saved game holds it (every member but its goal, which only the command that made the
/// group reads).
#[derive(Clone, Debug, PartialEq)]
pub struct SavedGroup {
    /// the id, whether the unit's `mActiveGroup` is this group
    pub units: Vec<(c_long, bool)>,
    pub formation_kind: c_ulong,
    pub formation: FormationState,
    pub type_counts: Vec<(c_long, c_long)>,
    pub kind_flags: c_long,
    pub action: c_long,
    pub destinations: Vec<(u32, [f32; 3])>,
    pub u48: Vec<[f32; 3]>,
    pub u58: Vec<[f32; 3]>,
    pub values: [f32; 17],
    pub flags: [bool; 4],
    pub requests: Vec<SavedRequest>,
}

impl Default for SavedGroup {
    fn default() -> Self {
        SavedGroup {
            units: Vec::new(),
            formation_kind: KEEP_FORMATION,
            formation: FormationState { b5c: true, ..Default::default() },
            type_counts: Vec::new(),
            kind_flags: 0,
            action: 0,
            destinations: Vec::new(),
            u48: Vec::new(),
            u58: Vec::new(),
            values: [0.0; 17],
            flags: [false; 4],
            requests: Vec::new(),
        }
    }
}

/// The saved game's path requests (`EESaveAutoPathRequest`, `EELoadAutoPathRequest`).
pub trait SavedPaths {
    fn save_path(&mut self, path: u64);
    /// the file's error, or 0 with the new request (one reference)
    fn load_path(&mut self) -> (i32, u64);
}

/// The group's values in their order (`mSpeed` .. `mCapFactor`).
fn values_of(f: &EeUnitsActiveGroupFields) -> [f32; 17] {
    [
        f.mSpeed,
        f.mMaxSpeed,
        f.mMinSpeed,
        f.mAcceleration,
        f.mMaxAcceleration,
        f.mMinAcceleration,
        f.mTurnSpeed,
        f.mMaxTurnSpeed,
        f.mMinTurnSpeed,
        f.mTurnAcceleration,
        f.mMaxTurnAcceleration,
        f.mMinTurnAcceleration,
        f.mInverseSpeed,
        f.mInverseAcceleration,
        f.mInverseTurnSpeed,
        f.mInverseTurnAcceleration,
        f.mCapFactor,
    ]
}

fn write_point<F: PlayerSaveHost + ?Sized>(file: &mut F, p: &[f32; 3]) {
    for &v in p {
        file.write_float(v);
    }
}

fn read_point<F: PlayerSaveHost + ?Sized>(file: &mut F, p: &mut [f32; 3]) -> i32 {
    let mut result = file.read_float(&mut p[0]);
    if result == OK {
        result = file.read_float(&mut p[1]);
    }
    if result == OK {
        result = file.read_float(&mut p[2]);
    }
    result
}

/// One active group into the saved game (`EESaveActiveGroup`; every write's error ignored, as the original).
///
/// # Safety
/// `g` is a live active group.
pub unsafe fn save_active<H: GroupHost, F: PlayerSaveHost + SavedPaths + ?Sized>(h: &mut H, g: G, file: &mut F) {
    // SAFETY: the caller's contract
    unsafe {
        let units = units(g);
        file.write_ulong(units.len() as c_ulong);
        for &u in &units {
            let info = h.unit(u);
            file.write_long(info.id as c_long);
            file.write_bool(info.active_group == g as u64);
        }
        let formation = head(g).mFormation as u64;
        file.write_ulong(if formation != 0 { h.formation_kind(formation) as c_ulong } else { KEEP_FORMATION });
        if formation != 0 {
            let s = h.formation_state(formation);
            for &v in &s.orientation {
                file.write_float(v);
            }
            write_point(file, &s.center);
            file.write_float(s.heading);
            file.write_bool(s.b5c);
        }
        let counts = type_counts(g);
        file.write_ulong(counts.len() as c_ulong);
        for (k, v) in counts {
            file.write_long(k as c_long);
            file.write_long(v as c_long);
        }
        file.write_long(head(g).mKindFlags);
        file.write_long(head(g).mFields.mAction);
        let destinations = destinations(g);
        file.write_ulong(destinations.len() as c_ulong);
        for (id, p) in &destinations {
            file.write_ulong(*id as c_ulong);
            write_point(file, p);
        }
        for list in [data(g).u48.clone(), data(g).u58.clone()] {
            file.write_ulong(list.len() as c_ulong);
            for p in &list {
                write_point(file, p);
            }
        }
        let f = head(g).mFields;
        for v in values_of(&f) {
            file.write_float(v);
        }
        file.write_bool(f.mIsFinished);
        file.write_bool(f.mIsQueued);
        file.write_bool(f.mMovesTogether);
        file.write_bool(f.mIsSpecialAttack);
        let requests: Vec<(i64, PathRequest)> = requests(g).into_iter().filter(|(_, r)| r.path != 0).collect();
        file.write_ulong(requests.len() as c_ulong);
        for (key, r) in &requests {
            file.write_long(*key as c_long);
            file.write_ulong(r.unit_ids.len() as c_ulong);
            for &id in &r.unit_ids {
                file.write_long(id);
            }
            file.write_long(r.flags_player);
            for &b in &r.flags {
                file.write_bool(b != 0);
            }
            file.write_long(r.l30);
            file.save_path(r.path);
        }
    }
}

/// One active group from the saved game (`EELoadActiveGroup`): the first read error.
pub fn load_active<F: PlayerSaveHost + SavedPaths + ?Sized>(file: &mut F, g: &mut SavedGroup) -> i32 {
    let mut count: c_ulong = 0;
    let mut result = file.read_ulong(&mut count);
    let mut i = 0;
    while i < count && result == OK {
        let mut id: c_long = 0;
        let mut member = false;
        result = file.read_long(&mut id);
        if result == OK {
            result = file.read_bool(&mut member);
        }
        g.units.push((id, member));
        i += 1;
    }
    if result == OK {
        result = file.read_ulong(&mut g.formation_kind);
    }
    if result == OK && g.formation_kind != KEEP_FORMATION {
        let mut f = 0;
        while f < 16 && result == OK {
            result = file.read_float(&mut g.formation.orientation[f]);
            f += 1;
        }
        if result == OK {
            result = read_point(file, &mut g.formation.center);
        }
        if result == OK {
            result = file.read_float(&mut g.formation.heading);
        }
        if result == OK {
            result = file.read_bool(&mut g.formation.b5c);
        }
    }
    if result == OK {
        result = file.read_ulong(&mut count);
    }
    let mut i = 0;
    while i < count && result == OK {
        let (mut k, mut v): (c_long, c_long) = (0, 0);
        result = file.read_long(&mut k);
        if result == OK {
            result = file.read_long(&mut v);
        }
        g.type_counts.push((k, v));
        i += 1;
    }
    if result == OK {
        result = file.read_long(&mut g.kind_flags);
    }
    if result == OK {
        result = file.read_long(&mut g.action);
    }
    if result == OK {
        result = file.read_ulong(&mut count);
    }
    let mut i = 0;
    while i < count && result == OK {
        let mut id: c_ulong = 0;
        let mut p = [0.0f32; 3];
        result = file.read_ulong(&mut id);
        if result == OK {
            result = read_point(file, &mut p);
        }
        g.destinations.push((id as u32, p));
        i += 1;
    }
    for list in 0..2 {
        if result == OK {
            result = file.read_ulong(&mut count);
        }
        let mut i = 0;
        while i < count && result == OK {
            let mut p = [0.0f32; 3];
            result = read_point(file, &mut p);
            if list == 0 {
                g.u48.push(p);
            } else {
                g.u58.push(p);
            }
            i += 1;
        }
    }
    let mut v = 0;
    while v < 17 && result == OK {
        result = file.read_float(&mut g.values[v]);
        v += 1;
    }
    let mut f = 0;
    while f < 4 && result == OK {
        result = file.read_bool(&mut g.flags[f]);
        f += 1;
    }
    if result == OK {
        result = file.read_ulong(&mut count);
    }
    let mut i = 0;
    while i < count && result == OK {
        g.requests.push(SavedRequest::default());
        let r = g.requests.last_mut().expect("pushed");
        let mut ids: c_ulong = 0;
        result = file.read_long(&mut r.key);
        if result == OK {
            result = file.read_ulong(&mut ids);
        }
        let mut u = 0;
        while u < ids && result == OK {
            let mut id: c_long = 0;
            result = file.read_long(&mut id);
            r.unit_ids.push(id);
            u += 1;
        }
        if result == OK {
            result = file.read_long(&mut r.flags_player);
        }
        let mut f = 0;
        while f < 17 && result == OK {
            result = file.read_bool(&mut r.flags[f]);
            f += 1;
        }
        if result == OK {
            result = file.read_long(&mut r.l30);
        }
        if result == OK {
            let (e, request) = file.load_path();
            result = e;
            r.request = request;
        }
        i += 1;
    }
    result
}

/// The loaded requests released (a failed load, a player gone).
pub fn release_saved<H: GroupHost>(h: &mut H, groups: &mut [SavedGroup]) {
    for g in groups {
        for r in &mut g.requests {
            if r.request != 0 {
                h.path_dereference(r.request);
                r.request = 0;
            }
        }
    }
}

/// The waits a restore asks (`EEAwaitAutoPathRequest(id, attach)` with the group referenced and the key).
pub trait PathWaits {
    fn await_path(&mut self, request_id: i64, group: G, key: i64);
}

/// `EERestoreActiveGroup`: a new active group of the player with the saved members; a running request waits for its
/// graph search.
pub fn restore_active<H: GroupHost + PathWaits>(h: &mut H, player: u64, s: &mut SavedGroup) -> G {
    let g = new(h, EE_UNITS_GROUP_KIND_ACTIVE, player, 0);
    // SAFETY: the new group
    unsafe {
        for &(id, member) in &s.units {
            let unit = h.world_unit(id as i64);
            if unit == 0 || !h.is_complex(unit) {
                continue;
            }
            base_add(h, g, unit);
            if member {
                unit_set_active_group(h, unit, g);
            }
        }
        let formation = if s.formation_kind != KEEP_FORMATION { h.formation_new(s.formation_kind as i64) } else { 0 };
        if formation != 0 {
            h.set_formation_state(formation, &s.formation);
        }
        let old = head(g).mFormation as u64;
        if old != 0 {
            h.formation_dereference(old);
        }
        head(g).mFormation = formation as usize as *mut c_void;
        data(g).type_counts = s.type_counts.iter().map(|&(k, v)| (k as i64, v as i64)).collect();
        head(g).mKindFlags = s.kind_flags;
        head(g).mFields.mAction = s.action;
        for &(id, p) in &s.destinations {
            data(g).destinations.insert(id, p);
        }
        data(g).u48 = s.u48.clone();
        data(g).u58 = s.u58.clone();
        let f = &mut head(g).mFields;
        f.set_values(&s.values);
        f.mIsFinished = s.flags[0];
        f.mIsQueued = s.flags[1];
        f.mMovesTogether = s.flags[2];
        f.mIsSpecialAttack = s.flags[3];
        for r in &mut s.requests {
            let path = std::mem::take(&mut r.request);
            if path != 0 {
                h.path_reference(path);
            }
            let mut flags = [0u8; 17];
            for (k, &b) in r.flags.iter().enumerate() {
                flags[k] = u8::from(b);
            }
            let request =
                PathRequest { path, unit_ids: r.unit_ids.clone(), flags_player: r.flags_player, flags, l30: r.l30 };
            h.path_dereference(path);
            let key = r.key as i64;
            data(g).requests.insert(key, request);
            if h.path_done(path) {
                continue;
            }
            reference(g);
            let id = h.path_id(path);
            h.await_path(id, g, key);
        }
    }
    g
}

/// `EESaveActiveGroups`: per world player its running, then its pending active groups.
pub fn save_all<H: GroupHost, F: PlayerSaveHost + SavedPaths + ?Sized>(h: &mut H, file: &mut F) {
    let players = h.world_players();
    file.write_ulong(players.len() as c_ulong);
    for &player in &players {
        for list in 0..2 {
            let groups: Vec<u64> = if player != 0 {
                h.player_groups(player, list)
                    .into_iter()
                    // SAFETY: the player's live groups
                    .filter(|&g| g != 0 && unsafe { head(g as G) }.mGroupKind == EE_UNITS_GROUP_KIND_ACTIVE)
                    .collect()
            } else {
                Vec::new()
            };
            file.write_ulong(groups.len() as c_ulong);
            for g in groups {
                // SAFETY: as above
                unsafe { save_active(h, g as G, file) };
            }
        }
    }
}

/// `EELoadActiveGroups`: the saved groups per player and list (running, then pending), or the first read error
/// (the loaded requests released).
pub fn load_all<H: GroupHost, F: PlayerSaveHost + SavedPaths + ?Sized>(
    h: &mut H,
    file: &mut F,
) -> Result<Vec<Vec<SavedGroup>>, i32> {
    let mut lists: Vec<Vec<SavedGroup>> = Vec::new();
    let mut count: c_ulong = 0;
    let mut result = file.read_ulong(&mut count);
    let mut p: c_ulong = 0;
    while p < count.wrapping_mul(2) && result == OK {
        lists.push(Vec::new());
        let mut groups: c_ulong = 0;
        result = file.read_ulong(&mut groups);
        let mut i = 0;
        while i < groups && result == OK {
            let list = lists.last_mut().expect("pushed");
            list.push(SavedGroup::default());
            result = load_active(file, list.last_mut().expect("pushed"));
            i += 1;
        }
        p += 1;
    }
    if result != OK {
        for l in &mut lists {
            release_saved(h, l);
        }
        return Err(result);
    }
    Ok(lists)
}

/// The loaded groups back at the loaded game's first tick (`EELoadActiveGroups`' `EEAfterLoad`).
pub fn restore_all<H: GroupHost + PathWaits>(h: &mut H, lists: &mut [Vec<SavedGroup>]) {
    let players = h.world_players();
    for (l, saved) in lists.iter_mut().enumerate() {
        let player = players.get(l / 2).copied().unwrap_or(0);
        if player == 0 {
            release_saved(h, saved);
            continue;
        }
        for s in saved.iter_mut() {
            let g = restore_active(h, player, s);
            h.player_groups_push(player, (l % 2) as u32, g as u64);
        }
    }
}

// ---- the player's groups (EEPlayer's destructor, Reset, Clear, RemovalsDone and tick)

/// The player's running groups emptied and released, the list cleared (`~EEPlayer`, `Reset`, `Clear`).
pub fn release_running<H: GroupHost>(h: &mut H, player: u64) {
    for g in h.player_groups(player, 0) {
        // SAFETY: the player's live groups
        unsafe {
            clear(h, g as G);
            dereference(h, g as G);
        }
    }
    h.player_groups_clear(player, 0);
}

/// `~EEPlayer`'s other groups: the finished ones, the two selections and the 20 hot-key groups emptied and released.
pub fn release_others<H: GroupHost>(h: &mut H, player: u64) {
    let mut groups = h.player_groups(player, 2);
    groups.push(h.player_selection(player, 0));
    groups.push(h.player_selection(player, 1));
    groups.extend((0..20).map(|i| h.player_group(player, i)));
    for g in groups {
        // SAFETY: the player's live groups
        unsafe {
            clear(h, g as G);
            dereference(h, g as G);
        }
    }
}

/// `Reset`'s and `Clear`'s: the active selection and the 20 hot-key groups emptied.
pub fn clear_selections<H: GroupHost>(h: &mut H, player: u64) {
    let active = h.player_selection(player, 0);
    // SAFETY: the player's live groups
    unsafe { clear(h, active as G) };
    for i in 0..20 {
        let g = h.player_group(player, i);
        // SAFETY: as above
        unsafe { clear(h, g as G) };
    }
}

/// `EEPlayer::RemovalsDone`: running groups left without units (not finished yet) finish; every finished group is
/// emptied, released, and released again out of the running groups it is in; the finished list cleared.
pub fn removals_done<H: GroupHost>(h: &mut H, player: u64) {
    for g in h.player_groups(player, 0) {
        // SAFETY: the player's live groups
        if unsafe { len(g as G) } == 0 && !h.player_groups(player, 2).contains(&g) {
            // SAFETY: as above (the running groups are active groups)
            unsafe { finish(h, g as G) };
        }
    }
    for g in h.player_groups(player, 2) {
        // SAFETY: as above
        unsafe {
            clear(h, g as G);
            dereference(h, g as G);
        }
        if h.player_groups(player, 0).contains(&g) {
            // SAFETY: as above (the running list's reference)
            unsafe { dereference(h, g as G) };
            h.player_groups_erase(player, 0, g);
        }
    }
    h.player_groups_clear(player, 2);
}

/// The player tick's validation of its groups: the active selection, the selection, the 20 hot-key groups, the
/// running groups (those there when it starts).
pub fn validate_player<H: GroupHost>(h: &mut H, player: u64) {
    let mut groups = vec![h.player_selection(player, 0), h.player_selection(player, 1)];
    groups.extend((0..20).map(|i| h.player_group(player, i)));
    groups.extend(h.player_groups(player, 0));
    for g in groups {
        // SAFETY: the player's live groups
        unsafe { validate(h, g as G) };
    }
}

// ---- the player's group lists (the groups lane, round 2: were EEPlayer's mRunningGroups, mPendingGroups and
// mFinishedGroups, `std::vector<EEGroup*>`)

/// The running list (the groups the tick moves)
pub const EE_UNITS_PLAYER_GROUPS_RUNNING: u32 = 0;
/// The pending list (the commands' new groups, started by `AddPendingUnits`)
pub const EE_UNITS_PLAYER_GROUPS_PENDING: u32 = 1;
/// The finished list (`EEActiveGroup::Finish` pushes itself; `RemovalsDone` releases them)
pub const EE_UNITS_PLAYER_GROUPS_FINISHED: u32 = 2;

/// A player's three group lists (EEPlayer's `mGroupLists`): the groups by address, in the vectors' order; the lists
/// hold no reference of their own (the bodies count them, as the original's did).
#[derive(Debug, Default)]
pub struct EeUnitsPlayerGroups {
    lists: [Vec<u64>; 3],
}

impl EeUnitsPlayerGroups {
    /// List `list`'s groups (an unknown list: none).
    pub fn get(&self, list: u32) -> &[u64] {
        self.lists.get(list as usize).map_or(&[], |l| l.as_slice())
    }
    /// `push_back`
    pub fn push(&mut self, list: u32, group: u64) {
        if let Some(l) = self.lists.get_mut(list as usize) {
            l.push(group);
        }
    }
    /// `clear()`
    pub fn clear(&mut self, list: u32) {
        if let Some(l) = self.lists.get_mut(list as usize) {
            l.clear();
        }
    }
    /// `erase(std::find(...))`: the first entry `group`, if any
    pub fn erase_first(&mut self, list: u32, group: u64) {
        if let Some(l) = self.lists.get_mut(list as usize) {
            if let Some(i) = l.iter().position(|&g| g == group) {
                l.remove(i);
            }
        }
    }
    /// `AddPendingUnits`' first part: every pending group appended to the running ones (the pending list kept: the
    /// original cleared it only in `ReleasePendingUnits`)
    pub fn start_pending(&mut self) {
        let pending = self.lists[1].clone();
        self.lists[0].extend(pending);
    }
}

/// A player's new lists (its constructors).
#[no_mangle]
pub extern "C" fn ee_units_player_groups_new() -> *mut EeUnitsPlayerGroups {
    Box::into_raw(Box::default())
}

/// Frees a player's lists (its destructor; the groups were released before).
///
/// # Safety
/// `p` came from [`ee_units_player_groups_new`] (or is null) and is not used afterwards.
#[no_mangle]
pub unsafe extern "C" fn ee_units_player_groups_free(p: *mut EeUnitsPlayerGroups) {
    if !p.is_null() {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(p) });
    }
}

/// List `list`'s length.
///
/// # Safety
/// `p` a player's live lists.
#[no_mangle]
pub unsafe extern "C" fn ee_units_player_groups_len(p: *const EeUnitsPlayerGroups, list: u32) -> c_ulong {
    // SAFETY: the caller's contract
    unsafe { (*p).get(list).len() as c_ulong }
}

/// List `list`'s group `i` (null past the end).
///
/// # Safety
/// As [`ee_units_player_groups_len`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_player_groups_at(
    p: *const EeUnitsPlayerGroups,
    list: u32,
    i: c_ulong,
) -> *mut EeUnitsGroup {
    // SAFETY: the caller's contract
    unsafe { (*p).get(list).get(i as usize).map_or(std::ptr::null_mut(), |&g| g as usize as *mut EeUnitsGroup) }
}

/// `push_back(group)` onto list `list`.
///
/// # Safety
/// As [`ee_units_player_groups_len`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_player_groups_push(p: *mut EeUnitsPlayerGroups, list: u32, group: *mut EeUnitsGroup) {
    // SAFETY: the caller's contract
    unsafe { (*p).push(list, group as usize as u64) }
}

/// `op` 0: [`EeUnitsPlayerGroups::start_pending`] (`AddPendingUnits`), 1: the pending list cleared
/// (`ReleasePendingUnits`).
///
/// # Safety
/// As [`ee_units_player_groups_len`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_player_groups_do(p: *mut EeUnitsPlayerGroups, op: u32) {
    // SAFETY: the caller's contract
    let l = unsafe { &mut *p };
    match op {
        0 => l.start_pending(),
        1 => l.clear(EE_UNITS_PLAYER_GROUPS_PENDING),
        _ => {}
    }
}

/// The test groups the other crates' tests read (`GroupView`): a plain group of `units`, owning no references.
pub fn detached(units: &[u64]) -> G {
    let alloc = Box::new(Alloc {
        head: EeUnitsGroup {
            mPlayer: std::ptr::null_mut(),
            mFormation: std::ptr::null_mut(),
            mKindFlags: 0,
            mSavedGroup: std::ptr::null_mut(),
            mChangeTime: 0,
            mNeedsSort: false,
            mGroupKind: EE_UNITS_GROUP_KIND_SAVED,
            mIndex: 0,
            mGoal: std::ptr::null_mut(),
            mFields: EeUnitsActiveGroupFields::default(),
        },
        data: GroupData { references: 1, units: units.to_vec(), ..Default::default() },
    });
    Box::into_raw(alloc) as G
}

/// Frees a [`detached`] group.
///
/// # Safety
/// `g` came from [`detached`] and is not used afterwards.
pub unsafe fn free_detached(g: G) {
    // SAFETY: the caller's contract
    drop(unsafe { Box::from_raw(g as *mut Alloc) });
}
