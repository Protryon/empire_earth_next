// C:\EEX\Empire Earth\AI Thread\EEAIAStarHillGoal.h (RECONSTRUCTION; file name inferred).
// RTTI EEAIAStarHillGoal > EEAIAStarGoalBase<EEAIAStarHillStorage, EEMap>; vtable 0x439f00 (5 slots, all the base's;
// EEAIAStarHillGoal.cpp 0x16c0e..0x171b5): the flood fill that grows one hill at a time over the high tiles (level 4
// and up, not terrain 9) of one continent; the twin of EEAIAStarContinentGoal. Members by offset (provisional names).
// Requires ee/EEAIAStarGoalBase.h, ee/EEAIAStarHillStorage.h, ee/EEMap.h, ee/EEAIHill.h, <map>, <vector>.
#ifndef EE_EEAIASTARHILLGOAL_H
#define EE_EEAIASTARHILLGOAL_H

class EEAIAStarHillGoal : public EEAIAStarGoalBase<EEAIAStarHillStorage, EEMap> {
public:
    // the machine's hooks, constants for a flood fill (EEAIAStarMachine<S, G, M>: no goal node but the empty list,
    // no estimate, unit steps, no limits, no reopening) (provisional names)
    bool IsGoalNode(EEAIAStarNode* inNode) const { return inNode == NULL; }
    long Estimate(const U2DPoint<long>& inFrom, const U2DPoint<long>& inTo) const { return 1; }
    long GetCost(long inFromX, long inFromY, long inX, long inY) const { return 1; }
    bool IsStepLimit(long inSteps) const { return false; }
    bool IsRevLimit(long inRevs) const { return false; }
    bool IsCheaper(unsigned long inCost, unsigned long inOld) const { return false; }
    EEAIAStarHillGoal();                    // 0x16c0e
    virtual ~EEAIAStarHillGoal();           // 0x170a9
    void CollectTiles();                    // 0x16c56: the storage's open and closed nodes get the ids; mTiles gets them
    void CreateHill();                      // 0x16d04: a new EEAIHill of mTiles, registered in mHills
    // 0x16db8 (ret 0xc): whether the fill may take the tile
    bool AcceptTile(unsigned long inX, unsigned long inY, long inStart);

    std::map<unsigned short, EEAIHill*> mHills;   // +0x30
    std::vector<U2DPoint<long> > mTiles;    // +0x3c
    long mTileCount;                        // +0x4c (counted by AcceptTile; reserves mTiles)
    long mLevelSum;                         // +0x50 (the accepted tiles' height levels)
    unsigned short mNextID;                 // +0x54 (= 1)
    unsigned short mContinentID;            // +0x56 (the first tile's; 0 between hills)
};

#endif
