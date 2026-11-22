//! The scenario editor's control kinds over the generic shells (`shells.rs`): the player tab's select buttons,
//! page tabs, unit-type radios and max-players combo, the editor's main tab buttons, the map tab's buttons, the
//! terrain tab's paint types and the trigger screen's show-ids box. Each kind is a deleted C++ class, step for step.

use core::cell::Cell;
use core::ffi::CStr;
use std::sync::Mutex;

use crate::screens::shells::*;

/// `EESETSShowIDsCheckBox` (a check box with no override).
pub const EE_SCREENS_SK_SHOW_IDS: u32 = 0x20;
/// `EESEBSelect` (a button; args: the font).
pub const EE_SCREENS_SK_SELECT: u32 = 0x21;
/// `EESETab` (a radio button; args: the font).
pub const EE_SCREENS_SK_TAB: u32 = 0x22;
/// `EESERBUnitTypes` (a radio button).
pub const EE_SCREENS_SK_UNIT_TYPES: u32 = 0x23;
/// `EESECBMaxPlayer` (a combo box).
pub const EE_SCREENS_SK_MAX_PLAYER: u32 = 0x24;
/// `UIBMap` (a button; args: the map tab).
pub const EE_SCREENS_SK_MAP_BUTTON: u32 = 0x25;
/// `UIRBMap` (a radio button).
pub const EE_SCREENS_SK_MAP_RADIO: u32 = 0x26;
/// `EEUIFSETRBPaintType` (a radio button).
pub const EE_SCREENS_SK_PAINT_TYPE: u32 = 0x27;
/// `EEUIFSEMainButton` (a radio button; args: the flag, the texture, the tab, the normal and pressed cells' x, y
/// as `f32` bits).
pub const EE_SCREENS_SK_MAIN_BUTTON: u32 = 0x28;

/// `EEUIFSEMainButton::CheckDirect()`.
pub const EE_SCREENS_SKOP_CHECK_DIRECT: u32 = 0x20;

/// A button's `SetText(UWideString(a: NUL-terminated bytes), (GEFont*)b)` (`UIButton::SetText`, a radio button's too).
pub const EE_SCREENS_SH_BUTTON_TEXT_BYTES: u32 = 0x20;
/// A radio button's four checked and unchecked bitmaps NULL.
pub const EE_SCREENS_SH_CLEAR_RADIO_BITMAPS: u32 = 0x21;
/// The form's string list box of id `a`: `GetLineCount()`.
pub const EE_SCREENS_SH_LIST_COUNT: u32 = 0x22;
/// The form's string list box of id `a`: `GetCurrentSelection` (-1 when it has none).
pub const EE_SCREENS_SH_LIST_SELECTION: u32 = 0x23;
/// The scenario editor's player combo box's current selection.
pub const EE_SCREENS_SH_EDITOR_PLAYER: u32 = 0x24;
/// `EESEARust(kSEATechTree, NULL, a & 0xffffffff, a >> 32, b & 0xffffffff, b >> 32 != 0)`, run and released.
pub const EE_SCREENS_SH_TECH_TREE_MOVE: u32 = 0x25;
/// The player tab's `SelectPage(a)`.
pub const EE_SCREENS_SH_PLAYER_PAGE: u32 = 0x26;
/// The player tab's `FillTechLists()`.
pub const EE_SCREENS_SH_FILL_TECH_LISTS: u32 = 0x27;
/// The player tab's `SetPlayerCount(a)`.
pub const EE_SCREENS_SH_SET_PLAYER_COUNT: u32 = 0x28;
/// A combo box's list's `GetCurrentSelection` (the value it leaves).
pub const EE_SCREENS_SH_LIST_OF_COMBO_SELECTION: u32 = 0x29;
/// The map tab `a`'s `CreateMap()`.
pub const EE_SCREENS_SH_MAP_CREATE: u32 = 0x2a;
/// The map tab's kind check of the shell (`ee_screens_semp_kind(form, id)`; answered in Rust).
pub const EE_SCREENS_SH_MAP_KIND: u32 = 0x2b;
/// The terrain tab's paint type check of the shell (`ee_screens_setn_check(form, id)`; answered in Rust).
pub const EE_SCREENS_SH_TERRAIN_CHECK: u32 = 0x2c;
/// A new `GEBitmap` of texture `a` on the screen (`b` 0: `gScreenIndex`, 1: the form's manager's) -> the bitmap.
pub const EE_SCREENS_SH_NEW_BITMAP: u32 = 0x2d;
/// The bitmap `a`'s texture coordinates (left, top, right, bottom in `f`).
pub const EE_SCREENS_SH_BITMAP_COORDS: u32 = 0x2e;
/// The bitmap `a`'s `SetXYScale(f[0], f[1])`.
pub const EE_SCREENS_SH_BITMAP_SCALE: u32 = 0x2f;
/// The bitmap `a`'s `Dereference()`.
pub const EE_SCREENS_SH_BITMAP_RELEASE: u32 = 0x30;
/// The texture `a`'s width and height (`f[0]`, `f[1]`, as floats).
pub const EE_SCREENS_SH_TEXTURE_SIZE: u32 = 0x31;
/// A button's `SetNormalBitmap(a)`.
pub const EE_SCREENS_SH_SET_NORMAL: u32 = 0x32;
/// A button's `SetPressedBitmap(a)`.
pub const EE_SCREENS_SH_SET_PRESSED: u32 = 0x33;
/// A button's `SetBitmapDrawOffset(f[0], f[1])`.
pub const EE_SCREENS_SH_BITMAP_OFFSET: u32 = 0x34;
/// The editor's tab selection: `a` 0 posted (`PostSelectEditorTab` with a UI thread, else `SelectTab`), 1 direct
/// (`SelectTab`); `b` the flag << 32 | the tab.
pub const EE_SCREENS_SH_SELECT_TAB: u32 = 0x35;

