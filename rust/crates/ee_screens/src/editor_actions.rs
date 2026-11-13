//! The scenario editor's actions (`Scenario Editor Actions/EESEA*.cpp`): what each one does to the world, over a
//! narrow world-edit host ([`SeaHost`]: a tile's existence, a mesh point's height, a terrain rectangle painted, an
//! area raised, the units on a tile, a unit removed, the cliffs and the minimap). The C++ action classes stay the
//! objects the editor's tabs create and post (`EEAction`s in the UI thread's queue, `actions.rs`); their queued areas
//! are a Rust record ([`SePaint`]) behind each, created and freed with it. The actions run under the UI copy lock
//! and the world's write lock (the queue's), on the UI thread.
//!
//! The paint actions (group 1):
//! - the terrain paint (`EESEAPaintTerrain`): each queued area painted with the terrain and its texture (unless the
//!   terrain only scatters ambient units), its ambient units scattered at random by the terrain's chance (the random
//!   seeded from the clock, as the original's), the cliffs and the minimap updated;
//! - the elevation paint (`EESEAPaintElevation`): each area raised as the elevation tab's radio says (a plateau at
//!   the level, rough hills, or flat ground), its flat cliffs removed, the cliffs and the minimap updated; the
//!   effects told about the last area only;
//! - the bulldozer (`EESEAPaintMoutains`): every unit on each area's tiles removed (multi-part units whole, path
//!   points first);
//! - the trigger area's rectangle (`EESEAPaintArea::SetCorner`): the rectangle between the drag's two corners.
//!
//! Every host call is in the original's order; the floats are the original's operations (`f32`, in order).

use std::collections::VecDeque;

use ee_random::URandom;

// ---- the host's ops (`ee_screenscpp_sea`: `i` up to 6 integers, `f` up to 5 floats, in and out)

/// The camera's follow flags cleared (`gB529066`, `gCameraFollowing`, `gB529068`), as every paint does first.
pub const EE_SCREENS_SEA_STOP_FOLLOW: u32 = 1;
/// -> whether the tile (i0, i1) is on the map (`EEMap::GetTile` not NULL).
pub const EE_SCREENS_SEA_TILE: u32 = 2;
/// -> whether the mesh point (i0, i1) is valid (unsigned coordinates, as `GetIsValidPoint`); f0 its height.
pub const EE_SCREENS_SEA_POINT: u32 = 3;
/// `EEMap::PaintTerrainRect(left i0, right i1, top i2, bottom i3, terrain i4, texture i5)`.
pub const EE_SCREENS_SEA_PAINT_RECT: u32 = 4;
/// `EEMap::UpdateCliffs(left i0, right i1, bottom i2, top i3)`.
pub const EE_SCREENS_SEA_UPDATE_CLIFFS: u32 = 5;
/// `gMiniMap->RedrawRect(i0, i1, i2, i3)`.
pub const EE_SCREENS_SEA_MINIMAP_RECT: u32 = 6;
/// `gMiniMap->Refresh()`.
pub const EE_SCREENS_SEA_MINIMAP_REFRESH: u32 = 7;
/// The terrain i0's table entry: -> its ambient unit chance; i1 = ambients only, i2 = its unit class count.
pub const EE_SCREENS_SEA_TERRAIN: u32 = 8;
/// -> the terrain i0's ambient unit class i1 (`EETerrainTable::GetUnitClass`).
pub const EE_SCREENS_SEA_TERRAIN_CLASS: u32 = 9;
/// `TMGetTime()` (the ambient units' seed).
pub const EE_SCREENS_SEA_TIME: u32 = 10;
/// An ambient unit of class i0 on the tile (i1, i2) for the world player, when `EECanPlaceOnTile` allows it.
pub const EE_SCREENS_SEA_PLANT: u32 = 11;
/// -> the elevation tab's checked radio: 0 raise (a plateau), 1 hills, 2 level ground.
pub const EE_SCREENS_SEA_ELEVATION_MODE: u32 = 12;
/// `EEMap::Raise(&i0, &i1, &i2, &i3, f0, f1, f2, f3, i4 != 0)` (left, right, top, bottom; the area comes back as
/// Raise left it).
pub const EE_SCREENS_SEA_RAISE: u32 = 13;
/// `EEMap::RemoveFlatCliffs(left i0, right i1, bottom i2, top i3)`.
pub const EE_SCREENS_SEA_REMOVE_FLAT_CLIFFS: u32 = 14;
/// The elevation tab's paint control stores its mouse tile (`EEUIFSEPaint::StoreMouseTile`).
pub const EE_SCREENS_SEA_STORE_MOUSE_TILE: u32 = 15;
/// `gEffectManager->ElevationChanged(left i0, top i1, right i2, bottom i3)`.
pub const EE_SCREENS_SEA_ELEVATION_CHANGED: u32 = 16;
/// -> the height of one elevation level (`kSEPELevelHeight`) in f0.
pub const EE_SCREENS_SEA_LEVEL_HEIGHT: u32 = 17;

/// The hills' height per level and the height from which they are less rough (0x44598c, 0x445988).
const SEA_HILL_HEIGHT: f32 = 0.816_496_55;
const SEA_HILL_LIMIT: f32 = 1.632_993_1;
/// The raise's slopes and plateau (the original's constants).
const SEA_SLOPE_GENTLE: f32 = 0.408_248_29;
const SEA_SLOPE_STEEP: f32 = 1.632_993_1;

/// The world-edit host.
pub trait SeaHost {
    /// One op of the `EE_SCREENS_SEA_*` list.
    fn op(&mut self, op: u32, i: &mut [i32; 6], f: &mut [f32; 5]) -> i32;
    /// The units on the tile (x, y), each the first part of its unit and referenced, with whether it is a path
    /// point: in the tile's list order.
    fn tile_units(&mut self, x: i32, y: i32) -> Vec<(usize, bool)>;
    /// A unit removed as the bulldozer removes it (a resource emptied first, killed, taken from its player) and its
    /// reference released.
    fn remove_unit(&mut self, unit: usize);
}

fn call<H: SeaHost + ?Sized>(h: &mut H, op: u32, ints: &[i32]) -> i32 {
    let mut i = [0; 6];
    i[..ints.len()].copy_from_slice(ints);
    h.op(op, &mut i, &mut [0.0; 5])
}

/// An area in tiles (top >= bottom: the loops count y down).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct SeaRect {
    pub left: i32,
    pub top: i32,
    pub right: i32,
    pub bottom: i32,
}

/// Which paint an action is.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SePaintKind {
    /// `EESEAPaintTerrain(terrain, texture)`.
    Terrain { terrain: i32, texture: i32 },
    /// `EESEAPaintElevation(level)`.
    Elevation { level: i8 },
    /// `EESEAPaintMoutains`.
    Bulldozer,
}

/// The paint kinds of `ee_screens_sea_paint_new` (`kSEAPaint*` in `ee/EESEAPaint.h`).
pub const EE_SCREENS_SEA_PAINT_TERRAIN: u32 = 0;
pub const EE_SCREENS_SEA_PAINT_ELEVATION: u32 = 1;
pub const EE_SCREENS_SEA_PAINT_BULLDOZER: u32 = 2;

