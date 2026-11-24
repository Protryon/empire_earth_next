//! The render objects' per-frame bodies (was EERenderObject.cpp; game core lane round 9): the frame's render lists
//! (FillRenderLists: the visible tiles' units, map objects and remembered ones, the ghost units, the music and the
//! ambient sounds), the scene's clocks, fade, letterbox, camera and time-of-day lights (Fn10C036), the terrain pass's
//! set-up (Fn10E8D1), the effects' rows, the lighting set-ups' rasterizer calls, the scene's start and release, the
//! render objects' posts, and the raster table's and the fill host's answers that were more than one C++ statement.
//! Each C++ call or member is a case of EERenderObject.cpp's `EECore9OpA` ([`super::EeGameCpp`]); the C++ functions
//! forward here. The arithmetic is ee_gfx's (scene.rs, capture.rs, fill.rs, lighting.rs), called as the C++ called it.
//!
//! Only a game with a window runs these (the tour, gates.sh): EE-SIM draws nothing.

use std::collections::BTreeSet;
use std::ffi::{c_long, c_ulong, c_void, CStr};
use std::sync::atomic::{AtomicBool, AtomicU32, AtomicUsize, Ordering::Relaxed};
use std::sync::Mutex;

use ee_base::math::transform::RotationOrder;
use ee_base::math::types::{Ge3DPoint, GeRectangle, GeTransformation, GeVector};
use ee_base::math::um;
use ee_render::gfx::capture;
use ee_render::gfx::effects::EeGfxColor;
use ee_render::gfx::fill::{
    fill_object_fields, fill_unit_fields, EeGfxFillFrame, EeGfxFillMorale, EeGfxFillObject, EeGfxFillPoint,
    EeGfxFillUnit, FillHost,
};
use ee_render::gfx::lighting::{lighting, EeGfxLighting, EeGfxLightingInputs, EeGfxRasterOpKind};
use ee_render::gfx::raster::EeGfxTextureLoad;
use ee_render::gfx::scene::{self, EeGfxSceneClocks, EeGfxSceneFade};
use ee_render::gfx::walk::EeGfxDrawnHost;
use ee_sim::world::{UnitView, WorldRead};

use super::{cptr, h, hd, hf, hp, pa, ptr, set_position, O, P};

/// `kUnitTypeRoboticFarm`, `kUnitTypeSpaceTurret`, `kUnitTypeOrbitalSpaceStation` (static_asserted by the C++).
const RO_TYPE_ROBOTIC_FARM: c_ulong = 0x33a;
const RO_TYPE_SPACE_TURRET: i64 = 0x302;
const RO_TYPE_ORBITAL_STATION: i64 = 0x31f;
/// `kAreaEffectMorale`, `kAreaEffectMoraleFromBuildings`, `kAreaEffectMoraleFromMilitaryHeroes`.
const RO_AE_MORALE: [i64; 3] = [2, 5, 6];
/// `kStageOpModulate`.
const RO_STAGE_MODULATE: i64 = 2;
/// The render pools' sizes (`EERenderLists::mRenderUnits`, `mRenderShadows`).
const RO_UNIT_POOL: c_ulong = 0x200;
const RO_OBJECT_POOL: c_ulong = 0x600;
/// FillRenderLists' yaw offset (pi / 4) and the music check's period (ms).
const RO_YAW_OFFSET: f32 = 0.78539819;
const RO_MUSIC_PERIOD: c_ulong = 0xfa;
/// DrawGroundCircle's points (the 50 and the first again).
const RO_RING_POINTS: usize = 0x33;

/// the space-cliff and cliff-edge textures (`sCliffTextures`, 0x4be37c)
const RO_CLIFF_TEXTURES: [&CStr; 13] = [
    c"textures\\spacecliff_1",
    c"textures\\spacecliff_2",
    c"textures\\spacecliff_3",
    c"textures\\spacecliff_4",
    c"textures\\spacecliff_5",
    c"textures\\spacecliff_6",
    c"textures\\spacecliff_7",
    c"textures\\spacecliff_8",
    c"textures\\cliffedge_1",
    c"textures\\cliffedge_2",
    c"textures\\cliffedge_3",
    c"textures\\cliffedge_4",
    c"textures\\cliffedge_5",
];

// the file's statics that only these bodies read (the original's .bss; provisional names)
/// `sSwayClock` (0x529084) and its last time `sU529094`
static SWAY_CLOCK: AtomicU32 = AtomicU32::new(0);
static SWAY_LAST: AtomicU32 = AtomicU32::new(0);
/// `sU529090`: the music check's last time; `sL52908C`: the frame counter
static MUSIC_LAST: Mutex<c_ulong> = Mutex::new(0);
static FRAME_COUNTER: Mutex<c_ulong> = Mutex::new(0);
/// Fn10C036's and Fn10E8D1's `static unsigned long sLastTime = TMGetTime()`
static SCENE_LAST: Mutex<Option<c_ulong>> = Mutex::new(None);
static TERRAIN_LAST: Mutex<Option<c_ulong>> = Mutex::new(None);
/// `sSceneInitialised` (0x529064) and the scene's bitmap `sU529060`
static SCENE_INITIALISED: AtomicBool = AtomicBool::new(false);
static SCENE_BITMAP: AtomicUsize = AtomicUsize::new(0);

/// `gRenderEnvironment` (ee/EERenderLists.h `EERenderEnvironment`), member for member.
#[repr(C)]
struct RenderEnv {
    direction: [f32; 3],
    terrain_ambient_color: [f32; 3],
    terrain_light_color: [f32; 3],
    unit_ambient_color: [f32; 3],
    unit_light_color: [f32; 3],
    prelit_color: [f32; 3],
    dim_prelit_color: [f32; 3],
    color54: [f32; 3],
    unit_ambient_intensity: f32,
    unit_light_intensity: f32,
    terrain_ambient_intensity: f32,
    terrain_light_intensity: f32,
    sea_level: f32,
    time_of_day: f32,
    water_phase: f32,
    cloud_offset: f32,
    wind_speed: f32,
    camera_yaw: f32,
    show_hidden_units: c_long,
    b8c: bool,
    time_of_day_changed: bool,
}

/// `EERenderFrameInfo` (ee/EERenderLists.h), member for member: FillRenderLists' frame state.
#[repr(C)]
struct FrameInfo {
    picked_unit: P,
    yaw: f32,
    fighting: f32,
    frame: c_ulong,
    ambient: bool,
}

/// `GETextureDescription` (lle/GETextureManager.h), member for member.
#[repr(C)]
struct TextureDescription {
    surface_volatility: c_ulong,
    frame_format: c_ulong,
    pixel_depth: c_ulong,
    mip_levels_to_drop: c_long,
    is_3d_renderable: bool,
    transparency: bool,
    has_alpha: bool,
    mip_maps: bool,
    use_texture_pack: bool,
}

fn now() -> c_ulong {
    h(O::TimeNow, 0, &[]) as c_ulong
}

fn world() -> P {
    hp(O::WorldAddr, 0, &[])
}

/// `gRenderEnvironment` (0x528ce0; its ctor 0x111722): the graphics thread's lighting and sky state, Rust's since
/// round 10 (no C++ reads it).
struct RenderEnvCell(std::cell::UnsafeCell<RenderEnv>);
// SAFETY: the graphics thread's (the C++ global's access, unchanged)
unsafe impl Sync for RenderEnvCell {}
const RO_WHITE: [f32; 3] = [1.0, 1.0, 1.0];
static RENDER_ENV: RenderEnvCell = RenderEnvCell(std::cell::UnsafeCell::new(RenderEnv {
    direction: [0.0; 3],
    terrain_ambient_color: RO_WHITE,
    terrain_light_color: RO_WHITE,
    unit_ambient_color: RO_WHITE,
    unit_light_color: RO_WHITE,
    prelit_color: RO_WHITE,
    dim_prelit_color: RO_WHITE,
    color54: RO_WHITE,
    unit_ambient_intensity: 0.0,
    unit_light_intensity: 0.0,
    terrain_ambient_intensity: 0.0,
    terrain_light_intensity: 0.0,
    sea_level: 0.0,
    time_of_day: 0.0,
    water_phase: 0.0,
    cloud_offset: 0.0,
    wind_speed: 1.25e-6,
    camera_yaw: 0.0,
    show_hidden_units: 1,
    b8c: false,
    time_of_day_changed: true,
}));

fn env() -> *mut RenderEnv {
    RENDER_ENV.0.get()
}

/// `gRenderEnvironment.mCameraYaw` (the units' scene).
pub fn camera_yaw() -> f32 {
    // SAFETY: gRenderEnvironment
    unsafe { (*env()).camera_yaw }
}

/// `gRenderEnvironment.mTimeOfDay` and `mCloudOffset` (the terrain's frame).
pub fn time_of_day_and_cloud_offset() -> (f32, f32) {
    // SAFETY: gRenderEnvironment
    unsafe { ((*env()).time_of_day, (*env()).cloud_offset) }
}

/// `gRenderEnvironment.mCloudOffset = offset` (the terrain's frame).
pub fn set_cloud_offset(offset: f32) {
    // SAFETY: gRenderEnvironment
    unsafe { (*env()).cloud_offset = offset };
}

/// A C++ bool global by its address op.
fn flag(op: O) -> bool {
    // SAFETY: the op answers a bool global's address
    unsafe { *(hp(op, 0, &[]) as *const bool) }
}

fn set_flag(op: O, v: bool) {
    // SAFETY: as flag
    unsafe { *(hp(op, 0, &[]) as *mut bool) = v };
}

/// A C++ float global by its address op.
fn float_at(op: O) -> *mut f32 {
    hp(op, 0, &[]) as *mut f32
}

/// The render pools' counts (`mRenderUnitCount`, `mRenderShadowCount`).
fn unit_count() -> *mut c_ulong {
    hp(O::RoUnitCount, 0, &[]) as *mut c_ulong
}

fn object_count() -> *mut c_ulong {
    hp(O::RoObjectCount, 0, &[]) as *mut c_ulong
}

