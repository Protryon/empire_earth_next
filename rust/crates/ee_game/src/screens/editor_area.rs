//! The trigger tab's area sub-screen whole in Rust (`EESETSArea`, once `Scenario Editor/EESETSArea.cpp`
//! 0x57ccf3..0x57da08): its constructor (the red "no area selected" label, the selection mode combo box, the centre
//! button), the destructor, the list buttons, `Update`, `Deactivate`, `Activate`, `Store`, `GetCenter`, `GetNames`,
//! `Hide`, `Load`, `SetAreaMode`, `Show` and `MouseMove` (the rectangle painted with the left button, or the
//! anchor tile set on its release).
//!
//! The controls are the shared maker's records; the scenario's areas, the trigger tab's boxes, the editor's paint
//! area and highlight, the mouse, the map's picking and the paint action (`EESEARust` over `EESEAPaintArea`) are the
//! screen's host, `ee_screenscpp_sear` (ops from 600, `Scenario Editor/EESETSArea.cpp`). The sub-screen's
//! selection (`EESETSBaseObject::mSelection`) stays the C++ base's: each call passes it. A host call does not come
//! back into the screen; its state is in `Cell`s all the same (the C++ events reach it through `&self`).

use core::cell::Cell;

use crate::screens::editor_object::{
    screen_event, EE_SCREENS_SEOB_SM_DELETE, EE_SCREENS_SEOB_SM_DOWN, EE_SCREENS_SEOB_SM_DUPLICATE,
    EE_SCREENS_SEOB_SM_NEW, EE_SCREENS_SEOB_SM_SORT, EE_SCREENS_SEOB_SM_UP, EE_SCREENS_SEPL_COMBO_LINES_STRINGS,
    EE_SCREENS_SEPL_COMBO_PLAIN, EE_SCREENS_SEPL_ON_BUTTON_UP, EE_SCREENS_SEPL_ON_SELECTION,
};
use crate::screens::editor_players::{
    display, EeScreensSeplCtl, Maker, SeplHost, EE_SCREENS_SEPL_ALIGN_CENTER, EE_SCREENS_SEPL_BUTTON,
    EE_SCREENS_SEPL_FG, EE_SCREENS_SEPL_LABEL, EE_SCREENS_SEPL_LIST_COMBO, EE_SCREENS_SEPL_OP_DELETE,
};
use crate::screens::scenario_editor::{EE_SCREENS_SE_AREA_CHECK, EE_SCREENS_SE_CENTER_CAMERA_ON_AREA};

/// `EeScreensSeplCtl::flags`: the text red (and transparent); a combo box's list's selected line left in its
/// default colour (not set blue).
pub const EE_SCREENS_SEPL_FG_RED: u32 = 256;
pub const EE_SCREENS_SEPL_NO_SELECT_COLOR: u32 = 512;

// ---- the screen's host (`ee_screenscpp_sear`)

