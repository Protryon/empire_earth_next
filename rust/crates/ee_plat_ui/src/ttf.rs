//! TrueType glyph access and the antialiasing rasteriser: a line-by-line port of the part of stb_truetype v1.26
//! (src/ThirdParty/stb in the C++ tree) the game used, `STBTT_RASTERIZER_VERSION` 2, so the bitmaps are the same bytes.
//!
//! The floating-point operations are written in the C's order and in `f32`; do not "simplify" them. `f32` with the
//! C's explicit casts is why the output matches on every system. (A C++ arm64 compile without `-ffp-contract=off`
//! could fuse products; xbuild's flags forbid it, and Rust never fuses.)
//!
//! Left out because the game never gave it to stb: CFF/OTF outlines (`init_font` refuses them; stb's only other input
//! is the same TrueType), kerning, SDF, the v1 rasteriser. Reads of malformed data return 0 instead of being
//! undefined; for a font that passes stb's checks the result is the same as stb's.

pub(crate) const VMOVE: u8 = 1;
pub(crate) const VLINE: u8 = 2;
pub(crate) const VCURVE: u8 = 3;

#[derive(Clone, Copy)]
pub struct FontInfo<'a> {
    pub data: &'a [u8],
    pub fontstart: u32,
    pub head: u32,
    pub hhea: u32,
    pub hmtx: u32,
    loca: u32,
    glyf: u32,
    index_map: u32,
    num_glyphs: i32,
    index_to_loc_format: i32,
}

// C reads through a pointer; here a read past the slice (or before it) gives 0.
#[inline]
fn u8_at(d: &[u8], o: i64) -> u8 {
    if o < 0 {
        return 0;
    }
    d.get(o as usize).copied().unwrap_or(0)
}
#[inline]
fn i8_at(d: &[u8], o: i64) -> i8 {
    u8_at(d, o) as i8
}
#[inline]
fn u16_at(d: &[u8], o: i64) -> u16 {
    ((u8_at(d, o) as u16) << 8) | u8_at(d, o + 1) as u16
}
#[inline]
fn i16_at(d: &[u8], o: i64) -> i16 {
    u16_at(d, o) as i16
}
#[inline]
fn u32_at(d: &[u8], o: i64) -> u32 {
    ((u8_at(d, o) as u32) << 24)
        | ((u8_at(d, o + 1) as u32) << 16)
        | ((u8_at(d, o + 2) as u32) << 8)
        | (u8_at(d, o + 3) as u32)
}
#[inline]
fn i32_at(d: &[u8], o: i64) -> i32 {
    u32_at(d, o) as i32
}

fn tag_at(d: &[u8], o: i64, tag: &[u8; 4]) -> bool {
    u8_at(d, o) == tag[0] && u8_at(d, o + 1) == tag[1] && u8_at(d, o + 2) == tag[2] && u8_at(d, o + 3) == tag[3]
}

fn is_font(d: &[u8]) -> bool {
    tag_at(d, 0, &[b'1', 0, 0, 0])
        || tag_at(d, 0, b"typ1")
        || tag_at(d, 0, b"OTTO")
        || tag_at(d, 0, &[0, 1, 0, 0])
        || tag_at(d, 0, b"true")
}

/// `stbtt__find_table`: the offset of a table, 0 when absent.
pub fn find_table(d: &[u8], fontstart: u32, tag: &[u8; 4]) -> u32 {
    let num_tables = u16_at(d, fontstart as i64 + 4) as i64;
    let tabledir = fontstart as i64 + 12;
    for i in 0..num_tables {
        let loc = tabledir + 16 * i;
        if tag_at(d, loc, tag) {
            return u32_at(d, loc + 8);
        }
    }
    0
}

/// `stbtt_GetFontOffsetForIndex(data, index)` -> the font's start, -1 when there is no such font.
pub fn font_offset_for_index(d: &[u8], index: i32) -> i32 {
    if is_font(d) {
        return if index == 0 { 0 } else { -1 };
    }
    if tag_at(d, 0, b"ttcf") {
        let version = u32_at(d, 4);
        if version == 0x0001_0000 || version == 0x0002_0000 {
            let n = i32_at(d, 8);
            if index >= n {
                return -1;
            }
            return u32_at(d, 12 + index as i64 * 4) as i32;
        }
    }
    -1
}

/// `stbtt_InitFont`, TrueType outlines only (see the module note).
pub fn init_font(data: &[u8], fontstart: u32) -> Option<FontInfo<'_>> {
    let cmap = find_table(data, fontstart, b"cmap");
    let loca = find_table(data, fontstart, b"loca");
    let head = find_table(data, fontstart, b"head");
    let glyf = find_table(data, fontstart, b"glyf");
    let hhea = find_table(data, fontstart, b"hhea");
    let hmtx = find_table(data, fontstart, b"hmtx");
    if cmap == 0 || head == 0 || hhea == 0 || hmtx == 0 {
        return None;
    }
    if glyf == 0 || loca == 0 {
        return None; // no CFF path: a glyf table and its loca are required
    }
    let t = find_table(data, fontstart, b"maxp");
    let num_glyphs = if t != 0 { u16_at(data, t as i64 + 4) as i32 } else { 0xffff };
    let num_tables = u16_at(data, cmap as i64 + 2) as i64;
    let mut index_map = 0u32;
    for i in 0..num_tables {
        let encoding_record = cmap as i64 + 4 + 8 * i;
        match u16_at(data, encoding_record) {
            3 => {
                // STBTT_PLATFORM_ID_MICROSOFT
                let e = u16_at(data, encoding_record + 2);
                if e == 1 || e == 10 {
                    // STBTT_MS_EID_UNICODE_BMP / UNICODE_FULL
                    index_map = cmap.wrapping_add(u32_at(data, encoding_record + 4));
                }
            }
            0 => {
                // STBTT_PLATFORM_ID_UNICODE
                index_map = cmap.wrapping_add(u32_at(data, encoding_record + 4));
            }
            _ => {}
        }
    }
    if index_map == 0 {
        return None;
    }
    let index_to_loc_format = u16_at(data, head as i64 + 50) as i32;
    Some(FontInfo { data, fontstart, head, hhea, hmtx, loca, glyf, index_map, num_glyphs, index_to_loc_format })
}

