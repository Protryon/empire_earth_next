//! `GXDirectDraw` (IDirectDraw7 + IDirect3D7): the display object, its modes and `CreateSurface`.

use std::ffi::c_void;

use crate::abi::*;
use crate::device::Device;
use crate::format::Format;
use crate::gx::{mode_format, Backend, Gx, Platform};
use crate::modes;
use crate::surface::{Surface, SurfaceId};

pub struct DirectDraw {
    pub references: u32,
    pub window: *mut c_void,
    pub cooperative_flags: u32,
    pub mode_width: u32,
    pub mode_height: u32,
    pub mode_bits: u32,
}

impl DirectDraw {
    /// A display object at the desktop's size in 32 bits.
    pub fn new(info: &modes::DisplayInfo) -> DirectDraw {
        let (mode_width, mode_height) = modes::desktop_size(info);
        DirectDraw {
            references: 1,
            window: std::ptr::null_mut(),
            cooperative_flags: 0,
            mode_width,
            mode_height,
            mode_bits: 32,
        }
    }

    pub fn display_mode(&self) -> GxSurfaceDesc {
        GxSurfaceDesc {
            dwSize: std::mem::size_of::<GxSurfaceDesc>() as _,
            dwFlags: (GXSD_WIDTH | GXSD_HEIGHT | GXSD_PIXELFORMAT) as _,
            dwWidth: self.mode_width as _,
            dwHeight: self.mode_height as _,
            dwRefreshRate: 60,
            ddpfPixelFormat: mode_format(self.mode_bits),
            ..Default::default()
        }
    }
}

/// One entry of `EnumDisplayModes`.
pub fn mode_description(width: u32, height: u32, bits: u32) -> GxSurfaceDesc {
    GxSurfaceDesc {
        dwSize: std::mem::size_of::<GxSurfaceDesc>() as _,
        dwFlags: (GXSD_WIDTH | GXSD_HEIGHT | GXSD_PIXELFORMAT) as _,
        dwWidth: width as _,
        dwHeight: height as _,
        dwRefreshRate: 60,
        ddpfPixelFormat: mode_format(bits),
        ..Default::default()
    }
}

/// `GXDirectDraw::CreateSurface`.
pub fn create_surface<B: Backend, P: Platform>(
    gx: &mut Gx<B, P>,
    dd: &DirectDraw,
    desc: &GxSurfaceDesc,
) -> Result<SurfaceId, GxResult> {
    let caps = desc.ddsCaps.dwCaps as u32;
    let flags = desc.dwFlags as u32;
    let mut s = Surface {
        caps,
        caps2: desc.ddsCaps.dwCaps2 as u32,
        is_primary: caps & GXSCAPS_PRIMARYSURFACE != 0,
        is_z: caps & GXSCAPS_ZBUFFER != 0,
        is_texture: caps & GXSCAPS_TEXTURE != 0,
        ..Default::default()
    };
    s.is_render_target = caps & GXSCAPS_3DDEVICE != 0 && !s.is_texture;
    if s.is_primary {
        s.width = dd.mode_width;
        s.height = dd.mode_height;
        s.pixel_format = mode_format(dd.mode_bits);
        s.window = dd.window;
        if !dd.window.is_null() {
            s.native = gx.plat.native_window(dd.window);
        }
    } else {
        s.width = desc.dwWidth as u32;
        s.height = desc.dwHeight as u32;
        s.pixel_format = if flags & GXSD_PIXELFORMAT != 0 { desc.ddpfPixelFormat } else { mode_format(dd.mode_bits) };
    }
    s.format = Format::set(&s.pixel_format);
    // a render target or Z buffer of the display mode's size is (or goes with) the back buffer: the render scale
    if (s.is_render_target || s.is_z) && s.width == dd.mode_width && s.height == dd.mode_height {
        s.scale = gx.render_scale(s.height);
    }
    if !s.is_z && !s.is_primary && s.format.bits != 16 && s.format.bits != 32 {
        // DXT1 and other formats the renderer never creates
        return Err(GXERR_UNSUPPORTED);
    }
    if s.is_z {
        s.cpu_valid = false;
        s.gpu_valid = true;
    }
    if flags & GXSD_CKSRCBLT != 0 {
        s.has_key = true;
        s.key = desc.ddckCKSrcBlt.dwColorSpaceLowValue as u32;
    }
    let levels = desc.dwMipMapCount as u32;
    let mips = caps & GXSCAPS_MIPMAP != 0 && flags & GXSD_MIPMAPCOUNT != 0 && levels > 1;
    if mips {
        s.mip_levels = levels;
    }
    let (is_primary, width, height, pixel_format, format) = (s.is_primary, s.width, s.height, s.pixel_format, s.format);
    let id = gx.add_surface(s);
    if mips {
        let mut parent = id;
        for i in 1..levels {
            let level = gx.add_surface(Surface {
                caps,
                is_texture: true,
                width: (width >> i).max(1),
                height: (height >> i).max(1),
                pixel_format,
                format,
                mip_top: Some(id),
                mip_level: i,
                ..Default::default()
            });
            gx.m(parent).attached.push(level);
            parent = level;
        }
    }
    if is_primary && caps & GXSCAPS_FLIP != 0 {
        let scale = gx.render_scale(height);
        let back = gx.add_surface(Surface {
            caps: (caps & !(GXSCAPS_PRIMARYSURFACE | GXSCAPS_FLIP)) | GXSCAPS_3DDEVICE,
            is_render_target: true,
            width,
            height,
            pixel_format,
            format,
            scale,
            ..Default::default()
        });
        gx.m(id).attached.push(back); // one back buffer is enough: frames are presented
    }
    Ok(id)
}

