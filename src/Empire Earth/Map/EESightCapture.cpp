// New (port), TEST ONLY: TSGT, the tile sight's capture record, and the app profile's end sweep of EEMap's sight and
// out-of-synch checksum (ee/EESightCapture.h; tools/sim_capture.py). Reads the objects; the sweep calls the map's
// sight and checksum as the game does.
#include <windows.h>
#include <math.h>
#include <string.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEMap.h"
#include "ee/EEDatabaseTables.h"
#include "rust/render/ge.h"
#include "ee/EETechCapture.h"

namespace {

const uint64_t kFnvBasis = 0xcbf29ce484222325ull;

void Fnv(uint64_t& ioHash, const void* inData, size_t inSize)
{
    const unsigned char* theBytes = static_cast<const unsigned char*>(inData);
    for (size_t i = 0; i < inSize; i++)
        ioHash = (ioHash ^ theBytes[i]) * 0x100000001b3ull;
}

template <class T> void FnvValue(uint64_t& ioHash, T inValue)
{
    Fnv(ioHash, &inValue, sizeof(inValue));
}

thread_local int sSightDepth = 0;

unsigned long Words(EEMap* inMap)
{
    return (inMap->mU19561C >> 1) + (inMap->mU19561C & 1);
}

// the tile's index in the map's tile array (-1: not one of them)
long TileIndex(EEMap* inMap, const RTSTile* inTile)
{
    if (inTile == NULL || inMap->mTileArray == NULL)
        return -1;
    const EETile* theTile = static_cast<const EETile*>(inTile);
    if (theTile < inMap->mTileArray || theTile >= inMap->mTileArray + inMap->mTileArrayCount)
        return -1;
    return (long)(theTile - inMap->mTileArray);
}

// the tiles from floor(c) - reach to floor(c) + reach on both axes
void Around(float inX, float inY, float inRadius, long inClamp, long* outRect)
{
    long theReach = 0;
    if (inRadius >= 0.0f && inRadius < 100000.0f)
        theReach = (long)ceilf(inRadius);
    if (inClamp >= 0 && theReach > inClamp)
        theReach = inClamp;
    theReach++;
    long theX = (inX > -100000.0f && inX < 100000.0f) ? (long)floorf(inX) : 0;
    long theY = (inY > -100000.0f && inY < 100000.0f) ? (long)floorf(inY) : 0;
    outRect[0] = theX - theReach;
    outRect[1] = theX + theReach;
    outRect[2] = theY - theReach;
    outRect[3] = theY + theReach;
}

// every tile of the rectangle (clipped to the map): its visibility words and its mesh tile's reference count, in row
// order; a missing tile hashes a marker
uint64_t RectHash(EEMap* inMap, const long* inRect, unsigned int& outTiles)
{
    uint64_t theHash = kFnvBasis;
    outTiles = 0;
    long theLeft = inRect[0], theRight = inRect[1], theBottom = inRect[2], theTop = inRect[3];
    if (theLeft > theRight) {
        theLeft = 0;
        theRight = inMap->mSize - 1;
        theBottom = 0;
        theTop = inMap->mSize - 1;
    }
    if (theLeft < 0)
        theLeft = 0;
    if (theBottom < 0)
        theBottom = 0;
    if (theRight > inMap->mSize - 1)
        theRight = inMap->mSize - 1;
    if (theTop > inMap->mSize - 1)
        theTop = inMap->mSize - 1;
    unsigned long theWords = Words(inMap);
    GETerrainMesh& theMesh = inMap->mTerrainMesh;
    for (long y = theBottom; y <= theTop; y++) {
        for (long x = theLeft; x <= theRight; x++) {
            outTiles++;
            EEMapTile* theTile = inMap->GetTile(x, y);
            if (theTile == NULL || theTile->mVisibility == NULL) {
                FnvValue(theHash, (uint32_t)0xfffffff1u);
                continue;
            }
            Fnv(theHash, theTile->mVisibility, theWords * sizeof(uint32_t));
            if (theMesh.GetIsValidTile(x, y))
                FnvValue(theHash, ee_ge_tm_tile(theMesh.GetRust(), x, y)->reference_count);
            else
                FnvValue(theHash, (uint32_t)0xfffffff2u);
        }
    }
    return theHash;
}

}   // namespace

EESightScope::EESightScope(unsigned int inKind, EEPlayer* inPlayer, const RTSTile* inTile, long inX, long inY,
                           unsigned long inValue)
    : mOuter(sSightDepth++ == 0), mKind(inKind), mPlayer(inPlayer), mTile(inTile), mArgs{0, 0, 0, 0, 0}, mTileX(inX),
      mTileY(inY), mValue(inValue), mRect{0, -1, 0, -1}
{
}

