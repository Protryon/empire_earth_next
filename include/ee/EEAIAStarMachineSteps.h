// EEAIAStarMachine<S, G, M>::Run and its flood-fill steps (COMDATs 0x1ceb1..0x1d6b1 for EEAIChokePoint's machine, twins
// 0x268be.. for EEAIContinentManager's four, EEPathManager's 0xaa4e5..). Moved from EEAIChokePoint.cpp so every
// instantiating unit emits them. Inferred header: they belong in ee/EEAIAStarMachine.h once
// the storages share one AddOpen/RemoveBestOpen/AddClosed API.
#ifndef EE_EEAIASTARMACHINESTEPS_H
#define EE_EEAIASTARMACHINESTEPS_H
#include "ee/EEAIKindIDs.h"
#include "ee/EEAIAStarCapture.h"
#include "rust/ee_astar.h"
template <class S, class G, class M>
void EEAIAStarMachine<S, G, M>::Run()
{
    EEAIAStarCaptureRun theCapture;         // TEST ONLY (EE_SIM_CAPTURE): this run's record
    if (theCapture.IsOn())
        this->CaptureInputs(theCapture);
    if (!this->mFound) {
        this->mNumRuns = 0;
        this->mMapWidth = this->mMap->mSize;
        this->mMapHeight = this->mMap->mSize;
        this->mRevs = 0;
        Init(this->mMap->mSize, this->mMap->mSize);
        this->mGoal.Attach(this->mMap, this, this);
        this->mGoal.Prepare();
        this->Start();
        this->Expand();
    } else
        this->Expand();
    if (theCapture.IsOn()) {
        this->CaptureOutputs(theCapture);
        theCapture.Commit();
    }
}

// New (port), TEST ONLY: the search's inputs as Run finds them
template <class S, class G, class M>
void EEAIAStarMachine<S, G, M>::CaptureInputs(EEAIAStarCaptureRun& ioRun)
{
    ioRun.I32(this->mType);
    ioRun.U8(this->mFound);
    ioRun.U8(this->mCheckCorners);
    ioRun.U8(this->mGoal.mAcceptNearest);
    ioRun.I32(this->mGoal.mStartTile.mX);
    ioRun.I32(this->mGoal.mStartTile.mY);
    ioRun.I32(this->mGoal.mGoalTile.mX);
    ioRun.I32(this->mGoal.mGoalTile.mY);
    ioRun.I32(this->mGoal.mRevLimit);
    ioRun.I32(this->mGoal.mMaxRevs);
    ioRun.I32(this->mMap->mSize);
}

// New (port), TEST ONLY: the search's outputs: the machine's members, the goal's flags, the storage's nodes
template <class S, class G, class M>
void EEAIAStarMachine<S, G, M>::CaptureOutputs(EEAIAStarCaptureRun& ioRun)
{
    ioRun.U8(this->mFound);
    ioRun.U8(this->mCheckCorners);
    ioRun.I32(this->mRevs);
    ioRun.I32(this->mSteps);
    ioRun.I32(this->mNumRuns);
    ioRun.I32(this->mTile.mX);
    ioRun.I32(this->mTile.mY);
    ioRun.U8(this->mGoal.mSearchFailed);
    ioRun.U8(this->mGoal.mTookNearest);
    ioRun.I32(this->mGoal.mStartTile.mX);
    ioRun.I32(this->mGoal.mStartTile.mY);
    ioRun.I32(this->mGoal.mGoalTile.mX);
    ioRun.I32(this->mGoal.mGoalTile.mY);
    EEAIAStarStorage* theStorage = this;
    EEAIAStarNodeView theNode;
    for (long i = 0; i < 2; i++) {
        bool theHas = theStorage->GetNodeView(i, theNode);
        ioRun.U8(theHas);
        if (theHas)
            ioRun.Node(theNode);
    }
    EEAIAStarBounds theBounds = theStorage->GetSearchBounds();
    ioRun.I32(theBounds.mBottom);
    ioRun.I32(theBounds.mLeft);
    ioRun.I32(theBounds.mRight);
    ioRun.I32(theBounds.mTop);
    ioRun.U8(theStorage->GetHasBounds());
    std::vector<EEAIAStarNodeView> theNodes;
    for (long theList = kAStarListSortedOpen; theList <= kAStarListClosedTable; theList++) {
        theNodes.clear();
        theStorage->ListNodes(theList, theNodes);
        ioRun.Nodes(theNodes);
    }
}

