//! atc.rs's tests: the bodies over a mock world of units (each a small number as its address).

use super::*;
use std::collections::BTreeMap;

#[derive(Clone, Debug, Default)]
struct U {
    weight: i64,
    capacity: i64,
    refs: i64,
    clearance: bool,
    hp: i64,
    cargo_room: i64,
    cargo: Vec<Unit>,
    family: i64,
    command: i64,
    exiting: Option<Unit>,
    tile: (i64, i64),
    loc: [f32; 3],
    traffic: i64,
    cruise: f32,
    container: bool,
    home: Unit,
    aircraft: bool,
    killed: bool,
    rerouted: Option<Unit>,
}

#[derive(Default)]
struct World {
    units: BTreeMap<usize, U>,
    /// the control a kill reenters (the plane's weight off the used cargo, as `EEComplexUnit::Kill` does)
    atc: Option<*mut EeUnitsAtc>,
    lines: Vec<String>,
    log: Vec<(i32, usize)>,
}

fn u(n: usize) -> Unit {
    n as Unit
}

impl World {
    fn unit(&mut self, unit: Unit) -> &mut U {
        self.units.get_mut(&(unit as usize)).expect("a mock unit")
    }
    fn add(&mut self, n: usize, x: U) {
        self.units.insert(n, x);
    }
}

impl AtcHost for World {
    fn call(&mut self, op: i32, unit: Unit, other: Unit, io: &mut EeUnitsAtcIo) -> i64 {
        if op != EE_UNITS_ATC_DUMP_LINE && op != EE_UNITS_ATC_RESOLVE {
            self.log.push((op, unit as usize));
        }
        match op {
            EE_UNITS_ATC_WEIGHT => self.unit(unit).weight,
            EE_UNITS_ATC_TRANSPORT_CAPACITY => self.unit(unit).capacity,
            EE_UNITS_ATC_REFERENCE => {
                self.unit(unit).refs += 1;
                0
            }
            EE_UNITS_ATC_DEREFERENCE => {
                self.unit(unit).refs -= 1;
                0
            }
            EE_UNITS_ATC_SET_CLEARANCE => {
                self.unit(unit).clearance = io.a != 0;
                0
            }
            EE_UNITS_ATC_CLEARANCE => self.unit(unit).clearance as i64,
            EE_UNITS_ATC_HIT_POINTS => self.unit(unit).hp,
            EE_UNITS_ATC_CARGO_ROOM => self.unit(unit).cargo_room,
            EE_UNITS_ATC_IN_CARGO => self.unit(unit).cargo.contains(&other) as i64,
            EE_UNITS_ATC_FAMILY => self.unit(unit).family,
            EE_UNITS_ATC_COMMAND_TYPE => self.unit(unit).command,
            EE_UNITS_ATC_RELEASE_EXITING => {
                if self.unit(unit).exiting == Some(other) {
                    self.unit(unit).exiting = None;
                }
                0
            }
            EE_UNITS_ATC_TILE => {
                let (x, y) = self.unit(unit).tile;
                io.a = x;
                io.b = y;
                0
            }
            EE_UNITS_ATC_LOCATION => {
                io.p = self.unit(unit).loc;
                0
            }
            EE_UNITS_ATC_AIR_TRAFFIC_TYPE => self.unit(unit).traffic,
            EE_UNITS_ATC_CRUISE_ALTITUDE => {
                io.f = self.unit(unit).cruise;
                0
            }
            EE_UNITS_ATC_IN_CONTAINER => self.unit(unit).container as i64,
            EE_UNITS_ATC_KILL => {
                self.unit(unit).killed = true;
                self.unit(unit).hp = 0;
                if let Some(atc) = self.atc {
                    remove_cargo_weight(AtcRef(atc), self, unit);
                }
                0
            }
            EE_UNITS_ATC_REROUTE => {
                self.unit(unit).home = other;
                self.unit(unit).rerouted = Some(other);
                0
            }
            EE_UNITS_ATC_IS_AIRCRAFT => self.unit(unit).aircraft as i64,
            EE_UNITS_ATC_HOME_BASE => self.unit(unit).home as usize as i64,
            EE_UNITS_ATC_UNIT_ID => unit as usize as i64 + 0x1000,
            EE_UNITS_ATC_RESOLVE => {
                let n = io.a as usize - 0x1000;
                if self.units.contains_key(&n) {
                    n as i64
                } else {
                    0
                }
            }
            EE_UNITS_ATC_DUMP_LINE => {
                // SAFETY: the NUL-terminated line dump passes
                let text = unsafe { std::ffi::CStr::from_ptr(unit as *const std::os::raw::c_char) };
                self.lines.push(text.to_string_lossy().into_owned());
                0
            }
            _ => panic!("op {op}"),
        }
    }
}

