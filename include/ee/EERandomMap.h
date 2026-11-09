// The game's random-map state as the generator PIMs see it (the pointer every EERMPIM* export receives).
// Defined by the game (C:\EEX\Empire Earth\Random Map\...); file name inferred; member names are provisional.
// Only what Default.dll's codegen proves: fields at proven offsets, unknown stretches as `char _gapXXX[n]`.
//
// SHARED by every Default.dll unit (and edited by several people at once):
//  - add a field by splitting a gap; never move, rename or retype an existing field without checking every
//    unit that uses it (grep src/Empire Earth/) and re-running their unit_check;
//  - one small Edit per change; if the Edit fails because the text moved, re-read and retry;
//  - each field carries `// +0xOFF proven by <rva>`.
#ifndef EE_EERANDOMMAP_H
#define EE_EERANDOMMAP_H

#include <vector>
// Requires lle/LowLevelEngine.h (URandom).

struct EERMPlayer {                         // proven sizeof 0x3c (map +0xe0 vector: index * 0x3c, 0x4da1)
    // No user-declared ctors: the implicit default ctor (0x2812: two vector ctors + GE2DPoint() at +0x20, nothing
    // else) and copy ctor (0x697f) are out-of-line COMDATs emitted after their first user (CMU unit).
    std::vector<GE2DPoint> mTiles;          // +0x00 proven by CMUCreateLandWaterMap 0x15e5 (vector<GE2DPoint>
                                            // operator= 0x4555) (provisional name)
    std::vector<GE2DPoint> mClumpCenters;   // +0x10 proven by 0x15e5 (push_back / clear of 8-byte elements)
    GE2DPoint mPosition;                    // +0x20 proven by 0x2812 (fldz x2), CMUGenerateResources 0x307e
    float mRadius;                          // +0x28 proven by 0x15e5 (max of draws, then sqrt) (provisional name)
    unsigned long mIndex;                   // +0x2c proven by CMUGenerateNeutralPositions 0x244e (loop index)
    long mTeam;                             // +0x30 proven by 0x244e (-15/-16/-14 per neutral kind) (provisional name)
    long mTileCount;                        // +0x34 proven by 0x15e5 (EEClumps 0x8129 result) (provisional name)
    bool mIsNeutral;                        // +0x38 proven by 0x78c5 (bool: passed to a bool parameter with no
                                            // setne; an unsigned char would be converted) (provisional name)
    bool mIsOuter;                          // +0x39 proven by CMUCreateFlatMap 0x115e (third case) (provisional name)
    bool mIsMoon;                           // +0x3a proven by 0x78c5 (selects EEIslandClumps vs EELandClumps)
    bool mIsPlaced;                         // +0x3b proven by 0x244e (byte store with +0x38..+0x3a) (provisional name)
};

struct EERMCircle {                         // proven sizeof 0xc (map +0x1a0 inner vectors: idiv 0xc, EELandClumps 0x9a4a)
    EERMCircle() : mRadius(0.0f) {}         // inline: EELandClumps::AddClump 0x9e27 pushes three fldz
    void Extend(const GE2DPoint& inPoint);  // 0x9df0, defined in EELandClumps.cpp (grow the radius to reach the point)

    GE2DPoint mCenter;                      // +0x00 proven by 0x9a4a (DistanceToSquared), 0x98df (= the location)
    float mRadius;                          // +0x08 proven by 0x9df0 (sqrt of the squared distance)
};

struct EERMObject {                         // proven sizeof 0x10 (map +0x120 elements; EETreeClumps 0xa798)
    GE2DPoint mLocation;                    // +0x00 (zeroed by GE2DPoint(), then the claimed cell)
    long mKind;                             // +0x08 (2 for trees) (provisional name)
    long mType;                             // +0x0c (tree type) (provisional name)
};

