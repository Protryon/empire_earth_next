//! The fixture's orders (TEST ONLY). "<seq> order <form> <kind>" gives an order to a group of the local player's
//! units through the real input path, then judges every unit of the group by the original's rule once the physics
//! has executed the order's message. The group commands' bodies (rust/crates/ee_sim/src/units group.rs) are reached
//! by no capture game (EE-SIM's human idles), so this is the only run of their world calls in a played game.
//!
//! Kinds:
//!   0 gather         up to four citizens; a right click on a resource of the world player in view that a citizen of
//!                    the group can gather -> EEMGatherResource. With none in view, the group first walks (EEMPathTo)
//!                    to the nearest one it could gather until the player sees it, the camera is centred on it and the
//!                    view scanned again (note "scouting" meanwhile; "scout-unseen" / "scout-no-pick" when that fails);
//!   1 repair         up to four citizens; one of the player's built land buildings in view is damaged first (its hit
//!                    points set to half its maximum under the world's write lock: there is no game message for it); a
//!                    right click on it -> EEMRepairUnit;
//!   2 repair-queued  as 1 with Shift held around the click. With none of the player's buildings in view, the camera
//!                    is first centred on one of them (the town centre, else the capitol, else the first built one;
//!                    note "home" meanwhile, "homed" through the verdict; "home-no-pick" when the new view shows none);
//!   3 patrol         up to six mobile units (other than citizens when the player has some); the Patrol button
//!                    clicked -> EEMPatrol(-1), the nearest town's patrol route;
//!   4 attack-ground  up to four units whose class can attack the ground; the Attack Ground button, then a left click
//!                    on a seen point beyond the first unit's minimum range -> EEMAttackGround;
//!   5 move           up to six mobile units; a right click on open ground in view with no unit under it -> EEMPathTo.
//! The group is selected with EEMSelectTheseUnits (as the unit icon bar sends it), queued as a UI action; the order
//! goes through the platform's events, the game form's mode, EEAMouseClick or the status button, as a player's input
//! does. The target and the click points are found with the game's own picker over a grid of the game view, under the
//! world's read lock.
//!
//! The verdict: the physics' message hook (EEPhysics::DrainInput, the world's write lock held) sees the order's
//! message before and after it executes. Before: the sender's active group and each unit's state the rule reads.
//! After, each unit by the original's rule:
//!   gather   (target T with resources left) a commandable, living citizen: a Gather goal on T when the continents let
//!            it reach T's tile (its current goal; the last of its queue when queued and it had a goal), else a
//!            Location goal at T's tile; other units: skip;
//!   repair   (T in the world) a commandable, living citizen: T first in its build queue (queued: T in its queue);
//!   patrol   (the first unit's nearest town has a patrol route with a point P) a commandable, living mobile unit that
//!            can patrol: a Patrol goal on P's route; one that cannot: a Location goal at P; a transport in a
//!            container: skip; an immobile unit: its goal unchanged; no route: every goal unchanged;
//!   attack   a commandable, living unit that can attack the ground: an Attack Ground goal at the message's point;
//!   move     a commandable, living unit with a movement domain: a Location goal; immobile ones: skip.
//! A click that gives no message within 10 s is retried once (note "retry", then "retried"); "no-message" when the
//! second click gives none either ("retry-no-pick" when the new view has no target).
//! Every poll writes "order seq <s> kind <k> phase <p> type <t> target <id> point <x> <y> queued <q> units <n> ok <n>
//! bad <n> skip <n> note <word>" (phase 1 choosing, 2 the input sent, 3 judged, 9 refused) and per unit
//! "orderunit <seq> <id> <ok|bad|skip> <expected> <found>".

use std::collections::BTreeMap;
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::sync::Mutex;

use ee_sim::world::{PlayerView, UnitView, WorldRead, WorldView};

use super::boxsel;
use super::host::{self, Call, EeFixtureOp as Op};

const EE_FIXTURE_GATHER: i32 = 0;
const EE_FIXTURE_REPAIR: i32 = 1;
const EE_FIXTURE_REPAIR_QUEUED: i32 = 2;
const EE_FIXTURE_PATROL: i32 = 3;
const EE_FIXTURE_ATTACK_GROUND: i32 = 4;
const EE_FIXTURE_MOVE: i32 = 5;
const EE_FIXTURE_KINDS: u64 = 6;

/// The messages each kind waits for (ee/EEMessageID.h: kMessageGatherResource, kMessageRepairUnit x 2,
/// kMessagePatrol, kMessageAttackGround, kMessagePathTo).
const EE_FIXTURE_TYPES: [u64; 6] = [0x51, 0x57, 0x57, 0x73, 0x9f, 0x4d];
/// dbbuttons.csv row 18 "patrol" (EEUIFMGStatusButton::Command 0x12), row 6 "attack_ground"
const EE_FIXTURE_BUTTON_PATROL: i64 = 18;
const EE_FIXTURE_BUTTON_ATTACK_GROUND: i64 = 6;
/// kMainGameSelectUnits (ee/EEUIManager.h): the game view's control
const EE_FIXTURE_MAIN_GAME_SELECT_UNITS: i64 = 28;

// ee/EEUnitClassIDs.h
const EE_FIXTURE_CATEGORY_AIRPLANE: u64 = 3;
const EE_FIXTURE_CATEGORY_LAND_CITIZEN: u64 = 6;
const EE_FIXTURE_CATEGORY_NAVAL_CITIZEN: u64 = 0x16;
const EE_FIXTURE_CATEGORY_PLANET_SPACESHIP: u64 = 0x22;
const EE_FIXTURE_FAMILY_ANIMAL: u64 = 0x11;
const EE_FIXTURE_TYPE_TOWN_CENTER: u64 = 0x23;
const EE_FIXTURE_TYPE_CAPITOL: u64 = 0x29;
const EE_FIXTURE_TYPE_AIRPORT: u64 = 0x42;
const EE_FIXTURE_OFF_MAP: i64 = -10000;

/// A unit's state before the message, as the rule reads it.
#[derive(Clone, Debug, Default)]
struct OrderUnit {
    id: i64,
    /// the current goal (compared by address: "unchanged")
    goal: u64,
    had_goal: bool,
    /// its current goal gathered the target already
    gathering_target: bool,
    verdict: String,
    expected: String,
    found: String,
}

#[derive(Clone, Debug)]
struct OrderState {
    seq: u64,
    kind: i32,
    phase: i32,
    note: String,
    kind_type: u64,
    armed: bool,
    before: bool,
    target: i64,
    x: f32,
    y: f32,
    queued: i32,
    route_point: i64,
    route: u64,
    units: Vec<OrderUnit>,
    others: String,
}

