//! The group objects over a mock world (group_obj.rs): the kinds' adds and removals, the type counts and kind flags,
//! the reference counts, the selection's sort, save, load and repair, the active group's fill, finish and path
//! requests, the clusters, and the active groups' save and restore.

use super::group_obj::*;
use crate::units::player_save::{
    EeUnitsPlayerSaveIo, PlayerSaveHost, EE_UNITS_PLAYER_SAVE_READ_BOOL, EE_UNITS_PLAYER_SAVE_READ_FLOAT,
    EE_UNITS_PLAYER_SAVE_READ_LONG, EE_UNITS_PLAYER_SAVE_READ_ULONG, EE_UNITS_PLAYER_SAVE_WRITE_BOOL,
    EE_UNITS_PLAYER_SAVE_WRITE_FLOAT, EE_UNITS_PLAYER_SAVE_WRITE_LONG, EE_UNITS_PLAYER_SAVE_WRITE_ULONG,
};
use std::collections::{BTreeMap, VecDeque};
use std::os::raw::c_long;

const PLAYER: u64 = 0x5000;
const OTHER_PLAYER: u64 = 0x5100;

#[derive(Default, Clone)]
struct Path {
    done: bool,
    id: i64,
    tiles: Vec<(i64, i64)>,
    refs: i32,
}

#[derive(Default)]
struct World {
    units: BTreeMap<u64, GroupUnit>,
    refs: BTreeMap<u64, i32>,
    flags: BTreeMap<u64, [u8; 17]>,
    formations: BTreeMap<u64, (i64, i32, [f32; 3])>,
    next_formation: u64,
    paths: BTreeMap<u64, Path>,
    next_path: u64,
    group_paths: Vec<(u64, Vec<[f32; 3]>)>,
    log: Vec<String>,
    time: c_long,
    current: u64,
    selected: u64,
    hot_keys: Vec<u64>,
    lists: BTreeMap<(u64, u32), Vec<u64>>,
    computed: u32,
    keeps: BTreeMap<u32, bool>,
    selections: [u64; 2],
    viewed: u64,
    acks: (c_long, c_long),
    upgrade_values: (c_long, c_long, c_long, i64),
}

impl World {
    fn unit(&mut self, handle: u64, id: u32, unit_type: i64, kind_flags: i64) -> u64 {
        self.units.insert(
            handle,
            GroupUnit {
                id,
                hit_points: 10,
                valid: true,
                has_tile: true,
                tile_x: (id % 7) as i64,
                tile_y: (id % 5) as i64,
                location: [id as f32, id as f32 * 2.0, 0.0],
                player: PLAYER,
                complex: handle,
                class: 0x100 + unit_type as u64,
                unit_type,
                kind_flags,
                category: 1,
                stats: 0x200 + unit_type as u64,
                max_hit_points: 10,
                max_velocity: unit_type as f32,
                ..Default::default()
            },
        );
        handle
    }
    fn u(&mut self, handle: u64) -> &mut GroupUnit {
        self.units.get_mut(&handle).unwrap()
    }
    fn refs(&self, handle: u64) -> i32 {
        self.refs.get(&handle).copied().unwrap_or(0)
    }
}

