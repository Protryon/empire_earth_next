//! `GETransformation` (`GETransformation.cpp`, the transformation block of `GE3DPoint.cpp`), `GEOrientation::Normalize`
//! and `GEQuaternion` (`GEQuaternion.cpp`). The header inlines `Apply(point)` and `Apply(vector)` stay C++; they are
//! re-stated here, in the C++'s summation order, for the Rust algorithms.

use crate::types::{Ge3DLine, Ge3DPlane, Ge3DPoint, GeOrientation, GeQuaternion, GeTransformation, GeVector};
use crate::um::{sqrt, um_arccos, um_arctan2, um_cos, um_sin, HALF_PI, PI, TWO_PI};

/// The orientation `SetOrientation*` variants (the order the three rotations are applied in).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(u32)]
pub enum RotationOrder {
    Pry = 0,
    Pyr = 1,
    Rpy = 2,
    Ryp = 3,
    Ypr = 4,
    Yrp = 5,
}

struct Trig {
    cy: f32,
    sy: f32,
    nsy: f32,
    cp: f32,
    sp: f32,
    nsp: f32,
    cr: f32,
    sr: f32,
    nsr: f32,
}

fn trig(yaw: f32, pitch: f32, roll: f32) -> Trig {
    let cy = um_cos(yaw);
    let sy = um_sin(yaw);
    let cp = um_cos(pitch);
    let sp = um_sin(pitch);
    let cr = um_cos(roll);
    let sr = um_sin(roll);
    Trig { cy, sy, nsy: -sy, cp, sp, nsp: -sp, cr, sr, nsr: -sr }
}

impl GeTransformation {
    /// The identity (`GETransformation()`).
    pub const IDENTITY: GeTransformation = GeTransformation {
        m: [[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0], [0.0, 0.0, 0.0, 1.0]],
    };

    #[inline]
    pub fn x(&self) -> f32 {
        self.m[0][3]
    }

    #[inline]
    pub fn y(&self) -> f32 {
        self.m[1][3]
    }

    #[inline]
    pub fn z(&self) -> f32 {
        self.m[2][3]
    }

    /// The header's `Apply(point, out)`.
    #[inline]
    pub fn apply_point(&self, p: Ge3DPoint) -> Ge3DPoint {
        let m = &self.m;
        Ge3DPoint {
            x: p.z * m[0][2] + p.y * m[0][1] + p.x * m[0][0] + m[0][3],
            y: p.x * m[1][0] + p.z * m[1][2] + p.y * m[1][1] + m[1][3],
            z: p.x * m[2][0] + p.z * m[2][2] + p.y * m[2][1] + m[2][3],
        }
    }

    /// The header's `Apply(vector, out)`.
    #[inline]
    pub fn apply_vector(&self, v: GeVector) -> GeVector {
        let m = &self.m;
        GeVector {
            x: v.z * m[0][2] + v.y * m[0][1] + v.x * m[0][0],
            y: v.x * m[1][0] + v.z * m[1][2] + v.y * m[1][1],
            z: v.x * m[2][0] + v.z * m[2][2] + v.y * m[2][1],
        }
    }

    /// `Apply(transformation, out)`: this x other in the 3x4 part, the bottom row copied from `other`. The C++ loads
    /// both matrices before it stores, so an output equal to either input is safe.
    pub fn compose(&self, other: &GeTransformation) -> GeTransformation {
        let a = &self.m;
        let b = &other.m;
        let mut out = GeTransformation::default();
        for i in 0..3 {
            out.m[i][0] = a[i][0] * b[0][0] + a[i][1] * b[1][0] + a[i][2] * b[2][0];
            out.m[i][1] = a[i][0] * b[0][1] + a[i][1] * b[1][1] + a[i][2] * b[2][1];
            out.m[i][2] = a[i][0] * b[0][2] + a[i][1] * b[1][2] + a[i][2] * b[2][2];
            out.m[i][3] = a[i][0] * b[0][3] + a[i][1] * b[1][3] + a[i][2] * b[2][3] + a[i][3];
        }
        out.m[3] = b[3];
        out
    }

