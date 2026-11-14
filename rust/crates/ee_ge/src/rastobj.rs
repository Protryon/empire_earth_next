//! `GERasterizer`'s base state as a whole Rust object (R4.3, twelfth part): the ambient colours, the per-screen
//! render state records, the debug font and text, the drawing lock, the water ripple, the culling, the flags, the
//! name and the screen id, with every method of GERasterizer.cpp (the constructor's debug text, the destructor,
//! `Activate`, `AddValidScreen`, `CreateFontBitmap`, `Deactivate`, the two `GetPixelSize`s, the two model visibility
//! tests and the state setters) and the statics (the rasterizer list and the active rasterizer).
//!
//! As `bmpobj.rs`: the C++ `GERasterizer` stays the abstract base DX7Rasterizer derives from (its vtable, its pure
//! virtuals); its methods are one-line forwarders and its only member is this object. Every call the original made on
//! another object (the scheduler, the font, the text, the font bitmap, the surfaces, the textures, the models, the
//! placed models, its own `ActivateRasterizer`) is one host operation (`EeGeRastOp`, `GERasterizer::Host`), in the
//! original's order. Floats cross the host as their bits.
//!
//! A screen's record (`EeGeRastScreen`) is `GERScreenState`'s layout: the plug-in reads and writes its device-side
//! fields through `GERasterizer::GetScreenState`, a pointer into the record, as it read the C++ struct (GERasterizer.cpp
//! pins the layout). The viewport inside it is `GeViewport`, `GEViewport`'s mirror, so `GetViewport` still returns a
//! reference.

use std::ffi::{c_ulong, c_void};
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Mutex;

use crate::types::{Ge3DPoint, GeBoundingBox, GeTransformation, GeVector};
use crate::viewport::GeViewport;

/// `GEColor`: red, green, blue.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeGeRastColor {
    pub red: f32,
    pub green: f32,
    pub blue: f32,
}

/// `GEColor()`: white.
const WHITE: EeGeRastColor = EeGeRastColor { red: 1.0, green: 1.0, blue: 1.0 };

/// `GERFogAttributes` (sizeof 0x20 on Windows).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeGeRastFog {
    pub color: EeGeRastColor,
    pub kind: c_ulong,
    pub density: f32,
    pub start: f32,
    pub end: f32,
    /// `1 / (end - start)`.
    pub scale: f32,
}

/// `GERScreenState`: one screen's render state, in the C++'s member order.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeGeRastScreen {
    pub viewport: GeViewport,
    pub fog: EeGeRastFog,
    pub screen: *mut c_void,
    pub primary_surface: *mut c_void,
    pub secondary_surface: *mut c_void,
    pub render_target: *mut c_void,
    pub texture: *mut c_void,
    pub window: *mut c_void,
    pub alpha_testing_ref_value: f32,
    pub drawn_poly_area: f32,
    pub drawn_poly_area_per_frame: f32,
    pub drawn_poly_area_per_second: f32,
    pub drawn_polys_per_frame: f32,
    pub drawn_polys_per_second: f32,
    pub frames_per_second: f32,
    pub scene_polys_per_frame: f32,
    pub scene_polys_per_second: f32,
    pub total_texture_changes_per_frame: f32,
    pub alpha_testing: c_ulong,
    pub alpha_testing_compare_state: c_ulong,
    pub anti_aliasing: c_ulong,
    pub blend_source: c_ulong,
    pub blend_destination: c_ulong,
    pub color_keying: c_ulong,
    pub dithering: c_ulong,
    pub draw_style: c_ulong,
    pub fog_state: c_ulong,
    pub lighting: c_ulong,
    pub projection_mode: c_ulong,
    pub shading: c_ulong,
    pub shading_color_source: c_ulong,
    pub specular: c_ulong,
    pub z_buffer: c_ulong,
    pub z_compare: c_ulong,
    pub drawn_poly_count: c_ulong,
    /// The plug-in's frame counters (frames, scene polygons, the last time, texture changes).
    pub counters: [c_ulong; 4],
    /// The plug-in's "a texture is set".
    pub has_texture: bool,
    pub is_screen_activated: bool,
    pub wait_for_vsync: bool,
}

impl EeGeRastScreen {
    /// `GERScreenState(inScreen)`: a default viewport and fog, no surfaces, zero statistics, waiting for the vertical
    /// sync. The render states and the alpha reference the original left unset are 0.
    fn new(screen: *mut c_void) -> EeGeRastScreen {
        EeGeRastScreen {
            viewport: GeViewport::default(),
            fog: EeGeRastFog { color: WHITE, kind: 0, density: 1.0, start: 0.0, end: 1.0, scale: 1.0 },
            screen,
            primary_surface: std::ptr::null_mut(),
            secondary_surface: std::ptr::null_mut(),
            render_target: std::ptr::null_mut(),
            texture: std::ptr::null_mut(),
            window: std::ptr::null_mut(),
            alpha_testing_ref_value: 0.0,
            drawn_poly_area: 0.0,
            drawn_poly_area_per_frame: 0.0,
            drawn_poly_area_per_second: 0.0,
            drawn_polys_per_frame: 0.0,
            drawn_polys_per_second: 0.0,
            frames_per_second: 0.0,
            scene_polys_per_frame: 0.0,
            scene_polys_per_second: 0.0,
            total_texture_changes_per_frame: 0.0,
            alpha_testing: 0,
            alpha_testing_compare_state: 0,
            anti_aliasing: 0,
            blend_source: 0,
            blend_destination: 0,
            color_keying: 0,
            dithering: 0,
            draw_style: 0,
            fog_state: 0,
            lighting: 0,
            projection_mode: 0,
            shading: 0,
            shading_color_source: 0,
            specular: 0,
            z_buffer: 0,
            z_compare: 0,
            drawn_poly_count: 0,
            counters: [0; 4],
            has_texture: false,
            is_screen_activated: false,
            wait_for_vsync: true,
        }
    }
}

