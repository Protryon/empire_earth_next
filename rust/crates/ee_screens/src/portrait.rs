//! The selected unit's portrait (`EEUICPortrait`): which progress bars it shows (the research of a building or a
//! unit in production, the unit type's improvement), their rectangles, the improvement level it shows, its drawing
//! (each shown bar with its red remainder), its tooltip's choice and its click (cancel the research or the
//! improvement). Over a host ([`PortraitHost`], `ee_screenscpp_portrait` in `User Interface Thread/EEUICPortrait.cpp`)
//! whose ops are each one read of the unit or one toolkit or message call; the C++ class keeps its bitmap, bars,
//! text, the unit's references (`SetUnit`) and the level's drawing.

// ---- the host's ops (ee_screenscpp_portrait(call, op, a, b, f)): units and nodes are handles (0: none).
/// -> the portrait's level | its first bar shown << 32 | its second << 33.
pub const EE_SCREENS_PT_OP_STATE: u32 = 0;
/// (state as [`EE_SCREENS_PT_OP_STATE`]) stores the level and the two flags.
pub const EE_SCREENS_PT_OP_SET_STATE: u32 = 1;
/// -> the shown unit's complex unit (`GetComplexUnit`).
pub const EE_SCREENS_PT_OP_COMPLEX: u32 = 2;
/// (unit) -> whether its player is the local one.
pub const EE_SCREENS_PT_OP_LOCAL: u32 = 3;
/// (unit) -> its production time left.
pub const EE_SCREENS_PT_OP_PRODUCTION: u32 = 4;
/// `SetDirty(true)`.
pub const EE_SCREENS_PT_OP_DIRTY: u32 = 5;
/// (unit) -> the improvement its player researches for its type (0x10: none).
pub const EE_SCREENS_PT_OP_RESEARCHED: u32 = 6;
/// (unit) -> its current research.
pub const EE_SCREENS_PT_OP_RESEARCH: u32 = 7;
/// (technology) -> the local player's tech tree node.
pub const EE_SCREENS_PT_OP_NODE: u32 = 8;
/// (node) -> f32 bits of the shown unit's player's build time scale of the node's unit class when the node makes
/// a unit and that class has an upgrade; -1 otherwise.
pub const EE_SCREENS_PT_OP_SCALE: u32 = 9;
/// (node) -> its build time.
pub const EE_SCREENS_PT_OP_BUILD_TIME: u32 = 10;
/// -> f[0..4] the portrait's rectangle.
pub const EE_SCREENS_PT_OP_RECT: u32 = 11;
/// (bar: 0 first, 1 second, 2 the background) its rectangle f[0..4].
pub const EE_SCREENS_PT_OP_SET_BAR: u32 = 12;
/// (unit, improvement) -> its stats' level of the improvement.
pub const EE_SCREENS_PT_OP_IMPROVEMENT_LEVEL: u32 = 13;
/// (1 or 2, improvement) -> the world table's improvement time (its first or second value, unsigned).
pub const EE_SCREENS_PT_OP_IMPROVEMENT_TIME: u32 = 14;
/// (unit) -> f32 bits of its player's improvement progress for its type.
pub const EE_SCREENS_PT_OP_PROGRESS: u32 = 15;
/// (unit) -> its stats' improvements' total cost.
pub const EE_SCREENS_PT_OP_TOTAL_COST: u32 = 16;
/// -> the world table's improvement points maximum (unsigned).
pub const EE_SCREENS_PT_OP_POINTS_MAX: u32 = 17;
/// (bytes, length) the level's text.
pub const EE_SCREENS_PT_OP_TEXT: u32 = 18;
/// The form's help text updated.
pub const EE_SCREENS_PT_OP_UPDATE_HELP: u32 = 19;
/// -> `UIControl::DrawPolygons()`.
pub const EE_SCREENS_PT_OP_DRAW_BASE: u32 = 20;
/// (0 the portrait, 1 the first bar, 2 the second, 3 the background) -> drawn: the draw's result.
pub const EE_SCREENS_PT_OP_DRAW: u32 = 21;
/// (bar: 0 first, 1 second) -> f[0..4] its rectangle.
pub const EE_SCREENS_PT_OP_BAR: u32 = 22;
/// -> f[0..4] the copied draw rectangle.
pub const EE_SCREENS_PT_OP_DRAW_RECT: u32 = 23;
/// (f[0..4] a rectangle) -> whether it contains the mouse.
pub const EE_SCREENS_PT_OP_CONTAINS: u32 = 24;
/// -> whether the shown unit's class is a building.
pub const EE_SCREENS_PT_OP_BUILDING: u32 = 25;
/// (node) the tooltip: "cancel the research" of the node's unit class's or its own name.
pub const EE_SCREENS_PT_OP_HELP_RESEARCH: u32 = 26;
/// The tooltip: "cancel the improvement" of the shown unit's researched improvement's name.
pub const EE_SCREENS_PT_OP_HELP_IMPROVEMENT: u32 = 27;
/// -> whether the shown unit is set, alive and on the map.
pub const EE_SCREENS_PT_OP_ALIVE: u32 = 28;
/// -> the shown unit's complex unit (its field).
pub const EE_SCREENS_PT_OP_COMPLEX_FIELD: u32 = 29;
/// (unit, technology) sends "remove the technology".
pub const EE_SCREENS_PT_OP_REMOVE_TECHNOLOGY: u32 = 30;
/// (unit, improvement) sends "remove the improvement".
pub const EE_SCREENS_PT_OP_REMOVE_IMPROVEMENT: u32 = 31;
/// Plays the button sound.
pub const EE_SCREENS_PT_OP_SOUND: u32 = 32;
/// -> the portrait shows a unit (`mUnit != NULL`).
pub const EE_SCREENS_PT_OP_HAS_UNIT: u32 = 33;
/// The shown button's `Release()`, the shown unit's `Dereference()`.
pub const EE_SCREENS_PT_OP_RELEASE: u32 = 34;
/// `mUnit` = the unit a (0: none).
pub const EE_SCREENS_PT_OP_SET_UNIT: u32 = 35;
/// The unit's `Reference()`; `mButton` = its stats' button, `Acquire()`d.
pub const EE_SCREENS_PT_OP_ACQUIRE: u32 = 36;
/// The portrait bitmap's texture = the button's; the button's face on it (`SetFaceBitmap`, the portrait's rectangle).
pub const EE_SCREENS_PT_OP_FACE: u32 = 37;

