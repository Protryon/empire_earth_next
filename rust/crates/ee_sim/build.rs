//! The vendored Lua (src/ThirdParty/lua/ee_lua.c) for this crate's own tests only: lua_host.rs links it under
//! cfg(test). The game links the Lua its C++ build compiles (tools/units.py LIB_UNITS "lua"); a release build of
//! ee_rust compiles nothing here, so the exe never sees a second Lua.

fn main() {
    // cargo's test and example programs of this crate link objects whose calls into the game's C++ they never
    // make (docs/progress/crate-restructure.md R1): the linker leaves those unresolved. The game's static library is
    // linked by tools/build.py and tools/xbuild.py, which this does not reach.
    let triple = std::env::var("TARGET").unwrap_or_default();
    let ignore = if triple.contains("msvc") {
        "/FORCE:UNRESOLVED"
    } else if triple.contains("apple") {
        "-Wl,-undefined,dynamic_lookup"
    } else {
        "-Wl,--unresolved-symbols=ignore-all"
    };
    println!("cargo:rustc-link-arg={ignore}");
    println!("cargo:rerun-if-changed=build.rs");
    let lua = std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("../../../src/ThirdParty/lua");
    println!("cargo:rerun-if-changed=build.rs");
    println!("cargo:rerun-if-changed={}", lua.display());
    let profile = std::env::var("PROFILE").unwrap_or_default();
    let (target, host) = (std::env::var("TARGET").unwrap_or_default(), std::env::var("HOST").unwrap_or_default());
    if profile == "release" || target != host {
        return;
    }
    cc::Build::new().file(lua.join("ee_lua.c")).warnings(false).cargo_metadata(false).compile("ee_ai_lua_test");
    println!("cargo:rustc-link-search=native={}", std::env::var("OUT_DIR").unwrap());
}
