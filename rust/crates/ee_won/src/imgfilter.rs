//! WONGUI's image filters' pixel work in Rust (R6 WON round 36): ImageFilters.cpp's loops over the raw images'
//! pixels. ScaleImageFilter's size (CheckSize) and its nearest-neighbour scale of 1-, 8- and 32-bit images,
//! CropImageFilter's row copies, TintImageFilter's channel percentages (a palette's colours, or a 32-bit image's
//! pixels with their alpha kept), TransparentImageFilter's corner sample, palette search and 32-bit alpha pass, and
//! TrimImageFilter's transparent border search.
//!
//! The C++ keeps the filter classes (the configs make them and the images hold them), the images (`new`
//! RawImage1/8/32, the palette's Duplicate, the reference counts) and the dispatch on the image's type; each
//! loop is one call here over the image's pixel buffer as the original walked it (an 8-bit or 1-bit image's rows
//! `pitch` bytes apart, a 32-bit image's rows `width` pixels apart).
//!
//! Ints wrap (the game builds with -fwrapv); a division by zero is the original's fault (a scale to a zero width
//! cannot happen: CheckSize makes the size at least 1).

/// One raw image's pixels: `kind` 0 (1-bit), 1 (8-bit) or 2 (32-bit), its size and its row pitch in bytes.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeWonRawPixels {
    pub kind: i32,
    pub data: *mut u8,
    pub width: i32,
    pub height: i32,
    pub pitch: i32,
}

/// `ScaleImageFilter::CheckSize`: the target size (a percentage of the image when `is_percent`, at least 1 x 1),
/// written back; true when it is the image's size.
pub fn check_size(is_percent: bool, pw: i32, ph: i32, w: &mut i32, h: &mut i32, img_w: i32, img_h: i32) -> bool {
    if is_percent {
        *w = img_w.wrapping_mul(pw) / 100;
        *h = img_h.wrapping_mul(ph) / 100;
    }
    if *w <= 0 {
        *w = 1;
    }
    if *h <= 0 {
        *h = 1;
    }
    *w == img_w && *h == img_h
}

/// The nearest-neighbour walk of ScalePixels / ScaleBits: for each target row, the source row it reads; for each
/// target pixel, the source column. The original advanced a row pointer by `cx / width` after each pixel and read
/// the next value there (past the row's end after its last pixel, a value it never used); this reads only the
/// pixels it writes.
fn scale_walk(src_w: i32, src_h: i32, w: i32, h: i32, mut put: impl FnMut(i32, i32, i32, i32)) {
    let (mut cy, mut oy) = (0i32, 0i32);
    for y in 0..h {
        let (mut cx, mut ox) = (0i32, 0i32);
        for x in 0..w {
            put(x, y, ox, oy);
            cx = cx.wrapping_add(src_w);
            if cx < w {
                continue;
            }
            ox = ox.wrapping_add(cx / w);
            cx %= w;
        }
        cy = cy.wrapping_add(src_h);
        if cy >= h {
            oy = oy.wrapping_add(cy / h);
            cy %= h;
        }
    }
}

/// The scale of `src` into `dst` (the same kind, `dst`'s size the target's).
///
/// # Safety
/// Both buffers hold their images' rows as described.
pub unsafe fn scale(src: &EeWonRawPixels, dst: &EeWonRawPixels) {
    let (sp, dp) = (src.pitch as isize, dst.pitch as isize);
    scale_walk(src.width, src.height, dst.width, dst.height, |x, y, ox, oy| {
        let (x, y, ox, oy) = (x as isize, y as isize, ox as isize, oy as isize);
        // SAFETY: the walk stays inside both images (each target pixel's source pixel is inside the source)
        unsafe {
            match dst.kind {
                0 => {
                    let s = *src.data.offset(oy * sp + ox / 8) & (0x80u8 >> (ox % 8)) != 0;
                    let d = dst.data.offset(y * dp + x / 8);
                    let m = 0x80u8 >> (x % 8);
                    if s {
                        *d |= m;
                    } else {
                        *d &= !m;
                    }
                }
                1 => *dst.data.offset(y * dp + x) = *src.data.offset(oy * sp + ox),
                _ => {
                    let s = src.data.offset(oy * sp + ox * 4) as *const u32;
                    let d = dst.data.offset(y * dp + x * 4) as *mut u32;
                    d.write_unaligned(s.read_unaligned());
                }
            }
        }
    });
}

