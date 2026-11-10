//! PlatFont (include/platform/PlatFont.h): GDI's metrics rebuilt from the font's tables, glyphs rendered by code
//! point (U+0020..U+00FF when the font is created, the others on first use), text drawn into pixel buffers. The port
//! of `src/Platform/Common/PlatFont.cpp`; the bitmaps come from `ttf.rs`, the bytes stb_truetype made. Text is UTF-8
//! (decision D6): a stray byte, a surrogate or a character the font lacks draws `?` (what the bytes Windows-1252
//! leaves undefined drew before).

use std::collections::BTreeMap;
use std::ffi::{c_char, c_int, c_void, CStr};
use std::sync::Mutex;

use ee_plat_core::utf8;

use crate::ttf::{self, FontInfo};

/// `PlatFontMetrics`: the C header's layout.
#[repr(C)]
#[derive(Clone, Copy, Default, Debug, PartialEq)]
pub struct PlatFontMetrics {
    pub height: i32,
    pub ascent: i32,
    pub descent: i32,
    pub internal_leading: i32,
    pub external_leading: i32,
    pub ave_char_width: i32,
    pub max_char_width: i32,
    pub first_char: u8,
    pub last_char: u8,
}

/// `PlatFontTarget`: the C header's layout.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct PlatFontTarget {
    pub pixels: *mut c_void,
    pub pitch: i32,
    pub width: i32,
    pub height: i32,
    pub flags: u32,
    pub scale: f32,
}

/// `PlatFontRect`
#[repr(C)]
#[derive(Clone, Copy)]
pub struct PlatFontRect {
    pub left: i32,
    pub top: i32,
    pub right: i32,
    pub bottom: i32,
}

/// `PLAT_FONT_TARGET_PREMULTIPLIED`
pub const TARGET_PREMULTIPLIED: u32 = 0x1;

/// The bytes of a C string, empty for NULL.
///
/// # Safety
/// `text` is NULL or a valid NUL-terminated string.
pub unsafe fn c_bytes<'a>(text: *const c_char) -> &'a [u8] {
    if text.is_null() {
        &[]
    } else {
        CStr::from_ptr(text).to_bytes()
    }
}

/// `text` as a NUL-terminated buffer; a NUL inside ends the C string there.
pub fn c_terminated(text: &[u8]) -> Vec<u8> {
    let end = text.iter().position(|&b| b == 0).unwrap_or(text.len());
    let mut out = text[..end].to_vec();
    out.push(0);
    out
}

#[derive(Clone)]
pub struct Face {
    name: Vec<u8>,
    bold: bool,
    italic: bool,
    info: FontInfo<'static>,
}

#[derive(Default)]
pub struct Registry {
    faces: Vec<Face>,
    aliases: Vec<(Vec<u8>, Vec<u8>)>,
}

fn same_no_case(a: &[u8], b: &[u8]) -> bool {
    a.eq_ignore_ascii_case(b)
}

impl Registry {
    pub const fn new() -> Self {
        Registry { faces: Vec::new(), aliases: Vec::new() }
    }

    /// `PlatFontRegister`: `data` must stay valid (C: the font data, kept in place).
    pub fn register(&mut self, face: &[u8], bold: bool, italic: bool, data: &'static [u8]) -> bool {
        if data.len() < 12 {
            return false;
        }
        let offset = ttf::font_offset_for_index(data, 0);
        if offset < 0 {
            return false;
        }
        let Some(info) = ttf::init_font(data, offset as u32) else {
            return false;
        };
        self.faces.push(Face { name: face.to_vec(), bold, italic, info });
        true
    }

    pub fn alias(&mut self, face: &[u8], target: &[u8]) {
        self.aliases.push((face.to_vec(), target.to_vec()));
    }

    /// The registered face closest to the request: name, then style. `None` only when no face is registered.
    pub fn find_face(&self, name: &[u8], bold: bool, italic: bool) -> Option<Face> {
        let mut name = name.to_vec();
        for (from, to) in &self.aliases {
            if same_no_case(from, &name) {
                name = to.clone();
            }
        }
        let mut best: Option<&Face> = None;
        let mut best_score = -1;
        for face in &self.faces {
            let score = (if same_no_case(&face.name, &name) { 4 } else { 0 })
                + (if face.bold == bold { 2 } else { 0 })
                + (if face.italic == italic { 1 } else { 0 });
            if score > best_score {
                best = Some(face);
                best_score = score;
            }
        }
        best.cloned()
    }
}

static REGISTRY: Mutex<Registry> = Mutex::new(Registry::new());

/// Round as the C++ `Round`: `floorf(v + 0.5f)`.
fn round(v: f32) -> i32 {
    (v + 0.5).floor() as i32
}

