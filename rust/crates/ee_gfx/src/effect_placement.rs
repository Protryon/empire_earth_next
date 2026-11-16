//! The battlefield decals' placement (`EEE2DBattlefieldAmbient.cpp`'s helpers, 0xde374..0xde6e9): a unit's shell
//! holes around a point (`EEPlaceImpactDecal`: an airplane's crash, a shot's impact) and its blood stain or track under
//! it (`EEPlaceTrackDecal`), each a placed effect of the graphic effect or one of its linked variants, rolled by its
//! chance; the first shell hole renews a decal already on the spot unless its scatter roll moves it.
//!
//! Simulation code (called from the physics models and the units' update, RUST_PLAN 2.1): the effects' random numbers
//! are drawn in the original's order and count; the same f32 operations in order.

use std::ffi::c_void;

use ee_ge::types::Ge3DPoint;
use ee_ge::um::{um_arctan2, um_cos};

use crate::effect_state::{self, EeGfxEffectRecord};

/// A graphic effect's numbers the placement reads (`EEGraphicEffect`: `GetParam2` .. `GetParam5`, `mIntParam2`, the
/// linked effects 1-3).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeGfxDecalEffect {
    pub param2: f32,
    pub param3: f32,
    pub param4: f32,
    pub param5: f32,
    pub int_param2: i32,
    pub linked: [*mut c_void; 3],
}

/// The C++'s answers (`context`: none).
#[repr(C)]
pub struct EeGfxDecalHost {
    pub context: *mut c_void,
    /// a graphic effect's numbers (`EEGraphicEffect*`)
    pub effect: extern "C" fn(context: *mut c_void, effect: *mut c_void) -> EeGfxDecalEffect,
    /// the manager's effect after `effect` in its list (newest first; null: the head), null past the last
    pub next: extern "C" fn(context: *mut c_void, effect: *mut c_void) -> *mut c_void,
    /// an effect's record (`EEEffect::mRecord`)
    pub record: extern "C" fn(context: *mut c_void, effect: *mut c_void) -> *mut EeGfxEffectRecord,
    /// the map's height at a point (`gWorld.mMap->GetHeight`)
    pub height: extern "C" fn(context: *mut c_void, point: *const Ge3DPoint) -> f32,
    /// the effect placed at a point (`gEffectManager->CreatePlacedEffect(point, effect, NULL)`)
    pub place: extern "C" fn(context: *mut c_void, effect: *mut c_void, point: *const Ge3DPoint),
}

/// The placement's questions (the tests' log).
pub trait DecalHost {
    fn effect(&mut self, effect: *mut c_void) -> EeGfxDecalEffect;
    /// the manager's list (newest first): the effect after `effect` (null: the head)
    fn next(&mut self, effect: *mut c_void) -> *mut c_void;
    fn record(&mut self, effect: *mut c_void) -> *mut EeGfxEffectRecord;
    fn height(&mut self, point: &Ge3DPoint) -> f32;
    fn place(&mut self, effect: *mut c_void, point: &Ge3DPoint);
}

impl DecalHost for &EeGfxDecalHost {
    fn effect(&mut self, effect: *mut c_void) -> EeGfxDecalEffect {
        (self.effect)(self.context, effect)
    }
    fn next(&mut self, effect: *mut c_void) -> *mut c_void {
        (self.next)(self.context, effect)
    }
    fn record(&mut self, effect: *mut c_void) -> *mut EeGfxEffectRecord {
        (self.record)(self.context, effect)
    }
    fn height(&mut self, point: &Ge3DPoint) -> f32 {
        (self.height)(self.context, point)
    }
    fn place(&mut self, effect: *mut c_void, point: &Ge3DPoint) {
        (self.place)(self.context, effect, point)
    }
}

/// `AmbientPickVariant` (0xde6a1): the effect or one of its up to three linked variants, each equally likely (no
/// draw without a linked effect).
fn pick_variant<H: DecalHost>(h: &mut H, effect: *mut c_void) -> *mut c_void {
    let e = h.effect(effect);
    let count = if !e.linked[2].is_null() {
        4
    } else if !e.linked[1].is_null() {
        3
    } else if !e.linked[0].is_null() {
        2
    } else {
        return effect;
    };
    match effect_state::random_long(1, count) {
        2 => e.linked[0],
        3 => e.linked[1],
        4 => e.linked[2],
        _ => effect,
    }
}

/// `AmbientRollChance` (0xde66d): the effect's chance (parameter 2); always without a draw at 1 or more.
fn roll_chance(e: &EeGfxDecalEffect) -> bool {
    if e.param2 >= 1.0 {
        return true;
    }
    effect_state::random() <= e.param2
}

