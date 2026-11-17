//! ag.rs' tests over a mock world: groups, units and goals as plain records, every world call logged in order (the
//! statements the C++ bodies made).

use super::*;
use std::collections::BTreeMap;

#[derive(Clone, Default)]
struct MockGroup {
    units: Vec<u64>,
    goal: u64,
    action: i32,
    queued: bool,
    moves_together: bool,
    special: bool,
    values: [f32; 17],
    destinations: Vec<(u32, [f32; 3])>,
}

#[derive(Clone, Copy, Default)]
struct MockGoal {
    kind: i32,
    location: [f32; 3],
    fields: LocationFields,
    target: u64,
    storage: u64,
    resource: u64,
    unload_ok: bool,
    suits: bool,
}

#[derive(Default)]
struct Mock {
    groups: BTreeMap<u64, MockGroup>,
    units: BTreeMap<u64, EeUnitsAgUnit>,
    goals: BTreeMap<u64, MockGoal>,
    slots: Vec<Slot>,
    next: u64,
    log: Vec<String>,
    cap: f32,
    can_attack: bool,
    can_get_to: bool,
    can_take: bool,
    seen: bool,
    tile: Option<(f32, i32)>,
}

impl Mock {
    fn new() -> Self {
        Mock { next: 1000, cap: 2.0, can_attack: true, can_get_to: true, tile: Some((1.0, 0)), ..Default::default() }
    }
    fn obj(&mut self) -> u64 {
        self.next += 1;
        self.next
    }
    fn group_of(&mut self, units: &[u64], goal: u64, action: i32) -> u64 {
        let g = self.obj();
        self.groups.insert(g, MockGroup { units: units.to_vec(), goal, action, ..Default::default() });
        g
    }
    fn soldier(&mut self, x: f32, y: f32) -> u64 {
        let u = self.obj();
        let s = EeUnitsAgUnit {
            obj: u,
            class_obj: 500,
            hit_points: 10,
            commandable: 1,
            is_built: 1,
            location: [x, y, 0.0],
            tile_x: x as i32,
            tile_y: y as i32,
            tile_height: 0.0,
            velocity: 1.0,
            max_velocity: 1.0,
            class_max_velocity: 1.0,
            attack: 3.0,
            category: 1,
            unit_type: 0x10,
            formation_slot: 99,
            ..Default::default()
        };
        self.units.insert(u, s);
        u
    }
    fn goal(&mut self, g: MockGoal) -> u64 {
        let id = self.obj();
        self.goals.insert(id, g);
        id
    }
    fn set(&mut self, u: u64, f: impl FnOnce(&mut EeUnitsAgUnit)) {
        f(self.units.get_mut(&u).unwrap())
    }
    fn say(&mut self, s: String) {
        self.log.push(s)
    }
    fn has(&self, s: &str) -> bool {
        self.log.iter().any(|l| l == s)
    }
}

