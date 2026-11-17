//! The render fill over the world's views (`EERenderObject.cpp`'s `FillObjectFields` and `FillUnitFields`, the object
//! plan's Lane R): every read and field write of a filled entry, in the C++'s order, through `ee_sim::world`'s views;
//! what is not a view (the terrain mesh, the graphic objects, the physical model, virtual class members, the render
//! lists' vectors and names) through [`FillHost`]. The C++ keeps the calls into other objects (the media, the ambient
//! counters, the picker, the texture maps) around these functions (docs/progress/obj-lane-r.md).

use std::os::raw::{c_long, c_ulong, c_void};

use ee_base::math::transform::RotationOrder;
use ee_base::math::types::GeTransformation;
use ee_base::math::um;
use ee_sim::world::{ClassView, MapView, PlayerView, StatsView, TileView, UnitView, WorldRead};
use ee_sim::world_field;

use crate::gfx::capture::{object_place, object_scale, sway_fraction};
use crate::gfx::effects::EeGfxColor;
use crate::gfx::walk::{owned_food, waypoint_tint};

const EE_GFX_FILL_RESOURCE_FOOD: u64 = 2;
const EE_GFX_FILL_TECH_NONE: c_long = 100000;
const EE_GFX_FILL_CATEGORY_GATE: u64 = 0x19;
const EE_GFX_FILL_GOAL_ATTACK_UNIT: u8 = 2;
const EE_GFX_FILL_GOAL_ATTACK_GROUND: u8 = 3;
const EE_GFX_FILL_GOAL_CAST_CALAMITY: u8 = 11;
const EE_GFX_FILL_TYPE_PRIEST_TOWER: u64 = 0x2f0;
const EE_GFX_FILL_TYPE_SPACE_TURRET: u64 = 0x302;
const EE_GFX_FILL_TYPE_ORBITAL_SPACE_STATION: u64 = 0x31f;
const EE_GFX_FILL_TYPE_WAYPOINT: u64 = 0x104;
const EE_GFX_FILL_TYPE_PATH_POINT: u64 = 0xdf;

/// `EERenderShadow` (ee/EERenderLists.h), field for field (the C++ static_asserts every offset).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeGfxFillObject {
    pub location: [f32; 3],
    /// `EEGraphicObject*` (the C++ sets it before the fill)
    pub object: u64,
    pub model_scale: f32,
    pub selection_size: f32,
    pub scale: f32,
    pub f1c: f32,
    pub health: f32,
    pub opacity: f32,
    pub fraction: f32,
    pub model_index: c_ulong,
    pub current_frame: c_ulong,
    pub next_frame: c_ulong,
    pub is_fogged: bool,
    pub gap: [u8; 3],
}

/// `EERenderUnit`'s head (ee/EERenderLists.h): every member but the vectors and the name, which follow it (the C++
/// static_asserts every offset).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeGfxFillUnit {
    pub transformation: GeTransformation,
    pub player_color: [f32; 3],
    pub tint_color: [f32; 3],
    /// `EEUnitClass*`, `EEGraphicObject*` twice (the C++ sets `source` before the fill)
    pub info: u64,
    pub source: u64,
    pub source2: u64,
    pub scale: f32,
    pub selection_size: f32,
    pub shadow_scale: f32,
    pub ff4: f32,
    pub ground_z: f32,
    pub opacity: f32,
    pub health: f32,
    pub fraction: f32,
    pub sub_model_offset_scale: f32,
    pub fog_scale: f32,
    pub category: c_long,
    pub unit_class_index: c_ulong,
    pub l118: c_long,
    pub command_type: c_long,
    pub current_research: c_long,
    pub animation: c_long,
    pub grid_size: c_long,
    pub current_frame: c_ulong,
    pub next_frame: c_ulong,
    pub l134: c_long,
    pub unit_id: c_ulong,
    pub player_id: c_long,
    pub area_effect_strength: c_ulong,
    pub can_place: bool,
    pub is_fogged: bool,
    pub is_tinted: bool,
    pub has_sub_model_offset: bool,
    pub second_full_opacity: bool,
}

