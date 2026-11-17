//! The 3D particle, the particle emitter and the marching line whole in Rust on `EEE3DRust` (`EEE3DParticle`
//! 0xf4bf3..0xf5d94, `EEE3DParticleEmitter` 0xf5d94..0xf69e3, `EEE3DMarchingLine` 0xf3580..0xf4247, deleted).
//!
//! - The particle (kind 0x39): placed at a point or a line's start (moving along its direction), set up with a random
//!   turn and brightness, moved by the render queue (drag, gravity, wind, rise, a bounce off the terrain), drawn in
//!   pass 6 growing and fading (or additive), its position applied by the step. Its set-up, move, tick and files were
//!   already `effect_state.rs`'s; the original's memory pool is gone (a `new` effect each).
//! - The emitter (kind 0x3a): at a point, along a line, or on a unit's tag point; each step it follows its unit's tag
//!   point (the host's `emitter_unit`) or moves itself, ends with its unit or duration, plays its animation sound and
//!   launches bursts of line particles (`emitter_burst`). It draws nothing (its queue answers 10).
//! - The marching line (kind 0x2d): a waypoint line between two points (or units), registered in the map's effect
//!   regions along it (the C++'s), drawn in pass 4 as a screen line fading from opaque to 0.3; its age, its ends and
//!   its files.
//!
//! The particles' and the emitter's steps, set-ups and files are simulation-side (the effects' random draws:
//! RUST_PLAN 2.1); the queues and draws are the renderer's.

use crate::ge::transform::RotationOrder;
use crate::ge::types::{Ge3DPoint, GeTransformation};

use crate::gfx::effect_rising::{EeGfxEffect3DOp, EeGfxGraphicFacts, Effect3DHost};
use crate::gfx::effect_state::{
    emitter_begin, emitter_burst, emitter_move, identity, load_emitter, load_particle, particle_set_up, particle_step,
    particle_update, save_emitter, save_particle, EeGfxEffectRecord, EeGfxEmitterBurst, EeGfxEmitterMove,
    EeGfxParticleSetUp, EffectFile, EmitterHost, EE_GFX_EFFECT_KIND_PARTICLE_3D,
};
use crate::gfx::effects::EeGfxColor;
use crate::gfx::ffi_effect::Ground;
use crate::gfx::lighting::EeGfxLighting;
use crate::gfx::raster::{Raster, BLEND_INV_SRC_ALPHA, BLEND_ONE, BLEND_SRC_ALPHA, COMPARE_LESS, ZBUFFER_TEST_ONLY};

const OK: i32 = 0;
/// The end of an effect (`kErrorNoMore`).
const ENDED: i32 = 7;

/// The particle emitter (`EEE3DParticleEmitter`).
pub const EE_GFX_EFFECT_KIND_EMITTER: i32 = 0x3a;
/// The marching line (`EEE3DMarchingLine`).
pub const EE_GFX_EFFECT_KIND_MARCHING_LINE: i32 = 0x2d;

/// Whether `kind` is one of this module's.
pub fn is_particle(kind: i32) -> bool {
    kind == EE_GFX_EFFECT_KIND_PARTICLE_3D
        || kind == EE_GFX_EFFECT_KIND_EMITTER
        || kind == EE_GFX_EFFECT_KIND_MARCHING_LINE
}

/// What an emitter's step reads of its unit (`EEE3DParticleEmitter::Step`, 0xf6417; `emitter_unit`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxEmitterUnit {
    /// the effect has a unit, a complex one
    pub has_unit: bool,
    pub complex: bool,
    /// the complex unit's command is the dead one (`kActionDead`); the unit's hit points (`GetHitPoints`)
    pub dead: bool,
    pub hit_points: i32,
    /// a complex unit followed (the graphic effect's flag 1): the turret part's result, the tag point's location
    /// at the unit's frame in its scaled location (its position without a tag point list), and that location
    pub follow_result: i32,
    pub location: Ge3DPoint,
    pub transformation: GeTransformation,
}

/// A marching line's state (`EEE3DMarchingLine`'s plain members).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxEffectMarch {
    /// `mFrom`, `mTo`: the ends without units
    pub from: Ge3DPoint,
    pub to: Ge3DPoint,
    /// `mDrawFrom`, `mDrawTo`: the ends `Queue` took for the draw (the units' positions, else the points)
    pub draw_from: Ge3DPoint,
    pub draw_to: Ge3DPoint,
    /// `mCC` (saved; nothing else reads it) and `mTime` (ms; saved)
    pub cc: f32,
    pub time: f32,
}