impl AgHost for Mock {
    fn group(&mut self, g: u64) -> EeUnitsAgGroup {
        let m = &self.groups[&g];
        let kind = if m.goal != 0 { self.goals[&m.goal].kind } else { -1 };
        EeUnitsAgGroup {
            goal: m.goal,
            goal_kind: kind,
            action: m.action,
            count: m.units.len() as u32,
            speed: m.values[0],
            queued: m.queued as u8,
            moves_together: m.moves_together as u8,
            special_attack: m.special as u8,
        }
    }
    fn group_unit(&mut self, g: u64, i: u32) -> u64 {
        self.groups[&g].units[i as usize]
    }
    fn remove(&mut self, g: u64, u: u64) {
        self.groups.get_mut(&g).unwrap().units.retain(|&x| x != u);
        self.say(format!("remove {g} {u}"));
    }
    fn finish(&mut self, g: u64) {
        self.say(format!("finish {g}"));
    }
    fn set_moves_together(&mut self, g: u64) {
        self.groups.get_mut(&g).unwrap().moves_together = true;
    }
    fn request_all_paths(&mut self, g: u64) {
        self.say(format!("paths {g}"));
    }
    fn request_paths(&mut self, g: u64, units: &[u64]) {
        self.say(format!("paths {g} {units:?}"));
    }
    fn send_command(&mut self, g: u64, target: u64, command: u32) {
        self.say(format!("send {g} {target} {command:#x}"));
    }
    fn arrange(&mut self, g: u64, center: [f32; 3], yaw: Option<f32>) -> Vec<Slot> {
        self.say(format!("arrange {g} {center:?} {yaw:?}"));
        self.slots.clone()
    }
    fn new_group(&mut self, _g: u64) -> u64 {
        let ng = self.obj();
        self.groups.insert(ng, MockGroup::default());
        self.say(format!("new group {ng}"));
        ng
    }
    fn add(&mut self, g: u64, u: u64) {
        self.groups.get_mut(&g).unwrap().units.push(u);
    }
    fn set_formation(&mut self, ng: u64, g: u64) {
        self.say(format!("formation {ng} {g}"));
    }
    fn launch(&mut self, g: u64, ng: u64, goal: u64, queued: bool, action: i32, target: u64, command: u32) {
        self.say(format!("launch {g} {ng} goal {goal} {queued} {action} {target} {command:#x}"));
    }
    fn destinations(&mut self, g: u64) -> Vec<(u32, [f32; 3])> {
        self.groups[&g].destinations.clone()
    }
    fn world_unit_sphere(&mut self, id: u32) -> Option<f32> {
        (id != 0).then_some(id as f32)
    }
    fn regions_size(&mut self) -> i32 {
        8
    }
    fn cap_factor(&mut self) -> f32 {
        self.cap
    }
    fn set_values(&mut self, g: u64, values: &[f32; 17]) {
        self.groups.get_mut(&g).unwrap().values = *values;
    }
    fn unit(&mut self, u: u64) -> EeUnitsAgUnit {
        self.units.get(&u).copied().unwrap_or_default()
    }
    fn queued_goal(&mut self, _u: u64, _q: u32) -> u64 {
        0
    }
    fn clear_goal_queue(&mut self, u: u64, q: u32) {
        self.say(format!("clear queue {u} {q}"));
    }
    fn clear_goals(&mut self, u: u64, q: u32) {
        self.say(format!("clear goals {u} {q}"));
    }
    fn queue_goal(&mut self, u: u64, goal: u64, q: u32) {
        self.say(format!("queue {u} {goal} {q}"));
    }
    fn set_goal(&mut self, u: u64, goal: u64, q: u32) {
        self.say(format!("set goal {u} {goal} {q}"));
    }
    fn queue_add(&mut self, u: u64, goal: u64, q: u32) {
        self.say(format!("add {u} {goal} {q}"));
    }
    fn add_time(&mut self, u: u64) {
        self.say(format!("time {u}"));
    }
    fn act(&mut self, u: u64, action: i32) {
        self.say(format!("act {u} {action}"));
    }
    fn squadron(&mut self, u: u64, goal: u64, flag: bool, queued: bool) {
        self.say(format!("squadron {u} {goal} {flag} {queued}"));
    }
    fn clear_gathered(&mut self, u: u64) {
        self.say(format!("ungather {u}"));
    }
    fn set_special_attack(&mut self, u: u64, v: bool) {
        self.say(format!("special {u} {v}"));
    }
    fn set_formation_place(&mut self, u: u64, slot: Option<u32>, rank: i32) {
        self.say(format!("place {u} {slot:?} {rank}"));
    }
    fn can_take(&mut self, _storage: u64, _u: u64, _t: u32) -> bool {
        self.can_take
    }
    fn nearest_footprint(&mut self, _u: u64, target: u64, _x: i32, _y: i32) -> (i32, i32) {
        let t = self.units[&target];
        (t.tile_x - 1, t.tile_y)
    }
    fn has_tile(&mut self, x: i32, y: i32) -> bool {
        x >= 0 && y >= 0
    }
    fn tile_at(&mut self, _p: [f32; 3]) -> Option<(f32, i32)> {
        self.tile
    }
    fn can_get_to(&mut self, _u: u64, _x: i32, _y: i32) -> bool {
        self.can_get_to
    }
    fn can_convert(&mut self, _u: u64, _t: u64, flag: &mut bool) -> bool {
        *flag = true;
        true
    }
    fn can_attack_class(&mut self, _u: u64, _c: u64) -> bool {
        self.can_attack
    }
    fn has_action(&mut self, _c: u64, _a: i32) -> bool {
        false
    }
    fn command_type(&mut self, _u: u64) -> i32 {
        ACTION_IDLE
    }
    fn num_cargo(&mut self, _u: u64) -> u32 {
        1
    }
    fn position(&mut self, u: u64) -> [f32; 3] {
        self.units[&u].location
    }
    fn angle_from_north(&mut self, from: [f32; 3], to: [f32; 3]) -> f32 {
        if from[1] < to[1] {
            0.0
        } else {
            3.0
        }
    }
    fn tile_units(&mut self, x: i32, y: i32) -> Vec<u64> {
        self.units.values().filter(|u| u.tile_x == x && u.tile_y == y && u.tile_size > 1).map(|u| u.obj).collect()
    }
    fn passable_by(&mut self, _o: u64, _b: u64) -> bool {
        false
    }
    fn seen_by(&mut self, _t: u64, _p: u64) -> bool {
        self.seen
    }
    fn can_priest_convert(&mut self, _u: u64, _t: u64) -> bool {
        true
    }
    fn can_cast(&mut self, u: u64, _c: u64, _n: u64) -> bool {
        self.units[&u].power >= 5
    }
    fn reference(&mut self, u: u64) {
        self.say(format!("ref {u}"));
    }
    fn dereference(&mut self, u: u64) {
        self.say(format!("deref {u}"));
    }
    fn rect_closest(&mut self, r: Rect, p: [f32; 2]) -> [f32; 2] {
        [p[0].clamp(r.left, r.right), p[1].clamp(r.top, r.bottom)]
    }
    fn goal_kind(&mut self, goal: u64) -> i32 {
        self.goals.get(&goal).map_or(-1, |g| g.kind)
    }
    fn clone_goal(&mut self, goal: u64) -> u64 {
        let g = self.goals[&goal];
        let c = self.goal(g);
        self.say(format!("clone {goal} {c}"));
        c
    }
    fn delete_goal(&mut self, goal: u64) {
        self.say(format!("delete {goal}"));
    }
    fn location_fields(&mut self, goal: u64) -> LocationFields {
        self.goals[&goal].fields
    }
    fn goal_b1c(&mut self, goal: u64) -> bool {
        self.goals[&goal].fields.b1c
    }
    fn goal_location(&mut self, goal: u64) -> [f32; 3] {
        self.goals[&goal].location
    }
    fn new_location(&mut self, l: NewLocation) -> u64 {
        let g = self.goal(MockGoal { kind: GOAL_LOCATION, location: l.point, ..Default::default() });
        self.say(format!(
            "new location {g} {:?} {} {} {} {} {}",
            l.point, l.b1c, l.forced, l.face, l.yaw, l.group_state
        ));
        g
    }
    fn new_unload(&mut self, u: u64, x: i32, y: i32) -> u64 {
        let ok = self.goals.values().any(|g| g.unload_ok);
        let g = self.goal(MockGoal { kind: GOAL_UNLOAD_TRANSPORT, unload_ok: ok, ..Default::default() });
        self.say(format!("new unload {g} {u} {x} {y}"));
        g
    }
    fn can_unload_at_destination(&mut self, goal: u64) -> bool {
        self.goals[&goal].unload_ok
    }
    fn unload_destination(&mut self, _goal: u64) -> (i32, i32) {
        (10, 20)
    }
    fn set_b1d(&mut self, goal: u64) {
        self.say(format!("b1d {goal}"));
    }
    fn clear_group_path_state(&mut self, goal: u64) {
        self.say(format!("path state {goal}"));
    }
    fn gather_storage(&mut self, goal: u64) -> u64 {
        self.goals[&goal].storage
    }
    fn gather_resource(&mut self, goal: u64) -> u64 {
        self.goals[&goal].resource
    }
    fn set_gather_storage(&mut self, goal: u64, storage: u64) {
        self.say(format!("storage {goal} {storage}"));
    }
    fn attack_target(&mut self, goal: u64) -> u64 {
        self.goals[&goal].target
    }
    fn is_convert_attack(&mut self, _goal: u64) -> bool {
        false
    }
    fn clear_attacker(&mut self, goal: u64) {
        self.say(format!("no attacker {goal}"));
    }
    fn heal_target(&mut self, goal: u64) -> u64 {
        self.goals[&goal].target
    }
    fn suits(&mut self, goal: u64, _u: u64) -> bool {
        self.goals[&goal].suits
    }
    fn set_explore_tile(&mut self, goal: u64, x: i32, y: i32) {
        self.say(format!("explore tile {goal} {x} {y}"));
    }
    fn patrol_point(&mut self, goal: u64) -> u64 {
        self.goals[&goal].target
    }
    fn new_attack_unit(&mut self, first: u64, target: u64) -> u64 {
        let g = self.goal(MockGoal { kind: 2, target, ..Default::default() });
        self.say(format!("new attack {g} {first} {target}"));
        g
    }
}

