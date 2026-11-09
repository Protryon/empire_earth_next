// C:\EEX\Empire Earth\AI Thread\EEAIAStarGoalBase.h (RECONSTRUCTION; file name inferred).
// template<S, M> EEAIAStarGoalBase: the goal half of the AI's A* searches over storage S on map M (RTTI
// .?AV?$EEAIAStarGoalBase@V<S>@@VEEMap@@@@, no base; 5 slots; sizeof 0x30: EEAIAStarPathfindingGoal has
// EEAIAStarPathfindingGoalBase at +0x30). Instantiations (vtables): <EEAIAStarBoundedStorage> 0x439ba8,
// <EEAIAStarChokePointStorage> 0x439d24, <EEAIAStarContinentStorage> 0x439e6c, <EEAIAStarHillStorage> 0x439f4c,
// <EEAIAStarPathAreaStorage> 0x439f94, <EEAIAStarStorage> 0x43a030, <EEAIAStarPathSectorStorage> 0x43a0c0.
// The ctor is an out-of-line COMDAT per instantiation (0x158e5, 0x199b0, ...: every member in the init list, then
// the vptr); the dtor is a user-declared empty inline (each ??_G stores its own vptr). Slots +0x04/+0x08/+0x0c are
// ICF-folded trivial bodies, +0x10 the kept copy 0x1b704. Members by offset (provisional names). Requires
// lle/U2DPoint.h.
#ifndef EE_EEAIASTARGOALBASE_H
#define EE_EEAIASTARGOALBASE_H

#include "ee/EEAIAStarView.h"

class EEAIAStarMachineBase;

template <class S, class M>
class EEAIAStarGoalBase {
public:
    EEAIAStarGoalBase()
        : mStorage(NULL), mMap(NULL), mMachine(NULL), mRevLimit(-1), mMaxRevs(-1),
          mSearchFailed(false), mB29(false), mAcceptNearest(false), mB2B(false), mB2C(false), mTookNearest(false)
    {
    }
    virtual ~EEAIAStarGoalBase() {}         // +0x00
    virtual bool Virtual04() { return false; }   // +0x04 (ICF 0xac9f3)
    virtual void SearchDone(EEAIAStarMachineBase* inMachine) {}  // +0x08 (ICF 0x156dd8; the running machine)
    virtual bool Prepare() { return true; }      // +0x0c (ICF 0x18aa9)
    // +0x10 (kept copy 0x1b704: arg 1 -> +0x18, arg 2 -> +0x14, arg 3 -> +0x1c)
    // the machine's limit and reopen hooks (the path machines' Expand 0xaa98d, AddNode 0xab504; the flood goals
    // hide them with constants) (provisional names)
    bool IsStepLimit(long inSteps) const { return this->mRevLimit != -1 && inSteps >= this->mRevLimit; }
    bool IsRevLimit(long inRevs) const { return this->mMaxRevs != -1 && inRevs >= this->mMaxRevs; }
    bool IsCheaper(unsigned long inCost, unsigned long inOld) const { return inOld > inCost; }
    // the machine's path slots forward here; a flood fill has no path (its machines' slots +0x0c..+0x18 are the
    // ICF-folded empty `ret 8` 0xac9c8 / `ret 4` 0x156dd8; the path goals hide these) (provisional name)
    void GetPoints(std::vector<GE3DPoint>& outPath, std::vector<U2DPoint<long> >& outVisited) {}
    void GetPoints(std::vector<GE3DPoint>& outPath) {}
    void GetPath(std::vector<U2DPoint<long> >& outPath) {}
    void GetPath(std::vector<U2DPoint<long> >& outPath, std::vector<U2DPoint<long> >& outVisited) {}
    virtual void Attach(M* inMap, S* inStorage, EEAIAStarMachineBase* inMachine)
    {
        this->mMap = inMap;
        this->mStorage = inStorage;
        this->mMachine = inMachine;
        this->mView.SetMap(inMap);
    }
    // inline (COMDAT 0x26452 in EEAIContinentManager.cpp's range: the machines' Reset)
    void Reset()
    {
        this->mStartTile = U2DPoint<long>(0, 0);
        this->mGoalTile = U2DPoint<long>(0, 0);
        this->mMaxRevs = -1;
        this->mRevLimit = -1;
        this->mSearchFailed = false;
        this->mB29 = false;
        this->mAcceptNearest = false;
        this->mB2B = false;
        this->mB2C = false;
        this->mTookNearest = false;
    }

    // +0x04/+0x0c: two points (their inline ctors store before the scalars of the init list: 0x158e5)
    U2DPoint<long> mStartTile;              // +0x04
    U2DPoint<long> mGoalTile;               // +0x0c
    S* mStorage;                            // +0x14 (the goals walk its open/closed lists and per-tile arrays)
    M* mMap;                                // +0x18 (GetTile)
    EEAIAStarMachineBase* mMachine;         // +0x1c (Virtual10's third argument: the machine)
    long mRevLimit;                         // +0x20 (= -1)
    long mMaxRevs;                          // +0x24 (= -1)
    bool mSearchFailed;                     // +0x28
    bool mB29;                              // +0x29
    bool mAcceptNearest;                    // +0x2a
    bool mB2B;                              // +0x2b
    bool mB2C;                              // +0x2c
    bool mTookNearest;                      // +0x2d
    // New (port): the search's reads of the world, by tile (ee/EEAIAStarView.h; on Attach's map)
    EEAIAStarView mView;
};

#endif
