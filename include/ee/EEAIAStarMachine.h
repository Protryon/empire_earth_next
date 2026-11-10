// C:\EEX\Empire Earth\AI Thread\EEAIAStarMachine.h (RECONSTRUCTION; file name inferred).
// The AI's A* search machines: a goal-independent base (EEAIAStarMachineBase, vtable 0x43a17c, 16 slots; ctor 0x1d700;
// EEAIAStarGraphMachineBase, vtable 0x43a2e0, 12 slots; ctor 0x262ba) and the templates that join a node storage S
// (second base, at +0x20 / +0x28), a goal G and a map M. Instantiations (EEPathManager.cpp; RTTI, multiple inheritance):
//   EEAIAStarMachine<EEAIAStarStorage, EEAIAStarPathfindingGoal, EEMap>          vtables 0x43c46c/0x43c4b0, sizeof 0x11c
//   EEAIAStarMachine<EEAIAStarBoundedStorage, EEAIAStarBoundedPathGoal, EEMap>   vtables 0x43c514/0x43c558, sizeof 0x148
//   EEAIAStarGraphMachine<EEAIAStarGraphStorage, EEAIAStarPathGraphGoal, EEAIPathSubSector> 0x43c4c8/0x43c4fc, 0xd8
// The base slots are pure or trivial (+0x04 empty, +0x08/+0x1c return 0, +0x24/+0x30 empty with one argument).
// Slots named by vtable offset, members by offset (provisional names). Requires <vector>, lle/U2DPoint.h,
// lle/GE3DPoint.h,
// ee/EETileCoordinate.h and the storage/goal headers of the instantiation used.
#ifndef EE_EEAIASTARMACHINE_H
#define EE_EEAIASTARMACHINE_H

#include "rust/ee_rust.h"   // EeAStarCallbacks, EeAStarGraphCallbacks (the Rust search steps)
#include "ee/EEAIAStarCapture.h"   // TEST ONLY: the graph machine's capture notes
#include "ee/EEAIAStarGraphStorage.h"   // EEAIAStarGraphNode (the graph hooks' node views)
class EEAIAStarCaptureRun;              // ee/EEAIAStarCapture.h
class AStarGraphableObjectBase;         // ee/AStarGraphableObjectBase.h

class EEAIAStarMachineBase {
public:
    EEAIAStarMachineBase(long inType)       // 0x1d700 (inline COMDAT in EEAIChokePoint.cpp's range; mTile: U2DPoint())
        : mType(inType), mRevs(0), mNumRuns(0), mSteps(0), mCheckCorners(false), mFound(false)
    {
    }
    virtual ~EEAIAStarMachineBase() {}      // +0x00 (??_G 0x1d72e)
    virtual void Reset() {}                 // +0x04
    virtual long GetMaxRevs() { return 0; } // +0x08
    virtual void GetPoints(std::vector<GE3DPoint>& outPath) = 0;   // +0x10
    virtual void GetPoints(std::vector<GE3DPoint>& outPath, std::vector<U2DPoint<long> >& outVisited) = 0;   // +0x0c (the goal's 0x19429 takes the visited tiles)
    virtual void GetPath(std::vector<U2DPoint<long> >& outPath, std::vector<U2DPoint<long> >& outVisited) = 0;    // +0x18
    virtual void GetPath(std::vector<U2DPoint<long> >& outPath) = 0;                // +0x14
    virtual long GetRevLimit() { return 0; }                // +0x1c
    virtual void Run() = 0;                 // +0x20 (sets mFound)
    virtual void SetMaxRevs(long inRevs) {} // +0x24
    virtual void SetGoal(long inX, long inY) = 0;               // +0x2c
    virtual void SetGoal(const EETileCoordinate& inTile) = 0;   // +0x28
    virtual void SetRevLimit(long inLimit) {}                   // +0x30
    virtual void CancelSearch() { this->mFound = false; }  // +0x34 0x1d729 (inline COMDAT)
    virtual void SetStart(long inX, long inY) = 0;              // +0x3c
    virtual void SetStart(const EETileCoordinate& inTile) = 0;  // +0x38

    EETileCoordinate mTile;                 // +0x04 (the goal tile)
    long GetType() const { return this->mType; }
    long mType;                             // +0x0c (2: land, 0x10: sea/space)
    long GetRevs() const { return this->mRevs; }
    long mRevs;                             // +0x10
    long mNumRuns;                          // +0x14
    long mSteps;                            // +0x18
    bool mCheckCorners;                     // +0x1c
    bool mFound;                            // +0x1d
};

