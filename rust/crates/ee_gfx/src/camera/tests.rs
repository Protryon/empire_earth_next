use super::*;

const NO_WORLD: GeTransformation = GeTransformation::IDENTITY;

struct Flat(f32);
impl Ground for Flat {
    fn valid_tile(&mut self, x: f32, y: f32) -> bool {
        (0.0..10.0).contains(&x) && (0.0..10.0).contains(&y)
    }
    fn valid_point(&mut self, x: f32, y: f32) -> bool {
        (0.0..11.0).contains(&x) && (0.0..11.0).contains(&y)
    }
    fn mesh_z(&mut self, _: f32, _: f32) -> f32 {
        self.0
    }
}

#[test]
fn pitch_runs_from_near_to_far_and_holds_past_zoom_one() {
    assert_eq!(pitch(0.0, 0, &NO_WORLD), PITCH_NEAR);
    assert_eq!(pitch(1.0, 0, &NO_WORLD), PITCH_FAR);
    assert_eq!(pitch(2.0, 0, &NO_WORLD), pitch(1.0, 0, &NO_WORLD));
    assert_eq!(pitch(2.0, 1, &NO_WORLD), pitch(1.0, 1, &NO_WORLD));
    // the free camera's pitch is odd in the zoom
    assert_eq!(pitch(-0.5, 3, &NO_WORLD), -pitch(0.5, 3, &NO_WORLD));
    // the world camera's: the identity looks along +y (level)
    assert!(pitch(0.3, 2, &NO_WORLD).abs() < 1e-3);
    assert_eq!(pitch(0.3, 7, &NO_WORLD), 0.0);
}

#[test]
fn go_to_takes_the_slower_move_and_snaps_short_ones() {
    let a = Ge3DPoint::new(0.0, 0.0, 0.0);
    let b = Ge3DPoint::new(30.0, 0.0, 0.0);
    let g = go_to_speed(a, a, a, b, 1.0, false, 0.0);
    assert!(g.snap_offset && !g.snap_focus);
    assert_eq!(g.speed, 0.5); // 15 a second over 30
    let near = Ge3DPoint::new(3.0, 0.0, 0.0);
    // 15 / 3 = 5 a second: at most 2.5 moves a second
    assert_eq!(go_to_speed(a, a, a, near, 1.0, false, 0.0).speed, MINIMUM_TIME);
    let g = go_to_speed(a, b, a, a, 1.0, false, 0.0);
    assert!(!g.snap_offset && g.snap_focus);
    // following: the unit's speed (at least 15) over the distance, no floor
    let g = go_to_speed(a, a, a, b, 1.0, true, 60.0);
    assert_eq!(g.speed, 2.0);
}

#[test]
fn clamp_height_lifts_over_the_ground() {
    let mut t = GeTransformation::IDENTITY;
    t.m[0][3] = 5.0;
    t.m[1][3] = 5.0;
    t.m[2][3] = 0.0;
    clamp_height(&mut t, 0, 0.1, true, &mut Flat(2.0));
    assert_eq!(t.m[2][3], 2.0 + MINIMUM_HEIGHT);
    // off the map: at least the minimum
    t.m[0][3] = -5.0;
    t.m[2][3] = -3.0;
    clamp_height(&mut t, 3, 0.1, false, &mut Flat(2.0));
    assert_eq!(t.m[2][3], MINIMUM_HEIGHT_FREE);
}

#[test]
fn clip_and_clamp_keep_points_on_the_map() {
    let plane = |x: f32, y: f32, nx: f32, ny: f32| Ge3DPlane {
        point: Ge3DPoint::new(x, y, 0.0),
        normal: GeVector::new(nx, ny, 0.0),
    };
    let planes = EeGfxEdgePlanes {
        front: plane(0.0, 0.0, 0.0, 1.0),
        back: plane(0.0, 10.0, 0.0, -1.0),
        left: plane(0.0, 0.0, 1.0, 0.0),
        right: plane(10.0, 0.0, -1.0, 0.0),
    };
    let line = Ge3DLine { start: Ge3DPoint::new(5.0, 5.0, 1.0), direction: GeVector::new(10.0, 0.0, 0.0) };
    let p = clip_to_edges(&line, &planes, line.point_at(1.0));
    assert_eq!((p.x, p.y), (10.0 - 0.0001, 5.0 - 0.0001));
    let p = clamp_to_map(Ge3DPoint::new(-3.0, 12.0, 1.0), &planes);
    assert_eq!((p.x, p.y), (0.0, 10.0));
    let g = ground_point(&line, &planes, Ge3DPoint::new(0.0, -5.0, 0.0), 0, &mut Flat(1.0));
    assert_eq!(g.z, 1.0 + MINIMUM_HEIGHT);
    let g = ground_point(&line, &planes, Ge3DPoint::new(0.0, -50.0, 0.0), 0, &mut Flat(1.0));
    assert_eq!(g.z, MINIMUM_HEIGHT);
}

#[test]
fn step_waits_jumps_and_advances() {
    let mut s = EeGfxCameraStep { fraction: 1.0, rate: 1.0, wait: 1.0, waited: 0.5, changed: false, locked: true };
    assert_eq!(step(&mut s, 0.25), CAMERA_STEP_DONE);
    assert_eq!(s.waited, 0.75);
    s.waited = 1.0;
    assert_eq!(step(&mut s, 0.25), CAMERA_STEP_NEXT_KEY);
    s.locked = false;
    assert_eq!(step(&mut s, 0.25), CAMERA_STEP_DONE);
    s.changed = true;
    assert_eq!(step(&mut s, 0.25), CAMERA_STEP_VIEW);
    assert!(!s.changed);
    s.fraction = 0.5;
    s.rate = 4.0;
    assert_eq!(step(&mut s, 0.25), CAMERA_STEP_VIEW);
    assert_eq!(s.fraction, 1.0);
}

#[test]
fn ortho_view_sizes_with_the_offset() {
    let near = ortho_view(Ge3DPoint::default(), -5.0);
    let far = ortho_view(Ge3DPoint::default(), -48.0);
    assert!(near.size < far.size);
    let o = ortho_view(Ge3DPoint::new(3.0, 4.0, 0.0), -20.0);
    assert!(o.view.m[0][3] != 0.0 && o.view.m[2][3] != 0.0);
}

#[test]
fn scroll_direction_is_as_long_as_the_delta() {
    let d = scroll_direction(&GeTransformation::IDENTITY, 3.0, 4.0, 1.0);
    assert!((d.x * d.x + d.y * d.y).sqrt() - 5.0 < 1e-5);
    assert_eq!(d.z, 0.0);
    assert!(scroll_is_near(Ge3DPoint::new(0.0, -10.0, 0.0)));
    assert!(!scroll_is_near(Ge3DPoint::new(0.0, -10.1, 0.0)));
}
