//! The round-2 unit utilities' tests (uu_world.rs) over a small world of their own: the units' frames and the
//! writes the bodies make, recorded in order.

#![allow(clippy::unnecessary_cast)]
use std::collections::BTreeMap;

use ee_render::ge::types::GeTransformation;

use crate::units::unit_class_fields::EeUnitsUnitClassFields;
use crate::units::unit_fields::{EeUnitsComplexUnitFields, EeUnitsRtsUnitFields, EeUnitsUnitFields};
use crate::units::uu::*;
use crate::units::uu_tiles::Tile;
use crate::units::uu_world::k2::*;
use crate::units::uu_world::*;

struct Unit {
    rts: Box<EeUnitsRtsUnitFields>,
    unit: Box<EeUnitsUnitFields>,
    cx: Box<EeUnitsComplexUnitFields>,
    class: Box<EeUnitsUnitClassFields>,
    player: u64,
    location: [f32; 3],
    command: i32,
    goal: i32,
}

/// A map of `w` x `h` flat tiles at height 0, the units on it, and every write the bodies make (`acts`).
#[derive(Default)]
struct World {
    tiles: BTreeMap<(i32, i32), Vec<u64>>,
    heights: BTreeMap<(i32, i32), f32>,
    units: Vec<Unit>,
    acts: Vec<String>,
    near: Vec<u64>,
    group: Vec<u64>,
    path: Option<Tile>,
    sight: f32,
}

fn h_of(obj: u64) -> usize {
    (obj / 0x100 - 1) as usize
}

impl World {
    fn new(w: i32, h: i32) -> World {
        let mut m = World { sight: 4.0, ..Default::default() };
        for x in 0..w {
            for y in 0..h {
                m.tiles.insert((x, y), Vec::new());
            }
        }
        m
    }
    fn add(&mut self, x: i32, y: i32, unit_type: u32, player: u64, f: impl FnOnce(&mut EeUnitsUnitClassFields)) -> u64 {
        let mut class = EeUnitsUnitClassFields { mUnitType: unit_type as _, mTileSize: 1, ..Default::default() };
        class.mInnerCollisionSphere = 0.5;
        f(&mut class);
        let id = self.units.len() as u64 + 1;
        self.units.push(Unit {
            rts: Box::new(EeUnitsRtsUnitFields {
                mTileX: x as _,
                mTileY: y as _,
                mUnitID: (id * 10) as _,
                mIsValid: true,
                ..Default::default()
            }),
            unit: Box::new(EeUnitsUnitFields { mHitPoints: 10, ..Default::default() }),
            cx: Box::default(),
            class: Box::new(class),
            player,
            location: [x as f32 + 0.5, y as f32 + 0.5, 0.0],
            command: UU_ACTION_IDLE,
            goal: -1,
        });
        let obj = id * 0x100;
        if let Some(t) = self.tiles.get_mut(&(x, y)) {
            t.push(obj);
        }
        obj
    }
    fn u(&self, obj: u64) -> &Unit {
        &self.units[h_of(obj)]
    }
}

fn tile_handle(x: i32, y: i32) -> u64 {
    0x1_0000_0000 | ((x as u32 as u64) << 16) | (y as u32 as u64)
}

fn tile_of(tile: u64) -> (i32, i32) {
    (((tile >> 16) & 0xffff) as u16 as i16 as i32, (tile & 0xffff) as u16 as i16 as i32)
}

