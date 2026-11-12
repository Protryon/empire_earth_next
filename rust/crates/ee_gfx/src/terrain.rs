//! The terrain's render object's sky and clouds (`src/Empire Earth/Graphics Thread/EEROTerrain.cpp`): the time of
//! day's blend of the four sky bands (`UpdateSkyColors`: 17 steps, the fog colour the lowest band), the sky dome's
//! and the star models' points stretched over the map's square, the cloud layer's points and its scrolled texture
//! coordinates (`UpdateCloudLayer`), and the scenario editor's grid mode step (`StepGridMode`). The C++ keeps the
//! models, every vertex-array call and the draws.
//!
//! This is the renderer's, not the simulation's: nothing here is simulation state.

use ee_ge::types::{Ge2DPoint, Ge3DPoint};
use ee_ge::um::{floor, floor_unsigned};

use crate::effects::{EeGfxColor, EeGfxTexCoord};

/// The band of each group of four sky vertices (0x441a04).
const SKY_BANDS: [usize; 5] = [0, 0, 1, 2, 3];
/// The sky vertices (0x441a18): a corner of the map square and a height.
const SKY_VERTICES: [(usize, f32); 20] = [
    (0, -4500.0),
    (1, -4500.0),
    (2, -4500.0),
    (3, -4500.0),
    (0, 0.0),
    (1, 0.0),
    (2, 0.0),
    (3, 0.0),
    (0, 150.0),
    (1, 150.0),
    (2, 150.0),
    (3, 150.0),
    (0, 1000.0),
    (1, 1000.0),
    (2, 1000.0),
    (3, 1000.0),
    (0, 10000.0),
    (1, 10000.0),
    (2, 10000.0),
    (3, 10000.0),
];
/// The star vertices (0x441ab8).
const STAR_VERTICES: [(usize, f32); 24] = [
    (3, 5000.0),
    (2, 5000.0),
    (1, 5000.0),
    (0, 5000.0),
    (3, 5000.0),
    (0, 5000.0),
    (0, -4500.0),
    (3, -4500.0),
    (1, 5000.0),
    (2, 5000.0),
    (2, -4500.0),
    (1, -4500.0),
    (0, 5000.0),
    (1, 5000.0),
    (1, -4500.0),
    (0, -4500.0),
    (2, 5000.0),
    (3, 5000.0),
    (3, -4500.0),
    (2, -4500.0),
    (0, -4500.0),
    (1, -4500.0),
    (2, -4500.0),
    (3, -4500.0),
];

/// One frame's sky: the fog colour (the lowest band's), the sky dome's vertex colours and points, and the outer and
/// inner star models' points (the inner a quarter of the outer).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeGfxSkyFrame {
    pub fog: EeGfxColor,
    pub sky_colors: [EeGfxColor; 20],
    pub sky_points: [Ge3DPoint; 20],
    pub outer_stars: [Ge3DPoint; 24],
    pub inner_stars: [Ge3DPoint; 24],
}

/// The map's square in left, top, right, bottom order, its edges pushed out 10,000 units (`UpdateSkyColors`).
fn sky_corners(size: i32, width: i32) -> [Ge2DPoint; 4] {
    let mut c = [Ge2DPoint::default(); 4];
    c[0].x = 0.0;
    c[0].y = size as f32 - width as f32 - 1.0;
    c[1].x = width as f32 + 1.0;
    c[1].y = size as f32;
    c[2].x = size as f32;
    c[2].y = width as f32 + 1.0;
    c[3].x = size as f32 - width as f32 - 1.0;
    c[3].y = 0.0;
    c[0].y = c[0].y + 1.0;
    c[1].x = c[1].x - 1.0;
    c[2].y = c[2].y - 1.0;
    c[3].x = c[3].x + 1.0;
    c[0].x = -10000.0;
    c[1].y = c[1].y + 10000.0;
    c[2].x = c[2].x + 10000.0;
    c[3].y = -10000.0;
    c
}

