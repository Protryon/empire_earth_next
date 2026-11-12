//! The effects' quads (`src/Empire Earth/Graphics Thread/EEEffect.cpp`): the four vertices `AddGlowSprite`,
//! `AddGroundSprite`, `AddGroundQuad`, `AddBlendedSprite` and `AddGroundStrip` add to the two quad batches (their
//! colour, points, normal, texture coordinates and opacities), and `GetEffectFogFactor`'s fade. The C++ keeps the
//! batches: their counts, the flush when full, the alpha reference and every `SetVertex`, in the original's order.
//!
//! This is the renderer's, not the simulation's: nothing here is simulation state.

use ee_ge::transform::RotationOrder;
use ee_ge::types::{Ge3DPoint, GeTransformation, GeVector};

/// `GEColor`'s storage: red, green, blue.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxColor {
    pub red: f32,
    pub green: f32,
    pub blue: f32,
}

impl EeGfxColor {
    fn scaled(self, s: f32) -> EeGfxColor {
        EeGfxColor { red: self.red * s, green: self.green * s, blue: self.blue * s }
    }
}

/// `GETextureCoordinate`'s storage.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxTexCoord {
    pub u: f32,
    pub v: f32,
}

/// One quad of an effect batch: its colour and normal (the same at every corner) and its four corners in the order
/// they are set (the batch's indices `count` .. `count + 3`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxEffectQuad {
    pub color: EeGfxColor,
    pub normal: GeVector,
    pub points: [Ge3DPoint; 4],
    pub coords: [EeGfxTexCoord; 4],
    pub opacities: [f32; 4],
}

/// The rasterizer's fog for `GetEffectFogFactor`: its state (1 linear) and its range.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxEffectFog {
    pub state: u32,
    pub start: f32,
    pub end: f32,
    pub scale: f32,
}

/// `GetEffectFogFactor`: 1 unless the fog is linear, else fading from 1 at the fog's start to 0 at its end by the
/// distance along the camera (row 1 of the camera's inverse).
pub fn fog_factor(point: Ge3DPoint, fog: &EeGfxEffectFog, camera_inverse: &GeTransformation) -> f32 {
    if fog.state != 1 {
        return 1.0;
    }
    let m = &camera_inverse.m;
    let distance = point.y * m[1][1] + point.z * m[1][2] + point.x * m[1][0] + m[1][3];
    if distance <= fog.start {
        return 1.0;
    }
    if distance <= fog.end {
        let factor = 1.0 - (distance - fog.start) * fog.scale;
        if factor >= 0.0 {
            return factor;
        }
    }
    0.0
}

/// The quads' transformation: the rotation, the position and an identity bottom row.
fn quad_transformation(yaw: f32, pitch: f32, roll: f32, position: Ge3DPoint) -> GeTransformation {
    let mut t = GeTransformation::IDENTITY;
    t.set_orientation(RotationOrder::Ypr, yaw, pitch, roll);
    t.m[0][3] = position.x;
    t.m[1][3] = position.y;
    t.m[2][3] = position.z;
    t.m[3] = [0.0, 0.0, 0.0, 1.0];
    t
}

/// The unit square's texture corners in the quads' order.
const SQUARE: [EeGfxTexCoord; 4] = [
    EeGfxTexCoord { u: 0.0, v: 0.0 },
    EeGfxTexCoord { u: 1.0, v: 0.0 },
    EeGfxTexCoord { u: 1.0, v: 1.0 },
    EeGfxTexCoord { u: 0.0, v: 1.0 },
];

/// A camera-facing quad (turned by the camera's yaw and pitch, rolled by `rotation`): corners in its x-z plane.
fn camera_quad(position: Ge3DPoint, rotation: f32, size: f32, yaw: f32, pitch: f32) -> ([Ge3DPoint; 4], GeVector) {
    let half = size * 0.5;
    let t = quad_transformation(yaw, pitch, rotation, position);
    let normal = t.apply_vector(GeVector { x: 0.0, y: -1.0, z: 0.0 });
    let neg = -half;
    let corners = [(neg, half), (half, half), (half, neg), (neg, neg)];
    (corners.map(|(x, z)| t.apply_point(Ge3DPoint { x, y: 0.0, z })), normal)
}

/// A flat ground quad turned by `yaw`: corners in its x-y plane.
fn ground_quad(position: Ge3DPoint, yaw: f32, size: f32) -> ([Ge3DPoint; 4], GeVector) {
    let half = size * 0.5;
    let t = quad_transformation(yaw, 0.0, 0.0, position);
    let normal = t.apply_vector(GeVector { x: 0.0, y: 0.0, z: 1.0 });
    let neg = -half;
    let corners = [(neg, half), (half, half), (half, neg), (neg, neg)];
    (corners.map(|(x, y)| t.apply_point(Ge3DPoint { x, y, z: 0.0 })), normal)
}

/// `AddGlowSprite`'s quad: the colour dimmed by the fog factor, the alpha and the brightness; opaque corners.
#[allow(clippy::too_many_arguments)]
pub fn glow_sprite(
    position: Ge3DPoint,
    color: EeGfxColor,
    rotation: f32,
    size: f32,
    alpha: f32,
    brightness: f32,
    fog: f32,
    camera_yaw: f32,
    camera_pitch: f32,
) -> EeGfxEffectQuad {
    let mut dim = fog;
    dim *= alpha * brightness;
    let (points, normal) = camera_quad(position, rotation, size, camera_yaw, camera_pitch);
    EeGfxEffectQuad { color: color.scaled(dim), normal, points, coords: SQUARE, opacities: [1.0; 4] }
}

