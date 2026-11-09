//! GX's logic behind one lock: the object table, the CPU/GPU bookkeeping of the surfaces, the blits, the pass and ring
//! tracking and presentation. Everything that talks to a GPU goes through [`Backend`], so all of this runs against a fake
//! in `cargo test`; the real backend is thin (`gpu.rs`).

use std::ffi::c_void;
use std::sync::atomic::{AtomicI64, AtomicU32, Ordering};
use std::sync::Arc;

use crate::abi::*;
use crate::format::Format;
use crate::keys::{self, BlitMode};
use crate::modes::{self, DisplayInfo};
use crate::stats::DrawStats;
use crate::surface::{cpu_blt, CpuView, Surface, SurfaceId};
use crate::uniform::{self, BlitUniform};

pub const VERTEX_RING_SIZE: u64 = 16 << 20;
pub const INDEX_RING_SIZE: u64 = 4 << 20;
pub const UNIFORM_RING_SIZE: u64 = 8 << 20;
pub const UNIFORM_BINDING: u64 = 2048;

const XRGB32: GxPixelFormat = GxPixelFormat::rgb(32, 0x00ff_0000, 0x0000_ff00, 0x0000_00ff);
const ARGB32: GxPixelFormat = GxPixelFormat::rgba(32, 0x00ff_0000, 0x0000_ff00, 0x0000_00ff, 0xff00_0000);
const RGB565: GxPixelFormat = GxPixelFormat::rgb(16, 0xf800, 0x07e0, 0x001f);

