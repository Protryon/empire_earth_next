//! WONGUI's images on the platform layer, their pixel work as Rust (R6 WON round 40): the C++ `PlatNativeImage`'s
//! draw (a blit within the destination's clip with GDI's mask rule, SRCAND then SRCPAINT: a masked pixel keeps the
//! destination where the image is black, as the raw images make it), its draw between pixel scales (bilinear, never
//! across a transparency mask's edge or out of the source rectangle; the mask applied at its logical pixels), its
//! raw copy, the raw images' native images (32-bit, 8-bit through the palette, 1-bit as a monochrome bitmap), the
//! texture pack's copies (the file names under Data/Textures-4x, the factor and the reduction of a 4x file at the
//! 2x setting, the original's mask kept) and the WON hand cursor's pixels.
//!
//! An image has a logical size (what the GUI lays out, draws and clips in) and a pixel scale: its surface holds
//! `pixels(size, scale)` pixels (WONLobbyPlatform.h "Pixel scale"). The C++ classes keep the objects (the surfaces,
//! the masks, the new images); each method hands their pixels here.

use crate::imgfilter::EeWonRawPixels;
use crate::platgfx::pixels;
use crate::surface::{EeWonSurface, EeWonSurfaceField as F};

/// A 1-bit mask (`PlatNativeMask`: RawImage1's rows, MSB first, rows padded to 4 bytes); a set bit is a transparent
/// pixel.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeWonPlatMask {
    pub bits: *const u8,
    pub width: i32,
    pub height: i32,
    pub bytes_per_row: i32,
}

/// A mask's bits borrowed.
#[derive(Clone, Copy)]
pub struct Mask<'a> {
    pub bits: &'a [u8],
    pub width: i32,
    pub height: i32,
    pub bpr: i32,
}

impl Mask<'_> {
    /// `IsTransparent(x, y)` (x, y inside the mask).
    pub fn is_transparent(&self, x: i32, y: i32) -> bool {
        let i = y as i64 * self.bpr as i64 + (x >> 3) as i64;
        usize::try_from(i).ok().and_then(|i| self.bits.get(i)).is_some_and(|b| b & (0x80u8 >> (x & 7)) != 0)
    }
    /// Inside the mask and transparent.
    pub fn transparent(&self, x: i32, y: i32) -> bool {
        x < self.width && y < self.height && self.is_transparent(x, y)
    }
}

/// The mask at `m` (None for null).
///
/// # Safety
/// `m` is null or its bits hold `bytes_per_row * height` bytes.
pub unsafe fn mask_of<'a>(m: *const EeWonPlatMask) -> Option<Mask<'a>> {
    if m.is_null() {
        return None;
    }
    // SAFETY: the contract
    let m = unsafe { &*m };
    let n = (m.bytes_per_row as i64 * m.height.max(0) as i64).max(0) as usize;
    let bits: &[u8] = if n == 0 || m.bits.is_null() {
        &[]
    } else {
        // SAFETY: the contract
        unsafe { std::slice::from_raw_parts(m.bits, n) }
    };
    Some(Mask { bits, width: m.width, height: m.height, bpr: m.bytes_per_row })
}

/// One axis of a scaled draw for one destination pixel: the logical source pixel it lies in (the mask's), the two
/// source pixels it is filtered from with the second's weight (0..256), and their logical pixels.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Tap {
    pub logical: i32,
    pub pixel0: i32,
    pub pixel1: i32,
    pub weight1: i32,
    pub logical0: i32,
    pub logical1: i32,
}

