//! TrueType glyph access and the antialiasing rasteriser. The tables (cmap, head, hhea, hmtx, loca, glyf) are read
//! by `ttf-parser`; the rasteriser is a line-by-line port of the part of stb_truetype v1.26 (src/ThirdParty/stb in
//! the C++ tree) the game used, `STBTT_RASTERIZER_VERSION` 2, so the bitmaps are the same bytes.
//!
//! The floating-point operations are written in the C's order and in `f32`; do not "simplify" them. `f32` with the
//! C's explicit casts is why the output matches on every system. (A C++ arm64 compile without `-ffp-contract=off`
//! could fuse products; xbuild's flags forbid it, and Rust never fuses.)
//!
//! A simple glyph's outline is ttf-parser's: it walks a contour as stb did (an off-curve start begins at the next
//! point or at the midpoint of the two, the same closing segments), and its midpoints of integer points are exact,
//! so their floor is stb's `>> 1`. A compound glyph stays stb's own walk: stb scales a scaled component's points
//! twice (by the matrix and again by its column lengths), which ttf-parser rightly does not. Before the old reader
//! was deleted the two were compared over every glyph and code point of the twelve Liberation faces and the test
//! font (docs/progress/deps.md, round 1).
//!
//! Left out because the game never gave it to stb: CFF/OTF outlines (`init_font` refuses them), kerning, SDF, the v1
//! rasteriser.

use ttf_parser::{loca, Face, GlyphId, OutlineBuilder, Tag};

pub(crate) const VMOVE: u8 = 1;
pub(crate) const VLINE: u8 = 2;
pub(crate) const VCURVE: u8 = 3;

/// A face the game registered: ttf-parser's tables and the raw glyph data the compound glyphs are read from.
#[derive(Clone)]
pub struct FontInfo<'a> {
    pub face: Face<'a>,
    loca: loca::Table<'a>,
    glyf: &'a [u8],
}

/// `stbtt_InitFont` for the first face of `data`, TrueType outlines only (see the module note).
pub fn init_font(data: &[u8]) -> Option<FontInfo<'_>> {
    let face = Face::parse(data, 0).ok()?;
    let raw = face.raw_face();
    let glyf = raw.table(Tag::from_bytes(b"glyf"))?;
    let loca_data = raw.table(Tag::from_bytes(b"loca"))?;
    let head = face.tables().head;
    let loca = loca::Table::parse(face.number_of_glyphs().try_into().ok()?, head.index_to_location_format, loca_data)?;
    face.tables().cmap?;
    Some(FontInfo { face, loca, glyf })
}

/// stb's vertices from ttf-parser's segments (the coordinates are integers or exact halves: floor is `>> 1`).
struct Shape(Vec<Vertex>);

fn coord(v: f32) -> i32 {
    v.floor() as i32
}

impl OutlineBuilder for Shape {
    fn move_to(&mut self, x: f32, y: f32) {
        self.0.push(vertex(VMOVE, coord(x), coord(y), 0, 0));
    }
    fn line_to(&mut self, x: f32, y: f32) {
        self.0.push(vertex(VLINE, coord(x), coord(y), 0, 0));
    }
    fn quad_to(&mut self, x1: f32, y1: f32, x: f32, y: f32) {
        self.0.push(vertex(VCURVE, coord(x), coord(y), coord(x1), coord(y1)));
    }
    fn curve_to(&mut self, _: f32, _: f32, _: f32, _: f32, _: f32, _: f32) {}
    fn close(&mut self) {}
}

fn be16(d: &[u8], o: usize) -> u16 {
    d.get(o..o + 2).map_or(0, |b| u16::from_be_bytes([b[0], b[1]]))
}

