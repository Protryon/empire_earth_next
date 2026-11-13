//! The scenario editor's actions that live whole in Rust (round 16): their C++ classes are gone; one C++ action
//! (`EESEARust`, `ee/EESEARust.h`) holds a [`SewAction`] and runs it. The kinds:
//! - the save, test and exit actions (`EESEASaveScenario`, `EESEATestScenario`, `EESEAExitScenario`): the save
//!   screen opened (the map prepared first in the editor), the editor left to test the scenario or not;
//! - the quick save (`EESEAQuickSaveScenario`): the tab's settings stored, the editor's quick save action posted in
//!   save mode, written to disk;
//! - the tech lists' move (`EESEATechTree`): the lines `first..last` refilled in the left or right list;
//! - the rotation (`EESEARotateUnits`): the selection turned to a yaw;
//! - the trigger area's rectangle (`EESEAPaintArea`): the drag's rectangle as the editor's paint area;
//! - the blank map (`EESEACreateBlankMap`): a new world over a map of one terrain, its heights from the map tab's
//!   elevation file when one is picked (the file's heights rotated 45 degrees onto the diamond, scaled to the land
//!   height and water depth, then water, shorelines and cliffs);
//! - the random map (`EESEACreateRandomMap`): a new world from a random map generator for the players not observing;
//! - the wall and gate placements (`EESEAPlaceWall`, `EESEAPlaceGate`): queued lines and gates built for their
//!   players;
//! - the unit placement and move (`EESEAPlaceUnit`, `EESEAMoveUnit`: `editor_place.rs`).
//!
//! The world, the screens and the files are the host's ([`SewHost`]); every call is in the original's order and
//! the elevation file's floats are the original's operations (`f32`, in order; Windows' `min` and `max` macros).

use ee_ge::types::GeTransformation;
use ee_ge::um::lrintf;

use crate::editor_actions::area_rectangle;
use crate::editor_place::{SepHost, SepMove, SepPlace};
use crate::save_load::{EE_SCREENS_SL_SAVE_SCREEN_GAME, EE_SCREENS_SL_SAVE_SCREEN_SCENARIO};

// ---- the kinds (`EESEARust`'s constructor: `object`, then a..d)

/// `EESEASaveScenario`.
pub const EE_SCREENS_SEW_SAVE: u32 = 1;
/// `EESEATestScenario`.
pub const EE_SCREENS_SEW_TEST: u32 = 2;
/// `EESEAExitScenario`.
pub const EE_SCREENS_SEW_EXIT: u32 = 3;
/// `EESEAQuickSaveScenario` (object: the editor's `EEAQuickSaveLoad`, referenced while the action lives).
pub const EE_SCREENS_SEW_QUICK_SAVE: u32 = 4;
/// `EESEATechTree` (a first line, b last line, c player, d the left list); posted when made.
pub const EE_SCREENS_SEW_TECH_TREE: u32 = 5;
/// `EESEARotateUnits` (`ee_screens_sew_rotate` posts it).
pub const EE_SCREENS_SEW_ROTATE: u32 = 6;
/// `EESEAPaintArea` (a, b the first corner); posted when made.
pub const EE_SCREENS_SEW_PAINT_AREA: u32 = 7;
/// `EESEACreateBlankMap` (a width, b height, c terrain); posted when made.
pub const EE_SCREENS_SEW_BLANK_MAP: u32 = 8;
/// `EESEACreateRandomMap` (a size, b style); posted when made.
pub const EE_SCREENS_SEW_RANDOM_MAP: u32 = 9;
/// `EESEAPlaceWall` (`ee_screens_sew_wall` queues and posts).
pub const EE_SCREENS_SEW_PLACE_WALL: u32 = 10;
/// `EESEAPlaceGate` (`ee_screens_sew_gate` queues and posts).
pub const EE_SCREENS_SEW_PLACE_GATE: u32 = 11;
/// `EESEAPlaceUnit` (`ee_screens_sew_place_unit` queues and posts).
pub const EE_SCREENS_SEW_PLACE_UNIT: u32 = 12;
/// `EESEAMoveUnit` (`ee_screens_sew_pick_up`, `_put_down`, `_cancel`).
pub const EE_SCREENS_SEW_MOVE_UNIT: u32 = 13;

// ---- the host's ops (`ee_screenscpp_sew`: `p` a pointer, `i` up to 8 integers, `f` up to 4 floats, in and out)

