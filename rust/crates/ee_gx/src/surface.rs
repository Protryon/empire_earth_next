//! A DirectDraw surface's plain data and its CPU-side operations (the copy in the pixel format, CPUBlt). GPU work goes
//! through `gx::Gx`.

use std::ffi::c_void;

use crate::abi::*;
use crate::format::Format;

pub type SurfaceId = usize;

/// `GXSurfaceData`.
pub struct Surface {
    /// the `GXSurface*` the present hook hands out; null for GX's own temporaries
    pub handle: *mut c_void,
    pub references: u32,
    pub caps: u32,
    pub caps2: u32,
    pub width: u32,
    pub height: u32,
    pub pixel_format: GxPixelFormat,
    pub format: Format,
    pub window: *mut c_void,
    pub native: NativeWindow,
    pub is_primary: bool,
    pub is_z: bool,
    pub is_texture: bool,
    pub is_render_target: bool,
    pub scale: f32,
    pub memory: Option<Vec<u8>>,
    pub pitch: i32,
    pub locked: bool,
    pub cpu_valid: bool,
    pub gpu_valid: bool,
    pub has_texture: bool,
    pub mip_levels: u32,
    pub mip_top: Option<SurfaceId>,
    pub mip_level: u32,
    pub levels_dirty: bool,
    pub used_serial: u64,
    pub attached: Vec<SurfaceId>,
    pub depth: Option<SurfaceId>,
    pub has_key: bool,
    pub key: u32,
    pub swap_created: bool,
    pub swap_width: u32,
    pub swap_height: u32,
    pub swap_format: Option<wgpu::TextureFormat>,
    pub swap_mode: wgpu::PresentMode,
    pub last_frame: Option<SurfaceId>,
    pub window_image: Option<SurfaceId>,
    pub window_image_current: bool,
    pub origin_x: i32,
    pub origin_y: i32,
}

impl Default for Surface {
    fn default() -> Self {
        Surface {
            handle: std::ptr::null_mut(),
            references: 1,
            caps: 0,
            caps2: 0,
            width: 0,
            height: 0,
            pixel_format: GxPixelFormat::default(),
            format: Format::default(),
            window: std::ptr::null_mut(),
            native: NativeWindow::default(),
            is_primary: false,
            is_z: false,
            is_texture: false,
            is_render_target: false,
            scale: 1.0,
            memory: None,
            pitch: 0,
            locked: false,
            cpu_valid: true,
            gpu_valid: false,
            has_texture: false,
            mip_levels: 1,
            mip_top: None,
            mip_level: 0,
            levels_dirty: false,
            used_serial: 0,
            attached: Vec::new(),
            depth: None,
            has_key: false,
            key: 0,
            swap_created: false,
            swap_width: 0,
            swap_height: 0,
            swap_format: None,
            swap_mode: wgpu::PresentMode::Fifo,
            last_frame: None,
            window_image: None,
            window_image_current: false,
            origin_x: 0,
            origin_y: 0,
        }
    }
}

// SAFETY: the raw pointers (`handle`, `window`, `native`) are identities and platform handles the renderer's lock
// serialises; nothing dereferences them outside that lock.
unsafe impl Send for Surface {}

impl Surface {
    /// `TexWidth`: the GPU copy's size at this surface's render scale.
    pub fn tex_width(&self) -> u32 {
        if self.scale == 1.0 {
            self.width
        } else {
            (self.width as f32 * self.scale + 0.5) as u32
        }
    }

    pub fn tex_height(&self) -> u32 {
        if self.scale == 1.0 {
            self.height
        } else {
            (self.height as f32 * self.scale + 0.5) as u32
        }
    }

    /// `AllocateMemory`: the CPU copy, zeroed, one DWORD-aligned row at a time; `false` when it cannot have one.
    pub fn allocate_memory(&mut self) -> bool {
        if self.memory.is_some() {
            return true;
        }
        if self.is_z || self.is_primary || self.format.bits == 0 {
            return false;
        }
        self.pitch = ((self.width * (self.format.bits / 8) + 3) & !3) as i32;
        self.memory = Some(vec![0; self.pitch as usize * self.height as usize]);
        true
    }

