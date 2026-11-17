//! The scenario editor's trigger tab whole in Rust (`EESETriggerScreen`, once `EESETriggerScreen.cpp`): the
//! controls it makes (records for the shared maker, `EeScreensSeplCtl`, and its own kinds through its host), the
//! entry list of the open sub-screen (trigger, condition, effect, object, area) and its buttons (new, duplicate,
//! delete, sort, priority up and down, the name's Enter), the sub-tabs, the store of the open entry and the load of
//! the selected one, entering and leaving the tab. The five sub-screens stay C++ objects of the shell
//! (`mScreens`, their own vtables); the tab drives them, the controls and the world through the shared control ops
//! (`ee_screenscpp_sepl`, below 100) and its own (`ee_screenscpp_sets`, from 400).
//!
//! A host call can come back into the tab (a sub-screen's store or load refreshes it, the editor's events call its
//! members): it is only used through `&self`, its current sub-screen and control group are `Cell`s.

use core::cell::Cell;

use crate::screens::editor_object::*;
use crate::screens::editor_players::*;
use crate::screens::scenario_editor::*;

// ---- the trigger tab's host ops (`ee_screenscpp_sets`)

/// A sub-tab radio button: p0 -> its handle, made on the form p1 at the control index i0 for the sub-screen i1.
pub const EE_SCREENS_SETS_OP_TAB: u32 = 400;
/// A bitmap button (i1: 0 the alphabetical sort, 1 priority up, 2 priority down) at y f0: p0 -> its handle, made on
/// the form p1 at the index i0, its button-up event the screen event i2.
pub const EE_SCREENS_SETS_OP_IMAGE_BUTTON: u32 = 401;
/// The debug ids check box at y f0: p0 -> its handle, made on the form p1 at the index i0.
pub const EE_SCREENS_SETS_OP_SHOW_IDS: u32 = 402;
/// -> the height of the font i0 (`EE_SCREENS_SEPL_FONT_*`).
pub const EE_SCREENS_SETS_OP_FONT_HEIGHT: u32 = 403;
/// The shell p0's sub-screen i0: its member i1 (`EE_SCREENS_SETS_SCREEN_*`; `MouseMove` at f0, f1).
pub const EE_SCREENS_SETS_OP_SCREEN: u32 = 404;
/// The shell p0's sub-screen i0's selection -> it; i1 -> whether it has one.
pub const EE_SCREENS_SETS_OP_SELECTION: u32 = 405;
/// The shell p0's sub-screen i0's selection set to i1 (unless negative), whether it has one to i2 (unless negative).
pub const EE_SCREENS_SETS_OP_SET_SELECTION: u32 = 406;
/// The list p0 filled with the names of the shell p1's sub-screen i0 (`GetNames`, `AppendStringArray`).
pub const EE_SCREENS_SETS_OP_NAMES: u32 = 407;
/// Leaving (i0 = 0) or entering (1) the tab: the hide-UI box, the game view, the camera flags, the mini map's view,
/// the manager redrawn and the current form's controls dirty (entering: the hide-UI event's views set, the box
/// checked, the tab's group 14 enabled by id).
pub const EE_SCREENS_SETS_OP_VIEW: u32 = 408;
/// The shell p0's panel background drawn where it overlaps the rectangle p1, unless the editor's UI is hidden.
pub const EE_SCREENS_SETS_OP_DRAW: u32 = 409;
/// -> the scenario's show debug ids flag.
pub const EE_SCREENS_SETS_OP_DEBUG_IDS: u32 = 410;
/// The list p0's selection -> it, -1 for none / its line count / the line i0 selected (i1: notify, i2: scroll).
pub const EE_SCREENS_SETS_OP_LIST_SELECTION: u32 = 411;
pub const EE_SCREENS_SETS_OP_LIST_COUNT: u32 = 412;
pub const EE_SCREENS_SETS_OP_LIST_SELECT: u32 = 413;
/// The edit box p0's caret put after its text.
pub const EE_SCREENS_SETS_OP_CARET_END: u32 = 414;
/// The UI manager redrawn.
pub const EE_SCREENS_SETS_OP_REDRAW: u32 = 415;
/// The writing-back flag (`gB4C3220`) set to i0.
pub const EE_SCREENS_SETS_OP_STORING: u32 = 416;
/// The form's control group i0 enabled (i1 = 1) or disabled (the form of the control p0).
pub const EE_SCREENS_SETS_OP_GROUP: u32 = 417;

