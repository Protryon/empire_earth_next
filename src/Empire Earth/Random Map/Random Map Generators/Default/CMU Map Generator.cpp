// C:\EEX\Empire Earth\Random Map\Random Map Generators\Default\CMU Map Generator.cpp (file name inferred:
// the first unit in link order, before Default.cpp; its debug strings say "CMU")
// Unit flags (calibrated, docs/CALIBRATION.md): -O1 -G6 -MD -GR
#include <windows.h>
#include <float.h>
#include <math.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/U2DGeometry.h"
#include "ee/EERandomMap.h"
#include "CMU Map Generator.h"
#include "EEClumps.h"
#include "EEFlatClumps.h"
#include "EETreeClumps.h"
#include "Unit09.h"
#include "Unit11.h"
#include "Unit12.h"
#include "ee/EERandomMapIDs.h"

// ---- Unit-local types (provisional names)
struct CMUSegment {                         // proven sizeof 0x10 (vector stride, CMUConnectNext 0x33b8)
    GE2DPoint mStart;                       // +0x00
    GE2DPoint mEnd;                         // +0x08
};

struct CMUTeamAngle {                       // proven sizeof 0xc (vector stride, CMUSpreadTeams 0x3aa3); no ctor:
    float mDistance;                        // +0x00 resize() copies an uninitialized temporary
    float mAngle;                           // +0x04
    unsigned long mIndex;                   // +0x08
};

struct CMUCandidate : public CMUSegment {   // proven sizeof 0x14; push_back into a vector<CMUSegment> slices it
    float mDistance;                        // +0x10 (squared length; minimum picked first)
};

void CMUAdjustPositions(EERandomMap* ioMap);
bool CMUIsCellSet(const std::vector<std::vector<unsigned char> >& inGrid, unsigned long inX, unsigned long inY);
void CMUDrawShallows(EERandomMap* ioMap, std::vector<CMUSegment>& inSegments);
void CMUConnectNext(std::vector<GE2DPoint>& ioRemaining, std::vector<GE2DPoint>& ioConnected,
                    std::vector<CMUSegment>& ioSegments, unsigned long inCount);
void CMUAssignTeamPositions(EERandomMap* ioMap, std::vector<EERMTeam>& ioTeams, std::vector<GE2DPoint>& inPositions);
void CMUSpreadTeams(std::vector<EERMTeam>& ioTeams, EERMRing* inRing, float inSpread);
float CMUWrapAngle(float inAngle);

