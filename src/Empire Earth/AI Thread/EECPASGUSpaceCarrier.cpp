// C:\EEX\Empire Earth\AI Thread\EECPASGUSpaceCarrier.cpp (file name inferred: no __FILE__). 0xd3d6..0xd5d8.
#include <windows.h>
#include <vector>
#include <set>
#include <deque>

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
#include "ee/EEUnitVectorItem.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EECPILocation.h"
#include "ee/EECPASTargetUnit.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EECPAttackSubGroupUnit.h"
#include "ee/EECPASGUSpaceCarrier.h"

// FUNCTION: EXE 0x40d3d6
// Creates a space carrier sub group unit for a unit
EECPASGUSpaceCarrier::EECPASGUSpaceCarrier(EEComplexUnit* inUnit, EECPAttackSubGroup* inSubGroup)
    : EECPAttackSubGroupUnit(inUnit, inSubGroup)
{
    this->mType = kASGUKindSpaceCarrier;
}

// SYNTHETIC: EXE 0x40d3f9
// (the scalar deleting destructor; 0x1c bytes)

// FUNCTION: EXE 0x40d415
// Creates an empty space carrier sub group unit (no unit yet; used when loading)
EECPASGUSpaceCarrier::EECPASGUSpaceCarrier(EECPAttackSubGroup* inSubGroup)
    : EECPAttackSubGroupUnit(inSubGroup)
{
    this->mType = kASGUKindSpaceCarrier;
}

// FUNCTION: EXE 0x40d434
// Destroys the space carrier sub group unit (base members only)
EECPASGUSpaceCarrier::~EECPASGUSpaceCarrier()
{
}

// FUNCTION: EXE 0x40d43f
// Picks a target and, unless the carrier already attacks it, orders the carrier and each cargo unit to attack it
// and opens the bays; returns whether it re-targeted
bool EECPASGUSpaceCarrier::Retarget()
{
    EETileCoordinate theTile(0, 0);
    EECPASTargetUnit* theTarget = this->ChooseTarget(false, theTile);
    if (theTarget == NULL)
        return false;
    bool theNewTarget = this->mTarget != theTarget->GetUnit();
    if (!theNewTarget) {
        EEUGAttackUnit* theGoal = dynamic_cast<EEUGAttackUnit*>(this->GetUnit()->GetLoadGoal());
        if (theGoal == NULL && this->GetUnit()->mGoalQueues[1].mGoals.size() > 0)
            theGoal = dynamic_cast<EEUGAttackUnit*>(this->GetUnit()->mGoalQueues[1].mGoals.back());
        if (theGoal != NULL && theGoal->GetTarget() == theTarget->GetUnit())
            return false;
    }
    this->SetTarget(theTarget->GetUnit());
    EEUGAttackUnit* theAttack = new EEUGAttackUnit(this->GetUnit(), theTarget->GetUnit(), true, false);
    this->GetUnit()->ClearGoalQueue(1);
    this->GetUnit()->SetGoal(theAttack->Clone(), 0, 1);
    unsigned long theCount = this->GetUnit()->GetNumCargoUnits();
    for (unsigned long i = 0; i < theCount; i++) {
        EEComplexUnit* theCargo = this->GetUnit()->GetCargo(i);
        theCargo->SetGoal(theAttack->Clone(), 0, 0);
    }
    delete theAttack;   // the template the goals were cloned from
    this->GetUnit()->StartAction(kActionAirportOpen);
    this->GetUnit()->GetUnitClass()->ActionChanged(this->GetUnit());
    return true;
}

// FUNCTION: EXE 0x40d5c5
// Returns the number of cargo units the unit carries
// (EEComplexUnit::GetNumCargoUnits, inline in ee/EEUnit.h: its first user is Virtual14)

