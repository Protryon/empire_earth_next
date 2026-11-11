//! The Graphics Engine's text (`GEText.cpp`, `GEFontBitmap.cpp`): a text's measured extent, the glyph atlas's
//! cell layout and its alpha pass, and the bitmap font's draw as a plan of glyph draws. `GEFont`, `GEText` and
//! `GEFontBitmap` stay the C++ objects callers use (their members, reference counts, textures, surfaces and the
//! `GEBitmap` that draws); the C++ measures with `PlatFont`, renders each cell, creates the textures and runs the plan's
//! bitmap calls. The UI and the renderer use this (never the simulation); the C++'s `f32` operations and their order
//! are kept, so menus measure and draw as before.
//!
//! Text is UTF-8 (decision D6). The character stepping and decoding are the platform's (`PlatMbsNInc`,
//! `PlatUtf8Decode`, `PlatCp1252ByteOf`, `ee_plat_core`), reached through `TextOps` so this crate keeps no platform
//! dependency (the random map gate links it with its own platform stubs).

use std::ffi::{c_int, c_ulong};

use crate::pixel;
use crate::types::{Ge2DPoint, GeRectangle};

/// The glyph atlas textures' pixel format (`kPixelFormatA4R4G4B4`).
pub const GE_FONT_ATLAS_FORMAT: c_ulong = 2;
/// The glyph atlas textures' side in pixels (`CreateNewFontTexture`).
pub const GE_FONT_ATLAS_SIZE: f32 = 256.0;
/// `ComputeTextExtent` measures a long text in chunks of this many characters (GDI's limit once).
pub const GE_TEXT_MEASURE_CHUNK: c_ulong = 256;

/// The platform's text stepping, over NUL-terminated byte slices (each slice ends at or after its terminator).
pub trait TextOps {
    /// `PlatUtf8Decode`: the character at the start of `text` and its byte count (0 at the terminator).
    fn decode(&self, text: &[u8]) -> (u32, usize);
    /// `PlatMbsNInc`: the byte offset `characters` characters on (stopping at the terminator).
    fn next_chars(&self, text: &[u8], characters: usize) -> usize;
    /// `PlatCp1252ByteOf`: the character's Windows-1252 byte, -1 when the code page has none.
    fn cp1252_byte_of(&self, character: u32) -> c_int;
}

/// The bytes before the terminator.
fn strlen(text: &[u8]) -> usize {
    text.iter().position(|&b| b == 0).unwrap_or(text.len())
}

/// `GEText::ComputeTextExtent` once the font is known to exist: the text (`characters` is its `GetLength`) measured
/// in chunks of 256 characters and the rest, each chunk's width added in turn as a float; the height is the font's
/// cell (`height`, `tmHeight`). `measure` is `PlatFontMeasure` on the font.
pub fn text_extent(
    text: &[u8],
    characters: c_ulong,
    height: i32,
    ops: &impl TextOps,
    mut measure: impl FnMut(&[u8]) -> i32,
) -> GeRectangle {
    let mut right = 0.0f32;
    let mut at = 0usize;
    for _ in 0..characters / GE_TEXT_MEASURE_CHUNK {
        let next = at + ops.next_chars(&text[at..], GE_TEXT_MEASURE_CHUNK as usize);
        right = right + measure(&text[at..next]) as f32;
        at = next;
    }
    let end = at + strlen(&text[at..]);
    right = right + measure(&text[at..end]) as f32;
    GeRectangle { bottom: height as f32, left: 0.0, right, top: 0.0 }
}

/// The glyph atlas layout of `GEFontBitmap`'s constructor: the pen in the current texture and the row's height so far.
/// Starts zeroed (no texture yet).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct GeFontPacker {
    pub x: f32,
    pub y: f32,
    /// the tallest cell of the row (`unsigned long`: the cell heights are truncated into it)
    pub row_height: c_ulong,
    pub has_texture: bool,
}

impl GeFontPacker {
    /// Places the next cell of the given extent: a new row when it would reach the texture's right edge, a new
    /// texture (`true`; the pen back at the top left) when there is none yet or it would reach the bottom edge.
    /// The row height is not reset by a new texture (the original's). Returns the cell (left, top, right, bottom)
    /// and moves the pen past it.
    pub fn place(&mut self, width: f32, height: f32) -> (GeRectangle, bool) {
        if height > self.row_height as f32 {
            self.row_height = height as c_ulong;
        }
        if width + self.x >= GE_FONT_ATLAS_SIZE {
            self.y = self.y + self.row_height as f32;
            self.row_height = 0;
            self.x = 0.0;
        }
        let mut new_texture = false;
        if !self.has_texture || self.y + height >= GE_FONT_ATLAS_SIZE {
            new_texture = true;
            self.has_texture = true;
            self.x = 0.0;
            self.y = 0.0;
        }
        let rect = GeRectangle { bottom: self.y + height, left: self.x, right: width + self.x, top: self.y };
        self.x = (rect.right - rect.left) + self.x;
        (rect, new_texture)
    }
}

