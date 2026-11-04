// C:\EEX\Empire Earth\AI Thread\EECPASGLand.cpp (file name inferred: no __FILE__). 0x45ee9..0x4dc37 (unit map
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
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEInfluenceTable.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIPathSubSector.h"
#include "ee/EECPAttackGroup.h"
#include "ee/EECPAttackCommander.h"
#include "ee/EEAutoPathRequest.h"
#include "ee/EECPASGTransport.h"
#include "ee/EECPASGLand.h"
#include "ee/EECPASGAirTransport.h"
#include "ee/EEUCTransport.h"
#include "ee/EEUGLoadTransport.h"
#include "ee/EEUGLocation.h"
#include "ee/EECPCombatManager.h"
#include "ee/EEPathManager.h"
#include "ee/EEAIPathGoalModifierBase.h"
#include "ee/EEAIBoundedPathGoalModifierBase.h"
#include "ee/EEAIPathGoalModifiers.h"
#include "ee/EECPAAirTargetFilters.h"
#include "ee/EECPAreaTargetCollector.h"

// Genuine cdecl factory at 0x4564f6, also declared in EECPAttackSubGroup.cpp.
EECPAttackSubGroupUnit* EECPCreateSubGroupUnit(EEComplexUnit* inUnit, EECPAttackSubGroup* inSubGroup, bool inGeneric);

// A transport and the units planned to board it, with its current load (24 bytes): built by the original at
// 0x447c96/0x4483b7, copied at 0x44d47f. The vector, transport pointer and load are each proven by the callers
// (provisional name).
struct EECPAILoadGroup {
    EECPAILoadGroup() : mTransport(NULL), mLoad(0) {}
    std::vector<EEComplexUnit*> mUnits;
    EEComplexUnit* mTransport;
    long mLoad;
    void ResetLoad();
};

// The land paths' tile order: the FARTHER tile first, so tiles.back() is the nearest (this unit's sort COMDATs
// 0x44d1a5..0x44dbe3 compare `cmp eax, esi; jg` (_Insertion_sort_1 0x44d6a8), `jle` (_Unguarded_insert 0x44d939,
// _Adjust_heap 0x44da86); the volcano's EETileDistanceLess twin 0x4b55fd has `jl`). A type of its own: its sort
// COMDATs are a second copy beside the volcano's (provisional name).
struct EECPASGLandTileFarther {
    EECPASGLandTileFarther(const EETileCoordinate& inCenter) : mCenter(inCenter) {}
    EECPASGLandTileFarther(const EECPASGLandTileFarther& inOther) : mCenter(inOther.mCenter) {}
    ~EECPASGLandTileFarther() {}
    long DistanceSquared(const EETileCoordinate& inTile) const;   // ICF-folded into 0x408a3f
    bool operator()(const EETileCoordinate& inFirst, const EETileCoordinate& inSecond) const
        { return this->DistanceSquared(inFirst) > this->DistanceSquared(inSecond); }

    EETileCoordinate mCenter;
};

// EECPASGLandTileFarther's out-of-line distance (the sort COMDATs at the end call it): the same body as
// EETileDistanceLess::DistanceSquared, which ICF keeps at 0x408a3f; no address of its own (placement inferred).
long EECPASGLandTileFarther::DistanceSquared(const EETileCoordinate& inTile) const
{
    long theDY = this->mCenter.GetY() - inTile.GetY();
    long theDX = this->mCenter.GetX() - inTile.GetX();
    return theDX * theDX + theDY * theDY;
}

#include "ee/EEAIRandom.h"
#include "ee/EECPIntelligence.h"
#include "ee/EEMapDimensions.h"
#include "LostDebug.h"   // synthetic, NOT original source (LOST-DEBUG: EE_LOST_DEBUG)

// FUNCTION: EXE 0x445ee9
// Creates a land attack sub-group for the given attack group, with no transport and no avoided tiles
EECPASGLand::EECPASGLand(EECPAttackGroup* inGroup, long inKind)
    : EECPAttackSubGroup(inGroup, inKind), mTransport(NULL), mTransportStage(0), mNextSubSectorCheckTime(0), mNoTransport(false)
{
}

// SYNTHETIC: EXE 0x445f80
// (the scalar deleting destructor)

// FUNCTION: EXE 0x445f9c
// Destroys the land sub-group (members only)
EECPASGLand::~EECPASGLand()
{
}

// FUNCTION: EXE 0x445fca
// Marks a tile to avoid for the next 30 s (an already listed tile gets a fresh expiry)
void EECPASGLand::AvoidTile(const EETileLocation& inTile)
{
    std::vector<EECPASGLandPathPoint>::iterator theIter;
    for (theIter = this->mAvoidedTiles.begin(); theIter != this->mAvoidedTiles.end(); theIter++) {
        if (theIter->mTile == inTile)
            break;
    }
    if (theIter != this->mAvoidedTiles.end()) {
        theIter->mTime = gPhysics->mGameTime + 30000;
    } else {
        this->mAvoidedTiles.push_back(EECPASGLandPathPoint());
        this->mAvoidedTiles.back().mTile = inTile;
        this->mAvoidedTiles.back().mTime = gPhysics->mGameTime + 30000;
    }
}

// FUNCTION: EXE 0x446065
// Drops the transport unless the sub-group is in transit, then sets the new task
void EECPASGLand::SetTask(const EECPAGTask& inTask)
{
    if (this->mState != kASGStateInTransit && this->mTransport != NULL) {
        this->DropTransport();
    }
    EECPAttackSubGroup::SetTask(inTask);
}

// FUNCTION: EXE 0x446094
// Orders a unit to attack only when it is not inside a transport or building
void EECPASGLand::AttackWith(EECPAttackSubGroupUnit* inUnit, EEComplexUnit* inTarget, EETileCoordinate* inTile)
{
    if (inUnit->mUnit->mContainer == NULL)
        EECPAttackSubGroup::AttackWith(inUnit, inTarget, inTile);
}

// FUNCTION: EXE 0x4460b5
// Releases the transport and requests a path from the group's position to the task (an off-map task tile is
// replaced by the tile nearest the origin); with no position, goes in transit if it has a transport, else no-path
void EECPASGLand::RequestPathToTask()
{
    EECPILocation theLocation(this->mTask.mLocation);
    EETileLocation theTile;
    theTile = this->GetPosition();
    if (theTile.GetX() != -10000) {
        this->ReleaseTransport();
        if (gWorld.GetMap()->GetTileAtLocation(theLocation.mTile) == NULL) {
            GE3DPoint thePoint(0.0f, 0.0f, 0.0f);
            long theX;
            long theY;
            gWorld.GetMap()->GetClosestTile(&thePoint, &theX, &theY);
            theLocation.mTile.mX = theX;
            theLocation.mTile.mY = theY;
        }
        this->RequestPath(&theTile, &theLocation.mTile);
    } else if (this->mTransport != NULL) {
        this->mState = kASGStateInTransit;
    } else {
        this->mState = kASGStateNoPath;
    }
}

// FUNCTION: EXE 0x446178
// Releases the transport
void EECPASGLand::DropTransport()
{
    this->ReleaseTransport();
}

