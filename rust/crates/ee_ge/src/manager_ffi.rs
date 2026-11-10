//! The C ABI of the managers' logic (`managers.rs`), called by `GETextureManager.cpp` and `GEModelManager.cpp`. Names
//! go in as NUL-terminated C strings and come back into a caller's buffer (NUL-terminated, cut to the capacity).

use std::ffi::{c_char, c_long, c_ulong, CStr};

use crate::managers::{self, with_pack};

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

/// Writes `bytes` NUL-terminated into `out` (cut to `capacity - 1` bytes); the bytes written without the NUL.
///
/// # Safety
/// `out` holds `capacity` bytes.
unsafe fn put_c_string(bytes: &[u8], out: *mut c_char, capacity: usize) -> usize {
    if out.is_null() || capacity == 0 {
        return 0;
    }
    let n = bytes.len().min(capacity - 1);
    // SAFETY: the caller's contract
    unsafe {
        std::ptr::copy_nonoverlapping(bytes.as_ptr(), out.cast::<u8>(), n);
        *out.add(n) = 0;
    }
    n
}

/// `GETextureStem`: the length of a texture's cache name within `name`.
///
/// # Safety
/// `name` is NUL-terminated.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_texture_stem(name: *const c_char) -> usize {
    // SAFETY: the caller's contract
    managers::texture_stem(unsafe { c_bytes(name) })
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

/// The pack's name of a texture (`GETexturePackName`; with `require_enabled`, `GetTexturePackName`): false and nothing
/// written when there is none.
///
/// # Safety
/// `name` is NUL-terminated; `out` holds `capacity` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_texpack_name(
    name: *const c_char,
    require_enabled: bool,
    out: *mut c_char,
    capacity: usize,
) -> bool {
    // SAFETY: the caller's contract
    let name = unsafe { c_bytes(name) };
    let packed = with_pack(|p| if require_enabled { p.enabled_pack_name(name) } else { p.pack_name(name) });
    match packed {
        Some(bytes) => {
            // SAFETY: the caller's contract
            unsafe { put_c_string(&bytes, out, capacity) };
            true
        }
        None => false,
    }
}

/// Whether the pack is on (`sTexturePackEnabled`).
#[no_mangle]
pub extern "C" fn ee_ge_texpack_enabled() -> bool {
    with_pack(|p| p.enabled)
}

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

/// `LoadTexture`'s MIP decisions: the levels to drop and whether to build MIP levels; returns the pack's extra drop.
///
/// # Safety
/// The out pointers are valid for writes.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_texpack_load_plan(
    create_mipmaps: bool,
    drop: c_long,
    from_pack: bool,
    out_drop: *mut c_long,
    out_mipmaps: *mut bool,
) -> c_long {
    let (pack_drop, total, mipmaps) = with_pack(|p| p.load_plan(create_mipmaps, drop, from_pack));
    // SAFETY: the caller's contract
    unsafe {
        *out_drop = total;
        *out_mipmaps = mipmaps;
    }
    pack_drop
}

/// `SetTexturePackSizes`' arithmetic: the texture's new `mNumMipLevelsToDrop` and its logical shift.
///
/// # Safety
/// The out pointers are valid for writes.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_texpack_sizes(
    drop: c_ulong,
    mip_levels: c_long,
    out_drop: *mut c_ulong,
    out_shift: *mut c_ulong,
) {
    let (new_drop, shift) = with_pack(|p| p.pack_sizes(drop, mip_levels));
    // SAFETY: the caller's contract
    unsafe {
        *out_drop = new_drop;
        *out_shift = shift;
    }
}

/// The surface description a texture description asks for: depth, format and `mNoAlpha`.
///
/// # Safety
/// The out pointers are valid for writes.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_texture_surface_format(
    pixel_depth: c_ulong,
    has_alpha: bool,
    out_depth: *mut c_ulong,
    out_format: *mut c_ulong,
    out_no_alpha: *mut bool,
) {
    let (depth, format, no_alpha) = managers::surface_format(pixel_depth, has_alpha);
    // SAFETY: the caller's contract
    unsafe {
        *out_depth = depth;
        *out_format = format;
        *out_no_alpha = no_alpha;
    }
}

/// A manager's removal decision at reference count `count`: whether to remove and release; the result code.
///
/// # Safety
/// `out_remove` is valid for a write.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_manager_remove_decision(count: c_ulong, out_remove: *mut bool) -> c_long {
    let (remove, result) = managers::remove_decision(count);
    // SAFETY: the caller's contract
    unsafe { *out_remove = remove };
    result
}

/// The next made texture's name (`MakeTexture`), into `out`.
///
/// # Safety
/// `out` holds `capacity` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_texman_made_name(out: *mut c_char, capacity: usize) -> usize {
    // SAFETY: the caller's contract
    unsafe { put_c_string(&managers::next_made_texture_name(), out, capacity) }
}

/// The next created model's name (`CreateModel`) from the caller's, into `out`.
///
/// # Safety
/// `name` is NUL-terminated; `out` holds `capacity` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_modman_created_name(name: *const c_char, out: *mut c_char, capacity: usize) -> usize {
    // SAFETY: the caller's contract
    let name = unsafe { c_bytes(name) };
    // SAFETY: the caller's contract
    unsafe { put_c_string(&managers::next_created_model_name(name), out, capacity) }
}
