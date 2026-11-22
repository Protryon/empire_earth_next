//! The unit utilities' world host's compound questions and writes over their C++ statements (the units' lane, round
//! 8; were EEFOFindNewTargetNearOldTarget.cpp `ee_units_uu_host_world`, `_world_f` and `_spot`'s multi-statement
//! cases and `UUMakeWallPiece`): the build target, the idle restart, a tile's unit, the ambient clearing, the step
//! aside, the editor's transport, `EEPlanWall`'s new piece, the wall's rise, the drop point, the path area and the
//! gate's ends. Each C++ statement is one case of those switches (`world` from 20, `world_f` from 5, `spot` from 4),
//! asked in the original's order; the TEST ONLY UUTL notes and the Rust the cases called (the wall's shape) are
//! called here.

// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::os::raw::{c_long, c_ulong};

// ---- `world`'s statements (`a`, `b`, `c` objects or values, `x`, `y` values, `f` a float)
/// `UUId(unit a)` (its id, -2 for none)
pub(crate) const UW_UNIT_ID: i32 = 20;
/// complex unit `a`'s `AddBuildTarget(unit b)`
pub(crate) const UW_ADD_BUILD_TARGET: i32 = 21;
/// complex unit `a`'s `ClearGoals(0)`
pub(crate) const UW_CLEAR_GOALS: i32 = 22;
/// complex unit `a`'s `StartAction(kActionIdle)`
pub(crate) const UW_START_IDLE: i32 = 23;
/// complex unit `a`'s `mUnitClass->ActionChanged(a)`
pub(crate) const UW_ACTION_CHANGED: i32 = 24;
/// tile `a`'s `mUnits = AddUnit(unit b, mUnits)`
pub(crate) const UW_TILE_ADD: i32 = 25;
/// tile `a`'s `RemoveUnit(unit b)`
pub(crate) const UW_TILE_REMOVE: i32 = 26;
/// `EEUnitClassOfKind<EE_CLASS_KIND_AMBIENT>(unit a's mUnitClass) != NULL`
pub(crate) const UW_IS_AMBIENT: i32 = 27;
/// unit `a`'s `mUnitClass->mUnitType`
pub(crate) const UW_CLASS_TYPE: i32 = 28;
/// unit `a`'s `mUnitClass->mIsDeletable`
pub(crate) const UW_DELETABLE: i32 = 29;
/// unit `a`'s `mResourceAmount`
pub(crate) const UW_RESOURCE_AMOUNT: i32 = 30;
/// unit `a`'s `mResourceAmount = 0`
pub(crate) const UW_CLEAR_RESOURCE_AMOUNT: i32 = 31;
/// unit `a`'s `Kill(false, false, false)`
pub(crate) const UW_KILL: i32 = 32;
/// unit `a`'s `mPlayer->KillUnit(a, false)`
pub(crate) const UW_PLAYER_KILL: i32 = 33;
/// complex unit `a`'s `StepAside(player b, class c, x, y, f != 0)`
pub(crate) const UW_STEP_ASIDE: i32 = 34;
/// `gUIManager->GetScenarioEditorForm()->GetUnitUnderMouse()`
pub(crate) const UW_UNIT_UNDER_MOUSE: i32 = 35;
/// `::gWorld.GetUnit(x)` (as an `EEUnit*`)
pub(crate) const UW_WORLD_UNIT: i32 = 36;
/// unit `a`'s `GetComplexUnit()`
pub(crate) const UW_COMPLEX: i32 = 37;
/// complex unit `a`'s `GetPlayer()`
pub(crate) const UW_PLAYER: i32 = 38;
/// unit `a`'s `mUnitClass->mTransportCapacity`
pub(crate) const UW_TRANSPORT_CAPACITY: i32 = 39;
/// complex unit `a`'s `mCargoRoom`
pub(crate) const UW_CARGO_ROOM: i32 = 40;
/// `gUnitClassTable[x]->mWeight`
pub(crate) const UW_TYPE_WEIGHT: i32 = 41;
/// `gUnitClassTable[x]->CanEnter(complex unit a's mUnitClass)`
pub(crate) const UW_TYPE_CAN_ENTER: i32 = 42;
/// class `a`'s `CreateUnit(x, y, player b's GetPlayerID(), f != 0, NULL)`
pub(crate) const UW_CREATE_UNIT: i32 = 43;
/// `::gWorld.GetMap()->GetNumRaisedCorners(x, y)`
pub(crate) const UW_RAISED_CORNERS: i32 = 44;
/// unit `a`'s `SetHitPoints(x)`
pub(crate) const UW_SET_HIT_POINTS: i32 = 45;
/// unit `a`'s `mTile->GetVisibility(player b's GetPlayerID())`
pub(crate) const UW_TILE_VISIBILITY: i32 = 46;
/// `gDoppelgangerManager.AddForUnseenUnit(complex unit a)`
pub(crate) const UW_ADD_UNSEEN: i32 = 47;
/// player `b`'s `Pay(its GetTechTree()->Get(class a's GetTechnology()), 0x20, 1.0f)`
pub(crate) const UW_PAY: i32 = 48;
/// complex unit `a`'s `mUnitClass->mIsWall && !mIsTower`
pub(crate) const UW_IS_WALL_NOT_TOWER: i32 = 49;
/// complex unit `a`'s `GetPlayer()->AddConstructionSite(a)`
pub(crate) const UW_ADD_CONSTRUCTION_SITE: i32 = 50;
/// unit `a`'s `GetStats()->mMaxHitPoints`
pub(crate) const UW_MAX_HIT_POINTS: i32 = 51;
/// complex unit `a`'s `SetBuilt(1, 0, 1)`
pub(crate) const UW_SET_BUILT: i32 = 52;
/// player `b`'s `GetTechTree()->Get(kTechWallSpace)->GetIsAvailable()`
pub(crate) const UW_SPACE_WALL_AVAILABLE: i32 = 53;
/// `EECreateSpaceWallEnergyWall(complex unit a)`
pub(crate) const UW_SPACE_ENERGY_WALL: i32 = 54;
/// complex unit `a`'s `mIsPaidFor`
pub(crate) const UW_PAID: i32 = 55;
/// complex unit `a`'s `mIsPaidFor = true`
pub(crate) const UW_SET_PAID: i32 = 56;
/// unit `a`'s `mUnitClass->mCategory`
pub(crate) const UW_CATEGORY: i32 = 57;
/// unit `a`'s `mComplexUnit->mIsBuilt`
pub(crate) const UW_COMPLEX_BUILT: i32 = 58;
/// `gContinentManager.GetPathAreaAt((x, y))` (its CNTQ record: asked once)
pub(crate) const UW_PATH_AREA: i32 = 59;

