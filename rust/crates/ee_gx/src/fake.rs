//! Test doubles: a [`Backend`] that records its calls and a [`Platform`] driven by fields.
#![allow(clippy::type_complexity)]

use std::cell::RefCell;
use std::collections::HashMap;
use std::ffi::c_void;

use crate::abi::NativeWindow;
use crate::gx::*;
use crate::modes::DisplayInfo;
use crate::surface::SurfaceId;

#[derive(Default)]
pub struct FakePlatform {
    pub pixels: Option<(i32, i32)>,
    pub logical: Option<(i32, i32)>,
    pub env_vars: HashMap<String, String>,
    pub client: (i32, i32),
    pub logs: RefCell<Vec<String>>,
    pub now_ms: std::cell::Cell<u64>,
    pub now_us: std::cell::Cell<u64>,
    pub slept_us: RefCell<Vec<u64>>,
}

impl Platform for FakePlatform {
    fn env(&self, name: &str) -> Option<String> {
        self.env_vars.get(name).cloned()
    }
    fn log(&self, line: &str) {
        self.logs.borrow_mut().push(line.to_owned());
    }
    fn client_size(&self, _window: *mut c_void) -> (i32, i32) {
        self.client
    }
    fn display_info(&self) -> DisplayInfo {
        DisplayInfo { logical: self.logical, pixels: self.pixels }
    }
    fn native_window(&self, _window: *mut c_void) -> NativeWindow {
        NativeWindow {
            kind: crate::abi::PLAT_NATIVE_WIN32,
            display: std::ptr::dangling_mut(),
            window: 2 as *mut c_void,
        }
    }
    fn now_ms(&self) -> u64 {
        self.now_ms.get()
    }
    fn now_us(&self) -> u64 {
        self.now_us.get()
    }
    fn sleep_us(&self, microseconds: u64) {
        self.slept_us.borrow_mut().push(microseconds);
    }
    fn redirect_stderr(&self, _path: &str) {}
    fn install_wgpu_log(&self, _level: &str) {}
}

/// `FakePlatform` for the device tests in `gpu.rs`: the environment is empty and the window the zero-sized fake.
#[derive(Default)]
pub struct PlatformBase {
    pub pixels: Option<(i32, i32)>,
}

impl Platform for PlatformBase {
    fn env(&self, _name: &str) -> Option<String> {
        None
    }
    fn log(&self, line: &str) {
        crate::plat::gx_log(line);
    }
    fn client_size(&self, _window: *mut c_void) -> (i32, i32) {
        (0, 0)
    }
    fn display_info(&self) -> DisplayInfo {
        DisplayInfo { logical: None, pixels: self.pixels }
    }
    fn native_window(&self, _window: *mut c_void) -> NativeWindow {
        NativeWindow::default()
    }
    fn now_ms(&self) -> u64 {
        ee_plat_core::time::PlatTimeMilliseconds64()
    }
    fn now_us(&self) -> u64 {
        0
    }
    fn sleep_us(&self, _microseconds: u64) {}
    fn redirect_stderr(&self, _path: &str) {}
    fn install_wgpu_log(&self, _level: &str) {}
}

#[derive(Clone, Debug, PartialEq)]
pub enum Call {
    Start,
    CreateTexture(SurfaceId, u32, u32, u32, bool),
    Release(SurfaceId),
    WriteRing(RingKind, u64, usize),
    UploadLevel(SurfaceId, u32, u32, u32),
    ReadTexture(SurfaceId, u32, u32),
    BeginPass(SurfaceId, Option<SurfaceId>),
    EndPass,
    Submit,
    DrawBlit(BlitDraw),
    Draw(DrawCall),
    SwapCreate(SurfaceId),
    SwapCaps(SurfaceId),
    SwapConfigure(SurfaceId, wgpu::TextureFormat, u32, u32, wgpu::PresentMode),
    SwapAcquire(SurfaceId),
    SwapBeginPass(SurfaceId),
    SwapPresent(SurfaceId),
}