/// The frame's values the fill reads (`EERenderFrameInfo` and the render globals).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeGfxFillFrame {
    /// `EERenderFrameInfo::mYaw`
    pub yaw: f32,
    /// `gFreeCamera`
    pub free_camera: bool,
    /// the objects' sway clock (`sSwayClock`)
    pub sway_clock: u32,
    /// the current player (`gWorld.GetCurrentPlayer()`), the world player (`gWorld.mPlayers[0]`) and the map
    pub player: u64,
    pub world_player: u64,
    pub map: u64,
    /// `gU4E9230`: the rectangle's picked unit ids
    pub picked: *const c_ulong,
    pub picked_count: usize,
    /// `EERenderFrameInfo::mPickedUnit` (the unit under the pointer, 0: none)
    pub pointed: u64,
    /// `gMusicFollowsBattle`, `gCollisionSphereMode`
    pub music_follows_battle: bool,
    pub collision_spheres: bool,
    /// `gRenderEnvironment.mSeaLevel`, `gSpaceTurretHeight`, `gSpaceStationHeight`
    pub sea_level: f32,
    pub space_turret_height: f32,
    pub space_station_height: f32,
}

/// `FillUnitFields`' answers from the frame: the fighting count it adds to and whether the priest tower's circle is
/// drawn.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeGfxFillUnitResult {
    pub fighting: f32,
    pub circle: bool,
}

/// A point the host answers (`found` false: none).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeGfxFillPoint {
    pub found: bool,
    pub point: [f32; 3],
}

/// What the fill asks of the C++ that is not a view.
pub trait FillHost {
    /// `theMap->mTerrainMesh.GetMeshZ(x, y)`
    fn mesh_z(&mut self, x: f32, y: f32) -> f32;
    /// `inObject->mModels[inIndex]->GetFrameCount()` of an `EEGraphicObject*`
    fn model_frames(&mut self, object: u64, index: u64) -> u64;
    /// `GetIsWaterTile(x, y)`, `GetIsValidTile(x, y)` of the mesh
    fn water_tile(&mut self, x: c_long, y: c_long) -> bool;
    fn valid_tile(&mut self, x: c_long, y: c_long) -> bool;
    /// `theMap->GetGroundPoint(point)`'s z
    fn ground_z(&mut self, point: [f32; 3]) -> f32;
    /// a complex unit's `mPhysicalModel.GetScale()` and `GetFrame()`
    fn physical_scale(&mut self, unit: u64) -> f32;
    fn physical_frame(&mut self, unit: u64) -> c_ulong;
    /// a complex unit's `mColor`
    fn tint(&mut self, unit: u64) -> EeGfxColor;
    /// the gate class's `IsSidePart(unit)`, the class's `IsLandBuilding()`, `GroundLocation(point)`
    fn side_part(&mut self, unit: u64) -> bool;
    fn land_building(&mut self, class: u64) -> bool;
    fn ground_location(&mut self, class: u64, point: [f32; 3]) -> [f32; 3];
    /// the current player's `mSelectedGroup->Contains(unit)`
    fn group_contains(&mut self, player: u64, unit: u64) -> bool;
    /// the turning parts' vectors of the render unit from the complex unit (not a foundation)
    fn parts(&mut self, out: *mut EeGfxFillUnit, unit: u64);
    /// the render unit's health bars: resized (new bars 0) or one set
    fn bars_resize(&mut self, out: *mut EeGfxFillUnit, count: usize);
    fn bars_set(&mut self, out: *mut EeGfxFillUnit, index: usize, value: f32);
    fn bars_clear(&mut self, out: *mut EeGfxFillUnit);
    /// the render unit's name: cleared, or the complex unit's stats' name
    fn name_clear(&mut self, out: *mut EeGfxFillUnit);
    fn name_from_stats(&mut self, out: *mut EeGfxFillUnit, unit: u64);
    /// the render lists: a selected unit, the pointed-at unit; the debug label's unit and its owner's name
    fn select(&mut self, out: *mut EeGfxFillUnit);
    fn point_at(&mut self, out: *mut EeGfxFillUnit);
    fn label(&mut self, out: *mut EeGfxFillUnit, player: u64);
    /// the strength of the first morale area effect on the complex unit (`found` false: none)
    fn morale(&mut self, unit: u64) -> EeGfxFillMorale;
    /// the scaffolding's model: `gWorld.mPlayers[player]->GetUpgrade(class->mScaffoldingClass's type)->mModel`, 0
    /// without a scaffolding class
    fn scaffolding(&mut self, player: c_long, class: u64) -> u64;
    /// `gWorld.GetUnit(id)` (0: none)
    fn unit(&mut self, id: c_long) -> u64;
    /// the carrier's `FindWaypointAtTile(x, y)`: the waypoint's location
    fn waypoint(&mut self, carrier: u64, x: c_long, y: c_long) -> EeGfxFillPoint;
}

