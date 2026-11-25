//! WONGUI's images whole in Rust (R6 WONLobby round 24): every image class of the lobby (Image.cpp's Image,
//! NativeImage, NativeMask and the 32-, 8- and 1-bit raw images, Palette.cpp, DelayLoadImage.cpp, the Windows skin's
//! MSImage, the stretch images of GuiHost.cpp and WONLobbyStretchImage.h, PlatNativeImage.cpp's native images, masks
//! and texture pack copies), the image filters (ImageFilters.cpp's crop, scale, tint and transparent colour,
//! StretchImageFilter) and the image decoders (ImageDecoder.cpp's multi-decoder with the PNG and JPEG decoders, imgdec.rs).
//!
//! One object, [`EeWonImage`], is each image; its kind is the original's class. The C++ `Image` is a handle with one
//! pointer to it (the handle owns the object, the handle's reference count is the image's): what the GUI holds,
//! draws and passes around. What an image holds of other C++ objects (other images, the display context
//! of a native image, its Graphics) it holds by their handles through the host (`EeWonImageHost`): references taken
//! and given back as the original's SmartPtr members did, a new handle for each image a call makes (`Wrap`), the
//! Graphics calls of a program image's draw (`GfxRun` runs a draw plan, guidraw.rs's operations). The palettes and
//! masks no C++ code holds are Rust values (`Rc`).
//!
//! Every function returning an image returns its handle with one reference the caller owns (0 for NULL).

use std::cell::{Cell, RefCell};
use std::ffi::{c_char, CStr};
use std::rc::Rc;

use crate::guidraw::{
    self, EeWonDrawOp, EeWonStretchFilter, EE_WON_DRAW_POP_CLIP, EE_WON_DRAW_PUSH_CLIP, EE_WON_DRAW_TRANSLATE,
};
use crate::imgdec::{self, EeWonImgKind};
use crate::imgfilter::{self, EeWonRawPixels};
use crate::mspaint::{EeWonMsPaint, EeWonMsPaintKind, Plan as MsPlan};
use crate::platimg::{self, EeWonPlatMask};
use crate::stretchobj::EeWonStretchImg;
use crate::surface::EeWonSurface;
use ee_base::jpeg::Stop;
use ee_won_net::file::EeWonFileReader;
use ee_won_net::strcase::compare_no_case;
use ee_won_net::wonfile::EeWonFile;

/// What an image asks of the C++ (`a`..`d` per op).
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonImageHost {
    /// `((RefCount*)a)->AddRef()`.
    Ref = 0,
    /// `((RefCount*)a)->Release()`.
    Unref = 1,
    /// A new handle for the image `(EeWonImage*)a` (its class by the kind), no reference taken. (3..6 read a handle's
    /// `mRust` and a DisplayContext's members: read in place since R6 WON GUI round 38, [`EE_WON_IMAGE_RUST_AT`] and
    /// [`EeWonDisplayContextRec`].)
    Wrap = 2,
    /// `((Graphics*)a)`'s translation into `((int*)b)[0..2]`.
    GfxTrans = 7,
    /// `((Graphics*)a)->mDisplayContext.get()`.
    GfxContext = 8,
    /// The `c` draw operations at `(EeWonDrawOp*)b` run on `(Graphics*)a` (WONDrawRun), their image slots the four
    /// `Image*` at `(intptr_t*)d`.
    GfxRun = 9,
    /// `((Graphics*)a)->DrawImage((Image*)b, ((int*)c)[0], ((int*)c)[1])`.
    GfxDrawImage = 10,
    /// `new DisplayContext((EeWonSurface*)a, *(const float*)b)` (its pixel scale): one reference. (11 and 12 made the
    /// C++ SurfaceBase and read its surface; a native image's surface is its own since R6 WON GUI round 36.)
    NewContext = 13,
    /// `new PlatGraphics` attached to `(DisplayContext*)a`: owned by the image.
    NewGraphics = 14,
    /// `delete (Graphics*)a`.
    FreeGraphics = 15,
    /// `((DisplayContext*)a)->CreateImage(b, c)`: a native image's handle with one reference.
    CreateImage = 16,
    /// `WindowManager::GetDefaultWindowManager()->DecodeImage((const char*)a)`: a handle with one reference, or 0.
    Decode = 17,
    /// `GETextureManager::GetTexturePackLogicalShift()` when there is a default window manager, else 0.
    PackShift = 18,
}

/// The offset of `mRust` (the `EeWonImage*`) in an image handle (RefCount's vtable pointer and its 32-bit count,
/// padded): every host's handle class lays it out there (the game's `Image`, a driver's own).
pub const EE_WON_IMAGE_RUST_AT: usize = 16;
/// The offset of a DisplayContext's members ([`EeWonDisplayContextRec`]) in the C++ object (after RefCount's head).
pub const EE_WON_CONTEXT_AT: usize = 16;

/// A DisplayContext's members, in their C++ order (`mSurface`, `mGraphics`, `mPixelScale`), at
/// [`EE_WON_CONTEXT_AT`] in the object (the game's static_asserts: PlatNativeImage.cpp).
#[repr(C)]
pub struct EeWonDisplayContextRec {
    /// `mSurface`: not owned; NULL: nothing to draw into.
    pub surface: *mut EeWonSurface,
    /// `mGraphics` (a `Graphics*`, borrowed), 0 for none.
    pub graphics: isize,
    /// `mPixelScale`.
    pub pixel_scale: f32,
}

const _: () = assert!(
    std::mem::offset_of!(EeWonDisplayContextRec, graphics) == std::mem::size_of::<usize>()
        && std::mem::offset_of!(EeWonDisplayContextRec, pixel_scale) == 2 * std::mem::size_of::<usize>()
);

/// The members of the DisplayContext `ctx` (None for 0).
///
/// # Safety
/// `ctx` is 0 or a live DisplayContext of a host that lays its members out as [`EeWonDisplayContextRec`].
unsafe fn context_rec<'a>(ctx: isize) -> Option<&'a EeWonDisplayContextRec> {
    // SAFETY: the contract
    unsafe { ((ctx as usize).wrapping_add(EE_WON_CONTEXT_AT) as *const EeWonDisplayContextRec).as_ref().filter(|_| ctx != 0) }
}

/// The host: one C++ function for every op.
pub type EeWonImageHostFn = extern "C" fn(op: EeWonImageHost, a: isize, b: isize, c: isize, d: isize) -> isize;

/// The original's class of an image.
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonImageKind {
    /// `WONMSSkin::MSImage` (a ProgramImage).
    Ms = 0,
    /// `DelayLoadImage`.
    Delay = 1,
    /// `StretchImage` (a ProgramImage).
    Stretch = 2,
    /// `StretchCenterImage` (a ProgramImage).
    StretchCenter = 3,
    /// `RawImage32`.
    Raw32 = 4,
    /// `RawImage8`.
    Raw8 = 5,
    /// `RawImage1`.
    Raw1 = 6,
    /// `PlatNativeImage` (the only NativeImage).
    Native = 7,
}

/// A colour table (`Palette`), shared by the 8-bit images that were given it.
pub struct Palette {
    pub colors: Box<[Cell<u32>]>,
}

impl Palette {
    pub fn new(n: usize) -> Rc<Palette> {
        Rc::new(Palette { colors: (0..n).map(|_| Cell::new(0)).collect() })
    }
    /// `Duplicate`: a copy of the colours.
    pub fn duplicate(&self) -> Rc<Palette> {
        Rc::new(Palette { colors: self.colors.iter().map(|c| Cell::new(c.get())).collect() })
    }
    fn ptr(&self) -> *mut u32 {
        self.colors.as_ptr() as *mut u32
    }
}

/// A native image's transparency mask (`PlatNativeMask`: RawImage1's rows, MSB first; a set bit is transparent).
pub struct Mask {
    pub width: i32,
    pub height: i32,
    pub bytes_per_row: i32,
    pub bits: Vec<u8>,
}

impl Mask {
    fn plat(&self) -> EeWonPlatMask {
        EeWonPlatMask {
            bits: self.bits.as_ptr(),
            width: self.width,
            height: self.height,
            bytes_per_row: self.bytes_per_row,
        }
    }
}

/// A C++ object held with one reference (a SmartPtr member): 0 for none.
pub struct Held(Cell<isize>);

