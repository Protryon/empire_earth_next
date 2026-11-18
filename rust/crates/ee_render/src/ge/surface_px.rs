//! `GESurface`'s image-file pixel loops (R4.3, tenth part): a locked surface's rows as the 3-byte pixels the BMP
//! and JPEG writers take (blue, green, red for a BMP; red, green, blue or the alpha three times for a JPEG or a
//! JPG2's alpha image), a decoded JPEG scanline into the surface (opaque colour, or the alpha merged into the pixels
//! already there), and a decoded PNG into the surface's top left. The files and the codecs are surface_file.rs's, the
//! surface's virtuals C++'s (the renderer's DX7Surface implements them); each row's conversion is here.
//!
//! A 32-bit surface is written as A8R8G8B8 whatever its format (the original's `(a << 24) | (r << 16) | ...`); a
//! 16-bit one through `GEPackColor` (`pixel::pack`). Pixels are read and written unaligned, in the machine's order.

use std::ffi::c_ulong;

use crate::ge::pixel::{pack, unpack_alpha, unpack_blue, unpack_green, unpack_red};

/// The 3-byte order of `ee_ge_surf_row_out`.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeGeSurfRow {
    /// A BMP's: blue, green, red.
    Bgr = 0,
    /// A JPEG's: red, green, blue.
    Rgb,
    /// A JPG2's alpha image: the alpha three times.
    Alpha,
}

/// Pixel `x` of a row (32 or 16 bits), zero-extended.
///
/// # Safety
/// The row holds pixel `x`.
unsafe fn read(row: *const u8, depth32: bool, x: usize) -> c_ulong {
    // SAFETY: the caller's contract
    unsafe {
        if depth32 {
            c_ulong::from(row.cast::<u32>().add(x).read_unaligned())
        } else {
            c_ulong::from(row.cast::<u16>().add(x).read_unaligned())
        }
    }
}

/// Writes pixel `x` of a row.
///
/// # Safety
/// The row holds pixel `x`.
#[allow(clippy::unnecessary_cast)] // `c_ulong` is `u32` on Windows, `u64` on Linux and macOS
unsafe fn write(row: *mut u8, depth32: bool, x: usize, value: c_ulong) {
    // SAFETY: the caller's contract
    unsafe {
        if depth32 {
            row.cast::<u32>().add(x).write_unaligned(value as u32);
        } else {
            row.cast::<u16>().add(x).write_unaligned(value as u16);
        }
    }
}

/// `SaveAsBMP`'s, `SaveAsJPG`'s and `SaveAsJPG2`'s row: `width` pixels of `row` (`depth` 32, else 16 bits) as 3
/// bytes each into `out`.
///
/// # Safety
/// `row` holds `width` pixels; `out` has room for `width * 3` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_surf_row_out(
    format: c_ulong,
    depth: c_ulong,
    row: *const u8,
    width: c_ulong,
    order: EeGeSurfRow,
    out: *mut u8,
) {
    let depth32 = depth == 32;
    for x in 0..width as usize {
        // SAFETY: the function's contract
        unsafe {
            let p = read(row, depth32, x);
            let o = out.add(x * 3);
            match order {
                EeGeSurfRow::Bgr => {
                    *o.add(2) = unpack_red(format, p);
                    *o.add(1) = unpack_green(format, p);
                    *o = unpack_blue(format, p);
                }
                EeGeSurfRow::Rgb => {
                    *o = unpack_red(format, p);
                    *o.add(1) = unpack_green(format, p);
                    *o.add(2) = unpack_blue(format, p);
                }
                EeGeSurfRow::Alpha => {
                    let a = unpack_alpha(format, p);
                    *o = a;
                    *o.add(1) = a;
                    *o.add(2) = a;
                }
            }
        }
    }
}

/// `LoadJPG`'s and `LoadJPG2`'s scanline: `stride` bytes of `src`, 3 a pixel, into the surface from `dst` (`depth`
/// 32, else 16 bits): opaque colour, or (`alpha`) the first byte of each as the alpha of the pixel already there.
/// Returns the next row's start: `skip` pixels after the row's last.
///
/// # Safety
/// `src` holds `stride` bytes; the surface holds the row's pixels and the skip from `dst`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_surf_row_in(
    format: c_ulong,
    depth: c_ulong,
    src: *const u8,
    stride: c_ulong,
    alpha: bool,
    dst: *mut u8,
    skip: c_ulong,
) -> *mut u8 {
    let depth32 = depth == 32;
    let size = if depth32 { 4 } else { 2 };
    let stride = stride as usize;
    // a byte past the scanline (a one-component JPEG's last pixels) reads 0 (the original read past the buffer)
    // SAFETY: the function's contract
    let byte = |i: usize| if i < stride { c_ulong::from(unsafe { *src.add(i) }) } else { 0 };
    let mut x = 0usize;
    let mut index = 0usize;
    while index < stride {
        // SAFETY: the function's contract
        unsafe {
            if !alpha {
                let (r, g, b) = (byte(index), byte(index + 1), byte(index + 2));
                let v = if depth32 { 0xff00_0000 | (r << 16) | (g << 8) | b } else { pack(format, r, g, b, 0xff) };
                write(dst, depth32, x, v);
            } else {
                let a = byte(index);
                let p = read(dst, depth32, x);
                let (r, g, b) = (
                    c_ulong::from(unpack_red(format, p)),
                    c_ulong::from(unpack_green(format, p)),
                    c_ulong::from(unpack_blue(format, p)),
                );
                let v = if depth32 { (a << 24) | (r << 16) | (g << 8) | b } else { pack(format, r, g, b, a) };
                write(dst, depth32, x, v);
            }
        }
        x += 1;
        index += 3;
    }
    // SAFETY: the function's contract
    unsafe { dst.add((x + skip as usize) * size) }
}

/// `LoadPNG`'s store: the decoded picture (`width` x `height`, blue, green, red, alpha a pixel, top row first) into
/// the locked surface's top left (`depth` 32, else 16 bits; `no_alpha`: opaque).
///
/// # Safety
/// `src` holds the picture; the surface (`bytes_per_row` a row) holds its rows.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_surf_png_store(
    format: c_ulong,
    depth: c_ulong,
    no_alpha: bool,
    src: *const u8,
    width: c_ulong,
    height: c_ulong,
    memory: *mut u8,
    bytes_per_row: c_ulong,
) {
    let depth32 = depth == 32;
    let mut s = src;
    for y in 0..height as usize {
        // SAFETY: the function's contract
        unsafe {
            let row = memory.add(y * bytes_per_row as usize);
            for x in 0..width as usize {
                let (b, g, r) = (c_ulong::from(*s), c_ulong::from(*s.add(1)), c_ulong::from(*s.add(2)));
                let a = if no_alpha { 0xff } else { c_ulong::from(*s.add(3)) };
                let v = if depth32 { (a << 24) | (r << 16) | (g << 8) | b } else { pack(format, r, g, b, a) };
                write(row, depth32, x, v);
                s = s.add(4);
            }
        }
    }
}

#[cfg(test)]
#[path = "surface_px_tests.rs"]
mod tests;
