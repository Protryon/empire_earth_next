use super::*;
use crate::gfx::raster::LogRaster;
use std::sync::Mutex;

/// The media are one set of statics: one test at a time.
static LOCK: Mutex<()> = Mutex::new(());

extern "C" fn no_effects(context: *mut c_void) {
    // SAFETY: the tests pass a Vec<String>
    unsafe { (*(context as *mut Vec<String>)).push("effects".into()) }
}

fn unit() -> EeGfxRenderUnit {
    let mut t = GeTransformation::IDENTITY;
    t.m[0][3] = 10.0;
    t.m[1][3] = 20.0;
    t.m[2][3] = 1.0;
    EeGfxRenderUnit {
        transformation: t,
        player_color: c(0.5, 0.25, 0.125),
        tint_color: c(0.0, 0.5, 1.0),
        object: 0x10 as EeGfxModel,
        model: 0x20 as EeGfxModel,
        texture: 0x30 as EeGfxModel,
        center: Ge3DPoint::default(),
        bar: Ge3DPoint { x: 0.0, y: 0.0, z: 2.0 },
        drawn: true,
        alpha_ref: 0.5,
        object_opacity: 0.8,
        object2: std::ptr::null_mut(),
        model2: std::ptr::null_mut(),
        texture2: std::ptr::null_mut(),
        drawn2: false,
        health_bars: std::ptr::null(),
        health_bar_count: 0,
        turret_angles: std::ptr::null(),
        turret_models: std::ptr::null(),
        turret_count: 0,
        orbital_angles: std::ptr::null(),
        orbital_models: std::ptr::null(),
        orbital_count: 0,
        wheel_angles: std::ptr::null(),
        wheel_models: std::ptr::null(),
        wheel_count: 0,
        name: std::ptr::null(),
        scale: 1.5,
        selection_size: 1.0,
        opacity: 1.0,
        health: -1.0,
        fraction: 0.25,
        sub_model_offset_scale: 0.0,
        fog_scale: 0.6,
        inner_collision_sphere: 0.0,
        category: 3,
        command_type: 0,
        animation: 2,
        current_frame: 4,
        next_frame: 5,
        area_effect_strength: 0,
        air: false,
        waypoint: false,
        fogged: false,
        tinted: false,
        has_sub_model_offset: false,
        second_full_opacity: false,
    }
}

struct Frame {
    vp: GeViewport,
    units: Vec<EeGfxRenderUnit>,
    units0: Vec<u32>,
    selected: Vec<u32>,
    log: Vec<String>,
}

impl Frame {
    fn new(units: Vec<EeGfxRenderUnit>) -> Frame {
        let all = (0..units.len() as u32).collect::<Vec<_>>();
        Frame { vp: GeViewport::default(), units, units0: all.clone(), selected: all, log: Vec::new() }
    }
    fn scene(&mut self) -> EeGfxUnitsScene {
        EeGfxUnitsScene {
            context: &mut self.log as *mut Vec<String> as *mut c_void,
            viewport: &self.vp,
            orthographic: false,
            cinematic: false,
            collision_spheres: false,
            interpolate_frames: true,
            model_detail: 0,
            free_camera: false,
            panel_active: false,
            grid_phase: 0.5,
            camera_yaw: 0.0,
            window_width: 1024,
            window_height: 768,
            units: self.units.as_ptr(),
            unit_count: self.units.len(),
            objects: std::ptr::null(),
            object_count: 0,
            units0: self.units0.as_ptr(),
            units0_count: self.units0.len(),
            units1: std::ptr::null(),
            units1_count: 0,
            selected_units: self.selected.as_ptr(),
            selected_unit_count: self.selected.len(),
            objects0: std::ptr::null(),
            objects0_count: 0,
            objects1: std::ptr::null(),
            objects1_count: 0,
            selected_objects: std::ptr::null(),
            selected_object_count: 0,
            selected_unit: -1,
            selected_object: -1,
            ghosts: std::ptr::null(),
            ghost_count: 0,
            debug_unit: -1,
            debug_text: std::ptr::null(),
            draw_effects: no_effects,
        }
    }
}

