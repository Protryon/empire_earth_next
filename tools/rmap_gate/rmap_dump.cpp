// rmap_dump: the random map generator's byte gate (RUST_PLAN R3.4; tools/rmap_gate/rmap_gate.py builds and drives it).
//
// For every map script of a data tree x sizes x player counts x teams x seeds: fills an EERandomMap the way the game
// does (EERandomMap::SetUp and Generate, src/Empire Earth/Random Map/EERandomMap.cpp, without the EEMap), runs the
// "rmap" PIM entry points in the game's order, and writes one line per map:
//   <script>|<map size>|<players>|<ffa or teams>|<seed> <64-bit digest of every EERandomMap output>
// With --stages: the digest after every PIM step, to find a divergence. Floats go into the digest as bit patterns,
// never as text.
//
// The generator is a seam: linked with the C++ generator's units before the port and with the Rust generator (through
// Default.cpp, the C++ adapter) after it.
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/U2DGeometry.h"
#include "lle/FSFileSpec.h"
#include "ee/EERandomMap.h"
#include "ee/EERandomMapIDs.h"

extern "C" {
long EERMPIMCreateFlatMap(EERandomMap* ioMap);
long EERMPIMCreateHeightMap(EERandomMap* ioMap);
long EERMPIMCreateNeutralLandWaterMap(EERandomMap* ioMap);
long EERMPIMCreatePlayerLandWaterMap(EERandomMap* ioMap);
long EERMPIMCreateTrees(EERandomMap* ioMap);
long EERMPIMFilterHeightMap(EERandomMap* ioMap);
long EERMPIMGenerateNeutralPositions(EERandomMap* ioMap);
long EERMPIMGeneratePlayerPositions(EERandomMap* ioMap);
long EERMPIMGenerateResources(EERandomMap* ioMap);
long EERMPIMGenerateShallows(EERandomMap* ioMap);
long EERMPIMSetRandomMapValues(EERandomMap* ioMap, const UString& inScriptName);
}

