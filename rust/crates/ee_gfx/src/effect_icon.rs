//! The animated 3D icons on the effects' records (`EEE3DIcon`, also the base of `EEE3DGoal`,
//! `src/Empire Earth/Graphics Thread/EEE3DIcon.cpp`): the age and its end, the scale pulsing between the graphic
//! effect's start size and size, the texture's scroll, the animation frame, the draw's opacity, Load and Save. The C++
//! keeps the graphic object, the orbitals (copies of the graphic's), the position at the unit and the draw.
//!
//! Simulation state (RUST_PLAN 2.1): the C++'s f32 operations in order, `lrintf` for `UMFloatToLong`.

use ee_ge::types::Ge3DPoint;
use ee_ge::um;

use crate::effect_state::{load_point, save_point, EeGfxEffectRecord, EffectFile};

/// `kErrorOK`.
const OK: i32 = 0;

/// `EEE3DIcon`'s state.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
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

/// `EEE3DIcon::Load`'s own reads after `EEEffect3D::Load`'s: the age, the follow flag, a fixed icon's location; the
/// first error.
pub fn load_icon<F: EffectFile>(r: &mut EeGfxEffectRecord, f: &mut F) -> i32 {
    let i = &mut r.icon;
    let mut res = f.read_f32(&mut i.age);
    if res == OK {
        res = f.read_bool(&mut i.follow_unit);
        if res == OK && !i.follow_unit {
            res = load_point(f, &mut i.location);
        }
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
    res
}

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
