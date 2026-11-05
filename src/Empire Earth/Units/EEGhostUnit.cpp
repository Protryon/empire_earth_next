// C:\EEX\Empire Earth\Units\EEGhostUnit.cpp (file name inferred: no __FILE__; the class name). A unit placed but not yet
// built (RTTI EEGhostUnit, vtable 0x446ef4): 0x20360e..0x20560c. Unit flags: the exe's module set. No init chain.

#include <windows.h>
#include <stdio.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/ESChecksum.h"
#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEDataPreLoading.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEWheel.h"
#include "ee/EEGhostUnit.h"
#include "ee/EEUCBuilding.h"
#include "ee/EEUCGate.h"
#include "lle/GEViewport.h"
#include "ee/EEUnitPicker.h"
#include "ee/EEAEManager.h"
#include "ee/EEUUUnitFunctor.h"
#include "ee/EEUnitUtilities.h"
bool gGhostUnitsChanged;                    // 0x563240 (.bss after EEComplexUnit's, before EEGoalQueueContainer's; ee/EEGhostUnit.h)

long EETurnGatePart(GETransformation&, long, long);
void EEGetWallGateTiles(const U2DPoint<long>&, long, std::vector<U2DPoint<long> >&);
bool EEIsLandTerrain(long);
bool EEIsSpaceDockSiteOpen(long, long, EEPlayer*);
extern GEViewport gViewport;

// .rdata 0x446dc0: the four facing yaws (docks, robotic farm pieces) (provisional name)
static const float kWallRotations[4] = {0.0f, 3.1415927f, -1.5707964f, 1.5707964f};

// FUNCTION: EXE 0x60360e
// Creates a placement ghost of a unit class for a player, loading the class media unless told not to
EEGhostUnit::EEGhostUnit(long inUnitClass, EEPlayer* inPlayer, bool inNoMedia)
{
    this->Reset();
    this->mUnitClass = gUnitClassTable[inUnitClass];
    this->mPlayer = inPlayer;
    if (gFreeCamera) {
        inPlayer->LoadUnitClassMedia(inUnitClass);
        this->mPlayer->SetUnitClassMedia(inUnitClass, 0x7f);
    } else if (!inNoMedia)
        gDataPreLoading->PostLoadUnitClass(inPlayer->mPlayerID, inUnitClass);
    this->mNoMedia = inNoMedia;
}

// SYNTHETIC: EXE 0x60368b
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x6036a7
// Destroys the ghost, unloading its class media (queued unless in free camera or shutting down)
EEGhostUnit::~EEGhostUnit()
{
    if (!this->mNoMedia) {
        if (!gFreeCamera && UShutdownInProgress() != true) {
            gDataPreLoading->PostUnloadUnitClass(this->mPlayer->mPlayerID, this->mUnitClass->GetUnitType());
            return;
        }
        this->mPlayer->UnloadUnitClassMedia(this->mUnitClass->GetUnitType());
    }
}

// FUNCTION: EXE 0x60370a
// Fills a space building's footprint and heights: placeable when its space dock site is open or every
// inner cell is, with 2+ land rim tiles; then turns it to the space dock yaw or the dock facing
void EEGhostUnit::FillDockSquare()
{
    GE3DPoint thePosition(this->mTransformation);
    EEMap* theMap = gWorld.GetMap();
    long theType = this->mUnitClass->GetUnitType();
    float theOffset = this->GetFootprintOffset();
    float theYaw = 0.0f;
    long theBorder = 0;
    long theInterior = 0;
    long theWater = 0;
    bool theValid = true;
    long theLeft = (long)((float)this->mTileX - theOffset);
    long theBottom = (long)((float)this->mTileY - theOffset);
    long theRight = (long)((float)this->mTileX + theOffset);
    long theTop = (long)((float)this->mTileY + theOffset);
    if (this->mUnitClass->GetUnitType() == kUnitTypeSpaceDock) {
        theValid = EEIsSpaceDockSiteOpen(this->mTileX, this->mTileY, this->mPlayer);
        if (!theValid)
            theBorder = 0;
        else {
            theBorder = 2;
            for (long i = theLeft + 1; i < theRight; i++)
                for (long j = theBottom + 1; j < theTop; j++)
                    this->mFootprint[i - theLeft - 1][j - theBottom - 1] = true;
        }
    } else {
        for (long x = theLeft; x <= theRight; x++) {
            for (long y = theBottom; y <= theTop; y++) {
                EETile* theTile = theMap->GetTile(x, y);
                if (theTile == NULL || theTile->mTerrainType == kTerrainDeepWater || theTile->mTerrainType == kTerrainShallowWater || theTile->mTerrainType == kTerrainSpace)
                    this->mHeights[(x - theLeft) * 12 + y - theBottom] = 0.0f;
                else
                    this->mHeights[(x - theLeft) * 12 + y - theBottom] = theMap->mTerrainMesh.GetZ(x, y);
                if (x == theLeft || y == theBottom || x == theRight || y == theTop) {
                    if (theTile != NULL) {
                        if (EEIsLandTerrain(theTile->mTerrainType))
                            theBorder++;
                        else if (theTile->mTerrainType == kTerrainSpace)
                            theWater++;
                    }
                } else {
                    bool theCell = false;
                    if (theTile != NULL) {
                        theCell = EECanPlace(this->mPlayer, theType, &thePosition, x, y, 0, 0);
                        if (EEIsLandTerrain(theTile->mTerrainType))
                            theInterior++;
                    }
                    this->mFootprint[x - theLeft - 1][y - theBottom - 1] = theCell;
                    if (!theCell)
                        theValid = false;
                }
            }
        }
    }
    if (theValid) {
        if (this->mUnitClass->GetUnitType() == kUnitTypeSpaceDock)
            theYaw = EEGetSpaceDockYaw(this->mTileX, this->mTileY, this->mUnitClass->GetTileSize());
        else
            theYaw = kWallRotations[EEGetDockFacing(this->mTileX, this->mTileY)];
    }
    if (theBorder < 2) {
        for (long a = theLeft + 1; a < theRight; a++)
            for (long b = theBottom + 1; b < theTop; b++)
                this->mFootprint[a - theLeft - 1][b - theBottom - 1] = false;
    }
    this->mCanPlace = theValid && theBorder >= 2;
    if (this->mCanPlace)
        this->mTransformation.SetOrientationYPR(theYaw, 0.0f, 0.0f);
}

