// C:\EEX\Empire Earth\User Interface Thread\EEUIBuildGate.cpp (file name inferred: no __FILE__; RTTI class
// EEUIBuildGate). The gate placement mode: 0x25619b..0x25643d (the members in alphabetical order; ctor and ??_G are
// inline in the header, emitted by the scenario editor form's unit; EEUIBuildWall follows at 0x25643d). Unit flags:
// the exe module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include "lle/LowLevelEngine.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIBuildGate.h"
#include "ee/EEARust.h"

// cdecl helpers (the units range 0x21048f..0x216d57; types inferred) (provisional names)
void EEClearGateHighlights(EEPlayer* inPlayer, long inUnitType);  // 0x21176e: restores the colour of inPlayer's walls
void EEHighlightGateWalls(EEPlayer* inPlayer, long inUnitType);  // 0x211957: highlights them

// the gate placement's world calls (placement_ffi.rs: Initialize, Terminate, ClearGhostUnits)
extern "C" void ee_screenscpp_pl_gate_highlight(int32_t inPlayer, int32_t inWallType)
{ EEHighlightGateWalls(gWorld.GetPlayer(inPlayer), inWallType); }
extern "C" void ee_screenscpp_pl_gate_unhighlight(int32_t inPlayer, int32_t inWallType)
{ EEClearGateHighlights(gWorld.GetPlayer(inPlayer), inWallType); }
extern "C" void ee_screenscpp_pl_ghosts_enter() { gWorld.mGhostUnitsLock->Enter(); }
extern "C" void ee_screenscpp_pl_ghosts_leave() { gWorld.mGhostUnitsLock->Leave(); }
extern "C" void ee_screenscpp_pl_ghosts_clear() { gWorld.ClearGhostUnits(); }
extern "C" void ee_screenscpp_pl_gate_load_media(int32_t inPlayer, EEUnitClass* inClass)
{ gWorld.GetPlayer(inPlayer)->SetUnitClassMedia(inClass->mUnitType, 0x7f); }
extern "C" void ee_screenscpp_pl_gate_unload_media(int32_t inPlayer, EEUnitClass* inClass)
{ gWorld.GetPlayer(inPlayer)->UnloadUnitClassMedia(inClass->mUnitType); }

// the main game form's Build (0x25619b; its Cancel: EEUIBuildWall.cpp) (placement_ffi.rs)
extern "C" void ee_screenscpp_pl_main_build_gate(float inX, float inY, int32_t inDirection)
{ static_cast<EEABuild*>(gUIManager->GetMainGameForm()->mBuild)->BuildGate(GE2DPoint(inX, inY), (long)inDirection); }