/// The particles' and the emitter's ground through the host.
fn ground<H: Effect3DHost>(h: &mut H) -> crate::gfx::ffi_effect::EeGfxParticleGround {
    h.particle_ground()
}

/// The record's position.
///
/// # Safety
/// As `effect_rising::effect3d`.
unsafe fn position(r: *const EeGfxEffectRecord) -> Ge3DPoint {
    // SAFETY: the function's contract
    let t = unsafe { (*r).transformation };
    Ge3DPoint { x: t.m[0][3], y: t.m[1][3], z: t.m[2][3] }
}

/// The effect moved to a fresh transformation at `p` (`SetTransformation`).
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

/// The emitter's lines through the host (`CreateLineEffect` of the graphic effect's linked effect 1).
struct Lines<'a, H: Effect3DHost>(&'a mut H);

impl<H: Effect3DHost> EmitterHost for Lines<'_, H> {
    fn line(&mut self, start: Ge3DPoint, direction: Ge3DPoint) -> i32 {
        self.0.emitter_line(&start, &direction)
    }
}

/// The 3D particle's virtuals.
///
/// # Safety
/// As `effect_rising::effect3d`.
unsafe fn particle<H: Effect3DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY (every access below): the function's contract; no reference to the record is kept across a host call
    match op {
        // 0x4f4d77 / 0x4f4e27: at the point or the line's start (the C++ put it in the record's `aged.location`, the
        // line's direction in the velocity), the game clock, the graphic effect's duration
        EeGfxEffect3DOp::CreateAt | EeGfxEffect3DOp::CreateLine => {
            let p = unsafe { (*r).aged.location };
            unsafe { place(r, h, p) };
            let now = h.particle_tick(&p).now;
            unsafe {
                let s = &mut (*r).particle;
                s.draw_position = p;
                s.last_time = now;
                s.duration = g.duration as f32;
            }
            OK
        }
        // 0x4f4f56: the turn and brightness, the radius, the media
        EeGfxEffect3DOp::SetUp => {
            let s = EeGfxParticleSetUp { int_param1: g.int_param1, param2: g.param2, param3: g.param3 };
            unsafe {
                particle_set_up(&mut *r, &s);
                (*r).bounding_radius = g.size;
            }
            h.load_media()
        }
        // 0x4f4d0f: the base's, the move (0x4f50b2), render queue 4
        EeGfxEffect3DOp::Queue => {
            let mut res = h.base_queue();
            if res == OK {
                let p = unsafe { position(r) };
                let tick = h.particle_tick(&p);
                let table = ground(h);
                res = unsafe { particle_update(&mut *r, &tick, &mut Ground(&table)) };
                if res == OK {
                    h.queue(4);
                }
            }
            res
        }
        EeGfxEffect3DOp::DrawPass6 => unsafe { particle_draw(r, g, h, x) },
        // 0x4f588d: 7 once outlived; the moved position applied
        EeGfxEffect3DOp::Step => {
            let p = unsafe { position(r) };
            let now = h.particle_tick(&p).now;
            let mut moved = false;
            let res = unsafe { particle_step(&mut *r, now, &mut moved) };
            if moved {
                let q = unsafe { (*r).particle.draw_position };
                unsafe { place(r, h, q) };
            }
            res
        }
        _ => OK,
    }
}

