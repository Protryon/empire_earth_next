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

#include "ee/EETileLocation.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEMap.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEDoppelgangerManager.h"
#include "ee/EEGlobalConstants.h"
#include "ee/EEAIKindIDs.h"
// unit-local constants (.rdata next to RTSMap's vtable 0x441d94: 0x441d8c before it, 0x441dbc after it)
const float kFlatSlope = 0.7f;              // 0x441d8c (IsTileFlat)
const float kNegativeHeightEpsilon = -0.01f;   // 0x441dbc (SmoothVertex)

// chain at 0x521c78 (wrapper)
//   object 0x929840  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ (guard 0x929838 bit 1)
const UString RTSMap::sMapWidth("RTSMap-MapWidth");
// SYNTHETIC: EXE 0x521c78   (wrapper)
// SYNTHETIC: EXE 0x521c82   (ctor)
// SYNTHETIC: EXE 0x521c93   (registrar)
// SYNTHETIC: EXE 0x521c9f   (dtor)

// chain at 0x521cbb (wrapper)
//   object 0x929828  (guard 0x929838 bit 2)
const UString RTSMap::sMapHeight("RTSMap-MapHeight");
// SYNTHETIC: EXE 0x521cbb   (wrapper)
// SYNTHETIC: EXE 0x521cc5   (ctor)
// SYNTHETIC: EXE 0x521cd6   (registrar)
// SYNTHETIC: EXE 0x521ce2   (dtor)

// chain at 0x521cfe (wrapper)
//   object 0x929818  (guard 0x929838 bit 4)
const UString RTSMap::sVertexColorIndex("RTSMap-VertexColorIndex");
// SYNTHETIC: EXE 0x521cfe   (wrapper)
// SYNTHETIC: EXE 0x521d08   (ctor)
// SYNTHETIC: EXE 0x521d19   (registrar)
// SYNTHETIC: EXE 0x521d25   (dtor)

// FUNCTION: EXE 0x521d41
// Creates an empty map with the given scale value (also copied to gF52988C) and its colored-rectangle lock
RTSMap::RTSMap(float inF1955F4)
    : mF1955F4(inF1955F4), mF1955F8(0.0f), mF1955FC(0.0f), mB195620(false)
{
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
    gF52988C = this->GetF1955F4();
}

// ICF-FOLDED: EXE 0x557269
// Adds nothing to the sync checksum (the engine map's empty override)
// (vtable 0x841d94 slot 0: an out-of-line empty override; EEMap::GetChecksum 0x51c531 calls it)
void RTSMap::GetChecksum(ESChecksum& ioChecksum) const
{
}

// SYNTHETIC: EXE 0x521e20
// (the scalar deleting destructor, emitted with the vtable; slot RTSMap+0x10)

// FUNCTION: EXE 0x521e3c
// Destroys the map, freeing its tile table and its colored-rectangle lock
RTSMap::~RTSMap()
{
    this->DeleteTiles();
    TSScheduler::DestroyCriticalSection(this->mCriticalSectionForColoredRectangles);
}

// FUNCTION: EXE 0x521e7d
// Smooths the slope below a raised area ring by ring until nothing changes; returns the lowest row reached
// and keeps the deepest ring that moved a corner vertex for SpreadLeft/SpreadRight
long RTSMap::SpreadDown(long inX, long inY, long inEnd, float inHeight, float inSlope)
{
    float theHeight = inHeight;
    float theSteps = inHeight / (inHeight * inSlope);
    unsigned long theCount = UMFloorUnsigned(theSteps);
    long theRing = 0;
    unsigned long theStep = 1;
    bool theChanged;
    do {
        theChanged = false;
        theRing++;
        if (theStep == theCount)
            inHeight = theHeight;
        else {
            inHeight = theStep * theHeight * inSlope;
            theStep++;
        }
        long theLeft = inX - theRing;
        long theRight = inY + theRing + 1;
        long theY = inEnd - theRing;
        for (long theX = theLeft; theX <= theRight; theX++) {
            if (theX == theLeft) {
                if (this->SmoothVertex(theX, theY, theX + 1, theY + 1, inHeight)) {
                    if (theRing > this->mL195600)
                        this->mL195600 = theRing;
                    theChanged = true;
                }
            } else if (theX == theRight) {
                if (this->SmoothVertex(theX, theY, theX - 1, theY + 1, inHeight)) {
                    if (theRing > this->mL195604)
                        this->mL195604 = theRing;
                    theChanged = true;
                }
            } else if (this->SmoothVertex(theX, theY, theX, theY + 1, inHeight))
                theChanged = true;
        }
    } while (theChanged);
    return inEnd - theRing;
}

