// C:\EEX\Empire Earth\AI Thread\EECPASGHelicopter.cpp (file name inferred: no __FILE__). 0x4597f..0x45ee9 (unit map
// edges, not verified yet).
#include <windows.h>
#include <vector>
#include <set>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEViewport.h"
#include "lle/GETransformation.h"
#include "lle/GEPhysicalModel.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSRestorable.h"
#include "lle/USingleton.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EECPILocation.h"
#include "ee/EECPASTargetUnit.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EECPAttackSubGroupUnit.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EECPAttackGroup.h"
#include "ee/EECPAttackCommander.h"
#include "ee/EECPASGAir.h"
#include "ee/EECPASGHelicopter.h"
#include "ee/EECPAAirTargetFilters.h"
#include "lle/U2DRectangle.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEPathManager.h"
#include "ee/EEAIPathGoalModifierBase.h"
#include "ee/EEAIBoundedPathGoalModifierBase.h"
#include "ee/EEAIPathGoalModifiers.h"
#include "ee/EECPCombatManager.h"
#include "ee/EECPAreaTargetCollector.h"
#include "lle/U2DSparseArray.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIPathSector.h"
#include "ee/EEAIPathSubSector.h"
#include "ee/EEAIAStarGraphStorage.h"
#include "ee/EEAIAStarGraphGoalBase.h"
#include "ee/EEAIAStarPathGraphGoal.h"
#include "ee/EEAIKindIDs.h"

// FUNCTION: EXE 0x44597f
// Creates a helicopter attack sub-group (a kind of land sub-group) for the attack group
EECPASGHelicopter::EECPASGHelicopter(EECPAttackGroup* inGroup, long inKind) : EECPASGLand(inGroup, inKind)
{
}

// SYNTHETIC: EXE 0x44599b
// (the scalar deleting destructor)

// FUNCTION: EXE 0x4459b7
// Destroys the helicopter sub-group (base members only)
EECPASGHelicopter::~EECPASGHelicopter()
{
}

// FUNCTION: EXE 0x4459c2
// Attacks with the generic sub-group behaviour, bypassing the land sub-group's version
void EECPASGHelicopter::AttackWith(EECPAttackSubGroupUnit* inUnit, EEComplexUnit* inTarget, EETileCoordinate* inTile)
{
    EECPAttackSubGroup::AttackWith(inUnit, inTarget, inTile);
}

// FUNCTION: EXE 0x4459d6
// Sets every path cost weight to zero
void EECPASGHelicopter::GetPathCosts(void* inFrom, void* inTo, EECPASGWeights& outWeights)
{
    outWeights.Fill(0.0f);
}

// FUNCTION: EXE 0x4459e6
// Takes the requested units as the base does, then refreshes the units; returns the base's result
bool EECPASGHelicopter::TakeRequestedUnits()
{
    bool theResult = EECPAttackSubGroup::TakeRequestedUnits();
    this->RefreshUnits();
    return theResult;
}

// FUNCTION: EXE 0x445a00
// Picks units to split off as the base does, but always leaves at least one in the group
void EECPASGHelicopter::PickSplitUnits(std::vector<EEComplexUnit*>& outUnits)
{
    EECPAttackSubGroup::PickSplitUnits(outUnits);
    if (outUnits.size() >= this->GetNumUnits() && !outUnits.empty())
        outUnits.pop_back();
}

// FUNCTION: EXE 0x445a4e
// Returns the player's high-ground guard share raised by 15% (0 without a computer player)
float EECPASGHelicopter::GetGuardHighGroundPercentage()
{
    return this->mPlayer2 != NULL ? this->mPlayer2->mGuardHighGroundPercentage * 1.15f : 0;
}

// FUNCTION: EXE 0x445a68
// Adds every unit in the bounds that passes the helicopter target filter (enemies optional) to the targets
void EECPASGHelicopter::CollectTargets(EEAIAStarBounds* inBounds, bool inIncludeEnemies)
{
    std::vector<EEComplexUnit*> theUnits;
    EECPAHelicopterTargetFilter theFilter;
    EECPASTargetUnit* theTarget;
    theFilter.mReserved = false;
    theFilter.mPlayer = this->mPlayer2;
    theFilter.mGroup = this;
    theFilter.mIncludeEnemies = inIncludeEnemies;
    theFilter.mPlayerID = this->mTask.GetPlayerID();
    // Only the low byte of the argument is the include-enemies flag (0x445a8f); the parent passes its
    // Virtual90 argument

    if (EECollectASGTargets(*inBounds, theUnits, theFilter, true) != 0) {
        this->mTargets.reserve(theUnits.size());
        const std::vector<EEComplexUnit*>& theConst = theUnits;
        for (long i = theConst.size() - 1; i >= 0; i--) {
            theTarget = EECPAttackSubGroup::NewTarget(theConst[i]);
            this->mTargets.push_back(theTarget);
        }
    }
}