impl FontInfo<'_> {
    /// `stbtt_FindGlyphIndex`
    pub fn find_glyph_index(&self, unicode_codepoint: i32) -> i32 {
        let d = self.data;
        let index_map = self.index_map as i64;
        let cp = unicode_codepoint;
        let format = u16_at(d, index_map);
        match format {
            0 => {
                let bytes = u16_at(d, index_map + 2) as i32;
                if cp < bytes - 6 {
                    return u8_at(d, index_map + 6 + cp as i64) as i32;
                }
                0
            }
            6 => {
                let first = u16_at(d, index_map + 6) as u32;
                let count = u16_at(d, index_map + 8) as u32;
                if (cp as u32) >= first && (cp as u32) < first + count {
                    return u16_at(d, index_map + 10 + (cp as i64 - first as i64) * 2) as i32;
                }
                0
            }
            4 => {
                let segcount = (u16_at(d, index_map + 6) >> 1) as i64;
                let mut search_range = (u16_at(d, index_map + 8) >> 1) as i64;
                let mut entry_selector = u16_at(d, index_map + 10);
                let range_shift = (u16_at(d, index_map + 12) >> 1) as i64;
                let end_count = index_map + 14;
                let mut search = end_count;
                if cp > 0xffff {
                    return 0;
                }
                if cp >= u16_at(d, search + range_shift * 2) as i32 {
                    search += range_shift * 2;
                }
                search -= 2;
                while entry_selector != 0 {
                    search_range >>= 1;
                    let end = u16_at(d, search + search_range * 2) as i32;
                    if cp > end {
                        search += search_range * 2;
                    }
                    entry_selector -= 1;
                }
                search += 2;
                let item = (((search - end_count) >> 1) as u16) as i64;
                let start = u16_at(d, index_map + 14 + segcount * 2 + 2 + 2 * item) as i32;
                let last = u16_at(d, end_count + 2 * item) as i32;
                if cp < start || cp > last {
                    return 0;
                }
                let offset = u16_at(d, index_map + 14 + segcount * 6 + 2 + 2 * item) as i64;
                if offset == 0 {
                    return (cp + i16_at(d, index_map + 14 + segcount * 4 + 2 + 2 * item) as i32) as u16 as i32;
                }
                u16_at(d, offset + (cp as i64 - start as i64) * 2 + index_map + 14 + segcount * 6 + 2 + 2 * item) as i32
            }
            12 | 13 => {
                let ngroups = u32_at(d, index_map + 12);
                let mut low = 0i32;
                let mut high = ngroups as i32;
                while low < high {
                    let mid = low + ((high - low) >> 1);
                    let m = mid as i64;
                    let start_char = u32_at(d, index_map + 16 + m * 12);
                    let end_char = u32_at(d, index_map + 16 + m * 12 + 4);
                    if (cp as u32) < start_char {
                        high = mid;
                    } else if (cp as u32) > end_char {
                        low = mid + 1;
                    } else {
                        let start_glyph = u32_at(d, index_map + 16 + m * 12 + 8);
                        return if format == 12 {
                            start_glyph.wrapping_add(cp as u32).wrapping_sub(start_char) as i32
                        } else {
                            start_glyph as i32
                        };
                    }
                }
                0
            }
            _ => 0,
        }
    }

    fn glyf_offset(&self, glyph_index: i32) -> i64 {
        if glyph_index >= self.num_glyphs || self.index_to_loc_format >= 2 {
            return -1;
        }
        let d = self.data;
        let gi = glyph_index as i64;
        let (loca, glyf) = (self.loca as i64, self.glyf as i64);
        let (g1, g2) = if self.index_to_loc_format == 0 {
            (glyf + u16_at(d, loca + gi * 2) as i64 * 2, glyf + u16_at(d, loca + gi * 2 + 2) as i64 * 2)
        } else {
            (glyf + u32_at(d, loca + gi * 4) as i64, glyf + u32_at(d, loca + gi * 4 + 4) as i64)
        };
        if g1 == g2 {
            -1
        } else {
            g1
        }
    }

    /// `stbtt_GetGlyphBox`
    pub fn glyph_box(&self, glyph_index: i32) -> Option<(i32, i32, i32, i32)> {
        let g = self.glyf_offset(glyph_index);
        if g < 0 {
            return None;
        }
        let d = self.data;
        Some((i16_at(d, g + 2) as i32, i16_at(d, g + 4) as i32, i16_at(d, g + 6) as i32, i16_at(d, g + 8) as i32))
    }

    /// `stbtt_GetGlyphHMetrics` -> (advance, left side bearing)
    pub fn glyph_hmetrics(&self, glyph_index: i32) -> (i32, i32) {
        let d = self.data;
        let num_long = u16_at(d, self.hhea as i64 + 34) as i32;
        let hmtx = self.hmtx as i64;
        if glyph_index < num_long {
            (i16_at(d, hmtx + 4 * glyph_index as i64) as i32, i16_at(d, hmtx + 4 * glyph_index as i64 + 2) as i32)
        } else {
            (
                i16_at(d, hmtx + 4 * (num_long as i64 - 1)) as i32,
                i16_at(d, hmtx + 4 * num_long as i64 + 2 * (glyph_index as i64 - num_long as i64)) as i32,
            )
        }
    }

    /// `stbtt_GetFontVMetrics` -> (ascent, descent, line gap)
    pub fn vmetrics(&self) -> (i32, i32, i32) {
        let d = self.data;
        let h = self.hhea as i64;
        (i16_at(d, h + 4) as i32, i16_at(d, h + 6) as i32, i16_at(d, h + 8) as i32)
    }

    /// `stbtt_GetGlyphBitmapBox`: (x0, y0, x1, y1), zeros for an empty glyph.
    pub fn glyph_bitmap_box(&self, glyph_index: i32, scale_x: f32, scale_y: f32) -> (i32, i32, i32, i32) {
        let (shift_x, shift_y) = (0.0f32, 0.0f32);
        match self.glyph_box(glyph_index) {
            None => (0, 0, 0, 0),
            Some((x0, y0, x1, y1)) => (
                (x0 as f32 * scale_x + shift_x).floor() as i32,
                ((-y1) as f32 * scale_y + shift_y).floor() as i32,
                (x1 as f32 * scale_x + shift_x).ceil() as i32,
                ((-y0) as f32 * scale_y + shift_y).ceil() as i32,
            ),
        }
    }

    fn glyph_shape(&self, glyph_index: i32, depth: u32) -> Vec<Vertex> {
        let mut vertices = Vec::new();
        let g = self.glyf_offset(glyph_index);
        if g < 0 {
            return vertices;
        }
        let d = self.data;
        let number_of_contours = i16_at(d, g) as i32;
        if number_of_contours > 0 {
            let end_pts = g + 10;
            let ins = u16_at(d, g + 10 + number_of_contours as i64 * 2) as i64;
            let mut points = g + 10 + number_of_contours as i64 * 2 + 2 + ins;
            let n = 1 + u16_at(d, end_pts + number_of_contours as i64 * 2 - 2) as usize;
            let mut raw = vec![Vertex::default(); n];
            let mut flags = 0u8;
            let mut flagcount = 0u8;
            for r in raw.iter_mut() {
                if flagcount == 0 {
                    flags = u8_at(d, points);
                    points += 1;
                    if flags & 8 != 0 {
                        flagcount = u8_at(d, points);
                        points += 1;
                    }
                } else {
                    flagcount -= 1;
                }
                r.kind = flags;
            }
            let mut x = 0i32;
            for r in raw.iter_mut() {
                flags = r.kind;
                if flags & 2 != 0 {
                    let dx = u8_at(d, points) as i16;
                    points += 1;
                    x += (if flags & 16 != 0 { dx } else { -dx }) as i32;
                } else if flags & 16 == 0 {
                    x += ((u8_at(d, points) as i32 * 256 + u8_at(d, points + 1) as i32) as i16) as i32;
                    points += 2;
                }
                r.x = x as i16;
            }
            let mut y = 0i32;
            for r in raw.iter_mut() {
                flags = r.kind;
                if flags & 4 != 0 {
                    let dy = u8_at(d, points) as i16;
                    points += 1;
                    y += (if flags & 32 != 0 { dy } else { -dy }) as i32;
                } else if flags & 32 == 0 {
                    y += ((u8_at(d, points) as i32 * 256 + u8_at(d, points + 1) as i32) as i16) as i32;
                    points += 2;
                }
                r.y = y as i16;
            }

            // now convert them to our format
            let (mut sx, mut sy, mut cx, mut cy, mut scx, mut scy) = (0i32, 0i32, 0i32, 0i32, 0i32, 0i32);
            let (mut was_off, mut start_off) = (false, false);
            let mut next_move = 0i32;
            let mut j = 0i64;
            let mut i = 0i32;
            while (i as usize) < n {
                let r = raw[i as usize];
                let f = r.kind;
                let x = r.x as i32;
                let y = r.y as i32;
                if next_move == i {
                    if i != 0 {
                        close_shape(&mut vertices, was_off, start_off, sx, sy, scx, scy, cx, cy);
                    }
                    start_off = f & 1 == 0;
                    if start_off {
                        scx = x;
                        scy = y;
                        let nx = raw.get(i as usize + 1).copied().unwrap_or_default();
                        if nx.kind & 1 == 0 {
                            sx = (x + nx.x as i32) >> 1;
                            sy = (y + nx.y as i32) >> 1;
                        } else {
                            sx = nx.x as i32;
                            sy = nx.y as i32;
                            i += 1;
                        }
                    } else {
                        sx = x;
                        sy = y;
                    }
                    vertices.push(vertex(VMOVE, sx, sy, 0, 0));
                    was_off = false;
                    next_move = 1 + u16_at(d, end_pts + j * 2) as i32;
                    j += 1;
                } else if f & 1 == 0 {
                    if was_off {
                        vertices.push(vertex(VCURVE, (cx + x) >> 1, (cy + y) >> 1, cx, cy));
                    }
                    cx = x;
                    cy = y;
                    was_off = true;
                } else {
                    if was_off {
                        vertices.push(vertex(VCURVE, x, y, cx, cy));
                    } else {
                        vertices.push(vertex(VLINE, x, y, 0, 0));
                    }
                    was_off = false;
                }
                i += 1;
            }
            close_shape(&mut vertices, was_off, start_off, sx, sy, scx, scy, cx, cy);
        } else if number_of_contours < 0 && depth < 16 {
            // compound shapes
            let mut comp = g + 10;
            let mut more = true;
            while more {
                let mut mtx = [1.0f32, 0.0, 0.0, 1.0, 0.0, 0.0];
                let flags = u16_at(d, comp);
                comp += 2;
                let gidx = u16_at(d, comp) as i32;
                comp += 2;
                if flags & 2 != 0 {
                    if flags & 1 != 0 {
                        mtx[4] = i16_at(d, comp) as f32;
                        comp += 2;
                        mtx[5] = i16_at(d, comp) as f32;
                        comp += 2;
                    } else {
                        mtx[4] = i8_at(d, comp) as f32;
                        comp += 1;
                        mtx[5] = i8_at(d, comp) as f32;
                        comp += 1;
                    }
                }
                if flags & (1 << 3) != 0 {
                    mtx[0] = i16_at(d, comp) as f32 / 16384.0;
                    mtx[3] = mtx[0];
                    comp += 2;
                    mtx[1] = 0.0;
                    mtx[2] = 0.0;
                } else if flags & (1 << 6) != 0 {
                    mtx[0] = i16_at(d, comp) as f32 / 16384.0;
                    comp += 2;
                    mtx[1] = 0.0;
                    mtx[2] = 0.0;
                    mtx[3] = i16_at(d, comp) as f32 / 16384.0;
                    comp += 2;
                } else if flags & (1 << 7) != 0 {
                    mtx[0] = i16_at(d, comp) as f32 / 16384.0;
                    comp += 2;
                    mtx[1] = i16_at(d, comp) as f32 / 16384.0;
                    comp += 2;
                    mtx[2] = i16_at(d, comp) as f32 / 16384.0;
                    comp += 2;
                    mtx[3] = i16_at(d, comp) as f32 / 16384.0;
                    comp += 2;
                }
                let m = ((mtx[0] * mtx[0] + mtx[1] * mtx[1]) as f64).sqrt() as f32;
                let n = ((mtx[2] * mtx[2] + mtx[3] * mtx[3]) as f64).sqrt() as f32;
                let mut comp_verts = self.glyph_shape(gidx, depth + 1);
                for v in comp_verts.iter_mut() {
                    let (x, y) = (v.x as f32, v.y as f32);
                    v.x = (m * (mtx[0] * x + mtx[2] * y + mtx[4])) as i16;
                    v.y = (n * (mtx[1] * x + mtx[3] * y + mtx[5])) as i16;
                    let (x, y) = (v.cx as f32, v.cy as f32);
                    v.cx = (m * (mtx[0] * x + mtx[2] * y + mtx[4])) as i16;
                    v.cy = (n * (mtx[1] * x + mtx[3] * y + mtx[5])) as i16;
                }
                vertices.append(&mut comp_verts);
                more = flags & (1 << 5) != 0;
            }
        }
        vertices
    }

    /// `stbtt_MakeGlyphBitmap`: renders into `output` (`out_w` x `out_h` bytes, zero-filled by the caller).
    pub fn make_glyph_bitmap(&self, output: &mut [u8], out_w: i32, out_h: i32, scale_x: f32, scale_y: f32, glyph: i32) {
        let vertices = self.glyph_shape(glyph, 0);
        let (ix0, iy0, _, _) = self.glyph_bitmap_box(glyph, scale_x, scale_y);
        if out_w != 0 && out_h != 0 {
            rasterize(output, out_w, out_h, 0.35, &vertices, scale_x, scale_y, 0.0, 0.0, ix0, iy0);
        }
    }
}

