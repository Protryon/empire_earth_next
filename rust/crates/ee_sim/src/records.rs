//! The records C++ classes derive from or embed (the data records pass, docs/progress/headers.md "How to add a
//! record"). Their C declarations, and those of the types they name, go to `include/rust/sim/records.h`
//! (xtask split.rs), a header that declares no function, so a class header that includes only it is not rebuilt
//! by a change to a function. A record is listed here by a `pub use` of its definition, or defined here.

pub use crate::app::storm::EeAppStormFields;
pub use crate::app::voting::EeAppVoting;
pub use crate::astar::area::EeAStarAreaRecord;
pub use crate::astar::area_kinds::{
    EeAStarChokeRecord, EeAStarForestRecord, EeAStarHillRecord, EeAStarPathAreaRecord, EeAStarRectRecord,
};
pub use crate::astar::cm::EeAStarCmRecord;
pub use crate::astar::continent::EeAStarContinentRecord;
pub use crate::astar::sector_rec::{EeAStarSectorRecord, EeAStarSubSectorRecord};
pub use crate::cp::fields::EeCpFields;
pub use crate::cp::herd::EeHerdFields;
pub use crate::cp::town::EeTownFields;
pub use crate::scenario::area_fields::EeScenarioAreaFields;
pub use crate::scenario::fields::{
    EeScenarioConditionFields, EeScenarioEffectFields, EeScenarioMessageFields, EeScenarioObjectFields,
    EeScenarioTriggerFields,
};
pub use crate::scenario::manager::EeScenarioManagerFields;
pub use crate::tech::area_effect::{EeAreaEffectFields, EeAreaEffectTemplateFields};
pub use crate::tech::calamity_class::EeCalamityClassFields;
pub use crate::tech::calamity_record::EeCalamityFields;
pub use crate::tech::contagion_functor::EeContagionFunctorFields;
pub use crate::tech::nodes::{EeTechNodeFields, EeTechTreeFields};
pub use crate::units::goal_obj::EeUnitsGoalBase;
pub use crate::units::group_obj::EeUnitsGroup;
pub use crate::units::obj_fields::{EeUnitsComplexDopFields, EeUnitsGhostFields};
pub use crate::units::physics_save::EeUnitsPhysicsSaved;
pub use crate::world::map_fields::{EeUnitsMapFields, EeUnitsRtsMapFields};
pub use crate::world::player_fields::{EeUnitsPlayerFields, EeUnitsPlayerSaved, EeUnitsRtsPlayerFields};
pub use crate::world::stats_fields::{EeUnitsUnitClassUpgradeFields, EeUnitsUnitClassUpgradeLinks};
pub use crate::world::unit_class_fields::{EeUnitsUnitClassFields, EeUnitsUnitClassLinks};
pub use crate::world::unit_fields::{
    EeUnitsComplexUnitFields, EeUnitsIgnoredTarget, EeUnitsProductionEntry, EeUnitsRtsUnitFields, EeUnitsUnitFields,
};
// types the class headers name in members and declarations (EEPlayer.h: its lists, events, groups, statistics)
pub use crate::units::atc::EeUnitsAtc;
pub use crate::units::group_obj::EeUnitsPlayerGroups;
pub use crate::units::patrol_group::EeUnitsPatrolGroup;
pub use crate::units::player::EeUnitsPlayerRefs;
pub use crate::units::player_events::{EeUnitsPlayerEvent, EeUnitsPlayerEvents};
pub use crate::units::player_lists::{EeUnitsOffMapInfo, EeUnitsPlayerLists};
pub use crate::units::player_save::EeUnitsPlayerSaveIo;
pub use crate::units::player_stats::{EeUnitsPlayerStats, EeUnitsStatPoint};