#[test]
fn compute_values_averages_caps_and_inverts() {
    let mut m = Mock::new();
    let a = m.soldier(0.0, 0.0);
    let b = m.soldier(1.0, 0.0);
    m.set(a, |u| {
        (u.max_velocity, u.acceleration, u.max_angular_velocity, u.angular_acceleration) = (1.0, 2.0, 0.5, 4.0)
    });
    m.set(b, |u| {
        (u.max_velocity, u.acceleration, u.max_angular_velocity, u.angular_acceleration) = (5.0, 2.0, 1.5, 0.0)
    });
    let g = m.group_of(&[a, b], 0, ACTION_IDLE);
    compute_values(&mut m, g);
    let v = m.groups[&g].values;
    // speed: mean 3 capped at 2 x 1 = 2; acceleration 2; turn speed: mean 1 capped at 1; turn acceleration: mean 2
    // capped at 2 x 0 = 0
    assert_eq!(&v[0..3], &[2.0, 5.0, 1.0]);
    assert_eq!(&v[3..6], &[2.0, 2.0, 2.0]);
    assert_eq!(&v[6..9], &[1.0, 1.5, 0.5]);
    assert_eq!(&v[9..12], &[0.0, 4.0, 0.0]);
    assert_eq!(&v[12..16], &[0.5, 0.5, 1.0, 0.0]);
    assert_eq!(v[16], 2.0);
    // an empty group: zeros, the minimums at FLT_MAX, the factor kept
    let e = m.group_of(&[], 0, ACTION_IDLE);
    compute_values(&mut m, e);
    let v = m.groups[&e].values;
    assert_eq!((v[0], v[2], v[5], v[8], v[11], v[16]), (0.0, FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX, 2.0));
}

