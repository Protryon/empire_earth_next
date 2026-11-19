//! The trigger tab's five sub-screens as one Rust record (the scenario editor lane, round 1): once the C++ classes
//! `EESETSBaseObject` (the base, vtable 0x443858: the selection and its flag, 14 pure and four empty slots) and its
//! subclasses `EESETSTrigger`, `EESETSCondition`, `EESETSEffect`, `EESETSObject` and `EESETSArea`, each a holder of
//! its Rust object (`editor_trigger.rs`, `editor_subscreens.rs`, `editor_object.rs`, `editor_area.rs`) whose slots
//! forwarded to it with the base's selection. The trigger tab (`editor_tscreen.rs`) reaches them through its shell,
//! which is now this record: the tab's host answers its sub-screen ops here (`editor_tscreen_ffi.rs`).
//!
//! [`route`] is the forwarders' table: which of a sub-screen's own calls each base slot was, and where the selection
//! went in its arguments. The calls themselves are the FFI's.

use core::cell::Cell;

use crate::screens::editor_area::*;
use crate::screens::editor_object::*;
use crate::screens::editor_subscreens::*;
use crate::screens::editor_trigger::*;
use crate::screens::editor_tscreen::*;

/// The sub-screens by index (`kSETriggerScreenTrigger` ..: the factory's order, `EESETriggerScreen::CreateScreen`).
pub const SUB_TRIGGER: usize = 0;
pub const SUB_CONDITION: usize = 1;
pub const SUB_EFFECT: usize = 2;
pub const SUB_OBJECT: usize = 3;
pub const SUB_AREA: usize = 4;
pub const SUB_COUNT: usize = 5;

/// An argument of a routed call: a constant, or the sub-screen's selection (`(uint32_t)this->mSelection`).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Arg {
    Zero,
    Selection,
    Value(u32),
}

/// One base slot on one sub-screen.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Route {
    /// The base's empty slot (or an empty override: the trigger's, condition's and effect's `Deactivate`).
    None,
    /// The sub-screen's own call (its `what` and two arguments; the condition and effect screens take one).
    Call(u32, Arg, Arg),
    /// `MouseMove` at the point (the object and area screens').
    MouseMove,
}

