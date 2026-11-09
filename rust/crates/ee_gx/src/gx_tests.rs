//! The logic of [`crate::gx`] and [`crate::device`] against the recording backend.

use crate::abi::*;
use crate::device::{self, Device, VertexBuffer};
use crate::directdraw::{self, DirectDraw};
use crate::fake::{Call, FakeBackend, FakePlatform};
use crate::gx::*;
use crate::keys::BlitMode;
use crate::modes::DisplayInfo;
use crate::surface::SurfaceId;

type G = Gx<FakeBackend, FakePlatform>;

fn gx() -> G {
    Gx::new(FakeBackend::default(), FakePlatform::default())
}

fn dd() -> DirectDraw {
    DirectDraw::new(&DisplayInfo { logical: None, pixels: Some((1024, 768)) })
}

const XRGB: GxPixelFormat = GxPixelFormat::rgb(32, 0xff0000, 0xff00, 0xff);
const RGB565: GxPixelFormat = GxPixelFormat::rgb(16, 0xf800, 0x7e0, 0x1f);

fn description(width: u32, height: u32, caps: u32, format: GxPixelFormat) -> GxSurfaceDesc {
    GxSurfaceDesc {
        dwFlags: (GXSD_WIDTH | GXSD_HEIGHT | GXSD_PIXELFORMAT) as _,
        dwWidth: width as _,
        dwHeight: height as _,
        ddpfPixelFormat: format,
        ddsCaps: GxSCaps { dwCaps: caps as _, dwCaps2: 0 },
        ..Default::default()
    }
}

fn surface(g: &mut G, width: u32, height: u32, caps: u32) -> SurfaceId {
    directdraw::create_surface(g, &dd(), &description(width, height, caps, XRGB)).unwrap()
}

fn offscreen(g: &mut G, width: u32, height: u32) -> SurfaceId {
    surface(g, width, height, 0)
}

fn target(g: &mut G, width: u32, height: u32) -> SurfaceId {
    surface(g, width, height, GXSCAPS_3DDEVICE)
}

fn drained(g: &mut G) -> Vec<Call> {
    std::mem::take(&mut g.backend.calls)
}

fn primary(g: &mut G) -> SurfaceId {
    let mut display = dd();
    display.window = std::ptr::dangling_mut();
    g.plat.client = (1024, 768);
    directdraw::create_surface(
        g,
        &display,
        &GxSurfaceDesc { ddsCaps: GxSCaps { dwCaps: GXSCAPS_PRIMARYSURFACE as _, dwCaps2: 0 }, ..Default::default() },
    )
    .unwrap()
}

// ---- start

#[test]
fn start_logs_the_adapter_and_runs_once() {
    let mut g = gx();
    g.backend.info = AdapterInfo { name: "WARP".into(), vendor_id: 1, device_id: 2, backend: 4, device_type: 3 };
    assert!(g.start());
    assert!(g.start());
    assert_eq!(drained(&mut g).iter().filter(|c| **c == Call::Start).count(), 1);
    assert_eq!(g.plat.logs.borrow()[0], "GX: adapter WARP (backend 4, type 3)");
    assert_eq!(g.adapter.device_id, 2);
}

#[test]
fn a_failed_start_is_not_retried() {
    let mut g = gx();
    g.backend.start_result = Some(StartError::NoAdapter);
    assert!(!g.start());
    assert_eq!(g.plat.logs.borrow()[0], "GX: no adapter");
    assert!(!g.start());
    assert_eq!(g.backend.count(|c| *c == Call::Start), 1);
    let mut g = gx();
    g.backend.start_result = Some(StartError::NoDevice);
    assert!(!g.start());
    assert_eq!(g.plat.logs.borrow()[0], "GX: no device");
}

#[test]
fn the_environment_selects_backends_and_flags() {
    struct Cfg;
    let _ = Cfg;
    let mut g = gx();
    g.plat.env_vars.insert("EE_WGPU_BACKEND".into(), "VULKAN".into());
    g.plat.env_vars.insert("EE_WGPU_FLAGS".into(), "3".into());
    g.plat.env_vars.insert("EE_WGPU_FALLBACK".into(), "1".into());
    g.plat.env_vars.insert("EE_WGPU_DX12_COMPILER".into(), "DXC".into());
    let cfg = cfg_of(&mut g);
    assert_eq!(cfg.backends, wgpu::Backends::VULKAN);
    assert!(cfg.flags.contains(wgpu::InstanceFlags::DEBUG | wgpu::InstanceFlags::VALIDATION));
    assert!(cfg.force_fallback);
    assert!(cfg.dxc);
    let cfg = cfg_of(&mut gx());
    assert_eq!(cfg.backends, wgpu::Backends::DX12 | wgpu::Backends::VULKAN | wgpu::Backends::METAL);
    assert_eq!(cfg.flags, wgpu::InstanceFlags::empty());
    assert!(!cfg.force_fallback && !cfg.dxc);
}

/// `Gx::start`'s config, captured by a backend that fails.
fn cfg_of(g: &mut G) -> StartConfig {
    use std::sync::Mutex;
    static CAPTURED: Mutex<Option<StartConfig>> = Mutex::new(None);
    struct Capture;
    impl Backend for Capture {
        fn start(&mut self, config: &StartConfig) -> Result<AdapterInfo, StartError> {
            *CAPTURED.lock().unwrap() = Some(*config);
            Err(StartError::NoAdapter)
        }
        fn create_texture(&mut self, _: SurfaceId, _: u32, _: u32, _: u32, _: bool) -> bool {
            true
        }
        fn release_surface(&mut self, _: SurfaceId) {}
        fn write_ring(&mut self, _: RingKind, _: u64, _: &[u8]) {}
        fn upload_level(&mut self, _: SurfaceId, _: u32, _: u32, _: u32, _: &[u8]) {}
        fn read_texture(&mut self, _: SurfaceId, _: u32, _: u32) -> ReadBack {
            ReadBack::Failed
        }
        fn begin_pass(&mut self, _: SurfaceId, _: Option<SurfaceId>) {}
        fn end_pass(&mut self) {}
        fn submit(&mut self) {}
        fn draw_blit(&mut self, _: &BlitDraw) {}
        fn draw(&mut self, _: &DrawCall) {}
        fn swap_create(&mut self, _: SurfaceId, _: &NativeWindow) -> bool {
            false
        }
        fn swap_caps(&mut self, _: SurfaceId) -> SwapCaps {
            SwapCaps { formats: vec![], present_modes: vec![] }
        }
        fn swap_configure(&mut self, _: SurfaceId, _: wgpu::TextureFormat, _: u32, _: u32, _: wgpu::PresentMode) {}
        fn swap_acquire(&mut self, _: SurfaceId) -> bool {
            false
        }
        fn swap_begin_pass(&mut self, _: SurfaceId) {}
        fn swap_present(&mut self, _: SurfaceId) {}
    }
    let plat = FakePlatform { env_vars: g.plat.env_vars.clone(), ..Default::default() };
    let mut capturing = Gx::new(Capture, plat);
    capturing.start();
    let config = CAPTURED.lock().unwrap().take().unwrap();
    config
}

#[test]
fn stderr_and_log_settings_are_applied_at_start() {
    let mut g = gx();
    g.plat.env_vars.insert("EE_GX_STDERR".into(), "x.txt".into());
    g.plat.env_vars.insert("EE_GX_LOG".into(), "info".into());
    assert!(g.start());
}

// ---- the bookkeeping of the copies

#[test]
fn unlock_marks_the_gpu_copy_stale_and_the_next_gpu_use_uploads() {
    let mut g = gx();
    let s = offscreen(&mut g, 4, 4);
    assert!(g.lock(s, None, 0).is_ok());
    assert_eq!(g.unlock(s), GX_OK);
    assert!(!g.s(s).gpu_valid);
    g.for_gpu_read(s);
    let calls = drained(&mut g);
    assert!(calls.contains(&Call::CreateTexture(s, 4, 4, 1, false)));
    assert!(calls.contains(&Call::UploadLevel(s, 0, 4, 4)));
    assert!(g.s(s).gpu_valid);
    // nothing changed since: no second upload
    g.for_gpu_read(s);
    assert!(drained(&mut g).is_empty());
}

#[test]
fn unlock_without_lock_reports_not_locked() {
    let mut g = gx();
    let s = offscreen(&mut g, 4, 4);
    assert_eq!(g.unlock(s), GXERR_NOTLOCKED);
}