#[derive(Default)]
pub struct Glyph {
    advance: i32,
    x0: i32,
    y0: i32,
    width: i32,
    height: i32,
    coverage: Vec<u8>,
}

/// What `PlatFont*` points at.
pub struct PlatFont {
    metrics: PlatFontMetrics,
    antialiased: bool,
    underline: bool,
    synthetic_bold: bool,
    underline_offset: i32,
    underline_thickness: i32,
    /// U+0000..U+00FF, rendered when the font is made (as the GDI port rendered its 256 cells)
    glyphs: Vec<Glyph>,
    /// every other code point, rendered on first use; a Box so that a reference survives the lock
    extra: Mutex<BTreeMap<u32, Box<Glyph>>>,
    info: FontInfo<'static>,
    scale: f32,
    face: Vec<u8>,
    height: i32,
    bold: i32,
    italic: i32,
    scaled: Mutex<BTreeMap<i32, Box<PlatFont>>>,
}

/// The character a code point is drawn as: a stray byte or a surrogate is U+FFFD.
fn drawn(cp: u32) -> u32 {
    if (0xD800..=0xDFFF).contains(&cp) {
        utf8::REPLACEMENT
    } else {
        cp
    }
}

fn render(info: &FontInfo<'static>, scale: f32, synthetic_bold: bool, cp: u32) -> Glyph {
    let mut index = info.find_glyph_index(cp as i32);
    if index == 0 && cp >= 0x80 {
        // no glyph: a question mark, as the GDI port drew the bytes Windows-1252 leaves undefined
        index = info.find_glyph_index(b'?' as i32);
    }
    let mut g = Glyph::default();
    let (advance, _bearing) = info.glyph_hmetrics(index);
    g.advance = round(advance as f32 * scale) + i32::from(synthetic_bold);
    let (x0, y0, x1, y1) = info.glyph_bitmap_box(index, scale, scale);
    if x1 > x0 && y1 > y0 {
        g.x0 = x0;
        g.y0 = y0;
        g.width = x1 - x0;
        g.height = y1 - y0;
        g.coverage = vec![0; g.width as usize * g.height as usize];
        info.make_glyph_bitmap(&mut g.coverage, g.width, g.height, scale, scale, index);
    }
    g
}

fn create(
    registry: &Registry,
    face_name: &[u8],
    height: i32,
    bold: i32,
    italic: i32,
    underline: i32,
    antialiased: i32,
) -> Option<Box<PlatFont>> {
    let face = registry.find_face(face_name, bold != 0, italic != 0)?;
    let info = face.info;
    let d = info.data;
    let units_per_em = u16_be(d, info.head as usize + 18);
    // usWinAscent / usWinDescent (OS/2), else hhea's ascender / descender
    let (win_ascent, win_descent, average);
    let os2 = ttf::find_table(d, info.fontstart, b"OS/2");
    if os2 != 0 {
        win_ascent = u16_be(d, os2 as usize + 74);
        win_descent = u16_be(d, os2 as usize + 76);
        average = i16_be(d, os2 as usize + 2);
    } else {
        let (a, de, _) = info.vmetrics();
        win_ascent = a;
        win_descent = -de;
        average = 0;
    }
    let scale = if height > 0 {
        height as f32 / (win_ascent + win_descent) as f32
    } else {
        (if height < 0 { height.wrapping_neg() } else { 12 }) as f32 / units_per_em as f32
    };
    let mut m = PlatFontMetrics {
        ascent: round(win_ascent as f32 * scale),
        descent: round(win_descent as f32 * scale),
        ..Default::default()
    };
    m.height = m.ascent + m.descent;
    m.internal_leading = (m.height - round(units_per_em as f32 * scale)).max(0);
    let (ascender, descender, line_gap) = info.vmetrics();
    // GDI: the external leading is what the Macintosh line spacing (hhea) adds over the Windows cell
    let external = round((ascender - descender + line_gap) as f32 * scale) - m.height;
    m.external_leading = external.max(0);
    m.first_char = 0x20;
    m.last_char = 0xFF;
    let synthetic_bold = bold != 0 && !face.bold;
    let thick = round(units_per_em as f32 * scale / 14.0);
    let mut glyphs: Vec<Glyph> = Vec::with_capacity(256);
    let mut max_advance = 0;
    for c in 0..256u32 {
        let g = if c >= m.first_char as u32 { render(&info, scale, synthetic_bold, c) } else { Glyph::default() };
        if g.advance > max_advance {
            max_advance = g.advance;
        }
        glyphs.push(g);
    }
    m.max_char_width = max_advance;
    m.ave_char_width = if average > 0 { round(average as f32 * scale) } else { glyphs[b'x' as usize].advance };
    Some(Box::new(PlatFont {
        metrics: m,
        antialiased: antialiased != 0,
        underline: underline != 0,
        synthetic_bold,
        underline_offset: (m.descent + 1) / 2,
        underline_thickness: if thick > 0 { thick } else { 1 },
        glyphs,
        extra: Mutex::new(BTreeMap::new()),
        info,
        scale,
        face: face_name.to_vec(),
        height,
        bold,
        italic,
        scaled: Mutex::new(BTreeMap::new()),
    }))
}

