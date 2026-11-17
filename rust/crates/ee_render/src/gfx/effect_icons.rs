//! The marching icons (kinds 0x10 circle and 0xf square, `EEE2DMarchingIcons`) whole in Rust on `EEE2DRust` (R4.4,
//! twenty-first round), the class deleted: icons circling a unit while an area effect of a kind is on it, shown to
//! the unit's owner while its type is selected or ghost units are placed, each drawn on the ground where the
//! owner's area effect of that kind does not already cover the spot. The offsets are `effect_state.rs`'s layouts;
//! the C++ host answers the unit, its area effects, the selection and the ground.
//!
//! The original's `mAreaEffectIconCount` (the matching area effect template's id, taken by the constructor and
//! `RepairPointers`) was never read: it is gone with the walk that set it. `mL98` was always 0 (its `PrepareQueue`
//! call was empty).

use ee_base::math::transform::RotationOrder;
use ee_base::math::types::{Ge3DPoint, GeTransformation};

use crate::gfx::effect_2d::Effect2DHost;
use crate::gfx::effect_rising::{EeGfxEffect3DOp, EeGfxGraphicFacts};
use crate::gfx::effect_state::{
    marching_offsets_into, square_radius, EeGfxEffectRecord, EffectFile, EE_GFX_EFFECT_KIND_SQUARE_ICONS, OK,
};
use crate::gfx::lighting::EeGfxLighting;
use crate::gfx::raster::{Raster, BLEND_ONE, BLEND_ZERO, ZBUFFER_TEST_ONLY};

/// The degrees-to-radians factor's first half (0x43fe2c, loaded from memory: `1 / 180`).
const DEGREES_TO_PI: f32 = 1.0 / 180.0;
/// How high above the ground the icons are drawn (`GetHeightOffset`).
const HEIGHT_OFFSET: f32 = 0.05;
/// The most icons a layout makes (the C++'s stack buffer).
const MAX_ICONS: usize = 64;

/// The icons' offsets and the frame's copy (owned by the record).
#[derive(Clone, Debug, Default, PartialEq)]
pub struct EeGfxMarchingLists {
    /// `mOffsets`: the shown icons' offsets from the unit (`LayOutIcons`)
    pub offsets: Vec<Ge3DPoint>,
    /// `mShown`: the queue's copy, drawn
    pub shown: Vec<Ge3DPoint>,
}

/// The marching icons' state.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeGfxEffectMarching {
    /// the offsets (made on first use; owned by the record)
    pub lists: *mut EeGfxMarchingLists,
    /// `mAreaEffectType` (saved)
    pub area_type: i32,
    /// `mLocation`: the unit's position for the draw
    pub location: Ge3DPoint,
    /// `mAngularOffset`, `mRotationSpeed` (radians per second) (saved)
    pub angle: f32,
    pub speed: f32,
    /// `mIconCount` (saved)
    pub count: u32,
    /// `mVisible`
    pub visible: bool,
}

impl Default for EeGfxEffectMarching {
    fn default() -> EeGfxEffectMarching {
        EeGfxEffectMarching {
            lists: std::ptr::null_mut(),
            area_type: 0,
            location: Ge3DPoint::default(),
            angle: 0.0,
            speed: 0.0,
            count: 0,
            visible: false,
        }
    }
}

/// What `IsIconShown` reads of the effect's unit.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxIconUnit {
    /// the effect has a unit
    pub has_unit: bool,
    /// its class is a land building, and its tile size
    pub land_building: bool,
    pub tile_size: i32,
    /// it has a player, and the player's id
    pub has_player: bool,
    pub player: u32,
    /// its complex unit's location's x and y
    pub x: f32,
    pub y: f32,
}

/// The record's lists.
///
/// # Safety
/// `r` is a live record; the reference does not live across a host call.
unsafe fn lists<'a>(r: *mut EeGfxEffectRecord) -> &'a mut EeGfxMarchingLists {
    // SAFETY: the function's contract; the lists are the record's own Box, freed with it
    unsafe {
        if (*r).marching.lists.is_null() {
            (*r).marching.lists = Box::into_raw(Box::default());
        }
        &mut *(*r).marching.lists
    }
}

