//! The main group against the real C++: a scratch driver (`build/sc28/mg`) ran the unmodified `EEUIFMGMainGroup`'s
//! `Refresh`, `SetUnit`, `GetSelectedUnit`, `Open`, `ShowCommandButton`, `ShowSpecialButtons`, `Update` and
//! `WantsRefresh` (cut from 2cac466f) and the new ones (forwarders and host over this module) over one fake world per
//! case (selections of 0-4 units or a viewed unit, dead units, units without a complex unit or off the map, the local
//! player's or not, citizens, markets and airports, cloaked markets, behaviours and formations past the faces, button
//! objects missing or without hot keys, goal queues running or not, epochs before and after WW2, build queues with
//! runs of the same technology), each case a sequence of 1-4 calls, comparing every effect and the group's members
//! after. `golden_main_group.txt` is the new side's host log: per call `> kind unit b`, then `|op a b c answer` per op
//! (units as stable handles), then `= result`.

use super::*;

const GOLDEN: &str = include_str!("../golden_main_group.txt");

struct Replay<'a> {
    ops: std::slice::Iter<'a, [i64; 5]>,
    case: String,
}

impl GroupHost for Replay<'_> {
    fn op(&mut self, op: u32, a: i64, b: i64, c: i64) -> i64 {
        let want = self.ops.next().unwrap_or_else(|| panic!("{}: an extra op {op} {a} {b} {c}", self.case));
        assert_eq!([op as i64, a, b, c], want[..4], "{}", self.case);
        want[4]
    }
}

#[test]
fn replays_the_golden() {
    let mut case = String::new();
    let mut call: Option<(char, i64, i64)> = None;
    let mut ops: Vec<[i64; 5]> = Vec::new();
    let mut kinds = std::collections::BTreeMap::new();
    for line in GOLDEN.lines() {
        if line.starts_with("case ") {
            case = line.to_string();
        } else if let Some(rest) = line.strip_prefix("> ") {
            let f: Vec<&str> = rest.split(' ').collect();
            call = Some((f[0].chars().next().unwrap(), f[1].parse().unwrap(), f[2].parse().unwrap()));
        } else if let Some(r) = line.strip_prefix("= ") {
            let (kind, unit, b) = call.take().unwrap();
            let want: i64 = r.parse().unwrap();
            let mut h = Replay { ops: ops.iter(), case: format!("{case} {kind}") };
            let which = match kind {
                'R' => EE_SCREENS_MGR_DO_REFRESH,
                'W' => EE_SCREENS_MGR_DO_WANTS_REFRESH,
                'G' => EE_SCREENS_MGR_DO_SELECTED_UNIT,
                'O' => EE_SCREENS_MGR_DO_OPEN,
                'S' => EE_SCREENS_MGR_DO_SET_UNIT,
                'C' => EE_SCREENS_MGR_DO_COMMAND_BUTTON,
                'P' => EE_SCREENS_MGR_DO_SPECIAL_BUTTONS,
                'U' => EE_SCREENS_MGR_DO_UPDATE,
                _ => 0,
            };
            let got = member(&mut h, which, unit, b);
            assert!(h.ops.next().is_none(), "{case} {kind}: ops left");
            if kind == 'W' || kind == 'G' {
                assert_eq!(got, want, "{case} {kind}");
            }
            *kinds.entry(kind).or_insert(0) += 1;
            ops.clear();
        } else {
            for field in line.split('|').skip(1) {
                let v: Vec<i64> = field.split(' ').map(|x| x.parse().unwrap()).collect();
                ops.push([v[0], v[1], v[2], v[3], v[4]]);
            }
        }
    }
    for kind in ['R', 'W', 'G', 'O', 'S', 'C', 'P', 'U'] {
        assert!(kinds.get(&kind).copied().unwrap_or(0) >= 150, "{kinds:?}");
    }
}

/// A run of equal queued technologies counts once on its research button, by its length.
#[test]
fn a_queued_run_counts_on_its_button() {
    struct Log(Vec<(u32, i64, i64)>);
    impl GroupHost for Log {
        fn op(&mut self, op: u32, a: i64, b: i64, c: i64) -> i64 {
            self.0.push((op, a, b));
            match op {
                EE_SCREENS_MGR_OP_UNIT_HP => 10,
                EE_SCREENS_MGR_OP_ON_MAP_LOCAL => 1,
                EE_SCREENS_MGR_OP_QUEUE_SIZE => 4,
                EE_SCREENS_MGR_OP_QUEUE_KEY => [100, 100, 100, 7][c as usize],
                EE_SCREENS_MGR_OP_BUTTON_TECH if a == 0x70 => 100,
                EE_SCREENS_MGR_OP_BUTTON_TECH if a == 0x71 => 7,
                EE_SCREENS_MGR_OP_BUTTON_TECH => EE_SCREENS_MGR_COMMAND,
                _ => 0,
            }
        }
    }
    let mut h = Log(Vec::new());
    update(&mut h, 1);
    let adds: Vec<_> = h.0.iter().filter(|o| o.0 == EE_SCREENS_MGR_OP_ADD_COUNT).collect();
    assert_eq!(adds, [&(EE_SCREENS_MGR_OP_ADD_COUNT, 0x70, 3), &(EE_SCREENS_MGR_OP_ADD_COUNT, 0x71, 1)]);
}
