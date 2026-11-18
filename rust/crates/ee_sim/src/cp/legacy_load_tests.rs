use super::*;
use crate::cp::file::test_file::{file, Bytes};

// The record sizes in bytes, worked out by hand from the deleted managers' Load (EECPLegacyLoad.cpp's comments): a
// word is 4 bytes, a bool 1, an unsigned short 2.
const LOCATION: usize = 5 * 4; // tile (2), area serial, area type, player
const TASK: usize = LOCATION + 5 * 4 + 2; // unit id, player id, aggression, type, mode; active, B31
const SUB_GROUP_UNIT: usize = LOCATION + 4 * 4 + 3 + 4 + 4 * 4 + 3 * 4;

fn zeros(b: &mut Bytes, n: usize) {
    b.data.extend(std::iter::repeat_n(0u8, n));
}

/// An attack sub group with `units` units and `path` path tiles (its fixed part after them).
fn sub_group(b: &mut Bytes, units: u32, path: u32) {
    b.u32(units);
    for _ in 0..units {
        b.u32(3); // the unit's kind
        zeros(b, SUB_GROUP_UNIT);
    }
    b.u32(path);
    zeros(b, path as usize * 12);
    zeros(b, 4 * 4 + 2 * LOCATION + TASK + 3 * 4 + 8 * 4 + 3 * 4 + 4 + 2 * 4);
}

fn land_part(b: &mut Bytes, avoided: u32) {
    zeros(b, 2 * LOCATION + 2 * 4 + 1 + 2 * 8);
    b.u32(avoided);
    zeros(b, avoided as usize * 12);
}

/// One attack group with one sub group of each kind given.
fn attack_group(b: &mut Bytes, kinds: &[i32], orders: u32) {
    zeros(b, TASK);
    b.u32(kinds.len() as u32);
    for &k in kinds {
        b.u32(k as u32);
        sub_group(b, 2, 1);
        match k {
            0 | 5 | 8 => land_part(b, 2),
            3 | 4 | 10 => zeros(b, 8),
            _ => {}
        }
    }
    b.u32(orders);
    zeros(b, orders as usize * (LOCATION + 3 * 4 + 3 * 4 + 1 + 4));
    zeros(b, 4 + 2 + 1);
}

fn request(b: &mut Bytes, units: u32, deadline: bool) {
    b.u32(units);
    zeros(b, units as usize * 13);
    zeros(b, 4 + 4 + 2 + 4 + 8 + 4 + if deadline { 4 } else { 0 });
}

fn combat_manager(b: &mut Bytes, minor: u32, groups: &[&[i32]], aid: bool) {
    // the personnel: pairs, requests, next id, deferred
    b.u32(1);
    zeros(b, 8);
    b.u32(2);
    for _ in 0..2 {
        b.u32(7);
        request(b, 2, minor >= 0xde);
    }
    zeros(b, 4);
    b.u32(3);
    zeros(b, 12);
    // the attack commander
    b.u32(groups.len() as u32);
    for g in groups {
        attack_group(b, g, 2);
    }
    zeros(b, 4);
    // the defence commander: time, enabled, aid packages (one invalid, one valid if `aid`)
    zeros(b, 5);
    b.u32(if aid { 2 } else { 1 });
    b.b(false);
    if aid {
        b.b(true);
        b.u32(2);
        zeros(b, 8 + 8 + 4 + 4 + 4 + 4 + 12);
    }
    zeros(b, 4);
    b.u32(1);
    zeros(b, 4);
    zeros(b, 4 * (usize::from(minor >= 0xd0) + usize::from(minor >= 0xed) + usize::from(minor >= 0xf0)));
    // the combat manager's own
    zeros(b, 8 + 2);
    b.u32(1);
    zeros(b, TASK);
    b.u32(2);
    zeros(b, 2 * TASK);
    zeros(b, 1);
    b.u32(2);
    zeros(b, 8);
    b.u32(1);
    zeros(b, 12);
}

fn civilization_manager(b: &mut Bytes, minor: u32, tile: (i32, i32)) {
    for k in 0..7 {
        b.u32(k);
        zeros(b, k as usize * 4);
    }
    zeros(b, 7 * 4 + 7 * 4 + 7 + 2 + 4 * 4);
    let epochs = if minor >= 0xea { 15 } else { 14 };
    for _ in 0..epochs {
        b.u32(1);
        zeros(b, 12);
    }
    b.u32(tile.0 as u32).u32(tile.1 as u32);
    if minor >= 0xdc {
        zeros(b, 4);
    }
    b.u32(2);
    zeros(b, 8);
    for _ in 0..epochs {
        b.u32(1);
        zeros(b, 4);
    }
    zeros(b, 4 + 4 + 1);
    for _ in 0..epochs {
        b.u32(2);
        zeros(b, 2 * (4 + 4 + 2 + 4));
    }
    b.u32(1);
    zeros(b, 8);
    zeros(b, 4 + 1);
    zeros(b, 4 + 4 + 1 + 4 + 3 + 3 * 4);
    b.u32(2);
    zeros(b, 16);
    b.u32(1);
    zeros(b, 4);
    zeros(b, 3 * 8 + 4 + 4 + 4 + 1 + 4);
    zeros(b, 4 + 4 + 1);
}