impl Held {
    fn none() -> Held {
        Held(Cell::new(0))
    }
    pub fn get(&self) -> isize {
        self.0.get()
    }
}

/// Pixels the C++ may write through (`Cell`s: writes through the pointer are allowed).
type Px<T> = Box<[Cell<T>]>;

fn px<T: Copy + Default>(n: i64) -> Px<T> {
    (0..n.max(0)).map(|_| Cell::new(T::default())).collect()
}

/// The kind's members.
pub enum Kind {
    Ms {
        image_type: u8,
        flags: u8,
    },
    Delay {
        path: Vec<u8>,
        loaded: Cell<bool>,
        image: Held,
    },
    Stretch {
        raw: Held,
        cache: Held,
        s: Cell<EeWonStretchImg>,
    },
    StretchCenter {
        stretch: Held,
        center: Held,
    },
    Raw32 {
        px: Px<u32>,
        transparent: Cell<bool>,
    },
    Raw8 {
        px: Px<u8>,
        bytes_per_row: i32,
        palette: RefCell<Option<Rc<Palette>>>,
        transparent: Cell<i32>,
    },
    Raw1 {
        px: Px<u8>,
        bytes_per_row: i32,
    },
    Native {
        /// the image's own surface (the original's Surface32 member), freed with it
        surf: *mut EeWonSurface,
        context: Held,
        scale: f32,
        graphics: Cell<isize>,
        mask: RefCell<Option<Rc<Mask>>>,
    },
}

/// One image.
pub struct EeWonImage {
    host: EeWonImageHostFn,
    /// The C++ handle (`Image*`) that owns this object.
    me: Cell<isize>,
    width: Cell<u16>,
    height: Cell<u16>,
    pub kind: Kind,
}

/// A reference to a C++ object owned by a Rust call (a SmartPtr local): released when dropped.
pub(crate) struct Ref {
    host: EeWonImageHostFn,
    p: isize,
}

impl Ref {
    /// One reference already owned (a returned handle).
    fn adopt(host: EeWonImageHostFn, p: isize) -> Ref {
        Ref { host, p }
    }
    /// A new reference.
    fn hold(host: EeWonImageHostFn, p: isize) -> Ref {
        if p != 0 {
            href(host, p, true);
        }
        Ref { host, p }
    }
    /// The reference handed to the caller.
    fn into_raw(self) -> isize {
        let p = self.p;
        std::mem::forget(self);
        p
    }
    fn img(&self) -> Option<&EeWonImage> {
        // SAFETY: a live handle (its reference held)
        unsafe { image_of(self.p) }
    }
}

impl Drop for Ref {
    fn drop(&mut self) {
        if self.p != 0 {
            href(self.host, self.p, false);
        }
    }
}

/// A handle's reference taken or given back: the game's reference function, else the host's Ref / Unref.
fn href(host: EeWonImageHostFn, p: isize, hold: bool) {
    if crate::gamewin::rf(p, hold).is_none() {
        host(if hold { EeWonImageHost::Ref } else { EeWonImageHost::Unref }, p, 0, 0, 0);
    }
}

/// The Rust image of a live handle (None for 0).
///
/// # Safety
/// `h` is 0 or a live image handle; the object lives while the caller holds a reference to the handle.
unsafe fn image_of<'a>(h: isize) -> Option<&'a EeWonImage> {
    if h == 0 {
        return None;
    }
    // SAFETY: the contract: a live handle's `mRust` at EE_WON_IMAGE_RUST_AT
    unsafe { (*((h as usize + EE_WON_IMAGE_RUST_AT) as *const *const EeWonImage)).as_ref() }
}

/// `PlatGUIPixels(logical, scale)`.
pub fn plat_pixels(logical: i32, scale: f32) -> i32 {
    if scale == 1.0 {
        logical
    } else {
        (logical as f32 * scale + 0.5).floor() as i32
    }
}

/// The C++ `CalcBytesPerRow`: `n` rounded up to a multiple of 4.
fn round4(n: i32) -> i32 {
    if n % 4 != 0 {
        n + 4 - n % 4
    } else {
        n
    }
}

impl EeWonImage {
    fn boxed(host: EeWonImageHostFn, w: i32, h: i32, kind: Kind) -> Box<EeWonImage> {
        Box::new(EeWonImage { host, me: Cell::new(0), width: Cell::new(w as u16), height: Cell::new(h as u16), kind })
    }

    /// A new image's handle (with one reference).
    fn wrap(o: Box<EeWonImage>) -> Ref {
        let host = o.host;
        let p = Box::into_raw(o);
        let h = host(EeWonImageHost::Wrap, p as isize, 0, 0, 0);
        Ref::hold(host, h)
    }

    fn h(&self, op: EeWonImageHost, a: isize, b: isize, c: isize, d: isize) -> isize {
        (self.host)(op, a, b, c, d)
    }

    /// `held = p` (the SmartPtr assignment: the new reference first).
    fn set_held(&self, held: &Held, p: isize) {
        if p != 0 {
            href(self.host, p, true);
        }
        let old = held.0.replace(p);
        if old != 0 {
            href(self.host, old, false);
        }
    }

    pub fn width(&self) -> i32 {
        self.width.get() as i32
    }
    pub fn height(&self) -> i32 {
        self.height.get() as i32
    }
    pub fn kind_id(&self) -> EeWonImageKind {
        match self.kind {
            Kind::Ms { .. } => EeWonImageKind::Ms,
            Kind::Delay { .. } => EeWonImageKind::Delay,
            Kind::Stretch { .. } => EeWonImageKind::Stretch,
            Kind::StretchCenter { .. } => EeWonImageKind::StretchCenter,
            Kind::Raw32 { .. } => EeWonImageKind::Raw32,
            Kind::Raw8 { .. } => EeWonImageKind::Raw8,
            Kind::Raw1 { .. } => EeWonImageKind::Raw1,
            Kind::Native { .. } => EeWonImageKind::Native,
        }
    }

    // ---- the makers (the original constructors)

    /// `new RawImage32(width, height)`: transparency off (the original's pixels were uninitialised; zero here).
    pub fn new_raw32(host: EeWonImageHostFn, w: i32, h: i32) -> Box<EeWonImage> {
        let n = w as i64 * h as i64;
        Self::boxed(host, w, h, Kind::Raw32 { px: px(n), transparent: Cell::new(false) })
    }

    /// `new RawImage8(width, height, palette)`: rows of the width rounded up to 4 bytes, no transparent colour.
    pub fn new_raw8(host: EeWonImageHostFn, w: i32, h: i32, palette: Option<Rc<Palette>>) -> Box<EeWonImage> {
        let bpr = round4(w as u16 as i32);
        Self::boxed(
            host,
            w,
            h,
            Kind::Raw8 {
                px: px(h as i64 * bpr as i64),
                bytes_per_row: bpr,
                palette: RefCell::new(palette),
                transparent: Cell::new(-1),
            },
        )
    }

    /// `new RawImage1(width, height)`: rows of (width + 7) / 8 bytes rounded up to 4.
    pub fn new_raw1(host: EeWonImageHostFn, w: i32, h: i32) -> Box<EeWonImage> {
        let width = w as u16 as i32;
        let bpr = round4(width / 8 + if width % 8 != 0 { 1 } else { 0 });
        let height = h as u16 as i32;
        Self::boxed(host, w, h, Kind::Raw1 { px: px(bpr as i64 * height as i64), bytes_per_row: bpr })
    }

    /// `new WONMSSkin::MSImage(type, flags, width, height)`.
    pub fn new_ms(host: EeWonImageHostFn, image_type: u8, flags: u8, w: i32, h: i32) -> Box<EeWonImage> {
        Self::boxed(host, w, h, Kind::Ms { image_type, flags })
    }

    /// `new DelayLoadImage(path, width, height)`.
    pub fn new_delay(host: EeWonImageHostFn, path: &[u8], w: i32, h: i32) -> Box<EeWonImage> {
        Self::boxed(host, w, h, Kind::Delay { path: path.to_vec(), loaded: Cell::new(false), image: Held::none() })
    }

    /// `new StretchImage(raw, left, top, right, bottom)` (size 0 x 0).
    pub fn new_stretch(host: EeWonImageHostFn, raw: isize, s: EeWonStretchImg) -> Box<EeWonImage> {
        let o = Self::boxed(host, 0, 0, Kind::Stretch { raw: Held::none(), cache: Held::none(), s: Cell::new(s) });
        if let Kind::Stretch { raw: r, .. } = &o.kind {
            o.set_held(r, raw);
        }
        o
    }