/// `EEE3DParticle::DrawPass6` (0x4f556b): the model in the effect's colour growing from the start size, alpha blended
/// and fading with age (or additive and lit by the fog factor with the effect's flag 1), at the particle's turn and
/// position; the model's colour, opacity and the fog restored.
///
/// # Safety
/// As `effect_rising::effect3d`.
unsafe fn particle_draw<H: Effect3DHost, R: Raster>(
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY: the function's contract
    let p = unsafe { (*r).particle };
    let model = h.model();
    let color = EeGfxColor { red: g.param5, green: g.param6, blue: g.param7 };
    let size = (g.size - g.start_size) * p.draw_age + g.start_size;
    x.set_alpha_ref(if g.has_object { g.alpha_ref } else { 0.0 });
    let fog = x.get_fog();
    x.set_zbuffer(ZBUFFER_TEST_ONLY);
    x.set_shading_source(0);
    if !g.flag1 {
        x.set_blending(BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA);
        x.lighting(EeGfxLighting::Unit);
        x.model_opacity(model, (1.0 - p.draw_age) * g.param1);
        x.set_ambient(EeGfxColor { red: p.brightness, green: p.brightness, blue: p.brightness });
    } else {
        let light = x.fog_factor(&p.draw_position, fog);
        x.set_blending(BLEND_ONE, BLEND_ONE);
        x.lighting(EeGfxLighting::White);
        x.model_opacity(model, g.param1);
        let v = (1.0 - p.draw_age) * p.brightness * light;
        x.set_ambient(EeGfxColor { red: v, green: v, blue: v });
    }
    x.model_color(model, color);
    let texture = h.texture();
    x.model_texture(model, texture);
    let mut t = identity();
    t.set_orientation(RotationOrder::Ypr, p.yaw, p.pitch, p.roll);
    t.m[0][3] = p.draw_position.x;
    t.m[1][3] = p.draw_position.y;
    t.m[2][3] = p.draw_position.z;
    t.m[3] = [0.0, 0.0, 0.0, 1.0];
    x.model_transform(model, &t);
    x.model_scale(model, size);
    x.model_prepare(model);
    let res = x.model_draw(model, 1.0);
    x.model_color(model, EeGfxColor { red: 1.0, green: 1.0, blue: 1.0 });
    x.model_opacity(model, 1.0);
    x.set_fog(fog);
    res
}

/// The emitter's virtuals.
///
/// # Safety
/// As `effect_rising::effect3d`.
unsafe fn emitter<H: Effect3DHost>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
) -> i32 {
    // SAFETY (every access below): the function's contract; no reference to the record is kept across a host call
    match op {
        // 0x4f5dd7 / 0x4f5e82: at the point or the line's start (the C++ put the line's direction in the velocity)
        EeGfxEffect3DOp::CreateAt | EeGfxEffect3DOp::CreateLine => {
            let p = unsafe { (*r).aged.location };
            unsafe { place(r, h, p) };
            OK
        }
        // 0x4f5f23: on the unit (the C++ places it)
        EeGfxEffect3DOp::Create => {
            h.emitter_place();
            OK
        }
        // 0x4f634b: the timers
        EeGfxEffect3DOp::SetUp => {
            unsafe {
                (*r).emitter.time = 0.0;
                (*r).emitter.emit_time = 0.0;
            }
            OK
        }
        // nothing to draw (ICF-folded into 0x1020dc)
        EeGfxEffect3DOp::Queue => 10,
        EeGfxEffect3DOp::Step => unsafe { emitter_step(r, g, h) },
        _ => OK,
    }
}

