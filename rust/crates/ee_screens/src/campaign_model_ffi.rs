//! The campaign editor's model over the C++ host (`campaign_model.rs`; `UI Events/EEScreens.cpp`'s
//! `ee_screenscpp_ce_campaign`, the edited `EECampaign` and the files). The editor's events reach the model through
//! [`call`], [`text`] and [`put`] (their host answers the model's ops here, before C++).

use std::sync::Mutex;

use crate::campaign_editor::*;
use crate::campaign_model::{CampaignModel, CampaignModelHost};
use crate::menus::EE_SCREENS_TEXT_EDIT;
use crate::menus_ffi::Form;

extern "C" {
    fn ee_screenscpp_ce_campaign(op: u32, a: u32, text: *const u8, length: usize) -> u32;
    fn ee_screenscpp_ce_campaign_text(which: u32, arg: u32, out: *mut u8, capacity: usize) -> usize;
}

/// The model (the original's `gCampaignEditor` but its campaign). No host op re-enters the editor's events, so the
/// lock is held across the model's host calls.
static MODEL: Mutex<CampaignModel> = Mutex::new(CampaignModel {
    campaigns: Vec::new(),
    available: Vec::new(),
    password: Vec::new(),
    is_new: false,
    password_entered: false,
    error: Vec::new(),
    list_ok: false,
});

struct Cpp;

impl CampaignModelHost for Cpp {
    fn call(&mut self, op: u32, a: u32, text: &[u8]) -> u32 {
        unsafe { ee_screenscpp_ce_campaign(op, a, text.as_ptr(), text.len()) }
    }
    fn text(&mut self, which: u32, arg: u32) -> Vec<u8> {
        let mut out = vec![0u8; 256];
        let n = unsafe { ee_screenscpp_ce_campaign_text(which, arg, out.as_mut_ptr(), out.len()) };
        if n > out.len() {
            out.resize(n, 0);
            unsafe { ee_screenscpp_ce_campaign_text(which, arg, out.as_mut_ptr(), out.len()) };
        }
        out.truncate(n);
        out
    }
}

fn model() -> std::sync::MutexGuard<'static, CampaignModel> {
    MODEL.lock().unwrap_or_else(|e| e.into_inner())
}

/// The model's answer to an editor call (`EE_SCREENS_GAME_CE_*`); `None` for the toolkit's and the other calls.
/// `EE_SCREENS_GAME_CE_CHANGED` (control, field): the edit box's text against the model's field.
pub(crate) fn call(form: &mut Form, op: u32, a: u32, b: u32) -> Option<u32> {
    if op == EE_SCREENS_GAME_CE_CHANGED {
        let edit = form.get(EE_SCREENS_TEXT_EDIT, a);
        let field = text(EE_SCREENS_TEXT_CE_FIELD, b).unwrap_or_default();
        return Some((edit != field) as u32);
    }
    model().op(&mut Cpp, op)
}

/// The model's text (`EE_SCREENS_TEXT_CE_FIELD`, `_LIST`, `_ERROR`); `None` for another.
pub(crate) fn text(which: u32, arg: u32) -> Option<Vec<u8>> {
    model().text(&mut Cpp, which, arg)
}

/// The model's put (`EE_SCREENS_TEXT_CE_SET_FIELD` .. `_LOAD`); `None` for another.
pub(crate) fn put(which: u32, arg: u32, text: &[u8]) -> Option<u32> {
    model().put(&mut Cpp, which, arg, text)
}

/// The model's ops for a caller without the events (the golden driver): `kind` 0 a call (`which` the op, `a` its
/// argument), 1 a text (written to `out` up to `capacity`; its length), 2 a put; `usize::MAX` for an op it does
/// not own.
///
/// # Safety
/// `text` holds `length` bytes, `out` `capacity`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_campaign_model(
    kind: u32,
    which: u32,
    a: u32,
    text_in: *const u8,
    length: usize,
    out: *mut u8,
    capacity: usize,
) -> usize {
    let input = if length == 0 { &[][..] } else { std::slice::from_raw_parts(text_in, length) };
    match kind {
        0 => model().op(&mut Cpp, which).map_or(usize::MAX, |r| r as usize),
        1 => match text(which, a) {
            Some(t) => {
                std::ptr::copy_nonoverlapping(t.as_ptr(), out, t.len().min(capacity));
                t.len()
            }
            None => usize::MAX,
        },
        _ => put(which, a, input).map_or(usize::MAX, |r| r as usize),
    }
}