    pub fn memory(&self) -> Option<&[u8]> {
        self.memory.as_deref()
    }

    pub fn read_pixel(&self, x: u32, y: u32) -> u32 {
        read_pixel_in(self.memory.as_deref().unwrap_or(&[]), self.pitch, &self.format, x, y)
    }

    pub fn write_pixel(&mut self, x: u32, y: u32, pixel: u32) {
        let (pitch, format) = (self.pitch, self.format);
        if let Some(memory) = self.memory.as_deref_mut() {
            format.write(&mut memory[offset(pitch, &format, x, y)..], pixel);
        }
    }

    pub fn whole(&self) -> Rect {
        Rect::whole(self.width, self.height)
    }

    /// `Clip`: the rectangle inside the surface.
    pub fn clip(&self, rect: Rect) -> Rect {
        Rect {
            left: rect.left.max(0),
            top: rect.top.max(0),
            right: rect.right.min(self.width as i32),
            bottom: rect.bottom.min(self.height as i32),
        }
    }

    /// `GPUPreferred`: a render target, or a surface whose CPU copy is stale.
    pub fn gpu_preferred(&self) -> bool {
        self.is_render_target || !self.cpu_valid
    }

    /// The 32-bit BGRA rows of the surface's CPU copy, `width` pixels each.
    pub fn bgra_rows(&self) -> Vec<u8> {
        let mut out = vec![0u8; self.width as usize * self.height as usize * 4];
        for y in 0..self.height {
            let row = &mut out[y as usize * self.width as usize * 4..][..self.width as usize * 4];
            self.bgra_row(y, row);
        }
        out
    }

    /// One row of the CPU copy as BGRA (`GXTestReadFrame`).
    pub fn bgra_row(&self, y: u32, out: &mut [u8]) {
        let Some(memory) = self.memory.as_deref() else { return };
        let row = &memory[y as usize * self.pitch as usize..];
        if self.format.is_bgra32() {
            out[..self.width as usize * 4].copy_from_slice(&row[..self.width as usize * 4]);
        } else {
            for x in 0..self.width {
                out[x as usize * 4..][..4].copy_from_slice(&self.format.to_bgra(self.read_pixel(x, y)));
            }
        }
    }

    /// `UploadLevel`'s texel conversion: the level's pixels as BGRA, with the colour key baked into a texture's alpha.
    pub fn upload_pixels(&self, keyed: bool, key: u32) -> Vec<u8> {
        let memory = self.memory.as_deref().unwrap_or(&[]);
        let (width, height) = (self.width as usize, self.height as usize);
        let mut pixels = vec![0u8; width * height * 4];
        let format = &self.format;
        for y in 0..height {
            let row = &memory[y * self.pitch as usize..];
            let out = &mut pixels[y * width * 4..][..width * 4];
            if format.is_bgra32() {
                out.copy_from_slice(&row[..width * 4]);
                if format.mask[3] == 0 {
                    for x in 0..width {
                        out[x * 4 + 3] = 255;
                    }
                }
            } else {
                for x in 0..width {
                    out[x * 4..][..4].copy_from_slice(&format.to_bgra(self.read_pixel(x as u32, y as u32)));
                }
            }
            if keyed {
                // colour-keyed texels are transparent to the 3D device (COLORKEYENABLE and blending)
                let not_alpha = !format.mask[3];
                for x in 0..width {
                    if self.read_pixel(x as u32, y as u32) & not_alpha == key & not_alpha {
                        out[x * 4 + 3] = 0;
                    }
                }
            }
        }
        pixels
    }

    /// `Download`'s conversion: the BGRA rows read from the GPU into the CPU copy (`rows`: `width` pixels each).
    pub fn set_from_bgra_rows(&mut self, rows: &[u8]) {
        let (width, height) = (self.width as usize, self.height as usize);
        for y in 0..height {
            let row = &rows[y * width * 4..][..width * 4];
            if self.format.is_bgra32() {
                let pitch = self.pitch as usize;
                if let Some(memory) = self.memory.as_deref_mut() {
                    memory[y * pitch..][..width * 4].copy_from_slice(row);
                }
            } else {
                for x in 0..width {
                    let pixel = self.format.from_bgra(&row[x * 4..x * 4 + 4]);
                    self.write_pixel(x as u32, y as u32, pixel);
                }
            }
        }
    }
}

