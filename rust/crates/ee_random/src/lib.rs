//! `URandom` and `UMFloatToLong`: the engine's linear congruential generator with 32-bit state, and `lrintf`.
//!
//! The sequence must equal the C++ bit for bit (RUST_PLAN 2.1): `Generate` divides in `f64`, and the integer form
//! rounds with `lrintf` (round half to even).

/// The LCG constants of the C++ `URandom`.
const MULTIPLIER: u32 = 0x343fd;
const MODULUS: u32 = 0x8000_0000;
const INCREMENT: u32 = 0x269ec3;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct URandom {
    /// The current state, exposed for the plug-ins that carry the seed across the FFI seam.
    pub seed: u32,
}

impl URandom {
    /// A generator; a seed of 0 becomes 1, as the C++ constructor does (`SetSeed` has no such fix-up).
    pub fn new(seed: u32) -> Self {
        URandom { seed: if seed == 0 { 1 } else { seed } }
    }

    /// The next value of the series.
    pub fn next_in_series(&mut self) -> u32 {
        self.seed = self.seed.wrapping_mul(MULTIPLIER).wrapping_add(INCREMENT) % MODULUS;
        self.seed
    }

    /// `URandom::Generate()`: the next value in [0, 1).
    pub fn generate(&mut self) -> f32 {
        (f64::from(self.next_in_series()) / f64::from(MODULUS)) as f32
    }

    /// `URandom::Generate(min, max)`.
    pub fn generate_range(&mut self, min: f32, max: f32) -> f32 {
        self.generate() * (max - min) + min
    }

    /// `URandomIntegers::Generate`: integers in `min..=max` (the C++ takes `long`s).
    pub fn generate_int(&mut self, min: i32, max: i32) -> i32 {
        let limit = max.wrapping_add(1);
        let value = self.generate() * limit.wrapping_sub(min) as f32 + min as f32;
        let result = float_to_long(value);
        if result == limit {
            max
        } else {
            result
        }
    }
}

/// `UMFloatToLong`: `lrintf` in the default rounding mode (round half to even). Out-of-range values are the x86
/// conversion's "indefinite" value on Windows, but these never occur in the generator's arithmetic.
pub fn float_to_long(value: f32) -> i32 {
    value.round_ties_even() as i32
}

#[cfg(test)]
mod tests {
    use super::*;

    const GOLDEN: &str = include_str!("golden.txt");

    fn rows(tag: &str) -> Vec<Vec<&'static str>> {
        GOLDEN.lines().map(|l| l.split('\t').collect::<Vec<_>>()).filter(|t| t[0] == tag).collect()
    }

    #[test]
    fn generate_matches_the_cpp_bit_for_bit() {
        for t in rows("random") {
            let mut r = URandom::new(t[1].parse().unwrap());
            let mut hex = String::new();
            for _ in 0..20 {
                hex.push_str(&format!("{:08x}", r.generate().to_bits()));
            }
            assert_eq!(hex, t[2], "{t:?}");
            assert_eq!(r.seed.to_string(), t[3], "{t:?}");
        }
    }

    #[test]
    fn series_matches_the_cpp() {
        for t in rows("series") {
            let mut r = URandom::new(t[1].parse().unwrap());
            let hex: String = (0..20).map(|_| format!("{:08x}", r.next_in_series())).collect();
            assert_eq!(hex, t[2], "{t:?}");
        }
    }

    #[test]
    fn integers_match_the_cpp() {
        for t in rows("integers") {
            let seed: u32 = t[1].parse().unwrap();
            let mut r = URandom { seed: if seed == 0 { 1 } else { seed } };
            let values: String = (0..20).map(|_| format!("{},", r.generate_int(-5, 9))).collect();
            assert_eq!(values, t[2], "{t:?}");
        }
    }

    #[test]
    fn ranges_match_the_cpp() {
        for t in rows("range") {
            let mut r = URandom::new(t[1].parse().unwrap());
            let hex: String = (0..10).map(|_| format!("{:08x}", r.generate_range(2.5, 7.0).to_bits())).collect();
            assert_eq!(hex, t[2], "{t:?}");
        }
    }

    #[test]
    fn packet_keys_match_the_cpp() {
        for t in rows("keys") {
            let mut r = URandom::new(777);
            let keys: String = (0..30).map(|_| format!("{},", (r.generate() * 254.0 + 1.0) as u32 as u8)).collect();
            assert_eq!(keys, t[1], "{t:?}");
        }
    }

    #[test]
    fn seed_zero_is_one_but_set_seed_is_raw() {
        assert_eq!(URandom::new(0).seed, 1);
        let mut raw = URandom { seed: 0 };
        // the series from a raw 0 state is the C++'s too: no fix-up
        assert_eq!(raw.next_in_series(), 0x269ec3);
    }

    #[test]
    fn lrintf_rounds_half_to_even() {
        assert_eq!(float_to_long(0.5), 0);
        assert_eq!(float_to_long(1.5), 2);
        assert_eq!(float_to_long(2.5), 2);
        assert_eq!(float_to_long(-0.5), 0);
        assert_eq!(float_to_long(-1.5), -2);
        assert_eq!(float_to_long(2.4999), 2);
    }

    #[test]
    fn generate_int_clamps_the_top_edge() {
        let mut r = URandom::new(1);
        for _ in 0..1000 {
            let v = r.generate_int(3, 5);
            assert!((3..=5).contains(&v));
        }
    }
}
