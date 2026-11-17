//! The 2D sprites made of parts (one sprite per tag point of the effect's kind on the unit, `EEEffect2D::mParts`, a
//! C++ list the host reads and writes) whole in Rust on `EEE2DRust` (R4.4, twentieth round): the explosion (kind 4,
//! `EEE2DExplosion`), the fire (5, `EEE2DFire`) and the exhaust trail (3, `EEE2DExhaustTrail`), and the smoke muzzle
//! blast (0x14, `EEE2DSmokeMuzzleBlast`, no parts: its puffs trail back along its direction) and the smoke puffs (0xb and
//! 0xc, `EEE2DSmokePlume`, whose state, tick and files were already `effect_state.rs`'s); their classes deleted.

use ee_base::math::transform::RotationOrder;
use ee_base::math::types::{Ge3DPoint, GeTransformation, GeVector};
use ee_base::math::um;

use crate::gfx::effect_2d::Effect2DHost;
use crate::gfx::effect_rising::{EeGfxEffect3DOp, EeGfxGraphicFacts};
use crate::gfx::effect_sprite::EeGfxEffectSprite;
use crate::gfx::effect_state::{
    load_plume, load_point, plume_new, plume_step, random, random_long, save_plume, save_point, EeGfxEffectRecord,
    EeGfxPlumeTick, EffectFile, EE_GFX_EFFECT_KIND_SMOKE_PLUME, EE_GFX_EFFECT_KIND_VELOCITY_PARTICLE, OK,
};
use crate::gfx::effects::EeGfxColor;
use crate::gfx::lighting::EeGfxLighting;
use crate::gfx::raster::{
    Raster, BLEND_INV_SRC_ALPHA, BLEND_ONE, BLEND_SRC_ALPHA, EE_GFX_SPRITE_BLENDED, ZBUFFER_TEST_ONLY,
};

/// `kEffect2DExhaustTrail` .. the kinds of this file.
pub const EE_GFX_EFFECT_KIND_EXHAUST_TRAIL: i32 = 3;
pub const EE_GFX_EFFECT_KIND_EXPLOSION: i32 = 4;
pub const EE_GFX_EFFECT_KIND_FIRE: i32 = 5;
pub const EE_GFX_EFFECT_KIND_SMOKE_MUZZLE_BLAST: i32 = 0x14;

/// The explosion's parts' lift (0x43fb24); the exhaust trail's growth (0x43fb18); the muzzle blast's rise per ms.
const EXPLOSION_LIFT: f32 = 0.4;
const EXHAUST_GROWTH: f32 = 6.0;
const BLAST_RISE: f32 = 0.00035;

/// One part as the host reads and writes it (`EEEffect2DPart` without its graphic object).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfx2DPart {
    pub position: Ge3DPoint,
    /// the tag point's index (10000: the unit's own position)
    pub tag: u32,
    pub frame: u32,
    pub draw_frame: u32,
    pub time: f32,
    pub done: bool,
}

/// `CreateParts`' place: the position and the height (the farthest part plus the effect's size).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfx2DPlace {
    pub position: Ge3DPoint,
    pub height: f32,
}

/// The record's sprite state.
///
/// # Safety
/// `r` is a live record; the reference does not live across a host call.
unsafe fn sp<'a>(r: *mut EeGfxEffectRecord) -> &'a mut EeGfxEffectSprite {
    // SAFETY: the function's contract
    unsafe { &mut (*r).sprite }
}

/// `SetPosition(p)`.
///
/// # Safety
/// As `sp`.
unsafe fn set_position<H: Effect2DHost>(r: *mut EeGfxEffectRecord, p: Ge3DPoint, h: &mut H) {
    // SAFETY: the function's contract
    unsafe { (*r).position = p };
    h.move_to_point();
}

fn alpha_ref(g: &EeGfxGraphicFacts) -> f32 {
    if g.has_object {
        g.alpha_ref
    } else {
        0.0
    }
}

/// `SetUp` of the explosion and exhaust trail: the parts `lift` above a buried unit, the position and radius; 0.
///
/// # Safety
/// As `sp`.
unsafe fn make_parts<H: Effect2DHost>(r: *mut EeGfxEffectRecord, lift: f32, h: &mut H) -> EeGfx2DPlace {
    let place = h.create_parts(lift);
    // SAFETY: the function's contract
    unsafe { set_position(r, place.position, h) };
    place
}