struct EERMRing {                         // proven sizeof 0x10 (Default.dll unit 9 point placement) (provisional names)
    GE2DPoint mCenter;                      // +0x00 proven by 0xc0db (center + rotated offset)
    float mMinRadius;                       // +0x08 proven by 0xc0db/0xc29f (lower bound of the draw / distance)
    float mMaxRadius;                       // +0x0c proven by 0xc0db/0xc29f
};

struct EERMTeam {                           // proven sizeof 0x20 (map +0x110 vector: index << 5, 0x4db3) (provisional name)
    long GetPlayersAt(unsigned long inIndex) const { return this->mPlayers[inIndex]; }
    std::vector<long> mPlayers;             // +0x00 proven by CMUGeneratePlayerPositions 0x2843 (1-based player
                                            // numbers: players[mPlayers[j] - 1])
    std::vector<GE2DPoint> mPositions;      // +0x10 proven by 0x2843 (mPositions[j] -> player position)

    // exe 0x28a05a: a user copy ctor (a push_back loop over both vectors), called out of line by the exe's
    // _Construct/_Ufill/_Ucopy<EERMTeam> (0x28ab05, 0x28aebd); hence the user-declared empty default ctor too
    EERMTeam() {}
    EERMTeam(const EERMTeam& inTeam)
    {
        for (unsigned long i = 0; i < inTeam.mPlayers.size(); i++) {
            this->mPlayers.push_back(inTeam.mPlayers[i]);
            this->mPositions.push_back(inTeam.mPositions[i]);
        }
    }

    // inline; CMUSpreadTeams 0x3aa3 calls it out of line (refused there), COMDAT 0x417c. Name is provisional.
    void SetPosition(unsigned long inIndex, const GE2DPoint& inPosition) { this->mPositions[inIndex] = inPosition; }
    void AddPlayer(long inPlayer);          // exe 0x285907 (EERandomMap::CreateTeams; push_back to both vectors)
};

struct EERMNamedEntry {                     // proven sizeof 0x18 (map +0x100 vector: size / 0x18, .rmv reader
                                            // 0xe703) (provisional names)
    UString mName;                          // +0x00 proven by 0xe703 (registered as a map type, value = index)
    long mUnitClass;                        // +0x10 unproven use
    unsigned long mSize;                    // +0x14 proven by SetRandomMapValues 0xd378 (fild of an unsigned)
};

struct EERMNamedValue {                     // proven sizeof 0x14 (map +0x140/+0x150 vectors: size / 0x14, 0xe703)
    UString mName;                          // +0x00 proven by 0xe703 (registered as a map type)
    long mValue;                            // +0x10 proven by 0xe703 (the registered value, "%d")
};

// ---- Resource/feature placement records (unit 12, 0x15c98..0x18af3). Names are provisional.
struct EERMFeature {                        // proven sizeof 0x18 (map +0x160 vector; built on the stack by 0x16035)
    float mAngle;                           // +0x00 proven by 0x16035 (draw * 2pi - pi)
    U2DPoint<float> mPosition;              // +0x04 proven by 0x16035 (jittered position). Not a GE2DPoint:
                                            // vector<EERMFeature>::insert copies the record with rep movsd
                                            // (trivial copy, 0x131bd), and the .rmv reader's frame (0xf361)
                                            // lays theFeature out as a non-POD (user-declared default ctor)
    long mUnitType;                         // +0x0c proven by 0x16035 (resource kind +0x0c)
    long mIndex;                            // +0x10 proven by 0x16035 (player / list index)
    bool mIsWorldUnit;                      // +0x14 proven by 0x16035 (= true)
    bool mInRing;                           // +0x15 proven by 0x16035 (= false)
    bool mIsHerd;                           // +0x16 proven by 0x16035 (= true)
};

