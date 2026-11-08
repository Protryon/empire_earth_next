// C:\EEX\Empire Earth\WONAPI\WONLobby\GameCode\EmpireEarth\EELobbyPlayerOptionsCtrl.cpp (__FILE__ 0x8d0e70, InitComponent's
// lines 50..70). The game room's player rows: 0x699b17..0x69a43d (EELobbyPlayer.cpp's slot and player classes end at
// 0x699b17).
#include <windows.h>
#include <stdio.h>
#include <string>
#include <list>

#include "lle/LowLevelEngine.h"

#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/EELobbyGameCode.h"

using namespace WONAPI;

// FUNCTION: EXE 0x699b17
// Creates the inactive player rows panel, not host and with no own slot
EELobbyPlayerOptionsCtrl::EELobbyPlayerOptionsCtrl()
{
    this->mIsActive = false;
    this->mIsHost = false;
    this->mMySlot = -1;
    this->mRoomSpec = 2;
}

// FUNCTION: EXE 0x699ba5
// Destroys the player rows panel (members only)
EELobbyPlayerOptionsCtrl::~EELobbyPlayerOptionsCtrl()
{
}

// FUNCTION: EXE 0x699c56
// Looks up the eight player rows' name, team, colour, citizen, observe and transfer controls from the layout
// The original strength-reduces the six control ids from i * 6 + 3 (ours from another base)
void EELobbyPlayerOptionsCtrl::InitComponent(ComponentInit& inInit)
{
    if (inInit.mType == 2) {
        ComponentConfig* theConfig = ((ComponentConfigInit&)inInit).mConfig;
        char theName[50];
        for (int i = 0; i < 8; i++) {
            sprintf(theName, "UserNameCombo%d", i + 1);
            SafeGetComponent(theConfig, this->mUserNameCombo[i], theName, __FILE__, 50);
            this->mUserNameCombo[i]->mControlId = i * 6 + 1;

            sprintf(theName, "TeamCombo%d", i + 1);
            SafeGetComponent(theConfig, this->mTeamCombo[i], theName, __FILE__, 54);
            this->mTeamCombo[i]->mControlId = i * 6 + 2;

            sprintf(theName, "ColorButton%d", i + 1);
            SafeGetComponent(theConfig, this->mColorButton[i], theName, __FILE__, 58);
            this->mColorButton[i]->mControlId = i * 6 + 3;

            sprintf(theName, "CitizenCombo%d", i + 1);
            SafeGetComponent(theConfig, this->mCitizenCombo[i], theName, __FILE__, 62);
            this->mCitizenCombo[i]->mControlId = i * 6 + 4;

            sprintf(theName, "ObserveCheck%d", i + 1);
            SafeGetComponent(theConfig, this->mObserveCheck[i], theName, __FILE__, 66);
            this->mObserveCheck[i]->mControlId = i * 6 + 5;

            sprintf(theName, "FileTransferLabel%d", i + 1);
            SafeGetComponent(theConfig, this->mFileTransferLabel[i], theName, __FILE__, 70);
            this->mFileTransferLabel[i]->mControlId = i * 6 + 6;
        }
        this->SubscribeToBroadcast(1);
    }
}

// Returns the current lobby game (0x699dcd, ICF-shared with EELobbyGameOptionsCtrl::GetGame, the name bound there)
EELobbyGame* EELobbyPlayerOptionsCtrl::GetGame()
{
    return static_cast<EELobbyGame*>(GetMyLobbyGame());
}