#[test]
fn the_media_are_loaded_and_released() {
    let _g = LOCK.lock().unwrap();
    let mut x = LogRaster::default();
    load_media(&mut x, 2);
    let made: Vec<_> = x.models.iter().map(|m| m.name.clone()).collect();
    assert_eq!(made, ["area effect", "unit selection", "unit selection", "unit selection"]);
    let textures: Vec<_> = x.calls.iter().filter(|c| c.starts_with("texture ")).cloned().collect();
    assert_eq!(textures[0], "texture textures\\DottedLine 16 false 0 0 2 true");
    assert_eq!(textures[1], "texture textures\\UnitCommandConfirmation 16 true 1 1 2 true");
    assert_eq!(textures.len(), 6);
    assert_eq!(x.model(get(RING_MODEL)).triangles.len(), 10);
    assert_eq!(x.model(get(SELECTION0 + 2)).opacities, vec![1.0; 20]);
    assert!(!command_texture(1).is_null() && !line_texture().is_null());
    release_media(&mut x);
    assert_eq!(x.calls.iter().filter(|c| c.starts_with("dereference")).count(), 4);
    assert_eq!(x.calls.iter().filter(|c| c.starts_with("texture dereference")).count(), 6);
    assert!(command_texture(0).is_null());
}

#[test]
fn air_units_get_the_small_ring_and_ground_units_one_by_size() {
    let mut air = unit();
    air.air = true;
    let mut big = unit();
    big.selection_size = 2.0;
    let mut f = Frame::new(vec![air, big]);
    let s = f.scene();
    // SAFETY: the frame's arrays live
    let l = unsafe { s.lists() };
    let mut x = LogRaster::default();
    draw_selection_rings(&mut x, &l);
    assert_eq!(x.calls.iter().filter(|c| c.starts_with("scale")).cloned().collect::<Vec<_>>(), ["scale 1"]);
    let mut x = LogRaster::default();
    draw_selection(&mut x, &l, false);
    // size 2 picks the third ring, grown by 0.3
    let scales: Vec<_> = x.calls.iter().filter(|c| c.starts_with("scale")).cloned().collect();
    assert!(scales.len() == 1 && scales[0].starts_with("scale 2.6"), "{scales:?}");
}

#[test]
fn the_area_effect_ring_colours_one_segment_per_strength_point() {
    let _g = LOCK.lock().unwrap();
    let mut x = LogRaster::default();
    load_media(&mut x, 0);
    let mut u = unit();
    u.area_effect_strength = 3;
    let mut f = Frame::new(vec![u, unit()]);
    let s = f.scene();
    // SAFETY: the frame's arrays live
    let l = unsafe { s.lists() };
    x.calls.clear();
    draw_rings(&mut x, &l, 0.0);
    assert!(x.calls.contains(&format!("store {} 21 0", get(RING_MODEL) as usize)));
    assert_eq!(x.calls.iter().filter(|c| c.starts_with("draw")).count(), 1);
    release_media(&mut x);
}

#[test]
fn a_unit_model_takes_its_detail_opacity_and_second_model() {
    let mut u = unit();
    u.object2 = 0x40 as EeGfxModel;
    u.model2 = 0x50 as EeGfxModel;
    u.drawn2 = true;
    u.second_full_opacity = true;
    u.opacity = 0.5;
    let angles = [0.25_f32];
    let models = [0x60 as EeGfxModel];
    u.wheel_angles = angles.as_ptr();
    u.wheel_models = models.as_ptr();
    u.wheel_count = 1;
    let mut f = Frame::new(vec![u]);
    let mut s = f.scene();
    s.model_detail = 1;
    let mut x = LogRaster::default();
    draw_unit_model(&mut x, &s, &u);
    let want = [
        "alpharef 0.5",
        "zbuffer 2",
        "group 16 0.5 0.25 0.125 2",
        "opacity 0.5",
        "own texture",
        "frame textures 32 48",
        "pre 1",
        "frame 4",
        "draw 0.4",
        "texture",
        "group 64 0.5 0.25 0.125 0",
        "transform 10 20 1",
        "opacity 1",
        "draw 0.8",
    ];
    for w in want {
        assert!(x.calls.iter().any(|c| c == w), "{w} in {:?}", x.calls);
    }
    let mut hidden = unit();
    hidden.drawn = false;
    let mut x = LogRaster::default();
    draw_unit_model(&mut x, &s, &hidden);
    assert!(x.calls.is_empty());
}

