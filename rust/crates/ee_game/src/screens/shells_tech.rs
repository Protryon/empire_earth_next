//! `EESEUITechListBox` as a shell kind (`shells.rs`): the scenario editor player tab's two technology lists (the
//! enabled techs, id 0x154, and the disabled ones, 0x155). The list keeps its tech ids (one per line) and the lines
//! to mark; `Rebuild` sorts the ids with MSVC's `std::sort` (`ee_sim::units::msvc_sort`: the order of techs of equal
//! names is the introsort's, so it must be that sort) by epoch then name, or by name, and rewrites the lines; `Fill`
//! moves lines to the other list; `CopyData` marks the visible techs related to the selected one and `DrawPolygons`
//! draws a blue bar behind each. The tech tree, the strings' ids and the drawing are the host's.

use core::cell::{Cell, RefCell};
use core::ffi::CStr;

use crate::screens::shells::*;

/// `EESEUITechListBox` (a string list box).
pub const EE_SCREENS_SK_TECH_LIST: u32 = 0x40;

/// `AddTech(a)`.
pub const EE_SCREENS_SKOP_TECH_ADD: u32 = 0x40;
/// `Clear()`.
pub const EE_SCREENS_SKOP_TECH_CLEAR: u32 = 0x41;
/// `Rebuild()`.
pub const EE_SCREENS_SKOP_TECH_REBUILD: u32 = 0x42;
/// `Fill(a, b)`.
pub const EE_SCREENS_SKOP_TECH_FILL: u32 = 0x43;
/// `mTechs.size()`.
pub const EE_SCREENS_SKOP_TECH_COUNT: u32 = 0x44;
/// `mTechs[a]` (100000 past the end).
pub const EE_SCREENS_SKOP_TECH_AT: u32 = 0x45;

/// `mDependents.size()`.
pub const EE_SCREENS_SKOP_TECH_MARKED_COUNT: u32 = 0x46;
/// `mDependents[a]`.
pub const EE_SCREENS_SKOP_TECH_MARKED_AT: u32 = 0x47;

/// A list box's `GetCurrentSelection` (-1 when it reports none).
pub const EE_SCREENS_SH_LIST_CURRENT: u32 = 0x50;
/// A list box's visible lines: `ScrollBarExists() ? GetDisplayLine() | (GetNumberLinesDisplayed() + it) << 32 :
/// GetNumLines() << 32`.
pub const EE_SCREENS_SH_LIST_VIEW: u32 = 0x51;
/// A list box's `DeleteAllEntries()`.
pub const EE_SCREENS_SH_LIST_DELETE_ALL: u32 = 0x52;
/// A list box's `AppendLine(UWideString(a: NUL-terminated bytes))`.
pub const EE_SCREENS_SH_LIST_APPEND_BYTES: u32 = 0x53;
/// A list box's `GetLineCount()`.
pub const EE_SCREENS_SH_LIST_LINE_COUNT: u32 = 0x54;
/// A list box's `SetStartingDisplayLine(a)`.
pub const EE_SCREENS_SH_LIST_START_LINE: u32 = 0x55;
/// A list box's `ChangeSelectionIndex(a, false, true)` (virtual).
pub const EE_SCREENS_SH_LIST_SELECT_SCROLL: u32 = 0x56;
/// The form's control of id `a`: `GetIsChecked()` (a check box or radio button).
pub const EE_SCREENS_SH_FORM_CHECKED: u32 = 0x57;
/// The form's control of id `a` (its address).
pub const EE_SCREENS_SH_FORM_CONTROL: u32 = 0x58;
/// The kind's op `b >> 32` with the argument `b & 0xffffffff` on the shell control `a` (answered in Rust).
pub const EE_SCREENS_SH_KIND_OP: u32 = 0x59;
/// `gTechTree.Get(a)`'s string: `b` 0 its unit class's name (`gUnitClassTable[mData->mUnitClass]->mNameStringID`),
/// 1 its own (`mData->mNameID`) -> the string id.
pub const EE_SCREENS_SH_TECH_NAME_ID: u32 = 0x5a;
/// `EETechTreeEpochOf(a)`.
pub const EE_SCREENS_SH_TECH_EPOCH: u32 = 0x5b;
/// `gTechTree.Get(a)->mData->mID`.
pub const EE_SCREENS_SH_TECH_DATA_ID: u32 = 0x5c;
/// The edited player's tech tree: `b` 0: `Get(a & 0xffffffff)->DependsOn(a >> 32)`; 1:
/// `Get(a & 0xffffffff)->DependsOn(Get(a >> 32)->mData->mID)`.
pub const EE_SCREENS_SH_TECH_DEPENDS: u32 = 0x5d;
/// The edited player's tech tree: `Get(a)->SetScenEditorObsoleted(b != 0, true)` -> whether its epoch is reached
/// (`mEpoch <= mCurrentEpoch`).
pub const EE_SCREENS_SH_TECH_OBSOLETE: u32 = 0x5e;
/// The edited player's tech tree: `Get(a)->mScenEditorObsoleted`.
pub const EE_SCREENS_SH_TECH_OBSOLETED: u32 = 0x5f;
/// The player tab's `MarkPlayerChanged(the edited player's id, false)`.
pub const EE_SCREENS_SH_TECH_MARK_PLAYER: u32 = 0x60;
/// The list's blue bars: `a` the lines (u32s), `b` their count (`DrawPolygons`' loop) -> the last draw's result.
pub const EE_SCREENS_SH_TECH_BARS: u32 = 0x61;