impl GroupHost for World {
    fn unit(&mut self, unit: u64) -> GroupUnit {
        self.units.get(&unit).copied().unwrap_or_default()
    }
    fn reference_unit(&mut self, unit: u64) {
        *self.refs.entry(unit).or_insert(0) += 1;
    }
    fn dereference_unit(&mut self, unit: u64) {
        *self.refs.entry(unit).or_insert(0) -= 1;
    }
    fn set_active_group_member(&mut self, unit: u64, group: u64) {
        if let Some(u) = self.units.get_mut(&unit) {
            u.active_group = group;
        }
    }
    fn world_unit(&mut self, id: i64) -> u64 {
        self.units.iter().find(|(_, u)| u.id as i64 == id).map_or(0, |(&h, _)| h)
    }
    fn is_complex(&mut self, unit: u64) -> bool {
        self.units.contains_key(&unit)
    }
    fn formation_new(&mut self, kind: i64) -> u64 {
        if !(0..8).contains(&kind) {
            return 0;
        }
        self.next_formation += 1;
        let f = 0x9000 + self.next_formation;
        self.formations.insert(f, (kind, 1, [0.0; 3]));
        f
    }
    fn formation_reference(&mut self, formation: u64) {
        self.formations.get_mut(&formation).unwrap().1 += 1;
    }
    fn formation_dereference(&mut self, formation: u64) {
        self.formations.get_mut(&formation).unwrap().1 -= 1;
    }
    fn formation_kind(&mut self, formation: u64) -> i64 {
        self.formations[&formation].0
    }
    fn formation_center(&mut self, formation: u64) -> [f32; 3] {
        self.formations[&formation].2
    }
    fn set_formation_center(&mut self, formation: u64, center: [f32; 3]) {
        self.formations.get_mut(&formation).unwrap().2 = center;
    }
    fn formation_place(&mut self, formation: u64, units: &[u64]) -> Vec<[f32; 3]> {
        let c = self.formations[&formation].2;
        (0..units.len()).map(|i| [c[0] + i as f32, c[1], 0.0]).collect()
    }
    fn formation_state(&mut self, formation: u64) -> FormationState {
        let c = self.formations[&formation].2;
        FormationState { orientation: [0.5; 16], center: c, heading: 1.5, b5c: false }
    }
    fn set_formation_state(&mut self, formation: u64, state: &FormationState) {
        self.formations.get_mut(&formation).unwrap().2 = state.center;
    }
    fn terrain_flags(&mut self, unit: u64) -> [u8; 17] {
        self.flags.get(&unit).copied().unwrap_or([1; 17])
    }
    fn continent_of(&mut self, unit: u64) -> u16 {
        (self.units[&unit].tile_x % 2) as u16
    }
    fn continent_at(&mut self, x: i64, _y: i64) -> u16 {
        x as u16
    }
    fn subsector_at(&mut self, x: i64, y: i64) -> (i64, i64) {
        (x, y)
    }
    fn path_area_at(&mut self, x: i64, _y: i64) -> u16 {
        (x / 4) as u16
    }
    fn slot_area_at(&mut self, x: i64, y: i64) -> Option<u16> {
        (x >= 0 && y >= 0).then_some((x / 4) as u16)
    }
    fn path_request(&mut self, player_id: i64, from: (i64, i64), to: (i64, i64), _flags: &[u8; 17]) -> u64 {
        self.log.push(format!("request {player_id} {from:?} {to:?}"));
        self.next_path += 1;
        let p = 0x7000 + self.next_path;
        self.paths.insert(
            p,
            Path { done: false, id: 40 + self.next_path as i64, tiles: vec![from, (1, 1), (5, 2), to], refs: 1 },
        );
        p
    }
    fn path_cancel(&mut self, path: u64) {
        self.log.push(format!("cancel {path:#x}"));
    }
    fn path_reference(&mut self, path: u64) {
        self.paths.get_mut(&path).unwrap().refs += 1;
    }
    fn path_dereference(&mut self, path: u64) {
        self.paths.get_mut(&path).unwrap().refs -= 1;
    }
    fn path_done(&mut self, path: u64) -> bool {
        self.paths[&path].done
    }
    fn path_id(&mut self, path: u64) -> i64 {
        self.paths[&path].id
    }
    fn path_tiles(&mut self, path: u64) -> Vec<(i64, i64)> {
        self.paths[&path].tiles.clone()
    }
    fn set_group_path(&mut self, unit: u64, points: &[[f32; 3]]) {
        self.group_paths.push((unit, points.to_vec()));
    }
    fn clear_build_targets(&mut self, unit: u64) {
        self.log.push(format!("clear targets {unit:#x}"));
    }
    fn time(&mut self) -> c_long {
        self.time += 1;
        self.time
    }
    fn post_selection(&mut self) {
        self.log.push("post selection".into());
    }
    fn post_saved_group(&mut self, index: c_long) {
        self.log.push(format!("post saved {index}"));
    }
    fn current_player(&mut self) -> u64 {
        self.current
    }
    fn selected_group_of(&mut self, _player: u64) -> u64 {
        self.selected
    }
    fn player_group(&mut self, _player: u64, index: i64) -> u64 {
        self.hot_keys.get(index as usize).copied().unwrap_or(0xdead)
    }
    fn player_id(&mut self, player: u64) -> i64 {
        (player >> 8) as i64
    }
    fn player_groups(&mut self, player: u64, list: u32) -> Vec<u64> {
        self.lists.get(&(player, list)).cloned().unwrap_or_default()
    }
    fn player_groups_clear(&mut self, player: u64, list: u32) {
        self.lists.remove(&(player, list));
    }
    fn player_groups_erase(&mut self, player: u64, list: u32, group: u64) {
        let l = self.lists.entry((player, list)).or_default();
        if let Some(i) = l.iter().position(|&g| g == group) {
            l.remove(i);
        }
    }
    fn player_selection(&mut self, _player: u64, which: u32) -> u64 {
        self.selections[which as usize]
    }
    fn player_groups_push(&mut self, player: u64, list: u32, group: u64) {
        self.lists.entry((player, list)).or_default().push(group);
    }
    fn compute_values(&mut self, _group: u64) {
        self.computed += 1;
    }
    fn goal_location(&mut self, _goal: u64) -> [f32; 3] {
        [9.5, 3.25, 0.0]
    }
    fn delete_goal(&mut self, goal: u64) {
        self.log.push(format!("delete goal {goal:#x}"));
    }
    fn goal_moves_together(&mut self, goal: u64) -> bool {
        goal == 0xaa
    }
    fn keeps(&mut self, _test: u32, id: u32) -> bool {
        self.keeps.get(&id).copied().unwrap_or(true)
    }
    fn fill_members(&mut self, _player: u64, ids: &[u32], slot: i64) -> Vec<u32> {
        (0..ids.len() as u32).filter(|&i| slot == 0 || ids[i as usize] % 2 == 1).collect()
    }
    fn clear_area_outlines(&mut self) {
        self.log.push("clear outlines".into());
    }
    fn area_outlines(&mut self, areas: &[[i64; 4]], continents: &[u16]) {
        self.log.push(format!("outlines {areas:?} {continents:?}"));
    }
    fn area_effect_radius(&mut self, _template: i64) -> f32 {
        2.0
    }
    fn player_units(&mut self, _player: u64) -> Vec<u64> {
        self.units.keys().copied().collect()
    }
    fn world_players(&mut self) -> Vec<u64> {
        vec![PLAYER, 0]
    }
}

