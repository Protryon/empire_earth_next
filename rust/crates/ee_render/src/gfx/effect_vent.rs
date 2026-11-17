//! The damage vent (kind 0x35) whole in Rust (`EEE3DDamageVent`, deleted): jets of smoke or fire from a damaged
//! unit's vent tag points (kind 5, aimed at the same index of kind 0x38), more of them the lower its hit points.
//! - Every 2 s the number of jets is matched to the damage below param 2 (one per vent at most; a new jet on a free
//!   vent from a random one, its speed random in param 3..4).
//! - Each step the jets grow or shrink towards the damage scale times their speed (by param 5 a second), animate
//!   through the graphic's animation 0, follow the unit (re-aimed each step under flag 1) and, with a linked effect 1,
//!   place it near their vent every param 6..7 ms (spread by param 8). 7 once the unit is dead, healed or gone.
//! - Queue 4 with the jets' draw copies; pass 6: each jet's model scaled by its length at its frame, additive under
//!   flag 2 (fog factor read at the effect, which turns the fog off) else alpha blended, white lit; the fog restored.
//! - Load: the base's result ignored, the media's; Save: the base's.
//!
//! The jets are a Rust list the record owns (`vent.parts`, made on first use, freed with the record). The C++ host
//! answers the tag points (`vent_lists`, `vent_count`, `vent_offsets`, `vent_points`) and places the sub-effects.

use crate::ge::types::{Ge3DPoint, GeTransformation, GeVector};
use crate::ge::um;

use crate::gfx::effect_rising::{EeGfxEffect3DOp, EeGfxGraphicFacts, Effect3DHost};
use crate::gfx::effect_state::{identity, random, random_float, random_long, EeGfxEffectRecord};
use crate::gfx::lighting::EeGfxLighting;
use crate::gfx::raster::{Raster, BLEND_INV_SRC_ALPHA, BLEND_ONE, BLEND_SRC_ALPHA, ZBUFFER_TEST_ONLY};

/// `kEffect3DDamageVent`
pub const EE_GFX_EFFECT_KIND_DAMAGE_VENT: i32 = 0x35;

/// ms between two matches of the jets to the damage.
const COUNT_CHECK_TIME: f32 = 2000.0;

/// The jets' base orientation (x stays, y -> z, z -> -y).
const VENT_ROTATION: [[f32; 4]; 4] =
    [[1.0, 0.0, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0], [0.0, -1.0, 0.0, 0.0], [0.0, 0.0, 0.0, 1.0]];

/// One jet (`EEE3DDamageVentPart`).
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeGfxVentPart {
    /// the vent's tag point index
    pub tag: i32,
    /// the unit's location moved along the jet
    pub transformation: GeTransformation,
    pub speed: f32,
    /// the current length
    pub length: f32,
    pub frame: u32,
    /// ms left in the frame
    pub time: f32,
    /// ms to the next sub-effect
    pub next_sub: f32,
    /// the orientation from the vent towards its aim (`Aim`)
    pub orientation: GeTransformation,
    /// the queue's copies
    pub draw_length: f32,
    pub draw_frame: u32,
    pub draw_transformation: GeTransformation,
    pub draw_orientation: GeTransformation,
}

impl EeGfxVentPart {
    /// A new jet on `tag` (the implicit constructor's identities; the C++ left the draw copies unset).
    fn new(tag: i32, speed: f32) -> EeGfxVentPart {
        EeGfxVentPart {
            tag,
            transformation: identity(),
            speed,
            length: 0.0,
            frame: 0,
            time: 0.0,
            next_sub: 0.0,
            orientation: identity(),
            draw_length: 0.0,
            draw_frame: 0,
            draw_transformation: identity(),
            draw_orientation: identity(),
        }
    }
}

/// The jets (`mParts`).
#[derive(Debug, Default)]
pub struct EeGfxVentParts {
    pub list: Vec<EeGfxVentPart>,
}

/// The damage vent's state.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeGfxEffectVent {
    /// `mParts` (made on first use; owned by the record)
    pub parts: *mut EeGfxVentParts,
    /// `mDrawCount`: the queue's jet count
    pub draw_count: u32,
    /// `mNextCountCheckTime`: ms to the next match of the jets to the damage
    pub next_count_time: f32,
}

impl Default for EeGfxEffectVent {
    fn default() -> EeGfxEffectVent {
        EeGfxEffectVent { parts: std::ptr::null_mut(), draw_count: 0, next_count_time: 0.0 }
    }
}

