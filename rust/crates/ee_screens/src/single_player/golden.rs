//! The name check and the random map against the real C++: scratch drivers (`build/sc2/name_driver.cpp`,
//! `build/sc2/sp_driver.cpp`) ran the unmodified `EEAJoinOrHostAGame.cpp`'s `EEIsValidName` over the real
//! `UWideString` (4,000 random names of letters, spaces, the refused characters, controls, two- to four-byte UTF-8
//! sequences and stray bytes; `golden_names.txt`) and the unmodified `EEUIESinglePlayerEvents.cpp`'s random map over
//! a fake form, server and messages (1,000 cases: the typed name empty or not, the name check's answer, the last
//! game's slots; `golden_single_player.txt`). The replays rebuild every line from the printed inputs over this
//! module with the C++ forwarders' call sequences; both drivers relinked over the forwarders print the same files,
//! and the same on seeds 2-4 at ten times the size.

use super::*;

const NAMES: &str = include_str!("../golden_names.txt");
const SINGLE: &str = include_str!("../golden_single_player.txt");

#[test]
fn replays_the_name_golden() {
    let mut n = 0;
    for line in NAMES.lines() {
        let t: Vec<&str> = line.split(' ').collect();
        let at = t.iter().position(|&x| x == "chars").unwrap();
        let end = t.iter().position(|&x| x == "valid").unwrap();
        let chars: Vec<u8> = t[at + 1..end].iter().map(|x| u8::from_str_radix(x, 16).unwrap()).collect();
        let expect = t[end + 1] == "1";
        assert_eq!(valid_name(&chars), expect, "{line}");
        n += 1;
    }
    assert_eq!(n, 4000);
}

/// The default human name (`IDS_SE_HUMAN`) and the computer's (`IDS_COMPUTER`), as the driver's strings.
const HUMAN: &str = "S20552";
const COMPUTER: &str = "S30241";

#[test]
fn replays_the_single_player_golden() {
    let lines: Vec<&str> = SINGLE.lines().collect();
    let starts: Vec<usize> = (0..lines.len()).filter(|&i| lines[i].starts_with("case ")).collect();
    assert_eq!(starts.len(), 1000);
    let mut player = "Prev".to_string();
    for (n, &a) in starts.iter().enumerate() {
        let head = lines[a];
        let name = &head[head.find('\'').unwrap() + 1..head.rfind('\'').unwrap()];
        let rest: Vec<&str> = head[head.rfind('\'').unwrap() + 2..].split(' ').collect();
        let valid = rest[1] == "1";
        let adapter = rest[5];
        let mut out = vec![head.to_string(), "  writelock".to_string(), "  lastoptions".to_string()];
        let typed = !name.is_empty();
        let shown = if typed { name.to_string() } else { HUMAN.to_string() };
        if typed {
            player = name.to_string();
        }
        out.push(format!("  checkname {shown}"));
        if valid {
            out.push("  writeunlock".to_string());
            out.push("  formopen".to_string());
            out.push(format!("  server EEName {adapter}"));
            out.push(format!("  localinput 255 {shown} v1 7 0"));
        } else {
            out.push("  display S10004".to_string());
            out.push("  writeunlock".to_string());
        }
        // the slots after: the table's when the name passed, else as the last game left them (taken in place)
        let player_line = a + out.len();
        let mp = if valid { "0" } else { &lines[player_line][lines[player_line].len() - 1..] };
        out.push(format!("  player {player} mp {mp}"));
        let table = random_map_slots();
        for s in 0..16 {
            let golden = lines[player_line + 1 + s];
            if !valid {
                out.push(golden.to_string());
                continue;
            }
            let t = table[s];
            let name = match t.source {
                EE_SCREENS_SP_HUMAN => shown.as_str(),
                EE_SCREENS_SP_COMPUTER => COMPUTER,
                _ => "",
            };
            let kind = if t.source == EE_SCREENS_SP_CLOSED { 3 } else { t.kind };
            out.push(format!(
                "  slot {s} type {kind} colour {} slot {} local {} ready {} team 0 loaded 3 b36 1 name {name}",
                t.color, t.slot, t.local as u8, t.ready as u8
            ));
        }
        let b = starts.get(n + 1).copied().unwrap_or(lines.len());
        for (k, (x, y)) in out.iter().zip(&lines[a..b]).enumerate() {
            assert_eq!(x, y, "case {n} line {k}");
        }
        assert_eq!(out.len(), b - a, "case {n}");
    }
}
