use super::*;
use crate::units::dm_body::{EE_UNITS_DM_CLEAR, EE_UNITS_DM_POOL_DELETE};
use std::collections::BTreeMap;

/// A recording host: every statement logged, answers from `answers` keyed by op (0 else).
#[derive(Default)]
struct Rec {
    log: Vec<(i32, u64, i64)>,
    answers: BTreeMap<i32, i64>,
}

impl DmHost for Rec {
    fn dm(&mut self, op: i32, a: u64, b: i64, _c: i64) -> i64 {
        self.log.push((op, a, b));
        self.answers.get(&op).copied().unwrap_or(0)
    }
}

fn rec(answers: &[(i32, i64)]) -> Rec {
    Rec { answers: answers.iter().copied().collect(), ..Default::default() }
}

fn ops(r: &Rec) -> Vec<i32> {
    r.log.iter().map(|e| e.0).collect()
}

const D: u64 = 0x100;

#[test]
fn clear_unloads_a_simple_doppelgangers_media_then_releases_and_resets() {
    let mut r = rec(&[(EE_UNITS_DM_DOPPELGANGER_UNIT, 0x200)]);
    clear(&mut r, D);
    assert_eq!(
        ops(&r),
        [
            EE_UNITS_DM_DOPPELGANGER_UNIT,
            EE_UNITS_DM_IS_COMPLEX,
            EE_UNITS_DM_DG_UNLOAD_MEDIA,
            EE_UNITS_DM_DG_DEREFERENCE,
            EE_UNITS_DM_DG_SET_UNIT,
            EE_UNITS_DM_DG_SET_PLAYER,
            EE_UNITS_DM_DG_SET_TILE
        ]
    );
    assert_eq!(r.log[3].1, 0x200);
    let mut r = rec(&[(EE_UNITS_DM_DOPPELGANGER_UNIT, 0x200), (EE_UNITS_DM_IS_COMPLEX, 1)]);
    clear(&mut r, D);
    assert!(!ops(&r).contains(&EE_UNITS_DM_DG_UNLOAD_MEDIA));
    let mut r = rec(&[]);
    clear(&mut r, D);
    assert_eq!(ops(&r)[1], EE_UNITS_DM_DG_SET_UNIT);
}

#[test]
fn initialize_sets_player_unit_media_reference_tile_in_order() {
    let mut r = rec(&[]);
    initialize(&mut r, D, 0x200, 0x300, 0x400);
    assert_eq!(
        r.log,
        [
            (EE_UNITS_DM_DG_SET_PLAYER, D, 0x300),
            (EE_UNITS_DM_DG_SET_UNIT, D, 0x200),
            (EE_UNITS_DM_IS_COMPLEX, D, 0),
            (EE_UNITS_DM_DG_LOAD_MEDIA, D, 0),
            (EE_UNITS_DM_DG_REFERENCE, 0x200, 0),
            (EE_UNITS_DM_DG_SET_TILE, D, 0x400)
        ]
    );
}

#[test]
fn initialize_copy_takes_the_other_ones_unit_and_tile() {
    let mut r = rec(&[(EE_UNITS_DM_DOPPELGANGER_UNIT, 0x200), (EE_UNITS_DM_DOPPELGANGER_TILE, 0x400), (EE_UNITS_DM_IS_COMPLEX, 1)]);
    initialize_copy(&mut r, D, 0x500, 0x300);
    assert_eq!(
        r.log,
        [
            (EE_UNITS_DM_DG_SET_PLAYER, D, 0x300),
            (EE_UNITS_DM_DOPPELGANGER_UNIT, 0x500, 0),
            (EE_UNITS_DM_DG_REFERENCE, 0x200, 0),
            (EE_UNITS_DM_DOPPELGANGER_UNIT, 0x500, 0),
            (EE_UNITS_DM_DG_SET_UNIT, D, 0x200),
            (EE_UNITS_DM_DOPPELGANGER_TILE, 0x500, 0),
            (EE_UNITS_DM_DG_SET_TILE, D, 0x400),
            (EE_UNITS_DM_IS_COMPLEX, D, 0)
        ]
    );
}