impl PathWaits for World {
    fn await_path(&mut self, request_id: i64, group: G, key: i64) {
        self.log.push(format!("await {request_id} {group:p} {key}"));
    }
}

#[test]
fn a_hot_key_group_counts_types_and_kind_flags_and_tells_the_interface() {
    let mut w = World::default();
    let a = w.unit(0x10, 1, 7, 0b01);
    let b = w.unit(0x20, 2, 7, 0b10);
    let c = w.unit(0x30, 3, 9, 0b100);
    let g = new(&mut w, EE_UNITS_GROUP_KIND_SAVED, PLAYER, 4);
    unsafe {
        assert_eq!((*g).mFormation as u64, 0x9001);
        add(&mut w, g, a);
        add(&mut w, g, b);
        add(&mut w, g, c);
        // only the first add posts (the group's first unit)
        assert_eq!(w.log, vec!["post saved 4"]);
        assert_eq!((*g).mKindFlags, 0b111);
        assert_eq!(type_counts(g), vec![(7, 2), (9, 1)]);
        assert_eq!((w.refs(a), w.refs(b), w.refs(c)), (1, 1, 1));
        // a dead unit is not taken
        let d = w.unit(0x40, 4, 9, 0);
        w.u(d).hit_points = 0;
        add(&mut w, g, d);
        assert_eq!(len(g), 3);
        // removing the last of a type recomputes the flags; the first unit's removal posts first
        remove_unit(&mut w, g, c);
        assert_eq!((*g).mKindFlags, 0b011);
        assert_eq!(type_counts(g), vec![(7, 2)]);
        remove_unit(&mut w, g, a);
        assert_eq!(w.log.last().unwrap(), "post saved 4");
        assert_eq!((*g).mKindFlags, 0b011, "a type still counted keeps the flags");
        assert_eq!(units(g), vec![b]);
        // validate drops a unit given away and posts again when the first went
        w.u(b).player = OTHER_PLAYER;
        w.log.clear();
        validate(&mut w, g);
        assert_eq!(len(g), 0);
        assert_eq!(w.log, vec!["post saved 4", "post saved 4"]);
        assert_eq!(w.refs(b), 0);
        dereference(&mut w, g);
    }
    assert_eq!(w.formations[&0x9001].1, 0, "the formation released with the group");
}

#[test]
fn a_selection_adds_once_sorts_and_stamps_its_changes() {
    let mut w = World::default();
    let a = w.unit(0x10, 5, 7, 0);
    let b = w.unit(0x20, 2, 7, 0);
    let c = w.unit(0x30, 3, 9, 0);
    w.u(c).range = 4.0;
    w.u(b).hit_points = 1; // wounded: after the healthy one of its class
    let g = new(&mut w, EE_UNITS_GROUP_KIND_SELECTED, PLAYER, 0);
    w.current = PLAYER;
    w.selected = g as u64;
    unsafe {
        add(&mut w, g, a);
        add(&mut w, g, a);
        add(&mut w, g, b);
        add(&mut w, g, c);
        assert_eq!(len(g), 3);
        assert!((*g).mNeedsSort);
        assert_eq!(w.log.iter().filter(|l| *l == "post selection").count(), 3);
        assert_eq!((*g).mChangeTime, 3);
        sort_units(&mut w, g);
        // range 0 before range 4; in class 7 the healthy unit 5 before the wounded unit 2
        assert_eq!(units(g), vec![a, b, c]);
        assert!(!(*g).mNeedsSort);
        // a copy moves the references and keeps the counts
        let copy = new(&mut w, EE_UNITS_GROUP_KIND_SELECTED, PLAYER, 0);
        copy_selection(&mut w, copy, g);
        assert_eq!(units(copy), units(g));
        assert_eq!(w.refs(a), 2);
        assert_eq!(unit_type_count(copy, 7), 2);
        clear(&mut w, copy);
        assert_eq!(w.refs(a), 1);
        dereference(&mut w, copy);
        dereference(&mut w, g);
    }
    assert_eq!((w.refs(a), w.refs(b), w.refs(c)), (0, 0, 0));
}

