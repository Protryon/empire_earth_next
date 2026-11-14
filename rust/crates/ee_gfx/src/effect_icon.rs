//! The animated 3D icon (kind 2) and the goal (kind 0x3c, an icon with a countdown label) whole in Rust on
//! `EEE3DRust` (`EEE3DIcon` and `EEE3DGoal`, deleted): the age and its end, the scale pulsing between the graphic
//! effect's start size and size, the texture's scroll, the animation frame, the orbitals (`effect_orbit.rs`'s list),
//! the place at the unit or the point, the queue, pass 6's tweened model, the goal's pass 3 label, Load and Save. The
//! C++ keeps the graphic object (`EEE3DRust::mGraphicObject`) and answers the world.
//!
//! Simulation state (RUST_PLAN 2.1): the C++'s f32 operations in order, `lrintf` for `UMFloatToLong`.

use ee_ge::transform::RotationOrder;
use ee_ge::types::{Ge3DPoint, GeTransformation};
use ee_ge::um;

use crate::effect_orbit::{copy_orbitals, keep_angles, update_orbitals};
use crate::effect_rising::{EeGfxEffect3DOp, EeGfxGraphicFacts, Effect3DHost};
use crate::effect_state::{identity, load_point, save_point, EeGfxEffectRecord, EffectFile};
use crate::effects::EeGfxColor;
use crate::lighting::EeGfxLighting;
use crate::raster::{
    Raster, BLEND_INV_SRC_ALPHA, BLEND_ONE, BLEND_SRC_ALPHA, ZBUFFER_TEST_AND_WRITE, ZBUFFER_TEST_ONLY,
};

/// `kEffect3DIcon`, `kEffect3DGoal`.
pub const EE_GFX_EFFECT_KIND_ICON: i32 = 2;
pub const EE_GFX_EFFECT_KIND_GOAL: i32 = 0x3c;
/// The goal's label stands this high over its location.
const GOAL_LABEL_HEIGHT: f32 = 1.2;

/// `kErrorOK`.
const OK: i32 = 0;

/// `EEE3DIcon`'s state (and `EEE3DGoal`'s time left).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeGfxEffectIcon {
    /// `mLocation`: where a fixed icon stands (saved when it does not follow its unit)
    pub location: Ge3DPoint,
    /// `mOpacity`: the draw's (Queue)
    pub opacity: f32,
    /// `mTime`: ms into the current frame
    pub time: f32,
    /// `mScale`
    pub scale: f32,
    /// `mAge`: ms (saved)
    pub age: f32,
    /// `mFraction`: the draw's fractional frame position (Queue)
    pub fraction: f32,
    /// `mFrame`, `mFrameCount`, `mFrameTime`: animation 13 of the graphic
    pub frame: u32,
    pub frame_count: u32,
    pub frame_time: u32,
    /// `mScroll`: the texture's u offset
    pub scroll: f32,
    /// `mFollowUnit` (saved)
    pub follow_unit: bool,
    /// `mGrowing`: the scale grows towards the size
    pub growing: bool,
    /// the draw's copies (`Queue`): `mDrawTransformation`, `mDrawFrame`, `mDrawScroll`
    pub draw_transformation: GeTransformation,
    pub draw_frame: u32,
    pub draw_scroll: f32,
    /// the goal's `mTimeLeft` in ms (saved; its label shows it as m:ss while positive)
    pub time_left: u32,
}

impl Default for EeGfxEffectIcon {
    fn default() -> EeGfxEffectIcon {
        EeGfxEffectIcon {
            location: Ge3DPoint::default(),
            opacity: 0.0,
            time: 0.0,
            scale: 0.0,
            age: 0.0,
            fraction: 0.0,
            frame: 0,
            frame_count: 0,
            frame_time: 0,
            scroll: 0.0,
            follow_unit: false,
            growing: false,
            draw_transformation: identity(),
            draw_frame: 0,
            draw_scroll: 0.0,
            time_left: 0,
        }
    }
}