/// The ids (editor_ids.rs): the tech lists, the select buttons, the page tabs.
const TECH_LIST: i64 = 0x154;
const OTHER_TECH_LIST: i64 = 0x155;
const ADD: u32 = 0x156;
const REMOVE: u32 = 0x157;
const ADD_ALL: u32 = 0x158;
const REMOVE_ALL: u32 = 0x159;

/// The control's id (its record's `mId`; 0 in a unit test).
fn id_of(c: &Ctx) -> u32 {
    c.call(EE_SCREENS_SH_ID, 0, 0) as u32
}

pub(crate) fn make(kind: u32, args: &[i64]) -> Option<Box<dyn Kind>> {
    let a = |i: usize| args.get(i).copied().unwrap_or(0);
    Some(match kind {
        EE_SCREENS_SK_SHOW_IDS => Box::new(Named(c"EESETSShowIDsCheckBox")),
        EE_SCREENS_SK_SELECT => Box::new(Select { font: a(0) }),
        EE_SCREENS_SK_TAB => Box::new(Tab { font: a(0) }),
        EE_SCREENS_SK_UNIT_TYPES => Box::new(UnitTypes),
        EE_SCREENS_SK_MAX_PLAYER => Box::new(MaxPlayer),
        EE_SCREENS_SK_MAP_BUTTON => Box::new(MapButton { map: a(0) }),
        EE_SCREENS_SK_MAP_RADIO => Box::new(CheckThen { name: c"UIRBMap", op: EE_SCREENS_SH_MAP_KIND }),
        EE_SCREENS_SK_PAINT_TYPE => {
            Box::new(CheckThen { name: c"EEUIFSETRBPaintType", op: EE_SCREENS_SH_TERRAIN_CHECK })
        }
        EE_SCREENS_SK_MAIN_BUTTON => Box::new(MainButton {
            flag: a(0) as u8,
            texture: a(1),
            tab: a(2) as i32,
            cells: [a(3), a(4), a(5), a(6)].map(|b| f32::from_bits(b as u32)),
            normal: Cell::new(0),
            pressed: Cell::new(0),
        }),
        _ => return crate::screens::shells_tech::make(kind, args),
    })
}

/// A class with no override of its own (its name kept).
struct Named(&'static CStr);

impl Kind for Named {
    fn name(&self) -> &'static CStr {
        self.0
    }
}

/// `EESEBSelect`: the tech tree page's move buttons (`<<`, `>>`, `<`, `>`): a single move repeats while held (after
/// 500 ms, every 200 ms), a move-all moves on release.
struct Select {
    font: i64,
}

/// `EESEBSelect::sRepeatDelay` (500 ms), `sRepeatRate` (200 ms): the two shared polls.
static SELECT_POLLS: Mutex<Option<[ee_base::time::EeTimePoll; 2]>> = Mutex::new(None);

