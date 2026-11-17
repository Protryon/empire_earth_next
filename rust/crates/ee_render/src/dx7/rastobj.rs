//! `DX7Rasterizer` whole (`Common/DX7Rasterizer*.cpp`; `DX7TnLRasterizer` is its T&L flavour): the per-screen DX7
//! records, the debug colour, the light slots and the water clock, and every method of the class, over the GERasterizer
//! base's Rust object (`ee_ge::rastobj`: its screen records, colours, lock and statics are read and written here
//! directly). The C++ class keeps its vtable (the game calls it through `GERasterizer`) and one member, the object; every
//! call on another C++ object (screens, surfaces, textures, windows, lights, the lock) is one host operation
//! ([`EeDx7RastOp`]).
//!
//! No reference to the object lives across a host call: each method works on the raw pointer and re-reads what a call
//! may have changed (a host call can re-enter the rasterizer: a texture's `Promote`, a screen's `CreateSurface`).
#![allow(clippy::missing_safety_doc, clippy::unnecessary_cast)]

use core::ffi::{c_char, c_ulong, c_void};
use std::sync::Mutex;

use crate::ge::rastobj::{
    ee_ge_rast_add_screen, ee_ge_rast_color, ee_ge_rast_deactivate, ee_ge_rast_get, ee_ge_rast_screen,
    ee_ge_rast_screen_set, ee_ge_rast_set, ee_ge_rast_set_active, ee_ge_rast_set_ambient_color,
    ee_ge_rast_set_ambient_intensity, ee_ge_rast_set_blending, ee_ge_rast_set_fog_color, ee_ge_rast_set_fog_range,
    ee_ge_rast_set_viewport, EeGeRastColor, EeGeRastScreen, EeGeRastState, EeGeRastVal, EeGeRasterizer,
};
use crate::ge::viewport::GeViewport;

use crate::dx7::api::*;
use crate::dx7::gx::{direct_draw_enumerate, RawDevice, RawGx, RawHost};
use crate::dx7::raster::{
    self, view_and_projection, EnumerationHost, Flavor, FrameStats, LightParams, RasterHost, LIGHT_SLOTS,
};
use crate::dx7::screen::VideoMode;
use crate::dx7::state::{self, ScreenState, StageView};

/// The calls the rasterizer makes on other C++ objects. `obj` is the object called; `a`, `b` as each says.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeDx7RastOp {
    /// `new DX7Screen(NULL, (GXDirectDraw*)obj, (const char*)a, b & 1, b & 2)`, as its `GEScreen*`.
    ScreenNew = 1,
    /// `delete (DX7Screen*)obj`.
    ScreenDelete,
    /// `((DX7Screen*)obj)->mDirectDraw`.
    ScreenDirectDraw,
    /// `obj->GetIsFullScreen()`.
    ScreenIsFullScreen,
    /// `*(GEVideoMode*)a = obj->GetCurrentMode()`.
    ScreenCurrentMode,
    /// `obj->GetModeCount()`.
    ScreenModeCount,
    /// `*(GEVideoMode*)b = obj->GetMode(a)`.
    ScreenMode,
    /// `obj->CreateSurface(*(GESurfaceDescription*)a, (GESurface**)b)`.
    ScreenCreateSurface,
    /// `obj->SetTextureManager((GETextureManager*)a)`.
    ScreenSetTextureManager,
    /// `((GETextureManager*)obj)->SetScreen((GEScreen*)a)`.
    ManagerSetScreen,
    /// `obj->GetAttachedSurface(0)`.
    SurfaceAttached,
    /// `obj->AttachSurface((GESurface*)a)`.
    SurfaceAttach,
    /// `obj->Reference()` (a surface).
    SurfaceReference,
    /// `obj->Dereference()` (a surface).
    SurfaceDereference,
    /// `(GXSurface*)*static_cast<DX7Surface*>(obj)`.
    SurfaceGx,
    /// `(GXSurface*)*dynamic_cast<DX7Surface*>(obj)`.
    SurfaceGxChecked,
    /// `((uint32_t*)a)[0] = obj->GetWidth(); ((uint32_t*)a)[1] = obj->GetHeight()`.
    SurfaceSize,
    /// `obj->Flip(a != 0)`.
    SurfaceFlip,
    /// `obj->HasChanged()` (a texture).
    TextureHasChanged,
    /// `obj->Reference()` (a texture).
    TextureReference,
    /// `obj->Dereference()` (a texture).
    TextureDereference,
    /// `obj->Promote()`.
    TexturePromote,
    /// `*(StageView*)a`: `obj->GetStage()`'s fields and `obj->GetStageTexture(0)->GetFrameSurface()`'s `GXSurface*`.
    TextureStage,
    /// `obj->ValidateChanges()`.
    TextureValidate,
    /// `((float*)a)[0..2] = obj->GetClientOrigin()` (a window).
    WindowOrigin,
    /// `*(LightParams*)a`: the `GELight` fields `UpdateLight` reads.
    LightParams,
    /// `obj->Enter()` (the drawing lock).
    LockEnter,
    /// `obj->Leave()`.
    LockLeave,
    /// `TMGetTime()`.
    Time,
    /// `ESVERIFY_AT(a != 0, b)`.
    Verify,
}

/// The host: `owner` is the C++ `DX7Rasterizer` of the object calling.
pub type EeDx7RastHostFn =
    unsafe extern "C" fn(owner: *mut c_void, op: EeDx7RastOp, obj: *mut c_void, a: isize, b: isize) -> isize;

/// The rasterizer.
pub struct EeDx7Rasterizer {
    host: EeDx7RastHostFn,
    /// The C++ rasterizer (`this`).
    owner: usize,
    /// The `GERasterizer` base's object.
    base: *mut EeGeRasterizer,
    flavor: Flavor,
    /// `mDX7ScreenStates`: each record boxed (the draws hold its address for one call).
    screens: Vec<*mut ScreenState>,
    /// `mDebugColor`.
    debug_color: u32,
    /// The activation in progress (`ActivateScreen`/`ActivateRenderingSurface`): screen, manager, window.
    activation: (c_ulong, usize, usize),
}

/// `gLightsInUse`: the D3D light slots in use, shared by every screen and rasterizer.
static LIGHTS_IN_USE: Mutex<[c_ulong; LIGHT_SLOTS]> = Mutex::new([0; LIGHT_SLOTS]);
/// `sWaterTime`: the water animation clock `TransformToCamera` advances.
static WATER_TIME: Mutex<f32> = Mutex::new(0.0);

/// `kAlphaTestingRefValue`'s "force" seed: 2^32 (`4294967296.0f`).
const FORCE_REF_VALUE: f32 = 4_294_967_296.0;
/// `-16` of the `unsigned long` caches: the next set is sent.
const FORCE: c_ulong = (-16_i64) as c_ulong;

/// # Safety
/// `p` is live; the reference is not held across a host call.
unsafe fn rs<'a>(p: *mut EeDx7Rasterizer) -> &'a mut EeDx7Rasterizer {
    // SAFETY: the caller's contract
    unsafe { &mut *p }
}

/// `GERScreenState` of the base: the record of screen `screen` (NULL past the records).
///
/// # Safety
/// `p` is live.
unsafe fn ge<'a>(p: *mut EeDx7Rasterizer, screen: c_ulong) -> &'a mut EeGeRastScreen {
    // SAFETY: the caller's contract; a screen past the records is a NULL dereference, as the original's vector index
    unsafe { &mut *ee_ge_rast_screen(rs(p).base, screen) }
}