// FUNCTION: EXE 0x44617d
// True when no next sub-sector tile is set or a unit can attack a target on it or in its sub-sector; otherwise,
// if that sub-sector is a wall, moves the tile to the wall tile nearest the group and returns whether it moved
bool EECPASGLand::HasTargetsAtNextSubSector()
{
    std::vector<EECPASTargetUnit*> theTargets;
    EETileLocation theCenter;
    bool theResult = true;
    if (this->mNextSubSectorTile.GetX() == 0 || this->mNextSubSectorTile.GetY() == 0)
        return theResult;
    theResult = !this->mTargets.empty();
    if (theResult) {
        for (long i = this->mTargets.size() - 1; i >= 0; --i) {
            EECPASTargetUnit* theTarget = this->mTargets[i];
            std::pair<long, long> theTile(theTarget->mUnit->mTileX, theTarget->mUnit->mTileY);
            if ((theTile.first == this->mNextSubSectorTile.GetX() && theTile.second == this->mNextSubSectorTile.GetY()) ||
                gContinentManager.GetPathSubSectorID(theTarget->mUnit) ==
                gContinentManager.GetPathSubSectorID(this->mNextSubSectorTile.GetX(), this->mNextSubSectorTile.GetY()))
                theTargets.push_back(theTarget);
        }
        theResult = false;
        if (!theTargets.empty()) {
            for (long j = this->mUnits.size() - 1; j >= 0 && !theResult; --j) {
                for (long k = theTargets.size() - 1; k >= 0; --k) {
                    if (this->mUnits[j]->CanAttack(theTargets[k]) &&
                        !this->mUnits[j]->GetUnit()->IsTargetIgnored(theTargets[k]->mUnit, true)) {
                        theResult = true;
                        break;
                    }
                }
            }
        }
        if (theResult)
            return theResult;
    }
    this->FindPosition(theCenter, true, true);
    EEAIPathSubSector* theSector = gContinentManager.GetPathSubSectorAt(this->mNextSubSectorTile.GetX(), this->mNextSubSectorTile.GetY());
    if (theSector != NULL && theSector->mKind == kAITileWall) {
        EETileLocation theClosest = theSector->mTiles.GetClosestPoint(theCenter);
        if (theClosest.GetX() != this->mNextSubSectorTile.GetX() || theClosest.GetY() != this->mNextSubSectorTile.GetY()) {
            this->mNextSubSectorTile = theClosest;
            theResult = true;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x446422
// Returns true when the transport would leave no unit behind
bool EECPASGLand::CanCarryAllUnits()
{
    std::vector<EEComplexUnit*> theUnits;
    this->GetUnitsLeftBehind(theUnits);
    return theUnits.size() == 0;
}

// FUNCTION: EXE 0x44646c
// Releases the transport, then shuts the sub-group down
void EECPASGLand::Shutdown()
{
    this->ReleaseTransport();
    EECPAttackSubGroup::Shutdown();
}

// FUNCTION: EXE 0x44647d
// Forgets every avoided tile
void EECPASGLand::ClearAvoidedTiles()
{
    this->mAvoidedTiles.clear();
}

// FUNCTION: EXE 0x446488
// Clears the avoided tiles, lets go of the transport (ending its land link to this group if it points here and
// dropping the reference) and clears the pickup and unload locations
void EECPASGLand::ReleaseTransport()
{
    EECPASGTransport* theTransport = this->mTransport;
    this->ClearAvoidedTiles();
    if (theTransport != NULL) {
        this->mTransport = NULL;
        EECPAttackSubGroup* theSubGroup = dynamic_cast<EECPAttackSubGroup*>(theTransport);
        if (theSubGroup != NULL) {
            if (theTransport->mLand == this)
                theTransport->ReleaseLand();
            theSubGroup->Dereference();
        }
    }
    this->ClearLocations();
}

// FUNCTION: EXE 0x4464e4
// Clears the pickup and unload locations and the embark and landing tiles
void EECPASGLand::ClearLocations()
{
    this->mUnloadLocation.Clear();
    this->mLandingTile = EETileLocation();
    this->mPickupLocation.Clear();
    this->mEmbarkTile = EETileLocation();
}

// FUNCTION: EXE 0x446527
// Resets the location to no area, no player and tile (0, 0)
void EECPILocation::Clear()
{
    this->mSerial = 0;
    this->mType = kAIAreaNone;
    this->mPlayer = 0;
    this->mTile.mX = 0;
    this->mTile.mY = 0;
}

// FUNCTION: EXE 0x446539
// Returns true while the tile is avoided and its time has not run out; an expired entry is removed
bool EECPASGLand::IsTileAvoided(const EETileLocation& inTile)
{
    bool theResult = false;
    std::vector<EECPASGLandPathPoint>::iterator theIter;
    for (theIter = this->mAvoidedTiles.begin(); theIter != this->mAvoidedTiles.end(); theIter++) {
        if (theIter->mTile == inTile)
            break;
    }
    if (theIter != this->mAvoidedTiles.end()) {
        if (theIter->mTime > gPhysics->mGameTime)
            theResult = true;
        else
            this->mAvoidedTiles.erase(theIter);
    }
    return theResult;
}

// FUNCTION: EXE 0x4465a5
// Chooses where the transport unloads by its kind (sea: shore, air, space); returns whether the point changed
// (false without a transport)
bool EECPASGLand::ChooseUnloadPoint()
{
    EECPAttackSubGroup* theTransport = dynamic_cast<EECPAttackSubGroup*>(this->mTransport);
    bool theResult = false;
    if (theTransport != NULL) {
        switch (theTransport->mKind) {
        case kASGKindSeaTransport:
            theResult = this->ChooseShoreUnloadPoint(0);
            break;
        case kASGKindAirTransport:
            theResult = this->ChooseAirUnloadPoint();
            break;
        case kASGKindSpaceTransport:
            theResult = this->ChooseSpaceUnloadPoint();
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x446605
// Returns the danger ratio: the transport's while in transit (0 without one), else the base sub-group's
float EECPASGLand::GetDangerRatio(long inArg1)
{
    float theResult = 0.0f;
    if (this->mState == kASGStateInTransit && this->mTransport != NULL) {
        EECPAttackSubGroup* theSubGroup = dynamic_cast<EECPAttackSubGroup*>(this->mTransport);
        if (theSubGroup != NULL) {
            theResult = theSubGroup->GetDangerRatio(0);
        }
    } else {
        theResult = EECPAttackSubGroup::GetDangerRatio(inArg1);
    }
    return theResult;
}

// FUNCTION: EXE 0x44665b
// Returns the continent to land on: the given one when both IDs match and it is land (or unknown), else the
// continent of the land tile nearest the landing tile (the task tile when no landing tile is set)
EEAIContinent* EECPASGLand::GetTargetContinent(unsigned short inID, unsigned short inOtherID, long inArg3)
{
    EEAIContinent* theContinent = gContinentManager.GetContinentByID(inID);
    if (inID == inOtherID && (theContinent == NULL || theContinent->mCategory == kContinentCategoryLand))
    {
        const std::vector<EEAIContinent*>& theContinents = gContinentManager.mContinents;
        return theContinents[inOtherID];
    }
    EETileLocation theTile;
    if (this->mLandingTile.GetX() == 0 && this->mLandingTile.GetY() == 0)
        theTile = gContinentManager.GetNearestLandTile(this->mTask.mLocation.mTile);
    else
        theTile = gContinentManager.GetNearestLandTile(this->mLandingTile);
    return gContinentManager.GetContinentAt(theTile.mX, theTile.mY);
}

// FUNCTION: EXE 0x4466dd
// Builds the terrain mask the sub-group may cross from its units' terrain flags (the intersection
// over all units), plus walls and gates
// NEAR: inlining and register allocation differ from the original
void EECPASGLand::BuildMoveMask()
{
    memset(this->mFlags.mFlags, 1, sizeof(this->mFlags.mFlags));
    EECPASGFlags theUnitFlags;   // each unit's flags, ANDed into the mask (was written straight into it)
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        this->mUnits[i]->mUnit->GetUnitClass()->GetTerrainFlags(this->mUnits[i]->mUnit, theUnitFlags);
        this->mFlags.And(theUnitFlags);
    }
    this->mFlags.mPlayerID = this->mPlayer->mPlayerID;
    this->mFlags.mFlags[kAITileWall] = true;
    this->mFlags.mFlags[kAITileGate] = true;
}

static const float kASGLWeightMin = 0.02f;   // 0x43ad6c (unit-local consts, not the pooled literals)
static const float kASGLWeightMax = 0.15f;   // 0x43ad70
static const float kASGLMixedChance = 0.8f;  // 0x43ad74

// FUNCTION: EXE 0x446776
// Fills the route cost weights: across continents water and space are cheap (cliffs free from the Atomic age
// with air-borne units); on one continent they are cheapened at random by epoch, boarding abilities and aggression
// NEAR: the cost rules and the order of the random calls follow the original; the weight slot meanings
// are inferred
void EECPASGLand::GetPathCosts(void* inFrom, void* inTo, EECPASGWeights& outWeights)
{
    EETileLocation* theFrom = static_cast<EETileLocation*>(inFrom);
    EETileLocation* theTo = static_cast<EETileLocation*>(inTo);
    unsigned short theFromContinent = gContinentManager.GetContinentID(theFrom->GetX(), theFrom->GetY());
    unsigned short theToContinent = gContinentManager.GetContinentID(theTo->GetX(), theTo->GetY());
    float theReadiness = 0.0f;
    bool theHasWall = false;
    bool theHasLand = false;
    bool theHasSpace = false;
    bool theMixedSectors = false;
    long theSpecial = 0;
    long theLand = 0;
    long theAir = 0;
    long theSpace = 0;
    outWeights.Fill(1.0f);
    outWeights.mValues[6] = 0.0f;
    outWeights.mValues[3] = this->GetWallWeight();
    outWeights.mValues[2] = this->GetGateWeight();
    if (outWeights.mValues[3] == 1.0f) {
        EEAIPathSubSector* theSector = gContinentManager.GetPathSubSectorAt(theFrom->GetX(), theFrom->GetY());
        if (theSector != NULL) {
            for (long i = theSector->mNeighbors.size() - 1; i >= 0; --i) {
                EEAIPathSubSector* theNeighbor = theSector->mNeighbors[i];
                if (theNeighbor == NULL)
                    continue;
                if (!theHasWall && theNeighbor->mKind == kAITileWall)
                    theHasWall = true;
                else if (!theHasLand) {
                    if (theNeighbor->mKind == kAITileWater || (theNeighbor->mKind > kAITileMapEdge && theNeighbor->mKind <= kAITileNearShore))
                        theHasLand = true;
                } else if (!theHasSpace && theNeighbor->mKind == kAITileSpace)
                    theHasSpace = true;
            }
            if (theHasWall && (theHasLand || theHasSpace)) {
                outWeights.mValues[3] = gPhysics->GetRandom(kASGLWeightMin, kASGLWeightMax);
                outWeights.mValues[2] = gPhysics->GetRandom(kASGLWeightMin, kASGLWeightMax);
                theMixedSectors = true;
            }
        }
    }
    for (long j = this->mUnits.size() - 1; j >= 0; --j) {
        EEUnitClass* theClass = this->mUnits[j]->mUnit->GetUnitClass();
        if (theClass->mCanEnterAirTransports && !this->mNoTransport)
            ++theLand;
        if (theClass->mCanEnterSeaTransports)
            ++theAir;
        if (theClass->mCanEnterSpaceTransports)
            ++theSpace;
        if ((this->mUnits[j]->mUnit->mStats->mTerrainMask >> 14) & 1)
            ++theSpecial;
        theReadiness += this->mUnits[j]->mAggression * 1.23f;
    }
    if (theToContinent != theFromContinent) {
        outWeights.mValues[1] = 0.02f;
        outWeights.mValues[10] = 0.03f;
        if (this->mPlayer->mTechnologies->mCurrentEpoch >= kTechEpochAtomicModern && theLand != 0)
            outWeights.mValues[5] = 0.0f;
    } else {
        if (this->mPlayer->mTechnologies->mCurrentEpoch > kTechEpochPrehistoric && theAir != 0 && (this->mMission == 2 || this->mMission == 0) &&
            gPhysics->mRandom.Generate() > 0.15f && (!theMixedSectors || gPhysics->mRandom.Generate() > kASGLMixedChance) &&
            theReadiness != 0.0f) {
            theReadiness /= (float)this->mUnits.size();
            outWeights.mValues[1] = max(0.0f, 1.0f - theReadiness);
        }
        if (this->mPlayer->mTechnologies->mCurrentEpoch >= kTechEpochSpace && theSpace != 0 && (this->mMission == 2 || this->mMission == 0) &&
            theReadiness != 0.0f) {
            theReadiness /= (float)this->mUnits.size();
            outWeights.mValues[10] = max(0.0f, 1.0f - theReadiness);
        }
        if (this->mPlayer->mTechnologies->mCurrentEpoch >= kTechEpochAtomicModern && theLand != 0 &&
            gPhysics->mRandom.Generate() > 0.15f && theReadiness != 0.0f)
            outWeights.mValues[5] = max(0.0f, 1.0f - theReadiness / (float)this->mUnits.size());
    }
    if (outWeights.mValues[1] != 1.0f) {
        if (this->mFlags.mFlags[kAITileWater])
            outWeights.mValues[1] = 0.0f;
        else if (outWeights.mValues[1] == 0.0f)
            outWeights.mValues[1] = 0.02f;
    }
    if (outWeights.mValues[10] != 1.0f) {
        if (this->mFlags.mFlags[kAITileSpace])
            outWeights.mValues[10] = 0.0f;
        else if (outWeights.mValues[10] == 0.0f)
            outWeights.mValues[10] = 0.02f;
    }
    if (theSpecial == (long)this->mUnits.size())
        outWeights.mValues[0] = 0.0f;
}

// FUNCTION: EXE 0x446c72
// Moves the given units into another land sub-group, copying task, locations, transport tiles and settings;
// other kinds use the base split; returns whether units moved
bool EECPASGLand::SplitInto(EECPAttackSubGroup* inOther, std::vector<EEComplexUnit*>& inUnits)
{
    EECPILocation theLocation;
    EECPASGLand* theOther = dynamic_cast<EECPASGLand*>(inOther);
    EECPAttackSubGroupUnit* theUnit = NULL;
    bool theResult = false;
    if (theOther != NULL) {
        theOther->mArea = this->mArea;
        theOther->mNextWaypoint = this->mNextWaypoint;
        theOther->mTask = this->mTask;
        theOther->mRallyPoint = this->mRallyPoint;
        theOther->mPickupLocation = this->mPickupLocation;
        theOther->mUnloadLocation = this->mUnloadLocation;
        theOther->mEmbarkTile = this->mEmbarkTile;
        theOther->mLandingTile = this->mLandingTile;
        theOther->mGroup = this->mGroup;
        theOther->mPlayer2 = this->mPlayer2;
        theOther->mRequest = NULL;
        theOther->mNextTaskCheckTime = 0;
        theOther->mU1BC = 0;
        theOther->mNextFollowTime = 0;
        theOther->mNextScanTime = 0;
        theOther->mNextUpdateTime = 0;
        theOther->mU1B0 = 0;
        theOther->mNextPositionTime = 0;
        theOther->mScanRange = this->mScanRange;
        theOther->mU1B4 = this->mU1B4;
        theOther->mState = kASGStateIdle;
        theOther->mKind = this->mKind;
        theOther->mB1E0 = this->mB1E0;
        theOther->mB1E1 = this->mB1E1;
        theOther->mB1E2 = this->mB1E2;
        theOther->mIsOutOfFight = this->mIsOutOfFight;
        theOther->mAggression = this->mAggression;
        theOther->mGatherDeadline = this->mGatherDeadline;
        theOther->mMission = this->mMission;
        theOther->mHasAreaEffect = this->mHasAreaEffect;
        theOther->mPosition = this->mPosition;
        if (inUnits.size() > 0) {
            this->mNumSplits++;
            if (this->mTransport != NULL)
                theOther->mTask.mIsActive = false;
            theOther->mNumSplits = this->mNumSplits;
            for (long i = inUnits.size() - 1; i >= 0; i--) {
                EEComplexUnit*& theTarget = inUnits[i];
                theUnit = EECPCreateSubGroupUnit(theTarget, theOther, 0);
                theUnit->Virtual48();
                if (theOther->mIsOutOfFight)
                    theUnit->mIsOutOfFight = true;
                theOther->mUnits.push_back(theUnit);
            }
            theOther->RankUnits();
            theResult = inUnits.size() > 0;
            this->RemoveUnits(inUnits);
        }
        if (this->mUnits.size() > 0)
            this->RefreshUnits();
        if (theOther->mUnits.size() > 0)
            theOther->RefreshUnits();
    } else {
        theResult = EECPAttackSubGroup::SplitInto(inOther, inUnits);
    }
    return theResult;
}

// FUNCTION: EXE 0x446f78
// Counts the units currently carried as cargo by their container
long EECPASGLand::CountUnitsAboard()
{
    long theCount = 0;
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        EEComplexUnit* theUnit = this->mUnits[i]->mUnit;
        if (theUnit->mContainer != NULL && theUnit->mContainer->HasCargo(theUnit))
            theCount++;
    }
    return theCount;
}

// FUNCTION: EXE 0x446fc0
// Returns true when the given unit is in this unit's cargo
bool EEComplexUnit::HasCargo(EEComplexUnit* inCargo)
{
    return std::find(this->mCargo.begin(), this->mCargo.end(), inCargo) != this->mCargo.end();
}

// FUNCTION: EXE 0x447015
// Counts the units GetUnitsNotCarriable lists (those the transport would not pick up)
long EECPASGLand::CountUnitsNotCarriable()
{
    std::vector<EEComplexUnit*> theUnits;
    this->GetUnitsNotCarriable(theUnits);
    return theUnits.size();
}

// FUNCTION: EXE 0x44705c
// Sums the weight of the non-transport units per transport kind they can board (outA land, outB sea, outC air,
// outD space); returns how many can board at least one kind
long EECPASGLand::GetTransportNeeds(long& outA, long& outB, long& outC, long& outD)
{
    outA = 0;
    outB = 0;
    outC = 0;
    outD = 0;
    long theCount = 0;
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        EEUnitClass* theClass = this->mUnits[i]->mUnit->GetUnitClass();
        if (theClass->mTransportCapacity != 0)
            continue;
        if (theClass->mCanEnterLandTransports)
            outA += theClass->mWeight;
        if (theClass->mCanEnterAirTransports)
            outC += theClass->mWeight;
        if (theClass->mCanEnterSeaTransports)
            outB += theClass->mWeight;
        if (theClass->mCanEnterSpaceTransports)
            outD += theClass->mWeight;
        if (theClass->mCanEnterLandTransports || theClass->mCanEnterAirTransports || theClass->mCanEnterSeaTransports || theClass->mCanEnterSpaceTransports)
            theCount++;
    }
    return theCount;
 EE_LOST_DEBUG(8); /* LOST-DEBUG: none: exact only (lostdebug_search, smallest n) */ }

// FUNCTION: EXE 0x447145
// Takes the units the request delivered; when they stand on more than one continent, queues a split order
bool EECPASGLand::TakeRequestedUnits()
{
    std::map<unsigned short, std::vector<EECPAttackSubGroupUnit*>*> theUnits;
    std::map<unsigned short, std::vector<EECPAttackSubGroupUnit*>*>::iterator theIter;
    EECPAGOrder theOrder;
    bool theResult = EECPAttackSubGroup::TakeRequestedUnits();
    if (theResult && this->mUnits.size() > 1) {
        this->GroupUnitsByContinent(theUnits, true);
        if (theUnits.size() > 1) {
            theOrder.mType = kAGOrderSplitSubGroup;
            theOrder.mSplitKind = 7;
            theOrder.SetSubGroup(this);
            theOrder.mIsActive = true;
            this->mGroup->mOrders.push_back(theOrder);
        }
        for (theIter = theUnits.begin(); theIter != theUnits.end(); theIter++)
            delete theIter->second;
    }
    this->RefreshUnits();
    return theResult;
}

// FUNCTION: EXE 0x447231
// Picks units to split off: with a transport the units it cannot load (no fighters: GetUnitsNotCarriable); for
// split kind 11 the units not aboard; else those left behind or the base choice; never every unit
void EECPASGLand::PickSplitUnits(std::vector<EEComplexUnit*>& outUnits)
{
    EE_LOST_DEBUG(1);  // LOST-DEBUG: none: exact only (lostdebug_search, smallest n)
    long i;
    EEComplexUnit* theUnit;
    EECPAttackSubGroup* theTransport = dynamic_cast<EECPAttackSubGroup*>(this->mTransport);
    outUnits.clear();
    bool theFound = false;
    if (theTransport != NULL && this->mSplitKind != 11) {
        if (theTransport->CountFightingUnits() > 0) {
            theFound = this->GetUncarriableUnits(outUnits);
        } else {
            this->GetUnitsNotCarriable(outUnits);
            theFound = outUnits.size() > 0;
        }
    }
    if (!theFound) {
        if (this->mSplitKind == 11) {
            for (i = this->mUnits.size() - 1; i >= 0; i--) {
                if (this->mUnits[i]->mUnit->mContainer == NULL) {
                    theUnit = this->mUnits[i]->mUnit;
                    outUnits.push_back(theUnit);
                }
            }
            theFound = outUnits.size() != 0;
        }
        if (!theFound) {
            if (theTransport != NULL && theTransport->CountFightingUnits() > 0)
                this->GetUnitsLeftBehind(outUnits);
            else
                EECPAttackSubGroup::PickSplitUnits(outUnits);
        }
    }
    if (outUnits.size() >= this->GetNumUnits() && !outUnits.empty())
        outUnits.pop_back();
}

// FUNCTION: EXE 0x4473aa
// Lists the units outside transports and not already boarding that the transport's class rejects (needs a
// transport with fighting units and more than one own fighter); returns true when there are any
bool EECPASGLand::GetUncarriableUnits(std::vector<EEComplexUnit*>& outUnits)
{
    // The original (0x4473bb/0x447589) constructs and destroys this first local without using it.
    std::vector<EEComplexUnit*> theUnits;
    std::vector<EEComplexUnit*> theCandidates;
    bool theResult = false;
    outUnits.clear();
    if (this->mTransport != NULL && this->CountFightingUnits() > 1) {
        EECPAttackSubGroup* theGroup = dynamic_cast<EECPAttackSubGroup*>(this->mTransport);
        if (theGroup != NULL && theGroup->CountFightingUnits() > 0) {
            EEComplexUnit* theTransport = theGroup->GetUnitUnit(0);
            EEUCTransport* theClass = dynamic_cast<EEUCTransport*>(theTransport->GetUnitClass());
            for (long i = this->mUnits.size() - 1; i >= 0; i--) {
                if (!this->mUnits[i]->IsDone()) {
                    EEComplexUnit* theUnit = this->mUnits[i]->GetUnit();
                    if (theUnit->GetContainer() == NULL &&
                        (theUnit->GetGoal() == NULL || dynamic_cast<EEUGLoadTransport*>(theUnit->GetLoadGoal()) == NULL)) {
                        theCandidates.push_back(this->mUnits[i]->GetUnit());
                    } else {
                        dynamic_cast<EEUGLoadTransport*>(theUnit->GetLoadGoal()); EE_LOST_DEBUG(1);  // LOST-DEBUG: none: exact only (else arm: a compiled-away trace; the discarded cast stays)
                    }
                }
            }
            theClass = dynamic_cast<EEUCTransport*>(theTransport->GetUnitClass());
            theClass->FilterLoadable(&theCandidates, theTransport, &outUnits);
            theResult = outUnits.size() != 0;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x44759b
// Returns the distance to the transport sub-group, 0 without one
float EECPASGLand::GetTransportDistance()
{
    float theResult = 0.0f;
    if (this->mTransport != NULL)
        theResult = this->DistanceTo(dynamic_cast<EECPAttackSubGroup*>(this->mTransport));
    return theResult;
}

// FUNCTION: EXE 0x4475cf
// Returns the computer player's chokepoint-guard share (0 without one)
float EECPASGLand::GetGuardChokepointPercentage()
{
    return this->mPlayer2 != NULL ? this->mPlayer2->mGuardChokepointPercentage : 0;
}

// FUNCTION: EXE 0x4475e3
// Returns the computer player's high-ground-guard share (0 without one)
float EECPASGLand::GetGuardHighGroundPercentage()
{
    return this->mPlayer2 != NULL ? this->mPlayer2->mGuardHighGroundPercentage : 0;
}

// FUNCTION: EXE 0x4475f7
// Returns the computer player's continent-guard share (0 without one)
float EECPASGLand::GetGuardContinentPercentage()
{
    return this->mPlayer2 != NULL ? this->mPlayer2->mGuardContinentPercentage : 0;
}

// FUNCTION: EXE 0x44760b
// Returns the computer player's land town-guard share (0 without one)
float EECPASGLand::GetGuardTownPercentage()
{
    return this->mPlayer2 != NULL ? this->mPlayer2->mGuardTownLandPercentage : 0;
}

// FUNCTION: EXE 0x44761f
// Collects land targets in the bounds: walls go to the siege list (and the target list when clearing a sector
// they are in or near); others to the target list, towers, gates and kind-4 tech nodes also to the siege list
void EECPASGLand::CollectTargets(EEAIAStarBounds* inBounds, bool inIncludeEnemies)
{
    std::vector<EEComplexUnit*> theUnits;
    unsigned short theSector = 0;
    if (this->mNextSubSectorTile.GetX() != 0 || this->mNextSubSectorTile.GetY() != 0)
        theSector = gContinentManager.GetPathSubSectorID(this->mNextSubSectorTile.GetX(), this->mNextSubSectorTile.GetY());
    bool theTileReady = false;
    if (this->mNextSubSectorTile.GetX() != 0 || this->mNextSubSectorTile.GetY() != 0) {
        EETile* theTile = gWorld.GetMap()->GetTile(this->mNextSubSectorTile.GetX(), this->mNextSubSectorTile.GetY());
        theTileReady = theTile != NULL && theTile->IsSeenBy(this->mPlayer);
    }
    EECPALandTargetFilter theFilter;
    theFilter.mCenter = this->mNextSubSectorTile;
    theFilter.mGroup = this;
    theFilter.mPlayer = this->mPlayer2;
    theFilter.mPlayerID = this->mTask.GetPlayerID();
    theFilter.mIncludeEnemies = inIncludeEnemies;
    theFilter.mAllowWalls = true;
    theFilter.mAllowGates = true;
    theFilter.mTileReady = theTileReady;
    EETileDistanceLess theDistance(this->mNextSubSectorTile);
    if (EECollectASGTargets(*inBounds, theUnits, theFilter, true) != 0) {
        this->mTargets.reserve(theUnits.size());
        this->mSiegeTargets.reserve(theUnits.size());
        for (long i = theUnits.size() - 1; i >= 0; --i) {
            EECPASTargetUnit* theTarget = EECPAttackSubGroup::NewTarget(theUnits[i]);
            EEUnitClass* theClass = theUnits[i]->GetUnitClass();
            if (theClass->mIsWall && !theClass->mIsTower) {
                this->mSiegeTargets.push_back(theTarget);
                if (this->mState == kASGStateClearSubSector && (theDistance.DistanceSquared(theTarget->mUnit->GetTile()) <= 9 ||
                    gContinentManager.GetPathSubSectorID(theTarget->mUnit) == theSector)) {
                    theTarget = EECPAttackSubGroup::NewTarget(theUnits[i]);
                    this->mTargets.push_back(theTarget);
                }
            } else {
                this->mTargets.push_back(theTarget);
                if (theClass->mIsTower || theClass->mCategory == kUnitCategoryGate ||
                    (theTarget->mUnit->GetTechTreeNode() != NULL &&
                     theTarget->mUnit->GetTechTreeNode()->mData->mKind == 4)) {
                    theTarget = EECPAttackSubGroup::NewTarget(theUnits[i]);
                    this->mSiegeTargets.push_back(theTarget);
                }
            }
        }
    }
}

// FUNCTION: EXE 0x4478bd
// True with no units, on the same continent, or when a unit can ride a sea/air transport there (air only if
// cliffs cut it off, then type 17 must be buildable); water/space targets also need a unit able to move there
bool EECPASGLand::CanReach(const EECPILocation* inLocation)
{
    std::vector<long> theTypes;
    EECPILocation theLocation(*inLocation);
    bool theResult = false;
    bool theNeedBuildable = false;
    if (this->mUnits.size() > 0) {
        if (theLocation.IsValid()) {
        unsigned short theCurrentID = this->GetMainContinent();
        // The original (0x447921..0x447930) reads the argument itself after validating a separate copy.
        // Keep that distinction in case IsValid repairs the copy.
        EEAIContinent* theContinent = gContinentManager.GetContinentAt(inLocation->mTile.GetX(), inLocation->mTile.GetY());
        if (theContinent != NULL) {
            if (theCurrentID == theContinent->mID) {
                theResult = true;
            } else if (theContinent->mCategory != kContinentCategoryCliff) {
                bool theAllWater = gContinentManager.AllNeighborsOfKind(theContinent->mID, kContinentCategoryCliff);
                for (long i = this->mUnits.size() - 1; i >= 0 && !theResult; --i) {
                    EEUnitClass* theClass = this->mUnits[i]->GetUnit()->GetUnitClass();
                    if (!theAllWater) {
                        theResult = theClass->mCanEnterAirTransports || theClass->mCanEnterSeaTransports;
                    } else {
                        theResult = theClass->mCanEnterAirTransports;
                        theNeedBuildable = true;
                    }
                }
                if (theResult == true && (theContinent->mCategory == kContinentCategoryWater || theContinent->mCategory == kContinentCategorySpace)) {
                    theResult = false;
                    for (long i = this->mUnits.size() - 1; i >= 0 && !theResult; --i) {
                        theResult = (*(this->mUnits.begin() + i))->GetUnit()->GetUnitClass()->CanMoveOnTerrain(this->mUnits[i]->GetUnit(), theContinent->mCategory == kContinentCategoryWater ? 1 : 12);
                    }
                }
            }
        }
        }
    } else {
        theResult = true;
    }
    if (theResult && theNeedBuildable)
        theResult = this->mPlayer->GetBuildableTypes(17, theTypes) != 0;
    return theResult;
}

// FUNCTION: EXE 0x447aa0
// For a wall, gate or obstacle sector: false when a living wall or gate on its tile belongs to an ally, or a
// blocking land building to an enemy; true otherwise
// NEAR: inlining and register allocation differ from the original
bool EECPASGLand::IsGatePassable(EEAIPathSubSector* inSector)
{
    bool theResult = true;
    if (inSector != NULL && (inSector->mKind == kAITileWall || inSector->mKind == kAITileGate || inSector->mKind == kAITileUnitObstacle)) {
        EEMapTile* theTile = gWorld.GetMap()->GetTile(inSector->mLocation.GetX(), inSector->mLocation.GetY());
        if (theTile != NULL) {
            for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL && theResult == true; theNode = theNode->mNext) {
                EEUnit* theUnit = theNode->mUnit;
                if (theUnit->mHitPoints <= 0)
                    continue;
                EEUnitClass* theClass = theUnit->GetUnitClass();
                if (theClass->mIsWall || theClass->mCategory == kUnitCategoryGate) {
                    theResult = !(this->mPlayer2->IsAlly(theUnit->mPlayer));
                } else if (theClass->IsLandBuilding() && theUnit->GetUnitClass()->mBlocksTile) {
                    theResult = theUnit->mPlayer == this->mPlayer2 || this->mPlayer2->mEnemies[theUnit->mPlayer->mPlayerID] == 0;
                } else {
                    theResult = true;
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x447b93
// For a wall, gate or obstacle sector: true when a living wall, a gate that will not let the player through, or
// a finished blocking land building on its tile belongs to a non-ally
// NEAR: inlining and register allocation differ from the original
bool EECPASGLand::IsGateBlocked(EEAIPathSubSector* inSector)
{
    bool theResult = false;
    if (inSector != NULL && (inSector->mKind == kAITileWall || inSector->mKind == kAITileGate || inSector->mKind == kAITileUnitObstacle)) {
        EEMapTile* theTile = gWorld.GetMap()->GetTile(inSector->mLocation.GetX(), inSector->mLocation.GetY());
        if (theTile != NULL) {
            for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL && !theResult; theNode = theNode->mNext) {
                EEUnit* theUnit = theNode->mUnit;
                if (theUnit->mHitPoints <= 0)
                    continue;
                EEUnitClass* theClass = theUnit->GetUnitClass();
                if (!theClass->mIsWall) {
                    if (theClass->mCategory == kUnitCategoryGate)
                        theResult = !inSector->IsPassable(this->mPlayer2->mPlayerID);
                    else if (theClass->IsLandBuilding() && theUnit->GetUnitClass()->mBlocksTile && !theUnit->IsFoundation())
                        theResult = true;
                    else
                        theResult = false;
                    if (theResult != true)
                        continue;
                }
                theResult = !(this->mPlayer->IsAlly(theUnit->mPlayer));
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x447c96
// Plans which units not aboard fit on each transport unit; returns true as soon as a fitting unit and its
// transport are both standing still with no load order
bool EECPASGLand::HasIdleUnitToLoad()
{
    std::vector<EEComplexUnit*> theAvailable;
    std::vector<EEComplexUnit*> theTransports;
    std::vector<EECPAILoadGroup> theGroups;
    std::vector<EEComplexUnit*> theRejected;
    bool theResult = false;
    EECPAttackSubGroup* theTransport = NULL;
    if (this->mTransport != NULL) {
        this->GetUnitsNotAboard(theAvailable);
        if (theAvailable.size() > 0) {
            theTransport = dynamic_cast<EECPAttackSubGroup*>(this->mTransport);
            if (theTransport != NULL && theTransport->CountFightingUnits() > 0) {
                for (long i = 0; i < (long)theTransport->GetNumUnits(); i++)
                    theTransports.push_back(theTransport->GetUnitUnit(i));
                EEComplexUnit* theLast = theTransports.back();
                dynamic_cast<EEUCTransport*>(theLast->GetUnitClass())->FilterLoadable(&theAvailable, theLast, &theRejected);
            }
        }
        for (long i = theTransports.size() - 1; i >= 0; i--) {
            theGroups.push_back(EECPAILoadGroup());
            EECPAILoadGroup& theCurrent = theGroups.back();
            theCurrent.mTransport = theTransports[i];
            theCurrent.ResetLoad();
            if (theCurrent.mTransport->mHitPoints > 0) {
                while (theAvailable.size() != 0) {
                    if (theAvailable.back()->mContainer == NULL) {
                        if (theAvailable.back()->GetUnitClass()->mWeight + theCurrent.mLoad > theCurrent.mTransport->GetUnitClass()->mTransportCapacity)
                            break;
                        if (theCurrent.mTransport->mWaypoints.size() == 0 && theCurrent.mTransport->mIsPathDone &&
                            theAvailable.back()->GetNumWaypoints() == 0 && theAvailable.back()->mIsPathDone &&
                            dynamic_cast<EEUGLoadTransport*>(theAvailable.back()->GetLoadGoal()) == NULL) {
                            theResult = true;
                            i = -1;
                            break;
                        }
                        theCurrent.mUnits.push_back(theAvailable.back());
                        theCurrent.mLoad += theAvailable.back()->GetUnitClass()->mWeight;
                    }
                    theAvailable.pop_back();
                }
            }
            if (theAvailable.size() == 0)
                break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x447f58
// Sets the load group's current load from the capacity its transport already uses
void EECPAILoadGroup::ResetLoad()
{
    if (this->mTransport != NULL)
        this->mLoad = this->mTransport->GetUnitClass()->mTransportCapacity - this->mTransport->mCargoRoom;
}

// FUNCTION: EXE 0x447f72
// Finds a free landing tile on the first continent's shore toward the second (shared regions, or the shore
// next to it), nearest the given tile (outTile when both are the same); returns whether one was found
bool EECPASGLand::FindCrossingTile(EEAIContinent* inFirst, EEAIContinent* inSecond,
    const EETileLocation& inTile, EETileLocation& outTile, bool inWater)
{
    std::vector<U2DPoint<long> > theTiles;
    std::vector<unsigned short> theIDs;
    bool theResult = inFirst != NULL && inSecond != NULL;
    long theKind = inWater ? 3 : 1;
    if (theResult) {
        theResult = false;
        unsigned short theID = gContinentManager.GetContinent(inTile, theKind);
        if (theID != 0) {
            inFirst->GetShoreInRegion(theID, theTiles, theKind);
        } else {
            // the original uses BC's count even when indexing the separate DC vector; an index past DC's end
            // reads as id 0 instead of reading past the vector
            for (long i = inFirst->mIDsBC.size() - 1; i >= 0; i--) {
                if (inWater) {
                    unsigned short theDCID = (unsigned long)i < inFirst->mIDsDC.size() ? inFirst->mIDsDC[i] : 0;
                    if (std::find(inSecond->mIDsDC.begin(), inSecond->mIDsDC.end(), theDCID) != inSecond->mIDsDC.end())
                        theIDs.push_back(theDCID);
                } else {
                    if (std::find(inSecond->mIDsBC.begin(), inSecond->mIDsBC.end(), inFirst->mIDsBC[i]) != inSecond->mIDsBC.end())
                        theIDs.push_back(inFirst->mIDsBC[i]);
                }
            }
            if (theIDs.size() > 0)
                inFirst->GetShoreInRegions(theIDs, theTiles, theKind);
            else
                inFirst->GetShoreNextTo(inSecond->mID, theTiles);
        }
        if (inFirst != inSecond)
            std::sort(theTiles.begin(), theTiles.end(), EECPASGLandTileFarther(inTile));
        else
            std::sort(theTiles.begin(), theTiles.end(), EECPASGLandTileFarther(outTile));
        while (!theTiles.empty()) {
            if (this->CanTransportLandAt(theTiles.back())) {
                theResult = true;
                outTile = theTiles.back();
                break;
            }
            theTiles.pop_back();
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x44820e
// Appends the units that are not inside a container
void EECPASGLand::GetUnitsNotAboard(std::vector<EEComplexUnit*>& outUnits)
{
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        if (this->mUnits[i]->mUnit->mContainer == NULL) {
            EEComplexUnit* theUnit = this->mUnits[i]->mUnit;
            outUnits.push_back(theUnit);
        }
    }
}

// FUNCTION: EXE 0x448262
// Lists the active units outside containers that this transport kind would not pick up: transports, units
// unable to board it, off-map units, and units on water, cliff or map edge (air: on water); unknown kind: none
// NEAR: inlining and register allocation differ from the original
void EECPASGLand::GetUnitsNotCarriable(std::vector<EEComplexUnit*>& outUnits)
{
    outUnits.clear();
    if (this->mTransport == NULL)
        return;
    EECPAttackSubGroup* theTransport = dynamic_cast<EECPAttackSubGroup*>(this->mTransport);
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        if (this->mUnits[i]->IsDone())
            continue;
        EEComplexUnit* theUnit = this->mUnits[i]->mUnit;
        if (theUnit->mContainer != NULL)
            continue;
        EEUnitClass* theClass = theUnit->GetUnitClass();
        if (theClass->mTransportCapacity == 0) {
            EEAIContinent* theContinent;
            switch (theTransport->mKind) {
            case kASGKindSeaTransport:
                if (theClass->mCanEnterSeaTransports && theUnit->mTileX != -10000) {
                    theContinent = gContinentManager.GetContinentAt(theUnit);
                    if (theContinent != NULL && theContinent->mCategory != kContinentCategoryCliff && theContinent->mCategory != kContinentCategoryWater
                        && theContinent->mCategory != kContinentCategoryMapEdge)
                        continue;
                }
                break;
            case kASGKindAirTransport:
                if (theClass->mCanEnterAirTransports && theUnit->mTileX != -10000) {
                    theContinent = gContinentManager.GetContinentAt(theUnit);
                    if (theContinent != NULL && theContinent->mCategory != kContinentCategoryWater)
                        continue;
                }
                break;
            case kASGKindSpaceTransport:
                if (theClass->mCanEnterSpaceTransports && theUnit->mTileX != -10000) {
                    theContinent = gContinentManager.GetContinentAt(theUnit);
                    if (theContinent != NULL && theContinent->mCategory != kContinentCategoryCliff && theContinent->mCategory != kContinentCategoryWater
                        && theContinent->mCategory != kContinentCategoryMapEdge)
                        continue;
                }
                break;
            default:
                continue;
            }
        }
        outUnits.push_back(theUnit);
    }
}

// FUNCTION: EXE 0x4483b7
// Lists the units the transport rejects plus, when it has fighting units, those that do not fit in its units'
// remaining capacity
void EECPASGLand::GetUnitsLeftBehind(std::vector<EEComplexUnit*>& outUnits)
{
    std::vector<EEComplexUnit*> theAvailable;
    std::vector<EEComplexUnit*> theTransports;
    std::vector<EECPAILoadGroup> theGroups;
    EECPAttackSubGroup* theTransport = NULL;
    outUnits.clear();
    if (this->mTransport != NULL) {
        this->GetUnitsNotAboard(theAvailable);
        if (theAvailable.size() > 0) {
            theTransport = dynamic_cast<EECPAttackSubGroup*>(this->mTransport);
            if (theTransport != NULL && theTransport->CountFightingUnits() > 0) {
                for (long i = 0; i < (long)theTransport->GetNumUnits(); i++)
                    theTransports.push_back(theTransport->GetUnitUnit(i));
                EEComplexUnit* theLast = theTransports.back();
                dynamic_cast<EEUCTransport*>(theLast->GetUnitClass())->FilterLoadable(&theAvailable, theLast, &outUnits);
            }
        }
        for (long i = theTransports.size() - 1; i >= 0; i--) {
            theGroups.push_back(EECPAILoadGroup());
            EECPAILoadGroup& theCurrent = theGroups.back();
            theCurrent.mTransport = theTransports[i];
            theCurrent.ResetLoad();
            if (theCurrent.mTransport->mHitPoints > 0) {
                while (theAvailable.size() != 0) {
                    if (theAvailable.back()->mContainer == NULL) {
                        if (theAvailable.back()->GetUnitClass()->mWeight + theCurrent.mLoad > theCurrent.mTransport->GetUnitClass()->mTransportCapacity)
                            break;
                        theCurrent.mUnits.push_back(theAvailable.back());
                        theCurrent.mLoad += theAvailable.back()->GetUnitClass()->mWeight;
                    }
                    theAvailable.pop_back();
                }
            }
            if (theAvailable.size() == 0)
                break;
        }
        if (theTransport != NULL && theTransport->CountFightingUnits() > 0) {
            while (!theAvailable.empty()) {
                outUnits.push_back(theAvailable.back());
                theAvailable.pop_back();
            }
        }
    }
}

// FUNCTION: EXE 0x44862a
// Enters a land sub-group state and runs its entry action (idle: drop the transport and get a task; split,
// request a path, take the next waypoint, start loading, unload at the chosen point); others go to the base
void EECPASGLand::SetState(long inState, long inTime)
{
    unsigned long theOldState = this->mState;
    this->mState = inState;
    switch (this->mState) {
    case kASGStateIdle:
        if (this->mTransport != NULL)
            this->DropTransport();
        if (this->mUnits.size() > 0 && !this->mTask.mIsActive)
            this->mGroup->AssignTask(this);
        break;
    case kASGStatePrepareTravel:
        break;
    case 1005:
        break;
    case kASGStateClearSubSector:
        break;
    case kASGStateApproachTask:
        break;
    case kASGStateUnload:
        break;
    case kASGStateAttackWhileBoarding:
        break;
    case kASGStateNoPath:
        break;
    case kASGStateSplit:
        this->RequestSplit(true);
        break;
    case kASGStateSplitOff:
        this->RequestSplit(false);
        break;
    case kASGStateRequestPath:
        this->RequestPathToTask();
        break;
    case kASGStateNextWaypoint:
        this->mNextSubSectorTile = EETileLocation();
        this->mNextWaypoint.Clear();
        if (!this->mPath.empty()) {
            this->mNextWaypoint.mTile = this->mPath.front().mTile;
            this->mPath.pop_front();
        }
        break;
    case kASGStateBoardTransport:
        this->StartTransportLoading();
        break;
    case kASGStateInTransit:
        if (this->mTransport != NULL) {
            EECPAttackSubGroup* theTransport = dynamic_cast<EECPAttackSubGroup*>(this->mTransport);
            unsigned long theTransportState = theTransport->mState;
            if (!this->mUnloadLocation.IsOnMap() || this->IsTileAvoided(this->mUnloadLocation.mTile))
                this->ChooseUnloadPoint();
            if (this->mPlayer->mDifficulty != kDifficultyEasy)
                this->mPlayer->mCombatManager->CoverLanding(this);
            this->mTransport->UnloadAt(this->mUnloadLocation);
            this->mTransportStage = 2;
            if (theTransportState == kASGStateRedirectCargo)
                theTransport->SetState(kASGStateRedirectCargo, inTime);
        }
        break;
    case kASGStateAttack:
        if (this->IsAwayFromTowns())
            this->SetState(kASGStateRetreatToTown, inTime);
        break;
    case kASGStateLoad:
        this->ScanTargets(1);
        break;
    case kASGStateSplitStragglers:
        this->mSplitKind = 11;
        this->RequestSplit(false);
        break;
    default:
        this->mState = theOldState;
        EECPAttackSubGroup::SetState(inState, inTime);
        break;
    }
}

// FUNCTION: EXE 0x4488b6
// Returns true when the location's tile is on the map
bool EECPILocation::IsOnMap()
{
    return gWorld.GetMap()->GetTileAtLocation(this->GetTile()) != NULL;
}

// FUNCTION: EXE 0x4488ce
// Runs one step of the land sub-group's state machine: scans targets, updates units, tests the state's
// conditions and switches state; the next update is in 2.5 s (0.5 s while requesting a path)
void EECPASGLand::Update(long inTime)
{
    long theDelay = 2500;
    if ((this->mNextScanTime <= inTime || this->mState == kASGStateClearSubSector) && this->mUnits.size() > 0 && this->mState != kASGStateInTransit && this->mState != kASGStateRetreatToTown)
        this->ScanTargets(1);
    else
        this->DropLostTargets(inTime);
    this->UpdateUnits(inTime);
    bool theFailed = false;
    bool theDone;
    switch (this->mState) {
    case kASGStateIdle:
        theFailed = !this->TestCondition(kASGConditionHasUnits, inTime);
        if (theFailed)
            this->SetState(kASGStateWaitForUnits, inTime);
        if (!theFailed) {
            if (this->TestCondition(kASGConditionHasTargets, inTime))
                this->SetState(kASGStateAttack, inTime);
            else if (this->TestCondition(kASGConditionTaskInOtherSubSector, inTime))
                this->SetState(kASGStatePrepareTravel, inTime);
            else if (this->TestCondition(kASGConditionShouldReturn, inTime))
                this->SetState(kASGStateReturnToRallyPoint, inTime);
            else {
                theFailed = !this->TestCondition(kASGConditionTaskValid, inTime);
                if (theFailed)
                    this->SetState(kASGStateIdle, inTime);
                if (!theFailed)
                    this->CheckTaskArrival(inTime);
            }
        }
        break;
    case kASGStateWaitForUnits:
        if (this->TestCondition(kASGConditionRequestEmpty, inTime))
            this->SetState(kASGStateRemove, inTime);
        else if (this->TestCondition(kASGConditionRequestFilled, inTime))
            this->SetState(kASGStateGather, inTime);
        break;
    case kASGStateGather:
        theFailed = !this->TestCondition(kASGConditionHasUnits, inTime);
        if (theFailed)
            this->SetState(kASGStateReturnToRallyPoint, inTime);
        if (!theFailed) {
            theDone = this->TestCondition(kASGConditionShouldSplit, inTime);
            if (theDone)
                this->SetState(kASGStateIdle, inTime);
            else
                this->mSplitKind = 10;
            if (!theDone && this->TestCondition(kASGConditionGathered, inTime))
                this->SetState(kASGStateIdle, inTime);
        }
        break;
    case kASGStateRemove:
        break;
    case kASGStatePrepareTravel:
        if (this->TestCondition(kASGConditionTaskValid, inTime)) {
            if (this->TestCondition(1013, inTime))
                this->SetState(kASGStateRequestPath, inTime);
            else
                this->SetState(kASGStateIdle, inTime);
        } else
            this->SetState(kASGStateIdle, inTime);
        break;
    case kASGStateSplit:
        if (this->TestCondition(kASGConditionAlways, inTime))
            this->SetState(kASGStateIdle, inTime);
        break;
    case kASGStateSplitOff:  // the original dispatches state 1007 here (0x448961 je, ebx = 0x3ef), not 1015
        if (!this->TestCondition(kASGConditionTaskValid, inTime) && !this->TestCondition(kASGConditionAnyAboard, inTime))
            this->SetState(kASGStateIdle, inTime);
        theFailed = !this->TestCondition(kASGConditionHasTransport, inTime);
        if (!theFailed)
            theFailed = this->TestCondition(kASGConditionNoneCarriable, inTime);
        if (theFailed)
            this->SetState(kASGStateIdle, inTime);
        else
            this->SetState(kASGStateBoardTransport, inTime);
        break;
    case kASGStateRequestPath:
        if (this->TestCondition(kASGConditionHasTransport, inTime))
            this->SetState(kASGStateLoad, inTime);
        else {
            theFailed = !this->TestCondition(kASGConditionTaskValid, inTime);
            if (theFailed)
                this->SetState(kASGStateIdle, inTime);
            if (!theFailed) {
                if (this->TestCondition(kASGConditionPathReady, inTime))
                    this->SetState(kASGStateNextWaypoint, inTime);
                else if (this->TestCondition(kASGConditionHasTargets, inTime))
                    this->SetState(kASGStateAttack, inTime);
            }
        }
        theDelay = 500;
        break;
    case kASGStateMove:
        if (!this->CheckTaskArrival(inTime)) {
            if (this->TestCondition(kASGConditionHasTargets, inTime))
                this->SetState(kASGStateAttack, inTime);
            else {
                theFailed = !this->TestCondition(kASGConditionTaskValid, inTime);
                if (theFailed)
                    this->SetState(kASGStateIdle, inTime);
                if (!theFailed) {
                    if (this->TestCondition(kASGConditionUnitsSettled, inTime) && this->TestCondition(1007, inTime))
                        this->SetState(kASGStateClearSubSector, inTime);
                    else {
                        theDone = false;
                        if (this->TestCondition(kASGConditionAllUnitsSettled, inTime)) {
                            theDone = this->TestCondition(1007, inTime);
                            if (theDone)
                                this->SetState(kASGStateClearSubSector, inTime);
                            else {
                                this->SetState(kASGStateNextWaypoint, inTime);
                                theDone = true;
                            }
                        }
                        if (!theDone && this->TestCondition(kASGConditionShouldReturn, inTime))
                            this->SetState(kASGStateReturnToRallyPoint, inTime);
                    }
                }
            }
        }
        break;
    case kASGStateClearSubSector:
        theFailed = !this->TestCondition(1007, inTime);
        if (theFailed)
            this->SetState(kASGStateIdle, inTime);
        if (!theFailed && !this->CheckTaskArrival(inTime)) {
            theFailed = !this->TestCondition(kASGConditionTargetsAtNextSubSector, inTime);
            if (theFailed)
                this->SetState(kASGStateIdle, inTime);
            if (!theFailed && !this->TestCondition(kASGConditionNextSubSectorValid, inTime))
                this->SetState(kASGStateIdle, inTime);
        }
        break;
    case kASGStateNextWaypoint:
        if (!this->CheckTaskArrival(inTime)) {
            if (this->TestCondition(kASGConditionHasTargets, inTime))
                this->SetState(kASGStateAttack, inTime);
            else {
                theFailed = !this->TestCondition(kASGConditionTaskValid, inTime);
                if (theFailed)
                    this->SetState(kASGStateIdle, inTime);
                if (!theFailed) {
                    if (this->TestCondition(1005, inTime))
                        this->SetState(kASGStateMove, inTime);
                    else if (this->TestCondition(kASGConditionShouldReturn, inTime))
                        this->SetState(kASGStateReturnToRallyPoint, inTime);
                    else
                        this->SetState(kASGStateApproachTask, inTime);
                }
            }
        }
        break;
    case kASGStateApproachTask:
        if (this->TestCondition(kASGConditionHasTransport, inTime))
            this->SetState(kASGStateLoad, inTime);
        else if (!this->CheckTaskArrival(inTime)) {
            if (this->TestCondition(kASGConditionHasTargets, inTime))
                this->SetState(kASGStateAttack, inTime);
            else {
                this->TestCondition(kASGConditionTaskValid, inTime);
                this->SetState(kASGStateIdle, inTime);
            }
        }
        break;
    case kASGStateBoardTransport:
        theFailed = !this->TestCondition(kASGConditionHasTransport, inTime);
        if (theFailed)
            this->SetState(kASGStateIdle, inTime);
        if (!theFailed) {
            theFailed = !this->TestCondition(kASGConditionTaskValid, inTime);
            if (theFailed) {
                theFailed = !this->TestCondition(kASGConditionAnyAboard, inTime);
                if (theFailed)
                    this->SetState(kASGStateIdle, inTime);
            }
            if (!theFailed) {
                theFailed = !this->TestCondition(4007, inTime);
                if (theFailed)
                    this->SetState(kASGStateSplitOff, inTime);  // push ebx at 0x44929f: ebx = 1007 on this path, not 1015
                if (!theFailed) {
                    if (this->TestCondition(kASGConditionAllAboard, inTime))
                        this->SetState(kASGStateInTransit, inTime);
                    else if (this->TestCondition(4009, inTime))
                        this->SetState(kASGStateBoardTransport, inTime);
                    else if (this->TestCondition(kASGConditionHasTargets, inTime))
                        this->SetState(kASGStateAttackWhileBoarding, inTime);
                }
            }
        }
        break;
    case kASGStateInTransit:
        if (this->TestCondition(kASGConditionAllUnloaded, inTime))
            this->SetState(kASGStateIdle, inTime);
        else {
            theDone = this->TestCondition(kASGConditionUnloadPointChosen, inTime);
            if (!theDone)
                theDone = this->TestCondition(kASGConditionTransportUnitsWaiting, inTime);
            if (theDone)
                this->SetState(kASGStateUnload, inTime);
            if (!theDone && !this->TestCondition(kASGConditionAllAboard, inTime) && !this->TestCondition(kASGConditionAllUnloaded, inTime))
                this->SetState(kASGStateSplitStragglers, inTime);
        }
        break;
    case kASGStateUnload:
        if (this->TestCondition(kASGConditionAllAboard, inTime))
            this->SetState(kASGStateInTransit, inTime);
        else if (this->TestCondition(kASGConditionAllUnloaded, inTime))
            this->SetState(kASGStateIdle, inTime);
        else
            this->SetState(kASGStateSplitStragglers, inTime);
        break;
    case kASGStateAttack:
        theFailed = !this->TestCondition(kASGConditionHasTargets, inTime);
        if (theFailed)
            this->SetState(kASGStateIdle, inTime);
        if (!theFailed && this->TestCondition(kASGConditionAwayFromTowns, inTime))
            this->SetState(kASGStateRetreatToTown, inTime);
        break;
    case kASGStateAttackWhileBoarding:
        theFailed = !this->TestCondition(kASGConditionHasTransport, inTime);
        if (theFailed)
            this->SetState(kASGStateIdle, inTime);
        if (!theFailed) {
            theFailed = !this->TestCondition(kASGConditionHasTargets, inTime);
            if (theFailed)
                this->SetState(kASGStateBoardTransport, inTime);
            if (!theFailed) {
                theFailed = !this->TestCondition(kASGConditionTaskValid, inTime);
                if (theFailed)
                    this->SetState(kASGStateIdle, inTime);
                if (!theFailed && this->TestCondition(4009, inTime))
                    this->SetState(kASGStateBoardTransport, inTime);
            }
        }
        break;
    case kASGStateRetreatToTown:
        if (this->TestCondition(kASGConditionAllUnitsSettled, inTime))
            this->SetState(kASGStateIdle, inTime);
        break;
    case 6000:
        if (this->TestCondition(kASGConditionHasTargets, inTime))
            this->SetState(kASGStateAttack, inTime);
        else if (!this->TestCondition(3005, inTime))
            this->SetState(kASGStateIdle, inTime);
        break;
    case kASGStateHoldAtTask:
        if (this->TestCondition(kASGConditionHasTargets, inTime))
            this->SetState(kASGStateAttack, inTime);
        else {
            theFailed = !this->TestCondition(kASGConditionTaskValid, inTime);
            if (theFailed)
                this->SetState(kASGStateIdle, inTime);
            if (!theFailed && this->TestCondition(kASGConditionShouldRefreshTask, inTime))
                this->SetState(kASGStateRepositionAtTask, inTime);
        }
        break;
    case kASGStateFollowTaskUnit:
        if (this->TestCondition(kASGConditionHasTargets, inTime))
            this->SetState(kASGStateAttack, inTime);
        else if (this->TestCondition(3004, inTime))
            this->SetState(kASGStateFollowTaskUnit, inTime);
        else {
            theFailed = !this->TestCondition(kASGConditionTaskValid, inTime);
            if (theFailed)
                this->SetState(kASGStateIdle, inTime);
            if (!theFailed && this->TestCondition(kASGConditionTaskInOtherSubSector, inTime))
                this->SetState(kASGStatePrepareTravel, inTime);
        }
        break;
    case kASGStateReturnToRallyPoint:
        if (this->TestCondition(kASGConditionAtRallyPoint, inTime))
            this->SetState(kASGStateRemove, inTime);
        else if (this->TestCondition(kASGConditionHasTargets, inTime))
            this->SetState(kASGStateAttack, inTime);
        break;
    case kASGStateRepositionAtTask:
        if (this->TestCondition(kASGConditionTaskValid, inTime))
            this->SetState(kASGStateHoldAtTask, inTime);
        else
            this->SetState(kASGStateIdle, inTime);
        break;
    case kASGStateLoad:
        theFailed = !this->TestCondition(kASGConditionHasTransport, inTime);
        if (theFailed)
            this->SetState(kASGStateIdle, inTime);
        if (!theFailed) {
            if (this->TestCondition(kASGConditionTransportRequestFilled, inTime))
                this->SetState(kASGStateBoardTransport, inTime);
            else if (this->TestCondition(kASGConditionHasTargets, inTime))
                this->SetState(kASGStateAttackWhileBoarding, inTime);
            else if (!this->TestCondition(kASGConditionTaskValid, inTime))
                this->SetState(kASGStateIdle, inTime);
        }
        break;
    case kASGStateNoPath:
        if (!this->TestCondition(kASGConditionOffMap, inTime))
            this->SetState(kASGStateIdle, inTime);
        break;
    case kASGStateSplitStragglers:
        if (!this->TestCondition(kASGConditionAllUnloaded, inTime))
            this->SetState(kASGStateInTransit, inTime);
        else
            this->SetState(kASGStateIdle, inTime);
        break;
    case kASGStateHoldTerritory:
        if (this->TestCondition(kASGConditionHasTargets, inTime))
            this->SetState(kASGStateAttack, inTime);
        else if (this->TestCondition(kASGConditionTerritoryTaskInvalid, inTime))
            this->SetState(kASGStateIdle, inTime);
        break;
    default:
        this->SetState(kASGStateIdle, inTime);
        break;
    }
    this->mNextUpdateTime = inTime + theDelay;
}

// FUNCTION: EXE 0x449387
// True when the group may advance (no next sub-sector, already in it, no blocking wall/gate on the way); else
// moves the next sub-sector tile onto the blocker and returns whether it is unseen or beyond the scan area
// NEAR: the wall handling follows the original, including its one-past weight store below
bool EECPASGLand::AdvanceSubSector()
{
    std::vector<std::pair<EEAIPathSubSector*, EEAIPathSubSector*> > theLinks;
    EECPASGFlags theFlags;
    EEAIAStarBounds theBounds;
    EETileLocation theCenter;
    bool theResult = true;
    if (this->mNextSubSectorTile.GetX() == 0 && this->mNextSubSectorTile.GetY() == 0) {
        if (this->mPath.empty())
            return theResult;
        this->mNextSubSectorTile = this->mPath.front().mTile;
    }
    EEAIPathSubSector* theTarget = gContinentManager.GetPathSubSectorAt(this->mNextSubSectorTile.GetX(), this->mNextSubSectorTile.GetY());
    if (theTarget == NULL)
        return theResult;
    this->FindPosition(theCenter, true, true);
    EEAIPathSubSector* theCurrent = gContinentManager.GetPathSubSectorAt(theCenter.GetX(), theCenter.GetY());
    if (theCurrent == NULL)
        return theResult;
    theResult = theCurrent == theTarget;
    if (theResult)
        return theResult;
    if (!this->IsGateBlocked(theTarget)) {
        memcpy(theFlags.mFlags, this->mFlags.mFlags, sizeof(theFlags.mFlags));
        theFlags.mPlayerID = this->mFlags.mPlayerID;
        theFlags.mFlags[kAITileWall] = false;
        theFlags.mFlags[kAITileGate] = false;
        if (theCurrent->IsNeighbor(theTarget)) {
            theResult = theCurrent->AreLinksBlocked(theTarget, theFlags);
            if (theResult)
                return theResult;
            theResult = true;
            theCurrent->GetLinks(theTarget->mID, theLinks);
            for (long i = theLinks.size() - 1; i >= 0 && theResult; --i) {
                if (this->IsGateBlocked(theLinks[i].first) &&
                    (this->mFlags.mFlags[theLinks[i].second->mKind] != true || theLinks[i].second->mKind == 13 ||
                     theLinks[i].second->mKind == 14 || theLinks[i].second->mKind == 12)) {
                    this->mNextSubSectorTile = theLinks[i].first->mLocation;
                    theResult = false;
                } else if (this->IsGateBlocked(theLinks[i].second) &&
                    (this->mFlags.mFlags[theLinks[i].first->mKind] != true || theLinks[i].first->mKind == 13 ||
                     theLinks[i].first->mKind == 14 || theLinks[i].first->mKind == 12)) {
                    this->mNextSubSectorTile = theLinks[i].second->mLocation;
                    theResult = false;
                }
            }
        } else {
            EECPASGWeights theWeights;
            theWeights.Fill(0.0f);
            theWeights.Fill(1.0f);
            if (this->mFlags.mFlags[kAITileWater] == true)
                theWeights.mValues[1] = 0.0f;   // the water weight (GetPathCosts); was the tile kind 2 used as the slot
            // The space weight is slot 10, as in GetPathCosts (the tile kind 15 indexed one past the 15 weights).
            if (this->mFlags.mFlags[kAITileSpace] == true)
                theWeights.mValues[10] = 0.0f;
            EEAIGPGModifierLandWallAvoider* theModifier = new EEAIGPGModifierLandWallAvoider(this, theWeights);
            EETileCoordinate theFromTile(theCenter.GetX(), theCenter.GetY());
            EETileCoordinate theToTile(theTarget->mLocation.GetX(), theTarget->mLocation.GetY());
            EEAutoPathRequest* theRequest = this->NewPathRequest(theFromTile, theToTile, 10, theModifier);
            if (theRequest == NULL || !theRequest->mIsDone || theRequest->mPath.empty()) {
                theResult = true;
            } else {
                gContinentManager.GetPathSubSectorAt(theRequest->mPath.back().GetX(),
                                                   theRequest->mPath.back().GetY());
                theResult = true;
                EEAIPathSubSector* thePrevious = theCurrent;
                for (long j = 0; j < (long)theRequest->mPath.size() && theResult == true; ++j) {
                    EEAIPathSubSector* theNext = gContinentManager.GetPathSubSectorAt(
                        theRequest->mPath[j].GetX(), theRequest->mPath[j].GetY());
                    theResult = !this->IsGateBlocked(theNext);
                    if (!theResult) {
                        this->mNextSubSectorTile = theNext->mLocation;
                    } else {
                        theLinks.clear();
                        if (thePrevious->IsNeighbor(theNext) && thePrevious != theNext) {
                            thePrevious->GetLinks(theNext->mID, theLinks);
                            for (long k = theLinks.size() - 1; k >= 0 && theResult; --k) {
                                if (this->IsGateBlocked(theLinks[k].first) &&
                                    (this->mFlags.mFlags[theLinks[k].second->mKind] != true || theLinks[k].second->mKind == 13 ||
                                     theLinks[k].second->mKind == 14 || theLinks[k].second->mKind == 12)) {
                                    this->mNextSubSectorTile = theLinks[k].first->mLocation;
                                    theResult = false;
                                } else if (this->IsGateBlocked(theLinks[k].second) &&
                                    (this->mFlags.mFlags[theLinks[k].first->mKind] != true || theLinks[k].first->mKind == 13 ||
                                     theLinks[k].first->mKind == 14 || theLinks[k].first->mKind == 12)) {
                                    this->mNextSubSectorTile = theLinks[k].second->mLocation;
                                    theResult = false;
                                }
                            }
                        }
                    }
                    thePrevious = theNext;
                }
            }
            if (theRequest != NULL) {
                theRequest->Dereference();
                theModifier->Dereference();
            }
        }
        if (theResult)
            return theResult;
    }
    EETile* theTile = gWorld.mMap->GetTileAtLocation(this->mNextSubSectorTile);
    if (theTile == NULL || !theTile->IsSeenBy(this->mPlayer))
        return true;
    theBounds = this->mArea;
    long theMargin = this->mScanRange;
    if (theMargin >= 20)
        theMargin = 20;
    theBounds.Grow(theMargin, true);
    return !theBounds.Contains(this->mNextSubSectorTile.GetX(), this->mNextSubSectorTile.GetY());
}

// FUNCTION: EXE 0x449b21
// Returns true when the player has explored the tile (visibility above 1) or sees the whole map
bool EETile::IsSeenBy(EEPlayer* inPlayer)
{
    return this->GetVisibility(inPlayer->mPlayerID) > 1 || inPlayer->mSeesAll;
}

// FUNCTION: EXE 0x449b5d
// At most every 5 s: true when the next sub-sector holds a wall/gate blocking the path to the next open
// waypoint (or the task); between checks, true when a next sub-sector tile is set
// NEAR: the wall handling follows the original, including its one-past weight store below
bool EECPASGLand::CheckNextSubSector(long inTime)
{
    std::vector<std::pair<EEAIPathSubSector*, EEAIPathSubSector*> > theLinks;
    EECPASGFlags theUnusedFlags;
    EETileLocation theCenter;
    EETileLocation theTo;
    bool theResult = this->mNextSubSectorTile.GetX() != 0 || this->mNextSubSectorTile.GetY() != 0;
    unsigned long theLimit = min((unsigned long)this->mPath.size(), 3UL);
    if (theResult && (unsigned long)inTime > (unsigned long)this->mNextSubSectorCheckTime) {
        EEAIPathSubSector* theTarget = NULL;
        for (unsigned long i = 0; i < theLimit; ++i) {
            EEAIPathSubSector* theSector = gContinentManager.GetPathSubSectorAt(this->mPath[i].mTile.GetX(), this->mPath[i].mTile.GetY());
            if (theSector == NULL || this->IsGateBlocked(theSector))
                continue;
            theTo = this->mPath[i].mTile;
            theTarget = theSector;
            break;
        }
        if (theTarget == NULL) {
            theTo = this->mTask.mLocation.mTile;
            theTarget = gContinentManager.GetPathSubSectorAt(theTo.GetX(), theTo.GetY());
        }
        if (theTarget != NULL) {
            this->FindPosition(theCenter, true, true);
            EEAIPathSubSector* thePrevious = gContinentManager.GetPathSubSectorAt(theCenter.GetX(), theCenter.GetY());
            if (thePrevious != NULL) {
                theResult = thePrevious != theTarget;
                if (theResult) {
                    EECPASGWeights theWeights;
                    theWeights.Fill(0.0f);
                    theWeights.Fill(1.0f);
                    if (this->mFlags.mFlags[kAITileWater] == true)
                        theWeights.mValues[1] = 0.0f;   // the water weight (GetPathCosts); was the tile kind 2 used as the slot
                    // The space weight is slot 10, as in GetPathCosts (the tile kind 15 indexed one past the 15 weights).
                    if (this->mFlags.mFlags[kAITileSpace] == true)
                        theWeights.mValues[10] = 0.0f;
                    EEAIGPGModifierLandWallAvoider* theModifier = new EEAIGPGModifierLandWallAvoider(this, theWeights);
                    EETileCoordinate theFromTile(theCenter.GetX(), theCenter.GetY());
                    EETileCoordinate theToTile(theTo.GetX(), theTo.GetY());
                    EEAutoPathRequest* theRequest = this->NewPathRequest(theFromTile, theToTile, 12, theModifier);
                    if (theRequest != NULL) {
                        if (theRequest->mIsDone && !theRequest->mPath.empty()) {
                            gContinentManager.GetPathSubSectorAt(theRequest->mPath.back().GetX(),
                                                               theRequest->mPath.back().GetY());
                            theResult = false;
                            for (long j = 0; j < (long)theRequest->mPath.size() && !theResult; ++j) {
                                EEAIPathSubSector* theNext = gContinentManager.GetPathSubSectorAt(
                                    theRequest->mPath[j].GetX(), theRequest->mPath[j].GetY());
                                theResult = this->IsGateBlocked(theNext);
                                if (theResult)
                                    theResult = gContinentManager.GetPathSubSectorAt(this->mNextSubSectorTile.GetX(), this->mNextSubSectorTile.GetY()) == theNext;
                                if (!theResult) {
                                    theLinks.clear();
                                    if (thePrevious->IsNeighbor(theNext) && thePrevious != theNext) {
                                        thePrevious->GetLinks(theNext->mID, theLinks);
                                        for (long k = theLinks.size() - 1; k >= 0 && !theResult; --k) {
                                            if (this->IsGateBlocked(theLinks[k].first)) {
                                                theResult = gContinentManager.GetPathSubSectorAt(this->mNextSubSectorTile.GetX(), this->mNextSubSectorTile.GetY()) == theLinks[k].first;
                                                if (theResult && this->mFlags.mFlags[theLinks[k].second->mKind] == true &&
                                                    theLinks[k].second->mKind != 13 && theLinks[k].second->mKind != 14 && theLinks[k].second->mKind != 12)
                                                    theResult = false;
                                            }
                                            if (theResult || !this->IsGateBlocked(theLinks[k].second))
                                                continue;
                                            theResult = gContinentManager.GetPathSubSectorAt(this->mNextSubSectorTile.GetX(), this->mNextSubSectorTile.GetY()) == theLinks[k].second;
                                            if (theResult && this->mFlags.mFlags[theLinks[k].first->mKind] == true &&
                                                theLinks[k].first->mKind != 13 && theLinks[k].first->mKind != 14 && theLinks[k].first->mKind != 12)
                                                theResult = false;
                                        }
                                    }
                                }
                                thePrevious = theNext;
                            }
                        }
                        theRequest->Dereference();
                        theModifier->Dereference();
                    }
                }
            }
        }
        this->mNextSubSectorCheckTime = (unsigned long)inTime + 5000;
    }
    return theResult;
}

// FUNCTION: EXE 0x44a0db
// Returns true when the tile is on the map, not deep water, cliff or space, holds no blocking or collidable
// unit and is not avoided
// NEAR: inlining and register allocation differ from the original
bool EECPASGLand::CanTransportLandAt(const EETileLocation& inTile)
{
    bool theResult;
    EETile* theTile = gWorld.GetMap()->GetTileAtLocation(inTile);
    if (theTile == NULL || theTile->mTerrainType == kTerrainDeepWater || theTile->mTerrainType == kTerrainCliff || theTile->mTerrainType == kTerrainSpace) {
        theResult = false;
    } else {
        theResult = true;
        for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL; theNode = theNode->mNext) {
            EEUnitClass* theClass = theNode->mUnit->GetUnitClass();
            if (theClass->mBlocksTile || theClass->mIsCollidable) {
                theResult = false;
                break;
            }
        }
    }
    if (theResult) {
        for (long i = this->mAvoidedTiles.size() - 1; i >= 0 && theResult == true; i--) {
            if (this->mAvoidedTiles[i].mTile.mX == inTile.mX && this->mAvoidedTiles[i].mTile.mY == inTile.mY)
                theResult = false;
            else
                theResult = true;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x44a186
// Loads the sub-group, pickup/unload locations, transport ID and stage, path flag and transport tiles (newer
// saves add the avoided tiles); returns the first read error; a pending path resumes travel
long EECPASGLand::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    unsigned long theID = 0;
    unsigned long theCount = 0;
    EECPILocation theLocation;
    bool theFlag = false;
    long theResult;
    if (inMajorVersion == 0 && inMinorVersion < 0xe3) {
        theResult = EECPAttackSubGroup::Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult == 0) {
            theResult = this->mPickupLocation.Load(inMajorVersion, inMinorVersion, inFile);
            if (theResult == 0)
                theResult = this->mUnloadLocation.Load(inMajorVersion, inMinorVersion, inFile);
        }
        theID = 0;
        if (theResult == 0) {
            theResult = inFile.Read(theID);
            this->mTransport = reinterpret_cast<EECPASGTransport*>(theID);
            if (theResult == 0) {
                theResult = inFile.Read(this->mTransportStage);
                if (theResult == 0) {
                    theResult = inFile.Read(theFlag);
                    if (theResult == 0) {
                        theResult = this->mEmbarkTile.Load(inFile);
                        if (theResult == 0)
                            theResult = this->mLandingTile.Load(inFile);
                    }
                }
            }
        }
    } else {
        theResult = EECPAttackSubGroup::Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult == 0) {
            theResult = this->mPickupLocation.Load(inMajorVersion, inMinorVersion, inFile);
            if (theResult == 0)
                theResult = this->mUnloadLocation.Load(inMajorVersion, inMinorVersion, inFile);
        }
        theID = 0;
        if (theResult == 0) {
            theResult = inFile.Read(theID);
            this->mTransport = reinterpret_cast<EECPASGTransport*>(theID);
            if (theResult == 0) {
                theResult = inFile.Read(this->mTransportStage);
                if (theResult == 0) {
                    theResult = inFile.Read(theFlag);
                    if (theResult == 0) {
                        theResult = this->mEmbarkTile.Load(inFile);
                        if (theResult == 0)
                            theResult = this->mLandingTile.Load(inFile);
                    }
                }
            }
        }
        // the avoided tiles are read only after the fields before them read without error (their result used
        // to replace an earlier error, and the list was resized from a count read after a failure)
        theCount = 0;
        if (theResult == 0) {
            theResult = inFile.Read(theCount);
            if (theResult == 0)
                this->mAvoidedTiles.resize(theCount);
            for (unsigned long i = 0; i < theCount && theResult == 0; i++) {
                theResult = this->mAvoidedTiles[i].mTile.Load(inFile);
                if (theResult == 0)
                    theResult = inFile.Read(this->mAvoidedTiles[i].mTime);
            }
        }
    }
    if (theFlag && !this->mUnits.empty())
        this->mState = kASGStatePrepareTravel;
    return theResult;
}

// FUNCTION: EXE 0x44a3a9
// Shares the units outside transports among the transports by capacity and gives each a load goal; the rest
// walk to the last rendezvous and are split off; returns whether a load goal was given
bool EECPASGLand::BoardTransports(std::vector<EEComplexUnit*>* inTransports)
{
    std::vector<EECPAILoadGroup> theGroups;
    std::vector<EEComplexUnit*> theAvailable;
    std::vector<EEComplexUnit*> theRejected;
    EECPAGOrder theOrder;
    bool theResult = false;
    U2DPoint<long> theRendezvous;
    if (this->mTransport != NULL && !inTransports->empty()) {
        for (long i = this->mUnits.size() - 1; i >= 0; i--) {
            if (this->mUnits[i]->IsDone())
                continue;
            EEComplexUnit* theUnit = this->mUnits[i]->mUnit;
            if (theUnit->mContainer == NULL && theUnit->mTileX != -10000)
                theAvailable.push_back(theUnit);
        }
        EEComplexUnit* theTransport = inTransports->back();
        dynamic_cast<EEUCTransport*>(theTransport->GetUnitClass())->FilterLoadable(&theAvailable, theTransport, &theRejected);
        for (i = inTransports->size() - 1; i >= 0; i--) {
            theGroups.push_back(EECPAILoadGroup());
            EECPAILoadGroup& theCurrent = theGroups.back();
            theCurrent.mTransport = (*inTransports)[i];
            theCurrent.ResetLoad();
            while (!theAvailable.empty()) {
                if (theAvailable.back()->mContainer == NULL) {
                    if (theAvailable.back()->GetUnitClass()->mWeight + theCurrent.mLoad > theCurrent.mTransport->GetUnitClass()->mTransportCapacity)
                        break;
                    theCurrent.mUnits.push_back(theAvailable.back());
                    theCurrent.mLoad += theAvailable.back()->GetUnitClass()->mWeight;
                }
                theAvailable.pop_back();
            }
            if (theAvailable.empty())
                break;
        }
        while (!theAvailable.empty()) {
            theRejected.push_back(theAvailable.back());
            theAvailable.pop_back();
        }
    }
    for (long i = theGroups.size() - 1; i >= 0; i--) {
        EECPAILoadGroup& theCurrent = theGroups[i];
        if (!theCurrent.mUnits.empty()) {
            theResult = true;
            EEUGLoadTransport* theGoal = new EEUGLoadTransport(theCurrent.mTransport, theCurrent.mUnits, true);
            // a refused goal is deleted only after its tile and the units' copies are taken from it
            bool theRefused = !this->GiveGoal(theCurrent.mTransport, theGoal, 0, kActionPrepareToMove);
            theRendezvous = theGoal->mUnitTile;
            for (std::vector<EEComplexUnit*>::iterator theUnit = theCurrent.mUnits.begin(); theUnit != theCurrent.mUnits.end(); ++theUnit) {
                EEUnitGoal* theCopy = theGoal->Clone();
                if (!this->GiveGoal(*theUnit, theCopy, 0, kActionPrepareToMove) && theCopy != NULL)
                    delete theCopy;
            }
            if (theRefused && theGoal != NULL)
                delete theGoal;
        } else {
            theRendezvous.mX = theCurrent.mTransport->mTileX;
            theRendezvous.mY = theCurrent.mTransport->mTileY;
        }
    }
    for (std::vector<EEComplexUnit*>::iterator theUnit = theRejected.begin(); theUnit != theRejected.end(); ++theUnit) {
        EEUnitGoal* theGoal = new EEUGLocation(GE2DPoint(theRendezvous.GetX(), theRendezvous.GetY()), true, true, false, 0.0f, false, false);
        if (!this->GiveGoal(*theUnit, theGoal, 0, kActionPrepareToMove) && theGoal != NULL)
            delete theGoal;
    }
    if (!theRejected.empty()) {
        theOrder.mType = kAGOrderSplitSubGroup;
        theOrder.SetSubGroup(this);
        theOrder.mIsActive = true;
        this->mGroup->mOrders.push_back(theOrder);
    }
    if (theResult)
        this->mTransportStage = 1;
    return theResult;
}

// FUNCTION: EXE 0x44a81c
// Drops the transport and sets the new destination
void EECPASGLand::SetDestination(const EECPILocation& inLocation)
{
    this->DropTransport();
    EECPAttackSubGroup::SetDestination(inLocation);
}

// FUNCTION: EXE 0x44a836
// Sets the unload point to the free landing tile nearest the transport (on an adjacent land continent, or its
// own when on land) and redirects the transport's cargo; returns whether one was found
bool EECPASGLand::ChooseNearestUnloadPoint(bool inWater)
{
    EECPAttackSubGroup* theTransport = dynamic_cast<EECPAttackSubGroup*>(this->mTransport);
    std::vector<unsigned short> theIDs;
    std::pair<EETileLocation, long> theBest(EETileLocation(0, 0), 0);
    EETileLocation theCandidate(0, 0);
    EETileLocation theCenter(0, 0);
    bool theResult = false;
    if (theTransport != NULL && theTransport->FindPosition(theCenter, true, true)) {
        EEAIContinent* theContinent;
        if (theCenter.GetX() < gMapWidth && theCenter.GetY() < gMapHeight)
            theContinent = gContinentManager.GetContinentAt(theCenter.GetX(), theCenter.GetY());
        else
            theContinent = NULL;
        theBest.second = 0;
        if (theContinent != NULL) {
            if (theContinent->mCategory != kContinentCategoryLand) {
                gContinentManager.GetAdjacentIDs(theIDs, theContinent->mID);
                for (long i = theIDs.size() - 1; i >= 0; i--) {
                    EEAIContinent* theNeighbor = gContinentManager.GetContinentByID(theIDs[i]);
                    if (theNeighbor != NULL && theNeighbor->GetCategory() == kContinentCategoryLand &&
                        this->FindCrossingTile(theNeighbor, theContinent, theCenter, theCandidate, inWater) &&
                        (theBest.second == 0 || theCandidate.DistanceToSquared(theCenter) < theBest.second)) {
                        theBest.first = theCandidate;
                        theBest.second = theCandidate.DistanceToSquared(theCenter);
                    }
                }
                if (theBest.second != 0) {
                    EECPILocation theLocation(theBest.first);
                    this->mUnloadLocation = theLocation;
                    theResult = true;
                }
            } else if (this->FindCrossingTile(theContinent, theContinent, theCenter, theCandidate, inWater)) {
                EECPILocation theLocation(theCandidate);
                this->mUnloadLocation = theLocation;
                theResult = true;
            }
            if (theResult)
                theTransport->SetState(kASGStateRedirectCargo, gPhysics->mGameTime);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x44aa22
// When the unload tile is unusable, picks an air drop on the task continent (a free random point or an enemy
// farm, else the nearest free shore or closest point; else retargets the task); returns whether it changed
bool EECPASGLand::ChooseAirUnloadPoint()
{
    std::vector<std::pair<long, float> > theChoices;
    std::vector<U2DPoint<long> > theTiles;
    std::vector<EEComplexUnit*> theUnits;
    EETileLocation& theTile = this->mUnloadLocation.mTile;
    EETileLocation theOld = theTile;
    EETileLocation theCenter(0, 0);
    EETileLocation thePoint(0, 0);
    EECPAttackSubGroup* theTransport = dynamic_cast<EECPAttackSubGroup*>(this->mTransport);
    EETileLocation& theTaskTile = this->mTask.mLocation.mTile;
    EEAIContinent* theContinent = gContinentManager.GetContinentAt(theTaskTile.mX, this->mTask.mLocation.mTile.mY);
    bool theFound = false;
    bool thePending = true;
    if (theTransport != NULL) {
        bool theInvalid = gWorld.GetMap()->GetTileAtLocation(theTile) == NULL || !this->CanTransportLandAt(theTile);
        if (this->FindPosition(theCenter, true, false) && theInvalid && theContinent != NULL) {
            theChoices.reserve(8);
            theChoices.push_back(std::pair<long, float>(0, 0.2f));
            theChoices.push_back(std::pair<long, float>(4, 0.2f));
            EEAIRandomShuffle(theChoices.begin(), theChoices.end());
            EECPIntelligencePlayerFile* theFile = this->GetTargetPlayerFile();
            if (theFile != NULL && theChoices.size() != 0) {
                do {
                    if (thePending != true)
                        break;
                    float theRandom = gPhysics->GetRandom();
                    switch (theChoices.back().first) {
                    case 4:
                        if (theContinent->mCategory != kContinentCategoryWater && theContinent->mCategory != kContinentCategorySpace) {
                            for (long i = 15; i >= 0; i--) {
                                thePoint = theContinent->GetRandomPoint();
                                if (gWorld.GetMap()->GetTile(thePoint.mX, thePoint.mY) == NULL || !this->CanTransportLandAt(thePoint))
                                    continue;
                                thePending = false;
                                this->mUnloadLocation.mSerial = theContinent->mSerial;
                                this->mUnloadLocation.mType = kAIAreaContinent;
                                theTile = thePoint;
                                break;
                            }
                        }
                        break;
                    case 0:
                        if (theRandom <= theChoices.back().second) {
                            theFile->GetAllUnits(theContinent->mID, theUnits);
                            for (long i = theUnits.size() - 1; i >= 0; i--) {
                                if (theUnits[i]->mUnitClass->mUnitType != kUnitTypeFarm && theUnits[i]->mUnitClass->mUnitType != kUnitTypeRoboticFarm)
                                    continue;
                                this->mUnloadLocation.mSerial = theContinent->mSerial;
                                this->mUnloadLocation.mType = kAIAreaContinent;
                                theTile.SetXY(theUnits[i]->mTileX, theUnits[i]->mTileY);
                                thePending = false;
                                break;
                            }
                        }
                        break;
                    }
                    theChoices.pop_back();
                } while (theChoices.size() != 0);
            }
            if (thePending) {
                theTiles = theContinent->mShoreTiles;
                std::sort(theTiles.begin(), theTiles.end(), EECPASGLandTileFarther(theCenter));
                while (!theTiles.empty() && !this->CanTransportLandAt(theTiles.back()))
                    theTiles.pop_back();
                EECPILocation& theLocation = this->mUnloadLocation;
                theLocation.mSerial = theContinent->mSerial;
                theLocation.mType = kAIAreaContinent;
                if (!theTiles.empty()) {
                    theTile = theTiles.back();
                } else {
                    theTile = theContinent->GetClosestPoint(theCenter);
                    if (!this->CanTransportLandAt(theTile)) {
                        for (long i = 15; i >= 0; i--) {
                            if (theFound)
                                break;
                            theTile = theContinent->GetClosestPoint(theCenter);
                            theFound = this->CanTransportLandAt(theTile);
                        }
                        if (!theFound) {
                            this->mTask.mIsActive = false;
                            EEAIContinent* theRandom = dynamic_cast<EEAIContinent*>(gContinentManager.GetRandomContinent(kContinentCategoryLand, gPhysics->mRandom));
                            this->mTask.mLocation = theRandom != NULL ? EECPILocation(theRandom->GetRandomPoint()) : this->mRallyPoint;
                            theLocation = EECPILocation(theTaskTile);
                        }
                    }
                }
            }
        }
    }
    return theTile.mX != theOld.mX || theTile.mY != theOld.mY;
}

// FUNCTION: EXE 0x44afce
// Picks the shore tile where the transport unloads: the nearest one when in danger, else a crossing on the target
// continent; an unusable tile calls cover and retargets the task; returns whether the tile changed
// The unit scan walks the transport's units (original 0x44b14f: GetNumUnits/GetUnitAt on [ebp-8]), not ours
bool EECPASGLand::ChooseShoreUnloadPoint(bool inWater)
{
    EETileLocation& theTile = this->mUnloadLocation.mTile;
    EETileLocation theOld = theTile;
    EETileLocation thePoint(0, 0);
    EETileLocation theCenter(0, 0);
    EETileLocation theClosest(0, 0);
    EECPAttackSubGroup* theTransport = dynamic_cast<EECPAttackSubGroup*>(this->mTransport);
    unsigned short theOwnID = this->GetMainContinent();
    unsigned short theTaskID = gContinentManager.GetContinentID(this->mTask.mLocation.mTile.GetX(), this->mTask.mLocation.mTile.GetY());
    bool theInvalid = false;
    bool theMove = false;
    if (theTransport != NULL) {
        if (gWorld.GetMap()->GetTileAtLocation(theTile) == NULL)
            theInvalid = true;
        if (this->FindPosition(theCenter, true, false)) {
            bool theContinue = true;
            if (!theInvalid) {
                bool theExamine = false;
                if (this->mLandingTile.mX != 0 || this->mLandingTile.mY != 0 || gPhysics->GetRandom() > 0.1f) {
                    theMove = this->GetDangerRatio(0) >= 0.75f;
                    if (!theMove)
                        theExamine = true;
                } else {
                    EEAIContinent* theTask = gContinentManager.mContinents[theTaskID];
                    if (theTask != NULL) {
                        U2DConvexHull<long>* theHull = &theTask->mHull;
                        if (!theHull->BoundedContains(theCenter)) {
                            theClosest = theHull->GetClosestOutlinePoint(theCenter);
                            if (theClosest.DistanceToSquared(theCenter) > 36)
                                theExamine = true;
                        }
                    }
                }
                if (theExamine) {
                    theContinue = false;
                    if (theTile.mX == theOld.mX && theTile.mY == theOld.mY) {
                        for (long i = theTransport->GetNumUnits() - 1; i >= 0; --i) {
                            EEComplexUnit* theUnit = theTransport->GetUnitAt(i)->mUnit;
                            if ((theUnit->mCommand != NULL ? theUnit->mCommand->mType : 0) == kActionIdle && theUnit->GetNumCargoUnits() != 0 &&
                                gPathManager.IsPathDone(theUnit) && !gPathManager.HasAutoPath(theUnit)) {
                                theContinue = true;
                                break;
                            }
                        }
                    }
                }
            }
            if (theContinue) {
                if (theMove) {
                    this->ChooseNearestUnloadPoint(inWater);
                } else {
                    EEAIContinent* theDestination = this->GetTargetContinent(theOwnID, theTaskID, inWater ? 10 : 3);
                    if (theDestination != NULL) {
                        thePoint = this->mLandingTile;
                        if (this->FindCrossingTile(theDestination, gContinentManager.GetContinentByID(theOwnID), thePoint, thePoint, inWater)) {
                            this->mUnloadLocation.mSerial = theDestination->mSerial;
                            this->mUnloadLocation.mType = kAIAreaContinent;
                            theTile = thePoint;
                        } else {
                            theTile = theDestination->GetClosestPoint(theOwnID == theTaskID ? this->mTask.mLocation.mTile : theCenter);
                        }
                        if (!this->CanTransportLandAt(theTile)) {
                            EETile* theMapTile = gWorld.GetMap()->GetTileAtLocation(theTile);
                            if (theMapTile != NULL) {
                                for (RTSTileUnitNode* theNode = theMapTile->mUnits; theNode != NULL; theNode = theNode->mNext) {
                                    if (theNode->mUnit->mUnitClass->mIsWall && theNode->mUnit->mPlayer != this->mPlayer &&
                                        this->mPlayer->mEnemies[theNode->mUnit->mPlayer->mPlayerID] != 0) {
                                        this->mPlayer->mCombatManager->CoverLanding(this);
                                        break;
                                    }
                                }
                            }
                            this->mTask.mIsActive = false;
                            EEAIContinent* theRandom = dynamic_cast<EEAIContinent*>(gContinentManager.GetRandomContinent(kContinentCategoryLand, gPhysics->mRandom));
                            this->mTask.mLocation = theRandom != NULL ? EECPILocation(theRandom->GetRandomPoint()) : this->mRallyPoint;
                            this->mUnloadLocation = EECPILocation(this->mTask.mLocation.mTile);
                        }
                    }
                }
            }
        } else {
            this->mUnloadLocation = this->mRallyPoint;
        }
    }
    return theTile.mX != theOld.mX || theTile.mY != theOld.mY;
}

// FUNCTION: EXE 0x44b3bc
// Picks the space transport's unload point with the shore rules in water mode
bool EECPASGLand::ChooseSpaceUnloadPoint()
{
    return this->ChooseShoreUnloadPoint(1);
}

// FUNCTION: EXE 0x44b3c7
// Requests a new route: opens the terrain the cost weights allow (water, space, cliff, forest, walls, gates),
// clears the old path and asks the path manager with a land route-planner modifier
void EECPASGLand::RequestPath(void* inFrom, void* inTo)
{
    EECPASGFlags theFlags;
    memcpy(theFlags.mFlags, this->mFlags.mFlags, sizeof(theFlags.mFlags));
    EECPASGWeights theWeights;
    theWeights.Fill(0.0f);
    this->GetPathCosts(inFrom, inTo, theWeights);
    theFlags.mFlags[kAITileWall] = true;
    theFlags.mFlags[kAITileGate] = true;
    if (theWeights.mValues[1] != 1.0f) {
        theFlags.mFlags[kAITileWater] = true;
        theFlags.mFlags[kAITileShallows] = true;
        theFlags.mFlags[kAITileShallowsShore] = true;
        theFlags.mFlags[kAITileShallowsNearShore] = true;
        theFlags.mFlags[kAITileShore] = true;
        theFlags.mFlags[kAITileNearShore] = true;
    }
    if (theWeights.mValues[10] != 1.0f)
        theFlags.mFlags[kAITileSpace] = true;
    if (theWeights.mValues[5] != 1.0f)
        theFlags.mFlags[kAITileCliff] = true;
    if (theWeights.mValues[0] != 1.0f)
        theFlags.mFlags[kAITileForest] = true;
    this->mPath.clear();
    this->CancelPath();
    EETileCoordinate* theFrom = static_cast<EETileCoordinate*>(inFrom);
    EETileCoordinate* theTo = static_cast<EETileCoordinate*>(inTo);
    EEAIGPGModifierLandAttackSubGroupRoutePlanner* theModifier = new EEAIGPGModifierLandAttackSubGroupRoutePlanner(
        this, gContinentManager.GetContinentID(theFrom->mX, theFrom->mY),
        gContinentManager.GetContinentID(theTo->mX, theTo->mY), theWeights.mValues[5] == 0.0f, theWeights);
    this->mPathRequest = gPathManager.RequestPath(this->mPlayer->GetPlayerID(), *theFrom, *theTo, &theFlags, 2, -1,
                                           theModifier, 0);
    ++this->mPlayer->mCombatManager->mAttackCommander->mUpdatesThisTick;
    theModifier->Dereference();
}

// FUNCTION: EXE 0x44b53d
// Drops land steps from the path (after a friendly gate, or up to 7 in a row for wonder or location attacks);
// at the first impassable continent, requests a transport, sets embark/landing tiles and cuts the path there
void EECPASGLand::PrunePath()
{
    typedef std::pair<EEAIPathSubSector*, EEAIPathSubSector*> SectorLink;
    std::vector<SectorLink> theLinks;
    EETileLocation theTile;
    EEAIPathSubSector* theSector = NULL;
    unsigned short theContinentID = this->GetMainContinent();
    bool thePassedGate = false;
    this->mEmbarkTile = EETileLocation();
    this->mLandingTile = EETileLocation();
    this->mNoTransport = false;
    EEAIContinent* theLastContinent = NULL;
    long theRemoved = 0;
    if (this->mPath.size() != 0)
        theLastContinent = gContinentManager.GetContinentAt(this->mPath.back().mTile.GetX(), this->mPath.back().mTile.GetY());
    for (long i = 0; i < this->mPath.size(); i++) {
        EEAIPathSubSector* thePrevious = theSector;
        theSector = gContinentManager.GetPathSubSectorAt(this->mPath[i].mTile.GetX(), this->mPath[i].mTile.GetY());
        if (theSector == NULL)
            continue;
        EEAIContinent* theContinent = gContinentManager.GetContinentAt(this->mPath[i].mTile.GetX(), this->mPath[i].mTile.GetY());
        long theType = theContinent->mCategory;
        if (theContinentID == theContinent->mID || theType == kContinentCategoryLand || theType == kContinentCategoryMapEdge) {
            long theKind = theSector->mKind;
            if ((theKind == kAITileLand || (theKind > kAITileMapEdge && theKind <= kAITileShore) || theKind == kAITileGate) &&
                i != 0 && i < this->mPath.size() - 2) {
                bool theCanRemove = false;
                if (theKind == kAITileGate) {
                    EEComplexUnit* theUnit = theSector->GetBlockingUnit();
                    if (theUnit != NULL && (theUnit->mPlayer == this->mPlayer ||
                        this->mPlayer->mEnemies[theUnit->mPlayer->mPlayerID] == 0)) {
                        theCanRemove = true;
                        thePassedGate = true;
                    }
                } else if (this->mTask.mMode == kAGModeWonder || this->mTask.mType == kAGTaskAttackLocation)
                    theCanRemove = theRemoved < 7;
                else if (thePassedGate) {
                    thePassedGate = false;
                    theCanRemove = true;
                }
                if (theCanRemove) {
                    if (thePrevious != NULL) {
                        theLinks.clear();
                        theSector->GetLinks(thePrevious->mID, theLinks);
                        for (long j = theLinks.size() - 1; j >= 0 && theCanRemove == true; j--) {
                            if (this->IsGatePassable(theLinks[j].first) || this->IsGatePassable(theLinks[j].second))
                                theCanRemove = false;
                        }
                    } else {
                        typedef std::multimap<unsigned short, SectorLink> LinkMap;
                        for (LinkMap::iterator it = theSector->mLinks.begin();
                             it != theSector->mLinks.end() && theCanRemove == true; it++) {
                            if (this->IsGatePassable(it->second.first) || this->IsGatePassable(it->second.second))
                                theCanRemove = false;
                        }
                    }
                    if (theCanRemove) {
                        this->mPath.erase(this->mPath.begin() + i, this->mPath.begin() + i + 1);
                        i--;
                        theRemoved++;
                        continue;
                    }
                }
                theRemoved = 0;
            }
        } else if ((theType != kContinentCategoryWater || !this->mFlags.mFlags[kAITileWater]) &&
                   (theType != kContinentCategorySpace || !this->mFlags.mFlags[kAITileSpace]) &&
                   (theType != kContinentCategoryCliff || !this->mFlags.mFlags[kAITileCliff])) {
            theTile = this->mPath[i].mTile;
            this->RequestTransport(theTile);
            if (this->mTransport == NULL)
                this->mTask.mIsActive = false;
            else {
                this->mEmbarkTile = theTile;
                this->mLandingTile = this->mPath.back().mTile;
                if (dynamic_cast<EECPASGAirTransport*>(this->mTransport) == NULL) {
                    for (long j = i; j < this->mPath.size(); j++) {
                        EEAIContinent* theNext = gContinentManager.GetContinentAt(this->mPath[j].mTile.GetX(),
                                                                               this->mPath[j].mTile.GetY());
                        if (theNext != theContinent && theNext->mCategory == kContinentCategoryLand &&
                            theNext->mID == theLastContinent->mID) {
                            this->mLandingTile = this->mPath[j].mTile;
                            break;
                        }
                    }
                } else {
                    for (long j = i; j < this->mPath.size(); j++) {
                        if (gContinentManager.GetContinentAt(this->mPath[j].mTile.GetX(), this->mPath[j].mTile.GetY()) == theLastContinent) {
                            this->mLandingTile = this->mPath[j].mTile;
                            break;
                        }
                    }
                }
            }
            this->mPath.erase(this->mPath.begin() + i, this->mPath.end());
            break;
        }
    }
    if (this->mTransport != NULL)
        this->ChooseUnloadPoint();
}

// FUNCTION: EXE 0x44b971
// Tells the transport to start loading
void EECPASGLand::StartTransportLoading()
{
    if (this->mTransport != NULL) {
        this->mTransport->StartLoading();
    }
}

// FUNCTION: EXE 0x44b981
// Sets the pickup tile and asks the group for a transport; with one, splits off the units it would not take
// and returns true, or deactivates the task when it would take none; without one, deactivates the task
bool EECPASGLand::RequestTransport(const EETileLocation& inTile)
{
    bool theResult = false;
    this->mPickupLocation.mTile = inTile;
    this->mTransport = this->mGroup->FindLandTransport(this);
    EECPAttackSubGroup* theTransport = dynamic_cast<EECPAttackSubGroup*>(this->mTransport);
    if (theTransport != NULL) {
        theTransport->Reference();
        long theCount = this->CountUnitsNotCarriable();
        if (theCount != this->mUnits.size()) {
            theResult = true;
            if (theCount != 0) {
                this->mSplitKind = 1;
                this->RequestSplit(false);
            }
        } else {
            this->mTask.mIsActive = false;
        }
    } else {
        this->mTask.mIsActive = false;
        this->mTransport = NULL;
    }
    return theResult;
}

// FUNCTION: EXE 0x44ba29
// Restores the sub-group and its locations after loading and turns the saved transport ID back into a
// referenced transport (NULL if it is gone)
long EECPASGLand::Restore(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    long theResult = EECPAttackSubGroup::Restore(inMajorVersion, inMinorVersion);
    if (theResult == 0) {
        theResult = this->mPickupLocation.Restore(inMajorVersion, inMinorVersion);
        if (theResult == 0)
            theResult = this->mUnloadLocation.Restore(inMajorVersion, inMinorVersion);
    }
    EECPAttackSubGroup* theSubGroup;
    if (this->mTransport != NULL) {
        EECPAttackCommander* theCommander = this->mGroup->mCommander;
        theSubGroup = theCommander->GetSubGroup(reinterpret_cast<unsigned long>(this->mTransport));
        if (theSubGroup != NULL) {
            theSubGroup->Reference();
            this->mTransport = dynamic_cast<EECPASGTransport*>(theSubGroup);
            return theResult;
        }
    }
    this->mTransport = NULL;
    return theResult;
}

// FUNCTION: EXE 0x44bab9
// Writes the sub-group, pickup/unload locations, transport ID and stage, path-pending flag, transport tiles
// and avoided tiles; returns the first write error
long EECPASGLand::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EECPAttackSubGroup* theTransport = dynamic_cast<EECPAttackSubGroup*>(this->mTransport);
    bool theFlag = false;
    unsigned long theID = 0;
    unsigned long theCount = 0;
    long theResult = EECPAttackSubGroup::Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult == 0) {
        theResult = this->mPickupLocation.Save(inMajorVersion, inMinorVersion, outFile);
        if (theResult == 0)
            theResult = this->mUnloadLocation.Save(inMajorVersion, inMinorVersion, outFile);
    }
    theID = theTransport != NULL ? theTransport->GetID() : 0;
    if (theResult == 0) {
        theResult = outFile.Write(theID);
        if (theResult == 0)
            theResult = outFile.Write(this->mTransportStage);
    }
    if (this->mPathRequest != NULL && this->IsPathPending())
        theFlag = true;
    else
        theFlag = false;
    if (theResult == 0)
        theResult = outFile.Write(theFlag);
    if (theResult == 0)
        theResult = this->mEmbarkTile.Save(outFile);
    if (theResult == 0)
        theResult = this->mLandingTile.Save(outFile);
    // the avoided tiles are written only after the fields before them (their result used to replace an earlier
    // error)
    theCount = this->mAvoidedTiles.size();
    if (theResult == 0)
        theResult = outFile.Write(theCount);
    for (unsigned long i = 0; i < theCount && theResult == 0; i++) {
        theResult = this->mAvoidedTiles[i].mTile.Save(outFile);
        if (theResult == 0)
            theResult = outFile.Write(this->mAvoidedTiles[i].mTime);
    }
    return theResult;
}

// FUNCTION: EXE 0x44bc40
// Returns true when there is no path request or it is not done yet
bool EECPAttackSubGroup::IsPathPending()
{
    return this->mPathRequest == NULL || !this->mPathRequest->mIsDone;
}

// FUNCTION: EXE 0x44bc57
// Replaces the transport (releasing the old one) and references the new one
void EECPASGLand::SetTransport(EECPASGTransport* inTransport)
{
    if (inTransport == this->mTransport)
        return;
    this->ReleaseTransport();
    this->mTransport = inTransport;
    if (inTransport != NULL)
        dynamic_cast<EECPAttackSubGroup*>(inTransport)->Reference();
}

// FUNCTION: EXE 0x44bc95
// For an active territory task with no path pending: true when within 8 tiles of it (or approaching with no
// landing tile), the task player has influence there and it is on the group's main continent
bool EECPASGLand::HasReachedTerritory(long inTime)
{
    EECPILocation theLocation;
    bool theResult = false;
    if (this->mTask.mType == kAGTaskTerritory && this->mTask.mIsActive) {
        if ((this->mPathRequest != NULL && !this->mPathRequest->mIsDone) || this->mPath.size() != 0)
            return false;
        {
            this->GetLocation(theLocation);
            EETileLocation& theTile = this->mTask.mLocation.mTile;
            theResult = theTile.DistanceToSquared(theLocation.mTile) <= 64;
            if (!theResult) {
                if (this->mState == kASGStateApproachTask && this->mLandingTile.GetX() == 0 && this->mLandingTile.GetY() == 0)
                    theResult = true;
            }
            if (theResult) {
                unsigned long thePlayerID = gWorld.mPlayers[this->mTask.GetPlayerID()]->mPlayerID;
                EEInfluenceTable* theTable = gWorld.mInfluenceTable;
                theResult = theTable->GetTileValue(thePlayerID, theTile) != 0;
                if (theResult) {
                    long theY = theTile.GetY();
                    long theX = theTile.GetX();
                    unsigned short theContinent = gContinentManager.GetContinentID(theX, theY);
                    theResult = this->GetMainContinent() == theContinent;
                }
            }
        }
    }
    return theResult;
}

static const float kASGLReadyRatio = 0.7f;   // 0x43ad68 (a unit-local const: not the pooled 0.7f)

// FUNCTION: EXE 0x44bd80
// Returns true when the group's units are settled (under 7 waypoints left, idle or waiting on an obstacle): at least
// 70% of them in groups of more than 4, otherwise all of them
bool EECPASGLand::AreUnitsSettled()
{
    unsigned long theReady = 0;
    for (long i = this->mUnits.size() - 1; i >= 0; --i) {
        EEComplexUnit* theUnit = this->mUnits[i]->mUnit;
        if (theUnit->mWaypoints.size() < 7 || theUnit->GetCommandType() == kActionIdle ||
            (theUnit->GetCommandType() >= kActionWaitForObstacleToClear && theUnit->GetCommandType() <= kActionPostCollisionNextWaypoint))
            ++theReady;
    }
    if (theReady != 0 && this->mUnits.size() > 4) {
        if ((float)theReady / (float)this->mUnits.size() >= kASGLReadyRatio)
            return true;
        return false;
    }
    return theReady == this->mUnits.size() ? true : false;
}

// FUNCTION: EXE 0x44be6e
// Checks task arrival as the base sub-group does and drops the transport once the group has arrived
bool EECPASGLand::CheckTaskArrival(long inTime)
{
    bool theResult = EECPAttackSubGroup::CheckTaskArrival(inTime);
    if (theResult) {
        this->DropTransport();
    }
    return theResult;
}

// FUNCTION: EXE 0x44be92
// Evaluates a state-machine condition for a land group (path ready, units aboard, transport status, unload point,
// units settled, next sub-sector ...); other conditions go to the base sub-group
bool EECPASGLand::TestCondition(long inEvent, long inTime)
{
    bool theResult = false;
    switch ((unsigned long)inEvent) {   // unsigned compares (ja), as the base 0x4604cf
    case kASGConditionPathReady:
        theResult = !this->IsPathPending();
        if (this->mPathRequest != NULL) {
            theResult = this->mPathRequest->mIsDone;
            if (theResult)
                this->ApplyPathResult();
        } else {
            this->mTask.mIsActive = false;
        }
        break;
    case 1005:
        theResult = gWorld.GetMap()->GetTileAtLocation(this->mNextWaypoint.mTile) != NULL;
        break;
    case kASGConditionAllUnitsSettled:
        theResult = this->AreAllUnitsSettled();
        break;
    case 1007:
        theResult = !this->AdvanceSubSector();
        break;
    case kASGConditionPathEmpty:
        theResult = this->mPath.empty();
        break;
    case kASGConditionShouldRefreshTask:
        theResult = this->ShouldRefreshTask();
        break;
    case kASGConditionAllAboard:
        theResult = this->CountUnitsAboard() == (long)this->mUnits.size();
        break;
    case kASGConditionAllUnloaded:
        theResult = this->CountUnitsAboard() == 0;
        break;
    case kASGConditionAnyAboard:
        theResult = this->CountUnitsAboard() != 0;
        break;
    case kASGConditionHasTransport:
        if (this->mTransport != NULL) {
            EECPAttackSubGroup* theTransport = dynamic_cast<EECPAttackSubGroup*>(this->mTransport);
            theResult = (theTransport->GetNumUnits() == 0 && theTransport->mRequest != NULL) || theTransport->CountFightingUnits() != 0;
        }
        break;
    case 4007:
        theResult = this->CanCarryAllUnits();
        break;
    case kASGConditionTransportRequestFilled:
        theResult = this->mTransport != NULL && dynamic_cast<EECPAttackSubGroup*>(this->mTransport)->IsRequestFilled();
        break;
    case 4009:
        theResult = this->HasIdleUnitToLoad();
        break;
    case kASGConditionUnloadPointChosen:
        theResult = this->ChooseUnloadPoint();
        break;
    case kASGConditionTransportUnitsWaiting:
        theResult = this->AreTransportUnitsWaiting();
        if (theResult) {
            this->mUnloadLocation.Clear();
            this->ChooseUnloadPoint();
        }
        break;
    case kASGConditionTargetsAtNextSubSector:
        theResult = this->HasTargetsAtNextSubSector();
        break;
    case kASGConditionOffMap:
        theResult = this->GetPosition().GetX() == -10000;
        break;
    case kASGConditionNoneCarriable:
        theResult = this->CountUnitsNotCarriable() == (long)this->mUnits.size();
        break;
    case kASGConditionUnitsSettled:
        theResult = this->AreUnitsSettled();
        break;
    case kASGConditionNextSubSectorValid:
        theResult = this->CheckNextSubSector(inTime);
        break;
    default:
        theResult = EECPAttackSubGroup::TestCondition(inEvent, inTime);
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x44c169
// Returns true when a unit of the group's transport carries cargo, has no waypoints and has no goal or is
// reacquiring it
// (mTransport read into a local before the dynamic_cast: the original loads ecx before the pushes)
bool EECPASGLand::AreTransportUnitsWaiting()
{
    EECPASGTransport* theASG = this->mTransport;
    EECPAttackSubGroup* theTransport = dynamic_cast<EECPAttackSubGroup*>(theASG);
    bool theFound = false;
    if (theTransport != NULL) {
        for (long i = theTransport->GetNumUnits() - 1; i >= 0 && !theFound; i--) {
            EEComplexUnit* theUnit = theTransport->GetUnitUnit(i);
            if (theUnit->GetNumCargoUnits() > 0 && theUnit->mWaypoints.empty()
                && (theUnit->mGoalState == NULL || (theUnit->mCommand ? theUnit->mCommand->mType : 0) == kActionReacquireGoal))
                theFound = true;
            else
                theFound = false;
        }
    }
    return theFound;
}

// FUNCTION: EXE 0x44c1f1
// Removes the last tile of a tile list (std::vector pop_back)

// FUNCTION: EXE 0x44c225
// Returns the last unit of a unit list (std::vector back)

// FUNCTION: EXE 0x44c22c
// Appends an order to an attack group's order queue (std::deque push_back)

// FUNCTION: EXE 0x44c2b8
// Sets all 15 path-cost weights to one value (EECPASGWeights::Fill)

// FUNCTION: EXE 0x44c2c8
// Keeps each of the 17 flags only where the other flag set has it too
void EECPASGFlags::And(const EECPASGFlags& inOther)
{
    for (long i = 16; i >= 0; i--) {
        this->mFlags[i] = this->mFlags[i] != 0 && inOther.mFlags[i] != 0;
    }
}

// FUNCTION: EXE 0x44c2ee
// Returns the path point at an index (std::deque operator[])

// FUNCTION: EXE 0x44c310
// Returns the last path point (std::deque back)

// FUNCTION: EXE 0x44c32f
// Erases a range of path points (std::deque erase)

// FUNCTION: EXE 0x44c3d8
// Resizes the avoided-tile list, filling new entries with defaults (std::vector resize)

// FUNCTION: EXE 0x44c3f3
// Appends an avoided tile to the list (std::vector push_back)

// FUNCTION: EXE 0x44c41f
// Empties the avoided-tile list (std::vector clear)

// FUNCTION: EXE 0x44c45a
// Destroys a map of sub-group unit lists keyed by continent id (std::map tree destructor)

// FUNCTION: EXE 0x44c485
// Returns the last transport load group (std::vector back)

// FUNCTION: EXE 0x44c48c
// Appends a transport load group to a list (std::vector push_back)

// FUNCTION: EXE 0x44c4b8
// Reserves room in a list of weighted choices (std::vector<std::pair<long, float> > reserve)

// FUNCTION: EXE 0x44c564
// Appends a weighted choice to a list (std::vector<std::pair<long, float> > push_back)

// FUNCTION: EXE 0x44c590
// Erases one tile from a tile list (std::vector erase)

// FUNCTION: EXE 0x44c5f4
// Grows the block map of an attack group's order queue (std::deque _Growmap)

// FUNCTION: EXE 0x44c700
// Returns the path point a path iterator refers to (std::deque iterator operator*)

// FUNCTION: EXE 0x44c734
// Resizes the avoided-tile list, filling new entries with a given value (std::vector resize)

// FUNCTION: EXE 0x44c7e8
// Inserts one avoided tile before a position (std::vector insert)

// FUNCTION: EXE 0x44c842
// Erases a range of tiles from a tile list (std::vector erase)

// FUNCTION: EXE 0x44c886
// Erases a range of entries from a map of sub-group unit lists (std::map erase)

// FUNCTION: EXE 0x44c8f7
// Inserts one transport load group before a position (std::vector insert)

// FUNCTION: EXE 0x44c951
// Destroys every transport load group in a list and frees its storage (std::vector _Tidy)

// FUNCTION: EXE 0x44c989
// Inserts n copies of an avoided tile before a position (std::vector insert)

// FUNCTION: EXE 0x44cc4f
// Frees a subtree of a map of sub-group unit lists (std::map _Erase)

// FUNCTION: EXE 0x44cc81
// Inserts n copies of a transport load group before a position (std::vector insert)

// FUNCTION: EXE 0x44cf67
// Copy-constructs a list of 32-bit values (std::vector<unsigned long> copy constructor, a shared body)

// FUNCTION: EXE 0x44cfae
// Copy-constructs n avoided tiles into raw storage (std::vector _Ufill)

// FUNCTION: EXE 0x44cfd5
// Copy-constructs n transport load groups into raw storage (std::vector _Ufill)

// FUNCTION: EXE 0x44cffc
// Collects the units in the bounds that pass the land target filter, dropping duplicates when a multi-tile unit
// was accepted and uniqueness was asked for; returns the count
// EECollectASGTargets<EECPALandTargetFilter> (ee/EECPAreaTargetCollector.h), emitted by CollectTargets above

// FUNCTION: EXE 0x44d1a5
// Sorts a tile list farthest tile first (std::sort with EECPASGLandTileFarther)

// FUNCTION: EXE 0x44d28b
// Copy-constructs a tile point (U2DPoint<long> copy constructor)

// FUNCTION: EXE 0x44d29e
// Shuffles a list of weighted choices with the AI random generator (EEAIRandomShuffle)

// FUNCTION: EXE 0x44d2c5
// Copies a range of path points backwards (std::copy_backward on deque iterators)

// FUNCTION: EXE 0x44d308
// Copies a range of path points (std::copy on deque iterators)

// FUNCTION: EXE 0x44d34b
// Copies a range of avoided tiles (std::copy)

// FUNCTION: EXE 0x44d37b
// Copy-constructs a range of avoided tiles into raw storage (std::vector _Ucopy)

// FUNCTION: EXE 0x44d3a3
// Assigns one value to a range of avoided tiles (std::fill)

// FUNCTION: EXE 0x44d3cb
// Copy-constructs a range of transport load groups into raw storage (std::vector _Ucopy)

// FUNCTION: EXE 0x44d3f3
// Assigns one value to a range of transport load groups (std::fill)

// FUNCTION: EXE 0x44d423
// Copies a range of transport load groups backwards (std::copy_backward)

// FUNCTION: EXE 0x44d45b
// Copy-constructs one avoided tile in place (std::_Construct)

// FUNCTION: EXE 0x44d47f
// Copy-constructs one transport load group in place (std::_Construct)

// FUNCTION: EXE 0x44d4a4
// Insertion-sorts a tile list farthest first (std::_Insertion_sort with EECPASGLandTileFarther)

// FUNCTION: EXE 0x44d4d7
// Quick-sorts a tile list farthest first, falling back to a heap sort past the depth limit (std::_Sort)

// FUNCTION: EXE 0x44d5e4
// Insertion-sorts the rest of a partly sorted tile list without a bounds check (std::_Unguarded_insertion_sort)

// FUNCTION: EXE 0x44d62e
// Shuffles a list of weighted choices with the AI random generator (std::random_shuffle)

// FUNCTION: EXE 0x44d660
// Insertion-sorts a tile list farthest first (std::_Insertion_sort_1)

// FUNCTION: EXE 0x44d72e
// Arranges a tile list into a heap with the farthest tile on top (std::make_heap)

// FUNCTION: EXE 0x44d772
// Sorts a tile heap farthest first (std::sort_heap)

// FUNCTION: EXE 0x44d7bf
// Partitions a tile list around a median-of-three pivot for the farthest-first sort (std::_Unguarded_partition)

// FUNCTION: EXE 0x44d914
// Moves one tile back to its farthest-first position without a bounds check (std::_Unguarded_insert)

// FUNCTION: EXE 0x44d95c
// Shuffles a list of weighted choices, swapping each with a random earlier entry (std::_Random_shuffle)

// FUNCTION: EXE 0x44d9b3
// Builds a farthest-first heap over a tile list (std::_Make_heap)

// FUNCTION: EXE 0x44da18
// Moves the top tile of a tile heap to the end (std::pop_heap)

// FUNCTION: EXE 0x44da4b
// Sifts a tile down a farthest-first heap (std::_Adjust_heap)

// FUNCTION: EXE 0x44daf6
// Pops the top tile of a tile heap (std::_Pop_heap_0)

// FUNCTION: EXE 0x44db6e
// Sifts a tile up a farthest-first heap (std::_Push_heap)

// FUNCTION: EXE 0x44dbe3
// Moves the top tile of a tile heap into a given slot and re-heaps the rest (std::_Pop_heap)

// ICF-FOLDED: EXE 0x67c0b4
// Returns false
// (no address of its own: folded into the shared `xor al, al; ret` body the vtable slot points at)
bool EECPASGLand::Virtual214()
{
    return false;
}

// ICF-FOLDED: EXE 0x67c0b4
// (no address of its own: folded into the shared `xor al, al; ret` body the vtable slot points at)
bool EECPASGLand::Virtual240()
{
    return false;
}

// ICF-FOLDED: EXE 0x67c0b4
// (no address of its own: folded into the shared `xor al, al; ret` body the vtable slot points at)
bool EECPASGLand::Virtual244()
{
    return false;
}
