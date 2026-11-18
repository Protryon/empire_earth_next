use super::*;

fn rect(left: i32, top: i32, right: i32, bottom: i32) -> EeCpRect {
    EeCpRect { left, top, right, bottom }
}

#[test]
fn the_grid_is_the_ceiling_of_the_map_over_nine_tiles() {
    let t = EeCpInfluence::new(100, 9, 3);
    // 100 / 9 = 11.1: 12 regions a side, 4 entries each
    assert_eq!((t.regions_x, t.regions_y, t.values.len()), (12, 12, 12 * 12 * 4));
    let t = EeCpInfluence::new(99, 9, 2);
    assert_eq!(t.regions_x, 11);
    assert_eq!(ceil_unsigned(10.5), 11);
    assert_eq!(ceil_unsigned(10.4), 11);
    assert_eq!(ceil_unsigned(11.0), 11);
}

#[test]
fn units_add_to_their_owner_and_the_total_and_removals_clamp_at_zero() {
    let mut t = EeCpInfluence::new(90, 9, 3);
    t.add(1, 50, 20, 30); // region (2, 3)
    t.add(2, 7, 26, 35);
    t.add(1, 5, -1, 3); // -1 / 9 truncates to 0: region (0, 0)
    assert_eq!(t.value(1, 2, 3), 50);
    assert_eq!(t.value(2, 2, 3), 7);
    assert_eq!(t.value(3, 2, 3), 57, "the total");
    assert_eq!(t.value(1, 0, 0), 5);
    t.add(1, 5, -10000, -10000); // a unit off the map: no region
    assert_eq!(t.sum_value(3, rect(0, 0, 9, 9)), 62);
    assert_eq!(t.others_value(1, 2, 3), 7);
    assert_eq!(t.others_value(2, 2, 3), 50);
    assert_eq!(t.others_value(1, 20, 3), 0, "off the grid");
    // a player past the entries (the route planner's -1) reads 0
    assert_eq!(t.value(u32::MAX, 2, 3), 0);
    t.remove(2, 10, 26, 35);
    assert_eq!((t.value(2, 2, 3), t.value(3, 2, 3)), (0, 47));
    t.remove(1, 100, 20, 30);
    assert_eq!((t.value(1, 2, 3), t.value(3, 2, 3)), (0, 0));
}

#[test]
fn sums_clip_the_start_corner_only_for_the_others_and_the_enemies() {
    let mut t = EeCpInfluence::new(90, 9, 2);
    t.add(0, 10, 0, 0);
    t.add(1, 3, 0, 0);
    t.add(0, 4, 9, 9);
    // a rectangle from -1: SumValue reads the -1 rows as off the grid (0), the others' and enemies' sums start at 0
    assert_eq!(t.sum_value(0, rect(-1, -1, 1, 1)), 14);
    assert_eq!(t.sum_others_value(1, rect(-1, -1, 1, 1)), 14);
    assert_eq!(t.sum_others_value(0, rect(-1, -1, 0, 0)), 3);
    assert_eq!(t.enemy_value(&[0], 0, 0), 10);
    assert_eq!(t.enemy_value(&[0, 1], 1, 1), 4);
    assert_eq!(t.enemy_value(&[0, 1], -1, 0), 0);
    assert_eq!(t.sum_enemy_value(&[0], rect(-5, -5, 1, 1)), 14);
    assert_eq!(t.sum_enemy_value(&[], rect(0, 0, 1, 1)), 0);
}

#[test]
fn the_enemies_are_the_other_players_at_war_from_slot_one_unless_asked() {
    // slots: 0 nature (id 0), 1 me (id 1), 2 at war (id 2), 3 an ally (id 3), 4 empty
    let slot = |i: usize| match i {
        0..=3 => Some((0x1000 + i as u64, i as u32)),
        _ => None,
    };
    let war = |id: u32| id == 0 || id == 2;
    assert_eq!(enemy_indices(5, false, Some(0x1001), slot, war), Some(vec![2]));
    assert_eq!(enemy_indices(5, true, Some(0x1001), slot, war), Some(vec![0, 2]));
    // me at war with myself counts nothing
    assert_eq!(enemy_indices(5, true, Some(0x1002), slot, |_| true), Some(vec![0, 1, 3]));
    // the table's players bound the loop
    assert_eq!(enemy_indices(2, true, Some(0x1001), slot, war), Some(vec![0]));
    assert_eq!(enemy_indices(5, true, None, slot, war), None);
}