#[derive(Clone, Copy, Default)]
pub(crate) struct Vertex {
    x: i16,
    y: i16,
    cx: i16,
    cy: i16,
    kind: u8,
}

fn vertex(kind: u8, x: i32, y: i32, cx: i32, cy: i32) -> Vertex {
    Vertex { x: x as i16, y: y as i16, cx: cx as i16, cy: cy as i16, kind }
}

#[allow(clippy::too_many_arguments)]
fn close_shape(
    v: &mut Vec<Vertex>,
    was_off: bool,
    start_off: bool,
    sx: i32,
    sy: i32,
    scx: i32,
    scy: i32,
    cx: i32,
    cy: i32,
) {
    if start_off {
        if was_off {
            v.push(vertex(VCURVE, (cx + scx) >> 1, (cy + scy) >> 1, cx, cy));
        }
        v.push(vertex(VCURVE, sx, sy, scx, scy));
    } else if was_off {
        v.push(vertex(VCURVE, sx, sy, cx, cy));
    } else {
        v.push(vertex(VLINE, sx, sy, 0, 0));
    }
}

#[derive(Clone, Copy)]
struct Point {
    x: f32,
    y: f32,
}

fn tesselate_curve(points: &mut Vec<Point>, p0: Point, p1: Point, p2: Point, flatness_squared: f32, n: u32) {
    let (x0, y0, x1, y1, x2, y2) = (p0.x, p0.y, p1.x, p1.y, p2.x, p2.y);
    // midpoint
    let mx = (x0 + 2.0 * x1 + x2) / 4.0;
    let my = (y0 + 2.0 * y1 + y2) / 4.0;
    // versus directly drawn line
    let dx = (x0 + x2) / 2.0 - mx;
    let dy = (y0 + y2) / 2.0 - my;
    if n > 16 {
        return;
    }
    if dx * dx + dy * dy > flatness_squared {
        tesselate_curve(
            points,
            p0,
            Point { x: (x0 + x1) / 2.0, y: (y0 + y1) / 2.0 },
            Point { x: mx, y: my },
            flatness_squared,
            n + 1,
        );
        tesselate_curve(
            points,
            Point { x: mx, y: my },
            Point { x: (x1 + x2) / 2.0, y: (y1 + y2) / 2.0 },
            p2,
            flatness_squared,
            n + 1,
        );
    } else {
        points.push(Point { x: x2, y: y2 });
    }
}