impl UuHost for World {
    fn map(&mut self) -> u64 {
        7
    }
    fn map_tile(&mut self, _map: u64, x: i32, y: i32) -> u64 {
        if self.tiles.contains_key(&(x, y)) {
            tile_handle(x, y)
        } else {
            0
        }
    }
    fn tile_terrain(&mut self, _tile: u64) -> i32 {
        0
    }
    fn tile_height(&mut self, tile: u64) -> f32 {
        self.heights.get(&tile_of(tile)).copied().unwrap_or(0.0)
    }
    fn tile_units(&mut self, tile: u64, out: &mut Vec<u64>) {
        *out = self.tiles[&tile_of(tile)].clone();
    }
    fn is_edge_tile(&mut self, _x: i32, _y: i32) -> bool {
        false
    }
    fn raised_corners(&mut self, _map: u64, _x: i32, _y: i32) -> i32 {
        0
    }
    fn mesh_z(&mut self, _map: u64, x: i32, y: i32) -> f32 {
        (x + y) as f32 * 0.1
    }
    fn mesh_ground(&mut self, _x: f32, _y: f32) -> f32 {
        0.05
    }
    fn unit(&mut self, obj: u64) -> EeUnitsUuUnit {
        if obj == 0 {
            return EeUnitsUuUnit::default();
        }
        let u = self.u(obj);
        EeUnitsUuUnit {
            obj,
            rts: &*u.rts,
            unit: &*u.unit,
            cx: &*u.cx,
            class_fields: &*u.class,
            class_obj: obj + 1,
            complex: obj,
            player: u.player,
            tile: tile_handle(u.rts.mTileX as i32, u.rts.mTileY as i32),
            player_id: u.player as i32,
            on_map: 1,
            location: u.location,
            has_stats: 1,
            command: u.command,
            ..Default::default()
        }
    }
    fn class_of_type(&mut self, unit_type: i32) -> EeUnitsUuClass {
        match self.units.iter().position(|u| u.class.mUnitType as i32 == unit_type) {
            Some(i) => {
                EeUnitsUuClass { obj: (i as u64 + 1) * 0x100 + 1, fields: &*self.units[i].class, ..Default::default() }
            }
            None => EeUnitsUuClass::default(),
        }
    }
    fn class(&mut self, class_obj: u64) -> EeUnitsUuClass {
        EeUnitsUuClass { obj: class_obj, fields: &*self.u(class_obj - 1).class, ..Default::default() }
    }
    fn position(&mut self, obj: u64) -> [f32; 3] {
        self.u(obj).location
    }
    fn passable_by(&mut self, _obj: u64, _by: u64) -> bool {
        false
    }
    fn is_foundation(&mut self, _obj: u64) -> bool {
        false
    }
    fn can_move_on_terrain(&mut self, _class: u64, unit: u64, terrain: i32) -> bool {
        // the even units move on terrain 1
        terrain == 1 && h_of(unit).is_multiple_of(2)
    }
    fn passable_flags(&mut self, _class: u64, _unit: u64) -> i32 {
        1
    }
    fn surrounding_tiles(&mut self, _target: u64, _virtual_call: bool) -> Vec<Tile> {
        Vec::new()
    }
    fn container_continent(&mut self, _container: u64, _unit: u64) -> u64 {
        1
    }
    fn continent_id(&mut self, _x: i32, _y: i32) -> u16 {
        1
    }
    fn continent_by_id(&mut self, id: u16) -> u64 {
        u64::from(id)
    }
    fn continent_at(&mut self, _x: i32, _y: i32) -> u64 {
        1
    }
    fn continent_number(&mut self, continent: u64) -> u16 {
        continent as u16
    }
    fn are_connected(&mut self, a: u64, b: u64, _flags: i32) -> bool {
        a == b
    }
    fn are_connected_for_unit(&mut self, _unit: u64, a: u64, b: u64) -> bool {
        a == b
    }
    fn continent_tile_flag(&mut self, _x: i32, _y: i32) -> i32 {
        0
    }
    fn continent_link(&mut self, _row: u16, _column: u16) -> bool {
        false
    }
    fn line_blocked(&mut self, _x1: i32, _y1: i32, _x2: i32, _y2: i32) -> bool {
        false
    }
    fn tile_line(&mut self, x1: i32, y1: i32, x2: i32, _y2: i32, _ends: bool, _thin: bool) -> Vec<Tile> {
        self.acts.push(format!("line {x1} {y1} {x2}"));
        // a horizontal line without its ends
        let step = if x2 >= x1 { 1 } else { -1 };
        let mut out = Vec::new();
        let mut x = x1 + step;
        while x != x2 {
            out.push([x, y1]);
            x += step;
        }
        out
    }
    fn reserved(&mut self, _group: u64, _point: [f32; 3]) -> bool {
        false
    }
    fn ground_point(&mut self, point: [f32; 3]) -> [f32; 3] {
        point
    }
    fn terrain_tilt(&mut self, _m: &GeTransformation, _roll: bool) -> f32 {
        0.0
    }
    fn enemy(&mut self, player: u64, other_id: i32) -> bool {
        player != other_id as u64
    }
    fn is_human(&mut self, _player: u64) -> bool {
        false
    }
    fn unit_set_contains(&mut self, _unit_type: u32, _set: i32) -> bool {
        false
    }
    fn player_units(&mut self, _index: i32) -> Vec<u64> {
        Vec::new()
    }
    fn part(&mut self, _complex: u64, _next: bool) -> u64 {
        0
    }
    fn facing_quarter(&mut self, _x: i32, _y: i32) -> i32 {
        0
    }
    fn resource_users(&mut self, _unit: u64) -> i32 {
        0
    }
    fn random_long(&mut self, min: i32, _max: i32) -> i32 {
        min
    }
    fn random_float(&mut self, min: f32, _max: f32) -> f32 {
        min
    }
    fn location(&mut self, complex: u64) -> GeTransformation {
        let mut m = GeTransformation::IDENTITY;
        let l = self.u(complex).location;
        m.m[0][3] = l[0];
        m.m[1][3] = l[1];
        m.m[2][3] = l[2];
        m
    }
    fn set_animation(&mut self, complex: u64, animation: i32) {
        self.acts.push(format!("animation {} {animation}", h_of(complex)));
    }
    fn set_transformation(&mut self, complex: u64, _m: &GeTransformation) {
        self.acts.push(format!("frame {}", h_of(complex)));
    }
    fn is_tile_flat(&mut self, _x: i32, _y: i32) -> bool {
        true
    }
    fn tile_visible_to(&mut self, tile: u64, _player: u64) -> bool {
        tile_of(tile).0 >= 3
    }
    fn group_units(&mut self, _player: u64) -> i32 {
        0
    }
    fn group_continent(&mut self, _player: u64) -> u16 {
        1
    }
    fn citizen_passability(&mut self, _player: u64) -> i32 {
        1
    }
    fn saves_visibility(&mut self, _player: u64) -> bool {
        false
    }
    fn continent_category(&mut self, _continent: u64) -> u8 {
        1
    }
    fn are_connected_by_id(&mut self, a: u16, b: u16, _flags: i32) -> bool {
        a == b
    }
    fn regions_size(&mut self) -> i32 {
        4
    }
    fn regions_len(&mut self, _x: i32, _y: i32) -> i32 {
        0
    }
    fn goal_kind(&mut self, unit: u64) -> i32 {
        self.u(unit).goal
    }
    fn stats_range(&mut self, _unit: u64) -> f32 {
        0.0
    }
    fn center_offset(&mut self, _complex: u64, x: f32, y: f32) -> (f32, f32) {
        (x, y)
    }
    fn mesh_tile_is(&mut self, _which: i32, x: u32, y: u32) -> bool {
        self.tiles.contains_key(&(x as i32, y as i32))
    }
    fn shore_tile(&mut self, _x: i32, _y: i32) -> bool {
        false
    }
    fn continent_of(&mut self, _x: i32, _y: i32, _kind: i32) -> u16 {
        1
    }
    fn footprint_continent(&mut self, _complex: u64, _kind: i32) -> u16 {
        1
    }
    fn in_same_area(&mut self, _tiles: &[Tile], _kind: i32) -> bool {
        true
    }
    fn has_shore_tile(&mut self, _tiles: &[Tile]) -> bool {
        false
    }
    fn drop_point(&mut self, target: u64) -> [f32; 3] {
        let l = self.u(target).location;
        [l[0], l[1] + 1.0, l[2]]
    }
    fn part_left_out(&mut self, _piece: u64) -> bool {
        false
    }
    fn path_area_id(&mut self, _x: i32, _y: i32) -> u16 {
        0
    }
    fn gate_ends(&mut self, _gate: u64) -> [[f32; 3]; 2] {
        [[0.0; 3]; 2]
    }
}