/// The enabled list, the disabled list; Display Epochs (0x15e), the Technologies page (0x14d).
const TECH_LIST: u32 = 0x154;
const OTHER_LIST: u32 = 0x155;
const DISPLAY_EPOCH: i64 = 0x15e;
const BY_TECH: i64 = 0x14d;
/// "no tech".
const NONE: i64 = 100_000;

pub(crate) fn make(kind: u32, args: &[i64]) -> Option<Box<dyn Kind>> {
    if kind != EE_SCREENS_SK_TECH_LIST {
        return crate::screens::shells_unit::make(kind, args);
    }
    Some(Box::new(TechList { techs: RefCell::new(Vec::new()), dependents: RefCell::new(Vec::new()), id: Cell::new(0) }))
}

pub(crate) struct TechList {
    /// `mTechs`.
    techs: RefCell<Vec<i32>>,
    /// `mDependents`.
    dependents: RefCell<Vec<u32>>,
    /// The control's id (read at its first use).
    id: Cell<u32>,
}

/// One tech's sort key: its epoch (of the id) and its name's bytes.
#[derive(Clone)]
struct Key {
    epoch: i32,
    name: Vec<u8>,
}

/// The four orders of `Rebuild` (`EESETLEpochUnitLess`, `EESETLEpochTechLess`: by epoch then name;
/// `EESETLUnitLess`, `EESETLTechLess`: by name; names in byte order, as `PlatMbsCmp`).
fn less(by_epoch: bool, l: &Key, r: &Key) -> bool {
    if by_epoch {
        l.epoch < r.epoch || (l.epoch == r.epoch && l.name < r.name)
    } else {
        l.name < r.name
    }
}

/// `Rebuild`'s sort: the ids ordered as `std::sort` with the predicate over their keys.
fn sort_techs(techs: &mut [i32], keys: &dyn Fn(i32) -> Key, by_epoch: bool) {
    let mut keyed: Vec<(i32, usize)> = Vec::with_capacity(techs.len());
    let mut table: Vec<Key> = Vec::with_capacity(techs.len());
    for &t in techs.iter() {
        keyed.push((t, table.len()));
        table.push(keys(t));
    }
    ee_sim::units::msvc_sort::sort_by(&mut keyed, |a, b| less(by_epoch, &table[a.1], &table[b.1]));
    for (d, s) in techs.iter_mut().zip(keyed) {
        *d = s.0;
    }
}

/// A resource string's bytes (the strings table; nothing for a missing id).
fn string(id: i64) -> Vec<u8> {
    #[cfg(not(test))]
    {
        crate::screens::hud_direct::resource_string(id).map(|s| s.to_vec()).unwrap_or_default()
    }
    #[cfg(test)]
    {
        format!("s{id}").into_bytes()
    }
}