/// The sub-screens' members (`EESETSBaseObject`'s slots).
pub const EE_SCREENS_SETS_SCREEN_SORT: i32 = 0;
pub const EE_SCREENS_SETS_SCREEN_MOVE_DOWN: i32 = 1;
pub const EE_SCREENS_SETS_SCREEN_DELETE: i32 = 2;
pub const EE_SCREENS_SETS_SCREEN_DEACTIVATE: i32 = 3;
pub const EE_SCREENS_SETS_SCREEN_DUPLICATE: i32 = 4;
pub const EE_SCREENS_SETS_SCREEN_ACTIVATE: i32 = 5;
pub const EE_SCREENS_SETS_SCREEN_STORE: i32 = 6;
pub const EE_SCREENS_SETS_SCREEN_HIDE: i32 = 7;
pub const EE_SCREENS_SETS_SCREEN_LOAD: i32 = 8;
pub const EE_SCREENS_SETS_SCREEN_MOVE_UP: i32 = 9;
pub const EE_SCREENS_SETS_SCREEN_NEW: i32 = 10;
pub const EE_SCREENS_SETS_SCREEN_SHOW: i32 = 11;
pub const EE_SCREENS_SETS_SCREEN_UPDATE: i32 = 12;
pub const EE_SCREENS_SETS_SCREEN_UPDATE_KEYS: i32 = 13;
pub const EE_SCREENS_SETS_SCREEN_MOUSE_MOVE: i32 = 14;
pub const EE_SCREENS_SETS_SCREEN_REFRESH: i32 = 15;

/// The sub-tabs' control ids (`kSETriggerTabTrigger`..), the sub-screens' first control group (9: the trigger).
pub const EE_SCREENS_SETS_TAB_FIRST: u32 = 0x7d0;
const GROUP_FIRST: i32 = 9;
/// Each sub-screen's control group (`kEntityGroup*`: the trigger 9, the condition 12, the effect 13, the object 10,
/// the area 11).
pub const EE_SCREENS_SETS_GROUPS: [i32; 5] = [9, 12, 13, 10, 11];
/// The tab's own control group.
const GROUP: u32 = 14;

// ---- the language's strings (`ee/EEStringIDs.h`; the host `static_assert`s them)

/// New, Duplicate, Delete, Name (30304..30307); Description.
pub const EE_SCREENS_SETS_IDS_NEW: u32 = 30304;
pub const EE_SCREENS_SETS_IDS_DESCRIPTION: u32 = 30376;

/// The shared maker's flag: the control hidden before it is added to its groups.
pub const EE_SCREENS_SEPL_HIDE: u32 = 16;
/// The shared maker's unit list kind (`EE_SCREENS_SEPL_UNIT_LIST`'s extra 0): a plain `UILBString`.
pub const EE_SCREENS_SEPL_LIST_PLAIN: i32 = 3;

/// The trigger tab (`EESETriggerScreen`).
#[derive(Debug, Default)]
pub struct TriggerTab {
    /// the C++ shell (its sub-screens and panel rectangle)
    pub shell: usize,
    pub list: usize,
    pub tabs: [usize; 5],
    pub new_button: usize,
    pub duplicate_button: usize,
    pub delete_button: usize,
    pub alpha_sort: usize,
    pub up_button: usize,
    pub down_button: usize,
    pub show_ids: usize,
    pub name_label: usize,
    pub name_edit: usize,
    pub text_label: usize,
    pub text_edit: usize,
    /// the open sub-screen (0 trigger .. 4 area) and its control group (9 .. 13)
    pub screen: Cell<i32>,
    pub group: Cell<i32>,
}

fn op<H: SeplHost + ?Sized>(h: &mut H, op: u32, p: [usize; 3], ints: [i32; 4], floats: [f32; 4]) -> i32 {
    let (mut i, mut f) = (ints, floats);
    h.op(op, p, &mut i, &mut f)
}

fn made<H: SeplHost + ?Sized>(h: &mut H, which: u32, form: usize, ints: [i32; 4], floats: [f32; 4]) -> usize {
    let mut out = 0usize;
    op(h, which, [&mut out as *mut usize as usize, form, 0], ints, floats);
    out
}

/// A yellow button of the tab (90 x 20, border 2) with its screen event on the release.
fn text_button(id: u32, string: u32, x: f32) -> EeScreensSeplCtl {
    EeScreensSeplCtl::new(EE_SCREENS_SEPL_BUTTON, id, x, 586.0, 90.0, 20.0)
        .string(string)
        .flags(EE_SCREENS_SEPL_FG)
        .bg(YELLOW_BG)
        .group(GROUP)
        .extra([2, 0, 0, 0])
}

/// A yellow label of the tab at the screen font's height.
fn label(id: u32, string: u32, x: f32, y: f32, w: f32) -> EeScreensSeplCtl {
    EeScreensSeplCtl::new(EE_SCREENS_SEPL_LABEL, id, x, y, w, 0.0)
        .string(string)
        .flags(EE_SCREENS_SEPL_FG | EE_SCREENS_SEPL_FONT_HEIGHT)
        .bg(YELLOW_BG)
        .group(GROUP)
}