/// What the icon's tick reads: the tick, the physics' speed and the graphic effect's duration, size, start size,
/// `mParam7` (the scale's rate), `mIntParam2` (it scrolls when positive) and `mParam11` (the scroll's rate).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxIconTick {
    pub tick_time: f32,
    pub speed: f32,
    pub duration: u32,
    pub size: f32,
    pub start_size: f32,
    pub rate: f32,
    pub int_param2: i32,
    pub param11: f32,
}

/// `EEE3DIcon::Clear`'s part: the age, scale, animation and frame (the C++ drops the orbitals and the graphic).
pub fn icon_clear(r: &mut EeGfxEffectRecord) {
    let i = &mut r.icon;
    i.age = 0.0;
    i.scale = 0.0;
    i.growing = false;
    i.time = 0.0;
    i.frame = 0;
    i.frame_count = 0;
    i.fraction = 0.0;
    i.frame_time = 0;
}

/// `EEE3DIcon::Step`'s part: the age (7 once a set duration is up), the scale pulsing between the start size and the
/// size, the scroll, the frame. The C++ then updates the orbitals, sets the radius to the scale and places the icon.
pub fn icon_step(r: &mut EeGfxEffectRecord, t: &EeGfxIconTick) -> i32 {
    let i = &mut r.icon;
    let mut res = 0;
    let mut time = i.time;
    let mut frame = i.frame;
    i.age += t.tick_time;
    if i.age >= t.duration as f32 && t.duration != 0 {
        res = 7;
    }
    if i.growing {
        if t.size > i.scale {
            i.scale += t.rate;
        }
        if t.size <= i.scale {
            i.growing = false;
            i.scale = t.size;
        }
    } else {
        if t.start_size < i.scale {
            i.scale -= t.rate;
        }
        if t.start_size >= i.scale {
            i.growing = true;
            i.scale = t.start_size;
        }
    }
    time += t.speed * 1000.0;
    if t.int_param2 > 0 {
        i.scroll += t.param11 * t.tick_time / 1000.0;
        let scroll = i.scroll;
        if scroll > 1.0 {
            i.scroll = scroll - 1.0;
        }
        if i.scroll < 0.0 {
            i.scroll += 1.0;
        }
    }
    if time >= i.frame_time as f32 {
        let count = um::lrintf(time / i.frame_time as f32);
        time -= count.wrapping_mul(i.frame_time as i32) as u32 as f32;
        // the original divides by the frame count (no animation: it would fault; the frame stays)
        frame = (count as u32).wrapping_add(i.frame).checked_rem(i.frame_count).unwrap_or(frame);
    }
    i.time = time;
    i.frame = frame;
    res
}

/// `EEE3DIcon::Queue`'s opacity (the graphic effect's `GetParam8()`, faded in from `GetParam9()` over a positive
/// `GetParam10()` ms) and fractional frame.
pub fn icon_queue(r: &mut EeGfxEffectRecord, param8: f32, param9: f32, param10: f32) {
    let i = &mut r.icon;
    i.opacity = param8;
    if param10 > 0.0 && param10 > i.age {
        i.opacity = (param8 - param9) * (i.age / param10) + param9;
    }
    i.fraction = if i.frame_count > 0 && i.frame_time != 0 { i.time / i.frame_time as f32 } else { 0.0 };
}

/// `EEE3DIcon::Load`'s own reads after `EEEffect3D::Load`'s: the age, the follow flag, a fixed icon's location (and
/// `EEE3DGoal::Load`'s time left); the first error.
pub fn load_icon<F: EffectFile>(r: &mut EeGfxEffectRecord, f: &mut F) -> i32 {
    let goal = r.kind == EE_GFX_EFFECT_KIND_GOAL;
    let i = &mut r.icon;
    let mut res = f.read_f32(&mut i.age);
    if res == OK {
        res = f.read_bool(&mut i.follow_unit);
        if res == OK && !i.follow_unit {
            res = load_point(f, &mut i.location);
        }
    }
    if res == OK && goal {
        res = f.read_u32(&mut i.time_left);
    }
    res
}