fn u16_be(d: &[u8], at: usize) -> i32 {
    let hi = d.get(at).copied().unwrap_or(0) as i32;
    let lo = d.get(at + 1).copied().unwrap_or(0) as i32;
    (hi << 8) | lo
}

fn i16_be(d: &[u8], at: usize) -> i32 {
    u16_be(d, at) as u16 as i16 as i32
}

impl PlatFont {
    pub fn metrics(&self) -> PlatFontMetrics {
        self.metrics
    }

    /// The glyph of a code point, rendered on first use past U+00FF. The pointer stays valid while the font lives:
    /// the extra glyphs are boxed and never removed.
    pub fn glyph(&self, cp: u32) -> *const Glyph {
        let cp = drawn(cp);
        if cp < 0x100 {
            return &self.glyphs[cp as usize];
        }
        let mut extra = self.extra.lock().unwrap_or_else(|e| e.into_inner());
        let g = extra.entry(cp).or_insert_with(|| Box::new(render(&self.info, self.scale, self.synthetic_bold, cp)));
        g.as_ref() as *const Glyph
    }

    pub fn measure(&self, text: &[u8]) -> i32 {
        // SAFETY: `glyph` gives a pointer that lives as long as `self`
        utf8::chars(text).map(|(_, cp, _)| unsafe { (*self.glyph(cp)).advance }).sum()
    }

    fn scaled(&self, registry: &Registry, scale: f32) -> *const PlatFont {
        #[allow(clippy::neg_cmp_op_on_partial_ord)] // a NaN is "no scale" here, as the C's !(s > 1.01f)
        if !(scale > 1.01f32) {
            return self as *const PlatFont;
        }
        let key = (scale * 64.0).round() as i32;
        let mut map = self.scaled.lock().unwrap_or_else(|e| e.into_inner());
        if let Some(s) = map.get(&key) {
            return s.as_ref() as *const PlatFont;
        }
        let h = if self.height != 0 { self.height } else { -12 };
        let new_height = (h as f32 * scale).round() as i32;
        match create(
            registry,
            &self.face,
            new_height,
            self.bold,
            self.italic,
            i32::from(self.underline),
            i32::from(self.antialiased),
        ) {
            None => self as *const PlatFont,
            Some(s) => {
                let p = s.as_ref() as *const PlatFont;
                map.insert(key, s);
                p
            }
        }
    }
}

#[derive(Clone, Copy)]
struct Clip {
    left: i32,
    top: i32,
    right: i32,
    bottom: i32,
}

#[inline]
fn blend(p: &mut [u8; 4], color: u32, coverage: i32, premultiplied: bool) {
    let b = (color & 0xFF) as i32;
    let g = ((color >> 8) & 0xFF) as i32;
    let r = ((color >> 16) & 0xFF) as i32;
    if coverage >= 255 {
        p[0] = b as u8;
        p[1] = g as u8;
        p[2] = r as u8;
        p[3] = 0xFF;
    } else {
        let mix = |c: i32, d: u8| ((c * coverage + d as i32 * (255 - coverage) + 127) / 255) as u8;
        p[0] = mix(b, p[0]);
        p[1] = mix(g, p[1]);
        p[2] = mix(r, p[2]);
        p[3] = if premultiplied { (coverage + (p[3] as i32 * (255 - coverage) + 127) / 255) as u8 } else { 0xFF };
    }
}

/// # Safety
/// The target's pixels are `height` rows of `pitch` bytes; only the clip rectangle (inside the target) is touched.
unsafe fn pixel(t: &PlatFontTarget, x: i32, y: i32) -> *mut [u8; 4] {
    (t.pixels as *mut u8).offset(y as isize * t.pitch as isize + x as isize * 4).cast()
}

unsafe fn fill(t: &PlatFontTarget, c: Clip, x0: i32, y0: i32, x1: i32, y1: i32, color: u32) {
    let (x0, y0, x1, y1) = (x0.max(c.left), y0.max(c.top), x1.min(c.right), y1.min(c.bottom));
    for y in y0..y1 {
        for x in x0..x1 {
            blend(&mut *pixel(t, x, y), color, 255, false);
        }
    }
}