#[test]
fn the_selection_sorter_follows_its_rules() {
    let base = GroupUnit { class: 1, hit_points: 10, max_hit_points: 10, ..Default::default() };
    let healthy = GroupUnit { id: 9, ..base };
    let wounded = GroupUnit { id: 1, hit_points: 1, ..base };
    assert!(selection_less(&healthy, &wounded) && !selection_less(&wounded, &healthy));
    let a = GroupUnit { id: 1, ..base };
    let b = GroupUnit { id: 2, ..base };
    assert!(selection_less(&a, &b));
    let short = GroupUnit { class: 2, range: 1.0, ..base };
    let long = GroupUnit { class: 3, range: 2.0, ..base };
    assert!(selection_less(&short, &long));
    let small = GroupUnit { class: 2, scale: 1.0, unit_type: 9, ..base };
    let big = GroupUnit { class: 3, scale: 2.0, unit_type: 1, ..base };
    assert!(selection_less(&small, &big));
    let strong = GroupUnit { class: 2, melee: true, attack: 5.0, ..base };
    let weak = GroupUnit { class: 3, attack: 1.0, ..base };
    assert!(selection_less(&strong, &weak));
    let slow = GroupUnit { stats: 1, max_velocity: 1.0, ..base };
    let fast = GroupUnit { stats: 2, max_velocity: 2.0, ..base };
    assert!(velocity_less(&slow, &fast) && !velocity_less(&fast, &slow));
    let few = GroupUnit { stats: 1, hit_points: 2, ..base };
    assert!(velocity_less(&few, &slow));
}

/// A saved game's file of plain values.
#[derive(Default)]
struct File {
    values: VecDeque<(i32, i64, f32)>,
    paths: Vec<u64>,
    loaded: VecDeque<u64>,
}

impl PlayerSaveHost for File {
    fn call(&mut self, op: i32, _index: u32, io: &mut EeUnitsPlayerSaveIo) -> i32 {
        let kind = match op {
            EE_UNITS_PLAYER_SAVE_WRITE_ULONG | EE_UNITS_PLAYER_SAVE_READ_ULONG => 0,
            EE_UNITS_PLAYER_SAVE_WRITE_LONG | EE_UNITS_PLAYER_SAVE_READ_LONG => 1,
            EE_UNITS_PLAYER_SAVE_WRITE_FLOAT | EE_UNITS_PLAYER_SAVE_READ_FLOAT => 2,
            _ => 3,
        };
        match op {
            EE_UNITS_PLAYER_SAVE_WRITE_ULONG | EE_UNITS_PLAYER_SAVE_WRITE_LONG => {
                self.values.push_back((kind, io.i, 0.0))
            }
            EE_UNITS_PLAYER_SAVE_WRITE_FLOAT => self.values.push_back((kind, 0, io.f)),
            EE_UNITS_PLAYER_SAVE_WRITE_BOOL => self.values.push_back((kind, io.b as i64, 0.0)),
            EE_UNITS_PLAYER_SAVE_READ_ULONG
            | EE_UNITS_PLAYER_SAVE_READ_LONG
            | EE_UNITS_PLAYER_SAVE_READ_FLOAT
            | EE_UNITS_PLAYER_SAVE_READ_BOOL => {
                let Some((k, i, f)) = self.values.pop_front() else { return 2 };
                assert_eq!(k, kind, "the read's type is the write's");
                io.i = i;
                io.f = f;
                io.b = i as u8;
            }
            _ => panic!("op {op}"),
        }
        0
    }
}

impl SavedPaths for File {
    fn save_path(&mut self, path: u64) {
        self.paths.push(path);
    }
    fn load_path(&mut self) -> (i32, u64) {
        (0, self.loaded.pop_front().unwrap_or(0))
    }
}