/// `EEE3DIcon::Save`'s own writes after `EEEffect3D::Save`'s.
pub fn save_icon<F: EffectFile>(r: &EeGfxEffectRecord, f: &mut F) -> i32 {
    let i = &r.icon;
    let mut res = f.write_f32(i.age);
    if res == OK {
        res = f.write_bool(i.follow_unit);
        if res == OK && !i.follow_unit {
            res = save_point(f, &i.location);
        }
    }
    if res == OK && r.kind == EE_GFX_EFFECT_KIND_GOAL {
        res = f.write_u32(i.time_left);
    }
    res
}

/// `EEE3DIcon::Clear`: `icon_clear` and the orbitals dropped (their angles kept, as the original's `mAngles`).
///
/// # Safety
/// `r` is a live record.
unsafe fn clear(r: *mut EeGfxEffectRecord) {
    // SAFETY: the function's contract
    unsafe {
        icon_clear(&mut *r);
        if !(*r).orbit.orbitals.is_null() {
            (*(*r).orbit.orbitals).list.clear();
        }
        (*r).orbit.count = 0;
    }
}

/// `UpdatePosition`: at the unit (raised by its effect height while on the map and alive; the origin without one)
/// or the fixed location, plus the graphic effect's offset (params 1..3).
///
/// # Safety
/// `r` is a live record, not referenced across the host's calls.
unsafe fn place<H: Effect3DHost>(r: *mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, h: &mut H) {
    // SAFETY (every access below): the function's contract
    let mut at =
        if unsafe { (*r).icon.follow_unit } { h.unit_raised_position() } else { unsafe { (*r).icon.location } };
    at.x += g.param1;
    at.y += g.param2;
    at.z += g.param3;
    let mut t = identity();
    t.m[0][3] = at.x;
    t.m[1][3] = at.y;
    t.m[2][3] = at.z;
    unsafe { (*r).transformation = t };
    h.move_to();
}

/// Animation 13 of the kept graphic object restarted (`mFrame`, `mFrameCount`, `mFrameTime`, `mTime`).
///
/// # Safety
/// As `place`.
unsafe fn restart<H: Effect3DHost>(r: *mut EeGfxEffectRecord, h: &mut H) {
    let a = h.animation();
    // SAFETY: the function's contract
    unsafe {
        let i = &mut (*r).icon;
        i.frame = a.start;
        i.frame_count = a.frame_count;
        i.frame_time = a.frame_time;
        i.time = 0.0;
    }
}

/// The icon's and goal's virtuals.
///
/// # Safety
/// As `effect_rising::effect3d`.
pub(crate) unsafe fn icon<H: Effect3DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY (every access below): the function's contract; no reference to the record is kept across a host call
    match op {
        // on a unit: it follows the unit
        EeGfxEffect3DOp::Create => {
            unsafe {
                (*r).icon.follow_unit = true;
                (*r).icon.growing = false;
                (*r).icon.scroll = 0.0;
            }
            0
        }
        // at the point the constructor left in `aged.location`
        EeGfxEffect3DOp::CreateAt => {
            unsafe {
                (*r).icon.follow_unit = false;
                (*r).icon.location = (*r).aged.location;
                (*r).aged.location = Ge3DPoint::default();
                (*r).icon.growing = false;
                (*r).icon.scroll = 0.0;
            }
            0
        }
        EeGfxEffect3DOp::SetUp => {
            unsafe { clear(r) };
            h.reload_media();
            unsafe {
                restart(r, h);
                copy_orbitals(r, h, false);
                (*r).bounding_radius = g.size;
                place(r, g, h);
                (*r).icon.scale = g.size;
            }
            0
        }
        EeGfxEffect3DOp::Repair => {
            let result = h.load_media();
            unsafe {
                if g.has_effect {
                    (*r).icon.scale = g.size;
                }
                copy_orbitals(r, h, false);
                restart(r, h);
            }
            result
        }
        EeGfxEffect3DOp::Step => {
            let t = EeGfxIconTick {
                tick_time: g.tick_time,
                speed: g.speed,
                duration: g.duration,
                size: g.size,
                start_size: g.start_size,
                rate: g.param7,
                int_param2: g.int_param2,
                param11: g.param11,
            };
            unsafe {
                let result = icon_step(&mut *r, &t);
                update_orbitals(r, g.tick_seconds);
                (*r).bounding_radius = (*r).icon.scale;
                place(r, g, h);
                result
            }
        }
        EeGfxEffect3DOp::Queue => {
            let result = if g.flag2 { 0 } else { h.base_queue() };
            if result != 0 {
                return result;
            }
            if h.market_unbuilt() {
                return 10;
            }
            unsafe {
                let i = &mut (*r).icon;
                i.draw_transformation = (*r).transformation;
                keep_angles(r);
                let i = &mut (*r).icon;
                i.draw_frame = i.frame;
                i.draw_scroll = i.scroll;
                icon_queue(&mut *r, g.param8, g.param9, g.param10);
            }
            h.queue_screen(4);
            0
        }
        EeGfxEffect3DOp::DrawPass3 if unsafe { (*r).kind } == EE_GFX_EFFECT_KIND_GOAL => {
            let (left, location) = unsafe { ((*r).icon.time_left, (*r).icon.location) };
            if left > 0 {
                draw_label(x, left, &location);
            }
            0
        }
        EeGfxEffect3DOp::DrawPass6 => unsafe { draw(r, g, h, x) },
        _ => 0,
    }
}

