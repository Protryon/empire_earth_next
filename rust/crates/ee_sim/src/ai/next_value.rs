//! The next engine's view (`EECPNextView`, EECPNext.h before; lane A, round 3): the snapshot of what the strategy
//! may know, built fresh each step (`next_game`), and the table the script gets (`EECPNextViewValue`,
//! EECPNextLua.cpp before): snake_case fields, arrays 1-based, per-resource values as maps by resource name, the 0/1
//! flags as booleans, classes by name. Also the orders' check (every order is refused now: the verbs are
//! data/ai/lib/verbs.lua's).

use std::os::raw::c_long;

use crate::ai::lua_value::{OwnedKey, Value};
use crate::ai::names;
use crate::ai::next_save::OrderResult;

/// `kNextResources`
pub const RESOURCES: usize = 5;
const RESOURCE_NAMES: [&str; RESOURCES] = ["food", "wood", "stone", "gold", "iron"];
/// `kEECPClaimRange`, `kEECPClaimRangeStep`, `kEECPClaimRangeMax` (ee/EECPTrace.h)
pub const CLAIM_RANGE: c_long = 25;
pub const CLAIM_RANGE_STEP: c_long = 5;
pub const CLAIM_RANGE_MAX: c_long = 45;
/// `kNextMaxOrders`: orders past this many in one step are dropped (traced)
pub const MAX_ORDERS: usize = 200;
/// `kNextRefused`, `kNextReasonBadArgument`
pub const CODE_REFUSED: i32 = 2;
pub const REASON_BAD_ARGUMENT: i32 = 6;

/// A resource centre (`EECPNextSite`).
#[derive(Clone, Copy, Debug, Default)]
pub struct Site {
    pub index: c_long,
    pub id: c_long,
    pub resource: c_long,
    pub units: c_long,
    pub amount: c_long,
    pub room: c_long,
    pub workers: c_long,
    pub capacity: c_long,
    pub x: c_long,
    pub y: c_long,
    pub distance: c_long,
    pub claimed: c_long,
    pub has_drop: c_long,
    pub ready: [c_long; RESOURCES],
    pub drop: [c_long; RESOURCES],
    pub has_tower: c_long,
    pub danger: c_long,
    pub patch: c_long,
    pub explored: c_long,
    pub blocked: c_long,
    pub holds: [c_long; RESOURCES],
    pub home: c_long,
    pub cause: c_long,
}

/// The player's buildings of one type (`EECPNextBuildings`).
#[derive(Clone, Copy, Debug, Default)]
pub struct Buildings {
    pub unit_type: c_long,
    pub finished: c_long,
    pub unfinished: c_long,
    pub queued: c_long,
    pub busy: c_long,
    pub queue: c_long,
}

/// The player's living land soldiers of one class (`EECPNextSoldiers`).
#[derive(Clone, Copy, Debug, Default)]
pub struct Soldiers {
    pub unit_type: c_long,
    pub count: c_long,
    pub strength: c_long,
}

/// Seen enemy soldiers near the town, by player (`EECPNextThreat`).
#[derive(Clone, Copy, Debug, Default)]
pub struct Threat {
    pub player: c_long,
    pub count: c_long,
    pub strength: c_long,
    pub x: c_long,
    pub y: c_long,
    pub distance: c_long,
}

/// What the player has seen of an enemy (`EECPNextEnemy`).
#[derive(Clone, Copy, Debug, Default)]
pub struct Enemy {
    pub player: c_long,
    pub epoch: c_long,
    pub seen_army: c_long,
    pub seen_strength: c_long,
    pub last_seen: c_long,
    pub town_x: c_long,
    pub town_y: c_long,
    pub towers: c_long,
    pub citizens_seen: c_long,
    pub citizen_x: c_long,
    pub citizen_y: c_long,
    pub army_x: c_long,
    pub army_y: c_long,
}

/// A class the player can make now (`EECPNextClassCost`).
#[derive(Clone, Copy, Debug, Default)]
pub struct ClassCost {
    pub unit_type: c_long,
    pub cost: [c_long; RESOURCES],
    pub building: c_long,
    pub military: c_long,
    pub producer: c_long,
    pub producers: c_long,
    pub requested: c_long,
    pub in_production: c_long,
    pub counts_toward_epoch: c_long,
}