#[test]
fn keep_filters_remove_in_index_order() {
    let mut m = Mock::new();
    let a = m.soldier(0.0, 0.0);
    let b = m.soldier(1.0, 0.0);
    let c = m.soldier(2.0, 0.0);
    m.set(a, |u| u.attack = 0.0);
    m.set(b, |u| (u.attack, u.unit_type) = (0.0, UNIT_TYPE_HURRICANE));
    m.set(c, |u| u.is_built = 0);
    let g = m.group_of(&[a, b, c], 0, ACTION_IDLE);
    keep_attackers(&mut m, g);
    assert_eq!(m.groups[&g].units, vec![b]);
    assert_eq!(m.log, vec![format!("remove {g} {a}"), format!("remove {g} {c}")]);
    m.set(b, |u| u.max_velocity = 0.0);
    keep_mobile(&mut m, g);
    assert!(m.groups[&g].units.is_empty());
}

#[test]
fn plan_dropoff_queues_takes_or_moves_beside_the_storage() {
    let mut m = Mock::new();
    let storage = m.soldier(10.0, 10.0);
    m.set(storage, |u| u.tile_size = 3);
    let goal = m.goal(MockGoal { kind: GOAL_GATHER, storage, ..Default::default() });
    let busy = m.soldier(0.0, 0.0);
    let carrying = m.soldier(1.0, 0.0);
    let empty = m.soldier(2.0, 0.0);
    let current = m.goal(MockGoal { kind: GOAL_GATHER, ..Default::default() });
    m.set(busy, |u| u.goal_state = current);
    m.set(carrying, |u| (u.resource_amount, u.goal_state) = (5, current));
    m.can_take = true;
    let g = m.group_of(&[busy, carrying, empty], goal, ACTION_DROPOFF_RESOURCE);
    m.groups.get_mut(&g).unwrap().queued = true;
    plan(&mut m, g);
    // the busy unit queues a copy; the carrying one (also busy) too; the empty one moves beside the storage
    assert!(m.log[0].starts_with(&format!("clone {goal}")));
    assert!(m.log[1].starts_with(&format!("queue {busy}")));
    assert!(m.log[3].starts_with(&format!("queue {carrying}")));
    assert!(m.has(&format!("act {empty} {ACTION_PREPARE_TO_MOVE}")));
    assert!(m.log.iter().any(|l| l.starts_with("new location") && l.contains("[9.0, 10.0, 0.0] true true false")));
    // not queued: the carrying unit keeps its gather goal with the new storage
    m.log.clear();
    m.groups.get_mut(&g).unwrap().queued = false;
    plan(&mut m, g);
    assert!(m.has(&format!("ungather {carrying}")));
    assert!(m.has(&format!("storage {current} {storage}")));
    assert!(m.has(&format!("act {carrying} {ACTION_MOVE_TO_STORAGE}")));
}