// Compiler-generated for this unit (names in analysis/default/names.txt):
// TEMPLATE: DEFAULT 0x10003632
// TEMPLATE: DEFAULT 0x100048e4
// TEMPLATE: DEFAULT 0x10004974
// TEMPLATE: DEFAULT 0x10005080
// TEMPLATE: DEFAULT 0x10005156
// TEMPLATE: DEFAULT 0x1000636c
// TEMPLATE: DEFAULT 0x1000755e
// TEMPLATE: DEFAULT 0x10007583
// TEMPLATE: DEFAULT 0x10004e78
// TEMPLATE: DEFAULT 0x10005d89
// TEMPLATE: DEFAULT 0x1000603d
// TEMPLATE: DEFAULT 0x1000729c
// TEMPLATE: DEFAULT 0x100060b8
// TEMPLATE: DEFAULT 0x10006bf7
// TEMPLATE: DEFAULT 0x10006c6c
// TEMPLATE: DEFAULT 0x10006f09
// TEMPLATE: DEFAULT 0x10006f35
// TEMPLATE: DEFAULT 0x10006f52
// TEMPLATE: DEFAULT 0x10006f7e
// TEMPLATE: DEFAULT 0x10006fa2
// TEMPLATE: DEFAULT 0x10006fce
// TEMPLATE: DEFAULT 0x10006feb
// TEMPLATE: DEFAULT 0x10007017
// TEMPLATE: DEFAULT 0x1000719a
// TEMPLATE: DEFAULT 0x100071ac
// TEMPLATE: DEFAULT 0x10004555
// TEMPLATE: DEFAULT 0x100046c7
// TEMPLATE: DEFAULT 0x100069fb
// TEMPLATE: DEFAULT 0x10006c22
// TEMPLATE: DEFAULT 0x100073b0
// TEMPLATE: DEFAULT 0x10007518
// TEMPLATE: DEFAULT 0x10006b37
// TEMPLATE: DEFAULT 0x100046e7
// TEMPLATE: DEFAULT 0x10004713
// TEMPLATE: DEFAULT 0x100047c5
// TEMPLATE: DEFAULT 0x100048be
// TEMPLATE: DEFAULT 0x10004ad6
// TEMPLATE: DEFAULT 0x10004bf6
// TEMPLATE: DEFAULT 0x10004dc5
// TEMPLATE: DEFAULT 0x10004f2b
// TEMPLATE: DEFAULT 0x10004f9c
// TEMPLATE: DEFAULT 0x10005047
// TEMPLATE: DEFAULT 0x1000697f
// TEMPLATE: DEFAULT 0x10006a71
// TEMPLATE: DEFAULT 0x10006a75
// TEMPLATE: DEFAULT 0x10006a98
// TEMPLATE: DEFAULT 0x10006ab5
// TEMPLATE: DEFAULT 0x10006afc
// TEMPLATE: DEFAULT 0x10006b09
// TEMPLATE: DEFAULT 0x10006b8d
// TEMPLATE: DEFAULT 0x10006d3a
// TEMPLATE: DEFAULT 0x10006d5a
// TEMPLATE: DEFAULT 0x10006e1f
// TEMPLATE: DEFAULT 0x10006e4b
// TEMPLATE: DEFAULT 0x10006e68
// TEMPLATE: DEFAULT 0x1000703b
// TEMPLATE: DEFAULT 0x10007154
// TEMPLATE: DEFAULT 0x10007176
// TEMPLATE: DEFAULT 0x10007188
// TEMPLATE: DEFAULT 0x10007207
// TEMPLATE: DEFAULT 0x10007568
// TEMPLATE: DEFAULT 0x1000758f
// TEMPLATE: DEFAULT 0x100075b4
// TEMPLATE: DEFAULT 0x100075d6
// TEMPLATE: DEFAULT 0x1000763a
// TEMPLATE: DEFAULT 0x10007655
// TEMPLATE: DEFAULT 0x10007687
// TEMPLATE: DEFAULT 0x100076bc
// TEMPLATE: DEFAULT 0x10004817
// TEMPLATE: DEFAULT 0x100049a0
// TEMPLATE: DEFAULT 0x100051a6
// TEMPLATE: DEFAULT 0x10004ca9
// TEMPLATE: DEFAULT 0x100057e2
// SYNTHETIC: DEFAULT 0x1000417c
// TEMPLATE: DEFAULT 0x10004777
// TEMPLATE: DEFAULT 0x100047f1
// TEMPLATE: DEFAULT 0x10004898
// TEMPLATE: DEFAULT 0x10004910
// TEMPLATE: DEFAULT 0x10004b20
// TEMPLATE: DEFAULT 0x10004d12
// TEMPLATE: DEFAULT 0x1000525a
// TEMPLATE: DEFAULT 0x10005537
// TEMPLATE: DEFAULT 0x1000579e
// TEMPLATE: DEFAULT 0x10006ce7
// TEMPLATE: DEFAULT 0x10006d11
// TEMPLATE: DEFAULT 0x10006d7e
// TEMPLATE: DEFAULT 0x10007127
// SYNTHETIC: DEFAULT 0x100071be
// SYNTHETIC: DEFAULT 0x100071f3
// TEMPLATE: DEFAULT 0x10007264
// SYNTHETIC: DEFAULT 0x10007545
// SYNTHETIC: DEFAULT 0x1000361c
// TEMPLATE: DEFAULT 0x100046da
// TEMPLATE: DEFAULT 0x100047b2
// TEMPLATE: DEFAULT 0x10004852
// TEMPLATE: DEFAULT 0x1000488b
// TEMPLATE: DEFAULT 0x10004bc2
// TEMPLATE: DEFAULT 0x100050da
// TEMPLATE: DEFAULT 0x10005141
// SYNTHETIC: DEFAULT 0x100069dc
// TEMPLATE: DEFAULT 0x10006b7e
// TEMPLATE: DEFAULT 0x10006c97
// SYNTHETIC: DEFAULT 0x10006cc2
// TEMPLATE: DEFAULT 0x10006da6
// TEMPLATE: DEFAULT 0x10006dcb
// TEMPLATE: DEFAULT 0x10006de1
// TEMPLATE: DEFAULT 0x10006e02
// TEMPLATE: DEFAULT 0x10007053
// TEMPLATE: DEFAULT 0x1000707f
// TEMPLATE: DEFAULT 0x1000709c
// TEMPLATE: DEFAULT 0x100070c8
// TEMPLATE: DEFAULT 0x100070f0
// SYNTHETIC: DEFAULT 0x10007165
// SYNTHETIC: DEFAULT 0x100071d0
// TEMPLATE: DEFAULT 0x1000485f
// TEMPLATE: DEFAULT 0x10004a86
// TEMPLATE: DEFAULT 0x10007280
// SYNTHETIC: DEFAULT 0x10002812
// TEMPLATE: DEFAULT 0x100046ae
// TEMPLATE: DEFAULT 0x100049db
// TEMPLATE: DEFAULT 0x10004d4a
// TEMPLATE: DEFAULT 0x10004da1
// TEMPLATE: DEFAULT 0x10004db3
// TEMPLATE: DEFAULT 0x10004f64
// TEMPLATE: DEFAULT 0x10005114
// TEMPLATE: DEFAULT 0x10005294
// TEMPLATE: DEFAULT 0x100052a6
// TEMPLATE: DEFAULT 0x10005a96
// SYNTHETIC: DEFAULT 0x10005d75
// TEMPLATE: DEFAULT 0x100060a6
// TEMPLATE: DEFAULT 0x100063d5
// TEMPLATE: DEFAULT 0x100063e2
// TEMPLATE: DEFAULT 0x100066ba
// TEMPLATE: DEFAULT 0x100066c6
// TEMPLATE: DEFAULT 0x10006a42
// TEMPLATE: DEFAULT 0x10006bb8
// TEMPLATE: DEFAULT 0x10006be3
// TEMPLATE: DEFAULT 0x10006e94
// TEMPLATE: DEFAULT 0x10006ec0
// TEMPLATE: DEFAULT 0x10006edd
// SYNTHETIC: DEFAULT 0x1000752e
// SYNTHETIC: DEFAULT 0x10001000
// SYNTHETIC: DEFAULT 0x1000100a
// SYNTHETIC: DEFAULT 0x1000101b
// SYNTHETIC: DEFAULT 0x10001027
// SYNTHETIC: DEFAULT 0x10001032
// SYNTHETIC: DEFAULT 0x1000103c
// SYNTHETIC: DEFAULT 0x1000104d
// SYNTHETIC: DEFAULT 0x10001059
// SYNTHETIC: DEFAULT 0x10001064
// SYNTHETIC: DEFAULT 0x1000106e
// SYNTHETIC: DEFAULT 0x1000107f
// SYNTHETIC: DEFAULT 0x1000108b
// SYNTHETIC: DEFAULT 0x10001096
// SYNTHETIC: DEFAULT 0x100010a0
// SYNTHETIC: DEFAULT 0x100010b1
// SYNTHETIC: DEFAULT 0x100010bd
// SYNTHETIC: DEFAULT 0x100010c8
// SYNTHETIC: DEFAULT 0x100010d2
// SYNTHETIC: DEFAULT 0x100010e3
// SYNTHETIC: DEFAULT 0x100010ef
// SYNTHETIC: DEFAULT 0x100010fa
// SYNTHETIC: DEFAULT 0x10001104
// SYNTHETIC: DEFAULT 0x10001115
// SYNTHETIC: DEFAULT 0x10001121
// SYNTHETIC: DEFAULT 0x1000112c
// SYNTHETIC: DEFAULT 0x10001136
// SYNTHETIC: DEFAULT 0x10001147
// SYNTHETIC: DEFAULT 0x10001153