/// `GEFontBitmap::CreateSurfaceAlpha` over a locked surface's pixels (A4R4G4B4, `bytes_per_row` apart): a pixel whose
/// green is 0 becomes transparent black, any other white with that green as its alpha. `pixels` covers the rows
/// (the last one without its padding).
pub fn surface_alpha(pixels: &mut [u16], width: c_ulong, height: c_ulong, bytes_per_row: c_ulong) {
    let skip = bytes_per_row.wrapping_sub(width.wrapping_mul(2)) / 2;
    let mut at = 0usize;
    for _ in 0..height {
        for _ in 0..width {
            let green = pixel::unpack_green(GE_FONT_ATLAS_FORMAT, c_ulong::from(pixels[at]));
            pixels[at] = if green == 0 {
                pixel::pack(GE_FONT_ATLAS_FORMAT, 0, 0, 0, 0)
            } else {
                pixel::pack(GE_FONT_ATLAS_FORMAT, 255, 255, 255, c_ulong::from(green))
            } as u16;
            at += 1;
        }
        at = at.wrapping_add(skip as usize);
    }
}

/// The pixels `surface_alpha` touches: the rows' units, without the last row's padding.
pub fn surface_alpha_len(width: c_ulong, height: c_ulong, bytes_per_row: c_ulong) -> usize {
    if height == 0 || width == 0 {
        return 0;
    }
    let skip = bytes_per_row.wrapping_sub(width.wrapping_mul(2)) / 2;
    ((height - 1) as usize) * ((width + skip) as usize) + width as usize
}

/// A glyph draw's kind: the cell's own coordinates and scale, a part of it stretched, or nothing drawn (the texture
/// is still set and the pen still advances).
pub const GE_GLYPH_FULL: u32 = 0;
pub const GE_GLYPH_CLIPPED: u32 = 1;
pub const GE_GLYPH_SKIPPED: u32 = 2;

/// One glyph of `GEFontBitmap::Draw`: the cell (an index into `mCharacters`), where the bitmap goes (`location`, the
/// destination's top left), and for a clipped glyph the destination and source rectangles of
/// `ComputeStretchedCoordsAndScale(destination, destination, source)`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct GeGlyphDraw {
    pub kind: u32,
    pub cell: u32,
    pub location: Ge2DPoint,
    pub destination: GeRectangle,
    pub source: GeRectangle,
}

/// `GEFontBitmap::Draw`'s walk over the text: each character's cell is its Windows-1252 byte less the font's first
/// character; a character outside the code page or the font is skipped (no draw, no advance). Unclipped (`clip`
/// `None`) every glyph is drawn whole at the pen. Clipped, a glyph whose cell rectangle (inclusive: width and height
/// less one) lies inside `clip` is drawn whole, one that meets it is cut to it, one fully outside is not drawn.
/// The pen advances by the cell's width either way. `cells` gives each cell's rectangle in its texture.
pub fn draw_plan(
    text: &[u8],
    first_character: c_ulong,
    cells: &[GeRectangle],
    location: Ge2DPoint,
    clip: Option<&GeRectangle>,
    ops: &impl TextOps,
    out: &mut Vec<GeGlyphDraw>,
) {
    let mut pen = location;
    let mut at = 0usize;
    while at < text.len() && text[at] != 0 {
        let (character, length) = ops.decode(&text[at..]);
        at += length.max(1);
        let index = i64::from(ops.cp1252_byte_of(character)) - first_character as i64;
        if index < 0 || index >= cells.len() as i64 {
            continue;
        }
        let cell = &cells[index as usize];
        let width = cell.right - cell.left;
        let mut glyph =
            GeGlyphDraw { kind: GE_GLYPH_FULL, cell: index as u32, location: pen, ..GeGlyphDraw::default() };
        if let Some(r) = clip {
            let height = cell.bottom - cell.top;
            // GERectangle(point, width, height): inclusive
            let mut destination =
                GeRectangle { bottom: pen.y + (height - 1.0), left: pen.x, right: pen.x + (width - 1.0), top: pen.y };
            let d = destination;
            if d.left >= r.left && d.right <= r.right && d.top >= r.top && d.bottom <= r.bottom {
                // whole
            } else if d.left <= r.right && d.right >= r.left && d.top <= r.bottom && d.bottom >= r.top {
                let mut source = *cell;
                if r.top > destination.top {
                    source.top = source.top + (r.top - destination.top);
                    destination.top = r.top;
                }
                if r.left > destination.left {
                    source.left = source.left + (r.left - destination.left);
                    destination.left = r.left;
                }
                if r.right < destination.right {
                    source.right = source.right - (destination.right - r.right);
                    destination.right = r.right;
                }
                if r.bottom < destination.bottom {
                    source.bottom = source.bottom - (destination.bottom - r.bottom);
                    destination.bottom = r.bottom;
                }
                glyph.kind = GE_GLYPH_CLIPPED;
                glyph.source = source;
            } else {
                glyph.kind = GE_GLYPH_SKIPPED;
            }
            glyph.destination = destination;
            glyph.location = Ge2DPoint { x: destination.left, y: destination.top };
        }
        out.push(glyph);
        pen.x = pen.x + width;
    }
}