#[test]
fn lock_refuses_primary_and_z_and_hands_a_pointer() {
    let mut g = gx();
    let p = primary(&mut g);
    assert_eq!(g.lock(p, None, 0).err(), Some(GXERR_UNSUPPORTED));
    let z = surface(&mut g, 4, 4, GXSCAPS_ZBUFFER);
    assert_eq!(g.lock(z, None, 0).err(), Some(GXERR_UNSUPPORTED));
    let s = offscreen(&mut g, 4, 4);
    let whole = g.lock(s, None, 0).unwrap();
    let inner = g.lock(s, Some(Rect::new(2, 1, 4, 4)), 0).unwrap();
    assert_eq!(inner as usize - whole as usize, 16 + 8);
}

#[test]
fn a_gpu_drawn_surface_reads_back_on_lock() {
    let mut g = gx();
    let s = target(&mut g, 2, 2);
    g.blit(s, &Rect::new(0, 0, 2, 2), None, None, BlitMode::Fill, Some([1.0; 4]), 0.0, false);
    assert!(!g.s(s).cpu_valid);
    g.backend.readback.insert(s, ReadBack::Pixels(vec![1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]));
    let p = g.lock(s, None, 0).unwrap();
    // SAFETY: inside the 2x2 surface's memory
    assert_eq!(unsafe { *p }, 1);
    assert!(g.s(s).cpu_valid);
    assert_eq!(g.s(s).read_pixel(1, 1), u32::from_le_bytes([13, 14, 15, 16]));
    let calls = drained(&mut g);
    let submit = calls.iter().position(|c| *c == Call::Submit).unwrap();
    let read = calls.iter().position(|c| matches!(c, Call::ReadTexture(..))).unwrap();
    assert!(submit < read, "the open encoder's commands land before the copy");
}

#[test]
fn a_write_only_lock_skips_the_read_back() {
    let mut g = gx();
    let s = target(&mut g, 2, 2);
    g.blit(s, &Rect::new(0, 0, 2, 2), None, None, BlitMode::Fill, Some([1.0; 4]), 0.0, false);
    assert!(g.lock(s, None, GXLOCK_WRITEONLY).is_ok());
    assert!(!drained(&mut g).iter().any(|c| matches!(c, Call::ReadTexture(..))));
}

#[test]
fn read_back_failures_are_logged_and_leave_the_cpu_copy_marked_valid() {
    let mut g = gx();
    let s = target(&mut g, 2, 2);
    g.blit(s, &Rect::new(0, 0, 2, 2), None, None, BlitMode::Fill, Some([1.0; 4]), 0.0, false);
    g.backend.readback.insert(s, ReadBack::Failed);
    assert!(g.lock(s, None, 0).is_ok());
    assert_eq!(g.plat.logs.borrow().last().unwrap(), "GX: read-back failed");
    let mut g = gx();
    let s = target(&mut g, 2, 2);
    g.blit(s, &Rect::new(0, 0, 2, 2), None, None, BlitMode::Fill, Some([1.0; 4]), 0.0, false);
    g.backend.readback.insert(s, ReadBack::TimedOut);
    assert!(g.lock(s, None, 0).is_ok());
    assert!(g.plat.logs.borrow().iter().any(|l| l == "GX: read-back of 2x2 timed out"));
    assert_eq!(g.plat.logs.borrow().last().unwrap(), "GX: read-back failed");
}

#[test]
fn an_upload_after_use_in_the_open_encoder_submits_first() {
    let mut g = gx();
    let texture = surface(&mut g, 2, 2, GXSCAPS_TEXTURE);
    let t = target(&mut g, 4, 4);
    g.blit(t, &Rect::new(0, 0, 4, 4), Some(texture), None, BlitMode::Copy, None, 0.0, false);
    assert_eq!(g.s(texture).used_serial, g.serial);
    drained(&mut g);
    g.lock(texture, None, 0).unwrap();
    g.unlock(texture);
    g.for_gpu_read(texture);
    let calls = drained(&mut g);
    assert_eq!(calls, vec![Call::EndPass, Call::Submit, Call::UploadLevel(texture, 0, 2, 2)]);
}

#[test]
fn a_mip_level_write_uploads_only_dirty_levels() {
    let mut g = gx();
    let mut d = description(8, 8, GXSCAPS_TEXTURE | GXSCAPS_MIPMAP, RGB565);
    d.dwFlags |= GXSD_MIPMAPCOUNT as std::ffi::c_ulong;
    d.dwMipMapCount = 3;
    let top = directdraw::create_surface(&mut g, &dd(), &d).unwrap();
    let level1 = g.s(top).attached[0];
    let level2 = g.s(level1).attached[0];
    g.lock(top, None, 0).unwrap();
    g.unlock(top);
    g.for_gpu_read(top);
    drained(&mut g);
    assert!(g.s(top).gpu_valid);
    // touching level 2 marks the chain top dirty; only that level re-uploads (level 1 was uploaded with the chain)
    g.lock(level2, None, 0).unwrap();
    g.unlock(level2);
    g.lock(level1, None, 0).unwrap();
    g.unlock(level1);
    assert!(g.s(top).levels_dirty);
    g.for_gpu_read(top);
    let calls = drained(&mut g);
    assert!(calls.contains(&Call::UploadLevel(top, 1, 4, 4)));
    assert!(calls.contains(&Call::UploadLevel(top, 2, 2, 2)));
    assert!(!g.s(top).levels_dirty);
    assert!(g.s(level1).gpu_valid && g.s(level2).gpu_valid);
}

#[test]
fn a_colour_key_change_on_a_texture_invalidates_the_gpu_copy() {
    let mut g = gx();
    let t = surface(&mut g, 2, 2, GXSCAPS_TEXTURE);
    g.for_gpu_read(t);
    assert!(g.s(t).gpu_valid);
    assert_eq!(g.set_color_key(t, Some(5)), GX_OK);
    assert!(!g.s(t).gpu_valid, "the key is baked into the texture's alpha");
    g.for_gpu_read(t);
    assert_eq!(g.set_color_key(t, Some(5)), GX_OK);
    assert!(g.s(t).gpu_valid, "the same key keeps the texture");
    assert_eq!(g.set_color_key(t, None), GX_OK);
    assert!(!g.s(t).has_key);
}

#[test]
fn a_scaled_surface_uploads_through_a_temporary() {
    let mut g = gx();
    g.plat.pixels = Some((1536, 1152));
    let display = DirectDraw { mode_width: 1024, mode_height: 768, mode_bits: 32, ..dd() };
    let t = directdraw::create_surface(&mut g, &display, &description(1024, 768, GXSCAPS_3DDEVICE, XRGB)).unwrap();
    assert!(g.s(t).scale > 1.0);
    let live = g.live_surfaces();
    g.for_cpu_write(t);
    g.for_gpu_read(t);
    let calls = drained(&mut g);
    // the CPU pixels go up at the logical size, then stretch onto the scaled texture
    assert!(calls.iter().any(|c| matches!(c, Call::UploadLevel(_, 0, 1024, 768))));
    assert!(calls
        .iter()
        .any(|c| matches!(c, Call::DrawBlit(BlitDraw { mode: BlitMode::Copy, width: 1536, height: 1152, .. }))));
    assert_eq!(g.live_surfaces(), live, "the temporary is gone");
    assert!(g.s(t).cpu_valid, "the blit did not stale the CPU copy it came from");
    assert!(g.s(t).gpu_valid);
}

#[test]
fn a_scaled_surface_reads_back_at_its_logical_size() {
    let mut g = gx();
    g.plat.pixels = Some((1536, 1152));
    let display = DirectDraw { mode_width: 1024, mode_height: 768, mode_bits: 32, ..dd() };
    let t = directdraw::create_surface(&mut g, &display, &description(1024, 768, GXSCAPS_3DDEVICE, XRGB)).unwrap();
    g.blit(t, &Rect::new(0, 0, 1024, 768), None, None, BlitMode::Fill, Some([1.0; 4]), 0.0, false);
    let live = g.live_surfaces();
    g.for_cpu_read(t);
    let calls = drained(&mut g);
    assert!(calls.iter().any(|c| matches!(c, Call::ReadTexture(_, 1024, 768))));
    assert_eq!(g.live_surfaces(), live);
    assert!(g.s(t).cpu_valid);
    assert_eq!(g.s(t).read_pixel(0, 0), 0xabababab);
}

// ---- the rings and the pass

#[test]
fn ring_writes_align_pad_and_wrap_with_a_submit() {
    let mut g = gx();
    let a = g.ring(RingKind::Vertex, 64, &[1, 2, 3, 4, 5, 6], 4);
    assert_eq!(a, 0);
    let b = g.ring(RingKind::Vertex, 64, &[7; 8], 4);
    assert_eq!(b, 8, "the 6-byte write was padded to 8");
    let wrapped = g.ring(RingKind::Vertex, 64, &[9; 60], 4);
    assert_eq!(wrapped, 0);
    let calls = drained(&mut g);
    assert!(calls.contains(&Call::Submit));
    assert_eq!(g.ring_room(RingKind::Vertex), 60);
    assert_eq!(g.backend.rings[&(0, 0)].len(), 60);
    assert_eq!(g.ring(RingKind::Uniform, 1000, &[0; 4], 256), 0);
    assert_eq!(g.ring(RingKind::Uniform, 1000, &[0; 4], 256), 256);
}

