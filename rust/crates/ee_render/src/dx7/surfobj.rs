//! `DX7Surface` whole (`Common/DX7Surface.cpp`, deleted): the description, the attached surfaces, the transparency
//! colour, the GX surface, the parent screen, the lock's memory and pitch and the lock flags, with every method of the
//! class over [`crate::surface`]'s GX calls and [`crate::filter`]'s bilinear copies. The C++ class keeps its vtable
//! (the game calls it through `GESurface`), the `GESurface` base (the reference count, the logical-size members) and
//! one member, the object; every call on another C++ object is one host operation ([`EeDx7ObjOp`], shared with
//! [`crate::scrobj`]).
//!
//! No reference to an object lives across a host call: a dereference may delete a surface (its destructor runs
//! [`EeDx7Surface::destroy`]), so each method works on the raw pointer and copies out what it needs first.
#![allow(clippy::missing_safety_doc, clippy::not_unsafe_ptr_arg_deref, clippy::unnecessary_cast)] // c_ulong differs per target

use core::ffi::{c_ulong, c_void};

use crate::ge::pixel;

use crate::dx7::api::*;
use crate::dx7::filter::{copy_with_filter, PixelOps, Rect};
use crate::dx7::surface::{self, LogicalMetrics};

/// The calls `DX7Screen` and `DX7Surface` make on C++ objects. `obj` is the object called; `a`, `b` as each says.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeDx7ObjOp {
    /// `new UString(*(const UString*)obj)`.
    NameNew = 1,
    /// `delete (UString*)obj`.
    NameDelete,
    /// `new DX7Surface((GXSurface*)obj, *(const GESurfaceDescription*)a, (DX7Screen*)b)`, as its `GESurface*`.
    SurfaceNew,
    /// The object of `static_cast<DX7Surface*>((GESurface*)obj)`.
    SurfaceObject,
    /// `obj->Reference()` (a surface).
    SurfaceReference,
    /// `obj->Dereference()` (a surface).
    SurfaceDereference,
    /// `obj->GetAllocatedSize()`.
    SurfaceAllocatedSize,
    /// `*(LogicalMetrics*)a`: the surface's logical-size members and its pixel size.
    SurfaceMetrics,
    /// `((DX7Screen*)obj)->CreateAWindow({0, 0}, 16, 16, "TestWindowClass", "", *(GEWindow**)a, 2, NULL)`.
    ProbeWindow,
    /// `((GEWindow*)obj)->GetOSRepresentation()`.
    WindowOs,
    /// `delete (GEWindow*)obj`.
    WindowDelete,
    /// `TSThread::DispatchWindowEvents()`.
    DispatchEvents,
}

/// The host of the screen and surface objects.
pub type EeDx7ObjHostFn = unsafe extern "C" fn(op: EeDx7ObjOp, obj: *mut c_void, a: isize, b: isize) -> isize;

/// `GEUnpack*`/`GEPackColor` of the bilinear copy: the Graphics Engine's format tables (`ee_ge::pixel`).
struct FormatOps(c_ulong);

#[allow(clippy::unnecessary_cast)]
impl PixelOps for FormatOps {
    fn unpack_red(&self, p: u32) -> u8 {
        pixel::unpack_red(self.0, p as c_ulong)
    }
    fn unpack_green(&self, p: u32) -> u8 {
        pixel::unpack_green(self.0, p as c_ulong)
    }
    fn unpack_blue(&self, p: u32) -> u8 {
        pixel::unpack_blue(self.0, p as c_ulong)
    }
    fn unpack_alpha(&self, p: u32) -> u8 {
        pixel::unpack_alpha(self.0, p as c_ulong)
    }
    fn pack(&self, red: i32, green: i32, blue: i32, alpha: i32) -> u32 {
        // `GEPackColor(format, (unsigned long)(long)red, ...)`
        pixel::pack(self.0, red as c_ulong, green as c_ulong, blue as c_ulong, alpha as c_ulong) as u32
    }
}