/// The goal's countdown: `left` ms as m:ss over `location`, black, then white one pixel up and left
/// (`GetMinutesAndSeconds`, `"%ld:%.2ld"`).
fn draw_label<R: Raster>(x: &mut R, left: u32, location: &Ge3DPoint) {
    let minutes = left / 1000 / 60;
    let seconds = (left - minutes.wrapping_mul(60000)) / 1000;
    let text = format!("{minutes}:{seconds:02}");
    let at = Ge3DPoint { x: location.x, y: location.y, z: location.z + GOAL_LABEL_HEIGHT };
    let (sx, sy) = x.screen_point(&at);
    x.draw_text(sx, sy, &text, EeGfxColor { red: 0.0, green: 0.0, blue: 0.0 });
    x.draw_text(sx - 1.0, sy - 1.0, &text, EeGfxColor { red: 1.0, green: 1.0, blue: 1.0 });
}

/// Pass 6: the graphic's model `mIntParam1` tweened between the kept frame and the next, alpha blended with z
/// writes and prelit (flag 1), else one-one blended over the z buffer, white lit and dimmed by the fog at its place,
/// facing the camera; its orbitals turned and textured, the texture's u scrolled for the draw when `mIntParam2` is
/// positive; the fog restored.
///
/// # Safety
/// As `icon`.
unsafe fn draw<H: Effect3DHost, R: Raster>(
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY (every access below): the function's contract
    let i = unsafe { (*r).icon };
    let mut t = i.draw_transformation;
    let model = h.model_at(g.int_param1 as u32);
    let fog = x.get_fog();
    x.set_alpha_ref(if g.has_object { g.alpha_ref } else { 0.0 });
    x.set_shading_source(0);
    if g.flag1 {
        x.set_blending(BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA);
        x.lighting(EeGfxLighting::Prelit);
        x.set_zbuffer(ZBUFFER_TEST_AND_WRITE);
    } else {
        x.set_blending(BLEND_ONE, BLEND_ONE);
        x.lighting(EeGfxLighting::White);
        let at = Ge3DPoint { x: t.m[0][3], y: t.m[1][3], z: t.m[2][3] };
        let value = x.fog_factor(&at, fog);
        x.set_ambient(EeGfxColor { red: value, green: value, blue: value });
        let yaw = x.camera_yaw();
        t.set_orientation(RotationOrder::Ypr, yaw, 0.0, 0.0);
        x.set_zbuffer(ZBUFFER_TEST_ONLY);
    }
    let texture = h.texture_at(0);
    x.model_texture(model, texture);
    x.model_frame(model, i.draw_frame);
    // the original divides by the frame count (no animation: it would fault; frame 0)
    x.model_tween(model, i.draw_frame.wrapping_add(1).checked_rem(i.frame_count).unwrap_or(0), i.fraction);
    x.model_transform(model, &t);
    x.model_scale(model, i.scale);
    x.model_opacity(model, i.opacity);
    x.model_prepare(model);
    unsafe {
        let count = (*r).orbit.count;
        if count > 0 {
            let o = &*(*r).orbit.orbitals;
            let mut turn = identity();
            for k in 0..count as usize {
                turn.set_orientation(RotationOrder::Ypr, o.angles[k], 0.0, 0.0);
                x.model_texture(o.list[k].model, texture);
                x.model_pre_transform(o.list[k].model, &turn);
            }
        }
    }
    let scroll = g.int_param2 > 0;
    if scroll {
        x.model_scroll_u(model, i.draw_scroll);
    }
    let result = x.model_draw(model, 1.0);
    x.set_fog(fog);
    if scroll {
        x.model_restore_u(model);
    }
    result
}