impl TechList {
    fn id(&self, c: &Ctx) -> u32 {
        if self.id.get() == 0 {
            self.id.set(c.call(EE_SCREENS_SH_ID, 0, 0) as u32);
        }
        self.id.get()
    }
    fn techs(&self) -> Vec<i32> {
        self.techs.borrow().clone()
    }
    /// `Rebuild`.
    fn rebuild(&self, c: &Ctx) {
        let by_epoch = c.call(EE_SCREENS_SH_FORM_CHECKED, DISPLAY_EPOCH, 0) != 0;
        let by_tech = c.call(EE_SCREENS_SH_FORM_CHECKED, BY_TECH, 0) != 0;
        let name_of = |t: i32| string(c.call(EE_SCREENS_SH_TECH_NAME_ID, i64::from(t), i64::from(by_tech)));
        let mut techs = self.techs();
        sort_techs(
            &mut techs,
            &|t| Key { epoch: c.call(EE_SCREENS_SH_TECH_EPOCH, i64::from(t), 0) as i32, name: name_of(t) },
            by_epoch,
        );
        *self.techs.borrow_mut() = techs.clone();
        c.call(EE_SCREENS_SH_LIST_DELETE_ALL, 0, 0);
        for &t in &techs {
            let mut text = Vec::new();
            if by_epoch {
                let data_id = c.call(EE_SCREENS_SH_TECH_DATA_ID, i64::from(t), 0);
                let epoch = c.call(EE_SCREENS_SH_TECH_EPOCH, data_id, 0) as i32;
                text.extend_from_slice(format!("{}: ", epoch.wrapping_add(1)).as_bytes());
            }
            text.extend_from_slice(&name_of(t));
            text.push(0);
            c.call(EE_SCREENS_SH_LIST_APPEND_BYTES, text.as_ptr() as i64, 0);
        }
        if c.call(EE_SCREENS_SH_LIST_LINE_COUNT, 0, 0) > 0 {
            // (GetCurrentSelection leaves the selection index either way)
            let s = c.call(EE_SCREENS_SH_LIST_CURRENT, 1, 0);
            c.call(EE_SCREENS_SH_LIST_START_LINE, s, 0);
        }
    }
    /// `Fill(first, last)`: moves lines first..=last to the other list.
    fn fill(&self, c: &Ctx, first: i32, last: i32) {
        let enabled = self.id(c) == TECH_LIST;
        let other = c.call(EE_SCREENS_SH_FORM_CONTROL, i64::from(if enabled { OTHER_LIST } else { TECH_LIST }), 0);
        let other_op =
            |op: u32, arg: i64| c.call(EE_SCREENS_SH_KIND_OP, other, (i64::from(op) << 32) | (arg & 0xffff_ffff));
        let other_selection = c.host.call(other as Cpp, EE_SCREENS_SH_LIST_CURRENT, 0, 0, &mut [0.0; 4]);
        let other_tech = if other_selection >= 0 { other_op(EE_SCREENS_SKOP_TECH_AT, other_selection) } else { NONE };
        let techs = self.techs();
        let mut reached = false;
        for &t in &techs[first as usize..=last as usize] {
            if c.call(EE_SCREENS_SH_TECH_OBSOLETE, i64::from(t), i64::from(enabled)) != 0 {
                reached = true;
            }
        }
        if reached {
            c.call(EE_SCREENS_SH_TECH_MARK_PLAYER, 0, 0);
        }
        let wanted = i64::from(!enabled);
        let obsoleted = |t: i32| c.call(EE_SCREENS_SH_TECH_OBSOLETED, i64::from(t), 0);
        let mut tech = NONE;
        if let Some(&t) = techs[last as usize..].iter().find(|&&t| obsoleted(t) == wanted) {
            tech = i64::from(t);
        }
        if tech == NONE {
            // from first down to 1 (the loop stops at begin)
            if let Some(&t) = techs[1..=first as usize].iter().rev().find(|&&t| obsoleted(t) == wanted) {
                tech = i64::from(t);
            }
        }
        c.call(crate::screens::shells_editor::EE_SCREENS_SH_FILL_TECH_LISTS, 0, 0);
        let index_of = |list: &[i32], t: i64| list.iter().position(|&x| i64::from(x) == t).unwrap_or(list.len()) as i64;
        let mine = if tech < NONE { index_of(&self.techs(), tech) } else { 0 };
        c.call(EE_SCREENS_SH_LIST_SELECT_SCROLL, mine, 0);
        let theirs = if other_tech < NONE {
            let n = other_op(EE_SCREENS_SKOP_TECH_COUNT, 0);
            (0..n).find(|&i| other_op(EE_SCREENS_SKOP_TECH_AT, i) == other_tech).unwrap_or(n)
        } else {
            0
        };
        c.host.call(other as Cpp, EE_SCREENS_SH_LIST_SELECT_SCROLL, theirs, 0, &mut [0.0; 4]);
    }
}

