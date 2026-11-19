//! The effects' constructors by the manager's entry points (the original leaves' of the graphic effect's kind; in
//! Rust since the graphics + LLE cluster's second round, from `EEEffectHost.cpp`): each attaches the effect to its
//! unit and graphic effect (`effect_obj.rs`), puts the entry's points, units and the unit's reads into its record and
//! calls its class's constructor (`effect_rising.rs`, `effect_2d.rs`). The units, their classes, stats, owners and
//! physical models are read through `ee_sim::world`'s views and `effect_reads.rs`' layout; the shooters' weapon
//! points stay the units' C++ (`EECombatEffectWeaponPoint`, with its CBEW capture).

// the C longs' widths differ by platform (a cast to the same type on Windows)
#![allow(clippy::unnecessary_cast)]

use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};

use ee_base::math::types::{Ge3DLine, Ge3DPoint, GeTransformation};
use ee_sim::world::{UnitView, WorldRead};
use ee_sim::world_field;

use crate::gfx::effect_manager::EeGfxEffectEntry;
use crate::gfx::effect_obj::{self as obj, world, EffectPtr};
use crate::gfx::effect_reads::{self, Gobj, GraphicEffect};
use crate::gfx::effect_rising::EeGfxEffect3DOp;
use crate::gfx::effect_state::{EeGfxAmbientNew, EeGfxEffectRecord};
use crate::gfx::ffi_manager::EeGfxEffectArgs;

const SMOKE: i32 = 0x1e;
const PARTICLE_2D: i32 = 0x2e;
const MULTI_EFFECT: i32 = 0x3b;
const LIGHT_GLOW: i32 = 0xd;
const CONFIRMATION: i32 = 0x41;
const EMITTER: i32 = 0x3a;
const PARTICLE_3D: i32 = 0x39;
const VELOCITY_PARTICLE: i32 = 0xc;
const SMOKE_MUZZLE_BLAST: i32 = 0x14;
const PROJECTILE_2D: i32 = 0x30;
const TRACER_BEAM: i32 = 0x40;
const CITIZEN_BEAM: i32 = 0x33;
const TRACERS: i32 = 0x21;
const SHELLS: i32 = 0x26;
const WINTER_BREATH: i32 = 0x16;
const MARCHING_LINE: i32 = 0x2d;
const DECAL: i32 = 0x32;
/// the high-arc shooter's target lift (the smoke muzzle blast's)
const TARGET_LIFT: f32 = 15.0;

fn rec<'a>(e: EffectPtr) -> &'a mut EeGfxEffectRecord {
    // SAFETY: a live effect's record, one field at a time
    unsafe { &mut *(*e).record }
}

fn call(e: EffectPtr, op: EeGfxEffect3DOp, arg: *mut c_void) {
    obj::ee_gfx_effect_call(e, op, arg);
}

fn attach(e: EffectPtr, unit: u64, g: u64, kind: i32, keeps_unit: bool) {
    obj::ee_gfx_effect_attach(e, unit, g, kind, keeps_unit);
}

fn position(t: &GeTransformation) -> Ge3DPoint {
    Ge3DPoint { x: t.x(), y: t.y(), z: t.z() }
}

/// What the constructors read of a unit (`EEUnit*`): its tile, class and stats numbers, its owner's colour, its
/// complex unit's physical model, animation, weapon tag point, velocity and high fire.
struct Reads {
    tile: (i64, i64),
    class_scale: f32,
    tile_size: c_long,
    unit_type: u64,
    shoots_high: bool,
    over_walls: bool,
    color: [f32; 3],
    complex: Option<ComplexReads>,
}

struct ComplexReads {
    physical: effect_reads::Physical,
    animation: usize,
    weapon_tag_point: c_ulong,
    moving: bool,
    firing_high: bool,
}