/// The DX7 record of screen `screen`.
///
/// # Safety
/// `p` is live; `screen` is one of its screens.
unsafe fn dx<'a>(p: *mut EeDx7Rasterizer, screen: c_ulong) -> &'a mut ScreenState {
    // SAFETY: the caller's contract
    unsafe { &mut *rs(p).screens[screen as usize] }
}

impl EeDx7Rasterizer {
    /// A host call made by rasterizer `p`.
    ///
    /// # Safety
    /// `p` is live; the operation's arguments are as [`EeDx7RastOp`] describes.
    pub(crate) unsafe fn op(p: *mut Self, op: EeDx7RastOp, obj: H, a: isize, b: isize) -> isize {
        // SAFETY: the caller's contract (host and owner are copied out before the call)
        let (host, owner) = unsafe { (rs(p).host, rs(p).owner) };
        // SAFETY: the host's contract
        unsafe { host(owner as *mut c_void, op, obj, a, b) }
    }

    /// The base's object.
    ///
    /// # Safety
    /// `p` is live.
    pub(crate) unsafe fn base(p: *mut Self) -> *mut EeGeRasterizer {
        // SAFETY: the caller's contract
        unsafe { rs(p).base }
    }

    /// The DX7 record of screen `screen`, for one draw call.
    ///
    /// # Safety
    /// `p` is live; `screen` is one of its screens.
    pub(crate) unsafe fn record(p: *mut Self, screen: c_ulong) -> *mut ScreenState {
        // SAFETY: the caller's contract
        unsafe { rs(p).screens[screen as usize] }
    }

    /// The base's record of screen `screen`.
    ///
    /// # Safety
    /// `p` is live.
    pub(crate) unsafe fn ge_record(p: *mut Self, screen: c_ulong) -> *mut EeGeRastScreen {
        // SAFETY: the caller's contract
        unsafe { ee_ge_rast_screen(rs(p).base, screen) }
    }

    /// `DX7Rasterizer(inName)` (after the base's constructor): the dark blue debug colour; `tnl` makes it the
    /// `DX7TnLRasterizer`.
    pub fn create(host: EeDx7RastHostFn, owner: usize, base: *mut EeGeRasterizer, tnl: bool) -> *mut Self {
        Box::into_raw(Box::new(EeDx7Rasterizer {
            host,
            owner,
            base,
            flavor: if tnl { Flavor::Tnl } else { Flavor::Hal },
            screens: Vec::new(),
            debug_color: 0xff00_0040,
            activation: (0, 0, 0),
        }))
    }

    /// `~DX7Rasterizer()`: the records are freed (the original's vector freed only its array of pointers).
    ///
    /// # Safety
    /// `p` is live; it is freed.
    pub unsafe fn destroy(p: *mut Self) {
        // SAFETY: the caller's contract
        let b = unsafe { Box::from_raw(p) };
        for &s in &b.screens {
            // SAFETY: each record is a Box this object made
            drop(unsafe { Box::from_raw(s) });
        }
    }

    // ---- activation -------------------------------------------------------------------------------------------------