    /// `new StretchCenterImage(stretch, center)` (size 0 x 0).
    pub fn new_stretch_center(host: EeWonImageHostFn, stretch: isize, center: isize) -> Box<EeWonImage> {
        let o = Self::boxed(host, 0, 0, Kind::StretchCenter { stretch: Held::none(), center: Held::none() });
        if let Kind::StretchCenter { stretch: s, center: c } = &o.kind {
            o.set_held(s, stretch);
            o.set_held(c, center);
        }
        o
    }

    /// `new PlatNativeImage(context, width, height, scale)`: black, on its own surface of PlatGUIPixels(size, scale)
    /// pixels (the scale at least 1) with its own display context at that scale.
    pub fn new_native(host: EeWonImageHostFn, w: i32, h: i32, scale: f32) -> Box<EeWonImage> {
        let scale = if scale >= 1.0 { scale } else { 1.0 };
        // a new black 32-bit surface (negative sizes 0; `create` clears the pixels)
        let surf = crate::surface::ee_won_surface_new(crate::surface::EE_WON_SURFACE_32);
        // SAFETY: the new surface
        unsafe { (*surf).create(plat_pixels(w, scale).max(0), plat_pixels(h, scale).max(0)) };
        let context = host(EeWonImageHost::NewContext, surf as isize, &scale as *const f32 as isize, 0, 0);
        Self::boxed(
            host,
            w,
            h,
            Kind::Native {
                surf,
                context: Held(Cell::new(context)),
                scale,
                graphics: Cell::new(0),
                mask: RefCell::new(None),
            },
        )
    }

    // ---- the raw images' pixels

    /// The raw image's pixels as imgfilter.rs walks them (None for another kind).
    pub fn raw_pixels(&self) -> Option<EeWonRawPixels> {
        let (w, h) = (self.width(), self.height());
        let (kind, data, pitch) = match &self.kind {
            Kind::Raw1 { px, bytes_per_row } => (0, px.as_ptr() as *mut u8, *bytes_per_row),
            Kind::Raw8 { px, bytes_per_row, .. } => (1, px.as_ptr() as *mut u8, *bytes_per_row),
            Kind::Raw32 { px, .. } => (2, px.as_ptr() as *mut u8, w * 4),
            _ => return None,
        };
        Some(EeWonRawPixels { kind, data, width: w, height: h, pitch })
    }

    /// `RawImage::mType` (0 1-bit, 1 8-bit, 2 32-bit; -1 for another kind).
    pub fn raw_type(&self) -> i32 {
        self.raw_pixels().map_or(-1, |p| p.kind)
    }

    /// `HasTransparency()`.
    pub fn has_transparency(&self) -> bool {
        match &self.kind {
            Kind::Raw32 { transparent, .. } => transparent.get(),
            Kind::Raw8 { transparent, .. } => transparent.get() >= 0,
            _ => false,
        }
    }

    /// `RawImage32::GetPixel` / `RawImage8::GetPixel`.
    pub fn pixel(&self, x: i32, y: i32) -> u32 {
        match &self.kind {
            Kind::Raw32 { px, .. } => px[(y as i64 * self.width() as i64 + x as i64) as usize].get(),
            Kind::Raw8 { px, bytes_per_row, .. } => {
                px[(y as i64 * *bytes_per_row as i64 + x as i64) as usize].get() as u32
            }
            _ => 0,
        }
    }

    /// A raw image's copy of the other's palette or transparency (`CopyInfo`, `CopyRawImageInfo`).
    fn copy_info(&self, from: &EeWonImage) {
        match (&self.kind, &from.kind) {
            (Kind::Raw8 { palette, transparent, .. }, Kind::Raw8 { palette: p, transparent: t, .. }) => {
                *palette.borrow_mut() = p.borrow().clone();
                transparent.set(t.get());
            }
            (Kind::Raw32 { transparent, .. }, Kind::Raw32 { transparent: t, .. }) => transparent.set(t.get()),
            _ => {}
        }
    }

    /// A new raw image of this one's type and the given size, with its palette or transparency.
    fn raw_like(&self, w: i32, h: i32) -> Option<Box<EeWonImage>> {
        let o = match self.kind {
            Kind::Raw32 { .. } => Self::new_raw32(self.host, w, h),
            Kind::Raw8 { .. } => Self::new_raw8(self.host, w, h, None),
            Kind::Raw1 { .. } => Self::new_raw1(self.host, w, h),
            _ => return None,
        };
        o.copy_info(self);
        Some(o)
    }

    // ---- native images

    fn native_mask(&self) -> Option<Rc<Mask>> {
        match &self.kind {
            Kind::Native { mask, .. } => mask.borrow().clone(),
            _ => None,
        }
    }

    fn set_native_mask(&self, m: Option<Rc<Mask>>) {
        if let Kind::Native { mask, .. } = &self.kind {
            *mask.borrow_mut() = m;
        }
    }

    fn native_surf(&self) -> *mut EeWonSurface {
        match &self.kind {
            Kind::Native { surf, .. } => *surf,
            _ => std::ptr::null_mut(),
        }
    }

    fn native_scale(&self) -> f32 {
        match &self.kind {
            Kind::Native { scale, .. } => *scale,
            _ => 1.0,
        }
    }

    /// `NativeImage::GetGraphics()`: made on first use (a PlatGraphics attached to the image's display context).
    pub fn graphics(&self) -> isize {
        match &self.kind {
            Kind::Native { graphics, context, .. } => {
                if graphics.get() == 0 {
                    graphics.set(self.h(EeWonImageHost::NewGraphics, context.get(), 0, 0, 0));
                }
                graphics.get()
            }
            _ => 0,
        }
    }

    /// The native image of a raw image's pixels (`NativeOfRaw`): 32-bit without the alpha byte, 8-bit through the
    /// palette with the transparent index black, 1-bit white where a bit is set; scale 1.
    fn native_of_raw(&self) -> Ref {
        let n = Self::new_native(self.host, self.width(), self.height(), 1.0);
        let surf = n.native_surf();
        let p = self.raw_pixels().expect("a raw image");
        let (pal, colors, transparent) = match &self.kind {
            Kind::Raw8 { palette, transparent, .. } => match &*palette.borrow() {
                Some(p) => (p.ptr() as *const u32, p.colors.len() as i32, transparent.get()),
                None => (std::ptr::null(), 0, transparent.get()),
            },
            _ => (std::ptr::null(), 0, -1),
        };
        // SAFETY: the raw image's live pixels, the new image's live surface
        unsafe { platimg::ee_won_plat_img_from_raw(&p, pal, colors, transparent, surf) };
        Self::wrap(n)
    }

    /// `RawImage1::GetMask`: a mask of the bits.
    fn mask_of_raw1(&self) -> Rc<Mask> {
        let Kind::Raw1 { px, bytes_per_row } = &self.kind else { unreachable!() };
        let n = (*bytes_per_row as i64 * self.height().max(0) as i64).max(0) as usize;
        Rc::new(Mask {
            width: self.width(),
            height: self.height(),
            bytes_per_row: *bytes_per_row,
            bits: px.iter().take(n).map(Cell::get).collect(),
        })
    }

    // ---- the virtuals

    /// `Draw(context, x, y, left, top, width, height)`.
    #[allow(clippy::too_many_arguments)]
    pub fn draw(&self, ctx: isize, x: i32, y: i32, left: i32, top: i32, w: i32, h: i32) {
        match &self.kind {
            Kind::Raw32 { .. } | Kind::Raw8 { .. } | Kind::Raw1 { .. } => {
                // RawImage::Draw: its native image drawn
                let n = self.native(ctx);
                if let Some(i) = n.img() {
                    i.draw(ctx, x, y, left, top, w, h);
                }
            }
            Kind::Native { .. } => self.native_draw(ctx, x, y, left, top, w, h),
            Kind::Delay { .. } => {
                let n = self.delay_load();
                if let Some(i) = image_of_ref(n) {
                    i.draw(ctx, x, y, left, top, w, h);
                }
            }
            Kind::Ms { .. } | Kind::Stretch { .. } | Kind::StretchCenter { .. } => {
                self.program_draw(ctx, x, y, left, top, w, h)
            }
        }
    }