/// `EEE3DParticleEmitter::Step` (0x4f6417): the place (the unit's tag point, else its own move), the end (its unit's
/// death or hit points, its duration), the transformation there, the animation sound, the burst.
///
/// # Safety
/// As `effect_rising::effect3d`.
unsafe fn emitter_step<H: Effect3DHost>(r: *mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, h: &mut H) -> i32 {
    // SAFETY (every access below): the function's contract
    let delta = unsafe { emitter_begin(&mut *r, g.tick_seconds) };
    let seconds = delta / 1000.0;
    let mut location = Ge3DPoint::default();
    let mut transformation = identity();
    let mut res;
    let unit = h.emitter_unit();
    let time = |r: *mut EeGfxEffectRecord| unsafe { (*r).emitter.time };
    let over = |r: *mut EeGfxEffectRecord| (g.duration as f32) < time(r);
    // `Move` (0x4f60f5): drag, gravity, wind and rise; a bounce off the terrain (7 off the map with it)
    let step = |r: *mut EeGfxEffectRecord, h: &mut H, out: &mut Ge3DPoint| {
        let wind = h.particle_tick(out).wind;
        let m = EeGfxEmitterMove {
            wind,
            param4: g.param4,
            param8: g.param8,
            param9: g.param9,
            param10: g.param10,
            param11: g.param11,
            flag2: g.flag2,
        };
        let table = ground(h);
        unsafe { emitter_move(&mut *r, seconds, &m, &mut Ground(&table), out) }
    };
    if unit.has_unit && unit.complex {
        if g.flag1 {
            transformation = unit.transformation;
            location = unit.location;
            res = unit.follow_result;
        } else {
            res = step(r, h, &mut location);
            transformation = unsafe { (*r).transformation };
        }
        if g.flag2 {
            if unit.dead {
                res = ENDED;
            }
        } else if g.ends_with_unit {
            if unit.hit_points <= 0 || over(r) {
                res = ENDED;
            }
        } else if over(r) {
            res = ENDED;
        }
    } else if unit.has_unit {
        if g.flag1 {
            location = unsafe { position(r) };
            res = OK;
        } else {
            res = step(r, h, &mut location);
        }
        if g.flag2 {
            if unit.hit_points <= 0 {
                res = ENDED;
            }
        } else if g.ends_with_unit {
            if unit.hit_points <= 0 || over(r) {
                res = ENDED;
            }
        } else if over(r) {
            res = ENDED;
        }
    } else {
        res = step(r, h, &mut location);
        if over(r) {
            res = ENDED;
        }
    }
    if res != ENDED {
        transformation.m[0][3] = location.x;
        transformation.m[1][3] = location.y;
        transformation.m[2][3] = location.z;
        unsafe { (*r).transformation = transformation };
        h.move_to();
        let first = unsafe { !(*r).emitter.stepped };
        h.emitter_sound(&location, first);
        let b = EeGfxEmitterBurst {
            count: g.int_param3 as u32,
            position_spread: g.emit_spread,
            flat: g.emit_flat,
            param1: g.param1,
            param2: g.param2,
            param3: g.param3,
            param4: g.param4,
            param5: g.param5,
            param6: g.param6,
            int_param1: g.int_param1,
            int_param2: g.int_param2,
        };
        res = unsafe { on_burst(r, delta, location, &transformation, &b, res, h) };
    }
    unsafe { (*r).emitter.stepped = true };
    res
}

/// The burst over a copy of the record (the lines it launches make other effects, not this one).
///
/// # Safety
/// As `effect_rising::effect3d`.
unsafe fn on_burst<H: Effect3DHost>(
    r: *mut EeGfxEffectRecord,
    delta: f32,
    location: Ge3DPoint,
    transformation: &GeTransformation,
    b: &EeGfxEmitterBurst,
    res: i32,
    h: &mut H,
) -> i32 {
    // SAFETY: the function's contract; the copy is the only reference while the burst runs
    let mut local = unsafe { r.read() };
    let out = emitter_burst(&mut local, delta, location, transformation, b, res, &mut Lines(h));
    // SAFETY: as above
    unsafe { (*r).emitter = local.emitter };
    out
}

/// The marching line's virtuals.
///
/// # Safety
/// As `effect_rising::effect3d`.
unsafe fn march<H: Effect3DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY (every access below): the function's contract; no reference to the record is kept across a host call
    match op {
        // 0x4f38a4: the timers, the graphic object's media
        EeGfxEffect3DOp::SetUp => {
            unsafe {
                (*r).march.cc = 0.0;
                (*r).march.time = 0.0;
            }
            h.march_media();
            OK
        }
        // 0x4f3ee8 after the base's: the saved units back, the media
        EeGfxEffect3DOp::Repair => {
            h.march_media();
            OK
        }
        // 0x4f3793: the base effect's (not the 3D one's), the ends for the draw, render queue 2
        EeGfxEffect3DOp::Queue => {
            let res = h.effect_queue();
            if res == OK {
                let (from, to) = unsafe { ((*r).march.from, (*r).march.to) };
                let ends = h.march_ends(&from, &to);
                unsafe {
                    (*r).march.draw_from = ends[0];
                    (*r).march.draw_to = ends[1];
                }
                h.queue(2);
            }
            res
        }
        // 0x4f3c68: a screen line between the projected ends, opaque to 0.3, without fog, depth test only
        EeGfxEffect3DOp::DrawPass4 => {
            x.set_alpha_ref(if g.has_object { g.alpha_ref } else { 0.0 });
            let (from, to) = unsafe { ((*r).march.draw_from, (*r).march.draw_to) };
            let start = x.screen_point(&from);
            let end = x.screen_point(&to);
            let fog = x.get_fog();
            let zbuffer = x.get_zbuffer();
            let zcompare = x.get_zcompare();
            x.set_fog(0);
            x.set_zbuffer(ZBUFFER_TEST_ONLY);
            x.set_zcompare(COMPARE_LESS);
            x.set_blending(BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA);
            x.set_shading_source(1);
            x.lighting(EeGfxLighting::White);
            let res = x.screen_line(start, end, 1.0, 0.3);
            x.set_fog(fog);
            x.set_zbuffer(zbuffer);
            x.set_zcompare(zcompare);
            res
        }
        // 0x4f4068: 7 once a set duration is up
        EeGfxEffect3DOp::Step => {
            let time = unsafe {
                (*r).march.time += g.tick_time;
                (*r).march.time
            };
            if time >= g.duration as f32 && g.duration != 0 {
                ENDED
            } else {
                OK
            }
        }
        _ => OK,
    }
}