impl OrderState {
    const fn new() -> OrderState {
        OrderState {
            seq: 0,
            kind: -1,
            phase: 0,
            note: String::new(),
            kind_type: 0,
            armed: false,
            before: false,
            target: -1,
            x: 0.0,
            y: 0.0,
            queued: 0,
            route_point: -1,
            route: 0,
            units: Vec::new(),
            others: String::new(),
        }
    }
    fn note(&self) -> &str {
        if self.note.is_empty() {
            "-"
        } else {
            &self.note
        }
    }
}

static ORDER: Mutex<OrderState> = Mutex::new(OrderState::new());
static BUSY: AtomicBool = AtomicBool::new(false);
static MARKS: AtomicU64 = AtomicU64::new(0);

fn order() -> std::sync::MutexGuard<'static, OrderState> {
    ORDER.lock().unwrap_or_else(|e| e.into_inner())
}

fn set_note(note: &str) {
    order().note = note.to_string();
}

// ---- reads (the world's lock held by the caller)

fn unit_by_id(t: &WorldRead, id: i64) -> Option<UnitView<'_>> {
    let h = Call::new(Op::UnitById).i(&[id]).go() as u64;
    // SAFETY: the world's lookup names a live unit (the caller holds the world's lock)
    unsafe { UnitView::of(t, h) }
}

/// `gWorld.GetUnit(id)` when it is its own complex unit.
fn own_complex(t: &WorldRead, id: i64) -> Option<UnitView<'_>> {
    unit_by_id(t, id).filter(|u| u.complex_unit_handle() == u.handle())
}

fn category(u: &UnitView) -> u64 {
    u.class().map_or(u64::MAX, |c| c.category())
}

fn unit_type(u: &UnitView) -> u64 {
    u.class().map_or(u64::MAX, |c| c.unit_type())
}

fn is_citizen(u: &UnitView) -> bool {
    matches!(category(u), EE_FIXTURE_CATEGORY_LAND_CITIZEN | EE_FIXTURE_CATEGORY_NAVAL_CITIZEN)
}

fn is_living(u: &UnitView) -> bool {
    u.valid() && u.hit_points() > 0
}

fn is_transport_in_container(u: &UnitView) -> bool {
    matches!(category(u), EE_FIXTURE_CATEGORY_AIRPLANE | EE_FIXTURE_CATEGORY_PLANET_SPACESHIP)
        && u.container_handle() != 0
}

fn is_mobile(u: &UnitView) -> bool {
    let velocity: f32 = u.stats().map_or(0.0, |s| ee_sim::world_field!(s.fields(), mMaxVelocity));
    velocity != 0.0 || unit_type(u) == EE_FIXTURE_TYPE_AIRPORT
}

fn can_attack_ground(u: &UnitView) -> bool {
    u.class().is_some_and(|c| ee_sim::world_field!(c.fields(), mCanAttackGround))
}

fn max_hit_points(u: &UnitView) -> i64 {
    u.stats().map_or(0, |s| s.max_hit_points())
}

/// EEActiveGroup::IsInFormationSlot for some slot 1..5
fn in_some_slot(u: &UnitView) -> bool {
    u.class().is_some_and(|c| {
        let domains: u8 = ee_sim::world_field!(c.fields(), mDomains);
        domains & 0xf != 0 || c.land_building()
    })
}

fn id_of(t: &WorldRead, handle: u64) -> i64 {
    // SAFETY: a handle the game answered with (a goal's target, a queue's unit)
    unsafe { UnitView::of(t, handle) }.map_or(-1, |u| i64::from(u.id()))
}

fn point(x: f32, y: f32) -> String {
    format!("{x:.3},{y:.3}")
}

/// A goal (`EeFixtureOp::Goal`).
struct Goal {
    kind: i64,
    class: i64,
    target_id: i64,
    target: u64,
    group: u64,
    x: f32,
    y: f32,
}

fn goal(handle: u64) -> Option<Goal> {
    if handle == 0 {
        return None;
    }
    let a = Call::new(Op::Goal).h(handle).run();
    Some(Goal {
        kind: a.result,
        class: a.i(0),
        target_id: a.i(1),
        group: a.i(2) as u64,
        target: a.call.h2,
        x: a.f(0),
        y: a.f(1),
    })
}

/// A goal as "<kind>:<its target>" (none: "none").
fn goal_text(handle: u64) -> String {
    let Some(g) = goal(handle) else { return "none".into() };
    match g.class {
        0 => format!("gather:{}", g.target_id),
        1 => format!("location:{}", point(g.x, g.y)),
        2 => format!("attack:{}", g.target_id),
        3 => format!("ground:{}", point(g.x, g.y)),
        4 => format!("patrol:{}", g.target_id),
        5 => "build".into(),
        6 => "explore".into(),
        _ => format!("{}:-", g.kind),
    }
}

fn build_queue(u: &UnitView) -> Vec<u64> {
    Call::new(Op::BuildQueue).h(u.handle()).run().sink.ids.iter().map(|&h| h as u64).collect()
}

fn queue_text(t: &WorldRead, u: &UnitView) -> String {
    let queue = build_queue(u);
    if queue.is_empty() {
        return "queue:-".into();
    }
    let ids: Vec<String> = queue.iter().take(4).map(|&h| id_of(t, h).to_string()).collect();
    format!("queue:{}", ids.join(","))
}

// ---- the verdicts (the physics thread, the world's write lock held)

fn judge(row: &mut OrderUnit, ok: bool, expected: String, found: String) {
    row.verdict = if ok { "ok" } else { "bad" }.into();
    row.expected = expected;
    row.found = found;
}

fn skip(row: &mut OrderUnit, why: &str, found: String) {
    row.verdict = "skip".into();
    row.expected = why.into();
    row.found = found;
}

fn judge_unchanged(row: &mut OrderUnit, u: &UnitView) {
    let ok = u.goal_handle() == row.goal;
    let (expected, found) = (format!("unchanged:{}", goal_text(row.goal)), goal_text(u.goal_handle()));
    judge(row, ok, expected, found);
}

fn judge_gather(t: &WorldRead, s: &mut OrderState) {
    let target = unit_by_id(t, s.target);
    let (no_resource, queued_order) = (s.note == "no-resource", s.queued != 0);
    let target_id = s.target;
    for row in &mut s.units {
        let Some(u) = own_complex(t, row.id) else {
            skip(row, "gone", "-".into());
            continue;
        };
        let found = goal_text(u.goal_handle());
        let Some(target) = target.filter(|_| !no_resource) else {
            judge_unchanged(row, &u);
            continue;
        };
        if !is_living(&u) || !is_citizen(&u) || !u.commandable() {
            skip(row, "not-a-commandable-living-citizen", found);
            continue;
        }
        let (x, y) = target.tile_xy();
        let queued = queued_order && row.had_goal && !row.gathering_target;
        let handle = if queued { Call::new(Op::LastQueued).h(u.handle()).go() as u64 } else { u.goal_handle() };
        let got = goal_text(handle);
        let g = goal(handle);
        if Call::new(Op::CanGetTo).h(u.handle()).i(&[x, y]).go() != 0 {
            let ok = g.is_some_and(|g| g.class == 0 && g.target == target.handle());
            judge(row, ok, format!("gather:{target_id}"), got);
        } else {
            let ok = g.is_some_and(|g| g.class == 1 && g.x == x as f32 && g.y == y as f32);
            judge(row, ok, format!("location:{}", point(x as f32, y as f32)), got);
        }
    }
}