/// `gUserInterface->PostAction(p)` (p: the action's `EESEARust`).
pub const EE_SCREENS_SEW_OP_POST: u32 = 1;
/// The object p referenced / released (`UReferenceCounter`).
pub const EE_SCREENS_SEW_OP_REFERENCE: u32 = 2;
pub const EE_SCREENS_SEW_OP_DEREFERENCE: u32 = 3;
/// The editor left (`ExitEditor(i0 != 0)`: to test the scenario).
pub const EE_SCREENS_SEW_OP_EXIT_EDITOR: u32 = 4;
/// The editor's current tab's settings stored (`RefreshCurrentGroup`).
pub const EE_SCREENS_SEW_OP_REFRESH_GROUP: u32 = 5;
/// -> whether the scenario editor is open (`mInScenarioEditor`).
pub const EE_SCREENS_SEW_OP_IN_EDITOR: u32 = 6;
/// `gWorld.mMap->PrepareSave()`.
pub const EE_SCREENS_SEW_OP_PREPARE_SAVE: u32 = 7;
/// The screen event i0 (`EECreateScreenEvent`) posted.
pub const EE_SCREENS_SEW_OP_POST_SCREEN_EVENT: u32 = 8;
/// The quick save p (`EEAQuickSaveLoad`) posted to save, written to disk.
pub const EE_SCREENS_SEW_OP_QUICK_SAVE: u32 = 9;
/// The unit types tab's tech list (i0: the left one) filled with the lines i1..i2.
pub const EE_SCREENS_SEW_OP_TECH_FILL: u32 = 10;
/// The selection turned to the yaw f0 (`editor_actions::rotate_selection`).
pub const EE_SCREENS_SEW_OP_ROTATE: u32 = 11;
/// The editor's paint area set to the rectangle i0..i3 (left, top, right, bottom).
pub const EE_SCREENS_SEW_OP_PAINT_AREA: u32 = 12;
/// The renderer paused (posted, then waited for with both locks let go) and the locks taken again.
pub const EE_SCREENS_SEW_OP_PAUSE: u32 = 13;
/// The world reset, a new scenario manager and environment manager.
pub const EE_SCREENS_SEW_OP_RESET: u32 = 14;
/// -> the world's player count (`GetNumPlayers`).
pub const EE_SCREENS_SEW_OP_PLAYER_COUNT: u32 = 15;
/// A new map (width i0, height i1, players i2, terrain i3) set as the world's.
pub const EE_SCREENS_SEW_OP_NEW_MAP: u32 = 16;
/// -> the map's size (`mSize`).
pub const EE_SCREENS_SEW_OP_MAP_SIZE: u32 = 17;
/// -> whether the mesh point (i0, i1) is valid (the coordinates unsigned, as `GetIsValidPoint`).
pub const EE_SCREENS_SEW_OP_POINT: u32 = 18;
/// -> whether the mesh tile (i0, i1) is valid.
pub const EE_SCREENS_SEW_OP_TILE: u32 = 19;
/// The mesh point (i0, i1)'s height -> f0.
pub const EE_SCREENS_SEW_OP_GET_Z: u32 = 20;
/// The mesh point (i0, i1)'s height set to f0.
pub const EE_SCREENS_SEW_OP_SET_Z: u32 = 21;
/// `ComputeVertexNormal(i0, i1)`.
pub const EE_SCREENS_SEW_OP_NORMAL: u32 = 22;
/// The tile (i0, i1): water when i2 (`SetIsWater`), its height f0.
pub const EE_SCREENS_SEW_OP_TILE_WATER: u32 = 23;
/// `ComputeShorelines(i0, i1)`.
pub const EE_SCREENS_SEW_OP_SHORELINES: u32 = 24;
/// `EEMap::PaintTerrain(i0, i1, i2)`.
pub const EE_SCREENS_SEW_OP_PAINT_TERRAIN: u32 = 25;
/// `SetWaterColorSet(i0, i1)`.
pub const EE_SCREENS_SEW_OP_WATER_COLOR: u32 = 26;
/// `EEMap::UpdateCliffs(i0, i1, i2, i3)`.
pub const EE_SCREENS_SEW_OP_UPDATE_CLIFFS: u32 = 27;
/// The continents cleared (`gContinentManager.Clear`).
pub const EE_SCREENS_SEW_OP_CONTINENTS_CLEAR: u32 = 28;
/// The continents' regions built (`BuildRegions`).
pub const EE_SCREENS_SEW_OP_CONTINENTS_BUILD: u32 = 29;
/// -> whether the terrain i0 scatters ambient units (its chance above 0).
pub const EE_SCREENS_SEW_OP_TERRAIN_AMBIENT: u32 = 30;
/// A terrain paint of the terrain i0 (its own colour type) over the whole map (size i1) posted.
pub const EE_SCREENS_SEW_OP_POST_AMBIENT_PAINT: u32 = 31;
/// The influence table made for the map.
pub const EE_SCREENS_SEW_OP_INFLUENCE: u32 = 32;
/// The camera put at (f0, f1, f2).
pub const EE_SCREENS_SEW_OP_CAMERA: u32 = 33;
/// The minimap given the map (`SetMap`, `EEUIMiniMap::Update`).
pub const EE_SCREENS_SEW_OP_MINIMAP_SET: u32 = 34;
/// The minimap redrawn (`RedrawAll`, `Redraw`, `Refresh`).
pub const EE_SCREENS_SEW_OP_MINIMAP_REDRAW: u32 = 35;
/// The scene released and made again, the renderer resumed.
pub const EE_SCREENS_SEW_OP_RESUME: u32 = 36;
/// -> the local player's id.
pub const EE_SCREENS_SEW_OP_LOCAL_PLAYER: u32 = 37;
/// The random map size i0's dimensions -> i0, i1 (`EEGameOptions::GetMapDimensions`).
pub const EE_SCREENS_SEW_OP_MAP_DIMENSIONS: u32 = 38;
/// -> the default terrain (`gDefaultTerrain`).
pub const EE_SCREENS_SEW_OP_DEFAULT_TERRAIN: u32 = 39;
/// -> the world's player slots (`mMaxPlayers`: `CopyPlayers`' count).
pub const EE_SCREENS_SEW_OP_MAX_PLAYERS: u32 = 40;
/// The player i0: -> whether there is one; i1 observing, i2 human, i3 its tech tree's start epoch.
pub const EE_SCREENS_SEW_OP_PLAYER: u32 = 41;
/// `EERandomMap::CreateRandomMap` for the players p (i32 ids, i0 of them), the local index i1, the generator of
/// style i2, the start epoch i3, the size i4 -> the camera's location f0..f2.
pub const EE_SCREENS_SEW_OP_RANDOM_MAP: u32 = 42;
/// The player i0's pending units given their media and added to the world (buildings' buttons loaded).
pub const EE_SCREENS_SEW_OP_ADD_PENDING: u32 = 43;
/// The player i0 loads the media of the technology i1's unit class (`SetUnitClassMedia(.., 0x7f)`).
pub const EE_SCREENS_SEW_OP_LOAD_MEDIA: u32 = 44;
/// The player i0 releases the media of the technology i1's unit class.
pub const EE_SCREENS_SEW_OP_UNLOAD_MEDIA: u32 = 45;
/// `EEPlanWall` from (f0, f1) to (f2, f3) of the player i0's class of the technology i1.
pub const EE_SCREENS_SEW_OP_PLAN_WALL: u32 = 46;
/// The ghosts' lock taken (i0 != 0) or let go.
pub const EE_SCREENS_SEW_OP_GHOST_LOCK: u32 = 47;
/// -> whether every ghost can be placed (true without any).
pub const EE_SCREENS_SEW_OP_GHOSTS_PLACEABLE: u32 = 48;
/// `EEBuildGate` on the tile (i0, i1) for the player i2, the technology i3, the direction i4.
pub const EE_SCREENS_SEW_OP_BUILD_GATE: u32 = 49;
/// The unit tab places its selected unit class again (`SelectUnitType`).
pub const EE_SCREENS_SEW_OP_SELECT_UNIT_TYPE: u32 = 50;

/// The elevation file picked on the map tab, read whole.
#[derive(Clone, Debug, PartialEq)]
pub struct SewElevation {
    pub width: u32,
    pub height: u32,
    /// the heights, `width * height` (row by row)
    pub data: Vec<i16>,
    pub water_cutoff: i32,
    pub water_depth: i32,
    pub land_height: i32,
    pub land_cutoff: i32,
}