fn offset(pitch: i32, format: &Format, x: u32, y: u32) -> usize {
    y as usize * pitch as usize + x as usize * (format.bits as usize / 8)
}

fn read_pixel_in(memory: &[u8], pitch: i32, format: &Format, x: u32, y: u32) -> u32 {
    format.read(&memory[offset(pitch, format, x, y)..])
}

/// One side of a CPU blit: a surface's memory and format, or a copy of them for a self-overlapping blit.
pub struct CpuView<'a> {
    pub memory: &'a [u8],
    pub pitch: i32,
    pub width: u32,
    pub height: u32,
    pub format: &'a Format,
    pub key: u32,
}

/// `CPUBlt`: DirectDraw Blt on the CPU, a nearest-neighbour stretch, the source colour key compared in the source format.
#[allow(clippy::too_many_arguments)]
pub fn cpu_blt(
    target: &mut [u8],
    target_pitch: i32,
    target_width: u32,
    target_height: u32,
    target_format: &Format,
    destination: Rect,
    source: &CpuView,
    source_rect: Rect,
    keyed: bool,
    same_surface: bool,
) {
    let (dw, dh) = (destination.right - destination.left, destination.bottom - destination.top);
    let (sw, sh) = (source_rect.right - source_rect.left, source_rect.bottom - source_rect.top);
    if dw <= 0 || dh <= 0 || sw <= 0 || sh <= 0 {
        return;
    }
    let same_format = target_format.bits == source.format.bits && target_format.mask == source.format.mask;
    let bytes = (target_format.bits / 8) as usize;
    if same_format && !keyed && dw == sw && dh == sh && !same_surface {
        for y in 0..dh {
            let to = (destination.top + y) as usize * target_pitch as usize + destination.left as usize * bytes;
            let from = (source_rect.top + y) as usize * source.pitch as usize + source_rect.left as usize * bytes;
            target[to..to + dw as usize * bytes].copy_from_slice(&source.memory[from..from + dw as usize * bytes]);
        }
        return;
    }
    let key_mask = !source.format.mask[3];
    let mut write = |tx: u32, ty: u32, pixel: u32| {
        target_format.write(&mut target[offset(target_pitch, target_format, tx, ty)..], pixel);
    };
    for y in 0..dh {
        let sy = source_rect.top + (i64::from(y) * i64::from(sh) / i64::from(dh)) as i32;
        let ty = destination.top + y;
        if sy < 0 || sy >= source.height as i32 || ty < 0 || ty >= target_height as i32 {
            continue;
        }
        for x in 0..dw {
            let sx = source_rect.left + (i64::from(x) * i64::from(sw) / i64::from(dw)) as i32;
            let tx = destination.left + x;
            if sx < 0 || sx >= source.width as i32 || tx < 0 || tx >= target_width as i32 {
                continue;
            }
            let mut pixel = read_pixel_in(source.memory, source.pitch, source.format, sx as u32, sy as u32);
            if keyed && pixel & key_mask == source.key & key_mask {
                continue;
            }
            if !same_format {
                pixel = target_format.from_bgra(&source.format.to_bgra(pixel));
            }
            write(tx as u32, ty as u32, pixel);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const XRGB: GxPixelFormat = GxPixelFormat::rgb(32, 0xff0000, 0xff00, 0xff);
    const RGB565: GxPixelFormat = GxPixelFormat::rgb(16, 0xf800, 0x7e0, 0x1f);
    const ARGB1555: GxPixelFormat = GxPixelFormat::rgba(16, 0x7c00, 0x3e0, 0x1f, 0x8000);
    const ARGB: GxPixelFormat = GxPixelFormat::rgba(32, 0xff0000, 0xff00, 0xff, 0xff000000);

    fn surface(width: u32, height: u32, format: GxPixelFormat) -> Surface {
        let mut s = Surface { width, height, pixel_format: format, format: Format::set(&format), ..Default::default() };
        assert!(s.allocate_memory());
        s
    }

    #[test]
    fn allocation_aligns_rows_to_dwords() {
        let s = surface(3, 2, RGB565);
        assert_eq!(s.pitch, 8);
        assert_eq!(s.memory().unwrap().len(), 16);
        let s = surface(3, 1, XRGB);
        assert_eq!(s.pitch, 12);
    }

    #[test]
    fn allocation_refuses_z_and_primary_surfaces_and_formats_without_a_size() {
        let mut s = Surface { width: 4, height: 4, is_z: true, ..Default::default() };
        s.format.bits = 16;
        assert!(!s.allocate_memory());
        let mut s = Surface { width: 4, height: 4, is_primary: true, ..Default::default() };
        s.format.bits = 16;
        assert!(!s.allocate_memory());
        let mut s = Surface { width: 4, height: 4, ..Default::default() };
        assert!(!s.allocate_memory());
    }

    #[test]
    fn allocation_is_idempotent() {
        let mut s = surface(2, 2, XRGB);
        s.write_pixel(1, 1, 7);
        assert!(s.allocate_memory());
        assert_eq!(s.read_pixel(1, 1), 7, "the same memory");
    }

    #[test]
    fn scale_rounds_the_texture_size() {
        let s = Surface { width: 800, height: 600, scale: 1.5, ..Default::default() };
        assert_eq!((s.tex_width(), s.tex_height()), (1200, 900));
        let s = Surface { width: 1025, height: 767, scale: 1.25, ..Default::default() };
        assert_eq!(s.tex_width(), 1281); // 1281.25 + 0.5 truncates to 1281
        assert_eq!(s.tex_height(), 959);
        let s = Surface { width: 800, height: 600, ..Default::default() };
        assert_eq!((s.tex_width(), s.tex_height()), (800, 600));
    }

    #[test]
    fn clip_clamps_to_the_surface() {
        let s = surface(100, 50, XRGB);
        assert_eq!(s.clip(Rect::new(-5, -6, 200, 300)), Rect::new(0, 0, 100, 50));
        assert_eq!(s.clip(Rect::new(10, 20, 30, 40)), Rect::new(10, 20, 30, 40));
    }

    #[test]
    fn pixels_round_trip_in_both_widths() {
        let mut s = surface(4, 4, XRGB);
        s.write_pixel(3, 2, 0xdead_beef);
        assert_eq!(s.read_pixel(3, 2), 0xdead_beef);
        let mut s = surface(4, 4, RGB565);
        s.write_pixel(3, 2, 0xf800);
        assert_eq!(s.read_pixel(3, 2), 0xf800);
    }

    #[test]
    fn upload_pixels_makes_an_alpha_channel() {
        let mut s = surface(2, 1, XRGB);
        s.write_pixel(0, 0, 0x123456);
        let p = s.upload_pixels(false, 0);
        assert_eq!(&p[0..4], &[0x56, 0x34, 0x12, 255]);
    }

    #[test]
    fn upload_pixels_keys_texels_regardless_of_format() {
        let mut s = surface(3, 1, ARGB1555);
        s.write_pixel(0, 0, 0x7fff);
        s.write_pixel(1, 0, 0xffff); // the same colour with alpha set: the key ignores alpha
        s.write_pixel(2, 0, 0x0000);
        let p = s.upload_pixels(true, 0xffff);
        assert_eq!(p[3], 0);
        assert_eq!(p[4 + 3], 0);
        assert_eq!(p[8 + 3], 0, "alpha-less black reads as alpha 0 from the 1555 bit");
        let mut s = surface(2, 1, XRGB);
        s.write_pixel(0, 0, 0xff00ff);
        s.write_pixel(1, 0, 0x00ff00);
        let p = s.upload_pixels(true, 0xff00ff);
        assert_eq!(p[3], 0);
        assert_eq!(p[7], 255);
    }

    #[test]
    fn download_conversion_writes_each_format() {
        let mut s = surface(2, 1, RGB565);
        let rows = [255, 0, 0, 255, 0, 0, 255, 255];
        s.set_from_bgra_rows(&rows);
        assert_eq!(s.read_pixel(0, 0), 0x001f);
        assert_eq!(s.read_pixel(1, 0), 0xf800);
        let mut s = surface(1, 1, XRGB);
        s.set_from_bgra_rows(&[1, 2, 3, 4]);
        assert_eq!(s.read_pixel(0, 0), 0x04030201);
    }

    #[test]
    fn bgra_rows_read_each_format() {
        let mut s = surface(2, 1, XRGB);
        s.write_pixel(1, 0, 0x80ff4000);
        let rows = s.bgra_rows();
        assert_eq!(&rows[4..8], &[0x00, 0x40, 0xff, 0x80]);
        let mut s = surface(1, 1, RGB565);
        s.write_pixel(0, 0, 0xffff);
        assert_eq!(s.bgra_rows(), vec![255, 255, 255, 255]);
    }

    fn view<'a>(s: &'a Surface, key: u32) -> CpuView<'a> {
        CpuView {
            memory: s.memory().unwrap(),
            pitch: s.pitch,
            width: s.width,
            height: s.height,
            format: &s.format,
            key,
        }
    }

    #[allow(clippy::too_many_arguments)]
    fn blt(t: &mut Surface, d: Rect, s: &Surface, sr: Rect, keyed: bool, key: u32) {
        let format = t.format;
        let (pitch, w, h) = (t.pitch, t.width, t.height);
        let mut memory = t.memory.take().unwrap();
        cpu_blt(&mut memory, pitch, w, h, &format, d, &view(s, key), sr, keyed, false);
        t.memory = Some(memory);
    }

    #[test]
    fn same_format_blt_copies_rows() {
        let mut src = surface(2, 2, XRGB);
        src.write_pixel(0, 0, 1);
        src.write_pixel(1, 1, 2);
        let mut dst = surface(4, 4, XRGB);
        blt(&mut dst, Rect::new(1, 1, 3, 3), &src, src.whole(), false, 0);
        assert_eq!(dst.read_pixel(1, 1), 1);
        assert_eq!(dst.read_pixel(2, 2), 2);
        assert_eq!(dst.read_pixel(0, 0), 0);
    }

    #[test]
    fn keyed_blt_skips_key_pixels_ignoring_alpha_bits() {
        let mut src = surface(2, 1, ARGB);
        src.write_pixel(0, 0, 0x80ff00ff); // key colour, alpha set
        src.write_pixel(1, 0, 0x8000ff00);
        let mut dst = surface(2, 1, XRGB);
        dst.write_pixel(0, 0, 0x111111);
        dst.write_pixel(1, 0, 0x111111);
        let d = dst.whole();
        blt(&mut dst, d, &src, src.whole(), true, 0xff00ff);
        assert_eq!(dst.read_pixel(0, 0), 0x111111);
        assert_eq!(dst.read_pixel(1, 0) & 0xffffff, 0x00ff00);
    }

    #[test]
    fn stretch_is_nearest_neighbour() {
        let mut src = surface(2, 1, XRGB);
        src.write_pixel(0, 0, 10);
        src.write_pixel(1, 0, 20);
        let mut dst = surface(4, 1, XRGB);
        let d = dst.whole();
        blt(&mut dst, d, &src, src.whole(), false, 0);
        assert_eq!(
            [dst.read_pixel(0, 0), dst.read_pixel(1, 0), dst.read_pixel(2, 0), dst.read_pixel(3, 0)],
            [10, 10, 20, 20]
        );
    }

    #[test]
    fn blt_between_formats_converts() {
        let mut src = surface(1, 1, RGB565);
        src.write_pixel(0, 0, 0xf800);
        let mut dst = surface(1, 1, XRGB);
        let d = dst.whole();
        blt(&mut dst, d, &src, src.whole(), false, 0);
        assert_eq!(dst.read_pixel(0, 0) & 0xffffff, 0xff0000);
    }

    #[test]
    fn degenerate_rectangles_do_nothing() {
        let mut src = surface(1, 1, XRGB);
        src.write_pixel(0, 0, 9);
        let mut dst = surface(1, 1, XRGB);
        blt(&mut dst, Rect::new(0, 0, 0, 1), &src, src.whole(), false, 0);
        let d = dst.whole();
        blt(&mut dst, d, &src, Rect::new(0, 0, 0, 0), false, 0);
        assert_eq!(dst.read_pixel(0, 0), 0);
    }

    #[test]
    fn stretched_blit_outside_the_target_is_clipped() {
        let mut src = surface(2, 2, XRGB);
        src.write_pixel(1, 1, 5);
        let mut dst = surface(2, 2, XRGB);
        // keyed so the row-copy path is not taken; negative top-left and an overhang
        blt(&mut dst, Rect::new(-1, -1, 3, 3), &src, src.whole(), true, 0xffffff);
        assert_eq!(dst.read_pixel(1, 1), 5);
    }

    fn pattern_surface(width: u32, height: u32, format: GxPixelFormat) -> Surface {
        let mut s = surface(width, height, format);
        for y in 0..height {
            for x in 0..width {
                s.write_pixel(x, y, (x * 37 + y * 101 + 11).wrapping_mul(0x0101_0101));
            }
        }
        s
    }

    fn golden_format(index: usize) -> GxPixelFormat {
        [XRGB, RGB565, ARGB1555][index]
    }

    fn hex(text: &str) -> Vec<u8> {
        (0..text.len() / 2).map(|i| u8::from_str_radix(&text[i * 2..i * 2 + 2], 16).unwrap()).collect()
    }

    /// golden_cpublt.txt: the C++ `CPUBlt` over formats, stretches, colour keys and clipping.
    #[test]
    fn golden_cpu_blt_from_the_cpp() {
        let mut cases = 0;
        for line in include_str!("golden_cpublt.txt").lines() {
            let f: Vec<&str> = line.split_whitespace().collect();
            if f.is_empty() {
                continue;
            }
            let n = |i: usize| f[i].parse::<i32>().unwrap();
            let mut dst = surface(n(2) as u32, n(3) as u32, golden_format(n(0) as usize));
            dst.memory.as_mut().unwrap().fill(0x5a);
            let src = pattern_surface(n(4) as u32, n(5) as u32, golden_format(n(1) as usize));
            let d = Rect::new(n(6), n(7), n(8), n(9));
            let sr = Rect::new(n(10), n(11), n(12), n(13));
            blt(&mut dst, d, &src, sr, n(14) != 0, u32::from_str_radix(f[15], 16).unwrap());
            assert_eq!(dst.memory().unwrap(), hex(f[16]).as_slice(), "{line}");
            cases += 1;
        }
        assert_eq!(cases, 11);
    }

    /// golden_upload.txt: the C++ `UploadLevel`'s texel conversion, colour-key baking included.
    #[test]
    fn golden_upload_conversion_from_the_cpp() {
        let mut cases = 0;
        for line in include_str!("golden_upload.txt").lines() {
            let f: Vec<&str> = line.split_whitespace().collect();
            if f.is_empty() {
                continue;
            }
            let s = pattern_surface(4, 3, golden_format(f[0].parse::<usize>().unwrap()));
            let keyed = f[2] == "1";
            let key = u32::from_str_radix(f[3], 16).unwrap();
            assert_eq!(s.upload_pixels(keyed, key), hex(f[4]), "{line}");
            cases += 1;
        }
        assert_eq!(cases, 6);
    }

    #[test]
    fn gpu_preferred_for_targets_and_stale_cpu_copies() {
        let mut s = Surface { cpu_valid: true, ..Default::default() };
        assert!(!s.gpu_preferred());
        s.cpu_valid = false;
        assert!(s.gpu_preferred());
        s.cpu_valid = true;
        s.is_render_target = true;
        assert!(s.gpu_preferred());
    }
}