#[cfg(test)]
#[path = "effect_icon_tests.rs"]
mod virtual_tests;

#[cfg(test)]
mod tests {
    use super::*;

    fn tick() -> EeGfxIconTick {
        EeGfxIconTick {
            tick_time: 100.0,
            speed: 0.1,
            duration: 0,
            size: 2.0,
            start_size: 1.0,
            rate: 0.25,
            int_param2: 0,
            param11: 0.0,
        }
    }

    #[test]
    fn age_ends_only_with_a_duration() {
        let mut r = EeGfxEffectRecord::new(0x31);
        r.icon.frame_time = 1000;
        r.icon.frame_count = 4;
        let mut t = tick();
        for _ in 0..30 {
            assert_eq!(icon_step(&mut r, &t), 0);
        }
        t.duration = 3500;
        assert_eq!(icon_step(&mut r, &t), 0);
        assert_eq!(r.icon.age, 3100.0);
        for _ in 0..3 {
            icon_step(&mut r, &t);
        }
        assert_eq!(icon_step(&mut r, &t), 7);
    }

    #[test]
    fn scale_pulses_between_start_size_and_size() {
        let mut r = EeGfxEffectRecord::new(0x31);
        r.icon.frame_time = 1000;
        r.icon.frame_count = 1;
        let t = tick();
        r.icon.scale = 1.5;
        // shrinking: 1.25, then 1.0 reaches the start size and turns
        icon_step(&mut r, &t);
        assert_eq!((r.icon.scale, r.icon.growing), (1.25, false));
        icon_step(&mut r, &t);
        assert_eq!((r.icon.scale, r.icon.growing), (1.0, true));
        for _ in 0..3 {
            icon_step(&mut r, &t);
        }
        assert_eq!((r.icon.scale, r.icon.growing), (1.75, true));
        icon_step(&mut r, &t);
        assert_eq!((r.icon.scale, r.icon.growing), (2.0, false));
    }

    #[test]
    fn frames_advance_by_the_speed_and_wrap() {
        let mut r = EeGfxEffectRecord::new(0x31);
        r.icon.frame_time = 30;
        r.icon.frame_count = 4;
        r.icon.frame = 3;
        let mut t = tick();
        t.speed = 0.1;
        // 100 ms: lrintf(100 / 30) = 3 frames, 10 ms left
        icon_step(&mut r, &t);
        assert_eq!((r.icon.frame, r.icon.time), (2, 10.0));
        // 110 ms: lrintf(3.67) = 4 frames, 110 - 120 = -10 ms left
        icon_step(&mut r, &t);
        assert_eq!((r.icon.frame, r.icon.time), (2, -10.0));
        // no frames: the time grows, the frame stays
        r.icon.frame_count = 0;
        icon_step(&mut r, &t);
        assert_eq!(r.icon.frame, 2);
    }

