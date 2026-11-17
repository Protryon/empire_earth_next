//! `effect_glow.rs`'s name scan and virtuals over the scripted host and the logging rasterizer.

use super::*;
use crate::gfx::effect_2d::{effect2d, effect2d_file};
use crate::gfx::effect_2d_mock::{Host, Mem};
use crate::gfx::raster::LogRaster;

#[test]
fn scans_as_sscanf() {
    assert_eq!(scan_float(b"255_"), Some(255.0));
    assert_eq!(scan_float(b" -1.5e2x"), Some(-150.0));
    assert_eq!(scan_float(b".5_"), Some(0.5));
    assert_eq!(scan_float(b"7.e"), Some(7.0));
    assert_eq!(scan_float(b"_"), None);
    assert_eq!(scan_float(b"."), None);
    assert_eq!(scan_long(b"500_"), Some(500));
    assert_eq!(scan_long(b"+12"), Some(12));
    assert_eq!(scan_long(b"-3"), Some(-3));
    assert_eq!(scan_long(b"x1"), None);
}

#[test]
fn a_full_name_gives_every_field() {
    let mut rgb = [0.0; 3];
    let s = spot_from_name(b"light_255_128_0_300_1000_250", &mut rgb);
    assert_eq!(s.color, EeGfxColor { red: 1.0, green: 128.0 / 255.0, blue: 0.0 });
    assert_eq!((s.on_time, s.period, s.start), (300, 1000, 250));
}

#[test]
fn missing_fields_take_the_defaults() {
    let mut rgb = [0.0; 3];
    // no underscore: each colour 1 (over 255, as the original: nearly black), 500 on, the period the on time, and the
    // start scanned from the whole name (none)
    let s = spot_from_name(b"light", &mut rgb);
    let dim = 1.0f32 / 255.0;
    assert_eq!(s.color, EeGfxColor { red: dim, green: dim, blue: dim });
    assert_eq!((s.on_time, s.period, s.start), (500, 500, 0));
    // the colour only: the timing's defaults
    let s = spot_from_name(b"l_10_20_30", &mut rgb);
    assert_eq!(s.color, EeGfxColor { red: 10.0 / 255.0, green: 20.0 / 255.0, blue: dim });
    assert_eq!((s.on_time, s.period), (500, 500));
    // a field that does not scan keeps the carried colour
    let mut rgb = [51.0, 0.0, 0.0];
    let s = spot_from_name(b"l_x_0_0_100_200_", &mut rgb);
    assert_eq!(s.color.red, 51.0 / 255.0);
    assert_eq!((s.on_time, s.period, s.start), (100, 200, 0));
    // a bare number after the fifth underscore is the start
    let s = spot_from_name(b"l_1_2_3_4_5_6", &mut rgb);
    assert_eq!((s.on_time, s.period, s.start), (4, 5, 6));
}

#[test]
fn set_up_makes_a_spot_per_tag_or_one_default() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_LIGHT_GLOW);
    let mut h = Host {
        set_up: EeGfxGlowSetUp { count: 2, position: Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 } },
        names: vec!["light_255_0_0_100_200_0", "light_0_255_0_50_100_10"],
        ..Default::default()
    };
    let mut x = LogRaster::default();
    let g = EeGfxGraphicFacts { size: 1.5, ..Default::default() };
    // SAFETY (each call): the record is live and only reached through this pointer during the call
    unsafe {
        assert_eq!(effect2d(EeGfxEffect3DOp::SetUp, &mut r, &g, &mut h, &mut x), 0);
        assert_eq!(spots(&mut r).len(), 2);
        assert_eq!(spots(&mut r)[1].start, 10);
        assert_eq!((r.position, r.bounding_radius), (Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 }, 1.5));
        h.set_up.count = 0;
        effect2d(EeGfxEffect3DOp::SetUp, &mut r, &g, &mut h, &mut x);
        let s = spots(&mut r).clone();
        assert_eq!(s.len(), 1);
        assert_eq!((s[0].color, s[0].on_time, s[0].period, s[0].start), (WHITE, 500, 500, 0));
    }
    assert_eq!(&h.calls[..3], ["media", "glow setup", "move"]);
}