/// An event since the last step (`EECPNextEvent`).
#[derive(Clone, Copy, Debug, Default)]
pub struct ViewEvent {
    pub kind: c_long,
    pub unit_type: c_long,
    pub count: c_long,
}

/// The view (`EECPNextView`; value-initialized: every count starts at 0).
#[derive(Clone, Debug, Default)]
pub struct View {
    pub player: c_long,
    pub time: c_long,
    pub epoch: c_long,
    pub epoch_start: c_long,
    pub last_epoch: c_long,
    pub advancing: c_long,
    pub stock: [c_long; RESOURCES],
    pub income: [c_long; RESOURCES],
    pub income_total: [c_long; RESOURCES],
    pub reserve: [c_long; RESOURCES],
    pub gatherers: [c_long; RESOURCES],
    pub wanted: [c_long; RESOURCES],
    pub idle: c_long,
    pub citizens: c_long,
    pub boats: c_long,
    pub population: c_long,
    pub population_cap: c_long,
    pub town_x: c_long,
    pub town_y: c_long,
    pub claim_range: c_long,
    pub sites: Vec<Site>,
    pub site_units: Vec<Vec<c_long>>,
    pub buildings: Vec<Buildings>,
    pub soldiers: Vec<Soldiers>,
    pub army_count: c_long,
    pub army_strength: c_long,
    pub threats: Vec<Threat>,
    pub threat_strength: c_long,
    pub enemies: Vec<Enemy>,
    pub next_epoch_cost: [c_long; RESOURCES],
    pub epoch2_cost: [c_long; RESOURCES],
    pub sol_missing: c_long,
    pub costs: Vec<ClassCost>,
    pub events: Vec<ViewEvent>,
    pub free_soldiers: c_long,
    pub free_strength: c_long,
    pub hunt: c_long,
    pub hunters: c_long,
    pub wood_x: c_long,
    pub wood_y: c_long,
    pub wood_distance: c_long,
    pub wood_drop: c_long,
    pub wood_ready: c_long,
}

/// A map built field by field, its keys kept in the tree's order (`EECPLuaValue::Set`: a key set twice keeps its
/// last value).
pub struct MapBuilder(Vec<(OwnedKey, Value)>);

impl Default for MapBuilder {
    fn default() -> Self {
        Self::new()
    }
}

impl MapBuilder {
    pub fn new() -> MapBuilder {
        MapBuilder(Vec::new())
    }
    pub fn set(&mut self, key: &str, v: Value) -> &mut Self {
        let k = OwnedKey::Str(key.as_bytes().to_vec());
        match self.0.binary_search_by(|(x, _)| x.as_key().cmp(&k.as_key())) {
            Ok(i) => self.0[i].1 = v,
            Err(i) => self.0.insert(i, (k, v)),
        }
        self
    }
    pub fn int(&mut self, key: &str, v: c_long) -> &mut Self {
        self.set(key, Value::Int(v as i64))
    }
    pub fn flag(&mut self, key: &str, v: c_long) -> &mut Self {
        self.set(key, Value::Bool(v != 0))
    }
    pub fn text(&mut self, key: &str, v: &[u8]) -> &mut Self {
        self.set(key, Value::Str(v.to_vec()))
    }
    pub fn done(self) -> Value {
        Value::Map(self.0)
    }
}

fn resources(v: &[c_long; RESOURCES]) -> Value {
    let mut m = MapBuilder::new();
    for (i, name) in RESOURCE_NAMES.iter().enumerate() {
        m.int(name, v[i]);
    }
    m.done()
}

const BLOCKED: [&str; 6] = ["", "unexplored", "enemy_near", "danger", "area", "tower"];