impl SePaintKind {
    /// The kind of a code (a and b: the terrain and its texture, or the level); an unknown code is the bulldozer.
    pub fn from_code(code: u32, a: i32, b: i32) -> Self {
        match code {
            EE_SCREENS_SEA_PAINT_TERRAIN => SePaintKind::Terrain { terrain: a, texture: b },
            EE_SCREENS_SEA_PAINT_ELEVATION => SePaintKind::Elevation { level: a as i8 },
            _ => SePaintKind::Bulldozer,
        }
    }
}

/// A paint action's record: its kind and queued areas (and the elevation's painted ones, kept as the original kept
/// them).
#[derive(Clone, Debug, PartialEq)]
pub struct SePaint {
    pub kind: SePaintKind,
    pub pending: VecDeque<SeaRect>,
    pub painted: Vec<SeaRect>,
}

impl SePaint {
    pub fn new(kind: SePaintKind) -> Self {
        SePaint { kind, pending: VecDeque::new(), painted: Vec::new() }
    }

    /// `SetArea`: queues the area when it changes anything; whether it queued.
    pub fn set_area<H: SeaHost + ?Sized>(&mut self, h: &mut H, left: i32, top: i32, right: i32, bottom: i32) -> bool {
        let rect = SeaRect { left, top, right, bottom };
        let changed = match self.kind {
            // any of its tiles on the map
            SePaintKind::Terrain { .. } => {
                let mut any = false;
                for y in (bottom..=top).rev() {
                    for x in left..=right {
                        if call(h, EE_SCREENS_SEA_TILE, &[x, y]) != 0 {
                            any = true;
                        }
                    }
                }
                any
            }
            // a point not yet at the level's height
            SePaintKind::Elevation { level } => {
                let height = elevation_height(h, level);
                let mut any = false;
                for y in (bottom..=top.wrapping_add(1)).rev() {
                    for x in left..=right.wrapping_add(1) {
                        let mut i = [x, y, 0, 0, 0, 0];
                        let mut f = [0.0; 5];
                        if h.op(EE_SCREENS_SEA_POINT, &mut i, &mut f) != 0 && f[0] != height {
                            any = true;
                        }
                    }
                }
                any
            }
            SePaintKind::Bulldozer => true,
        };
        if changed {
            self.pending.push_back(rect);
        }
        changed
    }

    /// `Virtual4`: every queued area painted, the last queued first.
    pub fn run<H: SeaHost + ?Sized>(&mut self, h: &mut H) {
        match self.kind {
            SePaintKind::Terrain { terrain, texture } => self.run_terrain(h, terrain, texture),
            SePaintKind::Elevation { level } => self.run_elevation(h, level),
            SePaintKind::Bulldozer => self.run_bulldozer(h),
        }
    }

    fn run_terrain<H: SeaHost + ?Sized>(&mut self, h: &mut H, terrain: i32, texture: i32) {
        let mut info = [terrain, 0, 0, 0, 0, 0];
        h.op(EE_SCREENS_SEA_TERRAIN, &mut info, &mut [0.0; 5]);
        let ambients_only = info[1] != 0;
        call(h, EE_SCREENS_SEA_STOP_FOLLOW, &[]);
        while let Some(r) = self.pending.pop_back() {
            if !ambients_only {
                call(h, EE_SCREENS_SEA_PAINT_RECT, &[r.left, r.right, r.top, r.bottom, terrain, texture]);
            }
            plant_terrain_units(h, terrain, r);
            call(
                h,
                EE_SCREENS_SEA_UPDATE_CLIFFS,
                &[r.left.wrapping_sub(2), r.right.wrapping_add(3), r.bottom.wrapping_sub(2), r.top.wrapping_add(3)],
            );
            call(h, EE_SCREENS_SEA_MINIMAP_RECT, &[r.left, r.right, r.top, r.bottom]);
        }
        call(h, EE_SCREENS_SEA_MINIMAP_REFRESH, &[]);
    }

    fn run_elevation<H: SeaHost + ?Sized>(&mut self, h: &mut H, level: i8) {
        call(h, EE_SCREENS_SEA_STOP_FOLLOW, &[]);
        let (height, raise, slope, plateau, roughness) = match call(h, EE_SCREENS_SEA_ELEVATION_MODE, &[]) {
            0 => (f32::from(level) * level_height(h), true, SEA_SLOPE_GENTLE, 0.0f32, 1.0f32),
            1 => {
                let height = f32::from(level) * SEA_HILL_HEIGHT;
                (height, true, SEA_SLOPE_STEEP, 0.0, if height < SEA_HILL_LIMIT { 0.4 } else { 0.26 })
            }
            _ => (0.0, false, SEA_SLOPE_STEEP, SEA_SLOPE_GENTLE, 1.0),
        };
        let mut region = SeaRect::default();
        while let Some(r) = self.pending.pop_back() {
            region = r;
            self.painted.push(r);
            let mut i = [region.left, region.right, region.top, region.bottom, i32::from(raise), 0];
            let mut f = [height, plateau, slope, roughness, 0.0];
            h.op(EE_SCREENS_SEA_RAISE, &mut i, &mut f);
            region = SeaRect { left: i[0], right: i[1], top: i[2], bottom: i[3] };
            call(
                h,
                EE_SCREENS_SEA_REMOVE_FLAT_CLIFFS,
                &[region.left, region.right.wrapping_add(1), region.bottom, region.top.wrapping_add(1)],
            );
            call(
                h,
                EE_SCREENS_SEA_UPDATE_CLIFFS,
                &[
                    region.left.wrapping_sub(2),
                    region.right.wrapping_add(3),
                    region.bottom.wrapping_sub(2),
                    region.top.wrapping_add(3),
                ],
            );
            call(h, EE_SCREENS_SEA_MINIMAP_RECT, &[region.left, region.right, region.top, region.bottom]);
        }
        call(h, EE_SCREENS_SEA_STORE_MOUSE_TILE, &[]);
        call(h, EE_SCREENS_SEA_ELEVATION_CHANGED, &[region.left, region.top, region.right, region.bottom]);
        call(h, EE_SCREENS_SEA_MINIMAP_REFRESH, &[]);
    }

    fn run_bulldozer<H: SeaHost + ?Sized>(&mut self, h: &mut H) {
        call(h, EE_SCREENS_SEA_STOP_FOLLOW, &[]);
        while let Some(r) = self.pending.pop_back() {
            let mut units = Vec::new();
            for y in (r.bottom..=r.top).rev() {
                for x in r.left..=r.right {
                    units.extend(h.tile_units(x, y));
                }
            }
            remove_in_order(h, units);
            call(h, EE_SCREENS_SEA_MINIMAP_RECT, &[r.left, r.right, r.top, r.bottom]);
        }
    }
}

