//! campaign_model_calls.rs over the recording fake: each op's C++ calls, in the C++ case's order.

use super::*;
use crate::screens::game_cpp::Fake;

#[test]
fn the_fields_and_the_scenarios() {
    let mut f = Fake::default();
    call(&mut f, EE_SCREENS_CEM_SET_FIELD, EE_SCREENS_CE_FIELD_EMAIL, b"a@b");
    call(&mut f, EE_SCREENS_CEM_SET_SCENARIOS, 0, b"one\0two\0three");
    call(&mut f, EE_SCREENS_CEM_LOAD, 0, b"x");
    assert_eq!(
        f.calls(),
        "cem_set_email(\"a@b\")\ncem_scenarios_clear()\ncem_scenarios_add(\"one\")\ncem_scenarios_add(\"two\")\n\
         cem_set_scenarios()\ncem_kept_clear()\ncem_load(\"Campaigns\\\\x\")"
    );
}

#[test]
fn the_files() {
    let mut f = Fake::default();
    f.answer("cem_delete", &[1]);
    assert_eq!(call(&mut f, EE_SCREENS_CEM_DELETE, 0, b"c.eec"), 1);
    call(&mut f, EE_SCREENS_CEM_LIST_FILES, 1, b"");
    assert_eq!(
        f.calls(),
        "cem_delete_reset()\ncem_delete_volume()\ncem_delete_directory()\ncem_delete_file(\"c.eec\")\ncem_delete()\n\
         cem_directory_volume()\ncem_directory_scenarios()\ncem_path_add(1)\ncem_list_scenario_files()"
    );
}

#[test]
fn the_check_sums_the_archived_copy() {
    let mut f = Fake::default();
    f.out("cem_file_length", 3).answer("cem_file_byte", &[1, 2, 3]).answer("cem_file_checksum", &[7]);
    assert_eq!(call(&mut f, EE_SCREENS_CEM_CHECK, 0, b"s"), 1);
    let calls = f.calls();
    assert!(calls.contains("cem_differs_format()\ncem_differs_print(\"s\")\ncem_differs_show()"));
    assert!(calls.ends_with("cem_file_close()\ncem_file_delete()\ncem_file_clear()\ncem_archive_close(0)\ncem_archive_delete(0)"));
    let mut f = Fake::default();
    f.out("cem_file_length", 2).answer("cem_file_byte", &[3, 4]).answer("cem_file_checksum", &[7]);
    assert_eq!(call(&mut f, EE_SCREENS_CEM_CHECK, 0, b"s"), 0);
    let mut f = Fake::default();
    f.answer("cem_find_campaign", &[1]);
    assert_eq!(call(&mut f, EE_SCREENS_CEM_CHECK, 0, b"s"), 0);
    assert_eq!(f.calls(), "cem_find_campaign()");
}

#[test]
fn the_texts() {
    let mut f = Fake::default();
    f.answer("cem_line_count", &[2]).text("cem_line", b"a").text("cem_line", b"bc");
    assert_eq!(text(&mut f, EE_SCREENS_CEM_TEXT_LINES, 0), b"a\0bc\0");
    f.text("cem_scenario_extension", b".scn");
    assert_eq!(text(&mut f, EE_SCREENS_CEM_TEXT_EXTENSION, 1), b".scn");
}
