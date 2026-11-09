// C:\EEX\Empire Earth\AI Thread\EEAIAStarBoundedPathGoal.cpp (file name inferred: no __FILE__; RTTI class
// EEAIAStarBoundedPathGoal, vtables 0x439b64 / 0x439b8c). The bounded path search's goal: 0xe9a1..0xf53b (EEAIArea's
// COMDATs end at 0xe9a1; the modifiers' unit starts with EEAIPathGoalModifierBase::GetStepCost at 0xf53b). ctor, dtor,
// the members the machine calls, then the COMDATs of their first users: vector reserve/push_back/insert, the goal
// base ctor/??_G, the second base's thunks, EEAIAStarPathfindingGoalBase::ClearSearch, AcceptTile. The twin of
// EEAIAStarPathfindingGoal.cpp (its AcceptDefault/AcceptTile(EETile*) are ICF-folded into that unit's copies).
#include <windows.h>
#include <string.h>
#include <vector>
#include <map>
#include <math.h>
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
#include "lle/FSFile.h"
#include "lle/U2DPoint.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"

#include "ee/EmpireEarth.h"
#include "ee/EEMathInline.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAreaUtilities.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIAStarFastStorage.h"
#include "ee/EEAIAStarTerrainStorage.h"
#include "ee/EEPlayer.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAIPathGoalModifierBase.h"
#include "ee/EEAIAStarGoalBase.h"
#include "ee/EEAIAStarCapture.h"
#include "ee/EEAIAStarPathfindingGoalBase.h"
#include "ee/EEAIAStarBoundedStorage.h"
#include "ee/EEAIBoundedPathGoalModifierBase.h"
#include "ee/EEAIAStarBoundedPathGoal.h"
#include "ee/EEAIAStarMachine.h"
#include "ee/EEUnitUtilities.h"

// 0x248e06 (cdecl): the tile inDistance steps from (inX, inY) in direction inDirection (0..7) (provisional name)

// .rdata 0x439b5c (next to the vtables): a unit's full hit point fraction
static const float kEEBPGFull = 1.0f;

// FUNCTION: EXE 0x40e9a1
// Creates a bounded path goal with no bounded modifier and the large-bounds flag set
EEAIAStarBoundedPathGoal::EEAIAStarBoundedPathGoal()
    : mBoundedModifier(NULL), mB8(true)
{
}

// SYNTHETIC: EXE 0x40e9e5
// (the scalar deleting destructor)

// FUNCTION: EXE 0x40ea01
// Destroys the goal (base members only)
EEAIAStarBoundedPathGoal::~EEAIAStarBoundedPathGoal()
{
}

// FUNCTION: EXE 0x40ea26
// Clears the search and the goal base, empties the search bounds and sets the large-bounds flag again
void EEAIAStarBoundedPathGoal::Reset()
{
    this->ClearSearch();
    EEAIAStarGoalBase<EEAIAStarBoundedStorage, EEMap>::Reset();
    this->mBounds.Clear();
    this->mB8 = true;
}

// New (port): the goal's state as ee_astar's path_goals read it (inMachine: the machine running the search)
EeAStarPathGoal EEAIAStarBoundedPathGoal::PathState(EEAIAStarMachineBase* inMachine)
{
    EeAStarPathGoal theGoal;
    theGoal.bounded = true;
    theGoal.bounds = EEAIAStarRectOf(this->mBounds);
    theGoal.goal_x = this->mGoalTile.mX;
    theGoal.goal_y = this->mGoalTile.mY;
    theGoal.walker = this->mUnit;
    theGoal.modifier = this->mModifier;
    theGoal.continent_id = this->mContinentID;
    theGoal.check_corners = inMachine->mCheckCorners;
    theGoal.accept_nearest = this->mAcceptNearest;
    theGoal.search_failed = this->mSearchFailed;
    theGoal.took_nearest = this->mTookNearest;
    theGoal.revs = inMachine->mRevs;
    theGoal.max_revs = inMachine->GetMaxRevs();
    return theGoal;
}

// FUNCTION: EXE 0x40ea54
// Drops the reference to the bounded modifier, then releases the base goal's modifier
void EEAIAStarBoundedPathGoal::ReleaseModifier()
{
    if (this->mBoundedModifier)
        this->mBoundedModifier->Dereference();
    this->mBoundedModifier = NULL;
    EEAIAStarPathfindingGoalBase::ReleaseModifier();
}

