//! The unit utilities' tests: the pure geometry against hand-worked values, the world searches over a mock map.

// the C++'s negated float comparisons kept as written (a NaN); `c_long` casts needed on LP64
#![allow(clippy::neg_cmp_op_on_partial_ord, clippy::unnecessary_cast)]
use std::collections::BTreeMap;

use ee_render::ge::types::GeTransformation;

use crate::unit_class_fields::EeUnitsUnitClassFields;
use crate::unit_fields::{EeUnitsComplexUnitFields, EeUnitsRtsUnitFields, EeUnitsUnitFields};
use crate::uu::k::*;
use crate::uu::*;
use crate::uu_tiles::*;

// ---- the pure geometry ----

#[test]
fn the_sub_tile_steps_follow_the_cases() {
    // the tile itself: four spots of a half-tile unit, the square always
    let s = sub_tile_steps(0.5, 1, 0, 0, false);
    assert_eq!(s, EeUnitsUuSteps { start_x: 0.25, start_y: 0.25, step_x: 0.0, step_y: 0.5, count: 4 });
    // from the left (dx -1, dy 0): from the far side, down the tile, one side's spots
    let s = sub_tile_steps(0.5, 1, -1, 0, false);
    assert_eq!(s, EeUnitsUuSteps { start_x: 0.75, start_y: 0.75, step_x: 0.0, step_y: -0.5, count: 2 });
    // the leg's length matches: one spot
    assert_eq!(sub_tile_steps(0.5, 2, -1, 2, false).count, 1);
    assert_eq!(sub_tile_steps(0.5, 3, -1, 2, false).count, 2);
    assert_eq!(sub_tile_steps(0.5, 3, -1, 2, true).count, 4);
    // the corner (1, 1): a half step
    let s = sub_tile_steps(0.25, 1, 1, 1, false);
    assert_eq!(s, EeUnitsUuSteps { start_x: 0.125, start_y: 0.125, step_x: 0.0, step_y: 0.125, count: 1 });
    // an unlisted pair falls through to the truncated square
    let s = sub_tile_steps(0.3, 1, 5, 5, false);
    assert_eq!((s.start_x, s.step_y, s.count), (0.15, 0.3, 9));
    // a zero size: the C++'s integer indefinite, squared (wrapping)
    assert_eq!(sub_tile_steps(0.0, 1, 9, 9, false).count, 0);
}

#[test]
fn the_rings_list_their_tiles_in_order() {
    assert_eq!(square_ring(5, 5, 1, 1), vec![[4, 5], [6, 5], [5, 4], [5, 6]]);
    assert_eq!(square_ring(0, 0, 2, 1), vec![[-1, 0], [2, 0], [0, -1], [0, 1], [1, -1], [1, 1]]);
    assert_eq!(square_ring_tiles(0, 0, 1), vec![[-1, 1], [0, 1], [1, 1], [-1, 0], [1, 0], [-1, -1], [0, -1], [1, -1]]);
    assert_eq!(square_ring_tiles(3, 3, 0), vec![[3, 3], [3, 3]]);
    let mut out = vec![[9, 9]];
    tiles_in_radius(0, 0, 1, false, &mut out);
    assert_eq!(out, vec![[9, 9], [-1, -1], [-1, 0], [0, -1], [0, 0]]);
    let mut out = Vec::new();
    tiles_in_radius(0, 0, 1, true, &mut out);
    assert_eq!(out.len(), 9);
    let mut out = Vec::new();
    tiles_in_radius(0, 0, 0x8000_0000, true, &mut out);
    assert!(out.is_empty());
}

#[test]
fn the_tiles_near_a_point_are_those_whose_nearest_point_is_in_reach() {
    let mut out = Vec::new();
    tiles_near_point(5.5, 5.5, 0.5, &mut out);
    assert_eq!(out, vec![[5, 5]]);
    let mut out = Vec::new();
    tiles_near_point(5.5, 5.5, 1.2, &mut out);
    // the corners' nearest points are 0.707 away: in; the ring past them is not
    assert_eq!(out, vec![[4, 4], [4, 5], [4, 6], [5, 4], [5, 5], [5, 6], [6, 4], [6, 5], [6, 6]]);
}