fn color(c: [f32; 3]) -> EeGfxColor {
    EeGfxColor { red: c[0], green: c[1], blue: c[2] }
}

fn rgb(c: EeGfxColor) -> [f32; 3] {
    [c.red, c.green, c.blue]
}

/// `GETransformation::SetElement(3, 0..3, 0 0 0 1)`.
fn set_bottom_row(t: &mut GeTransformation) {
    t.m[3] = [0.0, 0.0, 0.0, 1.0];
}

/// A transformation facing the yaw at a point (the map objects' and IsUnitDrawn's model placement).
fn placed(yaw: f32, x: f32, y: f32, z: f32) -> GeTransformation {
    let mut t = GeTransformation::default();
    t.set_orientation(RotationOrder::Ypr, yaw, 0.0, 0.0);
    set_position(&mut t, x, y, z);
    set_bottom_row(&mut t);
    t
}

/// The camera's yaw: the world's forward (0, 1, 0) through the camera transformation.
fn camera_direction() -> GeVector {
    // SAFETY: the world's transformation member
    let t = unsafe { *(hp(O::RoWorldTransformation, 0, &[]) as *const GeTransformation) };
    t.apply_vector(GeVector::new(0.0, 1.0, 0.0))
}

/// `GEPhysicalModel(model, location, scale).Intersects(gViewport, gOrthographicView)`.
fn model_in_view(model: P, t: &GeTransformation, scale: f32) -> bool {
    hf(O::RoModelInView, model, &[cptr(t)], &[scale]) != 0
}

// ---------------------------------------------------------------------------------------------- the scene

/// EEInitialiseScene: the light and the time-of-day colour, the fade and letterbox, the scene's flags, the effect
/// queues, the render units' bars, once per scene, and the shared bitmap.
#[no_mangle]
pub extern "C" fn ee_gamecpp_ro_initialise_scene() {
    if SCENE_INITIALISED.load(Relaxed) {
        return;
    }
    hf(O::RoLightAttenuation0, 0, &[], &[1.0]);
    hf(O::RoLightAttenuation1, 0, &[], &[0.0]);
    // SAFETY: gRenderEnvironment
    unsafe { (*env()).time_of_day = -100.0 };
    h(O::RoLightStyle, 0, &[1]);
    hf(O::RoLightAttenuation2, 0, &[], &[0.0]);
    // SAFETY: gU4E9338's three floats
    unsafe { *(hp(O::RoAmbientColor, 0, &[]) as *mut [f32; 3]) = [0.663, 0.784, 1.0] };
    // the key frames and the fill lights: ee_gfx scene.rs
    // SAFETY: the C++'s fade state, not otherwise borrowed during the call
    unsafe { scene::reset(&mut *(hp(O::RoSceneFade, 0, &[]) as *mut EeGfxSceneFade)) };
    set_flag(O::RoFlag529066, false);
    set_flag(O::RoCameraFollowing, false);
    set_flag(O::RoFlag529068, false);
    // SAFETY: gF4BE374
    unsafe { *float_at(O::RoLetterboxZoom) = 1.0 };
    // SAFETY: the effects' render queues (ee_gfx effect_obj.rs)
    unsafe { ee_render::gfx::effect_obj::ee_gfx_effect_queues_clear(false) };
    for j in 0..RO_UNIT_POOL {
        h(O::RoUnitBarsResize, hp(O::RoRenderUnit, 0, &[j as i64]), &[1]);
    }
    SCENE_INITIALISED.store(true, Relaxed);
    SCENE_BITMAP.store(hp(O::RoNewBitmap, 0, &[]), Relaxed);
}

/// EEReleaseScene: the effect queues' references dropped, the render pools and lists emptied, the shared bitmap
/// released and the unit picker reset (only after EEInitialiseScene).
#[no_mangle]
pub extern "C" fn ee_gamecpp_ro_release_scene() {
    if !SCENE_INITIALISED.load(Relaxed) {
        return;
    }
    h(O::WorldReadLock, 0, &[]);
    // SAFETY: the effects' render queues, under the world's read lock
    unsafe { ee_render::gfx::effect_obj::ee_gfx_effect_queues_clear(true) };
    h(O::WorldReadUnlock, 0, &[]);
    unload_render_objects();
    unload_render_units();
    clear_lists();
    // SAFETY: the pools' counts
    unsafe {
        *unit_count() = 0;
        *object_count() = 0;
    }
    unload_ghost_records();
    h(O::RoBitmapDereference, SCENE_BITMAP.load(Relaxed), &[]);
    h(O::PickerReset, 0, &[]);
    SCENE_INITIALISED.store(false, Relaxed);
}

/// The six texture maps and the two selected lists emptied (FillRenderLists' and EEReleaseScene's order).
fn clear_lists() {
    super::ro_lists::clear();
}

/// UnloadRenderShadows: every pooled map object's media unloaded, the pool emptied.
fn unload_render_objects() {
    let count = object_count();
    // SAFETY: the pool's count
    for i in 0..unsafe { *count } {
        h(O::RoObjectUnloadMedia, hp(O::RoRenderObject, 0, &[i as i64]), &[]);
    }
    // SAFETY: as above
    unsafe { *count = 0 };
}

/// UnloadRenderUnits: every pooled render unit's media unloaded, its bars and turning parts reset; the pointed-at
/// unit and object cleared, the pool emptied.
fn unload_render_units() {
    let count = unit_count();
    // SAFETY: the pool's count
    for i in 0..unsafe { *count } {
        let ru = hp(O::RoRenderUnit, 0, &[i as i64]);
        let head = ru as *const EeGfxFillUnit;
        // SAFETY: the render unit's head
        h(O::GobjUnloadMedia, unsafe { (*head).source } as P, &[]);
        // SAFETY: as above
        let source2 = unsafe { (*head).source2 } as P;
        if source2 != 0 {
            h(O::GobjUnloadMedia, source2, &[]);
        }
        if h(O::RoUnitBarsCount, ru, &[]) != 1 {
            h(O::RoUnitBarsResize, ru, &[1]);
        }
        h(O::RoUnitPartsClear, ru, &[]);
    }
    super::ro_lists::clear_selected();
    // SAFETY: as above
    unsafe { *count = 0 };
}

/// UnloadGhostRecords: the ghost records emptied from the back, each one's media unloaded.
fn unload_ghost_records() {
    while h(O::RoGhostRecordsCount, 0, &[]) > 0 {
        let source = hp(O::RoGhostRecordsBackSource, 0, &[]);
        h(O::RoGhostRecordsPop, 0, &[]);
        h(O::GobjUnloadMedia, source, &[]);
    }
}

/// EEStartLetterboxOut / EEStartLetterboxIn over a number of frames (at once for 0).
#[no_mangle]
pub extern "C" fn ee_gamecpp_ro_letterbox(out: bool, frames: c_ulong) {
    // SAFETY: the fade state and gF4BE374
    unsafe {
        let zoom = *float_at(O::RoLetterboxZoom);
        scene::start_letterbox(&mut *(hp(O::RoSceneFade, 0, &[]) as *mut EeGfxSceneFade), out, frames as u32, zoom)
    }
}