/// The world, the screens and the files (the placements' world calls: [`SepHost`]).
pub trait SewHost: SepHost {
    /// One op of the `EE_SCREENS_SEW_OP_*` list.
    fn op(&mut self, op: u32, p: usize, i: &mut [i32; 8], f: &mut [f32; 4]) -> i32;
    /// The map tab's elevation file when one is picked and reads whole (`CreateMap`'s files).
    fn elevation(&mut self) -> Option<SewElevation>;
}

fn op<H: SewHost + ?Sized>(h: &mut H, code: u32, ints: &[i32]) -> i32 {
    let mut i = [0i32; 8];
    i[..ints.len()].copy_from_slice(ints);
    h.op(code, 0, &mut i, &mut [0.0; 4])
}

fn op_f<H: SewHost + ?Sized>(h: &mut H, code: u32, ints: &[i32], floats: &[f32]) -> i32 {
    let mut i = [0i32; 8];
    i[..ints.len()].copy_from_slice(ints);
    let mut f = [0.0f32; 4];
    f[..floats.len()].copy_from_slice(floats);
    h.op(code, 0, &mut i, &mut f)
}

/// A queued wall line.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct SewWall {
    pub start: (f32, f32),
    pub end: (f32, f32),
    pub player: i32,
    pub technology: i32,
}

/// A queued gate.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct SewGate {
    pub x: i32,
    pub y: i32,
    pub player: i32,
    pub technology: i32,
    pub direction: i32,
}

/// What an action does.
#[derive(Clone, Debug, PartialEq)]
pub enum SewKind {
    Save,
    Test,
    Exit,
    QuickSave { quick_save: usize },
    TechTree { first: i32, last: i32, left: bool },
    Rotate { yaw: f32 },
    PaintArea { start: (i32, i32), end: (i32, i32), rectangle: [i32; 4] },
    BlankMap { width: u32, height: u32, terrain: i32 },
    RandomMap { size: u32, style: u32 },
    PlaceWall { walls: Vec<SewWall> },
    PlaceGate { gates: Vec<SewGate> },
    PlaceUnit(SepPlace),
    MoveUnit(SepMove),
}

/// An action: its kind and its `EESEARust` (the object posted).
#[derive(Clone, Debug, PartialEq)]
pub struct SewAction {
    pub owner: usize,
    pub kind: SewKind,
}

impl SewAction {
    /// The action of `kind` (its `EESEARust` `owner`; `object` and a..d as the kinds say); None for an unknown kind.
    /// The quick save's object is referenced.
    #[allow(clippy::too_many_arguments)]
    pub fn new<H: SewHost + ?Sized>(
        h: &mut H,
        owner: usize,
        kind: u32,
        object: usize,
        a: i32,
        b: i32,
        c: i32,
        d: i32,
    ) -> Option<SewAction> {
        let _ = c;
        let kind = match kind {
            EE_SCREENS_SEW_SAVE => SewKind::Save,
            EE_SCREENS_SEW_TEST => SewKind::Test,
            EE_SCREENS_SEW_EXIT => SewKind::Exit,
            EE_SCREENS_SEW_QUICK_SAVE => {
                h.op(EE_SCREENS_SEW_OP_REFERENCE, object, &mut [0; 8], &mut [0.0; 4]);
                SewKind::QuickSave { quick_save: object }
            }
            EE_SCREENS_SEW_TECH_TREE => SewKind::TechTree { first: a, last: b, left: d != 0 },
            EE_SCREENS_SEW_ROTATE => SewKind::Rotate { yaw: 0.0 },
            EE_SCREENS_SEW_PAINT_AREA => SewKind::PaintArea { start: (a, b), end: (a, b), rectangle: [a, b, a, b] },
            EE_SCREENS_SEW_BLANK_MAP => SewKind::BlankMap { width: a as u32, height: b as u32, terrain: c },
            EE_SCREENS_SEW_RANDOM_MAP => SewKind::RandomMap { size: a as u32, style: b as u32 },
            EE_SCREENS_SEW_PLACE_WALL => SewKind::PlaceWall { walls: Vec::new() },
            EE_SCREENS_SEW_PLACE_GATE => SewKind::PlaceGate { gates: Vec::new() },
            EE_SCREENS_SEW_PLACE_UNIT => SewKind::PlaceUnit(SepPlace::default()),
            EE_SCREENS_SEW_MOVE_UNIT => SewKind::MoveUnit(SepMove::default()),
            _ => return None,
        };
        Some(SewAction { owner, kind })
    }

    fn post<H: SewHost + ?Sized>(&self, h: &mut H) {
        h.op(EE_SCREENS_SEW_OP_POST, self.owner, &mut [0; 8], &mut [0.0; 4]);
    }

    /// After the C++ object holds the action: the kinds whose constructor posted them post now.
    pub fn begin<H: SewHost + ?Sized>(&self, h: &mut H) {
        match self.kind {
            SewKind::TechTree { .. }
            | SewKind::PaintArea { .. }
            | SewKind::BlankMap { .. }
            | SewKind::RandomMap { .. } => self.post(h),
            _ => {}
        }
    }

    /// The action freed: the quick save's object released.
    pub fn free<H: SewHost + ?Sized>(&self, h: &mut H) {
        if let SewKind::QuickSave { quick_save } = self.kind {
            h.op(EE_SCREENS_SEW_OP_DEREFERENCE, quick_save, &mut [0; 8], &mut [0.0; 4]);
        }
    }

    /// `EESEARotateUnits::Rotate`: the yaw kept and the action posted.
    pub fn rotate<H: SewHost + ?Sized>(&mut self, h: &mut H, to: f32) {
        if let SewKind::Rotate { yaw } = &mut self.kind {
            *yaw = to;
            self.post(h);
        }
    }

    /// `EESEAPaintArea::SetCorner`: when the dragged corner moved, the rectangle between it and the first corner,
    /// and the action posted.
    pub fn area_corner<H: SewHost + ?Sized>(&mut self, h: &mut H, x: i32, y: i32) {
        if let SewKind::PaintArea { start, end, rectangle } = &mut self.kind {
            if *end == (x, y) {
                return;
            }
            *end = (x, y);
            let (l, t, r, b) = area_rectangle(start.0, start.1, x, y);
            *rectangle = [l, t, r, b];
            self.post(h);
        }
    }

