// C:\EEX\Empire Earth\AI Thread\EEAIAStarGraphGoalBase.h (RECONSTRUCTION; file name inferred). template<S>
// EEAIAStarGraphGoalBase: the goal half of the AI's graph A* searches over storage S (RTTI
// .?AV?$EEAIAStarGraphGoalBase@V<S>@@@@, no base; 5 slots; sizeof 0x38: EEAIAStarGraphContinentalChainGoal's members
// start at +0x38). Instantiations (vtables): <EEAIAStarGraphTerrainStorage> 0x439ed0, <EEAIAStarGraphStorage> 0x43a090.
// The ctor is an out-of-line COMDAT per instantiation (0x15fbb, 0x1a961: the init list, then the vptr; the twin of
// EEAIAStarGoalBase's); the dtor is a user-declared empty inline (each ??_G stores its own vptr: 0x16053, 0x1a9cf).
// Slots +0x04/+0x08/+0x0c are ICF-folded trivial bodies, +0x10 the kept copy 0x1a9be. Members by offset
// (provisional names). Requires lle/U2DPoint.h.
#ifndef EE_EEAIASTARGRAPHGOALBASE_H
#define EE_EEAIASTARGRAPHGOALBASE_H

class EEAIAStarGraphMachineBase;        // ee/EEAIAStarMachine.h

template <class S>
class EEAIAStarGraphGoalBase {
public:
    // port: the sectors start at 0 as Reset leaves them (the original left them as the memory had them; the
    // continental chain goal never sets them, and the ASGR capture reads them)
    EEAIAStarGraphGoalBase()
        : mStorage(NULL), mMachine(NULL), mPlayerID(0), mRevLimit(-1), mMaxRevs(-1), mGoalSector(0),
          mStartSector(0), mSearchFailed(false), mB31(false), mAcceptNearest(false), mB33(false), mB34(false), mTookNearest(false)
    {
    }
    virtual ~EEAIAStarGraphGoalBase() {}    // +0x00
    virtual bool Virtual04() { return false; }   // +0x04 (ICF 0xac9f3)
    virtual void Virtual08(long inArg) {}   // +0x08 (ICF 0x156dd8)
    virtual bool Prepare() { return true; }      // +0x0c (ICF 0x18aa9)
    // +0x10 (kept copy 0x1a9be: arg 1 -> +0x14, arg 2 -> +0x18)
    virtual void Attach(S* inStorage, EEAIAStarGraphMachineBase* inMachine)      // (the graph machines' Run 0x26941)
    {
        this->mStorage = inStorage;
        this->mMachine = inMachine;
    }

    // 0x15ff5 (an out-of-line COMDAT; callers 0x1a8ea, 0x25cd2): the search state back to the ctor's
    void Reset()
    {
        this->mStartTile.mX = 0;
        this->mStartTile.mY = 0;
        this->mGoalTile.mX = 0;
        this->mGoalTile.mY = 0;
        this->mMaxRevs = -1;
        this->mRevLimit = -1;
        this->mGoalSector = 0;
        this->mStartSector = 0;
        this->mPlayerID = 0;
        this->mSearchFailed = false;
        this->mB31 = false;
        this->mAcceptNearest = false;
        this->mB33 = false;
        this->mB34 = false;
        this->mTookNearest = false;
    }

    U2DPoint<long> mStartTile;              // +0x04
    U2DPoint<long> mGoalTile;               // +0x0c (EEAIAStarPathGraphGoal 0x1a6c7 compares the path's tile with it)
    S* mStorage;                            // +0x14
    EEAIAStarGraphMachineBase* mMachine;    // +0x18 (Virtual10: the machine running the search)
    long mPlayerID;                         // +0x1c
    long mRevLimit;                         // +0x20 (= -1)
    long mMaxRevs;                          // +0x24 (= -1)
    long mGoalSector;                       // +0x28
    long mStartSector;                      // +0x2c (EEAIAStarPathGraphGoal 0x1a6c7 compares a node's index with it)
    bool mSearchFailed;                     // +0x30
    bool mB31;                              // +0x31
    bool mAcceptNearest;                    // +0x32
    bool mB33;                              // +0x33
    bool mB34;                              // +0x34
    bool mTookNearest;                      // +0x35
};

#endif