    /// `Apply(line, out)`.
    pub fn apply_line(&self, line: &Ge3DLine) -> Ge3DLine {
        Ge3DLine { start: self.apply_point(line.start), direction: self.apply_vector(line.direction) }
    }

    /// `Apply(plane, out)`.
    pub fn apply_plane(&self, plane: &Ge3DPlane) -> Ge3DPlane {
        Ge3DPlane { point: self.apply_point(plane.point), normal: self.apply_vector(plane.normal) }
    }

    /// `GetYaw`.
    pub fn yaw(&self) -> f32 {
        um_arctan2(self.m[1][0], self.m[0][0])
    }

    /// `GetPitch`.
    pub fn pitch(&self) -> f32 {
        um_arccos(-self.m[2][1]) - HALF_PI
    }

    /// `GetRoll`.
    pub fn roll(&self) -> f32 {
        um_arctan2(self.m[2][0], self.m[0][0])
    }

    /// `ConvertToOrientation`.
    pub fn orientation(&self) -> GeOrientation {
        GeOrientation { yaw: self.yaw(), pitch: self.pitch(), roll: self.roll() }
    }

    /// `DistanceTo(point)`: the full-precision square root (the C++'s `(float)sqrt`, the same bits as the fast one).
    pub fn distance_to_point(&self, p: Ge3DPoint) -> f32 {
        sqrt(
            (p.x - self.x()) * (p.x - self.x())
                + (p.y - self.y()) * (p.y - self.y())
                + (p.z - self.z()) * (p.z - self.z()),
        )
    }

    /// `DistanceSquaredTo(point)`.
    pub fn distance_squared_to_point(&self, p: Ge3DPoint) -> f32 {
        let dz = p.z - self.z();
        let dy = p.y - self.y();
        let dx = p.x - self.x();
        dx * dx + dy * dy + dz * dz
    }

    /// `DistanceSquaredTo(transformation)`.
    pub fn distance_squared_to(&self, other: &GeTransformation) -> f32 {
        let dz = self.z() - other.z();
        let dy = self.y() - other.y();
        let dx = self.x() - other.x();
        dx * dx + dy * dy + dz * dz
    }

    /// `DistanceTo(transformation)` (`UMFastMath::Sqrt`).
    pub fn distance_to(&self, other: &GeTransformation) -> f32 {
        sqrt(self.distance_squared_to(other))
    }

    /// `MakeLookAtTransform(eye, target)`: a camera at the origin looking along +y toward the target's direction
    /// (straight up or down: a fixed matrix). The position column is cleared.
    pub fn make_look_at(&mut self, eye: Ge3DPoint, target: Ge3DPoint) {
        const LOOK_DOWN: [[f32; 4]; 4] =
            [[1.0, 0.0, 0.0, 0.0], [0.0, 0.0, -1.0, 0.0], [0.0, 1.0, 0.0, 0.0], [0.0, 0.0, 0.0, 1.0]];
        let mut direction = GeVector::new(target.x - eye.x, target.y - eye.y, target.z - eye.z);
        direction.normalize();
        if direction.x >= -0.0001 && direction.x <= 0.0001 && direction.y >= -0.0001 && direction.y <= 0.0001 {
            self.m = LOOK_DOWN;
            if direction.z < 0.0 {
                self.m[1][2] = 1.0;
                self.m[2][1] = -1.0;
            }
        } else {
            let length = sqrt(direction.x * direction.x + direction.y * direction.y);
            let right = GeVector::new(direction.y / length, -(direction.x / length), 0.0);
            let mut up = right.cross(direction);
            up.normalize();
            self.m = [
                [right.x, direction.x, up.x, 0.0],
                [right.y, direction.y, up.y, 0.0],
                [right.z, direction.z, up.z, 0.0],
                [0.0, 0.0, 0.0, 1.0],
            ];
        }
    }

