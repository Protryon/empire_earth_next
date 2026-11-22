use super::*;
use std::collections::BTreeMap;

/// A recording host: every statement logged, answers from `answers` keyed by op (0 else).
#[derive(Default)]
struct Rec {
    log: Vec<(i32, u64, u64, i64)>,
    answers: BTreeMap<i32, i64>,
}

impl GhostHost for Rec {
    fn ghost(&mut self, op: i32, ghost: u64, a: u64, b: i64, _c: i64) -> i64 {
        self.log.push((op, ghost, a, b));
        self.answers.get(&op).copied().unwrap_or(0)
    }
}

fn rec(answers: &[(i32, i64)]) -> Rec {
    Rec { answers: answers.iter().copied().collect(), ..Default::default() }
}

fn ops(r: &Rec) -> Vec<i32> {
    r.log.iter().map(|e| e.0).collect()
}

const G: u64 = 0x100;

#[test]
fn reset_clears_every_member_in_the_originals_order() {
    let mut r = rec(&[]);
    reset(&mut r, G);
    assert_eq!(
        ops(&r),
        [
            EE_UNITS_GHOST_SET_X,
            EE_UNITS_GHOST_SET_Y,
            EE_UNITS_GHOST_SET_Z,
            EE_UNITS_GHOST_SET_CAN_PLACE,
            EE_UNITS_GHOST_M_SET_PLAYER,
            EE_UNITS_GHOST_SET_TILE_X,
            EE_UNITS_GHOST_SET_TILE_Y,
            EE_UNITS_GHOST_M_SET_CLASS,
            EE_UNITS_GHOST_M_SET_FRAME,
            EE_UNITS_GHOST_M_SET_ANIMATION_INDEX,
            EE_UNITS_GHOST_M_SET_NO_MEDIA,
            EE_UNITS_GHOST_M_CLEAR_FOOTPRINT,
            EE_UNITS_GHOST_M_CLEAR_HEIGHTS
        ]
    );
    assert_eq!(r.log[9].3, 7);
}

#[test]
fn construct_loads_media_in_free_camera_queues_it_otherwise() {
    let mut r = rec(&[(EE_UNITS_GHOST_CLASS_AT, 0x500), (EE_UNITS_GHOST_M_FREE_CAMERA, 1)]);
    construct(&mut r, G, 9, 0x300, true);
    let o = ops(&r);
    let tail = &o[13..];
    assert_eq!(
        tail,
        [
            EE_UNITS_GHOST_CLASS_AT,
            EE_UNITS_GHOST_M_SET_CLASS,
            EE_UNITS_GHOST_M_SET_PLAYER,
            EE_UNITS_GHOST_M_FREE_CAMERA,
            EE_UNITS_GHOST_M_LOAD_CLASS_MEDIA,
            EE_UNITS_GHOST_M_SET_CLASS_MEDIA,
            EE_UNITS_GHOST_M_SET_NO_MEDIA
        ]
    );
    assert_eq!(r.log[14].3, 0x500);
    assert_eq!(r.log[17], (EE_UNITS_GHOST_M_LOAD_CLASS_MEDIA, G, 0x300, 9));
    assert_eq!(r.log[19].3, 1);
    let mut r = rec(&[]);
    construct(&mut r, G, 9, 0x300, false);
    assert!(ops(&r).contains(&EE_UNITS_GHOST_M_POST_LOAD));
    let mut r = rec(&[]);
    construct(&mut r, G, 9, 0x300, true);
    assert!(!ops(&r).contains(&EE_UNITS_GHOST_M_POST_LOAD));
}

