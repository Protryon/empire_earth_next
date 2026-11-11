//! The tech tree, the event effects and the calamities' timing (docs/RUST_PLAN.md R5b.2).
//!
//! The C++ classes keep their members and object lifetimes (`EETechTree`, `EETechTreeEpoch`, `EETechTreeNode` in
//! `ee/EEPlayer.h`, `EEEvent`, the calamity classes); they hand plain views to these functions and apply what comes
//! back, so every decision and every piece of arithmetic here is the C++'s, in its order (RUST_PLAN 2.1: `f32` in the
//! same order, wrapping integers, `lrintf` and the C cast's x86-64 result, never `as`).
//!
//! - [`effects`]: `EEApplyEventEffect` (the float and the integer forms), the gather rates, the hit point and flight
//!   time rescale, the terrain masks, which stat an attribute changes, and `RunEffect`'s dispatch.
//! - [`tree`]: a node's and an epoch's availability, prerequisites, research decisions, costs and research counters
//!   toward the next epoch; the tree's checksum, `DependsOn` and `SumCosts` over a view of the tree.
//! - [`calamity`]: a calamity's age against its class's lifetime.
//! - [`calamity_cast`]: `CanCastAt` and the classes' target and location tests.
//! - [`calamity_update`]: every calamity class's own update over a host of world calls.
//! - [`capture`]: the simulation capture's records (`ee/EETechCapture.h`) and their replay.

pub mod calamity;
pub mod calamity_cast;
pub mod calamity_update;
pub mod capture;
pub mod effects;
pub mod ffi;
pub mod tree;

/// The C cast `(long)inValue` of a float as the x86-64 build compiles it (`cvttss2si`): toward zero, and the
/// conversion's "indefinite" value `i32::MIN` for NaN or a value out of range (`as i32` would saturate).
#[inline]
pub fn ftol(value: f32) -> i32 {
    if (-2_147_483_648.0..2_147_483_648.0).contains(&value) {
        value as i32
    } else {
        i32::MIN
    }
}

/// `UMCeil` (`LowLevelEngineCore.h`): `lrintf`, then one up when that is below the value (compared as a float).
#[inline]
pub fn um_ceil(value: f32) -> i32 {
    let result = ee_ge::um::lrintf(value);
    if (result as f32) < value {
        result.wrapping_add(1)
    } else {
        result
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn ftol_truncates_toward_zero_with_the_indefinite_value() {
        assert_eq!(ftol(2.9), 2);
        assert_eq!(ftol(-2.9), -2);
        assert_eq!(ftol(0.0001), 0);
        assert_eq!(ftol(f32::NAN), i32::MIN);
        assert_eq!(ftol(3.0e9), i32::MIN);
        assert_eq!(ftol(-3.0e9), i32::MIN);
        assert_eq!(ftol(-2_147_483_648.0), i32::MIN);
        assert_eq!(ftol(2_147_483_520.0), 2_147_483_520);
    }

    #[test]
    fn um_ceil_rounds_up_from_the_nearest() {
        assert_eq!(um_ceil(2.0), 2);
        assert_eq!(um_ceil(2.1), 3);
        assert_eq!(um_ceil(2.5), 3); // lrintf gives 2 (half to even), below 2.5: one up
        assert_eq!(um_ceil(3.5), 4); // lrintf gives 4
        assert_eq!(um_ceil(-2.5), -2);
        assert_eq!(um_ceil(-2.1), -2);
        assert_eq!(um_ceil(0.0), 0);
    }
}