impl UuWorld for World {
    fn line_of_sight(&mut self, _complex: u64) -> f32 {
        self.sight
    }
    fn units_near(&mut self, _center: [f32; 3], _radius: f32) -> Vec<u64> {
        self.near.clone()
    }
    fn gather_reach(&mut self, distance: f32, _unit: u64, _target: u64) -> bool {
        self.acts.push(format!("reach {distance}"));
        distance <= 2.0
    }
    fn tile_unit(&mut self, tile: u64, unit: u64, add: bool) {
        let (x, y) = tile_of(tile);
        self.acts.push(format!("{} {} {x} {y}", if add { "add" } else { "remove" }, h_of(unit)));
    }
    fn clear_ambient(&mut self, unit: u64) {
        self.acts.push(format!("clear {}", h_of(unit)));
    }
    fn regions_footprint(&mut self, x: i32, y: i32, size: i32, complex: u64, add: bool) {
        self.acts.push(format!("regions {x} {y} {size} {} {add}", h_of(complex)));
    }
    fn loading_saved_game(&mut self) -> bool {
        false
    }
    fn step_aside(&mut self, complex: u64, _player: u64, _class: u64, x: i32, y: i32, force: bool) {
        self.acts.push(format!("aside {} {x} {y} {force}", h_of(complex)));
    }
    fn group_len(&mut self, _group: u64) -> i32 {
        self.group.len() as i32
    }
    fn group_unit(&mut self, _group: u64, i: i32) -> u64 {
        self.group[i as usize]
    }
    fn add_build_target(&mut self, unit: u64, piece: u64) {
        self.acts.push(format!("build {} {}", h_of(unit), h_of(piece)));
    }
    fn restart_idle(&mut self, unit: u64) {
        self.acts.push(format!("restart {}", h_of(unit)));
    }
    fn can_produce(&mut self, _player: u64, _class: u64) -> bool {
        true
    }
    fn make_wall_piece(&mut self, _class: u64, player: u64, x: i32, y: i32, corner: i32, _foundation: bool) -> u64 {
        self.acts.push(format!("piece {x} {y} {corner}"));
        self.add(x, y, 0x60, player, |c| c.mIsWall = true)
    }
    fn wall_rise(&mut self, _wall: u64) -> f32 {
        2.0
    }
    fn map_size(&mut self) -> i32 {
        16
    }
    fn find_path(&mut self, _unit: u64, from: Tile, to: Tile, b: [i32; 4], limit: u32, _walls: bool) -> Option<Tile> {
        self.acts.push(format!("path {from:?} {to:?} {b:?} {limit}"));
        self.path
    }
    fn can_take(&mut self, _container: u64, _unit: u64, kind: u32) {
        self.acts.push(format!("take {kind}"));
    }
    fn deposit_factor(&mut self) -> f32 {
        1.5
    }
    fn last_resource(&mut self, unit: u64) -> u64 {
        // the unit's site: the unit after it
        unit + 0x100
    }
    fn distance_squared_to_unit(&mut self, _complex: u64, _other: u64) -> f32 {
        4.0
    }
    fn deposit_effect(&mut self, unit: u64) {
        self.acts.push(format!("effect {}", h_of(unit)));
    }
    fn add_resource(&mut self, _container: u64, _unit: u64, amount: i32, carried: i32, near: bool, bonus: f32) {
        self.acts.push(format!("resource {amount} {carried} {near} {bonus}"));
    }
    fn statistic(&mut self, _container: u64, series: i32, amount: i32) {
        self.acts.push(format!("statistic {series} {amount}"));
    }
    fn editor_transport_takes(&mut self, _player: u64, unit_type: i32) -> bool {
        unit_type == 0x70
    }
    fn region_units(&mut self, x: i32, y: i32) -> Vec<u64> {
        // regions of 4 tiles: the units whose tile is in the region
        (0..self.units.len())
            .filter(|&i| self.units[i].rts.mTileX as i32 / 4 == x && self.units[i].rts.mTileY as i32 / 4 == y)
            .map(|i| (i as u64 + 1) * 0x100)
            .collect()
    }
}

