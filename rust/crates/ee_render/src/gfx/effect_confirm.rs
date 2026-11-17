//! The command confirmation (kind 0x41) whole in Rust on `EEE2DRust` (`EEE3DCommandConfirmation`, deleted: despite
//! its name an `EEEffect2D`): the ring under a commanded unit or at a commanded point.
//! - Made on a unit for one of its goal queues (state 2, no size), or at a point (moved there).
//! - Set up: the state is int param 1; the colour by the state (4 attack: yellow, red on an enemy's unit; 3 green;
//!   2 yellow; 1 red; 0 the unit's goal: red where it cannot stand on a seen tile, green for a forced move, else
//!   orange; without a goal yellow while unloading a transport, else pink); on a unit the sizes are its selection
//!   circle's (x 1.25: the start size a fraction of it); the graphic's media loaded (its result ignored).
//! - Each step it ages by the game speed (ms); a unit's ring (not state 0) follows the unit (a tree: an id-based spot
//!   on its tile on the ground; a unit without hit points: its model's top point); its animation 0 advances (looping
//!   with flag 1, else 7 at its end); 7 past the duration.
//! - Queue 2 (after `EEEffect::Queue`) with its age, frame and state; pass 4: the ring at its place facing the camera
//!   (yaw - pi/4), swelling and fading on a half sine of its life, without fog, z tested less, alpha blended, white
//!   lit, in its colour, with the command texture for its state (state 3's animated); fog and z state restored.
//! - Not saved: Load, Save and RepairPointers return 0 (not even the base's).

use crate::ge::transform::RotationOrder;
use crate::ge::types::{Ge3DPoint, GeTransformation};
use crate::ge::um;

use crate::gfx::effect_2d::Effect2DHost;
use crate::gfx::effect_rising::{EeGfxEffect3DOp, EeGfxGraphicFacts};
use crate::gfx::effect_state::EeGfxEffectRecord;
use crate::gfx::effects::EeGfxColor;
use crate::gfx::lighting::EeGfxLighting;
use crate::gfx::raster::{Raster, BLEND_INV_SRC_ALPHA, BLEND_SRC_ALPHA, COMPARE_LESS, ZBUFFER_TEST_ONLY};

/// `kEffect3DCommandConfirmation`
pub const EE_GFX_EFFECT_KIND_COMMAND_CONFIRMATION: i32 = 0x41;

const GREEN: EeGfxColor = EeGfxColor { red: 0.37, green: 1.0, blue: 0.37 };
const ORANGE: EeGfxColor = EeGfxColor { red: 0.94, green: 0.61, blue: 0.26 };
const RED: EeGfxColor = EeGfxColor { red: 1.0, green: 0.15, blue: 0.15 };
const YELLOW: EeGfxColor = EeGfxColor { red: 1.0, green: 1.0, blue: 0.1 };
const PINK: EeGfxColor = EeGfxColor { red: 1.0, green: 0.5, blue: 1.0 };
/// `GEColor()`
const WHITE: EeGfxColor = EeGfxColor { red: 1.0, green: 1.0, blue: 1.0 };

/// The command confirmation's state.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxEffectConfirm {
    /// `mColor`
    pub color: EeGfxColor,
    /// `mState`: 0 the unit's goal, 1..4 fixed colours; `mDrawState` the queue's copy
    pub state: i32,
    pub draw_state: i32,
    /// `mAge` (ms) and the queue's copy
    pub age: f32,
    pub draw_age: f32,
    pub start_size: f32,
    pub end_size: f32,
    /// `mTime`: ms into the frame
    pub time: f32,
    pub frame: u32,
    pub draw_frame: u32,
    /// `mGoalIndex`: the unit's goal queue
    pub goal_index: u32,
}

/// The goal a state-0 confirmation shows.
#[repr(u32)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum EeGfxConfirmGoal {
    /// no unit or no complex unit
    #[default]
    NoUnit = 0,
    /// a location goal on a tile the unit cannot stand on and its player sees
    Blocked = 1,
    /// a forced move
    Forced = 2,
    /// another location goal
    Move = 3,
    /// no location goal, unloading a transport
    Unloading = 4,
    /// no location goal
    Other = 5,
}