unsafe fn draw_glyph(f: &PlatFont, g: &Glyph, t: &PlatFontTarget, c: Clip, at_x: i32, at_y: i32, color: u32) {
    for gy in 0..g.height {
        let y = at_y + g.y0 + gy;
        if y < c.top || y >= c.bottom {
            continue;
        }
        for gx in 0..g.width {
            let x = at_x + g.x0 + gx;
            if x < c.left || x >= c.right {
                continue;
            }
            let mut coverage = g.coverage[gy as usize * g.width as usize + gx as usize] as i32;
            if !f.antialiased {
                coverage = if coverage >= 128 { 255 } else { 0 };
            }
            if coverage != 0 {
                blend(&mut *pixel(t, x, y), color, coverage, t.flags & TARGET_PREMULTIPLIED != 0);
            }
        }
    }
}

/// `PlatFontDraw`.
///
/// # Safety
/// `target` describes pixels that can be written for its whole rectangle.
#[allow(clippy::too_many_arguments)]
pub unsafe fn draw(
    font: &PlatFont,
    text: &[u8],
    x: i32,
    y: i32,
    color: u32,
    opaque: bool,
    background: u32,
    clip: Option<PlatFontRect>,
    target: &PlatFontTarget,
) {
    let mut c = Clip { left: 0, top: 0, right: target.width, bottom: target.height };
    if let Some(r) = clip {
        c.left = c.left.max(r.left);
        c.top = c.top.max(r.top);
        c.right = c.right.min(r.right);
        c.bottom = c.bottom.min(r.bottom);
    }
    if c.left >= c.right || c.top >= c.bottom {
        return;
    }
    let width = font.measure(text);
    if opaque {
        fill(target, c, x, y, x + width, y + font.metrics.height, background);
    }
    let baseline = y + font.metrics.ascent;
    let mut pen = x;
    for (_, cp, _) in utf8::chars(text) {
        let g = &*font.glyph(cp);
        if g.width != 0 {
            draw_glyph(font, g, target, c, pen, baseline, color);
            if font.synthetic_bold {
                draw_glyph(font, g, target, c, pen + 1, baseline, color);
            }
        }
        pen += g.advance;
    }
    if font.underline {
        let uy = baseline + font.underline_offset;
        fill(target, c, x, uy, x + width, uy + font.underline_thickness, color);
    }
}

// ---- the C functions (platform/PlatFont.h)

/// # Safety
/// `face` and `data` as the C header documents; `data` stays valid while fonts of the face exist.
#[no_mangle]
#[allow(non_snake_case)]
pub unsafe extern "C" fn PlatFontRegister(
    face: *const c_char,
    bold: c_int,
    italic: c_int,
    data: *const c_void,
    size: u32,
) -> c_int {
    if face.is_null() || data.is_null() || size < 12 {
        return 0;
    }
    let data: &'static [u8] = std::slice::from_raw_parts(data as *const u8, size as usize);
    let ok = REGISTRY.lock().unwrap_or_else(|e| e.into_inner()).register(c_bytes(face), bold != 0, italic != 0, data);
    i32::from(ok)
}

/// # Safety
/// Both are NUL-terminated strings, or NULL (refused).
#[no_mangle]
#[allow(non_snake_case)]
pub unsafe extern "C" fn PlatFontAlias(face: *const c_char, target: *const c_char) -> c_int {
    if face.is_null() || target.is_null() {
        return 0;
    }
    REGISTRY.lock().unwrap_or_else(|e| e.into_inner()).alias(c_bytes(face), c_bytes(target));
    1
}

/// # Safety
/// `face` is NULL or a NUL-terminated string.
#[no_mangle]
#[allow(non_snake_case)]
pub unsafe extern "C" fn PlatFontCreate(
    face: *const c_char,
    height: i32,
    bold: c_int,
    italic: c_int,
    underline: c_int,
    antialiased: c_int,
) -> *mut PlatFont {
    let name = c_bytes(face);
    let font = {
        let registry = REGISTRY.lock().unwrap_or_else(|e| e.into_inner());
        create(&registry, name, height, bold, italic, underline, antialiased)
    };
    font.map_or(std::ptr::null_mut(), Box::into_raw)
}

/// # Safety
/// `font` is NULL or came from `PlatFontCreate` and has not been destroyed.
#[no_mangle]
#[allow(non_snake_case)]
pub unsafe extern "C" fn PlatFontDestroy(font: *mut PlatFont) {
    if !font.is_null() {
        drop(Box::from_raw(font));
    }
}

/// # Safety
/// `font` is NULL or a live font.
#[no_mangle]
#[allow(non_snake_case)]
pub unsafe extern "C" fn PlatFontScaled(font: *const PlatFont, scale: f32) -> *const PlatFont {
    if font.is_null() {
        return font;
    }
    let registry = REGISTRY.lock().unwrap_or_else(|e| e.into_inner());
    (*font).scaled(&registry, scale)
}