#[test]
fn a_submit_resets_the_rings_and_counts() {
    let mut g = gx();
    g.ring(RingKind::Index, 100, &[0; 8], 4);
    let serial = g.serial;
    g.submit();
    assert_eq!(g.ring_room(RingKind::Index), 0);
    assert_eq!(g.serial, serial + 1);
}

#[test]
fn the_pass_is_kept_for_the_same_target_and_reopened_for_another() {
    let mut g = gx();
    let a = target(&mut g, 2, 2);
    let b = target(&mut g, 2, 2);
    let rect = Rect::new(0, 0, 2, 2);
    g.blit(a, &rect, None, None, BlitMode::Fill, Some([1.0; 4]), 0.0, false);
    g.blit(a, &rect, None, None, BlitMode::Fill, Some([1.0; 4]), 0.0, false);
    let passes = g.backend.count(|c| matches!(c, Call::BeginPass(..)));
    assert_eq!(passes, 1, "one pass for two blits to one target");
    g.blit(b, &rect, None, None, BlitMode::Fill, Some([1.0; 4]), 0.0, false);
    assert!(g.backend.calls.contains(&Call::EndPass));
    assert_eq!(g.backend.count(|c| matches!(c, Call::BeginPass(..))), 2);
}

#[test]
fn deleting_the_target_ends_its_pass() {
    let mut g = gx();
    let a = target(&mut g, 2, 2);
    g.blit(a, &Rect::new(0, 0, 2, 2), None, None, BlitMode::Fill, Some([1.0; 4]), 0.0, false);
    assert!(g.pass_open);
    g.release(a);
    assert!(!g.pass_open);
    assert!(g.backend.calls.contains(&Call::EndPass));
    assert!(g.backend.calls.contains(&Call::Release(a)));
    assert_eq!(g.live_surfaces(), 0);
}

#[test]
fn a_depth_attachment_change_ends_the_pass() {
    let mut g = gx();
    let a = target(&mut g, 2, 2);
    let z = surface(&mut g, 2, 2, GXSCAPS_ZBUFFER);
    g.blit(a, &Rect::new(0, 0, 2, 2), None, None, BlitMode::Fill, Some([1.0; 4]), 0.0, false);
    assert_eq!(g.add_attached(a, z), GX_OK);
    assert!(!g.pass_open);
    assert_eq!(g.s(a).depth, Some(z));
    assert_eq!(g.s(z).references, 2);
    g.blit(a, &Rect::new(0, 0, 2, 2), None, None, BlitMode::Fill, Some([1.0; 4]), 0.0, false);
    assert_eq!(g.delete_attached(a, z), GX_OK);
    assert!(!g.pass_open);
    assert_eq!(g.s(a).depth, None);
    assert_eq!(g.s(z).references, 1);
    assert_eq!(g.delete_attached(a, z), GXERR_NOTFOUND);
}

#[test]
fn get_attached_matches_by_caps_and_adds_a_reference() {
    let mut g = gx();
    let parent = target(&mut g, 2, 2);
    let z = surface(&mut g, 2, 2, GXSCAPS_ZBUFFER);
    let back = target(&mut g, 2, 2);
    g.add_attached(parent, z);
    g.add_attached(parent, back);
    assert_eq!(g.get_attached(parent, GXSCAPS_ZBUFFER), Some(z));
    assert_eq!(g.s(z).references, 3);
    assert_eq!(g.get_attached(parent, GXSCAPS_FLIP), Some(back));
    assert_eq!(g.get_attached(parent, GXSCAPS_MIPMAP), None);
    assert_eq!(g.get_attached(parent, 0), None);
}

// ---- the blits

#[test]
fn a_colour_fill_on_a_cpu_surface_stays_on_the_cpu() {
    let mut g = gx();
    let s = offscreen(&mut g, 2, 2);
    let outcome = g.blt(s, None, None, None, GXBLT_COLORFILL, 0x00ff00);
    assert_eq!(outcome.result, GX_OK);
    assert!(g.backend.calls.is_empty());
    assert_eq!(g.s(s).read_pixel(1, 1), 0x00ff00);
    assert!(!g.s(s).gpu_valid);
}

#[test]
fn a_colour_fill_on_a_gpu_surface_blits_the_colour() {
    let mut g = gx();
    let t = target(&mut g, 2, 2);
    let outcome = g.blt(t, None, None, None, GXBLT_COLORFILL, 0xff102030);
    assert_eq!(outcome.result, GX_OK);
    let draw = g.backend.calls.iter().find_map(|c| if let Call::DrawBlit(d) = c { Some(*d) } else { None }).unwrap();
    assert_eq!(draw.mode, BlitMode::Fill);
    // XRGB: the fill's alpha is the 32-bit format's "no alpha" 255
    let uniform = g.backend.rings[&(2, 0)].clone();
    let colour = &uniform[48..64];
    assert_eq!(f32::from_le_bytes(colour[..4].try_into().unwrap()), 16.0 / 255.0);
    assert_eq!(f32::from_le_bytes(colour[12..16].try_into().unwrap()), 1.0);
}

#[test]
fn a_fill_outside_the_surface_does_nothing() {
    let mut g = gx();
    let s = offscreen(&mut g, 2, 2);
    assert_eq!(g.blt(s, Some(Rect::new(5, 5, 9, 9)), None, None, GXBLT_COLORFILL, 1).result, GX_OK);
    assert_eq!(g.s(s).memory(), None, "nothing allocated, nothing written");
}

#[test]
fn z_surfaces_refuse_blts() {
    let mut g = gx();
    let z = surface(&mut g, 2, 2, GXSCAPS_ZBUFFER);
    assert_eq!(g.blt(z, None, None, None, GXBLT_COLORFILL, 0).result, GXERR_UNSUPPORTED);
}

#[test]
fn a_blit_without_a_source_is_invalid() {
    let mut g = gx();
    let s = offscreen(&mut g, 2, 2);
    assert_eq!(g.blt(s, None, None, None, 0, 0).result, GXERR_INVALIDPARAMS);
}

#[test]
fn a_blit_between_cpu_surfaces_never_touches_the_gpu() {
    let mut g = gx();
    let source = offscreen(&mut g, 2, 2);
    g.lock(source, None, 0).unwrap();
    g.m(source).write_pixel(0, 0, 7);
    g.unlock(source);
    let destination = offscreen(&mut g, 2, 2);
    assert_eq!(g.blt(destination, None, Some(source), None, 0, 0).result, GX_OK);
    assert!(g.backend.calls.is_empty());
    assert_eq!(g.s(destination).read_pixel(0, 0), 7);
}

#[test]
fn a_blit_from_a_gpu_surface_uses_the_gpu() {
    let mut g = gx();
    let source = target(&mut g, 2, 2);
    g.blit(source, &Rect::new(0, 0, 2, 2), None, None, BlitMode::Fill, Some([1.0; 4]), 0.0, false);
    let destination = offscreen(&mut g, 2, 2);
    drained(&mut g);
    assert_eq!(g.blt(destination, None, Some(source), None, 0, 0).result, GX_OK);
    let calls = drained(&mut g);
    assert!(calls.iter().any(|c| matches!(c, Call::DrawBlit(BlitDraw { mode: BlitMode::Copy, .. }))));
}

#[test]
fn a_keyed_blit_needs_the_flag_and_a_key() {
    let mut g = gx();
    let source = target(&mut g, 2, 2);
    g.blit(source, &Rect::new(0, 0, 2, 2), None, None, BlitMode::Fill, Some([1.0; 4]), 0.0, false);
    let destination = target(&mut g, 2, 2);
    g.m(source).has_key = true;
    g.m(source).key = 0xff00ff;
    drained(&mut g);
    g.blt(destination, None, Some(source), None, GXBLT_KEYSRC, 0);
    let calls = drained(&mut g);
    assert!(calls.iter().any(|c| matches!(c, Call::DrawBlit(BlitDraw { mode: BlitMode::Keyed, .. }))));
    // the key's colour goes into the uniform as floats
    let draw = calls.iter().find_map(|c| if let Call::DrawBlit(d) = c { Some(*d) } else { None }).unwrap();
    let uniform = g.backend.rings[&(2, u64::from(draw.uniform_offset))].clone();
    assert_eq!(f32::from_le_bytes(uniform[32..36].try_into().unwrap()), 1.0);
    assert_eq!(f32::from_le_bytes(uniform[36..40].try_into().unwrap()), 0.0);
    g.blt(destination, None, Some(source), None, 0, 0);
    assert!(
        drained(&mut g).iter().any(|c| matches!(c, Call::DrawBlit(BlitDraw { mode: BlitMode::Copy, .. }))),
        "no flag: a plain copy"
    );
}