/// One step of the shared polls: `f` with the delay and the rate (made on first use, stopped).
fn with_polls<R>(f: impl FnOnce(&mut [ee_base::time::EeTimePoll; 2]) -> R) -> R {
    let mut g = SELECT_POLLS.lock().unwrap_or_else(|e| e.into_inner());
    let p = g.get_or_insert_with(|| {
        let mut p: [ee_base::time::EeTimePoll; 2] = Default::default();
        // SAFETY: two live polls
        unsafe {
            ee_base::time::ee_time_poll_init(&mut p[0], 500, 0, false);
            ee_base::time::ee_time_poll_init(&mut p[1], 200, 0, false);
        }
        p
    });
    f(p)
}

impl Select {
    /// `Select`: the lines to move and the direction, then the tech tree action.
    fn select(&self, c: &Ctx) {
        let (first, last, left) = match id_of(c) {
            REMOVE_ALL | ADD_ALL => {
                let all = id_of(c) == ADD_ALL;
                let n = c.call(EE_SCREENS_SH_LIST_COUNT, if all { TECH_LIST } else { OTHER_TECH_LIST }, 0) as u32;
                if n == 0 {
                    return;
                }
                (0, n - 1, all)
            }
            REMOVE | ADD => {
                let add = id_of(c) == ADD;
                let s = c.call(EE_SCREENS_SH_LIST_SELECTION, if add { TECH_LIST } else { OTHER_TECH_LIST }, 0);
                if s < 0 {
                    return;
                }
                (s as u32, s as u32, add)
            }
            _ => return,
        };
        let player = (c.call(EE_SCREENS_SH_EDITOR_PLAYER, 0, 0) as u32).wrapping_add(1);
        c.call(
            EE_SCREENS_SH_TECH_TREE_MOVE,
            i64::from(first) | (i64::from(last) << 32),
            i64::from(player) | (i64::from(left) << 32),
        );
    }
}

impl Kind for Select {
    fn name(&self) -> &'static CStr {
        c"EESEBSelect"
    }
    fn overrides(&self) -> u64 {
        1 << EE_SCREENS_SV_BUTTON_DOWN | 1 << EE_SCREENS_SV_BUTTON_UP
    }
    fn init(&self, c: &Ctx) {
        let text: &CStr = match id_of(c) {
            REMOVE_ALL => c"<<",
            ADD_ALL => c">>",
            REMOVE => c"<",
            ADD => c">",
            _ => c"",
        };
        c.call(EE_SCREENS_SH_BUTTON_TEXT_BYTES, text.as_ptr() as i64, self.font);
        c.call_f(EE_SCREENS_SH_TEXT_FG, 0, [1.0, 1.0, 1.0, 0.0]);
    }
    fn virt(&self, c: &Ctx, v: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        let id = id_of(c);
        match v {
            EE_SCREENS_SV_BUTTON_DOWN if (ADD..=REMOVE).contains(&id) => {
                use ee_base::time::*;
                // which step: 1 the delay ran out (select, start the rate, stop the delay), 2 the rate ran out
                // (select), 3 the first press (select, start the delay), 0 none
                // SAFETY (each poll call): the shared polls, live
                let step = with_polls(|p| unsafe {
                    if ee_time_poll_is_running(&p[0]) {
                        if ee_time_poll_poll(&mut p[0]) > 0 {
                            1
                        } else {
                            0
                        }
                    } else if ee_time_poll_is_running(&p[1]) {
                        if ee_time_poll_poll(&mut p[1]) > 0 {
                            2
                        } else {
                            0
                        }
                    } else {
                        3
                    }
                });
                if step != 0 {
                    self.select(c);
                }
                // SAFETY: the shared polls
                with_polls(|p| unsafe {
                    match step {
                        1 => {
                            ee_time_poll_start(&mut p[1]);
                            ee_time_poll_stop(&mut p[0]);
                        }
                        3 => ee_time_poll_start(&mut p[0]),
                        _ => {}
                    }
                });
            }
            EE_SCREENS_SV_BUTTON_UP => {
                if (ADD_ALL..=REMOVE_ALL).contains(&id) {
                    self.select(c);
                }
                // SAFETY: the shared polls
                with_polls(|p| unsafe { ee_base::time::ee_time_poll_stop(&mut p[1]) });
            }
            _ => {}
        }
        c.base(v, a, b, f)
    }
}

