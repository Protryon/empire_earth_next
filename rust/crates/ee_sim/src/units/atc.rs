//! `EEAirTrafficControl` whole (0x54e839..0x551c28; RUST_PLAN R5b.4, the thirtieth round): one per player and air
//! traffic type, the airports its planes are routed to, each with a landing and a takeoff queue, and the player's air
//! cargo totals. The state is Rust's ([`EeUnitsAtc`], owned by the C++ object's `mRust`); the units (the airports and
//! the queued planes, each holding one reference while listed) are the C++ objects' addresses, read and changed by
//! host operations ([`EeUnitsAtcFn`]). Its Load and Save are atc_save.rs's.
//!
//! The bodies are the C++ ones in their order. The host may call back into the same object while one of them runs
//! (a plane's `Kill` takes its weight off the used cargo; a rerouted plane's `ActionChanged` queues it at its new
//! airport), so no Rust reference to the state lives across a host call: every access borrows it anew through the
//! raw pointer, and the queues are walked by index with their lengths read each step.
//!
//! Kept quirks: `QueueRequest` refuses every non-zero kind while the used cargo is over the maximum; an airport's
//! room counts a landing plane not yet inside and a departing one still inside; `ClearAirportQueue` matches the
//! families of kinds 1-3 and only idle planes; `UpdateLandings` stops at the first plane that landed or died and
//! never lets a landing block the takeoffs (the original's always-false test); a lost airport's departing planes
//! low outside it die, its landing ones go to the nearest airport with room or die. The candidates' sort is stable
//! (the original's `std::sort` sorted its few candidates by insertion, which keeps equal distances in order).

#![allow(clippy::unnecessary_cast)]

use ee_base::math::types::GeTransformation;
use std::collections::VecDeque;
use std::ffi::c_void;
use std::fmt::Write as _;
use std::os::raw::{c_long, c_ulong};

/// A unit: the C++ object's address (an `EEComplexUnit*`), or between a load and `RepairPointers` its saved id.
pub type Unit = *mut c_void;

/// One queued plane: the unit and the kind of request (1 a landing, any other a takeoff).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Item {
    pub unit: Unit,
    pub kind: c_ulong,
}

/// One airport with its queues.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Slot {
    pub landing: VecDeque<Item>,
    pub takeoff: VecDeque<Item>,
    pub airport: Unit,
}

impl Slot {
    pub fn new(airport: Unit) -> Slot {
        Slot { landing: VecDeque::new(), takeoff: VecDeque::new(), airport }
    }
}

/// The class's state (`mSlots`, `mMaxCargo`, `mUsedCargo`).
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct EeUnitsAtc {
    pub slots: Vec<Slot>,
    pub max_cargo: c_long,
    pub used_cargo: c_long,
}

/// A candidate airport for a reroute (`EEATCCandidate`).
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Candidate {
    pub airport: Unit,
    pub distance: f32,
    pub free: c_long,
}

/// One host operation's values.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsAtcIo {
    pub a: i64,
    pub b: i64,
    pub f: f32,
    /// a location's x, y, z
    pub p: [f32; 3],
}

/// The host: `(ctx, op, unit, other, io) -> the answer`.
pub type EeUnitsAtcFn = unsafe extern "C" fn(*mut c_void, i32, *mut c_void, *mut c_void, *mut EeUnitsAtcIo) -> i64;

// ---- the host operations (the callback's `op`)
/// the unit's class's `mWeight`
pub const EE_UNITS_ATC_WEIGHT: i32 = 1;
/// the unit's class's `mTransportCapacity`
pub const EE_UNITS_ATC_TRANSPORT_CAPACITY: i32 = 2;
/// `unit->Reference()`
pub const EE_UNITS_ATC_REFERENCE: i32 = 3;
/// `unit->Dereference()`
pub const EE_UNITS_ATC_DEREFERENCE: i32 = 4;
/// `mIsClearanceGranted`, written only when it differs: = `io.a != 0`
pub const EE_UNITS_ATC_SET_CLEARANCE: i32 = 5;
/// `mIsClearanceGranted`
pub const EE_UNITS_ATC_CLEARANCE: i32 = 6;
/// `mHitPoints`
pub const EE_UNITS_ATC_HIT_POINTS: i32 = 7;
/// the airport's `mCargoRoom`
pub const EE_UNITS_ATC_CARGO_ROOM: i32 = 8;
/// whether `other` is in the airport `unit`'s `mCargo`
pub const EE_UNITS_ATC_IN_CARGO: i32 = 9;
/// the unit's class's `mFamily`
pub const EE_UNITS_ATC_FAMILY: i32 = 10;
/// `GetCommandType()`
pub const EE_UNITS_ATC_COMMAND_TYPE: i32 = 11;
/// the airport `unit`'s exiting unit set to -1 when it is set and its passenger is the plane `other`
pub const EE_UNITS_ATC_RELEASE_EXITING: i32 = 12;
/// `mTileX` into `io.a`, `mTileY` into `io.b`
pub const EE_UNITS_ATC_TILE: i32 = 13;
/// the physical model's location: x, y, z into `io.p`
pub const EE_UNITS_ATC_LOCATION: i32 = 14;
/// the unit's class's `mAirTrafficType`
pub const EE_UNITS_ATC_AIR_TRAFFIC_TYPE: i32 = 15;
/// the unit's class's `GetCruiseAltitude()` into `io.f`
pub const EE_UNITS_ATC_CRUISE_ALTITUDE: i32 = 16;
/// `mContainer != NULL`
pub const EE_UNITS_ATC_IN_CONTAINER: i32 = 17;
/// `Kill(1, 0, 0)`
pub const EE_UNITS_ATC_KILL: i32 = 18;
/// `SetHomeBase(other)`, `StartAction(kActionRequestLanding)`, the class's `ActionChanged(unit)`
pub const EE_UNITS_ATC_REROUTE: i32 = 19;
/// the unit's class is an airplane or a planet spaceship (by category)
pub const EE_UNITS_ATC_IS_AIRCRAFT: i32 = 20;
/// `mHomeBase`
pub const EE_UNITS_ATC_HOME_BASE: i32 = 21;
/// `GetUnitID()`
pub const EE_UNITS_ATC_UNIT_ID: i32 = 22;
/// the complex unit of id `io.a` in the world (`EEUnitCast<EEComplexUnit>(gWorld.GetUnit(id))`), or null
pub const EE_UNITS_ATC_RESOLVE: i32 = 23;
/// the dump's line: the NUL-terminated text at `unit`
pub const EE_UNITS_ATC_DUMP_LINE: i32 = 24;

