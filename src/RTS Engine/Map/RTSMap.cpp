// C:\EEX\RTS Engine\Map\RTSMap.cpp (__FILE__ 0x4befc8). The engine-side map: 0x121c78..0x1240b3 (the three
// "RTSMap-..." key chains, the ctor, ??_G, the dtor and the members in definition order, then the template COMDATs of
// vector<RTSMapVertexColors> and RTSTile::SetVisibility's out-of-line copy). Unit flags: the exe's module set.
#include <windows.h>
#include <math.h>
#include <vector>
#include <utility>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "lle/GE3DLine.h"
#include "lle/U2DPoint.h"
#include "lle/U2DRectangle.h"
#include "lle/FSFile.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEMap.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEDoppelgangerManager.h"
#include "ee/EEGlobalConstants.h"
#include "ee/EEAIKindIDs.h"
#include "rust/render/ge.h"

// chain at 0x521c78 (wrapper)
//   object 0x929840  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ (guard 0x929838 bit 1)
const UString RTSMap::sMapWidth("RTSMap-MapWidth");

// chain at 0x521cbb (wrapper)
//   object 0x929828  (guard 0x929838 bit 2)
const UString RTSMap::sMapHeight("RTSMap-MapHeight");

// chain at 0x521cfe (wrapper)
//   object 0x929818  (guard 0x929838 bit 4)
const UString RTSMap::sVertexColorIndex("RTSMap-VertexColorIndex");

// Creates an empty map with the given scale value (also copied to gMapTileSize) and its colored-rectangle lock
RTSMap::RTSMap(float inF1955F4)
{
    this->mF1955F4 = inF1955F4;   // (the record EeUnitsRtsMapFields' members: no member initializers)
    this->mF1955F8 = 0.0f;
    this->mF1955FC = 0.0f;
    this->mB195620 = false;
    this->mL1955EC = 0;
    this->mTiles = NULL;
    this->mL195600 = 0;
    this->mL195604 = 0;
    this->mL195610 = 0;
    this->mL195614 = 0;
    this->mSize = 0;
    this->mL195608 = 0;
    this->mL19560C = 0;
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mCriticalSectionForColoredRectangles) == keseOK, 149);
    gMapTileSize = this->GetF1955F4();
}

// Adds nothing to the sync checksum (the engine map's empty override)
// (vtable 0x841d94 slot 0: an out-of-line empty override; EEMap::GetChecksum 0x51c531 calls it)
void RTSMap::GetChecksum(ESChecksum& ioChecksum) const
{
}

// Destroys the map, freeing its tile table and its colored-rectangle lock
RTSMap::~RTSMap()
{
    this->DeleteTiles();
    TSScheduler::DestroyCriticalSection(this->mCriticalSectionForColoredRectangles);
}

// The terrain edits (Raise, its spreads and smoothing, the tiles' upkeep, GetNumRaisedCorners, IsTileFlat) are ee_ge
// map_raise.rs' over the mesh's Rust object; the map's tiles and water are asked here
namespace {
struct EEMapRaiseOps {
    static bool HasTile(void* inMap, unsigned long inX, unsigned long inY)
    {
        return static_cast<RTSMap*>(inMap)->GetTile(inX, inY) != NULL;
    }
    // the tile's height, then each of its units back on the ground (UpdateTiles 0x1225fa)
    static void SettleTile(void* inMap, unsigned long inX, unsigned long inY, float inHeight)
    {
        RTSMap* theMap = static_cast<RTSMap*>(inMap);
        EEMapTile* theTile = theMap->GetTile(inX, inY);
        theTile->mHeight = inHeight;
        for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL; theNode = theNode->mNext)
            theMap->PlaceUnitOnGround(theNode->mUnit);
    }
    static void UpdateWater(void* inMap, long inX, long inY) { static_cast<RTSMap*>(inMap)->UpdateWater(inX, inY); }
    static EeGeMapHost Of(RTSMap* inMap)
    {
        EeGeMapHost theHost = {inMap, inMap->mTerrainMesh.GetRust(), HasTile, SettleTile, UpdateWater};
        return theHost;
    }
};
}