#[test]
fn plan_attack_unit_by_kind() {
    let mut m = Mock::new();
    let target = m.soldier(20.0, 20.0);
    m.set(target, |u| u.complex = u.obj);
    let goal = m.goal(MockGoal { kind: 2, target, ..Default::default() });
    let airport = m.soldier(0.0, 0.0);
    m.set(airport, |u| u.unit_type = UNIT_TYPE_AIRPORT);
    let held = m.soldier(1.0, 0.0);
    m.set(held, |u| (u.category, u.container) = (CATEGORY_AIRPLANE, 77));
    let landed = m.soldier(2.0, 0.0);
    m.set(landed, |u| (u.category, u.velocity) = (CATEGORY_PLANET_SPACESHIP, 0.0));
    let carrier = m.soldier(3.0, 0.0);
    m.set(carrier, |u| u.ai_kind = AG_AI_KIND_AIRCRAFT_CARRIER);
    let plain = m.soldier(4.0, 0.0);
    let priest = m.soldier(5.0, 0.0);
    m.set(priest, |u| u.is_priest = 1);
    let g = m.group_of(&[airport, held, landed, carrier, plain, priest], goal, ACTION_ATTACK_ENEMY_UNIT);
    m.groups.get_mut(&g).unwrap().special = true;
    plan(&mut m, g);
    assert!(m.has(&format!("squadron {airport} {goal} true false")));
    assert!(m.has(&format!("act {airport} {ACTION_AIRPORT_OPEN}")));
    assert!(m.has(&format!("time {held}")));
    assert!(m.has(&format!("act {landed} {ACTION_TAKEOFF}")));
    assert!(m.has(&format!("clear queue {carrier} 1")));
    assert!(m.has(&format!("act {carrier} {ACTION_AIRPORT_OPEN}")));
    assert!(m.has(&format!("act {plain} {ACTION_INITIAL_ATTACK_STATE}")));
    let act = m.log.iter().position(|l| *l == format!("act {plain} {ACTION_INITIAL_ATTACK_STATE}")).unwrap();
    assert_eq!(m.log[act + 1], format!("special {plain} true"));
    assert!(m.has(&format!("act {priest} {ACTION_CONVERT_ENEMY_UNIT}")));
    assert_eq!(m.log.last().unwrap(), &format!("no attacker {goal}"));
    // the plain attack clears the queue before its copy
    let clear = m.log.iter().position(|l| *l == format!("clear queue {plain} 0")).unwrap();
    assert!(m.log[clear + 1].starts_with(&format!("clone {goal}")));
}