/// The taps of destination pixels `from..to` drawing logical source cells `start..start+count` placed at logical
/// `at`, at destination scale `dest` and source scale `source`. Equal scales with equal pixel extents copy pixel for
/// pixel; otherwise bilinear, clamped to the source rectangle's pixels.
#[allow(clippy::too_many_arguments)]
pub fn taps(from: i32, to: i32, at: i32, start: i32, count: i32, dest: f32, source: f32) -> Vec<Tap> {
    let dest_start = pixels(at, dest);
    let dest_end = pixels(at.wrapping_add(count), dest);
    let first = pixels(start, source);
    let mut last = pixels(start.wrapping_add(count), source).wrapping_sub(1);
    if last < first {
        last = first;
    }
    let exact = dest == source && dest_end.wrapping_sub(dest_start) == last.wrapping_sub(first).wrapping_add(1);
    let mut out = Vec::with_capacity(if to > from { (to - from) as usize } else { 0 });
    for p in from..to {
        let mut t = Tap::default();
        let logical = (p as f32 + 0.5) / dest - at as f32 + start as f32;
        if exact {
            t.pixel0 = first.wrapping_add(p.wrapping_sub(dest_start));
            t.pixel1 = t.pixel0;
            t.weight1 = 0;
        } else {
            let mut centre = logical * source - 0.5;
            if centre < first as f32 {
                centre = first as f32;
            }
            if centre > last as f32 {
                centre = last as f32;
            }
            let base = centre.floor() as i32;
            t.pixel0 = base;
            t.pixel1 = if base.wrapping_add(1) <= last { base.wrapping_add(1) } else { base };
            t.weight1 = ((centre - base as f32) * 256.0 + 0.5) as i32;
        }
        t.pixel0 = t.pixel0.clamp(first, last.max(first));
        t.pixel1 = t.pixel1.clamp(first, last.max(first));
        let cell = logical.floor() as i32;
        let end = start.wrapping_add(count);
        t.logical = if cell < start {
            start
        } else if cell >= end {
            end.wrapping_sub(1)
        } else {
            cell
        };
        t.logical0 = ((t.pixel0 as f32 + 0.5) / source).floor() as i32;
        t.logical1 = ((t.pixel1 as f32 + 0.5) / source).floor() as i32;
        out.push(t);
    }
    out
}

/// A source image's pixels.
#[derive(Clone, Copy)]
pub struct Src<'a> {
    pub px: &'a [u32],
    pub pitch: i32,
    pub scale: f32,
    /// The logical size.
    pub width: i32,
    pub height: i32,
    pub mask: Option<Mask<'a>>,
}

impl Src<'_> {
    fn at(&self, x: i32, y: i32) -> u32 {
        let i = y as i64 * self.pitch as i64 + x as i64;
        usize::try_from(i).ok().and_then(|i| self.px.get(i)).copied().unwrap_or(0)
    }
}

/// A destination surface's pixels.
pub struct Dst<'a> {
    pub px: &'a mut [u32],
    pub pitch: i32,
    pub scale: f32,
    pub clip: (i32, i32, i32, i32),
}

impl Dst<'_> {
    fn at(&mut self, x: i32, y: i32) -> Option<&mut u32> {
        let i = y as i64 * self.pitch as i64 + x as i64;
        usize::try_from(i).ok().and_then(move |i| self.px.get_mut(i))
    }
}