/// `EESETab`: the player tab's page tabs (a 150x25 push radio button with no bitmaps); checking one selects its page.
struct Tab {
    font: i64,
}

/// The tabs by id: their string (`IDS_*`) and page (`kPlayerScreen*`).
const TABS: [(u32, i64, i64); 5] =
    [(0x170, 20_565, 2), (0x15d, 20_520, 1), (0x15a, 20_546, 4), (0x12d, 20_521, 3), (0x12c, 20_519, 0)];

impl Kind for Tab {
    fn name(&self) -> &'static CStr {
        c"EESETab"
    }
    fn overrides(&self) -> u64 {
        1 << EE_SCREENS_SV_CHECK
    }
    fn init(&self, c: &Ctx) {
        let id = id_of(c);
        c.call(EE_SCREENS_SH_CLEAR_RADIO_BITMAPS, 0, 0);
        match TABS.iter().find(|t| t.0 == id) {
            Some(t) => c.call(EE_SCREENS_SH_BUTTON_TEXT, t.1, self.font),
            None => c.call(EE_SCREENS_SH_BUTTON_TEXT_BYTES, c"".as_ptr() as i64, self.font),
        };
        c.call_f(EE_SCREENS_SH_TEXT_FG, 0, [1.0, 1.0, 1.0, 0.0]);
    }
    fn virt(&self, c: &Ctx, v: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        if v == EE_SCREENS_SV_CHECK {
            let id = id_of(c);
            if let Some(t) = TABS.iter().find(|t| t.0 == id) {
                c.call(EE_SCREENS_SH_PLAYER_PAGE, t.2, 0);
            }
        }
        c.base(v, a, b, f)
    }
}

/// `EESERBUnitTypes`: the tech tree page's unit-type radios; checking one refills the tech lists.
struct UnitTypes;

impl Kind for UnitTypes {
    fn name(&self) -> &'static CStr {
        c"EESERBUnitTypes"
    }
    fn overrides(&self) -> u64 {
        1 << EE_SCREENS_SV_CHECK
    }
    fn virt(&self, c: &Ctx, v: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        let r = c.base(v, a, b, f);
        if v == EE_SCREENS_SV_CHECK {
            c.call(EE_SCREENS_SH_FILL_TECH_LISTS, 0, 0);
        }
        r
    }
}

/// `EESECBMaxPlayer`: the max players combo box; a choice sets the player count (the line + 2) first.
struct MaxPlayer;

impl Kind for MaxPlayer {
    fn name(&self) -> &'static CStr {
        c"EESECBMaxPlayer"
    }
    fn overrides(&self) -> u64 {
        1 << EE_SCREENS_SV_CHANGE_SELECTION
    }
    fn virt(&self, c: &Ctx, v: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        if v == EE_SCREENS_SV_CHANGE_SELECTION {
            let line = c.call(EE_SCREENS_SH_LIST_OF_COMBO_SELECTION, 0, 0) as u32;
            c.call(EE_SCREENS_SH_SET_PLAYER_COUNT, i64::from(line.wrapping_add(2)), 0);
        }
        c.base(v, a, b, f)
    }
}

/// `UIBMap`: the map tab's 'Create Map' button.
struct MapButton {
    map: i64,
}

impl Kind for MapButton {
    fn name(&self) -> &'static CStr {
        c"UIBMap"
    }
    fn overrides(&self) -> u64 {
        1 << EE_SCREENS_SV_BUTTON_UP
    }
    fn virt(&self, c: &Ctx, v: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        let r = c.base(v, a, b, f);
        if v == EE_SCREENS_SV_BUTTON_UP {
            c.call(EE_SCREENS_SH_MAP_CREATE, self.map, 0);
        }
        r
    }
}

/// `UIRBMap`, `EEUIFSETRBPaintType`: a radio button whose check tells its tab (the base's check first).
struct CheckThen {
    name: &'static CStr,
    op: u32,
}

