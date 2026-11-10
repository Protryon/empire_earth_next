//! `GEBitmap`'s and `GERhombus`'s geometry (`GEBitmap.cpp`, `GERhombus.cpp`): the four corner positions of a bitmap
//! from its location, frame size, scale and rotation about the centre, a rhombus's from its rectangle and rotation,
//! and a bitmap's texture coordinates and scale when a source rectangle of the screen's texture is stretched onto a
//! destination rectangle. The classes, their vertex arrays (behind `GEVertexArray`'s virtuals) and their textures
//! stay C++; `ComputeRotation` and `ComputeStretchedCoordsAndScale` forward here. The UI draws with them (not the
//! simulation); the C++'s `f32` order is kept, the scale through `double` as its `(double)` divisor was.

use std::ffi::c_ulong;

use crate::types::{Ge2DPoint, GeRectangle};

/// Corner order of a bitmap's and a rhombus's vertex array: top left, top right, bottom left, bottom right.
pub type Corners = [Ge2DPoint; 4];

/// The four corners turned about the centre of the unrotated quad (`ComputeRotation`'s shared tail): nothing
/// moves for a rotation of 0.
fn rotate_corners(mut c: Corners, rotation: f32) -> Corners {
    if rotation != 0.0 {
        let center = Ge2DPoint::new((c[1].x + c[0].x) * 0.5, (c[0].y + c[2].y) * 0.5);
        for p in c.iter_mut() {
            *p = p.rotated(rotation, center);
        }
    }
    c
}

/// `GEBitmap::ComputeRotation`: the corners from the location (half a pixel up and left), the frame size times the
/// scale, and the rotation.
pub fn bitmap_corners(
    location: Ge2DPoint,
    width: f32,
    height: f32,
    x_scale: f32,
    y_scale: f32,
    rotation: f32,
) -> Corners {
    let top_left = Ge2DPoint::new(location.x - 0.5, location.y - 0.5);
    let top_right = Ge2DPoint::new(x_scale * width + top_left.x, top_left.y);
    let bottom_left = Ge2DPoint::new(top_left.x, y_scale * height + top_left.y);
    let bottom_right = Ge2DPoint::new(top_right.x, bottom_left.y);
    rotate_corners([top_left, top_right, bottom_left, bottom_right], rotation)
}

/// `GERhombus::ComputeRotation`: the rectangle's corners and the rotation.
pub fn rhombus_corners(rectangle: &GeRectangle, rotation: f32) -> Corners {
    let r = rectangle;
    rotate_corners(
        [
            Ge2DPoint::new(r.left, r.top),
            Ge2DPoint::new(r.right, r.top),
            Ge2DPoint::new(r.left, r.bottom),
            Ge2DPoint::new(r.right, r.bottom),
        ],
        rotation,
    )
}

/// `ComputeStretchedCoordsAndScale`'s result: the texture coordinates per corner (top left, top right, bottom left,
/// bottom right) and the x and y scale.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Stretch {
    pub uv: [[f32; 2]; 4],
    pub x_scale: f32,
    pub y_scale: f32,
}

