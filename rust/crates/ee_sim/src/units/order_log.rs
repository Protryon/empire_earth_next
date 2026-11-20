//! The game log's human orders and selections (TEST ONLY: training data from playtests; ee/EEGameLog.h), moved
//! whole from EEPhysics.cpp (`EEPhysicsLogOrder`, `EEPhysicsLogSelection` and their helpers; the units' lane,
//! round 5). One "order" line per order message a human player's units run, as it executes in the lockstep (the
//! same in multiplayer and replays), and one "select" line when a human's selection changes. It reads the message's
//! record (its members by wire index, ee_netmsg records.rs) and the units through the world views; it changes
//! nothing. The C++ keeps one host call: a class's or a technology's name (`EECPNextClassName`, `EECPNextTechName`).

use std::collections::BTreeMap;
use std::sync::Mutex;

use crate::netmsg::records::{record_member, EE_NETMSG_RECORD_BYTES};
use crate::world::{GroupView, PlayerView, UnitView, WorldRead};

/// The member reads the log makes: a record's bytes by wire index.
pub trait OrderRecord {
    fn bytes(&self, index: u32, size: usize) -> Option<&[u8]>;
    fn i32(&self, index: u32) -> i32 {
        self.bytes(index, 4).map_or(0, |b| i32::from_le_bytes([b[0], b[1], b[2], b[3]]))
    }
    fn f32(&self, index: u32) -> f32 {
        f32::from_bits(self.i32(index) as u32)
    }
    fn bool(&self, index: u32) -> bool {
        self.bytes(index, 1).is_some_and(|b| b[0] != 0)
    }
    /// a `GE2DPoint` member's x and y
    fn point(&self, index: u32) -> (f32, f32) {
        self.bytes(index, 8).map_or((0.0, 0.0), |b| {
            (f32::from_le_bytes([b[0], b[1], b[2], b[3]]), f32::from_le_bytes([b[4], b[5], b[6], b[7]]))
        })
    }
}

/// An `EEMRecord`'s `mRecord` of message type `ty`.
pub struct Wire<'a> {
    pub ty: u8,
    pub record: &'a [u8],
}

impl OrderRecord for Wire<'_> {
    fn bytes(&self, index: u32, size: usize) -> Option<&[u8]> {
        let f = record_member(self.ty, index);
        // (the C++'s Get verified the member's size)
        if f.size as usize != size {
            return None;
        }
        self.record.get(f.offset as usize..f.offset as usize + size)
    }
}

/// What the log reads of the world.
pub trait OrderWorld {
    /// the class name of unit type `t` (`EECPNextClassName`)
    fn class_name(&mut self, t: i64) -> String;
    /// the technology's name (`EECPNextTechName`)
    fn tech_name(&mut self, tech: i64) -> String;
    /// unit `id`'s class type, owner id (0 none) and tile, when `gWorld.GetUnit(id)` has a unit with a class
    fn target(&mut self, id: i64) -> Option<(i64, u64, i64, i64)>;
    /// the player's technology `tech`: Some(its class) when it makes a unit
    fn makes_unit(&mut self, tech: i64) -> Option<i64>;
    /// the group's units: (class type, id) for each, None where the unit or its class is missing
    fn group_units(&mut self) -> Vec<Option<(i64, u64)>>;
}

// ee/EEMessageID.h
const MSG_ATTACK_UNIT: u8 = 0x4c;
const MSG_PATH_TO: u8 = 0x4d;
const MSG_GATHER_RESOURCE: u8 = 0x51;
const MSG_RESEARCH_TECHNOLOGY: u8 = 0x52;
const MSG_BUILD_UNIT: u8 = 0x56;
const MSG_REPAIR_UNIT: u8 = 0x57;
const MSG_HALT_SELECTED_GROUP: u8 = 0x58;
const MSG_BUILD_WALL: u8 = 0x59;
const MSG_KILL_UNIT: u8 = 0x5c;
const MSG_DROPOFF_RESOURCE: u8 = 0x61;
const MSG_SET_RALLY_GOAL: u8 = 0x66;
const MSG_LOAD_TRANSPORT: u8 = 0x67;
const MSG_UNLOAD_TRANSPORT: u8 = 0x68;
const MSG_ADD_PATROL_POINT: u8 = 0x72;
const MSG_PATROL: u8 = 0x73;
const MSG_CAST_CALAMITY: u8 = 0x75;
const MSG_IMPROVE_UNIT: u8 = 0x77;
const MSG_CHANGE_FORMATION: u8 = 0x78;
const MSG_EXPLORE: u8 = 0x79;
const MSG_LOAD_GARRISON: u8 = 0x7b;
const MSG_REPLANT_FARMS: u8 = 0x81;
const MSG_BUILD_GATE: u8 = 0x86;
const MSG_STOP_ACTIVE_GROUP: u8 = 0x8a;
const MSG_RETURN_TO_BASE: u8 = 0x90;
const MSG_HEAL_UNIT: u8 = 0x92;
const MSG_IMMEDIATE_CARGO_UNLOAD: u8 = 0x95;
const MSG_SET_UNIT_BEHAVIOR: u8 = 0x9e;
const MSG_ATTACK_GROUND: u8 = 0x9f;
const MSG_DISBAND: u8 = 0xa4;
const MSG_SELF_DESTRUCT: u8 = 0xa5;
const MSG_PACK_UNPACK: u8 = 0xab;
const MSG_GO_FANATIC: u8 = 0xac;

