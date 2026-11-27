//! The C ABI of the managers' logic (`managers.rs`), called by `GETextureManager.cpp` and `GEModelManager.cpp`. Names
//! go in as NUL-terminated C strings and come back into a caller's buffer (NUL-terminated, cut to the capacity).

use std::ffi::{c_char, c_long, c_void, CStr};

use crate::ge::managers::with_pack;

/// The C string at `s` as bytes (empty for NULL).
///
/// # Safety
/// `s` is NULL or NUL-terminated.
unsafe fn c_bytes<'a>(s: *const c_char) -> &'a [u8] {
    if s.is_null() {
        &[]
    } else {
        // SAFETY: the caller's contract
        unsafe { CStr::from_ptr(s) }.to_bytes()
    }
}

/// `GETextureManager::SetTexturePack`.
///
/// # Safety
/// `directory` is NULL or NUL-terminated.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_texpack_set(directory: *const c_char, scale_shift: c_long, mip_levels_to_drop: c_long) {
    // SAFETY: the caller's contract
    let directory = unsafe { c_bytes(directory) };
    with_pack(|p| p.set(directory, scale_shift, mip_levels_to_drop));
}

/// `GETextureManager::GetTexturePackName(name, out)`: the pack's name of a texture while the pack is on, into the
/// `UString` `out` through the original's 1,024-byte buffer (cut to 1,023 bytes); false and `out` unchanged when
/// there is none.
///
/// # Safety
/// `name` is NUL-terminated; `out` is a C++ `UString`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_texpack_name(name: *const c_char, out: *mut c_void) -> bool {
    // SAFETY: the caller's contract
    let name = unsafe { c_bytes(name) };
    match with_pack(|p| p.enabled_pack_name(name)) {
        Some(mut bytes) => {
            bytes.truncate(PACK_NAME_BUFFER - 1);
            // SAFETY: the caller's contract
            unsafe { crate::ge::cppobj::str_set_bytes(out as usize, &bytes) };
            true
        }
        None => false,
    }
}

/// `GetTexturePackName`'s buffer (`char theBuffer[1024]`).
const PACK_NAME_BUFFER: usize = 1024;

/// `GetTexturePackLogicalShift`.
#[no_mangle]
pub extern "C" fn ee_ge_texpack_logical_shift() -> c_long {
    with_pack(|p| p.logical_shift())
}

/// `GetTexturePackMipLevelsToDrop`.
#[no_mangle]
pub extern "C" fn ee_ge_texpack_mip_levels_to_drop() -> c_long {
    with_pack(|p| p.mip_levels_to_drop)
}