class EEAIAStarGraphMachineBase {
public:
    EEAIAStarGraphMachineBase(long inType, long inNodeCount)  // 0x262ba (inline COMDAT in EEAIContinentManager.cpp's range)
        : mTile(0, 0), mType(inType), mRevs(0), mNumRuns(0), mSteps(0), mSector(0), mNodeCount(inNodeCount), mFound(false)
    {
    }
    virtual ~EEAIAStarGraphMachineBase() {} // +0x00 (??_G 0x262ef)
    virtual void Reset() {}                 // +0x04
    virtual long GetMaxRevs() { return 0; } // +0x08
    virtual void GetPath(std::vector<U2DPoint<long> >& outTiles, std::vector<long>& outIndices) = 0;   // +0x10
    virtual void GetPath(std::vector<long>& outPath) = 0;   // +0x0c (declared second: VC6 slots overloads in reverse order; vtable 0x43c4c8 +0x0c = 0xa98ca, RequestUnitPaths 0x4a7b3d calls +0x0c)
    virtual long GetRevLimit() { return 0; }                // +0x14
    virtual void Run() = 0;                 // +0x18 (sets mFound)
    virtual void SetMaxRevs(long inRevs) {} // +0x1c
    virtual void SetStart(const EETileCoordinate& inTile, long inSector) = 0;   // +0x20
    virtual void SetRevLimit(long inLimit) {}                   // +0x24
    virtual void CancelSearch() { this->mFound = false; }  // +0x28 0x262ea (inline COMDAT)
    virtual void SetGoal(const EETileCoordinate& inTile, long inSector) = 0;    // +0x2c

    EETileCoordinate mTile;                 // +0x04 (the goal tile)
    long mType;                             // +0x0c
    long GetRevs() const { return this->mRevs; }
    long mRevs;                             // +0x10
    long mNumRuns;                          // +0x14
    long mSteps;                            // +0x18
    long GetSector() const { return this->mSector; }
    long mSector;                           // +0x1c (the goal's path sub sector id)
    long mNodeCount;                        // +0x20
    bool mFound;                            // +0x24
};

template <class S, class G, class M>
class EEAIAStarMachine : public EEAIAStarMachineBase, public S {
public:
    EEAIAStarMachine(long inType, M* inMap);   // 0xa977e / 0xa996d
    virtual ~EEAIAStarMachine();            // ??_G 0xa9ad1 / 0xa9b6b
    virtual void Reset();                   // +0x04 0xa97c8 / 0xa99ba
    virtual long GetMaxRevs() { return this->mGoal.mMaxRevs; }  // +0x08 0xa982d
    virtual void GetPoints(std::vector<GE3DPoint>& outPath, std::vector<U2DPoint<long> >& outVisited);   // +0x0c 0xa9813 (the goal's)
    virtual void GetPoints(std::vector<GE3DPoint>& outPath);   // +0x10 0xa9804 (the goal's)
    virtual void GetPath(std::vector<U2DPoint<long> >& outPath);               // +0x14 0xa97f5 (the goal's)
    virtual void GetPath(std::vector<U2DPoint<long> >& outPath, std::vector<U2DPoint<long> >& outVisited);   // +0x18 0xa97e2 (the goal's)
    virtual long GetRevLimit() { return this->mGoal.mRevLimit; }  // +0x1c 0xa9826
    virtual void Run();                     // +0x20 0xaa4e5 / 0xaa5b8
    virtual void SetMaxRevs(long inRevs) { this->mGoal.mMaxRevs = inRevs; }  // +0x24 0xa9834
    virtual void SetGoal(const EETileCoordinate& inTile) { this->mGoal.mGoalTile = inTile; }  // +0x28 0xa9852
    virtual void SetGoal(long inX, long inY) { this->mGoal.mGoalTile = U2DPoint<long>(inX, inY); }  // +0x2c 0xa9841
    virtual void SetRevLimit(long inLimit) { this->mGoal.mRevLimit = inLimit; }  // +0x30 0xa9864
    virtual void SetStart(const EETileCoordinate& inTile) { this->mGoal.mStartTile = inTile; }  // +0x38 0xa9882
    virtual void SetStart(long inX, long inY) { this->mGoal.mStartTile = U2DPoint<long>(inX, inY); }  // +0x3c 0xa9871