#[test]
fn a_selection_saves_and_repairs_as_the_original() {
    let mut w = World::default();
    let a = w.unit(0x10, 11, 7, 0);
    let b = w.unit(0x20, 12, 7, 0);
    let hot = new(&mut w, EE_UNITS_GROUP_KIND_SAVED, PLAYER, 1);
    let hot2 = new(&mut w, EE_UNITS_GROUP_KIND_SAVED, PLAYER, 2);
    w.hot_keys = vec![hot as u64, hot2 as u64];
    let g = new(&mut w, EE_UNITS_GROUP_KIND_SELECTED, PLAYER, 0);
    let mut f = File::default();
    unsafe {
        add(&mut w, g, a);
        add(&mut w, g, b);
        (*g).mSavedGroup = hot2;
        assert_eq!(save(&mut w, g, &mut f), 0);
        let v: Vec<i64> = f.values.iter().map(|x| x.1).collect();
        // the formation kind 0, two ids, the saved group's slot
        assert_eq!(v, vec![0, 2, 11, 12, 1]);
        let l = new(&mut w, EE_UNITS_GROUP_KIND_SELECTED, PLAYER, 0);
        assert_eq!(load(&mut w, l, &mut f), 0);
        assert_eq!(units(l), vec![11, 12], "ids until RepairPointers");
        assert_eq!((*l).mSavedGroup as usize, 1);
        repair_pointers(&mut w, l);
        assert_eq!(units(l), vec![a, b]);
        // the adds forgot the saved group (AddIfAbsent): the repair reads index 0, as the original did
        assert_eq!((*l).mSavedGroup, hot);
        // none saved: -1; an empty selection keeps it
        let e = new(&mut w, EE_UNITS_GROUP_KIND_SELECTED, PLAYER, 0);
        let mut f2 = File::default();
        save(&mut w, e, &mut f2);
        assert_eq!(f2.values.back().unwrap().1, std::os::raw::c_ulong::MAX as i64);
        let e2 = new(&mut w, EE_UNITS_GROUP_KIND_SELECTED, PLAYER, 0);
        load(&mut w, e2, &mut f2);
        repair_pointers(&mut w, e2);
        assert!((*e2).mSavedGroup.is_null());
        for x in [g, l, e, e2, hot, hot2] {
            dereference(&mut w, x);
        }
    }
    assert_eq!((w.refs(a), w.refs(b)), (0, 0));
}

#[test]
fn an_active_group_fills_moves_members_and_finishes() {
    let mut w = World::default();
    let a = w.unit(0x10, 1, 7, 0);
    let b = w.unit(0x20, 2, 8, 0);
    let c = w.unit(0x30, 3, 9, 0);
    let sel = new(&mut w, EE_UNITS_GROUP_KIND_SELECTED, PLAYER, 0);
    let g = new(&mut w, EE_UNITS_GROUP_KIND_ACTIVE, PLAYER, 0);
    unsafe {
        assert_eq!((*g).mFields.mAction, 5000);
        for u in [c, b, a] {
            add(&mut w, sel, u);
        }
        // slot 1 takes the odd ids, in the selection's order; sort 1 sorts them slowest first
        fill(&mut w, g, sel, 1, 1);
        assert_eq!(units(g), vec![a, c]);
        assert_eq!(w.u(a).active_group, g as u64);
        assert_eq!(references(g), 3, "its own and its two units'");
        // a second group taking the unit takes it out of the first
        let g2 = new(&mut w, EE_UNITS_GROUP_KIND_ACTIVE, PLAYER, 0);
        w.lists.insert((PLAYER, 0), vec![g as u64]);
        add_member(&mut w, g2, a, true);
        assert_eq!(units(g), vec![c]);
        assert_eq!(w.u(a).active_group, g2 as u64);
        assert_eq!(references(g), 2);
        assert_eq!(active_group_of(&mut w, PLAYER, c), g as u64);
        // the goal, the destinations, the finish
        set_goal(&mut w, g, 0xaa, true);
        assert!((*g).mFields.mIsQueued && (*g).mFields.mMovesTogether);
        tell_group(&mut w, g, c, [1.0, 2.0, 3.0]);
        assert_eq!(destinations(g), vec![(3, [1.0, 2.0, 3.0])]);
        w.u(c).hit_points = 0;
        validate(&mut w, g);
        assert_eq!(len(g), 0);
        assert!(destinations(g).is_empty());
        assert!((*g).mFields.mIsFinished);
        assert!(w.log.contains(&"delete goal 0xaa".to_string()));
        assert_eq!(w.lists[&(PLAYER, 2)], vec![g as u64]);
        assert_eq!(references(g), 2, "its own and the finished list's");
        dereference(&mut w, g);
        dereference(&mut w, g);
        clear(&mut w, g2);
        assert_eq!(w.u(a).active_group, 0);
        dereference(&mut w, g2);
        dereference(&mut w, sel);
    }
    assert_eq!((w.refs(a), w.refs(b), w.refs(c)), (0, 0, 0));
}

