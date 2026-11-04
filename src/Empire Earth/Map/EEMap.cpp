// C:\EEX\Empire Earth\Map\EEMap.cpp (file name inferred: no __FILE__, no asserts). The game map (RTTI EEMap > RTSMap,
// FSPersistent; vtables 0x441cbc / 0x441c9c): 0x11b844..0x11e36e (the two ctors, ??_G, the dtor and the members in
// definition order, then the inline COMDATs first used here: EEPlayer::IsAlliedWith 0x11c10d, vector<GEColor>::
// push_back, the region tables, the FSPersistent thunks, the database tables' inline statics, EETile's ctor).
// Unit flags: the exe's module set.
#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <vector>
#include <utility>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/GETextureSet.h"
#include "lle/GE3DLine.h"
#include "lle/GEViewport.h"
#include "lle/GEPhysicalModel.h"
#include "lle/U2DPoint.h"
#include "lle/U2DRectangle.h"
#include "lle/URandom.h"
#include "lle/FSFile.h"

#include "ee/EETileLocation.h"
#include "ee/EETileCoordinate.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEEffectsTable.h"
#include "ee/EEMiniMap.h"
#include "ee/EEGraphics.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EETerrainColorTable.h"
#include "ee/EEPersistentTag.h"

extern long gDefaultTerrain;                // 0x4e86d0 (EETerrainTable.cpp)
extern long gFirstCliffTerrain;             // 0x4e86e8 (EETerrainTable.cpp)
extern bool gSightUpdatesEnabled;           // 0x4c76ec (EEUserInterface.cpp; provisional name)
// 0x24b212 (EEFOFindNewTargetNearOldTarget.cpp's range): the tiles within inRadius of inCenter (provisional name)
void Fn24B212(const GE3DPoint& inCenter, float inRadius, std::vector<EETileCoordinate>& outTiles);

// unit-local constants (.rdata next to EEMap's vtables)
const float kMapTileSize = 0.40824828f;     // 0x441ce8 (RTSMap's ctor argument; provisional name)
const float kChecksumScale = -1000.0f;      // 0x441cf4 (ComputeChecksum)
const float kSlopeScale = -0.76923078f;     // 0x441cf8 (GetTerrainPitch / GetTerrainRoll)
const float kCliffNormalTolerance = 0.075f; // 0x441cfc (UpdateCliffs)

// FUNCTION: EXE 0x51b844
// Creates an empty map on the default terrain
EEMap::EEMap()
    : RTSMap(kMapTileSize), FSPersistent(kPersistentMap), mRandom(1)
{
    this->mU195650 = gDefaultTerrain;
    this->Initialize();
}

// SYNTHETIC: EXE 0x51b8bd
// (the scalar deleting destructor, emitted with the vtable; slot EEMap+0x10)

// FUNCTION: EXE 0x51b8d9
// Creates a map of the given size and player count, filled with the given terrain
EEMap::EEMap(long inWidth, long inHeight, unsigned long inPlayerCount, long inTerrain)
    : RTSMap(kMapTileSize), FSPersistent(kPersistentMap), mRandom(1)
{
    this->Initialize();
    this->mU195650 = inTerrain;
    this->CreateMap(inWidth, inHeight, inPlayerCount);
    this->mU19565C = gFirstCliffTerrain;
}

// FUNCTION: EXE 0x51b971
// Destroys the map: frees its region tables, tiles and visibility words and unloads its terrain textures
EEMap::~EEMap()
{
    delete this->mUnitRegions;
    delete this->mEffectRegions;
    delete[] this->mTileArray;
    if (this->mVisibilityWords)
        delete[] this->mVisibilityWords;
    EETerrainTable::Get(this->mTerrainMesh.GetGlobalWaterSetID())->Unload(this->mTerrainMesh);
    for (unsigned long x = 0; x < this->mSize + 1; x++) {
        for (unsigned long y = 0; y < this->mSize + 1; y++) {
            if (this->mTerrainMesh.GetIsValidTile(x, y) && EETerrainTable::Get(this->mTerrainMesh.GetTextureSetID(x, y)))
                EETerrainTable::Get(this->mTerrainMesh.GetTextureSetID(x, y))->Unload(this->mTerrainMesh);
        }
    }
}

// FUNCTION: EXE 0x51baa0
// Hands out the next reserved tile at the given height, with its slice of the visibility words
EEMapTile* EEMap::CreateTile(float inHeight)
{
    unsigned long thePlayers = this->mU19561C;
    EETile* theTile = &this->mTileArray[this->mNextTile];
    theTile->mHeight = inHeight;
    theTile->mVisibility = &this->mVisibilityWords[((thePlayers >> 1) + (thePlayers & 1)) * this->mNextTile];
    this->mNextTile++;
    return theTile;
}

// FUNCTION: EXE 0x51bae6
// Allocates the given number of tiles and cleared visibility words (two players per word), dropping old ones
void EEMap::ReserveTiles(unsigned long inCount)
{
    unsigned long theWords = (this->mU19561C >> 1) + (this->mU19561C & 1);
    if (this->mTileArray) {
        delete[] this->mTileArray;
        delete[] this->mVisibilityWords;
        this->mTileArray = NULL;
        this->mVisibilityWords = NULL;
    }
    this->mTileArray = new EETile[inCount];
    this->mVisibilityWords = new unsigned long[theWords * inCount];
    memset(this->mVisibilityWords, 0, theWords * inCount * sizeof(unsigned long));
    this->mNextTile = 0;
    this->mTileArrayCount = inCount;
}

// SYNTHETIC: EXE 0x51bba9
// (EETile's vector deleting destructor: Virtual1C's delete[])

