// C:\EEX\Empire Earth\Formations\EEFormation.cpp (file and folder names inferred: no __FILE__). The formation:
// 0xdd160..0xde331 (the base's Create and Arrange, and the eight classes the original had, EEFCircle.cpp to
// EEFWedge.cpp, folded into this one unit: the units' lane, round 2; ee/EEFormation.h).
// Unit flags: the exe's module set.
#include <windows.h>
#include <algorithm>

#include "lle/LowLevelEngine.h"

#include "ee/RTSWorld.h"
#include "ee/EEPlayer.h"

#include "ee/EEFormation.h"
#include "rust/sim/formations.h"

// The group's units, then the units', placed into formation slots: a circle appends to outSlots, the other types
// replace it; one slot per unit (ee_sim formations units.rs: the radii, the shapes, the capture's record)
void EEFormation::Arrange(EEGroup* inGroup, std::vector<EEFormationSlot>& outSlots)
{
    outSlots.resize((this->IsCircle() ? outSlots.size() : 0) + inGroup->GetNumUnits());
    ee_formation_arrange_group((int32_t)this->mType, this->mMinRadius, &this->mOrientation.mMatrix, reinterpret_cast<const EeFormationVec3*>(&this->mCenter.mX), this->mHeading,
                               inGroup, reinterpret_cast<EeFormationUnitSlot*>(outSlots.data() + outSlots.size() - inGroup->GetNumUnits()));
}

void EEFormation::Place(std::vector<EEComplexUnit*>& inUnits, std::vector<EEFormationSlot>& outSlots)
{
    outSlots.resize((this->IsCircle() ? outSlots.size() : 0) + inUnits.size());
    ee_formation_place_units((int32_t)this->mType, this->mMinRadius, &this->mOrientation.mMatrix, reinterpret_cast<const EeFormationVec3*>(&this->mCenter.mX), this->mHeading,
                             reinterpret_cast<void* const*>(inUnits.data()), inUnits.size(),
                             reinterpret_cast<EeFormationUnitSlot*>(outSlots.data() + outSlots.size() - inUnits.size()));
}

static_assert(sizeof(EEFormationSlot) == sizeof(EeFormationUnitSlot) && offsetof(EEFormationSlot, mUnit) == offsetof(EeFormationUnitSlot, unit)
              && offsetof(EEFormationSlot, mHeading) == offsetof(EeFormationUnitSlot, heading), "ee_sim formations units.rs");
