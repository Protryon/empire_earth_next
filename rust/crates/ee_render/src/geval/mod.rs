//! The Graphics Engine's value types whose members are inline C++ over this C ABI (lle/GERust.h; LLE round 8): the
//! field stream over a C++ `FSFile` (`io.rs`), `GETransformation`'s copies, comparisons and file codec and the value
//! types' float codecs (`values.rs`), `GEViewport` (`viewport.rs`, `viewport_ffi.rs`) and the vertex and polygon
//! arrays (`vertarr.rs`; TEST ONLY, the store gate's capture of them, `vertarr_capture.rs`). A module of its own so that its header (rust/render/geval.h) is small and names only
//! ee_base's math: every C++ unit includes it. `ge` re-exports the modules under their old paths.

// the C++'s shapes are kept on purpose (as in `ge`): its comparisons, its operand order, its index loops
#![allow(
    clippy::approx_constant,
    clippy::excessive_precision,
    clippy::neg_cmp_op_on_partial_ord,
    clippy::manual_range_contains,
    clippy::assign_op_pattern,
    clippy::needless_range_loop
)]

pub mod io;
pub mod values;
pub mod vertarr;
pub mod vertarr_capture;
pub mod viewport;
pub mod viewport_ffi;