// FUNCTION: EXE 0x445b25
// Requests a path between two tiles with the group's terrain flags and the helicopter route planner
void EECPASGHelicopter::RequestPath(void* inFrom, void* inTo)
{
    EECPASGWeights theWeights;
    theWeights.Fill(0.0f);
    this->GetPathCosts(inFrom, inTo, theWeights);
    this->mPath.clear();
    this->CancelPath();
    EEAIGPGModifierHelicopterAttackSubGroupRoutePlanner* theModifier =
        new EEAIGPGModifierHelicopterAttackSubGroupRoutePlanner(this, theWeights);
    // RequestPath takes the graph modifier as a long-sized opaque handle; the two arguments point at tile
    // coordinates
    this->mPathRequest = gPathManager.RequestPath(this->mPlayer->GetPlayerID(),
        *static_cast<EETileCoordinate*>(inFrom), *static_cast<EETileCoordinate*>(inTo),
        &this->mFlags, 2, -1, theModifier, 0);
    this->mPlayer->mCombatManager->mAttackCommander->mUpdatesThisTick++;
    theModifier->Dereference();
}

// FUNCTION: EXE 0x445bc8
// Collects the units in the bounds that pass the helicopter target filter, dropping duplicates only when a
// multi-tile unit was accepted and uniqueness was asked for; returns the count
// EECollectASGTargets<EECPAHelicopterTargetFilter>, emitted by CollectTargets above.

// FUNCTION: EXE 0x445d71
// Returns the step cost between path sub-sectors: 10 straight or 14 diagonal, doubled into a wall and again when a
// link joins a kind-13 sub-sector to another kind 13 or one the goal cannot cross; at least 1
long EEAIGPGModifierLandWallAvoider::GetCost(EEAIAStarGraphNode* inFrom, EEAIPathSubSector* inTo)
{
    std::vector<std::pair<EEAIPathSubSector*, EEAIPathSubSector*> > theLinks;
    long theKind = inTo->mKind;
    EEAIPathSubSector* theFrom = static_cast<EEAIPathSubSector*>(inFrom->mObject);
    long theCost = (theFrom->mSector->mCenter.mX == inTo->mSector->mCenter.mX ||
                             theFrom->mSector->mCenter.mY == inTo->mSector->mCenter.mY) ? 10 : 14;
    if (theKind == kAITileWall)
        theCost *= 2;
    inTo->GetLinks(theFrom->mID, theLinks);
    for (long i = theLinks.size() - 1; i >= 0; --i) {
        if ((theLinks[i].first->mKind == 13 &&
             (theLinks[i].second->mKind == 13 || !this->mGoal->mPathFlags.mFlags[theLinks[i].second->mKind])) ||
            (theLinks[i].second->mKind == 13 &&
             (theLinks[i].first->mKind == 13 || !this->mGoal->mPathFlags.mFlags[theLinks[i].first->mKind]))) {
            theCost *= 2;
            break;
        }
    }
    if (theCost <= 1)
        theCost = 1;
    return theCost;
}

// ICF-FOLDED: EXE 0x67c0b4
// Returns false
// (no address of its own: folded into the shared `xor al, al; ret` body the vtable slot points at)
bool EECPASGHelicopter::DecideSplit()
{
    return false;
}

// ICF-FOLDED: EXE 0x4e123d
// Does nothing for helicopter paths
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EECPASGHelicopter::PrunePath()
{
}

// ICF-FOLDED: EXE 0x4b3257
// Returns true: helicopters can reach any location
// (no address of its own: folded into the shared `mov al, 1; ret 4` body the vtable slot points at)
bool EECPASGHelicopter::CanReach(EECPILocation const*)
{
    return true;
}

// ICF-FOLDED: EXE 0x441948
// Returns the continent-guard share, fixed at 1.0
// (no address of its own: the same body as EECPASGAir::GetGuardContinentPercentage, which ICF kept; [sib 60])
float EECPASGHelicopter::GetGuardContinentPercentage()
{
    return 1.0f;
}