// New (port): the goal and the machine's members behind the Rust machine's callbacks (ee_astar's EeAStarCallbacks).
// Each asks the goal what the C++ machine asked it, at the same point of the search, and notes it for the capture as
// the C++ machine did. What a goal reads of the storage meanwhile comes in the arguments (the storage's mHook*).
template <class S, class G, class M>
struct EEAIAStarHooks {
    typedef EEAIAStarMachine<S, G, M> Machine;

    static bool AcceptTile(void* inMachine, int32_t inX, int32_t inY, bool inFlag)
    {
        Machine* theMachine = (Machine*)inMachine;
        return EEAIAStarNote(kAStarEventAccept, inX, inY, inFlag, 0, 0, theMachine->mGoal.AcceptTile(inX, inY, inFlag)) != 0;
    }
    static int32_t GetCost(void* inMachine, int32_t inFromX, int32_t inFromY, int32_t inX, int32_t inY, bool inHasParent,
                           int32_t inParentX, int32_t inParentY)
    {
        Machine* theMachine = (Machine*)inMachine;
        theMachine->mHookHasParent = inHasParent;
        theMachine->mHookParentX = inParentX;
        theMachine->mHookParentY = inParentY;
        return EEAIAStarNote(kAStarEventCost, inFromX, inFromY, inX, inY, 0,
                             theMachine->mGoal.GetCost(inFromX, inFromY, inX, inY));
    }
    static int32_t Estimate(void* inMachine, int32_t inFromX, int32_t inFromY, int32_t inToX, int32_t inToY)
    {
        Machine* theMachine = (Machine*)inMachine;
        return EEAIAStarNote(kAStarEventEstimate, inFromX, inFromY, inToX, inToY, 0,
                             theMachine->mGoal.Estimate(U2DPoint<long>(inFromX, inFromY), U2DPoint<long>(inToX, inToY)));
    }
    static bool IsGoalNode(void* inMachine, bool inHasNode, int32_t inX, int32_t inY)
    {
        Machine* theMachine = (Machine*)inMachine;
        EEAIAStarNode theNode;
        memset(&theNode, 0, sizeof(theNode));
        theNode.mX = inX;
        theNode.mY = inY;
        return EEAIAStarNote(kAStarEventGoalNode, inHasNode, inX, inY, 0, 0,
                             theMachine->mGoal.IsGoalNode(inHasNode ? &theNode : NULL)) != 0;
    }
    static void SearchDone(void* inMachine, bool inHasCurrent, int32_t inX, int32_t inY, bool inHasOpen)
    {
        Machine* theMachine = (Machine*)inMachine;
        theMachine->mHookHasCurrent = inHasCurrent;
        theMachine->mHookCurrentX = inX;
        theMachine->mHookCurrentY = inY;
        theMachine->mHookHasOpen = inHasOpen;
        theMachine->mGoal.SearchDone(theMachine);
        EEAIAStarNote(kAStarEventSearchDone, theMachine->mGoal.mSearchFailed, theMachine->mGoal.mTookNearest, 0, 0, 0, 0);
    }
    static bool SearchFailed(void* inMachine) { return ((Machine*)inMachine)->mGoal.mSearchFailed; }
    static bool AcceptNearest(void* inMachine) { return ((Machine*)inMachine)->mGoal.mAcceptNearest; }
    static bool IsStepLimit(void* inMachine, int32_t inSteps) { return ((Machine*)inMachine)->mGoal.IsStepLimit(inSteps); }
    static bool IsRevLimit(void* inMachine, int32_t inRevs) { return ((Machine*)inMachine)->mGoal.IsRevLimit(inRevs); }
    static bool IsCheaper(void* inMachine, uint32_t inCost, uint32_t inOld)
    {
        return ((Machine*)inMachine)->mGoal.IsCheaper(inCost, inOld);
    }
    static void Tiles(void* inMachine, int32_t* outTiles)
    {
        Machine* theMachine = (Machine*)inMachine;
        outTiles[0] = theMachine->mGoal.mStartTile.mX;
        outTiles[1] = theMachine->mGoal.mStartTile.mY;
        outTiles[2] = theMachine->mGoal.mGoalTile.mX;
        outTiles[3] = theMachine->mGoal.mGoalTile.mY;
    }
    static void GetState(void* inMachine, EeAStarMachineState* outState)
    {
        Machine* theMachine = (Machine*)inMachine;
        outState->tile_x = theMachine->mTile.mX;
        outState->tile_y = theMachine->mTile.mY;
        outState->revs = theMachine->mRevs;
        outState->num_runs = theMachine->mNumRuns;
        outState->steps = theMachine->mSteps;
        outState->check_corners = theMachine->mCheckCorners;
        outState->found = theMachine->mFound;
        outState->map_width = theMachine->mMapWidth;
        outState->map_height = theMachine->mMapHeight;
    }
    static void SetState(void* inMachine, const EeAStarMachineState* inState)
    {
        Machine* theMachine = (Machine*)inMachine;
        theMachine->mTile.mX = inState->tile_x;
        theMachine->mTile.mY = inState->tile_y;
        theMachine->mRevs = inState->revs;
        theMachine->mNumRuns = inState->num_runs;
        theMachine->mSteps = inState->steps;
        theMachine->mCheckCorners = inState->check_corners;
        theMachine->mFound = inState->found;
    }
};