/// The parts' virtuals.
///
/// # Safety
/// As `effect_2d::effect2d`.
pub unsafe fn parts<H: Effect2DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY (every access below): the function's contract; no reference to the record lives across a host call
    let kind = unsafe { (*r).kind };
    match (kind, op) {
        // 0xdf6d0: a random spin direction
        (EE_GFX_EFFECT_KIND_EXHAUST_TRAIL, EeGfxEffect3DOp::Create) => {
            let clockwise = random() < 0.5;
            let s = unsafe { sp(r) };
            s.angle = 0.0;
            s.draw_angle = 0.0;
            s.flag = clockwise;
            0
        }
        // 0xdfdd2 / 0xdf7f2: the parts, 0.4 up for the explosion
        (EE_GFX_EFFECT_KIND_EXPLOSION | EE_GFX_EFFECT_KIND_EXHAUST_TRAIL, EeGfxEffect3DOp::SetUp) => {
            let lift = if kind == EE_GFX_EFFECT_KIND_EXPLOSION { EXPLOSION_LIFT } else { 0.0 };
            let place = unsafe { make_parts(r, lift, h) };
            unsafe { (*r).bounding_radius = place.height };
            0
        }
        // 0xe0498: the parts raised by half the size, each on a random frame of its animation
        (EE_GFX_EFFECT_KIND_FIRE, EeGfxEffect3DOp::SetUp) => {
            let offset = g.size * 0.5;
            let place = h.create_parts(0.0);
            for i in 0..h.part_count() {
                let mut part = h.part(i);
                part.position.z += offset;
                let frames = h.part_frames(i);
                part.frame = frames.start.wrapping_add(random_long(0, frames.count.wrapping_sub(1) as i32) as u32);
                h.set_part(i, &part);
            }
            unsafe {
                set_position(r, place.position, h);
                (*r).bounding_radius = place.height;
            }
            0
        }
        // 0xdfdf5 / 0xdf776: the parts' frames for the draw, render queue 4 (3 for the trail, its spin copied)
        (EE_GFX_EFFECT_KIND_EXPLOSION | EE_GFX_EFFECT_KIND_EXHAUST_TRAIL, EeGfxEffect3DOp::Queue) => {
            let res = h.base_queue();
            if res == 0 {
                if kind == EE_GFX_EFFECT_KIND_EXHAUST_TRAIL {
                    unsafe { sp(r).draw_angle = sp(r).angle };
                }
                h.parts_snapshot();
                h.queue(if kind == EE_GFX_EFFECT_KIND_EXPLOSION { 4 } else { 3 });
            }
            res
        }
        // 0xe042c: queued while the unit is alive and damaged to the threshold, more parts the more damaged; else 9
        (EE_GFX_EFFECT_KIND_FIRE, EeGfxEffect3DOp::Queue) => {
            let count = h.part_count();
            let mut res = h.base_queue();
            if res == 0 {
                h.parts_snapshot();
                let health = g.unit_hit_points as f32 / h.max_hit_points() as f32;
                if health <= g.param2 && g.unit_hit_points > 0 {
                    let damage = 1.0 - health / g.param2;
                    let shown = um::lrintf(count as f32 * damage);
                    let last = count as i32 - 1;
                    unsafe { sp(r).last_part = if shown < last { shown } else { last } };
                    h.queue(4);
                } else {
                    res = 9;
                }
            }
            res
        }
        // 0xdfe62 / 0xe0526: each (shown) part an additive fog-lit sprite, the fire's facing the camera
        (EE_GFX_EFFECT_KIND_EXPLOSION | EE_GFX_EFFECT_KIND_FIRE, EeGfxEffect3DOp::DrawPass6) => {
            let fire = kind == EE_GFX_EFFECT_KIND_FIRE;
            let mut t = if fire { GeTransformation::IDENTITY } else { x.camera() };
            let model = h.model();
            let count = if fire { unsafe { sp(r).last_part as u32 }.wrapping_add(1) } else { h.part_count() };
            x.set_alpha_ref(alpha_ref(g));
            let fog = x.get_fog();
            if fire {
                t.set_orientation(RotationOrder::Ypr, x.camera_yaw(), 0.0, 0.0);
            }
            x.set_zbuffer(ZBUFFER_TEST_ONLY);
            x.set_blending(BLEND_ONE, BLEND_ONE);
            x.set_shading_source(0);
            x.lighting(EeGfxLighting::White);
            let mut res = 0;
            let mut i = 0;
            while i < count && res == 0 {
                let texture = h.part_texture(i);
                let part = h.part(i);
                x.texture_frame(texture, part.draw_frame);
                x.model_texture(model, texture);
                let value = x.fog_factor(&part.position, fog);
                x.set_ambient(EeGfxColor { red: value, green: value, blue: value });
                t.m[0][3] = part.position.x;
                t.m[1][3] = part.position.y;
                t.m[2][3] = part.position.z;
                x.model_transform(model, &t);
                x.model_scale(model, g.size);
                x.model_prepare(model);
                res = x.model_draw(model, 1.0);
                i += 1;
            }
            x.set_fog(fog);
            res
        }
        // 0xdf856: each part an alpha-blended sprite spun by the angle, growing to seven times the size as the first
        // part animates
        (EE_GFX_EFFECT_KIND_EXHAUST_TRAIL, EeGfxEffect3DOp::DrawPass5) => {
            let model = h.model();
            let height = g.size;
            let first = h.part(0);
            let frames = h.part_frames(0);
            let growth = (first.draw_frame as f32 - frames.start as i32 as f32) / frames.count as i32 as f32 * height;
            let scale = growth * EXHAUST_GROWTH + height;
            let count = h.part_count();
            x.set_alpha_ref(alpha_ref(g));
            let mut spin = GeTransformation::IDENTITY;
            spin.set_orientation(RotationOrder::Ypr, 0.0, 0.0, unsafe { sp(r).draw_angle });
            let mut t = x.camera().compose(&spin);
            x.set_zbuffer(ZBUFFER_TEST_ONLY);
            x.set_blending(BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA);
            x.lighting(EeGfxLighting::Prelit);
            let mut res = 0;
            let mut i = 0;
            while i < count && res == 0 {
                let texture = h.part_texture(i);
                let part = h.part(i);
                x.texture_frame(texture, part.draw_frame);
                x.model_texture(model, texture);
                t.m[0][3] = part.position.x;
                t.m[1][3] = part.position.y;
                t.m[2][3] = part.position.z;
                x.model_transform(model, &t);
                x.model_scale(model, scale);
                x.model_prepare(model);
                res = x.model_draw(model, 1.0);
                i += 1;
            }
            res
        }
        // 0xe0123 / 0xdfa66: the unfinished parts animate (the exhaust trail spinning a turn a second); 7 once all are
        // done
        (EE_GFX_EFFECT_KIND_EXPLOSION | EE_GFX_EFFECT_KIND_EXHAUST_TRAIL, EeGfxEffect3DOp::Step) => {
            if kind == EE_GFX_EFFECT_KIND_EXHAUST_TRAIL {
                let spin = g.tick_seconds * 6.283_185_5;
                let s = unsafe { sp(r) };
                if s.flag {
                    s.angle += spin;
                    if s.angle > 3.141_592_7 {
                        s.angle -= 6.283_185_5;
                    }
                } else {
                    s.angle -= spin;
                    if s.angle < -3.141_592_7 {
                        s.angle += 6.283_185_5;
                    }
                }
            }
            let count = h.part_count();
            let mut done = 0;
            for i in 0..count {
                let mut part = h.part(i);
                if !part.done {
                    if kind == EE_GFX_EFFECT_KIND_EXPLOSION {
                        unsafe { explosion_part(r, i, &mut part, g, h) };
                    } else {
                        exhaust_part(i, &mut part, g, h);
                    }
                }
                if part.done {
                    done += 1;
                }
            }
            if done == count {
                7
            } else {
                0
            }
        }
        // 0xe076f: follows the unit and animates every part while it is alive and damaged enough; else 7
        (EE_GFX_EFFECT_KIND_FIRE, EeGfxEffect3DOp::Step) => {
            let location = h.unit_point();
            let count = h.part_count();
            if unsafe { (*r).position } != location {
                unsafe { set_position(r, location, h) };
            }
            if g.unit_hit_points as f32 / h.max_hit_points() as f32 <= g.param2 && g.unit_hit_points > 0 {
                for i in 0..count {
                    unsafe { fire_part(r, i, g, h) };
                }
                0
            } else {
                7
            }
        }
        (EE_GFX_EFFECT_KIND_SMOKE_MUZZLE_BLAST, _) => unsafe { blast(op, r, g, h, x) },
        (EE_GFX_EFFECT_KIND_SMOKE_PLUME | EE_GFX_EFFECT_KIND_VELOCITY_PARTICLE, _) => unsafe { plume(op, r, g, h, x) },
        _ => 0,
    }
}

