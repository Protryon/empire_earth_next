use super::*;

/// An id per handle: the handle times ten.
fn ids(unit: EeCombatHandle) -> u32 {
    unit as u32 * 10
}

#[test]
fn a_region_keeps_the_order_units_entered_and_erases_the_first_copy() {
    let mut t = EeCombatUnitRegions::new(4, 3);
    assert_eq!((t.size(), t.count()), (3, 16));
    t.add(1, 2, 7);
    t.add(1, 2, 8);
    t.add(1, 2, 7);
    t.add(1, 2, 9);
    assert_eq!(t.region(1, 2).unwrap(), &[7, 8, 7, 9]);
    t.remove(1, 2, 7);
    assert_eq!(t.region(1, 2).unwrap(), &[8, 7, 9]);
    // a unit not there, a region off the grid: nothing
    t.remove(1, 2, 5);
    t.add(4, 0, 1);
    t.remove(4, 0, 1);
    assert_eq!(t.region(1, 2).unwrap(), &[8, 7, 9]);
    // the array is column-major, as the original's (x * side + y)
    assert_eq!(t.at(6).unwrap(), &[8, 7, 9]);
    // off the grid: none (a negative coordinate is a large unsigned one)
    assert!(t.region(4, 0).is_none() && t.region(0, -1i32 as u32).is_none());
}

#[test]
fn the_values_walk_the_regions_backwards_from_the_last_unit_and_collect_sorts() {
    let mut t = EeCombatUnitRegions::new(3, 3);
    t.add(0, 0, 5);
    t.add(0, 0, 3);
    t.add(0, 1, 9);
    t.add(1, 0, 3);
    t.add(2, 2, 1);
    // tiles 0..5 by 0..5: regions (0,0), (0,1), (1,0), (1,1); from the last region and its last unit
    assert_eq!(t.values(0, 0, 5, 5), [3, 9, 3, 5]);
    assert_eq!(t.collect(0, 0, 5, 5), [3, 5, 9]);
    // the low edges clamped to 0, the high ones not (regions off the grid skipped)
    assert_eq!(t.values(-7, -7, 100, 100), [1, 3, 9, 3, 5]);
    // C's truncating division: tile -1 is region 0
    assert_eq!(t.rect(-1, -4, 2, 3), (0, -1, 0, 1));
}

#[test]
fn a_footprint_enters_or_leaves_every_region_under_it() {
    let mut t = EeCombatUnitRegions::new(4, 3);
    // tiles 2..5 by 2..5: regions 0..1 by 0..1
    t.footprint(2, 2, 4, 7, true);
    for (x, y) in [(0, 0), (0, 1), (1, 0), (1, 1)] {
        assert_eq!(t.region(x, y).unwrap(), &[7]);
    }
    assert!(t.region(2, 2).unwrap().is_empty());
    // a one-tile unit: one region
    t.footprint(4, 4, 1, 8, true);
    assert_eq!(t.region(1, 1).unwrap(), &[7, 8]);
    t.footprint(2, 2, 4, 7, false);
    assert_eq!(t.region(1, 1).unwrap(), &[8]);
    assert!(t.region(0, 0).unwrap().is_empty());
}

#[test]
fn the_saved_order_lists_the_regions_of_two_units_or_more() {
    let mut t = EeCombatUnitRegions::new(2, 3);
    t.add(0, 1, 4);
    t.add(0, 1, 0);
    t.add(1, 0, 6);
    t.add(1, 1, 2);
    t.add(1, 1, 3);
    t.add(1, 1, 1);
    // two regions: index 1 (a unit without a complex unit: -1) and index 3
    assert_eq!(t.order(ids), [2, 1, 2, 40, u32::MAX, 3, 3, 20, 30, 10]);
    assert_eq!(EeCombatUnitRegions::new(2, 3).order(ids), [0]);
}

#[test]
fn a_restore_puts_the_saved_order_back_only_for_the_same_units() {
    let mut t = EeCombatUnitRegions::new(2, 3);
    for u in [1, 2, 3, 2] {
        t.add(1, 1, u);
    }
    assert!(t.restore(3, &[20, 30, 20, 10], ids));
    assert_eq!(t.region(1, 1).unwrap(), &[2, 3, 2, 1]);
    // another unit, another count: left as it is
    assert!(!t.restore(3, &[20, 30, 20, 40], ids));
    assert!(!t.restore(3, &[20, 30, 10], ids));
    assert_eq!(t.region(1, 1).unwrap(), &[2, 3, 2, 1]);
    // an empty region and no ids: put back (nothing to do)
    assert!(t.restore(0, &[], ids));
}

#[test]
fn the_c_interface_answers_off_the_grid_and_without_a_table() {
    let t = ee_combat_regions_new(2, 3);
    unsafe {
        ee_combat_regions_add(t, 1, 0, 11);
        ee_combat_regions_footprint(t, 0, 0, 1, 12, true);
        assert_eq!(ee_combat_regions_size(t), 3);
        let mut n = 0;
        let units = ee_combat_regions_units(t, 1, 0, &mut n);
        assert_eq!(std::slice::from_raw_parts(units, n as usize), &[11]);
        assert!(ee_combat_regions_units(t, 2, 0, &mut n).is_null());
        assert_eq!(n, -1);
        assert_eq!((ee_combat_regions_len(t, 0, 0), ee_combat_regions_len(t, 0, 5)), (1, -1));
        // the collection: its length first, then the values
        assert_eq!(ee_combat_regions_collect(t, 0, 0, 5, 5, std::ptr::null_mut(), 0), 2);
        let mut out = [0u64; 2];
        ee_combat_regions_collect(t, 0, 0, 5, 5, out.as_mut_ptr(), 2);
        assert_eq!(out, [11, 12]);
        // no region of two units: the order is its count 0
        let mut order = [9u32; 1];
        assert_eq!(ee_combat_regions_order(t, order.as_mut_ptr(), 1), 1);
        assert_eq!(order, [0]);
        assert_eq!(ee_combat_regions_order(std::ptr::null(), order.as_mut_ptr(), 1), 1);
        // no table, a region off the table: -1 (the load stops)
        assert_eq!(ee_combat_regions_restore(std::ptr::null_mut(), 0, std::ptr::null(), 0), -1);
        assert_eq!(ee_combat_regions_restore(t, 4, std::ptr::null(), 0), -1);
        ee_combat_regions_remove(t, 1, 0, 11);
        assert_eq!(ee_combat_regions_len(t, 1, 0), 0);
        ee_combat_regions_free(t);
        ee_combat_regions_free(std::ptr::null_mut());
    }
}