/// EERenderObject::Fn10C036: the grid, water and cloud clocks, the light fade, the letterbox and the view zoom (the
/// bars cleared, the view band set, the UI's backgrounds shown or hidden), the camera and viewport, and the light
/// colours on a time-of-day change; once per frame (gB529066).
#[no_mangle]
pub extern "C" fn ee_gamecpp_ro_fn10c036(_ro: *mut c_void) {
    let time_of_day = {
        // SAFETY: the world's time of day
        unsafe { *(hp(O::WTimeOfDay, world(), &[]) as *const f32) }
    };
    let last = {
        let mut s = SCENE_LAST.lock().unwrap();
        *s.get_or_insert_with(now)
    };
    if flag(O::RoFlag529066) {
        return;
    }
    let now = now();
    let elapsed = now.wrapping_sub(last) as f32;
    *SCENE_LAST.lock().unwrap() = Some(now);
    let e = env();
    // the clocks, the fade and the letterbox: ee_gfx scene.rs
    let wind = hd(O::RoEnvironmentWind, 0, &[], &[]);
    let grid = float_at(O::RoGridPhase);
    // SAFETY: gRenderEnvironment, gU4E9330 (no C++ call between the reads and the writes)
    let step = unsafe {
        (*e).wind_speed = wind;
        let mut clocks =
            EeGfxSceneClocks { grid_phase: *grid, water_phase: (*e).water_phase, cloud_offset: (*e).cloud_offset };
        scene::step_clocks(&mut clocks, elapsed, (*e).wind_speed);
        *grid = clocks.grid_phase;
        (*e).water_phase = clocks.water_phase;
        (*e).cloud_offset = clocks.cloud_offset;
        scene::step(&mut *(hp(O::RoSceneFade, 0, &[]) as *mut EeGfxSceneFade), elapsed, *float_at(O::RoLetterboxZoom))
    };
    let height = h(O::GameWindowHeight, 0, &[]) as c_long as f32;
    let width = h(O::GameWindowWidth, 0, &[]) as c_long as f32;
    let bars = scene::letterbox(step.zoom, height);
    let clear = |top: f32, bottom: f32| {
        let r = GeRectangle { bottom, left: 0.0, right: width, top };
        h(O::RoRastClearBlack, 0, &[cptr(&r)]);
    };
    if step.clear_bars {
        clear(0.0, bars.top_bar_end);
        clear(bars.bottom_bar_start, height);
    }
    // the zoom changed: the view band, the bars cleared (one row more at the top), the UI's backgrounds at full view
    if step.set_view {
        let view = GeRectangle { bottom: bars.bottom, left: bars.left, right: bars.right, top: bars.top };
        // SAFETY: the view's rectangle
        unsafe { super::startup::ee_gamecpp_startup_game_view(&view) };
        clear(0.0, bars.top_bar_end + 1.0);
        clear(bars.bottom_bar_start, height);
        if step.show_backgrounds {
            h(O::RoShowAllBackgrounds, 0, &[]);
            h(O::UiRedrawAll, 0, &[]);
        }
        if step.hide_backgrounds {
            h(O::RoHideAllBackgrounds, 0, &[]);
            h(O::UiRedrawAll, 0, &[]);
        }
    }
    // SAFETY: gF4BE374
    unsafe { *float_at(O::RoLetterboxZoom) = step.zoom };
    h(O::RoViewportSetCamera, 0, &[]);
    h(O::RoRastSetViewport, 0, &[]);
    h(O::RoCopyViewport, 0, &[]);
    let direction = camera_direction();
    // SAFETY: the world's wind direction (a GEVector)
    let wind_direction = unsafe { *(hp(O::RoWindDirection, 0, &[]) as *const [f32; 3]) };
    // SAFETY: gRenderEnvironment
    let changed = unsafe {
        (*e).camera_yaw = um::um_arctan2(-direction.x, direction.y);
        (*e).direction = wind_direction;
        (*e).sea_level = 0.0;
        (*e).b8c = false;
        (*e).time_of_day != time_of_day
    };
    if changed {
        // SAFETY: gU4E9338, the world's sun and moon directions
        let (ambient, sun, moon) = unsafe {
            (
                *(hp(O::RoAmbientColor, 0, &[]) as *const [f32; 3]),
                *(hp(O::WSunDirection, world(), &[]) as *const GeVector),
                *(hp(O::WMoonDirection, world(), &[]) as *const GeVector),
            )
        };
        // the blend of the 17 key frames, the lit direction and the prelit colour: ee_gfx scene.rs
        let mut tables = scene::initial_tables();
        tables.ambient_color = color(ambient);
        tables.sun = sun;
        tables.moon = moon;
        let light = scene::daylight(&tables, time_of_day);
        // SAFETY: gU4E9210, gRenderEnvironment
        unsafe {
            *(hp(O::RoSunDirection, 0, &[]) as *mut GeVector) = light.direction;
            (*e).time_of_day = time_of_day;
            (*e).time_of_day_changed = true;
            let blend = rgb(light.light);
            let prelit = rgb(light.prelit);
            (*e).terrain_light_color = blend;
            (*e).unit_light_color = blend;
            (*e).terrain_ambient_color = ambient;
            (*e).unit_ambient_color = ambient;
            (*e).terrain_light_intensity = light.light_intensity;
            (*e).unit_light_intensity = light.light_intensity;
            (*e).terrain_ambient_intensity = light.ambient_intensity;
            (*e).unit_ambient_intensity = light.unit_ambient_intensity;
            (*e).prelit_color = prelit;
            (*e).dim_prelit_color = rgb(light.dim_prelit);
            (*e).color54 = prelit;
        }
    } else {
        // SAFETY: gRenderEnvironment
        unsafe { (*e).time_of_day_changed = false };
    }
    if (changed || step.fade_changed) && flag(O::RoCameraFollowing) {
        apply_lighting(EeGfxLighting::Terrain);
        h(O::RoRastLightMesh, 0, &[]);
    }
    let hidden = h(O::RoHiddenUnitsMode, 0, &[]) as c_long;
    // SAFETY: gRenderEnvironment
    unsafe { (*e).show_hidden_units = hidden };
    set_flag(O::RoFlag529066, true);
}

/// EERenderObject::Fn10E8D1: the terrain pass's set-up: the frame's scene state, the space-cliff textures, the
/// visible terrain and its lighting, then the camera transform (at most every 100 ms while the camera follows).
#[no_mangle]
pub extern "C" fn ee_gamecpp_ro_fn10e8d1(ro: *mut c_void) {
    let last = {
        let mut s = TERRAIN_LAST.lock().unwrap();
        *s.get_or_insert_with(now)
    };
    let viewport = hp(O::RoRastViewport, 0, &[]);
    let now = now();
    let elapsed = now.wrapping_sub(last);
    if !flag(O::RoCameraFollowing) {
        ee_gamecpp_ro_fn10c036(ro);
        if h(O::MeshIsSpaceMap, 0, &[]) != 0 && h(O::RoMeshCliffTextureCount, 0, &[]) < 1 {
            if h(O::RoCliffTexturesCount, 0, &[]) == 0 {
                load_cliff_textures();
            }
            h(O::RoMeshSetCliffTextures, 0, &[]);
        }
        h(O::RoUpdateVisibleTerrain, viewport, &[]);
        apply_lighting(EeGfxLighting::Terrain);
        h(O::RoRastLightMesh, 0, &[]);
    }
    if !flag(O::RoCameraFollowing) || elapsed >= 100 {
        hf(O::RoTransformToCamera, viewport, &[], &[elapsed as f32 / 1000.0]);
        *TERRAIN_LAST.lock().unwrap() = Some(now);
    }
    set_flag(O::RoCameraFollowing, true);
}

/// LoadCliffTextures: the space-cliff and cliff-edge textures loaded into gU4E8EF0 (modulated, referenced).
fn load_cliff_textures() {
    let description = TextureDescription {
        surface_volatility: 1,
        frame_format: 0,
        pixel_depth: 16,
        mip_levels_to_drop: 0,
        is_3d_renderable: false,
        transparency: false,
        has_alpha: true,
        mip_maps: true,
        use_texture_pack: true,
    };
    for name in RO_CLIFF_TEXTURES {
        let texture = hp(O::RoLoadTexture, 0, &[name.as_ptr() as usize as i64, cptr(&description)]);
        h(O::RoTexAlphaMode, texture, &[RO_STAGE_MODULATE]);
        h(O::RoTexColorMode, texture, &[RO_STAGE_MODULATE]);
        h(O::RoTexReference, texture, &[]);
        h(O::RoCliffTexturesPush, texture, &[]);
    }
}

/// The raster table's load_texture (ee_gfx raster.rs): a texture loaded with its stage settings.
///
/// # Safety
/// `name` is a NUL-terminated path, `load` a readable `EeGfxTextureLoad`.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_ro_load_texture(name: *const u8, load: *const c_void) -> *mut c_void {
    // SAFETY: the function's contract (an EeGfxTextureLoad)
    let l = unsafe { &*(load as *const EeGfxTextureLoad) };
    let description = TextureDescription {
        surface_volatility: l.volatility as c_ulong,
        frame_format: l.frame_format as c_ulong,
        pixel_depth: l.pixel_depth as c_ulong,
        mip_levels_to_drop: l.mip_levels_to_drop as c_long,
        is_3d_renderable: false,
        transparency: false,
        has_alpha: l.has_alpha,
        mip_maps: l.mip_maps,
        use_texture_pack: true,
    };
    let texture = hp(O::RoLoadTexture, 0, &[name as usize as i64, cptr(&description)]);
    if l.reference {
        h(O::RoTexReference, texture, &[]);
    }
    h(O::RoTexAlphaMode, texture, &[l.alpha_mode as i64]);
    h(O::RoTexColorMode, texture, &[l.color_mode as i64]);
    h(O::RoTexMinFilter, texture, &[l.min_filter as i64]);
    h(O::RoTexMagFilter, texture, &[l.mag_filter as i64]);
    h(O::RoTexMipFilter, texture, &[l.mip_filter as i64]);
    hf(O::RoTexMipBias, texture, &[], &[l.mip_bias]);
    h(O::RoTexAddressing, texture, &[l.addressing as i64]);
    texture as *mut c_void
}

/// EERenderObject::QueueEffects: the effect render queues rebuilt over the visible tile rows (ee_gfx effect_obj.rs).
#[no_mangle]
pub extern "C" fn ee_gamecpp_ro_queue_effects(_ro: *mut c_void) {
    let count = h(O::RoVisibleRowCount, 0, &[]) as usize;
    let mut rows = vec![0i64; count * 3];
    for j in 0..count {
        let row = visible_row(j);
        rows[j * 3..j * 3 + 3].copy_from_slice(&[row[0] as i64, row[1] as i64, row[2] as i64]);
    }
    let p = if rows.is_empty() { std::ptr::null() } else { rows.as_ptr() };
    // SAFETY: `count` rows of three
    unsafe { ee_render::gfx::effect_obj::ee_gfx_effect_queues_fill(p, count) };
}

/// A visible tile row: its y, first and last x.
fn visible_row(j: usize) -> [c_long; 3] {
    let mut row: [c_long; 3] = [0; 3];
    h(O::RoVisibleRow, 0, &[j as i64, ptr(&mut row)]);
    row
}

// ---------------------------------------------------------------------------------------------- the lighting

/// The lighting set-ups (SetSceneFog, SetLightingOff, SetTerrainLighting, the raster table's): ee_gfx lighting.rs's
/// rasterizer calls over the environment, applied in order.
#[no_mangle]
pub extern "C" fn ee_gamecpp_ro_lighting(kind: u32) {
    let kind = match kind {
        0 => EeGfxLighting::SceneFog,
        1 => EeGfxLighting::FogOff,
        2 => EeGfxLighting::Off,
        3 => EeGfxLighting::Prelit,
        4 => EeGfxLighting::Cloud,
        5 => EeGfxLighting::White,
        6 => EeGfxLighting::Terrain,
        7 => EeGfxLighting::MapObject,
        8 => EeGfxLighting::Unit,
        9 => EeGfxLighting::RestoreUnit,
        _ => EeGfxLighting::DimUnit,
    };
    apply_lighting(kind);
}

