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

/// A chat line posted to the UI thread (message 0x418; the dialog log's line): the original's `EEChatLogEntry`
/// (UI round 16), its name and text NUL-terminated (`UWideString`'s bytes).
#[repr(C)]
pub struct EeScreensChatEntry {
    pub head: EeScreensChatLogHead,
    pub name: *mut core::ffi::c_char,
    pub text: *mut core::ffi::c_char,
}

fn c_text(text: *const core::ffi::c_char) -> *mut core::ffi::c_char {
    let bytes = if text.is_null() {
        Vec::new()
    } else {
        // SAFETY: the caller's NUL-terminated text
        unsafe { core::ffi::CStr::from_ptr(text) }.to_bytes().to_vec()
    };
    std::ffi::CString::new(bytes).unwrap_or_default().into_raw()
}

fn entry<'a>(entry: usize) -> &'a mut EeScreensChatEntry {
    // SAFETY: every caller's entry came from ee_screens_chat_entry_new / _blank and is not freed
    unsafe { &mut *(entry as *mut EeScreensChatEntry) }
}

/// `new EEChatLogEntry{colour, name, text, kind, time}` (`EEUserInterface::AddChatLogEntry`) -> the entry.
#[no_mangle]
pub extern "C" fn ee_screens_chat_entry_new(
    head: EeScreensChatLogHead,
    name: *const core::ffi::c_char,
    text: *const core::ffi::c_char,
) -> *mut EeScreensChatEntry {
    Box::into_raw(Box::new(EeScreensChatEntry { head, name: c_text(name), text: c_text(text) }))
}

/// `new EEChatLogEntry` (the dialog log's: white, no name or text, kind and time 0; the original left the kind and
/// time unset) -> the entry.
#[no_mangle]
pub extern "C" fn ee_screens_chat_entry_blank() -> usize {
    let head = EeScreensChatLogHead { colour: [1.0; 3], kind: 0, time: 0 };
    ee_screens_chat_entry_new(head, core::ptr::null(), core::ptr::null()) as usize
}

/// The entry's text set from NUL-terminated bytes.
#[no_mangle]
pub extern "C" fn ee_screens_chat_entry_set_text(e: usize, text: *const core::ffi::c_char) {
    let e = entry(e);
    // SAFETY: made by c_text
    drop(unsafe { std::ffi::CString::from_raw(e.text) });
    e.text = c_text(text);
}

/// The entry's kind (`mType`).
#[no_mangle]
pub extern "C" fn ee_screens_chat_entry_set_kind(e: usize, kind: u32) {
    entry(e).head.kind = kind;
}

/// The entry's kind (`mType`).
#[no_mangle]
pub extern "C" fn ee_screens_chat_entry_kind(e: usize) -> i64 {
    entry(e).head.kind as i64
}

/// The entry added to the chat log (the original's `EEUIECLEFormManager::AddEntry`).
#[no_mangle]
pub extern "C" fn ee_screens_chat_entry_log(e: usize) {
    let e = entry(e);
    // SAFETY: made by c_text
    let (name, text) = unsafe { (core::ffi::CStr::from_ptr(e.name), core::ffi::CStr::from_ptr(e.text)) };
    let line = ChatLogEntry {
        colour: e.head.colour,
        name: name.to_bytes().to_vec(),
        text: text.to_bytes().to_vec(),
        kind: e.head.kind,
        time: e.head.time,
    };
    CHAT_LOG.lock().unwrap().add(line);
}

/// The entry freed.
#[no_mangle]
pub extern "C" fn ee_screens_chat_entry_free(e: usize) {
    if e == 0 {
        return;
    }
    // SAFETY: made by ee_screens_chat_entry_new, not used again
    let e = unsafe { Box::from_raw(e as *mut EeScreensChatEntry) };
    // SAFETY: made by c_text
    unsafe {
        drop(std::ffi::CString::from_raw(e.name));
        drop(std::ffi::CString::from_raw(e.text));
    }
}

/// The dialog log's add (the shim list's second name for [`ee_screens_chat_entry_log`]).
#[no_mangle]
pub extern "C" fn ee_screens_chat_entry_add(e: usize) {
    ee_screens_chat_entry_log(e);
}

/// The dialog log's delete (the shim list's second name for [`ee_screens_chat_entry_free`]).
#[no_mangle]
pub extern "C" fn ee_screens_chat_entry_delete(e: usize) {
    ee_screens_chat_entry_free(e);
}

#[cfg(test)]
mod entry_tests {
    use super::*;

    #[test]
    fn a_posted_entry_logs_its_fields_and_frees() {
        let head = EeScreensChatLogHead { colour: [0.5, 0.25, 1.0], kind: 3, time: 7 };
        let e = ee_screens_chat_entry_new(head, c"Ann".as_ptr(), c"hello".as_ptr()) as usize;
        assert_eq!(ee_screens_chat_entry_kind(e), 3);
        ee_screens_chat_entry_set_kind(e, 1);
        ee_screens_chat_entry_set_text(e, c"bye".as_ptr());
        let before = 0;
        ee_screens_chat_entry_log(e);
        let log = CHAT_LOG.lock().unwrap();
        let last = log.entries.back().unwrap();
        let _ = before;
        assert_eq!(
            (last.name.as_slice(), last.text.as_slice(), last.kind, last.time),
            (&b"Ann"[..], &b"bye"[..], 1, 7)
        );
        drop(log);
        ee_screens_chat_entry_free(e);
    }

    #[test]
    fn a_blank_entry_is_white_and_empty() {
        let e = ee_screens_chat_entry_blank();
        assert_eq!(entry(e).head, EeScreensChatLogHead { colour: [1.0; 3], kind: 0, time: 0 });
        ee_screens_chat_entry_free(e);
    }
}