// FUNCTION: EXE 0x51bc03
// Resizes every tile's visibility words for a new player count, keeping the existing visibility
void EEMap::SetPlayerCount(unsigned long inPlayerCount)
{
    unsigned long theOldWords = (this->mU19561C >> 1) + (this->mU19561C & 1);
    unsigned long theNewWords = (inPlayerCount >> 1) + (inPlayerCount & 1);
    unsigned long theCopy = theOldWords < theNewWords ? theOldWords * sizeof(unsigned long) : theNewWords * sizeof(unsigned long);
    unsigned long* theWords = new unsigned long[this->GetTileCount() * theNewWords];
    memset(theWords, 0, this->GetTileCount() * theNewWords * sizeof(unsigned long));
    if (this->mVisibilityWords) {
        unsigned long theIndex = 0;
        for (long y = 0; y < this->mSize; y++) {
            for (long x = 0; x < this->mSize; x++) {
                EETile* theTile = this->GetTile(x, y);
                if (!theTile)
                    continue;
                memcpy(&theWords[theIndex * theNewWords], &this->mVisibilityWords[theIndex * theOldWords], theCopy);
                theTile->mVisibility = &theWords[theIndex * theNewWords];
                theIndex++;
            }
        }
        delete[] this->mVisibilityWords;
    }
    this->mVisibilityWords = theWords;
    this->mU19561C = inPlayerCount;
}

// FUNCTION: EXE 0x51bd55
// Drops a unit placed on the map onto the ground at its current position (camera objects excluded)
void EEMap::PlaceUnitOnGround(EEUnit* inUnit)
{
    EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(inUnit);
    GE3DPoint theLocation;
    if (theUnit->mTileNode != NULL && theUnit->GetUnitClass()->mUnitType != kUnitTypeCameraObject) {
        theUnit->mPhysicalModel.GetLocation().GetPosition(theLocation);
        theUnit->PlaceOnGround(theLocation);
    }
}

// FUNCTION: EXE 0x51bdaa
// Refreshes the terrain type of the four tiles around a vertex, then gives the vertex the water colour set
// when below sea level, else its terrain's colour set
void EEMap::UpdateWater(long inX, long inY)
{
    GETerrainMesh& theMesh = this->mTerrainMesh;
    for (long y = inY - 1; y <= inY; y++) {
        for (long x = inX - 1; x <= inX; x++) {
            EEMapTile* theTile = this->GetTile(x, y);
            if (theTile) {
                EETileLocation theLocation(x, y);
                theTile->SetTerrainType(EETerrainTable::Get(theMesh.GetTextureSetID(x, y))->mTerrainType, theLocation,
                                        this);
            }
        }
    }
    long theX = inX;
    long theY = inY;
    if (!theMesh.GetIsValidTile(inX, inY)) {
        if (inY > this->mL195614 && inX > this->mL195614) {
            theX = inX - 1;
            theY = inY - 1;
        } else if (inX > this->mL195610 && inY <= this->mL195614)
            theX = inX - 1;
        else if (inY > this->mL195610 && inX <= this->mL195614)
            theY = inY - 1;
    }
    if (theMesh.GetZ(inX, inY) < 0.0f)
        theMesh.SetWaterColorSet(inX, inY);
    else {
        long theTextureSet;
        if (theMesh.GetIsValidTile(theX - 1, theY - 1))
            theTextureSet = theMesh.GetTextureSetID(theX - 1, theY - 1);
        else
            theTextureSet = theMesh.GetTextureSetID(theX, theY);
        theMesh.SetColorSet(inX, inY,
                                 EETerrainTypeTable::Get(EETerrainTable::Get(theTextureSet)->mColorType)->mColorSetID);
    }
}

// FUNCTION: EXE 0x51bf4a
// Builds a new map of the given size: fresh unit and effect region tables, every tile on the base terrain
// (the terrain mesh is assigned to itself, as in the original)
void EEMap::CreateMap(long inWidth, long inHeight, unsigned long inPlayerCount)
{
    GETerrainMesh& theMesh = this->mTerrainMesh;
    this->Create(inWidth, inHeight, inPlayerCount, true);
    delete this->mUnitRegions;
    delete this->mEffectRegions;
    this->mUnitRegions = new RTSRegionTable<EEComplexUnit*>(this, 3);
    this->mEffectRegions = new EEEffectsTable(this, 10);
    theMesh = theMesh;
    unsigned long theCount = this->mSize + 1;
    long theTextureSet = EETerrainTable::Get(this->mU195650)->mTextureSetID;
    long theTerrainType = EETerrainTable::Get(this->mU195650)->mTerrainType;
    unsigned long theColorSet = EETerrainTypeTable::Get(EETerrainTable::Get(this->mU195650)->mColorType)->mColorSetID;
    for (unsigned long x = 0; x < theCount; x++) {
        for (unsigned long y = 0; y < theCount; y++) {
            if (this->mTerrainMesh.GetIsValidPoint(x, y))
                this->mTerrainMesh.SetColorSet(x, y, theColorSet);
            if (!this->mTerrainMesh.GetIsValidTile(x, y))
                continue;
            this->SetTileTerrain(x, y, theTextureSet, false);
            EETileLocation theLocation(x, y);
            this->GetTile(x, y)->SetTerrainType(theTerrainType, theLocation, this);
        }
    }
}

// FUNCTION: EXE 0x51c0e6
// Creates an empty map for loading
FSPersistent* EEMap::CreateDefault() const
{
    return new EEMap;
}

