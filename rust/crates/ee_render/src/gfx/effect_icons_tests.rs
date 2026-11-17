//! `effect_icons.rs`'s virtuals over the scripted host and the logging rasterizer.

use super::*;
use crate::gfx::effect_2d::{effect2d, effect2d_file};
use crate::gfx::effect_2d_mock::{Host, Mem};
use crate::gfx::effect_state::EE_GFX_EFFECT_KIND_CIRCULAR_ICONS;
use crate::gfx::raster::LogRaster;

fn facts() -> EeGfxGraphicFacts {
    EeGfxGraphicFacts {
        int_param1: 4,
        int_param2: 90,
        int_param3: 7,
        param1: 2.0,
        size: 0.0,
        tick_time: 1000.0,
        unit_hit_points: 5,
        ..Default::default()
    }
}

fn owner() -> Host {
    let mut h = Host {
        unit: EeGfxIconUnit { has_unit: true, has_player: true, player: 3, x: 10.0, y: 20.0, ..Default::default() },
        owner: true,
        selected: true,
        ..Default::default()
    };
    h.location.m[0][3] = 10.0;
    h.location.m[1][3] = 20.0;
    h.location.m[2][3] = 1.0;
    h
}

#[test]
fn create_takes_the_graphic_effects_values() {
    for (kind, radius) in
        [(EE_GFX_EFFECT_KIND_CIRCULAR_ICONS, 2.0), (EE_GFX_EFFECT_KIND_SQUARE_ICONS, square_radius(2.0))]
    {
        let mut r = EeGfxEffectRecord::new(kind);
        let mut h = Host::default();
        let mut x = LogRaster::default();
        // SAFETY: the record is live and only reached through this pointer during the call
        unsafe { effect2d(EeGfxEffect3DOp::Create, &mut r, &facts(), &mut h, &mut x) };
        let m = r.marching;
        assert_eq!((m.count, m.area_type, m.visible, r.bounding_radius), (4, 7, false, radius));
        assert_eq!(m.speed, 90.0f32 * (1.0 / 180.0) * 3.141_592_7);
    }
}

#[test]
fn step_lays_out_the_uncovered_icons_and_turns() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_CIRCULAR_ICONS);
    let mut h = owner();
    let mut x = LogRaster::default();
    let g = facts();
    // the icon at angle 0 (offset (2, 0)) is under the owner's area effect
    h.covered.push((12.0, 20.0));
    // SAFETY (each call): as above
    unsafe {
        effect2d(EeGfxEffect3DOp::Create, &mut r, &g, &mut h, &mut x);
        assert_eq!(effect2d(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x), 0);
        assert!(r.marching.visible);
        assert_eq!(lists(&mut r).offsets.len(), 3);
        // a half turn a second
        assert!((r.marching.angle - 1.570_796_4).abs() < 1e-5);
        assert_eq!(r.position, Ge3DPoint { x: 10.0, y: 20.0, z: 1.0 });
        // not selected and no ghosts: hidden, the queue skips it
        h.selected = false;
        assert_eq!(effect2d(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x), 0);
        assert!(!r.marching.visible);
        assert_eq!(effect2d(EeGfxEffect3DOp::Queue, &mut r, &g, &mut h, &mut x), 10);
        // ghost units show it again
        h.ghosts = 1;
        effect2d(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x);
        assert_eq!(effect2d(EeGfxEffect3DOp::Queue, &mut r, &g, &mut h, &mut x), 0);
        assert_eq!(lists(&mut r).shown, lists(&mut r).offsets);
        assert_eq!(r.marching.location, Ge3DPoint { x: 10.0, y: 20.0, z: 1.0 });
        // a pending area effect ends it, counted down
        h.pending = 2;
        assert_eq!(effect2d(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x), 7);
        assert_eq!(h.pending, 1);
    }
    assert!(h.calls.contains(&"queue 4".to_string()));
    // no type: every icon shows, with no area question
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_SQUARE_ICONS);
    let mut h = owner();
    let g = EeGfxGraphicFacts { int_param3: 0, ..facts() };
    // SAFETY: as above
    unsafe {
        effect2d(EeGfxEffect3DOp::Create, &mut r, &g, &mut h, &mut x);
        effect2d(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x);
        assert_eq!(lists(&mut r).offsets.len(), 4);
    }
    assert!(!h.calls.iter().any(|c| c.starts_with("area")));
}

