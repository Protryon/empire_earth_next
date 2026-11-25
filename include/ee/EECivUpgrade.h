// ee/EECivUpgrade.h (new): a civilization builder upgrade id, a category and a level, (category << 16) | level. The
// original packed them in an unsigned short as (category << 6) | level (35 categories, 60 levels); the legacy .civ
// files and the premade civilizations' checksums keep that form (EECivilizationManager::FromLegacy / ToLegacy).
#ifndef EE_EECIVUPGRADE_H
#define EE_EECIVUPGRADE_H

// EECivUpgrade, kCivUpgradeLevelBits and kCivUpgradeLevelMask: Rust's (ee_sim tech_consts.rs)
#include "rust/sim/tech_consts.h"

#endif