/// The calls the rasterizer makes on other objects. `obj` is the object called; `a`, `b` as each says.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeGeRastOp {
    /// `new UString(*(const UString*)a)` (the name).
    NameNew = 1,
    /// `delete (UString*)obj`.
    NameDelete,
    /// `TSScheduler::CreateCriticalSection(*(TSCriticalSection**)a)`.
    LockCreate,
    /// `TSScheduler::DestroyCriticalSection((TSCriticalSection*)obj)`.
    LockDestroy,
    /// `ESVERIFY_AT(a != 0, b)`.
    Verify,
    /// `new GEFont("Arial Bold", 9, 0, kFontQualityAntialiased)`.
    FontNew,
    /// `obj->Dereference()` (a font).
    FontDereference,
    /// `new GEText((GEFont*)a)`.
    TextNew,
    /// `delete (GEText*)obj`.
    TextDelete,
    /// `obj->SetAlignment(a)`.
    TextSetAlignment,
    /// `obj->SetFgColor(*(const GEColor*)a)`.
    TextSetFgColor,
    /// `obj->SetBgColor(*(const GEColor*)a)`.
    TextSetBgColor,
    /// `obj->SetFont((GEFont*)a)`.
    TextSetFont,
    /// `obj->SetIsTransparent(a != 0)`.
    TextSetIsTransparent,
    /// `obj->SetLocation(a's bits, b's bits)`.
    TextSetLocation,
    /// The owner's `ActivateRasterizer()`.
    ActivateRasterizer,
    /// `new GEFontBitmap(owner, (GEFont*)a, b)`.
    FontBitmapNew,
    /// `obj->Dereference()` (a surface).
    SurfaceDereference,
    /// `obj->Dereference()` (a texture: GETexture's own).
    TextureDereference,
    /// `*(GE3DPoint*)a = obj->GetCenterPoint()` (a model).
    ModelCenter,
    /// `obj->GetBoundingRadius(a)`, as its bits.
    ModelBoundingRadius,
    /// `obj->GetGraphicsBoundingRadius()`, as its bits.
    ModelGraphicsRadius,
    /// `*(GEBoundingBox*)a = obj->GetGraphicsBoundingBox()`.
    ModelGraphicsBox,
    /// `obj->SetClipToPlane(a, b != 0)`.
    ModelSetClip,
    /// `*(GETransformation*)b = obj->GetInternalTransform(a)`.
    ModelInternalTransform,
    /// `obj->GetScale()` (a placed model), as its bits.
    PlacedScale,
    /// `obj->GetFrame()`.
    PlacedFrame,
    /// `obj->GetModel()`.
    PlacedModel,
    /// `*(GETransformation*)a = obj->GetLocation()`.
    PlacedLocation,
}

/// The host: `owner` is the C++ rasterizer of the object calling.
pub type EeGeRastHostFn =
    unsafe extern "C" fn(owner: *mut c_void, op: EeGeRastOp, obj: *mut c_void, a: isize, b: isize) -> isize;

/// A field of the rasterizer (`ee_ge_rast_get`/`ee_ge_rast_set`). Floats cross as their bits.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeGeRastVal {
    /// `mAmbientIntensity` (bits; read only: `SetAmbientIntensity` sets it).
    AmbientIntensity = 1,
    /// `mCulling` (read only: `SetCulling` sets it).
    Culling,
    /// `mIsPolyAreaProfiling`.
    IsPolyAreaProfiling,
    /// `mIsActivated` (read only).
    IsActivated,
    /// `mName`: the C++ `UString*` (read only).
    Name,
    /// `mScreenStates.size()` (read only).
    ScreenCount,
    /// `mScreenID`.
    ScreenId,
    /// `mDrawingAccess` (read only).
    DrawingAccess,
    /// `mDebugFont` (read only).
    DebugFont,
    /// `mDebugText` (read only).
    DebugText,
}

/// A render state of one screen (`ee_ge_rast_screen_set`): the base's setters.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeGeRastState {
    AlphaTesting = 1,
    AlphaTestingCompareState,
    /// Bits of a float.
    AlphaTestingRefValue,
    AntiAliasing,
    ColorKeying,
    Dithering,
    DrawStyle,
    /// `SetFog`.
    FogState,
    /// `SetFogType`.
    FogType,
    /// `SetFogValue`: bits of a float.
    FogDensity,
    Lighting,
    ProjectionMode,
    Shading,
    ShadingColorSource,
    Specular,
    /// A bool.
    WaitForVSync,
    ZBuffer,
    ZCompare,
}