// Counts the tile's corners at least 0.4 above its lowest corner (0 off the map)
long RTSMap::GetNumRaisedCorners(unsigned long inX, unsigned long inY)
{
    EeGeMapHost theHost = EEMapRaiseOps::Of(this);
    return ee_ge_map_raised_corners(&theHost, inX, inY);
}

// Turns every explored but unseen tile back to unexplored for each player but player 0, then unregisters
// those players from the doppelganger manager (done before a scenario is saved)
void EEMap::PrepareSave()
{
    EESightScope theCapture(kSightPrepareSave, NULL);   // TSGT (TEST ONLY)
    ee_units_tile_sight_map((uint64_t)(uintptr_t)&::gWorld, (uint64_t)(uintptr_t)this, 2, 0);   // ee_units tile_sight.rs
    for (unsigned long i = 1; i < this->mU19561C; i++)
        gDoppelgangerManager.UnregisterPlayer(::gWorld.mPlayers[i]);
}

// Empties the map's area-outline tile runs
void RTSMap::ClearU1955DC()
{
    this->mU1955DC.clear();
}

// NEAR: only the load of inPlayerCount is scheduled differently (the original loads it with inWidth).
// Sets up a diamond-shaped map of the given width and height for the player count, optionally creating its
// tiles, and builds a flat terrain mesh
void RTSMap::Create(long inWidth, long inHeight, unsigned long inPlayerCount, bool inCreateTiles)
{
    long theRight = inHeight;
    long theLeft = inHeight - 1;
    bool theLeftShrinking = true;
    bool theRightGrowing = true;
    if (this->mB195620)
        this->DeleteTiles();
    this->mL195608 = inHeight - 1;
    this->mL195614 = inWidth;
    this->mSize = inWidth + inHeight;
    this->mL195610 = inHeight;
    this->mU19561C = inPlayerCount;
    this->mL19560C = this->mSize + inWidth - 1;
    this->mB195620 = true;
    this->mF1955F8 = 84.0f;
    this->mF1955FC = 42.0f;
    long theNumTiles = this->mSize * this->mSize;
    this->ReserveTiles(theNumTiles);
    this->mTiles = new EEMapTile*[theNumTiles];
    for (long theY = 0; theY < this->mSize; theY++) {
        if (inCreateTiles) {
            for (long theX = 0; theX < this->mSize; theX++) {
                EEMapTile* theTile;
                theTile = theX >= theLeft && theX < theRight + 1 ? this->CreateTile(0.0f) : NULL;
                this->mTiles[this->mSize * theY + theX] = theTile;
            }
        }
        if (theLeftShrinking) {
            if (theLeft != 0)
                theLeft--;
            else
                theLeftShrinking = false;
        } else
            theLeft++;
        if (theRightGrowing) {
            theRight++;
            if (theRight == this->mSize) {
                theRight = this->mSize - 1;
                theRightGrowing = false;
            }
        } else
            theRight--;
    }
    this->mTerrainMesh.CreateFlatMesh(inWidth, inHeight);
}

// NEAR: inline budget: the original refuses every _Ptrit::operator+ except the innermost mIndices site.
// Paints the queued vertex-color rectangles onto the terrain mesh (newest first) and empties the queue
void RTSMap::ApplyVertexColors()
{
    for (long i = this->mVertexColors.size() - 1; i >= 0; i--) {
        long theWidth = this->mVertexColors[i].mRight - this->mVertexColors[i].mLeft;
        long theOffset = 0;
        for (long theY = this->mVertexColors[i].mBottom; theY < this->mVertexColors[i].mTop; theY++) {
            long k = 0;
            for (long theX = this->mVertexColors[i].mLeft; theX < this->mVertexColors[i].mRight; theX++, k++) {
                if (this->mTerrainMesh.GetIsValidPoint(theX, theY))
                    this->mTerrainMesh.SetColorFromColorTableIndex(theX, theY, this->mVertexColors[i].mIndices[theOffset + k]);
            }
            theOffset += theWidth;
        }
    }
    this->mVertexColors.clear();
}

