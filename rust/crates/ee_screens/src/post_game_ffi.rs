//! The post-game summary's host over the C++ calls (`post_game.rs`; `UI Events/EEScreens.cpp`).

use core::ffi::{c_long, c_ulong};
use std::sync::Mutex;

use crate::menus_ffi::Form;
use crate::post_game::{EeScreensPgsHistory, EeScreensPgsRecord, PostGame, PostGameHost};

/// cbindgen:ignore
mod sys {
    use crate::post_game::{EeScreensPgsHistory, EeScreensPgsRecord};

    extern "C" {
        /// `gWorld.mStatistics` into `out`.
        pub fn ee_screenscpp_pgs_history(out: *mut EeScreensPgsHistory);
        /// Record `index` of the statistics into `out`.
        pub fn ee_screenscpp_pgs_record(index: u32, out: *mut EeScreensPgsRecord);
    }
}

static POST_GAME: Mutex<PostGame> = Mutex::new(PostGame::new());

/// One of the summary's events and its result; `None` for another form's.
pub(crate) fn dispatch(host: &mut Form, event: u32) -> Option<c_long> {
    crate::post_game::dispatch(host, &POST_GAME, event)
}

impl PostGameHost for Form {
    fn call(&mut self, op: u32, a: u32, b: u32, c: u32, d: u32) -> u32 {
        self.op(op, a, b, c, d)
    }
    fn text(&mut self, which: u32, arg: u32) -> Option<Vec<u8>> {
        self.get_checked(which, arg)
    }
    fn put_text(&mut self, which: u32, arg: u32, text: &[u8]) -> u32 {
        self.put(which, arg, text)
    }
    fn history(&mut self) -> EeScreensPgsHistory {
        let mut out = EeScreensPgsHistory::default();
        unsafe { sys::ee_screenscpp_pgs_history(&mut out) };
        out
    }
    fn record(&mut self, index: u32) -> EeScreensPgsRecord {
        let mut out = EeScreensPgsRecord::default();
        unsafe { sys::ee_screenscpp_pgs_record(index, &mut out) };
        out
    }
}

/// The post-game summary's event code for a control event id, 0 for another form's.
#[no_mangle]
pub extern "C" fn ee_screens_post_game_event_for(id: c_ulong) -> u32 {
    crate::post_game::event_for_control(id)
}
