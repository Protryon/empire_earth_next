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
#include "ee/EEUnitKind.h"
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
#include "ee/EEUnitClassKinds.h"
#include "lle/GEViewport.h"
#include "ee/EEUnitPicker.h"
#include "ee/EEAEManager.h"
#include "ee/EEUUUnitFunctor.h"
#include "ee/EEUnitUtilities.h"
#include "rust/sim/units.h"
bool gGhostUnitsChanged;                    // 0x563240 (.bss after EEComplexUnit's, before EEGoalQueueContainer's; ee/EEGhostUnit.h)

long EETurnGatePart(GETransformation&, long, long);
void EEGetWallGateTiles(const U2DPoint<long>&, long, std::vector<U2DPoint<long> >&);
bool EEIsLandTerrain(long);
bool EEIsSpaceDockSiteOpen(long, long, EEPlayer*);
extern GEViewport gViewport;

// .rdata 0x446dc0: the four facing yaws (docks, robotic farm pieces) (provisional name)
static const float kWallRotations[4] = {0.0f, 3.1415927f, -1.5707964f, 1.5707964f};

template <class T> static T* EEGhostObject(uint64_t inHandle)
{
    return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle));
}

static int64_t EEGhostBits(float inValue)
{
    uint32_t theBits;
    memcpy(&theBits, &inValue, sizeof theBits);
    return theBits;
}

static float EEGhostFloat(int64_t inBits)
{
    uint32_t theBits = (uint32_t)inBits;
    float theValue;
    memcpy(&theValue, &theBits, sizeof theValue);
    return theValue;
}


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

// Fills a space building's footprint and heights: placeable when its space dock site is open or every
// inner cell is, with 2+ land rim tiles; then turns it to the space dock yaw or the dock facing
void EEGhostUnit::FillDockSquare()
{
    ee_units_ghost_body(EE_UNITS_GHOST_DO_FILL_DOCK_SQUARE, this, NULL, NULL);   // ghost_body.rs
}

// Fills a dock's footprint and heights: placeable when every inner cell is and at least 3 rim tiles are land
void EEGhostUnit::FillRimSquare()
{
    ee_units_ghost_body(EE_UNITS_GHOST_DO_FILL_RIM_SQUARE, this, NULL, NULL);   // ghost_body.rs
}

// Marks a mining building's whole footprint placeable when a unit of its overlap set stands on tile (x-1, y-1)
void EEGhostUnit::UpdateFootprint()
{
    ee_units_ghost_body(EE_UNITS_GHOST_DO_UPDATE_FOOTPRINT, this, NULL, NULL);   // ghost_body.rs
}

// Fills the ghost's footprint and heights and whether it can be placed; mining, dock, space, volcano have variants
void EEGhostUnit::FillSquare()
{
    ee_units_ghost_body(EE_UNITS_GHOST_DO_FILL_SQUARE, this, NULL, NULL);   // ghost_body.rs
}

// Fills a volcano's footprint and heights: each cell unblocked, off the map edge and free of area effect 3
void EEGhostUnit::FillVolcanoSquare()
{
    ee_units_ghost_body(EE_UNITS_GHOST_DO_FILL_VOLCANO_SQUARE, this, NULL, NULL);   // ghost_body.rs
}

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

// Replaces the placement ghosts with a unit's ghost, plus 3 runway ghosts for an airport (0x42) and the
// farm ghosts around a granary unless it is being moved
void EEGhostUnit::Create(const GE2DPoint& inLocation, long inUnitType, EEPlayer* inPlayer, float inRotation,
                         bool inIsMove)
{
    ee_units_ghost_create(inUnitType, inPlayer, inRotation, inIsMove);   // ghost_make.rs
}

// Replaces the placement ghosts with a gate's three parts (left, middle, right), each turned as its part
void EEGhostUnit::CreateGate(float inRotation, EEUnitClass* inClass, EEPlayer* inPlayer)
{
    ee_units_ghost_create_gate(inRotation, inClass, inPlayer);   // ghost_make.rs
}

// Replaces the placement ghosts with one wall ghost per planned tile, shaped by its corner and set on the ground
// (free function; whether it was static is not known) (provisional name)
void EEBuildWallGhosts(std::vector<EETileCoordinate>& inTiles, std::vector<long>& inCorners,
                long inTechnology, EEPlayer* inPlayer, float inRotation)
{
    EeUnitsGhostWalls theArgs = {&inTiles, &inCorners, inTechnology, inPlayer, inRotation};
    ee_units_ghost_build_walls(&theArgs);   // ghost_make.rs
}