/// The units removed path points first, the others after them, each group in its gathered order (the original's
/// `std::sort` with "a path point before a unit that is not": an order its ties leave to the library).
pub fn remove_in_order<H: SeaHost + ?Sized>(h: &mut H, mut units: Vec<(usize, bool)>) {
    units.sort_by_key(|&(_, path_point)| !path_point);
    for (unit, _) in units {
        h.remove_unit(unit);
    }
}

fn level_height<H: SeaHost + ?Sized>(h: &mut H) -> f32 {
    let mut f = [0.0; 5];
    h.op(EE_SCREENS_SEA_LEVEL_HEIGHT, &mut [0; 6], &mut f);
    f[0]
}

/// The height an elevation paint of `level` gives in the tab's mode (`SetArea`'s test).
fn elevation_height<H: SeaHost + ?Sized>(h: &mut H, level: i8) -> f32 {
    match call(h, EE_SCREENS_SEA_ELEVATION_MODE, &[]) {
        0 => f32::from(level) * level_height(h),
        1 => f32::from(level) * SEA_HILL_HEIGHT,
        _ => 0.0,
    }
}

/// `PlantTerrainUnits`: the terrain's ambient units over the area's tiles, one draw (1..100 against the chance) per
/// tile on the map and one more for the class when it hits; the random seeded from the clock.
pub fn plant_terrain_units<H: SeaHost + ?Sized>(h: &mut H, terrain: i32, r: SeaRect) {
    let mut info = [terrain, 0, 0, 0, 0, 0];
    let chance = h.op(EE_SCREENS_SEA_TERRAIN, &mut info, &mut [0.0; 5]);
    if chance <= 0 {
        return;
    }
    let classes = info[2];
    let mut random = URandom { seed: call(h, EE_SCREENS_SEA_TIME, &[]) as u32 };
    for y in (r.bottom..=r.top).rev() {
        for x in r.left..=r.right {
            if call(h, EE_SCREENS_SEA_TILE, &[x, y]) == 0 || random.generate_int(1, 100) >= chance {
                continue;
            }
            let index = random.generate_int(0, classes.wrapping_sub(1));
            let class = call(h, EE_SCREENS_SEA_TERRAIN_CLASS, &[terrain, index]);
            call(h, EE_SCREENS_SEA_PLANT, &[class, x, y]);
        }
    }
}

/// `EESEAPaintArea::SetCorner`'s rectangle: the drag's start and new end corners -> (left, top, right, bottom), the
/// smaller coordinate first on each axis.
pub fn area_rectangle(start_x: i32, start_y: i32, end_x: i32, end_y: i32) -> (i32, i32, i32, i32) {
    let (left, right) = if start_x > end_x { (end_x, start_x) } else { (start_x, end_x) };
    let (top, bottom) = if start_y > end_y { (end_y, start_y) } else { (start_y, end_y) };
    (left, top, right, bottom)
}

// ---- group 2: the selection's actions (the click, the lasso, the rotation, the deletion)

/// The selection host's ops (`ee_screenscpp_seu(op, unit, a, f)`; a unit is an `EEUnit*`, 0 for none).
/// -> the editor's selection count.
pub const EE_SCREENS_SEU_SELECTED_COUNT: u32 = 1;
/// -> the selection's unit a (`GetSelectedUnit`).
pub const EE_SCREENS_SEU_SELECTED: u32 = 2;
/// -> the selection's first unit (`GetFirstSelectedUnit`, 0 when none).
pub const EE_SCREENS_SEU_FIRST_SELECTED: u32 = 3;
/// `ClearSelection`.
pub const EE_SCREENS_SEU_CLEAR: u32 = 4;
/// `DeselectFirstUnit`.
pub const EE_SCREENS_SEU_DESELECT_FIRST: u32 = 5;
/// `SelectUnit(unit)`.
pub const EE_SCREENS_SEU_SELECT: u32 = 6;
/// `DeselectUnit(unit)` (the unit may be NULL, as the click's toggle passes it).
pub const EE_SCREENS_SEU_DESELECT: u32 = 7;
/// The unit tab's `UpdateSelection`.
pub const EE_SCREENS_SEU_UPDATE_SELECTION: u32 = 8;
/// -> the world's unit of id a (`gWorld.GetUnit`).
pub const EE_SCREENS_SEU_UNIT: u32 = 9;
/// -> the unit of id a of the player whose index is `unit` (`gWorld.GetPlayer(index)->GetUnit(id)`).
pub const EE_SCREENS_SEU_PLAYER_UNIT: u32 = 10;
/// -> the unit's editor selection flag.
pub const EE_SCREENS_SEU_IS_SELECTED: u32 = 11;
/// -> whether the unit has a complex unit.
pub const EE_SCREENS_SEU_HAS_COMPLEX: u32 = 12;
/// -> whether the unit's class turns (angular acceleration above 0).
pub const EE_SCREENS_SEU_TURNS: u32 = 13;
/// The complex unit turned to the yaw f (its location's orientation, pitch and roll 0).
pub const EE_SCREENS_SEU_ROTATE: u32 = 14;
/// -> whether the unit is a path point.
pub const EE_SCREENS_SEU_PATH_POINT: u32 = 15;
/// -> whether the unit is off the map (its tile -10000, -10000).
pub const EE_SCREENS_SEU_OFF_MAP: u32 = 16;
/// -> whether the unit is on a tile's list (`mTileNode`: a complex unit's).
pub const EE_SCREENS_SEU_ON_TILE: u32 = 17;
/// A simple unit removed: a resource emptied, killed, taken from its player.
pub const EE_SCREENS_SEU_REMOVE_SIMPLE: u32 = 18;
/// -> the complex unit's player (`mPlayer`, an `EEPlayer*`).
pub const EE_SCREENS_SEU_PLAYER: u32 = 19;
/// An airport building leaves its player's air traffic list (when the unit is a land building with air traffic).
pub const EE_SCREENS_SEU_LEAVE_AIR_TRAFFIC: u32 = 20;
/// -> the complex unit's cargo count.
pub const EE_SCREENS_SEU_CARGO_COUNT: u32 = 21;
/// The complex unit's cargo killed (`KillCargo`).
pub const EE_SCREENS_SEU_KILL_CARGO: u32 = 22;
/// The complex unit killed and taken from the player a (an `EEPlayer*`: the first part's).
pub const EE_SCREENS_SEU_KILL_PART: u32 = 23;
/// -> the part after the complex unit (`mNextPart`).
pub const EE_SCREENS_SEU_NEXT_PART: u32 = 24;
/// -> the transport or building carrying the unit (`mContainer`).
pub const EE_SCREENS_SEU_CONTAINER: u32 = 25;
/// -> the container's cargo a.
pub const EE_SCREENS_SEU_CARGO: u32 = 26;
/// The container's cargo a unloaded at the container's location.
pub const EE_SCREENS_SEU_UNLOAD: u32 = 27;
/// -> the message's type (`unit`: an `EEMessage*`).
pub const EE_SCREENS_SEU_MESSAGE_TYPE: u32 = 28;
/// -> the message's unit id (an `EEMAddUnitToSelectedGroup`'s or `EEMSelectUnit`'s).
pub const EE_SCREENS_SEU_MESSAGE_UNIT: u32 = 29;
/// The message's reference released.
pub const EE_SCREENS_SEU_MESSAGE_RELEASE: u32 = 30;
/// -> the lasso message's unit type (`EEMSelectUnits::mL30`).
pub const EE_SCREENS_SEU_MESSAGE_UNIT_TYPE: u32 = 31;
/// -> the unit under the mouse, referenced (`gUnitPicker.PickUnit(gMouse->mPosition, 0)`), or 0.
pub const EE_SCREENS_SEU_PICK_AT_MOUSE: u32 = 32;
/// The unit's reference released.
pub const EE_SCREENS_SEU_UNIT_RELEASE: u32 = 33;
/// -> a new `EEMAddUnitToSelectedGroup` of the unit's id (an `EEMessage*`).
pub const EE_SCREENS_SEU_NEW_TOGGLE: u32 = 34;
/// -> a new `EEMUnselectAllUnits` (an `EEMessage*`).
pub const EE_SCREENS_SEU_NEW_UNSELECT_ALL: u32 = 35;
/// The click action (`unit`: an `EEAction*`, an `EEAMouseClick`) holds the message a (`SetMessage`).
pub const EE_SCREENS_SEU_SET_MESSAGE: u32 = 36;
/// The action (`unit`: an `EEAction*`) posted (`gUserInterface->PostAction`).
pub const EE_SCREENS_SEU_POST: u32 = 37;
/// -> the lasso message's rectangle (`&EEMSelectUnits::mRectangle`, a `GERectangle*`).
pub const EE_SCREENS_SEU_MESSAGE_RECTANGLE: u32 = 38;

