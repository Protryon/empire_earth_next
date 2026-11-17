//! The civilization builder's events' host (`civ_builder.rs`): the builder's ops over `civ_whole.rs`, the rest over
//! the screens' C++ calls (`UI Events/EEScreens.cpp`).

use core::ffi::c_long;
use std::sync::Mutex;

use crate::screens::civ_builder::{CivBuilder, CivBuilderHost};
use crate::screens::menus_ffi::Form;

static CIV_BUILDER: Mutex<CivBuilder> = Mutex::new(CivBuilder::new());

/// One of the builder's events and its result; `None` for another form's.
pub(crate) fn dispatch(host: &mut Form, event: u32) -> Option<c_long> {
    crate::screens::civ_builder::dispatch(host, &CIV_BUILDER, event)
}

/// The builder's ops are Rust's (`civ_whole.rs`, over its host); the rest the screens' host's.
impl CivBuilderHost for Form {
    fn call(&mut self, op: u32, a: u32) -> u32 {
        let host = &mut crate::screens::civ_whole_ffi::Host;
        match crate::screens::civ_whole::event_call(host, self.0 as usize, self.1 as usize, op, a) {
            Some(value) => value,
            None => self.op(op, a, 0, 0, 0),
        }
    }
    fn text(&mut self, which: u32) -> Vec<u8> {
        crate::screens::civ_whole::event_text(which).unwrap_or_else(|| self.get(which, 0))
    }
    fn put_text(&mut self, which: u32, text: &[u8]) -> u32 {
        let host = &mut crate::screens::civ_whole_ffi::Host;
        crate::screens::civ_whole::event_put_text(host, which, text).unwrap_or_else(|| self.put(which, 0, text))
    }
}