#[test]
fn the_diagonal_corners_and_repeats_drop() {
    // walking back from (2, 2): (1, 1) is one step on, so (1, 2)... the tile after (0, 0) when the last differs by
    // (+1, +1)
    let mut t = vec![[0, 0], [0, 1], [1, 1]];
    drop_diagonal_corners(&mut t);
    assert_eq!(t, vec![[0, 0], [1, 1]]);
    let mut t = vec![[0, 0], [1, 0], [2, 0]];
    drop_diagonal_corners(&mut t);
    assert_eq!(t, vec![[0, 0], [1, 0], [2, 0]]);
    let mut t: Vec<Tile> = Vec::new();
    drop_diagonal_corners(&mut t);
    let mut t = vec![[1000, 1000], [1, 1], [1, 1], [2, 2], [1, 1]];
    drop_repeated_tiles(&mut t);
    assert_eq!(t, vec![[1, 1], [2, 2], [1, 1]]);
}

#[test]
fn the_compass_turns_and_steps() {
    for d in 0..8 {
        assert_eq!(turn_direction(d, 8, true), d);
        assert_eq!(turn_direction(turn_direction(d, 3, true), 3, false), d);
    }
    assert_eq!(turn_direction(0, 1, true), 4);
    assert_eq!(turn_direction(3, 1, false), 7);
    assert_eq!(turn_direction(9, 4, true), 9);
    assert_eq!(turn_direction(2, 0, true), 2);
    assert_eq!(tile_in_direction(10, 10, 0, 2), [8, 12]);
    assert_eq!(tile_in_direction(10, 10, 7, 1), [11, 9]);
    assert_eq!(tile_in_direction(10, 10, 8, 1), [10, 10]);
    assert_eq!(move_in_direction(10.25, 3.5, 6, 2), [12.25, 3.5]);
    assert_eq!(slot_tile([4, 4], 1), [2, 6]);
    assert_eq!(slot_tile([4, 4], 9), [4, 4]);
    // north (straight up y): sector 4; east: 6; south: 3 (the wrap); west: 1
    assert_eq!(tile_direction(0, 0, 0, 5), 4);
    assert_eq!(tile_direction(0, 0, 5, 0), 6);
    assert_eq!(tile_direction(0, 0, 0, -5), 3);
    assert_eq!(tile_direction(0, 0, -5, 0), 1);
}

#[test]
fn the_wall_pieces_turn_as_the_corners_ask() {
    assert_eq!(wall_piece(1), EeUnitsUuWallPiece { frame_set: 9, turns: 1, yaw: 1.5707964 });
    assert_eq!(wall_piece(2), EeUnitsUuWallPiece { frame_set: 9, turns: 0, yaw: 0.0 });
    assert_eq!(wall_piece(10).frame_set, 3);
    assert_eq!(wall_piece(0).frame_set, 11);
    assert_eq!(wall_piece(-4).frame_set, 11);
}

#[test]
fn a_jittered_heading_stays_in_a_half_turn() {
    assert_eq!(jitter_heading(0.0, 0.0), 0.0);
    let a = jitter_heading(3.0, 10.0);
    assert!(a > -std::f32::consts::PI && a <= std::f32::consts::PI);
    assert_eq!(ceil(1.2), 2);
    assert_eq!(ceil(-1.2), -1);
    assert_eq!(trunc_long(f32::NAN), i32::MIN);
    assert_eq!(trunc_long(-2.7), -2);
}

// ---- a mock world ----

struct MockUnit {
    rts: Box<EeUnitsRtsUnitFields>,
    unit: Box<EeUnitsUnitFields>,
    cx: Box<EeUnitsComplexUnitFields>,
    class: Box<EeUnitsUnitClassFields>,
    player: u64,
    position: [f32; 3],
    passable: bool,
    foundation: bool,
}

#[derive(Default)]
struct Mock {
    tiles: BTreeMap<(i32, i32), (i32, f32, Vec<u64>)>,
    units: Vec<MockUnit>,
    draws: Vec<i32>,
    lines: Vec<(i32, i32, i32, i32)>,
    regions: Vec<(i32, i32)>,
}

const MAP: u64 = 7;