fn judge_repair(t: &WorldRead, s: &mut OrderState) {
    let target = unit_by_id(t, s.target);
    let (queued, target_id) = (s.queued != 0, s.target);
    for row in &mut s.units {
        let Some(u) = own_complex(t, row.id) else {
            skip(row, "gone", "-".into());
            continue;
        };
        let Some(target) = target else {
            judge_unchanged(row, &u);
            continue;
        };
        let found = queue_text(t, &u);
        if !is_living(&u) || !is_citizen(&u) || !u.commandable() {
            skip(row, "not-a-commandable-living-citizen", found);
            continue;
        }
        let queue = build_queue(&u);
        if queued {
            let ok = queue.contains(&target.handle());
            judge(row, ok, format!("queue-has:{target_id}"), found);
        } else {
            let ok = queue.first() == Some(&target.handle());
            judge(row, ok, format!("queue-first:{target_id}"), found);
        }
    }
}

fn judge_patrol(t: &WorldRead, s: &mut OrderState) {
    let point_unit = if s.route_point >= 0 { own_complex(t, s.route_point) } else { None };
    let (route, route_point) = (s.route, s.route_point);
    for row in &mut s.units {
        let Some(u) = own_complex(t, row.id) else {
            skip(row, "gone", "-".into());
            continue;
        };
        let found = goal_text(u.goal_handle());
        let Some(p) = point_unit.filter(|_| unit_type(&u) != boxsel::EE_FIXTURE_TYPE_PATH_POINT) else {
            judge_unchanged(row, &u);
            continue;
        };
        if !is_living(&u) || !in_some_slot(&u) {
            skip(row, "not-taken", found);
            continue;
        }
        if !u.commandable() || !is_mobile(&u) {
            judge_unchanged(row, &u);
            continue;
        }
        if is_transport_in_container(&u) {
            skip(row, "transport-in-container", found);
            continue;
        }
        let g = goal(u.goal_handle());
        let can_patrol = u.class().is_some_and(|c| ee_sim::world_field!(c.fields(), mCanPatrol));
        let detached = ee_sim::world_field!(u.complex(), mIsDetachedFromAI);
        if !can_patrol && (!detached || Call::new(Op::PatrolAction).h(u.handle()).go() == 0) {
            let [x, y, _] = p.location();
            let ok = g.is_some_and(|g| g.class == 1 && g.x == x && g.y == y);
            judge(row, ok, format!("location:{}", point(x, y)), found);
        } else {
            let ok = g.is_some_and(|g| g.class == 4 && g.group == route);
            judge(row, ok, format!("patrol-route-of:{route_point}"), found);
        }
    }
}

fn judge_attack_ground(t: &WorldRead, s: &mut OrderState) {
    let (x, y) = (s.x, s.y);
    for row in &mut s.units {
        let Some(u) = own_complex(t, row.id) else {
            skip(row, "gone", "-".into());
            continue;
        };
        let found = goal_text(u.goal_handle());
        if !is_living(&u) || !in_some_slot(&u) {
            skip(row, "not-taken", found);
            continue;
        }
        if !can_attack_ground(&u) || !u.commandable() {
            judge_unchanged(row, &u);
            continue;
        }
        if is_transport_in_container(&u) {
            skip(row, "transport-in-container", found);
            continue;
        }
        let ok = goal(u.goal_handle()).is_some_and(|g| g.class == 3 && g.x == x && g.y == y);
        judge(row, ok, format!("ground:{}", point(x, y)), found);
    }
}

fn judge_move(t: &WorldRead, s: &mut OrderState) {
    for row in &mut s.units {
        let Some(u) = own_complex(t, row.id) else {
            skip(row, "gone", "-".into());
            continue;
        };
        let found = goal_text(u.goal_handle());
        if !is_living(&u) || !u.commandable() || !is_mobile(&u) || is_transport_in_container(&u) {
            skip(row, "not-a-commandable-living-mobile-unit", found);
            continue;
        }
        let ok = goal(u.goal_handle()).is_some_and(|g| g.class == 1);
        judge(row, ok, "location".into(), found);
    }
}

/// The physics thread (DrainInput), under the world's write lock: `phase` 0 before the message executes, 1 after;
/// `local`: the message came from the local player.
pub fn message(msg: u64, kind_type: u64, local: bool, phase: i32) {
    let mut s = order();
    if !s.armed || !local {
        return;
    }
    if kind_type != s.kind_type {
        if phase == 0 && s.others.len() < 200 {
            s.others.push_str(&format!(" {kind_type}"));
        }
        return;
    }
    // SAFETY: the world's write lock is held by the physics thread
    let t = unsafe { WorldRead::begin() };
    let value = |k: i64, as_type: i64| Call::new(Op::MessageValue).h(msg).i(&[k, as_type]).run();
    if phase == 0 {
        s.before = true;
        s.units.clear();
        match s.kind {
            EE_FIXTURE_GATHER | EE_FIXTURE_REPAIR | EE_FIXTURE_REPAIR_QUEUED => {
                s.target = value(1, 0).result; // the target, queued
                s.queued = i32::from(value(2, 1).result != 0);
            }
            EE_FIXTURE_PATROL => s.target = value(1, 0).result,
            EE_FIXTURE_ATTACK_GROUND => {
                s.x = value(1, 2).f(0); // x, y
                s.y = value(2, 2).f(0);
            }
            _ => {}
        }
        let target = if s.target >= 0 { unit_by_id(&t, s.target) } else { None };
        if s.kind == EE_FIXTURE_GATHER && target.is_some_and(|u| u.resource_amount() <= 0) {
            s.note = "no-resource".into();
        }
        let group = boxsel::active_group();
        for &h in group.iter().flatten() {
            // SAFETY: the group's units are live (the write lock)
            let Some(u) = (unsafe { UnitView::of(&t, h) }) else { continue };
            let g = u.goal_handle();
            let gathering = target.is_some_and(|tu| goal(g).is_some_and(|g| g.class == 0 && g.target == tu.handle()));
            let row = OrderUnit {
                id: i64::from(u.id()),
                goal: g,
                had_goal: g != 0,
                gathering_target: gathering,
                ..Default::default()
            };
            if !s.units.iter().any(|r| r.id == row.id) {
                s.units.push(row);
            }
        }
        let first = group.as_ref().and_then(|g| g.first().copied()).unwrap_or(0);
        if s.kind == EE_FIXTURE_PATROL && first != 0 && s.target == -1 {
            // EEMPatrol(-1): the first unit's nearest town's route
            let a = Call::new(Op::NearestRoute).h(first).run();
            if a.result >= 0 {
                s.route_point = a.result;
                s.route = a.call.h2;
            }
            if s.route_point < 0 {
                s.note = "no-route".into();
            }
        }
        return;
    }
    if !s.before {
        return;
    }
    match s.kind {
        EE_FIXTURE_GATHER => judge_gather(&t, &mut s),
        EE_FIXTURE_REPAIR | EE_FIXTURE_REPAIR_QUEUED => judge_repair(&t, &mut s),
        EE_FIXTURE_PATROL => judge_patrol(&t, &mut s),
        EE_FIXTURE_ATTACK_GROUND => judge_attack_ground(&t, &mut s),
        _ => judge_move(&t, &mut s),
    }
    s.route = 0; // never used after this
    s.armed = false;
    s.phase = 3;
}