/// `AddGroundSprite`'s (one opacity) and `AddGroundQuad`'s (one per corner) quad: the colour by the brightness.
pub fn ground_sprite(
    position: Ge3DPoint,
    color: EeGfxColor,
    rotation: f32,
    size: f32,
    opacities: [f32; 4],
    brightness: f32,
) -> EeGfxEffectQuad {
    let (points, normal) = ground_quad(position, rotation, size);
    EeGfxEffectQuad { color: color.scaled(brightness), normal, points, coords: SQUARE, opacities }
}

/// `AddBlendedSprite`'s quad: camera-facing, the colour by the brightness, the alpha at every corner.
#[allow(clippy::too_many_arguments)]
pub fn blended_sprite(
    position: Ge3DPoint,
    color: EeGfxColor,
    rotation: f32,
    size: f32,
    alpha: f32,
    brightness: f32,
    camera_yaw: f32,
    camera_pitch: f32,
) -> EeGfxEffectQuad {
    let (points, normal) = camera_quad(position, rotation, size, camera_yaw, camera_pitch);
    EeGfxEffectQuad { color: color.scaled(brightness), normal, points, coords: SQUARE, opacities: [alpha; 4] }
}

/// `AddGroundStrip`'s quad: upright from `from` to `to`, `height` tall above the line between them, textured from
/// `origin` over `extent`; the colour by the brightness, the alpha at every corner.
#[allow(clippy::too_many_arguments)]
pub fn ground_strip(
    from: Ge3DPoint,
    to: Ge3DPoint,
    height: f32,
    color: EeGfxColor,
    origin: EeGfxTexCoord,
    extent: EeGfxTexCoord,
    alpha: f32,
    brightness: f32,
) -> EeGfxEffectQuad {
    let dx = to.x - from.x;
    let dy = to.y - from.y;
    let half_dx = dx * 0.5;
    let half_dy = dy * 0.5;
    let center = Ge3DPoint { x: from.x + half_dx, y: from.y + half_dy, z: from.z };
    let t = quad_transformation(0.0, 0.0, 0.0, center);
    let normal = t.apply_vector(GeVector { x: 0.0, y: -1.0, z: 0.0 });
    let back_x = dx * -0.5;
    let back_y = dy * -0.5;
    let rise = to.z - from.z;
    let points = [
        t.apply_point(Ge3DPoint { x: half_dx, y: half_dy, z: rise + height }),
        t.apply_point(Ge3DPoint { x: back_x, y: back_y, z: height }),
        t.apply_point(Ge3DPoint { x: back_x, y: back_y, z: 0.0 }),
        t.apply_point(Ge3DPoint { x: half_dx, y: half_dy, z: rise }),
    ];
    let (u1, v1) = (origin.u + extent.u, origin.v + extent.v);
    let coords = [
        EeGfxTexCoord { u: origin.u, v: origin.v },
        EeGfxTexCoord { u: u1, v: origin.v },
        EeGfxTexCoord { u: u1, v: v1 },
        EeGfxTexCoord { u: origin.u, v: v1 },
    ];
    EeGfxEffectQuad { color: color.scaled(brightness), normal, points, coords, opacities: [alpha; 4] }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_fog_fades_from_its_start_to_its_end() {
        let mut inverse = GeTransformation::IDENTITY;
        inverse.m[1][3] = 0.0;
        let fog = EeGfxEffectFog { state: 1, start: 10.0, end: 20.0, scale: 0.1 };
        let at = |y: f32| fog_factor(Ge3DPoint { x: 0.0, y, z: 0.0 }, &fog, &inverse);
        assert_eq!(at(5.0), 1.0);
        assert_eq!(at(15.0), 0.5);
        assert_eq!(at(25.0), 0.0);
        let off = EeGfxEffectFog { state: 0, ..fog };
        assert_eq!(fog_factor(Ge3DPoint { x: 0.0, y: 25.0, z: 0.0 }, &off, &inverse), 1.0);
    }

    #[test]
    fn a_ground_sprite_lies_flat_round_its_centre() {
        let white = EeGfxColor { red: 1.0, green: 1.0, blue: 1.0 };
        let q = ground_sprite(Ge3DPoint { x: 5.0, y: 5.0, z: 2.0 }, white, 0.0, 2.0, [0.5; 4], 0.5);
        assert_eq!(q.points[0], Ge3DPoint { x: 4.0, y: 6.0, z: 2.0 });
        assert_eq!(q.points[2], Ge3DPoint { x: 6.0, y: 4.0, z: 2.0 });
        assert_eq!(q.normal, GeVector { x: 0.0, y: 0.0, z: 1.0 });
        assert_eq!(q.color.red, 0.5);
        assert_eq!(q.coords, SQUARE);
    }

    #[test]
    fn a_strip_stands_between_its_ends() {
        let white = EeGfxColor { red: 1.0, green: 1.0, blue: 1.0 };
        let q = ground_strip(
            Ge3DPoint { x: 0.0, y: 0.0, z: 1.0 },
            Ge3DPoint { x: 4.0, y: 0.0, z: 3.0 },
            2.0,
            white,
            EeGfxTexCoord { u: 0.0, v: 0.0 },
            EeGfxTexCoord { u: 1.0, v: 1.0 },
            1.0,
            1.0,
        );
        assert_eq!(q.points[0], Ge3DPoint { x: 4.0, y: 0.0, z: 5.0 });
        assert_eq!(q.points[1], Ge3DPoint { x: 0.0, y: 0.0, z: 3.0 });
        assert_eq!(q.points[2], Ge3DPoint { x: 0.0, y: 0.0, z: 1.0 });
        assert_eq!(q.points[3], Ge3DPoint { x: 4.0, y: 0.0, z: 3.0 });
    }
}
