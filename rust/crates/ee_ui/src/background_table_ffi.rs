//! `extern "C"` for `UIEvent` (`UIEvent.cpp`): the database files are read through `ee_uicpp_file_*` (UIEvent.cpp),
//! the owners' event lists through `ee_uicpp_event_*` (the background table is `bgtableobj.rs`).
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_int, c_long, c_ulong, c_void};

use crate::event::{self, DbEvent, EventCountHost, EventsHost};

/// cbindgen:ignore
mod sys {
    use core::ffi::{c_int, c_long, c_ulong, c_void};

    extern "C" {
        pub fn ee_uicpp_file_open(file: *mut c_void);
        pub fn ee_uicpp_file_read_count(file: *mut c_void, count: *mut c_ulong) -> c_long;
        pub fn ee_uicpp_file_read(file: *mut c_void, data: *mut u8, length: c_ulong) -> c_long;
        pub fn ee_uicpp_file_close(file: *mut c_void);
        pub fn ee_uicpp_event_owner(kind: c_int, manager: *mut c_void, owner: c_ulong) -> *mut c_void;
        pub fn ee_uicpp_event_add(kind: c_int, owner: *mut c_void, event_id: c_ulong, event_type: c_ulong);
        pub fn ee_uicpp_event_initialize(event: *mut c_void) -> c_long;
        pub fn ee_uicpp_event_uninitialize(event: *mut c_void) -> c_long;
    }
}

/// A database file (`FSFileSpec&`).
struct File(*mut c_void);

impl File {
    fn open(&mut self) {
        unsafe { sys::ee_uicpp_file_open(self.0) }
    }
    fn read_count(&mut self, count: &mut c_ulong) -> c_long {
        unsafe { sys::ee_uicpp_file_read_count(self.0, count) }
    }
    fn read<T>(&mut self, record: &mut T) -> c_long {
        unsafe { sys::ee_uicpp_file_read(self.0, (record as *mut T).cast(), core::mem::size_of::<T>() as c_ulong) }
    }
    fn close(&mut self) {
        unsafe { sys::ee_uicpp_file_close(self.0) }
    }
}

/// An events file and its owners: `kind` 0 control tables (`CreateControlEvents`), 1 the manager's forms
/// (`CreateFormEvents`).
struct Events {
    file: File,
    kind: c_int,
    manager: *mut c_void,
}

impl EventsHost for Events {
    fn open(&mut self) {
        self.file.open()
    }
    fn read_count(&mut self, count: &mut c_ulong) -> c_long {
        self.file.read_count(count)
    }
    fn read_record(&mut self, record: &mut DbEvent) -> c_long {
        self.file.read(record)
    }
    fn owner(&mut self, owner: c_ulong) -> *mut c_void {
        unsafe { sys::ee_uicpp_event_owner(self.kind, self.manager, owner) }
    }
    fn add(&mut self, owner: *mut c_void, event_id: c_ulong, event_type: c_ulong) {
        unsafe { sys::ee_uicpp_event_add(self.kind, owner, event_id, event_type) }
    }
    fn close(&mut self) {
        self.file.close()
    }
}

/// `UIEvent::CreateControlEvents` (`kind` 0, no manager) and `CreateFormEvents` (`kind` 1).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_event_create(file: *mut c_void, kind: c_int, manager: *mut c_void) -> c_long {
    event::create_events(&mut Events { file: File(file), kind, manager })
}

/// An event's count (`mInitializeCount`, read and written through the pointer each time) and virtuals.
struct Count {
    count: *mut c_ulong,
    event: *mut c_void,
}

impl EventCountHost for Count {
    fn count(&self) -> c_ulong {
        unsafe { self.count.read() }
    }
    fn set_count(&mut self, count: c_ulong) {
        unsafe { self.count.write(count) }
    }
    fn initialize_event(&mut self) -> c_long {
        unsafe { sys::ee_uicpp_event_initialize(self.event) }
    }
    fn uninitialize_event(&mut self) -> c_long {
        unsafe { sys::ee_uicpp_event_uninitialize(self.event) }
    }
}

/// `UIEvent::Initialize`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_event_initialize(count: *mut c_ulong, event: *mut c_void) -> c_long {
    event::initialize(&mut Count { count, event })
}

/// `UIEvent::Uninitialize`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_event_uninitialize(count: *mut c_ulong, event: *mut c_void) -> c_long {
    event::uninitialize(&mut Count { count, event })
}