// FUNCTION: DEFAULT 0x1000115e
// Grows the flat area of every player, neutral and moon from the script's flat shares and clump
// settings, one flat-area grower per owner
long CMUCreateFlatMap(EERandomMap* ioMap)
{
    std::vector<unsigned long> theSizes;
    std::vector<std::vector<unsigned long> > theDistances;
    std::vector<EEClumps*> theClumps;
    unsigned long theStart = TMGetTime();
    unsigned long thePlayerCount = ioMap->mPlayers.size();
    float theWaterShare = ioMap->mOuterNeutralPercentLand;
    float theLandShare = ioMap->mInnerNeutralPercentLand;
    float theRest = 1.0f - ioMap->mOuterNeutralPercentLand - ioMap->mInnerNeutralPercentLand;
    unsigned long theTiles = UMFloatToLong(ioMap->mTileCount * ioMap->mPercentLand);
    float theTileCount = theTiles;
    unsigned long thePlayerTiles = UMFloatToLong(theTileCount * theRest * ioMap->mPlayerPercentFlat /
        (long)(thePlayerCount - ioMap->mNumMoons - ioMap->mNumOuterNeutrals - ioMap->mNumInnerNeutrals));
    unsigned long theLandTiles = UMFloatToLong(theTileCount * theLandShare * ioMap->mInnerNeutralPercentFlat / ioMap->mNumInnerNeutrals);
    unsigned long theWaterTiles = UMFloatToLong(theTileCount * theWaterShare * ioMap->mOuterNeutralPercentFlat / ioMap->mNumOuterNeutrals);
    unsigned long theNeutralTiles = ioMap->mMoonSize * ioMap->mMoonSize;
    unsigned long i, j;
    theClumps.resize(thePlayerCount);
    theDistances.resize(thePlayerCount);
    theSizes.resize(thePlayerCount);
    for (i = 0; i < thePlayerCount; i++) {
        long theX = UMFloatToLong(ioMap->mPlayers[i].mPosition.GetX());
        long theY = UMFloatToLong(ioMap->mPlayers[i].mPosition.GetY());
        unsigned long theCount;
        float theSpread;
        unsigned long theTotal;
        if (ioMap->mPlayers[i].mIsNeutral) {
            if (ioMap->mPlayers[i].mIsMoon) {
                theCount = 1;
                theSpread = 0.8f;
                theTotal = theNeutralTiles;
            } else if (ioMap->mPlayers[i].mIsOuter) {
                theCount = ioMap->mOuterNeutralFlatClumps;
                theSpread = ioMap->mOuterNeutralFlatChaos;
                theTotal = theWaterTiles;
            } else {
                theCount = ioMap->mInnerNeutralFlatClumps;
                theSpread = ioMap->mInnerNeutralFlatChaos;
                theTotal = theLandTiles;
            }
        } else {
            theCount = ioMap->mPlayerFlatClumps;
            theSpread = ioMap->mPlayerFlatChaos;
            theTotal = thePlayerTiles;
        }
        unsigned long theTarget = theTotal / theCount;
        unsigned long theDistance = UMFloatToLong((float)sqrt(theTarget));
        for (j = 0; j < theCount; j++)
            theDistances[i].push_back(theDistance);
        EEClumps* theClump = new EEFlatClumps(ioMap, &theDistances[i], theCount, theSpread, theTarget,
                                              ioMap->mPlayers[i].mIndex, ioMap->mPlayers[i].mIsNeutral);
        theClumps[i] = theClump;
        theClumps[i]->Begin(ioMap->mPlayers[i].mPosition);
        theClump->Generate();
        theSizes[i] = theClump->GetTotalSize();
        delete theClump;
    }
    ESDebugOut("CMUCreateFlatMap: time:     %6.2f s\n", (float)(TMGetTime() - theStart) * 0.001f);
    for (i = 0; i < theSizes.size(); i++)
        theSizes[i];
    theSizes.clear();
    theDistances.clear();
    return 0;
}

// FUNCTION: DEFAULT 0x100015e5
// Grows every owner's land mass step by step in turn until none grows, then records each owner's
// clump centres, tiles and radius (furthest tile from the start)
long CMUCreateLandWaterMap(EERandomMap* ioMap, CMUClumpsFactory inFactory)
{
    std::vector<std::vector<unsigned long> > theDistances;
    std::vector<EEClumps*> theClumps;
    unsigned long theStart = TMGetTime();
    std::vector<EERMPlayer>& thePlayers = ioMap->mPlayers;
    unsigned long thePlayerCount = thePlayers.size();
    unsigned long i, j, k;
    if (ioMap->mClumpCircles.empty())
        ioMap->mClumpCircles.resize(thePlayerCount);
    float theWaterShare = ioMap->mOuterNeutralPercentLand;
    float theLandShare = ioMap->mInnerNeutralPercentLand;
    float theRest = 1.0f - ioMap->mOuterNeutralPercentLand - ioMap->mInnerNeutralPercentLand;
    unsigned long theTiles = UMFloatToLong((ioMap->mLandfillPercentage + ioMap->mPercentLand) * ioMap->mTileCount);
    float theTileCount = theTiles;
    unsigned long thePlayerTiles = UMFloatToLong(theTileCount * theRest /
        (thePlayerCount - ioMap->mNumMoons - ioMap->mNumOuterNeutrals - ioMap->mNumInnerNeutrals));
    unsigned long theLandTiles = UMFloatToLong(theTileCount * theLandShare / ioMap->mNumInnerNeutrals);
    unsigned long theWaterTiles = UMFloatToLong(theTileCount * theWaterShare / ioMap->mNumOuterNeutrals);
    unsigned long theNeutralTiles = ioMap->mMoonSize * ioMap->mMoonSize;
    theClumps.resize(thePlayerCount);
    theDistances.resize(thePlayerCount);
    for (i = 0; i < thePlayerCount; i++) {
        long theX = UMFloatToLong(thePlayers[i].mPosition.GetX());
        long theY = UMFloatToLong(ioMap->mPlayers[i].mPosition.GetY());
        unsigned long theCount;
        float theSpread;
        unsigned long theTotal;
        if (ioMap->mPlayers[i].mIsNeutral) {
            if (ioMap->mPlayers[i].mIsMoon) {
                theTotal = theNeutralTiles;
                theCount = 1;
                theSpread = 0.8f;
            } else if (thePlayers[i].mIsOuter) {
                theSpread = ioMap->mOuterNeutralLandChaos;
                theCount = ioMap->mOuterNeutralLandClumps;
                theTotal = theWaterTiles;
            } else {
                theSpread = ioMap->mInnerNeutralLandChaos;
                theCount = ioMap->mInnerNeutralLandClumps;
                theTotal = theLandTiles;
            }
        } else {
            theSpread = ioMap->mPlayerLandChaos;
            theCount = ioMap->mPlayerLandClumps;
            theTotal = thePlayerTiles;
        }
        unsigned long theTarget = theTotal / theCount;
        unsigned long theDistance = UMFloatToLong((float)sqrt(theTarget));
        for (j = 0; j < theCount; j++)
            theDistances[i].push_back(theDistance);
        theClumps[i] = inFactory(ioMap, theDistances[i], i, theCount, theSpread, theTotal);
        theClumps[i]->Begin(thePlayers[i].mPosition);
    }
    unsigned long theRounds = ioMap->mMaxIterationsClumpGrowth * ioMap->mMaxIterationsClumpGrowth;
    bool theGrowing;
    for (k = 0; k < theRounds; k++) {
        theGrowing = false;
        for (i = 0; i < thePlayerCount; i++)
            theGrowing = theClumps[i]->Step() || theGrowing;
        if (!theGrowing)
            break;
    }
    for (i = 0; i < thePlayerCount; i++) {
        ioMap->mPlayers[i].mClumpCenters.clear();
        for (j = 0; j < theClumps[i]->mClumpCount; j++) {
            if (!(theClumps[i]->mSizes[j] > 0))
                continue;
            ioMap->mPlayers[i].mClumpCenters.push_back(theClumps[i]->mCenters[j]);
            float theCenterX = theClumps[i]->mCenters[j].GetX();
            float theCenterY = theClumps[i]->mCenters[j].GetY();
        }
    }
    for (i = 0; i < thePlayerCount; i++) {
        ioMap->mPlayers[i].mTileCount = theClumps[i]->GetTotalSize();
        ioMap->mPlayers[i].mTiles = theClumps[i]->mPlaced;
        if (thePlayers[i].mIsNeutral)
            continue;
        for (j = 0; j < thePlayers[i].mTiles.size(); j++) {
            float theDistance = thePlayers[i].mPosition.DistanceToSquared(thePlayers[i].mTiles[j]);
            if (theDistance > thePlayers[i].mRadius)
                thePlayers[i].mRadius = theDistance;
        }
        thePlayers[i].mRadius = sqrt(thePlayers[i].mRadius);
    }
    for (i = 0; i < thePlayerCount; i++)
        delete theClumps[i];
    theClumps.clear();
    theDistances.clear();
    unsigned long theTime = TMGetTime() - theStart;
    for (i = 0; i < thePlayers.size(); i++)
        ESDebugOut("::CMUCreateLandWaterMap: P%ld tiles:   %ld\n", i, thePlayers[i].mTileCount);
    return 0;
}

