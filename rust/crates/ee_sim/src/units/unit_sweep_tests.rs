//! unit_sweep.rs: the sweep's pure choices.

use super::*;

#[test]
fn the_rings_go_row_sides_row() {
    let t = ring_tiles(5, 5, 2);
    assert_eq!(&t[..8], &[(4, 4), (5, 4), (6, 4), (4, 5), (6, 5), (4, 6), (5, 6), (6, 6)]);
    // ring 2: its top row of five, then the sides of the three middle rows, then the bottom row
    assert_eq!(t.len(), 8 + 16);
    assert_eq!(t[8], (3, 3));
    assert_eq!(t[13], (3, 4));
    assert_eq!(t[14], (7, 4));
}

#[test]
fn the_swept_models_and_categories() {
    assert!(swept(1 << 8, 0x10));
    assert!(swept(11 << 8, 0x10));
    assert!(!swept(0xff << 8, 0x10), "no model");
    assert!(!swept(0, 0x10), "the land model");
    assert!(!swept(1 << 8 | 1, 0x10), "a building");
    assert!(!swept(2 << 8 | 2, 0x10), "a land building");
    assert!(!swept(5 << 8, CATEGORY_TORPEDO));
    assert!(!swept(9 << 8, CATEGORY_BOMB));
}

#[test]
fn producers_by_traffic_and_domain() {
    assert_eq!(producer_for(true, 1, 0), Some(Some(AIRPORT)));
    assert_eq!(producer_for(true, 3, 0), Some(Some(2)));
    assert_eq!(producer_for(true, 0, 0), Some(None));
    assert_eq!(producer_for(true, 5, 0), Some(None));
    assert_eq!(producer_for(false, 0, MOVEMENT_SPACE | MOVEMENT_SEA), Some(Some(SPACE_CARRIER)));
    assert_eq!(producer_for(false, 0, MOVEMENT_SEA), Some(Some(DOCK)));
    assert_eq!(producer_for(false, 0, 2), None);
}

#[test]
fn targets_fall_back_to_the_town() {
    assert_eq!(target_of(MOVEMENT_SPACE, 0, 9, 8, 7, 6), 9);
    assert_eq!(target_of(MOVEMENT_SPACE, 0, 0, 8, 7, 6), 6);
    assert_eq!(target_of(MOVEMENT_SEA, 0, 9, 8, 7, 6), 8);
    assert_eq!(target_of(MOVEMENT_SEA, 0, 9, 0, 7, 6), 7);
    assert_eq!(target_of(1, 2, 9, 8, 7, 6), 8);
    assert_eq!(target_of(1, 1, 9, 8, 7, 6), 6);
    assert_eq!(target_of(1, 2, 9, 0, 7, 6), 6);
}