/// The explosion's part `i` (0xe01b6): its frame by the tick, done past the last; the first part's sound.
///
/// # Safety
/// As `sp`.
unsafe fn explosion_part<H: Effect2DHost>(
    r: *mut EeGfxEffectRecord,
    i: u32,
    part: &mut EeGfx2DPart,
    g: &EeGfxGraphicFacts,
    h: &mut H,
) {
    // SAFETY: the function's contract
    let location = unsafe { (*r).position };
    let (time, old) = (part.time, part.frame);
    let frames = h.part_frames(i);
    let start = part.frame == frames.start && time == 0.0;
    let mut frame = old;
    let mut time = g.tick_time + time;
    let mut done = false;
    if time >= frames.time as f32 {
        let n = um::lrintf(time / frames.time as f32) as u32;
        time -= n.wrapping_mul(frames.time) as f32;
        frame = n.wrapping_add(old);
        done = frame > frames.last;
        part.done = done;
        h.set_part(i, part);
    }
    if !done {
        part.frame = frame;
        part.time = time;
        h.set_part(i, part);
        if i == 0 {
            h.animation_sound(location, frame, start || frame != old, true);
        }
    }
}

/// The exhaust trail's part `i` (0xdfb2e): its frame by the tick, done past the last.
fn exhaust_part<H: Effect2DHost>(i: u32, part: &mut EeGfx2DPart, g: &EeGfxGraphicFacts, h: &mut H) {
    let frames = h.part_frames(i);
    let mut frame = part.frame;
    let mut time = g.tick_time + part.time;
    if time >= frames.time as f32 {
        let n = um::lrintf(time / frames.time as f32) as u32;
        time -= n.wrapping_mul(frames.time) as f32;
        frame = n.wrapping_add(frame);
        let done = frame > frames.last;
        part.done = done;
        if done {
            h.set_part(i, part);
            return;
        }
    }
    part.frame = frame;
    part.time = time;
    h.set_part(i, part);
}

