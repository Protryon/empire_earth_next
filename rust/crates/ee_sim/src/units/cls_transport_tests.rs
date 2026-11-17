//! cls_transport.rs's tests: the transports' bodies over a mock world (a small map of terrains and continent ids,
//! units with their records), each checked against the C++ body's statements worked by hand.

use super::*;
use crate::units::unit_fields::{EeUnitsComplexUnitFields, EeUnitsRtsUnitFields, EeUnitsUnitFields};
use std::collections::BTreeMap;

struct MockUnit {
    rts: Box<EeUnitsRtsUnitFields>,
    unit: Box<EeUnitsUnitFields>,
    cx: Box<EeUnitsComplexUnitFields>,
    class: Box<EeUnitsUnitClassFields>,
    player: u64,
    location: [f32; 3],
    container: u64,
    cargo: Vec<u64>,
    waypoints: Vec<Ge3DPoint>,
    goal: i32,
    queued: i32,
    load_goal: Option<Tile>,
}

#[derive(Default)]
struct Mock {
    /// (terrain, continent id) by tile
    tiles: BTreeMap<(i32, i32), (i32, u16)>,
    units: Vec<MockUnit>,
    /// the continents' categories by id (a continent's handle is its id + 0x1000)
    categories: BTreeMap<u16, u8>,
    links: Vec<(u16, u16)>,
    adjacent: BTreeMap<u16, Vec<u16>>,
    /// tiles a blocker stands on
    blocked: Vec<Tile>,
    /// the shore lists by (continent id, region id)
    shores: BTreeMap<(u16, u16), Vec<Tile>>,
    /// a path length for every question (10000: none)
    path: f32,
    /// the operations that write (set_location), in order
    log: Vec<String>,
}

const CONT: u64 = 0x1000;

impl Mock {
    fn map(w: i32, h: i32, terrain: i32, id: u16) -> Mock {
        let mut m = Mock { path: 1.0, ..Default::default() };
        for x in 0..w {
            for y in 0..h {
                m.tiles.insert((x, y), (terrain, id));
            }
        }
        m.categories.insert(id, TR_CONTINENT_LAND);
        m
    }
    fn set(&mut self, x: i32, y: i32, terrain: i32, id: u16) {
        self.tiles.insert((x, y), (terrain, id));
    }
    fn class(
        category: u32,
        unit_type: u32,
        f: impl FnOnce(&mut EeUnitsUnitClassFields),
    ) -> Box<EeUnitsUnitClassFields> {
        let mut c =
            EeUnitsUnitClassFields { mCategory: category as _, mUnitType: unit_type as _, ..Default::default() };
        f(&mut c);
        Box::new(c)
    }
    fn add(&mut self, x: i32, y: i32, class: Box<EeUnitsUnitClassFields>) -> u64 {
        let id = self.units.len() as u64 + 1;
        self.units.push(MockUnit {
            rts: Box::new(EeUnitsRtsUnitFields { mTileX: x as _, mTileY: y as _, ..Default::default() }),
            unit: Box::default(),
            cx: Box::new(EeUnitsComplexUnitFields { mIsCommandable: true, ..Default::default() }),
            class,
            player: 5,
            location: [x as f32 + 0.5, y as f32 + 0.5, 0.0],
            container: 0,
            cargo: Vec::new(),
            waypoints: Vec::new(),
            goal: -1,
            queued: 0,
            load_goal: None,
        });
        id * 0x100
    }
    fn u(&mut self, obj: u64) -> &mut MockUnit {
        &mut self.units[(obj / 0x100 - 1) as usize]
    }
    fn cls(&mut self, obj: u64) -> Cls {
        Cls(&*self.u(obj).class)
    }
}

