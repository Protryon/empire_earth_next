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

struct EEAIAStarGraphNode;               // ee/EEAIAStarGraphStorage.h
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

    // Run's flood-fill steps (the <EEAIAStarChokePointStorage, EEAIAStarChokePointGoal, EEMap> COMDATs 0x1d184.. in
    // EEAIChokePoint.cpp's range; bodies there for now) (provisional names)
    bool Start();                           // 0x1d184: the start node, the goal tile
    void Expand();                          // 0x1d1fb: pops nodes and adds the neighbours the goal accepts
    bool Restart();                         // 0x1d527: the nearest remaining node the goal accepts
    bool TestTile(unsigned long inX, unsigned long inY);   // 0x1d63b: the goal's AcceptTile, cached in mTileFlags (1/2)
    EEAIAStarNode* NextNode();              // 0x1d6b1: the nearer of the nearest open and closed nodes
    void AddNode(EEAIAStarNode* inParent, long inX, long inY);   // 0x28fc9 (ICF): a new open node unless known

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
    // GetGraphLocation. (provisional names)
    bool Start();                           // 0x27434: the start node from the goal's start object
    void Expand();                          // 0x274a3: pops nodes and adds the neighbours the goal accepts
    bool Restart();                         // 0x28a34: the nearest remaining node, else a new start node
    EEAIAStarGraphNode* NextNode();         // 0x2945f: the nearer of the nearest open and closed nodes
    void AddNode(EEAIAStarGraphNode* inParent, M* inObject);   // 0x28aa7: a new open node unless known cheaper

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
}

template <class S, class G, class M>
bool EEAIAStarGraphMachine<S, G, M>::Start()
{
    this->mTile.mX = this->mGoal.mGoalTile.mX;
    this->mTile.mY = this->mGoal.mGoalTile.mY;
    this->mSector = this->mGoal.GetGoalID();
    M* theStart = this->mGoal.GetStartObject();
    mCurrentNode = NewNode(this->mGoal.mStartTile, theStart, theStart->mID);
    mCurrentNode->mPathCost = 0;
    mCurrentNode->mEstimate = this->mGoal.Estimate(this->mGoal.mStartTile, this->mTile);
    mCurrentNode->mCost = mCurrentNode->mEstimate + mCurrentNode->mPathCost;
    AddOpen(mCurrentNode);
    return true;
}

template <class S, class G, class M>
void EEAIAStarGraphMachine<S, G, M>::Expand()
{
    this->mNumRuns++;
    this->mRevs++;
    this->mSteps = 0;
    for (;;) {
        mCurrentNode = RemoveBestOpen();
        if (this->mGoal.IsGoalNode(mCurrentNode)) {
            this->mFound = false;
            break;
        }
        M* theObject = static_cast<M*>(mCurrentNode->mObject);
        if (theObject->GetNumNeighbors() > 0) {
            for (long i = theObject->mNeighbors.size() - 1; i >= 0; i--) {
                AStarGraphableObjectBase* theNeighbor = theObject->mNeighbors[i];
                if (theObject->mID != -1 && this->mGoal.CanEnter(static_cast<M*>(theNeighbor), theObject))
                    this->AddNode(mCurrentNode, theObject->mNeighbors[i]);
            }
        }
        AddClosed(mCurrentNode);
        if (this->mGoal.mRevLimit != -1 && this->mSteps >= this->mGoal.mRevLimit) {
            mCurrentNode = 0;
            this->mFound = true;
            break;
        }
        if (this->mGoal.mMaxRevs != -1 && this->mRevs >= this->mGoal.mMaxRevs) {
            mCurrentNode = 0;
            this->mFound = false;
            break;
        }
        this->mSteps++;
        this->mRevs++;
    }
    if (!this->mFound)
        this->mGoal.SearchDone(this);
    if (this->mGoal.mSearchFailed) {
        if (mCurrentNode) {
            AddClosed(mCurrentNode);
            mCurrentNode = 0;
        }
        this->Restart();
    } else if (!this->mFound && mCurrentNode)
        mPath = mCurrentNode;
    else
        mCurrentNode = 0;
}

template <class S, class G, class M>
bool EEAIAStarGraphMachine<S, G, M>::Restart()
{
    mPath = 0;
    EEAIAStarGraphNode* theNode = this->NextNode();
    if (theNode) {
        this->mTile.mX = theNode->mTile.mX;
        this->mTile.mY = theNode->mTile.mY;
        mPath = theNode;
        mCurrentNode = theNode;
        this->mSector = theNode->mIndex;
    } else {
        mCurrentNode = NewNode(this->mGoal.mStartTile, this->mGoal.GetStartObject(), -1);
        mCurrentNode->mPathCost = 0;
        mCurrentNode->mEstimate = 1;
        mCurrentNode->mCost = 1;
        mPath = mCurrentNode;
    }
    this->mFound = false;
    return true;
}

template <class S, class G, class M>
EEAIAStarGraphNode* EEAIAStarGraphMachine<S, G, M>::NextNode()
{
    EEAIAStarGraphNode* theOpen = RemoveNearestOpen(this->mTile);
    EEAIAStarGraphNode* theClosed = RemoveNearestClosed(this->mTile);
    if (theOpen) {
        if (theClosed) {
            if (theOpen->mCost <= theClosed->mCost)
                AddClosed(theClosed);
            else {
                AddOpen(theOpen);
                theOpen = theClosed;
            }
        }
    } else
        theOpen = theClosed;
    return theOpen;
}

template <class S, class G, class M>
void EEAIAStarGraphMachine<S, G, M>::AddNode(EEAIAStarGraphNode* inParent, M* inObject)
{
    unsigned long theCost = inParent->mPathCost + this->mGoal.GetStepCost(inParent, inObject);
    U2DPoint<long> theTile = inObject->GetGraphLocation();
    long theIndex = inObject->mID;
    EEAIAStarGraphNode* theOpen = GetOpenNode(theIndex);
    EEAIAStarGraphNode* theClosed = GetClosedNode(theIndex);
    if (theOpen && theOpen->mPathCost <= theCost)
        return;
    if (theClosed) {
        if (theClosed->mPathCost <= theCost)
            return;
        RemoveClosed(theClosed);
        ReleaseNode(theClosed);
    }
    if (!theOpen) {
        EEAIAStarGraphNode* theNode = NewNode(theTile, inObject, theIndex);
        theNode->mPathCost = theCost;
        theNode->mParent = inParent;
        theNode->mEstimate = this->mGoal.Estimate(theNode->mTile, this->mTile);  // the goal's heuristic (0xab6c1: Manhattan * 10)
        theNode->mCost = theNode->mEstimate + theCost;
        AddOpen(theNode);
    } else {
        theOpen->mParent = inParent;
        unsigned long theTotal = theOpen->mEstimate + theCost;
        theOpen->mPathCost = theCost;
        if (theOpen->mCost != theTotal) {
            theOpen->mCost = theTotal;
            UpdateOpen(theOpen);                // re-sorts the open list (0xab6c1 calls 0x1656d)
        }
    }
}

#endif
