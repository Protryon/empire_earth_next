// C:\EEX\Empire Earth\AI Thread\EEAIAStarPathSectorGoal.h (RECONSTRUCTION; file name inferred). RTTI
// EEAIAStarPathSectorGoal > EEAIAStarGoalBase<EEAIAStarPathSectorStorage, EEMap>; vtable 0x43a0a8 (5 slots, all the
// base's; EEAIAStarPathSectorGoal.cpp 0x1a9ec..0x1b7dc): the flood fill that cuts the path sub-sectors
// (EEAIPathSubSector, ids from EEAIContinentManager::NewSubSectorID). Members by offset (provisional names); the two
// rectangles have U2DRectangle<long>'s ctor store order (+0xc, +4, +0, +8). Requires ee/EEAIAStarGoalBase.h,
// ee/EEAIAStarPathSectorStorage.h, ee/EEMap.h, lle/U2DRectangle.h, <map>, <vector>.
#ifndef EE_EEAIASTARPATHSECTORGOAL_H
#define EE_EEAIASTARPATHSECTORGOAL_H

class EEAIPathSubSector;
class EEAIAStarPathAreaStorage;           // ee/EEAIAStarPathAreaStorage.h

class EEAIAStarPathSectorGoal : public EEAIAStarGoalBase<EEAIAStarPathSectorStorage, EEMap> {
public:
    // the machine's hooks, constants for a flood fill (EEAIAStarMachine<S, G, M>: no goal node but the empty list,
    // no estimate, unit steps, no limits, no reopening) (provisional names)
    bool IsGoalNode(EEAIAStarNode* inNode) const { return inNode == NULL; }
    long Estimate(const U2DPoint<long>& inFrom, const U2DPoint<long>& inTo) const { return 1; }
    long GetCost(long inFromX, long inFromY, long inX, long inY) const { return 1; }
    bool IsStepLimit(long inSteps) const { return false; }
    bool IsRevLimit(long inRevs) const { return false; }
    bool IsCheaper(unsigned long inCost, unsigned long inOld) const { return false; }
    EEAIAStarPathSectorGoal();              // 0x1a9ec
    virtual ~EEAIAStarPathSectorGoal();     // 0x1b6a8
    void CollectTiles();                    // 0x1aac4: a new sub-sector id; the storage's open/closed nodes get it
    void CreateSubSector();                 // 0x1ab68: a new EEAIPathSubSector over mTiles
    // 0x1acbf (ret 0xc): whether the fill may take the tile (0x1ae1d the detailed test)
    bool AcceptTile(unsigned long inX, unsigned long inY, long inStart);
    // 0x1ae1d (ret 0x10; not written yet: a switch over the region kind mLA4) (provisional name)
    bool ClassifyTile(EETile* inTile, unsigned long inX, unsigned long inY, bool inArg);

    std::map<unsigned short, EEAIPathSubSector*> mSubSectors;   // +0x30 (CreateSubSector inserts mNextID)
    std::vector<EETileCoordinate> mStaleTiles;  // +0x3c
    std::vector<unsigned short> mV4C;       // +0x4c
    std::vector<U2DPoint<long> > mTiles;    // +0x5c
    U2DRectangle<long> mSectorBounds;       // +0x6c (AcceptTile tests the tile against it)
    U2DRectangle<long> mTileBounds;         // +0x7c (the first tile; CreateSubSector copies it to the storage)
    EEAIPathSector* mSector;                // +0x8c (the sector: CreateSubSector inserts into its +0x04 map)
    // +0x90: the path-area fill's storage (CreatePathSectors 0x20b48 stores its EEAIAStarPathAreaStorage* argument;
    // AcceptTile reads its +0x40 width and +0x60 per-tile path-area ids)
    EEAIAStarPathAreaStorage* GetPathAreaStorage() const { return this->mPathAreaStorage; }
    EEAIAStarPathAreaStorage* mPathAreaStorage;
    long GetTileCount() const { return this->mTileCount; }
    long mTileCount;                        // +0x94 (reserves mTiles)
    long mUnitID;                           // +0x98 (= -1)
    long mPlayerID;                         // +0x9c
    unsigned short mWA0;                    // +0xa0
    unsigned short mWA2;                    // +0xa2
    long mKind;                             // +0xa4
    unsigned short mContinentID;            // +0xa8 (the sub-sector's +0x4c)
    unsigned char mCategory;                // +0xaa
    unsigned short GetPathAreaID() const { return this->mPathAreaID; }
    unsigned short mPathAreaID;             // +0xac (the sub-sector's +0x4e; the first tile's id)
    unsigned short mNextID;                 // +0xae (= 1; NewSubSectorID)
    long GetPathAreaKind() const { return this->mPathAreaKind; }
    long mPathAreaKind;                     // +0xb0
    bool GetClassifyTiles() const { return this->mClassifyTiles; }
    bool mClassifyTiles;                    // +0xb4
};

#endif
