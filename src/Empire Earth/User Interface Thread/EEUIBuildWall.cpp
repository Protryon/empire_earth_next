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

// Ends wall placement: unlocks the cursor and unloads the wall class's media
void EEUIBuildWall::Terminate()
{
    EEPlayer* thePlayer = gWorld.GetPlayer(this->mPlayer);
    gUIManager->mMainGameForm->mIsCursorLocked = false;
    EETechTreeNode* theNode = thePlayer->GetTechTree()->Get(this->mTechnology);
    thePlayer->UnloadUnitClassMedia(theNode->mData->mUnitClass);
}