/// `PlatNativeImage::Draw` after its surface checks: part of the image into the destination, within its clip.
#[allow(clippy::too_many_arguments)]
pub fn draw(d: &mut Dst<'_>, s: &Src<'_>, mut x: i32, mut y: i32, mut left: i32, mut top: i32, mut w: i32, mut h: i32) {
    // the source rectangle inside the image (GDI reads nothing outside a bitmap)
    if left < 0 {
        x = x.wrapping_sub(left);
        w = w.wrapping_add(left);
        left = 0;
    }
    if top < 0 {
        y = y.wrapping_sub(top);
        h = h.wrapping_add(top);
        top = 0;
    }
    if left.wrapping_add(w) > s.width {
        w = s.width.wrapping_sub(left);
    }
    if top.wrapping_add(h) > s.height {
        h = s.height.wrapping_sub(top);
    }
    if w <= 0 || h <= 0 {
        return;
    }
    if d.scale != 1.0 || s.scale != 1.0 {
        draw_scaled(d, s, x, y, left, top, w, h);
        return;
    }
    let (cx1, cy1, cx2, cy2) = d.clip;
    if x < cx1 {
        left += cx1 - x;
        w -= cx1 - x;
        x = cx1;
    }
    if y < cy1 {
        top += cy1 - y;
        h -= cy1 - y;
        y = cy1;
    }
    if x + w - 1 > cx2 {
        w = cx2 - x + 1;
    }
    if y + h - 1 > cy2 {
        h = cy2 - y + 1;
    }
    if w <= 0 || h <= 0 {
        return;
    }
    for j in 0..h {
        let sy = top + j;
        let masked = s.mask.filter(|m| sy < m.height);
        for i in 0..w {
            let from = s.at(left + i, sy);
            let Some(to) = d.at(x + i, y + j) else {
                continue;
            };
            *to = match masked {
                None => from,
                Some(m) => {
                    let sx = left + i;
                    let transparent = sx < m.width && m.is_transparent(sx, sy);
                    (if transparent { *to } else { 0 }) | from
                }
            };
        }
    }
}

/// `DrawScaled`: the logical rectangle's destination pixels (within the destination's clip) filtered from the source
/// rectangle's pixels; with a mask, each destination pixel is transparent exactly where its logical source pixel is,
/// and an opaque one is filtered from opaque source pixels only (no dark fringe of the transparent black around a
/// masked shape).
#[allow(clippy::too_many_arguments)]
pub fn draw_scaled(d: &mut Dst<'_>, s: &Src<'_>, x: i32, y: i32, left: i32, top: i32, w: i32, h: i32) {
    let (cx1, cy1, cx2, cy2) = d.clip;
    let x1 = pixels(x, d.scale).max(cx1);
    let y1 = pixels(y, d.scale).max(cy1);
    let x2 = pixels(x.wrapping_add(w), d.scale).min(cx2.wrapping_add(1));
    let y2 = pixels(y.wrapping_add(h), d.scale).min(cy2.wrapping_add(1));
    if x2 <= x1 || y2 <= y1 {
        return;
    }
    let cols = taps(x1, x2, x, left, w, d.scale, s.scale);
    let rows = taps(y1, y2, y, top, h, d.scale, s.scale);
    let tr = |lx: i32, ly: i32| s.mask.is_some_and(|m| m.transparent(lx, ly));
    for j in y1..y2 {
        let r = rows[(j - y1) as usize];
        for i in x1..x2 {
            let c = cols[(i - x1) as usize];
            let p00 = s.at(c.pixel0, r.pixel0);
            let Some(to) = d.at(i, j) else {
                continue;
            };
            if tr(c.logical, r.logical) {
                *to |= p00 & 0x00FF_FFFF;
                continue;
            }
            if c.weight1 == 0 && r.weight1 == 0 {
                *to = p00 & 0x00FF_FFFF;
                continue;
            }
            let px = [p00, s.at(c.pixel1, r.pixel0), s.at(c.pixel0, r.pixel1), s.at(c.pixel1, r.pixel1)];
            let mut wt = [
                (256 - c.weight1) * (256 - r.weight1),
                c.weight1 * (256 - r.weight1),
                (256 - c.weight1) * r.weight1,
                c.weight1 * r.weight1,
            ];
            if s.mask.is_some() {
                if tr(c.logical0, r.logical0) {
                    wt[0] = 0;
                }
                if tr(c.logical1, r.logical0) {
                    wt[1] = 0;
                }
                if tr(c.logical0, r.logical1) {
                    wt[2] = 0;
                }
                if tr(c.logical1, r.logical1) {
                    wt[3] = 0;
                }
            }
            let total = wt[0] + wt[1] + wt[2] + wt[3];
            if total <= 0 {
                // every tap is masked though the pixel is not: the nearest source pixel of its own logical pixel
                let v = s.at(pixels(c.logical, s.scale), pixels(r.logical, s.scale));
                *to = v & 0x00FF_FFFF;
                continue;
            }
            let (mut red, mut green, mut blue) = (0u32, 0u32, 0u32);
            for k in 0..4 {
                red = red.wrapping_add(((px[k] >> 16) & 0xff).wrapping_mul(wt[k] as u32));
                green = green.wrapping_add(((px[k] >> 8) & 0xff).wrapping_mul(wt[k] as u32));
                blue = blue.wrapping_add((px[k] & 0xff).wrapping_mul(wt[k] as u32));
            }
            let total = total as u32;
            let half = total / 2;
            *to = (((red + half) / total) << 16) | (((green + half) / total) << 8) | ((blue + half) / total);
        }
    }
}

