//! `DX7Surface::Copy32BitWithFilter` / `Copy16BitWithFilter`: bilinear rectangle copy, pixel format through
//! `GEUnpack*`/`GEPackColor` (C++-owned tables, reached through [`PixelOps`]).
//!
//! Golden: `golden_filter.txt`, printed by a scratch copy of the C++ loop (`DX7Surface.cpp`, the two functions, with
//! the `GEPixelFormat` table of `Graphics Engine.cpp`), compiled with clang-cl -O1.

use crate::vertex::float_to_long;

/// `GEUnpack*` and `GEPackColor` for one pixel format.
pub trait PixelOps {
    fn unpack_red(&self, pixel: u32) -> u8;
    fn unpack_green(&self, pixel: u32) -> u8;
    fn unpack_blue(&self, pixel: u32) -> u8;
    fn unpack_alpha(&self, pixel: u32) -> u8;
    fn pack(&self, red: i32, green: i32, blue: i32, alpha: i32) -> u32;
}

/// A source/destination rectangle in float pixels (`GERectangle`'s left, top, right, bottom, right/bottom exclusive).
#[derive(Clone, Copy, Debug)]
pub struct Rect {
    pub left: f32,
    pub top: f32,
    pub right: f32,
    pub bottom: f32,
}

impl Rect {
    fn width(&self) -> f32 {
        self.right - self.left
    }
    fn height(&self) -> f32 {
        self.bottom - self.top
    }
}

/// The bilinear copy. `pitch` in pixels (words). `read`/`write` access the surfaces' pixels at a word index.
/// Faithful to the C++, every float evaluated in the same order (`(x - left + 1) * xstep` etc.).
#[allow(clippy::too_many_arguments)]
pub fn copy_with_filter<P: PixelOps>(
    ops: &P,
    source: Rect,
    destination: Rect,
    source_pitch: u32,
    destination_pitch: u32,
    mut read: impl FnMut(i64) -> u32,
    mut write: impl FnMut(u64, u32),
) {
    // (unsigned long)float: truncation
    let top = destination.top as u32;
    let bottom = destination.bottom as u32;
    let left = destination.left as u32;
    let right = destination.right as u32;
    let x_step = 1.0f32 / destination.width();
    let y_step = 1.0f32 / destination.height();
    let x_offset = 1.0f32 / (2.0f32 * destination.width());
    let y_offset = 1.0f32 / (2.0f32 * destination.height());
    let min_x = source.left as i32;
    let max_x = source.right as i32 - 1;
    let min_y = source.top as i32;
    let max_y = source.bottom as i32 - 1;
    for y in top..bottom {
        for x in left..right {
            let tx = source.width() * ((x.wrapping_sub(left).wrapping_add(1)) as f32 * x_step - x_offset) + source.left
                - 0.5;
            let ty =
                source.height() * ((y.wrapping_sub(top).wrapping_add(1)) as f32 * y_step - y_offset) + source.top - 0.5;
            let mut sx = float_to_long(tx);
            let mut sy = float_to_long(ty);
            let mut fx = tx - sx as f32;
            let mut fy = ty - sy as f32;
            if fx < 0.0 {
                fx += 1.0;
                sx -= 1;
            }
            if fy < 0.0 {
                fy += 1.0;
                sy -= 1;
            }
            if sx < min_x {
                sx = min_x;
                fx = 0.0;
            }
            if sx > max_x {
                sx = max_x;
            }
            if sy < min_y {
                sy = min_y;
                fy = 0.0;
            }
            if sy > max_y {
                sy = max_y;
            }
            let next_x: i64 = i64::from(sx < max_x);
            let next_y: i64 = if sy < max_y { source_pitch as i64 } else { 0 };
            let ix = 1.0f32 - fx;
            let iy = 1.0f32 - fy;
            let index = (sy as i64) * (source_pitch as i64) + sx as i64;
            let p00 = read(index);
            let p01 = read(index + next_x);
            let p10 = read(index + next_y);
            let p11 = read(index + next_y + next_x);

            let r1 = f32::from(ops.unpack_red(p00));
            let g1 = f32::from(ops.unpack_green(p00));
            let b1 = f32::from(ops.unpack_blue(p00));
            let a1 = f32::from(ops.unpack_alpha(p00));
            let r2 = f32::from(ops.unpack_red(p01));
            let g2 = f32::from(ops.unpack_green(p01));
            let b2 = f32::from(ops.unpack_blue(p01));
            let a2 = f32::from(ops.unpack_alpha(p01));
            let top_r = r1 * ix + r2 * fx;
            let top_g = g1 * ix + g2 * fx;
            let top_b = b1 * ix + b2 * fx;
            let top_a = a1 * ix + a2 * fx;
            let r1 = f32::from(ops.unpack_red(p10));
            let g1 = f32::from(ops.unpack_green(p10));
            let b1 = f32::from(ops.unpack_blue(p10));
            let a1 = f32::from(ops.unpack_alpha(p10));
            let r2 = f32::from(ops.unpack_red(p11));
            let g2 = f32::from(ops.unpack_green(p11));
            let b2 = f32::from(ops.unpack_blue(p11));
            let a2 = f32::from(ops.unpack_alpha(p11));
            let red = top_r * iy + (r1 * ix + r2 * fx) * fy;
            let green = top_g * iy + (g1 * ix + g2 * fx) * fy;
            let blue = top_b * iy + (b1 * ix + b2 * fx) * fy;
            let alpha = top_a * iy + (a1 * ix + a2 * fx) * fy;
            let packed = ops.pack(float_to_long(red), float_to_long(green), float_to_long(blue), float_to_long(alpha));
            write(u64::from(y) * u64::from(destination_pitch) + u64::from(x), packed);
        }
    }
}