// ---- `world_f`'s
/// the z of unit `a`'s complex unit's location applied to its model's box top at its frame (`Apply`)
pub(crate) const UWF_BOX_TOP_Z: i32 = 5;
/// unit `a`'s `mStats->GetScale()`
pub(crate) const UWF_STATS_SCALE: i32 = 6;
/// `(float)` unit `a`'s `mComplexUnit->mHitPoints`
pub(crate) const UWF_HIT_POINTS: i32 = 7;
/// `(float)` unit `a`'s `mComplexUnit->mStats->mMaxHitPoints`
pub(crate) const UWF_MAX_HIT_POINTS: i32 = 8;

// ---- `spot`'s (the floats at `out`)
/// unit's `GetHitPoints()`
pub(crate) const US_HIT_POINTS: i32 = 4;
/// unit's `mComplexUnit != NULL`
pub(crate) const US_HAS_COMPLEX: i32 = 5;
/// unit's `GetModel()->mModel != NULL`
pub(crate) const US_HAS_MODEL: i32 = 6;
/// `out[0..3]` = unit's `GetModel()->mTopPoint`
pub(crate) const US_TOP_POINT: i32 = 7;
/// `out[0]` = unit's `mComplexUnit->mPhysicalModel.GetScale()`
pub(crate) const US_SCALE: i32 = 8;
/// `out[3..6]` = unit's `mComplexUnit->mPhysicalModel.GetLocation()` applied to `out[0..3]`
pub(crate) const US_APPLY: i32 = 9;
/// `out[0..3]` = unit's `GetPosition()`
pub(crate) const US_POSITION: i32 = 10;
/// path area `unit`'s `GetID()`
pub(crate) const US_PATH_AREA_ID: i32 = 12;
/// `out[0..3]` = the gate class's `GetLastLinkedCarrier(gate's GetOutermostCarrier())->GetPosition()`
pub(crate) const US_GATE_CARRIER_POSITION: i32 = 13;
/// `out[0..3]` = the gate class's `GetLastLinked(gate's GetOutermostCarrier())->GetPosition()`
pub(crate) const US_GATE_LINKED_POSITION: i32 = 14;