/// The message types the click and the lasso apply (`EEMessageID.h`).
pub const EE_SCREENS_SEU_MSG_SELECT_UNIT: usize = 0x48;
pub const EE_SCREENS_SEU_MSG_ADD_UNIT: usize = 0x49;
pub const EE_SCREENS_SEU_MSG_ALWAYS_ADD_UNIT: usize = 0x76;
pub const EE_SCREENS_SEU_MSG_UNSELECT_ALL: usize = 0x4b;
pub const EE_SCREENS_SEU_MSG_SELECT_UNITS: usize = 0x4e;
pub const EE_SCREENS_SEU_MSG_ADD_UNITS: usize = 0x4f;
pub const EE_SCREENS_SEU_MSG_ADD_REMOVE_UNITS: usize = 0x50;

/// The selection host.
pub trait SeuHost {
    fn op(&mut self, op: u32, unit: usize, a: isize, f: f32) -> usize;
    /// The ids of the units of `unit_type` picked in the screen rectangle (`gUnitPicker.PickUnitsInRectangle`; the
    /// rectangle a `GERectangle*`).
    fn pick(&mut self, rectangle: usize, unit_type: i32) -> Vec<u32>;
}

fn seu<H: SeuHost + ?Sized>(h: &mut H, op: u32, unit: usize) -> usize {
    h.op(op, unit, 0, 0.0)
}

/// `EESEAMouseClick::Virtual4`: the click's message applied (select only the unit, toggle it, clear, always add),
/// the unit tab refreshed, the message released (the caller forgets it).
pub fn apply_click<H: SeuHost + ?Sized>(h: &mut H, message: usize) {
    if message == 0 {
        return;
    }
    let unit_of = |h: &mut H| {
        let id = seu(h, EE_SCREENS_SEU_MESSAGE_UNIT, message);
        h.op(EE_SCREENS_SEU_UNIT, 0, id as isize, 0.0)
    };
    match seu(h, EE_SCREENS_SEU_MESSAGE_TYPE, message) {
        EE_SCREENS_SEU_MSG_SELECT_UNIT => {
            seu(h, EE_SCREENS_SEU_CLEAR, 0);
            let unit = unit_of(h);
            if unit != 0 {
                seu(h, EE_SCREENS_SEU_SELECT, unit);
            }
        }
        EE_SCREENS_SEU_MSG_ADD_UNIT => {
            let unit = unit_of(h);
            if unit != 0 && seu(h, EE_SCREENS_SEU_IS_SELECTED, unit) == 0 {
                seu(h, EE_SCREENS_SEU_SELECT, unit);
            } else {
                seu(h, EE_SCREENS_SEU_DESELECT, unit);
            }
        }
        EE_SCREENS_SEU_MSG_UNSELECT_ALL => {
            seu(h, EE_SCREENS_SEU_CLEAR, 0);
        }
        EE_SCREENS_SEU_MSG_ALWAYS_ADD_UNIT => {
            let unit = unit_of(h);
            if unit != 0 && seu(h, EE_SCREENS_SEU_IS_SELECTED, unit) == 0 {
                seu(h, EE_SCREENS_SEU_SELECT, unit);
            }
        }
        _ => {}
    }
    seu(h, EE_SCREENS_SEU_UPDATE_SELECTION, 0);
    seu(h, EE_SCREENS_SEU_MESSAGE_RELEASE, message);
}

/// `EESEAMouseClick::Execute` (not while rotating): a selected unit under the mouse is toggled off (the toggle
/// message), empty ground clears the selection (the unselect-all message), an unselected unit does nothing; the
/// message made is the action's (`action`: its `EEAction*`) and the action posted.
pub fn click_execute<H: SeuHost + ?Sized>(h: &mut H, action: usize, rotating: bool) {
    if rotating {
        return;
    }
    let unit = seu(h, EE_SCREENS_SEU_PICK_AT_MOUSE, 0);
    let mut message = 0;
    if unit != 0 {
        if seu(h, EE_SCREENS_SEU_HAS_COMPLEX, unit) != 0 && seu(h, EE_SCREENS_SEU_IS_SELECTED, unit) != 0 {
            message = seu(h, EE_SCREENS_SEU_NEW_TOGGLE, unit);
        }
        seu(h, EE_SCREENS_SEU_UNIT_RELEASE, unit);
    } else {
        message = seu(h, EE_SCREENS_SEU_NEW_UNSELECT_ALL, 0);
    }
    if message != 0 {
        h.op(EE_SCREENS_SEU_SET_MESSAGE, action, message as isize, 0.0);
        seu(h, EE_SCREENS_SEU_POST, action);
    }
}

/// `EESEALasso::SelectInRectangle`: the selected units in the rectangle deselected, the action (`action`: its
/// `EEAction*`) posted (the original's second argument is unused).
pub fn lasso_in_rectangle<H: SeuHost + ?Sized>(h: &mut H, action: usize, rectangle: usize) {
    lasso_select(h, rectangle, 0, true, true);
    seu(h, EE_SCREENS_SEU_POST, action);
}

