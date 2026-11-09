//! `DX7Rasterizer Draw2D.cpp`: bitmaps, lines, points, rhombi and the text layout. C++ owns the `GEBitmap`/`GELine`/
//! `GEPoint`/`GEText` objects and reaches them for Rust through [`Draw2DHost`] (vertex memory comes back as raw pointers,
//! read and written between host calls).
//!
//! Golden: `golden_draw.txt` (sections `bitmap_*`, `lines_*`, `points_*`, `rhombus`, `text_*`), printed by a scratch run
//! of the original C++ bodies.
#![allow(clippy::unnecessary_cast)]

use core::ffi::{c_char, c_long, c_ulong, c_void};

use crate::draw::*;
use crate::vertex::Dx7StdColorVertex;

const INDICES_QUAD: [u16; 6] = [0, 1, 2, 2, 1, 3];

/// The line-nudge of `Draw(GELine*)` (`kLineOffset`).
const LINE_OFFSET: f32 = 0.001;

/// The per-object reads of the 2D draws. Handles are the C++ `GEBitmap*`, `GELine*` (array), `GEPoint*` (array),
/// `GERhombus*`, `GETexture*`.
pub trait Draw2DHost: DrawHost {
    fn bitmap_texture(&mut self, bitmap: Handle) -> Handle;
    fn bitmap_vertices(&mut self, bitmap: Handle) -> *mut Dx7StdColorVertex;
    /// `GELine[0].GetTexture(screen)`.
    fn line_texture(&mut self, lines: Handle) -> Handle;
    /// `GELine[index]`'s two vertices.
    fn line_vertices(&mut self, lines: Handle, index: u32) -> *mut Dx7StdColorVertex;
    fn point_texture(&mut self, points: Handle) -> Handle;
    fn point_vertices(&mut self, points: Handle, index: u32) -> *mut Dx7StdColorVertex;
    fn rhombus_vertices(&mut self, rhombus: Handle) -> *mut Dx7StdColorVertex;
    /// `GetFrameUVSize` and `GetFrameTopLeftCorner` of the texture.
    fn texture_frame(&mut self, texture: Handle) -> (f32, [f32; 2]);
}

unsafe fn remap_uv(v: *mut Dx7StdColorVertex, size: f32, corner: [f32; 2]) {
    let tu = unsafe { (*v).tu };
    let tv = unsafe { (*v).tv };
    unsafe {
        (*v).tu = tu * size + corner[0];
        (*v).tv = tv * size + corner[1];
    }
}

/// `Draw(GEBitmap*)`: a textured quad; rewrites the quad's four UVs in place into the texture's frame. 9 without a
/// texture.
pub fn draw_bitmap(h: &mut impl Draw2DHost, bitmap: Handle) -> LongResult {
    h.enter();
    h.set_2d_mode();
    let texture = h.bitmap_texture(bitmap);
    let result;
    if !texture.is_null() {
        let vertices = h.bitmap_vertices(bitmap);
        let (size, corner) = h.texture_frame(texture);
        if size != 1.0 {
            for i in 0..4 {
                unsafe { remap_uv(vertices.add(i), size, corner) };
            }
        }
        h.set_texture(texture);
        let buffer = h.lock_vb(4);
        unsafe { core::ptr::copy_nonoverlapping(vertices, buffer.add(h.vb_position() as usize), 4) };
        result = h.unlock_vb();
        let start = h.vb_position();
        draw_indexed_retry(h, PT_TRIANGLELIST, start, 4, INDICES_QUAD.as_ptr(), 6);
        h.advance_vb(4);
    } else {
        result = 9;
    }
    h.leave();
    result
}