#[test]
fn a_self_blit_copies_through_the_cpu() {
    let mut g = gx();
    let s = offscreen(&mut g, 3, 1);
    g.lock(s, None, 0).unwrap();
    g.m(s).write_pixel(0, 0, 11);
    g.m(s).write_pixel(1, 0, 22);
    g.m(s).write_pixel(2, 0, 33);
    g.unlock(s);
    drained(&mut g);
    // overlapping one to the right: the source is read from a copy, as DirectDraw guarantees
    assert_eq!(g.blt(s, Some(Rect::new(1, 0, 3, 1)), Some(s), Some(Rect::new(0, 0, 2, 1)), 0, 0).result, GX_OK);
    assert_eq!((g.s(s).read_pixel(0, 0), g.s(s).read_pixel(1, 0), g.s(s).read_pixel(2, 0)), (11, 11, 22));
    assert!(!g.backend.calls.iter().any(|c| matches!(c, Call::DrawBlit(..))));
}

#[test]
fn a_blit_into_a_missing_depth_buffer_is_skipped() {
    let mut g = gx();
    let t = target(&mut g, 2, 2);
    g.blit(t, &Rect::new(0, 0, 2, 2), None, None, BlitMode::Depth, Some([0.0; 4]), 0.5, false);
    assert!(!g.backend.calls.iter().any(|c| matches!(c, Call::DrawBlit(..))));
}

#[test]
fn a_depth_clear_opens_the_pass_on_the_attached_z() {
    let mut g = gx();
    let t = target(&mut g, 2, 2);
    let z = surface(&mut g, 2, 2, GXSCAPS_ZBUFFER);
    g.add_attached(t, z);
    g.blit(t, &Rect::new(0, 0, 2, 2), None, None, BlitMode::Depth, Some([0.0; 4]), 0.5, false);
    assert!(g.backend.calls.contains(&Call::BeginPass(t, Some(z))));
    let draw = g.backend.calls.iter().find_map(|c| if let Call::DrawBlit(d) = c { Some(*d) } else { None }).unwrap();
    assert!(draw.depth);
}

// ---- presentation

fn setup_present(g: &mut G) -> (SurfaceId, SurfaceId) {
    let p = primary(g);
    let frame = target(g, 1024, 768);
    (p, frame)
}

#[test]
fn flip_presents_the_last_attached_non_z_surface() {
    let mut g = gx();
    let mut display = dd();
    display.window = std::ptr::dangling_mut();
    g.plat.client = (1024, 768);
    let p = directdraw::create_surface(
        &mut g,
        &display,
        &GxSurfaceDesc {
            ddsCaps: GxSCaps { dwCaps: (GXSCAPS_PRIMARYSURFACE | GXSCAPS_FLIP) as _, dwCaps2: 0 },
            ..Default::default()
        },
    )
    .unwrap();
    let back = g.s(p).attached[0];
    let outcome = g.flip(p, 0);
    assert_eq!(outcome.result, GX_OK);
    assert_eq!(outcome.present, Some(PresentRequest { primary: p, source: back, wait: true }));
    assert!(!g.flip(p, GXFLIP_NOVSYNC).present.unwrap().wait);
}

#[test]
fn a_primary_without_a_window_never_presents() {
    let mut g = gx();
    let p = directdraw::create_surface(
        &mut g,
        &dd(),
        &GxSurfaceDesc {
            ddsCaps: GxSCaps { dwCaps: (GXSCAPS_PRIMARYSURFACE | GXSCAPS_FLIP) as _, dwCaps2: 0 },
            ..Default::default()
        },
    )
    .unwrap();
    assert!(g.flip(p, 0).present.is_none());
}

fn present(g: &mut G, primary: SurfaceId, source: SurfaceId) {
    g.present(&PresentRequest { primary, source, wait: true });
}

#[test]
fn the_first_present_creates_the_swap_chain_and_prefers_bgra() {
    let mut g = gx();
    let (p, frame) = setup_present(&mut g);
    g.blit(frame, &Rect::new(0, 0, 1024, 768), None, None, BlitMode::Fill, Some([1.0; 4]), 0.0, false);
    drained(&mut g);
    present(&mut g, p, frame);
    let calls = drained(&mut g);
    assert!(calls.contains(&Call::SwapCreate(p)));
    assert!(calls.contains(&Call::SwapConfigure(
        p,
        wgpu::TextureFormat::Bgra8Unorm,
        1024,
        768,
        wgpu::PresentMode::Fifo
    )));
    assert!(calls.contains(&Call::SwapBeginPass(p)));
    assert!(calls.contains(&Call::SwapPresent(p)));
    assert_eq!(
        g.plat.logs.borrow().iter().filter(|l| l.contains("presenting 1024x768 frames into a 1024x768 window")).count(),
        1
    );
    // the blit draws into the swap format at the window's size
    let draw = calls.iter().find_map(|c| if let Call::DrawBlit(d) = c { Some(*d) } else { None }).unwrap();
    assert_eq!(draw.format, BlitTarget::Swap(wgpu::TextureFormat::Bgra8Unorm));
    assert_eq!((draw.width, draw.height), (1024, 768));
    assert_eq!(draw.source, Some(frame));
    // a second present neither recreates nor reconfigures, and uses the same format
    present(&mut g, p, frame);
    let calls = drained(&mut g);
    assert!(!calls.iter().any(|c| matches!(c, Call::SwapCreate(_) | Call::SwapConfigure(..) | Call::SwapCaps(_))));
    assert!(calls.contains(&Call::SwapPresent(p)));
}

#[test]
fn a_resized_window_reconfigures_the_swap_chain() {
    let mut g = gx();
    let (p, frame) = setup_present(&mut g);
    present(&mut g, p, frame);
    g.plat.client = (800, 600);
    drained(&mut g);
    present(&mut g, p, frame);
    assert!(g.backend.calls.contains(&Call::SwapConfigure(
        p,
        wgpu::TextureFormat::Bgra8Unorm,
        800,
        600,
        wgpu::PresentMode::Fifo
    )));
    let draw = g.backend.calls.iter().find_map(|c| if let Call::DrawBlit(d) = c { Some(*d) } else { None }).unwrap();
    // scaled to the window: linear filtering
    assert_eq!(draw.sampler_key, keys_sampler(&g, true));
}

fn keys_sampler(g: &G, linear: bool) -> u32 {
    let (min, mag) = if linear { (GXTFN_LINEAR, GXTFG_LINEAR) } else { (GXTFN_POINT, GXTFG_POINT) };
    g.sampler_key(GXTADDRESS_CLAMP, min, mag, GXTFP_NONE)
}

#[test]
fn a_window_of_no_size_presents_nothing() {
    let mut g = gx();
    let (p, frame) = setup_present(&mut g);
    g.plat.client = (0, 768);
    present(&mut g, p, frame);
    assert!(!g.backend.calls.iter().any(|c| matches!(c, Call::SwapCreate(_))));
}

#[test]
fn a_frame_rate_setting_uses_the_last_immediate_or_mailbox_mode_and_the_first_format() {
    let mut g = gx();
    let (p, frame) = setup_present(&mut g);
    g.set_frame_limit(-1);
    present(&mut g, p, frame);
    let calls = drained(&mut g);
    // the frame-limit path takes formats[0] as is (no BGRA preference), as the C++ did
    assert!(calls.contains(&Call::SwapConfigure(
        p,
        wgpu::TextureFormat::Rgba8Unorm,
        1024,
        768,
        wgpu::PresentMode::Immediate
    )));
}

#[test]
fn a_failed_acquire_reconfigures_next_time() {
    let mut g = gx();
    let (p, frame) = setup_present(&mut g);
    present(&mut g, p, frame);
    g.backend.acquire_fails = true;
    drained(&mut g);
    present(&mut g, p, frame);
    assert!(g.backend.calls.contains(&Call::SwapAcquire(p)));
    assert!(!g.backend.calls.contains(&Call::SwapPresent(p)));
    assert_eq!(g.s(p).swap_width, 0);
    g.backend.acquire_fails = false;
    drained(&mut g);
    present(&mut g, p, frame);
    assert!(g.backend.calls.iter().any(|c| matches!(c, Call::SwapConfigure(..))));
}