impl Kind for CheckThen {
    fn name(&self) -> &'static CStr {
        self.name
    }
    fn overrides(&self) -> u64 {
        1 << EE_SCREENS_SV_CHECK
    }
    fn virt(&self, c: &Ctx, v: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        let r = c.base(v, a, b, f);
        if v == EE_SCREENS_SV_CHECK {
            c.call(self.op, i64::from(id_of(c)), 0);
        }
        r
    }
}

/// `EEUIFSEMainButton`: the editor's main tab buttons (32x32 cells of the button texture: the normal and the
/// pressed one; a checked button shows the pressed cell both ways).
struct MainButton {
    flag: u8,
    texture: i64,
    tab: i32,
    /// The normal cell's x, y, the pressed cell's.
    cells: [f32; 4],
    /// `mNormalBitmap2`, `mPressedBitmap2`.
    normal: Cell<i64>,
    pressed: Cell<i64>,
}

impl MainButton {
    /// One 32x32 cell of the texture as a new bitmap.
    fn cell(&self, c: &Ctx, size: [f32; 2], x: f32, y: f32) -> i64 {
        let b = c.call(EE_SCREENS_SH_NEW_BITMAP, self.texture, 0);
        let (left, top) = (x / size[0], y / size[1]);
        let (right, bottom) = ((x + 32.0) / size[0], (y + 32.0) / size[1]);
        c.call_f(EE_SCREENS_SH_BITMAP_COORDS, b, [left, top, right, bottom]);
        c.call_f(EE_SCREENS_SH_BITMAP_SCALE, b, [32.0 / size[0], 32.0 / size[1], 0.0, 0.0]);
        b
    }
    fn show(&self, c: &Ctx, normal: i64) {
        c.call(EE_SCREENS_SH_SET_PRESSED, self.pressed.get(), 0);
        c.call(EE_SCREENS_SH_SET_NORMAL, normal, 0);
    }
    fn tab(&self) -> i64 {
        (i64::from(self.flag) << 32) | i64::from(self.tab as u32)
    }
}

