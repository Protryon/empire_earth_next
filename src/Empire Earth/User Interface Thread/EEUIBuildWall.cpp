// C:\EEX\Empire Earth\User Interface Thread\EEUIBuildWall.cpp (file name inferred: no __FILE__; RTTI class
// EEUIBuildWall). The wall placement mode: 0x25643d..0x256845 (the members in alphabetical order; Cancel is ICF-folded
// into EEUIBuildGate's 0x2561b6; ctor and ??_G are inline in the header, emitted by the scenario editor form's unit;
// EEUIBUpgrade follows at 0x256845). Unit flags: the exe module set.
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
#include "ee/EEGraphics.h"
#include "ee/EEUnitPicker.h"
#include "ee/EEUIBuildWall.h"
#include "ee/EEUIBuildGate.h"


// cdecl helper (types inferred): the wall ghosts on inTiles for technology inTechnology of player inPlayer
// (provisional name)
void EEPreviewWall(std::vector<EETileCoordinate>& inTiles, long inTechnology, long inPlayer);  // 0x23eb4c

// FUNCTION: EXE 0x65643d
// Asks the main game form to build a wall from the start to the end point
void EEUIBuildWall::Build()
{
    gUIManager->mMainGameForm->BuildWall(this->mStart, this->mEnd);
}

// Leaves the placement mode (ICF-folded into EEUIBuildGate::Cancel 0x2561b6)
void EEUIBuildWall::Cancel()
{
    gUIManager->mMainGameForm->SetUIState(0);
}

// FUNCTION: EXE 0x656456
// Starts wall placement for a player: no points yet, the wall class's media loaded
void EEUIBuildWall::Initialize(long inPlayer)
{
    EEPlayer* thePlayer = gWorld.GetPlayer(inPlayer);
    this->mStart.SetX(-99.0f);
    this->mStart.SetY(-99.0f);
    this->mEnd.SetX(-99.0f);
    this->mEnd.SetY(-99.0f);
    this->mState = 0;
    EETechTreeNode* theNode = thePlayer->mTechnologies->Get(this->mTechnology);
    thePlayer->SetUnitClassMedia(theNode->mData->mUnitClass, 0x7f);
    this->mPlayer = inPlayer;
}

// FUNCTION: EXE 0x6564a2
// Ends wall placement: unlocks the cursor and unloads the wall class's media
void EEUIBuildWall::Terminate()
{
    EEPlayer* thePlayer = gWorld.GetPlayer(this->mPlayer);
    gUIManager->mMainGameForm->mIsCursorLocked = false;
    EETechTreeNode* theNode = thePlayer->GetTechTree()->Get(this->mTechnology);
    thePlayer->UnloadUnitClassMedia(theNode->mData->mUnitClass);
}

// FUNCTION: EXE 0x6564df
// Tracks the mouse in wall placement: a left click sets the start, then builds each segment and chains from
// its end; a right click cancels; moving previews the wall ghosts along the line
// NEAR: the original keeps 0.5f on the FP stack for the tile centres and compares the floored ends through a
// 4-byte copy of the tile's y (an inline tile helper not reconstructed yet); control flow and calls as here
void EEUIBuildWall::Update(const GE2DPoint& inLocation)
{
    std::vector<EETileCoordinate> theTiles;
    std::vector<GE2DPoint> thePoints;
    GETransformation theCamera(gWorld.mTransformation);
    EETileCoordinate theTile;
    EETile* theUnit;
    long theX;
    long theY;
    theTile.mX = 0;
    theTile.mY = 0;
    EEPointUnitOrTileXY(inLocation, theUnit, theX, theY, true);
    theTile.mX = theX;
    theTile.mY = theY;
    if (UIManager::GetLMouseButton()->JustPressed() || UIManager::GetLMouseButton()->GetValue())
        return;
    if (UIManager::GetLMouseButton()->JustUnpressed() && gViewport.GetViewRectangle().Contains(inLocation)) {
        switch (this->mState) {
        case 0:
            this->mState = 1;
            this->mEnd = this->mStart = GE2DPoint(theTile.GetX() + 0.5f, theTile.GetY() + 0.5f);
            theTiles.clear();
            this->Build();
            break;
        case 1:
            this->mEnd.SetX(theTile.GetX() + 0.5f);
            this->mEnd.SetY(theTile.GetY() + 0.5f);
            theTiles.clear();
            this->Build();
            this->mStart.SetX(theTile.GetX() + 0.5f);
            this->mStart.SetY(theTile.GetY() + 0.5f);
            break;
        }
    } else if (!UIManager::GetRMouseButton()->JustPressed() && !UIManager::GetRMouseButton()->GetValue()) {
        if (UIManager::GetRMouseButton()->JustUnpressed()) {
            gWorld.mGhostUnitsLock->Enter();
            gWorld.ClearGhostUnits();
            gWorld.mGhostUnitsLock->Leave();
            switch (this->mState) {
            case 0:
            case 1:
                this->Cancel();
                break;
            }
        } else if ((UMFloor(this->mStart.GetX()) != (float)theTile.GetX() || UMFloor(this->mStart.GetY()) != (float)theTile.GetY()) &&
                   (UMFloor(this->mEnd.GetX()) != (float)theTile.GetX() || UMFloor(this->mEnd.GetY()) != (float)theTile.GetY())) {
            if (this->mState == 0) {
                theTiles.clear();
                theTiles.push_back(theTile);
                gWorld.mGhostUnitsLock->Enter();
                EEPreviewWall(theTiles, this->mTechnology, this->mPlayer);
                gWorld.mGhostUnitsLock->Leave();
            }
            if (this->mState == 1) {
                this->mEnd.SetX(theTile.GetX() + 0.5f);
                this->mEnd.SetY(theTile.GetY() + 0.5f);
                theTiles.clear();
                this->mStart.InterpolateTo(this->mEnd, 1.0f, thePoints);
                for (unsigned long i = 0; i < thePoints.size(); i++) {
                    GE2DPoint& thePoint = thePoints[i];
                    EETileCoordinate theStep;
                    theStep.mX = (long)thePoint.GetX();
                    theStep.mY = (long)thePoint.GetY();
                    theTiles.push_back(theStep);
                }
                gWorld.mGhostUnitsLock->Enter();
                EEPreviewWall(theTiles, this->mTechnology, this->mPlayer);
                gWorld.mGhostUnitsLock->Leave();
            }
        }
    }
}
