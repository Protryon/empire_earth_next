//! The goal markers (`EEGraphicalGoalManager.cpp`, the class pair `EEGraphicalGoal` / `EEGraphicalGoalManager`): the
//! "go here" lines and icons of the current player's selected units' queued goals, as a whole Rust object. The markers
//! by player, then by type (a `BTreeMap` of `BTreeMap`s of vectors, walked in key order as the original's
//! `std::map`s), `Rebuild`'s walk over the selection, its queues and goals (which marker types a unit's kind shows for
//! which queue, the icons' spacing, the ground points), `AddGoal`, `TrimGoals`, `Update` and the destructor.
//!
//! The C++ answers through [`EeGfxGoalHost`]: the player's and the selected units' facts, the goals' locations and
//! targets, the map's ground and the effects (a marker's effect made, moved and destroyed through `gEffectManager`).
//! Every marker effect is local only and unsaved, so this is the player's view, not the simulation's state; it runs
//! in the simulation tick (`EEEffectManager::Update`), and the effects it makes take handles in the original's order.

use std::collections::BTreeMap;
use std::ffi::c_void;

use ee_ge::types::Ge3DPoint;

/// `kGoalSpacing` (0x441294): the icons' spacing across the queues.
const EE_GFX_MARKER_GOAL_SPACING: f32 = 0.4;
/// `kGoalHeight` (0x441298): the markers' height above the ground.
const EE_GFX_MARKER_GOAL_HEIGHT: f32 = 0.02;
/// The marker types: 0 a marching line, 1 a waypoint icon, 2..6 the rally flags.
const EE_GFX_MARKER_GOAL_TYPES: u32 = 7;

/// The unit categories and type `Rebuild` sorts units by (`EEUnitClassIDs.h`).
const EE_GFX_MARKER_CATEGORY_BUILDING: u32 = 2;
const EE_GFX_MARKER_CATEGORY_AIRPLANE: u32 = 3;
const EE_GFX_MARKER_CATEGORY_NAVAL_BUILDING: u32 = 0x15;
const EE_GFX_MARKER_CATEGORY_SPACE_BUILDING: u32 = 0x20;
const EE_GFX_MARKER_CATEGORY_PLANET_SPACESHIP: u32 = 0x22;
const EE_GFX_MARKER_TYPE_AIRPORT: u32 = 0x42;
/// The goal kinds `Rebuild` reads (`EEUnitGoal.h`).
const EE_GFX_MARKER_GOAL_LOCATION: u32 = 1;
const EE_GFX_MARKER_GOAL_ATTACK_UNIT: u32 = 2;
const EE_GFX_MARKER_GOAL_LOAD_GARRISON: u32 = 12;

/// One marker (`EEGraphicalGoal`, 0x28 bytes in the original).
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct GoalMarker {
    pub from: Ge3DPoint,
    pub to: Ge3DPoint,
    /// its effect's handle (0: none: the graphic effect is above the detail setting)
    pub effect: u32,
    pub kind: u32,
    pub changed: bool,
}

/// The player `Rebuild` places markers for.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxGoalPlayer {
    /// its epoch is Atomic - WW2 or later
    pub late_epoch: bool,
    pub shift_mode: bool,
    /// its selected group's units
    pub unit_count: u32,
}

/// A selected unit.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxGoalUnit {
    pub category: u32,
    pub unit_type: u32,
    pub air_traffic_type: i32,
    /// `mTile != NULL`
    pub on_tile: bool,
    /// `mContainer != NULL`
    pub held: bool,
    pub queue_count: u32,
}

/// A queued goal.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeGfxGoalStep {
    pub kind: u32,
    /// a location goal's automatic waypoint (no marker)
    pub auto_waypoint: bool,
    /// a garrison's container or an attack's target was found (`target` then)
    pub has_target: bool,
    pub target: *mut c_void,
    /// `GetGoalLocation`
    pub to: Ge3DPoint,
}