/// `Draw(GELine*, count)`: one line list, both ends nudged by 0.001; only each line's first vertex UV is remapped in
/// place into the texture frame (a null texture: no remap).
pub fn draw_lines(h: &mut impl Draw2DHost, lines: Handle, count: u32) -> LongResult {
    let vertex_count = count * 2;
    h.enter();
    h.set_2d_mode();
    let texture = h.line_texture(lines);
    let has_texture = !texture.is_null();
    h.set_texture(texture);
    let locked = h.lock_vb(vertex_count);
    let mut buffer = unsafe { locked.add(h.vb_position() as usize) };
    for i in 0..count {
        let vertices = h.line_vertices(lines, i);
        if has_texture {
            let (size, corner) = h.texture_frame(texture);
            if size != 1.0 {
                unsafe { remap_uv(vertices, size, corner) };
            }
        }
        unsafe {
            core::ptr::copy_nonoverlapping(vertices, buffer, 2);
            (*buffer).x += LINE_OFFSET;
            (*buffer).y += LINE_OFFSET;
            (*buffer.add(1)).x += LINE_OFFSET;
            (*buffer.add(1)).y += LINE_OFFSET;
            buffer = buffer.add(2);
        }
    }
    let result = h.unlock_vb();
    let start = h.vb_position();
    draw_primitive_retry(h, PT_LINELIST, start, vertex_count);
    h.advance_vb(vertex_count);
    h.leave();
    result
}

/// `Draw(GEPoint*, count)`: one point list, each point's UV remapped in place.
pub fn draw_points(h: &mut impl Draw2DHost, points: Handle, count: u32) -> LongResult {
    h.enter();
    h.set_2d_mode();
    let texture = h.point_texture(points);
    let has_texture = !texture.is_null();
    h.set_texture(texture);
    let locked = h.lock_vb(count);
    let mut buffer = unsafe { locked.add(h.vb_position() as usize) };
    for i in 0..count {
        let vertices = h.point_vertices(points, i);
        if has_texture {
            let (size, corner) = h.texture_frame(texture);
            if size != 1.0 {
                unsafe { remap_uv(vertices, size, corner) };
            }
        }
        unsafe {
            core::ptr::copy_nonoverlapping(vertices, buffer, 1);
            buffer = buffer.add(1);
        }
    }
    let result = h.unlock_vb();
    let start = h.vb_position();
    draw_primitive_retry(h, PT_POINTLIST, start, count);
    h.advance_vb(count);
    h.leave();
    result
}

/// `Draw(GERhombus*)`: an untextured quad.
pub fn draw_rhombus(h: &mut impl Draw2DHost, rhombus: Handle) -> LongResult {
    h.enter();
    h.set_2d_mode();
    h.set_texture(core::ptr::null_mut());
    let vertices = h.rhombus_vertices(rhombus);
    let buffer = h.lock_vb(4);
    unsafe { core::ptr::copy_nonoverlapping(vertices, buffer.add(h.vb_position() as usize), 4) };
    let result = h.unlock_vb();
    let start = h.vb_position();
    draw_indexed_retry(h, PT_TRIANGLELIST, start, 4, INDICES_QUAD.as_ptr(), 6);
    h.advance_vb(4);
    h.leave();
    result
}

// --- text -------------------------------------------------------------------------------------------------------

pub const ALIGN_LEFT: u32 = 0;
pub const ALIGN_CENTER: u32 = 1;
pub const ALIGN_RIGHT: u32 = 2;

/// `RECT`: Win32 `LONG`s, 32 bits on every system (`wintypes.h`), pinned by the C++.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct TextRect {
    pub left: i32,
    pub top: i32,
    pub right: i32,
    pub bottom: i32,
}

/// One `GEText` and its font as plain data (`DX7DrawText`). `font` is a `PlatFont*`, null when the font has none.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct TextInfo {
    pub font: *const c_void,
    pub text: *const c_char,
    pub length: u32,
    pub x: f32,
    pub y: f32,
    pub alignment: c_ulong,
    pub transparent: bool,
    pub shadow_font: bool,
    pub foreground: [f32; 3],
    pub background: [f32; 3],
}

/// The clip of one text draw: left/top/right/bottom exclusive, in target pixels.
pub type TextClip = [i32; 4];

/// The PlatFont calls (`platform/PlatFont.h`), seen by the text layout.
pub trait TextHost {
    /// `PlatFontMeasure` of the text.
    fn measure(&mut self, font: *const c_void, text: *const c_char, length: u32) -> i32;
    /// `PlatFontScaled(font, scale)`.
    fn scaled(&mut self, font: *const c_void, scale: f32) -> *const c_void;
    /// `PlatFontGetMetrics(font).mHeight`.
    fn font_height(&mut self, font: *const c_void) -> i32;
    #[allow(clippy::too_many_arguments)]
    fn draw(
        &mut self,
        font: *const c_void,
        text: *const c_char,
        length: u32,
        x: i32,
        y: i32,
        color: u32,
        opaque: bool,
        background: u32,
        clip: &TextClip,
    );
}

