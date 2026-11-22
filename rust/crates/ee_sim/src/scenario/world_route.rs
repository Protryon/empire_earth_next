//! The world channel's routing (once the range tests at the head of `ee_scenariocpp_host_world`, Scenario
//! Manager/EESMEffect.cpp): which C++ statement host answers an `EE_SCENARIO_WORLD_*`, `WX`, `WC`, `WK`, `RX`, `WY`
//! or `WZ` operation, in the C++'s order of tests. entry.rs's host table calls the host [`route`] names; each C++ host
//! is its own `extern "C"` function over its typed context. Also the effect run's lifetime operations (the run is the
//! C++'s `EESMEffectRun`, made and freed by the Rust that runs an effect or a Create Units) and the float bits the
//! scenario channels carry, and the packed points.

use std::os::raw::c_long;

use crate::scenario::ai_store::EE_SCENARIO_WK_AI_NEXT_ATTACK_NOW;
use crate::scenario::ca_compose::EE_SCENARIO_WK_PLAYER;
use crate::scenario::effect_text::{EE_SCENARIO_WY_CU_KILL_UNIT, EE_SCENARIO_WY_EFFECT_FIELDS};
use crate::scenario::object_reads::{EE_SCENARIO_WZ_TILE_Y, EE_SCENARIO_WZ_UNIT};
use crate::scenario::read_compose::{EE_SCENARIO_RX_EFFECT_COPY, EE_SCENARIO_RX_TECH_NODE};
use crate::scenario::unit_change::{EE_SCENARIO_WORLD_CU_RANDOM, EE_SCENARIO_WORLD_CU_SET};
use crate::scenario::world_compose::{EE_SCENARIO_WC_CLASS, EE_SCENARIO_WC_SOUND_PLAY, EE_SCENARIO_WX_DETACH};

// ---- the effect run (answered by `ee_scenariocpp_host_world`)
/// `new EESMEffectRun((EESMEffect*)a)` (the context is unused).
pub const EE_SCENARIO_WORLD_RUN_NEW: u32 = 3000;
/// `delete inRun`.
pub const EE_SCENARIO_WORLD_RUN_FREE: u32 = 3001;
/// `inRun->mObject = gWorld.mScenarioManager->GetScenarioObject(inRun->mEffect->mObject)` (Create Units' object).
pub const EE_SCENARIO_WORLD_RUN_OBJECT: u32 = 3002;
/// `inRun->mCreatedUnits.size()`.
pub const EE_SCENARIO_WORLD_RUN_CREATED: u32 = 3003;
/// `inRun->mObject->SetUnits(inRun->mCreatedUnits)`.
pub const EE_SCENARIO_WORLD_RUN_SET_UNITS: u32 = 3004;

/// The C++ host of a world operation.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(crate) enum EeScenarioWorldRoute {
    /// `ee_scenariocpp_world_cu`: EEChangeUnit's calls on the context's unit (no effect run).
    ChangeUnit,
    /// `ee_scenariocpp_world_class`: a class attribute's and Set AI Variable's calls (ca_compose.rs, ai_store.rs).
    Class,
    /// `ee_scenariocpp_world_read`: the host's reads (read_compose.rs).
    Read,
    /// `ee_scenariocpp_world_y`: the effect run's texts (effect_text.rs).
    Text,
    /// `ee_scenariocpp_world_object`: the object reads (object_reads.rs).
    Object,
    /// `ee_scenariocpp_world_create`: Create Units' unit (world_compose.rs create_unit, on Create Units' run).
    Create,
    /// `ee_scenariocpp_world_atom`: the effect run's single statements (world_compose.rs).
    Atom,
    /// `ee_scenariocpp_host_world`: the effect run's own calls and the run's lifetime.
    Run,
}

/// The host of operation `op`, tested in the C++'s order.
pub(crate) fn route(op: u32) -> EeScenarioWorldRoute {
    use EeScenarioWorldRoute as R;
    if (EE_SCENARIO_WORLD_CU_RANDOM..=EE_SCENARIO_WORLD_CU_SET).contains(&op) {
        R::ChangeUnit
    } else if (EE_SCENARIO_WK_PLAYER..=EE_SCENARIO_WK_AI_NEXT_ATTACK_NOW).contains(&op) {
        R::Class
    } else if (EE_SCENARIO_RX_TECH_NODE..=EE_SCENARIO_RX_EFFECT_COPY).contains(&op) {
        R::Read
    } else if (EE_SCENARIO_WY_EFFECT_FIELDS..=EE_SCENARIO_WY_CU_KILL_UNIT).contains(&op) {
        R::Text
    } else if (EE_SCENARIO_WZ_UNIT..=EE_SCENARIO_WZ_TILE_Y).contains(&op) {
        R::Object
    } else if (EE_SCENARIO_WC_CLASS..=EE_SCENARIO_WC_SOUND_PLAY).contains(&op) {
        R::Create
    } else if (EE_SCENARIO_WX_DETACH..EE_SCENARIO_WORLD_RUN_NEW).contains(&op) {
        R::Atom
    } else {
        R::Run
    }
}