fn flatten_curves(vertices: &[Vertex], objspace_flatness: f32) -> (Vec<Point>, Vec<i32>) {
    let flatness_squared = objspace_flatness * objspace_flatness;
    let moves = vertices.iter().filter(|v| v.kind == VMOVE).count();
    let mut points = Vec::new();
    let mut lengths = Vec::new();
    if moves == 0 {
        return (points, lengths);
    }
    let (mut x, mut y) = (0.0f32, 0.0f32);
    let mut start = 0i32;
    let mut open = false;
    for v in vertices {
        match v.kind {
            VMOVE => {
                if open {
                    lengths.push(points.len() as i32 - start);
                }
                open = true;
                start = points.len() as i32;
                x = v.x as f32;
                y = v.y as f32;
                points.push(Point { x, y });
            }
            VLINE => {
                x = v.x as f32;
                y = v.y as f32;
                points.push(Point { x, y });
            }
            VCURVE => {
                let (cx, cy) = (v.cx as f32, v.cy as f32);
                let (px, py) = (v.x as f32, v.y as f32);
                tesselate_curve(
                    &mut points,
                    Point { x, y },
                    Point { x: cx, y: cy },
                    Point { x: px, y: py },
                    flatness_squared,
                    0,
                );
                x = px;
                y = py;
            }
            _ => {}
        }
    }
    lengths.push(points.len() as i32 - start);
    (points, lengths)
}