/// `DX7TextColor`: 0xRRGGBB, each byte `(BYTE)(c * 255)` (truncation).
pub fn text_color(c: [f32; 3]) -> u32 {
    let byte = |v: f32| (v * 255.0) as i32 as u8 as u32;
    (byte(c[0]) << 16) | (byte(c[1]) << 8) | byte(c[2])
}

fn aligned_x(info: &TextInfo, width: i32) -> f32 {
    match info.alignment as u32 {
        ALIGN_CENTER => info.x - (width / 2) as f32,
        ALIGN_RIGHT => info.x - width as f32,
        _ => info.x,
    }
}

/// `DX7TextBounds`: the pixels a text draw can touch.
pub fn text_bounds(h: &mut impl TextHost, info: &TextInfo, clip: Option<&TextRect>) -> TextRect {
    let mut width: c_long = 0;
    let mut height: c_long = 0;
    if !info.font.is_null() {
        height = h.font_height(info.font) as c_long;
        width = h.measure(info.font, info.text, info.length) as c_long;
    }
    let x = aligned_x(info, width as i32);
    let margin = height / 2 + 2;
    let mut bounds = TextRect {
        left: ((x as c_long) - margin) as i32,
        top: ((info.y as c_long) - 2) as i32,
        right: ((x as c_long) + width + margin) as i32,
        bottom: ((info.y as c_long) + height + 3) as i32,
    };
    if let Some(c) = clip {
        bounds.left = bounds.left.max(c.left);
        bounds.top = bounds.top.max(c.top);
        bounds.right = bounds.right.min(c.right);
        bounds.bottom = bounds.bottom.min(c.bottom);
    }
    bounds
}