    /// `PlatNativeImage::Draw`: part of the image into the context's surface within its clip (platimg.rs draw).
    #[allow(clippy::too_many_arguments)]
    fn native_draw(&self, ctx: isize, x: i32, y: i32, left: i32, top: i32, w: i32, h: i32) {
        // SAFETY: the caller's live context
        let Some(rec) = (unsafe { context_rec(ctx) }) else { return };
        let (dst, dscale) = (rec.surface, rec.pixel_scale);
        if dst.is_null() {
            return;
        }
        let m = self.native_mask();
        let pm = m.as_ref().map(|m| m.plat());
        // SAFETY: live surfaces; the mask's bits live while `m` does
        unsafe {
            platimg::ee_won_plat_img_draw(
                dst,
                dscale,
                self.native_surf(),
                self.native_scale(),
                self.width(),
                self.height(),
                pm.as_ref().map_or(std::ptr::null(), |p| p as *const EeWonPlatMask),
                x,
                y,
                left,
                top,
                w,
                h,
            )
        };
    }

    /// `ProgramImage::Draw`: the paint at (x, y) through the context's Graphics, clipped to the source rectangle when
    /// it is not the whole image, then the translation restored.
    #[allow(clippy::too_many_arguments)]
    fn program_draw(&self, ctx: isize, x: i32, y: i32, left: i32, top: i32, w: i32, h: i32) {
        // SAFETY: the caller's live context
        let g = unsafe { context_rec(ctx) }.map_or(0, |r| r.graphics);
        if g == 0 {
            return;
        }
        let mut t = [0i32; 2];
        self.h(EeWonImageHost::GfxTrans, g, t.as_mut_ptr() as isize, 0, 0);
        let (tx, ty) = (t[0], t[1]);
        let clipped = left != 0 || top != 0 || w != self.width() || h != self.height();
        let mut pre = vec![op(EE_WON_DRAW_TRANSLATE, &[x - tx, y - ty])];
        if clipped {
            pre.push(op(EE_WON_DRAW_PUSH_CLIP, &[left, top, w, h]));
        }
        self.run(g, &pre, [0; 4]);
        self.paint(g);
        let mut post = Vec::new();
        if clipped {
            post.push(op(EE_WON_DRAW_POP_CLIP, &[]));
        }
        post.push(op(EE_WON_DRAW_TRANSLATE, &[tx - x, ty - y]));
        self.run(g, &post, [0; 4]);
    }

    fn run(&self, g: isize, ops: &[EeWonDrawOp], slots: [isize; 4]) {
        self.h(EeWonImageHost::GfxRun, g, ops.as_ptr() as isize, ops.len() as isize, slots.as_ptr() as isize);
    }

    /// `Paint(g)` of the program images.
    fn paint(&self, g: isize) {
        let (w, h) = (self.width(), self.height());
        match &self.kind {
            Kind::Ms { image_type, flags } => {
                // WONMSSkin::MSImage::Paint (mspaint.rs SkinImage)
                let p = EeWonMsPaint {
                    kind: EeWonMsPaintKind::SkinImage,
                    flags: 0,
                    width: w,
                    height: h,
                    a: *image_type as i32,
                    b: *flags as i32,
                    c: 0,
                    d: 0,
                    e: 0,
                };
                let mut plan = MsPlan::default();
                plan.paint(&p);
                self.run(g, &plan.ops, [0; 4]);
            }
            Kind::Stretch { cache, s, .. } => {
                // StretchImage::Paint: the cache rebuilt first if needed, then drawn (skip-centre: its borders)
                if s.get().need_recalc {
                    self.calc_image(self.h(EeWonImageHost::GfxContext, g, 0, 0, 0));
                }
                let ops = s.get().paint(w, h);
                let c = Ref::hold(self.host, cache.get());
                self.run(g, &ops, [c.p, 0, 0, 0]);
            }
            Kind::StretchCenter { stretch, center } => {
                // StretchCenterImage::Paint (guidraw.rs stretch_center_paint)
                let (s, c) = (Ref::hold(self.host, stretch.get()), Ref::hold(self.host, center.get()));
                let (cw, ch) = c.img().map_or((0, 0), |i| (i.width(), i.height()));
                let ops = guidraw::stretch_center_paint(w, h, cw, ch);
                self.run(g, &ops, [s.p, c.p, 0, 0]);
            }
            _ => {}
        }
    }

    /// `StretchImage::CalcImage(context)`: the stretch filter over the source image at the current size, its
    /// native image the cache.
    fn calc_image(&self, ctx: isize) {
        let Kind::Stretch { raw, cache, s } = &self.kind else { return };
        let st = s.get();
        let f = EeWonStretchFilter {
            width: self.width(),
            height: self.height(),
            left: st.left,
            right: st.right,
            top: st.top,
            bottom: st.bottom,
            src_width: 0,
            src_height: 0,
            tile_horz: st.tile_horz,
            tile_vert: st.tile_vert,
            skip_center: st.skip_center,
        };
        let r = Ref::hold(self.host, raw.get());
        let filtered = r.img().and_then(|i| i.stretch_filter(f));
        let native = filtered.as_ref().and_then(|f| f.img()).map(|i| i.native(ctx));
        self.set_held(cache, native.as_ref().map_or(0, |n| n.p));
        let mut st = s.get();
        st.need_recalc = false;
        s.set(st);
    }

    /// `SetSize(width, height)`.
    pub fn set_size(&self, w: i32, h: i32) {
        match &self.kind {
            Kind::Ms { .. } => {
                self.width.set(w as u16);
                self.height.set(h as u16);
            }
            Kind::Stretch { s, .. } => {
                let mut st = s.get();
                if st.set_size(self.width(), self.height(), w, h) {
                    self.width.set(w as u16);
                    self.height.set(h as u16);
                }
                s.set(st);
            }
            Kind::StretchCenter { stretch, .. } => {
                self.width.set(w as u16);
                self.height.set(h as u16);
                let s = Ref::hold(self.host, stretch.get());
                if let Some(i) = s.img() {
                    i.set_size(w, h);
                }
            }
            _ => {}
        }
    }

    /// `Duplicate()` (0 where the class had no copy: the 1-bit and delay-load images).
    fn duplicate(&self) -> Ref {
        let host = self.host;
        match &self.kind {
            Kind::Raw32 { px, transparent } => {
                let o = Self::new_raw32(host, self.width(), self.height());
                if let Kind::Raw32 { px: d, transparent: t } = &o.kind {
                    t.set(transparent.get());
                    for (a, b) in d.iter().zip(px.iter()) {
                        a.set(b.get());
                    }
                }
                Self::wrap(o)
            }
            Kind::Raw8 { px, palette, transparent, .. } => {
                let o = Self::new_raw8(host, self.width(), self.height(), palette.borrow().clone());
                if let Kind::Raw8 { px: d, transparent: t, .. } = &o.kind {
                    t.set(transparent.get());
                    for (a, b) in d.iter().zip(px.iter()) {
                        a.set(b.get());
                    }
                }
                Self::wrap(o)
            }
            Kind::Native { surf, context, scale, .. } => {
                // PlatNativeImage::Duplicate: the pixels, the mask shared
                let _ = context;
                let o = Self::new_native(host, self.width(), self.height(), *scale);
                // SAFETY: two live surfaces of the same pixel size
                unsafe {
                    let (s, d) = (&mut **surf, &mut *o.native_surf());
                    if let (Some(a), Some(b)) = (s.words_mut(), d.words_mut()) {
                        let n = a.len().min(b.len());
                        b[..n].copy_from_slice(&a[..n]);
                    }
                }
                o.set_native_mask(self.native_mask());
                Self::wrap(o)
            }
            Kind::Ms { image_type, flags } => {
                Self::wrap(Self::new_ms(host, *image_type, *flags, self.width(), self.height()))
            }
            Kind::Stretch { raw, s, .. } => Self::wrap(Self::new_stretch(host, raw.get(), s.get().duplicate())),
            Kind::StretchCenter { stretch, center } => {
                let s = Ref::hold(host, stretch.get());
                let d = s.img().map_or(Ref::adopt(host, 0), |i| i.duplicate());
                Self::wrap(Self::new_stretch_center(host, d.p, center.get()))
            }
            Kind::Raw1 { .. } | Kind::Delay { .. } => Ref::adopt(host, 0),
        }
    }

