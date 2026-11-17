//! The Graphics Engine's math (from ee_ge): the GE value types, the table trigonometry (um), the transforms and the
//! hull.
#![allow(
    clippy::approx_constant,
    clippy::excessive_precision,
    clippy::neg_cmp_op_on_partial_ord,
    clippy::manual_range_contains,
    clippy::assign_op_pattern,
    clippy::needless_range_loop
)]

/// cbindgen:ignore
pub mod bbox;
pub mod ffi;
/// cbindgen:ignore
pub mod hull;
/// cbindgen:ignore
pub mod line;
/// cbindgen:ignore
pub mod point;
/// cbindgen:ignore
pub mod transform;
pub mod types;
/// cbindgen:ignore
pub mod um;
/// cbindgen:ignore
pub mod vector;