/// `TintColor`: each channel times its percentage, at most 255; the alpha byte 0. The channel is a `DWORD`, so the
/// original's product and quotient are unsigned (a negative percentage wraps), then stored in an int and compared
/// signed.
pub fn tint_color(color: u32, r: i32, g: i32, b: i32) -> u32 {
    let red = (((color >> 16) & 0xff).wrapping_mul(r as u32) / 100) as i32;
    let green = (((color >> 8) & 0xff).wrapping_mul(g as u32) / 100) as i32;
    let blue = ((color & 0xff).wrapping_mul(b as u32) / 100) as i32;
    let red = red.min(255);
    let green = green.min(255);
    let blue = blue.min(255);
    ((((red << 8) | green) << 8) | blue) as u32
}

/// TintImageFilter over `n` colours: a palette's (`keep_alpha` false: the alpha byte cleared, as the original
/// stored TintColor's value) or a 32-bit image's pixels (`keep_alpha`: the alpha byte kept).
pub fn tint(colors: &mut [u32], r: i32, g: i32, b: i32, keep_alpha: bool) {
    for c in colors {
        let t = tint_color(*c, r, g, b);
        *c = if keep_alpha { (*c & 0xff00_0000) | t } else { t };
    }
}

/// TransparentImageFilter's 32-bit pass: the colour's pixels alpha 0 (the whole pixel 0), the others opaque.
pub fn transparent32(pixels: &mut [u32], color: u32) {
    for p in pixels {
        *p = if *p == color { 0 } else { *p | 0xff00_0000 };
    }
}

/// TransparentImageFilter's corner sample (`UpperLeft`, `UpperRight`, `LowerLeft`, `LowerRight`; another value
/// samples the upper left).
pub fn sample_place(sample: i32, w: i32, h: i32) -> (i32, i32) {
    match sample {
        1 => (w.wrapping_sub(1), 0),
        2 => (0, h.wrapping_sub(1)),
        3 => (w.wrapping_sub(1), h.wrapping_sub(1)),
        _ => (0, 0),
    }
}

/// The first palette index holding `color`, or -1.
pub fn palette_find(colors: &[u32], color: u32) -> i32 {
    colors.iter().position(|c| *c == color).map_or(-1, |i| i as i32)
}

/// TrimImageFilter's search: the smallest rectangle (left, top, width, height) holding every pixel that is not
/// transparent (an 8-bit image's `transparent` index, a 32-bit pixel whose alpha byte is 0); None when every
/// pixel is (the original returned NULL).
///
/// # Safety
/// `img` holds its rows as described (kind 1 or 2).
pub unsafe fn trim(img: &EeWonRawPixels, transparent: u8) -> Option<(i32, i32, i32, i32)> {
    let (w, h) = (img.width, img.height);
    let filled = |x: i32, y: i32| -> bool {
        // SAFETY: (x, y) is inside the image
        unsafe {
            if img.kind == 1 {
                *img.data.offset(y as isize * img.pitch as isize + x as isize) != transparent
            } else {
                let p = img.data.offset((y as isize * w as isize + x as isize) * 4) as *const u32;
                p.read_unaligned() & 0xff00_0000 != 0
            }
        }
    };
    let top = (0..h).find(|&y| (0..w).any(|x| filled(x, y)))?;
    let mut bottom = h.wrapping_sub(1);
    while bottom > top && !(0..w).any(|x| filled(x, bottom)) {
        bottom -= 1;
    }
    let rows = bottom - top + 1;
    let left = (0..w).find(|&x| (top..top + rows).any(|y| filled(x, y)))?;
    let mut right = w.wrapping_sub(1);
    while right > left && !(top..top + rows).any(|y| filled(right, y)) {
        right -= 1;
    }
    Some((left, top, right - left + 1, rows))
}

/// CropPixels' rows: `height` rows of `row_bytes` bytes from (`x_bytes`, `y`) of the source into the
/// destination's rows.
///
/// # Safety
/// Both buffers hold the rows read and written.
pub unsafe fn crop_rows(src: &EeWonRawPixels, dst: &EeWonRawPixels, x_bytes: i32, y: i32, row_bytes: i32, height: i32) {
    for row in 0..height {
        // SAFETY: the contract
        unsafe {
            let from = src.data.offset((row + y) as isize * src.pitch as isize + x_bytes as isize);
            let to = dst.data.offset(row as isize * dst.pitch as isize);
            std::ptr::copy_nonoverlapping(from, to, row_bytes.max(0) as usize);
        }
    }
}