/// No improvement researched; no technology.
const NO_IMPROVEMENT: i64 = 0x10;
const TECH_NONE: i64 = 100_000;
const BAR_FIRST: i64 = 0;
const BAR_SECOND: i64 = 1;
const BAR_BACKGROUND: i64 = 2;
const DRAW_PORTRAIT: i64 = 0;
const DRAW_BACKGROUND: i64 = 3;

/// The portrait's calls.
pub trait PortraitHost {
    /// `EE_SCREENS_PT_OP_*` with its arguments and floats; its answer.
    fn op(&mut self, op: u32, a: i64, b: i64, f: &mut [f32]) -> i64;
}

/// The portrait's own state (its C++ members).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
struct State {
    level: u32,
    first: bool,
    second: bool,
}

impl State {
    fn read<H: PortraitHost + ?Sized>(h: &mut H) -> State {
        let v = h.op(EE_SCREENS_PT_OP_STATE, 0, 0, &mut []);
        State { level: v as u32, first: v >> 32 & 1 != 0, second: v >> 33 & 1 != 0 }
    }
    fn write<H: PortraitHost + ?Sized>(&self, h: &mut H) {
        let v = self.level as i64 | (self.first as i64) << 32 | (self.second as i64) << 33;
        h.op(EE_SCREENS_PT_OP_SET_STATE, v, 0, &mut []);
    }
}