/// `GetRaw`'s pixels (`out` holds width x height logical pixels): the pixel at each logical pixel's centre, its
/// alpha byte 0 (GetDIBits), or with a mask 0xff where the mask is opaque. `pw`, `ph`: the surface's pixel size.
pub fn raw(s: &Src<'_>, pw: i32, ph: i32, out: &mut [u32]) {
    let (w, h) = (s.width, s.height);
    for y in 0..h {
        let py = if s.scale == 1.0 { y } else { (((y as f32 + 0.5) * s.scale).floor() as i32).min(ph - 1) };
        for x in 0..w {
            let pxl = if s.scale == 1.0 { x } else { (((x as f32 + 0.5) * s.scale).floor() as i32).min(pw - 1) };
            let mut v = s.at(pxl, py) & 0x00FF_FFFF;
            if let Some(m) = s.mask {
                if !m.transparent(x, y) {
                    v |= 0xff00_0000;
                }
            }
            if let Some(o) = out.get_mut(y as usize * w as usize + x as usize) {
                *o = v;
            }
        }
    }
}

/// The native image of a raw image (`RawImage32/8/1::CreateNativeImage`, `RawImage1::GetNative`) into the new
/// image's pixels: 32-bit without the alpha byte; 8-bit through the palette, the transparent index (and an index
/// past the palette) black; 1-bit white where a bit is set.
#[allow(clippy::too_many_arguments)]
pub fn from_raw(
    kind: i32,
    data: &[u8],
    pitch: i32,
    w: i32,
    h: i32,
    palette: &[u32],
    transparent: i32,
    d: &mut Dst<'_>,
) {
    let byte = |i: i64| usize::try_from(i).ok().and_then(|i| data.get(i)).copied().unwrap_or(0);
    for y in 0..h {
        for x in 0..w {
            let v = match kind {
                0 => {
                    let b = byte(y as i64 * pitch as i64 + (x >> 3) as i64);
                    if b & (0x80u8 >> (x & 7)) != 0 {
                        0x00FF_FFFF
                    } else {
                        0
                    }
                }
                1 => {
                    let index = byte(y as i64 * pitch as i64 + x as i64) as i32;
                    if (index as usize) < palette.len() && index != transparent {
                        palette[index as usize] & 0x00FF_FFFF
                    } else {
                        0
                    }
                }
                _ => {
                    let i = (y as i64 * w as i64 + x as i64) * 4;
                    let p = [byte(i), byte(i + 1), byte(i + 2), byte(i + 3)];
                    u32::from_ne_bytes(p) & 0x00FF_FFFF
                }
            };
            if let Some(to) = d.at(x, y) {
                *to = v;
            }
        }
    }
}

// ---- the texture pack's copies