    /// The trigger area's rectangle (left, top, right, bottom).
    pub fn area(&self) -> [i32; 4] {
        match self.kind {
            SewKind::PaintArea { rectangle, .. } => rectangle,
            _ => [0; 4],
        }
    }

    /// `EESEAPlaceWall::Place`: the wall's media loaded, the line queued, the action posted.
    pub fn place_wall<H: SewHost + ?Sized>(&mut self, h: &mut H, wall: SewWall) {
        if let SewKind::PlaceWall { walls } = &mut self.kind {
            op(h, EE_SCREENS_SEW_OP_LOAD_MEDIA, &[wall.player, wall.technology]);
            walls.push(wall);
            self.post(h);
        }
    }

    /// `EESEAPlaceGate::Place`: when every ghost can be placed, the gate's media loaded, the gate queued and the
    /// action posted (under the ghosts' lock); whether it was.
    pub fn place_gate<H: SewHost + ?Sized>(&mut self, h: &mut H, gate: SewGate) -> bool {
        if !matches!(self.kind, SewKind::PlaceGate { .. }) {
            return false;
        }
        op(h, EE_SCREENS_SEW_OP_GHOST_LOCK, &[1]);
        let placed = op(h, EE_SCREENS_SEW_OP_GHOSTS_PLACEABLE, &[]) != 0;
        if placed {
            op(h, EE_SCREENS_SEW_OP_LOAD_MEDIA, &[gate.player, gate.technology]);
            if let SewKind::PlaceGate { gates } = &mut self.kind {
                gates.push(gate);
            }
            self.post(h);
        }
        op(h, EE_SCREENS_SEW_OP_GHOST_LOCK, &[0]);
        placed
    }

    /// `Virtual4`: the action run (on the UI thread, under the UI copy lock and the world's write lock).
    pub fn run<H: SewHost + ?Sized>(&mut self, h: &mut H) {
        match &mut self.kind {
            SewKind::Test => {
                op(h, EE_SCREENS_SEW_OP_EXIT_EDITOR, &[1]);
            }
            SewKind::Exit => {
                op(h, EE_SCREENS_SEW_OP_EXIT_EDITOR, &[0]);
            }
            SewKind::Save => {
                op(h, EE_SCREENS_SEW_OP_REFRESH_GROUP, &[]);
                let event = if op(h, EE_SCREENS_SEW_OP_IN_EDITOR, &[]) != 0 {
                    op(h, EE_SCREENS_SEW_OP_PREPARE_SAVE, &[]);
                    EE_SCREENS_SL_SAVE_SCREEN_SCENARIO
                } else {
                    EE_SCREENS_SL_SAVE_SCREEN_GAME
                };
                op(h, EE_SCREENS_SEW_OP_POST_SCREEN_EVENT, &[event as i32]);
            }
            SewKind::QuickSave { quick_save } => {
                op(h, EE_SCREENS_SEW_OP_REFRESH_GROUP, &[]);
                h.op(EE_SCREENS_SEW_OP_QUICK_SAVE, *quick_save, &mut [0; 8], &mut [0.0; 4]);
            }
            SewKind::TechTree { first, last, left } => {
                op(h, EE_SCREENS_SEW_OP_TECH_FILL, &[i32::from(*left), *first, *last]);
            }
            SewKind::Rotate { yaw } => {
                op_f(h, EE_SCREENS_SEW_OP_ROTATE, &[], &[*yaw]);
            }
            SewKind::PaintArea { rectangle, .. } => {
                op(h, EE_SCREENS_SEW_OP_PAINT_AREA, &rectangle[..]);
            }
            SewKind::BlankMap { width, height, terrain } => blank_map(h, *width, height, *terrain),
            SewKind::RandomMap { size, style } => random_map(h, *size, *style),
            SewKind::PlaceWall { walls } => {
                while let Some(w) = walls.pop() {
                    op_f(
                        h,
                        EE_SCREENS_SEW_OP_PLAN_WALL,
                        &[w.player, w.technology],
                        &[w.start.0, w.start.1, w.end.0, w.end.1],
                    );
                    op(h, EE_SCREENS_SEW_OP_UNLOAD_MEDIA, &[w.player, w.technology]);
                }
            }
            SewKind::PlaceGate { gates } => {
                while let Some(g) = gates.pop() {
                    op(h, EE_SCREENS_SEW_OP_BUILD_GATE, &[g.x, g.y, g.player, g.technology, g.direction]);
                    op(h, EE_SCREENS_SEW_OP_UNLOAD_MEDIA, &[g.player, g.technology]);
                }
                op(h, EE_SCREENS_SEW_OP_SELECT_UNIT_TYPE, &[]);
            }
            SewKind::PlaceUnit(place) => place.run(h),
            SewKind::MoveUnit(moving) => moving.run(h),
        }
    }

    /// `EESEAPlaceUnit::Place(player)`.
    pub fn place_unit<H: SewHost + ?Sized>(&mut self, h: &mut H, player: i32) -> bool {
        let owner = self.owner;
        match &mut self.kind {
            SewKind::PlaceUnit(place) => place.place(h, owner, player),
            _ => false,
        }
    }

    /// A unit queued as `Place` queues one, without a ghost (the original's `mUnits.push_back`; not posted).
    pub fn place_push(&mut self, entry: crate::editor_place::SepPlaced) {
        if let SewKind::PlaceUnit(place) = &mut self.kind {
            place.units.push(entry);
        }
    }

    /// `EESEAMoveUnit::PickUp(id)`.
    pub fn pick_up<H: SewHost + ?Sized>(&mut self, h: &mut H, id: u32) {
        let owner = self.owner;
        if let SewKind::MoveUnit(moving) = &mut self.kind {
            moving.pick_up(h, owner, id);
        }
    }

    /// `EESEAMoveUnit::PutDown`.
    pub fn put_down<H: SewHost + ?Sized>(&mut self, h: &mut H) {
        let owner = self.owner;
        if let SewKind::MoveUnit(moving) = &mut self.kind {
            moving.put_down(h, owner);
        }
    }

    /// `EESEAMoveUnit::Cancel`.
    pub fn cancel<H: SewHost + ?Sized>(&mut self, h: &mut H) {
        if let SewKind::MoveUnit(moving) = &mut self.kind {
            moving.cancel(h);
        }
    }