/// the UUTL notes' third values (the C++'s UUEvent)
const EVENT_BUILD_TARGET: i32 = 1;
const EVENT_RESTART: i32 = 2;
/// `kUnitTypePathPoint`
const TYPE_PATH_POINT: i64 = 0xdf;
/// `kUnitCategoryGate`
const CATEGORY_GATE: i64 = 0x19;
/// a gate's rise
const GATE_RISE_Z: f32 = 1.2;
/// `kUnitTypeWallSpace`
const TYPE_WALL_SPACE: i64 = 0x2fe;

/// The world host's statements and the Rust they call.
pub trait UuOps {
    #[allow(clippy::too_many_arguments)]
    fn world(&mut self, which: i32, a: u64, b: u64, c: u64, x: i32, y: i32, f: f32) -> u64;
    fn world_f(&mut self, which: i32, a: u64, b: u64, f: f32) -> f32;
    fn spot(&mut self, which: i32, unit: u64, x: i32, y: i32, out: &mut [f32; 6]) -> i32;
    /// `UUEvent(a, b, c)` (TEST ONLY: UUTL)
    fn note_event(&mut self, a: i32, b: i32, c: i32);
    /// `ee_units_uu_shape_wall(complex, corner, raised)`
    fn shape_wall(&mut self, complex: u64, corner: i32, raised: i32);
}

fn w<H: UuOps + ?Sized>(h: &mut H, which: i32, a: u64) -> u64 {
    h.world(which, a, 0, 0, 0, 0, 0.0)
}

fn wab<H: UuOps + ?Sized>(h: &mut H, which: i32, a: u64, b: u64) -> u64 {
    h.world(which, a, b, 0, 0, 0, 0.0)
}

/// `AddBuildTarget` with its UUTL note
pub fn add_build_target<H: UuOps + ?Sized>(h: &mut H, unit: u64, piece: u64) {
    let (a, b) = (w(h, UW_UNIT_ID, unit) as i32, w(h, UW_UNIT_ID, piece) as i32);
    h.note_event(a, b, EVENT_BUILD_TARGET);
    wab(h, UW_ADD_BUILD_TARGET, unit, piece);
}

/// the goals cleared and the idle action restarted, with its UUTL note
pub fn restart_idle<H: UuOps + ?Sized>(h: &mut H, unit: u64) {
    let a = w(h, UW_UNIT_ID, unit) as i32;
    h.note_event(a, 0, EVENT_RESTART);
    w(h, UW_CLEAR_GOALS, unit);
    w(h, UW_START_IDLE, unit);
    w(h, UW_ACTION_CHANGED, unit);
}