    /// `Normalize`: each rotation column scaled to unit length (`1 / sqrt`).
    pub fn normalize(&mut self) {
        for i in 0..3 {
            let m = &mut self.m;
            let scale = m[0][i] * m[0][i] + m[1][i] * m[1][i] + m[2][i] * m[2][i];
            let scale = 1.0 / sqrt(scale);
            m[0][i] *= scale;
            m[1][i] *= scale;
            m[2][i] *= scale;
        }
    }

    /// A rotation matrix as `Revolve`/`Rotate` build it: no translation, an identity bottom row, `SetOrientationYPR`.
    fn rotation(yaw: f32, pitch: f32, roll: f32) -> GeTransformation {
        let mut r = GeTransformation::default();
        r.m[3][3] = 1.0;
        r.set_orientation(RotationOrder::Ypr, yaw, pitch, roll);
        r
    }

    /// `Revolve(yaw, pitch, roll)`: this x R (about the transformation's own axes; the bottom row becomes R's).
    pub fn revolve(&mut self, yaw: f32, pitch: f32, roll: f32) {
        *self = self.compose(&Self::rotation(yaw, pitch, roll));
    }

    /// `Rotate(yaw, pitch, roll)`: R x this (about the parent axes).
    pub fn rotate(&mut self, yaw: f32, pitch: f32, roll: f32) {
        *self = Self::rotation(yaw, pitch, roll).compose(self);
    }

    /// `Scale(x, y, z)`: S x this.
    pub fn scale(&mut self, x: f32, y: f32, z: f32) {
        let mut s = GeTransformation::default();
        s.set_scale(x, y, z);
        *self = s.compose(self);
    }

    /// `SetScale(x, y, z)`: a pure scale, the position column kept.
    pub fn set_scale(&mut self, x: f32, y: f32, z: f32) {
        let p = [self.m[0][3], self.m[1][3], self.m[2][3]];
        self.m = [[x, 0.0, 0.0, p[0]], [0.0, y, 0.0, p[1]], [0.0, 0.0, z, p[2]], [0.0, 0.0, 0.0, 1.0]];
    }

