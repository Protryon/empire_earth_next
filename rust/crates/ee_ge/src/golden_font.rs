//! `GEText`'s extent and `GEFontBitmap`'s cells, alpha pass and draws against the real C++ (`golden_font.txt`). A
//! scratch driver (`build/font_golden`, PORT_LEARNINGS "R4.3, sixth part") compiled the unmodified `GEText.cpp`,
//! `GEFont.cpp` and `GEFontBitmap.cpp` (their includes replaced by fakes, the real `UWideString.cpp` and value
//! headers kept) over fake fonts (an advance per code point from a hash, plus one pixel a measured run so a chunked
//! measure differs from a whole one), a fake rasterizer that writes a glyph pattern into the fake surfaces, fake
//! textures and a fake `GEBitmap` that logs every call. Each case's log is in the golden as its line count and an
//! FNV-1a 64 digest under a header that carries the case's inputs: the fonts (`F`), the texts (`T`, hex UTF-8:
//! empty, one glyph, Latin-1, the 1252 specials, Greek, CJK, emoji, stray and truncated bytes, controls, 255 to 1,000
//! characters around the 256-character chunks), the extents (`X`, each text in each font and again after
//! `SetFont`), the six font bitmaps (`B`: 0..255, 32..126, 65..90 characters, 1 to 2 textures, row padding 0 to 2,
//! one texture with a dropped MIP level), 234 unclipped draws (`D`) of 13 texts at three locations and 1,872 clipped
//! ones (`C`:
//! containing, exactly the text's cell, cut on every side, half, outside, one point, fractional, inverted), and the
//! alpha pass over 60 surfaces of every shape (`A`). This test replays every case through `font.rs` with the same
//! fakes and compares the digests; the driver relinked over the forwarders printed the same file.

// `c_ulong` is `u32` here but `u64` on LP64: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::ffi::c_ulong;
use std::fmt::Write;

use ee_plat_core::string::{PlatCp1252ToUtf8, PlatMbsLen, PlatUtf8Decode};

use crate::bitmap::stretched_coords;
use crate::font::tests::Plat;
use crate::font::{
    draw_plan, surface_alpha, surface_alpha_len, text_extent, GeFontPacker, GeGlyphDraw, GE_GLYPH_FULL,
    GE_GLYPH_SKIPPED,
};
use crate::types::{Ge2DPoint, GeRectangle};

const GOLDEN: &str = include_str!("golden_font.txt");

fn mix(mut x: u32) -> u32 {
    x = x.wrapping_mul(0x9E37_79B1);
    x ^= x >> 15;
    x = x.wrapping_mul(0x85EB_CA77);
    x ^= x >> 13;
    x
}

fn b(f: f32) -> String {
    format!("{:08x}", f.to_bits())
}

fn hex_decode(s: &str) -> Vec<u8> {
    if s == "-" {
        return Vec::new();
    }
    (0..s.len()).step_by(2).map(|i| u8::from_str_radix(&s[i..i + 2], 16).unwrap()).collect()
}

fn hex(s: &[u8]) -> String {
    if s.is_empty() {
        return "-".into();
    }
    s.iter().map(|c| format!("{c:02x}")).collect()
}

fn word(s: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(s, 16).unwrap())
}

#[derive(Clone)]
struct Font {
    id: usize,
    height: i32,
    first: u8,
    last: u8,
    seed: u32,
    scale: i32,
}

struct Cell {
    rect: GeRectangle,
    texture: usize,
    top_left: [f32; 2],
    bottom_right: [f32; 2],
    x_scale: f32,
    y_scale: f32,
}

struct Surface {
    id: usize,
    width: c_ulong,
    height: c_ulong,
    pitch: c_ulong,
    memory: Vec<u16>,
}

struct Bitmap {
    cells: Vec<Cell>,
    first: c_ulong,
    /// the textures' sizes as `GETexture` reports them (width and height after the drop) and the drop
    texture_size: (c_ulong, c_ulong, c_ulong),
}

struct Replay {
    log: String,
    fonts: Vec<Option<Font>>,
    texts: Vec<Vec<u8>>,
    bitmaps: Vec<Option<Bitmap>>,
    draws: i64,
}

