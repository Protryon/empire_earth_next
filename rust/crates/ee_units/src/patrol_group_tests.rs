//! patrol_group.rs over a mock world: units on tiles, a file of words, a log of the world calls.

use std::collections::BTreeMap;

use super::*;

const G: u64 = 0x1000;

#[derive(Default)]
struct Mock {
    points: BTreeMap<u64, Vec<u64>>,
    fields: BTreeMap<u64, GroupFields>,
    tiles: BTreeMap<u64, (i64, i64)>,
    groups: BTreeMap<u64, u64>,
    ids: BTreeMap<u64, u32>,
    next_unit: u64,
    log: Vec<String>,
    file: Vec<u32>,
    read_at: usize,
    fail_read_at: Option<usize>,
    refuse_select: Option<u64>,
}

impl Mock {
    fn new() -> Mock {
        Mock { next_unit: 0x100, ..Default::default() }
    }
    fn unit(&mut self, x: i64, y: i64) -> u64 {
        let u = self.next_unit;
        self.next_unit += 0x10;
        self.tiles.insert(u, (x, y));
        self.ids.insert(u, u as u32 + 7);
        u
    }
    fn with_points(&mut self, tiles: &[(i64, i64)]) -> Vec<u64> {
        let v: Vec<u64> = tiles.iter().map(|&(x, y)| self.unit(x, y)).collect();
        for &u in &v {
            self.groups.insert(u, G);
        }
        self.points.insert(G, v.clone());
        v
    }
}

impl PatrolGroupHost for Mock {
    fn points(&mut self, group: u64) -> Vec<u64> {
        self.points.get(&group).cloned().unwrap_or_default()
    }
    fn set_points(&mut self, group: u64, points: &[u64]) {
        self.points.insert(group, points.to_vec());
    }
    fn fields(&mut self, group: u64) -> GroupFields {
        self.fields.get(&group).copied().unwrap_or_default()
    }
    fn set_fields(&mut self, group: u64, fields: GroupFields) {
        self.fields.insert(group, fields);
    }
    fn unit_tile(&mut self, unit: u64) -> (i64, i64) {
        self.tiles[&unit]
    }
    fn distance_squared_to_tile(&mut self, unit: u64, x: i64, y: i64) -> f32 {
        let (ux, uy) = self.tiles[&unit];
        ((ux - x) * (ux - x) + (uy - y) * (uy - y)) as f32
    }
    fn tile_at(&mut self, x: f32, y: f32, probe: u64) -> (f32, bool) {
        let tile = (x as i64, y as i64);
        let has = probe != 0 && self.tiles.get(&probe) == Some(&tile);
        ((tile.0 + tile.1) as f32 * 0.5, has)
    }
    fn create_point(&mut self, group: u64, x: f32, y: f32, player: u32) -> u64 {
        let u = self.unit(x as i64, y as i64);
        self.groups.insert(u, group);
        self.log.push(format!("create {u:x} {x} {y} p{player}"));
        u
    }
    fn set_patrol_group(&mut self, unit: u64, group: u64) {
        self.groups.insert(unit, group);
        self.log.push(format!("group {unit:x} {group:x}"));
    }
    fn patrol_group_of(&mut self, unit: u64) -> u64 {
        self.groups.get(&unit).copied().unwrap_or(0)
    }
    fn kill_unit(&mut self, unit: u64, kill: bool) {
        self.log.push(format!("kill {unit:x} {kill}"));
    }
    fn remove_from_player(&mut self, unit: u64, group: u64) {
        self.log.push(format!("remove {unit:x} {group:x}"));
    }
    fn try_select(&mut self, player: u64, unit: u64, flag: bool) -> bool {
        self.log.push(format!("select {player:x} {unit:x} {flag}"));
        self.refuse_select != Some(unit)
    }
    fn unit_id(&mut self, unit: u64) -> u32 {
        self.ids[&unit]
    }
    fn find_unit(&mut self, id: u32) -> u64 {
        self.ids.iter().find(|(_, &i)| i == id).map(|(&u, _)| u).unwrap_or(0)
    }
    fn live_state(&mut self, major: u32, minor: u32) -> bool {
        (major, minor) >= (0, 254)
    }
    fn write_u32(&mut self, value: u32) -> i32 {
        self.file.push(value);
        0
    }
    fn write_bool(&mut self, value: bool) -> i32 {
        self.file.push(value as u32);
        0
    }
    fn read_u32(&mut self, value: &mut u32) -> i32 {
        if self.fail_read_at == Some(self.read_at) || self.read_at >= self.file.len() {
            return 5;
        }
        *value = self.file[self.read_at];
        self.read_at += 1;
        0
    }
    fn read_bool(&mut self, value: &mut bool) -> i32 {
        let mut v = 0;
        let r = self.read_u32(&mut v);
        if r == 0 {
            *value = v != 0;
        }
        r
    }
}