    /// `ActivateRasterizer`: the DirectDraw devices enumerated into screens, then the active rasterizer.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn activate_rasterizer(p: *mut Self) -> i32 {
        // SAFETY: p is live; the callback's context is p
        let status = unsafe { direct_draw_enumerate(device_enumerated, p as *mut c_void) };
        raster::finish_activation(&mut RawHost, &mut Enumeration(p), status)
    }

    /// `AddValidScreen`: a fresh DX7 record, then the base's.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn add_valid_screen(p: *mut Self, screen: H) {
        // SAFETY: zero is a valid ScreenState (the original's new left the rest unset; ActivateTheScreen seeds it)
        let mut state: ScreenState = unsafe { core::mem::zeroed() };
        state.mScreen = screen;
        state.mViewportChanged = true;
        // SAFETY: p is live
        unsafe {
            rs(p).screens.push(Box::into_raw(Box::new(state)));
            ee_ge_rast_add_screen(rs(p).base, screen);
        }
    }

    /// `ActivateScreen`/`ActivateRenderingSurface`'s context.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn begin_activation(p: *mut Self, screen: c_ulong, manager: H, window: H) {
        // SAFETY: the caller's contract
        unsafe { rs(p).activation = (screen, manager as usize, window as usize) };
    }

    /// `ActivateScreen`.
    ///
    /// # Safety
    /// `p` is live; `begin_activation` named the screen.
    pub unsafe fn activate_screen(p: *mut Self, width: u32, height: u32, back: u32, z: bool, z_format: u32) -> i32 {
        raster::activate_screen(&mut Activation(p), width, height, back, z, z_format)
    }

    /// `ActivateRenderingSurface`.
    ///
    /// # Safety
    /// As `activate_screen`.
    pub unsafe fn activate_rendering_surface(p: *mut Self, width: u32, height: u32) -> i32 {
        raster::activate_rendering_surface(&mut Activation(p), width, height)
    }

    /// `ActivateTheScreen`: the device on the secondary surface (a `DX7Surface`).
    ///
    /// # Safety
    /// `p` is live; `screen` is one of its screens.
    pub unsafe fn activate_the_screen(p: *mut Self, screen: c_ulong, secondary: H) -> i32 {
        // SAFETY: p is live; the host's contract
        unsafe {
            let target = Self::op(p, EeDx7RastOp::SurfaceGxChecked, secondary, 0, 0) as H;
            let dd = Self::op(p, EeDx7RastOp::ScreenDirectDraw, dx(p, screen).mScreen, 0, 0) as H;
            let (width, height) = Self::surface_size(p, secondary);
            let mut local = *dx(p, screen);
            let flavor = rs(p).flavor;
            let result =
                raster::activate_the_screen(&mut RawGx, &mut RawHost, &mut local, dd, target, width, height, flavor);
            *dx(p, screen) = local;
            result
        }
    }

    /// A surface's `GetWidth()`, `GetHeight()`.
    ///
    /// # Safety
    /// `p` is live; `surface` is a C++ surface.
    unsafe fn surface_size(p: *mut Self, surface: H) -> (u32, u32) {
        let mut size = [0u32; 2];
        // SAFETY: the host's contract
        unsafe { Self::op(p, EeDx7RastOp::SurfaceSize, surface, size.as_mut_ptr() as isize, 0) };
        (size[0], size[1])
    }

    /// The activation's commit: the texture manager and the screen joined, the base record's surfaces and window, the
    /// render target (referenced).
    ///
    /// # Safety
    /// `p` is live, in an activation.
    unsafe fn commit_activation(p: *mut Self, primary: H, secondary: H) {
        // SAFETY: p is live; the host's contract; the base record is re-read after each call
        unsafe {
            let (screen, manager, window) = rs(p).activation;
            let gscreen = ge(p, screen).screen;
            Self::op(p, EeDx7RastOp::ScreenSetTextureManager, gscreen, manager as isize, 0);
            Self::op(p, EeDx7RastOp::ManagerSetScreen, manager as H, gscreen as isize, 0);
            let g = ge(p, screen);
            g.primary_surface = primary;
            g.texture = NULL;
            g.render_target = NULL;
            g.secondary_surface = secondary;
            g.window = window as H;
            g.is_screen_activated = true;
            Self::set_render_target(p, secondary, screen);
            Self::op(p, EeDx7RastOp::SurfaceReference, secondary, 0, 0);
        }
    }

    /// `ActivateScreen`'s defaults: every cached state forced, then sent, and the 3D transforms.
    ///
    /// # Safety
    /// `p` is live, in an activation.
    unsafe fn apply_defaults(p: *mut Self) {
        // SAFETY: p is live; each store is to the record re-read after the calls before it
        unsafe {
            let s = rs(p).activation.0;
            ge(p, s).alpha_testing = FORCE;
            Self::set_alpha_testing(p, 0, s);
            ge(p, s).alpha_testing_compare_state = FORCE;
            Self::set_alpha_testing_compare_state(p, 0, s);
            ge(p, s).alpha_testing_ref_value = FORCE_REF_VALUE;
            Self::set_alpha_testing_ref_value(p, 0.0, s);
            ge(p, s).anti_aliasing = FORCE;
            Self::set_anti_aliasing(p, 0, s);
            ge(p, s).blend_source = FORCE;
            ge(p, s).blend_destination = FORCE;
            Self::set_blending(p, 1, 0, s);
            ge(p, s).color_keying = FORCE;
            Self::set_color_keying(p, 0, s);
            ee_ge_rast_set(rs(p).base, EeGeRastVal::Culling, FORCE as isize);
            Self::set_culling(p, 0);
            ge(p, s).dithering = FORCE;
            Self::set_dithering(p, 0, s);
            ge(p, s).draw_style = FORCE;
            Self::set_draw_style(p, 0, s);
            ge(p, s).fog_state = FORCE;
            Self::set_fog(p, 0, s);
            ge(p, s).lighting = FORCE;
            Self::set_lighting(p, 0, s);
            ge(p, s).projection_mode = FORCE;
            ee_ge_rast_screen_set(rs(p).base, s, EeGeRastState::ProjectionMode, 1);
            ge(p, s).shading = FORCE;
            Self::set_shading(p, 1, s);
            ge(p, s).shading_color_source = FORCE;
            Self::set_shading_color_source(p, 1, s);
            ge(p, s).specular = FORCE;
            Self::set_specular(p, 0, s);
            ge(p, s).z_buffer = FORCE;
            Self::set_z_buffer(p, 0, s);
            ge(p, s).z_compare = FORCE;
            Self::set_z_compare(p, 0, s);
            dx(p, s).mIs3DMode = false;
            Self::set_3d_mode(p, s);
        }
    }

    /// `Deactivate`: each screen's texture, vertex buffer, lights, device and Direct3D released and its screen deleted,
    /// then the base's.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn deactivate(p: *mut Self) {
        // SAFETY: p is live; the records stay until the loop ends (SetTexture reads them by index)
        unsafe {
            let count = rs(p).screens.len();
            for i in 0..count {
                let screen = i as c_ulong;
                Self::set_texture(p, NULL, screen);
                let state = dx(p, screen);
                if !state.mVertexBuffer.is_null() {
                    RawGx.vb_release(state.mVertexBuffer);
                    dx(p, screen).mVertexBuffer = NULL;
                }
                if !dx(p, screen).mDevice.is_null() {
                    Self::remove_all_lights(p, screen);
                    RawGx.device_release(dx(p, screen).mDevice);
                    dx(p, screen).mDevice = NULL;
                }
                if !dx(p, screen).mDirect3D.is_null() {
                    RawGx.dd_release(dx(p, screen).mDirect3D);
                    dx(p, screen).mDirect3D = NULL;
                }
                let gscreen = dx(p, screen).mScreen;
                if !gscreen.is_null() {
                    Self::op(p, EeDx7RastOp::ScreenDelete, gscreen, 0, 0);
                }
            }
            for s in std::mem::take(&mut rs(p).screens) {
                drop(Box::from_raw(s));
            }
            ee_ge_rast_deactivate(rs(p).base);
        }
    }

    // ---- scene, clears, memory ---------------------------------------------------------------------------------------

    /// # Safety
    /// `p` is live; `screen` is one of its screens.
    unsafe fn device(p: *mut Self, screen: c_ulong) -> H {
        // SAFETY: the caller's contract
        unsafe { dx(p, screen).mDevice }
    }

    /// `BeginScene`: the render target, then the scene (9 when the surface is lost).
    ///
    /// # Safety
    /// `p` is live; `screen` is one of its screens.
    pub unsafe fn begin_scene(p: *mut Self, target: H, screen: c_ulong) -> i32 {
        // SAFETY: the caller's contract
        unsafe {
            Self::set_render_target(p, target, screen);
            raster::begin_scene(&mut RawGx, &mut RawHost, Self::device(p, screen))
        }
    }

    /// `EndScene`.
    ///
    /// # Safety
    /// As `begin_scene`.
    pub unsafe fn end_scene(p: *mut Self, screen: c_ulong) -> i32 {
        // SAFETY: the caller's contract
        raster::end_scene(&mut RawGx, &mut RawHost, unsafe { Self::device(p, screen) })
    }

    /// `CanDoCurrentState`: the texture, then whether the device validates the state in one pass.
    ///
    /// # Safety
    /// As `begin_scene`.
    pub unsafe fn can_do_current_state(p: *mut Self, texture: H, screen: c_ulong) -> bool {
        // SAFETY: the caller's contract
        unsafe {
            Self::set_texture(p, texture, screen);
            let (error, _) = RawGx.device_validate(Self::device(p, screen));
            if error == GX_OK {
                true
            } else {
                RawHost.record_native_error(error);
                false
            }
        }
    }

    /// `Clear`.
    ///
    /// # Safety
    /// As `begin_scene`.
    pub unsafe fn clear(p: *mut Self, rect: [f32; 4], rgb: [f32; 3], screen: c_ulong) -> i32 {
        // SAFETY: the caller's contract
        raster::clear(&mut RawGx, &mut RawHost, unsafe { Self::device(p, screen) }, rect, rgb)
    }

    /// `ClearZBuffer`.
    ///
    /// # Safety
    /// As `begin_scene`.
    pub unsafe fn clear_z(p: *mut Self, rect: [f32; 4], z: f32, screen: c_ulong) -> i32 {
        // SAFETY: the caller's contract
        raster::clear_z(&mut RawGx, &mut RawHost, unsafe { Self::device(p, screen) }, rect, z)
    }

    /// `GetVideoMemory`: (result, total, free).
    ///
    /// # Safety
    /// As `begin_scene`.
    pub unsafe fn video_memory(p: *mut Self, screen: c_ulong) -> (i32, u32, u32) {
        // SAFETY: the caller's contract
        unsafe {
            let dd = Self::op(p, EeDx7RastOp::ScreenDirectDraw, dx(p, screen).mScreen, 0, 0) as H;
            raster::video_memory(&mut RawGx, dd)
        }
    }

    /// `GetCanModulateTextureAlpha`, `GetDirtyRectangleSupport`.
    ///
    /// # Safety
    /// As `begin_scene`.
    pub unsafe fn card_flags(p: *mut Self, screen: c_ulong) -> (bool, bool) {
        // SAFETY: the caller's contract
        let state = unsafe { dx(p, screen) };
        (state.mCanModulateTextureAlpha, state.mDirtyRectangleSupport)
    }

    // ---- lights ---------------------------------------------------------------------------------------------------------

    /// # Safety
    /// `p` is live; `light` is a C++ `GELight`.
    unsafe fn light_params(p: *mut Self, light: H) -> LightParams {
        let mut params = LightParams::default();
        // SAFETY: the host's contract
        unsafe { Self::op(p, EeDx7RastOp::LightParams, light, &mut params as *mut LightParams as isize, 0) };
        params
    }

    /// `AddLight`: (result, the slot).
    ///
    /// # Safety
    /// As `begin_scene`; `light` is a C++ `GELight`.
    pub unsafe fn add_light(p: *mut Self, light: H, screen: c_ulong) -> (i32, u32) {
        // SAFETY: the caller's contract
        let (params, device) = unsafe { (Self::light_params(p, light), Self::device(p, screen)) };
        let mut in_use = LIGHTS_IN_USE.lock().unwrap_or_else(|e| e.into_inner());
        raster::add_light(&mut RawGx, device, &mut in_use, &params)
    }

    /// `UpdateLight`.
    ///
    /// # Safety
    /// As `add_light`.
    pub unsafe fn update_light(p: *mut Self, index: u32, light: H, screen: c_ulong) -> i32 {
        // SAFETY: the caller's contract
        let (params, device) = unsafe { (Self::light_params(p, light), Self::device(p, screen)) };
        let in_use = LIGHTS_IN_USE.lock().unwrap_or_else(|e| e.into_inner());
        raster::update_light(&mut RawGx, device, &in_use, index, &params)
    }

    /// `RemoveLight`.
    ///
    /// # Safety
    /// As `begin_scene`.
    pub unsafe fn remove_light(p: *mut Self, index: u32, screen: c_ulong) -> i32 {
        // SAFETY: the caller's contract
        let device = unsafe { Self::device(p, screen) };
        let mut in_use = LIGHTS_IN_USE.lock().unwrap_or_else(|e| e.into_inner());
        raster::remove_light(&mut RawGx, device, &mut in_use, index)
    }

    /// `RemoveAllLights`.
    ///
    /// # Safety
    /// As `begin_scene`.
    pub unsafe fn remove_all_lights(p: *mut Self, screen: c_ulong) -> i32 {
        // SAFETY: the caller's contract
        let device = unsafe { Self::device(p, screen) };
        let mut in_use = LIGHTS_IN_USE.lock().unwrap_or_else(|e| e.into_inner());
        raster::remove_all_lights(&mut RawGx, device, &mut in_use)
    }

    /// Whether slot `index` is in use (`gLightsInUse[index] != 0`).
    pub(crate) fn light_in_use(index: usize) -> bool {
        LIGHTS_IN_USE.lock().unwrap_or_else(|e| e.into_inner())[index] != 0
    }

    // ---- transforms, target, viewport ----------------------------------------------------------------------------------

    /// `Set2DMode`.
    ///
    /// # Safety
    /// As `begin_scene`.
    pub(crate) unsafe fn set_2d_mode(p: *mut Self, screen: c_ulong) -> i32 {
        // SAFETY: the caller's contract
        raster::set_2d_mode(&mut RawGx, unsafe { dx(p, screen) })
    }

    /// `Set3DMode`: the camera view (y and z swapped after the camera's inverse) and the perspective or orthographic
    /// projection of the screen's viewport.
    ///
    /// # Safety
    /// As `begin_scene`.
    pub(crate) unsafe fn set_3d_mode(p: *mut Self, screen: c_ulong) -> i32 {
        // SAFETY: the caller's contract
        unsafe {
            let g = ge(p, screen);
            let (view, projection) = view_and_projection(&g.viewport, g.projection_mode);
            raster::set_3d_mode(&mut RawGx, dx(p, screen), view.m, projection.m)
        }
    }

    /// `SetRenderTarget`: the target referenced (the previous one dereferenced), the device target and viewport size.
    ///
    /// # Safety
    /// As `begin_scene`; `target` is a C++ `DX7Surface`.
    pub unsafe fn set_render_target(p: *mut Self, target: H, screen: c_ulong) {
        // SAFETY: the caller's contract; the base record is re-read after each host call
        unsafe {
            let current = ge(p, screen).render_target;
            if target == current {
                return;
            }
            if !current.is_null() {
                Self::op(p, EeDx7RastOp::SurfaceDereference, current, 0, 0);
            }
            ge(p, screen).render_target = target;
            Self::op(p, EeDx7RastOp::SurfaceReference, target, 0, 0);
            let gx_target = Self::op(p, EeDx7RastOp::SurfaceGx, target, 0, 0) as H;
            let (width, height) = Self::surface_size(p, target);
            raster::set_render_target(&mut RawGx, dx(p, screen), gx_target, width, height);
        }
    }

    /// `SetViewport`: the base's, then the device viewport and depth range.
    ///
    /// # Safety
    /// As `begin_scene`; `viewport` is a `GEViewport`.
    pub unsafe fn set_viewport(p: *mut Self, viewport: *const GeViewport, screen: c_ulong) {
        // SAFETY: the caller's contract
        unsafe {
            ee_ge_rast_set_viewport(rs(p).base, screen, viewport);
            let v = &*viewport;
            let r = v.view_rectangle;
            raster::set_viewport(
                &mut RawGx,
                dx(p, screen),
                [r.left, r.top, r.right, r.bottom],
                v.near_depth,
                v.far_depth,
            );
        }
    }

    // ---- the cached render states ------------------------------------------------------------------------------------

    /// `SetAlphaTesting`.
    ///
    /// # Safety
    /// As `begin_scene`.
    pub unsafe fn set_alpha_testing(p: *mut Self, value: c_ulong, screen: c_ulong) {
        // SAFETY: the caller's contract
        unsafe {
            if state::set_alpha_testing(&mut RawDevice(Self::device(p, screen)), value, ge(p, screen).alpha_testing) {
                ee_ge_rast_screen_set(rs(p).base, screen, EeGeRastState::AlphaTesting, value as isize);
            }
        }
    }

    /// `SetAlphaTestingCompareState`.
    ///
    /// # Safety
    /// As `begin_scene`.
    pub unsafe fn set_alpha_testing_compare_state(p: *mut Self, value: c_ulong, screen: c_ulong) {
        // SAFETY: the caller's contract
        unsafe {
            let current = ge(p, screen).alpha_testing_compare_state;
            if state::set_alpha_testing_compare_state(&mut RawDevice(Self::device(p, screen)), value, current) {
                ee_ge_rast_screen_set(rs(p).base, screen, EeGeRastState::AlphaTestingCompareState, value as isize);
            }
        }
    }

    /// `SetAlphaTestingRefValue`.
    ///
    /// # Safety
    /// As `begin_scene`.
    pub unsafe fn set_alpha_testing_ref_value(p: *mut Self, value: f32, screen: c_ulong) {
        // SAFETY: the caller's contract
        unsafe {
            let current = ge(p, screen).alpha_testing_ref_value;
            if state::set_alpha_testing_ref_value(&mut RawDevice(Self::device(p, screen)), value, current) {
                let bits = value.to_bits() as isize;
                ee_ge_rast_screen_set(rs(p).base, screen, EeGeRastState::AlphaTestingRefValue, bits);
            }
        }
    }

    /// `SetAmbientColor` (`color` is a `GEColor`) / `SetAmbientIntensity` (`color` NULL): the base's, then the
    /// computed ambient colour sent.
    ///
    /// # Safety
    /// As `begin_scene`.
    pub unsafe fn set_ambient(p: *mut Self, color: *const EeGeRastColor, intensity: f32, screen: c_ulong) {
        // SAFETY: the caller's contract
        unsafe {
            if color.is_null() {
                ee_ge_rast_set_ambient_intensity(rs(p).base, intensity);
            } else {
                ee_ge_rast_set_ambient_color(rs(p).base, color);
            }
            let c = *ee_ge_rast_color(rs(p).base, true);
            state::set_ambient(&mut RawDevice(Self::device(p, screen)), [c.red, c.green, c.blue]);
        }
    }

    /// `SetAntiAliasing`.
    ///
    /// # Safety
    /// As `begin_scene`.
    pub unsafe fn set_anti_aliasing(p: *mut Self, value: c_ulong, screen: c_ulong) {
        // SAFETY: the caller's contract
        unsafe {
            if state::set_anti_aliasing(&mut RawDevice(Self::device(p, screen)), value, ge(p, screen).anti_aliasing) {
                ee_ge_rast_screen_set(rs(p).base, screen, EeGeRastState::AntiAliasing, value as isize);
            }
        }
    }

    /// `SetBlending`.
    ///
    /// # Safety
    /// As `begin_scene`.
    pub unsafe fn set_blending(p: *mut Self, source: c_ulong, destination: c_ulong, screen: c_ulong) {
        // SAFETY: the caller's contract
        unsafe {
            let (cs, cd) = (ge(p, screen).blend_source, ge(p, screen).blend_destination);
            let state = dx(p, screen);
            let mut device = RawDevice(state.mDevice);
            if state::set_blending(state, &mut device, source, destination, cs, cd) {
                ee_ge_rast_set_blending(rs(p).base, screen, source, destination);
            }
        }
    }

    /// `SetColorKeying`.
    ///
    /// # Safety
    /// As `begin_scene`.
    pub unsafe fn set_color_keying(p: *mut Self, value: c_ulong, screen: c_ulong) {
        // SAFETY: the caller's contract
        unsafe {
            if state::set_color_keying(&mut RawDevice(Self::device(p, screen)), value, ge(p, screen).color_keying) {
                ee_ge_rast_screen_set(rs(p).base, screen, EeGeRastState::ColorKeying, value as isize);
            }
        }
    }

    /// `SetCulling`: every screen's device, when it changes.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn set_culling(p: *mut Self, value: c_ulong) {
        // SAFETY: the caller's contract
        unsafe {
            if value as isize == ee_ge_rast_get(rs(p).base, EeGeRastVal::Culling) {
                return;
            }
            for i in 0..rs(p).screens.len() {
                let device = Self::device(p, i as c_ulong);
                if !device.is_null() {
                    state::set_cull_mode(&mut RawDevice(device), value);
                }
            }
            ee_ge_rast_set(rs(p).base, EeGeRastVal::Culling, value as isize);
        }
    }

    /// `SetDebugColor`.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn set_debug_color(p: *mut Self, rgb: [f32; 3]) {
        // SAFETY: the caller's contract
        unsafe { rs(p).debug_color = state::debug_color(rgb) };
    }

    /// `SetDithering`.
    ///
    /// # Safety
    /// As `begin_scene`.
    pub unsafe fn set_dithering(p: *mut Self, value: c_ulong, screen: c_ulong) {
        // SAFETY: the caller's contract
        unsafe {
            if state::set_dithering(&mut RawDevice(Self::device(p, screen)), value, ge(p, screen).dithering) {
                ee_ge_rast_screen_set(rs(p).base, screen, EeGeRastState::Dithering, value as isize);
            }
        }
    }

    /// `SetDrawStyle`.
    ///
    /// # Safety
    /// As `begin_scene`.
    pub unsafe fn set_draw_style(p: *mut Self, value: c_ulong, screen: c_ulong) {
        // SAFETY: the caller's contract
        unsafe {
            if state::set_draw_style(&mut RawDevice(Self::device(p, screen)), value, ge(p, screen).draw_style) {
                ee_ge_rast_screen_set(rs(p).base, screen, EeGeRastState::DrawStyle, value as isize);
            }
        }
    }

    /// `SetFog`.
    ///
    /// # Safety
    /// As `begin_scene`.
    pub unsafe fn set_fog(p: *mut Self, value: c_ulong, screen: c_ulong) {
        // SAFETY: the caller's contract
        unsafe {
            if state::set_fog(&mut RawDevice(Self::device(p, screen)), value, ge(p, screen).fog_state) {
                ee_ge_rast_screen_set(rs(p).base, screen, EeGeRastState::FogState, value as isize);
            }
        }
    }

    /// `SetFogColor` (`color` is a `GEColor`).
    ///
    /// # Safety
    /// As `begin_scene`.
    pub unsafe fn set_fog_color(p: *mut Self, color: *const EeGeRastColor, screen: c_ulong) {
        // SAFETY: the caller's contract
        unsafe {
            let c = *color;
            let f = ge(p, screen).fog.color;
            let equal = f.red == c.red && f.green == c.green && f.blue == c.blue;
            if state::set_fog_color(&mut RawDevice(Self::device(p, screen)), [c.red, c.green, c.blue], equal) {
                ee_ge_rast_set_fog_color(rs(p).base, screen, color);
            }
        }
    }

    /// `SetFogRange`.
    ///
    /// # Safety
    /// As `begin_scene`.
    pub unsafe fn set_fog_range(p: *mut Self, start: f32, end: f32, screen: c_ulong) {
        // SAFETY: the caller's contract
        unsafe {
            let f = ge(p, screen).fog;
            state::set_fog_range(&mut RawDevice(Self::device(p, screen)), start, end, f.start, f.end);
            ee_ge_rast_set_fog_range(rs(p).base, screen, start, end);
        }
    }

    /// `SetLighting`.
    ///
    /// # Safety
    /// As `begin_scene`.
    pub unsafe fn set_lighting(p: *mut Self, value: c_ulong, screen: c_ulong) {
        // SAFETY: the caller's contract
        unsafe {
            if state::set_lighting(&mut RawDevice(Self::device(p, screen)), value, ge(p, screen).lighting) {
                ee_ge_rast_screen_set(rs(p).base, screen, EeGeRastState::Lighting, value as isize);
            }
        }
    }

    /// `SetShading`.
    ///
    /// # Safety
    /// As `begin_scene`.
    pub unsafe fn set_shading(p: *mut Self, value: c_ulong, screen: c_ulong) {
        // SAFETY: the caller's contract
        unsafe {
            if state::set_shading(&mut RawDevice(Self::device(p, screen)), value, ge(p, screen).shading) {
                ee_ge_rast_screen_set(rs(p).base, screen, EeGeRastState::Shading, value as isize);
            }
        }
    }

    /// `SetShadingColorSource`.
    ///
    /// # Safety
    /// As `begin_scene`.
    pub unsafe fn set_shading_color_source(p: *mut Self, value: c_ulong, screen: c_ulong) {
        // SAFETY: the caller's contract
        unsafe {
            let current = ge(p, screen).shading_color_source;
            if state::set_shading_color_source(&mut RawDevice(Self::device(p, screen)), value, current) {
                ee_ge_rast_screen_set(rs(p).base, screen, EeGeRastState::ShadingColorSource, value as isize);
            }
        }
    }

    /// `SetSpecular`.
    ///
    /// # Safety
    /// As `begin_scene`.
    pub unsafe fn set_specular(p: *mut Self, value: c_ulong, screen: c_ulong) {
        // SAFETY: the caller's contract
        unsafe {
            if state::set_specular(&mut RawDevice(Self::device(p, screen)), value, ge(p, screen).specular) {
                ee_ge_rast_screen_set(rs(p).base, screen, EeGeRastState::Specular, value as isize);
            }
        }
    }

    /// `SetZBuffer`.
    ///
    /// # Safety
    /// As `begin_scene`.
    pub unsafe fn set_z_buffer(p: *mut Self, value: c_ulong, screen: c_ulong) {
        // SAFETY: the caller's contract
        unsafe {
            let current = ge(p, screen).z_buffer;
            let state = dx(p, screen);
            let mut device = RawDevice(state.mDevice);
            if state::set_z_buffer(state, &mut device, value, current) {
                ee_ge_rast_screen_set(rs(p).base, screen, EeGeRastState::ZBuffer, value as isize);
            }
        }
    }

    /// `SetZCompare`.
    ///
    /// # Safety
    /// As `begin_scene`.
    pub unsafe fn set_z_compare(p: *mut Self, value: c_ulong, screen: c_ulong) {
        // SAFETY: the caller's contract
        unsafe {
            if state::set_z_compare(&mut RawDevice(Self::device(p, screen)), value, ge(p, screen).z_compare) {
                ee_ge_rast_screen_set(rs(p).base, screen, EeGeRastState::ZCompare, value as isize);
            }
        }
    }

    /// `SetTexture`: the texture referenced (the previous one dereferenced) and promoted, the texture-change count,
    /// then stage 0 from its stage settings (NULL: texturing off).
    ///
    /// # Safety
    /// As `begin_scene`; `texture` is NULL or a C++ `GETexture`.
    pub unsafe fn set_texture(p: *mut Self, texture: H, screen: c_ulong) {
        // SAFETY: the caller's contract; the base record is re-read after each host call
        unsafe {
            let current = ge(p, screen).texture;
            if texture == current
                && (texture.is_null() || Self::op(p, EeDx7RastOp::TextureHasChanged, texture, 0, 0) == 0)
            {
                return;
            }
            let has = !texture.is_null();
            if has != ge(p, screen).has_texture {
                ge(p, screen).has_texture = has;
            }
            let current = ge(p, screen).texture;
            if !current.is_null() {
                Self::op(p, EeDx7RastOp::TextureDereference, current, 0, 0);
            }
            ge(p, screen).texture = texture;
            if has {
                Self::op(p, EeDx7RastOp::TextureReference, texture, 0, 0);
                Self::op(p, EeDx7RastOp::TexturePromote, texture, 0, 0);
            }
            let g = ge(p, screen);
            g.counters[3] = g.counters[3].wrapping_add(1);
            if !has {
                let state = dx(p, screen);
                let mut device = RawDevice(state.mDevice);
                state::clear_texture(state, &mut device);
                return;
            }
            let mut view: StageView = core::mem::zeroed();
            Self::op(p, EeDx7RastOp::TextureStage, texture, &mut view as *mut StageView as isize, 0);
            let state = dx(p, screen);
            let mut device = RawDevice(state.mDevice);
            state::apply_texture_stage(state, &mut device, &view);
            Self::op(p, EeDx7RastOp::TextureValidate, texture, 0, 0);
        }
    }

    // ---- presentation ---------------------------------------------------------------------------------------------------

    /// `ShowScene`: the frame statistics (the clock read every 12 frames), then the flip (full screen) or the blit to
    /// the window's client area.
    ///
    /// # Safety
    /// As `begin_scene`.
    pub unsafe fn show_scene(p: *mut Self, screen: c_ulong) -> i32 {
        // SAFETY: the caller's contract; the base record is re-read after each host call
        unsafe {
            let g = ge(p, screen);
            let mut stats = FrameStats {
                frame_counter: g.counters[0] as u32,
                scene_polys: g.counters[1] as u32,
                last_time: g.counters[2] as u32,
                texture_changes: g.counters[3] as u32,
                drawn_poly_count: g.drawn_poly_count as u32,
                drawn_poly_area: g.drawn_poly_area,
                area_per_frame: g.drawn_poly_area_per_frame,
                area_per_second: g.drawn_poly_area_per_second,
                polys_per_frame: g.drawn_polys_per_frame,
                polys_per_second: g.drawn_polys_per_second,
                frames_per_second: g.frames_per_second,
                scene_polys_per_frame: g.scene_polys_per_frame,
                scene_polys_per_second: g.scene_polys_per_second,
                texture_changes_per_frame: g.total_texture_changes_per_frame,
            };
            let time = if stats.frame_counter.wrapping_add(1) == 12 {
                Self::op(p, EeDx7RastOp::Time, NULL, 0, 0) as u32
            } else {
                0
            };
            raster::update_frame_stats(&mut stats, time);
            let g = ge(p, screen);
            g.counters = [
                stats.frame_counter as c_ulong,
                stats.scene_polys as c_ulong,
                stats.last_time as c_ulong,
                stats.texture_changes as c_ulong,
            ];
            g.drawn_poly_count = stats.drawn_poly_count as c_ulong;
            g.drawn_poly_area = stats.drawn_poly_area;
            g.drawn_poly_area_per_frame = stats.area_per_frame;
            g.drawn_poly_area_per_second = stats.area_per_second;
            g.drawn_polys_per_frame = stats.polys_per_frame;
            g.drawn_polys_per_second = stats.polys_per_second;
            g.frames_per_second = stats.frames_per_second;
            g.scene_polys_per_frame = stats.scene_polys_per_frame;
            g.scene_polys_per_second = stats.scene_polys_per_second;
            g.total_texture_changes_per_frame = stats.texture_changes_per_frame;

            let (primary_surface, gscreen) = (g.primary_surface, g.screen);
            let primary = Self::op(p, EeDx7RastOp::SurfaceGx, primary_surface, 0, 0) as H;
            let full_screen = Self::op(p, EeDx7RastOp::ScreenIsFullScreen, gscreen, 0, 0) != 0;
            let (mut secondary, mut width, mut height, mut origin) = (NULL, 0, 0, [0.0f32; 2]);
            if !full_screen {
                let (secondary_surface, window) = (ge(p, screen).secondary_surface, ge(p, screen).window);
                secondary = Self::op(p, EeDx7RastOp::SurfaceGx, secondary_surface, 0, 0) as H;
                (width, height) = Self::surface_size(p, secondary_surface);
                Self::op(p, EeDx7RastOp::WindowOrigin, window, origin.as_mut_ptr() as isize, 0);
            }
            let wait = ge(p, screen).wait_for_vsync;
            raster::show_scene(&mut RawGx, &mut RawHost, full_screen, primary, secondary, width, height, origin, || {
                Self::op(p, EeDx7RastOp::SurfaceFlip, primary_surface, wait as isize, 0) as i32
            })
        }
    }

    // ---- the water clock --------------------------------------------------------------------------------------------

    /// `sWaterTime`, for one `TransformToCamera`.
    pub(crate) fn with_water_time<R>(f: impl FnOnce(&mut f32) -> R) -> R {
        f(&mut WATER_TIME.lock().unwrap_or_else(|e| e.into_inner()))
    }
}

