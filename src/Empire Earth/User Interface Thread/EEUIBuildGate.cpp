// C:\EEX\Empire Earth\User Interface Thread\EEUIBuildGate.cpp (file name inferred: no __FILE__; RTTI class
// EEUIBuildGate). The gate placement mode: 0x25619b..0x25643d (the members in alphabetical order; ctor and ??_G are
// inline in the header, emitted by the scenario editor form's unit; EEUIBuildWall follows at 0x25643d). Unit flags:
// the exe module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUnitPicker.h"
#include "ee/EEGhostUnit.h"
#include "ee/EEUIBuildGate.h"

// cdecl helpers (the units range 0x21048f..0x216d57; types inferred) (provisional names)
bool EEFindGateWall(const EETileCoordinate& inTile, long& ioDirection);  // 0x2116a2: a gate fits on inTile's wall
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
// Tracks the mouse in gate placement: previews a gate on a wall under the cursor, builds it on a left click
// (then leaves the mode), cancels on a right click
void EEUIBuildGate::Update(const GE2DPoint& inLocation)
{
    GE2DPoint theLocation;
    EETile* theTarget;
    long theX;
    long theY;
    long theDirection;
    EEPlayer* thePlayer = gWorld.GetPlayer(this->mPlayer);
    EEUnit* theUnit = gUnitPicker.PickUnit(inLocation, 0);
    if (theUnit && theUnit->GetUnitClass()->mIsWall) {
        theX = theUnit->GetTileX();
        theY = theUnit->GetTileY();
    } else
        EEPointUnitOrTileXY(inLocation, theTarget, theX, theY, true);
    if (theUnit)
        theUnit->Dereference();
    theDirection = this->mDirection;
    theLocation.SetX(theX);
    theLocation.SetY(theY);
    if (UIManager::GetLMouseButton()->JustPressed() || UIManager::GetLMouseButton()->GetValue())
        return;
    if (UIManager::GetLMouseButton()->JustUnpressed()) {
        EETileCoordinate theTile;
        theTile.mX = theX;
        theTile.mY = theY;
        if (EEFindGateWall(theTile, theDirection)) {
            this->mTileX = theX;
            this->mTileY = theY;
            this->Build(theLocation, theDirection);
        }
        this->ClearGhostUnits();
        this->Cancel();
    } else if (!UIManager::GetRMouseButton()->JustPressed() && !UIManager::GetRMouseButton()->GetValue()) {
        if (UIManager::GetRMouseButton()->JustUnpressed()) {
            this->ClearGhostUnits();
            this->Cancel();
        } else if (theX == this->mTileX && theY == this->mTileY) {
            this->mTileX = theX;
            this->mTileY = theY;
        } else {
            EETileCoordinate theTile;
            theTile.mX = theX;
            theTile.mY = theY;
            if (EEFindGateWall(theTile, theDirection)) {
                EEGhostUnit::CreateGate(0.0f, this->mUnitClass, thePlayer);
                EEGhostUnit::PreviewGate(inLocation, theDirection);
                this->mDirection = theDirection;
            } else {
                gWorld.mGhostUnitsLock->Enter();
                gWorld.ClearGhostUnits();
                gWorld.mGhostUnitsLock->Leave();
            }
        }
    }
}