// the families and the command `ClearAirportQueue` tests (ee/EEUnitKind.h, ee/EEUnit.h)
const FAMILY_AIRCRAFT: i64 = 5;
const FAMILY_SPACE_FIGHTER: i64 = 0x39;
const FAMILY_BOMBER: i64 = 0x14;
const FAMILY_ATOMIC_BOMBER: i64 = 0x2a;
const ACTION_IDLE: i64 = 5000;

/// The host's side: the units' reads and the world calls.
pub trait AtcHost {
    fn call(&mut self, op: i32, unit: Unit, other: Unit, io: &mut EeUnitsAtcIo) -> i64;

    fn get(&mut self, op: i32, unit: Unit) -> i64 {
        self.call(op, unit, std::ptr::null_mut(), &mut EeUnitsAtcIo::default())
    }
    fn get2(&mut self, op: i32, unit: Unit, other: Unit) -> i64 {
        self.call(op, unit, other, &mut EeUnitsAtcIo::default())
    }
    fn weight(&mut self, unit: Unit) -> c_long {
        self.get(EE_UNITS_ATC_WEIGHT, unit) as c_long
    }
    fn reference(&mut self, unit: Unit) {
        self.get(EE_UNITS_ATC_REFERENCE, unit);
    }
    fn dereference(&mut self, unit: Unit) {
        self.get(EE_UNITS_ATC_DEREFERENCE, unit);
    }
    fn set_clearance(&mut self, unit: Unit, value: bool) {
        self.call(
            EE_UNITS_ATC_SET_CLEARANCE,
            unit,
            std::ptr::null_mut(),
            &mut EeUnitsAtcIo { a: value as i64, ..Default::default() },
        );
    }
    fn hit_points(&mut self, unit: Unit) -> c_long {
        self.get(EE_UNITS_ATC_HIT_POINTS, unit) as c_long
    }
    fn location(&mut self, unit: Unit) -> GeTransformation {
        let mut io = EeUnitsAtcIo::default();
        self.call(EE_UNITS_ATC_LOCATION, unit, std::ptr::null_mut(), &mut io);
        let mut t = GeTransformation::default();
        t.m[0][3] = io.p[0];
        t.m[1][3] = io.p[1];
        t.m[2][3] = io.p[2];
        t
    }
    fn cruise_altitude(&mut self, unit: Unit) -> f32 {
        let mut io = EeUnitsAtcIo::default();
        self.call(EE_UNITS_ATC_CRUISE_ALTITUDE, unit, std::ptr::null_mut(), &mut io);
        io.f
    }
    fn in_container(&mut self, unit: Unit) -> bool {
        self.get(EE_UNITS_ATC_IN_CONTAINER, unit) != 0
    }
    fn kill(&mut self, unit: Unit) {
        self.get(EE_UNITS_ATC_KILL, unit);
    }
    fn unit_id(&mut self, unit: Unit) -> c_ulong {
        self.get(EE_UNITS_ATC_UNIT_ID, unit) as c_ulong
    }
}

/// The state behind its raw pointer: each access a fresh borrow that ends before the next host call.
#[derive(Clone, Copy)]
pub struct AtcRef(pub *mut EeUnitsAtc);

