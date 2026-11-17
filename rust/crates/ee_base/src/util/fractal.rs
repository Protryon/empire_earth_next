//! `U2DFractal` whole in Rust (LLE lane round 7): a square height map filled by diamond-square subdivision. The
//! random map's `PaintTiles` and the terrain mesh's detail noise use it, so it is simulation code (RUST_PLAN 2.1):
//! the C++'s float operations in order, its `URandom` stream seeded raw (`theRandom.mSeed = inSeed`, no 0 -> 1
//! fix-up) and drawn once per value, its clamp's comparisons.
//!
//! The C++ class holds one [`EeUtilFractal`] pointer. Its values were `new float[]` rows the computation does not
//! all reach for a dimension that is not a power of two (and it wrote past them); here they start at 0 and an index
//! past the map panics. The game's dimensions are powers of two (the random map's grid sizes, the mesh's 64).

use crate::random::URandom;
use std::ffi::c_ulong;

/// A `U2DFractal`: `dimension` x `dimension` values, column `x` first as the C++'s `mValues[x][y]`.
#[derive(Clone, Debug, PartialEq)]
pub struct EeUtilFractal {
    dimension: u32,
    values: Vec<f32>,
}

impl EeUtilFractal {
    pub fn new(dimension: u32) -> Self {
        let d = dimension as usize;
        EeUtilFractal { dimension, values: vec![0.0; d * d] }
    }

    fn at(&self, x: u32, y: u32) -> usize {
        assert!(x < self.dimension && y < self.dimension, "U2DFractal index ({x}, {y}) past {}", self.dimension);
        x as usize * self.dimension as usize + y as usize
    }

    pub fn value(&self, x: u32, y: u32) -> f32 {
        self.values[self.at(x, y)]
    }

    fn set(&mut self, x: u32, y: u32, value: f32) {
        let i = self.at(x, y);
        self.values[i] = value;
    }

    /// `GenerateFractalValue`: the average plus a random offset within the scaled range, clamped to 0..1 (the C++'s
    /// `min(max(v, 0), 1)` macros: a NaN becomes 0).
    fn generate(random: &mut URandom, range: f32, scale: f32, average: f32) -> f32 {
        let value = (range - random.generate() * (range + range)) * scale + average;
        let value = if value > 0.0 { value } else { 0.0 };
        if value < 1.0 {
            value
        } else {
            1.0
        }
    }

    /// `ComputeFractal`: diamond-square subdivision wrapping at the edges, the random range shrinking with the step.
    pub fn compute(&mut self, corner: f32, range: f32, seed: u32) {
        let mut random = URandom { seed };
        let dimension = self.dimension;
        let mut step = dimension;
        self.set(0, 0, corner);
        if step <= 1 {
            return;
        }
        let the_dimension = step as f32;
        loop {
            let scale = step as f32 / the_dimension;
            let half = step / 2;
            let mut y = 0u32;
            while y < dimension {
                let mut x = 0u32;
                while x < dimension {
                    let next_x = (x + step) % dimension;
                    let next_y = (y + step) % dimension;
                    let average =
                        (self.value(x, y) + self.value(next_x, y) + self.value(x, next_y) + self.value(next_x, next_y))
                            * 0.25;
                    let v = Self::generate(&mut random, range, scale, average);
                    self.set(x + half, y + half, v);
                    x += step;
                }
                y += step;
            }
            let mut y = 0u32;
            while y < dimension {
                let mut x = 0u32;
                while x < dimension {
                    let next_x = (x + step) % dimension;
                    let previous_x = x.wrapping_sub(half) % dimension;
                    let next_y = (y + step) % dimension;
                    let average = (self.value(previous_x, y + half)
                        + self.value(x, next_y)
                        + self.value(x + half, y + half)
                        + self.value(x, y))
                        * 0.25;
                    let v = Self::generate(&mut random, range, scale, average);
                    self.set(x, y + half, v);
                    let previous_y = y.wrapping_sub(half) % dimension;
                    let average = (self.value(x + half, previous_y)
                        + self.value(next_x, y)
                        + self.value(x, y)
                        + self.value(x + half, y + half))
                        * 0.25;
                    let v = Self::generate(&mut random, range, scale, average);
                    self.set(x + half, y, v);
                    x += step;
                }
                y += step;
            }
            step /= 2;
            if step <= 1 {
                break;
            }
        }
    }
}

// ---- the C interface (lle/U2DFractal.h)

/// `U2DFractal(dimension)`: an uncomputed map (all 0).
#[no_mangle]
pub extern "C" fn ee_util_fractal_new(dimension: c_ulong) -> *mut EeUtilFractal {
    Box::into_raw(Box::new(EeUtilFractal::new(dimension as u32)))
}

/// The destructor.
///
/// # Safety
/// `fractal` came from `ee_util_fractal_new` and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_util_fractal_free(fractal: *mut EeUtilFractal) {
    if !fractal.is_null() {
        drop(Box::from_raw(fractal));
    }
}

/// `ComputeFractal`.
///
/// # Safety
/// `fractal` is a live map.
#[no_mangle]
pub unsafe extern "C" fn ee_util_fractal_compute(fractal: *mut EeUtilFractal, corner: f32, range: f32, seed: c_ulong) {
    (*fractal).compute(corner, range, seed as u32);
}

/// `GetValue`.
///
/// # Safety
/// `fractal` is a live map.
#[no_mangle]
pub unsafe extern "C" fn ee_util_fractal_value(fractal: *const EeUtilFractal, x: c_ulong, y: c_ulong) -> f32 {
    (*fractal).value(x as u32, y as u32)
}

/// `GetSquareDimension`.
///
/// # Safety
/// `fractal` is a live map.
#[no_mangle]
pub unsafe extern "C" fn ee_util_fractal_dimension(fractal: *const EeUtilFractal) -> c_ulong {
    (*fractal).dimension as c_ulong
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn small_maps() {
        let mut f = EeUtilFractal::new(1);
        f.compute(0.5, 1.0, 7);
        assert_eq!(f.value(0, 0), 0.5);
        let mut f = EeUtilFractal::new(2);
        f.compute(0.25, 0.0, 7); // no range: every value is the average of its (wrapped) neighbours
        assert_eq!(f.values, vec![0.25; 4]);
        for v in &f.values {
            assert!((0.0..=1.0).contains(v));
        }
    }

    #[test]
    fn clamp_and_seed() {
        let mut r = URandom { seed: 0 };
        // seed 0 is kept raw: the first draw is the increment over the modulus
        let v = EeUtilFractal::generate(&mut r, 10.0, 1.0, 0.5);
        assert!(v == 0.0 || v == 1.0);
        assert_eq!(EeUtilFractal::generate(&mut URandom { seed: 1 }, f32::NAN, 1.0, 0.0), 0.0);
    }
}