/// A morale area effect's strength.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeGfxFillMorale {
    pub found: bool,
    pub strength: c_ulong,
}

/// The C++'s [`FillHost`]: its context and the calls.
#[repr(C)]
pub struct EeGfxFillHost {
    pub context: *mut c_void,
    pub mesh_z: extern "C" fn(*mut c_void, f32, f32) -> f32,
    pub model_frames: extern "C" fn(*mut c_void, u64, u64) -> u64,
    pub water_tile: extern "C" fn(*mut c_void, c_long, c_long) -> bool,
    pub valid_tile: extern "C" fn(*mut c_void, c_long, c_long) -> bool,
    pub ground_z: extern "C" fn(*mut c_void, f32, f32, f32) -> f32,
    pub physical_scale: extern "C" fn(*mut c_void, u64) -> f32,
    pub physical_frame: extern "C" fn(*mut c_void, u64) -> c_ulong,
    pub tint: extern "C" fn(*mut c_void, u64) -> EeGfxColor,
    pub side_part: extern "C" fn(*mut c_void, u64) -> bool,
    pub land_building: extern "C" fn(*mut c_void, u64) -> bool,
    pub ground_location: extern "C" fn(*mut c_void, u64, *mut f32),
    pub group_contains: extern "C" fn(*mut c_void, u64, u64) -> bool,
    pub parts: extern "C" fn(*mut c_void, *mut EeGfxFillUnit, u64),
    pub bars_resize: extern "C" fn(*mut c_void, *mut EeGfxFillUnit, usize),
    pub bars_set: extern "C" fn(*mut c_void, *mut EeGfxFillUnit, usize, f32),
    pub bars_clear: extern "C" fn(*mut c_void, *mut EeGfxFillUnit),
    pub name_clear: extern "C" fn(*mut c_void, *mut EeGfxFillUnit),
    pub name_from_stats: extern "C" fn(*mut c_void, *mut EeGfxFillUnit, u64),
    pub select: extern "C" fn(*mut c_void, *mut EeGfxFillUnit),
    pub point_at: extern "C" fn(*mut c_void, *mut EeGfxFillUnit),
    pub label: extern "C" fn(*mut c_void, *mut EeGfxFillUnit, u64),
    pub morale: extern "C" fn(*mut c_void, u64) -> EeGfxFillMorale,
    pub scaffolding: extern "C" fn(*mut c_void, c_long, u64) -> u64,
    pub unit: extern "C" fn(*mut c_void, c_long) -> u64,
    pub waypoint: extern "C" fn(*mut c_void, u64, c_long, c_long) -> EeGfxFillPoint,
}