    /// `GetNative(context)`.
    fn native(&self, ctx: isize) -> Ref {
        let host = self.host;
        match &self.kind {
            Kind::Raw32 { px, transparent } => {
                if !transparent.get() {
                    return self.native_of_raw();
                }
                // the pixels of alpha 0 cleared and masked (imgfilter.rs mask32)
                let mask = Self::new_raw1(host, self.width(), self.height());
                let Kind::Raw1 { px: mp, bytes_per_row } = &mask.kind else { unreachable!() };
                // SAFETY: the live pixels and mask bits
                unsafe {
                    imgfilter::ee_won_imgf_mask32(
                        px.as_ptr() as *mut u32,
                        self.width(),
                        self.height(),
                        mp.as_ptr() as *mut u8,
                        *bytes_per_row,
                    )
                };
                let n = self.native_of_raw();
                if let Some(i) = n.img() {
                    i.set_native_mask(Some(mask.mask_of_raw1()));
                }
                n
            }
            Kind::Raw8 { px, bytes_per_row, transparent, .. } => {
                let n = self.native_of_raw();
                if transparent.get() != -1 {
                    let mask = Self::new_raw1(host, self.width(), self.height());
                    let Kind::Raw1 { px: mp, bytes_per_row: mb } = &mask.kind else { unreachable!() };
                    // SAFETY: the live pixels and mask bits
                    unsafe {
                        imgfilter::ee_won_imgf_mask8(
                            px.as_ptr() as *const u8,
                            self.width(),
                            self.height(),
                            *bytes_per_row,
                            transparent.get(),
                            mp.as_ptr() as *mut u8,
                            *mb,
                        )
                    };
                    if let Some(i) = n.img() {
                        i.set_native_mask(Some(mask.mask_of_raw1()));
                    }
                }
                n
            }
            Kind::Raw1 { .. } => self.native_of_raw(),
            Kind::Native { .. } => Ref::hold(host, self.me.get()),
            Kind::Delay { .. } => Ref::hold(host, self.delay_load()),
            Kind::Stretch { cache, s, .. } => {
                if s.get().need_recalc {
                    self.calc_image(ctx);
                }
                Ref::hold(host, cache.get())
            }
            Kind::Ms { .. } | Kind::StretchCenter { .. } => {
                // Image::GetNative: a native image of the size with this image drawn into it
                let n = Ref::adopt(
                    host,
                    self.h(EeWonImageHost::CreateImage, ctx, self.width() as isize, self.height() as isize, 0),
                );
                if let Some(i) = n.img() {
                    let g = i.graphics();
                    let xy = [0i32; 2];
                    self.h(EeWonImageHost::GfxDrawImage, g, self.me.get(), xy.as_ptr() as isize, 0);
                }
                n
            }
        }
    }

    /// DelayLoadImage: the file decoded on the first need (the flag set first); the image (a borrowed handle).
    fn delay_load(&self) -> isize {
        let Kind::Delay { path, loaded, image } = &self.kind else { return 0 };
        if !loaded.get() {
            loaded.set(true);
            let mut p = path.clone();
            p.push(0);
            let d = Ref::adopt(self.host, self.h(EeWonImageHost::Decode, p.as_ptr() as isize, 0, 0, 0));
            self.set_held(image, d.p);
        }
        image.get()
    }

    /// `NativeImage::GetRaw()` (PlatNativeImage's): a 32-bit raw copy; with a mask it is transparent and its opaque
    /// pixels get alpha 0xff (platimg.rs raw).
    fn get_raw(&self) -> Ref {
        if !matches!(self.kind, Kind::Native { .. }) {
            return Ref::adopt(self.host, 0);
        }
        let o = Self::new_raw32(self.host, self.width(), self.height());
        let m = self.native_mask();
        let pm = m.as_ref().map(|m| m.plat());
        if let Kind::Raw32 { px, transparent } = &o.kind {
            if m.is_some() {
                transparent.set(true);
            }
            // SAFETY: the live surface, the new pixels (width * height)
            unsafe {
                platimg::ee_won_plat_img_raw(
                    self.native_surf(),
                    self.native_scale(),
                    self.width(),
                    self.height(),
                    pm.as_ref().map_or(std::ptr::null(), |p| p as *const EeWonPlatMask),
                    px.as_ptr() as *mut u32,
                )
            };
        }
        Self::wrap(o)
    }

    // ---- the filters (ImageFilters.cpp, StretchImageFilter)

    /// `CropImageFilter(x, y, width, height).Filter(this)`: the rectangle copied row by row from an 8- or 32-bit
    /// image with its palette or transparency; None for other types.
    pub(crate) fn crop(&self, x: i32, y: i32, w: i32, h: i32) -> Option<Ref> {
        let size = match self.kind {
            Kind::Raw8 { .. } => 1,
            Kind::Raw32 { .. } => 4,
            _ => return None,
        };
        let d = self.raw_like(w, h)?;
        let (s, dp) = (self.raw_pixels()?, d.raw_pixels()?);
        // SAFETY: both images' live pixels
        unsafe { imgfilter::ee_won_imgf_crop(&s, &dp, x * size, y, w * size, h) };
        Some(Self::wrap(d))
    }

    /// `ScaleImageFilter::Filter(this)` (percent or absolute): the image itself when the size matches.
    pub(crate) fn scale(&self, is_percent: bool, pw: i32, ph: i32, w: i32, h: i32) -> Ref {
        let (mut w, mut h) = (w, h);
        if imgfilter::check_size(is_percent, pw, ph, &mut w, &mut h, self.width(), self.height()) {
            return Ref::hold(self.host, self.me.get());
        }
        let Some(d) = self.raw_like(w, h) else {
            return Ref::hold(self.host, self.me.get());
        };
        let (s, dp) = (self.raw_pixels().expect("raw"), d.raw_pixels().expect("raw"));
        // SAFETY: both images' live pixels
        unsafe { imgfilter::ee_won_imgf_scale(&s, &dp) };
        Self::wrap(d)
    }

    /// `TintImageFilter(r, g, b).Filter(this)`: in place (8-bit: on a duplicated palette; 32-bit: every pixel,
    /// alpha kept); the image itself.
    pub(crate) fn tint(&self, r: i32, g: i32, b: i32) -> Ref {
        match &self.kind {
            Kind::Raw8 { palette, .. } => {
                let d = palette.borrow().as_ref().map(|p| p.duplicate());
                if let Some(p) = &d {
                    // SAFETY: the palette's live colours
                    unsafe { imgfilter::ee_won_imgf_tint(p.ptr(), p.colors.len(), r, g, b, false) };
                }
                *palette.borrow_mut() = d;
            }
            Kind::Raw32 { px, .. } => {
                // SAFETY: the live pixels
                unsafe { imgfilter::ee_won_imgf_tint(px.as_ptr() as *mut u32, px.len(), r, g, b, true) };
            }
            _ => {}
        }
        Ref::hold(self.host, self.me.get())
    }

    /// `TransparentImageFilter(color).Filter(this)`: 8-bit images get the colour's palette index as transparent,
    /// 32-bit pixels alpha 0 or opaque; the image itself.
    pub(crate) fn transparent(&self, color: u32) -> Ref {
        match &self.kind {
            Kind::Raw8 { palette, transparent, .. } => {
                if let Some(p) = &*palette.borrow() {
                    // SAFETY: the palette's live colours
                    let i = unsafe { imgfilter::ee_won_imgf_palette_find(p.ptr(), p.colors.len(), color) };
                    if i >= 0 {
                        transparent.set(i);
                    }
                }
            }
            Kind::Raw32 { px, transparent } => {
                transparent.set(true);
                // SAFETY: the live pixels
                unsafe { imgfilter::ee_won_imgf_transparent32(px.as_ptr() as *mut u32, px.len(), color) };
            }
            _ => {}
        }
        Ref::hold(self.host, self.me.get())
    }