/// `AmbientScatter` (0xde6e9): with the scatter chance (parameter 3) the point moved within parameter 4; whether it
/// moved.
fn scatter(e: &EeGfxDecalEffect, at: &mut Ge3DPoint) -> bool {
    if effect_state::random() >= e.param3 {
        return false;
    }
    at.x += e.param4 * (0.5 - effect_state::random());
    at.y += e.param4 * (0.5 - effect_state::random());
    true
}

/// `AmbientFindNear` (0xde5ca): the first battlefield decal (kind 0x32) of type `kind` within 0.05 of `at` in the
/// manager's list (compared in double, as the original's literals were): its record.
fn find_near<H: DecalHost>(h: &mut H, kind: u32, at: &Ge3DPoint) -> *mut EeGfxEffectRecord {
    let mut e = h.next(std::ptr::null_mut());
    while !e.is_null() {
        let r = h.record(e);
        // SAFETY: the host hands the live records of the manager's effects
        let d = unsafe { &*r };
        if d.kind == BATTLEFIELD_DECAL && d.ambient.kind == kind {
            let (x, y) = ((at.x - d.ambient.center.x) as f64, (at.y - d.ambient.center.y) as f64);
            if x < 0.05 && x > -0.05 && y < 0.05 && y > -0.05 {
                return r;
            }
        }
        e = h.next(e);
    }
    std::ptr::null_mut()
}

/// `kEffect2DBattlefieldAmbient`.
const BATTLEFIELD_DECAL: i32 = 0x32;

/// `EEPlaceImpactDecal` (0xde374) after its tests (ambient effects on, the class's shell hole effect `effect`): up to
/// int parameter 2 decals around `location`, raised 0.005, each a picked variant that passed its roll (a failed roll
/// stops); the first renews a type-0 decal on the spot (its age 0) unless its scatter roll moved it, the others spread
/// over parameter 5 at the map's height.
pub fn impact<H: DecalHost>(h: &mut H, effect: *mut c_void, location: &Ge3DPoint) {
    let e = h.effect(effect);
    let mut at = *location;
    at.z += 0.005;
    for i in 0..e.int_param2 {
        let variant = pick_variant(h, effect);
        let v = h.effect(variant);
        if !roll_chance(&v) {
            return;
        }
        if i == 0 {
            let near = find_near(h, 0, &at);
            if !near.is_null() && !scatter(&v, &mut at) {
                // SAFETY: a live record the host handed
                unsafe { (*near).ambient.age = 0.0 };
                continue;
            }
        } else {
            at.x = (0.5 - effect_state::random()) * e.param5 + location.x;
            at.y = (0.5 - effect_state::random()) * e.param5 + location.y;
            at.z = h.height(&at) + 0.005;
        }
        h.place(variant, &at);
    }
}

/// `EEPlaceTrackDecal` (0xde4a9) after its tests (ambient effects and blood on, the class's blood stain effect
/// `effect`): a picked variant that passed its roll at `position`, moved forward along the heading (`m00`, `m10`: the
/// unit's location's first column) for a giraffe (0.4) or a horse (0.3, turned back an eighth), raised 0.005.
pub fn track<H: DecalHost>(h: &mut H, effect: *mut c_void, position: &Ge3DPoint, kind: u32, m00: f32, m10: f32) {
    let variant = pick_variant(h, effect);
    if !roll_chance(&h.effect(variant)) {
        return;
    }
    let mut at = *position;
    if kind == EE_GFX_TRACK_GIRAFFE || kind == EE_GFX_TRACK_HORSE {
        // the game's lookup-table trigonometry (UMArcTan2, UMCos), never std's: std's differ by ulps
        let mut angle = um_arctan2(m10, m00);
        let mut distance = 0.4f32;
        if kind == EE_GFX_TRACK_HORSE {
            distance = 0.3;
            angle -= 0.785_398_2;
        }
        at.x += um_cos(angle) * distance;
        at.y += um_cos(1.570_796_4 - angle) * distance;
    }
    at.z += 0.005;
    h.place(variant, &at);
}

/// The track's unit kinds: `track`'s `kind` 1 a giraffe (`kUnitTypeResGiraffe`), 2 a horse (`kUnitTypeResHorse`),
/// 0 any other.
pub const EE_GFX_TRACK_GIRAFFE: u32 = 1;
pub const EE_GFX_TRACK_HORSE: u32 = 2;

/// `impact` for the C++.
///
/// # Safety
/// `host` and `location` are valid; `effect` is a graphic effect the host answers for.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_decal_impact(
    host: *const EeGfxDecalHost,
    effect: *mut c_void,
    location: *const Ge3DPoint,
) {
    // SAFETY: the function's contract
    unsafe { impact(&mut &*host, effect, &*location) }
}

