//! platimg: unit tests and the replay of the driver's golden (`golden_platimg.txt`, recorded by
//! tests/won/won_platimg_test.cpp `--print`: draws, raw copies, native images and texture pack copies with their
//! inputs by seed, and the hand cursor's hash).
use super::*;
use crate::surface::{EeWonSurfaceDraw, EE_WON_SURFACE_32};

fn next(s: &mut u32) -> u32 {
    *s = s.wrapping_mul(1103515245).wrapping_add(12345);
    *s ^ (*s >> 15)
}
fn fill_words(p: &mut [u32], mut seed: u32) {
    for v in p.iter_mut() {
        *v = next(&mut seed);
    }
}
fn fill_bytes(n: usize, mut seed: u32) -> Vec<u8> {
    (0..n).map(|_| (next(&mut seed) >> 8) as u8).collect()
}
fn fnv(b: &[u8]) -> u32 {
    b.iter().fold(2166136261u32, |h, &c| (h ^ c as u32).wrapping_mul(16777619))
}
fn words_bytes(w: &[u32]) -> Vec<u8> {
    w.iter().flat_map(|v| v.to_ne_bytes()).collect()
}

/// A PlatNativeImage of `w` x `h` logical pixels at scale `s`, its pixels from `seed`.
fn image(w: i32, h: i32, s: f32, seed: u32) -> EeWonSurface {
    let mut sf = EeWonSurface::new(EE_WON_SURFACE_32).unwrap();
    sf.create(pixels(w, s).max(0), pixels(h, s).max(0));
    let n = (sf.field(F::PixelsPerRow) * sf.field(F::Height)).max(0) as usize;
    if let Some(px) = sf.words_mut() {
        fill_words(&mut px[..n], seed);
    }
    sf
}

/// A mask of `mw` x `mh` from `seed` as the driver makes it.
fn mask_bits(mw: i32, mh: i32, seed: u32) -> (Vec<u8>, i32) {
    let bpr = ((mw + 7) / 8 + 3) / 4 * 4;
    let mut bits = fill_bytes((bpr * mh.max(0)) as usize + 1, seed);
    bits.truncate((bpr * mh.max(0)) as usize);
    (bits, bpr)
}

fn surface_hash(s: &mut EeWonSurface) -> u32 {
    if s.pixels().is_null() {
        return 0;
    }
    let n = (s.field(F::BytesPerRow) * s.field(F::Height)).max(0) as usize;
    fnv(&s.bytes()[..n])
}

#[test]
fn taps_exact_at_equal_scales() {
    let t = taps(4, 8, 2, 3, 2, 2.0, 2.0);
    let p: Vec<i32> = t.iter().map(|t| t.pixel0).collect();
    assert_eq!(p, vec![6, 7, 8, 9]);
    assert!(t.iter().all(|t| t.weight1 == 0));
    assert_eq!(t.iter().map(|t| t.logical).collect::<Vec<_>>(), vec![3, 3, 4, 4]);
}

#[test]
fn taps_bilinear_between_scales() {
    // a 1x source drawn at 2x: destination pixel 2's centre is source 1.25 - 0.5 = 0.75: pixel 0, three quarters
    // towards pixel 1
    let t = taps(0, 4, 0, 0, 2, 2.0, 1.0);
    assert_eq!((t[0].pixel0, t[0].pixel1, t[0].weight1), (0, 1, 0));
    assert_eq!((t[2].pixel0, t[2].pixel1, t[2].weight1), (0, 1, 192));
    // clamped to the source rectangle's last pixel
    assert_eq!((t[3].pixel0, t[3].pixel1), (1, 1));
}

#[test]
fn masked_blit_keeps_the_destination() {
    let mut d = vec![0x0011_1111u32; 4];
    let s = [0x0000_00ffu32, 0x0000_ff00];
    let bits = [0x80u8, 0, 0, 0];
    let m = Mask { bits: &bits, width: 2, height: 1, bpr: 4 };
    let src = Src { px: &s, pitch: 2, scale: 1.0, width: 2, height: 1, mask: Some(m) };
    let mut dst = Dst { px: &mut d, pitch: 4, scale: 1.0, clip: (0, 0, 3, 0) };
    draw(&mut dst, &src, 1, 0, 0, 0, 2, 1);
    // pixel 1: transparent (dest | src); pixel 2: opaque (src)
    assert_eq!(d, vec![0x0011_1111, 0x0011_11ff, 0x0000_ff00, 0x0011_1111]);
}