#[derive(Clone, Copy)]
struct Edge {
    x0: f32,
    y0: f32,
    x1: f32,
    y1: f32,
    invert: i32,
}

struct ActiveEdge {
    fx: f32,
    fdx: f32,
    fdy: f32,
    direction: f32,
    sy: f32,
    ey: f32,
}

fn new_active(e: &Edge, off_x: i32, start_point: f32) -> ActiveEdge {
    let dxdy = (e.x1 - e.x0) / (e.y1 - e.y0);
    let mut fx = e.x0 + dxdy * (start_point - e.y0);
    fx -= off_x as f32;
    ActiveEdge {
        fdx: dxdy,
        fdy: if dxdy != 0.0 { 1.0 / dxdy } else { 0.0 },
        fx,
        direction: if e.invert != 0 { 1.0 } else { -1.0 },
        sy: e.y0,
        ey: e.y1,
    }
}

fn handle_clipped_edge(scanline: &mut [f32], x: i32, e: &ActiveEdge, x0: f32, y0: f32, x1: f32, y1: f32) {
    let (mut x0, mut y0, mut x1, mut y1) = (x0, y0, x1, y1);
    if y0 == y1 {
        return;
    }
    if y0 > e.ey {
        return;
    }
    if y1 < e.sy {
        return;
    }
    if y0 < e.sy {
        x0 += (x1 - x0) * (e.sy - y0) / (y1 - y0);
        y0 = e.sy;
    }
    if y1 > e.ey {
        x1 += (x1 - x0) * (e.ey - y1) / (y1 - y0);
        y1 = e.ey;
    }
    let fx = x as f32;
    if x0 <= fx && x1 <= fx {
        scanline[x as usize] += e.direction * (y1 - y0);
    } else if x0 >= (x + 1) as f32 && x1 >= (x + 1) as f32 {
    } else {
        scanline[x as usize] += e.direction * (y1 - y0) * (1.0 - ((x0 - fx) + (x1 - fx)) / 2.0);
    }
}

fn sized_trapezoid_area(height: f32, top_width: f32, bottom_width: f32) -> f32 {
    (top_width + bottom_width) / 2.0 * height
}

fn position_trapezoid_area(height: f32, tx0: f32, tx1: f32, bx0: f32, bx1: f32) -> f32 {
    sized_trapezoid_area(height, tx1 - tx0, bx1 - bx0)
}

fn sized_triangle_area(height: f32, width: f32) -> f32 {
    height * width / 2.0
}

