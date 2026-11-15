//! `UIEvent` whole in Rust (R6 ui round 21): the base of every triggered event. Its member is [`EeUiEventFields`], the
//! C++ class's private base (the inline constructor and `GetIsInitialized` read it in place); `Initialize` and
//! `Uninitialize` (round 7's count, `event.rs`) run over it, calling the virtual `InitializeEvent`/`UninitializeEvent`
//! through the C++ object (the game's events override them); `CreateControlEvents` and `CreateFormEvents` (round 7's
//! loop over the events file) call out to the file and the owners. The virtual `ProcessEvent` stays the subclasses'.
//!
//! Every call out is an [`EeUiOp`] on the host `UIEvent::HostOp` (`UIManager.cpp`): on the record (630, 631) or on the
//! file (the UI database file's ops shared with the background table, 597..600, and the owners' 632, 633).

use core::ffi::{c_int, c_long, c_ulong, c_void};

use crate::event::{self, DbEvent, EventCountHost, EventsHost};
use crate::obj::{EeUiHostFn, EeUiOp as Op, Host};

/// `UIEvent`'s member (`include/lle/UIEvent.h`), the C++ class's private base.
#[repr(C)]
#[allow(non_snake_case)]
#[derive(Debug)]
pub struct EeUiEventFields {
    /// `Initialize`/`Uninitialize`'s nesting: the event is initialised while it is not 0.
    pub mInitializeCount: c_ulong,
}

/// UIEvent's inward calls on an event.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeUiEventCall {
    /// `Initialize()`: one more user, `InitializeEvent` on the first -> its result.
    Initialize = 1,
    /// `Uninitialize()`: one user less, `UninitializeEvent` when the last leaves -> its result.
    Uninitialize = 2,
}

struct Count {
    rec: *mut EeUiEventFields,
    host: Host,
}

impl EventCountHost for Count {
    fn count(&self) -> c_ulong {
        // SAFETY: a live record (ee_ui_event_call's contract), read afresh
        unsafe { (*self.rec).mInitializeCount }
    }
    fn set_count(&mut self, count: c_ulong) {
        // SAFETY: as count
        unsafe { (*self.rec).mInitializeCount = count }
    }
    fn initialize_event(&mut self) -> c_long {
        self.host.op(Op::EvInitialize, 0, 0, 0, 0) as c_long
    }
    fn uninitialize_event(&mut self) -> c_long {
        self.host.op(Op::EvUninitialize, 0, 0, 0, 0) as c_long
    }
}

/// An events file and its owners: `kind` 0 the control tables (`CreateControlEvents`), 1 the manager's forms
/// (`CreateFormEvents`).
struct Events {
    host: Host,
    kind: c_int,
    manager: *mut c_void,
}

impl EventsHost for Events {
    fn open(&mut self) {
        self.host.op(Op::BgtFileOpen, 0, 0, 0, 0);
    }
    fn read_count(&mut self, count: &mut c_ulong) -> c_long {
        let mut f = [0.0f32; 4];
        let r = self.host.op_f(Op::BgtFileCount, 0, 0, 0, 0, &mut f);
        *count = f[0].to_bits() as c_ulong;
        r as c_long
    }
    fn read_record(&mut self, record: &mut DbEvent) -> c_long {
        let p = (record as *mut DbEvent) as isize;
        self.host.op(Op::BgtFileRead, 0, p, core::mem::size_of::<DbEvent>() as isize, 0) as c_long
    }
    fn owner(&mut self, owner: c_ulong) -> *mut c_void {
        self.host.op(Op::EvOwner, self.kind as isize, owner as isize, self.manager as isize, 0) as *mut c_void
    }
    fn add(&mut self, owner: *mut c_void, event_id: c_ulong, event_type: c_ulong) {
        self.host.op(Op::EvAdd, self.kind as isize, owner as isize, event_id as isize, event_type as isize);
    }
    fn close(&mut self) {
        self.host.op(Op::BgtFileClose, 0, 0, 0, 0);
    }
}

/// One of UIEvent's methods on an event's record ([`EeUiEventCall`]); the host's object is the record.
///
/// # Safety
/// `rec` is a live record (a C++ event's base) for the call; the host's object is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_event_call(rec: *mut EeUiEventFields, host: EeUiHostFn, call: EeUiEventCall) -> c_long {
    let mut c = Count { rec, host: Host { obj: rec.cast(), f: host } };
    match call {
        EeUiEventCall::Initialize => event::initialize(&mut c),
        EeUiEventCall::Uninitialize => event::uninitialize(&mut c),
    }
}

/// `UIEvent::CreateControlEvents` (`kind` 0, no manager) and `CreateFormEvents` (`kind` 1): the host's object is the
/// file.
///
/// # Safety
/// `file` is the C++ caller's live file; the host answers the file's and owners' ops.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_event_create(
    file: *mut c_void,
    host: EeUiHostFn,
    kind: c_int,
    manager: *mut c_void,
) -> c_long {
    event::create_events(&mut Events { host: Host { obj: file, f: host }, kind, manager })
}

#[cfg(test)]
#[path = "eventobj_tests.rs"]
mod tests;
