//! The reads' bodies over a mock world (no game, data or window).

use std::collections::BTreeMap;

use super::*;

#[derive(Clone, Copy, Default)]
struct Unit {
    tile: (i32, i32),
    hp: i32,
    max_hp: i32,
    stats_scale: f32,
    model: bool,
    built: bool,
    unit_type: u32,
    category: u32,
    flags: u32,
    targetable: bool,
    player: u64,
    firing_high: bool,
    fire_over_walls: bool,
    passable: bool,
    in_set: bool,
    command: u32,
    side_part: bool,
    previous: u64,
    owner: u64,
    goal_target: u64,
    location: [f32; 3],
    prev: u64,
    next: u64,
    dummy: bool,
    seen: bool,
    footprint: (i32, i32),
    hit_distance: f32,
}

#[derive(Default)]
struct World {
    units: BTreeMap<u64, Unit>,
    /// a player's id and enemies
    players: BTreeMap<u64, (u32, [u8; 4])>,
    region: Option<Vec<u64>>,
    target: u64,
    path_attempts: u32,
    can_reach: bool,
    attack_reachable: bool,
    ignored: bool,
    in_range: bool,
    contact: bool,
    ballistic: Vec<bool>,
    weapon_tag: u32,
    tag_count: u32,
    intersecting: Vec<u64>,
    range: f32,
    heal_range: f32,
    special: bool,
    projectile: u32,
    collision: f32,
    search_timer: i32,
    sight: f32,
    scale: f32,
    contact_point: [f32; 3],
    acts: Vec<(i32, u64, i32)>,
    intersections: Vec<(u64, bool, u32, f32)>,
}

impl World {
    fn unit(&self, h: u64) -> Unit {
        self.units.get(&h).copied().unwrap_or_default()
    }
}

impl QueryHost for World {
    fn read(&mut self, code: i32, object: u64, arg: i32) -> u32 {
        let u = self.unit(object);
        match code {
            EE_COMBAT_Q_TILE_X => u.tile.0 as u32,
            EE_COMBAT_Q_TILE_Y => u.tile.1 as u32,
            EE_COMBAT_Q_HIT_POINTS => u.hp as u32,
            EE_COMBAT_Q_MAX_HIT_POINTS => u.max_hp as u32,
            EE_COMBAT_Q_STATS_SCALE => u.stats_scale.to_bits(),
            EE_COMBAT_Q_MODEL => u.model as u32,
            EE_COMBAT_Q_BUILT => u.built as u32,
            EE_COMBAT_Q_UNIT_TYPE => u.unit_type,
            EE_COMBAT_Q_CATEGORY => u.category,
            EE_COMBAT_Q_CLASS_FLAGS => u.flags,
            EE_COMBAT_Q_TARGETABLE => u.targetable as u32,
            EE_COMBAT_Q_PLAYER_ID => self.players[&object].0,
            EE_COMBAT_Q_ENEMY => self.players[&object].1[arg as usize] as u32,
            EE_COMBAT_Q_FIRING_HIGH => u.firing_high as u32,
            EE_COMBAT_Q_FIRE_OVER_WALLS => u.fire_over_walls as u32,
            EE_COMBAT_Q_PASSABLE => u.passable as u32,
            EE_COMBAT_Q_UNIT_SET => {
                assert_eq!(arg, 0x2a);
                u.in_set as u32
            }
            EE_COMBAT_Q_SCALE => self.scale.to_bits(),
            EE_COMBAT_Q_COMMAND => u.command,
            EE_COMBAT_Q_SIDE_PART => u.side_part as u32,
            EE_COMBAT_Q_PATH_ATTEMPTS => self.path_attempts,
            EE_COMBAT_Q_CAN_REACH_UNIT => self.can_reach as u32,
            EE_COMBAT_Q_ATTACK_REACHABLE => self.attack_reachable as u32,
            EE_COMBAT_Q_IGNORED => self.ignored as u32,
            EE_COMBAT_Q_SEARCH_TIMER => self.search_timer as u32,
            EE_COMBAT_Q_LINE_OF_SIGHT => self.sight.to_bits(),
            EE_COMBAT_Q_SEEN => u.seen as u32,
            EE_COMBAT_Q_DUMMY_HEAD => (object == 0 || u.dummy) as u32,
            EE_COMBAT_Q_SPECIAL_ATTACK => self.special as u32,
            EE_COMBAT_Q_RANGE => self.range.to_bits(),
            EE_COMBAT_Q_HEAL_RANGE => self.heal_range.to_bits(),
            EE_COMBAT_Q_PROJECTILE_CATEGORY => self.projectile,
            EE_COMBAT_Q_COLLISION_RADIUS => self.collision.to_bits(),
            EE_COMBAT_Q_HIT_DISTANCE_SQUARED => u.hit_distance.to_bits(),
            EE_COMBAT_Q_TAG_COUNT => self.tag_count,
            EE_COMBAT_Q_WEAPON_TAG => self.weapon_tag,
            EE_COMBAT_Q_IN_RANGE_TO_HIT => self.in_range as u32,
            EE_COMBAT_Q_BALLISTIC_HIT => self.ballistic[self.weapon_tag as usize] as u32,
            EE_COMBAT_Q_CONTACT_RANGE => {
                assert_eq!(f32::from_bits(arg as u32), u.hit_distance);
                self.contact as u32
            }
            EE_COMBAT_Q_ATTACK_GOAL => (u.goal_target != 0) as u32,
            EE_COMBAT_Q_REGION_COUNT => self.region.as_ref().map_or(EE_COMBAT_Q_NONE, |r| r.len() as u32),
            EE_COMBAT_Q_CONTACT_X => self.contact_point[0].to_bits(),
            EE_COMBAT_Q_CONTACT_Y => self.contact_point[1].to_bits(),
            EE_COMBAT_Q_CONTACT_Z => self.contact_point[2].to_bits(),
            _ => panic!("read {code}"),
        }
    }

