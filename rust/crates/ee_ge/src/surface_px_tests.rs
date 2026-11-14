//! `surface_px.rs`'s tests: each loop against the original's formulas on 32-bit and 16-bit surfaces (the scratch
//! golden over the unmodified GESurface.cpp runs the whole codecs through libjpeg; these pin the rows).

use super::*;

const A8R8G8B8: c_ulong = 0;
const R5G6B5: c_ulong = 7;
const A4R4G4B4: c_ulong = 2;

#[test]
fn a_row_out_in_each_order() {
    let row32: [u32; 2] = [0x8011_2233, 0xff44_5566];
    let mut out = [0u8; 6];
    // SAFETY: two pixels, six bytes
    unsafe { ee_ge_surf_row_out(A8R8G8B8, 32, row32.as_ptr().cast(), 2, EeGeSurfRow::Bgr, out.as_mut_ptr()) };
    assert_eq!(out, [0x33, 0x22, 0x11, 0x66, 0x55, 0x44]);
    // SAFETY: as above
    unsafe { ee_ge_surf_row_out(A8R8G8B8, 32, row32.as_ptr().cast(), 2, EeGeSurfRow::Rgb, out.as_mut_ptr()) };
    assert_eq!(out, [0x11, 0x22, 0x33, 0x44, 0x55, 0x66]);
    // SAFETY: as above
    unsafe { ee_ge_surf_row_out(A8R8G8B8, 32, row32.as_ptr().cast(), 2, EeGeSurfRow::Alpha, out.as_mut_ptr()) };
    assert_eq!(out, [0x80, 0x80, 0x80, 0xff, 0xff, 0xff]);
    let row16: [u16; 1] = [0xf81f];
    let mut out = [0u8; 3];
    // SAFETY: one pixel, three bytes
    unsafe { ee_ge_surf_row_out(R5G6B5, 16, row16.as_ptr().cast(), 1, EeGeSurfRow::Rgb, out.as_mut_ptr()) };
    let p = c_ulong::from(row16[0]);
    assert_eq!(out, [unpack_red(R5G6B5, p), unpack_green(R5G6B5, p), unpack_blue(R5G6B5, p)]);
    assert_eq!(out[1], 0);
}

#[test]
fn a_scanline_in_packs_opaque_and_skips_the_padding() {
    let src = [1u8, 2, 3, 4, 5, 6];
    let mut px = [0u32; 4];
    // SAFETY: two pixels and a skip of one fit
    let next = unsafe { ee_ge_surf_row_in(A4R4G4B4, 32, src.as_ptr(), 6, false, px.as_mut_ptr().cast(), 1) };
    assert_eq!(px[..2], [0xff01_0203, 0xff04_0506]);
    assert_eq!(next as usize - px.as_ptr() as usize, 12);
    let mut px = [0u16; 3];
    let src = [0xffu8, 0x80, 0x10];
    // SAFETY: one pixel and a skip of two fit
    let next = unsafe { ee_ge_surf_row_in(R5G6B5, 16, src.as_ptr(), 3, false, px.as_mut_ptr().cast(), 2) };
    assert_eq!(c_ulong::from(px[0]), pack(R5G6B5, 0xff, 0x80, 0x10, 0xff) & 0xffff);
    assert_eq!(next as usize - px.as_ptr() as usize, 6);
}

#[test]
fn a_scanline_in_merges_the_alpha_and_reads_past_the_line_as_zero() {
    let mut px = [0x0011_2233u32];
    // SAFETY: one pixel
    unsafe { ee_ge_surf_row_in(A8R8G8B8, 32, [0x9au8, 0, 0].as_ptr(), 3, true, px.as_mut_ptr().cast(), 0) };
    assert_eq!(px[0], 0x9a11_2233);
    let mut px = [0xffffu16];
    // SAFETY: one pixel
    unsafe { ee_ge_surf_row_in(A4R4G4B4, 16, [0x30u8, 0, 0].as_ptr(), 3, true, px.as_mut_ptr().cast(), 0) };
    assert_eq!(px[0], 0x3fff);
    // a one-component line of two bytes: the second pixel's green and blue are past it
    let mut px = [0u32; 2];
    // SAFETY: two pixels
    unsafe { ee_ge_surf_row_in(A8R8G8B8, 32, [7u8, 8].as_ptr(), 2, false, px.as_mut_ptr().cast(), 0) };
    assert_eq!(px[0], 0xff07_0800);
    assert_eq!(px[1], 0);
}

#[test]
fn a_png_is_stored_into_the_top_left_with_or_without_alpha() {
    let src = [0x10u8, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80];
    let mut px = [0u32; 6];
    // SAFETY: a 1 x 2 picture into a surface of three pixels a row
    unsafe { ee_ge_surf_png_store(A8R8G8B8, 32, false, src.as_ptr(), 1, 2, px.as_mut_ptr().cast(), 12) };
    assert_eq!(px, [0x4030_2010, 0, 0, 0x8070_6050, 0, 0]);
    let mut px = [0u16; 2];
    // SAFETY: a 2 x 1 picture
    unsafe { ee_ge_surf_png_store(A4R4G4B4, 16, true, src.as_ptr(), 2, 1, px.as_mut_ptr().cast(), 4) };
    assert_eq!(c_ulong::from(px[0]), pack(A4R4G4B4, 0x30, 0x20, 0x10, 0xff));
    assert_eq!(c_ulong::from(px[1]), pack(A4R4G4B4, 0x70, 0x60, 0x50, 0xff));
}