/// The fire's part `i` (0xe0a22): to its tag point on the unit, half the size up; its frame looping over the
/// animation; the first part's sound.
///
/// # Safety
/// As `sp`.
unsafe fn fire_part<H: Effect2DHost>(r: *mut EeGfxEffectRecord, i: u32, g: &EeGfxGraphicFacts, h: &mut H) {
    // SAFETY: the function's contract
    let location = unsafe { (*r).position };
    let mut half = g.size * 0.5;
    let mut part = h.part(i);
    let (time, old) = (part.time, part.frame);
    let frames = h.part_frames(i);
    let start = part.frame == frames.start && time == 0.0;
    let placed = h.part_follow(i);
    if placed.has {
        part.position = placed.point;
        half += part.position.z;
        part.position.z = half;
    }
    let mut frame = old;
    let mut time = g.tick_time + time;
    if time >= frames.time as f32 {
        let n = um::lrintf(time / frames.time as f32) as u32;
        time -= n.wrapping_mul(frames.time) as f32;
        frame = frames.start.wrapping_add(n.wrapping_add(old).wrapping_sub(frames.start) % frames.count);
    }
    part.frame = frame;
    part.time = time;
    h.set_part(i, &part);
    if i == 0 {
        h.animation_sound(location, frame, start || frame != old, true);
    }
}