impl TrHost for Mock {
    fn unit(&mut self, obj: u64) -> EeUnitsUuUnit {
        let u = self.u(obj);
        EeUnitsUuUnit {
            obj,
            rts: &*u.rts,
            unit: &*u.unit,
            cx: &*u.cx,
            class_fields: &*u.class,
            player: u.player,
            location: u.location,
            ..Default::default()
        }
    }
    fn class(&mut self, class: u64) -> *const EeUnitsUnitClassFields {
        class as *const EeUnitsUnitClassFields
    }
    fn map_tile(&mut self, x: i32, y: i32) -> u64 {
        if self.tiles.contains_key(&(x, y)) {
            0x10_0000 + (x as u64) * 0x100 + y as u64
        } else {
            0
        }
    }
    fn tile_terrain(&mut self, tile: u64) -> i32 {
        let (x, y) = (((tile - 0x10_0000) / 0x100) as i32, ((tile - 0x10_0000) % 0x100) as i32);
        self.tiles[&(x, y)].0
    }
    fn is_edge_tile(&mut self, x: i32, y: i32) -> bool {
        x == 0 || y == 0
    }
    fn tile_visible_to(&mut self, _tile: u64, player: u64) -> bool {
        player == 5
    }
    fn tile_location(&mut self, x: i32, y: i32) -> Ge3DPoint {
        Ge3DPoint { x: x as f32 + 0.5, y: y as f32 + 0.5, z: 0.0 }
    }
    fn continent_id(&mut self, x: i32, y: i32) -> u16 {
        self.tiles.get(&(x, y)).map_or(0, |t| t.1)
    }
    fn continent_at(&mut self, x: i32, y: i32) -> u64 {
        match self.continent_id(x, y) {
            0 => 0,
            id => CONT + u64::from(id),
        }
    }
    fn continent_number(&mut self, continent: u64) -> u16 {
        (continent - CONT) as u16
    }
    fn continent_category(&mut self, continent: u64) -> u8 {
        self.categories.get(&((continent - CONT) as u16)).copied().unwrap_or(0)
    }
    fn continent_by_id(&mut self, id: u16) -> u64 {
        if self.categories.contains_key(&id) {
            CONT + u64::from(id)
        } else {
            0
        }
    }
    fn continent_of(&mut self, x: i32, y: i32, _kind: i32) -> u16 {
        // the region of a tile: its id + 100 on water
        match self.tiles.get(&(x, y)) {
            Some(&(TR_TERRAIN_DEEP_WATER, id)) => id,
            Some(_) => 0,
            None => 0,
        }
    }
    fn continent_link(&mut self, row: u16, column: u16) -> bool {
        self.links.contains(&(row, column))
    }
    fn next_to_area(&mut self, x: i32, y: i32, id: u16) -> bool {
        ring_tiles(x, y).iter().chain(std::iter::once(&[x, y])).any(|t| self.continent_id(t[0], t[1]) == id)
    }
    fn adjacent_ids(&mut self, id: u16) -> Vec<u16> {
        self.adjacent.get(&id).cloned().unwrap_or_default()
    }
    fn shore_in_region(&mut self, continent: u64, id: u16, _kind: i32) -> Vec<Tile> {
        self.shores.get(&((continent - CONT) as u16, id)).cloned().unwrap_or_default()
    }
    fn shore_next_to(&mut self, continent: u64, id: u16) -> Vec<Tile> {
        self.shores.get(&((continent - CONT) as u16, id)).cloned().unwrap_or_default()
    }
    fn blocked(&mut self, _player: u64, _unit_type: u32, tile: u64) -> bool {
        let (x, y) = (((tile - 0x10_0000) / 0x100) as i32, ((tile - 0x10_0000) % 0x100) as i32);
        self.blocked.contains(&[x, y])
    }
    fn line_blocked(&mut self, _x1: i32, _y1: i32, _x2: i32, _y2: i32) -> bool {
        false
    }
    fn tile_line(&mut self, from: Tile, to: Tile) -> Vec<Tile> {
        // a straight line along x or y (the tests' lines)
        let (dx, dy) = ((to[0] - from[0]).signum(), (to[1] - from[1]).signum());
        let n = (to[0] - from[0]).abs().max((to[1] - from[1]).abs());
        (0..=n).map(|i| [from[0] + dx * i, from[1] + dy * i]).collect()
    }
    fn tiles_in_radius(&mut self, at: Tile) -> Vec<Tile> {
        let mut out = Vec::new();
        for x in -2..=2 {
            for y in -2..=2 {
                out.push([at[0] + x, at[1] + y]);
            }
        }
        out
    }
    fn line_tiles(&mut self, from: Tile, to: Tile) -> Vec<Tile> {
        self.tile_line(from, to)
    }
    fn path_length(&mut self, _unit: u64, _tile: &mut Tile, _walls: bool) -> f32 {
        self.path
    }
    fn set_location(&mut self, unit: u64, point: Ge3DPoint) {
        self.log.push(format!("set {unit:x} {} {}", point.x, point.y));
    }
    fn surrounding_tiles(&mut self, unit: u64) -> Vec<Tile> {
        let t = Un(self.unit(unit)).tile();
        ring_tiles(t[0], t[1]).to_vec()
    }
    fn load_goal_tile(&mut self, unit: u64) -> Option<Tile> {
        self.u(unit).load_goal
    }
    fn cargo(&mut self, unit: u64, index: i32) -> u64 {
        self.u(unit).cargo[index as usize]
    }
    fn container(&mut self, unit: u64) -> u64 {
        self.u(unit).container
    }
    fn waypoints(&mut self, unit: u64) -> i32 {
        self.u(unit).waypoints.len() as i32
    }
    fn next_waypoint(&mut self, unit: u64) -> Ge3DPoint {
        self.u(unit).waypoints.first().copied().unwrap_or_default()
    }
    fn queue_goals(&mut self, unit: u64, _index: i32) -> i32 {
        self.u(unit).queued
    }
    fn goal_kind(&mut self, unit: u64) -> i32 {
        self.u(unit).goal
    }
}

