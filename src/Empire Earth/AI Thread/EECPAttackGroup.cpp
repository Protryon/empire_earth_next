// C:\EEX\Empire Earth\AI Thread\EECPAttackGroup.cpp (file name inferred: no __FILE__; RTTI class
// EECPAttackGroup, vtable 0x43bc94; local function object vtable 0x43bcd0). A computer player's attack group:
// 0x5473a..0x5ac9c (EECPAttackFormationNode.cpp ends at 0x5473a; EECPAttackSubGroup.cpp starts at 0x5ac9c). The ctor,
// ??_G, dtor, the members, the sub group factories (0x564f6, 0x5661e), EECPUnitSelectionGroup's ctor/dtor, the slots
// (Load, Virtual08, Restore, Save), then the template COMDATs to the UMemoryPool<EECPASTargetUnit> template-static
// chain 0x5ac57
#include <windows.h>
#include <vector>
#include <deque>
#include <map>
#include <set>
#include <algorithm>
#include <typeinfo>
#include <functional>

#include "lle/LowLevelEngine.h"
#include "ee/EmpireEarth.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSFile.h"
#include "lle/FSRestorable.h"
#include "lle/USingleton.h"
#include "lle/U2DPoint.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/UMemoryPool.h"

#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEGlobalConstants.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIChokePoint.h"
#include "ee/EETown.h"
#include "ee/EECPILocation.h"
#include "ee/EECPAGTask.h"
#include "ee/EECPAGOrder.h"
#include "ee/EECPASTargetUnit.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EECPAttackSubGroupUnit.h"
#include "ee/EECPASGLand.h"
#include "ee/EECPASGSea.h"
#include "ee/EECPASGAir.h"
#include "ee/EECPASGHelicopter.h"
#include "ee/EECPASGSubmarine.h"
#include "ee/EECPASGSpaceship.h"
#include "ee/EECPASGSpaceFighter.h"
#include "ee/EECPASGTransport.h"
#include "ee/EECPASGSeaTransport.h"
#include "ee/EECPASGAirTransport.h"
#include "ee/EECPASGSpaceTransport.h"
#include "ee/EECPASGUAirBomber.h"
#include "ee/EECPASGUAircraftCarrier.h"
#include "ee/EECPASGUAirFighter.h"
#include "ee/EECPASGUTransport.h"
#include "ee/EECPASGUSea.h"
#include "ee/EECPASGUPriest.h"
#include "ee/EECPASGUMechPriest.h"
#include "ee/EECPASGUProphetCalamityAnalyzerBase.h"
#include "ee/EECPASGUProphet.h"
#include "ee/EECPASGUHero.h"
#include "ee/EECPASGUMechProphet.h"
#include "ee/EECPASGUHelicopter.h"
#include "ee/EECPASGUSpace.h"
#include "ee/EECPASGUSpaceCarrier.h"
#include "ee/EECPAttackGroup.h"
#include "ee/EECPAttackCommander.h"
#include "ee/EECPUnitRequirement.h"
#include "ee/EECPCombatStrategy.h"
#include "ee/EECPCombatManager.h"
#include "ee/EECPDefenseCommander.h"
#include "ee/EECPIntelligence.h"
#include "ee/EECPUnitSelector.h"
#include "ee/EEAIRandom.h"
#include "ee/EECPCivilizationManager.h"
#include "ee/EECPUnitManager.h"
#include "ee/EEAIFOUnitsToKillScoringBase.h"
#include "ee/EECPUMGatherUnitsScreenBase.h"
#include <EEAIFunctionObjects.h>

extern long gDCGameSpeed;                   // 0x530d24 (EECPDefenseCommander.cpp's name; FindLandTransport: > 0 for a sea transport) (provisional name)

static const float kAGNearDistance = 324.0f;    // 0x43bca8

// 0x564f6 / 0x5661e (cdecl): a new sub group unit for inUnit (of its kind), or the load path's empty one (inUnit 0)
// (provisional names)
EECPAttackSubGroupUnit* EECPCreateSubGroupUnit(EEComplexUnit* inUnit, EECPAttackSubGroup* inSubGroup, bool inGeneric);
EECPAttackSubGroupUnit* EECPCreateSubGroupUnitOfKind(EEComplexUnit* inUnit, EECPAttackSubGroup* inSubGroup, long inKind);

// the towns by continent id, then by mL78 (PlaceInSubGroup's sort; COMDAT 0x5a8ad) (provisional name)
struct EECPAGTownLess {
    bool operator()(EETown* inA, EETown* inB) const;
};

// FUNCTION: EXE 0x45a8ad
// Orders towns by continent id, then by value
// not inline: every sort/heap helper calls it out of line (0x45aa73, 0x45a953, 0x45ab13, 0x45abd4); an in-class body
// is expanded there. Placed among the template COMDATs in the original.
bool EECPAGTownLess::operator()(EETown* inA, EETown* inB) const
{
    return inA->mContinent < inB->mContinent || inA->mContinent == inB->mContinent && inA->mValue < inB->mValue;
}

// FUNCTION: EXE 0x45473a
// Creates an empty attack group for the commander, with the combat manager's next group id and no continent
EECPAttackGroup::EECPAttackGroup(EECPAttackCommander* inCommander)
{
    this->mCommander = inCommander;
    this->mIsFinished = false;
    this->mL64 = 0;
    long theID = inCommander->GetCombatManager()->GetNextGroupID();
    this->mID = theID;
    this->mContinentID = 0;
}

// SYNTHETIC: EXE 0x45479a
// (the scalar deleting destructor)

// FUNCTION: EXE 0x4547b6
// Destroys the group, shutting down and releasing every sub-group
EECPAttackGroup::~EECPAttackGroup()
{
    for (long i = this->mSubGroups.size() - 1; i >= 0; i--) {
        this->mSubGroups[i]->Shutdown();
        this->mSubGroups[i]->Dereference();
    }
    this->mSubGroups.clear();
}

// FUNCTION: EXE 0x454829
// Adds a unit to the nearest sub-group of its kind within 18 tiles; returns false when none is that close
bool EECPAttackGroup::JoinSubGroup(EEComplexUnit* inOld, EEComplexUnit* inUnit)
{
    EEUnitClass* theClass = inUnit->mUnitClass;
    std::multimap<float, EECPAttackSubGroup*, std::greater<float> > theGroups;
    EETileLocation theTile;
    bool theResult = false;
    long theKind = this->GetSubGroupKind(theClass);
    for (long i = this->mSubGroups.size() - 1; i >= 0; i--) {
        if (this->mSubGroups[i]->mKind != theKind)
            continue;
        this->mSubGroups[i]->FindPosition(theTile, true, false);
        float theDistance = inUnit->DistanceSquaredToTileXY(theTile.mX, theTile.mY);
        if (theDistance < kAGNearDistance)
            theGroups.insert(std::make_pair(
                theDistance, this->mSubGroups[i]));
    }
    if (theGroups.size() > 0) {
        theResult = true;
        theGroups.begin()->second->AddUnit(inUnit);
    }
    return theResult;
}

// FUNCTION: EXE 0x454948
// Adds a combat unit to a sub-group of its kind with under 10 units, else to a new one sent to an own town, a
// near choke point or a random continent point against a random enemy; false for transports, ports, AM batteries
bool EECPAttackGroup::PlaceInSubGroup(EEComplexUnit* inUnit)
{
    std::multimap<long, EEAIChokePoint*>::iterator theIt;
    std::multimap<long, EEAIChokePoint*> theByDistance;
    std::vector<EETown*> theTowns;
    EECPAGTask theTask;
    EECPILocation theLocation;
    EETileLocation theUnitTile(inUnit->mTileX, inUnit->mTileY);
    EETileLocation theClosest;
    std::vector<EEAIChokePoint*> theChokePoints;
    EETown* theTown = NULL;
    unsigned short theContinentID =
        gContinentManager.GetContinentID(inUnit->mContainer ? static_cast<EEUnit*>(inUnit->mContainer) : static_cast<EEUnit*>(inUnit));
    EECPAttackSubGroup* theSubGroup = NULL;
    long theKind = this->GetSubGroupKind(inUnit->mUnitClass);
    bool theResult = false;
    if (inUnit->mUnitClass->IsTransport() && !(inUnit->mUnitClass->mDomains & kMovementLand))
        return theResult;
    if (inUnit->mUnitClass->mCategory == kUnitCategorySpaceBuilding || inUnit->mUnitClass->mCategory == kUnitCategoryNavalBuilding ||
        inUnit->mUnitClass->GetUnitType() == kUnitType14AntiMissileBattery)
        return theResult;
    for (long i = this->mSubGroups.size() - 1; i >= 0; i--) {
        if (!(this->mSubGroups[i]->GetNumUnits() < 10) || this->mSubGroups[i]->mKind != theKind)
            continue;
        this->mSubGroups[i]->AddUnit(inUnit);
        theResult = true;
        return theResult;
    }
    EEPlayer* thePlayer = this->mCommander->GetCombatManager()->mIntelligence->GetRandomEnemy(this->mCommander->mPlayer);
    if (!thePlayer)
        thePlayer = inUnit->mPlayer;
    // the new sub group's task carries the target player too (original 0x454ab0: [ebp-0x90] = theTask+0x20 before
    // [esi+0x48]); without it Virtual14 keeps a null player and Virtual08 reads gWorld.mPlayers[-1] (a crash)
    theTask.mPlayer = thePlayer;
    this->mTask.mPlayer = thePlayer;
    switch (theKind) {
    case kASGKindLand:
        theSubGroup = new EECPASGLand(this, 0);
        break;
    case kASGKindSea:
    case 7:
        theSubGroup = new EECPASGSea(this, theKind);
        break;
    case kASGKindAir:
        theSubGroup = new EECPASGAir(this, kASGKindAir);
        break;
    case kASGKindHelicopter:
        theSubGroup = new EECPASGHelicopter(this, kASGKindHelicopter);
        break;
    case kASGKindSubmarine:
        theSubGroup = new EECPASGSubmarine(this, kASGKindSubmarine);
        break;
    case 8:
        theSubGroup = new EECPASGHelicopter(this, 8);
        break;
    case kASGKindSpaceship:
        theSubGroup = new EECPASGSpaceship(this, kASGKindSpaceship);
        break;
    case kASGKindSpaceFighter:
        theSubGroup = new EECPASGSpaceFighter(this, kASGKindSpaceFighter);
        break;
    }
    if (theSubGroup) {
        theSubGroup->AddUnit(inUnit);
        this->mSubGroups.push_back(theSubGroup);
        bool theNeedTarget;
        long theSubKind = theSubGroup->mKind;
        if (theSubKind != kASGKindLand && theSubKind != kASGKindHelicopter && theSubKind != 8)
            theNeedTarget = true;
        else {
            bool theUseTowns;
            if (gPhysics->GetRandom() < 0.65f)
                theUseTowns = this->mCommander->mPlayer->mTowns.size() > 0;
            else {
                gContinentManager.GetChokePoints(theChokePoints, theContinentID, true);
                theUseTowns = theChokePoints.size() == 0;
            }
            if (theUseTowns) {
                theTowns = this->mCommander->mPlayer->mTowns;
                std::sort(theTowns.begin(), theTowns.end(), EECPAGTownLess());
                if (theTowns.size() == 0)
                    theNeedTarget = true;
                else {
                    for (long j = theTowns.size() - 1; j >= 0; j--) {
                        if (theTowns[j]->mContinent == theContinentID) {
                            theTown = theTowns[j];
                            break;
                        }
                    }
                    theNeedTarget = theTown == NULL;
                    if (theTown) {
                        theLocation.mSerial = theTown->mSerial;
                        theLocation.mType = kAIAreaTown;
                        theLocation.mTile = theTown->GetRandomPoint();
                        theTask.mLocation = theLocation;
                        theTask.mType = kAGTaskLocation;
                        theTask.mIsActive = true;
                    }
                }
            } else if (theChokePoints.size() == 0)
                theNeedTarget = true;
            else {
                theNeedTarget = false;
                long theCount;
                while (theCount = 0, theChokePoints.size() != 0) {
                    theClosest = theChokePoints.back()->GetClosestPoint(theUnitTile);
                    long theDistance = theClosest.DistanceToSquared(theUnitTile);
                    theByDistance.insert(std::make_pair(theDistance,
                        theChokePoints.back()));
                    theChokePoints.pop_back();
                }
                for (theIt = theByDistance.begin();
                     theIt != theByDistance.end() && theCount < 4; theIt++, theCount++)
                    theChokePoints.push_back(theIt->second);
                EEAIRandomShuffle(theChokePoints.begin(), theChokePoints.end());
                theLocation.mSerial = theChokePoints.front()->mSerial;
                theLocation.mType = kAIAreaChokePoint;
                theLocation.mTile = theChokePoints.front()->GetRandomPoint();
                theTask.mLocation = theLocation;
                theTask.mType = kAGTaskLocation;
                theTask.mIsActive = true;
            }
        }
        theResult = true;
        if (theNeedTarget) {
            EEAIContinent* theContinent = gContinentManager.mContinents[theContinentID];
            if (theContinent) {
                theLocation.mSerial = theContinent->mSerial;
                theLocation.mType = kAIAreaContinent;
                theLocation.mTile = theContinent->GetRandomPoint();
                theTask.mLocation = theLocation;
                theTask.mType = kAGTaskLocation;
                theTask.mIsActive = true;
            } else
                theResult = false;
        }
        if (theTask.mIsActive)
            theSubGroup->SetTask(theTask);
    }
    return theResult;
}

