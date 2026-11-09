//! `DX7Surface` (`Common/DX7Surface.cpp`): the GX calls with their retry loops and result mapping and the
//! logical-rectangle conversion. The C++ class keeps the fields (`mSurface`, the harness reads it), the
//! attached-surface vector and the `GESurface` reference counts; the bilinear copies are in `filter.rs`.
//!
//! Golden: `golden_surface.txt` is the call trace printed by the real `DX7Surface.cpp` compiled with clang-cl against a
//! fake of `GX.h` (scratch generator, 2026-11-09), replayed here on the recorder.

use core::ffi::c_void;

use crate::api::*;
use crate::filter::Rect;

/// `GESurface`'s logical-size members and the surface's pixel size (`GetLogicalShift`, `SetLogicalSize`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct LogicalMetrics {
    pub logical_width: u32,
    pub logical_height: u32,
    pub pixel_width: u32,
    pub pixel_height: u32,
    pub logical_shift: u32,
}

/// `DX7PixelRectangle`: a logical rectangle -> the surface's pixels; `None`: no surface (unchanged).
pub fn pixel_rect(rect: Rect, metrics: Option<&LogicalMetrics>) -> Rect {
    let Some(m) = metrics else { return rect };
    if m.logical_width != 0 && m.logical_height != 0 {
        let x = m.pixel_width as f32 / m.logical_width as f32;
        let y = m.pixel_height as f32 / m.logical_height as f32;
        return Rect { left: rect.left * x, top: rect.top * y, right: rect.right * x, bottom: rect.bottom * y };
    }
    if m.logical_shift == 0 {
        return rect;
    }
    let scale = (1u32 << m.logical_shift) as f32;
    Rect { left: rect.left * scale, top: rect.top * scale, right: rect.right * scale, bottom: rect.bottom * scale }
}

/// `(long)` of each edge (`theRect.left = (long)inRectangle.GetLeft()`, ...).
pub fn to_irect(rect: Rect) -> IRect {
    IRect {
        left: trunc_to_long(rect.left),
        top: trunc_to_long(rect.top),
        right: trunc_to_long(rect.right),
        bottom: trunc_to_long(rect.bottom),
    }
}

fn map_lost(error: i32, also_busy: bool, host: &mut impl Host) -> i32 {
    if error == GX_OK {
        K_OK
    } else if error == GXERR_SURFACELOST || (also_busy && error == GXERR_SURFACEBUSY) {
        K_NOT_SUPPORTED
    } else {
        host.record_native_error(error);
        K_NATIVE
    }
}

/// `DX7Surface::Clear` (both forms; `rect` is `None` for the whole surface), `fill` packed by the C++.
pub fn clear<G: Gx>(gx: &mut G, host: &mut impl Host, s: H, rect: Option<IRect>, fill: u32) -> i32 {
    let error = retry_still_drawing(host, 0, || gx.surface_blt(s, rect, NULL, None, GXBLT_COLORFILL, Some(fill)));
    map_lost(error, false, host)
}

/// `DX7Surface::CopyTo`: `target` is the destination's `GXSurface*`; the RECTs are in pixels.
pub fn copy_to<G: Gx>(
    gx: &mut G,
    host: &mut impl Host,
    source: H,
    target: H,
    source_rect: IRect,
    destination_rect: IRect,
    transparent: bool,
) -> i32 {
    let flags = if transparent { GXBLT_DONOTWAIT | GXBLT_KEYSRC } else { GXBLT_DONOTWAIT };
    let error = retry_still_drawing(host, 0, || {
        gx.surface_blt(target, Some(destination_rect), source, Some(source_rect), flags, None)
    });
    map_lost(error, true, host)
}

/// `DX7Surface::Flip`.
pub fn flip<G: Gx>(gx: &mut G, host: &mut impl Host, s: H, wait: bool) -> i32 {
    let flags = if wait { GXFLIP_DONOTWAIT } else { GXFLIP_DONOTWAIT | GXFLIP_NOVSYNC };
    let error = retry_still_drawing(host, 0, || gx.surface_flip(s, flags));
    map_lost(error, false, host)
}

/// The result of `DX7Surface::Lock` (the C++ stores the rest in its fields when `locked`).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct LockOutcome {
    pub result: i32,
    pub memory: *mut c_void,
    pub pitch: i32,
    pub locked: bool,
}