#[derive(Default)]
pub struct FakeBackend {
    pub calls: Vec<Call>,
    pub start_result: Option<StartError>,
    pub info: AdapterInfo,
    pub uploads: HashMap<(SurfaceId, u32), Vec<u8>>,
    pub readback: HashMap<SurfaceId, ReadBack>,
    pub rings: HashMap<(u64, u64), Vec<u8>>,
    pub swap_create_ok: Option<bool>,
    pub caps: Option<SwapCaps>,
    pub acquire_fails: bool,
    pub refuse_texture: bool,
}

impl FakeBackend {
    pub fn count(&self, f: impl Fn(&Call) -> bool) -> usize {
        self.calls.iter().filter(|c| f(c)).count()
    }
}

impl Backend for FakeBackend {
    fn start(&mut self, _config: &StartConfig) -> Result<AdapterInfo, StartError> {
        self.calls.push(Call::Start);
        match self.start_result {
            Some(error) => Err(error),
            None => Ok(self.info.clone()),
        }
    }
    fn create_texture(&mut self, id: SurfaceId, w: u32, h: u32, mips: u32, is_z: bool) -> bool {
        self.calls.push(Call::CreateTexture(id, w, h, mips, is_z));
        !self.refuse_texture
    }
    fn release_surface(&mut self, id: SurfaceId) {
        self.calls.push(Call::Release(id));
    }
    fn write_ring(&mut self, ring: RingKind, offset: u64, data: &[u8]) {
        self.calls.push(Call::WriteRing(ring, offset, data.len()));
        self.rings.insert((ring_key(ring), offset), data.to_vec());
    }
    fn upload_level(&mut self, top: SurfaceId, mip: u32, w: u32, h: u32, bgra: &[u8]) {
        self.calls.push(Call::UploadLevel(top, mip, w, h));
        self.uploads.insert((top, mip), bgra.to_vec());
    }
    fn read_texture(&mut self, id: SurfaceId, w: u32, h: u32) -> ReadBack {
        self.calls.push(Call::ReadTexture(id, w, h));
        self.readback.remove(&id).unwrap_or(ReadBack::Pixels(vec![0xab; w as usize * h as usize * 4]))
    }
    fn begin_pass(&mut self, target: SurfaceId, depth: Option<SurfaceId>) {
        self.calls.push(Call::BeginPass(target, depth));
    }
    fn end_pass(&mut self) {
        self.calls.push(Call::EndPass);
    }
    fn submit(&mut self) {
        self.calls.push(Call::Submit);
    }
    fn draw_blit(&mut self, draw: &BlitDraw) {
        self.calls.push(Call::DrawBlit(*draw));
    }
    fn draw(&mut self, draw: &DrawCall) {
        self.calls.push(Call::Draw(*draw));
    }
    fn swap_create(&mut self, primary: SurfaceId, _native: &NativeWindow) -> bool {
        self.calls.push(Call::SwapCreate(primary));
        self.swap_create_ok.unwrap_or(true)
    }
    fn swap_caps(&mut self, primary: SurfaceId) -> SwapCaps {
        self.calls.push(Call::SwapCaps(primary));
        self.caps.clone().unwrap_or(SwapCaps {
            formats: vec![wgpu::TextureFormat::Rgba8Unorm, wgpu::TextureFormat::Bgra8Unorm],
            present_modes: vec![wgpu::PresentMode::Fifo, wgpu::PresentMode::Mailbox, wgpu::PresentMode::Immediate],
        })
    }
    fn swap_configure(
        &mut self,
        primary: SurfaceId,
        format: wgpu::TextureFormat,
        w: u32,
        h: u32,
        mode: wgpu::PresentMode,
    ) {
        self.calls.push(Call::SwapConfigure(primary, format, w, h, mode));
    }
    fn swap_acquire(&mut self, primary: SurfaceId) -> bool {
        self.calls.push(Call::SwapAcquire(primary));
        !self.acquire_fails
    }
    fn swap_begin_pass(&mut self, primary: SurfaceId) {
        self.calls.push(Call::SwapBeginPass(primary));
    }
    fn swap_present(&mut self, primary: SurfaceId) {
        self.calls.push(Call::SwapPresent(primary));
    }
}

pub fn ring_key(ring: RingKind) -> u64 {
    match ring {
        RingKind::Vertex => 0,
        RingKind::Index => 1,
        RingKind::Uniform => 2,
    }
}
