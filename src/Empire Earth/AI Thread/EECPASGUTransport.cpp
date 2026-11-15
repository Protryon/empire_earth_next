// C:\EEX\Empire Earth\AI Thread\EECPASGUTransport.cpp (file name inferred: no __FILE__). 0xd5d8..0xd6e3
// (EEAIArea's AddTiles starts at 0xd6e3)
#include <windows.h>
#include <vector>
#include <set>

#include "lle/LowLevelEngine.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSRestorable.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGRust.h"
#include "ee/EECPILocation.h"
#include "ee/EECPASTargetUnit.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EECPAttackSubGroupUnit.h"
#include "ee/EECPASGUTransport.h"

// Creates the transport entry for a unit in the sub-group
EECPASGUTransport::EECPASGUTransport(EEComplexUnit* inUnit, EECPAttackSubGroup* inSubGroup)
    : EECPAttackSubGroupUnit(inUnit, inSubGroup)
{
    this->mType = kASGUKindTransport;
}

// Creates an empty transport entry for the sub-group (no unit yet)
EECPASGUTransport::EECPASGUTransport(EECPAttackSubGroup* inSubGroup) : EECPAttackSubGroupUnit(inSubGroup)
{
    this->mType = kASGUKindTransport;
}

// Destroys the transport entry (base members only)
EECPASGUTransport::~EECPASGUTransport()
{
}

// Switches state the plain sub-group-unit way
void EECPASGUTransport::SetState(long inState, unsigned long inTime)
{
    EECPAttackSubGroupUnit::SetState(inState, inTime);
}

// Returns true below 85% hit points if the transport was attacked or any sub-group target is attacking it
bool EECPASGUTransport::IsInDanger()
{
    bool theResult = false;
    if ((float)this->mUnit->mHitPoints / this->mUnit->mStats->mMaxHitPoints < 0.85f) {   // a float ratio (was integer division)
        theResult = this->mUnit->mLastAttacker != NULL;
        if (!theResult) {
            for (long i = this->mSubGroup->mTargets.size() - 1; i >= 0 && !theResult; i--) {
                EEUnitGoal* theGoal = this->mSubGroup->mTargets[i]->mUnit->mGoalState;
                if (theGoal != NULL && theGoal->mKind == kGoalAttackUnit)
                    theResult = static_cast<EEUGRust*>(theGoal)->GetAttackTarget() == this->mUnit;
            }
        }
    }
    return theResult;
}

// Tests a condition the plain sub-group-unit way
bool EECPASGUTransport::CheckCondition(long inState, unsigned long inTime)
{
    return EECPAttackSubGroupUnit::CheckCondition(inState, inTime);
}


// Never retargets: a transport returns false
bool EECPASGUTransport::Retarget()
{
    return false;
}

// Tells that a transport can attack no target (always false)
bool EECPASGUTransport::CanAttack(EECPASTargetUnit*)
{
    return false;
}

// Updates the transport the plain sub-group-unit way
void EECPASGUTransport::Update(unsigned long inTime)
{
    EECPAttackSubGroupUnit::Update(inTime);
}