// FUNCTION: EXE 0x603a8c
// Fills a dock's footprint and heights: placeable when every inner cell is and at least 3 rim tiles are land
void EEGhostUnit::FillRimSquare()
{
    GE3DPoint thePosition(this->mTransformation);
    EEMap* theMap = gWorld.GetMap();
    long theType = this->mUnitClass->GetUnitType();
    float theOffset = this->GetFootprintOffset();
    long theBorder = 0;
    bool theValid = true;
    long theLeft = (long)((float)this->mTileX - theOffset);
    long theBottom = (long)((float)this->mTileY - theOffset);
    long theRight = (long)((float)this->mTileX + theOffset);
    long theTop = (long)((float)this->mTileY + theOffset);
    for (long x = theLeft; x <= theRight; x++) {
        for (long y = theBottom; y <= theTop; y++) {
            EETile* theTile = theMap->GetTile(x, y);
            if (theTile == NULL || theTile->mTerrainType == kTerrainDeepWater || theTile->mTerrainType == kTerrainShallowWater)
                this->mHeights[(x - theLeft) * 12 + y - theBottom] = 0.0f;
            else
                this->mHeights[(x - theLeft) * 12 + y - theBottom] = theMap->mTerrainMesh.GetZ(x, y);
            if (x != theLeft && y != theBottom && x != theRight && y != theTop) {
                bool theCell = theTile != NULL ? EECanPlace(this->mPlayer, theType, &thePosition, x, y, 0, 0) : false;
                this->mFootprint[x - theLeft - 1][y - theBottom - 1] = theCell;
                if (!theCell)
                    theValid = false;
            } else {
                if (theTile != NULL && EEIsLandTerrain(theTile->mTerrainType))
                    theBorder++;
            }
        }
    }
    if (theBorder < 3) {
        for (long i = theLeft + 1; i < theRight; i++)
            for (long j = theBottom + 1; j < theTop; j++)
                this->mFootprint[i - theLeft - 1][j - theBottom - 1] = false;
    }
    this->mCanPlace = theValid && theBorder >= 3;
}

// FUNCTION: EXE 0x603cb1
// Marks a mining building's whole footprint placeable when a unit of its overlap set stands on tile (x-1, y-1)
void EEGhostUnit::UpdateFootprint()
{
    EEMap* theMap = ::gWorld.GetMap();
    float theOffset = this->GetFootprintOffset();
    float theX = (float)this->mTileX;
    long theLeft = (long)(theX - theOffset);
    float theY = (float)this->mTileY;
    long theBottom = (long)(theY - theOffset);
    long theRight = (long)(theX + theOffset);
    long theTop = (long)(theY + theOffset);
    this->mCanPlace = false;
    EEMapTile* theTile = theMap->GetTile(this->mTileX - 1, this->mTileY - 1);
    if (theTile != NULL) {
        for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL && !this->mCanPlace; theNode = theNode->mNext) {
            if (theNode->mUnit != NULL && gUnitSetManager.Contains(theNode->mUnit->mUnitClass->GetUnitType(), this->mUnitSet))
                this->mCanPlace = true;
        }
    }
    for (long i = theLeft + 1; i < theRight; i++)
        for (long j = theBottom + 1; j < theTop; j++)
            this->mFootprint[i - theLeft - 1][j - theBottom - 1] = this->mCanPlace;
}