// FUNCTION: EXE 0x51c13f
// Takes a player's own or shared sight away from the mesh-flagged tiles within a radius; if the local
// player sees through it, updates the mini map and terrain visibility counts too
// NEAR: the original calls the tile vector's begin() and _Ptrit::operator+ out of line for each theTiles[i]
// (inline budget); ours expands them
void EEMap::RemoveSight(EEPlayer* inPlayer, float inX, float inY, float inRadius, bool inShared)
{
    std::vector<EETileCoordinate> theTiles;
    GETerrainMesh* theMesh = &::gWorld.mMap->mTerrainMesh;
    if (gSightUpdatesEnabled == true) {
        bool theVisible;
        if (inShared) {
            if (::gWorld.mPlayers[::gWorld.mCurrentPlayer] == inPlayer
                || (::gWorld.mPlayers[::gWorld.mCurrentPlayer]->IsAlliedWith(inPlayer)
                    && ::gWorld.mPlayers[::gWorld.mCurrentPlayer]->mSharedLOS && inPlayer->mSharedLOS))
                theVisible = true;
            else
                theVisible = false;
        } else
            theVisible = ::gWorld.mPlayers[::gWorld.mCurrentPlayer] == inPlayer;
        Fn24B212(GE3DPoint(inX, inY, 0.0f), inRadius, theTiles);
        for (unsigned long i = 0, theCount = theTiles.size(); i < theCount; i++) {
            if (!this->mTerrainMesh.GetIsValidTile(theTiles[i].GetX(), theTiles[i].GetY())
                || !this->mTerrainMesh.GetIsWaterTile(theTiles[i].GetX(), theTiles[i].GetY()))
                continue;
            if (inShared == true)
                inPlayer->RemoveSharedSightAt(theTiles[i]);
            else
                inPlayer->RemoveOwnSightAt(theTiles[i]);
            if (theVisible) {
                gMiniMap->CountCellOff(theTiles[i].GetX(), theTiles[i].GetY());
                theMesh->DereferenceTile(theTiles[i].GetX(), theTiles[i].GetY());
            }
        }
    }
}

// FUNCTION: EXE 0x51c3ca
// Sums the tile and corner-height checksums of the next 64 x 64 window and advances the window;
// past the right edge the cursor moves to the start of the next band
long EEMap::ComputeChecksum()
{
    unsigned long theStartX = this->mChecksumCursor % this->mSize;
    unsigned long theStartY = this->mChecksumCursor / this->mSize;
    long theChecksum = 0;
    unsigned long theEndX = theStartX + 0x40;
    unsigned long theEndY = theStartY + 0x40;
    for (unsigned long y = theStartY; y < theEndY; y++) {
        for (unsigned long x = theStartX; x < theEndX; x++) {
            EEMapTile* theTile = this->GetTile(x, y);
            if (!theTile)
                continue;
            theChecksum += theTile->GetChecksum(this->mU19561C);
            theChecksum += (long)(this->mTerrainMesh.GetZ(x, y + 1) * 1000.0f) - (long)(this->mTerrainMesh.GetZ(x + 1, y + 1) * kChecksumScale) -
                           (long)(this->mTerrainMesh.GetZ(x, y) * kChecksumScale) - (long)(this->mTerrainMesh.GetZ(x + 1, y) * kChecksumScale);
        }
    }
    if (theEndX < (unsigned long)this->mSize)
        this->mChecksumCursor += 0x40;
    else {
        // the next band starts at the beginning of row theEndY
        this->mChecksumCursor = theEndY * this->mSize;
        if (this->mChecksumCursor >= (unsigned long)(this->mSize * this->mSize))
            this->mChecksumCursor = 0;
    }
    return theChecksum;
}

// FUNCTION: EXE 0x51c531
// Adds the map's base checksum (the RTSMap part)
void EEMap::GetChecksum(ESChecksum& ioChecksum) const
{
    RTSMap::GetChecksum(ioChecksum);
}

// FUNCTION: EXE 0x51c53d
// Returns the tile under a screen point and its coordinates (in orthographic view, where the ray meets z 0)
EETile* EEMap::GetTileAt(const GE2DPoint& inPoint, long& outX, long& outY)
{
    GE3DLine theLine;
    GE3DPoint thePoint;
    this->GetScreenLine(inPoint, theLine);
    if (gOrthographicView) {
        float theDistance;
        if (!theLine.SolveForZ(0.0f, theDistance, thePoint))
            thePoint = theLine.GetStart();
        this->mTerrainMesh.GetClosestTile(thePoint, outX, outY);
    } else
        this->mTerrainMesh.GetClosestTile(theLine, outX, outY);
    return this->GetTile(outX, outY);
}

// FUNCTION: EXE 0x51c5fb
// Gives the ground point at a tile's centre; returns false when there is no such tile
bool EEMap::GetTileLocation(long inX, long inY, GE3DPoint& outLocation)
{
    if (this->GetTile(inX, inY)) {
        outLocation.SetX(inX + 0.5f);
        outLocation.SetY(inY + 0.5f);
        outLocation.SetZ(this->mTerrainMesh.GetMeshZ(outLocation.GetX(), outLocation.GetY()));
        return true;
    }
    return false;
}

// FUNCTION: EXE 0x51c665
// Builds the 3D line through a screen point from the near to the far clipping plane
void EEMap::GetScreenLine(const GE2DPoint& inPoint, GE3DLine& outLine)
{
    GE3DPoint theNear;
    GE3DPoint theFar;
    float theScale = 1.0f / gViewport.GetHalfWidth();
    GE3DPoint thePoint((inPoint.GetX() - gViewport.GetXCenter()) * theScale,
                       -((inPoint.GetY() - gViewport.GetYCenter()) * theScale), 0.0f);
    GE3DPoint theFarPoint(thePoint.GetX(), thePoint.GetY(), 1.0f);
    if (!gOrthographicView) {
        gViewport.DenormalizePerspectivePoint(thePoint, gViewport.GetFrontClippingDistance(), theNear);
        gViewport.DenormalizePerspectivePoint(theFarPoint, gViewport.GetRearClippingDistance(), theFar);
    } else {
        gViewport.DenormalizeOrthographicPoint(thePoint, theNear);
        gViewport.DenormalizeOrthographicPoint(theFarPoint, theFar);
    }
    outLine.Define(theNear, theFar);
}

