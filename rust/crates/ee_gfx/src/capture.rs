//! The capture's per-object arithmetic (`EERenderObject.cpp`): a map object's place inside its tiles and its size
//! jitter by unit id (`FillObject`, `FillDoppelgangerObject`), the sway clock (`FillRenderLists`) and a swaying
//! object's blend fraction, and the area ring's points on the ground (`DrawGroundCircle`). The C++ keeps the render
//! records, the world and the terrain (asked through `GroundHeights`).
//!
//! Drawn only (the renderer's thread). The C++'s `long` is 32 bits on Windows: the id arithmetic wraps as `i32`/`u32`.

use ee_ge::types::Ge3DPoint;
use ee_ge::um;

/// The sway clock's step (ms; `FillRenderLists` adds 2 when more than this has passed).
const SWAY_PERIOD: u32 = 0x21;
/// Degrees to radians (0x441344).
const DEGREES: f32 = 0.017453292;
/// A map object's size jitter per id step (0x441354).
const SCALE_STEP: f32 = 0.025;
/// The ring's segments (0x44134c).
const RING_SEGMENTS: f32 = 50.0;
/// The ring's points: 50 and the first again.
pub(crate) const EE_GFX_RING_POINTS: usize = 0x33;

/// A map object's place: off its tiles' centre by an eighth of its size in x by `id % 3` and in y by `id / 3 % 3`.
pub fn object_place(id: i32, size: i32, tile_x: i32, tile_y: i32) -> (f32, f32) {
    let x = (id % 3 - 1).wrapping_mul(size) as f32 * 0.125 + size as f32 * 0.5 + tile_x as f32;
    let y = (id / 3 % 3 - 1).wrapping_mul(size) as f32 * 0.125 + size as f32 * 0.5 + tile_y as f32;
    (x, y)
}

/// A map object's size jitter: 1 to 1.25 by `id % 11` (the id as unsigned).
pub fn object_scale(id: i32) -> f32 {
    (id as u32 % 11) as f32 * SCALE_STEP + 1.0
}

/// The sway clock after a frame at `now` (ms): +2 once more than 33 ms passed since `last`, which then moves to `now`.
pub fn sway_tick(clock: &mut u32, last: &mut u32, now: u32) {
    if now.wrapping_sub(*last) > SWAY_PERIOD {
        *clock = clock.wrapping_add(2);
        *last = now;
    }
}

/// A swaying map object's blend between its frames, in [0, 1]: the sine of its phase, by its id, on the clock.
// the C++'s comparisons (a NaN passes through)
#[allow(clippy::manual_clamp)]
pub fn sway_fraction(clock: u32, id: i32) -> f32 {
    let angle = (clock.wrapping_add((id as u32).wrapping_mul(8)) % 360) as i32 - 180;
    let fraction = (um::um_sin(angle as f32 * DEGREES) + 1.0) * 0.5;
    if fraction < 0.0 {
        0.0
    } else if fraction > 1.0 {
        1.0
    } else {
        fraction
    }
}

/// The terrain as the ring asks it.
pub trait GroundHeights {
    fn is_valid_tile(&mut self, x: u32, y: u32) -> bool;
    fn mesh_z(&mut self, x: f32, y: f32) -> f32;
}

/// `DrawGroundCircle`'s points around (`x`, `y`): 50 on the ground (0 off the map and under the sea), the first again,
/// and the strip's width (the sine of a segment times the radius).
pub fn ground_circle<G: GroundHeights>(
    g: &mut G,
    x: f32,
    y: f32,
    radius: f32,
) -> ([Ge3DPoint; EE_GFX_RING_POINTS], f32) {
    let mut points = [Ge3DPoint::default(); EE_GFX_RING_POINTS];
    let width = um::um_sin(6.2831855 / RING_SEGMENTS) * radius;
    for (i, p) in points.iter_mut().enumerate().take(0x32) {
        let angle = i as u32 as f32 / RING_SEGMENTS * 6.2831855 - 3.1415927;
        p.x = um::um_cos(angle) * radius + x;
        p.y = um::um_sin(angle) * radius + y;
        p.z = if g.is_valid_tile(um::floor_unsigned(p.x), um::floor_unsigned(p.y)) {
            let z = g.mesh_z(p.x, p.y);
            if z < 0.0 {
                0.0
            } else {
                z
            }
        } else {
            0.0
        };
    }
    points[0x32] = points[0];
    (points, width)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn objects_sit_in_their_tiles_and_jitter_by_id() {
        assert_eq!(object_place(0, 2, 10, 20), (10.75, 20.75));
        assert_eq!(object_place(4, 2, 10, 20), (11.0, 21.0));
        assert_eq!(object_place(8, 4, 0, 0), (2.5, 2.5));
        // the C++'s truncating remainder for a negative id
        assert_eq!(object_place(-1, 4, 0, 0), (1.0, 1.5));
        assert_eq!(object_scale(0), 1.0);
        assert_eq!(object_scale(10), 10.0 * 0.025 + 1.0);
        assert_eq!(object_scale(-1), (u32::MAX % 11) as f32 * 0.025 + 1.0);
    }

    #[test]
    fn the_sway_clock_ticks_past_33_ms() {
        let (mut clock, mut last) = (0, 100);
        sway_tick(&mut clock, &mut last, 133);
        assert_eq!((clock, last), (0, 100));
        sway_tick(&mut clock, &mut last, 134);
        assert_eq!((clock, last), (2, 134));
        // a clock that went back wraps past the period
        sway_tick(&mut clock, &mut last, 10);
        assert_eq!((clock, last), (4, 10));
        assert_eq!(sway_fraction(180, 0), 0.5);
        assert!(sway_fraction(270, 0) > 0.99);
        assert!(sway_fraction(90, 0) < 0.01);
        assert_eq!(sway_fraction(178, 1), sway_fraction(186, 0));
    }

    struct Flat;

    impl GroundHeights for Flat {
        fn is_valid_tile(&mut self, x: u32, _y: u32) -> bool {
            x < 100
        }
        fn mesh_z(&mut self, x: f32, _y: f32) -> f32 {
            x - 99.5
        }
    }

    #[test]
    fn the_ring_lies_on_the_ground_and_closes() {
        let (p, w) = ground_circle(&mut Flat, 100.0, 50.0, 2.0);
        assert_eq!(p[0], p[0x32]);
        assert_eq!(w, um::um_sin(6.2831855 / 50.0) * 2.0);
        // the first point is at -pi: left of the centre, on the map, above the sea
        assert!((p[0].x - 98.0).abs() < 1e-3 && p[0].z == 0.0);
        // a point right of the centre is off the map
        assert!(p[25].x > 101.9 && p[25].z == 0.0);
        let on = p.iter().filter(|q| q.x < 100.0 && q.x > 99.5).count();
        assert!(on > 0);
        assert!(p.iter().all(|q| q.z >= 0.0));
    }
}
