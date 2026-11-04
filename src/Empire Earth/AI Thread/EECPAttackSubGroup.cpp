// C:\EEX\Empire Earth\AI Thread\EECPAttackSubGroup.cpp (file name inferred: no __FILE__; RTTI class
// EECPAttackSubGroup > UReferenceCounter, vtable 0x43bd20). A computer player's attack sub group: 0x5ac9c..0x62f8a
// (EECPAttackGroup.cpp's UMemoryPool<EECPASTargetUnit> template-static chain 0x5ac57 ends before it): the unit counts
// helpers 0x5ac9c/0x5ad6e, the path point copy ctor 0x5ad99, the ctor 0x5adb2, ??_G 0x5afdc, the dtor 0x5aff8, the
// slots and members to 0x6136d, then the map/vector/deque/pool COMDATs to 0x62f8a (EECPAttackSubGroupUnit.cpp's
// class-static chains start there).
#include <windows.h>
#include <vector>
#include <deque>
#include <map>
#include <set>
#include <algorithm>
#include <string.h>
#include <typeinfo>

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
#include "ee/EEUnitGoal.h"
#include "ee/EEUGLocation.h"
#include "ee/EEUGAttackUnit.h"
#include "lle/GERectangle.h"
#include "ee/EEMapDimensions.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEGlobalConstants.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIPathSubSector.h"
#include "ee/EETown.h"
#include "ee/EECPILocation.h"
#include "ee/EECPAGTask.h"
#include "ee/EECPASTargetUnit.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EECPAttackGroup.h"
#include "ee/EECPAttackCommander.h"
#include "ee/EECPAttackSubGroupUnit.h"
#include "ee/EECPCombatManager.h"
#include "ee/EECPUnitSelector.h"
#include "ee/EECPIntelligence.h"
#include "ee/EECPPersonnel.h"
#include "ee/EEAutoPathRequest.h"
#include "ee/EEPathManager.h"
#include "ee/EECPAttackFormation.h"
#include "ee/EEFOLocationScreenBase.h"
#include "ee/EECalamityClass.h"
#include "ee/EEUnitCalamityClass.h"
#include "ee/EECCSpecialAttack.h"
#include "ee/EECalamityManager.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EECivPowersManager.h"
#include "ee/EEGroupCluster.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEInfluenceTable.h"
#include "ee/EEUUUnitFunctor.h"
#include "ee/EEUnitUtilities.h"
#include "LostDebug.h"   // synthetic, NOT original source (LOST-DEBUG: EE_LOST_DEBUG)

// 0x564f6 (cdecl; EECPAttackGroup.cpp's range): a new sub group unit of inUnit's kind (provisional name)
EECPAttackSubGroupUnit* EECPCreateSubGroupUnit(EEComplexUnit* inUnit, EECPAttackSubGroup* inSubGroup, bool inGeneric);
// 0x5661e (cdecl; EECPAttackGroup.cpp's range): a new sub group unit of the saved kind inKind (provisional name)
EECPAttackSubGroupUnit* EECPCreateSubGroupUnitOfKind(EEComplexUnit* inUnit, EECPAttackSubGroup* inSubGroup, long inKind);

// 0x4e5c88 (.data, zero; between EEComputerPlayer.obj's 0x4e5c70 and EECPCivilizationManager.obj's 0x4e5d18): the
// targets' pool. Its chain 0x5ac57..0x5ac9c (ctor 0x613a1 unguarded, dtor guarded by bit 1 of 0x4e5c80) sits right
// before this unit's first function and every user is in this unit (Fn5B397 and the target new/delete/release), so
// it is defined here. The dtor guard suggests a class static data member whose owner is not decoded; the file-scope
// form is inferred (provisional name)
UMemoryPool<EECPASTargetUnit> EECPAttackSubGroup::sTargetPool;
// SYNTHETIC: EXE 0x45ac57
// SYNTHETIC: EXE 0x45ac61
// SYNTHETIC: EXE 0x45ac6b
// SYNTHETIC: EXE 0x45ac77
// Declared as in EECPAttackCommander.cpp and EECPAttackSubGroupUnit.cpp; the global is bound at 0x530e0c
extern long gNumPlayers;

// 0x43bf0c (.rdata; both neighbours are this unit's constants): declared extern in ee/EEGlobalConstants.h
const float kHeightEpsilon = 0.01f;
static const float kASGRollLow = 0.42f;     // 0x43bf14
// 0x43bf10 is the pooled `/ 10.0f` literal (6 readers; RollAggression 0x45cb74)
static const float kASGNear = 1.8f;         // 0x43bf18
static const float kFastUnitVelocity = 13.0f;  // 0x43bd10
static const float kASGRandomChance = 0.65f;    // 0x43bd00
static const float kASGUnitStep = 0.15f;    // 0x43bd04
static const float kASGRandomLow = 0.15f;   // 0x43bd08
static const float kASGRandomHigh = 1.0f;   // 0x43bd0c
static const float kASGRandomChance58 = 0.5f;   // 0x43bcf0
static const float kASGUnitStep58 = 0.15f;  // 0x43bcf4
static const float kASGRandomLow58 = 0.15f; // 0x43bcf8
static const float kASGRandomHigh58 = 1.0f; // 0x43bcfc

// FUNCTION: EXE 0x45ac9c
// Counts a unit into the tallies by kind (aircraft, bombers, ships, submarines, space units, buildings, wonders,
// citizens, cyborgs, plague targets), adds its attack and notes any calamity on it
void EECPASGUnitCounts::Add(EEComplexUnit* inUnit, long inArg2)
{
    EEUnitClass* theClass = inUnit->mUnitClass;
    if (theClass->mDomains & kMovementAir) {
        this->mNumAircraft++;
        switch (theClass->mAIKind) {
        case 0x21:
            this->mNumBombers++;
            this->mL20++;
            break;
        case kAIKindBomber:
            this->mNumBombers++;
            break;
        }
    } else if (theClass->mDomains & kMovementSea) {
        this->mNumShips++;
        if (theClass->mFamily == kUnitFamilySubmarine)
            this->mNumSubmarines++;
    } else if (theClass->mDomains & kMovementSpace) {
        this->mNumSpaceUnits++;
    } else if (theClass->mCategory == kUnitCategoryBuilding) {
        this->mNumBuildings++;
        EETechTreeNode* theNode = inUnit->GetTechTreeNode();
        if (theNode && theNode->mData->mKind == 4)
            this->mNumWonders++;
    } else if (theClass->mCategory == kUnitCategoryLandCitizen) {
        this->mNumCitizens++;
        this->mNumPlagueTargets++;
    } else if (theClass->mCategory == kUnitCategoryNavalCitizen) {
        this->mNumCitizens++;
        this->mNumPlagueTargets++;
    } else if (theClass->mIsCyber) {
        this->mNumCyborgs++;
    } else if (theClass->mIsHuman || theClass->mIsHorse) {
        this->mNumPlagueTargets++;
    }
    this->mTotalAttack += inUnit->mStats->mAttack;
    if (inUnit->mCalamityFlags)
        this->mHasCalamity = true;
}

// FUNCTION: EXE 0x45ad6e
// Resets every tally, the total attack and the calamity flag
void EECPASGUnitCounts::Clear()
{
    this->mTotalAttack = 0.0f;
    this->mNumBuildings = 0;
    this->mNumWonders = 0;
    this->mNumPlagueTargets = 0;
    this->mNumCyborgs = 0;
    this->mNumShips = 0;
    this->mHasCalamity = false;
    this->mNumAircraft = 0;
    this->mNumBombers = 0;
    this->mL20 = 0;
    this->mNumCitizens = 0;
    this->mNumSubmarines = 0;
    this->mNumSpaceUnits = 0;
}

// FUNCTION: EXE 0x45ad99
// Copies a path point's tile and path sector kind
// out of line (the deque/vector fill and copy_backward helpers 0x6239a/0x623c6/0x623e3 and EECPASGLand call it)
EECPASGPathPoint& EECPASGPathPoint::operator=(const EECPASGPathPoint& inPoint)
{
    this->mTile = inPoint.mTile;
    this->mSectorKind = inPoint.mSectorKind;
    return *this;
}

// FUNCTION: EXE 0x45adb2
// Creates an idle sub-group of the attack group: new ID from the combat manager, aggression 0.5, scan range 13,
// the group's task player (unless loading a saved game) and a new formation
EECPAttackSubGroup::EECPAttackSubGroup(EECPAttackGroup* inGroup, long inKind)
{
    EE_LOST_DEBUG(2);  // LOST-DEBUG: none: exact only (lostdebug_search, smallest n)
    EECPCombatManager* theManager = inGroup->mCommander->GetCombatManager();
    this->mID = theManager->GetNextSubGroupID();
    this->mRequest = NULL;
    this->mGroup = inGroup;
    this->mPlayer = inGroup->mCommander->mPlayer;
    this->mScanRange = 0xd;
    this->mAggression = 0.5f;
    this->mNextScanTime = 0;
    this->mNextUpdateTime = 0;
    this->mNumSplits = 0;
    this->mU1B0 = 0;
    this->mU1B4 = 0;
    this->mState = kASGStateIdle;
    this->mMission = 0;
    this->mSplitKind = 0;
    this->mKind = inKind;
    this->mPathRequest = NULL;
    this->mPlayer2 = inGroup->mCommander->mPlayer;
    this->mNextTaskCheckTime = 0;
    this->mNextFollowTime = 0;
    this->mNextPositionTime = 0;
    this->mU1BC = 0;
    this->mGatherDeadline = 0;
    this->mB1E0 = false;
    this->mB1E1 = false;
    this->mB1E2 = false;
    this->mIsOutOfFight = false;
    this->mHasAreaEffect = false;
    this->mCanAttackShore = true;
    long thePlayerID = !gLoadingSavedGame ? inGroup->mTask.GetPlayerID() : 0;
    if (!gLoadingSavedGame)
        this->mTask.mPlayer = gWorld.mPlayers[thePlayerID];
    this->mFormation = new EECPAttackFormation(this);
}

// FUNCTION: EXE 0x45aff8
// Destroys the sub-group (members only)
EECPAttackSubGroup::~EECPAttackSubGroup()
{
}

// FUNCTION: EXE 0x45b075
// Adds a unit not yet in the sub-group, re-ranks and sends it to the group (or the retreat waypoint); a carried
// aircraft or spaceship goes to the task location or a free spot near its carrier; then refreshes the units
void EECPAttackSubGroup::AddUnit(EEComplexUnit* inUnit)
{
    EEAIAStarBounds theBounds;
    EETileLocation theTile;
    EETileLocation theFound;
    EECPAttackSubGroupUnit* theUnit = NULL;
    bool theKnown = false;
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        if ((*(this->mUnits.begin() + i))->GetUnit() == inUnit) {
            theKnown = true;
            break;
        }
    }
    if (!theKnown) {
        theUnit = EECPCreateSubGroupUnit(inUnit, this, 0);
        theUnit->Virtual48();
        this->mUnits.push_back(theUnit);
        this->RankUnits();
        if (inUnit->mContainer == NULL) {
            if (this->mState == kASGStateRetreatToTown && gWorld.GetMap()->GetTileAtLocation(this->mNextWaypoint.mTile))
                theTile = this->mNextWaypoint.mTile;
            else
                this->FindPosition(theTile, true, false);
            EEUGLocation* theGoal = new EEUGLocation(theTile.GetX() + 0.5f, theTile.GetY() + 0.5f, 0.0f, true, true, false,
                                                     0.0f, false, false);
            if (!this->GiveGoal(inUnit, theGoal, 0, kActionPrepareToMove) && theGoal)
                delete theGoal;
        } else if ((inUnit->mUnitClass->mDomains & kMovementAir) || inUnit->mUnitClass->mCategory == kUnitCategoryPlanetSpaceship) {
            if (this->mTask.mIsActive && this->mTask.mLocation.IsOnMap()) {
                this->SendUnitTo(theUnit, this->mTask.mLocation, true);
            } else {
                EEComplexUnit* theContainer = inUnit->mContainer;
                if ((unsigned long)theContainer->GetTileX() < gMapWidth && (unsigned long)theContainer->mTileY < gMapHeight) {
                    theBounds.mLeft = theContainer->GetTileX();
                    theBounds.mTop = theContainer->mTileY;
                    theBounds.mRight = theContainer->GetTileX();
                    theBounds.mBottom = theContainer->mTileY;
                    theBounds.Grow(10, true);
                    EEFOLocationScreenEnsureContinentAndPathOverlay theScreen;
                    // NEAR: 0x249201 returns its bool, but names.txt binds it as the void EEFn249201
                    if (EEFOFindLocation(theBounds, theFound, gPhysics->mRandom, theScreen, 10))
                        this->SendUnitTo(theUnit, EECPILocation(theFound), true);
                }
            }
        }
    }
    this->RefreshUnits();
}

// FUNCTION: EXE 0x45b2c4
// Replaces the personnel request with a town request (at the town's centre) or a continent request for the classes;
// in a wonder race it expires after 300 s; refreshes the units when there are any
void EECPAttackSubGroup::RequestUnits(std::vector<long>& inClasses, std::vector<long>& inUnitIDs, long inContinent,
                                   long inTownID)
{
    if (this->mRequest) {
        this->mGroup->mCommander->mPersonnel->ReleaseRequest(this->mRequest->mID);
        this->mRequest->Dereference();
    }
    EETown* theTown = NULL;
    if (inTownID != 0)
        theTown = this->mPlayer->GetTownByID(inTownID);
    if (theTown == NULL)
        this->mRequest = this->mGroup->mCommander->mPersonnel->NewContinentRequest(inClasses, inUnitIDs, inContinent);
    else {
        EECPPersonnel* thePersonnel = this->mGroup->mCommander->mPersonnel;
        this->mRequest = thePersonnel->NewTownRequest(inClasses, inUnitIDs, inTownID, theTown->mHull.GetCenter());
    }
    if (this->mRequest && this->mPlayer2->mCombatManager->mIsWonderRace)
        this->mRequest->mDeadline = gPhysics->mGameTime + 300000;
    if (this->mUnits.size() > 0)
        this->RefreshUnits();
}

// FUNCTION: EXE 0x45b397
// Sizes the shared target pool (chunk size and maximum chunk count)
// cdecl; EECPCombatManager's ctor calls it with 0x200, 0x19 (provisional name)
void Fn5B397(long inChunkSize, long inMaxChunks)
{
    EECPAttackSubGroup::sTargetPool.SetSize(inChunkSize, inMaxChunks, 1);
}

// FUNCTION: EXE 0x45b3ac
// Recounts the target tallies over the current targets
void EECPAttackSubGroup::CountTargets()
{
    this->mCounts.Clear();
    for (long i = this->mTargets.size() - 1; i >= 0; i--)
        this->mCounts.Add(this->mTargets[i]->mUnit, 1);
}

// FUNCTION: EXE 0x45b3f1
// Returns true when the sub-group has units, has no request, or its request is completely filled
bool EECPAttackSubGroup::IsRequestFilled()
{
    return this->mUnits.size() > 0 || this->mRequest == NULL || this->mRequest->mFill == 1.0f;
}

// FUNCTION: EXE 0x45b421
// Returns true once every unit is in the rally point's path sector, idle or near its path's end (under 11
// waypoints), or once the gather deadline has passed
bool EECPAttackSubGroup::HasGathered()
{
    long theCount = 0;
    EEAIPathSector* theSector = gContinentManager.GetPathSector(this->mRallyPoint.mTile);
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        EEComplexUnit* theUnit = this->mUnits[i]->GetUnit();
        if (theSector == gContinentManager.GetPathSector(theUnit) || theUnit->GetCommandType() == kActionIdle ||
            (long)theUnit->mWaypoints.size() < 11)
            theCount++;
    }
    if (theCount == this->mUnits.size() || gPhysics->mGameTime > this->mGatherDeadline)
        return true;
    return false;
}

// FUNCTION: EXE 0x45b4d7
// Takes a new task (task player from the attack group if unset; an inactive task becomes a return to the rally point),
// sets or rolls the aggression, and goes idle when it has units and is not in transit
void EECPAttackSubGroup::SetTask(const EECPAGTask& inTask)
{
    this->mTask = inTask;
    if (this->mTask.GetPlayerID() == 0) {
        EECPAttackGroup* theGroup = this->mGroup;
        this->mTask.mPlayer = gWorld.mPlayers[theGroup->mTask.GetPlayerID()];
    }
    if (!this->mTask.mIsActive) {
        this->mTask.mType = kAGTaskReturn;
        this->mTask.mLocation = this->mRallyPoint;
        this->mTask.mIsActive = true;
    }
    if (this->mTask.mAggression == 0.0f)
        this->RollAggression();
    else
        this->SetAggression(this->mTask.mAggression);
    if (this->mUnits.size() > 0 && this->mState != kASGStateInTransit)
        this->SetState(kASGStateIdle, gPhysics->mGameTime);
}

