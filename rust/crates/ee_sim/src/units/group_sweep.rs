//! TEST ONLY: the group command sweep (docs/RUST_PLAN.md R5b.4 leaf 4 (c); was C++, `EEGroupCommandSweep.cpp`;
//! the units' behaviour lane, round 2). No capture game sends a group command (EE-SIM's human idles; the computer
//! players send only walls), so the Rust bodies of the group commands (group.rs, commands.rs, rally.rs, ...) are run
//! here over the live world through the dry host (dry.rs: every world call logged, nothing changed), every five game
//! minutes, for every player and a set of selections and arguments. The USGC record holds the body's log, so the
//! capture's digest gates it like the others; a dry host mismatch stops EE-SIM (exit code 3).
//!
//! The cases are built from plain facts of the world ([`PlayerFacts`], [`WorldFacts`]: read through the world views
//! and the tech tree's views by [`ee_units_group_command_sweep`]) by [`player_cases`], in the C++ sweep's order and
//! with its field carry-over between cases (each case's record holds the fields its body reads).

/// The bodies (the record's first word).
pub const SWEEP_ATTACK_GROUND: u32 = 1;
pub const SWEEP_PATROL: u32 = 2;
pub const SWEEP_GATHER: u32 = 3;
pub const SWEEP_REPAIR: u32 = 4;
pub const SWEEP_ADD_PATROL_POINT: u32 = 5;
pub const SWEEP_ATTACK_UNIT: u32 = 6;
pub const SWEEP_HEAL_UNIT: u32 = 7;
pub const SWEEP_PATH_TO: u32 = 8;
pub const SWEEP_DROPOFF_RESOURCE: u32 = 9;
pub const SWEEP_BUILD_WALL: u32 = 10;
pub const SWEEP_UNLOAD_TRANSPORT: u32 = 11;
pub const SWEEP_BUILD_UNIT: u32 = 12;
pub const SWEEP_SMART_FARMERS: u32 = 13;
pub const SWEEP_CAST_CALAMITY: u32 = 14;
pub const SWEEP_SET_AIRPORT_COMMAND: u32 = 15;
pub const SWEEP_SET_RALLY_GOAL: u32 = 16;
pub const SWEEP_CLEAR_AIRPORT_COMMANDS: u32 = 17;
pub const SWEEP_STOP_ACTIVE_GROUP: u32 = 18;
pub const SWEEP_LOAD_GARRISON: u32 = 19;
pub const SWEEP_LOAD_TRANSPORT: u32 = 20;
pub const SWEEP_RESEARCH_TECHNOLOGY: u32 = 21;
pub const SWEEP_HACK_A_NEW_UNIT: u32 = 22;

/// The record's tag ("USGC").
pub const SWEEP_TAG: u32 = 0x4347_5355;

/// An id no unit has (the sweep's "no such unit" target).
pub const SWEEP_NO_UNIT: i32 = 0x7e00_0001;

const NONE: i32 = -1;
const UNIT_TYPE_FARM: u64 = 0x2a;
const UNIT_TYPE_GRANARY: u64 = 0x34;
const UNIT_TYPE_NAVY_YARD: u64 = 0x41;
const UNIT_TYPE_AIRPORT: u64 = 0x42;
const UNIT_TYPE_PATH_POINT: u64 = 0xdf;
const UNIT_TYPE_GRANARY_IMPROVED: u64 = 0x2ba;
/// Build Unit's technology no tree has a node of
const NO_TECHNOLOGY: i32 = 0x7e00;
/// Hack A New Unit's class past the table
const NO_CLASS: i32 = 0x7fff;

