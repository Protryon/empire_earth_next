//! The command panel's buttons against the real C++ (`golden_buttons.txt`): a scratch driver
//! (`build/sc3/button_driver.cpp`) ran the unmodified `EEUIFMGStatusButton.cpp`'s `CopyData`, `GetHelpString` and
//! `Command` (its targeting modes, the patrol and the flare) over calloc'ed players, selected groups (none, empty
//! entries, teleporters short of power), units, tech nodes (calamities with and without a cost, nodes without a
//! class), button objects and hot keys: 2,000 cases. The replay rebuilds every line from the printed inputs over
//! this module. The driver relinked over the forwarders prints the same file, and the same on seeds 2-4 at ten
//! times the size.

use super::*;

const GOLDEN: &str = include_str!("../golden_buttons.txt");

const IDS_TRADE_BUY: c_long = 5538;
const IDS_TRADE_SELL: c_long = 5539;
const IDS_TRADE_FOR: c_long = 5540;
const IDS_RESOURCE_NAME_GOLD: c_long = 30119;
const IDS_RESOURCE_NAME_BASE: c_long = 30114;
const IDS_IMPROVEMENT_POWER: c_long = 30024;
const IDS_POP_COST: c_long = 30469;
const BUTTON_SOUND: u32 = 5000 + 0x207;

fn field<'a>(t: &[&'a str], key: &str) -> &'a str {
    t[t.iter().position(|&s| s == key).unwrap_or_else(|| panic!("no {key}")) + 1]
}
fn l(s: &str) -> c_long {
    s.parse::<i64>().unwrap() as c_long
}

