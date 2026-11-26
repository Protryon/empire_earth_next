//! The computer players' decision trace line (was EECPTrace.cpp's EECPTraceOut and its `gCPTraceManager`, the AI
//! Thread cluster's round 8):
//!
//! `EE-AI <game ms> p<player> <manager> <event> <text> | f=<food> w=<wood> s=<stone> g=<gold> i=<iron>`
//!
//! to the debug output and, when a game log is open, the log. The manager is the one running now (a scope sets it
//! for one manager's call and puts the previous one back); the resources are the player's `mResourceCaps`. The
//! C++'s buffers are kept: the text is cut at 639 bytes, the line at 799.

#![allow(clippy::unnecessary_cast)] // c_long is i32 on Windows, i64 on LP64

use std::ffi::{c_char, c_void, CStr};
use std::sync::atomic::{AtomicPtr, Ordering};

use crate::world::view::{PlayerView, WorldRead, WorldView};

/// The manager running now (`gCPTraceManager`; "-" outside every scope).
static MANAGER: AtomicPtr<c_char> = AtomicPtr::new(c"-".as_ptr() as *mut c_char);

/// Sets the manager running now -> the previous one (for [`scope_end`]).
pub fn scope(name: *const c_char) -> *const c_char {
    MANAGER.swap(name as *mut c_char, Ordering::AcqRel)
}

/// Puts back the manager [`scope`] returned.
pub fn scope_end(saved: *const c_char) {
    MANAGER.store(saved as *mut c_char, Ordering::Release);
}

fn manager() -> Vec<u8> {
    // SAFETY: every manager name is a static C string
    unsafe { CStr::from_ptr(MANAGER.load(Ordering::Acquire)) }.to_bytes().to_vec()
}

/// The line (the C++'s `vsnprintf` into 640 bytes, then `snprintf` into 800).
pub fn line(time: u32, player: u32, manager: &[u8], event: &[u8], text: &[u8], caps: [i32; 5]) -> Vec<u8> {
    let text = &text[..text.len().min(639)];
    let mut l = format!("EE-AI {time} p{player} ").into_bytes();
    for part in [manager, b" ", event, b" ", text] {
        l.extend_from_slice(part);
    }
    l.extend_from_slice(
        format!(" | f={} w={} s={} g={} i={}\n", caps[0], caps[1], caps[2], caps[3], caps[4]).as_bytes(),
    );
    l.truncate(799);
    l
}

/// `EECPTraceOut(player, event, "%s", text)`: the line of the computer player `player` (its `EEPlayer*`).
pub fn trace_out(player: *mut c_void, event: &[u8], text: &[u8]) {
    let t = unsafe { WorldRead::begin() };
    let time = WorldView::global(&t).map_or(0, |w| w.game_time()) as u32;
    // SAFETY: the caller's live computer player
    let Some(p) = (unsafe { PlayerView::of(&t, player as u64) }) else { return };
    let all: [std::ffi::c_long; 7] = crate::world_field!(p.saved(), mResourceCaps);
    let caps = [all[2] as i32, all[3] as i32, all[4] as i32, all[5] as i32, all[6] as i32];
    let id = p.id();
    drop(t);
    let mut l = line(time, id, &manager(), event, text, caps);
    // the C++'s "%s" of the line stops at a NUL (the callers' texts have none)
    if let Some(end) = l.iter().position(|&c| c == 0) {
        l.truncate(end);
    }
    let c = std::ffi::CString::new(l).unwrap_or_default();
    #[cfg(not(test))]
    super::next_game::debug_and_log(player, &c);
    #[cfg(test)]
    let _ = c;
}

/// `EECPTraceOut` for the C++ (its `printf` already done into `text`).
///
/// # Safety
/// `player` is a live computer player's `EEPlayer*`; `event` and `text` NUL-terminated strings.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_trace_out(player: *mut c_void, event: *const c_char, text: *const c_char) {
    // SAFETY: the caller's contract
    let (e, x) = unsafe { (CStr::from_ptr(event), CStr::from_ptr(text)) };
    trace_out(player, e.to_bytes(), x.to_bytes());
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_line_is_the_cpp_format() {
        let l = line(4000, 2, b"next", b"start", b"a=1", [10, 20, 0, -5, 7]);
        assert_eq!(l, b"EE-AI 4000 p2 next start a=1 | f=10 w=20 s=0 g=-5 i=7\n".to_vec());
    }

    #[test]
    fn the_text_and_the_line_keep_the_cpp_buffers() {
        let text = vec![b'x'; 700];
        let l = line(1, 1, b"-", b"e", &text, [0; 5]);
        assert_eq!(l, line(1, 1, b"-", b"e", &text[..639], [0; 5]));
        assert_eq!(l.len(), 15 + 639 + 23);
        let long = line(1, 1, &[b'm'; 300], b"e", &text, [0; 5]);
        assert_eq!(long.len(), 799);
    }

    #[test]
    fn a_scope_puts_the_manager_back() {
        let saved = scope(c"econ".as_ptr());
        assert_eq!(manager(), b"econ");
        scope_end(saved);
        assert_eq!(manager(), b"-");
    }
}
