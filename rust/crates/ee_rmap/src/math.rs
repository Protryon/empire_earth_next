//! The engine's float helpers the generator uses (`LowLevelEngineCore.h`, `UMath.cpp`, `GE2DPoint`, `GEVector`,
//! `GETransformation`), bit for bit: every operation in `f32`, in the C++'s order (RUST_PLAN 2.1).
//!
//! `UMCos` and `UMArcCos` are lookup tables. Rather than carry 49,154 numbers, the tables are recomputed the way the
//! C++ tables were generated (`UMTables.h`: the `%f` spelling of an `f32` cosine); `tables_digest` pins them to the C++.

use std::sync::OnceLock;

pub const PI: f32 = 3.1415927;
pub const TWO_PI: f32 = 6.2831855;
pub const HALF_PI: f32 = 1.5707964;

const COS_TABLE_SCALE: f32 = 5215.189;
const ONE_THIRD: f32 = 0.33333334;
const ONE_FIFTH: f32 = 0.2;
const ONE_SEVENTH: f32 = 0.14285715;
const ARCCOS_TABLE_LIMIT: f32 = 0.9995;
const MINUS_HALF_PI: f32 = -1.5707964;

/// `lrintf` as the x86-64 build computes it: round half to even, and the conversion's "indefinite" value for NaN or a
/// value out of range (which `as i32` would saturate instead).
pub fn lrintf(value: f32) -> i32 {
    let rounded = value.round_ties_even();
    if (-2_147_483_648.0..2_147_483_648.0).contains(&rounded) {
        rounded as i32
    } else {
        i32::MIN
    }
}

/// `UMFloor`: rounds to nearest, then steps down when that went up.
pub fn floor(value: f32) -> i32 {
    let result = lrintf(value);
    if result as f32 > value {
        result.wrapping_sub(1)
    } else {
        result
    }
}

/// `UMFloorUnsigned`: as `floor`, with the comparison on the unsigned value (a negative input wraps).
pub fn floor_unsigned(value: f32) -> u32 {
    let result = lrintf(value) as u32;
    if result as f32 > value {
        result.wrapping_sub(1)
    } else {
        result
    }
}

/// `UMFastMath::Sqrt`, and the C++'s `(float)sqrt(float)`: through `double`.
pub fn sqrt(value: f32) -> f32 {
    f64::from(value).sqrt() as f32
}

/// `(float)sqrt(unsigned long)`.
pub fn sqrt_u32(value: u32) -> f32 {
    f64::from(value).sqrt() as f32
}

struct Tables {
    cos: Vec<f32>,
    arccos: Vec<f32>,
}

/// One table entry: the `%f` text of an `f32` read back as an `f32`, as the C++ compiler reads `UMTables.h`.
fn entry(value: f32) -> f32 {
    let parsed: f32 = format!("{value:.6}").parse().expect("a formatted float parses");
    // the table spells cos(pi/2), a tiny negative, as 0.000000: a plus zero
    if parsed == 0.0 {
        0.0
    } else {
        parsed
    }
}

fn tables() -> &'static Tables {
    static TABLES: OnceLock<Tables> = OnceLock::new();
    TABLES.get_or_init(|| {
        let cos = (0..=16384u32).map(|i| entry(f64::from(i as f32 * PI / 16384.0).cos() as f32)).collect();
        let arccos = (0..=32768u32).map(|i| entry(f64::from(i as f32 / 32768.0).acos() as f32)).collect();
        Tables { cos, arccos }
    })
}

/// `UMCos`.
pub fn um_cos(angle: f32) -> f32 {
    let mut angle = angle;
    while angle < -PI {
        angle += TWO_PI;
    }
    while angle > PI {
        angle -= TWO_PI;
    }
    if angle < 0.0 {
        angle = -angle;
    }
    tables().cos[lrintf(COS_TABLE_SCALE * angle) as usize]
}

/// `UMSin`.
pub fn um_sin(angle: f32) -> f32 {
    um_cos(HALF_PI - angle)
}

/// `UMArcCos`.
pub fn um_arccos(value: f32) -> f32 {
    let mut value = value;
    if value > 0.0 {
        if value < ARCCOS_TABLE_LIMIT {
            return tables().arccos[lrintf(value * 32768.0) as usize];
        }
        if value < 1.0 {
            let mut power = sqrt(1.0 - value * value) / value;
            let square = power * power;
            value = power;
            power *= square;
            value -= ONE_THIRD * power;
            power *= square;
            value += ONE_FIFTH * power;
            power *= square;
            value -= power * ONE_SEVENTH;
        } else {
            value = 0.0;
        }
        return value;
    }
    if value < 0.0 {
        if value < -1.0 {
            value = -1.0;
        }
        return PI - um_arccos(-value);
    }
    HALF_PI
}

