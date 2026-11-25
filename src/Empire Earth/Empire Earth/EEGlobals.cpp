// The references the original's code made to members of gWorld and gGameState past their standard containers
// (gNumPlayers is gWorld.mNumPlayers, ...; it addressed them as globals). The objects' storage, the other
// references and the save version are Rust's (ee_game gamecpp global_objects.rs: there the offsets come from
// #[repr(C)] mirrors; these members' LP64 offsets depend on the standard library's containers, so C++ computes them);
// the plain pointers and scalars and the game's UI table gEEGameUI too (globals.rs). No function and no constructor is named here,
// so the unit depends on no other: 197 units named Empire Earth.cpp only for these globals, and that put them all in
// one dependency cycle (tools/cpp_deps.py).
#include <windows.h>
#include <stdint.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEGameOptions.h"
#include "ee/EEGameState.h"

long& gNumPlayers = reinterpret_cast<long&>(gWorld.mNumPlayers);   // +0xcc
long& gGameResources = reinterpret_cast<long&>(gWorld.mGameOptions.mResources);   // +0x4fc
bool& gAllowCustomCivs = gWorld.mGameOptions.mAllowCustomCivs;   // +0x516
bool& gIsScenarioGame = gWorld.mGameOptions.mIsScenarioGame;   // +0x519
EEGameOptions& gGameOptions = gWorld.mLobbyOptions;   // +0x51c
float& gCameraZoom = gWorld.mCameraControl.mZoom;   // +0xab0
EEEffectManager*& gEffectManager = gWorld.mEffectManager;   // +0xb1c
float& gSavedGameSpeed = gWorld.mSavedGameSpeed;   // +0xb70
unsigned long& gMaxPlayers = reinterpret_cast<unsigned long&>(gWorld.mMaxPlayers);   // +0xb8c
unsigned long& gStartTime = reinterpret_cast<unsigned long&>(gWorld.mStartTime);   // +0xb90
unsigned short& gLagStatus = gWorld.mLagStatus;   // +0xb94
bool& gGameScreenActive = gWorld.mGameScreenActive;   // +0xb97
bool& gLoadingSavedGame = gWorld.mLoadingSavedGame;   // +0xb98
bool& gWorldClearing = gWorld.mClearing;   // +0xb99
bool& gUnitImprovementsAllowed = gWorld.mUnitImprovementsAllowed;   // +0xb9b
bool& gShowCollisionSpheres = gWorld.mShowCollisionSpheres;   // +0xb9c
bool& gInCinematic = gWorld.mInCinematic;   // +0xba0
bool& gUsingCarbon = gWorld.mUsingCarbon;   // +0xba4
float& gGameStateGameSpeed = gGameState.mGameSpeed;   // +0x4dc