/// One case: the body and its arguments (`SweepCase`).
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct SweepCase {
    pub body: u32,
    /// which selection (the record's)
    pub selection: u32,
    /// Attack Ground's, Add Patrol Point's, Path To's, ... point
    pub x: f32,
    pub y: f32,
    /// Patrol's, Gather's and Repair's target id (Hack A New Unit's player id)
    pub target: i32,
    pub queued: bool,
    /// Attack Unit's
    pub convert: bool,
    /// Path To's
    pub facing: f32,
    /// Path To's: 1 forced, 2 rotating, 4 queued, 8 group movement
    pub flags: u32,
    /// Build Wall's end
    pub end_x: f32,
    pub end_y: f32,
    /// Build Unit's, Cast Calamity's and Research Technology's (Hack A New Unit's class)
    pub technology: i32,
    /// Cast Calamity's caster; Set Rally Goal's building; Load Transport's passenger
    pub caster: i32,
    /// Set Airport Command's command
    pub command: i32,
}

/// A complex unit of the player's list (in its order), as the sweep reads it.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct OwnUnit {
    pub id: u32,
    /// its class's facts (None: the unit has no class)
    pub class: Option<OwnClass>,
    pub tile: (i64, i64),
    /// `GetPower()`
    pub power: i64,
    /// `mHitPoints < mStats->mMaxHitPoints`
    pub damaged: bool,
}

/// The class facts the sweep reads.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct OwnClass {
    pub unit_type: u64,
    /// `IsLandBuilding()`
    pub land_building: bool,
    /// `mGarrisonCapacity > 0`
    pub garrison: bool,
    /// `IsTransport()`
    pub transport: bool,
}

/// One player's facts.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct PlayerFacts {
    /// its index in the world's players (the record's)
    pub index: u32,
    pub id: u32,
    /// its list's complex units
    pub own: Vec<OwnUnit>,
    /// the next player's first complex unit
    pub next_first: Option<u32>,
    /// the technologies that make a granary, a house and a capitol (-1: none)
    pub build_techs: [i32; 3],
    /// the calamity technologies (one of each class kind, then Teleport)
    pub calamities: Vec<i32>,
    /// the first technology of each kind 0-9 and the first hero's
    pub research: Vec<i32>,
}

/// The world's facts.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct WorldFacts {
    /// the first unit of any list with resources left, and its class's type (None: no class)
    pub resource: Option<(u32, Option<u64>)>,
    /// the map's size (tiles per side)
    pub map_size: i64,
}

/// `(float)(long)v`
fn whole(v: f32) -> f32 {
    v as i32 as f32
}