/// The areas' list operation i0 (`EE_SCREENS_SEOB_SM_*`) on the area i1 (the selection + 2).
pub const EE_SCREENS_SEAR_OP_LIST: u32 = 600;
/// The user areas' names into the `std::vector<UWideString>` p0.
pub const EE_SCREENS_SEAR_OP_NAMES: u32 = 601;
/// The editor's paint area highlighted (i0 = 1) or not / set to (f0, f1, f2, f3) (`SetPaintArea`).
pub const EE_SCREENS_SEAR_OP_HIGHLIGHT: u32 = 602;
pub const EE_SCREENS_SEAR_OP_PAINT_AREA: u32 = 603;
/// -> whether the editor's UI is hidden.
pub const EE_SCREENS_SEAR_OP_UI_HIDDEN: u32 = 604;
/// The trigger tab's text box hidden and alphabetic sort shown (`Activate`) / its text box shown (`Load`).
pub const EE_SCREENS_SEAR_OP_ACTIVATE: u32 = 605;
pub const EE_SCREENS_SEAR_OP_SHOW_TEXT: u32 = 606;
/// The area i0: -> whether there is one; its rectangle into f, its anchor into i1, i2, its flags into i3 (1 a
/// rectangle, 2 an anchor) and its select mode into the combo box p0 (`Load`'s reads, the name and text boxes set
/// from it).
pub const EE_SCREENS_SEAR_OP_LOAD: u32 = 607;
/// The area i0's name and text from the trigger tab's boxes, its select mode from the combo box p0 (`Store`'s
/// first lines) / its rectangle set to f (`EESMArea::SetRectangle`) / its anchor to (i1, i2) (`SetAnchor`).
pub const EE_SCREENS_SEAR_OP_STORE: u32 = 608;
pub const EE_SCREENS_SEAR_OP_SET_RECT: u32 = 609;
pub const EE_SCREENS_SEAR_OP_SET_ANCHOR: u32 = 610;
/// The rectangle f's centre (`GERectangle::GetCenter`) -> into f0, f1.
pub const EE_SCREENS_SEAR_OP_CENTER: u32 = 611;
/// -> whether the point (f0, f1) (the mouse's with i0 = 1) is in the game view.
pub const EE_SCREENS_SEAR_OP_IN_VIEW: u32 = 612;
/// The tile under the point (f0, f1) (the mouse's with i0 = 1) -> whether the map picks one (`PickScreenTile`),
/// its x, y into i1, i2; / the ground point under it (`GetTileAt`, the screen line solved at the tile's height)
/// -> its x, y truncated into i1, i2.
pub const EE_SCREENS_SEAR_OP_PICK: u32 = 613;
pub const EE_SCREENS_SEAR_OP_GROUND: u32 = 614;
/// -> the left mouse button: 1 held (value above 0), 2 just pressed, 4 just released.
pub const EE_SCREENS_SEAR_OP_LBUTTON: u32 = 615;
/// The paint action (`EESEARust` over `EESEAPaintArea`) begun at the tile (i0, i1) -> its handle into p0; the
/// mouse kept in the game view and the edge scroll's edges set to it.
pub const EE_SCREENS_SEAR_OP_PAINT_BEGIN: u32 = 616;
/// The paint action p0's corner dragged to the tile (i0, i1).
pub const EE_SCREENS_SEAR_OP_PAINT_CORNER: u32 = 617;
/// The paint action p0 ended: the mouse and the edge scroll given the window back, its rectangle into f, the
/// action released.
pub const EE_SCREENS_SEAR_OP_PAINT_END: u32 = 618;
/// The paint action p0 released (the destructor's).
pub const EE_SCREENS_SEAR_OP_PAINT_RELEASE: u32 = 619;

/// The screen's slots (`ee_screens_sear_call`, with the selection): `Sort`, `Update`, `MoveDown`, `Delete`,
/// `Deactivate`, `Duplicate`, `Activate`, `Store`, `Hide`, `Load`, `MoveUp`, `New`, `Show`; `SetAreaMode(a != 0)`.
pub const EE_SCREENS_SEAR_SORT: u32 = 1;
pub const EE_SCREENS_SEAR_UPDATE: u32 = 2;
pub const EE_SCREENS_SEAR_MOVE_DOWN: u32 = 3;
pub const EE_SCREENS_SEAR_DELETE: u32 = 4;
pub const EE_SCREENS_SEAR_DEACTIVATE: u32 = 5;
pub const EE_SCREENS_SEAR_DUPLICATE: u32 = 6;
pub const EE_SCREENS_SEAR_ACTIVATE: u32 = 7;
pub const EE_SCREENS_SEAR_STORE: u32 = 8;
pub const EE_SCREENS_SEAR_HIDE: u32 = 9;
pub const EE_SCREENS_SEAR_LOAD: u32 = 10;
pub const EE_SCREENS_SEAR_MOVE_UP: u32 = 11;
pub const EE_SCREENS_SEAR_NEW: u32 = 12;
pub const EE_SCREENS_SEAR_SHOW: u32 = 13;
pub const EE_SCREENS_SEAR_SET_MODE: u32 = 14;

const IDS_NO_AREA_SELECTED: u32 = 30326;
const IDS_SELECT_RECTANGULAR_AREA: u32 = 30700;
const IDS_SELECT_TOWN_TILE: u32 = 30702;
const IDS_CENTER_CAMERA_ON_AREA: u32 = 30325;

/// The sub-screen's control group on the trigger form.
const GROUP_AREA: u32 = 11;