// FUNCTION: EXE 0x45b5a5
// Sends the sub-group back to the task location unless it is already next to it
void EECPAttackSubGroup::ReturnToTask(long inTime)
{
    EECPILocation theLocation;
    this->GetLocation(theLocation);
    if (!theLocation.IsAdjacent(this->mTask.mLocation))
        this->SetDestination(this->mTask.mLocation);
}

// FUNCTION: EXE 0x45b5e6
// Orders a unit to attack the target (or its carrier); crusaders may convert, and flaming archers, fanatics and
// capital ships may use their special attack; then queues a move to the given tile unless it is (0,0)
void EECPAttackSubGroup::AttackWith(EECPAttackSubGroupUnit* inUnit, EEComplexUnit* inTarget, EETileCoordinate* inTile)
{
    bool theUnused;
    long theAction;
    if (inTarget->mContainer)
        inTarget = inTarget->mContainer;
    bool theRanged = inUnit->GetUnit()->GetUnitClass()->mConversionRate != 0.0f;
    if (inUnit->IsDone())
        return;
    inUnit->SetTarget(inTarget);
    if (!theRanged) {
        theAction = 2016;
    } else {
        theAction = 2004;
        if (inTarget->mUnitClass->mIsGaiaPlaceable)
            return;
    }
    bool theSpecial = false;
    if (inUnit->GetUnit()->GetUnitClass()->IsCrusader() && EECanConvert(inUnit->GetUnit(), inTarget, theUnused)
        && gPhysics->GetRandom() <= 0.35f)
        theSpecial = true;
    EEUGAttackUnit* theGoal = new EEUGAttackUnit(inUnit->GetUnit(), inTarget, true, theSpecial);
    if (!this->GiveGoal(inUnit->GetUnit(), theGoal, 0, theAction)) {
        if (theGoal)
            delete theGoal;
    } else {
        long theType = inUnit->GetUnit()->GetUnitClass()->mUnitType;
        if ((theType == kUnitTypeArch03SimpleBowmanFlaming || theType == kUnitTypeArch05CompositeBowFlaming || theType == kUnitTypeArch06LongBowFlaming) && inTarget->mUnitClass->IsLandBuilding()) {
            EECCSpecialAttack* theClass = NULL;
            USingleton<EECalamityManager>::GetInstance()->GetCalamityClass(0x17, theClass);
            if (gUnitSetManager.GetSet(theClass->GetUnitSet())->Contains(inTarget->mUnitClass->mUnitType)
                && inUnit->GetUnit()->GetPower() >= theClass->GetCost()
                && gWorldTable.GetFloatValue(0x54) >= gPhysics->GetRandom()) {
                inUnit->GetUnit()->SetPower(inUnit->GetUnit()->GetPower() - theClass->GetCost());
                inUnit->GetUnit()->mIsSpecialAttack = true;
            }
        } else if (gCivPowersManager.HasPower(inUnit->GetUnit()->GetPlayer(), kCivPowerFanaticism)
                   && gUnitSetManager.GetSet(0x2f)->Contains(inUnit->GetUnit()->GetUnitClass()->mUnitType)
                   && !inUnit->GetUnit()->IsCloaked()
                   && gPhysics->mRandom.Generate() <= gWorldTable.GetFloatValue2(0x54)) {
            inUnit->GetUnit()->GoFanatic();
        } else if (inUnit->GetUnit()->GetUnitClass()->mUnitType == kUnitTypeSp15CapitalShip || inUnit->GetUnit()->GetUnitClass()->mUnitType == kUnitTypeCapitalShipYamato) {
            EECCSpecialAttack* theClass = NULL;
            USingleton<EECalamityManager>::GetInstance()->GetCalamityClass(0x1e, theClass);
            if (inTarget->mHitPoints > inUnit->GetUnit()->GetUnitClass()->mProjectileClass->mAttack
                && inUnit->GetUnit()->GetPower() >= theClass->GetCost()
                && gPhysics->mRandom.Generate() <= 0.25f) {
                inUnit->GetUnit()->SetPower(inUnit->GetUnit()->GetPower() - theClass->GetCost());
                inUnit->GetUnit()->mIsSpecialAttack = true;
            }
        }
    }
    if (inTile->GetX() == 0 && inTile->GetY() == 0)
        return;
    EEUGLocation* theMove = new EEUGLocation(GE2DPoint(inTile->GetX() + 0.5f, inTile->GetY() + 0.5f), true, true, false,
                                             0.0f, false, false);
    if (!this->GiveGoal(inUnit->GetUnit(), theMove, 1, kActionPrepareToMove) && theMove)
        delete theMove;
}

// FUNCTION: EXE 0x45b96d
// Returns true for the four crusader unit types (short sword, long sword, bronze spear cavalry, knight)
// An EEUnitClass member (Virtual1C calls it with ecx = the unit's class); its body follows its first user here.
bool EEUnitClass::IsCrusader()
{
    return this->mUnitType == kUnitTypeInf04ShortSwordCrusader || this->mUnitType == kUnitTypeInf06LongSwordCrusader || this->mUnitType == kUnitTypeCav04BronzeSpearCavalryCrusader || this->mUnitType == kUnitTypeCav06KnightCrusader;
}

// FUNCTION: EXE 0x45b996
// Sends the sub-group to a random point of the nearest town not containing it (the allies loop asks the own player
// again); with no usable town, drops the task and heads for a random point of its continent
void EECPAttackSubGroup::MoveToNearestTown()
{
    std::multimap<long, EETown*> theTowns;
    EETileLocation theTile;
    EETileLocation theTownTile;
    this->FindPosition(theTile, true, false);
    EEAIContinent* theContinent = gContinentManager.GetContinentAt(theTile.GetX(), theTile.mY);
    EETown* theTown = this->mPlayer->GetNearestTown(theTile, theTownTile);
    if (theTown) {
        if (!theTown->Contains(theTile)) {
            long theDistance = theTile.DistanceToSquared(theTownTile);
            theTowns.insert(std::pair<long, EETown*>(theDistance, theTown));
            for (long i = this->mPlayer->mAllies.size() - 1; i >= 0; i--) {
                if (*(this->mPlayer->mAllies.begin() + i) == 0 || this->mPlayer->mAllies[i] == this->mPlayer)
                    continue;
                theTownTile.mX = 0;
                theTownTile.mY = 0;
                theTown = this->mPlayer->mAllies[i]->GetNearestTown(theTile, theTownTile);   // the ally's (was mPlayer's)
                if (theTown && !theTown->Contains(theTile))
                    theTowns.insert(std::make_pair(theTile.DistanceToSquared(theTownTile),
                                                                            theTown));
            }
        }
        bool theFound = theTowns.size() > 0;
        if (theFound) {
            theTile = (*theTowns.begin()).second->GetRandomPoint();
            if (gWorld.GetMap()->GetTile(theTile.GetX(), theTile.mY)) {
                this->mTask.mLocation = EECPILocation(theTile);
                this->SetDestination(this->mTask.mLocation);
            } else {
                theFound = false;
            }
            if (theFound)
                return;
        }
    }
    this->mTask.mIsActive = false;
    if (theContinent)
        this->SetDestination(EECPILocation(theContinent->GetRandomPoint()));
}

// FUNCTION: EXE 0x45bbc5
// Releases the given unit's target
void EECPAttackSubGroup::DropTargetOf(EECPAttackSubGroupUnit* inUnit)
{
    inUnit->ReleaseTarget();
}

// FUNCTION: EXE 0x45bbd1
// Sets the out-of-fight flag on the sub-group and on every living unit
void EECPAttackSubGroup::SetOutOfFight(bool inIsOutOfFight)
{
    this->mIsOutOfFight = inIsOutOfFight;
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        EECPAttackSubGroupUnit* theUnit = this->mUnits[i];
        if (theUnit->GetUnit() && theUnit->GetUnit()->mHitPoints > 0)
            theUnit->mIsOutOfFight = inIsOutOfFight;
    }
}

// FUNCTION: EXE 0x45bc18
// Returns a central tile of the units' tiles (carriers stand in for carried units, off-map units skipped), or (0,0)
EETileLocation EECPAttackSubGroup::ComputeCenterPoint(long inMustBeInList)
{
    std::vector<EETileCoordinate> theTiles;
    EETileLocation theResult;
    theTiles.reserve(this->GetNumUnits());
    for (long i = this->GetNumUnits() - 1; i >= 0; i--) {
        EEComplexUnit* theUnit = this->GetUnitUnit(i);
        if (theUnit->mContainer)
            theUnit = theUnit->mContainer;
        if (theUnit->GetTileX() != -10000)
            theTiles.push_back(theUnit->GetTile());
    }
    if (theTiles.size() > 0)
        theResult = EEFn08282(theTiles, inMustBeInList);
    return theResult;
}

// FUNCTION: EXE 0x45bcd0
// Moves the task location to a random point of its area and re-checks the task in 6-35 s; returns whether it moved
bool EECPAttackSubGroup::RandomizeTaskLocation()
{
    EECPILocation theLocation(this->mTask.mLocation);
    if (!this->mTask.mIsActive)
        return false;
    EEAIArea* theArea = gContinentManager.GetArea(this->mTask.mLocation.mSerial);
    if (!theArea)
        return false;
    theLocation.mTile = theArea->GetRandomPoint();
    if (!gWorld.GetMap()->GetTileAtLocation(theLocation.mTile))
        return false;
    this->mTask.mLocation = theLocation;
    unsigned long theTime = gPhysics->mGameTime;
    this->mNextTaskCheckTime = theTime + gPhysics->GetRandom(6000L, 35000L);
    // report the move (every path used to return false; SetState, the only caller, ignores the result)
    return true;
}

// FUNCTION: EXE 0x45bd63
// Deletes the formation, cancels the path request, releases units and targets and gives back the personnel request
void EECPAttackSubGroup::Shutdown()
{
    delete this->mFormation;
    this->mFormation = NULL;
    if (this->mPathRequest) {
        gPathManager.CancelAutoPathRequest(this->mPathRequest);
        this->mPathRequest->Dereference();
        this->mPathRequest = NULL;
    }
    this->ReleaseUnits();
    this->ClearTargets();
    if (!this->mRequest)
        return;
    this->mGroup->mCommander->mPersonnel->ReleaseRequest(this->mRequest->mID);
    this->mRequest->Dereference();
    this->mRequest = NULL;
}

// FUNCTION: EXE 0x45bdeb
// Returns every target and siege target to the pool and empties both lists
void EECPAttackSubGroup::ClearTargets()
{
    long i;
    for (i = this->mTargets.size() - 1; i >= 0; i--)
        EECPAttackSubGroup::ReleaseTarget(this->mTargets[i]);
    this->mTargets.clear();
    for (i = this->mSiegeTargets.size() - 1; i >= 0; i--)
        EECPAttackSubGroup::ReleaseTarget(this->mSiegeTargets[i]);
    this->mSiegeTargets.clear();
}

// FUNCTION: EXE 0x45be5a
// Cancels the path request if it is still running and drops it
void EECPAttackSubGroup::CancelPath()
{
    if (!this->mPathRequest)
        return;
    if (!this->mPathRequest->mIsDone)
        gPathManager.CancelAutoPathRequest(this->mPathRequest);
    this->mPathRequest->Dereference();
    this->mPathRequest = NULL;
}

// FUNCTION: EXE 0x45be8d
// Returns the gate route weight: the share of units not air or in unit sets 0x60/0x61 (0 if one is in set 0x62);
// unless on a wonder or attack-location task, minus 0.15 per other unit (min 0.01) or, with none, 35% a random 0.15-1.0
float EECPAttackSubGroup::GetGateWeight()
{
    EEUnitSet* theSet60 = gUnitSetManager.GetSet(0x60);
    EEUnitSet* theSet61 = gUnitSetManager.GetSet(0x61);
    EEUnitSet* theSet62 = gUnitSetManager.GetSet(0x62);
    float theResult = 1.0f;
    float theCount = 0.0f;
    if (this->mUnits.size() > 0) {
        for (long i = this->mUnits.size() - 1; i >= 0; i--) {
            EEComplexUnit* theUnit = this->mUnits[i]->GetUnit();
            if (theUnit->mUnitClass->mDomains & kMovementAir) {
                theCount += 1.0f;
            } else {
                if ((!theSet61->IsEmpty() && theSet61->Contains(theUnit->mUnitClass->mUnitType))
                    || theSet60->Contains(theUnit->mUnitClass->mUnitType))
                    theCount += 1.0f;
                if (theSet62->Contains(theUnit->mUnitClass->mUnitType)) {
                    theCount = (float)this->mUnits.size();
                    break;
                }
            }
        }
        theResult = ((float)this->mUnits.size() - theCount) / this->mUnits.size();
        if (!this->mTask.mIsActive || (this->mTask.mMode != kAGModeWonder && this->mTask.mType != kAGTaskAttackLocation && theResult != 0.0f)) {
            if (theCount != 0.0f) {
                theResult -= kASGUnitStep * theCount;
                if (theResult < kHeightEpsilon)
                    theResult = kHeightEpsilon;
            } else if (gPhysics->mRandom.Generate() >= kASGRandomChance) {
                theResult = gPhysics->GetRandom(kASGRandomLow, kASGRandomHigh);
            }
        }
    }
    return theResult;
}
// FUNCTION: EXE 0x45c0d7
// Finds the group's position: the unit tile nearest the centre of the units' area (cached 5 s unless forced); falls
// back to the rally point and returns false when no unit has a position
bool EECPAttackSubGroup::FindPosition(EETileLocation& outTile, bool inArg2, bool inForce)
{
    std::vector<EEComplexUnit*> theUnits;
    bool theStarted = false;
    outTile.mX = 0;
    outTile.mY = 0;
    EETileLocation theCenter;
    EETileLocation theTile;
    EEComplexUnit* theUnit = NULL;
    long thePass = 0;
    bool theResult = true;
    if (this->GetNumUnits() > 0 && (inForce || this->mNextPositionTime <= gPhysics->mGameTime)) {
        this->mArea.mTop = 0;
        this->mArea.mLeft = 0;
        this->mArea.mBottom = 0;
        this->mArea.mRight = 0;
        theUnits.reserve(this->GetNumUnits());
        while (theUnits.size() == 0 && thePass <= 1) {
            for (long i = this->GetNumUnits() - 1; i >= 0; i--) {
                if (thePass != 1 && (this->mUnits[i])->IsDone())
                    continue;
                theUnit = (this->mUnits[i])->GetUnit();
                if (theUnit->mContainer)
                    theUnit = theUnit->mContainer;
                theUnits.push_back(theUnit);
                if (theUnit->GetTileX() != -10000) {
                    if (theStarted == true) {
                        this->mArea.Extend(U2DPoint<long>(theUnit->GetTileX(), theUnit->mTileY));
                    } else {
                        this->mArea.mTop = theUnit->mTileY;
                        this->mArea.mBottom = theUnit->mTileY;
                        this->mArea.mLeft = theUnit->GetTileX();
                        this->mArea.mRight = theUnit->GetTileX();
                        theStarted = true;
                    }
                } else if (theUnit->mLastMapPosition.GetX() == 0.0f && theUnit->mLastMapPosition.GetY() == 0.0f) {
                    theUnits.pop_back();
                } else if (theStarted == true) {
                    this->mArea.Extend(U2DPoint<long>(UMFloor(theUnit->mLastMapPosition.GetX()), UMFloor(theUnit->mLastMapPosition.GetY())));
                } else {
                    this->mArea.mTop = UMFloor(theUnit->mLastMapPosition.GetY());
                    this->mArea.mBottom = UMFloor(theUnit->mLastMapPosition.GetY());
                    this->mArea.mLeft = UMFloor(theUnit->mLastMapPosition.GetX());
                    this->mArea.mRight = UMFloor(theUnit->mLastMapPosition.GetX());
                    theStarted = true;
                }
            }
            thePass++;
        }
        if (theUnits.size() > 0) {
            this->mArea.GetCenter(theCenter);
            this->mNextPositionTime = gPhysics->mGameTime + 5000;
            theUnit = theUnits.back();
            if (theUnit->mContainer)
                theUnit = theUnit->mContainer;
            if (theUnit->GetTileX() != -10000) {
                outTile.mX = theUnit->GetTileX();
                outTile.mY = theUnit->mTileY;
            } else {
                outTile.mX = UMFloor(theUnit->mLastMapPosition.GetX());
                outTile.mY = UMFloor(theUnit->mLastMapPosition.GetY());
            }
            long theBest = theCenter.DistanceToSquared(outTile);
            if (theBest != 0) {
                for (long j = theUnits.size() - 2; j >= 0; j--) {
                    theUnit = theUnits[j];
                    if (theUnit->mContainer)
                        theUnit = theUnit->mContainer;
                    if (theUnit->GetTileX() != -10000) {
                        theTile.mX = theUnit->GetTileX();
                        theTile.mY = theUnit->mTileY;
                    } else {
                        theTile.mX = UMFloor(theUnit->mLastMapPosition.GetX());
                        theTile.mY = UMFloor(theUnit->mLastMapPosition.GetY());
                    }
                    long theDistance = theCenter.DistanceToSquared(theTile);
                    if (theDistance >= theBest)
                        continue;
                    theBest = theDistance;
                    outTile = theTile;
                    if (theTile.GetX() == theCenter.GetX() && theTile.GetY() == theCenter.GetY())
                        break;
                }
            }
        } else {
            theResult = false;
            outTile = this->mRallyPoint.mTile;
            this->mArea.mTop = outTile.GetY();
            this->mArea.mLeft = outTile.GetX();
            this->mArea.mBottom = outTile.GetY();
            this->mArea.mRight = outTile.GetX();
        }
        this->mPosition = outTile;
    } else if (this->mPosition.GetX() == 0 && this->mPosition.GetY() == 0) {
        theResult = false;
        outTile = this->mRallyPoint.mTile;
        this->mArea.mTop = outTile.GetY();
        this->mArea.mLeft = outTile.GetX();
        this->mArea.mBottom = outTile.GetY();
        this->mArea.mRight = outTile.GetX();
        this->mPosition = outTile;
    } else {
        outTile = this->mPosition;
        theResult = true;
    }
    return theResult;
}
// RankUnits's sort order: the units by importance, highest first (std::sort COMDATs 0x621c6..) (provisional name)
struct EECPASGUnitValueGreater {
    bool operator()(EECPAttackSubGroupUnit* inA, EECPAttackSubGroupUnit* inB) const { return inA->mImportance > inB->mImportance; }
};

