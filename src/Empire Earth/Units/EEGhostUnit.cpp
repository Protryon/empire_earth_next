// C:\EEX\Empire Earth\Units\EEGhostUnit.cpp (file name inferred: no __FILE__; the class name). A unit placed but not yet
// built (RTTI EEGhostUnit, vtable 0x446ef4): 0x20360e..0x20560c. Unit flags: the exe's module set. No init chain.

#include <windows.h>
#include <stdio.h>

#include "lle/LowLevelEngine.h"
#include "lle/GETerrainMesh.h"
#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEDataPreLoading.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EEWheel.h"
#include "ee/EEGhostUnit.h"
#include "ee/EEUnitClassKinds.h"
#include "lle/GEViewport.h"
#include "ee/EEUnitPicker.h"
#include "ee/EEAEManager.h"
#include "ee/EEUnitUtilities.h"
bool gGhostUnitsChanged;                    // 0x563240 (.bss after EEComplexUnit's, before EEGoalQueueContainer's; ee/EEGhostUnit.h)

long EETurnGatePart(GETransformation&, long, long);
void EEGetWallGateTiles(const U2DPoint<long>&, long, std::vector<U2DPoint<long> >&);
bool EEIsLandTerrain(long);
bool EEIsSpaceDockSiteOpen(long, long, EEPlayer*);
extern GEViewport gViewport;

template <class T> static T* EEGhostObject(uint64_t inHandle)
{
    return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle));
}

// Replaces the placement ghosts with one wall ghost per planned tile, shaped by its corner and set on the ground
// (free function; whether it was static is not known) (provisional name)
void EEBuildWallGhosts(std::vector<EETileCoordinate>& inTiles, std::vector<long>& inCorners,
                long inTechnology, EEPlayer* inPlayer, float inRotation)
{
    EeUnitsGhostWalls theArgs = {&inTiles, &inCorners, inTechnology, inPlayer, inRotation};
    ee_units_ghost_build_walls(&theArgs);   // ghost_make.rs
}