/// `EECPNextViewValue`: the view as the script sees it, with the last step's results. `class_name(type)` is
/// `EECPNextClassName`.
pub fn to_value(view: &View, results: &[OrderResult], class_name: &mut dyn FnMut(c_long) -> Vec<u8>) -> Value {
    let mut v = MapBuilder::new();
    v.int("player", view.player)
        .int("time", view.time)
        .int("epoch", view.epoch)
        .int("epoch_start", view.epoch_start)
        .int("last_epoch", view.last_epoch)
        .flag("advancing", view.advancing)
        .set("stock", resources(&view.stock))
        .set("income", resources(&view.income))
        .set("income_total", resources(&view.income_total))
        .set("reserve", resources(&view.reserve))
        .set("gatherers", resources(&view.gatherers))
        .set("wanted", resources(&view.wanted))
        .int("idle", view.idle)
        .int("citizens", view.citizens)
        .int("boats", view.boats)
        .int("population", view.population)
        .int("population_cap", view.population_cap)
        .int("town_x", view.town_x)
        .int("town_y", view.town_y)
        .int("claim_range", view.claim_range)
        .int("claim_range_min", CLAIM_RANGE)
        .int("claim_range_max", CLAIM_RANGE_MAX)
        .int("claim_range_step", CLAIM_RANGE_STEP);
    let mut sites = Vec::new();
    for (k, s) in view.sites.iter().enumerate() {
        let mut e = MapBuilder::new();
        e.int("index", s.index.wrapping_add(1)).int("id", s.id);
        if (0..RESOURCES as c_long).contains(&s.resource) {
            e.text("resource", RESOURCE_NAMES[s.resource as usize].as_bytes());
        }
        e.int("units", s.units)
            .int("amount", s.amount)
            .int("room", s.room)
            .int("workers", s.workers)
            .int("capacity", s.capacity)
            .int("x", s.x)
            .int("y", s.y)
            .int("distance", s.distance)
            .flag("claimed", s.claimed)
            .flag("has_drop", s.has_drop)
            .set("ready", resources(&s.ready))
            .set("drop", resources(&s.drop))
            .flag("has_tower", s.has_tower)
            .int("danger", s.danger)
            .flag("patch", s.patch)
            .set("holds", resources(&s.holds))
            .flag("explored", s.explored)
            .flag("home", s.home);
        if s.blocked > 0 && s.blocked <= 5 {
            e.text("blocked", BLOCKED[s.blocked as usize].as_bytes());
        }
        if let Some(ids) = view.site_units.get(k) {
            e.set("ids", Value::Array(ids.iter().map(|&i| Value::Int(i as i64)).collect()));
        }
        sites.push(e.done());
    }
    v.set("sites", Value::Array(sites));
    let mut buildings = Vec::new();
    for b in &view.buildings {
        let mut e = MapBuilder::new();
        e.set("type", Value::Str(class_name(b.unit_type)))
            .int("finished", b.finished)
            .int("unfinished", b.unfinished)
            .int("queued", b.queued)
            .int("busy", b.busy)
            .int("queue", b.queue);
        buildings.push(e.done());
    }
    v.set("buildings", Value::Array(buildings));
    let mut soldiers = Vec::new();
    for s in &view.soldiers {
        let mut e = MapBuilder::new();
        e.set("type", Value::Str(class_name(s.unit_type))).int("count", s.count).int("strength", s.strength);
        soldiers.push(e.done());
    }
    v.set("soldiers", Value::Array(soldiers))
        .int("army_count", view.army_count)
        .int("army_strength", view.army_strength);
    let mut threats = Vec::new();
    for t in &view.threats {
        let mut e = MapBuilder::new();
        e.int("player", t.player)
            .int("count", t.count)
            .int("strength", t.strength)
            .int("x", t.x)
            .int("y", t.y)
            .int("distance", t.distance);
        threats.push(e.done());
    }
    v.set("threats", Value::Array(threats)).int("threat_strength", view.threat_strength);
    let mut enemies = Vec::new();
    for n in &view.enemies {
        let mut e = MapBuilder::new();
        e.int("player", n.player)
            .int("epoch", n.epoch)
            .int("seen_army", n.seen_army)
            .int("seen_strength", n.seen_strength)
            .int("last_seen", n.last_seen)
            .int("town_x", n.town_x)
            .int("town_y", n.town_y)
            .int("towers", n.towers)
            .int("citizens_seen", n.citizens_seen)
            .int("citizen_x", n.citizen_x)
            .int("citizen_y", n.citizen_y)
            .int("army_x", n.army_x)
            .int("army_y", n.army_y);
        enemies.push(e.done());
    }
    v.set("enemies", Value::Array(enemies))
        .set("next_epoch_cost", resources(&view.next_epoch_cost))
        .set("epoch2_cost", resources(&view.epoch2_cost))
        .int("sol_missing", view.sol_missing);
    let mut costs = Vec::new();
    for c in &view.costs {
        let mut e = MapBuilder::new();
        e.set("type", Value::Str(class_name(c.unit_type)))
            .set("cost", resources(&c.cost))
            .flag("building", c.building)
            .flag("military", c.military);
        if c.producer >= 0 {
            e.set("producer", Value::Str(class_name(c.producer)));
        }
        e.int("producers", c.producers)
            .int("requested", c.requested)
            .int("in_production", c.in_production)
            .flag("counts_toward_epoch", c.counts_toward_epoch);
        costs.push(e.done());
    }
    v.set("costs", Value::Array(costs));
    let mut events = Vec::new();
    for ev in &view.events {
        let mut e = MapBuilder::new();
        e.text("kind", names::event_name(ev.kind as i64, c"unknown").to_bytes())
            .set("type", Value::Str(class_name(ev.unit_type)))
            .int("count", ev.count);
        events.push(e.done());
    }
    v.set("events", Value::Array(events))
        .int("free_soldiers", view.free_soldiers)
        .int("free_strength", view.free_strength)
        .int("hunt", view.hunt)
        .int("hunters", view.hunters);
    if view.wood_x >= 0 {
        let mut w = MapBuilder::new();
        w.int("x", view.wood_x)
            .int("y", view.wood_y)
            .int("distance", view.wood_distance)
            .flag("has_drop", view.wood_drop)
            .flag("ready", view.wood_ready);
        v.set("wood", w.done());
    }
    let mut out = Vec::new();
    for r in results {
        let mut e = MapBuilder::new();
        let reason = if r.reason == 0 { c"none" } else { names::reason_name(r.reason as i64) };
        e.set("verb", Value::Str(r.verb.clone()))
            .text("code", names::code_name(r.code as i64).to_bytes())
            .text("reason", reason.to_bytes())
            .int("count", r.count as c_long);
        out.push(e.done());
    }
    v.set("results", Value::Array(out));
    v.done()
}