/// `ActivateScreen`'s host: the base record's screen and the activation's surfaces.
struct Activation(*mut EeDx7Rasterizer);

impl Activation {
    fn screen(&self) -> H {
        // SAFETY: the object is live during its activation
        unsafe { ge(self.0, rs(self.0).activation.0).screen }
    }
    fn call(&self, op: EeDx7RastOp, obj: H, a: isize, b: isize) -> isize {
        // SAFETY: the object is live during its activation; the host's contract
        unsafe { EeDx7Rasterizer::op(self.0, op, obj, a, b) }
    }
}

impl RasterHost for Activation {
    fn is_full_screen(&mut self) -> bool {
        self.call(EeDx7RastOp::ScreenIsFullScreen, self.screen(), 0, 0) != 0
    }
    fn current_mode(&mut self) -> VideoMode {
        let mut m = VideoMode::default();
        self.call(EeDx7RastOp::ScreenCurrentMode, self.screen(), &mut m as *mut VideoMode as isize, 0);
        m
    }
    fn mode_count(&mut self) -> u32 {
        self.call(EeDx7RastOp::ScreenModeCount, self.screen(), 0, 0) as u32
    }
    fn mode(&mut self, index: u32) -> VideoMode {
        let mut m = VideoMode::default();
        self.call(EeDx7RastOp::ScreenMode, self.screen(), index as isize, &mut m as *mut VideoMode as isize);
        m
    }
    fn create_surface(&mut self, description: &mut SurfaceDescription) -> (i32, H) {
        let mut out = NULL;
        let d = description as *mut SurfaceDescription as isize;
        let r = self.call(EeDx7RastOp::ScreenCreateSurface, self.screen(), d, &mut out as *mut H as isize);
        (r as i32, out)
    }
    fn attached_surface(&mut self, surface: H) -> H {
        self.call(EeDx7RastOp::SurfaceAttached, surface, 0, 0) as H
    }
    fn attach_surface(&mut self, surface: H, attached: H) -> i32 {
        self.call(EeDx7RastOp::SurfaceAttach, surface, attached as isize, 0) as i32
    }
    fn dereference(&mut self, surface: H) {
        self.call(EeDx7RastOp::SurfaceDereference, surface, 0, 0);
    }
    fn activate_the_screen(&mut self, _primary: H, secondary: H) -> i32 {
        // SAFETY: the object is live during its activation
        unsafe { EeDx7Rasterizer::activate_the_screen(self.0, rs(self.0).activation.0, secondary) }
    }
    fn commit_activation(&mut self, primary: H, secondary: H) {
        // SAFETY: as above
        unsafe { EeDx7Rasterizer::commit_activation(self.0, primary, secondary) }
    }
    fn apply_defaults(&mut self) {
        // SAFETY: as above
        unsafe { EeDx7Rasterizer::apply_defaults(self.0) }
    }
    fn verify(&mut self, ok: bool, line: u32) {
        self.call(EeDx7RastOp::Verify, NULL, ok as isize, line as isize);
    }
}