// FUNCTION: EXE 0x454f62
// Gives an air sub-group a strike on the best reachable target region, else a defence task, else a return to its
// rally point
void EECPAttackGroup::AssignAirTask(EECPAttackSubGroup* inSubGroup)
{
    EECPAGTask theTask;
    EECPILocation theTarget;
    EECPILocation theFrom;
    theTask.mPlayer = this->mTask.mPlayer;
    inSubGroup->GetLocation(theFrom);
    if (this->FindAirTarget(inSubGroup, theFrom, theTarget)) {
        theTask.mType = kAGTaskTerritory;
        theTask.mLocation = theTarget;
        theTask.mIsActive = true;
    } else {
        EECPCombatManager* theCombat = this->mCommander->GetCombatManager();
        theCombat->mDefenseCommander->AssignSubGroupTask(inSubGroup, theTask, this->mContinentID != 0);
    }
    if (!theTask.mIsActive) {
        theTask.mType = kAGTaskReturn;
        theTask.mLocation = inSubGroup->mRallyPoint;
        theTask.mIsActive = true;
    }
    inSubGroup->SetTask(theTask);
}

// FUNCTION: EXE 0x45502e
// Gives a land sub-group an attack on the best reachable target region, else a defence task, a random point on
// the group's continent, its rally point or its own position (the continent pointer is never NULL-checked)
void EECPAttackGroup::AssignLandTask(EECPAttackSubGroup* inSubGroup)
{
    EECPILocation theTarget;
    EECPILocation theFrom;
    EECPAGTask theTask;
    EECPCombatManager* theCombat;
    theTask.mPlayer = this->mTask.mPlayer;
    inSubGroup->GetLocation(theFrom);
    if (inSubGroup->mMission == 2 || inSubGroup->mMission == 0) {
        if (this->FindLandTarget(inSubGroup, theFrom, theTarget)) {
            inSubGroup->mMission = 2;
            theTask.mType = kAGTaskTerritory;
            theTask.mLocation = theTarget;
            theTask.mIsActive = true;
            inSubGroup->SetTask(theTask);
            unsigned long theTime = gPhysics->mGameTime;
            if (inSubGroup->CheckTask(theTime))
                goto done;
            theTask.mIsActive = false;
        }
        inSubGroup->mMission = 0;
    }
    theCombat = this->mCommander->GetCombatManager();
    theCombat->mDefenseCommander->AssignSubGroupTask(inSubGroup, theTask, this->mContinentID != 0);
    if (!theTask.mIsActive && this->mContinentID != 0) {
        EEAIContinent* theContinent = gContinentManager.GetContinentByID(this->mContinentID);
        // test the continent found, not the id again: the id may no longer name a continent
        if (theContinent != NULL) {
            theTarget.mSerial = theContinent->mSerial;
            theTarget.mType = kAIAreaContinent;
            theTarget.mTile = theContinent->GetRandomPoint();
            theTask.mLocation = theTarget;
            theTask.mType = kAGTaskLocation;
            theTask.mIsActive = true;
        }
    }
done:
    if (!theTask.mIsActive) {
        theTask.mType = kAGTaskReturn;
        theTask.mLocation = inSubGroup->mRallyPoint;
        theTask.mIsActive = gWorld.mMap->GetTileAtLocation(theTask.mLocation.mTile) != NULL;
        if (!theTask.mIsActive) {
            theTask.mType = kAGTaskReturn;
            EECPILocation theRally(inSubGroup->GetPosition());
            theTask.mLocation = theRally;
            theTask.mIsActive = true;
        }
    }
    inSubGroup->SetTask(theTask);
}

// FUNCTION: EXE 0x4551d6
// Gives a sea sub-group a task: 25% of the time hunts a known enemy unit on its or a connected water body, else a
// defence task or a random point on its (or a connected) water body, else its rally point
void EECPAttackGroup::AssignSeaTask(EECPAttackSubGroup* inSubGroup)
{
    std::vector<EETileCoordinate> theUnused;
    std::vector<EEComplexUnit*> theUnits;
    std::vector<unsigned short> theIDs;
    EECPILocation theFrom;
    EECPILocation theTarget;
    EECPAGTask theTask;
    EECPIntelligencePlayerFile* theFile = this->mCommander->GetIntelligence()->GetFile(this->mTask.GetPlayer());
    theTask.mPlayer = this->mTask.mPlayer;
    inSubGroup->GetLocation(theFrom);
    bool theFound = this->FindSeaTarget(inSubGroup, theFrom, theTarget);
    if (theFound) {
        theTask.mType = kAGTaskTerritory;
        theTask.mLocation = theTarget;
        theTask.mIsActive = true;
    }
    if (!theTask.mIsActive) {
        if (gPhysics->GetRandom() <= 0.25f) {
            unsigned short theID = inSubGroup->GetMainContinent();
            theFile->GetUnits(theUnits, theID, true);
            if (theUnits.size() == 0) {
                gContinentManager.GetConnectedIDs(theIDs, theID);
                EEAIContinent* theContinent;
                long i = theIDs.size();
                do {
                    if (--i < 0)
                        goto ranged;
                    theContinent = gContinentManager.mContinents[*(theIDs.begin() + i)];
                } while (!theContinent || theContinent->mCategory != kContinentCategoryWater);
                theFile->GetUnits(theUnits, theContinent->mID, true);
                if (theUnits.size() > 0) {
                    unsigned long theIndex = gPhysics->GetRandom(0L, theUnits.size() - 1);
                    if (theIndex < theUnits.size()) {
                        theTarget.mTile.mX = theUnits[theIndex]->mTileX;
                        theTarget.mTile.mY = theUnits[theIndex]->mTileY;
                        theTask.mType = kAGTaskUnit;
                        theTask.SetUnit(theUnits[theIndex]);
                        theTask.mLocation = theTarget;
                        theTask.mIsActive = true;
                    }
                }
            } else {
                unsigned long theIndex = gPhysics->GetRandom(0L, theUnits.size() - 1);
                if (theIndex < theUnits.size()) {
                    theTarget.mTile.mX = theUnits[theIndex]->mTileX;
                    theTarget.mTile.mY = theUnits[theIndex]->mTileY;
                    theTask.mType = kAGTaskUnit;
                    theTask.SetUnit(theUnits[theIndex]);
                    theTask.mLocation = theTarget;
                    theTask.mIsActive = true;
                }
            }
        }
    }
ranged:
    if (!theTask.mIsActive && gPhysics->GetRandom() <= 1.0f) {
        unsigned short theID = inSubGroup->GetMainContinent();
        if (this->mCommander->GetCombatManager()->mDefendTasks.size() > 0) {
            float theChance = 0.9f;
            if (0.9f <= this->mCommander->GetCombatManager()->mDefendTasks.size() * 0.05f + 0.5f)
                theChance = this->mCommander->GetCombatManager()->mDefendTasks.size() * 0.05f + 0.5f;
            if (gPhysics->mRandom.Generate() < theChance)
                theFound = theTask.mIsActive = this->mCommander->GetCombatManager()->ChooseTask(theTask, false, true, false, theID);
        }
        if (!theFound) {
            EEAIContinent* theContinent = gContinentManager.GetContinentByID(theID);
            if (theContinent) {
                if (theContinent->mCategory == kContinentCategoryWater) {
                    theTarget.mSerial = theContinent->mSerial;
                    theTarget.mType = kAIAreaContinent;
                    theTarget.mTile = theContinent->GetRandomPoint();
                    theTask.mType = kAGTaskLocation;
                    theTask.mLocation = theTarget;
                    theTask.mIsActive = true;
                } else {
                    gContinentManager.GetConnectedIDs(theIDs, theID);
                    for (long i = theIDs.size() - 1; i >= 0; i--) {
                        EEAIContinent* theOther = gContinentManager.GetContinentByID(theIDs[i]);
                        if (theOther && theOther->mCategory == kContinentCategoryWater) {
                            theTarget.mSerial = theOther->mSerial;
                            theTarget.mType = kAIAreaContinent;
                            theTarget.mTile = theOther->GetRandomPoint();
                            theTask.mType = kAGTaskLocation;
                            theTask.mLocation = theTarget;
                            theTask.mIsActive = true;
                        }
                    }
                }
            }
        }
    }
    if (!theTask.mIsActive) {
        theTask.mType = kAGTaskReturn;
        theTask.mLocation = inSubGroup->mRallyPoint;
        theTask.mIsActive = true;
    }
    if (!theTask.mIsActive) {
        theTask.mType = kAGTaskReturn;
        theTask.mLocation = EECPILocation(inSubGroup->GetPosition());
        theTask.mIsActive = true;
    }
    inSubGroup->SetTask(theTask);
}

// FUNCTION: EXE 0x455754
// Gives a space sub-group a task: 25% of the time hunts a known enemy unit in its or a connected space area, else
// a defence task or a random point in its (or a connected) space area, else its rally point
void EECPAttackGroup::AssignSpaceTask(EECPAttackSubGroup* inSubGroup)
{
    std::vector<EETileCoordinate> theUnused;
    std::vector<EEComplexUnit*> theUnits;
    std::vector<unsigned short> theIDs;
    EECPILocation theFrom;
    EECPILocation theTarget;
    EECPAGTask theTask;
    EECPIntelligencePlayerFile* theFile = this->mCommander->GetIntelligence()->GetFile(this->mTask.GetPlayer());
    theTask.mPlayer = this->mTask.mPlayer;
    inSubGroup->GetLocation(theFrom);
    bool theFound = this->FindSpaceTarget(inSubGroup, theFrom, theTarget);
    if (theFound) {
        theTask.mType = kAGTaskTerritory;
        theTask.mLocation = theTarget;
        theTask.mIsActive = true;
    }
    if (!theTask.mIsActive) {
        if (gPhysics->GetRandom() <= 0.25f) {
            unsigned short theID = inSubGroup->GetMainContinent();
            theFile->GetUnits(theUnits, theID, false);
            if (theUnits.size() == 0) {
                gContinentManager.GetConnectedIDs(theIDs, theID);
                EEAIContinent* theContinent;
                long i = theIDs.size();
                do {
                    if (--i < 0)
                        goto ranged;
                    theContinent = gContinentManager.mContinents[theIDs[i]];
                } while (!theContinent || theContinent->mCategory != kContinentCategorySpace);
                theFile->GetUnits(theUnits, theContinent->mID, false);
                if (theUnits.size() > 0) {
                    unsigned long theIndex = gPhysics->GetRandom(0L, theUnits.size() - 1);
                    if (theIndex < theUnits.size()) {
                        theTarget.mTile.mX = theUnits[theIndex]->mTileX;
                        theTarget.mTile.mY = theUnits[theIndex]->mTileY;
                        theTask.mType = kAGTaskUnit;
                        theTask.SetUnit(theUnits[theIndex]);
                        theTask.mLocation = theTarget;
                        theTask.mIsActive = true;
                    }
                }
            } else {
                unsigned long theIndex = gPhysics->GetRandom(0L, theUnits.size() - 1);
                if (theIndex < theUnits.size()) {
                    theTarget.mTile.mX = theUnits[theIndex]->mTileX;
                    theTarget.mTile.mY = theUnits[theIndex]->mTileY;
                    theTask.mType = kAGTaskUnit;
                    theTask.SetUnit(theUnits[theIndex]);
                    theTask.mLocation = theTarget;
                    theTask.mIsActive = true;
                }
            }
        }
    }
ranged:
    if (!theTask.mIsActive && gPhysics->GetRandom() <= 1.0f) {
        unsigned short theID = inSubGroup->GetMainContinent();
        if (this->mCommander->GetCombatManager()->mDefendTasks.size() > 0) {
            float theChance = 0.9f;
            if (0.9f <= this->mCommander->GetCombatManager()->mDefendTasks.size() * 0.05f + 0.5f)
                theChance = this->mCommander->GetCombatManager()->mDefendTasks.size() * 0.05f + 0.5f;
            if (gPhysics->mRandom.Generate() < theChance)
                theFound = theTask.mIsActive = this->mCommander->GetCombatManager()->ChooseTask(theTask, false, true, false, theID);
        }
        if (!theFound) {
            EEAIContinent* theContinent = gContinentManager.GetContinentByID(theID);
            if (theContinent) {
                if (theContinent->mCategory == kContinentCategorySpace) {
                    theTarget.mSerial = theContinent->mSerial;
                    theTarget.mType = kAIAreaContinent;
                    theTarget.mTile = theContinent->GetRandomPoint();
                    theTask.mType = kAGTaskLocation;
                    theTask.mLocation = theTarget;
                    theTask.mIsActive = true;
                } else {
                    gContinentManager.GetConnectedIDs(theIDs, theID);
                    for (long i = theIDs.size() - 1; i >= 0; i--) {
                        EEAIContinent* theOther = gContinentManager.GetContinentByID(theIDs[i]);
                        if (theOther && theOther->mCategory == kContinentCategorySpace) {
                            theTarget.mSerial = theOther->mSerial;
                            theTarget.mType = kAIAreaContinent;
                            theTarget.mTile = theOther->GetRandomPoint();
                            theTask.mType = kAGTaskLocation;
                            theTask.mLocation = theTarget;
                            theTask.mIsActive = true;
                        }
                    }
                }
            }
        }
    }
    if (!theTask.mIsActive) {
        theTask.mType = kAGTaskReturn;
        theTask.mLocation = inSubGroup->mRallyPoint;
        theTask.mIsActive = true;
    }
    if (!theTask.mIsActive) {
        theTask.mType = kAGTaskReturn;
        theTask.mLocation = EECPILocation(inSubGroup->GetPosition());
        theTask.mIsActive = true;
    }
    inSubGroup->SetTask(theTask);
}