// FUNCTION: EXE 0x521fa8
// Smooths the slope above a raised area ring by ring until nothing changes; returns the highest row reached
// and keeps the last ring that moved a corner vertex (overwritten, not maxed as in SpreadDown)
long RTSMap::SpreadUp(long inX, long inY, long inEnd, float inHeight, float inSlope)
{
    float theHeight = inHeight;
    float theSteps = inHeight / (inHeight * inSlope);
    unsigned long theCount = UMFloorUnsigned(theSteps);
    long theRing = 0;
    unsigned long theStep = 1;
    bool theChanged;
    do {
        theChanged = false;
        theRing++;
        if (theStep == theCount)
            inHeight = theHeight;
        else {
            inHeight = theStep * theHeight * inSlope;
            theStep++;
        }
        long theLeft = inX - theRing;
        long theRight = inY + theRing + 1;
        long theY = inEnd + theRing + 1;
        for (long theX = theLeft; theX <= theRight; theX++) {
            if (theX == theLeft) {
                if (this->SmoothVertex(theX, theY, theX + 1, theY - 1, inHeight)) {
                    this->mL195600 = theRing;
                    theChanged = true;
                }
            } else if (theX == theRight) {
                if (this->SmoothVertex(theX, theY, theX - 1, theY - 1, inHeight)) {
                    this->mL195604 = theRing;
                    theChanged = true;
                }
            } else if (this->SmoothVertex(theX, theY, theX, theY - 1, inHeight))
                theChanged = true;
        }
    } while (theChanged);
    return theRing + inEnd;
}

// NEAR: register choice only; the original keeps inX in edx through the loop (reloaded after the calls)
// and the ring count in eax, and reloads `this` before each call instead of after it.
// FUNCTION: EXE 0x5220d6
// Smooths the slope left of a raised area until nothing changes and the corner rings are covered;
// returns the leftmost column reached
long RTSMap::SpreadLeft(long inX, long inY, long inEnd, float inHeight, float inSlope)
{
    float theHeight = inHeight;
    float theSteps = inHeight / (inHeight * inSlope);
    unsigned long theCount = UMFloorUnsigned(theSteps);
    long theRing = 0;
    unsigned long theStep = 1;
    bool theChanged = true;
    while (theChanged || this->mL195600 > theRing) {
        theChanged = false;
        theRing++;
        if (theStep == theCount)
            inHeight = theHeight;
        else {
            inHeight = theStep * theHeight * inSlope;
            theStep++;
        }
        long theStart = theRing + inY + 1;
        long theBottom = inEnd - theRing;
        long theX = inX - theRing;
        for (long theY = theStart; theY >= theBottom; theY--) {
            if (theY == theStart) {
                if (this->SmoothVertex(theX, theY, theX + 1, theY - 1, inHeight))
                    theChanged = true;
            } else if (theY == theBottom) {
                if (this->SmoothVertex(theX, theY, theX + 1, theY + 1, inHeight))
                    theChanged = true;
            } else if (this->SmoothVertex(theX, theY, theX + 1, theY, inHeight))
                theChanged = true;
        }
    }
    return inX - max(this->mL195600, theRing);
}

// FUNCTION: EXE 0x5221e6
// Smooths the slope right of a raised area until nothing changes and the corner rings are covered;
// returns the rightmost column reached
long RTSMap::SpreadRight(long inX, long inY, long inEnd, float inHeight, float inSlope)
{
    float theHeight = inHeight;
    float theSteps = inHeight / (inHeight * inSlope);
    unsigned long theCount = UMFloorUnsigned(theSteps);
    long theRing = 0;
    unsigned long theStep = 1;
    bool theChanged = true;
    long theBottom = inEnd;
    while (theChanged || this->mL195604 > theRing) {
        theChanged = false;
        theRing++;
        theBottom--;
        if (theStep == theCount)
            inHeight = theHeight;
        else {
            inHeight = theStep * theHeight * inSlope;
            theStep++;
        }
        long theStart = theRing + inY + 1;
        long theX = theRing + inX + 1;
        for (long theY = theStart; theY >= theBottom; theY--) {
            if (theY == theStart) {
                if (this->SmoothVertex(theX, theY, theX - 1, theY - 1, inHeight))
                    theChanged = true;
            } else if (theY == theBottom) {
                if (this->SmoothVertex(theX, theY, theX - 1, theY + 1, inHeight))
                    theChanged = true;
            } else if (this->SmoothVertex(theX, theY, theX - 1, theY, inHeight))
                theChanged = true;
        }
    }
    if (this->mL195604 > theRing)
        return this->mL195604 + inX;
    return inX + theRing;
}