#[test]
fn nearest_takes_the_first_of_equals() {
    let mut m = Mock::new();
    let p = m.with_points(&[(10, 10), (2, 2), (4, 0), (0, 4)]);
    assert_eq!(nearest(G, 2, 2, &mut m), p[1]);
    // (4,0) and (0,4) are equally near (2,2)... from (2,2) itself p[1]; from (2,0): (4,0) at 4, (2,2) at 4: first
    assert_eq!(nearest(G, 2, 0, &mut m), p[1]);
    assert_eq!(nearest(0x9999, 0, 0, &mut m), 0);
}

#[test]
fn next_and_previous_wrap_by_loop() {
    let mut m = Mock::new();
    let p = m.with_points(&[(0, 0), (5, 0), (9, 0)]);
    assert_eq!(next(G, 0, &mut m), p[0]);
    assert_eq!(next(G, p[0], &mut m), p[1]);
    assert_eq!(next(G, p[2], &mut m), 0);
    assert_eq!(previous(G, p[0], &mut m), 0);
    assert_eq!(previous(G, p[2], &mut m), p[1]);
    m.fields.insert(G, GroupFields { is_loop: true, ..Default::default() });
    assert_eq!(next(G, p[2], &mut m), p[0]);
    assert_eq!(previous(G, p[0], &mut m), p[2]);
    // a unit not on the route: its nearest point's index
    let stray = m.unit(8, 1);
    assert_eq!(index_of(G, stray, &mut m), 2);
    assert_eq!(next(G, stray, &mut m), p[0]);
    assert_eq!(previous(G, stray, &mut m), p[1]);
}

#[test]
fn one_point_patrol_returns_to_its_point() {
    let mut m = Mock::new();
    let p = m.with_points(&[(3, 3)]);
    assert_eq!(next(G, p[0], &mut m), p[0]);
    assert_eq!(previous(G, p[0], &mut m), 0);
    assert_eq!(last(G, &mut m), p[0]);
}

#[test]
fn add_point_appends_or_closes_the_loop() {
    let mut m = Mock::new();
    add_point(G, 3.7, 4.2, 2, &mut m);
    add_point(G, 6.0, 1.0, 2, &mut m);
    assert_eq!(m.points[&G].len(), 2);
    assert!(!m.fields(G).is_loop);
    add_point(G, 3.1, 4.9, 2, &mut m);
    assert_eq!(m.points[&G].len(), 2);
    assert!(m.fields(G).is_loop);
    assert_eq!(m.log[0], format!("create {:x} 3.7 4.2 p2", m.points[&G][0]));
}

#[test]
fn insert_goes_after_the_given_point() {
    let mut m = Mock::new();
    let p = m.with_points(&[(0, 0), (5, 0)]);
    insert_point_at(G, 2.0, 0.0, 1, p[0], &mut m);
    let v = m.points[&G].clone();
    assert_eq!(v.len(), 3);
    assert_eq!(v[0], p[0]);
    assert_eq!(v[2], p[1]);
    assert_eq!(m.tiles[&v[1]], (2, 0));
    append_point(G, [7.0, 0.0, 0.0], 1, &mut m);
    assert_eq!(m.tiles[m.points[&G].last().unwrap()], (7, 0));
}

#[test]
fn remove_releases_the_empty_group() {
    let mut m = Mock::new();
    let p = m.with_points(&[(0, 0), (5, 0)]);
    remove_unit(G, p[0], &mut m);
    assert!(m.log.is_empty());
    remove_unit(G, p[1], &mut m);
    assert_eq!(m.log, vec![format!("remove {:x} {G:x}", p[1])]);
}