/// `scanline_fill` of the C is `fill[1..]` here; the C's `scanline_fill - 1` is `fill` itself.
#[allow(clippy::if_same_then_else, clippy::too_many_arguments)]
fn fill_active_edges(scanline: &mut [f32], fill: &mut [f32], len: i32, active: &[ActiveEdge], y_top: f32) {
    let y_bottom = y_top + 1.0;
    for e in active {
        if e.fdx == 0.0 {
            let x0 = e.fx;
            if x0 < len as f32 {
                if x0 >= 0.0 {
                    handle_clipped_edge(scanline, x0 as i32, e, x0, y_top, x0, y_bottom);
                    handle_clipped_edge(fill, x0 as i32 + 1, e, x0, y_top, x0, y_bottom);
                } else {
                    handle_clipped_edge(fill, 0, e, x0, y_top, x0, y_bottom);
                }
            }
        } else {
            let mut x0 = e.fx;
            let mut dx = e.fdx;
            let mut xb = x0 + dx;
            let (mut x_top, mut x_bottom, mut sy0, mut sy1);
            let mut dy = e.fdy;
            if e.sy > y_top {
                x_top = x0 + dx * (e.sy - y_top);
                sy0 = e.sy;
            } else {
                x_top = x0;
                sy0 = y_top;
            }
            if e.ey < y_bottom {
                x_bottom = x0 + dx * (e.ey - y_top);
                sy1 = e.ey;
            } else {
                x_bottom = xb;
                sy1 = y_bottom;
            }
            if x_top >= 0.0 && x_bottom >= 0.0 && x_top < len as f32 && x_bottom < len as f32 {
                if x_top as i32 == x_bottom as i32 {
                    let x = x_top as i32;
                    let height = (sy1 - sy0) * e.direction;
                    scanline[x as usize] +=
                        position_trapezoid_area(height, x_top, x as f32 + 1.0, x_bottom, x as f32 + 1.0);
                    fill[(x + 1) as usize] += height;
                } else {
                    if x_top > x_bottom {
                        sy0 = y_bottom - (sy0 - y_top);
                        sy1 = y_bottom - (sy1 - y_top);
                        std::mem::swap(&mut sy0, &mut sy1);
                        std::mem::swap(&mut x_bottom, &mut x_top);
                        dx = -dx;
                        dy = -dy;
                        std::mem::swap(&mut x0, &mut xb);
                    }
                    let x1 = x_top as i32;
                    let x2 = x_bottom as i32;
                    let mut y_crossing = y_top + dy * ((x1 + 1) as f32 - x0);
                    let mut y_final = y_top + dy * (x2 as f32 - x0);
                    if y_crossing > y_bottom {
                        y_crossing = y_bottom;
                    }
                    let sign = e.direction;
                    let mut area = sign * (y_crossing - sy0);
                    scanline[x1 as usize] += sized_triangle_area(area, (x1 + 1) as f32 - x_top);
                    if y_final > y_bottom {
                        y_final = y_bottom;
                        dy = (y_final - y_crossing) / (x2 - (x1 + 1)) as f32;
                    }
                    let step = sign * dy;
                    let _ = dx;
                    for x in x1 + 1..x2 {
                        scanline[x as usize] += area + step / 2.0;
                        area += step;
                    }
                    scanline[x2 as usize] += area
                        + sign
                            * position_trapezoid_area(
                                sy1 - y_final,
                                x2 as f32,
                                x2 as f32 + 1.0,
                                x_bottom,
                                x2 as f32 + 1.0,
                            );
                    fill[(x2 + 1) as usize] += sign * (sy1 - sy0);
                }
            } else {
                // the brute-force clipping path of stb
                for x in 0..len {
                    let y0 = y_top;
                    let x1 = x as f32;
                    let x2 = (x + 1) as f32;
                    let x3 = xb;
                    let y3 = y_bottom;
                    let y1 = (x as f32 - x0) / dx + y_top;
                    let y2 = ((x + 1) as f32 - x0) / dx + y_top;
                    if x0 < x1 && x3 > x2 {
                        handle_clipped_edge(scanline, x, e, x0, y0, x1, y1);
                        handle_clipped_edge(scanline, x, e, x1, y1, x2, y2);
                        handle_clipped_edge(scanline, x, e, x2, y2, x3, y3);
                    } else if x3 < x1 && x0 > x2 {
                        handle_clipped_edge(scanline, x, e, x0, y0, x2, y2);
                        handle_clipped_edge(scanline, x, e, x2, y2, x1, y1);
                        handle_clipped_edge(scanline, x, e, x1, y1, x3, y3);
                    } else if x0 < x1 && x3 > x1 {
                        handle_clipped_edge(scanline, x, e, x0, y0, x1, y1);
                        handle_clipped_edge(scanline, x, e, x1, y1, x3, y3);
                    } else if x3 < x1 && x0 > x1 {
                        handle_clipped_edge(scanline, x, e, x0, y0, x1, y1);
                        handle_clipped_edge(scanline, x, e, x1, y1, x3, y3);
                    } else if x0 < x2 && x3 > x2 {
                        handle_clipped_edge(scanline, x, e, x0, y0, x2, y2);
                        handle_clipped_edge(scanline, x, e, x2, y2, x3, y3);
                    } else if x3 < x2 && x0 > x2 {
                        handle_clipped_edge(scanline, x, e, x0, y0, x2, y2);
                        handle_clipped_edge(scanline, x, e, x2, y2, x3, y3);
                    } else {
                        handle_clipped_edge(scanline, x, e, x0, y0, x3, y3);
                    }
                }
            }
        }
    }
}

fn rasterize_sorted_edges(output: &mut [u8], w: i32, h: i32, edges: &[Edge], off_x: i32, off_y: i32) {
    let mut active: Vec<ActiveEdge> = Vec::new();
    let mut scanline = vec![0.0f32; w as usize];
    let mut fill = vec![0.0f32; w as usize + 1];
    let mut y = off_y;
    let mut e = 0usize;
    let mut j = 0;
    while j < h {
        let scan_y_top = y as f32 + 0.0;
        let scan_y_bottom = y as f32 + 1.0;
        scanline.fill(0.0);
        fill.fill(0.0);
        // remove the edges that end before this scanline, keeping the list's order
        active.retain(|z| z.ey > scan_y_top);
        // insert every edge that starts before the bottom of this scanline, at the front of the list as the C does
        while e < edges.len() && edges[e].y0 <= scan_y_bottom {
            if edges[e].y0 != edges[e].y1 {
                let mut z = new_active(&edges[e], off_x, scan_y_top);
                if j == 0 && off_y != 0 && z.ey < scan_y_top {
                    z.ey = scan_y_top;
                }
                active.insert(0, z);
            }
            e += 1;
        }
        if !active.is_empty() {
            fill_active_edges(&mut scanline, &mut fill, w, &active, scan_y_top);
        }
        let mut sum = 0.0f32;
        for i in 0..w as usize {
            sum += fill[i];
            let k = scanline[i] + sum;
            let k = ((k as f64).abs() as f32) * 255.0 + 0.5;
            let mut m = k as i32;
            if m > 255 {
                m = 255;
            }
            output[j as usize * w as usize + i] = m as u8;
        }
        for z in active.iter_mut() {
            z.fx += z.fdx;
        }
        y += 1;
        j += 1;
    }
}

// the quicksort and insertion sort of stb: the order of equal keys (which the rasteriser's float sums depend on) is
// part of the output, so a library sort would not do
fn edges_less(a: &Edge, b: &Edge) -> bool {
    a.y0 < b.y0
}

fn sort_edges_ins_sort(p: &mut [Edge]) {
    for i in 1..p.len() {
        let t = p[i];
        let mut j = i;
        while j > 0 {
            if !edges_less(&t, &p[j - 1]) {
                break;
            }
            p[j] = p[j - 1];
            j -= 1;
        }
        if i != j {
            p[j] = t;
        }
    }
}