// FUNCTION: EXE 0x699dd2
// Shows one slot's row: name or state, team, colour, citizen, observer and transfer percent, enabled by role
void EELobbyPlayerOptionsCtrl::UpdatePlayer(int inSlot, EELobbyGame* inGame)
{
    if (inGame == NULL || inSlot < 0 || inSlot >= 8)
        return;
    EELobbyPlayerSlot* theSlot = inGame->GetPlayerSlot(inSlot);
    if (theSlot == NULL)
        return;
    SmartPtr<ComboBox>& theTeam = this->mTeamCombo[inSlot];
    theTeam->SetSelItem(theSlot->mOptions[1]);
    SmartPtr<ComboBox>& theCitizen = this->mCitizenCombo[inSlot];
    theCitizen->SetSelItem(theSlot->mOptions[3] - 1);
    bool theTaken = false;
    bool theShowTeam = false;
    bool theNotRandomMap = inGame->GetOption(0) != 0;
    bool theIsMe = false;
    if (inSlot == this->mMySlot && !GetMyLobbyPlayer()->mIsReady)
        theIsMe = true;
    if (theSlot->mOptions[0] == 1)
        theShowTeam = true;
    SmartPtr<ComboBox>* theName;
    if (theSlot->mOptions[0] != 3 && theSlot->mOptions[0] != 4) {
        theName = &this->mUserNameCombo[inSlot];
        (*theName)->SetSelItem(theSlot->mOptions[0]);
        (*theName)->mEdit->mTextColor = 0x8000000e;
        this->mObserveCheck[inSlot]->SetVisible(false);
        this->mColorButton[inSlot]->SetVisible(false);
        this->mFileTransferLabel[inSlot]->SetVisible(false);
    } else {
        theShowTeam = theSlot->mOptions[4] == 0;
        theName = &this->mUserNameCombo[inSlot];
        theTaken = true;
        (*theName)->SetText(theSlot->GetName());
        if (theSlot->mOptions[0] == 4)
            (*theName)->mEdit->mTextColor = 0xff0000;
        else
            (*theName)->mEdit->mTextColor = 0x8000000e;
        this->mObserveCheck[inSlot]->SetVisible(true);
        this->mObserveCheck[inSlot]->SetEnabled(theIsMe && !theNotRandomMap);
        this->mObserveCheck[inSlot]->SetCheck(theSlot->mOptions[4] != 0);
        this->mColorButton[inSlot]->SetVisible(theSlot->mOptions[4] == 0);
        this->mColorButton[inSlot]->SetEnabled(theIsMe && !theNotRandomMap);
        this->mColorButton[inSlot]->SetColor(theSlot->mOptions[2]);
        if (theSlot->mOptions[5] >= 100)
            this->mFileTransferLabel[inSlot]->SetVisible(false);
        else {
            this->mFileTransferLabel[inSlot]->SetVisible(true);
            char theBuffer[50];
            sprintf(theBuffer, "%d", theSlot->mOptions[5]);
            this->mFileTransferLabel[inSlot]->SetText(
                LobbyFormatString(gEELobbyPlayerOptionsCtrl_TransferPercent, theBuffer));
        }
    }
    (*theName)->SetEnabled(this->mIsHost && inSlot != this->mMySlot && !theNotRandomMap);
    theCitizen->SetEnabled(this->mIsHost && !theNotRandomMap);
    theTeam->SetEnabled((theIsMe || (this->mIsHost && !theTaken)) && !theNotRandomMap);
    theCitizen->SetVisible(theShowTeam);
    theTeam->SetVisible(theShowTeam);
}

// FUNCTION: EXE 0x69a0f5
// Shows one player slot's row for the current lobby game
void EELobbyPlayerOptionsCtrl::UpdatePlayer(int inSlot)
{
    this->UpdatePlayer(inSlot, this->GetGame());
}

// FUNCTION: EXE 0x69a10d
// Shows all eight player rows of the current lobby game
void EELobbyPlayerOptionsCtrl::UpdateAll()
{
    EELobbyGame* theGame = this->GetGame();
    if (theGame != NULL)
        for (int i = 0; i < 8; i++)
            this->UpdatePlayer(i, theGame);
}

// FUNCTION: EXE 0x69a132
// Activates the panel for a matching game room, noting whether we host and our slot, and shows every row
void EELobbyPlayerOptionsCtrl::Activate()
{
    EELobbyGame* theGame = this->GetGame();
    if (theGame == NULL || !theGame->MatchesRoomSpec(this->mRoomSpec))
        return;
    this->mIsActive = true;
    this->mIsHost = LobbyIAmCaptain();
    EELobbyPlayer* thePlayer = static_cast<EELobbyPlayer*>(GetMyLobbyPlayer());
    if (thePlayer != NULL)
        this->mMySlot = thePlayer->GetSlot();
    this->UpdateAll();
}

