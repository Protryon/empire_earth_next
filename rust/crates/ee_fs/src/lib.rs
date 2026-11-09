//! The engine's File System behind the C++ classes (`docs/RUST_PLAN.md` R3.5): `FSMemoryFile`, `FSChunk` and `FSArchive`.
//! The C++ facades keep what touches C++ objects (`FSFile` virtuals, `UString`, `FSFileSpec`, the directory walk, the
//! file mapping, the asserts); the logic and its formats are here, tested without game, data or window.
//!
//! The formats are contracts:
//! - saved games are `FSChunk`s (header of four dwords + the FSMemoryFile payload), `include/lle/FSChunk.h`;
//! - `.ssa` archives are a header, a FAT, a tag table and the files (`FSArchive.h`).
//!
//! `golden_fs.txt` holds the values of the C++ implementation, dumped by a throwaway driver before it was replaced.

mod archive;
mod chunk;
pub mod ffi;
mod file;
mod memfile;

pub use archive::{tokenize_star, Archive, FatEntry, ARCHIVE_IDENTIFIER, ARCHIVE_RESERVED, ARCHIVE_VERSION};
pub use chunk::EeFsChunk;
pub use file::{EeFsFileOps, FsFile, FsWrite};
pub use memfile::EeFsMemFile;

/// The `kError*` codes (`include/lle/LowLevelEngineCore.h`) that this crate returns; the C++ facades take them through
/// as `long`.
pub mod status {
    pub const OK: i32 = 0;
    pub const NOT_SUSPENDED: i32 = 2;
    pub const NOT_FOUND: i32 = 3;
    pub const NOT_OPEN: i32 = 4;
    pub const FAILED: i32 = 9;
    pub const BEYOND_END: i32 = 0xa;
    pub const END_OF_FILE: i32 = 0x12;
}

#[cfg(test)]
mod golden;