/// A transformation's translation.
fn translation(t: &GeTransformation) -> Ge3DPoint {
    Ge3DPoint { x: t.m[0][3], y: t.m[1][3], z: t.m[2][3] }
}

/// `IsIconShown` (0xe3cdf): the unit has an owner, and the spot is not already under the owner's area effect of the
/// icons' type (always, with no type).
fn icon_shown<H: Effect2DHost>(area_type: i32, offset: Ge3DPoint, h: &mut H) -> bool {
    let u = h.icon_unit();
    if !u.has_unit || !u.has_player {
        return false;
    }
    let radius = if u.land_building { u.tile_size as f32 * 0.5 } else { 0.0 };
    let x = offset.x + (radius + u.x);
    let y = offset.y + (radius + u.y);
    area_type == 0 || !h.area_effect(area_type, x, y, u.player)
}

/// `LayOutIcons` (0xe42db): the shown offsets at the current angle, the angle advanced by the tick (kept within a
/// turn either way), the effect on the unit.
///
/// # Safety
/// As `marching`.
unsafe fn lay_out<H: Effect2DHost>(r: *mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, h: &mut H) {
    // SAFETY (every access below): the function's contract; no reference to the record lives across a host call
    let (kind, m) = unsafe { ((*r).kind, (*r).marching) };
    unsafe { lists(r).offsets.clear() };
    let count = m.count as i32;
    if count > 0 {
        let mut candidates = [Ge3DPoint::default(); MAX_ICONS];
        let radius = unsafe { (*r).bounding_radius };
        let made = marching_offsets_into(kind, count, m.angle, radius, g.param1, &mut candidates);
        for p in &candidates[..made] {
            if icon_shown(m.area_type, *p, h) {
                unsafe { lists(r).offsets.push(*p) };
            }
        }
    }
    unsafe {
        let m = &mut (*r).marching;
        m.angle += m.speed * (g.tick_time * 0.001);
        if m.angle > 6.283_185_5 {
            m.angle -= 6.283_185_5;
        } else if m.angle < -6.283_185_5 {
            m.angle += 6.283_185_5;
        }
    }
    let p = translation(&h.unit_location());
    unsafe { (*r).position = p };
    h.move_to_point();
}

/// The marching icons' virtuals.
///
/// # Safety
/// As `effect_2d::effect2d`.
pub unsafe fn marching<H: Effect2DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY (every access below): the function's contract; no reference to the record lives across a host call
    match op {
        // 0xe3b1d on the unit: the icon count, the spin (degrees per second) and the area effect type from the
        // graphic effect, hidden; the radius: the circle's param 1, the square's half diagonal
        EeGfxEffect3DOp::Create => {
            unsafe {
                let kind = (*r).kind;
                let m = &mut (*r).marching;
                m.count = g.int_param1 as u32;
                m.angle = 0.0;
                m.speed = g.int_param2 as f32 * DEGREES_TO_PI * std::f32::consts::PI;
                m.area_type = g.int_param3;
                m.visible = false;
                (*r).bounding_radius =
                    if kind == EE_GFX_EFFECT_KIND_SQUARE_ICONS { square_radius(g.param1) } else { g.param1 };
            }
            0
        }
        // 0xe3df8: the graphic's media
        EeGfxEffect3DOp::SetUp => h.object_media(),
        // 0xe3c32: the unit's position and the offsets for the draw, render queue 4, while visible; 10 while hidden
        EeGfxEffect3DOp::Queue => {
            let mut res = h.base_queue();
            unsafe { lists(r).shown.clear() };
            if res == 0 {
                if unsafe { (*r).marching.visible } {
                    let p = translation(&h.unit_location());
                    unsafe {
                        (*r).marching.location = p;
                        let l = lists(r);
                        l.shown.clone_from(&l.offsets);
                    }
                    h.queue(4);
                } else {
                    res = 10;
                }
            }
            res
        }
        // 0xe3e92: an opaque fog-lit icon on the ground at each shown offset on a map tile
        EeGfxEffect3DOp::DrawPass6 => unsafe { draw(r, g, h, x) },
        // 0xe41ef: 7 when the unit is dead or an area effect is pending (counted down); else shown and turned while
        // its built unit is the current player's and its type is selected (or ghost units are placed)
        EeGfxEffect3DOp::Step => {
            let selected = h.icons_selected();
            let end = if g.unit_hit_points <= 0 {
                true
            } else {
                let pending = h.area_effects();
                if pending > 0 {
                    h.set_area_effects(pending - 1);
                    true
                } else {
                    false
                }
            };
            if end {
                unsafe { (*r).marching.visible = false };
                return 7;
            }
            if h.icons_owner() && (selected || h.ghost_units() > 0) {
                unsafe {
                    lay_out(r, g, h);
                    (*r).marching.visible = true;
                }
            } else {
                unsafe { (*r).marching.visible = false };
            }
            0
        }
        _ => 0,
    }
}

