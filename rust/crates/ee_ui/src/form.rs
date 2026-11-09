//! A `UIForm`'s keyboard focus walk and mouse hit test (`User Interface/UIForm.cpp`: `Tab`, `ShiftTab`,
//! `CheckedRadioButton` and the control search of `UpdateMouseLocation`).
//!
//! The form and its controls stay C++ objects; the walk sees them by index through [`FormHost`] (the slots of
//! `mControls`, each control's virtual `GetType`/`GetTabable`, its display mode and group) and moves the tab stop
//! with the C++ `SetTabStop` (which calls the controls' `TabbedFrom`/`TabbedTo`). The hit test sees the entity groups
//! through [`HitHost`].
//!
//! Faithful to the C++, including its loops: a run of radio buttons is one stop (the checked one), `ShiftTab`'s run
//! skip stops at index 1 (`> 0`), and a form whose only eligible control sits inside a radio run the walk jumps over
//! loops for ever, as the C++ did (the game's forms do not have one).

use core::ffi::{c_long, c_ulong};

use crate::rect::{UiPoint, UiRect};

/// `kControlTypeRadioButton`.
pub const RADIO_BUTTON: c_ulong = 6;

/// The form's control slots as the focus walk asks about them.
pub trait FormHost {
    /// `mControlCount` (signed, as the C++ compares it).
    fn count(&self) -> c_long;
    /// `mControls[i] != NULL`.
    fn exists(&self, i: c_long) -> bool;
    /// `mControls[i]->GetType()` (virtual).
    fn kind(&self, i: c_long) -> c_ulong;
    /// `static_cast<UIRadioButton*>(mControls[i])->GetIsChecked()`.
    fn checked(&self, i: c_long) -> bool;
    /// `GetDisplayMode() == kDisplayModeShown && GetEntityGroupById(GetId())->GetIsEnabled()`.
    fn shown_enabled(&self, i: c_long) -> bool;
    /// `mControls[i]->GetTabable()` (virtual).
    fn tabable(&self, i: c_long) -> bool;
    /// `UIForm::SetTabStop(i)`.
    fn set_tab_stop(&mut self, i: c_ulong) -> bool;
}

fn is_radio(h: &impl FormHost, i: c_long) -> bool {
    h.exists(i) && h.kind(i) == RADIO_BUTTON
}

/// `CheckedRadioButton`: a radio button's index moved to the checked button of its run (first looking back, then
/// on); other controls and checked buttons stay.
pub fn checked_radio_button(h: &impl FormHost, index: &mut c_long) {
    let mut i = *index;
    if h.kind(i) != RADIO_BUTTON || h.checked(i) {
        return;
    }
    i -= 1;
    while i >= 0 && is_radio(h, i) {
        if h.checked(i) {
            *index = i;
            break;
        }
        i -= 1;
    }
    if i != *index {
        i = *index + 1;
        while i < h.count() && is_radio(h, i) {
            if h.checked(i) {
                *index = i;
                break;
            }
            i += 1;
        }
    }
}

/// One step of the walk at an existing control: a shown control of an enabled group takes the stop when tabbable
/// (a radio run gives it to its checked button) -> whether the walk ends.
fn visit(h: &mut impl FormHost, index: &mut c_long) -> bool {
    let control = *index;
    if h.shown_enabled(control) {
        checked_radio_button(h, index);
        if h.tabable(control) {
            h.set_tab_stop(*index as c_ulong);
            return true;
        }
    }
    false
}

/// Past a run of radio buttons forward (`do ++ while`).
fn skip_forward(h: &impl FormHost, i: &mut c_long) {
    loop {
        *i += 1;
        if !(*i < h.count() && is_radio(h, *i)) {
            break;
        }
    }
}

/// Past a run of radio buttons backward, never below index 0 (the C++'s `> 0`).
fn skip_backward(h: &impl FormHost, i: &mut c_long) {
    *i -= 1;
    while *i > 0 && is_radio(h, *i) {
        *i -= 1;
    }
}