/// The damage vent's unit: its command is to die; its maximum hit points.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxVentUnit {
    pub dead: bool,
    pub max_hit_points: f32,
}

/// A jet's places on the unit (the C++'s tag points at frame 0 around the unit's location scaled by its stats): its
/// vent, its aim, and the vent's offset from the model's centre turned by that location.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxVentPoints {
    pub from: Ge3DPoint,
    pub to: Ge3DPoint,
    pub translation: GeVector,
}

/// A graphic object's animation `index`: its start frame, frame time (ms), frame count and last frame.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxFrames {
    pub start: u32,
    pub time: u32,
    pub count: u32,
    pub last: u32,
}

/// The record's jets (made on first use).
///
/// # Safety
/// `r` is a live record; no other reference to its jets lives.
pub(crate) unsafe fn parts<'a>(r: *mut EeGfxEffectRecord) -> &'a mut Vec<EeGfxVentPart> {
    // SAFETY: the function's contract; the list is the record's own Box
    unsafe {
        if (*r).vent.parts.is_null() {
            (*r).vent.parts = Box::into_raw(Box::default());
        }
        &mut (*(*r).vent.parts).list
    }
}

/// `Aim`: the jet's orientation from its vent's offset towards its aim's, turned to the jet axis.
fn aim<H: Effect3DHost>(tag: i32, h: &mut H) -> GeTransformation {
    let (from, to) = h.vent_offsets(tag);
    let rotation = GeTransformation { m: VENT_ROTATION };
    let mut look = GeTransformation::default();
    look.make_look_at(from, to);
    look.compose(&rotation)
}

/// `Step` while the unit lives damaged: follows the unit, matches the jets to the damage every 2 s, moves them.
///
/// # Safety
/// As `damage_vent`.
unsafe fn step<H: Effect3DHost>(r: *mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, health: f32, h: &mut H) -> i32 {
    let position = h.unit_position();
    let mut t = identity();
    t.m[0][3] = position.x;
    t.m[1][3] = position.y;
    t.m[2][3] = position.z;
    t.set_orientation(crate::ge::transform::RotationOrder::Ypr, 0.0, 0.0, 0.0);
    t.m[3] = [0.0, 0.0, 0.0, 1.0];
    // SAFETY (every access below): the function's contract; no reference to the record lives across a host call
    unsafe { (*r).transformation = t };
    h.move_to();
    let elapsed = g.tick_time;
    let due = unsafe {
        (*r).vent.next_count_time -= elapsed;
        (*r).vent.next_count_time <= 0.0
    };
    if due {
        unsafe { (*r).vent.next_count_time = COUNT_CHECK_TIME };
        let count = h.vent_count();
        let have = unsafe { parts(r) }.len() as u32;
        let mut wanted = um::floor((g.param2 - health) * count as f32 / g.param2).wrapping_add(1) as u32;
        if wanted > count {
            wanted = count;
        }
        if have > wanted {
            let list = unsafe { parts(r) };
            for _ in 0..have - wanted {
                list.pop();
            }
        } else if have < wanted {
            let mut tag = random_long(0, count.wrapping_sub(1) as i32);
            let list = unsafe { parts(r) };
            let mut other = 0;
            while other < list.len() {
                if tag != list[other].tag {
                    other += 1;
                } else {
                    tag += 1;
                    if tag == count as i32 {
                        tag = 0;
                    }
                    other = 0;
                }
            }
            let mut part = EeGfxVentPart::new(tag, random_float(g.param3, g.param4));
            if !g.flag1 {
                part.orientation = aim(tag, h);
            }
            unsafe { parts(r) }.push(part);
        }
    }
    let n = unsafe { parts(r) }.len();
    if n == 0 {
        return 0;
    }
    let scale = (g.param2 - health) / g.param2 * (g.size - g.start_size) + g.start_size;
    for i in 0..n {
        let tag = {
            let p = &mut unsafe { parts(r) }[i];
            let target = scale * p.speed;
            if target > p.length {
                p.length += g.param5 * elapsed / 1000.0;
                if p.length > target {
                    p.length = target;
                }
            } else if target < p.length {
                p.length -= g.param5 * elapsed / 1000.0;
                if p.length < target {
                    p.length = target;
                }
            }
            p.time -= elapsed;
            p.tag
        };
        if unsafe { parts(r) }[i].time <= 0.0 {
            let frames = h.frames(0);
            let p = &mut unsafe { parts(r) }[i];
            p.frame = p.frame.wrapping_add(1);
            if p.frame >= frames.count {
                p.frame = 0;
            }
            p.time = frames.time as f32;
        }
        if g.flag1 {
            let orientation = aim(tag, h);
            let list = unsafe { parts(r) };
            list[i].orientation = orientation;
        }
        let location = h.unit_location();
        let points = h.vent_points(tag);
        let length = unsafe { parts(r) }[i].length;
        let mut direction =
            GeVector::new(points.to.x - points.from.x, points.to.y - points.from.y, points.to.z - points.from.z);
        direction.normalize();
        direction.x = direction.x * length;
        direction.y = direction.y * length;
        direction.z = direction.z * length;
        let mut moved = location;
        moved.m[0][3] += direction.x;
        moved.m[1][3] += direction.y;
        moved.m[2][3] += direction.z;
        moved.m[0][3] += points.translation.x;
        moved.m[1][3] += points.translation.y;
        moved.m[2][3] += points.translation.z;
        let list = unsafe { parts(r) };
        list[i].transformation = moved;
        if !g.has_linked1 {
            continue;
        }
        let place = {
            let p = &mut unsafe { parts(r) }[i];
            p.next_sub -= elapsed;
            if !(p.next_sub <= 0.0) {
                continue;
            }
            p.next_sub = random_float(g.param6, g.param7);
            let spread = g.param8;
            let z = (0.5 - random()) * spread + points.from.z;
            let y = (0.5 - random()) * spread + points.from.y;
            let x = (0.5 - random()) * spread + points.from.x;
            Ge3DPoint { x, y, z }
        };
        h.place_linked(&place);
    }
    0
}

