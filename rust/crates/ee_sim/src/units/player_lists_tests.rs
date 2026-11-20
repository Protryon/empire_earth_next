use super::*;
use crate::units::player_save::{
    EE_UNITS_PLAYER_SAVE_READ_LONG, EE_UNITS_PLAYER_SAVE_READ_ULONG, EE_UNITS_PLAYER_SAVE_WRITE_LONG,
    EE_UNITS_PLAYER_SAVE_WRITE_ULONG,
};
use std::collections::{BTreeMap, VecDeque};

/// Reference counts by unit, the calls in order, the world's units by id (id = unit / 0x10).
#[derive(Default)]
struct Units {
    counts: BTreeMap<u64, i64>,
    calls: Vec<(i32, u64)>,
    world: Vec<u64>,
}

impl ListsHost for Units {
    fn call(&mut self, op: i32, value: u64) -> u64 {
        self.calls.push((op, value));
        match op {
            EE_UNITS_LISTS_HOST_REFERENCE => *self.counts.entry(value).or_default() += 1,
            EE_UNITS_LISTS_HOST_DEREFERENCE => *self.counts.entry(value).or_default() -= 1,
            EE_UNITS_LISTS_HOST_UNIT_ID => return value / 0x10,
            EE_UNITS_LISTS_HOST_UNIT_BY_ID => {
                return self.world.iter().copied().find(|&u| u / 0x10 == value).unwrap_or(0)
            }
            _ => {}
        }
        0
    }
}

#[derive(Default)]
struct File {
    words: VecDeque<i64>,
    fail_read: Option<usize>,
    reads: usize,
}

impl PlayerSaveHost for File {
    fn call(&mut self, op: i32, _: u32, io: &mut EeUnitsPlayerSaveIo) -> i32 {
        match op {
            EE_UNITS_PLAYER_SAVE_WRITE_ULONG | EE_UNITS_PLAYER_SAVE_WRITE_LONG => self.words.push_back(io.i),
            EE_UNITS_PLAYER_SAVE_READ_ULONG | EE_UNITS_PLAYER_SAVE_READ_LONG => {
                self.reads += 1;
                if self.fail_read == Some(self.reads) {
                    return 3;
                }
                match self.words.pop_front() {
                    Some(v) => io.i = v,
                    None => return 2,
                }
            }
            _ => {}
        }
        0
    }
}

#[test]
fn the_unit_lists_add_once_with_a_reference_and_release_from_the_last() {
    let mut h = Units::default();
    let mut l = EeUnitsPlayerLists::default();
    assert!(l.add(&mut h, EE_UNITS_LISTS_STRANDED, 0x100));
    assert!(!l.add(&mut h, EE_UNITS_LISTS_STRANDED, 0x100));
    assert!(l.add(&mut h, EE_UNITS_LISTS_REPATROL, 0x200));
    assert!(l.add(&mut h, EE_UNITS_LISTS_REPATROL, 0x300));
    assert_eq!(h.counts[&0x100], 1);
    l.clear(&mut h, EE_UNITS_LISTS_REPATROL, true);
    assert!(l.lists[EE_UNITS_LISTS_REPATROL as usize].is_empty());
    assert_eq!(
        &h.calls[h.calls.len() - 2..],
        &[(EE_UNITS_LISTS_HOST_DEREFERENCE, 0x300), (EE_UNITS_LISTS_HOST_DEREFERENCE, 0x200)]
    );
    // the stranded list cleared without its references (KillStrandedUnits dereferences them itself)
    l.clear(&mut h, EE_UNITS_LISTS_STRANDED, false);
    assert_eq!(h.counts[&0x100], 1);
}

