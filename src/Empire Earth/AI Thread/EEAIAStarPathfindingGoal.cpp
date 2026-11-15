// C:\EEX\Empire Earth\AI Thread\EEAIAStarPathfindingGoal.cpp (file name inferred: no __FILE__; RTTI class
// EEAIAStarPathfindingGoal, vtables 0x439ffc / 0x43a014). The unbounded path search's goal: 0x190bf..0x19a93
// (EEAIAStarPathAreaStorage's dtor ends at 0x190bf; EEAIAStarPathfindingGoalBase's ctor starts at 0x19a93). ctor, the
// members the machine calls, then the COMDATs: the goal base ctor/??_G and the second base's adjustor thunks. The
// twin of EEAIAStarBoundedPathGoal.cpp (same bodies over the same storage layout; Virtual10 ICF-folded into its
// 0xf298).
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
#include "ee/EEAIAStarPathfindingGoal.h"
#include "ee/EEAIAStarMachine.h"
#include "ee/EEUnitUtilities.h"

// 0x248e06 (cdecl): the tile inDistance steps from (inX, inY) in direction inDirection (0..7) (provisional name)

// .rdata 0x439ff4 (next to the vtables): a unit's full hit point fraction
static const float kEEPFGFull = 1.0f;

// FUNCTION: EXE 0x4190bf
// Creates the unbounded unit path search goal (bases only)
EEAIAStarPathfindingGoal::EEAIAStarPathfindingGoal()
{
}

// SYNTHETIC: EXE 0x4190e3
// (the scalar deleting destructor)

// FUNCTION: EXE 0x4190ff
// Destroys the goal (bases only)
EEAIAStarPathfindingGoal::~EEAIAStarPathfindingGoal()
{
}

// FUNCTION: EXE 0x419124
// Clears the last search's state and resets the goal base
void EEAIAStarPathfindingGoal::Reset()
{
    this->ClearSearch();
    EEAIAStarGoalBase<EEAIAStarStorage, EEMap>::Reset();
}

