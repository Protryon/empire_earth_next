//! `UIForm::SetHelpTextPopupLocation` against the real C++ (`golden_help.txt`): a scratch driver (`build/u9_golden`,
//! `driver_help.cpp`) ran the unmodified `UIForm.cpp` over 3,000 random forms, mouse places (near the right and
//! bottom edges, fractional) and tool tip sizes, the label a fake whose virtual `SetLocation` logs the point. The
//! replay below places the same tips; the driver relinked over the forwarder prints the same file, and the same on
//! two more seeds at ten times the size.

use super::*;
use crate::rect::UiPoint;

const GOLDEN: &str = include_str!("../golden_help.txt");

fn fx(s: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(s, 16).unwrap())
}

fn replay(golden: &str) -> usize {
    let lines: Vec<&str> = golden.lines().collect();
    let mut out = Vec::new();
    let mut tips = 0;
    // the label's SetLocation slot, as the driver found it
    let slot = lines.iter().find(|l| l.starts_with("  slot ")).unwrap().split(' ').nth(3).unwrap();
    for line in &lines {
        let t: Vec<&str> = line.split(' ').collect();
        if t[0] == "tip" {
            tips += 1;
            out.push(line.to_string());
            let form = UiRect::new(fx(t[2]), fx(t[3]), fx(t[4]), fx(t[5]));
            let mouse = UiPoint { x: fx(t[7]), y: fx(t[8]) };
            let label = UiRect::new(fx(t[10]), fx(t[11]), fx(t[12]), fx(t[13]));
            let at = help_popup_location(mouse, label, form);
            out.push(format!("  slot {} {:08x} {:08x}", slot, at.x.to_bits(), at.y.to_bits()));
        }
    }
    for (n, (a, g)) in out.iter().zip(lines.iter()).enumerate() {
        assert_eq!(a, g, "line {}", n + 1);
    }
    assert_eq!(out.len(), lines.len());
    tips
}

#[test]
fn replays_the_cpp_golden() {
    assert_eq!(replay(GOLDEN), 3000);
}

/// Another seed's golden (seeds 2 and 3, ten times the size): `EE_UI_GOLDEN=<file> cargo test -- --ignored help`.
#[test]
#[ignore]
fn replays_another_seed() {
    let path = std::env::var("EE_UI_GOLDEN").expect("EE_UI_GOLDEN");
    assert_eq!(replay(&std::fs::read_to_string(path).unwrap()), 30000);
}

#[test]
fn a_tip_at_the_bottom_right_goes_left_and_up() {
    let form = UiRect::new(0.0, 0.0, 639.0, 479.0);
    let label = UiRect::new(0.0, 0.0, 100.0, 20.0);
    let at = help_popup_location(UiPoint { x: 600.5, y: 470.0 }, label, form);
    assert_eq!(at, UiPoint { x: 538.0, y: 450.0 });
    let at = help_popup_location(UiPoint { x: 10.0, y: 10.0 }, label, form);
    assert_eq!(at, UiPoint { x: 10.0, y: 42.0 });
}