impl FillHost for &EeGfxFillHost {
    fn mesh_z(&mut self, x: f32, y: f32) -> f32 {
        (self.mesh_z)(self.context, x, y)
    }
    fn model_frames(&mut self, object: u64, index: u64) -> u64 {
        (self.model_frames)(self.context, object, index)
    }
    fn water_tile(&mut self, x: c_long, y: c_long) -> bool {
        (self.water_tile)(self.context, x, y)
    }
    fn valid_tile(&mut self, x: c_long, y: c_long) -> bool {
        (self.valid_tile)(self.context, x, y)
    }
    fn ground_z(&mut self, p: [f32; 3]) -> f32 {
        (self.ground_z)(self.context, p[0], p[1], p[2])
    }
    fn physical_scale(&mut self, unit: u64) -> f32 {
        (self.physical_scale)(self.context, unit)
    }
    fn physical_frame(&mut self, unit: u64) -> c_ulong {
        (self.physical_frame)(self.context, unit)
    }
    fn tint(&mut self, unit: u64) -> EeGfxColor {
        (self.tint)(self.context, unit)
    }
    fn side_part(&mut self, unit: u64) -> bool {
        (self.side_part)(self.context, unit)
    }
    fn land_building(&mut self, class: u64) -> bool {
        (self.land_building)(self.context, class)
    }
    fn ground_location(&mut self, class: u64, point: [f32; 3]) -> [f32; 3] {
        let mut p = point;
        (self.ground_location)(self.context, class, p.as_mut_ptr());
        p
    }
    fn group_contains(&mut self, player: u64, unit: u64) -> bool {
        (self.group_contains)(self.context, player, unit)
    }
    fn parts(&mut self, out: *mut EeGfxFillUnit, unit: u64) {
        (self.parts)(self.context, out, unit)
    }
    fn bars_resize(&mut self, out: *mut EeGfxFillUnit, count: usize) {
        (self.bars_resize)(self.context, out, count)
    }
    fn bars_set(&mut self, out: *mut EeGfxFillUnit, index: usize, value: f32) {
        (self.bars_set)(self.context, out, index, value)
    }
    fn bars_clear(&mut self, out: *mut EeGfxFillUnit) {
        (self.bars_clear)(self.context, out)
    }
    fn name_clear(&mut self, out: *mut EeGfxFillUnit) {
        (self.name_clear)(self.context, out)
    }
    fn name_from_stats(&mut self, out: *mut EeGfxFillUnit, unit: u64) {
        (self.name_from_stats)(self.context, out, unit)
    }
    fn select(&mut self, out: *mut EeGfxFillUnit) {
        (self.select)(self.context, out)
    }
    fn point_at(&mut self, out: *mut EeGfxFillUnit) {
        (self.point_at)(self.context, out)
    }
    fn label(&mut self, out: *mut EeGfxFillUnit, player: u64) {
        (self.label)(self.context, out, player)
    }
    fn morale(&mut self, unit: u64) -> EeGfxFillMorale {
        (self.morale)(self.context, unit)
    }
    fn scaffolding(&mut self, player: c_long, class: u64) -> u64 {
        (self.scaffolding)(self.context, player, class)
    }
    fn unit(&mut self, id: c_long) -> u64 {
        (self.unit)(self.context, id)
    }
    fn waypoint(&mut self, carrier: u64, x: c_long, y: c_long) -> EeGfxFillPoint {
        (self.waypoint)(self.context, carrier, x, y)
    }
}

/// `GetTileHasTree`: whether a unit on the tile (none off the map) is of a tree class.
pub fn tile_has_tree(tile: Option<TileView<'_>>) -> bool {
    let mut result = false;
    if let Some(tile) = tile {
        for unit in tile.units() {
            if result {
                break;
            }
            // a node of no unit: the C++ would read through NULL; the game's nodes always hold a unit
            result = unit.and_then(|u| u.class()).is_some_and(|c| c.tree());
        }
    }
    result
}

fn picked(frame: &EeGfxFillFrame, id: u32) -> bool {
    if frame.picked.is_null() || frame.picked_count == 0 {
        return false;
    }
    // SAFETY: the C++ passes its vector's elements
    let ids = unsafe { std::slice::from_raw_parts(frame.picked, frame.picked_count) };
    ids.iter().any(|&p| p as u64 == id as u64)
}

