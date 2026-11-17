//! The light glow (kind 0xd, `EEE2DLightGlow`) whole in Rust on `EEE2DRust` (R4.4, twenty-first round), the class
//! deleted: a coloured glow sprite on each of a unit's light tag points (kind 0xd), blinking by the timing in the
//! tag's name (`light_<red>_<green>_<blue>_<on ms>_<period ms>_<start ms>`), drawn while the unit is within two tiles
//! of the visible terrain. The spots are a list the record owns; the C++ host answers the unit's tags, their names
//! and points, and the visible terrain.
//!
//! Not kept: the original built a shared "light glow" model (two nested quads) on its first set-up and released it
//! at shutdown, but never drew it. A tag name's field that does not scan leaves the value at 0 (the colour's at the
//! previous spot's: the original read an uninitialised local or the spot array's garbage).

use ee_base::math::types::Ge3DPoint;

use crate::gfx::effect_2d::Effect2DHost;
use crate::gfx::effect_rising::{EeGfxEffect3DOp, EeGfxGraphicFacts};
use crate::gfx::effect_state::{EeGfxEffectRecord, EffectFile};
use crate::gfx::effects::EeGfxColor;
use crate::gfx::raster::{Raster, EE_GFX_SPRITE_GLOW};

/// `kEffect2DLightGlow`.
pub const EE_GFX_EFFECT_KIND_LIGHT_GLOW: i32 = 0xd;
/// `GEColor()`.
const WHITE: EeGfxColor = EeGfxColor { red: 1.0, green: 1.0, blue: 1.0 };

/// One glow (`EEE2DLightGlowSpot`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxGlowSpot {
    /// `mPosition` and the queue's copy
    pub position: Ge3DPoint,
    pub draw_position: Ge3DPoint,
    /// `mColor` (the name's 0..255 over 255)
    pub color: EeGfxColor,
    /// `mOnTime`, `mPeriod`, `mStart` (ms): lit while `(age - start) % period <= on`
    pub on_time: u32,
    pub period: u32,
    pub start: u32,
    /// `mVisible` and the queue's copy
    pub visible: bool,
    pub draw_visible: bool,
}

/// The glows (owned by the record).
#[derive(Clone, Debug, Default, PartialEq)]
pub struct EeGfxGlowSpots {
    pub list: Vec<EeGfxGlowSpot>,
}

/// The light glow's state.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeGfxEffectGlow {
    /// the spots (made by the set-up; owned by the record)
    pub spots: *mut EeGfxGlowSpots,
    /// `mAge` (ms; saved) and the queue's fade-in `mDrawAge` (0..1)
    pub age: f32,
    pub draw_age: f32,
    /// `mHidden`: the unit is off the visible terrain
    pub hidden: bool,
}

impl Default for EeGfxEffectGlow {
    fn default() -> EeGfxEffectGlow {
        EeGfxEffectGlow { spots: std::ptr::null_mut(), age: 0.0, draw_age: 0.0, hidden: false }
    }
}

/// The set-up's unit: its light tags (0 without a list or tags; the host keeps the list for the names) and where it
/// stands (its complex unit's location, else its tile's centre on the terrain).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxGlowSetUp {
    pub count: u32,
    pub position: Ge3DPoint,
}

/// The glows' placing: whether they show (not for another player's cloaked unit or an unfinished land building),
/// whether the unit has light tags (the host keeps the list, the frame and the scaled location for `glow_point`),
/// and where it stands.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxGlowPlace {
    pub show: bool,
    pub has_tags: bool,
    pub position: Ge3DPoint,
}

/// The visible terrain's bottom-left tile and size, and the unit's tile.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxGlowArea {
    pub x: u32,
    pub y: u32,
    pub width: u32,
    pub height: u32,
    pub tile_x: i32,
    pub tile_y: i32,
}

/// The record's spots (empty until the set-up).
///
/// # Safety
/// `r` is a live record; the reference does not live across a host call.
unsafe fn spots<'a>(r: *mut EeGfxEffectRecord) -> &'a mut Vec<EeGfxGlowSpot> {
    // SAFETY: the function's contract; the list is the record's own Box, freed with it
    unsafe {
        if (*r).glow.spots.is_null() {
            (*r).glow.spots = Box::into_raw(Box::default());
        }
        &mut (*(*r).glow.spots).list
    }
}

/// `UString::Find("_", start)`: the position, or the length when there is none.
fn find(name: &[u8], start: u32) -> u32 {
    let length = name.len() as u32;
    if start >= length {
        return length;
    }
    name[start as usize..].iter().position(|&c| c == b'_').map_or(length, |i| start + i as u32)
}

