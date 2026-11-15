//! The weather storms whole in Rust on `EEE3DRust` (`EEE3DStorm` 0xfb088..0xfb9f6, `EEE3DRain` 0xf7394..0xf8ceb,
//! `EEE3DSnow` 0xf9b6d..0xfa70d, deleted): kind 0x2a the rain and 0x3d the snow over a list of tiles. Their tiles,
//! drops, move, set-up, step and files were already Rust (`effect_storm.rs`); this is the class: the constructor's
//! centre, the settings read from the graphic effect (`Virtual64`), the drop and flake models built on the first
//! set-up (a raindrop triangle, a snowflake quad, with their textures), the queue's copy of the drops for the frame,
//! both draws, the rain's sounds (the C++'s `rain_sounds` after the thunder timer) and the files.
//!
//! Step, Load, Save and the set-up's placing are simulation-side (the drops' random draws come from the effects'
//! generator: RUST_PLAN 2.1); the models, the queue and the draws are the renderer's.

use ee_ge::transform::RotationOrder;
use ee_ge::types::{Ge3DPoint, GeTransformation};

use crate::effect_rising::{EeGfxEffect3DOp, EeGfxGraphicFacts, Effect3DHost};
use crate::effect_state::{identity, EeGfxEffectRecord, EffectFile};
use crate::effect_storm::{self, EeGfxStormDraws, StormHost, EE_GFX_EFFECT_KIND_RAIN, EE_GFX_EFFECT_KIND_SNOW};
use crate::effects::{EeGfxColor, EeGfxTexCoord};
use crate::ffi_effect::{on_storm, put_storm, take_storm};
use crate::lighting::EeGfxLighting;
use crate::mesh::{EeGfxMeshTriangle, EeGfxMeshVertex, EE_GFX_MESH_COORD, EE_GFX_MESH_POINT};
use crate::raster::{
    EeGfxTextureLoad, Raster, ADDRESSING_CLAMP, BLEND_INV_SRC_ALPHA, BLEND_SRC_ALPHA, ZBUFFER_TEST_ONLY,
};

const OK: i32 = 0;

/// The raindrop's width and length (0x440bf0, 0x440bf4), its colour (`Virtual64`'s static) and opacity.
const RAIN_DROP_WIDTH: f32 = 0.04;
const RAIN_DROP_LENGTH: f32 = 0.6;
const RAIN_COLOR: EeGfxColor = EeGfxColor { red: 0.71, green: 0.87, blue: 1.0 };
const RAIN_OPACITY: f32 = 0.47;
/// The snowflake's width and height (0x440dac, 0x440db0), its opacity and the camera distance it fades within
/// (0x440db4).
const SNOW_FLAKE_WIDTH: f32 = 0.054;
const SNOW_FLAKE_HEIGHT: f32 = 0.047;
const SNOW_OPACITY: f32 = 0.8;
const SNOW_FADE_DISTANCE: f32 = 1.0;
/// The rain sound's fade-in (0x440be8, ms) and its share until then (0x440bec).
pub const EE_GFX_RAIN_FADE_TIME: f32 = 6500.0;
pub const EE_GFX_RAIN_FADE_SHARE: f32 = 0.4;

/// Whether `kind` is a weather storm's.
pub fn is_weather(kind: i32) -> bool {
    kind == EE_GFX_EFFECT_KIND_RAIN || kind == EE_GFX_EFFECT_KIND_SNOW
}

/// The storm's world through the effect's host.
pub(crate) struct Weather<'a, H: Effect3DHost>(pub(crate) &'a mut H);

