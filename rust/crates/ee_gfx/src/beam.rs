//! The tracer beam (`src/Empire Earth/Graphics Thread/EEE3DTracerBeam.cpp`): `Aim`'s beam between the shooter's
//! point and the target's (growing from the start until the effect's param 5, shrinking towards the end after
//! param 6): its length and the transformation that places the beam model at its middle, turned along it; and
//! `DrawPass6`'s stretch of the model to the beam's length and width with the texture scrolled. The C++ keeps the
//! tag points (the shooter's weapon point, its turret part, the target's picked point) and the model.
//!
//! `Aim` runs in the simulation tick (`Step`, before the scroll it feeds): its length and transformation are
//! simulation state (the length is saved). RUST_PLAN 2.1: the C++'s operations in its order.

use ee_ge::transform::RotationOrder;
use ee_ge::types::{Ge3DPoint, GeTransformation, GeVector};
use ee_ge::um::{um_arccos, um_arctan2};

use crate::effects::EeGfxTexCoord;

/// The beam model's stretch along its length (`DrawPass6`: 1.15 times the beam).
const LENGTH_STRETCH: f32 = 1.15;

/// `Aim`'s result: the beam's transformation (`SetTransformation`) and length (`mLength`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxBeamAim {
    pub transform: GeTransformation,
    pub length: f32,
}

/// One vertex of the beam model as `DrawPass6` stretches it: its point and texture coordinate.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxBeamVertex {
    pub point: Ge3DPoint,
    pub coord: EeGfxTexCoord,
}

fn lerp(from: Ge3DPoint, to: Ge3DPoint, f: f32) -> Ge3DPoint {
    Ge3DPoint { x: (to.x - from.x) * f + from.x, y: (to.y - from.y) * f + from.y, z: (to.z - from.z) * f + from.z }
}

/// `Aim` from `start` to `end` at `age`: `grow` and `shrink` are the effect's params 5 and 6.
pub fn aim(start: Ge3DPoint, end: Ge3DPoint, age: f32, grow: f32, shrink: f32) -> EeGfxBeamAim {
    let mut start = start;
    let mut end = end;
    if grow > 0.0 && age < grow {
        end = lerp(start, end, age / grow);
    }
    if shrink > 0.0 && shrink < age {
        start = lerp(start, end, (age - shrink) / (1.0 - shrink));
    }
    let mut d = GeVector { x: end.x - start.x, y: end.y - start.y, z: end.z - start.z };
    let length = d.magnitude();
    d = GeVector { x: 0.5 * d.x, y: 0.5 * d.y, z: 0.5 * d.z };
    start.x += d.x;
    start.y += d.y;
    start.z += d.z;
    d.normalize();
    let yaw = um_arctan2(-d.x, d.y);
    let pitch = um_arccos(-d.z) - 1.5707964;
    let mut t = GeTransformation::default();
    t.m[0][3] = start.x;
    t.m[1][3] = start.y;
    t.m[2][3] = start.z;
    t.set_orientation(RotationOrder::Ypr, yaw, pitch, 0.0);
    t.m[3] = [0.0, 0.0, 0.0, 1.0];
    EeGfxBeamAim { transform: t, length }
}

/// A y stretched to a beam of `length` (the model's vertices and its sub-models' internal transforms).
pub fn stretch_y(y: f32, length: f32) -> f32 {
    y * length * LENGTH_STRETCH
}

/// `DrawPass6`'s stretch of the model's vertices: y to the length, x to the width, u scrolled.
pub fn stretch(vertices: &mut [EeGfxBeamVertex], length: f32, width: f32, scroll: f32) {
    for v in vertices {
        v.point.y = stretch_y(v.point.y, length);
        v.point.x *= width;
        v.coord.u += scroll;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_level_beam_lies_along_its_middle() {
        let a = aim(Ge3DPoint { x: 0.0, y: 0.0, z: 1.0 }, Ge3DPoint { x: 0.0, y: 4.0, z: 1.0 }, 0.5, 0.0, 0.0);
        assert_eq!(a.length, 4.0);
        assert_eq!([a.transform.m[0][3], a.transform.m[1][3], a.transform.m[2][3]], [0.0, 2.0, 1.0]);
        assert_eq!(a.transform.m[3], [0.0, 0.0, 0.0, 1.0]);
    }

    #[test]
    fn the_beam_grows_then_shrinks() {
        let s = Ge3DPoint { x: 0.0, y: 0.0, z: 0.0 };
        let e = Ge3DPoint { x: 0.0, y: 8.0, z: 0.0 };
        assert_eq!(aim(s, e, 0.25, 0.5, 0.0).length, 4.0);
        assert_eq!(aim(s, e, 0.75, 0.0, 0.5).length, 4.0);
        let mut v =
            [EeGfxBeamVertex { point: Ge3DPoint { x: 2.0, y: 1.0, z: 3.0 }, coord: EeGfxTexCoord { u: 0.25, v: 1.0 } }];
        stretch(&mut v, 2.0, 0.5, 0.5);
        assert_eq!(v[0].point, Ge3DPoint { x: 1.0, y: 2.3, z: 3.0 });
        assert_eq!(v[0].coord, EeGfxTexCoord { u: 0.75, v: 1.0 });
    }
}
