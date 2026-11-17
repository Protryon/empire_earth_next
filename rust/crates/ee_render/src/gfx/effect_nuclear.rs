//! The nuclear explosion (kind 0x28) and the energy wave (kind 0x47) whole in Rust (`EEE3DNuclearExplosion`,
//! `EEE3DEnergyWave`, deleted).
//!
//! - The nuclear explosion stands on the ground at its unit (its tile's centre when the unit is off the map), its
//!   flash over the effect's duration and its rise over param 1 (ms); drawn in pass 6: the mushroom cloud (part 11)
//!   and its stem (part 9) of the graphic, squashed and lifted as it rises, fading, their textures scrolling apart by
//!   param 2 (the vertex arrays' v shifted for good, as the original), then the additive flash on the standing plane
//!   at the camera's orientation in front of everything, dimming with the flash and the fog; 7 once fully risen.
//! - The energy wave is a ground ring around its unit, growing over param 1 (ms) and starting again, for the effect's
//!   duration; drawn in pass 6 by `AddGroundQuad` at its corners' heights (param 3, 0 over a space tile, taken by the
//!   queue) raised by param 5, not for a cloaked enemy's unit (also taken by the queue).
//!
//! `EEE3DRust` holds the graphic object and answers the world (`EeGfxEffect3DHost`); the draws go through the
//! rasterizer table (`raster.rs`). The record is reached through its raw pointer field by field: the C++ reads it
//! during the host calls (the region move), so no Rust reference to it lives across one.

use crate::ge::types::Ge3DPoint;
use crate::ge::um::floor_unsigned;

use crate::gfx::effect_rising::{EeGfxEffect3DOp, EeGfxGraphicFacts, Effect3DHost};
use crate::gfx::effect_state::{identity, EeGfxEffectRecord, EffectFile};
use crate::gfx::effects::EeGfxColor;
use crate::gfx::lighting::EeGfxLighting;
use crate::gfx::raster::{
    Raster, BLEND_INV_SRC_ALPHA, BLEND_ONE, BLEND_SRC_ALPHA, COMPARE_LESS, COMPARE_LESS_EQUAL, ZBUFFER_TEST_AND_WRITE,
    ZBUFFER_TEST_ONLY,
};

/// `kEffect3DNuclearExplosion`, `kEffect3DEnergyWave`.
pub const EE_GFX_EFFECT_KIND_NUCLEAR_EXPLOSION: i32 = 0x28;
pub const EE_GFX_EFFECT_KIND_ENERGY_WAVE: i32 = 0x47;

/// `kErrorOK`.
const OK: i32 = 0;
/// The graphic's parts: the cloud and the stem (their texture is the graphic's texture 9).
const CLOUD: u32 = 11;
const STEM: u32 = 9;
/// The energy wave's corners' directions (`sEnergyWaveCornerX` / `Y`).
const CORNER_X: [f32; 4] = [-1.0, 1.0, 1.0, -1.0];
const CORNER_Y: [f32; 4] = [1.0, 1.0, -1.0, -1.0];

/// The nuclear explosion's and the energy wave's state beyond the aged record's (the wave's age is `aged.age`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxEffectBlast {
    /// the explosion's `mFlash` (0..1 over the duration) and `mRise` (0..1 over param 1), both saved, and the draw's
    pub flash: f32,
    pub draw_flash: f32,
    pub rise: f32,
    pub draw_rise: f32,
    /// the explosion's texture scroll of the last step, taken (and cleared) by the queue
    pub scroll: f32,
    pub draw_scroll: f32,
    /// the wave's `mPhase` (0..1, saved) and the draw's
    pub phase: f32,
    pub draw_phase: f32,
    /// the wave's corners' heights and whether its unit is hidden (a cloaked enemy's), both taken by the queue
    pub heights: [f32; 4],
    pub hidden: bool,
}

/// A unit's tile facts (the nuclear explosion's place off the map): on the map (`mTileNode`), the class's tile size
/// and the unit's tile, as floats.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxUnitTile {
    pub on_map: bool,
    pub size: f32,
    pub x: f32,
    pub y: f32,
}