// FUNCTION: EXE 0x51c75a
// Returns the ground's pitch under a location, from the ground heights 0.65 ahead of and behind it
float EEMap::GetTerrainPitch(const GETransformation& inLocation)
{
    GE3DPoint theFront = inLocation.GetPosition();
    GE3DPoint theBack = theFront;
    GE3DPoint theFrontGround;
    GE3DPoint theBackGround;
    GEVector theOffset(0.0f, 0.65f, 0.0f);
    GEVector theDirection;
    inLocation.Apply(theOffset, theDirection);
    theFront += theDirection;
    this->GetGroundPoint(theFront, theFrontGround);
    theOffset.mY = -0.65f;
    inLocation.Apply(theOffset, theDirection);
    theBack += theDirection;
    this->GetGroundPoint(theBack, theBackGround);
    return UMArcCos((theFrontGround.GetZ() - theBackGround.GetZ()) * kSlopeScale) - 1.5707964f;
}

// FUNCTION: EXE 0x51c847
// Returns the ground's roll under a location, from the ground heights 0.65 to either side of it
float EEMap::GetTerrainRoll(const GETransformation& inLocation)
{
    GE3DPoint theFront = inLocation.GetPosition();
    GE3DPoint theBack = theFront;
    GE3DPoint theFrontGround;
    GE3DPoint theBackGround;
    GEVector theOffset(0.65f, 0.0f, 0.0f);
    GEVector theDirection;
    inLocation.Apply(theOffset, theDirection);
    theFront += theDirection;
    this->GetGroundPoint(theFront, theFrontGround);
    theOffset.mX = -0.65f;
    inLocation.Apply(theOffset, theDirection);
    theBack += theDirection;
    this->GetGroundPoint(theBack, theBackGround);
    return UMArcCos((theFrontGround.GetZ() - theBackGround.GetZ()) * kSlopeScale) - 1.5707964f;
}

// FUNCTION: EXE 0x51c934
// Returns the tile hit by the ray through a screen point, optionally stopping at the water surface
EETile* EEMap::PickScreenTile(const GE2DPoint& inPoint, long& outX, long& outY, bool inHitWaterSurface)
{
    GE3DLine theLine;
    this->GetScreenLine(inPoint, theLine);
    return this->GetTileOnLine(theLine, outX, outY, inHitWaterSurface);
}

// FUNCTION: EXE 0x51c96d
// Returns the tile and the 3D point hit by the ray through a screen point, optionally at the water surface
EETile* EEMap::GetLocationAt(const GE2DPoint& inPoint, GE3DPoint& outLocation, bool inHitWaterSurface)
{
    GE3DLine theLine;
    this->GetScreenLine(inPoint, theLine);
    return this->GetLocationOnLine(theLine, outLocation, inHitWaterSurface);
}

// FUNCTION: EXE 0x51c9a3
// Resets the map to empty: no tiles or region tables, seeds its random generator, registers terrain sets
void EEMap::Initialize()
{
    this->mU195650 = 0;
    this->mRandom.SetSeed(TMGetTime());
    this->mUnitRegions = NULL;
    this->mEffectRegions = NULL;
    this->mTileArray = NULL;
    this->mVisibilityWords = NULL;
    this->mNextTile = 0;
    this->mTileArrayCount = 0;
    this->mChecksumCursor = 0;
    this->RegisterTerrainSets();
    this->mMapCharacteristics = 0;
    EETerrainTable::Get(this->mTerrainMesh.GetGlobalWaterSetID())->Load(this->mTerrainMesh);
}

// FUNCTION: EXE 0x51ca0d
// Reads the map, its base terrain, characteristics and cliff terrain from the saved game and re-types every
// tile (cliffs and unknown terrain become the base terrain; saves up to 0.203 have no space tiles)
long EEMap::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult;
    EETerrainTable* theDefault;
    if (inMajorVersion == 0 && inMinorVersion <= 0xcb) {
        if (RTSMap::Load(inMajorVersion, inMinorVersion, inFile) == kErrorOK && inFile.Read(this->mU195650) == kErrorOK) {
            if (EETerrainTable::Get(this->mU195650) == NULL)
                this->mU195650 = gDefaultTerrain;
            theDefault = EETerrainTable::Get(this->mU195650);
        } else theDefault = gDefaultTerrain != -1 ? EETerrainTable::Get(gDefaultTerrain) : NULL;
        for (long x = 0; x < this->mSize; x++) {
            for (long y = 0; y < this->mSize; y++) {
                if (!this->mTerrainMesh.GetIsValidTile(x, y))
                    continue;
                EETerrainTable* theTerrain = EETerrainTable::Get(this->mTerrainMesh.GetTextureSetID(x, y));
                if (theTerrain) {
                    long theType = theTerrain->mTerrainType;
                    if (theType != kTerrainCliff) {
                        EETileLocation theLocation(x, y);
                        this->GetTile(x, y)->SetTerrainType(theType, theLocation, this);
                        theTerrain->Load(this->mTerrainMesh);
                    } else {
                        this->SetTileTerrain(x, y, theDefault->mTextureSetID, false);
                        EETileLocation theLocation(x, y);
                        this->GetTile(x, y)->SetTerrainType(theDefault->mTerrainType, theLocation, this);
                    }
                } else {
                    this->SetTileTerrain(x, y, theDefault->mTextureSetID, false);
                    EETileLocation theLocation(x, y);
                    this->GetTile(x, y)->SetTerrainType(theDefault->mTerrainType, theLocation, this);
                }
            }
        }
        theResult = inFile.Read(this->mMapCharacteristics);
        this->mU19565C = gFirstCliffTerrain;
    } else {
        if (RTSMap::Load(inMajorVersion, inMinorVersion, inFile) == kErrorOK && inFile.Read(this->mU195650) == kErrorOK) {
            if (EETerrainTable::Get(this->mU195650) == NULL)
                this->mU195650 = gDefaultTerrain;
            theDefault = EETerrainTable::Get(this->mU195650);
        } else theDefault = gDefaultTerrain != -1 ? EETerrainTable::Get(gDefaultTerrain) : NULL;
        for (long x = 0; x < this->mSize; x++) {
            for (long y = 0; y < this->mSize; y++) {
                if (!this->mTerrainMesh.GetIsValidTile(x, y))
                    continue;
                EETerrainTable* theTerrain = EETerrainTable::Get(this->mTerrainMesh.GetTextureSetID(x, y));
                if (theTerrain) {
                    if (theTerrain->mTerrainType != kTerrainCliff) {
                        if (this->mTerrainMesh.GetIsSpaceTile(x, y)) {
                            EETileLocation theLocation(x, y);
                            this->GetTile(x, y)->SetTerrainType(kTerrainSpace, theLocation, this);
                        } else {
                            long theType = theTerrain->mTerrainType;
                            EETileLocation theLocation(x, y);
                            this->GetTile(x, y)->SetTerrainType(theType, theLocation, this);
                        }
                        theTerrain->Load(this->mTerrainMesh);
                    } else {
                        this->SetTileTerrain(x, y, theDefault->mTextureSetID, false);
                        EETileLocation theLocation(x, y);
                        this->GetTile(x, y)->SetTerrainType(theDefault->mTerrainType, theLocation, this);
                    }
                } else {
                    this->SetTileTerrain(x, y, theDefault->mTextureSetID, false);
                    EETileLocation theLocation(x, y);
                    this->GetTile(x, y)->SetTerrainType(theDefault->mTerrainType, theLocation, this);
                }
            }
        }
        inFile.Read(this->mMapCharacteristics);
        theResult = inFile.Read(this->mU19565C);
        if (this->mU19565C >= EETerrainTable::GetNumCliffTerrains() || EETerrainTable::sCliffTable[this->mU19565C] == NULL)
            this->mU19565C = gFirstCliffTerrain;
    }
    delete this->mUnitRegions;
    delete this->mEffectRegions;
    this->mUnitRegions = new RTSRegionTable<EEComplexUnit*>(this, 3);
    this->mEffectRegions = new EEEffectsTable(this, 10);
    this->UpdateCliffs(0, this->mSize, 0, this->mSize);
    return theResult;
}