/// The virtuals of this module's kinds.
///
/// # Safety
/// As `effect_rising::effect3d`.
pub unsafe fn particles<H: Effect3DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY: the function's contract
    match unsafe { (*r).kind } {
        EE_GFX_EFFECT_KIND_PARTICLE_3D => unsafe { particle(op, r, g, h, x) },
        EE_GFX_EFFECT_KIND_EMITTER => unsafe { emitter(op, r, g, h) },
        _ => unsafe { march(op, r, g, h, x) },
    }
}

/// The `Load` and `Save` of this module's kinds after `EEEffect3D`'s (result `base`).
pub fn particles_file<H: Effect3DHost, F: EffectFile>(
    load: bool,
    base: i32,
    r: &mut EeGfxEffectRecord,
    f: &mut F,
    major: u32,
    minor: u32,
    h: &mut H,
) -> i32 {
    match r.kind {
        // 0x4f53bc / 0x4f57fc: the base's result is ignored; the media's too
        EE_GFX_EFFECT_KIND_PARTICLE_3D => {
            if !load {
                return save_particle(r, f);
            }
            h.load_media();
            let mut loaded = false;
            let res = load_particle(r, f, major, minor, &mut loaded);
            if loaded {
                let t = r.transformation;
                r.particle.draw_position = Ge3DPoint { x: t.m[0][3], y: t.m[1][3], z: t.m[2][3] };
                r.particle.duration = h.effect_duration() as f32;
            }
            res
        }
        // 0x4f635e / 0x4f63b9: the base's result is ignored
        EE_GFX_EFFECT_KIND_EMITTER => {
            if load {
                load_emitter(r, f)
            } else {
                save_emitter(r, f)
            }
        }
        // 0x4f3b66 / 0x4f3f56: the time, mCC, then each end's unit (a flag and its id)
        _ => {
            if base != OK {
                return base;
            }
            if load {
                load_march(r, f, h)
            } else {
                save_march(r, f, h)
            }
        }
    }
}

/// The marching line's reads after the base's: the time, mCC and the two units' ids (kept by the C++ until
/// `RepairPointers`).
fn load_march<H: Effect3DHost, F: EffectFile>(r: &mut EeGfxEffectRecord, f: &mut F, h: &mut H) -> i32 {
    let mut res = f.read_f32(&mut r.march.time);
    if res != OK {
        return res;
    }
    res = f.read_f32(&mut r.march.cc);
    if res != OK {
        return res;
    }
    for which in 0..2 {
        let mut has = false;
        res = f.read_bool(&mut has);
        if res != OK || !has {
            if which == 0 && res == OK {
                continue;
            }
            return res;
        }
        let mut id = 0;
        res = f.read_u32(&mut id);
        if res != OK {
            return res;
        }
        h.march_set_unit(which, id);
    }
    res
}

/// The marching line's writes after the base's: the time, mCC and each end unit's flag and id.
fn save_march<H: Effect3DHost, F: EffectFile>(r: &EeGfxEffectRecord, f: &mut F, h: &mut H) -> i32 {
    let mut res = f.write_f32(r.march.time);
    if res != OK {
        return res;
    }
    res = f.write_f32(r.march.cc);
    if res != OK {
        return res;
    }
    for which in 0..2 {
        let id = h.march_unit_id(which);
        res = f.write_bool(id >= 0);
        if res != OK {
            return res;
        }
        if id >= 0 {
            res = f.write_u32(id as u32);
            if res != OK {
                return res;
            }
        }
    }
    res
}

#[cfg(test)]
#[path = "effect_particles_tests.rs"]
mod tests;