    /// `SetOrientationPRY` .. `SetOrientationYRP`: the 3x3 rotation from yaw, pitch and roll (the table sine and
    /// cosine), the position and bottom row kept. Each variant's products in the C++'s order.
    pub fn set_orientation(&mut self, order: RotationOrder, yaw: f32, pitch: f32, roll: f32) {
        let t = trig(yaw, pitch, roll);
        let m = &mut self.m;
        match order {
            RotationOrder::Pry => {
                let pr10 = t.nsp * t.sr;
                let pr12 = t.nsp * t.cr;
                let pr20 = t.cp * t.sr;
                let pr22 = t.cp * t.cr;
                m[0][0] = t.cr * t.cy;
                m[0][1] = t.cr * t.nsy;
                m[0][2] = t.nsr;
                m[1][0] = pr10 * t.cy + t.cp * t.sy;
                m[1][1] = pr10 * t.nsy + t.cp * t.cy;
                m[1][2] = pr12;
                m[2][0] = pr20 * t.cy + t.sp * t.sy;
                m[2][1] = pr20 * t.nsy + t.sp * t.cy;
                m[2][2] = pr22;
            }
            RotationOrder::Pyr => {
                let py10 = t.cp * t.sy;
                let py11 = t.cp * t.cy;
                let py20 = t.sp * t.sy;
                let py21 = t.sp * t.cy;
                m[0][0] = t.cy * t.cr;
                m[0][1] = t.nsy;
                m[0][2] = t.cy * t.nsr;
                m[1][0] = py10 * t.cr + t.nsp * t.sr;
                m[1][1] = py11;
                m[1][2] = py10 * t.nsr + t.nsp * t.cr;
                m[2][0] = py20 * t.cr + t.cp * t.sr;
                m[2][1] = py21;
                m[2][2] = py20 * t.nsr + t.cp * t.cr;
            }
            RotationOrder::Rpy => {
                let rp02 = t.nsr * t.cp;
                let rp22 = t.cr * t.cp;
                let rp01 = t.nsr * t.sp;
                let rp21 = t.cr * t.sp;
                m[0][0] = t.cr * t.cy + rp01 * t.sy;
                m[0][1] = t.cr * t.nsy + rp01 * t.cy;
                m[0][2] = rp02;
                m[1][0] = t.cp * t.sy;
                m[1][1] = t.cp * t.cy;
                m[1][2] = t.nsp;
                m[2][0] = t.sr * t.cy + rp21 * t.sy;
                m[2][1] = t.sr * t.nsy + rp21 * t.cy;
                m[2][2] = rp22;
            }
            RotationOrder::Ryp => {
                let ry00 = t.cr * t.cy;
                let ry01 = t.cr * t.nsy;
                let ry20 = t.sr * t.cy;
                let ry21 = t.sr * t.nsy;
                m[0][0] = ry00;
                m[0][1] = ry01 * t.cp + t.nsr * t.sp;
                m[0][2] = ry01 * t.nsp + t.nsr * t.cp;
                m[1][0] = t.sy;
                m[1][1] = t.cy * t.cp;
                m[1][2] = t.cy * t.nsp;
                m[2][0] = ry20;
                m[2][1] = ry21 * t.cp + t.cr * t.sp;
                m[2][2] = ry21 * t.nsp + t.cr * t.cp;
            }
            RotationOrder::Ypr => {
                let yp01 = t.nsy * t.cp;
                let yp02 = t.nsy * t.nsp;
                let yp11 = t.cy * t.cp;
                let yp12 = t.cy * t.nsp;
                m[0][0] = t.cy * t.cr + yp02 * t.sr;
                m[0][1] = yp01;
                m[0][2] = t.cy * t.nsr + yp02 * t.cr;
                m[1][0] = t.sy * t.cr + yp12 * t.sr;
                m[1][1] = yp11;
                m[1][2] = t.sy * t.nsr + yp12 * t.cr;
                m[2][0] = t.cp * t.sr;
                m[2][1] = t.sp;
                m[2][2] = t.cp * t.cr;
            }
            RotationOrder::Yrp => {
                let yr00 = t.cy * t.cr;
                let yr02 = t.cy * t.nsr;
                let yr10 = t.sy * t.cr;
                let yr12 = t.sy * t.nsr;
                m[0][0] = yr00;
                m[0][1] = t.nsy * t.cp + yr02 * t.sp;
                m[0][2] = t.nsy * t.nsp + yr02 * t.cp;
                m[1][0] = yr10;
                m[1][1] = t.cy * t.cp + yr12 * t.sp;
                m[1][2] = t.cy * t.nsp + yr12 * t.cp;
                m[2][0] = t.sr;
                m[2][1] = t.cr * t.sp;
                m[2][2] = t.cr * t.cp;
            }
        }
    }
}

/// `MakeInverse(out)` exactly as the C++ runs it: the transposed rotation is stored element by element (so an output
/// equal to `this` transposes through its own stores), then the negated position is rotated by the output.
///
/// # Safety
/// Both pointers are valid; they may be equal.
pub unsafe fn make_inverse_raw(this: *const GeTransformation, out: *mut GeTransformation) {
    // SAFETY: the function's contract; every access goes through the raw pointers, in the C++'s order
    unsafe {
        let negative = Ge3DPoint::new(-(*this).m[0][3], -(*this).m[1][3], -(*this).m[2][3]);
        (*out).m[0][0] = (*this).m[0][0];
        (*out).m[0][1] = (*this).m[1][0];
        (*out).m[0][2] = (*this).m[2][0];
        (*out).m[0][3] = 0.0;
        (*out).m[1][0] = (*this).m[0][1];
        (*out).m[1][1] = (*this).m[1][1];
        (*out).m[1][2] = (*this).m[2][1];
        (*out).m[1][3] = 0.0;
        (*out).m[2][0] = (*this).m[0][2];
        (*out).m[2][1] = (*this).m[1][2];
        (*out).m[2][2] = (*this).m[2][2];
        (*out).m[2][3] = 0.0;
        (*out).m[3][0] = 0.0;
        (*out).m[3][1] = 0.0;
        (*out).m[3][2] = 0.0;
        (*out).m[3][3] = 1.0;
        let position = (*out).apply_point(negative);
        (*out).m[0][3] = position.x;
        (*out).m[1][3] = position.y;
        (*out).m[2][3] = position.z;
    }
}

