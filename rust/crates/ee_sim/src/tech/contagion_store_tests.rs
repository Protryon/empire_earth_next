use super::*;
use crate::tech::contagion::EE_CONTAGION_SKIPS_SPREAD;
use std::collections::BTreeMap;

/// A fake world: units by handle, a log of the world calls, a byte file.
#[derive(Default)]
struct World {
    units: BTreeMap<u64, ContagionUnit>,
    refs: BTreeMap<u64, i32>,
    marked: BTreeMap<u64, bool>,
    log: Vec<String>,
    near: Vec<u64>,
    immune: Vec<u64>,
    random: f32,
    distance: f32,
    file: Vec<u8>,
    at: usize,
    text: String,
    active: Vec<u64>,
}

impl World {
    fn unit(&mut self, handle: u64, id: c_ulong, alive: bool, player: u64) {
        self.units.insert(handle, ContagionUnit { alive, complex: handle, id, player });
    }
}

impl ContagionWorld for World {
    fn unit(&mut self, unit: u64) -> ContagionUnit {
        self.units.get(&unit).copied().unwrap_or_default()
    }
    fn enemies(&mut self, player: u64, other: u64) -> bool {
        player != other
    }
    fn reference(&mut self, unit: u64, on: bool) {
        *self.refs.entry(unit).or_default() += if on { 1 } else { -1 };
    }
    fn mark(&mut self, unit: u64, _calamity: u64, on: bool) {
        self.marked.insert(unit, on);
    }
    fn infect(&mut self, _functor: u64, unit: u64, _calamity: u64) {
        self.log.push(format!("infect {unit}"));
    }
    fn cure(&mut self, _functor: u64, unit: u64, _calamity: u64) {
        self.log.push(format!("cure {unit}"));
    }
    fn apply(&mut self, _functor: u64, unit: u64, _calamity: u64, elapsed: f32) {
        self.log.push(format!("apply {unit} {elapsed}"));
    }
    fn functor_update(&mut self, _functor: u64, _calamity: u64, elapsed: f32) {
        self.log.push(format!("update {elapsed}"));
    }
    fn near(&mut self, _unit: u64, _radius: f32, out: &mut Vec<u64>) {
        out.extend_from_slice(&self.near);
    }
    fn can_infect(&mut self, _functor: u64, _unit: u64) -> bool {
        true
    }
    fn immune(&mut self, unit: u64) -> bool {
        self.immune.contains(&unit)
    }
    fn random(&mut self) -> f32 {
        self.random
    }
    fn distance_squared(&mut self, _unit: u64, _other: u64) -> f32 {
        self.distance
    }
    fn unit_by_id(&mut self, id: c_ulong) -> u64 {
        self.units.iter().find(|(_, u)| u.id == id).map_or(0, |(&h, _)| h)
    }
    fn calamity_checksum(&mut self, calamity: u64) -> c_long {
        calamity as c_long * 1000
    }
    fn calamity_index(&mut self, calamity: u64) -> Option<c_ulong> {
        self.active.iter().position(|&c| c == calamity).map(|i| i as c_ulong)
    }
    fn calamity_functor(&mut self, calamity: u64) -> u64 {
        calamity + 100
    }
    fn calamity_dump(&mut self, calamity: u64, _file: u64) {
        self.text += &format!("[calamity {calamity}]\n");
    }
    fn text(&mut self, _file: u64, text: &str) {
        self.text += text;
    }
    fn file(&mut self, _file: u64, op: u32, value: *mut c_void) -> c_long {
        // SAFETY: the ops' types (contagion_save.rs); c_long, c_ulong and f32 are 4 bytes here
        unsafe {
            match op {
                EE_CONTAGION_IO_READ_ULONG | EE_CONTAGION_IO_READ_LONG | EE_CONTAGION_IO_READ_FLOAT => {
                    if self.at + 4 > self.file.len() {
                        return 1;
                    }
                    let bytes: [u8; 4] = self.file[self.at..self.at + 4].try_into().unwrap();
                    self.at += 4;
                    match op {
                        EE_CONTAGION_IO_READ_FLOAT => *(value as *mut f32) = f32::from_le_bytes(bytes),
                        _ => *(value as *mut u32) = u32::from_le_bytes(bytes),
                    }
                }
                EE_CONTAGION_IO_WRITE_FLOAT => self.file.extend_from_slice(&(*(value as *const f32)).to_le_bytes()),
                _ => self.file.extend_from_slice(&(*(value as *const u32)).to_le_bytes()),
            }
        }
        0
    }
    fn capture(&mut self, kind: i32, a: i32, b: i32, _x: f32, _y: f32) {
        self.log.push(format!("event {kind} {a} {b}"));
    }
}