/// `Tab`: the tab stop on to the next shown, enabled, tabbable control, wrapping round; nothing when tabbing is off.
pub fn tab(h: &mut impl FormHost, tabbing: bool, tab_stop: c_ulong) {
    if !tabbing {
        return;
    }
    let stop = tab_stop as c_long;
    let mut i = stop;
    if h.kind(i) == RADIO_BUTTON {
        skip_forward(h, &mut i);
    } else {
        i += 1;
    }
    loop {
        if i == h.count() {
            i = 0;
        }
        if i == stop {
            break;
        }
        if !h.exists(i) {
            i += 1;
            continue;
        }
        let control = i;
        if visit(h, &mut i) {
            break;
        }
        if h.kind(control) == RADIO_BUTTON {
            skip_forward(h, &mut i);
        } else {
            i += 1;
        }
    }
}

/// `ShiftTab`: the tab stop back to the previous shown, enabled, tabbable control, wrapping round.
pub fn shift_tab(h: &mut impl FormHost, tabbing: bool, tab_stop: c_ulong) {
    if !tabbing {
        return;
    }
    let mut i = tab_stop as c_long;
    if h.kind(i) == RADIO_BUTTON {
        skip_backward(h, &mut i);
    } else {
        i -= 1;
    }
    loop {
        if i < 0 {
            i = h.count() - 1;
        }
        if i == tab_stop as c_long {
            break;
        }
        if !h.exists(i) {
            i -= 1;
            continue;
        }
        let control = i;
        if visit(h, &mut i) {
            break;
        }
        if h.kind(control) == RADIO_BUTTON {
            skip_backward(h, &mut i);
        } else {
            i -= 1;
        }
    }
}

/// The entity groups as the hit test asks about them.
pub trait HitHost {
    /// `mControlGroupCount`.
    fn group_count(&self) -> c_long;
    /// `mEntityGroups[g]->GetIsEnabled()`.
    fn group_enabled(&self, g: c_long) -> bool;
    /// `mEntityGroups[g]->GetEntityCount()`.
    fn entity_count(&self, g: c_long) -> c_ulong;
    /// The entity as a control: `GetDisplayMode() == kDisplayModeHidden`.
    fn entity_hidden(&self, g: c_long, j: c_ulong) -> bool;
    /// The entity as a control: `GetRectangle()`.
    fn entity_rect(&self, g: c_long, j: c_ulong) -> UiRect;
}

/// `UpdateMouseLocation`'s search: the first control (groups from the last, each group's entities in order) not
/// hidden whose rectangle contains the point -> (group, entity).
pub fn hit(h: &impl HitHost, at: UiPoint) -> Option<(c_long, c_ulong)> {
    let mut g = h.group_count() - 1;
    while g >= 0 {
        if h.group_enabled(g) {
            let mut j = 0;
            while j < h.entity_count(g) {
                if !h.entity_hidden(g, j) && h.entity_rect(g, j).contains(at) {
                    return Some((g, j));
                }
                j += 1;
            }
        }
        g -= 1;
    }
    None
}

#[cfg(test)]
mod tests {
    use super::*;

    const GOLDEN: &str = include_str!("golden_form.txt");

    #[derive(Clone)]
    struct Control {
        kind: c_ulong,
        tab: bool,
        mode: c_ulong,
        checked: bool,
        shown_enabled: bool,
        rect: UiRect,
    }

    /// A form as the golden driver built it; `SetTabStop` as the C++ (trace lines for TabbedFrom/TabbedTo).
    struct Form {
        controls: Vec<Option<Control>>,
        groups: Vec<(bool, Vec<usize>)>,
        stop: c_ulong,
        log: Vec<String>,
    }

    impl Form {
        fn at(&self, i: c_long) -> &Control {
            self.controls[i as usize].as_ref().unwrap()
        }
    }