// FUNCTION: EXE 0x51ceb0
// Reads the next tile from the saved game (saves up to 0.46 also carry its terrain); returns the tile
EEMapTile* EEMap::LoadTile(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile, long inX,
                            long inY)
{
    long theTerrain = 0;
    long theModel = 0;
    EETile* theTile = &this->mTileArray[this->mNextTile];
    theTile->mVisibility = &this->mVisibilityWords[((this->mU19561C >> 1) + (this->mU19561C & 1)) * this->mNextTile];
    this->mNextTile++;
    if (inMajorVersion == 0 && inMinorVersion <= 0x2e) {
        inFile.Read((unsigned long&)theTerrain);
        inFile.Read((unsigned long&)theModel);
        EETerrainTable* theTable = EETerrainTable::Get(theTerrain);
        if (theTable)
            this->SetTileTerrain(inX, inY, theTable->mTextureSetID, true);
        else
            this->SetTileTerrain(inX, inY, this->mU195650, true);
    }
    theTile->Load(inMajorVersion, inMinorVersion, inFile, this->mU19561C);
    return theTile;
}

// FUNCTION: EXE 0x51cf60
// Writes every tile's state to the out-of-sync dump file
void EEMap::DumpOutOfSynch(FILE* inFile)
{
    fprintf(inFile, "OUT OF SYNCH DUMP: ************************************************* MAP "
                    "*********************************\n\n");
    for (long y = 0; y < this->mSize; y++) {
        for (long x = 0; x < this->mSize; x++) {
            EEMapTile* theTile = this->GetTile(x, y);
            if (!theTile)
                continue;
            fprintf(inFile, "Tile X(%d), Y(%d)\n", x, y);
            theTile->Dump(inFile, this->mU19561C);
            fprintf(inFile, "\n");
        }
    }
}

// FUNCTION: EXE 0x51d006
// Gives a player own or shared sight of the mesh-flagged tiles within a radius; if the local player sees
// through it, updates the mini map and terrain visibility counts too
// NEAR: the original calls the tile vector's begin() and _Ptrit::operator+ out of line for each theTiles[i]
// (inline budget); ours expands them
void EEMap::AddSight(EEPlayer* inPlayer, float inX, float inY, float inRadius, bool inShared)
{
    std::vector<EETileCoordinate> theTiles;
    GETerrainMesh* theMesh = &::gWorld.mMap->mTerrainMesh;
    if (gSightUpdatesEnabled == true) {
        bool theVisible;
        if (inShared) {
            if (::gWorld.mPlayers[::gWorld.mCurrentPlayer] == inPlayer
                || (::gWorld.mPlayers[::gWorld.mCurrentPlayer]->IsAlliedWith(inPlayer)
                    && ::gWorld.mPlayers[::gWorld.mCurrentPlayer]->mSharedLOS && inPlayer->mSharedLOS))
                theVisible = true;
            else
                theVisible = false;
        } else
            theVisible = ::gWorld.mPlayers[::gWorld.mCurrentPlayer] == inPlayer;
        Fn24B212(GE3DPoint(inX, inY, 0.0f), inRadius, theTiles);
        for (unsigned long i = 0, theCount = theTiles.size(); i < theCount; i++) {
            if (!this->mTerrainMesh.GetIsValidTile(theTiles[i].GetX(), theTiles[i].GetY())
                || !this->mTerrainMesh.GetIsWaterTile(theTiles[i].GetX(), theTiles[i].GetY()))
                continue;
            if (inShared == true)
                inPlayer->AddSharedSightAt(theTiles[i]);
            else
                inPlayer->AddOwnSightAt(theTiles[i]);
            if (theVisible) {
                gMiniMap->CountCellOn(theTiles[i].GetX(), theTiles[i].GetY());
                theMesh->ReferenceTile(theTiles[i].GetX(), theTiles[i].GetY());
            }
        }
    }
}

