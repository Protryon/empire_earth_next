use super::*;
use crate::player::{EeUnitsPlayerFields, EeUnitsPlayerIo};
use crate::player_save::EeUnitsPlayerSaved;
use crate::player_stats::EeUnitsPlayerStats;
use crate::player_units::EeUnitsPlayerUnit;

/// The player's areas: towns and herds by address (towns 100.., herds 200..) with the units they hold or can take
/// and their tile counts; the units (1..) as the bookkeeping's records; the random draw's answer; the log of the
/// changes.
#[derive(Default)]
struct World {
    towns: Vec<i64>,
    herds: Vec<i64>,
    takes: Vec<(i64, i64)>,
    holds: Vec<(i64, i64)>,
    tiles: Vec<(i64, i64)>,
    contains: Vec<(i64, (i64, i64))>,
    units: Vec<EeUnitsPlayerUnit>,
    town_units: Vec<i64>,
    animals: Vec<i64>,
    holder: i64,
    random: i64,
    groups: Vec<(i64, i64)>,
    log: Vec<String>,
}

impl PlayerHost for World {
    fn call(&mut self, op: i32, io: &mut EeUnitsPlayerIo) -> i64 {
        let a = io.i;
        let list = |w: &World, k: i64| if k == 0 { w.towns.clone() } else { w.herds.clone() };
        match op {
            EE_UNITS_PLAYER_AREA_COUNT => list(self, a[0]).len() as i64,
            EE_UNITS_PLAYER_AREA_AT => list(self, a[0])[a[1] as usize],
            EE_UNITS_PLAYER_TOWN_UNIT => i64::from(self.town_units.contains(&a[0])),
            EE_UNITS_PLAYER_HERD_ANIMAL => i64::from(self.animals.contains(&a[0])),
            EE_UNITS_PLAYER_AREA_CAN_TAKE => i64::from(self.takes.contains(&(a[1], a[2]))),
            EE_UNITS_PLAYER_HERD_HAS => i64::from(self.holds.contains(&(a[0], a[1]))),
            EE_UNITS_PLAYER_AREA_TILES => self.tiles.iter().find(|t| t.0 == a[1]).map_or(1, |t| t.1),
            EE_UNITS_PLAYER_TOWN_CONTAINS => i64::from(self.contains.contains(&(a[0], (a[1], a[2])))),
            EE_UNITS_PLAYER_HOLDER => self.holder,
            EE_UNITS_PLAYER_RANDOM => {
                self.log.push(format!("random {} {}", a[0], a[1]));
                self.random
            }
            EE_UNITS_PLAYER_AREA_TILE | EE_UNITS_PLAYER_ASSIGN_TILE => {
                io.i[0] = a[0] * 10;
                io.i[1] = a[0] * 10 + 1;
                0
            }
            EE_UNITS_PLAYER_UNIT_INFO => {
                // SAFETY: the caller's record
                unsafe { *(a[1] as *mut EeUnitsPlayerUnit) = self.units[a[0] as usize - 1] };
                0
            }
            EE_UNITS_PLAYER_NEXT_UNIT => {
                let next = a[0] + 1;
                if next as usize <= self.units.len() {
                    next
                } else {
                    0
                }
            }
            EE_UNITS_PLAYER_TOWN_MERGE => {
                self.log.push(format!("merge {} {}", a[0], a[1]));
                self.towns.retain(|&t| t != a[1]);
                self.groups.iter().find(|g| g.0 == a[1]).map_or(0, |g| g.1)
            }
            EE_UNITS_PLAYER_TOWN_REPATROL => {
                // SAFETY: the caller's list
                let groups = unsafe { std::slice::from_raw_parts(a[1] as *const i64, a[2] as usize) };
                self.log.push(format!("repatrol {} {groups:?}", a[0]));
                0
            }
            EE_UNITS_PLAYER_AREA_DROP => {
                self.log.push(format!("drop {} {}", a[0], a[1]));
                if a[0] == 0 {
                    self.towns.remove(a[1] as usize);
                } else {
                    self.herds.remove(a[1] as usize);
                }
                0
            }
            _ => {
                self.log.push(format!("{op} {} {} {}", a[0], a[1], a[2]));
                1
            }
        }
    }
}

fn run<R>(w: &mut World, body: impl FnOnce(&mut Player<World>) -> R) -> R {
    let mut saved = EeUnitsPlayerSaved::default();
    let mut fields = EeUnitsPlayerFields::default();
    let stats = EeUnitsPlayerStats::default();
    let mut p = Player { h: w, saved: &mut saved, fields: &mut fields, stats: &stats };
    body(&mut p)
}