    // Run's search steps (the <EEAIAStarChokePointStorage, EEAIAStarChokePointGoal, EEMap> COMDATs 0x1d184.. in
    // EEAIChokePoint.cpp's range) (provisional names). Port: Start 0x1d184, Expand 0x1d1fb and their Restart
    // 0x1d527, TestTile 0x1d63b, NextNode 0x1d6b1 and AddNode 0x28fc9 are Rust (rust/crates/ee_astar machine.rs);
    // these call it with the goal behind EeAStarCallbacks (EEAIAStarHooks, ee/EEAIAStarMachineSteps.h)
    bool Start();                           // 0x1d184: the start node, the goal tile
    void Expand();                          // 0x1d1fb: pops nodes and adds the neighbours the goal accepts
    EeAStarCallbacks Callbacks();
    // New (port), TEST ONLY: Run's record in the A* capture (ee/EEAIAStarCapture.h)
    void CaptureInputs(EEAIAStarCaptureRun& ioRun);
    void CaptureOutputs(EEAIAStarCaptureRun& ioRun);

    G mGoal;                                // +0x6c / +0x80
    M* mMap;                                // +0x110 / +0x13c
    long mMapWidth;                         // +0x114 / +0x140
    long mMapHeight;                        // +0x118 / +0x144
};

template <class S, class G, class M>
class EEAIAStarGraphMachine : public EEAIAStarGraphMachineBase, public S {
public:
    EEAIAStarGraphMachine(long inType, long inNodeCount);  // 0xa9894
    virtual ~EEAIAStarGraphMachine();       // ??_G 0xa9b1e
    virtual void Reset();                   // +0x04 (ICF 0x25cc3)
    virtual long GetMaxRevs() { return this->mGoal.mMaxRevs; }  // +0x08 0xa98e0
    virtual void GetPath(std::vector<long>& outPath);       // +0x0c 0xa98ca (the goal's)
    virtual void GetPath(std::vector<U2DPoint<long> >& outTiles, std::vector<long>& outIndices);   // +0x10 (ICF 0x25cd9: the goal's)
    virtual long GetRevLimit() { return this->mGoal.mRevLimit; }  // +0x14 0xa98d9
    virtual void Run();                     // +0x18 0xaa562
    virtual void SetMaxRevs(long inRevs) { this->mGoal.mMaxRevs = inRevs; }  // +0x1c 0xa98e7
    virtual void SetStart(const EETileCoordinate& inTile, long inSector)    // +0x20 (ICF 0x25cec)
        { this->mGoal.mGoalTile = inTile; this->mGoal.mStartSector = inSector; }
    virtual void SetRevLimit(long inLimit) { this->mGoal.mRevLimit = inLimit; }  // +0x24 0xa98f4
    virtual void SetGoal(const EETileCoordinate& inTile, long inSector)     // +0x2c (ICF 0x25d08)
        { this->mGoal.mStartTile = inTile; this->mGoal.mGoalSector = inSector; }

    // Run's steps (the continental-chain machine's COMDATs 0x27434.. in EEAIContinentManager.cpp's range, the path
    // machine's 0xaacf0.. in EEPathManager.cpp's). The goal supplies the hooks: IsGoalNode, GetStartObject,
    // GetGoalID, Estimate, GetStepCost, AcceptNeighbor, SearchDone; M its mNeighbors, GetNumNeighbors, mID and
    // GetGraphLocation. (provisional names) Port: Start 0x27434, Expand 0x274a3 and their Restart 0x28a34, NextNode
    // 0x2945f and AddNode 0x28aa7 are Rust (rust/crates/ee_astar graph.rs); these call it with the goal and the
    // objects behind EeAStarGraphCallbacks (EEAIAStarGraphHooks below)
    bool Start();
    void Expand();
    EeAStarGraphCallbacks Callbacks();
    // New (port), TEST ONLY: Run's record in the A* capture (ee/EEAIAStarCapture.h, tag ASGR)
    void CaptureInputs(EEAIAStarCaptureRun& ioRun);
    void CaptureOutputs(EEAIAStarCaptureRun& ioRun);

    G mGoal;                                // +0x64
};

// the out-of-line template members (COMDATs per instantiation; EEAIContinentManager.cpp's
// continent/hill/path-area/path-sector machines 0x25ba0.., the continental-chain graph machine 0x25c55..)
template <class S, class G, class M>
EEAIAStarMachine<S, G, M>::EEAIAStarMachine(long inType, M* inMap)
    : EEAIAStarMachineBase(inType), mMap(inMap), mMapWidth(0), mMapHeight(0)
{
}

template <class S, class G, class M>
EEAIAStarMachine<S, G, M>::~EEAIAStarMachine()
{
}