fn read_rect<H: PortraitHost + ?Sized>(h: &mut H, op: u32, a: i64) -> [f32; 4] {
    let mut f = [0.0f32; 4];
    h.op(op, a, 0, &mut f);
    f
}

/// `GetBarRectangles`: both the portrait's rectangle; with both bars shown the second the top half and the first
/// below its top (left, top, right, bottom).
fn bar_rectangles(rect: [f32; 4], state: State) -> ([f32; 4], [f32; 4]) {
    let (mut first, mut second) = (rect, rect);
    if state.second && state.first {
        second[3] = (second[3] - second[1]) * 0.5 + second[1];
        first[1] = second[1] + 1.0;
    }
    (first, second)
}

/// The x86 float to `long` conversion: truncation, `i32::MIN` for NaN and out of range.
fn cvtt(value: f32) -> i32 {
    if value.is_nan() || !(-2_147_483_648.0..2_147_483_648.0).contains(&value) {
        i32::MIN
    } else {
        value as i32
    }
}

/// The node's build time with the upgrade's scale, rounded as `(long)(t * scale + 0.5)`.
fn total_time<H: PortraitHost + ?Sized>(h: &mut H, node: i64) -> i32 {
    let scale = h.op(EE_SCREENS_PT_OP_SCALE, node, 0, &mut []);
    let scale = if scale == -1 { 1.0f32 } else { f32::from_bits(scale as u32) };
    cvtt(h.op(EE_SCREENS_PT_OP_BUILD_TIME, node, 0, &mut []) as i32 as f32 * scale + 0.5)
}

/// The improvement's time: the world table's first value at level 1, its second after.
fn improvement_time<H: PortraitHost + ?Sized>(h: &mut H, unit: i64, improvement: i64) -> f32 {
    let which = if h.op(EE_SCREENS_PT_OP_IMPROVEMENT_LEVEL, unit, improvement, &mut []) as i32 == 1 { 1 } else { 2 };
    h.op(EE_SCREENS_PT_OP_IMPROVEMENT_TIME, which, improvement, &mut []) as u32 as i32 as f32
}