/// The two kinds' virtuals.
///
/// # Safety
/// As `effect_rising::effect3d`.
pub(crate) unsafe fn blast<H: Effect3DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY: the function's contract
    if unsafe { (*r).kind } == EE_GFX_EFFECT_KIND_ENERGY_WAVE {
        return unsafe { wave(op, r, g, h, x) };
    }
    // SAFETY (every access below): the function's contract; no reference to the record is kept across a host call
    match op {
        EeGfxEffect3DOp::Create => {
            unsafe {
                (*r).blast.flash = 0.0;
                (*r).blast.rise = 0.0;
            }
            0
        }
        EeGfxEffect3DOp::SetUp => {
            let tile = h.unit_tile();
            let mut p = if tile.on_map {
                h.complex_position()
            } else {
                Ge3DPoint { x: tile.size * 0.5 + tile.x, y: tile.size * 0.5 + tile.y, z: 0.0 }
            };
            p.z = h.terrain_z(p.x, p.y);
            if p.z < 0.0 {
                p.z = 0.0;
            }
            let mut t = identity();
            t.m[0][3] = p.x;
            t.m[1][3] = p.y;
            t.m[2][3] = p.z;
            unsafe { (*r).transformation = t };
            h.move_to();
            unsafe { (*r).bounding_radius = g.size };
            h.load_media()
        }
        EeGfxEffect3DOp::Queue => {
            let result = h.base_queue();
            if result == 0 {
                unsafe {
                    let b = &mut (*r).blast;
                    b.draw_flash = b.flash;
                    b.draw_rise = b.rise;
                    b.draw_scroll = b.scroll;
                    b.scroll = 0.0;
                }
                h.queue(4);
            }
            result
        }
        EeGfxEffect3DOp::DrawPass6 => unsafe { draw_explosion(r, g, h, x) },
        EeGfxEffect3DOp::Step => {
            let elapsed = g.tick_seconds * 1000.0;
            unsafe {
                let b = &mut (*r).blast;
                b.flash += elapsed / g.duration as f32;
                b.rise += elapsed / g.param1;
                b.scroll = elapsed / g.param2;
                if b.flash > 1.0 {
                    b.flash = 1.0;
                }
                if b.rise > 1.0 {
                    7
                } else {
                    0
                }
            }
        }
        _ => 0,
    }
}

/// The explosion's pass 6.
///
/// # Safety
/// As `blast`.
unsafe fn draw_explosion<H: Effect3DHost, R: Raster>(
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    let camera = x.camera();
    let flash = x.standing_plane();
    // SAFETY (every access below): the function's contract
    let (draw_rise, draw_scroll, draw_flash, t) =
        unsafe { ((*r).blast.draw_rise, (*r).blast.draw_scroll, (*r).blast.draw_flash, (*r).transformation) };
    let mut opacity = draw_rise * draw_rise;
    opacity = 1.0 - opacity;
    let cloud = h.model_at(CLOUD);
    let stem = h.model_at(STEM);
    let height = g.start_size;
    let flash_scale = g.size;
    let scroll = opacity * draw_scroll;
    let fall = 1.0 - draw_rise;
    let lift = (1.0 - fall * fall) * height * 0.5;
    x.set_alpha_ref(if g.has_object { g.alpha_ref } else { 0.0 });
    let fog = x.get_fog();
    x.set_zbuffer(ZBUFFER_TEST_AND_WRITE);
    x.set_blending(BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA);
    x.set_shading_source(0);
    x.lighting(EeGfxLighting::Unit);
    let texture = h.texture_at(STEM);
    x.model_texture(cloud, texture);
    x.model_texture(stem, texture);
    let position = Ge3DPoint { x: t.m[0][3], y: t.m[1][3], z: t.m[2][3] };
    let squash = (height - lift) / height;
    let mut c = identity();
    c.scale(squash, squash, 1.0);
    c.m[0][3] += position.x;
    c.m[1][3] += position.y;
    c.m[2][3] += position.z;
    x.model_transform(cloud, &c);
    x.model_scale(cloud, height);
    let mut s = identity();
    s.m[0][3] += position.x;
    s.m[1][3] += position.y;
    s.m[2][3] += position.z;
    x.model_transform(stem, &s);
    x.model_scale(stem, lift + height);
    x.model_opacity(cloud, opacity);
    x.model_opacity(stem, opacity);
    x.model_shift_v(cloud, scroll);
    // v - scroll (the same bits as v + -scroll)
    x.model_shift_v(stem, -scroll);
    x.model_prepare(cloud);
    x.model_draw(cloud, 1.0);
    x.model_prepare(stem);
    x.model_draw(stem, 1.0);
    x.lighting(EeGfxLighting::White);
    let mut light = x.fog_factor(&position, fog);
    light = (1.0 - draw_flash * draw_flash) * light;
    x.set_ambient(EeGfxColor { red: light, green: light, blue: light });
    x.set_blending(BLEND_ONE, BLEND_ONE);
    x.set_shading_source(0);
    x.depth_range(0.0, 0.0);
    x.set_zbuffer(ZBUFFER_TEST_ONLY);
    x.set_zcompare(COMPARE_LESS);
    x.model_color(flash, EeGfxColor { red: 1.0, green: 1.0, blue: 1.0 });
    x.model_material_opacity(flash, 1.0);
    let mut f = camera;
    f.m[0][3] = position.x;
    f.m[1][3] = position.y;
    f.m[2][3] = position.z;
    x.model_transform(flash, &f);
    x.model_scale(flash, flash_scale);
    let texture = h.texture();
    x.model_texture(flash, texture);
    x.model_prepare(flash);
    let result = x.model_draw(flash, 1.0);
    x.set_fog(fog);
    x.depth_range(0.0, 1.0);
    x.set_zbuffer(ZBUFFER_TEST_AND_WRITE);
    x.set_zcompare(COMPARE_LESS_EQUAL);
    result
}