#[test]
fn clusters_split_by_continent_air_and_distance() {
    let mut w = World::default();
    let near1 = w.unit(0x10, 1, 7, 0);
    let near2 = w.unit(0x20, 2, 7, 0);
    let far = w.unit(0x30, 3, 7, 0);
    let flyer = w.unit(0x40, 4, 7, 0);
    let building = w.unit(0x50, 5, 7, 0);
    w.u(near1).tile_x = 10;
    w.u(near1).tile_y = 10;
    w.u(near2).tile_x = 12;
    w.u(near2).tile_y = 13;
    w.u(far).tile_x = 40;
    w.u(far).tile_y = 40;
    w.u(flyer).domains = 1;
    w.u(building).land_building = true;
    let clusters = make_clusters(&mut w, &[near1, near2, far, flyer, building], 10, true);
    assert_eq!(clusters.len(), 4);
    // from the last unit: the far one first, then near2 with near1 joining it
    assert_eq!((clusters[0].kind, clusters[0].units.clone()), (1, vec![far]));
    assert_eq!((clusters[1].kind, clusters[1].units.clone()), (1, vec![near2, near1]));
    assert_eq!((clusters[1].left, clusters[1].right, clusters[1].top, clusters[1].bottom), (10, 12, 10, 13));
    assert_eq!((clusters[2].kind, clusters[2].units.clone()), (3, vec![building]));
    assert_eq!((clusters[3].kind, clusters[3].units.clone()), (2, vec![flyer]));
    // other terrain flags make another cluster
    w.flags.insert(near1, [0; 17]);
    let clusters = make_clusters(&mut w, &[near1, near2], 10, true);
    assert_eq!(clusters.len(), 2);
}

#[test]
fn path_requests_wait_follow_and_cancel() {
    let mut w = World::default();
    let a = w.unit(0x10, 1, 7, 0);
    let b = w.unit(0x20, 2, 7, 0);
    let g = new(&mut w, EE_UNITS_GROUP_KIND_ACTIVE, PLAYER, 0);
    unsafe {
        add(&mut w, g, a);
        add(&mut w, g, b);
        (*g).mGoal = 0x77 as *mut std::ffi::c_void;
        assert!(request_paths(&mut w, g, &units(g)));
        assert_eq!(w.log[0], "request 80 (2, 2) (9, 3)");
        let r = requests(g);
        assert_eq!(r.len(), 1);
        assert_eq!(r[0].0, 41);
        assert_eq!(r[0].1.unit_ids, vec![1, 2], "the cluster's units (from the last) reversed");
        let path = r[0].1.path;
        assert_eq!(w.paths[&path].refs, 1, "the request's reference");
        assert_eq!(path_of_unit(g, 1), path);
        // not found yet: kept; found: followed and dropped
        move_along_found_paths(&mut w, g);
        assert_eq!(requests(g).len(), 1);
        w.paths.get_mut(&path).unwrap().done = true;
        move_along_found_paths(&mut w, g);
        assert!(requests(g).is_empty());
        assert_eq!(w.paths[&path].refs, 0);
        // the units by id from the last: b then a; the inner steps' centres (1.5, 1.5) and (5.5, 2.5)
        assert_eq!(w.group_paths.len(), 2);
        assert_eq!(w.group_paths[0].0, a);
        assert_eq!(w.group_paths[0].1.len(), 2);
        // removing every unit of a request cancels it
        request_all_paths(&mut w, g);
        let path = requests(g)[0].1.path;
        remove_unit(&mut w, g, a);
        remove_unit(&mut w, g, b);
        assert!(requests(g).is_empty());
        assert!(w.log.contains(&format!("cancel {path:#x}")));
        assert_eq!(w.paths[&path].refs, 0);
        dereference(&mut w, g);
    }
}

#[test]
fn active_groups_save_load_and_restore() {
    let mut w = World::default();
    let a = w.unit(0x10, 1, 7, 0b1);
    let b = w.unit(0x20, 2, 9, 0b10);
    let g = new(&mut w, EE_UNITS_GROUP_KIND_ACTIVE, PLAYER, 0);
    unsafe {
        add_member(&mut w, g, a, false);
        base_add(&mut w, g, b);
        (*g).mFields.mAction = 77;
        (*g).mFields.mSpeed = 2.5;
        (*g).mFields.mIsQueued = true;
        tell_group(&mut w, g, b, [4.0, 5.0, 6.0]);
        (*g).mGoal = 0x77 as *mut std::ffi::c_void;
        request_all_paths(&mut w, g);
        w.lists.insert((PLAYER, 0), vec![g as u64]);
        let mut f = File::default();
        save_all(&mut w, &mut f);
        let path = requests(g)[0].1.path;
        assert_eq!(f.paths, vec![path]);
        // the loaded request is a new one with one reference
        w.paths.insert(0x7777, Path { id: 99, refs: 1, ..Default::default() });
        f.loaded.push_back(0x7777);
        let mut lists = load_all(&mut w, &mut f).expect("loaded");
        assert!(f.values.is_empty(), "every value read back");
        assert_eq!(lists.len(), 4, "two players' running and pending lists");
        assert_eq!(lists[0].len(), 1);
        let s = &lists[0][0];
        assert_eq!(s.units, vec![(1, true), (2, false)]);
        assert_eq!(s.type_counts, vec![(7, 1), (9, 1)]);
        assert_eq!((s.kind_flags, s.action), (0b11, 77));
        assert_eq!(s.destinations, vec![(2, [4.0, 5.0, 6.0])]);
        assert_eq!(s.values[0], 2.5);
        assert_eq!(s.flags, [false, true, false, false]);
        assert_eq!(s.requests[0].unit_ids, vec![1, 2]);
        assert_eq!(s.formation.heading, 1.5);
        w.lists.clear();
        restore_all(&mut w, &mut lists);
        let r = w.lists[&(PLAYER, 0)][0] as G;
        assert_eq!(units(r), vec![a, b]);
        assert_eq!(w.u(a).active_group, r as u64);
        assert_eq!(type_counts(r), vec![(7, 1), (9, 1)]);
        assert_eq!((*r).mFields.mAction, 77);
        assert!((*r).mFields.mIsQueued);
        assert_eq!(destinations(r), vec![(2, [4.0, 5.0, 6.0])]);
        assert_eq!(requests(r)[0].1.path, 0x7777);
        assert_eq!(w.paths[&0x7777].refs, 1, "the group request's reference");
        assert!(w.log.iter().any(|l| l.starts_with("await 99")));
        // the search resolved: the rebuilt request replaces the loaded one; the wait's group reference released
        w.paths.insert(0x7778, Path { id: 99, refs: 1, ..Default::default() });
        let before = references(r);
        path_resolved(&mut w, r, 41, 0x7778);
        assert_eq!(requests(r)[0].1.path, 0x7778);
        assert_eq!(w.paths[&0x7777].refs, 0);
        assert_eq!(references(r), before - 1);
    }
}