#[test]
fn pack_names_and_fit() {
    assert_eq!(
        pack_names(b"C:\\Game\\WONLOBBY RESOURCES\\sub\\ok.bmp"),
        vec![
            b"Data/Textures-4x/WONLobby Resources/sub/ok.bmp".to_vec(),
            b"Data/Textures-4x/WONLobby Resources/sub/ok.png".to_vec()
        ]
    );
    assert_eq!(pack_names(b"x/wonlobby resources/a.png"), vec![b"Data/Textures-4x/WONLobby Resources/a.png".to_vec()]);
    assert!(pack_names(b"Data/MyWONLobby Resources/x.bmp").is_empty());
    assert!(pack_names(b"Data/WONLobby Resources/").is_empty());
    assert_eq!(pack_names(b"wonlobby resources/a.b/c").len(), 1);
    assert_eq!(pack_factor(40, 40, 10, 10), 4);
    assert_eq!(pack_factor(41, 40, 10, 10), 0);
    assert_eq!(pack_factor(10, 10, 10, 10), 0);
    assert_eq!(pack_scale(4, 1), (2, 2));
    assert_eq!(pack_scale(4, 2), (4, 1));
    assert_eq!(pack_scale(3, 1), (3, 1));
}

#[test]
fn hand_cursor_planes() {
    let h = hand_cursor();
    assert_eq!(h[0], 0); // AND set, XOR clear: transparent
    let black = h.iter().filter(|&&p| p == 0xFF00_0000).count();
    let white = h.iter().filter(|&&p| p == 0xFFFF_FFFF).count();
    assert!(black > 0 && white > 0);
}

const GOLDEN: &str = include_str!("golden_platimg.txt");

fn nums(s: &str) -> Vec<f64> {
    s.split_whitespace().map(|t| t.parse().unwrap()).collect()
}

/// A raw image as the driver makes it (`MakeRaw`): its data, pitch, palette and transparent index.
fn make_raw(
    kind: i32,
    w: i32,
    h: i32,
    seed: u32,
    colors: i32,
    transparent: i32,
    pseed: u32,
) -> (Vec<u8>, i32, Vec<u32>, i32) {
    match kind {
        2 => {
            let mut v = vec![0u32; (w * h).max(0) as usize];
            fill_words(&mut v, seed);
            (words_bytes(&v), w * 4, Vec::new(), -1)
        }
        1 => {
            let mut pal = vec![0u32; colors.max(0) as usize];
            fill_words(&mut pal, pseed);
            let bpr = (w + 3) / 4 * 4;
            (fill_bytes((bpr * h) as usize, seed), bpr, pal, transparent)
        }
        _ => {
            let bpr = ((w / 8 + i32::from(w % 8 != 0)) + 3) / 4 * 4;
            (fill_bytes((bpr * h) as usize, seed), bpr, Vec::new(), -1)
        }
    }
}

/// `Describe` of a native image.
fn describe(w: i32, h: i32, scale: f32, s: &mut EeWonSurface, mask: Option<(i32, i32, &[u8])>) -> String {
    let m = match mask {
        Some((mw, mh, b)) => format!("{mw}x{mh}:{}", fnv(b)),
        None => "-".into(),
    };
    let (pw, ph) = (s.field(F::Width), s.field(F::Height));
    format!("{w}x{h} @{scale:.2} {pw}x{ph} {:08x} mask {m}", surface_hash(s))
}

/// A raw image's native image (a new black image, the raw's pixels into it).
fn native(kind: i32, w: i32, h: i32, seed: u32, colors: i32, transparent: i32, pseed: u32) -> EeWonSurface {
    let (data, pitch, pal, tr) = make_raw(kind, w, h, seed, colors, transparent, pseed);
    let mut sf = image(w, h, 1.0, 0);
    let pitch_px = sf.field(F::PixelsPerRow);
    if let Some(px) = sf.words_mut() {
        px.fill(0);
        let mut d = Dst { px, pitch: pitch_px, scale: 1.0, clip: (0, 0, w - 1, h - 1) };
        from_raw(kind, &data, pitch, w, h, &pal, tr, &mut d);
    }
    sf
}