#[test]
fn the_frame_limiter_sleeps_to_the_next_frame() {
    let mut g = gx();
    let (p, frame) = setup_present(&mut g);
    g.set_frame_limit(10);
    g.plat.now_us.set(1_000_000);
    present(&mut g, p, frame);
    assert!(g.plat.slept_us.borrow().is_empty(), "the first frame only sets the clock");
    g.plat.now_us.set(1_050_000);
    present(&mut g, p, frame);
    assert_eq!(*g.plat.slept_us.borrow(), vec![50_000]);
    // too far behind: the schedule restarts instead of bursting
    g.plat.slept_us.borrow_mut().clear();
    g.plat.now_us.set(9_000_000);
    present(&mut g, p, frame);
    assert!(g.plat.slept_us.borrow().is_empty());
}

#[test]
fn the_brightness_goes_to_the_present_uniform_only() {
    let mut g = gx();
    let (p, frame) = setup_present(&mut g);
    g.set_brightness(2.0);
    present(&mut g, p, frame);
    let uniform = g.backend.rings[&(2, 0)].clone();
    assert_eq!(f32::from_le_bytes(uniform[84..88].try_into().unwrap()), 2.0);
    assert_eq!(u32::from_le_bytes(uniform[68..72].try_into().unwrap()), 1, "presentation is opaque");
    // out of range: ignored
    g.set_brightness(9.0);
    let (p2, frame2) = setup_present(&mut g);
    present(&mut g, p2, frame2);
    let uniform = g.backend.rings[&(2, 0)].clone();
    assert_eq!(f32::from_le_bytes(uniform[84..88].try_into().unwrap()), 1.0);
}

#[test]
fn test_read_frame_reports_sizes_and_returns_bgra() {
    let mut g = gx();
    let s = offscreen(&mut g, 2, 1);
    g.lock(s, None, 0).unwrap();
    g.m(s).write_pixel(1, 0, 0x00112233);
    g.unlock(s);
    assert_eq!(g.test_read_frame(s, None, 0), (1, 2, 1));
    let mut out = [0u8; 8];
    assert_eq!(g.test_read_frame(s, Some(&mut out), 4).0, 1, "too small");
    assert_eq!(g.test_read_frame(s, Some(&mut out), 8), (0, 2, 1));
    assert_eq!(&out[4..8], &[0x33, 0x22, 0x11, 0x00]);
}

#[test]
fn test_read_frame_reads_back_a_gpu_frame() {
    let mut g = gx();
    let s = target(&mut g, 1, 1);
    g.blit(s, &Rect::new(0, 0, 1, 1), None, None, BlitMode::Fill, Some([1.0; 4]), 0.0, false);
    let mut out = [0u8; 4];
    assert_eq!(g.test_read_frame(s, Some(&mut out), 4), (0, 1, 1));
    assert!(g.s(s).cpu_valid, "a read-back; the GPU copy stays valid");
    assert!(g.s(s).gpu_valid);
}

#[test]
fn the_hook_gets_the_native_window_and_the_handle() {
    let mut g = gx();
    let (p, frame) = setup_present(&mut g);
    extern "C" fn hook(_: *mut std::ffi::c_void, _: *mut std::ffi::c_void) {}
    assert!(g.hook_arguments(&PresentRequest { primary: p, source: frame, wait: true }).is_none());
    g.set_test_present_hook(Some(hook));
    let (_, window, handle) = g.hook_arguments(&PresentRequest { primary: p, source: frame, wait: true }).unwrap();
    assert_eq!(window as usize, 2, "the fake's native window");
    assert_eq!(handle, g.s(frame).handle);
}

// ---- primary blits

#[test]
fn a_whole_frame_is_presented_and_held() {
    let mut g = gx();
    let (p, frame) = setup_present(&mut g);
    let outcome = g.blt(p, Some(Rect::new(30, 40, 1054, 808)), Some(frame), None, 0, 0);
    assert_eq!(outcome.present, Some(PresentRequest { primary: p, source: frame, wait: true }));
    assert_eq!(g.s(p).last_frame, Some(frame));
    assert_eq!((g.s(p).origin_x, g.s(p).origin_y), (30, 40));
    assert_eq!(g.s(frame).references, 2, "held");
}

#[test]
fn a_smaller_blit_with_no_frame_yet_does_nothing() {
    let mut g = gx();
    let (p, _) = setup_present(&mut g);
    let pointer = target(&mut g, 32, 32);
    assert!(g.blt(p, Some(Rect::new(10, 10, 42, 42)), Some(pointer), None, 0, 0).present.is_none());
    assert!(g.s(p).window_image.is_none());
}

#[test]
fn a_smaller_blit_goes_onto_a_copy_of_the_last_frame() {
    let mut g = gx();
    let (p, frame) = setup_present(&mut g);
    g.blt(p, Some(Rect::new(100, 100, 1124, 868)), Some(frame), None, 0, 0);
    let pointer = target(&mut g, 32, 32);
    g.blit(pointer, &Rect::new(0, 0, 32, 32), None, None, BlitMode::Fill, Some([1.0; 4]), 0.0, false);
    drained(&mut g);
    let outcome = g.blt(p, Some(Rect::new(110, 110, 142, 142)), Some(pointer), None, GXBLT_KEYSRC, 0);
    let image = g.s(p).window_image.unwrap();
    assert_eq!(outcome.present, Some(PresentRequest { primary: p, source: image, wait: true }));
    assert_eq!(g.s(image).pixel_format.dwRGBAlphaBitMask, 0xff000000);
    let calls = drained(&mut g);
    let blits: Vec<_> = calls.iter().filter_map(|c| if let Call::DrawBlit(d) = c { Some(*d) } else { None }).collect();
    assert_eq!(blits.len(), 2, "the frame's copy, then the small source");
    // the source rect placed in the frame's coordinates (screen minus the frame's origin)
    let uniform = g.backend.rings[&(2, blits[1].uniform_offset as u64)].clone();
    let ndc_left = f32::from_le_bytes(uniform[..4].try_into().unwrap());
    let expected = 2.0 * 10.0 / 1024.0 - 1.0;
    assert_eq!(ndc_left, expected);
    assert!(g.s(p).window_image_current);
    // a second small blit reuses the copy without blitting the frame again
    let pointer2 = target(&mut g, 16, 16);
    g.blit(pointer2, &Rect::new(0, 0, 16, 16), None, None, BlitMode::Fill, Some([1.0; 4]), 0.0, false);
    drained(&mut g);
    g.blt(p, Some(Rect::new(0, 0, 16, 16)), Some(pointer2), None, 0, 0);
    let calls = drained(&mut g);
    assert_eq!(calls.iter().filter(|c| matches!(c, Call::DrawBlit(..))).count(), 1);
}

#[test]
fn a_new_frame_makes_the_window_image_stale() {
    let mut g = gx();
    let (p, frame) = setup_present(&mut g);
    g.blt(p, None, Some(frame), None, 0, 0);
    let pointer = target(&mut g, 32, 32);
    g.blt(p, Some(Rect::new(0, 0, 32, 32)), Some(pointer), None, 0, 0);
    assert!(g.s(p).window_image_current);
    let frame2 = target(&mut g, 1024, 768);
    g.blt(p, None, Some(frame2), None, 0, 0);
    assert!(!g.s(p).window_image_current);
    assert_eq!(g.s(frame).references, 1, "the previous frame is no longer held");
}

#[test]
fn a_blit_small_against_the_window_is_not_a_frame() {
    let mut g = gx();
    let (p, _) = setup_present(&mut g);
    let small = target(&mut g, 300, 200);
    let outcome = g.blt(p, Some(Rect::new(0, 0, 300, 200)), Some(small), None, 0, 0);
    assert!(outcome.present.is_none(), "less than half the window: not a frame");
    let exact_half = target(&mut g, 512, 384);
    assert!(g.blt(p, None, Some(exact_half), None, 0, 0).present.is_some());
}

// ---- DrawText

#[test]
fn text_on_a_cpu_surface_writes_only_changed_pixels_with_opaque_alpha() {
    let mut g = gx();
    let s = offscreen(&mut g, 4, 1);
    g.lock(s, None, 0).unwrap();
    g.m(s).write_pixel(0, 0, 0x101010);
    g.m(s).write_pixel(1, 0, 0x202020);
    g.unlock(s);
    let job = g.draw_text_begin(s, Rect::new(0, 0, 4, 1));
    let TextJob::Pending(mut job) = job else { panic!("pending") };
    assert_eq!((job.width, job.height, job.pitch), (4, 1, 16));
    assert_eq!(job.flags, 0);
    assert_eq!(&job.bits[..4], &[0x10, 0x10, 0x10, 255]);
    // the callback draws pixel 1 and also writes an unchanged pixel 0's alpha
    job.bits[4] = 0x40;
    job.bits[3] = 7;
    assert_eq!(g.draw_text_finish(job, true), GX_OK);
    assert_eq!(g.s(s).read_pixel(1, 0), 0x202040, "the format has no alpha bits to set");
    assert_eq!(g.s(s).read_pixel(0, 0), 0x101010, "unchanged colour: no write");
    assert!(!g.s(s).gpu_valid);
}