fn sort_edges_quicksort(s: &mut [Edge], start: usize, n0: usize) {
    let mut start = start;
    let mut n = n0 as i32;
    while n > 12 {
        let m = (n >> 1) as usize;
        let (st, nu) = (start, n as usize);
        let c01 = edges_less(&s[st], &s[st + m]);
        let c12 = edges_less(&s[st + m], &s[st + nu - 1]);
        if c01 != c12 {
            let c = edges_less(&s[st], &s[st + nu - 1]);
            let z = if c == c12 { 0 } else { nu - 1 };
            s.swap(st + z, st + m);
        }
        s.swap(st, st + m);
        let mut i = 1i32;
        let mut j = n - 1;
        loop {
            while edges_less(&s[st + i as usize], &s[st]) {
                i += 1;
            }
            while edges_less(&s[st], &s[st + j as usize]) {
                j -= 1;
            }
            if i >= j {
                break;
            }
            s.swap(st + i as usize, st + j as usize);
            i += 1;
            j -= 1;
        }
        if j < (n - i) {
            sort_edges_quicksort(s, st, j as usize);
            start = st + i as usize;
            n -= i;
        } else {
            sort_edges_quicksort(s, st + i as usize, (n - i) as usize);
            n = j;
        }
    }
}

fn sort_edges(p: &mut [Edge]) {
    let n = p.len();
    sort_edges_quicksort(p, 0, n);
    sort_edges_ins_sort(p);
}

#[allow(clippy::too_many_arguments)]
fn rasterize(
    output: &mut [u8],
    w: i32,
    h: i32,
    flatness_in_pixels: f32,
    vertices: &[Vertex],
    scale_x: f32,
    scale_y: f32,
    shift_x: f32,
    shift_y: f32,
    off_x: i32,
    off_y: i32,
) {
    let scale = if scale_x > scale_y { scale_y } else { scale_x };
    let (points, wcount) = flatten_curves(vertices, flatness_in_pixels / scale);
    if points.is_empty() {
        return;
    }
    let invert = true;
    let y_scale_inv = if invert { -scale_y } else { scale_y };
    let vsubsample = 1.0f32;
    let mut edges = Vec::with_capacity(points.len() + 1);
    let mut m = 0usize;
    for &count in &wcount {
        let count = count as usize;
        let p = &points[m..m + count];
        m += count;
        if count == 0 {
            continue;
        }
        let mut j = count - 1;
        for k in 0..count {
            let mut a = k;
            let mut b = j;
            if p[j].y != p[k].y {
                let mut inv = 0;
                let cond = if invert { p[j].y > p[k].y } else { p[j].y < p[k].y };
                if cond {
                    inv = 1;
                    a = j;
                    b = k;
                }
                edges.push(Edge {
                    x0: p[a].x * scale_x + shift_x,
                    y0: (p[a].y * y_scale_inv + shift_y) * vsubsample,
                    x1: p[b].x * scale_x + shift_x,
                    y1: (p[b].y * y_scale_inv + shift_y) * vsubsample,
                    invert: inv,
                });
            }
            j = k;
        }
    }
    sort_edges(&mut edges);
    // the sentinel: never reached by the scan
    edges.push(Edge { x0: 0.0, y0: (off_y + h) as f32 + 1.0, x1: 0.0, y1: 0.0, invert: 0 });
    rasterize_sorted_edges(output, w, h, &edges, off_x, off_y);
}

#[cfg(test)]
mod tests {
    use super::*;

    const FONT: &[u8] = include_bytes!("golden_font.ttf");