fn world() -> World {
    let mut w = World::default();
    w.unit(1, 0x11, true, 7);
    w.unit(2, 0x22, true, 7);
    w.unit(3, 0x33, false, 8);
    w.unit(4, 0x44, true, 8);
    w
}

#[test]
fn a_helper_holds_live_units_once_and_releases_them_with_a_cure() {
    let mut w = world();
    let mut m = EeContagionManager::default();
    let id = m.create(&mut w, 50, 9, 1);
    assert_eq!(id, 1);
    let h = m.at(0);
    assert!(h.add_unit(&mut w, 2, true));
    assert!(!h.add_unit(&mut w, 2, true), "held already");
    assert!(!h.add_unit(&mut w, 3, true), "dead");
    assert!(h.add_unit(&mut w, 4, false));
    assert_eq!(h.units, vec![1, 2, 4]);
    assert_eq!(w.refs[&1], 1);
    assert_eq!(w.log, ["infect 1", "infect 2"]);
    assert!(w.marked[&4]);
    w.log.clear();
    assert!(m.destroy(&mut w, id));
    assert!(m.helpers.is_empty());
    assert_eq!(w.log, ["cure 1", "cure 2", "cure 4"]);
    assert_eq!((w.refs[&1], w.refs[&2], w.refs[&4]), (0, 0, 0));
    assert!(!w.marked[&1]);
}

#[test]
fn ids_go_on_from_the_last_and_a_loaded_game_keeps_its_helpers() {
    let mut w = world();
    let mut m = EeContagionManager::default();
    assert_eq!((m.create(&mut w, 0, 9, 0), m.create(&mut w, 0, 9, 0)), (1, 2));
    m.calamity_indexes.push(0);
    assert!(m.destroy(&mut w, 1));
    assert_eq!(m.helpers.len(), 2, "nothing is destroyed while the loaded game awaits its pointers");
    m.calamity_indexes.clear();
    m.destroy(&mut w, 1);
    assert_eq!(m.find(1), None);
    assert!(m.find(2).is_some());
}

#[test]
fn swap_replaces_a_held_unit_without_a_cure() {
    let mut w = world();
    let mut h = EeContagionHelper::new(50, 9);
    h.add_unit(&mut w, 1, false);
    h.swap_unit(&mut w, 2, 4);
    assert_eq!(h.units, vec![1], "an unheld unit is not swapped");
    h.swap_unit(&mut w, 1, 4);
    assert_eq!(h.units, vec![4]);
    assert!(w.log.is_empty());
    assert_eq!((w.refs[&1], w.refs[&4]), (0, 1));
}

#[test]
fn the_checksums_add_the_unit_ids_the_calamity_the_id_and_the_timer() {
    let mut w = world();
    let mut m = EeContagionManager::default();
    m.create(&mut w, 50, 2, 1);
    m.at(0).add_unit(&mut w, 2, false);
    m.at(0).fields.mTimeSinceLastSpread = 1.5;
    let expected = 1 + (1.5f32.to_bits() as i32 as c_long) + 0x11 + 0x22 + 2000;
    assert_eq!(m.at(0).checksum(&mut w), expected);
    assert_eq!(m.checksum(&mut w), expected + 1);
}

#[test]
fn the_dump_writes_the_helpers_their_units_and_calamities() {
    let mut w = world();
    let mut m = EeContagionManager::default();
    m.create(&mut w, 50, 9, 2);
    m.dump(&mut w, 0);
    assert_eq!(
        w.text,
        "\nOUT OF SYNCH DUMP:  Contangion Manager Dump\n\nOUT OF SYNCH DUMP:  Contangion Manager Help list size 1\n\
         \nOUT OF SYNCH DUMP:  Contangion Helper id is 1\nOUT OF SYNCH DUMP:  Contangion Helper time since last spread: \
         0.000000\nOUT OF SYNCH DUMP:  Contangion Helper unit list size is 1\nOUT OF SYNCH DUMP:  Contangion Helper Unit \
         ID for index 0 is #00000022\n[calamity 9]\nOUT OF SYNCH DUMP:  EEContagionManager-Unique Number is 1\n"
    );
}

