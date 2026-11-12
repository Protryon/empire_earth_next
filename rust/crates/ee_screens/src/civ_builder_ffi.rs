//! The civilization builder's events' host over the C++ calls (`civ_builder.rs`; `UI Events/EEScreens.cpp`).

use core::ffi::c_long;
use std::sync::Mutex;

use crate::civ_builder::{CivBuilder, CivBuilderHost};
use crate::menus_ffi::Form;

static CIV_BUILDER: Mutex<CivBuilder> = Mutex::new(CivBuilder::new());

/// One of the builder's events and its result; `None` for another form's.
pub(crate) fn dispatch(host: &mut Form, event: u32) -> Option<c_long> {
    crate::civ_builder::dispatch(host, &CIV_BUILDER, event)
}

impl CivBuilderHost for Form {
    fn call(&mut self, op: u32, a: u32) -> u32 {
        self.op(op, a, 0, 0, 0)
    }
    fn text(&mut self, which: u32) -> Vec<u8> {
        self.get(which, 0)
    }
    fn put_text(&mut self, which: u32, text: &[u8]) -> u32 {
        self.put(which, 0, text)
    }
}