#[test]
fn a_submarine_dives_to_its_depth_above_the_seabed() {
    let mut w = World::new(8, 8);
    let sub = w.add(2, 3, 9, 1, |c| c.mCruiseAltitude = 1.5);
    // its disc of tiles (0.5 + 1 around (2.5, 3.5)); the highest mesh point under them
    let ground = seabed_under(&mut w, sub, 2.5, 3.5);
    assert!((ground - 0.9).abs() < 1e-6, "{ground}");
    assert_eq!(seabed_depth(&mut w, sub, [2.5, 3.5, 0.5]), -0.4);
    assert_eq!(seabed_depth(&mut w, sub, [2.5, 3.5, 1.0]), -1.5);
    assert_eq!(adjust_dive_depth(&mut w, sub, [2.5, 3.5, 0.5]), -0.4);
    assert_eq!(adjust_dive_depth(&mut w, sub, [2.5, 3.5, 1.0]), 0.995);
    // off the mesh: 0
    let edge = w.add(7, 7, 9, 1, |c| c.mCruiseAltitude = 1.0);
    assert_eq!(seabed_under(&mut w, edge, 7.5, 7.5), 0.0);
}

#[test]
fn the_selection_bounds_follow_the_terrain_filter() {
    let mut w = World::new(8, 8);
    let a = w.add(4, 4, 1, 1, |_| {});
    let b = w.add(1, 6, 1, 1, |_| {});
    let c = w.add(6, 2, 1, 1, |_| {});
    let mut r = [9.0; 4];
    selection_bounds(&mut w, &[], &mut r, 0);
    assert_eq!(r, [9.0; 4]);
    selection_bounds(&mut w, &[a, b, c], &mut r, 0);
    assert_eq!(r, [1.0, 2.0, 6.0, 6.0]);
    // filter 2: only the units that move on terrain 1 (the even ones: c) beside the first
    selection_bounds(&mut w, &[a, b, c], &mut r, 2);
    assert_eq!(r, [4.0, 2.0, 6.0, 4.0]);
    selection_bounds(&mut w, &[a, b, c], &mut r, 1);
    assert_eq!(r, [1.0, 4.0, 4.0, 6.0]);
}