fn apply_lighting(kind: EeGfxLighting) {
    let e = env();
    // SAFETY: the fade's light level, gU4E9220, gU4E9210, gRenderEnvironment (read before any call)
    let input = unsafe {
        let fog = *(hp(O::RoFogColor, 0, &[]) as *const [f32; 3]);
        let sun = *(hp(O::RoSunDirection, 0, &[]) as *const GeVector);
        let fade = (*(hp(O::RoSceneFade, 0, &[]) as *const EeGfxSceneFade)).light_fade;
        EeGfxLightingInputs {
            light_fade: fade,
            fog_color: color(fog),
            orthographic: h(O::RoOrthographic, 0, &[]) != 0,
            unit_lighting: h(O::RoUnitLighting, 0, &[]) != 0,
            rear_clipping: hd(O::RoViewportRearClipping, 0, &[], &[]),
            zoom_lock: hd(O::RoZoomLock, 0, &[], &[]),
            prelit: color((*e).prelit_color),
            dim_prelit: color((*e).dim_prelit_color),
            cloud: color((*e).color54),
            terrain_ambient: color((*e).terrain_ambient_color),
            terrain_light: color((*e).terrain_light_color),
            unit_ambient: color((*e).unit_ambient_color),
            unit_light: color((*e).unit_light_color),
            terrain_ambient_intensity: (*e).terrain_ambient_intensity,
            terrain_light_intensity: (*e).terrain_light_intensity,
            unit_ambient_intensity: (*e).unit_ambient_intensity,
            unit_light_intensity: (*e).unit_light_intensity,
            sun,
        }
    };
    let ops = lighting(kind, &input);
    for op in ops.as_slice() {
        let (v, f) = (op.value as i64, &op.f[..]);
        match op.kind {
            EeGfxRasterOpKind::Fog => h(O::RoRastFog, 0, &[v]),
            EeGfxRasterOpKind::FogColor => hf(O::RoRastFogColor, 0, &[], f),
            EeGfxRasterOpKind::FogRange => hf(O::RoRastFogRange, 0, &[], f),
            EeGfxRasterOpKind::FogType => h(O::RoRastFogType, 0, &[v]),
            EeGfxRasterOpKind::AmbientColor => hf(O::RoRastAmbientColor, 0, &[], f),
            EeGfxRasterOpKind::AmbientIntensity => hf(O::RoRastAmbientIntensity, 0, &[], f),
            EeGfxRasterOpKind::RemoveLights => h(O::RoRastRemoveLights, 0, &[]),
            EeGfxRasterOpKind::Lighting => h(O::RoRastLighting, 0, &[v]),
            EeGfxRasterOpKind::LightColor => hf(O::RoLightColor, 0, &[], f),
            EeGfxRasterOpKind::LightIntensity => hf(O::RoLightIntensity, 0, &[], f),
            EeGfxRasterOpKind::LightDirection => hf(O::RoLightDirection, 0, &[], f),
            EeGfxRasterOpKind::AddLight => h(O::RoRastAddLight, 0, &[]),
            EeGfxRasterOpKind::UpdateLight => h(O::RoRastUpdateLight, 0, &[]),
        };
    }
}

// ---------------------------------------------------------------------------------------------- the raster table

/// The raster table's fog_factor: `GetFogFactor` at a point.
///
/// # Safety
/// `location` is a readable point (three floats).
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_ro_fog_factor(location: *const f32, fog: u32) -> f32 {
    // SAFETY: the function's contract
    let p = unsafe { *(location as *const Ge3DPoint) };
    super::picker::fog_factor(p, fog as c_ulong)
}

/// The raster table's depth_range: the main viewport's depth range, then the viewport set on the rasterizer.
#[no_mangle]
pub extern "C" fn ee_gamecpp_ro_depth_range(near_z: f32, far_z: f32) {
    hf(O::RoMainViewportDepthRange, 0, &[], &[near_z, far_z]);
    h(O::RoRastSetMainViewport, 0, &[]);
}

/// The raster table's screen_point: EEROUnits.cpp's GetScreenPoint.
///
/// # Safety
/// The pointers are valid (`point` three floats).
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_ro_screen_point(point: *const f32, x: *mut f32, y: *mut f32) {
    let mut out = [0.0f32; 2];
    h(O::RoScreenPoint, 0, &[point as usize as i64, ptr(&mut out)]);
    // SAFETY: the function's contract
    unsafe {
        *x = out[0];
        *y = out[1];
    }
}

/// The raster table's draw_text (EEE3DGoal::DrawPass3, 0xf13db): centred transparent Tahoma 10 at a point, clipped
/// to the viewport by the game's or the scenario editor's form.
///
/// # Safety
/// `text` is NUL-terminated.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_ro_draw_text(x: f32, y: f32, text: *const u8, red: f32, green: f32, blue: f32) {
    let mut rect = GeRectangle::default();
    h(O::RoRastViewRectangle, 0, &[ptr(&mut rect)]);
    let t = hp(O::RoTextNew, 0, &[]);
    h(O::RoTextSetTransparent, t, &[]);
    hf(O::RoTextSetColor, t, &[], &[red, green, blue]);
    h(O::RoTextSetCentred, t, &[]);
    h(O::RoTextSetText, t, &[text as usize as i64]);
    hf(O::RoTextSetLocation, t, &[], &[x, y]);
    if !crate::gamecpp::globals::get!(gFreeCamera) {
        h(O::RoMainGameDrawText, t, &[cptr(&rect)]);
    } else {
        h(O::RoEditorDrawText, t, &[cptr(&rect)]);
    }
    h(O::RoTextDelete, t, &[]);
}

/// The raster table's screen_line (EEE3DMarchingLine::DrawPass4, 0xf3c68): a GELine between two screen points, its
/// opacity per vertex.
#[no_mangle]
pub extern "C" fn ee_gamecpp_ro_screen_line(x0: f32, y0: f32, x1: f32, y1: f32, opacity0: f32, opacity1: f32) -> i32 {
    let line = super::Owned::new(O::RoGeLineNew, O::RoGeLineDelete);
    hf(O::RoGeLineStart, line.0, &[], &[x0, y0]);
    hf(O::RoGeLineEnd, line.0, &[], &[x1, y1]);
    hf(O::RoGeLineOpacity, line.0, &[], &[opacity0, opacity1]);
    h(O::RoGeLineDraw, line.0, &[]) as i32
}

/// The raster table's rear_clipping: the main viewport's rear clipping distance set, the old one answered.
#[no_mangle]
pub extern "C" fn ee_gamecpp_ro_rear_clipping(distance: f32) -> f32 {
    let old = hd(O::RoMainViewportRearClipping, 0, &[], &[]);
    hf(O::RoMainViewportSetRearClipping, 0, &[], &[distance]);
    old
}

/// EEPostGraphicsAll (`all`) / EEResetGraphicsValues: the game's render objects posted to the graphics thread, the
/// user interface's last (left out of the reset).
#[no_mangle]
pub extern "C" fn ee_gamecpp_ro_post_render_objects(all: bool) {
    h(O::RoPostClearRenderObjects, 0, &[]);
    for k in 0..if all { 6 } else { 5 } {
        h(O::RoPostPushRenderObject, 0, &[k]);
    }
}

// ---------------------------------------------------------------------------------------------- the render lists

/// The fill's host (ee_gfx fill.rs FillHost): each question a case.
struct GameFill;

impl FillHost for GameFill {
    fn mesh_z(&mut self, x: f32, y: f32) -> f32 {
        hd(O::MeshZ, 0, &[], &[x, y])
    }
    fn model_frames(&mut self, object: u64, index: u64) -> u64 {
        h(O::RoGobjFrameCount, object as P, &[index as i64]) as u64
    }
    fn water_tile(&mut self, x: c_long, y: c_long) -> bool {
        h(O::RoMeshWaterTile, 0, &[x as i64, y as i64]) != 0
    }
    fn valid_tile(&mut self, x: c_long, y: c_long) -> bool {
        h(O::MeshValidTile, 0, &[x as i64, y as i64]) != 0
    }
    fn ground_z(&mut self, point: [f32; 3]) -> f32 {
        ground_z(point)
    }
    fn physical_scale(&mut self, unit: u64) -> f32 {
        hd(O::RoUnitPhysicalScale, unit as P, &[], &[])
    }
    fn physical_frame(&mut self, unit: u64) -> c_ulong {
        h(O::RoUnitPhysicalFrame, unit as P, &[]) as c_ulong
    }
    fn tint(&mut self, unit: u64) -> EeGfxColor {
        let mut c = [0.0f32; 3];
        h(O::RoUnitTint, unit as P, &[ptr(&mut c)]);
        color(c)
    }
    fn side_part(&mut self, unit: u64) -> bool {
        h(O::RoUnitSidePart, unit as P, &[]) != 0
    }
    fn land_building(&mut self, class: u64) -> bool {
        h(O::UcIsLandBuilding, class as P, &[]) != 0
    }
    fn ground_location(&mut self, class: u64, point: [f32; 3]) -> [f32; 3] {
        let mut p = Ge3DPoint::new(point[0], point[1], point[2]);
        h(O::UcGroundLocation, class as P, &[ptr(&mut p)]);
        [p.x, p.y, p.z]
    }
    fn group_contains(&mut self, player: u64, unit: u64) -> bool {
        h(O::RoGroupContains, player as P, &[unit as i64]) != 0
    }
    fn parts(&mut self, out: *mut EeGfxFillUnit, unit: u64) {
        let (ru, un) = (out as P, unit as i64);
        let turrets = h(O::RoTurretPartCount, unit as P, &[]);
        h(O::RoUnitTurretsResize, ru, &[turrets]);
        for i in 0..turrets {
            h(O::RoUnitTurretSet, ru, &[un, i]);
        }
        let orbitals = h(O::RoOrbitalCount, unit as P, &[]);
        h(O::RoUnitOrbitalsResize, ru, &[orbitals]);
        for i in 0..orbitals {
            h(O::RoUnitOrbitalSet, ru, &[un, i]);
        }
        let wheels = h(O::RoWheelCount, unit as P, &[]);
        h(O::RoUnitWheelsResize, ru, &[wheels]);
        for i in 0..wheels {
            h(O::RoUnitWheelSet, ru, &[un, i]);
        }
    }
    fn bars_resize(&mut self, out: *mut EeGfxFillUnit, count: usize) {
        h(O::RoUnitBarsResize, out as P, &[count as i64]);
    }
    fn bars_set(&mut self, out: *mut EeGfxFillUnit, index: usize, value: f32) {
        hf(O::RoUnitBarSet, out as P, &[index as i64], &[value]);
    }
    fn bars_clear(&mut self, out: *mut EeGfxFillUnit) {
        h(O::RoUnitBarsClear, out as P, &[]);
    }
    fn name_clear(&mut self, out: *mut EeGfxFillUnit) {
        h(O::RoUnitNameClear, out as P, &[]);
    }
    fn name_from_stats(&mut self, out: *mut EeGfxFillUnit, unit: u64) {
        h(O::RoUnitNameFromStats, out as P, &[unit as i64]);
    }
    fn select(&mut self, out: *mut EeGfxFillUnit) {
        super::ro_lists::select_unit(out as usize);
    }
    fn point_at(&mut self, out: *mut EeGfxFillUnit) {
        super::ro_lists::set_selected_unit(out as usize);
    }
    fn label(&mut self, out: *mut EeGfxFillUnit, player: u64) {
        let player = player as P;
        h(O::RoSetLabelUnit, out as P, &[]);
        if h(O::PlIsHuman, player, &[]) == 0 && h(O::RoCivilizationUnchanged, player, &[]) == 0 {
            h(O::RoLabelCivilizationName, player, &[]);
        } else {
            h(O::RoLabelPlayerName, player, &[]);
        }
    }
    fn morale(&mut self, unit: u64) -> EeGfxFillMorale {
        let unit = unit as P;
        let mut morale = EeGfxFillMorale::default();
        for i in 0..h(O::RoUnitAreaEffectCount, unit, &[]) {
            if RO_AE_MORALE.contains(&h(O::RoUnitAreaEffectKind, unit, &[i])) {
                morale.found = true;
                morale.strength = h(O::RoUnitAreaEffectStrength, unit, &[i]) as u32;
                break;
            }
        }
        morale
    }
    fn scaffolding(&mut self, player: c_long, class: u64) -> u64 {
        h(O::RoScaffoldingModel, class as P, &[player as i64]) as u64
    }
    fn unit(&mut self, id: c_long) -> u64 {
        h(O::RoWorldUnit, 0, &[id as i64]) as u64
    }
    fn waypoint(&mut self, carrier: u64, x: c_long, y: c_long) -> EeGfxFillPoint {
        let mut point = EeGfxFillPoint::default();
        let mut waypoint: P = 0;
        if h(O::RoFindWaypoint, carrier as P, &[x as i64, y as i64, ptr(&mut waypoint)]) != 0 {
            let mut location = Ge3DPoint::default();
            h(O::RoWaypointLocation, waypoint, &[ptr(&mut location)]);
            point.found = true;
            point.point = [location.x, location.y, location.z];
        }
        point
    }
}