/// `UMArcTan2`.
pub fn um_arctan2(y: f32, x: f32) -> f32 {
    if x != 0.0 {
        let cosine = x / sqrt(y * y + x * x);
        if y >= 0.0 {
            return um_arccos(cosine);
        }
        return -um_arccos(cosine);
    }
    if y < 0.0 {
        return MINUS_HALF_PI;
    }
    HALF_PI
}

/// `GE2DPoint`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Point {
    pub x: f32,
    pub y: f32,
}

impl Point {
    pub fn new(x: f32, y: f32) -> Self {
        Point { x, y }
    }

    pub fn distance_to_squared(self, other: Point) -> f32 {
        let dy = self.y - other.y;
        let dx = self.x - other.x;
        dx * dx + dy * dy
    }

    pub fn distance_to(self, other: Point) -> f32 {
        sqrt((self.x - other.x) * (self.x - other.x) + (self.y - other.y) * (self.y - other.y))
    }

    /// `GE2DPoint::InterpolateTo`: points every `step` along the segment, always ending with `to`.
    pub fn interpolate_to(self, to: Point, step: f32, out: &mut Vec<Point>) {
        out.clear();
        if self != to {
            let mut direction = Vector { x: to.x - self.x, y: to.y - self.y, z: 0.0 };
            direction.normalize();
            direction.x *= step;
            direction.y *= step;
            // GE3DLine::SolveForX, else SolveForY. A direction normalized to zero (the points closer than 0.0001)
            // solves neither: the C++ then reads an uninitialized end, here 0 (no points before `to`).
            let end = if direction.x != 0.0 {
                (to.x - self.x) / direction.x
            } else if direction.y != 0.0 {
                (to.y - self.y) / direction.y
            } else {
                0.0
            };
            let mut t = 0.0f32;
            while t < end {
                out.push(Point { x: direction.x * t + self.x, y: direction.y * t + self.y });
                t += step;
            }
        }
        out.push(to);
    }
}

/// `GEVector`.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Vector {
    pub x: f32,
    pub y: f32,
    pub z: f32,
}

impl Vector {
    pub fn magnitude(self) -> f32 {
        sqrt(self.x * self.x + self.y * self.y + self.z * self.z)
    }

    pub fn normalize(&mut self) {
        let magnitude = self.magnitude();
        if (-0.0001..=0.0001).contains(&magnitude) {
            *self = Vector::default();
        } else {
            let scale = 1.0 / magnitude;
            self.x *= scale;
            self.y *= scale;
            self.z *= scale;
        }
    }

    pub fn scale(self, scale: f32) -> Vector {
        Vector { x: scale * self.x, y: scale * self.y, z: scale * self.z }
    }
}

/// `GETransformation`, its rotation part (the generator sets only `SetOrientationYPR` and applies it to vectors).
#[derive(Clone, Copy, Debug)]
pub struct Rotation {
    m: [[f32; 3]; 3],
}

impl Rotation {
    /// `SetOrientationYPR`.
    pub fn yaw_pitch_roll(yaw: f32, pitch: f32, roll: f32) -> Self {
        let cos_yaw = um_cos(yaw);
        let sin_yaw = um_sin(yaw);
        let neg_sin_yaw = -sin_yaw;
        let cos_pitch = um_cos(pitch);
        let sin_pitch = um_sin(pitch);
        let neg_sin_pitch = -sin_pitch;
        let cos_roll = um_cos(roll);
        let sin_roll = um_sin(roll);
        let neg_sin_roll = -sin_roll;
        let yp01 = neg_sin_yaw * cos_pitch;
        let yp02 = neg_sin_yaw * neg_sin_pitch;
        let yp11 = cos_yaw * cos_pitch;
        let yp12 = cos_yaw * neg_sin_pitch;
        Rotation {
            m: [
                [cos_yaw * cos_roll + yp02 * sin_roll, yp01, cos_yaw * neg_sin_roll + yp02 * cos_roll],
                [sin_yaw * cos_roll + yp12 * sin_roll, yp11, sin_yaw * neg_sin_roll + yp12 * cos_roll],
                [cos_pitch * sin_roll, sin_pitch, cos_pitch * cos_roll],
            ],
        }
    }

    /// A yaw alone, as the generator always calls it.
    pub fn yaw(yaw: f32) -> Self {
        Self::yaw_pitch_roll(yaw, 0.0, 0.0)
    }