#[cfg(test)]
pub(crate) mod tests {
    use super::*;
    use ee_plat_core::string::{PlatCp1252ByteOf, PlatMbsNInc, PlatUtf8Decode};

    /// The platform's own functions (the ones the game passes in).
    pub(crate) struct Plat;
    impl TextOps for Plat {
        fn decode(&self, text: &[u8]) -> (u32, usize) {
            let mut n = 0u32;
            // SAFETY: the slice holds its terminator
            let c = unsafe { PlatUtf8Decode(text.as_ptr().cast(), &mut n) };
            (c, n as usize)
        }
        fn next_chars(&self, text: &[u8], characters: usize) -> usize {
            // SAFETY: the slice holds its terminator
            unsafe { PlatMbsNInc(text.as_ptr(), characters).offset_from(text.as_ptr()) as usize }
        }
        fn cp1252_byte_of(&self, character: u32) -> c_int {
            PlatCp1252ByteOf(character)
        }
    }

    fn z(s: &str) -> Vec<u8> {
        let mut v = s.as_bytes().to_vec();
        v.push(0);
        v
    }

    fn rect(l: f32, t: f32, r: f32, b: f32) -> GeRectangle {
        GeRectangle { bottom: b, left: l, right: r, top: t }
    }

    #[test]
    fn a_short_text_is_measured_once_and_the_height_is_the_cell() {
        let mut calls = Vec::new();
        let e = text_extent(&z("Héllo"), 5, 14, &Plat, |s| {
            calls.push(s.to_vec());
            7 * s.len() as i32
        });
        assert_eq!(calls, vec!["Héllo".as_bytes().to_vec()]);
        assert_eq!(e, rect(0.0, 0.0, 42.0, 14.0));
        let e = text_extent(&z(""), 0, 9, &Plat, |s| s.len() as i32);
        assert_eq!(e, rect(0.0, 0.0, 0.0, 9.0));
    }

    #[test]
    fn a_long_text_is_measured_in_chunks_of_256_characters_not_bytes() {
        let text: String = "é".repeat(300);
        let mut calls = Vec::new();
        let e = text_extent(&z(&text), 300, 10, &Plat, |s| {
            calls.push(s.len());
            1
        });
        assert_eq!(calls, vec![512, 88]);
        assert_eq!(e.right, 2.0);
        // exactly 256 characters: one chunk and an empty rest
        let text: String = "a".repeat(256);
        let mut calls = Vec::new();
        text_extent(&z(&text), 256, 10, &Plat, |s| {
            calls.push(s.len());
            0
        });
        assert_eq!(calls, vec![256, 0]);
    }

    #[test]
    fn cells_fill_rows_then_textures() {
        let mut p = GeFontPacker::default();
        let (r, new) = p.place(10.0, 12.0);
        assert!(new);
        assert_eq!(r, rect(0.0, 0.0, 10.0, 12.0));
        let (r, new) = p.place(245.0, 12.0);
        assert!(!new);
        assert_eq!(r, rect(10.0, 0.0, 255.0, 12.0), "a cell may end one pixel short of the edge");
        // one that would end exactly at the edge starts a row
        let (r, new) = p.place(1.0, 12.0);
        assert!(!new);
        assert_eq!(r, rect(0.0, 12.0, 1.0, 24.0));
        // a row that would reach the bottom starts a texture; the row height carries over
        let mut p = GeFontPacker { x: 250.0, y: 230.0, row_height: 20, has_texture: true };
        let (r, new) = p.place(10.0, 6.0);
        assert!(new);
        assert_eq!(r, rect(0.0, 0.0, 10.0, 6.0));
        assert_eq!(p.row_height, 0, "the new row reset it before the texture changed");
        let mut p = GeFontPacker { x: 0.0, y: 240.0, row_height: 30, has_texture: true };
        let (_, new) = p.place(10.0, 16.0);
        assert!(new);
        assert_eq!(p.row_height, 30, "a new texture keeps the row height");
        // a fractional height is truncated into the row height
        let mut p = GeFontPacker::default();
        p.place(1.0, 12.75);
        assert_eq!(p.row_height, 12);
    }