/// A step's orders run: the trace lines (event, text) and the results.
pub type Orders = (Vec<(&'static str, String)>, Vec<OrderResult>);

/// What `EECPNextRunOrders` does with a step's orders: Err(message) when they are not an array of orders (a
/// `next.error` line), else the trace lines (event, text) in order and the results. Every order is refused as
/// bad_argument with verb "?" (stage 6: the verbs are data/ai/lib/verbs.lua's).
pub fn run_orders(orders: &Value) -> Result<Orders, &'static str> {
    let items = match orders {
        Value::Nil => return Ok((Vec::new(), Vec::new())),
        Value::Map(m) if m.is_empty() => return Ok((Vec::new(), Vec::new())),
        Value::Array(a) => a,
        _ => return Err("the orders must be an array of tables {verb = ...}"),
    };
    let mut lines = Vec::new();
    let mut n = items.len();
    if n > MAX_ORDERS {
        lines.push(("next.orders", format!("dropped={} (at most {} a step)", n - MAX_ORDERS, MAX_ORDERS)));
        n = MAX_ORDERS;
    }
    let mut results = Vec::new();
    for order in &items[..n] {
        let why = match order {
            Value::Map(m) => match m.iter().find(|(k, _)| *k == OwnedKey::Str(b"verb".to_vec())) {
                Some((_, Value::Str(verb))) => format!("unknown verb {}", String::from_utf8_lossy(verb)),
                _ => "no verb".to_string(),
            },
            _ => "an order is a table {verb = ...}".to_string(),
        };
        lines.push(("next.order", format!("verb=? ({why}) result=refused reason=bad_argument n=0")));
        results.push(OrderResult { verb: b"?".to_vec(), code: CODE_REFUSED, reason: REASON_BAD_ARGUMENT, count: 0 });
    }
    Ok((lines, results))
}

/// `EECPNextOneLine`: one line of trace text (no line breaks or tabs, at most 400 bytes)
pub fn one_line(text: &[u8]) -> Vec<u8> {
    let mut line = Vec::new();
    for &c in text {
        if line.len() >= 400 {
            break;
        }
        match c {
            b'\r' => {}
            b'\n' => line.extend_from_slice(b" | "),
            b'\t' => line.push(b' '),
            _ => line.push(c),
        }
    }
    line
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::ai::lua_value::to_text;

    fn text(v: &Value) -> String {
        let mut out = Vec::new();
        to_text(v, &mut out);
        String::from_utf8(out).unwrap()
    }

    #[test]
    fn map_keys_sorted_and_replaced() {
        let mut m = MapBuilder::new();
        m.int("b", 1).int("a", 2).int("b", 3).flag("c", 0);
        assert_eq!(text(&m.done()), "{a=2,b=3,c=false}");
    }

    #[test]
    fn the_view_table() {
        let mut view = View { player: 2, wood_x: -1, ..Default::default() };
        view.stock[0] = 100;
        view.sites.push(Site {
            index: 0,
            id: 41,
            resource: 3,
            blocked: 5,
            x: 4,
            y: 5,
            claimed: 1,
            ..Default::default()
        });
        view.site_units.push(vec![41, 42]);
        view.costs.push(ClassCost { unit_type: 7, producer: -1, ..Default::default() });
        view.events.push(ViewEvent { kind: 9, unit_type: 7, count: 1 });
        let results = [OrderResult { verb: b"?".to_vec(), code: 2, reason: 6, count: 0 }];
        let v = to_value(&view, &results, &mut |t| format!("c{t}").into_bytes());
        let t = text(&v);
        assert!(t.starts_with("{advancing=false,army_count=0,army_strength=0,boats=0,buildings=[],citizens=0,"), "{t}");
        assert!(t.contains("claim_range_max=45,claim_range_min=25,claim_range_step=5,"));
        assert!(t.contains("costs=[{building=false,cost={food=0,gold=0,iron=0,stone=0,wood=0},counts_toward_epoch=false,in_production=0,military=false,producers=0,requested=0,type=\"c7\"}]"));
        assert!(t.contains("events=[{count=1,kind=\"unknown\",type=\"c7\"}]"));
        assert!(t.contains("results=[{code=\"refused\",count=0,reason=\"bad_argument\",verb=\"?\"}]"));
        assert!(t.contains("blocked=\"tower\",capacity=0,claimed=true,"));
        assert!(t.contains("ids=[41,42],index=1,"));
        assert!(t.contains("resource=\"gold\","));
        assert!(t.contains("stock={food=100,"));
        assert!(!t.contains("wood={"));
    }

    #[test]
    fn orders_are_refused() {
        assert!(run_orders(&Value::Nil).unwrap().1.is_empty());
        assert!(run_orders(&Value::Map(Vec::new())).unwrap().1.is_empty());
        assert!(run_orders(&Value::Int(3)).is_err());
        let order = Value::Map(vec![(OwnedKey::Str(b"verb".to_vec()), Value::Str(b"build".to_vec()))]);
        let (lines, results) = run_orders(&Value::Array(vec![order, Value::Int(1), Value::Map(Vec::new())])).unwrap();
        assert_eq!(lines[0].1, "verb=? (unknown verb build) result=refused reason=bad_argument n=0");
        assert_eq!(lines[1].1, "verb=? (an order is a table {verb = ...}) result=refused reason=bad_argument n=0");
        assert_eq!(lines[2].1, "verb=? (no verb) result=refused reason=bad_argument n=0");
        assert_eq!(results.len(), 3);
        let many = Value::Array(vec![Value::Nil; 205]);
        let (lines, results) = run_orders(&many).unwrap();
        assert_eq!(lines[0], ("next.orders", "dropped=5 (at most 200 a step)".to_string()));
        assert_eq!(results.len(), 200);
    }

    #[test]
    fn one_line_text() {
        assert_eq!(one_line(b"a\r\nb\tc"), b"a | b c");
        assert_eq!(one_line(&[b'x'; 500]).len(), 400);
    }
}