/// `GXDirectDraw::CreateDevice`: a device on `target`, the viewport its whole size.
pub fn create_device<B: Backend, P: Platform>(gx: &mut Gx<B, P>, target: SurfaceId) -> Device {
    let mut device = Device::default();
    crate::device::set_render_target(gx, &mut device, target);
    device.viewport = GxViewport {
        dwWidth: gx.s(target).width as _,
        dwHeight: gx.s(target).height as _,
        dvMaxZ: 1.0,
        ..Default::default()
    };
    device.uniform_dirty = true;
    device
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::fake::{FakeBackend, FakePlatform};
    use crate::modes::DisplayInfo;

    fn dd() -> DirectDraw {
        DirectDraw::new(&DisplayInfo { logical: None, pixels: Some((1920, 1080)) })
    }

    #[test]
    fn a_new_display_object_is_at_the_desktop_size() {
        let d = dd();
        assert_eq!((d.mode_width, d.mode_height, d.mode_bits), (1920, 1080, 32));
        let d = DirectDraw::new(&DisplayInfo::default());
        assert_eq!((d.mode_width, d.mode_height), (1024, 768));
    }

    #[test]
    fn mode_descriptions_carry_size_refresh_and_pixel_format() {
        let m = mode_description(800, 600, 16);
        assert_eq!((m.dwWidth, m.dwHeight, m.dwRefreshRate), (800, 600, 60));
        assert_eq!(m.ddpfPixelFormat.dwRBitMask, 0xf800);
        assert_eq!(mode_description(1, 1, 32).ddpfPixelFormat.dwRBitMask, 0xff0000);
    }

    fn gx() -> Gx<FakeBackend, FakePlatform> {
        Gx::new(FakeBackend::default(), FakePlatform::default())
    }

    fn desc(width: u32, height: u32, caps: u32) -> GxSurfaceDesc {
        GxSurfaceDesc {
            dwWidth: width as _,
            dwHeight: height as _,
            ddsCaps: GxSCaps { dwCaps: caps as _, dwCaps2: 0 },
            ..Default::default()
        }
    }

    #[test]
    fn surfaces_take_the_mode_format_by_default() {
        let mut g = gx();
        let d = DirectDraw { mode_bits: 16, ..dd() };
        let id = create_surface(&mut g, &d, &desc(10, 10, 0)).unwrap();
        assert_eq!(g.s(id).pixel_format.dwRBitMask, 0xf800);
        assert_eq!(g.s(id).format.bits, 16);
    }

    #[test]
    fn surfaces_of_an_unknown_bit_depth_are_refused() {
        let mut g = gx();
        let mut d = desc(10, 10, 0);
        d.dwFlags = GXSD_PIXELFORMAT as _;
        d.ddpfPixelFormat = GxPixelFormat::rgb(8, 0xe0, 0x1c, 0x3);
        assert_eq!(create_surface(&mut g, &dd(), &d).err(), Some(GXERR_UNSUPPORTED));
        assert_eq!(g.live_surfaces(), 0, "the refused surface is released");
    }

    #[test]
    fn z_surfaces_start_on_the_gpu() {
        let mut g = gx();
        let id = create_surface(&mut g, &dd(), &desc(10, 10, GXSCAPS_ZBUFFER)).unwrap();
        assert!(!g.s(id).cpu_valid);
        assert!(g.s(id).gpu_valid);
    }

    #[test]
    fn the_source_color_key_comes_from_the_description() {
        let mut g = gx();
        let mut d = desc(10, 10, GXSCAPS_TEXTURE);
        d.dwFlags = GXSD_CKSRCBLT as _;
        d.ddckCKSrcBlt.dwColorSpaceLowValue = 0xf81f;
        let id = create_surface(&mut g, &dd(), &d).unwrap();
        assert!(g.s(id).has_key);
        assert_eq!(g.s(id).key, 0xf81f);
    }

    #[test]
    fn mip_chains_are_linked_and_halved_down() {
        let mut g = gx();
        let mut d = desc(16, 8, GXSCAPS_TEXTURE | GXSCAPS_MIPMAP);
        d.dwFlags = GXSD_MIPMAPCOUNT as _;
        d.dwMipMapCount = 5;
        d.ddpfPixelFormat = GxPixelFormat::rgb(16, 0xf800, 0x7e0, 0x1f);
        let id = create_surface(&mut g, &dd(), &d).unwrap();
        assert_eq!(g.s(id).mip_levels, 5);
        assert_eq!(g.live_surfaces(), 5);
        let mut level = g.s(id).attached[0];
        let mut expect = [(8, 4), (4, 2), (2, 1), (1, 1)].iter();
        loop {
            let (w, h) = *expect.next().unwrap();
            assert_eq!((g.s(level).width, g.s(level).height), (w, h));
            assert_eq!(g.s(level).mip_top, Some(id));
            assert!(g.s(level).is_texture);
            if let Some(&next) = g.s(level).attached.first() {
                level = next;
            } else {
                break;
            }
        }
        assert_eq!(g.s(level).mip_level, 4);
    }

    #[test]
    fn a_flipping_primary_gets_a_scaled_back_buffer() {
        let mut g = gx();
        g.plat.pixels = Some((1920, 1080));
        let d = DirectDraw { mode_width: 1024, mode_height: 768, ..dd() };
        let id = create_surface(&mut g, &d, &desc(0, 0, GXSCAPS_PRIMARYSURFACE | GXSCAPS_FLIP)).unwrap();
        let back = g.s(id).attached[0];
        assert!(g.s(id).is_primary);
        assert_eq!((g.s(id).width, g.s(id).height), (1024, 768));
        assert!(g.s(back).is_render_target);
        assert_eq!(g.s(back).caps & GXSCAPS_FLIP, 0);
        assert_eq!(g.s(back).caps & GXSCAPS_3DDEVICE, GXSCAPS_3DDEVICE);
        assert!((g.s(back).scale - 1080.0 / 768.0).abs() < 1e-6);
    }

    #[test]
    fn a_render_target_at_the_mode_size_is_scaled_and_others_are_not() {
        let mut g = gx();
        g.plat.pixels = Some((1920, 1080));
        let d = DirectDraw { mode_width: 1024, mode_height: 768, ..dd() };
        let at_mode = create_surface(&mut g, &d, &desc(1024, 768, GXSCAPS_3DDEVICE)).unwrap();
        assert!(g.s(at_mode).scale > 1.0);
        let small = create_surface(&mut g, &d, &desc(32, 32, GXSCAPS_3DDEVICE)).unwrap();
        assert_eq!(g.s(small).scale, 1.0);
        let z = create_surface(&mut g, &d, &desc(1024, 768, GXSCAPS_ZBUFFER)).unwrap();
        assert!(g.s(z).scale > 1.0);
        let texture = create_surface(&mut g, &d, &desc(1024, 768, GXSCAPS_TEXTURE)).unwrap();
        assert_eq!(g.s(texture).scale, 1.0);
    }

    #[test]
    fn create_device_targets_the_surface_with_a_whole_viewport() {
        let mut g = gx();
        let target = create_surface(&mut g, &dd(), &desc(64, 32, GXSCAPS_3DDEVICE)).unwrap();
        let device = create_device(&mut g, target);
        assert_eq!(device.target, Some(target));
        assert_eq!((device.viewport.dwWidth, device.viewport.dwHeight), (64, 32));
        assert_eq!(device.viewport.dvMaxZ, 1.0);
        assert_eq!(g.s(target).references, 2);
        assert!(g.s(target).is_render_target);
    }
}
