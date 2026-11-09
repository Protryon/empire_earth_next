//! See `Cargo.toml`: the generator's C API (`ee_rmap::ffi`) reaches the tool through this dependency.
use ee_rmap as _;
// the C ABI of the engine math the tool's UMath.cpp and GE units forward to (ee_ge::ffi)
use ee_ge as _;