// FUNCTION: DEFAULT 0x10001d93
// Plants each owner's forests: for each forest a centre on a ring around one of its clump centres,
// then a forest grower sized from the script's tree share
long CMUCreateTrees(EERandomMap* ioMap)
{
    GE2DPoint theCenter;
    GE2DPoint thePoint;
    EERMRing theRing;
    std::vector<unsigned long> theDistances;
    std::vector<unsigned long> theTreeCounts;
    unsigned long thePlayerCount = ioMap->mPlayers.size();
    unsigned long theX = 0;
    unsigned long theY = 0;
    unsigned long theTreeTotal = 0;
    unsigned long theStart = TMGetTime();
    unsigned long i, j, n;
    ESDebugOut("CMUCreateTrees start of function\n");
    if (ioMap->mTreeTypes.size() != 0 && ioMap->mMaxClumpsPerForest * ioMap->mForestsPerPlayer > 0) {
        theTreeCounts.resize(ioMap->mPlayers.size());
        float theWaterShare = ioMap->mOuterNeutralPercentLand;
        float theLandShare = ioMap->mInnerNeutralPercentLand;
        float theRest = 1.0f - ioMap->mOuterNeutralPercentLand - ioMap->mInnerNeutralPercentLand;
        unsigned long theTiles = UMFloatToLong(ioMap->mTileCount * ioMap->mPercentLand);
        float theTileCount = theTiles;
        unsigned long thePlayerTrees = UMFloatToLong(theRest * theTileCount * ioMap->mPlayerTreePercentage /
            (thePlayerCount - ioMap->mNumMoons - ioMap->mNumOuterNeutrals - ioMap->mNumInnerNeutrals));
        unsigned long theLandTrees = UMFloatToLong(theLandShare * theTileCount * ioMap->mInnerNeutralTreePercentage / ioMap->mNumInnerNeutrals);
        unsigned long theWaterTrees = UMFloatToLong(theWaterShare * theTileCount * ioMap->mOuterNeutralTreePercentage / ioMap->mNumOuterNeutrals);
        float theSpread = ioMap->mForestChaosLevel;
        for (i = 0; i < ioMap->mPlayers.size(); i++) {
            unsigned long theClumpCount = ioMap->mMaxClumpsPerForest;
            unsigned long thePerClump = ioMap->mForestsPerPlayer;
            unsigned long theTotal;
            if (ioMap->mPlayers[i].mIsNeutral) {
                if (ioMap->mPlayers[i].mIsMoon)
                    continue;
                theTotal = ioMap->mPlayers[i].mIsOuter ? theWaterTrees : theLandTrees;
            } else
                theTotal = thePlayerTrees;
            unsigned long theSize = theTotal / (thePerClump * theClumpCount);
            if (theSize == 0)
                theSize = 1;
            unsigned long theDistance = UMFloatToLong((float)sqrt(theSize));
            unsigned long thePoints = ioMap->mPlayers[i].mClumpCenters.size();
            if (thePoints == 0)
                continue;
            unsigned long theStep;
            if (thePoints > thePerClump)
                theStep = thePoints / thePerClump + 1;
            else if (thePoints < thePerClump)
                theStep = thePerClump / thePoints + 1;
            else
                theStep = 1;
            unsigned long theIndex = 0;
            for (j = 0; j < thePerClump; j++) {
                theDistances.clear();
                for (n = 0; n < theClumpCount; n++)
                    theDistances.push_back(theDistance);
                EEClumps* theClumps = new EETreeClumps(ioMap, &ioMap->mClaimedTiles, theSpread, theClumpCount, theSize,
                                                       &theDistances, ioMap->mPlayers[i].mIndex,
                                                       ioMap->mPlayers[i].mIsNeutral);
                theRing.mMinRadius = ioMap->mForestFreeRadius;
                theRing.mMaxRadius = theRing.mMinRadius + theRing.mMinRadius;
                theRing.mCenter = ioMap->mPlayers[i].mClumpCenters[theIndex];
                unsigned long theNext = (theStep + theIndex) % thePoints;
                bool theFound = false;
                float theAngle = ioMap->mRandom.Generate() * 6.2831855f;
                for (n = 0; n < ioMap->mMaxTreeLoopCount && !theFound; n++) {
                    PlacePointOnRing(ioMap->mRandom, theRing, thePoint, theAngle);
                    theX = UMFloatToLong(thePoint.GetX());
                    theY = UMFloatToLong(thePoint.GetY());
                    if (CMUIsCellSet(ioMap->mValidTiles, theX, theY) && CanPlaceResource(ioMap, kRMObjectTree, theX, theY)) {
                        theFound = true;
                        theCenter.SetXY((float)theX, (float)theY);
                    }
                }
                if (theFound) {
                    theClumps->Begin(theCenter);
                    theClumps->Generate();
                    theClumps->GetTotalSize();
                    theTreeTotal += theClumps->GetTotalSize();
                    theTreeCounts[i] += theClumps->GetTotalSize();
                } else {
                    ESDebugOut("CMUCreateTrees could not locate a center for");
                    ESDebugOut(" [CMU] the tree clumps in %ld iterations for treeClump %ld\n", n, j);
                    ESDebugOut("CMUCreateTrees center(%ld,%ld) point (%ld,%ld) innerR = %6.2f outerR = %6.2f\n",
                               UMFloatToLong(theRing.mCenter.GetX()), UMFloatToLong(theRing.mCenter.GetY()),
                               theX, theY, theRing.mMinRadius, theRing.mMaxRadius);
                }
                delete theClumps;
                theIndex = theNext;
            }
        }
        ESDebugOut("CMUCreateTrees: %ld trees in time:     %6.2f s\n", theTreeTotal,
                   (float)(TMGetTime() - theStart) * 0.001f);
        for (i = 0; i < theTreeCounts.size(); i++)
            ESDebugOut("CMUCreateTrees: P%ld trees:   %ld\n", i, theTreeCounts[i]);
    }
    return 0;
}

