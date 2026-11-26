// C:\EEX\Empire Earth\Units\EECloakingUtilities.cpp (file name inferred: no __FILE__, no RTTI). Unit helpers
// between EEWheel.cpp and RTSUnit.cpp: 0x23ceca..0x23d7c0 (unit map [C]; edge: a globals chain of one function). The
// cloaked units' opacity (Fn23CEE2, the render lists' and the farm's), the cloaking effect (EEAddCloakEffect/EERemoveCloakEffect), the
// space docks' yaws and sites (EEGetSpaceDockYaw, EEIsSpaceDockSiteOpen) and the space units' planets (EEGetLandContinentOf
// and its mPlanetID pass). Unit flags: the exe's module set (provisional names).
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GETerrainMesh.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEUnitClassKinds.h"
#include "rust/sim/units.h"

// 0x2035ef (EEGhostUnit.cpp's spelling): the terrain kind blocks a wall piece's ground
bool EEIsLandTerrain(long inTerrainType);

// (the cloaked opacity range and the wall pieces' yaws, kWallRotations: ee_units uu_cloak.rs)

// ---- the C++ of the cloaking utilities' Rust bodies (ee_units uu_cloak.rs; EE_UNITS_CK_*): one statement of the
// original bodies each; inA an object, inC, inD values, ioF floats

static EEEffectHandle sCkHandle;   // EEAddCloakEffect's new effect

template <class T> static T* CkObj(uint64_t inHandle) { return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle)); }

extern "C" int64_t ee_units_ck_host(int32_t inOp, uint64_t inA, int64_t inC, int64_t inD, float (*ioF)[4])
{
    switch (inOp) {
    case EE_UNITS_CK_TERRAIN: return gWorld.mMap->GetTile((long)inC, (long)inD) != NULL ? gWorld.mMap->GetTile((long)inC, (long)inD)->mTerrainType : -1;
    case EE_UNITS_CK_IS_EDGE: return gWorld.mMap->IsEdgeTile((long)inC, (long)inD);
    case EE_UNITS_CK_IS_LAND: return EEIsLandTerrain((long)inC);
    case EE_UNITS_CK_CAN_PLACE_DOCK: return EECanPlace(CkObj<EEPlayer>(inA), kUnitTypeSpaceDock, reinterpret_cast<GE3DPoint*>((*ioF)), (long)inC, (long)inD, 0, 0);
    case EE_UNITS_CK_DOCK_FACING: return (long)ee_units_ucn_facing(0, (int32_t)(unsigned long)((long)inC), (int32_t)(unsigned long)((long)inD));
    case EE_UNITS_CK_COMMAND_TYPE: return CkObj<EEComplexUnit>(inA)->GetCommandType();
    case EE_UNITS_CK_ROT_FRACTION: return (int64_t)UFloatBits((float)CkObj<EEComplexUnit>(inA)->mComplexUnit->mRottingAwayTime / (long)CkObj<EEUnit>(inA)->mUnitClass->mRottingTime);
    case EE_UNITS_CK_HAS_ROTTING_TIME: return (long)CkObj<EEUnit>(inA)->mUnitClass->mRottingTime != 0;
    case EE_UNITS_CK_IS_CLOAKABLE: return CkObj<EEComplexUnit>(inA)->mIsCloakable;
    case EE_UNITS_CK_IS_CLOAKED: return CkObj<EEComplexUnit>(inA)->mIsCloaked;
    case EE_UNITS_CK_FREE_CAMERA: return gFreeCamera;
    case EE_UNITS_CK_OPACITY: return (int64_t)UFloatBits(CkObj<EEComplexUnit>(inA)->mOpacity);
    case EE_UNITS_CK_CURRENT_PLAYER: return (int64_t)(uintptr_t)gWorld.GetCurrentPlayer();
    case EE_UNITS_CK_PLAYER: return (int64_t)(uintptr_t)CkObj<EEUnit>(inA)->mPlayer;
    case EE_UNITS_CK_ENEMY_OF_VIEWER: return CkObj<EEUnit>(inA)->mPlayer->mEnemies[gWorld.GetCurrentPlayer()->mPlayerID];
    case EE_UNITS_CK_OBSERVER: return gWorld.GetCurrentPlayer()->mObserverMode;
    case EE_UNITS_CK_SET_FLOAT: *CkObj<float>(inA) = (*ioF)[0]; return 0;
    case EE_UNITS_CK_CREATE_CLOAK: return gWorld.GetEffectManager()->CreateUnitEffect(CkObj<EEUnit>(inA), EEGraphicEffect::Get(kGraphicEffectMarketTicketTape), &sCkHandle, false, 0, 0);
    case EE_UNITS_CK_KEEP_CLOAK: EEUnitCast<EEComplexUnit>(CkObj<EEUnit>(inA))->mCloaked = sCkHandle.mValue; return 0;
    case EE_UNITS_CK_CLOAKED: return (long)EEUnitCast<EEComplexUnit>(CkObj<EEUnit>(inA))->mCloaked;
    case EE_UNITS_CK_DESTROY_EFFECT: gWorld.GetEffectManager()->DestroyEffect((sCkHandle.mValue = (decltype(sCkHandle.mValue))inC, sCkHandle)); return 0;
    case EE_UNITS_CK_CLEAR_CLOAKED: EEUnitCast<EEComplexUnit>(CkObj<EEUnit>(inA))->mCloaked = 0; return 0;
    case EE_UNITS_CK_SPACE_MAP: return gWorld.mMap->mTerrainMesh.GetIsSpaceMap();
    case EE_UNITS_CK_TILE_X: return CkObj<EEComplexUnit>(inA)->mTileX;
    case EE_UNITS_CK_TILE_Y: return CkObj<EEComplexUnit>(inA)->mTileY;
    case EE_UNITS_CK_CONTINENT: return gContinentManager.GetContinent(EETileCoordinate((long)inC, (long)inD), kAIPathNonSpace);
    case EE_UNITS_CK_FIRST_UNIT: return (int64_t)(uintptr_t)gWorld.mUnits.mFirst;
    case EE_UNITS_CK_NEXT_UNIT: return (int64_t)(uintptr_t)CkObj<RTSUnit>(inA)->mTileNode->mNext;
    case EE_UNITS_CK_COMPLEX_OF: return (int64_t)(uintptr_t)static_cast<EEUnit*>(CkObj<RTSUnit>(inA))->mComplexUnit;
    case EE_UNITS_CK_DOMAINS: return CkObj<EEComplexUnit>(inA)->mUnitClass->mDomains;
    case EE_UNITS_CK_UNIT_TYPE: return CkObj<EEComplexUnit>(inA)->mUnitClass->mUnitType;
    case EE_UNITS_CK_SET_PLANET: CkObj<EEComplexUnit>(inA)->mPlanetID = (unsigned short)inC; return 0;
    }
    return 0;
}