/// `EESEALasso::SelectUnits`: the units of the type picked in the rectangle selected when not yet (unless only
/// deselecting), else deselected when asked.
pub fn lasso_select<H: SeuHost + ?Sized>(
    h: &mut H,
    rectangle: usize,
    unit_type: i32,
    deselect: bool,
    deselect_only: bool,
) {
    for id in h.pick(rectangle, unit_type) {
        let unit = h.op(EE_SCREENS_SEU_UNIT, 0, id as isize, 0.0);
        if unit == 0 {
            continue;
        }
        if seu(h, EE_SCREENS_SEU_IS_SELECTED, unit) == 0 && !deselect_only {
            seu(h, EE_SCREENS_SEU_SELECT, unit);
        } else if deselect {
            seu(h, EE_SCREENS_SEU_DESELECT, unit);
        }
    }
}

/// `EESEALasso::Virtual4`: the lasso's message applied (replace the selection, add, or toggle the units in its
/// rectangle; `rectangle` is the message's), the unit tab refreshed, the message released.
pub fn apply_lasso<H: SeuHost + ?Sized>(h: &mut H, message: usize, rectangle: usize) {
    if message == 0 {
        return;
    }
    let unit_type = seu(h, EE_SCREENS_SEU_MESSAGE_UNIT_TYPE, message) as i32;
    match seu(h, EE_SCREENS_SEU_MESSAGE_TYPE, message) {
        EE_SCREENS_SEU_MSG_SELECT_UNITS => {
            seu(h, EE_SCREENS_SEU_CLEAR, 0);
            lasso_select(h, rectangle, unit_type, false, false);
        }
        EE_SCREENS_SEU_MSG_ADD_UNITS => lasso_select(h, rectangle, unit_type, false, false),
        EE_SCREENS_SEU_MSG_ADD_REMOVE_UNITS => lasso_select(h, rectangle, unit_type, true, false),
        _ => {}
    }
    seu(h, EE_SCREENS_SEU_UPDATE_SELECTION, 0);
    seu(h, EE_SCREENS_SEU_MESSAGE_RELEASE, message);
}

/// `EESEARotateUnits::Virtual4`: every selected unit that can turn turned to the yaw.
pub fn rotate_selection<H: SeuHost + ?Sized>(h: &mut H, yaw: f32) {
    let count = seu(h, EE_SCREENS_SEU_SELECTED_COUNT, 0);
    for k in 0..count {
        let unit = h.op(EE_SCREENS_SEU_SELECTED, 0, k as isize, 0.0);
        if seu(h, EE_SCREENS_SEU_HAS_COMPLEX, unit) == 0 || seu(h, EE_SCREENS_SEU_TURNS, unit) == 0 {
            continue;
        }
        h.op(EE_SCREENS_SEU_ROTATE, unit, 0, yaw);
    }
}

/// What a delete action removes: one unit of a player (`EESEADeleteUnit(id)`) or the selection, whole or its first
/// unit.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SeuDelete {
    Unit { player: i32, id: u32 },
    Selection { group: bool },
}

/// `EESEADeleteUnit::Virtual4`: the units gathered (the one unit, deselected; or the selection, cleared, or its
/// first unit, deselected), path points first; off-map units skipped, units on a tile removed part by part
/// (`DeleteComplexUnit`), the others emptied of resources and killed; the unit tab refreshed when a selected
/// transport changed.
pub fn delete_units<H: SeuHost + ?Sized>(h: &mut H, what: SeuDelete) {
    let mut update = false;
    let count = seu(h, EE_SCREENS_SEU_SELECTED_COUNT, 0);
    let mut units = Vec::new();
    match what {
        SeuDelete::Unit { player, id } => {
            let unit = h.op(EE_SCREENS_SEU_PLAYER_UNIT, player as usize, id as isize, 0.0);
            units.push(unit);
            if seu(h, EE_SCREENS_SEU_IS_SELECTED, unit) != 0 {
                seu(h, EE_SCREENS_SEU_DESELECT, unit);
            }
        }
        SeuDelete::Selection { group: true } if count > 0 => {
            for k in 0..count {
                let unit = h.op(EE_SCREENS_SEU_SELECTED, 0, k as isize, 0.0);
                if unit != 0 {
                    units.push(unit);
                }
            }
            seu(h, EE_SCREENS_SEU_CLEAR, 0);
        }
        SeuDelete::Selection { group: false } if count > 0 => {
            let unit = seu(h, EE_SCREENS_SEU_FIRST_SELECTED, 0);
            if unit != 0 {
                units.push(unit);
            }
            seu(h, EE_SCREENS_SEU_DESELECT_FIRST, 0);
        }
        SeuDelete::Selection { .. } => {}
    }
    // path points first (the original's std::sort with that order; ties in their gathered order)
    let mut keyed: Vec<(usize, bool)> =
        units.into_iter().map(|u| (u, seu(h, EE_SCREENS_SEU_PATH_POINT, u) != 0)).collect();
    keyed.sort_by_key(|&(_, path_point)| !path_point);
    for (unit, _) in keyed {
        if seu(h, EE_SCREENS_SEU_OFF_MAP, unit) != 0 {
            continue;
        }
        if seu(h, EE_SCREENS_SEU_ON_TILE, unit) != 0 {
            delete_complex_unit(h, unit, &mut update);
        } else {
            seu(h, EE_SCREENS_SEU_REMOVE_SIMPLE, unit);
        }
    }
    if update {
        seu(h, EE_SCREENS_SEU_UPDATE_SELECTION, 0);
    }
}

/// `DeleteComplexUnit`: an airport leaves the air traffic, then each part (the first part's player for all): its
/// cargo killed, out of its container, killed.
fn delete_complex_unit<H: SeuHost + ?Sized>(h: &mut H, first: usize, update: &mut bool) {
    let player = seu(h, EE_SCREENS_SEU_PLAYER, first);
    seu(h, EE_SCREENS_SEU_LEAVE_AIR_TRAFFIC, first);
    let mut unit = first;
    while unit != 0 {
        if seu(h, EE_SCREENS_SEU_CARGO_COUNT, unit) > 0 {
            seu(h, EE_SCREENS_SEU_KILL_CARGO, unit);
            if seu(h, EE_SCREENS_SEU_IS_SELECTED, unit) != 0 {
                *update = true;
            }
        }
        remove_from_container(h, unit, update);
        h.op(EE_SCREENS_SEU_KILL_PART, unit, player as isize, 0.0);
        unit = seu(h, EE_SCREENS_SEU_NEXT_PART, unit);
    }
}