/// The smoke muzzle blast.
///
/// # Safety
/// As `parts`.
unsafe fn blast<H: Effect2DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY (every access below): the function's contract
    match op {
        // 0xe6664 between two units / 0xe6a07 at a point: the C++ put the weapon point in `start` and the target in
        // `draw_position`; the direction, the place, the radius, a random rotation
        EeGfxEffect3DOp::CreateBetween => {
            let (start, end) = unsafe { (sp(r).start, sp(r).draw_position) };
            let mut v = GeVector { x: end.x - start.x, y: end.y - start.y, z: end.z - start.z };
            v.normalize();
            unsafe {
                let s = sp(r);
                s.velocity = Ge3DPoint { x: v.x, y: v.y, z: v.z };
                s.start = Ge3DPoint::default();
                s.draw_position = Ge3DPoint::default();
                set_position(r, start, h);
                (*r).bounding_radius = g.size;
            }
            let rotation = random() * 6.283_185_5;
            let s = unsafe { sp(r) };
            s.age = 0.0;
            s.rotation = rotation;
            0
        }
        // 0xe6b26
        EeGfxEffect3DOp::SetUp => h.object_media(),
        // 0xe844d (the winter breath's): the age and position for the draw, render queue 4
        EeGfxEffect3DOp::Queue => {
            let res = h.base_queue();
            if res == 0 {
                unsafe {
                    let s = sp(r);
                    s.draw_age = s.age;
                    s.draw_position = (*r).position;
                }
                h.queue(4);
            }
            res
        }
        // 0xe6b44: a growing, fading puff, then up to two smaller turned puffs back along the direction once their
        // delays (time 1, twice time 1) have passed; 0
        EeGfxEffect3DOp::DrawPass6 => {
            let s = unsafe { *sp(r) };
            let white = EeGfxColor { red: 1.0, green: 1.0, blue: 1.0 };
            let mut p = s.draw_position;
            let age = s.draw_age / g.duration as f32;
            let size = (g.size - g.start_size) * age + g.start_size;
            let size2 = size * 0.7;
            let size3 = size * 0.55;
            let delay = s.draw_age - g.time1 as f32;
            let delay2 = s.draw_age - g.time1 as f32 * 2.0;
            let alpha = 1.0 - (g.param3 * age + (1.0 - g.param3));
            let offset = -g.alpha;
            let reference = alpha_ref(g);
            x.sprite(EE_GFX_SPRITE_BLENDED, &p, white, s.rotation, size, alpha, 1.0, reference);
            if !(delay >= 0.0) {
                return 0;
            }
            p.x += s.velocity.x * offset;
            p.y += s.velocity.y * offset;
            p.z += s.velocity.z * offset;
            x.sprite(EE_GFX_SPRITE_BLENDED, &p, white, s.rotation + 1.047_197_6, size2, alpha, 1.0, reference);
            if !(delay2 >= 0.0) {
                return 0;
            }
            p.x += s.velocity.x * offset;
            p.y += s.velocity.y * offset;
            p.z += s.velocity.z * offset;
            x.sprite(EE_GFX_SPRITE_BLENDED, &p, white, s.rotation + 2.094_395_2, size3, alpha, 1.0, reference);
            0
        }
        // 0xe6d9b: ages and moves along the direction, slowing to a stop at parameter 2's age, drifting with the wind
        // and rising; 7 once its duration is over
        EeGfxEffect3DOp::Step => {
            let wind = h.wind();
            let elapsed = g.tick_seconds * 1000.0;
            let mut step = g.param1 * g.tick_seconds;
            let age = unsafe {
                sp(r).age += elapsed;
                sp(r).age
            };
            if !(age <= g.duration as f32) {
                return 7;
            }
            let mut fade = 1.0 - age / g.param2;
            if fade < 0.0 {
                fade = 0.0;
            }
            step = fade * fade * step;
            let (v, at) = unsafe { (sp(r).velocity, (*r).position) };
            let p = Ge3DPoint {
                x: v.x * step + wind.x * elapsed / 10.0 + at.x,
                y: v.y * step + wind.y * elapsed / 10.0 + at.y,
                z: (wind.z + BLAST_RISE) * elapsed / 10.0 + v.z * step + at.z,
            };
            unsafe { set_position(r, p, h) };
            0
        }
        _ => 0,
    }
}

