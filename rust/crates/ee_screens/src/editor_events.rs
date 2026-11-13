//! The scenario editor form's own events with state, whole in Rust behind one C++ event (`EESEERust`,
//! `ee/EESEARust.h`): the hide-UI check box (`EESEEHideUI`: the framed or the full game view posted as a viewport
//! change), the viewport change (`EESEEChangeViewport`: the editor's game view set, its UI shown or hidden, all
//! redrawn), the toolbar's quick load and quick save (`EESEEQuickLoad`, `EESEEQuickSave`: the editor's shared quick
//! save/load action, referenced while the event lives, shown loaded or saved through a quick save action); and the
//! trigger screens' controls' events (`EESEESliderChange`: the slider's value times a scale plus an offset printed
//! into a label through the host's `sprintf` of the event's format; `EESEECheckDisplay`: controls shown while a check
//! box is checked; `EESEEUseObjectArea`: a scenario combo box refilled with the areas or the objects as the area
//! radio or another was checked, its first line picked).

// ---- the kinds (`EESEERust`'s constructor)

/// `EESEEHideUI`.
pub const EE_SCREENS_SEE_HIDE_UI: u32 = 1;
/// `EESEEChangeViewport` (its rectangle and flag: `ee_screens_see_viewport`).
pub const EE_SCREENS_SEE_CHANGE_VIEWPORT: u32 = 2;
/// `EESEEQuickLoad` (object: the quick save/load action).
pub const EE_SCREENS_SEE_QUICK_LOAD: u32 = 3;
/// `EESEEQuickSave` (object: the quick save/load action).
pub const EE_SCREENS_SEE_QUICK_SAVE: u32 = 4;
/// `EESEESliderChange` (its label, format, scale and offset: `ee_screens_see_slider`).
pub const EE_SCREENS_SEE_SLIDER_CHANGE: u32 = 5;
/// `EESEECheckDisplay` (its controls: `ee_screens_see_push_control`).
pub const EE_SCREENS_SEE_CHECK_DISPLAY: u32 = 6;
/// `EESEEUseObjectArea` (its label, combo box and area control: `ee_screens_see_object_area`).
pub const EE_SCREENS_SEE_USE_OBJECT_AREA: u32 = 7;

// ---- the host's ops (`ee_screenscpp_see`: `p` and `q` pointers, `i` up to 2 integers, `f` up to 4 floats)

/// The object p referenced / released.
pub const EE_SCREENS_SEE_OP_REFERENCE: u32 = 1;
pub const EE_SCREENS_SEE_OP_DEREFERENCE: u32 = 2;
/// -> whether the check box p is checked.
pub const EE_SCREENS_SEE_OP_CHECKED: u32 = 3;
/// A viewport change to the rectangle f0..f3 (left, top, right, bottom), the UI hidden when i0, posted -> the
/// post's result.
pub const EE_SCREENS_SEE_OP_POST_VIEWPORT: u32 = 4;
/// -> whether the scenario editor's form exists.
pub const EE_SCREENS_SEE_OP_HAS_EDITOR: u32 = 5;
/// The editor's game view set to the rectangle f0..f3, its UI shown when i0 (`SetGameView`).
pub const EE_SCREENS_SEE_OP_SET_GAME_VIEW: u32 = 6;
/// Everything redrawn (`gUIManager->RedrawAll`).
pub const EE_SCREENS_SEE_OP_REDRAW_ALL: u32 = 7;
/// The scenario editor shows the loaded object p (`PostShowLoadedObject`; the event's form is the editor's).
pub const EE_SCREENS_SEE_OP_SHOW_LOADED: u32 = 8;
/// A quick save action of the quick save/load action p posted.
pub const EE_SCREENS_SEE_OP_QUICK_SAVE: u32 = 9;
/// -> the slider p's current value (`GetCurrentRangeValue`).
pub const EE_SCREENS_SEE_OP_RANGE: u32 = 10;
/// The label p's text set to the value f0 printed by the NUL-terminated format q (`sprintf` into a `UWideString`,
/// the float passed as a double as through the original's `...`).
pub const EE_SCREENS_SEE_OP_SLIDER_TEXT: u32 = 11;
/// The control p's display mode set to i0.
pub const EE_SCREENS_SEE_OP_SET_DISPLAY: u32 = 12;
/// The scenario combo box p refilled with the areas (i0 != 0) or the objects, the label q titled
/// (`FillScenarioComboBox`).
pub const EE_SCREENS_SEE_OP_FILL_SCENARIO: u32 = 13;
/// The combo box p's selection set to i0.
pub const EE_SCREENS_SEE_OP_SET_SELECTION: u32 = 14;

