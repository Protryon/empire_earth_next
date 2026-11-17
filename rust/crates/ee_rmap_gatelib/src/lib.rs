//! See `Cargo.toml`: the generator's C API (`ee_rmap::ffi`) reaches the tool through this dependency.

// `c_long`/`c_ulong` are `i32`/`u32` on Windows and `i64`/`u64` on Linux and macOS: a cast or `from` that one data
// model needs is to the same type on the other. The Windows gate keeps both lints; off Windows they would flag
// only those.
#![cfg_attr(not(windows), allow(clippy::unnecessary_cast, clippy::useless_conversion))]

use ee_sim as _;
// the C ABI of the engine math the tool's UMath.cpp and GE units forward to (ee_ge::ffi)
use ee_render as _;
// the file specs and the thread primitives the tool's FSFileSpec.cpp and engine headers forward to
use ee_base as _;
use ee_base as _;