impl AtcRef {
    fn with<R>(self, f: impl FnOnce(&mut EeUnitsAtc) -> R) -> R {
        // SAFETY: the C++ object's state, alive for the call; the borrow ends inside `f`, before any host call
        f(unsafe { &mut *self.0 })
    }
    fn slot_count(self) -> usize {
        self.with(|a| a.slots.len())
    }
    fn airport(self, s: usize) -> Unit {
        self.with(|a| a.slots[s].airport)
    }
    fn find(self, airport: Unit) -> Option<usize> {
        self.with(|a| a.slots.iter().position(|s| s.airport == airport))
    }
    /// The queue `takeoff` of slot `s`, its item `i` (none past the end).
    fn item(self, s: usize, takeoff: bool, i: usize) -> Option<Item> {
        self.with(|a| {
            let slot = a.slots.get(s)?;
            let q = if takeoff { &slot.takeoff } else { &slot.landing };
            q.get(i).copied()
        })
    }
    fn remove_item(self, s: usize, takeoff: bool, i: usize) {
        self.with(|a| {
            if let Some(slot) = a.slots.get_mut(s) {
                let q = if takeoff { &mut slot.takeoff } else { &mut slot.landing };
                if i < q.len() {
                    q.remove(i);
                }
            }
        })
    }
    fn set_item_unit(self, s: usize, takeoff: bool, i: usize, unit: Unit) {
        self.with(|a| {
            let slot = &mut a.slots[s];
            let q = if takeoff { &mut slot.takeoff } else { &mut slot.landing };
            q[i].unit = unit;
        })
    }
}

// ---- the bodies

/// The destructor (0x54e8d3): every queued plane and every airport dereferenced, the airports dropped.
pub fn destroy<H: AtcHost + ?Sized>(p: AtcRef, h: &mut H) {
    let mut s = 0;
    while s < p.slot_count() {
        for takeoff in [true, false] {
            let mut i = 0;
            while let Some(item) = p.item(s, takeoff, i) {
                h.dereference(item.unit);
                i += 1;
            }
        }
        p.with(|a| {
            a.slots[s].landing.clear();
            a.slots[s].takeoff.clear();
        });
        let airport = p.airport(s);
        h.dereference(airport);
        s += 1;
    }
    p.with(|a| a.slots.clear());
}

/// `AddCargoWeight` (0x54e9a8).
pub fn add_cargo_weight<H: AtcHost + ?Sized>(p: AtcRef, h: &mut H, unit: Unit) {
    let w = h.weight(unit);
    p.with(|a| a.used_cargo = a.used_cargo.wrapping_add(w));
}

/// `RemoveCargoWeight` (0x54fc39).
pub fn remove_cargo_weight<H: AtcHost + ?Sized>(p: AtcRef, h: &mut H, unit: Unit) {
    let w = h.weight(unit);
    p.with(|a| a.used_cargo = a.used_cargo.wrapping_sub(w));
}

/// `AddAirport` (0x54e9bb): a slot with empty queues, the airport referenced, its capacity added.
pub fn add_airport<H: AtcHost + ?Sized>(p: AtcRef, h: &mut H, airport: Unit) {
    h.reference(airport);
    p.with(|a| a.slots.push(Slot::new(airport)));
    let capacity = h.get(EE_UNITS_ATC_TRANSPORT_CAPACITY, airport) as c_long;
    p.with(|a| a.max_cargo = a.max_cargo.wrapping_add(capacity));
}

/// `GetRoom` (0x54eb2c): the airport's free room, a landing plane not yet inside and a departing one still inside
/// counted.
pub fn room<H: AtcHost + ?Sized>(p: AtcRef, h: &mut H, s: usize) -> c_long {
    let airport = p.airport(s);
    let mut free = h.get(EE_UNITS_ATC_CARGO_ROOM, airport) as c_long;
    let mut i = 0;
    while let Some(item) = p.item(s, false, i) {
        if h.get2(EE_UNITS_ATC_IN_CARGO, airport, item.unit) == 0 {
            free = free.wrapping_sub(h.weight(item.unit));
        }
        i += 1;
    }
    let mut i = 0;
    while let Some(item) = p.item(s, true, i) {
        if h.get2(EE_UNITS_ATC_IN_CARGO, airport, item.unit) != 0 {
            free = free.wrapping_add(h.weight(item.unit));
        }
        i += 1;
    }
    free
}

/// `RemoveFromEntry` (0x54eed0): the plane (every plane when null) out of slot `s`'s queues, its clearance revoked.
pub fn remove_from_entry<H: AtcHost + ?Sized>(p: AtcRef, h: &mut H, s: usize, unit: Unit) {
    for takeoff in [false, true] {
        let mut i = 0;
        while let Some(item) = p.item(s, takeoff, i) {
            if unit.is_null() || item.unit == unit {
                h.dereference(item.unit);
                h.set_clearance(item.unit, false);
                p.remove_item(s, takeoff, i);
            } else {
                i += 1;
            }
        }
    }
}

/// `DropDeadPlanes` (0x54f033).
pub fn drop_dead_planes<H: AtcHost + ?Sized>(p: AtcRef, h: &mut H, s: usize) {
    for takeoff in [false, true] {
        let mut i = 0;
        while let Some(item) = p.item(s, takeoff, i) {
            if h.hit_points(item.unit) <= 0 {
                h.dereference(item.unit);
                p.remove_item(s, takeoff, i);
            } else {
                i += 1;
            }
        }
    }
}

