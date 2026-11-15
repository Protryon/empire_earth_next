// C:\EEX\Empire Earth\Units\EESightRings.cpp (file name inferred: no __FILE__, no RTTI). The line-of-sight
// rings: 0x20834c..0x208c37 (unit map [C], between EEGroup.cpp and EEOrbital's range 0x208c37; edge: the globals
// chain). gSightRings[r] holds the tile offsets at a ceiling distance of r (0..32) from a unit's centre, built once at
// startup (EEBuildSightRings, called by Empire Earth.cpp); the reveal/hide helpers walk them for a unit's player and the
// current player's mini map and terrain reference counts (EEComplexUnit.cpp calls EEAddSightCircle, EERemoveSightCircle and
// EEMoveSightCircle). Unit flags: the exe's module set (provisional names).
#include <windows.h>
#include <math.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEMiniMap.h"

// the largest sight radius the rings cover (0x446f60, loaded through memory)
static const float kMaxSightRadius = 32.0f;

// SYNTHETIC: EXE 0x60834c   (wrapper)
// SYNTHETIC: EXE 0x608356   (ctor)
// SYNTHETIC: EXE 0x60837a   (registrar)
// SYNTHETIC: EXE 0x608386   (dtor)
// the original dtor tests a guard bit (0x963288): a static data member of a class not identified yet
// A ring entry. Not a GE2DPoint: its copy is memberwise (`mov edx, [ecx + eax*8]; mov eax, [ecx + eax*8 + 4]` where GE2DPoint's
// inline copy ctor goes through an address; push_back 0x608c0b is its own COMDAT next to GE2DPoint's 0x688c8d).
struct EESightOffset {
    EESightOffset(float inX, float inY) : mX(inX), mY(inY) {}
    float GetX() const { return this->mX; }
    float GetY() const { return this->mY; }
    float mX;
    float mY;
};
std::vector<EESightOffset> gSightRings[33];     // 0x963290 (r = 0..32)
bool gSightRingsBuilt;                      // 0x9634a0

// FUNCTION: EXE 0x6083b0
// Adds a player's sight to every tile within a radius (at most 32) of a point, showing them if the current player
// is that player or an ally both sharing line of sight
void EEAddSightCircle(float inX, float inY, float inRadius, EEPlayer* inPlayer)
{
    EEMap* theMap = gWorld.mMap;
    long theRadius = UMFloatToLong(inRadius);
    bool theShow;
    if (gWorld.GetCurrentPlayer() == inPlayer ||
        (gWorld.GetCurrentPlayer()->IsAlliedWith(inPlayer) && gWorld.GetCurrentPlayer()->mSharedLOS && inPlayer->mSharedLOS))
        theShow = true;
    else
        theShow = false;
    if (theRadius > UMFloatToLong(kMaxSightRadius))
        theRadius = UMFloatToLong(kMaxSightRadius);
    for (long r = 0; r <= theRadius; r++) {
        long theCount = gSightRings[r].size();
        for (long i = 0; i < theCount; i++) {
            EESightOffset theOffset = gSightRings[r][i];
            float theX = theOffset.GetX() + inX;
            float theY = theOffset.GetY() + inY;
            long theTileX = UMFloor(theX);
            long theTileY = UMFloor(theY);
            EETile* theTile = theMap->GetTile(theTileX, theTileY);
            if (theTile != NULL) {
                inPlayer->AddSharedSight(theTile, theTileX, theTileY);
                if (theShow) {
                    gMiniMap->CountCellOn(theTileX, theTileY);
                    theMap->mTerrainMesh.ReferenceTile(theTileX, theTileY);
                }
            }
        }
    }
}

// FUNCTION: EXE 0x608550
// Adds a player's sight to the tiles of the outer two rings around the new centre that the old one did not see
void EEAddSightRingStep(float inOldX, float inOldY, float inNewX, float inNewY, float inRadius, EEPlayer* inPlayer)
{
    EEMap* theMap = gWorld.mMap;
    bool theShow;
    if (gWorld.GetCurrentPlayer() == inPlayer ||
        (gWorld.GetCurrentPlayer()->IsAlliedWith(inPlayer) && gWorld.GetCurrentPlayer()->mSharedLOS && inPlayer->mSharedLOS))
        theShow = true;
    else
        theShow = false;
    if (inRadius > kMaxSightRadius)
        inRadius = kMaxSightRadius;
    long theRadius = UMFloatToLong(inRadius);
    float theRadiusSquared = (float)theRadius * (float)theRadius;
    for (long r = theRadius - 1; r <= theRadius; r++) {
        if (r < 0)
            continue;
        long theCount = gSightRings[r].size();
        for (long i = 0; i < theCount; i++) {
            EESightOffset theOffset = gSightRings[r][i];
            float theX = theOffset.GetX() + inNewX;
            float theY = theOffset.GetY() + inNewY;
            long theTileX = UMFloor(theX);
            long theTileY = UMFloor(theY);
            EETile* theTile = theMap->GetTile(theTileX, theTileY);
            if (theTile != NULL) {
                float theDX = theX - inOldX;
                float theDY = theY - inOldY;
                if (theDX * theDX + theDY * theDY > theRadiusSquared) {
                    inPlayer->AddSharedSight(theTile, theTileX, theTileY);
                    if (theShow) {
                        gMiniMap->CountCellOn(theTileX, theTileY);
                        theMap->mTerrainMesh.ReferenceTile(theTileX, theTileY);
                    }
                }
            }
        }
    }
}