#[test]
fn the_radius_and_the_angle_from_north_follow_the_facing() {
    let mut w = World::new(8, 8);
    let a = w.add(4, 4, 1, 1, |c| c.mInnerCollisionSphere = 2.0);
    let ahead = w.add(4, 6, 1, 1, |_| {});
    let side = w.add(6, 4, 1, 1, |_| {});
    // facing north: a unit straight north sees it end on (the full radius); one to the side a tenth
    assert_eq!(unit_radius(&mut w, a, ahead), 2.0);
    assert_eq!(unit_radius(&mut w, a, side), 0.2);
    assert_eq!(angle_from_north([0.0, 5.0, 0.0], [0.0, 0.0, 0.0]), 0.0);
    let east = angle_from_north([5.0, 0.0, 0.0], [0.0, 0.0, 0.0]);
    assert!((east - std::f32::consts::FRAC_PI_2).abs() < 1e-3);
}

#[test]
fn the_gather_reach_measures_to_the_targets_point() {
    let mut w = World::new(8, 8);
    let unit = w.add(2, 2, 1, 1, |_| {});
    let tree = w.add(2, 3, 2, 0, |_| {});
    // the target's point is a tile above its location (the mock's drop point): 2 tiles away, squared 4
    assert!(!within_reach(&mut w, unit, tree, None));
    assert!(within_reach(&mut w, unit, tree, Some([2.5, 4.0, 0.0])));
    assert_eq!(w.acts, vec!["reach 4", "reach 0.25"]);
    assert!(!footprint_visible(&mut w, unit, 1));
    let seen = w.add(3, 2, 1, 1, |_| {});
    assert!(footprint_visible(&mut w, seen, 1));
}

