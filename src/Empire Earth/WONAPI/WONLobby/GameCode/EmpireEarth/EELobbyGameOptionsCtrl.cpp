// C:\EEX\Empire Earth\WONAPI\WONLobby\GameCode\EmpireEarth\EELobbyGameOptionsCtrl.cpp (__FILE__ 0x8d09ac, InitComponent's
// lines 74..89). The game room's game options: 16 named combo/check boxes showing the lobby game's options:
// 0x6942c8..0x694d62 (the option table's static initializer first; EELobbyInit.cpp follows).
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

// one option control: its resource name and kind (0: combo box, 1: check box)
struct EELobbyOptionInfo {
    EELobbyOptionInfo() {}
    EELobbyOptionInfo(const char* inName, int inType) : mName(inName), mType(inType) {}

    const char* mName;
    int mType;
};

static EELobbyOptionInfo sOptionTable[16];
static bool sOptionTableInitialized;

static void EELobbyInitOptionTable();

// FUNCTION: EXE 0x6942ce
// Creates the inactive game options panel, filling the option table on first use
EELobbyGameOptionsCtrl::EELobbyGameOptionsCtrl()
{
    EELobbyInitOptionTable();
    this->mIsActive = false;
    this->mRoomSpec = 2;
}

// FUNCTION: EXE 0x694336
// Destroys the game options panel (members only)
EELobbyGameOptionsCtrl::~EELobbyGameOptionsCtrl()
{
}

// FUNCTION: EXE 0x694394
// Fills the table of the 16 option controls' resource names and kinds (combo or check box), once
static void EELobbyInitOptionTable()
{
    if (sOptionTableInitialized)
        return;
    sOptionTableInitialized = true;
    sOptionTable[0] = EELobbyOptionInfo("GameTypeCombo", 0);
    sOptionTable[1] = EELobbyOptionInfo("MapSizeCombo", 0);
    sOptionTable[2] = EELobbyOptionInfo("MapTypeCombo", 0);
    sOptionTable[3] = EELobbyOptionInfo("ResourcesCombo", 0);
    sOptionTable[4] = EELobbyOptionInfo("StartingEpochCombo", 0);
    sOptionTable[5] = EELobbyOptionInfo("EndingEpochCombo", 0);
    sOptionTable[6] = EELobbyOptionInfo("GameVariantCombo", 0);
    sOptionTable[7] = EELobbyOptionInfo("DifficultyLevelCombo", 0);
    sOptionTable[8] = EELobbyOptionInfo("GameSpeedCombo", 0);
    sOptionTable[9] = EELobbyOptionInfo("GameUnitLimitCombo", 0);
    sOptionTable[10] = EELobbyOptionInfo("WondersCombo", 0);
    sOptionTable[11] = EELobbyOptionInfo("RevealMapCheck", 1);
    sOptionTable[12] = EELobbyOptionInfo("UseCustomCivsCheck", 1);
    sOptionTable[13] = EELobbyOptionInfo("LockTeamsCheck", 1);
    sOptionTable[14] = EELobbyOptionInfo("LockSpeedCheck", 1);
    sOptionTable[15] = EELobbyOptionInfo("VictoryCheck", 1);
}

// FUNCTION: EXE 0x6944ae
// Looks up the campaign info and option controls from the layout, fills the unit limit and map type lists
void EELobbyGameOptionsCtrl::InitComponent(ComponentInit& inInit)
{
    if (inInit.mType == 2) {
        ComponentConfig* theConfig = ((ComponentConfigInit&)inInit).mConfig;
        SafeGetComponent(theConfig, this->mCampaignInfoLabel, "CampaignInfoLabel", __FILE__, 74);
        SafeGetComponent(theConfig, this->mCampaignInfoBox1, "CampaignInfoBox1", __FILE__, 75);
        SafeGetComponent(theConfig, this->mCampaignInfoBox2, "CampaignInfoBox2", __FILE__, 76);

        int i;
        for (i = 0; i < 16; i++) {
            SafeGetComponent(theConfig, this->mOptions[i], sOptionTable[i].mName, __FILE__, 80);
            bool theTypeOK = true;
            switch (sOptionTable[i].mType) {
            case 1: theTypeOK = dynamic_cast<Button*>(this->mOptions[i].get()) != NULL; break;
            case 0: theTypeOK = dynamic_cast<ComboBox*>(this->mOptions[i].get()) != NULL; break;
            }

            if (!theTypeOK)
                ComponentConfigMissing(std::string("Invalid type for ") + sOptionTable[i].mName, __FILE__, 89);
            this->mOptions[i]->mControlId = i + 1;
        }

        ComboBox* theCombo = this->GetComboBox(9);
        if (theCombo != NULL) {
            for (int theLimit = 50; theLimit <= 1200; theLimit += 50) {
                char theBuffer[50];
                sprintf(theBuffer, "%d", theLimit);
                theCombo->AddString(theBuffer);
            }
        }
        EELobbyFillMapTypeCombo(this->GetComboBox(2));
        this->SubscribeToBroadcast(1);
        this->GetCheckBox(15)->SetVisible(false);
    }
}