fn land_transport(m: &mut Mock, x: i32, y: i32) -> u64 {
    m.add(
        x,
        y,
        Mock::class(TR_CATEGORY_LAND_TRANSPORT, 0x50, |c| {
            c.mLoadRange = 2.0;
            c.mUnloadRange = 3.0;
            c.mMaxVelocity = 2.0;
        }),
    )
}

fn soldier(m: &mut Mock, x: i32, y: i32) -> u64 {
    m.add(
        x,
        y,
        Mock::class(8, 0x10, |c| {
            c.mCanEnterLandTransports = true;
            c.mCanEnterSeaTransports = true;
            c.mMaxVelocity = 1.0;
        }),
    )
}

#[test]
fn the_ring_lists_the_sides_then_the_corners() {
    assert_eq!(ring_tiles(5, 5), [[5, 6], [5, 4], [6, 5], [4, 5], [4, 6], [6, 6], [6, 4], [4, 4]]);
}

#[test]
fn tile_distances_square_in_long_and_sum_as_floats() {
    assert_eq!(tile_distance_squared([0, 0], [3, 4]), 25.0);
    assert_eq!(tile_distance_long([1, 1], [4, 5]), 25);
    assert_eq!(tile_distance_squared([TR_NONE, 0], [0, 0]), 1.0e8);
}

#[test]
fn the_gathering_tile_is_the_nearest_unit_first_found_on_ties() {
    let mut m = Mock::map(10, 10, 0, 1);
    let a = soldier(&mut m, 5, 5);
    let b = soldier(&mut m, 2, 2);
    let c = soldier(&mut m, 2, 2);
    let mut out = [9, 9];
    assert_eq!(gathering_tile(&mut m, &[a, b, c], [1, 1], &mut out), 1);
    assert_eq!(out, [2, 2]);
    let mut out = [9, 9];
    assert_eq!(gathering_tile(&mut m, &[], [1, 1], &mut out), 0);
    assert_eq!(out, [9, 9]);
}

#[test]
fn the_carry_tests_read_the_cargo_class_and_the_continents() {
    let mut m = Mock::map(10, 10, 0, 1);
    let t = land_transport(&mut m, 3, 3);
    let s = soldier(&mut m, 4, 4);
    let cls = m.cls(t);
    assert!(can_carry(&mut m, cls, t, s));
    m.set(4, 4, 0, 2);
    assert!(!can_carry(&mut m, cls, t, s));
    m.u(s).rts.mTileX = TR_NONE as _;
    assert!(!can_carry(&mut m, cls, t, s));
    // a plain class carries nothing
    let plain = Cls(&*Box::leak(Mock::class(8, 0x10, |_| {})));
    assert!(!can_carry(&mut m, plain, t, s));
}