impl TriggerTab {
    /// `EESETriggerScreen`'s constructor, up to its sub-screens: the entry list, the five sub-tabs, the new,
    /// duplicate and delete buttons, the sort and priority buttons, the debug ids box, the name and description;
    /// `index` is the form's next control index (advanced). The shell makes the sub-screens next, then calls
    /// [`TriggerTab::finish`].
    pub fn new<H: SeplHost + ?Sized>(h: &mut H, shell: usize, form: usize, index: &mut u32) -> TriggerTab {
        let mut t = TriggerTab { shell, ..TriggerTab::default() };
        let mut m = Maker { h, form, index: *index };
        t.list = m.make(
            EeScreensSeplCtl::new(EE_SCREENS_SEPL_UNIT_LIST, 0x7da, 4.0, 480.0, 300.0, 105.0)
                .font(EE_SCREENS_SEPL_FONT_530)
                .flags(EE_SCREENS_SEPL_FG)
                .bg(YELLOW_BG)
                .bg2(YELLOW_BG)
                .group(GROUP)
                .extra([EE_SCREENS_SEPL_LIST_PLAIN, 0, 0, 0]),
        );
        screen_event(m.h, t.list, EE_SCREENS_SE_SELECT_ENTRY, EE_SCREENS_SEPL_ON_SELECTION);
        for k in 0..5 {
            t.tabs[k] = made(m.h, EE_SCREENS_SETS_OP_TAB, form, [m.index as i32, k as i32, 0, 0], [0.0; 4]);
            m.index += 1;
        }
        let ids = EE_SCREENS_SETS_IDS_NEW;
        let buttons = [
            (0x7d5, ids, 4.0, EE_SCREENS_SE_NEW_ENTRY),
            (0x7d6, ids + 1, 104.0, EE_SCREENS_SE_DUPLICATE_ENTRY),
            (0x7d7, ids + 2, 204.0, EE_SCREENS_SE_DELETE_ENTRY),
        ];
        let mut made_buttons = [0usize; 3];
        for (k, (id, string, x, code)) in buttons.into_iter().enumerate() {
            made_buttons[k] = m.make(text_button(id, string, x));
            screen_event(m.h, made_buttons[k], code, EE_SCREENS_SEPL_ON_BUTTON_UP);
        }
        [t.new_button, t.duplicate_button, t.delete_button] = made_buttons;
        let mut y = 480.0f32;
        let mut images = [0usize; 3];
        let codes = [EE_SCREENS_SE_ALPHA_SORT, EE_SCREENS_SE_INC_PRIORITY, EE_SCREENS_SE_DEC_PRIORITY];
        for (k, code) in codes.into_iter().enumerate() {
            let ints = [m.index as i32, k as i32, code as i32, 0];
            images[k] = made(m.h, EE_SCREENS_SETS_OP_IMAGE_BUTTON, form, ints, [y, 0.0, 0.0, 0.0]);
            m.index += 1;
            y = rect(m.h, images[k])[3] + 1.0;
        }
        [t.alpha_sort, t.up_button, t.down_button] = images;
        t.show_ids = made(m.h, EE_SCREENS_SETS_OP_SHOW_IDS, form, [m.index as i32, 0, 0, 0], [y, 0.0, 0.0, 0.0]);
        m.index += 1;
        t.name_label = m.make(label(0x7dc, ids + 3, 329.0, 458.0, 250.0));
        let h530 =
            op(m.h, EE_SCREENS_SETS_OP_FONT_HEIGHT, [0; 3], [EE_SCREENS_SEPL_FONT_530 as i32, 0, 0, 0], [0.0; 4]);
        t.name_edit = m.make(
            EeScreensSeplCtl::new(EE_SCREENS_SEPL_EDIT, 0x7db, 329.0, 480.0, 250.0, h530 as f32 + 6.0)
                .font(EE_SCREENS_SEPL_FONT_530)
                .flags(EE_SCREENS_SEPL_FG)
                .bg(YELLOW_BG)
                .group(GROUP)
                .extra([0x1c, 0, 0, 0]),
        );
        screen_event(m.h, t.name_edit, EE_SCREENS_SE_SELECT_NAME, EE_SCREENS_SEPL_ON_ENTER);
        t.text_label =
            m.make(label(0x962, EE_SCREENS_SETS_IDS_DESCRIPTION, 589.0, 458.0, 220.0).flags(EE_SCREENS_SEPL_HIDE));
        let y = rect(m.h, t.text_label)[3] + 5.0;
        t.text_edit = m.make(
            EeScreensSeplCtl::new(EE_SCREENS_SEPL_EDIT, 0x963, 589.0, y, 220.0, 98.0)
                .font(EE_SCREENS_SEPL_FONT_530)
                .flags(EE_SCREENS_SEPL_FG | EE_SCREENS_SEPL_HIDE)
                .bg(YELLOW_BG)
                .group(GROUP)
                .extra([6, 0, 1, 0]),
        );
        *index = m.index;
        t
    }

