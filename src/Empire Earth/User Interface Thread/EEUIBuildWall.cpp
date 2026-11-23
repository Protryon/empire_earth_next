// C:\EEX\Empire Earth\User Interface Thread\EEUIBuildWall.cpp (file name inferred: no __FILE__; RTTI class
// EEUIBuildWall). The wall placement mode: 0x25643d..0x256845 (the members in alphabetical order; Cancel is ICF-folded
// into EEUIBuildGate's 0x2561b6; ctor and ??_G are inline in the header, emitted by the scenario editor form's unit;
// EEUIBUpgrade follows at 0x256845). Unit flags: the exe module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include "lle/LowLevelEngine.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIBuildWall.h"
#include "ee/EEUIBuildGate.h"
#include "ee/EEARust.h"

// the placements' world calls (placement_ffi.rs: Initialize, Terminate, the gate's ClearGhostUnits)
extern "C" void ee_screenscpp_pl_wall_load_media(int32_t inPlayer, int32_t inTechnology)
{ gWorld.GetPlayer(inPlayer)->SetUnitClassMedia(gWorld.GetPlayer(inPlayer)->mTechnologies->Get(inTechnology)->mData->mUnitClass, 0x7f); }
extern "C" void ee_screenscpp_pl_wall_unload_media(int32_t inPlayer, int32_t inTechnology)
{ gWorld.GetPlayer(inPlayer)->UnloadUnitClassMedia(gWorld.GetPlayer(inPlayer)->GetTechTree()->Get(inTechnology)->mData->mUnitClass); }
extern "C" void ee_screenscpp_pl_unlock_cursor() { gUIManager->GetMainGameForm()->mIsCursorLocked = false; }

// the main game form's Cancel (0x2561b6, ICF with EEUIBuildGate's) and Build (0x25643d) (placement_ffi.rs)
extern "C" void ee_screenscpp_pl_main_cancel() { gUIManager->GetMainGameForm()->SetUIState(0); }
extern "C" void ee_screenscpp_pl_main_build_wall(float inStartX, float inStartY, float inEndX, float inEndY)
{ static_cast<EEABuild*>(gUIManager->GetMainGameForm()->mBuild)->BuildWall(GE2DPoint(inStartX, inStartY), GE2DPoint(inEndX, inEndY)); }
