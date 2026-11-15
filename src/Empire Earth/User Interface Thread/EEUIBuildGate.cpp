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

// cdecl helpers (the units range 0x21048f..0x216d57; types inferred) (provisional names)
void EEClearGateHighlights(EEPlayer* inPlayer, long inUnitType);  // 0x21176e: restores the colour of inPlayer's walls
void EEHighlightGateWalls(EEPlayer* inPlayer, long inUnitType);  // 0x211957: highlights them

// FUNCTION: EXE 0x65619b
// Asks the main game form to build a gate at a location facing a direction
void EEUIBuildGate::Build(const GE2DPoint& inLocation, long inDirection)
{
    gUIManager->mMainGameForm->BuildGate(inLocation, inDirection);
}

// FUNCTION: EXE 0x6561b6
// Leaves the placement mode (back to the normal interface state)
void EEUIBuildGate::Cancel()
{
    gUIManager->mMainGameForm->SetUIState(0);
}

// FUNCTION: EXE 0x6561c9
// Restores the player's wall colours and removes the gate ghost
void EEUIBuildGate::ClearGhostUnits()
{
    EEClearGateHighlights(gWorld.GetPlayer(this->mPlayer), this->mWallType);
    gWorld.mGhostUnitsLock->Enter();
    gWorld.ClearGhostUnits();
    gWorld.mGhostUnitsLock->Leave();
}

// FUNCTION: EXE 0x6561ff
// Starts gate placement for a player: highlights the walls a gate can go on and loads the gate's media
void EEUIBuildGate::Initialize(long inPlayer)
{
    EEPlayer* thePlayer = gWorld.GetPlayer(inPlayer);
    EEHighlightGateWalls(thePlayer, this->mWallType);
    this->mTileY = this->mTileX = -99;
    this->mDirection = 2;
    this->mPlayer = inPlayer;
    thePlayer->SetUnitClassMedia(this->mUnitClass->mUnitType, 0x7f);
}

// FUNCTION: EXE 0x656245
// Ends gate placement: removes highlights and ghosts, unlocks the cursor and unloads the gate's media
void EEUIBuildGate::Terminate()
{
    EEPlayer* thePlayer = gWorld.GetPlayer(this->mPlayer);
    this->ClearGhostUnits();
    gUIManager->mMainGameForm->mIsCursorLocked = false;
    thePlayer->UnloadUnitClassMedia(this->mUnitClass->mUnitType);
}

// FUNCTION: EXE 0x65627d
// (Update: EEUIFMainGame.cpp, next to its host; ee_screens mouse_modes.rs gate_update)
