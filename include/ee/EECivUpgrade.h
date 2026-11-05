// ee/EECivUpgrade.h (new): a civilization builder upgrade id, a category and a level, (category << 16) | level. The
// original packed them in an unsigned short as (category << 6) | level (35 categories, 60 levels); the legacy .civ
// files and the premade civilizations' checksums keep that form (EECivilizationManager::FromLegacy / ToLegacy).
#ifndef EE_EECIVUPGRADE_H
#define EE_EECIVUPGRADE_H

#include <stdint.h>

typedef uint32_t EECivUpgrade;
static const unsigned long kCivUpgradeLevelBits = 16;
static const unsigned long kCivUpgradeLevelMask = 0xffff;

#endif