// FUNCTION: EXE 0x45c5e2
// Sorts the units by importance, marks the top quarter important, checks whether any unit's ammo can hit the shore
// (ammo table row 0x2c) and recomputes the group stats
void EECPAttackSubGroup::RankUnits()
{
    this->mCanAttackShore = true;
    std::sort(this->mUnits.begin(), this->mUnits.end(), EECPASGUnitValueGreater());
    if (this->mUnits.size() > 0) {
        this->mCanAttackShore = false;
        unsigned long theCount = UMCeil(this->mUnits.size() * 0.25f);
        if (theCount < this->mUnits.size() && theCount > 0) {
            for (unsigned long i = 0; i < theCount; i++)
                (this->mUnits[i])->mIsImportant = true;
        }
        for (long j = this->mUnits.size() - 1; j >= 0 && !this->mCanAttackShore; j--) {
            if (this->mUnits[j] && this->mUnits[j]->GetUnit())
                this->mCanAttackShore = gU4E8708[0x2c][(*(this->mUnits.begin() + j))->GetUnit()->mUnitClass->mAmmoType] != 0.0f;
        }
    }
    this->mStats.Compute(this->mUnits);
}

// FUNCTION: EXE 0x45c774
// Returns the share of units that are in danger
float EECPAttackSubGroup::GetDangerRatio(long inArg1)
{
    long theCount = 0;
    float theResult = 0.0f;
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        if (this->mUnits[i]->IsInDanger())
            theCount++;
    }
    if (theCount > 0)
        theResult = (float)theCount / this->mUnits.size();
    return theResult;
}

// FUNCTION: EXE 0x45c7e9
// Fills a location with the group's position, or clears it when only the rally point fallback was found
void EECPAttackSubGroup::GetLocation(EECPILocation& outLocation)
{
    EETileLocation theTile;
    bool theFound = this->FindPosition(theTile, true, false);
    outLocation.Clear();
    if (theFound)
        outLocation.mTile = theTile;
}

// FUNCTION: EXE 0x45c829
// Records whether any unit does area damage, from a researchable area-effect technology (kind 5) or its stats
void EECPAttackSubGroup::UpdateAreaEffect()
{
    EETechTree* theTree = this->mPlayer->GetTechTree();
    this->mHasAreaEffect = false;
    for (long i = this->mUnits.size() - 1; i >= 0 && !this->mHasAreaEffect; i--) {
        EEComplexUnit* theUnit = this->mUnits[i]->GetUnit();
        EE_LOST_DEBUG(6);   // LOST-DEBUG: none: exact only (the original calls mUnits[i]'s operator+ out of line)
        if (!theUnit)
            continue;
        if (theUnit->mUnitClass->GetNumResearchTechnologies() > 0) {
            EEUnitClass* theClass = theUnit->mUnitClass;
            for (long j = theClass->GetNumResearchTechnologies() - 1; j >= 0 && !this->mHasAreaEffect; j--) {
                long& theTech = theClass->mResearchTechnologies[j];
                EETechTreeNode* theNode = theTree->Get(theTech);
                this->mHasAreaEffect = theNode && theNode->mData->mKind == 5;
            }
        }
        if (!this->mHasAreaEffect)
            this->mHasAreaEffect = theUnit->mStats->mAreaEffect > 1.0f;
    }
}

// FUNCTION: EXE 0x45c925
// Rebuilds the move mask from the units' terrain flags (the intersection over all units) and stamps
// the player ID
void EECPAttackSubGroup::BuildMoveMask()
{
    memset(this->mFlags.mFlags, 1, sizeof(this->mFlags.mFlags));
    EECPASGFlags theUnitFlags;   // each unit's flags, ANDed into the mask (was written straight into it)
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        this->mUnits[i]->GetUnit()->mUnitClass->GetTerrainFlags(this->mUnits[i]->GetUnit(), theUnitFlags);
        this->mFlags.And(theUnitFlags);
    }
    this->mFlags.mPlayerID = this->mPlayer->GetPlayerID();
}

// FUNCTION: EXE 0x45c991
// Moves the rally point to the group's position (or the first unit's or its carrier's tile) and sends every active
// unit there; returns false when the sub-group has no units
bool EECPAttackSubGroup::RegroupHere()
{
    GE2DPoint thePoint;
    EETileLocation theTile;
    bool theResult = true;
    if (this->GetNumUnits() != 0) {
        this->FindPosition(theTile, true, false);
        thePoint.SetX(theTile.GetX());
        thePoint.SetY(theTile.GetY());
        if (!gWorld.GetMap()->GetTile((long)thePoint.GetX(), (long)thePoint.GetY())
            || (thePoint.GetX() == 0.0f && thePoint.GetY() == 0.0f)) {
            if ((*this->mUnits.begin())->GetUnit()->mContainer == 0) {
                thePoint.SetX((*this->mUnits.begin())->GetUnit()->GetTileX());
                thePoint.SetY((*(this->mUnits.begin() + 0))->GetUnit()->mTileY);
            } else if ((*(this->mUnits.begin() + 0))->GetUnit()->mContainer->GetTileX() != -10000) {
                thePoint.SetX((*(this->mUnits.begin() + 0))->GetUnit()->mContainer->GetTileX());
                thePoint.SetY((*(this->mUnits.begin() + 0))->GetUnit()->mContainer->mTileY);
            }
        }
        this->mRallyPoint.mTile.mX = (long)thePoint.GetX();
        this->mRallyPoint.mTile.mY = (long)thePoint.GetY();
        if (this->GetNumUnits() != 0) {
            thePoint.mX += 0.5f;
            thePoint.mY += 0.5f;
            for (long i = this->GetNumUnits() - 1; i >= 0; i--) {
                if (this->mUnits[i]->IsDone())
                    continue;
                EEUGLocation* theGoal = new EEUGLocation(thePoint, true, true, false, 0.0f, false, false);
                if (!this->GiveGoal(this->mUnits[i]->GetUnit(), theGoal, 0, kActionPrepareToMove) && theGoal)
                    delete theGoal;
            }
        }
    } else {
        theResult = false;
    }
    return theResult;
}

// FUNCTION: EXE 0x45cb74
// Rolls a new aggression: 42% 0.8-1.0, 25% 0.1-0.2, else 0.1-0.9 (in tenths, at most 1.0); returns it
float EECPAttackSubGroup::RollAggression()
{
    float theRoll = gPhysics->mRandom.Generate();
    if (theRoll <= kASGRollLow)
        this->mAggression = gPhysics->GetRandom(8L, 11L) / 10.0f;
    else if (theRoll >= 0.75f)
        this->mAggression = gPhysics->GetRandom(1L, 3L) / 10.0f;
    else
        this->mAggression = gPhysics->GetRandom(1L, 10L) / 10.0f;
    if (this->mAggression < 0.0f)
        this->mAggression = 0.01f;
    if (this->mAggression > 1.0f)
        this->mAggression = 1.0f;
    return this->mAggression;
}

// FUNCTION: EXE 0x45cc2d
// Fills the route cost weights: all zero except weight 4, which is the aggression
void EECPAttackSubGroup::GetPathCosts(void* inFrom, void* inTo, EECPASGWeights& outWeights)
{
    outWeights.Fill(0.0f);
    outWeights.mValues[4] = this->GetAggression();
}