#[test]
fn set_points_kills_adds_and_orders() {
    let mut m = Mock::new();
    let p = m.with_points(&[(0, 0), (5, 0), (9, 9)]);
    set_points(G, &[(9, 9), (1, 1), (0, 0)], 3, &mut m);
    let v = m.points[&G].clone();
    assert_eq!(v.len(), 3);
    assert_eq!(v[0], p[2]);
    assert_eq!(m.tiles[&v[1]], (1, 1));
    assert_eq!(v[2], p[0]);
    assert!(m.fields(G).is_loop);
    assert_eq!(m.log[0], format!("group {:x} 0", p[1]));
    assert_eq!(m.log[1], format!("kill {:x} true", p[1]));
}

#[test]
fn destroy_detaches_and_kills_every_point() {
    let mut m = Mock::new();
    let p = m.with_points(&[(0, 0), (5, 0)]);
    destroy(G, &mut m);
    assert!(m.points[&G].is_empty());
    assert_eq!(m.log, vec![
        format!("group {:x} 0", p[0]),
        format!("kill {:x} false", p[0]),
        format!("group {:x} 0", p[1]),
        format!("kill {:x} false", p[1]),
    ]);
}

#[test]
fn select_stops_at_the_first_refusal() {
    let mut m = Mock::new();
    let p = m.with_points(&[(0, 0), (5, 0), (6, 0)]);
    m.refuse_select = Some(p[1]);
    select(G, 0x77, true, &mut m);
    assert_eq!(m.log.len(), 2);
}

#[test]
fn save_load_repair_round_trip_and_the_id_counter() {
    reset_ids();
    assert_eq!(next_id(), 0);
    assert_eq!(next_id(), 1);
    let mut m = Mock::new();
    let p = m.with_points(&[(0, 0), (5, 0)]);
    m.fields.insert(G, GroupFields { id: 9, is_loop: true, is_town_patrol: true });
    assert_eq!(save(G, 0, 254, &mut m), 0);
    assert_eq!(m.file, vec![9, 2, m.ids[&p[0]], m.ids[&p[1]], 1, 1, 2]);
    // the old format: no town flag, no counter
    let mut old = Mock::new();
    old.points.insert(G, Vec::new());
    old.fields.insert(G, GroupFields { id: 4, ..Default::default() });
    assert_eq!(save(G, 0, 253, &mut old), 0);
    assert_eq!(old.file, vec![4, 0, 0]);

    let words = m.file.clone();
    let mut l = Mock::new();
    let a = l.unit(0, 0);
    let b = l.unit(5, 0);
    l.ids.insert(a, words[2]);
    l.ids.insert(b, words[3]);
    l.file = words;
    l.file[6] = 40;
    assert_eq!(load(G, 0, 254, &mut l), 0);
    assert_eq!(l.fields(G), GroupFields { id: 9, is_loop: true, is_town_patrol: true });
    assert_eq!(current_id(), 40);
    assert_eq!(l.points[&G], vec![u64::from(l.ids[&a]), u64::from(l.ids[&b])]);
    assert_eq!(repair_pointers(G, &mut l), 0);
    assert_eq!(l.points[&G], vec![a, b]);
    assert_eq!(l.groups[&a], G);
    // a saved id with no unit: 3, the points before it kept
    let mut r = Mock::new();
    let c = r.unit(0, 0);
    r.points.insert(G, vec![u64::from(r.ids[&c]), 12345]);
    assert_eq!(repair_pointers(G, &mut r), 3);
    assert_eq!(r.points[&G], vec![c]);
    // a read error stops the load
    let mut e = Mock::new();
    e.file = vec![1, 3, 10, 11, 12, 0];
    e.fail_read_at = Some(3);
    assert_eq!(load(G, 0, 254, &mut e), 5);
    assert_eq!(e.points[&G].len(), 1);
    reset_ids();
}

#[test]
fn run_dispatches_by_operation() {
    let mut m = Mock::new();
    let p = m.with_points(&[(0, 0), (5, 0)]);
    let args = EeUnitsPgArgs { unit: p[0], ..Default::default() };
    assert_eq!(run(EE_UNITS_PG_OP_NEXT, G, args, &mut m), p[1]);
    assert_eq!(run(EE_UNITS_PG_OP_INDEX_OF, G, EeUnitsPgArgs { unit: p[1], ..args }, &mut m), 1);
    assert_eq!(run(EE_UNITS_PG_OP_NEAREST, G, EeUnitsPgArgs { a: 4, b: 1, ..args }, &mut m), p[1]);
    assert_eq!(run(EE_UNITS_PG_OP_LAST, G, args, &mut m), p[1]);
}