/// `GEModel::SetClipToPlane`'s planes.
const RAST_CLIP_FRONT: isize = 0;
const RAST_CLIP_REAR: isize = 1;
const RAST_CLIP_TOP: isize = 2;
const RAST_CLIP_BOTTOM: isize = 3;
const RAST_CLIP_LEFT: isize = 4;
const RAST_CLIP_RIGHT: isize = 5;

/// `kTextAlignLeft`.
const TEXT_ALIGN_LEFT: isize = 0;
/// `kCullCCW`.
const CULL_CCW: c_ulong = 2;
/// `kErrorOK`.
const ERROR_OK: isize = 0;

/// The rasterizer.
pub struct EeGeRasterizer {
    host: EeGeRastHostFn,
    /// The C++ rasterizer (`this`).
    owner: usize,
    ambient_color: EeGeRastColor,
    computed_ambient_color: EeGeRastColor,
    /// `mScreenStates`: each record boxed, its address handed to the plug-in.
    screens: Vec<*mut EeGeRastScreen>,
    debug_font: usize,
    debug_text: usize,
    drawing_access: usize,
    ambient_intensity: f32,
    water_ripple: [f32; 3],
    culling: c_ulong,
    is_poly_area_profiling: bool,
    is_activated: bool,
    /// `mName`, a C++ `UString`.
    name: usize,
    screen_id: c_ulong,
}

/// `GERasterizer::sRasterizers`: the plug-ins' rasterizers, in load order.
static RASTERIZERS: Mutex<Vec<usize>> = Mutex::new(Vec::new());
/// `GERasterizer::sActiveRasterizer`.
static ACTIVE: AtomicUsize = AtomicUsize::new(0);

/// # Safety
/// `p` is a live object from `ee_ge_rast_new`.
unsafe fn rs<'a>(p: *mut EeGeRasterizer) -> &'a mut EeGeRasterizer {
    // SAFETY: the caller's contract; the reference is not held across a host call
    unsafe { &mut *p }
}

/// `GEViewport::NormalizePerspectiveCameraPoint`'s x and w.
fn perspective_x(v: &GeViewport, p: Ge3DPoint) -> (f32, f32) {
    (p.x * v.perspective_scale, p.y)
}

/// `GEViewport::NormalizeOrthographicCameraPoint`'s x.
fn orthographic_x(v: &GeViewport, p: Ge3DPoint) -> f32 {
    p.x * v.orthographic_scale
}

/// `GEVector::Scale(s, out)`.
fn scaled(v: GeVector, s: f32) -> GeVector {
    GeVector { x: s * v.x, y: s * v.y, z: s * v.z }
}

/// The width in pixels of a sphere of `radius` around the camera point `camera` (the end of both `GetPixelSize`s).
fn pixel_width(v: &GeViewport, mut camera: Ge3DPoint, radius: f32, orthographic: bool) -> f32 {
    let x = camera.x;
    camera.x = x - radius;
    let mut width;
    if !orthographic {
        let (px, w) = perspective_x(v, camera);
        let left = if w != 0.0 { px / w } else { 0.0 };
        camera.x = x + radius;
        let (px, w) = perspective_x(v, camera);
        width = if w != 0.0 { px / w - left } else { 0.0 };
    } else {
        let left = orthographic_x(v, camera);
        camera.x = x + radius;
        width = orthographic_x(v, camera) - left;
    }
    if width >= 0.0 {
        width *= v.half_width;
    } else {
        width = v.half_width * 2.0;
    }
    width
}

impl EeGeRasterizer {
    /// A host call made by rasterizer `p`.
    ///
    /// # Safety
    /// `p` is live; the operation's arguments are as `EeGeRastOp` describes.
    unsafe fn op(p: *mut Self, op: EeGeRastOp, obj: usize, a: isize, b: isize) -> isize {
        // SAFETY: the caller's contract (host and owner are copied out before the call)
        let (host, owner) = unsafe { (rs(p).host, rs(p).owner) };
        // SAFETY: the host's contract
        unsafe { host(owner as *mut c_void, op, obj as *mut c_void, a, b) }
    }

    /// A float a host call answered as its bits.
    ///
    /// # Safety
    /// As `op`.
    unsafe fn float(p: *mut Self, op: EeGeRastOp, obj: usize, a: isize) -> f32 {
        // SAFETY: the caller's contract
        f32::from_bits(unsafe { Self::op(p, op, obj, a, 0) } as u32)
    }