#[test]
fn the_players_groups_finish_release_and_validate() {
    let mut w = World::default();
    let a = w.unit(0x10, 1, 7, 0);
    let empty = new(&mut w, EE_UNITS_GROUP_KIND_ACTIVE, PLAYER, 0);
    let busy = new(&mut w, EE_UNITS_GROUP_KIND_ACTIVE, PLAYER, 0);
    unsafe {
        add_member(&mut w, busy, a, false);
        w.lists.insert((PLAYER, 0), vec![empty as u64, busy as u64]);
        // the empty running group finishes (referenced by the finished list), then is released twice and dropped
        removals_done(&mut w, PLAYER);
        assert_eq!(w.lists[&(PLAYER, 0)], vec![busy as u64]);
        assert!(!w.lists.contains_key(&(PLAYER, 2)));
        assert_eq!(references(busy), 2);
        // validation: a dead unit leaves the running group, which finishes
        w.selections = [
            new(&mut w, EE_UNITS_GROUP_KIND_SELECTED, PLAYER, 0) as u64,
            new(&mut w, EE_UNITS_GROUP_KIND_SELECTED, PLAYER, 0) as u64,
        ];
        w.hot_keys = (0..20).map(|i| new(&mut w, EE_UNITS_GROUP_KIND_SAVED, PLAYER, i + 1) as u64).collect();
        w.u(a).hit_points = 0;
        validate_player(&mut w, PLAYER);
        assert_eq!(len(busy), 0);
        assert_eq!(w.lists[&(PLAYER, 2)], vec![busy as u64]);
        clear_selections(&mut w, PLAYER);
        release_running(&mut w, PLAYER);
        assert!(!w.lists.contains_key(&(PLAYER, 0)));
        release_others(&mut w, PLAYER);
    }
    assert_eq!(w.refs(a), 0);
    assert!(w.formations.values().all(|f| f.1 == 0), "every group released its formation");
}

impl crate::units::selection::SelectionHost for World {
    fn player_unit(&mut self, _player: u64, id: i64) -> u64 {
        self.world_unit(id)
    }
    fn selectable(&mut self, _unit: u64) -> bool {
        true
    }
    fn has_doppelganger(&mut self, _player: u64, _unit: u64) -> bool {
        false
    }
    fn executing_local_selection(&mut self) -> bool {
        true
    }
    fn play_sound(&mut self, _player: u64, unit: u64, which: u32) {
        self.log.push(format!("sound {which} {unit:#x}"));
    }
    fn acknowledgement_delay(&mut self) -> std::os::raw::c_ulong {
        5
    }
    fn viewed_unit(&mut self, _player: u64) -> u64 {
        self.viewed
    }
    fn set_viewed_unit(&mut self, _player: u64, unit: u64) {
        self.viewed = unit;
    }
    fn sees_all(&mut self, _player: u64) -> bool {
        false
    }
    fn acknowledged(&mut self, _player: u64) -> (c_long, c_long) {
        self.acks
    }
    fn set_acknowledged(&mut self, _player: u64, group: c_long, time: c_long) {
        self.acks = (group, time);
    }
    fn select_pick(&mut self, _player: u64, ids: &[u32]) -> Vec<u32> {
        ids.iter().copied().filter(|i| i % 2 == 1).collect()
    }
}