// FUNCTION: EXE 0x51d2a2
// Paints a rectangle of tiles with a terrain, then recolours its vertices with the given colour type
void EEMap::PaintTerrainRect(long inLeft, long inRight, long inTop, long inBottom, long inTerrain, long inCliff)
{
    EETerrainTable* theTerrain = EETerrainTable::Get(inTerrain);
    for (long y = inTop; y >= inBottom; y--) {
        for (long x = inLeft; x <= inRight; x++) {
            EEMapTile* theTile = this->GetTile(x, y);
            if (theTile) {
                this->SetTileTerrain(x, y, theTerrain->mTextureSetID, true);
                EETileLocation theLocation(x, y);
                theTile->SetTerrainType(theTerrain->mTerrainType, theLocation, this);
            }
        }
    }
    this->SetCliffColorSets(inLeft, inRight, inTop, inBottom, inCliff);
}

// FUNCTION: EXE 0x51d34b
// Sets a tile's terrain type and paints it and its lower-left neighbours' textures with the terrain
void EEMap::PaintTerrain(long inX, long inY, long inTerrain)
{
    EETerrainTable* theTerrain = EETerrainTable::Get(inTerrain);
    EEMapTile* theTile = this->GetTile(inX, inY);
    if (theTile) {
        EETileLocation theLocation(inX, inY);
        theTile->SetTerrainType(theTerrain->mTerrainType, theLocation, this);
        long theLeft;
        long x;
        theLeft = inX - 1;
        for (x = inX - 1; x < inX + 1; x++) {
            for (long y = inY - 1; y < inY + 1; y++) {
                if (this->GetTile(x, y))
                    this->SetTileTerrain(x, y, theTerrain->mTextureSetID, true);
            }
        }
        this->SetCliffColorSets(theLeft, inX, inY, inY - 1, theTerrain->mColorType);
    }
}

// FUNCTION: EXE 0x51d433
// Gives the vertices of a tile rectangle that are not under water a colour type's colour set
void EEMap::SetCliffColorSets(long inLeft, long inRight, long inTop, long inBottom, long inCliff)
{
    EETerrainTypeTable* theCliff = EETerrainTypeTable::Get(inCliff);
    for (long y = inTop; y >= inBottom; y--) {
        for (long x = inLeft; x <= inRight; x++) {
            if (!this->GetTile(x, y))
                continue;
            for (unsigned long i = 0; i < 4; i++) {
                if (y == inBottom) {
                    if (x == inLeft) {
                        if (i != 1)
                            continue;
                    } else if (i == 2 || i == 3)
                        continue;
                } else if (x == inLeft && (i == 0 || i == 2))
                    continue;
                switch (i) {
                case 0:
                    if (!(-this->GetF1955F4() > this->mTerrainMesh.GetZ(x, y + 1)))
                        this->mTerrainMesh.SetColorSet(x, y + 1, theCliff->mColorSetID);
                    break;
                case 1:
                    if (!(-this->GetF1955F4() > this->mTerrainMesh.GetZ(x + 1, y + 1)))
                        this->mTerrainMesh.SetColorSet(x + 1, y + 1, theCliff->mColorSetID);
                    break;
                case 2:
                    if (!(-this->GetF1955F4() > this->mTerrainMesh.GetZ(x, y)))
                        this->mTerrainMesh.SetColorSet(x, y, theCliff->mColorSetID);
                    break;
                case 3:
                    if (!(-this->GetF1955F4() > this->mTerrainMesh.GetZ(x + 1, y)))
                        this->mTerrainMesh.SetColorSet(x + 1, y, theCliff->mColorSetID);
                    break;
                }
            }
        }
    }
}