// ---- the C ABI

/// `ScaleImageFilter::CheckSize` over the filter's fields (`w`, `h` written back).
///
/// # Safety
/// `w` and `h` point at live ints.
#[no_mangle]
pub unsafe extern "C" fn ee_won_imgf_check_size(
    is_percent: bool,
    pw: i32,
    ph: i32,
    w: *mut i32,
    h: *mut i32,
    img_w: i32,
    img_h: i32,
) -> bool {
    // SAFETY: the contract
    unsafe { check_size(is_percent, pw, ph, &mut *w, &mut *h, img_w, img_h) }
}

/// The nearest-neighbour scale of `src` into `dst` (the same kind).
///
/// # Safety
/// Both describe live images.
#[no_mangle]
pub unsafe extern "C" fn ee_won_imgf_scale(src: *const EeWonRawPixels, dst: *const EeWonRawPixels) {
    // SAFETY: the contract
    unsafe { scale(&*src, &*dst) }
}

/// TintImageFilter over `n` colours (a palette's, or a 32-bit image's pixels with `keep_alpha`).
///
/// # Safety
/// `colors` holds `n` colours (or `n` is 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_imgf_tint(colors: *mut u32, n: usize, r: i32, g: i32, b: i32, keep_alpha: bool) {
    if n == 0 {
        return;
    }
    // SAFETY: the contract
    tint(unsafe { std::slice::from_raw_parts_mut(colors, n) }, r, g, b, keep_alpha)
}

/// TransparentImageFilter's 32-bit pass over `n` pixels.
///
/// # Safety
/// `pixels` holds `n` pixels (or `n` is 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_imgf_transparent32(pixels: *mut u32, n: usize, color: u32) {
    if n == 0 {
        return;
    }
    // SAFETY: the contract
    transparent32(unsafe { std::slice::from_raw_parts_mut(pixels, n) }, color)
}

/// TransparentImageFilter's sample place in a `w` x `h` image.
///
/// # Safety
/// `x` and `y` point at live ints.
#[no_mangle]
pub unsafe extern "C" fn ee_won_imgf_sample(sample: i32, w: i32, h: i32, x: *mut i32, y: *mut i32) {
    let (sx, sy) = sample_place(sample, w, h);
    // SAFETY: the contract
    unsafe {
        *x = sx;
        *y = sy;
    }
}

/// The first of `n` palette colours equal to `color`, or -1.
///
/// # Safety
/// `colors` holds `n` colours (or `n` is 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_imgf_palette_find(colors: *const u32, n: usize, color: u32) -> i32 {
    if n == 0 {
        return -1;
    }
    // SAFETY: the contract
    palette_find(unsafe { std::slice::from_raw_parts(colors, n) }, color)
}

/// TrimImageFilter's rectangle into `rect` (left, top, width, height); false when every pixel is transparent.
///
/// # Safety
/// `img` describes a live 8- or 32-bit image; `rect` has room for four ints.
#[no_mangle]
pub unsafe extern "C" fn ee_won_imgf_trim(img: *const EeWonRawPixels, transparent: u8, rect: *mut i32) -> bool {
    // SAFETY: the contract
    match unsafe { trim(&*img, transparent) } {
        Some((l, t, w, h)) => {
            // SAFETY: the contract
            unsafe {
                *rect = l;
                *rect.add(1) = t;
                *rect.add(2) = w;
                *rect.add(3) = h;
            }
            true
        }
        None => false,
    }
}

/// CropPixels' row copies.
///
/// # Safety
/// Both describe live images holding the rows read and written.
#[no_mangle]
pub unsafe extern "C" fn ee_won_imgf_crop(
    src: *const EeWonRawPixels,
    dst: *const EeWonRawPixels,
    x_bytes: i32,
    y: i32,
    row_bytes: i32,
    height: i32,
) {
    // SAFETY: the contract
    unsafe { crop_rows(&*src, &*dst, x_bytes, y, row_bytes, height) }
}

#[cfg(test)]
#[path = "imgfilter_tests.rs"]
mod tests;