    fn handle(&mut self, code: i32, object: u64, arg: i32) -> u64 {
        let u = self.unit(object);
        match code {
            EE_COMBAT_H_COMPLEX => object,
            EE_COMBAT_H_PREVIOUS_PART => u.previous,
            EE_COMBAT_H_OWNER => u.owner,
            EE_COMBAT_H_GOAL_TARGET => u.goal_target,
            EE_COMBAT_H_PLAYER => u.player,
            EE_COMBAT_H_REGION_UNIT => self.region.as_ref().unwrap()[arg as usize],
            EE_COMBAT_H_TILE_PREV => u.prev,
            EE_COMBAT_H_TILE_NEXT => u.next,
            EE_COMBAT_H_TARGET => self.target,
            _ => panic!("handle {code}"),
        }
    }

    fn act(&mut self, code: i32, object: u64, arg: i32) {
        if code == EE_COMBAT_A_WEAPON_TAG {
            self.weapon_tag = arg as u32;
        }
        self.acts.push((code, object, arg));
    }

    fn location(&mut self, object: u64) -> GeTransformation {
        let mut t = GeTransformation::IDENTITY;
        let p = self.unit(object).location;
        t.m[0][3] = p[0];
        t.m[1][3] = p[1];
        t.m[2][3] = p[2];
        t
    }

    fn intersects(&mut self, candidate: u64, line: &Ge3DLine, segment: bool, lod: u32, scale: f32) -> bool {
        let _ = line;
        self.intersections.push((candidate, segment, lod, scale));
        self.intersecting.contains(&candidate)
    }

    fn footprint_tile(&mut self, object: u64) -> (i32, i32) {
        self.unit(object).footprint
    }
}

const SHOT: u64 = 1;
const LAUNCHER: u64 = 2;
const TARGET: u64 = 3;
const OTHER: u64 = 4;
const WALL: u64 = 5;
const P1: u64 = 100;
const P2: u64 = 200;

fn players(w: &mut World) {
    w.players.insert(P1, (1, [0, 0, 1, 0]));
    w.players.insert(P2, (2, [0, 1, 0, 0]));
}

fn shot_world() -> World {
    let mut w = World { scale: 0.5, contact_point: [3.0, 0.0, 0.0], ..Default::default() };
    players(&mut w);
    let unit = |player, x: f32| Unit {
        hp: 10,
        max_hp: 10,
        stats_scale: 1.0,
        model: true,
        built: true,
        targetable: true,
        player,
        location: [x, 0.0, 0.0],
        ..Default::default()
    };
    w.units.insert(SHOT, Unit { goal_target: TARGET, owner: LAUNCHER, ..unit(P1, 0.0) });
    w.units.insert(LAUNCHER, unit(P1, -5.0));
    w.units.insert(TARGET, unit(P2, 1.5));
    w.units.insert(OTHER, unit(P2, 0.5));
    w
}