    /// The move's `mIsMoving`: a unit is picked up.
    pub fn moving(&self) -> bool {
        matches!(&self.kind, SewKind::MoveUnit(m) if m.moving)
    }
}

/// Windows' `max(a, b)` macro: `a > b ? a : b`.
fn max_macro(a: f32, b: f32) -> f32 {
    if a > b {
        a
    } else {
        b
    }
}

/// Windows' `min(a, b)` macro: `a < b ? a : b`.
fn min_macro(a: f32, b: f32) -> f32 {
    if a < b {
        a
    } else {
        b
    }
}

/// The renderer paused and the world reset (both new-map actions begin so).
fn pause_and_reset<H: SewHost + ?Sized>(h: &mut H) {
    op(h, EE_SCREENS_SEW_OP_PAUSE, &[]);
    op(h, EE_SCREENS_SEW_OP_RESET, &[]);
}

/// The minimap redrawn and the renderer resumed (both new-map actions end so).
fn redraw_and_resume<H: SewHost + ?Sized>(h: &mut H) {
    op(h, EE_SCREENS_SEW_OP_MINIMAP_REDRAW, &[]);
    op(h, EE_SCREENS_SEW_OP_RESUME, &[]);
}

/// `EESEACreateBlankMap::Virtual4`.
fn blank_map<H: SewHost + ?Sized>(h: &mut H, width: u32, height: &mut u32, terrain: i32) {
    pause_and_reset(h);
    create_map(h, width, height, terrain);
    op(h, EE_SCREENS_SEW_OP_CONTINENTS_CLEAR, &[]);
    let size = op(h, EE_SCREENS_SEW_OP_MAP_SIZE, &[]);
    if op(h, EE_SCREENS_SEW_OP_TERRAIN_AMBIENT, &[terrain]) != 0 {
        op(h, EE_SCREENS_SEW_OP_POST_AMBIENT_PAINT, &[terrain, size]);
    }
    op(h, EE_SCREENS_SEW_OP_CONTINENTS_BUILD, &[]);
    op(h, EE_SCREENS_SEW_OP_INFLUENCE, &[]);
    let centre = size as u32 as f32 * 0.5;
    op_f(h, EE_SCREENS_SEW_OP_CAMERA, &[], &[centre, centre, 0.0]);
    op(h, EE_SCREENS_SEW_OP_MINIMAP_SET, &[]);
    redraw_and_resume(h);
}

/// The file's height at the map's point (x, y): the point through the transformation, rounded, clamped to the file.
fn file_point(t: &GeTransformation, x: u32, y: u32, file: &SewElevation) -> usize {
    let (px, py) = (x as f32, y as f32);
    let m = &t.m;
    let wx = py * m[0][1] + px * m[0][0] + m[0][3];
    let wy = px * m[1][0] + py * m[1][1] + m[1][3];
    // unsigned long theX = max(UMFloatToLong(x), 0); min(theX, width - 1) (unsigned)
    let fx = (lrintf(wx).max(0) as u32).min(file.width.wrapping_sub(1));
    let fy = (lrintf(wy).max(0) as u32).min(file.height.wrapping_sub(1));
    fy.wrapping_mul(file.width).wrapping_add(fx) as usize
}

/// `EESEACreateBlankMap::CreateMap`: the map of the terrain, shaped by the elevation file when one is picked (its
/// height follows the file's aspect then).
pub fn create_map<H: SewHost + ?Sized>(h: &mut H, width: u32, height: &mut u32, terrain: i32) {
    let file = h.elevation();
    if let Some(e) = &file {
        *height = width.wrapping_mul(e.height) / e.width;
    }
    let players = op(h, EE_SCREENS_SEW_OP_PLAYER_COUNT, &[]);
    op(h, EE_SCREENS_SEW_OP_NEW_MAP, &[width as i32, *height as i32, players, terrain]);
    let Some(file) = file else { return };
    let land_scale =
        if file.land_cutoff as f32 == 0.0 { 0.0 } else { (file.land_height as f32 / file.land_cutoff as f32).abs() };
    let water_scale =
        if file.water_cutoff as f32 == 0.0 { 0.0 } else { (file.water_depth as f32 / file.water_cutoff as f32).abs() };
    let mut t = GeTransformation::IDENTITY;
    t.rotate(-core::f32::consts::FRAC_PI_4, 0.0, 0.0);
    t.scale(1.0, -((file.height as f32 - 1.0) / (*height as f32 * core::f32::consts::SQRT_2)), 1.0);
    let (cx, cy) = {
        let m = &t.m;
        let (px, py) = (0.0f32, *height as f32);
        (py * m[0][1] + px * m[0][0] + m[0][3], px * m[1][0] + py * m[1][1] + m[1][3])
    };
    // Translate(-cx, -cy, 0)
    t.m[0][3] += -cx;
    t.m[1][3] += -cy;
    t.m[2][3] += 0.0;
    t.scale((file.width as f32 - 1.0) / (width as f32 * core::f32::consts::SQRT_2), 1.0, 1.0);
    let size = op(h, EE_SCREENS_SEW_OP_MAP_SIZE, &[]) as u32;
    let point = |h: &mut H, x: u32, y: u32| op(h, EE_SCREENS_SEW_OP_POINT, &[x as i32, y as i32]) != 0;
    let tile = |h: &mut H, x: u32, y: u32| op(h, EE_SCREENS_SEW_OP_TILE, &[x as i32, y as i32]) != 0;
    let get_z = |h: &mut H, x: u32, y: u32| {
        let mut i = [x as i32, y as i32, 0, 0, 0, 0, 0, 0];
        let mut f = [0.0f32; 4];
        h.op(EE_SCREENS_SEW_OP_GET_Z, 0, &mut i, &mut f);
        f[0]
    };
    // (float)-depth: the integer negated (no -0.0)
    let water_depth = file.water_depth.wrapping_neg() as f32;
    let land_height = file.land_height as f32;
    for x in 0..size {
        for y in 0..size {
            if !point(h, x, y) {
                continue;
            }
            let mut z = file.data[file_point(&t, x, y, &file)] as f32;
            if z < 0.0 {
                z = max_macro(z * water_scale, water_depth);
            } else {
                z = min_macro(z * land_scale, land_height);
            }
            if z < 0.0 && z > -0.1 {
                z = 0.0;
            }
            op_f(h, EE_SCREENS_SEW_OP_SET_Z, &[x as i32, y as i32], &[z]);
        }
    }
    for x in 0..size {
        for y in 0..size {
            if !point(h, x, y) {
                continue;
            }
            op(h, EE_SCREENS_SEW_OP_NORMAL, &[x as i32, y as i32]);
            if !tile(h, x, y) {
                continue;
            }
            let mut least = min_macro(get_z(h, x, y), get_z(h, x + 1, y));
            least = min_macro(least, get_z(h, x, y + 1));
            least = min_macro(least, get_z(h, x + 1, y + 1));
            op_f(h, EE_SCREENS_SEW_OP_TILE_WATER, &[x as i32, y as i32, i32::from(least < 0.0)], &[least]);
        }
    }
    for x in 0..size {
        for y in 0..size {
            if tile(h, x, y) {
                op(h, EE_SCREENS_SEW_OP_SHORELINES, &[x as i32, y as i32]);
            }
        }
    }
    // the neighbours in the original's order (x - 1 at 0 wraps: no point there)
    const AROUND: [(i32, i32); 8] = [(-1, 1), (0, 1), (1, 1), (-1, 0), (1, 0), (-1, -1), (0, -1), (1, -1)];
    for x in 0..size {
        for y in 0..size {
            if !point(h, x, y) {
                continue;
            }
            let depth = get_z(h, x, y);
            #[allow(clippy::neg_cmp_op_on_partial_ord)]
            if !(depth < 0.0) {
                continue;
            }
            if tile(h, x, y) {
                op(h, EE_SCREENS_SEW_OP_PAINT_TERRAIN, &[x as i32, y as i32, 0x1f]);
            }
            let mut highest = 0.0f32;
            for (dx, dy) in AROUND {
                let (nx, ny) = (x.wrapping_add_signed(dx), y.wrapping_add_signed(dy));
                if point(h, nx, ny) {
                    highest = max_macro(highest, get_z(h, nx, ny));
                }
            }
            if -depth > highest {
                op(h, EE_SCREENS_SEW_OP_WATER_COLOR, &[x as i32, y as i32]);
            }
        }
    }
    let size = op(h, EE_SCREENS_SEW_OP_MAP_SIZE, &[]);
    op(h, EE_SCREENS_SEW_OP_UPDATE_CLIFFS, &[0, size, 0, size]);
}