// FUNCTION: EXE 0x40ea7b
// Marks the search failed when the open lists ran empty, when the goal next to the current node cannot be
// entered, or when the machine used up its revolutions; a failure takes the nearest tile if allowed
void EEAIAStarBoundedPathGoal::SearchDone(EEAIAStarMachineBase* inMachine)
{
    // (the machine's hook: the current node, whether an open node is left)
    EeAStarPathGoal theGoal = this->PathState(inMachine);
    EeAStarViewCallbacks theCallbacks = this->mView.Callbacks();
    ee_astar_path_search_done(&theGoal, &theCallbacks, this->mStorage->mHookHasCurrent, this->mStorage->mHookCurrentX,
                              this->mStorage->mHookCurrentY, this->mStorage->mHookHasOpen);
    this->mSearchFailed = theGoal.search_failed;
    this->mTookNearest = theGoal.took_nearest;
}

// NEAR: the original materialises the false result right after the distance tests
// FUNCTION: EXE 0x40eb14
// Returns whether a node ends the search: no node, the goal tile, or a neighbour of a blocked goal (a diagonal
// neighbour also needs both corner tiles enterable); lets the bounded modifier inspect the node
bool EEAIAStarBoundedPathGoal::IsGoalNode(EEAIAStarNode* inNode)
{
    EeAStarPathGoal theGoal = this->PathState(this->mMachine);
    EeAStarViewCallbacks theCallbacks = this->mView.Callbacks();
    bool theResult = ee_astar_path_is_goal_node(&theGoal, &theCallbacks, inNode != NULL, inNode ? inNode->mX : 0,
                                                inNode ? inNode->mY : 0);
    if (this->mModifier)
        this->mBoundedModifier->CheckNode(inNode);
    return theResult;
}

// FUNCTION: EXE 0x40ec0e
// Appends the found path's tiles to outPath
void EEAIAStarBoundedPathGoal::GetPath(std::vector<U2DPoint<long> >& outPath)
{
    long theX;
    long theY;
    if (!this->mStorage->GetFirstPathTile(theX, theY))
        return;
    outPath.push_back(U2DPoint<long>(theX, theY));
    bool theMore;
    do {
        theMore = this->mStorage->GetNextPathTile(theX, theY);
        if (theMore == true)
            outPath.push_back(U2DPoint<long>(theX, theY));
    } while (theMore);
}

// FUNCTION: EXE 0x40ec7d
// Appends the found path to outPath and the tile of every closed and open search node to outVisited
void EEAIAStarBoundedPathGoal::GetPath(std::vector<U2DPoint<long> >& outPath, std::vector<U2DPoint<long> >& outVisited)
{
    this->GetPath(outPath);
    outVisited.reserve(this->mMachine->mRevs);
    this->mStorage->AppendTiles(kAStarListFastClosed, outVisited);
    this->mStorage->AppendTiles(kAStarListFastOpen, outVisited);
}

// NEAR: the original computes the point before the loop head (rotated loop)
// FUNCTION: EXE 0x40ecf0
// Appends the found path to outPath as tile-centre points
void EEAIAStarBoundedPathGoal::GetPoints(std::vector<GE3DPoint>& outPath)
{
    long theY;
    long theX;
    if (!this->mStorage->GetFirstPathTile(theX, theY))
        return;
    GE3DPoint thePoint(theX + 0.5f, theY + 0.5f, 0.0f);
    for (;;) {
        outPath.push_back(thePoint);
        if (!this->mStorage->GetNextPathTile(theX, theY))
            break;
        thePoint = GE3DPoint(theX + 0.5f, theY + 0.5f, 0.0f);
    }
}

// FUNCTION: EXE 0x40ed71
// Appends the found path to outPath as tile-centre points and every closed and open node's tile to outVisited
void EEAIAStarBoundedPathGoal::GetPoints(std::vector<GE3DPoint>& outPath, std::vector<U2DPoint<long> >& outVisited)
{
    this->GetPoints(outPath);
    outVisited.reserve(this->mMachine->mRevs);
    this->mStorage->AppendTiles(kAStarListFastClosed, outVisited);
    this->mStorage->AppendTiles(kAStarListFastOpen, outVisited);
}