// Returns the tile closest to a point and stores its coordinates
EEMapTile* EEMap::GetClosestTile(GE3DPoint* inPoint, long* ioX, long* ioY)
{
    this->mTerrainMesh.GetClosestTile(*inPoint, *ioX, *ioY);
    return this->GetTile(*ioX, *ioY);
}

// Returns whether a tile's corner heights differ by at most 0.7 times the map's scale value (false off the map)
bool RTSMap::IsTileFlat(unsigned long inX, unsigned long inY)
{
    EeGeMapHost theHost = EEMapRaiseOps::Of(this);
    return ee_ge_map_tile_flat(&theHost, this->mF1955F4, inX, inY);
}

// Returns whether the tiles a radius's diagonal away left, right, below and above a tile are all on the map
bool EEMap::IsAreaOnMap(unsigned long inX, unsigned long inY, unsigned long inRadius)
{
    float theDiagonal = sqrt((inRadius * inRadius) * 2.0);
    unsigned long theRadius = UMFloorUnsigned(theDiagonal + 0.5f);
    bool theResult = false;
    if (this->GetTile(inX - theRadius, inY) != NULL && this->GetTile(inX + theRadius, inY) != NULL &&
        this->GetTile(inX, inY - theRadius) != NULL && this->GetTile(inX, inY + theRadius) != NULL)
        theResult = true;
    return theResult;
}

// Returns the tile where a line meets the terrain, or the water surface if asked and it comes first,
// and stores its coordinates; NULL if nothing is hit
EEMapTile* RTSMap::GetTileOnLine(const GE3DLine& inLine, long& outX, long& outY, bool inHitWaterSurface)
{
    bool theFound = false;
    GE3DPoint thePoint;
    float theT = 1.0f;
    EEMapTile* theTile = NULL;
    if (inHitWaterSurface) {
        if (inLine.SolveForZ(0.0f, theT) && theT >= 0.0f && theT <= 1.0f) {
            theFound = true;
            inLine.GetPointAt(theT, thePoint);
            long theX = UMFloor(thePoint.GetX());
            long theY = UMFloor(thePoint.GetY());
            theTile = this->GetTile(theX, theY);
            if (theTile != NULL) {
                outX = theX;
                outY = theY;
            } else
                theFound = false;
        } else
            theFound = false;
    }
    float theDistance;
    if (!this->mTerrainMesh.Intersects(inLine, theDistance))
        return theTile;
    if (!inHitWaterSurface || !theFound || theDistance < theT) {
        inLine.GetPointAt(theDistance, thePoint);
        outX = UMFloor(thePoint.GetX());
        outY = UMFloor(thePoint.GetY());
        theTile = this->GetTile(outX, outY);
    }
    return theTile;
}

// Returns the tile where a line meets the terrain, or the water surface if asked and it comes first,
// and stores the hit point; NULL if nothing is hit
EEMapTile* RTSMap::GetLocationOnLine(const GE3DLine& inLine, GE3DPoint& outLocation, bool inHitWaterSurface)
{
    GE3DPoint thePoint;
    EEMapTile* theTile = NULL;
    float theT = 1.0f;
    bool theFound = false;
    if (inHitWaterSurface) {
        if (inLine.SolveForZ(0.0f, theT) && theT >= 0.0f && theT <= 1.0f) {
            theFound = true;
            inLine.GetPointAt(theT, thePoint);
            theTile = this->GetTile(UMFloor(thePoint.GetX()), UMFloor(thePoint.GetY()));
            if (theTile != NULL)
                outLocation = thePoint;
            else
                theFound = false;
        } else
            theFound = false;
    }
    float theDistance;
    if (!this->mTerrainMesh.Intersects(inLine, theDistance))
        return theTile;
    if (!inHitWaterSurface || !theFound || theDistance < theT) {
        inLine.GetPointAt(theDistance, outLocation);
        theTile = this->GetTile(UMFloor(outLocation.GetX()), UMFloor(outLocation.GetY()));
    }
    return theTile;
}