fn airport(capacity: i64, room: i64, tile: (i64, i64)) -> U {
    U { capacity, cargo_room: room, hp: 100, tile, ..Default::default() }
}

fn plane(weight: i64) -> U {
    U { weight, hp: 10, aircraft: true, cruise: 20.0, traffic: 1, ..Default::default() }
}

/// Airports 1 (room 4, at 0,0), 2 (room 10, at 3,4) and 3 (room 10, at 1,1) in that order; planes 10..14 of weight 2.
fn world() -> (World, Box<EeUnitsAtc>) {
    let mut w = World::default();
    w.add(1, airport(4, 4, (0, 0)));
    w.add(2, airport(10, 10, (3, 4)));
    w.add(3, airport(10, 10, (1, 1)));
    for n in 10..15 {
        w.add(n, plane(2));
    }
    let mut a = Box::<EeUnitsAtc>::default();
    let p = AtcRef(&mut *a);
    for n in 1..4 {
        add_airport(p, &mut w, u(n));
    }
    (w, a)
}

#[test]
fn airports_add_their_capacity_and_a_reference() {
    let (w, a) = world();
    assert_eq!(a.max_cargo, 24);
    assert_eq!(a.slots.len(), 3);
    assert_eq!(w.units[&1].refs, 1);
}

#[test]
fn queue_request_refuses_unknown_airports_full_landings_and_overused_cargo() {
    let (mut w, mut a) = world();
    let p = AtcRef(&mut *a);
    assert_eq!(queue_request(p, &mut w, u(9), u(10), 1), 3);
    assert_eq!(queue_request(p, &mut w, u(1), u(10), 1), 0);
    assert_eq!(queue_request(p, &mut w, u(1), u(11), 1), 0);
    // airport 1 has room 4: a third plane of weight 2 does not fit
    assert_eq!(queue_request(p, &mut w, u(1), u(12), 1), 7);
    assert_eq!(w.units[&10].refs, 1);
    assert_eq!(w.units[&12].refs, 0);
    // a requeue moves the plane: still one entry
    w.unit(u(10)).clearance = true;
    assert_eq!(queue_request(p, &mut w, u(1), u(10), 0), 0);
    assert!(!w.units[&10].clearance);
    assert_eq!(a.slots[0].landing.len(), 1);
    assert_eq!(a.slots[0].takeoff.len(), 1);
    assert_eq!(w.units[&10].refs, 1);
    // used over max: only kind 0 still queues
    a.used_cargo = 100;
    let p = AtcRef(&mut *a);
    assert_eq!(queue_request(p, &mut w, u(2), u(13), 1), 3);
    assert_eq!(queue_request(p, &mut w, u(2), u(13), 0), 0);
}

#[test]
fn room_counts_landings_outside_and_departures_inside() {
    let (mut w, mut a) = world();
    let p = AtcRef(&mut *a);
    queue_request(p, &mut w, u(2), u(10), 1);
    queue_request(p, &mut w, u(2), u(11), 1);
    w.unit(u(2)).cargo = vec![u(11), u(12)];
    queue_request(p, &mut w, u(2), u(12), 0);
    // 10 - 2 (plane 10 not inside) + 2 (plane 12 inside, leaving)
    assert_eq!(room(p, &mut w, 1), 10);
    assert_eq!(free_cargo_weight(p, &mut w, u(2)), 10);
    assert_eq!(free_cargo_weight(p, &mut w, u(7)), -1);
}

#[test]
fn candidates_are_the_other_airports_with_room_nearest_first() {
    let (mut w, mut a) = world();
    let p = AtcRef(&mut *a);
    let c = candidates(p, &mut w, u(1));
    assert_eq!(c.iter().map(|c| c.airport as usize).collect::<Vec<_>>(), vec![3, 2]);
    assert_eq!(c[0].distance, 2.0);
    assert_eq!(c[1].distance, 25.0);
    // equal distances keep the slots' order
    w.unit(u(3)).tile = (3, 4);
    let c = candidates(p, &mut w, u(1));
    assert_eq!(c.iter().map(|c| c.airport as usize).collect::<Vec<_>>(), vec![2, 3]);
    // no room: no candidate
    w.unit(u(2)).cargo_room = 0;
    assert_eq!(candidates(p, &mut w, u(1)).len(), 1);
    // plane 10 stands on airport 1's tile
    assert_eq!(find_nearest_airport(p, &mut w, u(10)), u(1));
    a.used_cargo = a.max_cargo + 1;
    assert!(find_nearest_airport(AtcRef(&mut *a), &mut w, u(10)).is_null());
}

