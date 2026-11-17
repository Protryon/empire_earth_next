//! imgfilter's loops: the golden of the original statements (`golden_imgfilter.txt`, recorded by
//! tests/won/won_imgfilter_test.cpp `--print` over the C++ before the port) replayed over images rebuilt from its
//! dumps, as the ported ImageFilters.cpp calls the functions, and unit tests of the corners.

use super::*;

/// An image of the golden: its kind, size, flag (8-bit: the transparent index; 32-bit: transparency on), bytes
/// (rows padded as the C++ allocated them) and the palette.
#[derive(Clone, Debug)]
struct Img {
    kind: i32,
    w: i32,
    h: i32,
    flag: i32,
    bytes: Vec<u8>,
    pal: Vec<u32>,
}

fn pitch(kind: i32, w: i32) -> i32 {
    let p = match kind {
        0 => (w + 7) / 8,
        1 => w,
        _ => return w * 4,
    };
    (p + 3) / 4 * 4
}

impl Img {
    fn new(kind: i32, w: i32, h: i32) -> Img {
        Img {
            kind,
            w,
            h,
            flag: if kind == 1 { -1 } else { 0 },
            bytes: vec![0; (pitch(kind, w) * h) as usize],
            pal: vec![],
        }
    }
    fn px(&mut self) -> EeWonRawPixels {
        EeWonRawPixels {
            kind: self.kind,
            data: self.bytes.as_mut_ptr(),
            width: self.w,
            height: self.h,
            pitch: pitch(self.kind, self.w),
        }
    }
    fn parse(s: &str) -> Img {
        let mut it = s.split(' ');
        let kind: i32 = it.next().unwrap().parse().unwrap();
        let (w, h) = it.next().unwrap().split_once('x').unwrap();
        let mut img = Img::new(kind, w.parse().unwrap(), h.parse().unwrap());
        let mut next = it.next().unwrap_or("");
        if kind != 0 {
            img.flag = next[1..].parse().unwrap();
            next = it.next().unwrap_or("");
        }
        img.bytes = (0..next.len() / 2).map(|i| u8::from_str_radix(&next[2 * i..2 * i + 2], 16).unwrap()).collect();
        if it.next() == Some("pal") {
            img.pal = it.map(|c| u32::from_str_radix(c, 16).unwrap()).collect();
        }
        img
    }
    fn dump(&self) -> String {
        let mut s = format!("{} {}x{}", self.kind, self.w, self.h);
        if self.kind == 1 {
            s += &format!(" t{}", self.flag);
        }
        if self.kind == 2 {
            s += &format!(" d{}", self.flag);
        }
        s += " ";
        for b in &self.bytes {
            s += &format!("{b:02x}");
        }
        s
    }
    fn dump_pal(&self) -> String {
        let mut s = "pal".to_string();
        for c in &self.pal {
            s += &format!(" {c:x}");
        }
        s
    }
    /// `f` over the 32-bit pixels (copied out and back: the bytes are not aligned for u32).
    fn with32(&mut self, f: impl FnOnce(&mut [u32])) {
        let n = (self.w * self.h) as usize;
        let mut px: Vec<u32> =
            (0..n).map(|i| u32::from_le_bytes(self.bytes[4 * i..4 * i + 4].try_into().unwrap())).collect();
        f(&mut px);
        for (i, p) in px.iter().enumerate() {
            self.bytes[4 * i..4 * i + 4].copy_from_slice(&p.to_le_bytes());
        }
    }
    fn pixel32(&self, x: i32, y: i32) -> u32 {
        let i = ((y * self.w + x) * 4) as usize;
        u32::from_le_bytes([self.bytes[i], self.bytes[i + 1], self.bytes[i + 2], self.bytes[i + 3]])
    }
}

