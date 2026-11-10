//! The C ABI of the pixel formats (`pixel.rs`; 'Graphics Engine.cpp' forwards `GEPackColor` and `GEUnpack*`), of a
//! frame's store into a surface (`GETexture.cpp`'s `GEStoreFrame`) and of `GETexture`'s, `GETextureSet`'s and
//! `GEMaterial`'s logic (`texture.rs`). The textures, surfaces and reference counts stay C++: it locks the surface and
//! passes its memory, and copies a plan's fields into its descriptions.

use std::ffi::{c_long, c_ulong};

use crate::pixel::{self, StoreTarget};
use crate::texture::{self, FramePlan};

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

/// The surface a frame is stored into (`GEStoreFrame`): depth 16 or 32, format, no alpha, pixel size, row pitch.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeGeStoreTarget {
    pub pixel_depth: c_ulong,
    pub pixel_format: c_ulong,
    pub no_alpha: bool,
    pub pixel_width: c_ulong,
    pub pixel_height: c_ulong,
    pub bytes_per_row: c_ulong,
}

impl From<&EeGeStoreTarget> for StoreTarget {
    fn from(t: &EeGeStoreTarget) -> Self {
        StoreTarget {
            pixel_depth: t.pixel_depth,
            pixel_format: t.pixel_format,
            no_alpha: t.no_alpha,
            pixel_width: t.pixel_width,
            pixel_height: t.pixel_height,
            bytes_per_row: t.bytes_per_row,
        }
    }
}

/// Whether a `width` x `height` frame fits the surface (else `GEStoreFrame` fails before locking).
///
/// # Safety
/// `target` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_texture_frame_fits(target: *const EeGeStoreTarget, width: u32, height: u32) -> bool {
    // SAFETY: the caller's contract
    pixel::frame_fits(&StoreTarget::from(unsafe { &*target }), width, height)
}

/// Stores a decoded frame (`channels` 3 or 4, top row first) into the locked surface's `memory`.
///
/// # Safety
/// `target` is valid, `memory` holds the surface's rows, `pixels` the frame's, and the frame fits
/// (`ee_ge_texture_frame_fits`).
#[no_mangle]
pub unsafe extern "C" fn ee_ge_texture_store_frame(
    target: *const EeGeStoreTarget,
    width: u32,
    height: u32,
    channels: u32,
    pixels: *const u8,
    memory: *mut u8,
) {
    // SAFETY: the caller's contract
    unsafe { pixel::store_frame(&StoreTarget::from(&*target), width, height, channels, pixels, memory) };
}

/// `GETexture::Load`'s plan for frame format 0 (one texture per frame).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeGeTextureFramePlan {
    pub mip_levels: c_long,
    pub drop: c_long,
    pub surface_type: c_ulong,
    pub mip_level_count: c_long,
    pub has_drop: bool,
    pub drop_type: c_ulong,
    pub drop_mip_level_count: c_long,
    pub drop_width: c_ulong,
    pub drop_height: c_ulong,
}

impl From<FramePlan> for EeGeTextureFramePlan {
    fn from(p: FramePlan) -> Self {
        EeGeTextureFramePlan {
            mip_levels: p.mip_levels,
            drop: p.drop,
            surface_type: p.surface_type,
            mip_level_count: p.mip_level_count,
            has_drop: p.has_drop,
            drop_type: p.drop_type,
            drop_mip_level_count: p.drop_mip_level_count,
            drop_width: p.drop_width,
            drop_height: p.drop_height,
        }
    }
}

/// The frame surfaces' description for a `width` x `height` file with `drop` levels dropped.
///
/// # Safety
/// `out` is valid for a write.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_texture_frame_plan(
    width: u32,
    height: u32,
    create_mipmaps: bool,
    drop: c_long,
    out: *mut EeGeTextureFramePlan,
) {
    // SAFETY: the caller's contract
    unsafe { *out = texture::frame_plan(width, height, create_mipmaps, drop).into() };
}

/// The frame grid of a one-surface texture (frame format 1): `mFramesPerRow` and `mFrameUVSize`.
///
/// # Safety
/// The out pointers are valid for writes.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_texture_grid_plan(frame_count: u32, frames_per_row: *mut c_ulong, uv_size: *mut f32) {
    let plan = texture::grid_plan(frame_count);
    // SAFETY: the caller's contract
    unsafe {
        *frames_per_row = plan.frames_per_row;
        *uv_size = plan.uv_size;
    }
}

/// Frame `frame`'s cell in the grid surface: left, top, right, bottom (`out[4]`) for a `mip_width` x `mip_height` level.
///
/// # Safety
/// `out` holds four floats.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_texture_grid_cell(
    frame: c_ulong,
    frames_per_row: c_ulong,
    mip_width: c_ulong,
    mip_height: c_ulong,
    out: *mut f32,
) {
    let cell = texture::grid_cell(frame, frames_per_row, mip_width, mip_height);
    for (k, v) in cell.into_iter().enumerate() {
        // SAFETY: the caller's contract
        unsafe { *out.add(k) = v };
    }
}

/// `SetFrame`'s UV corner of frame `frame`.
///
/// # Safety
/// `u` and `v` are valid for writes.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_texture_frame_corner(frame: c_ulong, frames_per_row: c_ulong, u: *mut f32, v: *mut f32) {
    let (cu, cv) = texture::frame_corner(frame, frames_per_row);
    // SAFETY: the caller's contract
    unsafe {
        *u = cu;
        *v = cv;
    }
}

/// `GETexture::Dereference`'s decision at reference count `count`: whether to drop a reference and whether to remove
/// the texture from its manager.
///
/// # Safety
/// `dereference` and `remove` are valid for writes.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_texture_dereference_decision(
    count: c_ulong,
    managed: bool,
    dereference: *mut bool,
    remove: *mut bool,
) {
    let (d, r) = texture::dereference_decision(count, managed);
    // SAFETY: the caller's contract
    unsafe {
        *dereference = d;
        *remove = r;
    }
}

/// `GETexture::GetAllocatedSize`.
#[no_mangle]
pub extern "C" fn ee_ge_texture_allocated_size(
    surface_size: c_ulong,
    frame_format: c_ulong,
    frame_count: c_ulong,
) -> c_ulong {
    texture::allocated_size(surface_size, frame_format, frame_count)
}

/// `SetStageMipFilterMode`'s mode after the texture pack's override.
#[no_mangle]
pub extern "C" fn ee_ge_texture_mip_filter_mode(
    mode: c_ulong,
    has_surface: bool,
    logical_shift: c_ulong,
    create_mipmaps: bool,
) -> c_ulong {
    texture::mip_filter_mode(mode, has_surface, logical_shift, create_mipmaps)
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