// FUNCTION: EXE 0x603dd6
// Fills the ghost's footprint and heights and whether it can be placed; mining, dock, space, volcano have variants
void EEGhostUnit::FillSquare()
{
    GE3DPoint thePosition(this->mTransformation);
    EEMap* theMap = gWorld.GetMap();
    long theType = this->mUnitClass->GetUnitType();
    bool theValid = true;
    if (this->mUnitClass->mCategory == kUnitCategoryMiningBuilding)
        this->UpdateFootprint();
    else if (theType == kUnitTypeDock || theType == kUnitTypeNavyYard)
        this->FillRimSquare();
    else if (theType == kUnitTypeSpaceDock || theType == kUnitTypeSpaceTurret || theType == kUnitTypeOrbitalSpaceStation)
        this->FillDockSquare();
    else if (theType == kUnitTypeVolcano)
        this->FillVolcanoSquare();
    else {
        long theSize = this->mUnitClass->GetTileSize();
        long theLeft;
        long theBottom;
        if (theSize <= 1) {
            theLeft = this->mTileX;
            theBottom = this->mTileY;
            theSize = 1;
        } else {
            long theHalf = (long)(theSize * 0.5f);
            theLeft = this->mTileX - theHalf;
            theBottom = this->mTileY - theHalf;
        }
        long theTop = theBottom + theSize;
        long theRight = theLeft + theSize;
        for (long x = theLeft; x <= theRight; x++) {
            for (long y = theBottom; y <= theTop; y++) {
                EETile* theTile = theMap->GetTile(x, y);
                bool theCell;
                if (theTile == NULL) {
                    theCell = false;
                    this->mHeights[(x - theLeft) * 12 + y - theBottom] = 0.0f;
                } else {
                    theCell = EECanPlace(this->mPlayer, theType, &thePosition, x, y, 0, 0);
                    if (theTile->mTerrainType == kTerrainDeepWater || theTile->mTerrainType == kTerrainShallowWater)
                        this->mHeights[(x - theLeft) * 12 + y - theBottom] = 0.0f;
                    else
                        this->mHeights[(x - theLeft) * 12 + y - theBottom] = theMap->mTerrainMesh.GetZ(x, y);
                }
                this->mFootprint[x - theLeft][y - theBottom] = theCell;
                if (x < theRight && y < theTop && !theCell)
                    theValid = false;
            }
        }
        this->mCanPlace = theValid;
    }
}

// FUNCTION: EXE 0x603fdb
// Fills a volcano's footprint and heights: each cell unblocked, off the map edge and free of area effect 3
void EEGhostUnit::FillVolcanoSquare()
{
    GE3DPoint thePosition(this->mTransformation);
    EEMap* theMap = gWorld.GetMap();
    long theType = this->mUnitClass->GetUnitType();
    long theClassSize = this->mUnitClass->GetTileSize();
    long theSize = 1;
    bool theValid = true;
    long theLeft;
    long theBottom;
    if (theClassSize <= 1) {
        theLeft = this->mTileX;
        theBottom = this->mTileY;
    } else {
        theSize = theClassSize;
        long theHalf = (long)(theSize * 0.5f);
        theLeft = this->mTileX - theHalf;
        theBottom = this->mTileY - theHalf;
    }
    long theRight = theLeft + theSize;
    long theTop = theBottom + theSize;
    for (long x = theLeft; x <= theRight; x++) {
        for (long y = theBottom; y <= theTop; y++) {
            EETile* theTile = theMap->GetTile(x, y);
            bool theCell;
            if (theTile != NULL) {
                theCell = EEIsTileUnblocked(this->mPlayer, theType, theTile);
                if (theTile->mTerrainType == kTerrainDeepWater || theTile->mTerrainType == kTerrainShallowWater)
                    this->mHeights[(x - theLeft) * 12 + y - theBottom] = 0.0f;
                else
                    this->mHeights[(x - theLeft) * 12 + y - theBottom] = theMap->mTerrainMesh.GetZ(x, y);
                if (theCell) {
                    theCell = !theMap->IsEdgeTile(x, y);
                    if (theCell)
                        theCell = !gAEManager.HasAreaEffect2(3, x, y, this->mPlayer->mPlayerID);
                }
            } else {
                theCell = false;
                this->mHeights[(x - theLeft) * 12 + y - theBottom] = 0.0f;
            }
            this->mFootprint[x - theLeft][y - theBottom] = theCell;
            if (x < theRight && y < theTop && !theCell)
                theValid = false;
        }
    }
    this->mCanPlace = theValid;
}