/// One filter of the golden over its input, as the ported ImageFilters.cpp runs it: the output line.
fn run(label: &str, input: &str) -> String {
    let mut img = Img::parse(input);
    let f: Vec<&str> = label.split(' ').collect();
    let n = |i: usize| -> i32 { f[i].parse().unwrap() };
    match f[0] {
        "crop" => {
            let (x, y, w, h) = (n(1), n(2), n(3), n(4));
            let mut dst = Img::new(img.kind, w, h);
            dst.flag = img.flag;
            let bpp = if img.kind == 1 { 1 } else { 4 };
            let (s, d) = (img.px(), dst.px());
            // SAFETY: the rectangle is inside the source
            unsafe { crop_rows(&s, &d, x * bpp, y, w * bpp, h) };
            dst.dump()
        }
        "scale" => {
            let (mut w, mut h) = (n(4), n(5));
            let same = check_size(n(1) != 0, n(2), n(3), &mut w, &mut h, img.w, img.h);
            let mut out = format!("size {} {} {}", same as i32, w, h);
            if !same && img.w > 0 && img.h > 0 {
                let mut dst = Img::new(img.kind, w, h);
                dst.flag = img.flag;
                let (s, d) = (img.px(), dst.px());
                // SAFETY: both images' buffers as described
                unsafe { scale(&s, &d) };
                out += " ";
                out += &dst.dump();
            }
            out
        }
        "tint" => {
            let (r, g, b) = (n(1), n(2), n(3));
            if img.kind == 1 {
                tint(&mut img.pal, r, g, b, false);
            } else if img.kind == 2 {
                img.with32(|p| tint(p, r, g, b, true));
            }
            format!("{} {}", img.dump(), img.dump_pal())
        }
        "transparent" => {
            let (use_sample, sample) = (n(1) != 0, n(2));
            let mut color = u32::from_str_radix(f[3], 16).unwrap();
            if img.kind != 0 && !(use_sample && (img.h <= 0 || img.w <= 0)) {
                let mut done = false;
                if use_sample {
                    let (x, y) = sample_place(sample, img.w, img.h);
                    if img.kind == 1 {
                        img.flag = img.bytes[(y * pitch(1, img.w) + x) as usize] as i32;
                        done = true;
                    } else {
                        color = img.pixel32(x, y);
                    }
                }
                if !done && img.kind == 1 {
                    let i = palette_find(&img.pal, color);
                    if i >= 0 {
                        img.flag = i;
                    }
                } else if !done {
                    img.flag = 1;
                    img.with32(|p| transparent32(p, color));
                }
            }
            img.dump()
        }
        "trim" => {
            let has = if img.kind == 1 { img.flag >= 0 } else { img.flag != 0 };
            if !has {
                return format!("trim 2 0 0 {} {}", img.w, img.h);
            }
            let p = img.px();
            // SAFETY: an 8- or 32-bit image as described
            match unsafe { trim(&p, img.flag as u8) } {
                Some((l, t, w, h)) => format!("trim 1 {l} {t} {w} {h}"),
                None => format!("trim 0 0 0 {} {}", img.w, img.h),
            }
        }
        k => panic!("unknown filter {k}"),
    }
}

const GOLDEN: &str = include_str!("golden_imgfilter.txt");

fn replay(text: &str) -> (usize, usize) {
    let lines: Vec<&str> = text.lines().collect();
    let (mut count, mut bad) = (0, 0);
    for t in lines.chunks(3) {
        let (label, input, want) = (&t[0][2..], &t[1][2..], &t[2][2..]);
        count += 1;
        if run(label, input) != want {
            bad += 1;
        }
    }
    (count, bad)
}

#[test]
fn golden_replays() {
    let (count, bad) = replay(GOLDEN);
    assert!(count > 300, "{count}");
    assert_eq!(bad, 0);
}

#[test]
fn golden_fails_on_a_changed_trim() {
    let i = GOLDEN.find("O trim 1 ").expect("a trim");
    let mut changed = GOLDEN.to_string();
    changed.replace_range(i + 9..i + 10, "7");
    assert_ne!(changed, GOLDEN);
    assert_eq!(replay(&changed).1, 1);
}

#[test]
fn check_size_is_at_least_one_pixel() {
    let (mut w, mut h) = (5, 5);
    assert!(!check_size(true, 0, -10, &mut w, &mut h, 20, 30));
    assert_eq!((w, h), (1, 1));
    let (mut w, mut h) = (0, 0);
    assert!(!check_size(true, 50, 200, &mut w, &mut h, 4, 3));
    assert_eq!((w, h), (2, 6));
    assert!(check_size(true, 100, 100, &mut w, &mut h, 4, 3));
    let (mut w, mut h) = (4, 3);
    assert!(check_size(false, 0, 0, &mut w, &mut h, 4, 3));
}

#[test]
fn tint_clamps_each_channel_and_keeps_alpha_only_when_asked() {
    assert_eq!(tint_color(0xff80_4020, 300, 100, 50), 0x00ff_4010);
    let mut c = [0xff80_4020u32];
    tint(&mut c, 100, 100, 100, true);
    assert_eq!(c[0], 0xff80_4020);
    tint(&mut c, 100, 100, 100, false);
    assert_eq!(c[0], 0x0080_4020);
}