impl Kind for MainButton {
    fn name(&self) -> &'static CStr {
        c"EEUIFSEMainButton"
    }
    fn overrides(&self) -> u64 {
        1 << EE_SCREENS_SV_CHECK | 1 << EE_SCREENS_SV_UNCHECK
    }
    fn init(&self, c: &Ctx) {
        let mut f = [0.0f32; 4];
        c.host.call(c.ctl, EE_SCREENS_SH_TEXTURE_SIZE, self.texture, 0, &mut f);
        let size = [f[0], f[1]];
        self.normal.set(self.cell(c, size, self.cells[0], self.cells[1]));
        self.pressed.set(self.cell(c, size, self.cells[2], self.cells[3]));
        c.call(EE_SCREENS_SH_SET_NORMAL, self.normal.get(), 0);
        c.call(EE_SCREENS_SH_SET_PRESSED, self.pressed.get(), 0);
        c.call_f(EE_SCREENS_SH_BITMAP_OFFSET, 0, [0.0; 4]);
    }
    fn free(&self, c: &Ctx) {
        c.call(EE_SCREENS_SH_BITMAP_RELEASE, self.normal.get(), 0);
        c.call(EE_SCREENS_SH_BITMAP_RELEASE, self.pressed.get(), 0);
    }
    fn virt(&self, c: &Ctx, v: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        let r = c.base(v, a, b, f);
        match v {
            EE_SCREENS_SV_CHECK => {
                c.call(EE_SCREENS_SH_SELECT_TAB, 0, self.tab());
                self.show(c, self.pressed.get());
            }
            EE_SCREENS_SV_UNCHECK => self.show(c, self.normal.get()),
            _ => {}
        }
        r
    }
    fn op(&self, c: &Ctx, op: u32, _a: i64, _b: i64, _f: &mut [f32; 4]) -> i64 {
        if op == EE_SCREENS_SKOP_CHECK_DIRECT {
            c.base0(EE_SCREENS_SV_CHECK);
            c.call(EE_SCREENS_SH_SELECT_TAB, 1, self.tab());
            self.show(c, self.pressed.get());
        }
        0
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use core::cell::RefCell;

    struct Rec {
        id: i64,
        answers: Vec<(u32, i64)>,
        calls: RefCell<Vec<(u32, i64, i64)>>,
    }

    impl ShellHost for Rec {
        fn call(&self, _ctl: Cpp, op: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
            if op == EE_SCREENS_SH_ID {
                return self.id;
            }
            if op == EE_SCREENS_SH_TEXTURE_SIZE {
                f[0] = 256.0;
                f[1] = 128.0;
            }
            self.calls.borrow_mut().push((op, a, b));
            self.answers.iter().find(|x| x.0 == op).map_or(0, |x| x.1)
        }
    }

    fn rec(id: i64, answers: &[(u32, i64)]) -> Rec {
        Rec { id, answers: answers.to_vec(), calls: RefCell::new(Vec::new()) }
    }

    #[test]
    fn the_select_buttons_move_their_lines() {
        // add all: list 0x154's lines 0..n-1 to the left, for the player combo's player + 1
        let h = rec(0x158, &[(EE_SCREENS_SH_LIST_COUNT, 5), (EE_SCREENS_SH_EDITOR_PLAYER, 2)]);
        let c = Ctx { ctl: core::ptr::null_mut(), host: &h };
        Select { font: 0 }.select(&c);
        assert_eq!(h.calls.borrow().last(), Some(&(EE_SCREENS_SH_TECH_TREE_MOVE, 4 << 32, 3 | 1 << 32)));
        // remove with no selection: nothing
        let h = rec(0x157, &[(EE_SCREENS_SH_LIST_SELECTION, -1)]);
        Select { font: 0 }.select(&Ctx { ctl: core::ptr::null_mut(), host: &h });
        assert_eq!(h.calls.borrow().len(), 1);
        // add the selected line 7
        let h = rec(0x156, &[(EE_SCREENS_SH_LIST_SELECTION, 7)]);
        Select { font: 0 }.select(&Ctx { ctl: core::ptr::null_mut(), host: &h });
        assert_eq!(h.calls.borrow().last(), Some(&(EE_SCREENS_SH_TECH_TREE_MOVE, 7 | 7 << 32, 1 | 1 << 32)));
    }

    #[test]
    fn the_tabs_pick_their_strings_and_pages() {
        let h = rec(0x15a, &[]);
        let c = Ctx { ctl: core::ptr::null_mut(), host: &h };
        let t = Tab { font: 9 };
        t.init(&c);
        assert_eq!(h.calls.borrow()[1], (EE_SCREENS_SH_BUTTON_TEXT, 20_546, 9));
        t.virt(&c, EE_SCREENS_SV_CHECK, 0, 0, &mut [0.0; 4]);
        let calls = h.calls.borrow();
        assert_eq!(calls[calls.len() - 2], (EE_SCREENS_SH_PLAYER_PAGE, 4, 0));
        assert_eq!(calls[calls.len() - 1], (EE_SCREENS_SH_BASE + EE_SCREENS_SV_CHECK, 0, 0));
    }

    #[test]
    fn the_main_button_cuts_its_cells_and_swaps_them() {
        let h = rec(1, &[(EE_SCREENS_SH_NEW_BITMAP, 77)]);
        let c = Ctx { ctl: core::ptr::null_mut(), host: &h };
        let k = make(EE_SCREENS_SK_MAIN_BUTTON, &[2, 5, 3, 0, 0, 0, 0]).unwrap();
        k.init(&c);
        k.virt(&c, EE_SCREENS_SV_CHECK, 0, 0, &mut [0.0; 4]);
        let calls = h.calls.borrow();
        assert!(calls.contains(&(EE_SCREENS_SH_SELECT_TAB, 0, 2 << 32 | 3)));
        assert_eq!(calls.last(), Some(&(EE_SCREENS_SH_SET_NORMAL, 77, 0)));
    }

    #[test]
    fn the_max_players_combo_sets_the_count_first() {
        let h = rec(1, &[(EE_SCREENS_SH_LIST_OF_COMBO_SELECTION, 4)]);
        let c = Ctx { ctl: core::ptr::null_mut(), host: &h };
        MaxPlayer.virt(&c, EE_SCREENS_SV_CHANGE_SELECTION, 0, 0, &mut [0.0; 4]);
        assert_eq!(
            *h.calls.borrow(),
            vec![
                (EE_SCREENS_SH_LIST_OF_COMBO_SELECTION, 0, 0),
                (EE_SCREENS_SH_SET_PLAYER_COUNT, 6, 0),
                (EE_SCREENS_SH_BASE + EE_SCREENS_SV_CHANGE_SELECTION, 0, 0)
            ]
        );
    }
}