impl Kind for TechList {
    fn name(&self) -> &'static CStr {
        c"EESEUITechListBox"
    }
    fn overrides(&self) -> u64 {
        1 << EE_SCREENS_SV_COPY_DATA | 1 << EE_SCREENS_SV_DRAW_POLYGONS
    }
    fn init(&self, c: &Ctx) {
        c.call_f(EE_SCREENS_SH_SELECTED_COLOR, 0, [0.0, 0.38, 0.0, 0.0]);
    }
    fn virt(&self, c: &Ctx, v: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        match v {
            EE_SCREENS_SV_COPY_DATA => {
                let r = c.base(v, a, b, f);
                self.dependents.borrow_mut().clear();
                if r == 0 {
                    let view = c.call(EE_SCREENS_SH_LIST_VIEW, 0, 0);
                    let (first, last) = ((view & 0xffff_ffff) as u32, (view >> 32) as u32);
                    let selection = c.call(EE_SCREENS_SH_LIST_CURRENT, 0, 0);
                    if selection >= 0 {
                        let techs = self.techs();
                        let s = techs[selection as usize];
                        let enabled = self.id(c) == TECH_LIST;
                        for i in first..last {
                            if i64::from(i) == selection {
                                continue;
                            }
                            let t = techs[i as usize];
                            let pair = |x: i32, y: i32| i64::from(x as u32) | (i64::from(y) << 32);
                            let depends = if enabled {
                                c.call(EE_SCREENS_SH_TECH_DEPENDS, pair(t, s), 0)
                            } else {
                                c.call(EE_SCREENS_SH_TECH_DEPENDS, pair(s, t), 1)
                            };
                            if depends != 0 {
                                self.dependents.borrow_mut().push(i - first);
                            }
                        }
                    }
                }
                r
            }
            EE_SCREENS_SV_DRAW_POLYGONS => {
                let r = c.base(v, a, b, f);
                let d = self.dependents.borrow().clone();
                if d.is_empty() {
                    r
                } else {
                    c.call(EE_SCREENS_SH_TECH_BARS, d.as_ptr() as i64, d.len() as i64)
                }
            }
            _ => c.base(v, a, b, f),
        }
    }
    fn op(&self, c: &Ctx, op: u32, a: i64, b: i64, _f: &mut [f32; 4]) -> i64 {
        match op {
            EE_SCREENS_SKOP_TECH_ADD => self.techs.borrow_mut().push(a as i32),
            EE_SCREENS_SKOP_TECH_CLEAR => {
                self.techs.borrow_mut().clear();
                c.call(EE_SCREENS_SH_LIST_DELETE_ALL, 0, 0);
            }
            EE_SCREENS_SKOP_TECH_REBUILD => self.rebuild(c),
            EE_SCREENS_SKOP_TECH_FILL => self.fill(c, a as i32, b as i32),
            EE_SCREENS_SKOP_TECH_COUNT => return self.techs.borrow().len() as i64,
            EE_SCREENS_SKOP_TECH_MARKED_COUNT => return self.dependents.borrow().len() as i64,
            EE_SCREENS_SKOP_TECH_MARKED_AT => {
                return self.dependents.borrow().get(a as usize).map_or(0, |&d| i64::from(d))
            }
            EE_SCREENS_SKOP_TECH_AT => {
                return self.techs.borrow().get(a as usize).map_or(NONE, |&t| i64::from(t));
            }
            _ => {}
        }
        0
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn rebuild_sorts_by_epoch_then_name_as_msvc_sort() {
        // equal keys keep the introsort's order; the names sort bytewise
        let names = |t: i32| -> Key {
            let n: &[u8] = match t % 4 {
                0 => b"Archer",
                1 => b"Zebra",
                2 => b"archer",
                _ => b"Archer",
            };
            Key { epoch: t / 10, name: n.to_vec() }
        };
        let mut v: Vec<i32> = (0..60).rev().collect();
        sort_techs(&mut v, &names, true);
        for w in v.windows(2) {
            let (a, b) = (names(w[0]), names(w[1]));
            assert!(!less(true, &b, &a));
        }
        // the same permutation as the MSVC sort over the ids with the keys' predicate
        let mut ids: Vec<i32> = (0..60).rev().collect();
        ee_sim::units::msvc_sort::sort_by(&mut ids, |a, b| less(true, &names(*a), &names(*b)));
        assert_eq!(v, ids);
        // by name only
        let mut w: Vec<i32> = (0..8).collect();
        sort_techs(&mut w, &names, false);
        assert_eq!(names(w[0]).name, b"Archer");
        assert_eq!(names(w[7]).name, b"archer");
    }
}
