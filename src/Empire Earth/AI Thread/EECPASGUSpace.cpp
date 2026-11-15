// C:\EEX\Empire Earth\AI Thread\EECPASGUSpace.cpp (file name inferred: no __FILE__; RTTI class
// EECPASGUSpace, vtable 0x4399cc). An attack sub group unit kind: 0xd22b..0xd3d6 (EECPASGUSea's Virtual6C ends at
// 0xd22b, EECPASGUSpaceCarrier's ctor starts at 0xd3d6). Both ctors set the kind in the body; ??_G follows the
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
#include "ee/EECPASGUSpace.h"

// FUNCTION: EXE 0x40d22b
// Creates a space sub group unit for a unit
EECPASGUSpace::EECPASGUSpace(EEComplexUnit* inUnit, EECPAttackSubGroup* inSubGroup)
    : EECPAttackSubGroupUnit(inUnit, inSubGroup)
{
    this->mType = kASGUKindSpace;
}

// SYNTHETIC: EXE 0x40d24e
// (the scalar deleting destructor)

// FUNCTION: EXE 0x40d26a
// Creates an empty space sub group unit (no unit yet; used when loading)
EECPASGUSpace::EECPASGUSpace(EECPAttackSubGroup* inSubGroup)
    : EECPAttackSubGroupUnit(inSubGroup)
{
    this->mType = kASGUKindSpace;
}

// FUNCTION: EXE 0x40d289
// Destroys the space sub group unit (base members only)
EECPASGUSpace::~EECPASGUSpace()
{
}

// FUNCTION: EXE 0x40d294
// Updates the unit; a planet spaceship that is carried or back on the map leaves the contained state for idle, and
// an idle one chooses an action when there are targets, else flies to the active task when free; next in 1 s
void EECPASGUSpace::Update(unsigned long inTime)
{
    if (this->mUnit->mUnitClass->mCategory != kUnitCategoryPlanetSpaceship) {
        EECPAttackSubGroupUnit::Update(inTime);
    } else {
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
}