// FUNCTION: EXE 0x69a183
// Marks the panel inactive, forgetting the host flag and our slot
void EELobbyPlayerOptionsCtrl::Deactivate()
{
    if (this->mIsActive) {
        this->mMySlot = -1;
        this->mIsActive = false;
        this->mIsHost = false;
    }
}

// FUNCTION: EXE 0x69a19f
// Sends the value of a changed player row control to the game as that slot's option
void EELobbyPlayerOptionsCtrl::PlayerOptionChanged(int inSlot, int inWhat)
{
    EELobbyGame* theGame = this->GetGame();
    if (theGame == NULL || inSlot < 0 || inSlot >= 8 || inWhat < 0 || inWhat >= 6)
        return;
    unsigned char theValue = 0;
    switch (inWhat) {
    case 4:
        theValue = this->mObserveCheck[inSlot]->mChecked;
        break;
    case 3:
        theValue = this->mCitizenCombo[inSlot]->GetSelItem() + 1;
        break;
    case 2:
        theValue = this->mColorButton[inSlot]->mColor;
        break;
    case 1:
        theValue = this->mTeamCombo[inSlot]->GetSelItem();
        break;
    case 0:
        theValue = this->mUserNameCombo[inSlot]->GetSelItem();
        break;
    }
    theGame->ChangePlayerOption(inSlot, inWhat, theValue);
}

// FUNCTION: EXE 0x69a240
// Refreshes our own row when our player's data changes
void EELobbyPlayerOptionsCtrl::HandlePlayerChanged(ComponentEvent* inEvent)
{
    PlayerChangedEvent* theEvent = static_cast<PlayerChangedEvent*>(inEvent);
    if (theEvent->IsModify() && theEvent->mClient.mPtr->mPlayer.mPtr != NULL
        && ((EELobbyPlayer*)theEvent->mClient.mPtr->mPlayer.mPtr)->GetSlot() == this->mMySlot)
        this->UpdatePlayer(this->mMySlot);
}

// FUNCTION: EXE 0x69a27c
// Handles the game room's events: activation, player updates and the user's changes to a row's controls
// A player change falls through (no break) into the slot update, using its change kind as the slot
void EELobbyPlayerOptionsCtrl::HandleComponentEvent(ComponentEvent* inEvent)
{
    switch (inEvent->mType) {
    case 100032:
        this->Deactivate();
        break;
    case 100029:
        this->Activate();
        break;
    case LobbyEvent_PlayerChanged:
        this->HandlePlayerChanged(inEvent);
    case LobbyEvent_EELobbySyncPlayer:
        if (this->mIsActive)
            this->UpdatePlayer(static_cast<EELobbySyncPlayerEvent*>(inEvent)->mParam);
        break;
    case 100065:
        if (this->mIsActive)
            this->UpdateAll();
        break;
    case ComponentEvent_ButtonPressed:
    case ComponentEvent_ComboSelChanged:
    case 100064:
        if (this->mIsActive) {
            int theId = inEvent->GetControlId() - 1;
            this->PlayerOptionChanged(theId / 6, theId % 6);
        }
        break;
    case LobbyEvent_SetRoomSpecFlags:
        this->mRoomSpec |= static_cast<SetRoomSpecFlagsEvent*>(inEvent)->mFlags;
        break;
    }
}

// The unit's other compiler-generated functions:
// SYNTHETIC: EXE 0x699b89
// (??_G EELobbyPlayerOptionsCtrl)
// TEMPLATE: EXE 0x69a33d
// (SafeGetComponent<ComboBox>)
// TEMPLATE: EXE 0x69a3bd
// (SafeGetComponent<EELobbyColorButton>)

// The ICF-shared GetGame copy LINK kept in this unit's range:
// SYNTHETIC: EXE 0x699dcd