#[test]
fn a_town_building_joins_the_only_town_or_a_new_one() {
    let mut w = World { towns: vec![100, 101], town_units: vec![1, 2], takes: vec![(101, 1)], ..Default::default() };
    assert!(run(&mut w, |p| p.add_unit_to_town(1)));
    assert!(run(&mut w, |p| p.add_unit_to_town(2)));
    assert!(!run(&mut w, |p| p.add_unit_to_town(3)), "not a town building");
    assert_eq!(w.log, ["90 0 101 1", "91 0 2 0"]);
}

#[test]
fn of_several_towns_the_largest_merges_the_others() {
    let mut w = World {
        towns: vec![100, 101, 102],
        town_units: vec![1],
        takes: vec![(100, 1), (101, 1), (102, 1)],
        tiles: vec![(100, 5), (101, 9), (102, 5)],
        groups: vec![(100, 7)],
        ..Default::default()
    };
    assert!(run(&mut w, |p| p.add_unit_to_town(1)));
    assert_eq!(w.log, ["merge 101 100", "merge 101 102", "90 0 101 1", "repatrol 101 [7]"]);
    // a tie keeps the later of the list's walk (the first found from the back)
    let mut w = World {
        towns: vec![100, 101],
        town_units: vec![1],
        takes: vec![(100, 1), (101, 1)],
        tiles: vec![(100, 5), (101, 5)],
        ..Default::default()
    };
    run(&mut w, |p| p.add_unit_to_town(1));
    assert_eq!(w.log, ["merge 100 101", "90 0 100 1"]);
}

#[test]
fn an_animal_joins_a_herd_by_its_holder_or_at_random() {
    let mut w = World {
        herds: vec![200, 201, 202],
        animals: vec![1],
        takes: vec![(200, 1), (201, 1), (202, 1)],
        holder: 9,
        holds: vec![(201, 9)],
        ..Default::default()
    };
    assert!(run(&mut w, |p| p.add_unit_to_herd(1)));
    assert_eq!(w.log, ["90 1 201 1"]);
    w.log.clear();
    w.holds.clear();
    w.random = 0;
    run(&mut w, |p| p.add_unit_to_herd(1));
    assert_eq!(w.log, ["random 0 2", "90 1 202 1"], "the candidates from the back: index 0 is the last herd");
    // a herd that holds it already: nothing
    w.log.clear();
    w.holds = vec![(202, 1)];
    assert!(run(&mut w, |p| p.add_unit_to_herd(1)));
    assert!(w.log.is_empty());
    // none can take it: a new herd
    w.takes.clear();
    w.holds.clear();
    run(&mut w, |p| p.add_unit_to_herd(1));
    assert_eq!(w.log, ["91 1 1 0"]);
}

#[test]
fn leaving_units_drop_the_emptied_areas() {
    let mut w = World {
        herds: vec![200, 201],
        towns: vec![100, 101],
        animals: vec![1],
        town_units: vec![2],
        holds: vec![(200, 1), (201, 1)],
        tiles: vec![(200, 0), (201, 3), (100, 0)],
        contains: vec![(100, (20, 21)), (101, (20, 21))],
        ..Default::default()
    };
    run(&mut w, |p| {
        p.remove_unit_from_herd(1);
        p.remove_unit_from_town(2);
    });
    assert_eq!(w.log, ["95 1 201 1", "95 1 200 1", "drop 1 0", "95 0 101 2", "95 0 100 2", "drop 0 0"]);
}

#[test]
fn the_assignments_take_the_units_no_area_holds() {
    let unit = |complex: u64, land: bool| EeUnitsPlayerUnit {
        complex,
        land_building: land,
        container_category: -1,
        ..Default::default()
    };
    let mut w = World {
        units: vec![unit(1, true), unit(2, true), unit(3, false), unit(0, true)],
        towns: vec![100],
        contains: vec![(100, (10, 11))],
        town_units: vec![2],
        animals: vec![3],
        ..Default::default()
    };
    run(&mut w, |p| {
        p.assign_towns();
        p.assign_herds();
    });
    // unit 1's tile is in town 100; unit 2 joins a new town; every town gets its patrol; unit 3 a new herd
    assert_eq!(w.log, ["91 0 2 0", "102 100 0 0", "91 1 3 0"]);
}