const float kNeutralScale = 1.0f;    // loaded from memory (a named constant), 0x1912c (provisional name)

// FUNCTION: DEFAULT 0x10002412
// Returns whether a tile is on the grid and set
bool CMUIsCellSet(const std::vector<std::vector<unsigned char> >& inGrid, unsigned long inX, unsigned long inY)
{
    return inX < inGrid.size() && inY < inGrid.size() && inGrid[inX][inY];
}

// FUNCTION: DEFAULT 0x1000244e
// Adds the outer neutrals, inner neutrals and moons as map owners at spread-out points on their
// rings, then pushes the new positions to valid ground
long CMUGenerateNeutralPositions(EERandomMap* ioMap, float inCount)
{
    std::vector<GE2DPoint> theOccupied;
    std::vector<GE2DPoint> thePositions;
    EERMPlayer thePlayer;
    unsigned long i;
    for (i = 0; i < ioMap->mPlayers.size(); i++)
        theOccupied.push_back(ioMap->mPlayers[i].mPosition);
    if (ioMap->mNumOuterNeutrals != 0) {
        thePositions.resize(ioMap->mNumOuterNeutrals);
        PlacePointsOnRing(ioMap->mRandom, ioMap->mRing, thePositions);
        if (thePositions.size() > 1) {
            float theSpacing = SpreadPoints(thePositions, theOccupied, ioMap->mRing, kNeutralScale, ioMap->mConvergentSampleSize,
                                           ioMap->mConvergentThreshold, ioMap->mMaxLoopCount);
            RelaxPoints(thePositions, theOccupied, ioMap->mRing, kNeutralScale, theSpacing, ioMap->mMaxLoopCount);
        }
        for (i = 0; i < thePositions.size(); i++) {
            thePlayer.mRadius = 0.0f;
            thePlayer.mIsNeutral = true;
            thePlayer.mIsOuter = true;
            thePlayer.mIsMoon = false;
            thePlayer.mIsPlaced = false;
            thePlayer.mTileCount = 0;
            thePlayer.mIndex = i;
            thePlayer.mTeam = kNeutralTeamOuterNeutral;
            thePlayer.mPosition = thePositions[i];
            ioMap->mPlayers.push_back(thePlayer);
            theOccupied.push_back(thePositions[i]);
        }
    }
    if (ioMap->mNumInnerNeutrals != 0) {
        thePositions.resize(ioMap->mNumInnerNeutrals);
        PlacePointsOnRing(ioMap->mRandom, ioMap->mNeutralRing, thePositions);
        if (thePositions.size() > 1) {
            float theSpacing = SpreadPoints(thePositions, ioMap->mNeutralRing, kNeutralScale, ioMap->mConvergentSampleSize, ioMap->mConvergentThreshold,
                                           ioMap->mMaxLoopCount);
            RelaxPoints(thePositions, ioMap->mNeutralRing, kNeutralScale, theSpacing, ioMap->mMaxLoopCount);
        }
        for (i = 0; i < thePositions.size(); i++) {
            thePlayer.mRadius = 0.0f;
            thePlayer.mIndex = i + ioMap->mNumOuterNeutrals;
            thePlayer.mIsNeutral = true;
            thePlayer.mIsOuter = false;
            thePlayer.mIsMoon = false;
            thePlayer.mIsPlaced = false;
            thePlayer.mTileCount = 0;
            thePlayer.mTeam = kNeutralTeamInnerNeutral;
            thePlayer.mPosition = thePositions[i];
            ioMap->mPlayers.push_back(thePlayer);
        }
    }
    if (ioMap->mNumMoons != 0) {
        thePositions.resize(ioMap->mNumMoons);
        PlacePointsOnRing(ioMap->mRandom, ioMap->mMoonRing, thePositions);
        if (thePositions.size() > 1) {
            float theSpacing = SpreadPoints(thePositions, ioMap->mMoonRing, kNeutralScale, ioMap->mConvergentSampleSize, ioMap->mConvergentThreshold,
                                           ioMap->mMaxLoopCount);
            RelaxPoints(thePositions, ioMap->mMoonRing, kNeutralScale, theSpacing, ioMap->mMaxLoopCount);
        }
        for (i = 0; i < thePositions.size(); i++) {
            thePlayer.mRadius = 0.0f;
            thePlayer.mIndex = ioMap->mNumInnerNeutrals + ioMap->mNumOuterNeutrals + i;
            thePlayer.mIsNeutral = true;
            thePlayer.mIsOuter = true;
            thePlayer.mIsMoon = true;
            thePlayer.mIsPlaced = false;
            thePlayer.mTileCount = 0;
            thePlayer.mTeam = kNeutralTeamMoon;
            thePlayer.mPosition = thePositions[i];
            ioMap->mPlayers.push_back(thePlayer);
        }
    }
    CMUAdjustPositions(ioMap);
    return 0;
}