/// `GEBitmap::ComputeStretchedCoordsAndScale`: a source rectangle of a texture of `width` x `height` pixels (at
/// `drop` MIP levels dropped) mapped onto texture coordinates through the destination rectangle, and the scale that
/// draws the source's size (the ranges are inclusive, so one pixel wide).
pub fn stretched_coords(
    texture: &GeRectangle,
    source: &GeRectangle,
    destination: &GeRectangle,
    width: c_ulong,
    height: c_ulong,
    drop: c_ulong,
) -> Stretch {
    let texture_width = (width << drop) as f32;
    let texture_height = (height << drop) as f32;
    let texture_x_range = texture.right - texture.left + 1.0;
    let destination_x_range = destination.right - destination.left;
    let top_left_u =
        ((source.left - texture.left) / texture_x_range * destination_x_range + destination.left) / texture_width;
    let texture_y_range = texture.bottom - texture.top + 1.0;
    let destination_y_range = destination.bottom - destination.top;
    let top_left_v =
        ((source.top - texture.top) / texture_y_range * destination_y_range + destination.top) / texture_height;
    let bottom_right_u = ((source.right + 1.0 - texture.left) / texture_x_range * destination_x_range
        + destination.left)
        / texture_width;
    let bottom_right_v = ((source.bottom + 1.0 - texture.top) / texture_y_range * destination_y_range
        + destination.top)
        / texture_height;
    Stretch {
        uv: [
            [top_left_u, top_left_v],
            [bottom_right_u, top_left_v],
            [top_left_u, bottom_right_v],
            [bottom_right_u, bottom_right_v],
        ],
        x_scale: (f64::from(source.right - source.left + 1.0) / width as f64) as f32,
        y_scale: (f64::from(source.bottom - source.top + 1.0) / height as f64) as f32,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn rect(l: f32, t: f32, r: f32, b: f32) -> GeRectangle {
        GeRectangle { bottom: b, left: l, right: r, top: t }
    }

    #[test]
    fn an_unrotated_bitmap_spans_its_scaled_frame_from_half_a_pixel_up_and_left() {
        let c = bitmap_corners(Ge2DPoint::new(10.0, 20.0), 32.0, 16.0, 2.0, 0.5, 0.0);
        assert_eq!(c[0], Ge2DPoint::new(9.5, 19.5));
        assert_eq!(c[1], Ge2DPoint::new(73.5, 19.5));
        assert_eq!(c[2], Ge2DPoint::new(9.5, 27.5));
        assert_eq!(c[3], Ge2DPoint::new(73.5, 27.5));
    }

    #[test]
    fn a_half_turn_swaps_opposite_corners_about_the_centre() {
        let c = bitmap_corners(Ge2DPoint::new(0.5, 0.5), 10.0, 4.0, 1.0, 1.0, crate::um::PI);
        // top left lands near the old bottom right (the table trigonometry is approximate)
        assert!((c[0].x - 10.0).abs() < 0.01 && (c[0].y - 4.0).abs() < 0.01, "{:?}", c[0]);
        assert!((c[3].x).abs() < 0.01 && (c[3].y).abs() < 0.01, "{:?}", c[3]);
    }

    #[test]
    fn a_rhombus_takes_its_rectangles_corners() {
        let c = rhombus_corners(&rect(1.0, 2.0, 5.0, 8.0), 0.0);
        assert_eq!(
            c,
            [Ge2DPoint::new(1.0, 2.0), Ge2DPoint::new(5.0, 2.0), Ge2DPoint::new(1.0, 8.0), Ge2DPoint::new(5.0, 8.0)]
        );
    }

    #[test]
    fn the_whole_texture_onto_itself_gives_unit_coordinates_and_scale() {
        let s = stretched_coords(
            &rect(0.0, 0.0, 63.0, 31.0),
            &rect(0.0, 0.0, 63.0, 31.0),
            &rect(0.0, 0.0, 64.0, 32.0),
            64,
            32,
            0,
        );
        assert_eq!(s.uv, [[0.0, 0.0], [1.0, 0.0], [0.0, 1.0], [1.0, 1.0]]);
        assert_eq!((s.x_scale, s.y_scale), (1.0, 1.0));
        // a dropped MIP level halves the coordinates, the right half of the source halves the scale
        let s = stretched_coords(
            &rect(0.0, 0.0, 63.0, 31.0),
            &rect(32.0, 0.0, 63.0, 31.0),
            &rect(0.0, 0.0, 64.0, 32.0),
            32,
            16,
            1,
        );
        assert_eq!(s.uv[0], [0.5, 0.0]);
        assert_eq!(s.uv[3], [1.0, 1.0]);
        assert_eq!(s.x_scale, 1.0);
        assert_eq!(s.y_scale, 2.0);
    }
}