// FUNCTION: EXE 0x604208
// Resets the ghost to an empty, unplaceable one at the origin with animation 7
void EEGhostUnit::Reset()
{
    this->mTransformation.SetX(0.0f);
    this->mTransformation.SetY(0.0f);
    this->mTransformation.SetZ(0.0f);
    this->mCanPlace = false;
    this->mPlayer = NULL;
    this->mTileX = 0;
    this->mTileY = 0;
    this->mUnitClass = NULL;
    this->mFrame = 0;
    this->mAnimation = 7;
    this->mNoMedia = false;
    memset(this->mFootprint, 0, sizeof(this->mFootprint));
    memset(this->mHeights, 0, sizeof(this->mHeights));
}

// FUNCTION: EXE 0x604278
// Replaces the placement ghosts with a unit's ghost, plus 3 runway ghosts for an airport (0x42) and the
// farm ghosts around a granary unless it is being moved
void EEGhostUnit::Create(const GE2DPoint& inLocation, long inUnitType, EEPlayer* inPlayer, float inRotation,
                         bool inIsMove)
{
    EEUnitClass* theClass = gUnitClassTable[inUnitType];
    EEUnitClass* theWallClass = gUnitClassTable[kUnitTypeAirportRunway];
    ::gWorld.mGhostUnitsLock->Enter();
    ::gWorld.ClearGhostUnits();
    EEGhostUnit* theGhost = new EEGhostUnit(theClass->GetUnitType(), inPlayer, false);
    theGhost->mTransformation.SetOrientationYPR(theClass->mMaxAngularVelocity == 0.0f ? 0.0f : inRotation, 0.0f, 0.0f);
    ::gWorld.AddGhostUnit(theGhost);
    if (inUnitType == 0x42) {
        for (long i = 0; i < 3; i++) {
            theGhost = new EEGhostUnit(theWallClass->GetUnitType(), inPlayer, false);
            theGhost->mTransformation.SetOrientationYPR(theWallClass->mMaxAngularVelocity == 0.0f ? 0.0f : inRotation, 0.0f, 0.0f);
            ::gWorld.AddGhostUnit(theGhost);
        }
    }
    if ((theClass->GetUnitType() == kUnitTypeGranaryImproved || theClass->GetUnitType() == kUnitTypeGranary) && !inIsMove) {
        EEUnitClass* thePieceClass = gUnitClassTable[EEGetFarmType(inPlayer)];
        long theSize = theClass->GetTileSize();
        long theCount = max(theSize, 3) * max(theSize, 3) - 1;
        for (long i = 0; i < theCount; i++) {
            theGhost = new EEGhostUnit(thePieceClass->GetUnitType(), inPlayer, false);
            theGhost->mTransformation.SetOrientationYPR(thePieceClass->mMaxAngularVelocity == 0.0f ? 0.0f : inRotation, 0.0f, 0.0f);
            if (thePieceClass->GetUnitType() == kUnitTypeRoboticFarm) {
                bool theFlag = false;
                long thePiece = EEGetWallPiecePart(i >= 4 ? i + 1 : i, &theFlag);
                if (theFlag)
                    theGhost->SetAnimation(0xb);
                float theYaw = kWallRotations[thePiece];
                theGhost->mTransformation.SetOrientationYPR(theYaw, 0.0f, 0.0f);
            }
            ::gWorld.AddGhostUnit(theGhost);
        }
    }
    if (theClass->mCategory == kUnitCategoryMiningBuilding)
        theGhost->mUnitSet = theClass->mOverlapUnitSet;
    gGhostUnitsChanged = false;
    ::gWorld.mGhostUnitsLock->Leave();
}

// FUNCTION: EXE 0x6044d4
// Replaces the placement ghosts with a gate's three parts (left, middle, right), each turned as its part
void EEGhostUnit::CreateGate(float inRotation, EEUnitClass* inClass, EEPlayer* inPlayer)
{
    EEUCGate* theClass = dynamic_cast<EEUCGate*>(inClass);
    GETransformation theTransformation(false);
    gWorld.mGhostUnitsLock->Enter();
    gWorld.ClearGhostUnits();
    EEGhostUnit* theGhost = new EEGhostUnit(theClass->GetUnitType(), inPlayer, true);
    theGhost->mTransformation.SetOrientationYPR(theClass->mMaxAngularVelocity == 0.0f ? 0.0f : inRotation, 0.0f, 0.0f);
    theTransformation = theGhost->mTransformation;
    theGhost->SetAnimation(EETurnGatePart(theTransformation, 2, 0));
    theGhost->mTransformation = theTransformation;
    gWorld.AddGhostUnit(theGhost);
    theGhost = new EEGhostUnit(theClass->GetUnitType(), inPlayer, true);
    theGhost->mTransformation.SetOrientationYPR(theClass->mMaxAngularVelocity == 0.0f ? 0.0f : inRotation, 0.0f, 0.0f);
    theTransformation = theGhost->mTransformation;
    theGhost->SetAnimation(EETurnGatePart(theTransformation, 2, 2));
    theGhost->mTransformation = theTransformation;
    gWorld.AddGhostUnit(theGhost);
    theGhost = new EEGhostUnit(theClass->GetUnitType(), inPlayer, true);
    theGhost->mTransformation.SetOrientationYPR(theClass->mMaxAngularVelocity == 0.0f ? 0.0f : inRotation, 0.0f, 0.0f);
    theTransformation = theGhost->mTransformation;
    theGhost->SetAnimation(EETurnGatePart(theTransformation, 2, 4));
    theGhost->mTransformation = theTransformation;
    gWorld.AddGhostUnit(theGhost);
    gGhostUnitsChanged = false;
    gWorld.mGhostUnitsLock->Leave();
}