/// `DX7Surface::Lock`.
pub fn lock<G: Gx>(gx: &mut G, host: &mut impl Host, s: H) -> LockOutcome {
    let locked = loop {
        let locked = gx.surface_lock(s);
        if locked.result != GXERR_WASSTILLDRAWING {
            break locked;
        }
        host.sleep(0);
    };
    if locked.result == GX_OK {
        LockOutcome { result: K_OK, memory: locked.memory, pitch: locked.pitch, locked: true }
    } else if locked.result == GXERR_SURFACELOST {
        LockOutcome { result: K_NOT_SUPPORTED, memory: NULL, pitch: 0, locked: false }
    } else {
        host.record_native_error(locked.result);
        LockOutcome { result: K_NATIVE, memory: NULL, pitch: 0, locked: false }
    }
}

/// `DX7Surface::Unlock`: (result, whether `mIsLocked` is cleared).
pub fn unlock<G: Gx>(gx: &mut G, host: &mut impl Host, s: H) -> (i32, bool) {
    let error = gx.surface_unlock(s);
    if error != GX_OK && error != GXERR_NOTLOCKED {
        host.record_native_error(error);
        (K_NATIVE, false)
    } else {
        (K_OK, true)
    }
}

/// `DX7Surface::RestoreSurface`.
pub fn restore<G: Gx>(gx: &mut G, host: &mut impl Host, s: H) -> i32 {
    let error = gx.surface_restore(s);
    if error != GX_OK {
        host.record_native_error(error);
        K_NATIVE
    } else {
        K_OK
    }
}

/// `DX7Surface::AttachSurface`'s GX call; OK -> the C++ records the reference.
pub fn attach<G: Gx>(gx: &mut G, host: &mut impl Host, s: H, other: H) -> i32 {
    let error = gx.surface_add_attached(s, other);
    if error == GX_OK {
        K_OK
    } else {
        host.record_native_error(error);
        K_NATIVE
    }
}

/// `DX7Surface::DetachSurface`'s GX call; OK -> the C++ erases and dereferences.
pub fn detach<G: Gx>(gx: &mut G, host: &mut impl Host, s: H, other: H) -> i32 {
    let error = gx.surface_delete_attached(s, other);
    if error == GX_OK {
        K_OK
    } else {
        host.record_native_error(error);
        K_NATIVE
    }
}

/// `DX7Surface::SetTransparencyColor`'s key (`GXCKEY_SRCBLT`, low = high = `color` packed by the C++).
pub fn set_transparency_color<G: Gx>(gx: &mut G, s: H, color: u32) {
    gx.surface_set_color_key(s, color);
}

/// `DX7Surface::GetAllocatedSize`'s own part: depth / 8 * height * width (`unsigned long` wrap-around).
pub fn own_size(pixel_depth: u32, height: u32, width: u32) -> u32 {
    (pixel_depth / 8).wrapping_mul(height).wrapping_mul(width)
}

/// `TextLock`: the new `mIsTextLocked` and the result.
pub fn text_lock(locked: bool) -> (bool, i32) {
    (true, if locked { K_ALREADY } else { K_OK })
}

