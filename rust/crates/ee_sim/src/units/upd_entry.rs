//! `EEUnitClassUpdate` and `EEUnitClassActionChanged` (EEUnitUpdate.cpp): the update and ActionChanged sequences
//! (update.rs, actions.rs) over the C++ host table, the table and its context made here: the context is the C++
//! `EEUpdateContext` (the class, the unit, the heal and airport scratch), the table's read and act the C++
//! `ee_units_upd_read` and `ee_units_upd_act`. ActionChanged's USST notes (kind 13) bracket the sequence.

use crate::units::update::EeUnitsUpdateHost;

/// EEUnitUpdate.cpp's `EEUpdateContext` (four pointers; asserted there).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeUnitsUpdateContext {
    pub class: u64,
    pub unit: u64,
    pub healee: u64,
    pub airport: u64,
}

/// The sequences and the unit's id, as a trait (the game's; the tests' mock).
pub trait UpdEntry {
    fn unit_id(&mut self, unit: u64) -> u32;
    fn update(&mut self, kind: i32, id: u32, unit: u64, host: &EeUnitsUpdateHost);
    fn action_changed(&mut self, kind: i32, id: u32, unit: u64, host: &EeUnitsUpdateHost);
    fn note_action_begin(&mut self, id: u32);
    fn note_action_end(&mut self);
}

/// The host table over `context` (the C++ read and act).
pub fn host_of(
    context: &mut EeUnitsUpdateContext,
    read: extern "C" fn(*mut core::ffi::c_void, i32) -> i64,
    act: extern "C" fn(*mut core::ffi::c_void, i32, i64, i64) -> i64,
) -> EeUnitsUpdateHost {
    EeUnitsUpdateHost {
        ctx: context as *mut EeUnitsUpdateContext as *mut core::ffi::c_void,
        read,
        act,
        class: context.class,
    }
}

/// `EEUnitClassUpdate(kind, class, unit)`
pub fn class_update<E: UpdEntry + ?Sized>(
    e: &mut E,
    kind: i32,
    class: u64,
    unit: u64,
    host: impl FnOnce(&mut EeUnitsUpdateContext) -> EeUnitsUpdateHost,
) {
    let mut context = EeUnitsUpdateContext { class, unit, healee: 0, airport: 0 };
    let h = host(&mut context);
    let id = e.unit_id(unit);
    e.update(kind, id, unit, &h);
}

/// `EEUnitClassActionChanged(kind, class, unit)`: its USST notes around the sequence.
pub fn class_action_changed<E: UpdEntry + ?Sized>(
    e: &mut E,
    kind: i32,
    class: u64,
    unit: u64,
    host: impl FnOnce(&mut EeUnitsUpdateContext) -> EeUnitsUpdateHost,
) {
    let id = e.unit_id(unit);
    e.note_action_begin(id);
    let mut context = EeUnitsUpdateContext { class, unit, healee: 0, airport: 0 };
    let h = host(&mut context);
    e.action_changed(kind, id, unit, &h);
    e.note_action_end();
}

/// cbindgen:ignore
#[cfg(not(test))]
mod game {
    use super::*;

    extern "C" {
        /// EEUnitUpdate.cpp: the host's reads and acts over an `EEUpdateContext`
        fn ee_units_upd_read(ctx: *mut core::ffi::c_void, code: i32) -> i64;
        fn ee_units_upd_act(ctx: *mut core::ffi::c_void, code: i32, a: i64, b: i64) -> i64;
    }

    extern "C" fn read(ctx: *mut core::ffi::c_void, code: i32) -> i64 {
        // SAFETY: the context of the sequence's call
        unsafe { ee_units_upd_read(ctx, code) }
    }
    extern "C" fn act(ctx: *mut core::ffi::c_void, code: i32, a: i64, b: i64) -> i64 {
        // SAFETY: as above
        unsafe { ee_units_upd_act(ctx, code, a, b) }
    }

    pub fn host(context: &mut EeUnitsUpdateContext) -> EeUnitsUpdateHost {
        host_of(context, read, act)
    }

    pub struct Game;

    impl UpdEntry for Game {
        fn unit_id(&mut self, unit: u64) -> u32 {
            crate::units::unit_geom::with_unit(unit, |u| u.id()).expect("a live unit")
        }
        fn update(&mut self, kind: i32, id: u32, unit: u64, host: &EeUnitsUpdateHost) {
            // SAFETY: a table live for the call
            unsafe { crate::units::update::ee_units_update(kind, id, unit, host) };
        }
        fn action_changed(&mut self, kind: i32, id: u32, unit: u64, host: &EeUnitsUpdateHost) {
            // SAFETY: as above
            unsafe { crate::units::actions::ee_units_action_changed(kind, id, unit, host) };
        }
        fn note_action_begin(&mut self, id: u32) {
            crate::units::notes::ee_units_note_action_begin(id);
        }
        fn note_action_end(&mut self) {
            crate::units::notes::ee_units_note_action_end();
        }
    }
}

/// `EEUnitClassUpdate(kind, class, unit)` (handles: the `EEUnitClass*`, the `EEComplexUnit*`).
#[cfg(not(test))]
#[no_mangle]
pub extern "C" fn ee_units_class_update(kind: i32, class: u64, unit: u64) {
    class_update(&mut game::Game, kind, class, unit, game::host);
}

/// `EEUnitClassActionChanged(kind, class, unit)`.
#[cfg(not(test))]
#[no_mangle]
pub extern "C" fn ee_units_class_action_changed(kind: i32, class: u64, unit: u64) {
    class_action_changed(&mut game::Game, kind, class, unit, game::host);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(Default)]
    struct Mock {
        log: Vec<String>,
    }

    impl UpdEntry for Mock {
        fn unit_id(&mut self, unit: u64) -> u32 {
            unit as u32 + 1
        }
        fn update(&mut self, kind: i32, id: u32, unit: u64, host: &EeUnitsUpdateHost) {
            // SAFETY: the context the entry made for this call
            let c = unsafe { *(host.ctx as *const EeUnitsUpdateContext) };
            self.log.push(format!("update {kind} {id} {unit} {} {:?}", host.class, c));
        }
        fn action_changed(&mut self, kind: i32, id: u32, unit: u64, host: &EeUnitsUpdateHost) {
            self.log.push(format!("action {kind} {id} {unit} {}", host.class));
        }
        fn note_action_begin(&mut self, id: u32) {
            self.log.push(format!("begin {id}"));
        }
        fn note_action_end(&mut self) {
            self.log.push("end".into());
        }
    }

    extern "C" fn read(_: *mut core::ffi::c_void, _: i32) -> i64 {
        0
    }
    extern "C" fn act(_: *mut core::ffi::c_void, _: i32, _: i64, _: i64) -> i64 {
        0
    }

    #[test]
    fn the_update_runs_over_a_fresh_context_of_the_class_and_unit() {
        let mut m = Mock::default();
        class_update(&mut m, 3, 0x10, 0x20, |c| host_of(c, read, act));
        assert_eq!(
            m.log,
            [format!(
                "update 3 33 32 16 {:?}",
                EeUnitsUpdateContext { class: 0x10, unit: 0x20, healee: 0, airport: 0 }
            )]
        );
    }

    #[test]
    fn action_changed_is_bracketed_by_its_notes() {
        let mut m = Mock::default();
        class_action_changed(&mut m, 4, 0x10, 0x20, |c| host_of(c, read, act));
        assert_eq!(m.log, ["begin 33", "action 4 33 32 16", "end"]);
    }
}