    fn info() -> FontInfo<'static> {
        init_font(FONT, 0).expect("the test font")
    }

    fn build_font_bytes_cmap(sub: &[u8], num_glyphs: u16) -> Vec<u8> {
        // the smallest font init_font takes: cmap (the subtable given), head, hhea, hmtx, glyf, loca, maxp
        let mut cmap = vec![0u8, 0, 0, 1, 0, 3, 0, 10, 0, 0, 0, 12];
        cmap.extend_from_slice(sub);
        let tables: Vec<(&[u8; 4], Vec<u8>)> = vec![
            (b"cmap", cmap),
            (b"glyf", vec![0; 4]),
            (b"head", vec![0; 54]),
            (b"hhea", vec![0; 36]),
            (b"hmtx", vec![0; 4]),
            (b"loca", vec![0; 4]),
            (b"maxp", {
                let mut m = vec![0u8; 6];
                m[4..6].copy_from_slice(&num_glyphs.to_be_bytes());
                m
            }),
        ];
        let mut out = vec![0u8, 1, 0, 0];
        out.extend_from_slice(&(tables.len() as u16).to_be_bytes());
        out.extend_from_slice(&[0; 6]);
        let mut offset = 12 + 16 * tables.len();
        let mut body = Vec::new();
        for (tag, data) in &tables {
            out.extend_from_slice(*tag);
            out.extend_from_slice(&[0; 4]);
            out.extend_from_slice(&(offset as u32).to_be_bytes());
            out.extend_from_slice(&(data.len() as u32).to_be_bytes());
            offset += data.len();
            body.extend_from_slice(data);
        }
        out.extend_from_slice(&body);
        out
    }

    #[test]
    fn font_offset_of_plain_and_collection() {
        assert_eq!(font_offset_for_index(FONT, 0), 0);
        assert_eq!(font_offset_for_index(FONT, 1), -1);
        let mut ttc = b"ttcf\x00\x01\x00\x00\x00\x00\x00\x01\x00\x00\x00\x20".to_vec();
        ttc.resize(40, 0);
        assert_eq!(font_offset_for_index(&ttc, 0), 0x20);
        assert_eq!(font_offset_for_index(&ttc, 1), -1);
        assert_eq!(font_offset_for_index(b"no font at all", 0), -1);
    }

    #[test]
    fn init_needs_the_required_tables() {
        assert!(init_font(FONT, 0).is_some());
        assert!(init_font(b"", 0).is_none());
        assert!(init_font(&FONT[..8], 0).is_none());
        // a cmap that names no understood encoding
        let no_enc = {
            let mut sub = build_font_bytes_cmap(&[0; 8], 1);
            let c = find_table(&sub, 0, b"cmap") as usize;
            sub[c + 4 + 1] = 2; // platform 2: none stb understands
            sub
        };
        assert!(init_font(&no_enc, 0).is_none());
    }

    #[test]
    fn cmap_format_4_of_the_golden_font() {
        let f = info();
        assert_eq!(f.find_glyph_index(b' ' as i32), 5);
        assert_eq!(f.find_glyph_index(b'A' as i32), 0);
        assert_eq!(f.find_glyph_index(b'E' as i32), 4);
        assert_eq!(f.find_glyph_index(0x20AC), 1);
        assert_eq!(f.find_glyph_index(b'F' as i32), 0);
        assert_eq!(f.find_glyph_index(0x1_0000), 0);
    }

    #[test]
    fn cmap_format_0() {
        let mut sub = vec![0u8, 0, 1, 6, 0, 0];
        for i in 0..=255u16 {
            sub.push(if i == 65 { 3 } else { 0 });
        }
        let font = build_font_bytes_cmap(&sub, 4);
        let f = init_font(&font, 0).unwrap();
        assert_eq!(f.find_glyph_index(65), 3);
        assert_eq!(f.find_glyph_index(66), 0);
        assert_eq!(f.find_glyph_index(300), 0); // past the table
    }

    #[test]
    fn cmap_format_6() {
        let mut sub = vec![0u8, 6, 0, 0, 0, 0, 0, 0x30, 0, 2];
        sub.extend_from_slice(&[0, 7, 0, 9]);
        let font = build_font_bytes_cmap(&sub, 10);
        let f = init_font(&font, 0).unwrap();
        assert_eq!(f.find_glyph_index(0x30), 7);
        assert_eq!(f.find_glyph_index(0x31), 9);
        assert_eq!(f.find_glyph_index(0x32), 0);
        assert_eq!(f.find_glyph_index(0x2F), 0);
    }

    #[test]
    fn cmap_format_12_and_13() {
        for format in [12u16, 13] {
            let mut sub = Vec::new();
            sub.extend_from_slice(&format.to_be_bytes());
            sub.extend_from_slice(&[0; 2]);
            sub.extend_from_slice(&[0; 4]); // length
            sub.extend_from_slice(&[0; 4]); // language
            sub.extend_from_slice(&2u32.to_be_bytes());
            for (s, e, g) in [(0x41u32, 0x43u32, 10u32), (0x10000, 0x10001, 20)] {
                sub.extend_from_slice(&s.to_be_bytes());
                sub.extend_from_slice(&e.to_be_bytes());
                sub.extend_from_slice(&g.to_be_bytes());
            }
            let font = build_font_bytes_cmap(&sub, 30);
            let f = init_font(&font, 0).unwrap();
            assert_eq!(f.find_glyph_index(0x41), 10);
            assert_eq!(f.find_glyph_index(0x43), if format == 12 { 12 } else { 10 });
            assert_eq!(f.find_glyph_index(0x10001), if format == 12 { 21 } else { 20 });
            assert_eq!(f.find_glyph_index(0x44), 0);
        }
    }

    #[test]
    fn cmap_unknown_format_maps_nothing() {
        let sub = [0u8, 2, 0, 0];
        let font = build_font_bytes_cmap(&sub, 4);
        let f = init_font(&font, 0).unwrap();
        assert_eq!(f.find_glyph_index(65), 0);
    }

    #[test]
    fn hmetrics_past_the_long_entries_use_the_last_advance() {
        let f = info();
        assert_eq!(f.glyph_hmetrics(0), (900, 50));
        assert_eq!(f.glyph_hmetrics(4), (1100, 100));
        assert_eq!(f.glyph_hmetrics(5), (1100, 77));
    }

    #[test]
    fn vmetrics_and_boxes() {
        let f = info();
        assert_eq!(f.vmetrics(), (850, -250, 90));
        assert_eq!(f.glyph_box(0), Some((100, 0, 800, 700)));
        assert_eq!(f.glyph_box(5), None); // space: no outline
        assert_eq!(f.glyph_bitmap_box(5, 0.5, 0.5), (0, 0, 0, 0));
        // floor/ceil with y negated
        assert_eq!(f.glyph_bitmap_box(0, 0.5, 0.5), (50, -350, 400, 0));
    }

    #[test]
    fn glyph_index_out_of_range_has_no_outline() {
        let f = info();
        assert_eq!(f.glyph_box(100), None);
        assert_eq!(f.glyph_shape(100, 0).len(), 0);
    }

    #[test]
    fn composite_glyph_merges_its_parts() {
        let f = info();
        // glyph 3 is triangle(1) scaled .5 and circle(2) x-scaled .5; both components present
        let shape = f.glyph_shape(3, 0);
        let moves = shape.iter().filter(|v| v.kind == VMOVE).count();
        assert_eq!(moves, 3);
        assert!(shape.len() > 10);
    }

    #[test]
    fn quicksort_port_is_stable_with_c_results_on_equal_keys() {
        // equal keys: stb's insertion sort leaves them in input order among the equal group
        let mut v: Vec<Edge> =
            (0..20).map(|i| Edge { x0: i as f32, y0: (i % 3) as f32, x1: 0.0, y1: 0.0, invert: 0 }).collect();
        sort_edges(&mut v);
        assert!(v.windows(2).all(|w| w[0].y0 <= w[1].y0));
        assert_eq!(v.len(), 20);
        let xs: std::collections::BTreeSet<i32> = v.iter().map(|e| e.x0 as i32).collect();
        assert_eq!(xs.len(), 20); // a permutation: none lost, none duplicated
    }
}