fn read(unit: u64) -> Reads {
    // SAFETY: an entry's unit is live; the world is held by the effects' callers
    unsafe {
        let t = WorldRead::begin();
        let u = UnitView::of(&t, unit).expect("an entry's unit");
        let class = u.class().expect("a unit's class").fields();
        let over_walls: bool = u.stats().is_some_and(|s| world_field!(s.fields(), mCanFireOverWalls));
        let complex = u.complex_unit().map(|c| {
            let weapon: c_ulong = world_field!(c.complex(), mWeaponTagPoint);
            let velocity: f32 = world_field!(c.complex(), mVelocity);
            ComplexReads {
                physical: effect_reads::physical(&c),
                animation: effect_reads::animation(&c),
                weapon_tag_point: weapon,
                moving: velocity != 0.0,
                firing_high: world_field!(c.complex(), mIsFiringHigh),
            }
        });
        Reads {
            tile: u.tile_xy(),
            class_scale: world_field!(class, mScale),
            tile_size: world_field!(class, mTileSize),
            unit_type: {
                let v: c_ulong = world_field!(class, mUnitType);
                v as u64
            },
            shoots_high: world_field!(class, mShootsHigh),
            over_walls,
            color: u.player().map_or([0.0; 3], |p| p.color()),
            complex,
        }
    }
}

fn graphic(unit: u64) -> Gobj {
    Gobj((world().unit_graphic)(std::ptr::null_mut(), unit) as usize)
}

/// `PickPoint()` of a unit's graphic: one of its attack points by the game time, else its default point.
fn pick_point(unit: u64) -> Ge3DPoint {
    let g = graphic(unit);
    let time = (world().tick)().game_time;
    g.pick(time as c_long).unwrap_or_else(|| g.default_point())
}

/// The shooter's weapon point (`EECombatEffectWeaponPoint` at `site`: with the tag points at its animation when
/// `test_list` finds them, else assumed) and whether its model has such points.
fn weapon_point(unit: u64, site: i32, test_list: bool) -> (Ge3DPoint, bool) {
    let mut tagged = false;
    let p = (world().weapon_point)(unit, site, test_list, &mut tagged);
    (p, tagged)
}

/// The 2D particle's start at a point, moving along `velocity` when given, for its graphic effect's duration from now.
fn start_particle(e: EffectPtr, at: Ge3DPoint, velocity: Option<Ge3DPoint>) {
    obj::ee_gfx_effect_set_position(e, at);
    let r = rec(e);
    r.particle.draw_position = at;
    if let Some(v) = velocity {
        r.particle.velocity = v;
    }
    r.particle.last_time = (world().tick)().game_clock;
    // SAFETY: a live effect
    let g = GraphicEffect(unsafe { (*e).graphic_effect } as usize);
    r.particle.duration = g.fields().mDuration as f32;
}

/// At a point: the 2D particle's start; the battlefield decal's size and corners on the terrain; the others' point
/// for their class (a 2D kind's `sprite.start`, a 3D kind's `aged.location`).
fn place_at(e: EffectPtr, at: Ge3DPoint) {
    let kind = rec(e).kind;
    if kind == PARTICLE_2D {
        start_particle(e, at, None);
        return;
    }
    if kind == DECAL {
        // SAFETY: a live effect
        let f = GraphicEffect(unsafe { (*e).graphic_effect } as usize).fields();
        let n = EeGfxAmbientNew {
            start_size: f.mStartSize,
            size: f.mSize,
            int_param1: f.mIntParam1 as i32,
            flag1: f.mFlag1,
        };
        let ground = (world().answers3d.particle_ground)(std::ptr::null_mut());
        // SAFETY: the effect's live record; the C++'s ground
        unsafe { crate::gfx::ffi_effect::ee_gfx_effect_ambient_new((*e).record, &at, &n, &ground) };
        return;
    }
    // SAFETY: a live effect
    if unsafe { (*e).is_2d } {
        rec(e).sprite.start = at;
    } else {
        rec(e).aged.location = at;
    }
    let mut point = at;
    call(e, EeGfxEffect3DOp::CreateAt, &mut point as *mut Ge3DPoint as *mut c_void);
}

