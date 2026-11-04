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
#include "ee/EEUGAttackUnit.h"
#include "ee/EECPILocation.h"
#include "ee/EECPASTargetUnit.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EECPAttackSubGroupUnit.h"
#include "ee/EECPASGUTransport.h"

// FUNCTION: EXE 0x40d5d8
// Creates the transport entry for a unit in the sub-group
EECPASGUTransport::EECPASGUTransport(EEComplexUnit* inUnit, EECPAttackSubGroup* inSubGroup)
    : EECPAttackSubGroupUnit(inUnit, inSubGroup)
{
    this->mType = kASGUKindTransport;
}

// SYNTHETIC: EXE 0x40d5fb
// (the scalar deleting destructor; 0x1c bytes)

// FUNCTION: EXE 0x40d617
// Creates an empty transport entry for the sub-group (no unit yet)
EECPASGUTransport::EECPASGUTransport(EECPAttackSubGroup* inSubGroup) : EECPAttackSubGroupUnit(inSubGroup)
{
    this->mType = kASGUKindTransport;
}

// FUNCTION: EXE 0x40d636
// Destroys the transport entry (base members only)
EECPASGUTransport::~EECPASGUTransport()
{
}

// FUNCTION: EXE 0x40d641
// Switches state the plain sub-group-unit way
void EECPASGUTransport::SetState(long inState, unsigned long inTime)
{
    EECPAttackSubGroupUnit::SetState(inState, inTime);
}

// FUNCTION: EXE 0x40d651
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
                    theResult = static_cast<EEUGAttackUnit*>(theGoal)->mTarget == this->mUnit;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x40d6d3
// Tests a condition the plain sub-group-unit way
bool EECPASGUTransport::CheckCondition(long inState, unsigned long inTime)
{
    return EECPAttackSubGroupUnit::CheckCondition(inState, inTime);
}


// ICF-FOLDED: EXE 0x67c0b4
// Never retargets: a transport returns false
// (no address of its own: folded into the shared `xor al, al; ret` body the vtable slot points at)
bool EECPASGUTransport::Retarget()
{
    return false;
}

// ICF-FOLDED: EXE 0x4bb118
// Tells that a transport can attack no target (always false)
// (no address of its own: folded into the shared `xor al, al; ret 4` body the vtable slot points at)
bool EECPASGUTransport::CanAttack(EECPASTargetUnit*)
{
    return false;
}

// ICF-FOLDED: EXE 0x40d21f
// Updates the transport the plain sub-group-unit way
// (no address of its own: the same body as EECPASGUSea::Update, which ICF kept)
void EECPASGUTransport::Update(unsigned long inTime)
{
    EECPAttackSubGroupUnit::Update(inTime);
}