fn call<H: SeplHost + ?Sized>(h: &mut H, op: u32, p0: usize, i: [i32; 4], f: [f32; 4]) -> (i32, [i32; 4], [f32; 4]) {
    let (mut i, mut f) = (i, f);
    let r = h.op(op, [p0, 0, 0], &mut i, &mut f);
    (r, i, f)
}

fn op<H: SeplHost + ?Sized>(h: &mut H, op: u32, a: i32) -> i32 {
    call(h, op, 0, [a, 0, 0, 0], [0.0; 4]).0
}

/// The area sub-screen (`EESETSArea`): its controls, the painted rectangle and anchor point, the paint action
/// while the left button paints, the mode and what the area has.
#[derive(Debug, Default)]
pub struct AreaScreen {
    pub label: usize,
    pub mode_combo: usize,
    pub center_button: usize,
    /// left, top, right, bottom
    pub rectangle: Cell<[f32; 4]>,
    pub point: Cell<[f32; 2]>,
    pub paint_action: Cell<usize>,
    pub rectangle_mode: Cell<bool>,
    pub has_rectangle: Cell<bool>,
    pub has_point: Cell<bool>,
}

impl AreaScreen {
    /// The constructor (0x57ccf3) on `form` from the control index `*index` (advanced): the red label, the mode
    /// combo box (its selection's `EESEEAreaCheck`), the centre button (`EESEECenterCameraOnArea`); rectangle
    /// mode, no area drawn.
    pub fn new<H: SeplHost + ?Sized>(h: &mut H, form: usize, index: &mut u32) -> AreaScreen {
        let mut m = Maker { h, form, index: *index };
        let label = m.make(
            EeScreensSeplCtl::new(EE_SCREENS_SEPL_LABEL, 0x89b, 329.0, 580.0, 250.0, 20.0)
                .string(IDS_NO_AREA_SELECTED)
                .bg(0.4)
                .align(EE_SCREENS_SEPL_ALIGN_CENTER)
                .flags(EE_SCREENS_SEPL_FG_RED)
                .group(GROUP_AREA),
        );
        let mut combo = EeScreensSeplCtl::new(EE_SCREENS_SEPL_LIST_COMBO, 0x898, 329.0, 504.0, 250.0, 20.0)
            .bg(1.0)
            .bg2(0.4)
            .flags(EE_SCREENS_SEPL_FG | EE_SCREENS_SEPL_NO_SELECT_COLOR)
            .extra([EE_SCREENS_SEPL_COMBO_PLAIN, EE_SCREENS_SEPL_COMBO_LINES_STRINGS, 0, 1])
            .list(100, 0x100)
            .group(GROUP_AREA);
        combo.string = IDS_SELECT_RECTANGULAR_AREA;
        combo.number = (IDS_SELECT_TOWN_TILE - IDS_SELECT_RECTANGULAR_AREA + 1) as i32;
        // (the original gives the two their events before they join the group: the same state)
        let mode_combo = m.make(combo);
        screen_event(m.h, mode_combo, EE_SCREENS_SE_AREA_CHECK, EE_SCREENS_SEPL_ON_SELECTION);
        let center_button = m.make(
            EeScreensSeplCtl::new(EE_SCREENS_SEPL_BUTTON, 0x89a, 329.0, 580.0, 250.0, 20.0)
                .string(IDS_CENTER_CAMERA_ON_AREA)
                .bg(0.4)
                .flags(EE_SCREENS_SEPL_FG)
                .extra([2, 0, 0, 0])
                .group(GROUP_AREA),
        );
        screen_event(m.h, center_button, EE_SCREENS_SE_CENTER_CAMERA_ON_AREA, EE_SCREENS_SEPL_ON_BUTTON_UP);
        *index = m.index;
        AreaScreen { label, mode_combo, center_button, rectangle_mode: Cell::new(true), ..AreaScreen::default() }
    }

    /// The destructor (0x57d09b): the combo box, the button, the label; a rectangle still painted released.
    pub fn destroy<H: SeplHost + ?Sized>(&self, h: &mut H) {
        for c in [self.mode_combo, self.center_button, self.label] {
            call(h, EE_SCREENS_SEPL_OP_DELETE, c, [0; 4], [0.0; 4]);
        }
        if self.paint_action.get() != 0 {
            call(h, EE_SCREENS_SEAR_OP_PAINT_RELEASE, self.paint_action.get(), [0; 4], [0.0; 4]);
        }
    }