    /// `Apply(const GEVector&, GEVector&)`: the sums in the C++'s order.
    pub fn apply(&self, v: Vector) -> Vector {
        let m = &self.m;
        Vector {
            x: v.z * m[0][2] + v.y * m[0][1] + v.x * m[0][0],
            y: v.x * m[1][0] + v.z * m[1][2] + v.y * m[1][1],
            z: v.x * m[2][0] + v.z * m[2][2] + v.y * m[2][1],
        }
    }
}

/// FNV-1a of the `f32` bit patterns of both lookup tables, compared with the C++ tables in a test.
pub fn tables_digest() -> u64 {
    let mut hash = 0xcbf2_9ce4_8422_2325u64;
    let t = tables();
    for value in t.cos.iter().chain(t.arccos.iter()) {
        for byte in value.to_bits().to_le_bytes() {
            hash ^= u64::from(byte);
            hash = hash.wrapping_mul(0x0000_0100_0000_01b3);
        }
    }
    hash
}

#[cfg(test)]
mod tests {
    use super::*;

    /// FNV-1a over `sCosTable` then `sArcCosTable` (UMTables.h), from `rmap_dump --tables`.
    const TABLES_DIGEST: u64 = 0xbca1_7df0_a7ca_d6c2;

    #[test]
    fn lrintf_rounds_half_to_even_and_flags_out_of_range() {
        assert_eq!(lrintf(0.5), 0);
        assert_eq!(lrintf(1.5), 2);
        assert_eq!(lrintf(2.5), 2);
        assert_eq!(lrintf(-1.5), -2);
        assert_eq!(lrintf(f32::NAN), i32::MIN);
        assert_eq!(lrintf(3e9), i32::MIN);
        assert_eq!(lrintf(-3e9), i32::MIN);
    }

    #[test]
    fn floors_step_down_only_when_rounding_went_up() {
        assert_eq!(floor(1.7), 1);
        assert_eq!(floor(-1.2), -2);
        assert_eq!(floor(2.0), 2);
        assert_eq!(floor_unsigned(3.9), 3);
        assert_eq!(floor_unsigned(0.2), 0);
    }

    #[test]
    fn floor_unsigned_of_a_negative_value_wraps() {
        // lrintf(-0.6) is -1, as unsigned 0xFFFFFFFF, which is above -0.6: one less
        assert_eq!(floor_unsigned(-0.6), 0xFFFF_FFFE);
    }

    #[test]
    fn the_tables_are_the_cpp_tables() {
        assert_eq!(tables_digest(), TABLES_DIGEST);
    }

    #[test]
    fn table_entries_at_the_ends() {
        assert_eq!(um_cos(0.0), 1.0);
        assert_eq!(um_cos(PI), -1.0);
        assert_eq!(um_arccos(1.0), 0.0);
        assert_eq!(um_arccos(0.0), HALF_PI);
    }

    #[test]
    fn cos_wraps_the_angle_and_is_even() {
        assert_eq!(um_cos(1.0), um_cos(-1.0));
        assert_eq!(um_cos(1.0).to_bits(), um_cos(1.0 + TWO_PI).to_bits());
    }

    #[test]
    fn arctan2_handles_the_axes() {
        assert_eq!(um_arctan2(1.0, 0.0), HALF_PI);
        assert_eq!(um_arctan2(-1.0, 0.0), MINUS_HALF_PI);
        assert_eq!(um_arctan2(0.0, 1.0), um_arccos(1.0));
    }

    #[test]
    fn normalize_zeroes_a_tiny_vector() {
        let mut v = Vector { x: 0.00001, y: 0.0, z: 0.0 };
        v.normalize();
        assert_eq!(v, Vector::default());
        let mut v = Vector { x: 3.0, y: 4.0, z: 0.0 };
        v.normalize();
        assert_eq!(v, Vector { x: 0.6, y: 0.8, z: 0.0 });
    }

    #[test]
    fn interpolate_ends_with_the_target_and_equal_points_give_one() {
        let mut out = Vec::new();
        Point::new(0.0, 0.0).interpolate_to(Point::new(2.0, 0.0), 1.0, &mut out);
        assert_eq!(out, vec![Point::new(0.0, 0.0), Point::new(1.0, 0.0), Point::new(2.0, 0.0)]);
        Point::new(5.0, 5.0).interpolate_to(Point::new(5.0, 5.0), 1.0, &mut out);
        assert_eq!(out, vec![Point::new(5.0, 5.0)]);
    }

    #[test]
    fn a_quarter_turn_rotates_y_to_minus_x() {
        let r = Rotation::yaw(HALF_PI);
        let v = r.apply(Vector { x: 0.0, y: 1.0, z: 0.0 });
        assert!((v.x + 1.0).abs() < 0.001, "{v:?}");
        assert!(v.y.abs() < 0.001, "{v:?}");
    }
}