// Returns the color of one of a tile's four corner vertices
const GEColor& RTSMap::GetVertexColor(long inX, long inY, long inCorner)
{
    switch (inCorner) {
    case 0:
        inY++;
        break;
    case 1:
        inY++;
        inX++;
        break;
    case 3:
        inX++;
        break;
    }
    return this->mTerrainMesh.GetVertexColor(inX, inY);
}

// Reads the map's size and player count, recreates the map and loads its tiles and terrain mesh; returns the
// first read error (tiles get x = row start + column, which only old saves' terrain lookup uses)
long RTSMap::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEMapSaveContext theContext = {this, &inFile, inMajorVersion, inMinorVersion, NULL, NULL};   // port: ee_units map_save.rs
    return ee_units_rts_map_load(&theContext, &EEMapSaveOperation, static_cast<EeUnitsRtsMapFields*>(this));
}

// Raises or smooths the terrain of a rectangle, spreads its edges, recomputes normals and shorelines and
// returns the changed rectangle through the in/out bounds
void RTSMap::Raise(long* ioLeft, long* ioRight, long* ioTop, long* ioBottom, float inHeight, float inSlope,
                   float inEdgeHeight, float inEdgeSlope, bool inRaise)
{
    EeGeMapHost theHost = EEMapRaiseOps::Of(this);
    long theBounds[4] = {*ioLeft, *ioRight, *ioTop, *ioBottom};
    ee_ge_map_raise(&theHost, this, &theBounds, inHeight, inSlope, inEdgeHeight, inEdgeSlope, inRaise);
    *ioLeft = theBounds[0];
    *ioRight = theBounds[1];
    *ioTop = theBounds[2];
    *ioBottom = theBounds[3];
}

// Frees the map's tile pointer table (not the tiles)
void RTSMap::DeleteTiles()
{
    delete[] this->mTiles;
}

// Fogs every tile the player has seen or explored: it becomes explored but not in sight
void RTSMap::FogSeen(EEPlayer* inPlayer)
{
    EESightScope theCapture(kSightFogSeen, inPlayer);   // TSGT (TEST ONLY)
    ee_units_tile_sight_map((uint64_t)(uintptr_t)&::gWorld, (uint64_t)(uintptr_t)this, 0, (uint64_t)(uintptr_t)inPlayer);
}

// Marks every unexplored tile as explored for the player (reveals the map)
void RTSMap::ExploreUnseen(EEPlayer* inPlayer)
{
    EESightScope theCapture(kSightExploreUnseen, inPlayer);   // TSGT (TEST ONLY)
    ee_units_tile_sight_map((uint64_t)(uintptr_t)&::gWorld, (uint64_t)(uintptr_t)this, 1, (uint64_t)(uintptr_t)inPlayer);
}

// Writes the map's size, player count, every tile and the terrain mesh; returns the first write error
long RTSMap::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEMapSaveContext theContext = {this, &outFile, inMajorVersion, inMinorVersion, NULL, NULL};   // port: ee_units map_save.rs
    return ee_units_rts_map_save(&theContext, &EEMapSaveOperation, static_cast<EeUnitsRtsMapFields*>(this));
}