impl Mock {
    fn with_tiles(w: i32, h: i32) -> Mock {
        let mut m = Mock::default();
        for x in 0..w {
            for y in 0..h {
                m.tiles.insert((x, y), (0, 0.0, Vec::new()));
            }
        }
        m
    }
    fn add(&mut self, x: i32, y: i32, unit_type: u32, player: u64, f: impl FnOnce(&mut EeUnitsUnitClassFields)) -> u64 {
        let mut class = EeUnitsUnitClassFields { mUnitType: unit_type as _, mTileSize: 1, ..Default::default() };
        class.mInnerCollisionSphere = 0.5;
        f(&mut class);
        let id = self.units.len() as u64 + 1;
        self.units.push(MockUnit {
            rts: Box::new(EeUnitsRtsUnitFields {
                mTileX: x as _,
                mTileY: y as _,
                mUnitID: (id * 10) as _,
                ..Default::default()
            }),
            unit: Box::new(EeUnitsUnitFields { mHitPoints: 10, ..Default::default() }),
            cx: Box::default(),
            class: Box::new(class),
            player,
            position: [x as f32 + 0.5, y as f32 + 0.5, 0.0],
            passable: false,
            foundation: false,
        });
        let obj = id * 0x100;
        if let Some(t) = self.tiles.get_mut(&(x, y)) {
            t.2.push(obj);
        }
        obj
    }
    fn u(&self, obj: u64) -> &MockUnit {
        &self.units[(obj / 0x100 - 1) as usize]
    }
}

fn tile_handle(x: i32, y: i32) -> u64 {
    0x1_0000_0000 | ((x as u32 as u64) << 16) | (y as u32 as u64)
}