// Adds the ghost's player number and unit class name to the sync checksum
void EEGhostUnit::AddChecksum(ESChecksum& ioChecksum)
{
    UString theText;
    sprintf(theText, "The Ghost Unit Player Number was: %ld", this->mPlayer->mPlayerID);
    ioChecksum.AddChecksum(theText, 0);
    sprintf(theText, "The Object Type was %s", (const char*)this->mUnitClass->mName);
    ioChecksum.AddChecksum(theText, 1);
}

// Returns the half-width of the footprint square around the ghost's tile (1 for a one-tile unit)
float EEGhostUnit::GetFootprintOffset()
{
    return EEGhostFloat(ee_units_ghost_body(EE_UNITS_GHOST_DO_FOOTPRINT_OFFSET, this, NULL, NULL));   // ghost_body.rs
}

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

// Writes the unit type and player id to the saved game; returns the first write error
long EEGhostUnit::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    unsigned long theType = this->mUnitClass->GetUnitType();
    long theResult = outFile.Write(theType);
    if (theResult == 0)
        theResult = outFile.Write(this->mPlayer->GetPlayerID());
    return theResult;
}

// Sets the ghost's animation and its frame to that animation's first
void EEGhostUnit::SetAnimation(long inIndex)
{
    this->mFrame = this->GetModel()->mStartFrames[inIndex];
    this->mAnimation = inIndex;
}

// Moves the placement ghosts under the cursor and rechecks where each can be placed; an airport turns
// to its facing quarter with its runways behind it, farm ghosts take their slots
void EEPreviewPlacement(const GE2DPoint& inLocation)
{
    ee_units_ghost_body(EE_UNITS_GHOST_DO_PREVIEW_PLACEMENT, NULL, &inLocation, NULL);   // ghost_body.rs
}

// Moves the gate ghost's three parts onto the wall tiles under the cursor; placeable only over a marked
// wall piece, whose gate direction it takes
void EEGhostUnit::PreviewGate(const GE2DPoint& inLocation, long& ioDirection)
{
    ee_units_ghost_body(EE_UNITS_GHOST_DO_PREVIEW_GATE, NULL, &inLocation, &ioDirection);   // ghost_body.rs
}

// ghost_body.rs's scratch values: the fills' position (their thePosition), the preview's position and tile, the gate
// preview's cursor tile, tiles and turned transformation (the originals' locals)
static GE3DPoint sGhostFillPosition;
static GE3DPoint sGhostPosition;
static EETile* sGhostTile;
static long sGhostXY[2];
static std::vector<U2DPoint<long> > sGhostPoints;
static GETransformation sGhostTransform(false);
static bool sGhostPieceFlag;

// a GE3DPoint's x, y or z (0, 1, 2)
static float& EEGhostPart(GE3DPoint& inPoint, int64_t inIndex)
{
    return inIndex == 0 ? inPoint.mX : inIndex == 1 ? inPoint.mY : inPoint.mZ;
}

