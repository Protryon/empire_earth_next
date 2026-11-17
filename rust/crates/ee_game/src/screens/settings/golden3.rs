//! The settings form's screen and resolution lists against the real C++ (`golden_settings3.txt`): a scratch driver
//! (`build/sc3/settings3_driver.cpp`, round 2's fakes) ran the unmodified `EEUIEGameSettingsEvents.cpp`'s
//! `ShowScreens`, `ShowResolutions` (with `FillModes`' sort) and `ApplySettings`' mode index over random display
//! tables (rasterizers and screens by index, repeated indices, modes of the settings', the defaults' and other
//! sizes): 2,000 cases. The replay rebuilds each case's lines from the printed tables over this module: the lists'
//! trace whole, `ApplySettings`' mode index from its "applied" line (its other lines are round 2's golden). The
//! driver relinked over the forwarders prints the same file, and the same on seeds 2-4 at ten times the size.

use super::*;

const GOLDEN: &str = include_str!("../golden_settings3.txt");

struct Screen {
    name: String,
    index: c_ulong,
    modes: Vec<EeScreensVideoMode>,
}
struct Rasterizer {
    index: c_ulong,
    screens: Vec<Screen>,
}

fn size(s: &str) -> EeScreensVideoMode {
    let v: Vec<c_ulong> = s.split('x').map(|x| x.parse().unwrap()).collect();
    EeScreensVideoMode { width: v[0], height: v[1], refresh_rate: 0, bit_count: v[2] }
}

/// `FillModes`: the first screen of that index in the rasterizers of that index, sorted largest first.
fn fill_modes(table: &[Rasterizer], rasterizer: c_ulong, screen: c_ulong) -> Vec<EeScreensVideoMode> {
    let mut modes = Vec::new();
    for r in table.iter().filter(|r| r.index == rasterizer) {
        if let Some(s) = r.screens.iter().find(|s| s.index == screen) {
            modes.extend_from_slice(&s.modes);
            break;
        }
    }
    modes.sort_by(|a, b| {
        if video_mode_greater(a, b) {
            core::cmp::Ordering::Less
        } else if video_mode_greater(b, a) {
            core::cmp::Ordering::Greater
        } else {
            core::cmp::Ordering::Equal
        }
    });
    modes
}

#[test]
fn replays_the_cpp_golden() {
    let lines: Vec<&str> = GOLDEN.lines().collect();
    let starts: Vec<usize> = (0..lines.len()).filter(|&i| lines[i].starts_with("case ")).collect();
    assert_eq!(starts.len(), 2000);
    let mut checked = [0; 3];
    for (n, &a) in starts.iter().enumerate() {
        let b = starts.get(n + 1).copied().unwrap_or(lines.len());
        let t: Vec<&str> = lines[a].split(' ').collect();
        let field = |key: &str| t[t.iter().position(|&s| s == key).unwrap() + 1];
        let op: usize = field("op").parse().unwrap();
        let settings = size(field("settings"));
        let defaults = size(field("defaults"));
        let screen_id: c_ulong = field("screen").parse().unwrap();
        let (raster, has) = field("raster").split_once('/').unwrap();
        let raster: c_ulong = raster.parse().unwrap();
        let card = field("card").parse::<i64>().unwrap() as c_ulong;
        let table = parse_table(lines[a + 1]);
        let mut out: Vec<String> = vec![lines[a].to_string(), lines[a + 1].to_string()];
        match op {
            0 => {
                let applied = lines[a..b].iter().find(|l| l.starts_with("  applied ")).unwrap();
                let at: Vec<&str> = applied.split(' ').collect();
                let rasterizer = if has == "1" { raster } else { 0 };
                let screen = if card == c_ulong::MAX { 0 } else { card };
                let modes = fill_modes(&table, rasterizer, screen);
                let mode = mode_index(&modes, &size(at[3]));
                assert_eq!(applied.to_string(), format!("  applied {} mode {mode}", at[3]), "case {n}");
                checked[0] += 1;
                continue;
            }
            1 => {
                let r = lines[a + 2].split(' ').nth(2).unwrap().parse::<usize>().unwrap();
                out.push(format!(" screens {r}"));
                out.push("  release 6".into());
                out.push("  clear 6".into());
                let screens = &table[r].screens;
                let line = screen_line(screens.len() as c_ulong, screen_id);
                for (i, s) in screens.iter().enumerate() {
                    out.push(format!("  append 6 {}", s.name));
                    if line == i as c_ulong {
                        out.push(format!("  sel 6 {i}"));
                    }
                }
            }
            _ => {
                out.push(" resolutions".into());
                let modes = fill_modes(&table, raster, card);
                let line = resolution_line(&modes, &settings, &defaults);
                out.push("  listclear 10".into());
                for m in &modes {
                    out.push(format!("  listappend 10 {} x {} x {} Bit", m.width, m.height, m.bit_count));
                }
                out.push(format!("  listselect 10 {line} 0 1"));
            }
        }
        for (k, (x, y)) in out.iter().zip(&lines[a..b]).enumerate() {
            assert_eq!(x, y, "case {n} line {k}");
        }
        assert_eq!(out.len(), b - a, "case {n}");
        checked[op.min(2)] += 1;
    }
    assert!(checked.iter().all(|&c| c > 400), "{checked:?}");
}