/// On a unit: the destruction smoke at its location, spread over half its class's scale; the 2D particle at it or
/// its tile's centre on the ground (0 off the map); the multi-effect's own unit is none (its linked effects are made
/// on the unit); the light glow keeps its unit when asked, the confirmation shows goal queue `goal`, the emitter
/// follows tag point `tag`.
fn on_unit(e: EffectPtr, unit: u64, g: u64, kind: i32, keeps: bool, tag: i32, goal: u32) {
    if kind == SMOKE {
        let u = read(unit);
        attach(e, 0, g, kind, false);
        place_at(e, position(&u.complex.expect("the smoke's complex unit").physical.location));
        rec(e).smoke.radius = u.class_scale * 0.5;
        return;
    }
    if kind == PARTICLE_2D {
        attach(e, unit, g, kind, false);
        let u = read(unit);
        let at = match &u.complex {
            Some(c) => position(&c.physical.location),
            None => {
                let half = u.tile_size as f32 * 0.5;
                let (x, y) = (half + u.tile.0 as c_long as f32, half + u.tile.1 as c_long as f32);
                let mesh = (world().mesh)();
                // SAFETY: the map's live terrain mesh
                let z = unsafe {
                    if crate::ge::tmobj::ee_ge_tm_is_valid(mesh, u.tile.0 as _, u.tile.1 as _, true) {
                        crate::ge::tmobj::ee_ge_tm_mesh_z(mesh, x, y)
                    } else {
                        0.0
                    }
                };
                Ge3DPoint { x, y, z }
            }
        };
        start_particle(e, at, None);
        return;
    }
    attach(e, if kind == MULTI_EFFECT { 0 } else { unit }, g, kind, kind == LIGHT_GLOW && keeps);
    if kind == CONFIRMATION {
        rec(e).confirm.goal_index = goal;
    }
    call(e, EeGfxEffect3DOp::Create, unit as *mut c_void);
    if kind == EMITTER {
        rec(e).emitter.tag_point = tag as u32;
    }
}

/// From a unit to a point: the confirmation there; the smoke muzzle blast and the 2D projectile from the unit's weapon
/// point; the pulse cannon ball from its weapon point, the citizen beam towards it, the tracer beam to it.
fn unit_to_point(e: EffectPtr, unit: u64, target: Ge3DPoint, g: u64, kind: i32, goal: u32) {
    if kind == SMOKE_MUZZLE_BLAST || kind == PROJECTILE_2D {
        attach(e, 0, g, kind, false);
        let (start, _) = if kind == PROJECTILE_2D { weapon_point(unit, 2, true) } else { weapon_point(unit, 5, false) };
        rec(e).sprite.start = start;
        rec(e).sprite.draw_position = target;
        call(e, EeGfxEffect3DOp::CreateBetween, std::ptr::null_mut());
        return;
    }
    attach(e, unit, g, kind, false);
    if kind == CONFIRMATION {
        rec(e).position = target;
        rec(e).confirm.goal_index = goal;
        call(e, EeGfxEffect3DOp::CreateAt, std::ptr::null_mut());
        return;
    }
    if kind == TRACER_BEAM {
        rec(e).tracer.target_point = target;
        rec(e).tracer.weapon_tag_point = read(unit).complex.expect("the beam's complex unit").weapon_tag_point as i32;
    } else if kind == CITIZEN_BEAM {
        rec(e).shot.end = target;
    } else {
        rec(e).shot.path.start = weapon_point(unit, 6, true).0;
        rec(e).shot.location = target;
    }
    call(e, EeGfxEffect3DOp::Create, std::ptr::null_mut());
}