/// The form, the controls and the actions.
pub trait SeeHost {
    fn op(&mut self, op: u32, p: usize, q: usize, i: &mut [i32; 2], f: &mut [f32; 4]) -> i32;
}

fn call<H: SeeHost + ?Sized>(h: &mut H, op: u32, p: usize, ints: [i32; 2], floats: [f32; 4]) -> i32 {
    call2(h, op, p, 0, ints, floats)
}

fn call2<H: SeeHost + ?Sized>(h: &mut H, op: u32, p: usize, q: usize, ints: [i32; 2], floats: [f32; 4]) -> i32 {
    let (mut i, mut f) = (ints, floats);
    h.op(op, p, q, &mut i, &mut f)
}

/// What an event does.
#[derive(Clone, Debug, PartialEq)]
pub enum SeeKind {
    /// the views posted unchecked and checked (the original's +0x0c and +0x1c rectangles): all zero when made,
    /// set by the trigger tab's `Enter` (`ee_screens_see_hide_ui_views`)
    HideUi {
        unchecked: [f32; 4],
        checked: [f32; 4],
    },
    ChangeViewport {
        rectangle: [f32; 4],
        hidden: bool,
    },
    QuickLoad {
        owner: usize,
    },
    QuickSave {
        owner: usize,
    },
    /// `format`: the bytes of the original's `UWideString` format and a NUL
    SliderChange {
        label: usize,
        format: Vec<u8>,
        scale: f32,
        offset: f32,
    },
    CheckDisplay {
        controls: Vec<usize>,
    },
    /// `area`: the control whose check means areas (compared with the event's entity)
    UseObjectArea {
        label: usize,
        combo: usize,
        area: usize,
    },
}

impl SeeKind {
    /// The event of `kind` (`object`: the quick save/load action, referenced); None for an unknown kind.
    pub fn new<H: SeeHost + ?Sized>(h: &mut H, kind: u32, object: usize) -> Option<SeeKind> {
        Some(match kind {
            EE_SCREENS_SEE_HIDE_UI => SeeKind::HideUi { unchecked: [0.0; 4], checked: [0.0; 4] },
            EE_SCREENS_SEE_CHANGE_VIEWPORT => SeeKind::ChangeViewport { rectangle: [0.0; 4], hidden: false },
            EE_SCREENS_SEE_SLIDER_CHANGE => {
                SeeKind::SliderChange { label: 0, format: vec![0], scale: 0.0, offset: 0.0 }
            }
            EE_SCREENS_SEE_CHECK_DISPLAY => SeeKind::CheckDisplay { controls: Vec::new() },
            EE_SCREENS_SEE_USE_OBJECT_AREA => SeeKind::UseObjectArea { label: 0, combo: 0, area: 0 },
            EE_SCREENS_SEE_QUICK_LOAD | EE_SCREENS_SEE_QUICK_SAVE => {
                call(h, EE_SCREENS_SEE_OP_REFERENCE, object, [0; 2], [0.0; 4]);
                if kind == EE_SCREENS_SEE_QUICK_LOAD {
                    SeeKind::QuickLoad { owner: object }
                } else {
                    SeeKind::QuickSave { owner: object }
                }
            }
            _ => return None,
        })
    }