#[test]
fn golden_replay() {
    let mut counts = [0usize; 5];
    for line in GOLDEN.lines() {
        let (head, want) = line.split_once(" = ").unwrap();
        let (kind, rest) = head.split_once(' ').unwrap_or((head, ""));
        match kind {
            "draw" => {
                let v = nums(rest);
                let (sw, sh, ss, sseed) = (v[0] as i32, v[1] as i32, v[2] as f32, v[3] as u32);
                let (mk, mw, mh, mseed) = (v[4] as i32, v[5] as i32, v[6] as i32, v[7] as u32);
                let (dw, dh, ds, dseed) = (v[8] as i32, v[9] as i32, v[10] as f32, v[11] as u32);
                let c: Vec<i32> = v[12..16].iter().map(|x| *x as i32).collect();
                let a: Vec<i32> = v[16..22].iter().map(|x| *x as i32).collect();
                let mut src = image(sw, sh, ss, sseed);
                let (bits, bpr) = mask_bits(mw, mh, mseed);
                let mut dst = EeWonSurface::new(EE_WON_SURFACE_32).unwrap();
                if dw >= 0 {
                    dst.create(dw, dh);
                    let n = (dst.field(F::PixelsPerRow) * dh) as usize;
                    fill_words(&mut dst.words_mut().unwrap()[..n], dseed);
                }
                dst.draw(EeWonSurfaceDraw::SetClipRect, c[0], c[1], c[2], c[3], 0, 0);
                let m = EeWonPlatMask { bits: bits.as_ptr(), width: mw, height: mh, bytes_per_row: bpr };
                let mp: *const EeWonPlatMask = if mk != 0 { &m } else { std::ptr::null() };
                // SAFETY: live surfaces and a mask over `bits`
                unsafe {
                    ee_won_plat_img_draw(&mut dst, ds, &mut src, ss, sw, sh, mp, a[0], a[1], a[2], a[3], a[4], a[5])
                };
                assert_eq!(format!("{:08x}", surface_hash(&mut dst)), want, "{line}");
                counts[0] += 1;
            }
            "raw" => {
                let v = nums(rest);
                let (sw, sh, ss, sseed) = (v[0] as i32, v[1] as i32, v[2] as f32, v[3] as u32);
                let (mk, mw, mh, mseed) = (v[4] as i32, v[5] as i32, v[6] as i32, v[7] as u32);
                let mut src = image(sw, sh, ss, sseed);
                let (bits, bpr) = mask_bits(mw, mh, mseed);
                let m = EeWonPlatMask { bits: bits.as_ptr(), width: mw, height: mh, bytes_per_row: bpr };
                let mp: *const EeWonPlatMask = if mk != 0 { &m } else { std::ptr::null() };
                let mut out = vec![0u32; (sw * sh) as usize];
                // SAFETY: a live surface, `out` of its logical size
                unsafe { ee_won_plat_img_raw(&mut src, ss, sw, sh, mp, out.as_mut_ptr()) };
                assert_eq!(format!("{sw}x{sh} {mk} {:08x}", fnv(&words_bytes(&out))), want, "{line}");
                counts[1] += 1;
            }
            "native" => {
                let v = nums(rest);
                let (k, w, h, seed) = (v[0] as i32, v[1] as i32, v[2] as i32, v[3] as u32);
                let (colors, tr, pseed) = (v[4] as i32, v[5] as i32, v[6] as u32);
                let mut sf = native(k, w, h, seed, colors, tr, pseed);
                assert_eq!(describe(w, h, 1.0, &mut sf, None), want, "{line}");
                counts[2] += 1;
            }
            "pack" => {
                replay_pack(rest, want);
                counts[3] += 1;
            }
            "hand" => {
                assert_eq!(format!("{:08x}", fnv(&words_bytes(&hand_cursor()))), want);
                counts[4] += 1;
            }
            _ => panic!("{line}"),
        }
    }
    assert_eq!(counts, [300, 200, 300, 300, 1]);
}

/// A pack copy: the names tried in turn (a missing file, or a decoder that throws, goes on to the next), the fit,
/// the copy reduced from the file's native image, the original's mask.
fn replay_pack(rest: &str, want: &str) {
    let (nums_part, tail) = rest.split_once(" [").unwrap();
    let v = nums(nums_part);
    let (w, h, mk, mseed, shift, manager) =
        (v[0] as i32, v[1] as i32, v[2] as i32, v[4] as u32, v[5] as i32, v[6] as i32);
    let (path, mut files_part) = tail.split_once(']').unwrap();
    // the files (a later one of the same name replaces an earlier, as the driver's map did)
    let mut files: Vec<(String, Vec<f64>)> = Vec::new();
    while let Some(open) = files_part.find('{') {
        let close = files_part.find('}').unwrap();
        let name = files_part[open + 1..close].to_string();
        let after = &files_part[close + 1..];
        let end = after.find('{').unwrap_or(after.len());
        let f = nums(&after[..end]);
        files.retain(|(n, _)| *n != name);
        files.push((name, f));
        files_part = &after[end..];
    }
    let mut expect = "same".to_string();
    if shift > 0 && manager != 0 {
        for name in pack_names(path.as_bytes()) {
            let name = String::from_utf8(name).unwrap();
            let Some((_, f)) = files.iter().find(|(n, _)| *n == name) else {
                continue;
            };
            if f[0] != 0.0 {
                continue;
            }
            let (kind, fw, fh, fseed) = (f[1] as i32, f[2] as i32, f[3] as i32, f[4] as u32);
            let factor = pack_factor(fw, fh, w, h);
            if factor == 0 {
                break;
            }
            let (scale, b) = pack_scale(factor, shift);
            let mut from = native(kind, fw, fh, fseed, 16, -1, 7);
            let pitch = from.field(F::PixelsPerRow);
            let fpx = from.words_mut().unwrap().to_vec();
            let mut to = image(w, h, scale as f32, 0);
            let (bits, bpr) = mask_bits(w, h, mseed);
            let m = Mask { bits: &bits, width: w, height: h, bpr };
            let to_pitch = to.field(F::PixelsPerRow);
            if let Some(px) = to.words_mut() {
                px.fill(0);
                let mut d = Dst { px, pitch: to_pitch, scale: scale as f32, clip: (0, 0, 0, 0) };
                pack_reduce(&fpx, pitch, &mut d, w, h, scale, b, if mk != 0 { Some(m) } else { None });
            }
            expect = describe(w, h, scale as f32, &mut to, if mk != 0 { Some((w, h, &bits)) } else { None });
            break;
        }
    }
    assert_eq!(expect, want, "pack {rest}");
}