#[test]
fn clear_airport_queue_drops_idle_planes_of_the_kind() {
    let (mut w, mut a) = world();
    let p = AtcRef(&mut *a);
    w.unit(u(10)).family = FAMILY_BOMBER;
    w.unit(u(10)).command = ACTION_IDLE;
    w.unit(u(11)).family = FAMILY_BOMBER;
    w.unit(u(11)).command = 5001;
    w.unit(u(12)).family = FAMILY_AIRCRAFT;
    w.unit(u(12)).command = ACTION_IDLE;
    w.unit(u(1)).exiting = Some(u(10));
    for n in 10..13 {
        queue_request(p, &mut w, u(1), u(n), 0);
    }
    assert!(clear_airport_queue(p, &mut w, u(1), 2));
    assert_eq!(a.slots[0].takeoff.iter().map(|i| i.unit as usize).collect::<Vec<_>>(), vec![11, 12]);
    assert_eq!(w.units[&1].exiting, None);
    assert_eq!(w.units[&10].refs, 0);
    assert!(!clear_airport_queue(AtcRef(&mut *a), &mut w, u(1), 3));
    assert!(clear_airport_queue(AtcRef(&mut *a), &mut w, u(1), 0));
    assert_eq!(a.slots[0].takeoff.len(), 1);
}

#[test]
fn queue_position_and_length_read_the_home_airport() {
    let (mut w, mut a) = world();
    let p = AtcRef(&mut *a);
    queue_request(p, &mut w, u(2), u(10), 0);
    queue_request(p, &mut w, u(2), u(11), 0);
    queue_request(p, &mut w, u(2), u(12), 1);
    w.unit(u(11)).home = u(2);
    w.unit(u(12)).home = u(2);
    assert_eq!(queue_position(p, &mut w, u(11), 0), 1);
    assert_eq!(queue_position(p, &mut w, u(11), 1), -1);
    assert_eq!(queue_position(p, &mut w, u(12), 1), 0);
    assert_eq!(queue_length(p, u(2)), 2);
    assert_eq!(queue_length(p, u(1)), 0);
    assert!(remove_from_takeoff_queues(p, &mut w, u(10)));
    assert!(!remove_from_takeoff_queues(p, &mut w, u(10)));
    remove_plane(p, &mut w, u(2), u(12));
    assert!(a.slots[1].landing.is_empty());
}

#[test]
fn takeoffs_clear_until_the_first_still_leaving() {
    let (mut w, mut a) = world();
    let p = AtcRef(&mut *a);
    for n in 10..13 {
        queue_request(p, &mut w, u(2), u(n), 0);
    }
    w.unit(u(10)).loc = [0.0, 0.0, 1.0]; // 1 >= 20 * 0.05 + 0: done
    w.unit(u(11)).hp = 0; // dead: dropped
    w.unit(u(12)).loc = [0.0, 0.0, 0.5]; // still climbing
    assert!(!update_takeoffs(p, &mut w, 1));
    assert_eq!(a.slots[1].takeoff.iter().map(|i| i.unit as usize).collect::<Vec<_>>(), vec![12]);
    assert!(w.units[&12].clearance);
    assert!(!w.units[&10].clearance);
    // type 2: by distance (sqrt 6) from the airport
    w.unit(u(12)).traffic = 2;
    w.unit(u(12)).loc = [2.0, 1.0, 1.0];
    assert!(!update_takeoffs(AtcRef(&mut *a), &mut w, 1));
    w.unit(u(12)).loc = [2.0, 1.0, 1.01];
    assert!(update_takeoffs(AtcRef(&mut *a), &mut w, 1));
}

#[test]
fn landings_stop_at_the_first_landed_and_reroute_an_overfull_airport() {
    let (mut w, mut a) = world();
    a.slots[0].landing.push_back(Item { unit: u(10), kind: 1 });
    a.slots[0].landing.push_back(Item { unit: u(11), kind: 1 });
    a.slots[0].landing.push_back(Item { unit: u(12), kind: 1 });
    w.unit(u(1)).cargo_room = -4; // overfull: -4 - 6 < -2
    w.unit(u(11)).container = true;
    let p = AtcRef(&mut *a);
    assert!(update_landings(p, &mut w, 0));
    // plane 10 went to the nearest airport with room (3), plane 11 landed and left the queue, 12 is untouched
    assert_eq!(w.units[&10].rerouted, Some(u(3)));
    assert_eq!(a.slots[2].landing.iter().map(|i| i.unit as usize).collect::<Vec<_>>(), vec![10]);
    assert_eq!(a.slots[0].landing.iter().map(|i| i.unit as usize).collect::<Vec<_>>(), vec![10, 12]);
    // the requeue at airport 3 revoked plane 10's clearance; plane 11 had it when it landed
    assert!(!w.units[&10].clearance);
    assert!(w.units[&11].clearance);
    assert!(!w.units[&12].clearance);
}

