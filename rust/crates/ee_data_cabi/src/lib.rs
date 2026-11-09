//! The C ABI of `ee_data` as a small static library for the C++ gate tool (tools/eedata/cppgate/eedata_gate.cpp).
//! Everything is in `ee_data::ffi`; this crate only keeps its `#[no_mangle]` exports in the link.

use ee_data as _;