template <class S, class G, class M>
void EEAIAStarMachine<S, G, M>::Reset()
{
    ResetNodes();
    this->mFound = false;
    this->mGoal.Reset();
}

// the path slots forward to the goal (0xa97e2..0xa9813; the flood machines' fold into empty ICF bodies)
template <class S, class G, class M>
void EEAIAStarMachine<S, G, M>::GetPoints(std::vector<GE3DPoint>& outPath, std::vector<U2DPoint<long> >& outVisited)
{
    this->mGoal.GetPoints(outPath, outVisited);
}

template <class S, class G, class M>
void EEAIAStarMachine<S, G, M>::GetPoints(std::vector<GE3DPoint>& outPath)
{
    this->mGoal.GetPoints(outPath);
}

template <class S, class G, class M>
void EEAIAStarMachine<S, G, M>::GetPath(std::vector<U2DPoint<long> >& outPath)
{
    this->mGoal.GetPath(outPath);
}

template <class S, class G, class M>
void EEAIAStarMachine<S, G, M>::GetPath(std::vector<U2DPoint<long> >& outPath, std::vector<U2DPoint<long> >& outVisited)
{
    this->mGoal.GetPath(outPath, outVisited);
}

template <class S, class G, class M>
void EEAIAStarGraphMachine<S, G, M>::GetPath(std::vector<long>& outPath)    // 0xa98ca
{
    this->mGoal.GetPath(outPath);
}

template <class S, class G, class M>
void EEAIAStarGraphMachine<S, G, M>::GetPath(std::vector<U2DPoint<long> >& outTiles, std::vector<long>& outIndices)
{
    this->mGoal.GetPath(outTiles, outIndices);
}

template <class S, class G, class M>
EEAIAStarGraphMachine<S, G, M>::EEAIAStarGraphMachine(long inType, long inNodeCount)
    : EEAIAStarGraphMachineBase(inType, inNodeCount)
{
}

template <class S, class G, class M>
EEAIAStarGraphMachine<S, G, M>::~EEAIAStarGraphMachine()
{
}

template <class S, class G, class M>
void EEAIAStarGraphMachine<S, G, M>::Reset()
{
    ResetNodes();
    this->mGoal.Reset();
}

template <class S, class G, class M>
void EEAIAStarGraphMachine<S, G, M>::Run()
{
    EEAIAStarCaptureRun theCapture;         // TEST ONLY (EE_SIM_CAPTURE): this run's record
    if (theCapture.IsOn())
        this->CaptureInputs(theCapture);
    if (!this->mFound) {
        this->mNumRuns = 0;
        this->mRevs = 0;
        Init(this->mNodeCount);
        this->mGoal.Attach(this, this);
        this->mGoal.Prepare();
        this->Start();
        this->Expand();
    } else
        this->Expand();
    if (theCapture.IsOn()) {
        this->CaptureOutputs(theCapture);
        theCapture.Commit(kAStarCaptureGraphRun);
    }
}

// New (port), TEST ONLY: the search's inputs as Run finds them
template <class S, class G, class M>
void EEAIAStarGraphMachine<S, G, M>::CaptureInputs(EEAIAStarCaptureRun& ioRun)
{
    ioRun.I32(this->mType);
    ioRun.U8(this->mFound);
    ioRun.U8(this->mGoal.mAcceptNearest);
    ioRun.I32(this->mGoal.mStartTile.mX);
    ioRun.I32(this->mGoal.mStartTile.mY);
    ioRun.I32(this->mGoal.mGoalTile.mX);
    ioRun.I32(this->mGoal.mGoalTile.mY);
    ioRun.I32(this->mGoal.mStartSector);
    ioRun.I32(this->mGoal.mGoalSector);
    ioRun.I32(this->mGoal.mPlayerID);
    ioRun.I32(this->mGoal.mRevLimit);
    ioRun.I32(this->mGoal.mMaxRevs);
    ioRun.I32(this->mNodeCount);
}

// New (port), TEST ONLY: the search's outputs: the machine's members, the goal's flags, the storage's nodes
template <class S, class G, class M>
void EEAIAStarGraphMachine<S, G, M>::CaptureOutputs(EEAIAStarCaptureRun& ioRun)
{
    ioRun.U8(this->mFound);
    ioRun.I32(this->mRevs);
    ioRun.I32(this->mSteps);
    ioRun.I32(this->mNumRuns);
    ioRun.I32(this->mTile.mX);
    ioRun.I32(this->mTile.mY);
    ioRun.I32(this->mSector);
    ioRun.U8(this->mGoal.mSearchFailed);
    ioRun.U8(this->mGoal.mTookNearest);
    S* theStorage = this;
    theStorage->CaptureNodes(ioRun);
}