impl GeOrientation {
    /// `Normalize`: each angle wrapped into -pi..pi.
    pub fn normalize(&mut self) {
        for angle in [&mut self.yaw, &mut self.pitch, &mut self.roll] {
            while *angle > PI {
                *angle -= TWO_PI;
            }
            while *angle < -PI {
                *angle += TWO_PI;
            }
        }
    }
}

impl GeQuaternion {
    /// `Normalize` (`1 / UMFastMath::Sqrt`).
    pub fn normalize(&mut self) {
        let scale = 1.0 / sqrt(self.x * self.x + self.y * self.y + self.z * self.z + self.w * self.w);
        self.w *= scale;
        self.x *= scale;
        self.y *= scale;
        self.z *= scale;
    }

    /// `ConvertFrom(transformation)`: Shoemake's matrix-to-quaternion, the vector part negated, then normalized.
    pub fn from_transformation(t: &GeTransformation) -> GeQuaternion {
        let e = |i: usize, j: usize| t.m[i][j];
        let mut q = GeQuaternion::default();
        let trace = e(0, 0) + e(2, 2) + e(1, 1);
        if trace > 0.0 {
            let mut scale = sqrt(trace + 1.0);
            q.w = scale * 0.5;
            scale = 0.5 / scale;
            q.x = (e(1, 2) - e(2, 1)) * scale;
            q.y = (e(2, 0) - e(0, 2)) * scale;
            q.z = (e(0, 1) - e(1, 0)) * scale;
        } else {
            const NEXT: [usize; 3] = [1, 2, 0];
            let mut i = 0;
            if e(1, 1) > e(0, 0) {
                i = 1;
            }
            if e(2, 2) > e(i, i) {
                i = 2;
            }
            let j = NEXT[i];
            let k = NEXT[j];
            let mut scale = sqrt(e(i, i) - (e(j, j) + e(k, k)) + 1.0);
            let mut v = [0.0f32; 4];
            v[i] = scale * 0.5;
            if scale != 0.0 {
                scale = 0.5 / scale;
            }
            v[3] = (e(j, k) - e(k, j)) * scale;
            v[j] = (e(i, j) + e(j, i)) * scale;
            v[k] = (e(i, k) + e(k, i)) * scale;
            q = GeQuaternion { x: v[0], y: v[1], z: v[2], w: v[3] };
        }
        q.x = -q.x;
        q.y = -q.y;
        q.z = -q.z;
        q.normalize();
        q
    }

    /// `ConvertTo(transformation)`: the rotation of the normalized quaternion, no translation, then the matrix's
    /// `Normalize`.
    pub fn to_transformation(&self) -> GeTransformation {
        let mut q = *self;
        q.normalize();
        let s = 2.0f32;
        let xs = q.x * s;
        let ys = q.y * s;
        let zs = q.z * s;
        let xx = q.x * xs;
        let xy = q.x * ys;
        let xz = q.x * zs;
        let yy = q.y * ys;
        let yz = q.y * zs;
        let zz = q.z * zs;
        let wx = q.w * xs;
        let wy = q.w * ys;
        let wz = q.w * zs;
        let mut t = GeTransformation {
            m: [
                [1.0 - (yy + zz), xy - wz, xz + wy, 0.0],
                [xy + wz, 1.0 - (xx + zz), yz - wx, 0.0],
                [xz - wy, yz + wx, 1.0 - (xx + yy), 0.0],
                [0.0, 0.0, 0.0, 1.0],
            ],
        };
        t.normalize();
        t
    }