// ---- the input (the order's own thread; never under the UI lock)

fn pointer(x: f32, y: f32) {
    host::mouse_set(x, y);
    host::ui_update();
}

/// A press and release of a mouse button (1 left, 3 right) at (x, y), Shift held around it when asked.
fn click(x: f32, y: f32, button: i64, shift: bool) {
    const LEFT_SHIFT: i64 = 0x2a; // DIK_LSHIFT
    pointer(x, y);
    host::sleep_ms(300);
    if shift {
        host::input(host::EE_FIXTURE_KEY_DOWN, LEFT_SHIFT);
        host::sleep_ms(150);
    }
    host::input(host::EE_FIXTURE_MOUSE_DOWN, button);
    host::sleep_ms(200);
    host::input(host::EE_FIXTURE_MOUSE_UP, button);
    host::sleep_ms(300);
    if shift {
        host::input(host::EE_FIXTURE_KEY_UP, LEFT_SHIFT);
    }
}

fn diag(text: &str) {
    host::stderr(&format!("TEST UI order diag {text}\n"));
}

/// The camera's focus and target (tiles).
fn camera_text() -> String {
    let c = Call::new(Op::Camera).run();
    format!("camera {:.2},{:.2} target {:.2},{:.2}", c.f(0), c.f(1), c.f(3), c.f(4))
}

fn refuse(note: &str) {
    let mut s = order();
    s.phase = 9;
    s.note = note.into();
    s.armed = false;
}

type Point = (f32, f32);

/// Every grid point (12 pixels apart) of the game view where the picker finds each unit, in scan order (`prefer`:
/// PickUnit's resource preference, as the click will pass it); the points with no unit in the second list.
fn scan(prefer: i64) -> (BTreeMap<i64, Vec<Point>>, Vec<Point>) {
    let view = boxsel::view_rect();
    let mut points: BTreeMap<i64, Vec<Point>> = BTreeMap::new();
    let mut empty = Vec::new();
    let step = 12.0f32;
    let mut y = view.top + 24.0;
    while y < view.bottom - 24.0 {
        let mut x = view.left + 24.0;
        while x < view.right - 24.0 {
            match Call::new(Op::Pick).i(&[prefer]).f(&[x, y]).go() {
                -1 => empty.push((x, y)),
                id => points.entry(id).or_default().push((x, y)),
            }
            x += step;
        }
        y += step;
    }
    (points, empty)
}

/// A unit's grid points ordered for the click: nearest their centroid first (the first point a scan meets is the
/// edge of the unit's hit area, where a pixel of motion changes what a click there picks).
pub fn click_order(points: &[Point]) -> Vec<Point> {
    let (mut cx, mut cy) = (0.0f32, 0.0f32);
    for p in points {
        cx += p.0;
        cy += p.1;
    }
    cx /= points.len() as f32;
    cy /= points.len() as f32;
    let mut order: Vec<(f32, usize)> = points
        .iter()
        .enumerate()
        .map(|(i, p)| {
            let (dx, dy) = (p.0 - cx, p.1 - cy);
            (dx * dx + dy * dy, i)
        })
        .collect();
    order.sort_by(|a, b| a.0.partial_cmp(&b.0).unwrap_or(std::cmp::Ordering::Equal));
    order.iter().map(|&(_, i)| points[i]).collect()
}

/// The visible status button of the game form for button object `button` -> its centre.
fn find_button(button: i64) -> Option<Point> {
    let a = Call::new(Op::StatusButton).i(&[button]).run();
    (a.result != 0).then(|| (a.f(0), a.f(1)))
}

/// The local player's selected group (the UI's) equals `ids` (as sets).
fn selected(ids: &[i64]) -> bool {
    let group = host::with_read_lock(|| {
        // SAFETY: the world's read lock is held
        let t = unsafe { WorldRead::begin() };
        let mut group = Vec::new();
        if let Some(g) = WorldView::global(&t).and_then(|w| w.current_player()).and_then(|p| p.selected_group()) {
            for i in 0..g.len() {
                let h = g.unit_handle(i);
                if h != 0 {
                    group.push(id_of(&t, h));
                }
            }
        }
        group
    })
    .unwrap_or_default();
    let (mut group, mut wanted) = (group, ids.to_vec());
    group.sort_unstable();
    wanted.sort_unstable();
    group == wanted
}

#[derive(Clone, Debug)]
struct OrderPlan {
    units: Vec<i64>,
    /// the target's point (gather, repair, move) or the ground point (attack)
    click: Point,
    /// the click points to try, best first
    candidates: Vec<Point>,
    target: i64,
    /// repair: the building to damage
    damage: i64,
    refusal: Option<&'static str>,
    /// gather with nothing in view: the nearest resource the group can gather, and its tile
    scout: i64,
    scout_at: (i64, i64),
    /// repair with none of the player's buildings in view: one of them, and its tile
    home: i64,
    home_at: (i64, i64),
}

impl Default for OrderPlan {
    fn default() -> OrderPlan {
        OrderPlan {
            units: Vec::new(),
            click: (0.0, 0.0),
            candidates: Vec::new(),
            target: -1,
            damage: -1,
            refusal: None,
            scout: -1,
            scout_at: (0, 0),
            home: -1,
            home_at: (0, 0),
        }
    }
}

fn player_units<'t>(p: &PlayerView<'t>) -> impl Iterator<Item = UnitView<'t>> {
    std::iter::successors(p.first_unit(), |u| u.next())
}

/// Repair: a building the repair could target were it in view, into `plan.home`: the town centre first, else the
/// capitol, else the first one in the player's list.
fn home_building(t: &WorldRead, player: &PlayerView, plan: &mut OrderPlan) {
    let mut rank = 3;
    for node in player_units(player) {
        let Some(u) = own_complex(t, i64::from(node.id())) else { continue };
        if !u.class().is_some_and(|c| c.land_building())
            || !u.is_built()
            || !is_living(&u)
            || u.tile_xy().0 == EE_FIXTURE_OFF_MAP
            || max_hit_points(&u) < 2
        {
            continue;
        }
        let unit_rank = match unit_type(&u) {
            EE_FIXTURE_TYPE_TOWN_CENTER => 0,
            EE_FIXTURE_TYPE_CAPITOL => 1,
            _ => 2,
        };
        if unit_rank >= rank {
            continue;
        }
        rank = unit_rank;
        plan.home = i64::from(u.id());
        plan.home_at = u.tile_xy();
    }
}