// FUNCTION: EXE 0x6046fc
// Replaces the placement ghosts with one wall ghost per planned tile, shaped by its corner and set on the ground
// (free function; whether it was static is not known) (provisional name)
void EEBuildWallGhosts(std::vector<EETileCoordinate>& inTiles, std::vector<long>& inCorners,
                long inTechnology, EEPlayer* inPlayer, float inRotation)
{
    EEUnitClass* theClass = gUnitClassTable[inPlayer->mTechnologies->Get(inTechnology)->mData->mUnitClass];
    gWorld.mGhostUnitsLock->Enter();
    gWorld.ClearGhostUnits();
    for (unsigned long i = 0; i < inTiles.size(); i++) {
        long theX = inTiles[i].GetX();
        long theY = inTiles[i].mY;
        EEGhostUnit* theGhost = new EEGhostUnit(theClass->GetUnitType(), inPlayer, true);
        theGhost->mTransformation.SetOrientationYPR(theClass->mMaxAngularVelocity == 0.0f ? 0.0f : inRotation, 0.0f, 0.0f);
        EEMapTile* theTile = gWorld.GetMap()->GetTile(theX, theY);
        long theRaised = gWorld.GetMap()->GetNumRaisedCorners(theX, theY);
        EEOrientWallGhost(theGhost, inCorners[i], theRaised);
        theGhost->mTileX = theX;
        theGhost->mTileY = theY;
        gWorld.AddGhostUnit(theGhost);
        if (theTile == NULL)
            theGhost->mCanPlace = false;
        else
            theGhost->FillSquare();
        float theCenterX = theX + 0.5f;
        float theCenterY = theY + 0.5f;
        float theHeight = 0.0f;
        if (theTile != NULL) {
            theHeight = gWorld.GetMap()->mTerrainMesh.GetMeshZ(theCenterX, theCenterY);
            if (theHeight < 0.0f)
                theHeight = 0.0f;
        }
        theGhost->mTransformation.SetX(theCenterX);
        theGhost->mTransformation.SetY(theCenterY);
        theGhost->mTransformation.SetZ(theHeight);
    }
    gGhostUnitsChanged = true;
    gWorld.mGhostUnitsLock->Leave();
}

// FUNCTION: EXE 0x6048f4
// Adds the ghost's player number and unit class name to the sync checksum
void EEGhostUnit::AddChecksum(ESChecksum& ioChecksum)
{
    UString theText;
    sprintf(theText, "The Ghost Unit Player Number was: %ld", this->mPlayer->mPlayerID);
    ioChecksum.AddChecksum(theText, 0);
    sprintf(theText, "The Object Type was %s", (const char*)this->mUnitClass->mName);
    ioChecksum.AddChecksum(theText, 1);
}

// FUNCTION: EXE 0x60496b
// Returns the half-width of the footprint square around the ghost's tile (1 for a one-tile unit)
float EEGhostUnit::GetFootprintOffset()
{
    long theSize = this->mUnitClass->GetTileSize();
    float theOffset;
    if (theSize <= 1)
        theOffset = 1.0f;
    else if (theSize & 1)
        theOffset = UMCeil(theSize * 0.5f);
    else {
        float theHalf = theSize * 0.5f;
        theOffset = theHalf + 0.5f;
    }
    return theOffset;
}

// FUNCTION: EXE 0x6049c0
// Reads the unit class and player ids from the saved game; returns the first read error
long EEGhostUnit::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    unsigned long theValue;
    long theResult = inFile.Read(theValue);
    this->mUnitClass = reinterpret_cast<EEUnitClass*>(theValue);
    if (theResult == kErrorOK) {
        theResult = inFile.Read(theValue);
        this->mPlayer = reinterpret_cast<EEPlayer*>(theValue);
    }
    return theResult;
}

// FUNCTION: EXE 0x6049fc
// Moves a farm ghost to the slot tile in the given direction from its tile
void EEGhostUnit::SetFarmSlotTile(EEGhostUnit* inGhost, long inDirection)
{
    EETileCoordinate theNext(0, 0);
    EETileCoordinate theTile;
    theTile.mX = inGhost->mTileX;
    theTile.mY = inGhost->mTileY;
    EEGetSlotTile(theTile, inDirection, theNext);
    inGhost->mTileX = theNext.GetX();
    inGhost->mTileY = theNext.mY;
}