/// The smoke puffs.
///
/// # Safety
/// As `parts`.
unsafe fn plume<H: Effect2DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY (every access below): the function's contract
    match op {
        // 0xe6f35 at a point / the velocity particle along a line (the C++ put the start in `start`, the direction in
        // `velocity`): a random rotation and brightness
        EeGfxEffect3DOp::CreateAt | EeGfxEffect3DOp::CreateLine => {
            let mut local = unsafe { r.read() };
            let (start, velocity) = (local.sprite.start, local.sprite.velocity);
            local.sprite.start = Ge3DPoint::default();
            local.sprite.velocity = Ge3DPoint::default();
            let velocity = if op == EeGfxEffect3DOp::CreateLine { velocity } else { Ge3DPoint::default() };
            plume_new(&mut local, start, g.param1, g.param2, velocity);
            unsafe { r.write(local) };
            0
        }
        // 0xe7049: the media; at the puff, the radius
        EeGfxEffect3DOp::SetUp => {
            let res = h.object_media();
            unsafe {
                set_position(r, (*r).plume.start, h);
                (*r).bounding_radius = g.size;
            }
            res
        }
        // 0xe70b2: the age and the puff's point for the draw, render queue 4
        EeGfxEffect3DOp::Queue => {
            let res = h.base_queue();
            if res == 0 {
                unsafe {
                    let s = sp(r);
                    s.draw_age = (*r).plume.age;
                    s.draw_position = (*r).plume.start;
                }
                h.queue(4);
            }
            res
        }
        // 0xe711b: a smoke sprite growing from the start size and fading with age; 0
        EeGfxEffect3DOp::DrawPass6 => {
            let (s, p) = unsafe { (*sp(r), (*r).plume) };
            let size = (g.size - g.start_size) * s.draw_age + g.start_size;
            let white = EeGfxColor { red: 1.0, green: 1.0, blue: 1.0 };
            x.sprite(
                EE_GFX_SPRITE_BLENDED,
                &s.draw_position,
                white,
                p.rotation,
                size,
                (1.0 - s.draw_age) * g.alpha,
                p.brightness,
                alpha_ref(g),
            );
            0
        }
        // 0xe7243: ages and drifts the puff (`plume_step`); the effect follows a velocity particle
        EeGfxEffect3DOp::Step => {
            let tick = EeGfxPlumeTick {
                tick_seconds: g.tick_seconds,
                tick_time: g.tick_time,
                wind: h.wind(),
                duration: g.duration,
                int_param1: g.int_param1,
                param3: g.param3,
                flag1: g.flag1,
            };
            let mut moved = false;
            let mut local = unsafe { r.read() };
            let res = plume_step(&mut local, &tick, &mut moved);
            unsafe { r.write(local) };
            if moved {
                unsafe { set_position(r, (*r).plume.start, h) };
            }
            res
        }
        _ => 0,
    }
}

/// `Load` (`load`) or `Save` of the parts' kinds after `EEEffect2D`'s: the fire's shown parts, the trail's angle and
/// direction, the muzzle blast's direction, age and rotation (the explosion has none: its C++ returns the base's).
pub fn parts_file<H: Effect2DHost, F: EffectFile>(load: bool, r: &mut EeGfxEffectRecord, f: &mut F, h: &mut H) -> i32 {
    let kind = r.kind;
    let s = &mut r.sprite;
    match kind {
        EE_GFX_EFFECT_KIND_FIRE => {
            if load {
                f.read_i32(&mut s.last_part)
            } else {
                f.write_i32(s.last_part)
            }
        }
        EE_GFX_EFFECT_KIND_EXHAUST_TRAIL => {
            if load {
                let res = f.read_f32(&mut s.angle);
                if res == OK {
                    f.read_bool(&mut s.flag)
                } else {
                    res
                }
            } else {
                let res = f.write_f32(s.angle);
                if res == OK {
                    f.write_bool(s.flag)
                } else {
                    res
                }
            }
        }
        EE_GFX_EFFECT_KIND_SMOKE_PLUME | EE_GFX_EFFECT_KIND_VELOCITY_PARTICLE => {
            if load {
                h.object_media();
                load_plume(r, f)
            } else {
                save_plume(r, f)
            }
        }
        EE_GFX_EFFECT_KIND_SMOKE_MUZZLE_BLAST => {
            if load {
                h.object_media();
                let mut res = load_point(f, &mut s.velocity);
                if res != OK {
                    return res;
                }
                res = f.read_f32(&mut s.age);
                if res == OK {
                    res = f.read_f32(&mut s.rotation);
                }
                res
            } else {
                let mut res = save_point(f, &s.velocity);
                if res != OK {
                    return res;
                }
                res = f.write_f32(s.age);
                if res == OK {
                    res = f.write_f32(s.rotation);
                }
                res
            }
        }
        _ => 0,
    }
}

#[cfg(test)]
#[path = "effect_parts_tests.rs"]
mod tests;