impl Default for EeGfxGoalStep {
    fn default() -> Self {
        EeGfxGoalStep {
            kind: 0,
            auto_waypoint: false,
            has_target: false,
            target: std::ptr::null_mut(),
            to: Ge3DPoint::default(),
        }
    }
}

/// The C++'s answers (`context`: none; the selection is the player's last asked).
#[repr(C)]
pub struct EeGfxGoalHost {
    pub context: *mut c_void,
    /// `gWorld.mCurrentPlayer`
    pub current_player: extern "C" fn(context: *mut c_void) -> u32,
    /// `gWorld.mNumPlayers`
    pub player_count: extern "C" fn(context: *mut c_void) -> u32,
    pub player: extern "C" fn(context: *mut c_void, player: u32) -> EeGfxGoalPlayer,
    /// the selected group's unit `unit`
    pub unit: extern "C" fn(context: *mut c_void, unit: u32) -> EeGfxGoalUnit,
    pub goal_count: extern "C" fn(context: *mut c_void, unit: u32, queue: u32) -> u32,
    pub goal: extern "C" fn(context: *mut c_void, unit: u32, queue: u32, goal: u32) -> EeGfxGoalStep,
    /// goal `goal`'s location (false: no goal there)
    pub goal_location:
        extern "C" fn(context: *mut c_void, unit: u32, queue: u32, goal: u32, out: *mut Ge3DPoint) -> bool,
    /// the unit's location on its tile, else its container's
    pub unit_location: extern "C" fn(context: *mut c_void, unit: u32, on_tile: bool) -> Ge3DPoint,
    /// a target's outer part when placed; a land building's centre written into `to`: the target as resolved
    pub target: extern "C" fn(context: *mut c_void, target: *mut c_void, to: *mut Ge3DPoint) -> *mut c_void,
    /// the ground point under `point` when its tile is on the map
    pub ground: extern "C" fn(context: *mut c_void, point: *const Ge3DPoint, out: *mut Ge3DPoint) -> bool,
    /// a marker's effect (`EEGraphicalGoal`'s constructor): its handle, 0 for none
    pub create: extern "C" fn(context: *mut c_void, kind: u32, player: u32) -> u32,
    pub destroy: extern "C" fn(context: *mut c_void, effect: u32),
    /// a line's effect to its ends (`SetFrom`, `SetTo`, its transformation at `from`)
    pub move_line: extern "C" fn(context: *mut c_void, effect: u32, from: *const Ge3DPoint, to: *const Ge3DPoint),
    /// an icon's effect to `to` (`SetLocation`)
    pub move_icon: extern "C" fn(context: *mut c_void, effect: u32, to: *const Ge3DPoint),
}

/// The markers' questions (the tests' log).
pub trait GoalHost {
    fn current_player(&mut self) -> u32;
    fn player_count(&mut self) -> u32;
    fn player(&mut self, player: u32) -> EeGfxGoalPlayer;
    fn unit(&mut self, unit: u32) -> EeGfxGoalUnit;
    fn goal_count(&mut self, unit: u32, queue: u32) -> u32;
    fn goal(&mut self, unit: u32, queue: u32, goal: u32) -> EeGfxGoalStep;
    fn goal_location(&mut self, unit: u32, queue: u32, goal: u32) -> Option<Ge3DPoint>;
    fn unit_location(&mut self, unit: u32, on_tile: bool) -> Ge3DPoint;
    fn target(&mut self, target: *mut c_void, to: &mut Ge3DPoint) -> *mut c_void;
    fn ground(&mut self, point: &Ge3DPoint) -> Option<Ge3DPoint>;
    fn create(&mut self, kind: u32, player: u32) -> u32;
    fn destroy(&mut self, effect: u32);
    fn move_line(&mut self, effect: u32, from: &Ge3DPoint, to: &Ge3DPoint);
    fn move_icon(&mut self, effect: u32, to: &Ge3DPoint);
}