/// The unit's mouth under `t`, with the offsets of the cavalry archer, the chariot archer, the bronze spear cavalry
/// and the knight (`EEE2DWinterBreath::GetMouthPoint`).
fn mouth_point(unit_type: u64, t: &GeTransformation) -> Ge3DPoint {
    let (y, z) = match unit_type {
        0xb => (0.465, 0.385),
        0xc => (0.872, 0.465),
        0x89 | 0x8b => (0.272, 0.235),
        _ => (0.4, 0.405),
    };
    t.apply_point(Ge3DPoint { x: 0.0, y, z })
}

/// Between two units: the tracer beam of `from` on `to`, referenced, aimed at a point picked on its model; the tracers
/// and shells on `to` from `from`'s weapon point to a point of `to`'s model in `from`'s player colour; the winter
/// breath at `from`'s mouth tag point (or its mouth by unit type), its scaled location, whether the tag list was found
/// and whether it moves; the smoke muzzle blast and the 2D projectile from `from`'s weapon tag point to `to`'s model
/// point on its scaled location, the blast's 15 higher for a high-arc shooter.
fn between_units(e: EffectPtr, from: u64, to: u64, g: u64, kind: i32) {
    if kind == TRACER_BEAM {
        attach(e, from, g, kind, false);
        // SAFETY: a live effect
        unsafe { (*e).target = to };
        if to != 0 {
            (world().unit_reference)(to);
            rec(e).tracer.target_point = pick_point(to);
        }
        rec(e).tracer.weapon_tag_point = read(from).complex.expect("the beam's complex unit").weapon_tag_point as i32;
        call(e, EeGfxEffect3DOp::Create, std::ptr::null_mut());
        return;
    }
    if kind == TRACERS || kind == SHELLS {
        attach(e, to, g, kind, false);
        let target = read(to).complex.expect("the target's complex unit").physical;
        let mut t = target.location;
        let (start, tagged) = weapon_point(from, 7, true);
        rec(e).shot.path.start = start;
        if tagged {
            crate::gfx::tags::scale(&mut t, target.scale);
        }
        let end = t.apply_point(pick_point(to));
        let c = read(from).color;
        rec(e).shot.location = end;
        rec(e).shot.color = crate::gfx::effects::EeGfxColor { red: c[0], green: c[1], blue: c[2] };
        call(e, EeGfxEffect3DOp::Create, std::ptr::null_mut());
        return;
    }
    attach(e, 0, g, kind, false);
    let shooter = read(from);
    let c = shooter.complex.as_ref().expect("the shooter's complex unit");
    if kind == WINTER_BREATH {
        let mut t = c.physical.location;
        let mouths = graphic(from).tag_points(c.animation, 0x16);
        crate::gfx::tags::scale(&mut t, c.physical.scale);
        let at = match mouths {
            Some(list) => effect_reads::tag_point(list, 0, c.physical.frame, &t, None),
            None => mouth_point(shooter.unit_type, &t),
        };
        let r = rec(e);
        r.sprite.start = at;
        r.aged.transformation = t;
        r.sprite.flag = mouths.is_some();
        r.sprite.shrinking = c.moving;
        call(e, EeGfxEffect3DOp::CreateBetween, std::ptr::null_mut());
        return;
    }
    let projectile = kind == PROJECTILE_2D;
    let target = read(to).complex.expect("the target's complex unit").physical;
    let mut t = target.location;
    let (start, _) = weapon_point(from, if projectile { 1 } else { 4 }, projectile);
    crate::gfx::tags::scale(&mut t, target.scale);
    let mut end = t.apply_point(graphic(to).default_point());
    if !projectile && (shooter.shoots_high || (shooter.over_walls && c.firing_high)) {
        end.z += TARGET_LIFT;
    }
    rec(e).sprite.start = start;
    rec(e).sprite.draw_position = end;
    call(e, EeGfxEffect3DOp::CreateBetween, std::ptr::null_mut());
}

