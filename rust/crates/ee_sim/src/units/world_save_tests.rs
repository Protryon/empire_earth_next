//! world_save.rs's tests: the running state saved, loaded and put back over a mock world (a unit is `0x100 + id`).

use super::*;
use crate::units::player_save::{
    EE_UNITS_PLAYER_SAVE_WRITE_BOOL, EE_UNITS_PLAYER_SAVE_WRITE_FLOAT, EE_UNITS_PLAYER_SAVE_WRITE_LONG,
    EE_UNITS_PLAYER_SAVE_WRITE_ULONG,
};
use std::collections::{BTreeMap, VecDeque};

fn unit(id: u64) -> u64 {
    0x100 + id
}

/// A forest: its address, its renewal, its tiles.
type Forest = (u64, u64, Vec<(i64, i64)>);

#[derive(Default)]
struct World {
    file: VecDeque<i64>,
    reads: usize,
    fail_read: Option<usize>,
    /// the save's version has every part up to this predicate
    has_upto: i64,
    chain: Vec<u64>,
    tile_x: BTreeMap<u64, i64>,
    steps: [Vec<u64>; 3],
    refs: BTreeMap<u64, i64>,
    active: Vec<u64>,
    forests: Vec<Forest>,
    ai: (i64, i64, i64),
    tiles: Vec<Vec<u64>>,
    players: Vec<Vec<u64>>,
    buckets: BTreeMap<(i64, i64), u64>,
    ghosts: Vec<u64>,
    after: Vec<i64>,
    parts: Vec<(i32, i64)>,
    debug: Vec<String>,
    links: Vec<(u64, u64)>,
}

impl World {
    fn fill(io: &mut EeUnitsWorldIo, items: &[u64]) -> i64 {
        for (i, &x) in items.iter().enumerate().take(io.cap) {
            // SAFETY: the list the caller gave, `cap` long
            unsafe { *io.list.add(i) = x };
        }
        items.len() as i64
    }
    fn given(io: &EeUnitsWorldIo) -> Vec<u64> {
        // SAFETY: the caller's list, `cap` long
        (0..io.cap).map(|i| unsafe { *io.list.add(i) }).collect()
    }
    fn exists(&self, u: u64) -> bool {
        self.chain.contains(&u) || self.players.iter().any(|p| p.contains(&u))
    }
}