    /// `GERasterizer(inName)`: the name, the drawing lock, the white-on-black Arial 9 debug text; not activated,
    /// culling counter-clockwise, screen 1, no ripple.
    ///
    /// # Safety
    /// `host` answers every operation for `owner`; `name` is a C++ `UString`.
    pub unsafe fn create(host: EeGeRastHostFn, owner: usize, name: usize) -> *mut Self {
        let p = Box::into_raw(Box::new(EeGeRasterizer {
            host,
            owner,
            ambient_color: WHITE,
            computed_ambient_color: WHITE,
            screens: Vec::new(),
            debug_font: 0,
            debug_text: 0,
            drawing_access: 0,
            ambient_intensity: 0.0,
            water_ripple: [0.0; 3],
            culling: 0,
            is_poly_area_profiling: false,
            is_activated: false,
            name: 0,
            screen_id: 0,
        }));
        // SAFETY: p is live; the host's contract
        unsafe {
            rs(p).name = Self::op(p, EeGeRastOp::NameNew, 0, name as isize, 0) as usize;
            let mut lock: usize = 0;
            let result = Self::op(p, EeGeRastOp::LockCreate, 0, &mut lock as *mut usize as isize, 0);
            rs(p).drawing_access = lock;
            Self::op(p, EeGeRastOp::Verify, 0, (result == ERROR_OK) as isize, 41);
            let font = Self::op(p, EeGeRastOp::FontNew, 0, 0, 0) as usize;
            rs(p).debug_font = font;
            let text = Self::op(p, EeGeRastOp::TextNew, 0, font as isize, 0) as usize;
            rs(p).debug_text = text;
            Self::op(p, EeGeRastOp::TextSetAlignment, text, TEXT_ALIGN_LEFT, 0);
            let white = WHITE;
            Self::op(p, EeGeRastOp::TextSetFgColor, text, &white as *const EeGeRastColor as isize, 0);
            let black = EeGeRastColor { red: 0.0, green: 0.0, blue: 0.0 };
            Self::op(p, EeGeRastOp::TextSetBgColor, text, &black as *const EeGeRastColor as isize, 0);
            Self::op(p, EeGeRastOp::TextSetFont, text, font as isize, 0);
            Self::op(p, EeGeRastOp::TextSetIsTransparent, text, 0, 0);
            Self::op(p, EeGeRastOp::TextSetLocation, text, 0.0f32.to_bits() as isize, 0.0f32.to_bits() as isize);
            let r = rs(p);
            r.is_activated = false;
            r.is_poly_area_profiling = false;
            r.culling = CULL_CCW;
            r.screen_id = 1;
            r.water_ripple = [0.0; 3];
        }
        p
    }

    /// `~GERasterizer()`: the debug font and text and the drawing lock released, then the name. The screen records
    /// are freed without releasing their surfaces (the original's vector freed only its array).
    ///
    /// # Safety
    /// `p` is live; it is freed.
    pub unsafe fn destroy(p: *mut Self) {
        // SAFETY: p is live; the host's contract
        unsafe {
            let (font, text, lock) = (rs(p).debug_font, rs(p).debug_text, rs(p).drawing_access);
            Self::op(p, EeGeRastOp::FontDereference, font, 0, 0);
            if text != 0 {
                Self::op(p, EeGeRastOp::TextDelete, text, 0, 0);
            }
            Self::op(p, EeGeRastOp::LockDestroy, lock, 0, 0);
            let name = rs(p).name;
            Self::op(p, EeGeRastOp::NameDelete, name, 0, 0);
            let b = Box::from_raw(p);
            for &s in &b.screens {
                drop(Box::from_raw(s));
            }
        }
    }

