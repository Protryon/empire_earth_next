use super::*;

fn slot(kind: c_ulong, n: c_ulong, local: bool) -> EeScreensLobbySlot {
    EeScreensLobbySlot { kind, slot: n, is_local: local, ..Default::default() }
}

/// The random map setup of a single-player game: the host's own human (slot 0, ready) and a computer (slot 1).
fn single_player() -> EeScreensLobbyContext {
    EeScreensLobbyContext { server: true, multiplayer: false, load_type: 0, host_ready: false }
}

#[test]
fn every_selected_line_exists_and_open_and_computer_map_back() {
    for menu in 1..=5 {
        let lines = player_type_lines(menu);
        for kind in 0..5 {
            let line = player_type_line(menu, kind).unwrap();
            assert!((line as usize) < lines.len(), "menu {menu} kind {kind}");
        }
        // an open or a computer line is that type again
        for (i, &l) in lines.iter().enumerate() {
            let back = player_type_from_menu(i as c_ulong, menu);
            match l {
                EE_SCREENS_LOBBY_LINE_OPEN => assert_eq!(back, 3),
                EE_SCREENS_LOBBY_LINE_COMPUTER => assert_eq!(back, 0),
                EE_SCREENS_LOBBY_LINE_CLOSED => assert_eq!(back, 4),
                EE_SCREENS_LOBBY_LINE_OBSERVE => assert_eq!(back, 2),
                _ => assert_eq!(back, 1),
            }
        }
    }
    assert_eq!(player_type_from_menu(3, 5), 5);
    assert_eq!(player_type_from_menu(0, 6), 5);
    assert_eq!(player_type_line(0, 1), None);
    assert!(player_type_lines(7).is_empty());
}

#[test]
fn the_single_player_setup() {
    let c = single_player();
    let mut human = slot(1, 0, true);
    human.is_ready = true;
    let r = host_row(&human, &c);
    assert_eq!((r.menu, r.text, r.controls), (3, EE_SCREENS_LOBBY_TEXT_NAME, [1, 1, 1, 1]));
    let computer = slot(0, 1, false);
    let r = host_row(&computer, &c);
    assert_eq!((r.menu, r.text, r.controls), (1, EE_SCREENS_LOBBY_TEXT_COMPUTER, [2, 1, 2, 1]));
    // the host edits the computer's team, citizens and engine; nobody's ready box shows in a single-player game
    let p = player_plan(&computer, &c);
    assert_eq!((p.ready, p.team, p.citizens, p.engine, p.ping), ([0, 0], [1, 0], [1, 0], [1, 0], 0));
    let p = player_plan(&human, &c);
    assert_eq!((p.ready, p.colour, p.team), ([0, 0], [0, 1], [0, 1]));
    // the computer's type box is the host's to change
    let n = name_plan(&computer, &c);
    assert_eq!((n.box_mode, n.label_mode, n.colour, n.box_dirty), (1, 0, EE_SCREENS_LOBBY_WHITE, true));
    let closed = slot(4, 5, false);
    assert_eq!(host_row(&closed, &c).text, EE_SCREENS_LOBBY_TEXT_CLOSED);
}

#[test]
fn rows_past_eight_never_show() {
    let c = single_player();
    let computer = slot(0, 9, false);
    let p = player_plan(&computer, &c);
    assert_eq!((p.team, p.citizens, p.engine), ([0, 0], [0, 0], [0, 0]));
    assert_eq!(name_plan(&computer, &c).box_mode, 0);
}

#[test]
fn a_saved_games_open_slots_are_coloured_by_their_old_owner() {
    let c = EeScreensLobbyContext { load_type: 2, ..single_player() };
    let mut open = slot(3, 2, false);
    open.loaded_type = 1;
    let n = name_plan(&open, &c);
    assert_eq!((n.colour, n.box_dirty, n.box_mode, n.label_mode), (EE_SCREENS_LOBBY_RED, false, 0, 1));
    open.loaded_type = 0;
    assert_eq!(name_plan(&open, &c).colour, EE_SCREENS_LOBBY_YELLOW);
    assert_eq!(host_row(&open, &c).text, EE_SCREENS_LOBBY_TEXT_NAME);
    let observer = slot(2, 3, false);
    assert_eq!(name_plan(&observer, &c).colour, EE_SCREENS_LOBBY_GREEN);
}

#[test]
fn a_scenarios_rows() {
    let c = EeScreensLobbyContext { load_type: 1, host_ready: true, ..single_player() };
    let open = slot(3, 4, false);
    let r = host_row(&open, &c);
    assert_eq!((r.menu, r.text, r.controls), (4, EE_SCREENS_LOBBY_TEXT_COMPUTER, [2, 2, 2, 2]));
    let mut human = slot(1, 2, false);
    assert_eq!(host_row(&human, &c).menu, 5);
    human.b36 = true;
    assert_eq!(host_row(&human, &c).menu, 1);
    // a ready scenario host's open slot plays as a computer: its team and citizens show as labels
    let p = player_plan(&open, &c);
    assert_eq!((p.team, p.citizens), ([0, 1], [0, 1]));
}

#[test]
fn a_clients_rows() {
    let c = EeScreensLobbyContext { server: false, multiplayer: true, load_type: 0, host_ready: false };
    let me = slot(1, 3, true);
    let r = client_row(&me, &c);
    assert_eq!((r.menu, r.controls), (3, [1, 1, 1, 0]));
    let other = slot(1, 4, false);
    assert_eq!(client_row(&other, &c).controls, [0, 0, 0, 0]);
    assert_eq!(name_plan(&me, &c).box_mode, 1); // not ready yet: the own type box
    assert_eq!(name_plan(&other, &c).box_mode, 0);
    assert_eq!(player_plan(&other, &c).ping, 1);
    assert_eq!(player_plan(&me, &c).ready, [1, 0]);
}

#[test]
fn pairs_and_engines() {
    assert_eq!([row_pair(0), row_pair(1), row_pair(2), row_pair(c_ulong::MAX)], [[1, 0], [0, 1], [0, 0], [0, 0]]);
    assert_eq!([engine_choice(1, 4), engine_choice(4, 4), engine_choice(9, 2)], [1, 0, 0]);
}