#[test]
fn load_keeps_the_ids_and_returns_the_second_read() {
    let mut r = rec(&[(EE_UNITS_DM_DG_VALUE, 17), (EE_UNITS_DM_DG_COORDINATE, 3), (EE_UNITS_DM_READ_LONG, 5), (EE_UNITS_DM_READ_ULONG, 6)]);
    assert_eq!(load(&mut r, D, 0x900), 5);
    assert_eq!(r.log[0], (EE_UNITS_DM_SET_FILE, 0x900, 0));
    assert_eq!(r.log[3], (EE_UNITS_DM_DG_SET_UNIT, D, 17));
    assert_eq!(r.log[6], (EE_UNITS_DM_DG_SET_PLAYER, D, 3));
}

#[test]
fn repair_pointers_puts_a_found_simple_doppelganger_back_on_its_tile() {
    let mut r = rec(&[(EE_UNITS_DM_DG_SAVED_UNIT, 0x200), (EE_UNITS_DM_PLAYER_AT, 0x300), (EE_UNITS_DM_DOPPELGANGER_TILE, 0x400)]);
    assert_eq!(repair_pointers(&mut r, D), 0);
    let o = ops(&r);
    assert_eq!(&o[o.len() - 4..], [EE_UNITS_DM_IS_COMPLEX, EE_UNITS_DM_DOPPELGANGER_TILE, EE_UNITS_DM_TILE_ADD, EE_UNITS_DM_DG_LOAD_MEDIA]);
    assert!(r.log.contains(&(EE_UNITS_DM_TILE_ADD, 0x400, D as i64)));
}

#[test]
fn repair_pointers_frees_a_simple_one_whose_unit_is_gone_and_fails_a_complex_one() {
    let mut r = rec(&[(EE_UNITS_DM_PLAYER_AT, 0x300)]);
    assert_eq!(repair_pointers(&mut r, D), 0);
    let o = ops(&r);
    assert!(o.contains(&EE_UNITS_DM_CLEAR) && o.contains(&EE_UNITS_DM_POOL_DELETE));
    assert!(!o.contains(&EE_UNITS_DM_DG_LOAD_MEDIA));
    let mut r = rec(&[(EE_UNITS_DM_DG_SAVED_UNIT, 0x200), (EE_UNITS_DM_IS_COMPLEX, 1)]);
    assert_eq!(repair_pointers(&mut r, D), 9);
    assert!(!ops(&r).contains(&EE_UNITS_DM_POOL_DELETE));
}

#[test]
fn save_writes_flag_unit_and_player_while_each_succeeds() {
    let mut r = rec(&[(EE_UNITS_DM_DG_IS_VALID, 1), (EE_UNITS_DM_IS_COMPLEX, 1), (EE_UNITS_DM_DG_UNIT_ID, 42), (EE_UNITS_DM_PLAYER_ID, 2)]);
    assert_eq!(save(&mut r, D, 0x900), 0);
    let writes: Vec<_> = r.log.iter().filter(|e| e.0 == EE_UNITS_DM_DG_WRITE_BOOL || e.0 == EE_UNITS_DM_DG_WRITE_ULONG).map(|e| e.2).collect();
    assert_eq!(writes, [1, 42, 2]);
    let mut r = rec(&[(EE_UNITS_DM_DG_IS_VALID, 1), (EE_UNITS_DM_DG_WRITE_BOOL, 3)]);
    assert_eq!(save(&mut r, D, 0x900), 3);
    assert!(!ops(&r).contains(&EE_UNITS_DM_DG_WRITE_ULONG));
    let mut r = rec(&[]);
    assert_eq!(save(&mut r, D, 0x900), 0);
    assert!(!ops(&r).contains(&EE_UNITS_DM_DG_WRITE_BOOL));
}

#[test]
fn the_manager_releases_both_pools_after_their_statistics() {
    let mut r = rec(&[]);
    release(&mut r);
    assert_eq!(
        ops(&r),
        [EE_UNITS_DM_DG_POOL_DUMP, EE_UNITS_DM_DG_POOL_RELEASE, EE_UNITS_DM_DG_COMPLEX_POOL_DUMP, EE_UNITS_DM_DG_COMPLEX_POOL_RELEASE]
    );
}