// Moves the placement ghosts under the cursor and rechecks where each can be placed; an airport turns
// to its facing quarter with its runways behind it, farm ghosts take their slots
void EEPreviewPlacement(const GE2DPoint& inLocation)
{
    ee_units_ghost_body(EE_UNITS_GHOST_DO_PREVIEW_PLACEMENT, NULL, &inLocation, NULL);   // ghost_body.rs
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
static FSFile* sGhostFile;
static unsigned long sGhostValue;
static UString sGhostText;

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
    case EE_UNITS_GHOST_UM_CEIL: return UFloatBits(UMCeil(UBitsFloat((uint32_t)(inB))));
    case EE_UNITS_GHOST_POSITION_FROM_TRANSFORM: sGhostFillPosition = GE3DPoint(theGhost->mTransformation); return 0;
    case EE_UNITS_GHOST_CAN_PLACE_AT:
        return EECanPlace(theGhost->mPlayer, theGhost->mUnitClass->GetUnitType(), &sGhostFillPosition, (long)inB, (long)inC, 0, 0);
    case EE_UNITS_GHOST_TILE_EXISTS: return theMap->GetTile((long)inB, (long)inC) != NULL;
    case EE_UNITS_GHOST_TILE_TERRAIN: return theMap->GetTile((long)inB, (long)inC)->mTerrainType;
    case EE_UNITS_GHOST_TERRAIN_Z: return UFloatBits(theMap->mTerrainMesh.GetZ((long)inB, (long)inC));
    case EE_UNITS_GHOST_SET_HEIGHT: theGhost->mHeights[inB] = UBitsFloat((uint32_t)(inC)); return 0;
    case EE_UNITS_GHOST_SET_FOOTPRINT: theGhost->mFootprint[inB][inC] = inA != 0; return 0;
    case EE_UNITS_GHOST_IS_LAND_TERRAIN: return EEIsLandTerrain((long)inB);
    case EE_UNITS_GHOST_SPACE_DOCK_SITE_OPEN: return EEIsSpaceDockSiteOpen(theGhost->mTileX, theGhost->mTileY, theGhost->mPlayer);
    case EE_UNITS_GHOST_DOCK_FACING: return EEGetDockFacing(theGhost->mTileX, theGhost->mTileY);
    case EE_UNITS_GHOST_SPACE_DOCK_YAW:
        return UFloatBits(EEGetSpaceDockYaw(theGhost->mTileX, theGhost->mTileY, theGhost->mUnitClass->GetTileSize()));
    case EE_UNITS_GHOST_SET_CAN_PLACE: theGhost->mCanPlace = inB != 0; return 0;
    case EE_UNITS_GHOST_CAN_PLACE: return theGhost->mCanPlace;
    case EE_UNITS_GHOST_SET_YAW: theGhost->mTransformation.SetOrientationYPR(UBitsFloat((uint32_t)(inB)), 0.0f, 0.0f); return 0;
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
    case EE_UNITS_GHOST_POSITION: return UFloatBits(EEGhostPart(sGhostPosition, inB));
    case EE_UNITS_GHOST_SET_POSITION_PART: EEGhostPart(sGhostPosition, inB) = UBitsFloat((uint32_t)(inC)); return 0;
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
    case EE_UNITS_GHOST_MESH_Z: return UFloatBits(theMap->mTerrainMesh.GetMeshZ(UBitsFloat((uint32_t)(inB)), UBitsFloat((uint32_t)(inC))));
    case EE_UNITS_GHOST_SET_POSITION_XYZ:
        theGhost->mTransformation.SetPosition(UBitsFloat((uint32_t)(inB)), UBitsFloat((uint32_t)(inC)), UBitsFloat((uint32_t)((int64_t)inA)));
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
    case EE_UNITS_GHOST_SET_X: theGhost->mTransformation.SetX(UBitsFloat((uint32_t)(inB))); return 0;
    case EE_UNITS_GHOST_SET_Y: theGhost->mTransformation.SetY(UBitsFloat((uint32_t)(inB))); return 0;
    case EE_UNITS_GHOST_SET_Z: theGhost->mTransformation.SetZ(UBitsFloat((uint32_t)(inB))); return 0;
    // ghost_more.rs's
    case EE_UNITS_GHOST_M_SET_PLAYER: theGhost->mPlayer = EEGhostObject<EEPlayer>((uint64_t)inB); return 0;
    case EE_UNITS_GHOST_M_SET_CLASS: theGhost->mUnitClass = EEGhostObject<EEUnitClass>((uint64_t)inB); return 0;
    case EE_UNITS_GHOST_M_FREE_CAMERA: return gFreeCamera;
    case EE_UNITS_GHOST_M_LOAD_CLASS_MEDIA: EEGhostObject<EEPlayer>(inA)->LoadUnitClassMedia((long)inB); return 0;
    case EE_UNITS_GHOST_M_SET_CLASS_MEDIA: theGhost->mPlayer->SetUnitClassMedia((long)inB, 0x7f); return 0;
    case EE_UNITS_GHOST_M_POST_LOAD: gDataPreLoading->PostLoadUnitClass(EEGhostObject<EEPlayer>(inA)->mPlayerID, (long)inB); return 0;
    case EE_UNITS_GHOST_M_SET_NO_MEDIA: theGhost->mNoMedia = inB != 0; return 0;
    case EE_UNITS_GHOST_M_NO_MEDIA: return theGhost->mNoMedia;
    case EE_UNITS_GHOST_M_SHUTTING_DOWN: return UShutdownInProgress() == true;
    case EE_UNITS_GHOST_M_POST_UNLOAD: gDataPreLoading->PostUnloadUnitClass(theGhost->mPlayer->mPlayerID, theGhost->mUnitClass->GetUnitType()); return 0;
    case EE_UNITS_GHOST_M_UNLOAD_CLASS_MEDIA: theGhost->mPlayer->UnloadUnitClassMedia(theGhost->mUnitClass->GetUnitType()); return 0;
    case EE_UNITS_GHOST_M_SET_FRAME: theGhost->mFrame = (long)inB; return 0;
    case EE_UNITS_GHOST_M_SET_ANIMATION_INDEX: theGhost->mAnimation = (long)inB; return 0;
    case EE_UNITS_GHOST_M_CLEAR_FOOTPRINT: memset(theGhost->mFootprint, 0, sizeof(theGhost->mFootprint)); return 0;
    case EE_UNITS_GHOST_M_CLEAR_HEIGHTS: memset(theGhost->mHeights, 0, sizeof(theGhost->mHeights)); return 0;
    case EE_UNITS_GHOST_M_SET_FILE: sGhostFile = EEGhostObject<FSFile>(inA); return 0;
    case EE_UNITS_GHOST_M_READ_ULONG: return sGhostFile->Read(sGhostValue);
    case EE_UNITS_GHOST_M_VALUE: return sGhostValue;
    case EE_UNITS_GHOST_M_WRITE_ULONG: return sGhostFile->Write((unsigned long)inB);
    case EE_UNITS_GHOST_M_WRITE_PLAYER_ID: return sGhostFile->Write(theGhost->mPlayer->GetPlayerID());
    case EE_UNITS_GHOST_M_START_FRAME: return theGhost->GetModel()->mStartFrames[inB];
    case EE_UNITS_GHOST_M_CHECKSUM_PLAYER_TEXT: sprintf(sGhostText, "The Ghost Unit Player Number was: %ld", theGhost->mPlayer->mPlayerID); return 0;
    case EE_UNITS_GHOST_M_CHECKSUM_CLASS_TEXT: sprintf(sGhostText, "The Object Type was %s", (const char*)theGhost->mUnitClass->mName); return 0;
    case EE_UNITS_GHOST_M_CHECKSUM_ADD: EEGhostObject<ESChecksum>(inA)->AddChecksum(sGhostText, (long)inB); return 0;
    default: return 0;
    }
}