// NEAR: the frame order of the five floats/tile/count (no-/Og layout follows the local-name hash;
// original: Z01 -4, Z00 -8, tile -0xc, count -0x10, Z11 -0x14, Z10 -0x18, lowest -0x1c). GetTile's ternary
// form (a temporary [-0x34], then the copy) is what the original shows; it is neutral at -O1 in the includers.
// FUNCTION: EXE 0x5222ed
// Counts the tile's corners at least 0.4 above its lowest corner (0 off the map)
// (built without /Og: every local lives in the frame, `leave`, the inline GetTile/GetZ results go through
// temporaries; its neighbours are normal -O1 code)
#pragma optimize("g", off)
long RTSMap::GetNumRaisedCorners(unsigned long inX, unsigned long inY)
{
    EEMapTile* theTile = this->GetTile(inX, inY);
    long theCount = 0;
    if (theTile != NULL) {
        float theZ00 = this->mTerrainMesh.GetZ(inX, inY);
        float theZ10 = this->mTerrainMesh.GetZ(inX + 1, inY);
        float theZ01 = this->mTerrainMesh.GetZ(inX, inY + 1);
        float theZ11 = this->mTerrainMesh.GetZ(inX + 1, inY + 1);
        float theLowest = min(theZ00, min(theZ10, min(theZ01, theZ11)));
        if (theZ00 - theLowest >= 0.4f)
            theCount++;
        if (theZ10 - theLowest >= 0.4f)
            theCount++;
        if (theZ01 - theLowest >= 0.4f)
            theCount++;
        if (theZ11 - theLowest >= 0.4f)
            theCount++;
    }
    return theCount;
}
#pragma optimize("g", on)

// FUNCTION: EXE 0x522517
// Turns every explored but unseen tile back to unexplored for each player but player 0, then unregisters
// those players from the doppelganger manager (done before a scenario is saved)
void EEMap::PrepareSave()
{
    long theY;
    for (theY = 0; theY < this->mSize; theY++) {
        for (long theX = 0; theX < this->mSize; theX++) {
            EEMapTile* theTile = this->mTiles[theY * this->mSize + theX];
            if (theTile == NULL)
                continue;
            for (unsigned long i = 1; i < this->mU19561C; i++) {
                if (theTile->GetVisibility(::gWorld.mPlayers[i]->GetPlayerID()) == 1)
                    theTile->SetVisibility(0, ::gWorld.mPlayers[i]);
            }
        }
    }
    for (unsigned long i = 1; i < this->mU19561C; i++)
        gDoppelgangerManager.UnregisterPlayer(::gWorld.mPlayers[i]);
}

// FUNCTION: EXE 0x5225ef
// Empties the map's area-outline tile runs
void RTSMap::ClearU1955DC()
{
    this->mU1955DC.clear();
}

// FUNCTION: EXE 0x5225fa
// Recomputes the height and water flag of the four tiles around a vertex and puts their units back on the ground
void RTSMap::UpdateTiles(long inX, long inY)
{
    for (long theY = inY - 1; theY <= inY; theY++) {
        for (long theX = inX - 1; theX <= inX; theX++) {
            if (!this->mTerrainMesh.GetIsValidTile(theX, theY))
                continue;
            EEMapTile* theTile = this->GetTile(theX, theY);
            float theHeight = min(this->mTerrainMesh.GetZ(theX, theY), this->mTerrainMesh.GetZ(theX + 1, theY));
            theHeight = min(theHeight, this->mTerrainMesh.GetZ(theX, theY + 1));
            theHeight = min(theHeight, this->mTerrainMesh.GetZ(theX + 1, theY + 1));
            theTile->mHeight = theHeight;
            if (theTile->mHeight < 0.0f)
                this->mTerrainMesh.SetIsWater(theX, theY, true);
            else
                this->mTerrainMesh.SetIsWater(theX, theY, false);
            for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL; theNode = theNode->mNext)
                this->PlaceUnitOnGround(theNode->mUnit);
        }
    }
}