    /// The constructor's end, after the sub-screens: their groups disabled, the trigger sub-screen open.
    pub fn finish<H: SeplHost + ?Sized>(&self, h: &mut H) {
        self.hide_groups(h);
        self.screen.set(0);
        self.group.set(GROUP_FIRST);
    }

    /// The destructor (the shell deletes the sub-screens between the tabs and the name box: `delete_screens`).
    pub fn destroy_first<H: SeplHost + ?Sized>(&self, h: &mut H) {
        delete(h, self.list);
        for c in self.tabs {
            delete(h, c);
        }
    }

    /// The destructor's rest, after the sub-screens.
    pub fn destroy_rest<H: SeplHost + ?Sized>(&self, h: &mut H) {
        for c in [
            self.name_edit,
            self.name_label,
            self.text_label,
            self.text_edit,
            self.new_button,
            self.duplicate_button,
            self.delete_button,
            self.up_button,
            self.down_button,
            self.alpha_sort,
            self.show_ids,
        ] {
            delete(h, c);
        }
    }

    fn screen_call<H: SeplHost + ?Sized>(&self, h: &mut H, what: i32) {
        op(h, EE_SCREENS_SETS_OP_SCREEN, [self.shell, 0, 0], [self.screen.get(), what, 0, 0], [0.0; 4]);
    }

    /// The open sub-screen's selection and whether it has one.
    fn selection_of<H: SeplHost + ?Sized>(&self, h: &mut H) -> (u32, bool) {
        let (mut i, mut f) = ([self.screen.get(), 0, 0, 0], [0.0f32; 4]);
        let s = h.op(EE_SCREENS_SETS_OP_SELECTION, [self.shell, 0, 0], &mut i, &mut f);
        (s as u32, i[1] != 0)
    }

    fn set_selection<H: SeplHost + ?Sized>(&self, h: &mut H, selection: Option<u32>, has: Option<bool>) {
        let s = selection.map_or(-1, |s| s as i32);
        let b = has.map_or(-1, |b| b as i32);
        op(h, EE_SCREENS_SETS_OP_SET_SELECTION, [self.shell, 0, 0], [self.screen.get(), s, b, 0], [0.0; 4]);
    }

    fn list_selection<H: SeplHost + ?Sized>(&self, h: &mut H) -> Option<u32> {
        let s = op(h, EE_SCREENS_SETS_OP_LIST_SELECTION, [self.list, 0, 0], [0; 4], [0.0; 4]);
        (s >= 0).then_some(s as u32)
    }

    fn line_count<H: SeplHost + ?Sized>(&self, h: &mut H) -> u32 {
        op(h, EE_SCREENS_SETS_OP_LIST_COUNT, [self.list, 0, 0], [0; 4], [0.0; 4]) as u32
    }

    fn select_line<H: SeplHost + ?Sized>(&self, h: &mut H, line: u32, scroll: bool) {
        op(h, EE_SCREENS_SETS_OP_LIST_SELECT, [self.list, 0, 0], [line as i32, 0, scroll as i32, 0], [0.0; 4]);
    }

    fn group_on<H: SeplHost + ?Sized>(&self, h: &mut H, control: usize, group: i32, enable: bool) {
        op(h, EE_SCREENS_SETS_OP_GROUP, [control, 0, 0], [group, enable as i32, 0, 0], [0.0; 4]);
    }

    /// `AlphaSort`: with a selection, the entries sorted and the first one selected.
    pub fn alpha_sort<H: SeplHost + ?Sized>(&self, h: &mut H) {
        if self.list_selection(h).is_none() {
            return;
        }
        self.store_entry(h);
        self.screen_call(h, EE_SCREENS_SETS_SCREEN_SORT);
        self.set_selection(h, Some(0), None);
        self.fill_list(h);
        self.select_line(h, 0, true);
        self.show_selection(h);
    }

    /// `SelectEntry`: the open entry stored and the list's selected one loaded (the last one without a selection).
    pub fn select_entry<H: SeplHost + ?Sized>(&self, h: &mut H) {
        let line = match self.list_selection(h) {
            Some(s) => s,
            None => {
                let count = self.line_count(h);
                if count == 0 {
                    self.set_selection(h, Some(0), Some(false));
                    return;
                }
                count - 1
            }
        };
        self.store_entry(h);
        self.set_selection(h, Some(line), None);
        self.fill_list(h);
        self.select_line(h, line, false);
        self.show_selection(h);
    }