impl<H: Effect3DHost> StormHost for Weather<'_, H> {
    fn on_map(&mut self, x: i32, y: i32) -> bool {
        self.0.storm_on_map(x, y)
    }
    fn is_water(&mut self, x: i32, y: i32) -> bool {
        self.0.storm_is_water(x, y)
    }
    fn ground_z(&mut self, x: i32, y: i32) -> f32 {
        self.0.storm_ground_z(x, y)
    }
    fn step_toward(&mut self, from: [i32; 2], to: [i32; 2]) -> [i32; 2] {
        self.0.storm_step_toward(from, to)
    }
    fn ripple(&mut self, x: f32, y: f32) {
        self.0.storm_ripple(x, y)
    }
}

/// `EEPlaceStorm`: the storm moved to a fresh transformation at `p` (`SetTransformation`).
///
/// # Safety
/// As `effect_rising::effect3d`.
unsafe fn place<H: Effect3DHost>(r: *mut EeGfxEffectRecord, h: &mut H, p: Ge3DPoint) {
    let mut t = identity();
    t.m[0][3] = p.x;
    t.m[1][3] = p.y;
    t.m[2][3] = p.z;
    // SAFETY: the function's contract
    unsafe { (*r).transformation = t };
    h.move_to();
}

/// `Virtual64`: the graphic effect's fall speed, spawn height, density and duration; the rain's sound ids and their
/// media; the drop or flake model on first use.
///
/// # Safety
/// As `effect_rising::effect3d`.
unsafe fn settings<H: Effect3DHost, R: Raster>(r: *mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, h: &mut H, x: &mut R) {
    // SAFETY (every access below): the function's contract; no reference is kept across a host call
    let rain = unsafe {
        let s = &mut (*r).storm;
        s.fall_speed = g.param1;
        s.spawn_height = g.param2;
        s.density = g.param3;
        s.duration = g.duration;
        (*r).kind == EE_GFX_EFFECT_KIND_RAIN
    };
    if rain {
        unsafe { (*r).storm.sounds = [g.int_param1 as u32, g.int_param2 as u32, g.int_param3 as u32] };
        h.rain_media();
    }
    if unsafe { (*r).storm.model.is_null() } {
        let model = if rain { rain_model(x) } else { snow_model(x) };
        unsafe { (*r).storm.model = model };
    }
}

/// A vertex with its point and texture coordinate.
fn vertex(x: f32, y: f32, z: f32, u: f32, v: f32) -> EeGfxMeshVertex {
    EeGfxMeshVertex {
        set: EE_GFX_MESH_POINT | EE_GFX_MESH_COORD,
        point: Ge3DPoint { x, y, z },
        coord: EeGfxTexCoord { u, v },
        ..Default::default()
    }
}

/// A triangle.
fn triangle(a: u32, b: u32, c: u32) -> EeGfxMeshTriangle {
    EeGfxMeshTriangle { set: 1, v: [a, b, c] }
}

/// The raindrop (`EEE3DRain::Virtual64`, 0xf7792): one triangle standing up, 0.04 wide and 0.6 long, light blue at
/// 0.47, with textures\raindrop.
fn rain_model<R: Raster>(x: &mut R) -> *mut std::ffi::c_void {
    let texture = x.load_texture("textures\\raindrop", &EeGfxTextureLoad::effect(ADDRESSING_CLAMP));
    let model = x.create_model(1, 3, 0, "EEE3DRainDropModel", 1);
    let v = [
        vertex(RAIN_DROP_WIDTH * -0.5, 0.0, 0.0, 0.0, 1.0),
        vertex(RAIN_DROP_WIDTH * 0.5, 0.0, 0.0, 1.0, 1.0),
        vertex(0.0, 0.0, RAIN_DROP_LENGTH, 0.5, 0.0),
    ];
    x.store_mesh(model, &v, &[triangle(0, 1, 2)]);
    x.material_ranges(model, 1, 3);
    x.model_material_opacity(model, RAIN_OPACITY);
    x.model_color(model, RAIN_COLOR);
    x.model_texture(model, texture);
    x.model_normals(model);
    x.model_bounds(model);
    model
}