/// `ActivateRasterizer`'s enumeration host.
struct Enumeration(*mut EeDx7Rasterizer);

impl EnumerationHost for Enumeration {
    fn new_screen(&mut self, dd: H, description: &[u8], attached: bool, primary: bool) -> H {
        let mut name = description.to_vec();
        name.push(0);
        let flags = attached as isize | (primary as isize) << 1;
        let name = name.as_ptr() as *const c_char as isize;
        // SAFETY: the object is live; the host's contract
        let screen = unsafe { EeDx7Rasterizer::op(self.0, EeDx7RastOp::ScreenNew, dd, name, flags) };
        screen as H
    }
    fn screen_is_valid(&mut self, screen: H) -> bool {
        // SAFETY: as above
        unsafe {
            let dd = EeDx7Rasterizer::op(self.0, EeDx7RastOp::ScreenDirectDraw, screen, 0, 0) as H;
            raster::is_screen_valid(&mut RawGx, &mut RawHost, dd, rs(self.0).flavor)
        }
    }
    fn add_valid_screen(&mut self, screen: H) {
        // SAFETY: as above
        unsafe { EeDx7Rasterizer::add_valid_screen(self.0, screen) }
    }
    fn delete_screen(&mut self, screen: H) {
        // SAFETY: as above
        unsafe { EeDx7Rasterizer::op(self.0, EeDx7RastOp::ScreenDelete, screen, 0, 0) };
    }
    fn set_active(&mut self) {
        // SAFETY: as above
        unsafe { ee_ge_rast_set_active(rs(self.0).owner as *mut c_void) }
    }
}