    fn highlight<H: SeplHost + ?Sized>(&self, h: &mut H, shown: bool) {
        op(h, EE_SCREENS_SEAR_OP_HIGHLIGHT, shown as i32);
    }

    fn paint_area<H: SeplHost + ?Sized>(&self, h: &mut H, r: [f32; 4]) {
        call(h, EE_SCREENS_SEAR_OP_PAINT_AREA, 0, [0; 4], r);
    }

    fn point_rect(&self) -> [f32; 4] {
        let p = self.point.get();
        [p[0], p[1], p[0], p[1]]
    }

    /// The label or the centre button shown: the button when the area has `has` (and is highlighted at `r`), the
    /// label otherwise; with `labels` false (the editor's UI hidden) the two left as they are.
    fn show_area<H: SeplHost + ?Sized>(&self, h: &mut H, has: Option<[f32; 4]>, labels: bool) {
        match has {
            Some(r) => {
                self.highlight(h, true);
                self.paint_area(h, r);
            }
            None => self.highlight(h, false),
        }
        if labels {
            display(h, self.label, has.is_none());
            display(h, self.center_button, has.is_some());
        }
    }

    /// The slot `what` (`EE_SCREENS_SEAR_*`) with the sub-screen's selection `sel` and argument `a`.
    pub fn call<H: SeplHost + ?Sized>(&self, h: &mut H, what: u32, sel: i32, a: i32) {
        let area = sel.wrapping_add(2);
        let list = |h: &mut H, o: i32, k: i32| {
            call(h, EE_SCREENS_SEAR_OP_LIST, 0, [o, k, 0, 0], [0.0; 4]);
        };
        match what {
            EE_SCREENS_SEAR_SORT => list(h, EE_SCREENS_SEOB_SM_SORT, 0),
            EE_SCREENS_SEAR_UPDATE => self.update(h),
            EE_SCREENS_SEAR_MOVE_DOWN => list(h, EE_SCREENS_SEOB_SM_DOWN, area),
            EE_SCREENS_SEAR_MOVE_UP => list(h, EE_SCREENS_SEOB_SM_UP, area),
            EE_SCREENS_SEAR_DELETE => {
                list(h, EE_SCREENS_SEOB_SM_DELETE, area);
                self.highlight(h, false);
            }
            EE_SCREENS_SEAR_DEACTIVATE => self.highlight(h, false),
            EE_SCREENS_SEAR_DUPLICATE => list(h, EE_SCREENS_SEOB_SM_DUPLICATE, area),
            EE_SCREENS_SEAR_NEW => list(h, EE_SCREENS_SEOB_SM_NEW, 0),
            EE_SCREENS_SEAR_ACTIVATE => {
                op(h, EE_SCREENS_SEAR_OP_ACTIVATE, 0);
            }
            EE_SCREENS_SEAR_STORE => self.store(h, area),
            EE_SCREENS_SEAR_HIDE => {
                for c in [self.label, self.mode_combo, self.center_button] {
                    display(h, c, false);
                }
            }
            EE_SCREENS_SEAR_LOAD => self.load(h, area),
            EE_SCREENS_SEAR_SHOW => {
                let none = !self.has_rectangle.get() && !self.has_point.get();
                display(h, self.label, none);
                display(h, self.center_button, !none);
                display(h, self.mode_combo, true);
            }
            EE_SCREENS_SEAR_SET_MODE => self.set_area_mode(h, a != 0),
            _ => {}
        }
    }

    /// `Store` (0x57d272): the area's name, text and select mode, then its rectangle and anchor if the screen
    /// has them.
    fn store<H: SeplHost + ?Sized>(&self, h: &mut H, area: i32) {
        call(h, EE_SCREENS_SEAR_OP_STORE, self.mode_combo, [area, 0, 0, 0], [0.0; 4]);
        if self.has_rectangle.get() {
            call(h, EE_SCREENS_SEAR_OP_SET_RECT, 0, [area, 0, 0, 0], self.rectangle.get());
        }
        if self.has_point.get() {
            let p = self.point.get();
            call(h, EE_SCREENS_SEAR_OP_SET_ANCHOR, 0, [area, p[0] as i32, p[1] as i32, 0], [0.0; 4]);
        }
    }

