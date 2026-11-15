//! cls_building.rs's tests over a mock world: a terrain grid, a mesh of corner heights, units with their records.

use super::*;
use crate::unit_class_fields::EeUnitsUnitClassFields;
use crate::unit_fields::{EeUnitsRtsUnitFields, EeUnitsUnitFields};
use std::collections::BTreeMap;

struct MockUnit {
    rts: Box<EeUnitsRtsUnitFields>,
    unit: Box<EeUnitsUnitFields>,
    class: Box<EeUnitsUnitClassFields>,
    player: u64,
    player_id: i32,
    location: [f32; 3],
    max_velocity: f32,
    next: u64,
}

#[derive(Default)]
struct Mock {
    size: i32,
    terrain: BTreeMap<(i32, i32), i32>,
    raised: BTreeMap<(i32, i32), i32>,
    z: BTreeMap<(i32, i32), f32>,
    space: bool,
    units: Vec<MockUnit>,
    /// the set 0x62's unit types; the hostile (player, other id) pairs
    set: Vec<u32>,
    enemies: Vec<(u64, i32)>,
}

impl Mock {
    fn new(size: i32, terrain: i32) -> Mock {
        let mut m = Mock { size, ..Default::default() };
        for x in 0..size {
            for y in 0..size {
                m.terrain.insert((x, y), terrain);
            }
        }
        m
    }
    fn add(&mut self, x: f32, y: f32, player: i32, unit_type: u32, sphere: f32) -> u64 {
        let class = EeUnitsUnitClassFields {
            mUnitType: unit_type as _,
            mInnerCollisionSphere: sphere,
            mDomains: MOVEMENT_LAND,
            ..Default::default()
        };
        self.units.push(MockUnit {
            rts: Box::default(),
            unit: Box::new(EeUnitsUnitFields { mHitPoints: 10, ..Default::default() }),
            class: Box::new(class),
            player: 100 + player as u64,
            player_id: player,
            location: [x, y, 0.0],
            max_velocity: 1.0,
            next: 0,
        });
        self.units.len() as u64 * 0x100
    }
    fn u(&mut self, obj: u64) -> &mut MockUnit {
        &mut self.units[(obj / 0x100 - 1) as usize]
    }
}

impl BuildingHost for Mock {
    fn unit(&mut self, obj: u64) -> EeUnitsUuUnit {
        let u = self.u(obj);
        EeUnitsUuUnit {
            obj,
            rts: &*u.rts,
            unit: &*u.unit,
            class_fields: &*u.class,
            complex: obj,
            player: u.player,
            player_id: u.player_id,
            location: u.location,
            stats_max_velocity: u.max_velocity,
            ..Default::default()
        }
    }
    fn map_tile(&mut self, x: i32, y: i32) -> u64 {
        if (0..self.size).contains(&x) && (0..self.size).contains(&y) {
            0x10000 + (x as u64) * 0x100 + y as u64
        } else {
            0
        }
    }
    fn tile_terrain(&mut self, tile: u64) -> i32 {
        let (x, y) = (((tile - 0x10000) / 0x100) as i32, ((tile - 0x10000) % 0x100) as i32);
        self.terrain[&(x, y)]
    }
    fn raised_corners(&mut self, x: i32, y: i32) -> i32 {
        self.raised.get(&(x, y)).copied().unwrap_or(0)
    }
    fn mesh_z(&mut self, x: i32, y: i32) -> f32 {
        self.z.get(&(x, y)).copied().unwrap_or(0.0)
    }
    fn is_space_map(&mut self) -> bool {
        self.space
    }
    fn map_size(&mut self) -> i32 {
        self.size
    }
    fn continent_at(&mut self, _x: i32, _y: i32) -> u64 {
        7
    }
    fn hull_center(&mut self, _continent: u64) -> Tile {
        [2, 2]
    }
    fn position(&mut self, unit: u64) -> [f32; 3] {
        self.u(unit).location
    }
    fn units_near(&mut self, _point: [f32; 3], _radius: f32) -> Vec<u64> {
        (1..=self.units.len() as u64).map(|i| i * 0x100).collect()
    }
    fn unit_set_contains(&mut self, unit_type: u32, set: i32) -> bool {
        set == GATE_SET && self.set.contains(&unit_type)
    }
    fn enemy(&mut self, player: u64, other_id: i32) -> bool {
        self.enemies.contains(&(player, other_id))
    }
    fn part(&mut self, complex: u64, next: bool) -> u64 {
        assert!(next);
        self.u(complex).next
    }
}

#[test]
fn a_tile_faces_away_from_the_map_middle() {
    let mut m = Mock::new(20, 0);
    // the middle is (10, 10): a tile below it faces north (0: the middle lies +y), one to its left faces 3
    assert_eq!(facing_quarter(&mut m, 10, 2), 0);
    assert_eq!(facing_quarter(&mut m, 10, 18), 1);
    assert_eq!(facing_quarter(&mut m, 2, 10), 2);
    assert_eq!(facing_quarter(&mut m, 18, 10), 3);
    assert_eq!(facing_quarter(&mut m, 10, 10), 2, "the middle itself");
    // a space map: the continent's hull centre
    m.space = true;
    assert_eq!(facing_quarter(&mut m, 2, 0), 0);
}