fn z(s: &[u8]) -> Vec<u8> {
    let mut v = s.to_vec();
    v.push(0);
    v
}

fn digest(memory: &[u16]) -> u64 {
    let mut h: u64 = 0xcbf2_9ce4_8422_2325;
    for &p in memory {
        h = (h ^ u64::from(p & 0xff)).wrapping_mul(0x100_0000_01b3);
        h = (h ^ u64::from(p >> 8)).wrapping_mul(0x100_0000_01b3);
    }
    h
}

/// The fake `PlatFontMeasure`: an advance per code point, one pixel more for a measured run; logged.
fn fake_measure(log: &mut String, font: &Font, s: &[u8]) -> i32 {
    let zs = z(s);
    let mut w = 0i32;
    let mut n = 0u32;
    let mut at = 0usize;
    while at < s.len() {
        let mut l = 0u32;
        // SAFETY: terminated
        let c = unsafe { PlatUtf8Decode(zs[at..].as_ptr().cast(), &mut l) };
        if l == 0 {
            break;
        }
        w += (mix(font.seed ^ c) % 9) as i32 * font.scale;
        n += 1;
        at += l as usize;
    }
    if n != 0 {
        w += 1;
    }
    let mut h: u32 = 2_166_136_261;
    for &c in s {
        h = (h ^ u32::from(c)).wrapping_mul(16_777_619);
    }
    writeln!(log, "  measure f{} {} {h:08x} -> {w}", font.id, s.len()).unwrap();
    w
}

impl Replay {
    /// `GEText::SetText`/`SetFont`'s `ComputeTextExtent` through `font.rs`: unchanged for a font without a
    /// `PlatFont`.
    fn extent(&mut self, font: &Option<Font>, text: &[u8], extent: &mut GeRectangle) {
        let Some(f) = font.clone() else { return };
        let zt = z(text);
        // SAFETY: terminated
        let characters = unsafe { PlatMbsLen(zt.as_ptr()) } as c_ulong;
        let log = &mut self.log;
        *extent = text_extent(&zt, characters, f.height, &Plat, |s| fake_measure(log, &f, s));
    }

    fn log_extent(&mut self, e: &GeRectangle) {
        writeln!(self.log, "  extent {} {} {} {}", b(e.left), b(e.top), b(e.right), b(e.bottom)).unwrap();
    }

    fn uv_line(&mut self, uv: [[f32; 2]; 4]) {
        writeln!(
            self.log,
            "  b uv {} {} {} {} {} {} {} {}",
            b(uv[0][0]),
            b(uv[0][1]),
            b(uv[1][0]),
            b(uv[1][1]),
            b(uv[2][0]),
            b(uv[2][1]),
            b(uv[3][0]),
            b(uv[3][1])
        )
        .unwrap();
    }

    /// The fake `GEBitmap::ComputeStretchedCoordsAndScale`: logs, stretches, sets -> the coordinates and scale.
    fn stretch(
        &mut self,
        t: &GeRectangle,
        s: &GeRectangle,
        d: &GeRectangle,
        size: (c_ulong, c_ulong, c_ulong),
    ) -> ([[f32; 2]; 4], f32, f32) {
        writeln!(
            self.log,
            "  b stretch {} {} {} {} | {} {} {} {} | {} {} {} {} 0",
            b(t.left),
            b(t.top),
            b(t.right),
            b(t.bottom),
            b(s.left),
            b(s.top),
            b(s.right),
            b(s.bottom),
            b(d.left),
            b(d.top),
            b(d.right),
            b(d.bottom)
        )
        .unwrap();
        let st = stretched_coords(t, s, d, size.0, size.1, size.2);
        self.uv_line(st.uv);
        writeln!(self.log, "  b scale {} {}", b(st.x_scale), b(st.y_scale)).unwrap();
        (st.uv, st.x_scale, st.y_scale)
    }

    fn alpha(&mut self, s: &mut Surface) {
        writeln!(self.log, "  lock s{}", s.id).unwrap();
        let n = surface_alpha_len(s.width, s.height, s.pitch);
        surface_alpha(&mut s.memory[..n], s.width, s.height, s.pitch);
        writeln!(self.log, "  unlock s{}", s.id).unwrap();
    }