// NEAR: (1) the 0.01f is the pooled constant 0x43bf0c, not the `__real` 0.01f names.txt binds at
// 0x43a688 (two 0.01f constants exist; 10 other functions use 0x43bf0c); (2) register/slot choice (`this` at -4,
// theResult at -8, the mesh pointer at -0xc and used for the row-count reads).
// FUNCTION: EXE 0x522761
// Brings a vertex within the slope of a neighbour (copies its height at slope 0), updating tiles and water;
// returns -1 if raised, 1 if lowered or copied, 0 if unchanged
long RTSMap::SmoothVertex(unsigned long inX, unsigned long inY, unsigned long inFromX, unsigned long inFromY,
                          float inSlope)
{
    long theResult = 0;
    GETerrainMesh* theMesh = &this->mTerrainMesh;
    if (theMesh->GetIsValidPoint(inX, inY) && theMesh->GetIsValidPoint(inFromX, inFromY)) {
        float theOldZ;
        if (inSlope == 0.0f) {
            theOldZ = this->mTerrainMesh.GetZ(inX, inY);
            theMesh->SetZ(inX, inY, this->mTerrainMesh.GetZ(inFromX, inFromY));
            theResult = 1;
        } else {
            long theDY = inY - inFromY;
            long theDX = inX - inFromX;
            inSlope = sqrt((float)(theDY * theDY) + theDX * theDX) * inSlope;
            float theZ = this->mTerrainMesh.GetZ(inX, inY);
            float theFromZ = this->mTerrainMesh.GetZ(inFromX, inFromY);
            if (theZ < theFromZ) {
                float theTarget = theFromZ - inSlope;
                if (theZ < theTarget) {
                    theOldZ = this->mTerrainMesh.GetZ(inX, inY);
                    this->mTerrainMesh.SetZ(inX, inY,
                                      theTarget > kNegativeHeightEpsilon && theTarget < kHeightEpsilon ? 0.0f : theTarget);
                    theResult = -1;
                }
            } else {
                float theTarget = theFromZ + inSlope;
                if (theZ > theTarget) {
                    theOldZ = this->mTerrainMesh.GetZ(inX, inY);
                    this->mTerrainMesh.SetZ(inX, inY,
                                      theTarget > kNegativeHeightEpsilon && theTarget < kHeightEpsilon ? 0.0f : theTarget);
                    theResult = 1;
                }
            }
        }
        if (theResult != 0) {
            this->UpdateTiles(inX, inY);
            if ((theOldZ < 0.0f && this->mTerrainMesh.GetZ(inX, inY) >= 0.0f) || this->mTerrainMesh.GetZ(inX, inY) < 0.0f)
                this->UpdateWater(inX, inY);
        }
    }
    return theResult;
}

// NEAR: only the load of inPlayerCount is scheduled differently (the original loads it with inWidth).
// FUNCTION: EXE 0x5229b3
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
    this->mL19560C = this->mSize + inWidth - 1;
    this->mL195610 = inHeight;
    this->mU19561C = inPlayerCount;
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
// FUNCTION: EXE 0x522b06
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

// FUNCTION: EXE 0x522c9b
// Returns the tile closest to a point and stores its coordinates
EEMapTile* EEMap::GetClosestTile(GE3DPoint* inPoint, long* ioX, long* ioY)
{
    this->mTerrainMesh.GetClosestTile(*inPoint, *ioX, *ioY);
    return this->GetTile(*ioX, *ioY);
}

// FUNCTION: EXE 0x522ce1
// Returns whether a tile's corner heights differ by at most 0.7 times the map's scale value (false off the map)
bool RTSMap::IsTileFlat(unsigned long inX, unsigned long inY)
{
    if (this->GetTile(inX, inY) == NULL)
        return false;
    float theLow = this->mTerrainMesh.GetZ(inX, inY);
    float theHigh = theLow;
    float theZ = this->mTerrainMesh.GetZ(inX + 1, inY);
    if (theZ < theLow)
        theLow = theZ;
    if (theZ > theHigh)
        theHigh = theZ;
    theZ = this->mTerrainMesh.GetZ(inX, inY + 1);
    if (theZ < theLow)
        theLow = theZ;
    if (theZ > theHigh)
        theHigh = theZ;
    theZ = this->mTerrainMesh.GetZ(inX + 1, inY + 1);
    if (theZ < theLow)
        theLow = theZ;
    if (theZ > theHigh)
        theHigh = theZ;
    if (theHigh - theLow <= kFlatSlope * this->mF1955F4)
        return true;
    return false;
}