/// `EESEACreateRandomMap::Virtual4`: the new world's map from the generator of `style` for the players not
/// observing (the local player's index among them; the start epoch the lowest of the human players', at most 14).
fn random_map<H: SewHost + ?Sized>(h: &mut H, size: u32, style: u32) {
    let local = op(h, EE_SCREENS_SEW_OP_LOCAL_PLAYER, &[]);
    op(h, EE_SCREENS_SEW_OP_PAUSE, &[]);
    let mut i = [size as i32, 0, 0, 0, 0, 0, 0, 0];
    h.op(EE_SCREENS_SEW_OP_MAP_DIMENSIONS, 0, &mut i, &mut [0.0; 4]);
    let (width, height) = (i[0], i[1]);
    let players = op(h, EE_SCREENS_SEW_OP_PLAYER_COUNT, &[]);
    op(h, EE_SCREENS_SEW_OP_RESET, &[]);
    let terrain = op(h, EE_SCREENS_SEW_OP_DEFAULT_TERRAIN, &[]);
    op(h, EE_SCREENS_SEW_OP_NEW_MAP, &[width, height, players, terrain]);
    op(h, EE_SCREENS_SEW_OP_CONTINENTS_CLEAR, &[]);
    op(h, EE_SCREENS_SEW_OP_INFLUENCE, &[]);
    op(h, EE_SCREENS_SEW_OP_MINIMAP_SET, &[]);
    let slots = op(h, EE_SCREENS_SEW_OP_MAX_PLAYERS, &[]).max(0);
    let mut active: Vec<i32> = Vec::new();
    let mut local_index = 0i32;
    let mut epoch = 14i32;
    for k in 0..slots {
        let mut i = [k, 0, 0, 0, 0, 0, 0, 0];
        if h.op(EE_SCREENS_SEW_OP_PLAYER, 0, &mut i, &mut [0.0; 4]) == 0 || i[1] != 0 {
            continue;
        }
        active.push(k);
        if k == local {
            local_index = active.len() as i32 - 1;
        }
        if k == 0 || i[2] == 0 {
            continue;
        }
        if i[3] < epoch {
            epoch = i[3];
        }
    }
    let mut i = [active.len() as i32, local_index, style as i32, epoch, size as i32, 0, 0, 0];
    let mut f = [0.0f32; 4];
    h.op(EE_SCREENS_SEW_OP_RANDOM_MAP, active.as_ptr() as usize, &mut i, &mut f);
    op_f(h, EE_SCREENS_SEW_OP_CAMERA, &[], &[f[0], f[1], f[2]]);
    for k in 0..slots {
        op(h, EE_SCREENS_SEW_OP_ADD_PENDING, &[k]);
    }
    op(h, EE_SCREENS_SEW_OP_CONTINENTS_CLEAR, &[]);
    op(h, EE_SCREENS_SEW_OP_CONTINENTS_BUILD, &[]);
    redraw_and_resume(h);
}

#[cfg(test)]
mod tests {
    use super::*;

    /// A recording host over a square mesh of `size` points (heights kept), with an optional elevation file.
    struct Rec {
        log: Vec<String>,
        size: u32,
        z: Vec<f32>,
        file: Option<SewElevation>,
        ghosts_ok: bool,
        players: Vec<(bool, bool, i32)>,
    }

    impl Rec {
        fn new() -> Rec {
            Rec { log: Vec::new(), size: 4, z: vec![0.0; 16], file: None, ghosts_ok: true, players: Vec::new() }
        }
        fn valid(&self, x: i32, y: i32) -> bool {
            x >= 0 && y >= 0 && (x as u32) < self.size && (y as u32) < self.size
        }
    }

    impl SepHost for Rec {
        fn sep(&mut self, op: u32, _: usize, _: &mut GeTransformation, _: &mut [i32; 8], _: &mut [f32; 4]) -> i32 {
            self.log.push(format!("sep {op}"));
            0
        }
    }

