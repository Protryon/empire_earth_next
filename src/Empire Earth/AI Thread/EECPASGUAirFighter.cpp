// C:\EEX\Empire Earth\AI Thread\EECPASGUAirFighter.cpp (file name inferred: no __FILE__; RTTI class
// EECPASGUAirFighter, vtable 0x439268). An attack sub group unit kind: 0x619b..0x6328 (EECPASGUAircraftCarrier's dtor
// ends at 0x619b, EECPASGUHelicopter's ctor starts at 0x6328). The first ctor sets the kind in the body (the load ctor does not); ??_G follows the
// first ctor (its first user); the dtor and the slots are out of line.
#include <windows.h>

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
#include "ee/EECPILocation.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EECPAttackSubGroupUnit.h"
#include "ee/EECPASGUAirFighter.h"

// FUNCTION: EXE 0x40619b
// Creates the fighter entry for a unit in the sub-group
EECPASGUAirFighter::EECPASGUAirFighter(EEComplexUnit* inUnit, EECPAttackSubGroup* inSubGroup)
    : EECPAttackSubGroupUnit(inUnit, inSubGroup)
{
    this->mType = kASGUKindAirFighter;
}

// SYNTHETIC: EXE 0x4061be
// (the scalar deleting destructor)

// FUNCTION: EXE 0x4061da
// Creates an empty fighter entry for the sub-group (no unit yet; the kind is left unset, unlike the other ctor)
EECPASGUAirFighter::EECPASGUAirFighter(EECPAttackSubGroup* inSubGroup)
    : EECPAttackSubGroupUnit(inSubGroup)
{
}

// FUNCTION: EXE 0x4061f2
// Destroys the fighter entry (base members only)
EECPASGUAirFighter::~EECPASGUAirFighter()
{
}

// FUNCTION: EXE 0x4061fd
// Updates the fighter every second: off the map it waits as contained, then heads to the task and idles; when
// idle it attacks if the sub-group has targets, else flies to the task location
void EECPASGUAirFighter::Update(unsigned long inTime)
{
    if (this->mUnit->mTileX == -10000 && this->mState != kASGUStateContained)
        this->mState = kASGUStateContained;
    switch (this->mState) {
    default:
        EECPAttackSubGroupUnit::Update(inTime);
        break;
    case kASGUStateContained:
        if (this->mUnit->mContainer != NULL || this->mUnit->mTileX != -10000) {
            if (this->mUnit->mGoalState == NULL && (this->mUnit->mCommand ? this->mUnit->mCommand->mType : 0) != kActionCircle
                && (this->mUnit->mFlightTime > 0 || this->mUnit->mContainer != NULL) && this->mSubGroup->mTask.mIsActive)
                this->mSubGroup->SendUnitTo(this, this->mSubGroup->mTask.mLocation, true);
            this->SetState(kASGUStateIdle, inTime);
        }
        break;
    case kASGUStateIdle:
        if (this->CheckCondition(kASGUConditionSubGroupHasTargets, inTime)) {
            this->SetState(kASGUStateChooseAction, inTime);
        } else if (this->mUnit->mGoalState == NULL && (this->mUnit->mCommand ? this->mUnit->mCommand->mType : 0) != kActionCircle
                   && (this->mUnit->mFlightTime > 0 || this->mUnit->mContainer != NULL) && this->mSubGroup->mTask.mIsActive) {
            this->mSubGroup->SendUnitTo(this, this->mSubGroup->mTask.mLocation, true);
        }
        break;
    }
    this->mNextUpdateTime = inTime + 1000;
}

// FUNCTION: EXE 0x40631c
// Applies the target preferences the base way
void EECPASGUAirFighter::ApplyPrejudices(EECPASTargetUnit* inTarget)
{
    EECPAttackSubGroupUnit::ApplyPrejudices(inTarget);
}

// Flee: does nothing for fighters (no address: ICF-folded into the shared `ret` 0x4e123d; vtable 0x439268 +0x1c)
void EECPASGUAirFighter::Flee()
{
}

// IsTargetValid: always true (no address: ICF-folded into EECPASGUAirBomber::IsTargetValid 0x4060b1;
// vtable 0x439268 +0x58)
bool EECPASGUAirFighter::IsTargetValid()
{
    return true;
}
