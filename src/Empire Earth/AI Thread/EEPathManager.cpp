// C:\EEX\Empire Earth\AI Thread\EEPathManager.cpp (file name inferred: no __FILE__; RTTI classes
// EEAutoPathRequest (vtable 0x43c430), EEPathManager (vtable 0x43c438), UMemoryPool<EEPathManagerNode> (0x43c440),
// EEAIPathGoalModifierSpace/Sea (0x43c448/0x43c458), the three A* machines (ee/EEAIAStarMachine.h) and
// EEPathManagerNode (0x43c570)). 0xa5c31..0xabbaf (EEInfluenceTable.cpp ends at 0xa5c31, EETown.cpp starts at 0xabbaf).
// The template and inline COMDATs 0xa952e..0xabbaf (the machines' slots, the pool, the deques and maps) follow the
// manager's members.
#include <windows.h>
#include <stdio.h>
#include <typeinfo>
#include <math.h>
#include <vector>
#include <map>
#include <deque>
#include <algorithm>
#include <string.h>

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
#include "lle/FSFile.h"
#include "lle/U2DPoint.h"
#include "lle/U2DRectangle.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/USingleton.h"
#include "lle/UReferenceCounter.h"
#include "lle/UMemoryPool.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGLocation.h"
#include "ee/EEUGAttackUnit.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIAStarFastStorage.h"
#include "ee/EEAIAStarBoundedStorage.h"
#include "ee/EEAIAStarGraphStorage.h"
#include "ee/EEAIAStarGoalBase.h"
#include "ee/EEAIAStarGraphGoalBase.h"
#include "ee/EEAIPathGoalModifierBase.h"
#include "ee/EEAIBoundedPathGoalModifierBase.h"
#include "ee/EEAIAStarPathfindingGoalBase.h"
#include "ee/EEAIAStarPathfindingGoal.h"
#include "ee/EEAIAStarBoundedPathGoal.h"
#include "ee/EECPILocation.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EEAutoPathRequest.h"
#include "ee/EEAIAStarPathGraphGoal.h"
#include "ee/EEAIAStarMachine.h"
#include "ee/EEAIAStarMachineSteps.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIPathSector.h"
#include "ee/EEAIPathSubSector.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEMathInline.h"

#include "ee/EEPathManager.h"


// NEAR: the original copies the (uninitialised) allocator byte of its fifth argument
// FUNCTION: EXE 0x4a5c31
// Creates a player's graph path request between two tiles with the given sub-group flags, not yet done
EEAutoPathRequest::EEAutoPathRequest(long inRequestID, long inPlayerID, const EETileCoordinate& inFrom,
                                     const EETileCoordinate& inTo, const EECPASGFlags& inFlags)
    : mFlags(inFlags), mFrom(inFrom), mTo(inTo)
{
    this->mPlayerID = inPlayerID;
    this->mPathLength = 0;
    this->mIsDone = false;
    this->mRequestID = inRequestID;
}

// SYNTHETIC: EXE 0x4a5caa
// (the scalar deleting destructor)

// FUNCTION: EXE 0x4a5cc6
// Destroys the path request (members only)
EEAutoPathRequest::~EEAutoPathRequest()
{
}

// A path search in progress (pooled: UMemoryPool<EEPathManagerNode>, chunks of 0x32; sizeof 0x38; vtable 0x43c570, one
// slot: the deleting dtor 0xaa82d). Its members are out-of-line COMDATs that ICF folded into twins elsewhere:
// ReleaseUnit = EECPAGTask::ReleaseUnit 0x417f5, SetUnit = 0x41912, ReleaseRequest = 0xa5ce0, SetRequest = 0xa5cf6
// (provisional member names)
class EEPathManagerNode {
public:
    // out of line (COMDAT 0xab32c, after Clear's twin; the pool's New 0xaa2f0 and chunk ctor 0xaa74d call it with
    // all four arguments 0)
    EEPathManagerNode(EEComplexUnit* inUnit = NULL, EEAIAStarMachineBase* inPathMachine = NULL,
                      EEGraphPathMachine* inGraphMachine = NULL, EEAutoPathRequest* inRequest = NULL)
        : mPathMachine(inPathMachine), mGraphMachine(inGraphMachine), mL20(0), mL24(0), mL28(0), mL2C(0), mPasses(0)
    {
        this->mRequest = NULL;
        this->mUnit = NULL;
        this->SetUnit(inUnit);
        this->SetRequest(inRequest);
    }
    virtual ~EEPathManagerNode() { this->ReleaseUnit(); this->ReleaseRequest(); }
    void ReleaseUnit();
    void SetUnit(EEComplexUnit* inUnit);
    void ReleaseRequest();
    void SetRequest(EEAutoPathRequest* inRequest);
    void Clear();                           // 0xab37c

    long mL04;                              // +0x04
    EETileCoordinate mTile;                 // +0x08 (the goal)
    EEAIAStarMachineBase* mPathMachine;     // +0x10 (an EEPathMachine, or the bounded one when its mType is 4)
    EEGraphPathMachine* mGraphMachine;      // +0x14
    EEAutoPathRequest* mRequest;            // +0x18 (referenced)
    EEComplexUnit* mUnit;                   // +0x1c (referenced)
    long mL20;                              // +0x20
    long mL24;                              // +0x24
    long mL28;                              // +0x28
    long mL2C;                              // +0x2c
    long mPasses;                           // +0x30 (the passes run so far)
    long mL34;                              // +0x34
};

void EEPathManagerNode::ReleaseUnit()
{
    if (this->mUnit)
        this->mUnit->Dereference();
    this->mUnit = NULL;
}

void EEPathManagerNode::SetUnit(EEComplexUnit* inUnit)
{
    this->ReleaseUnit();
    this->mUnit = inUnit;
    if (inUnit)
        inUnit->Reference();
}

// FUNCTION: EXE 0x4a5ce0
// Drops the node's reference to its path request
void EEPathManagerNode::ReleaseRequest()
{
    if (this->mRequest) {
        this->mRequest->Dereference();
        this->mRequest = NULL;
    }
}

// FUNCTION: EXE 0x4a5cf6
// Makes the node hold (and reference) the given path request, releasing the previous one
void EEPathManagerNode::SetRequest(EEAutoPathRequest* inRequest)
{
    this->ReleaseRequest();
    if (inRequest) {
        this->mRequest = inRequest;
        inRequest->Reference();
    }
}

// TEMPLATE: EXE 0x4a962b
// (compiler-generated; name in analysis/exe/names.txt)

// TEMPLATE: EXE 0x4a977e
// (??0?$EEAIAStarMachine@VEEAIAStarStorage@@VEEAIAStarPathfindingGoal@@VEEMap@@@@QAE@JPAVEEMap@@@Z)

// TEMPLATE: EXE 0x4a97c8
// (?Reset@?$EEAIAStarMachine@VEEAIAStarStorage@@VEEAIAStarPathfindingGoal@@VEEMap@@@@UAEXXZ)

// TEMPLATE: EXE 0x4a9826
// (?GetRevLimit@?$EEAIAStarMachine@VEEAIAStarStorage@@VEEAIAStarPathfindingGoal@@VEEMap@@@@UAEJXZ)

// TEMPLATE: EXE 0x4a982d
// (?GetMaxRevs@?$EEAIAStarMachine@VEEAIAStarStorage@@VEEAIAStarPathfindingGoal@@VEEMap@@@@UAEJXZ)

// TEMPLATE: EXE 0x4a9834
// (?SetMaxRevs@?$EEAIAStarMachine@VEEAIAStarStorage@@VEEAIAStarPathfindingGoal@@VEEMap@@@@UAEXJ@Z)

// TEMPLATE: EXE 0x4a9841
// (?SetGoal@?$EEAIAStarMachine@VEEAIAStarStorage@@VEEAIAStarPathfindingGoal@@VEEMap@@@@UAEXJJ@Z)

// TEMPLATE: EXE 0x4a9852
// (?SetGoal@?$EEAIAStarMachine@VEEAIAStarStorage@@VEEAIAStarPathfindingGoal@@VEEMap@@@@UAEXABUEETileCoordinate@@@Z)

// TEMPLATE: EXE 0x4a9864
// (?SetRevLimit@?$EEAIAStarMachine@VEEAIAStarStorage@@VEEAIAStarPathfindingGoal@@VEEMap@@@@UAEXJ@Z)

// TEMPLATE: EXE 0x4a9871
// (?SetStart@?$EEAIAStarMachine@VEEAIAStarStorage@@VEEAIAStarPathfindingGoal@@VEEMap@@@@UAEXJJ@Z)

// TEMPLATE: EXE 0x4a9882
// (?SetStart@?$EEAIAStarMachine@VEEAIAStarStorage@@VEEAIAStarPathfindingGoal@@VEEMap@@@@UAEXABUEETileCoordinate@@@Z)

// TEMPLATE: EXE 0x4a98d9
// (?GetRevLimit@?$EEAIAStarGraphMachine@VEEAIAStarGraphStorage@@VEEAIAStarPathGraphGoal@@VEEAIPathSubSector@@@@UAEJXZ)

// TEMPLATE: EXE 0x4a98e0
// (?GetMaxRevs@?$EEAIAStarGraphMachine@VEEAIAStarGraphStorage@@VEEAIAStarPathGraphGoal@@VEEAIPathSubSector@@@@UAEJXZ)

// TEMPLATE: EXE 0x4a98e7
// (?SetMaxRevs@?$EEAIAStarGraphMachine@VEEAIAStarGraphStorage@@VEEAIAStarPathGraphGoal@@VEEAIPathSubSector@@@@UAEXJ@Z)

// TEMPLATE: EXE 0x4a98f4
// (?SetRevLimit@?$EEAIAStarGraphMachine@VEEAIAStarGraphStorage@@VEEAIAStarPathGraphGoal@@VEEAIPathSubSector@@@@UAEXJ@Z)

// TEMPLATE: EXE 0x4a992c
// (compiler-generated; name in analysis/exe/names.txt)

// TEMPLATE: EXE 0x4a99ba
// (?Reset@?$EEAIAStarMachine@VEEAIAStarBoundedStorage@@VEEAIAStarBoundedPathGoal@@VEEMap@@@@UAEXXZ)

// TEMPLATE: EXE 0x4a9a37
// (?GetRevLimit@?$EEAIAStarMachine@VEEAIAStarBoundedStorage@@VEEAIAStarBoundedPathGoal@@VEEMap@@@@UAEJXZ)

// TEMPLATE: EXE 0x4a9a6b
// (?SetRevLimit@?$EEAIAStarMachine@VEEAIAStarBoundedStorage@@VEEAIAStarBoundedPathGoal@@VEEMap@@@@UAEXJ@Z)

// TEMPLATE: EXE 0x4a9a8e
// (compiler-generated; name in analysis/exe/names.txt)

// TEMPLATE: EXE 0x4a9aa5
// (compiler-generated; name in analysis/exe/names.txt)

// TEMPLATE: EXE 0x4a9ad1
// (??_G?$EEAIAStarMachine@VEEAIAStarStorage@@VEEAIAStarPathfindingGoal@@VEEMap@@@@UAEPAXI@Z)

// TEMPLATE: EXE 0x4a9aed
// (??1?$EEAIAStarMachine@VEEAIAStarStorage@@VEEAIAStarPathfindingGoal@@VEEMap@@@@UAE@XZ)

// TEMPLATE: EXE 0x4a9b1e
// (??_G?$EEAIAStarGraphMachine@VEEAIAStarGraphStorage@@VEEAIAStarPathGraphGoal@@VEEAIPathSubSector@@@@UAEPAXI@Z)