// FUNCTION: EXE 0x522dec
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

// FUNCTION: EXE 0x522ee8
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

// FUNCTION: EXE 0x52307d
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

// FUNCTION: EXE 0x52320d
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

// FUNCTION: EXE 0x523259
// Reads the map's size and player count, recreates the map and loads its tiles and terrain mesh; returns the
// first read error (tiles get x = row start + column, which only old saves' terrain lookup uses)
long RTSMap::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    bool theLeftShrinking = true;
    bool theRightGrowing = true;
    long theResult = inFile.Read(this->mL195614);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mL195610);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mU19561C);
    if (theResult != kErrorOK)
        return theResult;
    this->Create(this->mL195614, this->mL195610, this->mU19561C, false);
    long theRight = this->mL195610;
    long theLeft = theRight - 1;
    for (long theY = 0; theY < this->mSize; theY++) {
        for (long theX = 0; theX < this->mSize; theX++) {
            EEMapTile* theTile;
            if (theX >= theLeft && theX < theRight + 1)
                theTile = this->LoadTile(inMajorVersion, inMinorVersion, inFile, theX, theY);
            else
                theTile = NULL;
            this->mTiles[this->mSize * theY + theX] = theTile;
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
                theRightGrowing = false;
                theRight = this->mSize - 1;
            }
        } else
            theRight--;
    }
    theResult = this->mTerrainMesh.Load(inMajorVersion, inMinorVersion, inFile);
    return theResult;
}

// NEAR: induction variable x - 1 ([ebp-4], with x + 1 as +2) and the dead-parameter slots of the
// results/loop counters differ.
// FUNCTION: EXE 0x523389
// Raises or smooths the terrain of a rectangle, spreads its edges, recomputes normals and shorelines and
// returns the changed rectangle through the in/out bounds
void RTSMap::Raise(long* ioLeft, long* ioRight, long* ioTop, long* ioBottom, float inHeight, float inSlope,
                   float inEdgeHeight, float inEdgeSlope, bool inRaise)
{
    bool theDone = !inRaise;
    this->mL195600 = 0;
    this->mL195604 = 0;
    for (long theY = *ioTop + 1; theY >= *ioBottom; theY--) {
        for (long theX = *ioLeft; theX <= *ioRight + 1; theX++) {
            if (!this->mTerrainMesh.GetIsValidPoint(theX, theY))
                continue;
            if (!theDone) {
                float theOldZ = this->mTerrainMesh.GetZ(theX, theY);
                this->mTerrainMesh.SetZ(theX, theY, inHeight);
                this->UpdateTiles(theX, theY);
                if ((theOldZ < 0.0f && this->mTerrainMesh.GetZ(theX, theY) >= 0.0f) ||
                    this->mTerrainMesh.GetZ(theX, theY) < 0.0f)
                    this->UpdateWater(theX, theY);
                theDone = true;
            }
            if (theX == *ioLeft && theY == *ioTop + 1)
                ;
            else if (theX == *ioLeft) {
                this->SmoothVertex(theX, theY, theX, theY + 1, inSlope);
                this->SmoothVertex(theX, theY, theX + 1, theY + 1, inSlope);
            } else if (theY == *ioTop + 1)
                this->SmoothVertex(theX, theY, theX - 1, theY, inSlope);
            else {
                this->SmoothVertex(theX, theY, theX - 1, theY + 1, inSlope);
                this->SmoothVertex(theX, theY, theX - 1, theY, inSlope);
                this->SmoothVertex(theX, theY, theX, theY + 1, inSlope);
            }
        }
    }
    long theTop = this->SpreadUp(*ioLeft, *ioRight, *ioTop, inEdgeHeight, inEdgeSlope);
    long theBottom = this->SpreadDown(*ioLeft, *ioRight, *ioBottom, inEdgeHeight, inEdgeSlope);
    long theLeft = this->SpreadLeft(*ioLeft, *ioTop, *ioBottom, inEdgeHeight, inEdgeSlope);
    long theRight = this->SpreadRight(*ioRight, *ioTop, *ioBottom, inEdgeHeight, inEdgeSlope);
    for (long theNormalY = theBottom - 1; theNormalY <= theTop + 1; theNormalY++) {
        for (long theNormalX = theLeft - 1; theNormalX <= theRight + 1; theNormalX++) {
            if (this->mTerrainMesh.GetIsValidPoint(theNormalX, theNormalY))
                this->mTerrainMesh.ComputeVertexNormal(theNormalX, theNormalY);
        }
    }
    for (long theShoreY = theBottom - 1; theShoreY <= theTop + 1; theShoreY++) {
        for (long theShoreX = theLeft - 1; theShoreX <= theRight + 1; theShoreX++) {
            if (this->mTerrainMesh.GetIsValidTile(theShoreX, theShoreY))
                this->mTerrainMesh.ComputeShorelines(theShoreX, theShoreY);
        }
    }
    *ioLeft = theLeft;
    *ioTop = theTop;
    *ioRight = theRight;
    *ioBottom = theBottom;
}