    /// `Leave`: the hide-UI box hidden, the full game view, the sub-screens' groups disabled, the open entry stored,
    /// the sub-screen deactivated.
    pub fn leave<H: SeplHost + ?Sized>(&self, h: &mut H) {
        op(h, EE_SCREENS_SETS_OP_VIEW, [self.new_button, 0, 0], [0; 4], [0.0; 4]);
        self.hide_groups(h);
        self.store_entry(h);
        self.screen_call(h, EE_SCREENS_SETS_SCREEN_DEACTIVATE);
    }

    /// `Update`: the open sub-screen's.
    pub fn update<H: SeplHost + ?Sized>(&self, h: &mut H) {
        self.screen_call(h, EE_SCREENS_SETS_SCREEN_UPDATE);
    }

    /// `DecPriority`: the selected entry one place down, still selected.
    pub fn dec_priority<H: SeplHost + ?Sized>(&self, h: &mut H) {
        let Some(s) = self.list_selection(h) else { return };
        if s >= self.line_count(h).wrapping_sub(1) {
            return;
        }
        self.store_entry(h);
        self.screen_call(h, EE_SCREENS_SETS_SCREEN_MOVE_DOWN);
        self.set_selection(h, Some(s + 1), None);
        self.fill_list(h);
        self.select_line(h, s + 1, true);
        self.show_selection(h);
    }

    /// `DeleteEntry`: the selected entry deleted, the entry now in its place selected (or the last one).
    pub fn delete_entry<H: SeplHost + ?Sized>(&self, h: &mut H) {
        let Some(s) = self.list_selection(h) else { return };
        self.screen_call(h, EE_SCREENS_SETS_SCREEN_DELETE);
        self.fill_list(h);
        self.set_selection(h, None, Some(false));
        let count = self.line_count(h);
        let line = if s < count { s } else { count.wrapping_sub(1) };
        self.select_line(h, line, true);
        self.show_selection(h);
    }

    /// `Draw`: the panel's background behind the tab (the rectangle `area`).
    pub fn draw<H: SeplHost + ?Sized>(&self, h: &mut H, area: usize) {
        op(h, EE_SCREENS_SETS_OP_DRAW, [self.shell, area, 0], [0; 4], [0.0; 4]);
    }

    /// `DuplicateEntry`: the selected entry copied, the copy at the end selected.
    pub fn duplicate_entry<H: SeplHost + ?Sized>(&self, h: &mut H) {
        if self.list_selection(h).is_none() {
            return;
        }
        self.store_entry(h);
        self.screen_call(h, EE_SCREENS_SETS_SCREEN_DUPLICATE);
        self.fill_list(h);
        let line = self.line_count(h).wrapping_sub(1);
        self.select_line(h, line, true);
        self.show_selection(h);
    }

    /// `ShowTab`: the sub-screen of the tab button `id` opened: the open entry stored, the list refilled, the
    /// sub-screen's entry selected again (the last one when out of range), the sub-screen activated.
    pub fn show_tab<H: SeplHost + ?Sized>(&self, h: &mut H, id: u32) {
        self.store_entry(h);
        self.group_on(h, self.new_button, self.group.get(), false);
        self.screen_call(h, EE_SCREENS_SETS_SCREEN_DEACTIVATE);
        self.hide_text(h);
        let k = id.wrapping_sub(EE_SCREENS_SETS_TAB_FIRST);
        if k < 5 {
            self.group.set(EE_SCREENS_SETS_GROUPS[k as usize]);
            self.screen.set(k as i32);
        }
        self.fill_list(h);
        let (s, has) = self.selection_of(h);
        let count = self.line_count(h);
        if has && count > s {
            self.select_line(h, s, true);
        } else if count > 0 {
            self.set_selection(h, Some(count - 1), None);
            self.select_line(h, count - 1, true);
        } else {
            self.set_selection(h, None, Some(false));
            self.set_selection(h, Some(0), None);
        }
        self.screen_call(h, EE_SCREENS_SETS_SCREEN_ACTIVATE);
        self.show_selection(h);
        op(h, EE_SCREENS_SETS_OP_REDRAW, [0; 3], [0; 4], [0.0; 4]);
    }

    /// `StoreEntry`: the controls written back into the open entry when there is one (the writing-back flag set).
    pub fn store_entry<H: SeplHost + ?Sized>(&self, h: &mut H) {
        op(h, EE_SCREENS_SETS_OP_STORING, [0; 3], [1, 0, 0, 0], [0.0; 4]);
        if self.selection_of(h).1 {
            self.screen_call(h, EE_SCREENS_SETS_SCREEN_STORE);
        }
    }

