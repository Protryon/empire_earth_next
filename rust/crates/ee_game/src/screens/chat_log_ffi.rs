//! The chat log's C interface (`chat_log.rs`): the entries for the UI thread, the chat log form's host and the saved
//! games, and the saved game's file through the C++ host (`UI Events/EEScreens.cpp`).
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_long, c_void};
use std::sync::Mutex;

use crate::screens::chat_log::{ChatLog, ChatLogEntry, ChatLogFile};
use crate::screens::game_cpp::{
    Cpp, EeScreensShimOut as Out, EeScreensShimText as Text, EeScreensShimULongOut as ULongOut, Ffi,
};

/// A chat log entry's fixed fields (its name and text go as bytes beside it).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScreensChatLogHead {
    pub colour: [f32; 3],
    pub kind: u32,
    pub time: u32,
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

/// Entry `index`'s sender (`which` 0) or text (1); empty past the last.
pub(crate) fn text(index: u32, which: u32) -> Vec<u8> {
    let log = CHAT_LOG.lock().unwrap();
    let Some(e) = log.entries.get(index as usize) else { return Vec::new() };
    if which == 0 {
        e.name.clone()
    } else {
        e.text.clone()
    }
}

struct HostFile(*mut c_void);

// the saved game's file through the screens' shims (EEScreens.cpp: the string and colour are its statics)
impl ChatLogFile for HostFile {
    fn read_u32(&mut self) -> (c_long, u32) {
        let mut value: core::ffi::c_ulong = 0;
        let result = Ffi.cl_read_ulong(self.0 as usize, ULongOut::of(&mut value));
        (result as c_long, value as u32)
    }
    fn read_string(&mut self) -> (c_long, Vec<u8>) {
        let result = Ffi.cl_read_string(self.0 as usize);
        let mut text = Vec::new();
        Ffi.cl_string(Out::of(&mut text));
        (result as c_long, text)
    }
    fn read_colour(&mut self) -> (c_long, [f32; 3]) {
        let result = Ffi.cl_read_colour(self.0 as usize);
        (result as c_long, [Ffi.cl_red(), Ffi.cl_green(), Ffi.cl_blue()])
    }
    fn write_u32(&mut self, value: u32) -> c_long {
        Ffi.cl_write_ulong(self.0 as usize, value) as c_long
    }
    fn write_string(&mut self, text: &[u8]) -> c_long {
        Ffi.cl_write_string(self.0 as usize, Text::of(text)) as c_long
    }
    fn write_colour(&mut self, colour: [f32; 3]) -> c_long {
        Ffi.cl_set_red(colour[0]);
        Ffi.cl_set_green(colour[1]);
        Ffi.cl_set_blue(colour[2]);
        Ffi.cl_write_colour(self.0 as usize) as c_long
    }
}

/// `EEUIECLEFormManager::Load` from the saved game `file` (an `FSFile*`).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_chat_log_load(file: *mut c_void) -> c_long {
    crate::screens::chat_log::load(&CHAT_LOG, &mut HostFile(file))
}

/// `EEUIECLEFormManager::Save` to the saved game `file` (an `FSFile*`).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_chat_log_save(file: *mut c_void) -> c_long {
    crate::screens::chat_log::save(&CHAT_LOG, &mut HostFile(file))
}