#[test]
fn a_shot_hits_its_target_first() {
    let mut w = shot_world();
    w.intersecting = vec![TARGET, OTHER];
    w.region = Some(vec![OTHER]);
    assert_eq!(impact_unit(&mut w, SHOT), TARGET);
    // the segment: the contact point is 3 away, past the 0.5 scale; a small unit's LOD 1, built: scale 1
    assert_eq!(w.intersections, vec![(TARGET, true, 1, 1.0)]);
    assert!(w.acts.is_empty());
}

#[test]
fn an_unfinished_target_is_scaled_by_its_health() {
    let mut w = shot_world();
    w.units.get_mut(&TARGET).unwrap().built = false;
    w.units.get_mut(&TARGET).unwrap().hp = 0;
    w.units.get_mut(&TARGET).unwrap().stats_scale = 2.0;
    w.region = Some(vec![]);
    assert_eq!(impact_unit(&mut w, SHOT), 0);
    assert!(w.intersections.is_empty(), "a dead foundation is never hit");
    w.units.get_mut(&TARGET).unwrap().hp = 0;
    w.units.get_mut(&TARGET).unwrap().hp = 1;
    w.intersecting = vec![TARGET];
    assert_eq!(impact_unit(&mut w, SHOT), TARGET);
    // 1 / 10 = 0.1 (the minimum); a large unit's LOD 2
    assert_eq!(w.intersections, vec![(TARGET, true, 2, 0.1)]);
}

#[test]
fn the_region_walk_skips_friends_shots_and_buildings() {
    let mut w = shot_world();
    w.units.get_mut(&TARGET).unwrap().location = [50.0, 0.0, 0.0];
    let friend = 6;
    let shot = 7;
    let building = 8;
    w.units.insert(friend, Unit { player: P1, ..w.unit(OTHER) });
    w.units.insert(shot, Unit { flags: EE_COMBAT_CLASS_PROJECTILE, ..w.unit(OTHER) });
    w.units.insert(building, Unit { category: CATEGORY_BUILDING, ..w.unit(OTHER) });
    w.units.insert(9, Unit { targetable: false, ..w.unit(OTHER) });
    w.region = Some(vec![LAUNCHER, 9, friend, shot, building, OTHER]);
    w.intersecting = vec![friend, shot, building, OTHER, LAUNCHER];
    w.units.get_mut(&OTHER).unwrap().in_set = true;
    assert_eq!(impact_unit(&mut w, SHOT), OTHER);
    assert_eq!(w.intersections, vec![(OTHER, true, 1, 1.0)]);
    // a fire-high set unit that is not the target: the launcher fires high
    assert_eq!(w.acts, vec![(EE_COMBAT_A_FIRING_HIGH, LAUNCHER, 0)]);
    // a bomb hits buildings
    let mut w2 = shot_world();
    w2.units.get_mut(&TARGET).unwrap().location = [50.0, 0.0, 0.0];
    w2.units.get_mut(&SHOT).unwrap().category = CATEGORY_BOMB;
    w2.units.insert(building, Unit { category: CATEGORY_BUILDING, ..w2.unit(OTHER) });
    w2.region = Some(vec![building]);
    w2.intersecting = vec![building];
    assert_eq!(impact_unit(&mut w2, SHOT), building);
}

#[test]
fn some_shots_hit_only_their_target() {
    let mut w = shot_world();
    w.intersecting = vec![OTHER];
    w.region = Some(vec![OTHER]);
    for ty in [TYPE_FLAMING_ARROW, TYPE_PILUM, TYPE_CP_ICBM] {
        w.units.get_mut(&SHOT).unwrap().unit_type = ty;
        assert_eq!(impact_unit(&mut w, SHOT), 0);
    }
    w.units.get_mut(&SHOT).unwrap().unit_type = 0;
    w.region = None;
    assert_eq!(impact_unit(&mut w, SHOT), 0);
}

#[test]
fn walls_are_hit_unless_passable_or_fired_over() {
    let mut w = shot_world();
    w.units.get_mut(&TARGET).unwrap().location = [50.0, 0.0, 0.0];
    w.units.insert(WALL, Unit { flags: EE_COMBAT_CLASS_WALL, player: P1, ..w.unit(OTHER) });
    w.region = Some(vec![WALL]);
    w.intersecting = vec![WALL];
    assert_eq!(impact_unit(&mut w, SHOT), WALL, "a wall of any player");
    w.units.get_mut(&LAUNCHER).unwrap().fire_over_walls = true;
    w.units.get_mut(&LAUNCHER).unwrap().firing_high = true;
    assert_eq!(impact_unit(&mut w, SHOT), 0, "fired over");
    // the target's own wall (one part chain) is hit even when firing high
    w.units.get_mut(&TARGET).unwrap().previous = WALL;
    assert_eq!(impact_unit(&mut w, SHOT), WALL);
    w.units.get_mut(&WALL).unwrap().passable = true;
    assert_eq!(impact_unit(&mut w, SHOT), 0);
}

