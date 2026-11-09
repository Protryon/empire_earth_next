//! `extern "C"` for the bilinear copy (`DX7Surface::Copy16/32BitWithFilter`; the C++ keeps Lock/Unlock).
#![allow(clippy::missing_safety_doc)]

use core::ffi::c_ulong;

use crate::filter::{copy_with_filter, PixelOps, Rect};

/// cbindgen:ignore
mod sys {
    use core::ffi::c_ulong;

    extern "C" {
        pub fn ee_dx7cpp_unpack_red(format: c_ulong, color: c_ulong) -> u8;
        pub fn ee_dx7cpp_unpack_green(format: c_ulong, color: c_ulong) -> u8;
        pub fn ee_dx7cpp_unpack_blue(format: c_ulong, color: c_ulong) -> u8;
        pub fn ee_dx7cpp_unpack_alpha(format: c_ulong, color: c_ulong) -> u8;
        pub fn ee_dx7cpp_pack(format: c_ulong, red: i32, green: i32, blue: i32, alpha: i32) -> c_ulong;
    }
}

use sys::*;

/// `GEUnpack*`/`GEPackColor` through the C++ (the format tables are the Graphics Engine's, R4.3).
struct CppOps(c_ulong);

#[allow(clippy::unnecessary_cast)]
impl PixelOps for CppOps {
    fn unpack_red(&self, pixel: u32) -> u8 {
        unsafe { ee_dx7cpp_unpack_red(self.0, pixel as c_ulong) }
    }
    fn unpack_green(&self, pixel: u32) -> u8 {
        unsafe { ee_dx7cpp_unpack_green(self.0, pixel as c_ulong) }
    }
    fn unpack_blue(&self, pixel: u32) -> u8 {
        unsafe { ee_dx7cpp_unpack_blue(self.0, pixel as c_ulong) }
    }
    fn unpack_alpha(&self, pixel: u32) -> u8 {
        unsafe { ee_dx7cpp_unpack_alpha(self.0, pixel as c_ulong) }
    }
    fn pack(&self, red: i32, green: i32, blue: i32, alpha: i32) -> u32 {
        unsafe { ee_dx7cpp_pack(self.0, red, green, blue, alpha) as u32 }
    }
}

/// `rect` is 4 floats: left, top, right, bottom.
unsafe fn rect(p: *const f32) -> Rect {
    unsafe { Rect { left: *p, top: *p.add(1), right: *p.add(2), bottom: *p.add(3) } }
}

/// The 32-bit path; pitches in pixels. `source_rect`/`destination_rect`: float pixels (see `Rect`).
#[no_mangle]
pub unsafe extern "C" fn ee_dx7_copy_with_filter_32(
    format: c_ulong,
    source: *const u32,
    source_pitch: u32,
    destination: *mut u32,
    destination_pitch: u32,
    source_rect: *const f32,
    destination_rect: *const f32,
) {
    let ops = CppOps(format);
    unsafe {
        copy_with_filter(
            &ops,
            rect(source_rect),
            rect(destination_rect),
            source_pitch,
            destination_pitch,
            |i| *source.offset(i as isize),
            |i, v| *destination.add(i as usize) = v,
        );
    }
}

/// The 16-bit path: as the 32-bit one (the original copy-pasted loop, words differ).
#[no_mangle]
pub unsafe extern "C" fn ee_dx7_copy_with_filter_16(
    format: c_ulong,
    source: *const u16,
    source_pitch: u32,
    destination: *mut u16,
    destination_pitch: u32,
    source_rect: *const f32,
    destination_rect: *const f32,
) {
    let ops = CppOps(format);
    unsafe {
        copy_with_filter(
            &ops,
            rect(source_rect),
            rect(destination_rect),
            source_pitch,
            destination_pitch,
            |i| u32::from(*source.offset(i as isize)),
            |i, v| *destination.add(i as usize) = v as u16,
        );
    }
}