impl UuHost for Mock {
    fn map(&mut self) -> u64 {
        MAP
    }
    fn map_tile(&mut self, _map: u64, x: i32, y: i32) -> u64 {
        if self.tiles.contains_key(&(x, y)) {
            tile_handle(x, y)
        } else {
            0
        }
    }
    fn tile_terrain(&mut self, tile: u64) -> i32 {
        self.tiles[&(((tile >> 16) & 0xffff) as u16 as i16 as i32, (tile & 0xffff) as u16 as i16 as i32)].0
    }
    fn tile_height(&mut self, tile: u64) -> f32 {
        self.tiles[&(((tile >> 16) & 0xffff) as u16 as i16 as i32, (tile & 0xffff) as u16 as i16 as i32)].1
    }
    fn tile_units(&mut self, tile: u64, out: &mut Vec<u64>) {
        *out =
            self.tiles[&(((tile >> 16) & 0xffff) as u16 as i16 as i32, (tile & 0xffff) as u16 as i16 as i32)].2.clone();
    }
    fn is_edge_tile(&mut self, x: i32, _y: i32) -> bool {
        x == 0
    }
    fn raised_corners(&mut self, _map: u64, _x: i32, _y: i32) -> i32 {
        0
    }
    fn mesh_z(&mut self, _map: u64, _x: i32, _y: i32) -> f32 {
        0.0
    }
    fn mesh_ground(&mut self, _x: f32, _y: f32) -> f32 {
        0.0
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
            location: u.position,
            ..Default::default()
        }
    }
    fn class_of_type(&mut self, unit_type: i32) -> EeUnitsUuClass {
        let found = self.units.iter().position(|u| u.class.mUnitType as i32 == unit_type);
        match found {
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
        self.u(obj).position
    }
    fn passable_by(&mut self, obj: u64, _by: u64) -> bool {
        self.u(obj).passable
    }
    fn is_foundation(&mut self, obj: u64) -> bool {
        self.u(obj).foundation
    }
    fn can_move_on_terrain(&mut self, _class: u64, _unit: u64, terrain: i32) -> bool {
        terrain == 0
    }
    fn passable_flags(&mut self, _class: u64, _unit: u64) -> i32 {
        1
    }
    fn surrounding_tiles(&mut self, target: u64, _virtual_call: bool) -> Vec<Tile> {
        let u = self.u(target);
        square_ring(u.rts.mTileX as i32, u.rts.mTileY as i32, 1, 1)
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
    fn tile_line(&mut self, x1: i32, y1: i32, x2: i32, y2: i32, _ends: bool, _thin: bool) -> Vec<Tile> {
        self.lines.push((x1, y1, x2, y2));
        // a straight horizontal line, enough for the tests
        let step = if x2 >= x1 { 1 } else { -1 };
        let mut out = Vec::new();
        let mut x = x1;
        loop {
            out.push([x, y1]);
            if x == x2 {
                break;
            }
            x += step;
        }
        out
    }
    fn reserved(&mut self, _group: u64, _point: [f32; 3]) -> bool {
        false
    }
    fn ground_point(&mut self, point: [f32; 3]) -> [f32; 3] {
        [point[0], point[1], 2.0]
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
    fn player_units(&mut self, index: i32) -> Vec<u64> {
        (0..self.units.len())
            .filter(|&i| self.units[i].player == index as u64)
            .map(|i| (i as u64 + 1) * 0x100)
            .collect()
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
        self.draws.pop().unwrap_or(min)
    }
    fn random_float(&mut self, min: f32, _max: f32) -> f32 {
        min
    }
    fn location(&mut self, _complex: u64) -> GeTransformation {
        GeTransformation::IDENTITY
    }
    fn set_animation(&mut self, complex: u64, animation: i32) {
        let i = (complex / 0x100 - 1) as usize;
        self.units[i].cx.mAnimation = animation as _;
    }
    fn set_transformation(&mut self, _complex: u64, _m: &GeTransformation) {}
    fn is_tile_flat(&mut self, _x: i32, _y: i32) -> bool {
        true
    }
    fn tile_visible_to(&mut self, _tile: u64, _player: u64) -> bool {
        true
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
    fn regions_len(&mut self, x: i32, y: i32) -> i32 {
        i32::from(self.regions.contains(&(x, y)))
    }
    fn goal_kind(&mut self, _unit: u64) -> i32 {
        -1
    }
    fn stats_range(&mut self, _unit: u64) -> f32 {
        0.0
    }
    fn center_offset(&mut self, _complex: u64, x: f32, y: f32) -> (f32, f32) {
        (x + 0.5, y + 0.5)
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
        self.u(target).position
    }
    fn part_left_out(&mut self, _piece: u64) -> bool {
        false
    }
    fn path_area_id(&mut self, x: i32, _y: i32) -> u16 {
        u16::from(x >= 8)
    }
    fn gate_ends(&mut self, _gate: u64) -> [[f32; 3]; 2] {
        [[0.0; 3], [0.0, 10.0, 0.0]]
    }
}

#[test]
fn the_units_on_tiles_come_in_tile_and_node_order() {
    let mut m = Mock::with_tiles(8, 8);
    let a = m.add(2, 2, 5, 1, |_| {});
    let b = m.add(2, 2, 6, 1, |_| {});
    let c = m.add(3, 2, 5, 2, |_| {});
    let mut out = vec![99];
    assert_eq!(units_on(&mut m, &[[3, 2], [2, 2], [20, 20]], &mut out), 3);
    assert_eq!(out, vec![99, c, a, b]);
    // the last tile first; no big unit: no sort
    let mut out = Vec::new();
    assert_eq!(units_on_tiles(&mut m, &[[3, 2], [2, 2]], &mut out), 3);
    assert_eq!(out, vec![a, b, c]);
    // a big unit: by descending id, once each
    m.units[0].class.mTileSize = 2;
    m.tiles.get_mut(&(3, 2)).unwrap().2.push(a);
    let mut out = Vec::new();
    assert_eq!(units_on_tiles(&mut m, &[[3, 2], [2, 2]], &mut out), 3);
    assert_eq!(out, vec![c, b, a]);
    let mut out = Vec::new();
    assert_eq!(units_of_type_on_tiles(&mut m, &[[2, 2], [3, 2]], Some(1), 5, &mut out), 2);
    assert_eq!(out, vec![a, a]);
    assert_eq!(tile_unit_of_type(&mut m, [2, 2], 6, None), b);
    assert_eq!(tile_unit_of_type(&mut m, [2, 2], 6, Some(2)), 0);
    m.units[1].unit.mHitPoints = 0;
    assert_eq!(tile_unit_of_type(&mut m, [2, 2], 6, None), 0);
    let mut out = Vec::new();
    assert_eq!(player_units_of_type(&mut m, 5, 2, &mut out), 1);
    assert_eq!(out, vec![c]);
}

#[test]
fn the_corners_close_between_two_walls() {
    let mut m = Mock::with_tiles(8, 8);
    m.add(2, 3, 9, 1, |c| c.mIsWall = true);
    m.add(3, 2, 9, 1, |c| c.mIsWall = true);
    assert!(is_corner_blocked(&mut m, 2, 3, 3, 2));
    assert!(!is_corner_blocked(&mut m, 2, 3, 4, 4));
    assert!(!is_corner_blocked(&mut m, 2, 3, 30, 4));
    // the ring around (3, 3): its top-left corner (2, 2) is closed by the walls at (2, 3) and (3, 2); so is the
    // bottom-left one (2, 4): the C++ tests (x - 1, bottom - 1) with (x, height + 1), here the same two tiles
    assert_eq!(ring_corners(&mut m, 0, 3, 3, 1, 1), vec![[4, 2], [4, 4]]);
    assert_eq!(ring_corners(&mut m, 0x100, 3, 3, 1, 1), vec![[4, 2], [4, 4]]);
    assert_eq!(ring_corners(&mut m, 1, 3, 3, 1, 1).len(), 4);
    let mut out = vec![[0, 0]];
    block_ring(&mut m, 1, 3, 3, 1, 1, &mut out, true);
    assert_eq!(out.len(), 9);
    block_ring(&mut m, 1, 3, 3, 1, 1, &mut out, false);
    assert_eq!(out.len(), 8);
}

#[test]
fn the_rings_drop_edge_tiles_and_keep_their_orders() {
    let mut m = Mock::with_tiles(8, 8);
    let ring = ring_tiles(&mut m, 4, 4, 1, 1, 1);
    // the centre first, then the eight in the C++'s order (sides (3,4) (5,4) (4,3) (4,5), corners (3,3) (3,5) (5,3)
    // (5,5): indices 8 2 6 1 7 3 5 0 4)
    assert_eq!(ring, vec![[4, 4], [4, 3], [5, 3], [5, 4], [5, 5], [4, 5], [3, 5], [3, 4], [3, 3]]);
    // the tile on the edge (x 0) is dropped
    assert_eq!(ring_tiles(&mut m, 1, 4, 1, 1, 1).len(), 6);
    assert_eq!(ring_tiles(&mut m, 4, 4, 1, 0, 1), vec![[4, 4]]);
    let sorted = sorted_ring_tiles(&mut m, [6.9, 4.2, 0.0], 4, 4, 1, 1, 1);
    // nearest (6, 4) first: (5, 4), then (5, 3) and (5, 5) at 2 (in the ring's order: a stable insertion sort)
    assert_eq!(&sorted[..3], &[[5, 4], [5, 3], [5, 5]]);
}

#[test]
fn a_wall_line_extends_along_the_players_walls() {
    let mut m = Mock::with_tiles(16, 16);
    for x in 6..9 {
        m.add(x, 5, 9, 1, |c| c.mIsWall = true);
    }
    let mut out = Vec::new();
    extend_wall(&mut m, [4, 5], [5, 5], 1, 9, &mut out);
    assert_eq!(out, vec![[8, 5], [7, 5], [6, 5]]);
    let mut out = Vec::new();
    assert_eq!(extend_wall_line(&mut m, &[[3, 5], [5, 5]], 1, 9, &mut out), 0);
    // nothing before (3, 5); after (5, 5) the line's steps of two: (7, 5), then (9, 5) holds no wall
    assert_eq!(out, vec![[3, 5], [5, 5], [7, 5]]);
    // a single tile beside one wall: extended to it
    let mut out = Vec::new();
    extend_wall_line(&mut m, &[[9, 5]], 1, 9, &mut out);
    assert_eq!(out, vec![[6, 5], [7, 5], [8, 5], [9, 5]]);
    assert_eq!(wall_line(&mut m, [2, 2], [2, 2]), vec![[2, 2]]);
    assert_eq!(wall_line(&mut m, [2, 2], [5, 2]), vec![[2, 2], [3, 2], [4, 2], [5, 2]]);
    assert_eq!(m.lines, vec![(2, 2, 5, 2)]);
}

#[test]
fn flat_wall_lines_have_flat_pieces_and_junctions_take_the_plain_piece() {
    let mut m = Mock::with_tiles(16, 16);
    assert_eq!(pick_wall_pieces(&mut m, MAP, &[[2, 2], [2, 3], [2, 4], [2, 5]]), vec![0, 2, 2, 0]);
    assert_eq!(pick_wall_pieces(&mut m, MAP, &[[2, 2], [3, 2], [4, 2]]), vec![0, 1, 0]);
    assert_eq!(pick_wall_pieces(&mut m, MAP, &[[2, 2], [3, 3], [4, 4]]), vec![0, 4, 0]);
    assert!(pick_wall_pieces(&mut m, MAP, &[]).is_empty());
    let w = m.add(5, 5, 9, 1, |c| c.mIsWall = true);
    m.add(4, 5, 9, 1, |c| c.mIsWall = true);
    m.add(6, 5, 9, 1, |c| c.mIsWall = true);
    make_wall_junctions(&mut m, &[w], &[], 1, MAP);
    assert_eq!(m.u(w).cx.mAnimation, 0);
    // a placed tile below-right counts (the unsigned difference 0 or 1); one above-left does not
    make_wall_junctions(&mut m, &[w], &[[4, 4]], 1, MAP);
    assert_eq!(m.u(w).cx.mAnimation, 0);
    make_wall_junctions(&mut m, &[w], &[[6, 6]], 1, MAP);
    assert_eq!(m.u(w).cx.mAnimation, 0xb);
}

#[test]
fn units_stand_aside_or_block_as_their_owners_and_speeds_say() {
    let mut m = Mock::with_tiles(8, 8);
    let placed = m.add(1, 1, 40, 1, |_| {});
    let friend = m.add(3, 3, 41, 1, |_| {});
    let foe = m.add(3, 3, 42, 2, |_| {});
    // a friendly unit with no speed blocks; with speed it makes way
    assert_eq!(tile_unit_standing(&mut m, friend, 1, 40, false), 2);
    m.units[1].cx.mCurrentPhysicsAction = 1;
    assert_eq!(tile_unit_standing(&mut m, foe, 1, 40, false), 1);
    m.units[2].cx.mCurrentPhysicsAction = 1;
    assert_eq!(tile_unit_standing(&mut m, foe, 1, 40, false), 2);
    assert_eq!(tile_unit_standing(&mut m, foe, 0, 40, false), 2);
    m.units[2].class.mIsDecoration = true;
    assert_eq!(tile_unit_standing(&mut m, foe, 1, 40, false), 0);
    let mut out = Vec::new();
    let t = m.map_tile(MAP, 3, 3);
    assert!(tile_units_standing(&mut m, 1, 40, t, false, 2, &mut out));
    assert_eq!(out, vec![friend]);
    assert!(!tile_units_standing(&mut m, 1, 40, 0, false, 2, &mut out));
    assert!(out.is_empty());
    assert!(!is_tile_unblocked(&mut m, 1, 40, t));
    let _ = placed;
}

#[test]
fn a_unit_stands_on_free_land_and_finds_a_spot() {
    let mut m = Mock::with_tiles(8, 8);
    let me = m.add(2, 2, 50, 1, |_| {});
    assert!(can_stand_on(&mut m, [3, 3], me));
    assert!(!can_stand_on(&mut m, [0, 3], me));
    assert!(!can_stand_on(&mut m, [9, 3], me));
    m.tiles.get_mut(&(4, 4)).unwrap().0 = 3;
    assert!(!can_stand_on(&mut m, [4, 4], me));
    let other = m.add(3, 4, 51, 1, |_| {});
    assert!(!can_stand_on(&mut m, [3, 4], me));
    m.units[1].passable = true;
    assert!(can_stand_on(&mut m, [3, 4], me));
    let _ = other;
    // an empty tile entered from (3, 3) away: the first spot, a quarter in (the corner case (3, 3))
    let mut at = [0.0, 0.0, 7.0];
    assert!(find_place_on_tile(&mut m, me, 0, 2, 2, 1, 5, 5, 0, &mut at));
    assert_eq!(at, [5.25, 5.25, 7.0]);
    // a collidable unit in the middle of the tile takes every spot of a big unit
    m.add(5, 5, 52, 2, |c| c.mIsCollidable = true);
    m.units[0].class.mInnerCollisionSphere = 1.0;
    assert!(!find_place_on_tile(&mut m, me, 0, 5, 5, 1, 5, 5, 0, &mut at));
}

#[test]
fn the_placement_turns_and_grounds_the_place_found() {
    let mut m = Mock::with_tiles(12, 12);
    let me = m.add(5, 5, 60, 1, |_| {});
    let mut t = GeTransformation::IDENTITY;
    assert!(find_placement(&mut m, [0.0; 3], me, 0, 5, 5, 1, 2, true, false, false, false, &mut t, 0));
    assert_eq!(t.m[2][3], 2.0);
    // south: the yaw pi
    assert!(t.m[0][0] < -0.99);
    // no ring and no centre: nothing
    let mut t = GeTransformation::IDENTITY;
    assert!(!find_placement(&mut m, [0.0; 3], me, 0, 5, 5, 1, 0, true, false, false, false, &mut t, 0));
    assert_eq!(t, GeTransformation::IDENTITY);
}

#[test]
fn the_random_turns_draw_as_the_cpp() {
    // draws popped from the end: the turn 1 (+ 4 - 1 = 4), then the side 1 (clockwise)
    let mut m = Mock { draws: vec![1, 1], ..Default::default() };
    assert_eq!(turn_away(&mut m, 0, 1), turn_direction(0, 4, true));
    assert!(m.draws.is_empty());
    assert_eq!(jitter(&mut m, 1.0, 5.0, 5.0), jitter_heading(1.0, 5.0));
}

#[test]
fn a_farm_is_found_on_the_slot_tiles_and_granaries_beside_a_farm() {
    let mut m = Mock::with_tiles(16, 16);
    let me = m.add(8, 8, 70, 1, |_| {});
    // the granary class (size 1: nine rings)
    m.add(15, 15, UU_TYPE_GRANARY, 3, |_| {});
    let farm = m.add(6, 10, UU_TYPE_FARM, 1, |_| {});
    let mut ring = 0;
    assert_eq!(find_free_farm(&mut m, me, false, &mut ring), farm);
    assert_eq!(ring, 2);
    let mut ring = 2;
    assert_eq!(find_free_farm(&mut m, me, false, &mut ring), 0);
    assert_eq!(ring, 9);
    let mut out = Vec::new();
    assert!(!farm_drop_sites(&mut m, farm, &mut out));
    let g = m.add(7, 11, UU_TYPE_GRANARY, 1, |_| {});
    assert!(farm_drop_sites(&mut m, farm, &mut out));
    assert_eq!(out, vec![g]);
}

#[test]
fn footprints_and_resource_spots() {
    let mut m = Mock::with_tiles(16, 16);
    let big = m.add(4, 4, 80, 1, |c| c.mTileSize = 2);
    let mut out = vec![[0, 0]];
    footprint_tiles(&mut m, big, &mut out);
    assert_eq!(out, vec![[4, 4], [4, 5], [5, 4], [5, 5]]);
    let mut spots = Vec::new();
    resource_spots(&mut m, big, &mut spots);
    // the ring of a 4-square around (3, 3): the corners first, then the sides
    assert_eq!(spots.len(), 4 + 16);
    assert_eq!(spots[0], [2, 2]);
    m.add(2, 2, 81, 1, |_| {});
    let mut spots = Vec::new();
    resource_spots(&mut m, big, &mut spots);
    assert_eq!(spots.len(), 19);
    let (d, at) = nearest_footprint_tile(&mut m, big, [7.0, 7.0, 0.0]);
    assert_eq!((d, at), (2.5 * 2.5 / 2.5 * 0.0 + 1.5 * 1.5 * 2.0, [5, 5]));
}

// ---- the placement and path leaves ----

#[test]
fn a_site_takes_a_unit_on_free_land_with_neighbours() {
    use crate::uu_place::{block_takes, can_place_unit_on_tile};
    let mut m = Mock::with_tiles(8, 8);
    m.add(7, 7, 90, 1, |c| c.mTerrainTypes[0] = true);
    let mut flag = None;
    assert!(can_place_unit_on_tile(&mut m, 1, 0, 90, 3, 3, false, false, &mut flag));
    assert_eq!(flag, Some(false));
    // off the map; on its border (a neighbour missing)
    assert!(!can_place_unit_on_tile(&mut m, 1, 0, 90, 9, 3, false, false, &mut flag));
    assert!(!can_place_unit_on_tile(&mut m, 1, 0, 90, 0, 3, false, false, &mut flag));
    // a blocking friend (no speed); then a foe standing still makes way
    m.add(4, 4, 91, 1, |_| {});
    assert!(!can_place_unit_on_tile(&mut m, 1, 0, 90, 4, 4, false, false, &mut flag));
    m.add(5, 5, 92, 2, |_| {});
    assert!(can_place_unit_on_tile(&mut m, 1, 0, 90, 5, 5, false, false, &mut flag));
    assert_eq!(flag, Some(true));
    assert!(block_takes(&mut m, 1, 90, 1, 1, 3, 2, false));
    assert!(!block_takes(&mut m, 1, 90, 3, 3, 2, 2, false));
}

#[test]
fn a_ship_path_keeps_its_ends_its_first_points_and_the_points_near_units() {
    use crate::uu_place::naval_path;
    let mut m = Mock::with_tiles(32, 32);
    let ship = m.add(1, 1, 93, 1, |c| c.mDomains = 4);
    let path: Vec<[f32; 2]> = (0..10).map(|i| [2.5 + i as f32, 2.5]).collect();
    // on one row: from the fifth point the start's row matches, so each is dropped until three are
    let kept = naval_path(&mut m, ship, &path);
    assert_eq!(kept.first(), Some(&0));
    assert_eq!(kept.last(), Some(&9));
    assert!(kept.contains(&1) && kept.contains(&4));
    // a unit in a region beside the path keeps the point there
    m.regions.push((7 / 4, 2 / 4));
    let kept = naval_path(&mut m, ship, &path);
    assert!(kept.contains(&5));
    assert!(naval_path(&mut m, ship, &[]).is_empty());
    assert_eq!(naval_path(&mut m, ship, &[[1.0, 1.0]]), vec![0]);
}

#[test]
fn a_fan_of_lines_is_sorted_by_row_and_unique() {
    use crate::uu_place::fan_tiles;
    let mut m = Mock::with_tiles(16, 16);
    let tiles = fan_tiles(&mut m, [4.5, 4.5], 3.0, 0.0, 0.0, 2);
    // three rays at angle 0: one line (the later rays end where the first did)
    assert_eq!(tiles, vec![[7, 4], [6, 4], [5, 4], [4, 4]]);
    assert_eq!(m.lines.len(), 1);
}

#[test]
fn a_launch_from_a_free_dock_faces_its_first_yaw() {
    use crate::uu_place::dock_ship_placement;
    let mut m = Mock::with_tiles(32, 32);
    let dock = m.add(10, 10, 94, 1, |_| {});
    let ship = m.add(20, 20, 95, 1, |c| {
        c.mInnerCollisionSphere = 1.0;
        c.mOuterCollisionSphere = 2.0;
    });
    let class = ship + 1;
    let mut t = GeTransformation::IDENTITY;
    assert!(dock_ship_placement(&mut m, [0.0; 3], class, dock, false, true, &mut t));
    // the step from the class's radius: half 1, inner 1, distance 2.5, step 0.2; the first yaw is the last launch
    // yaw (0) plus the step; the unit's next launch yaw one step on
    assert!((m.u(dock).cx.mLaunchYaw - 0.4).abs() < 1e-6);
    assert!(t.m[0][3] > 10.5);
}

// ---- the spot searches ----

#[test]
fn an_open_spot_is_the_nearest_free_point_around_the_target() {
    use crate::uu_spots::{find_open_spot, units_near_tile};
    let mut m = Mock::with_tiles(16, 16);
    let me = m.add(2, 5, 96, 1, |c| c.mInnerCollisionSphere = 1.0);
    let target = m.add(5, 5, 97, 2, |c| c.mInnerCollisionSphere = 1.0);
    m.units[0].position = [2.5, 5.5, 0.0];
    let near = units_near_tile(&mut m, [5.5, 5.5, 0.0], 2.5);
    assert_eq!(near, vec![target]);
    assert_eq!(units_near_tile(&mut m, [5.5, 5.5, 0.0], 3.0).len(), 2);
    let mut at = [0.0, 0.0, 9.0];
    assert!(find_open_spot(&mut m, me, target, &mut at));
    // the side toward the unit: (5.5 - 1, 5.5)
    assert_eq!(at, [4.5, 5.5, 9.0]);
    // a collidable blocker on that spot: the next nearest, the corner listed first of the two at the same distance
    // (the sort of eight is an insertion sort: ties keep the list's order)
    let b = m.add(4, 5, 98, 2, |c| c.mIsCollidable = true);
    m.units[2].position = [4.5, 5.5, 0.0];
    assert!(find_open_spot(&mut m, me, target, &mut at));
    assert_eq!(at, [4.5, 4.5, 9.0]);
    let _ = b;
}

#[test]
fn the_attack_part_is_the_nearest_piece() {
    use crate::uu_spots::attack_part;
    let mut m = Mock::with_tiles(16, 16);
    let me = m.add(1, 1, 99, 1, |_| {});
    let wall = m.add(5, 5, 100, 2, |_| {});
    // no links in the mock: the piece itself
    assert_eq!(attack_part(&mut m, me, wall), wall);
}

#[test]
fn a_resource_spot_prefers_the_units_path_area() {
    use crate::uu_spots::{find_next_free_tile, find_resource_spot, wall_spot};
    let mut m = Mock::with_tiles(16, 16);
    let me = m.add(9, 5, 101, 1, |_| {});
    let mine = m.add(7, 5, 102, 0, |_| {});
    let mut at = [0.0, 0.0, 0.0];
    assert!(find_resource_spot(&mut m, me, mine, &mut at));
    // the mine's own tile first (a small target), then its ring nearest the unit; the path area (x >= 8) first
    assert_eq!(at[0].floor(), 8.0);
    let mut at2 = [0.0; 3];
    assert!(find_next_free_tile(&mut m, me, mine, &mut at2));
    let mut at3 = [0.0; 3];
    assert!(wall_spot(&mut m, me, mine, &mut at3));
    assert_eq!(at2, at3);
}