#[test]
fn transparent_pass_clears_the_colour_and_makes_the_rest_opaque() {
    let mut p = [0x0012_3456u32, 0x0000_0001, 0x8000_0001];
    transparent32(&mut p, 0x0000_0001);
    assert_eq!(p, [0xff12_3456, 0, 0xff00_0001]);
    assert_eq!(sample_place(3, 10, 4), (9, 3));
    assert_eq!(sample_place(7, 10, 4), (0, 0));
    assert_eq!(palette_find(&[1, 2, 2], 2), 1);
    assert_eq!(palette_find(&[], 2), -1);
}

#[test]
fn trim_finds_the_filled_rectangle() {
    let mut img = Img::new(1, 6, 5);
    let p = pitch(1, 6) as usize;
    img.bytes.iter_mut().for_each(|b| *b = 9);
    img.bytes[p + 2] = 1;
    img.bytes[3 * p + 4] = 1;
    let px = img.px();
    // SAFETY: the image's buffer
    assert_eq!(unsafe { trim(&px, 9) }, Some((2, 1, 3, 3)));
    // SAFETY: as above
    assert_eq!(unsafe { trim(&px, 1) }, Some((0, 0, 6, 5)));
    img.bytes.iter_mut().for_each(|b| *b = 9);
    let px = img.px();
    // SAFETY: as above
    assert_eq!(unsafe { trim(&px, 9) }, None);
}

#[test]
fn scale_of_one_bit_rows_walks_the_bits() {
    let mut src = Img::new(0, 3, 1);
    src.bytes[0] = 0b1010_0000;
    let mut dst = Img::new(0, 6, 2);
    let (s, d) = (src.px(), dst.px());
    // SAFETY: both buffers as described
    unsafe { scale(&s, &d) };
    assert_eq!(dst.bytes[0], 0b1100_1100);
    assert_eq!(dst.bytes[4], 0b1100_1100);
}

#[test]
fn trim_of_a_single_filled_row_at_the_top() {
    let mut img = Img::new(1, 4, 3);
    img.bytes.iter_mut().for_each(|b| *b = 9);
    img.bytes[1] = 2;
    let px = img.px();
    // SAFETY: the image's buffer
    assert_eq!(unsafe { trim(&px, 9) }, Some((1, 0, 1, 1)));
}

// ---- StretchImageFilter's pixel work (R6 WON round 37): `golden_stretchpx.txt`, recorded by
// tests/won/won_stretchpx_test.cpp `--print` over the original CopyRawImage and RawStretch, replayed over images
// rebuilt from its seeds through guidraw's plan and `stretch_run`

fn fnv_bytes(b: &[u8]) -> u64 {
    let mut h: u64 = 1469598103934665603;
    for &x in b {
        h ^= x as u64;
        h = h.wrapping_mul(1099511628211);
    }
    h
}

#[test]
fn stretch_golden_replays() {
    use crate::guidraw::{stretch_filter, EeWonStretchFilter};
    let text = include_str!("golden_stretchpx.txt");
    let mut n = 0;
    for line in text.lines() {
        let (head, want) = line.split_once(" = ").unwrap();
        let v: Vec<i64> = head.split(' ').map(|x| x.parse().unwrap()).collect();
        let (kind, sw, sh, seed) = (v[0] as i32, v[1] as i32, v[2] as i32, v[3] as u32);
        let f = EeWonStretchFilter {
            width: v[4] as i32,
            height: v[5] as i32,
            left: v[6] as i32,
            right: v[7] as i32,
            top: v[8] as i32,
            bottom: v[9] as i32,
            src_width: sw,
            src_height: sh,
            tile_horz: v[10] != 0,
            tile_vert: v[11] != 0,
            skip_center: v[12] != 0,
        };
        let mut src = Img::new(kind, sw, sh);
        let mut s = seed;
        for b in src.bytes.iter_mut() {
            s = s.wrapping_mul(1103515245).wrapping_add(12345);
            *b = (s >> 16) as u8;
        }
        let (w, h, ops) = stretch_filter(&f);
        let mut dst = Img::new(kind, w, h);
        // SAFETY: both images hold their rows; the plan stays inside them (the borders are inside the source)
        unsafe { stretch_run(&src.px(), &dst.px(), &ops) };
        let got = format!("{} {} {} {:016x}", w, h, ops.len(), fnv_bytes(&dst.bytes));
        assert_eq!(got, want, "{line}");
        n += 1;
    }
    assert_eq!(n, 300);
}

