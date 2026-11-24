use super::*;
use crate::units::uc_mock::Mock;

impl UckRust for Mock {
    fn footprint(&mut self, which: i32, unit: u64) {
        self.log.push((-7, unit, i64::from(which), 0));
    }
    fn has_sunk(&mut self, unit: u64) -> bool {
        self.log.push((-8, unit, 0, 0));
        unit == 2
    }
}

fn run(h: &mut Mock, row: usize, a: u64, b: i64, c: i64) -> i64 {
    // SAFETY: no point read
    unsafe { dispatch(h, row, 1, 2, a, b, c, std::ptr::null_mut()) }
}

#[test]
fn the_tile_and_region_bodies_follow_the_class_row() {
    let mut h = Mock::default().answer(EE_UNITS_UCK_BODY, 1);
    run(&mut h, EE_UNITS_BODY_ADD_TO_REGION, 9, 0, 0);
    assert_eq!(h.args(-7), [(9, 2, 0)]);
    let mut h = Mock::default().answer(EE_UNITS_UCK_BODY, 2);
    run(&mut h, EE_UNITS_BODY_ADD_TO_REGION, 9, 0, 0);
    assert_eq!(h.ops(), [EE_UNITS_UCK_BODY]);
    let mut h = Mock::default();
    run(&mut h, EE_UNITS_BODY_ADD_TO_REGION, 9, 0, 0);
    assert_eq!(h.args(EE_UNITS_UCK_D_BASE_ADD_TO_REGION), [(9, 0, 0)]);
    let mut h = Mock::default().answer(EE_UNITS_UCK_BODY, 5);
    run(&mut h, EE_UNITS_BODY_ADD_TO_TILE, 9, 0, 0);
    assert!(h.has(EE_UNITS_UCK_D_SUBMERGED_ADD_TO_TILE));
    let mut h = Mock::default().answer(crate::units::uc_rest::EE_UNITS_UC_R_RTS_TILE_OF, 4);
    run(&mut h, EE_UNITS_BODY_REMOVE_FROM_TILE, 9, 0, 0);
    assert_eq!(h.args(crate::units::uc_rest::EE_UNITS_UC_R_RTS_TILE_REMOVE), [(4, 9, 0)]);
}

#[test]
fn cargo_counts_the_population_unless_kept() {
    let mut h = Mock::default().answer(EE_UNITS_UCK_BODY, 1);
    run(&mut h, EE_UNITS_BODY_CARGO_ADDED, 7, 1, 0);
    assert!(!h.has(EE_UNITS_UCK_D_POPULATION_SUB));
    run(&mut h, EE_UNITS_BODY_CARGO_ADDED, 7, 0, 0);
    assert_eq!(h.args(EE_UNITS_UCK_D_POPULATION_SUB), [(7, 0, 0)]);
    run(&mut h, EE_UNITS_BODY_CARGO_UNLOADED, 7, 0, 0);
    assert!(h.has(EE_UNITS_UCK_D_POPULATION_ADD));
}

#[test]
fn creation_passes_its_arguments_to_the_chosen_body() {
    let mut h = Mock::default().answer(EE_UNITS_UCK_BODY, 3).answer(EE_UNITS_UCK_D_SPACE_BUILDING_CREATE_UNIT, 40);
    assert_eq!(run(&mut h, EE_UNITS_BODY_CREATE_UNIT, 5, 6, 7), 40);
    assert_eq!(h.args(EE_UNITS_UCK_D_SPACE_BUILDING_CREATE_UNIT), [(5, 6, 7)]);
    let mut h = Mock::default().answer(EE_UNITS_UCK_D_BASE_CREATE_UNIT_AT, 41);
    assert_eq!(run(&mut h, EE_UNITS_BODY_CREATE_UNIT_AT, 5, 6, 7), 41);
}