/// The energy wave's virtuals.
///
/// # Safety
/// As `blast`.
unsafe fn wave<H: Effect3DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY (every access below): the function's contract; no reference to the record is kept across a host call
    match op {
        EeGfxEffect3DOp::Create => {
            unsafe {
                (*r).aged.age = 0.0;
                (*r).blast.phase = 0.0;
            }
            0
        }
        EeGfxEffect3DOp::SetUp => {
            let p = h.complex_position();
            let mut t = identity();
            t.m[0][3] = p.x;
            t.m[1][3] = p.y;
            t.m[2][3] = p.z;
            unsafe { (*r).transformation = t };
            h.move_to();
            unsafe { (*r).bounding_radius = g.size };
            h.load_media()
        }
        EeGfxEffect3DOp::Queue => {
            let result = h.base_queue();
            if result == 0 {
                let phase = unsafe {
                    (*r).blast.draw_phase = (*r).blast.phase;
                    (*r).blast.draw_phase
                };
                let hidden = h.unit_hidden();
                unsafe { (*r).blast.hidden = hidden };
                let t = unsafe { (*r).transformation };
                for i in 0..4 {
                    let distance = g.size * phase;
                    let tile_x = floor_unsigned(distance * CORNER_X[i] + t.m[0][3]);
                    let tile_y = floor_unsigned(distance * CORNER_Y[i] + t.m[1][3]);
                    let height = if h.space_tile(tile_x, tile_y) { 0.0 } else { g.param3 };
                    unsafe { (*r).blast.heights[i] = height };
                }
                h.queue(4);
            }
            result
        }
        EeGfxEffect3DOp::DrawPass6 => {
            let (hidden, t, draw_phase, heights) =
                unsafe { ((*r).blast.hidden, (*r).transformation, (*r).blast.draw_phase, (*r).blast.heights) };
            if hidden {
                return 0;
            }
            let center = Ge3DPoint { x: t.m[0][3], y: t.m[1][3], z: g.param5 + t.m[2][3] };
            let reference = if g.has_object { g.alpha_ref } else { 0.0 };
            let white = EeGfxColor { red: 1.0, green: 1.0, blue: 1.0 };
            x.ground_quad(&center, white, 0.0, g.size * draw_phase * 2.0, &heights, g.param4, reference);
            0
        }
        EeGfxEffect3DOp::Step => {
            let elapsed = g.tick_seconds * 1000.0;
            unsafe {
                let b = &mut (*r).blast;
                b.phase += elapsed / g.param1;
                while b.phase >= 1.0 {
                    b.phase -= 1.0;
                }
                (*r).aged.age += elapsed / g.duration as f32;
                if (*r).aged.age >= 1.0 {
                    7
                } else {
                    0
                }
            }
        }
        _ => 0,
    }
}

/// `Load` after the base's (ignored) of either kind: the media (ignored), then the explosion's flash and rise (the
/// first read error) or the wave's age and phase (the phase read's result).
///
/// # Safety
/// `r` is a live record.
pub(crate) unsafe fn load_blast<H: Effect3DHost, F: EffectFile>(
    r: *mut EeGfxEffectRecord,
    f: &mut F,
    h: &mut H,
) -> i32 {
    h.load_media();
    // SAFETY (every access below): the function's contract
    unsafe {
        if (*r).kind == EE_GFX_EFFECT_KIND_ENERGY_WAVE {
            f.read_f32(&mut (*r).aged.age);
            return f.read_f32(&mut (*r).blast.phase);
        }
        let b = &mut (*r).blast;
        let mut res = f.read_f32(&mut b.flash);
        if res == OK {
            res = f.read_f32(&mut b.rise);
        }
        res
    }
}

/// `Save` after the base's (ignored): the explosion's flash and rise (the first write error) or the wave's age and
/// phase (the phase write's result).
pub(crate) fn save_blast<F: EffectFile>(r: &EeGfxEffectRecord, f: &mut F) -> i32 {
    if r.kind == EE_GFX_EFFECT_KIND_ENERGY_WAVE {
        f.write_f32(r.aged.age);
        return f.write_f32(r.blast.phase);
    }
    let mut res = f.write_f32(r.blast.flash);
    if res == OK {
        res = f.write_f32(r.blast.rise);
    }
    res
}

#[cfg(test)]
#[path = "effect_nuclear_tests.rs"]
mod tests;