#[test]
fn destroy_queues_the_unload_or_unloads_in_free_camera_or_at_shutdown() {
    let mut r = rec(&[]);
    destroy(&mut r, G);
    assert_eq!(ops(&r), [EE_UNITS_GHOST_M_NO_MEDIA, EE_UNITS_GHOST_M_FREE_CAMERA, EE_UNITS_GHOST_M_SHUTTING_DOWN, EE_UNITS_GHOST_M_POST_UNLOAD]);
    let mut r = rec(&[(EE_UNITS_GHOST_M_FREE_CAMERA, 1)]);
    destroy(&mut r, G);
    assert_eq!(ops(&r), [EE_UNITS_GHOST_M_NO_MEDIA, EE_UNITS_GHOST_M_FREE_CAMERA, EE_UNITS_GHOST_M_UNLOAD_CLASS_MEDIA]);
    let mut r = rec(&[(EE_UNITS_GHOST_M_NO_MEDIA, 1)]);
    destroy(&mut r, G);
    assert_eq!(ops(&r), [EE_UNITS_GHOST_M_NO_MEDIA]);
}

#[test]
fn load_reads_the_player_only_after_a_good_first_read() {
    let mut r = rec(&[(EE_UNITS_GHOST_M_VALUE, 33)]);
    assert_eq!(load(&mut r, G, 0x900), 0);
    assert_eq!(r.log.iter().filter(|e| e.0 == EE_UNITS_GHOST_M_READ_ULONG).count(), 2);
    assert!(r.log.contains(&(EE_UNITS_GHOST_M_SET_PLAYER, G, 0, 33)));
    let mut r = rec(&[(EE_UNITS_GHOST_M_READ_ULONG, 4)]);
    assert_eq!(load(&mut r, G, 0x900), 4);
    assert!(!ops(&r).contains(&EE_UNITS_GHOST_M_SET_PLAYER));
}

#[test]
fn save_writes_the_type_then_the_player() {
    let mut r = rec(&[(EE_UNITS_GHOST_UNIT_TYPE, 12)]);
    assert_eq!(save(&mut r, G, 0x900), 0);
    assert_eq!(ops(&r), [EE_UNITS_GHOST_M_SET_FILE, EE_UNITS_GHOST_UNIT_TYPE, EE_UNITS_GHOST_M_WRITE_ULONG, EE_UNITS_GHOST_M_WRITE_PLAYER_ID]);
    assert_eq!(r.log[2].3, 12);
    let mut r = rec(&[(EE_UNITS_GHOST_M_WRITE_ULONG, 2)]);
    assert_eq!(save(&mut r, G, 0x900), 2);
    assert!(!ops(&r).contains(&EE_UNITS_GHOST_M_WRITE_PLAYER_ID));
}

#[test]
fn farm_slot_and_animation() {
    let mut r = rec(&[(EE_UNITS_GHOST_TILE_X, 10), (EE_UNITS_GHOST_TILE_Y, 20)]);
    set_farm_slot_tile(&mut r, G, 2);
    let t = crate::units::uu_tiles::slot_tile([10, 20], 2);
    assert_eq!(r.log[2], (EE_UNITS_GHOST_SET_TILE_X, G, 0, i64::from(t[0])));
    assert_eq!(r.log[3], (EE_UNITS_GHOST_SET_TILE_Y, G, 0, i64::from(t[1])));
    let mut r = rec(&[(EE_UNITS_GHOST_M_START_FRAME, 40)]);
    set_animation(&mut r, G, 3);
    assert_eq!(r.log, [(EE_UNITS_GHOST_M_START_FRAME, G, 0, 3), (EE_UNITS_GHOST_M_SET_FRAME, G, 0, 40), (EE_UNITS_GHOST_M_SET_ANIMATION_INDEX, G, 0, 3)]);
}

#[test]
fn the_checksum_takes_the_player_then_the_class() {
    let mut r = rec(&[]);
    add_checksum(&mut r, G, 0x700);
    assert_eq!(
        r.log,
        [
            (EE_UNITS_GHOST_M_CHECKSUM_PLAYER_TEXT, G, 0, 0),
            (EE_UNITS_GHOST_M_CHECKSUM_ADD, G, 0x700, 0),
            (EE_UNITS_GHOST_M_CHECKSUM_CLASS_TEXT, G, 0, 0),
            (EE_UNITS_GHOST_M_CHECKSUM_ADD, G, 0x700, 1)
        ]
    );
}