impl WorldHost for World {
    fn call(&mut self, op: i32, io: &mut EeUnitsWorldIo) -> i64 {
        match op {
            EE_UNITS_PLAYER_SAVE_WRITE_ULONG | EE_UNITS_PLAYER_SAVE_WRITE_LONG => self.file.push_back(io.a),
            EE_UNITS_PLAYER_SAVE_WRITE_FLOAT => self.file.push_back(io.f.to_bits() as i64),
            EE_UNITS_PLAYER_SAVE_WRITE_BOOL => self.file.push_back(io.b),
            EE_UNITS_PLAYER_SAVE_READ_ULONG
            | EE_UNITS_PLAYER_SAVE_READ_LONG
            | EE_UNITS_PLAYER_SAVE_READ_FLOAT
            | EE_UNITS_PLAYER_SAVE_READ_BOOL => {
                self.reads += 1;
                if self.fail_read == Some(self.reads) {
                    return 2;
                }
                let Some(v) = self.file.pop_front() else {
                    return 2;
                };
                match op {
                    EE_UNITS_PLAYER_SAVE_READ_FLOAT => io.f = f32::from_bits(v as u32),
                    EE_UNITS_PLAYER_SAVE_READ_BOOL => io.b = v,
                    _ => io.a = v,
                }
            }
            EE_UNITS_WORLD_HAS => return (io.a <= self.has_upto) as i64,
            EE_UNITS_WORLD_PART_LOAD | EE_UNITS_WORLD_PART_SAVE => self.parts.push((op, io.a)),
            EE_UNITS_WORLD_AFTER_LOAD => self.after.push(io.a),
            EE_UNITS_WORLD_DEBUG => {
                // SAFETY: the NUL-terminated text debug() passes
                let t = unsafe { std::ffi::CStr::from_ptr(io.list as *const std::os::raw::c_char) };
                self.debug.push(t.to_string_lossy().into_owned());
            }
            EE_UNITS_WORLD_UNIT_ID => return io.a - 0x100,
            EE_UNITS_WORLD_CHAIN => return Self::fill(io, &self.chain.clone()),
            EE_UNITS_WORLD_UNIT => {
                let u = unit(io.a as u64);
                return if self.chain.contains(&u) { u as i64 } else { 0 };
            }
            EE_UNITS_WORLD_IN_CHAIN => return self.chain.contains(&(io.a as u64)) as i64,
            EE_UNITS_WORLD_TILE_X => return self.tile_x.get(&(io.a as u64)).copied().unwrap_or(0),
            EE_UNITS_WORLD_LINK => self.links.push((io.a as u64, io.b as u64)),
            EE_UNITS_WORLD_STEP_LIST => return Self::fill(io, &self.steps[io.a as usize].clone()),
            EE_UNITS_WORLD_COMPLEX => {
                let u = unit(io.a as u64);
                return if self.exists(u) { u as i64 } else { 0 };
            }
            EE_UNITS_WORLD_STEP_DROP | EE_UNITS_WORLD_STEP_ADD => {
                let d = if op == EE_UNITS_WORLD_STEP_ADD { 1 } else { -1 };
                let n = if io.a == 0 { 2 } else { 1 };
                *self.refs.entry(io.b as u64).or_default() += d * n;
                if io.a == 0 {
                    if d > 0 {
                        self.active.push(io.b as u64);
                    } else {
                        self.active.retain(|&x| x != io.b as u64);
                    }
                }
            }
            EE_UNITS_WORLD_STEP_SET => self.steps[io.a as usize] = Self::given(io),
            EE_UNITS_WORLD_FORESTS => {
                let mut all: Vec<u64> = self.forests.iter().map(|f| f.0).collect();
                all.insert(1, 0);
                return Self::fill(io, &all);
            }
            EE_UNITS_WORLD_FOREST_RENEWAL => {
                return self.forests.iter().find(|f| f.0 == io.a as u64).map_or(0, |f| f.1 as i64)
            }
            EE_UNITS_WORLD_FOREST_TILES => {
                let f = self.forests.iter().find(|f| f.0 == io.a as u64).unwrap();
                let tiles: Vec<u64> = f.2.iter().map(|&(x, y)| (x as u32 as u64) | ((y as u32 as u64) << 32)).collect();
                return Self::fill(io, &tiles);
            }
            EE_UNITS_WORLD_FOREST_AT => {
                return self.forests.iter().find(|f| f.2.contains(&(io.a, io.b))).map_or(0, |f| f.0 as i64)
            }
            EE_UNITS_WORLD_FOREST_SET_RENEWAL => {
                self.forests.iter_mut().find(|f| f.0 == io.a as u64).unwrap().1 = io.b as u64
            }
            EE_UNITS_WORLD_FOREST_SIZE => {
                return self.forests.iter().find(|f| f.0 == io.a as u64).map_or(0, |f| f.2.len() as i64)
            }
            EE_UNITS_WORLD_AI_SCHEDULE => {
                io.a = self.ai.0;
                io.b = self.ai.1;
                return self.ai.2;
            }
            EE_UNITS_WORLD_AI_SET_SCHEDULE => {
                self.ai.0 = io.a;
                self.ai.1 = io.b;
            }
            EE_UNITS_WORLD_TILE_COUNT => return self.tiles.len() as i64,
            EE_UNITS_WORLD_TILE_UNITS => return Self::fill(io, &self.tiles[io.a as usize].clone()),
            EE_UNITS_WORLD_TILE_SET => self.tiles[io.a as usize][io.b as usize] = Self::given(io)[0],
            EE_UNITS_WORLD_REGION_ORDER => return Self::fill(io, &[2, 3, 2, 7, 0xffff_ffff, 5, 0]),
            EE_UNITS_WORLD_REGION_RESTORE => {
                self.parts.push((op, io.a));
                return 1;
            }
            EE_UNITS_WORLD_GHOSTS => {
                io.a = 1;
                return Self::fill(io, &self.ghosts.clone());
            }
            EE_UNITS_WORLD_GHOST_FIELDS => {
                let id = io.a - 0x100;
                io.v = [id, 7, 2, 50, 3, 4, 1, 1];
                io.a = 125;
                io.m = [1.5; 16];
            }
            EE_UNITS_WORLD_DEREFERENCE => *self.refs.entry(io.a as u64).or_default() -= 1,
            EE_UNITS_WORLD_PLAYER_COUNT => return self.players.len() as i64,
            EE_UNITS_WORLD_PLAYER_UNITS => return Self::fill(io, &self.players[io.a as usize].clone()),
            EE_UNITS_WORLD_PLAYER_UNIT => {
                let u = unit(io.b as u64);
                return if self.players[io.a as usize].contains(&u) { u as i64 } else { 0 };
            }
            EE_UNITS_WORLD_PLAYER_SET_LIST => self.players[io.a as usize] = Self::given(io),
            EE_UNITS_WORLD_PLAYER_SET_BUCKET => {
                self.buckets.insert((io.a, io.b), Self::given(io)[0]);
            }
            _ => panic!("op {op}"),
        }
        0
    }
}