/// `theMap->GetGroundPoint(point)`'s z.
fn ground_z(point: [f32; 3]) -> f32 {
    let p = Ge3DPoint::new(point[0], point[1], point[2]);
    let mut ground = Ge3DPoint::default();
    h(O::MapGroundPoint, 0, &[cptr(&p), ptr(&mut ground)]);
    ground.z
}

/// The fill's frame (EEFillFrame): the frame state and the render globals.
fn fill_frame(info: &FrameInfo) -> EeGfxFillFrame {
    let mut picked: *const c_ulong = std::ptr::null();
    let picked_count = h(O::RoPickedIds, 0, &[ptr(&mut picked)]) as usize;
    // SAFETY: gRenderEnvironment
    let sea_level = unsafe { (*env()).sea_level };
    EeGfxFillFrame {
        yaw: info.yaw,
        free_camera: crate::gamecpp::globals::get!(gFreeCamera),
        sway_clock: SWAY_CLOCK.load(Relaxed),
        player: h(O::WorldCurrentPlayer, 0, &[]) as u64,
        map: h(O::WMap, world(), &[]) as u64,
        picked: if picked_count == 0 { std::ptr::null() } else { picked },
        picked_count,
        world_player: h(O::WorldPlayer, 0, &[0]) as u64,
        pointed: info.picked_unit as u64,
        music_follows_battle: h(O::RoMusicFollowsBattle, 0, &[]) != 0,
        collision_spheres: h(O::RoCollisionSphereMode, 0, &[]) != 0,
        sea_level,
        space_turret_height: hd(O::RoSpaceTurretHeight, 0, &[], &[]),
        space_station_height: hd(O::RoSpaceStationHeight, 0, &[], &[]),
    }
}

/// A unit's opacity this frame (EERenderObject::GetUnitOpacity, the C++'s); whether it is drawn translucent.
fn unit_opacity(unit: P, opacity: *mut f32) -> bool {
    // SAFETY: gRenderEnvironment
    let show_hidden = unsafe { (*env()).show_hidden_units };
    super::ro_raster::unit_opacity(unit, show_hidden as c_long, opacity)
}

/// A render map's insert: the units' (`objects` false) or the map objects' map `k` (0..2) by texture.
fn insert(objects: bool, k: i64, texture: P, item: P) {
    super::ro_lists::insert(objects, k, texture, item);
}

/// EERenderObject::FillUnit: a render unit for a unit, filed in the picker and the texture maps (the ambient
/// counters, the media, the priest tower's circle in the original's order; ee_gfx fill.rs makes every read and field
/// write between).
fn fill_unit(unit: P, out: P, map: i64, info: &mut FrameInfo) {
    let complex = hp(O::UnGetComplex, unit, &[]);
    if complex != 0 {
        if info.ambient {
            h(O::RoAmbientCountUnit, complex, &[]);
        }
    } else if info.ambient {
        h(O::RoAmbientCountTree, unit, &[]);
    }
    let head = out as *mut EeGfxFillUnit;
    let source = hp(O::UnGetModel, unit, &[]);
    // SAFETY: the render unit's head
    unsafe { (*head).source = source as u64 };
    h(O::GobjLoadMedia, source, &[]);
    let frame = fill_frame(info);
    // SAFETY: the graphics thread under the world's read lock; `unit` is live, `head` the render unit's head
    let result = unsafe {
        let t = WorldRead::begin();
        let view = UnitView::of(&t, unit as u64).expect("a unit");
        fill_unit_fields(&t, view, head, &frame, &mut GameFill)
    };
    info.fighting += result.fighting;
    if result.circle {
        // SAFETY: the complex unit's physical model's location
        let location = unsafe { *(hp(O::RoComplexLocation, complex, &[]) as *const GeTransformation) };
        draw_ground_circle(&location, hd(O::RoUnitStatsRange, unit, &[], &[]), 2);
    }
    // SAFETY: the render unit's head (the fill is done)
    let (source2, opacity, animation) = unsafe { ((*head).source2 as P, (*head).opacity, (*head).animation) };
    if source2 != 0 {
        h(O::GobjLoadMedia, source2, &[]);
    }
    // SAFETY: as above
    hf(O::RoPickerAdd, unit, &[cptr(unsafe { &(*head).transformation })], &[opacity]);
    insert(false, map, hp(O::RoGobjTexture, source, &[animation as i64]), out);
    if complex != 0 && h(O::RoComplexIsFoundation, complex, &[]) == 0 {
        insert(false, 2, hp(O::RoGobjShadowTexture, source, &[]), out);
    }
}

/// EERenderObject::FillObject: a render object for a map object (a class with mIsTree), filed by texture, in the
/// picker and in the shadow lists (ee_gfx fill.rs makes the reads and field writes).
fn fill_object(unit: P, out: P, map: i64, info: &mut FrameInfo) {
    let mut transformation = GeTransformation::default();
    if info.ambient {
        h(O::RoAmbientCountTree, unit, &[]);
    }
    let head = out as *mut EeGfxFillObject;
    let object = hp(O::UnGetModel, unit, &[]);
    // SAFETY: the render object
    unsafe { (*head).object = object as u64 };
    h(O::GobjLoadMedia, object, &[]);
    let frame = fill_frame(info);
    // SAFETY: as fill_unit
    let selected = unsafe {
        let t = WorldRead::begin();
        let view = UnitView::of(&t, unit as u64).expect("a unit");
        fill_object_fields(&t, view, &mut *head, &frame, &mut transformation, &mut GameFill)
    };
    if selected {
        super::ro_lists::select_object(out);
    }
    // SAFETY: the render object (filled)
    let (index, opacity) = unsafe { ((*head).model_index, (*head).opacity) };
    insert(true, map, hp(O::RoGobjTexture, object, &[index as i64]), out);
    hf(O::RoPickerAdd, unit, &[cptr(&transformation)], &[opacity]);
    insert(true, 2, hp(O::RoGobjShadowTexture, object, &[]), out);
}

/// GetTileHasTree: whether any unit on a tile is a map object (its class has mIsTree).
fn tile_has_tree(tile: P) -> bool {
    let mut result = false;
    if tile != 0 {
        let mut node = hp(O::RoTileFirstNode, tile, &[]);
        while node != 0 && !result {
            result = h(O::RoNodeUnitIsTree, node, &[]) != 0;
            node = hp(O::RoNodeNext, node, &[]);
        }
    }
    result
}

