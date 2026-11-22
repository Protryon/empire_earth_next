use super::*;
use crate::units::uc_mock::Mock;

#[test]
fn the_constructor_references_each_effect_found_before_storing_it() {
    let mut h = Mock::default().answer(EE_UNITS_UC_RECORD_ATTRIBUTES, 2).sequence(EE_UNITS_UC_RECORD_EFFECT, &[0, 7]);
    construct(&mut h, 1, 2, 1, 3);
    assert_eq!(h.args(EE_UNITS_UC_RESEARCH_SET).iter().map(|e| e.1).collect::<Vec<_>>(), [0, 1]);
    assert_eq!(h.count(EE_UNITS_UC_SET_EFFECT), EFFECTS as usize);
    assert_eq!(h.args(EE_UNITS_UC_EFFECT_REFERENCE), [(7, 0, 0)]);
    assert_eq!(h.args(EE_UNITS_UC_SET_EFFECT)[1], (7, 1, 0));
    assert_eq!(h.count(EE_UNITS_UC_CLEAR_RELATED), 6);
    assert!(h.has(EE_UNITS_UC_CLEAR_AI_SCRIPT));
    assert!(!h.has(EE_UNITS_UC_LOS_WARNING));
    assert!(h.at(EE_UNITS_UC_CLASS_FILL) < h.at(EE_UNITS_UC_SET_CLASS_BEHAVIOR));
}

#[test]
fn the_fill_input_reads_the_animal_the_epoch_and_the_calamity_powers() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_FIND_ANIMAL, 4)
        .answer(EE_UNITS_UC_ANIMAL_ID, 77)
        .answer(EE_UNITS_UC_RECORD_TECH_NODE, 5)
        .answer(EE_UNITS_UC_NODE_EPOCH, 9)
        .answer(EE_UNITS_UC_NUM_RESEARCH, 3)
        .sequence(EE_UNITS_UC_RESEARCH_TECH_NODE, &[6, 8, 9])
        .sequence(EE_UNITS_UC_NODE_KIND, &[1, 5])
        .answer(EE_UNITS_UC_SCRIPT_NAME_LENGTH, 4)
        .answer(EE_UNITS_UC_AI_SCRIPT, 66);
    construct(&mut h, 1, 2, 0, 0);
    assert_eq!(h.args(EE_UNITS_UC_ANIMAL_ID), [(4, 0, 0)]);
    assert_eq!(h.args(EE_UNITS_UC_NODE_EPOCH), [(5, 0, 0)]);
    assert!(h.at(EE_UNITS_UC_AI_SCRIPT) < h.at(EE_UNITS_UC_CLASS_FILL));
    // the search stops at the first calamity technology
    assert_eq!(h.count(EE_UNITS_UC_RESEARCH_TECH_NODE), 2);
    assert!(h.has(EE_UNITS_UC_SET_AI_SCRIPT));
}

#[test]
fn classes_are_built_by_category_and_linked() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_DB_COUNT, 2)
        .answer(EE_UNITS_UC_DB_FILE, 1)
        .answer(EE_UNITS_UC_RECORD_ENABLED, 1)
        .sequence(EE_UNITS_UC_DB_RECORD, &[10, 11, 10, 11, 10, 11])
        .sequence(EE_UNITS_UC_RECORD_TYPE, &[5, 900, 5, 900, 5, 5, 900, 900])
        .sequence(EE_UNITS_UC_RECORD_CATEGORY, &[2, 0x40])
        .answer(EE_UNITS_UC_NEW_CLASS, 20)
        .answer(EE_UNITS_UC_CLASS_AT, 20)
        .sequence(EE_UNITS_UC_RECORD_RELATED, &[0, 3]);
    create_unit_classes(&mut h);
    assert_eq!(
        h.args(EE_UNITS_UC_VERIFY).iter().map(|e| (e.1, e.2)).collect::<Vec<_>>(),
        [(1, 5710), (1, 5711), (1, 5713)]
    );
    assert!(h.has(EE_UNITS_UC_DB_DELETE));
    assert_eq!(h.args(EE_UNITS_UC_TABLE_ASSIGN), [(0, 901, 0)]);
    // the building category's model 8; an unknown category registers no class
    assert_eq!(h.args(EE_UNITS_UC_NEW_CLASS), [(10, 8, 0)]);
    assert_eq!(h.args(EE_UNITS_UC_REGISTER_CLASS), [(20, 0, 0), (0, 0, 0)]);
    assert_eq!(h.count(EE_UNITS_UC_CLASS_DEREFERENCE), 1);
    assert_eq!(h.args(EE_UNITS_UC_SET_RELATED), [(20, 1, 3)]);
    assert_eq!(h.count(EE_UNITS_UC_SET_IS_WALL), 2);
    assert_eq!(h.count(EE_UNITS_UC_SET_SKIPS_LINE_OF_FIRE), 2);
    assert!(h.at(EE_UNITS_UC_DB_CLEAR) < h.at(EE_UNITS_UC_SORT_TYPES));
}