/// A `GERectangle` in memory (bottom, left, right, top) as the left, top, right, bottom the algorithms take.
///
/// # Safety
/// `r` points at a `GERectangle`.
pub unsafe fn ge_rect(r: *const f32) -> Rect {
    // SAFETY: the caller's contract
    unsafe { Rect { left: *r.add(1), top: *r.add(3), right: *r.add(2), bottom: *r } }
}

/// The surface.
pub struct EeDx7Surface {
    host: EeDx7ObjHostFn,
    /// The C++ surface (`this`, as its `GESurface*`).
    owner: H,
    /// `mDescription`.
    pub desc: SurfaceDescription,
    /// `mAttachedSurfaces` (`GESurface*`).
    pub attached: Vec<H>,
    /// `mTransparencyColor` (a `GEColor`: red, green, blue).
    pub transparency: [f32; 3],
    /// `mSurface`.
    pub gx: H,
    /// `mParentScreen` (a `GEScreen*`).
    pub parent: H,
    /// `mMemoryAddress` (`Lock`).
    pub memory: *mut c_void,
    /// `mBytesPerRow` (`Lock`).
    pub bytes_per_row: c_ulong,
    /// `mIsLocked`.
    pub locked: bool,
    /// `mIsTextLocked`.
    pub text_locked: bool,
}

/// # Safety
/// `p` is live; the reference is not held across a host call.
unsafe fn sf<'a>(p: *mut EeDx7Surface) -> &'a mut EeDx7Surface {
    // SAFETY: the caller's contract
    unsafe { &mut *p }
}

impl EeDx7Surface {
    /// A host call made by surface `p`.
    ///
    /// # Safety
    /// `p` is live; the operation's arguments are as [`EeDx7ObjOp`] describes.
    unsafe fn op(p: *mut Self, op: EeDx7ObjOp, obj: H, a: isize, b: isize) -> isize {
        // SAFETY: the caller's contract (the host is copied out before the call)
        let host = unsafe { sf(p).host };
        // SAFETY: the host's contract
        unsafe { host(op, obj, a, b) }
    }

    /// The object of another surface (`static_cast<DX7Surface*>(surface)`).
    ///
    /// # Safety
    /// `p` is live; `surface` is a live `DX7Surface`.
    pub unsafe fn object_of(p: *mut Self, surface: H) -> *mut Self {
        // SAFETY: the caller's contract
        unsafe { Self::op(p, EeDx7ObjOp::SurfaceObject, surface, 0, 0) as *mut Self }
    }

    /// `GESurface`'s logical members and the pixel size of `surface` (`DX7Metrics`).
    ///
    /// # Safety
    /// `p` is live; `surface` is a live surface.
    unsafe fn metrics(p: *mut Self, surface: H) -> LogicalMetrics {
        let mut m = LogicalMetrics::default();
        // SAFETY: the caller's contract
        unsafe { Self::op(p, EeDx7ObjOp::SurfaceMetrics, surface, &mut m as *mut LogicalMetrics as isize, 0) };
        m
    }

    /// `DX7Surface(inSurface, inDescription, inParentScreen)`: unlocked, no attachments.
    pub fn create(host: EeDx7ObjHostFn, owner: H, gx: H, desc: SurfaceDescription, parent: H) -> *mut Self {
        Box::into_raw(Box::new(EeDx7Surface {
            host,
            owner,
            desc,
            attached: Vec::new(),
            transparency: [1.0; 3], // GEColor(): white
            gx,
            parent,
            memory: NULL,
            bytes_per_row: 0,
            locked: false,
            text_locked: false,
        }))
    }

    /// `~DX7Surface`: ends any text lock, dereferences the attached surfaces, releases the GX surface (not a back
    /// buffer's: the flip chain owns it), then frees the object.
    ///
    /// # Safety
    /// `p` was made by [`Self::create`] and is not used again.
    pub unsafe fn destroy<G: Gx>(p: *mut Self, gx: &mut G) {
        // SAFETY: the caller's contract
        unsafe {
            Self::text_unlock(p);
            let attached = sf(p).attached.clone();
            for surface in attached {
                Self::op(p, EeDx7ObjOp::SurfaceDereference, surface, 0, 0);
            }
            if sf(p).desc.mType != 7 {
                gx.surface_release(sf(p).gx);
            }
            drop(Box::from_raw(p));
        }
    }