/// The icons' pass 6 (0xe3e92): the draw's result is the last icon's (0 with none drawn).
///
/// # Safety
/// As `marching`.
unsafe fn draw<H: Effect2DHost, R: Raster>(
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    let model = h.model();
    let mut scale = g.size;
    x.set_alpha_ref(if g.has_object { g.alpha_ref } else { 0.0 });
    let fog = x.get_fog();
    if scale <= 0.0 {
        scale = 1.0;
    }
    x.set_zbuffer(ZBUFFER_TEST_ONLY);
    x.set_blending(BLEND_ONE, BLEND_ZERO);
    x.lighting(EeGfxLighting::White);
    let texture = h.object_texture(0);
    x.model_texture(model, texture);
    // SAFETY: the function's contract (the copies are taken before any host call)
    let (location, shown) = unsafe { ((*r).marching.location, lists(r).shown.clone()) };
    let mut t = GeTransformation::IDENTITY;
    let mut res = 0;
    for offset in shown {
        t.set_orientation(RotationOrder::Ypr, x.camera_yaw() * 0.5, 0.0, 0.0);
        let at = Ge3DPoint { x: location.x + offset.x, y: location.y + offset.y, z: location.z + offset.z };
        let ground = h.ground_point(at);
        if ground.has {
            let mut p = ground.point;
            p.z += HEIGHT_OFFSET;
            let value = x.fog_factor(&p, fog);
            x.set_ambient(crate::gfx::effects::EeGfxColor { red: value, green: value, blue: value });
            t.m[0][3] = p.x;
            t.m[1][3] = p.y;
            t.m[2][3] = p.z;
            x.model_transform(model, &t);
            x.model_scale(model, scale);
            x.model_prepare(model);
            res = x.model_draw(model, 1.0);
        }
    }
    x.set_fog(fog);
    res
}

/// `Load` (`load`) or `Save` of the icons after `EEEffect2D`'s (the C++ returns a failed base save itself; a load
/// ignores the base's): the media (a load), the angle, speed, icon count and area effect type.
pub fn marching_file<H: Effect2DHost, F: EffectFile>(
    load: bool,
    r: &mut EeGfxEffectRecord,
    f: &mut F,
    h: &mut H,
) -> i32 {
    let m = &mut r.marching;
    if load {
        // 0xe3e0f: no type until it reads one
        m.area_type = 0;
        let mut res = h.object_media();
        if res != OK {
            return res;
        }
        res = f.read_f32(&mut m.angle);
        if res != OK {
            return res;
        }
        res = f.read_f32(&mut m.speed);
        if res != OK {
            return res;
        }
        res = f.read_u32(&mut m.count);
        if res != OK {
            return res;
        }
        let mut kind = 0u32;
        res = f.read_u32(&mut kind);
        if res == OK {
            m.area_type = kind as i32;
        }
        res
    } else {
        // 0xe4182
        let mut res = f.write_f32(m.angle);
        if res != OK {
            return res;
        }
        res = f.write_f32(m.speed);
        if res != OK {
            return res;
        }
        res = f.write_u32(m.count);
        if res == OK {
            res = f.write_u32(m.area_type as u32);
        }
        res
    }
}

#[cfg(test)]
#[path = "effect_icons_tests.rs"]
mod tests;