#[test]
fn replays_the_cpp_golden() {
    let lines: Vec<&str> = GOLDEN.lines().collect();
    let starts: Vec<usize> = (0..lines.len()).filter(|&i| lines[i].starts_with("case ")).collect();
    assert_eq!(starts.len(), 2000);
    let mut ops = [0usize; 3];
    for (n, &a) in starts.iter().enumerate() {
        let b = starts.get(n + 1).copied().unwrap_or(lines.len());
        let case = &lines[a..b];
        let head = case[0].split(" upgrades").next().unwrap();
        let upgrades: Vec<&str> = case[0].split(" upgrades ").nth(1).unwrap().split(' ').collect();
        let t: Vec<&str> = head.split(' ').collect();
        let ctl = field(&t, "ctl");
        let command = field(&t, "command") == "1";
        let button: c_ulong = field(&t, "button").parse().unwrap();
        let object = field(&t, "object") == "1";
        let tech = field(&t, "tech");
        let unit_type = field(&t, "type");
        let count = l(field(&t, "count"));
        let state: c_ulong = field(&t, "state").parse().unwrap();
        let bitmaps: Vec<bool> = field(&t, "bitmaps").chars().map(|c| c == '1').collect();
        let has_group = field(&t, "group") == "1";
        let ui = t.iter().position(|&s| s == "units").unwrap();
        let up = t.iter().position(|&s| s == "produce").unwrap();
        let units: Vec<Option<(c_ulong, c_long, [bool; 3])>> = t[ui + 1..up]
            .iter()
            .map(|u| {
                if *u == "-" {
                    return None;
                }
                let v: Vec<&str> = u.split('/').collect();
                let f: Vec<bool> = v[2].chars().map(|c| c == '1').collect();
                Some((v[0].parse().unwrap(), l(v[1]), [f[0], f[1], f[2]]))
            })
            .collect();
        let produce = field(&t, "produce") == "1";
        let afford: Vec<bool> = t[up + 2].trim_start_matches("afford").chars().map(|c| c == '1').collect();
        let priest = field(&t, "priest") == "1";
        let cost = l(field(&t, "cost"));
        let population = l(field(&t, "pop"));
        let hotkey = field(&t, "hotkey") == "1";
        let resources = l(field(&t, "resources"));
        let clock = f32::from_bits(u32::from_str_radix(field(&t, "clock"), 16).unwrap());
        let delay: c_ulong = field(&t, "delay").parse().unwrap();
        let node = field(&t, "node") == "1";
        let kind = l(field(&t, "kind"));
        let calamity: c_ulong = field(&t, "calamity").parse().unwrap();
        let class = l(field(&t, "class"));
        let name = l(field(&t, "name"));
        let makes = field(&t, "makes") == "1";
        let mut out = vec![case[0].to_string()];
        let op = case[1].split(' ').nth(1).unwrap();
        out.push(case[1].to_string());
        let hot = if hotkey { " (Ctrl+K)" } else { "" };
        match op {
            "copy" => {
                ops[0] += 1;
                let mut facts = EeScreensButtonFacts {
                    has_group,
                    unload_cost: 100,
                    game_resources: resources,
                    clock,
                    jit_delay: delay,
                    ..Default::default()
                };
                let test = if command {
                    command_test(button)
                } else {
                    if count != 0 {
                        out.push(format!("  queuetext {count}"));
                    }
                    let is_calamity = tech_is_calamity(kind, calamity);
                    if is_calamity {
                        out.push(format!("  calamity {calamity}"));
                        facts.calamity_cost = cost;
                    }
                    tech_test(is_calamity, facts.calamity_cost)
                };
                if test == EE_SCREENS_BUTTON_TRADE {
                    facts.affordable = afford[((button - 0x5dc) & 7) as usize];
                }
                if test == EE_SCREENS_BUTTON_PRODUCE {
                    out.push(format!("  canproduce {tech} {unit_type} 1 1 3 0"));
                    facts.can_produce = produce;
                }
                // the driver's unit set and node answer by type: the first selected unit of that type's flags
                let by_type = |ty: c_ulong, k: usize| units.iter().flatten().find(|u| u.0 == ty).unwrap().2[k];
                let views: Vec<EeScreensButtonUnit> = units
                    .iter()
                    .map(|u| match u {
                        None => EeScreensButtonUnit::default(),
                        Some((ty, power, f)) => EeScreensButtonUnit {
                            present: true,
                            unit_type: *ty,
                            power: *power,
                            in_set: by_type(*ty, 0),
                            cloaked: f[1],
                            in_node: by_type(*ty, 2),
                        },
                    })
                    .collect();
                let available = available(test, &facts, &views);
                let icon = icon(object, button, state, bitmaps[0], bitmaps[1], bitmaps[2]);
                match icon {
                    EE_SCREENS_BUTTON_ICON_PRESSED => out.push(format!("  pressedicon {ctl}")),
                    EE_SCREENS_BUTTON_ICON_FACE => out.push(format!("  faceicon {ctl}")),
                    _ => {}
                }
                out.push("  buttoncopy".into());
                let text = if !command && count != 0 { count.to_string() } else { String::new() };
                out.push(format!(
                    "  available {} show {} icon {} text {text}",
                    available as i32,
                    (!command && count != 0) as i32,
                    (icon != EE_SCREENS_BUTTON_ICON_NONE) as i32
                ));
            }
            "help" => {
                ops[1] += 1;
                let forced = case[1].ends_with(" 1");
                let mut s = String::new();
                if command {
                    if (0x5dc..=0x5e3).contains(&button) {
                        let sell = button > 0x5df;
                        let resource: c_long = [2, 3, 4, 6][((button - 0x5dc) & 3) as usize];
                        let price = 100 + resource * 10 + sell as c_long;
                        let verb = if sell { IDS_TRADE_SELL } else { IDS_TRADE_BUY };
                        s = format!(
                            "S{verb} 100 S{} S{IDS_TRADE_FOR} {price} S{IDS_RESOURCE_NAME_GOLD}",
                            resource + IDS_RESOURCE_NAME_BASE
                        );
                    } else {
                        s = format!("S{}", command_help_string(button));
                        if command_help_priest(button) && (has_group || forced) {
                            if let Some(Some(_)) = units.first() {
                                if priest {
                                    s += &format!(": S{IDS_IMPROVEMENT_POWER}: 77");
                                }
                            }
                        }
                    }
                    s += hot;
                } else if node {
                    let has_class = class > 0 && class != 3;
                    let named = has_class && !upgrades[class as usize].is_empty();
                    if kind == 5 {
                        out.push(format!("  calamity {calamity}"));
                    }
                    let plan = tech_help(
                        has_class,
                        named,
                        if has_class { population } else { 0 },
                        kind,
                        if kind == 5 { cost } else { 0 },
                        makes,
                        name,
                    );
                    s = match plan.name {
                        1 => "Up".to_string(),
                        0 => format!("S{}", 3000 + class),
                        _ => format!("S{name}"),
                    };
                    s += ": ";
                    if plan.power {
                        s += &format!("S{IDS_IMPROVEMENT_POWER}: {cost}");
                    } else {
                        s += &format!("C{unit_type}/1");
                    }
                    if plan.population {
                        s += &format!(" S{IDS_POP_COST}: {}", population as c_ulong);
                    }
                    if plan.unit_name {
                        s += &format!(" S{name}");
                    }
                    s += hot;
                }
                out.push(format!("  help 1 {s}"));
            }
            _ => {
                ops[2] += 1;
                let button: c_ulong = case[1].split(' ').nth(2).unwrap().parse().unwrap();
                let (mut locked, mut pointer, mut refresh) = (false, 0, false);
                if let Some(mode) = command_mode(button) {
                    if mode.lock_cursor {
                        locked = true;
                        pointer = mode.pointer;
                    }
                    out.push(format!("  uistate {}", mode.ui_state));
                    out.push("  mode 244 1".into());
                    refresh = true;
                } else if button == 0x1b || button == 0x22 {
                    locked = true;
                    out.push("  startpatrol".into());
                } else if button == 0x14 || button == 0x23 {
                    locked = true;
                    out.push("  startflare".into());
                }
                out.push(format!("  play {BUTTON_SOUND}"));
                out.push(format!("  locked {} pointer {pointer} refresh {}", locked as i32, refresh as i32));
            }
        }
        for (k, (x, y)) in out.iter().zip(case).enumerate() {
            assert_eq!(x, y, "case {n} line {k}");
        }
        assert_eq!(out.len(), case.len(), "case {n}");
    }
    assert!(ops.iter().all(|&c| c > 500), "{ops:?}");
}