/// `QueueRequest` (0x54ea34): 0 queued, 7 no landing room, 3 an unknown airport or the used cargo over the maximum
/// (any kind but 0).
pub fn queue_request<H: AtcHost + ?Sized>(p: AtcRef, h: &mut H, airport: Unit, unit: Unit, kind: c_long) -> c_long {
    let over = p.with(|a| !(a.used_cargo <= a.max_cargo));
    if over && kind != 0 {
        return 3;
    }
    let Some(s) = p.find(airport) else {
        return 3;
    };
    remove_from_entry(p, h, s, unit);
    let fits = if kind == 1 {
        let weight = h.weight(unit);
        room(p, h, s).wrapping_sub(weight) >= 0
    } else {
        true
    };
    if !fits {
        return 7;
    }
    let item = Item { unit, kind: kind as c_ulong };
    p.with(|a| {
        if kind == 1 {
            a.slots[s].landing.push_back(item);
        } else {
            a.slots[s].takeoff.push_back(item);
        }
    });
    h.set_clearance(unit, false);
    h.reference(unit);
    0
}

/// `GetFreeCargoWeight` (0x54eaf7): the airport's room, -1 when it is not listed.
pub fn free_cargo_weight<H: AtcHost + ?Sized>(p: AtcRef, h: &mut H, airport: Unit) -> c_long {
    match p.find(airport) {
        Some(s) => room(p, h, s),
        None => -1,
    }
}

/// `RemoveFromTakeoffQueues` (0x54ec8c): the plane out of the first takeoff queue holding it, its clearance revoked.
pub fn remove_from_takeoff_queues<H: AtcHost + ?Sized>(p: AtcRef, h: &mut H, unit: Unit) -> bool {
    let mut s = 0;
    while s < p.slot_count() {
        let mut i = 0;
        while let Some(item) = p.item(s, true, i) {
            if item.unit == unit {
                h.set_clearance(unit, false);
                h.dereference(unit);
                p.remove_item(s, true, i);
                return true;
            }
            i += 1;
        }
        s += 1;
    }
    false
}

/// `ClearAirportQueue` (0x54ed30): the airport's idle departing planes of a kind (0 all, 1 fighters, 2 bombers,
/// 3 atomic bombers) out of its takeoff queue, its exiting unit cleared when it is one of them.
pub fn clear_airport_queue<H: AtcHost + ?Sized>(p: AtcRef, h: &mut H, airport: Unit, kind: c_long) -> bool {
    let Some(s) = p.find(airport) else {
        return false;
    };
    let mut result = false;
    let mut matched = false;
    let mut i = 0;
    while let Some(item) = p.item(s, true, i) {
        match kind {
            0 => matched = true,
            1 => {
                let family = h.get(EE_UNITS_ATC_FAMILY, item.unit);
                matched = family == FAMILY_AIRCRAFT || h.get(EE_UNITS_ATC_FAMILY, item.unit) == FAMILY_SPACE_FIGHTER;
            }
            2 => matched = h.get(EE_UNITS_ATC_FAMILY, item.unit) == FAMILY_BOMBER,
            3 => matched = h.get(EE_UNITS_ATC_FAMILY, item.unit) == FAMILY_ATOMIC_BOMBER,
            _ => {}
        }
        if matched && h.get(EE_UNITS_ATC_COMMAND_TYPE, item.unit) == ACTION_IDLE {
            h.set_clearance(item.unit, false);
            h.dereference(item.unit);
            h.get2(EE_UNITS_ATC_RELEASE_EXITING, airport, item.unit);
            p.remove_item(s, true, i);
            matched = false;
            result = true;
        } else {
            i += 1;
        }
    }
    result
}

/// `Dump` (0x54f15d): the out-of-sync dump's lines.
pub fn dump<H: AtcHost + ?Sized>(p: AtcRef, h: &mut H) {
    let line = |h: &mut H, text: String| {
        let mut bytes = text.into_bytes();
        bytes.push(0);
        h.get(EE_UNITS_ATC_DUMP_LINE, bytes.as_mut_ptr() as Unit);
    };
    let (max, used) = p.with(|a| (a.max_cargo, a.used_cargo));
    line(h, format!("EEAirTrafficControl::DumpOOSInformation - The max cargo weight is {max}\n"));
    line(h, format!("EEAirTrafficControl::DumpOOSInformation - The used cargo weight is {used}\n"));
    let mut s = 0;
    while s < p.slot_count() {
        let id = h.unit_id(p.airport(s));
        line(h, format!("EEAirTrafficControl::DumpOOSInformation - The Airport {s} ID is #{id:08x}\n"));
        for (takeoff, name) in [(false, "Landing"), (true, "Takeoff")] {
            let mut i = 0;
            while let Some(item) = p.item(s, takeoff, i) {
                let id = h.unit_id(item.unit);
                let mut text = String::new();
                let _ = writeln!(
                    text,
                    "EEAirTrafficControl::DumpOOSInformation - The {name} Request {i} has a unit ID of #{id:08x} with a request of {}",
                    item.kind as c_long
                );
                line(h, text);
                i += 1;
            }
        }
        s += 1;
    }
}