    /// `GEFontBitmap`'s constructor with the fakes, its layout through `GeFontPacker`.
    fn construct(&mut self, fi: usize, pad: c_ulong, drop: c_ulong) {
        let font = self.fonts[fi].clone().unwrap();
        writeln!(self.log, "  getscreen 0\n  getblending 0\n  setblending 5 6 0\n  bitmap new").unwrap();
        let mut surfaces: Vec<Surface> = Vec::new();
        let mut refs: Vec<u64> = Vec::new();
        let mut cells = Vec::new();
        let mut packer = GeFontPacker::default();
        let size = (256 >> drop, 256 >> drop, drop);
        let mut extent = GeRectangle::default();
        for character in u32::from(font.first)..=u32::from(font.last) {
            let byte = character as u8 as std::ffi::c_char;
            let mut cell = [0 as std::ffi::c_char; 8];
            // SAFETY: valid buffers
            unsafe { PlatCp1252ToUtf8(&byte, 1, cell.as_mut_ptr(), 8) };
            let text: Vec<u8> = cell.iter().take_while(|&&c| c != 0).map(|&c| c as u8).collect();
            self.extent(&Some(font.clone()), &text, &mut extent);
            let (rect, new_texture) = packer.place(extent.right - extent.left, extent.bottom - extent.top);
            if new_texture {
                if let Some(s) = surfaces.last_mut() {
                    writeln!(self.log, "  textunlock s{}", s.id).unwrap();
                    self.alpha(s);
                }
                let id = surfaces.len();
                writeln!(self.log, "  createsurface 3 2 16 1 256 256 0 0 0 0").unwrap();
                writeln!(self.log, "  clear s{id} 00000000 00000000 00000000 3f800000").unwrap();
                writeln!(self.log, "  maketexture t{id} s{id}").unwrap();
                for (what, a) in [("min", 0), ("mag", 0), ("mip", 0), ("addr", 3), ("alpha", 2), ("color", 4)] {
                    writeln!(self.log, "  t{id} {what} {a} -1").unwrap();
                }
                surfaces.push(Surface {
                    id,
                    width: 256,
                    height: 256,
                    pitch: (256 + pad) * 2,
                    memory: vec![0; ((256 + pad) * 256) as usize],
                });
                refs.push(1);
            }
            let t = surfaces.len() - 1;
            // the fake glyph
            let zt = z(&text);
            let mut l = 0u32;
            // SAFETY: terminated
            let c = unsafe { PlatUtf8Decode(zt.as_ptr().cast(), &mut l) };
            let (lx, ly) = (rect.left, rect.top);
            let w = (extent.right - extent.left) as i32;
            let h = (extent.bottom - extent.top) as i32;
            writeln!(self.log, "  drawtext s{t} {} {} {w} {h} {} 1 0", b(lx), b(ly), hex(&text)).unwrap();
            let s = &mut surfaces[t];
            let stride = (s.pitch / 2) as usize;
            for y in 0..h {
                for x in 0..w {
                    let px = (lx as i32 + x) as u32 as c_ulong;
                    let py = (ly as i32 + y) as u32 as c_ulong;
                    if px < s.width && py < s.height {
                        let mask = if (x + y) % 3 == 0 { 0xFF0F } else { 0xFFFF };
                        let v =
                            mix(c.wrapping_mul(7919).wrapping_add((x as u32).wrapping_mul(131)).wrapping_add(y as u32))
                                & mask;
                        s.memory[py as usize * stride + px as usize] = v as u16;
                    }
                }
            }
            refs[t] += 1;
            writeln!(self.log, "  b tex t{t} 0").unwrap();
            let r = GeRectangle { bottom: rect.bottom - 1.0, left: rect.left, right: rect.right - 1.0, top: rect.top };
            let (uv, xs, ys) = self.stretch(&r, &r, &rect, size);
            cells.push(Cell { rect, texture: t, top_left: uv[0], bottom_right: uv[3], x_scale: xs, y_scale: ys });
        }
        let last = surfaces.len() - 1;
        writeln!(self.log, "  textunlock s{last}").unwrap();
        self.alpha(&mut surfaces[last]);
        writeln!(self.log, "  setblending 11 12 0").unwrap();
        writeln!(self.log, "  attached 1 refs 2 first {} cells {}", font.first, cells.len()).unwrap();
        for (i, c) in cells.iter().enumerate() {
            writeln!(
                self.log,
                "  cell {i} t{} {} {} {} {} {} {} {} {} {} {}",
                c.texture,
                b(c.rect.left),
                b(c.rect.top),
                b(c.rect.right),
                b(c.rect.bottom),
                b(c.top_left[0]),
                b(c.top_left[1]),
                b(c.bottom_right[0]),
                b(c.bottom_right[1]),
                b(c.x_scale),
                b(c.y_scale)
            )
            .unwrap();
        }
        for s in &surfaces {
            writeln!(self.log, "  texture t{} s{} refs {} digest {:016x}", s.id, s.id, refs[s.id], digest(&s.memory))
                .unwrap();
        }
        self.bitmaps[fi] = Some(Bitmap { cells, first: c_ulong::from(font.first), texture_size: size });
    }

