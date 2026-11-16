//! `UIEntityGroup` whole in Rust (R6 ui round 21): a form's group of entities (controls and hot keys) enabled and
//! disabled together. The entities and the disable count are this object; the C++ class keeps one pointer to it and
//! every method forwards here: `Disable`/`Enable` (round 8's loops, `entity_group.rs`), `AddEntity`, `RemoveEntity`,
//! `GetEntity`, `GetEntityCount`, `GetIsEnabled`.
//!
//! Every call out is an [`EeUiOp`] (640..649) on an entity `a`, through the group's host (`UIEntityGroup::HostOp`,
//! `UIForm.cpp`): the questions (pure) and calls `Disable`/`Enable` make on a button or a hot key.

use core::cell::{Cell, RefCell};
use core::ffi::{c_long, c_ulong, c_void};

use crate::entity_group::{self, GroupHost};
use crate::obj::{EeUiHostFn, EeUiOp as Op, Host};

const ERROR_OK: c_long = 0;
const ERROR_NOT_FOUND: c_long = 3;

/// UIEntityGroup's inward calls.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeUiEntityGroupCall {
    /// `Disable()`: the count 1, the shown buttons on the current form hidden (tabbing past them), the hot keys off.
    Disable = 1,
    /// `Enable()`: the count 0, the buttons redrawn, the hot keys on.
    Enable = 2,
    /// `AddEntity(a)` -> `kErrorOK`.
    Add = 3,
    /// `RemoveEntity(a)` -> `kErrorOK`, or `kErrorNotFound` when `a` is not a member.
    Remove = 4,
    /// `GetEntity(a)` (NULL past the end).
    Entity = 5,
    /// `GetEntityCount()`.
    Count = 6,
    /// `GetIsEnabled()`: the count is 0.
    IsEnabled = 7,
}

/// The Rust UIEntityGroup.
pub struct EeUiEntityGroup {
    host: Host,
    entities: RefCell<Vec<*mut c_void>>,
    disable_count: Cell<c_ulong>,
}

struct Loop<'a>(&'a EeUiEntityGroup);

impl Loop<'_> {
    fn at(&self, i: usize) -> isize {
        self.0.entities.borrow().get(i).copied().unwrap_or(core::ptr::null_mut()) as isize
    }
    fn ask(&self, op: Op, i: usize) -> bool {
        self.0.host.op(op, self.at(i), 0, 0, 0) != 0
    }
    fn tell(&self, op: Op, i: usize, b: isize) {
        self.0.host.op(op, self.at(i), b, 0, 0);
    }
}

impl GroupHost for Loop<'_> {
    fn count(&self) -> usize {
        self.0.entities.borrow().len()
    }
    fn is_button(&mut self, i: usize) -> bool {
        self.ask(Op::GrpIsButton, i)
    }
    fn shown(&mut self, i: usize) -> bool {
        self.ask(Op::GrpShown, i)
    }
    fn on_current_form(&mut self, i: usize) -> bool {
        self.ask(Op::GrpOnCurrentForm, i)
    }
    fn add_dirty(&mut self, i: usize) {
        self.tell(Op::GrpAddDirty, i, 0)
    }
    fn hide(&mut self, i: usize) {
        self.tell(Op::GrpHide, i, 0)
    }
    fn set_dirty(&mut self, i: usize) {
        self.tell(Op::GrpSetDirty, i, 0)
    }
    fn tabbing(&mut self, i: usize) -> bool {
        self.ask(Op::GrpTabbing, i)
    }
    fn is_tab_stop(&mut self, i: usize) -> bool {
        self.ask(Op::GrpIsTabStop, i)
    }
    fn tab(&mut self, i: usize) {
        self.tell(Op::GrpTab, i, 0)
    }
    fn set_enabled(&mut self, i: usize, on: bool) {
        self.tell(Op::GrpSetEnabled, i, on as isize)
    }
}

impl EeUiEntityGroup {
    /// `UIEntityGroup()`: no entities, disabled (the count 1).
    pub fn new(host: Host) -> Self {
        EeUiEntityGroup { host, entities: RefCell::new(Vec::new()), disable_count: Cell::new(1) }
    }

    /// One inward call.
    pub fn call(&self, call: EeUiEntityGroupCall, a: isize) -> isize {
        type C = EeUiEntityGroupCall;
        match call {
            C::Disable => {
                self.disable_count.set(1);
                entity_group::disable(&mut Loop(self));
            }
            C::Enable => {
                self.disable_count.set(0);
                entity_group::enable(&mut Loop(self));
            }
            C::Add => {
                self.entities.borrow_mut().push(a as *mut c_void);
                return ERROR_OK as isize;
            }
            C::Remove => {
                let mut e = self.entities.borrow_mut();
                return match e.iter().position(|&x| x as isize == a) {
                    Some(i) => {
                        e.remove(i);
                        ERROR_OK as isize
                    }
                    None => ERROR_NOT_FOUND as isize,
                };
            }
            C::Entity => return Loop(self).at(a as usize),
            C::Count => return self.entities.borrow().len() as isize,
            C::IsEnabled => return (self.disable_count.get() == 0) as isize,
        }
        0
    }
}

// ---- the C ABI

/// A new Rust group for the C++ `group`.
#[no_mangle]
pub extern "C" fn ee_ui_entity_group_new(group: *mut c_void, host: EeUiHostFn) -> *mut EeUiEntityGroup {
    crate::direct::made(
        crate::direct::Kind::Group,
        group,
        Box::into_raw(Box::new(EeUiEntityGroup::new(Host { obj: group, f: host }))),
    )
}

/// The destructor.
///
/// # Safety
/// `group` came from `ee_ui_entity_group_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_entity_group_free(group: *mut EeUiEntityGroup) {
    crate::direct::forget(crate::direct::Kind::Group, group as *const core::ffi::c_void);
    if !group.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(group) });
    }
}

/// One of UIEntityGroup's methods ([`EeUiEntityGroupCall`]).
///
/// # Safety
/// `group` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_entity_group_call(
    group: *const EeUiEntityGroup,
    call: EeUiEntityGroupCall,
    a: isize,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out may re-enter (a form's Tab)
    unsafe { (*group).call(call, a) }
}

#[cfg(test)]
#[path = "groupobj_tests.rs"]
mod tests;