/// The cases of one player, each handed to `run` with its selection's unit ids, in the C++ sweep's order.
/// `on_map(x, y)`: the map has a tile there.
pub fn player_cases(
    p: &PlayerFacts,
    w: &WorldFacts,
    on_map: impl Fn(i64, i64) -> bool,
    mut run: impl FnMut(&SweepCase, &[u32]),
) {
    let own: Vec<u32> = p.own.iter().map(|u| u.id).collect();
    let tile_of = |id: u32| p.own.iter().find(|u| u.id == id).map(|u| u.tile);
    let path_point = p.own.iter().find(|u| u.class.is_some_and(|c| c.unit_type == UNIT_TYPE_PATH_POINT)).map(|u| u.id);
    let reversed: Vec<u32> = own.iter().rev().copied().collect();
    let from_point: Vec<u32> = match path_point {
        Some(pp) => std::iter::once(pp).chain(own.iter().copied()).collect(),
        None => Vec::new(),
    };
    let selections: [&[u32]; 4] = [&own, &reversed, &[], &from_point];
    // the targets: -1 (the nearest town's route), its first unit, the next player's first complex unit, its path
    // point, an id no unit has
    let mut targets = vec![NONE];
    targets.extend(own.first().map(|&i| i as i32));
    targets.extend(p.next_first.map(|i| i as i32));
    targets.extend(path_point.map(|i| i as i32));
    targets.push(SWEEP_NO_UNIT);
    let is_point = |t: i32| path_point.is_some_and(|pp| t == pp as i32);
    let resource = w.resource.map(|r| r.0 as i32);
    let mut gather_targets = Vec::new();
    gather_targets.extend(resource);
    gather_targets.extend(own.first().map(|&i| i as i32));
    gather_targets.push(SWEEP_NO_UNIT);
    let target2 = targets.get(2).copied();
    for (s, units) in selections.iter().enumerate() {
        let units: &[u32] = units;
        if s == 3 && units.is_empty() {
            continue;
        }
        let mut c = SweepCase { selection: s as u32, ..SweepCase::default() };
        // Attack Ground at the first unit's tile (else, or off the map: the map's centre) and at the map's centre
        c.body = SWEEP_ATTACK_GROUND;
        let first_tile = units.first().and_then(|&id| tile_of(id));
        let on = first_tile.is_some_and(|(x, y)| x >= 0 && y >= 0 && on_map(x, y));
        let center = (w.map_size / 2) as f32 + 0.5;
        let (fx, fy) = match first_tile {
            Some((x, y)) if on => (x as f32 + 0.5, y as f32 + 0.5),
            _ => (center, center),
        };
        c.x = fx;
        c.y = fy;
        run(&c, units);
        c.x = center;
        c.y = center;
        run(&c, units);
        // Add Patrol Point at the same two points
        c.body = SWEEP_ADD_PATROL_POINT;
        run(&c, units);
        c.x = fx;
        c.y = fy;
        run(&c, units);
        // Path To at the same two points, with no flag and with all four (facing 0.75)
        c.body = SWEEP_PATH_TO;
        for f in 0..2 {
            c.flags = if f == 1 { 0xf } else { 0 };
            c.facing = if f == 1 { 0.75 } else { 0.0 };
            c.x = fx;
            c.y = fy;
            run(&c, units);
            c.x = center;
            c.y = center;
            run(&c, units);
        }
        c.flags = 0;
        c.facing = 0.0;
        c.body = SWEEP_PATROL;
        c.x = 0.0;
        c.y = 0.0;
        for &t in &targets {
            c.target = t;
            run(&c, units);
        }
        // Gather, Repair and Attack Unit, each plain and queued
        for q in 0..2 {
            c.queued = q != 0;
            c.body = SWEEP_GATHER;
            for &t in &gather_targets {
                c.target = t;
                run(&c, units);
            }
            c.body = SWEEP_REPAIR;
            for &t in &targets[1..] {
                if is_point(t) {
                    continue;
                }
                c.target = t;
                run(&c, units);
            }
            c.body = SWEEP_ATTACK_UNIT;
            let mut attack: Vec<i32> = targets[1..].to_vec();
            attack.extend(resource);
            for (k, &t) in attack.iter().enumerate() {
                if is_point(t) {
                    continue;
                }
                c.target = t;
                for v in 0..(if k == 0 { 2 } else { 1 }) {
                    c.convert = v != 0;
                    run(&c, units);
                }
            }
            c.convert = false;
        }
        // Heal Unit: its own first unit, the next player's, no unit
        c.body = SWEEP_HEAL_UNIT;
        c.queued = false;
        for &t in &targets[1..] {
            if is_point(t) {
                continue;
            }
            c.target = t;
            run(&c, units);
        }
        // Dropoff Resource
        c.body = SWEEP_DROPOFF_RESOURCE;
        for &t in &targets[1..] {
            c.target = t;
            run(&c, units);
        }
        // Build Wall from the first point eight tiles east, and a one-tile wall at the centre
        c.body = SWEEP_BUILD_WALL;
        c.target = 0x2a;
        c.x = fx;
        c.y = fy;
        c.end_x = fx + 8.0;
        c.end_y = fy;
        run(&c, units);
        c.x = center;
        c.y = center;
        c.end_x = center;
        c.end_y = center;
        run(&c, units);
        c.end_x = 0.0;
        c.end_y = 0.0;
        // Unload Transport at the first point and at the centre
        c.body = SWEEP_UNLOAD_TRANSPORT;
        c.target = 0;
        c.x = whole(fx);
        c.y = whole(fy);
        run(&c, units);
        c.x = whole(center);
        c.y = whole(center);
        run(&c, units);
        // Build Unit: a granary, a house and a capitol and a technology with no node, plain and queued
        c.body = SWEEP_BUILD_UNIT;
        let build = [p.build_techs[0], p.build_techs[1], p.build_techs[2], NO_TECHNOLOGY];
        for (b, &tech) in build.iter().enumerate() {
            if tech == NONE {
                continue;
            }
            c.technology = tech;
            for q in 0..2 {
                c.queued = q != 0;
                c.x = whole(fx);
                c.y = whole(fy);
                run(&c, units);
                if b == 0 {
                    c.x = whole(center);
                    c.y = whole(center);
                    run(&c, units);
                }
            }
        }
        c.queued = false;
        // Smart Farmers at its own first unit, the next player's, its path point, no unit and its first granary
        c.body = SWEEP_SMART_FARMERS;
        c.x = 0.0;
        c.y = 0.0;
        let mut farm: Vec<i32> = targets[1..].to_vec();
        farm.extend(
            p.own
                .iter()
                .find(|u| {
                    u.class.is_some_and(|c| c.unit_type == UNIT_TYPE_GRANARY || c.unit_type == UNIT_TYPE_GRANARY_IMPROVED)
                })
                .map(|u| u.id as i32),
        );
        for &t in &farm {
            c.target = t;
            run(&c, units);
        }
        // Cast Calamity: casters its first unit and its first unit with power; targets none and targets[2]
        c.body = SWEEP_CAST_CALAMITY;
        let mut casters: Vec<i32> = own.first().map(|&i| i as i32).into_iter().collect();
        casters.extend(p.own.iter().find(|u| u.power > 0).map(|u| u.id as i32));
        let cast_targets = [NONE, target2.unwrap_or(NONE)];
        c.x = whole(fx);
        c.y = whole(fy);
        for &caster in &casters {
            for &t in &cast_targets {
                for &k in &p.calamities {
                    c.caster = caster;
                    c.target = t;
                    c.technology = k;
                    run(&c, units);
                }
            }
        }
        c.caster = 0;
        c.technology = 0;
        // Set Airport Command: the commands of each queue kind and one with no queue, plain and queued
        c.body = SWEEP_SET_AIRPORT_COMMAND;
        c.x = fx;
        c.y = fy;
        let mut order = vec![NONE];
        order.extend(target2);
        order.extend(own.first().map(|&i| i as i32));
        order.push(SWEEP_NO_UNIT);
        for command in [8, 10, 13, 15, 20, 11] {
            for &t in &order {
                for q in 0..2 {
                    c.command = command;
                    c.target = t;
                    c.queued = q != 0;
                    run(&c, units);
                }
            }
        }
        c.command = 0;
        c.queued = false;
        // Set Rally Goal: from its first unit, first airport, navy yard and land building
        c.body = SWEEP_SET_RALLY_GOAL;
        let mut buildings: Vec<i32> = own.first().map(|&i| i as i32).into_iter().collect();
        let mut found = [NONE; 6]; // airport, navy yard, building, garrison, farm, damaged
        for u in &p.own {
            let Some(cl) = u.class else { continue };
            let id = u.id as i32;
            let tests = [
                cl.unit_type == UNIT_TYPE_AIRPORT,
                cl.unit_type == UNIT_TYPE_NAVY_YARD,
                cl.land_building,
                cl.garrison,
                cl.unit_type == UNIT_TYPE_FARM,
                cl.land_building && u.damaged,
            ];
            for (slot, hit) in found.iter_mut().zip(tests) {
                if *slot == NONE && hit {
                    *slot = id;
                }
            }
        }
        buildings.extend(found[..3].iter().copied().filter(|&f| f != NONE));
        let mut rally = vec![NONE];
        rally.extend(resource);
        rally.extend(target2);
        rally.extend(path_point.map(|i| i as i32));
        rally.extend(found[3..].iter().copied().filter(|&f| f != NONE));
        rally.push(SWEEP_NO_UNIT);
        for &b in &buildings {
            c.caster = b;
            for t in 0..=rally.len() {
                for q in 0..2 {
                    // the extra target: the building itself (the rally point cleared)
                    c.target = if t < rally.len() { rally[t] } else { b };
                    c.queued = q != 0;
                    run(&c, units);
                }
            }
        }
        c.caster = 0;
        c.queued = false;
        // Clear Airport Commands and Stop Active Group over the selection
        c.target = 0;
        c.x = 0.0;
        c.y = 0.0;
        c.body = SWEEP_CLEAR_AIRPORT_COMMANDS;
        run(&c, units);
        c.body = SWEEP_STOP_ACTIVE_GROUP;
        run(&c, units);
        // Load Garrison: into its first garrison, its first unit, targets[2] and an id no unit has
        let mut carriers: Vec<i32> = Vec::new();
        if found[3] != NONE {
            carriers.push(found[3]);
        }
        carriers.extend(own.first().map(|&i| i as i32));
        carriers.extend(target2);
        carriers.push(SWEEP_NO_UNIT);
        c.body = SWEEP_LOAD_GARRISON;
        for &t in &carriers {
            c.target = t;
            run(&c, units);
        }
        // Load Transport: onto its first transport and the carriers above
        if let Some(t) = p.own.iter().find(|u| u.class.is_some_and(|c| c.transport)) {
            carriers.insert(0, t.id as i32);
        }
        let passengers = [NONE, own.last().map_or(NONE, |&i| i as i32), SWEEP_NO_UNIT];
        c.body = SWEEP_LOAD_TRANSPORT;
        for &t in &carriers {
            for &k in &passengers {
                c.target = t;
                c.caster = k;
                run(&c, units);
            }
        }
        c.caster = 0;
        // Research Technology at its first unit, first land building, targets[2] and an id no unit has
        let mut at: Vec<i32> = own.first().map(|&i| i as i32).into_iter().collect();
        if found[2] != NONE {
            at.push(found[2]);
        }
        at.extend(target2);
        at.push(SWEEP_NO_UNIT);
        c.body = SWEEP_RESEARCH_TECHNOLOGY;
        for &b in &at {
            for &k in &p.research {
                for q in 0..2 {
                    c.target = b;
                    c.technology = k;
                    c.queued = q != 0;
                    run(&c, units);
                }
            }
        }
        c.technology = 0;
        c.queued = false;
        // Hack A New Unit: no class, its first unit's class, its first building's, the resource's and a class past
        // the table, at the first point, the centre, the corner and the far edge
        let mut classes = vec![NONE];
        if let Some(cl) = p.own.first().and_then(|u| u.class) {
            classes.push(cl.unit_type as i32);
        }
        if found[2] != NONE {
            if let Some(cl) = p.own.iter().find(|u| u.id as i32 == found[2]).and_then(|u| u.class) {
                classes.push(cl.unit_type as i32);
            }
        }
        if let Some((_, Some(t))) = w.resource {
            classes.push(t as i32);
        }
        classes.push(NO_CLASS);
        let edge = w.map_size as f32 - 0.5;
        let points = [(fx, fy), (center, center), (0.5, 0.5), (edge, edge)];
        c.body = SWEEP_HACK_A_NEW_UNIT;
        c.target = p.id as i32;
        for &k in &classes {
            for &(x, y) in &points {
                c.technology = k;
                c.x = x;
                c.y = y;
                run(&c, units);
            }
        }
    }
}