fn at(line: &mut String, x: f32, y: f32) {
    line.push_str(&format!(" at={},{}", x as i64, y as i64));
}

fn target(line: &mut String, w: &mut dyn OrderWorld, id: i32) {
    let id = i64::from(id);
    let t = if id > 0 { w.target(id) } else { None };
    match t {
        Some((class, player, x, y)) => {
            let name = w.class_name(class);
            line.push_str(&format!(" target={id}:{name}:p{player}@{x},{y}"));
        }
        None => line.push_str(&format!(" target={id}:-")),
    }
}

fn tech(line: &mut String, w: &mut dyn OrderWorld, tech: i32, verb: &mut &'static str) {
    if let Some(class) = w.makes_unit(i64::from(tech)) {
        if *verb == "research" {
            *verb = "train";
        }
        line.push_str(" class=");
        line.push_str(&w.class_name(class));
    } else {
        line.push_str(" tech=");
        line.push_str(&w.tech_name(i64::from(tech)));
    }
}

/// `EEPhysicsLogUnits`: " units=<n> classes=<name>:<count>,... ids=<id>,..." (the first 24 ids, then ",...").
pub fn units(w: &mut dyn OrderWorld) -> String {
    let all = w.group_units();
    let mut classes: BTreeMap<Vec<u8>, i64> = BTreeMap::new();
    let mut ids = String::new();
    for (i, u) in all.iter().enumerate() {
        let Some((class, id)) = *u else { continue };
        *classes.entry(w.class_name(class).into_bytes()).or_insert(0) += 1;
        if i < 24 {
            if !ids.is_empty() {
                ids.push(',');
            }
            ids.push_str(&id.to_string());
        } else if i == 24 {
            ids.push_str(",...");
        }
    }
    let mut list = String::new();
    for (name, count) in &classes {
        if !list.is_empty() {
            list.push(',');
        }
        list.push_str(&String::from_utf8_lossy(name));
        list.push_str(&format!(":{count}"));
    }
    let dash = |s: String| if s.is_empty() { "-".to_string() } else { s };
    format!(" units={} classes={} ids={}", all.len(), dash(list), dash(ids))
}