#[test]
fn the_plain_answers() {
    let mut h = Mock::default().answer(EE_UNITS_UCK_BODY, 2).answer(EE_UNITS_UCK_D_DOMAINS, 3);
    assert_eq!(run(&mut h, EE_UNITS_BODY_FLIES_OVER, 0, 0, 0), 1);
    let mut h = Mock::default().answer(EE_UNITS_UCK_BODY, 1);
    assert_eq!(run(&mut h, EE_UNITS_BODY_GET_UNLOAD_FAILED_TEXT_ID, 0, 0, 0), 0x7c39);
    assert_eq!(run(&mut h, EE_UNITS_BODY_IS_LAND_BUILDING, 0, 0, 0), 1);
    assert_eq!(run(&mut h, EE_UNITS_BODY_CHECK_WATER_IMPACT, 0, 0, 0), 0);
    assert_eq!(run(&mut h, EE_UNITS_BODY_IS_PICKABLE, 0, 0, 0), 0);
    let mut h = Mock::default().answer(EE_UNITS_UCK_BODY, 2).answer(EE_UNITS_UC_THIS_UNIT_TYPE, UNIT_TYPE_VOLCANO);
    assert_eq!(run(&mut h, EE_UNITS_BODY_IS_SELECTABLE, 0, 0, 0), 0);
    let mut h = Mock::default().answer(EE_UNITS_UCK_BODY, 1).answer(EE_UNITS_UCK_D_UNIT_TYPE_OF, UNIT_TYPE_PATH_POINT);
    assert_eq!(run(&mut h, EE_UNITS_BODY_IS_SELECTABLE, 8, 0, 0), 1);
    let mut h = Mock::default().answer(EE_UNITS_UCK_BODY, 1).answer(EE_UNITS_UCK_D_FIRST_PART_COMMAND, ACTION_CLOSED);
    assert_eq!(run(&mut h, EE_UNITS_BODY_IS_PART_LEFT_OUT, 0, 0, 0), 0);
    let mut h = Mock::default();
    assert_eq!(run(&mut h, EE_UNITS_BODY_IS_PICKABLE, 0, 0, 0), 1);
}

#[test]
fn the_ground_location_lifts_or_drops_the_point() {
    let mut p = [1.0f32, 2.0, -3.0];
    let mut h = Mock::default().answer(EE_UNITS_UCK_BODY, 2);
    // SAFETY: a local point
    unsafe { dispatch(&mut h, EE_UNITS_BODY_GROUND_LOCATION, 1, 0, 0, 0, 0, p.as_mut_ptr()) };
    assert_eq!(p[2], 0.0);
    let mut h = Mock::default().answer(EE_UNITS_UCK_BODY, 3).float(EE_UNITS_UC_R_CRUISE_ALTITUDE, [9.0, 0.0, 0.0, 0.0]);
    // SAFETY: a local point
    unsafe { dispatch(&mut h, EE_UNITS_BODY_GROUND_LOCATION, 1, 0, 0, 0, 0, p.as_mut_ptr()) };
    assert_eq!(p[2], 9.0);
}

#[test]
fn unit_creation_runs_the_kind_body_or_the_base() {
    let mut h = Mock::default().answer(EE_UNITS_UCK_BODY, 9);
    run(&mut h, EE_UNITS_BODY_UNIT_CREATED, 0, 0, 0);
    assert!(h.has(EE_UNITS_UCK_D_LAND_CREATED));
    let mut h = Mock::default().answer(EE_UNITS_UCK_BODY, 14);
    run(&mut h, EE_UNITS_BODY_UNIT_CREATED, 0, 0, 0);
    assert!(h.has(EE_UNITS_UCK_D_TRANSPORT_CREATED));
    let mut h = Mock::default();
    run(&mut h, EE_UNITS_BODY_UNIT_CREATED, 0, 0, 0);
    assert!(h.has(crate::units::uc_create::EE_UNITS_UC_CLASS_ACTION_CHANGED));
}

#[test]
fn the_air_sound_hands_its_out_pointers() {
    let mut h = Mock::default().answer(EE_UNITS_UCK_BODY, 6);
    let (mut v, mut p, mut q) = (1.0f32, 2.0f32, 3.0f32);
    let outs = SoundOuts { volume: &mut v, pan: &mut p, pitch: &mut q };
    // SAFETY: local outs
    unsafe { sound(&mut h, 1, 2, 3, outs) };
    assert_eq!(h.args(EE_UNITS_UCK_D_AIR_SOUND).len(), 1);
    assert_eq!(h.args(EE_UNITS_UCK_D_AIR_SOUND)[0].0, 2);
}

#[test]
fn the_dock_sinks_by_the_trigger_test() {
    let mut h = Mock::default().answer(EE_UNITS_UCK_BODY, 1);
    assert_eq!(run(&mut h, EE_UNITS_BODY_HAS_SUNK, 0, 0, 0), 1);
    assert_eq!(h.args(-8), [(2, 0, 0)]);
    let mut h = Mock::default().answer(EE_UNITS_UCK_BODY, 2);
    assert_eq!(run(&mut h, EE_UNITS_BODY_HAS_SUNK, 0, 0, 0), 1);
    assert!(h.args(-8).is_empty());
    let mut h = Mock::default();
    assert_eq!(run(&mut h, EE_UNITS_BODY_HAS_SUNK, 0, 0, 0), 0);
}