#[test]
fn the_escape_point_flees_the_widest_gap() {
    let mut w = World::new(16, 16);
    let unit = w.add(8, 8, 1, 1, |_| {});
    // no attacker: nothing written
    let mut out = [1.0, 2.0];
    assert!(!escape_point(&mut w, unit, &mut out));
    assert_eq!(out, [1.0, 2.0]);
    // one attacker east: a step of half the sight (2 tiles) west
    let east = w.add(10, 8, 1, 2, |_| {});
    let friend = w.add(9, 8, 1, 1, |_| {});
    w.near = vec![friend, east];
    assert_eq!(enemies_around(&mut w, unit), vec![east]);
    assert!(escape_point(&mut w, unit, &mut out));
    assert_eq!(out, [6.0, 8.0]);
    // two attackers, north and east: between them, pushed out to the reach
    let north = w.add(8, 10, 1, 2, |_| {});
    w.near = vec![east, north];
    assert!(escape_point(&mut w, unit, &mut out));
    assert!(out[0] > 8.0 && out[1] > 8.0, "{out:?}");
}

#[test]
fn a_footprint_is_linked_tile_by_tile_its_own_tile_first() {
    let mut w = World::new(8, 8);
    let fort = w.add(6, 6, 0x50, 1, |c| c.mTileSize = 2);
    footprint_link(&mut w, fort, true);
    assert_eq!(w.acts, vec!["add 0 6 6", "add 0 6 7", "add 0 7 6", "add 0 7 7"]);
    w.acts.clear();
    let tree = w.add(7, 7, 2, 0, |_| {});
    building_region(&mut w, fort, true);
    assert_eq!(w.acts, vec!["regions 6 6 2 0 true", "clear 0", "clear 1"]);
    w.acts.clear();
    building_region(&mut w, fort, false);
    assert_eq!(w.acts, vec!["regions 6 6 2 0 false"]);
    let _ = tree;
}

#[test]
fn the_units_on_a_site_step_aside_unless_busy_or_hostile() {
    let mut w = World::new(8, 8);
    let class_unit = w.add(0, 0, 0x60, 1, |c| c.mTileSize = 2);
    let idle = w.add(3, 3, 1, 1, |_| {});
    let busy = w.add(4, 3, 1, 1, |_| {});
    w.units[h_of(busy)].command = 1;
    let enemy = w.add(3, 4, 1, 2, |_| {});
    let flier = w.add(4, 4, 1, 1, |c| c.mCanFly = true);
    w.units[h_of(flier)].location[2] = 3.0;
    // the class's square around (4, 4): tiles 3..5
    step_aside_from_site(&mut w, 0, class_unit + 1, 1, [4, 4], false);
    assert_eq!(w.acts, vec![format!("aside {} 3 3 false", h_of(idle))]);
    w.acts.clear();
    step_aside_from_site(&mut w, 0, class_unit + 1, 1, [4, 4], true);
    assert_eq!(w.acts.len(), 3, "{:?}", w.acts);
    let _ = enemy;
}

#[test]
fn the_wall_builders_spread_then_step_along() {
    let mut w = World::new(8, 8);
    let pieces: Vec<u64> = (0..5).map(|i| w.add(i, 0, 0x60, 1, |_| {})).collect();
    w.group = (0..3).map(|i| w.add(i, 5, 1, 1, |_| {})).collect();
    w.units[h_of(w.group[1])].goal = 7;
    build_wall(&mut w, 9, &pieces);
    assert_eq!(w.acts, vec!["build 5 0", "build 6 2", "build 7 4", "build 5 1", "build 6 1", "build 7 3", "restart 6"]);
    w.acts.clear();
    build_wall(&mut w, 9, &[]);
    assert!(w.acts.is_empty());
}