    /// `Activate()`: the plug-in's `ActivateRasterizer`, then activated (and the active rasterizer) on success.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn activate(p: *mut Self) -> isize {
        // SAFETY: p is live; no reference is held across the host call (it re-enters through AddValidScreen)
        unsafe {
            let result = Self::op(p, EeGeRastOp::ActivateRasterizer, 0, 0, 0);
            let activated = result == ERROR_OK;
            rs(p).is_activated = activated;
            ACTIVE.store(if activated { rs(p).owner } else { 0 }, Ordering::SeqCst);
            result
        }
    }

    /// `Deactivate()`: every screen's record deleted (releasing its surfaces and texture), no active rasterizer.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn deactivate(p: *mut Self) {
        // SAFETY: p is live; the records are taken out before the host calls
        unsafe {
            let screens = std::mem::take(&mut rs(p).screens);
            for s in screens {
                let state = *Box::from_raw(s);
                for (op, obj) in [
                    (EeGeRastOp::SurfaceDereference, state.primary_surface),
                    (EeGeRastOp::SurfaceDereference, state.secondary_surface),
                    (EeGeRastOp::SurfaceDereference, state.render_target),
                    (EeGeRastOp::TextureDereference, state.texture),
                ] {
                    if !obj.is_null() {
                        Self::op(p, op, obj as usize, 0, 0);
                    }
                }
            }
            ACTIVE.store(0, Ordering::SeqCst);
            rs(p).is_activated = false;
        }
    }

    /// The record of screen `screen`, or NULL past the screens.
    ///
    /// # Safety
    /// `p` is live.
    unsafe fn screen(p: *mut Self, screen: c_ulong) -> *mut EeGeRastScreen {
        // SAFETY: the caller's contract
        unsafe { rs(p).screens.get(screen as usize).copied().unwrap_or(std::ptr::null_mut()) }
    }

    /// A model's camera-space centre: `transform` then the viewport's camera inverse.
    ///
    /// # Safety
    /// `p` is live; `model` is a C++ model.
    unsafe fn camera_center(p: *mut Self, model: usize, v: &GeViewport, t: &GeTransformation) -> Ge3DPoint {
        let mut center = Ge3DPoint::default();
        // SAFETY: the caller's contract
        unsafe { Self::op(p, EeGeRastOp::ModelCenter, model, &mut center as *mut Ge3DPoint as isize, 0) };
        v.camera_inverse.apply_point(t.apply_point(center))
    }

    /// `GetPixelSize(model, viewport, transformation, scale, orthographic)`: frame 0's radius.
    ///
    /// # Safety
    /// `p` is live; `model` is a C++ model.
    pub unsafe fn pixel_size(
        p: *mut Self,
        model: usize,
        v: &GeViewport,
        t: &GeTransformation,
        scale: f32,
        orthographic: bool,
    ) -> f32 {
        // SAFETY: the caller's contract
        unsafe {
            let camera = Self::camera_center(p, model, v, t);
            let radius = Self::float(p, EeGeRastOp::ModelBoundingRadius, model, 0) * scale;
            pixel_width(v, camera, radius, orthographic)
        }
    }

    /// `GetPixelSize(placed model, viewport, orthographic)`: its location times its frame's internal transform,
    /// scaled about its location, and its frame's radius.
    ///
    /// # Safety
    /// `p` is live; `placed` is a C++ `GEPhysicalModel` with a model.
    pub unsafe fn placed_pixel_size(p: *mut Self, placed: usize, v: &GeViewport, orthographic: bool) -> f32 {
        // SAFETY: the caller's contract
        unsafe {
            let scale = Self::float(p, EeGeRastOp::PlacedScale, placed, 0);
            let frame = Self::op(p, EeGeRastOp::PlacedFrame, placed, 0, 0);
            let model = Self::op(p, EeGeRastOp::PlacedModel, placed, 0, 0) as usize;
            let mut location = GeTransformation::IDENTITY;
            Self::op(p, EeGeRastOp::PlacedLocation, placed, &mut location as *mut GeTransformation as isize, 0);
            let mut internal = GeTransformation::IDENTITY;
            Self::op(
                p,
                EeGeRastOp::ModelInternalTransform,
                model,
                frame,
                &mut internal as *mut GeTransformation as isize,
            );
            let mut t = location.compose(&internal);
            if scale != 1.0 {
                t.scale(scale, scale, scale);
                t.m[0][3] = location.m[0][3];
                t.m[1][3] = location.m[1][3];
                t.m[2][3] = location.m[2][3];
            }
            let camera = Self::camera_center(p, model, v, &t);
            let radius = Self::float(p, EeGeRastOp::ModelBoundingRadius, model, frame) * scale;
            pixel_width(v, camera, radius, orthographic)
        }
    }

    /// `IsOrthographicModelVisible`: the model's box in camera space against the clipping distances and the
    /// orthographic rectangle, plane by plane; each plane crossed is set on the model as a clip plane.
    ///
    /// # Safety
    /// `p` is live; `model` is a C++ model.
    pub unsafe fn orthographic_visible(
        p: *mut Self,
        model: usize,
        v: &GeViewport,
        t: &GeTransformation,
    ) -> (bool, bool) {
        let front = v.front_clipping_distance;
        let rear = v.rear_clipping_distance;
        let camera = v.camera_inverse.compose(t);
        let mut source = GeBoundingBox::default();
        // SAFETY: the caller's contract
        unsafe { Self::op(p, EeGeRastOp::ModelGraphicsBox, model, &mut source as *mut GeBoundingBox as isize, 0) };
        let b = source.transform(&camera);
        let rect = v.orthographic_rectangle;
        let y_clip = -v.y_clipping_plane;
        let top = rect.top * y_clip;
        let bottom = rect.bottom * y_clip;
        let (lo, hi) = (b.bottom_left_front, b.top_right_back);
        // (low, high, minimum, maximum, the planes at each end) in the original's order: y, x, z
        let tests = [
            (lo.y, hi.y, front, rear, RAST_CLIP_FRONT, RAST_CLIP_REAR),
            (lo.x, hi.x, rect.left, rect.right, RAST_CLIP_LEFT, RAST_CLIP_RIGHT),
            (lo.z, hi.z, bottom, top, RAST_CLIP_BOTTOM, RAST_CLIP_TOP),
        ];
        let mut clipped = false;
        for (low, high, minimum, maximum, min_plane, max_plane) in tests {
            if !(high >= minimum) {
                return (false, clipped);
            }
            let crosses = !(low >= minimum);
            // SAFETY: the caller's contract
            unsafe { Self::op(p, EeGeRastOp::ModelSetClip, model, min_plane, crosses as isize) };
            clipped |= crosses;
            if !(low <= maximum) {
                return (false, clipped);
            }
            let crosses = !(high <= maximum);
            // SAFETY: the caller's contract
            unsafe { Self::op(p, EeGeRastOp::ModelSetClip, model, max_plane, crosses as isize) };
            clipped |= crosses;
        }
        (true, clipped)
    }

    /// `IsPerspectiveModelVisible`: the model's sphere against the clipping distances and the four side planes;
    /// when visible, each plane the sphere crosses is set on the model as a clip plane.
    ///
    /// # Safety
    /// `p` is live; `model` is a C++ model.
    pub unsafe fn perspective_visible(
        p: *mut Self,
        model: usize,
        scale: f32,
        v: &GeViewport,
        t: &GeTransformation,
    ) -> (bool, bool) {
        let planes = [v.left_plane, v.right_plane, v.top_plane, v.bottom_plane];
        let mut normals = planes.map(|plane| plane.normal);
        // SAFETY: the caller's contract
        let center = unsafe { Self::camera_center(p, model, v, t) };
        // SAFETY: the caller's contract
        let radius = unsafe { Self::float(p, EeGeRastOp::ModelGraphicsRadius, model, 0) } * scale;
        for n in &mut normals {
            *n = scaled(*n, radius);
        }
        if !(center.y + radius >= v.front_clipping_distance) || !(center.y - radius <= v.rear_clipping_distance) {
            return (false, false);
        }
        for (plane, n) in planes.iter().zip(&normals) {
            let point = Ge3DPoint { x: center.x + n.x, y: center.y + n.y, z: center.z + n.z };
            if !plane.is_point_on_or_above(point) {
                return (false, false);
            }
        }
        let mut count = 0u32;
        let mut clip = |plane: isize, crosses: bool| {
            // SAFETY: the caller's contract
            unsafe { Self::op(p, EeGeRastOp::ModelSetClip, model, plane, crosses as isize) };
            count += crosses as u32;
        };
        clip(RAST_CLIP_FRONT, center.y - radius < v.front_clipping_distance);
        clip(RAST_CLIP_REAR, center.y + radius > v.rear_clipping_distance);
        for ((plane, n), id) in
            planes.iter().zip(&normals).zip([RAST_CLIP_LEFT, RAST_CLIP_RIGHT, RAST_CLIP_TOP, RAST_CLIP_BOTTOM])
        {
            let point = Ge3DPoint { x: center.x - n.x, y: center.y - n.y, z: center.z - n.z };
            clip(id, plane.is_point_on_or_below(point));
        }
        (true, count != 0)
    }

    /// `SetAmbientColor` / `SetAmbientIntensity`: the computed colour is the colour times the intensity.
    fn compute_ambient(&mut self) {
        let (c, i) = (self.ambient_color, self.ambient_intensity);
        self.computed_ambient_color = EeGeRastColor { red: c.red * i, green: c.green * i, blue: c.blue * i };
    }
}