/// Which call the base slot `slot` (`EE_SCREENS_SETS_SCREEN_*`) is on the sub-screen `sub` (`SUB_*`).
pub fn route(sub: usize, slot: i32) -> Route {
    use Arg::{Selection as S, Value as V, Zero as Z};
    let list = |sm: i32| V(sm as u32);
    match sub {
        SUB_TRIGGER => {
            let c = |what, a| Route::Call(what, S, a);
            match slot {
                EE_SCREENS_SETS_SCREEN_SORT => c(EE_SCREENS_SETR_LIST_OP, list(EE_SCREENS_SEOB_SM_SORT)),
                EE_SCREENS_SETS_SCREEN_MOVE_DOWN => c(EE_SCREENS_SETR_LIST_OP, list(EE_SCREENS_SEOB_SM_DOWN)),
                EE_SCREENS_SETS_SCREEN_MOVE_UP => c(EE_SCREENS_SETR_LIST_OP, list(EE_SCREENS_SEOB_SM_UP)),
                EE_SCREENS_SETS_SCREEN_DELETE => c(EE_SCREENS_SETR_LIST_OP, list(EE_SCREENS_SEOB_SM_DELETE)),
                EE_SCREENS_SETS_SCREEN_DUPLICATE => c(EE_SCREENS_SETR_LIST_OP, list(EE_SCREENS_SEOB_SM_DUPLICATE)),
                EE_SCREENS_SETS_SCREEN_NEW => c(EE_SCREENS_SETR_LIST_OP, list(EE_SCREENS_SEOB_SM_NEW)),
                EE_SCREENS_SETS_SCREEN_ACTIVATE => c(EE_SCREENS_SETR_ACTIVATE, Z),
                EE_SCREENS_SETS_SCREEN_STORE => c(EE_SCREENS_SETR_STORE, Z),
                EE_SCREENS_SETS_SCREEN_LOAD => c(EE_SCREENS_SETR_LOAD, Z),
                EE_SCREENS_SETS_SCREEN_HIDE => c(EE_SCREENS_SETR_HIDE, Z),
                EE_SCREENS_SETS_SCREEN_SHOW => c(EE_SCREENS_SETR_SHOW, Z),
                _ => Route::None,
            }
        }
        SUB_CONDITION | SUB_EFFECT => {
            let c = |what| Route::Call(what, S, Z);
            match slot {
                EE_SCREENS_SETS_SCREEN_SORT => c(EE_SCREENS_SESC_SORT),
                EE_SCREENS_SETS_SCREEN_MOVE_DOWN => c(EE_SCREENS_SESC_MOVE_DOWN),
                EE_SCREENS_SETS_SCREEN_MOVE_UP => c(EE_SCREENS_SESC_MOVE_UP),
                EE_SCREENS_SETS_SCREEN_DELETE => c(EE_SCREENS_SESC_DELETE),
                EE_SCREENS_SETS_SCREEN_DUPLICATE => c(EE_SCREENS_SESC_DUPLICATE),
                EE_SCREENS_SETS_SCREEN_NEW => c(EE_SCREENS_SESC_NEW),
                EE_SCREENS_SETS_SCREEN_ACTIVATE => c(EE_SCREENS_SESC_ACTIVATE),
                EE_SCREENS_SETS_SCREEN_STORE => c(EE_SCREENS_SESC_STORE),
                EE_SCREENS_SETS_SCREEN_HIDE => c(EE_SCREENS_SESC_HIDE),
                EE_SCREENS_SETS_SCREEN_LOAD => c(EE_SCREENS_SESC_LOAD),
                EE_SCREENS_SETS_SCREEN_SHOW => c(EE_SCREENS_SESC_SHOW),
                EE_SCREENS_SETS_SCREEN_REFRESH => c(EE_SCREENS_SESC_REFRESH),
                _ => Route::None,
            }
        }
        SUB_OBJECT => {
            let l = |sm: i32| Route::Call(EE_SCREENS_SEOB_LIST_OP, list(sm), S);
            match slot {
                EE_SCREENS_SETS_SCREEN_SORT => l(EE_SCREENS_SEOB_SM_SORT),
                EE_SCREENS_SETS_SCREEN_MOVE_DOWN => l(EE_SCREENS_SEOB_SM_DOWN),
                EE_SCREENS_SETS_SCREEN_MOVE_UP => l(EE_SCREENS_SEOB_SM_UP),
                EE_SCREENS_SETS_SCREEN_DELETE => l(EE_SCREENS_SEOB_SM_DELETE),
                EE_SCREENS_SETS_SCREEN_DUPLICATE => l(EE_SCREENS_SEOB_SM_DUPLICATE),
                EE_SCREENS_SETS_SCREEN_NEW => l(EE_SCREENS_SEOB_SM_NEW),
                EE_SCREENS_SETS_SCREEN_DEACTIVATE => Route::Call(EE_SCREENS_SEOB_DEACTIVATE, Z, Z),
                EE_SCREENS_SETS_SCREEN_ACTIVATE => Route::Call(EE_SCREENS_SEOB_ACTIVATE, Z, Z),
                EE_SCREENS_SETS_SCREEN_STORE => Route::Call(EE_SCREENS_SEOB_STORE, S, Z),
                EE_SCREENS_SETS_SCREEN_HIDE => Route::Call(EE_SCREENS_SEOB_HIDE, Z, Z),
                EE_SCREENS_SETS_SCREEN_LOAD => Route::Call(EE_SCREENS_SEOB_LOAD, S, Z),
                EE_SCREENS_SETS_SCREEN_SHOW => Route::Call(EE_SCREENS_SEOB_SHOW, Z, Z),
                EE_SCREENS_SETS_SCREEN_MOUSE_MOVE => Route::MouseMove,
                _ => Route::None,
            }
        }
        SUB_AREA => {
            let c = |what| Route::Call(what, S, Z);
            match slot {
                EE_SCREENS_SETS_SCREEN_SORT => c(EE_SCREENS_SEAR_SORT),
                EE_SCREENS_SETS_SCREEN_UPDATE => c(EE_SCREENS_SEAR_UPDATE),
                EE_SCREENS_SETS_SCREEN_MOVE_DOWN => c(EE_SCREENS_SEAR_MOVE_DOWN),
                EE_SCREENS_SETS_SCREEN_MOVE_UP => c(EE_SCREENS_SEAR_MOVE_UP),
                EE_SCREENS_SETS_SCREEN_DELETE => c(EE_SCREENS_SEAR_DELETE),
                EE_SCREENS_SETS_SCREEN_DEACTIVATE => c(EE_SCREENS_SEAR_DEACTIVATE),
                EE_SCREENS_SETS_SCREEN_DUPLICATE => c(EE_SCREENS_SEAR_DUPLICATE),
                EE_SCREENS_SETS_SCREEN_ACTIVATE => c(EE_SCREENS_SEAR_ACTIVATE),
                EE_SCREENS_SETS_SCREEN_STORE => c(EE_SCREENS_SEAR_STORE),
                EE_SCREENS_SETS_SCREEN_HIDE => c(EE_SCREENS_SEAR_HIDE),
                EE_SCREENS_SETS_SCREEN_LOAD => c(EE_SCREENS_SEAR_LOAD),
                EE_SCREENS_SETS_SCREEN_NEW => c(EE_SCREENS_SEAR_NEW),
                EE_SCREENS_SETS_SCREEN_SHOW => c(EE_SCREENS_SEAR_SHOW),
                EE_SCREENS_SETS_SCREEN_MOUSE_MOVE => Route::MouseMove,
                _ => Route::None,
            }
        }
        _ => Route::None,
    }
}