/// Where the unit stands for the confirmation: its position; a tree's id, class tile size and tile; else whether it
/// has no hit points, a complex unit and a model (`dead_top`) and then the model's top point and the scale.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxConfirmUnit {
    pub position: Ge3DPoint,
    pub tree: bool,
    pub id: i32,
    pub tile_size: i32,
    pub tile_x: i32,
    pub tile_y: i32,
    pub dead_top: bool,
    pub top: Ge3DPoint,
    pub scale: f32,
}

/// The sizes of a ring on a unit: its selection circle's.
fn unit_sizes<H: Effect2DHost>(c: &mut EeGfxEffectConfirm, h: &mut H) {
    let selection = h.selection_size();
    c.start_size = selection * 1.25 * c.start_size;
    c.end_size = selection * 1.25;
}

/// `SetUp`.
///
/// # Safety
/// As `confirm`.
unsafe fn set_up<H: Effect2DHost>(r: *mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, h: &mut H) -> i32 {
    // SAFETY (every access below): the function's contract; no reference lives across a host call
    let c = unsafe { &mut (*r).confirm };
    c.start_size = g.start_size;
    let height = g.size;
    let state = g.int_param1;
    c.end_size = g.size;
    h.load_media();
    let c = unsafe { &mut (*r).confirm };
    c.state = state;
    unsafe { (*r).bounding_radius = height };
    match state {
        4 => {
            unsafe { (*r).confirm.color = YELLOW };
            if g.has_unit {
                unit_sizes(unsafe { &mut (*r).confirm }, h);
                if h.confirm_enemy() {
                    unsafe { (*r).confirm.color = RED };
                }
            }
        }
        1..=3 => {
            unsafe { (*r).confirm.color = [RED, YELLOW, GREEN][state as usize - 1] };
            if g.has_unit {
                unit_sizes(unsafe { &mut (*r).confirm }, h);
            }
        }
        0 => {
            let (index, position) = unsafe { ((*r).confirm.goal_index, (*r).position) };
            let goal = h.confirm_goal(index, um::floor(position.x), um::floor(position.y));
            if goal != EeGfxConfirmGoal::NoUnit && index == 0 {
                unit_sizes(unsafe { &mut (*r).confirm }, h);
            }
            unsafe {
                (*r).confirm.color = match goal {
                    EeGfxConfirmGoal::Blocked => RED,
                    EeGfxConfirmGoal::Forced => GREEN,
                    EeGfxConfirmGoal::Move => ORANGE,
                    EeGfxConfirmGoal::Unloading => YELLOW,
                    EeGfxConfirmGoal::NoUnit | EeGfxConfirmGoal::Other => PINK,
                }
            };
        }
        _ => {}
    }
    let start = h.frames(0).start;
    let c = unsafe { &mut (*r).confirm };
    c.time = 0.0;
    c.frame = start;
    c.draw_frame = 0;
    0
}

/// `Step`.
///
/// # Safety
/// As `confirm`.
unsafe fn step<H: Effect2DHost>(r: *mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, h: &mut H) -> i32 {
    let frames = h.frames(0);
    // SAFETY (every access below): the function's contract; no reference lives across a host call
    let (current, state) = unsafe { ((*r).confirm.frame, (*r).confirm.state) };
    let mut frame = current;
    let age = unsafe {
        (*r).confirm.age += g.speed * 1000.0;
        (*r).confirm.age
    };
    let mut time = g.tick_time + unsafe { (*r).confirm.time };
    if !(age <= g.duration as f32) {
        return 7;
    }
    let mut res = 0;
    if state != 0 && g.has_unit {
        let u = h.confirm_unit();
        let mut at = u.position;
        if u.tree {
            let (id, size) = (u.id, u.tile_size);
            at.x = (id % 3 - 1).wrapping_mul(size) as f32 * 0.125 + size as f32 * 0.5 + u.tile_x as f32;
            at.y = (id / 3 % 3 - 1).wrapping_mul(size) as f32 * 0.125 + size as f32 * 0.5 + u.tile_y as f32;
            at.z = h.terrain_z(at.x, at.y);
        } else if u.dead_top {
            let offset = Ge3DPoint { x: u.top.x * u.scale, y: u.top.y * u.scale, z: u.top.z };
            at = h.unit_location().apply_point(offset);
        }
        unsafe { (*r).position = at };
        h.move_to_point();
    }
    if time >= frames.time as f32 {
        let n = um::lrintf(time / frames.time as f32);
        time -= (n as u32).wrapping_mul(frames.time) as f32;
        frame = (n as u32).wrapping_add(current);
        if frame > frames.last {
            if g.flag1 {
                frame = frames.start;
            } else {
                res = 7;
            }
        }
    }
    if res == 0 {
        let c = unsafe { &mut (*r).confirm };
        c.frame = frame;
        c.time = time;
    }
    res
}