    /// `Load` (0x57d41b): the area's texts and mode, its rectangle or anchor highlighted (or the label shown).
    fn load<H: SeplHost + ?Sized>(&self, h: &mut H, area: i32) {
        let (there, i, f) = call(h, EE_SCREENS_SEAR_OP_LOAD, self.mode_combo, [area, 0, 0, 0], [0.0; 4]);
        if there == 0 {
            return;
        }
        self.rectangle.set(f);
        self.point.set([i[1] as f32, i[2] as f32]);
        self.has_rectangle.set(i[3] & 1 != 0);
        self.has_point.set(i[3] & 2 != 0);
        let has = if self.has_rectangle.get() {
            Some(self.rectangle.get())
        } else if self.has_point.get() {
            Some(self.point_rect())
        } else {
            None
        };
        self.show_area(h, has, true);
    }

    /// `SetAreaMode` (0x57d5c5): rectangle or point mode; the area drawn in that mode highlighted, the label or
    /// the button shown unless the editor's UI is hidden.
    pub fn set_area_mode<H: SeplHost + ?Sized>(&self, h: &mut H, rectangle: bool) {
        self.rectangle_mode.set(rectangle);
        let has = if rectangle {
            self.has_rectangle.get().then(|| self.rectangle.get())
        } else {
            self.has_point.get().then(|| self.point_rect())
        };
        let labels = op(h, EE_SCREENS_SEAR_OP_UI_HIDDEN, 0) == 0;
        self.show_area(h, has, labels);
    }

    /// `GetCenter` (0x57d3aa): the painted rectangle's centre in rectangle mode, the anchor in point mode; `None`
    /// if the mode's area is not drawn.
    pub fn center<H: SeplHost + ?Sized>(&self, h: &mut H) -> Option<[f32; 2]> {
        if self.rectangle_mode.get() {
            if !self.has_rectangle.get() {
                return None;
            }
            let f = call(h, EE_SCREENS_SEAR_OP_CENTER, 0, [0; 4], self.rectangle.get()).2;
            Some([f[0], f[1]])
        } else {
            self.has_point.get().then(|| self.point.get())
        }
    }

    /// The tile under the point (`mouse`: the mouse's), else the ground point under it.
    fn tile<H: SeplHost + ?Sized>(&self, h: &mut H, mouse: bool, x: f32, y: f32) -> (bool, i32, i32) {
        let (picked, i, _) = call(h, EE_SCREENS_SEAR_OP_PICK, 0, [mouse as i32, 0, 0, 0], [x, y, 0.0, 0.0]);
        (picked != 0, i[1], i[2])
    }

    fn ground<H: SeplHost + ?Sized>(&self, h: &mut H, mouse: bool, x: f32, y: f32) -> (i32, i32) {
        let i = call(h, EE_SCREENS_SEAR_OP_GROUND, 0, [mouse as i32, 0, 0, 0], [x, y, 0.0, 0.0]).1;
        (i[1], i[2])
    }

    fn corner<H: SeplHost + ?Sized>(&self, h: &mut H, x: i32, y: i32) {
        call(h, EE_SCREENS_SEAR_OP_PAINT_CORNER, self.paint_action.get(), [x, y, 0, 0], [0.0; 4]);
    }

    /// `Update` (0x57d0eb): the painted rectangle's corner dragged to the tile under the mouse (or the ground point).
    pub fn update<H: SeplHost + ?Sized>(&self, h: &mut H) {
        if !self.rectangle_mode.get() || self.paint_action.get() == 0 {
            return;
        }
        if call(h, EE_SCREENS_SEAR_OP_IN_VIEW, 0, [1, 0, 0, 0], [0.0; 4]).0 == 0 {
            return;
        }
        let (picked, mut x, mut y) = self.tile(h, true, 0.0, 0.0);
        if !picked {
            (x, y) = self.ground(h, true, 0.0, 0.0);
        }
        self.corner(h, x, y);
    }