#[test]
fn a_land_building_is_tested_from_its_centre_and_a_dead_unit_ends() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_CIRCULAR_ICONS);
    let mut h = owner();
    h.unit.land_building = true;
    h.unit.tile_size = 3;
    let mut x = LogRaster::default();
    let g = EeGfxGraphicFacts { int_param1: 1, ..facts() };
    // SAFETY (each call): as above
    unsafe {
        effect2d(EeGfxEffect3DOp::Create, &mut r, &g, &mut h, &mut x);
        effect2d(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x);
        assert_eq!(h.calls.iter().find(|c| c.starts_with("area")).unwrap(), "area 7 13.5 21.5 3");
        let g = EeGfxGraphicFacts { unit_hit_points: 0, ..g };
        assert_eq!(effect2d(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x), 7);
    }
    // without a player nothing shows
    let mut h = owner();
    h.unit.has_player = false;
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_CIRCULAR_ICONS);
    // SAFETY: as above
    unsafe {
        effect2d(EeGfxEffect3DOp::Create, &mut r, &facts(), &mut h, &mut x);
        effect2d(EeGfxEffect3DOp::Step, &mut r, &facts(), &mut h, &mut x);
        assert!(lists(&mut r).offsets.is_empty());
    }
}

#[test]
fn draw_puts_each_icon_on_the_ground_fog_lit() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_CIRCULAR_ICONS);
    r.marching.location = Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 };
    // SAFETY: the record is live
    unsafe {
        lists(&mut r).shown =
            vec![Ge3DPoint { x: 1.0, y: 0.0, z: 0.0 }, Ge3DPoint { x: -5.0, y: 0.0, z: 0.0 }, Ge3DPoint::default()];
    }
    let mut h = Host::default();
    let mut x = LogRaster { fog: 9, fog_value: 0.5, ..Default::default() };
    let g = EeGfxGraphicFacts { has_object: true, alpha_ref: 0.25, ..facts() };
    // SAFETY: as above
    let res = unsafe { effect2d(EeGfxEffect3DOp::DrawPass6, &mut r, &g, &mut h, &mut x) };
    assert_eq!(res, 0);
    assert_eq!(&x.calls[..4], ["alpharef 0.25", "zbuffer 2", "blend 1 0", "lighting White"]);
    // the off-map icon (x -4) is skipped; the size 0 draws at scale 1
    assert_eq!(x.calls.iter().filter(|c| c.starts_with("draw")).count(), 2);
    assert!(x.calls.contains(&"transform 2 2 2.05".to_string()));
    assert!(x.calls.contains(&"scale 1".to_string()));
    assert_eq!(x.calls.last().unwrap(), "fog 9");
}

#[test]
fn files_round_trip_and_a_failed_media_stops_the_load() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_SQUARE_ICONS);
    r.marching = EeGfxEffectMarching { angle: 1.5, speed: -0.5, count: 6, area_type: 11, ..Default::default() };
    let mut h = Host::default();
    let mut f = Mem::default();
    assert_eq!(effect2d_file(EeGfxEffect3DOp::Save, &mut r, &mut f, 0, 0, &mut h), 0);
    let mut back = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_SQUARE_ICONS);
    back.marching.area_type = 5;
    assert_eq!(effect2d_file(EeGfxEffect3DOp::Load, &mut back, &mut f, 0, 0, &mut h), 0);
    assert_eq!(back.marching, r.marching);
    // a short file leaves no type
    let mut f = Mem { items: vec![('f', 0)], at: 0 };
    let mut back = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_SQUARE_ICONS);
    back.marching.area_type = 5;
    assert_eq!(effect2d_file(EeGfxEffect3DOp::Load, &mut back, &mut f, 0, 0, &mut h), 3);
    assert_eq!(back.marching.area_type, 0);
    h.media = 4;
    let mut f = Mem::default();
    assert_eq!(effect2d_file(EeGfxEffect3DOp::Load, &mut back, &mut f, 0, 0, &mut h), 4);
}