#[test]
fn reach_by_contact_range_and_ballistics() {
    let mut w = World { range: 4.0, ..Default::default() };
    w.units.insert(TARGET, Unit { hp: 1, hit_distance: 9.0, ..Default::default() });
    w.units.insert(SHOT, Unit { flags: EE_COMBAT_CLASS_MELEE, ..Default::default() });
    w.contact = true;
    assert!(in_reach(&mut w, SHOT, TARGET));
    w.range = 0.0;
    assert!(!in_reach(&mut w, SHOT, TARGET), "no range");
    w.range = 4.0;
    w.units.get_mut(&TARGET).unwrap().hp = 0;
    assert!(!in_reach(&mut w, SHOT, TARGET), "a dead target");
    w.units.get_mut(&TARGET).unwrap().hp = 1;
    // ranged without a projectile class: the range test
    w.units.get_mut(&SHOT).unwrap().flags = 0;
    w.projectile = EE_COMBAT_Q_NONE;
    w.in_range = true;
    assert!(in_reach(&mut w, SHOT, TARGET));
    // a ballistic shooter tries its tag points from the current one round
    w.projectile = CATEGORY_PROJECTILE;
    w.tag_count = 3;
    w.weapon_tag = 1;
    w.ballistic = vec![true, false, false];
    assert!(in_reach(&mut w, SHOT, TARGET));
    assert_eq!(w.acts, vec![(EE_COMBAT_A_WEAPON_TAG, SHOT, 2), (EE_COMBAT_A_WEAPON_TAG, SHOT, 0)]);
    assert_eq!(w.weapon_tag, 0, "the point that hit");
    w.acts.clear();
    w.ballistic = vec![false; 3];
    assert!(!in_reach(&mut w, SHOT, TARGET));
    assert_eq!(w.weapon_tag, 2, "the last point tried");
    // out of range plus the collision radius
    w.collision = 0.5;
    w.units.get_mut(&TARGET).unwrap().hit_distance = 20.26;
    w.ballistic = vec![true; 3];
    assert!(!in_reach(&mut w, SHOT, TARGET));
    w.units.get_mut(&TARGET).unwrap().hit_distance = 20.25;
    assert!(in_reach(&mut w, SHOT, TARGET));
    // a legionnaire's special attack: its heal range, and not melee
    w.units.get_mut(&SHOT).unwrap().unit_type = TYPE_ROMAN_LEGIONNAIRE;
    w.units.get_mut(&SHOT).unwrap().flags = EE_COMBAT_CLASS_MELEE;
    w.special = true;
    w.heal_range = 0.0;
    assert!(!in_reach(&mut w, SHOT, TARGET));
}

#[test]
fn still_reachable_or_ignored() {
    let mut w = World { target: TARGET, attack_reachable: true, path_attempts: 25, ..Default::default() };
    assert!(still_reachable(&mut w, SHOT));
    assert!(w.acts.is_empty());
    w.path_attempts = 26;
    assert!(!still_reachable(&mut w, SHOT));
    assert_eq!(w.acts, vec![(EE_COMBAT_A_IGNORE, TARGET, 0)]);
    w.acts.clear();
    w.ignored = true;
    assert!(!still_reachable(&mut w, SHOT));
    assert!(w.acts.is_empty(), "already ignored");
    // a melee unit must reach the target first
    w.ignored = false;
    w.path_attempts = 0;
    w.units.insert(SHOT, Unit { flags: EE_COMBAT_CLASS_MELEE, ..Default::default() });
    assert!(!still_reachable(&mut w, SHOT));
    w.can_reach = true;
    assert!(still_reachable(&mut w, SHOT));
}

