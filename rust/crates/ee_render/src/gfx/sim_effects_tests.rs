//! sim_effects.rs' tests: each of ee_sim's effect places becomes the entry point's arguments that
//! `EEEffectManager`'s `Create*` method builds (ee/EEEffectManager.h), every other member at its default.

use super::sim_effects::args_of;
use crate::gfx::effect_manager::EeGfxEffectEntry;
use crate::gfx::ffi_manager::EeGfxEffectArgs;
use ee_base::math::types::{Ge3DLine, Ge3DPoint, GeTransformation, GeVector};
use ee_sim::world::effects::{EffectPlace, EffectRequest};

const G: u64 = 0x1234_5678;

fn args(place: EffectPlace) -> EeGfxEffectArgs {
    args_of(&EffectRequest { graphic_effect: G, place })
}

fn base(entry: EeGfxEffectEntry) -> EeGfxEffectArgs {
    EeGfxEffectArgs { entry, graphic_effect: G, ..EeGfxEffectArgs::default() }
}

fn same(a: &EeGfxEffectArgs, b: &EeGfxEffectArgs) {
    // EeGfxEffectArgs holds a raw pointer: compare member by member
    assert_eq!(a.entry, b.entry);
    assert_eq!(a.graphic_effect, b.graphic_effect);
    assert_eq!((a.unit, a.to_unit), (b.unit, b.to_unit));
    assert_eq!((a.at, a.to), (b.at, b.to));
    assert_eq!(a.transformation, b.transformation);
    assert_eq!(a.line, b.line);
    assert_eq!((a.tiles, a.tile_count), (b.tiles, b.tile_count));
    assert_eq!((a.keeps_unit, a.tag_point, a.goal_index), (b.keeps_unit, b.tag_point, b.goal_index));
}

const P: Ge3DPoint = Ge3DPoint { x: 1.5, y: -2.0, z: 3.25 };
const Q: Ge3DPoint = Ge3DPoint { x: 7.0, y: 8.0, z: 9.0 };

#[test]
fn on_a_unit() {
    let a = args(EffectPlace::Unit { unit: 0x10, keeps_unit: true, tag_point: 4, goal_index: 2 });
    same(&a, &EeGfxEffectArgs { unit: 0x10, keeps_unit: true, tag_point: 4, goal_index: 2, ..base(EeGfxEffectEntry::Unit) });
}

#[test]
fn between_units_and_to_a_point() {
    let a = args(EffectPlace::BetweenUnits { from: 0x10, to: 0x20 });
    same(&a, &EeGfxEffectArgs { unit: 0x10, to_unit: 0x20, ..base(EeGfxEffectEntry::BetweenUnits) });
    let a = args(EffectPlace::UnitToPoint { unit: 0x30, at: P, goal_index: 1 });
    same(&a, &EeGfxEffectArgs { unit: 0x30, at: P, goal_index: 1, ..base(EeGfxEffectEntry::UnitToPoint) });
}

#[test]
fn placed_transformation_line_between() {
    same(&args(EffectPlace::Placed { at: P }), &EeGfxEffectArgs { at: P, ..base(EeGfxEffectEntry::Placed) });
    let mut t = GeTransformation::IDENTITY;
    t.m[0][3] = 5.0;
    same(
        &args(EffectPlace::Transformation(t)),
        &EeGfxEffectArgs { transformation: t, ..base(EeGfxEffectEntry::Transformation) },
    );
    let l = Ge3DLine { start: P, direction: GeVector { x: 0.0, y: 1.0, z: 0.0 } };
    same(&args(EffectPlace::Line(l)), &EeGfxEffectArgs { line: l, ..base(EeGfxEffectEntry::Line) });
    same(
        &args(EffectPlace::Between { from: P, to: Q }),
        &EeGfxEffectArgs { at: P, to: Q, ..base(EeGfxEffectEntry::Between) },
    );
}

#[test]
fn weather_tiles_as_x_y_pairs() {
    let tiles = [[3, 4], [5, 6], [7, 8]];
    let a = args(EffectPlace::Weather { tiles: &tiles });
    assert_eq!(a.entry, EeGfxEffectEntry::Weather);
    assert_eq!(a.tile_count, 3);
    // SAFETY: the pairs' six ints
    let flat = unsafe { std::slice::from_raw_parts(a.tiles, 6) };
    assert_eq!(flat, &[3, 4, 5, 6, 7, 8]);
    same(&a, &EeGfxEffectArgs { tiles: a.tiles, tile_count: 3, ..base(EeGfxEffectEntry::Weather) });
}
