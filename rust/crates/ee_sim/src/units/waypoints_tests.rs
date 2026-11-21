use super::*;

fn w(x: f32, y: f32, z: f32, distance: f32, start_distance: f32) -> EeUnitsWaypoint {
    EeUnitsWaypoint { location: [x, y, z], distance, start_distance }
}

#[test]
fn the_waypoint_is_the_cpp_element() {
    assert_eq!(std::mem::size_of::<EeUnitsWaypoint>(), 20);
    assert_eq!(std::mem::offset_of!(EeUnitsWaypoint, distance), 12);
    assert_eq!(std::mem::offset_of!(EeUnitsWaypoint, start_distance), 16);
}

#[test]
fn add_sums_the_steps_from_the_unit_then_the_last_waypoint() {
    let mut l = EeUnitsWaypoints::default();
    l.add([3.0, 4.0, 0.0], [0.0, 0.0, 0.0]);
    assert_eq!(l.get(0), Some(&w(3.0, 4.0, 0.0, 5.0, 0.0)));
    // the second step starts from the first waypoint, not the unit
    l.add([3.0, 4.0, 12.0], [100.0, 100.0, 100.0]);
    assert_eq!(l.get(1), Some(&w(3.0, 4.0, 12.0, 17.0, 5.0)));
    // the float sum in the original's order: ((dx * dx + dy * dy) + dz * dz)
    let mut m = EeUnitsWaypoints::default();
    let (a, b) = ([0.1f32, 0.2, 0.3], [1.7f32, -2.9, 0.05]);
    m.add(b, a);
    let (dx, dy, dz) = (b[0] - a[0], b[1] - a[1], b[2] - a[2]);
    assert_eq!(m.get(0).unwrap().distance.to_bits(), ((dx * dx + dy * dy) + dz * dz).sqrt().to_bits());
}

#[test]
fn path_start_and_second() {
    let mut l = EeUnitsWaypoints::default();
    assert_eq!(l.path_start(), None);
    assert_eq!(l.second(), None);
    l.push_back(w(1.0, 1.0, 0.0, 4.0, 2.5));
    assert_eq!(l.path_start(), Some(([1.0, 1.0, 0.0], 1.5)));
    assert_eq!(l.second(), None);
    l.push_back(w(2.0, 1.0, 0.0, 9.0, 4.0));
    assert_eq!(l.path_start(), Some(([1.0, 1.0, 0.0], 6.5)));
    assert_eq!(l.second(), Some([2.0, 1.0, 0.0]));
    l.pop_front();
    assert_eq!(l.path_start(), Some(([2.0, 1.0, 0.0], 5.0)));
}

#[test]
fn the_snapping_goal_kinds() {
    // location, attack unit, build/repair, 6, load transport, load garrison (SnapLastWaypoint's test)
    let kinds: Vec<u8> = (0..=255u8).filter(|&k| snaps_to_goal(k)).collect();
    assert_eq!(kinds, vec![1, 2, 4, 6, 7, 12]);
}

#[test]
fn take_path_skips_the_units_own_tile() {
    let p = [[5.5, 5.5, 0.0], [4.5, 5.5, 0.0], [3.2, 2.9, 0.0]];
    assert_eq!(take_path_skip((3, 2), &p), 2);
    assert_eq!(take_path_skip((3, 3), &p), 1);
    // a one-point path never skips two
    assert_eq!(take_path_skip((3, 2), &p[2..]), 1);
    assert_eq!(take_path_skip((3, 2), &[]), 1);
    // UMFloor of a negative coordinate
    assert_eq!(take_path_skip((-1, 0), &[[0.0, 0.0, 0.0], [-0.5, 0.5, 0.0]]), 2);
}

#[test]
fn thin_path_keeps_both_ends_and_every_step() {
    let p: Vec<[f32; 3]> = (0..10).map(|i| [i as f32, 0.0, 0.0]).collect();
    let xs = |v: Vec<[f32; 3]>| v.iter().map(|p| p[0] as i32).collect::<Vec<_>>();
    assert_eq!(xs(thin_path(&p, 3)), vec![0, 3, 6, 9]);
    assert_eq!(xs(thin_path(&p, 4)), vec![0, 4, 8, 9]);
    assert_eq!(xs(thin_path(&p, 1)), (0..10).collect::<Vec<_>>());
    // a negative step (the priest ship's sphere rounds below 0): C++'s remainder, the dividend's sign
    assert_eq!(xs(thin_path(&p, -3)), vec![0, 3, 6, 9]);
    assert!(thin_path(&[], 2).is_empty());
}

#[test]
fn merge_appends_the_waypoints_after_the_one_on_the_path() {
    let mut path = vec![[0.5, 0.5, 0.0], [1.5, 0.5, 0.0], [2.5, 0.5, 0.0]];
    let wps = [[9.5, 9.5, 0.0], [2.2, 0.7, 0.0], [3.5, 0.5, 0.0], [4.5, 0.5, 0.0]];
    merge(&mut path, &wps);
    assert_eq!(path.len(), 5);
    assert_eq!(path[3], [3.5, 0.5, 0.0]);
    assert_eq!(path[4], [4.5, 0.5, 0.0]);
    let mut other = vec![[0.5, 0.5, 0.0]];
    merge(&mut other, &[[7.5, 7.5, 0.0]]);
    assert_eq!(other.len(), 1);
}

#[test]
fn the_c_interface() {
    let l = ee_units_waypoints_new();
    let m = ee_units_waypoints_new();
    // SAFETY: lists made here
    unsafe {
        assert_eq!(ee_units_waypoints_len(l), 0);
        assert!(ee_units_waypoints_at(l, 0).is_null());
        let mut out = [9.0f32; 3];
        let mut length = 9.0f32;
        assert_eq!(ee_units_waypoints_start(l, out.as_mut_ptr(), &mut length), EE_UNITS_WAYPOINTS_NONE);
        assert_eq!((out, length), ([9.0; 3], 9.0));
        ee_units_waypoints_push(l, &w(1.0, 2.0, 3.0, 4.0, 1.0));
        ee_units_waypoints_push(l, &w(5.0, 6.0, 7.0, 8.0, 4.0));
        assert_eq!(ee_units_waypoints_len(l), 2);
        assert_eq!(*ee_units_waypoints_at(l, 1), w(5.0, 6.0, 7.0, 8.0, 4.0));
        assert_eq!(ee_units_waypoints_start(l, out.as_mut_ptr(), &mut length), 0);
        assert_eq!((out, length), ([1.0, 2.0, 3.0], 7.0));
        assert_eq!(ee_units_waypoints_start(l, std::ptr::null_mut(), &mut length), 0);
        assert_eq!(ee_units_waypoints_second(l, out.as_mut_ptr()), 0);
        assert_eq!(out, [5.0, 6.0, 7.0]);
        ee_units_waypoints_assign(m, l);
        ee_units_waypoints_assign(m, m);
        assert_eq!(ee_units_waypoints_len(m), 2);
        ee_units_waypoints_clear(l);
        assert_eq!(ee_units_waypoints_len(l), 0);
        assert_eq!(ee_units_waypoints_len(m), 2);
        assert_eq!(ee_units_waypoints_second(l, out.as_mut_ptr()), EE_UNITS_WAYPOINTS_NONE);
        ee_units_waypoints_free(l);
        ee_units_waypoints_free(m);
        ee_units_waypoints_free(std::ptr::null_mut());
    }
}