// FUNCTION: EXE 0x51d5e8
// Marks steep tiles in a rectangle as cliffs, texturing them from the cliff terrain by slope direction,
// then makes tiles with three or more cliff neighbours cliffs too
void EEMap::UpdateCliffs(long inLeft, long inRight, long inBottom, long inTop)
{
    long theRandomTerrains[32][32];
    URandomIntegers theRandom;
    GEVector theFacingY(0.0f, -1.0f, 1.0f);
    GEVector theFacingX(1.0f, 0.0f, 1.0f);
    float theCliffHeight = this->GetF1955F4() * 1.5f - 0.0001f;
    theFacingY.Normalize();
    theFacingX.Normalize();
    theRandom.SetSeed(0);
    EECliffTerrainTable* theCliff = EETerrainTable::sCliffTable[this->mU19565C];
    long* theEntry = &theRandomTerrains[0][0];
    for (long i = 0; i < 32; i++) {
        for (long j = 0; j < 32; j++)
            *theEntry++ = theRandom.Generate(0, theCliff->mLastTerrain - theCliff->mFirstTerrain) + theCliff->mFirstTerrain;
    }
    for (unsigned long x = inLeft; x < (unsigned long)inRight; x++) {
        for (unsigned long y = inBottom; y < (unsigned long)inTop; y++) {
            EEMapTile* theTile = this->GetTile(x, y);
            if (!theTile)
                continue;
            float theMin = this->mTerrainMesh.GetZ(x, y);
            float theMax = theMin;
            float theZ = this->mTerrainMesh.GetZ(x, y + 1);
            if (theZ < theMin)
                theMin = theZ;
            if (theZ > theMax)
                theMax = theZ;
            theZ = this->mTerrainMesh.GetZ(x + 1, y);
            if (theZ < theMin)
                theMin = theZ;
            if (theZ > theMax)
                theMax = theZ;
            theZ = this->mTerrainMesh.GetZ(x + 1, y + 1);
            if (theZ < theMin)
                theMin = theZ;
            if (theZ > theMax)
                theMax = theZ;
            if (theMax >= this->GetF1955F4() && theMax - theMin >= theCliffHeight) {
                EETileLocation theLocation(x, y);
                theTile->SetTerrainType(kTerrainCliff, theLocation, this);
                for (long tx = x - 1; tx < (long)x + 1; tx++) {
                    for (long ty = y - 1; ty < (long)y + 1; ty++) {
                        if (!this->GetTile(tx, ty))
                            continue;
                        GEVector theNormal = *this->mTerrainMesh.GetVertexNormal(tx, ty);
                        theNormal += *this->mTerrainMesh.GetVertexNormal(tx + 1, ty);
                        theNormal += *this->mTerrainMesh.GetVertexNormal(tx, ty + 1);
                        theNormal += *this->mTerrainMesh.GetVertexNormal(tx + 1, ty + 1);
                        theNormal.Scale(0.25f, theNormal);
                        long theTerrain;
                        if (1.0f - theFacingY.DotProduct(theNormal) < kCliffNormalTolerance)
                            theTerrain = theCliff->mFirstTerrain;
                        else if (1.0f - theFacingX.DotProduct(theNormal) < kCliffNormalTolerance)
                            theTerrain = theCliff->mLastTerrain;
                        else
                            theTerrain = theRandomTerrains[tx % 32][ty % 32];
                        this->SetTileTerrain(tx, ty, EETerrainTable::Get(theTerrain)->mTextureSetID, true);
                    }
                }
                this->SetCliffColorSets(x - 1, x, y, y - 1, EETerrainTable::Get(theCliff->mFirstTerrain)->mColorType);
            }
        }
    }
    for (unsigned long x2 = inLeft; x2 < (unsigned long)inRight; x2++) {
        for (unsigned long y2 = inBottom; y2 < (unsigned long)inTop; y2++) {
            EEMapTile* theTile = this->GetTile(x2, y2);
            if (!theTile || theTile->mTerrainType == kTerrainCliff)
                continue;
            unsigned long theCount = 0;
            bool theSpace = this->mTerrainMesh.GetIsSpaceTile(x2, y2);
            for (unsigned long n = 0; n < 4; n++) {
                unsigned long nx = x2 - (n & 1);
                unsigned long ny = y2 - (n >> 1);
                if (this->GetTile(nx, ny)) {
                    unsigned long theID = this->mTerrainMesh.GetTextureSetID(nx, ny);
                    for (long k = theCliff->mFirstTerrain; k <= theCliff->mLastTerrain; k++) {
                        if (EETerrainTable::Get(k)->mTextureSetID == (long)theID) {
                            theCount++;
                            break;
                        }
                    }
                } else if (!theSpace)
                    theCount++;
            }
            if (theCount >= 3) {
                EETileLocation theLocation(x2, y2);
                theTile->SetTerrainType(kTerrainCliff, theLocation, this);
            }
        }
    }
}

// FUNCTION: EXE 0x51dbf1
// Turns cliff tiles in a rectangle that are no longer steep back into the map's base terrain
void EEMap::RemoveFlatCliffs(unsigned long inLeft, unsigned long inRight, unsigned long inBottom, unsigned long inTop)
{
    GEVector theFacingX(1.0f, 0.0f, 0.5f);
    GEVector theFacingY(0.0f, -1.0f, 0.5f);
    float theFlat = this->GetF1955F4() * 1.5f;
    theFacingY.Normalize();
    theFacingX.Normalize();
    for (unsigned long x = inLeft; x < inRight; x++) {
        for (unsigned long y = inBottom; y < inTop; y++) {
            EEMapTile* theTile = this->GetTile(x, y);
            if (!theTile)
                continue;
            float theMin = this->mTerrainMesh.GetZ(x, y);
            float theMax = theMin;
            float theZ = this->mTerrainMesh.GetZ(x, y + 1);
            if (theZ < theMin)
                theMin = theZ;
            if (theZ > theMax)
                theMax = theZ;
            theZ = this->mTerrainMesh.GetZ(x + 1, y);
            if (theZ < theMin)
                theMin = theZ;
            if (theZ > theMax)
                theMax = theZ;
            theZ = this->mTerrainMesh.GetZ(x + 1, y + 1);
            if (theZ < theMin)
                theMin = theZ;
            if (theZ > theMax)
                theMax = theZ;
            if (!(theMax - theMin < theFlat) || theTile->mTerrainType != kTerrainCliff)
                continue;
            this->PaintTerrain(x, y, this->mU195650);
            EETileLocation theLocation(x, y);
            theTile->SetTerrainType(EETerrainTable::Get(this->mU195650)->mTerrainType, theLocation, this);
        }
    }
}

// FUNCTION: EXE 0x51dd95
// Registers every terrain's texture set and every terrain type's colour set with the terrain mesh, then
// the water state and the colour table
void EEMap::RegisterTerrainSets()
{
    std::vector<GEColor> theColors;
    std::vector<unsigned long> theColorSet;
    GETerrainMesh& theMesh = this->mTerrainMesh;
    unsigned long theWaterSet = 0;
    unsigned long theID;
    for (unsigned long i = 0; i < EETerrainTable::sTable.size(); i++) {
        EETerrainTable* theTerrain = EETerrainTable::Get(i);
        if (theTerrain) {
            GETextureSet* theSet = new GETextureSet(theTerrain->mColumns, theTerrain->mRows, theTerrain->mTextureTileCoverage);
            theMesh.RegisterTextureSet(theSet, theID);
            if (theTerrain->mIsWater == true)
                theWaterSet = theID;
            theTerrain->AttachTextures(theSet);
            theTerrain->mTextureSetID = theID;
        } else
            theMesh.RegisterTextureSet(NULL, theID);
    }
    for (unsigned long j = 0; j < EETerrainTypeTable::GetCount(); j++) {
        EETerrainTypeTable* theType = EETerrainTypeTable::Get(j);
        if (!theType)
            continue;
        theColorSet.clear();
        for (unsigned long k = theType->mFirstColor; (long)k <= theType->mLastColor; k++)
            theColorSet.push_back(k);
        theMesh.RegisterColorSet(theColorSet, theID);
        theType->mColorSetID = theID;
    }
    for (unsigned long m = 0; m < EETerrainColorTable::GetCount(); m++) {
        EETerrainColorTable* theColor = EETerrainColorTable::Get(m);
        if (theColor)
            theColors.push_back(theColor->mColor);
    }
    EETerrainTable* theWater = EETerrainTable::Get(theWaterSet);
    theMesh.SetGlobalWaterState(theWaterSet, EETerrainTypeTable::Get(theWater->mColorType)->mColorSetID, theWater->mWaterAnimationPeriod,
                                -(this->GetF1955F4() * theWater->mWaterDepth));
    theMesh.SetColorTable(theColors);
}

