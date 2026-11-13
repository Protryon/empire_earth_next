//! The chat log's C interface (`chat_log.rs`): the entries for the UI thread, the chat log form's host and the saved
//! games, and the saved game's file through the C++ host (`UI Events/EEScreens.cpp`).
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_long, c_void};
use std::sync::Mutex;

use crate::chat_log::{ChatLog, ChatLogEntry, ChatLogFile};

/// A chat log entry's fixed fields (its name and text go as bytes beside it).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScreensChatLogHead {
    pub colour: [f32; 3],
    pub kind: u32,
    pub time: u32,
}

/// cbindgen:ignore
mod sys {
    use core::ffi::{c_long, c_void};

    extern "C" {
        /// One read from the saved game `file`: kind 0 a 32-bit value (into `value`), 1 a string, 2 a colour (kept by
        /// the host for `ee_screenscpp_cl_last`); its result.
        pub fn ee_screenscpp_cl_read(file: *mut c_void, kind: u32, value: *mut u32) -> c_long;
        /// The last read's string or colour (12 bytes) into `out`; its length in bytes.
        pub fn ee_screenscpp_cl_last(kind: u32, out: *mut u8, capacity: usize) -> usize;
        /// One write: kind 0 `value`, 1 the string `bytes`, 2 the colour `bytes` (12 bytes); its result.
        pub fn ee_screenscpp_cl_write(
            file: *mut c_void,
            kind: u32,
            value: u32,
            bytes: *const u8,
            length: usize,
        ) -> c_long;
    }
}

static CHAT_LOG: Mutex<ChatLog> = Mutex::new(ChatLog::new());

unsafe fn bytes<'a>(text: *const u8, length: usize) -> &'a [u8] {
    if text.is_null() || length == 0 {
        &[]
    } else {
        core::slice::from_raw_parts(text, length)
    }
}

/// `EEUIECLEFormManager::AddEntry`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_chat_log_add(
    head: *const EeScreensChatLogHead,
    name: *const u8,
    name_length: usize,
    text: *const u8,
    text_length: usize,
) {
    let head = *head;
    let entry = ChatLogEntry {
        colour: head.colour,
        name: bytes(name, name_length).to_vec(),
        text: bytes(text, text_length).to_vec(),
        kind: head.kind,
        time: head.time,
    };
    CHAT_LOG.lock().unwrap().add(entry);
}

/// `EEUIECLEFormManager::Clear`.
#[no_mangle]
pub extern "C" fn ee_screens_chat_log_clear() {
    CHAT_LOG.lock().unwrap().entries.clear();
}

/// `EEUIECLEFormManager::GetEntryCount`.
#[no_mangle]
pub extern "C" fn ee_screens_chat_log_count() -> u32 {
    CHAT_LOG.lock().unwrap().entries.len() as u32
}

/// Entry `index`'s fixed fields into `out`; false (nothing written) past the last.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_chat_log_entry(index: u32, out: *mut EeScreensChatLogHead) -> bool {
    let log = CHAT_LOG.lock().unwrap();
    let Some(e) = log.entries.get(index as usize) else { return false };
    *out = EeScreensChatLogHead { colour: e.colour, kind: e.kind, time: e.time };
    true
}

/// Entry `index`'s name (`which` 0) or text (1) into `out` (up to `capacity` bytes); its length in bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_chat_log_text(index: u32, which: u32, out: *mut u8, capacity: usize) -> usize {
    let log = CHAT_LOG.lock().unwrap();
    let Some(e) = log.entries.get(index as usize) else { return 0 };
    let text = if which == 0 { &e.name } else { &e.text };
    core::ptr::copy_nonoverlapping(text.as_ptr(), out, text.len().min(capacity));
    text.len()
}

struct HostFile(*mut c_void);

impl HostFile {
    fn last(&mut self, kind: u32) -> Vec<u8> {
        let mut out = vec![0u8; 256];
        let n = unsafe { sys::ee_screenscpp_cl_last(kind, out.as_mut_ptr(), out.len()) };
        if n > out.len() {
            out.resize(n, 0);
            unsafe { sys::ee_screenscpp_cl_last(kind, out.as_mut_ptr(), out.len()) };
        }
        out.truncate(n);
        out
    }
}

impl ChatLogFile for HostFile {
    fn read_u32(&mut self) -> (c_long, u32) {
        let mut value = 0;
        let result = unsafe { sys::ee_screenscpp_cl_read(self.0, 0, &mut value) };
        (result, value)
    }
    fn read_string(&mut self) -> (c_long, Vec<u8>) {
        let result = unsafe { sys::ee_screenscpp_cl_read(self.0, 1, core::ptr::null_mut()) };
        (result, self.last(1))
    }
    fn read_colour(&mut self) -> (c_long, [f32; 3]) {
        let result = unsafe { sys::ee_screenscpp_cl_read(self.0, 2, core::ptr::null_mut()) };
        let b = self.last(2);
        let f = |k: usize| f32::from_le_bytes([b[4 * k], b[4 * k + 1], b[4 * k + 2], b[4 * k + 3]]);
        (result, [f(0), f(1), f(2)])
    }
    fn write_u32(&mut self, value: u32) -> c_long {
        unsafe { sys::ee_screenscpp_cl_write(self.0, 0, value, core::ptr::null(), 0) }
    }
    fn write_string(&mut self, text: &[u8]) -> c_long {
        unsafe { sys::ee_screenscpp_cl_write(self.0, 1, 0, text.as_ptr(), text.len()) }
    }
    fn write_colour(&mut self, colour: [f32; 3]) -> c_long {
        let mut b = [0u8; 12];
        for (k, c) in colour.iter().enumerate() {
            b[4 * k..4 * k + 4].copy_from_slice(&c.to_le_bytes());
        }
        unsafe { sys::ee_screenscpp_cl_write(self.0, 2, 0, b.as_ptr(), b.len()) }
    }
}

/// `EEUIECLEFormManager::Load` from the saved game `file` (an `FSFile*`).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_chat_log_load(file: *mut c_void) -> c_long {
    crate::chat_log::load(&CHAT_LOG, &mut HostFile(file))
}

/// `EEUIECLEFormManager::Save` to the saved game `file` (an `FSFile*`).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_chat_log_save(file: *mut c_void) -> c_long {
    crate::chat_log::save(&CHAT_LOG, &mut HostFile(file))
}