// New (port): the goal and the graph objects behind the Rust graph machine's callbacks (ee_astar's
// EeAStarGraphCallbacks). Each asks the goal what the C++ machine asked it, at the same point of the search, and notes
// it for the capture as the C++ machine did. The objects travel as opaque handles: an M's AStarGraphableObjectBase*
// (at +0x68 in EEAIContinent), which Rust stores in its nodes and hands back. The nodes the goal takes (IsGoalNode,
// GetStepCost) are EEAIAStarGraphNode views of the Rust node (mObject, mIndex, mTile, the costs; no links). What the
// goal reads of the storage meanwhile (SearchDone: the current node, an open node left) arrives in the arguments and
// is parked on the storage (mHook*).
template <class S, class G, class M>
struct EEAIAStarGraphHooks {
    typedef EEAIAStarGraphMachine<S, G, M> Machine;

    static void* Handle(M* inObject)
    {
        AStarGraphableObjectBase* theBase = inObject;
        return theBase;
    }
    static M* Object(void* inHandle) { return static_cast<M*>(static_cast<AStarGraphableObjectBase*>(inHandle)); }

    static void* StartObject(void* inMachine)
    {
        Machine* theMachine = (Machine*)inMachine;
        M* theStart = theMachine->mGoal.GetStartObject();
        EEAIAStarNote(kAStarEventGraphStart, 0, 0, 0, 0, 0, theStart ? theStart->mID : -1);
        return Handle(theStart);
    }
    static int32_t GoalID(void* inMachine) { return ((Machine*)inMachine)->mGoal.GetGoalID(); }
    static int32_t Estimate(void* inMachine, int32_t inFromX, int32_t inFromY, int32_t inToX, int32_t inToY)
    {
        Machine* theMachine = (Machine*)inMachine;
        return EEAIAStarNote(kAStarEventGraphEstimate, inFromX, inFromY, inToX, inToY, 0,
                             theMachine->mGoal.Estimate(U2DPoint<long>(inFromX, inFromY), U2DPoint<long>(inToX, inToY)));
    }
    static bool IsGoalNode(void* inMachine, bool inHasNode, const EeAStarGraphNodeView* inNode)
    {
        Machine* theMachine = (Machine*)inMachine;
        EEAIAStarGraphNode theNode(*inNode);
        return EEAIAStarNote(kAStarEventGraphGoalNode, inHasNode ? inNode->index : -1, 0, 0, 0, 0,
                             theMachine->mGoal.IsGoalNode(inHasNode ? &theNode : NULL)) != 0;
    }
    static int32_t ObjectID(void* inMachine, void* inObject) { return Object(inObject)->mID; }
    static uint32_t NumNeighbors(void* inMachine, void* inObject) { return Object(inObject)->GetNumNeighbors(); }
    static void* Neighbor(void* inMachine, void* inObject, uint32_t inIndex)
    {
        return Handle(Object(inObject)->mNeighbors[inIndex]);
    }
    static void GraphLocation(void* inMachine, void* inObject, int32_t* outX, int32_t* outY)
    {
        U2DPoint<long> theTile = Object(inObject)->GetGraphLocation();
        *outX = theTile.mX;
        *outY = theTile.mY;
    }
    static bool CanEnter(void* inMachine, void* inNeighbor, void* inFrom)
    {
        Machine* theMachine = (Machine*)inMachine;
        M* theNeighbor = Object(inNeighbor);
        M* theFrom = Object(inFrom);
        return EEAIAStarNote(kAStarEventGraphEnter, theNeighbor->mID, theFrom->mID, 0, 0, 0,
                             theMachine->mGoal.CanEnter(theNeighbor, theFrom)) != 0;
    }
    static int32_t StepCost(void* inMachine, const EeAStarGraphNodeView* inParent, void* inObject)
    {
        Machine* theMachine = (Machine*)inMachine;
        EEAIAStarGraphNode theParent(*inParent);
        M* theObject = Object(inObject);
        return EEAIAStarNote(kAStarEventGraphCost, inParent->index, theObject->mID, 0, 0, 0,
                             (long)theMachine->mGoal.GetStepCost(&theParent, theObject));
    }
    static void SearchDone(void* inMachine, bool inHasCurrent, const EeAStarGraphNodeView* inCurrent, bool inHasOpen)
    {
        Machine* theMachine = (Machine*)inMachine;
        S* theStorage = theMachine;
        theStorage->mHookHasCurrent = inHasCurrent;
        if (inHasCurrent)
            theStorage->mHookCurrent = EEAIAStarGraphNode(*inCurrent);
        theStorage->mHookHasOpen = inHasOpen;
        theMachine->mGoal.SearchDone(theMachine);
        EEAIAStarNote(kAStarEventGraphSearchDone, theMachine->mGoal.mSearchFailed, theMachine->mGoal.mTookNearest, 0, 0,
                      0, 0);
    }
    static bool SearchFailed(void* inMachine) { return ((Machine*)inMachine)->mGoal.mSearchFailed; }
    static int32_t RevLimit(void* inMachine) { return ((Machine*)inMachine)->mGoal.mRevLimit; }
    static int32_t MaxRevs(void* inMachine) { return ((Machine*)inMachine)->mGoal.mMaxRevs; }
    static void Tiles(void* inMachine, int32_t* outTiles)
    {
        Machine* theMachine = (Machine*)inMachine;
        outTiles[0] = theMachine->mGoal.mStartTile.mX;
        outTiles[1] = theMachine->mGoal.mStartTile.mY;
        outTiles[2] = theMachine->mGoal.mGoalTile.mX;
        outTiles[3] = theMachine->mGoal.mGoalTile.mY;
    }
    static void GetState(void* inMachine, EeAStarGraphMachineState* outState)
    {
        Machine* theMachine = (Machine*)inMachine;
        outState->tile_x = theMachine->mTile.mX;
        outState->tile_y = theMachine->mTile.mY;
        outState->revs = theMachine->mRevs;
        outState->num_runs = theMachine->mNumRuns;
        outState->steps = theMachine->mSteps;
        outState->sector = theMachine->mSector;
        outState->found = theMachine->mFound;
    }
    static void SetState(void* inMachine, const EeAStarGraphMachineState* inState)
    {
        Machine* theMachine = (Machine*)inMachine;
        theMachine->mTile.mX = inState->tile_x;
        theMachine->mTile.mY = inState->tile_y;
        theMachine->mRevs = inState->revs;
        theMachine->mNumRuns = inState->num_runs;
        theMachine->mSteps = inState->steps;
        theMachine->mSector = inState->sector;
        theMachine->mFound = inState->found;
    }
};