// NEAR: inline budget (begin() refused in the prologue, Contains/operator+ out of line) and the
// by-value point copies: the original copies the points with the out-of-line copy ctor 0x4d28b (479 callers in
// the exe), so the exe's U2DPoint<long> probably has the user-declared copy ctor that U2DPoint.h keeps engine-only.
// Rebuilds the outline runs of continent areas: every rectangle edge not covered by another area of the same
// continent
void RTSMap::BuildAreaOutlines(std::vector<U2DRectangle<long> >& inAreas, std::vector<unsigned short>& inContinents)
{
    U2DPoint<long> theStart;
    U2DPoint<long> theEnd;
    unsigned long theCount = inAreas.size();
    this->ClearU1955DC();
    for (unsigned long i = 0; i < theCount; i++) {
        long theLeft = inAreas[i].mLeft;
        long theRight = inAreas[i].mRight;
        long theTop = inAreas[i].mTop;
        long theBottom = inAreas[i].mBottom;
        unsigned short theContinent = inContinents[i];
        unsigned long j;
        theStart.mY = theTop;
        theEnd.mY = theTop;
        long theX; for (theX = theLeft; theX < theRight; theX++) {
            theStart.mX = theX;
            theEnd.mX = theX + 1;
            for (j = 0; j < theCount; j++) {
                if (j != i && inContinents[j] == theContinent && inAreas[j].Contains(theStart) &&
                    inAreas[j].Contains(theEnd) && inAreas[j].mTop != theTop)
                    break;
            }
            if (j == theCount)
                this->AddOutlineEdge(theStart, theEnd, inContinents[i]);
        }
        theStart.mY = theBottom;
        theEnd.mY = theBottom;
        for (theX = theLeft; theX < theRight; theX++) {
            theStart.mX = theX;
            theEnd.mX = theX + 1;
            for (j = 0; j < theCount; j++) {
                if (j != i && inContinents[j] == theContinent && inAreas[j].Contains(theStart) &&
                    inAreas[j].Contains(theEnd) && inAreas[j].mBottom != theBottom)
                    break;
            }
            if (j == theCount)
                this->AddOutlineEdge(theStart, theEnd, inContinents[i]);
        }
        theStart.mX = theLeft;
        theEnd.mX = theLeft;
        long theY; for (theY = theTop; theY < theBottom; theY++) {
            theStart.mY = theY;
            theEnd.mY = theY + 1;
            for (j = 0; j < theCount; j++) {
                if (j != i && inContinents[j] == theContinent && inAreas[j].Contains(theStart) &&
                    inAreas[j].Contains(theEnd) && inAreas[j].mLeft != theLeft)
                    break;
            }
            if (j == theCount)
                this->AddOutlineEdge(theStart, theEnd, inContinents[i]);
        }
        theStart.mX = theRight;
        theEnd.mX = theRight;
        for (theY = theTop; theY < theBottom; theY++) {
            theStart.mY = theY;
            theEnd.mY = theY + 1;
            for (j = 0; j < theCount; j++) {
                if (j != i && inContinents[j] == theContinent && inAreas[j].Contains(theStart) &&
                    inAreas[j].Contains(theEnd) && inAreas[j].mRight != theRight)
                    break;
            }
            if (j == theCount)
                this->AddOutlineEdge(theStart, theEnd, inContinents[i]);
        }
    }
}

// NEAR: which of the run start's coordinates sits in ebx (the original: y, from inStart.mY directly).
// Adds an outline edge as one tile run; on space maps splits it into the runs whose tiles lie on the continent
void RTSMap::AddOutlineEdge(U2DPoint<long> inStart, U2DPoint<long> inEnd, unsigned short inContinent)
{
    if (!this->mTerrainMesh.GetIsSpaceMap()) {
        RTSMapTileRun theRun(inStart, inEnd);
        this->mU1955DC.push_back(theRun);
        return;
    }
    long theX = inStart.mX;
    long theY = inStart.mY;
    long theRunY = theY;
    long theRunX = theX;
    long theDX = 0;
    long theDY = 0;
    long theCount;
    if (inStart.mX == inEnd.mX) {
        theCount = inEnd.mY - inStart.mY;
        theDY = 1;
    } else {
        theCount = inEnd.mX - inStart.mX;
        theDX = 1;
    }
    bool theInRun = false;
    for (long i = 0; i <= theCount; i++) {
        EETileCoordinate theTile;
        theTile.mX = theX;
        theTile.mY = theY;
        unsigned short theContinent = gContinentManager.GetContinent(theTile, kAIPathNonSpace);
        if (!theInRun) {
            if (theContinent == inContinent) {
                theRunX = theX;
                theRunY = theY;
                theInRun = true;
            }
        } else if (theContinent != inContinent) {
            RTSMapTileRun theRun(U2DPoint<long>(theRunX, theRunY), U2DPoint<long>(theX, theY));
            this->mU1955DC.push_back(theRun);
            theInRun = false;
        }
        theX += theDX;
        theY += theDY;
    }
    if (theInRun) {
        RTSMapTileRun theRun(U2DPoint<long>(theRunX, theRunY), U2DPoint<long>(theX - theDX, theY - theDY));
        this->mU1955DC.push_back(theRun);
    }
}