#[cfg(test)]
pub(crate) mod test_ops {
    use super::PixelOps;

    /// `sPixelFormats` of `Graphics Engine.cpp`: (redShift, redPos, greenShift, greenPos, blueShift, bluePos, alphaShift, alphaPos).
    const FORMATS: [[u32; 8]; 8] = [
        [0, 16, 0, 8, 0, 0, 0, 24],
        [0, 16, 0, 8, 0, 0, 8, 0],
        [4, 8, 4, 4, 4, 0, 4, 12],
        [3, 10, 3, 5, 3, 0, 7, 15],
        [3, 11, 3, 6, 3, 1, 7, 0],
        [3, 10, 3, 5, 3, 0, 8, 0],
        [3, 11, 3, 6, 3, 1, 8, 0],
        [3, 11, 2, 5, 3, 0, 8, 0],
    ];

    pub struct TableOps(pub usize);

    impl PixelOps for TableOps {
        fn unpack_red(&self, c: u32) -> u8 {
            let f = FORMATS[self.0];
            ((c >> f[1]) << f[0]) as u8
        }
        fn unpack_green(&self, c: u32) -> u8 {
            let f = FORMATS[self.0];
            ((c >> f[3]) << f[2]) as u8
        }
        fn unpack_blue(&self, c: u32) -> u8 {
            let f = FORMATS[self.0];
            ((c >> f[5]) << f[4]) as u8
        }
        fn unpack_alpha(&self, c: u32) -> u8 {
            let f = FORMATS[self.0];
            ((c >> f[7]) << f[6]) as u8
        }
        fn pack(&self, r: i32, g: i32, b: i32, a: i32) -> u32 {
            let f = FORMATS[self.0];
            let s = |v: i32, sh: u32, pos: u32| ((v as u32) >> sh) << pos;
            s(a, f[6], f[7]) | s(b, f[4], f[5]) | s(g, f[2], f[3]) | s(r, f[0], f[1])
        }
    }
}

#[cfg(test)]
mod tests {
    use super::test_ops::TableOps;
    use super::*;

    fn run32(format: usize, src: &[u32], w: u32, s: Rect, d: Rect, out_w: u32, out_h: u32) -> Vec<u32> {
        let mut out = vec![0u32; (out_w * out_h) as usize];
        copy_with_filter(&TableOps(format), s, d, w, out_w, |i| src[i as usize], |i, v| out[i as usize] = v);
        out
    }

    // golden_filter.txt: format, srcw, srch, src pixels..., dstw, dsth, dst pixels... (hex)
    #[test]
    fn matches_cpp_golden() {
        let text = include_str!("golden_filter.txt");
        let mut lines = text.lines();
        let mut cases = 0;
        while let Some(line) = lines.next() {
            let header: Vec<u32> =
                line.split(' ').filter(|s| !s.is_empty()).map(|s| u32::from_str_radix(s, 16).unwrap()).collect();
            if header.len() < 5 {
                continue;
            }
            let (format, sw, sh, dw, dh) = (header[0] as usize, header[1], header[2], header[3], header[4]);
            let src: Vec<u32> = lines.next().unwrap().split(' ').map(|s| u32::from_str_radix(s, 16).unwrap()).collect();
            let expected: Vec<u32> =
                lines.next().unwrap().split(' ').map(|s| u32::from_str_radix(s, 16).unwrap()).collect();
            let s = Rect { left: 0.0, top: 0.0, right: sw as f32, bottom: sh as f32 };
            let d = Rect { left: 0.0, top: 0.0, right: dw as f32, bottom: dh as f32 };
            let got = run32(format, &src, sw, s, d, dw, dh);
            assert_eq!(got, expected, "case {cases} format {format}");
            cases += 1;
        }
        assert!(cases >= 4, "golden cases read: {cases}");
    }

    #[test]
    fn identity_rect_one_to_one_copies_pixels_format1() {
        // 2x2 source mapped 1:1 onto a 2x2 destination at format X8R8G8B8: bilinear at exact centres -> unchanged
        let src = [0x0102_0304u32, 0x0a0b_0c0d, 0x1112_1314, 0x2122_2324];
        let s = Rect { left: 0.0, top: 0.0, right: 2.0, bottom: 2.0 };
        let got = run32(1, &src, 2, s, s, 2, 2);
        assert_eq!(got, [0x0002_0304, 0x000b_0c0d, 0x0012_1314, 0x0022_2324]);
    }

    #[test]
    fn taps_clamp_inside_rect() {
        let src = [0x00ff_0000u32];
        let s = Rect { left: 0.0, top: 0.0, right: 1.0, bottom: 1.0 };
        let d = Rect { left: 0.0, top: 0.0, right: 3.0, bottom: 3.0 };
        let got = run32(1, &src, 1, s, d, 3, 3);
        assert!(got.iter().all(|&p| p == 0x00ff_0000), "{got:x?}");
    }
}
