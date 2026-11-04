// C:\EEX\Empire Earth\AI Thread\EECPASGUAirBomber.cpp (file name inferred: no __FILE__). 0x5d47..0x6132.
#include <windows.h>
#include <vector>
#include <set>
#include <map>
#include <math.h>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSRestorable.h"
#include "lle/U2DPoint.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"

#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGAttackUnit.h"
#include "ee/EECPILocation.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EECPAttackSubGroupUnit.h"
#include "ee/EECPASTargetUnit.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEUGUnloadTransport.h"
#include "ee/EECPASGUAirBomber.h"

// FUNCTION: EXE 0x405d47
// Creates the bomber entry for a unit in the sub-group
EECPASGUAirBomber::EECPASGUAirBomber(EEComplexUnit* inUnit, EECPAttackSubGroup* inSubGroup)
    : EECPAttackSubGroupUnit(inUnit, inSubGroup)
{
    this->mType = kASGUKindAirBomber;
}

// SYNTHETIC: EXE 0x405d6a
// (the scalar deleting destructor; 0x1c bytes)

// FUNCTION: EXE 0x405d86
// Creates an empty bomber entry for the sub-group (no unit yet)
EECPASGUAirBomber::EECPASGUAirBomber(EECPAttackSubGroup* inSubGroup)
    : EECPAttackSubGroupUnit(inSubGroup)
{
    this->mType = kASGUKindAirBomber;
}

// FUNCTION: EXE 0x405da5
// Destroys the bomber entry (base members only)
EECPASGUAirBomber::~EECPASGUAirBomber()
{
}

// FUNCTION: EXE 0x405db0
// Picks a target: bombers switch unless the current one is alive within 10 tiles; paratrooper planes fly to drop
// on a land target (the unload goal leaks if it cannot unload there); returns true if it has a target
// The original calls gContinentManager's inline tile lookup 0x4060b4 out of line
bool EECPASGUAirBomber::Retarget()
{
    EETileCoordinate theTile(0, 0);
    EECPASTargetUnit* theTarget = this->ChooseTarget(false, theTile);
    if (theTarget != NULL) {
        EEUnitClass* theClass = this->mUnit->mUnitClass;
        if (theClass->GetUnitType() != kUnitTypeParatrooperPlane) {
            if (theClass->FindAction(kActionInitialAttackState) == NULL)
                return this->mTarget != NULL;
            bool theNewTarget = this->mTarget != theTarget->mUnit;
            if (!theNewTarget) {
                EEUGAttackUnit* theGoal = dynamic_cast<EEUGAttackUnit*>(this->mUnit->GetLoadGoal());
                if (theGoal != NULL && theGoal->mTarget == theTarget->mUnit)
                    return this->mTarget != NULL;
                theNewTarget = true;
            }
            if (this->mTarget != NULL && this->mTarget->mHitPoints > 0 && this->mUnit->DistanceSquaredTo(this->mTarget) <= 100.0f)
                theNewTarget = false;
            if (theNewTarget) {
                this->SetTarget(theTarget->mUnit);
                this->mSubGroup->AttackWith(this, this->mTarget->mComplexUnit, &theTile);
            }
        } else {
            EEComplexUnit* theUnit = theTarget->mUnit;
            long theX = theUnit->GetTileX();
            long theY = theUnit->GetTileY();
            theTile.mY = theY;
            theTile.mX = theX;
            EEAIContinent* theContinent = gContinentManager.GetContinentAt(theTile);
            if (theContinent == NULL || theContinent->mCategory != kContinentCategoryLand)
                return false;
            EEUGUnloadTransport* theGoal = new EEUGUnloadTransport(this->mUnit, theTile.GetX(), theTile.mY, false);
            if (theGoal->CanUnloadAtDestination()) {
                this->SetTarget(theTarget->mUnit);
                if (!this->mSubGroup->GiveGoal(this->mUnit, theGoal, 0, kActionFly)) {
                    delete theGoal;
                    return false;
                }
            } else {
                // the goal is not used: free it (it used to leak). The plane keeps the reference the goal took on
                // it, as before: unit reference counts are game state (cargo release and the unit pool read them)
                theGoal->mTransport = NULL;
                delete theGoal;
            }
        }
    }
    return this->mTarget != NULL;
}

// FUNCTION: EXE 0x405f27
// Returns the squared distance between two units
// (EEComplexUnit::DistanceSquaredTo, inline in ee/EEUnit.h: its first user is Virtual14)

// FUNCTION: EXE 0x405f58
// Updates the bomber every second: once out of its carrier or on the map it heads to the task and idles; when
// idle it attacks if the sub-group has targets, else attacks an enemy task unit or flies to the task location
void EECPASGUAirBomber::Update(unsigned long inTime)
{
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
            if (this->mSubGroup->mTask.mUnit != NULL && this->mSubGroup->mTask.mUnit->mPlayer != this->mSubGroup->mPlayer2
                && this->mSubGroup->mPlayer2->GetEnemy(this->mSubGroup->mTask.mUnit->mPlayer->GetPlayerID()) != 0) {
                EETileCoordinate theTile(0, 0);
                this->mSubGroup->AttackWith(this, this->mSubGroup->mTask.mUnit, &theTile);
            } else {
                this->mSubGroup->SendUnitTo(this, this->mSubGroup->mTask.mLocation, true);
            }
        }
        break;
    }
    this->mNextUpdateTime = inTime + 1000;
}

// FUNCTION: EXE 0x4060b1
// Returns true: any target stays valid
bool EECPASGUAirBomber::IsTargetValid()
{
    return true;
}

// FUNCTION: EXE 0x4060b4
// Returns the continent holding a tile
// (EEAIContinentManager's inline tile lookup 0x60b4, ee/EEAIContinentManager.h)

// FUNCTION: EXE 0x4060c7
// Returns the continent at a tile, or NULL off the continent map
// (EEAIContinentManager::GetContinentAt, inline in ee/EEAIContinentManager.h)

// FUNCTION: EXE 0x4060ef
// Returns the continent id stored for a tile
// (EEAIContinentManager::GetContinentID, inline in ee/EEAIContinentManager.h)

// Flee: does nothing for bombers (no address: ICF-folded into the shared `ret` 0x4e123d; vtable 0x439160 +0x1c)
void EECPASGUAirBomber::Flee()
{
}

// ICF-FOLDED: EXE 0x40631c
// Applies the target preferences the base way
// (no address of its own: the same body as EECPASGUAirFighter::ApplyPrejudices, which ICF kept; [sib 48])
void EECPASGUAirBomber::ApplyPrejudices(EECPASTargetUnit* inTarget)
{
    EECPAttackSubGroupUnit::ApplyPrejudices(inTarget);
}
