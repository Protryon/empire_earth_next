//! The lobby's game type and map type events against the real C++ (`golden_lobby3.txt`): a scratch driver
//! (`build/sc3/lobby3_driver.cpp`, round 2's fakes) ran the unmodified `EEUIELobbyEvents.cpp`'s
//! `EEUIELEGameType::ProcessEvent` and `EEUIELEGameMapType::ProcessEvent` over random options and box lines (none
//! included): 2,000 cases. The replay rebuilds each case's lines over this module: the lock, the load screen's
//! push and its cancel event's load type, the messages and the options after; the redraw a random map's line runs
//! (`LobbyShowGameOptions` and the rows, round 2's goldens) is taken from the golden as it is. The driver relinked
//! over the forwarders prints the same file, and the same on seeds 2-4 at ten times the size.

use super::*;

const GOLDEN: &str = include_str!("../golden_lobby3.txt");

#[test]
fn replays_the_cpp_golden() {
    let lines: Vec<&str> = GOLDEN.lines().collect();
    let starts: Vec<usize> = (0..lines.len()).filter(|&i| lines[i].starts_with("case ")).collect();
    assert_eq!(starts.len(), 2000);
    let mut actions = [0usize; 5];
    for (n, &a) in starts.iter().enumerate() {
        let b = starts.get(n + 1).copied().unwrap_or(lines.len());
        let case = &lines[a..b];
        let t: Vec<&str> = case[0].split(' ').collect();
        let game_type = t[3] == "1";
        let selection = t[7].parse::<i64>().unwrap() as c_ulong;
        let options = case[1];
        let o: Vec<&str> = options.split(' ').collect();
        let load_type: c_ulong = o[4].parse().unwrap();
        let map = *o.last().unwrap();
        let mut out = vec![case[0].to_string(), options.to_string()];
        let mut after = options.replacen("  options ", "  after ", 1);
        let (mut cancel, mut forms) = (77, 1);
        if selection != c_ulong::MAX {
            out.push("  writelock".into());
            if game_type {
                let action = game_type_action(selection, load_type);
                actions[action as usize] += 1;
                match action {
                    EE_SCREENS_GAME_TYPE_SCENARIO | EE_SCREENS_GAME_TYPE_SAVED_GAME => {
                        let kind = if action == EE_SCREENS_GAME_TYPE_SCENARIO { "scenario" } else { "saved" };
                        out.push(format!("  openevent {kind}"));
                        out.push("  push".into());
                        out.push("  trigger 2 1008".into());
                        cancel = load_type;
                        forms = 2;
                    }
                    EE_SCREENS_GAME_TYPE_RANDOM_MAP => {
                        out.push("  message 0 0 ".into());
                        out.push("  send 255".into());
                        // the redraw: as the golden has it, up to the unlock
                        let unlock = case.iter().rposition(|l| *l == "  writeunlock").unwrap();
                        out.extend(case[out.len()..unlock].iter().map(|l| l.to_string()));
                        after = after.replacen(&format!(" load {load_type} "), " load 0 ", 1);
                    }
                    _ => {}
                }
            } else {
                let name = format!("M{}", selection % 4);
                if map_type_changed(name.as_bytes(), map.as_bytes()) {
                    actions[4] += 1;
                    out.push(format!("  message 2 {selection} {name}"));
                    out.push("  send 255".into());
                    after = format!("{} {name}", &after[..after.len() - map.len() - 1]);
                }
            }
            out.push("  writeunlock".into());
        }
        out.push(format!("  cancel {cancel} forms {forms}"));
        out.push(after);
        for (k, (x, y)) in out.iter().zip(case).enumerate() {
            assert_eq!(x, y, "case {n} line {k}");
        }
        assert_eq!(out.len(), case.len(), "case {n}");
    }
    assert!(actions.iter().all(|&c| c > 50), "{actions:?}");
}

#[test]
fn game_type_actions() {
    assert_eq!(game_type_action(0, 0), EE_SCREENS_GAME_TYPE_NONE);
    assert_eq!(game_type_action(0, 2), EE_SCREENS_GAME_TYPE_RANDOM_MAP);
    assert_eq!(game_type_action(1, 0), EE_SCREENS_GAME_TYPE_SAVED_GAME);
    assert_eq!(game_type_action(2, 1), EE_SCREENS_GAME_TYPE_SCENARIO);
    assert_eq!(game_type_action(3, 1), EE_SCREENS_GAME_TYPE_NONE);
    assert!(map_type_changed(b"M1", b"M0"));
    assert!(!map_type_changed(b"M1", b"M1"));
}