// FUNCTION: EXE 0x604a48
// Writes the unit type and player id to the saved game; returns the first write error
long EEGhostUnit::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    unsigned long theType = this->mUnitClass->GetUnitType();
    long theResult = outFile.Write(theType);
    if (theResult == 0)
        theResult = outFile.Write(this->mPlayer->GetPlayerID());
    return theResult;
}

// FUNCTION: EXE 0x604a97
// Sets the ghost's animation and its frame to that animation's first
void EEGhostUnit::SetAnimation(long inIndex)
{
    this->mFrame = this->GetModel()->mStartFrames[inIndex];
    this->mAnimation = inIndex;
}

// FUNCTION: EXE 0x604aba
// Moves the placement ghosts under the cursor and rechecks where each can be placed; an airport turns
// to its facing quarter with its runways behind it, farm ghosts take their slots
void EEPreviewPlacement(const GE2DPoint& inLocation)
{
    GE3DPoint thePosition;
    std::vector<EETileCoordinate> theUnused;
    long theDirection = 0;
    gWorld.mGhostUnitsLock->Enter();
    if (!gViewport.GetViewRectangle().Contains(inLocation) || gWorld.GetNumGhostUnits() == 0)
        gGhostUnitsChanged = false;
    else {
        gGhostUnitsChanged = true;
        EEGhostUnit* theFirstGhost = gWorld.mGhostUnits[0];
        EETile* theTile;
        bool theOnMap = EEPointUnitOrTile(inLocation, theTile, thePosition, true);
        long theX = (long)thePosition.GetX();
        long theY = (long)thePosition.mY;
        for (unsigned long i = 0; i < gWorld.GetNumGhostUnits(); i++) {
            EEGhostUnit* theGhost = gWorld.mGhostUnits[i];
            theGhost->GetModel();
            theGhost->mTileX = theX;
            theGhost->mTileY = theY;
            long theType = theGhost->mUnitClass->GetUnitType();
            if (theType == kUnitTypeAirport) {
                EEUCBuilding* theClass = dynamic_cast<EEUCBuilding*>(gUnitClassTable[kUnitTypeAirport]);
                theDirection = EEGetFacingQuarter(theX - theClass->GetTileSize() / 2, theY - theClass->GetTileSize() / 2);
                switch (theDirection) {
                case 0: theGhost->mTransformation.SetOrientationYPR(0.0f, 0.0f, 0.0f); break;
                case 1: theGhost->mTransformation.SetOrientationYPR(3.1415927f, 0.0f, 0.0f); break;
                case 2: theGhost->mTransformation.SetOrientationYPR(-1.5707964f, 0.0f, 0.0f); break;
                case 3: theGhost->mTransformation.SetOrientationYPR(1.5707964f, 0.0f, 0.0f); break;
                }
            } else if (theType == kUnitTypeAirportRunway) {
                float theOffset = (float)(theGhost->mUnitClass->GetTileSize() * i);
                switch (theDirection) {
                case 0: theGhost->mTileY = theY + (long)theOffset; break;
                case 1: theGhost->mTileY = theY - (long)theOffset; break;
                case 2: theGhost->mTileX = theX + (long)theOffset; break;
                case 3: theGhost->mTileX = theX - (long)theOffset; break;
                }
            } else if ((theType == kUnitTypeFarm || theType == kUnitTypeRoboticFarm) && gWorld.GetNumGhostUnits() != 1)
                EEGhostUnit::SetFarmSlotTile(theGhost, i);
            if ((theGhost->mUnitClass->GetTileSize() > 1 || theGhost->mUnitClass->IsLandBuilding()
                 || (theGhost->mUnitClass->GetUnitType() != kUnitTypePathPoint && theGhost->mUnitClass->mIsComplex != 1)) && theOnMap) {
                if ((theGhost->mUnitClass->GetTileSize() & 1) == 0) {
                    thePosition.mX = (float)theGhost->mTileX;
                    thePosition.mY = (float)theGhost->mTileY;
                } else {
                    thePosition.mX = (float)theGhost->mTileX + 0.5f;
                    thePosition.mY = (float)theGhost->mTileY + 0.5f;
                }
                if (gWorld.GetMap()->GetTile(theGhost->mTileX, theGhost->mTileY) != NULL) {
                    thePosition.mZ = gWorld.GetMap()->mTerrainMesh.GetMeshZ(thePosition.GetX(), thePosition.mY);
                    if (thePosition.mZ < 0.0f)
                        thePosition.mZ = 0.0f;
                }
            }
            theGhost->mTransformation.SetPosition(thePosition);
            if (!theOnMap)
                theGhost->mCanPlace = false;
            else {
                if (theGhost->mUnitClass->GetUnitType() == kUnitTypeAirportRunway) {
                    for (unsigned long j = 0; j < i; j++) {
                        theGhost->mCanPlace = true;
                        if (!gWorld.mGhostUnits[j]->mCanPlace)
                            theGhost->mCanPlace = false;
                        if (theGhost->mCanPlace)
                            theGhost->FillSquare();
                    }
                } else
                    theGhost->FillSquare();
                if (!theGhost->mCanPlace && theGhost->mUnitClass->GetUnitType() == kUnitTypeAirportRunway) {
                    for (unsigned long k = 0; k < i; k++)
                        gWorld.mGhostUnits[k]->mCanPlace = false;
                }
            }
            theGhost->mUnitClass->LocationToTile(thePosition, theGhost->mTileX, theGhost->mTileY);
            if (gWorld.GetMap()->GetTile(theGhost->mTileX, theGhost->mTileY) == NULL)
                theGhost->mCanPlace = false;
        }
    }
    gWorld.mGhostUnitsLock->Leave();
}

