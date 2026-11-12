//! The command panel's layout against the real C++ (`golden_panel.txt`): a scratch driver
//! (`build/sc3/group_driver.cpp`) ran the unmodified `EEUIFMGMainGroup.cpp`'s `ShowCommands` (and its
//! `ShowCommandButton`) over calloc'ed players, a unit and its class (the panel's gate, the five modes, the civ
//! powers, rally points, command slots, research technologies with odd positions and kind 3 commands), button
//! objects and tech nodes without hot keys, no airport: 2,000 cases. The replay rebuilds every line from the printed
//! inputs over this module: the buttons each made a command or tech button in order, and the controls shown. The
//! driver relinked over the forwarders prints the same file, and the same on seeds 2-4 at ten times the size.

use super::*;

const GOLDEN: &str = include_str!("../golden_panel.txt");

fn after<'a>(t: &'a [&'a str], key: &str, count: usize) -> &'a [&'a str] {
    let i = t.iter().position(|&s| s == key).unwrap_or_else(|| panic!("no {key}"));
    &t[i + 1..i + 1 + count]
}
fn l(s: &str) -> c_long {
    s.parse::<i64>().unwrap() as c_long
}

#[test]
fn replays_the_cpp_golden() {
    let lines: Vec<&str> = GOLDEN.lines().collect();
    let starts: Vec<usize> = (0..lines.len()).filter(|&i| lines[i].starts_with("case ")).collect();
    assert_eq!(starts.len(), 2000);
    let mut shown_cases = 0;
    for (n, &a) in starts.iter().enumerate() {
        let b = starts.get(n + 1).copied().unwrap_or(lines.len());
        let case = &lines[a..b];
        let t: Vec<&str> = case[0].split(' ').collect();
        let one = |key: &str| after(&t, key, 1)[0];
        let flag = |key: &str| one(key) == "1";
        let jit: Vec<&str> = one("jit").split('/').collect();
        let observer = flag("observer");
        let category: c_ulong = one("category").parse().unwrap();
        let unit_type: c_ulong = one("type").parse().unwrap();
        let rally_point = (flag("land") && flag("rally"))
            || ((category == 0x15 || category == 0x20) && one("air") != "0")
            || unit_type == 0x2ed;
        let mut commands = [0 as c_ulong; 9];
        for (k, c) in after(&t, "commands", 9).iter().enumerate() {
            commands[k] = c.parse().unwrap();
        }
        let positions: Vec<c_long> = after(&t, "positions", 11).iter().map(|s| l(s)).collect();
        let mut trade_positions = [0 as c_long; 8];
        trade_positions.copy_from_slice(&positions[3..11]);
        let view = EeScreensPanelView {
            unit: !flag("none"),
            complex: true,
            built: flag("built"),
            alive: one("hp") != "0",
            commandable: flag("commandable"),
            owned: one("owner") == "0" && !observer,
            cancel_hidden: one("cancel") == "0",
            player: !observer,
            human: flag("human"),
            mode: l(one("mode")),
            fanatic: flag("fanatic") && flag("set"),
            jit: jit[0] == "1" && rally_point,
            jit_on: jit[1] == "1",
            fanatic_position: positions[0],
            jit_positions: [positions[1], positions[2]],
            trade_positions,
            commands,
        };
        let ri = t.iter().position(|&s| s == "research").unwrap();
        let mut ids = Vec::new();
        let research: Vec<EeScreensPanelResearch> = t[ri + 1..]
            .iter()
            .map(|r| {
                let v: Vec<&str> = r.split('/').collect();
                let f: Vec<bool> = v[4].chars().map(|c| c == '1').collect();
                ids.push(v[0]);
                let position = l(v[1]);
                EeScreensPanelResearch {
                    position,
                    command: l(v[2]),
                    kind: l(v[3]),
                    eligible: (0..10000).contains(&position) && f[0] && (f[1] || f[2]),
                }
            })
            .collect();
        let buttons = panel_buttons(&view, &research);
        let mut out = vec![case[0].to_string(), "  removehotkeys 2".to_string()];
        let mut shown = Vec::new();
        for button in &buttons {
            match button.kind {
                EE_SCREENS_PANEL_TECH => out.push(format!(
                    "  tech {} {} {unit_type} {}",
                    button.control,
                    ids[button.research as usize],
                    one("id")
                )),
                _ => out.push(format!("  command {} {}", button.control, button.button)),
            }
            shown.push(button.control);
        }
        shown.retain(|&c| (0x4e..0x57).contains(&c) || (0x6e..0xce).contains(&c));
        shown.sort_unstable();
        shown.dedup();
        shown_cases += !buttons.is_empty() as usize;
        let mut s = "  shown".to_string();
        for c in shown {
            s += &format!(" {c}");
        }
        out.push(s);
        for (k, (x, y)) in out.iter().zip(case).enumerate() {
            assert_eq!(x, y, "case {n} line {k}");
        }
        assert_eq!(out.len(), case.len(), "case {n}");
    }
    assert!(shown_cases > 600, "{shown_cases}");
}