    /// `AttachSurface`: attaches `surface` and keeps a reference to it; native error on failure.
    ///
    /// # Safety
    /// `p` is live; `surface` is a live `DX7Surface`.
    pub unsafe fn attach<G: Gx, O: Host>(p: *mut Self, gx: &mut G, host: &mut O, surface: H) -> i32 {
        // SAFETY: the caller's contract
        unsafe {
            let other = sf(Self::object_of(p, surface)).gx;
            let result = surface::attach(gx, host, sf(p).gx, other);
            if result == K_OK {
                Self::op(p, EeDx7ObjOp::SurfaceReference, surface, 0, 0);
                sf(p).attached.push(surface);
            }
            result
        }
    }

    /// `Clear` (both forms; `rect` a `GERectangle` in logical units, NULL: the whole surface).
    ///
    /// # Safety
    /// `p` is live; `rect` is NULL or a `GERectangle`.
    pub unsafe fn clear<G: Gx, O: Host>(
        p: *mut Self,
        gx: &mut G,
        host: &mut O,
        rect: *const f32,
        color: [f32; 3],
        alpha: f32,
    ) -> i32 {
        // SAFETY: the caller's contract
        unsafe {
            let fill = pixel::pack_float(sf(p).desc.mPixelFormat, color[0], color[1], color[2], alpha) as u32;
            let pixels = if rect.is_null() {
                None
            } else {
                let m = Self::metrics(p, sf(p).owner);
                Some(surface::to_irect(surface::pixel_rect(ge_rect(rect), Some(&m))))
            };
            surface::clear(gx, host, sf(p).gx, pixels, fill)
        }
    }

    /// `CopyTo`: blits a logical source rectangle onto `target`'s logical rectangle, optionally colour-keyed.
    ///
    /// # Safety
    /// `p` is live; `target` is a live `DX7Surface`; the rectangles are `GERectangle`s.
    pub unsafe fn copy_to<G: Gx, O: Host>(
        p: *mut Self,
        gx: &mut G,
        host: &mut O,
        target: H,
        source: *const f32,
        destination: *const f32,
        transparent: bool,
    ) -> i32 {
        // SAFETY: the caller's contract
        unsafe {
            let source_metrics = Self::metrics(p, sf(p).owner);
            let target_metrics = Self::metrics(p, target);
            let s = surface::to_irect(surface::pixel_rect(ge_rect(source), Some(&source_metrics)));
            let d = surface::to_irect(surface::pixel_rect(ge_rect(destination), Some(&target_metrics)));
            let target_gx = sf(Self::object_of(p, target)).gx;
            surface::copy_to(gx, host, sf(p).gx, target_gx, s, d, transparent)
        }
    }

    /// `CopyWithFilter`: the logical rectangles in each surface's pixels, then the 32-bit or 16-bit bilinear copy by this
    /// surface's depth.
    ///
    /// # Safety
    /// `p` is live; `target` is a live `DX7Surface`; the rectangles are `GERectangle`s.
    pub unsafe fn copy_with_filter<G: Gx, O: Host>(
        p: *mut Self,
        gx: &mut G,
        host: &mut O,
        target: H,
        source: *const f32,
        destination: *const f32,
    ) -> i32 {
        // SAFETY: the caller's contract
        unsafe {
            let source_metrics = Self::metrics(p, sf(p).owner);
            let s = surface::pixel_rect(ge_rect(source), Some(&source_metrics));
            let target_metrics = Self::metrics(p, target);
            let d = surface::pixel_rect(ge_rect(destination), Some(&target_metrics));
            let t = Self::object_of(p, target);
            Self::copy_filtered(p, gx, host, t, s, d, sf(p).desc.mPixelDepth == 32)
        }
    }