    /// `GEFontBitmap::Draw` (clip `None`) or its clipped form, through `draw_plan`.
    fn draw(&mut self, fi: usize, text: &[u8], location: Ge2DPoint, color: [f32; 3], clip: Option<&GeRectangle>) {
        let bitmap = self.bitmaps[fi].take().unwrap();
        let rects: Vec<GeRectangle> = bitmap.cells.iter().map(|c| c.rect).collect();
        let mut plan: Vec<GeGlyphDraw> = Vec::new();
        draw_plan(&z(text), bitmap.first, &rects, location, clip, &Plat, &mut plan);
        writeln!(self.log, "  b color {} {} {}", b(color[0]), b(color[1]), b(color[2])).unwrap();
        let mut result = 0i64;
        for g in &plan {
            let c = &bitmap.cells[g.cell as usize];
            writeln!(self.log, "  b tex t{} 0", c.texture).unwrap();
            if g.kind == GE_GLYPH_SKIPPED {
                continue;
            }
            if g.kind == GE_GLYPH_FULL {
                let (tl, br) = (c.top_left, c.bottom_right);
                self.uv_line([tl, [br[0], tl[1]], [tl[0], br[1]], br]);
                writeln!(self.log, "  b scale {} {}", b(c.x_scale), b(c.y_scale)).unwrap();
            } else {
                self.stretch(&g.destination, &g.destination, &g.source, bitmap.texture_size);
            }
            writeln!(self.log, "  b at {} {}", b(g.location.x), b(g.location.y)).unwrap();
            self.draws += 1;
            result = self.draws;
            writeln!(self.log, "  draw t{} 0 -> {}", c.texture, self.draws).unwrap();
        }
        writeln!(self.log, "  -> {result}").unwrap();
        self.bitmaps[fi] = Some(bitmap);
    }
}

fn fnv64(s: &str) -> (usize, u64) {
    let mut h: u64 = 0xcbf2_9ce4_8422_2325;
    for c in s.bytes() {
        h = (h ^ u64::from(c)).wrapping_mul(0x100_0000_01b3);
    }
    (s.bytes().filter(|&c| c == b'\n').count(), h)
}