/// An argument's value for a sub-screen whose selection is `selection`.
pub fn arg(a: Arg, selection: i32) -> u32 {
    match a {
        Arg::Zero => 0,
        Arg::Selection => selection as u32,
        Arg::Value(v) => v,
    }
}

/// The arguments of the sub-screen `sub`'s own call `what` made from outside the base's slots (the events', the
/// controls' and the editor gate's: `ChangeConditionType`, `UpdateSelection`, `SetAreaMode` ..), where its class
/// put its selection: the trigger and area screens' calls take it first and `a` second, the condition and effect
/// screens' it alone; the object screen's `Store`, `Load` and `UpdateSelection` take it first, its list operations
/// second, its other calls `a` and `b` as they are.
pub fn op_args(sub: usize, what: u32, a: u32, b: u32, selection: i32) -> (u32, u32) {
    let s = selection as u32;
    match sub {
        SUB_OBJECT => match what {
            EE_SCREENS_SEOB_STORE | EE_SCREENS_SEOB_LOAD | EE_SCREENS_SEOB_UPDATE_SELECTION => (s, 0),
            EE_SCREENS_SEOB_LIST_OP => (a, s),
            _ => (a, b),
        },
        SUB_CONDITION | SUB_EFFECT => (s, 0),
        _ => (s, a),
    }
}

/// One sub-screen: its Rust object (the FFI's box, as an address; 0 before it is made) and the base's two fields.
#[derive(Default)]
pub struct Sub {
    pub obj: Cell<usize>,
    /// `mSelection` (+0x04; a `long`, read and written as 32 bits)
    pub selection: Cell<i32>,
    /// `mHasSelection` (+0x08)
    pub has_selection: Cell<bool>,
}

/// The five, in the factory's order. The trigger tab's shell.
#[derive(Default)]
pub struct Subs(pub [Sub; SUB_COUNT]);