// FUNCTION: EXE 0x455cdd
// When the group's target player changes state: on true sends every sub-group home and disbands the group,
// otherwise marks every sub-group out of the fight
void EECPAttackGroup::TargetPlayerNotice(EEPlayer* inPlayer, bool inFlag)
{
    // inPlayer is the target player (compared with mTask.mPlayer), inFlag a bool
    if (!inPlayer || this->mTask.mPlayer != inPlayer)
        return;
    if (inFlag) {
        for (std::vector<EECPAttackSubGroup*>::iterator theIt = this->mSubGroups.begin(); theIt != this->mSubGroups.end();
             theIt++) {
            EECPAttackSubGroup* theSubGroup = *theIt;
            theSubGroup->ReturnToRallyPoint();
            this->DropOrdersOf(theSubGroup);
            theSubGroup->Disband();
            theSubGroup->Shutdown();
            theSubGroup->Dereference();
        }
        this->mSubGroups.clear();
        this->mIsFinished = true;
    } else {
        for (std::vector<EECPAttackSubGroup*>::iterator theIt = this->mSubGroups.begin(); theIt != this->mSubGroups.end();
             theIt++)
            (*theIt)->SetOutOfFight(true);
    }
}

// FUNCTION: EXE 0x455d55
// Moves every unit of one sub-group into another of the same kind; returns false for a NULL or mismatched pair
bool EECPAttackGroup::MergeSubGroups(EECPAttackSubGroup* inInto, EECPAttackSubGroup* inFrom)
{
    bool theResult;
    if (inInto && inFrom && inInto->mKind == inFrom->mKind) {
        theResult = true;
        for (unsigned long i = 0; i < (unsigned long)inFrom->GetNumUnits(); i++)
            inInto->AddUnit(inFrom->GetUnitUnit(i));
        inFrom->ReleaseUnits();
    } else
        theResult = false;
    return theResult;
}

// FUNCTION: EXE 0x455dbd
// Makes one sub-group per kind among the selected units and unit classes, sets its mission from the strategy,
// requests its units and gives it the group's task (a territory task when the target is only an area)
void EECPAttackGroup::MakeSubGroups(EECPUnitSelectionGroup& inGroup, std::vector<EECPAttackSubGroup*>& outSubGroups)
{
    typedef std::map<long, std::pair<std::vector<long>, std::vector<long> > > EECPAGKindMap;
    EECPAGKindMap theKinds;
    EECPAGKindMap::iterator theIt;
    long theKind = 100;
    EECPILocation theLocation(this->mTask.mLocation);
    EECPAGTask theTask(this->mTask);
    EECPAttackSubGroup* theSubGroup = NULL;
    if (this->mTask.mType != kAGTaskUnit && this->mTask.mType != kAGTaskDefendWonder && this->mTask.mType != kAGTaskAttackWonder && this->mTask.mType != kAGTaskAttackLocation && this->mTask.mType != kAGTaskLocation &&
        (theLocation.mTile.mX != 0 || theLocation.mTile.mY != 0 || theLocation.mSerial != 0)) {
        if (theTask.mLocation.mTile.mX == 0 && theTask.mLocation.mTile.mY == 0)
            theLocation.MoveIntoArea();
        theTask.mLocation = theLocation;
        theTask.mType = kAGTaskTerritory;
        theTask.mIsActive = true;
    }
    for (long i = inGroup.mUnitIDs.size() - 1; i >= 0; i--) {
        EEUnit* theUnit = gWorld.GetUnit(inGroup.mUnitIDs[i]);
        if (!theUnit || !theUnit->mTileNode)
            continue;
        theKind = this->GetSubGroupKind(theUnit->mUnitClass);
        theIt = theKinds.find(theKind);
        if (theIt == theKinds.end()) {
            std::pair<EECPAGKindMap::iterator, bool> theResult = theKinds.insert(std::make_pair(theKind,
                std::pair<std::vector<long>, std::vector<long> >(std::vector<long>(), std::vector<long>())));
            if (theResult.second == true) {
                theIt = theResult.first;
                theIt->second.second.push_back(theUnit->mUnitID);
            }
        } else
            theIt->second.second.push_back(theUnit->mUnitID);
    }
    for (long j = inGroup.mSelected.size() - 1; j >= 0; j--) {
        if (!gUnitClassTable[inGroup.mSelected[j]])
            continue;
        theKind = this->GetSubGroupKind(gUnitClassTable[inGroup.mSelected[j]]);
        theIt = theKinds.find(theKind);
        if (theIt == theKinds.end()) {
            std::pair<EECPAGKindMap::iterator, bool> theResult = theKinds.insert(std::make_pair(theKind,
                std::pair<std::vector<long>, std::vector<long> >(std::vector<long>(), std::vector<long>())));
            if (theResult.second == true) {
                theIt = theResult.first;
                theIt->second.first.push_back(inGroup.mSelected[j]);
            }
        } else
            theIt->second.first.push_back(inGroup.mSelected[j]);
    }
    for (theIt = theKinds.begin(); theIt != theKinds.end(); theIt++) {
        switch (theIt->first) {
        case kASGKindLand:
            theSubGroup = new EECPASGLand(this, 0);
            break;
        case kASGKindSea:
            theSubGroup = new EECPASGSea(this, kASGKindSea);
            break;
        case kASGKindAir:
            theSubGroup = new EECPASGAir(this, kASGKindAir);
            break;
        case kASGKindSeaTransport:
            theSubGroup = new EECPASGSeaTransport(this, kASGKindSeaTransport);
            break;
        case kASGKindAirTransport:
            theSubGroup = new EECPASGAirTransport(this, kASGKindAirTransport);
            break;
        case kASGKindHelicopter:
            theSubGroup = new EECPASGHelicopter(this, kASGKindHelicopter);
            break;
        case 8:
            theSubGroup = new EECPASGHelicopter(this, 8);
            break;
        case 7:
            theSubGroup = new EECPASGSea(this, 7);
            break;
        case kASGKindSubmarine:
            theSubGroup = new EECPASGSubmarine(this, kASGKindSubmarine);
            break;
        case kASGKindSpaceship:
            theSubGroup = new EECPASGSpaceship(this, kASGKindSpaceship);
            break;
        case kASGKindSpaceTransport:
            theSubGroup = new EECPASGSpaceTransport(this, kASGKindSpaceTransport);
            break;
        case kASGKindSpaceFighter:
            theSubGroup = new EECPASGSpaceFighter(this, kASGKindSpaceFighter);
            break;
        }
        switch (inGroup.mStrategy->mKind) {
        case 0:
        case 4:
            theSubGroup->mMission = 2;
            break;
        case 1:
        case 5:
            theSubGroup->mMission = 1;
            break;
        }
        theSubGroup->RequestUnits(theIt->second.first, theIt->second.second, inGroup.mStrategy->mContinentID,
            inGroup.mStrategy->mTownID);
        outSubGroups.push_back(theSubGroup);
        if (theSubGroup->IsTaskActive(theTask)) {
            theSubGroup->SetTask(theTask);
        }
    }
}

// FUNCTION: EXE 0x456416
// Makes the planned sub-groups and adds them to the group; for a non-location task makes none unless the target
// player has known targets
void EECPAttackGroup::PlaceSubGroups(EECPUnitSelectionGroup& inGroup)
{
    std::vector<EECPAttackSubGroup*> theSubGroups;
    std::vector<EECPILocation> theTargets;
    EECPIntelligencePlayerFile* theFile = this->mCommander->GetIntelligence()->GetFile(this->mTask.GetPlayer());
    if (this->mTask.mType != kAGTaskLocation && this->mTask.mType != kAGTaskUnit && (this->mTask.mType <= 9 || this->mTask.mType > kAGTaskAttackWonder)) {
        theFile->GetTargets(theTargets, -1);
        if (theTargets.size() <= 0)
            goto done;
        if (!this->mTask.mLocation.IsOnMap())
            this->mTask.mLocation = EECPILocation();
    }
    this->MakeSubGroups(inGroup, theSubGroups);
done:
    for (unsigned long i = 0; i < theSubGroups.size(); i++)
        this->mSubGroups.push_back(theSubGroups[i]);
}

// FUNCTION: EXE 0x4564f6
// Creates the sub-group entry for a unit, choosing its kind (ship, carrier, bomber, fighter, transport, priest,
// prophet, hero, space...) from its class; a generic or empty entry when asked or for no unit
EECPAttackSubGroupUnit* EECPCreateSubGroupUnit(EEComplexUnit* inUnit, EECPAttackSubGroup* inSubGroup, bool inGeneric)
{
    long theKind = 0;
    if (!inGeneric && inUnit) {
        EEUnitClass* theClass = inUnit->mUnitClass;
        if (!theClass->IsTransport()) {
            if (theClass->mDomains & kMovementSea)
                theKind = theClass->mAIKind != kAIKindAircraftCarrier ? 5 : 2;
            else if (theClass->mDomains & kMovementAir) {
                if (!theClass->mHasCalamityPowers || !theClass->mIsCyber) {
                    if (theClass->FliesOver())
                        theKind = 0xb;
                    else
                        theKind = theClass->mAIKind == kAIKindBomber ? 1 : 3;
                } else
                    theKind = 10;
            } else if (theClass->mDomains & kMovementSpace)
                theKind = (theClass->mAIKind == kAIKindSpaceCarrier) + 0xc;
            else {
                if (theClass->mHasCalamityPowers && theClass->mAIKind != kAIKindHero) {
                    if (theClass->GetUnitType() == kUnitTypeProphet)
                        theKind = 8;
                    else if (theClass->mIsCyber)
                        theKind = 10;
                }
                if (theKind == 0 && theClass->IsPriest()) {
                    if (theClass->GetUnitType() == kUnitTypePriest || theClass->GetUnitType() == kUnitTypeEmissary)
                        theKind = 6;
                    else if (theClass->mIsCyber)
                        theKind = 7;
                }
                if (theKind == 0 && theClass->mAIKind == kAIKindHero)
                    theKind = 9;
            }
        } else
            theKind = 4;
        return EECPCreateSubGroupUnitOfKind(inUnit, inSubGroup, theKind);
    }
    return EECPCreateSubGroupUnitOfKind(inUnit, inSubGroup, theKind);
}