    /// The viewport change's rectangle and flag.
    pub fn set_viewport(&mut self, to: [f32; 4], to_hidden: bool) {
        if let SeeKind::ChangeViewport { rectangle, hidden } = self {
            *rectangle = to;
            *hidden = to_hidden;
        }
    }

    /// The slider event's label, format (up to a NUL), scale and offset (the original's constructor).
    pub fn set_slider(&mut self, to_label: usize, to_format: &[u8], to_scale: f32, to_offset: f32) {
        if let SeeKind::SliderChange { label, format, scale, offset } = self {
            *label = to_label;
            *format = to_format.iter().copied().take_while(|&b| b != 0).chain(core::iter::once(0)).collect();
            *scale = to_scale;
            *offset = to_offset;
        }
    }

    /// A control shown and hidden with the check box (the original's `mControls.push_back`).
    pub fn push_control(&mut self, control: usize) {
        if let SeeKind::CheckDisplay { controls } = self {
            controls.push(control);
        }
    }

    /// The object/area event's label, combo box and area control (the original's constructor).
    pub fn set_object_area(&mut self, to_label: usize, to_combo: usize, to_area: usize) {
        if let SeeKind::UseObjectArea { label, combo, area } = self {
            *label = to_label;
            *combo = to_combo;
            *area = to_area;
        }
    }

    /// The hide-UI event's two views (`EESETriggerScreen::Enter`: the original's inline `SetRectangles`).
    pub fn set_hide_ui_views(&mut self, to_unchecked: [f32; 4], to_checked: [f32; 4]) {
        if let SeeKind::HideUi { unchecked, checked } = self {
            *unchecked = to_unchecked;
            *checked = to_checked;
        }
    }

    /// `ProcessEvent(form, entity)` (the form is the editor's: unused).
    pub fn process<H: SeeHost + ?Sized>(&self, h: &mut H, entity: usize) -> i32 {
        match self {
            SeeKind::HideUi { unchecked, checked } => {
                let is_checked = call(h, EE_SCREENS_SEE_OP_CHECKED, entity, [0; 2], [0.0; 4]) != 0;
                let rectangle = if is_checked { *checked } else { *unchecked };
                // the check box read again for the flag, as the original's second GetIsChecked
                let hidden = call(h, EE_SCREENS_SEE_OP_CHECKED, entity, [0; 2], [0.0; 4]);
                call(h, EE_SCREENS_SEE_OP_POST_VIEWPORT, 0, [hidden, 0], rectangle)
            }
            SeeKind::ChangeViewport { rectangle, hidden } => {
                if call(h, EE_SCREENS_SEE_OP_HAS_EDITOR, 0, [0; 2], [0.0; 4]) != 0 {
                    call(h, EE_SCREENS_SEE_OP_SET_GAME_VIEW, 0, [i32::from(!*hidden), 0], *rectangle);
                    call(h, EE_SCREENS_SEE_OP_REDRAW_ALL, 0, [0; 2], [0.0; 4]);
                }
                0
            }
            SeeKind::QuickLoad { owner } => {
                call(h, EE_SCREENS_SEE_OP_SHOW_LOADED, *owner, [0; 2], [0.0; 4]);
                0
            }
            SeeKind::QuickSave { owner } => {
                call(h, EE_SCREENS_SEE_OP_QUICK_SAVE, *owner, [0; 2], [0.0; 4]);
                0
            }
            SeeKind::SliderChange { label, format, scale, offset } => {
                // the original's `long * float + float`: the value made a float, one product, one sum (no FMA)
                let value = call(h, EE_SCREENS_SEE_OP_RANGE, entity, [0; 2], [0.0; 4]) as f32 * *scale + *offset;
                let text = format.as_ptr() as usize;
                call2(h, EE_SCREENS_SEE_OP_SLIDER_TEXT, *label, text, [0; 2], [value, 0.0, 0.0, 0.0]);
                0
            }
            SeeKind::CheckDisplay { controls } => {
                let mode = i32::from(call(h, EE_SCREENS_SEE_OP_CHECKED, entity, [0; 2], [0.0; 4]) != 0);
                for &control in controls {
                    call(h, EE_SCREENS_SEE_OP_SET_DISPLAY, control, [mode, 0], [0.0; 4]);
                }
                0
            }
            SeeKind::UseObjectArea { label, combo, area } => {
                let areas = i32::from(entity == *area);
                call2(h, EE_SCREENS_SEE_OP_FILL_SCENARIO, *combo, *label, [areas, 0], [0.0; 4]);
                call(h, EE_SCREENS_SEE_OP_SET_SELECTION, *combo, [0; 2], [0.0; 4]);
                0
            }
        }
    }