/// EERenderObject::FillDoppelgangerObject: a render object for a remembered map object on a tile the current player
/// cannot see, when the pool has room and the model is in the view.
fn fill_doppelganger_object(d: P, out: P, count: *mut c_ulong, x: c_long, y: c_long, info: &FrameInfo) {
    let head = out as *mut EeGfxFillObject;
    let unit = hp(O::RoDoppelUnit, d, &[]);
    let class = hp(O::UnClass, unit, &[]);
    let scale = hd(O::RoClassScale, class, &[], &[]);
    let id = h(O::RoUnitId, unit, &[]) as i32;
    let size = h(O::RoClassTileSize, class, &[]) as i32;
    let place = capture::object_place(id, size, x as i32, y as i32);
    let z = hd(O::MeshZ, 0, &[], &[place.0, place.1]);
    let t = placed(info.yaw, place.0, place.1, z);
    let model = hp(O::RoGobjModel, hp(O::RoDoppelModel, d, &[]), &[h(O::RoDoppelAnimation, d, &[])]);
    // SAFETY: the pool's count
    if unsafe { *count } >= RO_OBJECT_POOL || !model_in_view(model, &t, scale) {
        return;
    }
    let maximum = h(O::RoDoppelMaxHitPoints, d, &[]) as c_long;
    let hit_points = h(O::RoDoppelHitPoints, d, &[]) as c_long;
    let object = hp(O::RoDoppelModel, d, &[]);
    // SAFETY: the render object
    unsafe {
        (*head).location = [place.0, place.1, z];
        (*head).model_scale = scale;
        (*head).health = hit_points as f32 / maximum as f32;
        (*head).is_fogged = true;
        (*head).opacity = 1.0;
        (*head).model_scale = capture::object_scale(h(O::RoUnitId, unit, &[]) as i32) * scale;
        (*head).object = object as u64;
    }
    h(O::GobjLoadMedia, object, &[]);
    let shadowed = !tile_has_tree(hp(O::MapTile, 0, &[x as i64 - 1, y as i64 - 1]))
        || !tile_has_tree(hp(O::MapTile, 0, &[x as i64 + 1, y as i64 + 1]));
    let (shadow_scale, f1c) = if shadowed {
        (hd(O::RoClassShadowScale, class, &[], &[]), hd(O::RoClassF170, class, &[], &[]))
    } else {
        (0.0, 0.0)
    };
    let index = h(O::RoDoppelAnimation, d, &[]) as c_ulong;
    let frame = h(O::RoDoppelFrame, d, &[]) as c_ulong;
    let frames = h(O::RoGobjFrameCount, object, &[index as i64]) as c_ulong;
    // SAFETY: the render object
    unsafe {
        (*head).scale = shadow_scale;
        (*head).f1c = f1c;
        (*head).model_index = index;
        (*head).current_frame = frame;
        (*head).next_frame = frame;
        if frame.wrapping_add(1) < frames {
            (*head).current_frame = frame.wrapping_add(1);
            (*head).fraction = capture::sway_fraction(SWAY_CLOCK.load(Relaxed), h(O::RoUnitId, unit, &[]) as i32);
        } else {
            (*head).fraction = 0.0;
        }
    }
    if info.ambient {
        h(O::RoAmbientCountTree, unit, &[]);
    }
    insert(true, 0, hp(O::RoGobjTexture, object, &[0]), out);
    hf(O::RoPickerAdd, unit, &[cptr(&t)], &[1.0]);
    insert(true, 2, hp(O::RoGobjShadowTexture, object, &[]), out);
    // SAFETY: the pool's count
    unsafe { *count += 1 };
}

/// EERenderObject::FillDoppelgangerUnit: a render unit for a remembered unit on a tile the current player cannot
/// see, when the pool has room and the model is in the view: the remembered location (or the tile's centre on the
/// ground), the class's orientation flag and the model; the render unit's fields, the ground height (0 over water) and
/// the build progress scale.
fn fill_doppelganger_unit(d: P, out: P, count: *mut c_ulong, map: i64, x: c_long, y: c_long, info: &FrameInfo) {
    let head = out as *mut EeGfxFillUnit;
    let unit = hp(O::RoDoppelUnit, d, &[]);
    let class = hp(O::UnClass, unit, &[]);
    let scale = hd(O::RoDoppelScale, d, &[], &[]);
    let mut t = GeTransformation::default();
    if h(O::RoDoppelIsComplex, d, &[]) != 0 {
        h(O::RoDoppelTransformation, d, &[ptr(&mut t)]);
    } else {
        t = GeTransformation::IDENTITY;
        let size = h(O::RoClassTileSize, class, &[]) as c_long as f32;
        let px = size * 0.5 + x as f32;
        let py = size * 0.5 + y as f32;
        let pz = hd(O::MeshZ, 0, &[], &[px, py]);
        set_position(&mut t, px, py, pz);
    }
    if h(O::RoClassFacesCamera, class, &[]) != 0 {
        t.set_orientation(RotationOrder::Ypr, info.yaw, 0.0, 0.0);
    }
    if h(O::RoClassFacesCamera, class, &[]) != 0 {
        t.set_orientation(RotationOrder::Ypr, info.yaw, 0.0, 0.0);
    }
    let model = hp(O::RoGobjModel, hp(O::RoDoppelModel, d, &[]), &[h(O::RoDoppelAnimation, d, &[])]);
    // SAFETY: the pool's count
    if unsafe { *count } >= RO_UNIT_POOL || !model_in_view(model, &t, scale) {
        return;
    }
    let foundation = h(O::RoDoppelIsFoundation, d, &[]) != 0;
    let shadow_scale = if !foundation { hd(O::RoClassShadowScale, class, &[], &[]) } else { 0.0 };
    let ff4 = hd(O::RoClassF170, class, &[], &[]);
    // SAFETY: the render unit's head
    unsafe {
        (*head).info = class as u64;
        (*head).category = h(O::RoClassCategory, class, &[]) as c_long;
        (*head).unit_class_index = h(O::RoClassUnitType, class, &[]) as c_ulong;
        (*head).l118 = h(O::RoClassFamily, class, &[]) as c_long;
        (*head).shadow_scale = shadow_scale;
        (*head).ff4 = ff4;
    }
    h(O::RoUnitNameClear, out, &[]);
    let mut player_color = [0.0f32; 3];
    h(O::RoDoppelPlayerColor, d, &[ptr(&mut player_color)]);
    let source = hp(O::RoDoppelModel, d, &[]);
    // SAFETY: as above
    unsafe {
        (*head).source2 = 0;
        (*head).player_color = player_color;
        (*head).area_effect_strength = 0;
        (*head).scale = scale;
        (*head).opacity = 1.0;
        (*head).is_tinted = false;
        (*head).has_sub_model_offset = false;
        (*head).grid_size = h(O::RoClassTileSize, class, &[]) as c_long;
        (*head).is_fogged = true;
        (*head).source = source as u64;
    }
    if h(O::RoDoppelIsBuilt, d, &[]) == 0 && h(O::RoDoppelHitPoints, d, &[]) as c_long > 1 {
        // SAFETY: as above (the pool slot's player id, as the original)
        let player_id = unsafe { (*head).player_id };
        let source2 = hp(O::RoScaffoldingModel, class, &[player_id as i64]);
        // SAFETY: as above
        unsafe { (*head).source2 = source2 as u64 };
        if source2 != 0 {
            h(O::GobjLoadMedia, source2, &[]);
        }
    }
    h(O::GobjLoadMedia, source, &[]);
    let water = h(O::RoMeshWaterTile, 0, &[um::floor(t.x()) as i64, um::floor(t.y()) as i64]) != 0;
    let ground = if water && t.z() >= 0.0 { 0.0 } else { ground_z([t.x(), t.y(), t.z()]) };
    if h(O::RoDoppelIsBuilt, d, &[]) == 0 && h(O::RoDoppelHitPoints, d, &[]) as c_long != 1 {
        let maximum = h(O::RoDoppelMaxHitPoints, d, &[]) as c_long;
        let hit_points = h(O::RoDoppelHitPoints, d, &[]) as c_long;
        t.scale(1.0, 1.0, hit_points as f32 / maximum as f32);
    }
    let frame = h(O::RoDoppelFrame, d, &[]) as c_ulong;
    let animation = h(O::RoDoppelAnimation, d, &[]) as c_long;
    // SAFETY: as above
    unsafe {
        (*head).transformation = t;
        (*head).ground_z = ground;
        (*head).current_frame = frame;
        (*head).next_frame = frame;
        (*head).fraction = 0.0;
        (*head).animation = animation;
        (*head).command_type = 0;
    }
    // SAFETY: as above
    hf(O::RoPickerAdd, unit, &[cptr(unsafe { &(*head).transformation })], &[1.0]);
    insert(false, map, hp(O::RoGobjTexture, source, &[0]), out);
    insert(false, 2, hp(O::RoGobjShadowTexture, source, &[]), out);
    // SAFETY: the pool's count
    unsafe { *count += 1 };
}

/// EERenderObject::FillGhostRecords: a render record for every ghost unit, when the ghost units changed.
fn fill_ghost_records() {
    let record = super::Owned::new(O::RoGhostRecordNew, O::RoGhostRecordDelete);
    let head = record.0 as *mut EeGfxFillUnit;
    h(O::RoGhostLock, 0, &[]);
    if h(O::RoGhostUnitsChanged, 0, &[]) != 0 {
        for i in 0..h(O::RoGhostCount, 0, &[]) {
            let ghost = hp(O::RoGhostAt, 0, &[i]);
            let source = hp(O::RoGhostModel, ghost, &[]);
            // SAFETY: the record's unit head
            unsafe { (*head).source = source as u64 };
            h(O::GobjLoadMedia, source, &[]);
            let class = hp(O::RoGhostClass, ghost, &[]);
            let mut player_color = [0.0f32; 3];
            h(O::RoGhostPlayerColor, ghost, &[ptr(&mut player_color)]);
            let mut t = GeTransformation::default();
            h(O::RoGhostTransformation, ghost, &[ptr(&mut t)]);
            t.m[2][3] += 0.001;
            let unit_type = h(O::RoClassUnitType, class, &[]);
            if unit_type == RO_TYPE_SPACE_TURRET {
                t.m[2][3] += hd(O::RoSpaceTurretHeight, 0, &[], &[]);
            } else if unit_type == RO_TYPE_ORBITAL_STATION {
                t.m[2][3] += hd(O::RoSpaceStationHeight, 0, &[], &[]);
            }
            let frame = h(O::RoGhostFrame, ghost, &[]) as c_ulong;
            // SAFETY: as above
            unsafe {
                (*head).fraction = 0.0;
                (*head).current_frame = frame;
                (*head).next_frame = frame;
                (*head).animation = h(O::RoGhostAnimation, ghost, &[]) as c_long;
                (*head).source2 = 0;
                (*head).is_fogged = false;
                (*head).scale = hd(O::RoGhostScale, ghost, &[], &[]);
                (*head).can_place = h(O::RoGhostCanPlace, ghost, &[]) != 0;
                (*head).grid_size = h(O::RoClassTileSize, class, &[]) as c_long;
                (*head).player_color = player_color;
                (*head).info = class as u64;
                (*head).category = h(O::RoClassCategory, class, &[]) as c_long;
                (*head).is_tinted = false;
                (*head).has_sub_model_offset = false;
                (*head).transformation = t;
            }
            h(O::RoGhostRecordCopy, record.0, &[pa(ghost)]);
            h(O::RoGhostRecordsPush, record.0, &[]);
        }
    }
    h(O::RoGhostUnlock, 0, &[]);
}

