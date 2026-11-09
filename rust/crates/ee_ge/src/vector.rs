//! `GEVector` (`GE2DPoint.cpp`'s vector block, `GEVector.cpp`): magnitude, cross product, the header's `Normalize`
//! and `DotProduct` (re-stated for the Rust algorithms; the C++ inlines stay C++), and the cube-face quantization
//! (`GetQuantizedVersion`, the table `QuantizedVectorArray` builds).

use crate::types::{Ge3DLine, Ge3DPlane, Ge3DPoint, GeVector};
use crate::um::{lrintf, sqrt};

/// Rows and columns of the quantized-direction table (`mVectors[41][6 * 41]`).
pub const QUANT_ROWS: usize = 41;
pub const QUANT_COLUMNS: usize = 6 * 41;
pub const QUANT_COUNT: usize = QUANT_ROWS * QUANT_COLUMNS;
const QUANT_STEP: f32 = 0.05;
const QUANT_SIZE: f32 = 40.0;

/// `GetQuantizedVersion`'s answer for the zero vector (the C++ returns its file-static zero vector).
pub const QUANT_ZERO: i32 = -1;
/// No face was hit (only a NaN component gets here; the C++ returns NULL after its ESVERIFY).
pub const QUANT_NONE: i32 = -2;
const _: () = assert!(QUANT_ZERO == -1 && QUANT_NONE == -2); // the C ABI's documented values

impl GeVector {
    /// `GetMagnitudeSquared`.
    #[inline]
    pub fn magnitude_squared(self) -> f32 {
        self.x * self.x + self.y * self.y + self.z * self.z
    }

    /// `GetMagnitude` (`UMFastMath::Sqrt`).
    #[inline]
    pub fn magnitude(self) -> f32 {
        sqrt(self.x * self.x + self.y * self.y + self.z * self.z)
    }

    /// The header's inline `DotProduct`: z first, as the C++ sums it.
    #[inline]
    pub fn dot(self, other: GeVector) -> f32 {
        other.z * self.z + other.y * self.y + other.x * self.x
    }

    /// The header's inline `Normalize`: a magnitude within 0.0001 of zero gives the zero vector.
    #[inline]
    pub fn normalize(&mut self) {
        let magnitude = self.magnitude();
        if (-0.0001..=0.0001).contains(&magnitude) {
            *self = GeVector::default();
        } else {
            let scale = 1.0 / magnitude;
            self.x *= scale;
            self.y *= scale;
            self.z *= scale;
        }
    }

    /// `CrossProduct` into a separate vector (see `cross_product_raw` for the C++'s in-place behaviour).
    #[inline]
    pub fn cross(self, other: GeVector) -> GeVector {
        GeVector {
            x: self.y * other.z - other.y * self.z,
            y: other.x * self.z - self.x * other.z,
            z: other.y * self.x - other.x * self.y,
        }
    }
}

/// `GEVector::CrossProduct(in, out)` exactly as the C++ runs it: each component is stored before the next is
/// computed, so an output that is one of the inputs changes the later components (the C++ reads through the
/// pointers again).
///
/// # Safety
/// The three pointers are valid; any of them may be equal.
pub unsafe fn cross_product_raw(this: *const GeVector, other: *const GeVector, out: *mut GeVector) {
    // SAFETY: the function's contract; every access goes through the raw pointers, in the C++'s order
    unsafe {
        (*out).x = (*this).y * (*other).z - (*other).y * (*this).z;
        (*out).y = (*other).x * (*this).z - (*this).x * (*other).z;
        (*out).z = (*other).y * (*this).x - (*other).x * (*this).y;
    }
}

/// Fills `QuantizedVectorArray::mVectors` (row-major `[41][246]`): each cube face sampled on a 41 x 41 grid of
/// normalized directions, faces -X, -Y, -Z at columns 0, 41, 82 and +X, +Y, +Z at 123, 164, 205.
pub fn build_quantized_table(out: &mut [GeVector]) {
    assert_eq!(out.len(), QUANT_COUNT);
    let at = |row: usize, face: usize, column: usize| row * QUANT_COLUMNS + face * 41 + column;
    for y in 0..41 {
        let first = y as f32 * QUANT_STEP - 1.0;
        for z in 0..41 {
            let mut v = GeVector::new(-1.0, first, z as f32 * QUANT_STEP - 1.0);
            v.normalize();
            out[at(y, 0, z)] = v;
            v.x = -v.x;
            out[at(y, 3, z)] = v;
        }
    }
    for x in 0..41 {
        let first = x as f32 * QUANT_STEP - 1.0;
        for z in 0..41 {
            let mut v = GeVector::new(first, -1.0, z as f32 * QUANT_STEP - 1.0);
            v.normalize();
            out[at(x, 1, z)] = v;
            v.y = -v.y;
            out[at(x, 4, z)] = v;
        }
    }
    for x in 0..41 {
        let first = x as f32 * QUANT_STEP - 1.0;
        for y in 0..41 {
            let mut v = GeVector::new(first, y as f32 * QUANT_STEP - 1.0, -1.0);
            v.normalize();
            out[at(x, 2, y)] = v;
            v.z = -v.z;
            out[at(x, 5, y)] = v;
        }
    }
}

