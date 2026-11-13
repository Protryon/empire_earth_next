//! The tracer beams on the effects' records (`EEE3DTracerBeam`, `src/Empire Earth/Graphics Thread/EEE3DTracerBeam.cpp`):
//! the texture's scroll, the width's pulse, the timed beam's age, its impact and hit moments and its end, Load and
//! Save. The aim and the stretch are `beam.rs`'s; the C++ keeps the shooter and target units, the impact effect, the
//! hit report and the draw.
//!
//! Simulation state (RUST_PLAN 2.1): the C++'s f32 operations in order.

use ee_ge::types::Ge3DPoint;

use crate::effect_state::{load_point, save_point, EeGfxEffectRecord, EffectFile};

/// `kErrorOK`.
const OK: i32 = 0;

/// The tick's events: the impact effect is placed now.
pub const EE_GFX_TRACER_IMPACT: i32 = 1;
/// The target is told it was hit now.
pub const EE_GFX_TRACER_HIT: i32 = 2;
/// The beam ends (7).
pub const EE_GFX_TRACER_OVER: i32 = 4;

/// `EEE3DTracerBeam`'s state.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxEffectTracer {
    /// `mTargetPoint`: on the target's model, or the fixed point (saved)
    pub target_point: Ge3DPoint,
    /// `mScroll`: the texture's offset (saved)
    pub scroll: f32,
    /// `mAge`: 0..1 over the duration for a timed beam (saved)
    pub age: f32,
    /// `mLength`: `Aim`'s (saved)
    pub length: f32,
    /// `mWidth` (saved)
    pub width: f32,
    /// `mIsTimed` (saved)
    pub timed: bool,
    /// `mIsImpactCreated`
    pub impact_created: bool,
    /// `mWeaponTagPoint`: the shooter's (saved from 0.250)
    pub weapon_tag_point: i32,
}

/// What the beam's tick reads: the tick, the graphic effect's params 1-5 and 7 (the scroll's rate, the width's rate,
/// its restart and its top, the hit's age, the impact's age), its duration, whether it has a linked (impact) effect
/// and whether the beam has a target unit.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxTracerTick {
    pub tick_time: f32,
    pub param1: f32,
    pub param2: f32,
    pub param3: f32,
    pub param4: f32,
    pub param5: f32,
    pub param7: f32,
    pub duration: u32,
    pub has_linked1: bool,
    pub has_target: bool,
}

/// `EEE3DTracerBeam::Step` after the C++'s `Aim`: the scroll (by the length, at least 1), the width; a timed beam's
/// age and its events (`EE_GFX_TRACER_*`: the impact once past param 7 with a linked effect, the hit as the age
/// passes param 5, the end past 1).
pub fn tracer_step(r: &mut EeGfxEffectRecord, t: &EeGfxTracerTick) -> i32 {
    let b = &mut r.tracer;
    let mut length = b.length;
    if length < 1.0 {
        length = 1.0;
    }
    b.scroll = (t.tick_time * t.param1) / length + b.scroll;
    if b.scroll > 1.0 {
        b.scroll -= 1.0;
    }
    if b.scroll < 0.0 {
        b.scroll += 1.0;
    }
    b.width += t.tick_time * t.param2;
    if b.width > t.param4 {
        b.width = t.param3;
    }
    let mut events = 0;
    if b.timed {
        let previous = b.age;
        b.age += t.tick_time / t.duration as f32;
        if t.has_linked1 && !b.impact_created && b.age > t.param7 {
            events |= EE_GFX_TRACER_IMPACT;
            b.impact_created = true;
        }
        if t.has_target && previous <= t.param5 && t.param5 <= b.age {
            events |= EE_GFX_TRACER_HIT;
        }
        if b.age > 1.0 {
            events |= EE_GFX_TRACER_OVER;
        }
    }
    events
}