    /// `MouseMove` (0x57d6a9) at (`mx`, `my`): in rectangle mode the left button paints the rectangle (begun on a
    /// tile, its corner dragged while held, ended on release); in point mode its release on a tile sets the anchor.
    pub fn mouse_move<H: SeplHost + ?Sized>(&self, h: &mut H, mx: f32, my: f32) {
        if call(h, EE_SCREENS_SEAR_OP_IN_VIEW, 0, [0; 4], [mx, my, 0.0, 0.0]).0 == 0 {
            return;
        }
        let (picked, mut x, mut y) = self.tile(h, false, mx, my);
        let button = op(h, EE_SCREENS_SEAR_OP_LBUTTON, 0);
        if self.rectangle_mode.get() {
            if self.paint_action.get() != 0 {
                if button & 1 != 0 {
                    if !picked {
                        (x, y) = self.ground(h, false, mx, my);
                    }
                    self.corner(h, x, y);
                } else if button & 4 != 0 {
                    self.has_rectangle.set(true);
                    let r = call(h, EE_SCREENS_SEAR_OP_PAINT_END, self.paint_action.get(), [0; 4], [0.0; 4]).2;
                    self.rectangle.set(r);
                    self.paint_action.set(0);
                }
                return;
            }
            if button & 2 == 0 || !picked {
                return;
            }
            let mut out = 0usize;
            let (mut i, mut f) = ([x, y, 0, 0], [0.0f32; 4]);
            h.op(EE_SCREENS_SEAR_OP_PAINT_BEGIN, [&mut out as *mut usize as usize, 0, 0], &mut i, &mut f);
            self.paint_action.set(out);
        } else {
            if button & 4 == 0 || !picked {
                return;
            }
            self.has_point.set(true);
            self.point.set([x as f32, y as f32]);
            self.paint_area(h, self.point_rect());
        }
        self.highlight(h, true);
        if op(h, EE_SCREENS_SEAR_OP_UI_HIDDEN, 0) == 0 {
            display(h, self.label, false);
            display(h, self.center_button, true);
        }
    }

    /// The state the editor gate reads: 1 rectangle mode, 2 a rectangle, 4 a point, 8 a rectangle being painted.
    pub fn flags(&self) -> u32 {
        self.rectangle_mode.get() as u32
            | (self.has_rectangle.get() as u32) << 1
            | (self.has_point.get() as u32) << 2
            | ((self.paint_action.get() != 0) as u32) << 3
    }