// FUNCTION: EXE 0x45cc4c
// Returns the wall route weight: the share of units not air or in unit set 0x60; unless on a wonder or
// attack-location task, minus 0.15 per other unit (min 0.01) or, with none, 50% a random 0.15-1.0
float EECPAttackSubGroup::GetWallWeight()
{
    EEUnitSet* theSet = gUnitSetManager.GetSet(0x60);
    float theResult = 1.0f;
    float theCount = 0.0f;
    if (this->mUnits.size() > 0) {
        for (long i = this->mUnits.size() - 1; i >= 0; i--) {
            EEUnitClass* theClass = this->mUnits[i]->GetUnit()->mUnitClass;
            EE_LOST_DEBUG(6);   // LOST-DEBUG: none: exact only (the original calls mUnits[i]'s operator+ out of line)
            if ((theClass->mDomains & kMovementAir) || theSet->Contains(theClass->mUnitType))
                theCount += 1.0f;
        }
        theResult = ((float)this->mUnits.size() - theCount) / this->mUnits.size();
        if (!this->mTask.mIsActive || (this->mTask.mMode != kAGModeWonder && this->mTask.mType != kAGTaskAttackLocation)) {
            if (theCount != 0.0f) {
                theResult -= kASGUnitStep58 * theCount;
                if (kHeightEpsilon > theResult)
                    theResult = kHeightEpsilon;
            } else if (gPhysics->mRandom.Generate() >= kASGRandomChance58) {
                theResult = gPhysics->GetRandom(kASGRandomLow58, kASGRandomHigh58);
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x45cdb1
// Returns the number of targets
long EECPAttackSubGroup::GetNumTargets()
{
    return this->mTargets.size();
}

// FUNCTION: EXE 0x45cdc4
// Returns the rally point tile (the allies flag is ignored)
EETileLocation EECPAttackSubGroup::GetRallyPoint(bool inIncludeAllies)
{
    return this->mRallyPoint.mTile;
}

// FUNCTION: EXE 0x45cddf
// Counts the units that are not done
long EECPAttackSubGroup::CountFightingUnits()
{
    long theCount = 0;
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        if (!this->mUnits[i]->IsDone())
            theCount++;
    }
    return theCount;
}

// FUNCTION: EXE 0x45ce19
// Moves the given units into another sub-group that copies this one's task, rally point, timers and settings; counts
// a split (unless kind 7), re-ranks both and resets both split kinds; returns true when there were units
bool EECPAttackSubGroup::SplitInto(EECPAttackSubGroup* inOther, std::vector<EEComplexUnit*>& inUnits)
{
    EECPILocation theLocation;
    EECPAttackSubGroupUnit* theUnit = NULL;
    inOther->mArea = this->mArea;
    inOther->mNextWaypoint = this->mNextWaypoint;
    inOther->mTask = this->mTask;
    inOther->mRallyPoint = this->mRallyPoint;
    inOther->mGroup = this->mGroup;
    inOther->mPlayer2 = this->mPlayer2;
    inOther->mRequest = NULL;
    inOther->mNextTaskCheckTime = 0;
    inOther->mU1BC = 0;
    inOther->mNextFollowTime = 0;
    inOther->mNextScanTime = 0;
    inOther->mNextUpdateTime = 0;
    inOther->mU1B0 = 0;
    inOther->mNextPositionTime = 0;
    inOther->mGatherDeadline = this->mGatherDeadline;
    inOther->mScanRange = this->mScanRange;
    inOther->mU1B4 = this->mU1B4;
    inOther->mState = kASGStateIdle;
    inOther->mMission = this->mMission;
    inOther->mSplitKind = this->mSplitKind;
    inOther->mKind = this->mKind;
    inOther->mB1E0 = this->mB1E0;
    inOther->mB1E1 = this->mB1E1;
    inOther->mB1E2 = this->mB1E2;
    inOther->mIsOutOfFight = this->mIsOutOfFight;
    inOther->mHasAreaEffect = this->mHasAreaEffect;
    inOther->mAggression = this->mAggression;
    inOther->mPosition = this->mPosition;
    if (inUnits.size() > 0 && this->mSplitKind != 7)
        this->mNumSplits++;
    inOther->mNumSplits = this->mNumSplits;
    for (long i = inUnits.size() - 1; i >= 0; i--) {
        EEComplexUnit*& theTarget = inUnits[i];
        theUnit = EECPCreateSubGroupUnit(theTarget, inOther, 0);
        theUnit->Virtual48();
        if (inOther->mIsOutOfFight)
            theUnit->mIsOutOfFight = true;
        inOther->mUnits.push_back(theUnit);
    }
    inOther->RankUnits();
    bool theResult = inUnits.size() > 0;
    this->RemoveUnits(inUnits);
    if (this->mUnits.size() > 0)
        this->RefreshUnits();
    if (inOther->mUnits.size() > 0)
        inOther->RefreshUnits();
    this->mSplitKind = 0;
    inOther->mSplitKind = 0;
    return theResult;
}

// FUNCTION: EXE 0x45d0e3
// Cancels the personnel request and hands every unit back: to the attack commander when the group is bound to a
// continent and the unit is not a transport, else to personnel
void EECPAttackSubGroup::Disband()
{
    std::vector<EEComplexUnit*> theUnits;
    EECPPersonnel* thePersonnel = this->mGroup->mCommander->mPersonnel;
    if (this->mRequest) {
        this->mRequest->Cancel();
        this->mRequest->ReleaseAll();
        this->mRequest->Dereference();
        this->mRequest = NULL;
    }
    long i;
    for (i = this->mUnits.size() - 1; i >= 0; i--) {
        this->DropTargetOf(this->mUnits[i]);
        (this->mUnits[i])->GetUnit()->Reference();
        theUnits.push_back((this->mUnits[i])->GetUnit());
        (this->mUnits[i])->ReleaseTarget();
        (this->mUnits[i])->Dereference();
    }
    this->mUnits.clear();
    for (i = theUnits.size() - 1; i >= 0; i--) {
        if (this->mGroup->mContinentID != 0 && !theUnits[i]->mUnitClass->IsTransport()) {
            this->mPlayer->mCombatManager->mAttackCommander->TakeUnit(theUnits[i]);
            EE_LOST_TRACE(theUnits[i]);   // LOST-DEBUG: none: exact only (the original evaluates theUnits[i] here, out of line)
            EE_LOST_TRACE(theUnits[i]);   // LOST-DEBUG: none: exact only (the original evaluates theUnits[i] here, out of line)
            EE_LOST_TRACE(theUnits[i]);   // LOST-DEBUG: none: exact only (the original evaluates theUnits[i] here, out of line)
            EE_LOST_TRACE(theUnits[i]);   // LOST-DEBUG: none: exact only (the original evaluates theUnits[i] here, out of line)
        } else
            thePersonnel->ReturnUnit(theUnits[i]);
        theUnits[i]->Dereference();
    }
}

// FUNCTION: EXE 0x45d08f
// Takes a target record for the unit from the shared pool, with no attackers or nearby targets counted
EECPASTargetUnit* EECPAttackSubGroup::NewTarget(EEComplexUnit* inUnit)
{
    EECPASTargetUnit* theTarget = EECPAttackSubGroup::sTargetPool.New();
    theTarget->SetUnit(inUnit);
    theTarget->mNumAttackers = 0;
    theTarget->mNumNearbyTargets = 0;
    return theTarget;
}

// FUNCTION: EXE 0x45d0b3
// Clears a target record and returns it to the shared pool
void EECPAttackSubGroup::ReleaseTarget(EECPASTargetUnit* inTarget)
{
    inTarget->Clear();
    EECPAttackSubGroup::sTargetPool.Delete(inTarget);
}

// FUNCTION: EXE 0x45d0cb
// Dumps the target pool's statistics and frees the pool
void EECPAttackSubGroup::ReleaseTargetPool()
{
    EECPAttackSubGroup::sTargetPool.DumpStats(false);
    EECPAttackSubGroup::sTargetPool.Release();
}

// FUNCTION: EXE 0x45d349
// Returns the tile distance between this sub-group's position and another's (0 for none, itself or the same tile)
float EECPAttackSubGroup::DistanceTo(EECPAttackSubGroup* inOther)
{
    EETileLocation theOther;
    EETileLocation theThis;
    float theResult = 0.0f;
    if (inOther && inOther != this) {
        inOther->FindPosition(theOther, true, false);
        this->FindPosition(theThis, true, false);
        if (theOther.GetX() != theThis.GetX() || theOther.GetY() != theThis.GetY())
            theResult = theThis.DistanceToF(theOther);
    }
    return theResult;
}

// FUNCTION: EXE 0x45d3b3
// Returns the squared tile distance between this sub-group's position and another's (0 for none, itself or the same
// tile)
float EECPAttackSubGroup::DistanceSquaredTo(EECPAttackSubGroup* inOther)
{
    EETileLocation theOther;
    EETileLocation theThis;
    float theResult = 0.0f;
    if (inOther && inOther != this) {
        inOther->FindPosition(theOther, true, false);
        this->FindPosition(theThis, true, false);
        if (theOther.GetX() != theThis.GetX() || theOther.GetY() != theThis.GetY())
            theResult = theThis.DistanceToSquaredF(theOther);
    }
    return theResult;
}

// FUNCTION: EXE 0x45d41d
// With two or more units, buckets them by continent (carried units by their carrier's, or skipped) into new vectors;
// returns the number of continents
long EECPAttackSubGroup::GroupUnitsByContinent(std::map<unsigned short, std::vector<EECPAttackSubGroupUnit*>*>& outUnits, bool inSkipCarried)
{
    if (this->mUnits.size() > 1) {
        for (long i = this->mUnits.size() - 1; i >= 0; i--) {
            if ((this->mUnits[i])->GetUnit()->mContainer != 0 && inSkipCarried)
                continue;
            unsigned short theID;
            if ((this->mUnits[i])->GetUnit()->mContainer == 0)
                theID = gContinentManager.GetContinentID((this->mUnits[i])->GetUnit());
            else
                theID = gContinentManager.GetContinentID((this->mUnits[i])->GetUnit()->mContainer);
            std::map<unsigned short, std::vector<EECPAttackSubGroupUnit*>*>::iterator theIt = outUnits.find(theID);
            if (theIt == outUnits.end()) {
                std::vector<EECPAttackSubGroupUnit*>* theUnits = new std::vector<EECPAttackSubGroupUnit*>;
                theUnits->push_back(this->mUnits[i]);
                outUnits.insert(std::map<unsigned short, std::vector<EECPAttackSubGroupUnit*>*>::value_type(theID,
                                                                                                            theUnits));
            } else {
                (*theIt).second->push_back(this->mUnits[i]);
            }
        }
    }
    return outUnits.size();
}
// FUNCTION: EXE 0x45d5a2
// With two or more units, buckets them by their footprint area of the given kind (carriers for carried units, or
// skipped) into new vectors; returns the number of areas
long EECPAttackSubGroup::GroupUnitsByWaterArea(std::map<unsigned short, std::vector<EECPAttackSubGroupUnit*>*>& outUnits, long inKind, bool inSkipCarried)
{
    if (this->mUnits.size() > 1) {
        for (long i = this->mUnits.size() - 1; i >= 0; i--) {
            if ((this->mUnits[i])->GetUnit()->mContainer != 0 && inSkipCarried)
                continue;
            unsigned short theID;
            if ((this->mUnits[i])->GetUnit()->mContainer == 0)
                theID = (this->mUnits[i])->GetUnit()->GetFootprintContinent(inKind);
            else
                theID = (this->mUnits[i])->GetUnit()->mContainer->GetFootprintContinent(inKind);
            std::map<unsigned short, std::vector<EECPAttackSubGroupUnit*>*>::iterator theIt = outUnits.find(theID);
            if (theIt == outUnits.end()) {
                std::vector<EECPAttackSubGroupUnit*>* theUnits = new std::vector<EECPAttackSubGroupUnit*>;
                theUnits->push_back(this->mUnits[i]);
                outUnits.insert(std::map<unsigned short, std::vector<EECPAttackSubGroupUnit*>*>::value_type(theID,
                                                                                                            theUnits));
            } else {
                (*theIt).second->push_back(this->mUnits[i]);
            }
        }
    }
    return outUnits.size();
}

// FUNCTION: EXE 0x45d725
// Queues a split order for the sub-group with its attack group, plus an order to set it idle when asked
void EECPAttackSubGroup::RequestSplit(bool inResetState)
{
    EECPAGOrder theOrder;
    theOrder.mType = kAGOrderSplitSubGroup;
    theOrder.SetSubGroup(this);
    theOrder.mIsActive = true;
    this->mGroup->mOrders.push_back(theOrder);
    if (!inResetState)
        return;
    theOrder.mState = 0;
    theOrder.mType = kAGOrderSetState;
    theOrder.mIsActive = true;
    this->mGroup->mOrders.push_back(theOrder);
}

// FUNCTION: EXE 0x45d790
// Sends the sub-group to the task location when forced or more than 10 tiles away; always returns true
bool EECPAttackSubGroup::MoveToTask(bool inForce)
{
    EECPILocation theLocation;
    this->GetLocation(theLocation);
    if (inForce || theLocation.mTile.DistanceToSquared(this->mTask.mLocation.mTile) > 100)
        this->SetDestination(this->mTask.mLocation);
    return true;
}

// FUNCTION: EXE 0x45d7de
// Requests an automatic path with the group's move mask for its player and counts the update for this tick
EEAutoPathRequest* EECPAttackSubGroup::NewPathRequest(const EETileCoordinate& inFrom, const EETileCoordinate& inTo, long inRevs,
                                                 EEAIGraphPathGoalModifierBase* inModifier)
{
    unsigned long thePlayerID = this->mPlayer->GetPlayerID();
    EEAutoPathRequest* theRequest = gPathManager.RequestPath(thePlayerID, inFrom, inTo, &this->mFlags, 1, inRevs, inModifier, 1);
    this->mPlayer->mCombatManager->mAttackCommander->mUpdatesThisTick++;
    return theRequest;
}

// FUNCTION: EXE 0x45d829
// Returns true when the given unit is in the sub-group
bool EECPAttackSubGroup::HasUnit(EEComplexUnit* inUnit)
{
    bool theResult = false;
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        if (this->mUnits[i]->GetUnit() == inUnit) {
            theResult = true;
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x45d865
// Adds the sub-group to the list once per record of the given unit; returns the list's size
long EECPAttackSubGroup::ListIfHasUnit(EEComplexUnit* inUnit, std::vector<EECPAttackSubGroup*>& outGroups)
{
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        if (this->mUnits[i]->GetUnit() == inUnit)
            outGroups.push_back(this);
    }
    return outGroups.size();
}

// FUNCTION: EXE 0x45d8c3
// Builds the formation and places it at the location, facing along the heading from the group's position to it
void EECPAttackSubGroup::PlaceFormation(const EECPILocation& inLocation)
{
    EETileLocation theTile;
    theTile = this->GetPosition();
    float theDistance = EEGetSeaHeading(theTile, inLocation.mTile);
    this->mFormation->Build();
    this->mFormation->mNoForcedMove = false;
    this->mFormation->Place(inLocation, theDistance);
}

// FUNCTION: EXE 0x45d92f
// Moves every unit the personnel request delivered into the sub-group, regroups, releases the request and refreshes;
// returns RegroupHere's result (the largest scan range found is never stored)
bool EECPAttackSubGroup::TakeRequestedUnits()
{
    EEPlayer* thePlayer = this->mGroup->mTask.mPlayer;
    EECPIntelligence* theIntelligence = this->mGroup->mCommander->mIntelligence;
    EECPIntelligencePlayerFile* theFile = theIntelligence->GetFile(thePlayer);
    float theMax = (float)this->mScanRange;
    while (!this->mRequest->IsEmpty()) {
        EEComplexUnit* theUnit = this->mRequest->mUnits.back().mUnit;
        this->mRequest->PopSlot();
        if (!theUnit)
            continue;
        float theValue = theFile->GetScanRange(theUnit->GetLineOfSight());
        if (theValue > theMax)
            theMax = theValue;
        EECPAttackSubGroupUnit* theSubGroupUnit = EECPCreateSubGroupUnit(theUnit, this, 0);
        theSubGroupUnit->Virtual48();
        this->mUnits.push_back(theSubGroupUnit);
    }
    this->RankUnits();
    bool theResult = this->RegroupHere();
    this->mGroup->mCommander->mPersonnel->ReleaseRequest(this->mRequest->mID);
    this->mRequest->Dereference();
    this->mRequest = NULL;
    this->RefreshUnits();
    return theResult;
}

// FUNCTION: EXE 0x45da23
// Rebuilds the target lists around the group's area (scan range, at most 20 tiles), counts them and assigns
// attackers; schedules the next scan by state, player count and target count
void EECPAttackSubGroup::ScanTargets(bool inTime)
{
    unsigned long theCount = 0;
    long theMargin = this->mScanRange;
    EEAIAStarBounds theBounds;
    EETileLocation theTile;
    this->ClearTargets();
    if (this->FindPosition(theTile, true, false)) {
        theBounds = this->mArea;
        if (theMargin >= 20)
            theMargin = 20;
        theBounds.Grow(theMargin, true);
        this->CollectTargets(&theBounds, inTime);
        if (this->mHasAreaEffect)
            this->CountTargetNeighbors();
        this->CountTargets();
        theCount = this->mTargets.size() + this->mSiegeTargets.size();
    }
    if (this->mState == kASGStateAttack)
        this->mNextScanTime = gNumPlayers * 500 + gPhysics->mGameTime;
    else if (this->mState == kASGStateClearSubSector)
        this->mNextScanTime = gPhysics->mGameTime;
    else
        this->mNextScanTime = gNumPlayers * 1500 + gPhysics->mGameTime;
    if (theCount >= 30) {
        if (theCount < 40)
            this->mNextScanTime += 5000;
        else if (theCount < 80 || this->mTargets.size() < 5)
            this->mNextScanTime += 8000;
        else
            this->mNextScanTime += 19000;
    }
    if (this->mTargets.size() > 0)
        this->AssignTargets();
}
// The clusters EEGroup::MakeClusters made, deleted (COMDAT 0x62340 after the unit's functions; returns the count)
// (provisional name)
inline long EECPASGDeleteClusters(std::vector<EEGroupCluster*>& ioClusters)
{
    long theCount = 0;
    for (std::vector<EEGroupCluster*>::iterator theIt = ioClusters.begin(); theIt != ioClusters.end(); theIt++) {
        if (*theIt)
            delete *theIt;
        theCount++;
    }
    ioClusters.clear();
    return theCount;
}

// Sort's comparator is proved by template bodies 0x62b97.. reading ASGU+0x64.
struct EECPASGUnitPriorityGreater {
    bool operator()(EECPAttackSubGroupUnit* inA, EECPAttackSubGroupUnit* inB) const
    {
        float theA = inA->mAggression;
        float theB = inB->mAggression;
        return theA > theB;
    }
};

// FUNCTION: EXE 0x45db99
// Picks units to split off by split kind: a speed-ranked fraction (4; always 0 or 0.5: integer tenths), the most
// aggressive of half (2), the first continent's (7), a random cluster (9), else the first half
void EECPAttackSubGroup::PickSplitUnits(std::vector<EEComplexUnit*>& outUnits)
{
    std::vector<EEGroupCluster*> theClusters;
    std::map<unsigned short, std::vector<EECPAttackSubGroupUnit*>*> theContinents;
    std::vector<EECPAttackSubGroupUnit*> theSorted;
    std::vector<EEComplexUnit*> theUnits;
    std::vector<EECPAttackSubGroupUnit*> theSelected;
    long theCount = this->mUnits.size() / 2;
    bool theReverse = gPhysics->GetRandom(0L, 2L) != 0;
    if (this->mUnits.size() <= 1)
        return;
    if (this->mSplitKind == 4) {
        float theFraction;
        if (gPhysics->GetRandom() < 0.65f) {
            theFraction = gPhysics->GetRandom(3L, 7L) / 10.0f;   // 0.3..0.7 (was an integer division: 0)
            if (theFraction * this->mUnits.size() <= 0.0f)
                theFraction = 0.5f;
        } else {
            theFraction = gPhysics->GetRandom(3L, 7L) / 10.0f;   // 0.3..0.7 (was an integer division: 0)
        }
        this->PickByUpgrade(theSelected, theReverse, theFraction);
        theCount = theSelected.size();
        if (theCount >= this->mUnits.size() && theSelected.size() > 0)
            theSelected.pop_back();
        for (std::vector<EECPAttackSubGroupUnit*>::iterator theIt = theSelected.begin(); theIt != theSelected.end(); ++theIt) {
            if (*theIt && (*theIt)->GetUnit() && (*theIt)->GetUnit()->mContainer == 0)
                outUnits.push_back((*theIt)->GetUnit());
        }
    } else if (this->mSplitKind == 2) {
        theSorted = this->mUnits;
        std::sort(theSorted.begin(), theSorted.end(), EECPASGUnitPriorityGreater());
        if ((long)theSorted.size() < theCount)
            theCount = theSorted.size();
        for (long i = 0; i < theCount; ++i) {
            if (theSorted[i]->mAggression < 0.7f)
                break;
            outUnits.push_back(theSorted[i]->GetUnit());
        }
    } else if (this->mSplitKind == 7) {
        this->GroupUnitsByContinent(theContinents, true);
        if (theContinents.size() > 1) {
            std::vector<EECPAttackSubGroupUnit*>* theFirst = (*theContinents.begin()).second;
            for (long i = theFirst->size() - 1; i >= 0; --i)
                outUnits.push_back((*(theFirst->begin() + i))->GetUnit());
        }
        for (std::map<unsigned short, std::vector<EECPAttackSubGroupUnit*>*>::iterator theIt = theContinents.begin();
             theIt != theContinents.end(); ++theIt)
            delete (*theIt).second;
        theContinents.clear();
    } else if (this->mSplitKind == 9) {
        theUnits.reserve(this->mUnits.size());
        for (long i = this->mUnits.size() - 1; i >= 0; --i)
            theUnits.push_back((this->mUnits[i])->GetUnit());
        EEGroup::MakeClusters(theUnits, theClusters, 0, false);
        if (theClusters.size() > 1) {
            long theIndex = gPhysics->GetRandomIndex(theClusters);
            outUnits.insert(outUnits.end(), theClusters[theIndex]->mUnits.begin(), theClusters[theIndex]->mUnits.end());
        }
        EECPASGDeleteClusters(theClusters);
    } else {
        if (this->GetNumUnits() - theCount > 0) {
            for (long i = theCount - 1; i >= 0; --i) {
                EEComplexUnit* theUnit = this->GetUnitUnit(i);
                if (theUnit && theUnit->mContainer == NULL)
                    outUnits.push_back(theUnit);
            }
        }
    }
}

// FUNCTION: EXE 0x45e01b
// Returns the sub-group's record for the given unit, or NULL
EECPAttackSubGroupUnit* EECPAttackSubGroup::FindUnit(EEComplexUnit* inUnit)
{
    EECPAttackSubGroupUnit* theResult = NULL;
    const std::vector<EECPAttackSubGroupUnit*>& theUnits = this->mUnits;
    for (long i = this->mUnits.size() - 1; i >= 0 && theResult == NULL; i--) {
        if (theUnits[i]->GetUnit() == inUnit)
            theResult = theUnits[i];
    }
    return theResult;
}

// FUNCTION: EXE 0x45e05b
// Returns true when the task player keeps a visibility map and sees none of the units
bool EECPAttackSubGroup::IsUnseenByTaskPlayer()
{
    EEPlayer* thePlayer = gWorld.mPlayers[this->mTask.GetPlayerID()];
    bool theResult = false;
    if (thePlayer && thePlayer->mSaveVisMap) {
        theResult = true;
        for (long i = this->mUnits.size() - 1; i >= 0; i--) {
            if (EEIsSeenBy(this->mUnits[i]->GetUnit(), thePlayer, true)) {
                theResult = false;
                break;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x45e0c4
// Returns the centre tile of the given units' selection bounds, or (0,0) for an empty list
void EECPAttackSubGroup::GetUnitsCenter(EETileLocation& outTile, std::vector<EECPAttackSubGroupUnit*>& inUnits)
{
    std::vector<EEUnit*> theUnits;
    GE2DPoint theCenter;
    GERectangle theBounds;
    if (inUnits.size() > 0) {
        theUnits.reserve(inUnits.size());
        for (long i = inUnits.size() - 1; i >= 0; i--)
            theUnits.push_back(inUnits[i]->GetUnit());
        EEGetSelectionBounds(theUnits, theBounds, 0);
        theBounds.GetCenter(theCenter);
        outTile.mX = (long)theCenter.GetX();
        outTile.mY = (long)theCenter.GetY();
    } else {
        outTile.mX = 0;
        outTile.mY = 0;
    }
}

// FUNCTION: EXE 0x45e1b6
// Returns the continent holding the most units (carriers stand in for carried units), or 0
unsigned short EECPAttackSubGroup::GetMainContinent()
{
    std::map<unsigned short, long> theCounts;
    unsigned short theResult = 0;
    std::map<unsigned short, long>::iterator theIt;
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        EEComplexUnit* theUnit = (this->mUnits[i])->GetUnit();
        if (theUnit->mContainer)
            theUnit = theUnit->mContainer;
        unsigned short theID = gContinentManager.GetUnitContinentID(theUnit);
        if (theID == 0)
            continue;
        theIt = theCounts.find(theID);
        if (theIt != theCounts.end())
            theIt->second++;
        else
            theCounts.insert(std::map<unsigned short, long>::value_type(theID, 1));
    }
    if (theCounts.size() > 0) {
        theIt = theCounts.begin();
        std::pair<unsigned short, long> theBest;
        theBest = *theIt;
        for (theIt++; theIt != theCounts.end(); theIt++) {
            if (theIt->second > theBest.second)
                theBest = *theIt;
        }
        theResult = theBest.first;
    }
    return theResult;
}

// FUNCTION: EXE 0x45e2c8
// Appends every living unit that is not done to the list
void EECPAttackSubGroup::CollectUnits(std::vector<EEComplexUnit*>& outUnits)
{
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        if (!(*(this->mUnits.begin() + i))->IsDone() && (*(this->mUnits.begin() + i))->GetUnit()->mHitPoints > 0)
            outUnits.push_back((*(this->mUnits.begin() + i))->GetUnit());
    }
}

// FUNCTION: EXE 0x45e328
// Returns the intelligence file on the task's player
EECPIntelligencePlayerFile* EECPAttackSubGroup::GetTargetPlayerFile()
{
    EEPlayer* thePlayer = gWorld.mPlayers[this->mTask.GetPlayerID()];
    return this->mPlayer->mCombatManager->GetIntelligence()->GetFile(thePlayer);
}

// FUNCTION: EXE 0x45e354
// Returns true when every unit is near its path's end (under 3 waypoints when fast, 7 otherwise), idle, or waiting
// on an obstacle or a collision
bool EECPAttackSubGroup::AreAllUnitsSettled()
{
    long theCount = 0;
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        EEComplexUnit* theUnit = (*(this->mUnits.begin() + i))->GetUnit();
        long theLimit;
        if (theUnit->GetMaxVelocity() > kFastUnitVelocity)
            theLimit = 3;
        else
            theLimit = 7;
        if ((long)theUnit->mWaypoints.size() < theLimit ||  // signed compare (jl 0x45e3a3)
            (theUnit->mCommand ? theUnit->mCommand->mType : 0) == kActionIdle ||
            ((theUnit->mCommand ? theUnit->mCommand->mType : 0) >= kActionWaitForObstacleToClear &&
             (theUnit->mCommand ? theUnit->mCommand->mType : 0) <= kActionPostCollisionNextWaypoint))
            theCount++;
    }
    return theCount == this->mUnits.size() ? true : false;
}

// FUNCTION: EXE 0x45e424
// Returns true when there are no targets
bool EECPAttackSubGroup::HasNoTargets()
{
    return this->GetNumTargets() ? false : true;
}

// FUNCTION: EXE 0x45e430
// Returns whether the group is at the location: same path sub-sector or within 8 tiles, or a neighbouring
// sub-sector once all units have settled; false when it has no position
bool EECPAttackSubGroup::IsAt(const EECPILocation& inLocation)
{
    EEAIPathSubSector* theSector = inLocation.GetPathSubSector();
    EETileLocation theTile;
    bool theResult = this->FindPosition(theTile, true, false);
    if (theResult && theTile.GetX() != -10000) {
        EEAIPathSubSector* theOwn = gContinentManager.GetPathSubSectorAt(theTile.GetX(), theTile.GetY());
        if (theOwn) {
            theResult = theOwn == theSector || theTile.DistanceToSquared(inLocation.mTile) <= 0x40;
            if (!theResult && theOwn->IsNeighbor(theSector))
                theResult = this->AreAllUnitsSettled();
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x45e4c9
// Returns true: every location counts as reachable
bool EECPAttackSubGroup::CanReach(const EECPILocation* inLocation)
{
    std::vector<EECPAttackSubGroupUnit*>::iterator theIt = this->mUnits.begin();
    return true;
}

// FUNCTION: EXE 0x45e4d7
// Returns whether the given unit is in the sub-group and marked important
bool EECPAttackSubGroup::IsImportantUnit(EEComplexUnit* inUnit)
{
    bool theResult = false;
    for (unsigned long i = 0; i < this->mUnits.size(); i++) {
        if (this->mUnits[i]->GetUnit() == inUnit) {
            theResult = this->mUnits[i]->mIsImportant;
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x45e513
// Returns the group's position tile
EETileLocation EECPAttackSubGroup::GetPosition()
{
    EETileLocation theTile;
    this->FindPosition(theTile, true, false);
    return theTile;
}

// FUNCTION: EXE 0x45e53e
// Returns the total transport weight of the units
long EECPAttackSubGroup::GetCargoSize()
{
    long theResult = 0;
    for (long i = this->mUnits.size() - 1; i >= 0; i--)
        theResult += this->mUnits[i]->GetUnit()->mUnitClass->mWeight;
    return theResult;
}

// FUNCTION: EXE 0x45e576
// Returns the squared distance left to the task over the squared rally-to-task distance (0 when they coincide)
float EECPAttackSubGroup::GetDistanceLeftRatio()
{
    EETileLocation theNow;
    float theResult = 0.0f;
    long theTotal = this->mRallyPoint.mTile.DistanceToSquared(this->mTask.mLocation.mTile);
    theNow = this->GetPosition();
    long theLeft = theNow.DistanceToSquared(this->mTask.mLocation.mTile);
    if (theTotal > 0)
        theResult = (float)theLeft / theTotal;
    return theResult;
}

// FUNCTION: EXE 0x45e5dd
// Returns true when the task's unit is over 10 tiles from the units' centre, or, for a combat task without a unit,
// when the task re-check time has passed
bool EECPAttackSubGroup::ShouldRefreshTask()
{
    EETileLocation theTile;
    bool theResult = false;
    if (this->mTask.mIsActive) {
        if (this->mTask.mUnit == NULL) {
            theResult = this->mTask.mMode == kAGModeCombat && this->mNextTaskCheckTime < gPhysics->mGameTime;
        } else {
            this->GetUnitsCenter(theTile, this->mUnits);
            theResult = this->mTask.mUnit->DistanceSquaredToTileXY(theTile.GetX(), theTile.GetY()) > 100.0f;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x45e656
// Lists the given fraction of the units ordered by top speed, fastest or slowest first
void EECPAttackSubGroup::PickByUpgrade(std::vector<EECPAttackSubGroupUnit*>& outUnits, bool inFastestFirst, float inFraction)
{
    std::multimap<float, EECPAttackSubGroupUnit*> theUnits;
    std::multimap<float, EECPAttackSubGroupUnit*>::iterator theIt;
    std::multimap<float, EECPAttackSubGroupUnit*>::reverse_iterator theReverse;
    if (inFraction > 0.0f && this->mUnits.size() > 0) {
        for (long i = this->mUnits.size() - 1; i >= 0; i--) {
            if (this->mUnits[i]->GetUnit()) {  // mUnits[i] x3: owner-approved trade (2026-11-01): VirtualE0 58.33 -> 90.68, COMDAT 0x461393 (_Tree::rend) no longer emitted
                float theValue = this->mUnits[i]->GetUnit()->mStats->mMaxVelocity;
                theUnits.insert(std::make_pair(theValue,
                    this->mUnits[i]));
            }
        }
        long theCount = (long)(this->mUnits.size() * inFraction);
        if (inFastestFirst) {
            for (theReverse = theUnits.rbegin(), i = 0; theReverse != theUnits.rend() && i < theCount; ++theReverse, ++i)
                outUnits.push_back((*theReverse).second);
        } else {
            for (theIt = theUnits.begin(), i = 0; theIt != theUnits.end() && i < theCount; ++theIt, ++i)
                outUnits.push_back((*theIt).second);
        }
    }
}

// FUNCTION: EXE 0x45e7f9
// Gives a living, placed, enabled unit the goal and starts the action, counting the update; returns false otherwise
bool EECPAttackSubGroup::GiveGoal(EEComplexUnit* inUnit, EEUnitGoal* inGoal, bool inKeepCurrent, long inAction)
{
    bool theResult = false;
    if (inUnit && inUnit->mHitPoints > 0 && inUnit->GetTileX() != -10000 && !inUnit->mIsDisabled) {
        this->mGroup->mCommander->mUpdatesThisTick++;
        inUnit->SetGoal(inGoal, inKeepCurrent, 0);
        inUnit->StartAction(inAction);
        inUnit->mUnitClass->ActionChanged(inUnit);
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x45e852
// Sets every unit's AI state
void EECPAttackSubGroup::SetUnitsState(long inAIState)
{
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        EEComplexUnit* theUnit = (*(this->mUnits.begin() + i))->GetUnit();
        if (theUnit && theUnit->mAIState != inAIState)
            theUnit->mAIState = inAIState;
    }
}

// FUNCTION: EXE 0x45e890
// Queues an order for the attack group to remove the sub-group
void EECPAttackSubGroup::RequestRemoval()
{
    EECPAGOrder theOrder;
    theOrder.SetSubGroup(this);
    theOrder.mType = kAGOrderRemoveSubGroup;
    theOrder.mIsActive = true;
    this->mGroup->mOrders.push_back(theOrder);
}


// FUNCTION: EXE 0x45e8d2
// Decides whether a never-split group of 3+ units splits: with clusters kind 9 (35%, 65% above easy), else 10%
// (35%) for kind 7, 2, 4 or 3; queues the split and an idle order; returns true when it splits
bool EECPAttackSubGroup::DecideSplit()
{
    std::vector<EEComplexUnit*> theUnits;
    std::vector<EEGroupCluster*> theClusters;
    std::map<unsigned short, std::vector<EECPAttackSubGroupUnit*>*> theContinents;
    bool theResult = false;
    std::map<unsigned short, std::vector<EECPAttackSubGroupUnit*>*>::iterator theIt;
    long theKind = 0;
    EECPAGOrder theOrder;
    if (this->mNumSplits == 0 && this->mUnits.size() > 2 && this->mSplitKind != 10 && this->mGroup->mContinentID == 0) {
        theUnits.reserve(this->mUnits.size());
        for (long i = this->mUnits.size() - 1; i >= 0; i--)
            theUnits.push_back((this->mUnits[i])->GetUnit());
        EEGroup::MakeClusters(theUnits, theClusters, 0, false);
        if (theClusters.size() > 0) {
            float theChance = 0.35f;
            if (this->mPlayer->mDifficulty != kDifficultyEasy)
                theChance = 0.65f;
            if (gPhysics->GetRandom() <= theChance) {
                theResult = true;
                theKind = 9;
            }
        } else {
            float theChance = 0.1f;
            if (this->mPlayer->mDifficulty != kDifficultyEasy)
                theChance = 0.35f;
            if (gPhysics->GetRandom() <= theChance) {
                theResult = true;
                this->GroupUnitsByContinent(theContinents, true);
                if (theContinents.size() > 1)
                    theKind = 7;
                else if (gPhysics->mRandom.Generate() <= 0.25f)
                    theKind = 2;
                else {
                    theKind = 4;
                    if (gPhysics->mRandom.Generate() > 0.15f)
                        theKind = 3;
                }
                for (theIt = theContinents.begin(); theIt != theContinents.end(); theIt++) {
                    if ((*theIt).second)
                        delete (*theIt).second;
                }
                theContinents.clear();
            }
        }
        EECPASGDeleteClusters(theClusters);
        if (theResult) {
            this->mSplitKind = theKind;
            theOrder.mType = kAGOrderSplitSubGroup;
            theOrder.mSplitKind = theKind;
            theOrder.SetSubGroup(this);
            theOrder.mIsActive = true;
            this->mGroup->mOrders.push_back(theOrder);
            theOrder.mState = 0;
            theOrder.mSplitKind = 0;
            theOrder.mType = kAGOrderSetState;
            theOrder.mIsActive = true;
            this->mGroup->mOrders.push_back(theOrder);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x45eb96
// Returns the sub-group unit that the given unit's attack goal targets, or NULL
EECPAttackSubGroupUnit* EECPAttackSubGroup::FindUnitTargetedBy(EEComplexUnit* inUnit)
{
    EEUGAttackUnit* theGoal = dynamic_cast<EEUGAttackUnit*>(inUnit->GetLoadGoal());
    EECPAttackSubGroupUnit* theResult = NULL;
    if (theGoal) {
        for (long i = this->GetNumUnits() - 1; i >= 0; i--) {
            EEUnit* theTarget = theGoal->mTarget;
            if (theTarget == this->GetUnitUnit(i)) {
                theResult = this->GetUnitAt(i);
                break;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x45ebf9
// Reads the units (each created by its saved kind), path, area, waypoint, rally point, task, request ID, timers,
// state and flags from the saved game; returns the first read error
long EECPAttackSubGroup::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theKind = 0;
    long theRequestID = 0;
    unsigned long theCount = 0;
    unsigned long i;
    if (inFile.Read(theCount) == kErrorOK) {
        this->mUnits.resize(theCount);
        for (i = 0; i < theCount; i++) {
            if (inFile.Read(theKind) == kErrorOK) {
                (*(this->mUnits.begin() + i)) = EECPCreateSubGroupUnitOfKind(NULL, this, theKind);
                (*(this->mUnits.begin() + i))->Load(inMajorVersion, inMinorVersion, inFile);
            }
        }
    }
    theCount = 0;
    long theResult = inFile.Read(theCount);
    if (theResult != kErrorOK)
        return theResult;
    this->mPath.resize(theCount);
    for (i = 0; i < theCount; i++) {
        if (theResult != kErrorOK)
            continue;
        theResult = this->mPath[i].mTile.Load(inFile);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPath[i].mSectorKind);
    }
    if (theResult == kErrorOK) {
        theResult = this->mArea.Load(inFile);
        if (theResult == kErrorOK) {
            theResult = this->mNextWaypoint.Load(inMajorVersion, inMinorVersion, inFile);
            if (theResult == kErrorOK) {
                theResult = this->mRallyPoint.Load(inMajorVersion, inMinorVersion, inFile);
                if (theResult == kErrorOK) {
                    theResult = this->mTask.Load(inMajorVersion, inMinorVersion, inFile);
                    if (theResult == kErrorOK) {
                        theRequestID = 0;
                        theResult = inFile.Read(theRequestID);
                        this->mRequest = reinterpret_cast<EECPPersonnelRequest*>(theRequestID);
                        if (theResult == kErrorOK)
                            theResult = inFile.Read(this->mScanRange);
                        if (theResult == kErrorOK)
                            theResult = inFile.Read(this->mAggression);
                        if (theResult == kErrorOK)
                            theResult = inFile.Read(this->mID);
                        if (theResult == kErrorOK)
                            theResult = inFile.Read(this->mNextScanTime);
                        if (theResult == kErrorOK)
                            theResult = inFile.Read(this->mNextUpdateTime);
                        if (theResult == kErrorOK)
                            theResult = inFile.Read(this->mU1B0);
                        if (theResult == kErrorOK)
                            theResult = inFile.Read(this->mU1B4);
                        if (theResult == kErrorOK)
                            theResult = inFile.Read(this->mNextFollowTime);
                        if (theResult == kErrorOK)
                            theResult = inFile.Read(this->mU1BC);
                        if (theResult == kErrorOK)
                            theResult = inFile.Read(this->mNextTaskCheckTime);
                        if (theResult == kErrorOK)
                            theResult = inFile.Read(this->mNumSplits);
                        if (theResult == kErrorOK)
                            theResult = inFile.Read(this->mState);
                        if (theResult == kErrorOK)
                            theResult = inFile.Read(this->mKind);
                        if (theResult == kErrorOK)
                            theResult = inFile.Read(this->mB1E0);
                        if (theResult == kErrorOK)
                            theResult = inFile.Read(this->mB1E1);
                        if (theResult == kErrorOK)
                            theResult = inFile.Read(this->mB1E2);
                        if (theResult == kErrorOK)
                            theResult = inFile.Read(this->mIsOutOfFight);
                        if (theResult == kErrorOK)
                            theResult = inFile.Read(this->mMission);
                        if (theResult == kErrorOK)
                            theResult = inFile.Read(this->mSplitKind);
                    }
                }
            }
        }
    }
    return theResult;
}
// FUNCTION: EXE 0x45ef56
// Moves the formation to the next waypoint (the task location if unset, the rally point if off the map, then the
// closest valid tile)
void EECPAttackSubGroup::MoveToDestination()
{
    EECPILocation theLocation;
    GE3DPoint thePoint;
    long theX;
    long theY;
    if (this->mNextWaypoint.mTile.GetX() == 0 && this->mNextWaypoint.mTile.GetY() == 0)
        this->mNextWaypoint = this->mTask.mLocation;
    if (!gWorld.GetMap()->GetTileAtLocation(this->mNextWaypoint.mTile))
        this->mNextWaypoint.mTile = this->mRallyPoint.mTile;
    thePoint.mX = (float)this->mNextWaypoint.mTile.GetX();
    thePoint.mY = (float)this->mNextWaypoint.mTile.GetY();
    if (!gWorld.GetMap()->GetTile(this->mNextWaypoint.mTile.GetX(), this->mNextWaypoint.mTile.GetY())) {
        gWorld.GetMap()->GetClosestTile(&thePoint, &theX, &theY);
        if (gWorld.GetMap()->GetTile(theX, theY)) {
            thePoint.mX = (float)theX;
            thePoint.mY = (float)theY;
        }
    }
    theLocation.mTile.mX = (long)thePoint.GetX();
    theLocation.mTile.mY = (long)thePoint.GetY();
    this->PlaceFormation(theLocation);
}
// FUNCTION: EXE 0x45f06b
// Drops every target no unit can attack (back to the pool); returns true when any target has an attacker
bool EECPAttackSubGroup::AssignTargets()
{
    bool theResult = false;
    for (long i = this->mTargets.size() - 1; i >= 0; i--) {
        EECPASTargetUnit* theTarget = *(this->mTargets.begin() + i);
        theTarget->mNumAttackers = 0;
        for (long j = this->mUnits.size() - 1; j >= 0; j--) {
            if (!(*(this->mUnits.begin() + j))->CanAttack(theTarget))
                continue;
            theTarget->mNumAttackers++;
            theResult = true;
            break;
        }
        if (theTarget->mNumAttackers == 0) {
            this->mTargets.erase(this->mTargets.begin() + i);
            EECPAttackSubGroup::ReleaseTarget(theTarget);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x45f14a
// Sets the next waypoint and moves the formation there
void EECPAttackSubGroup::SetDestination(const EECPILocation& inLocation)
{
    this->mNextWaypoint = inLocation;
    this->MoveToDestination();
}

// FUNCTION: EXE 0x45f16a
// Sends one unit to the location's tile (its area's when the tile is off the map); does nothing without a unit
void EECPAttackSubGroup::SendUnitTo(EECPAttackSubGroupUnit* inUnit, const EECPILocation& inLocation, bool inFlag)
{
    GE2DPoint thePoint;
    EECPILocation theLocation(inLocation);
    if (!theLocation.IsOnMap() && !gWorld.GetMap()->GetTileAtLocation(theLocation.mTile))
        theLocation.ResetToArea();
    if (!inUnit || !gWorld.GetMap()->GetTileAtLocation(theLocation.mTile))
        return;
    thePoint.SetX((float)theLocation.mTile.GetX());
    thePoint.SetY((float)theLocation.mTile.GetY());
    EEUGLocation* theGoal = new EEUGLocation(thePoint, true, inFlag, false, 0.0f, false, false);
    if (!this->GiveGoal(inUnit->GetUnit(), theGoal, inFlag, kActionPrepareToMove) && theGoal)
        delete theGoal;
}

// FUNCTION: EXE 0x45f233
// Updates the task location from its area when it has one
void EECPAttackSubGroup::RefreshTaskLocation()
{
    EECPILocation theLocation;
    if (!this->mTask.mLocation.mSerial)
        return;
    theLocation = this->mTask.mLocation;
    theLocation.Update();
    this->mTask.mLocation = theLocation;
}

// FUNCTION: EXE 0x45f272
// Turns the finished path into sub-sector waypoints (skipping the start on the main continent unless it is a wall),
// prunes them, aims at the first one (or the task) and drops the request
void EECPAttackSubGroup::ApplyPathResult()
{
    std::vector<EECPASGPathPoint> thePath;
    EECPASGPathPoint thePoint;
    long theStart = 0;
    if (this->mPathRequest->mPath.size() > 2) {
        EETileCoordinate& theFirst = this->mPathRequest->mPath[0];
        EEAIPathSubSector* theSector = gContinentManager.GetPathSubSectorAt(theFirst.GetX(), theFirst.GetY());
        unsigned short theID = theSector->mContinentID;
        if (theID == this->GetMainContinent() && theSector->mKind != kAITileWall)
            theStart = 1;
    }
    for (long i = theStart; i < (long)this->mPathRequest->mPath.size(); i++) {
        EETileCoordinate& theTile = this->mPathRequest->mPath[i];
        EEAIPathSubSector* theSector = gContinentManager.GetPathSubSectorAt(theTile.GetX(), theTile.GetY());
        thePoint.mTile = theSector->mLocation;
        thePoint.mSectorKind = theSector->mKind;
        thePath.push_back(thePoint);
    }
    this->SetPath(thePath);
    this->PrunePath();
    if (this->mPath.size() > 0)
        this->mNextWaypoint.mTile = this->mPath.front().mTile;
    else
        this->mNextWaypoint = this->mTask.mLocation;
    this->CancelPath();
}

// FUNCTION: EXE 0x45f3ca
// Regroups when the rally point is off the map (not while loading), then re-ranks the units and recomputes area
// effect, move mask, prejudices and common move flags
void EECPAttackSubGroup::RefreshUnits()
{
    if (!gWorld.GetMap()->GetTileAtLocation(this->mRallyPoint.mTile) && !gLoadingSavedGame)
        this->RegroupHere();
    this->RankUnits();
    this->UpdateAreaEffect();
    this->BuildMoveMask();
    this->AssignPrejudices();
    this->UpdateCommonMoveFlags();
}

// FUNCTION: EXE 0x45f423
// Removes the unit with the given ID (AI state reset, two unit fields cleared, target released) and refreshes;
// returns true when found
bool EECPAttackSubGroup::RemoveUnitByID(long inUnitID)
{
    bool theResult = false;
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        if ((this->mUnits[i])->GetUnit()->GetUnitID() != (unsigned long)inUnitID)
            continue;
        EECPAttackSubGroupUnit* theUnit = this->mUnits[i];
        theUnit->GetUnit()->SetAIState(300000);
        if (theUnit->GetUnit()->mL334 != 0)
            theUnit->GetUnit()->mL334 = 0;
        if (theUnit->GetUnit()->mL338 != 0)
            theUnit->GetUnit()->mL338 = 0;
        this->DropTargetOf(theUnit);
        theUnit->ReleaseTarget();
        this->mUnits.erase(this->mUnits.begin() + i);
        theUnit->Dereference();
        theResult = true;
        if (this->mUnits.size() > 0)
            this->RefreshUnits();
        break;
    }
    return theResult;
}
// FUNCTION: EXE 0x45f536
// Removes each listed unit (AI state reset, target released) and refreshes once if any was removed
void EECPAttackSubGroup::RemoveUnits(std::vector<EEComplexUnit*>& inUnits)
{
    bool theFound = false;
    for (long i = inUnits.size() - 1; i >= 0; i--) {
        EEComplexUnit* theOne = inUnits[i];
        std::vector<EECPAttackSubGroupUnit*>::iterator theIt = this->mUnits.begin();
        while (true) {
            if (theIt == this->mUnits.end())
                break;
            if ((*theIt)->GetUnit() == theOne) {
                EECPAttackSubGroupUnit* theUnit = *theIt;
                theUnit->GetUnit()->SetAIState(300000);
                if (theUnit->GetUnit()->mL334 != 0)
                    theUnit->GetUnit()->mL334 = 0;
                if (theUnit->GetUnit()->mL338 != 0)
                    theUnit->GetUnit()->mL338 = 0;
                this->DropTargetOf(theUnit);
                this->mUnits.erase(theIt);
                theUnit->Dereference();
                theFound = true;
                break;
            }
            theIt++;
        }
    }
    if (theFound && this->mUnits.size() > 0)
        this->RefreshUnits();
}
// FUNCTION: EXE 0x45f622
// Restores the units after loading (dropping non-military ones; a failing unit is retried forever), the waypoints,
// task and personnel request; sets a 10 s gather deadline when missing
long EECPAttackSubGroup::Restore(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    unsigned long i = 0;
    long theResult = 0;
    while (i < this->mUnits.size()) {
        theResult = (this->mUnits[i])->Restore(inMajorVersion, inMinorVersion);
        if (theResult == 0) {
            if ((this->mUnits[i])->mUnit && EECPCombatManager::IsMilitaryTypeUnit((this->mUnits[i])->GetUnit())) {
                EE_LOST_DEBUG(5);   // LOST-DEBUG: none: exact only (the original refuses begin() at the IsMilitaryTypeUnit site)
                (this->mUnits[i])->GetUnit()->SetAIState(this->mState);
                (this->mUnits[i])->Virtual48();
                i++;
            } else {
                (this->mUnits[i])->Dereference();
                this->mUnits.erase(this->mUnits.begin() + i);
            }
        }
    }
    if (theResult == 0) {
        theResult = this->mNextWaypoint.Restore(inMajorVersion, inMinorVersion);
        if (theResult == 0) {
            theResult = this->mRallyPoint.Restore(inMajorVersion, inMinorVersion);
            if (theResult == 0)
                theResult = this->mTask.Restore(inMajorVersion, inMinorVersion);
        }
    }
    this->mRequest = reinterpret_cast<long>(this->mRequest) != -1 ? this->mGroup->mCommander->GetPersonnel()->GetRequest(reinterpret_cast<long>(this->mRequest)) : 0;
    if (this->mRequest)
        this->mRequest->Reference();
    if (this->mUnits.size() > 0)
        this->RefreshUnits();
    if (this->mGatherDeadline == 0 && this->mState == kASGStateGather && gPhysics)
        this->mGatherDeadline = gPhysics->mGameTime + 10000;
    return theResult;
}

// FUNCTION: EXE 0x45f82b
// Returns the personnel request with the given ID, or NULL
// EECPPersonnel's member, out of line right after its first user (perhaps inline in the real header)
EECPPersonnelRequest* EECPPersonnel::GetRequest(long inID)
{
    std::map<long, EECPPersonnelRequest*>::iterator theIt = this->mRequests.find(inID);
    return theIt != this->mRequests.end() ? (*theIt).second : 0;
}

// FUNCTION: EXE 0x45f855
// With an active task, sets the +0x1bc timer 30-90 s ahead and heads for the task location
void EECPAttackSubGroup::Virtual18C(long inTime)
{
    if (this->mTask.mIsActive) {
        this->mU1BC = gPhysics->GetRandom(30000L, 90000L) + inTime;
        this->SetDestination(this->mTask.mLocation);
    }
}

// FUNCTION: EXE 0x45f895
// Writes the units (kind and data), path, area, waypoint, rally point, task, request ID (-1 if none), timers, state
// and flags; returns the first write error
long EECPAttackSubGroup::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    unsigned long i = 0;
    long theKind = 0;
    long theRequestID = 0;
    unsigned long theCount = this->mUnits.size();
    long theResult = outFile.Write(theCount);
    if (theResult == 0) {
        for (i = 0; i < theCount; i++) {
            if (theResult != 0)
                continue;
            theKind = this->mUnits[i]->mType;
            theResult = outFile.Write(theKind);
            if (theResult == 0)
                theResult = (*(this->mUnits.begin() + i))->Save(inMajorVersion, inMinorVersion, outFile);
        }
    }
    theCount = this->mPath.size();
    // a unit write error is kept (the path count's result used to replace it)
    if (theResult == 0)
        theResult = outFile.Write(theCount);
    if (theResult == 0) {
        for (i = 0; i < theCount; i++) {
            if (theResult != 0)
                continue;
            theResult = this->mPath[i].mTile.Save(outFile);
            if (theResult == 0)
                theResult = outFile.Write(this->mPath[i].mSectorKind);
        }
        if (theResult == 0) {
            theResult = this->mArea.Save(outFile);
            if (theResult == 0) {
                theResult = this->mNextWaypoint.Save(inMajorVersion, inMinorVersion, outFile);
                if (theResult == 0) {
                    theResult = this->mRallyPoint.Save(inMajorVersion, inMinorVersion, outFile);
                    if (theResult == 0)
                        theResult = this->mTask.Save(inMajorVersion, inMinorVersion, outFile);
                }
            }
        }
    }
    long theID = this->mRequest ? this->mRequest->mID : -1;
    theRequestID = theID;
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(theRequestID);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mScanRange);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mAggression);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mID);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mNextScanTime);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mNextUpdateTime);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mU1B0);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mU1B4);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mNextFollowTime);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mU1BC);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mNextTaskCheckTime);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mNumSplits);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mState);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mKind);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mB1E0);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mB1E1);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mB1E2);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mIsOutOfFight);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mMission);
    if (theResult == 0)
        theResult = outFile.Write(this->mSplitKind);
    return theResult;
}
// FUNCTION: EXE 0x45fbcf
// Sends every active unit to the rally point (the first town's centre when unset) and makes it the task location
void EECPAttackSubGroup::ReturnToRallyPoint()
{
    GE3DPoint thePoint;
    long i = this->mRallyPoint.mTile.GetX();
    if (i == 0 && this->mRallyPoint.mTile.mY == 0) {
        if (this->mUnits.size() <= 0 || this->mPlayer->GetNumTowns() <= 0)
            return;
        this->mRallyPoint.mTile = this->mPlayer->mTowns.front()->mHull.GetCenter();
        thePoint.mX = this->mPlayer->mTowns.front()->mHull.GetCenter().GetX() + 0.5f;
        thePoint.mY = this->mPlayer->mTowns[0]->mHull.GetCenter().mY + 0.5f;
    } else {
        thePoint.mX = i + 0.5f;
        thePoint.mY = this->mRallyPoint.mTile.mY + 0.5f;
    }
    for (i = this->mUnits.size() - 1; i >= 0; i--) {
        if (this->mUnits[i]->IsDone())
            continue;
        EEComplexUnit* theUnit = this->mUnits[i]->mUnit;
        EEUGLocation* theGoal = new EEUGLocation(&thePoint, true, true, false, 0.0f, false, false);
        if (!this->GiveGoal(theUnit, theGoal, 0, kActionPrepareToMove) && theGoal)
            delete theGoal;
    }
    this->mTask.mLocation = this->mRallyPoint;
}

// FUNCTION: EXE 0x45fd38
// Replaces the path with a copy of the given one
void EECPAttackSubGroup::SetPath(const std::vector<EECPASGPathPoint>& inPath)
{
    this->mPath.clear();
    for (unsigned long i = 0; i < inPath.size(); i++)
        this->mPath.push_back(inPath[i]);
}

// FUNCTION: EXE 0x45fd81
// Lists the terrain kinds every unit can cross: forest (4) and cliff (3)
void EECPAttackSubGroup::UpdateCommonMoveFlags()
{
    EECPASGFlags theFlags;
    long theCount3 = 0;
    long theCount4 = 0;
    this->mCommonMoveFlags.clear();
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        memset(theFlags.mFlags, 0, sizeof(theFlags.mFlags));
        theFlags.mPlayerID = 0;
        this->mUnits[i]->GetUnit()->GetUnitClass()->GetTerrainFlags(this->mUnits[i]->GetUnit(), theFlags);
        EE_LOST_DEBUG(1);   // LOST-DEBUG: none: exact only (the original calls both operator+ out of line)
        if (theFlags.mFlags[kAITileCliff])
            theCount3++;
        if (theFlags.mFlags[kAITileForest])
            theCount4++;
    }
    if (theCount4 == this->mUnits.size())
        this->mCommonMoveFlags.push_back(4);
    if (theCount3 == this->mUnits.size())
        this->mCommonMoveFlags.push_back(3);
}
// FUNCTION: EXE 0x45fe93
// For an active territory task with no path pending, returns true when within 8 tiles (or approaching), the tile is
// in the task player's influence and on the group's main continent
bool EECPAttackSubGroup::HasReachedTerritory(long inTime)
{
    EECPILocation theLocation;
    bool theResult = false;
    if (this->mTask.mType == kAGTaskTerritory && this->mTask.mIsActive) {
        bool theBusy = (this->mPathRequest && !this->mPathRequest->mIsDone) || this->mPath.size() != 0;
        if (theBusy)
            theResult = false;
        else {
            this->GetLocation(theLocation);
            theResult = this->mTask.mLocation.mTile.DistanceToSquared(theLocation.mTile) <= 0x40;
            if (!theResult && this->mState == kASGStateApproachTask)
                theResult = true;
            if (theResult) {
                EEPlayer* thePlayer = gWorld.mPlayers[this->mTask.GetPlayerID()];
                const EETileLocation& theTile = this->mTask.mLocation.GetTile();
                theResult = gWorld.mInfluenceTable->GetTileValue(thePlayer->GetPlayerID(), theTile) != 0;
                if (theResult) {
                    unsigned short theID = gContinentManager.GetContinentID(theTile.GetX(), theTile.GetY());
                    theResult = this->GetMainContinent() == theID;
                }
            }
        }
    }
    return theResult;
}
// FUNCTION: EXE 0x45ff6c
// Returns true when the group has targets but is more than 6 tiles outside the own and allied towns; only below hard,
// before the attack started, not in deathmatch and not on a wonder or attack-location task
bool EECPAttackSubGroup::IsAwayFromTowns()
{
    EETileLocation theTile;
    EETileLocation theTownTile;
    bool theResult = false;
    if (!this->mPlayer->mAttackStarted && !this->mTargets.empty() && this->mPlayer->mDifficulty != kDifficultyHard && !this->mPlayer->IsDeathmatch() && !this->mPlayer->mIgnoreOthers
        && (!this->mTask.mIsActive || (this->mTask.mMode != kAGModeWonder && this->mTask.mType != kAGTaskDefendWonder && this->mTask.mType != kAGTaskAttackLocation && this->mTask.mType != kAGTaskAttackWonder))) {
        this->FindPosition(theTile, true, false);
        EETown* theTown = this->mPlayer->GetNearestTown(theTile, theTownTile);
        if (theTown == NULL || (!theTown->Contains(theTile) && theTile.DistanceToSquared(theTownTile) > 0x24)) {
            theResult = true;
            for (long i = this->mPlayer->mAllies.size() - 1; i >= 0 && theResult; i--) {
                if (this->mPlayer->mAllies[i] == NULL || *(this->mPlayer->mAllies.begin() + i) == this->mPlayer)
                    continue;
                theTownTile.mX = 0;
                theTownTile.mY = 0;
                theTown = this->mPlayer->mAllies[i]->GetNearestTown(theTile, theTownTile);
                if (!theTown)
                    continue;
                if (theTown->Contains(theTile))
                    theResult = false;
                else
                    theResult = theTile.DistanceToSquared(theTownTile) > 0x24;
            }
        }
    }
    return theResult;
}
// FUNCTION: EXE 0x460140
// For an active location task with no fight or path pending, returns true when within 8 tiles (or approaching) and
// not already holding at the task
bool EECPAttackSubGroup::HasReachedLocation(long inTime)
{
    EECPILocation theLocation;
    bool theResult = false;
    if ((this->mTask.mType == kAGTaskLocation || this->mTask.mType == kAGTaskDefendWonder || this->mTask.mType == kAGTaskAttackLocation) && this->mTask.mIsActive) {
        bool theBusy = this->mState == kASGStateAttack || (this->mPathRequest && this->IsPathPending()) || this->mPath.size() != 0;
        if (theBusy)
            theResult = false;
        else {
            this->GetLocation(theLocation);
            theResult = this->mTask.mLocation.mTile.DistanceToSquared(theLocation.mTile) <= 0x40;
            if (!theResult && this->mState == kASGStateApproachTask)
                theResult = true;
            if (theResult)
                theResult = this->mState != kASGStateHoldAtTask;
        }
    }
    return theResult;
}
// FUNCTION: EXE 0x4601e3
// For an active unit task with no path pending, returns true when the task unit is 8 tiles or more from the task
// location
bool EECPAttackSubGroup::HasTaskUnitMoved(long inTime)
{
    EECPILocation theLocation;
    bool theResult = false;
    if ((this->mTask.mType == kAGTaskUnit || this->mTask.mType == kAGTaskAttackWonder) && this->mTask.mIsActive) {
        theResult = !this->IsPathPending() && this->mPath.size() == 0;
        if (theResult) {
            theLocation.SetUnit(this->mTask.mUnit);
            if (theLocation.mTile.GetX() != 0 || theLocation.mTile.GetY() != 0)
                theResult = this->mTask.mLocation.mTile.DistanceToSquared(theLocation.mTile) >= 0x40;
        }
    }
    return theResult;
}
// FUNCTION: EXE 0x46025e
// With an active task, returns true when the task lies in another path sub-sector than the group or is connected to
// the group's location
bool EECPAttackSubGroup::Virtual120()
{
    EECPILocation theLocation;
    bool theResult = false;
    if (this->mTask.mIsActive) {
        this->GetLocation(theLocation);
        EECPILocation& theTarget = this->mTask.mLocation;
        EEAIPathSubSector* theSector = theLocation.GetPathSubSector();
        if (theSector != theTarget.GetPathSubSector() ||
            theTarget.IsConnected(theLocation))
            theResult = true;
    }
    return theResult;
}
// FUNCTION: EXE 0x4602b9
// Sends the group back to the task location unless next to it, re-checking the task in 10-120 s
void EECPAttackSubGroup::HoldAtTask(unsigned long inTime)
{
    EECPILocation theLocation;
    this->GetLocation(theLocation);
    if (!theLocation.IsAdjacent(this->mTask.mLocation)) {
        this->SetDestination(this->mTask.mLocation);
        this->mNextTaskCheckTime = gPhysics->GetRandom(10000L, 120000L) + inTime;
    }
}
// FUNCTION: EXE 0x460318
// Enters a state and runs its entry action (remove, gather, ask for a task, split, retreat, move, follow, hold, return,
// reposition), then sets every unit's AI state to it; an unknown state resets the units to 300000
void EECPAttackSubGroup::SetState(long inState, long inTime)
{
    this->mState = inState;
    switch (this->mState) {
    case kASGStateRemove:
        this->RequestRemoval();
        break;
    case kASGStateGather:
        this->TakeRequestedUnits();
        this->mGatherDeadline = inTime + 30000;
        break;
    case kASGStateIdle:
        if (this->mUnits.size() > 0 && !this->mTask.mIsActive)
            this->mGroup->AssignTask(this);
        break;
    case kASGStateSplit:
        this->RequestSplit(true);
        break;
    case kASGStateRetreatToTown:
        this->MoveToNearestTown();
        this->ClearTargets();
        break;
    case kASGStateMove:
        this->MoveToDestination();
        break;
    case 1022:
    case kASGStateHoldTerritory:
        this->MoveToTask(false);
        break;
    case kASGStateFollowTaskUnit:
        this->FollowTaskUnit(inTime);
        break;
    case kASGStateHoldAtTask:
        this->HoldAtTask(inTime);
        break;
    case kASGStateReturnToRallyPoint:
        this->ReturnToRallyPoint();
        break;
    case kASGStateWaitForUnits:
    case kASGStateAttack:
    case 4000:
    case kASGStateLaunch:
    case 5000:
    case 6001:
    case 10001:
    case 10002:
    case 10003:
        break;
    default:
        this->SetUnitsState(300000);
        return;
    case kASGStateRepositionAtTask:
        this->RandomizeTaskLocation();
        this->SetDestination(this->mTask.mLocation);
        break;
    }
    this->SetUnitsState(inState);
}
// FUNCTION: EXE 0x4604cf
// Evaluates a state-machine condition (request filled, gathered, task valid, at rally point, targets, units, split,
// away from towns, night, unseen, at task, ...); unknown conditions are false
bool EECPAttackSubGroup::TestCondition(long inState, long inTime)
{
    bool theResult = false;
    switch ((unsigned long)inState) {
    case kASGConditionAlways:
        theResult = true;
        break;
    case kASGConditionRequestFilled:
        theResult = this->IsRequestFilled();
        break;
    case kASGConditionGathered:
        theResult = this->HasGathered();
        break;
    case kASGConditionTaskValid:
        theResult = this->CheckTask(inTime);
        break;
    case kASGConditionAtRallyPoint:
        theResult = this->mUnits.empty() || this->IsAt(this->mRallyPoint);
        break;
    case kASGConditionHasTargets:
        theResult = this->GetNumTargets() > 0;
        break;
    case 1013:
    case kASGConditionHasUnits:
        theResult = this->mUnits.size() != 0;
        break;
    case kASGConditionTaskInOtherSubSector:
        theResult = this->Virtual120();
        break;
    case kASGConditionShouldSplit:
        theResult = this->mUnits.size() > 0 && this->DecideSplit();
        break;
    case kASGConditionRequestEmpty:
        theResult = this->mRequest != NULL && this->mRequest->IsEmpty();
        break;
    case kASGConditionAwayFromTowns:
        theResult = this->IsAwayFromTowns();
        break;
    case kASGConditionHasUnitTask:
        theResult = this->mTask.mIsActive && this->mTask.mType == kAGTaskUnit && this->mTask.mUnit != NULL;
        break;
    case 3004:
        theResult = false;
        break;
    case kASGConditionHasLocationTask:
        if (this->mTask.mIsActive) {
            if (this->mTask.mType == kAGTaskLocation || (this->mTask.mType > 9 && this->mTask.mType <= kAGTaskDefendWonder))
                theResult = true;
        }
        break;
    case kASGConditionShouldReturn:
        theResult = this->mUnits.empty() || (this->mTask.mIsActive && this->mTask.mType == kAGTaskReturn);
        break;
    case kASGConditionShouldRefreshTask:
        theResult = this->ShouldRefreshTask();
        break;
    case kASGConditionTerritoryTaskInvalid:
        theResult = !this->IsTerritoryTaskValid(inTime);
        break;
    case kASGConditionNight:
        theResult = gWorld.mTimeOfDay >= 0.5f;
        break;
    case kASGConditionUnseenByTaskPlayer:
        theResult = this->IsUnseenByTaskPlayer();
        break;
    case kASGConditionAtTask:
        theResult = this->IsAt(this->mTask.mLocation);
        break;
    }
    return theResult;
}
// FUNCTION: EXE 0x46072e
// Counts, for every target, the other targets within squared distance 1.8
void EECPAttackSubGroup::CountTargetNeighbors()
{
    if (this->mTargets.size() <= 0)
        return;
    for (unsigned long i = 0; i < this->mTargets.size() - 1; i++) {
        for (unsigned long j = i + 1; j < this->mTargets.size(); j++) {
            if (this->mTargets[i]->mUnit->DistanceSquaredTo(this->mTargets[j]->mUnit) <= kASGNear) {
                this->mTargets[i]->mNumNearbyTargets++;
                this->mTargets[j]->mNumNearbyTargets++;
            }
        }
    }
}

// FUNCTION: EXE 0x4607d0
// Schedules the next update 2 s ahead
void EECPAttackSubGroup::Update(long inTime)
{
    this->mNextUpdateTime = inTime + 2000;
}

// FUNCTION: EXE 0x4607e2
// Returns whether the given task is active
bool EECPAttackSubGroup::IsTaskActive(const EECPAGTask& inTask)
{
    return inTask.mIsActive;
}

// FUNCTION: EXE 0x4607ec
// Returns every unit to personnel (releasing its target) and empties the sub-group
void EECPAttackSubGroup::ReleaseUnits()
{
    EECPPersonnel* thePersonnel = this->mGroup->mCommander->mPersonnel;
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        if ((*(this->mUnits.begin() + i))->mUnit) {
            this->DropTargetOf(this->mUnits[i]);
            this->mUnits[i]->ReleaseTarget();
            thePersonnel->ReturnUnit(this->mUnits[i]->mUnit);
        }
        (*(this->mUnits.begin() + i))->Dereference();
    }
    this->mUnits.clear();
}
// FUNCTION: EXE 0x46087f
// Moves the task location to the task unit (or its carrier) and heads there; follows again in 15-70 s
void EECPAttackSubGroup::FollowTaskUnit(long inTime)
{
    EECPILocation theLocation;
    this->mNextFollowTime = gPhysics->GetRandom(15000L, 70000L) + inTime;
    theLocation = this->mTask.mLocation;
    if (this->mTask.mUnit) {
        if (this->mTask.mUnit->mContainer == NULL) {
            if (this->mTask.mUnit->GetTileX() != -10000) {
                theLocation.mTile.mX = this->mTask.mUnit->GetTileX();
                theLocation.mTile.mY = this->mTask.mUnit->mTileY;
            } else {
                theLocation.mTile = EETileLocation(UMFloor(this->mTask.mUnit->mLastMapPosition.GetX()), UMFloor(this->mTask.mUnit->mLastMapPosition.GetY()));
            }
        } else if (this->mTask.mUnit->mContainer->GetTileX() != -10000) {
            theLocation.mTile.mX = this->mTask.mUnit->mContainer->GetTileX();
            theLocation.mTile.mY = this->mTask.mUnit->mContainer->mTileY;
        } else {
            theLocation.mTile.SetXY(UMFloor(this->mTask.mUnit->mContainer->mLastMapPosition.GetX()), UMFloor(this->mTask.mUnit->mContainer->mLastMapPosition.GetY()));
        }
        this->mTask.mLocation = theLocation;
    }
    if (gWorld.GetMap()->GetTileAtLocation(this->mTask.mLocation.mTile))
        this->SetDestination(this->mTask.mLocation);
}

// FUNCTION: EXE 0x4609dd
// Points the record of a unit at a new unit; returns true when found
bool EECPAttackSubGroup::ReplaceUnit(EEComplexUnit* inUnit, EEComplexUnit* inNewUnit)
{
    bool theResult = false;
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        if (this->mUnits[i]->GetUnit() != inUnit)
            continue;
        (*(this->mUnits.begin() + i))->SetUnit(inNewUnit);
        theResult = true;
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x460a27
// Checks arrival for the task type and enters the matching state (hold at task, hold territory, follow unit, return);
// returns true on arrival
bool EECPAttackSubGroup::CheckTaskArrival(long inTime)
{
    bool theResult = false;
    switch (this->mTask.mType) {
    case kAGTaskLocation:
    case kAGTaskAttackLocation:
    case kAGTaskDefendWonder:
        theResult = this->HasReachedLocation(inTime);
        if (theResult)
            this->SetState(kASGStateHoldAtTask, inTime);
        break;
    case kAGTaskTerritory:
        theResult = this->HasReachedTerritory(inTime);
        if (theResult)
            this->SetState(kASGStateHoldTerritory, inTime);
        break;
    case 3:
        theResult = this->Virtual1B4(inTime);
        if (theResult)
            this->SetState(6000, inTime);
        break;
    case 4:
        theResult = this->Virtual1AC(inTime);
        if (theResult)
            this->SetState(6003, inTime);
        break;
    case 5:
        theResult = this->Virtual1A0(inTime);
        if (theResult)
            this->SetState(6005, inTime);
        break;
    case kAGTaskUnit:
    case kAGTaskAttackWonder:
        theResult = this->HasTaskUnitMoved(inTime);
        if (theResult)
            this->SetState(kASGStateFollowTaskUnit, inTime);
        break;
    case kAGTaskReturn:
        theResult = this->mTask.mIsActive;
        if (theResult)
            this->SetState(kASGStateReturnToRallyPoint, inTime);
        break;
    }
    return theResult;
}
// FUNCTION: EXE 0x460b67
// Removes the unit by its ID and refreshes the units a second time; returns true when found
bool EECPAttackSubGroup::RemoveUnit(EEComplexUnit* inUnit)
{
    bool theResult = false;
    if (inUnit) {
        theResult = this->RemoveUnitByID(inUnit->GetUnitID());
        if (theResult && this->mUnits.size() > 0)
            this->RefreshUnits();
    }
    return theResult;
}

// FUNCTION: EXE 0x460ba9
// Drops targets and siege targets that are no longer living enemies on the map, or that the player cannot see
// (cloaked targets, or any below hard difficulty)
void EECPAttackSubGroup::DropLostTargets(long inTime)
{
    long i;
    EEComplexUnit* theUnit;
    EEComputerPlayer* thePlayer;
    for (i = this->mTargets.size() - 1; i >= 0; i--) {
        theUnit = (*(this->mTargets.begin() + i))->mUnit;
        thePlayer = this->mPlayer;
        if (theUnit->GetPlayer() != thePlayer && thePlayer->mEnemies[theUnit->GetPlayer()->GetPlayerID()] != 0
            && theUnit->mHitPoints > 0 && theUnit->GetTileX() != -10000) {
            if ((theUnit->mIsCloakable ? theUnit->mIsCloaked : false) || thePlayer->mDifficulty != kDifficultyHard) {
                bool theLost = !EEIsSeenBy(theUnit, thePlayer, true);
                if (theLost) {
                    EECPASTargetUnit* theTarget = *(this->mTargets.begin() + i);
                    this->mTargets.erase(this->mTargets.begin() + i);
                    EECPAttackSubGroup::ReleaseTarget(theTarget);
                }
            }
        } else {
            EECPASTargetUnit* theTarget = *(this->mTargets.begin() + i);
            this->mTargets.erase(this->mTargets.begin() + i);
            EECPAttackSubGroup::ReleaseTarget(theTarget);
        }
    }
    for (i = this->mSiegeTargets.size() - 1; i >= 0; i--) {
        theUnit = (*(this->mSiegeTargets.begin() + i))->mUnit;
        thePlayer = this->mPlayer;
        if (theUnit->GetPlayer() != thePlayer && thePlayer->mEnemies[theUnit->GetPlayer()->GetPlayerID()] != 0
            && theUnit->mHitPoints > 0 && theUnit->GetTileX() != -10000) {
            if ((theUnit->mIsCloakable ? theUnit->mIsCloaked : false) || thePlayer->mDifficulty != kDifficultyHard) {
                bool theLost = !EEIsSeenBy(theUnit, thePlayer, true);
                if (theLost) {
                    EECPASTargetUnit* theTarget = this->mSiegeTargets[i];
                    this->mSiegeTargets.erase(this->mSiegeTargets.begin() + i);
                    EECPAttackSubGroup::ReleaseTarget(theTarget);
                }
            }
        } else {
            EECPASTargetUnit* theTarget = this->mSiegeTargets[i];
            this->mSiegeTargets.erase(this->mSiegeTargets.begin() + i);
            EECPAttackSubGroup::ReleaseTarget(theTarget);
        }
    }
}

// FUNCTION: EXE 0x460d85
// Updates every living unit not out of the fight whose update time has come
void EECPAttackSubGroup::UpdateUnits(long inTime)
{
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        EECPAttackSubGroupUnit* theUnit = (*(this->mUnits.begin() + i));
        if (theUnit->GetUnit()->mHitPoints > 0 && !theUnit->mIsOutOfFight && theUnit->mNextUpdateTime <= (unsigned long)inTime)
            theUnit->Update(inTime);
    }
}
// FUNCTION: EXE 0x460dd4
// Validates the active task by its type and deactivates it when invalid; returns whether it is still active
bool EECPAttackSubGroup::CheckTask(long inTime)
{
    bool theResult = this->mTask.mIsActive;
    if (theResult) {
        switch (this->mTask.mType) {
        case 5:
            theResult = this->Virtual1C8(inTime);
            break;
        case kAGTaskLocation:
        case kAGTaskAttackLocation:
        case kAGTaskDefendWonder:
            theResult = this->IsLocationTaskValid(inTime);
            break;
        case kAGTaskTerritory:
            theResult = this->IsTerritoryTaskValid(inTime);
            break;
        case 4:
            theResult = this->Virtual1D0(inTime);
            break;
        case kAGTaskUnit:
        case kAGTaskAttackWonder:
            theResult = this->IsUnitTaskValid(inTime);
            break;
        default:
            theResult = true;
            break;
        }
        this->mTask.mIsActive = theResult;
    }
    return theResult;
}
// FUNCTION: EXE 0x460e65
// Checks a territory task (task player's influence at or, with no buildings known, near the tile): false once the
// group is there without targets, else true unless every unit is counted in the stats' +0x24 tally
bool EECPAttackSubGroup::IsTerritoryTaskValid(long inTime)
{
    EEInfluenceTable* theTable = gWorld.mInfluenceTable;
    U2DRectangle<long> theBounds;
    EETileLocation theTile;
    long thePlayer = this->mTask.GetPlayerID();
    bool theResult = theTable->GetValue(thePlayer, theTable->GetRegionPoint(this->mTask.mLocation.mTile)) != 0;
    if (!theResult) {
        EECPIntelligencePlayerFile* theFile = this->GetTargetPlayerFile();
        if (theFile != NULL) {
            if (theFile->mNumBuildings == 0) {
                if (this->mTask.mLocation.mTile.GetX() != -10000) {
                    long theRegionSize = gWorld.mInfluenceTable->mRegionSize;
                    U2DPoint<long> theRegion(this->mTask.mLocation.mTile.GetX() / theRegionSize,
                                             this->mTask.mLocation.mTile.GetY() / theRegionSize);
                    theBounds.mLeft = theRegion.GetX();
                    theBounds.mTop = theRegion.GetY();
                    U2DPoint<long> theBottomRight(theBounds.mLeft, theBounds.mTop);
                    theBounds.mRight = theBottomRight.GetX();
                    theBounds.mBottom = theBottomRight.GetY();
                    U2DPoint<long> theTopLeft(theBounds.mLeft, theBounds.mTop);
                    theTile.mX = theTopLeft.GetX() - 2;
                    theBounds.mLeft = theTile.GetX();
                    theTile.mY = theTopLeft.GetY() - 2;
                    theBounds.mTop = theTile.GetY();
                    U2DPoint<long> theEnd(theBounds.mRight, theBounds.mBottom);
                    theTile.mX = theEnd.GetX() + 2;
                    theBounds.mRight = theTile.GetX();
                    theTile.mY = theEnd.GetY() + 2;
                    theBounds.mBottom = theTile.GetY();
                    theResult = gWorld.mInfluenceTable->SumValue(this->mTask.GetPlayerID(), theBounds) != 0;
                }
            } else {
                theResult = false;
            }
        }
    }
    if (theResult) {
        if (this->mTargets.size() == 0) {
            theTile = this->GetPosition();
            if (theTile.GetX() != -10000) {
                if (theTile.DistanceToSquared(this->mTask.mLocation.mTile) <= 0x40 ||
                    gContinentManager.GetPathSubSectorAt(theTile.GetX(), theTile.GetY()) ==
                        gContinentManager.GetPathSubSectorAt(this->mTask.mLocation.mTile.GetX(), this->mTask.mLocation.mTile.GetY()))
                    theResult = false;
            }
        }
        if (!theResult)
            return theResult;
    } else {
        if (!this->mTask.mLocation.IsOnMap())
            return theResult;
        if (this->mTargets.size() == 0) {
            theTile = this->GetPosition();
            if (theTile.GetX() != -10000) {
                if (theTile.DistanceToSquared(this->mTask.mLocation.mTile) <= 0x40 ||
                    gContinentManager.GetPathSubSectorAt(theTile.GetX(), theTile.GetY()) ==
                        gContinentManager.GetPathSubSectorAt(this->mTask.mLocation.mTile)) {
                    theResult = false;
                    return theResult;
                }
            }
        }
    }
    if (this->mUnits.size() != 0 && this->mUnits.size() == this->mStats.mL24)
        theResult = false;
    else
        theResult = true;
    return theResult;
}
// FUNCTION: EXE 0x46111a
// Checks a location task once the group is there and the re-check time passed: attack-location needs enemy influence
// nearby, defend-wonder a living wonder, defence tasks end 50% of the time; re-checks in 10-120 s
bool EECPAttackSubGroup::IsLocationTaskValid(long inTime)
{
    U2DRectangle<long> theBounds;
    bool theResult = true;
    if (this->mTask.mLocation.mSerial != 0)
        theResult = gContinentManager.GetArea(this->mTask.mLocation.mSerial) != NULL;
    if (theResult && this->IsAt(this->mTask.mLocation) && this->mNextTaskCheckTime < (unsigned long)inTime) {
        switch (this->mTask.mType) {
        case kAGTaskAttackLocation:
            theResult = false;
            if (gWorld.GetMap()->GetTileAtLocation(this->mTask.mLocation.mTile) != NULL) {
                long theRegionSize = gWorld.mInfluenceTable->mRegionSize;
                U2DPoint<long> theRegion(this->mTask.mLocation.mTile.GetX() / theRegionSize,
                                         this->mTask.mLocation.mTile.mY / theRegionSize);
                theBounds.mLeft = theRegion.GetX();
                theBounds.mTop = theRegion.mY;
                U2DPoint<long> theBottomRight(theBounds.mLeft, theBounds.mTop);
                theBounds.mRight = theBottomRight.GetX();
                theBounds.mBottom = theBottomRight.mY;
                U2DPoint<long> theTopLeft(theBounds.mLeft, theBounds.mTop);
                theBounds.mLeft = theTopLeft.GetX() - 2;
                theBounds.mTop = theTopLeft.mY - 2;
                U2DPoint<long> theEnd(theBounds.mRight, theBounds.mBottom);
                theBounds.mRight = theEnd.GetX() + 2;
                theBounds.mBottom = theEnd.mY + 2;
                theResult = gWorld.mInfluenceTable->SumEnemyValue(this->mPlayer->GetPlayerID(), theBounds, true) != 0;
                if (theResult)
                    this->mNextTaskCheckTime = gPhysics->GetRandom(10000L, 120000L) + inTime;
            }
            break;
        case kAGTaskDefendWonder:
            this->mNextTaskCheckTime = gPhysics->GetRandom(10000L, 120000L) + inTime;
            if (this->mTask.mUnit == NULL || this->mTask.mUnit->mHitPoints <= 0)
                theResult = false;
            else
                theResult = true;
            break;
        default:
            if (gPhysics->mRandom.Generate() > 0.5f && (this->mTask.mMode == kAGModeDefense || this->mTask.mMode == kAGModeNone)) {
                theResult = false;
            } else {
                theResult = true;
                this->mNextTaskCheckTime = gPhysics->GetRandom(10000L, 120000L) + inTime;
            }
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x46131a
// Sets the +0x1b4 timer 3-22 s ahead
void EECPAttackSubGroup::Virtual1D8(unsigned long inTime)
{
    this->mU1B4 = gPhysics->GetRandom(3000L, 22000L) + inTime;
}

// Template / inline COMDATs of the unit (named in names.txt; first-reference order).
// TEMPLATE: EXE 0x45afdc
// TEMPLATE: EXE 0x4613db
// TEMPLATE: EXE 0x461421
// TEMPLATE: EXE 0x46154d
// TEMPLATE: EXE 0x4617b6
// TEMPLATE: EXE 0x461a5c
// TEMPLATE: EXE 0x461e97
// TEMPLATE: EXE 0x461efa
// TEMPLATE: EXE 0x461f5d
// TEMPLATE: EXE 0x462052
// TEMPLATE: EXE 0x46240f
// TEMPLATE: EXE 0x462492
// TEMPLATE: EXE 0x4624f8
// TEMPLATE: EXE 0x461340
// TEMPLATE: EXE 0x461406
// TEMPLATE: EXE 0x4614c0
// TEMPLATE: EXE 0x4614eb
// TEMPLATE: EXE 0x4614ff
// TEMPLATE: EXE 0x461647
// TEMPLATE: EXE 0x4616b8
// TEMPLATE: EXE 0x461712
// TEMPLATE: EXE 0x461767
// TEMPLATE: EXE 0x461acd
// TEMPLATE: EXE 0x461aff
// TEMPLATE: EXE 0x46215f
// TEMPLATE: EXE 0x4621c6
// TEMPLATE: EXE 0x462340
// TEMPLATE: EXE 0x46256b
// TEMPLATE: EXE 0x462594
// TEMPLATE: EXE 0x462667
// TEMPLATE: EXE 0x4627c5
// TEMPLATE: EXE 0x462856
// TEMPLATE: EXE 0x462890
// TEMPLATE: EXE 0x4628d1
// TEMPLATE: EXE 0x4629cf

// Priority sort family emitted by ComputeLoadDistance; typed comparator identities audited from its call graph.
// TEMPLATE: EXE 0x462283
// TEMPLATE: EXE 0x462698
// TEMPLATE: EXE 0x4626c1
// TEMPLATE: EXE 0x462794
// TEMPLATE: EXE 0x4629f8
// TEMPLATE: EXE 0x462a92
// TEMPLATE: EXE 0x462acc
// TEMPLATE: EXE 0x462b0d
// TEMPLATE: EXE 0x462c22
// TEMPLATE: EXE 0x462cb9
// TEMPLATE: EXE 0x462cfd
// TEMPLATE: EXE 0x462dde
// TEMPLATE: EXE 0x462e46
// TEMPLATE: EXE 0x462f56

// Template and inline COMDATs this unit's code instantiates (the bodies come from the headers; each was compared
// against the original by name)
// TEMPLATE: EXE 0x461b2a
// TEMPLATE: EXE 0x462c4c
// TEMPLATE: EXE 0x462c90
// TEMPLATE: EXE 0x462f0c

// Compiled COMDATs (the deque<EECPASGPathPoint> helpers call the out-of-line operator=)
// TEMPLATE: EXE 0x461393
// TEMPLATE: EXE 0x461591
// TEMPLATE: EXE 0x461602
// TEMPLATE: EXE 0x461784
// TEMPLATE: EXE 0x4617eb
// TEMPLATE: EXE 0x461de8
// TEMPLATE: EXE 0x461dfb
// TEMPLATE: EXE 0x46237d
// TEMPLATE: EXE 0x46239a
// TEMPLATE: EXE 0x4623c6
// TEMPLATE: EXE 0x4623e3
// TEMPLATE: EXE 0x462d26
// TEMPLATE: EXE 0x462d8b
// TEMPLATE: EXE 0x462e99
// TEMPLATE: EXE 0x462ed8

// FUNCTION: EXE 0x45e407
// Returns the unit's top speed: its active group's value when it is in one, else its stats'
// Out of line after its first user here
float EEComplexUnit::GetMaxVelocity()
{
    if (this->mActiveGroup)
        return this->mActiveGroup->GetMaxVelocity(this);
    return this->mStats->mMaxVelocity;
}
// The targets pool (sTargetPool) COMDATs
// TEMPLATE: EXE 0x4613a1
// TEMPLATE: EXE 0x46152b

// ICF-FOLDED: EXE 0x557269
// (no address of its own: folded into the shared `ret 4` body the vtable slot points at)
void EECPAttackSubGroup::Virtual118(long)
{
}

// ICF-FOLDED: EXE 0x557269
// (no address of its own: folded into the shared `ret 4` body the vtable slot points at)
void EECPAttackSubGroup::Virtual138(long)
{
}

// ICF-FOLDED: EXE 0x4e123d
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EECPAttackSubGroup::RequestPathToTask()
{
}

// ICF-FOLDED: EXE 0x4e123d
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EECPAttackSubGroup::PrunePath()
{
}

// ICF-FOLDED: EXE 0x5720c6
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EECPAttackSubGroup::AssignPrejudices()
{
}

// ICF-FOLDED: EXE 0x4bb118
// (no address of its own: folded into the shared `xor al, al; ret 4` body the vtable slot points at)
bool EECPAttackSubGroup::Virtual1A0(long)
{
    return false;
}

// ICF-FOLDED: EXE 0x4bb118
// (no address of its own: folded into the shared `xor al, al; ret 4` body the vtable slot points at)
bool EECPAttackSubGroup::Virtual1AC(long)
{
    return false;
}

// ICF-FOLDED: EXE 0x4bb118
// (no address of its own: folded into the shared `xor al, al; ret 4` body the vtable slot points at)
bool EECPAttackSubGroup::Virtual1B4(long)
{
    return false;
}

// ICF-FOLDED: EXE 0x4b3257
// (no address of its own: folded into the shared `mov al, 1; ret 4` body the vtable slot points at)
bool EECPAttackSubGroup::Virtual1C8(long)
{
    return true;
}

// ICF-FOLDED: EXE 0x4bb118
// (no address of its own: folded into the shared `xor al, al; ret 4` body the vtable slot points at)
bool EECPAttackSubGroup::Virtual1D0(long)
{
    return false;
}

// ICF-FOLDED: EXE 0x67c0b4
// (no address of its own: folded into the shared `xor al, al; ret` body the vtable slot points at)
bool EECPAttackSubGroup::Virtual9C()
{
    return false;
}

// ICF-FOLDED: EXE 0x4ac7e2
// (no address of its own: folded into the shared `fld 0.0; ret` body the vtable slot points at)
float EECPAttackSubGroup::GetGuardChokepointPercentage()
{
    return 0.0f;
}

// ICF-FOLDED: EXE 0x4ac7e2
// (no address of its own: folded into the shared `fld 0.0; ret` body the vtable slot points at)
float EECPAttackSubGroup::GetGuardHighGroundPercentage()
{
    return 0.0f;
}

// ICF-FOLDED: EXE 0x4ac7e2
// (no address of its own: folded into the shared `fld 0.0; ret` body the vtable slot points at)
float EECPAttackSubGroup::GetGuardContinentPercentage()
{
    return 0.0f;
}

// ICF-FOLDED: EXE 0x4ac7e2
// (no address of its own: folded into the shared `fld 0.0; ret` body the vtable slot points at)
float EECPAttackSubGroup::GetGuardTownPercentage()
{
    return 0.0f;
}

// ICF-FOLDED: EXE 0x4ac9f3
// (no address of its own: folded into the shared `xor al, al; ret` body the vtable slot points at)
bool EECPAttackSubGroup::VirtualBC()
{
    return false;
}