    impl FormHost for Form {
        fn count(&self) -> c_long {
            self.controls.len() as c_long
        }
        fn exists(&self, i: c_long) -> bool {
            self.controls[i as usize].is_some()
        }
        fn kind(&self, i: c_long) -> c_ulong {
            self.at(i).kind
        }
        fn checked(&self, i: c_long) -> bool {
            self.at(i).checked
        }
        fn shown_enabled(&self, i: c_long) -> bool {
            self.at(i).shown_enabled
        }
        fn tabable(&self, i: c_long) -> bool {
            self.at(i).tab
        }
        fn set_tab_stop(&mut self, i: c_ulong) -> bool {
            let old = self.stop as c_long;
            if self.exists(old) {
                self.log.push(format!("  from {old}"));
            }
            if self.exists(i as c_long) && self.tabable(i as c_long) {
                self.log.push(format!("  to {i}"));
                self.stop = i;
                true
            } else {
                if self.exists(old) {
                    self.log.push(format!("  to {old}"));
                }
                false
            }
        }
    }

    impl HitHost for Form {
        fn group_count(&self) -> c_long {
            self.groups.len() as c_long
        }
        fn group_enabled(&self, g: c_long) -> bool {
            self.groups[g as usize].0
        }
        fn entity_count(&self, g: c_long) -> c_ulong {
            self.groups[g as usize].1.len() as c_ulong
        }
        fn entity_hidden(&self, g: c_long, j: c_ulong) -> bool {
            self.at(self.groups[g as usize].1[j as usize] as c_long).mode == 0
        }
        fn entity_rect(&self, g: c_long, j: c_ulong) -> UiRect {
            self.at(self.groups[g as usize].1[j as usize] as c_long).rect
        }
    }

    fn fx(s: &str) -> f32 {
        f32::from_bits(u32::from_str_radix(s, 16).unwrap())
    }

    #[test]
    fn replays_the_cpp_focus_walk_and_hit_test() {
        let golden: Vec<&str> = GOLDEN.lines().collect();
        let mut out: Vec<String> = Vec::new();
        let mut form: Option<Form> = None;
        let mut over: c_long = -1;
        let mut captured: c_long = -1;
        let mut tabbing = true;
        for line in &golden {
            let t: Vec<&str> = line.split(' ').collect();
            match t[0] {
                "form" => {
                    out.push(line.to_string());
                    let n: usize = t[3].parse().unwrap();
                    let groups = t[6..].iter().map(|e| (*e == "1", Vec::new())).collect();
                    form = Some(Form { controls: vec![None; n], groups, stop: 0, log: Vec::new() });
                    over = -1;
                    captured = -1;
                }
                "control" => {
                    out.push(line.to_string());
                    let f = form.as_mut().unwrap();
                    if t[2] == "null" {
                        continue;
                    }
                    let i: usize = t[1].parse().unwrap();
                    let group: usize = t[11].parse().unwrap();
                    f.controls[i] = Some(Control {
                        kind: t[3].parse().unwrap(),
                        tab: t[5] == "1",
                        mode: t[7].parse().unwrap(),
                        checked: t[9] == "1",
                        shown_enabled: t[13] == "1",
                        rect: UiRect { bottom: fx(t[15]), left: fx(t[16]), right: fx(t[17]), top: fx(t[18]) },
                    });
                    f.groups[group].1.push(i);
                }
                "radio" => {
                    let i: c_long = t[1].parse().unwrap();
                    let mut j = i;
                    checked_radio_button(form.as_ref().unwrap(), &mut j);
                    out.push(format!("radio {i} -> {j}"));
                }
                "stop" => {
                    out.push(line.to_string());
                    let f = form.as_mut().unwrap();
                    f.stop = t[1].parse().unwrap();
                    tabbing = t[3] == "1";
                }
                "tab" | "shift-tab" => {
                    out.push(line.to_string());
                    let f = form.as_mut().unwrap();
                    let stop = f.stop;
                    if t[0] == "tab" {
                        tab(f, tabbing, stop);
                    } else {
                        shift_tab(f, tabbing, stop);
                    }
                    out.append(&mut f.log);
                    out.push(format!("  = {}", f.stop));
                }
                "set-stop" => {
                    // the C++ SetTabStop itself (the fake's model of it, checked against the trace)
                    out.push(line.to_string());
                    let f = form.as_mut().unwrap();
                    // the driver's printf read mTabStop before the call (MSVC ABI: arguments right to left)
                    let before = f.stop;
                    let r = f.set_tab_stop(t[1].parse().unwrap());
                    out.append(&mut f.log);
                    out.push(format!("  = {} {}", r as u8, before));
                }
                "capture" => {
                    out.push(line.to_string());
                    captured = t[1].parse().unwrap();
                }
                "mouse" => {
                    out.push(line.to_string());
                    let f = form.as_ref().unwrap();
                    let at = UiPoint { x: fx(t[1]), y: fx(t[2]) };
                    // UpdateMouseLocation: the captured control while the mouse is on it, else the search
                    let control = if captured >= 0 && f.at(captured).rect.contains(at) {
                        captured
                    } else {
                        hit(f, at).map_or(-1, |(g, j)| f.groups[g as usize].1[j as usize] as c_long)
                    };
                    if control != over {
                        if over >= 0 {
                            out.push(format!("  trigger 3f3 on {over}"));
                        }
                        over = control;
                        if over >= 0 {
                            out.push(format!("  trigger 3f2 on {over}"));
                        }
                    }
                    out.push(format!("  = {over}"));
                }
                _ => {}
            }
        }
        assert_eq!(out.len(), golden.len(), "line count");
        for (k, (a, b)) in out.iter().zip(golden.iter()).enumerate() {
            assert_eq!(a, b, "line {}", k + 1);
        }
    }