fn tile_size(class: ClassView<'_>) -> c_long {
    world_field!(class.fields(), mTileSize)
}

fn max_hit_points(stats: StatsView<'_>) -> c_long {
    world_field!(stats.fields(), mMaxHitPoints)
}

fn stats_power(stats: StatsView<'_>) -> c_long {
    world_field!(stats.fields(), mPower)
}

/// `(float)a / b` of two C++ longs (the division in float)
fn ratio(a: c_long, b: c_long) -> f32 {
    a as f32 / b as f32
}

/// `FillObjectFields`: a map object's entry (its place in its tiles, its health, size jitter, shadow, selection ring
/// and sway) and the picker's transformation; returns whether the object is selected (the viewed unit, or in the free
/// camera an editor-selected or rectangle-picked one). `out.object` is set.
pub fn fill_object_fields<H: FillHost>(
    t: &WorldRead,
    unit: UnitView<'_>,
    out: &mut EeGfxFillObject,
    frame: &EeGfxFillFrame,
    transformation: &mut GeTransformation,
    host: &mut H,
) -> bool {
    let mut selected = false;
    // SAFETY: the C++ passes the live current player and map
    let player = unsafe { PlayerView::of(t, frame.player) }.expect("a current player");
    let map = unsafe { MapView::of(t, frame.map) }.expect("a map");
    let id = unit.id();
    let class = unit.class().expect("a unit's class");
    let size = tile_size(class);
    let stats = unit.stats().expect("a unit's stats");
    let (tile_x, tile_y) = unit.tile_xy();
    let (x, y) = object_place(id as i32, size as i32, tile_x as i32, tile_y as i32);
    out.location[0] = x;
    out.location[1] = y;
    out.location[2] = host.mesh_z(out.location[0], out.location[1]);
    transformation.set_orientation(RotationOrder::Ypr, frame.yaw, 0.0, 0.0);
    transformation.m[0][3] = out.location[0];
    transformation.m[1][3] = out.location[1];
    transformation.m[2][3] = out.location[2];
    transformation.m[3] = [0.0, 0.0, 0.0, 1.0];
    out.health = ratio(unit.hit_points() as c_long, max_hit_points(stats));
    out.is_fogged = false;
    out.model_scale = stats.scale();
    out.model_scale *= object_scale(id as i32);
    if !tile_has_tree(map.tile(tile_x - 1, tile_y - 1)) || !tile_has_tree(map.tile(tile_x + 1, tile_y + 1)) {
        out.scale = world_field!(class.fields(), mShadowScale);
        out.f1c = world_field!(class.fields(), mL170);
    } else {
        out.scale = 0.0;
        out.f1c = 0.0;
    }
    let editor_selected: bool = world_field!(unit.rts(), mIsEditorSelected);
    if unit.handle() == player.viewed_unit_handle() || frame.free_camera && (editor_selected || picked(frame, id)) {
        out.selection_size = world_field!(class.fields(), mSelectionSize);
        selected = true;
    }
    out.model_index = 0;
    out.current_frame = 0;
    out.next_frame = out.current_frame;
    if host.model_frames(out.object, out.model_index as u64) > 1 {
        out.current_frame += 1;
        out.fraction = sway_fraction(frame.sway_clock, id as i32);
    } else {
        out.fraction = 0.0;
    }
    selected
}

/// The complex unit's cargo's bars, the unit's own bar and its power's (a selected or pointed-at complex unit).
fn bars<H: FillHost>(
    unit: UnitView<'_>,
    complex: UnitView<'_>,
    current: PlayerView<'_>,
    out: *mut EeGfxFillUnit,
    head: &mut EeGfxFillUnit,
    host: &mut H,
) {
    let owner = complex.player().expect("a unit's owner");
    let cargo = complex.cargo_len();
    if cargo > 0 && (owner.handle() == current.handle() || owner.enemy(current.id() as usize) == 0) {
        host.bars_resize(out, cargo + 1);
        for j in 0..cargo {
            let c = complex.cargo(j).expect("a cargo unit");
            let max = max_hit_points(c.stats().expect("a cargo unit's stats"));
            host.bars_set(out, j + 1, ratio(c.hit_points() as c_long, max));
        }
    }
    let stats = unit.stats().expect("a unit's stats");
    host.bars_set(out, 0, ratio(unit.hit_points() as c_long, max_hit_points(stats)));
    let power = stats_power(stats);
    head.health = if power == 0 { -1.0 } else { ratio(complex.power() as c_long, power) };
}