// FUNCTION: EXE 0x604f02
// Moves the gate ghost's three parts onto the wall tiles under the cursor; placeable only over a marked
// wall piece, whose gate direction it takes
void EEGhostUnit::PreviewGate(const GE2DPoint& inLocation, long& ioDirection)
{
    std::vector<U2DPoint<long> > theTiles;
    EEMap* theMap = gWorld.GetMap();
    bool theNewGate = true;
    gWorld.mGhostUnitsLock->Enter();
    if (!gViewport.GetViewRectangle().Contains(inLocation))
        gGhostUnitsChanged = false;
    else {
        gGhostUnitsChanged = true;
        EEUnit* theUnit = gUnitPicker.PickUnit(inLocation, 0);
        long theX;
        long theY;
        EETile* theTile;
        if (theUnit == NULL || !theUnit->mUnitClass->mIsWall)
            EEPointUnitOrTileXY(inLocation, theTile, theX, theY, true);
        else {
            theX = theUnit->mTileX;
            theY = theUnit->mTileY;
        }
        if (theUnit != NULL)
            theUnit->Dereference();
        EEGhostUnit* theLeft = gWorld.mGhostUnits[0];
        EEGhostUnit* theMiddle = gWorld.mGhostUnits[1];
        EEGhostUnit* theRight = gWorld.mGhostUnits[2];
        theTile = gWorld.GetMap()->GetTile(theX, theY);
        if (theTile != NULL) {
            RTSTileUnitNode* theNode = theTile->mUnits;
            EEUnit* theExisting = theNode ? theNode->mUnit : NULL;
            while (theExisting != NULL && !theExisting->mUnitClass->mIsWall && theNode != NULL) {
                theNode = theNode->mNext;
                if (theNode != NULL)
                    theExisting = theNode->mUnit;
            }
            if (theExisting != NULL && theExisting->mUnitClass->mIsWall
                && theExisting->mComplexUnit != NULL && theExisting->mComplexUnit->mIsTinted) {
                theNewGate = false;
                ioDirection = theExisting->mComplexUnit->mGateDirection;
            }
            U2DPoint<long> theCenter(theX, theY);
            EEGetWallGateTiles(theCenter, ioDirection, theTiles);
            GETransformation theTransformation(false);
            float theZ;
            theTransformation = theMiddle->mTransformation;
            theMiddle->SetAnimation(EETurnGatePart(theTransformation, ioDirection, 2));
            theMiddle->mTransformation = theTransformation;
            theMiddle->mTileX = theTiles[2].GetX();
            theMiddle->mTileY = theTiles[2].mY;
            float theMiddleX = (float)theMiddle->mTileX + 0.5f;
            float theMiddleY = (float)theMiddle->mTileY + 0.5f;
            if (theTile == NULL || theTile->mTerrainType == kTerrainDeepWater || theTile->mTerrainType == kTerrainShallowWater)
                theZ = 0.0f;
            else
                theZ = gWorld.GetMap()->mTerrainMesh.GetMeshZ(theMiddleX, theMiddleY);
            theMiddle->mTransformation.SetPosition(theMiddleX, theMiddleY, theZ);
            long theMiddleStartX = theMiddle->mTileX;
            long theMiddleStartY = theMiddle->mTileY;
            for (long theMiddleXIndex = theMiddleStartX; theMiddleXIndex <= theMiddleStartX + 1; theMiddleXIndex++) {
                for (long theMiddleYIndex = theMiddleStartY; theMiddleYIndex <= theMiddleStartY + 1; theMiddleYIndex++) {
                    theTile = theMap->GetTile(theMiddleXIndex, theMiddleYIndex);
                    if (theTile == NULL)
                        continue;
                    if (theTile->mTerrainType == kTerrainDeepWater || theTile->mTerrainType == kTerrainShallowWater)
                        theMiddle->mHeights[(theMiddleXIndex - theMiddleStartX) * 12 + theMiddleYIndex - theMiddleStartY] = 0.0f;
                    else
                        theMiddle->mHeights[(theMiddleXIndex - theMiddleStartX) * 12 + theMiddleYIndex - theMiddleStartY] = theMap->mTerrainMesh.GetZ(theMiddleXIndex, theMiddleYIndex);
                }
            }
            theMiddle->mCanPlace = !theNewGate;
            theMiddle->mFootprint[0][0] = theMiddle->mCanPlace;
            theTransformation = theLeft->mTransformation;
            theLeft->SetAnimation(EETurnGatePart(theTransformation, ioDirection, 0));
            theLeft->mTransformation = theTransformation;
            theLeft->mTileX = theTiles[0].GetX();
            theLeft->mTileY = theTiles[0].mY;
            theTile = gWorld.GetMap()->GetTile(theLeft->mTileX, theLeft->mTileY);
            float theLeftX = (float)theLeft->mTileX + 0.5f;
            float theLeftY = (float)theLeft->mTileY + 0.5f;
            if (theTile == NULL || theTile->mTerrainType == kTerrainDeepWater || theTile->mTerrainType == kTerrainShallowWater)
                theZ = 0.0f;
            else
                theZ = gWorld.GetMap()->mTerrainMesh.GetMeshZ(theLeftX, theLeftY);
            theLeft->mTransformation.SetPosition(theLeftX, theLeftY, theZ);
            long theLeftStartX = theLeft->mTileX;
            long theLeftStartY = theLeft->mTileY;
            for (long theLeftXIndex = theLeftStartX; theLeftXIndex <= theLeftStartX + 1; theLeftXIndex++) {
                for (long theLeftYIndex = theLeftStartY; theLeftYIndex <= theLeftStartY + 1; theLeftYIndex++) {
                    theTile = theMap->GetTile(theLeftXIndex, theLeftYIndex);
                    if (theTile == NULL)
                        continue;
                    if (theTile->mTerrainType == kTerrainDeepWater || theTile->mTerrainType == kTerrainShallowWater)
                        theLeft->mHeights[(theLeftXIndex - theLeftStartX) * 12 + theLeftYIndex - theLeftStartY] = 0.0f;
                    else
                        theLeft->mHeights[(theLeftXIndex - theLeftStartX) * 12 + theLeftYIndex - theLeftStartY] = theMap->mTerrainMesh.GetZ(theLeftXIndex, theLeftYIndex);
                }
            }
            theLeft->mCanPlace = !theNewGate;
            theLeft->mFootprint[0][0] = theLeft->mCanPlace;
            theTransformation = theRight->mTransformation;
            theRight->SetAnimation(EETurnGatePart(theTransformation, ioDirection, 4));
            theRight->mTransformation = theTransformation;
            theRight->mTileX = theTiles[4].GetX();
            theRight->mTileY = theTiles[4].mY;
            theTile = gWorld.GetMap()->GetTile(theRight->mTileX, theRight->mTileY);
            float theRightX = (float)theRight->mTileX + 0.5f;
            float theRightY = (float)theRight->mTileY + 0.5f;
            if (theTile == NULL || theTile->mTerrainType == kTerrainDeepWater || theTile->mTerrainType == kTerrainShallowWater)
                theZ = 0.0f;
            else
                theZ = gWorld.GetMap()->mTerrainMesh.GetMeshZ(theRightX, theRightY);
            theRight->mTransformation.SetPosition(theRightX, theRightY, theZ);
            long theRightStartX = theRight->mTileX;
            long theRightStartY = theRight->mTileY;
            for (long theRightXIndex = theRightStartX; theRightXIndex <= theRightStartX + 1; theRightXIndex++) {
                for (long theRightYIndex = theRightStartY; theRightYIndex <= theRightStartY + 1; theRightYIndex++) {
                    theTile = theMap->GetTile(theRightXIndex, theRightYIndex);
                    if (theTile == NULL)
                        continue;
                    if (theTile->mTerrainType == kTerrainDeepWater || theTile->mTerrainType == kTerrainShallowWater)
                        theRight->mHeights[(theRightXIndex - theRightStartX) * 12 + theRightYIndex - theRightStartY] = 0.0f;
                    else
                        theRight->mHeights[(theRightXIndex - theRightStartX) * 12 + theRightYIndex - theRightStartY] = theMap->mTerrainMesh.GetZ(theRightXIndex, theRightYIndex);
                }
            }
            theRight->mCanPlace = !theNewGate;
            theRight->mFootprint[0][0] = theRight->mCanPlace;
        }
    }
    gWorld.mGhostUnitsLock->Leave();
}
