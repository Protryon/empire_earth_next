// C:\EEX\Empire Earth\AI Thread\EEAIAStarChokePointGoal.h (RECONSTRUCTION; file name inferred). RTTI
// EEAIAStarChokePointGoal > EEAIAStarGoalBase<EEAIAStarChokePointStorage, EEMap>;
// vtable 0x439d0c (5 slots: +0x10 its own Virtual10 0x10db7, the rest the base's; EEAIAStarChokePointGoal.cpp
// 0x10d5d..0x1540d): the flood fill that grows one choke point at a time over the tiles EEAIChokePoint.cpp's terrain
// pass marked (its file globals; EEAIAStarMachine<EEAIAStarChokePointStorage, EEAIAStarChokePointGoal, EEMap> runs it,
// goal at +0x84). sizeof 0x6c. Members by offset (provisional names). Requires ee/EEAIAStarGoalBase.h,
// ee/EEAIAStarChokePointStorage.h, ee/EEMap.h, ee/EEAIChokePoint.h, <map>, <vector>.
#ifndef EE_EEAIASTARCHOKEPOINTGOAL_H
#define EE_EEAIASTARCHOKEPOINTGOAL_H

class EEAIChokePoint;

class EEAIAStarChokePointGoal : public EEAIAStarGoalBase<EEAIAStarChokePointStorage, EEMap> {
public:
    // the machine's hooks, constants for a flood fill (EEAIAStarMachine<S, G, M>: no goal node but the empty list,
    // no estimate, unit steps, no limits, no reopening) (provisional names)
    bool IsGoalNode(EEAIAStarNode* inNode) const { return inNode == NULL; }
    long Estimate(const U2DPoint<long>& inFrom, const U2DPoint<long>& inTo) const { return 1; }
    long GetCost(long inFromX, long inFromY, long inX, long inY) const { return 1; }
    bool IsStepLimit(long inSteps) const { return false; }
    bool IsRevLimit(long inRevs) const { return false; }
    bool IsCheaper(unsigned long inCost, unsigned long inOld) const { return false; }
    EEAIAStarChokePointGoal();              // 0x10d5d
    virtual ~EEAIAStarChokePointGoal();     // 0x1118f (??_G 0x11173)
    // +0x10 0x10db7: the base's three members, then the map's size as the search size
    virtual void Attach(EEMap* inMap, EEAIAStarChokePointStorage* inStorage, EEAIAStarMachineBase* inMachine);
    void CollectTiles();                    // 0x10ddb: the storage's open and closed nodes get the index; mTiles gets them
    void CreateChokePoint();                // 0x10e6c: a new EEAIChokePoint of mTiles, registered in mChokePoints
    // 0x10fcc (ret 0xc): whether the fill may take the tile (marked by the terrain pass and not a 0x36 tile)
    bool AcceptTile(long inX, long inY, bool inStart);

    std::map<long, EEAIChokePoint*> mChokePoints;   // +0x30 by index (EEAIChokePoint +0x78)
    std::vector<U2DPoint<long> > mTiles;    // +0x3c
    long mWidth;                            // +0x4c (the map's size: Virtual10)
    long mHeight;                           // +0x50
    unsigned long GetAreaSerialsAt(unsigned long inIndex) const { return this->mAreaSerials[inIndex]; }
    std::vector<unsigned long> mAreaSerials;       // +0x54 (the serials of the areas around the accepted tiles)
    long mTileCount;                        // +0x64 (counted by AcceptTile; reserves mTiles)
    long mNextIndex;                        // +0x68 (= 1)
};

#endif