// FUNCTION: EXE 0x5236e6
// Frees the map's tile pointer table (not the tiles)
void RTSMap::DeleteTiles()
{
    delete[] this->mTiles;
}

// FUNCTION: EXE 0x5236f3
// Fogs every tile the player has seen or explored: it becomes explored but not in sight
void RTSMap::FogSeen(EEPlayer* inPlayer)
{
    for (long theX = 0; theX < this->mSize; theX++) {
        for (long theY = 0; theY < this->mSize; theY++) {
            EEMapTile* theTile = this->GetTile(theX, theY);
            if (theTile != NULL && theTile->GetVisibility(inPlayer->GetPlayerID()) > 0)
                theTile->SetVisibility(1, inPlayer);
        }
    }
}

// FUNCTION: EXE 0x52377f
// Marks every unexplored tile as explored for the player (reveals the map)
void RTSMap::ExploreUnseen(EEPlayer* inPlayer)
{
    for (long theX = 0; theX < this->mSize; theX++) {
        for (long theY = 0; theY < this->mSize; theY++) {
            EEMapTile* theTile = this->GetTile(theX, theY);
            if (theTile != NULL && theTile->GetVisibility(inPlayer->GetPlayerID()) == 0)
                theTile->SetVisibility(1, inPlayer);
        }
    }
}

// FUNCTION: EXE 0x523807
// Writes the map's size, player count, every tile and the terrain mesh; returns the first write error
long RTSMap::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = outFile.Write(this->mL195614);
    if (theResult == kErrorOK) {
        theResult = outFile.Write(this->mL195610);
        if (theResult == kErrorOK) {
            theResult = outFile.Write(this->mU19561C);
            if (theResult == kErrorOK) {
                for (long theY = 0; theY < this->mSize && theResult == kErrorOK; theY++) {
                    for (long theX = 0; theX < this->mSize && theResult == kErrorOK; theX++) {
                        EEMapTile* theTile = this->mTiles[this->mSize * theY + theX];
                        if (theTile != NULL)
                            theResult = theTile->Save(outFile, this->mU19561C);
                    }
                }
                if (theResult == kErrorOK)
                    theResult = this->mTerrainMesh.Save(outFile);
            }
        }
    }
    return theResult;
}

// NEAR: inline budget (begin() refused in the prologue, Contains/operator+ out of line) and the
// by-value point copies: the original copies the points with the out-of-line copy ctor 0x4d28b (479 callers in
// the exe), so the exe's U2DPoint<long> probably has the user-declared copy ctor that U2DPoint.h keeps engine-only.
// FUNCTION: EXE 0x5238bd
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
        for (long theX = theLeft; theX < theRight; theX++) {
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
        for (long theY = theTop; theY < theBottom; theY++) {
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
// FUNCTION: EXE 0x523e36
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

// TEMPLATE: EXE 0x523f63
// (vector<RTSMapVertexColors>::clear; the template COMDATs close the unit)

// TEMPLATE: EXE 0x523f9e
// (vector<RTSMapVertexColors>::erase(F, L))

// TEMPLATE: EXE 0x524008
// (vector<RTSMapVertexColors>::_Tidy)

// TEMPLATE: EXE 0x524040
// (std::copy of RTSMapVertexColors)

// TEMPLATE: EXE 0x524064
// (std::_Destroy(RTSMapVertexColors*))

// FUNCTION: EXE 0x524072
// Sets the tile's visibility state for a player
// (RTSTile::SetVisibility, inline in ee/EEMap.h: its out-of-line copy, first called by PrepareSave)