/// The six face planes (-X, -Y, -Z, +X, +Y, +Z), each through its face centre facing out.
const FACES: [Ge3DPlane; 6] = [
    Ge3DPlane { point: Ge3DPoint::new(-1.0, 0.0, 0.0), normal: GeVector::new(-1.0, 0.0, 0.0) },
    Ge3DPlane { point: Ge3DPoint::new(0.0, -1.0, 0.0), normal: GeVector::new(0.0, -1.0, 0.0) },
    Ge3DPlane { point: Ge3DPoint::new(0.0, 0.0, -1.0), normal: GeVector::new(0.0, 0.0, -1.0) },
    Ge3DPlane { point: Ge3DPoint::new(1.0, 0.0, 0.0), normal: GeVector::new(1.0, 0.0, 0.0) },
    Ge3DPlane { point: Ge3DPoint::new(0.0, 1.0, 0.0), normal: GeVector::new(0.0, 1.0, 0.0) },
    Ge3DPlane { point: Ge3DPoint::new(0.0, 0.0, 1.0), normal: GeVector::new(0.0, 0.0, 1.0) },
];

/// `GetQuantizedVersion` as a table index (row * 246 + column), `QUANT_ZERO` for (0, 0, 0), `QUANT_NONE` when no
/// face is hit. The faces are tried in the C++'s order: +Z, +Y, +X, -X, -Y, -Z, each only on the side the vector
/// points to (-Z always, as a last resort).
pub fn quantized_index(v: GeVector) -> i32 {
    if !(v.x != 0.0 || v.y != 0.0 || v.z != 0.0) {
        return QUANT_ZERO;
    }
    let line = Ge3DLine { start: Ge3DPoint::default(), direction: v };
    let (check_negative_x, check_positive_x) = if v.x < 0.0 { (true, false) } else { (false, true) };
    let (check_negative_y, check_positive_y) = if v.y < 0.0 { (true, false) } else { (false, true) };
    let check_positive_z = !(v.z < 0.0);
    // (face, which components give the grid coordinates, whether this face is tried)
    type Components = fn(GeVector) -> (f32, f32);
    let order: [(usize, Components, bool); 6] = [
        (5, |v| (v.x, v.y), check_positive_z),
        (4, |v| (v.x, v.z), check_positive_y),
        (3, |v| (v.y, v.z), check_positive_x),
        (0, |v| (v.y, v.z), check_negative_x),
        (1, |v| (v.x, v.z), check_negative_y),
        (2, |v| (v.x, v.y), true),
    ];
    for (face, components, tried) in order {
        if !tried {
            continue;
        }
        if let Some(t) = line.intersects_plane(&FACES[face]) {
            if t > 0.0 {
                let (a, b) = components(v);
                let first = t * a;
                let second = t * b;
                if (-1.0..=1.0).contains(&first) && (-1.0..=1.0).contains(&second) {
                    let first = (first + 1.0) * 0.5 * QUANT_SIZE;
                    let second = (second + 1.0) * 0.5 * QUANT_SIZE;
                    return lrintf(first) * QUANT_COLUMNS as i32 + face as i32 * 41 + lrintf(second);
                }
            }
        }
    }
    QUANT_NONE
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn normalize_zeroes_a_tiny_vector_and_scales_the_rest() {
        let mut v = GeVector::new(0.00001, 0.0, 0.0);
        v.normalize();
        assert_eq!(v, GeVector::default());
        let mut v = GeVector::new(3.0, 4.0, 0.0);
        v.normalize();
        assert_eq!(v, GeVector::new(0.6, 0.8, 0.0));
    }

    #[test]
    fn cross_product_of_the_axes() {
        let x = GeVector::new(1.0, 0.0, 0.0);
        let y = GeVector::new(0.0, 1.0, 0.0);
        assert_eq!(x.cross(y), GeVector::new(0.0, 0.0, 1.0));
        assert_eq!(y.cross(x), GeVector::new(0.0, 0.0, -1.0));
    }

    #[test]
    fn cross_product_in_place_reads_the_stored_components() {
        // out == this: x is stored first, then y reads the new x
        let mut a = GeVector::new(1.0, 2.0, 3.0);
        let b = GeVector::new(4.0, 5.0, 6.0);
        let p: *mut GeVector = &mut a;
        // SAFETY: valid pointers (two of them equal, which the function allows)
        unsafe { cross_product_raw(p, &b, p) };
        let x = 2.0 * 6.0 - 5.0 * 3.0; // -3
        let y = 4.0 * 3.0 - x * 6.0; // the stored x
        assert_eq!(a.x, x);
        assert_eq!(a.y, y);
        assert_ne!(a, GeVector::new(1.0, 2.0, 3.0).cross(b));
    }

    #[test]
    fn quantized_table_corners() {
        let mut table = vec![GeVector::default(); QUANT_COUNT];
        build_quantized_table(&mut table);
        let c = 1.0 / 3f32.sqrt();
        // the -X face's first entry is (-1, -1, -1) normalized
        assert!((table[0].x + c).abs() < 1e-6 && (table[0].y + c).abs() < 1e-6 && (table[0].z + c).abs() < 1e-6);
        // the +Z face centre is (0, 0, 1)
        assert_eq!(table[20 * QUANT_COLUMNS + 5 * 41 + 20], GeVector::new(0.0, 0.0, 1.0));
    }

    #[test]
    fn quantized_index_of_the_axes_and_zero() {
        assert_eq!(quantized_index(GeVector::default()), QUANT_ZERO);
        assert_eq!(quantized_index(GeVector::new(0.0, 0.0, 2.0)), (20 * QUANT_COLUMNS + 5 * 41 + 20) as i32);
        assert_eq!(quantized_index(GeVector::new(-1.0, -1.0, -1.0)), 0);
        assert_eq!(quantized_index(GeVector::new(f32::NAN, 1.0, 0.0)), QUANT_NONE);
    }
}