    fn form(spec: &[(c_ulong, bool, bool)]) -> Form {
        // (kind, checked, tabbable), all shown in one enabled group
        let controls = spec
            .iter()
            .map(|&(kind, checked, tab)| {
                Some(Control { kind, tab, mode: 1, checked, shown_enabled: true, rect: UiRect::default() })
            })
            .collect();
        Form { controls, groups: vec![(true, (0..spec.len()).collect())], stop: 0, log: Vec::new() }
    }

    #[test]
    fn a_radio_run_is_one_stop_at_its_checked_button() {
        let mut f = form(&[(2, false, true), (6, false, true), (6, true, true), (6, false, true), (2, false, true)]);
        tab(&mut f, true, 0);
        assert_eq!(f.stop, 2);
        tab(&mut f, true, 2);
        assert_eq!(f.stop, 4);
        shift_tab(&mut f, true, 4);
        assert_eq!(f.stop, 2);
        shift_tab(&mut f, true, 2);
        assert_eq!(f.stop, 0);
        shift_tab(&mut f, true, 0); // wraps to the end
        assert_eq!(f.stop, 4);
        let mut i = 3;
        checked_radio_button(&f, &mut i);
        assert_eq!(i, 2);
    }

    #[test]
    fn untabbable_and_off_change_nothing() {
        let mut f = form(&[(2, false, true), (3, false, false), (2, false, true)]);
        tab(&mut f, true, 0);
        assert_eq!(f.stop, 2);
        f.log.clear();
        tab(&mut f, false, 2);
        shift_tab(&mut f, false, 2);
        assert!(f.log.is_empty());
    }

    #[test]
    fn the_hit_test_takes_the_last_group_first_and_skips_hidden_controls() {
        let mut f = form(&[(2, false, true), (2, false, true), (2, false, true)]);
        for c in f.controls.iter_mut().flatten() {
            c.rect = UiRect::new(0.0, 0.0, 10.0, 10.0);
        }
        f.groups = vec![(true, vec![0]), (true, vec![1, 2])];
        assert_eq!(hit(&f, UiPoint { x: 5.0, y: 5.0 }), Some((1, 0)));
        f.controls[1].as_mut().unwrap().mode = 0;
        assert_eq!(hit(&f, UiPoint { x: 5.0, y: 5.0 }), Some((1, 1)));
        f.groups[1].0 = false;
        assert_eq!(hit(&f, UiPoint { x: 5.0, y: 5.0 }), Some((0, 0)));
        assert_eq!(hit(&f, UiPoint { x: 11.0, y: 5.0 }), None);
    }
}