/// `GetCandidates` (0x54f32f): the other airports with free room, nearest (by squared tile distance) first.
pub fn candidates<H: AtcHost + ?Sized>(p: AtcRef, h: &mut H, airport: Unit) -> Vec<Candidate> {
    let mut out = Vec::new();
    let mut s = 0;
    while s < p.slot_count() {
        let free = room(p, h, s);
        let other = p.airport(s);
        if !(free <= 0 || airport == other) {
            out.push(Candidate { airport: other, distance: tile_distance_squared(h, airport, other), free });
        }
        s += 1;
    }
    out.sort_by(|x, y| {
        if x.distance < y.distance {
            std::cmp::Ordering::Less
        } else if y.distance < x.distance {
            std::cmp::Ordering::Greater
        } else {
            std::cmp::Ordering::Equal
        }
    });
    out
}

/// `EEUnit::GetTileDistanceSquared` (0x54f455): `(float)(dx * dx + dy * dy)` over the tiles.
pub fn tile_distance_squared<H: AtcHost + ?Sized>(h: &mut H, unit: Unit, other: Unit) -> f32 {
    let mut a = EeUnitsAtcIo::default();
    h.call(EE_UNITS_ATC_TILE, unit, std::ptr::null_mut(), &mut a);
    let mut b = EeUnitsAtcIo::default();
    h.call(EE_UNITS_ATC_TILE, other, std::ptr::null_mut(), &mut b);
    let dy = (a.b as c_long).wrapping_sub(b.b as c_long);
    let dx = (a.a as c_long).wrapping_sub(b.a as c_long);
    dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy)) as f32
}

/// `GetChecksum` (0x54f47d): the totals, the airports' ids and each queued plane's id plus its request.
pub fn checksum<H: AtcHost + ?Sized>(p: AtcRef, h: &mut H) -> c_long {
    let mut sum = p.with(|a| a.used_cargo.wrapping_add(a.max_cargo));
    let mut s = 0;
    while s < p.slot_count() {
        sum = sum.wrapping_add(h.unit_id(p.airport(s)) as c_long);
        for takeoff in [false, true] {
            let mut i = 0;
            while let Some(item) = p.item(s, takeoff, i) {
                sum = sum.wrapping_add(h.unit_id(item.unit).wrapping_add(item.kind) as c_long);
                i += 1;
            }
        }
        s += 1;
    }
    sum
}

/// `GetQueueLength` (0x54f5fa): the airport's takeoff queue's length.
pub fn queue_length(p: AtcRef, airport: Unit) -> c_ulong {
    match p.find(airport) {
        Some(s) => p.with(|a| a.slots[s].takeoff.len() as c_ulong),
        None => 0,
    }
}

/// `GetQueuePosition` (0x54f61c): the plane's place in its home airport's takeoff (0) or landing queue, or -1.
pub fn queue_position<H: AtcHost + ?Sized>(p: AtcRef, h: &mut H, unit: Unit, which: c_long) -> c_long {
    let home = h.get(EE_UNITS_ATC_HOME_BASE, unit) as usize as Unit;
    p.with(|a| {
        for slot in &a.slots {
            if slot.airport != home {
                continue;
            }
            let q = if which == 0 { &slot.takeoff } else { &slot.landing };
            if let Some(i) = q.iter().position(|item| item.unit == unit) {
                return i as c_long;
            }
        }
        -1
    })
}

/// `IsTakeOffDone` (0x54f6e9): outside the airport and, for air traffic type 1, at 5% of the cruise altitude above
/// it, else more than sqrt(6) away.
#[allow(clippy::neg_cmp_op_on_partial_ord)] // the original's: a NaN height or distance counts as done
pub fn is_takeoff_done<H: AtcHost + ?Sized>(p: AtcRef, h: &mut H, s: usize, unit: Unit) -> bool {
    let airport = p.airport(s);
    let away = if h.get(EE_UNITS_ATC_AIR_TRAFFIC_TYPE, unit) == 1 {
        let z = h.location(unit).m[2][3];
        let cruise = h.cruise_altitude(unit);
        let base = h.location(airport).m[2][3];
        !(z < cruise * 0.05f32 + base)
    } else {
        let at = h.location(airport);
        let plane = h.location(unit);
        !(at.distance_squared_to(&plane) <= 6.0f32)
    };
    away && !h.in_container(unit)
}

/// `ReplaceUnits` (0x54f88d): each replaced aircraft's weight and queue entries to its replacement; a replacement
/// that is no aircraft leaves every queue.
pub fn replace_units<H: AtcHost + ?Sized>(p: AtcRef, h: &mut H, swaps: &[(Unit, Unit)]) {
    for &(old, new) in swaps {
        let old_aircraft = h.get(EE_UNITS_ATC_IS_AIRCRAFT, old) != 0;
        let new_aircraft = h.get(EE_UNITS_ATC_IS_AIRCRAFT, new) != 0;
        if old_aircraft {
            remove_cargo_weight(p, h, old);
        }
        if new_aircraft {
            add_cargo_weight(p, h, new);
        }
        if !old_aircraft {
            continue;
        }
        let mut s = 0;
        while s < p.slot_count() {
            if !new_aircraft {
                remove_from_entry(p, h, s, old);
                s += 1;
                continue;
            }
            for takeoff in [false, true] {
                let mut i = 0;
                while let Some(item) = p.item(s, takeoff, i) {
                    if item.unit == old {
                        h.dereference(item.unit);
                        p.set_item_unit(s, takeoff, i, new);
                        h.reference(new);
                    }
                    i += 1;
                }
            }
            s += 1;
        }
    }
}