/// The displays line: `[R<n> <index> (S<n>.<k> <index> <w>x<h>x<b>@<refresh>/<format> ...) ...] ...`.
fn parse_table(line: &str) -> Vec<Rasterizer> {
    let mut out: Vec<Rasterizer> = Vec::new();
    let words: Vec<&str> = line.split_whitespace().skip(1).collect();
    let mut i = 0;
    while i < words.len() {
        let word = words[i].trim_end_matches([')', ']']);
        if words[i].starts_with('[') {
            let index = words[i + 1].trim_end_matches([')', ']']).parse().unwrap();
            out.push(Rasterizer { index, screens: Vec::new() });
            i += 2;
        } else if let Some(name) = words[i].strip_prefix('(') {
            let index = words[i + 1].trim_end_matches([')', ']']).parse().unwrap();
            out.last_mut().unwrap().screens.push(Screen { name: name.to_string(), index, modes: Vec::new() });
            i += 2;
        } else {
            let (wh, rest) = word.split_once('@').unwrap();
            let mut m = size(wh);
            m.refresh_rate = rest.split('/').next().unwrap().parse().unwrap();
            out.last_mut().unwrap().screens.last_mut().unwrap().modes.push(m);
            i += 1;
        }
    }
    out
}

#[test]
fn the_lists_choices() {
    let m = |w, h, b, r| EeScreensVideoMode { width: w, height: h, refresh_rate: r, bit_count: b };
    assert!(video_mode_greater(&m(800, 600, 16, 60), &m(640, 900, 32, 75)));
    assert!(video_mode_greater(&m(800, 600, 16, 75), &m(800, 600, 16, 60)));
    assert!(!video_mode_greater(&m(800, 600, 16, 60), &m(800, 600, 16, 60)));
    let modes = [m(1024, 768, 32, 60), m(800, 600, 32, 60), m(1024, 768, 32, 75), m(640, 480, 16, 60)];
    // the last of the settings' size; the defaults' only before any of the settings'
    assert_eq!(resolution_line(&modes, &m(1024, 768, 32, 0), &m(800, 600, 32, 0)), 2);
    assert_eq!(resolution_line(&modes, &m(640, 480, 16, 0), &m(800, 600, 32, 0)), 3);
    assert_eq!(resolution_line(&modes, &m(1, 1, 1, 0), &m(800, 600, 32, 0)), 1);
    assert_eq!(resolution_line(&modes, &m(1, 1, 1, 0), &m(2, 2, 2, 0)), 0);
    assert_eq!(mode_index(&modes, &m(1024, 768, 32, 0)), 2);
    assert_eq!(mode_index(&modes, &m(1, 1, 1, 0)), 0);
    assert_eq!(screen_line(2, 1), 1);
    assert_eq!(screen_line(2, 2), c_ulong::MAX);
}