/// `GERasterizer(inName)`.
///
/// # Safety
/// `host` answers every operation for `owner`; `name` is a C++ `UString`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_rast_new(
    host: EeGeRastHostFn,
    owner: *mut c_void,
    name: *const c_void,
) -> *mut EeGeRasterizer {
    // SAFETY: the caller's contract
    unsafe { EeGeRasterizer::create(host, owner as usize, name as usize) }
}

/// `~GERasterizer()`.
///
/// # Safety
/// `p` is live; it is freed.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_rast_destroy(p: *mut EeGeRasterizer) {
    // SAFETY: the caller's contract
    unsafe { EeGeRasterizer::destroy(p) }
}

/// `Activate()`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_rast_activate(p: *mut EeGeRasterizer) -> isize {
    // SAFETY: the caller's contract
    unsafe { EeGeRasterizer::activate(p) }
}

/// `AddValidScreen(inScreen)` (the base's): a record for another screen.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_rast_add_screen(p: *mut EeGeRasterizer, screen: *mut c_void) {
    let record = Box::into_raw(Box::new(EeGeRastScreen::new(screen)));
    // SAFETY: the caller's contract
    unsafe { rs(p).screens.push(record) };
}

/// `CreateFontBitmap(inFont, inScreen)`: a font bitmap (it registers itself).
///
/// # Safety
/// `p` is live; `font` is a C++ font.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_rast_create_font_bitmap(p: *mut EeGeRasterizer, font: *mut c_void, screen: c_ulong) {
    // SAFETY: the caller's contract
    unsafe { EeGeRasterizer::op(p, EeGeRastOp::FontBitmapNew, 0, font as isize, screen as isize) };
}

/// `Deactivate()` (the base's).
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_rast_deactivate(p: *mut EeGeRasterizer) {
    // SAFETY: the caller's contract
    unsafe { EeGeRasterizer::deactivate(p) }
}

/// `GetScreenState(inScreen)`: the screen's record (`GERScreenState`'s layout), NULL past the screens.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_rast_screen(p: *mut EeGeRasterizer, screen: c_ulong) -> *mut EeGeRastScreen {
    // SAFETY: the caller's contract
    unsafe { EeGeRasterizer::screen(p, screen) }
}