template <class S, class G, class M>
EeAStarCallbacks EEAIAStarMachine<S, G, M>::Callbacks()
{
    typedef EEAIAStarHooks<S, G, M> Hooks;
    EeAStarCallbacks theCallbacks;
    theCallbacks.ctx = this;
    theCallbacks.accept_tile = Hooks::AcceptTile;
    theCallbacks.get_cost = Hooks::GetCost;
    theCallbacks.estimate = Hooks::Estimate;
    theCallbacks.is_goal_node = Hooks::IsGoalNode;
    theCallbacks.search_done = Hooks::SearchDone;
    theCallbacks.search_failed = Hooks::SearchFailed;
    theCallbacks.accept_nearest = Hooks::AcceptNearest;
    theCallbacks.is_step_limit = Hooks::IsStepLimit;
    theCallbacks.is_rev_limit = Hooks::IsRevLimit;
    theCallbacks.is_cheaper = Hooks::IsCheaper;
    theCallbacks.tiles = Hooks::Tiles;
    theCallbacks.get_state = Hooks::GetState;
    theCallbacks.set_state = Hooks::SetState;
    return theCallbacks;
}

// 0x1d184 (Rust: ee_astar machine.rs start)
template <class S, class G, class M>
bool EEAIAStarMachine<S, G, M>::Start()
{
    EeAStarCallbacks theCallbacks = this->Callbacks();
    return ee_astar_start(this->mCore, &theCallbacks);
}

// 0x1d1fb (Rust: ee_astar machine.rs expand, with Restart, TestTile, NextNode and AddNode)
template <class S, class G, class M>
void EEAIAStarMachine<S, G, M>::Expand()
{
    EeAStarCallbacks theCallbacks = this->Callbacks();
    ee_astar_expand(this->mCore, &theCallbacks);
}

#endif