/// # Safety
/// `font` is live, `out` writable.
#[no_mangle]
#[allow(non_snake_case)]
pub unsafe extern "C" fn PlatFontGetMetrics(font: *const PlatFont, out: *mut PlatFontMetrics) {
    *out = (*font).metrics;
}

/// # Safety
/// `text` holds `length` bytes.
#[no_mangle]
#[allow(non_snake_case)]
pub unsafe extern "C" fn PlatFontMeasure(font: *const PlatFont, text: *const c_char, length: u32) -> i32 {
    if length == 0 {
        return 0;
    }
    (*font).measure(std::slice::from_raw_parts(text as *const u8, length as usize))
}

/// # Safety
/// As `draw`; `clip` may be NULL.
#[no_mangle]
#[allow(non_snake_case, clippy::too_many_arguments)]
pub unsafe extern "C" fn PlatFontDraw(
    font: *const PlatFont,
    text: *const c_char,
    length: u32,
    x: i32,
    y: i32,
    color: u32,
    opaque: c_int,
    background: u32,
    clip: *const PlatFontRect,
    target: *const PlatFontTarget,
) {
    let bytes: &[u8] = if length == 0 { &[] } else { std::slice::from_raw_parts(text as *const u8, length as usize) };
    let clip = if clip.is_null() { None } else { Some(*clip) };
    draw(&*font, bytes, x, y, color, opaque != 0, background, clip, &*target);
}

#[cfg(test)]
mod tests {
    use super::*;

    const FONT: &[u8] = include_bytes!("golden_font.ttf");
    const GOLDEN: &str = include_str!("golden_font.txt");

    fn registry() -> Registry {
        let mut r = Registry::new();
        assert!(r.register(b"Synth", false, false, FONT));
        r.alias(b"Alias1", b"Synth");
        r
    }

    fn make(h: i32, b: i32, i: i32, u: i32, a: i32) -> Box<PlatFont> {
        create(&registry(), b"synth", h, b, i, u, a).unwrap()
    }

    fn fnv(p: &[u8]) -> u64 {
        let mut h = 1469598103934665603u64;
        for &b in p {
            h ^= b as u64;
            h = h.wrapping_mul(1099511628211);
        }
        h
    }

    /// The glyph the golden's cell `c` (a Windows-1252 byte of the GDI port's 256-cell table) holds.
    fn cell(font: &PlatFont, c: usize) -> &Glyph {
        // SAFETY: the pointer lives as long as `font`
        unsafe { &*font.glyph(utf8::cp1252_to_codepoint(c as u8)) }
    }

    fn check_font_block(label: &str, lines: &mut std::str::Lines<'_>) -> usize {
        // label "h-8_b0_i0_u0_a1"
        let v: Vec<i32> = label.split('_').map(|p| p[1..].parse().unwrap()).collect();
        let font = make(v[0], v[1], v[2], v[3], v[4]);
        let mut count = 0usize;
        #[allow(clippy::explicit_counter_loop)]
        for line in lines.by_ref() {
            if line.starts_with("measure ") {
                let parts: Vec<&str> = line.split_whitespace().collect();
                assert_eq!(font.measure(b"ABCDE "), parts[1].parse::<i32>().unwrap(), "{label}");
                // the golden measured the byte 0x80: the euro sign in Windows-1252
                assert_eq!(font.measure("€".as_bytes()), parts[2].parse::<i32>().unwrap(), "{label}");
                return count;
            }
            let p: Vec<&str> = line.split_whitespace().collect();
            assert_eq!(p[0], "g");
            let c: usize = p[1].parse().unwrap();
            let g = cell(&font, c);
            assert_eq!(g.advance, p[2].parse::<i32>().unwrap(), "{label} advance {c}");
            assert_eq!(g.x0, p[3].parse::<i32>().unwrap(), "{label} x0 {c}");
            assert_eq!(g.y0, p[4].parse::<i32>().unwrap(), "{label} y0 {c}");
            assert_eq!(g.width, p[5].parse::<i32>().unwrap(), "{label} w {c}");
            assert_eq!(g.height, p[6].parse::<i32>().unwrap(), "{label} h {c}");
            if p.len() > 8 {
                let expect: Vec<u8> =
                    (0..p[8].len()).step_by(2).map(|i| u8::from_str_radix(&p[8][i..i + 2], 16).unwrap()).collect();
                assert_eq!(g.coverage, expect, "{label} coverage {c}");
            }
            assert_eq!(format!("{:016x}", fnv(&g.coverage)), p[7], "{label} hash {c}");
            count += 1;
        }
        panic!("no measure line for {label}");
    }