impl GoalHost for &EeGfxGoalHost {
    fn current_player(&mut self) -> u32 {
        (self.current_player)(self.context)
    }
    fn player_count(&mut self) -> u32 {
        (self.player_count)(self.context)
    }
    fn player(&mut self, player: u32) -> EeGfxGoalPlayer {
        (self.player)(self.context, player)
    }
    fn unit(&mut self, unit: u32) -> EeGfxGoalUnit {
        (self.unit)(self.context, unit)
    }
    fn goal_count(&mut self, unit: u32, queue: u32) -> u32 {
        (self.goal_count)(self.context, unit, queue)
    }
    fn goal(&mut self, unit: u32, queue: u32, goal: u32) -> EeGfxGoalStep {
        (self.goal)(self.context, unit, queue, goal)
    }
    fn goal_location(&mut self, unit: u32, queue: u32, goal: u32) -> Option<Ge3DPoint> {
        let mut p = Ge3DPoint::default();
        (self.goal_location)(self.context, unit, queue, goal, &mut p).then_some(p)
    }
    fn unit_location(&mut self, unit: u32, on_tile: bool) -> Ge3DPoint {
        (self.unit_location)(self.context, unit, on_tile)
    }
    fn target(&mut self, target: *mut c_void, to: &mut Ge3DPoint) -> *mut c_void {
        (self.target)(self.context, target, to)
    }
    fn ground(&mut self, point: &Ge3DPoint) -> Option<Ge3DPoint> {
        let mut p = Ge3DPoint::default();
        (self.ground)(self.context, point, &mut p).then_some(p)
    }
    fn create(&mut self, kind: u32, player: u32) -> u32 {
        (self.create)(self.context, kind, player)
    }
    fn destroy(&mut self, effect: u32) {
        (self.destroy)(self.context, effect)
    }
    fn move_line(&mut self, effect: u32, from: &Ge3DPoint, to: &Ge3DPoint) {
        (self.move_line)(self.context, effect, from, to)
    }
    fn move_icon(&mut self, effect: u32, to: &Ge3DPoint) {
        (self.move_icon)(self.context, effect, to)
    }
}

/// How `Rebuild` sorts a selected unit (its first matching test).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum UnitKind {
    /// an airplane or a planet spaceship
    Aircraft,
    /// an airport
    Special,
    /// a naval or space building with air traffic
    Carrier,
    /// another building
    Citizen,
    Other,
}

fn unit_kind(u: &EeGfxGoalUnit) -> UnitKind {
    if u.category == EE_GFX_MARKER_CATEGORY_AIRPLANE || u.category == EE_GFX_MARKER_CATEGORY_PLANET_SPACESHIP {
        UnitKind::Aircraft
    } else if u.unit_type == EE_GFX_MARKER_TYPE_AIRPORT {
        UnitKind::Special
    } else if (u.category == EE_GFX_MARKER_CATEGORY_NAVAL_BUILDING
        || u.category == EE_GFX_MARKER_CATEGORY_SPACE_BUILDING)
        && u.air_traffic_type != 0
    {
        UnitKind::Carrier
    } else if u.category == EE_GFX_MARKER_CATEGORY_BUILDING {
        UnitKind::Citizen
    } else {
        UnitKind::Other
    }
}

/// `EEGraphicalGoalManager`: the markers by player, then by type.
#[derive(Debug, Default)]
pub struct EeGfxGoalMarkers {
    pub goals: BTreeMap<u32, BTreeMap<u32, Vec<GoalMarker>>>,
}

impl EeGfxGoalMarkers {
    pub fn new() -> Self {
        EeGfxGoalMarkers::default()
    }

    /// A player's markers of a type (the empty list made when missing, as `operator[]` did).
    fn list(&mut self, player: u32, kind: u32) -> &mut Vec<GoalMarker> {
        self.goals.entry(player).or_default().entry(kind).or_default()
    }

    /// `GetGoalCount`.
    pub fn count(&mut self, player: u32, kind: u32) -> u32 {
        self.list(player, kind).len() as u32
    }

    /// `AddGoal`: a new marker (its effect made) when the list has none at `index` yet.
    fn add<H: GoalHost>(&mut self, h: &mut H, player: u32, index: u32, kind: u32) {
        if index >= self.count(player, kind) {
            let effect = h.create(kind, player);
            self.list(player, kind).push(GoalMarker { effect, kind, ..Default::default() });
        }
    }