    /// `Slerp(target, t, out)`: along the shorter arc in `double`, linear when the two are within 0.001 of equal.
    /// Each output component reads only its own input component, so an output equal to `self` is safe.
    pub fn slerp(&self, target: &GeQuaternion, t: f32) -> GeQuaternion {
        let mut cos_omega = f64::from(target.z * self.z + target.y * self.y + target.x * self.x + self.w * target.w);
        let to = if cos_omega < 0.0 {
            cos_omega = -cos_omega;
            [-target.x, -target.y, -target.z, -target.w]
        } else {
            [target.x, target.y, target.z, target.w]
        };
        let t64 = f64::from(t);
        let (scale0, scale1) = if 1.0 - cos_omega > f64::from(0.001f32) {
            let omega = f64::from(um_arccos(cos_omega as f32));
            let sin_omega = f64::from(um_sin(omega as f32));
            (
                f64::from(um_sin(((1.0 - t64) * omega) as f32)) / sin_omega,
                f64::from(um_sin((t64 * omega) as f32)) / sin_omega,
            )
        } else {
            (1.0 - t64, t64)
        };
        GeQuaternion {
            x: (f64::from(self.x) * scale0 + f64::from(to[0]) * scale1) as f32,
            y: (f64::from(self.y) * scale0 + f64::from(to[1]) * scale1) as f32,
            z: (f64::from(self.z) * scale0 + f64::from(to[2]) * scale1) as f32,
            w: (f64::from(self.w) * scale0 + f64::from(to[3]) * scale1) as f32,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn close(a: f32, b: f32) -> bool {
        (a - b).abs() < 0.002
    }

    #[test]
    fn compose_with_identity_keeps_the_matrix_and_takes_its_bottom_row() {
        let mut a = GeTransformation::IDENTITY;
        a.set_orientation(RotationOrder::Ypr, 0.3, 0.2, 0.1);
        a.m[0][3] = 5.0;
        a.m[3] = [9.0, 9.0, 9.0, 9.0];
        let c = a.compose(&GeTransformation::IDENTITY);
        assert_eq!(&c.m[..3], &a.m[..3]);
        assert_eq!(c.m[3], [0.0, 0.0, 0.0, 1.0]);
        assert_eq!(GeTransformation::IDENTITY.compose(&a).m, a.m);
    }

    #[test]
    fn yaw_of_a_yaw_rotation() {
        let mut a = GeTransformation::IDENTITY;
        a.set_orientation(RotationOrder::Ypr, 0.5, 0.0, 0.0);
        assert!(close(a.yaw(), 0.5), "{}", a.yaw());
        assert!(close(a.pitch(), 0.0));
        let o = a.orientation();
        assert_eq!(o.yaw, a.yaw());
        assert_eq!(o.roll, a.roll());
    }

    #[test]
    fn inverse_undoes_a_rigid_transformation() {
        let mut a = GeTransformation::IDENTITY;
        a.set_orientation(RotationOrder::Ypr, 0.7, -0.2, 0.4);
        a.m[0][3] = 10.0;
        a.m[1][3] = -3.0;
        a.m[2][3] = 2.0;
        let mut inv = GeTransformation::default();
        // SAFETY: two valid, distinct pointers
        unsafe { make_inverse_raw(&a, &mut inv) };
        let p = inv.apply_point(a.apply_point(Ge3DPoint::new(1.0, 2.0, 3.0)));
        assert!(close(p.x, 1.0) && close(p.y, 2.0) && close(p.z, 3.0), "{p:?}");
    }

    #[test]
    fn inverse_in_place_transposes_through_its_own_stores() {
        let mut a = GeTransformation::IDENTITY;
        a.m[0][1] = 2.0;
        a.m[1][0] = 3.0;
        let p: *mut GeTransformation = &mut a;
        // SAFETY: one valid pointer, passed twice (allowed)
        unsafe { make_inverse_raw(p, p) };
        // [0][1] <- [1][0] = 3, then [1][0] <- [0][1], which is now 3
        assert_eq!(a.m[0][1], 3.0);
        assert_eq!(a.m[1][0], 3.0);
    }

    #[test]
    fn look_at_straight_down_and_ahead() {
        let mut t = GeTransformation::IDENTITY;
        t.make_look_at(Ge3DPoint::new(0.0, 0.0, 10.0), Ge3DPoint::new(0.0, 0.0, 0.0));
        assert_eq!(t.m[1][2], 1.0);
        assert_eq!(t.m[2][1], -1.0);
        t.make_look_at(Ge3DPoint::new(0.0, 0.0, 0.0), Ge3DPoint::new(0.0, 5.0, 0.0));
        assert_eq!(t.m, GeTransformation::IDENTITY.m);
    }

    #[test]
    fn normalize_scales_columns_to_unit_length() {
        let mut t = GeTransformation::IDENTITY;
        t.m[0][0] = 2.0;
        t.m[1][1] = 0.5;
        t.normalize();
        assert_eq!(t.m, GeTransformation::IDENTITY.m);
    }

    #[test]
    fn revolve_and_rotate_differ_in_the_bottom_row_and_order() {
        let mut a = GeTransformation::IDENTITY;
        a.m[3] = [1.0, 2.0, 3.0, 4.0];
        let mut r = a;
        r.revolve(0.3, 0.0, 0.0);
        assert_eq!(r.m[3], [0.0, 0.0, 0.0, 1.0]);
        let mut q = a;
        q.rotate(0.3, 0.0, 0.0);
        assert_eq!(q.m[3], [1.0, 2.0, 3.0, 4.0]);
        let mut s = a;
        s.scale(2.0, 3.0, 4.0);
        assert_eq!(s.m[1][1], 3.0);
    }

    #[test]
    fn every_rotation_order_gives_a_rotation() {
        for order in [
            RotationOrder::Pry,
            RotationOrder::Pyr,
            RotationOrder::Rpy,
            RotationOrder::Ryp,
            RotationOrder::Ypr,
            RotationOrder::Yrp,
        ] {
            let mut t = GeTransformation::IDENTITY;
            t.m[0][3] = 7.0;
            t.set_orientation(order, 0.4, -0.3, 1.1);
            assert_eq!(t.m[0][3], 7.0, "{order:?}");
            for i in 0..3 {
                let n = t.m[0][i] * t.m[0][i] + t.m[1][i] * t.m[1][i] + t.m[2][i] * t.m[2][i];
                assert!(close(n, 1.0), "{order:?} column {i}: {n}");
            }
        }
    }

    #[test]
    fn orientation_wraps_into_minus_pi_to_pi() {
        let mut o = GeOrientation { yaw: 7.0, pitch: -7.0, roll: 1.0 };
        o.normalize();
        assert_eq!(o.yaw, 7.0 - TWO_PI);
        assert_eq!(o.pitch, -7.0 + TWO_PI);
        assert_eq!(o.roll, 1.0);
    }

    #[test]
    fn quaternion_round_trip_and_slerp_ends() {
        let mut a = GeTransformation::IDENTITY;
        a.set_orientation(RotationOrder::Ypr, 0.6, 0.0, 0.0);
        let q = GeQuaternion::from_transformation(&a);
        let b = q.to_transformation();
        for i in 0..3 {
            for j in 0..3 {
                assert!(close(a.m[i][j], b.m[i][j]), "{i}{j}: {} {}", a.m[i][j], b.m[i][j]);
            }
        }
        let id = GeQuaternion::from_transformation(&GeTransformation::IDENTITY);
        assert_eq!(id, GeQuaternion { x: -0.0, y: -0.0, z: -0.0, w: 1.0 });
        assert_eq!(id.slerp(&q, 0.0), id);
        let end = id.slerp(&q, 1.0);
        assert!(close(end.w, q.w) && close(end.z, q.z), "{end:?} {q:?}");
        // nearly equal: linear
        assert_eq!(q.slerp(&q, 0.25), q);
    }
}