    /// `Copy16BitWithFilter`/`Copy32BitWithFilter`: locks both, copies, unlocks; returns this surface's `Unlock`
    /// result (a failed target lock is not reported).
    ///
    /// # Safety
    /// `p` and `t` are live.
    #[allow(clippy::too_many_arguments)]
    unsafe fn copy_filtered<G: Gx, O: Host>(
        p: *mut Self,
        gx: &mut G,
        host: &mut O,
        t: *mut Self,
        s: Rect,
        d: Rect,
        wide: bool,
    ) -> i32 {
        // SAFETY: the caller's contract; the pixel memory is what the locks returned
        unsafe {
            let result = Self::lock(p, gx, host);
            if result != K_OK {
                return result;
            }
            if Self::lock(t, gx, host) == K_OK {
                let ops = FormatOps(sf(p).desc.mPixelFormat);
                let (source, target) = (sf(p).memory, sf(t).memory);
                if wide {
                    let (sp, tp) = ((sf(p).bytes_per_row / 4) as u32, (sf(t).bytes_per_row / 4) as u32);
                    let (source, target) = (source as *const u32, target as *mut u32);
                    copy_with_filter(
                        &ops,
                        s,
                        d,
                        sp,
                        tp,
                        |i| *source.offset(i as isize),
                        |i, v| *target.add(i as usize) = v,
                    );
                } else {
                    let (sp, tp) = ((sf(p).bytes_per_row / 2) as u32, (sf(t).bytes_per_row / 2) as u32);
                    let (source, target) = (source as *const u16, target as *mut u16);
                    copy_with_filter(
                        &ops,
                        s,
                        d,
                        sp,
                        tp,
                        |i| u32::from(*source.offset(i as isize)),
                        |i, v| *target.add(i as usize) = v as u16,
                    );
                }
                Self::unlock(t, gx, host);
            }
            Self::unlock(p, gx, host)
        }
    }

    /// `DetachSurface`: detaches the attached surface at `index` and drops its reference; native error on failure.
    ///
    /// # Safety
    /// `p` is live; `index` is one of its attached surfaces.
    pub unsafe fn detach<G: Gx, O: Host>(p: *mut Self, gx: &mut G, host: &mut O, index: usize) -> i32 {
        // SAFETY: the caller's contract
        unsafe {
            let surface = sf(p).attached[index];
            let other = sf(Self::object_of(p, surface)).gx;
            let result = surface::detach(gx, host, sf(p).gx, other);
            if result == K_OK {
                sf(p).attached.remove(index);
                Self::op(p, EeDx7ObjOp::SurfaceDereference, surface, 0, 0);
            }
            result
        }
    }