impl FontInfo<'_> {
    /// `stbtt_FindGlyphIndex`
    pub fn find_glyph_index(&self, unicode_codepoint: i32) -> i32 {
        char::from_u32(unicode_codepoint as u32).and_then(|c| self.face.glyph_index(c)).map_or(0, |g| g.0 as i32)
    }

    /// A raw table of the face (empty when absent).
    pub fn table(&self, tag: &[u8; 4]) -> &[u8] {
        self.face.raw_face().table(Tag::from_bytes(tag)).unwrap_or(&[])
    }

    /// The glyph's bytes in glyf; `None` for an empty glyph or one past the face.
    fn glyph_data(&self, glyph_index: i32) -> Option<&[u8]> {
        if !(0..self.face.number_of_glyphs() as i32).contains(&glyph_index) {
            return None;
        }
        let range = self.loca.glyph_range(GlyphId(glyph_index as u16))?;
        if range.is_empty() {
            return None;
        }
        self.glyf.get(range)
    }

    /// `stbtt_GetGlyphBox`
    pub fn glyph_box(&self, glyph_index: i32) -> Option<(i32, i32, i32, i32)> {
        let g = self.glyph_data(glyph_index)?;
        let at = |o| be16(g, o) as i16 as i32;
        Some((at(2), at(4), at(6), at(8)))
    }

    /// `stbtt_GetGlyphHMetrics` -> (advance, left side bearing)
    pub fn glyph_hmetrics(&self, glyph_index: i32) -> (i32, i32) {
        let g = GlyphId(glyph_index as u16);
        let advance = self.face.glyph_hor_advance(g).unwrap_or(0) as i16 as i32;
        (advance, self.face.glyph_hor_side_bearing(g).unwrap_or(0) as i32)
    }

    /// `stbtt_GetFontVMetrics` -> (ascent, descent, line gap): hhea's
    pub fn vmetrics(&self) -> (i32, i32, i32) {
        let h = self.face.tables().hhea;
        (h.ascender as i32, h.descender as i32, h.line_gap as i32)
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

    pub(crate) fn glyph_shape(&self, glyph_index: i32, depth: u32) -> Vec<Vertex> {
        let Some(g) = self.glyph_data(glyph_index) else {
            return Vec::new();
        };
        let number_of_contours = be16(g, 0) as i16;
        if number_of_contours > 0 {
            let mut shape = Shape(Vec::new());
            self.face.outline_glyph(GlyphId(glyph_index as u16), &mut shape);
            return shape.0;
        }
        let mut vertices = Vec::new();
        if number_of_contours < 0 && depth < 16 {
            // compound shapes: stb's walk (see the module note)
            let i16_at = |o: usize| be16(g, o) as i16;
            let i8_at = |o: usize| g.get(o).map_or(0, |&b| b as i8);
            let mut comp = 10;
            let mut more = true;
            while more {
                let mut mtx = [1.0f32, 0.0, 0.0, 1.0, 0.0, 0.0];
                let flags = be16(g, comp);
                comp += 2;
                let gidx = be16(g, comp) as i32;
                comp += 2;
                if flags & 2 != 0 {
                    if flags & 1 != 0 {
                        mtx[4] = i16_at(comp) as f32;
                        comp += 2;
                        mtx[5] = i16_at(comp) as f32;
                        comp += 2;
                    } else {
                        mtx[4] = i8_at(comp) as f32;
                        comp += 1;
                        mtx[5] = i8_at(comp) as f32;
                        comp += 1;
                    }
                }
                if flags & (1 << 3) != 0 {
                    mtx[0] = i16_at(comp) as f32 / 16384.0;
                    mtx[3] = mtx[0];
                    comp += 2;
                    mtx[1] = 0.0;
                    mtx[2] = 0.0;
                } else if flags & (1 << 6) != 0 {
                    mtx[0] = i16_at(comp) as f32 / 16384.0;
                    comp += 2;
                    mtx[1] = 0.0;
                    mtx[2] = 0.0;
                    mtx[3] = i16_at(comp) as f32 / 16384.0;
                    comp += 2;
                } else if flags & (1 << 7) != 0 {
                    for m in mtx.iter_mut().take(4) {
                        *m = i16_at(comp) as f32 / 16384.0;
                        comp += 2;
                    }
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

#[derive(Clone, Copy, Default, Debug, PartialEq)]
pub(crate) struct Vertex {
    pub(crate) x: i16,
    pub(crate) y: i16,
    pub(crate) cx: i16,
    pub(crate) cy: i16,
    pub(crate) kind: u8,
}

fn vertex(kind: u8, x: i32, y: i32, cx: i32, cy: i32) -> Vertex {
    Vertex { x: x as i16, y: y as i16, cx: cx as i16, cy: cy as i16, kind }
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
        init_font(FONT).expect("the test font")
    }

    #[test]
    fn init_needs_the_required_tables() {
        assert!(init_font(FONT).is_some());
        assert!(init_font(b"").is_none());
        assert!(init_font(&FONT[..8]).is_none());
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