#[test]
fn plan_heal_sends_heal_or_move() {
    let mut m = Mock::new();
    let target = m.soldier(7.0, 8.0);
    let goal = m.goal(MockGoal { kind: 15, target, suits: true, ..Default::default() });
    let a = m.soldier(0.0, 0.0);
    let g = m.group_of(&[a], goal, ACTION_HEAL_UNIT);
    plan(&mut m, g);
    assert!(m.has(&format!("act {a} {ACTION_TURN_TO_FACE_HEALEE}")));
    assert_eq!(m.log.last().unwrap(), &format!("send {g} {target} 0xba"));
    m.goals.get_mut(&goal).unwrap().suits = false;
    m.log.clear();
    plan(&mut m, g);
    assert!(m.log.iter().any(|l| l.starts_with("new location") && l.contains("[7.0, 8.0, 0.0] false true")));
    assert_eq!(m.log.last().unwrap(), &format!("send {g} {target} 0xb8"));
}

#[test]
fn lay_out_near_takes_the_nearest_free_slot_slowest_first() {
    let mut m = Mock::new();
    let goal = m.goal(MockGoal {
        kind: GOAL_LOCATION,
        location: [5.0, 5.0, 0.0],
        fields: LocationFields { b1c: true, forced: false, should_face: true, yaw: 0.25 },
        ..Default::default()
    });
    let fast = m.soldier(4.0, 4.0);
    let slow = m.soldier(6.0, 6.0);
    m.set(slow, |u| u.class_max_velocity = 0.5);
    let g = m.group_of(&[fast, slow], goal, ACTION_PREPARE_TO_MOVE);
    m.slots = vec![
        Slot { position: [6.0, 5.0, 0.0], heading: 0.1, unit: 0 },
        Slot { position: [4.0, 5.0, 0.0], heading: 0.2, unit: 0 },
    ];
    lay_out_formation(&mut m, g);
    assert_eq!(m.log[0], format!("arrange {g} [5.0, 5.0, 0.0] Some(0.25)"));
    // the slow unit first: (6, 6) is nearer slot 0; the fast one takes slot 1
    assert_eq!(m.log[1], format!("place {slow} Some(0) 0"));
    assert!(m.log[2].starts_with("new location") && m.log[2].contains("[6.0, 5.0, 0.0] true false true 0.1 1"));
    assert!(m.has(&format!("place {fast} Some(1) 2")));
    assert!(m.has(&format!("act {fast} {ACTION_PREPARE_TO_MOVE}")));
    assert_eq!(m.log.last().unwrap(), &format!("paths {g}"));
}

#[test]
fn lay_out_far_orders_slots_and_units_across_the_move() {
    let mut m = Mock::new();
    let goal = m.goal(MockGoal { kind: GOAL_LOCATION, location: [50.0, 0.0, 0.0], ..Default::default() });
    let a = m.soldier(0.0, 3.0);
    let b = m.soldier(0.0, 0.0);
    let g = m.group_of(&[a, b], goal, ACTION_PREPARE_TO_MOVE);
    m.slots = vec![
        Slot { position: [50.0, 0.0, 0.0], ..Default::default() },
        Slot { position: [50.0, 3.0, 0.0], ..Default::default() },
    ];
    lay_out_formation(&mut m, g);
    // the move is along x: the slot and the unit nearer y 0 weigh less and pair
    assert!(m.has(&format!("place {b} Some(0) 0")));
    assert!(m.has(&format!("place {a} Some(1) 2")));
}