    #[test]
    fn every_glyph_and_metric_matches_stb_on_the_golden_font() {
        let mut lines = GOLDEN.lines();
        let mut fonts = 0;
        let mut glyphs = 0;
        while let Some(line) = lines.next() {
            if !line.starts_with("font ") {
                continue;
            }
            let p: Vec<&str> = line.split_whitespace().collect();
            let label = p[1];
            let v: Vec<i32> = label.split('_').map(|t| t[1..].parse().unwrap()).collect();
            let f = make(v[0], v[1], v[2], v[3], v[4]);
            let m = f.metrics();
            let vals: Vec<i32> = p[3..12].iter().map(|t| t.parse().unwrap()).collect();
            assert_eq!(m.height, vals[0], "{label}");
            assert_eq!(m.ascent, vals[1], "{label}");
            assert_eq!(m.descent, vals[2], "{label}");
            assert_eq!(m.internal_leading, vals[3], "{label}");
            assert_eq!(m.external_leading, vals[4], "{label}");
            assert_eq!(m.ave_char_width, vals[5], "{label}");
            assert_eq!(m.max_char_width, vals[6], "{label}");
            assert_eq!(m.first_char as i32, vals[7], "{label}");
            assert_eq!(m.last_char as i32, vals[8], "{label}");
            assert_eq!(f.underline_offset, p[13].parse::<i32>().unwrap(), "{label}");
            assert_eq!(f.underline_thickness, p[14].parse::<i32>().unwrap(), "{label}");
            assert_eq!(i32::from(f.synthetic_bold), p[16].parse::<i32>().unwrap(), "{label}");
            drop(f);
            glyphs += check_font_block(label, &mut lines);
            fonts += 1;
        }
        assert_eq!(fonts, 7);
        assert!(glyphs >= 40, "only {glyphs} glyphs compared");
    }

    #[test]
    fn wide_of_every_byte_is_the_golden_table() {
        // the Windows-1252 table the GDI port drew bytes with, from the C++ golden: now the conversion of old inputs
        let line = GOLDEN.lines().find(|l| l.starts_with("wide ")).unwrap();
        let values: Vec<u32> = line.split_whitespace().skip(1).map(|t| u32::from_str_radix(t, 16).unwrap()).collect();
        assert_eq!(values.len(), 256);
        for (i, &v) in values.iter().enumerate() {
            assert_eq!(utf8::cp1252_to_codepoint(i as u8), v, "byte {i:#x}");
        }
    }

    #[test]
    fn text_is_measured_and_drawn_by_code_point() {
        let f = make(-12, 0, 0, 0, 1);
        // the same character, as a Windows-1252 byte once drew it and as UTF-8 now: the same glyph
        let euro = cell(&f, 0x80);
        assert_eq!(f.measure("€".as_bytes()), euro.advance);
        assert_eq!(
            f.measure("aé€".as_bytes()),
            cell(&f, b'a' as usize).advance + cell(&f, 0xE9).advance + euro.advance
        );
        // a stray byte, a surrogate and a character the font lacks draw a question mark
        let question = cell(&f, b'?' as usize).advance;
        assert_eq!(f.measure(&[0xC3]), question);
        assert_eq!(f.measure(&[0xED, 0xA0, 0x80]), 3 * question, "a surrogate's bytes are three strays");
        assert_eq!(f.measure("\u{4E2D}".as_bytes()), question);
        assert_eq!(f.measure("\u{81}".as_bytes()), question, "a C1 control, as the undefined byte 0x81 drew");
        // a glyph past U+00FF is rendered once and kept
        let a = f.glyph(0x20AC);
        assert_eq!(f.glyph(0x20AC), a);
        assert!(f.extra.lock().unwrap().contains_key(&0x20AC));
    }

    #[allow(clippy::too_many_arguments)]
    fn draw_case(
        text: &[u8],
        x: i32,
        y: i32,
        c: u32,
        op: bool,
        bg: u32,
        clip: Option<PlatFontRect>,
        flags: u32,
        h: i32,
        b: i32,
        u: i32,
        a: i32,
    ) -> Vec<u8> {
        let font = make(h, b, 0, u, a);
        let mut buf = vec![0x55u8; 40 * 16 * 4];
        let t =
            PlatFontTarget { pixels: buf.as_mut_ptr().cast(), pitch: 40 * 4, width: 40, height: 16, flags, scale: 0.0 };
        unsafe { draw(&font, text, x, y, c, op, bg, clip, &t) };
        buf
    }