#[test]
fn a_failed_text_callback_reports_generic_and_writes_nothing() {
    let mut g = gx();
    let s = offscreen(&mut g, 2, 1);
    let TextJob::Pending(job) = g.draw_text_begin(s, Rect::new(0, 0, 2, 1)) else { panic!() };
    assert_eq!(g.draw_text_finish(job, false), GXERR_GENERIC);
}

#[test]
fn text_on_a_gpu_surface_blends_premultiplied_coverage() {
    let mut g = gx();
    let t = target(&mut g, 8, 8);
    g.blit(t, &Rect::new(0, 0, 8, 8), None, None, BlitMode::Fill, Some([1.0; 4]), 0.0, false);
    let TextJob::Pending(mut job) = g.draw_text_begin(t, Rect::new(1, 1, 5, 3)) else { panic!() };
    assert_eq!(job.flags, PLAT_FONT_TARGET_PREMULTIPLIED);
    assert_eq!((job.width, job.height), (4, 2));
    job.bits[3] = 200;
    drained(&mut g);
    let live = g.live_surfaces();
    assert_eq!(g.draw_text_finish(job, true), GX_OK);
    let calls = drained(&mut g);
    assert!(calls.iter().any(|c| matches!(c, Call::UploadLevel(_, 0, 4, 2))));
    assert!(calls.iter().any(|c| matches!(c, Call::DrawBlit(BlitDraw { mode: BlitMode::Premultiplied, .. }))));
    assert_eq!(g.live_surfaces(), live, "the text surface is gone");
}

#[test]
fn empty_text_coverage_uploads_nothing() {
    let mut g = gx();
    let t = target(&mut g, 8, 8);
    g.blit(t, &Rect::new(0, 0, 8, 8), None, None, BlitMode::Fill, Some([1.0; 4]), 0.0, false);
    let TextJob::Pending(job) = g.draw_text_begin(t, Rect::new(0, 0, 4, 4)) else { panic!() };
    drained(&mut g);
    assert_eq!(g.draw_text_finish(job, true), GX_OK);
    assert!(g.backend.calls.is_empty());
}

#[test]
fn text_on_a_scaled_target_is_rendered_at_its_scale_onto_the_logical_bounds() {
    let mut g = gx();
    g.plat.pixels = Some((1536, 1152));
    let display = DirectDraw { mode_width: 1024, mode_height: 768, mode_bits: 32, ..dd() };
    let t = directdraw::create_surface(&mut g, &display, &description(1024, 768, GXSCAPS_3DDEVICE, XRGB)).unwrap();
    g.blit(t, &Rect::new(0, 0, 1024, 768), None, None, BlitMode::Fill, Some([1.0; 4]), 0.0, false);
    let TextJob::Pending(mut job) = g.draw_text_begin(t, Rect::new(10, 10, 110, 30)) else { panic!() };
    assert_eq!((job.width, job.height), (150, 30));
    assert_eq!(job.pitch, 600);
    assert_eq!(job.scale, 1.5);
    let target = job.font_target();
    assert_eq!((target.mWidth, target.mHeight, target.mPitch), (150, 30, 600));
    job.bits[3] = 255;
    drained(&mut g);
    g.draw_text_finish(job, true);
    let draw = g.backend.calls.iter().find_map(|c| if let Call::DrawBlit(d) = c { Some(*d) } else { None }).unwrap();
    assert_eq!((draw.width, draw.height), (1536, 1152));
}

#[test]
fn text_refuses_the_primary_and_z_and_ignores_empty_bounds() {
    let mut g = gx();
    let p = primary(&mut g);
    assert!(matches!(g.draw_text_begin(p, Rect::new(0, 0, 2, 2)), TextJob::Done(GXERR_UNSUPPORTED)));
    let z = surface(&mut g, 4, 4, GXSCAPS_ZBUFFER);
    assert!(matches!(g.draw_text_begin(z, Rect::new(0, 0, 2, 2)), TextJob::Done(GXERR_UNSUPPORTED)));
    let s = offscreen(&mut g, 4, 4);
    assert!(matches!(g.draw_text_begin(s, Rect::new(9, 9, 12, 12)), TextJob::Done(GX_OK)));
    assert!(matches!(g.draw_text_begin(s, Rect::new(2, 2, 2, 3)), TextJob::Done(GX_OK)));
}

// ---- the object table

#[test]
fn releases_cascade_and_handles_are_freed() {
    let mut g = gx();
    let parent = target(&mut g, 2, 2);
    let child = target(&mut g, 2, 2);
    g.add_attached(parent, child);
    assert_eq!(g.s(child).references, 2);
    g.release(child);
    assert_eq!(g.release(parent), 0);
    assert_eq!(g.live_surfaces(), 0);
    let c = target(&mut g, 2, 2);
    assert_eq!(c, parent.min(child), "the slot is reused");
    assert_eq!(g.release(c), 0);
}

#[test]
fn surface_description_reports_the_computed_pitch() {
    let mut g = gx();
    let s = directdraw::create_surface(&mut g, &dd(), &description(3, 2, 0, RGB565)).unwrap();
    let d = g.surface_desc(s);
    assert_eq!(d.lPitch, 8, "before any allocation: computed");
    assert_eq!(d.dwFlags as u32, GXSD_CAPS | GXSD_WIDTH | GXSD_HEIGHT | GXSD_PIXELFORMAT);
    g.m(s).allocate_memory();
    assert_eq!(g.surface_desc(s).lPitch, 8);
}

// ---- the 3D device

fn draw_setup(g: &mut G) -> (SurfaceId, Device, VertexBuffer) {
    let t = target(g, 64, 64);
    let dev = directdraw::create_device(g, t);
    let vb = VertexBuffer::new(GXFVF_XYZ | GXFVF_NORMAL | GXFVF_DIFFUSE | GXFVF_TEX1, 4);
    (t, dev, vb)
}

fn draw_of(g: &G) -> DrawCall {
    g.backend.calls.iter().rev().find_map(|c| if let Call::Draw(d) = c { Some(*d) } else { None }).unwrap()
}

#[test]
fn a_draw_validates_its_buffer() {
    let mut g = gx();
    let (_, mut dev, vb) = draw_setup(&mut g);
    assert_eq!(device::draw(&mut g, &mut dev, GXPT_TRIANGLELIST, None, 0, 3, None), GXERR_INVALIDPARAMS);
    assert_eq!(device::draw(&mut g, &mut dev, GXPT_TRIANGLELIST, None, 0, 0, None), GX_OK);
    let bad = VertexBuffer::new(GXFVF_XYZ, 4);
    assert_eq!(device::draw(&mut g, &mut dev, GXPT_TRIANGLELIST, Some(&bad), 0, 3, None), GXERR_INVALIDPARAMS);
    assert_eq!(
        device::draw(&mut g, &mut dev, GXPT_TRIANGLELIST, Some(&vb), 2, 3, None),
        GXERR_INVALIDPARAMS,
        "past the end"
    );
    assert_eq!(
        device::draw(&mut g, &mut dev, GXPT_TRIANGLELIST, Some(&vb), 0, 3, Some(&[])),
        GX_OK,
        "no indices: nothing"
    );
    assert_eq!(g.backend.count(|c| matches!(c, Call::Draw(_))), 0);
}

#[test]
fn a_draw_writes_rings_and_renders_into_the_target() {
    let mut g = gx();
    let (t, mut dev, vb) = draw_setup(&mut g);
    assert_eq!(device::draw(&mut g, &mut dev, GXPT_TRIANGLELIST, Some(&vb), 0, 3, None), GX_OK);
    let draw = draw_of(&g);
    assert_eq!(draw.vertex_count, 3);
    assert_eq!(draw.vertex_bytes, 108);
    assert_eq!(draw.index, None);
    assert_eq!(draw.scissor, (64, 64));
    assert_eq!(draw.viewport, [0.0, 0.0, 64.0, 64.0, 0.0, 1.0]);
    assert!(g.s(t).used_serial == g.serial);
    assert!(!g.s(t).cpu_valid, "the CPU copy is stale after a draw");
    // the CCW/LESSEQUAL/modulate defaults: the key has cull 2 (CCW) with no depth target
    assert_eq!((draw.pipeline_key >> 2) & 3, 2);
    assert_eq!((draw.pipeline_key >> 13) & 1, 0);
}