/// `Update`: the research bar while the unit produces, the improvement bar while its type's improvement is
/// researched (both: the research the top half), their lengths from the time done; the level (the improvements'
/// total cost, at most the world's maximum) and its text when it changed; the form's help text when a bar
/// appeared or went.
pub fn update<H: PortraitHost + ?Sized>(h: &mut H) {
    let mut s = State::read(h);
    let old = s;
    let unit = h.op(EE_SCREENS_PT_OP_COMPLEX, 0, 0, &mut []);
    if unit != 0 {
        if h.op(EE_SCREENS_PT_OP_LOCAL, unit, 0, &mut []) != 0 {
            let production = h.op(EE_SCREENS_PT_OP_PRODUCTION, unit, 0, &mut []) as i32;
            if production > 0 {
                s.first = true;
                h.op(EE_SCREENS_PT_OP_DIRTY, 0, 0, &mut []);
            } else if s.first {
                s.first = false;
                h.op(EE_SCREENS_PT_OP_DIRTY, 0, 0, &mut []);
            }
            let improvement = h.op(EE_SCREENS_PT_OP_RESEARCHED, unit, 0, &mut []);
            if improvement != NO_IMPROVEMENT {
                s.second = true;
                h.op(EE_SCREENS_PT_OP_DIRTY, 0, 0, &mut []);
            } else if s.second {
                s.second = false;
                h.op(EE_SCREENS_PT_OP_DIRTY, 0, 0, &mut []);
            }
            let r = read_rect(h, EE_SCREENS_PT_OP_RECT, 0);
            let (width, height) = (r[2] - r[0], r[3] - r[1]);
            if s.second && s.first {
                let research = h.op(EE_SCREENS_PT_OP_RESEARCH, unit, 0, &mut []);
                let node = h.op(EE_SCREENS_PT_OP_NODE, research, 0, &mut []);
                let total = total_time(h, node);
                let (left, top) = (r[0], r[1]);
                let bottom = height * 0.5 + top + 1.0;
                let right = width * (total.wrapping_sub(production) as f32 / total as f32) + left;
                h.op(EE_SCREENS_PT_OP_SET_BAR, BAR_FIRST, 0, &mut [left, top, right, bottom]);
                let maximum = improvement_time(h, unit, improvement);
                let progress = f32::from_bits(h.op(EE_SCREENS_PT_OP_PROGRESS, unit, 0, &mut []) as u32);
                let right = (maximum - progress) / maximum * width + r[0] + 1.0;
                h.op(EE_SCREENS_PT_OP_SET_BAR, BAR_SECOND, 0, &mut [r[0], height * 0.5 + r[1], right, r[3] + 1.0]);
            } else if s.second {
                let maximum = improvement_time(h, unit, improvement);
                let progress = f32::from_bits(h.op(EE_SCREENS_PT_OP_PROGRESS, unit, 0, &mut []) as u32);
                let right = (maximum - progress) / maximum * width + r[0] + 1.0;
                h.op(EE_SCREENS_PT_OP_SET_BAR, BAR_SECOND, 0, &mut [r[0], r[1], right, r[3] + 1.0]);
            } else if s.first {
                let research = h.op(EE_SCREENS_PT_OP_RESEARCH, unit, 0, &mut []);
                let node = h.op(EE_SCREENS_PT_OP_NODE, research, 0, &mut []);
                if node != 0 {
                    let total = total_time(h, node);
                    let right = width * (total.wrapping_sub(production) as f32 / total as f32) + r[0];
                    h.op(EE_SCREENS_PT_OP_SET_BAR, BAR_FIRST, 0, &mut [r[0], r[1], right, height + r[1]]);
                }
            }
        }
        s.level = h.op(EE_SCREENS_PT_OP_TOTAL_COST, unit, 0, &mut []) as u32;
    } else {
        s.level = 0;
    }
    let maximum = h.op(EE_SCREENS_PT_OP_POINTS_MAX, 0, 0, &mut []) as u32;
    if s.level > maximum {
        s.level = maximum;
    }
    s.write(h);
    if s.level != old.level {
        let text = if s.level > 0 { (s.level as i32).to_string() } else { String::new() };
        h.op(EE_SCREENS_PT_OP_TEXT, text.as_ptr() as i64, text.len() as i64, &mut []);
    }
    if old.second != s.second || old.first != s.first {
        h.op(EE_SCREENS_PT_OP_UPDATE_HELP, 0, 0, &mut []);
    }
}

/// `SetUnit` (0x258f82): the shown unit and button released, the unit (0 none) shown: referenced, its button
/// acquired and its face on the portrait, both bars off, redrawn and updated.
pub fn set_unit<H: PortraitHost + ?Sized>(h: &mut H, unit: i64) {
    if h.op(EE_SCREENS_PT_OP_HAS_UNIT, 0, 0, &mut []) != 0 {
        h.op(EE_SCREENS_PT_OP_RELEASE, 0, 0, &mut []);
    }
    h.op(EE_SCREENS_PT_OP_SET_UNIT, unit, 0, &mut []);
    if unit == 0 {
        return;
    }
    h.op(EE_SCREENS_PT_OP_ACQUIRE, 0, 0, &mut []);
    h.op(EE_SCREENS_PT_OP_FACE, 0, 0, &mut []);
    let state = State { first: false, second: false, ..State::read(h) };
    state.write(h);
    h.op(EE_SCREENS_PT_OP_DIRTY, 0, 0, &mut []);
    update(h);
}