/// One text draw (`DX7DrawTextPlat`): the arguments are the callback's. Returns false when the font has no OS font.
#[allow(clippy::too_many_arguments)]
pub fn text_draw(
    h: &mut impl TextHost,
    info: &TextInfo,
    clip: Option<&TextRect>,
    origin_x: c_long,
    origin_y: c_long,
    target_width: i32,
    target_height: i32,
    target_scale: f32,
) -> bool {
    if info.font.is_null() {
        return false;
    }
    let scale = if target_scale > 1.0 { target_scale } else { 1.0 };
    let draw_font = h.scaled(info.font, scale);
    let draw_clip: TextClip = match clip {
        Some(c) => [
            (((c.left as c_long - origin_x) as f32 * scale).floor()) as i32,
            (((c.top as c_long - origin_y) as f32 * scale).floor()) as i32,
            (((c.right as c_long - origin_x) as f32 * scale).ceil()) as i32,
            (((c.bottom as c_long - origin_y) as f32 * scale).ceil()) as i32,
        ],
        None => [0, 0, target_width, target_height],
    };
    let width = h.measure(info.font, info.text, info.length);
    let x = aligned_x(info, width);
    let mut draw_x = (((x as c_long) - origin_x) as f32) * scale;
    if scale > 1.0 && info.alignment as u32 != ALIGN_LEFT {
        let scaled_width = h.measure(draw_font, info.text, info.length) as f32;
        draw_x +=
            (width as f32 * scale - scaled_width) * (if info.alignment as u32 == ALIGN_CENTER { 0.5 } else { 1.0 });
    }
    let draw_x = draw_x.round() as i32;
    let draw_y = (((info.y as c_long) - origin_y) as f32 * scale).round() as i32;
    let shadow = if scale.round() as i32 > 0 { scale.round() as i32 } else { 1 };
    let opaque = !info.transparent;
    let background = text_color(info.background);
    if info.shadow_font {
        let f = info.foreground;
        let shade = if f[0] + f[1] + f[2] > 0.1 { 0 } else { 0xffffff };
        h.draw(
            draw_font,
            info.text,
            info.length,
            draw_x + shadow,
            draw_y + shadow,
            shade,
            opaque,
            background,
            &draw_clip,
        );
    }
    h.draw(
        draw_font,
        info.text,
        info.length,
        draw_x,
        draw_y,
        text_color(info.foreground),
        opaque,
        background,
        &draw_clip,
    );
    true
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::draw::testing::{golden_section, impl_draw_host_for_rec, Rec};

    /// The generator's vertex filler (`V` in the scratch source).
    fn cv(s: f32) -> Dx7StdColorVertex {
        Dx7StdColorVertex {
            x: s + 1.1,
            y: s + 2.2,
            z: s + 3.3,
            nx: 0.1,
            ny: 0.2,
            nz: 0.3,
            color: 0x8011_2233u32.wrapping_add(s as u32),
            tu: s * 0.125,
            tv: 1.0 - s * 0.0625,
        }
    }

    struct Host {
        rec: Rec,
        names: Vec<(Handle, &'static str)>,
        frames: Vec<(Handle, f32, [f32; 2])>,
        // the "objects": bitmap/rhombus -> (texture, vertices); line/point arrays -> texture and per-element vertices
        objects: Vec<(Handle, Handle, Vec<*mut Dx7StdColorVertex>)>,
    }
    impl_draw_host_for_rec!(Host, names);

    impl Draw2DHost for Host {
        fn bitmap_texture(&mut self, b: Handle) -> Handle {
            self.objects.iter().find(|o| o.0 == b).unwrap().1
        }
        fn bitmap_vertices(&mut self, b: Handle) -> *mut Dx7StdColorVertex {
            self.objects.iter().find(|o| o.0 == b).unwrap().2[0]
        }
        fn line_texture(&mut self, l: Handle) -> Handle {
            self.objects.iter().find(|o| o.0 == l).unwrap().1
        }
        fn line_vertices(&mut self, l: Handle, i: u32) -> *mut Dx7StdColorVertex {
            self.objects.iter().find(|o| o.0 == l).unwrap().2[i as usize]
        }
        fn point_texture(&mut self, p: Handle) -> Handle {
            self.objects.iter().find(|o| o.0 == p).unwrap().1
        }
        fn point_vertices(&mut self, p: Handle, i: u32) -> *mut Dx7StdColorVertex {
            self.objects.iter().find(|o| o.0 == p).unwrap().2[i as usize]
        }
        fn rhombus_vertices(&mut self, r: Handle) -> *mut Dx7StdColorVertex {
            self.objects.iter().find(|o| o.0 == r).unwrap().2[0]
        }
        fn texture_frame(&mut self, t: Handle) -> (f32, [f32; 2]) {
            let f = self.frames.iter().find(|o| o.0 == t).unwrap();
            (f.1, f.2)
        }
    }

    fn handle(v: usize) -> Handle {
        v as Handle
    }

    fn host() -> Host {
        let (t1, t2) = (handle(0x100), handle(0x200));
        Host {
            rec: Rec::new(),
            names: vec![(t1, "T1"), (t2, "T2")],
            frames: vec![(t1, 0.5, [0.25, 0.125]), (t2, 1.0, [0.0, 0.0])],
            objects: Vec::new(),
        }
    }

    fn fmt_arr(v: &[Dx7StdColorVertex]) -> Vec<String> {
        use crate::draw::testing::b;
        v.iter().map(|x| format!("  arr {} {}", b(x.tu), b(x.tv))).collect()
    }

    fn assert_section(h: Host, section: &str) {
        let got = h.rec.log;
        let want = golden_section(section);
        assert_eq!(got, want, "section {section}");
    }

    fn finish(h: &mut Host, r: LongResult, arrs: &[Dx7StdColorVertex]) {
        h.rec.log.push(format!("ret {r}"));
        h.rec.log.extend(fmt_arr(arrs));
        let end = h.rec.end_line();
        h.rec.log.push(end);
    }

    #[test]
    fn bitmap_with_uv_rewrites_vertices_and_draws() {
        let mut h = host();
        let mut v: Vec<_> = (0..4).map(|i| cv(i as f32)).collect();
        h.objects.push((handle(1), handle(0x100), vec![v.as_mut_ptr()]));
        let r = draw_bitmap(&mut h, handle(1));
        finish(&mut h, r, &v);
        assert_section(h, "bitmap_uv");
    }

    #[test]
    fn bitmap_full_frame_keeps_uv_and_retries_busy() {
        let mut h = host();
        h.rec.busy = 2;
        let mut v: Vec<_> = (0..4).map(|i| cv(10.0 + i as f32)).collect();
        h.objects.push((handle(1), handle(0x200), vec![v.as_mut_ptr()]));
        let r = draw_bitmap(&mut h, handle(1));
        finish(&mut h, r, &v);
        assert_section(h, "bitmap_no_uv");
    }

    #[test]
    fn bitmap_without_texture_returns_9() {
        let mut h = host();
        let mut v: Vec<_> = (0..4).map(|i| cv(20.0 + i as f32)).collect();
        h.objects.push((handle(1), core::ptr::null_mut(), vec![v.as_mut_ptr()]));
        let r = draw_bitmap(&mut h, handle(1));
        finish(&mut h, r, &[]);
        assert_section(h, "bitmap_null");
    }

    #[test]
    fn bitmap_at_buffer_end_restarts_the_buffer() {
        let mut h = host();
        h.rec.position = 0x960 - 2;
        let mut v: Vec<_> = (0..4).map(|i| cv(30.0 + i as f32)).collect();
        h.objects.push((handle(1), handle(0x200), vec![v.as_mut_ptr()]));
        let r = draw_bitmap(&mut h, handle(1));
        finish(&mut h, r, &[]);
        assert_section(h, "bitmap_overflow");
    }

    fn run_lines(tex: Handle, section: &str) {
        let mut h = host();
        let mut data: Vec<_> = (0..3).flat_map(|k| (0..2).map(move |i| cv(100.0 * k as f32 + i as f32))).collect();
        let base = data.as_mut_ptr();
        let vptrs = (0..3).map(|k| unsafe { base.add(k * 2) }).collect();
        h.objects.push((handle(2), tex, vptrs));
        let r = draw_lines(&mut h, handle(2), 3);
        finish(&mut h, r, &data);
        assert_section(h, section);
    }

    #[test]
    fn lines_with_texture_remap_first_vertex_only() {
        run_lines(handle(0x100), "lines_tex");
    }

    #[test]
    fn lines_without_texture_are_only_nudged() {
        run_lines(core::ptr::null_mut(), "lines_notex");
    }

    fn run_points(tex: Handle, section: &str) {
        let mut h = host();
        let mut data: Vec<_> = (0..2).map(|i| cv(200.0 + i as f32)).collect();
        let base = data.as_mut_ptr();
        let vptrs = (0..2).map(|k| unsafe { base.add(k) }).collect();
        h.objects.push((handle(3), tex, vptrs));
        let r = draw_points(&mut h, handle(3), 2);
        finish(&mut h, r, &data);
        assert_section(h, section);
    }

    #[test]
    fn points_with_texture_remap_uv() {
        run_points(handle(0x100), "points_tex");
    }

    #[test]
    fn points_without_texture_copy_as_is() {
        run_points(core::ptr::null_mut(), "points_notex");
    }

    #[test]
    fn rhombus_is_an_untextured_quad() {
        let mut h = host();
        let mut v: Vec<_> = (0..4).map(|i| cv(300.0 + i as f32)).collect();
        h.objects.push((handle(4), core::ptr::null_mut(), vec![v.as_mut_ptr()]));
        let r = draw_rhombus(&mut h, handle(4));
        finish(&mut h, r, &[]);
        assert_section(h, "rhombus");
    }

    // --- text --------------------------------------------------------------------------------------------------

    /// The generator's PlatFont fake: `measure` is `len * 7 * scale` (the scale in the font pointer's low bits).
    struct TextRec {
        log: Vec<String>,
    }

    const FONT: usize = 0x10000;

    fn scale_of(font: *const c_void) -> f32 {
        let off = (font as usize) & 0xfff;
        if off != 0 {
            off as f32 / 256.0
        } else {
            1.0
        }
    }

    impl TextHost for TextRec {
        fn measure(&mut self, font: *const c_void, _t: *const c_char, len: u32) -> i32 {
            (len as f32 * 7.0 * scale_of(font)) as i32
        }
        fn scaled(&mut self, font: *const c_void, scale: f32) -> *const c_void {
            (font as usize + (scale * 256.0) as usize) as *const c_void
        }
        fn font_height(&mut self, _font: *const c_void) -> i32 {
            12
        }
        fn draw(
            &mut self,
            _font: *const c_void,
            text: *const c_char,
            len: u32,
            x: i32,
            y: i32,
            color: u32,
            opaque: bool,
            background: u32,
            clip: &TextClip,
        ) {
            let s = unsafe { core::slice::from_raw_parts(text as *const u8, len as usize) };
            self.log.push(format!(
                "fontdraw {} {x} {y} {color:08x} {} {background:08x} {} {} {} {}",
                core::str::from_utf8(s).unwrap(),
                if opaque { 1 } else { 0 },
                clip[0],
                clip[1],
                clip[2],
                clip[3]
            ));
        }
    }

    #[allow(clippy::too_many_arguments)]
    fn text_case(
        section: &str,
        text: &'static str,
        alignment: u32,
        x: f32,
        y: f32,
        style_shadow: bool,
        transparent: bool,
        fg: [f32; 3],
        bg: [f32; 3],
        scale: f32,
        no_font: bool,
        clip: Option<TextRect>,
    ) {
        let mut h = TextRec { log: Vec::new() };
        let info = TextInfo {
            font: if no_font { core::ptr::null() } else { FONT as *const c_void },
            text: text.as_ptr() as *const c_char,
            length: text.len() as u32,
            x,
            y,
            alignment: alignment as c_ulong,
            transparent,
            shadow_font: style_shadow,
            foreground: fg,
            background: bg,
        };
        let b = text_bounds(&mut h, &info, clip.as_ref());
        h.log.push(format!("textbounds {} {} {} {}", b.left, b.top, b.right, b.bottom));
        let ok = text_draw(&mut h, &info, clip.as_ref(), 3, 5, 640, 480, scale);
        let ret = if ok { 0 } else { kerror_native() };
        h.log.push(format!("ret {ret}"));
        // the textbounds line of the golden comes first; the callback's lines after
        let want = golden_section(section);
        assert_eq!(h.log, want, "{section}");
    }

    /// `kErrorNativeError` (the C++ value of the job result when the OS font is missing).
    fn kerror_native() -> i64 {
        golden_section("text_nofont").last().unwrap()["ret ".len()..].parse().unwrap()
    }

    const C1: [f32; 3] = [0.6, 0.3, 0.2];
    const BG: [f32; 3] = [0.1, 0.2, 0.3];
    const WHITE: [f32; 3] = [1.0, 1.0, 1.0];
    const BLACK: [f32; 3] = [0.0, 0.0, 0.0];

    #[test]
    fn text_left_aligned() {
        text_case("text_left", "abc", 0, 50.7, 80.3, false, true, C1, BG, 1.0, false, None);
    }

    #[test]
    fn text_center_with_shadow() {
        text_case("text_center_shadow", "abcd", 1, 100.25, 40.9, true, false, WHITE, BG, 1.0, false, None);
    }

    #[test]
    fn text_right_with_shadow_on_dark_text() {
        text_case("text_right_darkshadow", "ab", 2, 200.5, 41.0, true, true, BLACK, BG, 1.0, false, None);
    }

    #[test]
    fn text_without_os_font_fails_after_the_bounds() {
        text_case("text_nofont", "x", 0, 1.0, 2.0, false, true, C1, BG, 1.0, true, None);
    }

    #[test]
    fn text_scaled_left_with_shadow() {
        text_case("text_scaled_left", "abc", 0, 50.7, 80.3, true, true, C1, BG, 2.0, false, None);
    }

    #[test]
    fn text_scaled_center_with_clip() {
        let clip = TextRect { left: 10, top: 20, right: 300, bottom: 400 };
        text_case("text_scaled_center_clip", "abcde", 1, 100.25, 40.9, false, false, C1, BG, 1.5, false, Some(clip));
    }

    #[test]
    fn text_right_with_clip() {
        let clip = TextRect { left: 10, top: 20, right: 300, bottom: 400 };
        text_case("text_right_clip", "abc", 2, 90.0, 7.5, false, true, C1, BG, 1.0, false, Some(clip));
    }

    #[test]
    fn text_color_truncates_each_byte() {
        assert_eq!(text_color([1.0, 0.5, 0.0]), 0x00ff_7f00);
    }
}