/// The first candidate with `needed` room.
fn first_with_room(candidates: &[Candidate], needed: c_long) -> Option<Unit> {
    candidates.iter().find(|c| c.free >= needed).map(|c| c.airport)
}

/// `SendPlanesAway` (0x54fa2b): for a lost airport, its departing planes still low outside it die; each landing one
/// goes to the nearest airport with room, or dies.
pub fn send_planes_away<H: AtcHost + ?Sized>(p: AtcRef, h: &mut H, s: usize) {
    let airport = p.airport(s);
    let found = candidates(p, h, airport);
    let mut i = 0;
    while let Some(item) = p.item(s, true, i) {
        let z = h.location(item.unit).m[2][3];
        let cruise = h.cruise_altitude(item.unit);
        if z < cruise * 0.35f32 && !h.in_container(item.unit) && h.hit_points(item.unit) > 0 {
            h.kill(item.unit);
        }
        i += 1;
    }
    let mut i = 0;
    while let Some(item) = p.item(s, false, i) {
        i += 1;
        if !(h.hit_points(item.unit) > 0) {
            continue;
        }
        let needed = h.weight(item.unit);
        match first_with_room(&found, needed) {
            Some(target) => {
                h.get2(EE_UNITS_ATC_REROUTE, item.unit, target);
            }
            None => h.kill(item.unit),
        }
    }
}

/// The lost airport's removal (`RemoveAirport`'s and `Update`'s): its dead planes dropped, its capacity taken off,
/// its planes sent away and released, the airport dereferenced and its slot erased.
fn drop_airport<H: AtcHost + ?Sized>(p: AtcRef, h: &mut H, s: usize) {
    let airport = p.airport(s);
    drop_dead_planes(p, h, s);
    let capacity = h.get(EE_UNITS_ATC_TRANSPORT_CAPACITY, airport) as c_long;
    p.with(|a| a.max_cargo = a.max_cargo.wrapping_sub(capacity));
    send_planes_away(p, h, s);
    remove_from_entry(p, h, s, std::ptr::null_mut());
    h.dereference(airport);
    p.with(|a| {
        if s < a.slots.len() {
            a.slots.remove(s);
        }
    });
}

/// `RemoveAirport` (0x54fc4c): every slot of the airport dropped.
pub fn remove_airport<H: AtcHost + ?Sized>(p: AtcRef, h: &mut H, airport: Unit) {
    let mut s = 0;
    while s < p.slot_count() {
        if p.airport(s) == airport {
            drop_airport(p, h, s);
        } else {
            s += 1;
        }
    }
}

/// `RemovePlane` (0x54fcc1): the plane out of the given airport's queues.
pub fn remove_plane<H: AtcHost + ?Sized>(p: AtcRef, h: &mut H, airport: Unit, unit: Unit) {
    if let Some(s) = p.find(airport) {
        remove_from_entry(p, h, s, unit);
    }
}

/// `RepairPointers` (0x54fcf4): the loaded ids back to units (each referenced), the planes and airports that no
/// longer exist dropped.
pub fn repair_pointers<H: AtcHost + ?Sized>(p: AtcRef, h: &mut H) -> c_long {
    let resolve = |h: &mut H, id: Unit| -> Unit {
        let mut io = EeUnitsAtcIo { a: id as usize as u32 as i64, ..Default::default() };
        h.call(EE_UNITS_ATC_RESOLVE, std::ptr::null_mut(), std::ptr::null_mut(), &mut io) as usize as Unit
    };
    let mut s = 0;
    while s < p.slot_count() {
        let airport = resolve(h, p.airport(s));
        if airport.is_null() {
            p.with(|a| {
                a.slots.remove(s);
            });
            continue;
        }
        p.with(|a| a.slots[s].airport = airport);
        h.reference(airport);
        for takeoff in [false, true] {
            let mut i = 0;
            while let Some(item) = p.item(s, takeoff, i) {
                let unit = resolve(h, item.unit);
                if unit.is_null() {
                    p.remove_item(s, takeoff, i);
                } else {
                    p.set_item_unit(s, takeoff, i, unit);
                    h.reference(unit);
                    i += 1;
                }
            }
        }
        s += 1;
    }
    0
}

/// `FindNearestAirport` (0x55014c): the nearest other airport with room for the plane, or null (also with the used
/// cargo over the maximum).
pub fn find_nearest_airport<H: AtcHost + ?Sized>(p: AtcRef, h: &mut H, unit: Unit) -> Unit {
    let needed = h.weight(unit);
    if !p.with(|a| a.used_cargo <= a.max_cargo) {
        return std::ptr::null_mut();
    }
    let found = candidates(p, h, unit);
    first_with_room(&found, needed).unwrap_or(std::ptr::null_mut())
}