#[test]
fn naval_carry_needs_connected_continents() {
    let mut m = Mock::map(10, 10, 0, 1);
    let ship = m.add(3, 3, Mock::class(TR_CATEGORY_NAVAL_TRANSPORT, 0x60, |_| {}));
    let s = soldier(&mut m, 8, 8);
    m.set(8, 8, 0, 2);
    m.categories.insert(2, TR_CONTINENT_LAND);
    m.categories.insert(3, TR_CONTINENT_WATER);
    let cls = m.cls(ship);
    assert!(!can_carry(&mut m, cls, ship, s));
    // continent 1 borders water 3, which links to continent 2 (mTileFlags[to * w + neighbour])
    m.adjacent.insert(1, vec![3]);
    m.links.push((2, 3));
    assert!(can_carry(&mut m, cls, ship, s));
    // a cliff neighbour does not connect
    m.categories.insert(3, TR_CONTINENT_CLIFF);
    assert!(!can_carry(&mut m, cls, ship, s));
}

#[test]
fn unload_tests_by_kind() {
    let mut m = Mock::map(10, 10, 0, 1);
    m.set(5, 5, TR_TERRAIN_CLIFF, 1);
    m.set(6, 6, 0, 2);
    let t = land_transport(&mut m, 3, 3);
    let cls = m.cls(t);
    assert!(can_unload_at(&mut m, cls, t, 4, 4));
    assert!(!can_unload_at(&mut m, cls, t, 5, 5), "a cliff");
    assert!(!can_unload_at(&mut m, cls, t, 0, 4), "a map edge");
    assert!(!can_unload_at(&mut m, cls, t, 6, 6), "another continent");
    assert!(!can_unload_at(&mut m, cls, t, 20, 4), "off the map");
    // the fortress: within its unload range (3) of the transport's location
    let f = m.add(3, 3, Mock::class(TR_CATEGORY_FORTRESS, 0x70, |c| c.mUnloadRange = 3.0));
    let fc = m.cls(f);
    assert!(can_unload_at(&mut m, fc, f, 5, 3));
    assert!(!can_unload_at(&mut m, fc, f, 7, 3));
    // a teleporter: any visible tile
    let tp = m.add(3, 3, Mock::class(TR_CATEGORY_FORTRESS, TR_TYPE_TELEPORTER, |_| {}));
    let tc = m.cls(tp);
    assert!(can_unload_at(&mut m, tc, tp, 6, 6));
    m.u(tp).player = 6;
    assert!(!can_unload_at(&mut m, tc, tp, 6, 6));
    // the helicopter: not deep water
    let h = m.add(3, 3, Mock::class(TR_CATEGORY_HELICOPTER_TRANSPORT, 0x71, |_| {}));
    let hc = m.cls(h);
    m.set(7, 7, TR_TERRAIN_DEEP_WATER, 3);
    assert!(!can_unload_at(&mut m, hc, h, 7, 7));
    assert!(can_unload_at(&mut m, hc, h, 6, 6));
}

#[test]
fn a_naval_transport_unloads_where_its_water_links() {
    let mut m = Mock::map(10, 10, 0, 1);
    for y in 0..10 {
        m.set(5, y, TR_TERRAIN_DEEP_WATER, 3);
    }
    m.categories.insert(3, TR_CONTINENT_WATER);
    m.categories.insert(2, TR_CONTINENT_LAND);
    for x in 6..10 {
        for y in 0..10 {
            m.set(x, y, 0, 2);
        }
    }
    let ship = m.add(5, 4, Mock::class(TR_CATEGORY_NAVAL_TRANSPORT, 0x60, |_| {}));
    let cls = m.cls(ship);
    assert!(!can_unload_at(&mut m, cls, ship, 7, 4));
    m.links.push((3, 2));
    assert!(can_unload_at(&mut m, cls, ship, 7, 4));
    assert!(!can_unload_at(&mut m, cls, ship, 5, 3), "deep water");
}

#[test]
fn can_load_by_path_or_next_to_the_transport() {
    let mut m = Mock::map(10, 10, 0, 1);
    let t = land_transport(&mut m, 3, 3);
    let s = soldier(&mut m, 4, 3);
    let cls = m.cls(t);
    m.path = 1.5;
    assert!(can_load(&mut m, cls, t, s), "1.5 * 1.5 <= 4");
    m.path = 3.0;
    assert!(can_load(&mut m, cls, t, s), "a surrounding tile");
    m.u(s).rts.mTileX = 6;
    assert!(!can_load(&mut m, cls, t, s));
}