    /// `TrimGoals`: the markers past `count` destroyed from the end.
    fn trim<H: GoalHost>(&mut self, h: &mut H, player: u32, count: u32, kind: u32) {
        let list = self.list(player, kind);
        while list.len() as u32 > count {
            let m = list.pop().expect("a marker");
            h.destroy(m.effect);
        }
    }

    /// `Rebuild`: the player's markers for the selected units' queued goals (units only in shift mode, buildings
    /// always), then the leftovers and every other player's trimmed.
    pub fn rebuild<H: GoalHost>(&mut self, h: &mut H, player: u32) {
        let p = h.player(player);
        let mut counts = [0u32; EE_GFX_MARKER_GOAL_TYPES as usize];
        let mut target: *mut c_void = std::ptr::null_mut();
        let mut from = Ge3DPoint::default();
        for u in 0..p.unit_count {
            let unit = h.unit(u);
            let kind = unit_kind(&unit);
            let (other, citizen, special, aircraft, carrier) = (
                kind == UnitKind::Other,
                kind == UnitKind::Citizen,
                kind == UnitKind::Special,
                kind == UnitKind::Aircraft,
                kind == UnitKind::Carrier,
            );
            if !special && !citizen && !carrier && !p.shift_mode {
                continue;
            }
            if !unit.on_tile && !unit.held {
                continue;
            }
            for q in 0..unit.queue_count {
                let mut previous: i64 = -1;
                let goals = h.goal_count(u, q);
                for g in 0..goals {
                    let step = h.goal(u, q, g);
                    if step.kind == EE_GFX_MARKER_GOAL_LOCATION && step.auto_waypoint {
                        continue;
                    }
                    let previous_location =
                        if g > 0 && previous != -1 { h.goal_location(u, q, previous as u32) } else { None };
                    if let Some(l) = previous_location {
                        from = l;
                    } else if unit.on_tile || unit.held {
                        from = h.unit_location(u, unit.on_tile);
                    }
                    let mut to = step.to;
                    if step.kind == EE_GFX_MARKER_GOAL_LOAD_GARRISON || step.kind == EE_GFX_MARKER_GOAL_ATTACK_UNIT {
                        if step.has_target {
                            target = step.target;
                        }
                    } else {
                        target = std::ptr::null_mut();
                    }
                    if !target.is_null() {
                        target = h.target(target, &mut to);
                    }
                    for t in 0..EE_GFX_MARKER_GOAL_TYPES {
                        let mut offset = Ge3DPoint { x: 0.0, y: 0.0, z: EE_GFX_MARKER_GOAL_HEIGHT };
                        match t {
                            0 => {
                                if !(other && q == 0)
                                    && !(special && q != 0 && ((q != 3 && q != 4) || p.late_epoch))
                                    && !(citizen && q == 1 && self.count(player, 2) > q)
                                    && !aircraft
                                    && !(carrier && p.shift_mode)
                                {
                                    continue;
                                }
                                if special {
                                    offset.x = q.wrapping_sub(2) as i32 as f32 * EE_GFX_MARKER_GOAL_SPACING;
                                }
                            }
                            1 => {
                                if !(other && q == 0) && !(carrier && p.shift_mode && q == 0) && !aircraft {
                                    continue;
                                }
                            }
                            2 => {
                                if !citizen || q != 1 {
                                    continue;
                                }
                            }
                            3 => {
                                if !special && !aircraft && !carrier {
                                    continue;
                                }
                                if q != 1 {
                                    continue;
                                }
                                if special {
                                    offset.x = -EE_GFX_MARKER_GOAL_SPACING; // `kGoalSpacing * -1.0f`
                                }
                            }
                            4 => {
                                if !special || q != 2 {
                                    continue;
                                }
                                offset.x = 0.0;
                            }
                            5 => {
                                if !special || !p.late_epoch || q != 3 {
                                    continue;
                                }
                                offset.x = EE_GFX_MARKER_GOAL_SPACING;
                            }
                            _ => {
                                if !special || !p.late_epoch || q != 4 {
                                    continue;
                                }
                                offset.x = EE_GFX_MARKER_GOAL_SPACING + EE_GFX_MARKER_GOAL_SPACING;
                            }
                        }
                        let index = counts[t as usize];
                        self.add(h, player, index, t);
                        let mut from_point = match h.ground(&from) {
                            Some(ground) if g > 0 => ground,
                            _ => from,
                        };
                        if g > 0 {
                            from_point = add(from_point, offset);
                        }
                        if from_point.z < 0.0 {
                            from_point.z = 0.0;
                        }
                        let mut to_point = h.ground(&to).unwrap_or(to);
                        to_point = add(to_point, offset);
                        if to_point.z < 0.0 {
                            to_point.z = 0.0;
                        }
                        let m = &mut self.list(player, t)[index as usize];
                        m.from = from_point;
                        m.to = to_point;
                        m.changed = true;
                        counts[t as usize] += 1;
                    }
                    previous = g as i64;
                }
            }
        }
        let players = h.player_count();
        for other in 0..players {
            for t in 0..EE_GFX_MARKER_GOAL_TYPES {
                if other != player {
                    self.trim(h, other, 0, t);
                } else {
                    self.trim(h, player, counts[t as usize], t);
                }
            }
        }
    }