#[test]
fn save_load_and_restore_round_trip_the_helpers_on_their_calamities() {
    let mut w = world();
    w.active = vec![30, 40];
    let mut m = EeContagionManager::default();
    m.create(&mut w, 50, 40, 1);
    m.at(0).add_unit(&mut w, 2, false);
    m.at(0).fields.mTimeSinceLastUpdate = 2.5;
    m.create(&mut w, 50, 30, 0);
    assert_eq!(m.save(&mut w, 0, true), 0);
    let words: Vec<u32> = w.file.chunks(4).map(|c| u32::from_le_bytes(c.try_into().unwrap())).collect();
    assert_eq!(words[..7], [2, 1, 2, 0x11, 0x22, 1, 0], "count, the calamity index, the unit ids, the id, ...");

    let mut loaded = EeContagionManager::default();
    w.at = 0;
    assert_eq!(loaded.load(&mut w, 0, 0, 0x111, true), 0);
    assert_eq!((loaded.next_id, loaded.calamity_indexes.clone()), (2, vec![1, 0]));
    assert_eq!(loaded.at(0).unit_ids, vec![0x11, 0x22]);
    assert_eq!(loaded.at(0).fields.mTimeSinceLastUpdate, 2.5);
    assert!(loaded.at(0).units.is_empty());

    // unit 0x22 is gone; the second helper's calamity (index 0) too
    w.units.remove(&2);
    loaded.restore(&mut w, &[0, 40]);
    assert_eq!(loaded.helpers.len(), 1);
    let h = loaded.at(0);
    assert_eq!((h.units.clone(), h.calamity, h.functor), (vec![1], 40, 140));
    assert!(h.unit_ids.is_empty() && loaded.calamity_indexes.is_empty());
}

#[test]
fn a_save_fails_on_a_calamity_that_is_not_active() {
    let mut w = world();
    let mut m = EeContagionManager::default();
    m.create(&mut w, 50, 40, 0);
    assert_eq!(m.save(&mut w, 0, false), 9);
}

#[test]
fn an_old_helper_has_no_update_timer() {
    let mut w = world();
    let mut h = EeContagionHelper::new(50, 9);
    h.fields = EeContagionHelperFields { mHelperID: 5, mTimeSinceLastSpread: 1.0, mTimeSinceLastUpdate: 2.0 };
    h.save(&mut w, 0);
    let mut old = EeContagionHelper::default();
    w.at = 0;
    assert_eq!(old.load(&mut w, 0, 0, 0xe6,), 0);
    assert_eq!(
        old.fields,
        EeContagionHelperFields { mHelperID: 5, mTimeSinceLastSpread: 1.0, mTimeSinceLastUpdate: 0.0 }
    );
}

#[test]
fn an_update_applies_the_living_drops_the_dead_and_spreads_to_enemies_in_range() {
    let mut w = world();
    let mut h = EeContagionHelper::new(50, 9);
    h.add_unit(&mut w, 1, false);
    h.add_unit(&mut w, 2, false);
    w.units.get_mut(&2).unwrap().alive = false;
    w.log.clear();
    let skip = EeContagionParams { spread_time: 1000, flags: EE_CONTAGION_SKIPS_SPREAD, ..Default::default() };
    h.update(&mut w, &skip, 10.0);
    assert_eq!(w.log, ["event 10 0 0", "update 10", "event 5 17 0", "apply 1 10", "event 6 34 0", "cure 2"]);
    assert_eq!(h.units, vec![1]);
    assert_eq!((h.fields.mTimeSinceLastSpread, h.fields.mTimeSinceLastUpdate), (10.0, 10.0));

    // a spread: unit 4 (another player) near unit 1, taken; unit 2 immune
    w.log.clear();
    w.near = vec![4, 0];
    let spread = EeContagionParams { spread_time: 0, chance: 1000.0, radius: 2.0, flags: 0 };
    h.update(&mut w, &spread, 10.0);
    assert_eq!(h.units, vec![1, 4]);
    assert!(w.log.contains(&"event 4 68 1".to_string()) && w.log.contains(&"infect 4".to_string()));
    assert_eq!(h.fields.mTimeSinceLastUpdate, 0.0);
}
