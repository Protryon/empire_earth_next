//! The fixture's C ABI (TEST ONLY): EETestFixture.cpp's calls in.

use std::ffi::{c_char, CStr};

use super::host::{self, EeFixtureHostFn, EeFixtureSink};

/// The test switches (EETestFixtureConfigure, first in EERun): registers the host, then reads EE_TEST_UI and the
/// switches.
#[no_mangle]
pub extern "C" fn ee_fixture_configure(host_fn: EeFixtureHostFn) {
    host::set_host(host_fn);
    super::poll::configure();
}

/// The fixture's start (EETestFixtureStart, after the engine is initialised).
#[no_mangle]
pub extern "C" fn ee_fixture_start() {
    super::poll::start();
}

/// A text answer of a host call.
///
/// # Safety
/// `sink` is the call's sink; `text` is NUL ended or NULL.
#[no_mangle]
pub unsafe extern "C" fn ee_fixture_sink_text(sink: *mut EeFixtureSink, text: *const c_char) {
    if sink.is_null() || text.is_null() {
        return;
    }
    // SAFETY: the contract
    unsafe { (*sink).texts.push(CStr::from_ptr(text).to_bytes().to_vec()) };
}

/// An id (or handle) answer of a host call.
///
/// # Safety
/// `sink` is the call's sink.
#[no_mangle]
pub unsafe extern "C" fn ee_fixture_sink_id(sink: *mut EeFixtureSink, id: i64) {
    if !sink.is_null() {
        // SAFETY: the contract
        unsafe { (*sink).ids.push(id) };
    }
}

/// An input message before (`phase` 0) and after (1) it executes (EEPhysics::DrainInput, the world's write lock
/// held); `local`: the local player sent it.
#[no_mangle]
pub extern "C" fn ee_fixture_message(message: u64, kind: u64, local: bool, phase: i32) {
    super::orders::message(message, kind, local, phase);
}

/// The WON lobby's work on its own message loop (EETestFixture.cpp's UserEvent; lobby.rs).
#[no_mangle]
pub extern "C" fn ee_fixture_lobby_work(_param: std::ffi::c_int) {
    super::lobby::work();
}