    /// `Update`: the current player's markers rebuilt, then every changed marker's effect moved (a line to its ends,
    /// an icon to its `to`).
    pub fn update<H: GoalHost>(&mut self, h: &mut H) {
        let player = h.current_player();
        self.rebuild(h, player);
        for kinds in self.goals.values_mut() {
            for list in kinds.values_mut() {
                for m in list.iter_mut() {
                    if !m.changed {
                        continue;
                    }
                    match m.kind {
                        0 => h.move_line(m.effect, &m.from, &m.to),
                        1..=6 => h.move_icon(m.effect, &m.to),
                        _ => {}
                    }
                    m.changed = false;
                }
            }
        }
    }

    /// The destructor's walk: every marker's effect destroyed, in the maps' order.
    pub fn clear<H: GoalHost>(&mut self, h: &mut H) {
        for kinds in self.goals.values() {
            for list in kinds.values() {
                for m in list {
                    h.destroy(m.effect);
                }
            }
        }
        self.goals.clear();
    }
}

/// `GE3DPoint::operator+=`.
fn add(a: Ge3DPoint, b: Ge3DPoint) -> Ge3DPoint {
    Ge3DPoint { x: a.x + b.x, y: a.y + b.y, z: a.z + b.z }
}

/// `EEGraphicalGoalManager`'s constructor: no markers.
#[no_mangle]
pub extern "C" fn ee_gfx_goals_new() -> *mut EeGfxGoalMarkers {
    Box::into_raw(Box::new(EeGfxGoalMarkers::new()))
}

/// Its destructor: every marker's effect destroyed, the markers freed.
///
/// # Safety
/// `markers` came from [`ee_gfx_goals_new`] (or is null) and is not used again; `host` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_goals_free(markers: *mut EeGfxGoalMarkers, host: *const EeGfxGoalHost) {
    if markers.is_null() {
        return;
    }
    // SAFETY: the function's contract
    let mut m = unsafe { Box::from_raw(markers) };
    // SAFETY: the function's contract
    m.clear(&mut unsafe { &*host });
}

