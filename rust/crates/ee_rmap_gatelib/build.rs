//! The build script: the link of this crate's test programs (see below).

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
}