fn can_gather(worker: &UnitView, unit: &UnitView, x: i64, y: i64, a: bool, b: bool) -> bool {
    Call::new(Op::CanGather).h(worker.handle()).h2(unit.handle()).i(&[x, y, i64::from(a), i64::from(b)]).go() != 0
}

fn seen_by(unit: &UnitView, player: &PlayerView) -> bool {
    Call::new(Op::SeenBy).h(unit.handle()).h2(player.handle()).go() != 0
}

/// Gather: the world player's resource nearest `worker` that it could gather were its tile in view and that it can
/// reach, into `plan.scout`.
fn scout_target(world: &WorldView, worker: &UnitView, plan: &mut OrderPlan) {
    let mut best = -1i64;
    let Some(nature) = world.player(0) else { return };
    let (wx, wy) = worker.tile_xy();
    for u in player_units(&nature) {
        let (x, y) = u.tile_xy();
        if !u.valid()
            || x == EE_FIXTURE_OFF_MAP
            || u.resource_amount() <= 0
            || u.class().map_or(0, |c| c.family()) == EE_FIXTURE_FAMILY_ANIMAL
            || !can_gather(worker, &u, x, y, true, false)
            || Call::new(Op::CanGetTo).h(worker.handle()).i(&[x, y]).go() == 0
        {
            continue;
        }
        let (dx, dy) = (x - wx, y - wy);
        let distance = dx * dx + dy * dy;
        if best >= 0 && distance >= best {
            continue;
        }
        best = distance;
        plan.scout = i64::from(u.id());
        plan.scout_at = (x, y);
    }
}

/// Chooses the units, the target and the click point (the world's read lock held).
fn choose(kind: i32, plan: &mut OrderPlan) {
    // SAFETY: the caller holds the world's read lock
    let t = unsafe { WorldRead::begin() };
    let Some(world) = WorldView::global(&t) else { return };
    let Some(player) = world.current_player() else {
        plan.refusal = Some("no-player");
        return;
    };
    let (mut citizens, mut mobile, mut others, mut ground) = (Vec::new(), Vec::new(), Vec::new(), Vec::new());
    for node in player_units(&player) {
        let Some(u) = own_complex(&t, i64::from(node.id())) else { continue };
        if !is_living(&u)
            || !u.is_built()
            || !u.commandable()
            || unit_type(&u) == boxsel::EE_FIXTURE_TYPE_PATH_POINT
            || u.tile_xy().0 == EE_FIXTURE_OFF_MAP
        {
            continue;
        }
        if !is_mobile(&u) {
            continue;
        }
        let id = i64::from(u.id());
        mobile.push(id);
        if is_citizen(&u) {
            citizens.push(id);
        } else {
            others.push(id);
        }
        if can_attack_ground(&u) {
            ground.push(id);
        }
    }
    let (group, limit) = match kind {
        EE_FIXTURE_PATROL | EE_FIXTURE_MOVE => (if others.is_empty() { &mobile } else { &others }, 6),
        EE_FIXTURE_ATTACK_GROUND => (&ground, 4),
        _ => (&citizens, 4),
    };
    plan.units.extend(group.iter().take(limit));
    if plan.units.is_empty() {
        plan.refusal = Some("no-unit");
        return;
    }
    if kind == EE_FIXTURE_PATROL {
        return;
    }
    let (points, empty) =
        scan(if matches!(kind, EE_FIXTURE_GATHER | EE_FIXTURE_REPAIR | EE_FIXTURE_REPAIR_QUEUED) { 1 } else { 0 });
    if kind == EE_FIXTURE_GATHER {
        let Some(worker) = own_complex(&t, plan.units[0]) else { return };
        for (&id, at) in &points {
            let Some(u) = unit_by_id(&t, id) else { continue };
            let owner = u.player();
            if owner.map(|p| p.handle()) == Some(player.handle())
                || owner.map_or(0, |p| p.id()) != 0
                || u.resource_amount() <= 0
                || u.class().map_or(0, |c| c.family()) == EE_FIXTURE_FAMILY_ANIMAL
                || !seen_by(&u, &player)
                || !{
                    let (x, y) = u.tile_xy();
                    can_gather(&worker, &u, x, y, false, false)
                }
            {
                continue;
            }
            plan.target = id;
            plan.candidates = click_order(at);
            plan.click = plan.candidates[0];
            if u.class().is_some_and(|c| c.tree()) {
                break; // a tree first, else the first resource found
            }
        }
        if plan.target < 0 {
            plan.refusal = Some("no-target");
            scout_target(&world, &worker, plan);
        }
    } else if kind == EE_FIXTURE_REPAIR || kind == EE_FIXTURE_REPAIR_QUEUED {
        for (&id, at) in &points {
            let Some(u) = own_complex(&t, id) else { continue };
            if u.player().map(|p| p.handle()) != Some(player.handle())
                || !u.class().is_some_and(|c| c.land_building())
                || !u.is_built()
                || !is_living(&u)
                || max_hit_points(&u) < 2
            {
                continue;
            }
            plan.target = id;
            plan.candidates = click_order(at);
            plan.click = plan.candidates[0];
            if u.hit_points() < max_hit_points(&u) {
                break; // a damaged one first
            }
        }
        if plan.target < 0 {
            plan.refusal = Some("no-target");
            home_building(&t, &player, plan);
        } else {
            plan.damage = plan.target;
        }
    } else {
        // attack ground, move: a point in view with no unit under it, on a tile the player sees
        let Some(first) = own_complex(&t, plan.units[0]) else { return };
        let min: f32 = first.class().map_or(0.0, |c| ee_sim::world_field!(c.fields(), mMinRange));
        for i in 0..empty.len() {
            let k = i.wrapping_mul(7919) % empty.len(); // spread over the view, not its first row
            let at = empty[k];
            let a = Call::new(Op::PointTile).f(&[at.0, at.1]).run();
            if a.result == 0 {
                continue;
            }
            if kind == EE_FIXTURE_ATTACK_GROUND {
                let d = Call::new(Op::Distance).h(first.handle()).f(&[a.f(0), a.f(1), a.f(2)]).run().f(0);
                if d < min * min {
                    continue;
                }
            }
            if plan.target < 0 {
                plan.click = at;
            }
            plan.target = 0;
            plan.candidates.push(at);
            if plan.candidates.len() >= 16 {
                break;
            }
        }
        if plan.target < 0 {
            plan.refusal = Some("no-point");
        }
        plan.target = -1;
    }
}

fn choose_locked(kind: i32) -> OrderPlan {
    let mut plan = OrderPlan::default();
    host::with_read_lock(|| choose(kind, &mut plan));
    plan
}