/// The USGC record of a case after its body ran: the case's fields its body reads, the selection, the dry log and
/// its mismatches.
pub fn record(c: &SweepCase, player_index: u32, units: &[u32], log: &[u32], mismatches: u32) -> Vec<u8> {
    let mut r = Vec::new();
    let mut put = |v: u32| r.extend_from_slice(&v.to_le_bytes());
    put(c.body);
    put(player_index);
    put(c.selection);
    put(c.x.to_bits());
    put(c.y.to_bits());
    put(c.target as u32);
    let queued = if c.queued { 0x8000_0000 } else { 0 };
    match c.body {
        SWEEP_GATHER | SWEEP_REPAIR => put(u32::from(c.queued)),
        SWEEP_ATTACK_UNIT => put(u32::from(c.queued) | if c.convert { 2 } else { 0 }),
        SWEEP_PATH_TO => {
            put(c.flags);
            put(c.facing.to_bits());
        }
        SWEEP_BUILD_WALL => {
            put(c.end_x.to_bits());
            put(c.end_y.to_bits());
        }
        SWEEP_BUILD_UNIT | SWEEP_RESEARCH_TECHNOLOGY => put(c.technology as u32 | queued),
        SWEEP_CAST_CALAMITY => {
            put(c.technology as u32);
            put(c.caster as u32);
        }
        SWEEP_SET_AIRPORT_COMMAND => put(c.command as u32 | queued),
        SWEEP_SET_RALLY_GOAL => {
            put(c.caster as u32);
            put(u32::from(c.queued));
        }
        SWEEP_LOAD_TRANSPORT => put(c.caster as u32),
        SWEEP_HACK_A_NEW_UNIT => put(c.technology as u32),
        _ => {}
    }
    put(units.len() as u32);
    for &u in units {
        put(u);
    }
    put(log.len() as u32);
    for &w in log {
        put(w);
    }
    put(mismatches);
    r
}

