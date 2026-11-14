//! `extern "C"` for the main game form's per-frame work (`main_form.rs`). The host is `ee_screenscpp_main_form`
//! (`User Interface Thread/EEUIFMainGame.cpp`): `call` is its record of one call (the form, the picked unit, the
//! held text, the chat line's colour, name, text and flags, the dialog message).

use core::ffi::c_void;

use crate::main_form::{
    self, EeScreensMainFormIo, FormHost, EE_SCREENS_MF_OP_PREVIEW_WALL, EE_SCREENS_MF_OP_TEXT_GET,
    EE_SCREENS_MF_OP_TEXT_SET,
};

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    use crate::main_form::EeScreensMainFormIo;

    extern "C" {
        pub fn ee_screenscpp_main_form(call: *mut c_void, op: u32, a: i64, b: i64, io: *mut EeScreensMainFormIo)
            -> i64;
    }
}

struct Host(*mut c_void);

impl FormHost for Host {
    fn op(&mut self, op: u32, a: i64, b: i64, io: &mut EeScreensMainFormIo) -> i64 {
        unsafe { sys::ee_screenscpp_main_form(self.0, op, a, b, io) }
    }
    fn set_text(&mut self, text: &[u8]) {
        let mut io = EeScreensMainFormIo::default();
        self.op(EE_SCREENS_MF_OP_TEXT_SET, text.len() as i64, text.as_ptr() as i64, &mut io);
    }
    fn text(&mut self) -> Vec<u8> {
        let mut io = EeScreensMainFormIo::default();
        let len = self.op(EE_SCREENS_MF_OP_TEXT_GET, 0, 0, &mut io).max(0) as usize;
        let mut out = vec![0u8; len];
        self.op(EE_SCREENS_MF_OP_TEXT_GET, len as i64, out.as_mut_ptr() as i64, &mut io);
        out
    }
    fn preview_wall(&mut self, tiles: &[[i32; 2]]) {
        let mut io = EeScreensMainFormIo::default();
        self.op(EE_SCREENS_MF_OP_PREVIEW_WALL, tiles.len() as i64, tiles.as_ptr() as i64, &mut io);
    }
}

/// `EEUIFMainGame::CopyData` over the host's call record: `UIForm::CopyData`'s result.
#[no_mangle]
pub extern "C" fn ee_screens_main_form_copy_data(call: *mut c_void) -> i64 {
    main_form::copy_data(&mut Host(call))
}

/// `EEUIFMainGame::RefreshStatus` over the host's call record.
#[no_mangle]
pub extern "C" fn ee_screens_main_form_refresh_status(call: *mut c_void) {
    main_form::refresh_status(&mut Host(call));
}

/// `EEUIFMainGame::DisplayChatLine` of the host's call record's colour, name, text and flags.
#[no_mangle]
pub extern "C" fn ee_screens_main_form_chat_line(call: *mut c_void) {
    main_form::display_chat_line(&mut Host(call));
}

/// `EEUIFMainGame::ShowInformation` of the host's call record's dialog message.
#[no_mangle]
pub extern "C" fn ee_screens_main_form_show_information(call: *mut c_void) {
    main_form::show_information(&mut Host(call));
}