struct EERMResourceKind {                   // proven sizeof 0x14 (map +0x1e0 vector: idiv 0x14, 0x168dd)
    long mMinimum;                          // +0x00 proven by 0x16f09 (radius draw between +0 and +4)
    long mMaximum;                          // +0x04
    long mId;                               // +0x08 proven by 0x168dd (looked up by id)
    long mUnitType;                         // +0x0c proven by 0x16035 (copied into the feature)
    unsigned long mArea;                    // +0x10 proven by 0x16bb7 (sqrt of an unsigned)
};

struct EERMResourceSlot {                   // proven sizeof 8 (map +0x200 vector: >> 3, 0x168dd)
    long mId;                               // +0x00 proven by 0x168dd
    bool mPerPlayer;                        // +0x04 proven by 0x168dd (one placement per player)
    bool mRandomKind;                       // +0x05 proven by 0x168dd (kind drawn from map +0x1f0)
};

// .rmv statement records (reader 0xf361; names from the script keywords, InitializeDefines 0xd943). Names are
// provisional.
struct EERMWall {                           // proven sizeof 0x14 (map +0x170 vector; "PlaceWall" pushes 5 longs)
    long mStartX;                           // +0x00 proven by 0xf361 (keyword argument 1)
    long mStartY;                           // +0x04 (argument 2)
    long mEndX;                             // +0x08 (argument 3)
    long mEndY;                             // +0x0c (argument 4)
    long mUnitType;                         // +0x10 (argument 5)
};

struct EERMTerrain {                        // proven sizeof 0x80 (map +0x180 vector: sar 7; "DeclareRandomMapTerrain")
    long mId;                               // +0x00 proven by 0xf361 (looked up by id)
    long mTextures[31];                     // +0x04 proven by 0xf361 (filled with one value, then per elevation
                                            // -10..20 by "RandomMapTerrainRange")
};

struct EERMTerrainRegion {                  // proven sizeof 0xc (map +0x190 vector; "TerrainRegion")
    long mFrom;                             // +0x00 proven by 0xf361 (argument 1)
    long mTo;                               // +0x04 (argument 2)
    unsigned long mTerrain;                 // +0x08 proven by 0xf361 (index into map +0x180)
};

struct EERMForest {                         // proven sizeof 0x18 (map +0x1c0 vector; "DefineForest")
    std::vector<unsigned long> mTreeTypes;  // +0x00 proven by 0xf361 (arguments 3..n, pushed through a temporary)
    long mId;                               // +0x10 proven by 0xf361 (argument 1)
    long mTerrainTexture;                   // +0x14 (argument 2)
};

struct EERMSpacingRule {                    // proven sizeof 0xc (map +0x210 vector: idiv 0xc, 0x18143)
    long mKind;                             // +0x00 proven by 0x18143 (5: special case)
    long mMinimum;                          // +0x04 proven by 0x18143 (radius draw)
    long mMaximum;                          // +0x08
};