// FUNCTION: EXE 0x51df3f
// Writes the map, its base terrain, characteristics and cliff terrain to the saved game; returns the first error
long EEMap::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = RTSMap::Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mU195650);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mMapCharacteristics);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mU19565C);
    return theResult;
}

// FUNCTION: EXE 0x51df99
// Switches the map to another cliff terrain and re-runs cliff detection over the whole map
void EEMap::SetCliffTexture(long inCliffTexture)
{
    this->mU19565C = inCliffTexture;
    this->UpdateCliffs(0, this->mSize, 0, this->mSize);
}

// FUNCTION: EXE 0x51dfb7
// Sets a tile's texture set, loading the new terrain's textures and optionally unloading the old one's
void EEMap::SetTileTerrain(long inX, long inY, long inTextureSet, bool inUnload)
{
    if (inUnload == true)
        EETerrainTable::Get(this->mTerrainMesh.GetTextureSetID(inX, inY))->Unload(this->mTerrainMesh);
    EETerrainTable::Get(inTextureSet)->Load(this->mTerrainMesh);
    this->mTerrainMesh.SetTextureSet(inX, inY, EETerrainTable::Get(inTextureSet)->mTextureSetID);
}

// TEMPLATE: EXE 0x51e032
// (vector<GEColor>::push_back; the inline and template COMDATs close the unit)

// TEMPLATE: EXE 0x51e05e
// (RTSRegionTable<EEComplexUnit*>::RTSRegionTable)

// SYNTHETIC: EXE 0x51e0fe
// (RTSRegionTable<EEComplexUnit*>'s scalar deleting destructor)

// TEMPLATE: EXE 0x51e11a
// (RTSRegionTable<EEComplexUnit*>::~RTSRegionTable)

// FUNCTION: EXE 0x51e12f
// Returns the number of tiles: the square map minus its two cut-off corner triangles
unsigned long EEMap::GetTileCount()
{
    long theWidth = this->mL195614 - 1;
    long theHeight = this->mL195610 - 1;
    long theSize = this->mSize;
    return theSize * theSize - 2 * ((long)((theHeight + 1) * theHeight * 0.5f)
                                    + (long)((theWidth + 1) * theWidth * 0.5f));
}

// SYNTHETIC: EXE 0x51e18a
// (FSPersistent-base thunk of GetChecksum)
// SYNTHETIC: EXE 0x51e195
// (FSPersistent-base thunk of Load)
// SYNTHETIC: EXE 0x51e1a0
// (FSPersistent-base thunk of Save)
// SYNTHETIC: EXE 0x51e1ab
// (FSPersistent-base thunk of the scalar deleting destructor)

// SYNTHETIC: EXE 0x51e1b6
// (EEEffectsTable's scalar deleting destructor)

// FUNCTION: EXE 0x51e1d8
// Returns the number of cliff terrains
// (EETerrainTable::GetNumCliffTerrains, inline in ee/EEDatabaseTables.h: its out-of-line copy)

// FUNCTION: EXE 0x51e1f0
// Returns the number of terrain colours
// (EETerrainColorTable::GetCount, inline in ee/EETerrainColorTable.h: its out-of-line copy)

// FUNCTION: EXE 0x51e208
// Returns a terrain colour by index
// (EETerrainColorTable::Get, inline in ee/EETerrainColorTable.h: its out-of-line copy)

// FUNCTION: EXE 0x51e22b
// Returns a terrain type by index
// (EETerrainTypeTable::Get, inline in ee/EEDatabaseTables.h: its out-of-line copy)

// FUNCTION: EXE 0x51e24e
// Returns the number of terrain types
// (EETerrainTypeTable::GetCount, inline in ee/EEDatabaseTables.h: its out-of-line copy)

// FUNCTION: EXE 0x51e266
// Creates an empty tile
// (EETile::EETile, inline in ee/EEMap.h: its out-of-line copy, first called by Virtual1C's new[])

// TEMPLATE: EXE 0x51e290
// (RTSRegionTable<EEEffect*>::~RTSRegionTable)

// TEMPLATE: EXE 0x51e2a5
// (vector<EECliffTerrainTable*>::operator[], ICF-folded with vector<long>'s)

// SYNTHETIC: EXE 0x51e2b2
// (RTSRegionTable<EEEffect*>'s scalar deleting destructor)

// TEMPLATE: EXE 0x51e2ce
// (RTSRegionTable<EEEffect*>::RTSRegionTable)

// FUNCTION: EXE 0x51c10d
// Returns whether the other player is this player or neither of the two is at war with the other
// (out of line after its first user here)
bool EEPlayer::IsAlliedWith(EEPlayer* inPlayer)
{
    return inPlayer == this || this->mEnemies[inPlayer->mPlayerID] == 0 && inPlayer->mEnemies[this->mPlayerID] == 0;
}