#[test]
fn the_id_and_media_lists_push_find_and_remove() {
    let mut h = Units::default();
    let mut l = EeUnitsPlayerLists::default();
    assert!(l.add(&mut h, EE_UNITS_LISTS_LOADED_IDS, 7));
    assert!(l.add(&mut h, EE_UNITS_LISTS_LOADED_IDS, 7));
    assert_eq!(l.lists[EE_UNITS_LISTS_LOADED_IDS as usize], vec![7, 7]);
    assert!(l.has(EE_UNITS_LISTS_LOADED_IDS, 7) && !l.has(EE_UNITS_LISTS_LOADED_IDS, 8));
    assert!(!l.add(&mut h, EE_UNITS_LISTS_MEDIA, 0));
    assert!(l.add(&mut h, EE_UNITS_LISTS_MEDIA, 0x50));
    assert!(l.add(&mut h, EE_UNITS_LISTS_MEDIA, 0x60));
    assert!(l.remove(EE_UNITS_LISTS_MEDIA, 0x50));
    assert!(!l.remove(EE_UNITS_LISTS_MEDIA, 0x50));
    assert_eq!(l.lists[EE_UNITS_LISTS_MEDIA as usize], vec![0x60]);
    assert!(!l.add(&mut h, 99, 1));
    assert!(h.calls.is_empty());
}

#[test]
fn the_off_map_records_find_the_last_remove_and_repair() {
    let mut h = Units::default();
    let mut l = EeUnitsPlayerLists::default();
    l.off_map_add(&mut h, 0, 1, 2, 3);
    assert!(l.off_map.is_empty());
    l.off_map_add(&mut h, 0x110, 4, 5, 600);
    l.off_map_add(&mut h, 0x120, 6, 7, 700);
    l.off_map_add(&mut h, 0x110, 8, 9, 800);
    assert_eq!(h.counts[&0x110], 2);
    let r = l.off_map_find(0x110).unwrap();
    assert_eq!((r.mLocation, r.mTime), ([8, 9], 800));
    r.mReason = 5;
    assert!(l.off_map_find(0).is_none() && l.off_map_find(0x130).is_none());
    l.off_map_remove(&mut h, 0x110);
    assert_eq!(h.counts[&0x110], 1);
    assert_eq!(l.off_map.len(), 2);
    assert_eq!(l.off_map_find(0x110).unwrap().mTime, 600);
    // the repair: the ids back to units, the gone one dropped
    for r in l.off_map.iter_mut() {
        r.mUnit = (r.mUnit as u64 / 0x10) as *mut c_void;
    }
    h.world = vec![0x120];
    l.off_map_repair(&mut h);
    assert_eq!(l.off_map.len(), 1);
    assert_eq!(l.off_map[0].mUnit as u64, 0x120);
    assert_eq!(h.counts[&0x120], 2);
    l.off_map_clear(&mut h);
    assert!(l.off_map.is_empty());
    assert_eq!(h.counts[&0x120], 1);
}

#[test]
fn an_off_map_record_saves_and_loads_its_words() {
    let mut h = Units::default();
    let r = EeUnitsOffMapInfo { mLocation: [12, -3], mUnit: 0x450 as *mut c_void, mTime: 99, mReason: 4 };
    let mut f = File::default();
    assert_eq!(off_map_save(&r, &mut h, &mut f), 0);
    assert_eq!(f.words, [0x45, 12, -3, 99, 4]);
    let none = EeUnitsOffMapInfo::default();
    let mut g = File::default();
    off_map_save(&none, &mut h, &mut g);
    assert_eq!(g.words[0], c_ulong::MAX as i64);
    let mut back = EeUnitsOffMapInfo::default();
    assert_eq!(off_map_load(&mut back, &mut f), 0);
    assert_eq!((back.mUnit as u64, back.mLocation, back.mTime, back.mReason), (0x45, [12, -3], 99, 4));
    // a failed read: the id kept, the error returned, the rest untouched
    let mut bad = File { words: [7, 1, 2, 3, 4].into(), fail_read: Some(2), reads: 0 };
    let mut part = EeUnitsOffMapInfo::default();
    assert_eq!(off_map_load(&mut part, &mut bad), 3);
    assert_eq!((part.mUnit as u64, part.mLocation), (7, [0, 0]));
}