    /// `Disable`: the tab's group disabled, the sub-screen's controls hidden.
    pub fn disable<H: SeplHost + ?Sized>(&self, h: &mut H) {
        self.group_on(h, self.name_edit, GROUP as i32, false);
        self.screen_call(h, EE_SCREENS_SETS_SCREEN_HIDE);
    }

    /// `HideText` / `ShowText`: the description's box and label.
    pub fn hide_text<H: SeplHost + ?Sized>(&self, h: &mut H) {
        display(h, self.text_edit, false);
        display(h, self.text_label, false);
    }

    pub fn show_text<H: SeplHost + ?Sized>(&self, h: &mut H) {
        display(h, self.text_edit, true);
        display(h, self.text_label, true);
    }

    /// `HideAlphaSort` / `ShowAlphaSort`.
    pub fn show_alpha_sort<H: SeplHost + ?Sized>(&self, h: &mut H, show: bool) {
        display(h, self.alpha_sort, show);
    }

    /// `ShowSelection`: the list's selected entry loaded and the name, duplicate and delete controls shown; without
    /// a selection they and the description are hidden.
    pub fn show_selection<H: SeplHost + ?Sized>(&self, h: &mut H) {
        op(h, EE_SCREENS_SETS_OP_STORING, [0; 3], [0; 4], [0.0; 4]);
        let ids = op(h, EE_SCREENS_SETS_OP_DEBUG_IDS, [0; 3], [0; 4], [0.0; 4]) != 0;
        set_checked(h, self.show_ids, ids);
        let shown = self.list_selection(h);
        if let Some(s) = shown {
            self.set_selection(h, Some(s), Some(true));
        } else {
            self.set_selection(h, None, Some(false));
        }
        self.group_on(h, self.name_edit, self.group.get(), shown.is_some());
        for c in [self.name_edit, self.name_label, self.delete_button, self.duplicate_button] {
            display(h, c, shown.is_some());
        }
        if shown.is_some() {
            self.screen_call(h, EE_SCREENS_SETS_SCREEN_LOAD);
        } else {
            self.hide_text(h);
        }
    }

    /// `IncPriority`: the selected entry one place up, still selected.
    pub fn inc_priority<H: SeplHost + ?Sized>(&self, h: &mut H) {
        let Some(s) = self.list_selection(h) else { return };
        if s == 0 {
            return;
        }
        self.store_entry(h);
        self.screen_call(h, EE_SCREENS_SETS_SCREEN_MOVE_UP);
        self.set_selection(h, Some(s - 1), None);
        self.fill_list(h);
        self.select_line(h, s - 1, true);
        self.show_selection(h);
    }

    /// `HideGroups`: the five sub-screens' groups (9..13) disabled.
    pub fn hide_groups<H: SeplHost + ?Sized>(&self, h: &mut H) {
        for g in GROUP_FIRST..GROUP_FIRST + 5 {
            self.group_on(h, self.new_button, g, false);
        }
    }

    /// `NewEntry`: the open entry stored, a new one added and selected.
    pub fn new_entry<H: SeplHost + ?Sized>(&self, h: &mut H) {
        self.store_entry(h);
        self.screen_call(h, EE_SCREENS_SETS_SCREEN_NEW);
        self.fill_list(h);
        let line = self.line_count(h).wrapping_sub(1);
        self.select_line(h, line, true);
        self.show_selection(h);
    }

    /// `Enter`: the game view above the panel, the hide-UI box shown and checked, the list refilled, the
    /// sub-screen's entry selected again when still valid, the sub-screen activated.
    pub fn enter<H: SeplHost + ?Sized>(&self, h: &mut H) {
        op(h, EE_SCREENS_SETS_OP_VIEW, [self.new_button, 0, 0], [1, 0, 0, 0], [0.0; 4]);
        self.group_on(h, self.new_button, self.group.get(), true);
        self.fill_list(h);
        let (s, has) = self.selection_of(h);
        if has {
            if s < self.line_count(h) {
                self.select_line(h, s, true);
            } else {
                self.set_selection(h, None, Some(false));
            }
        }
        self.screen_call(h, EE_SCREENS_SETS_SCREEN_ACTIVATE);
    }

    /// `SelectName`: after Enter in the name box, the entry stored and shown, the list refilled, the caret at the
    /// end of the name.
    pub fn select_name<H: SeplHost + ?Sized>(&self, h: &mut H) {
        let Some(s) = self.list_selection(h) else { return };
        self.store_entry(h);
        self.show_selection(h);
        self.fill_list(h);
        self.select_line(h, s, false);
        op(h, EE_SCREENS_SETS_OP_CARET_END, [self.name_edit, 0, 0], [0; 4], [0.0; 4]);
    }

