# wgpu-native v24.0.3.1 (e305465e8f1abd2b13878274bf74bbde920096a3), MIT or Apache-2.0

Vendored so that `ee_rust` can link it: the crate declares only `cdylib` and `staticlib`, so it cannot be a cargo
dependency, and a second Rust staticlib in the exe would bring a second copy of Rust std (docs/RUST_PLAN.md section 4).

Changes from the tag:

- `Cargo.toml`: `crate-type = ["rlib"]`; the `[workspace]` tables moved to `rust/Cargo.toml`.
- `ffi/wgpu.h` and `ffi/webgpu-headers/webgpu.h` moved to `include/webgpu/`, where the C++ includes them; `build.rs`
  follows.

`rust/Cargo.lock` started from the tag's lock file, so wgpu-core and its dependencies are the released versions.
This crate goes away in R4, when GX moves to the `wgpu` crate.

`bindgen` is 0.72 instead of 0.70 (build-time only): 0.70 turns the headers' structs into empty ones under libclang 23.
