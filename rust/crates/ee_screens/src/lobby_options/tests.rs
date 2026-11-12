use super::*;

fn view() -> EeScreensLobbyOptionsView {
    EeScreensLobbyOptionsView { server: true, unit_minimum: 50, unit_step: 50, unit_limit: 200, ..Default::default() }
}

#[test]
fn the_host_before_ready_edits_a_random_map() {
    let p = show_options(&view());
    assert!(p.editable);
    assert_eq!(p.map_rows, EE_SCREENS_LOBBY_ROW_EDIT);
    assert_eq!(p.unit_line, 3);
    assert_eq!(p.cheats_row, EE_SCREENS_LOBBY_ROW_EDIT);
    let ready = show_options(&EeScreensLobbyOptionsView { host_ready: true, ..view() });
    assert!(!ready.editable);
    assert_eq!(ready.map_rows, EE_SCREENS_LOBBY_ROW_LABEL);
    let loaded = show_options(&EeScreensLobbyOptionsView { load_type: 2, ..view() });
    assert_eq!(loaded.map_rows, EE_SCREENS_LOBBY_ROW_HIDDEN);
    assert!(loaded.scenario_info && !loaded.editable);
    assert_eq!((loaded.type_line, loaded.type_string), (1, 1));
    let multiplayer = show_options(&EeScreensLobbyOptionsView { options_multiplayer: true, ..view() });
    assert_eq!(multiplayer.cheats_row, EE_SCREENS_LOBBY_ROW_KEEP);
}

#[test]
fn game_type_lines_and_strings() {
    assert_eq!([0, 1, 2, 3].map(game_type_line), [0, 2, 1, 0]);
    assert_eq!([0, 1, 2, 3].map(game_type_string), [0, 2, 1, EE_SCREENS_LOBBY_NO_STRING]);
    // the client never sets the box; the server only when it differs
    let v = EeScreensLobbyOptionsView { type_selected: true, ..view() };
    assert!(!show_options(&v).set_type_line);
    assert!(show_options(&EeScreensLobbyOptionsView { type_selected: false, ..view() }).set_type_line);
    assert!(!show_options(&EeScreensLobbyOptionsView { server: false, ..v }).set_type_line);
}

#[test]
fn unit_limit_lines_wrap_unsigned() {
    assert_eq!(unit_limit_line(275, 50, 25), 9);
    assert_eq!(unit_limit_line(40, 50, 50), (40 as c_ulong).wrapping_sub(50) / 50);
    assert_eq!(unit_limit_line(40, 50, 0), 0);
}

#[test]
fn variants_count_the_selectable_ones() {
    let s = [true, false, true, true, false];
    assert_eq!(variant_line(&s, 3), 2);
    assert_eq!(variant_line(&s, 1), 0); // not selectable
    assert_eq!(variant_line(&s, 9), 0);
    assert_eq!(variant_of_line(&s, 2), Some(3));
    assert_eq!(variant_of_line(&s, 3), None);
}

#[test]
fn epoch_start_moves_the_end_up() {
    let mut s = EeScreensLobbyOptionState { epoch_count: 15, ..Default::default() };
    s.current[EE_SCREENS_OPTION_EPOCH_START as usize] = 2;
    s.current[EE_SCREENS_OPTION_EPOCH_END as usize] = 4;
    let c = option_change(EE_SCREENS_OPTION_EPOCH_START, 6, &s, &[]);
    assert!(c.update_epoch_end);
    assert_eq!(c.count, 2);
    assert_eq!(c.messages[1], EeScreensLobbyMessage { option: EE_SCREENS_OPTION_EPOCH_END, value: 6 });
    // random (the epoch count) counts from the first epoch: the end stays
    assert_eq!(option_change(EE_SCREENS_OPTION_EPOCH_START, 15, &s, &[]).count, 1);
    // the end box counts from the start
    let e = option_change(EE_SCREENS_OPTION_EPOCH_END, 3, &s, &[]);
    assert_eq!(e.messages[0].value, 5);
    assert_eq!(option_change(EE_SCREENS_OPTION_EPOCH_END, 2, &s, &[]).count, 0);
}

#[test]
fn unit_limit_and_flags() {
    let mut s = EeScreensLobbyOptionState { unit_minimum: 100, unit_step: 50, ..Default::default() };
    s.current[EE_SCREENS_OPTION_UNIT_LIMIT as usize] = 300;
    assert_eq!(option_change(EE_SCREENS_OPTION_UNIT_LIMIT, 4, &s, &[]).count, 0);
    assert_eq!(option_change(EE_SCREENS_OPTION_UNIT_LIMIT, 5, &s, &[]).messages[0].value, 350);
    let c = option_change(EE_SCREENS_OPTION_REVEAL_MAP, 1, &s, &[]);
    assert_eq!(c.messages[0], EeScreensLobbyMessage { option: 13, value: 1 });
    assert!(option_change(EE_SCREENS_OPTION_DIFFICULTY, 2, &s, &[]).set_difficulty);
    assert_eq!(option_change(99, 2, &s, &[]).count, 0);
}

fn slot(kind: c_ulong, color: c_ulong, team: c_ulong) -> EeScreensLobbyStartSlot {
    EeScreensLobbyStartSlot { kind, color, team, ready: true, transfer: 0 }
}

#[test]
fn start_needs_two_players_and_a_human() {
    let mut s = [slot(3, 0, 0); 16];
    s[0] = slot(1, 0, 0);
    let r = start_check(0, true, false, &s);
    assert_eq!((r.result, r.count, r.messages[0]), (false, 1, EE_SCREENS_START_ONE_PLAYER));
    s[1] = slot(0, 1, 0);
    assert!(start_check(0, true, false, &s).result);
    s[0] = slot(0, 0, 0);
    assert_eq!(start_check(0, true, true, &s).messages[0], EE_SCREENS_START_NEED_HUMAN);
}

#[test]
fn start_colours_teams_ready_and_transfers() {
    let mut s = [slot(3, 0, 0); 16];
    s[0] = slot(1, 2, 1);
    s[1] = slot(1, 2, 1);
    s[2] = slot(1, 2, 1);
    let r = start_check(0, true, false, &s);
    assert_eq!(r.count, 2); // one message per clash; the team check needs a passing result
    s[1].color = 3;
    s[2].color = 4;
    assert_eq!(start_check(0, true, false, &s).messages[0], EE_SCREENS_START_SAME_TEAM);
    s[2].team = 0;
    assert!(start_check(0, true, false, &s).result);
    s[2].ready = false;
    assert_eq!(start_check(0, true, true, &s).messages[0], EE_SCREENS_START_NOT_READY);
    s[2].ready = true;
    s[2].transfer = 1;
    assert_eq!(start_check(0, true, false, &s).messages[0], EE_SCREENS_START_TRANSFER);
    // a loaded game that failed hides the rows
    let l = start_check(2, false, false, &s);
    assert!(!l.result && l.hide_rows && l.count == 0);
}
