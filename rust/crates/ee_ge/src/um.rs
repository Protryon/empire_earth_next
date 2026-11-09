//! The engine's float helpers (`LowLevelEngineCore.h`, `UMath.cpp`): `UMFloatToLong`, the floors, and the lookup-table
//! trigonometry `UMCos`, `UMArcCos`, `UMArcTan`, `UMArcTan2`, `UMTan`, `UMQuadraticEquation`. The C++ functions forward
//! here (`UMath.cpp`); the simulation calls them everywhere, so every operation is `f32` (or `f64` where the C++ is
//! `double`) in the C++'s order (RUST_PLAN 2.1).
//!
//! `UMCos` and `UMArcCos` are lookup tables. Rather than carry 49,154 numbers, the tables are recomputed the way the
//! C++ tables were generated (the `%f` spelling of an `f32` cosine, PORT_LEARNINGS "R3.4"); `tables_digest` pins them
//! to the C++ tables (`UMTables.h`, deleted with this move).

use std::sync::OnceLock;

pub const PI: f32 = 3.1415927;
pub const TWO_PI: f32 = 6.2831855;
pub const HALF_PI: f32 = 1.5707964;
pub const MINUS_HALF_PI: f32 = -1.5707964;

const COS_TABLE_SCALE: f32 = 5215.189;
const ONE_THIRD: f32 = 0.33333334;
const ONE_FIFTH: f32 = 0.2;
const ONE_SEVENTH: f32 = 0.14285715;
const ARCCOS_TABLE_LIMIT: f32 = 0.9995;
const QUARTER_PI: f32 = 0.78539819;

/// `UMFloatToLong` (`lrintf`) as the x86-64 build computes it: round half to even, and the conversion's "indefinite"
/// value for NaN or a value out of range (which `as i32` would saturate instead).
#[inline]
pub fn lrintf(value: f32) -> i32 {
    let rounded = value.round_ties_even();
    if (-2_147_483_648.0..2_147_483_648.0).contains(&rounded) {
        rounded as i32
    } else {
        i32::MIN
    }
}

/// `UMFloor`: rounds to nearest, then steps down when that went up.
#[inline]
pub fn floor(value: f32) -> i32 {
    let result = lrintf(value);
    if result as f32 > value {
        result.wrapping_sub(1)
    } else {
        result
    }
}

/// `UMFloorUnsigned`: as `floor`, with the comparison on the unsigned value (a negative input wraps).
#[inline]
pub fn floor_unsigned(value: f32) -> u32 {
    let result = lrintf(value) as u32;
    if result as f32 > value {
        result.wrapping_sub(1)
    } else {
        result
    }
}

/// `UMFastMath::Sqrt`, and the C++'s `(float)sqrt(float)`: through `double` (the same bits as a float square root).
#[inline]
pub fn sqrt(value: f32) -> f32 {
    f64::from(value).sqrt() as f32
}

/// `(float)sqrt(unsigned long)`.
#[inline]
pub fn sqrt_u32(value: u32) -> f32 {
    f64::from(value).sqrt() as f32
}

struct Tables {
    cos: Vec<f32>,
    arccos: Vec<f32>,
}

/// One table entry: the `%f` text of an `f32` read back as an `f32`, as the C++ compiler read `UMTables.h`.
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

/// `UMCos`: the table cosine after wrapping the angle into -pi..pi. A NaN angle indexes the C++ table at `INT_MIN`
/// (an access violation); here it panics.
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

/// `UMSin` (an inline of the header): the cosine of pi/2 minus the angle.
#[inline]
pub fn um_sin(angle: f32) -> f32 {
    um_cos(HALF_PI - angle)
}

