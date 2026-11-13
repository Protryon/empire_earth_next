//! The campaign editor's events' host over the C++ calls (`campaign_editor.rs`; `UI Events/EEScreens.cpp`).

use core::ffi::{c_long, c_ulong};

use crate::campaign_editor::{CampaignEditorHost, EE_SCREENS_CE_SAVE_CONTINUED};
use crate::menus_ffi::Form;
use crate::scenario_screens_ffi::ee_screenscpp_form;

/// One of the editor's events and its result; `None` for another form's. The posted save runs on form 24 (the
/// original's event held that form; a posted event gets none).
pub(crate) fn dispatch(host: &mut Form, event: u32) -> Option<c_long> {
    if event == EE_SCREENS_CE_SAVE_CONTINUED {
        let mut editor = Form(unsafe { ee_screenscpp_form(24) }, host.1);
        return crate::campaign_editor::dispatch(&mut editor, event);
    }
    crate::campaign_editor::dispatch(host, event)
}

impl CampaignEditorHost for Form {
    fn call(&mut self, op: u32, a: u32, b: u32, c: u32) -> u32 {
        self.op(op, a, b, c, 0)
    }
    fn text(&mut self, which: u32, arg: u32) -> Vec<u8> {
        self.get(which, arg)
    }
    fn put_text(&mut self, which: u32, arg: u32, text: &[u8]) -> u32 {
        self.put(which, arg, text)
    }
}

/// The campaign editor's event code for a control event id, 0 for another form's.
#[no_mangle]
pub extern "C" fn ee_screens_campaign_editor_event_for(id: c_ulong) -> u32 {
    crate::campaign_editor::event_for_control(id)
}