/// `FillUnitFields`: a unit's render entry (its class's fields, owner and fog; a complex unit's scale, tint, gate
/// offset, turning parts, bars, name, frames, orientation, command, morale ring, scaffolding and build height; a
/// placed object's place on its footprint or its carrier's waypoint) and its ground height. `out.source` is set. The
/// result's `circle`: the priest tower's range circle is drawn; `fighting`: what it adds to the frame's count.
pub fn fill_unit_fields<H: FillHost>(
    t: &WorldRead,
    unit: UnitView<'_>,
    out: *mut EeGfxFillUnit,
    frame: &EeGfxFillFrame,
    host: &mut H,
) -> EeGfxFillUnitResult {
    let mut result = EeGfxFillUnitResult::default();
    // SAFETY: the C++ passes the render unit, the live current player
    let head = unsafe { &mut *out };
    let current = unsafe { PlayerView::of(t, frame.player) }.expect("a current player");
    let tile = unit.tile();
    let complex = unit.complex_unit();
    let class = unit.class().expect("a unit's class");
    let stats = unit.stats().expect("a unit's stats");
    let owner = unit.player().expect("a unit's owner");
    let (tile_x, tile_y) = unit.tile_xy();
    let category = class.category();
    let food = unit.resource_type() == EE_GFX_FILL_RESOURCE_FOOD && unit.resource_amount() > 0;
    let fogged = owned_food(food, complex.is_some(), category as c_ulong as i32, owner.id() as i32);
    head.info = class.handle();
    head.grid_size = tile_size(class);
    head.player_color = owner.color();
    head.category = category as c_long;
    head.unit_class_index = class.unit_type() as c_ulong;
    head.l118 = class.family() as c_long;
    let alive = unit.hit_points() > 0;
    head.shadow_scale = if alive { world_field!(class.fields(), mShadowScale) } else { 0.0 };
    head.ff4 = if alive { world_field!(class.fields(), mL170) } else { 0.0 };
    head.selection_size = world_field!(class.fields(), mSelectionSize);
    head.player_id = owner.id() as c_long;
    head.current_research = EE_GFX_FILL_TECH_NONE;
    head.unit_id = unit.id() as c_ulong;
    head.source2 = 0;
    head.is_tinted = false;
    head.has_sub_model_offset = false;
    host.name_clear(out);
    head.area_effect_strength = 0;
    if !frame.free_camera && fogged {
        let tile = tile.expect("a unit's tile");
        head.is_fogged = !(tile.visibility(current.id() as usize) > 1 || current.sees_all());
    } else {
        head.is_fogged = false;
    }
    let editor_selected: bool = world_field!(unit.rts(), mIsEditorSelected);
    if let Some(c) = complex {
        let ch = c.handle();
        let c_class = c.class().expect("a unit's class");
        head.scale = host.physical_scale(ch);
        head.is_tinted = world_field!(c.complex(), mIsTinted);
        if head.is_tinted {
            let tint = host.tint(ch);
            head.tint_color = [tint.red, tint.green, tint.blue];
            head.fog_scale = 0.6;
        }
        if c_class.category() == EE_GFX_FILL_CATEGORY_GATE && c.hit_points() > 0 && host.side_part(ch) {
            head.sub_model_offset_scale = world_field!(c.complex(), mOpenFraction);
            head.has_sub_model_offset = true;
        }
        if frame.music_follows_battle
            && c.hit_points() != 0
            && c.active_goal().is_some_and(|(kind, _)| {
                kind == EE_GFX_FILL_GOAL_ATTACK_UNIT || kind == EE_GFX_FILL_GOAL_ATTACK_GROUND || kind == EE_GFX_FILL_GOAL_CAST_CALAMITY
            })
        {
            result.fighting += 1.0;
        }
        if c_class.unit_type() == EE_GFX_FILL_TYPE_PRIEST_TOWER && c.is_built() && host.group_contains(current.handle(), ch) {
            result.circle = true;
        }
        let foundation: bool = world_field!(c.complex(), mIsFoundation);
        if !foundation {
            host.parts(out, ch);
        }
        let selected = (!frame.free_camera
            && (host.group_contains(current.handle(), ch) || current.selected_viewed_unit_handle() == unit.handle()))
            || (frame.free_camera && editor_selected)
            || picked(frame, unit.id());
        if selected {
            if !host.land_building(class.handle()) || unit.hit_points() != 0 {
                head.selection_size = world_field!(class.fields(), mSelectionSize);
                bars(unit, c, current, out, head, host);
                host.select(out);
            }
        } else if frame.pointed == unit.handle() {
            host.name_from_stats(out, ch);
            bars(unit, c, current, out, head, host);
            host.point_at(out);
            let gaia = frame.world_player;
            if owner.handle() != gaia && owner.handle() != current.handle() {
                host.label(out, owner.handle());
            }
        } else if frame.collision_spheres {
            host.name_from_stats(out, ch);
        }
        let research: c_long = world_field!(c.complex(), mCurrentResearch);
        let production: c_long = world_field!(c.complex(), mProductionTime);
        if research != EE_GFX_FILL_TECH_NONE && production > 0 {
            head.current_research = research;
        }
        if !foundation {
            head.current_frame = host.physical_frame(ch);
            let next: c_long = world_field!(c.complex(), mNextFrame);
            head.next_frame = next as c_ulong;
            head.fraction = world_field!(c.complex(), mAnimationFraction);
        } else {
            head.current_frame = 0;
            head.next_frame = 0;
            head.fraction = 0.0;
        }
        head.transformation = GeTransformation { m: c.transform() };
        let faces: bool = world_field!(c_class.fields(), mFacesCamera);
        if faces {
            // Apply(up (0, 1, 0)): the vector's z, as GETransformation::Apply sums it
            let m = &head.transformation.m;
            let forward_z = 0.0 * m[2][0] + 0.0 * m[2][2] + 1.0 * m[2][1];
            head.transformation.set_orientation(
                RotationOrder::Ypr,
                frame.yaw,
                um::um_arccos(-forward_z) - 1.5707964,
                0.0,
            );
        }
        head.command_type = c.command_type() as c_long;
        let animation: c_ulong = world_field!(c.complex(), mAnimation);
        head.animation = animation as c_long;
        if unit.hit_points() > 0 {
            let morale = host.morale(ch);
            if morale.found {
                head.area_effect_strength = morale.strength;
            }
        }
        if !c.is_built() && !foundation {
            head.source2 = host.scaffolding(head.player_id, head.info);
            let z = head.transformation.m[2][3];
            head.transformation.m[2][3] = 0.0;
            head.transformation.scale(1.0, 1.0, ratio(unit.hit_points() as c_long, max_hit_points(stats)));
            head.transformation.m[2][3] = z;
        } else if foundation {
            if c_class.unit_type() == EE_GFX_FILL_TYPE_SPACE_TURRET {
                head.transformation.m[2][3] -= frame.space_turret_height;
            } else if c_class.unit_type() == EE_GFX_FILL_TYPE_ORBITAL_SPACE_STATION {
                head.transformation.m[2][3] -= frame.space_station_height;
            }
        }
    } else {
        head.scale = stats.scale();
        if unit.handle() == current.selected_viewed_unit_handle()
            || (frame.free_camera && editor_selected)
            || picked(frame, unit.id())
        {
            head.health = -1.0;
            host.bars_clear(out);
            host.select(out);
        }
        let mut point = [
            head.grid_size as f32 * 0.5 + tile_x as c_long as f32,
            head.grid_size as f32 * 0.5 + tile_y as c_long as f32,
            0.0,
        ];
        point[2] = host.mesh_z(point[0], point[1]);
        let path_marker = class.unit_type() == EE_GFX_FILL_TYPE_WAYPOINT || class.unit_type() == EE_GFX_FILL_TYPE_PATH_POINT;
        let carrier_id = unit.hit_points() as c_long;
        if path_marker && host.unit(carrier_id) != 0 {
            // SAFETY: the C++'s live unit
            let carrier = unsafe { UnitView::of(t, host.unit(carrier_id)) }.and_then(|u| u.complex_unit());
            if let Some(carrier) = carrier {
                let waypoint = host.waypoint(carrier.handle(), tile_x as c_long, tile_y as c_long);
                if waypoint.found {
                    point = waypoint.point;
                    if host.valid_tile(um_floor(point[0]), um_floor(point[1])) {
                        point[2] = host.mesh_z(point[0], point[1]);
                    }
                    let tint = waypoint_tint(carrier.id() as i32);
                    head.tint_color = [tint.red, tint.green, tint.blue];
                }
            }
        }
        point = host.ground_location(class.handle(), point);
        if path_marker && tile.expect("a unit's tile").height() < frame.sea_level {
            point[2] = frame.sea_level;
        }
        let faces: bool = world_field!(class.fields(), mFacesCamera);
        head.transformation.set_orientation(RotationOrder::Ypr, if faces { frame.yaw } else { 0.0 }, 0.0, 0.0);
        head.transformation.m[0][3] = point[0];
        head.transformation.m[1][3] = point[1];
        head.transformation.m[2][3] = point[2];
        head.current_frame = 0;
        head.next_frame = 0;
        head.fraction = 0.0;
        head.command_type = 0;
        head.animation = 0;
    }
    let m = head.transformation.m;
    if host.water_tile(tile_x as c_long, tile_y as c_long) && m[2][3] >= 0.0 {
        head.ground_z = 0.0;
    } else {
        head.ground_z = host.ground_z([m[0][3], m[1][3], m[2][3]]);
    }
    result
}