/// `DrawPolygons`: the portrait, then each shown bar with the background over the rest of its row to the copied
/// rectangle's right; the last draw's result.
pub fn draw<H: PortraitHost + ?Sized>(h: &mut H) -> i64 {
    h.op(EE_SCREENS_PT_OP_DRAW_BASE, 0, 0, &mut []);
    let s = State::read(h);
    let mut result = h.op(EE_SCREENS_PT_OP_DRAW, DRAW_PORTRAIT, 0, &mut []);
    for (shown, bar) in [(s.first, BAR_FIRST), (s.second, BAR_SECOND)] {
        if !shown {
            continue;
        }
        h.op(EE_SCREENS_PT_OP_DRAW, bar + 1, 0, &mut []);
        let b = read_rect(h, EE_SCREENS_PT_OP_BAR, bar);
        let copied = read_rect(h, EE_SCREENS_PT_OP_DRAW_RECT, 0);
        h.op(EE_SCREENS_PT_OP_SET_BAR, BAR_BACKGROUND, 0, &mut [b[2], b[1], copied[2] + 1.0, b[3]]);
        result = h.op(EE_SCREENS_PT_OP_DRAW, DRAW_BACKGROUND, 0, &mut []);
    }
    result
}

/// The research tooltip of the shown unit's complex unit's current research; false when it has no node.
fn help_research<H: PortraitHost + ?Sized>(h: &mut H) -> bool {
    let unit = h.op(EE_SCREENS_PT_OP_COMPLEX, 0, 0, &mut []);
    let research = h.op(EE_SCREENS_PT_OP_RESEARCH, unit, 0, &mut []);
    let node = h.op(EE_SCREENS_PT_OP_NODE, research, 0, &mut []);
    if node == 0 {
        return false;
    }
    h.op(EE_SCREENS_PT_OP_HELP_RESEARCH, node, 0, &mut []);
    true
}

/// `GetHelpString`: over the second bar of a building with both bars, or the first bar of a building alone, the
/// research's "cancel"; otherwise with the improvement bar, the improvement's; false (an empty text) for none.
pub fn help<H: PortraitHost + ?Sized>(h: &mut H) -> bool {
    let s = State::read(h);
    if s.second && s.first {
        let rect = read_rect(h, EE_SCREENS_PT_OP_RECT, 0);
        let (_, mut second) = bar_rectangles(rect, s);
        if h.op(EE_SCREENS_PT_OP_CONTAINS, 0, 0, &mut second) != 0
            && h.op(EE_SCREENS_PT_OP_BUILDING, 0, 0, &mut []) != 0
        {
            help_research(h)
        } else {
            h.op(EE_SCREENS_PT_OP_HELP_IMPROVEMENT, 0, 0, &mut []);
            true
        }
    } else if s.second {
        h.op(EE_SCREENS_PT_OP_HELP_IMPROVEMENT, 0, 0, &mut []);
        true
    } else if s.first && h.op(EE_SCREENS_PT_OP_BUILDING, 0, 0, &mut []) != 0 {
        help_research(h)
    } else {
        false
    }
}