#[cfg(not(test))]
mod game {
    use std::ffi::c_void;
    use std::os::raw::c_long;

    use super::*;
    use crate::units::cls_dispatch::{EE_UNITS_BODY_IS_LAND_BUILDING, EE_UNIT_CLASS_BODIES};
    use crate::units::dry;
    use crate::units::ffi::*;
    use crate::units::host::game::GameHost;
    use crate::units::host::{Host, HostRef};
    use crate::world::view::{ClassView, PlayerView, UnitView, WorldRead, WorldView};

    const AI_KIND_HERO: u64 = 0x1b;
    const UNIT_TYPE_HOUSE: u64 = 0x25;
    const UNIT_TYPE_CAPITOL: u64 = 0x29;
    const TECH_TELEPORT: i64 = 1916;
    const CALAMITY_GROUP_CAST: i32 = 0x12;

    fn land_building(c: &ClassView<'_>) -> bool {
        let k = c.category();
        k < 256 && matches!(EE_UNIT_CLASS_BODIES[EE_UNITS_BODY_IS_LAND_BUILDING][k as usize], 1 | 3)
    }

    /// `IsTransport()`: land, helicopter, naval and space transports and the fortress
    fn transport(c: &ClassView<'_>) -> bool {
        matches!(c.category(), 0x10 | 0x11 | 0x12 | 0x21 | 0x1d)
    }