    impl SewHost for Rec {
        fn op(&mut self, op: u32, p: usize, i: &mut [i32; 8], f: &mut [f32; 4]) -> i32 {
            match op {
                EE_SCREENS_SEW_OP_MAP_SIZE => return self.size as i32,
                EE_SCREENS_SEW_OP_POINT => return i32::from(self.valid(i[0], i[1])),
                EE_SCREENS_SEW_OP_TILE => return i32::from(self.valid(i[0] + 1, i[1] + 1) && self.valid(i[0], i[1])),
                EE_SCREENS_SEW_OP_GET_Z => {
                    f[0] = self.z[(i[0] as u32 * self.size + i[1] as u32) as usize];
                    return 0;
                }
                EE_SCREENS_SEW_OP_SET_Z => {
                    self.z[(i[0] as u32 * self.size + i[1] as u32) as usize] = f[0];
                    self.log.push(format!("z {} {} {}", i[0], i[1], f[0]));
                    return 0;
                }
                EE_SCREENS_SEW_OP_GHOSTS_PLACEABLE => return i32::from(self.ghosts_ok),
                EE_SCREENS_SEW_OP_MAX_PLAYERS => return self.players.len() as i32,
                EE_SCREENS_SEW_OP_PLAYER => {
                    let (obs, human, epoch) = self.players[i[0] as usize];
                    i[1] = i32::from(obs);
                    i[2] = i32::from(human);
                    i[3] = epoch;
                    return 1;
                }
                EE_SCREENS_SEW_OP_LOCAL_PLAYER => return 2,
                EE_SCREENS_SEW_OP_IN_EDITOR => return 1,
                EE_SCREENS_SEW_OP_TERRAIN_AMBIENT => return i32::from(i[0] == 7),
                EE_SCREENS_SEW_OP_MAP_DIMENSIONS => {
                    i[0] = 100;
                    i[1] = 90;
                    return 0;
                }
                EE_SCREENS_SEW_OP_RANDOM_MAP => {
                    let ids = unsafe { core::slice::from_raw_parts(p as *const i32, i[0] as usize) };
                    self.log.push(format!("random {ids:?} local {} style {} epoch {} size {}", i[1], i[2], i[3], i[4]));
                    f[0] = 5.0;
                    f[1] = 6.0;
                    return 0;
                }
                _ => {}
            }
            self.log.push(format!("{op} p{p} {:?} {:?}", &i[..5], &f[..3]));
            0
        }
        fn elevation(&mut self) -> Option<SewElevation> {
            self.file.clone()
        }
    }

    fn make(h: &mut Rec, kind: u32, object: usize, a: i32, b: i32, c: i32, d: i32) -> SewAction {
        let action = SewAction::new(h, 0x100, kind, object, a, b, c, d).unwrap();
        action.begin(h);
        action
    }

    #[test]
    fn unknown_kind_is_none() {
        assert!(SewAction::new(&mut Rec::new(), 1, 99, 0, 0, 0, 0, 0).is_none());
    }

    #[test]
    fn save_test_exit() {
        let mut h = Rec::new();
        for kind in [EE_SCREENS_SEW_SAVE, EE_SCREENS_SEW_TEST, EE_SCREENS_SEW_EXIT] {
            make(&mut h, kind, 0, 0, 0, 0, 0).run(&mut h);
        }
        assert_eq!(
            h.log,
            [
                "5 p0 [0, 0, 0, 0, 0] [0.0, 0.0, 0.0]",
                "7 p0 [0, 0, 0, 0, 0] [0.0, 0.0, 0.0]",
                "8 p0 [773, 0, 0, 0, 0] [0.0, 0.0, 0.0]",
                "4 p0 [1, 0, 0, 0, 0] [0.0, 0.0, 0.0]",
                "4 p0 [0, 0, 0, 0, 0] [0.0, 0.0, 0.0]",
            ]
        );
    }

    #[test]
    fn quick_save_references_its_object() {
        let mut h = Rec::new();
        let mut a = make(&mut h, EE_SCREENS_SEW_QUICK_SAVE, 0x55, 0, 0, 0, 0);
        a.run(&mut h);
        a.free(&mut h);
        let ops: Vec<&str> = h.log.iter().map(|l| l.split(' ').next().unwrap()).collect();
        assert_eq!(ops, ["2", "5", "9", "3"]);
        assert!(h.log[2].starts_with("9 p85"));
    }

    #[test]
    fn tech_tree_posts_when_made_and_fills_the_list() {
        let mut h = Rec::new();
        make(&mut h, EE_SCREENS_SEW_TECH_TREE, 0, 3, 9, 2, 1).run(&mut h);
        assert_eq!(h.log[0], "1 p256 [0, 0, 0, 0, 0] [0.0, 0.0, 0.0]");
        assert_eq!(h.log[1], "10 p0 [1, 3, 9, 0, 0] [0.0, 0.0, 0.0]");
    }

    #[test]
    fn rotation_keeps_the_yaw() {
        let mut h = Rec::new();
        let mut a = make(&mut h, EE_SCREENS_SEW_ROTATE, 0, 0, 0, 0, 0);
        assert!(h.log.is_empty());
        a.rotate(&mut h, 0.75);
        a.run(&mut h);
        assert_eq!(h.log, ["1 p256 [0, 0, 0, 0, 0] [0.0, 0.0, 0.0]", "11 p0 [0, 0, 0, 0, 0] [0.75, 0.0, 0.0]"]);
    }

    #[test]
    fn paint_area_follows_the_corner() {
        let mut h = Rec::new();
        let mut a = make(&mut h, EE_SCREENS_SEW_PAINT_AREA, 0, 5, 6, 0, 0);
        assert_eq!(a.area(), [5, 6, 5, 6]);
        a.area_corner(&mut h, 2, 9);
        assert_eq!(a.area(), [2, 6, 5, 9]);
        a.area_corner(&mut h, 2, 9); // unchanged: not posted again
        a.run(&mut h);
        assert_eq!(h.log.iter().filter(|l| l.starts_with("1 ")).count(), 2);
        assert_eq!(h.log.last().unwrap(), "12 p0 [2, 6, 5, 9, 0] [0.0, 0.0, 0.0]");
    }

