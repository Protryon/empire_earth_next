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
//! - [`area`]: `EESMArea::ContainsTile`, `ContainsUnit` and `GetCenterTile` (rectangles, continents, towns).
//! - [`object`]: `EESMObject::AcceptsClass` and `PickUnitType` (the class filter by epoch, family, AI kind, set).
//! - [`objeval`]: `EESMObject::Evaluate`, the object's units rebuilt and filtered (the list the C++'s).
//! - [`trigger`]: `EESMTrigger::Evaluate` with its condition chain (`EESMTConditionWrapper::Evaluate`), duration
//!   and the timed effects it schedules.
//! - [`fields`]: the effect's, condition's and object's numbers as records the C++ classes derive from (their
//!   defaults and index upkeep); [`save`]: their `Load` and `Save` over [`stream`], the `FSFile` calls by operation.
//! - [`manager`]: the manager's own numbers as a record the C++ class derives from, its timed effects' queue, and
//!   its `Load` and `Save`.
//! - [`round`]: `EEScenarioManager::Update`, the clocks and the trigger round with the timed effects.
//! - [`create`]: the Create Units effect's counts, owners and classes (`EESMEffect::CreateUnits`).
//! - [`placement`]: where a Create effect places a unit (`EESMObject::FindPlacement`), `IsNearTile`, `IsNearUnit`.
//! - [`player`]: the Set Player Attribute effect as a plan of writes (`EESMEffect::SetPlayerAttribute`).
//! - [`attribute`]: a unit's value for an object's attribute filter (`EESMObject::GetUnitAttribute`) as a table.
//! - [`aivar`]: the Set AI Variable effect's table as data (`EESMEffect::SetAIVariable`).
//! - [`tokens`]: a message's tags expanded (`EESMEffect::ExpandTokens`).
//! - [`lists`]: the manager's element lists as the editor changes them (sort, new, move, delete, duplicate, the
//!   name lists) and the index upkeep after each change (the `Renumber*` and `Remap*` broadcasts).
//! - [`text`]: the trigger text file (the editor's export and import of the scenario's translatable texts).
//! - [`effect`]: a trigger effect's run (`EESMEffect::Execute` and its helpers) over the world's calls by
//!   operation; [`effect_ai`] the AI's military effects, [`task`] the two task effects.
//! - [`ffi`]: the C ABI the C++ classes call.

pub mod aivar;
pub mod area;
pub mod attribute;
pub mod condition;
pub mod create;
pub mod effect;
pub mod effect_ai;
pub mod ffi;
pub mod fields;
pub mod host;
pub mod lists;
pub mod manager;
pub mod object;
pub mod objeval;
pub mod placement;
pub mod player;
pub mod round;
pub mod save;
pub mod stream;
pub mod task;
pub mod text;
pub mod tokens;
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
