//! The resource centres' unit tests over the placement tests' fake world.

use super::file::test_file::{self, Bytes};
use super::file::OK;
use super::lists::CpCenter;
use super::place_host::Tile;
use super::placement::{CP_TYPE_GRANARY, CP_TYPE_HOUSE, CP_TYPE_SETTLEMENT};
use super::placement_tests::Fake;
use super::resource_center::*;

fn center(h: &mut Fake, units: &[u64], main: u64, active: bool) -> u64 {
    let ids = units.iter().map(|&u| h.units[u as usize - 1].id).collect();
    let main = if main == 0 { u32::MAX } else { h.units[main as usize - 1].id };
    h.centers.push((ids, main, active));
    0x1000 + h.centers.len() as u64 - 1
}

#[test]
fn get_center_of_one_unit_and_of_many() {
    let mut h = Fake::default();
    let mine = h.add(0x60, [10, 10], false);
    let c = center(&mut h, &[mine], 0, true);
    let mut out: Tile = [-5, -5];
    get_center(&mut h, c, &mut out);
    // the block ring's first free tile: the left side
    assert_eq!(out, [9, 10]);
    // nothing to measure: unchanged
    let empty = center(&mut h, &[], 0, true);
    let mut out: Tile = [-5, -5];
    get_center(&mut h, empty, &mut out);
    assert_eq!(out, [-5, -5]);
    // two units: the free tile nearest the middle of their box
    let a = h.add(0x60, [20, 20], false);
    let b = h.add(0x60, [24, 20], false);
    let c = center(&mut h, &[a, b], 0, true);
    let mut out: Tile = [0, 0];
    get_center(&mut h, c, &mut out);
    assert_eq!(out, [22, 20]);
    assert!(is_within_radius(&mut h, c, a) && contains(&mut h, c, b) && !contains(&mut h, c, mine));
}

#[test]
fn area_effects_and_radius() {
    let mut h = Fake::default();
    h.effects.insert((3, 6, 6));
    // a 2 x 2 footprint at (5, 5) covers (6, 6)
    assert!(is_in_area_effect(&mut h, [5, 5], 0x27, 3));
    assert!(!is_in_area_effect(&mut h, [6, 7], 0x27, 3));
    assert!(!is_in_area_effect(&mut h, [5, 5], 0x27, 0));
    // the smallest template radius (lrintf of 3.6)
    assert_eq!(min_effect_radius(&mut h, 0x27), 4);
}

#[test]
fn drop_site_adoption_and_resource_units() {
    let mut h = Fake::default();
    let gold = h.add(0x61, [10, 10], false);
    h.units[gold as usize - 1].resource_type = 5;
    let c = center(&mut h, &[gold], 0, true);
    let camp = h.add(0x30, [12, 12], true);
    h.units[camp as usize - 1].player = 0x77;
    h.units[camp as usize - 1].drop_site = 1;
    assert!(adopt_drop_site(&mut h, c, camp));
    // not a granary, not someone else's
    h.units[camp as usize - 1].unit_type = CP_TYPE_GRANARY;
    assert!(!adopt_drop_site(&mut h, c, camp));
    h.units[camp as usize - 1].unit_type = 0x30;
    h.units[camp as usize - 1].player = 0x78;
    assert!(!adopt_drop_site(&mut h, c, camp));
    assert_eq!(find_resource_unit(&mut h, c, 5), gold);
    assert_eq!(find_resource_unit(&mut h, c, 6), 0);
    h.units[gold as usize - 1].resource_amount = 0;
    assert_eq!(find_resource_unit(&mut h, c, 5), 0);
}

#[test]
fn morale_fill() {
    let mut h = Fake::default();
    let tc = h.add(0x23, [10, 10], true);
    let c = center(&mut h, &[], tc, true);
    // no effect: full
    assert_eq!(super::resource_center::morale_fill(&mut h, c), 1.0);
    h.morale = Some((1, 4, 3.0));
    // the level plus the units in the rectangle (the fake's filter takes all: the centre's own building), over 4
    assert_eq!(super::resource_center::morale_fill(&mut h, c), 0.5);
}

#[test]
fn settlement_site_in_the_rings() {
    let mut h = Fake::default();
    let tree = h.add(0x62, [15, 15], false);
    let c = center(&mut h, &[tree], 0, true);
    let mut out: Tile = [0, 0];
    assert!(find_build_site(&mut h, c, CP_TYPE_SETTLEMENT, &mut out));
    // ring 2 round the centre's tile (14, 15), with the margin free of the tree
    assert_eq!((out[0] - 14).abs().max((out[1] - 15).abs()), 2);
    let _ = CP_TYPE_HOUSE;
}

#[test]
fn save_then_load() {
    let mut h = Fake::default();
    let a = h.add(0x61, [1, 1], false);
    let b = h.add(0x61, [2, 2], false);
    h.units[b as usize - 1].valid = 0;
    let tc = h.add(0x23, [3, 3], true);
    let c = center(&mut h, &[a, b], tc, true);
    let mut bytes = Bytes::default();
    let r = save(&mut h, &mut test_file::file(&mut bytes), c);
    assert_eq!(r, OK);
    // the valid unit only, the main building, the flag
    let (ida, idtc) = (h.units[a as usize - 1].id, h.units[tc as usize - 1].id);
    let mut want = Bytes::default();
    want.u32(1).u32(ida).u32(idtc).b(true);
    assert_eq!(bytes.data, want.data);
    let mut back = CpCenter { units: vec![9], ..Default::default() };
    assert_eq!(load_into(&mut test_file::file(&mut bytes), CP_ACTIVE_FLAG_VERSION, &mut back), OK);
    assert_eq!(back, CpCenter { units: vec![ida], main: idtc, active: true });
    // before 0.141 no flag; a short file: the read's error
    let mut short = Bytes::default();
    short.u32(2).u32(5);
    let mut back = CpCenter::default();
    assert_eq!(load_into(&mut test_file::file(&mut short), 0x8c, &mut back), 0xa);
    assert_eq!(back.units, vec![5]);
}

#[test]
fn restore_drops_missing_and_duplicate_units() {
    let mut h = Fake::default();
    let a = h.add(0x61, [1, 1], false);
    let tc = h.add(0x23, [3, 3], true);
    let ida = h.units[a as usize - 1].id;
    h.centers.push((vec![ida, 999, ida], h.units[tc as usize - 1].id, true));
    assert_eq!(restore(&mut h, 0x1000), 0);
    assert_eq!(h.centers[0].0, vec![ida]);
    h.centers.push((vec![], 999, true));
    assert_eq!(restore(&mut h, 0x1001), 3);
}