#[test]
fn a_planned_wall_places_its_pieces_and_reshapes_the_old() {
    let mut w = World::new(8, 8);
    let class = w.add(0, 7, 0x60, 1, |c| {
        c.mIsWall = true;
        c.mTerrainTypes[0] = true;
    });
    let mut out = Vec::new();
    plan_wall(&mut w, [1.2, 2.0], [4.9, 2.0], class + 1, 1, true, true, &mut out);
    // the mock's line has no ends: the two tiles between (2, 2) and (3, 2), each a new piece
    assert_eq!(out.len(), 2);
    assert_eq!(w.acts.iter().filter(|a| a.starts_with("piece")).count(), 2, "{:?}", w.acts);
    // again: the pieces stand there now, so they are reshaped instead
    w.acts.clear();
    let mut again = Vec::new();
    plan_wall(&mut w, [1.2, 2.0], [4.9, 2.0], class + 1, 1, true, true, &mut again);
    assert!(again.is_empty());
    assert_eq!(w.acts.iter().filter(|a| a.starts_with("animation")).count(), 2, "{:?}", w.acts);
}

#[test]
fn a_wall_on_the_line_blocks_it() {
    let mut w = World::new(8, 8);
    assert_eq!(line_blocked(&mut w, 1, 1, 5, 1), (false, Some(0), 4, 1));
    let wall = w.add(3, 1, 0x60, 1, |c| c.mIsWall = true);
    // bare: one tile up, above the start's height
    assert_eq!(tile_top_above(&mut w, 3, 1, 0.5, true), (true, wall));
    assert_eq!(tile_top_above(&mut w, 3, 1, 0.5, false), (true, wall));
    assert_eq!(tile_top_above(&mut w, 2, 1, -1.0, true), (false, 0));
    assert_eq!(tile_top_above(&mut w, 2, 1, -1.0, false), (true, 0));
    let (blocked, blocker, x, y) = line_blocked(&mut w, 1, 1, 5, 1);
    assert_eq!((blocked, blocker, x, y), (true, Some(wall), 3, 1));
    assert_eq!(line_blocked(&mut w, 1, 1, 20, 1), (false, None, 0, 0));
}

#[test]
fn the_path_length_is_the_straight_distance_when_the_path_arrives() {
    let mut w = World::new(16, 16);
    let unit = w.add(2, 2, 1, 1, |_| {});
    let mut tile = [8, 10];
    w.path = Some([8, 10]);
    assert_eq!(path_length_to(&mut w, unit, &mut tile, 100, false), 10.0);
    assert_eq!(tile, [8, 10]);
    assert_eq!(w.acts, vec!["path [2, 2] [8, 10] [0, 0, 12, 14] 50"]);
    w.path = Some([7, 10]);
    assert_eq!(path_length_to(&mut w, unit, &mut tile, 3, false), 10000.0);
    assert_eq!(tile, [-1, -1]);
}

#[test]
fn the_sound_fades_off_screen_and_far_away() {
    let mut v = EeUnitsUuSoundView { seen: 0, ..Default::default() };
    assert_eq!(sound_parameters(&v, 0.8, [0.0; 3]), [0.0, 0.0, 1.0]);
    v = EeUnitsUuSoundView { seen: 1, screen: [0.5, 0.0, 0.0], depth: 10.0, camera: [0.0; 3], y_clip: 0.75, zoom: 1.0 };
    // panned a third right, at full volume (near the camera: no edge fade)
    let p = sound_parameters(&v, 0.8, [1.0, 1.0, 1.0]);
    assert_eq!(p[0], 0.8);
    assert!((p[1] - 1.0 / 3.0).abs() < 1e-6);
    // far: 31 of the 27..35 fade
    v.depth = 31.0;
    assert_eq!(sound_parameters(&v, 1.0, [10.0, 0.0, 0.0])[0], 0.5);
    // behind the camera by 2: half
    v.depth = -2.0;
    assert_eq!(sound_parameters(&v, 1.0, [1.0, 0.0, 0.0])[0], 0.5);
    // off the screen's side, far from the camera: faded by the edge
    v.depth = 5.0;
    v.screen = [1.1, 0.0, 0.0];
    assert!((sound_parameters(&v, 1.0, [10.0, 0.0, 0.0])[0] - 0.6).abs() < 1e-5);
    assert_eq!(doppler_pitch(0.0, 3.0, &GeTransformation::IDENTITY, [0.0; 3]), None);
    assert_eq!(doppler_pitch(1.0, 0.0, &GeTransformation::IDENTITY, [0.0; 3]), None);
    // moving north towards a camera to the north: higher
    assert!(doppler_pitch(1.0, 2.0, &GeTransformation::IDENTITY, [0.0, 50.0, 0.0]).unwrap() > 1.0);
}