    /// `Flip`.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn flip<G: Gx, O: Host>(p: *mut Self, gx: &mut G, host: &mut O, wait: bool) -> i32 {
        // SAFETY: the caller's contract
        unsafe { surface::flip(gx, host, sf(p).gx, wait) }
    }

    /// `GetAllocatedSize`: this surface's bytes plus every attached surface's.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn allocated_size(p: *mut Self) -> c_ulong {
        // SAFETY: the caller's contract
        unsafe {
            let d = sf(p).desc;
            let mut size = surface::own_size(d.mPixelDepth as u32, d.mHeight as u32, d.mWidth as u32) as c_ulong;
            let mut i = 0;
            while i < sf(p).attached.len() {
                let child = sf(p).attached[i];
                size = size.wrapping_add(Self::op(p, EeDx7ObjOp::SurfaceAllocatedSize, child, 0, 0) as c_ulong);
                i += 1;
            }
            size
        }
    }

    /// `Lock`: records the memory address and row pitch.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn lock<G: Gx, O: Host>(p: *mut Self, gx: &mut G, host: &mut O) -> i32 {
        // SAFETY: the caller's contract
        unsafe {
            let outcome = surface::lock(gx, host, sf(p).gx);
            if outcome.locked {
                let s = sf(p);
                s.memory = outcome.memory;
                s.locked = true;
                s.bytes_per_row = outcome.pitch as c_ulong;
            }
            outcome.result
        }
    }

    /// `RestoreSurface`.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn restore<G: Gx, O: Host>(p: *mut Self, gx: &mut G, host: &mut O) -> i32 {
        // SAFETY: the caller's contract
        unsafe { surface::restore(gx, host, sf(p).gx) }
    }

    /// `SetTransparencyColor`: the source colour key of transparent blits.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn set_transparency_color<G: Gx>(p: *mut Self, gx: &mut G, color: [f32; 3]) {
        // SAFETY: the caller's contract
        unsafe {
            sf(p).transparency = color;
            let key = pixel::pack_float(sf(p).desc.mPixelFormat, color[0], color[1], color[2], 1.0) as u32;
            surface::set_transparency_color(gx, sf(p).gx, key);
        }
    }

    /// `TextLock`.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn text_lock(p: *mut Self) -> i32 {
        // SAFETY: the caller's contract
        let s = unsafe { sf(p) };
        let (next, result) = surface::text_lock(s.text_locked);
        s.text_locked = next;
        result
    }

    /// `TextUnlock`.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn text_unlock(p: *mut Self) -> i32 {
        // SAFETY: the caller's contract
        let s = unsafe { sf(p) };
        let (next, result) = surface::text_unlock(s.text_locked);
        s.text_locked = next;
        result
    }

    /// `Unlock` (an unlocked surface is not an error).
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn unlock<G: Gx, O: Host>(p: *mut Self, gx: &mut G, host: &mut O) -> i32 {
        // SAFETY: the caller's contract
        unsafe {
            let (result, cleared) = surface::unlock(gx, host, sf(p).gx);
            if cleared {
                sf(p).locked = false;
            }
            result
        }
    }

    /// `GetWidth`/`GetHeight`: the explicit logical size when set, else the pixels shifted by the logical shift (never
    /// below 1).
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn logical_size(p: *mut Self, set: c_ulong, shift: c_ulong, height: bool) -> c_ulong {
        // SAFETY: the caller's contract
        let d = unsafe { sf(p).desc };
        let pixels = if height { d.mHeight } else { d.mWidth };
        if set != 0 {
            set
        } else if shift == 0 || pixels.wrapping_shr(shift as u32) == 0 {
            pixels
        } else {
            pixels.wrapping_shr(shift as u32)
        }
    }
}

#[cfg(not(test))]
pub use ffi::*;

#[cfg(not(test))]
mod ffi {
    use super::*;
    use crate::dx7::gx::{RawGx, RawHost};

    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sf_new(
        host: EeDx7ObjHostFn,
        owner: *mut c_void,
        gx: *mut c_void,
        description: *const SurfaceDescription,
        parent: *mut c_void,
    ) -> *mut EeDx7Surface {
        EeDx7Surface::create(host, owner, gx, unsafe { *description }, parent)
    }

    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sf_destroy(p: *mut EeDx7Surface) {
        unsafe { EeDx7Surface::destroy(p, &mut RawGx) }
    }

    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sf_attach(p: *mut EeDx7Surface, surface: *mut c_void) -> i32 {
        unsafe { EeDx7Surface::attach(p, &mut RawGx, &mut RawHost, surface) }
    }