/// `UMFloor`: rounded to nearest (as fld/fistp), then down when above.
fn um_floor(value: f32) -> c_long {
    let mut r = value.round_ties_even() as c_long;
    if r as f32 > value {
        r -= 1;
    }
    r
}

/// `FillObjectFields` (EERenderObject.cpp) through Rust.
///
/// # Safety
/// The caller holds the world (the graphics thread under the world's read lock); `unit` is a live unit, the pointers
/// are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_fill_object_fields(
    unit: u64,
    out: *mut EeGfxFillObject,
    frame: *const EeGfxFillFrame,
    transformation: *mut GeTransformation,
    host: *const EeGfxFillHost,
) -> bool {
    // SAFETY: the function's contract
    unsafe {
        let t = WorldRead::begin();
        let unit = UnitView::of(&t, unit).expect("a unit");
        let mut host = &*host;
        fill_object_fields(&t, unit, &mut *out, &*frame, &mut *transformation, &mut host)
    }
}

/// `FillUnitFields` (EERenderObject.cpp) through Rust.
///
/// # Safety
/// As [`ee_gfx_fill_object_fields`]; `out` is the head of an `EERenderUnit`.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_fill_unit_fields(
    unit: u64,
    out: *mut EeGfxFillUnit,
    frame: *const EeGfxFillFrame,
    host: *const EeGfxFillHost,
) -> EeGfxFillUnitResult {
    // SAFETY: the function's contract
    unsafe {
        let t = WorldRead::begin();
        let unit = UnitView::of(&t, unit).expect("a unit");
        let mut host = &*host;
        fill_unit_fields(&t, unit, out, &*frame, &mut host)
    }
}