    #[test]
    fn walls_and_gates_run_last_queued_first() {
        let mut h = Rec::new();
        let mut w = make(&mut h, EE_SCREENS_SEW_PLACE_WALL, 0, 0, 0, 0, 0);
        w.place_wall(&mut h, SewWall { start: (1.0, 2.0), end: (3.0, 4.0), player: 1, technology: 7 });
        w.place_wall(&mut h, SewWall { start: (5.0, 6.0), end: (7.0, 8.0), player: 2, technology: 8 });
        h.log.clear();
        w.run(&mut h);
        assert_eq!(
            h.log,
            [
                "46 p0 [2, 8, 0, 0, 0] [5.0, 6.0, 7.0]",
                "45 p0 [2, 8, 0, 0, 0] [0.0, 0.0, 0.0]",
                "46 p0 [1, 7, 0, 0, 0] [1.0, 2.0, 3.0]",
                "45 p0 [1, 7, 0, 0, 0] [0.0, 0.0, 0.0]",
            ]
        );
        let mut g = make(&mut h, EE_SCREENS_SEW_PLACE_GATE, 0, 0, 0, 0, 0);
        h.ghosts_ok = false;
        assert!(!g.place_gate(&mut h, SewGate { x: 1, y: 2, player: 1, technology: 3, direction: 2 }));
        h.ghosts_ok = true;
        assert!(g.place_gate(&mut h, SewGate { x: 4, y: 5, player: 1, technology: 3, direction: 0 }));
        h.log.clear();
        g.run(&mut h);
        assert_eq!(
            h.log,
            [
                "49 p0 [4, 5, 1, 3, 0] [0.0, 0.0, 0.0]",
                "45 p0 [1, 3, 0, 0, 0] [0.0, 0.0, 0.0]",
                "50 p0 [0, 0, 0, 0, 0] [0.0, 0.0, 0.0]",
            ]
        );
    }

    #[test]
    fn blank_map_without_a_file_scatters_ambients_of_its_terrain() {
        let mut h = Rec::new();
        make(&mut h, EE_SCREENS_SEW_BLANK_MAP, 0, 40, 40, 7, 0).run(&mut h);
        let ops: Vec<&str> = h.log.iter().map(|l| l.split(' ').next().unwrap()).collect();
        assert_eq!(ops, ["1", "13", "14", "15", "16", "28", "31", "29", "32", "33", "34", "35", "36"]);
        assert_eq!(h.log[4], "16 p0 [40, 40, 0, 7, 0] [0.0, 0.0, 0.0]");
        assert_eq!(h.log[6], "31 p0 [7, 4, 0, 0, 0] [0.0, 0.0, 0.0]");
        assert_eq!(h.log[9], "33 p0 [0, 0, 0, 0, 0] [2.0, 2.0, 0.0]");
    }

    #[test]
    fn blank_map_from_a_file_follows_its_aspect_and_heights() {
        let mut h = Rec::new();
        h.file = Some(SewElevation {
            width: 4,
            height: 2,
            data: vec![-200, -5, 100, 300, -1, 0, 50, 1000],
            water_cutoff: 100,
            water_depth: 4,
            land_height: 12,
            land_cutoff: 100,
        });
        let mut height = 40;
        create_map(&mut h, 8, &mut height, 3);
        assert_eq!(height, 4);
        assert!(h.log.iter().any(|l| l == "16 p0 [8, 4, 0, 3, 0] [0.0, 0.0, 0.0]"));
        // every height is the file's scaled and clamped: water at most 4 deep, land at most 12 high, the shallowest
        // water (above -0.1) flat
        for z in &h.z {
            assert!((-4.0..=12.0).contains(z), "{z}");
            assert!(!(*z < 0.0 && *z > -0.1));
        }
        assert_eq!(h.log.last().unwrap(), "27 p0 [0, 4, 0, 4, 0] [0.0, 0.0, 0.0]");
        // the water tiles painted and coloured
        assert!(!h.log.iter().any(|l| l.starts_with("25 ")) || h.z.iter().any(|z| *z < 0.0));
    }

    #[test]
    fn the_transformation_maps_the_diamond_onto_the_file() {
        // the original's arithmetic for a 64 x 48 file on a 40-wide map (height 30): the map's corner point (0, 30)
        // lands on the file's corner (0, 0)
        let file = SewElevation {
            width: 64,
            height: 48,
            data: vec![0; 64 * 48],
            water_cutoff: 0,
            water_depth: 0,
            land_height: 0,
            land_cutoff: 0,
        };
        let mut t = GeTransformation::IDENTITY;
        t.rotate(-core::f32::consts::FRAC_PI_4, 0.0, 0.0);
        t.scale(1.0, -((48.0f32 - 1.0) / (30.0f32 * core::f32::consts::SQRT_2)), 1.0);
        let (cx, cy) = (30.0 * t.m[0][1] + t.m[0][3], 30.0 * t.m[1][1] + t.m[1][3]);
        t.m[0][3] += -cx;
        t.m[1][3] += -cy;
        t.scale((64.0f32 - 1.0) / (40.0f32 * core::f32::consts::SQRT_2), 1.0, 1.0);
        assert_eq!(file_point(&t, 0, 30, &file), 0);
        // far points clamp to the file's last row and column
        assert!(file_point(&t, 1000, 1000, &file) < 64 * 48);
    }

    #[test]
    fn macros_keep_the_second_on_ties() {
        assert_eq!(max_macro(-0.0, 0.0).to_bits(), 0.0f32.to_bits());
        assert_eq!(min_macro(0.0, -0.0).to_bits(), (-0.0f32).to_bits());
    }

    #[test]
    fn random_map_for_the_players_not_observing() {
        let mut h = Rec::new();
        // gaia, a human at epoch 5, an observer, the local human at epoch 3, a computer at epoch 1
        h.players = vec![(false, false, 0), (false, true, 5), (true, true, 1), (false, true, 3), (false, false, 1)];
        h.log.clear();
        let mut a = SewAction::new(&mut h, 1, EE_SCREENS_SEW_RANDOM_MAP, 0, 2, 4, 0, 0).unwrap();
        a.run(&mut h);
        assert!(h.log.iter().any(|l| l == "random [0, 1, 3, 4] local 0 style 4 epoch 3 size 2"), "{:?}", h.log);
        assert!(h.log.iter().any(|l| l == "16 p0 [100, 90, 0, 0, 0] [0.0, 0.0, 0.0]"));
        assert!(h.log.iter().any(|l| l == "33 p0 [0, 0, 0, 0, 0] [5.0, 6.0, 0.0]"));
        assert_eq!(h.log.iter().filter(|l| l.starts_with("43 ")).count(), 5);
    }
}