    /// `GetNames`: the user areas' names into the vector `names`.
    pub fn names<H: SeplHost + ?Sized>(&self, h: &mut H, names: usize) {
        call(h, EE_SCREENS_SEAR_OP_NAMES, names, [0; 4], [0.0; 4]);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// A host recording the ops: handles 1, 2, ... for the controls; `hidden` the editor's UI; a loaded area.
    #[derive(Default)]
    struct Rec {
        ops: Vec<(u32, usize, [i32; 4], [f32; 4])>,
        next: usize,
        hidden: bool,
        button: i32,
        picked: bool,
    }

    impl SeplHost for Rec {
        fn op(&mut self, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> i32 {
            self.ops.push((op, p[0], *i, *f));
            match op {
                crate::screens::editor_players::EE_SCREENS_SEPL_OP_MAKE | EE_SCREENS_SEAR_OP_PAINT_BEGIN => {
                    self.next += 1;
                    // SAFETY: the makers pass their out handle in p0
                    unsafe { *(p[0] as *mut usize) = self.next };
                    0
                }
                EE_SCREENS_SEAR_OP_UI_HIDDEN => self.hidden as i32,
                EE_SCREENS_SEAR_OP_IN_VIEW => 1,
                EE_SCREENS_SEAR_OP_LBUTTON => self.button,
                EE_SCREENS_SEAR_OP_PICK => {
                    i[1] = 7;
                    i[2] = 9;
                    self.picked as i32
                }
                EE_SCREENS_SEAR_OP_GROUND => {
                    i[1] = 70;
                    i[2] = 90;
                    0
                }
                EE_SCREENS_SEAR_OP_PAINT_END => {
                    *f = [1.0, 2.0, 3.0, 4.0];
                    0
                }
                EE_SCREENS_SEAR_OP_LOAD => {
                    *f = [10.0, 12.0, 20.0, 30.0];
                    i[1] = 5;
                    i[2] = 6;
                    i[3] = 2;
                    1
                }
                EE_SCREENS_SEAR_OP_CENTER => {
                    *f = [(f[0] + f[2]) / 2.0, (f[1] + f[3]) / 2.0, 0.0, 0.0];
                    0
                }
                _ => 0,
            }
        }
    }

    fn screen() -> (Rec, AreaScreen) {
        let mut h = Rec::default();
        let mut index = 264;
        let s = AreaScreen::new(&mut h, 3, &mut index);
        assert_eq!(index, 267);
        h.ops.clear();
        (h, s)
    }

    #[test]
    fn new_is_in_rectangle_mode_with_nothing_drawn() {
        let (mut h, s) = screen();
        assert_eq!((s.label, s.mode_combo, s.center_button), (1, 2, 3));
        assert_eq!(s.flags(), 1);
        assert_eq!(s.center(&mut h), None);
    }

    #[test]
    fn load_of_an_anchor_shows_the_button_and_highlights_the_point() {
        let (mut h, s) = screen();
        s.call(&mut h, EE_SCREENS_SEAR_LOAD, 1, 0);
        // the area's index is the selection + 2
        assert_eq!(h.ops[0].2[0], 3);
        assert_eq!(s.flags(), 1 | 4);
        assert!(h.ops.iter().any(|o| o.0 == EE_SCREENS_SEAR_OP_PAINT_AREA && o.3 == [5.0, 6.0, 5.0, 6.0]));
        // rectangle mode with no rectangle: no centre; point mode: the anchor
        assert_eq!(s.center(&mut h), None);
        s.call(&mut h, EE_SCREENS_SEAR_SET_MODE, 1, 0);
        assert_eq!(s.center(&mut h), Some([5.0, 6.0]));
    }

    #[test]
    fn set_mode_with_the_ui_hidden_leaves_the_label_and_button() {
        let (mut h, s) = screen();
        h.hidden = true;
        s.set_area_mode(&mut h, false);
        assert!(!h.ops.iter().any(|o| o.0 == crate::screens::editor_players::EE_SCREENS_SEPL_OP_SET_DISPLAY));
        assert!(h.ops.iter().any(|o| o.0 == EE_SCREENS_SEAR_OP_HIGHLIGHT && o.2[0] == 0));
    }

    #[test]
    fn painting_a_rectangle_begins_drags_and_ends() {
        let (mut h, s) = screen();
        h.picked = true;
        h.button = 2;
        s.mouse_move(&mut h, 100.0, 100.0);
        assert_eq!(s.paint_action.get(), 4);
        assert_eq!(s.flags(), 1 | 8);
        h.button = 1;
        h.picked = false;
        s.mouse_move(&mut h, 120.0, 110.0);
        assert!(h.ops.iter().any(|o| o.0 == EE_SCREENS_SEAR_OP_PAINT_CORNER && o.1 == 4 && o.2[..2] == [70, 90]));
        h.button = 4;
        s.mouse_move(&mut h, 120.0, 110.0);
        assert_eq!(s.flags(), 1 | 2);
        assert_eq!(s.rectangle.get(), [1.0, 2.0, 3.0, 4.0]);
        assert_eq!(s.center(&mut h), Some([2.0, 3.0]));
        // store: the rectangle set on the selected area, no anchor
        h.ops.clear();
        s.call(&mut h, EE_SCREENS_SEAR_STORE, 0, 0);
        let ops: Vec<u32> = h.ops.iter().map(|o| o.0).collect();
        assert_eq!(ops, vec![EE_SCREENS_SEAR_OP_STORE, EE_SCREENS_SEAR_OP_SET_RECT]);
    }

    #[test]
    fn point_mode_sets_the_anchor_on_release_over_a_tile() {
        let (mut h, s) = screen();
        s.rectangle_mode.set(false);
        h.button = 4;
        s.mouse_move(&mut h, 1.0, 1.0);
        assert_eq!(s.flags(), 0);
        h.picked = true;
        s.mouse_move(&mut h, 1.0, 1.0);
        assert_eq!(s.flags(), 4);
        assert_eq!(s.point.get(), [7.0, 9.0]);
    }
}