/// `EEPhysicsLogOrder`'s line for message type `ty` (None: not an order the log names).
pub fn order_line(ty: u8, m: &dyn OrderRecord, w: &mut dyn OrderWorld, time: u64, player: u64) -> Option<String> {
    let mut verb: &'static str;
    let mut rest = String::new();
    let mut group = true; // the order is for the player's active group
    match ty {
        MSG_PATH_TO => {
            verb = "move";
            at(&mut rest, m.f32(2), m.f32(3));
            if m.bool(4) {
                rest.push_str(" forced=1");
            }
            if m.bool(6) {
                rest.push_str(" queued=1");
            }
        }
        MSG_ATTACK_UNIT => {
            verb = if m.bool(3) { "convert" } else { "attack" };
            target(&mut rest, w, m.i32(1));
            if m.bool(2) {
                rest.push_str(" queued=1");
            }
        }
        MSG_GATHER_RESOURCE => {
            verb = "gather";
            target(&mut rest, w, m.i32(1));
            if m.bool(2) {
                rest.push_str(" queued=1");
            }
        }
        MSG_BUILD_UNIT => {
            verb = "build";
            tech(&mut rest, w, m.i32(2), &mut verb);
            let (x, y) = m.point(1);
            at(&mut rest, x, y);
        }
        MSG_REPAIR_UNIT => {
            verb = "repair";
            target(&mut rest, w, m.i32(1));
        }
        MSG_HALT_SELECTED_GROUP | MSG_STOP_ACTIVE_GROUP => verb = "stop",
        MSG_LOAD_GARRISON => {
            verb = "garrison";
            target(&mut rest, w, m.i32(1));
            if m.i32(2) > 0 {
                rest.push_str(&format!(" unit={}", m.i32(2)));
            }
        }
        MSG_LOAD_TRANSPORT => {
            verb = "load";
            target(&mut rest, w, m.i32(1));
        }
        MSG_UNLOAD_TRANSPORT => {
            verb = "unload";
            group = false;
            target(&mut rest, w, m.i32(1));
            at(&mut rest, m.i32(2) as f32, m.i32(3) as f32);
        }
        MSG_IMMEDIATE_CARGO_UNLOAD => {
            verb = "unload";
            group = false;
            target(&mut rest, w, m.i32(1));
        }
        MSG_PATROL => verb = "patrol",
        MSG_ADD_PATROL_POINT => {
            verb = "patrol_point";
            let (x, y) = m.point(1);
            at(&mut rest, x, y);
        }
        MSG_CHANGE_FORMATION => {
            verb = "formation";
            rest.push_str(&format!(" kind={}", m.i32(1)));
        }
        MSG_RESEARCH_TECHNOLOGY => {
            verb = "research";
            group = false;
            target(&mut rest, w, m.i32(1));
            tech(&mut rest, w, m.i32(2), &mut verb);
            if m.bool(3) {
                rest.push_str(" times=5");
            }
        }
        MSG_EXPLORE => verb = "explore",
        MSG_ATTACK_GROUND => {
            verb = "attack_ground";
            at(&mut rest, m.f32(1), m.f32(2));
        }
        MSG_HEAL_UNIT => {
            verb = "heal";
            target(&mut rest, w, m.i32(1));
        }
        MSG_SET_RALLY_GOAL => {
            verb = "rally";
            let (x, y) = m.point(1);
            at(&mut rest, x, y);
            if m.i32(2) > 0 {
                target(&mut rest, w, m.i32(2));
            }
        }
        MSG_RETURN_TO_BASE => verb = "return",
        MSG_DROPOFF_RESOURCE => {
            verb = "dropoff";
            target(&mut rest, w, m.i32(1));
        }
        MSG_SET_UNIT_BEHAVIOR => {
            verb = "stance";
            rest.push_str(&format!(" kind={}", m.i32(1)));
        }
        MSG_BUILD_WALL => {
            verb = "wall";
            tech(&mut rest, w, m.i32(3), &mut verb);
            let (x, y) = m.point(1);
            at(&mut rest, x, y);
            let (x, y) = m.point(2);
            at(&mut rest, x, y);
        }
        MSG_BUILD_GATE => {
            verb = "gate";
            tech(&mut rest, w, m.i32(2), &mut verb);
            let (x, y) = m.point(1);
            at(&mut rest, x, y);
        }
        MSG_DISBAND | MSG_KILL_UNIT | MSG_SELF_DESTRUCT => verb = "delete",
        MSG_IMPROVE_UNIT => {
            verb = "improve";
            group = false;
            target(&mut rest, w, m.i32(1));
        }
        MSG_PACK_UNPACK => verb = if m.bool(1) { "pack" } else { "unpack" },
        MSG_GO_FANATIC => verb = "fanatic",
        MSG_REPLANT_FARMS => verb = "replant",
        MSG_CAST_CALAMITY => verb = "calamity",
        _ => return None,
    }
    let mut line = format!("EE-GAME {time} p{player} order verb={verb}");
    if group {
        line.push_str(&units(w));
    }
    line.push_str(&rest);
    line.push('\n');
    Some(line)
}

/// `EEPhysicsLogSelection`'s line, when player `id`'s selection changed since its last one.
pub fn select_line(last: &mut Vec<String>, id: u64, w: &mut dyn OrderWorld, time: u64) -> Option<String> {
    if id >= 64 {
        return None;
    }
    let units = units(w);
    if last.len() < 64 {
        last.resize(64, String::new());
    }
    if last[id as usize] == units {
        return None;
    }
    last[id as usize].clone_from(&units);
    Some(format!("EE-GAME {time} p{id} select{units}\n"))
}

// ------------------------------------------------------------------------------------------------ the game's world