#[test]
fn an_indexed_draw_writes_its_indices() {
    let mut g = gx();
    let (_, mut dev, vb) = draw_setup(&mut g);
    assert_eq!(device::draw(&mut g, &mut dev, GXPT_TRIANGLELIST, Some(&vb), 0, 4, Some(&[0, 1, 2, 2, 1, 3])), GX_OK);
    let draw = draw_of(&g);
    assert_eq!(draw.index, Some((0, 12, 6)));
    assert_eq!(g.backend.rings[&(1, 0)], vec![0, 0, 1, 0, 2, 0, 2, 0, 1, 0, 3, 0]);
}

#[test]
fn an_odd_index_count_is_padded() {
    let mut g = gx();
    let (_, mut dev, vb) = draw_setup(&mut g);
    device::draw(&mut g, &mut dev, GXPT_TRIANGLELIST, Some(&vb), 0, 3, Some(&[0, 1, 2]));
    assert_eq!(draw_of(&g).index, Some((0, 8, 3)));
    assert_eq!(g.backend.rings[&(1, 0)].len(), 8);
}

#[test]
fn the_uniform_is_written_only_when_dirty() {
    let mut g = gx();
    let (_, mut dev, vb) = draw_setup(&mut g);
    device::draw(&mut g, &mut dev, GXPT_TRIANGLELIST, Some(&vb), 0, 3, None);
    let first = g.backend.count(|c| matches!(c, Call::WriteRing(RingKind::Uniform, ..)));
    device::draw(&mut g, &mut dev, GXPT_TRIANGLELIST, Some(&vb), 0, 3, None);
    assert_eq!(g.backend.count(|c| matches!(c, Call::WriteRing(RingKind::Uniform, ..))), first);
    dev.set_render_state(GXRS_ALPHAREF as u32, 9);
    device::draw(&mut g, &mut dev, GXPT_TRIANGLELIST, Some(&vb), 0, 3, None);
    assert_eq!(g.backend.count(|c| matches!(c, Call::WriteRing(RingKind::Uniform, ..))), first + 1);
    // a new encoder: the old offset is gone, so the block is written again
    g.submit();
    device::draw(&mut g, &mut dev, GXPT_TRIANGLELIST, Some(&vb), 0, 3, None);
    assert_eq!(g.backend.count(|c| matches!(c, Call::WriteRing(RingKind::Uniform, ..))), first + 2);
}

#[test]
fn a_depth_target_and_z_state_reach_the_pipeline_key() {
    let mut g = gx();
    let (t, mut dev, vb) = draw_setup(&mut g);
    let z = surface(&mut g, 64, 64, GXSCAPS_ZBUFFER);
    g.add_attached(t, z);
    device::draw(&mut g, &mut dev, GXPT_TRIANGLELIST, Some(&vb), 0, 3, None);
    let key = draw_of(&g).pipeline_key;
    assert_eq!((key >> 13) & 1, 1);
    assert_eq!((key >> 14) & 15, GXCMP_LESSEQUAL);
    assert_eq!((key >> 18) & 1, 1);
    assert!(g.backend.calls.iter().any(|c| matches!(c, Call::CreateTexture(id, _, _, _, true) if *id == z)));
    assert!(g.backend.calls.contains(&Call::BeginPass(t, Some(z))));
    dev.set_render_state(GXRS_ZENABLE as u32, 0);
    device::draw(&mut g, &mut dev, GXPT_TRIANGLELIST, Some(&vb), 0, 3, None);
    let key = draw_of(&g).pipeline_key;
    assert_eq!((key >> 13) & 1, 1, "still bound");
    assert_eq!((key >> 14) & 15, 0, "compares always");
}

#[test]
fn a_texture_stage_samples_the_top_of_the_chain_and_the_mip_filter_needs_levels() {
    let mut g = gx();
    let (_, mut dev, vb) = draw_setup(&mut g);
    let mut d = description(8, 8, GXSCAPS_TEXTURE | GXSCAPS_MIPMAP, RGB565);
    d.dwFlags |= GXSD_MIPMAPCOUNT as std::ffi::c_ulong;
    d.dwMipMapCount = 2;
    let top = directdraw::create_surface(&mut g, &dd(), &d).unwrap();
    let level1 = g.s(top).attached[0];
    assert_eq!(device::set_texture(&mut g, &mut dev, 0, Some(level1)), GX_OK);
    assert_eq!(g.s(level1).references, 2);
    dev.set_stage_state(0, GXTSS_MIPFILTER as u32, 2); // point
    dev.set_stage_state(0, GXTSS_MINFILTER as u32, u64::from(GXTFN_LINEAR));
    device::draw(&mut g, &mut dev, GXPT_TRIANGLELIST, Some(&vb), 0, 3, None);
    let draw = draw_of(&g);
    assert_eq!(draw.texture, Some(top));
    assert_eq!(draw.sampler_key, g.sampler_key(GXTADDRESS_WRAP, GXTFN_LINEAR, GXTFG_POINT, 2));
    assert_eq!(g.s(top).used_serial, g.serial);
    // a stage whose colour op is disabled samples nothing
    dev.set_stage_state(0, GXTSS_COLOROP as u32, u64::from(GXTOP_DISABLE));
    device::draw(&mut g, &mut dev, GXPT_TRIANGLELIST, Some(&vb), 0, 3, None);
    assert_eq!(draw_of(&g).texture, None);
}

#[test]
fn the_viewport_is_scaled_clamped_and_may_cancel_the_draw() {
    let mut g = gx();
    let (_, mut dev, vb) = draw_setup(&mut g);
    dev.viewport = GxViewport { dwX: 8, dwY: 8, dwWidth: 500, dwHeight: 500, dvMinZ: -1.0, dvMaxZ: 2.0 };
    dev.uniform_dirty = true;
    device::draw(&mut g, &mut dev, GXPT_TRIANGLELIST, Some(&vb), 0, 3, None);
    assert_eq!(draw_of(&g).viewport, [8.0, 8.0, 56.0, 56.0, 0.0, 1.0]);
    // a viewport off the target: no draw, but the pass opened and the call succeeded
    let draws = g.backend.count(|c| matches!(c, Call::Draw(_)));
    dev.viewport = GxViewport { dwX: 100, dwY: 0, dwWidth: 10, dwHeight: 10, dvMinZ: 0.0, dvMaxZ: 1.0 };
    assert_eq!(device::draw(&mut g, &mut dev, GXPT_TRIANGLELIST, Some(&vb), 0, 3, None), GX_OK);
    assert_eq!(g.backend.count(|c| matches!(c, Call::Draw(_))), draws);
}

#[test]
fn a_scaled_target_scales_the_viewport() {
    let mut g = gx();
    g.plat.pixels = Some((1536, 1152));
    let display = DirectDraw { mode_width: 1024, mode_height: 768, mode_bits: 32, ..dd() };
    let t = directdraw::create_surface(&mut g, &display, &description(1024, 768, GXSCAPS_3DDEVICE, XRGB)).unwrap();
    let mut dev = directdraw::create_device(&mut g, t);
    let vb = VertexBuffer::new(GXFVF_XYZ | GXFVF_NORMAL | GXFVF_DIFFUSE | GXFVF_TEX1, 4);
    device::draw(&mut g, &mut dev, GXPT_TRIANGLELIST, Some(&vb), 0, 3, None);
    let draw = draw_of(&g);
    assert_eq!(draw.viewport, [0.0, 0.0, 1536.0, 1152.0, 0.0, 1.0]);
    assert_eq!(draw.scissor, (1536, 1152));
}

#[test]
fn a_clear_fills_each_rectangle_inside_the_viewport() {
    let mut g = gx();
    let (_, mut dev, _) = draw_setup(&mut g);
    let rects = [GxRect { x1: 10, y1: 10, x2: 20, y2: 20 }, GxRect { x1: 100, y1: 100, x2: 200, y2: 200 }];
    assert_eq!(device::clear(&mut g, &mut dev, Some(&rects), GXCLEAR_TARGET, 0xff000000, 1.0), GX_OK);
    assert_eq!(g.backend.count(|c| matches!(c, Call::DrawBlit(_))), 1, "the second rectangle is off the target");
    let uniform = g.backend.rings.values().next().unwrap().clone();
    let left = f32::from_le_bytes(uniform[..4].try_into().unwrap());
    assert_eq!(left, 2.0 * 10.0 / 64.0 - 1.0);
}