#[test]
fn stretch_copies_a_corner_and_stretches_a_side() {
    // a 2 x 1 8-bit source: a copy of pixel 1 to the destination's 0, then the 1-pixel tile stretched over 3
    let mut src = Img::new(1, 2, 1);
    src.bytes[0] = 5;
    src.bytes[1] = 9;
    let mut dst = Img::new(1, 4, 1);
    let ops = vec![
        EeWonDrawOp { kind: EE_WON_DRAW_COPY_PIXELS, a: 0, b: 0, c: 0, d: 1, e: 0, f: 1, g: 1, ..Default::default() },
        EeWonDrawOp { kind: EE_WON_DRAW_STRETCH_PIXELS, a: 1, b: 4, c: 0, d: 1, e: 0, f: 0, g: 1, h: 1 },
    ];
    // SAFETY: as above
    unsafe { stretch_run(&src.px(), &dst.px(), &ops) };
    assert_eq!(&dst.bytes[..4], &[9, 5, 5, 5]);
}

// ---- the raw images' transparency masks (R6 WON round 37): `golden_imgmask.txt`, recorded by
// tests/won/won_imgmask_test.cpp `--print` over Image.cpp's GetNative loops, replayed over images rebuilt from its
// seeds

#[test]
fn mask_golden_replays() {
    let text = include_str!("golden_imgmask.txt");
    let mut n = 0;
    for line in text.lines() {
        let (head, want) = line.split_once(" = ").unwrap();
        let v: Vec<i64> = head.split(' ').map(|x| x.parse().unwrap()).collect();
        let (bits, w, h, transparent, seed) = (v[0], v[1] as i32, v[2] as i32, v[3] as i32, v[4] as u32);
        // the mask: RawImage1's rows (the unpadded bytes rounded up to four), filled as `new` left them
        let mpitch = (w / 8 + i32::from(w % 8 != 0) + 3) / 4 * 4;
        let mut mask = vec![0u8; (mpitch * h) as usize + 1];
        let mut s = seed;
        let mut next = || {
            s = s.wrapping_mul(1103515245).wrapping_add(12345);
            s
        };
        for b in mask.iter_mut() {
            *b = (next() >> 16) as u8;
        }
        let got = if bits == 32 {
            let mut px: Vec<u32> = (0..w * h)
                .map(|_| {
                    let s = next();
                    let mut v = s ^ (s << 11);
                    if (s >> 7) % 3 == 0 {
                        v &= 0x00ff_ffff;
                    }
                    v
                })
                .collect();
            // SAFETY: the pixels and the mask's rows
            unsafe { mask32(px.as_mut_ptr(), w, h, mask.as_mut_ptr(), mpitch) };
            let bytes: Vec<u8> = px.iter().flat_map(|p| p.to_le_bytes()).collect();
            fnv_chain(&mask[..(mpitch * h) as usize], fnv_chain(&bytes, 1469598103934665603))
        } else {
            let pitch = (w + 3) / 4 * 4;
            let px: Vec<u8> = (0..pitch * h)
                .map(|_| {
                    let s = next();
                    if (s >> 9) % 3 == 0 {
                        transparent as u8
                    } else {
                        (s >> 16) as u8
                    }
                })
                .collect();
            // SAFETY: as above
            unsafe { mask8(px.as_ptr(), w, h, pitch, transparent, mask.as_mut_ptr(), mpitch) };
            fnv_chain(&mask[..(mpitch * h) as usize], 1469598103934665603)
        };
        assert_eq!(format!("{got:016x}"), want, "{line}");
        n += 1;
    }
    assert_eq!(n, 300);
}

fn fnv_chain(b: &[u8], mut h: u64) -> u64 {
    for &x in b {
        h ^= x as u64;
        h = h.wrapping_mul(1099511628211);
    }
    h
}

#[test]
fn masks_set_the_transparent_pixels_high_bit_first() {
    let mut px = [0x00ff_ffffu32, 0xff00_0000, 0x0000_0001];
    let mut mask = [0xffu8; 4];
    // SAFETY: three pixels in one row, a 4-byte mask row
    unsafe { mask32(px.as_mut_ptr(), 3, 1, mask.as_mut_ptr(), 4) };
    assert_eq!(px, [0, 0xff00_0000, 0]);
    assert_eq!(mask[0], 0b1011_1111);
    let p8 = [7u8, 3, 7, 7];
    let mut m8 = [0u8; 4];
    // SAFETY: as above; an index of 263 matches nothing (the original compared an unsigned char with the int)
    unsafe { mask8(p8.as_ptr(), 4, 1, 4, 263, m8.as_mut_ptr(), 4) };
    assert_eq!(m8[0], 0);
    // SAFETY: as above
    unsafe { mask8(p8.as_ptr(), 4, 1, 4, 7, m8.as_mut_ptr(), 4) };
    assert_eq!(m8[0], 0b1011_0000);
}