fn exploration_manager(b: &mut Bytes) {
    b.u32(2);
    zeros(b, 8);
    zeros(b, 2);
    b.u32(1);
    zeros(b, 4);
}

#[test]
fn the_three_blocks_are_read_to_their_end_at_every_legacy_version() {
    for minor in [0xe5u32, 0xe8, 0xe9, 0xea, 0xec, 0xed, 0xf0, crate::cp::ffi::EE_CP_LEGACY_LAST_VERSION] {
        let mut b = Bytes::default();
        combat_manager(&mut b, minor, &[&[0, 1, 2, 3], &[4, 5, 6, 7, 8, 9, 10, 11]], true);
        let combat_end = b.data.len();
        civilization_manager(&mut b, minor, (31, -7));
        let civ_end = b.data.len();
        exploration_manager(&mut b);
        b.u32(0xdead_beef); // the computer player's own block follows
        assert_eq!(skip_combat_manager(&mut file(&mut b), 0, minor), OK, "minor {minor}");
        assert_eq!(b.at, combat_end, "minor {minor}");
        let mut tile = [-10000, -10000];
        assert_eq!(skip_civilization_manager(&mut file(&mut b), 0, minor, &mut tile), OK);
        assert_eq!(b.at, civ_end);
        assert_eq!(tile, [31, -7]);
        assert_eq!(skip_exploration_manager(&mut file(&mut b), 0, minor), OK);
        assert_eq!(b.data.len() - b.at, 4);
    }
}

#[test]
fn a_combat_block_before_0_228_or_an_unknown_sub_group_kind_fails() {
    let mut b = Bytes::default();
    assert_eq!(skip_combat_manager(&mut file(&mut b), 0, 0xe3), FAILED);
    assert_eq!(b.at, 0, "nothing read");
    let mut b = Bytes::default();
    combat_manager(&mut b, 0xf0, &[&[12]], false);
    assert_eq!(skip_combat_manager(&mut file(&mut b), 0, 0xf0), FAILED);
}

#[test]
fn a_short_file_stops_at_the_first_failed_read() {
    let mut whole = Bytes::default();
    combat_manager(&mut whole, 0xf0, &[&[0, 3]], true);
    for cut in [0, 1, 9, 100, whole.data.len() - 1] {
        let mut b = Bytes { data: whole.data[..cut].to_vec(), at: 0 };
        assert_eq!(skip_combat_manager(&mut file(&mut b), 0, 0xf0), 0xa, "cut at {cut}");
    }
    let mut whole = Bytes::default();
    civilization_manager(&mut whole, 0xf0, (1, 2));
    // a cut inside the town centre tile's y: x is read, y left as it was
    // the tile follows the resource manager's lists and fields and the build queues
    let tile_at = (0..7).map(|k| 4 + k * 4).sum::<usize>() + 7 * 4 + 7 * 4 + 7 + 2 + 4 * 4 + 15 * 16;
    assert_eq!(&whole.data[tile_at..tile_at + 8], &[1, 0, 0, 0, 2, 0, 0, 0]);
    let mut b = Bytes { data: whole.data[..tile_at + 6].to_vec(), at: 0 };
    let mut tile = [-10000, -10000];
    assert_eq!(skip_civilization_manager(&mut file(&mut b), 0, 0xf0, &mut tile), 0xa);
    assert_eq!(tile, [1, -10000]);
}

#[test]
fn the_exploration_block_of_a_file_before_0_033_has_one_more_word() {
    let mut b = Bytes::default();
    b.u32(1).u32(5).u32(9).b(true).b(false).u32(0);
    assert_eq!(skip_exploration_manager(&mut file(&mut b), 0, 0x20), OK);
    assert_eq!(b.at, b.data.len());
}

#[test]
fn a_sub_group_unit_s_failed_read_is_not_reported_by_the_unit_loop() {
    // two units claimed, the first unit cut short: the unit loop goes on; the path count read fails then
    let mut b = Bytes::default();
    b.u32(2).u32(3);
    zeros(&mut b, 10);
    assert_eq!(skip_sub_group(&mut file(&mut b)), 0xa);
}