/// `Update` (every tick, `EEEffectManager::Update`).
///
/// # Safety
/// `markers` came from [`ee_gfx_goals_new`]; `host` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_goals_update(markers: *mut EeGfxGoalMarkers, host: *const EeGfxGoalHost) {
    // SAFETY: the function's contract
    unsafe { (*markers).update(&mut &*host) }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(Default)]
    struct World {
        player: EeGfxGoalPlayer,
        units: Vec<(EeGfxGoalUnit, Vec<Vec<EeGfxGoalStep>>)>,
        players: u32,
        next: u32,
        log: Vec<String>,
    }

    fn pt(x: f32, y: f32, z: f32) -> Ge3DPoint {
        Ge3DPoint { x, y, z }
    }

    impl GoalHost for World {
        fn current_player(&mut self) -> u32 {
            1
        }
        fn player_count(&mut self) -> u32 {
            self.players
        }
        fn player(&mut self, _player: u32) -> EeGfxGoalPlayer {
            self.player
        }
        fn unit(&mut self, unit: u32) -> EeGfxGoalUnit {
            self.units[unit as usize].0
        }
        fn goal_count(&mut self, unit: u32, queue: u32) -> u32 {
            self.units[unit as usize].1.get(queue as usize).map_or(0, |q| q.len() as u32)
        }
        fn goal(&mut self, unit: u32, queue: u32, goal: u32) -> EeGfxGoalStep {
            self.units[unit as usize].1[queue as usize][goal as usize]
        }
        fn goal_location(&mut self, unit: u32, queue: u32, goal: u32) -> Option<Ge3DPoint> {
            Some(self.units[unit as usize].1[queue as usize][goal as usize].to)
        }
        fn unit_location(&mut self, _unit: u32, on_tile: bool) -> Ge3DPoint {
            if on_tile {
                pt(1.0, 1.0, 0.5)
            } else {
                pt(2.0, 2.0, 0.5)
            }
        }
        fn target(&mut self, target: *mut c_void, to: &mut Ge3DPoint) -> *mut c_void {
            to.x += 100.0;
            target
        }
        fn ground(&mut self, point: &Ge3DPoint) -> Option<Ge3DPoint> {
            (point.x >= 0.0).then_some(pt(point.x, point.y, -1.0))
        }
        fn create(&mut self, kind: u32, player: u32) -> u32 {
            self.next += 1;
            self.log.push(format!("create {kind} {player} -> {}", self.next));
            self.next
        }
        fn destroy(&mut self, effect: u32) {
            self.log.push(format!("destroy {effect}"));
        }
        fn move_line(&mut self, effect: u32, from: &Ge3DPoint, to: &Ge3DPoint) {
            self.log.push(format!("line {effect} {:?} {:?}", (from.x, from.y, from.z), (to.x, to.y, to.z)));
        }
        fn move_icon(&mut self, effect: u32, to: &Ge3DPoint) {
            self.log.push(format!("icon {effect} {:?}", (to.x, to.y, to.z)));
        }
    }

    fn goal(kind: u32, to: Ge3DPoint) -> EeGfxGoalStep {
        EeGfxGoalStep { kind, to, ..Default::default() }
    }

    fn soldier(queues: Vec<Vec<EeGfxGoalStep>>) -> (EeGfxGoalUnit, Vec<Vec<EeGfxGoalStep>>) {
        let n = queues.len() as u32;
        (EeGfxGoalUnit { category: 7, on_tile: true, queue_count: n, ..Default::default() }, queues)
    }

    #[test]
    fn a_soldiers_queue_gives_lines_and_waypoints_in_shift_mode_only() {
        let mut w = World { players: 3, ..Default::default() };
        w.units.push(soldier(vec![vec![
            goal(EE_GFX_MARKER_GOAL_LOCATION, pt(5.0, 6.0, 0.0)),
            goal(EE_GFX_MARKER_GOAL_LOCATION, pt(7.0, 8.0, 0.0)),
        ]]));
        let mut m = EeGfxGoalMarkers::new();
        m.update(&mut w);
        assert!(w.log.is_empty());
        w.player.shift_mode = true;
        w.player.unit_count = 1;
        m.update(&mut w);
        assert_eq!(
            w.log,
            [
                "create 0 1 -> 1",
                "create 1 1 -> 2",
                "create 0 1 -> 3",
                "create 1 1 -> 4",
                // the first line starts at the unit, unraised; the second at the first goal on the ground, raised
                "line 1 (1.0, 1.0, 0.5) (5.0, 6.0, 0.0)",
                "line 3 (5.0, 6.0, 0.0) (7.0, 8.0, 0.0)",
                "icon 2 (5.0, 6.0, 0.0)",
                "icon 4 (7.0, 8.0, 0.0)",
            ]
        );
        assert_eq!(m.goals[&1][&0][1].from, pt(5.0, 6.0, 0.0));
        // the next tick with one goal fewer: the leftovers trimmed by type (every marker is moved each tick)
        w.log.clear();
        w.units[0].1[0].pop();
        m.update(&mut w);
        assert_eq!(
            w.log,
            ["destroy 3", "destroy 4", "line 1 (1.0, 1.0, 0.5) (5.0, 6.0, 0.0)", "icon 2 (5.0, 6.0, 0.0)"]
        );
        // the other players' lists exist and are empty
        assert_eq!(m.goals.keys().copied().collect::<Vec<_>>(), [0, 1, 2]);
        w.log.clear();
        m.clear(&mut w);
        assert_eq!(w.log, ["destroy 1", "destroy 2"]);
    }

    #[test]
    fn an_airports_queues_spread_its_flags_and_targets_carry_over() {
        let mut w = World { players: 2, ..Default::default() };
        w.player = EeGfxGoalPlayer { late_epoch: true, shift_mode: false, unit_count: 1 };
        let mut attack = goal(EE_GFX_MARKER_GOAL_ATTACK_UNIT, pt(1.0, 1.0, 0.0));
        attack.has_target = true;
        attack.target = 9 as *mut c_void;
        // a garrison goal with no container found keeps the attack's target
        let garrison = goal(EE_GFX_MARKER_GOAL_LOAD_GARRISON, pt(2.0, 2.0, 0.0));
        let queues = vec![vec![], vec![attack, garrison], vec![], vec![], vec![goal(5, pt(3.0, 3.0, 0.0))]];
        w.units.push((
            EeGfxGoalUnit { unit_type: EE_GFX_MARKER_TYPE_AIRPORT, held: true, queue_count: 5, ..Default::default() },
            queues,
        ));
        let mut m = EeGfxGoalMarkers::new();
        m.update(&mut w);
        // queue 1: a line (offset (1 - 2) * 0.4) and the type 3 flag (offset -0.4) for both goals; queue 4: a line
        // (offset 0.8) and the type 6 flag (0.8)
        let lines = &m.goals[&1][&0];
        assert_eq!(lines.len(), 3);
        assert_eq!(lines[0].to, pt(101.0 - 0.4, 1.0, 0.0));
        assert_eq!(lines[0].from, pt(2.0, 2.0, 0.5));
        // the second line starts at the first goal's own location (not its target's), on the ground and raised
        assert_eq!(lines[1].from, pt(1.0 - 0.4, 1.0, 0.0));
        assert_eq!(lines[1].to, pt(102.0 - 0.4, 2.0, 0.0));
        assert_eq!(lines[2].to, pt(3.0 + 0.8, 3.0, 0.0));
        assert_eq!(m.goals[&1][&3].len(), 2);
        assert_eq!(m.goals[&1][&3][0].to, pt(101.0 - 0.4, 1.0, 0.0));
        assert_eq!(m.goals[&1][&6].len(), 1);
        assert_eq!(m.goals[&1][&6][0].to, pt(3.0 + 0.8, 3.0, 0.0));
    }

    #[test]
    fn auto_waypoints_and_unplaced_units_give_nothing() {
        let mut w = World { players: 2, ..Default::default() };
        w.player = EeGfxGoalPlayer { late_epoch: false, shift_mode: true, unit_count: 2 };
        let mut auto = goal(EE_GFX_MARKER_GOAL_LOCATION, pt(1.0, 1.0, 0.0));
        auto.auto_waypoint = true;
        w.units.push(soldier(vec![vec![auto]]));
        let mut away = soldier(vec![vec![goal(EE_GFX_MARKER_GOAL_LOCATION, pt(1.0, 1.0, 0.0))]]);
        away.0.on_tile = false;
        w.units.push(away);
        let mut m = EeGfxGoalMarkers::new();
        m.update(&mut w);
        assert!(w.log.is_empty());
    }
}