/// A float's bits as the scenario channels carry them (the low 32 bits).
#[no_mangle]
pub extern "C" fn ee_scenario_f32_bits(value: f32) -> i64 {
    value.to_bits() as i64
}

/// The float whose bits are the low 32 bits of `bits`.
#[no_mangle]
pub extern "C" fn ee_scenario_bits_f32(bits: i64) -> f32 {
    f32::from_bits(bits as u32)
}

/// Two longs as one value: `x` in the low 32 bits, `y` in the high (the reads' points; area.rs unpack).
#[no_mangle]
pub extern "C" fn ee_scenario_pack_xy(x: c_long, y: c_long) -> i64 {
    (x as u32 as i64) | (((y as u32) as u64) << 32) as i64
}

#[cfg(test)]
mod tests {
    use super::EeScenarioWorldRoute as R;
    use super::*;
    use crate::scenario::effect::{EE_SCENARIO_WORLD_CAMERA_MOVE, EE_SCENARIO_WORLD_CREATE_UNITS};

    #[test]
    fn routes_follow_the_ranges() {
        assert_eq!(route(EE_SCENARIO_WORLD_CREATE_UNITS), R::Run);
        assert_eq!(route(EE_SCENARIO_WORLD_CAMERA_MOVE), R::Run);
        assert_eq!(route(EE_SCENARIO_WORLD_CU_RANDOM), R::ChangeUnit);
        assert_eq!(route(EE_SCENARIO_WORLD_CU_SET), R::ChangeUnit);
        assert_eq!(route(EE_SCENARIO_WORLD_CU_SET + 1), R::Run);
        assert_eq!(route(EE_SCENARIO_WK_PLAYER), R::Class);
        assert_eq!(route(EE_SCENARIO_WK_AI_NEXT_ATTACK_NOW), R::Class);
        assert_eq!(route(EE_SCENARIO_RX_TECH_NODE), R::Read);
        assert_eq!(route(EE_SCENARIO_RX_EFFECT_COPY), R::Read);
        assert_eq!(route(EE_SCENARIO_WY_EFFECT_FIELDS), R::Text);
        assert_eq!(route(EE_SCENARIO_WY_CU_KILL_UNIT), R::Text);
        assert_eq!(route(EE_SCENARIO_WZ_UNIT), R::Object);
        assert_eq!(route(EE_SCENARIO_WZ_TILE_Y), R::Object);
        assert_eq!(route(EE_SCENARIO_WC_CLASS), R::Create);
        assert_eq!(route(EE_SCENARIO_WC_SOUND_PLAY), R::Create);
        assert_eq!(route(EE_SCENARIO_WX_DETACH), R::Atom);
        // the gaps between the named ranges above 400 were the atoms' in the C++ (`inOp >= WX_DETACH`)
        assert_eq!(route(EE_SCENARIO_WC_SOUND_PLAY + 1), R::Atom);
        assert_eq!(route(EE_SCENARIO_WZ_TILE_Y + 1), R::Atom);
        assert_eq!(route(EE_SCENARIO_WORLD_RUN_NEW), R::Run);
        assert_eq!(route(EE_SCENARIO_WORLD_RUN_SET_UNITS), R::Run);
    }

    #[test]
    fn float_bits_round_trip() {
        assert_eq!(ee_scenario_f32_bits(1.0), 0x3f80_0000);
        assert_eq!(ee_scenario_f32_bits(-0.0), 0x8000_0000);
        assert_eq!(ee_scenario_bits_f32(0x3f80_0000), 1.0);
        // the high bits are ignored, as the C++'s (uint32_t) casts did
        assert_eq!(ee_scenario_bits_f32(0x1234_5678_4000_0000), 2.0);
        assert!(ee_scenario_bits_f32(ee_scenario_f32_bits(f32::NAN)).is_nan());
    }

    #[test]
    fn pack_puts_x_low_and_y_high() {
        assert_eq!(ee_scenario_pack_xy(3, 4), 0x4_0000_0003);
        assert_eq!(ee_scenario_pack_xy(-1, 0), 0xffff_ffff);
        assert_eq!(ee_scenario_pack_xy(0, -1), 0xffff_ffff_0000_0000u64 as i64);
    }
}
