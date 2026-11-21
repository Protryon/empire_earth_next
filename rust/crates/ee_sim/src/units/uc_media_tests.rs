use super::*;
use crate::units::uc_mock::Mock;

#[test]
fn class_media_loads_each_present_medium_in_order() {
    let mut h = Mock::default()
        .sequence(EE_UNITS_UC_ACKNOWLEDGE_SOUND, &[5, 0])
        .sequence(EE_UNITS_UC_SELECTION_SOUND, &[0, 6])
        .answer(EE_UNITS_UC_CLASS_MODEL, 7)
        .answer(EE_UNITS_UC_CLASS_BUTTON, 8);
    class_media(&mut h, 1, true);
    assert_eq!(h.args(EE_UNITS_UC_SOUND_LOAD), [(5, 0, 0), (6, 0, 0)]);
    assert_eq!(h.args(EE_UNITS_UC_MODEL_LOAD), [(7, 0, 0)]);
    assert_eq!(*h.ops().last().unwrap(), EE_UNITS_UC_BUTTON_ACQUIRE);
    let mut h = Mock::default().answer(EE_UNITS_UC_CLASS_DEATH_SOUND, 9);
    class_media(&mut h, 1, false);
    assert_eq!(h.args(EE_UNITS_UC_SOUND_UNLOAD), [(9, 0, 0)]);
}

#[test]
fn buttons_skip_missing_nodes_and_load_calamities() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_TECH_TREE, 3)
        .answer(EE_UNITS_UC_RESEARCH_COUNT, 2)
        .sequence(EE_UNITS_UC_RESEARCH_NODE, &[0, 40])
        .answer(EE_UNITS_UC_PLAYER_IS_HUMAN, 1)
        .answer(EE_UNITS_UC_NODE_KIND, 5)
        .answer(EE_UNITS_UC_NODE_CALAMITY, 12)
        .sequence(EE_UNITS_UC_COMMAND, &[1, 2, 0x32, 7]);
    buttons(&mut h, 1, 50, true);
    assert_eq!(h.args(EE_UNITS_UC_NODE_BUTTON_ACQUIRE), [(40, 0, 0)]);
    assert_eq!(h.args(EE_UNITS_UC_CALAMITY_LOAD), [(50, 12, 0)]);
    assert_eq!(h.args(EE_UNITS_UC_COMMAND_ACQUIRE), [(0, 2, 0), (0, 7, 0)]);
}

#[test]
fn an_airport_marks_its_runways_and_flags() {
    let mut h = Mock::default()
        .answer(crate::units::uc_produce::EE_UNITS_UC_THIS_UNIT_TYPE, UNIT_TYPE_AIRPORT)
        .sequence(EE_UNITS_UC_RELATED_CLASS, &[30])
        .answer(EE_UNITS_UC_CLASS_TYPE_FIELD, 0x99)
        .sequence(EE_UNITS_UC_EFFECT_AT, &[0, 4]);
    base_media(&mut h, 1, 50, true);
    assert_eq!(h.args(EE_UNITS_UC_SET_CLASS_MEDIA), [(50, RUNWAY_TYPE, 0), (50, 0x99, 0)]);
    assert_eq!(h.count(EE_UNITS_UC_EFFECT_LOAD_ID), 4);
    assert_eq!(h.args(EE_UNITS_UC_EFFECT_LOAD), [(4, 0, 0)]);
    assert_eq!(h.count(EE_UNITS_UC_EFFECT_AT), 29);
    assert_eq!(h.args(EE_UNITS_UC_SET_MEDIA_LOADED), [(0, 1, 0)]);
}

#[test]
fn unloading_a_granary_unloads_its_farms() {
    let mut h = Mock::default().answer(crate::units::uc_produce::EE_UNITS_UC_THIS_UNIT_TYPE, UNIT_TYPE_GRANARY);
    base_media(&mut h, 1, 50, false);
    assert_eq!(h.args(EE_UNITS_UC_UNLOAD_CLASS_MEDIA), [(50, FARM_TYPE, 0)]);
    assert_eq!(h.args(EE_UNITS_UC_SET_MEDIA_LOADED), [(0, 0, 0)]);
}

#[test]
fn destruction_releases_effects_then_the_related_classes() {
    let mut h =
        Mock::default().sequence(EE_UNITS_UC_EFFECT_AT, &[3]).sequence(EE_UNITS_UC_RELATED_CLASS, &[0, 0, 0, 0, 22]);
    destroy(&mut h, 1);
    assert_eq!(h.args(EE_UNITS_UC_EFFECT_DEREFERENCE), [(3, 0, 0)]);
    assert_eq!(h.args(EE_UNITS_UC_CLASS_DEREFERENCE), [(22, 0, 0)]);
    assert_eq!(h.args(EE_UNITS_UC_CLEAR_RELATED), [(0, 4, 0)]);
    assert!(h.at(EE_UNITS_UC_CLEAR_AI_SCRIPT) < h.at(EE_UNITS_UC_CLASS_DEREFERENCE));
}