#[test]
fn lay_out_gives_held_aircraft_both_queues_and_queued_units_paths() {
    let mut m = Mock::new();
    let goal = m.goal(MockGoal { kind: GOAL_LOCATION, location: [5.0, 5.0, 0.0], ..Default::default() });
    let held = m.soldier(0.0, 0.0);
    m.set(held, |u| (u.category, u.container) = (CATEGORY_AIRPLANE, 9));
    let busy = m.soldier(4.0, 4.0);
    let idle = m.soldier(6.0, 6.0);
    let current = m.goal(MockGoal { kind: GOAL_LOCATION, ..Default::default() });
    m.set(busy, |u| u.goal_state = current);
    let g = m.group_of(&[held, busy, idle], goal, ACTION_PREPARE_TO_MOVE);
    m.groups.get_mut(&g).unwrap().queued = true;
    m.slots = vec![
        Slot { position: [4.0, 4.0, 0.0], unit: 0, heading: 0.0 },
        Slot { position: [6.0, 6.0, 0.0], unit: 0, heading: 0.0 },
    ];
    lay_out_formation(&mut m, g);
    assert_eq!(m.log[0], format!("clear queue {held} 0"));
    assert_eq!(m.log[1], format!("clear queue {held} 1"));
    assert!(m.log[2].starts_with("new location"));
    assert!(m.log[3].starts_with(&format!("add {held}")) && m.log[3].ends_with(" 0"));
    assert!(m.log[5].starts_with(&format!("add {held}")) && m.log[5].ends_with(" 1"));
    assert_eq!(m.log[6], format!("time {held}"));
    assert_eq!(m.log[7], format!("remove {g} {held}"));
    assert!(m.log.iter().any(|l| l.starts_with("path state")));
    assert!(m.log.iter().any(|l| l.starts_with(&format!("queue {busy}"))));
    assert_eq!(m.log.last().unwrap(), &format!("paths {g} [{idle}]"));
}

#[test]
fn lay_out_unload_sends_the_unload_command() {
    let mut m = Mock::new();
    let goal = m.goal(MockGoal {
        kind: GOAL_UNLOAD_TRANSPORT,
        location: [5.0, 5.0, 0.0],
        unload_ok: true,
        ..Default::default()
    });
    let t = m.soldier(10.0, 10.0);
    m.set(t, |u| u.is_transport = 1);
    let g = m.group_of(&[t], goal, ACTION_UNLOAD_TRANSPORT);
    m.slots = vec![Slot { position: [5.5, 4.5, 0.0], ..Default::default() }];
    plan(&mut m, g);
    // heading from the first unit's tile (10, 10) to the destination (10, 20)
    assert_eq!(m.log[0], format!("arrange {g} [5.0, 5.0, 0.0] {:?}", Some(um::um_arctan2(-0.0, 10.0))));
    assert!(m.log.iter().any(|l| l.starts_with("new unload") && l.ends_with(&format!("{t} 5 4"))));
    assert!(m.has(&format!("send {g} 0 0xbb")));
}

#[test]
fn spread_around_unit_splits_the_unable() {
    let mut m = Mock::new();
    let target = m.soldier(9.0, 9.0);
    let able = m.soldier(0.0, 0.0);
    let ranged = m.soldier(1.0, 0.0);
    m.set(ranged, |u| (u.range, u.line_of_sight) = (5.0, 2.0));
    m.can_attack = false;
    m.seen = false;
    m.set(able, |u| u.unit_type = UNIT_TYPE_CP_MISSILE_BASE);
    let g = m.group_of(&[able, ranged], 0, ACTION_IDLE);
    spread_around_unit(&mut m, g, target, 9, 9);
    // the ranged unit is unseen: removed, kept out of the split (slow); nothing splits
    assert_eq!(m.groups[&g].units, vec![able]);
    assert!(!m.log.iter().any(|l| l.starts_with("new group")));
    m.set(ranged, |u| u.range = 1.0);
    m.groups.get_mut(&g).unwrap().units.push(ranged);
    m.log.clear();
    spread_around_unit(&mut m, g, target, 9, 9);
    let ng: u64 = m.log.iter().find_map(|l| l.strip_prefix("new group ")).unwrap().parse().unwrap();
    let heading = um::um_arctan2(-(9.0 + 0.5 - 1.0), 9.0 + 0.5 - 0.0);
    assert!(m
        .log
        .iter()
        .any(|l| l.starts_with("new location") && l.contains(&format!("[9.0, 9.0, 0.0] true true true {heading} 0"))));
    assert!(m.log.last().unwrap().starts_with(&format!("launch {g} {ng}")));
}