#[test]
fn terrain_runs_count_until_the_terrain_changes() {
    let mut m = Mock::new(10, 0);
    for x in 5..10 {
        m.terrain.insert((x, 4), TERRAIN_DEEP_WATER);
    }
    assert_eq!(terrain_run(&mut m, 4, 4, 1, 0, 5, TERRAIN_DEEP_WATER), (true, 5));
    assert_eq!(terrain_run(&mut m, 6, 4, 1, 0, 5, TERRAIN_DEEP_WATER), (false, 3), "the map ends");
    assert_eq!(terrain_run(&mut m, 0, 0, -1, 0, 5, 0), (false, 0), "the unsigned coordinate wraps off the map");
}

#[test]
fn a_navy_yard_faces_the_water() {
    let mut m = Mock::new(20, 0);
    // its quarter (0: +y) has no water; the next side (+x) has 5 tiles of it
    for x in 11..17 {
        m.terrain.insert((x, 2), TERRAIN_DEEP_WATER);
    }
    assert_eq!(navy_yard_facing(&mut m, 10, 2), 2);
    // no side has a full run: the longest
    let mut m = Mock::new(20, 0);
    m.terrain.insert((9, 2), TERRAIN_DEEP_WATER);
    m.terrain.insert((8, 2), TERRAIN_DEEP_WATER);
    assert_eq!(navy_yard_facing(&mut m, 10, 2), 3);
}

#[test]
fn a_dock_faces_the_most_space() {
    let mut m = Mock::new(20, 0);
    for y in 0..10 {
        m.terrain.insert((11, y), TERRAIN_SPACE);
        m.terrain.insert((12, y), TERRAIN_SPACE);
    }
    for x in 11..17 {
        m.terrain.insert((x, 5), TERRAIN_SPACE);
    }
    assert_eq!(dock_facing(&mut m, 10, 5), 2);
}

#[test]
fn gate_pieces_turn_and_line_up() {
    assert_eq!(gate_part_turn(2, 4), (9, Some(std::f32::consts::PI)));
    assert_eq!(gate_part_turn(3, 0), (10, Some(0.78539819)));
    assert_eq!(gate_part_turn(1, 2), (11, None));
    assert_eq!(gate_part_turn(7, 4), (11, None));
    assert_eq!(wall_gate_tiles([5, 5], 3), vec![[7, 3], [6, 4], [5, 5], [4, 6], [3, 7]]);
    assert_eq!(wall_gate_tiles([5, 5], 2), vec![[5, 3], [5, 4], [5, 5], [5, 6], [5, 7]]);
    assert!(wall_gate_tiles([5, 5], 0).is_empty());
}

#[test]
fn a_gate_line_is_level_on_flat_tiles_or_ramps_within_0_6() {
    let mut m = Mock::new(10, 0);
    let tiles = wall_gate_tiles([5, 5], 1);
    assert!(tiles_level(&mut m, &tiles));
    m.z.insert((8, 5), 0.6);
    m.raised.insert((7, 5), 2);
    assert!(tiles_level(&mut m, &tiles));
    m.z.insert((8, 5), 0.7);
    assert!(!tiles_level(&mut m, &tiles));
    m.z.clear();
    m.raised.insert((7, 5), 1);
    assert!(!tiles_level(&mut m, &tiles), "one raised corner");
    assert!(!tiles_level(&mut m, &wall_gate_tiles([8, 5], 1)), "off the map");
}

#[test]
fn a_gate_opens_for_friends_and_against_enemies_near_it() {
    let mut m = Mock::new(10, 0);
    let gate = m.add(5.0, 5.0, 1, 0x200, 0.5);
    let door = m.add(6.0, 5.0, 1, 0x200, 0.5);
    let post = m.add(7.0, 5.0, 1, 0x200, 0.5);
    m.u(gate).next = door;
    m.u(door).next = post;
    // the pieces themselves are near, but they do not move
    for piece in [gate, door, post] {
        m.u(piece).max_velocity = 0.0;
    }
    // a friendly soldier 1.2 from the post (range (0.25 + 1)^2 = 1.5625)
    let soldier = m.add(8.2, 5.0, 1, 0x10, 0.5);
    assert!(passer_near(&mut m, gate));
    assert!(!enemies_near(&mut m, gate));
    // standing still it does not pass; an enemy of the set does, for both tests
    m.u(soldier).max_velocity = 0.0;
    assert!(!passer_near(&mut m, gate));
    m.u(soldier).player = 102;
    m.u(soldier).player_id = 2;
    m.enemies.push((101, 2));
    m.enemies.push((102, 1));
    assert!(!passer_near(&mut m, gate));
    m.set.push(0x10);
    assert!(passer_near(&mut m, gate));
    assert!(enemies_near(&mut m, gate));
    // too far
    m.u(soldier).location = [9.5, 5.0, 0.0];
    assert!(!enemies_near(&mut m, gate));
}