// The C++ of EEGhostUnit's Rust bodies (ee_units ghost_body.rs; EE_UNITS_GHOST_*): one statement of the originals
// each, in their order; inGhost the ghost acted on, inA an object or a value, inB and inC values (floats as bits)
extern "C" int64_t ee_units_ghost_host(int32_t inOp, void* inGhost, uint64_t inA, int64_t inB, int64_t inC)
{
    EEGhostUnit* theGhost = static_cast<EEGhostUnit*>(inGhost);
    EEMap* theMap = gWorld.GetMap();
    EEUnit* theUnit = EEGhostObject<EEUnit>(inA);
    RTSTileUnitNode* theNode = EEGhostObject<RTSTileUnitNode>(inA);
    const GE2DPoint* theLocation = EEGhostObject<const GE2DPoint>(inA);
    switch (inOp) {
    case EE_UNITS_GHOST_UNIT_TYPE: return theGhost->mUnitClass->GetUnitType();
    case EE_UNITS_GHOST_CATEGORY: return theGhost->mUnitClass->mCategory;
    case EE_UNITS_GHOST_TILE_SIZE: return theGhost->mUnitClass->GetTileSize();
    case EE_UNITS_GHOST_TILE_X: return theGhost->mTileX;
    case EE_UNITS_GHOST_TILE_Y: return theGhost->mTileY;
    case EE_UNITS_GHOST_SET_TILE_X: theGhost->mTileX = (long)inB; return 0;
    case EE_UNITS_GHOST_SET_TILE_Y: theGhost->mTileY = (long)inB; return 0;
    case EE_UNITS_GHOST_UM_CEIL: return EEGhostBits(UMCeil(EEGhostFloat(inB)));
    case EE_UNITS_GHOST_POSITION_FROM_TRANSFORM: sGhostFillPosition = GE3DPoint(theGhost->mTransformation); return 0;
    case EE_UNITS_GHOST_CAN_PLACE_AT:
        return EECanPlace(theGhost->mPlayer, theGhost->mUnitClass->GetUnitType(), &sGhostFillPosition, (long)inB, (long)inC, 0, 0);
    case EE_UNITS_GHOST_TILE_EXISTS: return theMap->GetTile((long)inB, (long)inC) != NULL;
    case EE_UNITS_GHOST_TILE_TERRAIN: return theMap->GetTile((long)inB, (long)inC)->mTerrainType;
    case EE_UNITS_GHOST_TERRAIN_Z: return EEGhostBits(theMap->mTerrainMesh.GetZ((long)inB, (long)inC));
    case EE_UNITS_GHOST_SET_HEIGHT: theGhost->mHeights[inB] = EEGhostFloat(inC); return 0;
    case EE_UNITS_GHOST_SET_FOOTPRINT: theGhost->mFootprint[inB][inC] = inA != 0; return 0;
    case EE_UNITS_GHOST_IS_LAND_TERRAIN: return EEIsLandTerrain((long)inB);
    case EE_UNITS_GHOST_SPACE_DOCK_SITE_OPEN: return EEIsSpaceDockSiteOpen(theGhost->mTileX, theGhost->mTileY, theGhost->mPlayer);
    case EE_UNITS_GHOST_DOCK_FACING: return EEGetDockFacing(theGhost->mTileX, theGhost->mTileY);
    case EE_UNITS_GHOST_SPACE_DOCK_YAW:
        return EEGhostBits(EEGetSpaceDockYaw(theGhost->mTileX, theGhost->mTileY, theGhost->mUnitClass->GetTileSize()));
    case EE_UNITS_GHOST_SET_CAN_PLACE: theGhost->mCanPlace = inB != 0; return 0;
    case EE_UNITS_GHOST_CAN_PLACE: return theGhost->mCanPlace;
    case EE_UNITS_GHOST_SET_YAW: theGhost->mTransformation.SetOrientationYPR(EEGhostFloat(inB), 0.0f, 0.0f); return 0;
    case EE_UNITS_GHOST_TILE_FIRST_NODE:
        return theMap->GetTile((long)inB, (long)inC) != NULL ? (uint64_t)(uintptr_t)theMap->GetTile((long)inB, (long)inC)->mUnits : 0;
    case EE_UNITS_GHOST_NODE_NEXT: return (uint64_t)(uintptr_t)theNode->mNext;
    case EE_UNITS_GHOST_NODE_IN_SET:
        return theNode->mUnit != NULL && gUnitSetManager.Contains(theNode->mUnit->mUnitClass->GetUnitType(), theGhost->mUnitSet);
    case EE_UNITS_GHOST_TILE_UNBLOCKED:
        return ee_units_uu_is_tile_unblocked((uint64_t)(uintptr_t)theGhost->mPlayer, theGhost->mUnitClass->GetUnitType(),
                                             (uint64_t)(uintptr_t)theMap->GetTile((long)inB, (long)inC));
    case EE_UNITS_GHOST_IS_EDGE_TILE: return theMap->IsEdgeTile((long)inB, (long)inC);
    case EE_UNITS_GHOST_HAS_AREA_EFFECT_3: return gAEManager.HasAreaEffect2(3, (long)inB, (long)inC, theGhost->mPlayer->mPlayerID);
    case EE_UNITS_GHOST_LOCK_ENTER: gWorld.mGhostUnitsLock->Enter(); return 0;
    case EE_UNITS_GHOST_LOCK_LEAVE: gWorld.mGhostUnitsLock->Leave(); return 0;
    case EE_UNITS_GHOST_VIEW_CONTAINS: return gViewport.GetViewRectangle().Contains(*theLocation);
    case EE_UNITS_GHOST_NUM_GHOSTS: return (int64_t)gWorld.GetNumGhostUnits();
    case EE_UNITS_GHOST_SET_CHANGED: gGhostUnitsChanged = inB != 0; return 0;
    case EE_UNITS_GHOST_GHOST_AT: return (uint64_t)(uintptr_t)gWorld.mGhostUnits[(size_t)inB];
    case EE_UNITS_GHOST_POINT_UNIT_OR_TILE: return EEPointUnitOrTile(*theLocation, sGhostTile, sGhostPosition, true);
    case EE_UNITS_GHOST_POSITION: return EEGhostBits(EEGhostPart(sGhostPosition, inB));
    case EE_UNITS_GHOST_SET_POSITION_PART: EEGhostPart(sGhostPosition, inB) = EEGhostFloat(inC); return 0;
    case EE_UNITS_GHOST_GET_MODEL: theGhost->GetModel(); return 0;
    case EE_UNITS_GHOST_AIRPORT_TILE_SIZE: return EEUnitClassOfKind<EE_CLASS_KIND_BUILDING>(gUnitClassTable[kUnitTypeAirport])->GetTileSize();
    case EE_UNITS_GHOST_FACING_QUARTER: return EEGetFacingQuarter((long)inB, (long)inC);
    case EE_UNITS_GHOST_SET_FARM_SLOT: EEGhostUnit::SetFarmSlotTile(theGhost, (long)inB); return 0;
    case EE_UNITS_GHOST_IS_LAND_BUILDING: return theGhost->mUnitClass->IsLandBuilding();
    case EE_UNITS_GHOST_IS_COMPLEX: return theGhost->mUnitClass->mIsComplex;
    case EE_UNITS_GHOST_SET_POSITION: theGhost->mTransformation.SetPosition(sGhostPosition); return 0;
    case EE_UNITS_GHOST_LOCATION_TO_TILE: theGhost->mUnitClass->LocationToTile(sGhostPosition, theGhost->mTileX, theGhost->mTileY); return 0;
    case EE_UNITS_GHOST_PICK_UNIT: return (uint64_t)(uintptr_t)gUnitPicker.PickUnit(*theLocation, 0);
    case EE_UNITS_GHOST_UNIT_IS_WALL: return theUnit->mUnitClass->mIsWall;
    case EE_UNITS_GHOST_POINT_TILE_XY: EEPointUnitOrTileXY(*theLocation, sGhostTile, sGhostXY[0], sGhostXY[1], true); return 0;
    case EE_UNITS_GHOST_SCRATCH_XY: return sGhostXY[inB];
    case EE_UNITS_GHOST_UNIT_TILE: return inB == 0 ? theUnit->mTileX : theUnit->mTileY;
    case EE_UNITS_GHOST_UNIT_DEREFERENCE: theUnit->Dereference(); return 0;
    case EE_UNITS_GHOST_NODE_UNIT: return (uint64_t)(uintptr_t)theNode->mUnit;
    case EE_UNITS_GHOST_UNIT_TINTED: return theUnit->mComplexUnit != NULL && theUnit->mComplexUnit->mIsTinted;
    case EE_UNITS_GHOST_UNIT_GATE_DIRECTION: return theUnit->mComplexUnit->mGateDirection;
    case EE_UNITS_GHOST_SCRATCH_POINTS_CLEAR: sGhostPoints.clear(); return 0;
    case EE_UNITS_GHOST_GATE_TILES: EEGetWallGateTiles(U2DPoint<long>((long)inB, (long)inC), (long)inA, sGhostPoints); return 0;
    case EE_UNITS_GHOST_SCRATCH_POINT: return inC == 0 ? sGhostPoints[(size_t)inB].GetX() : sGhostPoints[(size_t)inB].mY;
    case EE_UNITS_GHOST_SCRATCH_TRANSFORM_GET: sGhostTransform = theGhost->mTransformation; return 0;
    case EE_UNITS_GHOST_TURN_GATE_PART: theGhost->SetAnimation(EETurnGatePart(sGhostTransform, (long)inB, (long)inC)); return 0;
    case EE_UNITS_GHOST_SCRATCH_TRANSFORM_SET: theGhost->mTransformation = sGhostTransform; return 0;
    case EE_UNITS_GHOST_MESH_Z: return EEGhostBits(theMap->mTerrainMesh.GetMeshZ(EEGhostFloat(inB), EEGhostFloat(inC)));
    case EE_UNITS_GHOST_SET_POSITION_XYZ:
        theGhost->mTransformation.SetPosition(EEGhostFloat(inB), EEGhostFloat(inC), EEGhostFloat((int64_t)inA));
        return 0;
    case EE_UNITS_GHOST_FOOTPRINT_FROM_CAN_PLACE: theGhost->mFootprint[0][0] = theGhost->mCanPlace; return 0;
    case EE_UNITS_GHOST_POSITION_RESET: sGhostPosition = GE3DPoint(); return 0;
    // ghost_make.rs's
    case EE_UNITS_GHOST_CLASS_AT: return (uint64_t)(uintptr_t)gUnitClassTable[inB];
    case EE_UNITS_GHOST_CLEAR_GHOSTS: gWorld.ClearGhostUnits(); return 0;
    case EE_UNITS_GHOST_NEW:
        return (uint64_t)(uintptr_t)new EEGhostUnit(EEGhostObject<EEUnitClass>(inA)->GetUnitType(), EEGhostObject<EEPlayer>((uint64_t)inB), inC != 0);
    case EE_UNITS_GHOST_CLASS_FIXED: return EEGhostObject<EEUnitClass>(inA)->mMaxAngularVelocity == 0.0f;
    case EE_UNITS_GHOST_ADD: gWorld.AddGhostUnit(theGhost); return 0;
    case EE_UNITS_GHOST_CLASS_TYPE: return EEGhostObject<EEUnitClass>(inA)->GetUnitType();
    case EE_UNITS_GHOST_FARM_TYPE: return EEGetFarmType(EEGhostObject<EEPlayer>(inA));
    case EE_UNITS_GHOST_CLASS_TILE_SIZE: return EEGhostObject<EEUnitClass>(inA)->GetTileSize();
    case EE_UNITS_GHOST_WALL_PIECE_PART: return EEGetWallPiecePart((long)inB, &sGhostPieceFlag);
    case EE_UNITS_GHOST_PIECE_FLAG: return sGhostPieceFlag;
    case EE_UNITS_GHOST_SET_ANIMATION: theGhost->SetAnimation((long)inB); return 0;
    case EE_UNITS_GHOST_CLASS_CATEGORY: return EEGhostObject<EEUnitClass>(inA)->mCategory;
    case EE_UNITS_GHOST_SET_OVERLAP_SET: theGhost->mUnitSet = EEGhostObject<EEUnitClass>(inA)->mOverlapUnitSet; return 0;
    case EE_UNITS_GHOST_GATE_CLASS: return (uint64_t)(uintptr_t)EEUnitClassOfKind<EE_CLASS_KIND_GATE>(EEGhostObject<EEUnitClass>(inA));
    case EE_UNITS_GHOST_TECH_CLASS:
        return (uint64_t)(uintptr_t)gUnitClassTable[EEGhostObject<EEPlayer>(inA)->mTechnologies->Get((long)inB)->mData->mUnitClass];
    case EE_UNITS_GHOST_TILES_SIZE: return (int64_t)EEGhostObject<std::vector<EETileCoordinate> >(inA)->size();
    case EE_UNITS_GHOST_TILES_AT:
        return inC == 0 ? (*EEGhostObject<std::vector<EETileCoordinate> >(inA))[(size_t)inB].GetX() : (*EEGhostObject<std::vector<EETileCoordinate> >(inA))[(size_t)inB].mY;
    case EE_UNITS_GHOST_RAISED_CORNERS: return gWorld.GetMap()->GetNumRaisedCorners((long)inB, (long)inC);
    case EE_UNITS_GHOST_CORNER_AT: return (*EEGhostObject<std::vector<long> >(inA))[(size_t)inB];
    case EE_UNITS_GHOST_ORIENT_WALL: EEOrientWallGhost(theGhost, (long)inB, (long)inC); return 0;
    case EE_UNITS_GHOST_SET_X: theGhost->mTransformation.SetX(EEGhostFloat(inB)); return 0;
    case EE_UNITS_GHOST_SET_Y: theGhost->mTransformation.SetY(EEGhostFloat(inB)); return 0;
    case EE_UNITS_GHOST_SET_Z: theGhost->mTransformation.SetZ(EEGhostFloat(inB)); return 0;
    default: return 0;
    }
}