/// The damage vent's virtuals.
///
/// # Safety
/// As `effect_rising::effect3d`.
pub unsafe fn damage_vent<H: Effect3DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    match op {
        EeGfxEffect3DOp::SetUp => {
            let res = h.load_media();
            let radius = h.unit_class_scale() + g.size;
            // SAFETY: the function's contract
            unsafe { (*r).bounding_radius = radius };
            res
        }
        EeGfxEffect3DOp::Queue => {
            let res = h.base_queue();
            if res == 0 {
                // SAFETY: the function's contract; the list is the record's
                let list = unsafe { parts(r) };
                for p in list.iter_mut() {
                    p.draw_length = p.length;
                    p.draw_frame = p.frame;
                    p.draw_transformation = p.transformation;
                    p.draw_orientation = p.orientation;
                }
                let n = list.len() as u32;
                unsafe { (*r).vent.draw_count = n };
                h.queue(4);
            }
            res
        }
        EeGfxEffect3DOp::DrawPass6 => {
            let model = h.model();
            let texture = h.texture();
            let mut res = 0;
            x.set_alpha_ref(if g.has_object { g.alpha_ref } else { 0.0 });
            let fog = x.get_fog();
            x.set_zbuffer(ZBUFFER_TEST_ONLY);
            x.set_shading_source(0);
            if g.flag2 {
                x.set_blending(BLEND_ONE, BLEND_ONE);
            } else {
                x.set_blending(BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA);
            }
            x.lighting(EeGfxLighting::White);
            x.model_own_texture(model, texture);
            if g.flag2 {
                // SAFETY: the function's contract
                let t = unsafe { (*r).transformation };
                x.fog_factor(&Ge3DPoint { x: t.m[0][3], y: t.m[1][3], z: t.m[2][3] }, fog);
            }
            let count = unsafe { (*r).vent.draw_count } as usize;
            let mut i = 0;
            while i < count && i < unsafe { parts(r) }.len() {
                let p = unsafe { parts(r) }[i];
                x.model_pre_transform(model, &p.draw_orientation);
                let mut scale = GeTransformation::default();
                scale.set_scale(p.draw_length, p.draw_length, p.draw_length);
                x.model_transform(model, &p.draw_transformation.compose(&scale));
                x.texture_frame(texture, p.draw_frame);
                x.model_prepare(model);
                res = x.model_draw(model, 1.0);
                i += 1;
            }
            x.set_fog(fog);
            res
        }
        EeGfxEffect3DOp::Step => {
            if !g.has_unit {
                return 7;
            }
            h.vent_lists();
            let unit = h.vent_unit();
            let mut health = g.unit_hit_points as f32;
            health /= unit.max_hit_points;
            if !unit.dead && health < g.param2 {
                // SAFETY: the function's contract
                unsafe { step(r, g, health, h) }
            } else {
                7
            }
        }
        _ => 0,
    }
}

#[cfg(test)]
#[path = "effect_vent_tests.rs"]
mod tests;