// Returns whether a unit is drawn translucent and sets its opacity: the rotting fade, or its cloak opacity (dimmed and
// clamped for an enemy viewer); a translucent unit that is not cloaked is left as it is
bool Fn23CEE2(EEComplexUnit* inUnit, bool inTranslucent, float& ioOpacity)
{
    return ee_units_ck_body(EE_UNITS_CK_DO_TRANSLUCENT, (uint64_t)(uintptr_t)inUnit, (uint64_t)(uintptr_t)&ioOpacity, inTranslucent, 0) != 0;   // uu_cloak.rs
}

// Returns a space dock's yaw from which of the 2 x 2 tiles at (inX, inY) are space (one or two space tiles), else the
// yaw of EEGetDockFacing
float EEGetSpaceDockYaw(long inX, long inY, long inSize) { return UBitsFloat(ee_units_ck_body(EE_UNITS_CK_DO_SPACE_DOCK_YAW, 0, 0, inX, inY)); }  // uu_cloak.rs

// Returns whether a space dock (unit type 0x2d8) fits at (inX, inY): its inner 2 x 2 tiles placeable, its border on the
// map with at least two land tiles, and its inner land/space mix fitting a dock
bool EEIsSpaceDockSiteOpen(long inX, long inY, EEPlayer* inPlayer) { return ee_units_ck_body(EE_UNITS_CK_DO_SPACE_DOCK_SITE_OPEN, (uint64_t)(uintptr_t)inPlayer, 0, inX, inY) != 0; }  // uu_cloak.rs

// Sets the planet of every complex air unit, space dock, space turret and orbital space station on the map
// (cdecl; called by EEWorld.cpp and EEUserInterface.cpp)
void EEUpdateLandContinents() { ee_units_ck_body(EE_UNITS_CK_DO_UPDATE_LAND_CONTINENTS, 0, 0, 0, 0); }  // uu_cloak.rs