// FUNCTION: EXE 0x45661e
// Creates a sub-group entry of the given kind for a unit, or an empty one when the unit is NULL (the plain entry
// for unknown kinds)
EECPAttackSubGroupUnit* EECPCreateSubGroupUnitOfKind(EEComplexUnit* inUnit, EECPAttackSubGroup* inSubGroup, long inKind)
{
    switch (inKind) {
    case kASGUKindDefault:  // its own block (same body as default): the orig table starts at 0 (cmp eax, 0xd; no dec)
        if (inUnit)
            return new EECPAttackSubGroupUnit(inUnit, inSubGroup);
        return new EECPAttackSubGroupUnit(inSubGroup);
    case kASGUKindAirBomber:
        if (inUnit)
            return new EECPASGUAirBomber(inUnit, inSubGroup);
        return new EECPASGUAirBomber(inSubGroup);
    case kASGUKindAircraftCarrier:
        if (inUnit)
            return new EECPASGUAircraftCarrier(inUnit, inSubGroup);
        return new EECPASGUAircraftCarrier(inSubGroup);
    case kASGUKindAirFighter:
        if (inUnit)
            return new EECPASGUAirFighter(inUnit, inSubGroup);
        return new EECPASGUAirFighter(inSubGroup);
    case kASGUKindTransport:
        if (inUnit)
            return new EECPASGUTransport(inUnit, inSubGroup);
        return new EECPASGUTransport(inSubGroup);
    case kASGUKindSea:
        if (inUnit)
            return new EECPASGUSea(inUnit, inSubGroup);
        return new EECPASGUSea(inSubGroup);
    case kASGUKindPriest:
        if (inUnit)
            return new EECPASGUPriest(inUnit, inSubGroup);
        return new EECPASGUPriest(inSubGroup);
    case kASGUKindMechPriest:
        if (inUnit)
            return new EECPASGUMechPriest(inUnit, inSubGroup);
        return new EECPASGUMechPriest(inSubGroup);
    case kASGUKindProphet:
        if (inUnit)
            return new EECPASGUProphet(inUnit, inSubGroup);
        return new EECPASGUProphet(inSubGroup);
    case kASGUKindHero:
        if (inUnit)
            return new EECPASGUHero(inUnit, inSubGroup);
        return new EECPASGUHero(inSubGroup);
    case kASGUKindMechProphet:
        if (inUnit)
            return new EECPASGUMechProphet(inUnit, inSubGroup);
        return new EECPASGUMechProphet(inSubGroup);
    case kASGUKindHelicopter:
        if (inUnit)
            return new EECPASGUHelicopter(inUnit, inSubGroup);
        return new EECPASGUHelicopter(inSubGroup);
    case kASGUKindSpace:
        if (inUnit)
            return new EECPASGUSpace(inUnit, inSubGroup);
        return new EECPASGUSpace(inSubGroup);
    case kASGUKindSpaceCarrier:
        if (inUnit)
            return new EECPASGUSpaceCarrier(inUnit, inSubGroup);
        return new EECPASGUSpaceCarrier(inSubGroup);
    default:
        if (inUnit)
            return new EECPAttackSubGroupUnit(inUnit, inSubGroup);
        return new EECPAttackSubGroupUnit(inSubGroup);
    }
    return NULL;
}

// FUNCTION: EXE 0x4569f8
// Cancels a sub-group's orders, removes, disbands and releases it, and marks the group finished once empty
void EECPAttackGroup::RemoveSubGroup(EECPAttackSubGroup* inSubGroup)
{
    this->DropOrdersOf(inSubGroup);
    std::vector<EECPAttackSubGroup*>::iterator theIt = std::find(this->mSubGroups.begin(), this->mSubGroups.end(), inSubGroup);
    if (theIt != this->mSubGroups.end()) {
        this->mSubGroups.erase(theIt);
        inSubGroup->Disband();
        inSubGroup->Shutdown();
        inSubGroup->Dereference();
    }
    if (!this->mIsFinished)
        this->mIsFinished = this->mSubGroups.size() == 0;
}