    fn own_unit(u: &UnitView<'_>) -> OwnUnit {
        let class = u.class().map(|c| OwnClass {
            unit_type: c.unit_type(),
            land_building: land_building(&c),
            garrison: crate::world_field!(c.fields(), mGarrisonCapacity) > 0,
            transport: transport(&c),
        });
        OwnUnit {
            id: u.id(),
            class,
            tile: u.tile_xy(),
            power: u.power(),
            damaged: u.stats().is_some_and(|s| u.hit_points() < s.max_hit_points()),
        }
    }

    /// The tree's node ids, epoch by epoch, in the order of `GetNodeIDs`, with each node's data (None: no data).
    fn nodes<'t>(p: &PlayerView<'t>) -> Vec<(i64, Option<crate::world::view::Record<'t, crate::tech::nodes::EeTechNodeDataFields>>)> {
        let mut out = Vec::new();
        let Some(tree) = p.tech_tree() else { return out };
        for e in 0..tree.epochs() {
            let mut ids: Vec<c_long> = Vec::new();
            tree.epoch_node_ids(e, &mut ids);
            for id in ids {
                if let Some(n) = tree.node(id as i64) {
                    out.push((id as i64, n.data()));
                }
            }
        }
        out
    }

    fn player_facts(t: &WorldRead, w: &WorldView<'_>, index: usize, n: usize) -> Option<PlayerFacts> {
        let p = w.player(index)?;
        let own: Vec<OwnUnit> = p.units().filter(|u| u.is_complex()).map(|u| own_unit(&u)).collect();
        let next_first = w.player((index + 1) % n).and_then(|q| q.units().find(|u| u.is_complex()).map(|u| u.id()));
        let nodes = nodes(&p);
        let find = |unit_type: u64| {
            nodes
                .iter()
                .find(|(_, d)| d.is_some_and(|d| crate::world_field!(d, mUnitClass) as i64 == unit_type as i64))
                .map_or(NONE, |(id, _)| *id as i32)
        };
        let build_techs = [find(UNIT_TYPE_GRANARY), find(UNIT_TYPE_HOUSE), find(UNIT_TYPE_CAPITOL)];
        // the calamity technologies: the first of each class kind (0x12, cast by all the selected, any other), then
        // Teleport when the tree has it
        let mut host = GameHost;
        let mut slots = [NONE; 3];
        for (id, d) in &nodes {
            let Some(d) = d else { continue };
            if crate::world_field!(d, mCalamity) == 0 || *id == TECH_TELEPORT {
                continue;
            }
            let Some((_, class_enum, by_all)) = host.calamity_of(HostRef(p.handle() as *mut c_void), *id as i32)
            else {
                continue;
            };
            let slot = if class_enum == CALAMITY_GROUP_CAST {
                0
            } else if by_all {
                1
            } else {
                2
            };
            if slots[slot] == NONE {
                slots[slot] = *id as i32;
            }
        }
        let mut calamities: Vec<i32> = slots.iter().copied().filter(|&s| s != NONE).collect();
        if p.tech_tree().is_some_and(|tree| tree.node(TECH_TELEPORT).is_some()) {
            calamities.push(TECH_TELEPORT as i32);
        }
        // the first technology of each kind 0-9 and the first hero's
        let mut by_kind = [NONE; 11];
        for (id, d) in &nodes {
            let Some(d) = d else { continue };
            let kind = crate::world_field!(d, mKind) as i64;
            if (0..10).contains(&kind) && by_kind[kind as usize] == NONE {
                by_kind[kind as usize] = *id as i32;
            }
            let class = crate::world_field!(d, mUnitClass) as i64;
            if by_kind[10] == NONE
                && class > 0
                && ((class as u64) as usize) < ClassView::count(t)
                && ClassView::by_index(t, class as usize).is_some_and(|c| c.ai_kind() == AI_KIND_HERO)
            {
                by_kind[10] = *id as i32;
            }
        }
        let research = by_kind.iter().copied().filter(|&k| k != NONE).collect();
        Some(PlayerFacts { index: index as u32, id: p.id(), own, next_first, build_techs, calamities, research })
    }

    fn run_body(c: &SweepCase, player: *mut c_void, selection: *mut c_void) {
        let (x, y) = (c.x, c.y);
        match c.body {
            SWEEP_ATTACK_GROUND => ee_units_attack_ground(player, selection, x, y),
            SWEEP_PATROL => ee_units_patrol(player, selection, c.target),
            SWEEP_GATHER => ee_units_gather(player, selection, c.target as u32, c.queued),
            SWEEP_ADD_PATROL_POINT => ee_units_add_patrol_point(player, selection, x, y),
            SWEEP_ATTACK_UNIT => ee_units_attack_unit(player, selection, c.target as u32, c.queued, c.convert),
            SWEEP_HEAL_UNIT => ee_units_heal_unit(player, selection, c.target as u32),
            SWEEP_PATH_TO => {
                let (mut x, mut y) = (x, y);
                let f = c.flags;
                // SAFETY: the two points are live for the call
                unsafe {
                    ee_units_path_to(
                        player,
                        selection,
                        &mut x,
                        &mut y,
                        c.facing,
                        f & 1 != 0,
                        f & 2 != 0,
                        f & 4 != 0,
                        f & 8 != 0,
                    )
                };
            }
            SWEEP_BUILD_WALL => ee_units_build_wall(player, selection, c.target, x, y, c.end_x, c.end_y),
            SWEEP_UNLOAD_TRANSPORT => ee_units_unload_transport(player, selection, x as i64, y as i64),
            SWEEP_BUILD_UNIT => ee_units_build_unit(player, selection, c.technology, x as i64, y as i64, c.queued),
            SWEEP_SMART_FARMERS => ee_units_smart_farmers(player, selection, c.target as u32),
            SWEEP_CAST_CALAMITY => {
                ee_units_cast_calamity(player, selection, c.caster as u32, c.target, x as i32, y as i32, c.technology)
            }
            SWEEP_DROPOFF_RESOURCE => ee_units_dropoff_resource(player, selection, c.target as u32),
            SWEEP_REPAIR => ee_units_repair(player, selection, c.target as u32, c.queued),
            SWEEP_SET_AIRPORT_COMMAND => {
                ee_units_set_airport_command(player, selection, x, y, c.command, c.target, u8::from(c.queued))
            }
            // the building owner's selection is the sweep's (the dry host's player_selection)
            SWEEP_SET_RALLY_GOAL => ee_units_set_rally_goal(player, c.caster as u32, x, y, c.target, u8::from(c.queued)),
            SWEEP_CLEAR_AIRPORT_COMMANDS => ee_units_clear_airport_commands(selection),
            SWEEP_STOP_ACTIVE_GROUP => ee_units_stop_active_group(selection),
            SWEEP_LOAD_GARRISON => ee_units_load_garrison(player, selection, c.target as u32),
            SWEEP_LOAD_TRANSPORT => ee_units_load_transport(player, selection, c.target as u32, c.caster),
            SWEEP_RESEARCH_TECHNOLOGY => {
                ee_units_research_technology(player, selection, c.target as u32, c.technology, u8::from(c.queued))
            }
            SWEEP_HACK_A_NEW_UNIT => {
                ee_units_hack_new_unit(player, c.technology, x, y, i64::from(c.target));
            }
            _ => {}
        }
    }

    /// TEST ONLY: the sweep (EE-SIM, every five game minutes, while the capture runs; the C++ caller holds the
    /// geometry capture's scope).
    #[no_mangle]
    pub extern "C" fn ee_units_group_command_sweep() {
        if !crate::app::capture::ee_app_capture_active() {
            return;
        }
        // SAFETY: called from the simulation thread between ticks
        let t = unsafe { WorldRead::begin() };
        let Some(w) = WorldView::global(&t) else { return };
        let n = w.num_players() as usize;
        // the first unit of any list with resources left (a mine, a tree, a herd animal)
        let resource = (0..n)
            .filter_map(|p| w.player(p))
            .find_map(|p| p.units().find(|u| u.is_unit() && u.resource_amount() > 0))
            .map(|u| (u.id(), u.class().map(|c| c.unit_type())));
        let map_size = w.map().map_or(0, |m| m.size());
        let facts = WorldFacts { resource, map_size };
        for index in 0..n {
            let Some(p) = player_facts(&t, &w, index, n) else { continue };
            let player = w.player(index).map_or(std::ptr::null_mut(), |v| v.handle() as *mut c_void);
            let on_map = |x: i64, y: i64| w.map().is_some_and(|m| m.tile(x, y).is_some());
            player_cases(&p, &facts, on_map, |c, units| {
                let selection: HostRef = dry::begin(units);
                run_body(c, player, selection.0);
                let (log, mismatches) = dry::end();
                crate::app::capture::commit(SWEEP_TAG, &record(c, p.index, units, &log, mismatches));
                if mismatches != 0 {
                    eprintln!(
                        "EE-GROUP-COMMAND: body {} player {} selection {} target {}: {} dry host mismatches",
                        c.body, p.index, c.selection, c.target, mismatches
                    );
                    crate::app::capture::ee_app_capture_flush();
                    std::process::exit(3);
                }
            });
        }
    }
}

#[cfg(test)]
#[path = "group_sweep_tests.rs"]
mod tests;