// FUNCTION: DEFAULT 0x10002843
// Adds every team player as a map owner, spreads start points on the player ring, assigns them to
// teams, pulls team-mates together and pushes the positions to valid ground
long CMUGeneratePlayerPositions(EERandomMap* ioMap)
{
    std::vector<GE2DPoint> thePositions;
    EERMPlayer thePlayer;
    unsigned long theCount = 0;
    unsigned long i, j;
    for (i = 0; i < ioMap->mTeams.size(); i++)
        theCount += ioMap->mTeams[i].mPlayers.size();
    ioMap->mNumTeamPlayers = theCount;
    for (i = 0; i < theCount; i++) {
        thePlayer.mIsNeutral = false;
        thePlayer.mRadius = 0.0f;
        thePlayer.mIsMoon = false;
        thePlayer.mIsPlaced = false;
        thePlayer.mTileCount = 0;
        thePlayer.mTeam = 0;
        thePlayer.mIsOuter = true;
        thePlayer.mIndex = i + 1;
        ioMap->mPlayers.push_back(thePlayer);
    }
    thePositions.resize(theCount);
    PlacePointsOnRing(ioMap->mRandom, ioMap->mRing, thePositions);
    float theSpacing = SpreadPoints(thePositions, ioMap->mRing, kNeutralScale, ioMap->mConvergentSampleSize, ioMap->mConvergentThreshold,
                                   ioMap->mMaxLoopCount);
    RelaxPoints(thePositions, ioMap->mRing, kNeutralScale, theSpacing, ioMap->mMaxLoopCount);
    CMUAssignTeamPositions(ioMap, ioMap->mTeams, thePositions);
    CMUSpreadTeams(ioMap->mTeams, &ioMap->mRing, ioMap->mMaxAngleBetweenTeamMembers);
    for (i = 0; i < ioMap->mTeams.size(); i++)
        for (j = 0; j < ioMap->mTeams[i].mPlayers.size(); j++)
            ioMap->mPlayers[ioMap->mTeams[i].mPlayers[j] - 1].mPosition = ioMap->mTeams[i].mPositions[j];
    CMUAdjustPositions(ioMap);
    return 0;
}

// FUNCTION: DEFAULT 0x10002a9f
// Groups the start points into neighbouring runs, one per team (trying every start point and
// keeping the most compact grouping), and gives each team a run of its size
void CMUAssignTeamPositions(EERandomMap* ioMap, std::vector<EERMTeam>& ioTeams, std::vector<GE2DPoint>& inPositions)
{
    GE2DPoint thePoint;
    std::vector<std::vector<GE2DPoint> > theTeamPositions;
    std::vector<unsigned char> theFree;
    std::vector<GE2DPoint> theRemaining;
    std::vector<GE2DPoint>::iterator theClosest = theRemaining.begin();
    float theBest = FLT_MAX;
    unsigned long theBestIndex = 0;
    unsigned long i, j, k;
    float theTotal;
    theTeamPositions.resize(ioTeams.size());
    for (i = 0; i < ioTeams.size(); i++)
        theTeamPositions[i].resize(0);
    theFree.resize(ioTeams.size());
    for (i = 0; i < ioTeams.size(); i++)
        theFree[i] = true;
    for (k = 0; k < inPositions.size(); k++) {
        thePoint.mX = inPositions[k].mX + 0.01f;
        thePoint.mY = inPositions[k].mY;
        theRemaining.resize(0);
        for (j = 0; j < inPositions.size(); j++)
            theRemaining.push_back(inPositions[j]);
        theTotal = 0.0f;
        for (i = 0; i < theTeamPositions.size(); i++) {
            for (j = 0; j < ioTeams[i].mPlayers.size(); j++) {
                theTotal += FindClosestPoint(theRemaining, thePoint, theClosest);
                thePoint = *theClosest;
                theRemaining.erase(theClosest);
            }
            if (theRemaining.size() != 0)
                theTotal -= FindClosestPoint(theRemaining, thePoint, theClosest);
        }
        if (theTotal < theBest) {
            theBest = theTotal;
            theBestIndex = k;
        }
    }
    thePoint.mX = inPositions[theBestIndex].mX + 0.01f;
    thePoint.mY = inPositions[theBestIndex].mY;
    theRemaining.resize(0);
    for (j = 0; j < inPositions.size(); j++)
        theRemaining.push_back(inPositions[j]);
    for (i = 0; i < theTeamPositions.size(); i++) {
        for (j = 0; j < ioTeams[i].mPlayers.size(); j++) {
            FindClosestPoint(theRemaining, thePoint, theClosest);
            thePoint = *theClosest;
            theTeamPositions[i].push_back(thePoint);
            theRemaining.erase(theClosest);
        }
    }
    for (i = 0; i < theTeamPositions.size(); i++) {
        for (k = 0; k < ioTeams.size(); k++)
            if (theFree[k] && ioTeams[k].mPlayers.size() == theTeamPositions[i].size())
                break;
        theFree[k] = false;
        for (j = 0; j < ioTeams[k].mPlayers.size(); j++)
            ioTeams[k].mPositions[j] = theTeamPositions[i][j];
    }
    for (i = 0; i < ioTeams.size(); i++)
        for (j = 0; j < ioTeams[i].mPlayers.size(); j++)
            ioMap->mPlayers[ioTeams[i].mPlayers[j] - 1].mTeam = i;
}

// FUNCTION: DEFAULT 0x1000307e
// Places moon resources, each resource kind, the ring-placed objects and the herds
long CMUGenerateResources(EERandomMap* ioMap)
{
    std::vector<std::vector<GE2DPoint> > thePositions;
    unsigned long i;
    thePositions.resize(ioMap->mPlayers.size());
    for (i = 0; i < ioMap->mPlayers.size(); i++)
        thePositions[i].push_back(ioMap->mPlayers[i].mPosition);
    PlacePlayerObjects(ioMap);
    for (i = 0; i < kRMObjectCount; i++)
        PlaceObjects(ioMap, thePositions, i);
    PlaceSpacedObjects(ioMap, thePositions);
    PlaceResources(ioMap, thePositions);
    return 0;
}