// FUNCTION: EXE 0x694680
// Returns the control of a game option, or NULL for an option number out of range
Component* EELobbyGameOptionsCtrl::GetControl(int inOption)
{
    if (inOption >= 0 && inOption < 16)
        return this->mOptions[inOption].mPtr;
    return NULL;
}

// FUNCTION: EXE 0x69469b
// Returns the combo box of a game option, or NULL when the option is out of range or a check box
ComboBox* EELobbyGameOptionsCtrl::GetComboBox(int inOption)
{
    if (inOption >= 0 && inOption < 16 && sOptionTable[inOption].mType == 0)
        return (ComboBox*)this->mOptions[inOption].mPtr;
    return NULL;
}

// FUNCTION: EXE 0x6946c0
// Returns the check box of a game option, or NULL when the option is out of range or a combo box
Button* EELobbyGameOptionsCtrl::GetCheckBox(int inOption)
{
    if (inOption >= 0 && inOption < 16 && sOptionTable[inOption].mType == 1)
        return (Button*)this->mOptions[inOption].mPtr;
    return NULL;
}

// FUNCTION: EXE 0x6946e5
// Shows one of the game's options in its control; the unit limit is shown in steps of 50
// The range test lets 16 through (`> 16`, as the original)
void EELobbyGameOptionsCtrl::UpdateOption(int inOption, EELobbyGame* inGame)
{
    if (inGame == NULL || inOption < 0 || inOption > 16)
        return;
    if (inOption != 9) {
        switch (sOptionTable[inOption].mType) {
        case 1: {
            Button* theCheck = this->GetCheckBox(inOption);
            if (theCheck != NULL)
                theCheck->SetCheck(inGame->GetOption(inOption) != 0);
            break;
        }
        case 0: {
            ComboBox* theCombo = this->GetComboBox(inOption);
            if (theCombo != NULL)
                theCombo->SetSelItem(inGame->GetOption(inOption));
            break;
        }
        }
    } else {
        ComboBox* theCombo = this->GetComboBox(9);
        if (theCombo != NULL)
            theCombo->SetSelItem((unsigned long)inGame->GetOption(9) / 50 - 1);
    }
}

// FUNCTION: EXE 0x694788
// Shows one of the current lobby game's options in its control
void EELobbyGameOptionsCtrl::UpdateOption(int inOption)
{
    this->UpdateOption(inOption, this->GetGame());
}

// FUNCTION: EXE 0x6947a0
// Shows every game option, enables them for the host of a random-map game and shows the saved game's name if any
void EELobbyGameOptionsCtrl::UpdateAll()
{
    EELobbyGame* theGame = this->GetGame();
    if (theGame == NULL)
        return;
    int i;
    for (i = 0; i < 16; i++)
        this->UpdateOption(i, theGame);
    bool theNotRandomMap = theGame->GetOption(0) != 0;
    bool theFlag = theGame->mIsHost;
    for (int j = 0; j < 16; j++) {
        Component* theControl = this->GetControl(j);
        bool theEnabled;
        if (j == 0)
            theEnabled = theFlag;
        else if (theFlag && !theNotRandomMap)
            theEnabled = true;
        else
            theEnabled = false;
        if (theControl != NULL)
            theControl->SetEnabled(theEnabled);
    }
    if (theGame->HasGameFile()) {
        this->mCampaignInfoBox1->SetVisible(false);
        this->mCampaignInfoBox2->SetVisible(true);
        this->mCampaignInfoLabel->SetText(theGame->GetGameFileName());
    } else {
        this->mCampaignInfoBox1->SetVisible(true);
        this->mCampaignInfoBox2->SetVisible(false);
    }
}

// FUNCTION: EXE 0x69488a
// Activates the panel for a matching game room, showing every option and enabling them all for the host only
void EELobbyGameOptionsCtrl::Activate()
{
    EELobbyGame* theGame = this->GetGame();
    if (theGame == NULL || !theGame->MatchesRoomSpec(this->mRoomSpec))
        return;
    this->mIsActive = true;
    this->UpdateAll();
    bool theFlag = theGame->mIsHost;
    for (int i = 0; i < 16; i++)
        this->mOptions[i]->SetEnabled(theFlag);
}

// FUNCTION: EXE 0x6948e2
// Marks the panel inactive
void EELobbyGameOptionsCtrl::Deactivate()
{
    if (this->mIsActive)
        this->mIsActive = false;
}