// TEMPLATE: EXE 0x4a9b3a
// (??1?$EEAIAStarGraphMachine@VEEAIAStarGraphStorage@@VEEAIAStarPathGraphGoal@@VEEAIPathSubSector@@@@UAE@XZ)

// TEMPLATE: EXE 0x4a9b6b
// (??_G?$EEAIAStarMachine@VEEAIAStarBoundedStorage@@VEEAIAStarBoundedPathGoal@@VEEMap@@@@UAEPAXI@Z)

// TEMPLATE: EXE 0x4a9b87
// (??1?$EEAIAStarMachine@VEEAIAStarBoundedStorage@@VEEAIAStarBoundedPathGoal@@VEEMap@@@@UAE@XZ)

// TEMPLATE: EXE 0x4a9c59
// (compiler-generated; name in analysis/exe/names.txt)

// TEMPLATE: EXE 0x4a9cca
// (compiler-generated; name in analysis/exe/names.txt)

// TEMPLATE: EXE 0x4a9e5e
// (compiler-generated; name in analysis/exe/names.txt)

// TEMPLATE: EXE 0x4a9eea
// (compiler-generated; name in analysis/exe/names.txt)

// TEMPLATE: EXE 0x4a9f1c
// (compiler-generated; name in analysis/exe/names.txt)

// TEMPLATE: EXE 0x4aa1ff
// (compiler-generated; name in analysis/exe/names.txt)

// TEMPLATE: EXE 0x4aa63b
// (?Virtual04@EEAIPathGoalModifierSea@@UAEJJJJJJJ@Z)

// TEMPLATE: EXE 0x4aa65b
// (?Virtual08@EEAIPathGoalModifierSea@@UAE_NPAVEETile@@JJ_N@Z)

// TEMPLATE: EXE 0x4aa678
// (compiler-generated; name in analysis/exe/names.txt)

// TEMPLATE: EXE 0x4aa691
// (compiler-generated; name in analysis/exe/names.txt)

// TEMPLATE: EXE 0x4aa6e6
// (compiler-generated; name in analysis/exe/names.txt)

// TEMPLATE: EXE 0x4aa73b
// (compiler-generated; name in analysis/exe/names.txt)

// TEMPLATE: EXE 0x4aa7d3
// (??0?$pair@UEEPathHistoryEntry@@H@std@@QAE@ABUEEPathHistoryEntry@@ABH@Z; PruneHistory's pair)

// TEMPLATE: EXE 0x4aa7fb
// (compiler-generated; name in analysis/exe/names.txt)

// TEMPLATE: EXE 0x4aa817
// (compiler-generated; name in analysis/exe/names.txt)

// TEMPLATE: EXE 0x4aa82d
// (??_EEEPathManagerNode@@UAEPAXI@Z)

// TEMPLATE: EXE 0x4ab30d
// (compiler-generated; name in analysis/exe/names.txt)

// TEMPLATE: EXE 0x4ab32c
// (the node ctor, inline in the class above)

// TEMPLATE: EXE 0x4ab37c
// Empties the node for reuse: releases its unit and request, forgets its machines and goal tile, zeroes its passes
void EEPathManagerNode::Clear()
{
    this->ReleaseUnit();
    this->ReleaseRequest();
    this->mL20 = 0;
    this->mL28 = 0;
    this->mPathMachine = NULL;
    this->mGraphMachine = NULL;
    this->mL24 = 0;
    this->mL2C = 0;
    this->mPasses = 0;
    this->mTile.mX = 0;
    this->mTile.mY = 0;
}

// TEMPLATE: EXE 0x4ab3aa
// Moves a tile that is off the map onto the map tile closest to its center
inline void EEPathManager::ClampTile(EEMap* inMap, EETileCoordinate& ioTile)
{
    if (inMap->GetTileAtLocation(ioTile) != NULL)
        return;
    float theHalf = 0.5f;
    GE3DPoint thePoint((float)ioTile.GetX() + theHalf, (float)ioTile.GetY() + theHalf, 0.0f);
    long theX = 0;
    long theY = 0;
    inMap->GetClosestTile(&thePoint, &theX, &theY);
    ioTile.mX = theX;
    ioTile.mY = theY;
}

// The path goal's sea and space modifiers (vtables 0x43c458/0x43c448; sizeof 0xc; the ctor is inline: refcount 1, the
// goal, the vptr). Their slots +0x04 0xaa63b / 0x10b8a and +0x08 0xaa65b forward to the base's shared tests
// (provisional member names)
class EEAIPathGoalModifierSea : public EEAIPathGoalModifierBase {
public:
    EEAIPathGoalModifierSea(EEAIAStarPathfindingGoalBase* inGoal) { this->mGoal = inGoal; }
    virtual long GetCost(long inFromX, long inFromY, long inX, long inY, long inParentX, long inParentY)
    {
        return this->GetStepCost(true, inFromX, inFromY, inX, inY, inParentX, inParentY);
    }
    virtual bool CanEnter(EETile* inTile, long inX, long inY, bool inGoal)
    {
        return this->IsPassable(this->mGoal, NULL, inTile, inX, inY, inGoal);
    }
};

class EEAIPathGoalModifierSpace : public EEAIPathGoalModifierBase {
public:
    EEAIPathGoalModifierSpace(EEAIAStarPathfindingGoalBase* inGoal) { this->mGoal = inGoal; }
    virtual long GetCost(long inFromX, long inFromY, long inX, long inY, long inParentX, long inParentY)
    {
        return this->GetStepCost(false, inFromX, inFromY, inX, inY, inParentX, inParentY);
    }
    virtual bool CanEnter(EETile* inTile, long inX, long inY, bool inGoal)
    {
        return this->IsPassable(this->mGoal, NULL, inTile, inX, inY, inGoal);
    }
};

// The bounded goal's sea/space modifiers EEAIBPGModifierSea/Space: ee/EEAIBoundedPathGoalModifierBase.h

// FUNCTION: EXE 0x4a5d10
// Creates the path manager with no bounded machine, nothing queued and region updates off
EEPathManager::EEPathManager()
    : mNextHistoryPruneTime(0), mNextRegionPruneTime(0), mLCC(0), mNextRequestID(0), mNextRegionTime(0), mIsUpdatingRegions(false)
{
    this->mBoundedMachine = NULL;
    this->mLC8 = 5;
}

// SYNTHETIC: EXE 0x4a5e00
// (the scalar deleting destructor)

// FUNCTION: EXE 0x4a5e1c
// Destroys the path manager, freeing every machine, queued search, region map and the node pools
EEPathManager::~EEPathManager()
{
    this->Clear();
}

// FUNCTION: EXE 0x4a5e7b
// Records that a unit asked for a path to a tile, stamped with the game time (newest first)
void EEPathManager::AddHistory(EEComplexUnit* inUnit, const EETileCoordinate& inTile)
{
    EEPathHistoryEntry theEntry;
    theEntry.mUnitID = inUnit->mUnitID;
    theEntry.mTime = 0;
    theEntry.mX = inTile.GetX();
    theEntry.mY = inTile.GetY();
    theEntry.mTime = gPhysics->mGameTime;
    this->mHistory.push_front(theEntry);  // newest first: 0xa95ae is deque::push_front (_Myoff ? _Myoff : _Mapsize * 256, then --)
}

// FUNCTION: EXE 0x4a5eba
// Deletes every spare path and graph machine
void EEPathManager::FreeMachines()
{
    while (this->mSparePathMachines.size() != 0) {
        EEPathMachine* theMachine = this->mSparePathMachines.back();
        theMachine->Reset();
        this->mSparePathMachines.pop_back();
        delete theMachine;
    }
    while (this->mSpareGraphMachines.size() != 0) {
        EEGraphPathMachine* theMachine = this->mSpareGraphMachines.back();
        theMachine->Reset();
        this->mSpareGraphMachines.pop_back();
        delete theMachine;
    }
}

// FUNCTION: EXE 0x4a5f39
// Drops every queued tile and graph search, returning their machines and nodes
void EEPathManager::FreePaths()
{
    while (this->mPaths.size() != 0) {
        EEPathManagerNode* theNode = *this->mPaths.begin();
        this->ReleasePathMachine((EEPathMachine*)theNode->mPathMachine);
        this->ReleaseNode(theNode);
        this->mPaths.pop_front();
    }
    while (this->mAutoPaths.size() != 0) {
        EEPathManagerNode* theNode = *this->mAutoPaths.begin();
        this->ReleaseGraphMachine(theNode->mGraphMachine);
        this->ReleaseNode(theNode);
        this->mAutoPaths.pop_front();
    }
}

// FUNCTION: EXE 0x4a5fc4
// Deletes every unit's region map (its repeated goal tile counts)
void EEPathManager::FreeRegions()
{
    std::map<long, std::map<U2DPoint<long>, unsigned long>*>::iterator it;
    for (it = this->mRegions.begin(); it != this->mRegions.end(); it++)
        delete it->second;
    this->mRegions.clear();
}

// FUNCTION: EXE 0x4a6010
// Sets a graph search's step budget to the distance times 250/75/90/110 (by distance band); at most 40 per tile when
// the goal sector's kind is not allowed, 3 when a computer citizen's newest request within 4 s was the same tile
void EEPathManager::SetGraphRevs(EEGraphPathMachine* inMachine, const EETileCoordinate& inFrom,
                                 const EETileCoordinate& inTo)
{
    std::deque<EEPathHistoryEntry>::iterator it;
    EEAIPathSubSector* theSector = gContinentManager.GetPathSubSectorAt(inTo.GetX(), inTo.GetY());
    long theDistance = inFrom.DistanceTo(inTo);
    unsigned long theTime;
    theTime = gPhysics ? gPhysics->mGameTime : 0;
    if (theTime > 4000)
        theTime -= 4000;
    long theFactor;
    if (theDistance <= 0xf)
        theFactor = 0xfa;
    else if (theDistance <= 0x50)
        theFactor = 0x4b;
    else
        theFactor = theDistance <= 0xa0 ? 0x5a : 0x6e;
    if (!inMachine->mGoal.mPathFlags.mFlags[theSector->mKind] && theFactor >= 0x28)
        theFactor = 0x28;
    EEComplexUnit* theUnit = inMachine->mGoal.mUnit;
    if (theUnit && theUnit->mPlayer->mComputerPlayer && theUnit->mIsCitizen) {
        for (it = this->mHistory.begin(); it != this->mHistory.end(); it++) {
            if ((*it).mTime < theTime)
                break;
            if ((*it).mUnitID != theUnit->GetUnitID())
                continue;
            if ((*it).GetX() == inTo.GetX() && (*it).GetY() == inTo.GetY())
                theFactor = 3;
            break;
        }
    }
    inMachine->SetMaxRevs(theDistance * theFactor);
}

// FUNCTION: EXE 0x4a6157
// Sets a tile search's step budget to the distance times 375 (up to 15 tiles), 140 or 145 (over 80); at most 40
// per tile when a unit that is not amphibious goes to another continent
void EEPathManager::SetPathRevs(EEPathMachine* inMachine, EEComplexUnit* inUnit, const EETileCoordinate& inFrom,
                                const EETileCoordinate& inTo)
{
    EEAIContinent* theFromContinent = gContinentManager.GetContinentAt(inFrom.GetX(), inFrom.GetY());
    EEAIContinent* theToContinent = gContinentManager.GetContinentAt(inTo);
    long theDistance = inFrom.DistanceTo(inTo);
    bool theAmphibious;
    if (inUnit->mUnitClass->CanMoveOnTerrain(inUnit, 0) && inUnit->mUnitClass->CanMoveOnTerrain(inUnit, 1))
        theAmphibious = true;
    else
        theAmphibious = false;
    long theFactor;
    if (theDistance <= 0xf)
        theFactor = 0x177;
    else {
        theFactor = 0x8c;
        if (theDistance > 0x50)
            theFactor += 5;
    }
    if (!theAmphibious && theFromContinent != theToContinent && theFactor >= 0x28)
        theFactor = 0x28;
    inMachine->SetMaxRevs(theDistance * theFactor);
}