// FUNCTION: DEFAULT 0x10003146
// Picks one random tile per land mass, starts from the first of the farthest pair and connects the
// rest one by one, then draws the shallows; only when the script asks for shallows
long CMUGenerateShallows(EERandomMap* ioMap)
{
    unsigned long i = 0;
    unsigned long j;
    std::vector<CMUSegment> theSegments;
    std::vector<GE2DPoint> theRemaining;
    std::vector<GE2DPoint> theConnected;
    float theBest = 0.0f;
    std::vector<EERMPlayer>& thePlayers = ioMap->mPlayers;
    unsigned long thePlayerCount = thePlayers.size();
    unsigned long theBestIndex = 0;
    if (ioMap->mGenerateShallows && ioMap->mMaxShallowConnectionsPerPlayer > 0) {
        for (i = 0; i < thePlayerCount; i++) {
            if (thePlayers[i].mTiles.size() > 0) {
                long theIndex = ioMap->mRandomIntegers.Generate(0, thePlayers[i].mTiles.size() - 1);
                theRemaining.push_back(thePlayers[i].mTiles[theIndex]);
            }
        }
        unsigned long theCount = theRemaining.size();
        for (i = 0; i < theCount - 1; i++) {
            for (j = i + 1; j < theCount; j++) {
                float theDistance = theRemaining[i].DistanceToSquared(theRemaining[j]);
                if (theDistance > theBest) {
                    theBest = theDistance;
                    theBestIndex = i;
                }
            }
        }
        theConnected.push_back(theRemaining[theBestIndex]);
        theRemaining.erase(theRemaining.begin() + theBestIndex);
        while (!theRemaining.empty())
            CMUConnectNext(theRemaining, theConnected, theSegments, ioMap->mMaxShallowConnectionsPerPlayer);
        CMUDrawShallows(ioMap, theSegments);
    }
    return 0;
}

// FUNCTION: DEFAULT 0x100033b8
// Connects the next remaining point to its nearest connected point, adding up to inCount - 1 more
// of its shortest links, and moves it to the connected list
void CMUConnectNext(std::vector<GE2DPoint>& ioRemaining, std::vector<GE2DPoint>& ioConnected,
                    std::vector<CMUSegment>& ioSegments, unsigned long inCount)
{
    unsigned long i = 0;
    std::vector<CMUCandidate> theCandidates;
    CMUCandidate theCandidate;
    CMUSegment theSegment;
    float theBest = 0.0f;
    unsigned long theCount = ioConnected.size();
    unsigned long theBestIndex = 0;
    for (i = 0; i < theCount; i++) {
        float theDistance = ioRemaining[0].DistanceToSquared(ioConnected[i]);
        if (theDistance < theBest || i == 0) {
            theBest = theDistance;
            theBestIndex = i;
        }
    }
    theSegment.mStart = ioRemaining[0];
    theSegment.mEnd = ioConnected[theBestIndex];
    ioSegments.push_back(theSegment);
    if (inCount > 1) {
        theCandidate.mStart = theSegment.mStart;
        for (i = 1; i < ioRemaining.size(); i++) {
            theCandidate.mEnd = ioRemaining[i];
            theCandidate.mDistance = theCandidate.mStart.DistanceToSquared(theCandidate.mEnd);
            theCandidates.push_back(theCandidate);
        }
        for (i = 0; i < theCount; i++) {
            if (i == theBestIndex)
                continue;
            theCandidate.mEnd = ioConnected[i];
            theCandidate.mDistance = theCandidate.mStart.DistanceToSquared(theCandidate.mEnd);
            theCandidates.push_back(theCandidate);
        }
        for (i = 1; i < inCount; i++) {
            std::vector<CMUCandidate>::iterator theShortest = theCandidates.begin();
            std::vector<CMUCandidate>::iterator it;
            for (it = theCandidates.begin(); it != theCandidates.end(); it++)
                if (it->mDistance < theShortest->mDistance)
                    theShortest = it;
            if (theShortest != theCandidates.end()) {
                ioSegments.push_back(*theShortest);
                theCandidates.erase(theShortest);
            }
        }
    }
    ioConnected.push_back(ioRemaining[0]);
    ioRemaining.erase(ioRemaining.begin());
}

// FUNCTION: DEFAULT 0x10003633
// Raises the heights along each shallows segment, at the shallows width, up to the shallows elevation
void CMUDrawShallows(EERandomMap* ioMap, std::vector<CMUSegment>& inSegments)
{
    GE2DPoint theLeft;
    GE2DPoint theRight;
    GEVector theDirection;
    GEVector theNormal;
    GETransformation theRotation;
    std::vector<GE2DPoint> theLine;
    std::vector<GE2DPoint> theWidth;
    float theHalfWidth = ioMap->mShallowsWidth * 0.5f;
    unsigned long theSize = ioMap->mHeights.size();
    unsigned long i, j, k, x, y;
    for (i = 0; i < inSegments.size(); i++) {
        theRotation.SetOrientationYPR(1.5707964f, 0.0f, 0.0f);
        theDirection.SetX(inSegments[i].mEnd.GetX() - inSegments[i].mStart.GetX());
        theDirection.SetY(inSegments[i].mEnd.GetY() - inSegments[i].mStart.GetY());
        theRotation.Apply(theDirection, theNormal);
        theNormal.Normalize();
        theNormal.SetX(theNormal.GetX() * theHalfWidth);
        theNormal.SetY(theNormal.GetY() * theHalfWidth);
        inSegments[i].mStart.InterpolateTo(inSegments[i].mEnd, 1.0f, theLine);
        for (j = 0; j < theLine.size(); j++) {
            theLeft.SetX(theLine[j].GetX() + theNormal.GetX());
            theLeft.SetY(theLine[j].GetY() + theNormal.GetY());
            theRight.SetX(theLine[j].GetX() - theNormal.GetX());
            theRight.SetY(theLine[j].GetY() - theNormal.GetY());
            theLeft.InterpolateTo(theRight, 1.0f, theWidth);
            for (k = 0; k < theWidth.size(); k++) {
                unsigned long theX = UMFloatToLong(theWidth[k].GetX() - 1.0f);
                unsigned long theY = UMFloatToLong(theWidth[k].GetY() - 1.0f);
                for (x = theX; x < theX + 3; x++)
                    for (y = theY; y < theY + 3; y++)
                        if (x < theSize && y < theSize && ioMap->mHeights[x][y] < ioMap->mShallowsElevation)
                            ioMap->mHeights[x][y] = ioMap->mShallowsElevation;
            }
        }
    }
}