#[test]
fn a_deposit_near_a_garrisoned_town_gains_its_bonus() {
    let mut w = World::new(8, 8);
    let town = w.add(1, 1, UU_TYPE_TOWN_CENTER as u32, 1, |c| {
        c.mGarrisonCapacity = 5;
        c.mGarrisonBonus = 0.1;
    });
    w.add(7, 7, UU_TYPE_SETTLEMENT as u32, 1, |c| {
        c.mGarrisonCapacity = 2;
        c.mGarrisonBonus = 0.05;
    });
    w.add(7, 6, UU_TYPE_CAPITOL as u32, 1, |c| c.mGarrisonBonus = 0.2);
    w.units[h_of(town)].cx.mGarrisonCount = 4;
    let miner = w.add(2, 2, 1, 1, |_| {});
    w.units[h_of(miner)].unit.mResourceType = UU_RESOURCE_GOLD as _;
    w.units[h_of(miner)].unit.mResourceAmount = 10;
    w.add(3, 3, 3, 0, |_| {}); // the miner's site (the mock's last resource)
    deposit_load(&mut w, miner, town);
    // 15 carried after the factor; levels 2 (settlement) and 2 (town centre): bonus 0.3, 15 * 1.3 = 19.5 -> 20
    assert_eq!(w.acts, vec!["take 5", "effect 3", "resource 20 10 true 0.3", "statistic 9 20"]);
    assert_eq!(w.units[h_of(miner)].unit.mResourceAmount, 0);
}

#[test]
fn the_editor_places_away_from_the_edges_and_other_units() {
    let mut w = World::new(12, 12);
    let flag = w.add(0, 11, 0x50, 1, |c| {
        c.mTerrainTypes[0] = true;
        c.mIsComplex = 1;
        c.mInnerCollisionSphere = 1.0;
    });
    let _ = flag;
    // into a transport under the mouse: only the edges count
    w.add(0, 10, 0x70, 1, |_| {});
    assert!(editor_can_place(&mut w, 1, 0x70, [5.5, 5.5, 0.0], 5, 5));
    assert!(!editor_can_place(&mut w, 1, 0x70, [0.5, 5.5, 0.0], 0, 5));
    // a complex unit on open ground, then next to another unit
    assert!(editor_can_place(&mut w, 1, 0x50, [5.5, 5.5, 0.0], 5, 5));
    w.add(6, 5, 1, 2, |_| {});
    assert!(editor_can_place(&mut w, 1, 0x50, [5.5, 5.5, 0.0], 5, 5));
    assert!(!editor_can_place(&mut w, 1, 0x50, [6.0, 5.5, 0.0], 5, 5));
    assert!(editor_can_place(&mut w, 1, 0x50, [9.5, 9.5, 0.0], 9, 9));
    // off the map: never; the type 0x167 on the map: anywhere but the edges
    assert!(!editor_can_place(&mut w, 1, 0x50, [20.5, 5.5, 0.0], 20, 5));
    assert!(editor_can_place(&mut w, 1, 0x167, [5.5, 5.5, 0.0], 5, 5));
}