/// a tile's unit added or removed
pub fn tile_unit<H: UuOps + ?Sized>(h: &mut H, tile: u64, unit: u64, add: bool) {
    wab(h, if add { UW_TILE_ADD } else { UW_TILE_REMOVE }, tile, unit);
}

/// `EEClearUnderBuilding`'s unit: a deletable ambient object (not a path point) emptied and killed
pub fn clear_ambient<H: UuOps + ?Sized>(h: &mut H, unit: u64) {
    if w(h, UW_IS_AMBIENT, unit) == 0
        || w(h, UW_CLASS_TYPE, unit) as c_long as i64 == TYPE_PATH_POINT
        || w(h, UW_DELETABLE, unit) == 0
    {
        return;
    }
    if w(h, UW_RESOURCE_AMOUNT, unit) as c_long != 0 {
        w(h, UW_CLEAR_RESOURCE_AMOUNT, unit);
    }
    w(h, UW_KILL, unit);
    w(h, UW_PLAYER_KILL, unit);
}

/// `StepAside` with its UUTL note
pub fn step_aside<H: UuOps + ?Sized>(h: &mut H, complex: u64, player: u64, class: u64, x: i32, y: i32, force: bool) {
    let a = w(h, UW_UNIT_ID, complex) as i32;
    h.note_event(a, x, y);
    h.world(UW_STEP_ASIDE, complex, player, class, x, y, if force { 1.0 } else { 0.0 });
}

/// `EEEditorCanPlace`'s transport under the mouse takes a unit of the type
pub fn editor_transport_takes<H: UuOps + ?Sized>(h: &mut H, player: u64, unit_type: i32) -> bool {
    let id = w(h, UW_UNIT_UNDER_MOUSE, 0) as c_long;
    let carrier = if id != -1 { h.world(UW_WORLD_UNIT, 0, 0, 0, id as i32, 0, 0.0) } else { 0 };
    let transport = if carrier != 0 { w(h, UW_COMPLEX, carrier) } else { 0 };
    transport != 0
        && w(h, UW_PLAYER, transport) == player
        && w(h, UW_TRANSPORT_CAPACITY, carrier) as c_long > 0
        && w(h, UW_CARGO_ROOM, transport) as c_long >= h.world(UW_TYPE_WEIGHT, 0, 0, 0, unit_type, 0, 0.0) as c_long
        && h.world(UW_TYPE_CAN_ENTER, transport, 0, 0, unit_type, 0, 0.0) != 0
}

/// `EEPlanWall`'s new piece (its CreateUnit, shape, health, payment and paid flag, in the C++'s order)
pub fn make_wall_piece<H: UuOps + ?Sized>(
    h: &mut H,
    class: u64,
    player: u64,
    x: i32,
    y: i32,
    corner: i32,
    foundation: bool,
) -> u64 {
    let piece = h.world(UW_CREATE_UNIT, class, player, 0, x, y, if foundation { 1.0 } else { 0.0 });
    let complex = w(h, UW_COMPLEX, piece);
    let raised = h.world(UW_RAISED_CORNERS, 0, 0, 0, x, y, 0.0) as c_long as i32;
    h.shape_wall(complex, corner, raised);
    if foundation {
        h.world(UW_SET_HIT_POINTS, piece, 0, 0, 1, 0, 0.0);
        // (the original calls RTSTile::GetVisibility out of line through a helper level, 0x2035cf)
        let visibility = wab(h, UW_TILE_VISIBILITY, piece, player) as c_ulong;
        if visibility > 0 && wab(h, UW_TILE_VISIBILITY, piece, player) as c_ulong <= 1 {
            w(h, UW_ADD_UNSEEN, piece);
        }
        wab(h, UW_PAY, class, player);
        if w(h, UW_IS_WALL_NOT_TOWER, complex) != 0 {
            w(h, UW_ADD_CONSTRUCTION_SITE, complex);
        }
    } else {
        let maximum = w(h, UW_MAX_HIT_POINTS, piece) as c_long as i32;
        h.world(UW_SET_HIT_POINTS, piece, 0, 0, maximum, 0, 0.0);
        w(h, UW_SET_BUILT, complex);
        if w(h, UW_CLASS_TYPE, complex) as c_long as i64 == TYPE_WALL_SPACE
            && wab(h, UW_SPACE_WALL_AVAILABLE, 0, player) != 0
        {
            w(h, UW_SPACE_ENERGY_WALL, complex);
        }
    }
    if w(h, UW_PAID, complex) == 0 {
        w(h, UW_SET_PAID, complex);
    }
    piece
}