#[test]
fn can_load_across_water_through_the_goal_tile() {
    let mut m = Mock::map(10, 10, 0, 1);
    m.set(6, 3, 0, 2);
    let t = land_transport(&mut m, 3, 3);
    let s = soldier(&mut m, 6, 3);
    let cls = m.cls(t);
    m.u(t).load_goal = Some([5, 3]);
    m.path = 1.0;
    // the unit is 1.5 from the tile (2.25 < 4), the transport 2.5 more: 6.25 + 2.25 >= 4
    assert!(!can_load(&mut m, cls, t, s));
    m.u(t).location = [5.0, 3.0, 0.0];
    assert!(can_load(&mut m, cls, t, s));
    m.path = TR_NO_PATH;
    assert!(can_load(&mut m, cls, t, s), "close enough with a clear line");
}

#[test]
fn can_unload_near_tries_the_cargo_on_the_tile_and_takes_it_off() {
    let mut m = Mock::map(10, 10, 0, 1);
    let t = land_transport(&mut m, 3, 3);
    let s = soldier(&mut m, 4, 4);
    let cls = m.cls(t);
    m.path = 2.0;
    assert!(can_unload_near(&mut m, cls, t, s, [3, 3], [3, 3], [5, 5]));
    assert_eq!(m.log, vec![format!("set {s:x} 3.5 3.5"), format!("set {s:x} -10000 -10000")]);
    m.path = 3.0;
    assert!(!can_unload_near(&mut m, cls, t, s, [3, 3], [3, 3], [5, 5]), "9 is not under 9");
    // across water: the transport's distance to the first cargo tile counts
    m.set(5, 5, 0, 2);
    m.path = 1.0;
    m.log.clear();
    assert!(can_unload_near(&mut m, cls, t, s, [3, 3], [4, 4], [5, 5]));
    assert!(!can_unload_near(&mut m, cls, t, s, [3, 3], [6, 6], [5, 5]));
    assert_eq!(m.log.len(), 2, "the far tile is never tried");
}

#[test]
fn the_landing_tile_from_deep_water_is_the_free_ring_tile_nearest_the_target() {
    let mut m = Mock::map(10, 10, 0, 1);
    for y in 0..10 {
        m.set(4, y, TR_TERRAIN_DEEP_WATER, 3);
    }
    let ship = m.add(4, 4, Mock::class(TR_CATEGORY_NAVAL_TRANSPORT, 0x60, |_| {}));
    let cls = m.cls(ship);
    // the ring tiles next to the water continent 3: x 3 and 5
    assert_eq!(landing_tile(&mut m, cls, ship, [4, 4], [8, 4]), Some([5, 4]));
    m.blocked.push([5, 4]);
    // (5, 5) and (5, 3) are equally near: a tie goes to the later ring tile (`<=`)
    assert_eq!(landing_tile(&mut m, cls, ship, [4, 4], [8, 4]), Some([5, 3]));
    // shallow water and land: the unload tile itself; a cliff: the transport's tile; off the map: nothing
    assert_eq!(landing_tile(&mut m, cls, ship, [2, 2], [8, 4]), Some([2, 2]));
    m.set(2, 2, TR_TERRAIN_CLIFF, 1);
    assert_eq!(landing_tile(&mut m, cls, ship, [2, 2], [8, 4]), Some([4, 4]));
    assert_eq!(landing_tile(&mut m, cls, ship, [20, 2], [8, 4]), None);
}

#[test]
fn the_best_shore_tile_scans_from_the_last_tile_down() {
    let mut m = Mock::map(10, 10, 0, 1);
    for y in 0..10 {
        m.set(5, y, TR_TERRAIN_DEEP_WATER, 3);
    }
    for x in 6..10 {
        for y in 0..10 {
            m.set(x, y, 0, 2);
        }
    }
    m.categories.insert(2, TR_CONTINENT_LAND);
    m.categories.insert(3, TR_CONTINENT_WATER);
    let ship = m.add(5, 2, Mock::class(TR_CATEGORY_NAVAL_TRANSPORT, 0x60, |_| {}));
    // continent 2's shore in water region 3: two tiles, equally far from the target: the earlier one wins (the scan
    // runs from the last down and keeps ties)
    m.shores.insert((2, 3), vec![[6, 4], [6, 6]]);
    let mut out = [0, 0];
    let d = best_shore_tile(&mut m, ship, [5, 2], [8, 5], [8, 5], false, &mut out);
    assert_eq!(out, [6, 4]);
    assert_eq!(d, ee_render::ge::um::sqrt(5.0));
    // none: -10000 and 0
    m.shores.clear();
    let mut out = [0, 0];
    assert_eq!(best_shore_tile(&mut m, ship, [5, 2], [8, 5], [8, 5], false, &mut out), 0.0);
    assert_eq!(out, [TR_NONE, TR_NONE]);
}

