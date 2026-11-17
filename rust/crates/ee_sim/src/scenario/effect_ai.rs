//! The AI effects that steered a computer player's military (`EESMEffect::BuildObject`, `CreateAIMilitaryGroup`,
//! `ModifyAIMilitaryTarget`). They raised or aimed the C++ classic AI's attack groups, which main's stage 5 deleted
//! with the C++ combat tree (the Lua AIs command their own soldiers; main's docs/AI_RESEARCH.md section 8.90): the
//! effects do nothing now, as on main. Their world calls (`EE_SCENARIO_WORLD_AI_*` 100..130, 185) are gone with them.

use crate::scenario::fields::EeScenarioEffectFields;
use crate::scenario::host::Host;

/// Build Object: nothing (the computer players' C++ attack groups are gone).
pub fn build_object<H: Host>(_h: &mut H, _e: &EeScenarioEffectFields) {}

/// Create AI Military Group: nothing (the computer players' C++ attack groups are gone).
pub fn create_ai_military_group<H: Host>(_h: &mut H, _e: &EeScenarioEffectFields) {}

/// Modify AI Military Target: nothing (the computer players' C++ attack groups are gone).
pub fn modify_ai_military_target<H: Host>(_h: &mut H, _e: &EeScenarioEffectFields) {}