/// `UMArcCos`: the table below 0.9995, a series up to 1, 0 above 1; values below -1 are clamped.
pub fn um_arccos(value: f32) -> f32 {
    let mut value = value;
    if value > 0.0 {
        if value < ARCCOS_TABLE_LIMIT {
            return tables().arccos[lrintf(value * 32768.0) as usize];
        }
        if value < 1.0 {
            // the C++'s shape: one power variable, the sum kept in the argument
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

/// `UMArcTan`: a power series on 0..1, values outside folded back through 1/x; 0 and NaN give 0 and pi/4.
pub fn um_arctan(value: f32) -> f32 {
    if value > 0.0 && value < 1.0 {
        let square = value * value;
        let mut power = value * square;
        let mut result = value - ONE_THIRD * power;
        power = square * power;
        result += ONE_FIFTH * power;
        // (x^2 * x^5) first, then 1/7, as the C++ (a different grouping rounds differently)
        return result - (square * power) * ONE_SEVENTH;
    }
    if value > 1.0 {
        let inverse = 1.0 / value;
        if f64::from(inverse).is_finite() {
            return HALF_PI - um_arctan(inverse);
        }
        return HALF_PI;
    }
    if value < -1.0 {
        let inverse = -1.0 / value;
        if f64::from(inverse).is_finite() {
            return um_arctan(inverse) - HALF_PI;
        }
        return MINUS_HALF_PI;
    }
    if value < 0.0 {
        return -um_arctan(-value);
    }
    if value == 0.0 {
        0.0
    } else {
        QUARTER_PI
    }
}

/// `UMArcTan2`: the arc cosine of the normalised x, signed by y.
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

/// `UMTan`: the table sine over the table cosine; plus or minus `FLT_MAX` where the cosine is 0.
pub fn um_tan(angle: f32) -> f32 {
    let cosine = um_cos(angle);
    if cosine != 0.0 {
        return um_sin(angle) / cosine;
    }
    if (angle >= 0.0 && angle < HALF_PI) || (angle >= -PI && angle <= MINUS_HALF_PI) {
        return f32::MAX;
    }
    -f32::MAX
}

/// `UMQuadraticEquation`: the two roots of a*x^2 + b*x + c (the discriminant in `double`, not checked for a negative
/// value); `None` when a and b are 0 (the C++ leaves the roots unwritten then).
pub fn um_quadratic(a: f32, b: f32, c: f32) -> Option<(f32, f32)> {
    if a != 0.0 {
        let discriminant = f64::from(b * b - a * c * 4.0);
        let a2 = a + a;
        let root1 = ((discriminant.sqrt() as f32) - b) / a2;
        let root2 = (-b - (discriminant.sqrt() as f32)) / a2;
        return Some((root1, root2));
    }
    if b != 0.0 {
        let root = (-c) / b;
        return Some((root, root));
    }
    None
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

    /// FNV-1a over `sCosTable` then `sArcCosTable` (UMTables.h), from `rmap_dump --tables` (R3.4).
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
        assert_eq!(um_arccos(2.0), 0.0);
        assert_eq!(um_arccos(0.0), HALF_PI);
        assert_eq!(um_arccos(-5.0), PI - um_arccos(1.0));
    }

    #[test]
    fn cos_wraps_the_angle_and_is_even() {
        assert_eq!(um_cos(1.0), um_cos(-1.0));
        assert_eq!(um_cos(1.0).to_bits(), um_cos(1.0 + TWO_PI).to_bits());
        assert_eq!(um_sin(0.3).to_bits(), um_cos(HALF_PI - 0.3).to_bits());
    }

    #[test]
    fn arctan_special_values() {
        assert_eq!(um_arctan(0.0), 0.0);
        assert_eq!(um_arctan(1.0), QUARTER_PI);
        assert_eq!(um_arctan(-1.0), -QUARTER_PI);
        assert_eq!(um_arctan(f32::NAN), QUARTER_PI);
        assert_eq!(um_arctan(f32::INFINITY), HALF_PI); // 1/inf is 0: finite, pi/2 - atan(0)
        assert_eq!(um_arctan(-f32::INFINITY), MINUS_HALF_PI);
    }

    #[test]
    fn arctan2_handles_the_axes() {
        assert_eq!(um_arctan2(1.0, 0.0), HALF_PI);
        assert_eq!(um_arctan2(-1.0, 0.0), MINUS_HALF_PI);
        assert_eq!(um_arctan2(0.0, 0.0), HALF_PI);
        assert_eq!(um_arctan2(0.0, 1.0), um_arccos(1.0));
        assert_eq!(um_arctan2(-0.5, 1.0), -um_arctan2(0.5, 1.0));
    }

    #[test]
    fn tan_is_flt_max_where_the_table_cosine_is_zero() {
        // the table's cos(pi/2) entry is a plus zero
        assert_eq!(um_cos(HALF_PI), 0.0);
        assert_eq!(um_tan(HALF_PI), -f32::MAX); // pi/2 itself is outside [0, pi/2)
        assert_eq!(um_tan(-HALF_PI), f32::MAX);
        assert_eq!(um_tan(0.0), 0.0);
    }

    #[test]
    fn quadratic_roots_and_degenerate_cases() {
        assert_eq!(um_quadratic(1.0, -3.0, 2.0), Some((2.0, 1.0)));
        assert_eq!(um_quadratic(0.0, 2.0, -4.0), Some((2.0, 2.0)));
        assert_eq!(um_quadratic(0.0, 0.0, 1.0), None);
        let (r1, r2) = um_quadratic(1.0, 0.0, 1.0).unwrap();
        assert!(r1.is_nan() && r2.is_nan());
    }
}