pub fn mode_format(bits: u32) -> GxPixelFormat {
    if bits == 16 {
        RGB565
    } else {
        XRGB32
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum RingKind {
    Vertex,
    Index,
    Uniform,
}

/// What a read-back answered.
#[derive(Debug, PartialEq, Eq)]
pub enum ReadBack {
    /// the rows, `width * 4` bytes each
    Pixels(Vec<u8>),
    Failed,
    TimedOut,
}

/// A blit's draw into the open pass (`DrawBlit`).
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct BlitDraw {
    pub format: BlitTarget,
    pub width: u32,
    pub height: u32,
    pub source: Option<SurfaceId>,
    pub sampler_key: u32,
    pub mode: BlitMode,
    pub depth: bool,
    pub uniform_offset: u32,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum BlitTarget {
    /// a surface's BGRA8Unorm texture
    Surface,
    /// the window's swap chain in its format
    Swap(wgpu::TextureFormat),
}

/// A 3D draw into the open pass.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct DrawCall {
    pub pipeline_key: u32,
    pub viewport: [f32; 6],
    pub scissor: (u32, u32),
    pub texture: Option<SurfaceId>,
    pub sampler_key: u32,
    pub uniform_offset: u32,
    pub vertex_offset: u64,
    pub vertex_bytes: u64,
    pub index: Option<(u64, u64, u32)>, // offset, bytes, count
    pub vertex_count: u32,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct SwapCaps {
    pub formats: Vec<wgpu::TextureFormat>,
    pub present_modes: Vec<wgpu::PresentMode>,
}

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct AdapterInfo {
    pub name: String,
    pub vendor_id: u32,
    pub device_id: u32,
    pub backend: u32,
    pub device_type: u32,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum StartError {
    NoInstance,
    NoAdapter,
    NoDevice,
}

/// What to bring the device up with (the `EE_WGPU_*` environment).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct StartConfig {
    pub backends: wgpu::Backends,
    pub flags: wgpu::InstanceFlags,
    pub dxc: bool,
    pub force_fallback: bool,
}

/// Everything GX needs of a GPU. The real one is `gpu::WgpuBackend`.
pub trait Backend {
    fn start(&mut self, config: &StartConfig) -> Result<AdapterInfo, StartError>;
    fn create_texture(&mut self, id: SurfaceId, width: u32, height: u32, mip_levels: u32, is_z: bool) -> bool;
    fn release_surface(&mut self, id: SurfaceId);
    fn write_ring(&mut self, ring: RingKind, offset: u64, data: &[u8]);
    fn upload_level(&mut self, top: SurfaceId, mip: u32, width: u32, height: u32, bgra: &[u8]);
    fn read_texture(&mut self, id: SurfaceId, width: u32, height: u32) -> ReadBack;
    fn begin_pass(&mut self, target: SurfaceId, depth: Option<SurfaceId>);
    fn end_pass(&mut self);
    fn submit(&mut self);
    fn draw_blit(&mut self, draw: &BlitDraw);
    fn draw(&mut self, draw: &DrawCall);
    fn swap_create(&mut self, primary: SurfaceId, native: &NativeWindow) -> bool;
    fn swap_caps(&mut self, primary: SurfaceId) -> SwapCaps;
    fn swap_configure(
        &mut self,
        primary: SurfaceId,
        format: wgpu::TextureFormat,
        width: u32,
        height: u32,
        mode: wgpu::PresentMode,
    );
    fn swap_acquire(&mut self, primary: SurfaceId) -> bool;
    fn swap_begin_pass(&mut self, primary: SurfaceId);
    fn swap_present(&mut self, primary: SurfaceId);
    /// The live GPU objects (`EE_GX_STATS`'s "GX live" line); a fake has none.
    fn live(&self) -> LiveCounts {
        LiveCounts::default()
    }
}

/// The GPU objects alive now: `cached_groups` are the bind groups GX holds (per texture and the white texture's), the
/// rest wgpu-hal's counters (raw objects not yet destroyed, including those kept by submissions in flight).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct LiveCounts {
    pub cached_groups: usize,
    pub bind_groups: isize,
    pub textures: isize,
    pub texture_views: isize,
    pub samplers: isize,
    pub buffers: isize,
    /// submissions the GPU has not finished
    pub in_flight: usize,
}

/// The platform layer and the environment.
pub trait Platform {
    fn env(&self, name: &str) -> Option<String>;
    fn log(&self, line: &str);
    fn client_size(&self, window: *mut c_void) -> (i32, i32);
    fn display_info(&self) -> DisplayInfo;
    fn native_window(&self, window: *mut c_void) -> NativeWindow;
    fn now_ms(&self) -> u64;
    fn now_us(&self) -> u64;
    fn sleep_us(&self, microseconds: u64);
    fn redirect_stderr(&self, path: &str);
    fn install_wgpu_log(&self, level: &str);
}

/// A present to run after the hook has seen the frame (the hook runs outside the state borrow, it may call back).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct PresentRequest {
    pub primary: SurfaceId,
    pub source: SurfaceId,
    pub wait: bool,
}

pub type PresentHook = extern "C" fn(*mut c_void, *mut c_void);
/// A `GXSurface::TextCallback`.
pub type TextCallback = extern "C" fn(*const PlatFontTarget, std::ffi::c_long, std::ffi::c_long, *mut c_void) -> bool;

/// A `Blt` or `Flip` result: what to tell the caller, and a present left for the caller to run.
pub struct Outcome {
    pub result: GxResult,
    pub present: Option<PresentRequest>,
}

/// DrawText in progress.
pub enum TextJob {
    /// the GX call is over: this is its result
    Done(GxResult),
    /// the callback draws into `bits`
    Pending(PendingText),
}

/// The state DrawText keeps across the text callback.
pub struct PendingText {
    pub target: SurfaceId,
    pub bounds: Rect,
    pub bits: Vec<u8>,
    pub before: Vec<u8>, // CPU path: the bounds' pixels before the callback
    pub width: u32,
    pub height: u32,
    pub pitch: i32,
    pub flags: u32,
    pub scale: f32,
    pub gpu: bool,
}

impl PendingText {
    /// The drawing target of the callback.
    pub fn font_target(&mut self) -> PlatFontTarget {
        PlatFontTarget {
            mPixels: self.bits.as_mut_ptr() as *mut c_void,
            mPitch: self.pitch,
            mWidth: self.width as i32,
            mHeight: self.height as i32,
            mFlags: self.flags,
            mScale: self.scale,
        }
    }
}

/// The display settings (`GXSetRenderPercent` and the others): the game's settings code writes them, without GX's lock.
#[derive(Debug)]
pub struct DisplaySettings {
    render_percent: AtomicU32,
    frame_limit: AtomicI64,
    texture_filtering: AtomicU32,
    brightness_bits: AtomicU32,
}

impl Default for DisplaySettings {
    fn default() -> Self {
        DisplaySettings {
            render_percent: AtomicU32::new(100),
            frame_limit: AtomicI64::new(0),
            texture_filtering: AtomicU32::new(0),
            brightness_bits: AtomicU32::new(1.0f32.to_bits()),
        }
    }
}

impl DisplaySettings {
    pub fn set_render_percent(&self, percent: u32) {
        self.render_percent.store(percent.clamp(50, 100), Ordering::Relaxed);
    }

    pub fn render_percent(&self) -> u32 {
        self.render_percent.load(Ordering::Relaxed)
    }

    pub fn set_frame_limit(&self, frames_per_second: i64) {
        self.frame_limit.store(frames_per_second, Ordering::Relaxed);
    }

    pub fn frame_limit(&self) -> i64 {
        self.frame_limit.load(Ordering::Relaxed)
    }

    pub fn set_texture_filtering(&self, mode: u32) {
        self.texture_filtering.store(mode.min(2), Ordering::Relaxed);
    }

    pub fn texture_filtering(&self) -> u32 {
        self.texture_filtering.load(Ordering::Relaxed)
    }

    pub fn set_brightness(&self, gamma: f32) {
        let gamma = if gamma > 0.25 && gamma < 4.0 { gamma } else { 1.0 };
        self.brightness_bits.store(gamma.to_bits(), Ordering::Relaxed);
    }

    pub fn brightness(&self) -> f32 {
        f32::from_bits(self.brightness_bits.load(Ordering::Relaxed))
    }
}

/// What a `GXSurface*` points at: the slot of the surface in [`Gx`].
pub struct SurfaceHandle {
    pub id: SurfaceId,
}

/// The surface in slot `id` of the renderer's table, as C++ names it.
///
/// # Safety
/// `handle` must be a live `GXSurface*` from this crate.
pub unsafe fn surface_of_handle(handle: *mut c_void) -> SurfaceId {
    (*(handle as *const SurfaceHandle)).id
}

pub struct Gx<B: Backend, P: Platform> {
    pub backend: B,
    pub plat: P,
    surfaces: Vec<Option<Surface>>,
    free: Vec<SurfaceId>,
    // the device
    started: bool,
    failed: bool,
    pub adapter: AdapterInfo,
    // the command stream
    pub pass_open: bool,
    pub pass_target: Option<SurfaceId>,
    pub pass_depth: Option<SurfaceId>,
    pub serial: u64,
    pub ring_offsets: [u64; 3],
    // the display settings
    pub settings: Arc<DisplaySettings>,
    // diagnostics and caches of the environment
    pub stats: DrawStats,
    pub stats_every: i64,
    pub null3d_ms: i64,
    pub draw_scene: bool,
    pub next_key_scene: u64,
    render_scale_override: Option<Option<String>>,
    check_present: Option<bool>,
    check_frame: u32,
    last_present_size: (u32, u32),
    next_frame_us: u64,
    pub hook: Option<PresentHook>,
}

fn ring_index(ring: RingKind) -> usize {
    match ring {
        RingKind::Vertex => 0,
        RingKind::Index => 1,
        RingKind::Uniform => 2,
    }
}

impl<B: Backend, P: Platform> Gx<B, P> {
    pub fn new(backend: B, plat: P) -> Self {
        Self::with_settings(backend, plat, Arc::default())
    }

    /// A GX sharing `settings`, which are set without its lock (the C++'s setters took none).
    pub fn with_settings(backend: B, plat: P, settings: Arc<DisplaySettings>) -> Self {
        Gx {
            backend,
            plat,
            surfaces: Vec::new(),
            free: Vec::new(),
            started: false,
            failed: false,
            adapter: AdapterInfo::default(),
            pass_open: false,
            pass_target: None,
            pass_depth: None,
            serial: 1,
            ring_offsets: [0; 3],
            settings,
            stats: DrawStats::default(),
            stats_every: -2,
            null3d_ms: -2,
            draw_scene: true,
            next_key_scene: 0,
            render_scale_override: None,
            check_present: None,
            check_frame: 0,
            last_present_size: (0, 0),
            next_frame_us: 0,
            hook: None,
        }
    }

    pub fn log(&self, line: &str) {
        self.plat.log(line);
    }

    // ---- the object table

    pub fn add_surface(&mut self, surface: Surface) -> SurfaceId {
        let id = if let Some(id) = self.free.pop() {
            self.surfaces[id] = Some(surface);
            id
        } else {
            self.surfaces.push(Some(surface));
            self.surfaces.len() - 1
        };
        // the C++ `GXSurface*`: a small allocation naming the slot, freed with the surface
        self.m(id).handle = Box::into_raw(Box::new(SurfaceHandle { id })) as *mut c_void;
        id
    }

    fn remove_surface(&mut self, id: SurfaceId) {
        let handle = self.s(id).handle as *mut SurfaceHandle;
        // SAFETY: made by Box::into_raw in add_surface and only freed here
        drop(unsafe { Box::from_raw(handle) });
        self.backend.release_surface(id);
        self.surfaces[id] = None;
        self.free.push(id);
    }

    pub fn s(&self, id: SurfaceId) -> &Surface {
        self.surfaces[id].as_ref().expect("a live surface")
    }

    pub fn m(&mut self, id: SurfaceId) -> &mut Surface {
        self.surfaces[id].as_mut().expect("a live surface")
    }

    /// `Top`: a mip level's chain top (itself otherwise).
    pub fn top(&self, id: SurfaceId) -> SurfaceId {
        self.s(id).mip_top.unwrap_or(id)
    }

    pub fn live_surfaces(&self) -> usize {
        self.surfaces.iter().filter(|s| s.is_some()).count()
    }

    pub fn add_ref(&mut self, id: SurfaceId) -> u32 {
        let surface = self.m(id);
        surface.references += 1;
        surface.references
    }

    /// `GXSurface::Release`; returns the references left (0: destroyed).
    pub fn release(&mut self, id: SurfaceId) -> u32 {
        let surface = self.m(id);
        surface.references -= 1;
        let count = surface.references;
        if count == 0 {
            self.destroy(id);
        }
        count
    }

    fn destroy(&mut self, id: SurfaceId) {
        if self.pass_target == Some(id) || self.pass_depth == Some(id) {
            self.end_pass();
        }
        let attached = std::mem::take(&mut self.m(id).attached);
        let (last_frame, window_image) = {
            let s = self.m(id);
            (s.last_frame.take(), s.window_image.take())
        };
        for other in attached {
            self.release(other);
        }
        if let Some(last_frame) = last_frame {
            self.release(last_frame);
        }
        if let Some(window_image) = window_image {
            self.release(window_image);
        }
        self.remove_surface(id);
    }

    // ---- the core

    /// `GXCore::Start`.
    pub fn start(&mut self) -> bool {
        if self.started {
            return true;
        }
        if self.failed {
            return false;
        }
        self.failed = true;
        if let Some(path) = self.plat.env("EE_GX_STDERR") {
            self.plat.redirect_stderr(&path);
        }
        if let Some(level) = self.plat.env("EE_GX_LOG") {
            self.plat.install_wgpu_log(&level);
        }
        let mut backends = wgpu::Backends::DX12 | wgpu::Backends::VULKAN | wgpu::Backends::METAL;
        let mut flags = wgpu::InstanceFlags::empty();
        if let Some(text) = self.plat.env("EE_WGPU_FLAGS") {
            let bits = crate::cformat::strtoul_0(&text);
            flags = instance_flags(bits);
        }
        let dxc = self.plat.env("EE_WGPU_DX12_COMPILER").is_some_and(|c| c.eq_ignore_ascii_case("dxc"));
        if let Some(backend) = self.plat.env("EE_WGPU_BACKEND") {
            if backend.eq_ignore_ascii_case("dx12") {
                backends = wgpu::Backends::DX12;
            } else if backend.eq_ignore_ascii_case("vulkan") {
                backends = wgpu::Backends::VULKAN;
            } else if backend.eq_ignore_ascii_case("gl") {
                backends = wgpu::Backends::GL;
            } else if backend.eq_ignore_ascii_case("metal") {
                backends = wgpu::Backends::METAL;
            }
        }
        let force_fallback = self.plat.env("EE_WGPU_FALLBACK").is_some();
        match self.backend.start(&StartConfig { backends, flags, dxc, force_fallback }) {
            Ok(info) => {
                self.log(&format!("GX: adapter {} (backend {}, type {})", info.name, info.backend, info.device_type));
                self.adapter = info;
                self.started = true;
                self.failed = false;
                true
            }
            Err(StartError::NoInstance) => {
                self.log("GX: no wgpu instance");
                false
            }
            Err(StartError::NoAdapter) => {
                self.log("GX: no adapter");
                false
            }
            Err(StartError::NoDevice) => {
                self.log("GX: no device");
                false
            }
        }
    }

    pub fn end_pass(&mut self) {
        if self.pass_open {
            self.backend.end_pass();
            self.pass_open = false;
        }
        self.pass_target = None;
        self.pass_depth = None;
    }

    pub fn begin_pass(&mut self, target: SurfaceId, depth: Option<SurfaceId>) {
        if self.pass_open && self.pass_target == Some(target) && self.pass_depth == depth {
            return;
        }
        self.end_pass();
        self.backend.begin_pass(target, depth);
        self.pass_open = true;
        self.pass_target = Some(target);
        self.pass_depth = depth;
        let serial = self.serial;
        if let Some(depth) = depth {
            self.m(depth).used_serial = serial;
        }
        self.m(target).used_serial = serial;
    }

    pub fn submit(&mut self) {
        self.end_pass();
        self.backend.submit();
        self.ring_offsets = [0; 3];
        self.serial += 1;
    }

    /// `GXCore::Ring`: copies `data` into a ring (aligned), submitting first when it is full; returns the offset.
    pub fn ring(&mut self, ring: RingKind, size: u64, data: &[u8], align: u64) -> u64 {
        let index = ring_index(ring);
        let mut offset = self.ring_offsets[index].div_ceil(align) * align;
        let padded = (data.len() as u64 + 3) & !3;
        if offset + padded > size {
            self.submit();
            offset = 0;
        }
        if padded == data.len() as u64 {
            self.backend.write_ring(ring, offset, data);
        } else {
            let mut copy = vec![0u8; padded as usize];
            copy[..data.len()].copy_from_slice(data);
            self.backend.write_ring(ring, offset, &copy);
        }
        self.ring_offsets[index] = offset + padded;
        offset
    }

    pub fn ring_room(&self, ring: RingKind) -> u64 {
        self.ring_offsets[ring_index(ring)]
    }

    pub fn sampler_key(&self, address: u32, min: u32, mag: u32, mip: u32) -> u32 {
        keys::sampler_key(address, min, mag, mip, self.settings.texture_filtering())
    }

    // ---- the display settings

    pub fn set_render_percent(&mut self, percent: u32) {
        self.settings.set_render_percent(percent);
    }

    pub fn set_frame_limit(&mut self, frames_per_second: i64) {
        self.settings.set_frame_limit(frames_per_second);
    }

    pub fn set_texture_filtering(&mut self, mode: u32) {
        self.settings.set_texture_filtering(mode);
    }

    pub fn set_brightness(&mut self, gamma: f32) {
        self.settings.set_brightness(gamma);
    }

    /// `GXRenderScale`.
    pub fn render_scale(&mut self, mode_height: u32) -> f32 {
        if self.render_scale_override.is_none() {
            self.render_scale_override = Some(self.plat.env("EE_GX_RENDER_SCALE"));
        }
        let info = self.plat.display_info();
        let display_height = info.pixels.map(|p| p.1);
        modes::render_scale(
            mode_height,
            display_height,
            self.settings.render_percent(),
            self.render_scale_override.as_ref().unwrap().as_deref(),
        )
    }

    // ---- CPU/GPU bookkeeping

    pub fn create_texture(&mut self, id: SurfaceId) -> bool {
        let top = self.top(id);
        let surface = self.s(top);
        if surface.has_texture {
            return true;
        }
        if surface.is_primary {
            return false;
        }
        let (width, height) = (surface.tex_width().max(1), surface.tex_height().max(1));
        let (mips, is_z) = (surface.mip_levels, surface.is_z);
        if !self.backend.create_texture(top, width, height, mips, is_z) {
            return false;
        }
        self.m(top).has_texture = true;
        true
    }

    fn upload_level(&mut self, top: SurfaceId, level: SurfaceId) {
        if self.s(level).memory.is_none() || !self.s(top).has_texture {
            return;
        }
        if self.s(top).used_serial == self.serial {
            self.submit(); // the open encoder used the old pixels: keep them for it
        }
        let keyed = self.s(top).is_texture && self.s(top).has_key;
        let key = self.s(top).key;
        let (pixels, mip, width, height) = {
            let surface = self.s(level);
            (surface.upload_pixels(keyed, key), surface.mip_level, surface.width, surface.height)
        };
        self.backend.upload_level(top, mip, width, height, &pixels);
        self.m(level).gpu_valid = true;
    }

    fn upload(&mut self, id: SurfaceId) {
        let top = self.top(id);
        if !self.create_texture(top) {
            return;
        }
        if self.s(top).scale != 1.0 {
            if !self.s(top).gpu_valid {
                self.upload_scaled(top);
            }
            self.m(top).gpu_valid = true;
            return;
        }
        if !self.s(top).gpu_valid {
            self.upload_level(top, top);
        }
        if self.s(top).levels_dirty {
            let mut level = Some(top);
            while let Some(current) = level {
                if !self.s(current).gpu_valid {
                    self.upload_level(top, current);
                }
                let attached = self.s(current).attached.clone();
                let next = attached.into_iter().rfind(|&a| self.s(a).mip_top == Some(top));
                level = next;
            }
            self.m(top).levels_dirty = false;
        }
        self.m(top).gpu_valid = true;
    }

    fn download(&mut self, id: SurfaceId) {
        if self.s(id).cpu_valid {
            return;
        }
        if !self.m(id).allocate_memory() || !self.s(id).has_texture {
            self.m(id).cpu_valid = true;
            return;
        }
        if self.s(id).scale != 1.0 {
            self.download_scaled(id);
            self.m(id).cpu_valid = true;
            return;
        }
        self.submit();
        let (width, height) = (self.s(id).width, self.s(id).height);
        match self.backend.read_texture(id, width, height) {
            ReadBack::Pixels(rows) => self.m(id).set_from_bgra_rows(&rows),
            ReadBack::TimedOut => {
                self.log(&format!("GX: read-back of {width}x{height} timed out"));
                self.log("GX: read-back failed");
            }
            ReadBack::Failed => self.log("GX: read-back failed"),
        }
        self.m(id).cpu_valid = true;
    }

    pub fn for_gpu_read(&mut self, id: SurfaceId) {
        let top = self.top(id);
        let surface = self.s(top);
        if !surface.gpu_valid || surface.levels_dirty || !surface.has_texture {
            self.upload(top);
        }
    }

    pub fn for_gpu_write(&mut self, id: SurfaceId) {
        self.for_gpu_read(id);
        self.m(id).cpu_valid = false;
    }

    pub fn for_cpu_read(&mut self, id: SurfaceId) {
        if !self.s(id).cpu_valid {
            self.download(id);
        }
        self.m(id).allocate_memory();
    }

    pub fn for_cpu_write(&mut self, id: SurfaceId) {
        self.for_cpu_read(id);
        self.m(id).gpu_valid = false;
        if let Some(top) = self.s(id).mip_top {
            self.m(top).levels_dirty = true;
        }
    }

    // ---- the blits

    /// `BlitUniform`, returning the uniform's ring offset.
    #[allow(clippy::too_many_arguments)]
    fn blit_uniform(
        &mut self,
        width: u32,
        height: u32,
        destination: &Rect,
        source: Option<SurfaceId>,
        source_rect: Option<&Rect>,
        mode: BlitMode,
        color: Option<[f32; 4]>,
        depth: f32,
        opaque: bool,
    ) -> u32 {
        let mut u = BlitUniform { destination: uniform::to_ndc(destination, width, height), ..Default::default() };
        match (source, source_rect) {
            (Some(s), Some(rect)) => {
                let (w, h) = (self.s(s).width as f32, self.s(s).height as f32);
                u.source = [rect.left as f32 / w, rect.top as f32 / h, rect.right as f32 / w, rect.bottom as f32 / h];
            }
            _ => {
                u.source[2] = 1.0;
                u.source[3] = 1.0;
            }
        }
        if mode == BlitMode::Keyed {
            if let Some(s) = source {
                let surface = self.s(s);
                let key = surface.format.to_bgra(surface.key);
                u.key[0] = f32::from(key[2]) / 255.0;
                u.key[1] = f32::from(key[1]) / 255.0;
                u.key[2] = f32::from(key[0]) / 255.0;
            }
        }
        if let Some(c) = color {
            u.color = c;
        }
        u.mode[0] = match mode {
            BlitMode::Keyed => 1,
            BlitMode::Fill | BlitMode::Depth => 2,
            _ => 0,
        };
        u.misc[0] = depth;
        u.mode[1] = u32::from(opaque);
        u.misc[1] = if opaque { self.settings.brightness() } else { 0.0 };
        self.ring(RingKind::Uniform, UNIFORM_RING_SIZE - UNIFORM_BINDING, uniform::as_bytes(&u), 256) as u32
    }

    /// `GXBlit`: a quad from a source texture (or a fill colour) into a target surface.
    #[allow(clippy::too_many_arguments)]
    pub fn blit(
        &mut self,
        target: SurfaceId,
        destination: &Rect,
        source: Option<SurfaceId>,
        source_rect: Option<&Rect>,
        mode: BlitMode,
        color: Option<[f32; 4]>,
        depth: f32,
        linear: bool,
    ) {
        if let Some(source) = source {
            self.for_gpu_read(source);
        }
        if mode == BlitMode::Depth {
            let Some(depth_surface) = self.s(target).depth else { return };
            self.create_texture(depth_surface);
        }
        self.for_gpu_write(target);
        if !self.create_texture(target) {
            return;
        }
        let (width, height) = (self.s(target).width, self.s(target).height);
        // the uniform first: a full ring submits, which would close the pass
        let offset = self.blit_uniform(width, height, destination, source, source_rect, mode, color, depth, false);
        let pass_depth = if mode == BlitMode::Depth {
            self.s(target).depth
        } else if self.pass_open && self.pass_target == Some(target) {
            self.pass_depth
        } else {
            None
        };
        self.begin_pass(target, pass_depth);
        let (tex_width, tex_height) = (self.s(target).tex_width(), self.s(target).tex_height());
        let linear = linear || self.s(target).scale != 1.0;
        let draw = self.blit_draw(BlitTarget::Surface, tex_width, tex_height, source, mode, linear, offset);
        self.backend.draw_blit(&draw);
        if let Some(source) = source {
            let top = self.top(source);
            let serial = self.serial;
            self.m(top).used_serial = serial;
        }
    }

    #[allow(clippy::too_many_arguments)]
    fn blit_draw(
        &self,
        format: BlitTarget,
        width: u32,
        height: u32,
        source: Option<SurfaceId>,
        mode: BlitMode,
        linear: bool,
        uniform_offset: u32,
    ) -> BlitDraw {
        let (min, mag) = if linear { (GXTFN_LINEAR, GXTFG_LINEAR) } else { (GXTFN_POINT, GXTFG_POINT) };
        BlitDraw {
            format,
            width,
            height,
            source: source.and_then(|s| self.sampled_texture(s)),
            sampler_key: self.sampler_key(GXTADDRESS_CLAMP, min, mag, GXTFP_NONE),
            mode,
            depth: self.pass_depth.is_some(),
            uniform_offset,
        }
    }

    /// `GXCore::Group`'s choice: a mip level samples its chain's texture; a surface without one (primary, Z) samples
    /// white.
    pub fn sampled_texture(&self, id: SurfaceId) -> Option<SurfaceId> {
        let top = self.top(id);
        if self.s(top).has_texture {
            Some(top)
        } else {
            self.log(&format!(
                "GX: sampling a surface without a texture ({}x{})",
                self.s(top).width,
                self.s(top).height
            ));
            None
        }
    }

    fn new_temporary(&mut self, width: u32, height: u32, like: SurfaceId) -> SurfaceId {
        let like = self.s(like);
        let temporary = Surface {
            width,
            height,
            pixel_format: like.pixel_format,
            format: like.format,
            is_render_target: true,
            ..Default::default()
        };
        self.add_surface(temporary)
    }

    fn free_temporary(&mut self, id: SurfaceId) {
        if self.pass_target == Some(id) {
            self.end_pass();
        }
        self.remove_surface(id);
    }

    /// `UploadScaled`: the CPU copy through a texture of its logical size stretched onto the GPU copy.
    fn upload_scaled(&mut self, id: SurfaceId) {
        if self.s(id).memory.is_none() {
            return;
        }
        let (width, height) = (self.s(id).width, self.s(id).height);
        let temporary = self.new_temporary(width, height, id);
        let (memory, pitch) = (self.m(id).memory.take(), self.s(id).pitch);
        self.m(temporary).memory = memory;
        self.m(temporary).pitch = pitch;
        self.m(temporary).gpu_valid = false;
        self.upload(temporary);
        self.m(id).memory = self.m(temporary).memory.take();
        // the blit must not upload the target again (its GPU copy is being made) nor mark its CPU copy stale (it is
        // the source of these pixels)
        self.m(id).gpu_valid = true;
        let all = Rect::whole(width, height);
        self.blit(id, &all, Some(temporary), Some(&all), BlitMode::Copy, None, 0.0, true);
        self.m(id).cpu_valid = true;
        self.free_temporary(temporary);
    }

    /// `DownloadScaled`: the GPU copy brought down to its logical size into the CPU copy.
    fn download_scaled(&mut self, id: SurfaceId) {
        let (width, height) = (self.s(id).width, self.s(id).height);
        let temporary = self.new_temporary(width, height, id);
        if !self.create_texture(temporary) {
            self.free_temporary(temporary);
            return;
        }
        self.m(temporary).cpu_valid = false;
        self.m(temporary).gpu_valid = true;
        let all = Rect::whole(width, height);
        self.blit(temporary, &all, Some(id), Some(&all), BlitMode::Copy, None, 0.0, true);
        self.download(temporary);
        let copied = {
            let temp = self.s(temporary);
            temp.memory().map(|m| (m.to_vec(), temp.pitch))
        };
        if let Some((rows, temp_pitch)) = copied {
            let surface = self.m(id);
            let pitch = surface.pitch;
            if let Some(memory) = surface.memory.as_deref_mut() {
                let bytes = pitch.min(temp_pitch) as usize;
                for y in 0..height as usize {
                    memory[y * pitch as usize..][..bytes].copy_from_slice(&rows[y * temp_pitch as usize..][..bytes]);
                }
            }
        }
        self.free_temporary(temporary);
    }

    // ---- presentation

    pub fn set_test_present_hook(&mut self, hook: Option<PresentHook>) {
        self.hook = hook;
    }

    /// `GXTestReadFrame`.
    pub fn test_read_frame(&mut self, frame: SurfaceId, out: Option<&mut [u8]>, capacity: u32) -> (i32, u32, u32) {
        let (width, height) = (self.s(frame).width, self.s(frame).height);
        let out = match out {
            Some(out) if capacity >= width.wrapping_mul(height).wrapping_mul(4) => out,
            _ => return (1, width, height),
        };
        self.for_cpu_read(frame); // a read-back; the GPU copy stays valid
        if self.s(frame).memory.is_none() {
            return (1, width, height);
        }
        for y in 0..height {
            self.s(frame).bgra_row(y, &mut out[y as usize * width as usize * 4..][..width as usize * 4]);
        }
        (0, width, height)
    }

    /// A present of `source` on the window of `primary` would run if it has a window; the caller runs the hook.
    pub fn request_present(&self, primary: SurfaceId, source: SurfaceId, wait: bool) -> Option<PresentRequest> {
        if self.s(primary).window.is_null() {
            return None;
        }
        Some(PresentRequest { primary, source, wait })
    }

    /// What the hook needs: `(window, frame)` of a present.
    pub fn hook_arguments(&self, request: &PresentRequest) -> Option<(PresentHook, *mut c_void, *mut c_void)> {
        let hook = self.hook?;
        Some((hook, self.s(request.primary).native.window, self.s(request.source).handle))
    }

    /// `GXSurfaceData::Present` after the hook.
    pub fn present(&mut self, request: &PresentRequest) {
        let PresentRequest { primary, source, wait } = *request;
        let (client_width, client_height) = self.plat.client_size(self.s(primary).window);
        if client_width <= 0 || client_height <= 0 {
            return;
        }
        let (source_width, source_height) = (self.s(source).width, self.s(source).height);
        if (source_width, source_height) != self.last_present_size {
            self.log(&format!(
                "GX: presenting {source_width}x{source_height} frames into a {client_width}x{client_height} window"
            ));
            self.last_present_size = (source_width, source_height);
        }
        self.for_gpu_read(source);
        if self.check_present.is_none() {
            self.check_present = Some(self.plat.env("EE_GX_CHECK_PRESENT").is_some());
        }
        if self.check_present == Some(true) && self.s(source).cpu_valid && self.s(source).memory.is_some() {
            self.check_present_copies(source);
        }
        if !self.s(primary).swap_created {
            let native = self.s(primary).native;
            if !self.backend.swap_create(primary, &native) {
                return;
            }
            self.m(primary).swap_created = true;
        }
        let mut mode = wgpu::PresentMode::Fifo;
        if !wait || self.settings.frame_limit() != 0 || self.plat.env("EE_GX_NOVSYNC").is_some() {
            // port: the frame rate setting (not v-sync)
            let caps = self.backend.swap_caps(primary);
            mode = select_present_mode(&caps);
            if self.s(primary).swap_format.is_none() && !caps.formats.is_empty() {
                self.m(primary).swap_format = Some(caps.formats[0]);
            }
        }
        if self.s(primary).swap_format.is_none() {
            let caps = self.backend.swap_caps(primary);
            self.m(primary).swap_format = Some(select_swap_format(&caps));
        }
        let (width, height) = (client_width as u32, client_height as u32);
        if width != self.s(primary).swap_width
            || height != self.s(primary).swap_height
            || mode != self.s(primary).swap_mode
        {
            let format = self.s(primary).swap_format.unwrap();
            self.backend.swap_configure(primary, format, width, height, mode);
            let surface = self.m(primary);
            surface.swap_width = width;
            surface.swap_height = height;
            surface.swap_mode = mode;
        }
        if !self.backend.swap_acquire(primary) {
            self.m(primary).swap_width = 0; // reconfigure next time
            return;
        }
        let (swap_width, swap_height, format) =
            (self.s(primary).swap_width, self.s(primary).swap_height, self.s(primary).swap_format.unwrap());
        let all = Rect::whole(swap_width, swap_height);
        let linear = swap_width != self.s(source).tex_width() || swap_height != self.s(source).tex_height();
        let offset =
            self.blit_uniform(swap_width, swap_height, &all, Some(source), None, BlitMode::Copy, None, 0.0, true);
        self.end_pass();
        self.backend.swap_begin_pass(primary);
        self.pass_open = true;
        let draw = self.blit_draw(
            BlitTarget::Swap(format),
            swap_width,
            swap_height,
            Some(source),
            BlitMode::Copy,
            linear,
            offset,
        );
        self.backend.draw_blit(&draw);
        let top = self.top(source);
        let serial = self.serial;
        self.m(top).used_serial = serial;
        self.submit();
        self.backend.swap_present(primary);
        if self.settings.frame_limit() > 0 {
            self.wait_for_frame_limit();
        }
    }

    /// At most `frame_limit` frames a second (the std::chrono code of `Present`).
    fn wait_for_frame_limit(&mut self) {
        let period = 1_000_000 / self.settings.frame_limit() as u64;
        let now = self.plat.now_us();
        if self.next_frame_us > now && self.next_frame_us - now < period * 2 {
            self.plat.sleep_us(self.next_frame_us - now);
        } else {
            self.next_frame_us = now;
        }
        self.next_frame_us += period;
    }

    /// `EE_GX_CHECK_PRESENT`: the GPU copy against the CPU copy of a frame about to be presented.
    fn check_present_copies(&mut self, source: SurfaceId) {
        let (width, height, bytes, pitch) = {
            let s = self.s(source);
            (s.width, s.height, (s.format.bits / 8) as usize, s.pitch as usize)
        };
        let cpu = self.s(source).memory().unwrap().to_vec();
        self.m(source).cpu_valid = false;
        self.download(source);
        let (mut count, mut left, mut top, mut right, mut bottom) = (0u32, 1i64 << 30, 1i64 << 30, -1i64, -1i64);
        {
            let memory = self.s(source).memory().unwrap();
            for y in 0..height as usize {
                for x in 0..width as usize {
                    let i = y * pitch + x * bytes;
                    if cpu[i..i + bytes] != memory[i..i + bytes] {
                        count += 1;
                        left = left.min(x as i64);
                        top = top.min(y as i64);
                        right = right.max(x as i64);
                        bottom = bottom.max(y as i64);
                    }
                }
            }
        }
        self.m(source).memory = Some(cpu);
        let frame = self.check_frame;
        self.check_frame = frame.wrapping_add(1);
        if count != 0 || frame.is_multiple_of(200) {
            self.log(&format!(
                "GX: present check: {count} pixels of {width}x{height} differ GPU vs CPU (bounds {left},{top}-{right},{bottom})"
            ));
        }
    }

    // ---- GXSurface

    pub fn add_attached(&mut self, id: SurfaceId, other: SurfaceId) -> GxResult {
        self.add_ref(other);
        self.m(id).attached.push(other);
        if self.s(other).is_z {
            if self.pass_target == Some(id) {
                self.end_pass();
            }
            self.m(id).depth = Some(other);
        }
        GX_OK
    }

    pub fn delete_attached(&mut self, id: SurfaceId, other: SurfaceId) -> GxResult {
        let Some(position) = self.s(id).attached.iter().position(|&a| a == other) else {
            return GXERR_NOTFOUND;
        };
        self.m(id).attached.remove(position);
        if self.s(id).depth == Some(other) {
            if self.pass_target == Some(id) {
                self.end_pass();
            }
            self.m(id).depth = None;
        }
        self.release(other);
        GX_OK
    }

    /// `GetAttachedSurface`: the attachment matching the caps, or `GXERR_NOTFOUND`; the caller gets a reference.
    pub fn get_attached(&mut self, id: SurfaceId, caps: u32) -> Option<SurfaceId> {
        for index in 0..self.s(id).attached.len() {
            let other = self.s(id).attached[index];
            let surface = self.s(other);
            let matched = if caps & GXSCAPS_ZBUFFER != 0 {
                surface.is_z
            } else if caps & GXSCAPS_MIPMAP != 0 {
                surface.mip_top.is_some()
            } else if caps & GXSCAPS_FLIP != 0 {
                !surface.is_z && surface.mip_top.is_none()
            } else {
                false
            };
            if matched {
                self.add_ref(other);
                return Some(other);
            }
        }
        None
    }

    /// `GXSurface::Blt`.
    pub fn blt(
        &mut self,
        target: SurfaceId,
        destination: Option<Rect>,
        source: Option<SurfaceId>,
        source_rect: Option<Rect>,
        flags: u32,
        fill: u32,
    ) -> Outcome {
        let done = |result| Outcome { result, present: None };
        if self.s(target).is_z {
            return done(GXERR_UNSUPPORTED);
        }
        if flags & GXBLT_COLORFILL != 0 {
            if self.s(target).is_primary {
                return done(GX_OK);
            }
            let rect = self.s(target).clip(destination.unwrap_or_else(|| self.s(target).whole()));
            if rect.right <= rect.left || rect.bottom <= rect.top {
                return done(GX_OK);
            }
            if !self.s(target).gpu_preferred() {
                self.for_cpu_write(target);
                let surface = self.m(target);
                for y in rect.top..rect.bottom {
                    for x in rect.left..rect.right {
                        surface.write_pixel(x as u32, y as u32, fill);
                    }
                }
            } else {
                let bgra = self.s(target).format.to_bgra(fill);
                let color = [
                    f32::from(bgra[2]) / 255.0,
                    f32::from(bgra[1]) / 255.0,
                    f32::from(bgra[0]) / 255.0,
                    f32::from(bgra[3]) / 255.0,
                ];
                self.blit(target, &rect, None, None, BlitMode::Fill, Some(color), 0.0, false);
            }
            return done(GX_OK);
        }
        let Some(source) = source else {
            return done(GXERR_INVALIDPARAMS);
        };
        if self.s(target).is_primary {
            let keyed = flags & GXBLT_KEYSRC != 0 && self.s(source).has_key;
            return self.primary_blt(target, destination, source, source_rect, keyed);
        }
        let source_rect = source_rect.unwrap_or_else(|| self.s(source).whole());
        let destination = destination.unwrap_or_else(|| self.s(target).whole());
        let keyed = flags & GXBLT_KEYSRC != 0 && self.s(source).has_key;
        if !self.s(target).gpu_preferred()
            && self.s(source).cpu_valid
            && self.s(source).memory.is_some()
            && source != target
        {
            self.for_cpu_write(target);
            self.cpu_blt_between(target, destination, source, source_rect, keyed);
        } else if source == target {
            // overlapping copy inside one surface: through the CPU
            self.for_cpu_write(source);
            self.cpu_blt_self(target, destination, source_rect, keyed);
        } else {
            let mode = if keyed { BlitMode::Keyed } else { BlitMode::Copy };
            self.blit(target, &destination, Some(source), Some(&source_rect), mode, None, 0.0, false);
        }
        done(GX_OK)
    }

    fn cpu_blt_between(
        &mut self,
        target: SurfaceId,
        destination: Rect,
        source: SurfaceId,
        source_rect: Rect,
        keyed: bool,
    ) {
        let mut memory = self.m(target).memory.take().expect("for_cpu_write allocated it");
        {
            let s = self.s(source);
            let t = self.s(target);
            let view = CpuView {
                memory: s.memory().expect("the source's CPU copy"),
                pitch: s.pitch,
                width: s.width,
                height: s.height,
                format: &s.format,
                key: s.key,
            };
            cpu_blt(&mut memory, t.pitch, t.width, t.height, &t.format, destination, &view, source_rect, keyed, false);
        }
        self.m(target).memory = Some(memory);
    }

    fn cpu_blt_self(&mut self, target: SurfaceId, destination: Rect, source_rect: Rect, keyed: bool) {
        let mut memory = self.m(target).memory.take().expect("for_cpu_write allocated it");
        let copy = memory.clone();
        {
            let t = self.s(target);
            let format = t.format;
            let view = CpuView {
                memory: &copy,
                pitch: t.pitch,
                width: t.width,
                height: t.height,
                format: &format,
                key: t.key,
            };
            cpu_blt(&mut memory, t.pitch, t.width, t.height, &t.format, destination, &view, source_rect, keyed, true);
        }
        self.m(target).memory = Some(memory);
    }

    /// `PrimaryBlt`: a blit to the window (the primary surface). A whole frame is presented as it is and remembered;
    /// anything smaller goes onto a copy of the last frame at its place, and that copy is presented.
    fn primary_blt(
        &mut self,
        primary: SurfaceId,
        destination: Option<Rect>,
        source: SurfaceId,
        source_rect: Option<Rect>,
        keyed: bool,
    ) -> Outcome {
        let done = |result| Outcome { result, present: None };
        let source_rect = source_rect.unwrap_or_else(|| self.s(source).whole());
        let (width, height) = (source_rect.right - source_rect.left, source_rect.bottom - source_rect.top);
        let (client_width, client_height) =
            if self.s(primary).window.is_null() { (0, 0) } else { self.plat.client_size(self.s(primary).window) };
        let frame = source_rect.left == 0
            && source_rect.top == 0
            && width == self.s(source).width as i32
            && height == self.s(source).height as i32
            && width * 2 >= client_width
            && height * 2 >= client_height;
        if frame {
            if let Some(destination) = destination {
                let surface = self.m(primary);
                surface.origin_x = destination.left;
                surface.origin_y = destination.top;
            }
            self.add_ref(source);
            if let Some(last) = self.m(primary).last_frame.take() {
                self.release(last);
            }
            let surface = self.m(primary);
            surface.last_frame = Some(source);
            surface.window_image_current = false;
            return Outcome { result: GX_OK, present: self.request_present(primary, source, true) };
        }
        let Some(last) = self.s(primary).last_frame else {
            return done(GX_OK); // nothing shown yet to draw into
        };
        if width <= 0 || height <= 0 {
            return done(GX_OK);
        }
        let image_matches = self.s(primary).window_image.is_some_and(|image| {
            let (image, last) = (self.s(image), self.s(last));
            image.width == last.width && image.height == last.height && image.scale == last.scale
        });
        if !image_matches {
            if let Some(image) = self.m(primary).window_image.take() {
                self.release(image);
            }
            let (w, h, scale) = (self.s(last).width, self.s(last).height, self.s(last).scale);
            let image = self.add_surface(Surface {
                width: w,
                height: h,
                scale,
                pixel_format: ARGB32,
                format: Format::set(&ARGB32),
                is_render_target: true,
                ..Default::default()
            });
            if !self.create_texture(image) {
                self.release(image);
                return done(GX_OK);
            }
            let surface = self.m(image);
            surface.cpu_valid = false;
            surface.gpu_valid = true;
            let surface = self.m(primary);
            surface.window_image = Some(image);
            surface.window_image_current = false;
        }
        let image = self.s(primary).window_image.expect("just made");
        if !self.s(primary).window_image_current {
            let all = self.s(last).whole();
            let image_rect = self.s(image).whole();
            self.blit(image, &image_rect, Some(last), Some(&all), BlitMode::Copy, None, 0.0, false);
            self.m(primary).window_image_current = true;
        }
        let mut placed = destination.unwrap_or_else(|| self.s(image).whole());
        if destination.is_some() {
            // screen coordinates -> the frame's
            let (x, y) = (self.s(primary).origin_x, self.s(primary).origin_y);
            placed.left -= x;
            placed.right -= x;
            placed.top -= y;
            placed.bottom -= y;
        }
        if source != image {
            let mode = if keyed { BlitMode::Keyed } else { BlitMode::Copy };
            self.blit(image, &placed, Some(source), Some(&source_rect), mode, None, 0.0, false);
        }
        Outcome { result: GX_OK, present: self.request_present(primary, image, true) }
    }

    /// `GXSurface::Flip`.
    pub fn flip(&mut self, primary: SurfaceId, flags: u32) -> Outcome {
        let mut back = None;
        for &attached in &self.s(primary).attached {
            if !self.s(attached).is_z {
                back = Some(attached);
            }
        }
        let present = back.and_then(|back| self.request_present(primary, back, flags & GXFLIP_NOVSYNC == 0));
        Outcome { result: GX_OK, present }
    }

    /// `GXSurface::Lock`: the first pixel of `rect` in the surface's memory.
    pub fn lock(&mut self, id: SurfaceId, rect: Option<Rect>, flags: u32) -> Result<*mut u8, GxResult> {
        let surface = self.s(id);
        if surface.is_primary || surface.is_z {
            return Err(GXERR_UNSUPPORTED);
        }
        if flags & (GXLOCK_WRITEONLY | GXLOCK_DISCARDCONTENTS) != 0 {
            self.m(id).cpu_valid = true; // the old pixels are not wanted
        }
        self.for_cpu_read(id);
        let surface = self.m(id);
        if surface.memory.is_none() {
            return Err(GXERR_OUTOFMEMORY);
        }
        surface.locked = true;
        let (pitch, bytes) = (surface.pitch as usize, (surface.format.bits / 8) as usize);
        let memory = surface.memory.as_deref_mut().unwrap();
        let start = rect.map_or(0, |r| r.top as usize * pitch + r.left as usize * bytes);
        // SAFETY: stays inside the allocation for any rectangle inside the surface, as the C++'s pointer arithmetic did
        Ok(unsafe { memory.as_mut_ptr().add(start) })
    }

    pub fn unlock(&mut self, id: SurfaceId) -> GxResult {
        if !self.s(id).locked {
            return GXERR_NOTLOCKED;
        }
        self.m(id).locked = false;
        self.for_cpu_write(id);
        GX_OK
    }

    pub fn set_color_key(&mut self, id: SurfaceId, key: Option<u32>) -> GxResult {
        match key {
            Some(key) => {
                let changed = !self.s(id).has_key || self.s(id).key != key;
                let surface = self.m(id);
                surface.has_key = true;
                surface.key = key;
                if changed && self.s(id).is_texture {
                    // the key is baked into the texture's alpha
                    self.for_cpu_read(id);
                    self.m(id).gpu_valid = false;
                }
            }
            None => self.m(id).has_key = false,
        }
        GX_OK
    }

    pub fn surface_desc(&self, id: SurfaceId) -> GxSurfaceDesc {
        let s = self.s(id);
        GxSurfaceDesc {
            dwSize: std::mem::size_of::<GxSurfaceDesc>() as _,
            dwFlags: (GXSD_CAPS | GXSD_WIDTH | GXSD_HEIGHT | GXSD_PIXELFORMAT) as _,
            dwWidth: s.width as _,
            dwHeight: s.height as _,
            lPitch: (if s.pitch != 0 { s.pitch } else { ((s.width * (s.format.bits / 8) + 3) & !3) as i32 }) as _,
            ddpfPixelFormat: s.pixel_format,
            ddsCaps: GxSCaps { dwCaps: s.caps as _, dwCaps2: s.caps2 as _ },
            ..Default::default()
        }
    }

    // ---- DrawText

    /// `GXSurface::DrawText`, up to the callback.
    pub fn draw_text_begin(&mut self, target: SurfaceId, bounds: Rect) -> TextJob {
        let clipped = self.s(target).clip(bounds);
        let (width, height) = (clipped.right - clipped.left, clipped.bottom - clipped.top);
        if width <= 0 || height <= 0 {
            return TextJob::Done(GX_OK);
        }
        let (mut width, mut height) = (width as u32, height as u32);
        let surface = self.s(target);
        if surface.is_primary || surface.is_z {
            return TextJob::Done(GXERR_UNSUPPORTED);
        }
        let mut bits = vec![0u8; width as usize * height as usize * 4];
        let (mut flags, mut scale) = (0, 0.0);
        let gpu = !surface.cpu_valid;
        if gpu {
            // GPU-resident (a render target drawn into): the text goes onto a transparent buffer as premultiplied colour
            // and coverage, which is blended onto the GPU copy (no read-back)
            flags = PLAT_FONT_TARGET_PREMULTIPLIED;
            if surface.scale != 1.0 {
                // a scaled target: the text at its scale, onto the same logical bounds (sharp, not stretched)
                width = (width as f32 * surface.scale).ceil() as u32;
                height = (height as f32 * surface.scale).ceil() as u32;
                bits = vec![0u8; width as usize * height as usize * 4];
                scale = surface.scale;
            }
        } else {
            // the CPU copy: the text draws onto a 32-bit copy of the bounds and the pixels it changed are written back
            // in the surface's format (alpha opaque), as the original's GDI text on the surface's DC
            if !self.m(target).allocate_memory() {
                return TextJob::Done(GXERR_OUTOFMEMORY); // a new CPU-side surface has no memory until first used
            }
            let surface = self.s(target);
            for y in 0..height {
                for x in 0..width {
                    let pixel = surface.read_pixel(clipped.left as u32 + x, clipped.top as u32 + y);
                    bits[(y as usize * width as usize + x as usize) * 4..][..4]
                        .copy_from_slice(&surface.format.to_bgra(pixel));
                }
            }
        }
        let before = if gpu { Vec::new() } else { bits.clone() };
        TextJob::Pending(PendingText {
            target,
            bounds: clipped,
            bits,
            before,
            pitch: (width * 4) as i32,
            width,
            height,
            flags,
            scale,
            gpu,
        })
    }

    /// `GXSurface::DrawText` after the callback (`drawn`: its result).
    pub fn draw_text_finish(&mut self, job: PendingText, drawn: bool) -> GxResult {
        if !drawn {
            return GXERR_GENERIC;
        }
        let PendingText { target, bounds, mut bits, before, width, height, gpu, .. } = job;
        if gpu {
            let any = bits.iter().skip(3).step_by(4).any(|&alpha| alpha != 0);
            if !any {
                return GX_OK;
            }
            let text = self.add_surface(Surface {
                width,
                height,
                pixel_format: ARGB32,
                format: Format::set(&ARGB32),
                memory: Some(std::mem::take(&mut bits)),
                pitch: (width * 4) as i32,
                gpu_valid: false,
                ..Default::default()
            });
            self.upload(text);
            let source = Rect::whole(width, height);
            self.blit(target, &bounds, Some(text), Some(&source), BlitMode::Premultiplied, None, 0.0, false);
            self.remove_surface(text);
            return GX_OK;
        }
        for y in 0..height {
            for x in 0..width {
                let i = (y as usize * width as usize + x as usize) * 4;
                if before[i..i + 3] != bits[i..i + 3] {
                    bits[i + 3] = 255;
                    let surface = self.m(target);
                    let pixel = surface.format.from_bgra(&bits[i..i + 4]);
                    surface.write_pixel(bounds.left as u32 + x, bounds.top as u32 + y, pixel);
                }
            }
        }
        self.for_cpu_write(target);
        GX_OK
    }
}

/// `WGPUInstanceFlag`s to wgpu's.
pub fn instance_flags(bits: u32) -> wgpu::InstanceFlags {
    if bits == 0 {
        return wgpu::InstanceFlags::empty(); // WGPUInstanceFlag_Default in a release build
    }
    let mut flags = wgpu::InstanceFlags::empty();
    if bits & 1 != 0 {
        flags |= wgpu::InstanceFlags::DEBUG;
    }
    if bits & 2 != 0 {
        flags |= wgpu::InstanceFlags::VALIDATION;
    }
    if bits & 4 != 0 {
        flags |= wgpu::InstanceFlags::DISCARD_HAL_LABELS;
    }
    flags
}

/// `Present`'s no-v-sync choice: the last immediate or mailbox mode the surface lists, else FIFO.
pub fn select_present_mode(caps: &SwapCaps) -> wgpu::PresentMode {
    let mut mode = wgpu::PresentMode::Fifo;
    for &candidate in &caps.present_modes {
        if candidate == wgpu::PresentMode::Immediate || candidate == wgpu::PresentMode::Mailbox {
            mode = candidate;
        }
    }
    mode
}

/// `Present`'s first-time format choice: BGRA8Unorm when listed, otherwise the first listed (or BGRA8Unorm).
pub fn select_swap_format(caps: &SwapCaps) -> wgpu::TextureFormat {
    if caps.formats.contains(&wgpu::TextureFormat::Bgra8Unorm) {
        wgpu::TextureFormat::Bgra8Unorm
    } else {
        caps.formats.first().copied().unwrap_or(wgpu::TextureFormat::Bgra8Unorm)
    }
}