    #[test]
    fn scroll_wraps_into_zero_to_one() {
        let mut r = EeGfxEffectRecord::new(0x31);
        r.icon.frame_time = 1000;
        r.icon.frame_count = 1;
        let mut t = tick();
        t.int_param2 = 1;
        t.param11 = 6.0;
        icon_step(&mut r, &t);
        assert_eq!(r.icon.scroll, 0.6);
        icon_step(&mut r, &t);
        assert_eq!(r.icon.scroll, 0.6f32 + 0.6 - 1.0);
        t.param11 = -12.0;
        r.icon.scroll = 0.5;
        icon_step(&mut r, &t);
        assert_eq!(r.icon.scroll, 0.5f32 - 1.2 + 1.0);
        t.int_param2 = 0;
        let s = r.icon.scroll;
        icon_step(&mut r, &t);
        assert_eq!(r.icon.scroll, s);
    }

    #[test]
    fn queue_fades_in_and_takes_the_fraction() {
        let mut r = EeGfxEffectRecord::new(0x31);
        r.icon.age = 250.0;
        r.icon.time = 15.0;
        r.icon.frame_time = 30;
        r.icon.frame_count = 2;
        icon_queue(&mut r, 1.0, 0.2, 1000.0);
        assert_eq!(r.icon.opacity, (1.0f32 - 0.2) * 0.25 + 0.2);
        assert_eq!(r.icon.fraction, 0.5);
        icon_queue(&mut r, 0.75, 0.2, 0.0);
        assert_eq!(r.icon.opacity, 0.75);
        r.icon.frame_time = 0;
        icon_queue(&mut r, 0.75, 0.2, 100.0);
        assert_eq!((r.icon.opacity, r.icon.fraction), (0.75, 0.0));
    }

    #[derive(Default)]
    struct Mem {
        items: Vec<(char, u32)>,
        at: usize,
    }

    impl EffectFile for Mem {
        fn is_open(&mut self) -> bool {
            true
        }
        fn read_u32(&mut self, _: &mut u32) -> i32 {
            unreachable!()
        }
        fn read_i32(&mut self, _: &mut i32) -> i32 {
            unreachable!()
        }
        fn read_f32(&mut self, v: &mut f32) -> i32 {
            let Some(&(t, x)) = self.items.get(self.at) else { return 3 };
            assert_eq!(t, 'f');
            self.at += 1;
            *v = f32::from_bits(x);
            0
        }
        fn read_bool(&mut self, v: &mut bool) -> i32 {
            let Some(&(t, x)) = self.items.get(self.at) else { return 3 };
            assert_eq!(t, 'b');
            self.at += 1;
            *v = x != 0;
            0
        }
        fn write_u32(&mut self, _: u32) -> i32 {
            unreachable!()
        }
        fn write_i32(&mut self, _: i32) -> i32 {
            unreachable!()
        }
        fn write_f32(&mut self, v: f32) -> i32 {
            self.items.push(('f', v.to_bits()));
            0
        }
        fn write_bool(&mut self, v: bool) -> i32 {
            self.items.push(('b', v as u32));
            0
        }
    }

    #[test]
    fn save_and_load_keep_a_fixed_location_only() {
        let mut r = EeGfxEffectRecord::new(0x31);
        r.icon.age = 12.5;
        r.icon.location = Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 };
        let mut m = Mem::default();
        assert_eq!(save_icon(&r, &mut m), 0);
        assert_eq!(m.items.len(), 5);
        let mut back = EeGfxEffectRecord::new(0x31);
        assert_eq!(load_icon(&mut back, &mut m), 0);
        assert_eq!((back.icon.age, back.icon.follow_unit, back.icon.location), (12.5, false, r.icon.location));
        r.icon.follow_unit = true;
        let mut m = Mem::default();
        assert_eq!(save_icon(&r, &mut m), 0);
        assert_eq!(m.items.len(), 2);
        let mut back = EeGfxEffectRecord::new(0x31);
        assert_eq!(load_icon(&mut back, &mut m), 0);
        assert!(back.icon.follow_unit);
        assert_eq!(back.icon.location, Ge3DPoint::default());
        // a short file: the first error
        let mut short = Mem { items: vec![('f', 0)], at: 0 };
        assert_eq!(load_icon(&mut back, &mut short), 3);
    }
}