/// The camera's focus and its move's target both at tile (x, y)'s centre (EEACenterCamera's point).
fn camera_at(x: i64, y: i64) -> bool {
    let c = Call::new(Op::Camera).run();
    let (tx, ty) = (x as f32 + 0.5, y as f32 + 0.5);
    (c.f(0) - tx).abs() < 0.05 && (c.f(1) - ty).abs() < 0.05 && (c.f(3) - tx).abs() < 0.05 && (c.f(4) - ty).abs() < 0.05
}

/// The camera centred on tile (x, y), then the plan chosen from the new view: wait (at most 10 s each) for the camera
/// to reach the tile, then for two scans 250 ms apart to agree on the target and its point.
fn centre_and_choose(kind: i32, x: i64, y: i64) -> OrderPlan {
    Call::new(Op::CenterCamera).i(&[x, y]).go();
    let mut arrived = false;
    for _ in 0..200 {
        if arrived {
            break;
        }
        host::sleep_ms(50);
        arrived = host::with_read_lock(|| camera_at(x, y)).unwrap_or(false);
    }
    let mut last = OrderPlan::default();
    let mut scans = 0;
    while scans < 40 {
        host::sleep_ms(250);
        let plan = choose_locked(kind);
        let same = scans > 0 && plan.target >= 0 && plan.target == last.target && plan.click == last.click;
        last = plan;
        if same {
            break;
        }
        scans += 1;
    }
    let camera = host::with_read_lock(camera_text).unwrap_or_default();
    diag(&format!(
        "centred on {x},{y}: arrived {}; {} scans; target {} at {:.0},{:.0} ({} points); {camera}",
        i32::from(arrived),
        scans + 1,
        last.target,
        last.click.0,
        last.click.1,
        last.candidates.len()
    ));
    last
}

/// Gather with no resource in view: the group selected and sent to the nearest resource until the player sees its
/// tile (at most 60 s); then the group halted where it stands, the camera centred on the resource and the plan chosen
/// again from the new view.
fn scout(kind: i32, plan: &mut OrderPlan) {
    set_note("scouting");
    let (target, (x, y)) = (plan.scout, plan.scout_at);
    let worker_id = plan.units[0];
    let units = plan.units.clone();
    Call::new(Op::Post).i(&[0]).ids(&units).go();
    let mut is_selected = false;
    for _ in 0..40 {
        if is_selected {
            break;
        }
        host::sleep_ms(150);
        is_selected = selected(&units);
    }
    if is_selected {
        Call::new(Op::Post).i(&[2]).f(&[x as f32 + 0.5, y as f32 + 0.5]).go();
    }
    let mut seen = false;
    for _ in 0..120 {
        if !is_selected || seen {
            break;
        }
        host::sleep_ms(500);
        seen = host::with_read_lock(|| {
            // SAFETY: the world's read lock is held
            let t = unsafe { WorldRead::begin() };
            let unit = unit_by_id(&t, target);
            let worker = own_complex(&t, worker_id);
            let player = WorldView::global(&t).and_then(|w| w.current_player());
            match (unit, worker, player) {
                (Some(u), Some(w), Some(p)) => {
                    u.resource_amount() > 0 && seen_by(&u, &p) && can_gather(&w, &u, x, y, false, false)
                }
                _ => false,
            }
        })
        .unwrap_or(false);
    }
    let mut next = OrderPlan::default();
    if seen {
        let halted = select_group(&units);
        if halted {
            Call::new(Op::Post).i(&[3]).go();
        }
        let mut idle = false;
        for _ in 0..100 {
            if !halted || idle {
                break;
            }
            host::sleep_ms(100);
            idle = host::with_read_lock(|| {
                // SAFETY: the world's read lock is held
                let t = unsafe { WorldRead::begin() };
                units.iter().all(|&id| own_complex(&t, id).is_none_or(|u| u.goal_handle() == 0))
            })
            .unwrap_or(true);
        }
        let (wx, wy) = host::with_read_lock(|| {
            // SAFETY: the world's read lock is held
            let t = unsafe { WorldRead::begin() };
            own_complex(&t, worker_id).map_or((-1, -1), |w| w.tile_xy())
        })
        .unwrap_or((-1, -1));
        diag(&format!(
            "scout {target} at {x},{y} seen: halted {} idle {}, the worker at {wx},{wy}",
            i32::from(halted),
            i32::from(idle)
        ));
        next = centre_and_choose(kind, x, y);
    } else {
        diag(&format!("scout {target} at {x},{y}: selected {}, not seen", i32::from(is_selected)));
    }
    if !is_selected {
        next.refusal = Some("not-selected");
    } else if !seen {
        next.refusal = Some("scout-unseen"); // not a skip: the driver fails the step with this note
    } else if next.target < 0 {
        next.refusal = Some("scout-no-pick");
    }
    next.scout = -1;
    *plan = next;
    set_note("-");
}

/// Repair with none of the player's buildings in view: the camera centred on the building home_building found and the
/// plan chosen again from the new view.
fn home(kind: i32, plan: &mut OrderPlan) {
    set_note("home");
    let mut next = centre_and_choose(kind, plan.home_at.0, plan.home_at.1);
    if next.target < 0 {
        next.refusal = Some("home-no-pick"); // not a skip: the driver fails the step with this note
    }
    next.home = -1;
    let homed = next.refusal.is_none();
    *plan = next;
    set_note(if homed { "homed" } else { "-" }); // kept through the verdict: the driver logs it
}

/// The group selected (EEMSelectTheseUnits unless it already is) -> whether the UI's selection matches.
fn select_group(units: &[i64]) -> bool {
    if selected(units) {
        return true;
    }
    Call::new(Op::Post).i(&[0]).ids(units).go();
    for _ in 0..40 {
        host::sleep_ms(150);
        if selected(units) {
            return true;
        }
    }
    false
}

/// A marker action run by the UI thread (ee_fixture_mark).
pub fn mark() {
    MARKS.fetch_add(1, Ordering::SeqCst);
}

/// One marker action posted and run by the UI thread -> false when it did not run within 5 s.
fn ui_round_trip() -> bool {
    let before = MARKS.load(Ordering::SeqCst);
    Call::new(Op::Mark).go();
    for _ in 0..100 {
        if MARKS.load(Ordering::SeqCst) != before {
            return true;
        }
        host::sleep_ms(50);
    }
    false
}

/// The index of the game form's control under the pointer (-1: none, the game view).
fn pointer_over_view() -> i64 {
    host::with_ui_lock(|| Call::new(Op::TopForm).run().i(2))
}

fn picked(t: &WorldRead, at: Point, prefer: i64) -> i64 {
    let _ = t;
    Call::new(Op::Pick).i(&[prefer]).f(&[at.0, at.1]).go()
}