#[test]
fn a_tinted_unit_is_drawn_in_its_fog_band_and_the_fog_put_back() {
    let mut u = unit();
    u.tinted = true;
    let mut f = Frame::new(vec![u]);
    let s = f.scene();
    // SAFETY: the frame's arrays live
    let l = unsafe { s.lists() };
    let mut x = LogRaster { fog: 7, can_modulate: false, ..LogRaster::default() };
    x.fog_attributes.kind = 3;
    draw_unit_models(&mut x, &s, &l, l.units0);
    let fog: Vec<_> = x.calls.iter().filter(|c| c.starts_with("fog")).cloned().collect();
    assert_eq!(fog[0], "fog 1");
    assert_eq!(fog[1], "fog color 0 0.5 1");
    assert_eq!(&fog[3..5], ["fog 7", "fog type 3"]);
}

#[test]
fn the_scene_skips_the_rings_and_bars_in_cinematics() {
    let mut f = Frame::new(vec![unit()]);
    let mut s = f.scene();
    s.cinematic = true;
    let mut x = LogRaster { alpha_ref: 0.75, ..LogRaster::default() };
    render_scene(&mut x, &s);
    assert!(!x.calls.iter().any(|c| c.starts_with("depth")));
    assert_eq!(x.calls.last().unwrap(), "alpharef 0.75");
    assert_eq!(f.log, ["effects"]);
}

#[test]
fn labels_are_a_blank_led_command_name_or_the_debug_label() {
    let named = b" Name\0";
    let plain = b"Name\0";
    let debug = b"Debug\0";
    let mut a = unit();
    a.command_type = 1;
    a.name = named.as_ptr();
    let mut b = unit();
    b.command_type = 1;
    b.name = plain.as_ptr();
    let mut d = unit();
    d.player_color = c(0.25, 0.0, 0.0);
    let mut f = Frame::new(vec![a, b, d, unit()]);
    let mut s = f.scene();
    s.debug_unit = 2;
    s.debug_text = debug.as_ptr();
    let mut x = LogRaster::default();
    draw_labels(&mut x, &s);
    let labels: Vec<_> = x.calls.iter().map(|c| c.split(' ').nth(1).unwrap_or("").to_string()).collect();
    assert_eq!(labels, ["", "Debug"]);
    assert!(x.calls[1].contains(" 0.25 "));
}

#[test]
fn a_placement_outlines_land_buildings_in_batches() {
    let mut g = EeGfxGhostRecord {
        unit: unit(),
        heights: [[0.0; 12]; 12],
        cells: [[1; 12]; 12],
        tile_x: 4,
        tile_y: 5,
        grid_size: 3,
        can_place: true,
        land_building: true,
    };
    let mut f = Frame::new(vec![]);
    let s = f.scene();
    let mut x = LogRaster::default();
    draw_placement(&mut x, &s, &g);
    assert!(x.calls.iter().any(|c| c.starts_with("lines ") && c.ends_with(" 0.5 3.5")));
    assert_eq!(x.calls.last().unwrap(), "zcompare 2");
    g.land_building = false;
    g.unit.scale = 0.0;
    let mut x = LogRaster::default();
    draw_placement(&mut x, &s, &g);
    assert!(x.calls.is_empty());
}