    #[test]
    fn the_alpha_pass_whitens_by_green_and_skips_the_padding() {
        // 3 x 2 pixels, rows 8 bytes apart (one unit of padding)
        let mut px = [0x0F00u16, 0x00F0, 0x1234, 0xBEEF, 0x0000, 0xFFFF, 0x0010];
        let n = surface_alpha_len(3, 2, 8);
        assert_eq!(n, 7);
        surface_alpha(&mut px[..n], 3, 2, 8);
        assert_eq!(px, [0x0000, 0xFFFF, 0x3FFF, 0xBEEF, 0x0000, 0xFFFF, 0x1FFF]);
        assert_eq!(surface_alpha_len(0, 5, 0), 0);
        assert_eq!(surface_alpha_len(4, 0, 8), 0);
    }

    fn cells() -> Vec<GeRectangle> {
        // cells of the font's characters 0x20.. : 'A' = 0x41 at index 0x21, the euro sign (1252 0x80) at 0x60
        (0x20..=0xFFu32).map(|c| rect(0.0, 0.0, (c % 7 + 3) as f32, 10.0)).collect()
    }

    #[test]
    fn glyphs_walk_by_character_and_skip_what_the_font_lacks() {
        let cells = cells();
        let mut out = Vec::new();
        // 'A', U+00E9 (1252 0xE9), U+20AC (1252 0x80), U+03A9 (none), a stray byte, '\n' (below the font), 'B'
        let mut text = "Aé€Ω".as_bytes().to_vec();
        text.extend_from_slice(&[0xFF, b'\n', b'B', 0]);
        draw_plan(&text, 0x20, &cells, Ge2DPoint { x: 5.0, y: 7.0 }, None, &Plat, &mut out);
        let got: Vec<(u32, f32)> = out.iter().map(|g| (g.cell, g.location.x)).collect();
        let w = |c: u32| (c % 7 + 3) as f32;
        assert_eq!(
            got,
            vec![
                (0x21, 5.0),
                (0xE9 - 0x20, 5.0 + w(0x41)),
                (0x60, 5.0 + w(0x41) + w(0xE9)),
                (0x22, 5.0 + w(0x41) + w(0xE9) + w(0x80))
            ]
        );
        assert!(out.iter().all(|g| g.kind == GE_GLYPH_FULL && g.location.y == 7.0));
        out.clear();
        draw_plan(&z(""), 0x20, &cells, Ge2DPoint::default(), None, &Plat, &mut out);
        assert!(out.is_empty());
    }

    #[test]
    fn a_clip_keeps_inside_glyphs_cuts_edge_ones_and_skips_outside_ones() {
        let cells = vec![rect(10.0, 20.0, 18.0, 30.0)]; // 8 x 10, the font's only character 'A'
        let mut out = Vec::new();
        // pen at 0, 8, 16, 24; clip x 0..=15 (inclusive edges)
        let clip = rect(0.0, 0.0, 15.0, 9.0);
        draw_plan(&z("AAAA"), 0x41, &cells, Ge2DPoint::default(), Some(&clip), &Plat, &mut out);
        assert_eq!(out.len(), 4);
        assert_eq!(out[0].kind, GE_GLYPH_FULL, "x 0..=7, y 0..=9: inside");
        assert_eq!(out[1].kind, GE_GLYPH_FULL, "x 8..=15: exactly at the edge, inside");
        assert_eq!(out[2].kind, GE_GLYPH_SKIPPED, "x 16..=23: outside");
        assert_eq!(out[3].kind, GE_GLYPH_SKIPPED);
        let clip = rect(3.0, 2.0, 12.0, 5.0);
        out.clear();
        draw_plan(&z("AA"), 0x41, &cells, Ge2DPoint::default(), Some(&clip), &Plat, &mut out);
        assert_eq!(out[0].kind, GE_GLYPH_CLIPPED);
        assert_eq!(out[0].destination, rect(3.0, 2.0, 7.0, 5.0));
        assert_eq!(out[0].source, rect(13.0, 22.0, 18.0, 26.0));
        assert_eq!(out[0].location, Ge2DPoint { x: 3.0, y: 2.0 });
        assert_eq!(out[1].kind, GE_GLYPH_CLIPPED);
        assert_eq!(out[1].destination, rect(8.0, 2.0, 12.0, 5.0));
        assert_eq!(out[1].source, rect(10.0, 22.0, 15.0, 26.0));
    }
}