/// `UpdateSkyColors`: the four bands' colours at `time_of_day` (0..1 in 16 steps, each blended with the next) from
/// the 4 x 17 table `colors` (`gSkyColors`), and the models' points over a map of `size` tiles a side and width
/// `width` (`mL195614`). A time past 1 read past a band in the original (and past the table from 17/16 on): the
/// table index is held at its last entry instead.
pub fn sky_frame(time_of_day: f32, colors: &[EeGfxColor; 68], size: i32, width: i32) -> EeGfxSkyFrame {
    let time = time_of_day * 16.0;
    let fraction = time - floor(time) as f32;
    let index = floor_unsigned(time) as usize;
    let mut next = index.wrapping_add(1);
    if next > 0x10 {
        next = 0x10;
    }
    let complement = 1.0 - fraction;
    let entry = |band: usize, step: usize| colors[band.wrapping_mul(17).wrapping_add(step).min(67)];
    let mut bands = [EeGfxColor::default(); 4];
    for (i, c) in bands.iter_mut().enumerate() {
        let a = entry(i, index);
        let b = entry(i, next);
        c.red = a.red * complement + b.red * fraction;
        c.green = a.green * complement + b.green * fraction;
        c.blue = a.blue * complement + b.blue * fraction;
    }
    let corners = sky_corners(size, width);
    let mut frame = EeGfxSkyFrame {
        fog: bands[0],
        sky_colors: [EeGfxColor::default(); 20],
        sky_points: [Ge3DPoint::default(); 20],
        outer_stars: [Ge3DPoint::default(); 24],
        inner_stars: [Ge3DPoint::default(); 24],
    };
    for i in 0..20 {
        frame.sky_colors[i] = bands[SKY_BANDS[i >> 2]];
        let (corner, z) = SKY_VERTICES[i];
        frame.sky_points[i] = Ge3DPoint { x: corners[corner].x, y: corners[corner].y, z };
    }
    for i in 0..24 {
        let (corner, z) = STAR_VERTICES[i];
        let c = corners[corner];
        frame.outer_stars[i] = Ge3DPoint { x: c.x, y: c.y, z };
        frame.inner_stars[i] = Ge3DPoint { x: c.x * 0.25, y: c.y * 0.25, z: z * 0.25 };
    }
    frame
}

/// The cloud layer's height above the map's edge (0x441408), its overhang (0x44140c), its height (0x441410) and its
/// texture's repeat (0x441414).
const CLOUD_HEIGHT: f32 = 1500.0;
const CLOUD_OVERHANG: f32 = 3000.0;
const CLOUD_Z: f32 = 200.0;
const CLOUD_REPEAT: f32 = 4000.0;

/// One frame's cloud layer: its eight points (the outer square's four, then the inner's), their texture
/// coordinates, and the cloud offset (`wrapped`: it was 1 or more and is now its fraction, to be stored back).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeGfxCloudLayer {
    pub points: [Ge3DPoint; 8],
    pub coords: [EeGfxTexCoord; 8],
    pub offset: f32,
    pub wrapped: bool,
}