// FUNCTION: EXE 0x6948f5
// Switches the game between random map and saved game, asking for the .ees file and opening named slots
// The original calls the GUIString copy's SmartPtr(T*) out of line (0x693cd4, inline budget); ours expands it
void EELobbyGameOptionsCtrl::ChangeGameType()
{
    ComboBox* theCombo = this->GetComboBox(0);
    if (theCombo == NULL || theCombo->mListArea->mSelItem == NULL)
        return;
    EELobbyGame* theGame = this->GetGame();
    if (theGame == NULL)
        return;
    long theOldType = theGame->GetOption(0);
    if (theCombo->GetSelItem() == 1) {
        GUIString theDirectory = EELobbyFn2988E9();
        SmartPtr<EELobbyFileDialog> theDialog = EELobbyGetFileDialog();
        int theResult = -1;
        if (theDialog.mPtr != NULL) {
            theDialog->ListFiles(theDirectory, ".ees");
            theResult = LobbyDoDialog(theDialog.mPtr);
        }
        theGame = this->GetGame();
        if (theGame != NULL) {
            if (theResult == 100001) {
                theGame->SetOption(0, 1);
                theGame->SendGameState();
            } else
                theCombo->SetSelItem(theOldType);
        }
    } else {
        theCombo->SetSelItem(0);
        theGame->SetOption(0, 0);
        for (int i = 0; i < 8; i++) {
            EELobbyPlayerSlot* theSlot = theGame->GetPlayerSlot(i);
            if (theSlot != NULL && theSlot->mOptions[0] == 4)
                theSlot->SetOpen();
        }
        theGame->SendGameState();
    }
}

// FUNCTION: EXE 0x694a77
// Sends the host's change of a game option to the game; for anyone else restores the control from the game
// The original tail-merges the combo and unit limit calls into the check box's call; ours the other way round
void EELobbyGameOptionsCtrl::OptionChanged(int inOption)
{
    if (inOption < 0 || inOption >= 16)
        return;
    if (!LobbyIAmCaptain()) {
        this->UpdateOption(inOption);
    } else {
    EELobbyGame* theGame = this->GetGame();
    if (theGame == NULL)
        return;
    if (inOption != 0) {
        if (inOption != 9) {
            switch (sOptionTable[inOption].mType) {
            case 1: {
                Button* theCheck = this->GetCheckBox(inOption);
                if (theCheck != NULL)
                    theGame->ChangeOption(inOption, theCheck->mChecked, true);
                break;
            }
            case 0: {
                ComboBox* theCombo = this->GetComboBox(inOption);
                if (theCombo != NULL && theCombo->mListArea->mSelItem != NULL)
                    theGame->ChangeOption(inOption, theCombo->GetSelItem(), true);
                break;
            }
            }
        } else {
            ComboBox* theCombo = this->GetComboBox(9);
            if (theCombo != NULL && theCombo->mListArea->mSelItem != NULL)
                theGame->ChangeOption(9, (theCombo->GetSelItem() + 1) * 50, true);
        }
    } else
        this->ChangeGameType();
    }
}

// FUNCTION: EXE 0x694b57
// Handles the game room's events: activation, option updates from the game and the user's changes to the controls
void EELobbyGameOptionsCtrl::HandleComponentEvent(ComponentEvent* inEvent)
{
    switch (inEvent->mType) {
    case 100065:
        if (this->mIsActive)
            this->UpdateAll();
        break;
    case LobbyEvent_EELobbySyncOption:
        if (this->mIsActive)
            this->UpdateOption(static_cast<EELobbySyncOptionEvent*>(inEvent)->mParam);
        break;
    case LobbyEvent_SetRoomSpecFlags:
        this->mRoomSpec |= static_cast<SetRoomSpecFlagsEvent*>(inEvent)->mFlags;
        break;
    case 100032:
        this->Deactivate();
        break;
    case 100029:
        this->Activate();
        break;
    case ComponentEvent_ComboSelChanged:
    case ComponentEvent_ButtonPressed:
        if (this->mIsActive)
            this->OptionChanged(inEvent->GetControlId() - 1);
        break;
    }
}

// Returns the current lobby game; ICF-folded into 0x699dcd (its twin in EELobbyPlayerOptionsCtrl.cpp)
EELobbyGame* EELobbyGameOptionsCtrl::GetGame()
{
    return static_cast<EELobbyGame*>(GetMyLobbyGame());
}

// The unit's other compiler-generated functions:
// SYNTHETIC: EXE 0x6942c8
// SYNTHETIC: EXE 0x6942cd
// (the option table's `$E` chain: the element ctor loop, empty)
// SYNTHETIC: EXE 0x69431a
// (??_G EELobbyGameOptionsCtrl)
// TEMPLATE: EXE 0x694be5
// (SafeGetComponent<Label>)
// TEMPLATE: EXE 0x694c65
// (SafeGetComponent<Container>)
// TEMPLATE: EXE 0x694ce5
// (SafeGetComponent<Component>)