// FUNCTION: EXE 0x4a61f2
// Makes sure the bounded path machine exists for the map, recreating it (with corner checks) for another map
void EEPathManager::PrepareBoundedMachine(EEMap* inMap)
{
    if (this->mBoundedMachine != NULL && this->mBoundedMachine->mMap == inMap)
        return;
    if (this->mBoundedMachine) {
        this->mBoundedMachine->Reset();
        delete this->mBoundedMachine;
    }
    this->mBoundedMachine = new EEBoundedPathMachine(4, inMap);
    this->mBoundedMachine->mCheckCorners = true;
}

// FUNCTION: EXE 0x4a6251
// Finishes the unit's queued graph search at once (no step limit), applies its result and drops it
void EEPathManager::CancelAutoPath(EEComplexUnit* inUnit)
{
    if (this->mAutoPaths.size() == 0)
        return;
    for (std::deque<EEPathManagerNode*>::iterator it = this->mAutoPaths.begin(); it != this->mAutoPaths.end(); it++) {
        if ((*it)->mUnit != inUnit)
            continue;
        EEPathManagerNode* theNode = *it;
        if (theNode) {
            theNode->mGraphMachine->SetRevLimit(-1);
            theNode->mGraphMachine->Run();
            this->FinishGraphPath(theNode);
            this->CancelAutoPaths(inUnit);
        }
        return;
    }
}

// FUNCTION: EXE 0x4a62ce
// Finishes the queued graph search of a path request at once, applies its result and drops it
void EEPathManager::CompleteAutoPath(EEAutoPathRequest* inRequest)
{
    if (this->mAutoPaths.size() == 0)
        return;
    for (std::deque<EEPathManagerNode*>::iterator it = this->mAutoPaths.begin(); it != this->mAutoPaths.end(); it++) {
        if ((*it)->mRequest != inRequest)
            continue;
        EEPathManagerNode* theNode = *it;
        if (theNode) {
            theNode->mGraphMachine->SetRevLimit(-1);
            theNode->mGraphMachine->Run();
            this->FinishGraphPath(theNode);
            this->CancelAutoPathRequest(inRequest);
        }
        return;
    }
}

// FUNCTION: EXE 0x4a634b
// Finishes the unit's queued searches at once: its graph search, then its tile search, whose path it takes
void EEPathManager::CompletePath(EEComplexUnit* inUnit)
{
    if (this->mAutoPaths.size() != 0)
        this->CancelAutoPath(inUnit);
    EEPathManagerNode* theNode = this->FindPath(inUnit);
    if (!theNode)
        return;
    theNode->mPathMachine->SetRevLimit(-1);
    theNode->mPathMachine->Run();
    this->FinishPath(theNode);
    this->CancelPaths(inUnit, true);
}

// FUNCTION: EXE 0x4a639b
// Writes the path sub sectors and the queued tile and graph searches to an out-of-synch dump
void EEPathManager::DumpOutOfSynch(_iobuf* inFile)
{
    fprintf(inFile, "OUT OF SYNCH DUMP: Pathfinding Information\nOUT OF SYNCH DUMP:----------------------------------\n");
    fprintf(inFile, "OUT OF SYNCH DUMP: Path SubSectors: %d\n", gContinentManager.mPathSubSectors.size());
    for (long i = gContinentManager.mPathSubSectors.size() - 1; i > 0; i--) {
        EEAIPathSubSector* theSector = gContinentManager.mPathSubSectors[(unsigned short)i];
        if (theSector)
            fprintf(inFile, "OUT OF SYNCH DUMP: PathSubSector[%d] with center point: %d,%d\n", theSector->mID,
                    theSector->mLocation.GetX(), theSector->mLocation.GetY());
    }
    fprintf(inFile, "OUT OF SYNCH DUMP: Paths In progress: %d\n", this->mPaths.size());
    unsigned long j;
    for (j = 0; j < this->mPaths.size(); j++) {
        // cast the node's machine (the node itself used to be cast, which always failed)
        EEPathMachine* theMachine = dynamic_cast<EEPathMachine*>(this->mPaths[j]->mPathMachine);
        if (theMachine) {
            fprintf(inFile, "OUT OF SYNCH DUMP:\n");
            fprintf(inFile, "OUT OF SYNCH DUMP: Paths Queue[%d]: \n", j);
            unsigned long theUnitID;
            if (this->mPaths[j]->mUnit)
                theUnitID = this->mPaths[j]->mUnit->GetUnitID();
            else
                theUnitID = 0;
            fprintf(inFile, "OUT OF SYNCH DUMP: Path for Unit: %d to (%d,%d) has been queued %d time.\n", theUnitID,
                    this->mPaths[j]->mTile.GetX(), this->mPaths[j]->mTile.GetY(), this->mPaths[j]->mPasses);
            fprintf(inFile, "OUT OF SYNCH DUMP: Revs completed: %d. Tile Cap is: %d Pathfinder Type is: %d\n",
                    theMachine->mRevs, theMachine->GetMaxRevs(), theMachine->mType);
        } else {
            fprintf(inFile, "OUT OF SYNCH DUMP: !!! UNKNOWN PATHFINDER! Can't Dynamic cast it!\n");
        }
    }
    fprintf(inFile, "OUT OF SYNCH DUMP: Graphs In progress: %d\n", this->mAutoPaths.size());
    for (j = 0; j < this->mAutoPaths.size(); j++) {
        // the node's graph machine (the node itself used to be cast, which always failed)
        EEGraphPathMachine* theMachine = this->mAutoPaths[j]->mGraphMachine;
        if (theMachine) {
            fprintf(inFile, "OUT OF SYNCH DUMP:\n");
            fprintf(inFile, "OUT OF SYNCH DUMP: AutoPaths Queue[%d]: \n", j);
            unsigned long theUnitID;
            // the graph search's own unit, tile and passes (they used to be read from mPaths)
            if (this->mAutoPaths[j]->mUnit)
                theUnitID = this->mAutoPaths[j]->mUnit->GetUnitID();
            else
                theUnitID = 0;
            fprintf(inFile, "OUT OF SYNCH DUMP: AutoPath for Unit: %d to (%d,%d) has been queued %d time.\n",
                    theUnitID, this->mAutoPaths[j]->mTile.GetX(), this->mAutoPaths[j]->mTile.GetY(),
                    this->mAutoPaths[j]->mPasses);
            fprintf(inFile, "OUT OF SYNCH DUMP: Revs completed: %d. Tile Cap is: %d\n", theMachine->mRevs,
                    theMachine->GetMaxRevs());
        } else {
            fprintf(inFile, "OUT OF SYNCH DUMP: !!! UNKNOWN GraphFinder! Can't Dynamic cast it!\n");
        }
    }
}

// FUNCTION: EXE 0x4a6687
// Returns a graph machine of the given type: a spare one sized for the current sub sector count (spares of another
// size are deleted), else a new one
EEGraphPathMachine* EEPathManager::NewGraphMachine(long inType)
{
    EEGraphPathMachine* theMachine = NULL;
    while (this->mSpareGraphMachines.size() != 0) {
        if (this->mSpareGraphMachines.back()->mCount == gContinentManager.mPathSubSectors.size() + 1)
            theMachine = this->mSpareGraphMachines.back();
        else
            delete this->mSpareGraphMachines.back();
        this->mSpareGraphMachines.pop_back();
        if (theMachine)
            break;
    }
    if (theMachine == NULL)
        theMachine = new EEGraphPathMachine(inType, gContinentManager.mPathSubSectors.size() + 1);
    theMachine->mType = inType;
    return theMachine;
}

// FUNCTION: EXE 0x4a6724
// Returns a tile path machine for the map (a spare one built for the same map, else a new one), with a sea or
// space modifier for the types 2 and 0x10
EEPathMachine* EEPathManager::NewPathMachine(long inType, EEMap* inMap)
{
    EEPathMachine* theMachine = NULL;
    if (this->mSparePathMachines.size() == 0) {
        theMachine = new EEPathMachine(1, inMap);
    } else {
        theMachine = this->mSparePathMachines.back();
        this->mSparePathMachines.pop_back();
        if (inMap != theMachine->mGoal.mMap) {
            delete theMachine;
            theMachine = new EEPathMachine(1, inMap);
        }
    }
    EEAIPathGoalModifierBase* theModifier;
    if (inType == 2)
        theModifier = new EEAIPathGoalModifierSea(&theMachine->mGoal);
    else if (inType == 0x10)
        theModifier = new EEAIPathGoalModifierSpace(&theMachine->mGoal);
    else
        goto done;
    theMachine->mGoal.SetModifier(theModifier);
    theModifier->Dereference();
done:
    theMachine->mType = inType;
    return theMachine;
}

// FUNCTION: EXE 0x4a6819
// Returns an empty node from the pool
EEPathManagerNode* EEPathManager::NewNode()
{
    EEPathManagerNode* theNode = this->mNodePool.New();
    theNode->Clear();
    return theNode;
}