unsafe extern "C" fn device_enumerated(
    guid: *mut c_void,
    description: *mut c_char,
    _name: *mut c_char,
    context: *mut c_void,
    monitor: *mut c_void,
) -> i32 {
    // no reference across the C++ calls
    let bytes = unsafe { core::ffi::CStr::from_ptr(description) }.to_bytes().to_vec();
    raster::on_device_enumerated(
        &mut RawGx,
        &mut RawHost,
        &mut Enumeration(context as *mut EeDx7Rasterizer),
        guid.is_null(),
        monitor.is_null(),
        &bytes,
    );
    1 // TRUE: keep enumerating
}

// ---- the C ABI: DX7Rasterizer's forwarders ----------------------------------------------------------------------------

/// A cached render state (`ee_dx7r_set`): the `unsigned long` setters.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeDx7RastSet {
    AlphaTesting = 1,
    AlphaTestingCompareState,
    AntiAliasing,
    ColorKeying,
    Dithering,
    DrawStyle,
    Fog,
    Lighting,
    Shading,
    ShadingColorSource,
    Specular,
    ZBuffer,
    ZCompare,
}

/// A colour setter (`ee_dx7r_set_color`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeDx7RastColor {
    Ambient = 1,
    Fog,
    Debug,
}

/// `DX7Rasterizer(inName)`: `base` is the `GERasterizer`'s object.
#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_new(
    host: EeDx7RastHostFn,
    owner: *mut c_void,
    base: *mut EeGeRasterizer,
    tnl: bool,
) -> *mut EeDx7Rasterizer {
    EeDx7Rasterizer::create(host, owner as usize, base, tnl)
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_destroy(p: *mut EeDx7Rasterizer) {
    // SAFETY: the caller's contract
    unsafe { EeDx7Rasterizer::destroy(p) }
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_activate_rasterizer(p: *mut EeDx7Rasterizer) -> i32 {
    // SAFETY: the caller's contract
    unsafe { EeDx7Rasterizer::activate_rasterizer(p) }
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_add_valid_screen(p: *mut EeDx7Rasterizer, screen: *mut c_void) {
    // SAFETY: the caller's contract
    unsafe { EeDx7Rasterizer::add_valid_screen(p, screen) }
}

/// `ActivateScreen`: `width`, `height` are the window's.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_dx7r_activate_screen(
    p: *mut EeDx7Rasterizer,
    screen: c_ulong,
    manager: *mut c_void,
    window: *mut c_void,
    width: u32,
    height: u32,
    back_buffers: u32,
    z_buffer: bool,
    z_format: u32,
) -> i32 {
    // SAFETY: the caller's contract
    unsafe {
        EeDx7Rasterizer::begin_activation(p, screen, manager, window);
        EeDx7Rasterizer::activate_screen(p, width, height, back_buffers, z_buffer, z_format)
    }
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_activate_rendering_surface(
    p: *mut EeDx7Rasterizer,
    screen: c_ulong,
    manager: *mut c_void,
    window: *mut c_void,
    width: u32,
    height: u32,
) -> i32 {
    // SAFETY: the caller's contract
    unsafe {
        EeDx7Rasterizer::begin_activation(p, screen, manager, window);
        EeDx7Rasterizer::activate_rendering_surface(p, width, height)
    }
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_activate_the_screen(
    p: *mut EeDx7Rasterizer,
    screen: c_ulong,
    secondary: *mut c_void,
) -> i32 {
    // SAFETY: the caller's contract
    unsafe { EeDx7Rasterizer::activate_the_screen(p, screen, secondary) }
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_deactivate(p: *mut EeDx7Rasterizer) {
    // SAFETY: the caller's contract
    unsafe { EeDx7Rasterizer::deactivate(p) }
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_begin_scene(p: *mut EeDx7Rasterizer, target: *mut c_void, screen: c_ulong) -> i32 {
    // SAFETY: the caller's contract
    unsafe { EeDx7Rasterizer::begin_scene(p, target, screen) }
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_end_scene(p: *mut EeDx7Rasterizer, screen: c_ulong) -> i32 {
    // SAFETY: the caller's contract
    unsafe { EeDx7Rasterizer::end_scene(p, screen) }
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_can_do_current_state(
    p: *mut EeDx7Rasterizer,
    texture: *mut c_void,
    screen: c_ulong,
) -> bool {
    // SAFETY: the caller's contract
    unsafe { EeDx7Rasterizer::can_do_current_state(p, texture, screen) }
}

/// `Clear` (`color` a `GEColor`) or, `color` NULL, `ClearZBuffer` to `z`; `rect` is left, top, right, bottom.
#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_clear(
    p: *mut EeDx7Rasterizer,
    rect: *const f32,
    color: *const EeGeRastColor,
    z: f32,
    screen: c_ulong,
) -> i32 {
    // SAFETY: the caller's contract
    unsafe {
        let rect = [*rect, *rect.add(1), *rect.add(2), *rect.add(3)];
        match color.as_ref() {
            Some(c) => EeDx7Rasterizer::clear(p, rect, [c.red, c.green, c.blue], screen),
            None => EeDx7Rasterizer::clear_z(p, rect, z, screen),
        }
    }
}

/// `GetTotalVideoMemory` (`total`) or `GetAvailableVideoMemory`: `out` is written on success only.
#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_video_memory(
    p: *mut EeDx7Rasterizer,
    screen: c_ulong,
    total: bool,
    out: *mut u32,
) -> i32 {
    // SAFETY: the caller's contract
    unsafe {
        let (result, t, f) = EeDx7Rasterizer::video_memory(p, screen);
        if result == K_OK {
            *out = if total { t } else { f };
        }
        result
    }
}

/// `GetDirtyRectangleSupport` (`dirty`) or `GetCanModulateTextureAlpha`.
#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_card_flag(p: *mut EeDx7Rasterizer, screen: c_ulong, dirty: bool) -> bool {
    // SAFETY: the caller's contract
    let (modulate, dirty_rectangles) = unsafe { EeDx7Rasterizer::card_flags(p, screen) };
    if dirty {
        dirty_rectangles
    } else {
        modulate
    }
}

/// `AddLight`: `light` is a `GELight`.
#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_add_light(
    p: *mut EeDx7Rasterizer,
    light: *mut c_void,
    screen: c_ulong,
    out_index: *mut u32,
) -> i32 {
    // SAFETY: the caller's contract
    unsafe {
        let (result, index) = EeDx7Rasterizer::add_light(p, light, screen);
        *out_index = index;
        result
    }
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_update_light(
    p: *mut EeDx7Rasterizer,
    index: u32,
    light: *mut c_void,
    screen: c_ulong,
) -> i32 {
    // SAFETY: the caller's contract
    unsafe { EeDx7Rasterizer::update_light(p, index, light, screen) }
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_remove_light(p: *mut EeDx7Rasterizer, index: u32, screen: c_ulong) -> i32 {
    // SAFETY: the caller's contract
    unsafe { EeDx7Rasterizer::remove_light(p, index, screen) }
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_remove_all_lights(p: *mut EeDx7Rasterizer, screen: c_ulong) -> i32 {
    // SAFETY: the caller's contract
    unsafe { EeDx7Rasterizer::remove_all_lights(p, screen) }
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_set(p: *mut EeDx7Rasterizer, which: EeDx7RastSet, value: c_ulong, screen: c_ulong) {
    type R = EeDx7Rasterizer;
    // SAFETY: the caller's contract
    unsafe {
        match which {
            EeDx7RastSet::AlphaTesting => R::set_alpha_testing(p, value, screen),
            EeDx7RastSet::AlphaTestingCompareState => R::set_alpha_testing_compare_state(p, value, screen),
            EeDx7RastSet::AntiAliasing => R::set_anti_aliasing(p, value, screen),
            EeDx7RastSet::ColorKeying => R::set_color_keying(p, value, screen),
            EeDx7RastSet::Dithering => R::set_dithering(p, value, screen),
            EeDx7RastSet::DrawStyle => R::set_draw_style(p, value, screen),
            EeDx7RastSet::Fog => R::set_fog(p, value, screen),
            EeDx7RastSet::Lighting => R::set_lighting(p, value, screen),
            EeDx7RastSet::Shading => R::set_shading(p, value, screen),
            EeDx7RastSet::ShadingColorSource => R::set_shading_color_source(p, value, screen),
            EeDx7RastSet::Specular => R::set_specular(p, value, screen),
            EeDx7RastSet::ZBuffer => R::set_z_buffer(p, value, screen),
            EeDx7RastSet::ZCompare => R::set_z_compare(p, value, screen),
        }
    }
}

/// `SetAmbientIntensity` (`intensity`) or `SetAlphaTestingRefValue`.
#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_set_float(p: *mut EeDx7Rasterizer, intensity: bool, value: f32, screen: c_ulong) {
    // SAFETY: the caller's contract
    unsafe {
        if intensity {
            EeDx7Rasterizer::set_ambient(p, core::ptr::null(), value, screen);
        } else {
            EeDx7Rasterizer::set_alpha_testing_ref_value(p, value, screen);
        }
    }
}

/// `SetAmbientColor`, `SetFogColor`, `SetDebugColor`: `color` is a `GEColor`.
#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_set_color(
    p: *mut EeDx7Rasterizer,
    which: EeDx7RastColor,
    color: *const EeGeRastColor,
    screen: c_ulong,
) {
    // SAFETY: the caller's contract
    unsafe {
        match which {
            EeDx7RastColor::Ambient => EeDx7Rasterizer::set_ambient(p, color, 0.0, screen),
            EeDx7RastColor::Fog => EeDx7Rasterizer::set_fog_color(p, color, screen),
            EeDx7RastColor::Debug => {
                let c = *color;
                EeDx7Rasterizer::set_debug_color(p, [c.red, c.green, c.blue])
            }
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_set_blending(
    p: *mut EeDx7Rasterizer,
    source: c_ulong,
    destination: c_ulong,
    screen: c_ulong,
) {
    // SAFETY: the caller's contract
    unsafe { EeDx7Rasterizer::set_blending(p, source, destination, screen) }
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_set_culling(p: *mut EeDx7Rasterizer, value: c_ulong) {
    // SAFETY: the caller's contract
    unsafe { EeDx7Rasterizer::set_culling(p, value) }
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_set_fog_range(p: *mut EeDx7Rasterizer, start: f32, end: f32, screen: c_ulong) {
    // SAFETY: the caller's contract
    unsafe { EeDx7Rasterizer::set_fog_range(p, start, end, screen) }
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_set_render_target(p: *mut EeDx7Rasterizer, target: *mut c_void, screen: c_ulong) {
    // SAFETY: the caller's contract
    unsafe { EeDx7Rasterizer::set_render_target(p, target, screen) }
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_set_texture(p: *mut EeDx7Rasterizer, texture: *mut c_void, screen: c_ulong) {
    // SAFETY: the caller's contract
    unsafe { EeDx7Rasterizer::set_texture(p, texture, screen) }
}

/// `SetViewport`: `viewport` is a `GEViewport`.
#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_set_viewport(p: *mut EeDx7Rasterizer, viewport: *const c_void, screen: c_ulong) {
    // SAFETY: the caller's contract
    unsafe { EeDx7Rasterizer::set_viewport(p, viewport as *const GeViewport, screen) }
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_show_scene(p: *mut EeDx7Rasterizer, screen: c_ulong) -> i32 {
    // SAFETY: the caller's contract
    unsafe { EeDx7Rasterizer::show_scene(p, screen) }
}