    /// `rect`: a `GERectangle` or NULL (the whole surface); `color`: a `GEColor`.
    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sf_clear(
        p: *mut EeDx7Surface,
        rect: *const f32,
        color: *const f32,
        alpha: f32,
    ) -> i32 {
        let color = unsafe { [*color, *color.add(1), *color.add(2)] };
        unsafe { EeDx7Surface::clear(p, &mut RawGx, &mut RawHost, rect, color, alpha) }
    }

    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sf_copy_to(
        p: *mut EeDx7Surface,
        target: *mut c_void,
        source: *const f32,
        destination: *const f32,
        transparent: bool,
    ) -> i32 {
        unsafe { EeDx7Surface::copy_to(p, &mut RawGx, &mut RawHost, target, source, destination, transparent) }
    }

    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sf_copy_with_filter(
        p: *mut EeDx7Surface,
        target: *mut c_void,
        source: *const f32,
        destination: *const f32,
    ) -> i32 {
        unsafe { EeDx7Surface::copy_with_filter(p, &mut RawGx, &mut RawHost, target, source, destination) }
    }

    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sf_detach(p: *mut EeDx7Surface, index: c_ulong) -> i32 {
        unsafe { EeDx7Surface::detach(p, &mut RawGx, &mut RawHost, index as usize) }
    }

    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sf_flip(p: *mut EeDx7Surface, wait: bool) -> i32 {
        unsafe { EeDx7Surface::flip(p, &mut RawGx, &mut RawHost, wait) }
    }

    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sf_allocated_size(p: *mut EeDx7Surface) -> c_ulong {
        unsafe { EeDx7Surface::allocated_size(p) }
    }

    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sf_lock(p: *mut EeDx7Surface) -> i32 {
        unsafe { EeDx7Surface::lock(p, &mut RawGx, &mut RawHost) }
    }

    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sf_unlock(p: *mut EeDx7Surface) -> i32 {
        unsafe { EeDx7Surface::unlock(p, &mut RawGx, &mut RawHost) }
    }

    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sf_restore(p: *mut EeDx7Surface) -> i32 {
        unsafe { EeDx7Surface::restore(p, &mut RawGx, &mut RawHost) }
    }

    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sf_set_transparency_color(p: *mut EeDx7Surface, color: *const f32) {
        let color = unsafe { [*color, *color.add(1), *color.add(2)] };
        unsafe { EeDx7Surface::set_transparency_color(p, &mut RawGx, color) }
    }

    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sf_text_lock(p: *mut EeDx7Surface, lock: bool) -> i32 {
        unsafe {
            if lock {
                EeDx7Surface::text_lock(p)
            } else {
                EeDx7Surface::text_unlock(p)
            }
        }
    }

    /// `GetRenderScale`: the GX surface's (1 without one).
    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sf_render_scale(p: *mut EeDx7Surface) -> f32 {
        let s = unsafe { sf(p).gx };
        if s.is_null() {
            1.0
        } else {
            crate::dx7::gx::surface_render_scale(s)
        }
    }

    /// `GetWidth` (`height` false) / `GetHeight`: `set` and `shift` are the base's logical members.
    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sf_logical_size(
        p: *mut EeDx7Surface,
        set: c_ulong,
        shift: c_ulong,
        height: bool,
    ) -> c_ulong {
        unsafe { EeDx7Surface::logical_size(p, set, shift, height) }
    }

    /// `mDescription` (`GetSurfaceDescription` and the getters of its fields).
    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sf_description(p: *mut EeDx7Surface) -> *const SurfaceDescription {
        unsafe { &sf(p).desc }
    }

    /// `mTransparencyColor`, a `GEColor`.
    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sf_transparency(p: *mut EeDx7Surface) -> *const f32 {
        unsafe { sf(p).transparency.as_ptr() }
    }

    /// `mAttachedSurfaces[index]`.
    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sf_attached(p: *mut EeDx7Surface, index: c_ulong) -> *mut c_void {
        unsafe { sf(p).attached[index as usize] }
    }

    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sf_attached_count(p: *mut EeDx7Surface) -> c_ulong {
        unsafe { sf(p).attached.len() as c_ulong }
    }

    /// `mSurface` (the `GXSurface*`).
    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sf_gx(p: *mut EeDx7Surface) -> *mut c_void {
        unsafe { sf(p).gx }
    }

    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sf_parent(p: *mut EeDx7Surface) -> *mut c_void {
        unsafe { sf(p).parent }
    }

    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sf_memory(p: *mut EeDx7Surface) -> *mut u8 {
        unsafe { sf(p).memory as *mut u8 }
    }

    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sf_bytes_per_row(p: *mut EeDx7Surface) -> c_ulong {
        unsafe { sf(p).bytes_per_row }
    }

    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sf_text_locked(p: *mut EeDx7Surface) -> bool {
        unsafe { sf(p).text_locked }
    }
}