#[test]
fn a_gate_is_closed_by_its_first_part() {
    let mut w = World { target: TARGET, ..Default::default() };
    w.units.insert(TARGET, Unit { category: CATEGORY_GATE, previous: OTHER, ..Default::default() });
    w.units.insert(OTHER, Unit { previous: WALL, ..Default::default() });
    w.units.insert(WALL, Unit { command: ACTION_CLOSED, ..Default::default() });
    assert!(!gate_closed(&mut w, SHOT));
    w.units.get_mut(&WALL).unwrap().command = 0;
    assert!(gate_closed(&mut w, SHOT), "not closed: true (the original's name)");
    w.units.get_mut(&TARGET).unwrap().side_part = true;
    assert!(!gate_closed(&mut w, SHOT));
    w.units.get_mut(&TARGET).unwrap().category = CATEGORY_BUILDING;
    w.units.get_mut(&TARGET).unwrap().side_part = false;
    assert!(!gate_closed(&mut w, SHOT));
}

fn chain_world() -> World {
    let mut w = World { sight: 3.0, ..Default::default() };
    players(&mut w);
    // the chain: head(10) <- 11 <- SHOT(1) -> 12 -> 13
    let enemy = |x| Unit {
        tile: (x, 0),
        footprint: (x, 0),
        hp: 5,
        targetable: true,
        seen: true,
        player: P2,
        ..Default::default()
    };
    w.units.insert(10, Unit { dummy: true, ..Default::default() });
    w.units.insert(11, Unit { prev: 10, ..enemy(8) });
    w.units.insert(SHOT, Unit { tile: (10, 0), player: P1, prev: 11, next: 12, ..Default::default() });
    w.units.insert(12, Unit { next: 13, ..enemy(12) });
    w.units.insert(13, enemy(14));
    w
}

#[test]
fn an_enemy_in_sight_along_the_chain() {
    let mut w = chain_world();
    assert!(enemy_in_range(&mut w, SHOT));
    w.search_timer = 1;
    assert!(!enemy_in_range(&mut w, SHOT), "the search timer runs");
    w.search_timer = 0;
    // the near ones are allies: 13 is 4 tiles out (16 > 9)
    w.units.get_mut(&11).unwrap().player = P1;
    w.units.get_mut(&12).unwrap().player = P1;
    assert!(!enemy_in_range(&mut w, SHOT));
    w.units.get_mut(&13).unwrap().footprint = (13, 0);
    assert!(enemy_in_range(&mut w, SHOT), "the footprint's nearest tile");
    // the seen test is the first unit's on each side (the original's)
    w.units.get_mut(&12).unwrap().seen = false;
    assert!(!enemy_in_range(&mut w, SHOT));
    // nature (player 0) and a unit at 1 hit point are no enemies
    let mut w = chain_world();
    w.units.get_mut(&11).unwrap().hp = 1;
    w.players.insert(P2, (0, [0, 1, 0, 0]));
    w.players.get_mut(&P1).unwrap().1[0] = 1;
    assert!(!enemy_in_range(&mut w, SHOT));
}

fn facing(point: [f32; 2]) -> EeCombatFacingIn {
    let mut location = [0f32; 16];
    location[0] = 1.0;
    location[5] = 1.0;
    location[10] = 1.0;
    location[15] = 1.0;
    EeCombatFacingIn { location, point, firing_arc: 1.0, ..Default::default() }
}

#[test]
fn facing_a_point_ahead() {
    let none = |_| -> EeCombatTurret { panic!("no turrets") };
    // the model's forward is +y
    assert!(faces_point(&facing([0.0, 5.0]), none));
    assert!(!faces_point(&facing([5.0, 5.0]), none));
    let mut wide = facing([5.0, 5.0]);
    wide.use_tolerance = 1;
    wide.firing_arc = 1.6;
    assert!(faces_point(&wide, none), "45 degrees within a 1.6 arc");
    wide.firing_arc = 1.5;
    assert!(!faces_point(&wide, none));
    wide.unit_type = TYPE_CAPITAL_SHIP_YAMATO;
    wide.special_attack = 1;
    wide.firing_arc = 6.2831855;
    assert!(!faces_point(&wide, none), "the special arc replaces the full one");
    wide.unit_type = 0;
    assert!(faces_point(&wide, none), "the full arc");
    // broadside: behind counts
    let mut behind = facing([0.0, -5.0]);
    assert!(!faces_point(&behind, none));
    behind.fires_broadside = 1;
    assert!(faces_point(&behind, none));
    // the point under the unit
    assert!(faces_point(&facing([0.0, 0.0]), none));
    // the facing offset turns the forward (a quarter turn: +y to -x)
    let mut turned = facing([-5.0, 0.0]);
    turned.facing_offset = std::f32::consts::FRAC_PI_2;
    assert!(faces_point(&turned, none));
}