/// `track` for the C++.
///
/// # Safety
/// `host` and `position` are valid; `effect` is a graphic effect the host answers for.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_decal_track(
    host: *const EeGfxDecalHost,
    effect: *mut c_void,
    position: *const Ge3DPoint,
    kind: u32,
    m00: f32,
    m10: f32,
) {
    // SAFETY: the function's contract
    unsafe { track(&mut &*host, effect, &*position, kind, m00, m10) }
}

#[cfg(test)]
mod tests {
    use super::*;

    struct Log {
        effects: Vec<EeGfxDecalEffect>,
        decals: Vec<EeGfxEffectRecord>,
        placed: Vec<(usize, Ge3DPoint)>,
    }

    fn fx(param2: f32, int_param2: i32, linked: usize) -> EeGfxDecalEffect {
        let mut l = [std::ptr::null_mut(); 3];
        for (i, x) in l.iter_mut().enumerate().take(linked) {
            *x = (i + 2) as *mut c_void;
        }
        EeGfxDecalEffect { param2, param3: 0.0, param4: 0.5, param5: 2.0, int_param2, linked: l }
    }

    impl DecalHost for Log {
        fn effect(&mut self, effect: *mut c_void) -> EeGfxDecalEffect {
            self.effects[effect as usize - 1]
        }
        fn next(&mut self, effect: *mut c_void) -> *mut c_void {
            let next = effect as usize; // 0: the head; i: after decal i - 1
            if next < self.decals.len() {
                (next + 1) as *mut c_void
            } else {
                std::ptr::null_mut()
            }
        }
        fn record(&mut self, effect: *mut c_void) -> *mut EeGfxEffectRecord {
            &mut self.decals[effect as usize - 1]
        }
        fn height(&mut self, _point: &Ge3DPoint) -> f32 {
            7.0
        }
        fn place(&mut self, effect: *mut c_void, point: &Ge3DPoint) {
            self.placed.push((effect as usize, *point));
        }
    }

    /// a graphic effect's handle: its 1-based index
    fn id(n: usize) -> *mut c_void {
        n as *mut c_void
    }

    fn log(effects: Vec<EeGfxDecalEffect>) -> Log {
        Log { effects, decals: Vec::new(), placed: Vec::new() }
    }

    #[test]
    fn sure_holes_spread_round_the_first() {
        let mut h = log(vec![fx(1.0, 3, 0)]);
        impact(&mut h, id(1), &Ge3DPoint { x: 10.0, y: 20.0, z: 1.0 });
        assert_eq!(h.placed.len(), 3);
        assert_eq!(h.placed[0], (1, Ge3DPoint { x: 10.0, y: 20.0, z: 1.005 }));
        assert!(h.placed[1..]
            .iter()
            .all(|(_, p)| (p.x - 10.0).abs() <= 1.0 && (p.y - 20.0).abs() <= 1.0 && p.z == 7.005));
    }

    #[test]
    fn a_hole_on_a_hole_renews_it() {
        let mut h = log(vec![fx(1.0, 1, 0)]);
        let mut r = EeGfxEffectRecord::new(BATTLEFIELD_DECAL);
        r.ambient.center = Ge3DPoint { x: 10.01, y: 20.0, z: 0.0 };
        r.ambient.age = 0.5;
        h.decals.push(EeGfxEffectRecord::new(3));
        h.decals.push(r);
        impact(&mut h, id(1), &Ge3DPoint { x: 10.0, y: 20.0, z: 1.0 });
        assert!(h.placed.is_empty());
        assert_eq!(h.decals[1].ambient.age, 0.0);
    }

    #[test]
    fn a_failed_roll_places_nothing_and_variants_are_picked() {
        let mut h = log(vec![fx(-1.0, 2, 0)]);
        impact(&mut h, id(1), &Ge3DPoint::default());
        track(&mut h, id(1), &Ge3DPoint::default(), 0, 1.0, 0.0);
        assert!(h.placed.is_empty());
        let mut h = log(vec![fx(1.0, 1, 3), fx(1.0, 1, 0), fx(1.0, 1, 0), fx(1.0, 1, 0)]);
        for _ in 0..40 {
            track(&mut h, id(1), &Ge3DPoint::default(), EE_GFX_TRACK_GIRAFFE, 1.0, 0.0);
        }
        let mut seen: Vec<usize> = h.placed.iter().map(|(e, _)| *e).collect();
        seen.sort_unstable();
        seen.dedup();
        assert_eq!(seen, [1, 2, 3, 4]);
        assert!(h.placed.iter().all(|(_, p)| (p.x - 0.4).abs() < 1e-6 && p.y.abs() < 1e-6 && p.z == 0.005));
    }
}