/// A field read (the header's accessors).
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_rast_get(p: *mut EeGeRasterizer, field: EeGeRastVal) -> isize {
    // SAFETY: the caller's contract
    let r = unsafe { rs(p) };
    match field {
        EeGeRastVal::AmbientIntensity => r.ambient_intensity.to_bits() as isize,
        EeGeRastVal::Culling => r.culling as isize,
        EeGeRastVal::IsPolyAreaProfiling => r.is_poly_area_profiling as isize,
        EeGeRastVal::IsActivated => r.is_activated as isize,
        EeGeRastVal::Name => r.name as isize,
        EeGeRastVal::ScreenCount => r.screens.len() as isize,
        EeGeRastVal::ScreenId => r.screen_id as isize,
        EeGeRastVal::DrawingAccess => r.drawing_access as isize,
        EeGeRastVal::DebugFont => r.debug_font as isize,
        EeGeRastVal::DebugText => r.debug_text as isize,
    }
}

/// A field write (`SetPolyAreaProfiling`, `SetScreenID`, the base's `SetCulling`); the read-only fields are ignored.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_rast_set(p: *mut EeGeRasterizer, field: EeGeRastVal, value: isize) {
    // SAFETY: the caller's contract
    let r = unsafe { rs(p) };
    match field {
        EeGeRastVal::Culling => r.culling = value as c_ulong,
        EeGeRastVal::IsPolyAreaProfiling => r.is_poly_area_profiling = value != 0,
        EeGeRastVal::ScreenId => r.screen_id = value as c_ulong,
        _ => {}
    }
}

/// `GetAmbientColor()` (`computed` false) or `GetComputedAmbientColor()`: the colour inside the object.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_rast_color(p: *mut EeGeRasterizer, computed: bool) -> *const EeGeRastColor {
    // SAFETY: the caller's contract
    let r = unsafe { rs(p) };
    if computed {
        &r.computed_ambient_color
    } else {
        &r.ambient_color
    }
}

/// `SetAmbientColor(inColor, ...)` (the screen is ignored).
///
/// # Safety
/// `p` is live; `color` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_rast_set_ambient_color(p: *mut EeGeRasterizer, color: *const EeGeRastColor) {
    // SAFETY: the caller's contract
    unsafe {
        let r = rs(p);
        r.ambient_color = *color;
        r.compute_ambient();
    }
}

/// `SetAmbientIntensity(inIntensity, ...)` (the screen is ignored).
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_rast_set_ambient_intensity(p: *mut EeGeRasterizer, intensity: f32) {
    // SAFETY: the caller's contract
    let r = unsafe { rs(p) };
    r.ambient_intensity = intensity;
    r.compute_ambient();
}

/// `SetWaterRippleEffect` (`set`) or `GetWaterRippleEffect`: the three ripple values.
///
/// # Safety
/// `p` is live; `ripple` points at three floats.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_rast_ripple(p: *mut EeGeRasterizer, ripple: *mut [f32; 3], set: bool) {
    // SAFETY: the caller's contract
    unsafe {
        if set {
            rs(p).water_ripple = *ripple;
        } else {
            *ripple = rs(p).water_ripple;
        }
    }
}

/// One of the base's render state setters on screen `screen` (floats as their bits).
///
/// # Safety
/// `p` is live; `screen` is one of its screens.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_rast_screen_set(
    p: *mut EeGeRasterizer,
    screen: c_ulong,
    state: EeGeRastState,
    value: isize,
) {
    // SAFETY: the caller's contract
    let s = unsafe { &mut *EeGeRasterizer::screen(p, screen) };
    let v = value as c_ulong;
    let f = f32::from_bits(value as u32);
    match state {
        EeGeRastState::AlphaTesting => s.alpha_testing = v,
        EeGeRastState::AlphaTestingCompareState => s.alpha_testing_compare_state = v,
        EeGeRastState::AlphaTestingRefValue => s.alpha_testing_ref_value = f,
        EeGeRastState::AntiAliasing => s.anti_aliasing = v,
        EeGeRastState::ColorKeying => s.color_keying = v,
        EeGeRastState::Dithering => s.dithering = v,
        EeGeRastState::DrawStyle => s.draw_style = v,
        EeGeRastState::FogState => s.fog_state = v,
        EeGeRastState::FogType => s.fog.kind = v,
        EeGeRastState::FogDensity => s.fog.density = f,
        EeGeRastState::Lighting => s.lighting = v,
        EeGeRastState::ProjectionMode => s.projection_mode = v,
        EeGeRastState::Shading => s.shading = v,
        EeGeRastState::ShadingColorSource => s.shading_color_source = v,
        EeGeRastState::Specular => s.specular = v,
        EeGeRastState::WaitForVSync => s.wait_for_vsync = value != 0,
        EeGeRastState::ZBuffer => s.z_buffer = v,
        EeGeRastState::ZCompare => s.z_compare = v,
    }
}

/// `SetBlending(inSource, inDestination, inScreen)`.
///
/// # Safety
/// `p` is live; `screen` is one of its screens.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_rast_set_blending(
    p: *mut EeGeRasterizer,
    screen: c_ulong,
    source: c_ulong,
    destination: c_ulong,
) {
    // SAFETY: the caller's contract
    let s = unsafe { &mut *EeGeRasterizer::screen(p, screen) };
    s.blend_source = source;
    s.blend_destination = destination;
}