/// The snowflake (`EEE3DSnow::Virtual64`, 0xf9ef5): a quad standing up, 0.054 wide and 0.047 high, at 0.8, with
/// textures\snowflake.
fn snow_model<R: Raster>(x: &mut R) -> *mut std::ffi::c_void {
    let texture = x.load_texture("textures\\snowflake", &EeGfxTextureLoad::effect(ADDRESSING_CLAMP));
    let model = x.create_model(2, 4, 0, "EEE3DSnowFlakeModel", 1);
    let (w, hgt) = (SNOW_FLAKE_WIDTH, SNOW_FLAKE_HEIGHT);
    let v = [
        vertex(w * -0.5, 0.0, hgt, 0.0, 0.0),
        vertex(w * 0.5, 0.0, hgt, 1.0, 0.0),
        vertex(w * 0.5, 0.0, 0.0, 1.0, 1.0),
        vertex(w * -0.5, 0.0, 0.0, 0.0, 1.0),
    ];
    x.store_mesh(model, &v, &[triangle(0, 1, 2), triangle(0, 2, 3)]);
    x.material_ranges(model, 2, 4);
    x.model_material_opacity(model, SNOW_OPACITY);
    x.model_texture(model, texture);
    x.model_normals(model);
    x.model_bounds(model);
    model
}

/// `Queue` (the rain's 0xf76da, the snow's 0xf9e42): the base's, then the drops copied for the frame's draw and the
/// storm queued in render queue 4.
///
/// # Safety
/// As `effect_rising::effect3d`.
unsafe fn queue<H: Effect3DHost>(r: *mut EeGfxEffectRecord, h: &mut H) -> i32 {
    let res = h.base_queue();
    if res != OK {
        return res;
    }
    // SAFETY: the function's contract; the queue runs under the world's lock (no step takes the lists meanwhile)
    unsafe {
        if (*r).storm.draws.is_null() {
            (*r).storm.draws = Box::into_raw(Box::default());
        }
        let d: &mut EeGfxStormDraws = &mut *(*r).storm.draws;
        d.draws.clear();
        if let Some(l) = (*r).storm.lists.as_ref() {
            d.draws.resize(l.drops.len(), Default::default());
            effect_storm::storm_draws(l, &mut d.draws);
        }
    }
    h.queue(4);
    res
}

/// `DrawPass6` (the rain's 0xf7b55, the snow's 0xfa404): the graphic object's alpha reference, depth test only, alpha
/// blending, the clouds' light and the scene's fog; then every queued drop's model turned to the camera (a flake also
/// spun by its angle and fading within 1 of the camera); the first draw error stops it.
///
/// # Safety
/// As `effect_rising::effect3d`.
unsafe fn draw<R: Raster>(r: *mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, x: &mut R) -> i32 {
    x.set_alpha_ref(if g.has_object { g.alpha_ref } else { 0.0 });
    x.set_zbuffer(ZBUFFER_TEST_ONLY);
    x.set_blending(BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA);
    x.set_shading_source(0);
    x.lighting(EeGfxLighting::Cloud);
    x.lighting(EeGfxLighting::SceneFog);
    // SAFETY: the function's contract; the draws are only written by the queue (the same thread)
    let (snow, model, draws) = unsafe {
        let d = (*r).storm.draws.as_ref();
        ((*r).kind == EE_GFX_EFFECT_KIND_SNOW, (*r).storm.model, d.map_or(Vec::new(), |d| d.draws.clone()))
    };
    let mut t = identity();
    let mut res = OK;
    for d in &draws {
        if res != OK {
            break;
        }
        t.m[0][3] = d.position.x;
        t.m[1][3] = d.position.y;
        t.m[2][3] = d.position.z;
        let yaw = x.camera_yaw();
        if snow {
            t.set_orientation(RotationOrder::Ypr, yaw, 0.0, d.angle);
            let camera: GeTransformation = x.camera();
            // the original's double square root of the float distance
            let distance = (camera.distance_squared_to(&t) as f64).sqrt();
            let opacity = if (distance as f32) < SNOW_FADE_DISTANCE {
                (distance / SNOW_FADE_DISTANCE as f64) as f32
            } else {
                1.0
            };
            x.model_opacity(model, opacity);
        } else {
            t.set_orientation(RotationOrder::Ypr, yaw, 0.0, 0.0);
        }
        x.model_transform(model, &t);
        x.model_prepare(model);
        res = x.model_draw(model, 1.0);
    }
    res
}