#[cfg(not(test))]
extern "C" {
    /// `EECPNextClassName(key)` (`op` 0) or `EECPNextTechName(key)` (1) into `text` -> its length; `EEGameLogLine`
    /// of the NUL-ended `text` (2) (EEPhysics.cpp)
    fn ee_units_order_log_host(op: u8, key: i64, text: *mut u8, capacity: usize) -> usize;
}

#[cfg(test)]
unsafe fn ee_units_order_log_host(_op: u8, _key: i64, _text: *mut u8, _capacity: usize) -> usize {
    0
}

struct Game<'t> {
    t: &'t WorldRead,
    player: PlayerView<'t>,
    group: Option<GroupView<'t>>,
}

fn name(tech: u8, key: i64) -> String {
    let mut b = [0u8; 256];
    // SAFETY: a live buffer of its capacity
    let n = unsafe { ee_units_order_log_host(tech, key, b.as_mut_ptr(), b.len()) }.min(b.len());
    String::from_utf8_lossy(&b[..n]).into_owned()
}

impl OrderWorld for Game<'_> {
    fn class_name(&mut self, t: i64) -> String {
        name(0, t)
    }
    fn tech_name(&mut self, tech: i64) -> String {
        name(1, tech)
    }
    fn target(&mut self, id: i64) -> Option<(i64, u64, i64, i64)> {
        let u = UnitView::by_id(self.t, id as u32)?;
        let c = u.class()?;
        let (x, y) = u.tile_xy();
        Some((c.unit_type() as i64, u.player().map_or(0, |p| u64::from(p.id())), x, y))
    }
    fn makes_unit(&mut self, tech: i64) -> Option<i64> {
        let node = self.player.tech_tree()?.node(tech)?;
        let d = node.data()?;
        let makes: bool = crate::world_field!(d, mMakesUnit);
        let class: std::os::raw::c_long = crate::world_field!(d, mUnitClass);
        makes.then_some(i64::from(class))
    }
    fn group_units(&mut self) -> Vec<Option<(i64, u64)>> {
        let Some(g) = self.group else { return Vec::new() };
        (0..g.len())
            .map(|i| {
                // SAFETY: a live group's unit
                let u = unsafe { UnitView::of(self.t, g.unit_handle(i)) }?;
                Some((u.class()?.unit_type() as i64, u64::from(u.id())))
            })
            .collect()
    }
}

static LAST: Mutex<Vec<String>> = Mutex::new(Vec::new());

/// `EEPhysicsLogOrder` (`select` 0) and `EEPhysicsLogSelection` (1), while a game log is open (the C++ asks): the line
/// for a human `player`'s message of type
/// `ty` (its `EEMRecord::mRecord` at `record`, or NULL for a message that is not a record) at game time `time`;
/// `group` is the player's `mActiveGroup`.
///
/// # Safety
/// `player` and `group` are 0 or live; `record` is NULL or `EE_NETMSG_RECORD_BYTES` readable bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_units_order_log(player: u64, group: u64, ty: u8, record: *const u8, time: u32, select: u8) {
    if player == 0 {
        return;
    }
    // SAFETY: the physics thread reads the world it steps
    let t = unsafe { WorldRead::begin() };
    // SAFETY: the contract
    let Some(p) = (unsafe { PlayerView::of(&t, player) }) else { return };
    if !crate::units::uu_direct::with_game(|g| g.is_human(player)) {
        return;
    }
    // SAFETY: the contract
    let group = unsafe { GroupView::of(&t, group) };
    let mut w = Game { t: &t, player: p, group };
    let id = u64::from(p.id());
    let line = if select != 0 {
        select_line(&mut LAST.lock().unwrap_or_else(std::sync::PoisonError::into_inner), id, &mut w, u64::from(time))
    } else {
        let bytes: &[u8] = if record.is_null() {
            &[]
        } else {
            // SAFETY: the contract
            unsafe { std::slice::from_raw_parts(record, EE_NETMSG_RECORD_BYTES) }
        };
        order_line(ty, &Wire { ty, record: bytes }, &mut w, u64::from(time), id)
    };
    if let Some(line) = line {
        let mut text = line.into_bytes();
        text.push(0);
        // SAFETY: a NUL-ended line
        unsafe { ee_units_order_log_host(2, 0, text.as_mut_ptr(), text.len()) };
    }
}

#[cfg(test)]
#[path = "order_log_tests.rs"]
mod tests;