// FUNCTION: EXE 0x4a682f
// Returns whether a graph search is queued for the unit
bool EEPathManager::HasAutoPath(EEComplexUnit* inUnit)
{
    bool theResult = false;
    if (this->mAutoPaths.size() != 0) {
        for (std::deque<EEPathManagerNode*>::iterator it = this->mAutoPaths.begin(); it != this->mAutoPaths.end(); it++) {
            if ((*it)->mUnit == inUnit) {
                theResult = true;
                break;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x4a687d
// Returns false while a tile search is queued for the unit (true for no unit)
bool EEPathManager::IsPathDone(EEComplexUnit* inUnit)
{
    bool theResult = true;
    if (this->mPaths.size() != 0 && inUnit) {
        std::deque<EEPathManagerNode*>::iterator it;
        for (it = this->mPaths.begin(); it != this->mPaths.end(); it++) {
            if ((*it)->mUnit == inUnit) {
                theResult = false;
                break;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x4a68cf
// Gets the goal tile of the unit's queued tile search; returns false when one is queued, true (tile 0,0) if not
bool EEPathManager::GetPathGoal(EEComplexUnit* inUnit, U2DPoint<long>& outTile)
{
    bool theResult = true;
    outTile.mX = 0;
    outTile.mY = 0;
    if (this->mPaths.size() != 0 && inUnit) {
        for (std::deque<EEPathManagerNode*>::iterator it = this->mPaths.begin(); it != this->mPaths.end(); it++) {
            if ((*it)->mUnit != inUnit)
                continue;
            theResult = false;
            outTile = (*it)->mTile;
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x4a6948
// Creates a player's graph path request with the next request id and starts its search; returns the request
EEAutoPathRequest* EEPathManager::RequestPath(unsigned long inPlayerID, const EETileCoordinate& inFrom,
                                              const EETileCoordinate& inTo, EECPASGFlags* inFlags, long inType,
                                              long inRevs, EEAIGraphPathGoalModifierBase* inModifier, bool inUnlimitedRevs)
{
    EEAutoPathRequest* theRequest =
        new EEAutoPathRequest(this->mNextRequestID, inPlayerID, inFrom, inTo, *inFlags);
    bool theStarted = false;
    this->mNextRequestID++;
    this->StartGraphPath(NULL, inFrom, inTo, &theStarted, inPlayerID, theRequest, inType, inRevs, inModifier, inUnlimitedRevs);
    return theRequest;
}

// FUNCTION: EXE 0x4a69b0
// Starts a graph path search for a unit; returns whether the search still runs (queued)
bool EEPathManager::RequestGroupPath(EEComplexUnit* inUnit, const EETileCoordinate& inFrom, const EETileCoordinate& inTo,
                                     long inPlayerID, long inType, long inRevs, EEAIGraphPathGoalModifierBase* inModifier, bool inUnlimitedRevs)
{
    bool theStarted = false;
    this->StartGraphPath(inUnit, inFrom, inTo, &theStarted, inPlayerID, NULL, inType, inRevs, inModifier, inUnlimitedRevs);
    return theStarted;
}

// NEAR: the request's flags go to the goal through EECPASGFlags::operator= (out of line 0xaa1da in the original)
// FUNCTION: EXE 0x4a69e2
// Starts a sub sector graph search between two tiles (moved onto the map) for a unit or a request: queued while it
// runs (*outStarted true), else applied and freed at once; always returns false
bool EEPathManager::StartGraphPath(EEComplexUnit* inUnit, const EETileCoordinate& inFrom, const EETileCoordinate& inTo,
                                   bool* outStarted, long inPlayerID, EEAutoPathRequest* inRequest, long inType,
                                   long inRevs, EEAIGraphPathGoalModifierBase* inModifier, bool inUnlimitedRevs)
{
    EEPathManagerNode* theNode = NULL;
    EEGraphPathMachine* theMachine = this->NewGraphMachine(inType);
    EETileCoordinate theFrom = inFrom;
    EETileCoordinate theTo = inTo;
    *outStarted = false;
    EETileCoordinate theFromCenter = theFrom;
    EETileCoordinate theToCenter = theTo;
    EEAIAStarPathGraphGoal& theGoal = theMachine->mGoal;
    if (inUnit)
        this->CancelAutoPaths(inUnit);
    theMachine->Reset();
    theGoal.Reset();
    if (!inUnlimitedRevs)
        theGoal.mRevLimit = 0xf;
    if (inModifier)
        theGoal.SetModifier(inModifier);
    this->ClampTile(gWorld.mMap, theFromCenter);
    theFrom = theFromCenter;
    EEAIPathSubSector* theFromSector = gContinentManager.GetPathSubSectorAt(theFromCenter.GetX(), theFromCenter.GetY());
    theFromCenter = theFromSector->mSector->mCenter;
    this->ClampTile(gWorld.mMap, theToCenter);
    theTo = theToCenter;
    EEAIPathSubSector* theToSector = gContinentManager.GetPathSubSectorAt(theToCenter.GetX(), theToCenter.GetY());
    theToCenter = theToSector->mSector->mCenter;
    theGoal.SetUnit(inUnit);
    if (inRequest)
        theGoal.mPathFlags = inRequest->mFlags;
    theMachine->SetGoal(theFromCenter, theFromSector->mID);
    theMachine->SetStart(theToCenter, theToSector->mID);
    theMachine->mTile = theToCenter;
    theMachine->mSector = theToSector->mID;
    theGoal.mPoint38 = theToCenter;
    theGoal.mFrom = theFromSector;
    theGoal.mTo = theToSector;
    theGoal.mPlayerID = inPlayerID;
    if (inRevs <= 0)
        this->SetGraphRevs(theMachine, theFrom, theTo);
    else
        theMachine->SetMaxRevs(inRevs);
    theMachine->Run();
    *outStarted = theMachine->mFound;
    if (theMachine->mFound) {
        theNode = this->NewNode();
        theNode->SetRequest(inRequest);
        theNode->SetUnit(inUnit);
        theNode->mGraphMachine = theMachine;
        theNode->mTile = theTo;
        theNode->mPasses = 1;
        this->mAutoPaths.push_back(theNode);
    } else {
        this->RequestUnitPaths(inUnit, inRequest, theMachine);
        this->ReleaseGraphMachine(theMachine);
    }
    return false;
}

// FUNCTION: EXE 0x4a6bfd
// Runs a bounded tile search (at most 200 steps; a window around inFrom when inBounds is empty) and returns its
// path (and the visited tiles when asked)
void EEPathManager::FindPath(EEComplexUnit* inUnit, EEMap* inMap, const U2DPoint<long>& inFrom, const U2DPoint<long>& inTo,
                             const EEAIAStarBounds& inBounds, std::vector<U2DPoint<long> >& outPath, long inType,
                             long inRevs, EEAIPathGoalModifierBase* inModifier, std::vector<U2DPoint<long> >* outVisited)
{
    EEAIAStarBounds theBounds = inBounds;
    EETileCoordinate theFrom;
    theFrom = inFrom;
    EETileCoordinate theTo;
    theTo = inTo;
    long theRevs = inRevs;
    outPath.clear();
    this->ClampTile(inMap, theFrom);
    this->ClampTile(inMap, theTo);
    this->PrepareBoundedMachine(inMap);
    EEBoundedPathMachine* theMachine = this->mBoundedMachine;
    if (inModifier == NULL) {
        EEAIPathGoalModifierBase* theModifier = NULL;
        if (inType == 2)
            theModifier = new EEAIBPGModifierSea(&this->mBoundedMachine->mGoal);
        else if (inType == 0x10)
            theModifier = new EEAIBPGModifierSpace(&this->mBoundedMachine->mGoal);
        else
            goto set;
        theMachine->mGoal.SetModifier(theModifier);
        theModifier->Dereference();
    } else {
        theMachine->mGoal.SetModifier(inModifier);
    }
set:
    theMachine->mGoal.SetUnit(inUnit);
    this->mBoundedMachine->SetStart(theFrom);
    this->mBoundedMachine->SetGoal(theTo);
    this->mBoundedMachine->mTile = theTo;
    if (inRevs <= 0) {
        if (inUnit == NULL)
            this->mBoundedMachine->SetMaxRevs(200);
        else
            this->SetPathRevs((EEPathMachine*)this->mBoundedMachine, inUnit, theFrom, theTo);
        theRevs = this->mBoundedMachine->GetMaxRevs();
        if (theRevs > 200) {
            theRevs = 200;
            this->mBoundedMachine->SetMaxRevs(theRevs);
        }
    } else {
        if (inRevs > 200)
            theRevs = 200;
        this->mBoundedMachine->SetMaxRevs(theRevs);
    }
    if (theBounds.mLeft == theBounds.mRight || theBounds.mBottom == theBounds.mTop) {
        theBounds.mTop = inFrom.GetY() - theRevs - 1;
        if (theBounds.mTop <= 0)
            theBounds.mTop = 0;
        theBounds.mLeft = inFrom.GetX() - theRevs - 1;
        if (theBounds.mLeft <= 0)
            theBounds.mLeft = 0;
        long theLast = inMap->mSize - 1;
        theBounds.mRight = inFrom.GetX() + 1 + theRevs;
        if (theBounds.mRight >= theLast)
            theBounds.mRight = theLast;
        theBounds.mBottom = inFrom.GetY() + 1 + theRevs;
        if (theBounds.mBottom >= theLast)
            theBounds.mBottom = theLast;
    }
    this->mBoundedMachine->mGoal.mBounds = theBounds;
    this->mBoundedMachine->mSearchBounds = theBounds;
    this->mBoundedMachine->Run();
    if (outVisited == NULL)
        this->mBoundedMachine->GetPath(outPath);
    else
        this->mBoundedMachine->GetPath(outPath, *outVisited);
    bool theReached;
    if (inTo.GetX() == this->mBoundedMachine->mTile.GetX() && inTo.GetY() == this->mBoundedMachine->mTile.GetY())
        theReached = true;
    else
        theReached = false;
    this->mBoundedMachine->Reset();
}

// NEAR: the location goal's tile goes through __ftol here.
// FUNCTION: EXE 0x4a6e88
// Requests a unit's path: a graph search when asked; otherwise records the request, tries a short bounded search
// and, unless that settles it, starts a tile search (queued while it runs); nothing for a -10000 start
void EEPathManager::RequestUnitPath(EEComplexUnit* inUnit, const EETileCoordinate& inFrom, const EETileCoordinate& inTo,
                                    bool inUnlimitedRevs, long inRevs, bool inUseGraph, EEAIGraphPathGoalModifierBase* inModifier, bool inArg8)
{
    EETileCoordinate theShort;
    theShort.mX = 0;
    theShort.mY = 0;
    bool theStarted = false;
    bool theNoPath = true;
    EETileCoordinate theTo = inTo;
    EETileCoordinate theFrom = inFrom;
    EEMap* theMap = gWorld.mMap;
    if (theFrom.GetX() == -10000 || theFrom.GetY() == -10000)
        return;
    this->ClampTile(theMap, theFrom);
    this->ClampTile(theMap, theTo);
    if (inUseGraph) {
        theStarted = this->RequestGroupPath(inUnit, inFrom, theTo, inUnit->GetPlayer()->GetPlayerID(), 1, -1, inModifier, inUnlimitedRevs);
        if (!theStarted) {
            EEUGLocation* theGoal = dynamic_cast<EEUGLocation*>(inUnit->GetGoal());
            if (theGoal && theGoal->mIsAutoWaypoint) {
                theTo.mX = (long)theGoal->mLocation.GetX();
                theTo.mY = (long)theGoal->mLocation.GetY();
            }
        }
        theNoPath = inUnit->mWaypoints.size() == 0;
    }
    if (inUnit != NULL && !theNoPath)
        return;
    long theType;
    if (inUnit->mUnitClass->CanMoveOnTerrain(inUnit, 1))
        theType = 2;
    else
        theType = inUnit->mUnitClass->CanMoveOnTerrain(inUnit, 0xc) ? 0x10 : 1;
    if (inUnit->mNumPathRequests != inUnit->mNumPathRequests + 1)
        inUnit->mNumPathRequests = inUnit->mNumPathRequests + 1;
    if ((!inUnit->GetPlayer()->IsHuman() || inUnit->GetPlayer()->GetPlayerID() == 0) && !inUnit->mIsDetachedFromAI && inUnit->mPreviousPart == NULL)
        this->AddHistory(inUnit, inTo);
    this->CancelPaths(inUnit, false);
    if (!inUnlimitedRevs && (!inArg8 || inUnit == NULL || (long)inUnit->mWaypoints.size() < 7)) {
        theShort = theTo;
        bool theDone = this->TryShortPath(inUnit, theMap, theType, theFrom, theShort);
        theFrom = theShort;
        if (theDone)
            return;
    }
    if (!theStarted || !inUseGraph) {
        EEPathManagerNode* theNode = this->NewNode();
        EEPathMachine* theMachine = this->NewPathMachine(theType, theMap);
        EEAIAStarPathfindingGoalBase* theGoal = &theMachine->mGoal;
        theMachine->CancelSearch();
        theMachine->SetRevLimit(inUnlimitedRevs ? -1 : 0xf);
        if (inRevs <= 0)
            this->SetPathRevs(theMachine, inUnit, theFrom, theTo);
        else
            theMachine->SetMaxRevs(inRevs);
        theNode->SetUnit(inUnit);
        theNode->mPathMachine = theMachine;
        theNode->mTile = theTo;
        theMachine->SetStart(theFrom);
        theMachine->SetGoal(theTo.GetX(), theTo.GetY());
        theMachine->mTile = theTo;
        theGoal->mDestination = theTo;
        theGoal->SetUnit(inUnit);
        theMachine->mCheckCorners = true;
        theMachine->Run();
        if (theMachine->mFound) {
            theNode->mPasses = 1;
            this->mPaths.push_back(theNode);
        } else {
            this->FinishPath(theNode);
            theMachine->Reset();
            this->ReleasePathMachine(theMachine);
            this->ReleaseNode(theNode);
        }
    }
}

// FUNCTION: EXE 0x4a7170
// Returns the unit's queued tile search node, or NULL
EEPathManagerNode* EEPathManager::FindPath(EEComplexUnit* inUnit)
{
    EEPathManagerNode* theResult = NULL;
    if (this->mPaths.size() != 0 && inUnit) {
        std::deque<EEPathManagerNode*>::iterator it;
        for (it = this->mPaths.begin(); it != this->mPaths.end(); it++) {
            if ((*it)->mUnit == inUnit) {
                theResult = *it;
                break;
            }
        }
    }
    return theResult;
}

// The window grows from the unclamped inFrom; inArg8 is not read; *outReached is the result; the machine is prepared twice.
// FUNCTION: EXE 0x4a71c9
// Runs a bounded search within inMargin tiles of inFrom and, when it reaches inTo, appends the path (start first);
// returns whether it reached inTo
bool EEPathManager::FindBoundedPath(EEComplexUnit* inUnit, EEMap* inMap, long inType, const EETileCoordinate& inFrom,
                                    const EETileCoordinate& inTo, std::vector<GE3DPoint>& outPath, long inMargin,
                                    bool inArg8, bool* outReached)
{
    std::vector<GE3DPoint> thePath;
    EEAIAStarBounds theBounds;
    EETileCoordinate theFrom = inFrom;
    EETileCoordinate theTo = inTo;
    bool theReached;
    *outReached = false;
    this->ClampTile(inMap, theFrom);
    this->ClampTile(inMap, theTo);
    this->PrepareBoundedMachine(inMap);
    this->PrepareBoundedMachine(inMap);
    theBounds.mTop = inFrom.GetY() - inMargin > 0 ? inFrom.GetY() - inMargin : 0;
    theBounds.mLeft = inFrom.GetX() - inMargin > 0 ? inFrom.GetX() - inMargin : 0;
    theBounds.mRight = inFrom.GetX() + inMargin < inMap->mSize - 1 ? inFrom.GetX() + inMargin : inMap->mSize - 1;
    theBounds.mBottom = inFrom.GetY() + inMargin < inMap->mSize - 1 ? inFrom.GetY() + inMargin : inMap->mSize - 1;
    this->mBoundedMachine->mGoal.mBounds = theBounds;
    this->mBoundedMachine->mSearchBounds = theBounds;
    EEAIPathGoalModifierBase* theModifier;
    if (inType == 2)
        theModifier = new EEAIBPGModifierSea(&this->mBoundedMachine->mGoal);
    else if (inType == 0x10)
        theModifier = new EEAIBPGModifierSpace(&this->mBoundedMachine->mGoal);
    else
        goto start;
    this->mBoundedMachine->mGoal.SetModifier(theModifier);
    theModifier->Dereference();
start:
    this->mBoundedMachine->CancelSearch();
    EEAIAStarBoundedPathGoal& theGoal = this->mBoundedMachine->mGoal;
    theGoal.mMaxRevs = inMargin;
    this->mBoundedMachine->SetStart(theFrom);
    this->mBoundedMachine->SetGoal(theTo);
    theGoal.mDestination = theTo;
    theGoal.mRevLimit = -1;
    theGoal.SetUnit(inUnit);
    this->mBoundedMachine->mGoal.mIsBounded = true;
    this->mBoundedMachine->Run();
    this->mBoundedMachine->GetPoints(thePath);
    theReached = this->mBoundedMachine->mTile.GetX() == theTo.GetX() && this->mBoundedMachine->mTile.GetY() == theTo.GetY();
    *outReached = theReached;
    if (theReached) {
        for (long i = thePath.size() - 1; i >= 0; i--)
            outPath.push_back(thePath[i]);
    }
    theGoal.ReleaseModifier();
    this->mBoundedMachine->Reset();
    return theReached;
}

// Called by EEUnitClass 0x636b69 (cdecl, the third argument 0)
// FUNCTION: EXE 0x4a7414
// When one of the unit's waypoints, last first, lies on a tile of ioPath, appends the waypoints after it to ioPath
// (inArg3 unused)
void EEPathManager::MergeWaypoints(std::vector<GE3DPoint>& ioPath, std::deque<EEComplexUnitWaypoint>& inWaypoints,
                                   long inArg3)
{
    std::vector<GE3DPoint> theWaypoints;
    bool theFound = false;
    theWaypoints.reserve(ioPath.size() > inWaypoints.size() ? ioPath.size() : inWaypoints.size());
    for (long i = inWaypoints.size() - 1; i >= 0 && !theFound; i--) {
        long theY = UMFloor(inWaypoints[i].mLocation.GetY());
        long theX = UMFloor(inWaypoints[i].mLocation.GetX());
        for (long j = ioPath.size() - 1; j >= 0; j--) {
            if (UMFloor(ioPath[j].GetX()) == theX && UMFloor(ioPath[j].GetY()) == theY) {
                theFound = true;
                break;
            }
        }
        if (!theFound)
            theWaypoints.push_back(inWaypoints[i].mLocation);
    }
    if (theFound) {
        for (long k = theWaypoints.size() - 1; k >= 0; k--)
            ioPath.push_back(theWaypoints[k]);
    }
}

// The y test floors the comparison's result, not the coordinate (the original's parenthesis).
// FUNCTION: EXE 0x4a75f4
// Cuts the new path from its last point on the unit's tile on (the part already walked) and drops the unit's path;
// returns whether it cut
bool EEPathManager::TrimPath(EEComplexUnit* inUnit, std::vector<GE3DPoint>& ioPath)
{
    unsigned long theWaypoints = inUnit->mWaypoints.size();
    long theFirst = theWaypoints < ioPath.size() ? theWaypoints : ioPath.size();
    long theX = UMFloor(inUnit->mPhysicalModel.GetLocation().GetX());
    long theY = UMFloor(inUnit->mPhysicalModel.GetLocation().GetY());
    bool theResult = false;
    for (long i = ioPath.size() - 1; i > theFirst; i--) {
        if (UMFloor(ioPath[i].GetX()) != theX || !UMFloor(ioPath[i].GetY() == theY))
            continue;
        theResult = true;
        ioPath.erase(ioPath.begin() + i, ioPath.end());
        inUnit->DropPath();
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x4a777f
// Unless TrimPath cuts the path, appends the unit's last waypoint (when the path does not end on its tile) and its
// other waypoints, last first, then drops the unit's path
void EEPathManager::AppendWaypoints(EEComplexUnit* inUnit, std::vector<GE3DPoint>& ioPath)
{
    if (inUnit == NULL || this->TrimPath(inUnit, ioPath))
        return;
    std::deque<EEComplexUnitWaypoint>& theWaypoints = inUnit->mWaypoints;
    if (ioPath.size() != 0 && theWaypoints.size() != 0) {
        if (UMFloor(ioPath.back().GetX()) != UMFloor(theWaypoints.back().mLocation.GetX()) ||
            UMFloor(ioPath.back().GetY()) != UMFloor(theWaypoints.back().mLocation.GetY()))
            ioPath.push_back(theWaypoints.back().mLocation);
    }
    for (long i = theWaypoints.size() - 2; i >= 0; i--)
        ioPath.push_back(theWaypoints[i].mLocation);
    if (ioPath.size() != 0)
        inUnit->DropPath();
}

// FUNCTION: EXE 0x4a78c1
// Runs a short bounded search toward ioTo (a few steps, more with the queue length and distance), gives the unit
// its result and sets ioTo to the tile reached; returns the goal's took-nearest flag
bool EEPathManager::TryShortPath(EEComplexUnit* inUnit, EEMap* inMap, long inType, const EETileCoordinate& inFrom,
                                 EETileCoordinate& ioTo)
{
    EEPathManagerNode* theNode = this->NewNode();
    this->PrepareBoundedMachine(inMap);
    EEAIAStarPathfindingGoalBase* theGoal = &this->mBoundedMachine->mGoal;
    if (inType == 2) {
        EEAIPathGoalModifierBase* theModifier = new EEAIBPGModifierSea(&this->mBoundedMachine->mGoal);
        theGoal->SetModifier(theModifier);
        theModifier->Dereference();
    } else if (inType == 0x10) {
        EEAIPathGoalModifierBase* theModifier = new EEAIBPGModifierSpace(&this->mBoundedMachine->mGoal);
        theGoal->SetModifier(theModifier);
        theModifier->Dereference();
    }
    theGoal->SetUnit(inUnit);
    theNode->SetUnit(inUnit);
    theNode->mPathMachine = this->mBoundedMachine;
    theNode->mTile = ioTo;
    this->mBoundedMachine->SetStart(inFrom);
    this->mBoundedMachine->SetGoal(ioTo);
    this->mBoundedMachine->mTile = ioTo;
    unsigned long theDistance = UMAbs(inFrom.GetX() - ioTo.GetX()) + UMAbs(inFrom.GetY() - ioTo.GetY());
    if (theDistance > 20)
        theDistance /= 20;
    long theQueued = this->mPaths.size();
    long theRevs = (theQueued > 10 ? 11 : theQueued + 4) + theDistance;
    this->mBoundedMachine->SetMaxRevs(theRevs);
    EEAIAStarBounds theBounds;
    theBounds.mTop = inFrom.GetY() - theRevs - 1 > 0 ? inFrom.GetY() - theRevs - 1 : 0;
    theBounds.mLeft = inFrom.GetX() - theRevs - 1 > 0 ? inFrom.GetX() - theRevs - 1 : 0;
    theBounds.mRight = inFrom.GetX() + theRevs + 1 < inMap->mSize - 1 ? inFrom.GetX() + theRevs + 1 : inMap->mSize - 1;
    theBounds.mBottom = inFrom.GetY() + theRevs + 1 < inMap->mSize - 1 ? inFrom.GetY() + theRevs + 1 : inMap->mSize - 1;
    this->mBoundedMachine->mGoal.mBounds = theBounds;
    this->mBoundedMachine->mSearchBounds = theBounds;
    this->mBoundedMachine->Run();
    this->FinishPath(theNode);
    bool theResult = this->mBoundedMachine->mGoal.mTookNearest;
    ioTo = this->mBoundedMachine->mTile;
    if (inUnit->mIsPathDone != true)
        inUnit->mIsPathDone = true;
    inUnit->mBoundedPathTiles.clear();
    this->mBoundedMachine->Reset();
    this->ReleaseNode(theNode);
    return theResult;
}

// FUNCTION: EXE 0x4a7b03
// Applies a graph path: a unit gets a location goal per sub sector on the way (or inserted before a pending attack),
// a request the sub sectors' tiles and summed distance; returns whether it reached the goal's sub sector
bool EEPathManager::RequestUnitPaths(EEComplexUnit* inUnit, EEAutoPathRequest* inRequest, EEGraphPathMachine* inMachine)
{
    std::vector<long> thePath;
    EETileCoordinate theTile;
    theTile.mX = 0;
    theTile.mY = 0;
    EETileCoordinate theLast;
    theLast.mX = 0;
    theLast.mY = 0;
    long theDistance = 0;
    bool theAttack = false;
    bool theSet = false;
    inMachine->GetPath(thePath);
    if (inUnit) {
        EEUGLocation* theLocation = dynamic_cast<EEUGLocation*>(inUnit->GetGoal());
        std::deque<EEUnitGoal*>& theGoals = inUnit->mGoalQueues[0].mGoals;
        if (inUnit->GetGoal())
            inUnit->GetGoal()->mB1D = true;
        bool theForced = theLocation == NULL || theLocation->mIsForcedMove;
        if (theLocation == NULL) {
            EEUGAttackUnit* theAttackGoal = dynamic_cast<EEUGAttackUnit*>(inUnit->GetGoal());
            if (theAttackGoal && !theAttackGoal->mB1C) {
                theAttack = true;
                if (theGoals.size() <= 1)
                    goto done;
                goto build;
            }
        }
        if (theGoals.size() == 0)
            goto done;
        if (theGoals.back()->mKind == 1 && static_cast<EEUGLocation*>(theGoals.back())->mIsAutoWaypoint)
            goto done;
    build:
        if (thePath.size() > 1 && inUnit->mTile) {
            for (long i = 0; i < (long)thePath.size() - 2; i++) {
                EEAIPathSubSector* theSector = gContinentManager.mPathSubSectors[(unsigned short)thePath[i]];
                theTile = theSector->mLocation;
                EEUGLocation* theGoal = new EEUGLocation(theTile.GetX() + 0.5f, theTile.GetY() + 0.5f, 0.0f,
                                                         inUnit->GetGoal() && inUnit->GetGoal()->mB1C, theForced,
                                                         false, 0.0f, true, 0);
                if (!theAttack) {
                    inUnit->SetGoal(theGoal, 1, 0);
                    theSet = true;
                } else {
                    inUnit->mGoalQueues[0].Insert(1, theGoal);
                    inUnit->RefreshGoals();
                    inUnit->RefreshActiveGoal();
                }
            }
            inUnit->RefreshGoals();
            inUnit->RefreshActiveGoal();
            if (theSet) {
                inUnit->StartAction(kActionPrepareToMove);
                inUnit->mUnitClass->ActionChanged(inUnit);
            }
        }
    } else if (inRequest) {
        inRequest->mPath.clear();
        if (thePath.size() != 0) {
            EEAIPathSubSector* theSector = gContinentManager.GetPathSubSector((unsigned short)thePath.back());
            theLast = theSector->mLocation;
            inRequest->mPath.push_back(theLast);
            if (inMachine->mGoal.mFrom)
                theDistance = UMAbs(inMachine->mGoal.mFrom->mLocation.GetX() - theLast.GetX()) +
                              UMAbs(inMachine->mGoal.mFrom->mLocation.GetY() - theLast.GetY());
        }
        for (long i = thePath.size() - 2; i >= 0; i--) {
            EEAIPathSubSector* theSector = gContinentManager.GetPathSubSector((unsigned short)thePath[i]);
            theTile = theSector->mLocation;
            inRequest->mPath.push_back(theTile);
            theDistance += UMAbs(theTile.GetX() - theLast.GetX()) + UMAbs(theTile.GetY() - theLast.GetY());
        }
        inRequest->mPathLength = theDistance;
        inRequest->mIsDone = true;
    }
done:
    return inMachine->mGoal.mTo->mID == inMachine->mSector;
}

// FUNCTION: EXE 0x4a7e6c
// Applies a finished graph search and, when the unit now has an auto waypoint goal, drops its path and requests a
// tile path to it; returns whether the goal sub sector was reached
bool EEPathManager::FinishGraphPath(EEPathManagerNode* inNode)
{
    EEGraphPathMachine* theMachine = inNode->mGraphMachine;
    EEComplexUnit* theUnit = inNode->mUnit;
    EEAutoPathRequest* theRequest = inNode->mRequest;
    EETileCoordinate theFrom;
    theFrom.mX = 0;
    theFrom.mY = 0;
    EETileCoordinate theTo;
    theTo.mX = 0;
    theTo.mY = 0;
    bool theResult = this->RequestUnitPaths(theUnit, theRequest, theMachine);
    if (theUnit) {
        theFrom = inNode->mUnit->GetTile();
        EEUGLocation* theGoal = dynamic_cast<EEUGLocation*>(theUnit->GetGoal());
        if (theGoal && theGoal->mIsAutoWaypoint && theUnit->mTile) {
            long theY = UMFloor(theGoal->mLocation.GetY());
            long theX = UMFloor(theGoal->mLocation.GetX());
            theTo.mX = theX;
            theTo.mY = theY;
            theUnit->DropPath();
            this->RequestUnitPath(theUnit, theFrom, theTo, false, -1, false, NULL, false);
        }
    }
    return theResult;
}

// NEAR: the original builds the unit's tile through the (const long&, const long&) pair ctor 0xaa817 and tests the
// distance through the out-of-line U2DPoint<long>::DistanceToSquared 0x8a3f (its locals are EETileCoordinates here,
// the type RunBoundedPath and the machines take).
// FUNCTION: EXE 0x4a7f4a
// Gives the unit a failed or blocked path: the points found so far, joined through a short bounded detour (50 revs
// inside 26 tiles) onto its waypoints when one of the last ten points is reached, else with the waypoints appended
void EEPathManager::FinishFailedPath(EEPathManagerNode* inNode)
{
    std::vector<GE3DPoint> thePath;
    std::vector<GE3DPoint> theJoined;
    std::vector<GE3DPoint> theDetour;
    EEAIAStarMachineBase* theMachine = inNode->mPathMachine;
    EEComplexUnit* theUnit = inNode->mUnit;
    EEAIAStarBounds theBounds;
    EETileCoordinate theFrom;
    theFrom.mX = 0;
    theFrom.mY = 0;
    EETileCoordinate theTo;
    theTo.mX = 0;
    theTo.mY = 0;
    long theCount;
    theMachine->GetPoints(thePath);
    if (theUnit->mIsPathDone != true)
        theUnit->mIsPathDone = true;
    theUnit->mBoundedPathTiles.clear();
    if (thePath.size() > theUnit->mWaypoints.size() && (long)theUnit->mWaypoints.size() > 0 &&
        (long)(thePath.size() - theUnit->mWaypoints.size()) >= 0) {
        theFrom = theUnit->GetTile();
        long theY = UMFloor(thePath[(thePath.size() - theUnit->mWaypoints.size())].GetY());
        long theX = UMFloor((*(thePath.begin() + (thePath.size() - theUnit->mWaypoints.size()))).GetX());
        theTo.mX = theX;
        theTo.mY = theY;
        if ((theFrom.GetX() - theTo.GetX()) * (theFrom.GetX() - theTo.GetX()) + (theFrom.GetY() - theTo.GetY()) * (theFrom.GetY() - theTo.GetY()) <= 2)
            this->TrimPath(theUnit, thePath);
    }
    if ((long)theUnit->mWaypoints.size() >= 1 &&
        (thePath.size() > 4 ||
         (thePath.size() != 0 &&
          (UMAbs(UMFloor((*theUnit->mWaypoints.begin()).mLocation.GetX()) - theUnit->GetTileX()) > 1 ||
           UMAbs(UMFloor((*theUnit->mWaypoints.begin()).mLocation.GetY()) - theUnit->GetTileY()) > 1)))) {
        this->PrepareBoundedMachine(gWorld.mMap);
        theCount = thePath.size() < 10 ? thePath.size() : 10;
        long theY = UMFloor((*(thePath.begin() + (thePath.size() - theCount))).GetY());
        long theX = UMFloor(thePath[(thePath.size() - theCount)].GetX());
        theTo.mX = theX;
        theTo.mY = theY;
        theFrom.mX = theUnit->GetTileX();
        theFrom.mY = theUnit->GetTileY();
        theBounds.mTop = theFrom.GetY() - 26 > 0 ? theFrom.GetY() - 26 : 0;
        theBounds.mLeft = theFrom.GetX() - 26 > 0 ? theFrom.GetX() - 26 : 0;
        theBounds.mBottom = theFrom.GetY() + 26 < gWorld.GetMap()->mSize - 1 ? theFrom.GetY() + 26 : gWorld.GetMap()->mSize - 1;
        theBounds.mRight = theFrom.GetX() + 26 < gWorld.GetMap()->mSize - 1 ? theFrom.GetX() + 26 : gWorld.GetMap()->mSize - 1;
        this->mBoundedMachine->mSearchBounds = theBounds;
        this->mBoundedMachine->mGoal.mBounds = theBounds;
        this->mBoundedMachine->mGoal.mMaxRevs = 50;
        EEAIPathGoalModifierBase* theModifier;
        if (theMachine->mType == kAIMachineWaterPath)
            theModifier = new EEAIBPGModifierSea(&this->mBoundedMachine->mGoal);
        else if (theMachine->mType == kAIMachineSpacePath)
            theModifier = new EEAIBPGModifierSpace(&this->mBoundedMachine->mGoal);
        else
            goto run;
        this->mBoundedMachine->mGoal.SetModifier(theModifier);
        theModifier->Dereference();
    run:
        if (this->RunBoundedPath(this->mBoundedMachine, theUnit, theFrom, theTo, 50)) {
            this->mBoundedMachine->mGoal.GetPoints(theDetour);
            if (this->JoinPaths(thePath, theDetour, &theJoined, theCount))
                goto joined;
        }
        if (thePath.size() > 0) {
            this->mBoundedMachine->mGoal.GetPoints(theDetour);
            if (theDetour.size() > 0) {
                long theDetourY = UMFloor(theDetour[0].GetY());
                long theDetourX = UMFloor(theDetour[0].GetX());
                long theIndex = -1;
                for (long i = (thePath.size() < 10 ? thePath.size() : 10) - 1; i >= 0; i--) {
                    if (UMFloor(thePath[i].GetX()) == theDetourX && UMFloor(thePath[i].GetY()) == theDetourY) {
                        theIndex = thePath.size() - i;
                        break;
                    }
                }
                if (theIndex != -1 && this->JoinPaths(thePath, theDetour, &theJoined, theIndex))
                    goto joined;
            }
            this->AppendWaypoints(theUnit, thePath);
            theUnit->TakePath(thePath, true);
        }
        this->mBoundedMachine->Reset();
        goto done;
    joined:
        theUnit->DropPath();
        theUnit->TakePath(theJoined, true);
        this->mBoundedMachine->Reset();
    } else {
        this->AppendWaypoints(theUnit, thePath);
        theUnit->TakePath(thePath, true);
    }
done:
    theUnit->SnapLastWaypoint();
    if (theUnit->GetGoal())
        theUnit->GetGoal()->PathTaken(theUnit);
}

// NEAR: the original computes mPending.begin() + i three times through the out-of-line _Ptrit::operator+ 0x1ba01a
// (two results unused) and keeps i * 12 in a frame slot
// FUNCTION: EXE 0x4a8582
// Requests the pending unit paths again (the world's StartGame after a map change), last first; then none is pending
void EEPathManager::RequestPendingPaths(EEMap* inMap)
{
    for (long i = this->mPending.size() - 1; i >= 0; i--) {
        EEComplexUnit* theUnit = dynamic_cast<EEComplexUnit*>(static_cast<EEUnit*>(gWorld.GetUnit(this->mPending[i].mUnitID)));
        if (theUnit != NULL) {
            theUnit->DropPath();
            EEUGLocation* theGoal = dynamic_cast<EEUGLocation*>(theUnit->GetGoal());
            bool theGraph;
            if (theUnit->GetGoal() != NULL && (theGoal == NULL || !theGoal->mIsAutoWaypoint))
                theGraph = true;
            else
                theGraph = false;
            EETileCoordinate theFrom;
            theFrom.mX = theUnit->mTileX;
            theFrom.mY = theUnit->mTileY;
            this->RequestUnitPath(theUnit, theFrom, this->mPending[i].mTile, false, -1, theGraph, NULL, false);
        }
    }
    this->mPending.clear();
}

// FUNCTION: EXE 0x4a86cb
// Applies a finished tile search: as a failed path unless its goal accepted the nearest tile and did not take it,
// else as a found path
void EEPathManager::FinishPath(EEPathManagerNode* inNode)
{
    EEAIAStarMachineBase* theMachine = inNode->mPathMachine;
    bool theFound;
    bool theBlocked;
    if (theMachine->mType == kAIMachineBoundedPath) {
        theFound = ((EEBoundedPathMachine*)theMachine)->mGoal.mAcceptNearest;
        theBlocked = ((EEBoundedPathMachine*)theMachine)->mGoal.mTookNearest;
    } else {
        theFound = ((EEPathMachine*)theMachine)->mGoal.mAcceptNearest;
        theBlocked = ((EEPathMachine*)theMachine)->mGoal.mTookNearest;
    }
    if (!theFound || theBlocked)
        this->FinishFailedPath(inNode);
    else
        this->FinishFoundPath(inNode);
}

// (the vector clear() 0x688c2c between the flag and SnapLastWaypoint is the unit's mBoundedPathTiles)
// FUNCTION: EXE 0x4a870d
// Gives the unit the found path, marks its path done, clears its bounded path tiles and snaps its last waypoint
void EEPathManager::FinishFoundPath(EEPathManagerNode* inNode)
{
    std::vector<GE3DPoint> thePath;
    EEComplexUnit* theUnit = inNode->mUnit;
    EEAIAStarMachineBase* theMachine = inNode->mPathMachine;
    if (theMachine->mType == kAIMachineBoundedPath)
        ((EEBoundedPathMachine*)theMachine)->GetPoints(thePath);
    else
        ((EEPathMachine*)theMachine)->GetPoints(thePath);
    theUnit->TakePath(thePath, false);
    if (theUnit->mIsPathDone != true)
        theUnit->mIsPathDone = true;
    theUnit->mBoundedPathTiles.clear();
    theUnit->SnapLastWaypoint();
}

// FUNCTION: EXE 0x4a8778
// Frees the bounded machine, the queued searches, spare machines, region maps and history, and resets the node pool
// and the timers
void EEPathManager::Reset()
{
    if (this->mBoundedMachine)
        this->mBoundedMachine->Reset();
    if (this->mBoundedMachine)
        delete this->mBoundedMachine;
    this->mBoundedMachine = NULL;
    this->FreePaths();
    this->FreeMachines();
    this->FreeRegions();
    this->mHistory.clear();
    this->mNodePool.DumpStats(false);
    this->mNodePool.Release();
    this->mNodePool.SetSize(0x32, 0x32, 2);
    this->mNextHistoryPruneTime = 0;
    this->mNextRegionPruneTime = 0;
    this->mLCC = 0;
    this->mNextRequestID = 0;
    this->mIsUpdatingRegions = false;
    this->mNextRegionTime = 0;
}

// FUNCTION: EXE 0x4a8808
// Keeps a graph machine as a spare (up to 25; its modifier and unit released), else deletes it
void EEPathManager::ReleaseGraphMachine(EEGraphPathMachine* inMachine)
{
    inMachine->Reset();
    inMachine->mFound = false;
    if (this->mSpareGraphMachines.size() < 0x19) {
        inMachine->mGoal.ReleaseModifier();
        inMachine->mGoal.SetUnit(0);
        this->mSpareGraphMachines.push_back(inMachine);
    } else {
        delete inMachine;
    }
}

// FUNCTION: EXE 0x4a8861
// Keeps a tile path machine as a spare (up to 35; its modifier and unit released), else deletes it
void EEPathManager::ReleasePathMachine(EEPathMachine* inMachine)
{
    EEPathMachine* theMachine = inMachine;
    theMachine->Reset();
    theMachine->mFound = false;
    if (this->mSparePathMachines.size() < 0x23) {
        theMachine->mGoal.ReleaseModifier();
        theMachine->mGoal.SetUnit(0);
        this->mSparePathMachines.push_back(theMachine);
    } else {
        delete theMachine;
    }
}

// FUNCTION: EXE 0x4a88c3
// Releases the node's unit and returns the node to the pool (its request stays referenced until reuse)
void EEPathManager::ReleaseNode(EEPathManagerNode* inNode)
{
    inNode->ReleaseUnit();
    this->mNodePool.Delete(inNode);
}

// FUNCTION: EXE 0x4a88df
// Drops the unit's queued graph search, if any, returning its machine and node
void EEPathManager::CancelAutoPaths(EEComplexUnit* inUnit)
{
    if (this->mAutoPaths.size() == 0)
        return;
    for (std::deque<EEPathManagerNode*>::iterator it = this->mAutoPaths.begin(); it != this->mAutoPaths.end(); it++) {
        if ((*it)->mUnit != inUnit)
            continue;
        this->ReleaseGraphMachine((*it)->mGraphMachine);
        this->ReleaseNode(*it);
        this->mAutoPaths.erase(it, it + 1);
        return;
    }
}

// FUNCTION: EXE 0x4a896b
// Drops the queued graph search of a path request, if any, returning its machine and node
void EEPathManager::CancelAutoPathRequest(UReferenceCounter* inRequest)
{
    if (this->mAutoPaths.size() == 0)
        return;
    for (std::deque<EEPathManagerNode*>::iterator it = this->mAutoPaths.begin(); it != this->mAutoPaths.end(); it++) {
        if ((*it)->mRequest != inRequest)
            continue;
        this->ReleaseGraphMachine((*it)->mGraphMachine);
        this->ReleaseNode(*it);
        this->mAutoPaths.erase(it, it + 1);
        return;
    }
}

// FUNCTION: EXE 0x4a89f7
// Drops the unit's queued tile search (and its graph search when inAutoPaths)
void EEPathManager::CancelPaths(EEComplexUnit* inUnit, bool inAutoPaths)
{
    if (inAutoPaths)
        this->CancelAutoPaths(inUnit);
    if (this->mPaths.size() == 0)
        return;
    std::deque<EEPathManagerNode*>::iterator it;
    for (it = this->mPaths.begin(); it != this->mPaths.end(); it++) {
        if ((*it)->mUnit != inUnit)
            continue;
        this->ReleasePathMachine((EEPathMachine*)(*it)->mPathMachine);
        this->ReleaseNode(*it);
        this->mPaths.erase(it, it + 1);
        return;
    }
}

// FUNCTION: EXE 0x4a8a8e
// When due and no graph search is queued, rebuilds the continent regions: retried in 0.1 s until it completes, then
// region updates stop
void EEPathManager::UpdateRegions()
{
    if (!(this->mNextRegionTime <= gPhysics->mGameTime) || this->mAutoPaths.size() != 0)
        return;
    if (!gContinentManager.Rebuild()) {
        this->mNextRegionTime = gPhysics->mGameTime + 100;
        return;
    }
    this->mIsUpdatingRegions = false;
}

// FUNCTION: EXE 0x4a8adb
// Frees the bounded machine, the queued searches, spare machines, region maps, history and every node pool
void EEPathManager::Clear()
{
    if (this->mBoundedMachine)
        this->mBoundedMachine->Reset();
    if (this->mBoundedMachine)
        delete this->mBoundedMachine;
    this->mBoundedMachine = NULL;
    this->FreePaths();
    this->FreeMachines();
    this->FreeRegions();
    this->mHistory.clear();
    this->mNodePool.DumpStats(true);
    this->mNodePool.Release();
    EEAIAStarGraphStorage::FreeNodePool();
    EEAIAStarStorage::FreeNodePool();
}

// FUNCTION: EXE 0x4a8b40
// Turns region rebuilds on or off; turning them on schedules the next one now or 10 s (times the game speed) ahead
void EEPathManager::SetRegionUpdates(bool inOn, bool inNow)
{
    if (this->mIsUpdatingRegions == inOn)
        return;
    if (inOn == true) {
        if (inNow)
            this->mNextRegionTime = gPhysics->mGameTime + 1;
        else {
            float theSpeed = gWorld.mGameSpeed;
            unsigned long theTime = gPhysics->mGameTime;
            this->mNextRegionTime = UMFloatToLong(theSpeed * 10000.0f) + theTime;
        }
    }
    this->mIsUpdatingRegions = inOn;
}

// FUNCTION: EXE 0x4a8ba4
// Runs the bounded machine between two tiles for a unit with a step budget, accepting the nearest tile; returns
// whether the search did not fail
bool EEPathManager::RunBoundedPath(EEBoundedPathMachine* inMachine, EEComplexUnit* inUnit, const EETileCoordinate& inFrom,
                                   const EETileCoordinate& inTo, long inRevs)
{
    EEAIAStarBoundedPathGoal& theGoal = inMachine->mGoal;
    theGoal.mMaxRevs = inRevs;
    theGoal.SetUnit(inUnit);
    theGoal.mAcceptNearest = true;
    inMachine->SetStart(inFrom);
    inMachine->SetGoal(inTo);
    inMachine->Run();
    return !theGoal.mSearchFailed;
}

// FUNCTION: EXE 0x4a8bf4
// When inDetour starts on the point inCount from inPath's end, fills outPath with inPath up to that point, then
// inDetour; returns whether it joined them
bool EEPathManager::JoinPaths(std::vector<GE3DPoint>& inPath, std::vector<GE3DPoint>& inDetour,
                              std::vector<GE3DPoint>* outPath, long inCount)
{
    bool theResult = false;
    if (outPath != NULL && inDetour.size() > 0) {
        long theX = UMFloor(inPath[inPath.size() - inCount].GetX());
        long theY = UMFloor(inPath[inPath.size() - inCount].GetY());
        long theDetourX = UMFloor(inDetour[0].GetX());
        long theDetourY = UMFloor(inDetour[0].GetY());
        if (theX == theDetourX && theY == theDetourY) {
            outPath->reserve(inPath.size() + inDetour.size());
            unsigned long i;
            for (i = 0; i < inPath.size() - inCount; i++)
                outPath->push_back(inPath[i]);
            for (i = 0; i < inDetour.size(); i++)
                outPath->push_back(inDetour[i]);
            theResult = true;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x4a8db8
// Advances the queued graph and tile searches and the region rebuild, when there are any
void EEPathManager::Update()
{
    if (this->mAutoPaths.size() != 0)
        this->UpdateAutoPaths();
    if (this->mPaths.size() != 0)
        this->UpdatePaths();
    if (this->mIsUpdatingRegions)
        this->UpdateRegions();
}

// FUNCTION: EXE 0x4a8de8
// Runs the queued graph searches in turn until 32 steps are spent: finished ones are applied and freed, running
// ones get one more step per pass (up to 32) and go to the back
void EEPathManager::UpdateAutoPaths()
{
    EEPathManagerNode* theNode = NULL;
    long theRevs = 0;
    while (this->mAutoPaths.size() != 0 && theRevs < 0x20) {
        theNode = *this->mAutoPaths.begin();
        EEGraphPathMachine* theMachine = theNode->mGraphMachine;
        theMachine->Run();
        bool theRunning = theMachine->mFound;
        theNode->mPasses++;
        if (!theRunning) {
            theRevs += theMachine->mSteps;
            this->FinishGraphPath(theNode);
            this->ReleaseGraphMachine(theMachine);
            this->ReleaseNode(theNode);
        } else {
            theRevs += theMachine->mSteps;
            unsigned long theLimit = theMachine->GetRevLimit();
            if (theLimit < 0x20)
                theMachine->SetRevLimit(theLimit + 1);
            this->mAutoPaths.push_back(theNode);
        }
        this->mAutoPaths.pop_front();
    }
}

// Called every 10 s (UpdatePaths)
// FUNCTION: EXE 0x4a8ea1
// Counts the path requests of the last 5.5 s per unit and goal tile and adds a tile asked for more than once to
// its unit's region map (an existing entry counts up by one); then clears the history (no counting while loading)
void EEPathManager::PruneHistory()
{
    std::vector<std::pair<EEPathHistoryEntry, unsigned long> > theCounts;
    std::deque<EEPathHistoryEntry>::iterator it;
    unsigned long theTime = gPhysics->mGameTime;
    long i;
    if (theTime > 0x157c)
        theTime -= 0x157c;
    if (!gLoadingSavedGame) {
        for (it = this->mHistory.begin(); it != this->mHistory.end(); it++) {
            if ((*it).mTime <= theTime) {
                this->mHistory.erase(it, this->mHistory.end());
                break;
            }
            long theID = (*it).mUnitID;
            for (i = theCounts.size() - 1; i >= 0; i--) {
                if ((theCounts.begin() + i)->first.mUnitID == theID &&
                    (theCounts.begin() + i)->first == *it) {
                    (theCounts.begin() + i)->second++;
                    break;
                }
            }
            if (i == -1)
                theCounts.push_back(std::make_pair(*it, 1));
        }
        for (i = theCounts.size() - 1; i >= 0; i--) {
            if (!(theCounts[i].second > 1))
                continue;
            std::map<long, std::map<U2DPoint<long>, unsigned long>*>::iterator theRegion =
                this->mRegions.find(theCounts[i].first.mUnitID);
            if (theRegion != this->mRegions.end()) {
                std::map<U2DPoint<long>, unsigned long>::iterator theTile =
                    theRegion->second->find(theCounts[i].first);
                if (theTile != theRegion->second->end())
                    theTile->second++;
                else
                    theRegion->second->insert(std::pair<U2DPoint<long>, unsigned long>(
                        theCounts[i].first, theCounts[i].second));
            } else {
                std::map<U2DPoint<long>, unsigned long>* theMap = new std::map<U2DPoint<long>, unsigned long>;
                theMap->insert(std::pair<U2DPoint<long>, unsigned long>(theCounts[i].first,
                    theCounts[i].second));
                this->mRegions.insert(std::make_pair(theCounts[i].first.mUnitID, theMap));
            }
        }
    }
    this->mHistory.clear();
}

// Called every 30 s (UpdatePaths)
// FUNCTION: EXE 0x4a9266
// Cancels the paths of each unit whose region map holds a tile counted 8 or more and tells its player the unit is
// stranded, after freeing every region map (nothing while loading a saved game)
void EEPathManager::PruneRegions()
{
    std::vector<long> theUnitIDs;
    std::map<long, std::map<U2DPoint<long>, unsigned long>*>::iterator it;
    std::map<U2DPoint<long>, unsigned long>::iterator theEntry;
    if (!gLoadingSavedGame) {
        for (it = this->mRegions.begin(); it != this->mRegions.end(); it++) {
            for (theEntry = it->second->begin(); theEntry != it->second->end(); theEntry++) {
                if ((unsigned long)theEntry->second >= 8) {
                    theUnitIDs.push_back(it->first);
                    break;
                }
            }
        }
        this->FreeRegions();
        for (long i = theUnitIDs.size() - 1; i >= 0; i--) {
            long theID = theUnitIDs[i];
            EEComplexUnit* theUnit = dynamic_cast<EEComplexUnit*>(static_cast<EEUnit*>(gWorld.GetUnit(theID)));
            if (theUnit != NULL) {
                this->CancelPaths(theUnit, true);
                theUnit->mPlayer->UnitStranded(theUnit);
            }
        }
    }
}

// FUNCTION: EXE 0x4a9367
// Runs the queued tile searches in turn until 32 steps are spent (finished ones applied and freed), then prunes
// the history every 10 s and the region maps every 30 s
void EEPathManager::UpdatePaths()
{
    EEPathManagerNode* theNode = NULL;
    unsigned long theTime = gPhysics->mGameTime;
    long theRevs = 0;
    while (this->mPaths.size() != 0 && theRevs < 0x20) {
        theNode = *this->mPaths.begin();
        EEAIAStarMachineBase* theMachine = theNode->mPathMachine;
        theMachine->Run();
        bool theRunning = theMachine->mFound;
        theNode->mPasses++;
        if (!theRunning) {
            theRevs += theMachine->mSteps;
            this->FinishPath(theNode);
            this->ReleasePathMachine((EEPathMachine*)theMachine);
            this->ReleaseNode(theNode);
        } else {
            theRevs += theMachine->mSteps;
            unsigned long theLimit = theMachine->GetRevLimit();
            if (theLimit < 0x20)
                theMachine->SetRevLimit(theLimit + 1);
            this->mPaths.push_back(theNode);
        }
        this->mPaths.pop_front();
    }
    if (this->mNextHistoryPruneTime <= theTime) {
        if (this->mHistory.size() > 0)
            this->PruneHistory();
        this->mNextHistoryPruneTime = theTime + 10000;
    }
    if (!(this->mNextRegionPruneTime <= theTime))
        return;
    if (this->mRegions.size() > 0)
        this->PruneRegions();
    this->mNextRegionPruneTime = theTime + 30000;
}

// Compiler-generated for this unit (template COMDATs after the last member, in address order; names in
// analysis/exe/names.txt): the instantiations this unit emits.
// TEMPLATE: EXE 0x4a9472
// TEMPLATE: EXE 0x4a952e
// TEMPLATE: EXE 0x4a9535
// TEMPLATE: EXE 0x4a956f
// TEMPLATE: EXE 0x4a959a
// TEMPLATE: EXE 0x4a95ae
// TEMPLATE: EXE 0x4a9894
// TEMPLATE: EXE 0x4a9901
// TEMPLATE: EXE 0x4a996d
// TEMPLATE: EXE 0x4a9bbb
// TEMPLATE: EXE 0x4a9c2c
// TEMPLATE: EXE 0x4a9eb8
// TEMPLATE: EXE 0x4aa22a
// TEMPLATE: EXE 0x4aa28d
// TEMPLATE: EXE 0x4aa2f0
// TEMPLATE: EXE 0x4aa3d8
// TEMPLATE: EXE 0x4aa8a2
// Named COMDATs of this unit (compiled here; names in analysis/exe/names.txt)
// TEMPLATE: EXE 0x4a96d4
// TEMPLATE: EXE 0x4a96fd
// TEMPLATE: EXE 0x4aa74d
// TEMPLATE: EXE 0x4abb27
// (??_E?$EEAIAStarMachine@VEEAIAStarStorage@@VEEAIAStarPathfindingGoal@@VEEMap@@@@WCA@AEPAXI@Z)

// TEMPLATE: EXE 0x4abb2f
// (??_E?$EEAIAStarMachine@VEEAIAStarBoundedStorage@@VEEAIAStarBoundedPathGoal@@VEEMap@@@@WCA@AEPAXI@Z)

// TEMPLATE: EXE 0x4abb37
// (??_E?$EEAIAStarGraphMachine@VEEAIAStarGraphStorage@@VEEAIAStarPathGraphGoal@@VEEAIPathSubSector@@@@WCI@AEPAXI@Z)

// TEMPLATE: EXE 0x4abb62
// (??_G?$UMemoryPool@VEEPathManagerNode@@@@UAEPAXI@Z)

// TEMPLATE: EXE 0x4abb84

// The A* machines' steps (ee/EEAIAStarMachineSteps.h, ee/EEAIAStarMachine.h) for the path machine
// (EEAIAStarStorage/EEAIAStarPathfindingGoal), the bounded one and the path graph machine.
// TEMPLATE: EXE 0x4a97e2
// TEMPLATE: EXE 0x4a97f5
// TEMPLATE: EXE 0x4a9804
// TEMPLATE: EXE 0x4a9813
// TEMPLATE: EXE 0x4a98ca
// TEMPLATE: EXE 0x4a99d7
// TEMPLATE: EXE 0x4a99f1
// TEMPLATE: EXE 0x4a9a07
// TEMPLATE: EXE 0x4a9a1d
// TEMPLATE: EXE 0x4a9a3e
// TEMPLATE: EXE 0x4a9a54
// TEMPLATE: EXE 0x4a9a78
// TEMPLATE: EXE 0x4aaf29
// TEMPLATE: EXE 0x4ab29e
// TEMPLATE: EXE 0x4aa4e5
// TEMPLATE: EXE 0x4aa562
// TEMPLATE: EXE 0x4aa5b8
// TEMPLATE: EXE 0x4aa915
// TEMPLATE: EXE 0x4aa98d
// TEMPLATE: EXE 0x4aacf0
// TEMPLATE: EXE 0x4aad6c
// TEMPLATE: EXE 0x4aae9b
// TEMPLATE: EXE 0x4aaf4c
// TEMPLATE: EXE 0x4ab407
// TEMPLATE: EXE 0x4ab504
// TEMPLATE: EXE 0x4ab5e6
// TEMPLATE: EXE 0x4ab659
// TEMPLATE: EXE 0x4ab6c1
// TEMPLATE: EXE 0x4ab7e3
// TEMPLATE: EXE 0x4ab8f7
// TEMPLATE: EXE 0x4ab9dc
// TEMPLATE: EXE 0x4aba52
// TEMPLATE: EXE 0x4aba99
// TEMPLATE: EXE 0x4abae0

// The path graph machine's goal hooks (ee/EEAIAStarPathGraphGoal.h). GetGoalID and the node tile are read inline
// (0xaacf0, 0xab6c1).
inline unsigned short EEAIAStarPathGraphGoal::GetGoalID() const
{
    return this->mTo->mID;
}

inline U2DPoint<long> EEAIPathSubSector::GetGraphLocation() const
{
    return U2DPoint<long>(this->mSector->mCenter.mX, this->mSector->mCenter.mY);
}

// FUNCTION: EXE 0x4ab2bd
// Returns the Manhattan distance between two tiles times 10 (ICF-folded with EEAIAStarPathfindingGoalBase's)
long EEAIAStarPathGraphGoal::Estimate(const U2DPoint<long>& inFrom, const U2DPoint<long>& inTo) const
{
    return (UMAbs(inFrom.GetX() - inTo.GetX()) + UMAbs(inFrom.GetY() - inTo.GetY())) * 10;
}

// FUNCTION: EXE 0x4ab2e5
// Returns whether the search may enter a sub sector from another, asking the modifier when there is one
bool EEAIAStarPathGraphGoal::CanEnter(EEAIPathSubSector* inSector, EEAIPathSubSector* inFrom)
{
    if (!this->mModifier)
        return this->AcceptNeighbor(inSector, inFrom);
    return this->mModifier->CanEnter(inSector, inFrom);
}

// FUNCTION: EXE 0x4ab7bb
// Returns the cost of a step into a sub sector: the modifier's when there is one, else the default cost
long EEAIAStarPathGraphGoal::GetStepCost(EEAIAStarGraphNode* inParent, EEAIPathSubSector* inTo)
{
    if (!this->mModifier)
        return this->GetDefaultStepCost(inParent, inTo);
    return this->mModifier->GetCost(inParent, inTo);
}

// (ICF-folded into 0x410799, the wall planner's +0x04): 1 inside one sector, 10 straight, 14 diagonal
long EEAIAStarPathGraphGoal::GetDefaultStepCost(EEAIAStarGraphNode* inParent, EEAIPathSubSector* inTo)
{
    EEAIPathSector* theFrom = static_cast<EEAIPathSubSector*>(inParent->mObject)->mSector;
    EEAIPathSector* theTo = inTo->mSector;
    if (theFrom != theTo)
        return theFrom->mCenter.mX == theTo->mCenter.mX || theFrom->mCenter.mY == theTo->mCenter.mY ? 10 : 14;
    return 1;
}
