// C:\EEX\Empire Earth\AI Thread\EEAIAStarPathAreaGoal.h (RECONSTRUCTION, STUB; file name inferred). RTTI
// EEAIAStarPathAreaGoal > EEAIAStarGoalBase<EEAIAStarPathAreaStorage, EEMap>
// (EEAIAStarPathAreaGoal.cpp 0x173cc..0x19072, not written): the flood fill that grows the path areas and forests of one
// kind. Ctor 0x173cc, dtor 0x18ac8, CollectTiles 0x1747c, CreateAreas 0x17510. sizeof 0xa0 (EEAIContinentManager's
// path-area machine: goal +0x84, map +0x124). Members only where EEAIContinentManager 0x204ea/0x208cc read them
// (provisional names). Requires ee/EEAIAStarGoalBase.h, ee/EEAIAStarPathAreaStorage.h, ee/EEMap.h, <map>.
#ifndef EE_EEAIASTARPATHAREAGOAL_H
#define EE_EEAIASTARPATHAREAGOAL_H

class EEAIPathArea;
class EEAIForest;
class EETile;

// the full layout and members (EEAIAStarPathAreaGoal.cpp 0x173cc..0x19072)
class EEAIAStarPathAreaGoal : public EEAIAStarGoalBase<EEAIAStarPathAreaStorage, EEMap> {
public:
    // the machine's hooks, constants for a flood fill (EEAIAStarMachine<S, G, M>: no goal node but the empty list,
    // no estimate, unit steps, no limits, no reopening) (provisional names)
    bool IsGoalNode(EEAIAStarNode* inNode) const { return inNode == NULL; }
    long Estimate(const U2DPoint<long>& inFrom, const U2DPoint<long>& inTo) const { return 1; }
    long GetCost(long inFromX, long inFromY, long inX, long inY) const { return 1; }
    bool IsStepLimit(long inSteps) const { return false; }
    bool IsRevLimit(long inRevs) const { return false; }
    bool IsCheaper(unsigned long inCost, unsigned long inOld) const { return false; }
    EEAIAStarPathAreaGoal();                // 0x173cc
    virtual ~EEAIAStarPathAreaGoal();       // 0x18ac8 (??_G 0x18aac)
    void CollectTiles();                    // 0x1747c: the storage's open and closed nodes get the id; mTiles gets them
    // 0x17510: a new EEAIPathArea (and, on kind 4, an EEAIForest) of mTiles; in a kind pass (mMode) the kind ids
    void CreateAreas();
    // 0x176c7 (ret 0x18): whether a kind pass may take the tile; *ioKind is the first tile's class (0: set it)
    // Provisional name
    bool ClassifyTile(EETile* inTile, EEMap* inMap, long inX, long inY, unsigned char* ioKind, bool inShallowsAreWater);
    void AssignKindIDs();                   // 0x17a4b: the kind id into the tiles' path areas and their continents
    bool AcceptTile(long inX, long inY, bool inStart);   // 0x17bcb (ret 0xc): whether the fill may take the tile
    // inline (COMDAT 0x184d1, ret 0x10): ClassifyTile with the goal's map and continent kind (provisional name)
    bool ClassifyTile(EETile* inTile, long inX, long inY, long inArg);
    // inline (COMDAT 0x18a84): the path area of tile (inX, inY), 0 off the grid (provisional name)
    EEAIPathArea* GetArea(unsigned long inX, unsigned long inY);

    std::map<unsigned short, EEAIPathArea*> mPathAreas;   // +0x30 (0x208cc: mPathAreas[+0x7a] of the manager)
    std::map<unsigned short, EEAIForest*> mForests;       // +0x3c (0x208cc: the manager's mForests[+0x82])
    std::vector<U2DPoint<long> > mTiles;    // +0x48
    EEAIAStarBounds mTileBounds;            // +0x58 (the accepted tiles' bounds; copied to the storage's mDirtyBounds)
    EEAIPathArea** mAreas;                  // +0x68 (0x204ea: the map's path areas, x * mAreasSize + y)
    long mOwnerID;                          // +0x6c (-1; the first tile's unit's mUnitID) (provisional name)
    long mPlayerID;                         // +0x70 (that unit's player's mPlayerID) (provisional name)
    unsigned long mAreasSize;               // +0x74
    long mTileCount;                        // +0x78 (counted by AcceptTile; reserves mTiles)
    long mResources1;                       // +0x7c (resource tiles by kind 1..3; to the forest's +0x6c/+0x70/+0x74)
    long mResources2;                       // +0x80
    long mResources3;                       // +0x84
    unsigned short mContinentID;            // +0x88 (the first tile's continent; 0 between areas)
    unsigned char mContinentKind;           // +0x8a (that continent's +0x11a)
    unsigned short mForestID;               // +0x8c
    unsigned short mNextForestID;           // +0x8e (= 1; 0x208cc resizes the manager's mForests to it)
    unsigned short mNextPathAreaID;         // +0x90 (= 1; 0x208cc: the manager's mPathAreas size and mNextPathAreaID)
    long mKind;                             // +0x94 (the tile kind 1..0xf of the area)
    long mMode;                             // +0x98 (0: path areas; 1..4: 0x204ea's kind pass)
    unsigned short GetNextKindID() const { return this->mNextKindID; }
    unsigned short mNextKindID;             // +0x9c (0x204ea: the manager's mNextID124..mNextID130 by kind)
};

#endif