/// `RemoveFromContainer`: the unit unloaded from the transport carrying it (the cargo count read again each turn).
fn remove_from_container<H: SeuHost + ?Sized>(h: &mut H, unit: usize, update: &mut bool) {
    let container = seu(h, EE_SCREENS_SEU_CONTAINER, unit);
    if container == 0 {
        return;
    }
    let mut k = 0;
    while k < seu(h, EE_SCREENS_SEU_CARGO_COUNT, container) {
        if h.op(EE_SCREENS_SEU_CARGO, container, k as isize, 0.0) == unit {
            h.op(EE_SCREENS_SEU_UNLOAD, container, k as isize, 0.0);
            if seu(h, EE_SCREENS_SEU_IS_SELECTED, container) != 0 {
                *update = true;
            }
        }
        k += 1;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// A recording host over a small diamond map: tiles where |x - y| < 4 and 4 <= x + y <= 20, points one more.
    #[derive(Default)]
    struct Rec {
        log: Vec<String>,
        mode: i32,
        heights: std::collections::HashMap<(i32, i32), f32>,
        units: std::collections::HashMap<(i32, i32), Vec<(usize, bool)>>,
        chance: i32,
        ambients_only: bool,
        time: i32,
    }

    fn on_map(x: i32, y: i32) -> bool {
        (x - y).abs() < 4 && (4..=20).contains(&(x + y))
    }

    impl SeaHost for Rec {
        fn op(&mut self, op: u32, i: &mut [i32; 6], f: &mut [f32; 5]) -> i32 {
            match op {
                EE_SCREENS_SEA_TILE => i32::from(on_map(i[0], i[1])),
                EE_SCREENS_SEA_POINT => {
                    if i[0] < 0 || i[1] < 0 || !on_map(i[0], i[1]) {
                        return 0;
                    }
                    f[0] = *self.heights.get(&(i[0], i[1])).unwrap_or(&0.0);
                    1
                }
                EE_SCREENS_SEA_TERRAIN => {
                    i[1] = i32::from(self.ambients_only);
                    i[2] = 3;
                    self.chance
                }
                EE_SCREENS_SEA_TERRAIN_CLASS => 100 + i[1],
                EE_SCREENS_SEA_TIME => self.time,
                EE_SCREENS_SEA_ELEVATION_MODE => self.mode,
                EE_SCREENS_SEA_LEVEL_HEIGHT => {
                    f[0] = 0.204_124_14;
                    0
                }
                EE_SCREENS_SEA_RAISE => {
                    self.log.push(format!("raise {:?} {:?}", &i[..5], &f[..4]));
                    i[0] -= 1; // Raise may widen the area
                    0
                }
                _ => {
                    self.log.push(format!("op {op} {:?}", &i[..]));
                    0
                }
            }
        }
        fn tile_units(&mut self, x: i32, y: i32) -> Vec<(usize, bool)> {
            self.units.get(&(x, y)).cloned().unwrap_or_default()
        }
        fn remove_unit(&mut self, unit: usize) {
            self.log.push(format!("remove {unit}"));
        }
    }

    #[test]
    fn terrain_queues_only_areas_on_the_map() {
        let mut h = Rec::default();
        let mut p = SePaint::new(SePaintKind::Terrain { terrain: 3, texture: 7 });
        assert!(!p.set_area(&mut h, 30, 2, 32, 0));
        assert!(p.set_area(&mut h, 5, 6, 6, 5));
        assert_eq!(p.pending.len(), 1);
    }

    #[test]
    fn terrain_paints_last_area_first_then_refreshes() {
        let mut h = Rec::default();
        let mut p = SePaint::new(SePaintKind::Terrain { terrain: 3, texture: 7 });
        p.set_area(&mut h, 5, 6, 6, 5);
        p.set_area(&mut h, 8, 9, 9, 8);
        p.run(&mut h);
        assert_eq!(
            h.log,
            [
                "op 1 [0, 0, 0, 0, 0, 0]",
                "op 4 [8, 9, 9, 8, 3, 7]",
                "op 5 [6, 12, 6, 12, 0, 0]",
                "op 6 [8, 9, 9, 8, 0, 0]",
                "op 4 [5, 6, 6, 5, 3, 7]",
                "op 5 [3, 9, 3, 9, 0, 0]",
                "op 6 [5, 6, 6, 5, 0, 0]",
                "op 7 [0, 0, 0, 0, 0, 0]",
            ]
        );
        assert!(p.pending.is_empty());
    }

    #[test]
    fn ambients_only_plants_without_painting_and_draws_per_tile() {
        let mut h = Rec { chance: 100, ambients_only: true, time: 5000, ..Rec::default() };
        let mut p = SePaint::new(SePaintKind::Terrain { terrain: 41, texture: 2 });
        p.set_area(&mut h, 5, 6, 6, 5);
        p.run(&mut h);
        // no paint; four tiles on the map, every draw under 100 hits: one plant each, in y-down, x-up order
        let plants: Vec<&String> = h.log.iter().filter(|l| l.starts_with("op 11")).collect();
        assert_eq!(plants.len(), 4);
        assert!(!h.log.iter().any(|l| l.starts_with("op 4 ")));
        let mut r = URandom { seed: 5000 };
        let mut want = Vec::new();
        for (x, y) in [(5, 6), (6, 6), (5, 5), (6, 5)] {
            assert!(r.generate_int(1, 100) < 100);
            want.push(format!("op 11 [{}, {x}, {y}, 0, 0, 0]", 100 + r.generate_int(0, 2)));
        }
        assert_eq!(plants, want.iter().collect::<Vec<_>>());
    }

    #[test]
    fn no_chance_no_draw() {
        let mut h = Rec { chance: 0, ..Rec::default() };
        plant_terrain_units(&mut h, 3, SeaRect { left: 5, top: 6, right: 6, bottom: 5 });
        assert!(h.log.is_empty());
    }

    #[test]
    fn elevation_set_area_tests_the_points_one_beyond() {
        let mut h = Rec::default();
        let mut p = SePaint::new(SePaintKind::Elevation { level: 0 });
        h.mode = 2; // level ground: height 0 everywhere already
        assert!(!p.set_area(&mut h, 5, 6, 6, 5));
        h.heights.insert((7, 7), 0.5); // right + 1, top + 1
        assert!(p.set_area(&mut h, 5, 6, 6, 5));
    }

    #[test]
    fn elevation_modes_and_the_last_region() {
        for (mode, level, want) in [
            (0, 3i8, "raise [5, 6, 6, 5, 1] [0.6123724, 0.0, 0.40824828, 1.0]"),
            (1, 1, "raise [5, 6, 6, 5, 1] [0.81649655, 0.0, 1.6329931, 0.4]"),
            (1, 2, "raise [5, 6, 6, 5, 1] [1.6329931, 0.0, 1.6329931, 0.26]"),
            (2, 4, "raise [5, 6, 6, 5, 0] [0.0, 0.40824828, 1.6329931, 1.0]"),
        ] {
            let mut h = Rec { mode, ..Rec::default() };
            h.heights.insert((5, 5), 9.0);
            let mut p = SePaint::new(SePaintKind::Elevation { level });
            assert!(p.set_area(&mut h, 5, 6, 6, 5));
            p.run(&mut h);
            assert_eq!(h.log[1], want, "mode {mode} level {level}");
            // the area as Raise left it (left - 1) goes on to the cliffs, the minimap and the effects
            assert_eq!(h.log[2], "op 14 [4, 7, 5, 7, 0, 0]");
            assert_eq!(h.log[3], "op 5 [2, 9, 3, 9, 0, 0]");
            assert_eq!(h.log[4], "op 6 [4, 6, 6, 5, 0, 0]");
            assert_eq!(h.log[5], "op 15 [0, 0, 0, 0, 0, 0]");
            assert_eq!(h.log[6], "op 16 [4, 6, 6, 5, 0, 0]");
            assert_eq!(p.painted.len(), 1);
        }
    }

    #[test]
    fn elevation_without_areas_tells_the_effects_an_empty_one() {
        let mut h = Rec::default();
        let mut p = SePaint::new(SePaintKind::Elevation { level: 1 });
        p.run(&mut h);
        assert_eq!(h.log[1..], ["op 15 [0, 0, 0, 0, 0, 0]", "op 16 [0, 0, 0, 0, 0, 0]", "op 7 [0, 0, 0, 0, 0, 0]"]);
    }

    #[test]
    fn bulldozer_removes_path_points_first() {
        let mut h = Rec::default();
        h.units.insert((5, 6), vec![(1, false), (2, true)]);
        h.units.insert((6, 5), vec![(3, true), (4, false)]);
        let mut p = SePaint::new(SePaintKind::Bulldozer);
        assert!(p.set_area(&mut h, 5, 6, 6, 5));
        p.run(&mut h);
        assert_eq!(
            h.log,
            ["op 1 [0, 0, 0, 0, 0, 0]", "remove 2", "remove 3", "remove 1", "remove 4", "op 6 [5, 6, 6, 5, 0, 0]",]
        );
    }

    #[test]
    fn kinds_from_codes() {
        assert_eq!(
            SePaintKind::from_code(EE_SCREENS_SEA_PAINT_TERRAIN, 3, 7),
            SePaintKind::Terrain { terrain: 3, texture: 7 }
        );
        assert_eq!(SePaintKind::from_code(EE_SCREENS_SEA_PAINT_ELEVATION, -2, 0), SePaintKind::Elevation { level: -2 });
        assert_eq!(SePaintKind::from_code(EE_SCREENS_SEA_PAINT_BULLDOZER, 0, 0), SePaintKind::Bulldozer);
    }

    #[test]
    fn the_area_rectangle_orders_its_corners() {
        assert_eq!(area_rectangle(3, 4, 9, 12), (3, 4, 9, 12));
        assert_eq!(area_rectangle(9, 12, 3, 4), (3, 4, 9, 12));
        assert_eq!(area_rectangle(9, 4, 3, 12), (3, 4, 9, 12));
    }

    // ---- group 2

    #[derive(Clone, Default)]
    struct U {
        selected: bool,
        complex: bool,
        turns: bool,
        path: bool,
        off_map: bool,
        on_tile: bool,
        player: usize,
        cargo: Vec<usize>,
        container: usize,
        next: usize,
    }

    /// Units 1.. by handle; a message is 1000 + type with the unit id in `msg_unit`.
    #[derive(Default)]
    struct Sel {
        units: Vec<U>,
        selection: Vec<usize>,
        msg_unit: usize,
        picked: Vec<u32>,
        log: Vec<String>,
        /// the unit under the mouse
        mouse: usize,
    }

    impl Sel {
        fn with(n: usize) -> Self {
            Sel { units: vec![U::default(); n + 1], ..Sel::default() }
        }
    }

    impl SeuHost for Sel {
        fn op(&mut self, op: u32, unit: usize, a: isize, f: f32) -> usize {
            let u = |s: &Sel| s.units[unit].clone();
            match op {
                EE_SCREENS_SEU_SELECTED_COUNT => self.selection.len(),
                EE_SCREENS_SEU_SELECTED => self.selection[a as usize],
                EE_SCREENS_SEU_FIRST_SELECTED => self.selection.first().copied().unwrap_or(0),
                EE_SCREENS_SEU_UNIT => {
                    if (a as usize) < self.units.len() {
                        a as usize
                    } else {
                        0
                    }
                }
                EE_SCREENS_SEU_PLAYER_UNIT => a as usize,
                EE_SCREENS_SEU_IS_SELECTED => usize::from(u(self).selected),
                EE_SCREENS_SEU_HAS_COMPLEX => usize::from(u(self).complex),
                EE_SCREENS_SEU_TURNS => usize::from(u(self).turns),
                EE_SCREENS_SEU_PATH_POINT => usize::from(u(self).path),
                EE_SCREENS_SEU_OFF_MAP => usize::from(u(self).off_map),
                EE_SCREENS_SEU_ON_TILE => usize::from(u(self).on_tile),
                EE_SCREENS_SEU_PLAYER => u(self).player,
                EE_SCREENS_SEU_CARGO_COUNT => u(self).cargo.len(),
                EE_SCREENS_SEU_CARGO => u(self).cargo[a as usize],
                EE_SCREENS_SEU_CONTAINER => u(self).container,
                EE_SCREENS_SEU_NEXT_PART => u(self).next,
                EE_SCREENS_SEU_MESSAGE_TYPE => unit - 1000,
                EE_SCREENS_SEU_MESSAGE_UNIT => self.msg_unit,
                EE_SCREENS_SEU_MESSAGE_UNIT_TYPE => 7,
                EE_SCREENS_SEU_PICK_AT_MOUSE => self.mouse,
                EE_SCREENS_SEU_NEW_TOGGLE => {
                    self.log.push(format!("toggle message {unit}"));
                    2000 + unit
                }
                EE_SCREENS_SEU_NEW_UNSELECT_ALL => {
                    self.log.push("unselect-all message".into());
                    3000
                }
                EE_SCREENS_SEU_MESSAGE_RECTANGLE => unit + 5000,
                EE_SCREENS_SEU_SELECT => {
                    self.units[unit].selected = true;
                    self.selection.push(unit);
                    self.log.push(format!("select {unit}"));
                    0
                }
                EE_SCREENS_SEU_DESELECT => {
                    if unit != 0 {
                        self.units[unit].selected = false;
                    }
                    self.selection.retain(|&x| x != unit);
                    self.log.push(format!("deselect {unit}"));
                    0
                }
                EE_SCREENS_SEU_CLEAR => {
                    for &x in &self.selection {
                        self.units[x].selected = false;
                    }
                    self.selection.clear();
                    self.log.push("clear".into());
                    0
                }
                EE_SCREENS_SEU_DESELECT_FIRST => {
                    let x = self.selection.remove(0);
                    self.units[x].selected = false;
                    self.log.push("deselect first".into());
                    0
                }
                EE_SCREENS_SEU_UNLOAD => {
                    let x = self.units[unit].cargo.remove(a as usize);
                    self.units[x].container = 0;
                    self.log.push(format!("unload {unit}.{a}"));
                    0
                }
                EE_SCREENS_SEU_ROTATE => {
                    self.log.push(format!("rotate {unit} {f}"));
                    0
                }
                _ => {
                    self.log.push(format!("op {op} {unit} {a}"));
                    0
                }
            }
        }
        fn pick(&mut self, rectangle: usize, unit_type: i32) -> Vec<u32> {
            self.log.push(format!("pick {rectangle} {unit_type}"));
            self.picked.clone()
        }
    }

    #[test]
    fn click_messages() {
        let mut h = Sel::with(4);
        h.msg_unit = 2;
        apply_click(&mut h, 1000 + EE_SCREENS_SEU_MSG_SELECT_UNIT);
        assert_eq!(h.selection, [2]);
        h.msg_unit = 3;
        apply_click(&mut h, 1000 + EE_SCREENS_SEU_MSG_ALWAYS_ADD_UNIT);
        apply_click(&mut h, 1000 + EE_SCREENS_SEU_MSG_ALWAYS_ADD_UNIT);
        assert_eq!(h.selection, [2, 3]);
        apply_click(&mut h, 1000 + EE_SCREENS_SEU_MSG_ADD_UNIT); // the toggle: off
        assert_eq!(h.selection, [2]);
        h.msg_unit = 99; // no such unit: the toggle deselects NULL, as the original
        h.log.clear();
        apply_click(&mut h, 1000 + EE_SCREENS_SEU_MSG_ADD_UNIT);
        assert_eq!(h.log, ["deselect 0", "op 8 0 0", "op 30 1073 0"]);
        apply_click(&mut h, 1000 + EE_SCREENS_SEU_MSG_UNSELECT_ALL);
        assert!(h.selection.is_empty());
        h.log.clear();
        apply_click(&mut h, 0);
        assert!(h.log.is_empty());
    }

    #[test]
    fn lasso_messages() {
        let mut h = Sel::with(4);
        h.picked = vec![1, 2, 9];
        apply_lasso(&mut h, 1000 + EE_SCREENS_SEU_MSG_SELECT_UNITS, 55);
        assert_eq!(h.selection, [1, 2]);
        assert_eq!(h.log[0], "clear");
        assert_eq!(h.log[1], "pick 55 7");
        h.picked = vec![2, 3];
        apply_lasso(&mut h, 1000 + EE_SCREENS_SEU_MSG_ADD_REMOVE_UNITS, 55);
        assert_eq!(h.selection, [1, 3]);
        // SelectInRectangle: only the selected ones in the rectangle deselected
        h.picked = vec![1, 4];
        lasso_select(&mut h, 55, 0, true, true);
        assert_eq!(h.selection, [3]);
    }

    #[test]
    fn click_execute_makes_the_message_and_posts() {
        let (rel, set, post) = (EE_SCREENS_SEU_UNIT_RELEASE, EE_SCREENS_SEU_SET_MESSAGE, EE_SCREENS_SEU_POST);
        // rotating: nothing, not even a pick
        let mut h = Sel::with(3);
        h.mouse = 1;
        click_execute(&mut h, 77, true);
        assert!(h.log.is_empty());
        // nothing under the mouse: unselect all
        h.mouse = 0;
        click_execute(&mut h, 77, false);
        assert_eq!(h.log, ["unselect-all message".to_string(), format!("op {set} 77 3000"), format!("op {post} 77 0")]);
        // a selected unit of a complex unit: the toggle, then the unit released before the post
        h.log.clear();
        h.units[2] = U { complex: true, selected: true, ..U::default() };
        h.mouse = 2;
        click_execute(&mut h, 77, false);
        assert_eq!(
            h.log,
            [
                "toggle message 2".to_string(),
                format!("op {rel} 2 0"),
                format!("op {set} 77 2002"),
                format!("op {post} 77 0")
            ]
        );
        // an unselected unit, or a selected one without a complex unit: released, no message, not posted
        h.log.clear();
        h.units[1] = U { complex: true, ..U::default() };
        h.mouse = 1;
        click_execute(&mut h, 77, false);
        h.units[3] = U { selected: true, ..U::default() };
        h.mouse = 3;
        click_execute(&mut h, 77, false);
        assert_eq!(h.log, [format!("op {rel} 1 0"), format!("op {rel} 3 0")]);
    }

    #[test]
    fn lasso_in_rectangle_deselects_then_posts() {
        let mut h = Sel::with(4);
        h.units[1].selected = true;
        h.units[2].selected = true;
        h.selection = vec![1, 2];
        h.picked = vec![2, 3];
        lasso_in_rectangle(&mut h, 88, 55);
        assert_eq!(h.selection, [1]);
        let post = EE_SCREENS_SEU_POST;
        // (the original deselects an unselected unit too: DeselectUnit on it does nothing)
        assert_eq!(
            h.log,
            ["pick 55 0".to_string(), "deselect 2".to_string(), "deselect 3".to_string(), format!("op {post} 88 0")]
        );
    }

    #[test]
    fn rotation_skips_units_that_cannot_turn() {
        let mut h = Sel::with(3);
        h.units[1] = U { complex: true, turns: true, ..U::default() };
        h.units[2] = U { complex: true, ..U::default() };
        h.units[3] = U { turns: true, ..U::default() };
        h.selection = vec![1, 2, 3];
        rotate_selection(&mut h, 0.75);
        assert_eq!(h.log, ["rotate 1 0.75"]);
    }

    #[test]
    fn delete_one_unit_and_the_selection() {
        let mut h = Sel::with(5);
        h.units[1] = U { on_tile: true, selected: true, player: 77, ..U::default() };
        h.selection = vec![1];
        delete_units(&mut h, SeuDelete::Unit { player: 2, id: 1 });
        assert_eq!(h.log, ["deselect 1", "op 20 1 0", "op 23 1 77"]);
        // the group: path points first, the off-map unit skipped, a simple unit removed whole
        let mut h = Sel::with(5);
        h.units[2] = U { off_map: true, ..U::default() };
        h.units[3] = U { ..U::default() };
        h.units[4] = U { path: true, ..U::default() };
        h.selection = vec![2, 3, 4];
        delete_units(&mut h, SeuDelete::Selection { group: true });
        assert_eq!(h.log, ["clear", "op 18 4 0", "op 18 3 0"]);
        // the first only
        let mut h = Sel::with(5);
        h.selection = vec![3, 4];
        delete_units(&mut h, SeuDelete::Selection { group: false });
        assert_eq!(h.log, ["deselect first", "op 18 3 0"]);
        assert_eq!(h.selection, [4]);
        // nothing selected: nothing
        let mut h = Sel::with(2);
        delete_units(&mut h, SeuDelete::Selection { group: true });
        assert!(h.log.is_empty());
    }

    #[test]
    fn delete_a_multi_part_transport_with_cargo() {
        let mut h = Sel::with(6);
        // 1 -> 2 parts; 1 carries 5 and is selected; 2 is in transport 6 (selected)
        h.units[1] = U { on_tile: true, player: 9, cargo: vec![5], next: 2, selected: true, ..U::default() };
        h.units[2] = U { container: 6, ..U::default() };
        h.units[6] = U { cargo: vec![3, 2], selected: true, ..U::default() };
        h.selection = vec![1];
        delete_units(&mut h, SeuDelete::Selection { group: false });
        assert_eq!(
            h.log,
            ["deselect first", "op 20 1 0", "op 22 1 0", "op 23 1 9", "unload 6.1", "op 23 2 9", "op 8 0 0",]
        );
    }
}