    /// `FillList`: the list refilled from the open sub-screen; its group enabled only with entries.
    pub fn fill_list<H: SeplHost + ?Sized>(&self, h: &mut H) {
        call(h, EE_SCREENS_SEPL_OP_LIST_CLEAR, [self.list, 0, 0], [0; 4]);
        self.set_selection(h, None, Some(false));
        op(h, EE_SCREENS_SETS_OP_NAMES, [self.list, self.shell, 0], [self.screen.get(), 0, 0, 0], [0.0; 4]);
        let any = self.line_count(h) > 0;
        self.group_on(h, self.list, self.group.get(), any);
        if any {
            self.set_selection(h, None, Some(true));
        }
    }

    /// `Enable`: the tab's group enabled, the sub-screen's controls shown.
    pub fn enable<H: SeplHost + ?Sized>(&self, h: &mut H) {
        self.group_on(h, self.name_edit, GROUP as i32, true);
        self.screen_call(h, EE_SCREENS_SETS_SCREEN_SHOW);
    }

    /// `UpdateKeys`, `MouseMove`, `RefreshScreen`: the open sub-screen's, when it has a selection.
    pub fn when_selected<H: SeplHost + ?Sized>(&self, h: &mut H, what: i32, x: f32, y: f32) {
        if self.selection_of(h).1 {
            op(h, EE_SCREENS_SETS_OP_SCREEN, [self.shell, 0, 0], [self.screen.get(), what, 0, 0], [x, y, 0.0, 0.0]);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::HashMap;

    /// A recording host: the list's lines and selection, the sub-screens' selections, the calls.
    #[derive(Default)]
    struct Rec {
        made: Vec<EeScreensSeplCtl>,
        next: usize,
        lines: u32,
        selected: Option<u32>,
        screens: [(u32, bool); 5],
        names: [u32; 5],
        mode: HashMap<usize, i32>,
        log: Vec<(u32, [i32; 4])>,
    }

    impl SeplHost for Rec {
        fn op(&mut self, op: u32, p: [usize; 3], i: &mut [i32; 4], _f: &mut [f32; 4]) -> i32 {
            let out = |v: usize| unsafe { *(p[0] as *mut usize) = v };
            match op {
                EE_SCREENS_SEPL_OP_MAKE => {
                    let c = unsafe { *(p[1] as *const EeScreensSeplCtl) };
                    self.made.push(c);
                    out(self.made.len());
                    return 0;
                }
                EE_SCREENS_SETS_OP_TAB | EE_SCREENS_SETS_OP_IMAGE_BUTTON | EE_SCREENS_SETS_OP_SHOW_IDS => {
                    self.next += 1;
                    out(1000 + self.next);
                }
                EE_SCREENS_SEPL_OP_SET_DISPLAY => {
                    self.mode.insert(p[0], i[0]);
                }
                EE_SCREENS_SETS_OP_FONT_HEIGHT => return 11,
                EE_SCREENS_SETS_OP_LIST_SELECTION => return self.selected.map_or(-1, |s| s as i32),
                EE_SCREENS_SETS_OP_LIST_COUNT => return self.lines as i32,
                EE_SCREENS_SETS_OP_LIST_SELECT => {
                    if (i[0] as u32) < self.lines {
                        self.selected = Some(i[0] as u32);
                    }
                }
                EE_SCREENS_SEPL_OP_LIST_CLEAR => {
                    self.lines = 0;
                    self.selected = None;
                }
                EE_SCREENS_SETS_OP_NAMES => self.lines = self.names[i[0] as usize],
                EE_SCREENS_SETS_OP_SELECTION => {
                    let (s, b) = self.screens[i[0] as usize];
                    i[1] = b as i32;
                    return s as i32;
                }
                EE_SCREENS_SETS_OP_SET_SELECTION => {
                    let e = &mut self.screens[i[0] as usize];
                    if i[1] >= 0 {
                        e.0 = i[1] as u32;
                    }
                    if i[2] >= 0 {
                        e.1 = i[2] != 0;
                    }
                }
                EE_SCREENS_SETS_OP_SCREEN => match i[1] {
                    EE_SCREENS_SETS_SCREEN_NEW | EE_SCREENS_SETS_SCREEN_DUPLICATE => self.names[i[0] as usize] += 1,
                    EE_SCREENS_SETS_SCREEN_DELETE => self.names[i[0] as usize] -= 1,
                    _ => {}
                },
                _ => {}
            }
            self.log.push((op, *i));
            0
        }
    }

    fn screen_calls(r: &Rec) -> Vec<i32> {
        r.log.iter().filter(|(o, _)| *o == EE_SCREENS_SETS_OP_SCREEN).map(|(_, i)| i[1]).collect()
    }

    fn tab(r: &mut Rec) -> TriggerTab {
        let mut index = 100;
        let t = TriggerTab::new(r, 7, 8, &mut index);
        t.finish(r);
        t
    }

    #[test]
    fn the_constructor_makes_the_controls_at_their_indexes() {
        let mut r = Rec::default();
        let mut index = 100;
        let t = TriggerTab::new(&mut r, 7, 8, &mut index);
        assert_eq!(index, 100 + 1 + 5 + 3 + 3 + 1 + 4);
        let ids: Vec<(u32, u32)> = r.made.iter().map(|c| (c.id, c.index)).collect();
        assert_eq!(
            ids,
            [
                (0x7da, 100),
                (0x7d5, 106),
                (0x7d6, 107),
                (0x7d7, 108),
                (0x7dc, 113),
                (0x7db, 114),
                (0x962, 115),
                (0x963, 116)
            ]
        );
        // the name box at the small font's height + 6, the description hidden at its making, multi-line
        assert_eq!(r.made[5].h, 17.0);
        assert!(r.made[6].flags & EE_SCREENS_SEPL_HIDE != 0 && r.made[7].extra[2] == 1);
        assert_eq!(r.made[7].y, 5.0);
        assert_ne!(t.show_ids, 0);
        t.finish(&mut r);
        assert_eq!((t.screen.get(), t.group.get()), (0, 9));
    }

    #[test]
    fn new_duplicate_and_delete_keep_the_selection_in_range() {
        let mut r = Rec::default();
        let t = tab(&mut r);
        t.new_entry(&mut r);
        t.new_entry(&mut r);
        assert_eq!((r.lines, r.selected), (2, Some(1)));
        assert_eq!(r.screens[0], (1, true));
        t.duplicate_entry(&mut r);
        assert_eq!(r.selected, Some(2));
        t.delete_entry(&mut r);
        assert_eq!((r.lines, r.selected), (2, Some(1)));
        r.selected = Some(0);
        t.delete_entry(&mut r);
        assert_eq!((r.lines, r.selected), (1, Some(0)));
        // the description hidden with no selection
        t.delete_entry(&mut r);
        assert_eq!(r.lines, 0);
        assert_eq!(r.mode[&t.text_edit], EE_SCREENS_SEPL_HIDDEN);
        assert_eq!(r.mode[&t.delete_button], EE_SCREENS_SEPL_HIDDEN);
    }

    #[test]
    fn priorities_move_within_the_list() {
        let mut r = Rec::default();
        let t = tab(&mut r);
        for _ in 0..3 {
            t.new_entry(&mut r);
        }
        r.log.clear();
        t.dec_priority(&mut r);
        assert!(screen_calls(&r).is_empty());
        r.selected = Some(0);
        t.inc_priority(&mut r);
        assert!(screen_calls(&r).is_empty());
        t.dec_priority(&mut r);
        assert_eq!(
            screen_calls(&r),
            [EE_SCREENS_SETS_SCREEN_STORE, EE_SCREENS_SETS_SCREEN_MOVE_DOWN, EE_SCREENS_SETS_SCREEN_LOAD]
        );
        assert_eq!(r.selected, Some(1));
        t.inc_priority(&mut r);
        assert_eq!(r.selected, Some(0));
    }

    #[test]
    fn show_tab_switches_screens_and_groups() {
        let mut r = Rec::default();
        let t = tab(&mut r);
        r.names[2] = 3;
        r.screens[2] = (7, true);
        r.log.clear();
        t.show_tab(&mut r, EE_SCREENS_SETS_TAB_FIRST + 2);
        assert_eq!((t.screen.get(), t.group.get()), (2, 13));
        // out of range: the last entry
        assert_eq!(r.selected, Some(2));
        assert_eq!(r.screens[2], (2, true));
        let groups: Vec<[i32; 4]> =
            r.log.iter().filter(|(o, _)| *o == EE_SCREENS_SETS_OP_GROUP).map(|(_, i)| *i).collect();
        assert_eq!(groups[0], [9, 0, 0, 0]);
        // an empty sub-screen
        t.show_tab(&mut r, EE_SCREENS_SETS_TAB_FIRST + 4);
        assert_eq!(r.screens[4], (0, false));
        assert_eq!(r.selected, None);
    }

    #[test]
    fn select_entry_without_selection_takes_the_last_or_clears() {
        let mut r = Rec::default();
        let t = tab(&mut r);
        t.select_entry(&mut r);
        assert_eq!(r.screens[0], (0, false));
        r.names[0] = 2;
        t.fill_list(&mut r);
        t.select_entry(&mut r);
        assert_eq!(r.selected, Some(1));
        assert_eq!(r.screens[0], (1, true));
    }
}
