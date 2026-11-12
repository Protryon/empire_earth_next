//! The chat log (the chat log form's last 40 lines, saved with the game) whole: the original `EEUIEChatLogEvents.cpp`
//! (`EEUIECLEFormManager`: `AddEntry`, `Clear`, `GetEntryCount`, `Load`, `Save`) as a Rust record. The entries live
//! here; a saved game's reads and writes go through a host ([`ChatLogFile`]) whose calls are each one `FSFile` read
//! or write (`UWideString::Load`/`Save` and `GEColor::Load`/`Save` for the names, texts and colours).
//!
//! The original kept the entries in a form manager it attached to the chat log form on first use; nothing else read
//! that manager, so there is none now.

use core::ffi::c_long;

/// A chat log line (`EEChatLogEntry`).
#[derive(Clone, Debug, Default, PartialEq)]
pub struct ChatLogEntry {
    pub colour: [f32; 3],
    /// The sender (empty: a game message).
    pub name: Vec<u8>,
    pub text: Vec<u8>,
    /// The line's kind (2: not logged; the caller filters).
    pub kind: u32,
    pub time: u32,
}

/// The chat log.
#[derive(Debug, Default)]
pub struct ChatLog {
    pub entries: std::collections::VecDeque<ChatLogEntry>,
}

/// The most entries kept.
const CL_KEPT: usize = 40;

impl ChatLog {
    pub const fn new() -> Self {
        ChatLog { entries: std::collections::VecDeque::new() }
    }
    /// `AddEntry`: the entry appended, the oldest beyond 40 dropped (the original's loop recomputes the size as it
    /// pops, so it drops at most half the excess; one entry at a time it never matters).
    pub fn add(&mut self, entry: ChatLogEntry) {
        self.entries.push_back(entry);
        if self.entries.len() > CL_KEPT {
            let mut i = 0;
            while i < self.entries.len() - CL_KEPT {
                self.entries.pop_front();
                i += 1;
            }
        }
    }
}

/// The saved game's file: each call one read or write, with its result (0 for `kErrorOK`).
pub trait ChatLogFile {
    fn read_u32(&mut self) -> (c_long, u32);
    /// `UWideString::Load`: the result and the string as loaded (the original's string after a failed load).
    fn read_string(&mut self) -> (c_long, Vec<u8>);
    fn read_colour(&mut self) -> (c_long, [f32; 3]);
    fn write_u32(&mut self, value: u32) -> c_long;
    fn write_string(&mut self, text: &[u8]) -> c_long;
    fn write_colour(&mut self, colour: [f32; 3]) -> c_long;
}

/// `Load`: the log emptied, then the saved entries (type, name, colour, text, time) added up to the first read
/// error, which is returned.
pub fn load<F: ChatLogFile + ?Sized>(log: &std::sync::Mutex<ChatLog>, file: &mut F) -> c_long {
    log.lock().unwrap().entries.clear();
    let (mut result, count) = file.read_u32();
    if result != 0 {
        return result;
    }
    // (the original's one entry, reused from line to line)
    let mut entry = ChatLogEntry { colour: [1.0; 3], ..Default::default() };
    let mut i = 0;
    while i < count && result == 0 {
        i += 1;
        let (r, kind) = file.read_u32();
        result = r;
        if result != 0 {
            continue;
        }
        entry.kind = kind;
        let (r, name) = file.read_string();
        result = r;
        entry.name = name;
        if result != 0 {
            continue;
        }
        let (r, colour) = file.read_colour();
        result = r;
        entry.colour = colour;
        if result != 0 {
            continue;
        }
        let (r, text) = file.read_string();
        result = r;
        entry.text = text;
        if result != 0 {
            continue;
        }
        let (r, time) = file.read_u32();
        result = r;
        if result == 0 {
            entry.time = time;
            log.lock().unwrap().add(entry.clone());
        }
    }
    result
}

/// `Save`: the count, then each entry's type, name, colour, text and time, up to the first write error, which is
/// returned.
pub fn save<F: ChatLogFile + ?Sized>(log: &std::sync::Mutex<ChatLog>, file: &mut F) -> c_long {
    let entries: Vec<ChatLogEntry> = log.lock().unwrap().entries.iter().cloned().collect();
    let mut result = file.write_u32(entries.len() as u32);
    if result != 0 {
        return result;
    }
    for entry in &entries {
        if result != 0 {
            break;
        }
        result = file.write_u32(entry.kind);
        if result != 0 {
            continue;
        }
        result = file.write_string(&entry.name);
        if result != 0 {
            continue;
        }
        result = file.write_colour(entry.colour);
        if result != 0 {
            continue;
        }
        result = file.write_string(&entry.text);
        if result == 0 {
            result = file.write_u32(entry.time);
        }
    }
    result
}

#[cfg(test)]
mod golden;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn keeps_forty() {
        let mut log = ChatLog::new();
        for t in 0..45 {
            log.add(ChatLogEntry { time: t, ..Default::default() });
        }
        assert_eq!(log.entries.len(), 40);
        assert_eq!(log.entries[0].time, 5);
    }
}