// New (port): the goal's state as ee_astar's path_goals read it (inMachine: the machine running the search)
EeAStarPathGoal EEAIAStarPathfindingGoal::PathState(EEAIAStarMachineBase* inMachine)
{
    EeAStarPathGoal theGoal;
    EeAStarRect theNoBounds = {0, 0, 0, 0};
    theGoal.bounded = false;
    theGoal.bounds = theNoBounds;
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

// FUNCTION: EXE 0x419138
// Marks the search failed when the open lists run dry, when the reached tile next to the goal is not enterable,
// or when the machine has used up its revolutions
void EEAIAStarPathfindingGoal::SearchDone(EEAIAStarMachineBase* inMachine)
{
    // (the machine's hook: the current node, whether an open node is left)
    EeAStarPathGoal theGoal = this->PathState(inMachine);
    EeAStarViewCallbacks theCallbacks = this->mView.Callbacks();
    ee_astar_path_search_done(&theGoal, &theCallbacks, this->mStorage->mHookHasCurrent, this->mStorage->mHookCurrentX,
                              this->mStorage->mHookCurrentY, this->mStorage->mHookHasOpen);
    this->mSearchFailed = theGoal.search_failed;
    this->mTookNearest = theGoal.took_nearest;
}

// FUNCTION: EXE 0x4191d1
// Returns true for no node, the goal tile, or a tile next to an unenterable goal (diagonals only when both corner
// tiles are enterable while corner checks are on)
bool EEAIAStarPathfindingGoal::IsGoalNode(EEAIAStarNode* inNode)
{
    EeAStarPathGoal theGoal = this->PathState(this->mMachine);
    EeAStarViewCallbacks theCallbacks = this->mView.Callbacks();
    bool theResult = ee_astar_path_is_goal_node(&theGoal, &theCallbacks, inNode != NULL, inNode ? inNode->mX : 0,
                                                inNode ? inNode->mY : 0);
    return theResult;
}

// FUNCTION: EXE 0x41929e
// Appends the found path's tiles in order
void EEAIAStarPathfindingGoal::GetPath(std::vector<U2DPoint<long> >& outPath)
{
    long theX = 0;
    long theY = 0;
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

// FUNCTION: EXE 0x419315
// Appends the found path's tiles, and every tile on the fast closed and open lists to outVisited
void EEAIAStarPathfindingGoal::GetPath(std::vector<U2DPoint<long> >& outPath, std::vector<U2DPoint<long> >& outVisited)
{
    this->GetPath(outPath);
    outVisited.reserve(this->mMachine->mRevs);
    this->mStorage->AppendTiles(kAStarListFastClosed, outVisited);
    this->mStorage->AppendTiles(kAStarListFastOpen, outVisited);
}

// NEAR: the original adds the offsets as fld member + faddp
// FUNCTION: EXE 0x419388
// Appends the found path as world points (tile plus the goal's offset, height 0)
void EEAIAStarPathfindingGoal::GetPoints(std::vector<GE3DPoint>& outPath)
{
    long theX = 0;
    long theY = 0;
    if (!this->mStorage->GetFirstPathTile(theX, theY))
        return;
    outPath.push_back(GE3DPoint(theX + this->mOffsetX, theY + this->mOffsetY, 0.0f));
    bool theMore;
    do {
        theMore = this->mStorage->GetNextPathTile(theX, theY);
        if (theMore == true)
            outPath.push_back(GE3DPoint(theX + this->mOffsetX, theY + this->mOffsetY, 0.0f));
    } while (theMore);
}

// FUNCTION: EXE 0x419429
// Appends the found path as world points, and every tile on all open and closed lists to outVisited
void EEAIAStarPathfindingGoal::GetPoints(std::vector<GE3DPoint>& outPath, std::vector<U2DPoint<long> >& outVisited)
{
    this->GetPoints(outPath);
    outVisited.reserve(this->mMachine->mRevs);
    this->mStorage->AppendTiles(kAStarListSortedOpen, outVisited);
    this->mStorage->AppendTiles(kAStarListFastClosed, outVisited);
    this->mStorage->AppendTiles(kAStarListClosedTable, outVisited);
    this->mStorage->AppendTiles(kAStarListFastOpen, outVisited);
    this->mStorage->AppendTiles(kAStarListOpenTable, outVisited);
}

// NEAR: the original loads the parent tile into registers before the vtable
// FUNCTION: EXE 0x419528
// Returns the step cost between two tiles: the modifier's cost (given the current node's parent, or -10000s)
// when one is set, else the default cost
long EEAIAStarPathfindingGoal::GetCost(long inFromX, long inFromY, long inX, long inY)
{
    // (the machine's hook: the current node's parent)
    EeAStarPathGoal theGoal = this->PathState(this->mMachine);
    EeAStarViewCallbacks theCallbacks = this->mView.Callbacks();
    return ee_astar_path_cost(&theGoal, &theCallbacks, inFromX, inFromY, inX, inY, this->mStorage->mHookHasParent,
                              this->mStorage->mHookParentX, this->mStorage->mHookParentY);
}

// 0x19595 GetDefaultCost: ee_astar path_goals::default_cost

// AcceptDefault: ee_astar path_goals::accept_tile

// FUNCTION: EXE 0x419983
// Prepares the base and sets 3-tile start and goal bounds around the start and goal tiles; always returns true
bool EEAIAStarPathfindingGoal::Prepare()
{
    EEAIAStarPathfindingGoalBase::Prepare();
    this->SetStartBounds(this->mStartTile, 3);
    this->SetGoalBounds(this->mGoalTile, 3);
    return true;
}

// FUNCTION: EXE 0x4199fa
// Returns whether the unit can enter tile (inX, inY); false off the map
bool EEAIAStarPathfindingGoal::AcceptTile(long inX, long inY, bool inGoal)
{
    EeAStarPathGoal theGoal = this->PathState(this->mMachine);
    EeAStarViewCallbacks theCallbacks = this->mView.Callbacks();
    return ee_astar_path_accept(&theGoal, &theCallbacks, inX, inY, inGoal);
}

// AcceptOnMap: ee_astar path_goals::accept_tile

// Second base +0x10 (no address: ICF-folded into EEAIAStarBoundedPathGoal's identical 0xf298)
// Collects the tiles blocked around the goal and marks each one in the search storage
void EEAIAStarPathfindingGoal::MarkBlockedTiles()
{
    this->CollectBlockedTiles(this->mGoalTile);
    for (long i = this->mBlockedTiles.size() - 1; i >= 0; i--)
        this->mStorage->MarkTile(this->mBlockedTiles[i].mX, this->mBlockedTiles[i].mY, false);
}

// Compiler-generated for this unit: EEAIAStarGoalBase<EEAIAStarStorage, EEMap>'s ctor
// 0x199b0 and ??_G 0x19a76, the second base's adjustor thunks 0x199ea (??_E) / 0x199f2 (Virtual0C).
// TEMPLATE: EXE 0x4199b0
// TEMPLATE: EXE 0x4199ea
// TEMPLATE: EXE 0x4199f2
// TEMPLATE: EXE 0x419a76