#[test]
fn the_selection_members_select_view_toggle_and_acknowledge() {
    use crate::units::selection as sel;
    let mut w = World::default();
    let a = w.unit(0x10, 1, 7, 0);
    let b = w.unit(0x20, 2, 7, 0);
    let c = w.unit(0x30, 3, 7, 0);
    for u in [a, b, c] {
        w.u(u).has_tile_node = true;
        w.u(u).built = true;
    }
    let foreign = w.unit(0x40, 4, 7, 0);
    w.u(foreign).player = OTHER_PLAYER;
    w.selections = [
        new(&mut w, EE_UNITS_GROUP_KIND_SELECTED, PLAYER, 0) as u64,
        new(&mut w, EE_UNITS_GROUP_KIND_SELECTED, PLAYER, 0) as u64,
    ];
    let g = w.selections[0] as G;
    unsafe {
        // the pick keeps the odd ids; the last plays its sound
        sel::select_units(&mut w, PLAYER, &[1, 2, 3]);
        assert_eq!(units(g), vec![a, c]);
        assert_eq!(w.log.last().unwrap(), &format!("sound 0 {c:#x}"));
        // toggling removes the selected ones and adds the others
        sel::add_units(&mut w, PLAYER, &[1, 2], true);
        assert_eq!(units(g), vec![c, b]);
        // a foreign unit is refused while own units are selected, and only viewed when none are
        assert!(!sel::try_select_unit(&mut w, PLAYER, foreign, true));
        clear(&mut w, g);
        assert!(sel::try_select_unit(&mut w, PLAYER, foreign, true));
        assert_eq!(w.viewed, foreign);
        assert_eq!(w.refs(foreign), 1);
        // selecting own units again drops the viewed one
        sel::select_unit(&mut w, PLAYER, 1, true, true);
        assert_eq!(w.viewed, 0);
        assert_eq!(w.refs(foreign), 0);
        assert_eq!(units(g), vec![a]);
        // a new selection for another player's unit
        sel::add_unit_to_selected(&mut w, PLAYER, 4, true);
        assert!(units(g).is_empty());
        assert_eq!(w.viewed, foreign);
        sel::deselect_unit(&mut w, PLAYER, foreign);
        assert_eq!(w.viewed, 0);
        // the acknowledgement: once per delay for the same selection
        w.log.clear();
        let change = (*g).mChangeTime;
        w.acks = (change, w.time + 1);
        sel::play_acknowledgement(&mut w, PLAYER, a);
        assert!(w.log.is_empty(), "within the delay");
        w.acks = (change - 1, 0);
        sel::play_acknowledgement(&mut w, PLAYER, a);
        assert_eq!(w.log, vec![format!("sound 1 {a:#x}")]);
        assert_eq!(w.acks.0, change);
        for s in w.selections {
            dereference(&mut w, s as G);
        }
    }
}

impl crate::units::selection::UpgradesHost for World {
    fn class_count(&mut self) -> u64 {
        1
    }
    fn upgrade(&mut self, _player: u64, _class: u64) -> u64 {
        0x900
    }
    fn upgrade_values(&mut self, _upgrade: u64) -> (c_long, c_long, c_long, i64) {
        self.upgrade_values
    }
    fn init_upgrade(&mut self, _upgrade: u64) {
        self.upgrade_values = (20, 30, 0, 7);
    }
    fn flight_time(&mut self, _complex: u64) -> c_long {
        5
    }
    fn set_hit_points(&mut self, unit: u64, value: c_long) {
        self.log.push(format!("hp {unit:#x} {value}"));
    }
    fn set_flight_time(&mut self, complex: u64, value: c_long) {
        self.log.push(format!("flight {complex:#x} {value}"));
    }
    fn make_area_effects(&mut self, complex: u64) {
        self.log.push(format!("effects {complex:#x}"));
    }
    fn add_civilization_upgrades(&mut self, _player: u64) {
        self.log.push("civ".into());
    }
}

#[test]
fn refreshed_upgrades_rescale_the_classes_units() {
    let mut w = World::default();
    let a = w.unit(0x10, 1, 7, 0);
    w.u(a).built = true;
    w.u(a).hit_points = 7;
    let other = w.unit(0x20, 2, 9, 0);
    let _ = other;
    // hit points 10 -> 30, flight time 10 -> 20: 7 / 10 * 30 = 21, 5 / 10 * 20 = 10
    w.upgrade_values = (10, 10, 0, 7);
    crate::units::selection::refresh_upgrades(&mut w, PLAYER);
    assert_eq!(w.log, vec!["hp 0x10 21", "flight 0x10 10", "civ"]);
    // unchanged: only the civilization's upgrades
    w.log.clear();
    crate::units::selection::refresh_upgrades(&mut w, PLAYER);
    assert_eq!(w.log, vec!["civ"]);
}