struct EERandomMap {
    URandom mRandom;                      // +0x000 proven by HMUFractal::GenerateFractalValue 0xad23 (thiscall
                                            // Generate with this = the map)
    URandomIntegers mRandomIntegers;        // +0x010 proven by EEClumps ctor 0x7b58 (&map->+0x10 used for
                                            // URandomIntegers::Generate in 0x7c94) (provisional name)
    FSFileSpec mScriptDirectory;            // +0x020 proven by the .rmv reader 0xf222 (FSFileSpec copy ctor from
                                            // &map->+0x20, then SetFile(name + ".rmv")) (provisional name)
    unsigned long mKindSpacing[7][2];       // +0x068 proven by unit 12 0x172c9/0x17975 (+0x68/+0x6c + kind * 8,
                                            // fild qword: unsigned min/max per resource kind) (provisional
                                            // name)
    std::vector<std::vector<unsigned char> > mFlatTiles;  // +0x0a0 proven by 0xad23 (byte cells via [+0xa4]) (provisional name)
    std::vector<std::vector<float> > mHeights;          // +0x0b0 proven by 0xad23 (float cells via [+0xb4]) (provisional name)
    std::vector<std::vector<unsigned long> > mTiles;    // +0x0c0 proven by 0xaf40 (cell & 0x7fff) (provisional name)
    std::vector<std::vector<unsigned char> > mClaimedTiles;  // +0x0d0 proven by 0x78c5 (&map->+0xd0 -> EEClumps +0x64,
                                                        // cells via [+4][x].+4 [y]) (provisional name)
    std::vector<EERMPlayer> mPlayers;                   // +0x0e0 proven by 0x78c5 (0x3c-byte elements) (provisional name)
    std::vector<std::vector<unsigned char> > mValidTiles;  // +0x0f0 proven by EEClumps ctor 0x7b58 (-> +0x68) (provisional name)
    std::vector<EERMNamedEntry> mUnitNames;  // +0x100 proven by the .rmv reader 0xe703 (provisional name)
    std::vector<EERMTeam> mTeams;           // +0x110 proven by CMUGeneratePlayerPositions 0x2843 (provisional name)
    std::vector<EERMObject> mObjects;       // +0x120 proven by EETreeClumps ctor 0xa53e / Claim 0xa798 (push_back of a
                                            // 16-byte record, kind 2 = tree) (provisional name)
    std::vector<GE2DPoint> mWaterPoints;    // +0x130 proven by unit 12 0x172c9/0x17d7d (kind-5 sites: random
                                            // element, sar 3) (provisional name)
    std::vector<EERMNamedValue> mCliffNames;  // +0x140 proven by 0xe703 (third map-type loop) (provisional name)
    std::vector<EERMNamedValue> mTerrainNames;  // +0x150 proven by 0xe703 (second map-type loop) (provisional name)
    std::vector<EERMFeature> mFeatures;     // +0x160 proven by unit 12 0x16035 (push_back of a 0x18-byte record)
                                            // (provisional name)
    std::vector<EERMWall> mWalls;           // +0x170 proven by the .rmv reader 0xf361 ("PlaceWall") (provisional name)
    std::vector<EERMTerrain> mTerrains;     // +0x180 proven by 0xf361 ("DeclareRandomMapTerrain") (provisional name)
    std::vector<EERMTerrainRegion> mTerrainRegions;     // +0x190 proven by 0xf361 ("TerrainRegion") (provisional name)
    std::vector<std::vector<EERMCircle> > mClumpCircles;   // +0x1a0 proven by EELandClumps ctor 0x9833 (one per
                                                            // player, resized to the clump count) (provisional name)
    std::vector<unsigned long> mAvailableAnimals;   // +0x1b0 proven by the .rmv reader 0xf361 ("AvailableAnimals") (provisional name)
    std::vector<EERMForest> mForests;       // +0x1c0 proven by 0xf361 ("DefineForest") (provisional name)
    std::vector<long> mTreeTypes;           // +0x1d0 proven by EETreeClumps ctor 0xa53e (random element, sar 2) and
                                            // Claim 0xa798 (indexed by tile value modulo U3EC) (provisional name)
    std::vector<EERMResourceKind> mResourceKinds;   // +0x1e0 proven by unit 12 0x168dd (provisional name)
    std::vector<long> mResourceKindIds;     // +0x1f0 proven by 0x168dd (random element, sar 2) (provisional name)
    std::vector<EERMResourceSlot> mResourceSlots;   // +0x200 proven by 0x168dd (provisional name)
    std::vector<EERMSpacingRule> mSpacingRules;     // +0x210 proven by 0x18143 (provisional name)
    GERectangle mFlatArea;                  // +0x220 proven by EEFlatClumps 0x90ca (location + GetLeft/GetBottom/
                                            // GetRight/GetTop through the inline getters) (provisional name)
    EERMRing mRing;                         // +0x230 proven by CMUGenerateNeutralPositions 0x244e (&map->+0x230 ->
                                            // ring placement 0xc192) (provisional name)
    EERMRing mNeutralRing;                  // +0x240 proven by 0x244e (second neutral kind) (provisional name)
    EERMRing mMoonRing;                     // +0x250 proven by 0x244e (third neutral kind) (provisional name)
    long mMapSize;                          // +0x260 proven by the .rmv reader 0xe703 ("$MapSize" = "%d") (provisional name)
    long mStartEpoch;                       // +0x264 proven by 0xe703 ("$StartEpoch") (provisional name)
    long mPlayerCount;                      // +0x268 proven by 0xe703 ("$PlayerCount") (provisional name)
    float mCoastalToDeeperWaterPercentage;  // +0x26c proven by 0xad23 (> 0 and >= a URandom draw) (provisional name)
    float mConvergentSampleSize;            // +0x270 proven by 0x244e (spacing arg of 0xbf49/0xbec2) (provisional name)
    float mConvergentThreshold;             // +0x274 proven by 0x244e (next arg) (provisional name)
    float mElevationScale;                  // +0x278 proven by 0xad23 (scales the fractal offset) (provisional name)
    float mElevationUnit;                   // +0x27c proven by 0xad23 (provisional name)
    float mFlatElevation;                   // +0x280 proven by 0xad23 (height of a flagged cell) (provisional name)
    float mForestChaosLevel;                // +0x284 proven by CMUCreateTrees 0x1d93 (tree clump ctor arg) (provisional name)
    float mForestFreeRadius;                // +0x288 proven by 0x1d93 (doubled radius) (provisional name)
    float mInitialElevation;                // +0x28c proven by CMUCreateHeightMap 0xaf67 (ComputeFractal arg 1)
    // +0x290..+0x2e8: per-terrain-kind factors (CMU unit). Kind order in CMUCreateFlatMap 0x115e /
    // CMUCreateLandWaterMap 0x15e5 / CMUCreateTrees 0x1d93: player land / island / other. Names are provisional.
    float mInnerNeutralFlatChaos;           // +0x290 proven by 0x115e (clump factor)
    float mInnerNeutralLandChaos;           // +0x294 proven by 0x15e5 (clump factor)
    float mInnerNeutralPercentFlat;         // +0x298 proven by 0x115e (size factor)
    float mInnerNeutralPercentLand;         // +0x29c proven by 0x115e (share of the land: 1 - F2C8 - F29C)
    float mInnerNeutralTreePercentage;      // +0x2a0 proven by 0x1d93 (size factor)
    float mLandElevationThreshold;          // +0x2a4 proven by 0xad23 (lower clamp of land heights) (provisional name)
    float mMaxAngleBetweenTeamMembers;      // +0x2a8 proven by CMUGeneratePlayerPositions 0x2843 (arg of 0x3aa3)
    float mMaxResourceElevation;            // +0x2ac proven by unit 12 0x16461 (upper height bound) (provisional name)
    float mMaxTreeElevation;                // +0x2b0 proven by 0x16461 (upper height bound, kind 2) (provisional name)
    float mNeutralInnerRadius;              // +0x2b4 proven by the .rmv reader 0xf361 ("NeutralInnerRadius")
    float mNeutralOuterRadius;              // +0x2b8 proven by 0xf361 ("NeutralOuterRadius")
    float mOuterNeutralFlatChaos;           // +0x2bc proven by 0x115e (clump factor)
    float mOuterNeutralLandChaos;           // +0x2c0 proven by 0x15e5 (clump factor)
    float mOuterNeutralPercentFlat;         // +0x2c4 proven by 0x115e (size factor)
    float mOuterNeutralPercentLand;         // +0x2c8 proven by 0x115e (share)
    float mOuterNeutralTreePercentage;      // +0x2cc proven by 0x1d93 (size factor)
    float mPercentLand;                     // +0x2d0 proven by 0x115e (tiles = U3CC * F2D0)
    float mPlayerFlatChaos;                 // +0x2d4 proven by 0x115e (clump factor)
    float mPlayerInnerRadius;               // +0x2d8 proven by 0xf361 ("PlayerInnerRadius")
    float mPlayerLandChaos;                 // +0x2dc proven by 0x15e5 (clump factor)
    float mPlayerOuterRadius;               // +0x2e0 proven by 0xf361 ("PlayerOuterRadius")
    float mPlayerPercentFlat;               // +0x2e4 proven by 0x115e (size factor)
    float mPlayerTreePercentage;            // +0x2e8 proven by 0x1d93 (size factor)
    float mTerrainChaos;                    // +0x2ec proven by 0xf361 ("TerrainChaos")
    float mShallowsElevation;               // +0x2f0 proven by CMUDrawShallows 0x3633 (shallows height floor) (provisional name)
    float mHeightMapChaos;                  // +0x2f4 proven by 0xaf67 (ComputeFractal arg 2)
    float mHeightMapSmoothness;             // +0x2f8 proven by 0xf361 ("HeightMapSmoothness")
    float mLandfillPercentage;              // +0x2fc proven by 0x15e5 ((F2FC + F2D0) * U3CC)
    float mResourceSeperation;              // +0x300 proven by unit 12 0x17975 (squared spacing) (provisional name)
    float mHalfMapSize;                     // +0x304 proven by EELandClumps 0x9af6 ((F304 - L318) * 0.75) (provisional name)
    long mMaxIntElevation;                  // +0x308 proven by 0xf361 ("MaxIntElevation")
    long mMaxIterationsClumpGrowth;         // +0x30c proven by EEClumps ctor 0x7b58 (imul) and 0x8239 (jl) (provisional name)
    long mMaximumTreesPerClump;             // +0x310 proven by 0xf361 ("MaximumTreesPerClump")
    long mMinIntElevation;                  // +0x314 proven by 0xf361 ("MinIntElevation")
    long mWaterBorder;                      // +0x318 proven by EELandClumps 0x9af6 (jle: signed; fimul) (provisional name)
    long mShallowsWidth;                    // +0x31c proven by CMUDrawShallows 0x3633 (fild dword: shallows width) (provisional name)
    long mMaxShallowConnectionsPerPlayer;   // +0x320 proven by CMUGenerateShallows 0x3146 (jle: signed; > 0 gates
                                            // the shallows, then the per-step connection count) (provisional
                                            // name)
    long mOuterPushFromMapCenter;           // +0x324 proven by CMUAdjustPositions 0x41ca (fild dword: step) (provisional name)
    long mInnerPushFromMapCenter;           // +0x328 proven by 0x41ca (fild dword: step) (provisional name)
    long mMinimumStartPositionToMapEdge;    // +0x32c proven by 0x41ca (border = L32C + L318) (provisional name)
    long mResourceTerrainTexture[7];        // +0x330 proven by 0xf361 ("ResourceTerrainTexture" kind, value)
    long mResourcePerPlayer[7];             // +0x34c proven by unit 12 0x172c9/0x1811b (per-kind count) (provisional name)
    unsigned long mResourcePerNeutralLand[7];  // +0x368 proven by 0x17cd9 (per-kind loop bound) (provisional name)
    unsigned long mForestsPerPlayer;        // +0x384 proven by CMUCreateTrees 0x1d93 (U39C * U384 clumps) (provisional name)
    unsigned long mInnerNeutralFlatClumps;  // +0x388 proven by 0x115e (clump count) (provisional name)
    unsigned long mInnerNeutralLandClumps;  // +0x38c proven by 0x15e5 (clump count) (provisional name)
    unsigned long mMaximumY;                // +0x390 proven by EEClumps ctor 0x7b58 (-> +0x80, signed bound there);
                                            // unsigned: CMUAdjustPositions 0x41ca converts it with fild qword
                                            // (provisional name)
    unsigned long mGridSize;                // +0x394 proven by 0xaf67 (U2DFractal square dimension)
    unsigned long mMaximumX;                // +0x398 proven by EEClumps ctor 0x7b58 (-> +0x84, signed bound there);
                                            // unsigned: 0x41ca fild qword (provisional name)
    unsigned long mMaxClumpsPerForest;      // +0x39c proven by CMUCreateTrees 0x1d93 (U39C * U384) (provisional name)
    unsigned long mMaxLoopCount;            // +0x3a0 proven by EEClumps 0x8145 (unsigned loop bound) (provisional name)
    unsigned long mMaxTreeLoopCount;        // +0x3a4 proven by 0x1d93 (placement attempts) (provisional name)
    unsigned long mNumInnerNeutrals;        // +0x3a8 proven by 0x244e (second neutral kind count) (provisional name)
    unsigned long mNumOuterNeutrals;        // +0x3ac proven by 0x244e (first neutral kind count) (provisional name)
    unsigned long mNumTeamPlayers;          // +0x3b0 proven by CMUGeneratePlayerPositions 0x2843 (total) (provisional name)
    unsigned long mOuterNeutralFlatClumps;  // +0x3b4 proven by 0x115e (clump count) (provisional name)
    unsigned long mOuterNeutralLandClumps;  // +0x3b8 proven by 0x15e5 (clump count) (provisional name)
    unsigned long mPlayerFlatClumps;        // +0x3bc proven by 0x115e (clump count) (provisional name)
    unsigned long mPlayerLandClumps;        // +0x3c0 proven by 0x15e5 (clump count) (provisional name)
    unsigned long mSeed;                    // +0x3c4 proven by 0xaf67 (ComputeFractal arg 3)
    unsigned long mRiverWidth;             // +0x3c8 proven by EERMPIMGenerateNeutralPositions 0x798d (unsigned:
                                            // fild qword of (x, 0)) (provisional name)
    unsigned long mTileCount;               // +0x3cc proven by 0x115e (tiles = U3CC * F2D0) (provisional name)
    unsigned long mResourceToEdgeDistance;  // +0x3d0 proven by EETreeClumps ctor 0xa53e (-> +0xcc) (provisional name)
    unsigned long mResourceToWaterDistance;  // +0x3d4 proven by 0xa53e (-> +0xc8) (provisional name)
    unsigned long mMapCharacteristics;      // +0x3d8 proven by EELandClumps 0x9af6 (test byte & 0x18) (provisional name)
    long mCliffTexture;                     // +0x3dc proven by 0xf361 ("CliffTexture")
    unsigned long mNumMoons;                // +0x3e0 proven by 0x244e (third neutral kind count) (provisional name)
    unsigned long mMoonSize;                // +0x3e4 proven by 0x115e (squared: neutral clump size) (provisional name)
    unsigned long mMoonResources;           // +0x3e8 proven by unit 12 0x170a8 (per-player count, * 64 tries)
                                            // (provisional name)
    unsigned long mNumTerrainBands;         // +0x3ec proven by EETreeClumps::Claim 0xa798 (div: unsigned modulus) (provisional name)
    unsigned long mTerrainBandOffset;       // +0x3f0 proven by 0xa798 (added to the tile value) (provisional name)
    bool mTeamGrowth;                       // +0x3f4 proven by EEIslandClumps 0x9451 (selects the team-aware test) (provisional name)
    bool mFilterHeightMap;                  // +0x3f5 proven by 0xf361 ("FilterHeightMap" > 0)
    bool mTestingLandDistribution;          // +0x3f6 proven by 0xf361 ("TestingLandDistribution" > 0)
    bool mGenerateShallows;                 // +0x3f7 proven by CMUGenerateShallows 0x3146 (shallows on) (provisional name)
    bool mIsSpaceMap;                       // +0x3f8 proven by 0xad23 (gates the first tile test) (provisional name)
};

#endif
