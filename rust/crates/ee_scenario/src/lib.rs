//! The scenario manager's simulation logic (docs/RUST_PLAN.md R5b.5).
//!
//! The C++ classes of `src/Empire Earth/Scenario Manager` keep their members, object lifetimes and saves
//! (`EEScenarioManager`, `EESMTrigger`, `EESMCondition`, `EESMObject`, `EESMArea`, `EESMEffect`); they hand plain
//! records to these functions and read the world through one host table ([`host`]), so every decision here is the
//! C++'s, in its order (RUST_PLAN 2.1: the same reads and random draws in the same order, `f32` arithmetic as the
//! C++ writes it, the C cast's x86-64 result, wrapping integers).
//!
//! - [`host`]: the world's reads by operation code, and the C ABI's host table.
//! - [`condition`]: `EESMCondition::Compare`, `Evaluate` and `GetPlayerValue`, `EEScenarioManager::ChoosePlayer`
//!   with the AI's shuffle.
//! - [`trigger`]: `EESMTrigger::Evaluate` with its condition chain (`EESMTConditionWrapper::Evaluate`), duration
//!   and the timed effects it schedules.
//! - [`ffi`]: the C ABI the C++ classes call.

pub mod condition;
pub mod ffi;
pub mod host;
pub mod trigger;

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

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn ftol_truncates_toward_zero_with_the_indefinite_value() {
        assert_eq!(ftol(2.9), 2);
        assert_eq!(ftol(-2.9), -2);
        assert_eq!(ftol(-0.5), 0);
        assert_eq!(ftol(f32::NAN), i32::MIN);
        assert_eq!(ftol(3.0e9), i32::MIN);
        assert_eq!(ftol(-2_147_483_648.0), i32::MIN);
    }
}