/// `UpdateCloudLayer`: the layer over a map of `size` tiles a side and width `width`, its texture scrolled by
/// `offset` (the environment's cloud offset).
pub fn cloud_layer(size: i32, width: i32, offset: f32) -> EeGfxCloudLayer {
    let size = size as f32;
    let low = size - width as f32 - 1.0;
    let high = width as f32 + 1.0;
    let overhang = CLOUD_HEIGHT + CLOUD_OVERHANG;
    let p = |x: f32, y: f32| Ge3DPoint { x, y, z: CLOUD_Z };
    let points = [
        p(-overhang, low + 1.0),
        p(high - 1.0, size + overhang),
        p(low + 1.0, -overhang),
        p(size + overhang, high - 1.0),
        p(-CLOUD_OVERHANG, low + 1.0),
        p(high - 1.0, size + CLOUD_OVERHANG),
        p(low + 1.0, -CLOUD_OVERHANG),
        p(size + CLOUD_OVERHANG, high - 1.0),
    ];
    let mut width = points[0].distance_to(points[1]);
    let mut height = points[0].distance_to(points[2]);
    let mut inner_u = (1.0 - points[4].distance_to(points[5]) / width) * 0.5;
    let mut inner_v = (1.0 - points[4].distance_to(points[6]) / height) * 0.5;
    width = width / CLOUD_REPEAT;
    height = height / CLOUD_REPEAT;
    inner_u = inner_u * width;
    inner_v = inner_v * height;
    let mut offset = offset;
    let wrapped = offset >= 1.0;
    if wrapped {
        offset = offset - floor(offset) as f32;
    }
    let t = |u: f32, v: f32| EeGfxTexCoord { u, v };
    let coords = [
        t(offset, offset),
        t(width + offset, offset),
        t(offset, height + offset),
        t(width + offset, height + offset),
        t(inner_u + offset, inner_v + offset),
        t(width - inner_u + offset, inner_v + offset),
        t(inner_u + offset, height - inner_v + offset),
        t(width - inner_u + offset, height - inner_v + offset),
    ];
    EeGfxCloudLayer { points, coords, offset, wrapped }
}

/// `StepGridMode`: the editor's terrain grid mode, forwards or backwards through 0..17, wrapping round.
pub fn grid_mode_step(mode: i32, backwards: bool) -> i32 {
    if backwards {
        if mode == 0 {
            0x11
        } else {
            mode.wrapping_sub(1)
        }
    } else if mode == 0x11 {
        0
    } else {
        mode.wrapping_add(1)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn table() -> [EeGfxColor; 68] {
        let mut t = [EeGfxColor::default(); 68];
        for (i, c) in t.iter_mut().enumerate() {
            *c = EeGfxColor { red: i as f32, green: 0.5, blue: -(i as f32) };
        }
        t
    }

    #[test]
    fn the_sky_blends_two_steps() {
        let t = table();
        let f = sky_frame(0.5 / 16.0 + 3.0 / 16.0, &t, 100, 50);
        // band 0 between entries 3 and 4, band 3 between 54 and 55
        assert_eq!(f.fog.red, 3.5);
        assert_eq!(f.sky_colors[19].red, 54.5);
        assert_eq!(f.sky_colors[0], f.fog);
        // the last step holds its colour
        let f = sky_frame(1.0, &t, 100, 50);
        assert_eq!(f.fog.red, 16.0);
        // past the table: held at its end, no panic
        let f = sky_frame(9.0, &t, 100, 50);
        assert_eq!(f.sky_colors[19].red, 67.0);
    }

    #[test]
    fn the_sky_spans_the_map() {
        let f = sky_frame(0.25, &table(), 100, 50);
        assert_eq!(f.sky_points[0], Ge3DPoint { x: -10000.0, y: 50.0, z: -4500.0 });
        assert_eq!(f.sky_points[17], Ge3DPoint { x: 50.0, y: 10100.0, z: 10000.0 });
        assert_eq!(f.inner_stars[0].x, f.outer_stars[0].x * 0.25);
    }

    #[test]
    fn the_clouds_wrap_their_offset() {
        let c = cloud_layer(100, 50, 2.25);
        assert!(c.wrapped);
        assert_eq!(c.offset, 0.25);
        assert_eq!(c.coords[0], EeGfxTexCoord { u: 0.25, v: 0.25 });
        let c = cloud_layer(100, 50, 0.5);
        assert!(!c.wrapped && c.offset == 0.5);
        assert!(c.coords[4].u > c.coords[0].u && c.coords[5].u < c.coords[1].u);
    }

    #[test]
    fn the_grid_mode_wraps() {
        assert_eq!(grid_mode_step(0, true), 17);
        assert_eq!(grid_mode_step(17, false), 0);
        assert_eq!(grid_mode_step(5, false), 6);
        assert_eq!(grid_mode_step(5, true), 4);
    }
}