/// The unit picker's and the drawn set's questions of IsUnitDrawn (ee_gfx fill.rs, walk.rs).
struct DrawnAsk<'a> {
    unit: P,
    drawn: &'a mut BTreeSet<P>,
}

extern "C" fn drawn_first(ask: *mut c_void) -> bool {
    // SAFETY: the DrawnAsk of is_unit_drawn's call
    let ask = unsafe { &mut *(ask as *mut DrawnAsk) };
    ask.drawn.insert(ask.unit)
}

extern "C" fn drawn_complex(ask: *mut c_void) -> bool {
    // SAFETY: as drawn_first
    let unit = unsafe { (*(ask as *mut DrawnAsk)).unit };
    h(O::RoComplexInView, unit, &[]) != 0
}

extern "C" fn drawn_model(ask: *mut c_void, x: f32, y: f32) -> bool {
    // SAFETY: as drawn_first
    let unit = unsafe { (*(ask as *mut DrawnAsk)).unit };
    let z = hd(O::MeshZ, 0, &[], &[x, y]);
    // SAFETY: gRenderEnvironment
    let yaw = unsafe { (*env()).camera_yaw };
    let t = placed(yaw, x, y, z);
    let model = hp(O::RoGobjModel, hp(O::UnGetModel, unit, &[]), &[0]);
    model_in_view(model, &t, hd(O::RoUnitStatsScale, unit, &[], &[]))
}

/// EERenderObject::IsUnitDrawn: the unit is seen by the current player (or the free camera) and in the view; a unit
/// whose footprint spans several tiles counts once (`drawn`).
fn is_unit_drawn(unit: P, tile: P, drawn: &mut BTreeSet<P>, player: P, free_camera: bool) -> bool {
    let mut ask = DrawnAsk { unit, drawn };
    let mut host = EeGfxDrawnHost {
        context: &mut ask as *mut DrawnAsk as *mut c_void,
        first_drawn: drawn_first,
        complex_in_view: drawn_complex,
        model_in_view: drawn_model,
    };
    // SAFETY: the counts; the graphics thread under the world's read lock, the unit, tile and player live
    unsafe {
        let (units, objects) = (*unit_count() as u32, *object_count() as u32);
        ee_render::gfx::fill::ee_gfx_fill_unit_drawn(
            unit as u64,
            tile as u64,
            player as u64,
            free_camera,
            units,
            objects,
            &mut host,
        )
    }
}

/// EERenderObject::FillRenderLists: the frame's render lists: the pools reset, the units picked, the visible tiles
/// walked (units, map objects, remembered ones), the ghost units, then the music intensity and the ambient sounds.
#[no_mangle]
pub extern "C" fn ee_gamecpp_ro_fill_render_lists(ro: *mut c_void) {
    let mut drawn: BTreeSet<P> = BTreeSet::new();
    let mut doppelgangers: BTreeSet<P> = BTreeSet::new();
    let player = hp(O::WorldCurrentPlayer, 0, &[]);
    let selection = hp(O::RoMainSelectUnits, 0, &[]);
    let mut info = FrameInfo { picked_unit: 0, yaw: 0.0, fighting: 0.0, frame: 0, ambient: false };
    let mut count = 0.0f32;
    let time = now();
    if flag(O::RoFlag529068) {
        return;
    }
    {
        let (mut clock, mut last) = (SWAY_CLOCK.load(Relaxed), SWAY_LAST.load(Relaxed));
        capture::sway_tick(&mut clock, &mut last, time as u32);
        SWAY_CLOCK.store(clock, Relaxed);
        SWAY_LAST.store(last, Relaxed);
    }
    ee_gamecpp_ro_fn10e8d1(ro);
    unload_render_objects();
    unload_render_units();
    for i in 0..h(O::RoLinesCount, 0, &[]) {
        h(O::RoLineDereference, 0, &[i]);
    }
    h(O::RoLinesClear, 0, &[]);
    clear_lists();
    unload_ghost_records();
    h(O::RoSetLabelUnit, 0, &[]);
    let free_camera = crate::gamecpp::globals::get!(gFreeCamera);
    if !free_camera {
        if h(O::RoSelectionIsDrawn, selection, &[]) != 0 {
            h(O::RoPickUnits, selection, &[pa(player)]);
        } else {
            h(O::RoPickedClear, 0, &[]);
        }
    } else {
        let editor = hp(O::RoEditorSelectUnits, 0, &[]);
        let trigger = hp(O::RoTriggerSelectUnits, 0, &[]);
        if editor != 0 && h(O::RoSelectionIsDrawn, editor, &[]) != 0 {
            h(O::RoPickUnitsInRectangle, editor, &[]);
        } else if trigger != 0 && h(O::RoTriggerSelectionIsDrawn, trigger, &[]) != 0 {
            h(O::RoPickTriggerUnitsInRectangle, trigger, &[]);
        } else {
            h(O::RoPickedClear, 0, &[]);
        }
    }
    info.fighting = 0.0;
    if !free_camera {
        h(O::RoAmbientFrameCountIncrement, 0, &[]);
        info.ambient = h(O::RoAmbientUpdateDue, 0, &[]) != 0;
    } else {
        info.ambient = false;
    }
    let direction = camera_direction();
    info.yaw = um::um_arctan2(-direction.x, direction.y) - RO_YAW_OFFSET;
    {
        let ticks = if flag(O::RoFlag529065) { None } else { Some(h(O::RoPhysicsTicks, 0, &[]) as c_ulong) };
        let mut counter = FRAME_COUNTER.lock().unwrap();
        *counter = ticks.unwrap_or(*counter).wrapping_add(1);
        info.frame = *counter;
    }
    // port (R5a.3): no reference: the unit is only compared and read while the world's lock is held
    info.picked_unit = if !free_camera { hp(O::RoPickUnitUnreferenced, 0, &[]) } else { 0 };
    if info.picked_unit != 0 && h(O::RoUnitHitPoints, info.picked_unit, &[]) as c_long <= 0 {
        info.picked_unit = 0;
    }
    h(O::PickerLock, 0, &[]);
    h(O::PickerReset, 0, &[]);
    let (units, objects) = (unit_count(), object_count());
    let rows = h(O::RoVisibleRowCount, 0, &[]) as usize;
    for row in 0..rows {
        let [y, first, last] = visible_row(row);
        for x in first..=last {
            let tile = hp(O::MapTile, 0, &[x as i64, y as i64]);
            if h(O::TileVisibleTo, tile, &[pa(player)]) == 0 && !free_camera {
                continue;
            }
            let mut node = hp(O::RoTileFirstNode, tile, &[]);
            while node != 0 {
                let mut unit = hp(O::RoNodeUnit, node, &[]);
                if hp(O::UnComplex, unit, &[]) != 0 {
                    unit = hp(O::UnComplex, unit, &[]);
                    while hp(O::CxPreviousPart, unit, &[]) != 0 {
                        unit = hp(O::CxPreviousPart, unit, &[]);
                    }
                }
                loop {
                    if is_unit_drawn(unit, tile, &mut drawn, player, free_camera)
                        && h(O::RoPickerContains, unit, &[]) == 0
                    {
                        if h(O::RoUnitIsTree, unit, &[]) == 0 {
                            if hp(O::UnComplex, unit, &[]) != 0 {
                                count += 1.0;
                            }
                            // SAFETY: the pool's count
                            let out = hp(O::RoRenderUnit, 0, &[unsafe { *units } as i64]);
                            // SAFETY: the render unit's head
                            let translucent =
                                unit_opacity(unit, unsafe { &mut (*(out as *mut EeGfxFillUnit)).opacity });
                            fill_unit(unit, out, translucent as i64, &mut info);
                            // SAFETY: as above
                            unsafe { *units += 1 };
                        } else {
                            // SAFETY: the pool's count
                            let out = hp(O::RoRenderObject, 0, &[unsafe { *objects } as i64]);
                            // SAFETY: the render object
                            let translucent =
                                unit_opacity(unit, unsafe { &mut (*(out as *mut EeGfxFillObject)).opacity });
                            fill_object(unit, out, translucent as i64, &mut info);
                            // SAFETY: as above
                            unsafe { *objects += 1 };
                        }
                    }
                    unit = if hp(O::UnComplex, unit, &[]) != 0 { hp(O::RoUnitNextPart, unit, &[]) } else { 0 };
                    if unit == 0 {
                        break;
                    }
                }
                node = hp(O::RoNodeNext, node, &[]);
            }
            if h(O::RoTileSeenBy, tile, &[pa(player)]) == 0 && !free_camera {
                for k in 0..h(O::RoTileDoppelgangerCount, tile, &[]) {
                    let d = hp(O::RoTileDoppelganger, tile, &[k]);
                    if hp(O::RoDoppelPlayerMember, d, &[]) != player {
                        continue;
                    }
                    let unit = hp(O::RoDoppelUnit, d, &[]);
                    if hp(O::UnTileNode, unit, &[]) != 0
                        && h(O::RoUnitComplexIsFoundation, unit, &[]) != 0
                        && hp(O::RoDoppelPlayer, d, &[]) != player
                    {
                        continue;
                    }
                    if h(O::RoUnitTileSize, unit, &[]) > 1 && !doppelgangers.insert(d) {
                        continue;
                    }
                    if h(O::RoUnitIsTree, unit, &[]) == 0 {
                        // SAFETY: the pool's count
                        let out = hp(O::RoRenderUnit, 0, &[unsafe { *units } as i64]);
                        let map = (h(O::RoUnitIsObstacle, unit, &[]) != 0) as i64;
                        fill_doppelganger_unit(d, out, units, map, x, y, &info);
                    } else {
                        // SAFETY: the pool's count
                        let out = hp(O::RoRenderObject, 0, &[unsafe { *objects } as i64]);
                        fill_doppelganger_object(d, out, objects, x, y, &info);
                    }
                }
            }
        }
    }
    h(O::PickerUnlock, 0, &[]);
    fill_ghost_records();
    // port (R5a.3): what EEROUnits::DrawUnitModel read from the live unit after the lock was released, read here
    // with the same lookup (a pure one: the player's id table); the doppelgangers' render units keep the id their pool
    // slot had, as before
    // SAFETY: the pool's count
    for i in 0..unsafe { *units } {
        let head = hp(O::RoRenderUnit, 0, &[i as i64]) as *mut EeGfxFillUnit;
        // SAFETY: the render unit's head
        let (class_index, id) = unsafe { ((*head).unit_class_index, (*head).unit_id) };
        let second = class_index == RO_TYPE_ROBOTIC_FARM && {
            let live = hp(O::RoWorldUnit, 0, &[id as i64]);
            live == 0 || h(O::RoUnitHitPointsIsOne, live, &[]) != 0
        };
        // SAFETY: as above
        unsafe { (*head).second_full_opacity = second };
    }
    // RFIL (TEST ONLY)
    super::ro_views::rc_fill(
        info.picked_unit,
        info.yaw,
        info.fighting,
        info.frame as u32,
        info.ambient,
        SWAY_CLOCK.load(Relaxed),
        count,
    );
    set_flag(O::RoFlag529068, true);
    let music_due = {
        let last = *MUSIC_LAST.lock().unwrap();
        h(O::RoMusicFollowsBattle, 0, &[]) != 0 && time.wrapping_sub(last) >= RO_MUSIC_PERIOD
    };
    if music_due {
        *MUSIC_LAST.lock().unwrap() = time;
        let current = h(O::RoMusicIntensity, 0, &[]) as u32;
        h(O::MusicSetIntensity, 0, &[ee_render::gfx::walk::music_intensity(current, info.fighting, count) as i64]);
    }
    if info.ambient {
        h(O::RoAmbientUpdate, 0, &[]);
    }
}