/// The effect of an entry point: between two points the marching line (no set-up call), the linear projectile from
/// the start to the end, the earthquake's crack; along a line the 2D particle at its start with its direction as
/// velocity, the velocity particle, the 3D particle and the emitter at its start moving along it, the multi-effect's
/// linked effects along it; rain and snow over the tiles, centred on them; the water ripple at a transformation, the
/// battlefield decal at its point.
pub(crate) fn construct(a: &EeGfxEffectArgs) -> EffectPtr {
    let e = obj::ee_gfx_effect_new();
    let g = a.graphic_effect;
    let kind = GraphicEffect(g as usize).fields().mKind as i32;
    match a.entry {
        EeGfxEffectEntry::Unit => on_unit(e, a.unit, g, kind, a.keeps_unit, a.tag_point, a.goal_index),
        EeGfxEffectEntry::BetweenUnits => between_units(e, a.unit, a.to_unit, g, kind),
        EeGfxEffectEntry::UnitToPoint => unit_to_point(e, a.unit, a.at, g, kind, a.goal_index),
        EeGfxEffectEntry::Placed => {
            attach(e, 0, g, kind, false);
            place_at(e, a.at);
        }
        EeGfxEffectEntry::Transformation => {
            attach(e, 0, g, kind, false);
            if kind == DECAL {
                place_at(e, position(&a.transformation));
            } else {
                rec(e).aged.transformation = a.transformation;
                call(e, EeGfxEffect3DOp::CreateTransformation, std::ptr::null_mut());
            }
        }
        EeGfxEffectEntry::Line => {
            attach(e, 0, g, kind, false);
            let line: Ge3DLine = a.line;
            let d = line.direction;
            let velocity = Ge3DPoint { x: d.x, y: d.y, z: d.z };
            if kind == PARTICLE_2D {
                start_particle(e, line.start, Some(velocity));
                return e;
            }
            if kind == VELOCITY_PARTICLE {
                rec(e).sprite.start = line.start;
                rec(e).sprite.velocity = velocity;
            } else if kind == PARTICLE_3D || kind == EMITTER {
                rec(e).aged.location = line.start;
                if kind == PARTICLE_3D {
                    rec(e).particle.velocity = velocity;
                } else {
                    rec(e).emitter.velocity = velocity;
                }
            }
            let mut arg = line;
            call(e, EeGfxEffect3DOp::CreateLine, &mut arg as *mut Ge3DLine as *mut c_void);
        }
        EeGfxEffectEntry::Between => {
            attach(e, 0, g, kind, false);
            if kind == MARCHING_LINE {
                rec(e).march.from = a.at;
                rec(e).march.to = a.to;
            } else {
                rec(e).shot.path.start = a.at;
                rec(e).shot.location = a.to;
                call(e, EeGfxEffect3DOp::Create, std::ptr::null_mut());
            }
        }
        EeGfxEffectEntry::Weather => {
            attach(e, 0, g, kind, false);
            // SAFETY: the effect's live record; the entry's tiles
            unsafe { crate::gfx::ffi_effect::ee_gfx_storm_new((*e).record, a.tiles, a.tile_count) };
            call(e, EeGfxEffect3DOp::Create, std::ptr::null_mut());
        }
    }
    e
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_mouth_offsets_follow_the_unit_type() {
        let mut t = GeTransformation::IDENTITY;
        t.m[0][3] = 10.0;
        assert_eq!(mouth_point(0xb, &t), Ge3DPoint { x: 10.0, y: 0.465, z: 0.385 });
        assert_eq!(mouth_point(0xc, &t), Ge3DPoint { x: 10.0, y: 0.872, z: 0.465 });
        assert_eq!(mouth_point(0x89, &t), mouth_point(0x8b, &t));
        assert_eq!(mouth_point(0x50, &t), Ge3DPoint { x: 10.0, y: 0.4, z: 0.405 });
    }
}