impl Subs {
    /// The sub-screen `k` (None out of range).
    pub fn get(&self, k: i32) -> Option<&Sub> {
        usize::try_from(k).ok().and_then(|k| self.0.get(k))
    }

    /// `EE_SCREENS_SETS_OP_SELECTION`: the selection, and whether there is one into `has`.
    pub fn selection(&self, k: i32, has: &mut i32) -> i32 {
        let Some(s) = self.get(k) else { return 0 };
        *has = i32::from(s.has_selection.get());
        s.selection.get()
    }

    /// `EE_SCREENS_SETS_OP_SET_SELECTION`: the selection set unless `selection` is negative, the flag unless `has`
    /// is.
    pub fn set_selection(&self, k: i32, selection: i32, has: i32) {
        if let Some(s) = self.get(k) {
            if selection >= 0 {
                s.selection.set(selection);
            }
            if has >= 0 {
                s.has_selection.set(has != 0);
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_base_starts_with_no_selection() {
        let s = Subs::default();
        let mut has = 7;
        assert_eq!(s.selection(SUB_AREA as i32, &mut has), 0);
        assert_eq!(has, 0);
        s.set_selection(SUB_AREA as i32, 4, -1);
        assert_eq!(s.selection(SUB_AREA as i32, &mut has), 4);
        assert_eq!(has, 0);
        s.set_selection(SUB_AREA as i32, -1, 1);
        assert_eq!(s.selection(SUB_AREA as i32, &mut has), 4);
        assert_eq!(has, 1);
        // another sub-screen is untouched; out of range is nothing
        assert_eq!(s.selection(SUB_TRIGGER as i32, &mut has), 0);
        assert_eq!(s.selection(9, &mut has), 0);
        s.set_selection(-1, 3, 1);
    }

    #[test]
    fn the_forwarders_pass_the_selection_where_the_classes_did() {
        use Arg::*;
        // the trigger screen: the selection first, the list op's kind second
        assert_eq!(
            route(SUB_TRIGGER, EE_SCREENS_SETS_SCREEN_DUPLICATE),
            Route::Call(EE_SCREENS_SETR_LIST_OP, Selection, Value(EE_SCREENS_SEOB_SM_DUPLICATE as u32))
        );
        assert_eq!(
            route(SUB_TRIGGER, EE_SCREENS_SETS_SCREEN_STORE),
            Route::Call(EE_SCREENS_SETR_STORE, Selection, Zero)
        );
        // the object screen: the list op's kind first, the selection second; Store and Load take it first
        assert_eq!(
            route(SUB_OBJECT, EE_SCREENS_SETS_SCREEN_SORT),
            Route::Call(EE_SCREENS_SEOB_LIST_OP, Value(EE_SCREENS_SEOB_SM_SORT as u32), Selection)
        );
        assert_eq!(route(SUB_OBJECT, EE_SCREENS_SETS_SCREEN_LOAD), Route::Call(EE_SCREENS_SEOB_LOAD, Selection, Zero));
        assert_eq!(route(SUB_OBJECT, EE_SCREENS_SETS_SCREEN_SHOW), Route::Call(EE_SCREENS_SEOB_SHOW, Zero, Zero));
        assert_eq!(route(SUB_OBJECT, EE_SCREENS_SETS_SCREEN_MOUSE_MOVE), Route::MouseMove);
        // the condition and effect screens: one table
        for sub in [SUB_CONDITION, SUB_EFFECT] {
            assert_eq!(
                route(sub, EE_SCREENS_SETS_SCREEN_REFRESH),
                Route::Call(EE_SCREENS_SESC_REFRESH, Selection, Zero)
            );
            assert_eq!(route(sub, EE_SCREENS_SETS_SCREEN_DEACTIVATE), Route::None);
            assert_eq!(route(sub, EE_SCREENS_SETS_SCREEN_MOUSE_MOVE), Route::None);
        }
        // the area screen overrides Update and MouseMove; the others leave the base's empty slots
        assert_eq!(
            route(SUB_AREA, EE_SCREENS_SETS_SCREEN_UPDATE),
            Route::Call(EE_SCREENS_SEAR_UPDATE, Selection, Zero)
        );
        assert_eq!(route(SUB_AREA, EE_SCREENS_SETS_SCREEN_MOUSE_MOVE), Route::MouseMove);
        assert_eq!(route(SUB_AREA, EE_SCREENS_SETS_SCREEN_REFRESH), Route::None);
        for sub in [SUB_TRIGGER, SUB_OBJECT] {
            assert_eq!(route(sub, EE_SCREENS_SETS_SCREEN_UPDATE), Route::None);
            assert_eq!(route(sub, EE_SCREENS_SETS_SCREEN_UPDATE_KEYS), Route::None);
            assert_eq!(route(sub, EE_SCREENS_SETS_SCREEN_REFRESH), Route::None);
        }
        assert_eq!(route(SUB_TRIGGER, EE_SCREENS_SETS_SCREEN_DEACTIVATE), Route::None);
        assert_eq!(route(SUB_COUNT, EE_SCREENS_SETS_SCREEN_SORT), Route::None);
    }

    #[test]
    fn every_slot_of_every_sub_screen_has_a_route() {
        // the 14 pure slots: every sub-screen answers them (an empty override is None, as the original's `ret`)
        let pure = [
            EE_SCREENS_SETS_SCREEN_SORT,
            EE_SCREENS_SETS_SCREEN_MOVE_DOWN,
            EE_SCREENS_SETS_SCREEN_DELETE,
            EE_SCREENS_SETS_SCREEN_DUPLICATE,
            EE_SCREENS_SETS_SCREEN_ACTIVATE,
            EE_SCREENS_SETS_SCREEN_STORE,
            EE_SCREENS_SETS_SCREEN_HIDE,
            EE_SCREENS_SETS_SCREEN_LOAD,
            EE_SCREENS_SETS_SCREEN_MOVE_UP,
            EE_SCREENS_SETS_SCREEN_NEW,
            EE_SCREENS_SETS_SCREEN_SHOW,
        ];
        for sub in 0..SUB_COUNT {
            for slot in pure {
                assert!(matches!(route(sub, slot), Route::Call(..)), "sub {sub} slot {slot}");
            }
        }
        assert_eq!(arg(Arg::Selection, -2), u32::MAX - 1);
        assert_eq!(arg(Arg::Value(5), 1), 5);
        assert_eq!(arg(Arg::Zero, 1), 0);
    }

    #[test]
    fn the_own_calls_take_the_selection_where_the_classes_put_it() {
        assert_eq!(op_args(SUB_TRIGGER, EE_SCREENS_SETR_STEP, 2, 9, 5), (5, 2));
        assert_eq!(op_args(SUB_AREA, EE_SCREENS_SEAR_SET_MODE, 1, 9, 5), (5, 1));
        assert_eq!(op_args(SUB_CONDITION, EE_SCREENS_SESC_CHANGE_TYPE, 1, 9, 5), (5, 0));
        assert_eq!(op_args(SUB_EFFECT, EE_SCREENS_SESC_CHANGE_ACTION, 1, 9, 5), (5, 0));
        assert_eq!(op_args(SUB_OBJECT, EE_SCREENS_SEOB_UPDATE_SELECTION, 1, 9, 5), (5, 0));
        assert_eq!(op_args(SUB_OBJECT, EE_SCREENS_SEOB_CHECK_CHANGED, 0x846, 2, 5), (0x846, 2));
        assert_eq!(op_args(SUB_OBJECT, EE_SCREENS_SEOB_SET_SELECT_ON_MAP, 1, 0, 5), (1, 0));
        assert_eq!(op_args(SUB_OBJECT, EE_SCREENS_SEOB_LIST_OP, 3, 0, 5), (3, 5));
    }
}