/// The click point checked just before the click: the pointer there taken by the UI (two round trips), the game view
/// under it (not the HUD's panels), and the picker giving the target there (no unit at all for a move or the ground).
/// The plan's candidates are tried best first, then the target's points from a new scan, three rounds at most.
fn verify_click(kind: i32, plan: &mut OrderPlan) -> bool {
    let targeted = matches!(kind, EE_FIXTURE_GATHER | EE_FIXTURE_REPAIR | EE_FIXTURE_REPAIR_QUEUED);
    let mut candidates = plan.candidates.clone();
    let mut tried = 0;
    for round in 0..3 {
        if round > 0 {
            host::sleep_ms(500);
            candidates = if targeted {
                let (points, _) = host::with_read_lock(|| scan(1)).unwrap_or_default();
                points.get(&plan.target).map(|p| click_order(p)).unwrap_or_default()
            } else {
                choose_locked(kind).candidates
            };
        }
        for (i, &at) in candidates.iter().take(8).enumerate() {
            tried += 1;
            pointer(at.0, at.1);
            let trip = ui_round_trip() && ui_round_trip();
            let over = pointer_over_view();
            let (good, pick) = host::with_read_lock(|| {
                // SAFETY: the world's read lock is held
                let t = unsafe { WorldRead::begin() };
                let pick = picked(&t, at, i64::from(targeted));
                let good = if targeted { pick == plan.target } else { pick == -1 && picked(&t, at, 1) == -1 };
                (good, pick)
            })
            .unwrap_or((false, -1));
            if (over != -1 && over != EE_FIXTURE_MAIN_GAME_SELECT_UNITS) || !good {
                if i == 0 {
                    diag(&format!(
                        "click kind {kind}: {:.0},{:.0} refused: UI round trip {}, control {over} under the pointer, picks {pick}",
                        at.0,
                        at.1,
                        i32::from(trip)
                    ));
                }
                continue;
            }
            if tried > 1 {
                diag(&format!(
                    "click kind {kind}: point {:.0},{:.0} after {tried} tried (the first was {:.0},{:.0})",
                    at.0, at.1, plan.click.0, plan.click.1
                ));
            }
            plan.click = at;
            return true;
        }
    }
    diag(&format!(
        "click kind {kind}: no point verified ({tried} tried); target {} at {:.0},{:.0}",
        plan.target, plan.click.0, plan.click.1
    ));
    false
}

fn target_tile(id: i64) -> Option<(i64, i64)> {
    host::with_read_lock(|| {
        // SAFETY: the world's read lock is held
        let t = unsafe { WorldRead::begin() };
        unit_by_id(&t, id).map(|u| u.tile_xy())
    })
    .flatten()
}

/// The order's input (the click on the plan's point, verified first; or the status button, then the ground point)
/// and the wait for its message's verdict (at most 10 s) -> the phase then (2: no message executed).
fn send_order(kind: i32, plan: &mut OrderPlan, button: Point) -> i32 {
    if kind != EE_FIXTURE_PATROL && !verify_click(kind, plan) && plan.target > 0 {
        // no point of the target outside the HUD: the camera centred on it, as the retry does, and the plan's point
        // chosen there
        if let Some((x, y)) = target_tile(plan.target).filter(|&(x, _)| x != EE_FIXTURE_OFF_MAP) {
            let next = centre_and_choose(kind, x, y);
            if next.target == plan.target {
                plan.click = next.click;
                plan.candidates = next.candidates;
                verify_click(kind, plan);
            }
        }
    }
    match kind {
        EE_FIXTURE_GATHER | EE_FIXTURE_REPAIR | EE_FIXTURE_MOVE => click(plan.click.0, plan.click.1, 3, false),
        EE_FIXTURE_REPAIR_QUEUED => click(plan.click.0, plan.click.1, 3, true),
        EE_FIXTURE_PATROL => click(button.0, button.1, 1, false),
        _ => {
            click(button.0, button.1, 1, false);
            host::sleep_ms(400);
            click(plan.click.0, plan.click.1, 1, false);
        }
    }
    let mut phase = 2;
    for _ in 0..100 {
        if phase != 2 {
            break;
        }
        host::sleep_ms(100);
        phase = order().phase;
    }
    let others = order().others.clone();
    diag(&format!(
        "click kind {kind}: phase {phase}; the local player's other messages meanwhile:{}",
        if others.is_empty() { " none" } else { &others }
    ));
    phase
}

/// TEST ONLY: the building damaged to half its maximum (no game message does it).
fn damage(id: i64) {
    host::with_write_lock(|| {
        // SAFETY: the world's write lock is held
        let t = unsafe { WorldRead::begin() };
        if let Some(b) = own_complex(&t, id) {
            let max = max_hit_points(&b);
            if b.hit_points() > max / 2 {
                Call::new(Op::SetHitPoints).h(b.handle()).i(&[max / 2]).go();
            }
        }
    });
}

/// The retry after a click that gave no message: the camera centred on the target's tile, the view scanned and the
/// plan chosen again, the group selected again, the button found again -> the refusal (None: ready).
fn retry_plan(kind: i32, plan: &mut OrderPlan, button: &mut Point) -> Option<&'static str> {
    set_note("retry");
    let at = if plan.target >= 0 { target_tile(plan.target) } else { None };
    let (x, y) = at.unwrap_or((EE_FIXTURE_OFF_MAP, EE_FIXTURE_OFF_MAP));
    let next = if x != EE_FIXTURE_OFF_MAP {
        centre_and_choose(kind, x, y)
    } else {
        host::sleep_ms(1500); // frames drawn from the new view fill the picker
        choose_locked(kind)
    };
    diag(&format!(
        "retry kind {kind}: target was {} at {x},{y}; new target {} at {:.0},{:.0} refusal {}",
        plan.target,
        next.target,
        next.click.0,
        next.click.1,
        next.refusal.unwrap_or("-")
    ));
    if next.refusal.is_some() {
        return Some("retry-no-pick");
    }
    if next.damage >= 0 && next.damage != plan.damage {
        damage(next.damage); // as the first damage
    }
    *plan = next;
    if !select_group(&plan.units) {
        return Some("not-selected");
    }
    host::sleep_ms(800); // the group panel shows the selection's buttons
    let wanted = if kind == EE_FIXTURE_PATROL { EE_FIXTURE_BUTTON_PATROL } else { EE_FIXTURE_BUTTON_ATTACK_GROUND };
    if kind == EE_FIXTURE_PATROL || kind == EE_FIXTURE_ATTACK_GROUND {
        match find_button(wanted) {
            Some(b) => *button = b,
            None => return Some("no-button"),
        }
    }
    set_note("retried"); // kept through the verdict: the driver logs it
    None
}

fn refused(kind: i32, seq: u64, note: &str) {
    refuse(note);
    host::stderr(&format!("TEST UI order-refused order control {kind} value 0 on form 0 (seq {seq}, top form 0)\n"));
    BUSY.store(false, Ordering::SeqCst);
}