    /// `StretchImageFilter::Filter(this)`: the nine-piece stretched copy of an 8- or 32-bit image (guidraw.rs
    /// stretch_filter's plan, imgfilter.rs stretch_run's pixels); None for other types.
    pub(crate) fn stretch_filter(&self, mut f: EeWonStretchFilter) -> Option<Ref> {
        if !matches!(self.kind, Kind::Raw8 { .. } | Kind::Raw32 { .. }) {
            return None;
        }
        f.src_width = self.width();
        f.src_height = self.height();
        let (w, h, ops) = guidraw::stretch_filter(&f);
        let d = self.raw_like(w, h)?;
        let (s, dp) = (self.raw_pixels()?, d.raw_pixels()?);
        // SAFETY: both images' live pixels
        unsafe { imgfilter::ee_won_imgf_stretch_run(&s, &dp, ops.as_ptr(), ops.len()) };
        Some(Self::wrap(d))
    }
}

impl Drop for EeWonImage {
    fn drop(&mut self) {
        let host = self.host;
        let rel = |p: isize| {
            if p != 0 {
                href(host, p, false);
            }
        };
        match &self.kind {
            Kind::Delay { image, .. } => rel(image.get()),
            Kind::Stretch { raw, cache, .. } => {
                rel(raw.get());
                rel(cache.get());
            }
            Kind::StretchCenter { stretch, center } => {
                rel(stretch.get());
                rel(center.get());
            }
            Kind::Native { surf, context, graphics, mask, .. } => {
                // ~PlatNativeImage: the Graphics, the surface; ~NativeImage: the mask, the context
                if graphics.get() != 0 {
                    host(EeWonImageHost::FreeGraphics, graphics.get(), 0, 0, 0);
                }
                // SAFETY: the image's own surface, made by new_native, freed once
                unsafe { crate::surface::ee_won_surface_free(*surf) };
                mask.borrow_mut().take();
                rel(context.get());
            }
            _ => {}
        }
    }
}

fn op(kind: i32, args: &[i32]) -> EeWonDrawOp {
    let mut o = EeWonDrawOp { kind, ..Default::default() };
    for (s, v) in [&mut o.a, &mut o.b, &mut o.c, &mut o.d].into_iter().zip(args) {
        *s = *v;
    }
    o
}

fn image_of_ref<'a>(h: isize) -> Option<&'a EeWonImage> {
    // SAFETY: a handle the caller holds
    unsafe { image_of(h) }
}

// ---- the decoders (ImageDecoder.cpp)

/// The multi-decoder: the decoders by file extension (a map ordered and matched without case: the first spelling
/// of a key stays) and the current extension.
pub struct EeWonImgDecoder {
    host: EeWonImageHostFn,
    pub map: Vec<(Vec<u8>, EeWonImgKind)>,
    pub cur: Vec<u8>,
}

/// A decode's end: an image (or none), or a FileReader status the C++ throws.
type Decoded = Result<Option<Ref>, i32>;

impl EeWonImgDecoder {
    pub fn new(host: EeWonImageHostFn) -> EeWonImgDecoder {
        EeWonImgDecoder { host, map: Vec::new(), cur: Vec::new() }
    }

    /// `AddDecoder(decoder, extension)`: replaces the decoder of an extension already there.
    pub fn add(&mut self, kind: EeWonImgKind, ext: &[u8]) {
        match self.map.binary_search_by(|(k, _)| compare_no_case(k, ext).cmp(&0)) {
            Ok(i) => self.map[i].1 = kind,
            Err(i) => self.map.insert(i, (ext.to_vec(), kind)),
        }
    }

    /// The decoder for the current extension.
    pub fn find(&self) -> Option<EeWonImgKind> {
        self.map.binary_search_by(|(k, _)| compare_no_case(k, &self.cur).cmp(&0)).ok().map(|i| self.map[i].1)
    }

    /// `SetFileName(path)`: the path's extension (WONFile's) becomes the current one.
    pub fn set_file_name(&mut self, path: &[u8]) {
        self.cur = EeWonFile::new(path).extension().to_vec();
    }

    /// `ImageDecoder::DecodeRaw(reader)` through the current extension's decoder: None for a closed reader or no
    /// decoder, or a failed decode.
    fn decode_reader(&self, r: &mut EeWonFileReader) -> Decoded {
        if !r.is_open() {
            return Ok(None);
        }
        let Some(kind) = self.find() else { return Ok(None) };
        match imgdec::decode(kind, r) {
            Ok((w, h, t, pixels)) => {
                // a RawImage32 of the decoded pixels (allocated directly, as the original did)
                let o = EeWonImage::new_raw32(self.host, w, h);
                if let Kind::Raw32 { px, transparent } = &o.kind {
                    transparent.set(t);
                    for (a, b) in px.iter().zip(pixels.iter()) {
                        a.set(*b);
                    }
                }
                Ok(Some(EeWonImage::wrap(o)))
            }
            Err(Stop::Throw(s)) => Err(s),
            Err(_) => Ok(None),
        }
    }

    fn open(path: &CStr) -> EeWonFileReader {
        let mut r = EeWonFileReader::new();
        // SAFETY: a C string
        unsafe { r.open(path.as_ptr()) };
        r
    }

    /// `DecodeRaw(path)`.
    pub(crate) fn decode_raw(&mut self, path: &CStr) -> Decoded {
        self.set_file_name(path.to_bytes());
        let mut r = Self::open(path);
        let d = self.decode_reader(&mut r);
        r.close();
        d
    }

    /// `GetImageAttributes(path)`: the size, or None (a read error included).
    pub fn attributes(&mut self, path: &CStr) -> Option<(i32, i32)> {
        self.set_file_name(path.to_bytes());
        let mut r = Self::open(path);
        let a = if r.is_open() { self.find().and_then(|k| imgdec::attributes(k, &mut r).ok()) } else { None };
        r.close();
        a
    }

    /// `Decode(context, path)`: the raw image's native image, then the texture pack's copy when there is one.
    pub(crate) fn decode(&mut self, ctx: isize, path: &CStr) -> Decoded {
        let raw = self.decode_raw(path)?;
        let Some(native) = raw.as_ref().and_then(|r| r.img()).map(|i| i.native(ctx)) else {
            return Ok(None);
        };
        if native.p == 0 {
            return Ok(None);
        }
        Ok(Some(self.pack_image(ctx, native, path)))
    }

    /// `PlatGUIPackImage(context, image, path)`: with the texture pack on, its copy of the file at an integer
    /// multiple of the image's size becomes an image of the image's logical size and mask at that pixel scale
    /// (platimg.rs); else the image itself.
    fn pack_image(&mut self, ctx: isize, image: Ref, path: &CStr) -> Ref {
        let host = self.host;
        let shift = host(EeWonImageHost::PackShift, 0, 0, 0, 0) as i32;
        let Some(img) = image.img() else { return image };
        let (w, h) = (img.width(), img.height());
        if shift <= 0 || w == 0 || h == 0 {
            return image;
        }
        let mut raw: Option<Ref> = None;
        let mut name = [0u8; 4096];
        for i in 0..2 {
            if raw.is_some() {
                break;
            }
            // SAFETY: a C string, the buffer's size
            if unsafe { platimg::ee_won_plat_pack_name(path.as_ptr() as *const u8, i, name.as_mut_ptr(), name.len()) }
                < 0
            {
                break;
            }
            let n = CStr::from_bytes_until_nul(&name).expect("NUL-terminated");
            let mut r = Self::open(n);
            if !r.is_open() {
                continue;
            }
            self.set_file_name(n.to_bytes());
            raw = self.decode_reader(&mut r).unwrap_or(None);
            r.close();
        }
        let Some(raw) = raw else { return image };
        let Some(ri) = raw.img() else { return image };
        let mut b = 1;
        // SAFETY: a valid out pointer
        let scale = unsafe { platimg::ee_won_plat_pack_fit(ri.width(), ri.height(), w, h, shift, &mut b) };
        if scale == 0 {
            return image;
        }
        let pack = ri.native(ctx);
        let Some(pi) = pack.img() else { return image };
        // SAFETY: the pack copy's live surface
        if unsafe { (*pi.native_surf()).field(crate::surface::EeWonSurfaceField::Width) } != ri.width() {
            return image;
        }
        let o = EeWonImage::new_native(host, w, h, scale as f32);
        // the original's transparency, exactly: its mask, and black under it
        let m = img.native_mask();
        o.set_native_mask(m.clone());
        let pm = m.as_ref().map(|m| m.plat());
        // SAFETY: two live distinct surfaces; the mask's bits live while `m` does
        unsafe {
            platimg::ee_won_plat_pack_reduce(
                pi.native_surf(),
                o.native_surf(),
                w,
                h,
                scale,
                b,
                pm.as_ref().map_or(std::ptr::null(), |p| p as *const EeWonPlatMask),
            )
        };
        EeWonImage::wrap(o)
    }
}