    /// The event freed: the quick save/load action released.
    pub fn free<H: SeeHost + ?Sized>(&self, h: &mut H) {
        if let SeeKind::QuickLoad { owner } | SeeKind::QuickSave { owner } = self {
            call(h, EE_SCREENS_SEE_OP_DEREFERENCE, *owner, [0; 2], [0.0; 4]);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    struct Rec {
        log: Vec<String>,
        checked: bool,
        editor: bool,
        range: i32,
    }

    impl SeeHost for Rec {
        fn op(&mut self, op: u32, p: usize, q: usize, i: &mut [i32; 2], f: &mut [f32; 4]) -> i32 {
            match op {
                EE_SCREENS_SEE_OP_CHECKED => return i32::from(self.checked),
                EE_SCREENS_SEE_OP_RANGE => return self.range,
                EE_SCREENS_SEE_OP_SLIDER_TEXT => {
                    // the format the host prints with: up to its NUL
                    let format = unsafe { core::ffi::CStr::from_ptr(q as *const core::ffi::c_char) };
                    self.log.push(format!("text p{p} {:?} {}", format.to_str().unwrap(), f[0]));
                    return 0;
                }
                EE_SCREENS_SEE_OP_FILL_SCENARIO => {
                    self.log.push(format!("fill p{p} q{q} {}", i[0]));
                    return 0;
                }
                EE_SCREENS_SEE_OP_HAS_EDITOR => return i32::from(self.editor),
                EE_SCREENS_SEE_OP_POST_VIEWPORT => {
                    self.log.push(format!("post {f:?} {}", i[0]));
                    return 5;
                }
                _ => {}
            }
            self.log.push(format!("{op} p{p} {i:?} {f:?}"));
            0
        }
    }

    fn rec() -> Rec {
        Rec { log: Vec::new(), checked: false, editor: true, range: 0 }
    }

    #[test]
    fn hide_ui_posts_the_view_of_the_check_state() {
        let mut h = rec();
        let e = SeeKind::new(&mut h, EE_SCREENS_SEE_HIDE_UI, 0).unwrap();
        assert_eq!(e.process(&mut h, 2), 5);
        h.checked = true;
        e.process(&mut h, 2);
        assert_eq!(h.log, ["post [0.0, 0.0, 0.0, 0.0] 0", "post [0.0, 0.0, 0.0, 0.0] 1"]);
        // the trigger tab's views: unchecked the first, checked the second
        let mut e = e;
        e.set_hide_ui_views([0.0, 0.0, 1024.0, 606.0], [0.0, 0.0, 1024.0, 447.0]);
        h.log.clear();
        e.process(&mut h, 2);
        h.checked = false;
        e.process(&mut h, 2);
        assert_eq!(h.log, ["post [0.0, 0.0, 1024.0, 447.0] 1", "post [0.0, 0.0, 1024.0, 606.0] 0"]);
    }

    #[test]
    fn viewport_change_sets_the_view_when_the_editor_is_open() {
        let mut h = rec();
        let mut e = SeeKind::new(&mut h, EE_SCREENS_SEE_CHANGE_VIEWPORT, 0).unwrap();
        e.set_viewport([1.0, 2.0, 3.0, 4.0], true);
        e.process(&mut h, 0);
        assert_eq!(h.log, ["6 p0 [0, 0] [1.0, 2.0, 3.0, 4.0]", "7 p0 [0, 0] [0.0, 0.0, 0.0, 0.0]"]);
        h.log.clear();
        h.editor = false;
        e.process(&mut h, 0);
        assert!(h.log.is_empty());
        assert_eq!(SeeKind::new(&mut h, 9, 0), None);
    }

    #[test]
    fn quick_load_and_save_hold_their_action() {
        let mut h = rec();
        let l = SeeKind::new(&mut h, EE_SCREENS_SEE_QUICK_LOAD, 0x40).unwrap();
        let s = SeeKind::new(&mut h, EE_SCREENS_SEE_QUICK_SAVE, 0x40).unwrap();
        l.process(&mut h, 0);
        s.process(&mut h, 0);
        l.free(&mut h);
        s.free(&mut h);
        let ops: Vec<&str> = h.log.iter().map(|x| x.split(' ').next().unwrap()).collect();
        assert_eq!(ops, ["1", "1", "8", "9", "2", "2"]);
        assert!(h.log.iter().all(|x| x.contains("p64")));
    }

    #[test]
    fn slider_prints_value_times_scale_plus_offset() {
        let mut h = rec();
        let mut e = SeeKind::new(&mut h, EE_SCREENS_SEE_SLIDER_CHANGE, 0).unwrap();
        e.set_slider(0x30, b"%#2.1f", 0.1, 0.1);
        h.range = 17;
        assert_eq!(e.process(&mut h, 2), 0);
        // 17.0f * 0.1f + 0.1f in f32, as the original
        let want = 17.0f32 * 0.1f32 + 0.1f32;
        assert_eq!(h.log, [format!("text p48 \"%#2.1f\" {want}")]);
        assert_eq!(want.to_bits(), 0x3fe6_6667);
        // a format with its NUL keeps one NUL
        e.set_slider(0x30, b"%2.0f\0", 1.0, 0.0);
        if let SeeKind::SliderChange { format, .. } = &e {
            assert_eq!(format, b"%2.0f\0");
        }
    }

    #[test]
    fn check_display_shows_its_controls_while_checked() {
        let mut h = rec();
        let mut e = SeeKind::new(&mut h, EE_SCREENS_SEE_CHECK_DISPLAY, 0).unwrap();
        e.push_control(0x10);
        e.push_control(0x20);
        h.checked = true;
        e.process(&mut h, 2);
        h.checked = false;
        e.process(&mut h, 2);
        assert_eq!(
            h.log,
            [
                "12 p16 [1, 0] [0.0, 0.0, 0.0, 0.0]",
                "12 p32 [1, 0] [0.0, 0.0, 0.0, 0.0]",
                "12 p16 [0, 0] [0.0, 0.0, 0.0, 0.0]",
                "12 p32 [0, 0] [0.0, 0.0, 0.0, 0.0]"
            ]
        );
    }

    #[test]
    fn object_area_refills_with_areas_from_the_area_control() {
        let mut h = rec();
        let mut e = SeeKind::new(&mut h, EE_SCREENS_SEE_USE_OBJECT_AREA, 0).unwrap();
        e.set_object_area(0x30, 0x40, 0x50);
        e.process(&mut h, 0x50);
        e.process(&mut h, 0x60);
        assert_eq!(
            h.log,
            [
                "fill p64 q48 1",
                "14 p64 [0, 0] [0.0, 0.0, 0.0, 0.0]",
                "fill p64 q48 0",
                "14 p64 [0, 0] [0.0, 0.0, 0.0, 0.0]"
            ]
        );
        // the events without an action release nothing
        e.free(&mut h);
        assert_eq!(h.log.len(), 4);
    }
}
