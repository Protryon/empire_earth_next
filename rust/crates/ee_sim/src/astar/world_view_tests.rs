use super::*;

fn unit() -> TileUnit {
    TileUnit { id: 7, hit_points: 10, category: 3, unit_type: 0, blocks_tile: true, is_wall: false, foundation: false, player_id: 2 }
}

#[test]
fn the_area_blocker_is_a_live_tile_blocker_or_gate_not_a_foundation() {
    assert!(unit().blocks_area());
    assert!(!TileUnit { blocks_tile: false, ..unit() }.blocks_area());
    assert!(TileUnit { blocks_tile: false, category: CATEGORY_GATE, ..unit() }.blocks_area());
    assert!(!TileUnit { foundation: true, ..unit() }.blocks_area());
    assert!(!TileUnit { hit_points: 0, ..unit() }.blocks_area());
    // a dead resource still blocks
    assert!(TileUnit { hit_points: 0, category: CATEGORY_RESOURCE, ..unit() }.blocks_area());
    // the obstacle test: a gate foundation is an obstacle, a blocking foundation is not
    assert!(TileUnit { foundation: true, category: CATEGORY_GATE, ..unit() }.is_obstacle());
    assert!(!TileUnit { foundation: true, ..unit() }.is_obstacle());
    assert!(!TileUnit { foundation: true, category: CATEGORY_GATE, ..unit() }.blocks_area());
}

#[test]
fn the_wood_kinds_by_tree_type() {
    for t in [0xee, 0xef, 0x151, 0x152, 0x153, 0x154, 0x155, 0x156, 0x157, 0x158] {
        assert_eq!(resource_kind(t), 1, "{t:#x}");
    }
    for t in [0x159, 0x15a, 0x15e, 0x15f, 0x160, 0x161] {
        assert_eq!(resource_kind(t), 3, "{t:#x}");
    }
    for t in [0, 0xed, 0xf0, 0x150, 0x15b, 0x15c, 0x15d, 0x162] {
        assert_eq!(resource_kind(t), 2, "{t:#x}");
    }
}

#[test]
fn the_height_level_floors_the_half_step_count() {
    assert_eq!(height_level(0.0, 2.0), 0);
    assert_eq!(height_level(1.0, 2.0), 1);
    assert_eq!(height_level(2.5, 2.0), 2);
    assert_eq!(height_level(-0.5, 2.0), -1);
}

fn mover() -> Mover {
    Mover {
        handle: 10,
        collidable: true,
        foundation: false,
        active_group: 0,
        collision_radius: 0.5,
        command_type: ACTION_IDLE,
        velocity: 0.0,
        domains: MOVEMENT_SEA,
        category: 0,
        air_traffic_type: 0,
    }
}

#[test]
fn the_crowd_cost_counts_idle_and_attacking_collidable_units() {
    let m = mover();
    assert_eq!(crowd_step(&m, 1, 0, true), 50);
    assert_eq!(crowd_step(&m, 10, 0, true), 0, "the walker itself");
    assert_eq!(crowd_step(&Mover { collidable: false, ..m }, 1, 0, true), 0);
    assert_eq!(crowd_step(&Mover { foundation: true, ..m }, 1, 0, true), 0);
    // a small unit of the walker's group is no crowd, a big one is
    assert_eq!(crowd_step(&Mover { active_group: 5, ..m }, 1, 5, true), 0);
    assert_eq!(crowd_step(&Mover { active_group: 5, collision_radius: 1.0, ..m }, 1, 5, true), 150);
    assert_eq!(crowd_step(&Mover { active_group: 5, collision_radius: 1.0, ..m }, 1, 5, false), 50);
    assert_eq!(crowd_step(&Mover { command_type: ACTION_ATTACK_ENEMY_UNIT, ..m }, 1, 0, true), 50);
    assert_eq!(crowd_step(&Mover { command_type: 1, ..m }, 1, 0, true), 0, "moving units are no crowd");
}

#[test]
fn the_step_blockers_at_sea_and_in_space() {
    let m = mover();
    assert!(sea_blocker(&m, 1, 0, 0));
    assert!(!sea_blocker(&Mover { velocity: 1.0, ..m }, 1, 0, 0), "a moving unit");
    assert!(!sea_blocker(&m, 10, 0, 0), "the walker");
    assert!(!sea_blocker(&Mover { domains: 2, ..m }, 1, 0, 0), "not sea-going");
    assert!(!sea_blocker(&Mover { active_group: 3, ..m }, 1, 3, 0), "its own group");
    // a naval building with air traffic blocks in any group
    let yard = Mover { active_group: 3, category: CATEGORY_NAVAL_BUILDING, air_traffic_type: 1, ..m };
    assert!(sea_blocker(&yard, 1, 3, 0));
    // submerged against surfaced do not block each other
    assert!(!sea_blocker(&Mover { category: CATEGORY_SUBMERGED_NAVAL, ..m }, 1, 0, 0));
    assert!(sea_blocker(&Mover { category: CATEGORY_SUBMERGED_NAVAL, ..m }, 1, 0, CATEGORY_SEA_ANIMAL));
    let ship = Mover { domains: MOVEMENT_SPACE, ..m };
    assert!(land_blocker(&ship, 1, 0));
    assert!(!land_blocker(&m, 1, 0), "not space-going");
    assert!(!land_blocker(&Mover { active_group: 3, ..ship }, 1, 3));
    assert!(land_blocker(&Mover { active_group: 3, ..ship }, 1, 4));
}

#[test]
fn a_quiet_view_notes_nothing_a_loud_one_notes_its_answer() {
    crate::astar::search_note::TEST_NOTES.with(|n| n.borrow_mut().clear());
    let quiet = EeAStarWorldView { map: std::ptr::null_mut(), quiet: true };
    let loud = EeAStarWorldView { map: std::ptr::null_mut(), quiet: false };
    assert_eq!(note(&quiet, Q_TERRAIN, 1, 2, 5, 0, 0), 5);
    assert_eq!(note(&loud, Q_FIRST_BLOCKER, 3, 4, 1, 9, 2), 1);
    let notes = crate::astar::search_note::TEST_NOTES.with(|n| n.borrow().clone());
    assert_eq!(notes, vec![(EV_VIEW, [Q_FIRST_BLOCKER, 3, 4, 9, 2], 1)]);
}
