//! The vendored Lua (src/ThirdParty/lua/ee_lua.c) for this crate's own tests only: lua_host.rs links it under
//! cfg(test). The game links the Lua its C++ build compiles (tools/units.py LIB_UNITS "lua"); a release build of
//! ee_rust compiles nothing here, so the exe never sees a second Lua.

fn main() {
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