#[test]
fn step_blinks_the_spots_on_screen_and_hides_off_it() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_LIGHT_GLOW);
    let mut h = Host {
        set_up: EeGfxGlowSetUp { count: 2, position: Ge3DPoint::default() },
        names: vec!["light_255_0_0_100_200_0", "light_0_255_0_50_100_300"],
        place: EeGfxGlowPlace { show: true, has_tags: true, position: Ge3DPoint { x: 5.0, y: 6.0, z: 7.0 } },
        area: EeGfxGlowArea { x: 10, y: 10, width: 20, height: 20, tile_x: 8, tile_y: 32 },
        rows: 1,
        ..Default::default()
    };
    let mut x = LogRaster::default();
    let g = EeGfxGraphicFacts { tick_time: 150.0, unit_hit_points: 3, time1: 300, size: 2.0, ..Default::default() };
    // SAFETY (each call): as above
    unsafe {
        effect2d(EeGfxEffect3DOp::SetUp, &mut r, &g, &mut h, &mut x);
        // age 150: the first lit (150 % 200 > 100: off), the second not started
        assert_eq!(effect2d(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x), 0);
        assert!(!r.glow.hidden);
        assert_eq!(r.position, Ge3DPoint { x: 5.0, y: 6.0, z: 7.0 });
        assert_eq!((spots(&mut r)[0].visible, spots(&mut r)[1].visible), (false, false));
        // age 300: (300 - 0) % 200 = 100 <= 100 lit on its tag; (300 - 300) % 100 = 0 lit
        effect2d(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x);
        assert_eq!((spots(&mut r)[0].visible, spots(&mut r)[1].visible), (true, true));
        assert_eq!(spots(&mut r)[1].position, Ge3DPoint { x: 1.0, y: 10.0, z: 20.0 });
        assert_eq!(effect2d(EeGfxEffect3DOp::Queue, &mut r, &g, &mut h, &mut x), 0);
        assert_eq!(r.glow.draw_age, 1.0);
        effect2d(EeGfxEffect3DOp::DrawPass6, &mut r, &g, &mut h, &mut x);
        assert_eq!(x.calls.len(), 2);
        assert_eq!(x.calls[1], "sprite 0 1 10 20 color 0 1 0 rotation 0 size 2 1 1 0");
        // not shown (a cloaked enemy): every spot off
        h.place.show = false;
        effect2d(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x);
        assert!(!spots(&mut r)[0].visible);
        // the unit's tile two past the visible area's corner is outside: hidden, the queue skips it
        h.area.tile_y = 33;
        effect2d(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x);
        assert!(r.glow.hidden);
        assert_eq!(effect2d(EeGfxEffect3DOp::Queue, &mut r, &g, &mut h, &mut x), 9);
        // no visible rows: hidden; no hit points: 7
        h.area.tile_y = 20;
        h.rows = 0;
        effect2d(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x);
        assert!(r.glow.hidden);
        let g = EeGfxGraphicFacts { unit_hit_points: 0, ..g };
        assert_eq!(effect2d(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x), 7);
    }
}

#[test]
fn the_file_holds_the_age() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_LIGHT_GLOW);
    r.glow.age = 1234.5;
    let mut h = Host::default();
    let mut f = Mem::default();
    assert_eq!(effect2d_file(EeGfxEffect3DOp::Save, &mut r, &mut f, 0, 0, &mut h), 0);
    let mut back = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_LIGHT_GLOW);
    assert_eq!(effect2d_file(EeGfxEffect3DOp::Load, &mut back, &mut f, 0, 0, &mut h), 0);
    assert_eq!(back.glow.age, 1234.5);
    assert_eq!(effect2d_file(EeGfxEffect3DOp::Load, &mut back, &mut f, 0, 0, &mut h), 3);
}
