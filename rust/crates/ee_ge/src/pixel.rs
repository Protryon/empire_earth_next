//! The engine's pixel formats ('Graphics Engine.cpp': `GEPackColor`, `GEUnpackRed`..`GEUnpackAlpha` over the
//! `sPixelFormats` table) and the store of one decoded texture frame into a surface's memory (`GETexture.cpp`'s
//! `GEStoreFrame`, which is the original's Targa reader's loop: 32-bit surfaces get A8R8G8B8 whatever their format,
//! 16-bit ones the packed colour of their format, no alpha is 0xff).
//!
//! `unsigned long` arithmetic is `c_ulong`'s, so the packed value's bits are the C++'s on each data model (a caller
//! keeps the low 16 or 32 bits).

#![allow(clippy::unnecessary_cast)] // c_long and c_ulong differ per target

use std::ffi::{c_long, c_ulong};

use crate::um::lrintf;

/// One format: the right shift (from 8 bits) and the position of each channel (red, green, blue, alpha).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct PixelFormat {
    pub red_shift: u32,
    pub red_position: u32,
    pub green_shift: u32,
    pub green_position: u32,
    pub blue_shift: u32,
    pub blue_position: u32,
    pub alpha_shift: u32,
    pub alpha_position: u32,
}

const fn pf(v: [u32; 8]) -> PixelFormat {
    PixelFormat {
        red_shift: v[0],
        red_position: v[1],
        green_shift: v[2],
        green_position: v[3],
        blue_shift: v[4],
        blue_position: v[5],
        alpha_shift: v[6],
        alpha_position: v[7],
    }
}

/// `sPixelFormats` (0x100d9eb8): A8R8G8B8, X8R8G8B8, A4R4G4B4, A1R5G5B5, R5G6B5 and the three 16-bit forms without
/// alpha, by `GESurface`'s format numbers.
pub const PIXEL_FORMATS: [PixelFormat; 8] = [
    pf([0, 16, 0, 8, 0, 0, 0, 24]),
    pf([0, 16, 0, 8, 0, 0, 8, 0]),
    pf([4, 8, 4, 4, 4, 0, 4, 12]),
    pf([3, 10, 3, 5, 3, 0, 7, 15]),
    pf([3, 11, 3, 6, 3, 1, 7, 0]),
    pf([3, 10, 3, 5, 3, 0, 8, 0]),
    pf([3, 11, 3, 6, 3, 1, 8, 0]),
    pf([3, 11, 2, 5, 3, 0, 8, 0]),
];

/// `GEPackColor(format, red, green, blue, alpha)`: 8-bit channels into a pixel of the format (the C++ ors alpha, blue,
/// green, red in that order; the or is commutative).
pub fn pack(format: c_ulong, red: c_ulong, green: c_ulong, blue: c_ulong, alpha: c_ulong) -> c_ulong {
    let f = &PIXEL_FORMATS[format as usize];
    (alpha >> f.alpha_shift) << f.alpha_position
        | (blue >> f.blue_shift) << f.blue_position
        | (green >> f.green_shift) << f.green_position
        | (red >> f.red_shift) << f.red_position
}

/// `GEPackColor(format, color, alpha)`: float channels scaled by 255 and rounded (`UMFloatToLong`) into a pixel.
pub fn pack_float(format: c_ulong, red: f32, green: f32, blue: f32, alpha: f32) -> c_ulong {
    let f = &PIXEL_FORMATS[format as usize];
    let r = lrintf(red * 255.0) as c_long as c_ulong;
    let g = lrintf(green * 255.0) as c_long as c_ulong;
    let b = lrintf(blue * 255.0) as c_long as c_ulong;
    let a = lrintf(alpha * 255.0) as c_long as c_ulong;
    (r >> f.red_shift) << f.red_position
        | (g >> f.green_shift) << f.green_position
        | (b >> f.blue_shift) << f.blue_position
        | (a >> f.alpha_shift) << f.alpha_position
}

/// `GEUnpackRed`: a pixel's red channel scaled to 8 bits (the low byte of the shifted value, as the C++'s `unsigned
/// char` return).
pub fn unpack_red(format: c_ulong, color: c_ulong) -> u8 {
    let f = &PIXEL_FORMATS[format as usize];
    ((color >> f.red_position) << f.red_shift) as u8
}

/// `GEUnpackGreen`.
pub fn unpack_green(format: c_ulong, color: c_ulong) -> u8 {
    let f = &PIXEL_FORMATS[format as usize];
    ((color >> f.green_position) << f.green_shift) as u8
}

/// `GEUnpackBlue`.
pub fn unpack_blue(format: c_ulong, color: c_ulong) -> u8 {
    let f = &PIXEL_FORMATS[format as usize];
    ((color >> f.blue_position) << f.blue_shift) as u8
}

/// `GEUnpackAlpha`.
pub fn unpack_alpha(format: c_ulong, color: c_ulong) -> u8 {
    let f = &PIXEL_FORMATS[format as usize];
    ((color >> f.alpha_position) << f.alpha_shift) as u8
}