/// `EEIsTileTopAbove`'s wall rise: its box top's z (a gate's 1.2) by its scale, in part while it is built
// (the C++'s operand order)
#[allow(clippy::assign_op_pattern)]
pub fn wall_rise<H: UuOps + ?Sized>(h: &mut H, wall: u64) -> f32 {
    let z = if w(h, UW_CATEGORY, wall) as c_long as i64 != CATEGORY_GATE {
        h.world_f(UWF_BOX_TOP_Z, wall, 0, 0.0)
    } else {
        GATE_RISE_Z
    };
    let mut rise = z * h.world_f(UWF_STATS_SCALE, wall, 0, 0.0);
    if w(h, UW_COMPLEX_BUILT, wall) == 0 {
        let hit_points = h.world_f(UWF_HIT_POINTS, wall, 0, 0.0);
        let maximum = h.world_f(UWF_MAX_HIT_POINTS, wall, 0, 0.0);
        rise = (hit_points / maximum) * rise;
    }
    rise
}

fn spot<H: UuOps + ?Sized>(h: &mut H, which: i32, unit: u64, out: &mut [f32; 6]) -> i32 {
    h.spot(which, unit, 0, 0, out)
}

/// A unit's drop point: a dead unit's model top (scaled in x and y) through its location, else its position
pub fn drop_point<H: UuOps + ?Sized>(h: &mut H, target: u64) -> [f32; 3] {
    let mut io = [0.0f32; 6];
    if spot(h, US_HIT_POINTS, target, &mut io) as c_long <= 0
        && spot(h, US_HAS_COMPLEX, target, &mut io) != 0
        && spot(h, US_HAS_MODEL, target, &mut io) != 0
    {
        let mut top = [0.0f32; 6];
        spot(h, US_TOP_POINT, target, &mut top);
        let mut scale = [0.0f32; 6];
        spot(h, US_SCALE, target, &mut scale);
        top[0] *= scale[0];
        spot(h, US_SCALE, target, &mut scale);
        top[1] *= scale[0];
        spot(h, US_APPLY, target, &mut top);
        return [top[3], top[4], top[5]];
    }
    spot(h, US_POSITION, target, &mut io);
    [io[0], io[1], io[2]]
}

/// The path area's id at a tile (0 for none)
pub fn path_area_id<H: UuOps + ?Sized>(h: &mut H, x: i32, y: i32) -> u16 {
    let area = h.world(UW_PATH_AREA, 0, 0, 0, x, y, 0.0);
    if area == 0 {
        return 0;
    }
    h.spot(US_PATH_AREA_ID, area, 0, 0, &mut [0.0; 6]) as u16
}

/// A gate's ends: its last linked carrier's and its last linked part's positions
pub fn gate_ends<H: UuOps + ?Sized>(h: &mut H, gate: u64) -> [[f32; 3]; 2] {
    let mut first = [0.0f32; 6];
    spot(h, US_GATE_CARRIER_POSITION, gate, &mut first);
    let mut second = [0.0f32; 6];
    spot(h, US_GATE_LINKED_POSITION, gate, &mut second);
    [[first[0], first[1], first[2]], [second[0], second[1], second[2]]]
}

#[cfg(test)]
#[path = "uu_world_ops_tests.rs"]
mod tests;