template <class S, class G, class M>
EeAStarGraphCallbacks EEAIAStarGraphMachine<S, G, M>::Callbacks()
{
    typedef EEAIAStarGraphHooks<S, G, M> Hooks;
    EeAStarGraphCallbacks theCallbacks;
    theCallbacks.ctx = this;
    theCallbacks.start_object = Hooks::StartObject;
    theCallbacks.goal_id = Hooks::GoalID;
    theCallbacks.estimate = Hooks::Estimate;
    theCallbacks.is_goal_node = Hooks::IsGoalNode;
    theCallbacks.object_id = Hooks::ObjectID;
    theCallbacks.num_neighbors = Hooks::NumNeighbors;
    theCallbacks.neighbor = Hooks::Neighbor;
    theCallbacks.graph_location = Hooks::GraphLocation;
    theCallbacks.can_enter = Hooks::CanEnter;
    theCallbacks.step_cost = Hooks::StepCost;
    theCallbacks.search_done = Hooks::SearchDone;
    theCallbacks.search_failed = Hooks::SearchFailed;
    theCallbacks.rev_limit = Hooks::RevLimit;
    theCallbacks.max_revs = Hooks::MaxRevs;
    theCallbacks.tiles = Hooks::Tiles;
    theCallbacks.get_state = Hooks::GetState;
    theCallbacks.set_state = Hooks::SetState;
    return theCallbacks;
}

// 0x27434 (Rust: ee_astar graph.rs start)
template <class S, class G, class M>
bool EEAIAStarGraphMachine<S, G, M>::Start()
{
    EeAStarGraphCallbacks theCallbacks = this->Callbacks();
    return ee_astar_graph_start(this->mCore, &theCallbacks);
}

// 0x274a3 (Rust: ee_astar graph.rs expand, with Restart, NextNode and AddNode)
template <class S, class G, class M>
void EEAIAStarGraphMachine<S, G, M>::Expand()
{
    EeAStarGraphCallbacks theCallbacks = this->Callbacks();
    ee_astar_graph_expand(this->mCore, &theCallbacks);
}

#endif