/// A locked surface a frame is stored into: its depth (16 or 32), format, whether its description refuses alpha, its
/// pixel size and row pitch.
#[derive(Clone, Copy, Debug)]
pub struct StoreTarget {
    pub pixel_depth: c_ulong,
    pub pixel_format: c_ulong,
    pub no_alpha: bool,
    pub pixel_width: c_ulong,
    pub pixel_height: c_ulong,
    pub bytes_per_row: c_ulong,
}

/// Whether a `width` x `height` frame fits the surface (`GEStoreFrame` refuses a larger one before locking).
pub fn frame_fits(target: &StoreTarget, width: u32, height: u32) -> bool {
    !(width as c_ulong > target.pixel_width || height as c_ulong > target.pixel_height)
}

/// Stores a decoded frame (`width` x `height` pixels of `channels` bytes, top row first, R, G, B(, A)) into the
/// surface's memory: each row continues after the surface's pixel width minus the frame's plus its row padding
/// (`LoadTGA`'s skip, computed from the surface's pixel width). `memory` holds the surface's rows.
///
/// # Safety
/// `memory` is valid for `bytes_per_row * pixel_height` bytes and `pixels` for `width * height * channels`, and the
/// frame fits (`frame_fits`).
pub unsafe fn store_frame(
    target: &StoreTarget,
    width: u32,
    height: u32,
    channels: u32,
    pixels: *const u8,
    memory: *mut u8,
) {
    let bytes_per_pixel = channels as usize;
    let has_alpha = channels == 4 && !target.no_alpha;
    let mut source = pixels;
    // SAFETY: the caller's contract, per pixel below
    let channel = |p: *const u8, k: usize| unsafe { *p.add(k) };
    if target.pixel_depth == 32 {
        let skip = target.bytes_per_row.wrapping_sub(target.pixel_width.wrapping_mul(4)) / 4;
        let mut out = memory.cast::<u32>();
        for _ in 0..height {
            for _ in 0..width {
                let red = channel(source, 0) as u32;
                let green = channel(source, 1) as u32;
                let blue = channel(source, 2) as u32;
                let alpha = if has_alpha { channel(source, 3) as u32 } else { 0xff };
                source = source.wrapping_add(bytes_per_pixel);
                // SAFETY: the frame fits the surface
                unsafe {
                    out.write_unaligned((alpha << 24) | (red << 16) | (green << 8) | blue);
                    out = out.add(1);
                }
            }
            out = out.wrapping_add(skip as usize);
        }
    } else {
        let skip = target.bytes_per_row.wrapping_sub(target.pixel_width.wrapping_mul(2)) / 2;
        let mut out = memory.cast::<u16>();
        for _ in 0..height {
            for _ in 0..width {
                let red = channel(source, 0) as c_ulong;
                let green = channel(source, 1) as c_ulong;
                let blue = channel(source, 2) as c_ulong;
                let alpha = if has_alpha { channel(source, 3) as c_ulong } else { 0xff };
                source = source.wrapping_add(bytes_per_pixel);
                // SAFETY: the frame fits the surface
                unsafe {
                    out.write_unaligned(pack(target.pixel_format, red, green, blue, alpha) as u16);
                    out = out.add(1);
                }
            }
            out = out.wrapping_add(skip as usize);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a8r8g8b8_packs_each_channel_in_its_byte() {
        assert_eq!(pack(0, 0x12, 0x34, 0x56, 0x78), 0x7812_3456);
        assert_eq!(pack(1, 0x12, 0x34, 0x56, 0x78), 0x0012_3456); // X8R8G8B8 drops the alpha
    }

    #[test]
    fn the_16_bit_formats_drop_the_low_bits() {
        // A4R4G4B4
        assert_eq!(pack(2, 0xff, 0x80, 0x10, 0xf0), 0xff81);
        // A1R5G5B5
        assert_eq!(pack(3, 0xff, 0x00, 0xff, 0xff), 0x801f | (0x1f << 10));
        // R5G6B5 (format 7): the green keeps six bits
        assert_eq!(pack(7, 0, 0xff, 0, 0xff), 0x3f << 5);
        assert_eq!(pack(4, 0, 0xff, 0, 0), 0x1f << 6); // format 4's green has only five
    }

    #[test]
    fn the_float_form_rounds_half_to_even() {
        assert_eq!(pack_float(0, 1.0, 0.5, 0.0, 1.0), 0xff00_0000 | (255 << 16) | (128 << 8));
        // 0.1 * 255 = 25.5 (as f32 slightly below): lrintf gives 25 or 26 as the C++ does: both agree with lrintf
        assert_eq!(pack_float(0, 0.1, 0.0, 0.0, 0.0), ((lrintf(0.1f32 * 255.0) as c_ulong) << 16));
    }

    #[test]
    fn unpack_inverts_pack_for_8_bit_channels() {
        let p = pack(0, 0x12, 0x34, 0x56, 0x78);
        assert_eq!(unpack_red(0, p), 0x12);
        assert_eq!(unpack_green(0, p), 0x34);
        assert_eq!(unpack_blue(0, p), 0x56);
        assert_eq!(unpack_alpha(0, p), 0x78);
        // a 16-bit format gives the channel's high bits back
        let q = pack(2, 0xff, 0x80, 0x10, 0xf0);
        assert_eq!(unpack_red(2, q) & 0xf0, 0xf0);
        assert_eq!(unpack_green(2, q) & 0xf0, 0x80);
        assert_eq!(unpack_blue(2, q) & 0xf0, 0x10);
    }

    fn target(depth: c_ulong, format: c_ulong, no_alpha: bool, w: c_ulong, h: c_ulong, pitch: c_ulong) -> StoreTarget {
        StoreTarget {
            pixel_depth: depth,
            pixel_format: format,
            no_alpha,
            pixel_width: w,
            pixel_height: h,
            bytes_per_row: pitch,
        }
    }

    #[test]
    fn a_larger_frame_is_refused() {
        let t = target(32, 0, false, 4, 4, 16);
        assert!(frame_fits(&t, 4, 4));
        assert!(!frame_fits(&t, 5, 4));
        assert!(!frame_fits(&t, 4, 5));
    }

    #[test]
    fn a_32_bit_store_skips_the_row_padding_computed_from_the_surface_width() {
        // a 2-pixel wide surface with 8 padding bytes per row, a 2 x 2 RGBA frame: the rows land in the surface's
        let t = target(32, 7, false, 2, 2, 2 * 4 + 8);
        let pixels: [u8; 16] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16];
        let mut memory = vec![0xaau8; 16 * 2];
        // SAFETY: the buffers match the target
        unsafe { store_frame(&t, 2, 2, 4, pixels.as_ptr(), memory.as_mut_ptr()) };
        let px = |k: usize| u32::from_le_bytes(memory[k * 4..][..4].try_into().unwrap());
        assert_eq!(px(0), 0x0401_0203);
        assert_eq!(px(1), 0x0805_0607);
        assert_eq!(px(2), 0xaaaa_aaaa); // the padding
        assert_eq!(px(3), 0xaaaa_aaaa);
        assert_eq!(px(4), 0x0c09_0a0b);
        assert_eq!(px(5), 0x100d_0e0f);
    }

    #[test]
    fn a_narrower_frame_continues_after_the_skip_of_the_surface_width() {
        // LoadTGA's quirk, kept: the skip is the surface's pixel width minus the frame's plus the padding, so a 2-wide
        // frame in a 4-wide surface puts its second row 4 pixels after its first (in the first row's padding)
        let t = target(32, 7, false, 4, 2, 4 * 4 + 8);
        let pixels: [u8; 16] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16];
        let mut memory = vec![0xaau8; 24 * 2];
        // SAFETY: the buffers match the target
        unsafe { store_frame(&t, 2, 2, 4, pixels.as_ptr(), memory.as_mut_ptr()) };
        let px = |k: usize| u32::from_le_bytes(memory[k * 4..][..4].try_into().unwrap());
        assert_eq!(px(0), 0x0401_0203);
        assert_eq!(px(1), 0x0805_0607);
        assert_eq!(px(2), 0xaaaa_aaaa);
        assert_eq!(px(4), 0x0c09_0a0b);
        assert_eq!(px(5), 0x100d_0e0f);
        assert_eq!(px(6), 0xaaaa_aaaa); // the second surface row is untouched
    }

    #[test]
    fn no_alpha_is_opaque_and_rgb_frames_too() {
        let t = target(32, 0, true, 1, 1, 4);
        let mut memory = [0u8; 4];
        // SAFETY: the buffers match the target
        unsafe { store_frame(&t, 1, 1, 4, [9, 8, 7, 0].as_ptr(), memory.as_mut_ptr()) };
        assert_eq!(u32::from_le_bytes(memory), 0xff09_0807);
        let t = target(32, 0, false, 1, 1, 4);
        // SAFETY: as above
        unsafe { store_frame(&t, 1, 1, 3, [9, 8, 7].as_ptr(), memory.as_mut_ptr()) };
        assert_eq!(u32::from_le_bytes(memory), 0xff09_0807);
    }

    #[test]
    fn a_16_bit_store_packs_with_the_surface_format() {
        let t = target(16, 2, false, 2, 1, 2 * 2 + 8);
        let mut memory = [0u8; 12];
        // SAFETY: the buffers match the target
        unsafe { store_frame(&t, 2, 1, 4, [0xff, 0x80, 0x10, 0xf0, 0, 0, 0, 0].as_ptr(), memory.as_mut_ptr()) };
        assert_eq!(u16::from_le_bytes([memory[0], memory[1]]), 0xff81);
        assert_eq!(u16::from_le_bytes([memory[2], memory[3]]), 0);
    }
}
