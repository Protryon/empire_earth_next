//! `UIEvent` (`User Interface/UIEvent.cpp`): the UI database's event records and `CreateControlEvents`'/
//! `CreateFormEvents`' loop over them ([`create_events`]: a negative event id or an owner that does not exist skips a
//! record, a failed read keeps the previous record's bytes, the result is the last read's), and the event's
//! initialisation count ([`initialize`], [`uninitialize`]).
//!
//! The owners (a control table by index, a form by id) and their event lists stay C++ ([`EventsHost`]); the
//! event's `InitializeEvent`/`UninitializeEvent` are virtual and are called back.

use core::ffi::{c_long, c_ulong, c_void};

/// `kErrorOK`.
const ERROR_OK: c_long = 0;

/// One record of the control and form events database files (`UIDbDataEvents`, 116 bytes on every target).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct DbEvent {
    pub label: [u8; 100],
    pub unused_database_id: i32,
    pub event_id: i32,
    pub owner_id: u32,
    pub event_type: u32,
}

const _: () = assert!(core::mem::size_of::<DbEvent>() == 116);

impl DbEvent {
    /// A record of zero bytes (the C++'s was uninitialised until the first read).
    pub fn zeroed() -> DbEvent {
        // SAFETY: integers and a byte array: all bits zero is a value of each
        unsafe { core::mem::zeroed() }
    }
}

/// The C++ side of an events file: the file, the owners and their `AddEvent`.
pub trait EventsHost {
    /// `inFile.Open(kFileAccessRead)`.
    fn open(&mut self);
    /// `inFile.Read(theCount)`.
    fn read_count(&mut self, count: &mut c_ulong) -> c_long;
    /// `inFile.Read(&theData, sizeof theData, theRead)`.
    fn read_record(&mut self, record: &mut DbEvent) -> c_long;
    /// The record's owner: `UIControlsTable::GetUIControlsTable(owner)` or `inManager->GetFormByID(owner)` (null:
    /// none).
    fn owner(&mut self, owner: c_ulong) -> *mut c_void;
    /// `owner->AddEvent({id, type})`.
    fn add(&mut self, owner: *mut c_void, event_id: c_ulong, event_type: c_ulong);
    /// `inFile.Close()`.
    fn close(&mut self);
}

/// `UIEvent::CreateControlEvents` and `CreateFormEvents`.
pub fn create_events(h: &mut impl EventsHost) -> c_long {
    let mut count: c_ulong = 0;
    h.open();
    let mut result = h.read_count(&mut count);
    if result == ERROR_OK {
        let mut record = DbEvent::zeroed();
        for _ in 0..count {
            result = h.read_record(&mut record);
            if record.event_id < 0 {
                continue;
            }
            let owner = h.owner(record.owner_id as c_ulong);
            if owner.is_null() {
                continue;
            }
            h.add(owner, record.event_id as c_ulong, record.event_type as c_ulong);
        }
    }
    h.close();
    result
}

/// An event's initialisation count (`mInitializeCount`, read and written where the C++ read and wrote it) and its
/// virtual `InitializeEvent`/`UninitializeEvent`.
pub trait EventCountHost {
    fn count(&self) -> c_ulong;
    fn set_count(&mut self, count: c_ulong);
    fn initialize_event(&mut self) -> c_long;
    fn uninitialize_event(&mut self) -> c_long;
}

/// `UIEvent::Initialize`: the first user initialises the event; a success counts one user more.
pub fn initialize(h: &mut impl EventCountHost) -> c_long {
    let mut result = ERROR_OK;
    if h.count() == 0 {
        result = h.initialize_event();
    }
    if result == ERROR_OK {
        h.set_count(h.count().wrapping_add(1));
    }
    result
}

/// `UIEvent::Uninitialize`: the last user uninitialises it; a success counts one user less, kept: also from a count
/// already at 0 (it wraps).
pub fn uninitialize(h: &mut impl EventCountHost) -> c_long {
    let mut result = ERROR_OK;
    if h.count() == 1 {
        result = h.uninitialize_event();
    }
    if result == ERROR_OK {
        h.set_count(h.count().wrapping_sub(1));
    }
    result
}

#[cfg(test)]
mod tests;
