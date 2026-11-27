//! The records C++ classes derive from or embed (the data records pass, docs/progress/headers.md "How to add a
//! record"). Their C declarations, and those of the types they name, go to `include/rust/game/records.h`
//! (xtask split.rs), a header that declares no function, so a class header that includes only it is not rebuilt
//! by a change to a function. A record is listed here by a `pub use` of its definition, or defined here.

pub use crate::app::ambient::{EeAppAmbientCounts, EeAppAmbientFields, EeAppAmbientSoundFields};
pub use crate::app::camera::EeAppCameraFields;
pub use crate::app::game_options::{EeAppOptionsFields, EeAppOptionsPlayerFields};
pub use crate::app::settings::EeAppSettingsFields;
pub use crate::civilization::EeGameCivFields;
pub use crate::rts::graphics::EeRtsGraphicsFields;
pub use crate::rts::mouse::EeRtsMouseFields;
pub use crate::rts::physics::EeRtsPhysicsFields;
pub use crate::rts::units::EeRtsUnitBucket;
pub use crate::screens::command_units::EeScreensCommandUnitsFields;
pub use crate::screens::game_actions::EeScreensGaFields;
pub use crate::screens::hud_direct::EeScreensFormCallObjects;
pub use crate::screens::hud_fields::{
    EeScreensAttributeFields, EeScreensBackgroundFields, EeScreensCommandUnitsObjects, EeScreensManagerCallObjects,
    EeScreensMiniMapFields, EeScreensPortraitFields, EeScreensSciFormFields, EeScreensSelectUnitsObjects,
    EeScreensStatusBarFields, EeScreensStatusBoxCallObjects, EeScreensStatusButtonObjects, EeScreensUnitHackFields,
    EeScreensUpgradeCallObjects, EeScreensUpgradeFields,
};
pub use crate::screens::main_form_fields::{
    EeScreensMainFormFields, EeScreensMainFormObjects, EeScreensMainGroupFields, EeScreensStatusBoxFields,
    EeScreensStatusButtonFields,
};
pub use crate::screens::ping::EeScreensPingFields;
pub use crate::screens::select_units::EeScreensSelectUnitsFields;
pub use crate::screens::ui_objects::{EeScreensUiManagerFields, EeScreensUserInterfaceFields};