/// `Update` (0x5501d4): each airport's landings, then its takeoffs; a destroyed airport dropped.
pub fn update<H: AtcHost + ?Sized>(p: AtcRef, h: &mut H) {
    let mut s = 0;
    while s < p.slot_count() {
        let airport = p.airport(s);
        if h.hit_points(airport) > 0 {
            // UpdateLandings' flag: the original's always-false BlocksTakeoffs never sets it
            update_landings(p, h, s);
            update_takeoffs(p, h, s);
            s += 1;
        } else {
            drop_airport(p, h, s);
        }
    }
}

/// `UpdateLandings` (0x550277): the landing planes cleared in queue order, one the overfull airport cannot take
/// redirected or destroyed; the first landed or dead plane dropped, and there it stops.
pub fn update_landings<H: AtcHost + ?Sized>(p: AtcRef, h: &mut H, s: usize) -> bool {
    let mut done = false;
    let mut i = 0;
    while !done {
        let Some(item) = p.item(s, false, i) else {
            break;
        };
        let unit = item.unit;
        let weight = h.weight(unit);
        if h.hit_points(unit) > 0 {
            if h.get(EE_UNITS_ATC_CLEARANCE, unit) == 0 {
                h.set_clearance(unit, true);
            }
            done = h.in_container(unit);
            if !done {
                if room(p, h, s) < weight.wrapping_neg() {
                    let found = candidates(p, h, p.airport(s));
                    match first_with_room(&found, weight) {
                        Some(target) => {
                            if queue_request(p, h, target, unit, 1) == 0 {
                                h.get2(EE_UNITS_ATC_REROUTE, unit, target);
                            } else {
                                h.kill(unit);
                            }
                        }
                        None => h.kill(unit),
                    }
                }
                i += 1;
                continue;
            }
        } else {
            done = true;
        }
        h.dereference(unit);
        p.remove_item(s, false, i);
    }
    done
}

/// `UpdateTakeoffs` (0x550487): the departing planes cleared in queue order, each that left or died dropped; false
/// at the first still leaving.
pub fn update_takeoffs<H: AtcHost + ?Sized>(p: AtcRef, h: &mut H, s: usize) -> bool {
    let mut result = false;
    while let Some(item) = p.item(s, true, 0) {
        let unit = item.unit;
        if h.hit_points(unit) > 0 {
            if h.get(EE_UNITS_ATC_CLEARANCE, unit) == 0 {
                h.set_clearance(unit, true);
            }
            result = is_takeoff_done(p, h, s, unit);
        } else {
            result = true;
        }
        if !result {
            break;
        }
        h.dereference(unit);
        h.set_clearance(unit, false);
        p.remove_item(s, true, 0);
    }
    result
}

// ---- FFI

struct FfiHost {
    ctx: *mut c_void,
    f: EeUnitsAtcFn,
}

impl AtcHost for FfiHost {
    fn call(&mut self, op: i32, unit: Unit, other: Unit, io: &mut EeUnitsAtcIo) -> i64 {
        // SAFETY: the C++ callback with its own context; io lives for the call
        unsafe { (self.f)(self.ctx, op, unit, other, io) }
    }
}

/// A new, empty air traffic control.
#[no_mangle]
pub extern "C" fn ee_units_atc_new() -> *mut EeUnitsAtc {
    Box::into_raw(Box::default())
}

/// The destructor's body over the host, then the state freed. Null does nothing.
///
/// # Safety
/// `p` came from [`ee_units_atc_new`] and is not used again; `f` is valid with `ctx`.
#[no_mangle]
pub unsafe extern "C" fn ee_units_atc_free(p: *mut EeUnitsAtc, ctx: *mut c_void, f: EeUnitsAtcFn) {
    if p.is_null() {
        return;
    }
    destroy(AtcRef(p), &mut FfiHost { ctx, f });
    // SAFETY: the box made by ee_units_atc_new
    drop(unsafe { Box::from_raw(p) });
}

/// The maximum (0) or the used (1) air cargo.
///
/// # Safety
/// `p` is a live state.
#[no_mangle]
pub unsafe extern "C" fn ee_units_atc_cargo(p: *const EeUnitsAtc, which: i32) -> c_long {
    // SAFETY: the contract
    let a = unsafe { &*p };
    if which == 0 {
        a.max_cargo
    } else {
        a.used_cargo
    }
}

/// The number of airports.
///
/// # Safety
/// `p` is a live state.
#[no_mangle]
pub unsafe extern "C" fn ee_units_atc_slot_count(p: *const EeUnitsAtc) -> c_ulong {
    // SAFETY: the contract
    let a = unsafe { &*p };
    a.slots.len() as c_ulong
}

/// Airport `index` (null past the end).
///
/// # Safety
/// `p` is a live state.
#[no_mangle]
pub unsafe extern "C" fn ee_units_atc_slot_airport(p: *const EeUnitsAtc, index: c_ulong) -> *mut c_void {
    // SAFETY: the contract
    let a = unsafe { &*p };
    a.slots.get(index as usize).map_or(std::ptr::null_mut(), |s| s.airport)
}