fn sample() -> World {
    let mut w = World {
        has_upto: 99,
        chain: vec![unit(5), unit(2), unit(9)],
        steps: [vec![unit(2), unit(5)], vec![unit(9)], vec![]],
        forests: vec![(0x900, 1200, vec![(1, 2), (1, 3)]), (0x901, 0, vec![(4, 4)]), (0x902, 3400, vec![(8, -1)])],
        ai: (1, 4500, 3),
        tiles: vec![vec![], vec![unit(5), unit(2)], vec![unit(9)], vec![unit(2), 0, unit(9)]],
        players: vec![vec![], vec![unit(2), unit(0x82), unit(5)]],
        ghosts: vec![unit(40), unit(41)],
        ..Default::default()
    };
    w.tile_x = [(unit(5), 1), (unit(2), 1), (unit(9), 3)].into_iter().collect();
    w
}

#[test]
fn the_save_writes_every_part_in_order() {
    let mut w = sample();
    save_live(&mut w);
    let words: Vec<i64> = w.file.iter().copied().collect();
    const NONE: i64 = c_ulong::MAX as i64;
    let f15 = 1.5f32.to_bits() as i64;
    let mut expect = vec![
        3, 5, 2, 9, // the chain
        2, 2, 5, 1, 9, 0, // the stepped lists
        2, 1200, 2, 1, 2, 1, 3, 3400, 1, 8, -1, // the forests with a renewal
        1, 4500, // the schedule
        2, 1, 2, 5, 2, 3, 3, 2, NONE, 9, // the tiles of two units or more
        2, 3, 2, 7, NONE, 5, 0, // the regions' words
        2, // two ghosts, the first complex
        40, 7, 2, 50, 3, 4, 1,
    ];
    expect.extend(std::iter::repeat_n(f15, 16));
    expect.extend([1, 125, 1]);
    expect.extend([41, 7, 2, 50, 3, 4, 0]);
    expect.extend(std::iter::repeat_n(0, 16));
    expect.extend([1, 125, 0]);
    expect.extend([2, 0, 3, 2, 0x82, 5]); // the players' lists
    assert_eq!(words, expect);
    let parts: Vec<i64> = w.parts.iter().map(|p| p.1).collect();
    assert_eq!(parts, vec![1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
}

#[test]
fn a_load_reads_the_parts_and_queues_their_restores_in_order() {
    let mut w = sample();
    save_live(&mut w);
    let mut l = World { file: w.file.clone(), has_upto: 99, ..Default::default() };
    load_live(&mut l);
    assert!(l.file.is_empty());
    assert_eq!(l.parts.iter().map(|p| p.1).collect::<Vec<_>>(), vec![1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
    let pending: Vec<Box<Pending>> =
        l.after.iter().map(|&p| unsafe { Box::from_raw(p as usize as *mut Pending) }).collect();
    assert_eq!(pending.len(), 7);
    assert_eq!(*pending[0], Pending::UnitOrder(vec![5, 2, 9], [vec![2, 5], vec![9], vec![]]));
    assert_eq!(*pending[1], Pending::Forests(vec![(1200, vec![(1, 2), (1, 3)]), (3400, vec![(8, -1)])]));
    assert_eq!(*pending[2], Pending::AiSchedule(1, 4500));
    assert_eq!(*pending[3], Pending::TileOrder(vec![(1, vec![5, 2]), (3, vec![2, c_ulong::MAX, 9])]));
    assert_eq!(*pending[4], Pending::RegionOrder(vec![(3, vec![7, c_ulong::MAX]), (5, vec![])]));
    assert_eq!(*pending[5], Pending::Ghosts);
    assert_eq!(*pending[6], Pending::PlayerUnitOrder(vec![vec![], vec![2, 0x82, 5]]));
    let mut found = EeUnitsWorldGhost::default();
    assert!(unsafe { ee_units_world_ghost_find(41, &mut found) });
    assert!(!found.is_complex && !found.is_built);
    assert_eq!((found.unit_type, found.hit_points, found.resource_amount), (7, 50, 125));
    assert!(unsafe { ee_units_world_ghost_find(40, &mut found) });
    assert!(found.is_complex && found.is_built && found.placement[3][3] == 1.5);
    assert!(!unsafe { ee_units_world_ghost_find(42, &mut found) });
    restore(&mut l, &Pending::Ghosts);
    assert!(!unsafe { ee_units_world_ghost_find(40, &mut found) });
}

#[test]
fn the_tech_flags_follow_the_graph_searches_from_0_282_only() {
    // a 0.281 save (the predicates up to the graph searches): no tech flags part either way
    let mut w = sample();
    w.has_upto = EE_UNITS_WORLD_HAS_GRAPH_SEARCHES;
    save_live(&mut w);
    assert_eq!(w.parts.iter().map(|p| p.1).collect::<Vec<_>>(), vec![1, 2, 3, 4, 5, 6, 7, 8, 9]);
    let mut l = World { file: w.file.clone(), has_upto: EE_UNITS_WORLD_HAS_GRAPH_SEARCHES, ..Default::default() };
    load_live(&mut l);
    assert_eq!(l.parts.iter().map(|p| p.1).collect::<Vec<_>>(), vec![1, 2, 3, 4, 5, 6, 7, 8, 9]);
    // 0.282: the tech flags last, after the graph searches
    let mut n = World { file: w.file.clone(), has_upto: EE_UNITS_WORLD_HAS_TECH_ALLOWED, ..Default::default() };
    load_live(&mut n);
    assert_eq!(n.parts.len(), 10);
    assert_eq!(n.parts.last(), Some(&(EE_UNITS_WORLD_PART_LOAD, EE_UNITS_WORLD_PART_TECH_ALLOWED)));
    for p in l.after.iter().chain(n.after.iter()) {
        drop(unsafe { Box::from_raw(*p as usize as *mut Pending) });
    }
}

#[test]
fn a_failed_read_stops_the_parts_after_it() {
    let mut w = sample();
    save_live(&mut w);
    // the first forest's last tile read (the 17th) fails: no forests' restore, no later part
    let mut l = World { file: w.file.clone(), has_upto: 99, fail_read: Some(17), ..Default::default() };
    load_live(&mut l);
    assert_eq!(l.after.len(), 1);
    assert!(l.parts.is_empty());
    // a save without the running state: the order and the forests only
    let mut v = sample();
    v.has_upto = 0;
    save_live(&mut v);
    assert_eq!(v.file.len(), 21);
    let mut l = World { file: v.file.clone(), has_upto: 0, ..Default::default() };
    load_live(&mut l);
    assert_eq!(l.after.len(), 2);
    for p in l.after {
        drop(unsafe { Box::from_raw(p as usize as *mut Pending) });
    }
}

#[test]
fn the_unit_order_relinks_the_chain_and_sets_the_stepped_lists() {
    let mut w = sample();
    restore(&mut w, &Pending::UnitOrder(vec![2, 5, 9], [vec![5, 9, 5, 77], vec![], vec![2]]));
    assert_eq!(w.links, vec![(unit(2), 0), (unit(5), unit(2)), (unit(9), unit(5))]);
    assert_eq!(w.steps, [vec![unit(5), unit(9)], vec![], vec![unit(2)]]);
    // unit 2 left the stepped units (two references), 9 joined them (two) and left the activation queue (one)
    assert_eq!(w.refs[&unit(2)], -2 + 1);
    assert_eq!(w.refs[&unit(9)], 2 - 1);
    assert_eq!(w.active, vec![unit(9)]);
    assert_eq!(w.debug, vec!["EEPWorldHelper: saved stepped unit 77 is not in the loaded game\n"]);
    // not x-sorted: the chain is the load's
    let mut w = sample();
    restore(&mut w, &Pending::UnitOrder(vec![9, 5, 2], Default::default()));
    assert!(w.links.is_empty());
    assert_eq!(w.debug.len(), 1);
}

#[test]
fn tiles_forests_schedule_and_players_are_put_back() {
    let mut w = sample();
    restore(&mut w, &Pending::TileOrder(vec![(1, vec![2, 5]), (3, vec![9, 2, 7]), (9, vec![1])]));
    assert_eq!(w.tiles[1], vec![unit(2), unit(5)]);
    assert_eq!(w.tiles[3], vec![unit(2), 0, unit(9)]);
    assert_eq!(
        w.debug,
        vec!["EEPWorldHelper: 1 tiles hold other units than the saved ones: their order is the load's\n"]
    );
    restore(&mut w, &Pending::Forests(vec![(77, vec![(1, 2), (1, 3)]), (88, vec![(4, 4), (9, 9)])]));
    assert_eq!(w.forests.iter().map(|f| f.1).collect::<Vec<_>>(), vec![77, 88, 3400]);
    assert_eq!(w.debug.len(), 2);
    restore(&mut w, &Pending::AiSchedule(2, 99));
    assert_eq!(w.ai, (2, 99, 3));
    restore(&mut w, &Pending::AiSchedule(3, 5));
    assert_eq!(w.ai, (2, 99, 3));
    // player 1: 5 then 2 saved; 0x82 (bucket 2, as 2) goes after 2
    restore(&mut w, &Pending::PlayerUnitOrder(vec![vec![], vec![5, 2]]));
    assert_eq!(w.players[1], vec![unit(5), unit(2), unit(0x82)]);
    assert_eq!(w.buckets.get(&(1, 2)), Some(&unit(2)));
    assert_eq!(w.buckets.get(&(1, 5)), Some(&unit(5)));
}