    #[test]
    fn draws_match_the_golden_hashes() {
        let draws: Vec<&str> = GOLDEN.lines().filter(|l| l.starts_with("draw ")).collect();
        let hash = |s: &str| s.rsplit(' ').next().unwrap().to_string();
        let rect = PlatFontRect { left: 2, top: 3, right: 30, bottom: 12 };
        let cases = [
            draw_case(b"ABC", 1, 1, 0x102030, false, 0, None, 0, -12, 0, 0, 1),
            draw_case(b"ABC", 1, 1, 0xAA2030, true, 0x000011, None, 0, -12, 0, 0, 0),
            draw_case(b"A CDE", 0, 2, 0x00FF00, false, 0, Some(rect), 0, -12, 1, 1, 1),
            draw_case(b"BB", -3, -2, 0xFFFFFF, false, 0, None, TARGET_PREMULTIPLIED, -16, 0, 0, 1),
            draw_case(b"CD", 5, 0, 0x808080, true, 0xFF00FF, None, TARGET_PREMULTIPLIED, -16, 0, 1, 1),
        ];
        assert_eq!(draws.len(), cases.len());
        for (line, case) in draws.iter().zip(cases.iter()) {
            assert_eq!(format!("{:016x}", fnv(case)), hash(line), "{line}");
        }
    }

    #[test]
    fn a_latin1_string_draws_the_same_as_its_cp1252_bytes_did() {
        // what the GDI port drew for the bytes "caf\xe9 \x80" is what the UTF-8 text draws now
        let font = make(-12, 0, 0, 0, 1);
        let paint = |text: &[u8]| {
            let mut buf = vec![0x55u8; 40 * 16 * 4];
            let t = PlatFontTarget {
                pixels: buf.as_mut_ptr().cast(),
                pitch: 40 * 4,
                width: 40,
                height: 16,
                flags: 0,
                scale: 0.0,
            };
            unsafe { draw(&font, text, 1, 1, 0x102030, false, 0, None, &t) };
            buf
        };
        let old: Vec<u8> = b"caf\xe9 \x80".iter().flat_map(|&b| utf8::from_cp1252(&[b])).collect();
        assert_eq!(old, "café €".as_bytes());
        let ink = paint(&old);
        assert_ne!(ink, vec![0x55u8; 40 * 16 * 4]);
        assert_eq!(ink, paint("café €".as_bytes()));
    }

    #[test]
    fn empty_clip_draws_nothing() {
        let c = draw_case(
            b"AB",
            1,
            1,
            0,
            true,
            0,
            Some(PlatFontRect { left: 5, top: 5, right: 5, bottom: 9 }),
            0,
            -12,
            0,
            0,
            1,
        );
        assert!(c.iter().all(|&b| b == 0x55));
    }

    #[test]
    fn scaled_returns_self_at_or_below_one_and_caches() {
        let golden_scaled: Vec<&str> = GOLDEN.lines().filter(|l| l.starts_with("scaled")).collect();
        let reg = registry();
        let f = create(&reg, b"synth", -12, 0, 0, 0, 1).unwrap();
        assert_eq!(f.scaled(&reg, 1.0), &*f as *const PlatFont);
        assert_eq!(f.scaled(&reg, 0.5), &*f as *const PlatFont);
        let a = f.scaled(&reg, 1.5);
        assert_ne!(a, &*f as *const PlatFont);
        assert_eq!(f.scaled(&reg, 1.5), a);
        let height = unsafe { (*a).metrics.height };
        let expect: i32 = golden_scaled[0].split_whitespace().nth(1).unwrap().parse().unwrap();
        assert_eq!(height, expect);
        // NaN is not > 1.01: the same font, as the C's !(scale > 1.01f)
        assert_eq!(f.scaled(&reg, f32::NAN), &*f as *const PlatFont);
    }

    #[test]
    fn face_scoring_and_alias_chain() {
        let mut r = Registry::new();
        assert!(r.register(b"Serif", false, false, FONT));
        assert!(r.register(b"Serif", true, false, FONT));
        assert!(r.register(b"Other", false, true, FONT));
        r.alias(b"A", b"B");
        r.alias(b"B", b"Serif");
        // name + bold beats name alone
        let f = r.find_face(b"serif", true, false).unwrap();
        assert!(f.bold);
        // the alias chain is applied in order; "a" -> "B" -> "Serif"
        let f = r.find_face(b"a", false, false).unwrap();
        assert_eq!(f.name, b"Serif".to_vec());
        // an unknown name falls back to the best style match (the first of equal score)
        let f = r.find_face(b"Nobody", false, true).unwrap();
        assert_eq!(f.name, b"Other".to_vec());
        assert!(Registry::new().find_face(b"Nobody", false, false).is_none());
    }

    #[test]
    fn register_refuses_non_fonts() {
        let mut r = Registry::new();
        assert!(!r.register(b"Broken", false, false, b"not a font!!")); // 12 bytes, no font
        assert!(!r.register(b"Short", false, false, b"short"));
        assert!(r.register(b"Good", false, false, FONT));
    }