// FUNCTION: EXE 0x60872b
// Builds the tile offsets of each sight ring (distance rounded up, 0..32 tiles from the centre), once at startup
void EEBuildSightRings()
{
    for (long x = -32; x <= 32; x++) {
        for (long y = -32; y <= 32; y++) {
            float theDistance = (float)sqrt((double)(x * x + y * y));
            long theRing = UMFloor(theDistance);
            if (theDistance != (float)theRing)
                theRing++;
            if (theRing <= 32)
                gSightRings[theRing].push_back(EESightOffset((float)x, (float)y));
        }
    }
    gSightRingsBuilt = true;
}

// FUNCTION: EXE 0x6087cb
// Removes a player's sight from every tile within a radius (at most 32) of a point, hiding them if the current player
// is that player or an ally both sharing line of sight
void EERemoveSightCircle(float inX, float inY, float inRadius, EEPlayer* inPlayer)
{
    EEMap* theMap = gWorld.mMap;
    long theRadius = UMFloatToLong(inRadius);
    bool theShow;
    if (gWorld.GetCurrentPlayer() == inPlayer ||
        (gWorld.GetCurrentPlayer()->IsAlliedWith(inPlayer) && gWorld.GetCurrentPlayer()->mSharedLOS && inPlayer->mSharedLOS))
        theShow = true;
    else
        theShow = false;
    if (theRadius > UMFloatToLong(kMaxSightRadius))
        theRadius = UMFloatToLong(kMaxSightRadius);
    for (long r = 0; r <= theRadius; r++) {
        long theCount = gSightRings[r].size();
        for (long i = 0; i < theCount; i++) {
            EESightOffset theOffset = gSightRings[r][i];
            float theX = theOffset.GetX() + inX;
            float theY = theOffset.GetY() + inY;
            long theTileX = UMFloor(theX);
            long theTileY = UMFloor(theY);
            EETile* theTile = theMap->GetTile(theTileX, theTileY);
            if (theTile != NULL) {
                inPlayer->RemoveSharedSight(theTile);
                if (theShow) {
                    gMiniMap->CountCellOff(theTileX, theTileY);
                    theMap->mTerrainMesh.DereferenceTile(theTileX, theTileY);
                }
            }
        }
    }
}

// FUNCTION: EXE 0x608958
// Removes a player's sight from the tiles of the outer two rings around the old centre that the new one does not see
void EERemoveSightRingStep(float inOldX, float inOldY, float inNewX, float inNewY, float inRadius, EEPlayer* inPlayer)
{
    EEMap* theMap = gWorld.mMap;
    bool theShow;
    if (gWorld.GetCurrentPlayer() == inPlayer ||
        (gWorld.GetCurrentPlayer()->IsAlliedWith(inPlayer) && gWorld.GetCurrentPlayer()->mSharedLOS && inPlayer->mSharedLOS))
        theShow = true;
    else
        theShow = false;
    if (inRadius > kMaxSightRadius)
        inRadius = kMaxSightRadius;
    long theRadius = UMFloatToLong(inRadius);
    float theRadiusSquared = (float)theRadius * (float)theRadius;
    for (long r = theRadius - 1; r <= theRadius; r++) {
        if (r < 0)
            continue;
        long theCount = gSightRings[r].size();
        for (long i = 0; i < theCount; i++) {
            EESightOffset theOffset = gSightRings[r][i];
            float theX = theOffset.GetX() + inOldX;
            float theY = theOffset.GetY() + inOldY;
            long theTileX = UMFloor(theX);
            long theTileY = UMFloor(theY);
            EETile* theTile = theMap->GetTile(theTileX, theTileY);
            if (theTile != NULL) {
                float theDX = theX - inNewX;
                float theDY = theY - inNewY;
                if (theDX * theDX + theDY * theDY > theRadiusSquared) {
                    inPlayer->RemoveSharedSight(theTile);
                    if (theShow) {
                        gMiniMap->CountCellOff(theTileX, theTileY);
                        theMap->mTerrainMesh.DereferenceTile(theTileX, theTileY);
                    }
                }
            }
        }
    }
}

// FUNCTION: EXE 0x608b20
// Moves a unit's sight: a step of at most one tile updates only the outer rings, a longer one redoes the whole circle
void EEMoveSightCircle(float inOldX, float inOldY, float inNewX, float inNewY, float inRadius, EEPlayer* inPlayer)
{
    float theDX = inOldX - inNewX;
    float theDY = inOldY - inNewY;
    if (theDX < -1.0f || theDX > 1.0f || theDY < -1.0f || theDY > 1.0f) {
        EEAddSightCircle(inNewX, inNewY, inRadius, inPlayer);
        EERemoveSightCircle(inOldX, inOldY, inRadius, inPlayer);
    } else {
        EEAddSightRingStep(inOldX, inOldY, inNewX, inNewY, inRadius, inPlayer);
        EERemoveSightRingStep(inOldX, inOldY, inNewX, inNewY, inRadius, inPlayer);
    }
}

// TEMPLATE: EXE 0x608c0b
// (vector<GE2DPoint>::push_back, first used by EEBuildSightRings)
