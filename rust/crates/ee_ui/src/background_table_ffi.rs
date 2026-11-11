//! `extern "C"` for `UIBackgroundTable` (`User Interface/UIBackgroundTable.cpp`) and `UIEvent` (`UIEvent.cpp`): the
//! database files are read through `ee_uicpp_file_*` (UIEvent.cpp), the static array and the owners' event lists
//! through `ee_uicpp_bgtable_*` and `ee_uicpp_event_*`.
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_int, c_long, c_ulong, c_void};

use crate::background_table::{self, BackgroundTableFields, BackgroundTablesHost, DbBackground};
use crate::button::UiColor;
use crate::event::{self, DbEvent, EventCountHost, EventsHost};
use crate::layout::Layout;
use crate::rect::UiRect;

/// cbindgen:ignore
mod sys {
    use core::ffi::{c_int, c_long, c_ulong, c_void};

    use crate::background_table::DbBackground;

    extern "C" {
        pub fn ee_uicpp_file_open(file: *mut c_void);
        pub fn ee_uicpp_file_read_count(file: *mut c_void, count: *mut c_ulong) -> c_long;
        pub fn ee_uicpp_file_read(file: *mut c_void, data: *mut u8, length: c_ulong) -> c_long;
        pub fn ee_uicpp_file_close(file: *mut c_void);
        pub fn ee_uicpp_bgtable_resize(count: c_ulong);
        pub fn ee_uicpp_bgtable_set(index: c_ulong, record: *const DbBackground);
        pub fn ee_uicpp_event_owner(kind: c_int, manager: *mut c_void, owner: c_ulong) -> *mut c_void;
        pub fn ee_uicpp_event_add(kind: c_int, owner: *mut c_void, event_id: c_ulong, event_type: c_ulong);
        pub fn ee_uicpp_event_initialize(event: *mut c_void) -> c_long;
        pub fn ee_uicpp_event_uninitialize(event: *mut c_void) -> c_long;
    }
}

/// One table entry as Rust writes it: its record from `mTileWidthPercent` on, `mLayout`, `mTextureRect`, `mColor`.
#[repr(C)]
pub struct BackgroundTableRefs {
    pub fields: *mut BackgroundTableFields,
    pub layout: *mut Layout,
    pub texture_rect: *mut UiRect,
    pub color: *mut UiColor,
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

impl BackgroundTablesHost for File {
    fn open(&mut self) {
        File::open(self)
    }
    fn read_count(&mut self, count: &mut c_ulong) -> c_long {
        File::read_count(self, count)
    }
    fn resize(&mut self, count: c_ulong) {
        unsafe { sys::ee_uicpp_bgtable_resize(count) }
    }
    fn read_record(&mut self, record: &mut DbBackground) -> c_long {
        self.read(record)
    }
    fn set(&mut self, index: c_ulong, record: Option<&DbBackground>) {
        let p = record.map_or(core::ptr::null(), |r| r as *const DbBackground);
        unsafe { sys::ee_uicpp_bgtable_set(index, p) }
    }
    fn close(&mut self) {
        File::close(self)
    }
}

/// `UIBackgroundTable::Create`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_background_table_create(file: *mut c_void) -> c_long {
    background_table::create(&mut File(file))
}

/// The new table entry's members from a record (`Create`'s copy; the C++ copies the texture file name).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_background_table_from_record(record: *const DbBackground, out: BackgroundTableRefs) {
    let t = background_table::from_record(&*record);
    out.fields.write(t.fields);
    out.layout.write(t.layout);
    out.texture_rect.write(t.texture_rect);
    out.color.write(t.color);
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