/// "...WONLobby Resources/<rest>" -> <rest> with '/' separators (either separator, any case, the folder a whole
/// path part); None for another path or an empty rest.
pub fn pack_relative(path: &[u8]) -> Option<Vec<u8>> {
    const FOLDER: &[u8] = b"wonlobby resources";
    for p in 0..path.len() {
        let mut i = 0;
        while i < FOLDER.len() && p + i < path.len() && path[p + i].to_ascii_lowercase() == FOLDER[i] {
            i += 1;
        }
        let sep = |c: u8| c == b'/' || c == b'\\';
        if i == FOLDER.len() && p + i < path.len() && sep(path[p + i]) && (p == 0 || sep(path[p - 1])) {
            let rest: Vec<u8> = path[p + i + 1..].iter().map(|&c| if c == b'\\' { b'/' } else { c }).collect();
            return if rest.is_empty() { None } else { Some(rest) };
        }
    }
    None
}

/// The pack's file names to try for `path`: `Data/Textures-4x/WONLobby Resources/<rest>`, then the same with its
/// extension replaced by .png when that is another name.
pub fn pack_names(path: &[u8]) -> Vec<Vec<u8>> {
    let Some(rest) = pack_relative(path) else {
        return Vec::new();
    };
    let mut name = b"Data/Textures-4x/WONLobby Resources/".to_vec();
    name.extend_from_slice(&rest);
    let mut out = vec![name.clone()];
    if let Some(dot) = name.iter().rposition(|&c| c == b'.') {
        if !name[dot..].contains(&b'/') {
            let mut png = name[..dot].to_vec();
            png.extend_from_slice(b".png");
            if png != name {
                out.push(png);
            }
        }
    }
    out
}

/// The pack copy's factor: an integer multiple (2 or more) of the original's size in both directions, else 0.
pub fn pack_factor(raw_w: i32, raw_h: i32, w: i32, h: i32) -> i32 {
    if w == 0 || h == 0 {
        return 0;
    }
    let f = raw_w / w;
    if f < 2 || raw_w != w.wrapping_mul(f) || raw_h != h.wrapping_mul(f) {
        0
    } else {
        f
    }
}

/// The scale and box of a pack copy of `factor` at the setting's logical shift: the file reduced by 2x2 boxes as
/// often as the setting's scale is below the file's.
pub fn pack_scale(factor: i32, shift: i32) -> (i32, i32) {
    let (mut scale, mut b) = (factor, 1);
    while scale > (1 << shift) && scale % 2 == 0 {
        scale /= 2;
        b *= 2;
    }
    (scale, b)
}

/// The pack copy reduced into the new image (`w` x `h` logical pixels at `scale`, boxes of `b` x `b`), then black
/// under the original's mask (the mask draw ORs the image in there).
#[allow(clippy::too_many_arguments)]
pub fn pack_reduce(
    from: &[u32],
    from_pitch: i32,
    d: &mut Dst<'_>,
    w: i32,
    h: i32,
    scale: i32,
    b: i32,
    mask: Option<Mask<'_>>,
) {
    let at = |x: i32, y: i32| {
        usize::try_from(y as i64 * from_pitch as i64 + x as i64).ok().and_then(|i| from.get(i)).copied().unwrap_or(0)
    };
    let n = (b * b) as u32;
    for y in 0..h * scale {
        for x in 0..w * scale {
            let (mut red, mut green, mut blue) = (0u32, 0u32, 0u32);
            for j in 0..b {
                for i in 0..b {
                    let p = at(x * b + i, y * b + j);
                    red += (p >> 16) & 0xff;
                    green += (p >> 8) & 0xff;
                    blue += p & 0xff;
                }
            }
            if let Some(to) = d.at(x, y) {
                *to = (((red + n / 2) / n) << 16) | (((green + n / 2) / n) << 8) | ((blue + n / 2) / n);
            }
        }
    }
    if let Some(m) = mask {
        for y in 0..h * scale {
            for x in 0..w * scale {
                if m.transparent(x / scale, y / scale) {
                    if let Some(to) = d.at(x, y) {
                        *to = 0;
                    }
                }
            }
        }
    }
}

// ---- the WON hand cursor

