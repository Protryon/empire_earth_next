//! The C ABI of the pixel formats (`pixel.rs`; 'Graphics Engine.cpp' forwards `GEPackColor` and `GEUnpack*`) and of
//! `GETextureSet`'s and `GEMaterial`'s logic (`texture.rs`; `GETexture` is the whole object of `texobj_tex.rs`, which
//! stores a decoded frame into a surface itself: `GEStoreFrame`).

use std::ffi::c_ulong;

use crate::ge::pixel;
use crate::ge::texture;

/// `GEPackColor(format, red, green, blue, alpha)`.
#[no_mangle]
pub extern "C" fn ee_ge_pack_color(
    format: c_ulong,
    red: c_ulong,
    green: c_ulong,
    blue: c_ulong,
    alpha: c_ulong,
) -> c_ulong {
    pixel::pack(format, red, green, blue, alpha)
}

/// `GEPackColor(format, color, alpha)` with the colour's channels.
#[no_mangle]
pub extern "C" fn ee_ge_pack_color_float(format: c_ulong, red: f32, green: f32, blue: f32, alpha: f32) -> c_ulong {
    pixel::pack_float(format, red, green, blue, alpha)
}

/// `GEUnpackRed`.
#[no_mangle]
pub extern "C" fn ee_ge_unpack_red(format: c_ulong, color: c_ulong) -> u8 {
    pixel::unpack_red(format, color)
}

/// `GEUnpackGreen`.
#[no_mangle]
pub extern "C" fn ee_ge_unpack_green(format: c_ulong, color: c_ulong) -> u8 {
    pixel::unpack_green(format, color)
}

/// `GEUnpackBlue`.
#[no_mangle]
pub extern "C" fn ee_ge_unpack_blue(format: c_ulong, color: c_ulong) -> u8 {
    pixel::unpack_blue(format, color)
}

/// `GEUnpackAlpha`.
#[no_mangle]
pub extern "C" fn ee_ge_unpack_alpha(format: c_ulong, color: c_ulong) -> u8 {
    pixel::unpack_alpha(format, color)
}

/// `GETextureSet`'s slot of tile (x, y): the texture column and row.
///
/// # Safety
/// `out_x` and `out_y` are valid for writes.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_texture_set_slot(
    x: c_ulong,
    y: c_ulong,
    coverage: c_ulong,
    width: c_ulong,
    height: c_ulong,
    out_x: *mut c_ulong,
    out_y: *mut c_ulong,
) {
    let (sx, sy) = texture::texture_set_slot(x, y, coverage, width, height);
    // SAFETY: the caller's contract
    unsafe {
        *out_x = sx;
        *out_y = sy;
    }
}

/// `GETextureSet::GetTextureCoordinate`.
///
/// # Safety
/// `u` and `v` are valid for writes.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_texture_set_coordinate(
    x: c_ulong,
    y: c_ulong,
    coverage: c_ulong,
    u: *mut f32,
    v: *mut f32,
) {
    let (cu, cv) = texture::texture_set_coordinate(x, y, coverage);
    // SAFETY: the caller's contract
    unsafe {
        *u = cu;
        *v = cv;
    }
}

/// `GEMaterial::SizeOf` from the C++'s sizes.
#[no_mangle]
pub extern "C" fn ee_ge_material_size_of(
    object_size: usize,
    string_size: usize,
    name_size: c_ulong,
    screen_count: c_ulong,
    pointer_size: usize,
    lod_count: c_ulong,
    ulong_size: usize,
) -> c_ulong {
    texture::material_size_of(object_size, string_size, name_size, screen_count, pointer_size, lod_count, ulong_size)
}