/// The class's methods by kind (the `EE_UNITS_ATC_CALL_*` values): `unit`, `other` and `value` their arguments.
///
/// # Safety
/// `p` is a live state; `f` is valid with `ctx`.
#[no_mangle]
pub unsafe extern "C" fn ee_units_atc_call(
    p: *mut EeUnitsAtc,
    ctx: *mut c_void,
    f: EeUnitsAtcFn,
    call: i32,
    unit: *mut c_void,
    other: *mut c_void,
    value: c_long,
) -> i64 {
    let p = AtcRef(p);
    let h = &mut FfiHost { ctx, f };
    match call {
        EE_UNITS_ATC_CALL_ADD_CARGO_WEIGHT => add_cargo_weight(p, h, unit),
        EE_UNITS_ATC_CALL_REMOVE_CARGO_WEIGHT => remove_cargo_weight(p, h, unit),
        EE_UNITS_ATC_CALL_ADD_AIRPORT => add_airport(p, h, unit),
        EE_UNITS_ATC_CALL_QUEUE_REQUEST => return queue_request(p, h, unit, other, value) as i64,
        EE_UNITS_ATC_CALL_FREE_CARGO_WEIGHT => return free_cargo_weight(p, h, unit) as i64,
        EE_UNITS_ATC_CALL_REMOVE_FROM_TAKEOFF_QUEUES => return remove_from_takeoff_queues(p, h, unit) as i64,
        EE_UNITS_ATC_CALL_CLEAR_AIRPORT_QUEUE => return clear_airport_queue(p, h, unit, value) as i64,
        EE_UNITS_ATC_CALL_DUMP => dump(p, h),
        EE_UNITS_ATC_CALL_CHECKSUM => return checksum(p, h) as i64,
        EE_UNITS_ATC_CALL_QUEUE_LENGTH => return queue_length(p, unit) as i64,
        EE_UNITS_ATC_CALL_QUEUE_POSITION => return queue_position(p, h, unit, value) as i64,
        EE_UNITS_ATC_CALL_REMOVE_AIRPORT => remove_airport(p, h, unit),
        EE_UNITS_ATC_CALL_REMOVE_PLANE => remove_plane(p, h, unit, other),
        EE_UNITS_ATC_CALL_REPAIR_POINTERS => return repair_pointers(p, h) as i64,
        EE_UNITS_ATC_CALL_FIND_NEAREST_AIRPORT => return find_nearest_airport(p, h, unit) as usize as i64,
        EE_UNITS_ATC_CALL_UPDATE => update(p, h),
        _ => {}
    }
    0
}

/// `ReplaceUnits`: `count` (old, new) pairs at `olds` and `news`.
///
/// # Safety
/// `p` is a live state; `f` is valid with `ctx`; `olds` and `news` hold `count` units each.
#[no_mangle]
pub unsafe extern "C" fn ee_units_atc_replace_units(
    p: *mut EeUnitsAtc,
    ctx: *mut c_void,
    f: EeUnitsAtcFn,
    olds: *const *mut c_void,
    news: *const *mut c_void,
    count: usize,
) {
    let swaps: Vec<(Unit, Unit)> = if count == 0 {
        Vec::new()
    } else {
        // SAFETY: the contract
        unsafe { (0..count).map(|i| (*olds.add(i), *news.add(i))).collect() }
    };
    replace_units(AtcRef(p), &mut FfiHost { ctx, f }, &swaps);
}

// the methods (`ee_units_atc_call`'s `call`)
pub const EE_UNITS_ATC_CALL_ADD_CARGO_WEIGHT: i32 = 1;
pub const EE_UNITS_ATC_CALL_REMOVE_CARGO_WEIGHT: i32 = 2;
pub const EE_UNITS_ATC_CALL_ADD_AIRPORT: i32 = 3;
pub const EE_UNITS_ATC_CALL_QUEUE_REQUEST: i32 = 4;
pub const EE_UNITS_ATC_CALL_FREE_CARGO_WEIGHT: i32 = 5;
pub const EE_UNITS_ATC_CALL_REMOVE_FROM_TAKEOFF_QUEUES: i32 = 6;
pub const EE_UNITS_ATC_CALL_CLEAR_AIRPORT_QUEUE: i32 = 7;
pub const EE_UNITS_ATC_CALL_DUMP: i32 = 8;
pub const EE_UNITS_ATC_CALL_CHECKSUM: i32 = 9;
pub const EE_UNITS_ATC_CALL_QUEUE_LENGTH: i32 = 10;
pub const EE_UNITS_ATC_CALL_QUEUE_POSITION: i32 = 11;
pub const EE_UNITS_ATC_CALL_REMOVE_AIRPORT: i32 = 12;
pub const EE_UNITS_ATC_CALL_REMOVE_PLANE: i32 = 13;
pub const EE_UNITS_ATC_CALL_REPAIR_POINTERS: i32 = 14;
pub const EE_UNITS_ATC_CALL_FIND_NEAREST_AIRPORT: i32 = 15;
pub const EE_UNITS_ATC_CALL_UPDATE: i32 = 16;

#[cfg(test)]
#[path = "atc_tests.rs"]
mod tests;