// NEAR: the original loads the parent tile into registers before the vtable
// FUNCTION: EXE 0x40ede4
// Returns the modifier's step cost (given the current node's parent tile, or -10000,-10000 without one),
// else the default cost
long EEAIAStarBoundedPathGoal::GetCost(long inFromX, long inFromY, long inX, long inY)
{
    // (the machine's hook: the current node's parent)
    EeAStarPathGoal theGoal = this->PathState(this->mMachine);
    EeAStarViewCallbacks theCallbacks = this->mView.Callbacks();
    return ee_astar_path_cost(&theGoal, &theCallbacks, inFromX, inFromY, inX, inY, this->mStorage->mHookHasParent,
                              this->mStorage->mHookParentX, this->mStorage->mHookParentY);
}

// 0xee51 GetDefaultCost: ee_astar path_goals::default_cost

// FUNCTION: EXE 0x40f1c8
// Prepares the search: bounds it to the start and goal tiles plus 3, accepts the nearest tile, and sets the
// large-bounds flag when the bounds exceed 20 tiles both ways; returns true
bool EEAIAStarBoundedPathGoal::Prepare()
{
    EEAIAStarPathfindingGoalBase::Prepare();
    this->SetStartBounds(this->mStartTile, 3);
    this->SetGoalBounds(this->mGoalTile, 3);
    this->mAcceptNearest = true;
    this->mStorage->mSearchBounds = this->mBounds;
    this->mB8 = this->mBounds.mRight - this->mBounds.mLeft > 20 && this->mBounds.mBottom - this->mBounds.mTop > 20;
    return true;
}

// FUNCTION: EXE 0x40f251
// Replaces the goal's modifier and keeps a reference to it as the bounded modifier (it must be one)
void EEAIAStarBoundedPathGoal::SetModifier(EEAIPathGoalModifierBase* inModifier)
{
    this->ReleaseModifier();
    EEAIAStarPathfindingGoalBase::SetModifier(inModifier);
    this->mBoundedModifier = dynamic_cast<EEAIBoundedPathGoalModifierBase*>(inModifier);
    this->mBoundedModifier->Reference();
    this->mBoundedModifier->SetGoal(this);
}

// FUNCTION: EXE 0x40f298
// Collects the blocked tiles around the goal and marks each one closed in the search storage
void EEAIAStarBoundedPathGoal::MarkBlockedTiles()
{
    this->CollectBlockedTiles(this->mGoalTile);
    for (long i = this->mBlockedTiles.size() - 1; i >= 0; i--)
        this->mStorage->MarkTile(this->mBlockedTiles[i].mX, this->mBlockedTiles[i].mY, false);
}

// AcceptDefault: ee_astar path_goals::accept_tile

// AcceptOnMap: ee_astar path_goals::accept_tile

// FUNCTION: EXE 0x40f4bc
// Returns whether the unit may enter a map tile that lies inside the search bounds
bool EEAIAStarBoundedPathGoal::AcceptTile(long inX, long inY, bool inGoal)
{
    EeAStarPathGoal theGoal = this->PathState(this->mMachine);
    EeAStarViewCallbacks theCallbacks = this->mView.Callbacks();
    return ee_astar_path_accept(&theGoal, &theCallbacks, inX, inY, inGoal);
}

// Compiler-generated for this unit (first users here): vector<U2DPoint<long> >::reserve 0xf2dc, vector<GE3DPoint>::
// push_back 0xf38e / insert 0xf3f4 / _Ufill 0xf44e, the goal base ctor 0xf3ba and ??_G 0xf51e, the second base's
// thunks 0xf479 (??_E) / 0xf481 (Virtual0C), EEAIAStarPathfindingGoalBase::ClearSearch 0xf489.
// TEMPLATE: EXE 0x40f2dc
// TEMPLATE: EXE 0x40f38e
// TEMPLATE: EXE 0x40f3ba
// TEMPLATE: EXE 0x40f3f4
// TEMPLATE: EXE 0x40f44e
// TEMPLATE: EXE 0x40f479
// TEMPLATE: EXE 0x40f481
// TEMPLATE: EXE 0x40f489
// TEMPLATE: EXE 0x40f51e