/// The bytes `UString::Copy(name, start, count, 0, false)` leaves for `sscanf` (to the name's end at most).
fn field(name: &[u8], start: u32, count: u32) -> &[u8] {
    let start = (start as usize).min(name.len());
    let end = start.saturating_add(count as usize).min(name.len());
    &name[start..end]
}

/// The white space `sscanf` skips at the start of `s`.
fn skip_space(s: &[u8]) -> usize {
    s.iter().take_while(|c| matches!(c, b' ' | b'\t' | b'\n' | b'\r' | 0x0b | 0x0c)).count()
}

/// `sscanf(s, "%f", &v)`: the decimal number at the start of `s` (white space skipped; digits with an optional point
/// and exponent), or `None` when none scans.
pub fn scan_float(s: &[u8]) -> Option<f32> {
    let begin = skip_space(s);
    let mut i = begin;
    if i < s.len() && (s[i] == b'+' || s[i] == b'-') {
        i += 1;
    }
    let digits = |s: &[u8], i: usize| s[i..].iter().take_while(|c| c.is_ascii_digit()).count();
    let whole = digits(s, i);
    i += whole;
    let mut fraction = 0;
    if i < s.len() && s[i] == b'.' {
        fraction = digits(s, i + 1);
        if whole + fraction > 0 {
            i += 1 + fraction;
        }
    }
    if whole + fraction == 0 {
        return None;
    }
    if i < s.len() && (s[i] == b'e' || s[i] == b'E') {
        let mut j = i + 1;
        if j < s.len() && (s[j] == b'+' || s[j] == b'-') {
            j += 1;
        }
        let e = digits(s, j);
        if e > 0 {
            i = j + e;
        }
    }
    std::str::from_utf8(&s[begin..i]).ok()?.parse::<f32>().ok()
}

/// `sscanf(s, "%ld", &v)`: the decimal integer at the start of `s` (white space skipped, wrapping), or `None`.
pub fn scan_long(s: &[u8]) -> Option<i32> {
    let mut i = skip_space(s);
    let mut negative = false;
    if i < s.len() && (s[i] == b'+' || s[i] == b'-') {
        negative = s[i] == b'-';
        i += 1;
    }
    let mut value: i32 = 0;
    let mut any = false;
    while i < s.len() && s[i].is_ascii_digit() {
        value = value.wrapping_mul(10).wrapping_add((s[i] - b'0') as i32);
        any = true;
        i += 1;
    }
    any.then_some(if negative { value.wrapping_neg() } else { value })
}

/// One spot from a light tag's name (`SetUp` 0xe2067: the fields after the first `_`, each up to the next `_`, the
/// start after the fifth to the end; missing fields: a colour of 1 (over 255 like the others, as the original's),
/// 500 ms on, a period of the on time, no delay). `rgb` is the colour scan's carried value (the original's locals).
pub fn spot_from_name(name: &[u8], rgb: &mut [f32; 3]) -> EeGfxGlowSpot {
    let length = name.len() as u32;
    let mut spot = EeGfxGlowSpot::default();
    let mut start = find(name, 0).wrapping_add(1);
    let mut end = find(name, start);
    for (n, c) in rgb.iter_mut().enumerate() {
        if n > 0 {
            start = end.wrapping_add(1);
            end = find(name, start);
        }
        // the first test is also `start > 0`: always true (the find's result + 1)
        if end < length {
            if let Some(v) = scan_float(field(name, start, end.wrapping_sub(start).wrapping_add(1))) {
                *c = v;
            }
        } else {
            *c = 1.0;
        }
    }
    let mut times = [0u32; 2];
    for n in 0..2 {
        start = end.wrapping_add(1);
        end = find(name, start);
        if end < length {
            if let Some(v) = scan_long(field(name, start, end.wrapping_sub(start).wrapping_add(1))) {
                times[n] = v as u32;
            }
        } else {
            times[n] = if n == 0 { 500 } else { times[0] };
        }
    }
    spot.on_time = times[0];
    spot.period = times[1];
    start = end.wrapping_add(1);
    if start < length {
        if let Some(v) = scan_long(field(name, start, length - start)) {
            spot.start = v as u32;
        }
    }
    spot.color = EeGfxColor { red: rgb[0] / 255.0, green: rgb[1] / 255.0, blue: rgb[2] / 255.0 };
    spot
}