/// The WON hand cursor (32x32, hot spot 12, 4): the XOR plane.
#[rustfmt::skip]
const HAND_XOR: [u8; 128] = [
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00,
    0x00, 0x0c, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x0d, 0x80, 0x00, 0x00, 0x0d, 0xb0, 0x00,
    0x00, 0x0d, 0xb4, 0x00, 0x00, 0x0d, 0xb6, 0x00, 0x00, 0xcf, 0xf6, 0x00, 0x00, 0xef, 0xfe, 0x00,
    0x00, 0x6f, 0xfe, 0x00, 0x00, 0x2f, 0xfe, 0x00, 0x00, 0x3f, 0xfe, 0x00, 0x00, 0x1f, 0xfe, 0x00,
    0x00, 0x1f, 0xfc, 0x00, 0x00, 0x0f, 0xfc, 0x00, 0x00, 0x0f, 0xfc, 0x00, 0x00, 0x07, 0xf8, 0x00,
    0x00, 0x07, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
];

/// ... and the AND plane.
#[rustfmt::skip]
const HAND_AND: [u8; 128] = [
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xf3, 0xff, 0xff, 0xff, 0xe1, 0xff, 0xff, 0xff, 0xe1, 0xff, 0xff, 0xff, 0xe1, 0xff, 0xff,
    0xff, 0xe1, 0xff, 0xff, 0xff, 0xe0, 0x7f, 0xff, 0xff, 0xe0, 0x0f, 0xff, 0xff, 0xe0, 0x03, 0xff,
    0xff, 0xe0, 0x01, 0xff, 0xfe, 0x20, 0x00, 0xff, 0xfe, 0x00, 0x00, 0xff, 0xfe, 0x00, 0x00, 0xff,
    0xff, 0x00, 0x00, 0xff, 0xff, 0x80, 0x00, 0xff, 0xff, 0x80, 0x00, 0xff, 0xff, 0xc0, 0x00, 0xff,
    0xff, 0xc0, 0x01, 0xff, 0xff, 0xe0, 0x01, 0xff, 0xff, 0xe0, 0x01, 0xff, 0xff, 0xf0, 0x03, 0xff,
    0xff, 0xf0, 0x03, 0xff, 0xff, 0xf0, 0x03, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
];

/// The hand cursor's 32 x 32 pixels (0xAARRGGBB): the AND and XOR planes as a cursor's colours (transparent,
/// black, white, and the inverted screen shown black).
pub fn hand_cursor() -> [u32; 1024] {
    let mut out = [0u32; 1024];
    for y in 0..32 {
        for x in 0..32 {
            let byte = y * 4 + x / 8;
            let bit = 0x80u8 >> (x & 7);
            let and = HAND_AND[byte] & bit != 0;
            let xor = HAND_XOR[byte] & bit != 0;
            out[y * 32 + x] = match (and, xor) {
                (true, true) => 0xFF00_0000,
                (true, false) => 0,
                (false, true) => 0xFFFF_FFFF,
                (false, false) => 0xFF00_0000,
            };
        }
    }
    out
}

// ---- the C entry points (the C++ classes' methods)

/// The destination view of a live 32-bit surface at `scale` (None without pixels).
fn dst_of(s: &mut EeWonSurface, scale: f32) -> Option<Dst<'_>> {
    let pitch = s.field(F::PixelsPerRow);
    let clip = (s.field(F::ClipX1), s.field(F::ClipY1), s.field(F::ClipX2), s.field(F::ClipY2));
    let px = s.words_mut()?;
    Some(Dst { px, pitch, scale, clip })
}