/// `TextUnlock`.
pub fn text_unlock(locked: bool) -> (bool, i32) {
    (false, if locked { K_OK } else { K_ALREADY })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::api::test_recorder::*;

    fn rect(l: f32, t: f32, r: f32, b: f32) -> Rect {
        Rect { left: l, top: t, right: r, bottom: b }
    }

    const S1: H = 1 as H;
    const S2: H = 2 as H;
    const R10: IRect = IRect { left: 1, top: 2, right: 10, bottom: 12 };
    const R20: IRect = IRect { left: 3, top: 5, right: 21, bottom: 25 };

    fn run(name: &str, gx: &mut Recorder, h: &mut RecHost) -> i32 {
        match name {
            "clear_whole_retry" => {
                gx.queue("blt", &[GXERR_WASSTILLDRAWING, GXERR_WASSTILLDRAWING, GX_OK]);
                clear(gx, h, S1, None, 0x1122_3344)
            }
            "clear_rect_lost" => {
                gx.queue("blt", &[GXERR_SURFACELOST]);
                clear(gx, h, S1, Some(R10), 0xaa)
            }
            "clear_rect_err" => {
                gx.queue("blt", &[0x123]);
                clear(gx, h, S1, Some(R10), 7)
            }
            "copy_ok" => copy_to(gx, h, S1, S2, R10, R20, false),
            "copy_key_busy" => {
                gx.queue("blt", &[GXERR_SURFACEBUSY]);
                copy_to(gx, h, S1, S2, R10, R20, true)
            }
            "copy_lost" => {
                gx.queue("blt", &[GXERR_SURFACELOST]);
                copy_to(gx, h, S1, S2, R10, R20, false)
            }
            "copy_err" => {
                gx.queue("blt", &[0x77]);
                copy_to(gx, h, S1, S2, R10, R20, false)
            }
            "flip_wait_retry" => {
                gx.queue("flip", &[GXERR_WASSTILLDRAWING, GX_OK]);
                flip(gx, h, S1, true)
            }
            "flip_novsync_lost" => {
                gx.queue("flip", &[GXERR_SURFACELOST]);
                flip(gx, h, S1, false)
            }
            "lock_retry" => {
                gx.queue("lock", &[GXERR_WASSTILLDRAWING, GX_OK]);
                let o = lock(gx, h, S1);
                assert!(o.locked);
                assert_eq!(o.memory as usize, 0x1000);
                assert_eq!(o.pitch, 1280);
                o.result
            }
            "lock_lost" => {
                gx.queue("lock", &[GXERR_SURFACELOST]);
                lock(gx, h, S1).result
            }
            "lock_err" => {
                gx.queue("lock", &[0x55]);
                lock(gx, h, S1).result
            }
            "unlock_ok" => unlock(gx, h, S1).0,
            "unlock_notlocked" => {
                gx.queue("unlock", &[GXERR_NOTLOCKED]);
                let (r, cleared) = unlock(gx, h, S1);
                assert!(cleared);
                r
            }
            "unlock_err" => {
                gx.queue("unlock", &[0x56]);
                let (r, cleared) = unlock(gx, h, S1);
                assert!(!cleared);
                r
            }
            "attach_ok" => attach(gx, h, S1, S2),
            "attach_err" => {
                gx.queue("add_attached", &[0x10]);
                attach(gx, h, S1, S2)
            }
            "detach_ok" => detach(gx, h, S1, S2),
            "detach_err" => {
                gx.queue("delete_attached", &[0x11]);
                detach(gx, h, S1, S2)
            }
            "restore_ok" => restore(gx, h, S1),
            "colorkey" => {
                set_transparency_color(gx, S1, 0xf81f);
                0
            }
            other => panic!("unknown scenario {other}"),
        }
    }

    // golden_surface.txt: "# name", the trace lines, "= result".
    #[test]
    fn matches_cpp_golden() {
        let sections = golden_sections(include_str!("golden_surface.txt"));
        assert!(sections.len() >= 20, "golden sections read: {}", sections.len());
        for section in sections {
            let (mut gx, mut host) = Recorder::pair();
            let result = run(&section.name, &mut gx, &mut host);
            let got: Vec<String> = gx.log.borrow().clone();
            assert_eq!(got, section.trace, "trace of {}", section.name);
            assert_eq!(result, section.result, "result of {}", section.name);
        }
    }

    #[test]
    fn text_lock_unlock_states() {
        assert_eq!(text_lock(false), (true, K_OK));
        assert_eq!(text_lock(true), (true, K_ALREADY));
        assert_eq!(text_unlock(true), (false, K_OK));
        assert_eq!(text_unlock(false), (false, K_ALREADY));
    }

    #[test]
    fn own_size_is_depth_bytes_times_area() {
        assert_eq!(own_size(32, 4, 3), 48);
        assert_eq!(own_size(16, 4, 3), 24);
    }

    #[test]
    fn pixel_rect_scales_and_truncates() {
        let m = LogicalMetrics {
            logical_width: 1024,
            logical_height: 768,
            pixel_width: 2048,
            pixel_height: 1536,
            logical_shift: 0,
        };
        let p = pixel_rect(rect(0.0, 0.0, 512.0, 384.0), Some(&m));
        assert_eq!((p.left, p.top, p.right, p.bottom), (0.0, 0.0, 1024.0, 768.0));
        let shifted = LogicalMetrics { logical_shift: 2, ..Default::default() };
        let p = pixel_rect(rect(1.5, 2.0, 3.5, 4.0), Some(&shifted));
        assert_eq!((p.left, p.top, p.right, p.bottom), (6.0, 8.0, 14.0, 16.0));
        let plain = LogicalMetrics::default();
        let p = pixel_rect(rect(1.5, 2.5, 3.5, 4.5), Some(&plain));
        assert_eq!((p.left, p.right), (1.5, 3.5));
        let p = pixel_rect(rect(1.5, 2.5, 3.5, 4.5), None);
        assert_eq!((p.left, p.right), (1.5, 3.5));
        assert_eq!(to_irect(rect(1.9, -1.9, 3.5, 4.5)), IRect { left: 1, top: -1, right: 3, bottom: 4 });
    }

    #[test]
    fn trunc_to_long_edge_values() {
        assert_eq!(trunc_to_long(1.9), 1);
        assert_eq!(trunc_to_long(-1.9), -1);
        assert_eq!(trunc_to_long(f32::NAN), i32::MIN);
        assert_eq!(trunc_to_long(3e9), i32::MIN);
        assert_eq!(trunc_to_long(-3e9), i32::MIN);
    }
}
