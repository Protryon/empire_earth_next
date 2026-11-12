//! The status box's attribute texts against the real C++ (`golden_hud.txt`): a scratch driver
//! (`build/sc2/hud_driver.cpp`) ran the unmodified `EEUIFMGStatusBox.cpp`'s `GetAttributeText` over random floats
//! (NaN, the infinities, huge and tiny values and the rounding's ties included) and `UpdateArmorTexts` over
//! calloc'ed units, classes and stats (the stats' class the unit's or another, each armor attribute shown or not),
//! the real `UWideString`: 3,000 cases. The replay rebuilds every line from the printed inputs over this module;
//! the driver relinked over the forwarders prints the same file, and the same on seeds 2-4 at ten times the size.
//!
//! The golden is the Windows build's 32-bit `long`: on a 64-bit `long` the cases holding a value past 2^31 are
//! skipped.

use super::*;

const GOLDEN: &str = include_str!("../golden_hud.txt");

fn f(hex: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(hex, 16).unwrap())
}

fn text(base: f32, bonus: f32) -> String {
    let (a, b) = attribute_text(base, bonus);
    format!("{a}+{b}")
}

#[test]
fn replays_the_cpp_golden() {
    let wide = core::mem::size_of::<c_long>() == 8;
    let lines: Vec<&str> = GOLDEN.lines().collect();
    let starts: Vec<usize> = (0..lines.len()).filter(|&i| lines[i].starts_with("case ")).collect();
    assert_eq!(starts.len(), 3000);
    let mut replayed = 0;
    for (n, &a) in starts.iter().enumerate() {
        let b = starts.get(n + 1).copied().unwrap_or(lines.len());
        let t: Vec<&str> = lines[a].split(' ').collect();
        let mut out = vec![lines[a].to_string()];
        if t[2] == "text" {
            let (base, bonus) = (f(t[3]), f(t[4]));
            let mine = format!("case {n} text {} {} -> {}", t[3], t[4], text(base, bonus));
            if wide && mine != lines[a] {
                continue; // a conversion past 2^31: the 64-bit long's own answer
            }
            out[0] = mine;
        } else {
            let mut class = [0 as c_long; 6];
            let mut other = [0 as c_long; 6];
            let mut stat = [0f32; 6];
            for k in 0..6 {
                let v: Vec<&str> = t[5 + k].split('/').collect();
                class[k] = v[0].parse::<i64>().unwrap() as c_long;
                other[k] = v[1].parse::<i64>().unwrap() as c_long;
                stat[k] = f(v[2]);
            }
            let same = t[4] == "1";
            let icons: Vec<i64> = t[12..18].iter().map(|s| s.parse().unwrap()).collect();
            for (slot, &icon) in icons.iter().enumerate() {
                if icon == -1 {
                    continue;
                }
                let kind = ARMOR_KINDS[slot] as usize;
                let stats_class = if same { class[kind] } else { other[kind] };
                let (base, bonus) = armor_attribute(stat[kind], class[kind], stats_class);
                out.push(format!("  cost {} {}", 10 + icon, text(base, bonus)));
            }
            if wide && out[..] != lines[a..b] {
                continue;
            }
        }
        for (k, (x, y)) in out.iter().zip(&lines[a..b]).enumerate() {
            assert_eq!(x, y, "case {n} line {k}");
        }
        assert_eq!(out.len(), b - a, "case {n}");
        replayed += 1;
    }
    assert!(replayed > if wide { 2000 } else { 2999 }, "{replayed} cases replayed");
}