#[test]
fn filtering_keeps_the_order_of_both_lists() {
    let mut m = Mock::map(10, 10, 0, 1);
    let t = land_transport(&mut m, 3, 3);
    let a = soldier(&mut m, 4, 4);
    let b = soldier(&mut m, 5, 5);
    let c = soldier(&mut m, 6, 6);
    let d = soldier(&mut m, 7, 7);
    m.u(b).cx.mCalamityFlags = 0x10;
    m.u(d).container = t;
    let cls = m.cls(t);
    let (mut units, mut rejected) = (vec![a, b, c, d], vec![99]);
    filter_loadable(&mut m, cls, &mut units, t, &mut rejected);
    assert_eq!(units, vec![a, c]);
    assert_eq!(rejected, vec![99, b, d]);
}

#[test]
fn the_land_rendezvous_splits_the_line_by_speed() {
    let mut m = Mock::map(20, 10, 0, 1);
    let t = land_transport(&mut m, 2, 5);
    let s = soldier(&mut m, 11, 5);
    let cls = m.cls(t);
    let (mut a, mut b) = ([0, 0], [0, 0]);
    rendezvous_tiles(&mut m, cls, &[s], t, &mut a, &mut b);
    // ten line tiles, the transport's share 2 / 3: index 6
    assert_eq!(a, [8, 5]);
    assert_eq!(b, [8, 5]);
}

#[test]
fn meeting_tiles_fall_back_to_the_nearest_open_tile() {
    let mut m = Mock::map(10, 10, 0, 1);
    let t = land_transport(&mut m, 2, 2);
    let s = soldier(&mut m, 6, 6);
    let cls = m.cls(t);
    m.set(6, 6, 0, 2);
    let (mut io_t, mut io_u) = ([2, 2], [6, 6]);
    meeting_tiles(&mut m, cls, t, s, &mut io_t, &mut io_u);
    // the nearest tile on the transport's continent within two: (5, 5)... the sort's first at distance 1 is (5, 6)
    assert_eq!(io_u, [5, 6]);
}

#[test]
fn unload_tiles_by_kind() {
    let mut m = Mock::map(10, 10, 0, 1);
    let f = m.add(3, 3, Mock::class(TR_CATEGORY_FORTRESS, 0x70, |_| {}));
    let cls = m.cls(f);
    let mut out = [[0, 0]; 3];
    assert!(unload_tiles(&mut m, cls, f, [7, 7], &mut out));
    assert_eq!(out, [[3, 3]; 3]);
    let t = land_transport(&mut m, 3, 3);
    let cls = m.cls(t);
    assert!(unload_tiles(&mut m, cls, t, [7, 7], &mut out));
    assert_eq!(out, [[7, 7]; 3]);
    let plain = Cls(&*Box::leak(Mock::class(8, 0x10, |_| {})));
    let mut out = [[1, 1]; 3];
    assert!(!unload_tiles(&mut m, plain, t, [7, 7], &mut out));
    assert_eq!(out, [[1, 1]; 3]);
}

#[test]
fn a_naval_transport_arrives_at_its_last_tile_at_speed_or_slow() {
    let mut m = Mock::map(10, 10, TR_TERRAIN_DEEP_WATER, 3);
    let ship = m.add(4, 4, Mock::class(TR_CATEGORY_NAVAL_TRANSPORT, 0x60, |_| {}));
    m.u(ship).cx.mCollisionRadius = 0.5;
    assert!(naval_has_arrived(&mut m, ship, 0.01), "slow");
    assert!(!naval_has_arrived(&mut m, ship, 0.3));
    m.u(ship).waypoints = vec![Ge3DPoint { x: 4.2, y: 4.7, z: 0.0 }, Ge3DPoint::default()];
    assert!(naval_has_arrived(&mut m, ship, 0.3));
    assert!(!naval_has_arrived(&mut m, ship, 0.6));
    m.u(ship).cx.mCollisionState = 8;
    assert!(naval_has_arrived(&mut m, ship, 0.6), "sinking");
}