#[test]
fn spread_around_tile_moves_beside_a_building() {
    let mut m = Mock::new();
    let building = m.soldier(5.0, 5.0);
    m.set(building, |u| (u.tile_size, u.commandable, u.category) = (2, 0, CATEGORY_LAND_CITIZEN));
    let citizen = m.soldier(0.0, 0.0);
    m.set(citizen, |u| u.category = CATEGORY_LAND_CITIZEN);
    let soldier = m.soldier(1.0, 1.0);
    let g = m.group_of(&[citizen, soldier], 0, ACTION_IDLE);
    spread_around_tile(&mut m, g, 5, 5, false);
    assert_eq!(m.groups[&g].units, vec![citizen]);
    assert!(m.log.iter().any(|l| l.starts_with("new location") && l.contains("[4.0, 5.0, 0.0]")));
    // for an unload, the transports stay
    let t = m.soldier(2.0, 2.0);
    m.set(t, |u| u.is_transport = 1);
    let g2 = m.group_of(&[citizen, t], 0, ACTION_IDLE);
    spread_around_tile(&mut m, g2, 5, 5, true);
    assert_eq!(m.groups[&g2].units, vec![t]);
}

#[test]
fn split_calamity_casters_holds_a_reference_across_the_removal() {
    let mut m = Mock::new();
    let target = m.soldier(9.0, 9.0);
    let caster = m.soldier(0.0, 0.0);
    m.set(caster, |u| (u.has_special_attack, u.power) = (1, 5));
    let weak = m.soldier(1.0, 0.0);
    m.set(weak, |u| (u.has_special_attack, u.power) = (1, 1));
    let g = m.group_of(&[caster, weak], 0, ACTION_IDLE);
    split_calamity_casters(&mut m, g, 1, 2, target);
    assert_eq!(m.groups[&g].units, vec![caster]);
    let r = m.log.iter().position(|l| *l == format!("ref {weak}")).unwrap();
    assert_eq!(m.log[r + 1], format!("remove {g} {weak}"));
    assert!(m.log[r + 3] == format!("deref {weak}") || m.log[r + 2] == format!("deref {weak}"));
    assert!(m.log.last().unwrap().contains(&format!("{ACTION_ATTACK_ENEMY_UNIT} 0 0xb8")));
}

#[test]
fn reserved_destinations() {
    let mut m = Mock::new();
    let g = m.group_of(&[], 0, ACTION_IDLE);
    m.groups.get_mut(&g).unwrap().destinations = vec![(0, [1.0, 1.0, 0.0]), (2, [10.0, 10.0, 0.0])];
    assert!(is_reserved(&mut m, g, [1.0, 1.0, 0.0]));
    // unit 2's sphere is 2: within 1 (squared 1) of (10, 10)
    assert!(is_reserved(&mut m, g, [10.5, 10.5, 0.0]));
    assert!(!is_reserved(&mut m, g, [11.0, 11.0, 0.0]));
    // cells of 8 tiles
    assert!(is_cell_taken(&mut m, g, [15.9, 8.0, 0.0]));
    assert!(!is_cell_taken(&mut m, g, [16.0, 8.0, 0.0]));
}

#[test]
fn plan_explore_and_patrol() {
    let mut m = Mock::new();
    let goal = m.goal(MockGoal { kind: 10, ..Default::default() });
    let airport = m.soldier(3.0, 4.0);
    m.set(airport, |u| u.unit_type = UNIT_TYPE_AIRPORT);
    let g = m.group_of(&[airport], goal, ACTION_FIND_UNEXPLORED_AREA);
    plan(&mut m, g);
    let c = m.next;
    assert!(m.has(&format!("explore tile {c} 3 4")));
    assert!(m.has(&format!("squadron {airport} {c} false false")));
    assert!(m.has(&format!("act {airport} {ACTION_FIND_UNEXPLORED_AREA}")));
    assert!(m.has(&format!("act {airport} {ACTION_AIRPORT_OPEN}")));
    let point = m.soldier(8.0, 9.0);
    let patrol = m.goal(MockGoal { kind: 9, target: point, ..Default::default() });
    let walker = m.soldier(0.0, 0.0);
    let g = m.group_of(&[walker], patrol, ACTION_MOVE_TO_NEXT_PATROL_POINT);
    m.log.clear();
    plan(&mut m, g);
    assert!(m.log[0].starts_with("new location") && m.log[0].contains("[8.0, 9.0, 0.0] true true"));
    assert!(m.has(&format!("act {walker} {ACTION_PREPARE_TO_MOVE}")));
}