#[test]
fn a_clear_of_a_null_list_covers_the_viewport() {
    let mut g = gx();
    let (_, mut dev, _) = draw_setup(&mut g);
    device::clear(&mut g, &mut dev, None, GXCLEAR_TARGET, 0, 1.0);
    assert_eq!(g.backend.count(|c| matches!(c, Call::DrawBlit(_))), 1);
}

#[test]
fn z_clears_use_the_depth_blit_with_the_value() {
    let mut g = gx();
    let (t, mut dev, _) = draw_setup(&mut g);
    let z = surface(&mut g, 64, 64, GXSCAPS_ZBUFFER);
    g.add_attached(t, z);
    device::clear(&mut g, &mut dev, None, GXCLEAR_ZBUFFER, 0, 0.25);
    let uniform = g.backend.rings[&(2, 0)].clone();
    assert_eq!(f32::from_le_bytes(uniform[80..84].try_into().unwrap()), 0.25);
    assert!(g.backend.calls.iter().any(|c| matches!(c, Call::DrawBlit(BlitDraw { mode: BlitMode::Depth, .. }))));
}

#[test]
fn a_clear_without_a_target_fails() {
    let mut g = gx();
    let mut dev = Device::default();
    assert_eq!(device::clear(&mut g, &mut dev, None, GXCLEAR_TARGET, 0, 1.0), GXERR_GENERIC);
}

#[test]
fn the_null_device_skips_scenes_and_keeps_key_frames() {
    let mut g = gx();
    g.plat.env_vars.insert("EE_GX_NULL3D".into(), "100".into());
    let (_, mut dev, vb) = draw_setup(&mut g);
    g.plat.now_ms.set(1000);
    assert_eq!(device::begin_scene(&mut g), GX_OK);
    assert!(g.draw_scene, "the first scene is the first key frame");
    assert!(g.plat.logs.borrow().iter().any(|l| l == "GX: null 3D device, one scene every 100 ms"));
    device::draw(&mut g, &mut dev, GXPT_TRIANGLELIST, Some(&vb), 0, 3, None);
    assert_eq!(g.backend.count(|c| matches!(c, Call::Draw(_))), 1);
    g.plat.now_ms.set(1050);
    device::begin_scene(&mut g);
    assert!(!g.draw_scene);
    drained(&mut g);
    assert_eq!(device::draw(&mut g, &mut dev, GXPT_TRIANGLELIST, Some(&vb), 0, 3, None), GX_OK);
    assert_eq!(device::clear(&mut g, &mut dev, None, GXCLEAR_TARGET, 0, 0.0), GX_OK);
    assert!(g.backend.calls.is_empty(), "draws and clears are dropped");
    g.plat.now_ms.set(1100);
    device::begin_scene(&mut g);
    assert!(g.draw_scene);
    let mut g2 = gx();
    g2.plat.env_vars.insert("EE_GX_NULL3D".into(), "0".into());
    device::begin_scene(&mut g2);
    assert!(!g2.draw_scene, "0: never");
    let mut g3 = gx();
    device::begin_scene(&mut g3);
    assert!(g3.draw_scene, "unset: every scene is drawn");
    assert_eq!(g3.null3d_ms, -1);
}

#[test]
fn the_stats_switch_counts_draws_and_logs_every_n_scenes() {
    let mut g = gx();
    g.plat.env_vars.insert("EE_GX_STATS".into(), "2".into());
    let (_, mut dev, mut vb) = draw_setup(&mut g);
    for i in 0..3 {
        vb.vertices[i * 36 + 24..i * 36 + 28].copy_from_slice(&[1, 2, 3, 4]);
    }
    assert_eq!(device::begin_scene(&mut g), GX_OK);
    assert!(g.plat.logs.borrow().iter().all(|l| !l.starts_with("GX stats")));
    device::draw(&mut g, &mut dev, GXPT_TRIANGLELIST, Some(&vb), 0, 3, None);
    device::clear(&mut g, &mut dev, None, GXCLEAR_TARGET, 0, 0.0);
    assert_eq!(g.stats.vertices, 3);
    assert_eq!(g.stats.clears, 1);
    device::begin_scene(&mut g);
    let logs = g.plat.logs.borrow();
    assert_eq!(logs.iter().filter(|l| l.starts_with("GX stats")).count(), 3);
    assert!(logs.iter().any(|l| l.contains("2 scenes, 1 clears") && l.contains("triangles 1")));
    assert!(logs.iter().any(|l| l.contains("BGRA min 1,2,3,4 max 1,2,3,4")));
    drop(logs);
    assert_eq!(g.stats, crate::stats::DrawStats::default(), "the counters restart");
}

#[test]
fn no_stats_switch_means_no_accounting() {
    let mut g = gx();
    let (_, mut dev, vb) = draw_setup(&mut g);
    device::begin_scene(&mut g);
    device::draw(&mut g, &mut dev, GXPT_TRIANGLELIST, Some(&vb), 0, 3, None);
    assert_eq!(g.stats, crate::stats::DrawStats::default());
    assert_eq!(g.stats_every, -1);
}

#[test]
fn set_render_target_swaps_references() {
    let mut g = gx();
    let (a, mut dev, _) = draw_setup(&mut g);
    let b = target(&mut g, 2, 2);
    assert_eq!(g.s(a).references, 2);
    assert_eq!(device::set_render_target(&mut g, &mut dev, b), GX_OK);
    assert_eq!(g.s(a).references, 1);
    assert_eq!(g.s(b).references, 2);
    assert_eq!(device::set_texture(&mut g, &mut dev, 2, None), GXERR_INVALIDPARAMS);
    device::set_texture(&mut g, &mut dev, 0, Some(a));
    assert_eq!(g.s(a).references, 2);
    device::destroy_device(&mut g, &mut dev);
    assert_eq!(g.s(a).references, 1);
    assert_eq!(g.s(b).references, 1);
}

#[test]
fn settings_clamp_like_the_setters() {
    let mut g = gx();
    g.set_render_percent(10);
    g.plat.pixels = Some((1080, 1080));
    assert_eq!(g.render_scale(1080), 1.0);
    g.set_render_percent(200);
    assert_eq!(g.render_scale(540), 2.0, "clamped to 100%");
    g.set_texture_filtering(9);
    assert_eq!(g.texture_filtering, 2);
}

#[test]
fn render_scale_env_override_is_cached() {
    let mut g = gx();
    g.plat.env_vars.insert("EE_GX_RENDER_SCALE".into(), "3".into());
    g.plat.pixels = Some((100, 100));
    assert_eq!(g.render_scale(100), 3.0);
    g.plat.env_vars.insert("EE_GX_RENDER_SCALE".into(), "1".into());
    assert_eq!(g.render_scale(100), 3.0, "read once");
}

#[test]
fn present_mode_selection_picks_the_last_of_immediate_and_mailbox() {
    let caps = |modes: Vec<wgpu::PresentMode>| SwapCaps { formats: vec![], present_modes: modes };
    assert_eq!(select_present_mode(&caps(vec![wgpu::PresentMode::Fifo])), wgpu::PresentMode::Fifo);
    assert_eq!(
        select_present_mode(&caps(vec![wgpu::PresentMode::Mailbox, wgpu::PresentMode::Immediate])),
        wgpu::PresentMode::Immediate
    );
    assert_eq!(
        select_present_mode(&caps(vec![wgpu::PresentMode::Immediate, wgpu::PresentMode::Mailbox])),
        wgpu::PresentMode::Mailbox
    );
}

#[test]
fn swap_format_selection_prefers_bgra_else_the_first() {
    let caps = |formats: Vec<wgpu::TextureFormat>| SwapCaps { formats, present_modes: vec![] };
    assert_eq!(
        select_swap_format(&caps(vec![wgpu::TextureFormat::Rgba8Unorm, wgpu::TextureFormat::Bgra8Unorm])),
        wgpu::TextureFormat::Bgra8Unorm
    );
    assert_eq!(select_swap_format(&caps(vec![wgpu::TextureFormat::Rgba8Unorm])), wgpu::TextureFormat::Rgba8Unorm);
    assert_eq!(select_swap_format(&caps(vec![])), wgpu::TextureFormat::Bgra8Unorm);
}

#[test]
fn instance_flags_translate_the_c_values() {
    assert_eq!(instance_flags(0), wgpu::InstanceFlags::empty());
    assert_eq!(instance_flags(1), wgpu::InstanceFlags::DEBUG);
    assert_eq!(instance_flags(2), wgpu::InstanceFlags::VALIDATION);
    assert_eq!(instance_flags(3), wgpu::InstanceFlags::DEBUG | wgpu::InstanceFlags::VALIDATION);
    assert_eq!(instance_flags(4), wgpu::InstanceFlags::DISCARD_HAL_LABELS);
}