/// `EEE3DTracerBeam::Load`'s own reads after the graphic's media load succeeded: the target's saved id (`id`; `read`
/// once it was), the age, the timing, the length, the scroll, the width, the target point and (0.250 on) the weapon
/// tag point, which is 0 otherwise or after an earlier error; the first error.
pub fn load_tracer<F: EffectFile>(
    r: &mut EeGfxEffectRecord,
    f: &mut F,
    major: u32,
    minor: u32,
    id: &mut u32,
    read: &mut bool,
) -> i32 {
    let b = &mut r.tracer;
    let mut res = f.read_u32(id);
    if res == OK {
        *read = true;
        res = f.read_f32(&mut b.age);
        if res == OK {
            res = f.read_bool(&mut b.timed);
        }
        if res == OK {
            res = f.read_f32(&mut b.length);
        }
        if res == OK {
            res = f.read_f32(&mut b.scroll);
        }
        if res == OK {
            res = f.read_f32(&mut b.width);
        }
        if res == OK {
            res = load_point(f, &mut b.target_point);
        }
        if res == OK && (major > 0 || minor >= 0xfa) {
            return f.read_i32(&mut b.weapon_tag_point);
        }
    }
    b.weapon_tag_point = 0;
    res
}

/// `EEE3DTracerBeam::Save`'s own writes after `EEEffect3D::Save`'s (`id`: the target's, -1 for none).
pub fn save_tracer<F: EffectFile>(r: &EeGfxEffectRecord, f: &mut F, id: u32) -> i32 {
    let b = &r.tracer;
    let mut res = f.write_u32(id);
    if res == OK {
        res = f.write_f32(b.age);
    }
    if res == OK {
        res = f.write_bool(b.timed);
    }
    if res == OK {
        res = f.write_f32(b.length);
    }
    if res == OK {
        res = f.write_f32(b.scroll);
    }
    if res == OK {
        res = f.write_f32(b.width);
    }
    if res == OK {
        res = save_point(f, &b.target_point);
    }
    if res == OK {
        res = f.write_i32(b.weapon_tag_point);
    }
    res
}

#[cfg(test)]
mod tests {
    use super::*;

    fn tick() -> EeGfxTracerTick {
        EeGfxTracerTick {
            tick_time: 100.0,
            param1: 0.01,
            param2: 0.001,
            param3: 0.05,
            param4: 0.3,
            param5: 0.55,
            param7: 0.25,
            duration: 1000,
            has_linked1: true,
            has_target: true,
        }
    }

    #[test]
    fn scroll_and_width() {
        let mut r = EeGfxEffectRecord::new(0x40);
        r.tracer.length = 0.5;
        r.tracer.width = 0.1;
        let t = tick();
        assert_eq!(tracer_step(&mut r, &t), 0);
        // a short beam scrolls as one of length 1
        assert_eq!(r.tracer.scroll, (100.0f32 * 0.01) / 1.0);
        assert_eq!(r.tracer.width, 0.1f32 + 100.0 * 0.001);
        r.tracer.length = 4.0;
        r.tracer.scroll = 0.9;
        r.tracer.width = 0.29;
        tracer_step(&mut r, &t);
        assert_eq!(r.tracer.scroll, (100.0f32 * 0.01) / 4.0 + 0.9 - 1.0);
        assert_eq!(r.tracer.width, 0.05);
    }

    #[test]
    fn a_timed_beam_places_its_impact_hits_and_ends_once() {
        let mut r = EeGfxEffectRecord::new(0x40);
        r.tracer.timed = true;
        let t = tick();
        let mut seen = Vec::new();
        for _ in 0..12 {
            seen.push(tracer_step(&mut r, &t));
        }
        // ages 0.1 .. : the impact past 0.25 (0.3), the hit as it passes 0.55, the end past 1
        assert_eq!(seen[2], EE_GFX_TRACER_IMPACT);
        assert_eq!(seen.iter().filter(|e| *e & EE_GFX_TRACER_IMPACT != 0).count(), 1);
        assert_eq!(seen.iter().filter(|e| *e & EE_GFX_TRACER_HIT != 0).count(), 1);
        assert!(seen[10] & EE_GFX_TRACER_OVER != 0);
        assert!(seen[..9].iter().all(|e| e & EE_GFX_TRACER_OVER == 0));
        // both of the hit's tests take equality: an age landing on param 5 hits on that tick and the next
        let mut h = EeGfxEffectRecord::new(0x40);
        h.tracer.timed = true;
        h.tracer.age = 0.25;
        let mut v = tick();
        v.param5 = 0.5;
        v.tick_time = 250.0;
        assert_eq!(tracer_step(&mut h, &v) & EE_GFX_TRACER_HIT, EE_GFX_TRACER_HIT);
        assert_eq!(h.tracer.age, 0.5);
        assert_eq!(tracer_step(&mut h, &v) & EE_GFX_TRACER_HIT, EE_GFX_TRACER_HIT);
        // without a target or a linked effect: neither
        let mut q = EeGfxEffectRecord::new(0x40);
        q.tracer.timed = true;
        let mut u = tick();
        u.has_target = false;
        u.has_linked1 = false;
        for _ in 0..12 {
            assert_eq!(tracer_step(&mut q, &u) & (EE_GFX_TRACER_IMPACT | EE_GFX_TRACER_HIT), 0);
        }
        // untimed: no age
        let mut s = EeGfxEffectRecord::new(0x40);
        assert_eq!(tracer_step(&mut s, &t), 0);
        assert_eq!(s.tracer.age, 0.0);
    }