/// `DrawPass4`.
///
/// # Safety
/// As `confirm`.
unsafe fn draw<H: Effect2DHost, R: Raster>(
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    let model = h.model();
    // SAFETY: the function's contract
    let (c, position) = unsafe { ((*r).confirm, (*r).position) };
    // clamped as the original (a NaN stays NaN, as f32::clamp keeps it)
    let value = um::um_cos(1.5707964 - c.draw_age / g.duration as f32 * 3.1415927).clamp(0.0, 1.0);
    let size = (c.end_size - c.start_size) * value + c.start_size;
    if !(size > 0.0) {
        return 0;
    }
    x.set_alpha_ref(if g.has_object { g.alpha_ref } else { 0.0 });
    let fog = x.get_fog();
    let zbuffer = x.get_zbuffer();
    let zcompare = x.get_zcompare();
    x.set_fog(0);
    x.set_zbuffer(ZBUFFER_TEST_ONLY);
    x.set_zcompare(COMPARE_LESS);
    x.set_blending(BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA);
    x.set_shading_source(0);
    x.lighting(EeGfxLighting::White);
    let mut t = GeTransformation::default();
    t.m[0][3] = position.x;
    t.m[1][3] = position.y;
    t.m[2][3] = position.z;
    t.set_orientation(RotationOrder::Ypr, x.camera_yaw() - 0.7853982, 0.0, 0.0);
    t.m[3] = [0.0, 0.0, 0.0, 1.0];
    x.model_color(model, c.color);
    x.model_transform(model, &t);
    x.model_scale(model, size);
    x.model_opacity(model, value);
    x.model_prepare(model);
    if c.draw_state == 0 {
        let texture = h.command_texture(1);
        x.model_texture(model, texture);
    } else if c.draw_state == 3 {
        let texture = h.command_texture(2);
        x.texture_frame(texture, c.draw_frame);
        x.model_texture(model, texture);
    } else {
        let texture = h.command_texture(0);
        x.model_texture(model, texture);
    }
    let res = x.model_draw(model, 1.0);
    x.set_fog(fog);
    x.set_zbuffer(zbuffer);
    x.set_zcompare(zcompare);
    res
}

/// The command confirmation's virtuals.
///
/// # Safety
/// As `effect_2d::effect2d`.
pub unsafe fn confirm<H: Effect2DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    match op {
        EeGfxEffect3DOp::Create | EeGfxEffect3DOp::CreateAt => {
            if op == EeGfxEffect3DOp::CreateAt {
                h.move_to_point();
            }
            // SAFETY (every access below): the function's contract
            let c = unsafe { &mut (*r).confirm };
            c.age = 0.0;
            c.state = 2;
            c.start_size = 0.0;
            c.end_size = 0.0;
            c.color = WHITE;
            0
        }
        EeGfxEffect3DOp::SetUp => unsafe { set_up(r, g, h) },
        EeGfxEffect3DOp::Queue => {
            let res = h.base_queue();
            if res == 0 {
                let c = unsafe { &mut (*r).confirm };
                c.draw_age = c.age;
                c.draw_frame = c.frame;
                c.draw_state = c.state;
                h.queue(2);
            }
            res
        }
        EeGfxEffect3DOp::DrawPass4 => unsafe { draw(r, g, h, x) },
        EeGfxEffect3DOp::Step => unsafe { step(r, g, h) },
        _ => 0,
    }
}

#[cfg(test)]
#[path = "effect_confirm_tests.rs"]
mod tests;