/// EERenderObject::DrawGroundCircle: a 50-segment ground circle of a radius around a location queued as a 3D line
/// with the area-effect texture (mode 2 only).
fn draw_ground_circle(location: &GeTransformation, radius: f32, mode: c_long) {
    if mode == 1 || mode != 2 {
        return;
    }
    h(O::RoLinesPushBack, 0, &[]);
    let line = hp(O::RoLinesBack, 0, &[]);
    h(O::RoLineSetWidth, line, &[]);
    h(O::RoLineSetTexture, line, &[]);
    h(O::RoAreaEffectTextureReference, 0, &[]);
    // the 50 points on the ground and the first again, the strip's width: ee_gfx capture.rs
    let mut points = [Ge3DPoint::default(); RO_RING_POINTS];
    let ground = ee_render::gfx::effect_obj::ee_gfx_particle_ground();
    // SAFETY: the ground's answers and room for the ring's points
    let width = unsafe {
        ee_render::gfx::ffi_scene::ee_gfx_capture_ground_circle(
            &ground,
            location.x(),
            location.y(),
            radius,
            points.as_mut_ptr(),
        )
    };
    h(O::RoLinePointsResize, line, &[RO_RING_POINTS as i64]);
    for (i, p) in points.iter().enumerate() {
        hf(O::RoLinePointSet, line, &[i as i64, cptr(p)], &[width]);
    }
}

/// RFIL's world key (TEST ONLY; EERenderCapture.cpp's WorldKey): the fill's world as far as two runs can differ in it,
/// FNV-1a over the current player and its viewed units, every tile of the visible rows (its sight and remembered
/// units) and every unit on them (its id, hit points, resource, and a complex unit's location, frame, animation,
/// command, build state and selection).
#[no_mangle]
pub extern "C" fn ee_gamecpp_ro_world_key() -> u64 {
    let mut key = Fnv::new();
    let player = hp(O::WorldCurrentPlayer, 0, &[]);
    key.add(h(O::RoPlayerId, player, &[]) as u32);
    key.add(h(O::RoViewedUnitId, player, &[]) as u32);
    key.add(h(O::RoSelectedViewedUnitId, player, &[]) as u32);
    for row in 0..h(O::RoVisibleRowCount, 0, &[]) as usize {
        let [y, first, last] = visible_row(row);
        for x in first..=last {
            let tile = hp(O::MapTile, 0, &[x as i64, y as i64]);
            key.add(x as u32);
            key.add(y as u32);
            let visible = h(O::TileVisibleTo, tile, &[pa(player)]) != 0;
            let seen = h(O::RoTileSeenBy, tile, &[pa(player)]) != 0;
            key.add(visible as u32 | (seen as u32) << 1);
            key.add(h(O::RoTileDoppelgangerCount, tile, &[]) as u32);
            let mut node = hp(O::RoTileFirstNode, tile, &[]);
            while node != 0 {
                let unit = hp(O::RoNodeUnit, node, &[]);
                key.add(h(O::RoUnitId, unit, &[]) as u32);
                key.add(h(O::RoUnitHitPointsValue, unit, &[]) as u32);
                key.add(h(O::RoUnitResourceAmount, unit, &[]) as u32);
                let complex = hp(O::UnComplex, unit, &[]);
                if complex != 0 {
                    // SAFETY: the complex unit's physical model's location (sixteen floats)
                    let location = unsafe { *(hp(O::RoComplexLocation, complex, &[]) as *const GeTransformation) };
                    for row in location.m {
                        for v in row {
                            key.bytes(&v.to_le_bytes());
                        }
                    }
                    key.add(h(O::RoCxFrame, complex, &[]) as u32);
                    key.add(h(O::RoCxNextFrame, complex, &[]) as u32);
                    key.bytes(&hd(O::RoCxAnimationFraction, complex, &[], &[]).to_le_bytes());
                    key.add(h(O::RoCxAnimation, complex, &[]) as u32);
                    key.add(h(O::RoCxCommandType, complex, &[]) as u32);
                    let built = h(O::RoCxIsBuilt, complex, &[]) != 0;
                    let selected = h(O::RoGroupContainsComplex, player, &[pa(complex)]) != 0;
                    key.add(built as u32 | (selected as u32) << 1);
                }
                node = hp(O::RoNodeNext, node, &[]);
            }
        }
    }
    key.0
}

/// FNV-1a, 64 bits (RFIL's world key).
struct Fnv(u64);

impl Fnv {
    fn new() -> Fnv {
        Fnv(14695981039346656037)
    }
    fn bytes(&mut self, data: &[u8]) {
        for b in data {
            self.0 ^= *b as u64;
            self.0 = self.0.wrapping_mul(1099511628211);
        }
    }
    fn add(&mut self, value: u32) {
        self.bytes(&value.to_le_bytes());
    }
}

// the member functions' forwarders (EERenderObject.cpp)

/// EERenderObject::FillUnit et al. are FillRenderLists' own; the scene's pieces the C++ still names.
#[no_mangle]
pub extern "C" fn ee_gamecpp_ro_light_fade(down: bool, frames: c_ulong) {
    // SAFETY: the fade state
    unsafe { scene::start_fade(&mut *(hp(O::RoSceneFade, 0, &[]) as *mut EeGfxSceneFade), down, frames as u32) }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::mem::{offset_of, size_of};

    #[test]
    fn the_environment_mirrors_the_render_environment() {
        // ee/EERenderLists.h's offsets (the 32-bit long of the Windows build; LP64 moves the tail by four)
        assert_eq!(offset_of!(RenderEnv, terrain_ambient_color), 0x0c);
        assert_eq!(offset_of!(RenderEnv, color54), 0x54);
        assert_eq!(offset_of!(RenderEnv, unit_ambient_intensity), 0x60);
        assert_eq!(offset_of!(RenderEnv, camera_yaw), 0x84);
        assert_eq!(offset_of!(RenderEnv, show_hidden_units), 0x88);
        assert_eq!(offset_of!(RenderEnv, time_of_day_changed), 0x89 + size_of::<c_long>());
    }

    #[test]
    fn the_frame_info_and_texture_description_mirror_the_cpp() {
        assert_eq!(offset_of!(FrameInfo, yaw), size_of::<usize>());
        assert_eq!(offset_of!(FrameInfo, frame), size_of::<usize>() + 8);
        assert_eq!(offset_of!(FrameInfo, ambient), size_of::<usize>() + 8 + size_of::<c_ulong>());
        assert_eq!(offset_of!(TextureDescription, is_3d_renderable), 4 * size_of::<c_long>());
        assert_eq!(offset_of!(TextureDescription, use_texture_pack), 4 * size_of::<c_long>() + 4);
    }

    #[test]
    fn a_placed_model_faces_the_yaw_at_its_point() {
        let t = placed(0.0, 3.0, 4.0, 5.0);
        assert_eq!((t.x(), t.y(), t.z()), (3.0, 4.0, 5.0));
        assert_eq!(t.m[3], [0.0, 0.0, 0.0, 1.0]);
        assert_eq!(t.m[0][0], 1.0);
        assert_eq!(t.m[1][1], 1.0);
    }

    #[test]
    fn the_world_key_is_fnv_1a() {
        let mut k = Fnv::new();
        k.bytes(b"a");
        assert_eq!(k.0, 0xaf63dc4c8601ec8c);
        let mut k = Fnv::new();
        k.add(1);
        let mut j = Fnv::new();
        j.bytes(&[1, 0, 0, 0]);
        assert_eq!(k.0, j.0);
    }

    #[test]
    fn the_cliff_textures_are_the_original_thirteen() {
        assert_eq!(RO_CLIFF_TEXTURES.len(), 13);
        assert_eq!(RO_CLIFF_TEXTURES[0].to_bytes(), b"textures\\spacecliff_1");
        assert_eq!(RO_CLIFF_TEXTURES[12].to_bytes(), b"textures\\cliffedge_5");
    }
}