#[test]
fn every_turret_must_face() {
    let mut identity = [0f32; 16];
    identity[0] = 1.0;
    identity[5] = 1.0;
    identity[10] = 1.0;
    identity[15] = 1.0;
    let mut i = facing([0.0, 10.0]);
    i.turrets = 2;
    i.use_tolerance = 1;
    i.scale = 2.0;
    let mut asked = Vec::new();
    let ahead = faces_point(&i, |index| {
        asked.push(index);
        EeCombatTurret { position: [0.5, 0.0, 0.0], transformation: identity, angle: 0.0 }
    });
    assert!(ahead);
    assert_eq!(asked, vec![0, 1]);
    asked.clear();
    // the first turret looks away: the second is never read
    let away = faces_point(&i, |index| {
        asked.push(index);
        EeCombatTurret { position: [0.0; 3], transformation: identity, angle: 2.0 }
    });
    assert!(!away);
    assert_eq!(asked, vec![0]);
}

/// A host that knows ids and answers the C++'s values (the store's must win where it has the unit).
struct Plain {
    asked: Vec<i32>,
}

impl QueryHost for Plain {
    fn read(&mut self, code: i32, object: u64, _arg: i32) -> u32 {
        self.asked.push(code);
        match code {
            EE_COMBAT_Q_ID => object as u32 + 900_000,
            EE_COMBAT_Q_TILE_X => 5,
            EE_COMBAT_Q_LINE_OF_SIGHT => 2.0f32.to_bits(),
            _ => 1,
        }
    }
    fn handle(&mut self, _code: i32, _object: u64, _arg: i32) -> u64 {
        0
    }
    fn act(&mut self, _code: i32, _object: u64, _arg: i32) {}
    fn location(&mut self, _object: u64) -> GeTransformation {
        let mut t = GeTransformation::IDENTITY;
        t.m[0][3] = 1.0;
        t
    }
    fn intersects(&mut self, _candidate: u64, _line: &Ge3DLine, _segment: bool, _lod: u32, _scale: f32) -> bool {
        false
    }
    fn footprint_tile(&mut self, _object: u64) -> (i32, i32) {
        (0, 0)
    }
}

thread_local! {
    static STORE: std::cell::RefCell<BTreeMap<u32, UnitRecord>> = const { std::cell::RefCell::new(BTreeMap::new()) };
}

/// The tests' unit store.
pub(super) fn stored(id: u32) -> Option<UnitRecord> {
    STORE.with(|s| s.borrow().get(&id).copied())
}

fn record(id: u32, object: u64) -> (u32, UnitRecord) {
    (id, UnitRecord { object, ..Default::default() })
}

#[test]
fn plain_reads_come_from_the_unit_store() {
    let (unit_id, mut unit) = record(900_061, 61);
    unit.object_kind = EE_COMBAT_STORED_COMPLEX_UNIT;
    unit.tile_x = 9;
    unit.hit_points = -3;
    unit.location = [7.5f32.to_bits(), 0, 0];
    unit.target_search_timer = 250;
    let (other_id, mut other) = record(900_062, 99);
    other.tile_x = 9;
    STORE.with(|s| {
        s.borrow_mut().insert(unit_id, unit);
        s.borrow_mut().insert(other_id, other);
    });
    let mut host = Plain { asked: Vec::new() };
    {
        let mut h = Stored::new(&mut host);
        assert_eq!(h.read(EE_COMBAT_Q_TILE_X, 61, 0), 9, "the store's");
        assert_eq!(h.read(EE_COMBAT_Q_HIT_POINTS, 61, 0), (-3i32) as u32);
        assert_eq!(h.position(61).x, 7.5);
        assert_eq!(h.read(EE_COMBAT_Q_SEARCH_TIMER, 61, 0), 250);
        // a read the store does not mirror, a record of another object, no record: the host's
        assert_eq!(h.read(EE_COMBAT_Q_LINE_OF_SIGHT, 61, 0), 2.0f32.to_bits());
        assert_eq!(h.read(EE_COMBAT_Q_TILE_X, 62, 0), 5);
        assert_eq!(h.read(EE_COMBAT_Q_TILE_X, 63, 0), 5);
        assert_eq!(h.position(63).x, 1.0);
        assert_eq!(h.read(EE_COMBAT_Q_TILE_X, 0, 0), 5, "no handle");
    }
    // one id read per object in a row
    assert_eq!(host.asked.iter().filter(|&&c| c == EE_COMBAT_Q_ID).count(), 3);
}