#[test]
fn the_text_matches_the_cpp_golden() {
    let mut r = Replay { log: String::new(), fonts: Vec::new(), texts: Vec::new(), bitmaps: Vec::new(), draws: 0 };
    let mut counts = std::collections::BTreeMap::new();
    for line in GOLDEN.lines() {
        let w: Vec<&str> = line.split(' ').collect();
        if w[0] == "T" {
            r.texts.push(hex_decode(w[2]));
            continue;
        }
        let (fields, tail) = w.split_at(w.len() - 2);
        let text_of = |t: &str| -> usize { t[1..].parse().unwrap() };
        r.log.clear();
        match fields[0] {
            "F" => {
                let i: usize = fields[1].parse().unwrap();
                let (name, size, style, quality): (&str, i32, u32, u32) =
                    (fields[2], fields[3].parse().unwrap(), fields[4].parse().unwrap(), fields[5].parse().unwrap());
                let (add, first, last, scale): (i32, u8, u8, i32) = (
                    fields[6].parse().unwrap(),
                    fields[7].parse().unwrap(),
                    fields[8].parse().unwrap(),
                    fields[9].parse().unwrap(),
                );
                let bold = i32::from(style & 1 != 0);
                writeln!(
                    r.log,
                    "  fontcreate {name} {} {bold} {} {} {}",
                    -size,
                    (style >> 1) & 1,
                    (style >> 2) & 1,
                    i32::from(quality == 2)
                )
                .unwrap();
                let font = (scale != 0).then(|| Font {
                    id: i,
                    height: size + add + bold,
                    first,
                    last,
                    seed: mix(i as u32 + 1),
                    scale,
                });
                let height = font.as_ref().map_or(0, |f| f.height);
                writeln!(r.log, "  height {height} bitmap 0").unwrap();
                r.fonts.push(font);
                r.bitmaps.push(None);
            }
            "X" => {
                let fi: usize = fields[1].parse().unwrap();
                let text = r.texts[text_of(fields[2])].clone();
                let mut extent = GeRectangle::default();
                let font = r.fonts[fi].clone();
                r.extent(&font, &text, &mut extent);
                r.log_extent(&extent.clone());
                let next = r.fonts[(fi + 1) % r.fonts.len()].clone();
                r.extent(&next, &text, &mut extent);
                r.log_extent(&extent.clone());
            }
            "B" => {
                let fi: usize = fields[1].parse().unwrap();
                r.construct(fi, fields[2].parse().unwrap(), fields[3].parse().unwrap());
            }
            "D" | "C" => {
                let fi: usize = fields[1].parse().unwrap();
                let text = r.texts[text_of(fields[2])].clone();
                let location = Ge2DPoint { x: word(fields[3]), y: word(fields[4]) };
                let color = [word(fields[5]), word(fields[6]), word(fields[7])];
                if fields[0] == "D" {
                    let font = r.fonts[fi].clone();
                    let mut extent = GeRectangle::default();
                    r.extent(&font, &text, &mut extent);
                    r.draw(fi, &text, location, color, None);
                } else {
                    let clip = GeRectangle {
                        left: word(fields[8]),
                        top: word(fields[9]),
                        right: word(fields[10]),
                        bottom: word(fields[11]),
                    };
                    r.draw(fi, &text, location, color, Some(&clip));
                }
            }
            "A" => {
                let k: u32 = fields[1].parse().unwrap();
                let (width, height, pad): (c_ulong, c_ulong, c_ulong) =
                    (fields[2].parse().unwrap(), fields[3].parse().unwrap(), fields[4].parse().unwrap());
                let n = ((width + pad) * height) as usize;
                let memory = (0..n)
                    .map(|i| {
                        (mix(k.wrapping_mul(100_000).wrapping_add(i as u32)) & if i % 5 == 0 { 0xFF0F } else { 0xFFFF })
                            as u16
                    })
                    .collect();
                let mut s = Surface { id: 1000 + k as usize, width, height, pitch: (width + pad) * 2, memory };
                r.alpha(&mut s);
                writeln!(r.log, "  -> 0 digest {:016x}", digest(&s.memory)).unwrap();
            }
            other => panic!("unknown case {other}"),
        }
        let (lines, hash) = fnv64(&r.log);
        let want = (tail[0].parse::<usize>().unwrap(), u64::from_str_radix(tail[1], 16).unwrap());
        assert_eq!((lines, hash), want, "{}\n{}", fields.join(" "), r.log);
        *counts.entry(fields[0].to_string()).or_insert(0) += 1;
    }
    let counts: Vec<(String, i32)> = counts.into_iter().collect();
    assert_eq!(
        counts,
        [("A", 60), ("B", 6), ("C", 1872), ("D", 234), ("F", 7), ("X", 182)].map(|(k, n)| (k.to_string(), n)).to_vec()
    );
}
