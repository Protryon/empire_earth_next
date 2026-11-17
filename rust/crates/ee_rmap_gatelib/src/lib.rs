//! See `Cargo.toml`: the generator's C API (`ee_rmap::ffi`) reaches the tool through this dependency.
use ee_rmap as _;
// the C ABI of the engine math the tool's UMath.cpp and GE units forward to (ee_ge::ffi)
use ee_ge as _;
// the file specs and the thread primitives the tool's FSFileSpec.cpp and engine headers forward to
use ee_base as _;
use ee_base as _;