/// `SetFogColor(inColor, inScreen)`.
///
/// # Safety
/// `p` is live; `screen` is one of its screens; `color` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_rast_set_fog_color(
    p: *mut EeGeRasterizer,
    screen: c_ulong,
    color: *const EeGeRastColor,
) {
    // SAFETY: the caller's contract
    unsafe { (*EeGeRasterizer::screen(p, screen)).fog.color = *color };
}

/// `SetFogRange(inStart, inEnd, inScreen)`: the range and its scale `1 / (end - start)`.
///
/// # Safety
/// `p` is live; `screen` is one of its screens.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_rast_set_fog_range(p: *mut EeGeRasterizer, screen: c_ulong, start: f32, end: f32) {
    // SAFETY: the caller's contract
    let fog = unsafe { &mut (*EeGeRasterizer::screen(p, screen)).fog };
    fog.start = start;
    fog.end = end;
    fog.scale = 1.0 / (end - start);
}

/// `SetViewport(inViewport, inScreen)`: `GEViewport::operator=`, field by field.
///
/// # Safety
/// `p` is live; `screen` is one of its screens; `viewport` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_rast_set_viewport(p: *mut EeGeRasterizer, screen: c_ulong, viewport: *const GeViewport) {
    // SAFETY: the caller's contract
    unsafe { (*EeGeRasterizer::screen(p, screen)).viewport = *viewport };
}

/// `GetPixelSize(inModel, inViewport, inTransformation, inScale, inOrthographic)`.
///
/// # Safety
/// `p` is live; `model` is a C++ model; the pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_rast_pixel_size(
    p: *mut EeGeRasterizer,
    model: *mut c_void,
    viewport: *const GeViewport,
    transformation: *const GeTransformation,
    scale: f32,
    orthographic: bool,
) -> f32 {
    // SAFETY: the caller's contract (the viewport and transformation are copied first)
    unsafe {
        let (v, t) = (*viewport, *transformation);
        EeGeRasterizer::pixel_size(p, model as usize, &v, &t, scale, orthographic)
    }
}

/// `GetPixelSize(inModel, inViewport, inOrthographic)` of a placed model.
///
/// # Safety
/// `p` is live; `placed` is a C++ `GEPhysicalModel` with a model; `viewport` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_rast_placed_pixel_size(
    p: *mut EeGeRasterizer,
    placed: *const c_void,
    viewport: *const GeViewport,
    orthographic: bool,
) -> f32 {
    // SAFETY: the caller's contract
    unsafe {
        let v = *viewport;
        EeGeRasterizer::placed_pixel_size(p, placed as usize, &v, orthographic)
    }
}

/// `IsPerspectiveModelVisible` (`orthographic` false, `scale` used) or `IsOrthographicModelVisible`.
///
/// # Safety
/// `p` is live; `model` is a C++ model; the pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_rast_model_visible(
    p: *mut EeGeRasterizer,
    model: *mut c_void,
    scale: f32,
    viewport: *const GeViewport,
    transformation: *const GeTransformation,
    orthographic: bool,
    out_visible: *mut bool,
    out_clipped: *mut bool,
) {
    // SAFETY: the caller's contract (the inputs are copied first, the outputs written last)
    unsafe {
        let (v, t) = (*viewport, *transformation);
        let (visible, clipped) = if orthographic {
            EeGeRasterizer::orthographic_visible(p, model as usize, &v, &t)
        } else {
            EeGeRasterizer::perspective_visible(p, model as usize, scale, &v, &t)
        };
        *out_visible = visible;
        *out_clipped = clipped;
    }
}

/// `GERasterizer::GetActiveRasterizer()`.
#[no_mangle]
pub extern "C" fn ee_ge_rast_active() -> *mut c_void {
    ACTIVE.load(Ordering::SeqCst) as *mut c_void
}

/// `GERasterizer::SetActiveRasterizer(inRasterizer)` (the plug-in's activation).
#[no_mangle]
pub extern "C" fn ee_ge_rast_set_active(rasterizer: *mut c_void) {
    ACTIVE.store(rasterizer as usize, Ordering::SeqCst);
}

/// The rasterizer list (`sRasterizers`): `op` 0 clears it, 1 appends `rasterizer`, 2 answers its length, 3 its
/// entry `index` (NULL past the end).
#[no_mangle]
pub extern "C" fn ee_ge_rast_list(op: u32, rasterizer: *mut c_void, index: usize) -> *mut c_void {
    let mut list = RASTERIZERS.lock().unwrap_or_else(|e| e.into_inner());
    match op {
        0 => list.clear(),
        1 => list.push(rasterizer as usize),
        2 => return list.len() as *mut c_void,
        _ => return list.get(index).copied().unwrap_or(0) as *mut c_void,
    }
    std::ptr::null_mut()
}

#[cfg(test)]
#[path = "rastobj_tests.rs"]
mod tests;