// ---- the C ABI

/// An image's value (`ee_won_image_get`).
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonImageField {
    Width = 0,
    Height = 1,
    /// `EeWonImageKind`.
    Kind = 2,
    /// `RawImage::mType` (-1 for another kind).
    RawType = 3,
    HasTransparency = 4,
    /// A RawImage32's pixels (`GetImageData`), a native image's surface pixels (`GetPixels`).
    Pixels = 5,
    /// A native image's surface: its pixels per row.
    PitchPixels = 6,
    PixelWidth = 7,
    PixelHeight = 8,
    /// A native image's pixel scale's bits.
    Scale = 9,
    /// A native image's `EeWonSurface*`.
    Surface = 10,
    /// A native image's display context.
    Context = 11,
    /// A stretch image's borders.
    Left = 12,
    Top = 13,
    Right = 14,
    Bottom = 15,
    /// A 1- or 8-bit raw image's row pitch in bytes (a 32-bit one's width * 4).
    BytesPerRow = 16,
    /// A RawImage8's transparent index (-1: none).
    TransparentIndex = 17,
    /// A RawImage8's palette's colour count (-1: no palette).
    PaletteCount = 18,
    /// A stretch image's tiling, skip-centre and rebuild flags.
    TileHorz = 19,
    TileVert = 20,
    SkipCenter = 21,
    NeedRecalc = 22,
}

/// The calls (`ee_won_image_call`; `a`..`d` per call).
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonImageCall {
    /// `SetSize(a, b)`.
    SetSize = 0,
    /// `Duplicate()`.
    Duplicate = 1,
    /// `GetNative((DisplayContext*)a)`.
    Native = 2,
    /// `GetRaw()`.
    Raw = 3,
    /// `GetGraphics()` (`Graphics*`).
    Graphics = 4,
    /// RawImage32's `SetDoTransparency(a)`.
    SetTransparency = 5,
    /// StretchImage's `SetImage((RawImage*)a)` (the cache not marked).
    StretchSetImage = 6,
    /// StretchImage's `SetBorders(a, b, c, d)` (left, top, right, bottom).
    StretchSetBorders = 7,
    /// StretchImage's `CopyAttributes((Image*)a)` (its settings and size).
    StretchCopy = 8,
    /// StretchImage's `SetTile(a, b)`.
    StretchSetTile = 9,
    /// StretchImage's `SetSkipCenter(a)`.
    StretchSetSkip = 10,
    /// StretchCenterImage's `SetStretchImage((Image*)a)`.
    SetStretchImage = 11,
    /// StretchCenterImage's `SetCenterImage((Image*)a)`.
    SetCenterImage = 12,
    /// The crop filter (a, b, c, d: x, y, width, height).
    Crop = 13,
    /// The scale filter by percent (a, b).
    ScalePercent = 14,
    /// The scale filter to a size (a, b).
    ScaleTo = 15,
    /// The tint filter (a, b, c).
    Tint = 16,
    /// The transparent colour filter (a).
    Transparent = 17,
}

fn obj<'a>(o: *const EeWonImage) -> &'a EeWonImage {
    // SAFETY: every entry takes a live image (the C ABI's contract)
    unsafe { &*o }
}

/// The handle `me` now owns the image.
///
/// # Safety
/// `o` is a live image.
#[no_mangle]
pub unsafe extern "C" fn ee_won_image_bind(o: *const EeWonImage, me: isize) {
    obj(o).me.set(me);
}

/// The handle's destructor.
///
/// # Safety
/// `o` came from a maker here, freed once.
#[no_mangle]
pub unsafe extern "C" fn ee_won_image_free(o: *mut EeWonImage) {
    if !o.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(o) });
    }
}

#[no_mangle]
pub extern "C" fn ee_won_image_new_raw32(host: EeWonImageHostFn, w: i32, h: i32) -> *mut EeWonImage {
    Box::into_raw(EeWonImage::new_raw32(host, w, h))
}

/// `new RawImage8(w, h, NULL)`.
#[no_mangle]
pub extern "C" fn ee_won_image_new_raw8(host: EeWonImageHostFn, w: i32, h: i32) -> *mut EeWonImage {
    Box::into_raw(EeWonImage::new_raw8(host, w, h, None))
}

#[no_mangle]
pub extern "C" fn ee_won_image_new_ms(
    host: EeWonImageHostFn,
    image_type: u8,
    flags: u8,
    w: i32,
    h: i32,
) -> *mut EeWonImage {
    Box::into_raw(EeWonImage::new_ms(host, image_type, flags, w, h))
}

/// # Safety
/// `path` holds `len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_image_new_delay(
    host: EeWonImageHostFn,
    path: *const u8,
    len: usize,
    w: i32,
    h: i32,
) -> *mut EeWonImage {
    // SAFETY: the contract
    let p = if path.is_null() { &[][..] } else { unsafe { std::slice::from_raw_parts(path, len) } };
    Box::into_raw(EeWonImage::new_delay(host, p, w, h))
}

/// `new StretchImage((RawImage*)raw, left, top, right, bottom)`.
#[no_mangle]
pub extern "C" fn ee_won_image_new_stretch(
    host: EeWonImageHostFn,
    raw: isize,
    left: i32,
    top: i32,
    right: i32,
    bottom: i32,
) -> *mut EeWonImage {
    Box::into_raw(EeWonImage::new_stretch(host, raw, EeWonStretchImg::new(left, top, right, bottom)))
}

#[no_mangle]
pub extern "C" fn ee_won_image_new_stretch_center(
    host: EeWonImageHostFn,
    stretch: isize,
    center: isize,
) -> *mut EeWonImage {
    Box::into_raw(EeWonImage::new_stretch_center(host, stretch, center))
}

/// `new PlatNativeImage(context, w, h, scale)` (the context is not kept, as the original's was replaced).
#[no_mangle]
pub extern "C" fn ee_won_image_new_native(host: EeWonImageHostFn, w: i32, h: i32, scale: f32) -> *mut EeWonImage {
    Box::into_raw(EeWonImage::new_native(host, w, h, scale))
}

/// A value (`EeWonImageField`).
///
/// # Safety
/// `o` is a live image.
#[no_mangle]
pub unsafe extern "C" fn ee_won_image_get(o: *const EeWonImage, f: EeWonImageField) -> isize {
    let i = obj(o);
    let surface = |f: crate::surface::EeWonSurfaceField| -> isize {
        let s = i.native_surf();
        // SAFETY: a native image's live surface
        if s.is_null() {
            0
        } else {
            unsafe { (*s).field(f) as isize }
        }
    };
    let stretch = |g: fn(&EeWonStretchImg) -> i32| -> isize {
        if let Kind::Stretch { s, .. } = &i.kind {
            g(&s.get()) as isize
        } else {
            0
        }
    };
    use crate::surface::EeWonSurfaceField as SF;
    match f {
        EeWonImageField::Width => i.width() as isize,
        EeWonImageField::Height => i.height() as isize,
        EeWonImageField::Kind => i.kind_id() as isize,
        EeWonImageField::RawType => i.raw_type() as isize,
        EeWonImageField::HasTransparency => i.has_transparency() as isize,
        EeWonImageField::Pixels => match &i.kind {
            Kind::Raw32 { px, .. } => px.as_ptr() as isize,
            Kind::Raw8 { px, .. } | Kind::Raw1 { px, .. } => px.as_ptr() as isize,
            Kind::Native { surf, .. } if !surf.is_null() => {
                // SAFETY: the live surface
                unsafe { (**surf).pixels() as isize }
            }
            _ => 0,
        },
        EeWonImageField::PitchPixels => surface(SF::PixelsPerRow),
        EeWonImageField::PixelWidth => surface(SF::Width),
        EeWonImageField::PixelHeight => surface(SF::Height),
        EeWonImageField::Scale => i.native_scale().to_bits() as isize,
        EeWonImageField::Surface => match &i.kind {
            Kind::Native { surf, .. } => *surf as isize,
            _ => 0,
        },
        EeWonImageField::Context => match &i.kind {
            Kind::Native { context, .. } => context.get(),
            _ => 0,
        },
        EeWonImageField::Left => stretch(|s| s.left),
        EeWonImageField::Top => stretch(|s| s.top),
        EeWonImageField::Right => stretch(|s| s.right),
        EeWonImageField::Bottom => stretch(|s| s.bottom),
        EeWonImageField::TileHorz => stretch(|s| s.tile_horz as i32),
        EeWonImageField::TileVert => stretch(|s| s.tile_vert as i32),
        EeWonImageField::SkipCenter => stretch(|s| s.skip_center as i32),
        EeWonImageField::NeedRecalc => stretch(|s| s.need_recalc as i32),
        EeWonImageField::BytesPerRow => i.raw_pixels().map_or(0, |p| p.pitch as isize),
        EeWonImageField::TransparentIndex => match &i.kind {
            Kind::Raw8 { transparent, .. } => transparent.get() as isize,
            _ => -1,
        },
        EeWonImageField::PaletteCount => match &i.kind {
            Kind::Raw8 { palette, .. } => palette.borrow().as_ref().map_or(-1, |p| p.colors.len() as isize),
            _ => -1,
        },
    }
}