// ---- SHA-256 (the digest of the dump; no dependency, the tool must stay small)
struct Sha256 {
    uint32_t h[8];
    uint8_t buf[64];
    size_t fill;
    uint64_t total;
    Sha256() { Reset(); }
    void Reset()
    {
        static const uint32_t kInit[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                          0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
        memcpy(h, kInit, sizeof h);
        fill = 0;
        total = 0;
    }
    static uint32_t rr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
    void Block(const uint8_t* p)
    {
        static const uint32_t k[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
        uint32_t w[64];
        for (int i = 0; i < 16; i++)
            w[i] = (uint32_t)p[i * 4] << 24 | (uint32_t)p[i * 4 + 1] << 16 | (uint32_t)p[i * 4 + 2] << 8 | p[i * 4 + 3];
        for (int i = 16; i < 64; i++) {
            uint32_t s0 = rr(w[i - 15], 7) ^ rr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = rr(w[i - 2], 17) ^ rr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; i++) {
            uint32_t t1 = hh + (rr(e, 6) ^ rr(e, 11) ^ rr(e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
            uint32_t t2 = (rr(a, 2) ^ rr(a, 13) ^ rr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
            hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    void Update(const void* inData, size_t inSize)
    {
        const uint8_t* p = (const uint8_t*)inData;
        total += inSize;
        while (inSize) {
            size_t n = 64 - fill < inSize ? 64 - fill : inSize;
            memcpy(buf + fill, p, n);
            fill += n; p += n; inSize -= n;
            if (fill == 64) {
                Block(buf);
                fill = 0;
            }
        }
    }
    void Final(uint8_t out[32])
    {
        uint64_t bits = total * 8;
        uint8_t pad = 0x80;
        Update(&pad, 1);
        pad = 0;
        while (fill != 56)
            Update(&pad, 1);
        uint8_t len[8];
        for (int i = 0; i < 8; i++)
            len[i] = (uint8_t)(bits >> (56 - 8 * i));
        Update(len, 8);
        for (int i = 0; i < 32; i++)
            out[i] = (uint8_t)(h[i / 4] >> (24 - 8 * (i % 4)));
    }
};

struct Digest {
    Sha256 mSha;
    void U32(uint32_t v) { mSha.Update(&v, 4); }
    void I32(long v) { U32((uint32_t)v); }
    void F32(float v) { U32(*(uint32_t*)&v); }
    void Bool(bool v) { uint8_t b = v ? 1 : 0; mSha.Update(&b, 1); }
    void Point(const GE2DPoint& p) { F32(p.mX); F32(p.mY); }
    void String(const UString& s)
    {
        const char* c = s;
        size_t n = strlen(c);
        U32((uint32_t)n);
        mSha.Update(c, n);
    }
};

static void DumpMap(EERandomMap& m, Digest& d)
{
    size_t i, j;
    d.U32(m.mRandom.mSeed);
    d.U32(m.mRandomIntegers.mSeed);
    for (i = 0; i < 7; i++) {
        d.U32(m.mKindSpacing[i][0]);
        d.U32(m.mKindSpacing[i][1]);
    }
    d.U32((uint32_t)m.mFlatTiles.size());
    for (i = 0; i < m.mFlatTiles.size(); i++) {
        d.U32((uint32_t)m.mFlatTiles[i].size());
        d.mSha.Update(m.mFlatTiles[i].data(), m.mFlatTiles[i].size());
    }
    d.U32((uint32_t)m.mHeights.size());
    for (i = 0; i < m.mHeights.size(); i++) {
        d.U32((uint32_t)m.mHeights[i].size());
        d.mSha.Update(m.mHeights[i].data(), m.mHeights[i].size() * 4);
    }
    d.U32((uint32_t)m.mTiles.size());
    for (i = 0; i < m.mTiles.size(); i++) {
        d.U32((uint32_t)m.mTiles[i].size());
        d.mSha.Update(m.mTiles[i].data(), m.mTiles[i].size() * 4);
    }
    d.U32((uint32_t)m.mClaimedTiles.size());
    for (i = 0; i < m.mClaimedTiles.size(); i++) {
        d.U32((uint32_t)m.mClaimedTiles[i].size());
        d.mSha.Update(m.mClaimedTiles[i].data(), m.mClaimedTiles[i].size());
    }
    d.U32((uint32_t)m.mPlayers.size());
    for (i = 0; i < m.mPlayers.size(); i++) {
        EERMPlayer& p = m.mPlayers[i];
        d.U32((uint32_t)p.mTiles.size());
        for (j = 0; j < p.mTiles.size(); j++)
            d.Point(p.mTiles[j]);
        d.U32((uint32_t)p.mClumpCenters.size());
        for (j = 0; j < p.mClumpCenters.size(); j++)
            d.Point(p.mClumpCenters[j]);
        d.Point(p.mPosition);
        d.F32(p.mRadius);
        d.U32(p.mIndex);
        d.I32(p.mTeam);
        d.I32(p.mTileCount);
        d.Bool(p.mIsNeutral);
        d.Bool(p.mIsOuter);
        d.Bool(p.mIsMoon);
        d.Bool(p.mIsPlaced);
    }
    d.U32((uint32_t)m.mValidTiles.size());
    for (i = 0; i < m.mValidTiles.size(); i++) {
        d.U32((uint32_t)m.mValidTiles[i].size());
        d.mSha.Update(m.mValidTiles[i].data(), m.mValidTiles[i].size());
    }
    d.U32((uint32_t)m.mUnitNames.size());
    for (i = 0; i < m.mUnitNames.size(); i++) {
        d.String(m.mUnitNames[i].mName);
        d.I32(m.mUnitNames[i].mUnitClass);
        d.U32(m.mUnitNames[i].mSize);
    }
    d.U32((uint32_t)m.mTeams.size());
    for (i = 0; i < m.mTeams.size(); i++) {
        d.U32((uint32_t)m.mTeams[i].mPlayers.size());
        for (j = 0; j < m.mTeams[i].mPlayers.size(); j++) {
            d.I32(m.mTeams[i].mPlayers[j]);
            d.Point(m.mTeams[i].mPositions[j]);
        }
    }
    d.U32((uint32_t)m.mObjects.size());
    for (i = 0; i < m.mObjects.size(); i++) {
        d.Point(m.mObjects[i].mLocation);
        d.I32(m.mObjects[i].mKind);
        // mType is set for trees only (the C++ leaves it uninitialized on every other kind, and only reads it for trees)
        if (m.mObjects[i].mKind == kRMObjectTree)
            d.I32(m.mObjects[i].mType);
    }
    d.U32((uint32_t)m.mWaterPoints.size());
    for (i = 0; i < m.mWaterPoints.size(); i++)
        d.Point(m.mWaterPoints[i]);
    d.U32((uint32_t)m.mCliffNames.size());
    for (i = 0; i < m.mCliffNames.size(); i++) {
        d.String(m.mCliffNames[i].mName);
        d.I32(m.mCliffNames[i].mValue);
    }
    d.U32((uint32_t)m.mTerrainNames.size());
    for (i = 0; i < m.mTerrainNames.size(); i++) {
        d.String(m.mTerrainNames[i].mName);
        d.I32(m.mTerrainNames[i].mValue);
    }
    d.U32((uint32_t)m.mFeatures.size());
    for (i = 0; i < m.mFeatures.size(); i++) {
        EERMFeature& f = m.mFeatures[i];
        d.F32(f.mAngle);
        d.F32(f.mPosition.mX);
        d.F32(f.mPosition.mY);
        d.I32(f.mUnitType);
        d.I32(f.mIndex);
        d.Bool(f.mIsWorldUnit);
        d.Bool(f.mInRing);
        d.Bool(f.mIsHerd);
    }
    d.U32((uint32_t)m.mWalls.size());
    for (i = 0; i < m.mWalls.size(); i++) {
        d.I32(m.mWalls[i].mStartX);
        d.I32(m.mWalls[i].mStartY);
        d.I32(m.mWalls[i].mEndX);
        d.I32(m.mWalls[i].mEndY);
        d.I32(m.mWalls[i].mUnitType);
    }
    d.U32((uint32_t)m.mTerrains.size());
    for (i = 0; i < m.mTerrains.size(); i++) {
        d.I32(m.mTerrains[i].mId);
        for (j = 0; j < 31; j++)
            d.I32(m.mTerrains[i].mTextures[j]);
    }
    d.U32((uint32_t)m.mTerrainRegions.size());
    for (i = 0; i < m.mTerrainRegions.size(); i++) {
        d.I32(m.mTerrainRegions[i].mFrom);
        d.I32(m.mTerrainRegions[i].mTo);
        d.U32(m.mTerrainRegions[i].mTerrain);
    }
    d.U32((uint32_t)m.mClumpCircles.size());
    for (i = 0; i < m.mClumpCircles.size(); i++) {
        d.U32((uint32_t)m.mClumpCircles[i].size());
        for (j = 0; j < m.mClumpCircles[i].size(); j++) {
            d.Point(m.mClumpCircles[i][j].mCenter);
            d.F32(m.mClumpCircles[i][j].mRadius);
        }
    }
    d.U32((uint32_t)m.mAvailableAnimals.size());
    for (i = 0; i < m.mAvailableAnimals.size(); i++)
        d.U32(m.mAvailableAnimals[i]);
    d.U32((uint32_t)m.mForests.size());
    for (i = 0; i < m.mForests.size(); i++) {
        d.I32(m.mForests[i].mId);
        d.I32(m.mForests[i].mTerrainTexture);
        d.U32((uint32_t)m.mForests[i].mTreeTypes.size());
        for (j = 0; j < m.mForests[i].mTreeTypes.size(); j++)
            d.U32(m.mForests[i].mTreeTypes[j]);
    }
    d.U32((uint32_t)m.mTreeTypes.size());
    for (i = 0; i < m.mTreeTypes.size(); i++)
        d.I32(m.mTreeTypes[i]);
    d.U32((uint32_t)m.mResourceKinds.size());
    for (i = 0; i < m.mResourceKinds.size(); i++) {
        d.I32(m.mResourceKinds[i].mMinimum);
        d.I32(m.mResourceKinds[i].mMaximum);
        d.I32(m.mResourceKinds[i].mId);
        d.I32(m.mResourceKinds[i].mUnitType);
        d.U32(m.mResourceKinds[i].mArea);
    }
    d.U32((uint32_t)m.mResourceKindIds.size());
    for (i = 0; i < m.mResourceKindIds.size(); i++)
        d.I32(m.mResourceKindIds[i]);
    d.U32((uint32_t)m.mResourceSlots.size());
    for (i = 0; i < m.mResourceSlots.size(); i++) {
        d.I32(m.mResourceSlots[i].mId);
        d.Bool(m.mResourceSlots[i].mPerPlayer);
        d.Bool(m.mResourceSlots[i].mRandomKind);
    }
    d.U32((uint32_t)m.mSpacingRules.size());
    for (i = 0; i < m.mSpacingRules.size(); i++) {
        d.I32(m.mSpacingRules[i].mKind);
        d.I32(m.mSpacingRules[i].mMinimum);
        d.I32(m.mSpacingRules[i].mMaximum);
    }
    d.F32(m.mFlatArea.mLeft); d.F32(m.mFlatArea.mBottom); d.F32(m.mFlatArea.mRight); d.F32(m.mFlatArea.mTop);
    const EERMRing* rings[3] = {&m.mRing, &m.mNeutralRing, &m.mMoonRing};
    for (i = 0; i < 3; i++) {
        d.Point(rings[i]->mCenter);
        d.F32(rings[i]->mMinRadius);
        d.F32(rings[i]->mMaxRadius);
    }
    // the scalar settings and counters, in member order (every float by bits)
    const long* longs[] = {&m.mMapSize, &m.mStartEpoch, &m.mPlayerCount, &m.mMaxIntElevation, &m.mMaxIterationsClumpGrowth,
                           &m.mMaximumTreesPerClump, &m.mMinIntElevation, &m.mWaterBorder, &m.mShallowsWidth,
                           &m.mMaxShallowConnectionsPerPlayer, &m.mOuterPushFromMapCenter, &m.mInnerPushFromMapCenter,
                           &m.mMinimumStartPositionToMapEdge, &m.mCliffTexture};
    for (i = 0; i < sizeof longs / sizeof longs[0]; i++)
        d.I32(*longs[i]);
    const float* floats[] = {&m.mCoastalToDeeperWaterPercentage, &m.mConvergentSampleSize, &m.mConvergentThreshold,
                             &m.mElevationScale, &m.mElevationUnit, &m.mFlatElevation, &m.mForestChaosLevel,
                             &m.mForestFreeRadius, &m.mInitialElevation, &m.mInnerNeutralFlatChaos,
                             &m.mInnerNeutralLandChaos, &m.mInnerNeutralPercentFlat, &m.mInnerNeutralPercentLand,
                             &m.mInnerNeutralTreePercentage, &m.mLandElevationThreshold, &m.mMaxAngleBetweenTeamMembers,
                             &m.mMaxResourceElevation, &m.mMaxTreeElevation, &m.mNeutralInnerRadius, &m.mNeutralOuterRadius,
                             &m.mOuterNeutralFlatChaos, &m.mOuterNeutralLandChaos, &m.mOuterNeutralPercentFlat,
                             &m.mOuterNeutralPercentLand, &m.mOuterNeutralTreePercentage, &m.mPercentLand,
                             &m.mPlayerFlatChaos, &m.mPlayerInnerRadius, &m.mPlayerLandChaos, &m.mPlayerOuterRadius,
                             &m.mPlayerPercentFlat, &m.mPlayerTreePercentage, &m.mTerrainChaos, &m.mShallowsElevation,
                             &m.mHeightMapChaos, &m.mHeightMapSmoothness, &m.mLandfillPercentage, &m.mResourceSeperation,
                             &m.mHalfMapSize};
    for (i = 0; i < sizeof floats / sizeof floats[0]; i++)
        d.F32(*floats[i]);
    for (i = 0; i < 7; i++) {
        d.I32(m.mResourceTerrainTexture[i]);
        d.I32(m.mResourcePerPlayer[i]);
        d.U32(m.mResourcePerNeutralLand[i]);
    }
    const unsigned long* ulongs[] = {&m.mForestsPerPlayer, &m.mInnerNeutralFlatClumps, &m.mInnerNeutralLandClumps, &m.mMaximumY,
                                     &m.mGridSize, &m.mMaximumX, &m.mMaxClumpsPerForest, &m.mMaxLoopCount,
                                     &m.mMaxTreeLoopCount, &m.mNumInnerNeutrals, &m.mNumOuterNeutrals, &m.mNumTeamPlayers,
                                     &m.mOuterNeutralFlatClumps, &m.mOuterNeutralLandClumps, &m.mPlayerFlatClumps,
                                     &m.mPlayerLandClumps, &m.mSeed, &m.mRiverWidth, &m.mTileCount, &m.mResourceToEdgeDistance,
                                     &m.mResourceToWaterDistance, &m.mMapCharacteristics, &m.mNumMoons, &m.mMoonSize,
                                     &m.mMoonResources, &m.mNumTerrainBands, &m.mTerrainBandOffset};
    for (i = 0; i < sizeof ulongs / sizeof ulongs[0]; i++)
        d.U32(*ulongs[i]);
    d.Bool(m.mTeamGrowth);
    d.Bool(m.mFilterHeightMap);
    d.Bool(m.mTestingLandDistribution);
    d.Bool(m.mGenerateShallows);
    d.Bool(m.mIsSpaceMap);
}

static std::string HexDigest(EERandomMap& m)
{
    Digest d;
    DumpMap(m, d);
    uint8_t out[32];
    d.mSha.Final(out);
    char text[33];
    for (int i = 0; i < 8; i++)   // 64 bits are plenty for a gate
        sprintf(text + i * 2, "%02x", out[i]);
    return std::string(text, 16);
}

// ---- the map shape (RTSMap::Create, GETerrainMesh::CreateFlatMesh)
struct Shape {
    long mSize;
    long mHeight;
    std::vector<std::pair<long, long> > mPointRows;    // start, count
    std::vector<std::pair<long, long> > mTileRows;     // start, end inclusive (a tile exists there)

    void Create(long inWidth, long inHeight)
    {
        mHeight = inHeight;
        mSize = inWidth + inHeight;
        mTileRows.clear();
        {
            long theLeft = inHeight - 1, theRight = inHeight;
            bool theLeftShrinking = true, theRightGrowing = true;
            for (long y = 0; y < mSize; y++) {
                mTileRows.push_back(std::make_pair(theLeft, theRight));
                if (theLeftShrinking) {
                    if (theLeft != 0)
                        theLeft--;
                    else
                        theLeftShrinking = false;
                } else
                    theLeft++;
                if (theRightGrowing) {
                    theRight++;
                    if (theRight == mSize) {
                        theRight = mSize - 1;
                        theRightGrowing = false;
                    }
                } else
                    theRight--;
            }
        }
        mPointRows.clear();
        unsigned long theStart = inHeight - 1, theEnd = inHeight + 1, theBottomCount = 0, theTopCount = 0;
        bool theStartFalling = true, theEndRising = true;
        long theCount = inWidth + inHeight + 1;
        for (long j = 0; j < theCount; j++) {
            mPointRows.push_back(std::make_pair((long)theStart, (long)(theEnd - theStart + 1)));
            if (theStartFalling) {
                if (theStart != 0)
                    theStart--;
                else {
                    theBottomCount++;
                    if (theBottomCount == 2)
                        theStartFalling = false;
                }
            } else
                theStart++;
            if (theEndRising) {
                if (theEnd != (unsigned long)theCount - 1)
                    theEnd++;
                else {
                    theTopCount++;
                    if (theTopCount == 2)
                        theEndRising = false;
                }
            } else
                theEnd--;
        }
    }
    bool IsValidPoint(unsigned long inX, unsigned long inY) const
    {
        return inY < mPointRows.size() && inX - (unsigned long)mPointRows[inY].first < (unsigned long)mPointRows[inY].second;
    }
    bool HasTile(unsigned long inX, unsigned long inY) const
    {
        return inX < (unsigned long)mSize && inY < (unsigned long)mSize &&
               (long)inX >= mTileRows[inY].first && (long)inX < mTileRows[inY].second + 1;
    }
};

// ---- the game's tables the scripts name (EERandomMap::InitializeValues)
struct NamedTables {
    std::vector<EERMNamedEntry> mUnits;
    std::vector<EERMNamedValue> mTerrains;
    std::vector<EERMNamedValue> mCliffs;
};

static std::string Join(const std::string& inName)
{
    std::string theOut;
    for (size_t i = 0; i < inName.size(); i++)
        if (inName[i] != ' ')
            theOut += inName[i];
    return theOut;
}

static std::vector<std::string> SplitCsv(const std::string& inLine)
{
    std::vector<std::string> theFields;
    std::string theField;
    bool theQuoted = false;
    for (size_t i = 0; i < inLine.size(); i++) {
        char c = inLine[i];
        if (c == '"')
            theQuoted = !theQuoted;
        else if (c == ',' && !theQuoted) {
            theFields.push_back(theField);
            theField.clear();
        } else if (c != '\r')
            theField += c;
    }
    theFields.push_back(theField);
    return theFields;
}

static bool ReadCsvNames(const std::string& inPath, const char* inColumn, std::vector<EERMNamedValue>& outTable)
{
    std::ifstream theFile(inPath);
    if (!theFile)
        return false;
    std::string theLine;
    int theNameColumn = -1, theRowColumn = -1;
    while (std::getline(theFile, theLine)) {
        if (theLine.empty() || theLine[0] == '#')
            continue;
        std::vector<std::string> theFields = SplitCsv(theLine);
        if (theNameColumn < 0) {
            for (size_t i = 0; i < theFields.size(); i++) {
                if (theFields[i] == inColumn)
                    theNameColumn = (int)i;
                if (theFields[i] == "row")
                    theRowColumn = (int)i;
            }
            if (theNameColumn < 0)
                return false;
            continue;
        }
        if ((int)theFields.size() <= theNameColumn || (int)theFields.size() <= theRowColumn)
            continue;
        EERMNamedValue theValue;
        theValue.mName = Join(theFields[theNameColumn]).c_str();
        theValue.mValue = atol(theFields[theRowColumn].c_str());
        outTable.push_back(theValue);
    }
    return true;
}

// dbobjects.yaml's rows: name, enabled, tile size (the game's unit classes, in row order)
static bool ReadUnits(const std::string& inPath, std::vector<EERMNamedEntry>& outTable)
{
    std::ifstream theFile(inPath);
    if (!theFile)
        return false;
    std::string theLine;
    long theRow = -1;
    std::string theName;
    long theEnabled = 0, theTileSize = 1;
    auto theFlush = [&]() {
        if (theRow >= 0 && theEnabled && !theName.empty()) {
            EERMNamedEntry theEntry;
            theEntry.mName = Join(theName).c_str();
            theEntry.mUnitClass = theRow;
            theEntry.mSize = theTileSize <= 1 ? 1 : theTileSize;
            outTable.push_back(theEntry);
        }
    };
    while (std::getline(theFile, theLine)) {
        size_t thePos;
        if ((thePos = theLine.find("- row: ")) != std::string::npos) {
            theFlush();
            theRow = atol(theLine.c_str() + thePos + 7);
            theName.clear();
            theEnabled = 0;
            theTileSize = 1;
        } else if ((thePos = theLine.find("mName: ")) != std::string::npos && theName.empty()) {
            size_t theOpen = theLine.find('"', thePos);
            size_t theClose = theLine.rfind('"');
            if (theOpen != std::string::npos && theClose > theOpen)
                theName = theLine.substr(theOpen + 1, theClose - theOpen - 1);
        } else if ((thePos = theLine.find("mIsEnabled: ")) != std::string::npos)
            theEnabled = atol(theLine.c_str() + thePos + 12);
        else if ((thePos = theLine.find("mTileSize: ")) != std::string::npos)
            theTileSize = atol(theLine.c_str() + thePos + 11);
    }
    theFlush();
    return !outTable.empty();
}

// ---- a run's configuration
struct RunKey {
    std::string mScript;
    int mSizeIndex;                        // index into kMapSizes
    long mPlayers;
    int mMode;                             // 0 every player alone, 1 teams of two, 2 alone with the filter pass and the
                                           // shallows forced on (no shipped script enables them: the gate must run that code)
    unsigned long mSeed;
};

// the game's map sizes (gameoptions.yaml: tiles, script size), every script's every size
static const long kMapSizes[][2] = {{42, 0}, {58, 1}, {75, 2}, {105, 3}, {160, 4}, {220, 5}};
static const long kPlayers[][8] = {{2, 3, 0}, {2, 4, 0}, {2, 4, 6, 0}, {2, 4, 8, 0}, {2, 4, 8, 0}, {2, 4, 8, 0}};
static const char* const kModeNames[3] = {"ffa", "teams", "forced"};
static const unsigned long kSeeds[] = {1, 2, 3, 1000, 12345, 2147483647};

static float gScale;
static NamedTables gTables;
static std::string gScriptDirectoryText;

// EERandomMap::Fn284149 (water points; the space-map fill)
static void WaterPoints(EERandomMap& m, const Shape& inShape)
{
    unsigned long theSize = m.mGridSize, x, y;
    if (m.mIsSpaceMap) {
        if ((m.mMapCharacteristics & kRMMapEarthOrMarsMap) != 0) {
            float theRadius = (m.mHalfMapSize - m.mWaterBorder) * 0.75f;
            for (x = 0; x < theSize; x++)
                for (y = 0; y < theSize; y++)
                    if (m.mValidTiles[x][y] && m.mRing.mCenter.DistanceTo(GE2DPoint((float)x, (float)y)) <= theRadius &&
                        (m.mTiles[x][y] & 0x7fff) == 0)
                        m.mTiles[x][y] = 0x400;
        } else {
            for (x = 0; x < theSize; x++)
                for (y = 0; y < theSize; y++) {
                    if (!m.mValidTiles[x][y] || (m.mTiles[x][y] & 0x7fff) != 0)
                        continue;
                    unsigned long theValue = 0, theCount = 0;
                    for (unsigned long i = x - 1; i <= x + 1; i++)
                        for (unsigned long j = y - 1; j <= y + 1; j++)
                            if ((m.mTiles[i][j] & 0x7fff) != 0) {
                                theValue = m.mTiles[i][j];
                                theCount++;
                            }
                    if (theCount >= 5)
                        m.mTiles[x][y] = theValue;
                }
        }
    }
    m.mWaterPoints.clear();
    for (x = 0; x < theSize; x++)
        for (y = 0; y < theSize; y++) {
            if (!inShape.HasTile(x, y) || !(x < m.mValidTiles.size() && y < m.mValidTiles.size() && m.mValidTiles[x][y]) ||
                (m.mTiles[x][y] & 0x7fff) != 0)
                continue;
            GE2DPoint thePoint;
            thePoint.SetX((float)x);
            thePoint.SetY((float)y);
            m.mWaterPoints.push_back(thePoint);
        }
}

// EERandomMap::AddNeighbourHeight
static void AddNeighbour(EERandomMap& m, unsigned long inX, unsigned long inY, float& ioSum, long& ioCount)
{
    if (inX < m.mGridSize && inY < m.mGridSize && m.mValidTiles[inX][inY]) {
        ioSum += m.mHeights[inX][inY];
        ioCount++;
    }
}

// EERandomMap::SmoothHeights (the game-side pass between the height map and the resources)
static void SmoothHeights(EERandomMap& m, const Shape& inShape)
{
    float theRange = (float)(m.mMaxIntElevation - m.mMinIntElevation);
    float theScale = gScale;
    float theMinimum = (float)m.mMinIntElevation;
    float theSeaLevel = -(theMinimum / theRange);
    long x, y;
    for (x = 0; x < (long)m.mGridSize; x++) {
        for (y = 0; y < (long)m.mGridSize; y++) {
            if (!m.mValidTiles[x][y])
                continue;
            float theHeight = m.mHeights[x][y];
            float theSum = 0.0f;
            long theCount = 0;
            AddNeighbour(m, x - 1, y - 1, theSum, theCount);
            AddNeighbour(m, x - 1, y, theSum, theCount);
            AddNeighbour(m, x - 1, y + 1, theSum, theCount);
            AddNeighbour(m, x, y - 1, theSum, theCount);
            AddNeighbour(m, x, y + 1, theSum, theCount);
            AddNeighbour(m, x + 1, y - 1, theSum, theCount);
            AddNeighbour(m, x + 1, y, theSum, theCount);
            AddNeighbour(m, x + 1, y + 1, theSum, theCount);
            theSum = theSum / theCount;
            theHeight = (theHeight * theRange + theMinimum) * theScale;
            if (UMAbs(theHeight - (theSum * theRange + theMinimum) * theScale) >= theScale * m.mHeightMapSmoothness)
                m.mHeights[x][y] = theSum;
            if (m.mIsSpaceMap) {
                if (m.mHeights[x][y] < theSeaLevel && (m.mTiles[x][y] & 0x7fff) == 0)
                    m.mHeights[x][y] = theSeaLevel;
            } else {
                if (inShape.HasTile(x, y) && inShape.HasTile(x - 1, y) && inShape.HasTile(x - 1, y - 1) &&
                    inShape.HasTile(x, y - 1))
                    continue;
                if (m.mHeights[x][y] < theSeaLevel)
                    m.mHeights[x][y] = theSeaLevel;
            }
        }
    }
}

// EERandomMap::MarkFeatures (the claims the game makes before the resources; walls and cliffs left out: the gate
// only needs both generators to see the same input)
static void ClaimArea(EERandomMap& m, unsigned long inX, unsigned long inY, unsigned long inSize)
{
    unsigned long theSize = m.mClaimedTiles.size();
    for (unsigned long i = 0; i < inSize; i++)
        for (unsigned long j = 0; j < inSize; j++)
            if (inX + i < theSize && j + inY < theSize)
                m.mClaimedTiles[inX + i][j + inY] = true;
}

static void MarkFeatures(EERandomMap& m)
{
    for (size_t i = 0; i < m.mPlayers.size(); i++) {
        if (m.mPlayers[i].mIsNeutral)
            continue;
        float thePlayerX = m.mPlayers[i].mPosition.GetX();
        float thePlayerY = m.mPlayers[i].mPosition.mY;
        for (size_t j = 0; j < m.mFeatures.size(); j++) {
            EERMFeature theFeature = m.mFeatures[j];
            if (theFeature.mUnitType < 0 || theFeature.mUnitType >= (long)m.mUnitNames.size())
                continue;
            unsigned long theY = UMFloor(theFeature.mPosition.GetY() + thePlayerY);
            unsigned long theX = UMFloor(theFeature.mPosition.GetX() + thePlayerX);
            ClaimArea(m, theX, theY, m.mUnitNames[theFeature.mUnitType].mSize);
        }
    }
}

// EERandomMap::CreateGrids (the map dimensions as RTSMap::Create sets them)
static void CreateGrids(EERandomMap& m, const Shape& inShape, long inTiles)
{
    unsigned long theSize = inTiles * 2;
    m.mGridSize = theSize < 0x40 ? 0x40 : theSize < 0x80 ? 0x80 : theSize < 0x100 ? 0x100 : theSize < 0x200 ? 0x200 : 0x400;
    unsigned long n = m.mGridSize + 1;
    m.mClaimedTiles.assign(n, std::vector<unsigned char>(n, 0));
    m.mTiles.assign(n, std::vector<unsigned long>(n, 0));
    m.mHeights.assign(n, std::vector<float>(n, m.mInitialElevation));
    m.mFlatTiles.assign(n, std::vector<unsigned char>(n, 0));
    m.mValidTiles.assign(n, std::vector<unsigned char>(n, 0));
    for (unsigned long x = 0; x < n; x++)
        for (unsigned long y = 0; y < n; y++)
            m.mValidTiles[x][y] = inShape.IsValidPoint(x, y);
    float theExtent = UMAbs(m.mFlatArea.GetLeft());
    float theValue = UMAbs(m.mFlatArea.GetRight());
    if (theValue > theExtent)
        theExtent = theValue;
    theValue = UMAbs(m.mFlatArea.GetBottom());
    if (theValue > theExtent)
        theExtent = theValue;
    theValue = UMAbs(m.mFlatArea.GetTop());
    if (theValue > theExtent)
        theExtent = theValue;
    theValue = (float)inShape.mSize;
    float theRadius = (theValue - theExtent) * 0.35355338f;
    m.mHalfMapSize = theValue * 0.5f;
    m.mRing.mCenter = GE2DPoint(0.5f * (float)inShape.mSize, 0.5f * (float)inShape.mSize);
    m.mRing.mMinRadius = theRadius * m.mPlayerInnerRadius;
    m.mRing.mMaxRadius = theRadius * m.mPlayerOuterRadius;
    m.mNeutralRing.mCenter = m.mRing.mCenter;
    m.mNeutralRing.mMinRadius = theRadius * m.mNeutralInnerRadius;
    m.mNeutralRing.mMaxRadius = theRadius * m.mNeutralOuterRadius;
    m.mMoonRing.mCenter = m.mRing.mCenter;
    m.mMoonRing.mMaxRadius = m.mHalfMapSize;
    m.mMoonRing.mMinRadius = (m.mHalfMapSize - m.mRing.mMaxRadius) * 0.5f + m.mRing.mMaxRadius;
}

static void CreateTeams(EERandomMap& m, long inPlayers, bool inTeams)
{
    m.mTeams.clear();
    long thePlayer = 1;
    while (thePlayer <= inPlayers) {
        EERMTeam theTeam;
        theTeam.mPlayers.push_back(thePlayer);
        theTeam.mPositions.push_back(GE2DPoint());
        thePlayer++;
        if (inTeams && thePlayer <= inPlayers) {
            theTeam.mPlayers.push_back(thePlayer);
            theTeam.mPositions.push_back(GE2DPoint());
            thePlayer++;
        }
        m.mTeams.push_back(theTeam);
    }
}

typedef long (*Step)(EERandomMap*);
struct NamedStep {
    const char* mName;
    Step mStep;
};

// one map: the game's EERandomMap::SetUp + Generate. Writes one digest (or one per stage) -> false on a generator error
static bool RunOne(const RunKey& inKey, const std::string& inVolume, const std::string& inDirectory, bool inStages,
                   FILE* inOut)
{
    long theTiles = kMapSizes[inKey.mSizeIndex][0];
    EERandomMap* theMap = new EERandomMap();
    EERandomMap& m = *theMap;
    Shape theShape;
    theShape.Create(theTiles, theTiles);

    m.mSeed = inKey.mSeed;
    m.mRandom.mSeed = inKey.mSeed;
    m.mRandomIntegers.mSeed = inKey.mSeed;
    m.mMaximumY = theTiles << 1;
    m.mMaximumX = theTiles << 1;
    m.mScriptDirectory.SetVolume(inVolume.c_str());
    m.mScriptDirectory.SetDirectory(inDirectory.c_str());
    m.mTileCount = theShape.mSize * theShape.mSize -
                   2 * ((long)(theTiles * (theTiles - 1) * 0.5f) + (long)(theTiles * (theTiles - 1) * 0.5f));
    m.mClumpCircles.resize(0);
    m.mPlayerCount = inKey.mPlayers;
    m.mStartEpoch = 1;
    m.mMapSize = kMapSizes[inKey.mSizeIndex][1];
    m.mUnitNames = gTables.mUnits;
    m.mTerrainNames = gTables.mTerrains;
    m.mCliffNames = gTables.mCliffs;
    m.mFeatures.clear();

    std::string theDigests;
    auto theStage = [&](const char* inName) {
        if (inStages) {
            theDigests += " ";
            theDigests += inName;
            theDigests += "=";
            theDigests += HexDigest(m);
        }
    };

    long theResult = EERMPIMSetRandomMapValues(&m, inKey.mScript.c_str());
    if (theResult != 0)
        return false;
    CreateGrids(m, theShape, theTiles);
    CreateTeams(m, inKey.mPlayers, inKey.mMode == 1);
    if (inKey.mMode == 2) {
        m.mFilterHeightMap = true;
        m.mGenerateShallows = true;
        m.mMaxShallowConnectionsPerPlayer = 2;
        m.mShallowsWidth = 3;
        m.mShallowsElevation = -0.5f;
    }
    theStage("script");

    if ((m.mMapCharacteristics & kRMMapPlanetsMap) == kRMMapPlanetsMap) {
        m.mNumTerrainBands = (unsigned long)(m.mTerrainRegions[m.mTerrainRegions.size() - 1].mTo + 1) / 100;
        m.mTerrainBandOffset = m.mRandomIntegers.Generate(0, m.mNumTerrainBands);
    }
    const NamedStep theSteps[] = {{"players", EERMPIMGeneratePlayerPositions},
                                  {"neutrals", EERMPIMGenerateNeutralPositions},
                                  {"land", EERMPIMCreatePlayerLandWaterMap},
                                  {"neutralland", EERMPIMCreateNeutralLandWaterMap}};
    for (const NamedStep& s : theSteps) {
        if (s.mStep(&m) != 0)
            return false;
        theStage(s.mName);
    }
    WaterPoints(m, theShape);
    theStage("water");
    const NamedStep theSteps2[] = {{"flat", EERMPIMCreateFlatMap}, {"height", EERMPIMCreateHeightMap}, {"shallows", EERMPIMGenerateShallows}};
    for (const NamedStep& s : theSteps2) {
        if (s.mStep(&m) != 0)
            return false;
        theStage(s.mName);
    }
    if (m.mFilterHeightMap) {
        if (EERMPIMFilterHeightMap(&m) != 0)
            return false;
    }
    theStage("filter");
    SmoothHeights(m, theShape);
    theStage("smooth");
    MarkFeatures(m);
    theStage("mark");
    if (EERMPIMGenerateResources(&m) != 0)
        return false;
    theStage("resources");
    if (EERMPIMCreateTrees(&m) != 0)
        return false;
    theStage("trees");

    fprintf(inOut, "%s|%d|%ld|%s|%lu %s%s\n", inKey.mScript.c_str(), inKey.mSizeIndex, inKey.mPlayers,
            kModeNames[inKey.mMode], inKey.mSeed, HexDigest(m).c_str(), theDigests.c_str());
    fflush(inOut);
    delete theMap;
    return true;
}


// ---- small synthetic maps: the golden values of ee_rmap's own unit tests (rust/crates/ee_rmap/src/golden.rs builds
// the same maps by hand and must reach the same digests). FNV-1a over every generator output as little-endian words.
struct Fnv {
    uint64_t mHash;
    Fnv() : mHash(0xcbf29ce484222325ull) {}
    void U32(uint32_t v)
    {
        for (int k = 0; k < 4; k++) {
            mHash ^= (v >> (8 * k)) & 0xff;
            mHash *= 0x100000001b3ull;
        }
    }
    void F32(float v) { U32(*(uint32_t*)&v); }
    void Point(const GE2DPoint& p) { F32(p.mX); F32(p.mY); }
};

static uint64_t SyntheticDigest(EERandomMap& m)
{
    Fnv h;
    size_t n = m.mTiles.size();
    h.U32((uint32_t)n);
    for (size_t x = 0; x < n; x++)
        for (size_t y = 0; y < n; y++) {
            h.F32(m.mHeights[x][y]);
            h.U32((uint32_t)m.mTiles[x][y]);
            h.U32(m.mFlatTiles[x][y]);
            h.U32(m.mClaimedTiles[x][y]);
        }
    h.U32((uint32_t)m.mPlayers.size());
    for (size_t i = 0; i < m.mPlayers.size(); i++) {
        EERMPlayer& p = m.mPlayers[i];
        h.Point(p.mPosition);
        h.F32(p.mRadius);
        h.U32((uint32_t)p.mIndex);
        h.U32((uint32_t)p.mTeam);
        h.U32((uint32_t)p.mTileCount);
        h.U32((p.mIsNeutral ? 1 : 0) | (p.mIsOuter ? 2 : 0) | (p.mIsMoon ? 4 : 0) | (p.mIsPlaced ? 8 : 0));
        h.U32((uint32_t)p.mTiles.size());
        for (size_t j = 0; j < p.mTiles.size(); j++)
            h.Point(p.mTiles[j]);
        h.U32((uint32_t)p.mClumpCenters.size());
        for (size_t j = 0; j < p.mClumpCenters.size(); j++)
            h.Point(p.mClumpCenters[j]);
    }
    h.U32((uint32_t)m.mObjects.size());
    for (size_t i = 0; i < m.mObjects.size(); i++) {
        h.Point(m.mObjects[i].mLocation);
        h.U32((uint32_t)m.mObjects[i].mKind);
        if (m.mObjects[i].mKind == kRMObjectTree)
            h.U32((uint32_t)m.mObjects[i].mType);
    }
    h.U32((uint32_t)m.mFeatures.size());
    for (size_t i = 0; i < m.mFeatures.size(); i++) {
        h.F32(m.mFeatures[i].mAngle);
        h.F32(m.mFeatures[i].mPosition.mX);
        h.F32(m.mFeatures[i].mPosition.mY);
        h.U32((uint32_t)m.mFeatures[i].mUnitType);
        h.U32((uint32_t)m.mFeatures[i].mIndex);
    }
    h.U32((uint32_t)m.mClumpCircles.size());
    for (size_t i = 0; i < m.mClumpCircles.size(); i++) {
        h.U32((uint32_t)m.mClumpCircles[i].size());
        for (size_t j = 0; j < m.mClumpCircles[i].size(); j++) {
            h.Point(m.mClumpCircles[i][j].mCenter);
            h.F32(m.mClumpCircles[i][j].mRadius);
        }
    }
    for (size_t i = 0; i < m.mTeams.size(); i++)
        for (size_t j = 0; j < m.mTeams[i].mPositions.size(); j++)
            h.Point(m.mTeams[i].mPositions[j]);
    h.U32(m.mRandom.mSeed);
    h.U32(m.mRandomIntegers.mSeed);
    return h.mHash;
}

// A 64-tile all-valid grid: the settings of scenario inCase (0 two lone players; 1 four players in two teams on a
// river map; 2 a space map; 3 filter and shallows forced)
static void SyntheticSetup(EERandomMap& m, int inCase, unsigned long inSeed)
{
    m.mSeed = inSeed;
    m.mRandom.mSeed = inSeed;
    m.mRandomIntegers.mSeed = inSeed + 1;
    m.mGridSize = 64;
    m.mMaximumX = 64;
    m.mMaximumY = 64;
    m.mTileCount = 65 * 65;
    unsigned long n = 65;
    m.mValidTiles.assign(n, std::vector<unsigned char>(n, 1));
    for (unsigned long i = 0; i < n; i++) {   // an invalid border, as a real map has: off-map points fail the masks
        m.mValidTiles[0][i] = m.mValidTiles[n - 1][i] = 0;
        m.mValidTiles[i][0] = m.mValidTiles[i][n - 1] = 0;
    }
    m.mClaimedTiles.assign(n, std::vector<unsigned char>(n, 0));
    m.mFlatTiles.assign(n, std::vector<unsigned char>(n, 0));
    m.mTiles.assign(n, std::vector<unsigned long>(n, 0));
    m.mHeights.assign(n, std::vector<float>(n, 0.5f));
    m.mMaxLoopCount = 500;
    m.mMaxTreeLoopCount = 200;
    m.mMaxIterationsClumpGrowth = 300;
    m.mConvergentSampleSize = 2.0f;
    m.mConvergentThreshold = 0.5f;
    m.mElevationScale = 0.8f;
    m.mElevationUnit = 1.0f / 30.0f;
    m.mFlatElevation = 11.0f / 30.0f;
    m.mInitialElevation = 0.5f;
    m.mHeightMapChaos = 0.4f;
    m.mLandElevationThreshold = 10.0f / 30.0f;
    m.mMaxIntElevation = 20;
    m.mMinIntElevation = -10;
    m.mMaxResourceElevation = 0.9f;
    m.mMaxTreeElevation = 0.9f;
    m.mPercentLand = 0.4f;
    m.mLandfillPercentage = 0.05f;
    m.mPlayerFlatChaos = 0.5f;
    m.mPlayerLandChaos = 0.6f;
    m.mPlayerPercentFlat = 0.3f;
    m.mPlayerFlatClumps = 2;
    m.mPlayerLandClumps = 3;
    m.mPlayerTreePercentage = 0.1f;
    m.mForestChaosLevel = 0.4f;
    m.mForestFreeRadius = 4.0f;
    m.mForestsPerPlayer = 2;
    m.mMaxClumpsPerForest = 2;
    m.mMaxAngleBetweenTeamMembers = 0.6f;
    m.mWaterBorder = 2;
    m.mResourceToEdgeDistance = 1;
    m.mResourceToWaterDistance = 1;
    m.mResourceSeperation = 3.0f;
    m.mCoastalToDeeperWaterPercentage = 0.25f;
    m.mFlatArea.mLeft = -2.0f;
    m.mFlatArea.mBottom = -2.0f;
    m.mFlatArea.mRight = 2.0f;
    m.mFlatArea.mTop = 2.0f;
    m.mHalfMapSize = 32.0f;
    GE2DPoint theCenter(32.0f, 32.0f);
    m.mRing.mCenter = theCenter;
    m.mRing.mMinRadius = 14.0f;
    m.mRing.mMaxRadius = 20.0f;
    m.mNeutralRing = m.mRing;
    m.mMoonRing.mCenter = theCenter;
    m.mMoonRing.mMinRadius = 24.0f;
    m.mMoonRing.mMaxRadius = 30.0f;
    for (int i = 0; i < 7; i++) {
        m.mKindSpacing[i][0] = 8;
        m.mKindSpacing[i][1] = 100;
        m.mResourcePerPlayer[i] = i < 2 || i == 6 ? 2 : 1;
        m.mResourcePerNeutralLand[i] = i == 5 ? 2 : 1;
    }
    m.mTreeTypes.push_back(3);
    m.mTreeTypes.push_back(7);
    EERMResourceKind theKind;
    theKind.mMinimum = 6;
    theKind.mMaximum = 14;
    theKind.mId = 42;
    theKind.mUnitType = 5;
    theKind.mArea = 9;
    m.mResourceKinds.push_back(theKind);
    m.mResourceKindIds.push_back(42);
    EERMResourceSlot theSlot;
    theSlot.mId = 42;
    theSlot.mPerPlayer = true;
    theSlot.mRandomKind = false;
    m.mResourceSlots.push_back(theSlot);
    theSlot.mPerPlayer = false;
    theSlot.mRandomKind = true;
    m.mResourceSlots.push_back(theSlot);
    EERMSpacingRule theRule;
    theRule.mKind = 0;
    theRule.mMinimum = 8;
    theRule.mMaximum = 16;
    m.mSpacingRules.push_back(theRule);
    m.mNumInnerNeutrals = 0;
    m.mNumOuterNeutrals = 0;
    long thePlayers = 2;
    bool theTeams = false;
    if (inCase == 1) {
        thePlayers = 4;
        theTeams = true;
        m.mRiverWidth = 3;
        m.mTeamGrowth = true;
    } else if (inCase == 2) {
        m.mIsSpaceMap = true;
    } else if (inCase == 3) {
        m.mFilterHeightMap = true;
        m.mGenerateShallows = true;
        m.mMaxShallowConnectionsPerPlayer = 2;
        m.mShallowsWidth = 3;
        m.mShallowsElevation = 0.4f;
    }
    CreateTeams(m, thePlayers, theTeams);
}

static void RunSynthetic()
{
    for (int theCase = 0; theCase < 4; theCase++)
        for (unsigned long theSeed = 1; theSeed <= 3; theSeed++) {
            EERandomMap* m = new EERandomMap();
            SyntheticSetup(*m, theCase, theSeed);
            EERMPIMGeneratePlayerPositions(m);
            EERMPIMGenerateNeutralPositions(m);
            EERMPIMCreatePlayerLandWaterMap(m);
            for (unsigned long x = 0; x < 64; x++)
                for (unsigned long y = 0; y < 64; y++)
                    if ((m->mTiles[x][y] & 0x7fff) == 0)
                        m->mWaterPoints.push_back(GE2DPoint((float)x, (float)y));
            EERMPIMCreateFlatMap(m);
            EERMPIMCreateHeightMap(m);
            EERMPIMGenerateShallows(m);
            if (m->mFilterHeightMap)
                EERMPIMFilterHeightMap(m);
            EERMPIMGenerateResources(m);
            EERMPIMCreateTrees(m);
            printf("%d %lu %016llx\n", theCase, theSeed, (unsigned long long)SyntheticDigest(*m));
            delete m;
        }
}

static std::vector<std::string> ListScripts(const std::string& inDir)
{
    std::vector<std::string> theNames;
    WIN32_FIND_DATAA theFound;
    HANDLE theHandle = FindFirstFileA((inDir + "*.rmv").c_str(), &theFound);
    if (theHandle != INVALID_HANDLE_VALUE) {
        do {
            std::string theName = theFound.cFileName;
            theNames.push_back(theName.substr(0, theName.size() - 4));
        } while (FindNextFileA(theHandle, &theFound));
        FindClose(theHandle);
    }
    std::sort(theNames.begin(), theNames.end());
    return theNames;
}

int main(int argc, char** argv)
{
    if (argc == 2 && strcmp(argv[1], "--synthetic") == 0) {
        RunSynthetic();
        return 0;
    }
    std::string theData;
    bool theStages = false;
    int theShard = 0, theShards = 1;
    std::string theOnly;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--stages")
            theStages = true;
        else if (a == "--shard" && i + 2 < argc) {
            theShard = atoi(argv[++i]);
            theShards = atoi(argv[++i]);
        } else if (a == "--only" && i + 1 < argc)
            theOnly = argv[++i];
        else
            theData = a;
    }
    if (theData.empty()) {
        fprintf(stderr, "usage: rmap_dump <data tree> [--stages] [--shard K N] [--only key]\n");
        return 2;
    }
    if (theData.back() != '\\' && theData.back() != '/')
        theData += '\\';
    for (size_t i = 0; i < theData.size(); i++)
        if (theData[i] == '/')
            theData[i] = '\\';
    gScale = 2.0f;   // RTSMap's mF1955F4 (world units per elevation step) is set by EEMap; only SmoothHeights reads it

    if (!ReadUnits(theData + "Data\\db\\dbobjects.yaml", gTables.mUnits) ||
        !ReadCsvNames(theData + "Data\\db\\dbterrain.csv", "mName", gTables.mTerrains) ||
        !ReadCsvNames(theData + "Data\\db\\dbcliffterrain.csv", "mName", gTables.mCliffs)) {
        fprintf(stderr, "rmap_dump: cannot read the unit/terrain tables under %s\n", theData.c_str());
        return 2;
    }
    std::string theScripts = theData + "Data\\Random Map Scripts\\";
    size_t theSlash = theScripts.find('\\');
    std::string theVolume = theScripts.substr(0, theSlash);
    std::string theDirectory = theScripts.substr(theSlash);

    int theRun = 0, theErrors = 0;
    for (const std::string& theScript : ListScripts(theScripts))
        for (int s = 0; s < 6; s++) {
            for (int p = 0; kPlayers[s][p] != 0; p++) {
                long thePlayers = kPlayers[s][p];
                for (int t = 0; t < 3; t++) {
                    if ((t == 1 && thePlayers < 4) || (t == 2 && s > 1))
                        continue;
                    for (unsigned long theSeed : kSeeds) {
                        RunKey theKey{theScript, s, thePlayers, t, theSeed};
                        char theText[128];
                        sprintf(theText, "%s|%d|%ld|%s|%lu", theScript.c_str(), s, thePlayers, kModeNames[t], theSeed);
                        if (!theOnly.empty() && theOnly != theText)
                            continue;
                        if (theRun++ % theShards != theShard)
                            continue;
                        if (!RunOne(theKey, theVolume, theDirectory, theStages, stdout)) {
                            printf("%s ERROR\n", theText);
                            theErrors++;
                        }
                    }
                }
            }
        }
    fprintf(stderr, "rmap_dump: %d of %d runs in this shard errored\n", theErrors, theRun);
    return 0;
}
