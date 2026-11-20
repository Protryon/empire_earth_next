//! main_group_calls.rs over the recording fake: the host's cases, the hot keys and ShowCommands' calls.

use super::*;
use crate::screens::game_cpp::Fake;

const GROUP: usize = 0x80;

#[test]
fn the_host_cases() {
    let mut f = Fake::default();
    f.answer("mg_control", &[0, 5]);
    call(&mut f, GROUP, EE_SCREENS_MGR_OP_ICON_BAR, 0, 0, 0);
    call(&mut f, GROUP, EE_SCREENS_MGR_OP_ICON_BAR, 0, 0, 0);
    call(&mut f, GROUP, EE_SCREENS_MGR_OP_BOX, EE_SCREENS_MGR_BOX_QUEUE, 9, 0);
    call(&mut f, GROUP, EE_SCREENS_MGR_OP_CLOAK, 0, 9, 0);
    assert_eq!(
        f.calls(),
        "mg_control(128, 222)\nmg_control(128, 222)\nmg_icon_bar_update(5)\nmg_box_queued(128, 9)\nmg_remove_cloak(9)"
    );
    let mut f = Fake::default();
    f.answer("mg_unit_behavior_is", &[0, 0, 1]);
    assert_eq!(call(&mut f, GROUP, EE_SCREENS_MGR_OP_BEHAVIOR, 9, 0, 0), 2);
    assert_eq!(call(&mut Fake::default(), GROUP, EE_SCREENS_MGR_OP_BEHAVIOR, 9, 0, 0), -1);
    let mut f = Fake::default();
    f.out("mg_minutes_seconds", 3).out("mg_minutes_seconds", 7);
    call(&mut f, GROUP, EE_SCREENS_MGR_OP_TIMER_TEXT, 208, 1000, 0);
    assert_eq!(
        f.calls(),
        "mg_minutes_seconds(1000, out, out)\nmg_show(128, 208)\nmg_timer_text(128, 208, \"3:07\")"
    );
}

#[test]
fn the_hot_keys() {
    let mut f = Fake::default();
    f.answer("mg_hot_key_copy", &[50]).answer("mg_hot_key_event_new", &[61, 62]);
    hot_key(&mut f, GROUP, 40, 30, 2);
    assert_eq!(
        f.calls(),
        "mg_hot_key_copy(128, 40)\nmg_hot_key_shift(50)\nmg_hot_key_event_new(30, true)\n\
         mg_hot_key_pressed_event(50, 61)\nevent_dereference(61)\nmg_hot_key_event_new(30, false)\n\
         mg_hot_key_unpressed_event(50, 62)\nevent_dereference(62)\nmg_add_hot_key(128, 50)\nmg_hot_key_delete(50)"
    );
    let mut f = Fake::default();
    f.answer("mg_event_button", &[30, 30]).answer("mg_event_pressed", &[1, 0]);
    hot_key_event(&mut f, 1);
    hot_key_event(&mut f, 1);
    assert_eq!(
        f.calls(),
        "mg_event_button(1)\nmg_event_pressed(1)\nmg_button_hot_key_down(30)\nmg_saved_groups_reset()\n\
         mg_event_button(1)\nmg_event_pressed(1)\nsbt_pressed(30)"
    );
}

#[test]
fn show_commands_hides_everything_without_a_unit() {
    let mut f = Fake::default();
    let mut members = Vec::new();
    show_commands(&mut f, GROUP, 0, |w, u, b| members.push((w, u, b)));
    // 96 + 9 buttons hidden and released, the three rallies hidden, then the gate fails
    assert_eq!(f.log.iter().filter(|l| l.starts_with("mg_hide(")).count(), 96 + 9 + 3);
    assert_eq!(f.log.iter().filter(|l| l.starts_with("sbt_object(")).count(), 96 + 9);
    assert!(f.calls().ends_with("mg_current_player()\nmg_player_observer(0)\nmg_cancel_hidden(128)"));
    assert!(members.is_empty());
}

#[test]
fn show_commands_lays_out_a_unit() {
    let mut f = Fake::default();
    f.answer("mg_unit_complex", &[7]).answer("mg_current_player", &[3]).answer("mg_unit_player", &[3]);
    f.answer("mg_complex_built", &[1]).answer("mg_unit_alive", &[1]).answer("mg_complex_commandable", &[1]);
    f.answer("mg_cancel_hidden", &[1]).answer("mg_player_human", &[1]).answer("mg_mode", &[0]);
    f.answer("mg_complex_airport", &[1]);
    let mut members = Vec::new();
    show_commands(&mut f, GROUP, 9, |w, u, b| members.push((w, u, b)));
    assert!(f.log.contains(&"mg_class_command(9, 8)".to_string()));
    assert_eq!(members.last(), Some(&(EE_SCREENS_MGR_DO_SPECIAL_BUTTONS, 9, 0)));
}