    #[test]
    fn struct_layouts_match_the_header_shape() {
        use std::mem::{offset_of, size_of};
        assert_eq!(size_of::<PlatFontMetrics>(), 32);
        assert_eq!(offset_of!(PlatFontMetrics, first_char), 28);
        assert_eq!(offset_of!(PlatFontMetrics, last_char), 29);
        assert_eq!(size_of::<PlatFontRect>(), 16);
        assert_eq!(size_of::<PlatFontTarget>(), 32);
        assert_eq!(offset_of!(PlatFontTarget, pitch), 8);
        assert_eq!(offset_of!(PlatFontTarget, flags), 20);
        assert_eq!(offset_of!(PlatFontTarget, scale), 24);
    }

    #[test]
    fn exports_refuse_nulls_and_null_font_scaled() {
        unsafe {
            assert_eq!(PlatFontRegister(std::ptr::null(), 0, 0, FONT.as_ptr().cast(), FONT.len() as u32), 0);
            assert_eq!(PlatFontRegister(c"x".as_ptr(), 0, 0, std::ptr::null(), 50), 0);
            assert_eq!(PlatFontRegister(c"x".as_ptr(), 0, 0, FONT.as_ptr().cast(), 10), 0);
            assert_eq!(PlatFontAlias(std::ptr::null(), c"x".as_ptr()), 0);
            assert!(PlatFontScaled(std::ptr::null(), 2.0).is_null());
            PlatFontDestroy(std::ptr::null_mut());
        }
    }

    #[test]
    fn c_helpers() {
        assert_eq!(unsafe { c_bytes(std::ptr::null()) }, b"");
        assert_eq!(unsafe { c_bytes(c"hi".as_ptr()) }, b"hi");
        assert_eq!(c_terminated(b"ab\0cd"), b"ab\0".to_vec());
    }

    /// Run by hand against a dump of the C++ over the Liberation fonts:
    /// `EE_FONT_DUMP=<dump> EE_FONT_DIR=<toolchain/fonts> cargo test -p ee_plat_ui -- --ignored liberation`
    #[test]
    #[ignore = "needs the Liberation fonts and a C++ dump (see the comment)"]
    fn liberation_fonts_against_a_cpp_dump() {
        let dump = std::env::var("EE_FONT_DUMP").expect("EE_FONT_DUMP");
        let dir = std::env::var("EE_FONT_DIR").expect("EE_FONT_DIR");
        let text = std::fs::read_to_string(dump).unwrap();
        let files = [
            "LiberationSans-Regular.ttf",
            "LiberationSans-Bold.ttf",
            "LiberationSerif-Regular.ttf",
            "LiberationMono-Regular.ttf",
            "LiberationSans-Italic.ttf",
        ];
        let mut reg = Registry::new();
        for (i, name) in files.iter().enumerate() {
            let data: &'static [u8] = Box::leak(std::fs::read(format!("{dir}/{name}")).unwrap().into_boxed_slice());
            assert!(reg.register(format!("Lib{i}").as_bytes(), false, false, data));
        }
        let mut compared = 0u64;
        let mut lines = text.lines().peekable();
        while let Some(line) = lines.next() {
            let p: Vec<&str> = line.split_whitespace().collect();
            assert_eq!(p[0], "font");
            let label: Vec<&str> = p[1].split('_').collect();
            let name = label[0];
            let h: i32 = label[1][1..].parse().unwrap();
            let b: i32 = label[2][1..].parse().unwrap();
            let f = create(&reg, name.as_bytes(), h, b, 0, 0, 1).unwrap();
            let vals: Vec<i32> = p[3..12].iter().map(|t| t.parse().unwrap()).collect();
            let m = f.metrics();
            assert_eq!(
                [
                    m.height,
                    m.ascent,
                    m.descent,
                    m.internal_leading,
                    m.external_leading,
                    m.ave_char_width,
                    m.max_char_width
                ],
                [vals[0], vals[1], vals[2], vals[3], vals[4], vals[5], vals[6]],
                "{}",
                p[1]
            );
            while let Some(gl) = lines.peek() {
                if !gl.starts_with("g ") {
                    break;
                }
                let q: Vec<&str> = gl.split_whitespace().collect();
                let c: usize = q[1].parse().unwrap();
                let g = cell(&f, c);
                assert_eq!(g.advance, q[2].parse::<i32>().unwrap(), "{} {c}", p[1]);
                assert_eq!(g.x0, q[3].parse::<i32>().unwrap(), "{} {c}", p[1]);
                assert_eq!(g.y0, q[4].parse::<i32>().unwrap(), "{} {c}", p[1]);
                assert_eq!(g.width, q[5].parse::<i32>().unwrap(), "{} {c}", p[1]);
                assert_eq!(g.height, q[6].parse::<i32>().unwrap(), "{} {c}", p[1]);
                assert_eq!(format!("{:016x}", fnv(&g.coverage)), q[7], "{} coverage of {c}", p[1]);
                compared += 1;
                lines.next();
            }
        }
        println!("compared {compared} glyphs");
        assert!(compared > 10000);
    }
}