fn run(seq: u64, kind: i32) {
    let (mut plan, camera) = host::with_read_lock(|| {
        let mut plan = OrderPlan::default();
        choose(kind, &mut plan);
        (plan, camera_text())
    })
    .unwrap_or_default();
    diag(&format!(
        "plan kind {kind}: units {} target {} at {:.0},{:.0} scout {} home {} refusal {}; {camera}",
        plan.units.len(),
        plan.target,
        plan.click.0,
        plan.click.1,
        plan.scout,
        plan.home,
        plan.refusal.unwrap_or("-")
    ));
    if kind == EE_FIXTURE_GATHER && plan.target < 0 && plan.scout >= 0 {
        scout(kind, &mut plan);
    }
    if (kind == EE_FIXTURE_REPAIR || kind == EE_FIXTURE_REPAIR_QUEUED) && plan.target < 0 && plan.home >= 0 {
        home(kind, &mut plan);
    }
    if let Some(refusal) = plan.refusal {
        refused(kind, seq, refusal);
        return;
    }
    if plan.damage >= 0 {
        damage(plan.damage);
    }
    Call::new(Op::Post).i(&[0]).ids(&plan.units).go();
    let mut is_selected = false;
    for _ in 0..40 {
        if is_selected {
            break;
        }
        host::sleep_ms(150);
        is_selected = selected(&plan.units);
    }
    if !is_selected {
        refused(kind, seq, "not-selected");
        return;
    }
    host::sleep_ms(800); // the group panel shows the selection's buttons
    let mut button = (0.0f32, 0.0f32);
    if kind == EE_FIXTURE_PATROL || kind == EE_FIXTURE_ATTACK_GROUND {
        match find_button(if kind == EE_FIXTURE_PATROL {
            EE_FIXTURE_BUTTON_PATROL
        } else {
            EE_FIXTURE_BUTTON_ATTACK_GROUND
        }) {
            Some(b) => button = b,
            None => {
                refused(kind, seq, "no-button");
                return;
            }
        }
    }
    {
        let mut s = order();
        s.kind_type = EE_FIXTURE_TYPES[kind as usize];
        s.armed = true;
        s.phase = 2;
    }
    let mut phase = send_order(kind, &mut plan, button);
    let mut retry = None;
    if phase == 2 {
        // no message: once more from a fresh view (the hook stays armed: a late message counts)
        retry = retry_plan(kind, &mut plan, &mut button);
        phase = order().phase;
        if phase == 2 && retry.is_none() {
            phase = send_order(kind, &mut plan, button);
        }
    }
    if phase == 2 {
        refuse(retry.unwrap_or("no-message"));
        host::stderr(&format!(
            "TEST UI order-refused order control {kind} value 0 on form 0 (seq {seq}, top form 0)\n"
        ));
    } else {
        host::stderr(&format!("TEST UI order-done order control {kind} value 0 on form 0 (seq {seq}, top form 0)\n"));
    }
    BUSY.store(false, Ordering::SeqCst);
}

/// The order command (under the UI lock, from the poll) -> its verdict; the order runs on a thread of its own.
pub fn start(seq: u64, kind: u64) -> &'static str {
    let need = host::EE_FIXTURE_STATE_MOUSE | host::EE_FIXTURE_STATE_PHYSICS | host::EE_FIXTURE_STATE_LOCK;
    if kind >= EE_FIXTURE_KINDS || host::state() & need != need {
        return "rejected";
    }
    if BUSY.swap(true, Ordering::SeqCst) {
        return "order-busy";
    }
    {
        let mut s = order();
        *s = OrderState::new();
        s.seq = seq;
        s.kind = kind as i32;
        s.phase = 1;
    }
    std::thread::spawn(move || run(seq, kind as i32));
    "order-started"
}

/// The order lines for the ui-state file (the hook's copy).
pub fn snapshot() -> String {
    let s = order().clone();
    if s.seq == 0 {
        return String::new();
    }
    snapshot_of(&s)
}

fn snapshot_of(s: &OrderState) -> String {
    let (mut ok, mut bad, mut skipped) = (0, 0, 0);
    let mut units = String::new();
    for row in s.units.iter().filter(|r| !r.verdict.is_empty()) {
        ok += usize::from(row.verdict == "ok");
        bad += usize::from(row.verdict == "bad");
        skipped += usize::from(row.verdict == "skip");
        units.push_str(&format!("orderunit {} {} {} {} {}\n", s.seq, row.id, row.verdict, row.expected, row.found));
    }
    format!(
        "order seq {} kind {} phase {} type {} target {} point {:.3} {:.3} queued {} units {} ok {ok} bad {bad} skip {skipped} note {}\n{units}",
        s.seq,
        s.kind,
        s.phase,
        s.kind_type,
        if s.kind == EE_FIXTURE_PATROL { s.route_point } else { s.target },
        s.x,
        s.y,
        s.queued,
        s.units.len(),
        s.note()
    )
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn click_order_nearest_the_centroid_first_and_stable() {
        let points = [(0.0, 0.0), (12.0, 0.0), (24.0, 0.0), (12.0, 12.0), (0.0, 12.0)];
        // the centroid (9.6, 4.8): (12, 0) at 28.8, (12, 12) at 57.6, (0, 0) at 115.2, (0, 12) 144, (24, 0) 230.4
        assert_eq!(click_order(&points), vec![(12.0, 0.0), (12.0, 12.0), (0.0, 0.0), (0.0, 12.0), (24.0, 0.0)]);
        // equal distances keep the scan order
        assert_eq!(click_order(&[(0.0, 0.0), (2.0, 0.0)]), vec![(0.0, 0.0), (2.0, 0.0)]);
    }

    #[test]
    fn snapshot_lines() {
        let mut s = OrderState::new();
        s.seq = 7;
        s.kind = EE_FIXTURE_GATHER;
        s.phase = 3;
        s.kind_type = 0x51;
        s.target = 29;
        s.units.push(OrderUnit {
            id: 16777217,
            verdict: "ok".into(),
            expected: "gather:29".into(),
            found: "gather:29".into(),
            ..Default::default()
        });
        s.units.push(OrderUnit { id: 16777218, ..Default::default() }); // not judged yet: not listed
        assert_eq!(
            snapshot_of(&s),
            "order seq 7 kind 0 phase 3 type 81 target 29 point 0.000 0.000 queued 0 units 2 ok 1 bad 0 skip 0 note -\n\
             orderunit 7 16777217 ok gather:29 gather:29\n"
        );
        s.kind = EE_FIXTURE_PATROL;
        s.route_point = 5;
        s.note = "no-route".into();
        assert!(snapshot_of(&s).starts_with("order seq 7 kind 3 phase 3 type 81 target 5 point"));
        assert!(snapshot_of(&s).contains("note no-route\n"));
    }

    #[test]
    fn points_as_the_c_library_prints_them() {
        assert_eq!(point(12.5, -3.0), "12.500,-3.000");
        assert_eq!(format!("{:.0}", 2.5f32), "2"); // %.0f: half to even
    }
}
