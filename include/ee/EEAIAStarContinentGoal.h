// C:\EEX\Empire Earth\AI Thread\EEAIAStarContinentGoal.h (RECONSTRUCTION; file name inferred).
// RTTI EEAIAStarContinentGoal > EEAIAStarGoalBase<EEAIAStarContinentStorage, EEMap>; vtable 0x439e54 (5 slots, all the
// base's; EEAIAStarContinentGoal.cpp 0x155c0..0x15a47): the flood fill that grows one continent at a time over tiles of
// one terrain kind. Members by offset (provisional names). Requires ee/EEAIAStarGoalBase.h,
// ee/EEAIAStarContinentStorage.h, ee/EEMap.h, ee/EEAIContinent.h, <map>, <vector>.
#ifndef EE_EEAIASTARCONTINENTGOAL_H
#define EE_EEAIASTARCONTINENTGOAL_H

class EEAIAStarContinentGoal : public EEAIAStarGoalBase<EEAIAStarContinentStorage, EEMap> {
public:
    // the machine's hooks, constants for a flood fill (EEAIAStarMachine<S, G, M>: no goal node but the empty list,
    // no estimate, unit steps, no limits, no reopening) (provisional names)
    bool IsGoalNode(EEAIAStarNode* inNode) const { return inNode == NULL; }
    long Estimate(const U2DPoint<long>& inFrom, const U2DPoint<long>& inTo) const { return 1; }
    long GetCost(long inFromX, long inFromY, long inX, long inY) const { return 1; }
    bool IsStepLimit(long inSteps) const { return false; }
    bool IsRevLimit(long inRevs) const { return false; }
    bool IsCheaper(unsigned long inCost, unsigned long inOld) const { return false; }
    EEAIAStarContinentGoal();               // 0x155c0
    virtual ~EEAIAStarContinentGoal();      // 0x1593b
    void CollectTiles();                    // 0x15607: the storage's open and closed nodes get the id; mTiles gets them
    void CreateContinent();                 // 0x156b2: a new EEAIContinent of mTiles, registered in mContinents
    bool AcceptTile(unsigned long inX, unsigned long inY, bool inStart);   // 0x1575c (ret 0xc; the machine passes the flag): whether the fill may take the tile

    std::map<unsigned short, EEAIContinent*> mContinents;   // +0x30
    std::vector<U2DPoint<long> > mTiles;    // +0x3c
    long mTileCount;                        // +0x4c (counted by AcceptTile; reserves mTiles)
    unsigned char mKind;                    // +0x50 (0 unknown, 1 land, 2/3/4 terrain 1/9/0xc, 5 map edge)
    unsigned short mNextID;                 // +0x52 (= 1)
};

#endif