// FUNCTION: DEFAULT 0x10003aa3
// Pulls each team's members together around the ring centre so neighbouring members are at most the
// maximum angle apart, keeping their distances from the centre
void CMUSpreadTeams(std::vector<EERMTeam>& ioTeams, EERMRing* inRing, float inSpread)
{
    GE2DPoint thePoint;
    GETransformation theRotation;
    GEVector theOffset;
    std::vector<CMUTeamAngle> theAngles;
    GEVector theRotated;
    float theBaseAngle = 0.0f;
    unsigned long theTeamCount = ioTeams.size();
    unsigned long t, i, j, theLow, theHigh;
    for (t = 0; t < theTeamCount; t++) {
        unsigned long theCount = ioTeams[t].mPlayers.size();
        if (theCount <= 1)
            continue;
        theAngles.clear();
        theAngles.resize(theCount);
        for (i = 0; i < theCount; i++) {
            theAngles[i].mIndex = i;
            thePoint = ioTeams[t].mPositions[i];
            theOffset.SetX(thePoint.GetX() - inRing->mCenter.GetX());
            theOffset.SetY(thePoint.GetY() - inRing->mCenter.GetY());
            theAngles[i].mDistance = theOffset.GetMagnitude();
            theAngles[i].mAngle = UMArcTan2(-theOffset.GetX(), theOffset.GetY());
            if (i == 0) {
                theBaseAngle = theAngles[0].mAngle;
                theAngles[0].mAngle = 0.0f;
            } else {
                float theDelta = CMUWrapAngle(theAngles[i].mAngle - theBaseAngle) / (i + 1);
                theBaseAngle = CMUWrapAngle(theDelta + theBaseAngle);
                theAngles[i].mAngle = CMUWrapAngle(theAngles[i].mAngle - theBaseAngle);
                for (j = 0; j < i; j++)
                    theAngles[j].mAngle = CMUWrapAngle(theAngles[j].mAngle - theDelta);
            }
        }
        for (i = 0; i < theCount - 1; i++)
            for (j = i + 1; j < theCount; j++)
                if (theAngles[j].mAngle < theAngles[i].mAngle) {
                    CMUTeamAngle theSwap = theAngles[i];
                    theAngles[i] = theAngles[j];
                    theAngles[j] = theSwap;
                }
        for (theLow = (theCount >> 1) - 1, theHigh = (theCount >> 1) + 1; theLow < theCount || theHigh < theCount;
             theLow--, theHigh++) {
            if (theLow < theCount && theAngles[theLow + 1].mAngle - theAngles[theLow].mAngle > inSpread)
                theAngles[theLow].mAngle = theAngles[theLow + 1].mAngle - inSpread;
            if (theHigh < theCount && theAngles[theHigh].mAngle - theAngles[theHigh - 1].mAngle > inSpread)
                theAngles[theHigh].mAngle = inSpread + theAngles[theHigh - 1].mAngle;
        }
        for (i = 0; i < theCount; i++) {
            theRotation.SetOrientationYPR(theBaseAngle + theAngles[i].mAngle, 0.0f, 0.0f);
            theOffset.SetX(0.0f);
            theOffset.SetY(theAngles[i].mDistance);
            theOffset.SetZ(0.0f);
            theRotation.Apply(theOffset, theRotated);
            thePoint.SetX(inRing->mCenter.GetX() + theRotated.GetX());
            thePoint.SetY(inRing->mCenter.GetY() + theRotated.GetY());
            ioTeams[t].SetPosition(theAngles[i].mIndex, thePoint);
        }
    }
}

// FUNCTION: DEFAULT 0x100041ca
// Moves every unplaced owner from its point along the line from the map centre (one tile per try)
// until a valid spot with a usable square around it, then marks it placed
void CMUAdjustPositions(EERandomMap* ioMap)
{
    GEVector theDirection;
    unsigned long theBorder = ioMap->mMinimumStartPositionToMapEdge + ioMap->mWaterBorder;
    if (ioMap->mMapCharacteristics & kRMMapEarthOrMarsMap)
        theBorder -= ioMap->mWaterBorder;
    GE2DPoint theCenter(ioMap->mMaximumX * 0.5f, ioMap->mMaximumY * 0.5f);
    unsigned long theSize;
    if (UMAbs(ioMap->mFlatArea.GetWidth()) > UMAbs(ioMap->mFlatArea.GetHeight()))
        theSize = UMFloatToLong(UMAbs(ioMap->mFlatArea.GetWidth()));
    else
        theSize = UMFloatToLong(UMAbs(ioMap->mFlatArea.GetHeight()));
    unsigned long i, j;
    for (i = 0; i < ioMap->mPlayers.size(); i++) {
        if (ioMap->mPlayers[i].mIsPlaced)
            continue;
        GE2DPoint thePosition = ioMap->mPlayers[i].mPosition;
        theDirection.SetX(thePosition.GetX() - theCenter.GetX());
        theDirection.SetY(thePosition.GetY() - theCenter.GetY());
        theDirection.Normalize();
        float theStep;
        if (ioMap->mPlayers[i].mIsMoon)
            theStep = 1.0f;
        else if (ioMap->mPlayers[i].mIsOuter)
            theStep = ioMap->mOuterPushFromMapCenter;
        else
            theStep = ioMap->mInnerPushFromMapCenter;
        unsigned long theRadius;
        if (!ioMap->mPlayers[i].mIsNeutral)
            theRadius = theSize + theBorder;
        else
            theRadius = ioMap->mPlayers[i].mIsMoon ? 0 : theBorder;
        bool theSearching = true;
        for (j = 0; j < ioMap->mMaxLoopCount && theSearching; j++) {
            GEVector theOffset;
            theDirection.Scale(theStep, theOffset);
            unsigned long theX = UMFloatToLong(theOffset.GetX() + thePosition.GetX());
            unsigned long theY = UMFloatToLong(thePosition.GetY() + theOffset.GetY());
            if (theX < ioMap->mGridSize && theY < ioMap->mGridSize && ioMap->mValidTiles[theX][theY] &&
                !IsSquareOpen(ioMap->mValidTiles, theX, theY, theRadius)) {
                theSearching = false;
                ioMap->mPlayers[i].mPosition = GE2DPoint((float)theX, (float)theY);
            } else {
                theSearching = true;
                theStep -= 1.0f;
            }
        }
        ioMap->mPlayers[i].mIsPlaced = true;
    }
}

// FUNCTION: DEFAULT 0x10004197
// Wraps an angle into -pi..pi
float CMUWrapAngle(float inAngle)
{
    while (inAngle > 3.14159265f)
        inAngle -= 6.28318531f;
    while (inAngle < -3.14159265f)
        inAngle += 6.28318531f;
    return inAngle;
}