/// The weather storms' virtuals.
///
/// # Safety
/// As `effect_rising::effect3d`.
pub unsafe fn weather<H: Effect3DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY (every access below): the function's contract; no reference to the record is kept across a host call
    // (the storm host's calls read nothing of it)
    match op {
        // 0x4fb0c8 after `ee_gfx_storm_new` took the tiles: centred on them; the rain's first thunder (0xf7394)
        EeGfxEffect3DOp::Create => {
            if let Some(p) = unsafe { on_storm(r, |r, l| effect_storm::storm_center(r, l, &mut Weather(h))) } {
                unsafe { place(r, h, p) };
            }
            if unsafe { (*r).kind } == EE_GFX_EFFECT_KIND_RAIN {
                unsafe { crate::ffi_effect::ee_gfx_rain_new(r) };
            }
            OK
        }
        // 0x4fa291: the settings, then the storm at its tiles' mean with its radius
        EeGfxEffect3DOp::SetUp => {
            unsafe { settings(r, g, h, x) };
            let p = unsafe { on_storm(r, |r, l| effect_storm::storm_set_up(r, l, &mut Weather(h))) };
            unsafe { place(r, h, p) };
            OK
        }
        // 0x4f7d0f / 0x4fa5f3 after the base's: the settings again, the saved drops respawned
        EeGfxEffect3DOp::Repair => {
            unsafe { settings(r, g, h, x) };
            unsafe { on_storm(r, |r, l| effect_storm::storm_repair(r, l, &mut Weather(h))) };
            OK
        }
        EeGfxEffect3DOp::Queue => unsafe { queue(r, h) },
        EeGfxEffect3DOp::DrawPass6 => unsafe { draw(r, g, x) },
        // the rain's 0xf7e3f: its sounds (the loop at the start or after a load, the thunder when its timer runs
        // out, both levels); then the storm's 0xfa70d: the drops and the age
        EeGfxEffect3DOp::Step => {
            if unsafe { (*r).kind } == EE_GFX_EFFECT_KIND_RAIN {
                let start = unsafe { (*r).storm.elapsed == 0.0 || (*r).storm.restored };
                let rumble = unsafe { crate::ffi_effect::ee_gfx_rain_rumble(r, g.tick_time) };
                h.rain_sounds(start, rumble);
            }
            unsafe { on_storm(r, |r, l| effect_storm::storm_step(r, l, g.tick_time, g.world_speed, &mut Weather(h))) }
        }
        _ => OK,
    }
}

/// The storms' `Load` and `Save` after `EEEffect3D`'s (result `base`): the storm's and the leaf's own state when the
/// base's succeeded.
pub fn weather_file<F: EffectFile>(load: bool, base: i32, r: &mut EeGfxEffectRecord, f: &mut F) -> i32 {
    if base != OK {
        return base;
    }
    let rp: *mut EeGfxEffectRecord = r;
    // SAFETY: `r` is the only reference to the record; the lists are taken out and put back around the call
    unsafe {
        let mut l = take_storm(rp);
        let res =
            if load { effect_storm::load_storm(&mut *rp, &mut l, f) } else { effect_storm::save_storm(&*rp, &l, f) };
        put_storm(rp, l);
        res
    }
}

#[cfg(test)]
#[path = "effect_weather_tests.rs"]
mod tests;