/// `PlatNativeImage::Draw(context, x, y, left, top, width, height)` once the context has a surface `dst` (of pixel
/// scale `dst_scale`): the image `src` (scale `src_scale`, logical `src_w` x `src_h`, mask `mask` or null).
///
/// # Safety
/// `dst` and `src` are live surfaces (they may be the same one); `mask` as `mask_of`.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_plat_img_draw(
    dst: *mut EeWonSurface,
    dst_scale: f32,
    src: *mut EeWonSurface,
    src_scale: f32,
    src_w: i32,
    src_h: i32,
    mask: *const EeWonPlatMask,
    x: i32,
    y: i32,
    left: i32,
    top: i32,
    width: i32,
    height: i32,
) {
    if dst.is_null() || src.is_null() {
        return;
    }
    let dest_scale = if dst_scale > 0.0 { dst_scale } else { 1.0 };
    // SAFETY: the contract
    let m = unsafe { mask_of(mask) };
    // the original returned for an empty source rectangle or no pixels before choosing the scaled draw: the same
    // tests (draw clips first, and a surface without pixels has none to read or write)
    if std::ptr::eq(dst, src) {
        // an image drawn onto its own surface reads a copy of its pixels
        // SAFETY: a live surface (the contract)
        let s = unsafe { &mut *src };
        let pitch = s.field(F::PixelsPerRow);
        let Some(own) = s.words_mut().map(|w| w.to_vec()) else {
            return;
        };
        let sv = Src { px: &own, pitch, scale: src_scale, width: src_w, height: src_h, mask: m };
        if let Some(mut dv) = dst_of(s, dest_scale) {
            draw(&mut dv, &sv, x, y, left, top, width, height);
        }
        return;
    }
    // SAFETY: live distinct surfaces (the contract)
    let (d, s) = unsafe { (&mut *dst, &mut *src) };
    let pitch = s.field(F::PixelsPerRow);
    let Some(px) = s.words_mut() else {
        return;
    };
    let sv = Src { px, pitch, scale: src_scale, width: src_w, height: src_h, mask: m };
    if let Some(mut dv) = dst_of(d, dest_scale) {
        draw(&mut dv, &sv, x, y, left, top, width, height);
    }
}

/// `PlatNativeImage::GetRaw`'s pixels into `out` (`width * height` of them): `src` at `scale`, logical
/// `width` x `height`, mask `mask` or null.
///
/// # Safety
/// `src` is a live surface; `out` holds `width * height` pixels; `mask` as `mask_of`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_plat_img_raw(
    src: *mut EeWonSurface,
    scale: f32,
    width: i32,
    height: i32,
    mask: *const EeWonPlatMask,
    out: *mut u32,
) {
    if src.is_null() || out.is_null() || width <= 0 || height <= 0 {
        return;
    }
    // SAFETY: the contract
    let s = unsafe { &mut *src };
    let (pw, ph, pitch) = (s.field(F::Width), s.field(F::Height), s.field(F::PixelsPerRow));
    let px: &[u32] = match s.words_mut() {
        Some(w) => w,
        None => &[],
    };
    // SAFETY: the contract
    let m = unsafe { mask_of(mask) };
    let sv = Src { px, pitch, scale, width, height, mask: m };
    // SAFETY: the contract
    let o = unsafe { std::slice::from_raw_parts_mut(out, width as usize * height as usize) };
    raw(&sv, pw, ph, o);
}

/// A raw image's native image into the new image's surface `dst` (`from_raw`); `palette` holds `colors` colours.
///
/// # Safety
/// `src` describes a live raw image (its `pitch` the row bytes of a 1- or 8-bit one); `palette` null or `colors`
/// long; `dst` a live surface.
#[no_mangle]
pub unsafe extern "C" fn ee_won_plat_img_from_raw(
    src: *const EeWonRawPixels,
    palette: *const u32,
    colors: i32,
    transparent: i32,
    dst: *mut EeWonSurface,
) {
    if src.is_null() || dst.is_null() {
        return;
    }
    // SAFETY: the contract
    let r = unsafe { &*src };
    let bytes = match r.kind {
        2 => r.width as i64 * r.height as i64 * 4,
        _ => r.pitch as i64 * r.height as i64,
    };
    let data: &[u8] = if bytes <= 0 || r.data.is_null() {
        &[]
    } else {
        // SAFETY: the contract
        unsafe { std::slice::from_raw_parts(r.data, bytes as usize) }
    };
    let pal: &[u32] = if colors <= 0 || palette.is_null() {
        &[]
    } else {
        // SAFETY: the contract
        unsafe { std::slice::from_raw_parts(palette, colors as usize) }
    };
    // SAFETY: the contract
    let Some(mut d) = dst_of(unsafe { &mut *dst }, 1.0) else {
        return;
    };
    from_raw(r.kind, data, r.pitch, r.width, r.height, pal, transparent, &mut d);
}