// FUNCTION: EXE 0x456a9c
// Returns the sub-group holding a unit, or NULL
EECPAttackSubGroup* EECPAttackGroup::FindHolder(EEUnit* inUnit)
{
    // slot +0x8c is declared with an EEComplexUnit* (inferred)
    EECPAttackSubGroup* theResult = NULL;
    for (long i = this->mSubGroups.size() - 1; i >= 0; i--) {
        if (this->mSubGroups[i]->HasUnit(static_cast<EEComplexUnit*>(inUnit))) {
            theResult = this->mSubGroups[i];
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x456ae6
// Appends every sub-group listing a unit to a list; returns the list's size
unsigned long EECPAttackGroup::ListUnit(EEComplexUnit* inUnit, std::vector<EECPAttackSubGroup*>& outGroups)
{
    for (long i = this->mSubGroups.size() - 1; i >= 0; i--)
        this->mSubGroups[i]->ListIfHasUnit(inUnit, outGroups);
    return outGroups.size();
}

// FUNCTION: EXE 0x456b35
// Appends the units of every sub-group not on an attack mission to a list
void EECPAttackGroup::CollectUnits(std::vector<EEComplexUnit*>& inUnits)
{
    for (long i = this->mSubGroups.size() - 1; i >= 0; i--)
        if (this->mSubGroups[i]->mMission != 2)
            this->mSubGroups[i]->CollectUnits(inUnits);
}

// FUNCTION: EXE 0x456b73
// Fills a list with the target player's known target locations
void EECPAttackGroup::GetTargetLocations(std::vector<EECPILocation>& outTargets)
{
    EECPIntelligencePlayerFile* theFile = this->mCommander->GetIntelligence()->GetFile(this->mTask.GetPlayer());
    outTargets.clear();
    theFile->GetTargets(outTargets, -1);
    this->FilterTargets(outTargets);
}

// Filters the target list: does nothing
// (no address: GetTargetLocations calls 0x557269, the ICF representative of an identical empty `ret 4`, so this body was folded
// away; defined here for the link. Its source position is not known)
void EECPAttackGroup::FilterTargets(std::vector<EECPILocation>& ioTargets)
{
}

// FUNCTION: EXE 0x456bac
// Returns the sub-group with an id, or NULL
EECPAttackSubGroup* EECPAttackGroup::GetSubGroup(unsigned long inID)
{
    EECPAttackSubGroup* theResult = NULL;
    for (long i = this->mSubGroups.size() - 1; i >= 0; i--)
        if (this->mSubGroups[i]->mID == inID) {
            theResult = this->mSubGroups[i];
            break;
        }
    return theResult;
}

// FUNCTION: EXE 0x456bee
// Returns the sub-group kind for a unit class: land (and heroes), sea, submarine, hurricane, air, helicopter,
// storm, transports or space kinds; 100 when it moves in none of these
long EECPAttackGroup::GetSubGroupKind(EEUnitClass* inClass)
{
    long theKind = 100;
    if ((inClass->mDomains & kMovementLand) || inClass->mAIKind == kAIKindHero)
        theKind = 0;
    else if (inClass->mDomains & kMovementSea) {
        if (inClass->GetUnitType() != kUnitTypeHurricane) {
            if (!inClass->IsTransport())
                theKind = inClass->mCategory == kUnitCategorySubmergedNaval ? 6 : 1;
            else
                theKind = 3;
        } else
            theKind = 7;
    } else if (inClass->mDomains & kMovementAir) {
        if (inClass->GetUnitType() != kUnitTypeAntiMatterStorm) {
            if (!inClass->IsTransport())
                theKind = inClass->FliesOver() ? 5 : 2;
            else
                theKind = 4;
        } else
            theKind = 8;
    } else if (inClass->mDomains & kMovementSpace) {
        theKind = inClass->IsTransport() ? 10 : inClass->mCategory == kUnitCategoryPlanetSpaceship ? 0xb : 9;
    }
    return theKind;
}

// FUNCTION: EXE 0x456caf
// Returns true if the group has a continent and one of its sub-groups holds the unit
bool EECPAttackGroup::KeepsUnit(EEComplexUnit* inUnit)
{
    bool theResult = false;
    if (this->mContinentID != 0)
        theResult = this->FindHolder(inUnit) != NULL;
    return theResult;
}

// FUNCTION: EXE 0x456cc8
// Reads the task, the sub-groups (kind, then body), the orders, id, continent and finished flag from the saved
// game; returns the first read error
long EECPAttackGroup::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    unsigned long i = 0;
    unsigned long theCount = 0;
    long theKind = 0;
    // the task's result counts too (it used to be ignored)
    long theResult = this->mTask.Load(inMajorVersion, inMinorVersion, inFile);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theCount);
    if (theResult == kErrorOK) {
        this->mSubGroups.resize(theCount);
        for (unsigned long j = 0; j < theCount; j++) {
            EECPAttackSubGroup* theSubGroup = NULL;
            theKind = 0;
            if (theResult == kErrorOK)
                theResult = inFile.Read(theKind);
            // after a read error there is no sub-group to create (a NULL one used to be referenced): keep the
            // ones read so far and report the error
            if (theResult != kErrorOK) {
                this->mSubGroups.resize(j);
                return theResult;
            }
            switch (theKind) {
            case kASGKindLand:
                theSubGroup = new EECPASGLand(this, theKind);
                break;
            case kASGKindSea:
            case 7:
                theSubGroup = new EECPASGSea(this, theKind);
                break;
            case kASGKindAir:
                theSubGroup = new EECPASGAir(this, theKind);
                break;
            case kASGKindSeaTransport:
                theSubGroup = new EECPASGSeaTransport(this, theKind);
                break;
            case kASGKindAirTransport:
                theSubGroup = new EECPASGAirTransport(this, theKind);
                break;
            case kASGKindHelicopter:
            case 8:
                theSubGroup = new EECPASGHelicopter(this, theKind);
                break;
            case kASGKindSubmarine:
                theSubGroup = new EECPASGSubmarine(this, theKind);
                break;
            case kASGKindSpaceship:
                theSubGroup = new EECPASGSpaceship(this, theKind);
                break;
            case kASGKindSpaceTransport:
                theSubGroup = new EECPASGSpaceTransport(this, theKind);
                break;
            case kASGKindSpaceFighter:
                theSubGroup = new EECPASGSpaceFighter(this, theKind);
                break;
            }
            this->mSubGroups[j] = theSubGroup;
            theSubGroup->Reference();
            theSubGroup->Dereference();
            if (theResult == kErrorOK)
                theResult = theSubGroup->Load(inMajorVersion, inMinorVersion, inFile);
        }
        theCount = 0;
        // a sub-group read error is kept (the order count's result used to replace it)
        if (theResult == kErrorOK)
            theResult = inFile.Read(theCount);
        if (theResult == kErrorOK) {
            this->mOrders.resize(theCount);
            if (theCount != 0) {
                do {
                    if (theResult == kErrorOK)
                        theResult = this->mOrders[i].Load(inMajorVersion, inMinorVersion, inFile);
                    i++;
                } while (i < theCount);
                if (theResult != kErrorOK)
                    return theResult;
            }
            theResult = inFile.Read(this->mID);
            if (theResult == kErrorOK) {
                theResult = inFile.Read(this->mContinentID);
                if (theResult == kErrorOK)
                    theResult = inFile.Read(this->mIsFinished);
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x456f9e
// Cancels every pending order naming a sub-group
void EECPAttackGroup::DropOrdersOf(EECPAttackSubGroup* inSubGroup)
{
    if (!inSubGroup)
        return;
    for (long i = this->mOrders.size() - 1; i >= 0; i--)
        if (this->mOrders[i].mSubGroup == inSubGroup || this->mOrders[i].mOtherSubGroup == inSubGroup)
            this->mOrders[i].mIsActive = false;
}

// FUNCTION: EXE 0x457017
// Picks the best-scoring target the air sub-group can reach (any sub-group else takes the first target); returns
// false when there is none
bool EECPAttackGroup::FindAirTarget(EECPAttackSubGroup* inSubGroup, EECPILocation& inFrom, EECPILocation& outTo)
{
    EEAIFOBestInfluenceRegionToAirStrike theScorer(inSubGroup, inFrom.mTile, gPhysics->GetRandom());
    std::multimap<float, EECPILocation, std::greater<float> > theScores;
    std::vector<EECPILocation> theTargets;
    std::vector<EECPILocation> theStrikes;
    EECPILocation theFrom(inFrom);
    EECPASGAir* theAir = dynamic_cast<EECPASGAir*>(inSubGroup);
    bool theResult = false;
    outTo.Clear();
    this->GetTargetLocations(theTargets);
    if (theTargets.size() > 0) {
        if (theAir) {
            theAir->FilterReachableLocations(theTargets, theStrikes);
            for (long i = theStrikes.size() - 1; i >= 0; i--) {
                float theScore = theScorer(*(theStrikes.begin() + i));
                theScores.insert(std::make_pair(theScore, *(theStrikes.begin() + i)));
            }
            if (theScores.size() > 0) {
                outTo = theScores.begin()->second;
                theResult = true;
            }
        } else {
            theResult = true;
            outTo = (*(theTargets.begin() + 0));
        }
    }
    return theResult;
}

// (not emitted here) 0x4571dd
// EECPILocation::~EECPILocation (inline, ee/EECPILocation.h; out of line for the score map's pairs)

// FUNCTION: EXE 0x4571e4
// For a group with no continent, picks the best-scoring target region the sub-group can reach (the first target
// with no sub-group); returns false when there is none
bool EECPAttackGroup::FindLandTarget(EECPAttackSubGroup* inSubGroup, EECPILocation& inFrom, EECPILocation& outTo)
{
    EEAIFOBestInfluenceRegionToAttack theScorer(inSubGroup, inFrom.mTile);
    std::multimap<float, EECPILocation, std::greater<float> > theScores;
    std::multimap<float, EECPILocation, std::greater<float> >::iterator theIt;
    std::vector<EECPILocation> theTargets;
    EECPILocation theFrom(inFrom);
    bool theResult = false;
    outTo.Clear();
    if (this->mContinentID == 0 && theFrom.IsValid()) {
        this->GetTargetLocations(theTargets);
        if (theTargets.size() > 0) {
            if (inSubGroup) {
                for (long i = theTargets.size() - 1; i >= 0; i--) {
                    float theScore = theScorer(theTargets[i]);
                    theScores.insert(std::make_pair(theScore, theTargets[i]));
                }
                for (theIt = theScores.begin(); theIt != theScores.end(); ++theIt) {
                    if (!inSubGroup->CanReach(&theIt->second))
                        continue;
                    outTo = theIt->second;
                    theResult = true;
                    break;
                }
            } else {
                outTo = (theTargets[0]);
                theResult = true;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x45737e
// Finds no sea target: clears the result and returns false
bool EECPAttackGroup::FindSeaTarget(EECPAttackSubGroup* inSubGroup, EECPILocation& inFrom, EECPILocation& outTo)
{
    outTo.Clear();
    return false;
}

// FUNCTION: EXE 0x45738c
// Finds no space target (always false)
bool EECPAttackGroup::FindSpaceTarget(EECPAttackSubGroup* inSubGroup, EECPILocation& inFrom, EECPILocation& outTo)
{
    return false;
}

// FUNCTION: EXE 0x457391
// Selects units for the group's task with a strategy, buys them improvements when allowed (not on easy), makes
// the sub-groups and marks the group finished if none were made
void EECPAttackGroup::MakePlan(EECPCombatStrategy& inStrategy, std::vector<long>& outUnitIDs)
{
    EECPUnitSelectionGroup theGroup;
    theGroup.mLocation = this->mTask.mLocation;
    theGroup.mPlayer = this->mCommander->mPlayer;
    theGroup.mTarget = this->mTask.mPlayer;
    theGroup.mUnitIDs = outUnitIDs;
    theGroup.mStrategy = inStrategy.Clone();
    theGroup.mTask = this->mTask;
    this->mCommander->mPlayer->mCombatManager->mUnitSelector->Select(&theGroup);
    if (gUnitImprovementsAllowed && this->mCommander->GetCombatManager()->mImprovementsEnabled && this->mCommander->mPlayer->mDifficulty != kDifficultyEasy)
        this->GiveImprovements(theGroup);
    this->PlaceSubGroups(theGroup);
    this->mIsFinished = this->mSubGroups.size() == 0;
}

// FUNCTION: EXE 0x457470
// Creates an empty unit selection with no players or strategy
EECPUnitSelectionGroup::EECPUnitSelectionGroup()
{
    this->mPlayer = NULL;
    this->mTarget = NULL;
    this->mTargetFile = NULL;
    this->mPlayerFile = NULL;
    this->mStrategy = NULL;
}

// SYNTHETIC: EXE 0x45750e
// (EEAIFOPAvailablePersonnelGatherer's scalar deleting destructor: its inline dtor is declared in ee/EECPUnitSelector.h)

// FUNCTION: EXE 0x457533
// Destroys the selection, deleting its strategy
EECPUnitSelectionGroup::~EECPUnitSelectionGroup()
{
    if (this->mStrategy)
        delete this->mStrategy;
}

// SYNTHETIC: EXE 0x45759f
// (EECPUnitSelectionGroup's scalar deleting destructor)

// FUNCTION: EXE 0x4575bb
// Refreshes each sub-group's task location; a continent group then moves to the continent holding most of its
// units
void EECPAttackGroup::Update()
{
    std::map<unsigned short, long> theCounts;
    std::map<unsigned short, long>::iterator theIt;
    unsigned short theID = 0;
    for (long i = this->mSubGroups.size() - 1; i >= 0; i--) {
        this->mSubGroups[i]->RefreshTaskLocation();
        if (this->mContinentID == 0)
            continue;
        theID = this->mSubGroups[i]->GetMainContinent();
        if (theID == 0)
            continue;
        theIt = theCounts.find(theID);
        if (theIt != theCounts.end())
            theIt->second += this->mSubGroups[i]->GetNumUnits();
        else
            theCounts.insert(std::map<unsigned short, long>::value_type(theID, this->mSubGroups[i]->GetNumUnits()));
    }
    long theBest = 0;
    if (this->mContinentID == 0)
        return;
    theID = 0;
    for (theIt = theCounts.begin(); theIt != theCounts.end(); theIt++) {
        if (theID == 0 || theIt->second > theBest) {
            theID = theIt->first;
            theBest = theIt->second;
        }
    }
    this->mContinentID = theID;
}

// FUNCTION: EXE 0x457727
// Carries out a queued order (check unit, split, remove, set state, merge, travel, assign task, return home);
// returns true
bool EECPAttackGroup::ExecuteOrder(EECPAGOrder& inOrder, unsigned long inTime)
{
    switch (inOrder.mType) {
    case kAGOrderCheckUnit:
        this->CheckUnit(inOrder);
        break;
    case kAGOrderSplitSubGroup:
        this->SplitSubGroup(inOrder.mSubGroup);
        break;
    case kAGOrderRemoveSubGroup:
        this->RemoveSubGroup(inOrder.mSubGroup);
        break;
    case kAGOrderSetState:
        inOrder.mSubGroup->SetState(inOrder.mState, inTime);
        break;
    case kAGOrderMergeSubGroups:
        if (inOrder.mOtherSubGroup)
            this->MergeSubGroups(inOrder.mSubGroup, inOrder.mOtherSubGroup);
        break;
    case kAGOrderPrepareTravel:
        inOrder.mSubGroup->SetState(kASGStatePrepareTravel, inTime);
        break;
    case kAGOrderAssignTask:
        this->AssignTask(inOrder.mSubGroup);
        break;
    case kAGOrderReturnToRallyPoint:
        inOrder.mSubGroup->SetState(kASGStateReturnToRallyPoint, inTime);
        break;
    }
    return true;
}

// FUNCTION: EXE 0x4577d4
// Removes a unit from the group
bool EECPAttackGroup::ReleaseUnit(EEComplexUnit* inUnit)
{
    return this->RemoveUnit(inUnit);
}

// FUNCTION: EXE 0x4577e0
// Removes an order's unit from its sub-group if it still exists, and removes the sub-group once empty
void EECPAttackGroup::CheckUnit(EECPAGOrder& inOrder)
{
    EEComplexUnit* theUnit = dynamic_cast<EEComplexUnit*>(gWorld.GetUnit(inOrder.GetUnitID()));
    if (theUnit) {
        inOrder.mSubGroup->RemoveUnit(theUnit);
    }
    if (inOrder.mSubGroup->GetNumUnits() == 0)
        this->RemoveSubGroup(inOrder.mSubGroup);
}

// FUNCTION: EXE 0x457838
// Restores the task, sub-groups and orders after loading, then drops empty sub-groups without a request (all of
// them, and the continent, when the task has no player); returns the first error
long EECPAttackGroup::Restore(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    long theResult = this->mTask.Restore(inMajorVersion, inMinorVersion);
    unsigned long i = 0;
    if (theResult != 0)
        return theResult;
    bool theHasPlayer = this->mTask.mPlayer != NULL;
    theResult = 0;
    for (; i < this->mSubGroups.size(); i++)
        if (theResult == 0)
            theResult = this->mSubGroups[i]->Restore(inMajorVersion, inMinorVersion);
    i = 0;
    if (this->mOrders.size() > 0) {
        do {
            if (theResult == 0)
                theResult = this->mOrders[i].Restore(inMajorVersion, inMinorVersion, this);
            i++;
        } while (i < this->mOrders.size());
    }
    unsigned long j = 0;
    while (j < this->mSubGroups.size()) {
        if (!theHasPlayer || this->mSubGroups[j]->GetNumUnits() == 0 && this->mSubGroups[j]->mRequest == NULL) {
            long theCount = this->mSubGroups.size();
            this->RemoveSubGroup(this->mSubGroups[j]);
            if (theCount != (long)this->mSubGroups.size()) {
                j = 0;
                continue;
            }
        }
        j++;
    }
    if (!theHasPlayer)
        this->mContinentID = 0;
    return theResult;
}

// FUNCTION: EXE 0x457962
// Gives a sub-group a new task by its kind: land, sea, air or space
void EECPAttackGroup::AssignTask(EECPAttackSubGroup* inSubGroup)
{
    if (dynamic_cast<EECPASGLand*>(inSubGroup))
        this->AssignLandTask(inSubGroup);
    else if (dynamic_cast<EECPASGSea*>(inSubGroup))
        this->AssignSeaTask(inSubGroup);
    else if (dynamic_cast<EECPASGAir*>(inSubGroup))
        this->AssignAirTask(inSubGroup);
    else if (dynamic_cast<EECPASGSpaceship*>(inSubGroup))
        this->AssignSpaceTask(inSubGroup);
}

// The selected transport classes and the land units already in the selection go to the new sub group, which takes
// inLand
// FUNCTION: EXE 0x4579f4
// Creates a transport sub-group for a land sub-group's cargo: space (space epoch and map), else air (atomic
// epoch, with a carrier), else sea, sized by the cargo; returns it, or NULL
EECPASGTransport* EECPAttackGroup::FindLandTransport(EECPASGLand* inLand)
{
    std::vector<long> theUnitIDs;
    std::vector<unsigned short> theAdjacent;
    EECPCombatStrategy theStrategy;
    EECPUnitRequirement theRequirement;
    EECPUnitSelectionGroup theGroup;
    EECPAttackSubGroup* theSubGroup = NULL;
    EECPASGTransport* theTransport = NULL;
    EECPASGLand* theLand = dynamic_cast<EECPASGLand*>(inLand);
    long theNeedA = 0;
    long theSeaNeed = 0;
    long theAirNeed = 0;
    long theSpaceNeed = 0;
    theGroup.mPlayer = this->mCommander->mPlayer;
    theGroup.mTarget = this->mTask.mPlayer;
    unsigned short theContinentID = 0;
    long theCapacity = 0;
    if (theLand) {
        EEAIContinent* theContinent =
            gContinentManager.GetContinentAt(theLand->mPickupLocation.mTile.mX, theLand->mPickupLocation.mTile.mY);
        theLand->GetTransportNeeds(theNeedA, theSeaNeed, theAirNeed, theSpaceNeed);
        EEMap* theMap = gWorld.mMap;
        if (theMap->mTerrainMesh.GetIsSpaceMap() && this->mCommander->mPlayer->GetTechTree()->mCurrentEpoch >= kTechEpochSpace &&
            theSpaceNeed > 0 && (theContinent == NULL || theContinent->mCategory != kContinentCategoryCliff)) {
            theStrategy.Reset();
            long theClass = this->mCommander->mPlayer->mCivilizationManager->GetTransportClass(false, false, true);
            if (theClass && gUnitClassTable[theClass] && (theCapacity = gUnitClassTable[theClass]->mTransportCapacity) > 0) {
                theStrategy.Reset();
                float theCount = UMCeil((float)theSpaceNeed / theCapacity);
                theStrategy.mNumUnits = theStrategy.mNumRemaining = (long)theCount;
                theStrategy.mAddFavorites = false;
                theStrategy.mKind = 0;
                theRequirement.Clear();
                theRequirement.mCategory = 0x21;
                theRequirement.mKind = kRequirementAlreadyDefined;
                theStrategy.mRequirements.push_back(theRequirement);
                theGroup.mStrategy = theStrategy.Clone();
                this->mCommander->mPlayer->mCombatManager->mUnitSelector->Select(&theGroup);
                if (theGroup.mSelected.size() > 0) {
                    theSubGroup = new EECPASGSpaceTransport(this, kASGKindSpaceTransport);
                    theSubGroup->Reference();
                    theSubGroup->Dereference();
                    theTransport = dynamic_cast<EECPASGTransport*>(theSubGroup);
                    long theTotal = 0;
                    long i;
                    for (i = theGroup.mSelected.size() - 1; i >= 0; i--) {
                        EEUnitClass* theSelected = gUnitClassTable[theGroup.mSelected[i]];
                        if (theSelected)
                            theTotal += theSelected->mTransportCapacity;
                    }
                    if (theTotal > 0 && theSpaceNeed > 0 && theTotal < theSpaceNeed &&
                        (float)theTotal / theSpaceNeed < 1.0f)
                        theGroup.mSelected.push_back(*theGroup.mSelected.begin());
                    if ((theLand->mEmbarkTile.mX || theLand->mEmbarkTile.mY) && (theLand->mLandingTile.mX || theLand->mLandingTile.mY)) {
                        unsigned short theFirst = gContinentManager.GetContinentID(theLand->mEmbarkTile.mX, theLand->mEmbarkTile.mY);
                        unsigned short theSecond = gContinentManager.GetContinentID(theLand->mLandingTile.mX, theLand->mLandingTile.mY);
                        EEAIFOContinentScreenContinentsShareAdjacentOceanContinentID theScreen(theFirst, theSecond, true);
                        gContinentManager.GetAdjacentContinents(theFirst, theSecond, theAdjacent, &theScreen);
                        if (theAdjacent.size() > 0)
                            theContinentID = theAdjacent[0];
                    }
                    for (i = theGroup.mUnitIDs.size() - 1; i >= 0; i--) {
                        EEUnit* theUnit = static_cast<EEUnit*>(gWorld.GetUnit(theGroup.mUnitIDs[i]));
                        if (theUnit && (theUnit->mUnitClass->mDomains & kMovementLand)) {
                            theUnitIDs.push_back(theUnit->mUnitID);
                            theGroup.mUnitIDs.erase(theGroup.mUnitIDs.begin() + i);
                        }
                    }
                    theSubGroup->RequestUnits(theGroup.mSelected, theUnitIDs, theContinentID, 0);
                    theTransport->SetLand(dynamic_cast<EECPASGLand*>(inLand));
                    this->mSubGroups.push_back(theSubGroup);
                }
            }
        }
        if (theSubGroup == NULL && this->mCommander->mPlayer->GetTechTree()->mCurrentEpoch >= kTechEpochAtomicModern && theAirNeed > 0 &&
            (theMap->mMapCharacteristics & kRMMapPlanetsMap) != kRMMapPlanetsMap) {
            std::deque<EEComplexUnit*>* theCarriers = this->mCommander->mPlayer->mCivilizationManager->mUnitManager->GetUnits(0x42);
            bool theCarrier = true;
            if (theCarriers == NULL || theCarriers->size() == 0) {
                long theCarrierClass = this->mCommander->mPlayer->mCivilizationManager->ChooseTransportClass();
                theCarrier = theCarrierClass && gUnitClassTable[theCarrierClass] &&
                             (gUnitClassTable[theCarrierClass]->mDomains & kMovementAir);
            }
            if (theCarrier) {
                theStrategy.Reset();
                long theClass = this->mCommander->mPlayer->mCivilizationManager->GetTransportClass(false, true, false);
                if (theClass && gUnitClassTable[theClass] && (theCapacity = gUnitClassTable[theClass]->mTransportCapacity) > 0) {
                    float theCount = UMCeil((float)theAirNeed / theCapacity);
                    theStrategy.mNumUnits = theStrategy.mNumRemaining = (long)theCount;
                    theStrategy.mAddFavorites = false;
                    theStrategy.mKind = 0;
                    theRequirement.Clear();
                    theRequirement.mCategory = kAIKindNavalTransport;
                    theRequirement.mKind = kRequirementAlreadyDefined;
                    theStrategy.mRequirements.push_back(theRequirement);
                    theGroup.mStrategy = theStrategy.Clone();
                    this->mCommander->mPlayer->mCombatManager->mUnitSelector->Select(&theGroup);
                    if (theGroup.mSelected.size() > 0) {
                        theSubGroup = new EECPASGAirTransport(this, kASGKindAirTransport);
                        theTransport = dynamic_cast<EECPASGTransport*>(theSubGroup);
                        long theTotal = 0;
                        long i;
                        for (i = theGroup.mSelected.size() - 1; i >= 0; i--) {
                            EEUnitClass* theSelected = gUnitClassTable[theGroup.mSelected[i]];
                            if (theSelected)
                                theTotal += theSelected->mTransportCapacity;
                        }
                        if (theTotal > 0 && theAirNeed > 0 && theTotal < theAirNeed && (float)theTotal / theAirNeed < 1.0f)
                            theGroup.mSelected.push_back(*theGroup.mSelected.begin());
                        for (i = theGroup.mUnitIDs.size() - 1; i >= 0; i--) {
                            EEUnit* theUnit = static_cast<EEUnit*>(gWorld.GetUnit(theGroup.mUnitIDs[i]));
                            if (theUnit && (theUnit->mUnitClass->mDomains & kMovementLand)) {
                                theUnitIDs.push_back(theUnit->mUnitID);
                                theGroup.mUnitIDs.erase(theGroup.mUnitIDs.begin() + i);
                            }
                        }
                        theSubGroup->RequestUnits(theGroup.mSelected, theUnitIDs, 0, 0);
                        theTransport->SetLand(dynamic_cast<EECPASGLand*>(inLand));
                        this->mSubGroups.push_back(theSubGroup);
                    } else
                        theLand->mNoTransport = true;
                }
            }
        }
        if (theSeaNeed > 0 && gDCGameSpeed > 0 && theSubGroup == NULL && (theContinent == NULL || theContinent->mCategory != kContinentCategoryCliff)) {
            theStrategy.Reset();
            long theClass = this->mCommander->mPlayer->mCivilizationManager->GetTransportClass(true, false, false);
            if (theClass && gUnitClassTable[theClass] && (theCapacity = gUnitClassTable[theClass]->mTransportCapacity) > 0) {
                theStrategy.Reset();
                float theCount = UMCeil((float)theSeaNeed / theCapacity);
                theStrategy.mNumUnits = theStrategy.mNumRemaining = (long)theCount;
                theStrategy.mAddFavorites = false;
                theStrategy.mKind = 0;
                theRequirement.Clear();
                theRequirement.mCategory = 0x12;
                theRequirement.mKind = kRequirementAlreadyDefined;
                theStrategy.mRequirements.push_back(theRequirement);
                theGroup.mStrategy = theStrategy.Clone();
                this->mCommander->mPlayer->mCombatManager->mUnitSelector->Select(&theGroup);
                if (theGroup.mSelected.size() != 0) {
                    theSubGroup = new EECPASGSeaTransport(this, kASGKindSeaTransport);
                    theSubGroup->Reference();
                    theSubGroup->Dereference();
                    theTransport = dynamic_cast<EECPASGTransport*>(theSubGroup);
                    long theTotal = 0;
                    long i;
                    for (i = theGroup.mSelected.size() - 1; i >= 0; i--) {
                        EEUnitClass* theSelected = gUnitClassTable[theGroup.mSelected[i]];
                        if (theSelected)
                            theTotal += theSelected->mTransportCapacity;
                    }
                    if (theTotal > 0 && theSeaNeed > 0 && theTotal < theSeaNeed && (float)theTotal / theSeaNeed < 1.0f)
                        theGroup.mSelected.push_back(*theGroup.mSelected.begin());
                    if ((theLand->mEmbarkTile.mX || theLand->mEmbarkTile.mY) && (theLand->mLandingTile.mX || theLand->mLandingTile.mY)) {
                        unsigned short theFirst = gContinentManager.GetContinentID(theLand->mEmbarkTile.mX, theLand->mEmbarkTile.mY);
                        unsigned short theSecond = gContinentManager.GetContinentID(theLand->mLandingTile.mX, theLand->mLandingTile.mY);
                        EEAIFOContinentScreenContinentsShareAdjacentOceanContinentID theScreen(theFirst, theSecond, false);
                        gContinentManager.GetAdjacentContinents(theFirst, theSecond, theAdjacent, &theScreen);
                        if (theAdjacent.size() != 0)
                            theContinentID = theAdjacent[0];
                    }
                    for (i = theGroup.mUnitIDs.size() - 1; i >= 0; i--) {
                        EEUnit* theUnit = static_cast<EEUnit*>(gWorld.GetUnit(theGroup.mUnitIDs[i]));
                        if (theUnit && (theUnit->mUnitClass->mDomains & kMovementLand)) {
                            theUnitIDs.push_back(theUnit->mUnitID);
                            theGroup.mUnitIDs.erase(theGroup.mUnitIDs.begin() + i);
                        }
                    }
                    theSubGroup->RequestUnits(theGroup.mSelected, theUnitIDs, theContinentID, 0);
                    theTransport->SetLand(dynamic_cast<EECPASGLand*>(inLand));
                    this->mSubGroups.push_back(theSubGroup);
                }
            }
        }
    }
    return theTransport;
}

// TEMPLATE: EXE 0x458554
// (EEAIFOContinentScreenBase::Virtual04, inline in ee/EEAIFunctionObjects.h)

// TEMPLATE: EXE 0x458560
// (EEAIFOContinentScreenContinentsShareAdjacentID's inline ctor)

// TEMPLATE: EXE 0x458584
// (EEAIFOContinentScreenBase::Test, inline)

// FUNCTION: EXE 0x458591
// Accepts one of the two continents, or a water (or, for the space variant, space) area adjacent to both
// category 4 (space) when mOcean (the space transport path), else 2 (water)
bool EEAIFOContinentScreenContinentsShareAdjacentOceanContinentID::Test(unsigned short inID)
{
    bool theResult = false;
    if (inID == this->mContinent1 || inID == this->mContinent2)
        theResult = true;
    else {
        const std::vector<EEAIContinent*>& theContinents = gContinentManager.mContinents;
        EEAIContinent* theContinent = theContinents[inID];
        unsigned char theKind = this->mOcean ? 4 : 2;
        if (theContinent && theContinent->mCategory == theKind) {
            theResult = gContinentManager.mTileFlags[this->mContinent1 * gContinentManager.mFlagsWidth + inID];
            if (theResult)
                theResult = gContinentManager.mTileFlags[this->mContinent2 * gContinentManager.mFlagsWidth + inID];
        }
    }
    return theResult;
}

// SYNTHETIC: EXE 0x458608
// (EEAIFOContinentScreenBase's scalar deleting destructor; the derived screens' fold into it)

// FUNCTION: EXE 0x458625
// Writes the task, the sub-groups (kind, then body), the orders, id, continent and finished flag to the saved
// game; returns the first write error
long EECPAttackGroup::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    unsigned long i = 0;
    unsigned long theCount = 0;
    long theKind;
    // the task's result counts too (it used to be ignored)
    long theResult = this->mTask.Save(inMajorVersion, inMinorVersion, outFile);
    theCount = this->mSubGroups.size();
    if (theResult == 0)
        theResult = outFile.Write(theCount);
    if (theResult == 0) {
        for (i = 0; i < theCount; i++) {
            theKind = this->mSubGroups[i]->mKind;
            if (theResult == 0)
                theResult = outFile.Write(theKind);
            if (theResult == 0)
                theResult = this->mSubGroups[i]->Save(inMajorVersion, inMinorVersion, outFile);
        }
    }
    theCount = this->mOrders.size();
    // a sub-group write error is kept (the order count's result used to replace it)
    if (theResult == 0)
        theResult = outFile.Write(theCount);
    if (theResult != 0)
        return theResult;
    for (i = 0; i < theCount; i++) {
        if (theResult == 0)
            theResult = this->mOrders[i].Save(inMajorVersion, inMinorVersion, outFile);
    }
    if (theResult == 0)
        theResult = outFile.Write(this->mID);
    if (theResult == 0)
        theResult = outFile.Write(this->mContinentID);
    if (theResult == 0)
        theResult = outFile.Write(this->mIsFinished);
    return theResult;
}

// FUNCTION: EXE 0x458741
// Splits the units a sub-group picks off into a new sub-group of their kind (dropped if it ends up empty); a
// one-unit sub-group gets its task deactivated instead
void EECPAttackGroup::SplitSubGroup(EECPAttackSubGroup* inSubGroup)
{
    std::vector<EEComplexUnit*> theUnits;
    EECPAttackSubGroup* theSubGroup = NULL;
    EECPAGOrder theOrder;
    if (!inSubGroup)
        return;
    if ((unsigned long)inSubGroup->GetNumUnits() > 1) {
        inSubGroup->PickSplitUnits(theUnits);
        if (theUnits.size() <= 0)
            return;
        switch (this->GetSubGroupKind(theUnits[0]->mUnitClass)) {
        case kASGKindLand:
            theSubGroup = new EECPASGLand(this, 0);
            break;
        case kASGKindSea:
            theSubGroup = new EECPASGSea(this, kASGKindSea);
            break;
        case 7:
            theSubGroup = new EECPASGSea(this, 7);
            break;
        case kASGKindSubmarine:
            theSubGroup = new EECPASGSubmarine(this, kASGKindSubmarine);
            break;
        case kASGKindAir:
            theSubGroup = new EECPASGAir(this, kASGKindAir);
            break;
        case kASGKindAirTransport:
            theSubGroup = new EECPASGAirTransport(this, kASGKindAirTransport);
            break;
        case kASGKindSeaTransport:
            theSubGroup = new EECPASGSeaTransport(this, kASGKindSeaTransport);
            break;
        case kASGKindHelicopter:
            theSubGroup = new EECPASGHelicopter(this, kASGKindHelicopter);
            break;
        case 8:
            theSubGroup = new EECPASGHelicopter(this, 8);
            break;
        case kASGKindSpaceship:
            theSubGroup = new EECPASGSpaceship(this, kASGKindSpaceship);
            break;
        case kASGKindSpaceTransport:
            theSubGroup = new EECPASGSpaceTransport(this, kASGKindSpaceTransport);
            break;
        case kASGKindSpaceFighter:
            theSubGroup = new EECPASGSpaceFighter(this, kASGKindSpaceFighter);
            break;
        }
        if (theSubGroup) {
            inSubGroup->SplitInto(theSubGroup, theUnits);
            if (theSubGroup->GetNumUnits() != 0)
                this->mSubGroups.push_back(theSubGroup);
            else {
                this->DropOrdersOf(theSubGroup);
                theSubGroup->Shutdown();
                theSubGroup->Dereference();
                theSubGroup = NULL;
            }
        }
    } else
        inSubGroup->mTask.mIsActive = false;
}

// FUNCTION: EXE 0x4589c4
// Replaces a unit by a new one in the first sub-group holding it; returns true if one did
bool EECPAttackGroup::ReplaceUnit(EEComplexUnit* inUnit, EEComplexUnit* inNewUnit)
{
    bool theResult = false;
    for (long i = this->mSubGroups.size() - 1; i >= 0; i--) {
        theResult = this->mSubGroups[i]->ReplaceUnit(inUnit, inNewUnit);
        if (theResult)
            break;
    }
    return theResult;
}

// FUNCTION: EXE 0x458a06
// Removes a unit from the sub-groups, queueing removal of a sub-group it empties; returns whether a
// sub-group held it
bool EECPAttackGroup::RemoveUnit(EEUnit* inUnit)
{
    EECPAGOrder theOrder;
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    bool theResult = false;
    for (long i = this->mSubGroups.size() - 1; i >= 0; i--) {
        theResult = this->mSubGroups[i]->RemoveUnit(theUnit);
        if (!theResult)
            continue;
        if (this->mSubGroups[i]->GetNumUnits() != 0)
            break;   // removed: done (was: go on asking, later answers overwrite the result)
        theOrder.mType = kAGOrderRemoveSubGroup;
        theOrder.SetSubGroup(this->mSubGroups[i]);
        theOrder.mIsActive = true;
        this->mOrders.push_back(theOrder);
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x458a9e
// Updates every sub-group that is due, then carries out and empties the order queue
void EECPAttackGroup::Tick(unsigned long inTime)
{
    for (long i = this->mSubGroups.size() - 1; i >= 0; i--) {
        EECPAttackSubGroup* theSubGroup = this->mSubGroups[i];
        if (theSubGroup->mNextUpdateTime <= inTime)
            theSubGroup->Update(inTime);
    }
    while (this->mOrders.size() != 0) {
        if (this->mOrders.front().mIsActive)
            this->ExecuteOrder(this->mOrders.front(), inTime);
        this->mOrders.pop_front();
    }
}

// FUNCTION: EXE 0x458b2e
// Buys random affordable improvements for random unit types in the selection, up to the player's limits (each
// unit type once)
void EECPAttackGroup::GiveImprovements(EECPUnitSelectionGroup& inGroup)
{
    std::vector<long> theClasses;
    std::vector<long> theImprovements;
    EEComputerPlayer* thePlayer = this->mCommander->mPlayer;
    unsigned long theNumClasses = thePlayer->mNumImprovedUnitTypes;
    unsigned long theNumImprovements = thePlayer->mNumImprovedAttributes;
    unsigned long i;
    for (i = 0; i < inGroup.mSelected.size(); i++) {
        if (std::find(theClasses.begin(), theClasses.end(), inGroup.mSelected[i]) == theClasses.end())   // the type list (was the empty improvement list)
            theClasses.push_back(inGroup.mSelected[i]);
    }
    for (i = 0; i < inGroup.mUnitIDs.size(); i++) {
        EEUnit* theUnit = gWorld.GetUnit(inGroup.mUnitIDs[i]);
        if (!theUnit)
            continue;
        long theClass = theUnit->mUnitClass->GetUnitType();
        if (std::find(theClasses.begin(), theClasses.end(), theClass) == theClasses.end())   // the type list (was the empty improvement list)
            theClasses.push_back(theUnit->mUnitClass->GetUnitType());
    }
    EEAIRandomShuffle(theClasses.begin(), theClasses.end());
    for (unsigned long j = 0; j < theNumClasses; j++) {
        if (j >= theClasses.size())
            break;
        EEUnitClass* theClass = gUnitClassTable[theClasses[j]];
        // The original evaluates this vector access before the improvement scan.
        EEUnitClassUpgrade* theUpgrade = thePlayer->mUnitClassUpgrades[theClass->GetUnitType()];
        theImprovements.clear();
        for (i = 0; i < 16; i++) {
            if (thePlayer->CanBuy(theClass->GetUnitType(), i, NULL))
                theImprovements.push_back(i);
        }
        EEAIRandomShuffle(theImprovements.begin(), theImprovements.end());
        for (i = 0; i < theNumImprovements; i++) {
            if (i >= theImprovements.size())
                break;
            long theImprovement = theImprovements[i];
            for (unsigned long k = 0; k <= i; k++) {
                if (thePlayer->CanBuy(theClass->GetUnitType(), theImprovement, NULL)) {
                    thePlayer->PayImprovement(theClass, theImprovement);
                    thePlayer->ApplyImprovement(theClass->GetUnitType(), theImprovement, false);
                }
            }
        }
    }
}

// Template COMDATs (bodies from the Dinkum headers; names in analysis/exe/names.txt). Unnamed ones not listed:
// 0x458ea0, 0x459364, 0x459434, 0x4594a5, 0x45972d, 0x4596cb, 0x4590e1, 0x459a4d, 0x459a7f, 0x459e7c, 0x45a131,
// 0x45a16a, 0x45a19f, 0x45a333, 0x45a3f4, 0x45a5e0, 0x45a678, 0x45a6c6, the UMemoryPool<EECPASTargetUnit>
// template-static chain 0x45ac57..0x45ac9c.

// (ICF/other unit names it) 0x458e58: vector<EECPAttackSubGroup*>::push_back

// TEMPLATE: EXE 0x458e84
// (?resize@?$deque@VEECPAGOrder@@V?$allocator@VEECPAGOrder@@@std@@@std@@QAEXI@Z)

// TEMPLATE: EXE 0x458ec2
// (?pop_front@?$deque@VEECPAGOrder@@V?$allocator@VEECPAGOrder@@@std@@@std@@QAEXXZ)

// (ICF/other unit names it) 0x458f17: deque<EECPAGOrder>::iterator::operator* (ICF-named)

// TEMPLATE: EXE 0x458f45
// (??1?$_Tree@JU?$pair@$$CBJU?$pair@JPAVEEUnitClass@@@std@@@std@@U_Kfn@?$map@JU?$pair@JPAVEEUnitClass@@@std@@U?$l)

// TEMPLATE: EXE 0x458f70
// (??1?$_Tree@MU?$pair@$$CBMPAVEECPAttackSubGroup@@@std@@U_Kfn@?$multimap@MPAVEECPAttackSubGroup@@U?$greater@M@st)

// TEMPLATE: EXE 0x458f9b
// (??1?$_Tree@JU?$pair@$$CBJPAVEEAIChokePoint@@@std@@U_Kfn@?$multimap@JPAVEEAIChokePoint@@U?$less@J@std@@V?$alloc)

// TEMPLATE: EXE 0x458fc6
// (??1?$_Tree@JU?$pair@$$CBJU?$pair@V?$vector@JV?$allocator@J@std@@@std@@V12@@std@@@std@@U_Kfn@?$map@JU?$pair@V?$)

// TEMPLATE: EXE 0x458ff1
// (?insert@?$_Tree@JU?$pair@$$CBJU?$pair@V?$vector@JV?$allocator@J@std@@@std@@V12@@std@@@std@@U_Kfn@?$map@JU?$pai)

// TEMPLATE: EXE 0x4590a7
// (?find@?$_Tree@JU?$pair@$$CBJU?$pair@V?$vector@JV?$allocator@J@std@@@std@@V12@@std@@@std@@U_Kfn@?$map@JU?$pair@)

// (ICF/other unit names it) 0x45910c: vector<EECPAttackSubGroup*>::insert (ICF-named)

// TEMPLATE: EXE 0x4590e1
// (??1?$_Tree@MU?$pair@$$CBMVEECPILocation@@@std@@U_Kfn@?$multimap@MVEECPILocation@@U?$greater@M@std@@V?$allocator@U?$pair@$$CBMVEECPILocation@@@std@@@3@@2@U?$greater@M@2@V?$allocator@U?$pair@$$CBMVEECPILocation@@@std@@@2@$00@std@@QAE@XZ)

// TEMPLATE: EXE 0x45915c
// (?resize@?$deque@VEECPAGOrder@@V?$allocator@VEECPAGOrder@@@std@@@std@@QAEXIVEECPAGOrder@@@Z)

// TEMPLATE: EXE 0x4591a9
// (?erase@?$deque@VEECPAGOrder@@V?$allocator@VEECPAGOrder@@@std@@@std@@QAE?AViterator@12@V312@0@Z)

// TEMPLATE: EXE 0x459252
// (?pop_back@?$deque@VEECPAGOrder@@V?$allocator@VEECPAGOrder@@@std@@@std@@QAEXXZ)

// TEMPLATE: EXE 0x4592a1
// (??1?$deque@VEECPAGOrder@@V?$allocator@VEECPAGOrder@@@std@@@std@@QAE@XZ)

// TEMPLATE: EXE 0x4592ba
// (?_Tidy@?$vector@VEECPILocation@@V?$allocator@VEECPILocation@@@std@@@std@@IAEXXZ)

// TEMPLATE: EXE 0x4592f3
// (?erase@?$_Tree@JU?$pair@$$CBJU?$pair@JPAVEEUnitClass@@@std@@@std@@U_Kfn@?$map@JU?$pair@JPAVEEUnitClass@@@std@@)

// TEMPLATE: EXE 0x4593d5
// (?insert@?$_Tree@JU?$pair@$$CBJPAVEEAIChokePoint@@@std@@U_Kfn@?$multimap@JPAVEEAIChokePoint@@U?$less@J@std@@V?$)

// TEMPLATE: EXE 0x459516
// (?_Insert@?$_Tree@JU?$pair@$$CBJU?$pair@V?$vector@JV?$allocator@J@std@@@std@@V12@@std@@@std@@U_Kfn@?$map@JU?$pa)

// TEMPLATE: EXE 0x4596aa
// (??0?$_Tree@GU?$pair@$$CBGJ@std@@U_Kfn@?$map@GJU?$less@G@std@@V?$allocator@U?$pair@$$CBGJ@std@@@2@@2@U?$less@G@)

// TEMPLATE: EXE 0x4596cb
// (?insert@?$_Tree@MU?$pair@$$CBMVEECPILocation@@@std@@U_Kfn@?$multimap@MVEECPILocation@@U?$greater@M@std@@V?$allocator@U?$pair@$$CBMVEECPILocation@@@std@@@3@@2@U?$greater@M@2@V?$allocator@U?$pair@$$CBMVEECPILocation@@@std@@@2@$00@std@@QAE?AU?$pair@Viterator@?$_Tree@MU?$pair@$$CBMVEECPILocation@@@std@@U_Kfn@?$multimap@MVEECPILocation@@U?$greater@M@std@@V?$allocator@U?$pair@$$CBMVEECPILocation@@@std@@@3@@2@U?$greater@M@2@V?$allocator@U?$pair@$$CBMVEECPILocation@@@std@@@2@$00@std@@_N@2@ABU?$pair@$$CBMVEECPILocation@@@2@@Z)

// TEMPLATE: EXE 0x45972d
// (?erase@?$_Tree@MU?$pair@$$CBMVEECPILocation@@@std@@U_Kfn@?$multimap@MVEECPILocation@@U?$greater@M@std@@V?$allocator@U?$pair@$$CBMVEECPILocation@@@std@@@3@@2@U?$greater@M@2@V?$allocator@U?$pair@$$CBMVEECPILocation@@@std@@@2@$00@std@@QAE?AViterator@12@V312@0@Z)

// TEMPLATE: EXE 0x45979e
// (?insert@?$deque@VEECPAGOrder@@V?$allocator@VEECPAGOrder@@@std@@@std@@QAEXViterator@12@IABVEECPAGOrder@@@Z)

// TEMPLATE: EXE 0x4599e6
// (?_Erase@?$_Tree@JU?$pair@$$CBJU?$pair@JPAVEEUnitClass@@@std@@@std@@U_Kfn@?$map@JU?$pair@JPAVEEUnitClass@@@std@)

// TEMPLATE: EXE 0x459ab1
// (?erase@?$_Tree@JU?$pair@$$CBJU?$pair@V?$vector@JV?$allocator@J@std@@@std@@V12@@std@@@std@@U_Kfn@?$map@JU?$pair)

// TEMPLATE: EXE 0x459d69
// (?_Erase@?$_Tree@JU?$pair@$$CBJU?$pair@V?$vector@JV?$allocator@J@std@@@std@@V12@@std@@@std@@U_Kfn@?$map@JU?$pai)

// TEMPLATE: EXE 0x459dda
// (?_Lbound@?$_Tree@JU?$pair@$$CBJU?$pair@V?$vector@JV?$allocator@J@std@@@std@@V12@@std@@@std@@U_Kfn@?$map@JU?$pa)

// TEMPLATE: EXE 0x459e7c
// (?erase@?$_Tree@MU?$pair@$$CBMVEECPILocation@@@std@@U_Kfn@?$multimap@MVEECPILocation@@U?$greater@M@std@@V?$allocator@U?$pair@$$CBMVEECPILocation@@@std@@@3@@2@U?$greater@M@2@V?$allocator@U?$pair@$$CBMVEECPILocation@@@std@@@2@$00@std@@QAE?AViterator@12@V312@@Z)

// TEMPLATE: EXE 0x45a131
// (?_Erase@?$_Tree@MU?$pair@$$CBMVEECPILocation@@@std@@U_Kfn@?$multimap@MVEECPILocation@@U?$greater@M@std@@V?$allocator@U?$pair@$$CBMVEECPILocation@@@std@@@3@@2@U?$greater@M@2@V?$allocator@U?$pair@$$CBMVEECPILocation@@@std@@@2@$00@std@@IAEXPAU_Node@12@@Z)

// TEMPLATE: EXE 0x45a19f
// (?_Insert@?$_Tree@MU?$pair@$$CBMVEECPILocation@@@std@@U_Kfn@?$multimap@MVEECPILocation@@U?$greater@M@std@@V?$allocator@U?$pair@$$CBMVEECPILocation@@@std@@@3@@2@U?$greater@M@2@V?$allocator@U?$pair@$$CBMVEECPILocation@@@std@@@2@$00@std@@IAE?AViterator@12@_NPAU_Node@12@ABU?$pair@$$CBMVEECPILocation@@@2@@Z)

// TEMPLATE: EXE 0x45a333
// (?_Inc@const_iterator@?$_Tree@MU?$pair@$$CBMVEECPILocation@@@std@@U_Kfn@?$multimap@MVEECPILocation@@U?$greater@M@std@@V?$allocator@U?$pair@$$CBMVEECPILocation@@@std@@@3@@2@U?$greater@M@2@V?$allocator@U?$pair@$$CBMVEECPILocation@@@std@@@2@$00@std@@QAEXXZ)

// TEMPLATE: EXE 0x45a36b
// (?push_front@?$deque@VEECPAGOrder@@V?$allocator@VEECPAGOrder@@@std@@@std@@QAEXABVEECPAGOrder@@@Z)

// TEMPLATE: EXE 0x45a3f4
// (?_Rrotate@?$_Tree@MU?$pair@$$CBMVEECPILocation@@@std@@U_Kfn@?$multimap@MVEECPILocation@@U?$greater@M@std@@V?$allocator@U?$pair@$$CBMVEECPILocation@@@std@@@3@@2@U?$greater@M@2@V?$allocator@U?$pair@$$CBMVEECPILocation@@@std@@@2@$00@std@@IAEXPAU_Node@12@@Z)

// TEMPLATE: EXE 0x45a438
// (?sort@std@@YAXV?$_Ptrit@PAVEETown@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@0UEECPAGTownLess@@@Z)

// TEMPLATE: EXE 0x45a4f5
// (?EEAIRandomShuffle@@YAXABV?$_Ptrit@PAVEEAIChokePoint@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@std@@0@Z)

// TEMPLATE: EXE 0x45a51c
// (?copy_backward@std@@YA?AViterator@?$deque@VEECPAGOrder@@V?$allocator@VEECPAGOrder@@@std@@@1@V231@00@Z)

// TEMPLATE: EXE 0x45a55f
// (?copy@std@@YA?AViterator@?$deque@VEECPAGOrder@@V?$allocator@VEECPAGOrder@@@std@@@1@V231@00@Z)

// TEMPLATE: EXE 0x45a5a2
// (?fill@std@@YAXViterator@?$deque@VEECPAGOrder@@V?$allocator@VEECPAGOrder@@@std@@@1@0ABVEECPAGOrder@@@Z)

// TEMPLATE: EXE 0x45a5ce
// (?_Construct@std@@YAXPAU?$pair@$$CBJU?$pair@V?$vector@JV?$allocator@J@std@@@std@@V12@@std@@@1@ABU21@@Z)

// TEMPLATE: EXE 0x45a5e0
// (?_Construct@std@@YAXPAU?$pair@$$CBMVEECPILocation@@@1@ABU21@@Z)

// TEMPLATE: EXE 0x45a5fd
// (??0?$pair@V?$vector@JV?$allocator@J@std@@@std@@V12@@std@@QAE@ABU01@@Z)

// TEMPLATE: EXE 0x45a61e
// (??0?$pair@$$CBJU?$pair@V?$vector@JV?$allocator@J@std@@@std@@V12@@std@@@std@@QAE@ABU01@@Z)

// TEMPLATE: EXE 0x45a64d
// (??_G?$pair@$$CBJU?$pair@V?$vector@JV?$allocator@J@std@@@std@@V12@@std@@@std@@QAEPAXI@Z)

// TEMPLATE: EXE 0x45a696
// (??0?$pair@JU?$pair@V?$vector@JV?$allocator@J@std@@@std@@V12@@std@@@std@@QAE@ABJABU?$pair@V?$vector@J...@1@@Z)

// TEMPLATE: EXE 0x45a6c6
// (??0?$pair@MVEECPILocation@@@std@@QAE@ABMABVEECPILocation@@@Z)

// TEMPLATE: EXE 0x45a6e3
// (?_Insertion_sort@std@@YAXV?$_Ptrit@PAVEETown@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@0UEECPAGTownLess@@@Z)

// TEMPLATE: EXE 0x45a70c
// (?_Sort@std@@YAXV?$_Ptrit@PAVEETown@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@0HUEECPAGTownLess@@@Z)

// TEMPLATE: EXE 0x45a7df
// (?_Unguarded_insertion_sort@std@@YAXV?$_Ptrit@PAVEETown@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@0UEECPAGTownLess@@PAPA)

// TEMPLATE: EXE 0x45a810
// (?_Insertion_sort_1@std@@YAXV?$_Ptrit@PAVEETown@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@0UEECPAGTownLess@@PAPAVEETown@)

// TEMPLATE: EXE 0x45a8d8
// (?make_heap@std@@YAXV?$_Ptrit@PAVEETown@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@0UEECPAGTownLess@@@Z)

// TEMPLATE: EXE 0x45a912
// (?sort_heap@std@@YAXV?$_Ptrit@PAVEETown@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@0UEECPAGTownLess@@@Z)

// TEMPLATE: EXE 0x45a953
// (?_Unguarded_partition@std@@YA?AV?$_Ptrit@PAVEETown@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@V21@0UEECPAGTownLess@@PAPA)

// TEMPLATE: EXE 0x45aa73
// (?_Unguarded_insert@std@@YAXV?$_Ptrit@PAVEETown@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@PAVEETown@@UEECPAGTownLess@@@Z)

// TEMPLATE: EXE 0x45aaa6
// (?_Make_heap@std@@YAXV?$_Ptrit@PAVEETown@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@0UEECPAGTownLess@@PAHPAPAVEETown@@@Z)

// TEMPLATE: EXE 0x45aaea
// (?pop_heap@std@@YAXV?$_Ptrit@PAVEETown@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@0UEECPAGTownLess@@@Z)

// TEMPLATE: EXE 0x45ab13
// (?_Adjust_heap@std@@YAXV?$_Ptrit@PAVEETown@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@HHPAVEETown@@UEECPAGTownLess@@@Z)

// TEMPLATE: EXE 0x45ab81
// (?_Pop_heap_0@std@@YAXV?$_Ptrit@PAVEETown@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@0UEECPAGTownLess@@PAPAVEETown@@@Z)

// TEMPLATE: EXE 0x45abd4
// (?_Push_heap@std@@YAXV?$_Ptrit@PAVEETown@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@HHPAVEETown@@UEECPAGTownLess@@@Z)

// TEMPLATE: EXE 0x45ac23
// (?_Pop_heap@std@@YAXV?$_Ptrit@PAVEETown@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@00PAVEETown@@UEECPAGTownLess@@PAH@Z)

// Template and inline COMDATs this unit's code instantiates (the bodies come from the headers; each was compared
// against the original by name)
// TEMPLATE: EXE 0x458e58
// TEMPLATE: EXE 0x45910c
// TEMPLATE: EXE 0x459364
// TEMPLATE: EXE 0x459434
// TEMPLATE: EXE 0x4594a5
// TEMPLATE: EXE 0x459a4d
// TEMPLATE: EXE 0x459a7f
// TEMPLATE: EXE 0x45a16a
// Named COMDATs of this unit (compiled here; names in analysis/exe/names.txt)
// TEMPLATE: EXE 0x458f17
// TEMPLATE: EXE 0x459a18
// TEMPLATE: EXE 0x459da5
// TEMPLATE: EXE 0x459e03
// TEMPLATE: EXE 0x459e44
// More named COMDATs of this unit (names in analysis/exe/names.txt)
// TEMPLATE: EXE 0x4571dd