EESightScope::EESightScope(unsigned int inKind, EEPlayer* inPlayer, float inX0, float inY0, float inX1, float inY1,
                           float inRadius, unsigned long inValue)
    : mOuter(sSightDepth++ == 0), mKind(inKind), mPlayer(inPlayer), mTile(NULL), mArgs{inX0, inY0, inX1, inY1, inRadius},
      mTileX(-1), mTileY(-1), mValue(inValue), mRect{0, -1, 0, -1}
{
    if (!this->mOuter)
        return;
    // the circles clamp their radius to 32 tiles; the map's sight does not
    long theClamp = (inKind == kSightMapAdd || inKind == kSightMapRemove) ? -1 : 32;
    Around(inX0, inY0, inRadius, theClamp, this->mRect);
    if (inKind == kSightRingAdd || inKind == kSightRingRemove || inKind == kSightCircleMove) {
        long theOther[4];
        Around(inX1, inY1, inRadius, theClamp, theOther);
        this->mRect[0] = min(this->mRect[0], theOther[0]);
        this->mRect[1] = max(this->mRect[1], theOther[1]);
        this->mRect[2] = min(this->mRect[2], theOther[2]);
        this->mRect[3] = max(this->mRect[3], theOther[3]);
    }
}

EESightScope::EESightScope(unsigned int inKind, EEPlayer* inPlayer)
    : mOuter(sSightDepth++ == 0), mKind(inKind), mPlayer(inPlayer), mTile(NULL), mArgs{0, 0, 0, 0, 0}, mTileX(-1),
      mTileY(-1), mValue(0), mRect{1, 0, 1, 0}
{
}

EESightScope::~EESightScope()
{
    sSightDepth--;
    if (!this->mOuter || !EESimCaptureRecord::Active() || gWorld.mMap == NULL)
        return;
    EEMap* theMap = gWorld.mMap;
    uint64_t theHash = kFnvBasis;
    unsigned int theTiles = 0;
    long theIndex = -1;
    if (this->mTile != NULL || this->mKind <= kSightPlayerRemoveOwn) {
        // a tile's call: that tile's visibility words
        theIndex = TileIndex(theMap, this->mTile);
        const RTSTile* theTile = this->mTile;
        if (theTile != NULL && theTile->mVisibility != NULL) {
            Fnv(theHash, theTile->mVisibility, Words(theMap) * sizeof(uint32_t));
            theTiles = 1;
        }
    } else
        theHash = RectHash(theMap, this->mRect, theTiles);
    EESimCaptureRecord r(EECaptureTag("TSGT"));
    r.U32(this->mKind);
    r.U32(this->mPlayer != NULL ? (unsigned int)this->mPlayer->mPlayerID : 0xffffffffu);
    r.U32((unsigned int)this->mValue);
    r.I32((int)theIndex);
    r.I32((int)this->mTileX);
    r.I32((int)this->mTileY);
    r.F32s(this->mArgs, 5);
    for (long theSide : this->mRect)
        r.I32((int)theSide);
    r.U32(theTiles);
    r.U32((unsigned int)theHash);
    r.U32((unsigned int)(theHash >> 32));
    r.Commit();
}

void EESightCaptureSnapshot()
{
    if (!EESimCaptureRecord::Active() || gWorld.mMap == NULL)
        return;
    EESightScope theScope(kSightSnapshot, NULL);
}

void EESightCaptureSweep()
{
    if (!EESimCaptureRecord::Active() || gWorld.mMap == NULL)
        return;
    EEMap* theMap = gWorld.mMap;
    float theRadius = gWorldTable.GetFloatValue(kWorldWonderSightRadius);
    float theSize = (float)theMap->mSize;
    const float kPoints[][2] = {{theSize * 0.5f, theSize * 0.5f}, {5.5f, 7.5f}, {theSize - 3.25f, theSize * 0.3f}};
    for (unsigned long p = 1; p < gWorld.mNumPlayers; p++) {
        EEPlayer* thePlayer = gWorld.mPlayers[p];
        if (thePlayer == NULL)
            continue;
        for (const float* thePoint : kPoints) {
            theMap->AddSight(thePlayer, thePoint[0], thePoint[1], theRadius, false);
            theMap->AddSight(thePlayer, thePoint[0] + 2.0f, thePoint[1] - 1.0f, theRadius * 0.5f, true);
            theMap->RemoveSight(thePlayer, thePoint[0], thePoint[1], theRadius, false);
        }
        theMap->RemoveSight(thePlayer, kPoints[0][0] + 2.0f, kPoints[0][1] - 1.0f, theRadius * 0.5f, true);
    }
    if (gWorld.mNumPlayers > 1) {
        EEPlayer* theLast = gWorld.mPlayers[gWorld.mNumPlayers - 1];
        theMap->ExploreUnseen(theLast);
        theMap->FogSeen(theLast);
    }
    // two full cycles of the checksum's 64 x 64 windows
    long theBands = (theMap->mSize + 63) / 64;
    for (long i = 0; i < 2 * theBands * theBands + 1; i++)
        theMap->ComputeChecksum();
}