/// The pack's file name `which` (0, 1) for `path` (NUL-terminated) into `out` (`cap` bytes, NUL-terminated):
/// its length, or -1 when there is none (another path, or the .png name is the first).
///
/// # Safety
/// `path` is NUL-terminated; `out` holds `cap` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_plat_pack_name(path: *const u8, which: i32, out: *mut u8, cap: usize) -> i32 {
    if path.is_null() || out.is_null() || cap == 0 {
        return -1;
    }
    // SAFETY: the contract
    let p = unsafe { std::ffi::CStr::from_ptr(path as *const std::ffi::c_char) }.to_bytes();
    let names = pack_names(p);
    let Some(n) = usize::try_from(which).ok().and_then(|i| names.get(i)) else {
        return -1;
    };
    if n.len() + 1 > cap {
        return -1;
    }
    // SAFETY: the contract (cap bytes)
    unsafe {
        std::ptr::copy_nonoverlapping(n.as_ptr(), out, n.len());
        *out.add(n.len()) = 0;
    }
    n.len() as i32
}

/// `pack_factor` and `pack_scale`: the copy's scale (0: not a pack copy) and box into `*out_box`.
///
/// # Safety
/// `out_box` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_plat_pack_fit(
    raw_w: i32,
    raw_h: i32,
    w: i32,
    h: i32,
    shift: i32,
    out_box: *mut i32,
) -> i32 {
    let f = pack_factor(raw_w, raw_h, w, h);
    if f == 0 {
        return 0;
    }
    let (scale, b) = pack_scale(f, shift);
    // SAFETY: the contract
    unsafe { *out_box = b };
    scale
}

/// `pack_reduce` from the pack copy's surface `from` into the new image's surface `to`.
///
/// # Safety
/// `from`, `to` live distinct surfaces; `mask` as `mask_of`.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_plat_pack_reduce(
    from: *mut EeWonSurface,
    to: *mut EeWonSurface,
    w: i32,
    h: i32,
    scale: i32,
    b: i32,
    mask: *const EeWonPlatMask,
) {
    if from.is_null() || to.is_null() || std::ptr::eq(from, to) {
        return;
    }
    // SAFETY: the contract
    let (f, t) = unsafe { (&mut *from, &mut *to) };
    let pitch = f.field(F::PixelsPerRow);
    let Some(px) = f.words_mut() else {
        return;
    };
    let Some(mut d) = dst_of(t, 1.0) else {
        return;
    };
    // SAFETY: the contract
    let m = unsafe { mask_of(mask) };
    pack_reduce(px, pitch, &mut d, w, h, scale, b, m);
}

/// The WON hand cursor's 1,024 pixels into `out`.
///
/// # Safety
/// `out` holds 1,024 pixels.
#[no_mangle]
pub unsafe extern "C" fn ee_won_plat_hand_cursor(out: *mut u32) {
    if out.is_null() {
        return;
    }
    let p = hand_cursor();
    // SAFETY: the contract
    unsafe { std::ptr::copy_nonoverlapping(p.as_ptr(), out, p.len()) };
}

#[cfg(test)]
#[path = "platimg_tests.rs"]
mod tests;