#[test]
fn a_lost_airport_kills_low_departures_and_sends_landings_away() {
    let (mut w, mut a) = world();
    let p = AtcRef(&mut *a);
    w.unit(u(12)).weight = 20; // no airport has room for it
    add_cargo_weight(p, &mut w, u(10));
    add_cargo_weight(p, &mut w, u(11));
    add_cargo_weight(p, &mut w, u(12));
    queue_request(p, &mut w, u(1), u(10), 0);
    queue_request(p, &mut w, u(1), u(11), 1);
    a.slots[0].landing.push_back(Item { unit: u(12), kind: 1 });
    w.unit(u(12)).refs += 1;
    w.unit(u(1)).hp = 0;
    let atc: *mut EeUnitsAtc = &mut *a;
    w.atc = Some(atc);
    update(AtcRef(atc), &mut w);
    assert!(w.units[&10].killed);
    assert_eq!(w.units[&11].rerouted, Some(u(3)));
    assert!(w.units[&12].killed);
    assert_eq!(a.slots.len(), 2);
    assert_eq!(a.max_cargo, 20);
    // the kills took their weight off (reentering the control)
    assert_eq!(a.used_cargo, 2);
    for n in [1, 10, 11, 12] {
        assert_eq!(w.units[&n].refs, 0, "unit {n}");
    }
}

#[test]
fn replaced_planes_hand_over_their_entries_and_weight() {
    let (mut w, mut a) = world();
    let p = AtcRef(&mut *a);
    add_cargo_weight(p, &mut w, u(10));
    queue_request(p, &mut w, u(1), u(10), 1);
    queue_request(p, &mut w, u(2), u(10), 0);
    w.unit(u(11)).weight = 3;
    replace_units(p, &mut w, &[(u(10), u(11))]);
    assert_eq!(a.used_cargo, 3);
    assert_eq!(a.slots[1].takeoff[0].unit, u(11));
    assert_eq!((w.units[&10].refs, w.units[&11].refs), (0, 2));
    // no aircraft any more: out of every queue
    w.unit(u(12)).aircraft = false;
    replace_units(AtcRef(&mut *a), &mut w, &[(u(11), u(12))]);
    assert!(a.slots.iter().all(|s| s.landing.is_empty() && s.takeoff.is_empty()));
    assert_eq!(a.used_cargo, 0);
}

#[test]
fn repair_turns_ids_into_units_and_drops_the_missing() {
    let mut w = World::default();
    w.add(1, airport(4, 4, (0, 0)));
    w.add(10, plane(2));
    let mut a = EeUnitsAtc::default();
    let mut s = Slot::new(u(0x1001));
    s.landing.push_back(Item { unit: u(0x1000 + 10), kind: 1 });
    s.landing.push_back(Item { unit: u(0x1099), kind: 1 });
    a.slots.push(s);
    a.slots.push(Slot::new(u(0x1077)));
    assert_eq!(repair_pointers(AtcRef(&mut a), &mut w), 0);
    assert_eq!(a.slots.len(), 1);
    assert_eq!(a.slots[0].airport, u(1));
    assert_eq!(a.slots[0].landing.iter().map(|i| i.unit as usize).collect::<Vec<_>>(), vec![10]);
    assert_eq!((w.units[&1].refs, w.units[&10].refs), (1, 1));
}

#[test]
fn the_checksum_and_the_dump_read_every_entry() {
    let (mut w, mut a) = world();
    let p = AtcRef(&mut *a);
    queue_request(p, &mut w, u(2), u(10), 1);
    queue_request(p, &mut w, u(2), u(11), 0);
    a.used_cargo = 3;
    let p = AtcRef(&mut *a);
    let ids = 0x1001 + 0x1002 + 0x1003 + 0x100a + 1 + 0x100b;
    assert_eq!(checksum(p, &mut w), 24 + 3 + ids);
    dump(p, &mut w);
    assert_eq!(w.lines.len(), 7);
    assert_eq!(w.lines[0], "EEAirTrafficControl::DumpOOSInformation - The max cargo weight is 24\n");
    assert_eq!(w.lines[3], "EEAirTrafficControl::DumpOOSInformation - The Airport 1 ID is #00001002\n");
    assert_eq!(
        w.lines[4],
        "EEAirTrafficControl::DumpOOSInformation - The Landing Request 0 has a unit ID of #0000100a with a request of 1\n"
    );
    assert_eq!(
        w.lines[5],
        "EEAirTrafficControl::DumpOOSInformation - The Takeoff Request 0 has a unit ID of #0000100b with a request of 0\n"
    );
}

#[test]
fn the_destructor_releases_every_plane_and_airport() {
    let (mut w, mut a) = world();
    let p = AtcRef(&mut *a);
    queue_request(p, &mut w, u(2), u(10), 1);
    queue_request(p, &mut w, u(2), u(11), 0);
    destroy(p, &mut w);
    assert!(a.slots.is_empty());
    assert!(w.units.values().all(|x| x.refs == 0));
}