/// `MouseLButtonDown` (and the right press): for a live unit on the map, cancels the building's research (over
/// the second bar's rectangle with the first bar shown, or with no improvement) or the type's improvement.
pub fn mouse_down<H: PortraitHost + ?Sized>(h: &mut H) -> i64 {
    if h.op(EE_SCREENS_PT_OP_ALIVE, 0, 0, &mut []) == 0 {
        return 0;
    }
    let unit = h.op(EE_SCREENS_PT_OP_COMPLEX_FIELD, 0, 0, &mut []);
    let technology = h.op(EE_SCREENS_PT_OP_RESEARCH, unit, 0, &mut []);
    let improvement = h.op(EE_SCREENS_PT_OP_RESEARCHED, unit, 0, &mut []);
    let remove_technology = |h: &mut H| {
        h.op(EE_SCREENS_PT_OP_REMOVE_TECHNOLOGY, unit, technology, &mut []);
        h.op(EE_SCREENS_PT_OP_SOUND, 0, 0, &mut []);
    };
    let remove_improvement = |h: &mut H| {
        h.op(EE_SCREENS_PT_OP_REMOVE_IMPROVEMENT, unit, improvement, &mut []);
        h.op(EE_SCREENS_PT_OP_SOUND, 0, 0, &mut []);
    };
    if technology as i32 != TECH_NONE as i32 {
        if improvement != NO_IMPROVEMENT {
            let s = State::read(h);
            let rect = read_rect(h, EE_SCREENS_PT_OP_RECT, 0);
            let (_, mut second) = bar_rectangles(rect, s);
            if s.first && h.op(EE_SCREENS_PT_OP_CONTAINS, 0, 0, &mut second) != 0 {
                if h.op(EE_SCREENS_PT_OP_BUILDING, 0, 0, &mut []) != 0 {
                    remove_technology(h);
                }
            } else if s.second {
                remove_improvement(h);
            }
        } else if h.op(EE_SCREENS_PT_OP_BUILDING, 0, 0, &mut []) != 0 {
            remove_technology(h);
        }
    } else if improvement != NO_IMPROVEMENT {
        remove_improvement(h);
    }
    0
}

/// The op as the shared control host's (`EEUIRustControl`, `ee_screenscpp_hud_ctl`): the toolkit's calls it answers
/// itself (dirty, the rectangles, the base's draws, the mouse's release) as its ops, the others as the class's own
/// (`EE_SCREENS_HC_CLASS + op`, `RustOp`); with its a.
pub fn hud_op(op: u32, a: i64) -> (u32, i64) {
    use crate::hud_ctl::{
        EE_SCREENS_HC_CLASS, EE_SCREENS_HC_COPIED_RECT, EE_SCREENS_HC_DRAW_BASE, EE_SCREENS_HC_RECT,
        EE_SCREENS_HC_SET_DIRTY,
    };
    match op {
        EE_SCREENS_PT_OP_DIRTY => (EE_SCREENS_HC_SET_DIRTY, 1),
        EE_SCREENS_PT_OP_RECT => (EE_SCREENS_HC_RECT, a),
        EE_SCREENS_PT_OP_DRAW_RECT => (EE_SCREENS_HC_COPIED_RECT, a),
        EE_SCREENS_PT_OP_DRAW_BASE => (EE_SCREENS_HC_DRAW_BASE, a),
        _ => (EE_SCREENS_HC_CLASS + op, a),
    }
}

#[cfg(test)]
mod hud_op_tests {
    use super::*;
    use crate::hud_ctl::*;

    #[test]
    fn the_shared_ops_are_the_hosts_and_the_others_the_classs() {
        assert_eq!(hud_op(EE_SCREENS_PT_OP_DIRTY, 0), (EE_SCREENS_HC_SET_DIRTY, 1));
        assert_eq!(hud_op(EE_SCREENS_PT_OP_RECT, 0), (EE_SCREENS_HC_RECT, 0));
        assert_eq!(hud_op(EE_SCREENS_PT_OP_DRAW_RECT, 0), (EE_SCREENS_HC_COPIED_RECT, 0));
        assert_eq!(hud_op(EE_SCREENS_PT_OP_DRAW_BASE, 0), (EE_SCREENS_HC_DRAW_BASE, 0));
        let shared =
            [EE_SCREENS_PT_OP_DIRTY, EE_SCREENS_PT_OP_RECT, EE_SCREENS_PT_OP_DRAW_RECT, EE_SCREENS_PT_OP_DRAW_BASE];
        for op in (0..=37).filter(|op| !shared.contains(op)) {
            assert_eq!(hud_op(op, -3), (EE_SCREENS_HC_CLASS + op, -3));
        }
    }
}

#[cfg(test)]
mod golden;