    #[derive(Default)]
    struct Mem {
        items: Vec<(char, u32)>,
        at: usize,
    }

    impl Mem {
        fn read(&mut self, ty: char) -> Result<u32, i32> {
            let Some(&(t, v)) = self.items.get(self.at) else { return Err(3) };
            assert_eq!(t, ty, "item {}", self.at);
            self.at += 1;
            Ok(v)
        }
    }

    impl EffectFile for Mem {
        fn is_open(&mut self) -> bool {
            true
        }
        fn read_u32(&mut self, v: &mut u32) -> i32 {
            self.read('u').map(|x| *v = x).err().unwrap_or(0)
        }
        fn read_i32(&mut self, v: &mut i32) -> i32 {
            self.read('i').map(|x| *v = x as i32).err().unwrap_or(0)
        }
        fn read_f32(&mut self, v: &mut f32) -> i32 {
            self.read('f').map(|x| *v = f32::from_bits(x)).err().unwrap_or(0)
        }
        fn read_bool(&mut self, v: &mut bool) -> i32 {
            self.read('b').map(|x| *v = x != 0).err().unwrap_or(0)
        }
        fn write_u32(&mut self, v: u32) -> i32 {
            self.items.push(('u', v));
            0
        }
        fn write_i32(&mut self, v: i32) -> i32 {
            self.items.push(('i', v as u32));
            0
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
    fn save_and_load_with_and_without_the_weapon_tag_point() {
        let mut r = EeGfxEffectRecord::new(0x40);
        r.tracer = EeGfxEffectTracer {
            target_point: Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 },
            scroll: 0.25,
            age: 0.5,
            length: 7.0,
            width: 0.2,
            timed: true,
            impact_created: true,
            weapon_tag_point: 3,
        };
        let mut m = Mem::default();
        assert_eq!(save_tracer(&r, &mut m, 0xffff_ffff), 0);
        let types: String = m.items.iter().map(|x| x.0).collect();
        assert_eq!(types, "ufbffffffi");
        let mut back = EeGfxEffectRecord::new(0x40);
        let (mut id, mut read) = (0, false);
        assert_eq!(load_tracer(&mut back, &mut m, 0, 0xfa, &mut id, &mut read), 0);
        assert!(read);
        assert_eq!(id, 0xffff_ffff);
        let mut want = r.tracer;
        want.impact_created = false;
        assert_eq!(back.tracer, want);
        // before 0.250: no weapon tag point read, it is 0
        m.at = 0;
        let mut old = EeGfxEffectRecord::new(0x40);
        old.tracer.weapon_tag_point = 9;
        assert_eq!(load_tracer(&mut old, &mut m, 0, 0xf9, &mut id, &mut read), 0);
        assert_eq!(old.tracer.weapon_tag_point, 0);
        assert_eq!(m.at, 9);
        // an empty file: no id, the tag point 0
        let mut e = Mem::default();
        let mut none = EeGfxEffectRecord::new(0x40);
        none.tracer.weapon_tag_point = 9;
        let mut read2 = false;
        assert_eq!(load_tracer(&mut none, &mut e, 1, 0, &mut id, &mut read2), 3);
        assert!(!read2);
        assert_eq!(none.tracer.weapon_tag_point, 0);
    }
}