/// The light glow's virtuals.
///
/// # Safety
/// As `effect_2d::effect2d`.
pub unsafe fn glow<H: Effect2DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY (every access below): the function's contract; no reference to the record lives across a host call
    match op {
        // 0xe1a7a: age 0, shown, no spots (the record's)
        EeGfxEffect3DOp::Create => 0,
        // 0xe2067 (also `RepairPointers`' after the base's): the media, a spot per light tag (one white default spot
        // without any) at the unit; the radius
        EeGfxEffect3DOp::SetUp => {
            let res = h.object_media();
            let u = h.glow_set_up();
            let mut made = Vec::new();
            if u.count != 0 {
                let mut rgb = [0.0f32; 3];
                for i in 0..u.count {
                    made.push(spot_from_name(&h.glow_tag_name(i), &mut rgb));
                }
            } else {
                made.push(EeGfxGlowSpot { color: WHITE, on_time: 500, period: 500, ..EeGfxGlowSpot::default() });
            }
            unsafe {
                *spots(r) = made;
                (*r).position = u.position;
            }
            h.move_to_point();
            unsafe { (*r).bounding_radius = g.size };
            res
        }
        // 0xe1f89: 9 while hidden; else the fade-in and the spots for the draw, render queue 4
        EeGfxEffect3DOp::Queue => {
            let mut res = h.base_queue();
            if res == 0 {
                if unsafe { (*r).glow.hidden } {
                    res = 9;
                }
                if res == 0 {
                    unsafe {
                        let w = &mut (*r).glow;
                        w.draw_age = w.age / g.time1 as f32;
                        if w.draw_age > 1.0 {
                            w.draw_age = 1.0;
                        }
                        for s in spots(r).iter_mut() {
                            s.draw_visible = s.visible;
                            if s.visible {
                                s.draw_position = s.position;
                            }
                        }
                    }
                    h.queue(4);
                }
            }
            res
        }
        // 0xe2512: a coloured glow sprite at each visible spot
        EeGfxEffect3DOp::DrawPass6 => {
            let list = unsafe { spots(r).clone() };
            let alpha_ref = if g.has_object { g.alpha_ref } else { 0.0 };
            for s in list.iter().filter(|s| s.draw_visible) {
                x.sprite(EE_GFX_SPRITE_GLOW, &s.draw_position, s.color, 0.0, g.size, 1.0, 1.0, alpha_ref);
            }
            0
        }
        // 0xe25dc: 7 once the unit has no hit points; the glows placed while it is within two tiles of the visible
        // terrain, hidden otherwise
        EeGfxEffect3DOp::Step => {
            let a = h.glow_area();
            if g.unit_hit_points == 0 {
                return 7;
            }
            let (left, top) = ((a.x as i32).wrapping_sub(2), (a.y as i32).wrapping_sub(2));
            let right = (a.width as i32).wrapping_add(a.x as i32).wrapping_add(2);
            let bottom = (a.height as i32).wrapping_add(a.y as i32).wrapping_add(2);
            if h.glow_rows() != 0 && a.tile_x >= left && a.tile_x <= right && a.tile_y >= top && a.tile_y <= bottom {
                unsafe {
                    (*r).glow.hidden = false;
                    place(r, g, h);
                }
            } else {
                unsafe { (*r).glow.hidden = true };
            }
            0
        }
        _ => 0,
    }
}

/// `PlaceGlows` (0xe266f; its result unused): the effect at the unit, aged by the tick, each spot lit by its timing
/// on its tag point (the unit's position without tags).
///
/// # Safety
/// As `glow`.
unsafe fn place<H: Effect2DHost>(r: *mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, h: &mut H) {
    let p = h.glow_place();
    // SAFETY (every access below): the function's contract; no reference to the record lives across a host call
    unsafe { (*r).position = p.position };
    h.move_to_point();
    let age = unsafe {
        (*r).glow.age += g.tick_time;
        (*r).glow.age
    };
    let count = unsafe { spots(r).len() };
    for i in 0..count {
        let s = unsafe { spots(r)[i] };
        let mut visible = false;
        let mut position = s.position;
        if p.show && s.start as f32 <= age {
            if (age as u32).wrapping_sub(s.start) % s.period <= s.on_time {
                visible = true;
                position = if p.has_tags { h.glow_point(i as u32) } else { p.position };
            }
        }
        unsafe {
            let s = &mut spots(r)[i];
            s.visible = visible;
            s.position = position;
        }
    }
}

/// `Load` (`load`) or `Save` of the glow after `EEEffect2D`'s (whose result each ignores): the age.
pub fn glow_file<F: EffectFile>(load: bool, r: &mut EeGfxEffectRecord, f: &mut F) -> i32 {
    if load {
        f.read_f32(&mut r.glow.age)
    } else {
        f.write_f32(r.glow.age)
    }
}

#[cfg(test)]
#[path = "effect_glow_tests.rs"]
mod tests;