/// The image's kind (the original's class).
///
/// # Safety
/// `o` is a live image.
#[no_mangle]
pub unsafe extern "C" fn ee_won_image_kind(o: *const EeWonImage) -> EeWonImageKind {
    obj(o).kind_id()
}

/// `Draw((DisplayContext*)ctx, x, y, left, top, width, height)`.
///
/// # Safety
/// `o` is a live image.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_image_draw(
    o: *const EeWonImage,
    ctx: isize,
    x: i32,
    y: i32,
    left: i32,
    top: i32,
    w: i32,
    h: i32,
) {
    obj(o).draw(ctx, x, y, left, top, w, h);
}

/// A call (`EeWonImageCall`); an image comes back as a handle with one reference.
///
/// # Safety
/// `o` is a live image; the arguments as the call names them.
#[no_mangle]
pub unsafe extern "C" fn ee_won_image_call(
    o: *const EeWonImage,
    call: EeWonImageCall,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
) -> isize {
    let i = obj(o);
    let host = i.host;
    use EeWonImageCall as C;
    match call {
        C::SetSize => {
            i.set_size(a as i32, b as i32);
            0
        }
        C::Duplicate => i.duplicate().into_raw(),
        C::Native => i.native(a).into_raw(),
        C::Raw => i.get_raw().into_raw(),
        C::Graphics => i.graphics(),
        C::SetTransparency => {
            if let Kind::Raw32 { transparent, .. } = &i.kind {
                transparent.set(a != 0);
            }
            0
        }
        C::StretchSetImage => {
            if let Kind::Stretch { raw, .. } = &i.kind {
                i.set_held(raw, a);
            }
            0
        }
        C::StretchSetBorders | C::StretchSetTile | C::StretchSetSkip | C::StretchCopy => {
            if let Kind::Stretch { s, .. } = &i.kind {
                let mut st = s.get();
                match call {
                    C::StretchSetBorders => {
                        (st.left, st.top, st.right, st.bottom) = (a as i32, b as i32, c as i32, d as i32);
                    }
                    C::StretchSetTile => {
                        st.tile_horz = a != 0;
                        st.tile_vert = b != 0;
                    }
                    C::StretchSetSkip => st.skip_center = a != 0,
                    _ => {
                        let f = Ref::hold(host, a);
                        if let Some(Kind::Stretch { s: fs, .. }) = f.img().map(|x| &x.kind) {
                            st.copy_attributes(&fs.get());
                            let fi = f.img().expect("held");
                            i.width.set(fi.width.get());
                            i.height.set(fi.height.get());
                        }
                    }
                }
                s.set(st);
            }
            0
        }
        C::SetStretchImage | C::SetCenterImage => {
            if let Kind::StretchCenter { stretch, center } = &i.kind {
                i.set_held(if call == C::SetStretchImage { stretch } else { center }, a);
            }
            0
        }
        C::Crop => i.crop(a as i32, b as i32, c as i32, d as i32).map_or(0, Ref::into_raw),
        C::ScalePercent => i.scale(true, a as i32, b as i32, 0, 0).into_raw(),
        C::ScaleTo => i.scale(false, 0, 0, a as i32, b as i32).into_raw(),
        C::Tint => i.tint(a as i32, b as i32, c as i32).into_raw(),
        C::Transparent => i.transparent(a as u32).into_raw(),
    }
}

/// A RawImage8's palette: a new one of the `n` colours (the drivers'; the game's 8-bit images have none).
///
/// # Safety
/// `o` is a live image; `colors` holds `n` colours.
#[no_mangle]
pub unsafe extern "C" fn ee_won_image_set_palette(o: *const EeWonImage, colors: *const u32, n: usize) {
    if let Kind::Raw8 { palette, .. } = &obj(o).kind {
        let p = Palette::new(n);
        for (i, c) in p.colors.iter().enumerate() {
            // SAFETY: the contract
            c.set(unsafe { *colors.add(i) });
        }
        *palette.borrow_mut() = Some(p);
    }
}

/// A RawImage8's palette's first `cap` colours into `out`; its colour count (-1: none).
///
/// # Safety
/// `o` is a live image; `out` holds `cap` colours.
#[no_mangle]
pub unsafe extern "C" fn ee_won_image_palette(o: *const EeWonImage, out: *mut u32, cap: usize) -> isize {
    let Kind::Raw8 { palette, .. } = &obj(o).kind else { return -1 };
    let Some(p) = &*palette.borrow() else { return -1 };
    for (i, c) in p.colors.iter().take(cap).enumerate() {
        // SAFETY: the contract
        unsafe { *out.add(i) = c.get() };
    }
    p.colors.len() as isize
}

/// A new multi-decoder.
#[no_mangle]
pub extern "C" fn ee_won_img_decoder_new(host: EeWonImageHostFn) -> *mut EeWonImgDecoder {
    Box::into_raw(Box::new(EeWonImgDecoder::new(host)))
}

/// # Safety
/// `d` came from `ee_won_img_decoder_new`, freed once.
#[no_mangle]
pub unsafe extern "C" fn ee_won_img_decoder_free(d: *mut EeWonImgDecoder) {
    if !d.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(d) });
    }
}

/// `AddDecoder(kind's decoder, extension)`.
///
/// # Safety
/// `d` is live; `ext` a C string.
#[no_mangle]
pub unsafe extern "C" fn ee_won_img_decoder_add(d: *mut EeWonImgDecoder, kind: EeWonImgKind, ext: *const c_char) {
    // SAFETY: the contract
    unsafe { (*d).add(kind, CStr::from_ptr(ext).to_bytes()) };
}

/// What the decoder does with a path (`ee_won_img_decoder_run`).
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonImgDecoderRun {
    /// `Decode((DisplayContext*)ctx, path)`.
    Decode = 0,
    /// `DecodeRaw(path)`.
    DecodeRaw = 1,
    /// `GetImageAttributes(path)`: 1 with the size in `wh`, or 0.
    Attributes = 2,
}

/// A decode of `path`: an image's handle with one reference (or 0), and the FileReader status the C++ throws in
/// `*thrown` (0: none).
///
/// # Safety
/// `d` is live; `path` a C string; `wh` holds two ints; `thrown` valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_img_decoder_run(
    d: *mut EeWonImgDecoder,
    run: EeWonImgDecoderRun,
    ctx: isize,
    path: *const c_char,
    wh: *mut i32,
    thrown: *mut i32,
) -> isize {
    // SAFETY: the contract
    let (d, p) = unsafe { (&mut *d, CStr::from_ptr(path)) };
    // SAFETY: the contract
    unsafe { *thrown = 0 };
    let r = match run {
        EeWonImgDecoderRun::Decode => d.decode(ctx, p),
        EeWonImgDecoderRun::DecodeRaw => d.decode_raw(p),
        EeWonImgDecoderRun::Attributes => {
            return match d.attributes(p) {
                Some((w, h)) => {
                    // SAFETY: the contract
                    unsafe { (*wh, *wh.add(1)) = (w, h) };
                    1
                }
                None => 0,
            };
        }
    };
    match r {
        Ok(i) => i.map_or(0, Ref::into_raw),
        Err(s) => {
            // SAFETY: the contract
            unsafe { *thrown = s };
            0
        }
    }
}

#[cfg(test)]
#[path = "imgobj_tests.rs"]
mod tests;
